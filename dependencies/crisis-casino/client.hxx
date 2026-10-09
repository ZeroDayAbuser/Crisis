#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <bcrypt.h>
#include <objbase.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwctype>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <utility>

#pragma comment( lib, "bcrypt.lib" )
#pragma comment( lib, "ole32.lib" )
#pragma comment( lib, "ws2_32.lib" )

namespace crisis::casino {
    struct result_t {
        bool transport_ok = false;
        bool success = false;
        bool proof_verified = false;
        unsigned long status_code = 0;
        std::string error;
        std::string status;
        std::string message;
        std::string color;
        std::string commitment;
        std::string next_commitment;
        std::string server_seed;
        std::string kind;
        std::string feature;
        std::string risk;
        std::uint64_t balance = 0;
        std::uint64_t payout = 0;
        std::uint64_t bet = 0;
        int number = -1;
        int crossed = 0;
        int mines = 0;
        double multiplier = 1.0;
        double crash_point = 1.0;
        bool won = false;
        bool caught = false;
        std::vector< int > revealed;
        std::vector< int > mine_positions;
        std::vector< std::string > player_cards;
        std::vector< std::string > dealer_cards;
        std::string raw;

        [[nodiscard]] bool settled( ) const {
            return status == "won" || status == "lost" || status == "push"
                || status == "blackjack" || status == "cashed_out";
        }
    };

    namespace detail {
        inline std::wstring utf8_to_wide( const std::string& value ) {
            if ( value.empty( ) ) return {};
            const int size = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, value.data( ),
                static_cast< int >( value.size( ) ), nullptr, 0 );
            if ( size <= 0 ) return {};
            std::wstring result( static_cast< std::size_t >( size ), L'\0' );
            MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, value.data( ),
                static_cast< int >( value.size( ) ), result.data( ), size );
            return result;
        }

        inline std::string env( const char* name ) {
            char* value = nullptr; std::size_t size = 0;
            if ( _dupenv_s( &value, &size, name ) != 0 || !value ) return {};
            std::string result( value ); free( value ); return result;
        }
        inline bool valid_access_token( const std::string& value ) {
            return value.size( ) == 43 && std::all_of( value.begin( ), value.end( ), [ ]( const unsigned char ch ) {
                return std::isalnum( ch ) != 0 || ch == '-' || ch == '_';
            } );
        }

        inline std::string json_escape( std::string_view value ) {
            std::string output; output.reserve( value.size( ) + 8 );
            for ( const unsigned char ch : value ) {
                switch ( ch ) {
                case '"': output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '\b': output += "\\b"; break;
                case '\f': output += "\\f"; break;
                case '\n': output += "\\n"; break;
                case '\r': output += "\\r"; break;
                case '\t': output += "\\t"; break;
                default: if ( ch >= 0x20 ) output.push_back( static_cast< char >( ch ) ); break;
                }
            }
            return output;
        }

        inline std::optional< std::size_t > value_at( const std::string& json, std::string_view key ) {
            const std::string needle = "\"" + std::string( key ) + "\"";
            auto pos = json.find( needle ); if ( pos == std::string::npos ) return std::nullopt;
            pos = json.find( ':', pos + needle.size( ) ); if ( pos == std::string::npos ) return std::nullopt;
            do { ++pos; } while ( pos < json.size( ) && std::isspace( static_cast< unsigned char >( json[ pos ] ) ) );
            return pos < json.size( ) ? std::optional< std::size_t >( pos ) : std::nullopt;
        }
        inline std::string object_value( const std::string& json, std::string_view key ) {
            const auto at = value_at( json, key );
            if ( !at || json[ *at ] != '{' ) return {};
            std::size_t depth = 0; bool quoted = false; bool escaped = false;
            for ( std::size_t i = *at; i < json.size( ); ++i ) {
                const char ch = json[ i ];
                if ( quoted ) {
                    if ( escaped ) escaped = false;
                    else if ( ch == '\\' ) escaped = true;
                    else if ( ch == '"' ) quoted = false;
                    continue;
                }
                if ( ch == '"' ) { quoted = true; continue; }
                if ( ch == '{' ) ++depth;
                else if ( ch == '}' && --depth == 0 ) return json.substr( *at, i - *at + 1 );
            }
            return {};
        }

        inline std::string string_value( const std::string& json, std::string_view key ) {
            const auto at = value_at( json, key ); if ( !at || json[ *at ] != '"' ) return {};
            std::string out;
            for ( std::size_t i = *at + 1; i < json.size( ); ++i ) {
                const char ch = json[ i ]; if ( ch == '"' ) return out;
                if ( ch == '\\' && i + 1 < json.size( ) ) {
                    const char escaped = json[ ++i ];
                    if ( escaped == 'n' ) out.push_back( '\n' );
                    else if ( escaped == 'r' ) out.push_back( '\r' );
                    else if ( escaped == 't' ) out.push_back( '\t' );
                    else out.push_back( escaped );
                } else out.push_back( ch );
            }
            return {};
        }

        inline double number_value( const std::string& json, std::string_view key, double fallback = 0 ) {
            const auto at = value_at( json, key ); if ( !at ) return fallback;
            char* end = nullptr; const double value = std::strtod( json.c_str( ) + *at, &end );
            return end == json.c_str( ) + *at ? fallback : value;
        }

        inline bool bool_value( const std::string& json, std::string_view key, bool fallback = false ) {
            const auto at = value_at( json, key ); if ( !at ) return fallback;
            if ( json.compare( *at, 4, "true" ) == 0 ) return true;
            if ( json.compare( *at, 5, "false" ) == 0 ) return false;
            return fallback;
        }

        inline std::vector< int > int_array( const std::string& json, std::string_view key ) {
            std::vector< int > values; const auto at = value_at( json, key );
            if ( !at || json[ *at ] != '[' ) return values;
            std::size_t pos = *at + 1;
            while ( pos < json.size( ) && json[ pos ] != ']' ) {
                while ( pos < json.size( ) && ( std::isspace( static_cast< unsigned char >( json[ pos ] ) ) || json[ pos ] == ',' ) ) ++pos;
                char* end = nullptr; const long value = std::strtol( json.c_str( ) + pos, &end, 10 );
                if ( end == json.c_str( ) + pos ) break;
                values.push_back( static_cast< int >( value ) ); pos = static_cast< std::size_t >( end - json.c_str( ) );
            }
            return values;
        }

        inline std::vector< std::string > string_array( const std::string& json, std::string_view key ) {
            std::vector< std::string > values; const auto at = value_at( json, key );
            if ( !at || json[ *at ] != '[' ) return values;
            std::size_t pos = *at + 1;
            while ( pos < json.size( ) && json[ pos ] != ']' ) {
                while ( pos < json.size( ) && json[ pos ] != '"' && json[ pos ] != ']' ) ++pos;
                if ( pos >= json.size( ) || json[ pos ] == ']' ) break;
                std::string item;
                for ( ++pos; pos < json.size( ); ++pos ) {
                    if ( json[ pos ] == '"' ) { ++pos; break; }
                    if ( json[ pos ] == '\\' && pos + 1 < json.size( ) ) item.push_back( json[ ++pos ] );
                    else item.push_back( json[ pos ] );
                }
                values.push_back( std::move( item ) );
            }
            return values;
        }

        inline std::string hex( const unsigned char* bytes, std::size_t size ) {
            constexpr char digits[] = "0123456789abcdef"; std::string result( size * 2, '0' );
            for ( std::size_t i = 0; i < size; ++i ) { result[ i * 2 ] = digits[ bytes[ i ] >> 4 ]; result[ i * 2 + 1 ] = digits[ bytes[ i ] & 15 ]; }
            return result;
        }

        inline std::string sha256( const std::string& input ) {
            BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
            DWORD object_size = 0, result_size = 0; std::vector< unsigned char > object; std::array< unsigned char, 32 > digest {};
            if ( BCryptOpenAlgorithmProvider( &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0 ) < 0 ) return {};
            if ( BCryptGetProperty( algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast< PUCHAR >( &object_size ), sizeof( object_size ), &result_size, 0 ) < 0 ) { BCryptCloseAlgorithmProvider( algorithm, 0 ); return {}; }
            object.resize( object_size );
            if ( BCryptCreateHash( algorithm, &hash, object.data( ), object_size, nullptr, 0, 0 ) < 0
                || BCryptHashData( hash, reinterpret_cast< PUCHAR >( const_cast< char* >( input.data( ) ) ), static_cast< ULONG >( input.size( ) ), 0 ) < 0
                || BCryptFinishHash( hash, digest.data( ), static_cast< ULONG >( digest.size( ) ), 0 ) < 0 ) {
                if ( hash ) BCryptDestroyHash( hash ); BCryptCloseAlgorithmProvider( algorithm, 0 ); return {};
            }
            BCryptDestroyHash( hash ); BCryptCloseAlgorithmProvider( algorithm, 0 ); return hex( digest.data( ), digest.size( ) );
        }

        inline std::string guid( ) {
            GUID value {}; if ( FAILED( CoCreateGuid( &value ) ) ) return {};
            char buffer[ 40 ] {};
            std::snprintf( buffer, sizeof( buffer ), "%08lx-%04x-%04x-%04x-%012llx",
                value.Data1, value.Data2, value.Data3,
                static_cast< unsigned >( value.Data4[ 0 ] << 8 | value.Data4[ 1 ] ),
                ( static_cast< unsigned long long >( value.Data4[ 2 ] ) << 40 ) |
                ( static_cast< unsigned long long >( value.Data4[ 3 ] ) << 32 ) |
                ( static_cast< unsigned long long >( value.Data4[ 4 ] ) << 24 ) |
                ( static_cast< unsigned long long >( value.Data4[ 5 ] ) << 16 ) |
                ( static_cast< unsigned long long >( value.Data4[ 6 ] ) << 8 ) | value.Data4[ 7 ] );
            return buffer;
        }

        inline bool winsock_ready( ) {
            static const bool ready = [ ]( ) {
                WSADATA data{};
                return WSAStartup( MAKEWORD( 2, 2 ), &data ) == 0;
            }( );
            return ready;
        }

        inline std::uint16_t native_port( ) {
            const std::string value = env( "CRISIS_AUTH_PORT" );
            if ( value.empty( ) ) return 223;
            try { const auto port = std::stoul( value ); return port && port <= 65535 ? static_cast< std::uint16_t >( port ) : 223; }
            catch ( ... ) { return 223; }
        }

        inline std::string native_host( ) {
            std::string host = env( "CRISIS_AUTH_HOST" );
            if ( !host.empty( ) ) return host;
            return "wauth.aaddpp.lol";
        }

        inline std::string op_from_path( const std::string& path ) {
            std::string value = path;
            const std::string prefix = "/v1/player";
            if ( value.rfind( prefix, 0 ) == 0 ) value = value.substr( prefix.size( ) );
            while ( !value.empty( ) && value.front( ) == '/' ) value.erase( value.begin( ) );
            if ( value.empty( ) ) return "player";
            for ( auto& ch : value ) if ( ch == '/' ) ch = '.';
            if ( value.rfind( "casino.", 0 ) == 0 ) return value;
            return std::string( "player." ) + value;
        }

        inline bool tcp_json( const std::string& json, std::string& response, std::string& error ) {
            response.clear( );
            if ( !winsock_ready( ) ) { error = "Unable to initialize sockets"; return false; }
            const std::string host = native_host( );
            const auto port = native_port( );
            ADDRINFOA hints{};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            PADDRINFOA addresses = nullptr;
            if ( getaddrinfo( host.c_str( ), std::to_string( port ).c_str( ), &hints, &addresses ) != 0 || !addresses ) {
                error = "Unable to resolve authentication server";
                return false;
            }
            SOCKET socket = INVALID_SOCKET;
            for ( auto* entry = addresses; entry; entry = entry->ai_next ) {
                socket = ::socket( entry->ai_family, entry->ai_socktype, entry->ai_protocol );
                if ( socket == INVALID_SOCKET ) continue;
                DWORD timeout = 7000;
                setsockopt( socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast< const char* >( &timeout ), sizeof( timeout ) );
                setsockopt( socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast< const char* >( &timeout ), sizeof( timeout ) );
                if ( connect( socket, entry->ai_addr, static_cast< int >( entry->ai_addrlen ) ) == 0 ) break;
                closesocket( socket );
                socket = INVALID_SOCKET;
            }
            freeaddrinfo( addresses );
            if ( socket == INVALID_SOCKET ) { error = "Unable to connect to authentication server"; return false; }

            auto write_u32 = [ ]( std::uint8_t* dest, std::uint32_t value ) {
                dest[ 0 ] = static_cast< std::uint8_t >( value );
                dest[ 1 ] = static_cast< std::uint8_t >( value >> 8 );
                dest[ 2 ] = static_cast< std::uint8_t >( value >> 16 );
                dest[ 3 ] = static_cast< std::uint8_t >( value >> 24 );
            };
            auto read_u32 = [ ]( const std::uint8_t* src ) {
                return static_cast< std::uint32_t >( src[ 0 ] ) | ( static_cast< std::uint32_t >( src[ 1 ] ) << 8 ) |
                    ( static_cast< std::uint32_t >( src[ 2 ] ) << 16 ) | ( static_cast< std::uint32_t >( src[ 3 ] ) << 24 );
            };
            auto send_all = [ & ]( const std::uint8_t* data, std::size_t size ) {
                while ( size ) {
                    const int sent = send( socket, reinterpret_cast< const char* >( data ), static_cast< int >( ( std::min )( size, std::size_t { 65536 } ) ), 0 );
                    if ( sent <= 0 ) return false;
                    data += sent; size -= static_cast< std::size_t >( sent );
                }
                return true;
            };
            auto recv_exact = [ & ]( std::uint8_t* data, std::size_t size ) {
                while ( size ) {
                    const int received = recv( socket, reinterpret_cast< char* >( data ), static_cast< int >( ( std::min )( size, std::size_t { 65536 } ) ), 0 );
                    if ( received <= 0 ) return false;
                    data += received; size -= static_cast< std::size_t >( received );
                }
                return true;
            };

            std::array< std::uint8_t, 16 > header{};
            write_u32( header.data( ), 0x54554143 );
            header[ 4 ] = 1;
            write_u32( header.data( ) + 8, static_cast< std::uint32_t >( json.size( ) ) );
            const bool sent = send_all( header.data( ), header.size( ) ) &&
                send_all( reinterpret_cast< const std::uint8_t* >( json.data( ) ), json.size( ) ) &&
                recv_exact( header.data( ), header.size( ) );
            if ( !sent || read_u32( header.data( ) ) != 0x54554143 || header[ 4 ] != 1 ) {
                closesocket( socket );
                error = "Authentication server request failed";
                return false;
            }
            const auto json_len = read_u32( header.data( ) + 8 );
            const auto binary_len = read_u32( header.data( ) + 12 );
            if ( !json_len || json_len > 64 * 1024 || binary_len ) {
                closesocket( socket );
                error = "Authentication response was too large";
                return false;
            }
            response.resize( json_len );
            const bool ok = recv_exact( reinterpret_cast< std::uint8_t* >( response.data( ) ), json_len );
            closesocket( socket );
            if ( !ok ) { response.clear( ); error = "Failed to read authentication response"; return false; }
            return true;
        }
    }

    class client_t {
    public:
        bool configure_from_environment( ) {
            player_token_ = detail::env( "CRISIS_PLAYER_TOKEN" );
            client_seed_ = detail::guid( );
            configured_ = detail::valid_access_token( player_token_ ) && !client_seed_.empty( );
            return configured_;
        }

        void configure( std::string, std::string player_token ) {
            player_token_ = std::move( player_token );
            client_seed_ = detail::guid( );
            configured_ = detail::valid_access_token( player_token_ ) && !client_seed_.empty( );
        }

        [[nodiscard]] bool configured( ) const { return configured_; }
        [[nodiscard]] bool ready( ) const { return configured_ && !expected_commitment_.empty( ); }
        [[nodiscard]] std::uint64_t balance( ) const { return balance_; }
        [[nodiscard]] const std::string& last_error( ) const { return last_error_; }
        [[nodiscard]] std::optional< result_t > active_round( std::string_view kind ) const {
            const auto active = active_rounds_.find( std::string( kind ) );
            return active == active_rounds_.end( ) ? std::nullopt : std::optional< result_t >( active->second );
        }

        result_t state( ) { return request( L"GET", "/v1/player/casino/state", {} ); }
        result_t roulette( std::uint64_t bet, std::string_view choice ) { return wager( "/v1/player/casino/roulette", bet, ",\"choice\":\"" + detail::json_escape( choice ) + "\"" ); }
        result_t crash( std::uint64_t bet, double target ) { return wager( "/v1/player/casino/crash", bet, ",\"target\":" + number( target ) ); }
        result_t crate( std::uint64_t cost = 15 ) { return wager( "/v1/player/casino/crate", cost, {} ); }
        result_t mines_start( std::uint64_t bet, int mines ) { return wager( "/v1/player/casino/mines/start", bet, ",\"mines\":" + std::to_string( mines ) ); }
        result_t mines_reveal( int tile ) { return action( "/v1/player/casino/mines/reveal", ",\"tile\":" + std::to_string( tile ) ); }
        result_t mines_cashout( ) { return action( "/v1/player/casino/mines/cashout", {} ); }
        result_t chicken_start( std::uint64_t bet, std::string_view risk ) { return wager( "/v1/player/casino/chicken/start", bet, ",\"risk\":\"" + detail::json_escape( risk ) + "\"" ); }
        result_t chicken_jump( ) { return action( "/v1/player/casino/chicken/jump", {} ); }
        result_t chicken_cashout( ) { return action( "/v1/player/casino/chicken/cashout", {} ); }
        result_t blackjack_start( std::uint64_t bet ) { return wager( "/v1/player/casino/blackjack/start", bet, {} ); }
        result_t blackjack_action( std::string_view value ) { return action( "/v1/player/casino/blackjack/action", ",\"action\":\"" + detail::json_escape( value ) + "\"" ); }

    private:
        static std::string number( double value ) { char data[ 32 ] {}; std::snprintf( data, sizeof( data ), "%.2f", value ); return data; }
        static result_t parse_round( const std::string& round, std::uint64_t balance ) {
            result_t result;
            const std::string proof = detail::object_value( round, "proof" );
            result.success = true;
            result.balance = balance;
            result.status = detail::string_value( round, "status" );
            result.message = detail::string_value( round, "message" );
            result.color = detail::string_value( round, "color" );
            result.commitment = detail::string_value( proof, "commitment" );
            result.next_commitment = detail::string_value( proof, "next_commitment" );
            result.server_seed = detail::string_value( proof, "server_seed" );
            result.kind = detail::string_value( round, "kind" );
            result.feature = detail::string_value( round, "feature" );
            result.risk = detail::string_value( round, "risk" );
            result.payout = static_cast< std::uint64_t >( ( std::max )( 0.0, detail::number_value( round, "payout" ) ) );
            result.bet = static_cast< std::uint64_t >( ( std::max )( 0.0, detail::number_value( round, "bet" ) ) );
            result.number = static_cast< int >( detail::number_value( round, "number", -1 ) );
            result.crossed = static_cast< int >( detail::number_value( round, "crossed" ) );
            result.mines = static_cast< int >( detail::number_value( round, "mines" ) );
            result.multiplier = detail::number_value( round, "multiplier", 1 );
            result.crash_point = detail::number_value( round, "crash_point", 1 );
            result.won = detail::bool_value( round, "won" );
            result.caught = detail::bool_value( round, "caught" );
            result.revealed = detail::int_array( round, "revealed" );
            result.mine_positions = detail::int_array( round, "mine_positions" );
            result.player_cards = detail::string_array( round, "player_cards" );
            result.dealer_cards = detail::string_array( round, "dealer_cards" );
            return result;
        }
        result_t wager( const std::string& path, std::uint64_t bet, const std::string& extra ) {
            return request( L"POST", path, "{\"bet\":" + std::to_string( bet ) + ",\"client_seed\":\"" + detail::json_escape( client_seed_ )
                + "\",\"request_id\":\"" + detail::guid( ) + "\"" + extra + "}" );
        }
        result_t action( const std::string& path, const std::string& extra ) {
            return request( L"POST", path, "{\"request_id\":\"" + detail::guid( ) + "\"" + extra + "}" );
        }

        result_t request( const wchar_t*, const std::string& path, const std::string& body ) {
            result_t result;
            if ( !configured_ ) { result.error = "Casino authentication is not configured"; last_error_ = result.error; return result; }
            const std::string op = detail::op_from_path( path );
            std::string payload = body.empty( )
                ? std::string( "{\"op\":\"" ) + detail::json_escape( op ) + "\",\"player_token\":\"" + detail::json_escape( player_token_ ) + "\"}"
                : ( body.front( ) == '{'
                    ? std::string( "{\"op\":\"" ) + detail::json_escape( op ) + "\",\"player_token\":\"" + detail::json_escape( player_token_ ) + "\"," + body.substr( 1 )
                    : body );
            std::string error;
            if ( !detail::tcp_json( payload, result.raw, error ) ) { result.error = error; last_error_ = result.error; return result; }
            result.transport_ok = true;
            result.status_code = static_cast< unsigned long >( ( std::max )( 0.0, detail::number_value( result.raw, "status", 200 ) ) );
            result.success = result.status_code >= 200 && result.status_code < 300 && detail::bool_value( result.raw, "success" );
            result.error = detail::string_value( result.raw, "error" );
            result.balance = static_cast< std::uint64_t >( ( std::max )( 0.0, detail::number_value( result.raw, "balance" ) ) );

            const std::string game = detail::object_value( result.raw, "game" );
            const std::string fairness = detail::object_value( result.raw, "fairness" );
            if ( game.empty( ) ) {
                result.commitment = detail::string_value( fairness, "commitment" );
            } else {
                const result_t round = parse_round( game, result.balance );
                result.status = round.status;
                result.message = round.message;
                result.color = round.color;
                result.commitment = round.commitment;
                result.next_commitment = round.next_commitment;
                result.server_seed = round.server_seed;
                result.kind = round.kind;
                result.feature = round.feature;
                result.risk = round.risk;
                result.payout = round.payout;
                result.bet = round.bet;
                result.number = round.number;
                result.crossed = round.crossed;
                result.mines = round.mines;
                result.multiplier = round.multiplier;
                result.crash_point = round.crash_point;
                result.won = round.won;
                result.caught = round.caught;
                result.revealed = round.revealed;
                result.mine_positions = round.mine_positions;
                result.player_cards = round.player_cards;
                result.dealer_cards = round.dealer_cards;
            }

            if ( game.empty( ) ) {
                result.proof_verified = result.success && !result.commitment.empty( );
                if ( result.proof_verified ) {
                    expected_commitment_ = result.commitment;
                    balance_ = result.balance;
                    active_commitments_.clear( );
                    active_rounds_.clear( );
                    const std::string active_games = detail::object_value( result.raw, "active" );
                    static constexpr const char* kinds[ ] = { "mines", "chicken", "blackjack" };
                    for ( const char* kind : kinds ) {
                        const std::string active_json = detail::object_value( active_games, kind );
                        if ( active_json.empty( ) ) continue;
                        result_t active_round = parse_round( active_json, result.balance );
                        if ( active_round.kind != kind || active_round.status != "playing" || active_round.commitment.empty( ) ) continue;
                        active_round.proof_verified = true;
                        active_commitments_[ active_round.kind ] = active_round.commitment;
                        active_rounds_[ active_round.kind ] = std::move( active_round );
                    }
                }
            } else {
                const auto active = active_commitments_.find( result.kind );
                const std::string committed_before = active != active_commitments_.end( ) ? active->second : expected_commitment_;
                const bool commitment_matches = !result.commitment.empty( ) && !committed_before.empty( ) && result.commitment == committed_before;
                const bool reveal_matches = result.server_seed.empty( ) || detail::sha256( result.server_seed ) == result.commitment;
                result.proof_verified = result.success && !result.kind.empty( ) && commitment_matches && reveal_matches;
                if ( result.proof_verified ) {
                    balance_ = result.balance;
                    if ( result.status == "playing" ) {
                        if ( active == active_commitments_.end( ) ) active_commitments_[ result.kind ] = result.commitment;
                        active_rounds_[ result.kind ] = result;
                    } else if ( result.settled( ) ) {
                        active_commitments_.erase( result.kind );
                        active_rounds_.erase( result.kind );
                    }
                    if ( !result.next_commitment.empty( ) ) expected_commitment_ = result.next_commitment;
                } else if ( result.success && result.error.empty( ) ) {
                    result.error = "Casino fairness proof verification failed";
                }
            }
            if ( !result.success && result.error.empty( ) ) result.error = "Casino server rejected the request";
            last_error_ = result.error;
            return result;
        }

        bool configured_ = false;
        std::string player_token_, client_seed_, last_error_, expected_commitment_;
        std::unordered_map< std::string, std::string > active_commitments_;
        std::unordered_map< std::string, result_t > active_rounds_;
        std::uint64_t balance_ = 0;
    };
}
