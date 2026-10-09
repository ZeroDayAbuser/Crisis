#pragma once
#define page_align(va) ((std::uint64_t)(va) & ~(paging::page_4kb_size - 1))
volatile std::uint64_t m_last_sid_time = 0;

namespace etw {
    void setup( );
    void track( std::uint32_t );
}

namespace etw {
    constexpr auto etwp_start_trace = 1;
    constexpr auto etwp_stop_trace = 2;
    constexpr auto etwp_query_trace = 3;
    constexpr auto etwp_update_trace = 4;
    constexpr auto etwp_flush_trace = 5;

    constexpr auto wnode_flag_traced_guid = 0x00020000;
    constexpr auto event_trace_buffering_mode = 0x00000400;
    constexpr auto event_trace_flag_systemcall = 0x00000080;

    constexpr auto max_event_hooks = 512;
    std::uintptr_t m_ki_system_call64_start = 0;
    std::uintptr_t m_ki_system_call64_end = 0;
    const guid_t ckcl_session_guid =
    { 0x54dea73a, 0xed1f, 0x42a4, { 0xaf, 0x71, 0x3e, 0x63, 0xd0, 0x56, 0xf1, 0x74 } };

    struct hook_state_t {
        std::uint32_t   m_syscall_index;
        std::uintptr_t  m_ssdt_handler;
        void*           m_hook_handler;
        void**          m_original_address;
        char            m_function_name[ 64 ];
        bool            m_active;
    };

    hook_state_t    m_event_hooks[ max_event_hooks ]{ };
    std::size_t     m_hook_count = 0;

    std::uint64_t( *etw_trace_silo_kernel_event_org )(
        std::uint64_t,
        std::uint64_t,
        std::uint32_t,
        std::uint32_t,
        std::uint16_t,
        std::uint32_t
        ) = nullptr;

    std::size_t get_hook( std::uint32_t syscall_index ) {
        for ( auto idx = 0u; idx < m_hook_count; idx++ ) {
            if ( m_event_hooks[ idx ].m_syscall_index == syscall_index )
                return idx;
        }
        return static_cast< std::size_t >( -1 );
    }

    const char* get_hook_name( std::uint32_t syscall_index ) {
        auto hook_idx = get_hook( syscall_index );
        if ( hook_idx == static_cast< std::size_t >( -1 ) )
            return obf( "unknown" );

        auto name = m_event_hooks[ hook_idx ].m_function_name;
        if ( !name[ 0 ] )
            return obf( "unknown" );

        return name;
    }

    std::uint64_t etw_trace_silo_kernel_event_hk(
        std::uint64_t silo_context,
        std::uint64_t event_data,
        std::uint32_t event_count,
        std::uint32_t group_mask,
        std::uint16_t hook_id,
        std::uint32_t event_flags
    ) {
        if ( hook_id != 0xF33 )
            goto original;

        if ( KeGetCurrentIrql( ) > DISPATCH_LEVEL )
            goto original;

        if ( ExGetPreviousMode( ) == KernelMode )
            goto original;

        {
            auto current_thread = kernel::ps_get_current_thread( );
            if ( !current_thread )
                goto original;

            auto process = kernel::ps_get_current_process( );
            if ( !kernel::mm_is_address_valid( process ) )
                goto original;

            auto system_call_number = reinterpret_cast< std::uint32_t* >(
                reinterpret_cast< std::uint8_t* >( current_thread ) + 0x80 );
            if ( !kernel::mm_is_address_valid( system_call_number ) )
                goto original;

            char image_file_name[ 16 ]{ };
            if ( !memory::get_image_file_name( process, image_file_name, sizeof( image_file_name ) ) )
                goto original;

            const auto is_target =
                strstr_lowercase( image_file_name, obf( "fortnite" ) ) ||
                strstr_lowercase( image_file_name, obf( "rust" ) ) ||
                strstr_lowercase( image_file_name, obf( "eac" ) )/* ||
                strstr_lowercase( image_file_name, obf( "epic" ) )*/;

            if ( !is_target )
                goto original;

			etw::track( *system_call_number );

            auto hook_idx = get_hook( *system_call_number );
            if ( hook_idx == static_cast< std::size_t >( -1 ) )
                goto original;

            auto& entry = m_event_hooks[ hook_idx ];
            if ( !entry.m_active || !entry.m_original_address ||
                !entry.m_hook_handler || !entry.m_ssdt_handler )
                goto original;

            auto stack_bottom = reinterpret_cast< std::uintptr_t* >( _AddressOfReturnAddress( ) );
            auto stack_top = reinterpret_cast< std::uintptr_t* >( __readgsqword( 0x1A8 ) );

            for ( auto p = stack_bottom; p < stack_top; ++p ) {
                if ( !kernel::mm_is_address_valid( p ) )
                    continue;

                auto val = *p;
                if ( val < m_ki_system_call64_start || val >= m_ki_system_call64_end )
                    continue;

                for ( int fwd = 1; fwd < 128; ++fwd ) {
                    auto slot = p + fwd;
                    if ( !kernel::mm_is_address_valid( slot ) )
                        break;

                    if ( *slot != entry.m_ssdt_handler )
                        continue;

                    *entry.m_original_address = reinterpret_cast< void* >( *slot );
                    *slot = reinterpret_cast< std::uintptr_t >( entry.m_hook_handler );
                    goto original;
                }
                break;
            }
        }

    original:
        return etw_trace_silo_kernel_event_org(
            silo_context, event_data, event_count,
            group_mask, hook_id, event_flags );
    }

    bool configure_trace( ckcl_trace_operation_t operation ) {
        auto property = kernel::mm_allocate_independent_pages
            < ckcl_trace_properties_t* >( paging::page_4kb_size );
        if ( !property )
            return false;

        property->m_wnode.m_buffer_size = paging::page_4kb_size;
        property->m_wnode.m_flags = wnode_flag_traced_guid;
        property->m_provider_name = RTL_CONSTANT_STRING( L"Circular Kernel Context Logger" );
        property->m_wnode.m_guid = ckcl_session_guid;
        property->m_wnode.m_client_context = 1;
        property->m_buffer_size = sizeof( unsigned long );
        property->m_minimum_buffers = property->m_maximum_buffers = 2;
        property->m_log_file_mode = event_trace_buffering_mode;

        auto result = nt_status_t::success;
        std::uint32_t ret_len = 0;

        switch ( operation ) {
            case ckcl_trace_operation_t::start: {
                result = kernel::zw_trace_control(
                    etwp_start_trace, property, paging::page_4kb_size,
                    property, paging::page_4kb_size, &ret_len );
            } break;
            case ckcl_trace_operation_t::end: {
                result = kernel::zw_trace_control(
                    etwp_stop_trace, property, paging::page_4kb_size,
                    property, paging::page_4kb_size, &ret_len );
            } break;
            case ckcl_trace_operation_t::syscall: {
                property->m_enable_flags = event_trace_flag_systemcall;
                result = kernel::zw_trace_control(
                    etwp_update_trace, property, paging::page_4kb_size,
                    property, paging::page_4kb_size, &ret_len );
            } break;
        }

        kernel::mm_free_independent_pages(
            reinterpret_cast< std::uint64_t >( property ), paging::page_4kb_size );
        return !result;
    }

    bool restore_page_execute( paging::pph::hook_state_t* state ) {
        if ( !state )
            return false;

        pte entry{};
        if ( !paging::dpm::read_physical( state->m_original_pte_pa, &entry, sizeof( pte ) ) )
            return false;

        if ( !( entry.value & ( 1ull << 63 ) ) )
            return true;

        entry.value &= ~( 1ull << 63 );

        if ( !paging::dpm::write_physical( state->m_original_pte_pa, &entry, sizeof( pte ) ) )
            return false;

        kernel::ke_flush_entire_tb( true, true );
        disk::print( obf( "cleared NX on private page [pte_pa: 0x%llx]" ), state->m_original_pte_pa );
        return true;
    }

    bool initialize( ) {
        ULONG64 image_base = 0;
        ULONG   table_length = 0;
        etw::setup( );

        static const auto s_ki_syscall64 = pdb::get_symbol( obf("KiSystemCall64") );
        static const auto s_etw_silo     = pdb::get_symbol( obf("EtwTraceSiloKernelEvent") );
        auto rf = kernel::rtl_lookup_function_entry( s_ki_syscall64, &image_base, nullptr );
        if ( !rf ) return false;

        m_ki_system_call64_start = image_base + rf->m_begin_address;
        m_ki_system_call64_end = image_base + rf->m_end_address;

        auto etw_state = exception::hook::create_hook( s_etw_silo, &etw_trace_silo_kernel_event_org );
        if ( !etw_state ) { disk::print( obf( "could not create hook -> EtwTraceSiloKernelEvent\n" ) ); return false; }
        if ( !exception::hook::enable_hook( etw_state, reinterpret_cast< std::uint64_t >( etw_trace_silo_kernel_event_hk ) ) ) { disk::print( obf( "could not enable hook -> EtwTraceSiloKernelEvent\n" ) ); return false; }
		if ( !exception::hook::swap_to_hooked( etw_state ) ) { disk::print( obf( "could not swap hook -> EtwTraceSiloKernelEvent\n" ) ); return false; }

        if ( !configure_trace( ckcl_trace_operation_t::syscall ) &&
            ( !configure_trace( ckcl_trace_operation_t::start ) ||
            !configure_trace( ckcl_trace_operation_t::syscall ) ) )
            return false;
        return true;
    }

    template < typename org_t >
    hook_state_t* create_hook( const char* function_name, void* hook_handler, org_t* original ) {
        if ( m_hook_count >= max_event_hooks )
            return nullptr;

        auto syscall_index = memory::get_syscall_index( function_name );
        if ( syscall_index == static_cast< std::uint32_t >( -1 ) ) {
            disk::print( obf( "failed to get syscall index -> %s" ), function_name );
            return nullptr;
        }

        if ( get_hook( syscall_index ) != static_cast< std::size_t >( -1 ) ) {
            disk::print( obf( "already registered -> %s" ), function_name );
            return nullptr;
        }

        auto ssdt_handler = memory::get_ssdt_handler( syscall_index );
        if ( !ssdt_handler ) {
            disk::print( obf( "failed to get ssdt handler -> %s [syscall -> 0x%x]" ), function_name, syscall_index );
            return nullptr;
        }

        auto& entry = m_event_hooks[ m_hook_count++ ];
        entry.m_syscall_index = syscall_index;
        entry.m_hook_handler = hook_handler;
        entry.m_original_address = reinterpret_cast< void** >( original );
        entry.m_ssdt_handler = ssdt_handler;
        RtlSecureZeroMemory( entry.m_function_name, sizeof( entry.m_function_name ) );
        if ( function_name ) {
            for ( auto idx = 0u; idx < sizeof( entry.m_function_name ) - 1; ++idx ) {
                entry.m_function_name[ idx ] = function_name[ idx ];
                if ( !function_name[ idx ] )
                    break;
            }
        }
        entry.m_active = true;
        return &entry;
    }

    bool remove_hook( hook_state_t* state ) {
        if ( !state )
            return false;

        state->m_active = false;
        state->m_hook_handler = nullptr;
        state->m_original_address = nullptr;
        RtlSecureZeroMemory( state->m_function_name, sizeof( state->m_function_name ) );
        state->m_syscall_index = 0;
        return true;
    }

    bool unload( ) {
        return configure_trace( ckcl_trace_operation_t::end );
    }
}
