#pragma once

namespace memory {
	bool safe_strcmp( const char* str1, const char* str2, std::size_t max_len ) {
		std::size_t i = 0;
		while ( i < max_len ) {
			if ( str1[ i ] != str2[ i ] )
				return false;
			if ( str1[ i ] == '\0' )
				return true;
			i++;
		}
		return false;
	}

	std::size_t safe_strnlen( const char* str, std::size_t max_len ) {
		std::size_t len = 0;
		while ( len < max_len && str[ len ] != '\0' )
			len++;
		return len;
	}

	std::uint32_t rva_to_offset( nt_headers_t* nt_headers, std::uint32_t rva ) {
		if ( rva < static_cast< std::uint32_t >( nt_headers->m_size_of_headers ) )
			return rva;

		auto sections = reinterpret_cast< section_header_t* >(
			reinterpret_cast< std::uint64_t >( nt_headers ) + offsetof( nt_headers_t, m_magic ) +
			nt_headers->m_size_of_optional_header );

		for ( std::int32_t i = 0; i < nt_headers->m_number_of_sections; ++i ) {
			auto& sec = sections[ i ];
			const auto va = static_cast< std::uint32_t >( sec.m_virtual_address );
			const auto vsz = static_cast< std::uint32_t >( sec.m_virtual_size );

			if ( rva >= va && rva < va + vsz )
				return ( rva - va ) + static_cast< std::uint32_t >( sec.m_pointer_to_raw_data );
		}

		return 0;
	}

	std::uint32_t get_syscall_index( const char* export_name ) {
		std::uint32_t syscall_index = static_cast< std::uint32_t >( -1 );
		const bool is_win32k_syscall =
			export_name &&
			export_name[ 0 ] == 'N' &&
			export_name[ 1 ] == 't' &&
			export_name[ 2 ] == 'U' &&
			export_name[ 3 ] == 's' &&
			export_name[ 4 ] == 'e' &&
			export_name[ 5 ] == 'r';

		unicode_string_t file_name { };
		kernel::rtl_init_unicode_string( &file_name,
			is_win32k_syscall ?
			obf( L"\\SystemRoot\\System32\\win32u.dll" ) :
			obf( L"\\SystemRoot\\System32\\ntdll.dll" ) );

		object_attributes_t obj_attr { };
		obj_attr.m_length = sizeof( object_attributes_t );
		obj_attr.m_root_directory = nullptr;
		obj_attr.m_object_name = &file_name;
		obj_attr.m_attributes = 0x00000040L | 0x00000200L;
		obj_attr.m_security_descriptor = nullptr;

		io_status_block_t io_status { };
		void* file_handle = nullptr;

		auto status = kernel::zw_create_file(
			&file_handle,
			0x00100000L,
			&obj_attr,
			&io_status,
			nullptr,
			0x00000080,
			0x00000001 | 0x00000002,
			0x00000001,
			0x00000040 | 0x00000020,
			nullptr,
			0 );

		if ( status ) {
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: zwcreatefile failed: 0x%X\n" ), status );
			return static_cast< std::uint32_t >( -1 );
		}

		FILE_STANDARD_INFORMATION file_info;
		status = kernel::zw_query_information_file(
			file_handle,
			&io_status,
			&file_info,
			sizeof( file_info ),
			FileStandardInformation );

		if ( status ) {
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: zyqueryinformationfile failed: 0x%X\n" ), status );
			kernel::zw_close( file_handle );
			return static_cast< std::uint32_t >( -1 );
		}

		auto file_size = file_info.EndOfFile.LowPart;

		if ( file_size < 0x1000 || file_size > 0x10000000 ) {
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: Invalid file size: 0x%X\n" ), file_size );
			kernel::zw_close( file_handle );
			return static_cast< std::uint32_t >( -1 );
		}

		auto file_data = reinterpret_cast< std::uint8_t* >( kernel::mm_allocate_independent_pages( file_size ) );
		if ( !file_data ) {
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: ExAllocatePoolWithTag failed\n" ) );
			kernel::zw_close( file_handle );
			return static_cast< std::uint32_t >( -1 );
		}

		ularge_integer_t offset = { 0 };
		status = kernel::zw_read_file(
			file_handle,
			nullptr,
			nullptr,
			nullptr,
			&io_status,
			file_data,
			file_size,
			&offset,
			nullptr );

		kernel::zw_close( file_handle );

		if ( status ) {
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: ZwReadFile failed: 0x%X\n" ), status );
			kernel::mm_free_independent_pages( reinterpret_cast< std::uint64_t >( file_data ), file_size );
			return static_cast< std::uint32_t >( -1 );
		}

		auto dos_header = reinterpret_cast< dos_header_t* >( file_data );
		if ( !dos_header->is_valid( ) ) {
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: Invalid DOS header: 0x%X\n" ), dos_header->m_magic );
			kernel::mm_free_independent_pages( reinterpret_cast< std::uint64_t >( file_data ), file_size );
			return static_cast< std::uint32_t >( -1 );
		}

		if ( dos_header->m_lfanew >= file_size || dos_header->m_lfanew < sizeof( dos_header_t ) ) {
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: Invalid e_lfanew: 0x%X\n" ), dos_header->m_lfanew );
			kernel::mm_free_independent_pages( reinterpret_cast< std::uint64_t >( file_data ), file_size );
			return static_cast< std::uint32_t >( -1 );
		}

		auto nt_headers = reinterpret_cast< nt_headers_t* >( file_data + dos_header->m_lfanew );

		if ( dos_header->m_lfanew + sizeof( nt_headers_t ) > file_size ) {
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: NT headers out of bounds\n" ) );
			kernel::mm_free_independent_pages( reinterpret_cast< std::uint64_t >( file_data ), file_size );
			return static_cast< std::uint32_t >( -1 );
		}

		if ( !nt_headers->is_valid( ) ) {
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: Invalid NT headers\n" ) );
			kernel::mm_free_independent_pages( reinterpret_cast< std::uint64_t >( file_data ), file_size );
			return static_cast< std::uint32_t >( -1 );
		}

		auto export_dir_rva = static_cast< std::uint32_t >( nt_headers->m_export_table.m_virtual_address );
		auto export_dir_size = static_cast< std::uint32_t >( nt_headers->m_export_table.m_size );

		if ( export_dir_rva == 0 || export_dir_size == 0 ) {
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: No export directory found\n" ) );
			kernel::mm_free_independent_pages( reinterpret_cast< std::uint64_t >( file_data ), file_size );
			return static_cast< std::uint32_t >( -1 );
		}

		auto export_dir_offset = rva_to_offset( nt_headers, export_dir_rva );
		if ( !export_dir_offset || export_dir_offset + sizeof( export_directory_t ) > file_size ) {
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: Export directory offset invalid\n" ) );
			kernel::mm_free_independent_pages( reinterpret_cast< std::uint64_t >( file_data ), file_size );
			return static_cast< std::uint32_t >( -1 );
		}

		auto export_dir = reinterpret_cast< export_directory_t* >( file_data + export_dir_offset );
		auto number_of_names = static_cast< std::uint32_t >( export_dir->m_number_of_names );
		if ( number_of_names > 100000 ) {
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: Suspicious number of exports: %u\n" ), number_of_names );
			kernel::mm_free_independent_pages( reinterpret_cast< std::uint64_t >( file_data ), file_size );
			return static_cast< std::uint32_t >( -1 );
		}

		auto addr_of_funcoffset = rva_to_offset( nt_headers, static_cast< std::uint32_t >( export_dir->m_address_of_functions ) );
		auto addr_of_name_ordinaloffset = rva_to_offset( nt_headers, static_cast< std::uint32_t >( export_dir->m_address_of_names_ordinals ) );
		auto addr_of_nameoffset = rva_to_offset( nt_headers, static_cast< std::uint32_t >( export_dir->m_address_of_names ) );

		if ( !addr_of_funcoffset || !addr_of_name_ordinaloffset || !addr_of_nameoffset ) {
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: Export table offsets invalid\n" ) );
			kernel::mm_free_independent_pages( reinterpret_cast< std::uint64_t >( file_data ), file_size );
			return static_cast< std::uint32_t >( -1 );
		}

		if ( addr_of_funcoffset + ( number_of_names * sizeof( std::uint32_t ) ) > file_size ||
			addr_of_name_ordinaloffset + ( number_of_names * sizeof( std::uint16_t ) ) > file_size ||
			addr_of_nameoffset + ( number_of_names * sizeof( std::uint32_t ) ) > file_size ) {
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: Export arrays out of bounds\n" ) );
			kernel::mm_free_independent_pages( reinterpret_cast< std::uint64_t >( file_data ), file_size );
			return static_cast< std::uint32_t >( -1 );
		}

		auto addr_of_funcs = reinterpret_cast< std::uint32_t* >( file_data + addr_of_funcoffset );
		auto addr_of_name_ordinals = reinterpret_cast< std::uint16_t* >( file_data + addr_of_name_ordinaloffset );
		auto addr_of_names = reinterpret_cast< std::uint32_t* >( file_data + addr_of_nameoffset );

		std::uint32_t func_offset = 0;
		std::size_t export_name_len = std::strlen( export_name );

		for ( std::uint32_t i = 0; i < number_of_names; i++ ) {
			auto curr_name_offset = rva_to_offset( nt_headers, addr_of_names[ i ] );
			if ( !curr_name_offset || curr_name_offset >= file_size )
				continue;

			std::size_t remaining_bytes = file_size - curr_name_offset;
			if ( remaining_bytes < export_name_len + 1 )
				continue;

			auto curr_name = reinterpret_cast< const char* >( file_data + curr_name_offset );

			std::size_t curr_name_len = safe_strnlen( curr_name, remaining_bytes );
			if ( curr_name_len == remaining_bytes ) {
				kernel::dbg_print( obf( "[ssdt] get_syscall_index: Export name not null-terminated at 0x%X\n" ), curr_name_offset );
				continue;
			}

			if ( safe_strcmp( curr_name, export_name, remaining_bytes ) ) {
				auto ordinal = addr_of_name_ordinals[ i ];
				if ( ordinal >= export_dir->m_number_of_functions ) {
					kernel::dbg_print( obf( "[ssdt] get_syscall_index: Invalid ordinal %u for %s\n" ), ordinal, export_name );
					continue;
				}

				auto func_rva = addr_of_funcs[ ordinal ];

				if ( func_rva >= export_dir_rva && func_rva < export_dir_rva + export_dir_size ) {
					kernel::dbg_print( obf( "[ssdt] get_syscall_index: Forwarded export, ignoring: %s\n" ), export_name );
					continue;
				}

				func_offset = rva_to_offset( nt_headers, func_rva );
				break;
			}
		}

		if ( !func_offset || func_offset >= file_size ) {
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: Function %s not found in export table\n" ), export_name );
			kernel::mm_free_independent_pages( reinterpret_cast< std::uint64_t >( file_data ), file_size );
			return static_cast< std::uint32_t >( -1 );
		}

		auto func_code = file_data + func_offset;

		for ( int i = 0; i < 32 && func_offset + i + 5 <= file_size; i++ ) {
			if ( func_code[ i ] == 0xC2 || func_code[ i ] == 0xC3 )
				break;

			if ( func_code[ i ] == 0xB8 ) {
				if ( func_offset + i + 5 <= file_size ) {
					syscall_index = *reinterpret_cast< std::uint32_t* >( func_code + i + 1 );
					break;
				}
			}
		}

		if ( syscall_index == static_cast< std::uint32_t >( -1 ) )
			kernel::dbg_print( obf( "[ssdt] get_syscall_index: Syscall index not found in function %s\n" ), export_name );

		kernel::mm_free_independent_pages( reinterpret_cast< std::uint64_t >( file_data ), file_size );
		return syscall_index;
	}

	bool is_valid_service_descriptor( service_descriptor_table_t* table ) {
		if ( !kernel::mm_is_address_valid( table ) ||
			!kernel::mm_is_address_valid( table->m_service_table ) )
			return false;

		if ( !table->m_number_of_services || table->m_number_of_services > 0x2000 )
			return false;

		return true;
	}

	bool service_descriptor_matches(
		service_descriptor_table_t* left,
		service_descriptor_table_t* right
	) {
		return left->m_service_table == right->m_service_table &&
			left->m_counter_table == right->m_counter_table &&
			left->m_number_of_services == right->m_number_of_services &&
			left->m_argument_table == right->m_argument_table;
	}

	std::uint64_t find_service_descriptor_table_shadow( ) {
		static std::uint64_t table_address = 0;
		if ( table_address )
			return table_address;

		static const auto s_ke_sdt = pdb::get_symbol( obf("KeServiceDescriptorTable") );
		auto normal_table = reinterpret_cast< service_descriptor_table_t* >( s_ke_sdt );
		if ( !is_valid_service_descriptor( normal_table ) )
			return 0;

		auto dos_header = reinterpret_cast< dos_header_t* >( kernel::m_ntoskrnl_base );
		auto nt_headers = reinterpret_cast< nt_headers_t* >(
			kernel::m_ntoskrnl_base + dos_header->m_lfanew );
		if ( !dos_header->is_valid( ) || !nt_headers->is_valid( ) )
			return 0;

		auto section = reinterpret_cast< section_header_t* >(
			reinterpret_cast< std::uintptr_t >( nt_headers ) +
			nt_headers->m_size_of_optional_header + 0x18 );

		for ( auto section_index = 0; section_index < nt_headers->m_number_of_sections; section_index++ ) {
			auto characteristics = section[ section_index ].m_characteristics;
			if ( characteristics & 0x20000000 )
				continue;

			auto section_base = kernel::m_ntoskrnl_base + section[ section_index ].m_virtual_address;
			auto section_size = static_cast< std::uint32_t >( section[ section_index ].m_virtual_size );
			if ( !section_base || section_size < sizeof( service_descriptor_table_t ) * 2 )
				continue;

			auto section_end = section_base + section_size - ( sizeof( service_descriptor_table_t ) * 2 );
			for ( auto address = section_base; address <= section_end; address += sizeof( void* ) ) {
				auto candidate = reinterpret_cast< service_descriptor_table_t* >( address );
				if ( !kernel::mm_is_address_valid( candidate ) ||
					!kernel::mm_is_address_valid( candidate + 1 ) )
					continue;

				if ( !service_descriptor_matches( candidate, normal_table ) )
					continue;

				if ( !is_valid_service_descriptor( candidate + 1 ) )
					continue;

				table_address = address;
				kernel::dbg_print( obf( "[ssdt] shadow table found -> 0x%llx\n" ), table_address );
				return table_address;
			}
		}

		kernel::dbg_print( obf( "[ssdt] shadow table scan failed\n" ) );
		return 0;
	}

	std::uint64_t get_ssdt_handler( std::uint32_t syscall_index ) {
		static const auto s_ke_sdt = pdb::get_symbol( obf("KeServiceDescriptorTable") );
		auto table_address = s_ke_sdt;
		auto index = syscall_index & 0xFFF;
		const bool is_win32k_syscall = ( syscall_index & 0x1000 ) != 0;

		if ( is_win32k_syscall ) {
			table_address = kernel::get_export(
				kernel::m_ntoskrnl_base,
				obf( "KeServiceDescriptorTableShadow" ) );
			if ( !table_address ) {
				kernel::dbg_print( obf( "[ssdt] shadow table export missing\n" ) );
				table_address = find_service_descriptor_table_shadow( );
				if ( !table_address )
					return 0;
			}
		}

		if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( table_address ) ) ) {
			kernel::dbg_print( obf( "[ssdt] service descriptor table invalid\n" ) );
			return 0;
		}

		auto ssdt = reinterpret_cast< service_descriptor_table_t* >( table_address );
		if ( is_win32k_syscall )
			ssdt = ssdt + 1;

		if ( !is_valid_service_descriptor( ssdt ) ) {
			kernel::dbg_print( obf( "[ssdt] ssdt base invalid\n" ) );
			return 0;
		}

		if ( index >= ssdt->m_number_of_services ) {
			kernel::dbg_print( obf( "[ssdt] index 0x%x out of range (limit 0x%llx)\n" ), index, ssdt->m_number_of_services );
			return 0;
		}

		auto service_table = reinterpret_cast< std::int32_t* >( ssdt->m_service_table );
		auto encoded = service_table[ index ];
		auto disp = static_cast< std::int32_t >( encoded ) >> 4;
		auto handler = reinterpret_cast< std::uint64_t >( service_table )
			+ static_cast< std::int64_t >( disp );

		if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( handler ) ) ) {
			kernel::dbg_print( obf( "[ssdt] resolved handler 0x%llx invalid\n" ), handler );
			return 0;
		}

		return handler;
	}
}