#pragma once
#include <intrin.h>
#include <commctrl.h>

#pragma comment( lib, "comctl32.lib" )

// TaskDialog requires comctl32 v6.
#pragma comment( linker, "/manifestdependency:\"type='win32' " \
	"name='Microsoft.Windows.Common-Controls' version='6.0.0.0' " \
	"processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"" )

#define SE_LOAD_DRIVER_PRIVILEGE  ( 10L )
#define SE_BACKUP_PRIVILEGE       ( 17L )
#define SE_DEBUG_PRIVILEGE        ( 20L )

extern "C" NTSTATUS RtlAdjustPrivilege( ULONG Privilege, BOOLEAN Enable, BOOLEAN Client, PBOOLEAN WasEnabled );


struct monitor_info_t {
	RECT bounds { };
	bool is_primary { false };
};

namespace logging {
	static void print_prefix( ) {
		auto now = std::chrono::system_clock::now( );
		std::time_t time = std::chrono::system_clock::to_time_t( now );

		tm local_tm { };
		localtime_s( &local_tm, &time );

		// logo magenta / violet
		printf(
			obf( "\x1b[38;2;220;65;251m[%02d/%02d/%04d %02d:%02d:%02d]\x1b[0m " ),
			local_tm.tm_mon + 1,
			local_tm.tm_mday,
			local_tm.tm_year + 1900,
			local_tm.tm_hour,
			local_tm.tm_min,
			local_tm.tm_sec
		);

		// logo deeper blue / purple
		printf( obf( "\x1b[38;2;110;35;241m>\x1b[0m " ) );

		// log message
		printf( obf( "\x1b[37m" ) );
	}

	template<typename... args_t>
	void print( const char* fmt, args_t&&... args ) {
		print_prefix( );
		printf( fmt, std::forward<args_t>( args )... );
		printf( obf( "\x1b[0m\n" ) );
	}

	template<typename... args_t>
	inline void print_line( const char* fmt, args_t&&... args ) {
		printf( "\r" );
		print_prefix( );
		printf( fmt, std::forward<args_t>( args )... );
		printf( obf( "\x1b[0m" ) );
	}
}

namespace utility {
	void exit_process( int timeout = 6 ) {
		MessageBeep( MB_ICONERROR );
		std::this_thread::sleep_for( std::chrono::seconds( timeout ) );
		TerminateProcess( GetCurrentProcess( ), 0xeac );
	}

	void exit_process( HANDLE process_handle, int timeout = 3 ) {
		MessageBeep( MB_ICONERROR );
		std::this_thread::sleep_for( std::chrono::seconds( timeout ) );
		TerminateProcess( process_handle, 0xeac );
	}

	bool restart_process( const char* process_name ) {
		if ( !process_name || !*process_name )
			return false;

		std::string target( process_name );
		std::transform( target.begin( ), target.end( ), target.begin( ),
			[ ] ( unsigned char c ) { return static_cast< char >( std::tolower( c ) ); } );

		HANDLE snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
		if ( snap == INVALID_HANDLE_VALUE )
			return false;

		PROCESSENTRY32W entry { };
		entry.dwSize = sizeof( entry );

		std::wstring exe_path;
		std::vector< HANDLE > victims;

		if ( Process32FirstW( snap, &entry ) ) {
			do {
				char narrow[ MAX_PATH ] { };
				WideCharToMultiByte( CP_ACP, 0, entry.szExeFile, -1,
					narrow, sizeof( narrow ), nullptr, nullptr );
				std::string current( narrow );
				std::transform( current.begin( ), current.end( ), current.begin( ),
					[ ] ( unsigned char c ) { return static_cast< char >( std::tolower( c ) ); } );
				if ( current != target )
					continue;

				if ( exe_path.empty( ) ) {
					HANDLE q = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
						entry.th32ProcessID );
					if ( q ) {
						wchar_t path[ MAX_PATH ] { };
						DWORD size = MAX_PATH;
						if ( QueryFullProcessImageNameW( q, 0, path, &size ) )
							exe_path.assign( path );
						CloseHandle( q );
					}
				}

				HANDLE t = OpenProcess( PROCESS_TERMINATE | SYNCHRONIZE, FALSE,
					entry.th32ProcessID );
				if ( t )
					victims.push_back( t );
			} while ( Process32NextW( snap, &entry ) );
		}
		CloseHandle( snap );

		for ( HANDLE h : victims )
			TerminateProcess( h, 0 );
		for ( HANDLE h : victims ) {
			WaitForSingleObject( h, 3000 );
			CloseHandle( h );
		}

		STARTUPINFOW si { };
		si.cb = sizeof( si );
		PROCESS_INFORMATION pi { };

		if ( !exe_path.empty( ) ) {
			if ( CreateProcessW( exe_path.c_str( ), nullptr, nullptr, nullptr,
				FALSE, 0, nullptr, nullptr, &si, &pi ) ) {
				CloseHandle( pi.hProcess );
				CloseHandle( pi.hThread );
				return true;
			}
		}

		wchar_t cmd[ MAX_PATH ] { };
		if ( MultiByteToWideChar( CP_ACP, 0, process_name, -1, cmd, MAX_PATH ) <= 0 )
			return false;
		if ( !CreateProcessW( nullptr, cmd, nullptr, nullptr,
			FALSE, 0, nullptr, nullptr, &si, &pi ) )
			return false;
		CloseHandle( pi.hProcess );
		CloseHandle( pi.hThread );
		return true;
	}

	inline int wide_message_box( HWND owner, const char* text, const char* title,
		unsigned int flags, int width_dlg_units = 380 ) {

		using task_dialog_indirect_fn = HRESULT( WINAPI* )( const TASKDIALOGCONFIG*, int*, int*, BOOL* );

		static const auto pfn = [ ]( ) -> task_dialog_indirect_fn {
			auto h = ::GetModuleHandleA( "comctl32.dll" );
			if ( !h )
				h = ::LoadLibraryA( "comctl32.dll" );
			if ( !h )
				return nullptr;
			return reinterpret_cast< task_dialog_indirect_fn >(
				::GetProcAddress( h, "TaskDialogIndirect" ) );
			}( );

		if ( !pfn )
			return ::MessageBoxA( owner, text, title, flags );

		const auto to_wide = [ ]( const char* s ) -> std::wstring {
			if ( !s || !*s ) return { };
			const int n = ::MultiByteToWideChar( CP_UTF8, 0, s, -1, nullptr, 0 );
			if ( n <= 0 ) return { };
			std::wstring w( static_cast< std::size_t >( n - 1 ), L'\0' );
			::MultiByteToWideChar( CP_UTF8, 0, s, -1, w.data( ), n );
			return w;
		};

		const auto w_title = to_wide( title );
		const auto w_text  = to_wide( text  );

		TASKDIALOGCONFIG cfg { };
		cfg.cbSize             = sizeof( cfg );
		cfg.hwndParent         = owner;
		cfg.hInstance          = ::GetModuleHandleW( nullptr );
		cfg.dwFlags            = TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
		cfg.pszWindowTitle     = w_title.empty( ) ? L"Crisis" : w_title.c_str( );
		cfg.pszMainInstruction = nullptr;
		cfg.pszContent         = w_text.c_str( );
		cfg.cxWidth            = static_cast< UINT >( width_dlg_units );

		if ( flags & MB_ICONERROR )       cfg.pszMainIcon = TD_ERROR_ICON;
		else if ( flags & MB_ICONWARNING ) cfg.pszMainIcon = TD_WARNING_ICON;
		else if ( flags & MB_ICONQUESTION )cfg.pszMainIcon = TD_INFORMATION_ICON;
		else                              cfg.pszMainIcon = TD_INFORMATION_ICON;

		const auto button_kind = flags & 0xF;
		if ( button_kind == MB_YESNO )        cfg.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
		else if ( button_kind == MB_YESNOCANCEL ) cfg.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON;
		else if ( button_kind == MB_OKCANCEL ) cfg.dwCommonButtons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
		else                                  cfg.dwCommonButtons = TDCBF_OK_BUTTON;

		int   selected = 0;
		BOOL  verify   = FALSE;
		const HRESULT hr = pfn( &cfg, &selected, nullptr, &verify );

		if ( FAILED( hr ) )
			return ::MessageBoxA( owner, text, title, flags );

		// Map TaskDialog button ids back to MessageBox return values.
		switch ( selected ) {
			case IDOK:     return IDOK;
			case IDCANCEL: return IDCANCEL;
			case IDYES:    return IDYES;
			case IDNO:     return IDNO;
			default:       return selected;
		}
	}

	bool is_vvci_or_vbs_enabled( ) {
		DWORD value = 0;
		DWORD size = sizeof( DWORD );

		HKEY key = nullptr;
		if ( RegOpenKeyExA(
			HKEY_LOCAL_MACHINE,
			obf( "SYSTEM\\CurrentControlSet\\Control\\DeviceGuard" ),
			0,
			KEY_READ,
			&key ) != ERROR_SUCCESS )
			return false;

		auto query = [ & ]( const char* name ) -> bool
			{
				value = 0;
				size = sizeof( DWORD );
				if ( RegQueryValueExA( key, name, nullptr, nullptr,
					reinterpret_cast< LPBYTE >( &value ), &size ) != ERROR_SUCCESS )
					return false;
				return value != 0;
			};

		const bool vbs = query( obf( "EnableVirtualizationBasedSecurity" ) );
		const bool hvci = query( obf( "HypervisorEnforcedCodeIntegrity" ) );

		RegCloseKey( key );
		return vbs || hvci;
	}

	inline std::string add_commas( std::uint64_t n ) {
		auto s = std::to_string( n );
		auto pos = ( int ) s.size( ) - 3;
		while ( pos > 0 ) {
			s.insert( pos, "," );
			pos -= 3;
		}
		return s;
	}

	bool is_valorant_installed( wchar_t out_path[ MAX_PATH ] = nullptr ) {
		static const wchar_t* keys[ ] = {
			obf( L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Riot Game valorant.live" ),
			obf( L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Riot Game valorant.live" ),
			obf( L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\RiotClient" ),
		};

		for ( auto key_path : keys ) {
			HKEY hkey = nullptr;
			if ( RegOpenKeyExW( HKEY_LOCAL_MACHINE, key_path, 0, KEY_READ, &hkey ) != ERROR_SUCCESS )
				continue;

			if ( out_path ) {
				DWORD size = MAX_PATH * sizeof( wchar_t );
				RegQueryValueExW( hkey, obf( L"InstallLocation" ), nullptr, nullptr,
					reinterpret_cast< BYTE* >( out_path ), &size );
			}

			RegCloseKey( hkey );
			return true;
		}

		return false;
	}

	bool is_vanguard_running( wchar_t out_name[ MAX_PATH ] = nullptr ) {
		HANDLE snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
		if ( snap == INVALID_HANDLE_VALUE ) return false;

		PROCESSENTRY32W pe { sizeof( pe ) };
		bool found = false;

		if ( Process32FirstW( snap, &pe ) ) {
			do {
				auto exe_file = pe.szExeFile;
				const bool is_valorant =
					!_wcsicmp( exe_file, obf( L"vgc.exe" ) ) ||  // vanguard service
					!_wcsicmp( exe_file, obf( L"vgtray.exe" ) ) ||  // vanguard tray
					!_wcsicmp( exe_file, obf( L"vgk.sys" ) );                        // vanguard kernel driver

				if ( is_valorant ) {
					if ( out_name )
						wcscpy_s( out_name, MAX_PATH, exe_file );
					found = true;
					break;
				}

			} while ( Process32NextW( snap, &pe ) );
		}

		CloseHandle( snap );
		return found;
	}

	static auto enumerate_monitors( ) -> std::vector< monitor_info_t > {
		std::vector< monitor_info_t > monitors;
		EnumDisplayMonitors( nullptr, nullptr,
			[ ]( HMONITOR handle, HDC, LPRECT, LPARAM lp ) -> BOOL {
				auto* list = reinterpret_cast< std::vector< monitor_info_t >* >( lp );
				MONITORINFO info { sizeof( info ) };
				if ( !GetMonitorInfoW( handle, &info ) )
					return TRUE;

				monitor_info_t entry { };
				entry.bounds = info.rcMonitor;
				entry.is_primary = ( info.dwFlags & MONITORINFOF_PRIMARY ) != 0;
				list->push_back( entry );
				return TRUE;
			},
			reinterpret_cast< LPARAM >( &monitors )
		);

		std::sort( monitors.begin( ), monitors.end( ),
			[ ]( const monitor_info_t& a, const monitor_info_t& b ) {
				if ( a.is_primary != b.is_primary )
					return a.is_primary;
				if ( a.bounds.top != b.bounds.top )
					return a.bounds.top < b.bounds.top;
				return a.bounds.left < b.bounds.left;
			} );

		return monitors;
	}

	static std::uint64_t filetime_to_u64( const FILETIME& ft ) {
		ULARGE_INTEGER uli {};
		uli.LowPart = ft.dwLowDateTime;
		uli.HighPart = ft.dwHighDateTime;
		return uli.QuadPart;
	}

	static bool file_was_created_this_boot( const wchar_t* path ) {
		WIN32_FILE_ATTRIBUTE_DATA data {};
		if ( !GetFileAttributesExW( path, GetFileExInfoStandard, &data ) )
			return false;

		FILETIME now_ft {};
		GetSystemTimeAsFileTime( &now_ft );

		const std::uint64_t now = filetime_to_u64( now_ft );
		const std::uint64_t created = filetime_to_u64( data.ftCreationTime );

		const std::uint64_t uptime_100ns = GetTickCount64( ) * 10000ULL;
		const std::uint64_t boot_time = now - uptime_100ns;

		return created >= boot_time;
	}

	static bool file_was_modified_this_boot( const wchar_t* path ) {
		WIN32_FILE_ATTRIBUTE_DATA data {};
		if ( !GetFileAttributesExW( path, GetFileExInfoStandard, &data ) )
			return false;

		FILETIME now_ft {};
		GetSystemTimeAsFileTime( &now_ft );

		const std::uint64_t now = filetime_to_u64( now_ft );
		const std::uint64_t modified = filetime_to_u64( data.ftLastWriteTime );

		const std::uint64_t uptime_100ns = GetTickCount64( ) * 10000ULL;
		const std::uint64_t boot_time = now - uptime_100ns;

		return modified >= boot_time;
	}

	static std::wstring format_duration( std::uint64_t total_seconds ) {
		const auto days = total_seconds / 86400;
		total_seconds %= 86400;

		const auto hours = total_seconds / 3600;
		total_seconds %= 3600;

		const auto minutes = total_seconds / 60;
		const auto seconds = total_seconds % 60;

		std::vector<std::wstring> parts;

		if ( days )
			parts.push_back( std::to_wstring( days ) + L"d" );

		if ( hours )
			parts.push_back( std::to_wstring( hours ) + L"h" );

		if ( minutes )
			parts.push_back( std::to_wstring( minutes ) + L"m" );

		if ( seconds || parts.empty( ) )
			parts.push_back( std::to_wstring( seconds ) + L"s" );

		std::wstringstream ss;
		for ( std::size_t i = 0; i < parts.size( ); ++i ) {
			if ( i )
				ss << L", ";
			ss << parts[ i ];
		}

		return ss.str( );
	}

	static bool get_file_download_age( const wchar_t* path, std::wstring& out ) {
		WIN32_FILE_ATTRIBUTE_DATA data {};
		if ( !GetFileAttributesExW( path, GetFileExInfoStandard, &data ) )
			return false;

		FILETIME now_ft {};
		GetSystemTimeAsFileTime( &now_ft );

		const auto now = filetime_to_u64( now_ft );
		const auto created = filetime_to_u64( data.ftLastAccessTime );

		if ( created > now ) {
			out = L"0s";
			return true;
		}

		const auto elapsed_100ns = now - created;
		const auto elapsed_seconds = elapsed_100ns / 10000000ULL;

		out = format_duration( elapsed_seconds );
		return true;
	}

	bool toggle_privilege( unsigned long privilege, bool enable ) {
		BOOLEAN was_enabled = FALSE;
		return RtlAdjustPrivilege( privilege, enable, FALSE, &was_enabled ) == STATUS_SUCCESS;
	}

	bool toggle_privilege( const wchar_t* priv_name, bool enable ) {
		LUID luid {};
		if ( !LookupPrivilegeValueW( nullptr, priv_name, &luid ) )
			return false;

		return toggle_privilege( luid.LowPart, enable );
	}

    std::wstring get_exe_directory( ) {
        wchar_t path[ MAX_PATH ]{ };
        GetModuleFileNameW( nullptr, path, MAX_PATH );
        std::wstring ws( path );
        const auto pos = ws.rfind( L'\\' );
        if ( pos != std::wstring::npos )
            ws = ws.substr( 0, pos );
        return ws;
    }

    std::string wstring_to_ansi( const std::wstring& ws ) {
        if ( ws.empty( ) )
            return { };
        const auto size = WideCharToMultiByte( CP_ACP, 0, ws.c_str( ), -1, nullptr, 0, nullptr, nullptr );
        std::string result( size - 1, '\0' );
        WideCharToMultiByte( CP_ACP, 0, ws.c_str( ), -1, result.data( ), size, nullptr, nullptr );
        return result;
    }

    std::uint32_t find_pid( const wchar_t* target_process ) {
        auto snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
        if ( snapshot == INVALID_HANDLE_VALUE )
            return 0;

        PROCESSENTRY32W entry{ .dwSize = sizeof( entry ) };
        std::uint32_t result = 0;

        if ( Process32FirstW( snapshot, &entry ) ) {
            do {
                if ( _wcsicmp( target_process, entry.szExeFile ) == 0 ) {
                    result = entry.th32ProcessID;
                    break;
                }
            } while ( Process32NextW( snapshot, &entry ) );
        }

        CloseHandle( snapshot );
        return result;
    }

	bool is_physical_address_valid( std::uint64_t pa, std::size_t size ) {
		static const std::uint64_t physical_mask = [ ]( ) -> std::uint64_t {
			int regs[ 4 ] {};
			__cpuid( regs, 0x80000000 );
			unsigned max_ext = static_cast< unsigned >( regs[ 0 ] );
			unsigned phys_bits = 36;
			if ( max_ext >= 0x80000008 ) {
				__cpuid( regs, 0x80000008 );
				unsigned b = regs[ 0 ] & 0xFF;
				if ( b >= 32 && b <= 52 )
					phys_bits = b;
			}
			return ( phys_bits >= 63 ) ? ~0ULL : ( ( 1ULL << phys_bits ) - 1ULL );
			}( );

		if ( ( pa & ~physical_mask ) != 0 )
			return false;

		const auto end_pa = pa + ( size - 1 );
		if ( ( end_pa & ~physical_mask ) != 0 )
			return false;

		return true;
	}

	void gen_rnd_str( wchar_t* random_str ) {
		static std::mt19937 rng( std::random_device{}( ) );
		std::uniform_int_distribution<size_t> len_dist( 8, 19 );
		size_t length = len_dist( rng );

		std::uniform_int_distribution<int> char_type( 0, 2 );
		std::uniform_int_distribution<int> alpha( 0, 25 );
		std::uniform_int_distribution<int> digit( 0, 9 );

		for ( size_t i = 0; i < length; ++i ) {
			switch ( char_type( rng ) ) {
				case 0: random_str[ i ] = L'A' + alpha( rng ); break;
				case 1: random_str[ i ] = L'a' + alpha( rng ); break;
				case 2: random_str[ i ] = L'0' + digit( rng ); break;
			}
		}
		random_str[ length ] = L'\0';
	}

	std::uint64_t get_kernel_module( const char* module_name ) {
		void* buffer = nullptr;
		unsigned long buffer_size = 0;
		NTSTATUS status = NtQuerySystemInformation(
			static_cast< SYSTEM_INFORMATION_CLASS >( 11 ),
			buffer, buffer_size, &buffer_size
		);

		while ( status == 0xC0000004L ) {
			if ( buffer ) { VirtualFree( buffer, 0, MEM_RELEASE ); buffer = nullptr; }
			buffer = VirtualAlloc( nullptr, buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
			if ( !buffer ) return 0;
			status = NtQuerySystemInformation(
				static_cast< SYSTEM_INFORMATION_CLASS >( 11 ),
				buffer, buffer_size, &buffer_size
			);
		}

		if ( !NT_SUCCESS( status ) || !buffer ) {
			if ( buffer ) VirtualFree( buffer, 0, MEM_RELEASE );
			return 0;
		}

		const auto modules = reinterpret_cast< rtl_process_modules_t* >( buffer );
		for ( auto idx = 0u; idx < modules->m_count; ++idx ) {
			const auto name = std::string(
				reinterpret_cast< char* >( modules->m_modules[ idx ].m_full_path ) +
				modules->m_modules[ idx ].m_offset_to_file_name
			);
			if ( !_stricmp( name.c_str( ), module_name ) ) {
				const auto base = reinterpret_cast< std::uint64_t >( modules->m_modules[ idx ].m_image_base );
				VirtualFree( buffer, 0, MEM_RELEASE );
				return base;
			}
		}

		VirtualFree( buffer, 0, MEM_RELEASE );
		return 0;
	}

	std::uint32_t get_kernel_size( const char* module_name ) {
		void* buffer = nullptr;
		unsigned long buffer_size = 0;
		NTSTATUS status = NtQuerySystemInformation(
			static_cast< SYSTEM_INFORMATION_CLASS >( 11 ),
			buffer, buffer_size, &buffer_size
		);

		while ( status == 0xC0000004L ) {
			if ( buffer ) { VirtualFree( buffer, 0, MEM_RELEASE ); buffer = nullptr; }
			buffer = VirtualAlloc( nullptr, buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
			if ( !buffer ) return 0;
			status = NtQuerySystemInformation(
				static_cast< SYSTEM_INFORMATION_CLASS >( 11 ),
				buffer, buffer_size, &buffer_size
			);
		}

		if ( !NT_SUCCESS( status ) || !buffer ) {
			if ( buffer ) VirtualFree( buffer, 0, MEM_RELEASE );
			return 0;
		}

		const auto modules = reinterpret_cast< rtl_process_modules_t* >( buffer );
		for ( auto idx = 0u; idx < modules->m_count; ++idx ) {
			const auto name = std::string(
				reinterpret_cast< char* >( modules->m_modules[ idx ].m_full_path ) +
				modules->m_modules[ idx ].m_offset_to_file_name
			);
			if ( !_stricmp( name.c_str( ), module_name ) ) {
				const auto size = modules->m_modules[ idx ].m_image_size;
				VirtualFree( buffer, 0, MEM_RELEASE );
				return size;
			}
		}

		VirtualFree( buffer, 0, MEM_RELEASE );
		return 0;
	}

	bool is_already_running( ) {
		wchar_t self_path[ MAX_PATH ];
		GetModuleFileNameW( nullptr, self_path, MAX_PATH );
		std::wstring self_name = self_path;
		self_name = self_name.substr( self_name.find_last_of( L"\\/" ) + 1 );

		DWORD self_pid = GetCurrentProcessId( );

		HANDLE snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
		if ( snap == INVALID_HANDLE_VALUE ) return false;

		PROCESSENTRY32W entry { sizeof( entry ) };
		bool found = false;

		if ( Process32FirstW( snap, &entry ) ) {
			do {
				if ( entry.th32ProcessID == self_pid ) continue;
				if ( _wcsicmp( entry.szExeFile, self_name.c_str( ) ) == 0 ) {
					found = true;
					break;
				}
			} while ( Process32NextW( snap, &entry ) );
		}

		CloseHandle( snap );
		return found;
	}

	bool is_anti_cheat_running( wchar_t out_name[ MAX_PATH ] = nullptr ) {
		HANDLE snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
		if ( snap == INVALID_HANDLE_VALUE ) return false;

		PROCESSENTRY32W pe { sizeof( pe ) };
		bool found = false;

		if ( Process32FirstW( snap, &pe ) ) {
			do {
				auto exe_file = pe.szExeFile;
				const bool is_blacklisted =
					!_wcsicmp( exe_file, obf( L"VALORANT-Win64-Shipping.exe" ) ) ||
					!_wcsicmp( exe_file, obf( L"EasyAntiCheat.exe" ) ) ||
					!_wcsicmp( exe_file, obf( L"EasyAntiCheat_EOS.exe" ) ) ||
					!_wcsicmp( exe_file, obf( L"BEService.exe" ) ) ||
					!_wcsicmp( exe_file, obf( L"BEService_x64.exe" ) ) ||
					!_wcsicmp( exe_file, obf( L"faceit.exe" ) ) ||
					!_wcsicmp( exe_file, obf( L"FACEIT-AC.exe" ) ) ||
					!_wcsicmp( exe_file, obf( L"RobloxPlayerBeta.exe" ) ) ||
					!_wcsicmp( exe_file, obf( L"RobloxCrashHandler.exe" ) ) ||
					!_wcsicmp( exe_file, obf( L"Hyperion.exe" ) );          // Byfron/Hyperion AC

				if ( is_blacklisted ) {
					if ( out_name )
						wcscpy_s( out_name, MAX_PATH, exe_file );
					found = true;
					break;
				}

			} while ( Process32NextW( snap, &pe ) );
		}

		CloseHandle( snap );
		return found;
	}
	// Detached thread that rewrites the console title every
	// `interval_seconds` seconds with a fresh random UUIDv4 (RFC 4122):
	// 32 lowercase hex chars grouped 8-4-4-4-12, with the version and
	// variant nibbles fixed to '4' and one of {'8','9','a','b'}.
	inline void start_title_shuffle( int interval_seconds = 10 ) {
		std::thread( [ interval_seconds ] {
			std::mt19937_64 rng( static_cast< std::uint64_t >( ::GetTickCount64( ) )
				^ static_cast< std::uint64_t >(
					reinterpret_cast< std::uintptr_t >( &interval_seconds ) )
				^ ( static_cast< std::uint64_t >( ::GetCurrentThreadId( ) ) << 32 ) );

			static const char h[] = "0123456789abcdef";
			char buf[ 37 ]; // 36 chars + NUL
			buf[  8 ] = '-';
			buf[ 13 ] = '-';
			buf[ 18 ] = '-';
			buf[ 23 ] = '-';
			buf[ 36 ] = '\0';

			// Char positions that hold hex, in order.
			static constexpr int slots[ 32 ] = {
				 0, 1, 2, 3, 4, 5, 6, 7,     //  8: xxxxxxxx
				 9,10,11,12,                 //  4: xxxx
				14,15,16,17,                 //  4: 4xxx (version)
				19,20,21,22,                 //  4: yxxx (variant)
				24,25,26,27,28,29,30,31,32,33,34,35 // 12: xxxxxxxxxxxx
			};

			while ( true ) {
				std::uint8_t bytes[ 16 ];
				for ( int i = 0; i < 16; i += 8 ) {
					const auto v = rng( );
					for ( int j = 0; j < 8; ++j )
						bytes[ i + j ] = static_cast< std::uint8_t >( v >> ( j * 8 ) );
				}

				// RFC 4122 §4.4: fix version (v4) and variant (10x).
				bytes[  6 ] = ( bytes[  6 ] & 0x0F ) | 0x40; // version 4
				bytes[  8 ] = ( bytes[  8 ] & 0x3F ) | 0x80; // variant 10xxxxxx

				for ( int i = 0; i < 16; ++i ) {
					buf[ slots[ i * 2     ] ] = h[ bytes[ i ] >> 4  ];
					buf[ slots[ i * 2 + 1 ] ] = h[ bytes[ i ] & 0xF ];
				}

				::SetConsoleTitleA( buf );
				std::this_thread::sleep_for(
					std::chrono::seconds( interval_seconds ) );
			}
		} ).detach( );
	}

	namespace defender {
		inline DWORD read_hklm_dword( const char* subkey, const char* value_name, DWORD default_value = 0 ) {
			HKEY key = nullptr;
			if ( RegOpenKeyExA( HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ | KEY_WOW64_64KEY, &key ) != ERROR_SUCCESS )
				return default_value;

			DWORD value = default_value;
			DWORD size  = sizeof( value );
			DWORD type  = 0;
			if ( RegQueryValueExA( key, value_name, nullptr, &type,
					reinterpret_cast< LPBYTE >( &value ), &size ) != ERROR_SUCCESS
				|| type != REG_DWORD ) {
				value = default_value;
			}

			RegCloseKey( key );
			return value;
		}

		inline bool read_hklm_string( const char* subkey, const char* value_name,
			char* out, DWORD out_size ) {
			HKEY key = nullptr;
			if ( RegOpenKeyExA( HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ | KEY_WOW64_64KEY, &key ) != ERROR_SUCCESS )
				return false;

			DWORD type = 0;
			DWORD size = out_size;
			const auto status = RegQueryValueExA( key, value_name, nullptr, &type,
				reinterpret_cast< LPBYTE >( out ), &size );
			RegCloseKey( key );
			return status == ERROR_SUCCESS && type == REG_SZ;
		}

		inline bool is_realtime_protection_enabled( ) {
			if ( read_hklm_dword(
					obf( "SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection" ),
					obf( "DisableRealtimeMonitoring" ), 0 ) != 0 )
				return false;

			return read_hklm_dword(
				obf( "SOFTWARE\\Microsoft\\Windows Defender\\Real-Time Protection" ),
				obf( "DisableRealtimeMonitoring" ), 0 ) == 0;
		}

		inline bool is_behavior_monitoring_enabled( ) {
			if ( read_hklm_dword(
					obf( "SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection" ),
					obf( "DisableBehaviorMonitoring" ), 0 ) != 0 )
				return false;

			return read_hklm_dword(
				obf( "SOFTWARE\\Microsoft\\Windows Defender\\Real-Time Protection" ),
				obf( "DisableBehaviorMonitoring" ), 0 ) == 0;
		}

		inline bool is_on_access_protection_enabled( ) {
			if ( read_hklm_dword(
					obf( "SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection" ),
					obf( "DisableOnAccessProtection" ), 0 ) != 0 )
				return false;

			return read_hklm_dword(
				obf( "SOFTWARE\\Microsoft\\Windows Defender\\Real-Time Protection" ),
				obf( "DisableOnAccessProtection" ), 0 ) == 0;
		}

		inline bool is_ioav_protection_enabled( ) {
			if ( read_hklm_dword(
					obf( "SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection" ),
					obf( "DisableIOAVProtection" ), 0 ) != 0 )
				return false;

			return read_hklm_dword(
				obf( "SOFTWARE\\Microsoft\\Windows Defender\\Real-Time Protection" ),
				obf( "DisableIOAVProtection" ), 0 ) == 0;
		}

		inline bool is_script_scanning_enabled( ) {
			if ( read_hklm_dword(
					obf( "SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection" ),
					obf( "DisableScriptScanning" ), 0 ) != 0 )
				return false;

			return read_hklm_dword(
				obf( "SOFTWARE\\Microsoft\\Windows Defender\\Real-Time Protection" ),
				obf( "DisableScriptScanning" ), 0 ) == 0;
		}

		inline bool is_cloud_protection_enabled( ) {
			const auto policy = read_hklm_dword(
				obf( "SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Spynet" ),
				obf( "SpynetReporting" ), 0xFFFFFFFFu );
			if ( policy != 0xFFFFFFFFu )
				return policy != 0;

			return read_hklm_dword(
				obf( "SOFTWARE\\Microsoft\\Windows Defender\\Spynet" ),
				obf( "SpynetReporting" ), 0 ) != 0;
		}

		inline bool is_automatic_sample_submission_enabled( ) {
			const auto policy = read_hklm_dword(
				obf( "SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Spynet" ),
				obf( "SubmitSamplesConsent" ), 0xFFFFFFFFu );
			const DWORD value = ( policy != 0xFFFFFFFFu )
				? policy
				: read_hklm_dword(
					obf( "SOFTWARE\\Microsoft\\Windows Defender\\Spynet" ),
					obf( "SubmitSamplesConsent" ), 1 );
			return value == 1 || value == 3;
		}

		inline bool is_tamper_protection_enabled( ) {
			return read_hklm_dword(
				obf( "SOFTWARE\\Microsoft\\Windows Defender\\Features" ),
				obf( "TamperProtection" ), 0 ) == 5;
		}

		inline bool is_pua_protection_enabled( ) {
			const auto policy = read_hklm_dword(
				obf( "SOFTWARE\\Policies\\Microsoft\\Windows Defender" ),
				obf( "PUAProtection" ), 0xFFFFFFFFu );
			if ( policy != 0xFFFFFFFFu )
				return policy == 1 || policy == 2;

			return read_hklm_dword(
				obf( "SOFTWARE\\Microsoft\\Windows Defender" ),
				obf( "PUAProtection" ), 0 ) != 0;
		}

		inline bool is_controlled_folder_access_enabled( ) {
			const auto policy = read_hklm_dword(
				obf( "SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Windows Defender Exploit Guard\\Controlled Folder Access" ),
				obf( "EnableControlledFolderAccess" ), 0xFFFFFFFFu );
			if ( policy != 0xFFFFFFFFu )
				return policy != 0;

			return read_hklm_dword(
				obf( "SOFTWARE\\Microsoft\\Windows Defender\\Windows Defender Exploit Guard\\Controlled Folder Access" ),
				obf( "EnableControlledFolderAccess" ), 0 ) != 0;
		}

		inline bool is_network_protection_enabled( ) {
			const auto policy = read_hklm_dword(
				obf( "SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Windows Defender Exploit Guard\\Network Protection" ),
				obf( "EnableNetworkProtection" ), 0xFFFFFFFFu );
			if ( policy != 0xFFFFFFFFu )
				return policy == 1 || policy == 2;

			return read_hklm_dword(
				obf( "SOFTWARE\\Microsoft\\Windows Defender\\Windows Defender Exploit Guard\\Network Protection" ),
				obf( "EnableNetworkProtection" ), 0 ) != 0;
		}

		inline bool is_asr_enabled( ) {
			auto scan_rules = [ ]( const char* path ) -> bool {
				HKEY key = nullptr;
				if ( RegOpenKeyExA( HKEY_LOCAL_MACHINE, path, 0, KEY_READ | KEY_WOW64_64KEY, &key ) != ERROR_SUCCESS )
					return false;

				DWORD idx = 0;
				bool  active = false;
				for ( ;; ) {
					char  name[ 128 ] { };
					DWORD name_size = sizeof( name );
					char  data[ 16 ] { };
					DWORD data_size = sizeof( data );
					DWORD type = 0;

					const auto status = RegEnumValueA( key, idx++, name, &name_size,
						nullptr, &type,
						reinterpret_cast< LPBYTE >( data ), &data_size );
					if ( status == ERROR_NO_MORE_ITEMS )
						break;
					if ( status != ERROR_SUCCESS )
						continue;

					if ( type == REG_SZ && data_size ) {
						if ( data[ 0 ] == '1' || data[ 0 ] == '2' ) {
							active = true;
							break;
						}
					}
					else if ( type == REG_DWORD && data_size == sizeof( DWORD ) ) {
						const auto d = *reinterpret_cast< const DWORD* >( data );
						if ( d == 1 || d == 2 ) {
							active = true;
							break;
						}
					}
				}

				RegCloseKey( key );
				return active;
			};

			return scan_rules(
					obf( "SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Windows Defender Exploit Guard\\ASR\\Rules" ) )
				|| scan_rules(
					obf( "SOFTWARE\\Microsoft\\Windows Defender\\Windows Defender Exploit Guard\\ASR\\Rules" ) );
		}

		inline bool is_defender_service_enabled( ) {
			return read_hklm_dword(
				obf( "SYSTEM\\CurrentControlSet\\Services\\WinDefend" ),
				obf( "Start" ), 4 ) < 4;
		}

		inline bool is_windefend_actually_running( ) {
			SC_HANDLE scm = ::OpenSCManagerA( nullptr, nullptr, SC_MANAGER_CONNECT );
			if ( !scm ) return true;
			SC_HANDLE svc = ::OpenServiceA( scm, obf( "WinDefend" ), SERVICE_QUERY_STATUS );
			if ( !svc ) {
				::CloseServiceHandle( scm );
				return false;
			}
			SERVICE_STATUS_PROCESS status { };
			DWORD needed = 0;
			bool running = false;
			if ( ::QueryServiceStatusEx( svc, SC_STATUS_PROCESS_INFO,
				reinterpret_cast< LPBYTE >( &status ), sizeof( status ), &needed ) )
				running = ( status.dwCurrentState == SERVICE_RUNNING );
			::CloseServiceHandle( svc );
			::CloseServiceHandle( scm );
			return running;
		}

		inline bool is_defender_neutralized( ) {
			if ( read_hklm_dword(
					obf( "SOFTWARE\\Policies\\Microsoft\\Windows Defender" ),
					obf( "DisableAntiSpyware" ), 0 ) != 0 )
				return true;
			if ( read_hklm_dword(
					obf( "SYSTEM\\CurrentControlSet\\Services\\WinDefend" ),
					obf( "Start" ), 2 ) >= 4 )
				return true;
			if ( !is_windefend_actually_running( ) )
				return true;
			return false;
		}

		inline bool is_smartscreen_enabled( ) {
			const auto policy = read_hklm_dword(
				obf( "SOFTWARE\\Policies\\Microsoft\\Windows\\System" ),
				obf( "EnableSmartScreen" ), 0xFFFFFFFFu );
			if ( policy != 0xFFFFFFFFu )
				return policy != 0;

			char state[ 32 ] { };
			if ( !read_hklm_string(
					obf( "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer" ),
					obf( "SmartScreenEnabled" ), state, sizeof( state ) ) )
				return true; 
			return _stricmp( state, obf( "Off" ) ) != 0;
		}

		inline bool is_protection_enabled( ) {
			return is_realtime_protection_enabled( )
				|| is_behavior_monitoring_enabled( )
				|| is_on_access_protection_enabled( )
				|| is_ioav_protection_enabled( )
				|| is_script_scanning_enabled( )
				|| is_cloud_protection_enabled( )
				|| is_tamper_protection_enabled( )
				|| is_controlled_folder_access_enabled( )
				|| is_network_protection_enabled( )
				|| is_asr_enabled( );
		}

		// ------------------------------------------------------------------
		// Code Integrity / application-control surfaces.
		// These aren't on the "Virus & threat protection" page - they live
		// under Device security, App & browser control, or arrive as MDM /
		// enterprise policy - but they hard-block the loader before Defender
		// ever gets a chance.
		// ------------------------------------------------------------------

		// Microsoft's Vulnerable Driver Blocklist. When on, ntoskrnl refuses
		// to load drivers whose hashes appear in DriverSiPolicy.p7b - i.e.
		// the entire published BYOVD catalog. If our driver.sys is on the
		// list, NtLoadDriver returns STATUS_INVALID_IMAGE_HASH.
		inline bool is_vulnerable_driver_blocklist_enabled( ) {
			// HVCI implicitly enforces the blocklist even when the toggle is 0.
			if ( ::utility::is_vvci_or_vbs_enabled( ) )
				return true;
			return read_hklm_dword(
				obf( "SYSTEM\\CurrentControlSet\\Control\\CI\\Config" ),
				obf( "VulnerableDriverBlocklistEnable" ), 0 ) != 0;
		}

		// Smart App Control (Win11 22H2+). CI-level block on unsigned or
		// low-reputation binaries - no user prompt, no override.
		// VerifiedAndReputablePolicyState: 0 = off, 1 = evaluation, 2 = enforced.
		inline bool is_smart_app_control_enabled( ) {
			const auto state = read_hklm_dword(
				obf( "SYSTEM\\CurrentControlSet\\Control\\CI\\Policy" ),
				obf( "VerifiedAndReputablePolicyState" ), 0 );
			return state == 2;
		}

		// A WDAC / App Control custom policy is active (SIPolicy.p7b or CIP
		// files deployed under System32\CodeIntegrity). We can't parse the
		// policy from user mode, but the presence of the marker is enough
		// to know unsigned modules may be refused.
		inline bool is_wdac_policy_active( ) {
			// The "Protected" key exists whenever a CI policy is loaded.
			HKEY key = nullptr;
			if ( RegOpenKeyExA( HKEY_LOCAL_MACHINE,
					obf( "SYSTEM\\CurrentControlSet\\Control\\CI\\Protected" ),
					0, KEY_READ | KEY_WOW64_64KEY, &key ) == ERROR_SUCCESS ) {
				RegCloseKey( key );
				return true;
			}
			// Fallback: any .cip / .p7b under System32\CodeIntegrity\CiPolicies\Active.
			WIN32_FIND_DATAW fd {};
			HANDLE fh = FindFirstFileW(
				L"C:\\Windows\\System32\\CodeIntegrity\\CiPolicies\\Active\\*.cip", &fd );
			if ( fh != INVALID_HANDLE_VALUE ) {
				FindClose( fh );
				return true;
			}
			return false;
		}

		// LSA Protection (RunAsPPL). 0 = off, 1 = on, 2 = on with UEFI lock.
		// Only matters if the loader touches lsass.exe (this one doesn't).
		inline bool is_lsa_protection_enabled( ) {
			return read_hklm_dword(
				obf( "SYSTEM\\CurrentControlSet\\Control\\Lsa" ),
				obf( "RunAsPPL" ), 0 ) != 0;
		}

		// Credential Guard. LsaCfgFlags: 0 = off, 1 = on w/ UEFI lock, 2 = on.
		// Same story as LSA Protection - only matters for LSASS work.
		inline bool is_credential_guard_enabled( ) {
			const auto value = read_hklm_dword(
				obf( "SYSTEM\\CurrentControlSet\\Control\\Lsa" ),
				obf( "LsaCfgFlags" ), 0 );
			if ( value != 0 )
				return true;
			return read_hklm_dword(
				obf( "SYSTEM\\CurrentControlSet\\Control\\Lsa" ),
				obf( "LsaCfgFlagsDefault" ), 0 ) != 0;
		}

		// ------------------------------------------------------------------
		// Exploit-protection mitigations (App & browser control page).
		// These are informational for a loader - none of them BLOCK a
		// correctly-written manual-mapper. Exposed so the console can print
		// the system's mitigation profile alongside the AV state.
		// ------------------------------------------------------------------

		// System-wide DEP. GetSystemDEPPolicy returns:
		//   0 = AlwaysOff, 1 = AlwaysOn, 2 = OptIn, 3 = OptOut.
		inline bool is_dep_enabled_systemwide( ) {
			return ::GetSystemDEPPolicy( ) != 0;
		}

		// Loader-process view of the applied mitigation policies. On x64
		// nearly always Enable == true; kept as query helpers so callers
		// can render a mitigation summary.
		inline bool is_dep_enabled_here( ) {
			const auto p = ::GetSystemDEPPolicy( );
			return p == 1 || p == 3;
		}

		inline bool is_aslr_enabled_here( ) {
			PROCESS_MITIGATION_ASLR_POLICY policy { };
			if ( !::GetProcessMitigationPolicy( ::GetCurrentProcess( ),
					ProcessASLRPolicy, &policy, sizeof( policy ) ) )
				return false;
			return policy.EnableBottomUpRandomization
				|| policy.EnableForceRelocateImages
				|| policy.EnableHighEntropy;
		}

		inline bool is_cfg_enabled_here( ) {
			PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY policy { };
			if ( !::GetProcessMitigationPolicy( ::GetCurrentProcess( ),
					ProcessControlFlowGuardPolicy, &policy, sizeof( policy ) ) )
				return false;
			return policy.EnableControlFlowGuard != 0;
		}

		inline bool is_cet_enabled_here( ) {
			PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY policy { };
			if ( !::GetProcessMitigationPolicy( ::GetCurrentProcess( ),
					ProcessUserShadowStackPolicy, &policy, sizeof( policy ) ) )
				return false;
			return policy.EnableUserShadowStack != 0;
		}

		// SEHOP (x86-relevant; on x64 the SEH scheme already validates the
		// chain, so this is mostly informational).
		inline bool is_sehop_enabled( ) {
			// Value = 1 means SEHOP is DISABLED; absence / 0 means enabled.
			return read_hklm_dword(
				obf( "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\kernel" ),
				obf( "DisableExceptionChainValidation" ), 0 ) == 0;
		}
	}
}
