#pragma once

namespace cache {
	bool clear_piddb_cache_table( const wchar_t* driver_name, std::uint32_t timestamp );
	bool clear_kernel_hash_bucket_list( const wchar_t* driver_name, const wchar_t* driver_path );
	bool clear_mm_unloaded_drivers( const wchar_t* driver_name );
	bool clear_driver_image( uint32_t timestamp );
	bool clear_wd_filter_driver_list( );
}

namespace service {
	class c_service {
	public:
		bool create( const std::vector< std::uint8_t >& driver_image ) {
			m_driver_timestamp = get_timestamp( driver_image );
			if ( !m_driver_timestamp ) {
				logging::print( obf( "invalid driver image.\n" ) );
				return false;
			}
			if ( !utility::toggle_privilege( SE_LOAD_DRIVER_PRIVILEGE, true ) ) {
				logging::print( obf( "failed to enable driver loading privilege.\n" ) );
				return false;
			}

			const wchar_t* prefix = obf( L"\\??\\" );
			std::memset( m_driver_path, 0, sizeof( m_driver_path ) );
			std::wcscat( m_driver_path, prefix );
			auto file_path = m_driver_path + std::wcslen( prefix );

			GetTempPathW( MAX_PATH, file_path );
			utility::gen_rnd_str( m_driver_name );
			std::wcscat( file_path, m_driver_name );
			std::wcscat( file_path, obf( L".log" ) );

			if ( !install_service( m_driver_name, driver_image.data( ), driver_image.size( ) ) ) {
				utility::toggle_privilege( SE_LOAD_DRIVER_PRIVILEGE, false );
				return false;
			}

			return true;
		}

		bool cleanup( ) {
			if ( !cache::clear_piddb_cache_table( m_driver_name, get_timestamp( ) ) )
				return false;

			if ( !cache::clear_kernel_hash_bucket_list( m_driver_name, m_driver_path ) )
				return false;

			if ( !cache::clear_wd_filter_driver_list( ) )
				return false;

			printf( obf( "\n" ) );
			return true;
		}

		bool clean_unloaded_driver( ) {
			return cache::clear_mm_unloaded_drivers( m_driver_name );
		}

		bool clean_driver_image( ) {
			return cache::clear_driver_image( get_timestamp( ) );
		}

		bool unload( ) {
			uninstall_services( );
			utility::toggle_privilege( SE_LOAD_DRIVER_PRIVILEGE, false );
			return true;
		}

	private:
		wchar_t m_driver_name[ 20 ] { };
		wchar_t m_driver_path[ MAX_PATH ] { };

		std::uint32_t m_driver_timestamp { };

		std::uint32_t get_timestamp( const std::vector< std::uint8_t >& driver_image ) const {
			if ( driver_image.size( ) < sizeof( IMAGE_DOS_HEADER ) )
				return 0;
			auto dos_header = reinterpret_cast< const IMAGE_DOS_HEADER* >( driver_image.data( ) );
			if ( dos_header->e_magic != IMAGE_DOS_SIGNATURE )
				return 0;

			if ( dos_header->e_lfanew < 0 || static_cast< std::size_t >( dos_header->e_lfanew ) > driver_image.size( ) - sizeof( IMAGE_NT_HEADERS64 ) )
				return 0;
			auto nt_headers = reinterpret_cast< const IMAGE_NT_HEADERS64* >( driver_image.data( ) + dos_header->e_lfanew );
			if ( nt_headers->Signature != IMAGE_NT_SIGNATURE )
				return 0;

			return nt_headers->FileHeader.TimeDateStamp;
		}

		std::uint32_t get_timestamp( ) const { return m_driver_timestamp; }
	};
}