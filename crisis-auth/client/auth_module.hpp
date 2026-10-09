#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <ncrypt.h>
#include <bcrypt.h>
#include <vector>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <iostream>
#include <memory>
#include <format>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "bcrypt.lib")

namespace loader {

    static void derive_xor_key( const std::string& access_token, const std::string& hwid, std::vector<std::uint8_t>& out_key ) {
        const std::string seed = access_token + hwid;
        std::int32_t hash = 0;
        for ( size_t i = 0; i < seed.length( ); i++ ) {
            hash = ( ( hash << 5 ) - hash ) + static_cast<std::int8_t>( seed[ i ] );
            hash |= 0;
        }
        out_key.resize( 256 );
        for ( int i = 0; i < 256; i++ ) {
            out_key[ i ] = static_cast<std::uint8_t>( ( hash ^ ( i * 0x9E ) ) & 0xFF );
            hash = ( ( hash * 1103515245 + 12345 ) >> 0 );
        }
    }

    static void xor_buffer( std::vector<std::uint8_t>& buffer, const std::vector<std::uint8_t>& xor_key ) {
        for ( size_t i = 0; i < buffer.size( ); i++ ) {
            buffer[ i ] ^= xor_key[ i % xor_key.size( ) ];
        }
    }

    class device_key_t {
    public:
        ~device_key_t( ) {
            if ( key_ ) NCryptFreeObject( key_ );
            if ( provider_ ) NCryptFreeObject( provider_ );
        }

        device_key_t( const device_key_t& ) = delete;
        device_key_t& operator=( const device_key_t& ) = delete;
        device_key_t( ) = default;

        bool initialize( ) {
            constexpr const wchar_t* key_name = L"CrisisLoaderDeviceKeyV1";
            NCRYPT_PROV_HANDLE provider = 0;
            if ( NCryptOpenStorageProvider( &provider, MS_KEY_STORAGE_PROVIDER, 0 ) != ERROR_SUCCESS ) return false;
            NCRYPT_KEY_HANDLE key = 0;
            SECURITY_STATUS status = NCryptOpenKey( provider, &key, key_name, 0, 0 );
            if ( status != ERROR_SUCCESS ) {
                status = NCryptCreatePersistedKey( provider, &key, BCRYPT_ECDSA_P256_ALGORITHM, key_name, 0, 0 );
                if ( status == ERROR_SUCCESS ) status = NCryptFinalizeKey( key, 0 );
            }
            if ( status != ERROR_SUCCESS ) {
                if ( key ) NCryptFreeObject( key );
                NCryptFreeObject( provider );
                return false;
            }
            provider_ = provider;
            key_ = key;
            return export_public_key( );
        }

        const std::vector<std::uint8_t>& public_blob( ) const { return public_blob_; }

        bool sign( const std::string& message, std::vector<std::uint8_t>& signature ) const {
            std::array<std::uint8_t, 32> hash = { };
            if ( !sha256( message, hash ) ) return false;
            DWORD required = 0;
            if ( NCryptSignHash( key_, nullptr, const_cast<PBYTE>( hash.data( ) ), static_cast<DWORD>( hash.size( ) ), nullptr, 0, &required, 0 ) != ERROR_SUCCESS || required != 64 ) return false;
            signature.resize( required );
            return NCryptSignHash( key_, nullptr, const_cast<PBYTE>( hash.data( ) ), static_cast<DWORD>( hash.size( ) ), signature.data( ), required, &required, 0 ) == ERROR_SUCCESS &&
                ( signature.resize( required ), required == 64 );
        }

    private:
        static bool sha256( const std::string& message, std::array<std::uint8_t, 32>& output ) {
            BCRYPT_ALG_HANDLE algorithm = nullptr;
            BCRYPT_HASH_HANDLE hash = nullptr;
            DWORD object_size = 0, copied = 0;
            if ( BCryptOpenAlgorithmProvider( &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0 ) < 0 ||
                 BCryptGetProperty( algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>( &object_size ), sizeof( object_size ), &copied, 0 ) < 0 ) {
                if ( algorithm ) BCryptCloseAlgorithmProvider( algorithm, 0 );
                return false;
            }
            std::vector<std::uint8_t> object( object_size );
            const bool ok = BCryptCreateHash( algorithm, &hash, object.data( ), object_size, nullptr, 0, 0 ) >= 0 &&
                BCryptHashData( hash, reinterpret_cast<PUCHAR>( const_cast<char*>( message.data( ) ) ), static_cast<ULONG>( message.size( ) ), 0 ) >= 0 &&
                BCryptFinishHash( hash, output.data( ), static_cast<ULONG>( output.size( ) ), 0 ) >= 0;
            if ( hash ) BCryptDestroyHash( hash );
            BCryptCloseAlgorithmProvider( algorithm, 0 );
            return ok;
        }

        bool export_public_key( ) {
            DWORD required = 0;
            if ( NCryptExportKey( key_, 0, BCRYPT_ECCPUBLIC_BLOB, nullptr, nullptr, 0, &required, 0 ) != ERROR_SUCCESS || required != 72 ) return false;
            public_blob_.resize( required );
            return NCryptExportKey( key_, 0, BCRYPT_ECCPUBLIC_BLOB, nullptr, public_blob_.data( ), required, &required, 0 ) == ERROR_SUCCESS &&
                ( public_blob_.resize( required ), required == 72 );
        }

        NCRYPT_PROV_HANDLE provider_ = 0;
        NCRYPT_KEY_HANDLE key_ = 0;
        std::vector<std::uint8_t> public_blob_;
    };

    static std::string base64url_encode( const std::uint8_t* data, size_t size ) {
        static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        std::string output;
        output.reserve( ( size * 4 + 2 ) / 3 );
        for ( size_t index = 0; index < size; index += 3 ) {
            const std::uint32_t value = ( static_cast<std::uint32_t>( data[ index ] ) << 16 ) |
                ( index + 1 < size ? static_cast<std::uint32_t>( data[ index + 1 ] ) << 8 : 0 ) |
                ( index + 2 < size ? data[ index + 2 ] : 0 );
            output.push_back( alphabet[ ( value >> 18 ) & 63 ] );
            output.push_back( alphabet[ ( value >> 12 ) & 63 ] );
            if ( index + 1 < size ) output.push_back( alphabet[ ( value >> 6 ) & 63 ] );
            if ( index + 2 < size ) output.push_back( alphabet[ value & 63 ] );
        }
        return output;
    }

    static bool signed_proof( const device_key_t& key, const std::string& prefix, const std::string& suffix, std::string& nonce, std::string& signature ) {
        std::array<std::uint8_t, 32> nonce_bytes = { };
        if ( BCryptGenRandom( nullptr, nonce_bytes.data( ), static_cast<ULONG>( nonce_bytes.size( ) ), BCRYPT_USE_SYSTEM_PREFERRED_RNG ) < 0 ) return false;
        nonce = base64url_encode( nonce_bytes.data( ), nonce_bytes.size( ) );
        std::vector<std::uint8_t> raw_signature;
        if ( !key.sign( prefix + nonce + suffix, raw_signature ) ) return false;
        signature = base64url_encode( raw_signature.data( ), raw_signature.size( ) );
        return true;
    }

    static bool winsock_ready( ) {
        static const bool ready = [ ]( ) {
            WSADATA data = { };
            return WSAStartup( MAKEWORD( 2, 2 ), &data ) == 0;
        }( );
        return ready;
    }

    static bool parse_endpoint( const std::string& endpoint, std::string& host, std::uint16_t& port ) {
        auto split = endpoint.rfind( ':' );
        if ( split == std::string::npos || split == 0 ) {
            host = endpoint;
            port = 223;
            return !host.empty( );
        }
        host = endpoint.substr( 0, split );
        try { port = static_cast<std::uint16_t>( std::stoul( endpoint.substr( split + 1 ) ) ); }
        catch ( ... ) { return false; }
        return !host.empty( ) && port != 0;
    }

    static bool tcp_send_all( SOCKET socket, const std::uint8_t* data, size_t size ) {
        while ( size ) {
            const int sent = send( socket, reinterpret_cast<const char*>( data ), static_cast<int>( ( std::min )( size, size_t { 65536 } ) ), 0 );
            if ( sent <= 0 ) return false;
            data += sent;
            size -= static_cast<size_t>( sent );
        }
        return true;
    }

    static bool tcp_recv_exact( SOCKET socket, std::uint8_t* data, size_t size ) {
        while ( size ) {
            const int received = recv( socket, reinterpret_cast<char*>( data ), static_cast<int>( ( std::min )( size, size_t { 65536 } ) ), 0 );
            if ( received <= 0 ) return false;
            data += received;
            size -= static_cast<size_t>( received );
        }
        return true;
    }

    static void write_u32le( std::uint8_t* dest, std::uint32_t value ) {
        dest[ 0 ] = static_cast<std::uint8_t>( value );
        dest[ 1 ] = static_cast<std::uint8_t>( value >> 8 );
        dest[ 2 ] = static_cast<std::uint8_t>( value >> 16 );
        dest[ 3 ] = static_cast<std::uint8_t>( value >> 24 );
    }

    static std::uint32_t read_u32le( const std::uint8_t* src ) {
        return static_cast<std::uint32_t>( src[ 0 ] ) | ( static_cast<std::uint32_t>( src[ 1 ] ) << 8 ) |
            ( static_cast<std::uint32_t>( src[ 2 ] ) << 16 ) | ( static_cast<std::uint32_t>( src[ 3 ] ) << 24 );
    }

    static bool native_exchange( const std::string& endpoint, const std::string& json, std::string& response_json, std::vector<std::uint8_t>& response_binary ) {
        response_json.clear( );
        response_binary.clear( );
        std::string host;
        std::uint16_t port = 223;
        if ( !parse_endpoint( endpoint, host, port ) || !winsock_ready( ) ) return false;

        ADDRINFOA hints = { };
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        PADDRINFOA addresses = nullptr;
        if ( getaddrinfo( host.c_str( ), std::to_string( port ).c_str( ), &hints, &addresses ) != 0 || !addresses ) return false;
        SOCKET socket = INVALID_SOCKET;
        for ( auto* entry = addresses; entry; entry = entry->ai_next ) {
            socket = ::socket( entry->ai_family, entry->ai_socktype, entry->ai_protocol );
            if ( socket == INVALID_SOCKET ) continue;
            DWORD timeout = 15000;
            setsockopt( socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>( &timeout ), sizeof( timeout ) );
            setsockopt( socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>( &timeout ), sizeof( timeout ) );
            if ( connect( socket, entry->ai_addr, static_cast<int>( entry->ai_addrlen ) ) == 0 ) break;
            closesocket( socket );
            socket = INVALID_SOCKET;
        }
        freeaddrinfo( addresses );
        if ( socket == INVALID_SOCKET ) return false;

        std::array<std::uint8_t, 16> header = { };
        write_u32le( header.data( ), 0x54554143 );
        header[ 4 ] = 1;
        write_u32le( header.data( ) + 8, static_cast<std::uint32_t>( json.size( ) ) );
        const bool sent = tcp_send_all( socket, header.data( ), header.size( ) ) &&
            tcp_send_all( socket, reinterpret_cast<const std::uint8_t*>( json.data( ) ), json.size( ) ) &&
            tcp_recv_exact( socket, header.data( ), header.size( ) );
        if ( !sent || read_u32le( header.data( ) ) != 0x54554143 || header[ 4 ] != 1 ) { closesocket( socket ); return false; }
        const auto json_len = read_u32le( header.data( ) + 8 );
        const auto binary_len = read_u32le( header.data( ) + 12 );
        if ( !json_len || json_len > 64 * 1024 || binary_len > 64ull * 1024 * 1024 ) { closesocket( socket ); return false; }
        response_json.resize( json_len );
        bool ok = tcp_recv_exact( socket, reinterpret_cast<std::uint8_t*>( response_json.data( ) ), json_len );
        if ( ok && binary_len ) {
            response_binary.resize( binary_len );
            ok = tcp_recv_exact( socket, response_binary.data( ), binary_len );
        }
        closesocket( socket );
        return ok;
    }

    static bool parse_json_bool( const std::string& json, const std::string& key, bool& out_value ) {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find( search );
        if ( pos == std::string::npos ) return false;
        pos = json.find( ':', pos + search.length( ) );
        if ( pos == std::string::npos ) return false;
        ++pos;
        while ( pos < json.size( ) && ( json[ pos ] == ' ' || json[ pos ] == '\t' || json[ pos ] == '\n' || json[ pos ] == '\r' ) ) ++pos;
        if ( pos + 4 <= json.size( ) && json.compare( pos, 4, "true" ) == 0 ) { out_value = true; return true; }
        if ( pos + 5 <= json.size( ) && json.compare( pos, 5, "false" ) == 0 ) { out_value = false; return true; }
        return false;
    }

    static bool parse_json_string( const std::string& json, const std::string& key, std::string& out_value ) {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find( search );
        if ( pos == std::string::npos ) return false;
        pos = json.find( ':', pos + search.length( ) );
        if ( pos == std::string::npos ) return false;
        ++pos;
        while ( pos < json.size( ) && ( json[ pos ] == ' ' || json[ pos ] == '\t' || json[ pos ] == '\n' || json[ pos ] == '\r' ) ) ++pos;
        if ( pos >= json.size( ) || json[ pos ] != '"' ) return false;
        ++pos;
        size_t end = pos;
        while ( end < json.size( ) && json[ end ] != '"' ) ++end;
        out_value = json.substr( pos, end - pos );
        return true;
    }

    static bool parse_json_uint64( const std::string& json, const std::string& key, std::uint64_t& out_value ) {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find( search );
        if ( pos == std::string::npos ) return false;
        pos = json.find( ':', pos + search.length( ) );
        if ( pos == std::string::npos ) return false;
        ++pos;
        while ( pos < json.size( ) && ( json[ pos ] == ' ' || json[ pos ] == '\t' || json[ pos ] == '\n' || json[ pos ] == '\r' ) ) ++pos;
        if ( pos >= json.size( ) ) return false;
        bool is_string = json[ pos ] == '"';
        if ( is_string ) ++pos;
        size_t end = pos;
        while ( end < json.size( ) && ( ( json[ end ] >= '0' && json[ end ] <= '9' ) || json[ end ] == '.' ) ) ++end;
        if ( pos == end ) return false;
        try { out_value = std::stoull( json.substr( pos, end - pos ) ); return true; }
        catch ( ... ) { return false; }
    }

    static std::string json_escape( const std::string& value ) {
        std::string escaped;
        escaped.reserve( value.size( ) );
        for ( unsigned char character : value ) {
            switch ( character ) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if ( character < 0x20 ) {
                    char encoded[ 7 ] = { };
                    std::snprintf( encoded, sizeof( encoded ), "\\u%04x", character );
                    escaped += encoded;
                } else {
                    escaped.push_back( static_cast<char>( character ) );
                }
            }
        }
        return escaped;
    }

    static std::string url_encode_component( const std::string& value ) {
        std::ostringstream encoded;
        encoded << std::uppercase << std::hex;
        for ( unsigned char character : value ) {
            if ( ( character >= 'a' && character <= 'z' ) || ( character >= 'A' && character <= 'Z' ) ||
                 ( character >= '0' && character <= '9' ) || character == '-' || character == '_' || character == '.' ) {
                encoded << static_cast<char>( character );
            } else {
                encoded << '%' << std::setw( 2 ) << std::setfill( '0' ) << static_cast<int>( character );
            }
        }
        return encoded.str( );
    }

    static bool native_json( const std::string& endpoint, const std::string& body, std::string& out_response ) {
        std::vector<std::uint8_t> unused;
        const bool ok = native_exchange( endpoint, body, out_response, unused );
        unused.clear( );
        return ok && !out_response.empty( );
    }

    struct module_result_t {
        bool success = false;
        std::string message;
        std::vector<std::uint8_t> decrypted_bytes;
    };

    inline module_result_t authenticate_and_download( const std::string& key, const std::string& hwid, const std::string& host = "127.0.0.1:223", const std::string& module_name = "test.dll" ) {
        module_result_t result = { };

        device_key_t device_key;
        if ( !device_key.initialize( ) ) {
            result.message = "Failed to initialize the device identity key.";
            return result;
        }
        const std::uint64_t auth_timestamp = static_cast<std::uint64_t>( std::chrono::duration_cast<std::chrono::seconds>( std::chrono::system_clock::now( ).time_since_epoch( ) ).count( ) );
        std::string auth_nonce, auth_signature;
        const std::string auth_prefix = "crisis-client-auth:v1\n" + std::to_string( auth_timestamp ) + "\n";
        const std::string auth_suffix = "\n" + hwid + "\n" + key;
        if ( !signed_proof( device_key, auth_prefix, auth_suffix, auth_nonce, auth_signature ) ) {
            result.message = "Failed to sign the authentication request with the device identity.";
            return result;
        }
        const std::string public_key = base64url_encode( device_key.public_blob( ).data( ), device_key.public_blob( ).size( ) );
        const std::string auth_body = "{\"op\":\"authenticate\",\"key\":\"" + json_escape( key ) + "\",\"hwid\":\"" + json_escape( hwid ) +
            "\",\"client_public_key\":\"" + public_key + "\",\"timestamp_unix\":" + std::to_string( auth_timestamp ) +
            ",\"nonce\":\"" + auth_nonce + "\",\"signature\":\"" + auth_signature + "\"}";
        std::string auth_response;
        if ( !native_json( host, auth_body, auth_response ) ) {
            result.message = "Failed to contact authentication server over TCP.";
            return result;
        }

        bool success = false;
        if ( !parse_json_bool( auth_response, "success", success ) || !success ) {
            result.message = "Authentication failed: invalid, paused, or expired license.";
            return result;
        }

        std::uint64_t start_unix = 0, expiry_unix = 0;
        std::string access_token;
        if ( !parse_json_uint64( auth_response, "start_unix", start_unix ) ||
             !parse_json_uint64( auth_response, "expiry_unix", expiry_unix ) ||
             !parse_json_string( auth_response, "access_token", access_token ) ) {
            result.message = "Invalid auth response format.";
            return result;
        }

        const std::uint64_t now = static_cast<std::uint64_t>( std::chrono::duration_cast<std::chrono::seconds>( std::chrono::system_clock::now( ).time_since_epoch( ) ).count( ) );
        if ( now >= expiry_unix ) {
            result.message = "License has expired.";
            return result;
        }

        const std::uint64_t duration = expiry_unix - start_unix;
        const std::uint64_t remaining = expiry_unix - now;
        std::cout << std::format( "[AUTH] Subscription valid. Duration: {}s | Remaining: {}s\n", duration, remaining );

        const std::uint64_t module_timestamp = static_cast<std::uint64_t>( std::chrono::duration_cast<std::chrono::seconds>( std::chrono::system_clock::now( ).time_since_epoch( ) ).count( ) );
        std::string module_nonce, module_signature;
        const std::string module_prefix = "crisis-artifact-proof:v1\nmodule\n" + module_name + "\n" + access_token + "\n" + std::to_string( module_timestamp ) + "\n";
        if ( !signed_proof( device_key, module_prefix, "", module_nonce, module_signature ) ) {
            result.message = "Failed to sign the module request with the device identity.";
            return result;
        }
        const std::string module_body = "{\"op\":\"module\",\"name\":\"" + json_escape( module_name ) +
            "\",\"access_token\":\"" + json_escape( access_token ) +
            "\",\"timestamp_unix\":" + std::to_string( module_timestamp ) +
            ",\"nonce\":\"" + module_nonce + "\",\"signature\":\"" + module_signature + "\"}";
        std::string module_response;
        std::vector<std::uint8_t> encrypted_data;
        if ( !native_exchange( host, module_body, module_response, encrypted_data ) ) {
            result.message = "Failed to download module with the authenticated session.";
            return result;
        }
        bool module_ok = false;
        if ( !parse_json_bool( module_response, "success", module_ok ) || !module_ok ) {
            result.message = "Failed to download module with the authenticated session.";
            return result;
        }

        std::cout << std::format( "[MODULE] Downloaded {} bytes (session-masked)\n", encrypted_data.size( ) );
        std::vector<std::uint8_t> xor_key;
        derive_xor_key( access_token, hwid, xor_key );
        xor_buffer( encrypted_data, xor_key );

        result.success = true;
        result.decrypted_bytes = std::move( encrypted_data );
        result.message = std::format( "Module ready ({} bytes)", result.decrypted_bytes.size( ) );
        return result;
    }

}
