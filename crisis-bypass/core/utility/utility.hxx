#pragma once

namespace logging {
	template<typename... Args>
	static void print( const char* fmt, Args... args ) {
		char msg[ 512 ];
		sprintf_s( msg, fmt, args... );
		SYSTEMTIME t { };
		GetLocalTime( &t );
		char out[ 600 ];
		sprintf_s( out, "[%02d:%02d:%02d] [crisis-bypass] %s\n", t.wHour, t.wMinute, t.wSecond, msg );
		OutputDebugStringA( out );
	}
}

namespace utility {
	std::wstring ansi_to_wstring( const std::string& input, DWORD locale = 0 ) {
		if ( input.empty( ) ) return std::wstring( );

		int size_needed = MultiByteToWideChar( locale, 0, input.c_str( ), ( int ) input.length( ), nullptr, 0 );
		std::wstring result( size_needed, 0 );
		MultiByteToWideChar( locale, 0, input.c_str( ), ( int ) input.length( ), &result[ 0 ], size_needed );
		return result;
	}

	std::string wstring_to_ansi( const std::wstring& input, DWORD locale = 0 ) {
		if ( input.empty( ) ) return std::string( );

		int size_needed = WideCharToMultiByte( locale, 0, input.c_str( ), ( int ) input.length( ), nullptr, 0, nullptr, nullptr );
		std::string result( size_needed, 0 );
		WideCharToMultiByte( locale, 0, input.c_str( ), ( int ) input.length( ), &result[ 0 ], size_needed, nullptr, nullptr );
		return result;
	}

	static std::uint32_t find_pid( const wchar_t* process_name ) {
		auto snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
		if ( snap == INVALID_HANDLE_VALUE ) return 0;

		PROCESSENTRY32W entry { sizeof( entry ) };
		std::uint32_t pid = 0;
		if ( Process32FirstW( snap, &entry ) ) {
			do {
				if ( !_wcsicmp( entry.szExeFile, process_name ) ) {
					pid = entry.th32ProcessID;
					break;
				}
			} while ( Process32NextW( snap, &entry ) );
		}
		CloseHandle( snap );
		return pid;
	}

	static HWND find_window_for_pid( DWORD pid ) {
		struct ctx_t { DWORD pid; HWND hwnd; };
		ctx_t ctx { pid, nullptr };
		EnumWindows( [ ]( HWND w, LPARAM lp ) -> BOOL {
			auto* c = reinterpret_cast< ctx_t* >( lp );
			DWORD wpid = 0;
			GetWindowThreadProcessId( w, &wpid );
			if ( wpid == c->pid && IsWindowVisible( w ) ) { c->hwnd = w; return FALSE; }
			return TRUE;
			}, reinterpret_cast< LPARAM >( &ctx ) );
		return ctx.hwnd;
	}

	std::pair<std::uint32_t, bool> wait_for_pid(
		std::wstring process_name,
		std::chrono::milliseconds threshold = std::chrono::milliseconds( 1500 ) ) {

		const auto start = std::chrono::steady_clock::now( );
		std::uint32_t pid = 0;
		while ( !( pid = find_pid( process_name.c_str( ) ) ) )
			std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

		const auto elapsed = std::chrono::steady_clock::now( ) - start;
		return { pid, elapsed < threshold };
	}

	static bool is_physical_address_valid( std::uint64_t addr, std::size_t size ) {
		if ( !addr || !size ) return false;
		constexpr auto max_pa = 0x800000000000ull;
		return addr < max_pa && ( addr + size ) <= max_pa;
	}

	static std::uint32_t get_kernel_size( const char* module_name ) {
		ULONG size = 0;
		NtQuerySystemInformation( static_cast< SYSTEM_INFORMATION_CLASS >( 11 ), nullptr, 0, &size );
		if ( !size ) return 0;
		std::vector<std::uint8_t> buf( static_cast<std::size_t>( size ) + 0x1000 );
		if ( !NT_SUCCESS( NtQuerySystemInformation( static_cast< SYSTEM_INFORMATION_CLASS >( 11 ),
			buf.data( ), static_cast<ULONG>( buf.size( ) ), &size ) ) )
			return 0;
		auto* mods = reinterpret_cast< rtl_process_modules_t* >( buf.data( ) );
		for ( std::uint32_t i = 0; i < mods->m_count; ++i ) {
			auto& m = mods->m_modules[ i ];
			const char* name = reinterpret_cast<const char*>( m.m_full_path + m.m_offset_to_file_name );
			if ( !_stricmp( name, module_name ) )
				return m.m_image_size;
		}
		return 0;
	}

	static std::uint64_t get_kernel_module( const char* module_name ) {
		ULONG size = 0;
		NtQuerySystemInformation(
			static_cast< SYSTEM_INFORMATION_CLASS >( 11 ),
			nullptr, 0, &size );

		if ( !size ) return 0;
		std::vector<std::uint8_t> buf( size + 0x1000 );
		if ( !NT_SUCCESS( NtQuerySystemInformation(
			static_cast< SYSTEM_INFORMATION_CLASS >( 11 ),
			buf.data( ), static_cast< ULONG >( buf.size( ) ), &size ) ) )
			return 0;

		auto* modules = reinterpret_cast< rtl_process_modules_t* >( buf.data( ) );
		for ( std::uint32_t i = 0; i < modules->m_count; ++i ) {
			auto& m = modules->m_modules[ i ];
			const char* name = reinterpret_cast< const char* >(
				m.m_full_path + m.m_offset_to_file_name );
			if ( !_stricmp( name, module_name ) )
				return reinterpret_cast< std::uint64_t >( m.m_image_base );
		}
		return 0;
	}

	std::string format_name( std::string process_name ) {
		std::string name( process_name.begin( ), process_name.end( ) );
		auto dot = name.find( '.' );
		if ( dot != std::string::npos )
			name = name.substr( 0, dot );
		auto dash = name.find( '-' );
		if ( dash != std::string::npos )
			name = name.substr( 0, dash );
		std::size_t start = std::string::npos;
		for ( std::size_t i = 0; i < name.size( ); ++i ) {
			if ( std::isupper( static_cast< unsigned char >( name[ i ] ) ) ) {
				if ( start == std::string::npos ) {
					start = i;
				}
				else {
					name = name.substr( start, i - start );
					break;
				}
			}
		}
		if ( start != std::string::npos && start > 0 )
			name = name.substr( start );
		std::transform( name.begin( ), name.end( ), name.begin( ),
			[ ]( unsigned char c ) { return std::tolower( c ); } );
		return name;
	}

	std::uint64_t create_virtual_address( std::uint32_t pml4_index, std::uint32_t pdpt_index = 0, bool randomize_offset = false ) {
		std::uint64_t virtual_address =
			( static_cast< std::uint64_t >( pml4_index ) << 39 ) |
			( static_cast< std::uint64_t >( pdpt_index ) << 30 );

		if ( randomize_offset )
			virtual_address |= static_cast< std::uint64_t >( rand( ) % 512 ) << 21;

		return virtual_address;
	}

	std::uint64_t find_signature( const uint8_t* data, size_t size, const std::string& pattern ) {
		std::vector<int> bytes;
		std::istringstream stream( pattern );
		std::string byte;

		while ( stream >> byte ) {
			if ( byte == obf( "??" ) || byte == obf( "?" ) ) {
				bytes.push_back( -1 );
			}
			else {
				bytes.push_back( static_cast< int >( std::strtoul( byte.c_str( ), nullptr, 16 ) ) );
			}
		}

		auto pattern_length = bytes.size( );
		for ( auto idx = 0; idx < size - pattern_length; idx++ ) {
			bool found = true;
			for ( auto j = 0; j < pattern_length; j++ ) {
				if ( bytes[ j ] != -1 && data[ idx + j ] != bytes[ j ] ) {
					found = false;
					break;
				}
			}

			if ( found )
				return idx;
		}

		return 0;
	}

	uintptr_t resolve_relative_address( const std::vector<uint8_t>& image, size_t addr,
		size_t offset_off, size_t instr_len ) {
		auto dos = PIMAGE_DOS_HEADER( image.data( ) );
		if ( dos->e_magic != IMAGE_DOS_SIGNATURE ) return 0;

		auto nt = PIMAGE_NT_HEADERS( image.data( ) + dos->e_lfanew );
		if ( nt->Signature != IMAGE_NT_SIGNATURE ) return 0;

		auto sec = IMAGE_FIRST_SECTION( nt );
		uintptr_t rva = 0;
		for ( size_t i = 0, n = nt->FileHeader.NumberOfSections; i < n; ++i, ++sec ) {
			if ( addr >= sec->PointerToRawData && addr < sec->PointerToRawData + sec->SizeOfRawData ) {
				rva = addr - sec->PointerToRawData + sec->VirtualAddress;
				break;
			}
		}

		if ( addr + offset_off + sizeof( int32_t ) > image.size( ) ) return 0;

		int32_t rel = *reinterpret_cast< const int32_t* >( &image[ addr + offset_off ] );
		return rva + instr_len + rel;
	}
}
