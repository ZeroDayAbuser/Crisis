#pragma once
namespace image {
	class c_image {
	public:
		c_image( std::wstring path ): m_path( path ) {
			std::ifstream image( m_path, std::ios::binary | std::ios::ate );
			if ( !image.is_open( ) )
				return;

			const auto image_size = static_cast< std::size_t >( image.tellg( ) );
			m_image_bytes = std::make_unique<std::uint8_t[ ]>( image_size );
			m_image_size = image_size;

			image.seekg( 0, std::ios::beg );
			if ( !image.read( reinterpret_cast< char* >( m_image_bytes.get( ) ), image_size ) )
				return;

			this->update_headers( );
		}

		bool is_valid( ) const {
			return m_image_bytes != nullptr;
		}

		std::uint64_t get_entry_point( ) {
			auto entry_rva = m_nt_headers->m_address_of_entry_point;
			for ( auto idx = 0ull; idx < get_section_count( ); idx++ ) {
				auto section = get_section( idx );
				if ( entry_rva >= section->m_virtual_address &&
					entry_rva < ( section->m_virtual_address + section->m_virtual_size ) )
					break;
			}

			return entry_rva;
		}

		template <typename type>
		type rva_to_va( std::uint64_t rva ) {
			if ( rva < static_cast< std::uint64_t >( m_nt_headers->m_size_of_headers ) )
				return reinterpret_cast< type >( m_image_bytes.get( ) + rva );

			auto first_section = m_section_header;
			for ( auto section = first_section; section < first_section + m_nt_headers->m_number_of_sections; section++ ) {
				const std::uint32_t va = static_cast< std::uint32_t >( section->m_virtual_address );
				const std::uint32_t virtual_size = static_cast< std::uint32_t >( section->m_virtual_size );
				if ( rva >= va && rva < static_cast< std::uint64_t >( va ) + virtual_size ) {
					const std::uint32_t raw = static_cast< std::uint32_t >( section->m_pointer_to_raw_data );
					const std::uint32_t raw_size = static_cast< std::uint32_t >( section->m_size_of_raw_data );
					const std::uint32_t delta = static_cast< std::uint32_t >( rva - va );
					if ( delta < raw_size )
						return reinterpret_cast< type >( m_image_bytes.get( ) + raw + delta );
					return reinterpret_cast< type >( nullptr );
				}
			}
			return reinterpret_cast< type >( nullptr );
		}

		std::shared_ptr< section_header_t > get_section( std::uint32_t idx ) {
			return std::make_shared< section_header_t >( m_section_header[ idx ] );
		}

		section_header_t* find_section( const char* name ) const {
			auto sec = m_section_header;
			for ( auto i = 0; i < m_nt_headers->m_number_of_sections; ++i, ++sec )
				if ( !strncmp( sec->m_name, name, 8 ) )
					return sec;
			return nullptr;
		}

		bool add_section( const char* name, std::uint32_t size, std::uint32_t characteristics = 0xC0000040 ) {
			auto& count  = m_nt_headers->m_number_of_sections;
			auto  last   = m_section_header + count - 1;
			auto  header = m_section_header + count;

			auto align_up = []( std::uint32_t value, std::uint32_t align ) {
				return ( value + align - 1 ) & ~( align - 1 );
			};

			std::memset( header, 0, sizeof( section_header_t ) );
			strncpy_s( header->m_name, sizeof( header->m_name ), name, _TRUNCATE );

			header->m_virtual_size    = size;
			header->m_virtual_address = align_up(
				last->m_virtual_address + last->m_virtual_size,
				m_nt_headers->m_section_alignment );
			header->m_characteristics = characteristics;

			m_nt_headers->m_size_of_image = align_up(
				header->m_virtual_address + header->m_virtual_size,
				m_nt_headers->m_section_alignment );

			count++;
			return true;
		}

		std::uint64_t get_image_base( ) {
			return m_nt_headers->m_image_base;
		}

		std::uint32_t size_of_image( ) {
			return m_nt_headers->m_size_of_image;
		}

		std::uint32_t get_header_size( ) {
			return m_nt_headers->m_size_of_headers;
		}

		bool is_reloc_stripped( ) const {
			return ( m_nt_headers->m_characteristics & IMAGE_FILE_RELOCS_STRIPPED );
		}

		auto get_reloc_directory( ) const {
			return &m_nt_headers->m_base_relocation_table;
		}

		auto get_import_table( ) const {
			return &m_nt_headers->m_import_table;
		}

		auto get_delay_import_descriptor( ) const {
			return &m_nt_headers->m_delay_import_descriptor;
		}

		std::uint32_t get_section_count( ) {
			return m_nt_headers->m_number_of_sections;
		}

		std::int32_t get_time_date_stamp( ) const {
			return m_nt_headers->m_time_date_stamp;
		}

		std::wstring get_file_name( ) const {
			if ( m_path.empty( ) )
				return { };

			const auto last_sep = m_path.find_last_of( obf( L"\\/" ) );
			const auto name = last_sep == std::string::npos ? m_path : m_path.substr( last_sep + 1 );
			return name;
		}

		std::wstring get_file_path( ) const {
			if ( m_path.empty( ) )
				return { };
			return m_path;
		}

		std::wstring m_path { };
		std::unique_ptr<std::uint8_t[ ]> m_image_bytes { };
		std::size_t m_image_size { };
		dos_header_t* m_dos_header { };
		nt_headers_t* m_nt_headers { };
		section_header_t* m_section_header { };

	private:
		bool update_headers( ) {
			this->m_dos_header = reinterpret_cast< dos_header_t* >( m_image_bytes.get( ) );
			if ( !m_dos_header->is_valid( ) ) {
				m_image_bytes.release( );
				m_image_bytes = nullptr;
				return false;
			}

			this->m_nt_headers = reinterpret_cast< nt_headers_t* >( m_image_bytes.get( ) + m_dos_header->m_lfanew );
			if ( !m_nt_headers->is_valid( ) ) {
				m_image_bytes.release( );
				m_image_bytes = nullptr;
				return false;
			}

			this->m_section_header = reinterpret_cast< section_header_t* >(
				reinterpret_cast< std::uint8_t* >( this->m_nt_headers ) + offsetof( nt_headers_t, m_magic ) + m_nt_headers->m_size_of_optional_header
				);

			if ( !m_section_header ) {
				m_image_bytes.release( );
				m_image_bytes = nullptr;
				return false;
			}

			return true;
		}
	};
}