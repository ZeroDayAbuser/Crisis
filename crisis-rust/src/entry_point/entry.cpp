#include <incl.h>
#include <src/nexus/function_call.hxx>

namespace test {

	constexpr wchar_t target_executable[ ] = L"RustClient.exe";
	constexpr wchar_t game_assembly_name[ ] = L"GameAssembly.dll";
	constexpr wchar_t unity_player_name[ ] = L"UnityPlayer.dll";
	constexpr wchar_t baselib_name[ ] = L"baselib.dll";
	constexpr char realtime_icall_name[ ] = "UnityEngine.Time::get_realtimeSinceStartupAsDouble()";

	struct remote_module_t {
		std::uintptr_t base {};
		wchar_t path[ MAX_PATH ] {};
	};

	struct realtime_wrapper_t {
		std::uintptr_t function {};
		std::uintptr_t cached_icall {};
	};

	std::wstring dos_path_from_device_path( const std::wstring_view path ) {
		wchar_t drive[ 3 ] = L"A:";
		std::array< wchar_t, 1024 > device {};
		for ( wchar_t letter = L'A'; letter <= L'Z'; ++letter ) {
			drive[ 0 ] = letter;
			const DWORD length = QueryDosDeviceW(
				drive, device.data( ), static_cast< DWORD >( device.size( ) ) );
			if ( !length )
				continue;
			const std::wstring_view prefix( device.data( ) );
			if ( path.size( ) <= prefix.size( )
				|| _wcsnicmp( path.data( ), prefix.data( ), prefix.size( ) ) != 0
				|| path[ prefix.size( ) ] != L'\\' )
				continue;
			return std::wstring( drive ) + std::wstring( path.substr( prefix.size( ) ) );
		}
		return {};
	}

	remote_module_t find_remote_module( std::uint32_t process_id, const wchar_t* name ) {
		remote_module_t result {};
		const HANDLE snapshot = CreateToolhelp32Snapshot(
			TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process_id );
		if ( snapshot != INVALID_HANDLE_VALUE ) {
			MODULEENTRY32W entry {};
			entry.dwSize = sizeof( entry );
			if ( Module32FirstW( snapshot, &entry ) ) {
				do {
					if ( _wcsicmp( entry.szModule, name ) != 0 )
						continue;
					result.base = reinterpret_cast< std::uintptr_t >( entry.modBaseAddr );
					wcscpy_s( result.path, entry.szExePath );
					break;
				} while ( Module32NextW( snapshot, &entry ) );
			}
			CloseHandle( snapshot );
			if ( result.base )
				return result;
		}

		const HANDLE process = OpenProcess(
			PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process_id );
		if ( !process )
			return result;
		using nt_query_virtual_memory_t = NTSTATUS( NTAPI* )(
			HANDLE, void*, std::int32_t, void*, std::size_t, std::size_t* );
		const auto ntdll = GetModuleHandleW( L"ntdll.dll" );
		const auto query = ntdll
			? reinterpret_cast< nt_query_virtual_memory_t >(
				GetProcAddress( ntdll, "NtQueryVirtualMemory" ) )
			: nullptr;
		if ( !query ) {
			CloseHandle( process );
			return result;
		}

		std::array< std::uint8_t, 32 * 1024 > path_buffer {};
		std::uintptr_t current {};
		while ( true ) {
			MEMORY_BASIC_INFORMATION memory {};
			if ( VirtualQueryEx( process, reinterpret_cast< const void* >( current ),
				&memory, sizeof( memory ) ) != sizeof( memory ) )
				break;
			const auto next = reinterpret_cast< std::uintptr_t >( memory.BaseAddress )
				+ memory.RegionSize;
			if ( next <= current )
				break;
			current = next;
			if ( memory.Type != MEM_IMAGE && memory.Type != MEM_MAPPED )
				continue;

			std::size_t returned {};
			const auto status = query( process, memory.BaseAddress,
				2, path_buffer.data( ),
				path_buffer.size( ), &returned );
			if ( status < 0 || returned < sizeof( UNICODE_STRING ) )
				continue;
			const auto path = reinterpret_cast< const UNICODE_STRING* >( path_buffer.data( ) );
			const auto buffer_begin = reinterpret_cast< std::uintptr_t >( path_buffer.data( ) );
			const auto buffer_end = buffer_begin + path_buffer.size( );
			const auto text_begin = reinterpret_cast< std::uintptr_t >( path->Buffer );
			if ( !path->Buffer || !path->Length || path->Length % sizeof( wchar_t )
				|| text_begin < buffer_begin || text_begin + path->Length > buffer_end )
				continue;

			const std::wstring mapped_path(
				path->Buffer, path->Length / sizeof( wchar_t ) );
			const auto separator = mapped_path.find_last_of( L"\\/" );
			const std::wstring_view basename = separator == std::wstring::npos
				? std::wstring_view( mapped_path )
				: std::wstring_view( mapped_path ).substr( separator + 1 );
			if ( _wcsicmp( basename.data( ), name ) != 0 )
				continue;

			result.base = reinterpret_cast< std::uintptr_t >( memory.AllocationBase );
			const std::wstring dos_path = dos_path_from_device_path( mapped_path );
			if ( !dos_path.empty( ) && dos_path.size( ) < std::size( result.path ) )
				wcscpy_s( result.path, dos_path.c_str( ) );
			break;
		}
		if ( result.base && !result.path[ 0 ] ) {
			DWORD length = static_cast< DWORD >( std::size( result.path ) );
			if ( QueryFullProcessImageNameW( process, 0, result.path, &length ) ) {
				std::wstring candidate( result.path, length );
				const auto separator = candidate.find_last_of( L"\\/" );
				if ( separator != std::wstring::npos ) {
					candidate.resize( separator + 1 );
					candidate += name;
					if ( candidate.size( ) < std::size( result.path )
						&& GetFileAttributesW( candidate.c_str( ) ) != INVALID_FILE_ATTRIBUTES )
						wcscpy_s( result.path, candidate.c_str( ) );
					else
						result.path[ 0 ] = L'\0';
				}
			}
		}
		CloseHandle( process );
		return result;
	}

	bool executable_address( std::uintptr_t address ) {
		MEMORY_BASIC_INFORMATION memory {};
		if ( VirtualQuery( reinterpret_cast< const void* >( address ), &memory,
			sizeof( memory ) ) != sizeof( memory ) )
			return false;

		const DWORD protection = memory.Protect & 0xffu;
		return memory.State == MEM_COMMIT
			&& ( protection == PAGE_EXECUTE
				|| protection == PAGE_EXECUTE_READ
				|| protection == PAGE_EXECUTE_READWRITE
				|| protection == PAGE_EXECUTE_WRITECOPY );
	}

	realtime_wrapper_t find_realtime_wrapper( HMODULE game_assembly ) {
		const auto base = reinterpret_cast< std::uintptr_t >( game_assembly );
		const auto dos = reinterpret_cast< const IMAGE_DOS_HEADER* >( base );
		if ( dos->e_magic != IMAGE_DOS_SIGNATURE )
			return {};

		const auto nt = reinterpret_cast< const IMAGE_NT_HEADERS64* >(
			base + dos->e_lfanew );
		if ( nt->Signature != IMAGE_NT_SIGNATURE
			|| nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC )
			return {};

		const auto sections = IMAGE_FIRST_SECTION( nt );
		const auto image_size = static_cast< std::size_t >(
			nt->OptionalHeader.SizeOfImage );
		const auto needle = reinterpret_cast< const std::uint8_t* >(
			realtime_icall_name );
		const auto needle_size = std::strlen( realtime_icall_name ) + 1;
		std::uintptr_t icall_name_address {};

		for ( WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index ) {
			const auto section_offset = static_cast< std::size_t >(
				sections[ index ].VirtualAddress );
			const auto section_size = static_cast< std::size_t >(
				sections[ index ].Misc.VirtualSize );
			if ( !section_size
				|| section_offset >= image_size
				|| section_size > image_size - section_offset
				|| ( sections[ index ].Characteristics & IMAGE_SCN_MEM_READ ) == 0 )
				continue;

			const auto begin = reinterpret_cast< const std::uint8_t* >(
				base + section_offset );
			const auto end = begin + section_size;
			auto cursor = begin;
			while ( cursor < end ) {
				const auto match = std::search(
					cursor, end, needle, needle + needle_size );
				if ( match == end )
					break;
				if ( icall_name_address )
					return {};
				icall_name_address = reinterpret_cast< std::uintptr_t >( match );
				cursor = match + needle_size;
			}
		}
		if ( !icall_name_address )
			return {};

		const auto resolver_export = reinterpret_cast< const std::uint8_t* >(
			GetProcAddress( game_assembly, "il2cpp_resolve_icall" ) );
		if ( !resolver_export )
			return {};

		auto resolver = reinterpret_cast< std::uintptr_t >( resolver_export );
		if ( resolver_export[ 0 ] == 0xe9 ) {
			std::int32_t displacement {};
			std::memcpy( &displacement, resolver_export + 1, sizeof( displacement ) );
			resolver = reinterpret_cast< std::uintptr_t >( resolver_export + 5 )
				+ displacement;
		}

		static constexpr std::uint8_t prologue[ ] = {
			0x48, 0x83, 0xec, 0x28
		};
		static constexpr std::uint8_t load_and_branch[ ] = {
			0x48, 0x85, 0xc0, 0x75, 0x18
		};
		static constexpr std::uint8_t resolve_check[ ] = {
			0x48, 0x85, 0xc0, 0x74, 0x0e
		};
		static constexpr std::uint8_t tail_call[ ] = {
			0x48, 0x83, 0xc4, 0x28, 0x48, 0xff, 0xe0
		};

		realtime_wrapper_t result {};
		for ( WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index ) {
			const auto section_offset = static_cast< std::size_t >(
				sections[ index ].VirtualAddress );
			const auto section_size = static_cast< std::size_t >(
				sections[ index ].Misc.VirtualSize );
			if ( section_size < 47
				|| section_offset >= image_size
				|| section_size > image_size - section_offset
				|| ( sections[ index ].Characteristics & IMAGE_SCN_MEM_EXECUTE ) == 0 )
				continue;

			const auto begin = reinterpret_cast< const std::uint8_t* >(
				base + section_offset );
			const auto end = begin + section_size - 47;
			for ( auto code = begin; code <= end; ++code ) {
				if ( std::memcmp( code, prologue, sizeof( prologue ) ) != 0
					|| std::memcmp( code + 4, "\x48\x8b\x05", 3 ) != 0
					|| std::memcmp( code + 11, load_and_branch,
						sizeof( load_and_branch ) ) != 0
					|| std::memcmp( code + 16, "\x48\x8d\x0d", 3 ) != 0
					|| code[ 23 ] != 0xe8
					|| std::memcmp( code + 28, resolve_check,
						sizeof( resolve_check ) ) != 0
					|| std::memcmp( code + 33, "\x48\x89\x05", 3 ) != 0
					|| std::memcmp( code + 40, tail_call, sizeof( tail_call ) ) != 0 )
					continue;

				std::int32_t load_displacement {};
				std::int32_t name_displacement {};
				std::int32_t call_displacement {};
				std::int32_t store_displacement {};
				std::memcpy( &load_displacement, code + 7,
					sizeof( load_displacement ) );
				std::memcpy( &name_displacement, code + 19,
					sizeof( name_displacement ) );
				std::memcpy( &call_displacement, code + 24,
					sizeof( call_displacement ) );
				std::memcpy( &store_displacement, code + 36,
					sizeof( store_displacement ) );

				const auto load_target =
					reinterpret_cast< std::uintptr_t >( code + 11 )
					+ load_displacement;
				const auto name_target =
					reinterpret_cast< std::uintptr_t >( code + 23 )
					+ name_displacement;
				const auto call_target =
					reinterpret_cast< std::uintptr_t >( code + 28 )
					+ call_displacement;
				const auto store_target =
					reinterpret_cast< std::uintptr_t >( code + 40 )
					+ store_displacement;
				if ( name_target != icall_name_address
					|| call_target != resolver
					|| load_target != store_target )
					continue;

				if ( result.function )
					return {};
				result.function = reinterpret_cast< std::uintptr_t >( code );
				result.cached_icall = load_target;
			}
		}
		return result;
	}

	bool register_data_sections( HMODULE module, std::uintptr_t remote_base ) {
		const auto local_base = reinterpret_cast< std::uintptr_t >( module );
		const auto dos = reinterpret_cast< const IMAGE_DOS_HEADER* >( local_base );
		if ( dos->e_magic != IMAGE_DOS_SIGNATURE )
			return false;

		const auto nt = reinterpret_cast< const IMAGE_NT_HEADERS64* >(
			local_base + dos->e_lfanew );
		if ( nt->Signature != IMAGE_NT_SIGNATURE
			|| nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC )
			return false;

		const auto sections = IMAGE_FIRST_SECTION( nt );
		for ( WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index ) {
			char name[ 9 ] {};
			std::memcpy( name, sections[ index ].Name, 8 );

			Nexus::Policy policy {};
			if ( std::strcmp( name, ".rdata" ) == 0 )
				policy = Nexus::Policy::StaticOnce;
			else if ( std::strcmp( name, ".data" ) == 0
				|| std::strcmp( name, ".bss" ) == 0 )
				policy = Nexus::Policy::RefreshPerCall;
			else
				continue;

			auto size = static_cast< std::size_t >(
				sections[ index ].Misc.VirtualSize );
			if ( !size )
				size = sections[ index ].SizeOfRawData;
			if ( !size )
				continue;

			auto run_start = local_base + sections[ index ].VirtualAddress;
			const auto section_end = run_start + size;
			while ( run_start < section_end ) {
				MEMORY_BASIC_INFORMATION memory {};
				if ( VirtualQuery( reinterpret_cast< const void* >( run_start ),
					&memory, sizeof( memory ) ) != sizeof( memory ) )
					return false;

				const auto memory_end =
					reinterpret_cast< std::uintptr_t >( memory.BaseAddress )
					+ memory.RegionSize;
				const auto run_end = ( std::min )( memory_end, section_end );
				const auto protection = memory.Protect & 0xffu;
				const bool readable = memory.State == MEM_COMMIT
					&& ( memory.Protect & PAGE_GUARD ) == 0
					&& protection != PAGE_NOACCESS;
				if ( readable && !bypass::nexus::add_section(
					run_start,
					remote_base + ( run_start - local_base ),
					run_end - run_start,
					policy,
					name ) ) {
					logging::print(
						"nexus section failed: %s at 0x%llx (%lu)",
						name,
						static_cast< unsigned long long >( run_start ),
						static_cast< unsigned long >( GetLastError( ) ) );
					return false;
				}
				run_start = run_end;
			}
		}
		return true;
	}

	std::uintptr_t find_realtime_since_startup( ) {
#ifndef _WIN64
		logging::print( "the nexus call test requires an x64 build" );
		return false;
#else
		const auto game_assembly = find_remote_module(
			bypass::target::m_process_pid, game_assembly_name );
		const auto unity_player = find_remote_module(
			bypass::target::m_process_pid, unity_player_name );
		const auto baselib = find_remote_module(
			bypass::target::m_process_pid, baselib_name );
		if ( !game_assembly.base || !game_assembly.path[ 0 ] ) {
			logging::print( "failed to find %ls", game_assembly_name );
			return false;
		}
		if ( !unity_player.base || !unity_player.path[ 0 ] ) {
			logging::print( "failed to find %ls", unity_player_name );
			return false;
		}

		const HMODULE local_game_assembly = LoadLibraryExW(
			game_assembly.path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH );
		const HMODULE local_unity_player = LoadLibraryExW(
			unity_player.path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH );
		if ( !local_game_assembly || !local_unity_player ) {
			logging::print( "failed to load Unity modules (%lu)",
				static_cast< unsigned long >( GetLastError( ) ) );
			return false;
		}

		const HMODULE local_baselib = GetModuleHandleW( baselib_name );
		if ( baselib.base && local_baselib
			&& reinterpret_cast< std::uintptr_t >( local_baselib ) != baselib.base ) {
			logging::print( "local and remote baselib module bases differ" );
			return false;
		}
		if ( reinterpret_cast< std::uintptr_t >( local_game_assembly )
			!= game_assembly.base
			|| reinterpret_cast< std::uintptr_t >( local_unity_player )
			!= unity_player.base ) {
			logging::print( "local and remote Unity module bases differ" );
			return false;
		}

		const auto wrapper = find_realtime_wrapper( local_game_assembly );
		if ( !wrapper.function || !wrapper.cached_icall ) {
			logging::print( "failed to find the realtimeSinceStartup wrapper" );
			return false;
		}

		std::uintptr_t cached_icall {};
		if ( !bypass::target::read_memory(
			wrapper.cached_icall, &cached_icall, sizeof( cached_icall ) )
			|| !cached_icall
			|| !executable_address( cached_icall ) ) {
			logging::print( "the target realtimeSinceStartup icall is not ready" );
			return false;
		}

		if ( !bypass::nexus::attach( ) ) {
			logging::print( "failed to attach nexus (%lu)",
				static_cast< unsigned long >( GetLastError( ) ) );
			return false;
		}

		Nexus::Config config {};
		config.ReadBackend = Nexus::ReadBackend::BypassChannel;
		config.AllowArbitraryHeapShadow = true;
		config.ValidateHeapWithVirtualQueryEx = false;
		config.KeepHeapPagesMapped = true;
		config.AllowLocalWrites = true;
		config.RestoreDirtyStaticPages = true;
		config.CoalesceSectionRefresh = true;
		if ( !bypass::nexus::configure( config ) ) {
			logging::print( "failed to configure nexus (%lu)",
				static_cast< unsigned long >( GetLastError( ) ) );
			bypass::nexus::detach( );
			return false;
		}

		if ( !register_data_sections( local_game_assembly, game_assembly.base )
			|| !register_data_sections( local_unity_player, unity_player.base ) ) {
			logging::print( "failed to register Unity data sections" );
			bypass::nexus::detach( );
			return false;
		}

		return wrapper.function;
#endif
	}

}

DWORD WINAPI entry_point( LPVOID ) {
	logging::print( "rust-external" );

	if ( !bypass::open_shared_data( ) ) {
		logging::print( "failed to open shared data" );
		return 1;
	}
	if ( !bypass::initialize( test::target_executable, L"" ) ) {
		logging::print( "failed to initialize bypass" );
		return 1;
	}
	if ( !bypass::create( ) ) {
		logging::print( "failed to create bypass" );
		return 1;
	}
	if ( !bypass::target::setup( ) ) {
		logging::print( "failed to setup target" );
		bypass::cleanup( );
		return 1;
	}

	const auto address = test::find_realtime_since_startup( );
	if ( !address ) {
		bypass::cleanup( );
		return 1;
	}

	nexus::function_t function(
		"Time.realtimeSinceStartupAsDouble", address );
	if ( !function.valid( ) ) {
		logging::print( "failed to create the nexus function wrapper" );
		bypass::nexus::detach( );
		bypass::cleanup( );
		return 1;
	}

	nexus::registers_t parameters {};
	Nexus::CallReport report {};
	const bool succeeded = function.call(
		parameters, Nexus::CallMode::FaultDrivenFresh, &report );
	if ( !succeeded ) {
		logging::print( "nexus call failed (%lu)",
			static_cast< unsigned long >( report.Error ) );
	} else {
		double value {};
		std::memcpy( &value, parameters.xmm0.data( ), sizeof( value ) );
		logging::print( "realtimeSinceStartupAsDouble = %.7f", value );
	}

	bypass::nexus::detach( );

	bypass::cleanup( );
	return succeeded ? 0 : 1;
}

BOOL WINAPI DllMain( HMODULE module, DWORD reason, LPVOID ) {
	if ( reason != DLL_PROCESS_ATTACH )
		return TRUE;

	DisableThreadLibraryCalls( module );
	const HANDLE thread = CreateThread(
		nullptr, 0, entry_point, nullptr, 0, nullptr );
	if ( !thread )
		return FALSE;

	CloseHandle( thread );
	return TRUE;
}
