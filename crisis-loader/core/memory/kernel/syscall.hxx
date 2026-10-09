#pragma once
#include <shared_mutex>

namespace syscall {
	struct section_range_t {
		std::uint64_t start;
		std::uint64_t end;
	};

    class c_syscall {
    public:
        bool setup( std::uint64_t ke_service_descriptor_table, std::uint64_t ntoskrnl_base ) {
            if ( m_ssdt_base || m_ntdll_stub )
                return true;

            m_ntoskrnl_base = ntoskrnl_base;
            if ( !ke_service_descriptor_table ) {
                logging::print( obf( "failed to resolve service descriptor" ) );
                return false;
            }

            std::uint64_t ssdt_data[ 4 ]{};
            if ( !memory::read_virtual( ke_service_descriptor_table, ssdt_data, sizeof( ssdt_data ) ) ) {
                logging::print( obf( "failed to read service descriptor" ) );
                return false;
            }

            m_ssdt_base         = ssdt_data[ 0 ];
            const auto nt_count = static_cast< std::uint32_t >( ssdt_data[ 2 ] );

            std::vector< std::uint32_t > nt_table( nt_count );
            if ( !memory::read_virtual( m_ssdt_base, nt_table.data( ), nt_count * sizeof( std::uint32_t ) ) ) {
                logging::print( obf( "failed to read ssdt table" ) );
                return false;
            }

            std::uint32_t best_idx = 0, best_entry = 0, best_params = 0;
            void*         best_stub = nullptr;
            const char*   best_name = nullptr;
            bool          best_cold = false;

            std::uint32_t fallback_idx = 0, fallback_entry = 0, fallback_params = 0;
            void*         fallback_stub = nullptr;
            const char*   fallback_name = nullptr;

            for ( std::size_t idx = 1; idx < nt_count; idx++ ) {
                const auto entry = nt_table[ idx ];
                if ( !entry ) continue;

                const auto param_count = entry & 0xF;
                if ( !param_count || param_count > 16 ) continue;

                const auto fn_va = m_ssdt_base + ( static_cast< int >( entry ) >> 4 );
                if ( fn_va <= m_ssdt_base ) continue;

                const char* name = nullptr;
                auto* stub = find_ntdll_stub( static_cast< std::uint32_t >( idx ), &name );
                if ( !stub )
                    continue;

                if ( is_cold_syscall( name ) ) {
                    best_idx    = static_cast< std::uint32_t >( idx );
                    best_entry  = entry;
                    best_params = param_count;
                    best_stub   = stub;
                    best_name   = name;
                    best_cold   = true;
                    break;
                }

                if ( !fallback_stub || idx > fallback_idx ) {
                    fallback_idx    = static_cast< std::uint32_t >( idx );
                    fallback_entry  = entry;
                    fallback_params = param_count;
                    fallback_stub   = stub;
                    fallback_name   = name;
                }
            }

            if ( !best_stub ) {
                best_idx    = fallback_idx;
                best_entry  = fallback_entry;
                best_params = fallback_params;
                best_stub   = fallback_stub;
                best_name   = fallback_name;
            }

            if ( !best_stub ) {
                logging::print( obf( "failed to find valid syscall entry" ) );
                return false;
            }

            (void)best_cold;
            (void)best_name;

            m_saved_entry = best_entry;
            m_param_count = best_params;
            m_entry_idx   = best_idx;
            m_entry_va    = m_ssdt_base + best_idx * sizeof( std::uint32_t );
            m_entry_pa    = g_paging->translate_linear( m_entry_va );
            m_ntdll_stub  = best_stub;

            logging::print( obf( "found system call: 0x%llx" ), m_ntdll_stub );

            if ( !m_entry_pa ) {
                logging::print( obf( "failed to translate syscall entry" ) );
                return false;
            }

			const auto sections = find_sections( );
			if ( sections.m_exec.empty( ) ) {
				logging::print( obf( "failed to find ntoskrnl sections" ) );
				return true;
			}

			auto in_ranges = [ ]( const std::vector< section_range_t >& ranges,
				std::uint64_t addr ) -> bool {
				for ( auto& sec : ranges )
					if ( addr >= sec.start && addr + sizeof( std::uint64_t ) <= sec.end )
						return true;
				return false;
				};

			std::uint32_t seen = 0, rejected_rva = 0, rejected_slot = 0,
				rejected_pt = 0, rejected_read = 0;
			std::uint64_t fallback_gadget = 0, fallback_slot = 0,
				fallback_pa = 0, fallback_saved = 0;

			for ( auto& [exec_start, exec_end] : sections.m_exec ) {
				const auto size = static_cast< std::size_t >( exec_end - exec_start );
				std::vector< std::uint8_t > bytes( size );
				if ( !memory::read_virtual( exec_start, bytes.data( ), size ) )
					continue;

				for ( std::size_t idx = 0; idx + 12 <= size; idx++ ) {
					const auto gadget_va = exec_start + idx;

					auto slot = 0ull;
					if ( bytes[ idx ] == 0xFF && bytes[ idx + 1 ] == 0x25 ) {
						const auto disp = *reinterpret_cast< const std::int32_t* >( &bytes[ idx + 2 ] );
						slot = gadget_va + 6 + static_cast< std::int64_t >( disp );
					}
					else if ( bytes[ idx ] == 0x48 && bytes[ idx + 1 ] == 0xFF &&
							  bytes[ idx + 2 ] == 0x25 ) {
						const auto disp = *reinterpret_cast< const std::int32_t* >( &bytes[ idx + 3 ] );
						slot = gadget_va + 7 + static_cast< std::int64_t >( disp );
					}
					else if ( bytes[ idx ] == 0x48 && bytes[ idx + 1 ] == 0x8B &&
							  bytes[ idx + 2 ] == 0x05 &&
							  ( ( bytes[ idx + 7 ] == 0xFF && bytes[ idx + 8 ] == 0xE0 ) ||
								( bytes[ idx + 7 ] == 0x48 && bytes[ idx + 8 ] == 0xFF &&
								  bytes[ idx + 9 ] == 0xE0 ) ) ) {
						const auto disp = *reinterpret_cast< const std::int32_t* >( &bytes[ idx + 3 ] );
						slot = gadget_va + 7 + static_cast< std::int64_t >( disp );
					}
					else if ( bytes[ idx ] == 0x48 && bytes[ idx + 1 ] == 0xA1 &&
							  bytes[ idx + 10 ] == 0xFF && bytes[ idx + 11 ] == 0xE0 ) {
						slot = *reinterpret_cast< const std::uint64_t* >( &bytes[ idx + 2 ] );
					}
					else {
						continue;
					}

					++seen;

					const auto rva = static_cast< std::int64_t >( gadget_va ) -
						static_cast< std::int64_t >( m_ssdt_base );
					if ( rva < INT32_MIN / 16 || rva > INT32_MAX / 16 ) {
						++rejected_rva;
						continue;
					}

					const bool writable = in_ranges( sections.m_writable, slot );
					if ( !writable && !in_ranges( sections.m_readonly, slot ) ) {
						++rejected_slot;
						continue;
					}

					const auto slot_pa = g_paging->translate_linear( slot );
					if ( !g_paging->hyperspace_entries( gadget_va ) || !slot_pa ) {
						++rejected_pt;
						continue;
					}

					std::uint64_t saved {};
					if ( !memory::read_virtual( slot, &saved, sizeof( saved ) ) ) {
						++rejected_read;
						continue;
					}

					if ( writable ) {
						logging::print( obf( "found gadget: 0x%llx\n" ), gadget_va );
						m_gadget_va = gadget_va;
						m_slot_va = slot;
						m_slot_pa = slot_pa;
						m_slot_saved = saved;
						return true;
					}

					if ( !fallback_slot ) {
						fallback_gadget = gadget_va;
						fallback_slot = slot;
						fallback_pa = slot_pa;
						fallback_saved = saved;
					}
				}
			}

			if ( fallback_slot ) {
				logging::print( obf( "found gadget: 0x%llx\n" ), fallback_gadget );
				m_gadget_va = fallback_gadget;
				m_slot_va = fallback_slot;
				m_slot_pa = fallback_pa;
				m_slot_saved = fallback_saved;
				return true;
			}

			logging::print( obf( "warning: failed to find gadget stub\n" ) );
            return true;
        }

        template< typename ret_t  = std::uint64_t,
                  typename a1_t  = void*, typename a2_t  = void*,
                  typename a3_t  = void*, typename a4_t  = void*,
                  typename a5_t  = void*, typename a6_t  = void*,
                  typename a7_t  = void*, typename a8_t  = void*,
                  typename a9_t  = void*, typename a10_t = void* >
        ret_t call_kernel( std::uint64_t func,
            a1_t  a1  = {}, a2_t  a2  = {}, a3_t  a3  = {},
            a4_t  a4  = {}, a5_t  a5  = {}, a6_t  a6  = {},
            a7_t  a7  = {}, a8_t  a8  = {}, a9_t  a9  = {},
            a10_t a10 = {} ) {

            if ( !m_entry_pa || !m_ntdll_stub ) {
                logging::print( obf( "syscall not initialized" ) );
                if constexpr ( std::is_void_v< ret_t > ) return;
                else return ret_t{};
            }

            const auto direct_rva_i64 = static_cast< std::int64_t >( func ) -
                static_cast< std::int64_t >( m_ssdt_base );
            const bool direct_valid = direct_rva_i64 >= INT32_MIN / 16 &&
                direct_rva_i64 <= INT32_MAX / 16;

            const bool use_gadget = !direct_valid && m_slot_pa;
            if ( !direct_valid && !use_gadget ) {
                logging::print( obf( "syscall: rva out of range, no gadget available" ) );
                if constexpr ( std::is_void_v< ret_t > ) return;
                else return ret_t{};
            }

            std::unique_lock lock( m_lock );

            auto effective = func;
            if ( use_gadget ) {
                if ( !memory::write_physical( m_slot_pa, &func, sizeof( func ) ) ) {
                    if constexpr ( std::is_void_v< ret_t > ) return;
                    else return ret_t{};
                }
                effective = m_gadget_va;
            }

            const auto new_rva   = static_cast< int >( effective - m_ssdt_base ) << 4;
            auto       new_entry = ( new_rva & 0xfffffff0 ) | ( m_saved_entry & 0xf );

            if ( !memory::write_physical( m_entry_pa, &new_entry, sizeof( new_entry ) ) ) {
                if ( use_gadget )
                    memory::write_physical( m_slot_pa, &m_slot_saved, sizeof( m_slot_saved ) );
                if constexpr ( std::is_void_v< ret_t > ) return;
                else return ret_t{};
            }

            using fn_t = void* ( __stdcall* )(
                a1_t, a2_t, a3_t, a4_t, a5_t,
                a6_t, a7_t, a8_t, a9_t, a10_t );

            void* result = nullptr;
            if ( const auto stub_fn = reinterpret_cast< fn_t >( m_ntdll_stub ) )
                result = stub_fn( a1, a2, a3, a4, a5, a6, a7, a8, a9, a10 );

            memory::write_physical( m_entry_pa, &m_saved_entry, sizeof( m_saved_entry ) );
            if ( use_gadget )
                memory::write_physical( m_slot_pa, &m_slot_saved, sizeof( m_slot_saved ) );

            if constexpr ( std::is_void_v< ret_t > ) return;
            else return reinterpret_cast< ret_t >( result );
        }

    private:
        std::shared_mutex m_lock{};
        std::uint64_t m_ntoskrnl_base{ 0 };
        std::uint64_t m_ssdt_base{ 0 };
        std::uint32_t m_entry_idx{ 0 };
        std::uint64_t m_entry_va{ 0 };
        std::uint64_t m_entry_pa{ 0 };
        std::uint32_t m_saved_entry{ 0 };
        std::uint32_t m_param_count{ 0 };
        void*         m_ntdll_stub{ nullptr };
        std::uint64_t m_gadget_va{ 0 };
        std::uint64_t m_slot_va{ 0 };
        std::uint64_t m_slot_pa{ 0 };
        std::uint64_t m_slot_saved{ 0 };

		struct section_map_t {
			std::vector< section_range_t > m_exec;
			std::vector< section_range_t > m_writable;
			std::vector< section_range_t > m_readonly;
		};

		section_map_t find_sections( ) {
			section_map_t map;

			dos_header_t dos_header { };
			if ( !memory::read_virtual( m_ntoskrnl_base, &dos_header, sizeof( dos_header ) ) || !dos_header.is_valid( ) )
				return map;

			nt_headers_t nt_headers { };
			if ( !memory::read_virtual( m_ntoskrnl_base + static_cast<std::uint64_t>( dos_header.m_lfanew ), &nt_headers, sizeof( nt_headers ) ) || !nt_headers.is_valid( ) )
				return map;

			const auto sec_count = nt_headers.m_number_of_sections;
			const auto sec_addr = m_ntoskrnl_base + static_cast<std::uint64_t>( dos_header.m_lfanew ) + 0x18
				+ static_cast<std::uint64_t>( nt_headers.m_size_of_optional_header );

			std::vector< section_header_t > secs( sec_count );
			if ( !memory::read_virtual( sec_addr, secs.data( ), sec_count * sizeof( section_header_t ) ) )
				return map;

			for ( auto& sec : secs ) {
				const bool writable = !!( sec.m_characteristics & IMAGE_SCN_MEM_WRITE );
				const bool exec = !!( sec.m_characteristics & IMAGE_SCN_MEM_EXECUTE );
				const auto start = m_ntoskrnl_base + static_cast<std::uint64_t>( sec.m_virtual_address );
				const auto end = start + sec.m_virtual_size;
				if ( !end || end <= start ) continue;
				if ( exec && !writable )  map.m_exec.push_back( { start, end } );
				if ( writable && !exec )  map.m_writable.push_back( { start, end } );
				if ( !writable && !exec ) map.m_readonly.push_back( { start, end } );
			}

			return map;
		}

        void* find_ntdll_stub( std::uint32_t target_idx, const char** name_out = nullptr ) {
            auto ntdll = reinterpret_cast< std::uint8_t* >(
                GetModuleHandleA( obf( "ntdll.dll" ) ) );
            if ( !ntdll ) return nullptr;

            auto dos      = reinterpret_cast< IMAGE_DOS_HEADER* >( ntdll );
            auto nt       = reinterpret_cast< IMAGE_NT_HEADERS* >( ntdll + dos->e_lfanew );
            auto exp      = reinterpret_cast< IMAGE_EXPORT_DIRECTORY* >(
                ntdll + nt->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT ].VirtualAddress );
            auto names    = reinterpret_cast< std::uint32_t* >( ntdll + exp->AddressOfNames );
            auto ordinals = reinterpret_cast< std::uint16_t* >( ntdll + exp->AddressOfNameOrdinals );
            auto funcs    = reinterpret_cast< std::uint32_t* >( ntdll + exp->AddressOfFunctions );

            for ( std::uint32_t i = 0; i < exp->NumberOfNames; i++ ) {
                const auto name = reinterpret_cast< const char* >( ntdll + names[ i ] );
                if ( name[ 0 ] != 'N' || name[ 1 ] != 't' ) continue;

                auto fn = reinterpret_cast< std::uint8_t* >( ntdll + funcs[ ordinals[ i ] ] );
                if ( fn[ 0 ] == 0x4C && fn[ 1 ] == 0x8B && fn[ 2 ] == 0xD1 && fn[ 3 ] == 0xB8 ) {
                    const auto stub_idx = *reinterpret_cast< std::uint32_t* >( fn + 4 );
                    if ( stub_idx == target_idx ) {
                        if ( name_out ) *name_out = name;
                        return fn;
                    }
                }
            }
            return nullptr;
        }

        static bool is_cold_syscall( const char* name ) {
            if ( !name ) return false;

            static const char* k_cold[ ] = {
                "NtQueryPortInformationProcess",
                "NtSystemDebugControl",
                "NtSetCachedSigningLevel2",
                "NtSetCachedSigningLevel",
                "NtCreateDebugObject",
                "NtAllocateReserveObject",
                "NtTestAlert",
                "NtSetLdtEntries",
            };

            for ( const auto* cold : k_cold ) {
                const auto* a = name;
                const auto* b = cold;
                while ( *a && *a == *b ) { ++a; ++b; }
                if ( *a == '\0' && *b == '\0' )
                    return true;
            }
            return false;
        }
    };
}
