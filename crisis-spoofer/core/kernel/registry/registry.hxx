#pragma once

namespace hwid {
    struct identity_t {
        bool         m_ready;
        wchar_t      m_processor_name[ 64 ];
        wchar_t      m_manufacturer[ 64 ];
        wchar_t      m_product_name[ 64 ];
        wchar_t      m_serial[ 32 ];
        wchar_t      m_bios_vendor[ 64 ];
        wchar_t      m_bios_version[ 32 ];
        wchar_t      m_bios_release_date[ 16 ];
        wchar_t      m_computer_name[ 16 ];
        wchar_t      m_machine_guid[ 40 ];
        wchar_t      m_computer_hardware_id[ 40 ];
        char         m_manufacturer_ascii[ 64 ];
        char         m_product_ascii[ 64 ];
        char         m_serial_ascii[ 32 ];
        std::uint8_t m_smbios_uuid[ 16 ];
        std::uint8_t m_boot_guid[ 16 ];
        std::uint8_t m_mac[ 6 ];
        std::uint8_t m_interface_guid[ 16 ];
    };

    inline identity_t m_identity{ };
    inline bool       m_identity_ready = false;

    inline void copy_wstring( wchar_t* destination, unsigned long capacity, const wchar_t* source ) {
        if ( !destination || !capacity ) return;
        auto i = 0ul;
        for ( ; source && source[ i ] && i + 1 < capacity; ++i )
            destination[ i ] = source[ i ];
        destination[ i ] = L'\0';
    }

    inline void copy_ascii( char* destination, unsigned long capacity, const wchar_t* source ) {
        if ( !destination || !capacity ) return;
        auto i = 0ul;
        for ( ; source && source[ i ] && i + 1 < capacity; ++i )
            destination[ i ] = static_cast< char >( source[ i ] );
        destination[ i ] = '\0';
    }

    inline void random_guid( wchar_t* buffer, bool braces ) {
        constexpr wchar_t hex[] = L"0123456789ABCDEF";
        const auto offset = braces ? 1ul : 0ul;
        if ( braces ) buffer[ 0 ] = L'{';
        for ( auto i = 0ul; i < 36; ++i )
            buffer[ offset + i ] = hex[ kernel::rand( ) % 16 ];
        buffer[ offset + 8 ]  = L'-';
        buffer[ offset + 13 ] = L'-';
        buffer[ offset + 18 ] = L'-';
        buffer[ offset + 23 ] = L'-';
        if ( braces ) buffer[ 37 ] = L'}';
        buffer[ braces ? 38ul : 36ul ] = L'\0';
    }

    inline void random_alnum( wchar_t* buffer, unsigned long count ) {
        constexpr wchar_t alpha[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        for ( auto i = 0ul; i < count; ++i )
            buffer[ i ] = alpha[ kernel::rand( ) % 36 ];
        buffer[ count ] = L'\0';
    }

    inline void read_processor_brand( wchar_t* buffer, unsigned long capacity ) {
        if ( !buffer || capacity < 2 )
            return;
        buffer[ 0 ] = L'\0';

        int registers[ 4 ] = { 0 };
        __cpuid( registers, 0x80000000 );
        if ( static_cast< unsigned int >( registers[ 0 ] ) < 0x80000004u )
            return;

        char brand[ 49 ] = { };
        __cpuid( registers, 0x80000002 );
        std::memcpy( brand + 0, registers, 16 );
        __cpuid( registers, 0x80000003 );
        std::memcpy( brand + 16, registers, 16 );
        __cpuid( registers, 0x80000004 );
        std::memcpy( brand + 32, registers, 16 );

        auto begin = 0ul;
        while ( begin < 48 && brand[ begin ] == ' ' ) ++begin;
        auto end = 48ul;
        while ( end > begin && ( brand[ end - 1 ] == ' ' || brand[ end - 1 ] == '\0' ) ) --end;

        auto out = 0ul;
        for ( auto i = begin; i < end && out + 1 < capacity; ++i )
            buffer[ out++ ] = static_cast< wchar_t >( static_cast< unsigned char >( brand[ i ] ) );
        buffer[ out ] = L'\0';
    }

    static const wchar_t* processors[] = {
        L"Intel(R) Core(TM) i9-10900K CPU @ 3.70GHz",
        L"Intel(R) Core(TM) i7-10700K CPU @ 3.80GHz",
        L"Intel(R) Core(TM) i7-8700K CPU @ 3.70GHz",
        L"Intel(R) Core(TM) i5-10600K CPU @ 4.10GHz",
        L"Intel(R) Core(TM) i9-9900K CPU @ 3.60GHz",
        L"Intel(R) Core(TM) i7-9700K CPU @ 3.60GHz",
    };
    static const wchar_t* manufacturers[] = {
        L"ASUSTeK COMPUTER INC.",
        L"Micro-Star International Co., Ltd.",
        L"Gigabyte Technology Co., Ltd.",
        L"ASRock Inc.",
        L"Dell Inc.",
    };
    static const wchar_t* products[] = {
        L"X570 AORUS ELITE",
        L"B450 TOMAHAWK MAX",
        L"Z390-A PRO",
        L"B550M PRO-VDH WIFI",
        L"PRIME Z490-A",
    };
    static const wchar_t* bios_vendors[] = {
        L"American Megatrends Inc.",
        L"Phoenix Technologies LTD",
        L"Insyde Corp.",
        L"AMI",
    };

    inline void build_identity( ) {
        if ( m_identity_ready )
            return;

        read_processor_brand( m_identity.m_processor_name, 64 );
        if ( !m_identity.m_processor_name[ 0 ] )
            copy_wstring( m_identity.m_processor_name, 64, processors[ kernel::rand( ) % 6 ] );
        copy_wstring( m_identity.m_manufacturer, 64, manufacturers[ kernel::rand( ) % 5 ] );
        copy_wstring( m_identity.m_product_name, 64, products[ kernel::rand( ) % 5 ] );
        copy_wstring( m_identity.m_bios_vendor, 64, bios_vendors[ kernel::rand( ) % 4 ] );

        random_alnum( m_identity.m_serial, 12 );
        random_guid( m_identity.m_machine_guid, false );
        random_guid( m_identity.m_computer_hardware_id, true );

        constexpr wchar_t digits[] = L"0123456789";
        const auto month = 1 + ( kernel::rand( ) % 12 );
        const auto day   = 1 + ( kernel::rand( ) % 28 );
        const auto year  = 2018 + ( kernel::rand( ) % 6 );
        m_identity.m_bios_release_date[ 0 ] = digits[ month / 10 ];
        m_identity.m_bios_release_date[ 1 ] = digits[ month % 10 ];
        m_identity.m_bios_release_date[ 2 ] = L'/';
        m_identity.m_bios_release_date[ 3 ] = digits[ day / 10 ];
        m_identity.m_bios_release_date[ 4 ] = digits[ day % 10 ];
        m_identity.m_bios_release_date[ 5 ] = L'/';
        m_identity.m_bios_release_date[ 6 ] = digits[ year / 1000 ];
        m_identity.m_bios_release_date[ 7 ] = digits[ ( year / 100 ) % 10 ];
        m_identity.m_bios_release_date[ 8 ] = digits[ ( year / 10 ) % 10 ];
        m_identity.m_bios_release_date[ 9 ] = digits[ year % 10 ];
        m_identity.m_bios_release_date[ 10 ] = L'\0';

        const auto major = 1 + ( kernel::rand( ) % 9 );
        const auto minor = kernel::rand( ) % 100;
        m_identity.m_bios_version[ 0 ] = digits[ major ];
        m_identity.m_bios_version[ 1 ] = L'.';
        m_identity.m_bios_version[ 2 ] = digits[ minor / 10 ];
        m_identity.m_bios_version[ 3 ] = digits[ minor % 10 ];
        m_identity.m_bios_version[ 4 ] = L'\0';

        constexpr wchar_t prefix[] = L"DESKTOP-";
        for ( auto i = 0ul; i < 8; ++i ) m_identity.m_computer_name[ i ] = prefix[ i ];
        random_alnum( m_identity.m_computer_name + 8, 7 );

        copy_ascii( m_identity.m_manufacturer_ascii, 64, m_identity.m_manufacturer );
        copy_ascii( m_identity.m_product_ascii, 64, m_identity.m_product_name );
        copy_ascii( m_identity.m_serial_ascii, 32, m_identity.m_serial );

        for ( auto i = 0; i < 16; ++i ) {
            m_identity.m_smbios_uuid[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );
            m_identity.m_boot_guid[ i ]   = static_cast< std::uint8_t >( kernel::rand( ) );
        }
        m_identity.m_smbios_uuid[ 6 ] = ( m_identity.m_smbios_uuid[ 6 ] & 0x0F ) | 0x40;
        m_identity.m_smbios_uuid[ 8 ] = ( m_identity.m_smbios_uuid[ 8 ] & 0x3F ) | 0x80;
        m_identity.m_boot_guid[ 6 ]   = ( m_identity.m_boot_guid[ 6 ] & 0x0F ) | 0x40;
        m_identity.m_boot_guid[ 8 ]   = ( m_identity.m_boot_guid[ 8 ] & 0x3F ) | 0x80;

        for ( auto i = 0; i < 6; ++i )
            m_identity.m_mac[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );
        m_identity.m_mac[ 0 ] = ( m_identity.m_mac[ 0 ] & 0xFC ) | 0x02;

        for ( auto i = 0; i < 16; ++i )
            m_identity.m_interface_guid[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );
        m_identity.m_interface_guid[ 6 ] = ( m_identity.m_interface_guid[ 6 ] & 0x0F ) | 0x40;
        m_identity.m_interface_guid[ 8 ] = ( m_identity.m_interface_guid[ 8 ] & 0x3F ) | 0x80;

        m_identity.m_ready = true;
        m_identity_ready = true;
    }

    inline identity_t& identity( ) {
        if ( !m_identity_ready )
            build_identity( );
        return m_identity;
    }

    inline void patch_smbios_string(
        std::uint8_t* table, std::uint32_t table_len,
        std::uint32_t header_offset, std::uint8_t header_len,
        std::uint8_t string_index, const char* value )
    {
        if ( !table || string_index == 0 || !value )
            return;
        auto value_len = 0ul;
        while ( value[ value_len ] ) ++value_len;
        if ( !value_len )
            return;

        std::uint32_t pos = header_offset + header_len;
        std::uint8_t  current = 1;
        while ( pos < table_len ) {
            const std::uint32_t start = pos;
            while ( pos < table_len && table[ pos ] != 0 ) ++pos;
            if ( pos >= table_len ) return;
            if ( current == string_index ) {
                for ( std::uint32_t i = start; i < pos; ++i )
                    table[ i ] = static_cast< std::uint8_t >( value[ ( i - start ) % value_len ] );
                return;
            }
            ++current;
            ++pos;
            if ( pos < table_len && table[ pos ] == 0 )
                return;
        }
    }

    inline void patch_smbios( std::uint8_t* smbios_data, std::uint32_t smbios_len ) {
        if ( !smbios_data || smbios_len < 8 )
            return;

        auto& id = identity( );
        std::uint32_t off = 0;
        int walked = 0;
        while ( off + 4 <= smbios_len && walked < 512 ) {
            auto* hdr = smbios_data + off;
            const std::uint8_t stype = hdr[ 0 ];
            const std::uint8_t slen  = hdr[ 1 ];
            if ( slen < 4 || slen > 128 ) break;
            if ( off + slen > smbios_len ) break;
            if ( stype == 127 ) break;

            if ( stype == 1 && slen >= 0x18 ) {
                for ( int i = 0; i < 16; ++i )
                    hdr[ 8 + i ] = id.m_smbios_uuid[ i ];
                patch_smbios_string( smbios_data, smbios_len, off, slen, hdr[ 4 ], id.m_manufacturer_ascii );
                patch_smbios_string( smbios_data, smbios_len, off, slen, hdr[ 5 ], id.m_product_ascii );
                patch_smbios_string( smbios_data, smbios_len, off, slen, hdr[ 7 ], id.m_serial_ascii );
            }
            else if ( stype == 2 && slen >= 0x08 ) {
                patch_smbios_string( smbios_data, smbios_len, off, slen, hdr[ 4 ], id.m_manufacturer_ascii );
                patch_smbios_string( smbios_data, smbios_len, off, slen, hdr[ 5 ], id.m_product_ascii );
                patch_smbios_string( smbios_data, smbios_len, off, slen, hdr[ 7 ], id.m_serial_ascii );
            }
            else if ( stype == 3 && slen >= 0x08 ) {
                patch_smbios_string( smbios_data, smbios_len, off, slen, hdr[ 7 ], id.m_serial_ascii );
            }

            std::uint32_t next = off + slen;
            while ( next + 1 < smbios_len ) {
                if ( smbios_data[ next ] == 0 && smbios_data[ next + 1 ] == 0 ) {
                    next += 2;
                    break;
                }
                ++next;
            }
            if ( next <= off ) break;
            off = next;
            ++walked;
        }
    }

    inline void fill_consistent( void* buffer, unsigned long length ) {
        if ( !buffer || !length )
            return;

        auto* bytes = static_cast< std::uint8_t* >( buffer );
        unsigned long seed = 0;
        const auto* guid = identity( ).m_boot_guid;
        for ( int i = 0; i < 4; ++i )
            seed = ( seed << 8 ) | guid[ i ];
        if ( !seed )
            seed = 0x9E3779B9;

        for ( auto i = 0ul; i < length; ++i ) {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            bytes[ i ] = static_cast< std::uint8_t >( seed );
        }
    }

    inline bool is_interface_row( std::uint8_t* row ) {
        const auto physical_length = *reinterpret_cast< std::uint32_t* >( row + 0x420 );
        if ( physical_length > 32 )
            return false;

        auto guid_nonzero = false;
        for ( int i = 0; i < 16; ++i )
            if ( row[ 0x0C + i ] ) { guid_nonzero = true; break; }
        if ( !guid_nonzero )
            return false;

        const auto first_char = *reinterpret_cast< std::uint16_t* >( row + 0x1C );
        if ( first_char && ( first_char < 0x20 || first_char > 0x7E ) )
            return false;

        return true;
    }

    inline bool patch_interface_table( void* buffer, unsigned long length ) {
        if ( !buffer || length < 0x468 )
            return false;

        constexpr std::uint32_t row_size = 0x548;
        auto* bytes = static_cast< std::uint8_t* >( buffer );
        auto& id = identity( );

        const unsigned long bases[ ] = { 0ul, 8ul };
        for ( auto base : bases ) {
            if ( length <= base || ( length - base ) % row_size != 0 )
                continue;

            const auto count = ( length - base ) / row_size;
            if ( count == 0 || count > 1024 )
                continue;

            auto valid = true;
            for ( std::uint32_t r = 0; r < count && valid; ++r )
                valid = is_interface_row( bytes + base + r * row_size );
            if ( !valid )
                continue;

            for ( std::uint32_t r = 0; r < count; ++r ) {
                auto* row = bytes + base + r * row_size;
                for ( int i = 0; i < 16; ++i )
                    row[ 0x0C + i ] = static_cast< std::uint8_t >( id.m_interface_guid[ i ] ^ ( r + 1 ) );
                row[ 0x12 ] = ( row[ 0x12 ] & 0x0F ) | 0x40;
                row[ 0x14 ] = ( row[ 0x14 ] & 0x3F ) | 0x80;
                *reinterpret_cast< std::uint32_t* >( row + 0x420 ) = 6;
                for ( int i = 0; i < 6; ++i ) {
                    row[ 0x424 + i ] = id.m_mac[ i ];
                    row[ 0x444 + i ] = id.m_mac[ i ];
                }
            }
            return true;
        }

        return false;
    }
}

namespace registry {
    constexpr auto max_key_chars    = 260ul;
    constexpr auto max_name_chars   = 64ul;
    constexpr auto max_cache_entries = 64ul;
    constexpr auto max_cache_data    = 256ul;

    struct cache_entry_t {
        wchar_t       key_path  [ max_key_chars  ];
        wchar_t       value_name[ max_name_chars ];
        std::uint8_t  data      [ max_cache_data ];
        unsigned long data_size;
    };

    struct value_cache_t {
        KSPIN_LOCK    lock;
        unsigned long count;
        cache_entry_t entries[ max_cache_entries ];
    };

    inline value_cache_t g_value_cache{ };

    inline bool cache_wcs_eq( const wchar_t* a, const wchar_t* b, unsigned long max_chars ) {
        for ( auto i = 0ul; i < max_chars; ++i ) {
            wchar_t ca = a[ i ], cb = b[ i ];
            if ( ca >= L'a' && ca <= L'z' ) ca -= ( L'a' - L'A' );
            if ( cb >= L'a' && cb <= L'z' ) cb -= ( L'a' - L'A' );
            if ( ca != cb ) return false;
            if ( !ca      ) return true;
        }
        return true;
    }

    inline cache_entry_t* cache_find( const wchar_t* key_path, const wchar_t* value_name ) {
        for ( auto i = 0ul; i < g_value_cache.count; ++i ) {
            auto& e = g_value_cache.entries[ i ];
            if ( cache_wcs_eq( e.key_path,   key_path,   max_key_chars  ) &&
                 cache_wcs_eq( e.value_name, value_name, max_name_chars ) )
                return &e;
        }
        return nullptr;
    }

    inline void cache_insert(
        const wchar_t*      key_path,
        const wchar_t*      value_name,
        const std::uint8_t* data,
        unsigned long       data_size
    ) {
        if ( g_value_cache.count >= max_cache_entries ) return;
        auto& e = g_value_cache.entries[ g_value_cache.count++ ];

        unsigned long i;
        for ( i = 0; key_path[ i ] && i < max_key_chars - 1; ++i )
            e.key_path[ i ] = key_path[ i ];
        e.key_path[ i ] = L'\0';

        for ( i = 0; value_name[ i ] && i < max_name_chars - 1; ++i )
            e.value_name[ i ] = value_name[ i ];
        e.value_name[ i ] = L'\0';

        e.data_size = data_size < max_cache_data ? data_size : max_cache_data;
        kernel::rtl_copy_memory( e.data, data, e.data_size );
    }

    static const wchar_t* randomize_paths[] = {
        L"\\REGISTRY\\MACHINE\\HARDWARE\\",
        L"\\REGISTRY\\MACHINE\\SYSTEM\\Setup",
        L"\\REGISTRY\\MACHINE\\SYSTEM\\ControlSet001\\Control\\ComputerName\\",
        L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Control\\ComputerName\\",
        L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Control\\SystemInformation",
        L"\\REGISTRY\\MACHINE\\SYSTEM\\ControlSet001\\Control\\SystemInformation",
        L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Control\\IDConfigDB",
        L"\\REGISTRY\\MACHINE\\SYSTEM\\ControlSet001\\Control\\IDConfigDB",
        L"\\REGISTRY\\MACHINE\\SYSTEM\\ControlSet001\\Enum\\",
        L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Enum\\",
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\Microsoft\\Cryptography",
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Cryptography",
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate",
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\Microsoft\\SQMClient",
    };

    static const wchar_t* exclude_paths[] = {
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\Microsoft\\Cryptography\\OID",
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\Microsoft\\Cryptography\\Providers",
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\Microsoft\\Cryptography\\Defaults",
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Cryptography\\OID",
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Cryptography\\Providers",
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Cryptography\\Defaults",
    };

    inline unsigned long string_length( const wchar_t* s, unsigned long max ) {
        if ( !s ) return 0;
        auto n = 0ul;
        while ( n < max && s[ n ] ) ++n;
        return n;
    }

    inline bool same_string_ci( const wchar_t* a, const wchar_t* b ) {
        if ( !a || !b ) return false;
        return _wcsicmp( a, b ) == 0;
    }

    inline bool starts_with_ci( const wchar_t* s, const wchar_t* prefix ) {
        if ( !s || !prefix ) return false;
        return _wcsnicmp( s, prefix, string_length( prefix, max_key_chars ) ) == 0;
    }

    inline bool copy_unicode_string( wchar_t* dst, unsigned long dst_chars, UNICODE_STRING* src ) {
        if ( !dst || !dst_chars ) return false;
        dst[ 0 ] = 0;
        if ( !src || !src->Buffer || !src->Length ) return true;
        const auto n    = src->Length / sizeof( wchar_t );
        const auto copy = n >= dst_chars ? dst_chars - 1 : n;
        __try {
            for ( auto i = 0ul; i < copy; ++i ) dst[ i ] = src->Buffer[ i ];
            dst[ copy ] = 0;
        }
        __except ( EXCEPTION_EXECUTE_HANDLER ) { dst[ 0 ] = 0; return false; }
        return true;
    }

    inline bool get_key_path_from_handle( void* handle, wchar_t* dst, unsigned long dst_chars ) {
        if ( !handle || !dst || !dst_chars ) return false;
        dst[ 0 ] = 0;
        std::uint8_t buffer[ 1024 ]{ };
        auto return_length = 0ul;
        auto* info = reinterpret_cast< object_name_information_t* >( buffer );
        const auto status = kernel::zw_query_object( handle, 1, info, sizeof( buffer ), &return_length );
        if ( !NT_SUCCESS( status ) || !info->name.m_buffer || !info->name.m_length )
            return false;
        UNICODE_STRING name{ };
        name.Buffer        = info->name.m_buffer;
        name.Length        = info->name.m_length;
        name.MaximumLength = info->name.m_maximum_length;
        return copy_unicode_string( dst, dst_chars, &name );
    }

    inline bool should_randomize( const wchar_t* key_path ) {
        for ( const auto* prefix : exclude_paths )
            if ( starts_with_ci( key_path, prefix ) )
                return false;
        for ( const auto* prefix : randomize_paths )
            if ( starts_with_ci( key_path, prefix ) || same_string_ci( key_path, prefix ) )
                return true;
        return false;
    }

    inline void rand_hex( wchar_t* buf, unsigned long count ) {
        constexpr wchar_t hex[] = L"0123456789ABCDEF";
        for ( auto i = 0ul; i < count; ++i )
            buf[ i ] = hex[ kernel::rand( ) % 16 ];
    }

    inline void rand_guid( wchar_t* buf ) {
        buf[ 0 ] = L'{';
        rand_hex( buf + 1,  8  ); buf[  9 ] = L'-';
        rand_hex( buf + 10, 4  ); buf[ 14 ] = L'-';
        rand_hex( buf + 15, 4  ); buf[ 19 ] = L'-';
        rand_hex( buf + 20, 4  ); buf[ 24 ] = L'-';
        rand_hex( buf + 25, 12 );
        buf[ 37 ] = L'}';
        buf[ 38 ] = L'\0';
    }

    inline void rand_computer_name( wchar_t* buf ) {
        constexpr wchar_t alpha[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        constexpr wchar_t prefix[] = L"DESKTOP-";
        for ( auto i = 0ul; i < 8; ++i ) buf[ i ] = prefix[ i ];
        for ( auto i = 0ul; i < 7; ++i ) buf[ 8 + i ] = alpha[ kernel::rand( ) % 36 ];
        buf[ 15 ] = L'\0';
    }

    inline void randomize_data(
        const wchar_t* value_name,
        unsigned long  type,
        std::uint8_t*  data,
        unsigned long  data_size
    ) {
        if ( !data || !data_size ) return;

        if ( same_string_ci( value_name, L"~MHz" ) ) return;

        switch ( type ) {
            case REG_DWORD: {
                if ( data_size < 4 ) break;
                // A boolean is not a hardware identifier. OOBEInProgress, LoadAppInit_DLLs and
                // the rest of this family are documented 0/1 values that Windows itself compares,
                // so handing back 32 random bits changes OS behaviour and is a fingerprint no real
                // machine has. Anything already reading 0 or 1 is left exactly as it was.
                const auto current = *reinterpret_cast< const unsigned long* >( data );
                if ( current <= 1 )
                	break;
                const auto v = kernel::rand( );
                kernel::rtl_copy_memory( data, &v, 4 );
                break;
            }
            case REG_QWORD: {
                if ( data_size < 8 ) break;
                const auto lo = kernel::rand( ), hi = kernel::rand( );
                kernel::rtl_copy_memory( data,     &lo, 4 );
                kernel::rtl_copy_memory( data + 4, &hi, 4 );
                break;
            }
            case REG_BINARY: {
                for ( auto i = 0ul; i < data_size; ++i )
                    data[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );
                break;
            }
            case REG_SZ:
            case REG_EXPAND_SZ: {
                auto* wdata = reinterpret_cast< wchar_t* >( data );
                const auto wlen = data_size / sizeof( wchar_t );
                if ( !wlen ) break;

                auto& id = hwid::identity( );
                if ( same_string_ci( value_name, L"ProcessorNameString" ) ) {
                    hwid::copy_wstring( wdata, wlen, id.m_processor_name );
                } else if ( same_string_ci( value_name, L"SystemManufacturer" ) ) {
                    hwid::copy_wstring( wdata, wlen, id.m_manufacturer );
                } else if ( same_string_ci( value_name, L"SystemProductName" ) ) {
                    hwid::copy_wstring( wdata, wlen, id.m_product_name );
                } else if ( same_string_ci( value_name, L"BIOSVendor" ) ) {
                    hwid::copy_wstring( wdata, wlen, id.m_bios_vendor );
                } else if ( same_string_ci( value_name, L"BIOSVersion" ) ) {
                    hwid::copy_wstring( wdata, wlen, id.m_bios_version );
                } else if ( same_string_ci( value_name, L"BIOSReleaseDate" ) ) {
                    hwid::copy_wstring( wdata, wlen, id.m_bios_release_date );
                } else if ( same_string_ci( value_name, L"MachineGuid" ) ) {
                    hwid::copy_wstring( wdata, wlen, id.m_machine_guid );
                } else if ( same_string_ci( value_name, L"ComputerHardwareId" ) ) {
                    hwid::copy_wstring( wdata, wlen, id.m_computer_hardware_id );
                } else if ( same_string_ci( value_name, L"ComputerName" ) ) {
                    hwid::copy_wstring( wdata, wlen, id.m_computer_name );
                } else {
                    // No spoof exists for this value, so leave it unchanged. Fabricating random hex
                    // into an arbitrary REG_SZ protects no identifier - the ones that matter are
                    // matched by name above - and only makes the machine read back as something it
                    // is not.
                    break;
                }
                break;
            }
            default:
                break;
        }
    }

    inline bool try_randomize(
        void*           handle,
        UNICODE_STRING* value_name_us,
        std::uint32_t   info_class,
        void*           buffer,
        unsigned long   length,
        bool            force = false
    ) {
        if ( !buffer || !length ) 
			return false;

        wchar_t key_path[ max_key_chars ]{ };
        if ( !get_key_path_from_handle( handle, key_path, max_key_chars ) )
			return true;

        if ( !force && !should_randomize( key_path ) )
			return true;

        wchar_t value_name[ max_name_chars ]{ };
        copy_unicode_string( value_name, max_name_chars, value_name_us );

        if ( same_string_ci( value_name, L"SystemSetupInProgress" ) )
			return true;

		// we use this to block PCI from registry access attempts for network tracers
		// all results from us attempting to block access causes our wifi to shutoff
		if ( same_string_ci( value_name, L"Driver" ) ) {
			//exception::sleep_forever( nullptr ); // temporary
			return true;
		}

        auto* p           = reinterpret_cast< std::uint8_t* >( buffer );
        unsigned long  type      = 0;
        std::uint8_t*  data      = nullptr;
        unsigned long  data_size = 0;

        __try {
            switch ( info_class ) {
                case key_value_partial_information:
                case key_value_partial_information_align64:
                    type      = *reinterpret_cast< unsigned long* >( p + 4 );
                    data_size = *reinterpret_cast< unsigned long* >( p + 8 );
                    if ( 12u + data_size <= length )
                        data = p + 12;
                    break;
                case key_value_full_information:
                case key_value_full_information_align64: {
                    type                   = *reinterpret_cast< unsigned long* >( p + 4  );
                    const auto data_offset = *reinterpret_cast< unsigned long* >( p + 8  );
                    data_size              = *reinterpret_cast< unsigned long* >( p + 12 );
                    if ( data_offset + data_size <= length )
                        data = p + data_offset;
                    break;
                }
                default:
					return false;
			}
        }
        __except ( EXCEPTION_EXECUTE_HANDLER ) {
			return false;
		}

        if ( !data || !data_size ) 			return false;

        KIRQL irql;
        kernel::ke_acquire_spin_lock( &g_value_cache.lock, &irql );
        auto* hit = cache_find( key_path, value_name );
        if ( hit ) {
            const auto copy = hit->data_size < data_size ? hit->data_size : data_size;
            kernel::rtl_copy_memory( data, hit->data, copy );
            kernel::ke_release_spin_lock( &g_value_cache.lock, irql );
			return true;
		}
        kernel::ke_release_spin_lock( &g_value_cache.lock, irql );

        randomize_data( value_name, type, data, data_size );

        kernel::ke_acquire_spin_lock( &g_value_cache.lock, &irql );
        auto* existing = cache_find( key_path, value_name );
        if ( existing ) {
            const auto copy = existing->data_size < data_size ? existing->data_size : data_size;
            kernel::rtl_copy_memory( data, existing->data, copy );
        } else {
            cache_insert( key_path, value_name, data, data_size );
        }
        kernel::ke_release_spin_lock( &g_value_cache.lock, irql );

        switch ( type ) {
            case REG_DWORD:
                kernel::dbg_print( obf( "blocked %ws\\%ws -> 0x%08X\n" ),
                    key_path, value_name, *reinterpret_cast< unsigned long* >( data ) );
                break;
            case REG_QWORD:
                kernel::dbg_print( obf( "blocked %ws\\%ws -> 0x%016llX\n" ),
                    key_path, value_name, *reinterpret_cast< std::uint64_t* >( data ) );
                break;
            case REG_BINARY:
                kernel::dbg_print( obf( "blocked %ws\\%ws -> %02X %02X %02X %02X ... (%lu bytes)\n" ),
                    key_path, value_name, data[ 0 ], data[ 1 ], data[ 2 ], data[ 3 ], data_size );
                break;
            case REG_SZ:
            case REG_EXPAND_SZ:
                kernel::dbg_print( obf( "blocked %ws\\%ws -> '%ws'\n" ),
                    key_path, value_name, reinterpret_cast< wchar_t* >( data ) );
                break;
            default:
                kernel::dbg_print( obf( "blocked %ws\\%ws (type=0x%x, %lu bytes)\n" ),
                    key_path, value_name, type, data_size );
                break;
        }

		return true;
	}
}
