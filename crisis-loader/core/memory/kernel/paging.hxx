#pragma once

namespace bypass { namespace target { void flush_pt_cache( ) { } } }

namespace memory {
    bool read_physical( std::uint64_t addr, void* buffer, std::size_t size );
    bool write_physical( std::uint64_t addr, void* buffer, std::size_t size );
    bool read_virtual( std::uint64_t addr, void* buffer, std::size_t size );
}

namespace kernel {
    void memcpy( void* dst, void* src, std::uint64_t size );
    void memset( void* dst, int value, std::uint64_t size );
    void* mm_allocate_independent_pages( std::size_t size );
    bool mm_free_independent_pages( std::uint64_t address, std::uint32_t size );
    std::uint64_t mm_get_physical_address( std::uint64_t virtual_address );
    void invalidate_tlb_entry( std::uint64_t virtual_address );
}

namespace paging {
    constexpr auto page_4kb_size = 0x1000ull;
    constexpr auto page_2mb_size = 0x200000ull;
    constexpr auto page_1gb_size = 0x40000000ull;

    constexpr auto page_shift = 12ull;
    constexpr auto page_2mb_shift = 21ull;
    constexpr auto page_1gb_shift = 30ull;

    constexpr auto page_4kb_mask = 0xFFFull;
    constexpr auto page_2mb_mask = 0x1FFFFFull;
    constexpr auto page_1gb_mask = 0x3FFFFFFFull;

    struct pt_entries_t {
        std::optional<pml4e> m_pml4; std::uint64_t m_pml4_pa{};
        std::optional<pdpte> m_pdpt; std::uint64_t m_pdpt_pa{};
        std::optional<pde>   m_pd; std::uint64_t m_pd_pa{};
        std::optional<pte>   m_pt; std::uint64_t m_pt_pa{};
        std::uint64_t m_phys_addr{};
        std::uint64_t m_page_size{};
    };

    enum class e_pt_protection {
        none,
        read_write,
        read_write_execute
    };

    class c_paging {
    public:
        std::uint64_t m_directory_table_base{};
        std::uint64_t m_pte_base{};
        std::uint32_t m_self_ref_index{};

        c_paging( ) { }
        ~c_paging( ) { }

        bool setup( ) {
            auto page_buffer = std::make_unique<uint8_t[ ]>( 0x1000 );
            for ( uint64_t page = 0; page < 0x20000000; page += 0x1000 ) {
                if ( !memory::read_physical( page, page_buffer.get( ), 0x1000 ) )
                    continue;

                if ( page_buffer[ 0 ] == 0xE9 && page_buffer[ 1 ] == 0x4D &&
                    page_buffer[ 2 ] == 0x06 && page_buffer[ 3 ] == 0x00 ) {
                    auto dtb = *reinterpret_cast< uint64_t* >( page_buffer.get( ) + 0xA0 );
                    if ( dtb && ( dtb & 0xFFF ) == 0 ) {
                        logging::print( obf( "found directory table base: 0x%llx" ), dtb );
                        this->m_directory_table_base = dtb;
                        return true;
                    }
                }
            }

            return false;
        }

        template<typename type>
        std::optional<type> read_pt_entry( std::uint64_t phys_addr ) {
            type entry{};
            if ( !memory::read_physical( phys_addr, &entry, sizeof( type ) ) )
                return std::nullopt;
            return entry;
        }

        bool write_pt_entry( std::uint64_t phys_addr, std::uint64_t value ) {
            return memory::write_physical( phys_addr, &value, sizeof( value ) );
        }

        std::optional<std::uint64_t> attach_process( std::uint64_t new_dtb ) {
            const auto original_dtb = this->m_directory_table_base;
            this->m_directory_table_base = new_dtb;
            return std::optional<std::uint64_t>( original_dtb );
        }

        template<typename addr_t, typename callback_t>
        bool scan_va(
            addr_t va, std::size_t size,
            callback_t callback ) {
            std::uint64_t va64;
            const auto aligned_size = ( size + page_4kb_mask ) & ~page_4kb_mask;
            const auto page_count = aligned_size >> page_shift;
            for ( auto idx = 0; idx < page_count; idx++ ) {
                const auto current_va = va + ( idx << page_shift );
                auto ctx = this->hyperspace_entries( current_va );
                if ( !ctx ) continue;

                if ( !callback( current_va, *ctx ) )
                    return false;
            }

            return true;
        }

        std::uint64_t translate_linear(
            std::uint64_t virt_addr,
            std::uint32_t* page_size = nullptr ) {
            auto ctx = hyperspace_entries( virt_addr );
            if ( !ctx )
                return 0;

            if ( page_size )
                *page_size = ctx->m_page_size;

            return ctx->m_phys_addr;
        }

        std::uint64_t get_pte_address(
            std::uint64_t virt_addr,
            std::uint32_t* page_size = nullptr ) {
            auto ctx = hyperspace_entries( virt_addr );
            if ( !ctx )
                return 0;

            if ( page_size )
                *page_size = ctx->m_page_size;

            return ctx->m_pt_pa;
        }

        std::optional<pt_entries_t> hyperspace_entries(
            std::uint64_t virt_addr,
            std::function<bool( pt_entries_t& )> on_pml4 = nullptr,
            std::function<bool( pt_entries_t& )> on_pdpt = nullptr,
            std::function<bool( pt_entries_t& )> on_pd = nullptr,
            std::function<bool( pt_entries_t& )> on_pt = nullptr ) {

            virt_addr_t va{ virt_addr };
            pt_entries_t ctx{};

            ctx.m_pml4_pa = m_directory_table_base + ( va.pml4e_index * sizeof( pml4e ) );
            auto pml4 = read_pt_entry<pml4e>( ctx.m_pml4_pa );
            if ( !pml4 || !pml4->hard.present ) return std::nullopt;
            ctx.m_pml4 = pml4;
            if ( on_pml4 && !on_pml4( ctx ) ) return std::nullopt;

            ctx.m_pdpt_pa = ( ctx.m_pml4->hard.pfn << 12 ) + ( va.pdpte_index * sizeof( pdpte ) );
            auto pdpt = read_pt_entry<pdpte>( ctx.m_pdpt_pa );
            if ( !pdpt || !pdpt->hard.present ) return std::nullopt;
            ctx.m_pdpt = pdpt;

            if ( on_pdpt && !on_pdpt( ctx ) ) return std::nullopt;
            if ( ctx.m_pdpt->hard.page_size ) {
                ctx.m_page_size = page_1gb_size;
                ctx.m_phys_addr = ( ctx.m_pdpt->hard.pfn << 12 ) + ( virt_addr & page_1gb_mask );
                return ctx;
            }

            ctx.m_pd_pa = ( ctx.m_pdpt->hard.pfn << 12 ) + ( va.pde_index * sizeof( pde ) );
            auto pd = read_pt_entry<pde>( ctx.m_pd_pa );
            if ( !pd || !pd->hard.present ) return std::nullopt;
            ctx.m_pd = pd;

            if ( on_pd && !on_pd( ctx ) ) return std::nullopt;
            if ( ctx.m_pd->hard.page_size ) {
                ctx.m_page_size = page_2mb_size;
                ctx.m_phys_addr = ( ctx.m_pd->hard.pfn << 12 ) + ( virt_addr & page_2mb_mask );
                return ctx;
            }

            ctx.m_pt_pa = ( ctx.m_pd->hard.pfn << page_shift ) + ( va.pte_index * sizeof( pte ) );
            auto pt = read_pt_entry<pte>( ctx.m_pt_pa );
            if ( !pt || !pt->hard.present ) return std::nullopt;
            ctx.m_pt = pt;

            if ( on_pt && !on_pt( ctx ) ) return std::nullopt;
            ctx.m_page_size = page_4kb_size;
            ctx.m_phys_addr = ( ctx.m_pt->hard.pfn << 12 ) + ( virt_addr & page_4kb_mask );
            return ctx;
        }

        std::uint32_t get_page_size( std::uint64_t addr ) {
            auto ctx = hyperspace_entries( addr );
            if ( !ctx )
                return 0;

            if ( ctx->m_pdpt->hard.page_size )
                return page_1gb_size;
            if ( ctx->m_pd->hard.page_size )
                return page_2mb_size;
            return page_4kb_size;
        }

        e_pt_protection get_page_protect( std::uint64_t addr ) {
            auto ctx = hyperspace_entries( addr );
            if ( !ctx )
                return e_pt_protection::none;

            if ( ctx->m_pt->hard.read_write )
                return e_pt_protection::read_write;
            if ( !ctx->m_pt->hard.no_execute )
                return e_pt_protection::read_write_execute;
            return e_pt_protection::none;
        }

        bool set_page_protect( std::uint64_t va, paging::e_pt_protection protect ) {
            const auto ctx = hyperspace_entries( va );
            if ( !ctx || !ctx->m_pt )
                return false;

            auto new_pte = *ctx->m_pt;
            switch ( protect ) {
                case paging::e_pt_protection::read_write_execute:
                    new_pte.hard.read_write = 1;
                    new_pte.hard.no_execute = 0;
                    break;
                case paging::e_pt_protection::read_write:
                    new_pte.hard.read_write = 1;
                    new_pte.hard.no_execute = 1;
                    break;
                case paging::e_pt_protection::none:
                    new_pte.hard.read_write = 0;
                    new_pte.hard.no_execute = 1;
                    break;
                default:
                    return false;
            }

            if ( !write_pt_entry( ctx->m_pt_pa, new_pte.value ) )
                return false;

            kernel::invalidate_tlb_entry( va );
            bypass::target::flush_pt_cache( );
            return true;
        }

        bool find_pte_base( ) {
            if ( m_pte_base )
                return true;

            const auto dtb_pfn = m_directory_table_base >> page_shift;
            for ( auto idx = 256u; idx < 512u; ++idx ) {
                const auto pa = m_directory_table_base + ( idx * sizeof( pml4e ) );
                auto entry = read_pt_entry<pml4e>( pa );
                if ( !entry || !entry->hard.present )
                    continue;
                if ( entry->hard.pfn == dtb_pfn ) {
                    m_self_ref_index = idx;
                    m_pte_base = static_cast<std::uint64_t>( idx ) << 39;
                    if ( m_pte_base & ( 1ULL << 47 ) )
                        m_pte_base |= 0xFFFF000000000000ULL;
                    return true;
                }
            }
            return false;
        }

        std::uint64_t get_pte_va( std::uint64_t va ) {
            if ( !find_pte_base( ) )
                return 0;
            return m_pte_base + ( ( va >> 9 ) & 0x7FFFFFFFF8ULL );
        }

        bool split_2mb_to_4kb( std::uint64_t address,
            std::optional<e_pt_protection> protect = std::nullopt ) {
            const auto ctx = hyperspace_entries( address );
            if ( !ctx )
                return false;

            if ( !ctx->m_pd || !ctx->m_pd->hard.present || !ctx->m_pd->hard.page_size ) {
                logging::print( obf( "address 0x%llx is not a 2mb page" ), address );
                return false;
            }

            const auto new_pt_va = reinterpret_cast< std::uint64_t >(
                kernel::mm_allocate_independent_pages( page_4kb_size ) );
            if ( !new_pt_va ) {
                logging::print( obf( "failed to allocate pt" ) );
                return false;
            }
            kernel::memset( reinterpret_cast< void* >( new_pt_va ), 0, page_4kb_size );

            const auto new_pt_pa = kernel::mm_get_physical_address( new_pt_va );
            if ( !new_pt_pa ) {
                logging::print( obf( "failed to get physical address" ) );
                kernel::mm_free_independent_pages( new_pt_va, page_4kb_size );
                return false;
            }

            // resolve PTE protection bits
            std::uint64_t pte_rw = ctx->m_pd->hard.read_write;
            std::uint64_t pte_nx = ctx->m_pd->hard.no_execute;
            if ( protect ) {
                switch ( *protect ) {
                    case e_pt_protection::read_write_execute:
                        pte_rw = 1; pte_nx = 0; break;
                    case e_pt_protection::read_write:
                        pte_rw = 1; pte_nx = 1; break;
                    case e_pt_protection::none:
                        pte_rw = 0; pte_nx = 1; break;
                }
            }

            const auto base_pfn = ctx->m_pd->hard.pfn;
            for ( auto idx = 0u; idx < 512u; ++idx ) {
                pte entry{};
                entry.hard.present = 1;
                entry.hard.read_write = pte_rw;
                entry.hard.user_supervisor = ctx->m_pd->hard.user_supervisor;
                entry.hard.page_write_through = ctx->m_pd->hard.page_write_through;
                entry.hard.cached_disable = ctx->m_pd->hard.cached_disable;
                entry.hard.global = ctx->m_pd->hard.global;
                entry.hard.no_execute = pte_nx;
                entry.hard.pfn = base_pfn + idx;

                const auto pte_pa = new_pt_pa + ( idx * sizeof( pte ) );
                if ( !memory::write_physical( pte_pa, &entry, sizeof( pte ) ) ) {
                    logging::print( obf( "failed to write pte (pt: %u)" ), idx );
                    kernel::mm_free_independent_pages( new_pt_va, page_4kb_size );
                    return false;
                }
            }

            // PDE is permissive — per-PTE bits control actual protection
            pde new_pde{};
            new_pde.hard.present = 1;
            new_pde.hard.read_write = 1;
            new_pde.hard.user_supervisor = ctx->m_pd->hard.user_supervisor;
            new_pde.hard.page_write_through = ctx->m_pd->hard.page_write_through;
            new_pde.hard.cached_disable = ctx->m_pd->hard.cached_disable;
            new_pde.hard.page_size = 0;
            new_pde.hard.no_execute = 0;
            new_pde.hard.pfn = new_pt_pa >> page_shift;

            if ( !write_pt_entry( ctx->m_pd_pa, new_pde.value ) ) {
                logging::print( obf( "failed to write new pde" ) );
                kernel::mm_free_independent_pages( new_pt_va, page_4kb_size );
                return false;
            }

            kernel::invalidate_tlb_entry( address );
            bypass::target::flush_pt_cache( );
            return true;
        }

        bool split_1gb_to_4kb( std::uint64_t address,
            std::optional<e_pt_protection> protect = std::nullopt ) {
            const auto ctx = hyperspace_entries( address );
            if ( !ctx )
                return false;

            if ( !ctx->m_pdpt || !ctx->m_pdpt->hard.present || !ctx->m_pdpt->hard.page_size ) {
                logging::print( obf( "address 0x%llx is not a 1gb page" ), address );
                return false;
            }

            const auto new_pd_va = reinterpret_cast< std::uint64_t >(
                kernel::mm_allocate_independent_pages( page_4kb_size ) );
            if ( !new_pd_va ) {
                logging::print( obf( "failed to allocate pd" ) );
                return false;
            }
            kernel::memset( reinterpret_cast< void* >( new_pd_va ), 0, page_4kb_size );

            const auto new_pd_pa = kernel::mm_get_physical_address( new_pd_va );
            if ( !new_pd_pa ) {
                logging::print( obf( "failed to get physical address" ) );
                kernel::mm_free_independent_pages( new_pd_va, page_4kb_size );
                return false;
            }

            // resolve PTE protection bits
            std::uint64_t pte_rw = ctx->m_pdpt->hard.read_write;
            std::uint64_t pte_nx = ctx->m_pdpt->hard.no_execute;
            if ( protect ) {
                switch ( *protect ) {
                    case e_pt_protection::read_write_execute:
                        pte_rw = 1; pte_nx = 0; break;
                    case e_pt_protection::read_write:
                        pte_rw = 1; pte_nx = 1; break;
                    case e_pt_protection::none:
                        pte_rw = 0; pte_nx = 1; break;
                }
            }

            const auto base_pfn = ctx->m_pdpt->hard.pfn;
            for ( auto pd_idx = 0u; pd_idx < 512u; ++pd_idx ) {
                const auto new_pt_va = reinterpret_cast< std::uint64_t >(
                    kernel::mm_allocate_independent_pages( page_4kb_size ) );
                if ( !new_pt_va ) {
                    logging::print( obf( "failed to allocate pt (pd: %u)" ), pd_idx );
                    kernel::mm_free_independent_pages( new_pd_va, page_4kb_size );
                    return false;
                }
                kernel::memset( reinterpret_cast< void* >( new_pt_va ), 0, page_4kb_size );

                const auto new_pt_pa = kernel::mm_get_physical_address( new_pt_va );
                if ( !new_pt_pa ) {
                    logging::print( obf( "failed to get physical address (pd: %u)" ), pd_idx );
                    kernel::mm_free_independent_pages( new_pt_va, page_4kb_size );
                    kernel::mm_free_independent_pages( new_pd_va, page_4kb_size );
                    return false;
                }

                const auto pd_base_pfn = base_pfn + ( pd_idx * 512u );
                for ( auto pt_idx = 0u; pt_idx < 512u; ++pt_idx ) {
                    pte entry{};
                    entry.hard.present = 1;
                    entry.hard.read_write = pte_rw;
                    entry.hard.user_supervisor = ctx->m_pdpt->hard.user_supervisor;
                    entry.hard.page_write_through = ctx->m_pdpt->hard.page_write_through;
                    entry.hard.cached_disable = ctx->m_pdpt->hard.cached_disable;
                    entry.hard.no_execute = pte_nx;
                    entry.hard.pfn = pd_base_pfn + pt_idx;

                    const auto pte_pa = new_pt_pa + ( pt_idx * sizeof( pte ) );
                    if ( !memory::write_physical( pte_pa, &entry, sizeof( pte ) ) ) {
                        logging::print( obf( "failed to write pte (pd: %u) (pt: %u)" ), pd_idx, pt_idx );
                        kernel::mm_free_independent_pages( new_pt_va, page_4kb_size );
                        kernel::mm_free_independent_pages( new_pd_va, page_4kb_size );
                        return false;
                    }
                }

                // PDE permissive — per-PTE bits control actual protection
                pde pd_entry{};
                pd_entry.hard.present = 1;
                pd_entry.hard.read_write = 1;
                pd_entry.hard.user_supervisor = ctx->m_pdpt->hard.user_supervisor;
                pd_entry.hard.page_write_through = ctx->m_pdpt->hard.page_write_through;
                pd_entry.hard.cached_disable = ctx->m_pdpt->hard.cached_disable;
                pd_entry.hard.page_size = 0;
                pd_entry.hard.no_execute = 0;
                pd_entry.hard.pfn = new_pt_pa >> page_shift;

                const auto pde_pa = new_pd_pa + ( pd_idx * sizeof( pde ) );
                if ( !memory::write_physical( pde_pa, &pd_entry, sizeof( pde ) ) ) {
                    logging::print( obf( "failed to write pde (idx: %u)" ), pd_idx );
                    kernel::mm_free_independent_pages( new_pt_va, page_4kb_size );
                    kernel::mm_free_independent_pages( new_pd_va, page_4kb_size );
                    return false;
                }
            }

            // PDPTE permissive — PDE/PTE bits control actual protection
            pdpte new_pdpte{};
            new_pdpte.hard.present = 1;
            new_pdpte.hard.read_write = 1;
            new_pdpte.hard.user_supervisor = ctx->m_pdpt->hard.user_supervisor;
            new_pdpte.hard.page_write_through = ctx->m_pdpt->hard.page_write_through;
            new_pdpte.hard.cached_disable = ctx->m_pdpt->hard.cached_disable;
            new_pdpte.hard.page_size = 0;
            new_pdpte.hard.no_execute = 0;
            new_pdpte.hard.pfn = new_pd_pa >> page_shift;

            if ( !write_pt_entry( ctx->m_pdpt_pa, new_pdpte.value ) ) {
                logging::print( obf( "failed to write new pdpte" ) );
                kernel::mm_free_independent_pages( new_pd_va, page_4kb_size );
                return false;
            }

            kernel::invalidate_tlb_entry( address );
            bypass::target::flush_pt_cache( );
            return true;
        }

        std::uint64_t find_accessible_page( std::uint64_t process_dtb ) {
            const auto original_dtb = attach_process( process_dtb );
            if ( !original_dtb )
                return 0;

            std::uint64_t found_va = 0;

            for ( auto pml4_idx = 1u; pml4_idx < 256u && !found_va; ++pml4_idx ) {
                const auto pml4_pa = process_dtb + ( pml4_idx * sizeof( pml4e ) );
                const auto pml4_entry = read_pt_entry<pml4e>( pml4_pa );
                if ( !pml4_entry || !pml4_entry->hard.present ) continue;

                for ( auto pdpt_idx = 0u; pdpt_idx < 512u && !found_va; ++pdpt_idx ) {
                    const auto pdpt_pa = ( pml4_entry->hard.pfn << page_shift ) + ( pdpt_idx * sizeof( pdpte ) );
                    const auto pdpt_entry = read_pt_entry<pdpte>( pdpt_pa );
                    if ( !pdpt_entry || !pdpt_entry->hard.present ) continue;

                    if ( pdpt_entry->hard.page_size ) {
                        found_va = create_virtual_address( pml4_idx, pdpt_idx );
                        break;
                    }

                    for ( auto pd_idx = 0u; pd_idx < 512u && !found_va; ++pd_idx ) {
                        const auto pd_pa = ( pdpt_entry->hard.pfn << page_shift ) + ( pd_idx * sizeof( pde ) );
                        const auto pd_entry = read_pt_entry<pde>( pd_pa );
                        if ( !pd_entry || !pd_entry->hard.present ) continue;

                        if ( pd_entry->hard.page_size ) {
                            found_va = ( static_cast< std::uint64_t >( pml4_idx ) << 39 ) |
                                ( static_cast< std::uint64_t >( pdpt_idx ) << 30 ) |
                                ( static_cast< std::uint64_t >( pd_idx ) << 21 );
                            break;
                        }

                        for ( auto pt_idx = 0u; pt_idx < 512u; ++pt_idx ) {
                            const auto pt_pa = ( pd_entry->hard.pfn << page_shift ) + ( pt_idx * sizeof( pte ) );
                            const auto pt_entry = read_pt_entry<pte>( pt_pa );
                            if ( !pt_entry || !pt_entry->hard.present ) continue;
                            if ( !pt_entry->hard.user_supervisor ) continue;

                            found_va = ( static_cast< std::uint64_t >( pml4_idx ) << 39 ) |
                                ( static_cast< std::uint64_t >( pdpt_idx ) << 30 ) |
                                ( static_cast< std::uint64_t >( pd_idx ) << 21 ) |
                                ( static_cast< std::uint64_t >( pt_idx ) << page_shift );
                            break;
                        }
                    }
                }
            }

            attach_process( *original_dtb );
            return found_va;
        }

        bool copy_pfn_attributes( std::uint64_t source, std::uint64_t destination, std::uint64_t dtb ) {
            const auto mm_pfn_database_ptr = g_pdb->get_symbol_address( obf( "MmPfnDatabase" ) );
            if ( !mm_pfn_database_ptr ) return false;

            std::uint64_t pfn_database;
            memory::read_virtual( mm_pfn_database_ptr, &pfn_database, sizeof( pfn_database ) );
            if ( !pfn_database )
                return false;

            const auto original_dtb = attach_process( dtb );
            const auto src_pa = translate_linear( source );
            const auto dst_pa = translate_linear( destination );
            attach_process( *original_dtb );
            if ( !src_pa || !dst_pa ) return false;

            const auto src_pfn = src_pa >> page_shift;
            const auto dst_pfn = dst_pa >> page_shift;

            constexpr std::uint64_t mmpfn_size = 0x30;
            const auto src_entry_pa = translate_linear( pfn_database + src_pfn * mmpfn_size );
            const auto dst_entry_pa = translate_linear( pfn_database + dst_pfn * mmpfn_size );
            if ( !src_entry_pa || !dst_entry_pa ) return false;

            std::uint8_t src_data[ mmpfn_size ]{};
            std::uint8_t dst_data[ mmpfn_size ]{};
            if ( !memory::read_physical( src_entry_pa, src_data, mmpfn_size ) ) return false;
            if ( !memory::read_physical( dst_entry_pa, dst_data, mmpfn_size ) ) return false;

            const auto off_u3 = g_pdb->get_struct_member( obf( "_MMPFN" ), obf( "u3" ) );
            const auto off_u4 = g_pdb->get_struct_member( obf( "_MMPFN" ), obf( "u4" ) );

            dst_data[ off_u3 + 0x0A ] = src_data[ off_u3 + 0x0A ];

            const auto src_u4 = *reinterpret_cast< std::uint64_t* >( src_data + off_u4 );
            const auto dst_u4 = *reinterpret_cast< std::uint64_t* >( dst_data + off_u4 );
            const auto new_u4 = ( src_u4 & 0x0FFFFFFFFFFFFFFFULL ) | ( dst_u4 & 0xF000000000000000ULL );
            std::memcpy( dst_data + off_u4, &new_u4, sizeof( new_u4 ) );

            if ( !memory::write_physical( dst_entry_pa, dst_data, mmpfn_size ) ) return false;

            return true;
        }

        std::uint64_t create_virtual_address( std::uint32_t pml4_index, std::uint32_t pdpt_index = 0, bool randomize_offset = false ) {
            std::uint64_t virtual_address =
                ( static_cast< std::uint64_t >( pml4_index ) << 39 ) |
                ( static_cast< std::uint64_t >( pdpt_index ) << 30 );

            if ( randomize_offset )
                virtual_address |= static_cast< std::uint64_t >( rand( ) % 512 ) << 21;

            return virtual_address;
        }

        std::uint32_t find_free_pml4_index( const std::uint64_t* dtbs, std::uint32_t dtb_count, bool high_address = false ) {
            const auto range_start = high_address ? 256u : 1u;
            const auto range_end = high_address ? 512u : 256u;
            for ( auto idx = range_start; idx < range_end; idx++ ) {
                bool conflict = false;
                for ( auto j = 0u; j < dtb_count; j++ ) {
                    if ( !dtbs[ j ] ) continue;
                    pml4e entry{};
                    if ( memory::read_physical( dtbs[ j ] + ( idx * sizeof( pml4e ) ),
                        &entry, sizeof( entry ) ) && entry.hard.present ) {
                        conflict = true;
                        break;
                    }
                }
                if ( !conflict )
                    return idx;
            }

            return static_cast< std::uint32_t >( -1 );
        }

        bool mirror_pml4_index( std::uint32_t pml4_index, std::uint64_t src_dtb,
            const std::uint64_t* dst_dtbs, std::uint32_t dst_count ) {
            pml4e entry{};
            auto src_addr = src_dtb + ( pml4_index * sizeof( pml4e ) );
            if ( !memory::read_physical( src_addr, &entry, sizeof( entry ) ) || !entry.hard.present )
                return false;

            for ( std::uint32_t i = 0; i < dst_count; i++ ) {
                if ( !dst_dtbs[ i ] ) continue;
                auto dst_addr = dst_dtbs[ i ] + ( pml4_index * sizeof( pml4e ) );
                memory::write_physical( dst_addr, &entry, sizeof( entry ) );
            }

            return true;
        }

        void zero_pml4_index( std::uint32_t pml4_index, const std::uint64_t* dtbs, std::uint32_t dtb_count ) {
            pml4e zero{};
            for ( std::uint32_t i = 0; i < dtb_count; i++ ) {
                if ( !dtbs[ i ] ) continue;
                auto addr = dtbs[ i ] + ( pml4_index * sizeof( pml4e ) );
                memory::write_physical( addr, &zero, sizeof( zero ) );
            }
        }
    };
}