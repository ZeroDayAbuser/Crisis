#pragma once

namespace paging {
    namespace shadow {
        struct mapping_t {
            std::uint64_t m_pml4_pa{ };
            std::uint64_t m_pdpt_pa{ };
            std::uint64_t m_pd_pa{ };
            std::uint64_t m_pt_pa{ };
            std::uint64_t m_pte_pa{ };
            std::uint64_t m_shadow_va{ };
            std::uint32_t m_pml4_index{ };
            std::uint64_t m_canonical_pte_pa{ };
            std::uint64_t m_canonical_pte_value{ };
            bool m_active{ };
        };

        std::uint32_t find_free_pml4_slot( std::uint64_t system_pml4_pa ) {
            for ( std::uint32_t idx = 257; idx < 511; idx++ ) {
                pml4e entry{ };
                if ( !dpm::read_physical(
                    system_pml4_pa + ( idx * sizeof( pml4e ) ),
                    &entry, sizeof( pml4e ) ) )
                    continue;

                if ( !entry.hard.present )
                    return idx;
            }

            kernel::dbg_print( obf( "no non-present pml4\n" ) );
            return static_cast< std::uint32_t >( -1 );
        }

        std::uint64_t alloc_pt_page( ) {
            const auto va =
                kernel::mm_allocate_independent_pages( paging::page_4kb_size );
            if ( !va )
                return 0;

            const auto pa = kernel::mm_get_physical_address( va );
            if ( !pa )
                return 0;

            return pa;
        }

        bool build(
            std::uint64_t  private_pa,
            std::uint64_t  hooked_va,
            std::uint64_t  canonical_pte_pa,
            std::uint64_t  canonical_pte_value,
            mapping_t& out ) {
            const auto system_pml4_pa = __readcr3( ) & ~0xFFFULL;

            const auto slot = find_free_pml4_slot( system_pml4_pa );
            if ( slot == static_cast< std::uint32_t >( -1 ) )
                return false;

            const auto shadow_pml4_va =
                kernel::mm_allocate_independent_pages( paging::page_4kb_size );
            if ( !shadow_pml4_va )
                return false;

            const auto shadow_pml4_pa =
                kernel::mm_get_physical_address( shadow_pml4_va );
            if ( !shadow_pml4_pa )
                return false;

            {
                std::uint8_t buf[ paging::page_4kb_size ];
                if ( !dpm::read_physical(
                    system_pml4_pa, buf, sizeof( buf ) ) )
                    return false;
                if ( !dpm::write_physical(
                    shadow_pml4_pa, buf, sizeof( buf ) ) )
                    return false;
            }

            const auto shadow_pdpt_pa = alloc_pt_page( );
            const auto shadow_pd_pa = alloc_pt_page( );
            const auto shadow_pt_pa = alloc_pt_page( );

            if ( !shadow_pdpt_pa || !shadow_pd_pa || !shadow_pt_pa )
                return false;

            const auto shadow_va_base =
                paging::create_virtual_address( slot, true );

            const auto shadow_va =
                ( shadow_va_base & ~paging::page_4kb_mask ) |
                ( hooked_va & paging::page_4kb_mask );

            virt_addr_t sv{ shadow_va };

            pml4e s_pml4e{ };
            s_pml4e.hard.present = 1;
            s_pml4e.hard.read_write = 1;
            s_pml4e.hard.pfn = shadow_pdpt_pa >> paging::page_shift;
            dpm::write_physical(
                shadow_pml4_pa + ( sv.pml4e_index * sizeof( pml4e ) ),
                &s_pml4e, sizeof( pml4e ) );

            pdpte s_pdpte{ };
            s_pdpte.hard.present = 1;
            s_pdpte.hard.read_write = 1;
            s_pdpte.hard.pfn = shadow_pd_pa >> paging::page_shift;
            dpm::write_physical(
                shadow_pdpt_pa + ( sv.pdpte_index * sizeof( pdpte ) ),
                &s_pdpte, sizeof( pdpte ) );

            pde s_pde{ };
            s_pde.hard.present = 1;
            s_pde.hard.read_write = 1;
            s_pde.hard.pfn = shadow_pt_pa >> paging::page_shift;
            dpm::write_physical(
                shadow_pd_pa + ( sv.pde_index * sizeof( pde ) ),
                &s_pde, sizeof( pde ) );

            pte s_pte{ };
            s_pte.hard.present = 1;
            s_pte.hard.read_write = 0;
            s_pte.hard.no_execute = 0;
            s_pte.hard.global = 0;
            s_pte.hard.pfn = private_pa >> paging::page_shift;

            const auto shadow_pte_pa =
                shadow_pt_pa + ( sv.pte_index * sizeof( pte ) );
            dpm::write_physical( shadow_pte_pa, &s_pte, sizeof( pte ) );

            pte canonical_current{ };
            if ( !dpm::read_physical(
                canonical_pte_pa, &canonical_current, sizeof( pte ) ) )
                return false;




            kernel::flush_caches(
                kernel::mm_get_virtual_for_physical( private_pa ) );

            out.m_pml4_pa = shadow_pml4_pa;
            out.m_pdpt_pa = shadow_pdpt_pa;
            out.m_pd_pa = shadow_pd_pa;
            out.m_pt_pa = shadow_pt_pa;
            out.m_pte_pa = shadow_pte_pa;
            out.m_shadow_va = shadow_va;
            out.m_pml4_index = slot;
            out.m_canonical_pte_pa = canonical_pte_pa;
            out.m_canonical_pte_value = canonical_pte_value;
            out.m_active = true;
            return true;
        }

        bool teardown( mapping_t& m ) {
            if ( !m.m_active )
                return false;

            if ( m.m_canonical_pte_pa && m.m_canonical_pte_value ) {
                pte orig{ m.m_canonical_pte_value };
                dpm::write_physical(
                    m.m_canonical_pte_pa, &orig, sizeof( pte ) );
            }

            if ( m.m_pml4_pa ) {
                virt_addr_t sv{ m.m_shadow_va };
                pml4e zero{ };
                dpm::write_physical(
                    m.m_pml4_pa + ( sv.pml4e_index * sizeof( pml4e ) ),
                    &zero, sizeof( pml4e ) );
            }

            kernel::ke_flush_entire_tb( true, true );
            m.m_active = false;
            return true;
        }

    }
}
