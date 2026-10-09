#pragma once

namespace hooks {
    namespace km {
        
        void* ( *ex_allocate_pool_with_tag_org )( pool_type_t, std::size_t, unsigned long ) = nullptr;
        void* ex_allocate_pool_with_tag( pool_type_t pool_type, std::size_t number_of_bytes, unsigned long tag ) {
			if ( image::contains( _ReturnAddress( ) ) )
            kernel::dbg_print( obf( "ex_allocate_pool_with_tag (bytes: 0x%llx)" ), number_of_bytes );
            return ex_allocate_pool_with_tag_org( pool_type, number_of_bytes, tag );
        }

        nt_status_t( *mm_copy_memory_org )( void*, mm_copy_address_t, std::size_t, std::uint32_t, std::size_t* ) = nullptr;
        nt_status_t mm_copy_memory( void* target, mm_copy_address_t source, std::size_t number_of_bytes, std::uint32_t flags, std::size_t* bytes_transferred ) {
            if ( image::contains( _ReturnAddress( ) ) )
                return nt_status_t::unsuccessful;
            return mm_copy_memory_org( target, source, number_of_bytes, flags, bytes_transferred );
        }

        void* ( *mm_map_io_space_org )( std::uint64_t, std::size_t, std::uint32_t ) = nullptr;
        void* mm_map_io_space( std::uint64_t physical_address, std::size_t number_of_bytes, std::uint32_t cache_type ) {
            if ( image::contains( _ReturnAddress( ) ) )
                return nullptr;
            return mm_map_io_space_org( physical_address, number_of_bytes, cache_type );
        }

        void* ( *mm_map_io_space_ex_org )( std::uint64_t, std::size_t, std::uint32_t ) = nullptr;
        void* mm_map_io_space_ex( std::uint64_t physical_address, std::size_t number_of_bytes, std::uint32_t protect ) {
            if ( image::contains( _ReturnAddress( ) ) )
                return nullptr;
            return mm_map_io_space_ex_org( physical_address, number_of_bytes, protect );
        }
        
        nt_status_t( *nt_write_file_org )( HANDLE, HANDLE, void*, void*, IO_STATUS_BLOCK*, void*, std::uint32_t, LARGE_INTEGER*, std::uint32_t* ) = nullptr;
        nt_status_t nt_write_file( HANDLE file_handle, HANDLE event, void* apc_routine, void* apc_context, IO_STATUS_BLOCK* io_status_block, void* buffer, std::uint32_t length, LARGE_INTEGER* byte_offset, std::uint32_t* key ) {
            return nt_write_file_org( file_handle, event, apc_routine, apc_context, io_status_block, buffer, length, byte_offset, key );
        }

        void* ( *rtl_lookup_function_entry_org )( std::uint64_t, std::uint64_t*, void* ) = nullptr;
        void* rtl_lookup_function_entry( std::uint64_t control_pc, std::uint64_t* image_base, void* history_table ) {
            if ( image::contains( _ReturnAddress( ) ) )
                return nullptr;
            return rtl_lookup_function_entry_org( control_pc, image_base, history_table );
        }

        std::uint16_t( *rtl_walk_frame_chain_org )( void**, std::uint16_t, std::uint32_t ) = nullptr;
        std::uint16_t rtl_walk_frame_chain( void** callers, std::uint16_t count, std::uint32_t flags ) {
            if ( image::contains( _ReturnAddress( ) ) )
                return 0;
            return rtl_walk_frame_chain_org( callers, count, flags );
        }

        nt_status_t( *nt_query_directory_file_org )( HANDLE, HANDLE, void*, void*, IO_STATUS_BLOCK*, void*, std::uint32_t, std::uint32_t, BOOLEAN, UNICODE_STRING*, BOOLEAN ) = nullptr;
        nt_status_t nt_query_directory_file( HANDLE file_handle, HANDLE event, void* apc_routine, void* apc_context, IO_STATUS_BLOCK* io_status_block, void* file_information, std::uint32_t length, std::uint32_t file_information_class, BOOLEAN return_single_entry, UNICODE_STRING* file_name, BOOLEAN restart_scan ) {
            if ( image::contains( _ReturnAddress( ) ) )
                return nt_status_t::unsuccessful;
            return nt_query_directory_file_org( file_handle, event, apc_routine, apc_context, io_status_block, file_information, length, file_information_class, return_single_entry, file_name, restart_scan );
        }

        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        static bool km_unicode_contains(
            UNICODE_STRING* string, const wchar_t* needle )
        {
            if ( !string || !string->Buffer || !string->Length || !needle )
                return false;
            auto needle_length = wcslen( needle );
            auto string_length = string->Length / sizeof( wchar_t );
            if ( !needle_length || string_length < needle_length )
                return false;
            for ( std::size_t i = 0; i <= string_length - needle_length; ++i ) {
                bool matched = true;
                for ( std::size_t j = 0; j < needle_length; ++j ) {
                    auto a = string->Buffer[ i + j ];
                    auto b = needle[ j ];
                    if ( a >= L'A' && a <= L'Z' ) a += L'a' - L'A';
                    if ( b >= L'A' && b <= L'Z' ) b += L'a' - L'A';
                    if ( a != b ) { matched = false; break; }
                }
                if ( matched ) return true;
            }
            return false;
        }

        static bool km_is_sensitive_efi_name( UNICODE_STRING* name ) {
            return km_unicode_contains( name, L"UnlockID" ) ||
                   km_unicode_contains( name, L"OfflineUniqueID" ) ||
                   km_unicode_contains( name, L"PlatformModule" ) ||
                   km_unicode_contains( name, L"Endorsement" ) ||
                   km_unicode_contains( name, L"TPM" ) ||
                   km_unicode_contains( name, L"BootIdentifier" ) ||
                   km_unicode_contains( name, L"UUID" ) ||
                   km_unicode_contains( name, L"Guid" ) ||
                   km_unicode_contains( name, L"Serial" ) ||
                   km_unicode_contains( name, L"Boot" );
        }

        nt_status_t( *ex_get_firmware_environment_variable_org )(
            UNICODE_STRING*, GUID*, void*, unsigned long*, unsigned long* ) = nullptr;
        nt_status_t ex_get_firmware_environment_variable(
            UNICODE_STRING* variable_name,
            GUID*           vendor_guid,
            void*           value,
            unsigned long*  value_length,
            unsigned long*  attributes )
        {
            auto status = ex_get_firmware_environment_variable_org(
                variable_name, vendor_guid, value, value_length, attributes );

            if ( image::contains( _ReturnAddress( ) ) &&
                 km_is_sensitive_efi_name( variable_name ) &&
                 NT_SUCCESS( status ) && value && value_length &&
                 kernel::mm_is_address_valid( value ) &&
                 kernel::mm_is_address_valid( value_length ) ) {
                __try {
                    hwid::fill_consistent( value, *value_length );
                    kernel::dbg_print( obf( "[km.efi] spoofed %wZ (%lu bytes)\n" ),
                        variable_name, *value_length );
                }
                __except ( EXCEPTION_EXECUTE_HANDLER ) { }
            }

            return status;
        }

		NTSTATUS( *ex_uuid_create_org )( GUID* ) = nullptr;
		NTSTATUS ex_uuid_create( GUID* uuid ) {
			const auto status = ex_uuid_create_org( uuid );
			if ( !image::contains( _ReturnAddress( ) ) || !uuid )
				return status;

			auto* b = reinterpret_cast< std::uint8_t* >( uuid );
			for ( auto i = 0; i < static_cast< int >( sizeof( GUID ) ); ++i )
				b[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );

			b[ 6 ] = ( b[ 6 ] & 0x0F ) | 0x40;
			b[ 8 ] = ( b[ 8 ] & 0x3F ) | 0x80;

			kernel::dbg_print( obf( "spoofed uuid -> %08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X\n" ),
				uuid->Data1,
				uuid->Data2,
				uuid->Data3,
				uuid->Data4[ 0 ], uuid->Data4[ 1 ],
				uuid->Data4[ 2 ], uuid->Data4[ 3 ],
				uuid->Data4[ 4 ], uuid->Data4[ 5 ],
				uuid->Data4[ 6 ], uuid->Data4[ 7 ] );

			return status;
		}

        struct wmi_block_entry_t {
            void* block_ptr;
            GUID  guid;
        };

        constexpr unsigned long wmi_block_table_max = 128u;
        static wmi_block_entry_t s_wmi_block_table[ wmi_block_table_max ];
        static long              s_wmi_block_count = 0;

        static bool wmi_guid_equal( const GUID* a, const GUID* b ) {
            if ( !a || !b ) return false;
            if ( a->Data1 != b->Data1 ) return false;
            if ( a->Data2 != b->Data2 ) return false;
            if ( a->Data3 != b->Data3 ) return false;
            for ( int i = 0; i < 8; ++i )
                if ( a->Data4[ i ] != b->Data4[ i ] )
                    return false;
            return true;
        }

        static const GUID s_wmi_sensitive_guids[] = {
            { 0x78A1C341, 0x4539, 0x11D3,
              { 0xB8, 0x8D, 0x00, 0xC0, 0x4F, 0xAD, 0x51, 0x71 } },
            { 0x376B72C6, 0xA5DE, 0x4857,
              { 0xBC, 0x8B, 0x79, 0xB2, 0x2D, 0x69, 0xEF, 0x15 } },
            { 0xDFA22B8E, 0xE68D, 0x4DFF,
              { 0x9B, 0x49, 0x15, 0xE8, 0xAF, 0x95, 0x3D, 0x80 } },
            { 0x9D7BE1BA, 0x3F13, 0x4A94,
              { 0xBA, 0x5D, 0xA0, 0xB4, 0xC6, 0xCD, 0xE9, 0xBA } },
            { 0x4EA82135, 0xC1A6, 0x4D2C,
              { 0x9B, 0x84, 0x1D, 0x5C, 0x4D, 0x7C, 0x0A, 0xC5 } },
            { 0x5326DF60, 0xD2CA, 0x11D0,
              { 0x9E, 0xC2, 0x00, 0xC0, 0x4F, 0xB9, 0xAA, 0x46 } },
            { 0x671A8285, 0x4EDB, 0x4CAE,
              { 0x99, 0xFE, 0x69, 0xA1, 0x5C, 0x48, 0xC0, 0xBC } },
            { 0x8F680850, 0xA584, 0x11D1,
              { 0xBF, 0x38, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0x10 } },
        };

        static bool wmi_is_sensitive_guid( const GUID* guid ) {
            if ( !guid ) return false;
            for ( const auto& g : s_wmi_sensitive_guids )
                if ( wmi_guid_equal( guid, &g ) )
                    return true;
            return false;
        }

        static void wmi_track_block( void* block_ptr, const GUID* guid ) {
            if ( !block_ptr || !guid ) return;
            long cnt = s_wmi_block_count;
            for ( long i = 0; i < cnt; ++i )
                if ( s_wmi_block_table[ i ].block_ptr == block_ptr )
                    return;
            long idx = s_wmi_block_count;
            if ( idx >= static_cast< long >( wmi_block_table_max ) )
                return;
            s_wmi_block_table[ idx ].block_ptr = block_ptr;
            s_wmi_block_table[ idx ].guid      = *guid;
            s_wmi_block_count = idx + 1;
        }

        static const GUID* wmi_lookup_block( void* block_ptr ) {
            long cnt = s_wmi_block_count;
            for ( long i = 0; i < cnt; ++i )
                if ( s_wmi_block_table[ i ].block_ptr == block_ptr )
                    return &s_wmi_block_table[ i ].guid;
            return nullptr;
        }

		nt_status_t( *io_wmi_open_block_org )( GUID*, unsigned long, void** ) = nullptr;
		nt_status_t io_wmi_open_block(
			GUID* data_block_guid,
			unsigned long desired_access,
			void** data_block_object )
		{
			if ( image::contains( _ReturnAddress( ) ) ) {
				__try {
					if ( data_block_guid && kernel::mm_is_address_valid( data_block_guid ) &&
						wmi_is_sensitive_guid( data_block_guid ) ) {
						kernel::dbg_print( obf( "blocked open block -> %08X-%04X-%04X\n" ),
							data_block_guid->Data1,
							data_block_guid->Data2,
							data_block_guid->Data3 );
						if ( data_block_object && kernel::mm_is_address_valid( data_block_object ) )
							*data_block_object = nullptr;
						return nt_status_t::unsuccessful;
					}
				}
				__except ( EXCEPTION_EXECUTE_HANDLER ) { }
			}

			auto status = io_wmi_open_block_org( data_block_guid, desired_access, data_block_object );

			if ( NT_SUCCESS( status ) ) {
				__try {
					if ( data_block_guid && kernel::mm_is_address_valid( data_block_guid ) &&
						data_block_object && kernel::mm_is_address_valid( data_block_object ) &&
						*data_block_object )
						wmi_track_block( *data_block_object, data_block_guid );
				}
				__except ( EXCEPTION_EXECUTE_HANDLER ) { }
			}

			return status;
		}

		nt_status_t( *io_wmi_query_all_data_org )( void*, unsigned long*, void* ) = nullptr;
		nt_status_t io_wmi_query_all_data(
			void* data_block_object,
			unsigned long* in_out_buffer_size,
			void* out_buffer )
		{
			if ( image::contains( _ReturnAddress( ) ) ) {
				const GUID* tracked_guid = wmi_lookup_block( data_block_object );
				if ( tracked_guid && wmi_is_sensitive_guid( tracked_guid ) ) {
					kernel::dbg_print( obf( "blocked query data -> %08X-%04X-%04X\n" ),
						tracked_guid->Data1,
						tracked_guid->Data2,
						tracked_guid->Data3 );

					if ( in_out_buffer_size && kernel::mm_is_address_valid( in_out_buffer_size ) )
						*in_out_buffer_size = 0;
					return nt_status_t::unsuccessful;
				}
			}

			return io_wmi_query_all_data_org( data_block_object, in_out_buffer_size, out_buffer );
		}
        
        nt_status_t( *nt_device_io_control_file_org )( void*, void*, void*, void*, IO_STATUS_BLOCK*, unsigned long, void*, unsigned long, void*, unsigned long ) = nullptr;
        nt_status_t nt_device_io_control_file(
            void* file_handle,
            void* event,
            void* apc_routine,
            void* apc_context,
            IO_STATUS_BLOCK* io_status_block,
            unsigned long io_control_code,
            void* input_buffer,
            unsigned long input_buffer_length,
            void* output_buffer,
            unsigned long output_buffer_length
        ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                const auto caller_ra = reinterpret_cast< std::uint64_t >( _ReturnAddress( ) );

                void* file_object = nullptr;
                auto ref_status = kernel::ob_reference_object_by_handle(
                    file_handle, 0, *kernel::io_file_object_type, 0, &file_object, nullptr );

                if ( NT_SUCCESS( ref_status ) && file_object ) {
                    __try {
                        auto file_obj = reinterpret_cast< FILE_OBJECT* >( file_object );
                        auto device_object = file_obj->DeviceObject;

                        if ( device_object && kernel::mm_is_address_valid( device_object ) ) {
                            unsigned long return_length = 0;
                            kernel::ob_query_name_string( device_object, nullptr, 0, &return_length );

                            if ( return_length ) {
                                auto device_name = reinterpret_cast< object_name_information_t* >(
                                    kernel::mm_allocate_independent_pages( return_length ) );

                                if ( device_name ) {
                                    auto qs = kernel::ob_query_name_string(
                                        device_object, device_name, return_length, &return_length );

                                    if ( NT_SUCCESS( qs ) &&
                                        device_name->name.m_buffer &&
                                        device_name->name.m_length ) {
                                        disk::print(
                                            obf( "[km.ioctl] device=%wZ code=0x%08X in=%lu out=%lu ra=0x%llx" ),
                                            &device_name->name,
                                            io_control_code,
                                            input_buffer_length,
                                            output_buffer_length,
                                            caller_ra );
                                    }
                                    else {
                                        
                                        disk::print(
                                            obf( "[km.ioctl] anon device=0x%llx code=0x%08X in=%lu out=%lu ra=0x%llx" ),
                                            reinterpret_cast< std::uint64_t >( device_object ),
                                            io_control_code,
                                            input_buffer_length,
                                            output_buffer_length,
                                            caller_ra );
                                    }

                                    kernel::mm_free_independent_pages(
                                        reinterpret_cast< std::uint64_t >( device_name ), return_length );
                                }
                            }
                            else {
                                
                                disk::print(
                                    obf( "[km.ioctl] noname device=0x%llx code=0x%08X in=%lu out=%lu ra=0x%llx" ),
                                    reinterpret_cast< std::uint64_t >( device_object ),
                                    io_control_code,
                                    input_buffer_length,
                                    output_buffer_length,
                                    caller_ra );
                            }
                        }
                    }
                    __except ( EXCEPTION_EXECUTE_HANDLER ) {
                        disk::print(
                            obf( "[km.ioctl] fault resolving handle=0x%llx code=0x%08X ra=0x%llx" ),
                            reinterpret_cast< std::uint64_t >( file_handle ),
                            io_control_code,
                            caller_ra );
                    }

                    kernel::ob_dereference_object( file_object );
                }
                else {
                    
                    
                    
                    disk::print(
                        obf( "[km.ioctl] unref handle=0x%llx code=0x%08X in=%lu out=%lu ra=0x%llx status=0x%08x" ),
                        reinterpret_cast< std::uint64_t >( file_handle ),
                        io_control_code,
                        input_buffer_length,
                        output_buffer_length,
                        caller_ra,
                        static_cast< unsigned >( ref_status ) );
                }
            }

            
            
            
            
            
            switch ( io_control_code ) {
                case 0x006D0004u:  
                case 0x006D0030u:  
                case 0x006D0034u:  
                    if ( image::contains( _ReturnAddress( ) ) ) {
                        disk::print( obf( "[km.ioctl.block] MOUNTMGR code=0x%08X" ),
                            io_control_code );
                        return static_cast< nt_status_t >( 0xC0000225 ); 
                    }
                    break;
            }

            
            if ( ( io_control_code == 0x004D0000u || io_control_code == 0x004D0018u ) &&
                 image::contains( _ReturnAddress( ) ) ) {
                auto status = nt_device_io_control_file_org(
                    file_handle, event, apc_routine, apc_context, io_status_block,
                    io_control_code, input_buffer, input_buffer_length,
                    output_buffer, output_buffer_length );
                if ( NT_SUCCESS( status ) && output_buffer &&
                     kernel::mm_is_address_valid( output_buffer ) ) {
                    __try {
                        if ( io_control_code == 0x004D0000u &&
                             output_buffer_length >= 2 ) {
                            auto* resp = static_cast< std::uint8_t* >( output_buffer );
                            std::uint16_t len = *reinterpret_cast< std::uint16_t* >( resp );
                            if ( static_cast< unsigned long >( 2u + len ) <= output_buffer_length ) {
                                for ( std::uint16_t i = 0; i < len; ++i )
                                    resp[ 2u + i ] = static_cast< std::uint8_t >( kernel::rand( ) );
                            }
                        } else if ( io_control_code == 0x004D0018u &&
                                    output_buffer_length >= 16 ) {
                            auto* resp = static_cast< std::uint8_t* >( output_buffer );
                            for ( int i = 0; i < 16; ++i )
                                resp[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );
                            resp[ 6 ] = ( resp[ 6 ] & 0x0F ) | 0x40;
                            resp[ 8 ] = ( resp[ 8 ] & 0x3F ) | 0x80;
                        }
                    }
                    __except ( EXCEPTION_EXECUTE_HANDLER ) { }
                }
                return status;
            }

            
            if ( io_control_code == 0x00170002u &&
                 image::contains( _ReturnAddress( ) ) &&
                 input_buffer && input_buffer_length >= sizeof( std::uint32_t ) &&
                 kernel::mm_is_address_valid( input_buffer ) ) {
                std::uint32_t oid = 0;
                __try { oid = *reinterpret_cast< std::uint32_t* >( input_buffer ); }
                __except ( EXCEPTION_EXECUTE_HANDLER ) { oid = 0; }

                if ( oid == 0x01010101u || oid == 0x01010102u ) {
                    auto status = nt_device_io_control_file_org(
                        file_handle, event, apc_routine, apc_context, io_status_block,
                        io_control_code, input_buffer, input_buffer_length,
                        output_buffer, output_buffer_length );
                    if ( NT_SUCCESS( status ) && output_buffer &&
                         output_buffer_length >= 6 &&
                         kernel::mm_is_address_valid( output_buffer ) ) {
                        auto* spoof_mac = hwid::identity( ).m_mac;
                        __try {
                            auto* mac = static_cast< std::uint8_t* >( output_buffer );
                            for ( int i = 0; i < 6; ++i )
                                mac[ i ] = spoof_mac[ i ];
                        }
                        __except ( EXCEPTION_EXECUTE_HANDLER ) { }
                    }
                    return status;
                }
            }

            
            
            
            
            
            
            
            
            
            
            
            
            
            if ( io_control_code == 0x002D1400u &&
                 image::contains( _ReturnAddress( ) ) &&
                 input_buffer && input_buffer_length >= 8 &&
                 kernel::mm_is_address_valid( input_buffer ) ) {
                std::uint32_t property_id = 0xFFFFFFFFu;
                std::uint32_t query_type  = 0xFFFFFFFFu;
                __try {
                    auto* q = reinterpret_cast< std::uint32_t* >( input_buffer );
                    property_id = q[ 0 ];
                    query_type  = q[ 1 ];
                }
                __except ( EXCEPTION_EXECUTE_HANDLER ) { }

                if ( ( property_id == 0u || property_id == 2u || property_id == 3u ) &&
                     query_type == 0u ) {
                    auto status = nt_device_io_control_file_org(
                        file_handle, event, apc_routine, apc_context, io_status_block,
                        io_control_code, input_buffer, input_buffer_length,
                        output_buffer, output_buffer_length );

                    if ( NT_SUCCESS( status ) && output_buffer &&
                         output_buffer_length >= 8 &&
                         kernel::mm_is_address_valid( output_buffer ) ) {
                        __try {
                            auto* buf = reinterpret_cast< std::uint8_t* >( output_buffer );

                            if ( property_id == 0u && output_buffer_length >= 0x1C ) {
                                
                                const std::uint32_t serial_off =
                                    *reinterpret_cast< std::uint32_t* >( buf + 0x18 );
                                if ( serial_off >= 0x24 && serial_off < output_buffer_length ) {
                                    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
                                    constexpr std::uint32_t alphabet_len = sizeof( alphabet ) - 1;
                                    for ( std::uint32_t i = serial_off;
                                        i < output_buffer_length && buf[ i ] != 0;
                                        ++i ) {
                                        buf[ i ] = alphabet[ kernel::rand( ) % alphabet_len ];
                                    }
                                }
                            }
                            else if ( property_id == 2u && output_buffer_length >= 12 ) {
                                
                                
                                const std::uint32_t num_ids =
                                    *reinterpret_cast< std::uint32_t* >( buf + 8 );
                                if ( num_ids <= 32u ) {
                                    std::uint32_t off = 12;
                                    for ( std::uint32_t k = 0; k < num_ids; ++k ) {
                                        if ( off + 12u > output_buffer_length ) break;
                                        const std::uint16_t id_size =
                                            *reinterpret_cast< std::uint16_t* >( buf + off + 2 );
                                        const std::uint16_t next_off =
                                            *reinterpret_cast< std::uint16_t* >( buf + off + 4 );
                                        if ( id_size == 0 ||
                                            static_cast< std::uint32_t >( off + 12u + id_size ) > output_buffer_length )
                                            break;
                                        for ( std::uint16_t i = 0; i < id_size; ++i )
                                            buf[ off + 12u + i ] =
                                                static_cast< std::uint8_t >( kernel::rand( ) );
                                        if ( next_off == 0 || next_off < 12 ) break;
                                        if ( static_cast< std::uint32_t >( off + next_off ) > output_buffer_length ) break;
                                        off += next_off;
                                    }
                                }
                            }
                            else if ( property_id == 3u && output_buffer_length > 20 ) {
                                for ( std::uint32_t i = 20; i < output_buffer_length; ++i )
                                    buf[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );
                            }
                        }
                        __except ( EXCEPTION_EXECUTE_HANDLER ) { }
                    }
                    return status;
                }
            }

            if ( image::contains( _ReturnAddress( ) ) &&
                 ( io_control_code == 0x0007C088u || io_control_code == 0x0007C028u ) ) {
                auto status = nt_device_io_control_file_org(
                    file_handle, event, apc_routine, apc_context, io_status_block,
                    io_control_code, input_buffer, input_buffer_length,
                    output_buffer, output_buffer_length );
                if ( NT_SUCCESS( status ) && output_buffer &&
                     output_buffer_length >= 110u &&
                     kernel::mm_is_address_valid( output_buffer ) ) {
                    __try {
                        auto* buf = static_cast< std::uint8_t* >( output_buffer );
                        for ( std::uint32_t i = 36u; i < 56u; ++i )
                            buf[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );
                        for ( std::uint32_t i = 62u; i < 70u; ++i )
                            buf[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );
                        for ( std::uint32_t i = 70u; i < 110u; ++i )
                            buf[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );
                    }
                    __except ( EXCEPTION_EXECUTE_HANDLER ) {}
                }
                return status;
            }

            if ( image::contains( _ReturnAddress( ) ) &&
                 io_control_code == 0x0004D030u )
                return nt_status_t::unsuccessful;

            return nt_device_io_control_file_org(
                file_handle, event, apc_routine, apc_context, io_status_block,
                io_control_code, input_buffer, input_buffer_length,
                output_buffer, output_buffer_length );
        }

        void* ( *mm_allocate_contiguous_node_memory_org )( std::size_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint32_t, std::uint32_t ) = nullptr;
        void* mm_allocate_contiguous_node_memory( std::size_t number_of_bytes, std::uint64_t lowest, std::uint64_t highest, std::uint64_t boundary, std::uint32_t cache_type, std::uint32_t preferred_node ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                if ( number_of_bytes == 0xe184 ) {
                    static auto pool_rebuild_intialized = false;
                    if ( pool_rebuild_intialized )
                        return nullptr;
                    pool_rebuild_intialized = true;
                }

                if ( number_of_bytes == 0x1000 ) {
                    kernel::dbg_print( "blocked page table page allocation\n" );
                    return nullptr;
                }

            }
            return mm_allocate_contiguous_node_memory_org( number_of_bytes, lowest, highest, boundary, cache_type, preferred_node );
        }

        nt_status_t( *io_get_device_interfaces_org )( const GUID*, DEVICE_OBJECT*, std::uint32_t, UNICODE_STRING* ) = nullptr;
        nt_status_t io_get_device_interfaces( const GUID* interface_class_guid, DEVICE_OBJECT* physical_device_object, std::uint32_t flags, UNICODE_STRING* symbolic_link_list ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                kernel::dbg_print( obf( "blocked hardware serials -> %08X-%04X-%04X\n" ),
                    interface_class_guid->Data1,
                    interface_class_guid->Data2,
                    interface_class_guid->Data3 );

                auto empty_list = reinterpret_cast< wchar_t* >( kernel::ex_allocate_pool_with_tag(
                    pool_type_t::non_paged_pool, sizeof( wchar_t ) * 2, 'diwH' ) );
                if ( empty_list ) {
                    empty_list[ 0 ] = L'\0';
                    empty_list[ 1 ] = L'\0';
                    if ( symbolic_link_list )
                        *reinterpret_cast< wchar_t** >( symbolic_link_list ) = empty_list;
                    return nt_status_t::success;
                }
                return nt_status_t::unsuccessful;
            }
            return io_get_device_interfaces_org( interface_class_guid, physical_device_object, flags, symbolic_link_list );
        }

        nt_status_t( *io_enumerate_device_object_list_org )( DRIVER_OBJECT*, DEVICE_OBJECT**, std::uint32_t, std::uint32_t* ) = nullptr;
        nt_status_t io_enumerate_device_object_list( DRIVER_OBJECT* driver_object, DEVICE_OBJECT** device_object_list, std::uint32_t device_object_list_size, std::uint32_t* actual_number_devices_in_list ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                kernel::dbg_print( obf( "blocked hardware serials -> %wZ\n" ),
                    &driver_object->DriverName );
                return nt_status_t::unsuccessful;
            }
            return io_enumerate_device_object_list_org( driver_object, device_object_list, device_object_list_size, actual_number_devices_in_list );
        }

        nt_status_t( *io_get_device_object_pointer_org )( UNICODE_STRING*, std::uint32_t, FILE_OBJECT**, DEVICE_OBJECT** ) = nullptr;
        nt_status_t io_get_device_object_pointer( UNICODE_STRING* object_name, std::uint32_t desired_access, FILE_OBJECT** file_object, DEVICE_OBJECT** device_object ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                kernel::dbg_print( obf( "blocked hardware serials -> %wZ\n" ),
                    object_name );
                return nt_status_t::unsuccessful;
            }
            return io_get_device_object_pointer_org( object_name, desired_access, file_object, device_object );
        }

        nt_status_t( *io_query_file_dos_device_name_org )( FILE_OBJECT*, OBJECT_NAME_INFORMATION** ) = nullptr;
        nt_status_t io_query_file_dos_device_name( FILE_OBJECT* file_object, OBJECT_NAME_INFORMATION** object_name_information ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                constexpr wchar_t fake_path[ ] = L"\\Device\\HarddiskVolume3\\Windows\\explorer.exe";
                const auto name_len = static_cast< std::uint16_t >( sizeof( fake_path ) - sizeof( wchar_t ) );
                const auto alloc_size = sizeof( OBJECT_NAME_INFORMATION ) + sizeof( fake_path );

                auto* info = reinterpret_cast< OBJECT_NAME_INFORMATION* >(
                    kernel::ex_allocate_pool_with_tag( pool_type_t::paged_pool, alloc_size, 0x6E446F49u ) );

                if ( info ) {
                    auto* buf = reinterpret_cast< wchar_t* >( info + 1 );
                    RtlCopyMemory( buf, fake_path, sizeof( fake_path ) );
                    info->Name.Buffer = buf;
                    info->Name.Length = name_len;
                    info->Name.MaximumLength = name_len + sizeof( wchar_t );
                    *object_name_information = info;
                    return io_query_file_dos_device_name_org( file_object, object_name_information );
                }
            }
            return io_query_file_dos_device_name_org( file_object, object_name_information );
        }

        std::uint64_t( *mm_get_physical_address_org )( void* ) = nullptr;
        std::uint64_t mm_get_physical_address( void* base_address ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                kernel::dbg_print( obf( "mm_get_physical_address blocked (base: 0x%llx)\n" ),
                    reinterpret_cast< std::uint64_t >( base_address ) );
                
            }
            return mm_get_physical_address_org( base_address );
        }

        void ( *mm_unmap_io_space_org )( void*, std::size_t ) = nullptr;
        void mm_unmap_io_space( void* base_address, std::size_t number_of_bytes ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                kernel::dbg_print( obf( "mm_unmap_io_space blocked (base: 0x%llx)\n" ),
                    reinterpret_cast< std::uint64_t >( base_address ) );
                return;
            }
            mm_unmap_io_space_org( base_address, number_of_bytes );
        }

        nt_status_t( *nt_query_system_information_org )( std::uint32_t, void*, std::uint32_t, std::uint32_t* ) = nullptr;
        nt_status_t nt_query_system_information( std::uint32_t system_information_class, void* system_information, std::uint32_t system_information_length, std::uint32_t* return_length ) {
            
            
            
            
            

                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
            if ( image::contains( _ReturnAddress( ) ) ) {
                auto status = nt_query_system_information_org( system_information_class, system_information, system_information_length, return_length );
                if ( NT_SUCCESS( status ) && system_information &&
                     kernel::mm_is_address_valid( system_information ) ) {
                    __try {
                        if ( system_information_class == 0x5Au && system_information_length >= 16u ) {
                            auto* boot_guid = hwid::identity( ).m_boot_guid;
                            auto* buf = static_cast< std::uint8_t* >( system_information );
                            for ( int i = 0; i < 16; ++i )
                                buf[ i ] = boot_guid[ i ];
                        }
                        else if ( system_information_class == 0x4Cu && system_information_length >= 16u ) {
                            constexpr std::uint32_t rsmb_sig = 0x52534D42u;
                            auto* base = static_cast< std::uint8_t* >( system_information );
                            const auto provider  = *reinterpret_cast< std::uint32_t* >( base + 0 );
                            const auto table_len = *reinterpret_cast< std::uint32_t* >( base + 12 );
                            if ( provider == rsmb_sig && table_len >= 8 &&
                                 16u + table_len <= system_information_length ) {
                                auto* table_buf = base + 16;
                                const auto raw_len = *reinterpret_cast< std::uint32_t* >( table_buf + 4 );
                                if ( raw_len ) {
                                    const std::uint32_t smbios_len =
                                        raw_len <= ( table_len - 8 ) ? raw_len : ( table_len - 8 );
                                    if ( smbios_len )
                                        hwid::patch_smbios( table_buf + 8, smbios_len );
                                }
                            }
                        }
                    }
                    __except ( EXCEPTION_EXECUTE_HANDLER ) {}
                }
                return status;
            }

            return nt_query_system_information_org( system_information_class, system_information, system_information_length, return_length );
        }

        nt_status_t( *nt_query_information_file_org )( HANDLE, IO_STATUS_BLOCK*, void*, std::uint32_t, std::uint32_t ) = nullptr;
        nt_status_t nt_query_information_file( HANDLE file_handle, IO_STATUS_BLOCK* io_status_block, void* file_information, std::uint32_t length, std::uint32_t file_information_class ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                void* file_object = nullptr;
                auto status = kernel::ob_reference_object_by_handle(
                    file_handle, 0, *kernel::io_file_object_type, 0, &file_object, nullptr );

                if ( NT_SUCCESS( status ) ) {
                    auto file_obj = reinterpret_cast< FILE_OBJECT* >( file_object );
                    if ( file_obj->FileName.Buffer && file_obj->FileName.Length ) {
                        auto filename = file_obj->FileName.Buffer;
                        auto filename_len = file_obj->FileName.Length / sizeof( wchar_t );

                        if ( filename_len >= 4 ) {
                            auto end_tail = &filename[ filename_len - 4 ];
                            if ( !_wcsicmp( end_tail, obf( L".sys" ) ) ) {
                                kernel::dbg_print( obf( "nt_query_information_file redirected -> %ls\n" ), filename );

                                unicode_string_t redirect_name{ };
                                kernel::rtl_init_unicode_string( &redirect_name, obf( L"\\SystemRoot\\System32\\win32k.sys" ) );

                                object_attributes_t obj_attr{ };
                                InitializeObjectAttributes(
                                    reinterpret_cast< OBJECT_ATTRIBUTES* >( &obj_attr ),
                                    reinterpret_cast< UNICODE_STRING* >( &redirect_name ),
                                    OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr );

                                HANDLE new_handle = nullptr;
                                io_status_block_t isb = { };
                                auto open_status = kernel::zw_create_file(
                                    &new_handle,
                                    FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                                    &obj_attr, &isb,
                                    nullptr,
                                    FILE_ATTRIBUTE_NORMAL,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    FILE_OPEN,
                                    FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE,
                                    nullptr, 0 );

                                kernel::ob_dereference_object( file_object );

                                if ( NT_SUCCESS( open_status ) ) {
                                    auto result = nt_query_information_file_org(
                                        new_handle, io_status_block, file_information, length, file_information_class );
                                    ZwClose( new_handle );
                                    return result;
                                }
                                return nt_status_t::unsuccessful;
                            }
                        }
                    }
                    kernel::ob_dereference_object( file_object );
                }
            }

            return nt_query_information_file_org( file_handle, io_status_block, file_information, length, file_information_class );
        }

        static void km_patch_smbios_uuid( std::uint8_t* raw, std::uint32_t raw_len ) {
            static bool s_smbios_uuid_ready = false;
            static std::uint8_t s_smbios_uuid[ 16 ] = {};
            if ( !s_smbios_uuid_ready ) {
                for ( int i = 0; i < 16; ++i )
                    s_smbios_uuid[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );
                s_smbios_uuid_ready = true;
            }
            if ( !raw || raw_len < 8 ) return;
            std::uint8_t* p = raw;
            const std::uint8_t* end = raw + raw_len;
            while ( p + 4 <= end ) {
                const std::uint8_t type   = p[ 0 ];
                const std::uint8_t length = p[ 1 ];
                if ( length < 4 || p + length > end ) break;
                if ( type == 1 && length >= 0x19 ) {
                    __try {
                        for ( int i = 0; i < 16; ++i )
                            p[ 8 + i ] = s_smbios_uuid[ i ];
                    }
                    __except ( EXCEPTION_EXECUTE_HANDLER ) {}
                }
                p += length;
                while ( p + 2 <= end && !( p[ 0 ] == 0 && p[ 1 ] == 0 ) ) ++p;
                p += 2;
            }
        }

        nt_status_t( *nt_query_value_key_org )( HANDLE, UNICODE_STRING*, std::uint32_t, void*, std::uint32_t, std::uint32_t* ) = nullptr;
        nt_status_t nt_query_value_key( HANDLE key_handle, UNICODE_STRING* value_name, std::uint32_t key_value_information_class, void* key_value_information, std::uint32_t length, std::uint32_t* result_length ) {

            if ( image::contains( _ReturnAddress( ) ) ) {
                const bool sensitive_value =
                    km_unicode_contains( value_name, L"EacQuoteKey" ) ||
                    km_unicode_contains( value_name, L"QuoteKeyStatus" ) ||
                    km_unicode_contains( value_name, L"QuoteKeyStatusCode" ) ||
                    km_unicode_contains( value_name, L"WindowsAIKHash" ) ||
                    km_unicode_contains( value_name, L"TaskReadyForAttestation" ) ||
                    km_unicode_contains( value_name, L"AttestationKey" );
                const bool smbios_value =
                    km_unicode_contains( value_name, L"SMBiosData" );

                wchar_t key_path[ registry::max_key_chars ]{ };
                if ( registry::get_key_path_from_handle( key_handle, key_path, registry::max_key_chars ) ) {
                    auto wcs_icontains = []( const wchar_t* s, const wchar_t* needle ) -> bool {
                        if ( !s || !needle ) return false;
                        auto nl = 0u;
                        while ( needle[ nl ] ) ++nl;
                        for ( auto i = 0u; s[ i ]; ++i )
                            if ( _wcsnicmp( s + i, needle, nl ) == 0 ) return true;
                        return false;
                    };
                    if ( wcs_icontains( key_path, L"UserAssist" ) )
                        return static_cast< nt_status_t >( 0xC0000034 );
                }


				auto status = nt_query_value_key_org( key_handle, value_name, key_value_information_class, key_value_information, length, result_length );

				if ( !status ) {
					if ( smbios_value &&
						 key_value_information && length >= 12u &&
						 kernel::mm_is_address_valid( key_value_information ) ) {
						__try {
							auto* info = static_cast< std::uint8_t* >( key_value_information );
							std::uint32_t data_len = *reinterpret_cast< std::uint32_t* >( info + 8u );
							if ( data_len >= 8u && 12u + data_len <= length )
								hwid::patch_smbios( info + 12u, data_len );
						}
						__except ( EXCEPTION_EXECUTE_HANDLER ) { }
					}
					else if ( sensitive_value ) {
						if ( !registry::try_randomize( key_handle, value_name, key_value_information_class, key_value_information, length, true ) )
							return static_cast< nt_status_t >( 0xC0000034 );
					}
					else {
						if ( !registry::try_randomize( key_handle, value_name, key_value_information_class, key_value_information, length ) )
							return static_cast< nt_status_t >( 0xC0000034 );
					}
				}

				return status;
            }

            return nt_query_value_key_org( key_handle, value_name, key_value_information_class, key_value_information, length, result_length );
        }

        nt_status_t( *nt_query_system_environment_value_ex_org )( UNICODE_STRING*, GUID*, void*, unsigned long*, unsigned long* ) = nullptr;
        nt_status_t nt_query_system_environment_value_ex( UNICODE_STRING* variable_name, GUID* vendor_guid, void* value, unsigned long* value_length, unsigned long* attributes ) {
            auto status = nt_query_system_environment_value_ex_org( variable_name, vendor_guid, value, value_length, attributes );

            if ( image::contains( _ReturnAddress( ) ) &&
                 km_is_sensitive_efi_name( variable_name ) &&
                 NT_SUCCESS( status ) && value && value_length &&
                 kernel::mm_is_address_valid( value ) &&
                 kernel::mm_is_address_valid( value_length ) ) {
                __try {
                    hwid::fill_consistent( value, *value_length );
                }
                __except ( EXCEPTION_EXECUTE_HANDLER ) { }
            }

            return status;
        }

        nt_status_t( *nt_query_license_value_org )( UNICODE_STRING*, std::uint32_t*, void*, std::uint32_t, std::uint32_t* ) = nullptr;
        nt_status_t nt_query_license_value( UNICODE_STRING* name, std::uint32_t* type, void* data, std::uint32_t data_size, std::uint32_t* result_size ) {
            if ( image::contains( _ReturnAddress( ) ) &&
                 km_unicode_contains( name, L"CodeIntegrity" ) ) {
                __try {
                    if ( type )
                        *type = 4u;
                    if ( data && data_size >= sizeof( std::uint32_t ) )
                        *reinterpret_cast< std::uint32_t* >( data ) = 0u;
                    if ( result_size )
                        *result_size = sizeof( std::uint32_t );
                }
                __except ( EXCEPTION_EXECUTE_HANDLER ) { }
                return nt_status_t::success;
            }
            return nt_query_license_value_org( name, type, data, data_size, result_size );
        }

        nt_status_t( *nt_enumerate_value_key_org )( HANDLE, std::uint32_t, std::uint32_t, void*, std::uint32_t, std::uint32_t* ) = nullptr;
        nt_status_t nt_enumerate_value_key( HANDLE key_handle, std::uint32_t index, std::uint32_t key_value_information_class, void* key_value_information, std::uint32_t length, std::uint32_t* result_length ) {
                kernel::dbg_print( obf( "nt_enumerate_value_key (key: 0x%llx) (index: %u) (class: %u)\n" ),
                    reinterpret_cast< std::uint64_t >( key_handle ), index, key_value_information_class );
            return nt_enumerate_value_key_org( key_handle, index, key_value_information_class, key_value_information, length, result_length );
        }

        nt_status_t( *ob_reference_object_by_handle_org )( HANDLE, std::uint32_t, void*, KPROCESSOR_MODE, void**, void* ) = nullptr;
        nt_status_t ob_reference_object_by_handle( HANDLE handle, std::uint32_t desired_access, void* object_type, KPROCESSOR_MODE access_mode, void** object, void* handle_information ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                kernel::dbg_print( obf( "ob_reference_object_by_handle blocked (handle: 0x%llx) (access: 0x%x)\n" ),
                    reinterpret_cast< std::uint64_t >( handle ), desired_access );
                return nt_status_t::unsuccessful;
            }
            return ob_reference_object_by_handle_org( handle, desired_access, object_type, access_mode, object, handle_information );
        }

        nt_status_t( *ob_reference_object_by_name_org )( UNICODE_STRING*, std::uint32_t, void*, std::uint32_t, void*, KPROCESSOR_MODE, void*, void** ) = nullptr;
        nt_status_t ob_reference_object_by_name( UNICODE_STRING* object_name, std::uint32_t attributes, void* access_state, std::uint32_t desired_access, void* object_type, KPROCESSOR_MODE access_mode, void* parse_context, void** object ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                kernel::dbg_print( obf( "ob_reference_object_by_name blocked\n" ) );
                return nt_status_t::unsuccessful;
            }
            return ob_reference_object_by_name_org( object_name, attributes, access_state, desired_access, object_type, access_mode, parse_context, object );
        }

        nt_status_t( *ob_open_object_by_name_org )( OBJECT_ATTRIBUTES*, void*, KPROCESSOR_MODE, void*, std::uint32_t, void*, HANDLE* ) = nullptr;
        nt_status_t ob_open_object_by_name( OBJECT_ATTRIBUTES* object_attributes, void* object_type, KPROCESSOR_MODE access_mode, void* access_state, std::uint32_t desired_access, void* parse_context, HANDLE* handle ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                kernel::dbg_print( obf( "ob_open_object_by_name blocked\n" ) );
                return nt_status_t::unsuccessful;
            }
            return ob_open_object_by_name_org( object_attributes, object_type, access_mode, access_state, desired_access, parse_context, handle );
        }

        nt_status_t( *ob_open_object_by_pointer_org )( void*, std::uint32_t, void*, std::uint32_t, void*, KPROCESSOR_MODE, HANDLE* ) = nullptr;
        nt_status_t ob_open_object_by_pointer( void* object, std::uint32_t handle_attributes, void* passed_access_state, std::uint32_t desired_access, void* object_type, KPROCESSOR_MODE access_mode, HANDLE* handle ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                kernel::dbg_print( obf( "ob_open_object_by_pointer blocked\n" ) );
                return nt_status_t::unsuccessful;
            }
            return ob_open_object_by_pointer_org( object, handle_attributes, passed_access_state, desired_access, object_type, access_mode, handle );
        }

        nt_status_t( *ob_query_name_string_org )( void*, OBJECT_NAME_INFORMATION*, std::uint32_t, std::uint32_t* ) = nullptr;
        nt_status_t ob_query_name_string( void* object, OBJECT_NAME_INFORMATION* object_name_information, std::uint32_t length, std::uint32_t* return_length ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                kernel::dbg_print( obf( "ob_query_name_string blocked\n" ) );
                return nt_status_t::unsuccessful;
            }
            return ob_query_name_string_org( object, object_name_information, length, return_length );
        }

        nt_status_t( *nt_query_virtual_memory_org )( HANDLE, void*, std::uint32_t, void*, std::size_t, std::size_t* ) = nullptr;
        nt_status_t nt_query_virtual_memory( HANDLE process_handle, void* base_address, std::uint32_t memory_information_class, void* memory_information, std::size_t memory_information_length, std::size_t* return_length ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                kernel::dbg_print( obf( "nt_query_virtual_memory blocked (class: 0x%x)\n" ),
                    memory_information_class );
                return nt_status_t::unsuccessful;
            }
            return nt_query_virtual_memory_org( process_handle, base_address, memory_information_class, memory_information, memory_information_length, return_length );
        }

        nt_status_t( *nt_query_information_token_org )( HANDLE, std::uint32_t, void*, std::uint32_t, std::uint32_t* ) = nullptr;
        nt_status_t nt_query_information_token( HANDLE token_handle, std::uint32_t token_information_class, void* token_information, std::uint32_t token_information_length, std::uint32_t* return_length ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                kernel::dbg_print( obf( "nt_query_information_token blocked (class: 0x%x)\n" ),
                    token_information_class );
                return nt_status_t::unsuccessful;
            }
            return nt_query_information_token_org( token_handle, token_information_class, token_information, token_information_length, return_length );
        }

        BOOLEAN( *vsl_get_secure_pci_enabled_org )( ) = nullptr;
        BOOLEAN vsl_get_secure_pci_enabled( ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                kernel::dbg_print( obf( "vsl_get_secure_pci_enabled blocked\n" ) );
                return FALSE;
            }
            return vsl_get_secure_pci_enabled_org( );
        }

        struct fake_process_entry_t {
            void* m_process;
            HANDLE m_process_id;
            HANDLE m_parent_process_id;
            std::uint64_t m_create_time;
            char m_image_file_name[ 16 ];
        };

        constexpr auto max_fake_process_entries = 128;
        fake_process_entry_t m_fake_process_entries[ max_fake_process_entries ]{ };
        volatile long m_fake_process_entry_count = 0;
        char m_fake_process_image_name[ 16 ] = "svchost.exe";

        fake_process_entry_t* find_fake_process_entry( void* process ) {
            if ( !process )
                return nullptr;

            auto EntryCount = m_fake_process_entry_count;
            if ( EntryCount < 0 )
                EntryCount = 0;
            if ( EntryCount > max_fake_process_entries )
                EntryCount = max_fake_process_entries;

            for ( auto EntryIndex = 0; EntryIndex < EntryCount; EntryIndex++ ) {
                auto* Entry = &m_fake_process_entries[ EntryIndex ];
                if ( Entry->m_process == process )
                    return Entry;
            }

            return nullptr;
        }

        fake_process_entry_t* track_fake_process_entry( void* process, HANDLE process_id ) {
            if ( !process )
                return nullptr;

            auto* ExistingEntry = find_fake_process_entry( process );
            if ( ExistingEntry )
                return ExistingEntry;

            auto EntryIndex = InterlockedIncrement(
                reinterpret_cast< volatile long* >( &m_fake_process_entry_count ) ) - 1;
            if ( EntryIndex < 0 || EntryIndex >= max_fake_process_entries ) {
                m_fake_process_entry_count = max_fake_process_entries;
                return nullptr;
            }

            auto* Entry = &m_fake_process_entries[ EntryIndex ];
            Entry->m_process = process;
            Entry->m_process_id = process_id;
            Entry->m_parent_process_id = reinterpret_cast< HANDLE >( 4 );
            Entry->m_create_time = kernel::ke_query_system_time_precise( );
            RtlSecureZeroMemory( Entry->m_image_file_name, sizeof( Entry->m_image_file_name ) );
            std::memcpy( Entry->m_image_file_name, m_fake_process_image_name, sizeof( m_fake_process_image_name ) );
            Entry->m_image_file_name[ sizeof( Entry->m_image_file_name ) - 1 ] = 0;
            return Entry;
        }

        nt_status_t( *ps_lookup_process_by_process_id_org )( HANDLE, void** ) = nullptr;
        nt_status_t ps_lookup_process_by_process_id( HANDLE process_id, void** process ) {
            auto Result = ps_lookup_process_by_process_id_org( process_id, process );
            if ( image::contains( _ReturnAddress( ) ) ) {
                
                
                
                
                

                if ( NT_SUCCESS( Result ) && process && *process )
                    track_fake_process_entry( *process, process_id );
            }
            return Result;
        }

        HANDLE( *ps_get_process_id_org )( void* ) = nullptr;
        HANDLE ps_get_process_id( void* process ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                auto* Entry = find_fake_process_entry( process );
                if ( Entry ) {
                    
                    
                    
                    
                    return Entry->m_process_id;
                }
            }
            return ps_get_process_id_org( process );
        }

        char* ( *ps_get_process_image_file_name_org )( void* ) = nullptr;
        char* ps_get_process_image_file_name( void* process ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                auto* Entry = find_fake_process_entry( process );
                if ( Entry ) {
                    
                    
                    
                    
                    return Entry->m_image_file_name;
                }
            }
            return ps_get_process_image_file_name_org( process );
        }

        void* ( *ps_get_process_wow64_process_org )( void* ) = nullptr;
        void* ps_get_process_wow64_process( void* process ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                auto* Entry = find_fake_process_entry( process );
                if ( Entry ) {
                    kernel::dbg_print(
                        obf( "ps_get_process_wow64_process spoofed process=0x%llx\n" ),
                        reinterpret_cast< std::uint64_t >( process ) );
                    return nullptr;
                }
            }
            return ps_get_process_wow64_process_org( process );
        }

        void* ( *ps_get_process_peb_org )( void* ) = nullptr;
        void* ps_get_process_peb( void* process ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                auto* Entry = find_fake_process_entry( process );
                if ( Entry ) {
                    kernel::dbg_print(
                        obf( "ps_get_process_peb spoofed process=0x%llx\n" ),
                        reinterpret_cast< std::uint64_t >( process ) );
                    return nullptr;
                }
            }
            return ps_get_process_peb_org( process );
        }

        HANDLE( *ps_get_process_inherited_from_unique_process_id_org )( void* ) = nullptr;
        HANDLE ps_get_process_inherited_from_unique_process_id( void* process ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                

                
                
                

                
                
                
                
                
                
            }
            return ps_get_process_inherited_from_unique_process_id_org( process );
        }

        std::int64_t( *ps_get_process_create_time_quad_part_org )( void* ) = nullptr;
        std::int64_t ps_get_process_create_time_quad_part( void* process ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                auto* Entry = find_fake_process_entry( process );
                if ( Entry ) {
                    kernel::dbg_print(
                        obf( "ps_get_process_create_time_quad_part spoofed process=0x%llx create_time=0x%llx\n" ),
                        reinterpret_cast< std::uint64_t >( process ),
                        Entry->m_create_time );
                    return Entry->m_create_time;
                }
            }
            return ps_get_process_create_time_quad_part_org( process );
        }

        BOOLEAN( *ps_is_protected_process_org )( void* ) = nullptr;
        BOOLEAN ps_is_protected_process( void* process ) {
            if ( image::contains( _ReturnAddress( ) ) ) {
                auto* Entry = find_fake_process_entry( process );
                if ( Entry ) {
                    kernel::dbg_print(
                        obf( "ps_is_protected_process spoofed process=0x%llx\n" ),
                        reinterpret_cast< std::uint64_t >( process ) );
                    return FALSE;
                }
            }
            return ps_is_protected_process_org( process );
        }

		std::int32_t( *obp_call_pre_operation_callbacks_org )(
			void*,
			void*,
			list_entry_t* ) = nullptr;

		std::int32_t obp_call_pre_operation_callbacks(
			void* object_type,
			void* operation_information,
			list_entry_t* post_callback_list ) {
			auto* info =
				reinterpret_cast< ob_pre_operation_information_view* >(
					operation_information
					);

			if ( info && info->object && info->object_type ) {
				if ( info->object_type == *PsProcessType ) {
					char target_process[ 16 ] { };
					if ( !memory::get_image_file_name( info->object, target_process, sizeof( target_process ) ) )
						goto original;

					const auto is_target =
						strstr_lowercase( target_process, obf( "fortnite" ) ) ||
						strstr_lowercase( target_process, obf( "rust" ) ) ||
						strstr_lowercase( target_process, obf( "eac" ) );

					if ( !is_target )
						goto original;

					char opener_process[ 16 ] { };
					if ( !memory::get_image_file_name( kernel::ps_get_current_process( ), opener_process, sizeof( opener_process ) ) )
						goto original;

					const auto is_itself =
						strstr_lowercase( opener_process, obf( "fortnite" ) ) ||
						strstr_lowercase( opener_process, obf( "rust" ) ) ||
						strstr_lowercase( opener_process, obf( "eac" ) );

					if ( !is_itself )
						return STATUS_SUCCESS;
				}
			}

		original:
			return obp_call_pre_operation_callbacks_org(
				object_type,
				operation_information,
				post_callback_list
			);
		}

		void( __fastcall* psp_call_thread_notify_routines_org )(
			uintptr_t*,
			std::uint8_t,
			bool
			) = nullptr;

		void __fastcall psp_call_thread_notify_routines( uintptr_t* handle, std::uint8_t unk2, bool unk3 ) {
			if ( !handle
				|| !kernel::mm_is_address_valid( handle ) )
				return psp_call_thread_notify_routines_org( handle, unk2, unk3 );

			if ( unk2 != 1 )
				return psp_call_thread_notify_routines_org( handle, unk2, unk3 );

			static const auto off_process = pdb::get_member( obf( "_KTHREAD" ), obf( "Process" ) );
			const auto process = *reinterpret_cast< PEPROCESS* >(
				reinterpret_cast< std::uint8_t* >( handle ) + off_process );
			if ( !process
				|| !kernel::mm_is_address_valid( process ) )
				return psp_call_thread_notify_routines_org( handle, unk2, unk3 );

			char target_process[ 16 ] { };
			if ( !memory::get_image_file_name( process, target_process, sizeof( target_process ) ) )
				return psp_call_thread_notify_routines_org( handle, unk2, unk3 );

			const auto is_target =
				strstr_lowercase( target_process, obf( "fortnite" ) ) ||
				strstr_lowercase( target_process, obf( "rust" ) ) ||
				strstr_lowercase( target_process, obf( "eac" ) );

			if ( !is_target )
				return psp_call_thread_notify_routines_org( handle, unk2, unk3 );

			const auto current_process = kernel::ps_get_current_process( );
			if ( !current_process
				|| !kernel::mm_is_address_valid( current_process ) )
				return psp_call_thread_notify_routines_org( handle, unk2, unk3 );

			char opener_process[ 16 ] { };
			if ( !memory::get_image_file_name( current_process, opener_process, sizeof( opener_process ) ) )
				return psp_call_thread_notify_routines_org( handle, unk2, unk3 );

			const auto is_itself =
				strstr_lowercase( opener_process, obf( "fortnite" ) ) ||
				strstr_lowercase( opener_process, obf( "rust" ) ) ||
				strstr_lowercase( opener_process, obf( "eac" ) );

			if ( !is_itself )
				return;

			return psp_call_thread_notify_routines_org( handle, unk2, unk3 );
		}

        bool create( ) {
			/*    
			TEST USAGE:
			const auto nt_enumerate_value_key_va =  pdb::get_symbol( obf( "NtEnumerateValueKey" ) );
			auto nt_enumerate_value_key_state = exception::hook::create_hook( nt_enumerate_value_key_va, &nt_enumerate_value_key_org );
			if ( !nt_enumerate_value_key_state ) { disk::print( obf( "could not create hook -> NtEnumerateValueKey\n" ) ); return false; }
			if ( !exception::hook::enable_hook( nt_enumerate_value_key_state, reinterpret_cast< std::uint64_t >( nt_enumerate_value_key ) ) ) { disk::print( obf( "could not enable hook -> NtEnumerateValueKey\n" ) ); return false; }
			if ( !exception::hook::swap_to_hooked( nt_enumerate_value_key_state ) ) { disk::print( obf( "could not swap hook -> NtEnumerateValueKey\n" ) ); return false; }

			REMOVED ( TO CLOSE TO THE INTERNAL HARDWARE ):
            const auto mm_map_io_space_va = pdb::get_symbol( obf( "MmMapIoSpace" ) );
            auto mm_map_io_space_state = exception::hook::create_hook( mm_map_io_space_va, &mm_map_io_space_org );
            if ( !mm_map_io_space_state ) { disk::print( obf( "could not create hook -> MmMapIoSpace\n" ) ); return false; }
            if ( !exception::hook::enable_hook( mm_map_io_space_state, reinterpret_cast< std::uint64_t >( mm_map_io_space ) ) ) { disk::print( obf( "could not enable hook -> MmMapIoSpace\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( mm_map_io_space_state ) ) { disk::print( obf( "could not swap hook -> MmMapIoSpace\n" ) ); return false; }

            const auto mm_map_io_space_ex_va = pdb::get_symbol( obf( "MmMapIoSpaceEx" ) );
            auto mm_map_io_space_ex_state = exception::hook::create_hook( mm_map_io_space_ex_va, &mm_map_io_space_ex_org );
            if ( !mm_map_io_space_ex_state ) { disk::print( obf( "could not create hook -> MmMapIoSpaceEx\n" ) ); return false; }
            if ( !exception::hook::enable_hook( mm_map_io_space_ex_state, reinterpret_cast< std::uint64_t >( mm_map_io_space_ex ) ) ) { disk::print( obf( "could not enable hook -> MmMapIoSpaceEx\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( mm_map_io_space_ex_state ) ) { disk::print( obf( "could not swap hook -> MmMapIoSpaceEx\n" ) ); return false; }

            const auto rtl_lookup_function_entry_va = pdb::get_symbol( obf( "RtlLookupFunctionEntry" ) );
            auto rtl_lookup_function_entry_state = exception::hook::create_hook( rtl_lookup_function_entry_va, &rtl_lookup_function_entry_org );
            if ( !rtl_lookup_function_entry_state ) { disk::print( obf( "could not create hook -> RtlLookupFunctionEntry\n" ) ); return false; }
            if ( !exception::hook::enable_hook( rtl_lookup_function_entry_state, reinterpret_cast< std::uint64_t >( rtl_lookup_function_entry ) ) ) { disk::print( obf( "could not enable hook -> RtlLookupFunctionEntry\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( rtl_lookup_function_entry_state ) ) { disk::print( obf( "could not swap hook -> RtlLookupFunctionEntry\n" ) ); return false; }

            const auto rtl_walk_frame_chain_va = pdb::get_symbol( obf( "RtlWalkFrameChain" ) );
            auto rtl_walk_frame_chain_state = exception::hook::create_hook( rtl_walk_frame_chain_va, &rtl_walk_frame_chain_org );
            if ( !rtl_walk_frame_chain_state ) { disk::print( obf( "could not create hook -> RtlWalkFrameChain\n" ) ); return false; }
            if ( !exception::hook::enable_hook( rtl_walk_frame_chain_state, reinterpret_cast< std::uint64_t >( rtl_walk_frame_chain ) ) ) { disk::print( obf( "could not enable hook -> RtlWalkFrameChain\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( rtl_walk_frame_chain_state ) ) { disk::print( obf( "could not swap hook -> RtlWalkFrameChain\n" ) ); return false; }

            const auto mm_allocate_contiguous_node_memory_va = pdb::get_symbol( obf( "MmAllocateContiguousNodeMemory" ) );
            auto mm_allocate_contiguous_node_memory_state = exception::hook::create_hook( mm_allocate_contiguous_node_memory_va, &mm_allocate_contiguous_node_memory_org );
            if ( !mm_allocate_contiguous_node_memory_state ) { disk::print( obf( "could not create hook -> MmAllocateContiguousNodeMemory\n" ) ); return false; }
            if ( !exception::hook::enable_hook( mm_allocate_contiguous_node_memory_state, reinterpret_cast< std::uint64_t >( mm_allocate_contiguous_node_memory ) ) ) { disk::print( obf( "could not enable hook -> MmAllocateContiguousNodeMemory\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( mm_allocate_contiguous_node_memory_state ) ) { disk::print( obf( "could not swap hook -> MmAllocateContiguousNodeMemory\n" ) ); return false; }
			*/

			const auto mm_copy_memory_va = pdb::get_symbol( obf( "MmCopyMemory" ) );
			auto mm_copy_memory_state = exception::hook::create_hook( mm_copy_memory_va, &mm_copy_memory_org );
			if ( !mm_copy_memory_state ) { disk::print( obf( "could not create hook -> MmCopyMemory\n" ) ); return false; }
			if ( !exception::hook::enable_hook( mm_copy_memory_state, reinterpret_cast< std::uint64_t >( mm_copy_memory ) ) ) { disk::print( obf( "could not enable hook -> MmCopyMemory\n" ) ); return false; }
			if ( !exception::hook::swap_to_hooked( mm_copy_memory_state ) ) { disk::print( obf( "could not swap hook -> MmCopyMemory\n" ) ); return false; }

			const auto nt_write_file_va = pdb::get_symbol( obf( "NtWriteFile" ) );
			auto nt_write_file_state = exception::hook::create_hook( nt_write_file_va, &nt_write_file_org );
			if ( !nt_write_file_state ) { disk::print( obf( "could not create hook -> NtWriteFile\n" ) ); return false; }
			if ( !exception::hook::enable_hook( nt_write_file_state, reinterpret_cast< std::uint64_t >( nt_write_file ) ) ) { disk::print( obf( "could not enable hook -> NtWriteFile\n" ) ); return false; }
			if ( !exception::hook::swap_to_hooked( nt_write_file_state ) ) { disk::print( obf( "could not swap hook -> NtWriteFile\n" ) ); return false; }

            const auto io_get_device_interfaces_va = pdb::get_symbol( obf( "IoGetDeviceInterfaces" ) );
            auto io_get_device_interfaces_state = exception::hook::create_hook( io_get_device_interfaces_va, &io_get_device_interfaces_org );
            if ( !io_get_device_interfaces_state ) { disk::print( obf( "could not create hook -> IoGetDeviceInterfaces\n" ) ); return false; }
            if ( !exception::hook::enable_hook( io_get_device_interfaces_state, reinterpret_cast< std::uint64_t >( io_get_device_interfaces ) ) ) { disk::print( obf( "could not enable hook -> IoGetDeviceInterfaces\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( io_get_device_interfaces_state ) ) { disk::print( obf( "could not swap hook -> IoGetDeviceInterfaces\n" ) ); return false; }

            const auto io_enumerate_device_object_list_va = pdb::get_symbol( obf( "IoEnumerateDeviceObjectList" ) );
            auto io_enumerate_device_object_list_state = exception::hook::create_hook( io_enumerate_device_object_list_va, &io_enumerate_device_object_list_org );
            if ( !io_enumerate_device_object_list_state ) { disk::print( obf( "could not create hook -> IoEnumerateDeviceObjectList\n" ) ); return false; }
            if ( !exception::hook::enable_hook( io_enumerate_device_object_list_state, reinterpret_cast< std::uint64_t >( io_enumerate_device_object_list ) ) ) { disk::print( obf( "could not enable hook -> IoEnumerateDeviceObjectList\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( io_enumerate_device_object_list_state ) ) { disk::print( obf( "could not swap hook -> IoEnumerateDeviceObjectList\n" ) ); return false; }

            const auto io_get_device_object_pointer_va = pdb::get_symbol( obf( "IoGetDeviceObjectPointer" ) );
            auto io_get_device_object_pointer_state = exception::hook::create_hook( io_get_device_object_pointer_va, &io_get_device_object_pointer_org );
            if ( !io_get_device_object_pointer_state ) { disk::print( obf( "could not create hook -> IoGetDeviceObjectPointer\n" ) ); return false; }
            if ( !exception::hook::enable_hook( io_get_device_object_pointer_state, reinterpret_cast< std::uint64_t >( io_get_device_object_pointer ) ) ) { disk::print( obf( "could not enable hook -> IoGetDeviceObjectPointer\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( io_get_device_object_pointer_state ) ) { disk::print( obf( "could not swap hook -> IoGetDeviceObjectPointer\n" ) ); return false; }

            const auto io_query_file_dos_device_name_va = pdb::get_symbol( obf( "IoQueryFileDosDeviceName" ) );
            auto io_query_file_dos_device_name_state = exception::hook::create_hook( io_query_file_dos_device_name_va, &io_query_file_dos_device_name_org );
            if ( !io_query_file_dos_device_name_state ) { disk::print( obf( "could not create hook -> IoQueryFileDosDeviceName\n" ) ); return false; }
            if ( !exception::hook::enable_hook( io_query_file_dos_device_name_state, reinterpret_cast< std::uint64_t >( io_query_file_dos_device_name ) ) ) { disk::print( obf( "could not enable hook -> IoQueryFileDosDeviceName\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( io_query_file_dos_device_name_state ) ) { disk::print( obf( "could not swap hook -> IoQueryFileDosDeviceName\n" ) ); return false; }

            const auto nt_query_directory_file_va = pdb::get_symbol( obf( "NtQueryDirectoryFile" ) );
            auto nt_query_directory_file_state = exception::hook::create_hook( nt_query_directory_file_va, &nt_query_directory_file_org );
            if ( !nt_query_directory_file_state ) { disk::print( obf( "could not create hook -> NtQueryDirectoryFile\n" ) ); return false; }
            if ( !exception::hook::enable_hook( nt_query_directory_file_state, reinterpret_cast< std::uint64_t >( nt_query_directory_file ) ) ) { disk::print( obf( "could not enable hook -> NtQueryDirectoryFile\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( nt_query_directory_file_state ) ) { disk::print( obf( "could not swap hook -> NtQueryDirectoryFile\n" ) ); return false; }

			const auto ex_uuid_create_va = pdb::get_symbol( obf( "ExUuidCreate" ) );
			auto ex_uuid_create_state = exception::hook::create_hook( ex_uuid_create_va, &ex_uuid_create_org );
			if ( !ex_uuid_create_state ) { kernel::dbg_print( obf( "failed to hook ExUuidCreate\n" ) ); return false; }
			if ( !exception::hook::enable_hook( ex_uuid_create_state, reinterpret_cast< std::uint64_t >( ex_uuid_create ) ) ) { kernel::dbg_print( obf( "could not enable hook -> ExUuidCreate\n" ) ); return false; }
			if ( !exception::hook::swap_to_hooked( ex_uuid_create_state ) ) { kernel::dbg_print( obf( "could not swap hook -> ExUuidCreate\n" ) ); return false; }

            const auto io_wmi_open_block_va = pdb::get_symbol( obf( "IoWMIOpenBlock" ) );
            auto io_wmi_open_block_state = exception::hook::create_hook( io_wmi_open_block_va, &io_wmi_open_block_org );
            if ( !io_wmi_open_block_state ) { kernel::dbg_print( obf( "failed to hook IoWMIOpenBlock\n" ) ); return false; }
            if ( !exception::hook::enable_hook( io_wmi_open_block_state, reinterpret_cast< std::uint64_t >( io_wmi_open_block ) ) ) { kernel::dbg_print( obf( "could not enable hook -> IoWMIOpenBlock\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( io_wmi_open_block_state ) ) { kernel::dbg_print( obf( "could not swap hook -> IoWMIOpenBlock\n" ) ); return false; }

            const auto io_wmi_query_all_data_va = pdb::get_symbol( obf( "IoWMIQueryAllData" ) );
            auto io_wmi_query_all_data_state = exception::hook::create_hook( io_wmi_query_all_data_va, &io_wmi_query_all_data_org );
            if ( !io_wmi_query_all_data_state ) { kernel::dbg_print( obf( "failed to hook IoWMIQueryAllData\n" ) ); return false; }
            if ( !exception::hook::enable_hook( io_wmi_query_all_data_state, reinterpret_cast< std::uint64_t >( io_wmi_query_all_data ) ) ) { kernel::dbg_print( obf( "could not enable hook -> IoWMIQueryAllData\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( io_wmi_query_all_data_state ) ) { kernel::dbg_print( obf( "could not swap hook -> IoWMIQueryAllData\n" ) ); return false; }

            const auto nt_query_value_key_va = pdb::get_symbol( obf( "NtQueryValueKey" ) );
            auto nt_query_value_key_state = exception::hook::create_hook( nt_query_value_key_va, &nt_query_value_key_org );
            if ( !nt_query_value_key_state ) { disk::print( obf( "could not create hook -> NtQueryValueKey\n" ) ); return false; }
            if ( !exception::hook::enable_hook( nt_query_value_key_state, reinterpret_cast< std::uint64_t >( nt_query_value_key ) ) ) { disk::print( obf( "could not enable hook -> NtQueryValueKey\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( nt_query_value_key_state ) ) { disk::print( obf( "could not swap hook -> NtQueryValueKey\n" ) ); return false; }

            const auto ex_get_firmware_environment_variable_va = pdb::get_symbol( obf( "ExGetFirmwareEnvironmentVariable" ) );
            auto ex_get_firmware_environment_variable_state = exception::hook::create_hook( ex_get_firmware_environment_variable_va, &ex_get_firmware_environment_variable_org );
            if ( !ex_get_firmware_environment_variable_state ) { disk::print( obf( "could not create hook -> ExGetFirmwareEnvironmentVariable\n" ) ); return false; }
            if ( !exception::hook::enable_hook( ex_get_firmware_environment_variable_state, reinterpret_cast< std::uint64_t >( ex_get_firmware_environment_variable ) ) ) { disk::print( obf( "could not enable hook -> ExGetFirmwareEnvironmentVariable\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( ex_get_firmware_environment_variable_state ) ) { disk::print( obf( "could not swap hook -> ExGetFirmwareEnvironmentVariable\n" ) ); return false; }

            const auto nt_query_system_environment_value_ex_va = pdb::get_symbol( obf( "NtQuerySystemEnvironmentValueEx" ) );
            auto nt_query_system_environment_value_ex_state = exception::hook::create_hook( nt_query_system_environment_value_ex_va, &nt_query_system_environment_value_ex_org );
            if ( !nt_query_system_environment_value_ex_state ) { disk::print( obf( "could not create hook -> NtQuerySystemEnvironmentValueEx\n" ) ); return false; }
            if ( !exception::hook::enable_hook( nt_query_system_environment_value_ex_state, reinterpret_cast< std::uint64_t >( nt_query_system_environment_value_ex ) ) ) { disk::print( obf( "could not enable hook -> NtQuerySystemEnvironmentValueEx\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( nt_query_system_environment_value_ex_state ) ) { disk::print( obf( "could not swap hook -> NtQuerySystemEnvironmentValueEx\n" ) ); return false; }

            const auto nt_query_license_value_va = pdb::get_symbol( obf( "NtQueryLicenseValue" ) );
            auto nt_query_license_value_state = exception::hook::create_hook( nt_query_license_value_va, &nt_query_license_value_org );
            if ( !nt_query_license_value_state ) { disk::print( obf( "could not create hook -> NtQueryLicenseValue\n" ) ); return false; }
            if ( !exception::hook::enable_hook( nt_query_license_value_state, reinterpret_cast< std::uint64_t >( nt_query_license_value ) ) ) { disk::print( obf( "could not enable hook -> NtQueryLicenseValue\n" ) ); return false; }
            if ( !exception::hook::swap_to_hooked( nt_query_license_value_state ) ) { disk::print( obf( "could not swap hook -> NtQueryLicenseValue\n" ) ); return false; }

			const auto obp_call_pre_operation_callbacks_va = pdb::get_symbol( obf( "ObpCallPreOperationCallbacks" ) );
			auto obp_call_pre_operation_callbacks_state = exception::hook::create_hook( obp_call_pre_operation_callbacks_va, &obp_call_pre_operation_callbacks_org );
			if ( !obp_call_pre_operation_callbacks_state ) { disk::print( obf( "could not create hook -> ObpCallPreOperationCallbacks\n" ) ); return false; }
			if ( !exception::hook::enable_hook( obp_call_pre_operation_callbacks_state, reinterpret_cast< std::uint64_t >( obp_call_pre_operation_callbacks ) ) ) { disk::print( obf( "could not enable hook -> ObpCallPreOperationCallbacks\n" ) ); return false; }
			if ( !exception::hook::swap_to_hooked( obp_call_pre_operation_callbacks_state ) ) { disk::print( obf( "could not swap hook -> ObpCallPreOperationCallbacks\n" ) ); return false; }

			const auto psp_call_thread_notify_routines_va = pdb::get_symbol( obf( "PspCallThreadNotifyRoutines" ) );
			auto psp_call_thread_notify_routines_state = exception::hook::create_hook( psp_call_thread_notify_routines_va, &psp_call_thread_notify_routines_org );
			if ( !psp_call_thread_notify_routines_state ) { disk::print( obf( "could not create hook -> PspCallThreadNotifyRoutines\n" ) ); return false; }
			if ( !exception::hook::enable_hook( psp_call_thread_notify_routines_state, reinterpret_cast< std::uint64_t >( psp_call_thread_notify_routines ) ) ) { disk::print( obf( "could not enable hook -> PspCallThreadNotifyRoutines\n" ) ); return false; }
			if ( !exception::hook::swap_to_hooked( psp_call_thread_notify_routines_state ) ) { disk::print( obf( "could not swap hook -> PspCallThreadNotifyRoutines\n" ) ); return false; }

            return true;
        }
    }
}
