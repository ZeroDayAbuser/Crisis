#pragma once

namespace paging {
	namespace pph {
		struct hook_state_t {
			std::uint8_t  m_original_bytes[ 64 ] { };
			std::size_t   m_hook_size { };
			std::uint64_t m_trampoline_va { };
			std::uint64_t m_hooked_page_va { };
			std::uint64_t m_hooked_page_pa { };
			std::uint64_t m_target_pa { };
			std::uint64_t m_original_pte_pa { };
			std::uint64_t m_original_pte_value { };
			paging::shadow::mapping_t m_shadow { };
		};

		constexpr std::size_t shellcode_size = 14;
		constexpr std::size_t trampoline_size = 64 + 14;

		std::size_t calculate_hook_size(
			std::uint64_t target_pa,
			std::size_t   min_size ) {
			std::uint8_t buffer[ 64 ];
			if ( !dpm::read_physical( target_pa, buffer, sizeof( buffer ) ) )
				return 0;

			std::size_t total_len = 0;
			while ( total_len < min_size && total_len < sizeof( buffer ) ) {
				hde64s hde;
				hde64_disasm( &buffer[ total_len ], &hde );
				if ( hde.len == 0 ) break;
				total_len += hde.len;
			}
			return total_len;
		}

		bool write_trampoline(
			std::uint64_t cave_va,
			std::uint64_t continuation_va,
			std::uint8_t* original_bytes,
			std::size_t   hook_sz ) {
			pt_entries_t entries;
			if ( !paging::hyperspace_entries( entries, cave_va ) ||
				!entries.m_pte.hard.present )
				return false;

			const auto cave_pa =
				( entries.m_pte.hard.pfn << paging::page_shift ) |
				( cave_va & paging::page_4kb_mask );

			if ( !dpm::write_physical( cave_pa, original_bytes, hook_sz ) )
				return false;

			std::uint8_t jmp_back[ 14 ] {};
			jmp_back[ 0 ] = 0xFF;
			jmp_back[ 1 ] = 0x25;
			*reinterpret_cast< std::uint32_t* >( &jmp_back[ 2 ] ) = 0;
			*reinterpret_cast< std::uint64_t* >( &jmp_back[ 6 ] ) = continuation_va;

			if ( !dpm::write_physical(
				cave_pa + hook_sz, jmp_back, sizeof( jmp_back ) ) )
				return false;
			return true;
		}

		template <typename type_t = void*>
		hook_state_t* create_hook(
			std::uint64_t addr,
			type_t* original_function = nullptr ) {
			pt_entries_t entries;
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

			//
			// NOTE: this used to call paging::hide_pages( addr, page_4kb_size ).
			// With the default lock_page = false that marks the *target* page's
			// _MMPFN: MmMarkPhysicalMemoryAsBad(pa, 0x1000) on the page, then
			// m_u4.m_file_only = 1 if that call "succeeded", and -- with no guard
			// at all -- m_u3.m_e3.parity_error = 1.
			//
			// `addr` here is a live, mapped, executing kernel function:
			// KeBugCheckEx, KdpStub, KdpTrap, KeFreezeExecution and
			// SeSecureBootQueryInformation. So this mutated PFN-database state on
			// pages the memory manager was still using, five times, at init. The
			// failure mode is a later PAGE_FAULT_IN_NONPAGED_AREA whose
			// not-present bit is clear -- a present PTE over a page the MM now
			// believes it may reclaim -- in whatever pool consumer hits it next
			// (observed: nt!ExpHpCompactSessionPools+0xcd reading a session-heap
			// page, seconds after "hook state ready! x5").
			//
			// It bought the hook nothing: the jmp lives in the private page reached
			// by the shadow PTE, not in the target page, and nothing about building
			// the hook needs the target's PFN entry touched.
			//

			const auto target_pa =
				( entries.m_pte.hard.pfn << paging::page_shift ) |
				( addr & paging::page_4kb_mask );

			const auto hook_size =
				calculate_hook_size( target_pa, shellcode_size );
			if ( hook_size < shellcode_size )
				return nullptr;

			const auto cave_va = memory::find_cave( hook_size + 14 );
			if ( !cave_va )
				return nullptr;

			std::uint8_t orig_buffer[ 64 ] {};
			if ( !dpm::read_physical(
				target_pa, orig_buffer, sizeof( orig_buffer ) ) )
				return nullptr;

			const auto target_page_base =
				target_pa & ~static_cast< std::uint64_t >( paging::page_4kb_mask );

			const auto hook_page_va =
				kernel::mm_allocate_independent_pages( paging::page_4kb_size );
			if ( !hook_page_va )
				return nullptr;

			pt_entries_t hook_page_e {};
			if ( !paging::hyperspace_entries( hook_page_e, hook_page_va ) ||
				!hook_page_e.m_pte.hard.present )
				return nullptr;

			const auto hook_page_pa_base =
				hook_page_e.m_pte.hard.pfn << paging::page_shift;

			{
				std::uint8_t page_buf[ paging::page_4kb_size ] {};
				if ( !paging::dpm::read_physical( target_page_base, page_buf, sizeof( page_buf ) ) )
					return nullptr;
				if ( !paging::dpm::write_physical( hook_page_pa_base, page_buf, sizeof( page_buf ) ) )
					return nullptr;
			}

			const auto private_pa =
				hook_page_pa_base | ( addr & paging::page_4kb_mask );

			const auto canonical_pte_pa =
				( entries.m_pde.hard.pfn << paging::page_shift ) +
				( virt_addr.pte_index * sizeof( pte ) );
			const auto canonical_pte_value = entries.m_pte.value;

			paging::shadow::mapping_t shadow_mapping { };
			if ( !paging::shadow::build(
				private_pa,
				addr,
				canonical_pte_pa,
				canonical_pte_value,
				shadow_mapping ) ) {
				disk::print( obf( "could not build shadow mappings" ) );
				return nullptr;
			}

			if ( !write_trampoline(
				cave_va,
				addr + hook_size,
				orig_buffer,
				hook_size ) )
				return nullptr;

			if ( original_function )
				*original_function = reinterpret_cast< type_t >( cave_va );

			auto hook_state = reinterpret_cast< hook_state_t* >(
				kernel::mm_allocate_independent_pages( sizeof( hook_state_t ) ) );
			if ( !hook_state )
				return nullptr;

			hook_state->m_original_pte_pa = canonical_pte_pa;
			hook_state->m_original_pte_value = canonical_pte_value;
			hook_state->m_hook_size = hook_size;
			hook_state->m_trampoline_va = cave_va;
			hook_state->m_hooked_page_va = addr;
			hook_state->m_hooked_page_pa = private_pa;
			hook_state->m_target_pa = target_pa;
			hook_state->m_shadow = shadow_mapping;
			std::memcpy(
				hook_state->m_original_bytes, orig_buffer, hook_size );

			disk::print( obf( "hook state ready! [0x%llx -> 0x%llx]" ), addr, cave_va );
			return hook_state;
		}

		bool enable_hook(
			hook_state_t* state,
			void* hook_function ) {
			if ( !state || !state->m_hooked_page_pa )
				return false;

			if ( !state->m_shadow.m_active ) {
				disk::print( obf( "shadow is not active!" ) );
				return false;
			}

			std::uint8_t hook_bytes[ 14 ] {};
			hook_bytes[ 0 ] = 0xFF;
			hook_bytes[ 1 ] = 0x25;
			*reinterpret_cast< std::uint32_t* >( &hook_bytes[ 2 ] ) = 0;
			*reinterpret_cast< std::uint64_t* >( &hook_bytes[ 6 ] ) =
				reinterpret_cast< std::uint64_t >( hook_function );

			if ( !dpm::write_physical(
				state->m_hooked_page_pa, hook_bytes, shellcode_size ) )
				return false;

			if ( state->m_hook_size > shellcode_size ) {
				std::uint8_t nops[ 48 ] {};
				std::memset( nops, 0x90, state->m_hook_size - shellcode_size );
				if ( !dpm::write_physical(
					state->m_hooked_page_pa + shellcode_size,
					nops,
					state->m_hook_size - shellcode_size ) )
					return false;
			}

			return true;
		}

		bool swap_to_hooked( hook_state_t* state ) {
			if ( !state || !state->m_shadow.m_active )
				return false;

			pte canonical {};
			if ( !dpm::read_physical(
				state->m_shadow.m_canonical_pte_pa,
				&canonical, sizeof( pte ) ) )
				return false;

			canonical.hard.pfn =
				state->m_hooked_page_pa >> paging::page_shift;

			if ( !dpm::write_physical(
				state->m_shadow.m_canonical_pte_pa,
				&canonical, sizeof( pte ) ) )
				return false;

			kernel::flush_caches( state->m_hooked_page_va );
			return true;
		}

		bool swap_to_clean( hook_state_t* state ) {
			if ( !state || !state->m_shadow.m_active )
				return false;

			pte canonical {};
			if ( !dpm::read_physical(
				state->m_shadow.m_canonical_pte_pa,
				&canonical, sizeof( pte ) ) )
				return false;

			canonical.hard.pfn =
				state->m_target_pa >> paging::page_shift;

			if ( !dpm::write_physical(
				state->m_shadow.m_canonical_pte_pa,
				&canonical, sizeof( pte ) ) )
				return false;

			kernel::ke_flush_entire_tb( true, true );
			return true;
		}

		bool remove( std::uint64_t target_va, hook_state_t* state ) {
			if ( !target_va || !state )
				return false;

			if ( !state->m_original_pte_pa || !state->m_original_pte_value )
				return false;

			if ( state->m_shadow.m_active ) {
				swap_to_clean( state );

				if ( !paging::shadow::teardown( state->m_shadow ) ) {
					disk::print(
						obf( "shadow teardown failed." ) );
					return false;
				}
			}

			kernel::ke_flush_entire_tb( true, true );

			if ( state->m_trampoline_va ) {
				pt_entries_t cave_entries;
				if ( paging::hyperspace_entries(
					cave_entries, state->m_trampoline_va ) &&
					cave_entries.m_pte.hard.present ) {
					const auto cave_pa =
						( cave_entries.m_pte.hard.pfn << paging::page_shift ) |
						( state->m_trampoline_va & paging::page_4kb_mask );
					std::uint8_t zeros[ trampoline_size ] {};
					dpm::write_physical(
						cave_pa, zeros, state->m_hook_size + 14 );
				}
			}

			disk::print( obf( "hook removed: target=0x%llx" ), target_va );
			return true;
		}

		bool is_hooked(
			std::uint64_t       target_va,
			const hook_state_t* known_state ) {
			if ( !target_va || !known_state || !known_state->m_hook_size )
				return false;

			std::uint64_t target_pa = 0;
			if ( !paging::translate_linear( target_va, &target_pa ) )
				return false;

			std::uint8_t current_bytes[ 64 ];
			if ( !dpm::read_physical(
				target_pa, current_bytes, sizeof( current_bytes ) ) )
				return false;

			return std::memcmp(
				current_bytes,
				known_state->m_original_bytes,
				known_state->m_hook_size ) != 0;
		}
	}
}