#pragma once

//
// Reconstructed from nt!MiCopyOnWrite (ntoskrnl 19041.6456, 0x14023EC70).
//
// nt's MiCopyOnWrite is a generic copy-on-write routine: it switches on the
// region type (MiGetSystemRegionType), picks a session or multiplexed VM,
// allocates a page through the page-colour allocator, binds it into the PFN
// database, copies the source page with MiCopyPage, rebuilds the PTE, then
// installs it with MI_INTERLOCKED_EXCHANGE_PTE + MiInsertTbFlushEntry +
// MiFlushTbList.
//
// Its purpose is to serve a writer that faulted on a read-only page, and for
// that it only handles session and multiplexed regions -- a driver image lives
// in the system region and lands in the `default:` arm, which bails out with
// 0xC00000B4 whenever nt's guard condition holds. That is why the emulator's
// seed() never took the private-page route and fell back to a shadow PTE swap.
//
// This is the same mechanism with that purpose stripped. Only the steps that
// actually produce a private page are kept:
//
//   original                                    here
//   ------------------------------------------  --------------------------
//   Page = MiGetPage( ... )                     kernel::mm_allocate_indep_pages
//   MiCopyPage( Page, v112, 0, v75 )            kernel::rtl_copy_memory
//   v80 = original attrs, PFN -> Page           private_pte = original_pte
//                                               with only .pfn replaced
//   MI_INTERLOCKED_EXCHANGE_PTE( v52, v80 )     dpm::write_physical( pte_pa )
//   MiInsertTbFlushEntry + MiFlushTbList        kernel::ke_flush_entire_tb
//
// Deliberately omitted, and why:
//   * MiGetSessionVm / MiGetAnyMultiplexedVm / MiComputeFaultNode / page
//     colouring -- region- and VM-specific, and all of it only exists to choose
//     where the replacement page comes from. Ours comes from the kernel
//     allocator, which already returns a zeroed page with a valid _MMPFN entry,
//     so the PFN database stays consistent for free.
//   * the _MMPFN writes (PteAddress = MiSwizzleInvalidPte(...), refcount,
//     original-PTE frame, MiSetPfnTbFlushStamp, MiIdentifyPfn) -- the original
//     must bind a page it just pulled off the free list to the target PTE.
//     Ours is already bound, by its own VA; rewriting PteAddress would aim the
//     kernel's aging/flush bookkeeping at a frame it does not own.
//   * MiDecrementShareCount on the source prototype PFN -- the original does
//     this to detach the writer from the shared page. On an image page the
//     section holds its own reference, so dropping ours only makes the share
//     count on a live image page observable, for no benefit.
//   * EtwTraceKernelEvent / MiLogPageAccess / WSL and user-mode handling --
//     telemetry, and a purpose (user-mode writes) we do not have.
//   * MiWritePteShadow -- KVA shadowing is off on this target (the loader
//     asserts it), so there is no shadow PTE to mirror.
//

namespace paging {
    struct privatize_result_t {
        std::uint64_t m_private_pa{ };
        std::uint64_t m_private_va{ };
        std::uint64_t m_pte_pa{ };
        std::uint64_t m_original_pte{ };
        std::uint64_t m_source_pfn{ };
        bool          m_ok{ };
    };

    // Undo privatize_page(): put the original PTE value back and drop the private
    // page. Note the module's own page was never written, so unlike the old
    // "patch the page, restore the bytes" path there is nothing to copy back --
    // only the PTE to restore.
    bool privatize_restore( const privatize_result_t& state ) {
        if ( !state.m_ok || !state.m_pte_pa )
            return false;

        pte original{ };
        original.value = state.m_original_pte;

        if ( !dpm::write_physical( state.m_pte_pa, &original, sizeof( pte ) ) )
            return false;

        kernel::ke_flush_entire_tb( true, true );

        // MmFreeIndependentPages requires PASSIVE_LEVEL. If we are above it, leak
        // the page rather than bugcheck on teardown.
        if ( state.m_private_va && KeGetCurrentIrql( ) == PASSIVE_LEVEL )
            kernel::mm_free_independent_pages( state.m_private_va, page_4kb_size );

        return true;
    }

    // Privatizes the 4 KiB page containing `va`: gives it a fresh backing page and
    // copies the original PTE's attributes onto it, so the module's own section
    // page is never written to. Returns the private page's base physical/virtual
    // addresses and everything needed to undo it.
    //
    //   copy_contents : also copy the original page's bytes into the private page
    //                   (the role MiCopyPage plays in the original). false leaves
    //                   it demand-zero.
    //   install       : swap the target PTE to the private page now (the role of
    //                   MI_INTERLOCKED_EXCHANGE_PTE + MiFlushTbList).
    bool privatize_page( std::uint64_t va, privatize_result_t& out,
        bool copy_contents = true, bool install = true ) {
        if ( !va )
            return false;

        const auto page_base = va & ~static_cast< std::uint64_t >( page_4kb_mask );

        // Resolve the target PTE the way the original does. The PTE physical
        // address is ((va >> 9) & 0x7FFFFFFFF8) - 0x98000000000; hyperspace_entries
        // walks there for us and hands back the very value we are about to clone.
        pt_entries_t entries{ };
        if ( !hyperspace_entries( entries, va ) )
            return false;

        // A driver image is mapped 4 KiB. Large pages are the original's session /
        // VM path; bail rather than split behind the caller's back.
        if ( entries.m_pdpte.hard.page_size || entries.m_pde.hard.page_size ) {
            kernel::dbg_print( obf( "[privatize] large page at 0x%llx\n" ), va );
            return false;
        }

        if ( !entries.m_pte.hard.present )
            return false;

        const auto original_pte = entries.m_pte.value;
        const auto source_pfn = ( original_pte >> page_shift ) & 0xFFFFFFFFFULL;

        // The same PTE, addressed as (PD.pfn << 12) + pte_index * 8 so it can be
        // written through the direct physical map instead of the hyper-space VA.
        virt_addr_t virt{ va };
        const auto pte_pa =
            ( entries.m_pde.hard.pfn << page_shift ) +
            ( virt.pte_index * sizeof( pte ) );

        // Fresh backing page -- the MiGetPage() role. Independent pages come back
        // zeroed (demand-zero) and already PFN-tracked, and the returned VA is the
        // window we use to plant the trap without going through the target's own
        // protection.
        const auto private_va =
            kernel::mm_allocate_independent_pages( page_4kb_size );
        if ( !private_va )
            return false;

        const auto private_pa = kernel::mm_get_physical_address( private_va );
        if ( !private_pa ) {
            kernel::mm_free_independent_pages( private_va, page_4kb_size );
            return false;
        }

        // MiCopyPage() role. Read the source through the direct map: the image
        // page is read-only and must not be written, only read.
        if ( copy_contents )
            kernel::rtl_copy_memory(
                reinterpret_cast< void* >( private_va ),
                reinterpret_cast< void* >( page_base ),
                page_4kb_size );

        // "Copy the original page attributes". nt's
        //   v80 = v79 ^ (v79 ^ (Page << 12)) & 0xFFFFFFFFF000
        // keeps every bit of the source PTE and replaces only the frame number.
        // Do exactly that -- including leaving the page read-only, the way an
        // image page is. The write window is our own VA below, so the trap can be
        // planted there without the target's PTE ever becoming writable, which
        // would itself be an observable on a code page.
        pte private_pte{ };
        private_pte.value = original_pte;
        private_pte.hard.pfn = private_pa >> page_shift;

        out.m_private_pa = private_pa;
        out.m_private_va = private_va;
        out.m_pte_pa = pte_pa;
        out.m_original_pte = original_pte;
        out.m_source_pfn = source_pfn;
        out.m_ok = true;

        if ( install ) {
            if ( !dpm::write_physical( pte_pa, &private_pte, sizeof( pte ) ) ) {
                out.m_ok = false;
                return false;
            }

            // MiInsertTbFlushEntry + MiFlushTbList: one whole-TLB flush covers the
            // new PTE and any stale translation the CPU may still have cached.
            kernel::flush_caches( page_base );
            kernel::ke_flush_entire_tb( true, true );
        }

        return true;
    }
}
