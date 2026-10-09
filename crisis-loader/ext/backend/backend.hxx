#pragma once

#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ncrypt.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "bcrypt.lib")

namespace auth {
	struct response_t {
		bool success = false;
		std::uint64_t start_unix = 0;
		std::uint64_t expiry_unix = 0;
		std::uint64_t access_token_expires_unix = 0;
		std::uint64_t player_token_expires_unix = 0;
		std::string access_token;
		std::string player_token;
		std::vector< std::string > games;
		std::string pairing_code;
		std::string message;
	};

	namespace pe {
		struct manifest_entry_t {
			std::string name;
			std::uint32_t rva = 0;
			std::uint16_t ordinal = 0;
		};

		struct manifest_import_t {
			std::string dll;
			std::vector< manifest_entry_t > entries;
		};

		struct manifest_section_t {
			std::string name;
			std::uint32_t rva = 0;
			std::uint32_t size = 0;
			std::uint32_t characteristics = 0;
		};

		struct manifest_export_t {
			std::string name;
			std::uint32_t rva = 0;
			std::uint16_t ordinal = 0;
		};

		struct manifest_t {
			std::uint64_t image_base = 0;
			std::uint32_t size_of_image = 0;
			std::uint32_t entry_point = 0;
			std::uint32_t reloc_rva = 0;
			std::uint32_t reloc_size = 0;
			bool has_relocations = false;
			std::vector< manifest_section_t > sections;
			std::vector< manifest_import_t > imports;
			std::vector< std::uint32_t > tls_callbacks;
			std::vector< manifest_export_t > exports;
		};

		struct mapped_module_t {
			bool ok = false;
			std::uint8_t* base = nullptr;
			HMODULE* import_modules = nullptr;
			std::size_t import_module_count = 0;
			manifest_t manifest;
		};
	}

	namespace detail {
		class device_key_t {
		public:
			device_key_t( ) = default;
			~device_key_t( ) {
				if ( m_key ) NCryptFreeObject( m_key );
				if ( m_provider ) NCryptFreeObject( m_provider );
			}

			device_key_t( const device_key_t& ) = delete;
			device_key_t& operator=( const device_key_t& ) = delete;

			bool initialize( ) {
				if ( m_key ) return true;
				constexpr const wchar_t* key_name = L"CrisisLoaderDeviceKeyV1";
				NCRYPT_PROV_HANDLE provider = 0;
				if ( NCryptOpenStorageProvider( &provider, MS_KEY_STORAGE_PROVIDER, 0 ) != ERROR_SUCCESS ) return false;

				NCRYPT_KEY_HANDLE key = 0;
				auto status = NCryptOpenKey( provider, &key, key_name, 0, 0 );
				if ( status != ERROR_SUCCESS ) {
					status = NCryptCreatePersistedKey( provider, &key, BCRYPT_ECDSA_P256_ALGORITHM, key_name, 0, 0 );
					if ( status == ERROR_SUCCESS ) status = NCryptFinalizeKey( key, 0 );
				}
				if ( status != ERROR_SUCCESS ) {
					if ( key ) NCryptFreeObject( key );
					NCryptFreeObject( provider );
					return false;
				}

				m_provider = provider;
				m_key = key;
				if ( export_public_key( ) ) return true;
				NCryptFreeObject( m_key );
				NCryptFreeObject( m_provider );
				m_key = 0;
				m_provider = 0;
				return false;
			}

			[[nodiscard]] const std::vector< std::uint8_t >& public_blob( ) const { return m_public_blob; }

			bool sign( const std::string& message, std::vector< std::uint8_t >& signature ) const {
				std::array< std::uint8_t, 32 > hash{};
				if ( !sha256( message, hash ) ) return false;
				DWORD required = 0;
				if ( NCryptSignHash( m_key, nullptr, const_cast< PBYTE >( hash.data( ) ),
					static_cast< DWORD >( hash.size( ) ), nullptr, 0, &required, 0 ) != ERROR_SUCCESS ||
					required != 64 ) return false;
				signature.resize( required );
				const auto status = NCryptSignHash( m_key, nullptr, const_cast< PBYTE >( hash.data( ) ),
					static_cast< DWORD >( hash.size( ) ), signature.data( ), required, &required, 0 );
				signature.resize( status == ERROR_SUCCESS ? required : 0 );
				return status == ERROR_SUCCESS && required == 64;
			}

		private:
			static bool sha256( const std::string& message, std::array< std::uint8_t, 32 >& output ) {
				BCRYPT_ALG_HANDLE algorithm = nullptr;
				BCRYPT_HASH_HANDLE hash = nullptr;
				DWORD object_size = 0;
				DWORD copied = 0;
				if ( BCryptOpenAlgorithmProvider( &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0 ) < 0 ||
					BCryptGetProperty( algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast< PUCHAR >( &object_size ),
						sizeof( object_size ), &copied, 0 ) < 0 ) {
					if ( algorithm ) BCryptCloseAlgorithmProvider( algorithm, 0 );
					return false;
				}
				std::vector< std::uint8_t > object( object_size );
				const bool valid = BCryptCreateHash( algorithm, &hash, object.data( ), object_size, nullptr, 0, 0 ) >= 0 &&
					BCryptHashData( hash, reinterpret_cast< PUCHAR >( const_cast< char* >( message.data( ) ) ),
						static_cast< ULONG >( message.size( ) ), 0 ) >= 0 &&
					BCryptFinishHash( hash, output.data( ), static_cast< ULONG >( output.size( ) ), 0 ) >= 0;
				if ( hash ) BCryptDestroyHash( hash );
				BCryptCloseAlgorithmProvider( algorithm, 0 );
				SecureZeroMemory( object.data( ), object.size( ) );
				return valid;
			}

			bool export_public_key( ) {
				DWORD required = 0;
				if ( NCryptExportKey( m_key, 0, BCRYPT_ECCPUBLIC_BLOB, nullptr, nullptr, 0, &required, 0 ) != ERROR_SUCCESS ||
					required != 72 ) return false;
				m_public_blob.resize( required );
				const auto status = NCryptExportKey( m_key, 0, BCRYPT_ECCPUBLIC_BLOB, nullptr,
					m_public_blob.data( ), required, &required, 0 );
				m_public_blob.resize( status == ERROR_SUCCESS ? required : 0 );
				return status == ERROR_SUCCESS && required == 72;
			}

			NCRYPT_PROV_HANDLE m_provider = 0;
			NCRYPT_KEY_HANDLE m_key = 0;
			std::vector< std::uint8_t > m_public_blob;
		};

		inline device_key_t& device_key( ) {
			static device_key_t key;
			return key;
		}

		inline std::string base64url_encode( const std::uint8_t* data, const std::size_t size ) {
			constexpr char alphabet[ ] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
			std::string output;
			output.reserve( ( size * 4 + 2 ) / 3 );
			for ( std::size_t index = 0; index < size; index += 3 ) {
				const std::uint32_t value = ( static_cast< std::uint32_t >( data[ index ] ) << 16 ) |
					( index + 1 < size ? static_cast< std::uint32_t >( data[ index + 1 ] ) << 8 : 0 ) |
					( index + 2 < size ? data[ index + 2 ] : 0 );
				output.push_back( alphabet[ ( value >> 18 ) & 63 ] );
				output.push_back( alphabet[ ( value >> 12 ) & 63 ] );
				if ( index + 1 < size ) output.push_back( alphabet[ ( value >> 6 ) & 63 ] );
				if ( index + 2 < size ) output.push_back( alphabet[ value & 63 ] );
			}
			return output;
		}

		inline bool signed_proof( device_key_t& key, const std::string& prefix, const std::string& suffix,
			std::string& nonce, std::string& signature ) {
			if ( !key.initialize( ) ) return false;
			std::array< std::uint8_t, 32 > nonce_bytes{};
			if ( BCryptGenRandom( nullptr, nonce_bytes.data( ), static_cast< ULONG >( nonce_bytes.size( ) ),
				BCRYPT_USE_SYSTEM_PREFERRED_RNG ) < 0 ) return false;
			nonce = base64url_encode( nonce_bytes.data( ), nonce_bytes.size( ) );
			std::vector< std::uint8_t > raw_signature;
			if ( !key.sign( prefix + nonce + suffix, raw_signature ) ) return false;
			signature = base64url_encode( raw_signature.data( ), raw_signature.size( ) );
			SecureZeroMemory( raw_signature.data( ), raw_signature.size( ) );
			return true;
		}

		inline void secure_clear( std::string& value ) {
			if ( !value.empty( ) ) SecureZeroMemory( value.data( ), value.size( ) );
			value.clear( );
		}

		inline void secure_clear( std::wstring& value ) {
			if ( !value.empty( ) ) SecureZeroMemory( value.data( ), value.size( ) * sizeof( wchar_t ) );
			value.clear( );
		}

		inline void secure_clear( std::vector< std::uint8_t >& value ) {
			if ( !value.empty( ) ) SecureZeroMemory( value.data( ), value.size( ) );
			value.clear( );
		}

		inline bool valid_access_token( const std::string& token ) {
			return token.size( ) == 43 && std::all_of( token.begin( ), token.end( ),
				[]( const unsigned char ch ) { return std::isalnum( ch ) != 0 || ch == '-' || ch == '_'; } );
		}
		inline bool valid_game_id( const std::string_view game ) {
			return !game.empty( ) && game.size( ) <= 32 &&
				std::all_of( game.begin( ), game.end( ), []( const unsigned char ch ) {
					return ( ch >= 'a' && ch <= 'z' ) || ( ch >= '0' && ch <= '9' ) || ch == '-';
				} );
		}

		inline std::string trim( std::string value ) {
			const auto is_space = [ ]( const unsigned char ch ) { return std::isspace( ch ) != 0; };
			value.erase( value.begin( ), std::find_if_not( value.begin( ), value.end( ), is_space ) );
			value.erase( std::find_if_not( value.rbegin( ), value.rend( ), is_space ).base( ), value.end( ) );
			return value;
		}

		inline std::string json_escape( const std::string& value ) {
			constexpr char hex[ ] = "0123456789ABCDEF";
			std::string encoded;
			encoded.reserve( value.size( ) + 8 );

			for ( const unsigned char ch : value ) {
				switch ( ch ) {
				case '"': encoded += "\\\""; break;
				case '\\': encoded += "\\\\"; break;
				case '\b': encoded += "\\b"; break;
				case '\f': encoded += "\\f"; break;
				case '\n': encoded += "\\n"; break;
				case '\r': encoded += "\\r"; break;
				case '\t': encoded += "\\t"; break;
				default:
					if ( ch < 0x20 ) {
						encoded += "\\u00";
						encoded.push_back( hex[ ch >> 4 ] );
						encoded.push_back( hex[ ch & 0x0F ] );
					}
					else {
						encoded.push_back( static_cast< char >( ch ) );
					}
				}
			}
			return encoded;
		}

		inline std::string url_encode( const std::string& value ) {
			constexpr char hex[ ] = "0123456789ABCDEF";
			std::string encoded;
			encoded.reserve( value.size( ) * 3 );

			for ( const unsigned char ch : value ) {
				if ( std::isalnum( ch ) || ch == '-' || ch == '_' || ch == '.' || ch == '~' ) {
					encoded.push_back( static_cast< char >( ch ) );
				}
				else {
					encoded.push_back( '%' );
					encoded.push_back( hex[ ch >> 4 ] );
					encoded.push_back( hex[ ch & 0x0F ] );
				}
			}
			return encoded;
		}

		inline bool parse_json_bool( const std::string& json, const std::string& key, bool& value ) {
			const std::string marker = std::string( "\"" ) + key + "\"";
			auto pos = json.find( marker );
			if ( pos == std::string::npos || ( pos = json.find( ':', pos + marker.size( ) ) ) == std::string::npos ) return false;
			pos = json.find_first_not_of( " \t\r\n", pos + 1 );
			if ( pos == std::string::npos ) return false;
			if ( json.compare( pos, 4, "true" ) == 0 ) { value = true; return true; }
			if ( json.compare( pos, 5, "false" ) == 0 ) { value = false; return true; }
			return false;
		}

		inline bool parse_json_uint64( const std::string& json, const std::string& key, std::uint64_t& value ) {
			const std::string marker = std::string( "\"" ) + key + "\"";
			auto pos = json.find( marker );
			if ( pos == std::string::npos || ( pos = json.find( ':', pos + marker.size( ) ) ) == std::string::npos ) return false;
			pos = json.find_first_not_of( " \t\r\n", pos + 1 );
			if ( pos == std::string::npos ) return false;
			if ( json[ pos ] == '"' ) ++pos;

			const auto end = json.find_first_not_of( "0123456789", pos );
			if ( end == pos ) return false;
			try {
				value = std::stoull( json.substr( pos, end - pos ) );
				return true;
			}
			catch ( ... ) {
				return false;
			}
		}

		inline bool parse_json_string( const std::string& json, const std::string& key, std::string& value ) {
			const std::string marker = std::string( "\"" ) + key + "\"";
			auto pos = json.find( marker );
			if ( pos == std::string::npos || ( pos = json.find( ':', pos + marker.size( ) ) ) == std::string::npos ) return false;
			pos = json.find_first_not_of( " \t\r\n", pos + 1 );
			if ( pos == std::string::npos || json[ pos ] != '"' ) return false;

			value.clear( );
			bool escaped = false;
			for ( ++pos; pos < json.size( ); ++pos ) {
				const char ch = json[ pos ];
				if ( escaped ) {
					switch ( ch ) {
					case '"': case '\\': case '/': value.push_back( ch ); break;
					case 'b': value.push_back( '\b' ); break;
					case 'f': value.push_back( '\f' ); break;
					case 'n': value.push_back( '\n' ); break;
					case 'r': value.push_back( '\r' ); break;
					case 't': value.push_back( '\t' ); break;
					default: value.push_back( '?' ); break;
					}
					escaped = false;
				}
				else if ( ch == '\\' ) {
					escaped = true;
				}
				else if ( ch == '"' ) {
					return true;
				}
				else {
					value.push_back( ch );
				}
			}
			return false;
		}
		inline bool parse_json_string_array( const std::string& json, const std::string& key, std::vector< std::string >& values ) {
			const std::string marker = std::string( "\"" ) + key + "\"";
			auto pos = json.find( marker );
			if ( pos == std::string::npos || ( pos = json.find( ':', pos + marker.size( ) ) ) == std::string::npos ) return false;
			pos = json.find_first_not_of( " \t\r\n", pos + 1 );
			if ( pos == std::string::npos || json[ pos ] != '[' ) return false;

			values.clear( );
			pos = json.find_first_not_of( " \t\r\n", pos + 1 );
			if ( pos == std::string::npos ) return false;
			if ( json[ pos ] == ']' ) return true;

			while ( pos < json.size( ) ) {
				if ( json[ pos ] != '"' ) return false;
				std::string value;
				bool escaped = false;
				bool terminated = false;
				for ( ++pos; pos < json.size( ); ++pos ) {
					const unsigned char ch = static_cast< unsigned char >( json[ pos ] );
					if ( escaped ) {
						switch ( ch ) {
						case '"': case '\\': case '/': value.push_back( static_cast< char >( ch ) ); break;
						case 'b': value.push_back( '\b' ); break;
						case 'f': value.push_back( '\f' ); break;
						case 'n': value.push_back( '\n' ); break;
						case 'r': value.push_back( '\r' ); break;
						case 't': value.push_back( '\t' ); break;
						default: return false;
						}
						escaped = false;
					}
					else if ( ch == '\\' ) {
						escaped = true;
					}
					else if ( ch == '"' ) {
						++pos;
						terminated = true;
						break;
					}
					else {
						if ( ch < 0x20 ) return false;
						value.push_back( static_cast< char >( ch ) );
					}
				}
				if ( !terminated || escaped ) return false;
				values.push_back( std::move( value ) );

				pos = json.find_first_not_of( " \t\r\n", pos );
				if ( pos == std::string::npos ) return false;
				if ( json[ pos ] == ']' ) return true;
				if ( json[ pos ] != ',' ) return false;
				pos = json.find_first_not_of( " \t\r\n", pos + 1 );
				if ( pos == std::string::npos ) return false;
			}
			return false;
		}

		inline bool winsock_ready( ) {
			static const bool ready = [ ]( ) {
				WSADATA data{};
				return WSAStartup( MAKEWORD( 2, 2 ), &data ) == 0;
			}( );
			return ready;
		}

		inline bool set_socket_timeouts( const SOCKET socket, const DWORD milliseconds ) {
			return setsockopt( socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast< const char* >( &milliseconds ), sizeof( milliseconds ) ) == 0 &&
				setsockopt( socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast< const char* >( &milliseconds ), sizeof( milliseconds ) ) == 0;
		}

		inline bool raw_send_all( const SOCKET socket, const std::uint8_t* data, std::size_t size ) {
			while ( size ) {
				const int sent = send( socket, reinterpret_cast< const char* >( data ),
					static_cast< int >( ( std::min )( size, static_cast< std::size_t >( 64 * 1024 ) ) ), 0 );
				if ( sent <= 0 ) return false;
				data += sent;
				size -= static_cast< std::size_t >( sent );
			}
			return true;
		}

		inline bool raw_recv_exact( const SOCKET socket, std::uint8_t* data, std::size_t size ) {
			while ( size ) {
				const int received = recv( socket, reinterpret_cast< char* >( data ),
					static_cast< int >( ( std::min )( size, static_cast< std::size_t >( 64 * 1024 ) ) ), 0 );
				if ( received <= 0 ) return false;
				data += received;
				size -= static_cast< std::size_t >( received );
			}
			return true;
		}

		class native_channel_t {
		public:
			native_channel_t( ) = default;
			~native_channel_t( ) { close( ); }
			native_channel_t( const native_channel_t& ) = delete;
			native_channel_t& operator=( const native_channel_t& ) = delete;

			bool connect( const std::string& host, const std::uint16_t port, std::string& error ) {
				close( );
				if ( !winsock_ready( ) ) { error = "failed to initialize sockets."; return false; }

				ADDRINFOA hints{};
				hints.ai_family = AF_UNSPEC;
				hints.ai_socktype = SOCK_STREAM;
				hints.ai_protocol = IPPROTO_TCP;
				PADDRINFOA addresses = nullptr;
				if ( getaddrinfo( host.c_str( ), std::to_string( port ).c_str( ), &hints, &addresses ) != 0 || !addresses ) {
					error = "failed to resolve authentication server.";
					return false;
				}

				for ( auto* entry = addresses; entry; entry = entry->ai_next ) {
					const SOCKET socket = ::socket( entry->ai_family, entry->ai_socktype, entry->ai_protocol );
					if ( socket == INVALID_SOCKET ) continue;
					if ( !set_socket_timeouts( socket, 15000 ) ) { closesocket( socket ); continue; }
					if ( ::connect( socket, entry->ai_addr, static_cast< int >( entry->ai_addrlen ) ) == 0 ) {
						m_socket = socket;
						break;
					}
					closesocket( socket );
				}
				freeaddrinfo( addresses );
				if ( m_socket == INVALID_SOCKET ) {
					error = "failed to connect to authentication server.";
					return false;
				}
				return true;
			}

			bool send_all( const std::uint8_t* data, const std::size_t size ) {
				return m_socket != INVALID_SOCKET && raw_send_all( m_socket, data, size );
			}

			bool recv_exact( std::uint8_t* data, const std::size_t size ) {
				return m_socket != INVALID_SOCKET && raw_recv_exact( m_socket, data, size );
			}

			void close( ) {
				if ( m_socket != INVALID_SOCKET ) {
					closesocket( m_socket );
					m_socket = INVALID_SOCKET;
				}
			}

		private:
			SOCKET m_socket = INVALID_SOCKET;
		};

		inline constexpr std::uint32_t native_magic = 0x54554143;
		inline constexpr std::uint8_t native_version = 1;
		inline constexpr std::uint8_t native_flag_binary = 0x01;
		inline constexpr std::size_t native_header_size = 16;
		inline constexpr std::size_t native_max_json = 64 * 1024;
		inline constexpr std::size_t native_max_binary = 64ull * 1024 * 1024;

		inline bool native_write_u32( std::uint8_t* dest, const std::uint32_t value ) {
			dest[ 0 ] = static_cast< std::uint8_t >( value );
			dest[ 1 ] = static_cast< std::uint8_t >( value >> 8 );
			dest[ 2 ] = static_cast< std::uint8_t >( value >> 16 );
			dest[ 3 ] = static_cast< std::uint8_t >( value >> 24 );
			return true;
		}

		inline std::uint32_t native_read_u32( const std::uint8_t* src ) {
			return static_cast< std::uint32_t >( src[ 0 ] ) |
				( static_cast< std::uint32_t >( src[ 1 ] ) << 8 ) |
				( static_cast< std::uint32_t >( src[ 2 ] ) << 16 ) |
				( static_cast< std::uint32_t >( src[ 3 ] ) << 24 );
		}

		inline bool native_exchange( const std::string& host, const std::uint16_t port, const std::string& json,
			std::string& response_json, std::vector< std::uint8_t >& response_binary, std::string& error,
			const std::size_t max_binary = native_max_binary ) {
			secure_clear( response_json );
			secure_clear( response_binary );
			if ( json.empty( ) || json.size( ) > 32 * 1024 ) {
				error = "authentication request is invalid.";
				return false;
			}

			native_channel_t channel;
			if ( !channel.connect( host, port, error ) ) return false;

			std::array< std::uint8_t, native_header_size > header{};
			native_write_u32( header.data( ), native_magic );
			header[ 4 ] = native_version;
			header[ 5 ] = 0;
			header[ 6 ] = 0;
			header[ 7 ] = 0;
			native_write_u32( header.data( ) + 8, static_cast< std::uint32_t >( json.size( ) ) );
			native_write_u32( header.data( ) + 12, 0 );
			if ( !channel.send_all( header.data( ), header.size( ) ) ||
				!channel.send_all( reinterpret_cast< const std::uint8_t* >( json.data( ) ), json.size( ) ) ) {
				error = "authentication server request failed.";
				return false;
			}

			if ( !channel.recv_exact( header.data( ), header.size( ) ) ) {
				error = "failed to read authentication response.";
				return false;
			}
			if ( native_read_u32( header.data( ) ) != native_magic || header[ 4 ] != native_version ||
				header[ 6 ] != 0 || header[ 7 ] != 0 ) {
				error = "authentication server returned an invalid response.";
				return false;
			}
			const auto flags = header[ 5 ];
			const auto json_len = native_read_u32( header.data( ) + 8 );
			const auto binary_len = native_read_u32( header.data( ) + 12 );
			if ( !json_len || json_len > native_max_json || binary_len > max_binary ||
				( binary_len && !( flags & native_flag_binary ) ) ||
				( !binary_len && ( flags & native_flag_binary ) ) ) {
				error = "authentication response was too large.";
				return false;
			}

			response_json.resize( json_len );
			if ( !channel.recv_exact( reinterpret_cast< std::uint8_t* >( response_json.data( ) ), json_len ) ) {
				secure_clear( response_json );
				error = "failed to read authentication response.";
				return false;
			}
			if ( binary_len ) {
				response_binary.resize( binary_len );
				if ( !channel.recv_exact( response_binary.data( ), binary_len ) ) {
					secure_clear( response_json );
					secure_clear( response_binary );
					error = "failed to read authentication response.";
					return false;
				}
			}
			return true;
		}

		inline void derive_xor_key( const std::string& access_token, const std::string& hwid, std::vector< std::uint8_t >& out_key ) {
			std::int32_t hash = 0;
			const auto absorb = [ & ]( const std::string& value ) {
				for ( const unsigned char ch : value ) {
					const std::int64_t temp = ( static_cast< std::int64_t >( hash ) << 5 ) - hash + ch;
					hash = static_cast< std::int32_t >( temp & 0xFFFFFFFF );
				}
			};
			absorb( access_token );
			absorb( hwid );

			out_key.resize( 256 );
			std::uint32_t uhash = static_cast< std::uint32_t >( hash );
			for ( int i = 0; i < 256; ++i ) {
				out_key[ static_cast< std::size_t >( i ) ] = static_cast< std::uint8_t >( ( uhash ^ ( static_cast< std::uint32_t >( i ) * 0x9E ) ) & 0xFF );
				uhash = ( uhash * 1103515245u + 12345u ) & 0xFFFFFFFFu;
			}
		}

		inline void xor_decrypt( std::vector< std::uint8_t >& buffer, const std::vector< std::uint8_t >& xor_key ) {
			if ( xor_key.empty( ) ) return;
			for ( std::size_t i = 0; i < buffer.size( ); ++i ) {
				buffer[ i ] ^= xor_key[ i % xor_key.size( ) ];
			}
		}
	}

	inline std::string generate_hwid( ) {
		DWORD serial = 0;
		if ( !GetVolumeInformationW( L"C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0 ) ) return { };

		std::ostringstream stream;
		stream << std::uppercase << std::hex << std::setfill( '0' ) << std::setw( 8 ) << serial;
		return stream.str( );
	}

	inline bool prompt_key( std::string& key ) {
		logging::print( "your license key will be saved after logging in." );
		logging::print_line( "enter your license key: " );
		std::fflush( stdout );
		const HANDLE input = GetStdHandle( STD_INPUT_HANDLE );
		DWORD old_mode = 0;
		const bool console = input != INVALID_HANDLE_VALUE && GetConsoleMode( input, &old_mode );
		bool read = false;
		if ( console ) {
			SetConsoleMode( input, old_mode & ~( ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT ) );
			for ( ;; ) {
				INPUT_RECORD record{};
				DWORD count = 0;
				if ( !ReadConsoleInputA( input, &record, 1, &count ) || !count )
					break;
				if ( record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown )
					continue;

				const auto& event = record.Event.KeyEvent;
				if ( event.wVirtualKeyCode == VK_RETURN ) {
					read = true;
					break;
				}
				if ( event.wVirtualKeyCode == VK_BACK ) {
					if ( !key.empty( ) ) {
						key.pop_back( );
						std::fputs( "\b \b", stdout );
						std::fflush( stdout );
					}
					continue;
				}
				if ( event.wVirtualKeyCode == 'C' && ( event.dwControlKeyState & ( LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED ) ) )
					break;

				const auto ch = event.uChar.AsciiChar;
				if ( ch >= 0x20 && ch != 0x7f ) {
					key.append( event.wRepeatCount, ch );
					for ( WORD i = 0; i < event.wRepeatCount; ++i )
						std::fputc( '*', stdout );
					std::fflush( stdout );
				}
			}
			SetConsoleMode( input, old_mode );
			std::fputc( '\n', stdout );
		} else {
			read = static_cast< bool >( std::getline( std::cin, key ) );
		}
		key = detail::trim( std::move( key ) );
		return read && !key.empty( );
	}

	inline std::string server_url( ) {
		return "https://home.aaddpp.lol";
	}

	inline std::string native_host( ) {
		return "home.aaddpp.lol";
	}

	inline std::uint16_t native_port( ) {
		return 223;
	}

	inline response_t authenticate( const std::string& key, const std::string& hwid,
		const std::string& host = native_host( ), const std::uint16_t port = native_port( ) ) {
		response_t response;
		if ( key.empty( ) ) { response.message = "license key is required."; return response; }
		if ( hwid.empty( ) ) { response.message = "failed to generate the hardware identifier."; return response; }

		auto& device_key = detail::device_key( );
		const auto timestamp = static_cast< std::uint64_t >( std::chrono::duration_cast< std::chrono::seconds >(
			std::chrono::system_clock::now( ).time_since_epoch( ) ).count( ) );
		std::string nonce;
		std::string signature;
		const std::string proof_prefix = "crisis-client-auth:v1\n" + std::to_string( timestamp ) + "\n";
		const std::string proof_suffix = "\n" + hwid + "\n" + key;
		if ( !detail::signed_proof( device_key, proof_prefix, proof_suffix, nonce, signature ) ) {
			response.message = "failed to initialize or use the device identity.";
			return response;
		}
		const std::string public_key = detail::base64url_encode(
			device_key.public_blob( ).data( ), device_key.public_blob( ).size( ) );
		std::string payload = std::string( "{\"op\":\"authenticate\",\"key\":\"" ) + detail::json_escape( key ) +
			"\",\"hwid\":\"" + detail::json_escape( hwid ) +
			"\",\"client_public_key\":\"" + public_key +
			"\",\"timestamp_unix\":" + std::to_string( timestamp ) +
			",\"nonce\":\"" + nonce + "\",\"signature\":\"" + signature + "\"}";
		detail::secure_clear( nonce );
		detail::secure_clear( signature );
		std::string json;
		std::vector< std::uint8_t > unused_binary;
		const bool authenticated = detail::native_exchange( host, port, payload, json, unused_binary, response.message, 0 );
		detail::secure_clear( payload );
		detail::secure_clear( unused_binary );
		if ( !authenticated ) return response;

		if ( !detail::parse_json_bool( json, "success", response.success ) ) {
			response.message = "authentication server returned an invalid response.";
			return response;
		}
		if ( !response.success ) {
			if ( !detail::parse_json_string( json, "error", response.message ) ) response.message = "license is invalid or expired.";
			return response;
		}
		std::string token_type;
		std::string player_token_type;
		if ( !detail::parse_json_uint64( json, "start_unix", response.start_unix ) ||
			!detail::parse_json_uint64( json, "expiry_unix", response.expiry_unix ) ||
			!detail::parse_json_uint64( json, "access_token_expires_unix", response.access_token_expires_unix ) ||
			!detail::parse_json_uint64( json, "player_token_expires_unix", response.player_token_expires_unix ) ||
			!detail::parse_json_string( json, "access_token", response.access_token ) ||
			!detail::parse_json_string( json, "player_token", response.player_token ) ||
			!detail::parse_json_string( json, "token_type", token_type ) ||
			!detail::parse_json_string( json, "player_token_type", player_token_type ) ||
			!detail::parse_json_string_array( json, "games", response.games ) ||
			response.games.empty( ) ||
			std::any_of( response.games.begin( ), response.games.end( ),
				[]( const std::string& game ) { return !detail::valid_game_id( game ); } ) ||
			response.expiry_unix < response.start_unix || token_type != "Bearer" ||
			player_token_type != "Bearer" ||
			!detail::valid_access_token( response.access_token ) ||
			!detail::valid_access_token( response.player_token ) ) {
			response.success = false;
			response.message = "authentication response is missing valid bearer credentials or game entitlements.";
			detail::secure_clear( response.access_token );
			detail::secure_clear( response.player_token );
			response.games.clear( );
			detail::secure_clear( token_type );
			detail::secure_clear( player_token_type );
			detail::secure_clear( json );
			return response;
		}
		detail::secure_clear( token_type );
		detail::secure_clear( player_token_type );

		const auto now = static_cast< std::uint64_t >( std::chrono::duration_cast< std::chrono::seconds >(
			std::chrono::system_clock::now( ).time_since_epoch( ) ).count( ) );
		if ( now >= response.expiry_unix || now >= response.access_token_expires_unix ||
			now >= response.player_token_expires_unix ||
			response.access_token_expires_unix > response.expiry_unix ||
			response.player_token_expires_unix > response.expiry_unix ) {
			response.success = false;
			response.message = "authentication credentials have expired.";
			detail::secure_clear( response.access_token );
			detail::secure_clear( response.player_token );
			detail::secure_clear( json );
			return response;
		}

		const auto remaining = response.expiry_unix - now;
		std::uint64_t amount = remaining;
		std::string unit = "second";
		if ( remaining >= 365ull * 24 * 60 * 60 ) {
			amount = remaining / ( 365ull * 24 * 60 * 60 );
			unit = "year";
		}
		else if ( remaining >= 24ull * 60 * 60 ) {
			amount = remaining / ( 24ull * 60 * 60 );
			unit = "day";
		}
		else if ( remaining >= 60ull * 60 ) {
			amount = remaining / ( 60ull * 60 );
			unit = "hour";
		}
		response.message = std::string( "subscription has " ) + std::to_string( amount ) + " " + unit;
		if ( amount != 1 )
			response.message += "s";
		response.message += " remaining.";
		detail::parse_json_string( json, "pairing_code", response.pairing_code );
		detail::secure_clear( json );
		return response;
	}

	// Server /module payload: [u32 manifest_len][manifest utf-8][mapped PE image].
	// Returns the mapped PE bytes (starts with MZ). Optionally fills manifest_text.
	inline bool unwrap_module_payload( const std::vector< std::uint8_t >& payload, std::vector< std::uint8_t >& mapped_out, std::string* manifest_text = nullptr ) {
		mapped_out.clear( );
		if ( payload.size( ) < 4 ) return false;

		std::uint32_t manifest_len = 0;
		std::memcpy( &manifest_len, payload.data( ), 4 );
		if ( manifest_len == 0 || payload.size( ) < 4ull + manifest_len ) return false;

		const auto mapped_offset = 4ull + manifest_len;
		if ( mapped_offset > payload.size( ) || payload.size( ) - mapped_offset < 2 ) return false;
		if ( payload[ static_cast< std::size_t >( mapped_offset ) ] != 'M' ||
			payload[ static_cast< std::size_t >( mapped_offset ) + 1 ] != 'Z' ) {
			return false;
		}

		if ( manifest_text ) {
			*manifest_text = std::string( reinterpret_cast< const char* >( payload.data( ) + 4 ), manifest_len );
		}
		mapped_out.assign( payload.begin( ) + static_cast< std::ptrdiff_t >( mapped_offset ), payload.end( ) );
		return !mapped_out.empty( );
	}

	inline bool mapped_image_to_raw( const std::vector< std::uint8_t >& mapped, std::vector< std::uint8_t >& raw ) {
		raw.clear( );
		if ( mapped.size( ) < sizeof( IMAGE_DOS_HEADER ) ) return false;

		const auto* dos = reinterpret_cast< const IMAGE_DOS_HEADER* >( mapped.data( ) );
		if ( dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 ) return false;

		const auto nt_offset = static_cast< std::size_t >( dos->e_lfanew );
		if ( nt_offset > mapped.size( ) || sizeof( IMAGE_NT_HEADERS64 ) > mapped.size( ) - nt_offset ) return false;
		const auto* nt = reinterpret_cast< const IMAGE_NT_HEADERS64* >( mapped.data( ) + nt_offset );
		if ( nt->Signature != IMAGE_NT_SIGNATURE ||
			nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
			nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
			nt->FileHeader.SizeOfOptionalHeader != sizeof( IMAGE_OPTIONAL_HEADER64 ) ) return false;

		const auto section_offset = nt_offset + sizeof( DWORD ) + sizeof( IMAGE_FILE_HEADER ) + nt->FileHeader.SizeOfOptionalHeader;
		const auto section_count = static_cast< std::size_t >( nt->FileHeader.NumberOfSections );
		if ( section_offset > mapped.size( ) ||
			section_count > ( mapped.size( ) - section_offset ) / sizeof( IMAGE_SECTION_HEADER ) ) return false;
		if ( !nt->OptionalHeader.SizeOfHeaders || nt->OptionalHeader.SizeOfHeaders > mapped.size( ) ) return false;

		const auto* sections = reinterpret_cast< const IMAGE_SECTION_HEADER* >( mapped.data( ) + section_offset );
		std::size_t raw_size = nt->OptionalHeader.SizeOfHeaders;
		for ( std::size_t i = 0; i < section_count; ++i ) {
			const auto source = static_cast< std::size_t >( sections[ i ].VirtualAddress );
			const auto destination = static_cast< std::size_t >( sections[ i ].PointerToRawData );
			const auto size = static_cast< std::size_t >( sections[ i ].SizeOfRawData );
			if ( source > mapped.size( ) || size > mapped.size( ) - source ) return false;
			if ( destination > SIZE_MAX - size ) return false;
			raw_size = ( std::max )( raw_size, destination + size );
		}

		raw.assign( raw_size, 0 );
		std::memcpy( raw.data( ), mapped.data( ), nt->OptionalHeader.SizeOfHeaders );
		for ( std::size_t i = 0; i < section_count; ++i ) {
			const auto source = static_cast< std::size_t >( sections[ i ].VirtualAddress );
			const auto destination = static_cast< std::size_t >( sections[ i ].PointerToRawData );
			const auto size = static_cast< std::size_t >( sections[ i ].SizeOfRawData );
			if ( size )
				std::memcpy( raw.data( ) + destination, mapped.data( ) + source, size );
		}
		return true;
	}

	inline bool download_and_decrypt_module( const std::string& access_token, const std::string& hwid, const std::string& module_name, std::vector< std::uint8_t >& out_bytes,
		const std::string& host = native_host( ), const std::uint16_t port = native_port( ), std::string* manifest_text = nullptr ) {
		detail::secure_clear( out_bytes );
		if ( access_token.empty( ) || hwid.empty( ) || module_name.empty( ) ) return false;

		const auto timestamp = static_cast< std::uint64_t >( std::chrono::duration_cast< std::chrono::seconds >(
			std::chrono::system_clock::now( ).time_since_epoch( ) ).count( ) );
		std::string nonce;
		std::string signature;
		const std::string proof_prefix = std::string( "crisis-artifact-proof:v1\nmodule\n" ) + module_name + "\n" +
			access_token + "\n" + std::to_string( timestamp ) + "\n";
		if ( !detail::signed_proof( detail::device_key( ), proof_prefix, "", nonce, signature ) ) return false;
		std::string payload = std::string( "{\"op\":\"module\",\"name\":\"" ) + detail::json_escape( module_name ) +
			"\",\"access_token\":\"" + access_token +
			"\",\"timestamp_unix\":" + std::to_string( timestamp ) +
			",\"nonce\":\"" + nonce + "\",\"signature\":\"" + signature + "\"}";
		detail::secure_clear( nonce );
		detail::secure_clear( signature );

		std::string error;
		std::string json;
		std::vector< std::uint8_t > encrypted;
		const bool downloaded = detail::native_exchange( host, port, payload, json, encrypted, error );
		detail::secure_clear( payload );
		if ( !downloaded ) return false;
		bool success = false;
		if ( !detail::parse_json_bool( json, "success", success ) || !success ) {
			detail::secure_clear( json );
			detail::secure_clear( encrypted );
			return false;
		}
		detail::secure_clear( json );

		std::vector< std::uint8_t > xor_key;
		detail::derive_xor_key( access_token, hwid, xor_key );
		detail::xor_decrypt( encrypted, xor_key );
		detail::secure_clear( xor_key );

		const bool valid = unwrap_module_payload( encrypted, out_bytes, manifest_text );
		detail::secure_clear( encrypted );
		return valid;
	}

	inline bool download_driver( const std::string& access_token, const std::string& hwid, const std::string& driver_name, std::vector< std::uint8_t >& out_bytes,
		const std::string& host = native_host( ), const std::uint16_t port = native_port( ) ) {
		detail::secure_clear( out_bytes );
		if ( access_token.empty( ) || hwid.empty( ) || driver_name.empty( ) ) return false;

		const auto timestamp = static_cast< std::uint64_t >( std::chrono::duration_cast< std::chrono::seconds >(
			std::chrono::system_clock::now( ).time_since_epoch( ) ).count( ) );
		std::string nonce;
		std::string signature;
		const std::string proof_prefix = std::string( "crisis-artifact-proof:v1\ndriver\n" ) + driver_name + "\n" +
			access_token + "\n" + std::to_string( timestamp ) + "\n";
		if ( !detail::signed_proof( detail::device_key( ), proof_prefix, "", nonce, signature ) ) return false;
		std::string payload = std::string( "{\"op\":\"driver\",\"name\":\"" ) + detail::json_escape( driver_name ) +
			"\",\"access_token\":\"" + access_token +
			"\",\"timestamp_unix\":" + std::to_string( timestamp ) +
			",\"nonce\":\"" + nonce + "\",\"signature\":\"" + signature + "\"}";
		detail::secure_clear( nonce );
		detail::secure_clear( signature );

		std::string error;
		std::string json;
		std::vector< std::uint8_t > encrypted;
		const bool downloaded = detail::native_exchange( host, port, payload, json, encrypted, error );
		detail::secure_clear( payload );
		if ( !downloaded ) return false;
		bool success = false;
		if ( !detail::parse_json_bool( json, "success", success ) || !success ) {
			detail::secure_clear( json );
			detail::secure_clear( encrypted );
			return false;
		}
		detail::secure_clear( json );

		std::vector< std::uint8_t > xor_key;
		detail::derive_xor_key( access_token, hwid, xor_key );
		detail::xor_decrypt( encrypted, xor_key );
		detail::secure_clear( xor_key );
		if ( encrypted.size( ) < 2 || encrypted[ 0 ] != 'M' || encrypted[ 1 ] != 'Z' ) {
			detail::secure_clear( encrypted );
			return false;
		}
		out_bytes = std::move( encrypted );
		return true;
	}

	namespace pe {
		inline manifest_t parse_manifest( const std::string& text ) {
			manifest_t manifest;
			manifest_import_t* current_dll = nullptr;
			std::istringstream stream( text );
			std::string line;

			while ( std::getline( stream, line ) ) {
				if ( !line.empty( ) && line.back( ) == '\r' ) line.pop_back( );
				if ( line.empty( ) ) continue;

				std::istringstream line_stream( line );
				std::string cmd;
				line_stream >> cmd;

				if ( cmd == "BASE" ) {
					line_stream >> manifest.image_base;
				}
				else if ( cmd == "IMAGE" ) {
					line_stream >> manifest.size_of_image;
				}
				else if ( cmd == "EP" ) {
					line_stream >> manifest.entry_point;
				}
				else if ( cmd == "SEC" ) {
					manifest_section_t sec;
					line_stream >> sec.name >> sec.rva >> sec.size >> sec.characteristics;
					manifest.sections.push_back( std::move( sec ) );
				}
				else if ( cmd == "DLL" ) {
					manifest_import_t imp;
					line_stream >> imp.dll;
					manifest.imports.push_back( std::move( imp ) );
					current_dll = &manifest.imports.back( );
				}
				else if ( cmd == "FN" && current_dll ) {
					std::string type;
					line_stream >> type;
					manifest_entry_t entry;
					if ( type == "ordinal" ) {
						line_stream >> entry.ordinal >> entry.rva;
					}
					else {
						entry.name = type;
						line_stream >> entry.rva;
					}
					current_dll->entries.push_back( std::move( entry ) );
				}
				else if ( cmd == "TLS" ) {
					std::uint32_t rva = 0;
					line_stream >> rva;
					manifest.tls_callbacks.push_back( rva );
				}
				else if ( cmd == "EXP" ) {
					manifest_export_t exp;
					line_stream >> exp.name >> exp.rva >> exp.ordinal;
					manifest.exports.push_back( std::move( exp ) );
				}
				else if ( cmd == "RELOC" ) {
					line_stream >> manifest.reloc_rva >> manifest.reloc_size;
					manifest.has_relocations = true;
				}
			}
			return manifest;
		}

		inline mapped_module_t load_mapped( const std::vector< std::uint8_t >& mapped_bytes, const manifest_t& manifest ) {
			mapped_module_t result { };
			result.manifest = manifest;
			if ( mapped_bytes.empty( ) || manifest.size_of_image == 0 ) return result;

			auto* allocated = reinterpret_cast< std::uint8_t* >(
				VirtualAlloc( reinterpret_cast< void* >( manifest.image_base ), manifest.size_of_image, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) );

			std::uintptr_t delta = 0;
			if ( !allocated ) {
				allocated = reinterpret_cast< std::uint8_t* >(
					VirtualAlloc( nullptr, manifest.size_of_image, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) );
				if ( !allocated ) return result;
				delta = reinterpret_cast< std::uintptr_t >( allocated ) - static_cast< std::uintptr_t >( manifest.image_base );
			}

			result.base = allocated;
			const auto copy_size = ( std::min ) ( mapped_bytes.size( ), static_cast< std::size_t >( manifest.size_of_image ) );
			std::memcpy( allocated, mapped_bytes.data( ), copy_size );

			if ( delta != 0 ) {
				auto* dos = reinterpret_cast< IMAGE_DOS_HEADER* >( allocated );
				auto* optional = reinterpret_cast< IMAGE_OPTIONAL_HEADER* >( allocated + dos->e_lfanew + 4 + sizeof( IMAGE_FILE_HEADER ) );
				if ( optional->Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC ) {
					reinterpret_cast< IMAGE_OPTIONAL_HEADER32* >( optional )->ImageBase = static_cast< DWORD >( reinterpret_cast< std::uintptr_t >( allocated ) );
				}
				else {
					reinterpret_cast< IMAGE_OPTIONAL_HEADER64* >( optional )->ImageBase = reinterpret_cast< std::uintptr_t >( allocated );
				}
			}

			if ( delta != 0 && manifest.has_relocations ) {
				auto* reloc_dir = reinterpret_cast< IMAGE_BASE_RELOCATION* >( allocated + manifest.reloc_rva );
				while ( reloc_dir->VirtualAddress != 0 ) {
					const std::uintptr_t page = reinterpret_cast< std::uintptr_t >( allocated ) + reloc_dir->VirtualAddress;
					auto* entries = reinterpret_cast< WORD* >( reinterpret_cast< std::uint8_t* >( reloc_dir ) + sizeof( IMAGE_BASE_RELOCATION ) );
					const DWORD count = ( reloc_dir->SizeOfBlock - sizeof( IMAGE_BASE_RELOCATION ) ) / sizeof( WORD );
					for ( DWORD i = 0; i < count; ++i ) {
						const WORD type = ( entries[ i ] >> 12 ) & 0xF;
						const WORD offset = entries[ i ] & 0xFFF;
						if ( type == IMAGE_REL_BASED_DIR64 ) {
							*reinterpret_cast< std::uint64_t* >( page + offset ) += delta;
						}
						else if ( type == IMAGE_REL_BASED_HIGHLOW ) {
							*reinterpret_cast< std::uint32_t* >( page + offset ) += static_cast< std::uint32_t >( delta );
						}
						else if ( type == IMAGE_REL_BASED_HIGH ) {
							*reinterpret_cast< std::uint16_t* >( page + offset ) += static_cast< std::uint16_t >( ( delta >> 16 ) & 0xFFFF );
						}
						else if ( type == IMAGE_REL_BASED_LOW ) {
							*reinterpret_cast< std::uint16_t* >( page + offset ) += static_cast< std::uint16_t >( delta & 0xFFFF );
						}
					}
					reloc_dir = reinterpret_cast< IMAGE_BASE_RELOCATION* >(
						reinterpret_cast< std::uint8_t* >( reloc_dir ) + reloc_dir->SizeOfBlock );
				}
			}

			std::vector< HMODULE > loaded_modules;
			for ( const auto& imp : manifest.imports ) {
				HMODULE h_mod = LoadLibraryA( imp.dll.c_str( ) );
				loaded_modules.push_back( h_mod );
				for ( const auto& entry : imp.entries ) {
					void* addr = nullptr;
					if ( entry.name.empty( ) ) {
						addr = reinterpret_cast< void* >( GetProcAddress( h_mod, reinterpret_cast< LPCSTR >( static_cast< std::uintptr_t >( entry.ordinal ) ) ) );
					}
					else {
						addr = reinterpret_cast< void* >( GetProcAddress( h_mod, entry.name.c_str( ) ) );
					}
					*reinterpret_cast< void** >( allocated + entry.rva ) = addr;
				}
			}
			if ( !loaded_modules.empty( ) ) {
				result.import_modules = new HMODULE[ loaded_modules.size( ) ];
				result.import_module_count = loaded_modules.size( );
				std::memcpy( result.import_modules, loaded_modules.data( ), loaded_modules.size( ) * sizeof( HMODULE ) );
			}

			for ( const auto& sec : manifest.sections ) {
				if ( sec.size == 0 ) continue;
				DWORD new_protect = PAGE_NOACCESS;
				if ( sec.characteristics & IMAGE_SCN_MEM_EXECUTE ) {
					new_protect = ( sec.characteristics & IMAGE_SCN_MEM_WRITE ) ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
				}
				else if ( sec.characteristics & IMAGE_SCN_MEM_READ ) {
					new_protect = ( sec.characteristics & IMAGE_SCN_MEM_WRITE ) ? PAGE_READWRITE : PAGE_READONLY;
				}
				else if ( sec.characteristics & IMAGE_SCN_MEM_WRITE ) {
					new_protect = PAGE_READWRITE;
				}
				DWORD old_protect = 0;
				VirtualProtect( allocated + sec.rva, sec.size, new_protect, &old_protect );
			}

			using tls_callback_t = VOID( WINAPI* )( PVOID, DWORD, PVOID );
			for ( const auto rva : manifest.tls_callbacks ) {
				auto cb = reinterpret_cast< tls_callback_t >( allocated + rva );
				cb( allocated, DLL_PROCESS_ATTACH, nullptr );
			}

			using dllmain_t = BOOL( WINAPI* )( HINSTANCE, DWORD, LPVOID );
			if ( manifest.entry_point != 0 ) {
				auto entry = reinterpret_cast< dllmain_t >( allocated + manifest.entry_point );
				entry( reinterpret_cast< HINSTANCE >( allocated ), DLL_PROCESS_ATTACH, nullptr );
			}

			result.ok = true;
			return result;
		}

		inline void* get_export( mapped_module_t& mod, const char* export_name ) {
			if ( !mod.ok || !mod.base || !export_name ) return nullptr;
			for ( const auto& exp : mod.manifest.exports ) {
				if ( exp.name == export_name ) return mod.base + exp.rva;
			}
			return nullptr;
		}

		inline void free_mapped( mapped_module_t& mod ) {
			if ( !mod.ok || !mod.base ) return;

			using dllmain_t = BOOL( WINAPI* )( HINSTANCE, DWORD, LPVOID );
			if ( mod.manifest.entry_point != 0 ) {
				auto entry = reinterpret_cast< dllmain_t >( mod.base + mod.manifest.entry_point );
				entry( reinterpret_cast< HINSTANCE >( mod.base ), DLL_PROCESS_DETACH, nullptr );
			}

			using tls_callback_t = VOID( WINAPI* )( PVOID, DWORD, PVOID );
			for ( const auto rva : mod.manifest.tls_callbacks ) {
				auto cb = reinterpret_cast< tls_callback_t >( mod.base + rva );
				cb( mod.base, DLL_PROCESS_DETACH, nullptr );
			}

			VirtualFree( mod.base, 0, MEM_RELEASE );
			mod.base = nullptr;
			for ( std::size_t i = 0; i < mod.import_module_count; ++i ) {
				if ( mod.import_modules[ i ] ) FreeLibrary( mod.import_modules[ i ] );
			}
			delete[ ] mod.import_modules;
			mod.import_modules = nullptr;
			mod.import_module_count = 0;
			mod.ok = false;
		}
	}

	inline response_t authenticate_and_load( const std::string& key, const std::string& module_name, pe::mapped_module_t& out_module,
		const std::string& host = native_host( ), const std::uint16_t port = native_port( ) ) {
		out_module = { };
		const std::string hwid = generate_hwid( );
		response_t response = authenticate( key, hwid, host, port );
		if ( !response.success ) return response;

		std::string manifest_text;
		std::vector< std::uint8_t > mapped_bytes;
		if ( !download_and_decrypt_module( response.access_token, hwid, module_name, mapped_bytes, host, port, &manifest_text ) ) {
			response.success = false;
			response.message = "failed to download or decrypt module.";
			return response;
		}

		out_module = pe::load_mapped( mapped_bytes, pe::parse_manifest( manifest_text ) );
		detail::secure_clear( mapped_bytes );
		if ( !manifest_text.empty( ) ) SecureZeroMemory( manifest_text.data( ), manifest_text.size( ) );
		if ( !out_module.ok ) {
			response.success = false;
			response.message = "failed to map module into memory.";
			return response;
		}
		return response;
	}
}
