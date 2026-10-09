#include <deps/includes.h>

namespace stats::api {
        response_t get( const request_t& request ) {
            response_t result{ };

            const auto session = WinHttpOpen(
                obf( L"crisis-stats/1.0" ),
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0
            );
            if ( !session )
                return result;

            DWORD timeout_ms = 8000;
            WinHttpSetOption( session, WINHTTP_OPTION_RESOLVE_TIMEOUT, &timeout_ms, sizeof( timeout_ms ) );
            WinHttpSetOption( session, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof( timeout_ms ) );
            WinHttpSetOption( session, WINHTTP_OPTION_SEND_TIMEOUT,    &timeout_ms, sizeof( timeout_ms ) );
            WinHttpSetOption( session, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof( timeout_ms ) );

            const auto connect = WinHttpConnect(
                session,
                request.host.c_str( ),
                request.secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT,
                0
            );
            if ( !connect ) {
                WinHttpCloseHandle( session );
                return result;
            }

            const auto req = WinHttpOpenRequest(
                connect,
                obf( L"GET" ),
                request.path.c_str( ),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                request.secure ? WINHTTP_FLAG_SECURE : 0
            );
            if ( !req ) {
                WinHttpCloseHandle( connect );
                WinHttpCloseHandle( session );
                return result;
            }

            for ( const auto& [ key, value ] : request.headers ) {
                const auto header = key + std::wstring( static_cast<const wchar_t*>( obf( L": " ) ) ) + value;
                WinHttpAddRequestHeaders(
                    req,
                    header.c_str( ),
                    static_cast< DWORD >( header.size( ) ),
                    WINHTTP_ADDREQ_FLAG_ADD
                );
            }

            bool ok = WinHttpSendRequest(
                req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0, 0, 0
            );
            if ( ok )
                ok = WinHttpReceiveResponse( req, nullptr );

            if ( ok ) {
                DWORD status_size = sizeof( DWORD );
                DWORD status = 0;
                WinHttpQueryHeaders(
                    req,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX,
                    &status, &status_size, WINHTTP_NO_HEADER_INDEX
                );
                result.status_code = status;

                std::string body;
                DWORD available = 0;
                while ( WinHttpQueryDataAvailable( req, &available ) && available > 0 ) {
                    std::string chunk( available, '\0' );
                    DWORD read = 0;
                    if ( !WinHttpReadData( req, chunk.data( ), available, &read ) )
                        break;
                    chunk.resize( read );
                    body += chunk;
                }

                result.body = std::move( body );
                result.success = true;
            }

            WinHttpCloseHandle( req );
            WinHttpCloseHandle( connect );
            WinHttpCloseHandle( session );

            return result;
        }

        std::string url_encode( const std::string& value ) {
            std::string result;
            for ( unsigned char c : value ) {
                if ( std::isalnum( c ) || c == '-' || c == '_' || c == '.' || c == '~' ) {
                    result += static_cast< char >( c );
                } else {
                    char buffer[ 4 ]{ };
                    std::snprintf( buffer, sizeof( buffer ), obf( "%%%02X" ), c );
                    result += buffer;
                }
            }
            return result;
        }

        std::wstring to_wide( const std::string& value ) {
            return std::wstring( value.begin( ), value.end( ) );
        }

        // fetch a single asset (icon/image) from fortnite.gg with a browsery
        // User-Agent + Referer. body of the response is the raw bytes; this
        // is the same shape as get() but centralized so callers don't repeat
        // the header dance and can hand the bytes straight to WIC.
        response_t fetch_asset( const std::wstring& path ) {
            request_t request{ };
            request.host = static_cast<const wchar_t*>( obf( L"fortnite.gg" ) );
            request.path = path;
            request.secure = true;
            request.headers.push_back( {
                static_cast<const wchar_t*>( obf( L"User-Agent" ) ),
                static_cast<const wchar_t*>( obf( L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36" ) )
            } );
            request.headers.push_back( { static_cast<const wchar_t*>( obf( L"Referer" ) ), static_cast<const wchar_t*>( obf( L"https://fortnite.gg/weapons" ) ) } );
            return get( request );
        }

        std::string to_lower( std::string value ) {
            for ( auto& c : value )
                c = static_cast< char >( std::tolower( static_cast< unsigned char >( c ) ) );
            return value;
        }

        std::optional<std::string> extract_string_field( const std::string& json, const std::string& key ) {
            const auto needle = std::string( static_cast<const char*>( obf( "\"" ) ) ) + key + std::string( static_cast<const char*>( obf( "\":\"" ) ) );
            const auto pos = json.find( needle );
            if ( pos == std::string::npos )
                return std::nullopt;

            const auto start = pos + needle.size( );
            const auto end = json.find( '"', start );
            if ( end == std::string::npos )
                return std::nullopt;

            return json.substr( start, end - start );
        }

        std::optional<double> extract_number_field( const std::string& json, const std::string& key ) {
            const auto needle = std::string( static_cast<const char*>( obf( "\"" ) ) ) + key + std::string( static_cast<const char*>( obf( "\":" ) ) );
            const auto pos = json.find( needle );
            if ( pos == std::string::npos )
                return std::nullopt;

            auto start = pos + needle.size( );
            auto end = start;
            while ( end < json.size( ) &&
                ( std::isdigit( static_cast< unsigned char >( json[ end ] ) ) || json[ end ] == '.' || json[ end ] == '-' ) )
                ++end;

            if ( end == start )
                return std::nullopt;

            return std::stod( json.substr( start, end - start ) );
        }

        std::string slice_object( const std::string& json, const std::string& key, std::size_t from ) {
            const auto needle = std::string( static_cast<const char*>( obf( "\"" ) ) ) + key + std::string( static_cast<const char*>( obf( "\":{" ) ) );
            const auto pos = json.find( needle, from );
            if ( pos == std::string::npos )
                return { };

            const auto start = pos + needle.size( ) - 1;
            int depth = 0;
            std::size_t i = start;
            for ( ; i < json.size( ); i++ ) {
                if ( json[ i ] == '{' ) {
                    depth++;
                } else if ( json[ i ] == '}' ) {
                    depth--;
                    if ( depth == 0 ) {
                        i++;
                        break;
                    }
                }
            }

            return json.substr( start, i - start );
        }

        std::optional<std::string> extract_between(
            const std::string& text, const std::string& start_marker, const std::string& end_marker, std::size_t from ) {

            if ( start_marker.empty( ) || end_marker.empty( ) )
                return std::nullopt;

            const auto start = text.find( start_marker, from );
            if ( start == std::string::npos )
                return std::nullopt;

            const auto content_start = start + start_marker.size( );
            const auto end = text.find( end_marker, content_start );
            if ( end == std::string::npos )
                return std::nullopt;

            return text.substr( content_start, end - content_start );
        }

        std::string strip_tags( const std::string& fragment ) {
            std::string result;
            bool in_tag = false;
            for ( char c : fragment ) {
                if ( c == '<' ) {
                    in_tag = true;
                } else if ( c == '>' ) {
                    in_tag = false;
                } else if ( !in_tag ) {
                    result += c;
                }
            }
            return result;
        }
}
