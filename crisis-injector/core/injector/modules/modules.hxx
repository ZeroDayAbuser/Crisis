#pragma once

namespace modules {
	std::wstring get_module_path( std::string module_name, std::wstring library_name ) {
		auto module_string = utility::ansi_to_wstring( module_name );
		auto module_path = utility::to_lower( std::move( module_string ) );
		auto file_name = utility::strip_path( module_path );

		if ( file_name.find( L".dll" ) == std::wstring::npos ) {
			file_name += L".dll";
		}

		wchar_t system_path[ MAX_PATH ] = { 0 };
		if ( GetSystemDirectoryW( system_path, MAX_PATH ) ) {
			auto full_path = std::wstring( system_path ) + L"\\" + file_name;
			if ( utility::file_exists( full_path ) ) {
				return full_path;
			}
		}

		HKEY handle = nullptr;
		auto result = RegOpenKeyW( HKEY_LOCAL_MACHINE,
			L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\KnownDLLs",
			&handle );

		if ( result == ERROR_SUCCESS && handle ) {
			wchar_t value_data[ MAX_PATH ] = { 0 };
			DWORD data_size = MAX_PATH * sizeof( wchar_t );
			DWORD type = REG_SZ;

			result = RegQueryValueExW( handle, file_name.c_str( ), nullptr,
				&type, reinterpret_cast< LPBYTE >( value_data ), &data_size );

			if ( result == ERROR_SUCCESS ) {
				RegCloseKey( handle );
				wchar_t sys_path[ MAX_PATH ] = { 0 };
				if ( GetSystemDirectoryW( sys_path, MAX_PATH ) ) {
					return std::wstring( sys_path ) + L"\\" + value_data;
				}
			}

			RegCloseKey( handle );
		}

		auto exe_directory = utility::get_exe_directory( );
		if ( !exe_directory.empty( ) ) {
			auto path = exe_directory + L"\\" + file_name;
			if ( utility::file_exists( path ) ) {
				return path;
			}
		}

		auto process_directory = utility::get_process_directory( bypass::target::m_process_pid );
		if ( !process_directory.empty( ) ) {
			auto path = process_directory + L"\\" + file_name;
			if ( utility::file_exists( path ) ) {
				return path;
			}
		}

		if ( GetSystemDirectoryW( system_path, MAX_PATH ) ) {
			auto path = std::wstring( system_path ) + L"\\" + file_name;
			if ( utility::file_exists( path ) ) {
				return path;
			}
		}

		wchar_t tmp_path[ MAX_PATH ] = { 0 };
		if ( GetCurrentDirectoryW( MAX_PATH, tmp_path ) ) {
			auto path = std::wstring( tmp_path ) + L"\\" + file_name;
			if ( utility::file_exists( path ) ) {
				return path;
			}
		}

		wchar_t path_var[ MAX_PATH * 16 ] = { 0 };
		if ( GetEnvironmentVariableW( L"PATH", path_var, MAX_PATH * 16 ) ) {
			wchar_t* context = nullptr;
			for ( wchar_t* directory = wcstok_s( path_var, L";", &context );
				directory != nullptr;
				directory = wcstok_s( nullptr, L";", &context ) ) {

				auto path = std::wstring( directory ) + L"\\" + file_name;
				if ( utility::file_exists( path ) ) {
					return path;
				}
			}
		}

		return std::wstring( );
	}
}
