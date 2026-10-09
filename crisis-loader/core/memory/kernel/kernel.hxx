#pragma once

namespace kernel {
    bool ex_acquire_resource_exclusive_lite( void* resource, bool wait = true ) {
        static auto addr = g_pdb->get_symbol_address( obf( "ExAcquireResourceExclusiveLite" ) );
        if ( !addr ) return false;
        return g_syscall->call_kernel< std::uint64_t >( addr,
            reinterpret_cast< std::uint64_t >( resource ),
            wait ? 1u : 0u ) != 0;
    }

    bool ex_release_resource_lite( void* resource ) {
        static auto addr = g_pdb->get_symbol_address( obf( "ExReleaseResourceLite" ) );
        if ( !addr || !resource ) return false;
        g_syscall->call_kernel( addr, reinterpret_cast< std::uint64_t >( resource ) );
        return true;
    }

    void* rtl_lookup_element_generic_table_avl( void* table, void* buffer ) {
        static auto addr = g_pdb->get_symbol_address( obf( "RtlLookupElementGenericTableAvl" ) );
        if ( !addr )
            return nullptr;
        auto result = g_syscall->call_kernel< std::uint64_t >( addr,
            reinterpret_cast< std::uint64_t >( table ),
            reinterpret_cast< std::uint64_t >( buffer ) );
        if ( !result )
            return nullptr;
        return reinterpret_cast< void* >( result );
    }

    void memcpy( void* dst, void* src, std::uint64_t size ) {
        static auto addr = g_pdb->get_symbol_address( obf( "memcpy" ) );
        if ( !addr )
            return;
        g_syscall->call_kernel( addr, dst, src, size );
    }

    bool rtl_delete_element_generic_table_avl( void* table, void* buffer ) {
        static auto addr = g_pdb->get_symbol_address( obf( "RtlDeleteElementGenericTableAvl" ) );
        if ( !addr )
            return false;
        auto result = g_syscall->call_kernel< std::uint64_t >( addr,
            reinterpret_cast< std::uint64_t >( table ),
            reinterpret_cast< std::uint64_t >( buffer ) );
        return result != 0;
    }

    std::uint64_t allocate_pool( std::uint64_t pool_type, std::uint64_t size ) {
        static auto addr = g_pdb->get_symbol_address( obf( "ExAllocatePoolWithTag" ) );
        if ( !addr )
            return 0;
        return g_syscall->call_kernel< std::uint64_t >( addr, pool_type, size, 'BwtE' );
    }

    bool free_pool( std::uint64_t address ) {
        if ( !address )
            return false;
        static auto addr = g_pdb->get_symbol_address( obf( "ExFreePool" ) );
        if ( !addr )
            return false;
        g_syscall->call_kernel( addr, address );
        return true;
    }

    void* mm_allocate_independent_pages( std::size_t size ) {
        static auto addr = g_pdb->get_symbol_address( obf( "MmAllocateIndependentPages" ) );
        if ( !addr )
            return nullptr;
        return g_syscall->call_kernel<void*>( addr, size, -1 );
    }

    std::uint64_t alloc_locked_in_process( std::uint64_t eprocess ) {
        static auto ob_open  = g_pdb->get_symbol_address( obf( "ObOpenObjectByPointer" ) );
        static auto zw_alloc = g_pdb->get_symbol_address( obf( "ZwAllocateVirtualMemory" ) );
        static auto zw_lock  = g_pdb->get_symbol_address( obf( "ZwLockVirtualMemory" ) );
        static auto zw_close = g_pdb->get_symbol_address( obf( "ZwClose" ) );
        if ( !ob_open || !zw_alloc || !zw_lock || !zw_close )
            return 0;

        std::uint64_t handle = 0;
        const auto open_st = g_syscall->call_kernel<std::int32_t>(
            ob_open,
            eprocess,
            static_cast<std::uint64_t>( 0x200 ),
            static_cast<std::uint64_t>( 0 ),
            static_cast<std::uint64_t>( 0x28 ),
            static_cast<std::uint64_t>( 0 ),
            static_cast<std::uint64_t>( 0 ),
            reinterpret_cast<std::uint64_t>( &handle ) );
        if ( open_st < 0 || !handle )
            return 0;

        std::uint64_t base   = 0;
        std::uint64_t region = paging::page_4kb_size;
        const auto alloc_st = g_syscall->call_kernel<std::int32_t>(
            zw_alloc,
            handle,
            reinterpret_cast<std::uint64_t>( &base ),
            static_cast<std::uint64_t>( 0 ),
            reinterpret_cast<std::uint64_t>( &region ),
            static_cast<std::uint64_t>( 0x3000 ),
            static_cast<std::uint64_t>( 0x04 ) );
        if ( alloc_st < 0 ) {
            g_syscall->call_kernel( zw_close, handle );
            return 0;
        }

        g_syscall->call_kernel(
            zw_lock,
            handle,
            reinterpret_cast<std::uint64_t>( &base ),
            reinterpret_cast<std::uint64_t>( &region ),
            static_cast<std::uint64_t>( 1 ) );

        g_syscall->call_kernel( zw_close, handle );
        return base;
    }

    void memset( void* dst, int value, std::uint64_t size ) {
        static auto addr = g_pdb->get_symbol_address( obf( "memset" ) );
        if ( !addr )
            return;
        g_syscall->call_kernel( addr, dst, value, size );
    }

    bool mm_free_independent_pages( std::uint64_t address, std::uint32_t size ) {
        static auto addr = g_pdb->get_symbol_address( obf( "MmFreeIndependentPages" ) );
        if ( !addr )
            return false;
        g_syscall->call_kernel( addr, address, size );
        return true;
    }

    bool mm_set_page_protection( std::uint64_t address, std::uint32_t size, std::uint32_t new_protect ) {
        static auto addr = g_pdb->get_symbol_address( obf( "MmSetPageProtection" ) );
        if ( !addr )
            return false;
        auto result = g_syscall->call_kernel< std::uint64_t >( addr, address, size, new_protect );
        return result != 0;
    }

    std::uint64_t mm_allocate_contiguous_memory( std::uint64_t size ) {
        static auto addr = g_pdb->get_symbol_address( obf( "MmAllocateContiguousMemory" ) );
        if ( !addr )
            return 0;
        return g_syscall->call_kernel< std::uint64_t >( addr, size, 0xFFFFFFFFFFFFFFFFull );
    }

    bool mm_free_contiguous_memory( std::uint64_t address ) {
        static auto addr = g_pdb->get_symbol_address( obf( "MmFreeContiguousMemory" ) );
        if ( !addr )
            return false;
        g_syscall->call_kernel( addr, address );
        return true;
    }

    std::uint64_t mm_get_physical_address( std::uint64_t virtual_address ) {
        static auto addr = g_pdb->get_symbol_address( obf( "MmGetPhysicalAddress" ) );
        if ( !addr )
            return 0;
        return g_syscall->call_kernel< std::uint64_t >( addr, virtual_address );
    }

    std::uint64_t mi_reserve_system_pte( std::uint32_t count ) {
        static auto addr = g_pdb->get_symbol_address( obf( "MiReserveSystemPtes" ) );
        if ( !addr )
            return 0;
        return g_syscall->call_kernel< std::uint64_t >( addr, count );
    }

    void mi_release_system_pte( std::uint64_t address ) {
        static auto addr = g_pdb->get_symbol_address( obf( "MiReleaseSystemPtes" ) );
        if ( !addr )
            return;
        g_syscall->call_kernel( addr, address, 1 );
    }

    // MiInitializePfn( PMMPFN pfn_entry, PMMPTE pte_ptr, UINT protection, CHAR flags )
    // pfn_entry = MmPfnDatabase + pfn * sizeof(_MMPFN)
    // protection = MM protection index (0 = MM_ZERO_ACCESS, 4 = MM_READWRITE, etc.)
    // flags: bit0 = skip per-page lock, bit4 = use interlocked
    bool mi_initialize_pfn( std::uint64_t pfn_entry, std::uint64_t pte_va,
                            std::uint64_t protection = 0, std::uint64_t flags = 0 ) {
        static auto addr = g_pdb->get_symbol_address( obf( "MiInitializePfn" ) );
        if ( !addr )
            return false;
        g_syscall->call_kernel( addr, pfn_entry, pte_va, protection, flags );
        return true;
    }

    void invalidate_tlb_entry( std::uint64_t virtual_address ) {
        static auto addr = g_pdb->get_symbol_address( obf( "KeFlushSingleTb" ) );
        if ( !addr )
            return;
        g_syscall->call_kernel( addr, virtual_address, 1 );
    }

    std::uint64_t get_mm_pfn_database( ) {
        static std::uint64_t cached = 0;
        if ( cached )
            return cached;

        auto mm_pfn_database_ptr = g_pdb->get_symbol_address( obf( "MmPfnDatabase" ) );
        if ( !mm_pfn_database_ptr )
            return 0;

        std::uint64_t pfn_database = 0;
        memory::read_virtual( mm_pfn_database_ptr, &pfn_database, sizeof( pfn_database ) );

        cached = pfn_database;
        return cached;
    }

    std::uint64_t get_mmpfn_size( ) {
        static const auto size = [ ]( ) -> std::uint64_t {
            const auto off = g_pdb->get_struct_member( obf( "_MMPFN" ), obf( "u4" ) );
            return off ? off + sizeof( std::uint64_t ) : 0x30;
        }( );
        return size;
    }

    bool mi_decrement_share_count( std::uint64_t pfn ) {
        static auto addr = g_pdb->get_symbol_address( obf( "MiLockAndDecrementShareCount" ) );
        if ( !addr )
            return false;

        const auto pfn_database = get_mm_pfn_database( );
        if ( !pfn_database )
            return false;

        const auto mmpfn_size = get_mmpfn_size( );
        if ( !mmpfn_size )
            return false;

        g_syscall->call_kernel( addr, pfn_database + pfn * mmpfn_size, static_cast<std::uint64_t>( 0 ) );
        return true;
    }

    std::uint32_t mi_get_system_region_type( std::uint64_t va ) {
        static auto addr = g_pdb->get_symbol_address( obf( "MiGetSystemRegionType" ) );
        if ( !addr )
            return 0xFFFFFFFF;
        return g_syscall->call_kernel<std::uint32_t>( addr, va );
    }

    bool get_pfn_share_count( std::uint64_t pfn, std::uint64_t& out_count ) {
        const auto pfn_database = get_mm_pfn_database( );
        if ( !pfn_database )
            return false;

        const auto mmpfn_size = get_mmpfn_size( );
        if ( !mmpfn_size )
            return false;

        const auto entry_va = pfn_database + pfn * mmpfn_size;
        const auto entry_pa = g_paging->translate_linear( entry_va );
        if ( !entry_pa )
            return false;

        std::uint64_t pfn24 = 0;
        if ( !memory::read_physical( entry_pa + 24, &pfn24, sizeof( pfn24 ) ) )
            return false;

        out_count = pfn24 & 0x3FFFFFFFFFFFFFFFULL;
        return true;
    }

    std::int32_t mi_copy_on_write( std::uint64_t faulting_address, std::uint64_t pte_va ) {
        static auto addr = g_pdb->get_symbol_address( obf( "MiCopyOnWrite" ) );
        if ( !addr )
            return -1;
        return g_syscall->call_kernel<std::int32_t>( addr,
            faulting_address, pte_va,
            static_cast<std::uint64_t>( -1 ), static_cast<std::uint64_t>( 0 ) );
    }

    void flush_tlb_all( ) {
        static auto addr = g_pdb->get_symbol_address( obf( "KeFlushEntireTb" ) );
        if ( !addr ) {
            auto cr4 = __readcr4( );
            __writecr4( cr4 );
            __writecr3( __readcr3( ) );
            return;
        }
        g_syscall->call_kernel( addr, 0, 0 );
    }

    std::uint64_t ex_allocate_pool( std::uint64_t pool_type, std::uint64_t size, std::uint32_t tag = 'PgeM' ) {
        static auto addr = g_pdb->get_symbol_address( obf( "ExAllocatePoolWithTag" ) );
        if ( !addr )
            return 0;
        return g_syscall->call_kernel< std::uint64_t >( addr, pool_type, size, tag );
    }

    void ex_free_pool( std::uint64_t address ) {
        if ( !address )
            return;
        static auto addr = g_pdb->get_symbol_address( obf( "ExFreePool" ) );
        if ( !addr )
            return;
        g_syscall->call_kernel( addr, address );
    }

    void* ps_lookup_process_by_pid( std::uint32_t process_id ) {
        static auto addr = g_pdb->get_symbol_address( obf( "PsLookupProcessByProcessId" ) );
        if ( !addr )
            return nullptr;

        void* process = nullptr;
        auto result = g_syscall->call_kernel<NTSTATUS>(
            addr,
            reinterpret_cast< HANDLE >( process_id ),
            &process
        );

        if ( result ) {
            logging::print( obf( "failed to find process (%d)" ), result );
            return nullptr;
        }

        return process;
    }

    bool mark_pages_bad( std::uint64_t pa, std::uint64_t size ) {
        static auto addr = g_pdb->get_symbol_address( obf( "MmMarkPhysicalMemoryAsBad" ) );
        if ( !addr ) {
            logging::print( obf( "failed to find MmMarkPhysicalMemoryAsBad" ) );
            return false;
        }

        std::uint64_t base = pa;
        std::uint64_t bytes = size;

        auto result = g_syscall->call_kernel<std::int32_t>(
            addr,
            &base,
            &bytes
        );

        if ( result < 0 ) {
            logging::print( obf( "MmMarkPhysicalMemoryAsBad failed: 0x%x (pa: 0x%llx)" ), result, pa );
            return false;
        }

        return true;
    }

    physical_memory_range_t* mm_get_physical_memory_ranges( ) {
        static auto addr = g_pdb->get_symbol_address( obf( "MmGetPhysicalMemoryRanges" ) );
        if ( !addr ) {
            logging::print( obf( "failed to find MmGetPhysicalMemoryRanges" ) );
            return nullptr;
        }
        return g_syscall->call_kernel<physical_memory_range_t*>( addr );
    }

    template < typename type_t = std::uint64_t >
    std::optional< type_t > allocate_pages( std::size_t size ) {
        auto base_address = mm_allocate_independent_pages( size );
        if ( !base_address )
            return std::nullopt;

        kernel::memset( base_address, 0, size );
        return std::optional< type_t >( reinterpret_cast< type_t >( base_address ) );
    }

    // allocate private physical pages via MmAllocateIndependentPages.
    // returns the backing VA and extracted PFNs. the backing allocation must
    // remain alive while PFNs are mapped elsewhere — freed via free_backing_pages.
    struct backing_pages_t {
        std::uint64_t m_va;
        std::uint32_t m_size;
        std::vector<std::uint64_t> m_pfns;
    };

    std::optional<backing_pages_t> allocate_backing_pages( std::uint32_t page_count ) {
        const auto size = static_cast<std::size_t>( page_count ) * 0x1000;

        auto va = mm_allocate_independent_pages( size );
        if ( !va ) {
            logging::print( obf( "  backing: MmAllocateIndependentPages(%u pages) failed" ), page_count );
            return std::nullopt;
        }

        kernel::memset( va, 0, size );

        std::vector<std::uint64_t> pfns;
        pfns.reserve( page_count );

        for ( std::uint32_t i = 0; i < page_count; i++ ) {
            const auto page_va = reinterpret_cast<std::uint64_t>( va ) + i * 0x1000ULL;
            const auto pa = mm_get_physical_address( page_va );
            if ( !pa ) {
                logging::print( obf( "  backing: MmGetPhysicalAddress failed at page %u (va=0x%llx)" ),
                    i, page_va );
                mm_free_independent_pages( reinterpret_cast<std::uint64_t>( va ),
                    static_cast<std::uint32_t>( size ) );
                return std::nullopt;
            }
            pfns.push_back( pa >> 12 );
        }

        backing_pages_t result {};
        result.m_va = reinterpret_cast<std::uint64_t>( va );
        result.m_size = static_cast<std::uint32_t>( size );
        result.m_pfns = std::move( pfns );
        return result;
    }

    void free_backing_pages( std::uint64_t va, std::uint32_t size ) {
        if ( va && size )
            mm_free_independent_pages( va, size );
    }

    bool get_pfn_pte_frame( std::uint64_t pfn, std::uint64_t& out_pte_frame ) {
        static const auto off_u4 = g_pdb->get_struct_member( obf( "_MMPFN" ), obf( "u4" ) );
        if ( !off_u4 )
            return false;

        static const auto mmpfn_size = [ ]( ) -> std::uint64_t {
            const auto off = g_pdb->get_struct_member( obf( "_MMPFN" ), obf( "u4" ) );
            return off ? off + sizeof( std::uint64_t ) : 0x30;
            }( );

        const auto pfn_database = get_mm_pfn_database( );
        if ( !pfn_database )
            return false;

        const auto entry_va = pfn_database + pfn * mmpfn_size;
        const auto entry_pa = g_paging->translate_linear( entry_va );
        if ( !entry_pa )
            return false;

        std::uint64_t u4 = 0;
        if ( !memory::read_physical( entry_pa + off_u4, &u4, sizeof( u4 ) ) )
            return false;

        out_pte_frame = u4 & 0x000000FFFFFFFFFFULL;
        return true;
    }
}