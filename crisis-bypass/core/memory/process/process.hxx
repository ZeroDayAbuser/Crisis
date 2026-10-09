#pragma once

namespace process {
	static HWND g_target_window = nullptr;

	static bool is_target_alive( ) {
		return g_target_window && ::IsWindow( g_target_window );
	}

	static std::optional<std::uint64_t> find_eprocess( std::uint32_t pid ) {
		const auto s_head      = g_scanner->get_symbol( obf( "PsActiveProcessHead" ) );
		const auto off_pid   = g_scanner->get_member( obf( "_EPROCESS" ), obf( "UniqueProcessId" ) );
		const auto off_links = g_scanner->get_member( obf( "_EPROCESS" ), obf( "ActiveProcessLinks" ) );

		if ( !s_head ) {
			logging::print( obf( "failed to find active process head" ) );
			return std::nullopt;
		}

		std::uint64_t flink = 0;
		if ( !memory::read_system( s_head, &flink, 8 ) || !flink ) {
			logging::print( obf( "failed to read active process head flink" ) );
			return std::nullopt;
		}

		for ( auto idx = 0; flink && flink != s_head && idx < 0x1000; ++idx ) {
			const auto eprocess = flink - off_links;

			std::uint64_t cur_pid = 0;
			if ( !memory::read_system( eprocess + off_pid, &cur_pid, 8 ) )
				break;

			if ( static_cast< std::uint32_t >( cur_pid ) == pid )
				return eprocess;

			std::uint64_t next = 0;
			if ( !memory::read_system( flink, &next, 8 ) )
				break;
			flink = next;
		}

		logging::print( obf( "could not find eprocess." ) );
		return std::nullopt;
	}

	std::uintptr_t get_module_export( std::uintptr_t base, const char* name ) {
		if ( !base || !name ) return 0;

		IMAGE_DOS_HEADER dos {};
		if ( !memory::read_virtual( base, &dos, sizeof( dos ) ) || dos.e_magic != IMAGE_DOS_SIGNATURE )
			return 0;

		IMAGE_NT_HEADERS64 nt {};
		if ( !memory::read_virtual( base + static_cast< std::uint32_t >( dos.e_lfanew ), &nt, sizeof( nt ) ) ||
		     nt.Signature != IMAGE_NT_SIGNATURE )
			return 0;

		const auto& exp_dir = nt.OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT ];
		if ( !exp_dir.VirtualAddress ) return 0;

		IMAGE_EXPORT_DIRECTORY exp {};
		if ( !memory::read_virtual( base + exp_dir.VirtualAddress, &exp, sizeof( exp ) ) )
			return 0;

		for ( std::uint32_t i = 0; i < exp.NumberOfNames; ++i ) {
			std::uint32_t name_rva = 0;
			if ( !memory::read_virtual( base + exp.AddressOfNames + i * sizeof( name_rva ),
					&name_rva, sizeof( name_rva ) ) )
				break;

			char candidate[ 256 ] {};
			if ( !memory::read_virtual( base + name_rva, candidate, sizeof( candidate ) - 1 ) )
				continue;

			if ( !strcmp( candidate, name ) ) {
				std::uint16_t ordinal = 0;
				std::uint32_t function_rva = 0;
				if ( !memory::read_virtual( base + exp.AddressOfNameOrdinals + i * sizeof( ordinal ),
						&ordinal, sizeof( ordinal ) ) )
					break;
				if ( !memory::read_virtual( base + exp.AddressOfFunctions + ordinal * sizeof( function_rva ),
						&function_rva, sizeof( function_rva ) ) )
					break;
				return base + function_rva;
			}
		}

		return 0;
	}

	 uintptr_t get_module_base( std::uint64_t process_peb, const wchar_t* name ) {
		if ( !name ) return 0;

		std::uintptr_t result = 0;

		peb64_t peb {};
		if ( memory::read_virtual( process_peb, &peb, sizeof( peb ) ) && peb.m_ldr ) {
			peb_ldr_data64_t ldr {};
			if ( memory::read_virtual( peb.m_ldr, &ldr, sizeof( ldr ) ) ) {
				const auto list_head = peb.m_ldr +
					offsetof( peb_ldr_data64_t, m_in_load_order_module_list );
				auto current = ldr.m_in_load_order_module_list.m_flink;

				for ( std::size_t n = 0; current && current != list_head && n < 0x1000; ++n ) {
					ldr_data_table_entry64_t entry {};
					if ( !memory::read_virtual( current, &entry, sizeof( entry ) ) ) break;

					const auto& bn = entry.m_base_dll_name;
					if ( bn.m_buffer && bn.m_length &&
						!( bn.m_length % sizeof( wchar_t ) ) &&
						bn.m_length < MAX_PATH * sizeof( wchar_t ) ) {
						wchar_t name_buf[ MAX_PATH ] {};
						if ( memory::read_virtual( bn.m_buffer, name_buf, bn.m_length ) ) {
							name_buf[ bn.m_length / sizeof( wchar_t ) ] = L'\0';
							if ( !_wcsicmp( name_buf, name ) ) {
								result = static_cast< std::uintptr_t >( entry.m_dll_base );
								break;
							}
						}
					}
					const auto next = entry.m_in_load_order_links.m_flink;
					if ( next == current ) break;
					current = next;
				}
			}
		}

		return result;
	}

	 std::wstring get_module_path( std::uint64_t process_peb, const wchar_t* name ) {
		 if ( !name ) return {};

		 peb64_t peb {};
		 if ( !memory::read_virtual( process_peb, &peb, sizeof( peb ) ) || !peb.m_ldr )
			 return {};

		 peb_ldr_data64_t ldr {};
		 if ( !memory::read_virtual( peb.m_ldr, &ldr, sizeof( ldr ) ) )
			 return {};

		 const auto list_head = peb.m_ldr +
			 offsetof( peb_ldr_data64_t, m_in_load_order_module_list );
		 auto current = ldr.m_in_load_order_module_list.m_flink;

		 for ( std::size_t n = 0; current && current != list_head && n < 0x1000; ++n ) {
			 ldr_data_table_entry64_t entry {};
			 if ( !memory::read_virtual( current, &entry, sizeof( entry ) ) ) break;

			 const auto& bn = entry.m_base_dll_name;
			 if ( bn.m_buffer && bn.m_length &&
				 !( bn.m_length % sizeof( wchar_t ) ) &&
				 bn.m_length < MAX_PATH * sizeof( wchar_t ) )
			 {
				 wchar_t name_buf[ MAX_PATH ] {};
				 if ( memory::read_virtual( bn.m_buffer, name_buf, bn.m_length ) ) {
					 name_buf[ bn.m_length / sizeof( wchar_t ) ] = L'\0';

					 if ( !_wcsicmp( name_buf, name ) ) {
						 const auto& fp = entry.m_full_dll_name;
						 if ( !fp.m_buffer || !fp.m_length ||
							 ( fp.m_length % sizeof( wchar_t ) ) ||
							 fp.m_length >= MAX_PATH * sizeof( wchar_t ) )
							 return {};

						 wchar_t path_buf[ MAX_PATH ] {};
						 if ( !memory::read_virtual( fp.m_buffer, path_buf, fp.m_length ) )
							 return {};

						 path_buf[ fp.m_length / sizeof( wchar_t ) ] = L'\0';
						 return std::wstring( path_buf );
					 }
				 }
			 }

			 const auto next = entry.m_in_load_order_links.m_flink;
			 if ( next == current ) break;
			 current = next;
		 }

		 return {};
	 }

	std::uint64_t pattern_scan( std::uint64_t base, const char* pattern, int offset, bool relative ) {
		if ( !base || !pattern ) return 0;

		std::vector< std::int16_t > bytes;
		const char* p = pattern;
		while ( *p ) {
			while ( *p == ' ' ) ++p;
			if ( !*p ) break;
			if ( p[ 0 ] == '?' ) {
				bytes.push_back( -1 );
				if ( p[ 1 ] == '?' ) ++p;
				++p;
			}
			else {
				char hex[ 3 ] = { p[ 0 ], p[ 1 ], 0 };
				bytes.push_back( static_cast< std::int16_t >( std::strtoul( hex, nullptr, 16 ) ) );
				p += 2;
			}
		}
		if ( bytes.empty( ) ) return 0;

		// `base` belongs to the target process.  It is not mapped in this DLL's
		// address space, so every access must go through the paging-backed reader.
		IMAGE_DOS_HEADER dos {};
		if ( !memory::read_virtual( base, &dos, sizeof( dos ) ) ||
			dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
			dos.e_lfanew > 0x100000 )
			return 0;

		IMAGE_NT_HEADERS64 nt {};
		if ( !memory::read_virtual( base + static_cast< std::uint32_t >( dos.e_lfanew ),
			&nt, sizeof( nt ) ) ||
			nt.Signature != IMAGE_NT_SIGNATURE ||
			nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC )
			return 0;

		const auto scan_size = static_cast< std::size_t >( nt.OptionalHeader.SizeOfImage );
		if ( scan_size < bytes.size( ) || scan_size > 0x80000000ull ) return 0;

		constexpr std::size_t page_size = 0x1000;
		std::vector< std::uint8_t > page( page_size + bytes.size( ) - 1 );

		for ( std::size_t page_rva = 0; page_rva < scan_size; page_rva += page_size ) {
			const auto remaining = scan_size - page_rva;
			const auto bytes_to_read = ( std::min ) (
				page.size( ), remaining );

			// Reading the overlap in one operation lets signatures crossing a page
			// boundary match. If the following page is absent, retry this page alone.
			auto readable = bytes_to_read;
			if ( !memory::read_virtual( base + page_rva, page.data( ), readable ) ) {
				readable = ( std::min ) ( page_size, remaining );
				if ( !memory::read_virtual( base + page_rva, page.data( ), readable ) )
					continue;
			}

			const auto candidates = ( std::min ) ( page_size, remaining );
			for ( std::size_t i = 0; i < candidates && i + bytes.size( ) <= readable; ++i ) {
				bool found = true;
				for ( std::size_t j = 0; j < bytes.size( ); ++j ) {
					if ( bytes[ j ] != -1 && page[ i + j ] !=
						static_cast< std::uint8_t >( bytes[ j ] ) ) {
						found = false;
						break;
					}
				}
				if ( !found ) continue;

				const auto match = base + page_rva + i;
				if ( offset < 0 && static_cast< std::uint64_t >( -static_cast< std::int64_t >( offset ) ) > match )
					return 0;
				auto result = static_cast< std::uint64_t >(
					static_cast< std::int64_t >( match ) + offset );

				if ( relative ) {
					std::int32_t displacement = 0;
					if ( !memory::read_virtual( result, &displacement, sizeof( displacement ) ) )
						return 0;
					result = static_cast< std::uint64_t >(
						static_cast< std::int64_t >( result + sizeof( displacement ) ) + displacement );
				}
				return result;
			}
		}
		return 0;
	}
}
