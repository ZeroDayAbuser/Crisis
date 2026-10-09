#pragma once

namespace paging {
	namespace pth {
		// Detour hook: installs the same shadow-page trampoline the exception framework
		// uses for its own hooks (paging::pph), rather than a second, competing engine.
		// Anything hooked through here therefore shares one TB-flush / PTE-swap path
		// with KdTrap, KdpStub and KeBugCheckEx.
		using hook_state_t = paging::pph::hook_state_t;

		inline constexpr std::size_t k_max_claimed_pages = 32;
		inline std::uint64_t m_claimed_pages[ k_max_claimed_pages ] {};
		inline volatile long m_claimed_count {};

		// pph shadows a whole 4 KB page per hook and splits 2 MB pages to do it, so two
		// hooks that land on one page fight over m_original_pte_pa and the shadow. Claim
		// the page first and refuse the second hook instead of corrupting the first.
		bool claim_page( std::uint64_t address ) {
			const auto page =
				address & ~static_cast< std::uint64_t >( paging::page_4kb_mask );

			for ( std::size_t i = 0; i < k_max_claimed_pages; ++i )
				if ( m_claimed_pages[ i ] == page )
					return false;

			const auto slot = InterlockedIncrement( &m_claimed_count ) - 1;
			if ( slot < 0 || slot >= static_cast< long >( k_max_claimed_pages ) )
				return false;

			m_claimed_pages[ slot ] = page;
			return true;
		}

		bool create(
			std::uint64_t  target,
			std::uint64_t  replacement,
			std::uint64_t* original,
			hook_state_t*  state )
		{
			if ( !target || !replacement || !state )
				return false;

			if ( !claim_page( target ) ) {
				disk::print( obf( "pth: page 0x%llx already carries a hook" ),
					target & ~static_cast< std::uint64_t >( paging::page_4kb_mask ) );
				return false;
			}

			// Type the out-parameter as void* on purpose: create_hook reinterpret_casts
			// the cave VA into it, and reinterpret_cast from an integral to another
			// integral trips C2440. Going through a pointer type is legal for every
			// instantiation.
			void* original_va = nullptr;
			auto* created = paging::pph::create_hook( target, &original_va );
			if ( !created ) {
				disk::print( obf( "pth: create_hook failed for 0x%llx" ), target );
				return false;
			}

			if ( original )
				*original = reinterpret_cast< std::uint64_t >( original_va );

			if ( !paging::pph::enable_hook(
				created, reinterpret_cast< void* >( replacement ) ) ) {
				disk::print( obf( "pth: enable_hook failed for 0x%llx" ), target );
				return false;
			}

			if ( !paging::pph::swap_to_hooked( created ) ) {
				disk::print( obf( "pth: swap_to_hooked failed for 0x%llx" ), target );
				return false;
			}

			*state = *created;
			return true;
		}

		bool remove( std::uint64_t target, hook_state_t& state ) {
			if ( !target || !state.m_hook_size )
				return false;

			return paging::pph::remove( target, &state );
		}
	}
}
