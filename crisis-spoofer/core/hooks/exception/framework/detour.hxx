#pragma once

namespace exception {
	namespace hook {
		static constexpr std::size_t k_max_hooks = 32;
		static constexpr std::size_t k_trap_size = 2;
		static constexpr std::size_t k_cave_prefix = 15;
		static constexpr std::size_t k_trampoline_size = k_cave_prefix + 14;

		static std::size_t modrm_length( const std::uint8_t* p ) {
			const auto modrm = p[ 0 ];
			const auto mod = ( modrm >> 6 ) & 3;
			const auto rm = modrm & 7;
			std::size_t sz = 1;
			if ( mod == 3 ) return sz;
			bool sib = ( rm == 4 );
			if ( sib ) {
				++sz;
				if ( mod == 0 && ( p[ 1 ] & 7 ) == 5 ) { sz += 4; return sz; }
			}
			if ( mod == 0 ) { if ( !sib && rm == 5 ) sz += 4; }
			else if ( mod == 1 ) { sz += 1; }
			else { sz += 4; }
			return sz;
		}

		static std::size_t decode_insn_len( const std::uint8_t* p ) {
			std::size_t i = 0;
			bool        rw = false;
			if ( ( p[ i ] & 0xF0 ) == 0x40 ) { rw = ( p[ i ] & 0x08 ) != 0; ++i; }
			const auto op = p[ i++ ];
			switch ( op ) {
			case 0x90: case 0xC3: case 0xCC: return i;
			case 0x50: case 0x51: case 0x52: case 0x53:
			case 0x54: case 0x55: case 0x56: case 0x57:
			case 0x58: case 0x59: case 0x5A: case 0x5B:
			case 0x5C: case 0x5D: case 0x5E: case 0x5F:
				return i;
			case 0x70: case 0x71: case 0x72: case 0x73:
			case 0x74: case 0x75: case 0x76: case 0x77:
			case 0x78: case 0x79: case 0x7A: case 0x7B:
			case 0x7C: case 0x7D: case 0x7E: case 0x7F:
			case 0xEB:
				return i + 1;
			case 0x01: case 0x03: case 0x09: case 0x0B:
			case 0x21: case 0x23: case 0x29: case 0x2B:
			case 0x31: case 0x33: case 0x39: case 0x3B:
			case 0x85: case 0x87:
			case 0x88: case 0x89: case 0x8A: case 0x8B:
			case 0x8D: case 0xFE: case 0xFF:
				return i + modrm_length( p + i );
			case 0x83: case 0xC6:
				return i + modrm_length( p + i ) + 1;
			case 0x81: case 0xC7:
				return i + modrm_length( p + i ) + 4;
			case 0xF6:
				return i + modrm_length( p + i ) + ( ( p[ i ] & 0x38 ) == 0 ? 1 : 0 );
			case 0xF7:
				return i + modrm_length( p + i ) + ( ( p[ i ] & 0x38 ) == 0 ? ( rw ? 8 : 4 ) : 0 );
			case 0xB0: case 0xB1: case 0xB2: case 0xB3:
			case 0xB4: case 0xB5: case 0xB6: case 0xB7:
				return i + 1;
			case 0xB8: case 0xB9: case 0xBA: case 0xBB:
			case 0xBC: case 0xBD: case 0xBE: case 0xBF:
				return i + ( rw ? 8 : 4 );
			case 0xE8: case 0xE9:
				return i + 4;
			case 0x0F: {
				const auto op2 = p[ i++ ];
				switch ( op2 ) {
				case 0x0B: return i;
				case 0x1F:
				case 0x10: case 0x11: case 0x12: case 0x13:
				case 0x28: case 0x29: case 0x2E: case 0x2F:
				case 0x40: case 0x41: case 0x42: case 0x43:
				case 0x44: case 0x45: case 0x46: case 0x47:
				case 0x48: case 0x49: case 0x4A: case 0x4B:
				case 0x4C: case 0x4D: case 0x4E: case 0x4F:
				case 0x57: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
				case 0x6E: case 0x6F: case 0x7E: case 0x7F:
				case 0x94: case 0x95: case 0x96: case 0x97:
				case 0x9E: case 0x9F:
				case 0xA3: case 0xAB: case 0xAD: case 0xAF:
				case 0xAE:
				case 0xB6: case 0xB7: case 0xBE: case 0xBF:
					return i + modrm_length( p + i );
				case 0x80: case 0x81: case 0x82: case 0x83:
				case 0x84: case 0x85: case 0x86: case 0x87:
				case 0x88: case 0x89: case 0x8A: case 0x8B:
				case 0x8C: case 0x8D: case 0x8E: case 0x8F:
					return i + 4;
				default: return 0;
				}
			}
			case 0x64: case 0x65: case 0x66: case 0x67: {
				auto sub = decode_insn_len( p + i );
				return sub ? i + sub : 0;
			}
			default: return 0;
			}
		}

		static bool has_rip_relative_operand( const std::uint8_t* p ) {
			std::size_t i = 0;
			if ( ( p[ i ] & 0xF0 ) == 0x40 ) ++i;
			const auto op = p[ i++ ];
			switch ( op ) {
			case 0x01: case 0x03: case 0x09: case 0x0B:
			case 0x21: case 0x23: case 0x29: case 0x2B:
			case 0x31: case 0x33: case 0x39: case 0x3B:
			case 0x81: case 0x83: case 0x85: case 0x87:
			case 0x88: case 0x89: case 0x8A: case 0x8B:
			case 0x8D: case 0xC6: case 0xC7:
			case 0xF6: case 0xF7: case 0xFE: case 0xFF:
				return ( p[ i ] & 0xC7 ) == 0x05;
			case 0x0F: {
				const auto op2 = p[ i++ ];
				switch ( op2 ) {
				case 0x10: case 0x11: case 0x12: case 0x13:
				case 0x1F:
				case 0x28: case 0x29: case 0x2E: case 0x2F:
				case 0x40: case 0x41: case 0x42: case 0x43:
				case 0x44: case 0x45: case 0x46: case 0x47:
				case 0x48: case 0x49: case 0x4A: case 0x4B:
				case 0x4C: case 0x4D: case 0x4E: case 0x4F:
				case 0x57: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
				case 0x6E: case 0x6F: case 0x7E: case 0x7F:
				case 0x80: case 0x81: case 0x82: case 0x83:
				case 0x84: case 0x85: case 0x86: case 0x87:
				case 0x88: case 0x89: case 0x8A: case 0x8B:
				case 0x8C: case 0x8D: case 0x8E: case 0x8F:
				case 0x94: case 0x95: case 0x96: case 0x97:
				case 0x9E: case 0x9F:
				case 0xA3: case 0xAB: case 0xAD: case 0xAF:
				case 0xAE:
				case 0xB6: case 0xB7: case 0xBE: case 0xBF:
					return ( p[ i ] & 0xC7 ) == 0x05;
				default: return false;
				}
			}
			default: return false;
			}
		}

		static std::size_t compute_cave_prefix( const std::uint8_t* func, std::size_t min_size ) {
			std::size_t offset = 0;
			for ( std::size_t n = 0; n < 64 && offset < min_size; ++n ) {
				const auto* p = func + offset;
				const auto  op = *p;
				const bool is_rel = op == 0xE8 || op == 0xE9 || op == 0xEB ||
					( op >= 0x70 && op <= 0x7F );
				if ( is_rel || has_rip_relative_operand( p ) ||
					( op == 0x0F && p[ 1 ] >= 0x80 && p[ 1 ] <= 0x8F ) )
					return ( offset >= k_trap_size ) ? offset : 0u;
				const auto len = decode_insn_len( p );
				if ( !len ) return 0;
				offset += len;
			}
			return offset;
		}

		inline std::uint8_t k_trap_bytes[ k_trap_size ] = { 0x0F, 0x0B };

		struct hook_state_t {
			std::uint8_t              m_original_bytes[ 64 ] {};
			std::size_t               m_hook_size {};
			std::uint64_t             m_trampoline_va {};
			std::uint64_t             m_target_va {};
			std::uint64_t             m_hooked_page_va {};
			std::uint64_t             m_hooked_page_pa {};
			std::uint64_t             m_target_pa {};
			std::uint64_t             m_original_pte_pa {};
			std::uint64_t             m_original_pte_value {};
			std::uint64_t             m_handler_va {};
			bool                      m_active {};
			paging::shadow::mapping_t m_shadow {};
		};

		inline hook_state_t m_entries[ k_max_hooks ] {};
		inline std::size_t  m_count {};

		hook_state_t* find_entry( std::uint64_t target_va ) {
			for ( std::size_t i = 0; i < m_count; ++i )
				if ( m_entries[ i ].m_active && m_entries[ i ].m_target_va == target_va )
					return &m_entries[ i ];
			return nullptr;
		}

		hook_state_t* find_entry_by_handler( std::uint64_t handler_va ) {
			for ( std::size_t i = 0; i < m_count; ++i )
				if ( m_entries[ i ].m_active && m_entries[ i ].m_handler_va == handler_va )
					return &m_entries[ i ];
			return nullptr;
		}

		bool on_exception( PCONTEXT ctx ) {
			if ( !ctx ) return false;
			const auto entry = find_entry( ctx->Rip );
			if ( !entry ) return false;
			ctx->Rip = entry->m_handler_va;
			return true;
		}

		bool on_execute_fault( std::uint64_t faulting_va, PCONTEXT ctx ) {
			if ( !ctx ) return false;
			const auto entry = find_entry( faulting_va );
			if ( !entry ) return false;
			ctx->Rip = entry->m_handler_va;
			return true;
		}

		template <typename type_t = void*>
		hook_state_t* create_hook( std::uint64_t addr, type_t* original_function = nullptr ) {
			if ( m_count >= k_max_hooks )
				return nullptr;

			paging::pt_entries_t entries;
			if ( !paging::hyperspace_entries( entries, addr ) )
				return nullptr;

			virt_addr_t virt_addr { addr };

			if ( entries.m_pdpte.hard.page_size ) {
				if ( !paging::split_1gb_to_4kb( addr ) ) return nullptr;
				if ( !paging::hyperspace_entries( entries, addr ) ) return nullptr;
			}
			else if ( entries.m_pde.hard.page_size ) {
				if ( !paging::split_2mb_to_4kb( addr ) ) return nullptr;
				if ( !paging::hyperspace_entries( entries, addr ) ) return nullptr;
			}

			if ( !entries.m_pte.hard.present )
				return nullptr;

			const auto target_pa =
				( entries.m_pte.hard.pfn << paging::page_shift ) |
				( addr & paging::page_4kb_mask );

			const auto target_page_base = target_pa & ~static_cast< std::uint64_t >( paging::page_4kb_mask );

			std::uint8_t orig_buffer[ 64 ] {};
			if ( !paging::dpm::read_physical( target_pa, orig_buffer, sizeof( orig_buffer ) ) )
				return nullptr;

			const auto hook_page_va = kernel::mm_allocate_independent_pages( paging::page_4kb_size );
			if ( !hook_page_va )
				return nullptr;

			paging::pt_entries_t hook_page_e {};
			if ( !paging::hyperspace_entries( hook_page_e, hook_page_va ) ||
				!hook_page_e.m_pte.hard.present )
				return nullptr;

			const auto hook_page_pa_base = hook_page_e.m_pte.hard.pfn << paging::page_shift;

			{
				std::uint8_t page_buf[ paging::page_4kb_size ] {};
				if ( !paging::dpm::read_physical( target_page_base, page_buf, sizeof( page_buf ) ) )
					return nullptr;
				if ( !paging::dpm::write_physical( hook_page_pa_base, page_buf, sizeof( page_buf ) ) )
					return nullptr;
			}

			const auto stable_pa = hook_page_pa_base | ( addr & paging::page_4kb_mask );

			const auto canonical_pte_pa =
				( entries.m_pde.hard.pfn << paging::page_shift ) +
				( virt_addr.pte_index * sizeof( pte ) );
			const auto canonical_pte_value = entries.m_pte.value;

			paging::shadow::mapping_t shadow_mapping {};
			if ( !paging::shadow::build(
				stable_pa,
				addr,
				canonical_pte_pa,
				canonical_pte_value,
				shadow_mapping ) )
				return nullptr;

			const auto cave_va = kernel::mm_allocate_independent_pages( paging::page_4kb_size );
			if ( !cave_va )
				return nullptr;

			{
				paging::pt_entries_t tramp_e;
				if ( paging::hyperspace_entries( tramp_e, cave_va ) ) {
					if ( tramp_e.m_pde.hard.page_size ) {
						if ( !paging::split_2mb_to_4kb( cave_va ) ) return nullptr;
						if ( !paging::hyperspace_entries( tramp_e, cave_va ) ) return nullptr;
					}
					if ( tramp_e.m_pte.hard.no_execute ) {
						pte exec_pte = tramp_e.m_pte;
						exec_pte.hard.no_execute = 0;
						exec_pte.hard.read_write = 1;
						const auto tramp_pte_pa =
							( tramp_e.m_pde.hard.pfn << paging::page_shift ) +
							( virt_addr_t { cave_va }.pte_index * sizeof( pte ) );
						paging::dpm::write_physical( tramp_pte_pa, &exec_pte, sizeof( pte ) );
						kernel::ke_flush_entire_tb( true, true );
					}
				}
			}

			paging::pt_entries_t cave_entries;
			if ( !paging::hyperspace_entries( cave_entries, cave_va ) ||
				!cave_entries.m_pte.hard.present )
				return nullptr;

			if ( cave_entries.m_pte.hard.no_execute ) {
				pte exec_pte = cave_entries.m_pte;
				exec_pte.hard.no_execute = 0;
				exec_pte.hard.read_write = 1;
				const auto cave_pte_pa =
					( cave_entries.m_pde.hard.pfn << paging::page_shift ) +
					( virt_addr_t { cave_va }.pte_index * sizeof( pte ) );
				if ( !paging::dpm::write_physical( cave_pte_pa, &exec_pte, sizeof( pte ) ) )
					return nullptr;
				kernel::ke_flush_entire_tb( true, true );
			}

			const auto cave_pa =
				( cave_entries.m_pte.hard.pfn << paging::page_shift ) |
				( cave_va & paging::page_4kb_mask );

			const auto actual_prefix = compute_cave_prefix( orig_buffer, k_cave_prefix );
			if ( !actual_prefix )
				return nullptr;

			if ( !paging::dpm::write_physical( cave_pa, orig_buffer, actual_prefix ) )
				return nullptr;

			std::uint8_t jmp_back[ 14 ] {};
			jmp_back[ 0 ] = 0xFF;
			jmp_back[ 1 ] = 0x25;
			*reinterpret_cast< std::uint32_t* >( &jmp_back[ 2 ] ) = 0;
			*reinterpret_cast< std::uint64_t* >( &jmp_back[ 6 ] ) = addr + actual_prefix;

			if ( !paging::dpm::write_physical( cave_pa + actual_prefix, jmp_back, sizeof( jmp_back ) ) )
				return nullptr;

			if ( original_function )
				*original_function = reinterpret_cast< type_t >( cave_va );

			auto& entry = m_entries[ m_count++ ];
			entry.m_target_va = addr;
			entry.m_hooked_page_va = addr;
			entry.m_hooked_page_pa = stable_pa;
			entry.m_target_pa = target_pa;
			entry.m_original_pte_pa = canonical_pte_pa;
			entry.m_original_pte_value = canonical_pte_value;
			entry.m_trampoline_va = cave_va;
			entry.m_hook_size = k_trap_size;
			entry.m_shadow = shadow_mapping;
			entry.m_active = false;
			std::memcpy( entry.m_original_bytes, orig_buffer, k_trap_size );

			return &entry;
		}

		bool enable_hook( hook_state_t* state, std::uint64_t handler_va ) {
			if ( !state || !state->m_hooked_page_pa || !handler_va )
				return false;

			state->m_handler_va = handler_va;

			if ( !paging::dpm::write_physical(
				state->m_hooked_page_pa, k_trap_bytes, k_trap_size ) )
				return false;

			state->m_active = true;
			return true;
		}

		bool swap_to_hooked( hook_state_t* state ) {
			if ( !state || !state->m_shadow.m_active )
				return false;

			pte canonical {};
			if ( !paging::dpm::read_physical(
				state->m_shadow.m_canonical_pte_pa,
				&canonical, sizeof( pte ) ) )
				return false;

			canonical.hard.pfn = state->m_hooked_page_pa >> paging::page_shift;

			if ( !paging::dpm::write_physical(
				state->m_shadow.m_canonical_pte_pa,
				&canonical, sizeof( pte ) ) )
				return false;

			kernel::ke_flush_entire_tb( true, true );
			return true;
		}

		bool swap_to_clean( hook_state_t* state ) {
			if ( !state || !state->m_shadow.m_active )
				return false;

			pte canonical {};
			if ( !paging::dpm::read_physical(
				state->m_shadow.m_canonical_pte_pa,
				&canonical, sizeof( pte ) ) )
				return false;

			canonical.hard.pfn = state->m_target_pa >> paging::page_shift;

			if ( !paging::dpm::write_physical(
				state->m_shadow.m_canonical_pte_pa,
				&canonical, sizeof( pte ) ) )
				return false;

			kernel::ke_flush_entire_tb( true, true );
			return true;
		}

		bool remove( std::uint64_t target_va, hook_state_t* state ) {
			if ( !target_va || !state )
				return false;

			swap_to_clean( state );

			if ( state->m_shadow.m_active )
				paging::shadow::teardown( state->m_shadow );

			kernel::ke_flush_entire_tb( true, true );

			state->m_active = false;

			for ( std::size_t i = 0; i < m_count; ++i ) {
				if ( m_entries[ i ].m_target_va == target_va ) {
					m_entries[ i ] = m_entries[ --m_count ];
					m_entries[ m_count ] = {};
					break;
				}
			}

			return true;
		}
	}
}