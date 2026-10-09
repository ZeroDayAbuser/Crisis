#pragma once

namespace components {
	namespace boot {
		__int64( __fastcall* se_secure_boot_query_information_org )( int, void*, unsigned int, unsigned int* );
		__int64 __fastcall se_secure_boot_query_information( int info_class, void* buffer, unsigned int buffer_size, unsigned int* return_length ) {
			if ( !image::contains( _ReturnAddress( ) ) )
				return se_secure_boot_query_information_org( info_class, buffer, buffer_size, return_length );

			switch ( info_class ) {
			case 145: {
				if ( return_length )
					*return_length = 2;

				if ( buffer_size >= 2 && buffer && kernel::mm_is_address_valid( buffer ) ) {
					*reinterpret_cast < unsigned char* > ( buffer ) = 1;
					*reinterpret_cast < unsigned char* > ( reinterpret_cast < std::uint64_t >( buffer ) + 1 ) = 1;
					return 0;
				}

				return 0xC0000004;
			} break;
			case 143: {
				auto result = se_secure_boot_query_information_org( info_class, buffer, buffer_size, return_length );
				if ( result && buffer && buffer_size >= 24 && kernel::mm_is_address_valid( buffer ) ) {
					static unsigned char policy_code[ 24 ] {
						0xBD, 0x9A, 0xFA, 0x77, 0x59, 0x03, 0x32, 0x4D,
						0xBD, 0x60, 0x28, 0xF4, 0xE7, 0x8F, 0x78, 0x4B,
						0x01, 0x00, 0x00, 0x00,
						0x00, 0x00, 0x00, 0x00
					};

					for ( auto idx = 0; idx < 24; idx++ )
						reinterpret_cast< unsigned char* >( buffer )[ idx ] = policy_code[ idx ];

					if ( return_length )
						*return_length = 24;
					return 0;
				}

				return result;
			} break;
			}

			return se_secure_boot_query_information_org( info_class, buffer, buffer_size, return_length );
		}

		bool spoof( ) {
			static const auto s_fn = pdb::get_symbol( obf("SeSecureBootQueryInformation") );
			auto boot_state = paging::pph::create_hook( s_fn, &se_secure_boot_query_information_org );
			if ( !boot_state )
				return false;
			if ( !paging::pph::enable_hook( boot_state, se_secure_boot_query_information ) )
				return false;
			return true;
		}
	}
}
