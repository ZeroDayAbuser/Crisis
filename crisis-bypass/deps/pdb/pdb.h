#pragma once
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <memory>

#pragma pack(push, 1)
struct pdb_blob_header_t {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t count;
    std::uint32_t data_size;
};
#pragma pack(pop)

struct pdb_scanner_t {
    const std::uint8_t* m_data = nullptr;
    std::size_t         m_size = 0;

    static constexpr std::uint32_t k_magic   = 0x42445043u;
    static constexpr std::uint32_t k_version = 1u;

    void init( const std::uint8_t* data, std::size_t size ) {
        m_data = data;
        m_size = size;
    }

    std::uint64_t get_symbol( const char* name ) const {
        return lookup( 0, name, nullptr );
    }

    std::uint64_t get_member( const char* type_name, const char* member_name ) const {
        return lookup( 1, type_name, member_name );
    }

    std::uint64_t get_struct_size( const char* type_name ) const {
        return lookup( 2, type_name, nullptr );
    }

    std::uint64_t get_bit_position( const char* type_name, const char* member_name ) const {
        return lookup( 3, type_name, member_name );
    }

private:
    std::uint64_t lookup( std::uint8_t type, const char* key, const char* sub ) const {
        if ( !m_data || m_size < sizeof( pdb_blob_header_t ) ) return 0;
        const auto* hdr = reinterpret_cast<const pdb_blob_header_t*>( m_data );
        if ( hdr->magic != k_magic || hdr->version != k_version ) return 0;

        const std::uint8_t* ptr = m_data + sizeof( pdb_blob_header_t );
        const std::uint8_t* end = m_data + hdr->data_size;

        while ( ptr + 2 <= end ) {
            const auto etype    = ptr[ 0 ];
            const auto name_len = ptr[ 1 ];
            ptr += 2;
            if ( ptr + name_len + sizeof( std::uint64_t ) > end ) break;
            const char*  ename = reinterpret_cast<const char*>( ptr );
            ptr += name_len;
            const auto value = *reinterpret_cast<const std::uint64_t*>( ptr );
            ptr += sizeof( std::uint64_t );

            if ( etype != type ) continue;

            if ( type == 0 || type == 2 ) {
                if ( str_eq( ename, name_len, key ) ) return value;
            } else {
                std::uint8_t sep = 0;
                for ( std::uint8_t i = 0; i + 1 < name_len; ++i )
                    if ( ename[ i ] == ':' && ename[ i + 1 ] == ':' ) { sep = i; break; }
                if ( !sep ) continue;
                if ( str_eq( ename, sep, key ) &&
                     str_eq( ename + sep + 2,
                              static_cast<std::uint8_t>( name_len - sep - 2 ), sub ) )
                    return value;
            }
        }
        return 0;
    }

    static bool str_eq( const char* a, std::uint8_t alen, const char* b ) {
        if ( !b ) return false;
        const auto blen = static_cast<std::uint8_t>(
            ( std::min )( std::strlen( b ), std::size_t{ 255 } ) );
        if ( alen != blen ) return false;
        return std::memcmp( a, b, alen ) == 0;
    }
};

extern std::unique_ptr<pdb_scanner_t> g_scanner;
