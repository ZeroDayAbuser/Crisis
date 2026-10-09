#pragma once
#include <shared_mutex>

namespace syscall {
	struct section_range_t {
		std::uint64_t start;
		std::uint64_t end;
	};

    class c_syscall {
    public:
        bool setup( ) {
            if ( m_ssdt_base || m_ntdll_stub )
                return true;

            const auto ke_service_descriptor_table =
                g_scanner->get_symbol( obf( "KeServiceDescriptorTable" ) );
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

            for ( std::size_t idx = 1; idx < nt_count; idx++ ) {
                const auto entry = nt_table[ idx ];
                if ( !entry ) continue;

                const auto param_count = entry & 0xF;
                if ( !param_count || param_count > 16 ) continue;

                const auto fn_va = m_ssdt_base + ( static_cast< int >( entry ) >> 4 );
                if ( fn_va <= m_ssdt_base ) continue;

                m_saved_entry = entry;
                m_param_count = param_count;
                m_entry_va    = m_ssdt_base + idx * sizeof( std::uint32_t );
                m_entry_pa    = g_paging->translate_linear( m_entry_va );
                m_entry_idx   = static_cast< std::uint32_t >( idx );

                m_ntdll_stub = find_ntdll_stub( static_cast< std::uint32_t >( idx ) );
                if ( !m_ntdll_stub )
                    continue;

                break;
            }

            if ( !m_ntdll_stub ) {
                logging::print( obf( "failed to find valid syscall entry" ) );
                return false;
            }

			const auto [exec_section, data_section] = find_sections( );
			if ( data_section.empty( ) || exec_section.empty( ) ) {
				logging::print( obf( "failed to find ntoskrnl sections" ) );
				return true;
			}

			auto is_data = [ &data_section ]( std::uint64_t addr ) -> bool {
				for ( auto& sec : data_section )
					if ( addr >= sec.start && addr + sizeof( std::uint64_t ) <= sec.end )
						return true;
				return false;
				};

			for ( auto& [exec_start, exec_end] : exec_section ) {
				const auto size = static_cast< std::size_t >( exec_end - exec_start );
				std::vector< std::uint8_t > bytes( size );
				if ( !memory::read_virtual( exec_start, bytes.data( ), size ) )
					continue;

				for ( auto idx = 0; idx + 6 <= size; idx++ ) {
					const auto gadget_va = exec_start + idx;

					auto slot = 0ull;
					if ( bytes[ idx ] == 0xFF && bytes[ idx + 1 ] == 0x25 ) {
						const auto disp = *reinterpret_cast< const std::int32_t* >( &bytes[ idx + 2 ] );
						slot = gadget_va + 6 + static_cast< std::int64_t >( disp );
					}

					if ( idx + 9 <= size &&
						bytes[ idx ] == 0x48 && bytes[ idx + 1 ] == 0x8B && bytes[ idx + 2 ] == 0x05 &&
						bytes[ idx + 7 ] == 0xFF && bytes[ idx + 8 ] == 0xE0 ) {
						const auto disp = *reinterpret_cast< const std::int32_t* >( &bytes[ idx + 3 ] );
						slot = gadget_va + 7 + static_cast< std::int64_t >( disp );
					}

					const auto rva = static_cast< std::int64_t >( gadget_va ) -
						static_cast< std::int64_t >( m_ssdt_base );
					if ( rva < INT32_MIN / 16 || rva > INT32_MAX / 16 )
						continue;

					if ( !is_data( slot ) )
						continue;

					const auto slot_pa = g_paging->translate_linear( slot );
					if ( !g_paging->hyperspace_entries( gadget_va ) || !slot_pa )
						continue;

					std::uint64_t saved {};
					if ( !memory::read_virtual( slot, &saved, sizeof( saved ) ) )
						continue;

					m_gadget_va = gadget_va;
					m_slot_va = slot;
					m_slot_pa = slot_pa;
					m_slot_saved = saved;
					return true;
				}
			}

			logging::print( obf( "warning: failed to find gadget stub\n" ) );
            return true;
        }

		template< typename ret_t = std::uint64_t,
			typename a1_t = void*, typename a2_t = void*,
			typename a3_t = void*, typename a4_t = void*,
			typename a5_t = void*, typename a6_t = void*,
			typename a7_t = void*, typename a8_t = void*,
			typename a9_t = void*, typename a10_t = void* >
		ret_t call_kernel( std::uint64_t func,
			a1_t  a1 = {}, a2_t  a2 = {}, a3_t  a3 = {},
			a4_t  a4 = {}, a5_t  a5 = {}, a6_t  a6 = {},
			a7_t  a7 = {}, a8_t  a8 = {}, a9_t  a9 = {},
			a10_t a10 = {} ) {

			if ( !m_entry_pa || !m_ntdll_stub ) {
				logging::print( obf( "syscall not initialized" ) );
				if constexpr ( std::is_void_v< ret_t > ) return;
				else return ret_t {};
			}

			const auto direct_rva_i64 = static_cast< std::int64_t >( func ) -
				static_cast< std::int64_t >( m_ssdt_base );
			const bool direct_valid = direct_rva_i64 >= INT32_MIN / 16 &&
				direct_rva_i64 <= INT32_MAX / 16;

			const bool use_gadget = !direct_valid && m_slot_pa;
			if ( !direct_valid && !use_gadget ) {
				logging::print( obf( "syscall: rva out of range, no gadget available" ) );
				if constexpr ( std::is_void_v< ret_t > ) return;
				else return ret_t {};
			}

			std::unique_lock lock( m_lock );

			auto effective = func;
			if ( use_gadget ) {
				if ( !memory::write_physical( m_slot_pa, &func, sizeof( func ) ) ) {
					if constexpr ( std::is_void_v< ret_t > ) return;
					else return ret_t {};
				}
				effective = m_gadget_va;
			}

			const auto new_rva = static_cast< int >( effective - m_ssdt_base ) << 4;
			auto       new_entry = ( new_rva & 0xfffffff0 ) | ( m_saved_entry & 0xf );

			if ( !memory::write_physical( m_entry_pa, &new_entry, sizeof( new_entry ) ) ) {
				if ( use_gadget )
					memory::write_physical( m_slot_pa, &m_slot_saved, sizeof( m_slot_saved ) );
				if constexpr ( std::is_void_v< ret_t > ) return;
				else return ret_t {};
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

		std::pair < std::vector< section_range_t >, std::vector< section_range_t > > find_sections( ) {
			std::vector< section_range_t > exec_data, writable_data;

			const auto ntoskrnl_base = g_scanner->get_symbol( obf( "ntoskrnl_base" ) );
			if ( !ntoskrnl_base ) return { };

			dos_header_t dos_header { };
			if ( !memory::read_virtual( ntoskrnl_base, &dos_header, sizeof( dos_header ) ) || !dos_header.is_valid( ) )
				return { };

			nt_headers_t nt_headers { };
			if ( !memory::read_virtual( ntoskrnl_base + static_cast<std::uint64_t>( dos_header.m_lfanew ), &nt_headers, sizeof( nt_headers ) ) || !nt_headers.is_valid( ) )
				return { };

			const auto sec_count = nt_headers.m_number_of_sections;
			const auto sec_addr = ntoskrnl_base + static_cast<std::uint64_t>( dos_header.m_lfanew ) + 0x18
				+ static_cast<std::uint64_t>( nt_headers.m_size_of_optional_header );

			std::vector< section_header_t > secs( sec_count );
			if ( !memory::read_virtual( sec_addr, secs.data( ), sec_count * sizeof( section_header_t ) ) )
				return { };

			for ( auto& sec : secs ) {
				const bool writable = !!( sec.m_characteristics & IMAGE_SCN_MEM_WRITE );
				const bool exec = !!( sec.m_characteristics & IMAGE_SCN_MEM_EXECUTE );
				const bool is_gap = sec.m_size_of_raw_data == 0;
				const auto start = ntoskrnl_base + static_cast<std::uint64_t>( sec.m_virtual_address );
				const auto end = start + static_cast<std::uint64_t>( sec.m_virtual_size );
				if ( exec && !writable ) exec_data.push_back( { start, end } );
				if ( writable && !exec && !is_gap ) writable_data.push_back( { start, end } );
			}

			return { exec_data, writable_data };
		}

        void* find_ntdll_stub( std::uint32_t target_idx ) {
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
                    if ( stub_idx == target_idx )
                        return fn;
                }
            }
            return nullptr;
        }
    };
}
