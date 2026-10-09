#pragma once

namespace exception {
	struct info_t {
		std::uint64_t  m_base {};
		std::uint64_t  m_size {};
		const wchar_t* m_name {};
		std::uint16_t  m_name_length {};
		bool           m_is_ntoskrnl {};
	};

	info_t find( std::uint64_t address ) {
		info_t info {};

		const auto head = reinterpret_cast< list_entry_t* >(
			pdb::get_symbol( obf( "PsLoadedModuleList" ) ) );
		if ( !head )
			return info;

		auto* link = head->m_flink;
		while ( link && link != head ) {
			auto* entry = reinterpret_cast< kldr_data_table_entry_t* >( link );
			const auto base = reinterpret_cast< std::uint64_t >( entry->m_dll_base );

			if ( base && address >= base && address < base + entry->m_size_of_image ) {
				info.m_base = base;
				info.m_size = entry->m_size_of_image;
				info.m_name = entry->m_base_dll_name.m_buffer;
				info.m_name_length = static_cast< std::uint16_t >(
					entry->m_base_dll_name.m_length / sizeof( wchar_t ) );
				info.m_is_ntoskrnl = address >= kernel::m_ntoskrnl_base &&
					address < kernel::m_ntoskrnl_base + kernel::m_ntoskrnl_size;
				return info;
			}

			link = link->m_flink;
		}

		return info;
	}

	void format( std::uint64_t address, char* out, std::size_t out_size ) {
		const auto info = find( address );

		if ( !info.m_base ) {
			_snprintf( out, out_size, "0x%llx", address );
			return;
		}

		wchar_t name[ 64 ] {};
		const auto count = info.m_name_length < 63 ? info.m_name_length : 63;
		if ( info.m_name )
			for ( std::size_t i = 0; i < count; ++i )
				name[ i ] = info.m_name[ i ];

		if ( info.m_is_ntoskrnl ) {
			std::uint64_t symbol_value = 0;
			std::uint8_t  symbol_length = 0;
			const auto* symbol = pdb::get_symbol_name_by_address(
				address, &symbol_value, &symbol_length );

			if ( symbol && symbol_length ) {
				char text[ 128 ] {};
				const auto length = symbol_length < 127 ? symbol_length : 127;
				for ( std::size_t i = 0; i < length; ++i )
					text[ i ] = symbol[ i ];

				_snprintf( out, out_size, "%ws!%s+0x%llx",
					name, text, address - symbol_value );
				return;
			}
		}

		_snprintf( out, out_size, "%ws+0x%llx", name, address - info.m_base );
	}
}
