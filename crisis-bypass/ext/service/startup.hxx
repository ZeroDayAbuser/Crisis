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
		bool create( ) {
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

			if ( !install_service( m_driver_name ) ) {
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

			memset( driver_bytes, 0, sizeof( driver_bytes ) );
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
		wchar_t m_driver_name[ 20 ]{ };
		wchar_t m_driver_path[ MAX_PATH ]{ };

		std::uint32_t get_timestamp( ) {
			auto dos_header = reinterpret_cast< PIMAGE_DOS_HEADER >( driver_bytes );
			if ( dos_header->e_magic != IMAGE_DOS_SIGNATURE )
				return 0;

			auto nt_headers = reinterpret_cast< PIMAGE_NT_HEADERS64 >( reinterpret_cast< std::uint64_t >( driver_bytes ) + dos_header->e_lfanew );
			if ( nt_headers->Signature != IMAGE_NT_SIGNATURE )
				return 0;

			return nt_headers->FileHeader.TimeDateStamp;
		}
	};
}