#pragma once

namespace kernel {
    void rtl_zero_memory( void* destination, std::size_t length ) {
        if ( !destination || !length )
            return;

        std::memset( destination, 0, length );
    }

    void rtl_copy_memory( void* destination, const void* source, std::size_t length ) {
        if ( !destination || !source || !length )
            return;

        std::memcpy( destination, source, length );
    }

    void ke_initialize_spin_lock( PKSPIN_LOCK spin_lock ) {
        if ( !spin_lock )
            return;

        static auto fn_address = 0ull;
        if ( !fn_address )
            fn_address = get_export( m_ntoskrnl_base, obf( "KeInitializeSpinLock" ) );

        if ( !fn_address ) {
            *spin_lock = { };
            return;
        }

        using function_t = void( * )( PKSPIN_LOCK );
        reinterpret_cast< function_t >( fn_address )( spin_lock );
    }

    void ke_acquire_spin_lock( PKSPIN_LOCK spin_lock, PKIRQL old_irql ) {
        if ( !spin_lock || !old_irql )
            return;

        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "KeAcquireSpinLockRaiseToDpc" ) );
            if ( !fn_address ) return;
        }

        using function_t = KIRQL( * )( PKSPIN_LOCK );
        *old_irql = reinterpret_cast< function_t >( fn_address )( spin_lock );
    }

    void ke_release_spin_lock( PKSPIN_LOCK spin_lock, KIRQL old_irql ) {
        if ( !spin_lock )
            return;

        static auto fn_address = 0ull;
        if ( !fn_address )
            fn_address = get_export( m_ntoskrnl_base, obf( "KeReleaseSpinLock" ) );

        if ( fn_address ) {
            using function_t = void( * )( PKSPIN_LOCK, KIRQL );
            reinterpret_cast< function_t >( fn_address )( spin_lock, old_irql );
            return;
        }

        static auto release_from_dpc_address = 0ull;
        if ( !release_from_dpc_address )
            release_from_dpc_address = get_export( m_ntoskrnl_base, obf( "KeReleaseSpinLockFromDpcLevel" ) );

        if ( release_from_dpc_address ) {
            using release_from_dpc_t = void( * )( PKSPIN_LOCK );
            reinterpret_cast< release_from_dpc_t >( release_from_dpc_address )( spin_lock );
        }

        static auto lower_irql_address = 0ull;
        if ( !lower_irql_address )
            lower_irql_address = get_export( m_ntoskrnl_base, obf( "KeLowerIrql" ) );

        if ( lower_irql_address ) {
            using lower_irql_t = void( * )( KIRQL );
            reinterpret_cast< lower_irql_t >( lower_irql_address )( old_irql );
        }
    }

    physical_memory_range_t* mm_get_physical_memory_ranges( ) {
        static auto export_address = 0ull;
        if ( !export_address ) {
            export_address = get_export( m_ntoskrnl_base, obf( "MmGetPhysicalMemoryRanges" ) );
            if ( !export_address ) return nullptr;
        }

        using function_t = physical_memory_range_t * ( void );
        return reinterpret_cast< function_t* >( export_address )( );
    }

    nt_status_t ob_reference_object_by_handle(
        void* handle,
        std::uint32_t desired_access,
        void* object_type,
        std::uint8_t access_mode,
        void** object,
        void* handle_information
    ) {
        static auto function_address = 0ull;
        if ( !function_address ) {
            function_address = get_export( m_ntoskrnl_base, obf( "ObReferenceObjectByHandle" ) );
            if ( !function_address ) return nt_status_t::unsuccessful;
        }

        using function_t = nt_status_t( * )(
            void*,
            std::uint32_t,
            void*,
            std::uint8_t,
            void**,
            void*
            );

        return reinterpret_cast< function_t >( function_address )(
            handle,
            desired_access,
            object_type,
            access_mode,
            object,
            handle_information
            );
    }

    object_type_t* io_file_object_type( ) {
        static auto io_file_object_type_ptr = 0ull;
        if ( !io_file_object_type_ptr ) {
            io_file_object_type_ptr = get_export( m_ntoskrnl_base, obf( "IoFileObjectType" ) );
            if ( !io_file_object_type_ptr ) return nullptr;
        }

        return *reinterpret_cast< object_type_t** >( io_file_object_type_ptr );
    }

    void ob_dereference_object( void* object ) {
        static auto ob_dereference_object = 0ull;
        if ( !ob_dereference_object ) {
            ob_dereference_object = get_export( m_ntoskrnl_base, obf( "ObDereferenceObject" ) );
            if ( !ob_dereference_object ) return;
        }

        using function_t = void( void* );
        reinterpret_cast< function_t* >( ob_dereference_object )( object );
    }

    nt_status_t zw_query_information_file(
        void* file_handle,
        io_status_block_t* io_status,
        void* file_information,
        std::uint32_t length,
        std::uint32_t file_information_class
    ) {
        static std::uint64_t fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "ZwQueryInformationFile" ) );
            if ( !fn_address ) return { };
        }

        using function_t = nt_status_t( * )( void*, io_status_block_t*, void*, std::uint32_t, std::uint32_t );
        return reinterpret_cast< function_t >( fn_address )( file_handle, io_status, file_information, length, file_information_class );
    }

    nt_status_t zw_open_key(
        void** key_handle,
        unsigned long desired_access,
        OBJECT_ATTRIBUTES* object_attributes
    ) {
        static std::uint64_t fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "ZwOpenKey" ) );
            if ( !fn_address ) return nt_status_t::unsuccessful;
        }

        using function_t = nt_status_t( * )( void**, unsigned long, OBJECT_ATTRIBUTES* );
        return reinterpret_cast< function_t >( fn_address )( key_handle, desired_access, object_attributes );
    }

    nt_status_t zw_query_value_key(
        void* key_handle,
        UNICODE_STRING* value_name,
        std::uint32_t key_value_information_class,
        void* key_value_information,
        unsigned long length,
        unsigned long* result_length
    ) {
        static std::uint64_t fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "ZwQueryValueKey" ) );
            if ( !fn_address ) return nt_status_t::unsuccessful;
        }

        using function_t = nt_status_t( * )( void*, UNICODE_STRING*, std::uint32_t, void*, unsigned long, unsigned long* );
        return reinterpret_cast< function_t >( fn_address )(
            key_handle, value_name, key_value_information_class,
            key_value_information, length, result_length );
    }

    nt_status_t zw_close( void* handle ) {
        static std::uint64_t fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "ZwClose" ) );
            if ( !fn_address ) return { };
        }

        using function_t = nt_status_t( * )( void* );
        return reinterpret_cast< function_t >( fn_address )( handle );
    }

    nt_status_t zw_query_object(
        void* handle,
        std::uint32_t object_information_class,
        void* object_information,
        unsigned long object_information_length,
        unsigned long* return_length
    ) {
        static std::uint64_t fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "ZwQueryObject" ) );
            if ( !fn_address ) return nt_status_t::unsuccessful;
        }

        using function_t = nt_status_t( * )( void*, std::uint32_t, void*, unsigned long, unsigned long* );
        return reinterpret_cast< function_t >( fn_address )(
            handle,
            object_information_class,
            object_information,
            object_information_length,
            return_length );
    }

    template<typename type_t = std::uint64_t>
    type_t mm_get_virtual_for_physical( std::uint64_t phys_addr ) {
        static auto export_address = 0ull;
        if ( !export_address ) {
            export_address = get_export( m_ntoskrnl_base,
                obf( "MmGetVirtualForPhysical" ) );
            if ( !export_address ) return {};
        }

        using function_t = void* ( * )( std::uint64_t );
        return reinterpret_cast< type_t >(
            reinterpret_cast< function_t >( export_address )( phys_addr ) );
    }

    template<typename type_t = std::uint64_t>
    type_t mm_get_physical_address( std::uint64_t virtual_address ) {
        static auto export_address = 0ull;
        if ( !export_address ) {
            export_address = get_export( m_ntoskrnl_base,
                obf( "MmGetPhysicalAddress" ) );
            if ( !export_address ) return {};
        }

        using function_t = void* ( * )( std::uint64_t );
        return reinterpret_cast< type_t >(
            reinterpret_cast< function_t >( export_address )( virtual_address ) );
    }

    std::uint32_t ke_get_current_processor_number( ) {
        static auto fn_ke_get_current_processor_number = 0ull;
        if ( !fn_ke_get_current_processor_number ) {
            fn_ke_get_current_processor_number = get_export( m_ntoskrnl_base, obf( "KeGetCurrentProcessorNumberEx" ) );
            if ( !fn_ke_get_current_processor_number ) return {};
        }

        using function_t = std::uint32_t( __int64 );
        return reinterpret_cast< function_t* >( fn_ke_get_current_processor_number )( 0 );
    }

    std::uint64_t ke_query_system_time_precise( ) {
        static std::uint64_t ke_query_system_time_precise = 0ull;
        if ( !ke_query_system_time_precise ) {
            ke_query_system_time_precise = get_export( m_ntoskrnl_base, obf( "KeQuerySystemTimePrecise" ) );
            if ( !ke_query_system_time_precise )
                return {};
        }
        using ke_query_system_time_precise_t = void( __fastcall* )( ularge_integer_t* );

        ularge_integer_t system_time = {};
        reinterpret_cast< ke_query_system_time_precise_t >( ke_query_system_time_precise )( &system_time );
        return system_time.m_quad_part;
    }

    ularge_integer_t ke_query_tick_count( ) {
        static std::uint64_t ke_query_tick_count_addr = 0ull;
        if ( !ke_query_tick_count_addr ) {
            ke_query_tick_count_addr = get_export( m_ntoskrnl_base, obf( "KeQueryTickCount" ) );
            if ( !ke_query_tick_count_addr )
                return {};
        }
        using ke_query_tick_count_t = void( __fastcall* )( ularge_integer_t* );

        ularge_integer_t tick_count = {};
        reinterpret_cast< ke_query_tick_count_t >( ke_query_tick_count_addr )( &tick_count );
        return tick_count;
    }

    std::uint32_t rtl_random_ex( unsigned long* seed ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "RtlRandomEx" ) );
            if ( !fn_address ) return {};
        }

        using function_t = std::uint32_t( unsigned long* );
        return reinterpret_cast< function_t* >( fn_address )( seed );
    }


    nt_status_t ob_query_name_string(
        void* object,
        object_name_information_t* object_name_info,
        unsigned long length,
        unsigned long* return_length
    ) {
        static auto ob_query_name_string = 0ull;
        if ( !ob_query_name_string ) {
            ob_query_name_string = get_export( m_ntoskrnl_base, obf( "ObQueryNameString" ) );
            if ( !ob_query_name_string ) return {};
        }

        using function_t = nt_status_t( void*, object_name_information_t*, unsigned long, unsigned long* );
        return reinterpret_cast< function_t* >( ob_query_name_string )( object, object_name_info, length, return_length );
    }


    template<class... args_t>
    std::int8_t dbg_print( const char* format, args_t... va_args ) {
        static auto export_address = 0ull;
        if ( !export_address ) {
            export_address = get_export( m_ntoskrnl_base, obf( "DbgPrintEx" ) );
            if ( !export_address ) return 0;
        }

        // Always emit. The two guards that used to live here bailed out when
        // KdDebuggerEnabled was set or KdDebuggerNotPresent was clear - which is exactly
        // the state DbgView's kernel capture creates, so the driver went silent the
        // moment anything was listening for it and no diagnostic in it was reachable.

        using function_t = std::int32_t( std::uint32_t flag, std::uint32_t level,
            const char* format, args_t... va_args );
        return reinterpret_cast< function_t* >( export_address )( 0, 0, format, va_args... ) ==
            nt_status_t::success;
    }

    nt_status_t zw_terminate_thread( void* thread_handle, std::int32_t exit_status ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "ZwTerminateThread" ) );
            if ( !fn_address ) return nt_status_t::unsuccessful;
        }

        using function_t = nt_status_t( * )( void*, std::int32_t );
        return reinterpret_cast< function_t >( fn_address )( thread_handle, exit_status );
    }

    template <typename type_t = std::uint64_t>
    type_t mm_allocate_independent_pages( std::size_t size ) {
        const auto mm_allocate_independent_pages = pdb::get_symbol( obf("MmAllocateIndependentPagesEx") );
        if ( !mm_allocate_independent_pages )
            return { };

        using function_t = void* ( std::uint64_t, int, std::uint64_t, unsigned int );
        auto base_address = reinterpret_cast< function_t* >( mm_allocate_independent_pages )( size, -1, 0, 0 );
        if ( !base_address )
            return { };

        memset( base_address, 0, size );
        return reinterpret_cast< type_t >( base_address );
    }

    bool mm_set_page_protection( std::uint64_t virtual_address, std::size_t size, std::uint64_t protection ) {
        const auto mm_set_page_protection = pdb::get_symbol( obf("MmSetPageProtection") );
        if ( !mm_set_page_protection )
            return 0;

        using function_t = bool( __stdcall* )( std::uint64_t, std::size_t, std::uint64_t );
        return reinterpret_cast< function_t >( mm_set_page_protection )( virtual_address, size, protection );
    }

    nt_status_t zw_create_file(
        void* file_handle,
        std::uint32_t desired_access,
        object_attributes_t* object_attributes,
        io_status_block_t* io_status,
        ularge_integer_t* allocation_size,
        std::uint32_t file_attributes,
        std::uint32_t share_access,
        std::uint32_t create_disposition,
        std::uint32_t create_options,
        void* ea_buffer,
        std::uint32_t ea_length
    ) {
        static std::uint64_t fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "ZwCreateFile" ) );
            if ( !fn_address ) return { };
        }

        using function_t = nt_status_t( * )(
            void*, std::uint32_t, object_attributes_t*, io_status_block_t*, ularge_integer_t*,
            std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, void*, std::uint32_t );
        return reinterpret_cast< function_t >( fn_address )(
            file_handle, desired_access, object_attributes, io_status, allocation_size,
            file_attributes, share_access, create_disposition, create_options, ea_buffer, ea_length );
    }

    nt_status_t zw_read_file(
        void* file_handle,
        void* event_handle,
        void* apc_routine,
        void* apc_context,
        io_status_block_t* io_status,
        void* buffer,
        std::uint32_t length,
        ularge_integer_t* byte_offset,
        std::uint32_t* key
    ) {
        static std::uint64_t fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "ZwReadFile" ) );
            if ( !fn_address ) return { };
        }

        using function_t = nt_status_t( * )(
            void*, void*, void*, void*, io_status_block_t*, void*, std::uint32_t, ularge_integer_t*, std::uint32_t* );
        return reinterpret_cast< function_t >( fn_address )(
            file_handle, event_handle, apc_routine, apc_context, io_status, buffer, length, byte_offset, key );
    }

    void rtl_init_unicode_string( unicode_string_t* destination_string, const wchar_t* source_string ) {
        static std::uint64_t function_addr = 0;

        if ( !function_addr )
            function_addr = get_export( m_ntoskrnl_base, obf( "RtlInitUnicodeString" ) );

        using fn_t = void( * )( unicode_string_t*, const wchar_t* );
        reinterpret_cast< fn_t >( function_addr )( destination_string, source_string );
    }

    void rtl_init_unicode_string( UNICODE_STRING* destination_string, const wchar_t* source_string ) {
        static std::uint64_t function_addr = 0;

        if ( !function_addr )
            function_addr = get_export( m_ntoskrnl_base, obf( "RtlInitUnicodeString" ) );

        using fn_t = void( * )( UNICODE_STRING*, const wchar_t* );
        reinterpret_cast< fn_t >( function_addr )( destination_string, source_string );
    }

    void ke_flush_single_tb( std::uintptr_t address, bool all_processors, bool invalidate ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "KeFlushSingleTb" ) );
            if ( !fn_address ) return;
        }

        using function_t = void( * )( std::uintptr_t address, bool all_processors, bool invalidate );
        reinterpret_cast< function_t >( fn_address )( address, all_processors, invalidate );
    }

    void ke_flush_entire_tb( bool invalidate, bool all_processors ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "KeFlushEntireTb" ) );
            if ( !fn_address ) return;
        }

        using function_t = void( * )( bool invalidate, bool all_processors );
        reinterpret_cast< function_t >( fn_address )( invalidate, all_processors );
    }

    void ke_invalidate_all_caches( ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "KeInvalidateAllCaches" ) );
            if ( !fn_address ) return;
        }

        using function_t = void( * )( );
        reinterpret_cast< function_t >( fn_address )( );
    }

    void flush_caches( std::uint64_t address ) {
        auto cr4 = __readcr4( );
        auto pge_was_enabled = ( cr4 & ( 1ull << 7 ) ) != 0;
        if ( pge_was_enabled ) {
            __writecr4( cr4 & ~( 1ull << 7 ) );
            __writecr4( cr4 );
        }

        __writecr3( __readcr3( ) );

        kernel::ke_flush_entire_tb( true, true );
        kernel::ke_invalidate_all_caches( );
        kernel::ke_flush_single_tb( address, true, true );
    }

    void* se_register_image_verification_callback(
        std::uint32_t type,
        std::uint32_t flags,
        void* callback,
        void* context,
        std::uint64_t reserved,
        void** out_handle ) {

        static auto export_address = 0ull;
        if ( !export_address ) {
            export_address = get_export( m_ntoskrnl_base,
                obf( "SeRegisterImageVerificationCallback" ) );
            if ( !export_address ) return nullptr;
        }

        using function_t = std::int32_t( * )(
            std::uint32_t type,
            std::uint32_t flags,
            void* callback,
            void* context,
            std::uint64_t reserved,
            void** out_handle );

        const auto status = reinterpret_cast< function_t >( export_address )(
            type, flags, callback, context, reserved, out_handle );

        return !status ? *out_handle : nullptr;
    }

    bool rtl_compare_unicode_string( unicode_string_t* str1, unicode_string_t* str2, bool case_insensitive ) {
        static std::uint64_t fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base,
                obf( "RtlCompareUnicodeString" ) );
        }
        if ( !fn_address )
            return false;

        using function_t = std::int32_t( * )( unicode_string_t*, unicode_string_t*, bool );
        return reinterpret_cast< function_t >( fn_address )( str1, str2, case_insensitive ) == 0;
    }

    bool ps_set_load_image_notify_routine( void* callback ) {
        static std::uint64_t fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base,
                obf( "PsSetLoadImageNotifyRoutineEx" ) );
            if ( !fn_address ) return false;
        }
        using function_t = std::int32_t( * )( void*, std::uint64_t );
        return !(
            reinterpret_cast< function_t >( fn_address )( callback, 0 ) );
    }

    bool ps_remove_load_image_notify_routine( void* callback ) {
        static std::uint64_t fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base,
                obf( "PsRemoveLoadImageNotifyRoutine" ) );
            if ( !fn_address ) return false;
        }
        using function_t = std::int32_t( * )( void* );
        return !(
            reinterpret_cast< function_t >( fn_address )( callback ) );
    }

    bool se_unregister_image_verification_callback( void* handle ) {
        static auto export_address = 0ull;
        if ( !export_address ) {
            export_address = get_export( m_ntoskrnl_base,
                obf( "SeUnregisterImageVerificationCallback" ) );
            if ( !export_address ) return false;
        }

        using function_t = std::int32_t( * )( void* handle );
        return reinterpret_cast< function_t >( export_address )( handle );
    }

    void* ex_allocate_pool_with_tag( pool_type_t pool_type, std::size_t number_of_bytes, unsigned long tag ) {
        static auto export_address = 0ull;
        if ( !export_address ) {
            export_address = get_export( m_ntoskrnl_base, obf( "ExAllocatePoolWithTag" ) );
            if ( !export_address ) return nullptr;
        }

        using function_t = void* ( * )( pool_type_t, std::size_t, unsigned long );
        return reinterpret_cast< function_t >( export_address )( pool_type, number_of_bytes, tag );
    }

    nt_status_t ex_get_firmware_environment_variable(
        UNICODE_STRING* variable_name,
        GUID* vendor_guid,
        void* value,
        unsigned long* value_length,
        unsigned long* attributes
    ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "ExGetFirmwareEnvironmentVariable" ) );
            if ( !fn_address ) return nt_status_t::unsuccessful;
        }

        using function_t = nt_status_t( * )(
            UNICODE_STRING*,
            GUID*,
            void*,
            unsigned long*,
            unsigned long*
            );

        return reinterpret_cast< function_t >( fn_address )(
            variable_name,
            vendor_guid,
            value,
            value_length,
            attributes
            );
    }

    nt_status_t ex_set_firmware_environment_variable(
        UNICODE_STRING* variable_name,
        GUID* vendor_guid,
        void* value,
        unsigned long value_length,
        unsigned long attributes
    ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "ExSetFirmwareEnvironmentVariable" ) );
            if ( !fn_address ) return nt_status_t::unsuccessful;
        }

        using function_t = nt_status_t( * )(
            UNICODE_STRING*,
            GUID*,
            void*,
            unsigned long,
            unsigned long
            );

        return reinterpret_cast< function_t >( fn_address )(
            variable_name,
            vendor_guid,
            value,
            value_length,
            attributes
            );
    }

    nt_status_t zw_trace_control(
        std::uint32_t function_code,
        void* in_buffer,
        std::uint32_t in_buffer_len,
        void* out_buffer,
        std::uint32_t out_buffer_len,
        std::uint32_t* return_length
    ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "ZwTraceControl" ) );
            if ( !fn_address ) return nt_status_t::unsuccessful;
        }

        using function_t = nt_status_t( * )(
            std::uint32_t,
            void*,
            std::uint32_t,
            void*,
            std::uint32_t,
            std::uint32_t*
            );

        return reinterpret_cast< function_t >( fn_address )(
            function_code,
            in_buffer,
            in_buffer_len,
            out_buffer,
            out_buffer_len,
            return_length
            );
    }

    void mm_free_independent_pages( std::uint64_t independent_pages, std::size_t size ) {
        const auto mm_free_independent_pages = pdb::get_symbol( obf("MmFreeIndependentPages") );
        if ( !mm_free_independent_pages )
            return;

        using function_t = void( std::uint64_t, int );
        reinterpret_cast< function_t* >( mm_free_independent_pages )( independent_pages, size );
    }

    bool mm_is_address_valid( void* virtual_address ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "MmIsAddressValid" ) );
            if ( !fn_address ) return {};
        }

        using function_t = bool( void* virtual_address );
        return reinterpret_cast< function_t* >( fn_address )( virtual_address );
    }

    list_entry_t* get_rtlp_debug_print_callback_list( ) {
        const auto s_addr = pdb::get_symbol( obf("RtlpDebugPrintCallbackList") );
        return reinterpret_cast< list_entry_t* >( s_addr );
    }

    std::int32_t mi_copy_on_write( std::uint64_t va, std::uint64_t* pte_va ) {
        const auto fn = pdb::get_symbol( obf("MiCopyOnWrite") );
        if ( !fn ) return -1;
        using function_t = std::int32_t( * )( std::uint64_t, std::uint64_t*, std::int64_t, std::int32_t );
        return reinterpret_cast< function_t >( fn )( va, pte_va, -1, 0 );
    }

    mmpfn_t* get_mm_pfn_database( ) {
        static std::uint8_t* export_address = 0ull;
        if ( !export_address ) {
            export_address = reinterpret_cast< std::uint8_t* >(
                get_export( m_ntoskrnl_base, obf( "KeCapturePersistentThreadState" ) ) );
            if ( !export_address ) return { };
        }

        while ( export_address[ 0x0 ] != 0x48
            || export_address[ 0x1 ] != 0x8B
            || export_address[ 0x2 ] != 0x05 )
            export_address++;

        return *reinterpret_cast< mmpfn_t** >(
            &export_address[ 0x7 ] + *reinterpret_cast< std::int32_t* >( &export_address[ 0x3 ] ) );
    }

    __int64 __fastcall mi_lock_page_table_page( mmpfn_t* pfn_entry, int a2 ) {
        const auto mi_lock_page_table_page = pdb::get_symbol( obf("MiLockPageTablePage") );
        if ( !mi_lock_page_table_page )
            return 0;

        using function_t = __int64( mmpfn_t*, int );
        return reinterpret_cast< function_t* >( mi_lock_page_table_page )( pfn_entry, a2 );
    }

    bool mark_pages_bad( std::uint64_t pa, std::uint64_t size ) {
        static auto addr = get_export( kernel::m_ntoskrnl_base, obf( "MmMarkPhysicalMemoryAsBad" ) );
        if ( !addr )
            return false;

        std::uint64_t base = pa;
        std::uint64_t bytes = size;

        using mm_mark_physical_memory_as_bad_t = std::int32_t( __fastcall* )( std::uint64_t*, std::uint64_t* );
        auto result = reinterpret_cast< mm_mark_physical_memory_as_bad_t >( addr )( &base, &bytes );
        return !result;
    }

    runtime_function_t* rtl_lookup_function_entry(
        std::uint64_t control_pc,
        std::uint64_t* image_base,
        void* history_table ) {

        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "RtlLookupFunctionEntry" ) );
            if ( !fn_address ) return nullptr;
        }

        using function_t = runtime_function_t * ( * )( std::uint64_t, std::uint64_t*, void* );
        return reinterpret_cast< function_t >( fn_address )( control_pc, image_base, history_table );
    }

    void* ps_get_current_process( ) {
        static auto ps_get_current_process = 0ull;
        if ( !ps_get_current_process ) {
            ps_get_current_process = get_export( m_ntoskrnl_base, obf( "PsGetCurrentProcess" ) );
            if ( !ps_get_current_process ) return {};
        }

        using function_t = void* ( );
        return reinterpret_cast< function_t* >( ps_get_current_process )( );
    }

    void ke_initialize_event( kevent_t* event, std::int32_t type, std::uint8_t state ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "KeInitializeEvent" ) );
            if ( !fn_address ) return;
        }

        using function_t = void( kevent_t*, std::int32_t, std::uint8_t );
        reinterpret_cast< function_t* >( fn_address )( event, type, state );
    }

    std::int32_t ke_set_event( kevent_t* event, std::int32_t increment, std::uint8_t wait ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "KeSetEvent" ) );
            if ( !fn_address ) return 0;
        }

        using function_t = std::int32_t( kevent_t*, std::int32_t, std::uint8_t );
        return reinterpret_cast< function_t* >( fn_address )( event, increment, wait );
    }

    void ke_clear_event( kevent_t* event ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "KeResetEvent" ) );
            if ( !fn_address )
                fn_address = get_export( m_ntoskrnl_base, obf( "KeClearEvent" ) );
            if ( !fn_address ) return;
        }

        using function_t = std::int32_t( kevent_t* );
        reinterpret_cast< function_t* >( fn_address )( event );
    }

    // KeWaitForSingleObject(Object, WaitReason, WaitMode, Alertable, Timeout). Timeout is
    // a PLARGE_INTEGER - a pointer, in 100ns units, negative meaning relative. Passing a
    // bare integer dereferences that integer as an address. This wrapper takes
    // milliseconds and builds the LARGE_INTEGER; -1 means wait forever.
    nt_status_t ke_wait_for_single_object(
        kevent_t*    event,
        std::int32_t wait_reason,
        std::uint8_t wait_mode,
        std::uint8_t alertable,
        std::int64_t timeout_ms )
    {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "KeWaitForSingleObject" ) );
            if ( !fn_address )
                return static_cast< nt_status_t >( 0xC0000001 );
        }

        std::int64_t relative = 0;
        void*        timeout  = nullptr;
        if ( timeout_ms >= 0 ) {
            relative = -timeout_ms * 10000;
            timeout  = &relative;
        }

        using function_t = std::int32_t( void*, std::int32_t, std::uint8_t, std::uint8_t, void* );
        return static_cast< nt_status_t >(
            reinterpret_cast< function_t* >( fn_address )(
                event, wait_reason, wait_mode, alertable, timeout ) );
    }

    std::uint32_t* get_ki_cpu_tracing_flags( ) {
        static auto address = 0ull;
        if ( !address ) {
            address = pdb::get_symbol( obf( "KiCpuTracingFlags" ) );
            if ( !address )
                address = get_export( m_ntoskrnl_base, obf( "KiCpuTracingFlags" ) );
        }

        return reinterpret_cast< std::uint32_t* >( address );
    }

    void* io_get_initial_stack( ) {
        static auto ps_get_current_process = 0ull;
        if ( !ps_get_current_process ) {
            ps_get_current_process = get_export( m_ntoskrnl_base, obf( "IoGetInitialStack" ) );
            if ( !ps_get_current_process ) return {};
        }

        using function_t = void* ( );
        return reinterpret_cast< function_t* >( ps_get_current_process )( );
    }

    void* ps_get_current_thread( ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "PsGetCurrentThread" ) );
            if ( !fn_address ) return {};
        }

        using function_t = void* ( );
        return reinterpret_cast< function_t* >( fn_address )( );
    }

    std::uint32_t get_image_file_name_offset( ) {
        static std::uint32_t image_file_name_offset = 0;
        if ( !image_file_name_offset ) {
            auto ps_get_process_image_file_name = reinterpret_cast< std::uint8_t* >(
                kernel::get_export( m_ntoskrnl_base, obf( "PsGetProcessImageFileName" ) )
                );

            if ( !ps_get_process_image_file_name ) {
                return 0;
            }

            while ( !( ps_get_process_image_file_name[ 0 ] == 0x48 &&
                ps_get_process_image_file_name[ 1 ] == 0x8D &&
                ps_get_process_image_file_name[ 2 ] == 0x81 ) ) {
                ps_get_process_image_file_name++;
            }

            image_file_name_offset = *reinterpret_cast< std::uint32_t* >( ps_get_process_image_file_name + 3 );
        }

        return image_file_name_offset;
    }

    std::uint32_t get_thread_start_address_offset( ) {
        static std::uint32_t thread_start_address_offset = 0;
        if ( thread_start_address_offset )
            return thread_start_address_offset;

        auto ps_query_thread_start_address = reinterpret_cast< std::uint8_t* >(
            get_export( m_ntoskrnl_base, obf( "PsQueryThreadStartAddress" ) ) );

        if ( !ps_query_thread_start_address )
            ps_query_thread_start_address = reinterpret_cast< std::uint8_t* >(
                pdb::get_symbol( obf( "PsQueryThreadStartAddress" ) ) );

        if ( !ps_query_thread_start_address )
            return 0;

        for ( std::size_t i = 0; i < 0x40; ++i ) {
            if ( ps_query_thread_start_address[ i ] != 0x48 ||
                ps_query_thread_start_address[ i + 1 ] != 0x8B ||
                ps_query_thread_start_address[ i + 2 ] != 0x81 )
                continue;

            thread_start_address_offset = *reinterpret_cast< const std::uint32_t* >(
                ps_query_thread_start_address + i + 3 );
            break;
        }

        return thread_start_address_offset;
    }

    PEXCEPTION_ROUTINE rtl_virtual_unwind(
        std::uint32_t handler_type,
        std::uint64_t image_base,
        std::uint64_t control_pc,
        runtime_function_t* function_entry,
        PCONTEXT context_record,
        void** handler_data,
        std::uint64_t* establisher_frame,
        void* context_pointers ) {

        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "RtlVirtualUnwind" ) );
            if ( !fn_address ) return nullptr;
        }

        using function_t = PEXCEPTION_ROUTINE( * )(
            std::uint32_t,
            std::uint64_t,
            std::uint64_t,
            runtime_function_t*,
            PCONTEXT,
            void**,
            std::uint64_t*,
            void* );

        return reinterpret_cast< function_t >( fn_address )(
            handler_type,
            image_base,
            control_pc,
            function_entry,
            context_record,
            handler_data,
            establisher_frame,
            context_pointers );
    }

    void rtl_capture_context( PCONTEXT context_record ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "RtlCaptureContext" ) );
            if ( !fn_address ) return;
        }

        using function_t = void( * )( PCONTEXT );
        reinterpret_cast< function_t >( fn_address )( context_record );
    }

    void* rtl_pc_to_file_header( void* pc, void** image_base ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "RtlPcToFileHeader" ) );
            if ( !fn_address ) return nullptr;
        }

        using function_t = void* ( * )( void*, void** );
        return reinterpret_cast< function_t >( fn_address )( pc, image_base );
    }

    std::uint64_t get_pte_base( ) {
        static std::uint64_t cached = 0;
        if ( cached ) return cached;

        const auto scan_for_pte_base = [ ]( const std::uint8_t* p, std::size_t len ) -> std::uint64_t {
            for ( std::size_t i = 0; i + 10 < len; ++i ) {
                if ( ( p[ i ] != 0x48 && p[ i ] != 0x49 ) ||
                     p[ i + 1 ] < 0xB8 || p[ i + 1 ] > 0xBF )
                    continue;
                const auto val = *reinterpret_cast< const std::uint64_t* >( p + i + 2 );
                if ( ( val >> 48 ) == 0xFFFF )
                    return val;
            }
            return 0;
        };

        if ( const auto fn = pdb::get_symbol( obf( "MiGetPteAddress" ) ) ) {
            if ( const auto val = scan_for_pte_base(
                reinterpret_cast< const std::uint8_t* >( fn ), 32 ) ) {
                cached = val;
                return cached;
            }
        }

        static constexpr std::uint8_t k_mask_pattern[] = {
            0x48, 0xB8, 0xF8, 0xFF, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00,
            0x48, 0x23, 0xC8,
            0x48, 0xB8
        };

        if ( m_ntoskrnl_base && m_ntoskrnl_size ) {
            const auto* scan = reinterpret_cast< const std::uint8_t* >( m_ntoskrnl_base );
            for ( std::size_t i = 0; i + sizeof( k_mask_pattern ) + 8 <= m_ntoskrnl_size; ++i ) {
                if ( std::memcmp( scan + i, k_mask_pattern, sizeof( k_mask_pattern ) ) != 0 )
                    continue;
                const auto val = *reinterpret_cast< const std::uint64_t* >(
                    scan + i + sizeof( k_mask_pattern ) );
                if ( ( val >> 48 ) == 0xFFFF ) {
                    cached = val;
                    return cached;
                }
            }
        }

        return 0;
    }

    void rtl_restore_context( PCONTEXT context, PEXCEPTION_RECORD exception_record ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "RtlRestoreContext" ) );
            if ( !fn_address ) return;
        }

        using function_t = void( * )( PCONTEXT, PEXCEPTION_RECORD );
        reinterpret_cast< function_t >( fn_address )( context, exception_record );
    }

    std::uint32_t rand( ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "RtlRandomEx" ) );
            if ( !fn_address ) return {};
        }

        unsigned int low = *reinterpret_cast< unsigned int* >( 0xFFFFF78000000000 );
        unsigned int mul = *reinterpret_cast< unsigned int* >( 0xFFFFF78000000004 );
        auto seed = ( ( std::uint64_t )( low ) * ( uint64_t )( mul ) ) >> 24;

        using function_t = std::uint32_t( unsigned long* );
        return reinterpret_cast< function_t* >( fn_address )( ( unsigned long* )&seed );
    }

    NTSTATUS zw_continue( PCONTEXT context, BOOLEAN test_alert ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "ZwContinue" ) );
            if ( !fn_address ) return STATUS_NOT_FOUND;
        }

        using function_t = NTSTATUS( * )( PCONTEXT, BOOLEAN );
        return reinterpret_cast< function_t >( fn_address )( context, test_alert );
    }

    hal_private_dispatch_t* get_hal_private_dispatch( ) {
        static hal_private_dispatch_t* hal_private_dispatch{ };
        if ( !hal_private_dispatch ) {
            hal_private_dispatch = reinterpret_cast
                < hal_private_dispatch_t* >( get_export( m_ntoskrnl_base, obf( "HalPrivateDispatchTable" ) ) );
            if ( !hal_private_dispatch )
                return { };
        }

        return hal_private_dispatch;
    }

    kpcr_t* ke_get_pcr( ) {
        return reinterpret_cast< kpcr_t* >( KeGetPcr( ) );
    }

    kprcb_t* ke_get_current_prcb( ) {
        return reinterpret_cast< kprcb_t* >(
            __readgsqword( 0x20 ) );
    }

    std::uint32_t ke_query_active_processor_count( ) {
        const auto ke_query_active_processor_count_ex = pdb::get_symbol( obf("KeQueryActiveProcessorCountEx") );
        if ( !ke_query_active_processor_count_ex ) return {};

        using function_t = std::uint32_t( std::uint32_t );
        return reinterpret_cast< function_t* >( ke_query_active_processor_count_ex )( 0xFFFF );
    }

    void ke_set_system_affinity_thread( std::uint64_t affinity ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "KeSetSystemAffinityThread" ) );
            if ( !fn_address ) return;
        }

        using function_t = void( std::uint64_t );
        reinterpret_cast< function_t* >( fn_address )( affinity );
    }

    bool ke_get_processor_number_from_index( std::uint32_t index, processor_number_t* processor_number ) {
        static auto export_address = 0ull;
        if ( !export_address ) {
            export_address = get_export( m_ntoskrnl_base, obf( "KeGetProcessorNumberFromIndex" ) );
            if ( !export_address ) return false;
        }

        using function_t = std::int32_t( * )( std::uint32_t, processor_number_t* );
        return reinterpret_cast< function_t >( export_address )( index, processor_number ) >= 0;
    }

    void ps_terminate_system_thread( std::int32_t exit_status ) {
        static auto export_address = 0ull;
        if ( !export_address ) {
            export_address = get_export( m_ntoskrnl_base, obf( "PsTerminateSystemThread" ) );
            if ( !export_address ) return;
        }
        using function_t = void( * )( std::int32_t );
        reinterpret_cast< function_t >( export_address )( exit_status );
    }

    void ke_set_system_group_affinity_thread( group_affinity_t* affinity, group_affinity_t* old_affinity ) {
        static auto export_address = 0ull;
        if ( !export_address ) {
            export_address = get_export( m_ntoskrnl_base, obf( "KeSetSystemGroupAffinityThread" ) );
            if ( !export_address ) return;
        }

        using function_t = void( * )( group_affinity_t*, group_affinity_t* );
        reinterpret_cast< function_t >( export_address )( affinity, old_affinity );
    }

    void ke_revert_to_user_group_affinity_thread( group_affinity_t* old_affinity ) {
        static auto export_address = 0ull;
        if ( !export_address ) {
            export_address = get_export( m_ntoskrnl_base, obf( "KeRevertToUserGroupAffinityThread" ) );
            if ( !export_address ) return;
        }

        using function_t = void( * )( group_affinity_t* );
        reinterpret_cast< function_t >( export_address )( old_affinity );
    }

    nt_status_t rtl_convert_sid_to_unicode_string(
        unicode_string_t* unicode_string,
        sid_t* sid,
        bool allocate_destination_string
    ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "RtlConvertSidToUnicodeString" ) );
            if ( !fn_address ) return nt_status_t::unsuccessful;
        }

        using function_t = nt_status_t( * )( unicode_string_t*, sid_t*, bool );
        return reinterpret_cast< function_t >( fn_address )( unicode_string, sid, allocate_destination_string );
    }

    void rtl_free_unicode_string( unicode_string_t* unicode_string ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "RtlFreeUnicodeString" ) );
            if ( !fn_address ) return;
        }

        using function_t = void( * )( unicode_string_t* );
        reinterpret_cast< function_t >( fn_address )( unicode_string );
    }

    unsigned long rtl_length_sid( void* sid ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "RtlLengthSid" ) );
            if ( !fn_address ) return 0;
        }

        using function_t = unsigned long( * )( void* );
        return reinterpret_cast< function_t >( fn_address )( sid );
    }

    nt_status_t nt_query_information_token(
        void* token_handle,
        std::uint32_t token_information_class,
        void* token_information,
        std::uint32_t token_information_length,
        unsigned long* return_length
    ) {
        static std::uint64_t fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "NtQueryInformationToken" ) );
            if ( !fn_address ) return nt_status_t::unsuccessful;
        }

        using function_t = nt_status_t( * )(
            void*, std::uint32_t, void*, std::uint32_t, unsigned long* );
        return reinterpret_cast< function_t >( fn_address )(
            token_handle,
            token_information_class,
            token_information,
            token_information_length,
            return_length
            );
    }

    nt_status_t ke_delay_execution_thread(
        std::uint8_t wait_mode,
        bool alertable,
        std::int64_t* interval
    ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "KeDelayExecutionThread" ) );
            if ( !fn_address ) return nt_status_t::unsuccessful;
        }

        using function_t = nt_status_t( * )( std::uint8_t, bool, std::int64_t* );
        return reinterpret_cast< function_t >( fn_address )( wait_mode, alertable, interval );
    }

    nt_status_t ps_create_system_thread(
        void** thread_handle,
        std::uint32_t desired_access,
        object_attributes_t* object_attributes,
        void* process_handle,
        void* client_id,
        void* start_routine,
        void* start_context
    ) {
        static auto fn_address = 0ull;
        if ( !fn_address ) {
            fn_address = get_export( m_ntoskrnl_base, obf( "PsCreateSystemThread" ) );
            if ( !fn_address ) return nt_status_t::unsuccessful;
        }

        using function_t = nt_status_t( * )(
            void**, std::uint32_t, object_attributes_t*, void*, void*, void*, void* );
        return reinterpret_cast< function_t >( fn_address )(
            thread_handle, desired_access, object_attributes, process_handle,
            client_id, start_routine, start_context );
    }

}
