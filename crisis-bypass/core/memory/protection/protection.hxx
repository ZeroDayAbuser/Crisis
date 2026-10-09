#pragma once

namespace protection {
	namespace handles {
		static USHORT        s_file_type_idx = 0;
		static USHORT        s_section_type_idx = 0;
		static std::uint64_t s_our_section_object = 0;
		static std::wstring  m_image_path {};



		static constexpr std::uint64_t obj_hdr_off_pointer_count = 0x00;
		static constexpr std::uint64_t obj_hdr_off_handle_count  = 0x08;
		static constexpr std::uint64_t obj_hdr_size              = 0x30;

		static constexpr std::uint32_t strip_mask =
			PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
			PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION | PROCESS_SUSPEND_RESUME;

		static constexpr std::uint32_t thread_strip_mask =
			THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION;

		static auto get_nt_image_path( ) -> std::wstring {
			ULONG needed = 0;
			NtQueryInformationProcess( GetCurrentProcess( ),
				static_cast< PROCESSINFOCLASS >( 27 ), nullptr, 0, &needed );
			if ( !needed ) return {};
			std::vector< BYTE > buf( needed + sizeof( UNICODE_STRING ) );
			if ( NtQueryInformationProcess( GetCurrentProcess( ),
				static_cast< PROCESSINFOCLASS >( 27 ),
				buf.data( ), static_cast< ULONG >( buf.size( ) ), &needed ) < 0 )
				return {};
			auto* us = reinterpret_cast< UNICODE_STRING* >( buf.data( ) );
			if ( !us->Buffer || !us->Length ) return {};
			return std::wstring( us->Buffer, us->Length / sizeof( wchar_t ) );
		}

		static auto init_type_indices( ) -> void {
			auto section_h = CreateFileMappingW( INVALID_HANDLE_VALUE, nullptr,
				PAGE_READWRITE, 0, 0x1000, nullptr );

			wchar_t exe_path[ MAX_PATH ] {};
			GetModuleFileNameW( nullptr, exe_path, MAX_PATH );
			auto file_h = CreateFileW( exe_path, 0,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr, OPEN_EXISTING, 0, nullptr );

			const auto our_pid = GetCurrentProcessId( );
			unsigned long size  = 0x20000;
			std::unique_ptr< std::uint8_t[] > buf;
			std::int32_t status = 0;

			do {
				buf    = std::make_unique< std::uint8_t[] >( size );
				status = NtQuerySystemInformation(
					static_cast< SYSTEM_INFORMATION_CLASS >( 64 ),
					buf.get( ), size, &size );
				if ( status == 0xC0000004L ) size += 0x10000;
			} while ( status == 0xC0000004L );

			if ( status >= 0 ) {
				auto* list = reinterpret_cast< handle_list_t* >( buf.get( ) );
				for ( auto i = 0; i < static_cast< int >( list->m_count ); ++i ) {
					auto& e = list->m_entries[ i ];
					if ( e.m_unique_process_id != our_pid ) continue;
					if ( file_h != INVALID_HANDLE_VALUE &&
						 e.m_handle_value == reinterpret_cast< ULONG_PTR >( file_h ) )
						s_file_type_idx = e.m_object_type_index;
					if ( section_h &&
						 e.m_handle_value == reinterpret_cast< ULONG_PTR >( section_h ) )
						s_section_type_idx = e.m_object_type_index;
				}
			}

			if ( section_h ) CloseHandle( section_h );
			if ( file_h != INVALID_HANDLE_VALUE ) CloseHandle( file_h );
		}




		static auto decode_object_header( std::uint64_t entry_value ) -> std::uint64_t {
			if ( !entry_value ) return 0;
			auto ptr = ( entry_value >> 0x10 ) & 0x0000FFFFFFFFFFFFull;
			ptr |= 0xFFFF000000000000ull;
			return ptr & ~0xFull;
		}

		static auto zero_handle_entry( std::uint64_t entry_va ) -> void {
			auto zero32 = static_cast< std::uint32_t >( 0 );
			memory::write_system( entry_va + 0x8, &zero32, sizeof( zero32 ) );
		}

		static auto file_obj_is_our_image( std::uint64_t file_obj ) -> bool {
			if ( !file_obj || m_image_path.empty( ) ) return false;

			USHORT len = 0;
			if ( !memory::read_system( file_obj + 0x58, &len, sizeof( len ) ) || len > 1024 )
				return false;

			std::uint64_t buf_ptr = 0;
			if ( !memory::read_system( file_obj + 0x60, &buf_ptr, sizeof( buf_ptr ) ) )
				return false;

			std::wstring name( len / sizeof( wchar_t ), L'\0' );
			if ( !memory::read_system( buf_ptr, name.data( ), len ) )
				return false;

			const auto& full = m_image_path;
			if ( name.size( ) > full.size( ) ) return false;
			return _wcsicmp(
				full.c_str( ) + ( full.size( ) - name.size( ) ),
				name.c_str( ) ) == 0;
		}

		static auto get_handle_entry_va(
			std::uint64_t object_table, ULONG_PTR handle_value ) -> std::uint64_t {
			const auto off = g_scanner->get_member( obf( "_HANDLE_TABLE" ), obf( "TableCode" ) );
			if ( !off ) return 0;

			std::uint64_t table_code = 0;
			if ( !memory::read_system( object_table + off, &table_code, sizeof( table_code ) ) )
				return 0;

			const auto level = static_cast< std::uint8_t >( table_code & 0x3 );
			table_code &= ~0x3ull;

			const auto idx0 = ( handle_value >> 2  ) & 0xFF;
			const auto idx1 = ( handle_value >> 10 ) & 0xFF;
			const auto idx2 = ( handle_value >> 18 ) & 0xFF;

			std::uint64_t leaf = 0;

			if ( level == 0 ) {
				leaf = table_code;
			}
			else if ( level == 1 ) {
				std::uint64_t ptr = 0;
				if ( !memory::read_system( table_code + idx1 * 8, &ptr, sizeof( ptr) ) )
					return 0;
				leaf = ptr & ~0x3ull;
			}
			else {
				std::uint64_t mid = 0;
				if ( !memory::read_system( table_code + idx2 * 8, &mid, sizeof( mid ) ) )
					return 0;
				mid &= ~0x3ull;

				std::uint64_t ptr = 0;
				if ( !memory::read_system( mid + idx1 * 8, &ptr, sizeof( ptr ) ) )
					return 0;
				leaf = ptr & ~0x3ull;
			}

			if ( !leaf ) return 0;
			return leaf + idx0 * 0x10;
		}

		static void strip_handles( std::uint32_t process_id ) {
			const auto off_section_object = g_scanner->get_member( obf( "_EPROCESS" ), obf( "SectionObject" ) );
			const auto off_object_table   = g_scanner->get_member( obf( "_EPROCESS" ), obf( "ObjectTable" ) );
			if ( m_image_path.empty( ) )
				m_image_path = get_nt_image_path( );

			auto protected_process = process::find_eprocess( process_id );
			if ( !protected_process ) {
				logging::print( obf( "failed to protect current process" ) );
				return;
			}

			const auto loader_pid = static_cast< ULONG_PTR >(
				utility::find_pid( obf( L"crisis-loader.exe" ) ) );

			while ( true ) {
				constexpr auto max_thread_count = 128;

				HANDLE        our_thread_handles[ max_thread_count ] {};
				int           our_thread_count  = 0;
				std::uint64_t our_ethreads[ max_thread_count ] {};
				int           our_ethread_count = 0;

				unsigned long size   = 0x40000;
				std::unique_ptr< std::uint8_t[] > buffer;
				std::int32_t  result = 0;

				do {
					buffer = std::make_unique< std::uint8_t[] >( size );
					result = NtQuerySystemInformation(
						static_cast< SYSTEM_INFORMATION_CLASS >( 64 ),
						buffer.get( ), size, &size );
					if ( result == 0xC0000004L ) size += 0x10000;
				} while ( result == 0xC0000004L );

				if ( result < 0 ) {
					std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
					continue;
				}

				auto snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
				if ( snapshot != INVALID_HANDLE_VALUE ) {
					THREADENTRY32 te { sizeof( te ) };
					if ( Thread32First( snapshot, &te ) ) {
						do {
							if ( te.th32OwnerProcessID != process_id ) continue;
							if ( our_thread_count >= max_thread_count ) break;
							auto handle = OpenThread(
								THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID );
							if ( handle )
								our_thread_handles[ our_thread_count++ ] = handle;
						} while ( Thread32Next( snapshot, &te ) );
					}
					CloseHandle( snapshot );
				}

				auto* list = reinterpret_cast< handle_list_t* >( buffer.get( ) );

				if ( !s_file_type_idx || !s_section_type_idx )
					init_type_indices( );

				if ( !s_our_section_object && off_section_object && *protected_process )
					memory::read_system(
						*protected_process + off_section_object,
						&s_our_section_object, sizeof( s_our_section_object ) );

				for ( auto i = 0;
					  i < static_cast< int >( list->m_count ) && our_ethread_count < max_thread_count;
					  ++i ) {
					auto& entry = list->m_entries[ i ];
					if ( entry.m_unique_process_id != process_id ) continue;

					for ( auto j = 0; j < our_thread_count; ++j ) {
						if ( !our_thread_handles[ j ] ) continue;
						if ( entry.m_handle_value ==
							 reinterpret_cast< ULONG_PTR >( our_thread_handles[ j ] ) ) {
							our_ethreads[ our_ethread_count++ ] =
								reinterpret_cast< std::uint64_t >( entry.m_object );
							our_thread_handles[ j ] = nullptr;
							break;
						}
					}
				}

				for ( auto i = 0; i < our_thread_count; ++i )
					if ( our_thread_handles[ i ] )
						CloseHandle( our_thread_handles[ i ] );

				for ( auto i = 0; i < static_cast< int >( list->m_count ); ++i ) {
					auto& entry = list->m_entries[ i ];
					if ( entry.m_unique_process_id == process_id ) continue;
					if ( loader_pid && entry.m_unique_process_id == loader_pid ) continue;

					auto object = reinterpret_cast< std::uint64_t >( entry.m_object );

					bool should_zero = false;
					if ( s_section_type_idx &&
						 entry.m_object_type_index == s_section_type_idx &&
						 s_our_section_object )
						should_zero = ( object == s_our_section_object );
					else if ( s_file_type_idx &&
						      entry.m_object_type_index == s_file_type_idx )
						should_zero = file_obj_is_our_image( object );

					if ( should_zero ) {
						auto owner = process::find_eprocess(
							static_cast< std::uint32_t >( entry.m_unique_process_id ) );
						if ( !owner || !*owner ) continue;

						std::uint64_t object_table = 0;
						if ( !memory::read_system( *owner + off_object_table, &object_table, sizeof( object_table ) ) )
							continue;

						auto entry_va = get_handle_entry_va( object_table, entry.m_handle_value );
						if ( !entry_va ) continue;

						zero_handle_entry( entry_va );
						continue;
					}

					std::uint32_t mask = 0;
					if ( object == *protected_process ) {
						mask = strip_mask;
					}
					else {
						for ( auto j = 0; j < our_ethread_count; ++j ) {
							if ( our_ethreads[ j ] == object ) {
								mask = thread_strip_mask;
								break;
							}
						}
					}

					if ( !mask || !( entry.m_granted_access & mask ) ) continue;

					auto owner = process::find_eprocess(
						static_cast< std::uint32_t >( entry.m_unique_process_id ) );
					if ( !owner || !*owner ) continue;

					std::uint64_t object_table = 0;
					if ( !memory::read_system( *owner + off_object_table, &object_table, sizeof( object_table ) ) )
						continue;

					auto entry_va = get_handle_entry_va( object_table, entry.m_handle_value );
					if ( !entry_va ) continue;

					std::uint32_t granted = 0;
					if ( !memory::read_system( entry_va + 0x8, &granted, sizeof( granted ) ) ) continue;
					if ( !( granted & mask ) ) continue;

					auto new_access = granted & ~mask;
					if ( new_access == granted ) continue;

					memory::write_system( entry_va + 0x8, &new_access, sizeof( new_access ) );
				}

				std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
			}
		}

		template < std::size_t N >
		bool strip( const std::array< std::uint32_t, N >& pids,
			const char* section_name ) {
			const auto off_object_table = g_scanner->get_member( obf( "_EPROCESS" ), obf( "ObjectTable" ) );

			const int wlen = MultiByteToWideChar( CP_UTF8, 0, section_name, -1, nullptr, 0 );
			std::wstring section_wname( static_cast< std::size_t >( wlen - 1 ), L'\0' );
			MultiByteToWideChar( CP_UTF8, 0, section_name, -1, section_wname.data( ), wlen );

			HANDLE h_section = nullptr;
			for ( const auto* prefix : { L"Global\\", L"Local\\", L"" } ) {
				h_section = OpenFileMappingW( FILE_MAP_READ, FALSE,
					( std::wstring( prefix ) + section_wname ).c_str( ) );
				if ( h_section ) break;
			}
			if ( !h_section ) return false;

			ULONG buf_size = 0x40000;
			std::unique_ptr< std::uint8_t[ ] > buf;
			NTSTATUS status;
			do {
				buf = std::make_unique< std::uint8_t[ ] >( buf_size );
				status = NtQuerySystemInformation(
					static_cast< SYSTEM_INFORMATION_CLASS >( 64 ),
					buf.get( ), buf_size, &buf_size );
				if ( status == static_cast< NTSTATUS >( 0xC0000004 ) )
					buf_size += 0x10000;
			} while ( status == static_cast< NTSTATUS >( 0xC0000004 ) );

			if ( status < 0 ) { CloseHandle( h_section ); return false; }

			auto* list = reinterpret_cast< sys_handle_list_t* >( buf.get( ) );
			const auto our_pid = static_cast< ULONG_PTR >( GetCurrentProcessId( ) );

			PVOID target_object = nullptr;
			for ( ULONG_PTR i = 0; i < list->m_count; ++i ) {
				auto& e = list->m_entries[ i ];
				if ( e.m_pid != our_pid ) continue;
				if ( reinterpret_cast< HANDLE >( e.m_handle ) == h_section ) {
					target_object = e.m_object;
					break;
				}
			}

			const HANDLE h_section_val = h_section;
			CloseHandle( h_section );

			if ( !target_object )
				return false;

			for ( const auto pid : pids ) {
				const auto upid = static_cast< ULONG_PTR >( pid );
				bool       result = false;

				for ( ULONG_PTR i = 0; i < list->m_count; ++i ) {
					auto& e = list->m_entries[ i ];
					if ( e.m_pid != upid ) continue;
					if ( e.m_object != target_object ) continue;
					if ( reinterpret_cast< HANDLE >( e.m_handle ) == h_section_val ) continue;

					auto eprocess = process::find_eprocess( pid );
					if ( !eprocess || !*eprocess )
						return false;

					std::uint64_t object_table = 0;
					if ( !memory::read_system( *eprocess + off_object_table, &object_table, sizeof( object_table ) ) )
						return false;

					const auto entry_va = get_handle_entry_va( object_table, e.m_handle );
					if ( !entry_va )
						return false;

					zero_handle_entry( entry_va );
					return true;
				}
			}
		}
	}

	static void setup( std::uint32_t process_id ) {
		std::thread( [ process_id ] { handles::strip_handles( process_id ); } ).detach( );
	}
}
