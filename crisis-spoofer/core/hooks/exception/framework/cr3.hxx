#pragma once

namespace exception {
	namespace dtb {
		inline std::uint64_t m_off_pcb {};
		inline std::uint64_t m_off_dtb {};
		inline std::uint64_t m_off_user_dtb {};
		inline std::uint64_t m_off_links {};
		inline std::uint64_t m_off_module {};
		inline std::uint64_t m_off_peb {};
		inline std::uint64_t m_off_pid {};
		inline std::uint64_t m_ps_head {}; 
		inline std::uint64_t m_off_u4 {}; 
		inline std::uint64_t m_pfn_stride {}; 
		inline std::uint64_t m_pfn_db {}; 
		inline physical_memory_range_t* m_ranges {};

		inline volatile long m_busy {};
		inline volatile long m_log_config {};
		inline volatile long m_log_nofix {};
		inline volatile long m_log_nomatch {};
		inline volatile long m_logged_hits {};
		inline volatile long m_log_user_dtb {};
		inline std::uint64_t m_last_ep {};
		inline std::uint64_t m_last_dtb {};
		inline std::uint64_t m_fixed {};

		inline constexpr std::uint64_t k_scan_cycles = 600'000'000ull;
		inline constexpr std::size_t   k_max_processes = 8192;

		inline std::uint64_t gpr( PCONTEXT ctx, std::uint8_t index ) {
			switch ( index ) {
			case 0:  return ctx->Rax;
			case 1:  return ctx->Rcx;
			case 2:  return ctx->Rdx;
			case 3:  return ctx->Rbx;
			case 4:  return ctx->Rsp;
			case 5:  return ctx->Rbp;
			case 6:  return ctx->Rsi;
			case 7:  return ctx->Rdi;
			case 8:  return ctx->R8;
			case 9:  return ctx->R9;
			case 10: return ctx->R10;
			case 11: return ctx->R11;
			case 12: return ctx->R12;
			case 13: return ctx->R13;
			case 14: return ctx->R14;
			case 15: return ctx->R15;
			default: return 0;
			}
		}

		void set_gpr( PCONTEXT ctx, std::uint8_t index, std::uint64_t value ) {
			switch ( index ) {
			case 0:  ctx->Rax = value; break;
			case 1:  ctx->Rcx = value; break;
			case 2:  ctx->Rdx = value; break;
			case 3:  ctx->Rbx = value; break;
			case 4:  ctx->Rsp = value; break;
			case 5:  ctx->Rbp = value; break;
			case 6:  ctx->Rsi = value; break;
			case 7:  ctx->Rdi = value; break;
			case 8:  ctx->R8  = value; break;
			case 9:  ctx->R9  = value; break;
			case 10: ctx->R10 = value; break;
			case 11: ctx->R11 = value; break;
			case 12: ctx->R12 = value; break;
			case 13: ctx->R13 = value; break;
			case 14: ctx->R14 = value; break;
			case 15: ctx->R15 = value; break;
			default: break;
			}
		}

		// A real CR3 is a page-aligned physical address, so bits 11:0 are clear.
		// The old test looked at the top 16 bits instead, which only ever caught a high
		// value - and the randomised values seen on the protected target are low:
		// 0x2b4c5740 and 0x642c56e0 both pass that test and fail this one.
		bool is_valid_cr3( std::uint64_t value ) {
			return value && ( value & 0xFFFull ) == 0;
		}

		bool is_shuffled( std::uint64_t value ) {
			return !is_valid_cr3( value );
		}

		bool init( ) {
			if ( m_off_dtb && m_pfn_db )
				return true;

			m_off_pcb    = pdb::get_member( obf( "_EPROCESS" ), obf( "Pcb" ) );
			m_off_dtb    = pdb::get_member( obf( "_KPROCESS" ), obf( "DirectoryTableBase" ) );
			// _KPROCESS carries two table bases; the second is the user-mode PML4 the
			// kernel keeps alongside the first. Resolved here, read in stored_dtb().
			m_off_user_dtb = pdb::get_member( obf( "_KPROCESS" ), obf( "UserDirectoryTableBase" ) );
			m_off_links  = pdb::get_member( obf( "_EPROCESS" ), obf( "ActiveProcessLinks" ) );
			m_off_module = pdb::get_member( obf( "_EPROCESS" ), obf( "SectionBaseAddress" ) );
			m_off_peb    = pdb::get_member( obf( "_EPROCESS" ), obf( "Peb" ) );
			m_off_pid    = pdb::get_member( obf( "_EPROCESS" ), obf( "UniqueProcessId" ) );
			m_ps_head    = pdb::get_symbol( obf( "PsActiveProcessHead" ) );

			m_off_u4     = pdb::get_member( obf( "_MMPFN" ), obf( "u4" ) );
			m_pfn_stride = pdb::get_struct_size( obf( "_MMPFN" ) );
			if ( !m_pfn_stride )
				m_pfn_stride = m_off_u4 ? m_off_u4 + sizeof( std::uint64_t ) : 0;

			const auto symbol = pdb::get_symbol( obf( "MmPfnDatabase" ) );
			if ( symbol && kernel::mm_is_address_valid( reinterpret_cast< void* >( symbol ) ) ) {
				__try {
					m_pfn_db = *reinterpret_cast< const std::uint64_t* >( symbol );
				}
				__except ( EXCEPTION_EXECUTE_HANDLER ) {
					m_pfn_db = 0;
				}
			}
			if ( !m_pfn_db ) {
				const auto database = kernel::get_mm_pfn_database( );
				if ( database )
					m_pfn_db = reinterpret_cast< std::uint64_t >( database );
			}

			// m_off_pid belongs here. Excluded, a zero pid offset makes find_eprocess
			// compare every process against 0 and answer no_process for every request,
			// while the gate still reports the config as ready.
			return m_off_dtb && m_off_links && m_off_module &&
				m_ps_head && m_off_pid && m_off_u4 && m_pfn_stride && m_pfn_db;
		}

		bool prepare( ) {
			if ( !init( ) )
				return false;
			if ( !m_ranges )
				m_ranges = kernel::mm_get_physical_memory_ranges( );
			return m_ranges != nullptr;
		}

		bool read_physical( std::uint64_t pa, void* buffer, std::size_t size ) {
			if ( !size || !phys::is_address_valid( pa, size ) )
				return false;
			return paging::dpm::read_physical( pa, buffer, size );
		}

		template< typename entry_t >
		bool read_entry( std::uint64_t pa, entry_t& entry ) {
			return read_physical( pa, &entry, sizeof( entry ) );
		}

		bool chain_ok( std::uint64_t cr3, const virt_addr_t& va ) {
			pml4e pml4 {};
			if ( !read_entry( cr3 + va.pml4e_index * sizeof( pml4e ), pml4 ) ||
				!pml4.hard.present || !pml4.hard.pfn )
				return false;

			pdpte pdpt {};
			if ( !read_entry( ( pml4.hard.pfn << 12 ) + va.pdpte_index * sizeof( pdpte ), pdpt ) ||
				!pdpt.hard.present || !pdpt.hard.pfn )
				return false;
			if ( pdpt.hard.page_size )
				return true;

			pde pd {};
			if ( !read_entry( ( pdpt.hard.pfn << 12 ) + va.pde_index * sizeof( pde ), pd ) ||
				!pd.hard.present || !pd.hard.pfn )
				return false;
			if ( pd.hard.page_size )
				return true;

			pte pt {};
			return read_entry( ( pd.hard.pfn << 12 ) + va.pte_index * sizeof( pte ), pt ) &&
				pt.hard.present && pt.hard.pfn;
		}

		bool translate( std::uint64_t cr3, std::uint64_t address, std::uint64_t& out_pa ) {
			const virt_addr_t va { address };

			pml4e pml4 {};
			if ( !read_entry( cr3 + va.pml4e_index * sizeof( pml4e ), pml4 ) ||
				!pml4.hard.present )
				return false;

			pdpte pdpt {};
			if ( !read_entry( ( pml4.hard.pfn << 12 ) + va.pdpte_index * sizeof( pdpte ), pdpt ) ||
				!pdpt.hard.present )
				return false;
			if ( pdpt.hard.page_size ) {
				out_pa = ( ( pdpt.hard.pfn & 0x3FFFFull ) << 30 ) |
					( address & 0x3FFFFFFFull );
				return true;
			}

			pde pd {};
			if ( !read_entry( ( pdpt.hard.pfn << 12 ) + va.pde_index * sizeof( pde ), pd ) ||
				!pd.hard.present )
				return false;
			if ( pd.hard.page_size ) {
				out_pa = ( ( pd.hard.pfn & 0x1FFFFFull ) << 21 ) |
					( address & 0x1FFFFFull );
				return true;
			}

			pte pt {};
			if ( !read_entry( ( pd.hard.pfn << 12 ) + va.pte_index * sizeof( pte ), pt ) ||
				!pt.hard.present )
				return false;

			out_pa = ( pt.hard.pfn << 12 ) | ( address & 0xFFFull );
			return true;
		}

		bool read_u4( std::uint64_t pfn, std::uint64_t& u4 ) {
			const auto address = m_pfn_db + m_off_u4 + pfn * m_pfn_stride;
			if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( address ) ) )
				return false;

			__try {
				u4 = *reinterpret_cast< const std::uint64_t* >( address );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
				return false;
			}
			return true;
		}

		std::uint64_t scan( std::uint64_t module_va, std::uint64_t peb_va ) {
			const auto ranges = m_ranges;
			if ( !ranges || !module_va )
				return 0;

			const auto deadline = __rdtsc( ) + k_scan_cycles;
			const virt_addr_t module { module_va };
			const virt_addr_t peb { peb_va };
			std::uint64_t scanned = 0;

			for ( auto index = 0ul;
				ranges[ index ].m_base_page.m_quad_part ||
					ranges[ index ].m_page_count.m_quad_part; ++index ) {
				const auto base  = ranges[ index ].m_base_page.m_quad_part;
				const auto bytes = ranges[ index ].m_page_count.m_quad_part;
				if ( !bytes || base + bytes < base )
					continue;

				const auto end = base + bytes;
				for ( auto candidate = ( base + 0xFFFull ) & ~0xFFFull;
					candidate < end && end - candidate >= 0x1000;
					candidate += 0x1000 ) {
					if ( ( ++scanned & 0xFFFF ) == 0 && __rdtsc( ) >= deadline )
						return 0;

					const auto pfn = candidate >> 12;

					std::uint64_t u4 = 0;
					if ( !read_u4( pfn, u4 ) )
						continue;

					if ( ( u4 & 0xFFFFFFFFFFull ) != pfn )
						continue;

					if ( !chain_ok( candidate, module ) )
						continue;

					if ( peb_va && !chain_ok( candidate, peb ) )
						continue;

					std::uint64_t module_pa = 0;
					if ( !translate( candidate, module_va, module_pa ) )
						continue;

					std::uint16_t magic = 0;
					if ( !read_physical( module_pa, &magic, sizeof( magic ) ) ||
						magic != 0x5A4D )
						continue;

					return candidate;
				}
			}

			return 0;
		}

		// offset 0 is a legal read, and both of the ways we use it are with offset 0:
		// _EPROCESS.Pcb is at 0x000, and a list head's own Flink is read at its own
		// address. Guarding on !offset returned zero for both - which is exactly why
		// find_eprocess walked nothing (visited=0) and read_object_dtb always answered
		// 0x0 while the config gate reported m_off_pcb as resolved.
		std::uint64_t read_field( std::uint64_t base, std::uint64_t offset ) {
			if ( !base )
				return 0;

			std::uint64_t value = 0;

			__try {
				value = *reinterpret_cast< const std::uint64_t* >( base + offset );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
				return 0;
			}

			return value;
		}

		// Read straight off the process object - no hook, no context capture.
		// _KPROCESS carries two table bases. DirectoryTableBase (+0x28) is the field the
		// target overwrites with an allocated-but-random region; UserDirectoryTableBase
		// (+0x388) is the user-mode PML4 the kernel keeps alongside it. Take whichever
		// one is actually a page-aligned physical address.
		std::uint64_t stored_dtb( std::uint64_t eprocess ) {
			if ( !eprocess || !m_off_dtb )
				return 0;

			const auto base = eprocess + m_off_pcb;

			const auto primary = read_field( base, m_off_dtb );
			if ( is_valid_cr3( primary ) )
				return primary;

			if ( m_off_user_dtb ) {
				const auto secondary = read_field( base, m_off_user_dtb );
				if ( is_valid_cr3( secondary ) ) {
					return secondary;
				}
			}

			return primary;
		}

			// nt clears bit 63 itself on the way into cr3 - SwapContext does
			//   mov [rbx+8E80h], rcx / btr rcx, 3Fh / mov cr3, rcx
			// so the operand we decode is the field with bit 63 cleared. An exact compare
			// therefore misses whenever the shuffle set that bit. Compare the significant bits
			// and let check_dtb reject a wrong candidate on the merits.
			bool same_table_base( std::uint64_t a, std::uint64_t b ) {
				constexpr std::uint64_t k_sign = ~( 1ull << 63 );
				return a && b && ( a & k_sign ) == ( b & k_sign );
			}

			// The walk matches the operand against the object field, and fix() overwrites that
			// field with the real table base. So the search can only ever succeed once per
			// process: after the first repair the field no longer holds the value the next
			// switch faults on, and on_cr3_fault bails before it reaches fix(). The pairing has
			// to be remembered rather than re-derived from an object we already changed.
			struct cr3_pair_t {
				std::uint64_t m_shuffled {};
				std::uint64_t m_eprocess {};
				std::uint64_t m_real {};
			};

			inline constexpr std::size_t k_pair_slots = 8;
			inline cr3_pair_t m_pairs[ k_pair_slots ] {};
			inline volatile long m_pair_next {};

			std::uint64_t pair_lookup( std::uint64_t shuffled, std::uint64_t& out_eprocess ) {
				for ( std::size_t i = 0; i < k_pair_slots; ++i ) {
					if ( same_table_base( m_pairs[ i ].m_shuffled, shuffled ) && m_pairs[ i ].m_real ) {
						out_eprocess = m_pairs[ i ].m_eprocess;
						return m_pairs[ i ].m_real;
					}
				}
				return 0;
			}

			void pair_insert( std::uint64_t shuffled, std::uint64_t eprocess, std::uint64_t real ) {
				const auto slot = static_cast< std::size_t >(
					InterlockedIncrement( &m_pair_next ) & ( k_pair_slots - 1 ) );

				m_pairs[ slot ].m_shuffled = 0;
				m_pairs[ slot ].m_eprocess = eprocess;
				m_pairs[ slot ].m_real = real;
				m_pairs[ slot ].m_shuffled = shuffled;
			}

		bool find_process( std::uint64_t value, std::uint64_t& out_eprocess ) {
			if ( !m_ps_head || !m_off_links )
				return false;

			std::uint64_t link = 0;
			__try {
				link = *reinterpret_cast< const std::uint64_t* >( m_ps_head );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
				return false;
			}

			for ( std::size_t i = 0;
				i < k_max_processes && link && link != m_ps_head; ++i ) {
				const auto eprocess = link - m_off_links;

				const auto base = eprocess + m_off_pcb;

				if ( same_table_base( read_field( base, m_off_dtb ), value ) ||
					( m_off_user_dtb && same_table_base(
						read_field( base, m_off_user_dtb ), value ) ) ) {
					out_eprocess = eprocess;
					return true;
				}

				__try {
					link = *reinterpret_cast< const std::uint64_t* >( link );
				}
				__except ( EXCEPTION_EXECUTE_HANDLER ) {
					return false;
				}
			}

			return false;
		}

		std::uint64_t process_module( std::uint64_t eprocess ) {
			std::uint64_t module = 0;
			__try {
				module = *reinterpret_cast< const std::uint64_t* >(
					eprocess + m_off_module );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
				return 0;
			}
			return module;
		}

		std::uint64_t process_peb( std::uint64_t eprocess ) {
			std::uint64_t peb = 0;
			__try {
				peb = *reinterpret_cast< const std::uint64_t* >(
					eprocess + m_off_peb );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
				return 0;
			}
			return peb;
		}

		std::uint32_t process_id( std::uint64_t eprocess ) {
			return static_cast< std::uint32_t >( read_field( eprocess, m_off_pid ) );
		}

		std::uint64_t fix( std::uint64_t eprocess ) {
			if ( !eprocess )
				return 0;

			if ( eprocess == m_last_ep && m_last_dtb )
				return m_last_dtb;

			const auto module = process_module( eprocess );
			if ( !module ) {
				if ( InterlockedCompareExchange( &m_log_nofix, 1, 0 ) == 0 )
					disk::print( obf( "dtb: ep=0x%llx has no module base; not resolving" ),
						eprocess );
				return 0;
			}

			const auto real = scan( module, process_peb( eprocess ) );
			if ( !real ) {
				if ( InterlockedCompareExchange( &m_log_nofix, 1, 0 ) == 0 )
					disk::print( obf( "dtb: no candidate for ep=0x%llx pid=0x%llx module=0x%llx" ),
						eprocess, process_id( eprocess ), module );
				return 0;
			}

			__try {
				*reinterpret_cast< volatile std::uint64_t* >(
					eprocess + m_off_pcb + m_off_dtb ) = real;
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
				return 0;
			}

			m_last_ep  = eprocess;
			m_last_dtb = real;
			++m_fixed;

			return real;
		}

		struct cr3_write_t {
			std::size_t m_length {};
			std::uint8_t m_mod {};
			std::uint8_t m_rm {};
			std::uint8_t m_rex {};
			bool m_memory {};
			bool m_ok {};
		};

		cr3_write_t decode_cr3_write( std::uint64_t rip ) {
			cr3_write_t result {};
			if ( ( rip & 0xFFFull ) > 0xFF0ull )
				return result;

			if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( rip ) ) ||
				 !kernel::mm_is_address_valid( reinterpret_cast< void* >( rip + 15 ) ) )
				return result;

			const auto* p = reinterpret_cast< const std::uint8_t* >( rip );

			std::size_t at = 0;
			if ( ( p[ 0 ] & 0xF0 ) == 0x40 ) {
				result.m_rex = p[ 0 ];
				at = 1;
			}

			if ( p[ at ] != 0x0F || p[ at + 1 ] != 0x22 )
				return result;

			const auto modrm = p[ at + 2 ];
			if ( ( ( modrm >> 3 ) & 7 ) != 3 )
				return result;

			result.m_mod    = modrm >> 6;
			result.m_rm     = modrm & 7;
			result.m_memory = result.m_mod != 3;

			auto length = at + 3;
			if ( result.m_memory ) {
				if ( result.m_rm == 4 ) {
					const auto sib = p[ length ];
					length += 1;
					if ( ( sib & 7 ) == 5 && result.m_mod == 0 )
						length += 4;
				}
				else if ( result.m_rm == 5 && result.m_mod == 0 ) {
					length += 4;
				}

				if ( result.m_mod == 1 )
					length += 1;
				else if ( result.m_mod == 2 )
					length += 4;
			}

			result.m_length = length;
			result.m_ok = true;
			return result;
		}

		bool cr3_write_source( PCONTEXT ctx, std::uint64_t rip,
			const cr3_write_t& write, std::uint64_t& value, std::uint64_t& operand_va ) {
			operand_va = 0;

			if ( !write.m_memory ) {
				value = gpr( ctx, static_cast< std::uint8_t >(
					write.m_rm | ( ( write.m_rex & 0x01 ) ? 8 : 0 ) ) );
				return true;
			}

			const auto* p = reinterpret_cast< const std::uint8_t* >( rip );
			std::size_t at = write.m_rex ? 1 : 0;
			const auto modrm = p[ at + 2 ];
			const auto mod = modrm >> 6;

			std::uint64_t address = 0;
			std::uint8_t base = 0;
			std::uint8_t index = 0;
			std::size_t at2 = at + 3;

			if ( write.m_rm == 4 ) {
				const auto sib = p[ at2 ];
				at2 += 1;
				base  = sib & 7;
				index = ( sib >> 3 ) & 7;
				if ( index != 4 ) {
					index = static_cast< std::uint8_t >(
						index | ( ( write.m_rex & 0x02 ) ? 8 : 0 ) );
					address += gpr( ctx, index );
				}
				if ( ( sib & 7 ) == 5 && mod == 0 ) {
					std::int32_t disp = 0;
					std::memcpy( &disp, p + at2, sizeof( disp ) );
					at2 += 4;
					address += static_cast<std::uint64_t>(
						static_cast<std::int64_t>( disp ) );
				}
				else {
					base = static_cast<std::uint8_t >(
						base | ( ( write.m_rex & 0x01 ) ? 8 : 0 ) );
					address += gpr( ctx, base );
				}
			}
			else if ( write.m_rm == 5 && mod == 0 ) {
				std::int32_t disp = 0;
				std::memcpy( &disp, p + at2, sizeof( disp ) );
				at2 += 4;
				address = static_cast<std::uint64_t>( static_cast<std::int64_t>(
					rip + write.m_length ) ) + static_cast<std::int64_t>( disp );
			}
			else {
				const auto reg = static_cast<std::uint8_t >(
					write.m_rm | ( ( write.m_rex & 0x01 ) ? 8 : 0 ) );
				address = gpr( ctx, reg );
			}

			if ( mod == 1 ) {
				address += static_cast<std::uint64_t>(
					static_cast<std::int64_t>( static_cast<std::int8_t>( p[ at2 ] ) ) );
			}
			else if ( mod == 2 ) {
				std::int32_t disp = 0;
				std::memcpy( &disp, p + at2, sizeof( disp ) );
				address += static_cast<std::uint64_t>(
					static_cast<std::int64_t>( disp ) );
			}

			operand_va = address;

			if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( address ) ) )
				return false;

			__try {
				value = *reinterpret_cast< const std::uint64_t* >( address );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
				return false;
			}
			return true;
		}

		bool on_cr3_fault( PCONTEXT ctx, PEXCEPTION_RECORD record ) {
			if ( !ctx || !record )
				return false;

			if ( record->ExceptionCode != STATUS_PRIVILEGED_INSTRUCTION &&
				 record->ExceptionCode != STATUS_ACCESS_VIOLATION &&
				 record->ExceptionCode != STATUS_ILLEGAL_INSTRUCTION )
				return false;

			if ( !init( ) )
				return false;

			const auto rip = static_cast< std::uint64_t >( ctx->Rip );
			const auto write = decode_cr3_write( rip );
			if ( !write.m_ok )
				return false;

			std::uint64_t value = 0;
			std::uint64_t operand_va = 0;
			if ( !cr3_write_source( ctx, rip, write, value, operand_va ) )
				return false;

			if ( !is_shuffled( value ) )
				return false;

			if ( InterlockedCompareExchange( &m_busy, 1, 0 ) != 0 )
				return false;

			std::uint64_t eprocess = 0;
			auto real = pair_lookup( value, eprocess );

			if ( !real ) {
				if ( !find_process( value, eprocess ) ) {
					InterlockedExchange( &m_busy, 0 );
					if ( InterlockedIncrement( &m_log_nomatch ) <= 8 )
						disk::print( obf( "dtb: no process holds 0x%llx (rip=0x%llx)" ),
							value, rip );
					return false;
				}

				if ( !fix( eprocess ) ) {
					InterlockedExchange( &m_busy, 0 );
					return false;
				}

				real = m_last_dtb;
				pair_insert( value, eprocess, real );
			}

			InterlockedExchange( &m_busy, 0 );
			const auto field = eprocess + m_off_pcb + m_off_dtb;
			if ( !write.m_memory ) {
				set_gpr( ctx, static_cast< std::uint8_t >(
					write.m_rm | ( ( write.m_rex & 0x01 ) ? 8 : 0 ) ), real );
			}
			else if ( operand_va != field ) {
				if ( InterlockedIncrement( &m_logged_hits ) <= 8 )
					disk::print( obf( "dtb: fixed ep=0x%llx pid=0x%llx dtb=0x%llx but operand "
						"0x%llx is not the field; leaving the switch alone" ),
						eprocess, process_id( eprocess ), real, operand_va );
				return false;
			}

			if ( InterlockedIncrement( &m_logged_hits ) <= 8 )
				disk::print( obf( "dtb: repaired live switch ep=0x%llx pid=0x%llx "
					"shuffled=0x%llx real=0x%llx rip=0x%llx" ),
					eprocess, process_id( eprocess ), value, real, rip );

			ctx->Rip = rip - write.m_length;

			return true;
		}

		//
		// Process attachment, ported from crisis-bypass, whose equivalents are verified
		// stable: same walk, same bounds, same proof of a candidate table base.
		//

		std::uint64_t find_eprocess( std::uint32_t wanted ) {
			if ( !wanted || !m_ps_head || !m_off_links || !m_off_pid )
				return 0;

			std::uint32_t seen[ 4 ] { };
			std::size_t   visited = 0;

			auto link = read_field( m_ps_head, 0 );

			for ( std::size_t i = 0;
				i < k_max_processes && link && link != m_ps_head; ++i ) {
				const auto eprocess = link - m_off_links;
				const auto pid = process_id( eprocess );

				if ( visited < 4 )
					seen[ visited ] = pid;

				++visited;

				if ( pid == wanted )
					return eprocess;

				link = read_field( link, 0 );
			}

			static volatile long logged = 0;
			if ( InterlockedCompareExchange( &logged, 1, 0 ) == 0 )
				disk::print( obf( "dtb: pid %u not in list head=0x%llx links=0x%llx "
					"pid_off=0x%llx visited=%llu first=%u,%u,%u,%u" ),
					wanted, m_ps_head, m_off_links, m_off_pid,
					static_cast< std::uint64_t >( visited ),
					seen[ 0 ], seen[ 1 ], seen[ 2 ], seen[ 3 ] );

			return 0;
		}

		// A candidate is proven the way crisis-bypass proves one: every level of the
		// walk present with a nonzero frame, an optional PEB chain on top of it, and MZ
		// where the module is meant to be. No guessing from the shape of the value.
		bool check_dtb( std::uint64_t cr3, std::uint64_t module_va,
			std::uint64_t peb_va = 0 )
		{
			if ( !is_valid_cr3( cr3 ) || !module_va )
				return false;

			std::uint64_t module_pa = 0;
			if ( !translate( cr3, module_va, module_pa ) )
				return false;

			if ( peb_va ) {
				std::uint64_t peb_pa = 0;
				if ( !translate( cr3, peb_va, peb_pa ) )
					return false;
			}

			std::uint16_t magic = 0;
			return read_physical( module_pa, &magic, sizeof( magic ) ) &&
				magic == 0x5A4D;
		}

		// Read the table base straight off the process object pointer and keep whichever
		// of the two _KPROCESS fields survives the proof above.
		std::uint64_t read_object_dtb( std::uint64_t eprocess ) {
			if ( !eprocess || !m_off_dtb )
				return 0;

			const auto module = process_module( eprocess );
			if ( !module )
				return 0;

			const auto base = eprocess + m_off_pcb;
			const auto peb = process_peb( eprocess );

			const auto primary = read_field( base, m_off_dtb );
			if ( check_dtb( primary, module, peb ) )
				return primary;

			if ( m_off_user_dtb ) {
				const auto secondary = read_field( base, m_off_user_dtb );
				if ( check_dtb( secondary, module, peb ) ) {
					if ( InterlockedCompareExchange( &m_log_user_dtb, 1, 0 ) == 0 )
						disk::print( obf( "dtb: ep=0x%llx DirectoryTableBase=0x%llx rejected; "
							"using UserDirectoryTableBase=0x%llx" ),
							eprocess, primary, secondary );
					return secondary;
				}
			}

			return 0;
		}
	}
}
