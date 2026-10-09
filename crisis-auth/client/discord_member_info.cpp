#include "discord_member_info.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "winhttp.lib")

namespace discord_member_info {
namespace {

constexpr size_t kSha256Bytes = 32;
constexpr size_t kMaxHttpResponseBytes = 64 * 1024;
constexpr size_t kEcdhPublicBlobBytes = 72;
constexpr size_t kAesGcmIvBytes = 12;
constexpr size_t kAesGcmTagBytes = 16;
constexpr std::uint8_t kEnvelopeVersion = 1;
constexpr std::uint32_t kEcdhPublicP256Magic = BCRYPT_ECDH_PUBLIC_P256_MAGIC;
constexpr char kRequestEnvelopeAad[] = "discord-member-info/v2/request";
constexpr char kResponseEnvelopeAad[] = "discord-member-info/v2/response";

struct bcrypt_alg_deleter {
    void operator()( BCRYPT_ALG_HANDLE handle ) const {
        if ( handle ) BCryptCloseAlgorithmProvider( handle, 0 );
    }
};

struct bcrypt_hash_deleter {
    void operator()( BCRYPT_HASH_HANDLE handle ) const {
        if ( handle ) BCryptDestroyHash( handle );
    }
};

struct bcrypt_key_deleter {
    void operator()( BCRYPT_KEY_HANDLE handle ) const {
        if ( handle ) BCryptDestroyKey( handle );
    }
};

struct bcrypt_secret_deleter {
    void operator()( BCRYPT_SECRET_HANDLE handle ) const {
        if ( handle ) BCryptDestroySecret( handle );
    }
};

using bcrypt_alg_ptr = std::unique_ptr<std::remove_pointer_t<BCRYPT_ALG_HANDLE>, bcrypt_alg_deleter>;
using bcrypt_hash_ptr = std::unique_ptr<std::remove_pointer_t<BCRYPT_HASH_HANDLE>, bcrypt_hash_deleter>;
using bcrypt_key_ptr = std::unique_ptr<std::remove_pointer_t<BCRYPT_KEY_HANDLE>, bcrypt_key_deleter>;
using bcrypt_secret_ptr = std::unique_ptr<std::remove_pointer_t<BCRYPT_SECRET_HANDLE>, bcrypt_secret_deleter>;

bool is_decimal_snowflake( const std::string& value ) {
    return value.size( ) >= 17 && value.size( ) <= 20 &&
        std::all_of( value.begin( ), value.end( ), []( unsigned char ch ) { return ch >= '0' && ch <= '9'; } );
}

bool sha256( const std::string& input, std::array<std::uint8_t, kSha256Bytes>& output ) {
    BCRYPT_ALG_HANDLE raw_alg = nullptr;
    if ( BCryptOpenAlgorithmProvider( &raw_alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0 ) < 0 ) return false;
    bcrypt_alg_ptr alg( raw_alg );

    DWORD object_length = 0;
    DWORD result_length = 0;
    if ( BCryptGetProperty( alg.get( ), BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>( &object_length ), sizeof( object_length ), &result_length, 0 ) < 0 ) return false;
    std::vector<std::uint8_t> object( object_length );

    BCRYPT_HASH_HANDLE raw_hash = nullptr;
    if ( BCryptCreateHash( alg.get( ), &raw_hash, object.data( ), static_cast<ULONG>( object.size( ) ), nullptr, 0, 0 ) < 0 ) return false;
    bcrypt_hash_ptr hash( raw_hash );
    if ( BCryptHashData( hash.get( ), reinterpret_cast<PUCHAR>( const_cast<char*>( input.data( ) ) ), static_cast<ULONG>( input.size( ) ), 0 ) < 0 ) return false;
    return BCryptFinishHash( hash.get( ), output.data( ), static_cast<ULONG>( output.size( ) ), 0 ) >= 0;
}

bool hmac_sha256( const std::array<std::uint8_t, kSha256Bytes>& key, const std::string& input, std::array<std::uint8_t, kSha256Bytes>& output ) {
    BCRYPT_ALG_HANDLE raw_alg = nullptr;
    if ( BCryptOpenAlgorithmProvider( &raw_alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG ) < 0 ) return false;
    bcrypt_alg_ptr alg( raw_alg );

    DWORD object_length = 0;
    DWORD result_length = 0;
    if ( BCryptGetProperty( alg.get( ), BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>( &object_length ), sizeof( object_length ), &result_length, 0 ) < 0 ) return false;
    std::vector<std::uint8_t> object( object_length );

    BCRYPT_HASH_HANDLE raw_hash = nullptr;
    if ( BCryptCreateHash( alg.get( ), &raw_hash, object.data( ), static_cast<ULONG>( object.size( ) ), const_cast<PUCHAR>( key.data( ) ), static_cast<ULONG>( key.size( ) ), 0 ) < 0 ) return false;
    bcrypt_hash_ptr hash( raw_hash );
    if ( BCryptHashData( hash.get( ), reinterpret_cast<PUCHAR>( const_cast<char*>( input.data( ) ) ), static_cast<ULONG>( input.size( ) ), 0 ) < 0 ) return false;
    return BCryptFinishHash( hash.get( ), output.data( ), static_cast<ULONG>( output.size( ) ), 0 ) >= 0;
}

bool random_bytes( std::array<std::uint8_t, kSha256Bytes>& output ) {
    return BCryptGenRandom( nullptr, output.data( ), static_cast<ULONG>( output.size( ) ), BCRYPT_USE_SYSTEM_PREFERRED_RNG ) >= 0;
}

bool base64url_encode( const std::uint8_t* bytes, size_t length, std::string& output ) {
    DWORD required = 0;
    if ( !CryptBinaryToStringA( bytes, static_cast<DWORD>( length ), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &required ) || required == 0 ) return false;
    std::string base64( required, '\0' );
    DWORD written = required;
    if ( !CryptBinaryToStringA( bytes, static_cast<DWORD>( length ), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, base64.data( ), &written ) ) return false;
    base64.resize( written ? written - 1 : 0 );
    while ( !base64.empty( ) && base64.back( ) == '=' ) base64.pop_back( );
    std::replace( base64.begin( ), base64.end( ), '+', '-' );
    std::replace( base64.begin( ), base64.end( ), '/', '_' );
    output = std::move( base64 );
    return true;
}

bool base64url_decode( const std::string& input, std::vector<std::uint8_t>& output ) {
    if ( input.empty( ) || input.find( '=' ) != std::string::npos ||
        !std::all_of( input.begin( ), input.end( ), []( unsigned char ch ) { return std::isalnum( ch ) || ch == '-' || ch == '_'; } ) ) return false;

    std::string base64 = input;
    std::replace( base64.begin( ), base64.end( ), '-', '+' );
    std::replace( base64.begin( ), base64.end( ), '_', '/' );
    while ( base64.size( ) % 4 != 0 ) base64.push_back( '=' );

    DWORD required = 0;
    if ( !CryptStringToBinaryA( base64.c_str( ), static_cast<DWORD>( base64.size( ) ), CRYPT_STRING_BASE64, nullptr, &required, nullptr, nullptr ) || required == 0 ) return false;
    output.resize( required );
    if ( !CryptStringToBinaryA( base64.c_str( ), static_cast<DWORD>( base64.size( ) ), CRYPT_STRING_BASE64, output.data( ), &required, nullptr, nullptr ) ) return false;
    output.resize( required );

    std::string canonical;
    return base64url_encode( output.data( ), output.size( ), canonical ) && canonical == input;
}

std::string with_zeroes( std::initializer_list<std::string_view> fields ) {
    std::string output;
    size_t count = 0;
    for ( const auto& field : fields ) count += field.size( );
    output.reserve( count + fields.size( ) - 1 );
    size_t index = 0;
    for ( const auto& field : fields ) {
        output.append( field.data( ), field.size( ) );
        if ( ++index != fields.size( ) ) output.push_back( '\0' );
    }
    return output;
}

bool hash_to_base64url( const std::string& value, std::string& output ) {
    std::array<std::uint8_t, kSha256Bytes> hash = { };
    return sha256( value, hash ) && base64url_encode( hash.data( ), hash.size( ), output );
}

bool hash_to_bytes( const std::string& value, std::array<std::uint8_t, kSha256Bytes>& output ) {
    return sha256( value, output );
}

bool derive_envelope_key( const char* label, const std::vector<std::uint8_t>& shared_secret, const std::string* request_nonce, std::array<std::uint8_t, kSha256Bytes>& output ) {
    std::string material( label );
    material.push_back( '\0' );
    material.append( reinterpret_cast<const char*>( shared_secret.data( ) ), shared_secret.size( ) );
    if ( request_nonce ) {
        material.push_back( '\0' );
        material.append( *request_nonce );
    }
    return sha256( material, output );
}

bool aes_gcm_encrypt( const std::array<std::uint8_t, kSha256Bytes>& key, const std::vector<std::uint8_t>& iv, const std::string& plaintext,
    const char* aad, std::vector<std::uint8_t>& ciphertext, std::array<std::uint8_t, kAesGcmTagBytes>& tag ) {
    BCRYPT_ALG_HANDLE raw_alg = nullptr;
    if ( BCryptOpenAlgorithmProvider( &raw_alg, BCRYPT_AES_ALGORITHM, nullptr, 0 ) < 0 ) return false;
    bcrypt_alg_ptr alg( raw_alg );
    if ( BCryptSetProperty( alg.get( ), BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>( const_cast<wchar_t*>( BCRYPT_CHAIN_MODE_GCM ) ), sizeof( BCRYPT_CHAIN_MODE_GCM ), 0 ) < 0 ) return false;

    DWORD object_length = 0;
    DWORD result_length = 0;
    if ( BCryptGetProperty( alg.get( ), BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>( &object_length ), sizeof( object_length ), &result_length, 0 ) < 0 ) return false;
    std::vector<std::uint8_t> object( object_length );
    BCRYPT_KEY_HANDLE raw_key = nullptr;
    if ( BCryptGenerateSymmetricKey( alg.get( ), &raw_key, object.data( ), static_cast<ULONG>( object.size( ) ), const_cast<PUCHAR>( key.data( ) ), static_cast<ULONG>( key.size( ) ), 0 ) < 0 ) return false;
    bcrypt_key_ptr aes_key( raw_key );

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth = { };
    BCRYPT_INIT_AUTH_MODE_INFO( auth );
    auth.pbNonce = const_cast<PUCHAR>( iv.data( ) );
    auth.cbNonce = static_cast<ULONG>( iv.size( ) );
    auth.pbAuthData = reinterpret_cast<PUCHAR>( const_cast<char*>( aad ) );
    auth.cbAuthData = static_cast<ULONG>( std::strlen( aad ) );
    auth.pbTag = tag.data( );
    auth.cbTag = static_cast<ULONG>( tag.size( ) );

    ULONG required = 0;
    if ( BCryptEncrypt( aes_key.get( ), reinterpret_cast<PUCHAR>( const_cast<char*>( plaintext.data( ) ) ), static_cast<ULONG>( plaintext.size( ) ), &auth, nullptr, 0, nullptr, 0, &required, 0 ) < 0 ) return false;
    ciphertext.resize( required );
    return BCryptEncrypt( aes_key.get( ), reinterpret_cast<PUCHAR>( const_cast<char*>( plaintext.data( ) ) ), static_cast<ULONG>( plaintext.size( ) ), &auth, nullptr, 0, ciphertext.data( ), required, &required, 0 ) >= 0 && ( ciphertext.resize( required ), true );
}

bool aes_gcm_decrypt( const std::array<std::uint8_t, kSha256Bytes>& key, const std::vector<std::uint8_t>& iv, const std::vector<std::uint8_t>& ciphertext,
    const char* aad, const std::array<std::uint8_t, kAesGcmTagBytes>& tag, std::string& plaintext ) {
    BCRYPT_ALG_HANDLE raw_alg = nullptr;
    if ( BCryptOpenAlgorithmProvider( &raw_alg, BCRYPT_AES_ALGORITHM, nullptr, 0 ) < 0 ) return false;
    bcrypt_alg_ptr alg( raw_alg );
    if ( BCryptSetProperty( alg.get( ), BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>( const_cast<wchar_t*>( BCRYPT_CHAIN_MODE_GCM ) ), sizeof( BCRYPT_CHAIN_MODE_GCM ), 0 ) < 0 ) return false;

    DWORD object_length = 0;
    DWORD result_length = 0;
    if ( BCryptGetProperty( alg.get( ), BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>( &object_length ), sizeof( object_length ), &result_length, 0 ) < 0 ) return false;
    std::vector<std::uint8_t> object( object_length );
    BCRYPT_KEY_HANDLE raw_key = nullptr;
    if ( BCryptGenerateSymmetricKey( alg.get( ), &raw_key, object.data( ), static_cast<ULONG>( object.size( ) ), const_cast<PUCHAR>( key.data( ) ), static_cast<ULONG>( key.size( ) ), 0 ) < 0 ) return false;
    bcrypt_key_ptr aes_key( raw_key );

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth = { };
    BCRYPT_INIT_AUTH_MODE_INFO( auth );
    auth.pbNonce = const_cast<PUCHAR>( iv.data( ) );
    auth.cbNonce = static_cast<ULONG>( iv.size( ) );
    auth.pbAuthData = reinterpret_cast<PUCHAR>( const_cast<char*>( aad ) );
    auth.cbAuthData = static_cast<ULONG>( std::strlen( aad ) );
    auth.pbTag = const_cast<PUCHAR>( tag.data( ) );
    auth.cbTag = static_cast<ULONG>( tag.size( ) );

    ULONG required = 0;
    if ( BCryptDecrypt( aes_key.get( ), const_cast<PUCHAR>( ciphertext.data( ) ), static_cast<ULONG>( ciphertext.size( ) ), &auth, nullptr, 0, nullptr, 0, &required, 0 ) < 0 ) return false;
    plaintext.resize( required );
    if ( BCryptDecrypt( aes_key.get( ), const_cast<PUCHAR>( ciphertext.data( ) ), static_cast<ULONG>( ciphertext.size( ) ), &auth, nullptr, 0, reinterpret_cast<PUCHAR>( plaintext.data( ) ), required, &required, 0 ) < 0 ) return false;
    plaintext.resize( required );
    return true;
}

bool create_ephemeral_shared_secret( const std::string& server_public_key, std::string& client_public_key, std::vector<std::uint8_t>& shared_secret ) {
    std::vector<std::uint8_t> server_blob;
    if ( !base64url_decode( server_public_key, server_blob ) || server_blob.size( ) != kEcdhPublicBlobBytes ) return false;
    std::uint32_t magic = 0;
    std::uint32_t coordinate_bytes = 0;
    std::memcpy( &magic, server_blob.data( ), sizeof( magic ) );
    std::memcpy( &coordinate_bytes, server_blob.data( ) + sizeof( magic ), sizeof( coordinate_bytes ) );
    if ( magic != kEcdhPublicP256Magic || coordinate_bytes != kSha256Bytes ) return false;

    BCRYPT_ALG_HANDLE raw_alg = nullptr;
    if ( BCryptOpenAlgorithmProvider( &raw_alg, BCRYPT_ECDH_P256_ALGORITHM, nullptr, 0 ) < 0 ) return false;
    bcrypt_alg_ptr alg( raw_alg );
    BCRYPT_KEY_HANDLE raw_client_key = nullptr;
    if ( BCryptGenerateKeyPair( alg.get( ), &raw_client_key, 256, 0 ) < 0 ) return false;
    bcrypt_key_ptr client_key( raw_client_key );
    if ( BCryptFinalizeKeyPair( client_key.get( ), 0 ) < 0 ) return false;

    ULONG public_length = 0;
    if ( BCryptExportKey( client_key.get( ), nullptr, BCRYPT_ECCPUBLIC_BLOB, nullptr, 0, &public_length, 0 ) < 0 || public_length != kEcdhPublicBlobBytes ) return false;
    std::vector<std::uint8_t> client_blob( public_length );
    if ( BCryptExportKey( client_key.get( ), nullptr, BCRYPT_ECCPUBLIC_BLOB, client_blob.data( ), public_length, &public_length, 0 ) < 0 ) return false;
    if ( !base64url_encode( client_blob.data( ), client_blob.size( ), client_public_key ) ) return false;

    BCRYPT_KEY_HANDLE raw_server_key = nullptr;
    if ( BCryptImportKeyPair( alg.get( ), nullptr, BCRYPT_ECCPUBLIC_BLOB, &raw_server_key, server_blob.data( ), static_cast<ULONG>( server_blob.size( ) ), 0 ) < 0 ) return false;
    bcrypt_key_ptr imported_server_key( raw_server_key );
    BCRYPT_SECRET_HANDLE raw_secret = nullptr;
    if ( BCryptSecretAgreement( client_key.get( ), imported_server_key.get( ), &raw_secret, 0 ) < 0 ) return false;
    bcrypt_secret_ptr secret( raw_secret );

    ULONG secret_length = 0;
    if ( BCryptDeriveKey( secret.get( ), BCRYPT_KDF_RAW_SECRET, nullptr, nullptr, 0, &secret_length, 0 ) < 0 || secret_length == 0 ) return false;
    shared_secret.resize( secret_length );
    return BCryptDeriveKey( secret.get( ), BCRYPT_KDF_RAW_SECRET, nullptr, shared_secret.data( ), secret_length, &secret_length, 0 ) >= 0 && ( shared_secret.resize( secret_length ), true );
}

bool encrypt_envelope( const std::array<std::uint8_t, kSha256Bytes>& key, const std::string& plaintext, const char* aad, std::string& output ) {
    std::vector<std::uint8_t> iv( kAesGcmIvBytes );
    if ( BCryptGenRandom( nullptr, iv.data( ), static_cast<ULONG>( iv.size( ) ), BCRYPT_USE_SYSTEM_PREFERRED_RNG ) < 0 ) return false;
    std::vector<std::uint8_t> ciphertext;
    std::array<std::uint8_t, kAesGcmTagBytes> tag = { };
    if ( !aes_gcm_encrypt( key, iv, plaintext, aad, ciphertext, tag ) ) return false;
    std::vector<std::uint8_t> payload;
    payload.reserve( 1 + iv.size( ) + ciphertext.size( ) + tag.size( ) );
    payload.push_back( kEnvelopeVersion );
    payload.insert( payload.end( ), iv.begin( ), iv.end( ) );
    payload.insert( payload.end( ), ciphertext.begin( ), ciphertext.end( ) );
    payload.insert( payload.end( ), tag.begin( ), tag.end( ) );
    return base64url_encode( payload.data( ), payload.size( ), output );
}

bool decrypt_envelope( const std::array<std::uint8_t, kSha256Bytes>& key, const std::string& encoded, const char* aad, std::string& plaintext ) {
    std::vector<std::uint8_t> payload;
    if ( !base64url_decode( encoded, payload ) || payload.size( ) < 1 + kAesGcmIvBytes + kAesGcmTagBytes || payload[ 0 ] != kEnvelopeVersion ) return false;
    std::vector<std::uint8_t> iv( payload.begin( ) + 1, payload.begin( ) + 1 + kAesGcmIvBytes );
    std::array<std::uint8_t, kAesGcmTagBytes> tag = { };
    std::copy_n( payload.end( ) - kAesGcmTagBytes, kAesGcmTagBytes, tag.begin( ) );
    const std::vector<std::uint8_t> ciphertext( payload.begin( ) + 1 + kAesGcmIvBytes, payload.end( ) - kAesGcmTagBytes );
    return aes_gcm_decrypt( key, iv, ciphertext, aad, tag, plaintext );
}

bool json_escape_ascii( const std::string& value, std::string& output ) {
    for ( unsigned char ch : value ) {
        if ( ch == '"' || ch == '\\' ) {
            output.push_back( '\\' );
            output.push_back( static_cast<char>( ch ) );
        } else if ( ch < 0x20 ) {
            return false;
        } else {
            output.push_back( static_cast<char>( ch ) );
        }
    }
    return true;
}

bool make_https_post( const std::wstring& endpoint, const std::string& body, std::string& response, DWORD& status ) {
    URL_COMPONENTS components = { };
    components.dwStructSize = sizeof( components );
    components.dwSchemeLength = static_cast<DWORD>( -1 );
    components.dwHostNameLength = static_cast<DWORD>( -1 );
    components.dwUrlPathLength = static_cast<DWORD>( -1 );
    components.dwExtraInfoLength = static_cast<DWORD>( -1 );
    std::vector<wchar_t> endpoint_copy( endpoint.begin( ), endpoint.end( ) );
    endpoint_copy.push_back( L'\0' );
    if ( !WinHttpCrackUrl( endpoint_copy.data( ), 0, 0, &components ) || components.nScheme != INTERNET_SCHEME_HTTPS || !components.lpszHostName ) return false;

    const std::wstring hostname( components.lpszHostName, components.dwHostNameLength );
    std::wstring path = components.lpszUrlPath ? std::wstring( components.lpszUrlPath, components.dwUrlPathLength ) : L"/";
    if ( components.lpszExtraInfo && components.dwExtraInfoLength ) path.append( components.lpszExtraInfo, components.dwExtraInfoLength );
    if ( path.empty( ) || path.back( ) != L'/' ) path.append( L"/" );
    path.append( L"v2/discord/member-info" );

    HINTERNET session = WinHttpOpen( L"crisis-auth-discord-member-info/2", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0 );
    if ( !session ) return false;
    HINTERNET connection = WinHttpConnect( session, hostname.c_str( ), components.nPort, 0 );
    if ( !connection ) { WinHttpCloseHandle( session ); return false; }
    HINTERNET request = WinHttpOpenRequest( connection, L"POST", path.c_str( ), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE );
    if ( !request ) { WinHttpCloseHandle( connection ); WinHttpCloseHandle( session ); return false; }

    const wchar_t* headers = L"Content-Type: application/json\r\nAccept: application/json\r\n";
    bool ok = WinHttpSendRequest( request, headers, static_cast<DWORD>( -1L ), const_cast<char*>( body.data( ) ), static_cast<DWORD>( body.size( ) ), static_cast<DWORD>( body.size( ) ), 0 ) &&
        WinHttpReceiveResponse( request, nullptr );
    if ( ok ) {
        DWORD status_size = sizeof( status );
        ok = WinHttpQueryHeaders( request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &status_size, nullptr ) != FALSE;
    }
    while ( ok ) {
        DWORD available = 0;
        if ( !WinHttpQueryDataAvailable( request, &available ) ) { ok = false; break; }
        if ( available == 0 ) break;
        if ( response.size( ) + available > kMaxHttpResponseBytes ) { ok = false; break; }
        const size_t offset = response.size( );
        response.resize( offset + available );
        DWORD read = 0;
        if ( !WinHttpReadData( request, response.data( ) + offset, available, &read ) ) { ok = false; break; }
        response.resize( offset + read );
    }

    WinHttpCloseHandle( request );
    WinHttpCloseHandle( connection );
    WinHttpCloseHandle( session );
    return ok;
}

struct json_value {
    enum class type { null_value, boolean, string, number, array, object } kind = type::null_value;
    bool boolean = false;
    std::string text;
    std::vector<json_value> array;
    std::map<std::string, json_value> object;
};

class json_parser {
public:
    explicit json_parser( const std::string& input ) : input_( input ) { }

    bool parse( json_value& value ) {
        skip_whitespace( );
        if ( !parse_value( value, 0 ) ) return false;
        skip_whitespace( );
        return position_ == input_.size( );
    }

private:
    bool parse_value( json_value& value, int depth ) {
        if ( depth > 32 || position_ >= input_.size( ) ) return false;
        const char ch = input_[ position_ ];
        if ( ch == '"' ) { value.kind = json_value::type::string; return parse_string( value.text ); }
        if ( ch == '{' ) return parse_object( value, depth + 1 );
        if ( ch == '[' ) return parse_array( value, depth + 1 );
        if ( input_.compare( position_, 4, "true" ) == 0 ) { position_ += 4; value.kind = json_value::type::boolean; value.boolean = true; return true; }
        if ( input_.compare( position_, 5, "false" ) == 0 ) { position_ += 5; value.kind = json_value::type::boolean; value.boolean = false; return true; }
        if ( input_.compare( position_, 4, "null" ) == 0 ) { position_ += 4; value.kind = json_value::type::null_value; return true; }
        return parse_number( value );
    }

    bool parse_object( json_value& value, int depth ) {
        value.kind = json_value::type::object;
        ++position_;
        skip_whitespace( );
        if ( consume( '}' ) ) return true;
        while ( true ) {
            std::string key;
            if ( !parse_string( key ) ) return false;
            skip_whitespace( );
            if ( !consume( ':' ) ) return false;
            skip_whitespace( );
            json_value child;
            if ( !parse_value( child, depth ) || !value.object.emplace( std::move( key ), std::move( child ) ).second ) return false;
            skip_whitespace( );
            if ( consume( '}' ) ) return true;
            if ( !consume( ',' ) ) return false;
            skip_whitespace( );
        }
    }

    bool parse_array( json_value& value, int depth ) {
        value.kind = json_value::type::array;
        ++position_;
        skip_whitespace( );
        if ( consume( ']' ) ) return true;
        while ( true ) {
            json_value child;
            if ( !parse_value( child, depth ) ) return false;
            value.array.push_back( std::move( child ) );
            skip_whitespace( );
            if ( consume( ']' ) ) return true;
            if ( !consume( ',' ) ) return false;
            skip_whitespace( );
        }
    }

    bool parse_string( std::string& output ) {
        if ( !consume( '"' ) ) return false;
        while ( position_ < input_.size( ) ) {
            const unsigned char ch = static_cast<unsigned char>( input_[ position_++ ] );
            if ( ch == '"' ) return true;
            if ( ch < 0x20 ) return false;
            if ( ch != '\\' ) { output.push_back( static_cast<char>( ch ) ); continue; }
            if ( position_ >= input_.size( ) ) return false;
            const char escaped = input_[ position_++ ];
            switch ( escaped ) {
            case '"': output.push_back( '"' ); break;
            case '\\': output.push_back( '\\' ); break;
            case '/': output.push_back( '/' ); break;
            case 'b': output.push_back( '\b' ); break;
            case 'f': output.push_back( '\f' ); break;
            case 'n': output.push_back( '\n' ); break;
            case 'r': output.push_back( '\r' ); break;
            case 't': output.push_back( '\t' ); break;
            case 'u': {
                std::uint32_t code_point = 0;
                if ( !parse_hex4( code_point ) ) return false;
                if ( code_point >= 0xD800 && code_point <= 0xDBFF ) {
                    if ( position_ + 2 > input_.size( ) || input_[ position_++ ] != '\\' || input_[ position_++ ] != 'u' ) return false;
                    std::uint32_t low = 0;
                    if ( !parse_hex4( low ) || low < 0xDC00 || low > 0xDFFF ) return false;
                    code_point = 0x10000 + ( ( code_point - 0xD800 ) << 10 ) + ( low - 0xDC00 );
                } else if ( code_point >= 0xDC00 && code_point <= 0xDFFF ) {
                    return false;
                }
                append_utf8( output, code_point );
                break;
            }
            default: return false;
            }
        }
        return false;
    }

    bool parse_number( json_value& value ) {
        const size_t start = position_;
        if ( consume( '-' ) && position_ >= input_.size( ) ) return false;
        if ( consume( '0' ) ) {
            if ( position_ < input_.size( ) && std::isdigit( static_cast<unsigned char>( input_[ position_ ] ) ) ) return false;
        } else {
            if ( position_ >= input_.size( ) || input_[ position_ ] < '1' || input_[ position_ ] > '9' ) return false;
            while ( position_ < input_.size( ) && std::isdigit( static_cast<unsigned char>( input_[ position_ ] ) ) ) ++position_;
        }
        if ( consume( '.' ) ) {
            if ( position_ >= input_.size( ) || !std::isdigit( static_cast<unsigned char>( input_[ position_ ] ) ) ) return false;
            while ( position_ < input_.size( ) && std::isdigit( static_cast<unsigned char>( input_[ position_ ] ) ) ) ++position_;
        }
        if ( position_ < input_.size( ) && ( input_[ position_ ] == 'e' || input_[ position_ ] == 'E' ) ) {
            ++position_;
            if ( position_ < input_.size( ) && ( input_[ position_ ] == '+' || input_[ position_ ] == '-' ) ) ++position_;
            if ( position_ >= input_.size( ) || !std::isdigit( static_cast<unsigned char>( input_[ position_ ] ) ) ) return false;
            while ( position_ < input_.size( ) && std::isdigit( static_cast<unsigned char>( input_[ position_ ] ) ) ) ++position_;
        }
        value.kind = json_value::type::number;
        value.text = input_.substr( start, position_ - start );
        return true;
    }

    bool parse_hex4( std::uint32_t& result ) {
        if ( position_ + 4 > input_.size( ) ) return false;
        result = 0;
        for ( int i = 0; i < 4; ++i ) {
            const char ch = input_[ position_++ ];
            const int digit = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
            if ( digit < 0 ) return false;
            result = ( result << 4 ) | static_cast<std::uint32_t>( digit );
        }
        return true;
    }

    static void append_utf8( std::string& output, std::uint32_t value ) {
        if ( value <= 0x7F ) output.push_back( static_cast<char>( value ) );
        else if ( value <= 0x7FF ) { output.push_back( static_cast<char>( 0xC0 | ( value >> 6 ) ) ); output.push_back( static_cast<char>( 0x80 | ( value & 0x3F ) ) ); }
        else if ( value <= 0xFFFF ) { output.push_back( static_cast<char>( 0xE0 | ( value >> 12 ) ) ); output.push_back( static_cast<char>( 0x80 | ( ( value >> 6 ) & 0x3F ) ) ); output.push_back( static_cast<char>( 0x80 | ( value & 0x3F ) ) ); }
        else { output.push_back( static_cast<char>( 0xF0 | ( value >> 18 ) ) ); output.push_back( static_cast<char>( 0x80 | ( ( value >> 12 ) & 0x3F ) ) ); output.push_back( static_cast<char>( 0x80 | ( ( value >> 6 ) & 0x3F ) ) ); output.push_back( static_cast<char>( 0x80 | ( value & 0x3F ) ) ); }
    }

    void skip_whitespace( ) { while ( position_ < input_.size( ) && ( input_[ position_ ] == ' ' || input_[ position_ ] == '\t' || input_[ position_ ] == '\r' || input_[ position_ ] == '\n' ) ) ++position_; }
    bool consume( char expected ) { if ( position_ < input_.size( ) && input_[ position_ ] == expected ) { ++position_; return true; } return false; }

    const std::string& input_;
    size_t position_ = 0;
};

const json_value* member( const json_value& object, const char* name ) {
    if ( object.kind != json_value::type::object ) return nullptr;
    const auto found = object.object.find( name );
    return found == object.object.end( ) ? nullptr : &found->second;
}

bool string_member( const json_value& object, const char* name, std::string& output ) {
    const json_value* value = member( object, name );
    if ( !value || value->kind != json_value::type::string ) return false;
    output = value->text;
    return true;
}

bool bool_member( const json_value& object, const char* name, bool& output ) {
    const json_value* value = member( object, name );
    if ( !value || value->kind != json_value::type::boolean ) return false;
    output = value->boolean;
    return true;
}

bool int_member( const json_value& object, const char* name, std::int64_t& output ) {
    const json_value* value = member( object, name );
    if ( !value || value->kind != json_value::type::number || value->text.find_first_of( ".eE" ) != std::string::npos ) return false;
    const auto [pointer, error] = std::from_chars( value->text.data( ), value->text.data( ) + value->text.size( ), output );
    return error == std::errc( ) && pointer == value->text.data( ) + value->text.size( );
}

bool nullable_string_member( const json_value& object, const char* name, std::string& output, bool& is_null ) {
    const json_value* value = member( object, name );
    if ( !value ) return false;
    if ( value->kind == json_value::type::null_value ) { is_null = true; output.clear( ); return true; }
    if ( value->kind != json_value::type::string ) return false;
    is_null = false;
    output = value->text;
    return true;
}

bool parse_role( const json_value& value, DiscordRole& role ) {
    if ( value.kind != json_value::type::object || !string_member( value, "id", role.id ) || !is_decimal_snowflake( role.id ) ||
        !string_member( value, "name", role.name ) || role.name.empty( ) ) return false;
    std::int64_t position = 0;
    std::int64_t color = 0;
    if ( !int_member( value, "position", position ) || !int_member( value, "color", color ) ||
        position < std::numeric_limits<std::int32_t>::min( ) || position > std::numeric_limits<std::int32_t>::max( ) ||
        color < 0 || color > std::numeric_limits<std::int32_t>::max( ) ||
        !bool_member( value, "hoist", role.hoist ) || !bool_member( value, "managed", role.managed ) || !bool_member( value, "mentionable", role.mentionable ) ) return false;
    role.position = static_cast<std::int32_t>( position );
    role.color = static_cast<std::int32_t>( color );
    return true;
}

bool role_equal( const DiscordRole& left, const DiscordRole& right ) {
    return left.id == right.id && left.name == right.name && left.position == right.position && left.color == right.color &&
        left.hoist == right.hoist && left.managed == right.managed && left.mentionable == right.mentionable;
}

bool snowflake_descending( const std::string& left, const std::string& right ) {
    return left.size( ) != right.size( ) ? left.size( ) > right.size( ) : left > right;
}

bool import_es256_public_key( const std::string& pem, bcrypt_key_ptr& output ) {
    DWORD der_length = 0;
    if ( !CryptStringToBinaryA( pem.c_str( ), static_cast<DWORD>( pem.size( ) ), CRYPT_STRING_BASE64HEADER, nullptr, &der_length, nullptr, nullptr ) ) return false;
    std::vector<std::uint8_t> der( der_length );
    if ( !CryptStringToBinaryA( pem.c_str( ), static_cast<DWORD>( pem.size( ) ), CRYPT_STRING_BASE64HEADER, der.data( ), &der_length, nullptr, nullptr ) ) return false;
    DWORD info_length = 0;
    if ( !CryptDecodeObject( X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, X509_PUBLIC_KEY_INFO, der.data( ), der_length, 0, nullptr, &info_length ) ) return false;
    std::vector<std::uint8_t> info_buffer( info_length );
    auto* info = reinterpret_cast<CERT_PUBLIC_KEY_INFO*>( info_buffer.data( ) );
    if ( !CryptDecodeObject( X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, X509_PUBLIC_KEY_INFO, der.data( ), der_length, 0, info, &info_length ) ) return false;
    BCRYPT_KEY_HANDLE raw_key = nullptr;
    if ( !CryptImportPublicKeyInfoEx2( X509_ASN_ENCODING, info, 0, nullptr, &raw_key ) ) return false;
    output.reset( raw_key );
    return true;
}

bool verify_es256( const std::string& signing_input, const std::vector<std::uint8_t>& signature, const std::string& pem ) {
    if ( signature.size( ) != 64 ) return false;
    std::array<std::uint8_t, kSha256Bytes> digest = { };
    if ( !sha256( signing_input, digest ) ) return false;
    bcrypt_key_ptr key( nullptr );
    if ( !import_es256_public_key( pem, key ) ) return false;
    return BCryptVerifySignature( key.get( ), nullptr, digest.data( ), static_cast<ULONG>( digest.size( ) ), const_cast<PUCHAR>( signature.data( ) ), static_cast<ULONG>( signature.size( ) ), 0 ) >= 0;
}

bool parse_compact_jws( const std::string& token, json_value& header, json_value& payload, std::string& signing_input, std::vector<std::uint8_t>& signature ) {
    const size_t first = token.find( '.' );
    const size_t second = first == std::string::npos ? std::string::npos : token.find( '.', first + 1 );
    if ( first == std::string::npos || second == std::string::npos || token.find( '.', second + 1 ) != std::string::npos ) return false;
    const std::string header_part = token.substr( 0, first );
    const std::string payload_part = token.substr( first + 1, second - first - 1 );
    const std::string signature_part = token.substr( second + 1 );
    std::vector<std::uint8_t> header_bytes;
    std::vector<std::uint8_t> payload_bytes;
    if ( !base64url_decode( header_part, header_bytes ) || !base64url_decode( payload_part, payload_bytes ) || !base64url_decode( signature_part, signature ) ||
        header_bytes.size( ) > 4096 || payload_bytes.size( ) > 32768 ) return false;
    const std::string header_json( header_bytes.begin( ), header_bytes.end( ) );
    const std::string payload_json( payload_bytes.begin( ), payload_bytes.end( ) );
    if ( !json_parser( header_json ).parse( header ) || !json_parser( payload_json ).parse( payload ) ) return false;
    signing_input = header_part + "." + payload_part;
    return true;
}

bool valid_cdn_url( const std::string& value ) {
    return value.size( ) <= 2048 && value.rfind( "https://cdn.discordapp.com/", 0 ) == 0;
}

bool verify_attestation( const std::string& token, const DiscordMemberInfoConfig& config, const std::string& discord_id,
    const std::string& license_hash, const std::string& hwid_hash, const std::string& discord_id_hash, const std::string& nonce,
    const std::string& request_hash, DiscordMemberInfo& result ) {
    json_value header;
    json_value payload;
    std::string signing_input;
    std::vector<std::uint8_t> signature;
    if ( !parse_compact_jws( token, header, payload, signing_input, signature ) || !verify_es256( signing_input, signature, config.pinnedServerPublicKeyPem ) ) return false;

    std::string algorithm;
    std::string typ;
    std::string kid;
    if ( !string_member( header, "alg", algorithm ) || algorithm != "ES256" || !string_member( header, "typ", typ ) || typ != "discord-member-info+jwt" ||
        !string_member( header, "kid", kid ) || kid != config.expectedSigningKeyId ) return false;

    std::string issuer;
    std::string audience;
    std::string subject;
    std::string signed_discord_id;
    std::string signed_license_hash;
    std::string signed_hwid_hash;
    std::string signed_discord_id_hash;
    std::string signed_request_hash;
    std::string signed_nonce;
    std::string signed_guild_id;
    bool signed_member = false;
    if ( !string_member( payload, "iss", issuer ) || issuer != config.expectedIssuer || !string_member( payload, "aud", audience ) || audience != config.expectedAudience ||
        !string_member( payload, "sub", subject ) || subject != "discord:" + discord_id || !string_member( payload, "discord_id", signed_discord_id ) || signed_discord_id != discord_id ||
        !string_member( payload, "license_hash", signed_license_hash ) || signed_license_hash != license_hash || !string_member( payload, "hwid_hash", signed_hwid_hash ) || signed_hwid_hash != hwid_hash ||
        !string_member( payload, "discord_id_hash", signed_discord_id_hash ) || signed_discord_id_hash != discord_id_hash || !string_member( payload, "request_hash", signed_request_hash ) || signed_request_hash != request_hash ||
        !string_member( payload, "nonce", signed_nonce ) || signed_nonce != nonce || !string_member( payload, "guild_id", signed_guild_id ) || signed_guild_id != config.expectedGuildId ||
        !bool_member( payload, "member", signed_member ) ) return false;

    std::int64_t iat = 0;
    std::int64_t nbf = 0;
    std::int64_t exp = 0;
    std::int64_t observed = 0;
    const std::int64_t now = std::chrono::duration_cast<std::chrono::seconds>( std::chrono::system_clock::now( ).time_since_epoch( ) ).count( );
    if ( !int_member( payload, "iat", iat ) || !int_member( payload, "nbf", nbf ) || !int_member( payload, "exp", exp ) || !int_member( payload, "observed_unix", observed ) ||
        nbf > now || exp <= now || iat > now + 5 || nbf > iat || iat - nbf > 5 || exp < iat || exp - iat > 60 ) return false;

    const json_value* roles = member( payload, "roles" );
    const json_value* highest = member( payload, "highest_role" );
    if ( !roles || roles->kind != json_value::type::array || !highest ) return false;

    result.member = signed_member;
    result.discordId = discord_id;
    bool pfp_null = false;
    bool username_null = false;
    bool display_null = false;
    bool global_null = false;
    if ( !nullable_string_member( payload, "pfp_url", result.pfpUrl, pfp_null ) || !nullable_string_member( payload, "username", result.username, username_null ) ||
        !nullable_string_member( payload, "display_name", result.displayName, display_null ) || !nullable_string_member( payload, "global_display_name", result.globalDisplayName, global_null ) ) return false;

    if ( !signed_member ) {
        if ( !pfp_null || !username_null || !display_null || !global_null || !roles->array.empty( ) || highest->kind != json_value::type::null_value ) return false;
        result.success = true;
        return true;
    }

    if ( pfp_null || username_null || display_null || global_null || result.username.empty( ) || result.displayName.empty( ) || !valid_cdn_url( result.pfpUrl ) ) return false;
    for ( const json_value& role_value : roles->array ) {
        DiscordRole role;
        if ( !parse_role( role_value, role ) ) return false;
        if ( !result.roles.empty( ) && ( result.roles.back( ).position < role.position ||
            ( result.roles.back( ).position == role.position && !snowflake_descending( result.roles.back( ).id, role.id ) ) ) ) return false;
        result.roles.push_back( std::move( role ) );
    }
    if ( result.roles.empty( ) ) {
        if ( highest->kind != json_value::type::null_value ) return false;
    } else {
        if ( !parse_role( *highest, result.highestRole ) || !role_equal( result.highestRole, result.roles.front( ) ) ) return false;
        result.hasHighestRole = true;
    }
    result.success = true;
    return true;
}

}

DiscordMemberInfoClient::DiscordMemberInfoClient( DiscordMemberInfoConfig config ) : config_( std::move( config ) ) { }

DiscordMemberInfo DiscordMemberInfoClient::GetDiscordMemberInfo( const std::string& licenseKey, const std::string& hwid, const std::string& discordId ) const {
    DiscordMemberInfo result;
    if ( !is_decimal_snowflake( discordId ) || config_.endpoint.empty( ) || config_.pinnedTransportPublicKeyBase64url.empty( ) || config_.pinnedServerPublicKeyPem.empty( ) || config_.expectedSigningKeyId.empty( ) ||
        config_.expectedIssuer.empty( ) || config_.expectedAudience.empty( ) || !is_decimal_snowflake( config_.expectedGuildId ) ) {
        result.error = "Discord member-info configuration is invalid.";
        return result;
    }

    std::array<std::uint8_t, kSha256Bytes> nonce_bytes = { };
    if ( !random_bytes( nonce_bytes ) ) { result.error = "Failed to generate request nonce."; return result; }
    std::string nonce;
    if ( !base64url_encode( nonce_bytes.data( ), nonce_bytes.size( ), nonce ) ) { result.error = "Failed to encode request nonce."; return result; }

    std::string license_hash;
    std::string hwid_hash;
    if ( !hash_to_base64url( with_zeroes( { "discord-license:v2", licenseKey } ), license_hash ) ||
        !hash_to_base64url( with_zeroes( { "discord-hwid:v2", hwid } ), hwid_hash ) ) { result.error = "Failed to calculate request hashes."; return result; }
    const std::int64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>( std::chrono::system_clock::now( ).time_since_epoch( ) ).count( );
    const std::string timestamp_text = std::to_string( timestamp );
    std::string discord_id_hash;
    if ( !hash_to_base64url( with_zeroes( { "discord-user:v2", discordId, license_hash, hwid_hash, nonce, timestamp_text } ), discord_id_hash ) ) { result.error = "Failed to calculate Discord ID hash."; return result; }

    const std::string canonical = "POST\n/v2/discord/member-info\n" + license_hash + "\n" + hwid_hash + "\n" + discordId + "\n" + discord_id_hash + "\n" + timestamp_text + "\n" + nonce;
    std::array<std::uint8_t, kSha256Bytes> authentication_key = { };
    std::array<std::uint8_t, kSha256Bytes> request_mac = { };
    if ( !hash_to_bytes( with_zeroes( { "discord-request-auth:v2", licenseKey, hwid } ), authentication_key ) || !hmac_sha256( authentication_key, canonical, request_mac ) ) { result.error = "Failed to authenticate request."; return result; }
    std::string request_mac_text;
    std::string request_hash;
    if ( !base64url_encode( request_mac.data( ), request_mac.size( ), request_mac_text ) || !hash_to_base64url( canonical, request_hash ) ) { result.error = "Failed to encode request authentication."; return result; }

    const std::string clear_request = "{\"license_hash\":\"" + license_hash + "\",\"hwid_hash\":\"" + hwid_hash + "\",\"discord_id\":\"" + discordId + "\",\"discord_id_hash\":\"" + discord_id_hash + "\",\"timestamp_unix\":" + timestamp_text + ",\"nonce\":\"" + nonce + "\",\"request_mac\":\"" + request_mac_text + "\"}";
    std::string client_public_key;
    std::vector<std::uint8_t> shared_secret;
    std::array<std::uint8_t, kSha256Bytes> request_envelope_key = { };
    std::string encrypted_request;
    if ( !create_ephemeral_shared_secret( config_.pinnedTransportPublicKeyBase64url, client_public_key, shared_secret ) ||
        !derive_envelope_key( "discord-envelope-request:v1", shared_secret, nullptr, request_envelope_key ) ||
        !encrypt_envelope( request_envelope_key, clear_request, kRequestEnvelopeAad, encrypted_request ) ) {
        result.error = "Failed to encrypt Discord member-info request.";
        return result;
    }
    const std::string request_body = "{\"e\":\"" + client_public_key + "\",\"d\":\"" + encrypted_request + "\"}";
    std::string response;
    DWORD status = 0;
    if ( !make_https_post( config_.endpoint, request_body, response, status ) || status != 200 ) { result.error = "Discord member-info request failed."; return result; }

    json_value envelope;
    std::string encrypted_response;
    std::string attestation;
    std::array<std::uint8_t, kSha256Bytes> response_envelope_key = { };
    if ( !json_parser( response ).parse( envelope ) || !string_member( envelope, "d", encrypted_response ) ||
        !derive_envelope_key( "discord-envelope-response:v1", shared_secret, &nonce, response_envelope_key ) ||
        !decrypt_envelope( response_envelope_key, encrypted_response, kResponseEnvelopeAad, attestation ) ) {
        result.error = "Discord member-info response was invalid.";
        return result;
    }
    if ( !verify_attestation( attestation, config_, discordId, license_hash, hwid_hash, discord_id_hash, nonce, request_hash, result ) ) {
        result = DiscordMemberInfo { };
        result.error = "Discord member-info attestation verification failed.";
    }
    return result;
}

}
