#pragma once

namespace hooks {
	namespace um {
		std::uint32_t m_display_serial_0 = 0;
		std::uint32_t m_display_serial_1 = 0;
		std::uint32_t m_display_serial_2 = 0;
		std::uint32_t m_display_serial_3 = 0;
		std::uint32_t m_volume_serial = 0;
		std::uint8_t m_uuid_seed[ 6 ] { };
		unsigned long m_uuid_sequence = 0;
		std::uint64_t m_device_api_cookie = 0;
		std::uint64_t m_last_sid_time = 0;

		static bool is_epic_process( ) {
			auto process = kernel::ps_get_current_process( );
			if ( !kernel::mm_is_address_valid( process ) )
				return false;
			char name[ 16 ] { };
			if ( !memory::get_image_file_name( process, name, sizeof( name ) ) )
				return false;
			return strstr_lowercase( name, obf( "epic" ) );
		}

		bool unicode_contains(
			UNICODE_STRING* string,
			const wchar_t* needle
		) {
			if ( !string || !string->Buffer || !string->Length || !needle )
				return false;

			auto needle_length = wcslen( needle );
			auto string_length = string->Length / sizeof( wchar_t );
			if ( !needle_length || string_length < needle_length )
				return false;

			for ( std::size_t i = 0; i <= string_length - needle_length; i++ ) {
				bool matched = true;
				for ( std::size_t j = 0; j < needle_length; j++ ) {
					auto a = string->Buffer[ i + j ];
					auto b = needle[ j ];
					if ( a >= L'A' && a <= L'Z' )
						a += L'a' - L'A';
					if ( b >= L'A' && b <= L'Z' )
						b += L'a' - L'A';
					if ( a != b ) {
						matched = false;
						break;
					}
				}
				if ( matched )
					return true;
			}

			return false;
		}

		bool is_sensitive_name(
			UNICODE_STRING* name
		) {
			return unicode_contains( name, L"MachineGuid" ) ||
				unicode_contains( name, L"HwProfileGuid" ) ||
				unicode_contains( name, L"Hardware" ) ||
				unicode_contains( name, L"Identifier" ) ||
				unicode_contains( name, L"SystemBios" ) ||
				unicode_contains( name, L"VideoBios" ) ||
				unicode_contains( name, L"BaseBoard" ) ||
				unicode_contains( name, L"BIOS" ) ||
				unicode_contains( name, L"ProductId" ) ||
				unicode_contains( name, L"DigitalProductId" ) ||
				unicode_contains( name, L"ComputerHardwareId" ) ||
				unicode_contains( name, L"ContainerID" ) ||
				unicode_contains( name, L"MatchingDeviceId" ) ||
				unicode_contains( name, L"DriverDesc" ) ||
				unicode_contains( name, L"DeviceDesc" ) ||
				unicode_contains( name, L"Serial" ) ||
				unicode_contains( name, L"UUID" ) ||
				unicode_contains( name, L"Guid" ) ||
				unicode_contains( name, L"InstallDate" ) ||
				unicode_contains( name, L"Hostname" ) ||
				unicode_contains( name, L"NV Hostname" ) ||
				unicode_contains( name, L"ComputerName" ) ||
				unicode_contains( name, L"Dhcpv6" ) ||
				unicode_contains( name, L"DhcpNetworkHint" ) ||
				unicode_contains( name, L"Endorsement" ) ||
				unicode_contains( name, L"TPM" ) ||
				unicode_contains( name, L"DUID" ) ||
				unicode_contains( name, L"PhysicalMac" ) ||
				unicode_contains( name, L"NetworkAddress" ) ||
				unicode_contains( name, L"UnlockID" ) ||
				unicode_contains( name, L"OfflineUniqueID" ) ||
				unicode_contains( name, L"PlatformModule" ) ||
				unicode_contains( name, L"EDID" ) ||
				unicode_contains( name, L"MonitorID" ) ||
				unicode_contains( name, L"DnsFQDN" );
		}

		void set_io_status(
			io_status_block_t* io_status_block,
			nt_status_t status,
			unsigned long information
		) {
			if ( !io_status_block )
				return;

			*reinterpret_cast< unsigned long* >( &io_status_block->m_status ) =
				static_cast< unsigned long >( status );
			*reinterpret_cast< unsigned long* >( &io_status_block->m_information ) = information;
		}

		nt_status_t complete_zero_output(
			void* output_buffer,
			unsigned long output_buffer_length,
			unsigned long minimum_length,
			io_status_block_t* io_status_block
		) {
			if ( !output_buffer || output_buffer_length < minimum_length ) {
				set_io_status(
					io_status_block,
					nt_status_t::buffer_too_small,
					minimum_length );
				return nt_status_t::buffer_too_small;
			}

			RtlSecureZeroMemory( output_buffer, output_buffer_length );
			set_io_status(
				io_status_block,
				nt_status_t::success,
				output_buffer_length );
			return nt_status_t::success;
		}

		void scrub_registry_value_output(
			std::uint32_t information_class,
			void* key_value_information,
			unsigned long length
		) {
			if ( !key_value_information || !length )
				return;

			__try {
				auto* output = reinterpret_cast< std::uint8_t* >( key_value_information );
				switch ( information_class ) {
				case 0: {
					if ( length >= 12 ) {
						*reinterpret_cast< unsigned long* >( output + 4 ) = 1;
						*reinterpret_cast< unsigned long* >( output + 8 ) = 0;
					}
					break;
				}
				case 1:
				case 3: {
					if ( length >= 20 ) {
						*reinterpret_cast< unsigned long* >( output + 4 ) = 1;
						*reinterpret_cast< unsigned long* >( output + 8 ) = 0;
						*reinterpret_cast< unsigned long* >( output + 12 ) = 0;
					}
					break;
				}
				case 2:
				case 4: {
					if ( length >= 12 ) {
						*reinterpret_cast< unsigned long* >( output + 4 ) = 1;
						*reinterpret_cast< unsigned long* >( output + 8 ) = 0;
					}
					break;
				}
				default:
					break;
				}
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) { }
		}

		nt_status_t( *nt_query_value_key_org )( void*, UNICODE_STRING*, std::uint32_t, void*, unsigned long, unsigned long* ) = nullptr;
		nt_status_t nt_query_value_key(
			void* key_handle,
			UNICODE_STRING* value_name,
			std::uint32_t key_value_information_class,
			void* key_value_information,
			unsigned long length,
			unsigned long* result_length
		) {
			const auto status = nt_query_value_key_org(
				key_handle,
				value_name,
				key_value_information_class,
				key_value_information,
				length,
				result_length );

			if ( is_epic_process( ) )
				return status;

			if ( NT_SUCCESS( status ) )
				registry::try_randomize(
					key_handle,
					value_name,
					key_value_information_class,
					key_value_information,
					length );

			return status;
		}

		nt_status_t( *nt_query_boot_entry_order_org )( unsigned long*, unsigned long* ) = nullptr;
		nt_status_t nt_query_boot_entry_order(
			unsigned long* ids,
			unsigned long* count
		) {
			return nt_status_t::access_violation;
		}

		nt_status_t( *nt_open_section_org )( void**, unsigned long, object_attributes_t* ) = nullptr;
		nt_status_t nt_open_section(
			void** section_handle,
			unsigned long desired_access,
			object_attributes_t* object_attributes
		) {
			if ( is_epic_process( ) )
				return nt_open_section_org( section_handle, desired_access, object_attributes );

			__try {
				if ( object_attributes &&
					object_attributes->m_object_name &&
					object_attributes->m_object_name->m_buffer &&
					object_attributes->m_object_name->m_length ) {

					auto* name = object_attributes->m_object_name;

					static const wchar_t phys_mem[] = L"\\Device\\PhysicalMemory";
					const USHORT phys_mem_len = static_cast< USHORT >(
						sizeof( phys_mem ) - sizeof( wchar_t ) );
					if ( name->m_length == phys_mem_len &&
						RtlCompareMemory( name->m_buffer, phys_mem, phys_mem_len ) == phys_mem_len ) {
						kernel::dbg_print( obf( "blocked NtOpenSection -> \\Device\\PhysicalMemory\n" ) );
						return static_cast< nt_status_t >( 0xC0000022 );
					}
				}
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
			}

			return nt_open_section_org( section_handle, desired_access, object_attributes );
		}

		nt_status_t( *nt_query_system_information_org )(
			std::uint32_t, void*, std::uint32_t, std::uint32_t* ) = nullptr;
		nt_status_t nt_query_system_information(
			std::uint32_t system_information_class,
			void* system_information,
			std::uint32_t system_information_length,
			std::uint32_t* return_length
		) {
			const auto status = nt_query_system_information_org(
				system_information_class, system_information,
				system_information_length, return_length );

			if ( is_epic_process( ) )
				return status;

			if ( !NT_SUCCESS( status ) || !system_information || system_information_length == 0 )
				return status;

			__try {
				switch ( system_information_class ) {

					case 0x23: {
						if ( system_information_length >= 2 ) {
							auto* info = reinterpret_cast< std::uint8_t* >( system_information );
							info[ 0 ] = 0;
							info[ 1 ] = 1;
						}
						break;
					}

					case 0x67: {
						if ( system_information_length >= 8 ) {
							auto* info = reinterpret_cast< std::uint32_t* >( system_information );
							info[ 1 ] = 0x01;
						}
						break;
					}

					case 0x4C: {
						constexpr bool smbios_spoof_enabled = true;
						constexpr bool acpi_spoof_enabled   = true;

						constexpr std::uint32_t rsmb_sig = 0x52534D42u;
						constexpr std::uint32_t acpi_sig = 0x41435049u;

						if ( system_information_length < 16 ) break;
						auto* base = reinterpret_cast< std::uint8_t* >( system_information );
						const auto provider = *reinterpret_cast< std::uint32_t* >( base + 0 );
						const auto table_id = *reinterpret_cast< std::uint32_t* >( base + 8 );
						const auto table_len = *reinterpret_cast< std::uint32_t* >( base + 12 );
						if ( table_len < 4 || 16u + table_len > system_information_length ) break;
						auto* table_buf = base + 16;

						if ( smbios_spoof_enabled && provider == rsmb_sig ) {
							if ( table_len < 8 ) break;
							const auto raw_len = *reinterpret_cast< std::uint32_t* >( table_buf + 4 );
							if ( raw_len == 0 ) break;
							const std::uint32_t smbios_len =
								raw_len <= ( table_len - 8 ) ? raw_len : ( table_len - 8 );
							if ( smbios_len == 0 ) break;
							auto* smbios_data = table_buf + 8;

							static bool smbios_uuid_ready = false;
							static std::uint8_t smbios_uuid[ 16 ];
							if ( !smbios_uuid_ready ) {
								for ( int i = 0; i < 16; ++i )
									smbios_uuid[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );
								smbios_uuid[ 6 ] = ( smbios_uuid[ 6 ] & 0x0F ) | 0x40;
								smbios_uuid[ 8 ] = ( smbios_uuid[ 8 ] & 0x3F ) | 0x80;
								smbios_uuid_ready = true;
							}

							auto patch_string_at_index = [ & ](
								std::uint32_t hdr_off,
								std::uint8_t  hdr_len,
								std::uint8_t  string_idx )
							{
								if ( string_idx == 0 ) return;
								std::uint32_t pos = hdr_off + hdr_len;
								std::uint8_t current = 1;
								while ( pos < smbios_len ) {
									const std::uint32_t start = pos;
									while ( pos < smbios_len && smbios_data[ pos ] != 0 )
										++pos;
									if ( pos >= smbios_len ) return;
									if ( current == string_idx ) {
										static const char alphabet[] =
											"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
										constexpr std::uint32_t alphabet_len = sizeof( alphabet ) - 1;
										for ( std::uint32_t i = start; i < pos; ++i )
											smbios_data[ i ] = static_cast< std::uint8_t >(
												alphabet[ kernel::rand( ) % alphabet_len ] );
										return;
									}
									++current;
									++pos;

									if ( pos < smbios_len && smbios_data[ pos ] == 0 )
										return;
								}
							};

							std::uint32_t off = 0;
							int walked = 0;
							while ( off + 4 <= smbios_len && walked < 512 ) {
								auto* hdr = smbios_data + off;
								const std::uint8_t stype = hdr[ 0 ];
								const std::uint8_t slen  = hdr[ 1 ];
								if ( slen < 4 || slen > 128 ) break;
								if ( off + slen > smbios_len ) break;
								if ( stype == 127 ) break;

								if ( stype == 1 && slen >= 0x18 ) {
									for ( int i = 0; i < 16; ++i )
										hdr[ 8 + i ] = smbios_uuid[ i ];
									patch_string_at_index( off, slen, hdr[ 7 ] );
								}
								else if ( stype == 2 && slen >= 0x08 ) {
									patch_string_at_index( off, slen, hdr[ 7 ] );
								}
								else if ( stype == 3 && slen >= 0x08 ) {
									patch_string_at_index( off, slen, hdr[ 7 ] );
								}

								std::uint32_t next = off + slen;
								while ( next + 1 < smbios_len ) {
									if ( smbios_data[ next ] == 0 && smbios_data[ next + 1 ] == 0 ) {
										next += 2;
										break;
									}
									++next;
								}
								if ( next <= off ) break;
								off = next;
								++walked;
							}
							break;
						}

						if ( acpi_spoof_enabled && provider == acpi_sig ) {
							constexpr std::uint32_t msdm_sig = 0x4D44534Du;
							if ( table_id != msdm_sig ) break;
							if ( table_len < 36 ) break;

							const auto acpi_len = *reinterpret_cast< std::uint32_t* >( table_buf + 4 );
							if ( acpi_len < 36 || acpi_len > table_len ) break;

							static bool msdm_ready = false;
							static char  msdm_oemid[ 6 ];
							static char  msdm_oem_table_id[ 8 ];
							static std::uint32_t msdm_oem_revision = 0;
							static char  msdm_product_key[ 29 ];
							if ( !msdm_ready ) {
								static const char oem_pool[ 6 ][ 7 ] = {
									"DELL  ", "HPQOEM", "LENOVO", "ASUS  ", "ACRSYS", "MSI   "
								};
								const auto oem_pick = kernel::rand( ) % 6u;
								for ( int i = 0; i < 6; ++i )
									msdm_oemid[ i ] = oem_pool[ oem_pick ][ i ];

								for ( int i = 0; i < 8; ++i ) {
									const auto r = kernel::rand( ) % 26u;
									msdm_oem_table_id[ i ] = static_cast< char >( 'A' + r );
								}

								msdm_oem_revision = kernel::rand( );

								static const char key_alphabet[] = "BCDFGHJKMPQRTVWXY2346789";
								constexpr std::uint32_t alphabet_len = sizeof( key_alphabet ) - 1;
								int ki = 0;
								for ( int g = 0; g < 5; ++g ) {
									for ( int c = 0; c < 5; ++c ) {
										msdm_product_key[ ki++ ] =
											key_alphabet[ kernel::rand( ) % alphabet_len ];
									}
									if ( g < 4 )
										msdm_product_key[ ki++ ] = '-';
								}
								msdm_ready = true;
							}

							for ( int i = 0; i < 6; ++i )
								table_buf[ 0x0A + i ] = static_cast< std::uint8_t >( msdm_oemid[ i ] );
							for ( int i = 0; i < 8; ++i )
								table_buf[ 0x10 + i ] = static_cast< std::uint8_t >( msdm_oem_table_id[ i ] );
							*reinterpret_cast< std::uint32_t* >( table_buf + 0x18 ) = msdm_oem_revision;

							if ( acpi_len >= 0x38u + 29u ) {
								for ( int i = 0; i < 29; ++i )
									table_buf[ 0x38 + i ] =
										static_cast< std::uint8_t >( msdm_product_key[ i ] );
							}

							table_buf[ 0x09 ] = 0;
							std::uint32_t sum = 0;
							for ( std::uint32_t i = 0; i < acpi_len; ++i )
								sum += table_buf[ i ];
							table_buf[ 0x09 ] = static_cast< std::uint8_t >( ( 0u - sum ) & 0xFFu );
							break;
						}

						break;
					}

					case 0x5A: {
						constexpr bool boot_id_spoof_enabled = true;
						if ( !boot_id_spoof_enabled ) break;
						if ( system_information_length < 16 ) break;
						static bool boot_id_ready = false;
						static std::uint8_t boot_id[ 16 ];
						if ( !boot_id_ready ) {
							for ( int i = 0; i < 16; ++i )
								boot_id[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );
							boot_id[ 6 ] = ( boot_id[ 6 ] & 0x0F ) | 0x40;
							boot_id[ 8 ] = ( boot_id[ 8 ] & 0x3F ) | 0x80;
							boot_id_ready = true;
						}
						auto* base = reinterpret_cast< std::uint8_t* >( system_information );
						for ( int i = 0; i < 16; ++i )
							base[ i ] = boot_id[ i ];
						break;
					}

					default:
						break;
				}
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
			}

			return status;
		}

		nt_status_t( *nt_allocate_uuids_org )( LARGE_INTEGER*, unsigned long*, unsigned long*, std::uint8_t* ) = nullptr;
		nt_status_t nt_allocate_uuids(
			LARGE_INTEGER* time,
			unsigned long* range,
			unsigned long* sequence,
			std::uint8_t* seed
		) {
			auto result = nt_allocate_uuids_org(
				time, range, sequence, seed );

			kernel::dbg_print(
				obf( "[spoof] NtAllocateUuids time=0x%llx range=0x%llx sequence=0x%llx seed=0x%llx result=0x%08X\n" ),
				reinterpret_cast< std::uint64_t >( time ),
				reinterpret_cast< std::uint64_t >( range ),
				reinterpret_cast< std::uint64_t >( sequence ),
				reinterpret_cast< std::uint64_t >( seed ),
				static_cast< unsigned long >( result ) );

			if ( !NT_SUCCESS( result ) )
				return result;

			__try {
				if ( !m_uuid_sequence ) {
					auto tick_count = kernel::ke_query_tick_count( );
					auto random_seed = static_cast< unsigned long >(
						tick_count.m_low_part ^ tick_count.m_high_part ^ 0x55754944 );
					auto random_a = kernel::rtl_random_ex( &random_seed );
					auto random_b = kernel::rtl_random_ex( &random_seed );

					m_uuid_sequence = ( random_a & 0x3FFF ) | 0x8000;
					m_uuid_seed[ 0 ] = 0x02;
					m_uuid_seed[ 1 ] = static_cast< std::uint8_t >( random_a >> 8 );
					m_uuid_seed[ 2 ] = static_cast< std::uint8_t >( random_a >> 16 );
					m_uuid_seed[ 3 ] = static_cast< std::uint8_t >( random_a >> 24 );
					m_uuid_seed[ 4 ] = static_cast< std::uint8_t >( random_b );
					m_uuid_seed[ 5 ] = static_cast< std::uint8_t >( random_b >> 8 );
				}

				if ( time )
					time->QuadPart = 0x01DB1C8F00000000ULL;
				if ( range )
					*range = 0x100000;
				if ( sequence )
					*sequence = m_uuid_sequence;
				if ( seed )
					std::memcpy( seed, m_uuid_seed, sizeof( m_uuid_seed ) );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
				return nt_status_t::access_violation;
			}

			return result;
		}

		nt_status_t( *nt_user_enum_display_devices_org )( UNICODE_STRING*, unsigned long, void*, unsigned long ) = nullptr;
		nt_status_t nt_user_enum_display_devices(
			UNICODE_STRING* device,
			unsigned long   device_num,
			void* display_device,
			unsigned long   flags
		) {
			if ( !display_device || !nt_user_enum_display_devices_org || is_epic_process( ) )
				return nt_status_t::unsuccessful;

			auto result = nt_user_enum_display_devices_org(
				device, device_num, display_device, flags );
			if ( !NT_SUCCESS( result ) )
				return result;

			struct display_device_t {
				unsigned long cb;
				wchar_t device_name[ 32 ];
				wchar_t device_string[ 128 ];
				unsigned long state_flags;
				wchar_t device_id[ 128 ];
				wchar_t device_key[ 128 ];
			};

			__try {
				LARGE_INTEGER tick_count { };
				KeQueryTickCount( &tick_count );
				auto seed = static_cast< unsigned long >( tick_count.LowPart ^ tick_count.HighPart );

				if ( !m_display_serial_0 ) {
					m_display_serial_0 = kernel::rtl_random_ex( &seed );
					m_display_serial_1 = kernel::rtl_random_ex( &seed );
					m_display_serial_2 = kernel::rtl_random_ex( &seed );
					m_display_serial_3 = kernel::rtl_random_ex( &seed );
				}

				auto* output = reinterpret_cast< display_device_t* >( display_device );
				auto cb = output->cb;
				if ( cb && cb < sizeof( display_device_t ) )
					return result;

				RtlSecureZeroMemory( output, sizeof( display_device_t ) );
				output->cb = cb ? cb : sizeof( display_device_t );
				wcscpy( output->device_name, L"\\\\.\\DISPLAY1" );
				wcscpy( output->device_string, L"Generic Display Adapter" );
				output->state_flags = 0x00000001 | 0x00000004 | 0x04000000;
				swprintf(
					output->device_id,
					L"MONITOR\\GENERIC_DISPLAY\\SORA%08X%08X%08X%08X",
					m_display_serial_0,
					m_display_serial_1,
					m_display_serial_2,
					m_display_serial_3 );
				swprintf(
					output->device_key,
					L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Video\\{%08X-%04X-%04X-%04X-%04X%08X}\\0000",
					m_display_serial_0,
					static_cast< unsigned short >( m_display_serial_1 ),
					static_cast< unsigned short >( m_display_serial_1 >> 16 ),
					static_cast< unsigned short >( m_display_serial_2 ),
					static_cast< unsigned short >( m_display_serial_2 >> 16 ),
					m_display_serial_3 );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
				return nt_status_t::unsuccessful;
			}

			return result;
		}

		nt_status_t( *nt_query_license_value_org )( UNICODE_STRING*, unsigned long*, void*, unsigned long, unsigned long* ) = nullptr;
		nt_status_t nt_query_license_value(
			UNICODE_STRING* value_name,
			unsigned long* type,
			void* data,
			unsigned long   data_size,
			unsigned long* result_data_size
		) {
			if ( is_epic_process( ) ) {
				return nt_query_license_value_org(
					value_name, type, data, data_size, result_data_size );
			}

			if ( value_name && value_name->Buffer ) {
				unicode_string_t allow_key { };
				kernel::rtl_init_unicode_string( &allow_key,
					L"TerminalServices-RemoteConnectionManager-AllowAppServerMode" );
				if ( kernel::rtl_compare_unicode_string(
					reinterpret_cast< unicode_string_t* >( value_name ),
					&allow_key, true ) ) {
					return nt_query_license_value_org(
						value_name, type, data, data_size, result_data_size );
				}
			}

			if ( data && data_size )
				RtlSecureZeroMemory( data, data_size );
			if ( result_data_size ) *result_data_size = 0;
			kernel::dbg_print( obf( "blocked license -> %wZ\n" ), value_name );

			return nt_status_t::access_violation;
		}

		nt_status_t( *nt_power_information_org )( std::uint32_t, void*, unsigned long, void*, unsigned long ) = nullptr;
		nt_status_t nt_power_information(
			std::uint32_t   information_level,
			void* input_buffer,
			unsigned long   input_buffer_length,
			void* output_buffer,
			unsigned long   output_buffer_length
		) {
			if ( is_epic_process( ) ) {
				auto result = nt_power_information_org(
					information_level, input_buffer, input_buffer_length,
					output_buffer, output_buffer_length );
				return result;
			}

			switch ( information_level ) {
			case 0x04: {
				auto result = nt_power_information_org(
					information_level, input_buffer, input_buffer_length,
					output_buffer, output_buffer_length );
				if ( NT_SUCCESS( result ) && output_buffer && output_buffer_length )
					RtlSecureZeroMemory( output_buffer, output_buffer_length );
				return result;
			}
			case 0x0B: {
				auto result = nt_power_information_org(
					information_level, input_buffer, input_buffer_length,
					output_buffer, output_buffer_length );
				if ( NT_SUCCESS( result ) && output_buffer &&
					output_buffer_length >= sizeof( ULONG ) * 6 ) {
					auto* cpu = reinterpret_cast< ULONG* >( output_buffer );
					auto   count = output_buffer_length / ( sizeof( ULONG ) * 6 );
					for ( std::size_t i = 0; i < count; ++i ) {
						auto* e = cpu + ( i * 6 );
						e[ 1 ] = 3000;
						e[ 2 ] = 3000;
						e[ 3 ] = 3000;
						e[ 4 ] = 0;
						e[ 5 ] = 0;
					}
				}
				return result;
			}
			default:
				break;
			}

			auto result = nt_power_information_org(
				information_level, input_buffer, input_buffer_length,
				output_buffer, output_buffer_length );
			return result;
		}

		nt_status_t( *nt_query_information_token_org )( void*, std::uint8_t, void*, unsigned long, unsigned long* ) = nullptr;
		nt_status_t nt_query_information_token(
			void* token_handle,
			std::uint8_t token_info_class,
			void* token_info,
			unsigned long token_info_length,
			unsigned long* return_length
		) {
			auto result = nt_query_information_token_org(
				token_handle, token_info_class, token_info,
				token_info_length, return_length );

			if ( result || is_epic_process( ) || token_info_class != 1 )
				return result;

			auto process = kernel::ps_get_current_process( );
			if ( !kernel::mm_is_address_valid( process ) )
				return result;

			char image_file_name[ 16 ] { };
			if ( !memory::get_image_file_name( process, image_file_name, sizeof( image_file_name ) ) )
				return result;

			const auto is_fortnite_launcher =
				strstr_lowercase( image_file_name, obf( "FortniteLaunch" ) );
			if ( is_fortnite_launcher )
				m_last_sid_time = kernel::ke_query_system_time_precise( );

			if ( !token_info || token_info_length < sizeof( token_user_t ) )
				return result;

			auto token_length = token_info_length;
			if ( return_length && *return_length && *return_length < token_length )
				token_length = *return_length;

			auto token_user = reinterpret_cast< token_user_t* >( token_info );
			auto user_sid = token_user->m_user.m_sid;

			if ( !user_sid || !kernel::mm_is_address_valid( user_sid ) )
				return result;

			std::uint8_t sid_buffer[ 68 ] { 0 };
			auto sid_size = kernel::rtl_length_sid( user_sid );
			if ( sid_size > sizeof( sid_buffer ) )
				return result;

			auto tick_count = kernel::ke_query_tick_count( );
			auto seed = static_cast< unsigned long >( tick_count.m_low_part );
			seed = kernel::rtl_random_ex( &seed );
			std::memcpy( sid_buffer, user_sid, sid_size );

			auto time_now = kernel::ke_query_system_time_precise( );
			auto last_reset = m_last_sid_time;
			const auto one_minute = 1 * 60 * 10000000;

			if ( last_reset != 0 && ( time_now - last_reset ) >= one_minute ) {
				auto sid = reinterpret_cast< sid_internal_t* >( sid_buffer );
				sid->m_revision = 1;
				sid->m_sub_authority_count = 5;
				sid->m_identifier_authority.m_value[ 0 ] = 0;
				sid->m_identifier_authority.m_value[ 1 ] = 0;
				sid->m_identifier_authority.m_value[ 2 ] = 0;
				sid->m_identifier_authority.m_value[ 3 ] = 0;
				sid->m_identifier_authority.m_value[ 4 ] = 0;
				sid->m_identifier_authority.m_value[ 5 ] = 5;
				sid->m_sub_authority[ 0 ] = 21;
				sid->m_sub_authority[ 1 ] = ( seed * 1103515245 + 12345 ) & 0x7FFFFFFF;
				sid->m_sub_authority[ 2 ] = ( seed * 1664525 + 1013904223 ) & 0x7FFFFFFF;
				sid->m_sub_authority[ 3 ] = ( seed * 214013 + 2531011 ) & 0x7FFFFFFF;
				sid->m_sub_authority[ 4 ] = 1000 + ( seed % 10000 );
				std::memcpy( user_sid, sid_buffer, sid_size );
			}

			return result;
		}

		nt_status_t( *nt_fs_control_file_org )( void*, void*, void*, void*, io_status_block_t*, unsigned long, void*, unsigned long, void*, unsigned long ) = nullptr;
		nt_status_t nt_fs_control_file(
			void* file_handle,
			void* event,
			void* apc_routine,
			void* apc_context,
			io_status_block_t* io_status_block,
			unsigned long fs_control_code,
			void* input_buffer,
			unsigned long input_buffer_length,
			void* output_buffer,
			unsigned long output_buffer_length
		) {
			auto starts_with = [ ]( unicode_string_t* name, const wchar_t* prefix ) -> bool {
				if ( !name || !name->m_buffer || !name->m_length )
					return false;
				auto prefix_byte_len = static_cast< USHORT >( wcslen( prefix ) * sizeof( wchar_t ) );
				if ( name->m_length < prefix_byte_len )
					return false;
				return RtlCompareMemory( name->m_buffer, prefix, prefix_byte_len ) == prefix_byte_len;
				};

			if ( is_epic_process( ) ) {
				return nt_fs_control_file_org(
					file_handle, event, apc_routine, apc_context,
					io_status_block, fs_control_code, input_buffer,
					input_buffer_length, output_buffer, output_buffer_length );
			}

			file_object_t* file_object = nullptr;
			auto result = kernel::ob_reference_object_by_handle(
				file_handle,
				0,
				kernel::io_file_object_type( ),
				0,
				reinterpret_cast< void** >( &file_object ),
				nullptr
			);

			if ( !result && file_object ) {
				__try {
					auto device_object = file_object->m_device_object;
					if ( device_object && kernel::mm_is_address_valid( device_object ) ) {
						unsigned long return_length = 0;
						kernel::ob_query_name_string( device_object, nullptr, 0, &return_length );

						if ( return_length ) {
							const auto alloc_size = return_length;
							auto device_name = reinterpret_cast< object_name_information_t* >(
								kernel::mm_allocate_independent_pages( alloc_size ) );

							if ( device_name ) {
								result = kernel::ob_query_name_string(
									device_object, device_name, alloc_size, &return_length );

								if ( !result &&
									device_name->name.m_buffer &&
									device_name->name.m_length ) {

									const bool is_raw_disk =
										( starts_with( &device_name->name, L"\\Device\\Harddisk" ) &&
										  !starts_with( &device_name->name, L"\\Device\\HarddiskVolume" ) ) ||
										starts_with( &device_name->name, L"\\Device\\CdRom" ) ||
										starts_with( &device_name->name, L"\\Device\\Scsi" ) ||
										starts_with( &device_name->name, L"\\Device\\Ide" ) ||
										starts_with( &device_name->name, L"\\Device\\NVMe" );

									if ( is_raw_disk ) {
										kernel::mm_free_independent_pages(
											reinterpret_cast< std::uint64_t >( device_name ),
											alloc_size );
										kernel::ob_dereference_object( file_object );

										return static_cast< nt_status_t >( 0xC0000022 );
									}


									const bool is_volume =
										starts_with( &device_name->name, L"\\Device\\HarddiskVolume" );

									if ( is_volume ) {
										constexpr std::uint32_t fsctl_get_ntfs_volume_data   = 0x000900A8;
										constexpr std::uint32_t fsctl_get_retrieval_pointers = 0x00090073;

										if ( fs_control_code == fsctl_get_retrieval_pointers ) {
											kernel::mm_free_independent_pages(
												reinterpret_cast< std::uint64_t >( device_name ),
												alloc_size );
											kernel::ob_dereference_object( file_object );
											return static_cast< nt_status_t >( 0xC0000022 );
										}

										if ( fs_control_code == fsctl_get_ntfs_volume_data ) {
											kernel::mm_free_independent_pages(
												reinterpret_cast< std::uint64_t >( device_name ),
												alloc_size );
											kernel::ob_dereference_object( file_object );

											auto status = nt_fs_control_file_org(
												file_handle, event, apc_routine, apc_context,
												io_status_block, fs_control_code, input_buffer,
												input_buffer_length, output_buffer, output_buffer_length );

											if ( !status && output_buffer &&
												output_buffer_length >= sizeof( LONGLONG ) ) {
												__try {
													if ( !m_volume_serial ) {
														LARGE_INTEGER tick_count;
														KeQueryTickCount( &tick_count );
														auto seed = static_cast< unsigned long >(
															tick_count.LowPart ^ tick_count.HighPart ^ 0xA5C35A3C );
														m_volume_serial = kernel::rtl_random_ex( &seed );
													}
													auto* vol_serial = reinterpret_cast< LONGLONG* >( output_buffer );
													*vol_serial = static_cast< LONGLONG >( m_volume_serial ) |
														( static_cast< LONGLONG >( m_volume_serial ^ 0x5A3C ) << 32 );
												}
												__except ( EXCEPTION_EXECUTE_HANDLER ) { }
											}
											return status;
										}

										constexpr std::uint32_t fsctl_query_usn_journal = 0x000900F4;
										if ( fs_control_code == fsctl_query_usn_journal ) {
											kernel::mm_free_independent_pages(
												reinterpret_cast< std::uint64_t >( device_name ),
												alloc_size );
											kernel::ob_dereference_object( file_object );

											auto status = nt_fs_control_file_org(
												file_handle, event, apc_routine, apc_context,
												io_status_block, fs_control_code, input_buffer,
												input_buffer_length, output_buffer, output_buffer_length );

											if ( NT_SUCCESS( status ) && output_buffer &&
												output_buffer_length >= sizeof( LONGLONG ) ) {
												__try {
													if ( !m_volume_serial ) {
														LARGE_INTEGER tick_count;
														KeQueryTickCount( &tick_count );
														auto seed = static_cast< unsigned long >(
															tick_count.LowPart ^ tick_count.HighPart ^ 0xA5C35A3C );
														m_volume_serial = kernel::rtl_random_ex( &seed );
													}
													auto* usn_id = reinterpret_cast< LONGLONG* >( output_buffer );
													*usn_id = static_cast< LONGLONG >( m_volume_serial ^ 0xC3A5A55A ) |
														( static_cast< LONGLONG >( m_volume_serial ^ 0x3C5AA5C3 ) << 32 );
												}
												__except ( EXCEPTION_EXECUTE_HANDLER ) { }
											}
											return status;
										}
									}

									if ( !starts_with( &device_name->name, L"\\Device\\NamedPipe" ) ) {
										disk::print(
											obf( "[pass] NtFsControlFile device=%wZ code=0x%08X input=0x%llx input_len=%lu output=0x%llx output_len=%lu" ),
											&device_name->name,
											fs_control_code,
											reinterpret_cast< std::uint64_t >( input_buffer ),
											input_buffer_length,
											reinterpret_cast< std::uint64_t >( output_buffer ),
											output_buffer_length );
									}
								}

								kernel::mm_free_independent_pages(
									reinterpret_cast< std::uint64_t >( device_name ),
									alloc_size );
							}
						}

					}
				}
				__except ( EXCEPTION_EXECUTE_HANDLER ) { }

				kernel::ob_dereference_object( file_object );
			}

			return nt_fs_control_file_org(
				file_handle, event, apc_routine, apc_context,
				io_status_block, fs_control_code, input_buffer,
				input_buffer_length, output_buffer, output_buffer_length );
		}

		nt_status_t( *nt_query_volume_information_file_org )( void*, io_status_block_t*, void*, unsigned long, std::uint32_t ) = nullptr;
		nt_status_t nt_query_volume_information_file(
			void* file_handle,
			io_status_block_t* io_status_block,
			void* fs_information,
			unsigned long length,
			std::uint32_t fs_information_class
		) {
			auto result = nt_query_volume_information_file_org(
				file_handle, io_status_block, fs_information,
				length, fs_information_class );

			if ( result || is_epic_process( ) || fs_information_class != 1 || !fs_information || length < 16 )
				return result;

			__try {
				LARGE_INTEGER tick_count { };
				KeQueryTickCount( &tick_count );
				auto seed = static_cast< unsigned long >(
					tick_count.LowPart ^ tick_count.HighPart ^ 0xA5C35A3C );

				if ( !m_volume_serial )
					m_volume_serial = kernel::rtl_random_ex( &seed );

				auto* volume_serial = reinterpret_cast< unsigned long* >(
					reinterpret_cast< std::uint8_t* >( fs_information ) + sizeof( LARGE_INTEGER ) );
				*volume_serial = m_volume_serial;
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
				return nt_status_t::access_violation;
			}

			return result;
		}

		nt_status_t( *nt_device_io_control_file_org )( void*, void*, void*, void*, void*, unsigned long, void*, unsigned long, void*, unsigned long ) = nullptr;
		nt_status_t nt_device_io_control_file(
			void* file_handle,
			void* event,
			void* apc_routine,
			void* apc_context,
			io_status_block_t* io_status_block,
			unsigned long       io_control_code,
			void* input_buffer,
			unsigned long       input_buffer_length,
			void* output_buffer,
			unsigned long       output_buffer_length
		) {
			static const wchar_t* blocked_devices[ ] = {
				L"\\Device\\Harddisk",
				L"\\Device\\CdRom",
				L"\\Device\\Scsi",
				L"\\Device\\Ide",
				L"\\Device\\NVMe",
				L"\\Device\\PhysicalMemory",
				L"\\Device\\NetBT_Tcpip_",
				L"\\Device\\NvAdminDevice",
				L"\\Device\\USBPDO",
				L"\\Device\\NTPNP_PCI",
				L"\\Device\\PcwDrv",
				L"\\Device\\Tpm",
				L"\\Device\\BTH",
				L"\\Device\\BTHUSB",
				L"\\Device\\BthPan",
				L"\\Device\\BthLeEnum",
				L"\\Device\\{DEDAE202-1D20-4C40-A6F3-A19E387AD7BE}",
				L"\\Device\\FVEControl",
			};

			static const wchar_t* ignored_devices[ ] = {
				L"\\Device\\Nsi",
				L"\\Device\\Afd",
				L"\\Device\\KsecDD",
				L"\\Device\\MountPointManager",
				L"\\Device\\DeviceApi",
				L"\\Device\\CNG",
				L"\\Device\\MMCSS",
			};

			auto starts_with = [ ]( unicode_string_t* name, const wchar_t* prefix ) -> bool {
				if ( !name || !name->m_buffer || !name->m_length )
					return false;
				auto prefix_byte_len = static_cast< USHORT >( wcslen( prefix ) * sizeof( wchar_t ) );
				if ( name->m_length < prefix_byte_len )
					return false;
				return RtlCompareMemory( name->m_buffer, prefix, prefix_byte_len ) == prefix_byte_len;
				};

			auto equals = [ ]( unicode_string_t* name, const wchar_t* value ) -> bool {
				if ( !name || !name->m_buffer )
					return false;
				auto value_byte_len = static_cast< USHORT >( wcslen( value ) * sizeof( wchar_t ) );
				if ( name->m_length != value_byte_len )
					return false;
				return RtlCompareMemory( name->m_buffer, value, value_byte_len ) == value_byte_len;
				};

			if ( is_epic_process( ) ) {
				return nt_device_io_control_file_org(
					file_handle, event, apc_routine, apc_context, io_status_block,
					io_control_code, input_buffer, input_buffer_length,
					output_buffer, output_buffer_length );
			}

			file_object_t* file_object = nullptr;
			auto result = kernel::ob_reference_object_by_handle(
				file_handle,
				0,
				kernel::io_file_object_type( ),
				0,
				reinterpret_cast< void** >( &file_object ),
				nullptr
			);

			if ( !result && file_object ) {
				__try {
					auto device_object = file_object->m_device_object;
					if ( device_object && kernel::mm_is_address_valid( device_object ) ) {
						unsigned long return_length = 0;
						kernel::ob_query_name_string( device_object, nullptr, 0, &return_length );

						if ( return_length ) {
							const auto alloc_size = return_length;
							auto device_name = reinterpret_cast< object_name_information_t* >(
								kernel::mm_allocate_independent_pages( alloc_size ) );

							if ( device_name ) {
								result = kernel::ob_query_name_string(
									device_object, device_name, alloc_size, &return_length );

								if ( !result &&
									device_name->name.m_buffer &&
									device_name->name.m_length ) {

									for ( auto* prefix : blocked_devices ) {
										if ( !starts_with( &device_name->name, prefix ) )
											continue;

										if ( starts_with( &device_name->name, L"\\Device\\HarddiskVolume" ) )
											continue;

										kernel::dbg_print(
											obf( "blocked device -> %wZ (ioctl=0x%08X)\n" ),
											&device_name->name, io_control_code );

										kernel::mm_free_independent_pages(
											reinterpret_cast< std::uint64_t >( device_name ),
											alloc_size );
										kernel::ob_dereference_object( file_object );

										return static_cast< nt_status_t >( 0xC0000022 );
									}

									if ( io_control_code == 0x000B01A8 ) {
										kernel::dbg_print(
											obf( "blocked ioctl -> 0x%08X (ioctl=%wZ)\n" ),
											io_control_code, &device_name->name );

										kernel::mm_free_independent_pages(
											reinterpret_cast< std::uint64_t >( device_name ),
											alloc_size );
										kernel::ob_dereference_object( file_object );
										return static_cast< nt_status_t >( 0xC0000010 );
									}

									switch ( io_control_code ) {
										case 0x006D0004u:
										case 0x006D0030u:
										case 0x006D0034u:
											kernel::mm_free_independent_pages(
												reinterpret_cast< std::uint64_t >( device_name ),
												alloc_size );
											kernel::ob_dereference_object( file_object );
											return static_cast< nt_status_t >( 0xC0000225 );
									}

									if ( io_control_code == 0x004D0000u ||
										io_control_code == 0x004D0018u ) {
										kernel::mm_free_independent_pages(
											reinterpret_cast< std::uint64_t >( device_name ),
											alloc_size );
										kernel::ob_dereference_object( file_object );

										auto status = nt_device_io_control_file_org(
											file_handle, event, apc_routine, apc_context,
											io_status_block, io_control_code,
											input_buffer, input_buffer_length,
											output_buffer, output_buffer_length );

										if ( NT_SUCCESS( status ) && output_buffer ) {
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
										input_buffer && input_buffer_length >= sizeof( std::uint32_t ) ) {
										std::uint32_t oid = 0;
										__try { oid = *reinterpret_cast< std::uint32_t* >( input_buffer ); }
										__except ( EXCEPTION_EXECUTE_HANDLER ) { oid = 0; }

										if ( oid == 0x01010101u || oid == 0x01010102u ) {
											kernel::mm_free_independent_pages(
												reinterpret_cast< std::uint64_t >( device_name ),
												alloc_size );
											kernel::ob_dereference_object( file_object );

											auto status = nt_device_io_control_file_org(
												file_handle, event, apc_routine, apc_context,
												io_status_block, io_control_code,
												input_buffer, input_buffer_length,
												output_buffer, output_buffer_length );

											if ( NT_SUCCESS( status ) && output_buffer &&
												output_buffer_length >= 6 ) {
												static bool mac_ready = false;
												static std::uint8_t spoof_mac[ 6 ];
												if ( !mac_ready ) {
													for ( int i = 0; i < 6; ++i )
														spoof_mac[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );
													spoof_mac[ 0 ] = ( spoof_mac[ 0 ] & 0xFCu ) | 0x02u;
													mac_ready = true;
												}
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
										starts_with( &device_name->name, L"\\Device\\HarddiskVolume" ) &&
										input_buffer && input_buffer_length >= 8 ) {

										std::uint32_t property_id = 0xFFFFFFFFu;
										std::uint32_t query_type  = 0xFFFFFFFFu;
										__try {
											auto* q = reinterpret_cast< std::uint32_t* >( input_buffer );
											property_id = q[ 0 ];
											query_type  = q[ 1 ];
										}
										__except ( EXCEPTION_EXECUTE_HANDLER ) { }

										const bool identifier_property =
											property_id == 0u ||
											property_id == 2u ||
											property_id == 3u;
										const bool standard_query = query_type == 0u;

										if ( identifier_property && standard_query ) {
											kernel::mm_free_independent_pages(
												reinterpret_cast< std::uint64_t >( device_name ),
												alloc_size );
											kernel::ob_dereference_object( file_object );

											auto status = nt_device_io_control_file_org(
												file_handle, event, apc_routine, apc_context,
												io_status_block, io_control_code,
												input_buffer, input_buffer_length,
												output_buffer, output_buffer_length );

											if ( NT_SUCCESS( status ) && output_buffer &&
												output_buffer_length >= 8 ) {
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
														for ( std::uint32_t i = 20;
															i < output_buffer_length; ++i )
															buf[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );
													}
												}
												__except ( EXCEPTION_EXECUTE_HANDLER ) { }
											}
											return status;
										}
									}

									if ( starts_with( &device_name->name, L"\\Device\\EasyAntiCheat_EOS" ) ) {

										constexpr auto query_driver_info_code = 0x00226003;
										constexpr auto get_driver_status_code = 0x00226013;
										constexpr auto session_handshake_code = 0x0022E007;
										constexpr auto protect_region_code = 0x0022E01F;
										constexpr auto register_protected_regions = 0x0022E04B;
										constexpr auto challenge_response_code = 0x0022E04F;
										constexpr auto enable_kernel_protection = 0x0022E067;
										constexpr auto heartbeat_code = 0x0022E023;
										constexpr auto heartbeat_extended_code = 0x0022E053;
										constexpr auto heartbeat_small_code = 0x0022E05B;
										constexpr auto heartbeat_session_code = 0x0022E017;
										constexpr auto inject_game_dependency = 0x0022605F;
										constexpr auto inject_dll_dependency = 0x0022E06B;
										constexpr auto driver_init_code = 0x0022E00F;
										constexpr auto session_notify_code = 0x0022E01B;
										constexpr auto policy_update_code = 0x0022E047;

										switch ( io_control_code ) {
											
											
											
											

											
											

											
											
											

											
											
											
										case get_driver_status_code: {
											kernel::mm_free_independent_pages(
												reinterpret_cast< std::uint64_t >( device_name ),
												alloc_size );

											if ( output_buffer && output_buffer_length >= sizeof( std::uint32_t ) ) {
												*reinterpret_cast< std::uint32_t* >( output_buffer ) = 4;

												*reinterpret_cast< unsigned long* >( &io_status_block->m_status ) = 0x00000000;
												*reinterpret_cast< unsigned long* >( &io_status_block->m_information ) = sizeof( std::uint32_t );
											}

											kernel::ob_dereference_object( file_object );
											return nt_status_t::success;
										} break;
										case protect_region_code: {
											kernel::mm_free_independent_pages(
												reinterpret_cast< std::uint64_t >( device_name ),
												alloc_size );

											if ( !output_buffer || !output_buffer_length ) {
												kernel::ob_dereference_object( file_object );
												return nt_status_t::access_violation;
											}

											auto* input = reinterpret_cast< std::uint8_t* >( input_buffer );
											auto* output = reinterpret_cast< std::uint8_t* >( output_buffer );
											auto  version = *reinterpret_cast< std::uint32_t* >( input );

											if ( version == 1 && input_buffer_length == 36 && output_buffer_length == 36 ) {
												std::memcpy( output, input, 36 );

												auto& status_flag = *reinterpret_cast< std::uint16_t* >( output + 0x0E );
												auto& mem_size = *reinterpret_cast< std::uint32_t* >( output + 0x14 );

												status_flag = 0x0001;
												mem_size = ( mem_size + 0xFFF ) & ~0xFFF;
											}
											else if ( version == 2 && input_buffer_length == 44 && output_buffer_length == 44 ) {
												std::memcpy( output, input, 44 );

												auto  size_field = *reinterpret_cast< std::uint32_t* >( input + 0x1C );
												auto& out_field = *reinterpret_cast< std::uint32_t* >( output + 0x24 );

												out_field = size_field;
											}
											else {
												kernel::ob_dereference_object( file_object );
												return nt_device_io_control_file_org(
													file_handle, event, apc_routine, apc_context, io_status_block,
													io_control_code, input_buffer, input_buffer_length,
													output_buffer, output_buffer_length );
											}

											*reinterpret_cast< unsigned long* >( &io_status_block->m_status ) = 0x00000000;
											*reinterpret_cast< unsigned long* >( &io_status_block->m_information ) = output_buffer_length;

											kernel::ob_dereference_object( file_object );
											return nt_status_t::success;
										} break;
										case register_protected_regions: {
											kernel::mm_free_independent_pages(
												reinterpret_cast< std::uint64_t >( device_name ),
												alloc_size );

											if ( output_buffer && output_buffer_length == input_buffer_length ) {
												std::memcpy( output_buffer, input_buffer, output_buffer_length );

												*reinterpret_cast< unsigned long* >( &io_status_block->m_status ) = 0x00000000;
												*reinterpret_cast< unsigned long* >( &io_status_block->m_information ) = output_buffer_length;
											}

											kernel::ob_dereference_object( file_object );
											return nt_status_t::success;
										} break;
										case enable_kernel_protection: {
											kernel::mm_free_independent_pages(
												reinterpret_cast< std::uint64_t >( device_name ),
												alloc_size );

											if ( !output_buffer || output_buffer_length < sizeof( std::uint32_t ) ) {
												kernel::ob_dereference_object( file_object );
												return nt_status_t::access_violation;
											}

											std::memset( output_buffer, 0, output_buffer_length );

											*reinterpret_cast< unsigned long* >( &io_status_block->m_status ) = 0x00000000;
											*reinterpret_cast< unsigned long* >( &io_status_block->m_information ) = output_buffer_length;

											kernel::ob_dereference_object( file_object );
											return nt_status_t::success;
										} break;
										case driver_init_code:
										case session_notify_code:
										case policy_update_code: {
											kernel::mm_free_independent_pages(
												reinterpret_cast< std::uint64_t >( device_name ),
												alloc_size );

											if ( input_buffer && input_buffer_length )
												RtlSecureZeroMemory( input_buffer, input_buffer_length );
											if ( output_buffer && output_buffer_length )
												RtlSecureZeroMemory( output_buffer, output_buffer_length );

											*reinterpret_cast< unsigned long* >( &io_status_block->m_status ) = 0x00000000;
											*reinterpret_cast< unsigned long* >( &io_status_block->m_information ) = 0;

											kernel::ob_dereference_object( file_object );
											return nt_status_t::success;
										} break;
										case inject_game_dependency:
										case inject_dll_dependency:
										case session_handshake_code:
										case challenge_response_code:
										case heartbeat_code:
										case heartbeat_extended_code:
										case heartbeat_small_code:
										case heartbeat_session_code: {
											kernel::ob_dereference_object( file_object );

											kernel::mm_free_independent_pages(
												reinterpret_cast< std::uint64_t >( device_name ),
												alloc_size );

											return nt_device_io_control_file_org(
												file_handle, event, apc_routine, apc_context, io_status_block,
												io_control_code, input_buffer, input_buffer_length,
												output_buffer, output_buffer_length );
										} break;
										}

										kernel::dbg_print(
											obf( "[EAC_EOS] unknown ioctl=0x%08X\n" ),
											io_control_code );

										if ( input_buffer_length >= 0x1000 ) {
											kernel::mm_free_independent_pages(
												reinterpret_cast< std::uint64_t >( device_name ),
												alloc_size );

											return nt_device_io_control_file_org(
												file_handle, event, apc_routine, apc_context, io_status_block,
												io_control_code, input_buffer, input_buffer_length,
												output_buffer, output_buffer_length );
										}

										if ( input_buffer && input_buffer_length ) {
											kernel::dbg_print( obf( "[EAC_EOS] input_buffer len=%lu\n" ), input_buffer_length );
											auto* bytes = reinterpret_cast< unsigned char* >( input_buffer );
											for ( unsigned long i = 0; i < input_buffer_length; i += 16 ) {
												kernel::dbg_print(
													obf( "[EAC_EOS]  +%04lX: %02X %02X %02X %02X %02X %02X %02X %02X  %02X %02X %02X %02X %02X %02X %02X %02X\n" ),
													i,
													i + 0 < input_buffer_length ? bytes[ i + 0 ] : 0,
													i + 1 < input_buffer_length ? bytes[ i + 1 ] : 0,
													i + 2 < input_buffer_length ? bytes[ i + 2 ] : 0,
													i + 3 < input_buffer_length ? bytes[ i + 3 ] : 0,
													i + 4 < input_buffer_length ? bytes[ i + 4 ] : 0,
													i + 5 < input_buffer_length ? bytes[ i + 5 ] : 0,
													i + 6 < input_buffer_length ? bytes[ i + 6 ] : 0,
													i + 7 < input_buffer_length ? bytes[ i + 7 ] : 0,
													i + 8 < input_buffer_length ? bytes[ i + 8 ] : 0,
													i + 9 < input_buffer_length ? bytes[ i + 9 ] : 0,
													i + 10 < input_buffer_length ? bytes[ i + 10 ] : 0,
													i + 11 < input_buffer_length ? bytes[ i + 11 ] : 0,
													i + 12 < input_buffer_length ? bytes[ i + 12 ] : 0,
													i + 13 < input_buffer_length ? bytes[ i + 13 ] : 0,
													i + 14 < input_buffer_length ? bytes[ i + 14 ] : 0,
													i + 15 < input_buffer_length ? bytes[ i + 15 ] : 0
												);
											}
										}

										kernel::mm_free_independent_pages(
											reinterpret_cast< std::uint64_t >( device_name ),
											alloc_size );

										auto status = nt_device_io_control_file_org(
											file_handle, event, apc_routine, apc_context, io_status_block,
											io_control_code, input_buffer, input_buffer_length,
											output_buffer, output_buffer_length );

										if ( output_buffer && output_buffer_length ) {
											kernel::dbg_print( obf( "[EAC_EOS] output_buffer len=%lu status=0x%08X\n" ),
												output_buffer_length, static_cast< unsigned long >( status ) );
											auto* bytes = reinterpret_cast< unsigned char* >( output_buffer );
											for ( unsigned long i = 0; i < output_buffer_length; i += 16 ) {
												kernel::dbg_print(
													obf( "[EAC_EOS]  +%04lX: %02X %02X %02X %02X %02X %02X %02X %02X  %02X %02X %02X %02X %02X %02X %02X %02X\n" ),
													i,
													i + 0 < output_buffer_length ? bytes[ i + 0 ] : 0,
													i + 1 < output_buffer_length ? bytes[ i + 1 ] : 0,
													i + 2 < output_buffer_length ? bytes[ i + 2 ] : 0,
													i + 3 < output_buffer_length ? bytes[ i + 3 ] : 0,
													i + 4 < output_buffer_length ? bytes[ i + 4 ] : 0,
													i + 5 < output_buffer_length ? bytes[ i + 5 ] : 0,
													i + 6 < output_buffer_length ? bytes[ i + 6 ] : 0,
													i + 7 < output_buffer_length ? bytes[ i + 7 ] : 0,
													i + 8 < output_buffer_length ? bytes[ i + 8 ] : 0,
													i + 9 < output_buffer_length ? bytes[ i + 9 ] : 0,
													i + 10 < output_buffer_length ? bytes[ i + 10 ] : 0,
													i + 11 < output_buffer_length ? bytes[ i + 11 ] : 0,
													i + 12 < output_buffer_length ? bytes[ i + 12 ] : 0,
													i + 13 < output_buffer_length ? bytes[ i + 13 ] : 0,
													i + 14 < output_buffer_length ? bytes[ i + 14 ] : 0,
													i + 15 < output_buffer_length ? bytes[ i + 15 ] : 0
												);
											}
										}

										kernel::ob_dereference_object( file_object );
										return status;
									}
								}

								if ( starts_with( &device_name->name, L"\\Device\\Nsi" ) ) {
									kernel::mm_free_independent_pages(
										reinterpret_cast< std::uint64_t >( device_name ),
										alloc_size );
									kernel::ob_dereference_object( file_object );

									auto status = nt_device_io_control_file_org(
										file_handle, event, apc_routine, apc_context, io_status_block,
										io_control_code, input_buffer, input_buffer_length,
										output_buffer, output_buffer_length );

									if ( NT_SUCCESS( status ) && output_buffer &&
										output_buffer_length >= 0x468 &&
										kernel::mm_is_address_valid( output_buffer ) )
										hwid::patch_interface_table( output_buffer, output_buffer_length );

									return status;
								}

								bool ignored_device = false;
								for ( auto* device : ignored_devices ) {
									if ( !equals( &device_name->name, device ) )
										continue;

									ignored_device = true;
									break;
								}

								if ( ignored_device ) {
									kernel::mm_free_independent_pages(
										reinterpret_cast< std::uint64_t >( device_name ),
										alloc_size );
									kernel::ob_dereference_object( file_object );

									return nt_device_io_control_file_org(
										file_handle, event, apc_routine, apc_context, io_status_block,
										io_control_code, input_buffer, input_buffer_length,
										output_buffer, output_buffer_length );
								}

								kernel::dbg_print(
									obf( "unblocked device -> %wZ (ioctl=0x%08X)\n" ),
									&device_name->name, io_control_code );

								kernel::mm_free_independent_pages(
									reinterpret_cast< std::uint64_t >( device_name ),
									alloc_size );
							}
						}
					}
				}
				__except ( EXCEPTION_EXECUTE_HANDLER ) { }

				kernel::ob_dereference_object( file_object );
			}

			return nt_device_io_control_file_org(
				file_handle, event, apc_routine, apc_context, io_status_block,
				io_control_code, input_buffer, input_buffer_length,
				output_buffer, output_buffer_length );
		}

		nt_status_t( *nt_write_virtual_memory_org )( void*, void*, void*, std::size_t, std::size_t* ) = nullptr;
		nt_status_t nt_write_virtual_memory(
			void* process_handle,
			void* base_address,
			void* buffer,
			std::size_t number_of_bytes_to_write,
			std::size_t* number_of_bytes_written
		) {
			if ( is_epic_process( ) ) {
				return nt_write_virtual_memory_org(
					process_handle,
					base_address,
					buffer,
					number_of_bytes_to_write,
					number_of_bytes_written );
			}

			if ( buffer ) {
				if ( number_of_bytes_written )
					*number_of_bytes_written = 0;

				return nt_status_t::access_violation;
			}

			return nt_write_virtual_memory_org(
				process_handle,
				base_address,
				buffer,
				number_of_bytes_to_write,
				number_of_bytes_written );
		}

		nt_status_t( *nt_query_system_env_value_org )( UNICODE_STRING*, wchar_t*, unsigned long, unsigned long* ) = nullptr;
		nt_status_t nt_query_system_env_value(
			UNICODE_STRING* variable_name,
			wchar_t* variable_value,
			unsigned long value_length,
			unsigned long* return_length
		) {
			disk::print(
				obf( "[spoof] NtQuerySystemEnvironmentValue name=%wZ len=%lu" ),
				variable_name,
				value_length );

			if ( !is_epic_process( ) && is_sensitive_name( variable_name ) ) {
				if ( variable_value && value_length )
					RtlSecureZeroMemory( variable_value, value_length );
				if ( return_length ) *return_length = 0;
				kernel::dbg_print( obf( "[spoof] NtQuerySystemEnvironmentValue blocked -> %wZ\n" ), variable_name );
				return nt_status_t::access_violation;
			}

			auto result = nt_query_system_env_value_org(
				variable_name, variable_value, value_length, return_length );

			return result;
		}

		static constexpr unsigned char hwid_proto_guid[ 12 ] = {
			0x81u, 0xBBu, 0x7Au, 0x36u, 0x44u, 0x98u, 0xF1u, 0x35u,
			0xADu, 0x32u, 0x98u, 0xF0u
		};

		static constexpr unsigned long hwid_spoof_max = 32u;
		static bool                hwid_spoof_ready;
		static unsigned char       hwid_spoof[ hwid_spoof_max ][ 16 ];

		static constexpr unsigned long hwid_port_max = 64u;
		static void* hwid_port_table[ hwid_port_max ];
		static long   hwid_port_count;

		static bool guid_match( const unsigned char* a, const unsigned char* b, unsigned long n ) {
			for ( unsigned long i = 0; i < n; ++i )
				if ( a[ i ] != b[ i ] )
					return false;
			return true;
		}

		static void ensure_hwid_spoof_initialized( ) {
			if ( hwid_spoof_ready ) return;
			for ( unsigned long i = 0u; i < hwid_spoof_max; ++i ) {
				for ( unsigned long j = 0u; j < 16u; ++j )
					hwid_spoof[ i ][ j ] = static_cast< unsigned char >( kernel::rand( ) );
				hwid_spoof[ i ][ 6 ] = ( hwid_spoof[ i ][ 6 ] & 0x0Fu ) | 0x40u;
				hwid_spoof[ i ][ 8 ] = ( hwid_spoof[ i ][ 8 ] & 0x3Fu ) | 0x80u;
			}
			hwid_spoof_ready = true;
		}

		static bool is_hwid_port( void* handle ) {
			long cnt = hwid_port_count;
			for ( long i = 0; i < cnt; ++i )
				if ( hwid_port_table[ i ] == handle )
					return true;
			return false;
		}

		static void register_hwid_port( void* handle ) {
			if ( is_hwid_port( handle ) )
				return;
			ensure_hwid_spoof_initialized( );
			long idx = hwid_port_count;
			if ( idx < static_cast< long >( hwid_port_max ) ) {
				hwid_port_table[ idx ] = handle;
				hwid_port_count = idx + 1;
			}
		}

		nt_status_t( *nt_alpc_send_wait_receive_port_org )( void*, unsigned long, void*, void*, void*, unsigned long*, void*, LARGE_INTEGER* ) = nullptr;
		nt_status_t nt_alpc_send_wait_receive_port(
			void* port_handle,
			unsigned long flags,
			void* send_message,
			void* send_message_attributes,
			void* receive_message,
			unsigned long* buffer_length,
			void* receive_message_attributes,
			LARGE_INTEGER* timeout ) {

			constexpr auto port_msg_hdr = 0x28u;

			if ( send_message && kernel::mm_is_address_valid( send_message ) ) {
				__try {
					auto* hdr = reinterpret_cast< unsigned char* >( send_message );
					unsigned short data_len = *reinterpret_cast< unsigned short* >( hdr );

					if ( data_len >= 0x18u ) {
						auto* payload = hdr + port_msg_hdr;
						unsigned long send_type = *reinterpret_cast< unsigned long* >( payload );

						if ( send_type == 1u &&
							guid_match( payload + 0x0Cu, hwid_proto_guid, sizeof( hwid_proto_guid ) ) )
							register_hwid_port( port_handle );
					}
				}
				__except ( EXCEPTION_EXECUTE_HANDLER ) { }
			}

			const auto result = nt_alpc_send_wait_receive_port_org( port_handle, flags, send_message,
				send_message_attributes, receive_message, buffer_length,
				receive_message_attributes, timeout );

			if ( !result && is_hwid_port( port_handle ) &&
				receive_message && kernel::mm_is_address_valid( receive_message ) ) {
				__try {
					unsigned long recv_len = ( buffer_length && kernel::mm_is_address_valid( buffer_length ) )
						? *buffer_length : 0ul;
					if ( recv_len >= port_msg_hdr ) {
						auto* hdr = reinterpret_cast< unsigned char* >( receive_message );
						unsigned short data_len = *reinterpret_cast< unsigned short* >( hdr );

						if ( data_len >= 0x2Cu ) {
							auto* payload = hdr + port_msg_hdr;
							unsigned long resp_type = *reinterpret_cast< unsigned long* >( payload );
							unsigned long item_index = *reinterpret_cast< unsigned long* >( payload + 0x0Cu );

							if ( resp_type == 3u && item_index >= 1u &&
								item_index < hwid_spoof_max ) {
								auto* hwid_ptr = payload + 0x1Cu;
								bool has_data = false;
								for ( unsigned long k = 0u; k < 16u; ++k ) {
									if ( hwid_ptr[ k ] != 0u ) { has_data = true; break; }
								}
								if ( has_data ) {
									ensure_hwid_spoof_initialized( );
									for ( unsigned long i = 0u; i < 16u; ++i )
										hwid_ptr[ i ] = hwid_spoof[ item_index ][ i ];
								}
							}
						}
					}
				}
				__except ( EXCEPTION_EXECUTE_HANDLER ) { }
			}

			return result;
		}

		bool create( ) {
			auto nt_allocate_uuids_state = etw::create_hook(
				obf( "NtAllocateUuids" ), nt_allocate_uuids, &nt_allocate_uuids_org );
			if ( !nt_allocate_uuids_state ) { kernel::dbg_print( obf( "failed to hook NtAllocateUuids\n" ) ); return false; }

			auto nt_query_license_state = etw::create_hook(
				obf( "NtQueryLicenseValue" ), nt_query_license_value, &nt_query_license_value_org );
			if ( !nt_query_license_state ) { kernel::dbg_print( obf( "failed to hook NtQueryLicenseValue\n" ) ); return false; }

			auto nt_power_info_state = etw::create_hook(
				obf( "NtPowerInformation" ), nt_power_information, &nt_power_information_org );
			if ( !nt_power_info_state ) { kernel::dbg_print( obf( "failed to hook NtPowerInformation\n" ) ); return false; }

			auto nt_query_volume_state = etw::create_hook(
				obf( "NtQueryVolumeInformationFile" ), nt_query_volume_information_file, &nt_query_volume_information_file_org );
			if ( !nt_query_volume_state ) { kernel::dbg_print( obf( "failed to hook NtQueryVolumeInformationFile\n" ) ); return false; }

			auto nt_fs_control_state = etw::create_hook(
				obf( "NtFsControlFile" ), nt_fs_control_file, &nt_fs_control_file_org );
			if ( !nt_fs_control_state ) { kernel::dbg_print( obf( "failed to hook NtFsControlFile\n" ) ); return false; }

			auto nt_query_env_state = etw::create_hook(
				obf( "NtQuerySystemEnvironmentValue" ), nt_query_system_env_value, &nt_query_system_env_value_org );
			if ( !nt_query_env_state ) { kernel::dbg_print( obf( "failed to hook NtQuerySystemEnvironmentValue\n" ) ); return false; }

			auto nt_query_value_key_state = etw::create_hook(
				obf( "NtQueryValueKey" ), nt_query_value_key, &nt_query_value_key_org );
			if ( !nt_query_value_key_state ) { kernel::dbg_print( obf( "failed to hook NtQueryValueKey\n" ) ); return false; }

			auto nt_device_io_state = etw::create_hook(
				obf( "NtDeviceIoControlFile" ), nt_device_io_control_file, &nt_device_io_control_file_org );
			if ( !nt_device_io_state ) { kernel::dbg_print( obf( "failed to hook NtDeviceIoControlFile\n" ) ); return false; }

			//auto nt_alpc_state = etw::create_hook(
			//    obf( "NtAlpcSendWaitReceivePort" ), nt_alpc_send_wait_receive_port, &nt_alpc_send_wait_receive_port_org );
			//if ( !nt_alpc_state ) { kernel::dbg_print( obf( "failed to hook NtAlpcSendWaitReceivePort\n" ) ); return false; }

			return true;
		}
	}
}