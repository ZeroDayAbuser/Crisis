#pragma once

// Obsolete, unreferenced implementation retained only for repository history.
// Production authentication is implemented by ext/backend/backend.hxx over
// TCP sockets with device-key proof. Do not include this legacy unsigned client.

#include <Windows.h>
#include <winhttp.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace auth {
    struct response_t {
        bool success = false;
        std::uint64_t start_unix = 0;
        std::uint64_t expiry_unix = 0;
        std::uint64_t access_token_expires_unix = 0;
        std::uint64_t player_token_expires_unix = 0;
        std::string access_token;
        std::string player_token;
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
        class internet_handle_t {
        public:
            internet_handle_t( ) = default;
            explicit internet_handle_t( HINTERNET handle ) : m_handle( handle ) { }
            ~internet_handle_t( ) { reset( ); }

            internet_handle_t( const internet_handle_t& ) = delete;
            internet_handle_t& operator=( const internet_handle_t& ) = delete;

            [[nodiscard]] HINTERNET get( ) const { return m_handle; }
            [[nodiscard]] explicit operator bool( ) const { return m_handle != nullptr; }

            void reset( HINTERNET handle = nullptr ) {
                if ( m_handle ) WinHttpCloseHandle( m_handle );
                m_handle = handle;
            }

        private:
            HINTERNET m_handle = nullptr;
        };
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


        inline std::string trim( std::string value ) {
            const auto is_space = [ ]( const unsigned char ch ) { return std::isspace( ch ) != 0; };
            value.erase( value.begin( ), std::find_if_not( value.begin( ), value.end( ), is_space ) );
            value.erase( std::find_if_not( value.rbegin( ), value.rend( ), is_space ).base( ), value.end( ) );
            return value;
        }

        inline std::string normalize_pasted_key( std::string value ) {
            value = trim( std::move( value ) );
            while ( value.size( ) >= 2 ) {
                const char first = value.front( ), last = value.back( );
                const bool wrapped = ( first == '`' && last == '`' ) || ( first == '"' && last == '"' )
                    || ( first == '\'' && last == '\'' );
                if ( !wrapped ) break;
                value = trim( value.substr( 1, value.size( ) - 2 ) );
            }
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
        inline bool valid_access_token( const std::string& token ) {
            return token.size( ) == 43 &&
                std::all_of( token.begin( ), token.end( ), [ ]( const unsigned char ch ) {
                    return std::isalnum( ch ) != 0 || ch == '-' || ch == '_';
                } );
        }


        inline bool parse_json_bool( const std::string& json, const std::string& key, bool& value ) {
            const std::string marker = "\"" + key + "\"";
            auto pos = json.find( marker );
            if ( pos == std::string::npos || ( pos = json.find( ':', pos + marker.size( ) ) ) == std::string::npos ) return false;
            pos = json.find_first_not_of( " \t\r\n", pos + 1 );
            if ( pos == std::string::npos ) return false;
            if ( json.compare( pos, 4, "true" ) == 0 ) { value = true; return true; }
            if ( json.compare( pos, 5, "false" ) == 0 ) { value = false; return true; }
            return false;
        }

        inline bool parse_json_uint64( const std::string& json, const std::string& key, std::uint64_t& value ) {
            const std::string marker = "\"" + key + "\"";
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
            const std::string marker = "\"" + key + "\"";
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

        inline bool is_loopback( std::wstring host ) {
            std::transform( host.begin( ), host.end( ), host.begin( ), ::towlower );
            return host == L"localhost" || host == L"127.0.0.1" || host == L"::1" || host == L"[::1]";
        }

        inline bool crack_auth_url( const std::string& base_url, std::wstring& host, std::wstring& base_path, INTERNET_PORT& port, bool& secure, std::string& error ) {
            const int wide_size = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, base_url.c_str( ), -1, nullptr, 0 );
            if ( wide_size <= 0 ) { error = "Authentication URL is not valid UTF-8."; return false; }

            std::wstring wide_url( static_cast< std::size_t >( wide_size ), L'\0' );
            MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, base_url.c_str( ), -1, wide_url.data( ), wide_size );

            URL_COMPONENTS parts { };
            parts.dwStructSize = sizeof( parts );
            parts.dwSchemeLength = static_cast< DWORD >( -1 );
            parts.dwHostNameLength = static_cast< DWORD >( -1 );
            parts.dwUrlPathLength = static_cast< DWORD >( -1 );
            if ( !WinHttpCrackUrl( wide_url.data( ), 0, 0, &parts ) || !parts.lpszHostName || parts.dwHostNameLength == 0 ) {
                error = "CRISIS_AUTH_URL is invalid.";
                return false;
            }

            host.assign( parts.lpszHostName, parts.dwHostNameLength );
            secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
            if ( !secure && ( parts.nScheme != INTERNET_SCHEME_HTTP || !is_loopback( host ) ) ) {
                error = "Authentication requires HTTPS for non-loopback servers.";
                return false;
            }

            base_path = parts.dwUrlPathLength
                ? std::wstring( parts.lpszUrlPath, parts.dwUrlPathLength )
                : L"";
            while ( !base_path.empty( ) && base_path.back( ) == L'/' ) base_path.pop_back( );
            port = parts.nPort;
            return true;
        }

        inline bool to_wide_path( const std::wstring& base_path, const std::string& path, std::wstring& wide_path, std::string& error ) {
            const int path_size = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str( ), -1, nullptr, 0 );
            if ( path_size <= 0 ) { error = "Authentication request path is invalid."; return false; }
            wide_path.assign( static_cast< std::size_t >( path_size ), L'\0' );
            MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str( ), -1, wide_path.data( ), path_size );
            wide_path.resize( static_cast< std::size_t >( path_size - 1 ) );
            wide_path = base_path + wide_path;
            return true;
        }

        inline bool http_post_json( const std::string& base_url, const std::string& path, const std::string& payload, std::string& body, std::string& error ) {
            std::wstring host;
            std::wstring base_path;
            INTERNET_PORT port = 0;
            bool secure = false;
            if ( !crack_auth_url( base_url, host, base_path, port, secure, error ) ) return false;

            std::wstring wide_path;
            if ( !to_wide_path( base_path, path, wide_path, error ) ) return false;

            internet_handle_t session( WinHttpOpen(
                L"crisis-loader/1.0",
                WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0 ) );
            if ( !session ) { error = "Failed to initialize HTTP."; return false; }
            WinHttpSetTimeouts( session.get( ), 5000, 5000, 10000, 10000 );

            internet_handle_t connection( WinHttpConnect( session.get( ), host.c_str( ), port, 0 ) );
            if ( !connection ) { error = "Failed to connect to authentication server."; return false; }

            internet_handle_t request( WinHttpOpenRequest(
                connection.get( ), L"POST", wide_path.c_str( ), nullptr, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0 ) );
            if ( !request ) { error = "Failed to create authentication request."; return false; }

            const wchar_t headers[ ] = L"Accept: application/json\r\nContent-Type: application/json\r\nCache-Control: no-store\r\n";
            if ( !WinHttpSendRequest( request.get( ), headers, static_cast< DWORD >( -1L ),
                const_cast< char* >( payload.data( ) ), static_cast< DWORD >( payload.size( ) ),
                static_cast< DWORD >( payload.size( ) ), 0 ) || !WinHttpReceiveResponse( request.get( ), nullptr ) ) {
                error = "Authentication server request failed.";
                return false;
            }

            DWORD status = 0;
            DWORD status_size = sizeof( status );
            if ( !WinHttpQueryHeaders( request.get( ), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX ) ) {
                error = "Authentication server returned no status.";
                return false;
            }

            body.clear( );
            std::vector< char > buffer( 4096 );
            for ( ;; ) {
                DWORD read = 0;
                if ( !WinHttpReadData( request.get( ), buffer.data( ), static_cast< DWORD >( buffer.size( ) ), &read ) ) {
                    error = "Failed to read authentication response.";
                    return false;
                }
                if ( read == 0 ) break;
                if ( body.size( ) + read > 64 * 1024 ) { error = "Authentication response was too large."; return false; }
                body.append( buffer.data( ), read );
            }

            if ( status != HTTP_STATUS_OK ) {
                std::string server_error;
                error = parse_json_string( body, "error", server_error ) ? server_error
                    : "Authentication server rejected the request (HTTP " + std::to_string( status ) + ").";
                return false;
            }
            return !body.empty( );
        }


        inline bool http_download( const std::string& base_url, const std::string& path, const std::string& access_token, std::vector< std::uint8_t >& data, std::string& error ) {
            secure_clear( data );
            if ( !valid_access_token( access_token ) ) {
                error = "Authentication bearer token is invalid.";
                return false;
            }

            std::wstring host;
            std::wstring base_path;
            INTERNET_PORT port = 0;
            bool secure = false;
            if ( !crack_auth_url( base_url, host, base_path, port, secure, error ) ) return false;

            std::wstring wide_path;
            if ( !to_wide_path( base_path, path, wide_path, error ) ) return false;

            internet_handle_t session( WinHttpOpen(
                L"crisis-loader/1.0",
                WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0 ) );
            if ( !session ) { error = "Failed to initialize HTTP."; return false; }
            WinHttpSetTimeouts( session.get( ), 5000, 5000, 60000, 60000 );

            internet_handle_t connection( WinHttpConnect( session.get( ), host.c_str( ), port, 0 ) );
            if ( !connection ) { error = "Failed to connect to authentication server."; return false; }

            internet_handle_t request( WinHttpOpenRequest(
                connection.get( ), L"GET", wide_path.c_str( ), nullptr, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0 ) );
            if ( !request ) { error = "Failed to create authentication request."; return false; }

            std::wstring authorization = L"Authorization: Bearer " +
                std::wstring( access_token.begin( ), access_token.end( ) ) + L"\r\nAccept: application/octet-stream\r\n";
            const bool request_sent = WinHttpSendRequest(
                request.get( ), authorization.c_str( ), static_cast< DWORD >( -1L ),
                WINHTTP_NO_REQUEST_DATA, 0, 0, 0 ) != FALSE;
            secure_clear( authorization );
            if ( !request_sent || !WinHttpReceiveResponse( request.get( ), nullptr ) ) {
                error = "Authentication server request failed.";
                return false;
            }

            DWORD status = 0;
            DWORD status_size = sizeof( status );
            if ( !WinHttpQueryHeaders( request.get( ), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX ) ) {
                error = "Authentication server returned no status.";
                return false;
            }

            constexpr std::size_t maximum_download_size = 64ull * 1024 * 1024;
            for ( ;; ) {
                const auto before = data.size( );
                data.resize( before + 8192 );
                DWORD read = 0;
                if ( !WinHttpReadData( request.get( ), data.data( ) + before, 8192, &read ) ) {
                    secure_clear( data );
                    error = "Failed to read authentication response.";
                    return false;
                }
                if ( before > maximum_download_size || read > maximum_download_size - before ) {
                    secure_clear( data );
                    error = "Authentication response was too large.";
                    return false;
                }
                data.resize( before + read );
                if ( read == 0 ) break;
            }

            if ( status != HTTP_STATUS_OK ) {
                secure_clear( data );
                error = "Authentication server rejected the request.";
                return false;
            }
            return !data.empty( );
        }

        inline void derive_xor_key( const std::string& access_token, const std::string& hwid, std::vector< std::uint8_t >& out_key ) {
            std::int32_t hash = 0;
            const auto absorb = [ &hash ]( const std::string& value ) {
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
        std::cout << "Enter license key: " << std::flush;
        const HANDLE input = GetStdHandle( STD_INPUT_HANDLE );
        DWORD old_mode = 0;
        const bool console = input != INVALID_HANDLE_VALUE && GetConsoleMode( input, &old_mode );
        if ( console ) SetConsoleMode( input, old_mode & ~ENABLE_ECHO_INPUT );

        const bool read = static_cast< bool >( std::getline( std::cin, key ) );
        if ( console ) {
            SetConsoleMode( input, old_mode );
            std::cout << '\n';
        }
        key = detail::normalize_pasted_key( std::move( key ) );
        return read && !key.empty( );
    }

    inline std::string server_url( ) {
        return "https://wauth.aaddpp.lol";
    }

    inline response_t authenticate( const std::string& key, const std::string& hwid, const std::string& base_url = server_url( ) ) {
        response_t response;
        if ( key.empty( ) ) { response.message = "License key is required."; return response; }
        if ( hwid.empty( ) ) { response.message = "Failed to generate the hardware identifier."; return response; }

        std::string payload = "{\"key\":\"" + detail::json_escape( key ) + "\",\"hwid\":\"" + detail::json_escape( hwid ) + "\"}";
        std::string json;
        const bool authenticated = detail::http_post_json( base_url, "/api/client/authenticate", payload, json, response.message );
        detail::secure_clear( payload );
        if ( !authenticated ) return response;

        if ( !detail::parse_json_bool( json, "success", response.success ) ) {
            response.message = "Authentication server returned an invalid response.";
            detail::secure_clear( json );
            return response;
        }
        if ( !response.success ) {
            if ( !detail::parse_json_string( json, "error", response.message ) ) response.message = "License is invalid or expired.";
            detail::secure_clear( json );
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
            response.expiry_unix < response.start_unix || token_type != "Bearer" ||
            player_token_type != "Bearer" ||
            !detail::valid_access_token( response.access_token ) ||
            !detail::valid_access_token( response.player_token ) ) {
            response.success = false;
            response.message = "Authentication response is missing valid bearer credentials.";
            detail::secure_clear( response.access_token );
            detail::secure_clear( response.player_token );
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
            response.message = "Authentication credentials have expired.";
            detail::secure_clear( response.access_token );
            detail::secure_clear( response.player_token );
            detail::secure_clear( json );
            return response;
        }

        const auto remaining = response.expiry_unix - now;
        response.message = "Authenticated; subscription has " + std::to_string( remaining ) + " seconds remaining.";
        if ( detail::parse_json_string( json, "pairing_code", response.pairing_code ) && !response.pairing_code.empty( ) ) {
            response.message += " Discord pairing code: " + response.pairing_code;
        }
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
        if ( payload.size( ) - mapped_offset < 2 ) return false;
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

    inline bool download_and_decrypt_module( const std::string& access_token, const std::string& hwid, const std::string& module_name, std::vector< std::uint8_t >& out_bytes, const std::string& base_url = server_url( ) ) {
        out_bytes.clear( );
        if ( access_token.empty( ) || hwid.empty( ) || module_name.empty( ) ) return false;

        const std::string path = "/module?name=" + detail::url_encode( module_name );
        std::string error;
        std::vector< std::uint8_t > encrypted;
        if ( !detail::http_download( base_url, path, access_token, encrypted, error ) ) return false;

        std::vector< std::uint8_t > xor_key;
        detail::derive_xor_key( access_token, hwid, xor_key );
        detail::xor_decrypt( encrypted, xor_key );
        SecureZeroMemory( xor_key.data( ), xor_key.size( ) );

        const bool valid = unwrap_module_payload( encrypted, out_bytes );
        SecureZeroMemory( encrypted.data( ), encrypted.size( ) );
        return valid;
    }

    inline bool download_driver( const std::string& access_token, const std::string& hwid, const std::string& driver_name, std::vector< std::uint8_t >& out_bytes, const std::string& base_url = server_url( ) ) {
        out_bytes.clear( );
        if ( access_token.empty( ) || hwid.empty( ) || driver_name.empty( ) ) return false;

        const std::string path = "/driver?name=" + detail::url_encode( driver_name );
        std::string error;
        std::vector< std::uint8_t > encrypted;
        if ( !detail::http_download( base_url, path, access_token, encrypted, error ) ) return false;

        std::vector< std::uint8_t > xor_key;
        detail::derive_xor_key( access_token, hwid, xor_key );
        detail::xor_decrypt( encrypted, xor_key );
        SecureZeroMemory( xor_key.data( ), xor_key.size( ) );
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
            const auto copy_size = ( std::min )( mapped_bytes.size( ), static_cast< std::size_t >( manifest.size_of_image ) );
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

    inline response_t authenticate_and_load( const std::string& key, const std::string& module_name, pe::mapped_module_t& out_module, const std::string& base_url = server_url( ) ) {
        out_module = { };
        const std::string hwid = generate_hwid( );
        response_t response = authenticate( key, hwid, base_url );
        if ( !response.success ) return response;

        const std::string path = "/module?name=" + detail::url_encode( module_name );
        std::string error;
        std::vector< std::uint8_t > encrypted;
        if ( !detail::http_download( base_url, path, response.access_token, encrypted, error ) ) {
            response.success = false;
            response.message = "Failed to download or decrypt module.";
            return response;
        }

        std::vector< std::uint8_t > xor_key;
        detail::derive_xor_key( response.access_token, hwid, xor_key );
        detail::xor_decrypt( encrypted, xor_key );
        SecureZeroMemory( xor_key.data( ), xor_key.size( ) );

        std::string manifest_text;
        std::vector< std::uint8_t > mapped_bytes;
        if ( !unwrap_module_payload( encrypted, mapped_bytes, &manifest_text ) ) {
            SecureZeroMemory( encrypted.data( ), encrypted.size( ) );
            response.success = false;
            response.message = "Module response missing manifest or mapped image.";
            return response;
        }
        SecureZeroMemory( encrypted.data( ), encrypted.size( ) );

        out_module = pe::load_mapped( mapped_bytes, pe::parse_manifest( manifest_text ) );
        SecureZeroMemory( mapped_bytes.data( ), mapped_bytes.size( ) );
        SecureZeroMemory( manifest_text.data( ), manifest_text.size( ) );
        if ( !out_module.ok ) {
            response.success = false;
            response.message = "Failed to map module into memory.";
        }
        return response;
    }
}
