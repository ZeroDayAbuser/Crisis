#pragma once

namespace disk {
    UNICODE_STRING    m_file_path = { };
    UNICODE_STRING    m_dir_path = { };
    OBJECT_ATTRIBUTES m_file_attrs = { };
    OBJECT_ATTRIBUTES m_dir_attrs = { };
    WCHAR             m_file_buf[ 64 ] = L"\\??\\C:\\crisis\\crisis.log";
    WCHAR             m_dir_buf[ 32 ]  = L"\\??\\C:\\crisis";
    WCHAR             m_discover_buf[ 80 ] = L"\\??\\C:\\crisis\\crisis.discover.log";
    UNICODE_STRING    m_discover_path = { };
    OBJECT_ATTRIBUTES m_discover_attrs = { };
    bool              m_overwrite = true;

    constexpr std::size_t k_queue_depth = 128;
    constexpr std::size_t k_queue_line  = 512;

    struct queue_entry_t {
        volatile long m_ready { };
        char          m_data[ k_queue_line ] { };
    };

    inline queue_entry_t m_queue[ k_queue_depth ] { };
    inline volatile long m_queue_head { };
    inline volatile long m_queue_tail { };
    inline volatile long m_worker_state { };
    //
    // File I/O must run on the kernel stack of the calling thread. A kernel file
    // open crosses FLTMGR, and FltpPassThroughInternal asks IoGetStackLimits for
    // the headroom. That reaches RtlpGetStackLimits -> KeQueryCurrentStackInformationEx,
    // which certifies the current SP against the _KTHREAD stack bounds -- and at
    // IRQL < 2 with interrupts enabled it does NOT certify the per-CPU ISR and
    // exception stacks. When the SP is one of those, it answers FALSE and
    // RtlpGetStackLimits executes __fastfail(FAST_FAIL_INCORRECT_STACK): 0x139.
    // That is the recurring crash: an exception context -- single-step or debug
    // trap on an IST stack -- logging to disk while still standing on it.
    //
    // Memory-only output (DbgPrint and the queue) stays enabled everywhere; only
    // the file write is gated. If the members do not resolve, fail closed -- a
    // lost log line is cheaper than the bugcheck.
    //
    bool file_io_on_thread_stack( ) {
        static const auto s_limit   = pdb::get_member( obf( "_KTHREAD" ), obf( "StackLimit" ) );
        static const auto s_initial = pdb::get_member( obf( "_KTHREAD" ), obf( "InitialStack" ) );
        if ( !s_limit || !s_initial )
            return false;

        const auto thread = reinterpret_cast< std::uint64_t >(
            kernel::ps_get_current_thread( ) );
        if ( !thread )
            return false;

        char probe = 0;
        const auto sp = reinterpret_cast< std::uint64_t >( &probe );
        const auto lo = *reinterpret_cast< const std::uint64_t* >(
            thread + s_limit );
        const auto hi = *reinterpret_cast< const std::uint64_t* >(
            thread + s_initial );

        const bool ok = lo && hi && sp >= lo && sp < hi;
        if ( !ok ) {
            static volatile long s_reported {};
            if ( InterlockedCompareExchange( &s_reported, 1, 0 ) == 0 )
                kernel::dbg_print( obf( "disk: file write suppressed: "
                    "sp=0x%llx thread stack=[0x%llx,0x%llx)\n" ),
                    sp, lo, hi );
        }
        return ok;
    }

    bool setup( const char* product_name = nullptr ) {
        if ( product_name && *product_name ) {
            wchar_t wide[ 32 ] = { };
            for ( int i = 0; product_name[ i ] && i < 30; ++i )
                wide[ i ] = static_cast< wchar_t >( product_name[ i ] );
            _snwprintf( m_dir_buf,  32, L"\\??\\C:\\%ls",          wide );
            _snwprintf( m_file_buf, 64, L"\\??\\C:\\%ls\\%ls.log", wide, wide );
        }
        kernel::rtl_init_unicode_string( &m_dir_path, m_dir_buf );
        InitializeObjectAttributes( &m_dir_attrs, &m_dir_path,
            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr );

        IO_STATUS_BLOCK io_status = { };
        HANDLE dir = nullptr;

        const auto result = ZwCreateFile( &dir, FILE_LIST_DIRECTORY | SYNCHRONIZE,
            &m_dir_attrs, &io_status, nullptr, FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN_IF,
            FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, nullptr, 0 );
        if ( result ) {
            kernel::dbg_print( obf( "create directory failed: 0x%08X\n" ), result );
            return false;
        }

        ZwClose( dir );

        kernel::rtl_init_unicode_string( &m_file_path, m_file_buf );
        InitializeObjectAttributes( &m_file_attrs, &m_file_path,
            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr );

        kernel::rtl_init_unicode_string( &m_discover_path, m_discover_buf );
        InitializeObjectAttributes( &m_discover_attrs, &m_discover_path,
            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr );
        return true;
    }

    bool write_file( const char* data, std::size_t size ) {
        if ( !data || !size )
            return false;

        if ( KeGetCurrentIrql( ) > PASSIVE_LEVEL )
            return false;

        if ( !file_io_on_thread_stack( ) )
            return false;

        IO_STATUS_BLOCK io_status = { };
        HANDLE file = nullptr;

        auto result = ZwCreateFile( &file, FILE_APPEND_DATA,
            &m_file_attrs, &io_status, nullptr, FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ, m_overwrite ? FILE_OVERWRITE_IF : FILE_OPEN_IF,
            FILE_SYNCHRONOUS_IO_NONALERT, nullptr, 0 );
        if ( result ) {
            kernel::dbg_print( obf( "create file failed: 0x%08X\n" ), result );
            return false;
        }

        io_status = { };
        result = ZwWriteFile( file, nullptr, nullptr, nullptr,
            &io_status, const_cast< char* >( data ),
            static_cast< ULONG >( size ), nullptr, nullptr );
        if ( result )
            kernel::dbg_print( obf( "write file failed: 0x%08X\n" ), result );

        for ( auto idx = 0; idx < 10; ++idx ) {
            if ( io_status.Status != STATUS_PENDING )
                break;
            KeStallExecutionProcessor( 100 );
        }

        ZwClose( file );
        m_overwrite = false;
        return NT_SUCCESS( result );
    }

    void enqueue( const char* data, std::size_t size ) {
        if ( !data || !size )
            return;

        const auto index = static_cast< std::size_t >(
            InterlockedIncrement( &m_queue_head ) - 1 ) % k_queue_depth;
        auto& entry = m_queue[ index ];

        const auto copy = size < k_queue_line ? size : k_queue_line - 1;
        std::memcpy( entry.m_data, data, copy );
        entry.m_data[ copy ] = 0;

        InterlockedExchange( &entry.m_ready, 1 );
    }

    void flush( ) {
        for ( ;; ) {
            const auto index = static_cast< std::size_t >(
                m_queue_tail ) % k_queue_depth;
            auto& entry = m_queue[ index ];

            if ( InterlockedCompareExchange( &entry.m_ready, 0, 0 ) == 0 )
                break;

            kernel::dbg_print( obf( "%s" ), entry.m_data );
            write_file( entry.m_data, strlen( entry.m_data ) );

            InterlockedExchange( &entry.m_ready, 0 );
            InterlockedIncrement( &m_queue_tail );
        }
    }

    void worker( void* ) {
        std::int64_t interval = -50000;
        for ( ;; ) {
            kernel::ke_delay_execution_thread( 0, false, &interval );
            flush( );
        }
    }

    void start_worker( ) {
        if ( m_worker_state )
            return;

        if ( KeGetCurrentIrql( ) > PASSIVE_LEVEL )
            return;

        if ( InterlockedCompareExchange( &m_worker_state, 1, 0 ) != 0 )
            return;

        void* handle = nullptr;
        const auto status = kernel::ps_create_system_thread(
            &handle, THREAD_ALL_ACCESS, nullptr, nullptr, nullptr,
            reinterpret_cast< void* >( worker ), nullptr );
        if ( !NT_SUCCESS( status ) ) {
            InterlockedExchange( &m_worker_state, 0 );
            return;
        }

        InterlockedExchange( &m_worker_state, 2 );
    }

    template< class... args_t >
    bool print( const char* format, args_t... va_args ) {
        char buffer[ 512 ] = "";
        _snprintf( buffer, sizeof( buffer ) - 2, format, va_args... );
        strcat( buffer, "\n" );

        if ( m_worker_state != 2 && KeGetCurrentIrql( ) == PASSIVE_LEVEL ) {
            kernel::dbg_print( obf( "%s" ), buffer );
            return write_file( buffer, strlen( buffer ) );
        }

        enqueue( buffer, strlen( buffer ) );
        return true;
    }

    bool append( const char* data, std::size_t size ) {
        if ( !data || !size )
            return false;

        if ( KeGetCurrentIrql( ) > PASSIVE_LEVEL )
            return false;

        if ( !file_io_on_thread_stack( ) )
            return false;

        IO_STATUS_BLOCK io_status = { };
        HANDLE file = nullptr;

        auto result = ZwCreateFile( &file, FILE_APPEND_DATA,
            &m_file_attrs, &io_status, nullptr, FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ, m_overwrite ? FILE_OVERWRITE_IF : FILE_OPEN_IF,
            FILE_SYNCHRONOUS_IO_NONALERT, nullptr, 0 );
        if ( result )
            return false;

        io_status = { };
        result = ZwWriteFile( file, nullptr, nullptr, nullptr,
            &io_status, const_cast< char* >( data ),
            static_cast< ULONG >( size ), nullptr, nullptr );

        ZwClose( file );
        m_overwrite = false;
        return NT_SUCCESS( result );
    }

    bool rotate( const char* header ) {
        m_overwrite = true;
        if ( !header )
            return true;
        return append( header, strlen( header ) );
    }

    bool append_discovery( const char* data, std::size_t size ) {        if ( !data || !size )
            return false;

        if ( KeGetCurrentIrql( ) > PASSIVE_LEVEL )
            return false;

        if ( !file_io_on_thread_stack( ) )
            return false;

        IO_STATUS_BLOCK io_status = { };
        HANDLE file = nullptr;

        auto result = ZwCreateFile( &file, FILE_APPEND_DATA,
            &m_discover_attrs, &io_status, nullptr, FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN_IF,
            FILE_SYNCHRONOUS_IO_NONALERT, nullptr, 0 );
        if ( result )
            return false;

        io_status = { };
        result = ZwWriteFile( file, nullptr, nullptr, nullptr,
            &io_status, const_cast< char* >( data ),
            static_cast< ULONG >( size ), nullptr, nullptr );

        ZwClose( file );
        return NT_SUCCESS( result );
    }
}
