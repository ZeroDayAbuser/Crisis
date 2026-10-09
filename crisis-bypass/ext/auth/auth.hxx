#pragma once

namespace auth {
    VIRTUALIZE std::string generate_hwid( ) {
        DWORD volume_serial = 0;
        if ( !GetVolumeInformationA( "C:\\", nullptr, 0, &volume_serial, nullptr, nullptr, nullptr, 0 ) ) {
            volume_serial = 0xDEADBEEF;
        }

        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setfill( '0' ) << std::setw( 8 ) << volume_serial;
        return oss.str( );
    }
    VIRTUALIZE_MARK( generate_hwid );

    VIRTUALIZE bool prompt_key( std::string& out_key ) {
        rise::log_info( "Enter your license key below." );
        if ( !std::getline( std::cin, out_key ) ) {
            return false;
        }
        return !out_key.empty( );
    }
    VIRTUALIZE_MARK( prompt_key );

    VIRTUALIZE static bool http_get( const std::wstring& url, const std::wstring& path, std::string& out_response ) {
        HINTERNET h_session = WinHttpOpen( L"rise-loader/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0 );
        if ( !h_session ) {
            return false;
        }

        URL_COMPONENTS url_comp = { };
        url_comp.dwStructSize = sizeof( url_comp );
        url_comp.dwHostNameLength = ( DWORD )-1;
        url_comp.dwUrlPathLength = ( DWORD )-1;
        url_comp.dwSchemeLength = ( DWORD )-1;

        wchar_t url_buf[ 2048 ] = { };
        wcsncpy_s( url_buf, url.c_str( ), _TRUNCATE );
        WinHttpCrackUrl( url_buf, 0, 0, &url_comp );

        std::wstring hostname( url_comp.lpszHostName, url_comp.dwHostNameLength );
        INTERNET_PORT port = url_comp.nPort;
        DWORD request_flags = ( url_comp.nScheme == INTERNET_SCHEME_HTTPS ) ? WINHTTP_FLAG_SECURE : 0;

        HINTERNET h_connect = WinHttpConnect( h_session, hostname.c_str( ), port, 0 );
        if ( !h_connect ) {
            WinHttpCloseHandle( h_session );
            return false;
        }

        HINTERNET h_request = WinHttpOpenRequest( h_connect, L"GET", path.c_str( ), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, request_flags );
        if ( !h_request ) {
            WinHttpCloseHandle( h_connect );
            WinHttpCloseHandle( h_session );
            return false;
        }

        bool result = WinHttpSendRequest( h_request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0 ) &&
            WinHttpReceiveResponse( h_request, nullptr );

        if ( result ) {
            DWORD status_code = 0;
            DWORD status_size = sizeof( DWORD );
            if ( WinHttpQueryHeaders( h_request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status_code, &status_size, nullptr ) ) {
                if ( status_code != 200 ) {
                    result = false;
                }
            }
        }

        if ( result ) {
            DWORD bytes_read = 0;
            std::vector<char> buffer( 4096 );
            do {
                bytes_read = 0;
                if ( !WinHttpReadData( h_request, buffer.data( ), static_cast< DWORD >( buffer.size( ) ), &bytes_read ) ) {
                    result = false;
                    break;
                }
                if ( bytes_read > 0 ) {
                    out_response.append( buffer.data( ), bytes_read );
                }
            } while ( bytes_read > 0 );
        }

        WinHttpCloseHandle( h_request );
        WinHttpCloseHandle( h_connect );
        WinHttpCloseHandle( h_session );
        return result && !out_response.empty( );
    }
    VIRTUALIZE_MARK( http_get );

    VIRTUALIZE static bool http_download( const std::wstring& url, const std::wstring& path, std::vector<std::uint8_t>& out_data ) {
        HINTERNET h_session = WinHttpOpen( L"rise-loader/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0 );
        if ( !h_session ) return false;

        URL_COMPONENTS url_comp = { };
        url_comp.dwStructSize = sizeof( url_comp );
        url_comp.dwHostNameLength = ( DWORD )-1;
        url_comp.dwUrlPathLength = ( DWORD )-1;
        url_comp.dwSchemeLength = ( DWORD )-1;

        wchar_t url_buf[ 2048 ] = { };
        wcsncpy_s( url_buf, url.c_str( ), _TRUNCATE );
        WinHttpCrackUrl( url_buf, 0, 0, &url_comp );

        std::wstring hostname( url_comp.lpszHostName, url_comp.dwHostNameLength );
        INTERNET_PORT port = url_comp.nPort;
        DWORD request_flags = ( url_comp.nScheme == INTERNET_SCHEME_HTTPS ) ? WINHTTP_FLAG_SECURE : 0;

        HINTERNET h_connect = WinHttpConnect( h_session, hostname.c_str( ), port, 0 );
        if ( !h_connect ) { WinHttpCloseHandle( h_session ); return false; }

        HINTERNET h_request = WinHttpOpenRequest( h_connect, L"GET", path.c_str( ), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, request_flags );
        if ( !h_request ) { WinHttpCloseHandle( h_connect ); WinHttpCloseHandle( h_session ); return false; }

        bool result = WinHttpSendRequest( h_request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0 ) &&
            WinHttpReceiveResponse( h_request, nullptr );

        if ( result ) {
            DWORD status_code = 0;
            DWORD status_size = sizeof( DWORD );
            if ( WinHttpQueryHeaders( h_request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status_code, &status_size, nullptr ) ) {
                if ( status_code != 200 ) {
                    result = false;
                }
            }
        }

        if ( result ) {
            out_data.clear( );

            DWORD content_length = 0;
            DWORD content_length_size = sizeof( content_length );
            if ( WinHttpQueryHeaders( h_request, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &content_length, &content_length_size, nullptr ) && content_length > 0 ) {
                out_data.reserve( content_length );
            }

            while ( true ) {
                DWORD available = 0;
                if ( !WinHttpQueryDataAvailable( h_request, &available ) ) {
                    result = false;
                    break;
                }

                if ( available == 0 ) {
                    break;
                }

                size_t before = out_data.size( );
                out_data.resize( before + available );

                DWORD total_read = 0;
                while ( total_read < available ) {
                    DWORD bytes_read = 0;
                    if ( !WinHttpReadData( h_request, out_data.data( ) + before + total_read, available - total_read, &bytes_read ) ) {
                        out_data.resize( before + total_read );
                        result = false;
                        break;
                    }

                    if ( bytes_read == 0 ) {
                        out_data.resize( before + total_read );
                        break;
                    }

                    total_read += bytes_read;
                }

                if ( !result ) {
                    break;
                }

                out_data.resize( before + total_read );
            }
        }

        WinHttpCloseHandle( h_request );
        WinHttpCloseHandle( h_connect );
        WinHttpCloseHandle( h_session );
        return result && !out_data.empty( );
    }
    VIRTUALIZE_MARK( http_download );

    VIRTUALIZE static bool parse_json_uint64( const std::string& json, const std::string& key, std::uint64_t& out_value ) {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find( search );
        if ( pos == std::string::npos ) return false;

        pos = json.find( ':', pos + search.length( ) );
        if ( pos == std::string::npos ) return false;
        ++pos;

        while ( pos < json.size( ) && ( json[ pos ] == ' ' || json[ pos ] == '\t' || json[ pos ] == '\n' || json[ pos ] == '\r' ) ) {
            ++pos;
        }

        if ( pos >= json.size( ) ) return false;

        bool is_string = json[ pos ] == '"';
        if ( is_string ) ++pos;

        size_t end = pos;
        while ( end < json.size( ) && ( ( json[ end ] >= '0' && json[ end ] <= '9' ) || json[ end ] == '.' ) ) {
            ++end;
        }

        if ( pos == end ) return false;

        try {
            out_value = std::stoull( json.substr( pos, end - pos ) );
            return true;
        }
        catch ( ... ) {
            return false;
        }
    }
    VIRTUALIZE_MARK( parse_json_uint64 );

    VIRTUALIZE static bool parse_json_bool( const std::string& json, const std::string& key, bool& out_value ) {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find( search );
        if ( pos == std::string::npos ) return false;

        pos = json.find( ':', pos + search.length( ) );
        if ( pos == std::string::npos ) return false;
        ++pos;

        while ( pos < json.size( ) && ( json[ pos ] == ' ' || json[ pos ] == '\t' || json[ pos ] == '\n' || json[ pos ] == '\r' ) ) {
            ++pos;
        }

        if ( pos + 4 <= json.size( ) && json.compare( pos, 4, "true" ) == 0 ) {
            out_value = true;
            return true;
        }
        if ( pos + 5 <= json.size( ) && json.compare( pos, 5, "false" ) == 0 ) {
            out_value = false;
            return true;
        }
        return false;
    }
    VIRTUALIZE_MARK( parse_json_bool );

    static VIRTUALIZE void derive_xor_key( const std::string& key, const std::string& hwid, std::vector<std::uint8_t>& out_key ) {
        const std::string seed = key + hwid;
        std::int32_t hash = 0;

        for ( size_t i = 0; i < seed.length( ); i++ ) {
            std::int64_t temp = ( static_cast< std::int64_t >( hash ) << 5 ) - hash + static_cast< unsigned char >( seed[ i ] );
            hash = static_cast< std::int32_t >( temp & 0xFFFFFFFF );
        }

        out_key.resize( 256 );
        std::uint32_t uhash = static_cast< std::uint32_t >( hash );

        for ( int i = 0; i < 256; i++ ) {
            out_key[ i ] = static_cast< std::uint8_t >( ( uhash ^ ( i * 0x9E ) ) & 0xFF );
            uhash = ( ( uhash * 1103515245 + 12345 ) & 0xFFFFFFFF );
        }
    }
    VIRTUALIZE_MARK( derive_xor_key );

    static VIRTUALIZE void xor_decrypt( std::vector<std::uint8_t>& buffer, const std::vector<std::uint8_t>& xor_key ) {
        for ( size_t i = 0; i < buffer.size( ); i++ ) {
            buffer[ i ] ^= xor_key[ i % xor_key.size( ) ];
        }
    }
    VIRTUALIZE_MARK( xor_decrypt );

    VIRTUALIZE auth_response_t authenticate( const std::string& key ) {
        auth_response_t response = { };

        std::string hwid = generate_hwid( );
        //rise::log_info( "HWID -> %s", hwid.c_str( ) );

        std::ostringstream url_builder;
        url_builder << "/?key=" << key << "&hwid=" << hwid;
        std::string path = url_builder.str( );
        std::wstring w_path( path.begin( ), path.end( ) );

        std::string raw_response;
        if ( !http_get( L"http://mc.novacaine.wtf:1053", w_path, raw_response ) ) {
            response.message = "Failed to contact authentication server.";
            return response;
        }

        //rise::log_debug( "Server response: %s", raw_response.c_str( ) );

        if ( !parse_json_bool( raw_response, "success", response.success ) ) {
            response.message = "Invalid server response format.";
            return response;
        }

        if ( !response.success ) {
            response.message = "Key is invalid or expired.";
            return response;
        }

        if ( !parse_json_uint64( raw_response, "start_unix", response.start_unix ) ||
            !parse_json_uint64( raw_response, "expiry_unix", response.expiry_unix ) ) {
            response.success = false;
            response.message = "Missing expiry fields in server response.";
            return response;
        }

        std::uint64_t duration = response.expiry_unix - response.start_unix;
        std::uint64_t now = static_cast< std::uint64_t >( std::chrono::duration_cast< std::chrono::seconds >( std::chrono::system_clock::now( ).time_since_epoch( ) ).count( ) );
        std::uint64_t remaining = ( response.expiry_unix > now ) ? ( response.expiry_unix - now ) : 0;

        if ( remaining == 0 ) {
            response.success = false;
            response.message = "Key has expired.";
            return response;
        }

        response.message = std::format( "Subscription valid. Duration: {}s | Remaining: {}s", duration, remaining );
        return response;
    }
    VIRTUALIZE_MARK( authenticate );

    static VIRTUALIZE bool download_and_decrypt_module( const std::string& key, const std::string& hwid, const std::string& module_name, std::vector<std::uint8_t>& out_bytes ) {
        std::ostringstream mod_path;
        mod_path << "/module?key=" << key << "&hwid=" << hwid << "&name=" << module_name;
        std::string mod_path_str = mod_path.str( );
        std::wstring w_mod_path( mod_path_str.begin( ), mod_path_str.end( ) );

        std::vector<std::uint8_t> encrypted_data;
        if ( !http_download( L"http://mc.novacaine.wtf:1053", w_mod_path, encrypted_data ) ) {
            return false;
        }

        std::vector<std::uint8_t> xor_key;
        derive_xor_key( key, hwid, xor_key );
        xor_decrypt( encrypted_data, xor_key );

        out_bytes = std::move( encrypted_data );
        return true;
    }
    VIRTUALIZE_MARK( download_and_decrypt_module );

    VIRTUALIZE bool download_driver( const std::string& key, const std::string& hwid, const std::string& driver_name, std::vector<std::uint8_t>& out_bytes ) {
        std::ostringstream drv_path;
        drv_path << "/driver?key=" << key << "&hwid=" << hwid << "&name=" << driver_name;
        std::string drv_path_str = drv_path.str( );
        std::wstring w_drv_path( drv_path_str.begin( ), drv_path_str.end( ) );

        std::vector<std::uint8_t> encrypted_data;
        if ( !http_download( L"http://mc.novacaine.wtf:1053", w_drv_path, encrypted_data ) ) {
            return false;
        }

        std::vector<std::uint8_t> xor_key;
        derive_xor_key( key, hwid, xor_key );
        xor_decrypt( encrypted_data, xor_key );

        out_bytes = std::move( encrypted_data );
        return true;
    }
    VIRTUALIZE_MARK( download_driver );

    namespace pe {

        // -------------------------------------------------------------------------
        // Manifest parser: server sends a simple text format, client just follows it
        // Format: one directive per line, space-separated
        // BASE <image_base>  IMAGE <size>  EP <entry_point>
        // SEC <name> <rva> <size> <characteristics>
        // DLL <dll_name>
        // FN <func_name> <iat_rva>   or   FN ordinal <ordinal> <iat_rva>
        // TLS <callback_rva>
        // EXP <name> <rva> <ordinal>
        // RELOC <rva> <size>
        // -------------------------------------------------------------------------
        VIRTUALIZE static manifest_t parse_manifest( const std::string& text ) {
            manifest_t manifest;
            manifest_import_t* current_dll = nullptr;
            std::istringstream stream( text );
            std::string line;

            while ( std::getline( stream, line ) ) {
                if ( !line.empty( ) && line.back( ) == '\r' ) {
                    line.pop_back( );
                }
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
                    std::uint32_t rva;
                    line_stream >> rva;
                    manifest.tls_callbacks.push_back( rva );
                }
                else if ( cmd == "EXP" ) {
                    std::string name;
                    std::uint32_t rva;
                    std::uint16_t ordinal;
                    line_stream >> name >> rva >> ordinal;
                    manifest.exports.emplace_back( std::move( name ), rva, ordinal );
                }
                else if ( cmd == "RELOC" ) {
                    line_stream >> manifest.reloc_rva >> manifest.reloc_size;
                    manifest.has_relocations = true;
                }
            }
            return manifest;
        }
        VIRTUALIZE_MARK( parse_manifest );

        // -------------------------------------------------------------------------
        // Dumb executor: server did all parsing, client just follows instructions
        // -------------------------------------------------------------------------
        VIRTUALIZE mapped_module_t load_mapped( const std::vector<std::uint8_t>& mapped_bytes, const manifest_t& manifest ) {
            mapped_module_t result = { };
            result.manifest = manifest;

            // 1. Allocate at preferred base
            std::uint8_t* allocated = reinterpret_cast< std::uint8_t* >(
                VirtualAlloc( reinterpret_cast< void* >( manifest.image_base ), manifest.size_of_image, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) );

            std::uintptr_t delta = 0;
            if ( !allocated ) {
                allocated = reinterpret_cast< std::uint8_t* >(
                    VirtualAlloc( nullptr, manifest.size_of_image, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) );
                if ( !allocated ) {
                    return result;
                }
                delta = reinterpret_cast< std::uintptr_t >( allocated ) - manifest.image_base;
            }

            result.base = allocated;
            std::memcpy( allocated, mapped_bytes.data( ), mapped_bytes.size( ) );

            // 2. Fix ImageBase in headers if we relocated
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

            // 3. Second-pass relocation fix (only if fallback allocation)
            if ( delta != 0 && manifest.has_relocations ) {
                auto* reloc_dir = reinterpret_cast< IMAGE_BASE_RELOCATION* >( allocated + manifest.reloc_rva );
                while ( reloc_dir->VirtualAddress != 0 ) {
                    std::uintptr_t page = reinterpret_cast< std::uintptr_t >( allocated ) + reloc_dir->VirtualAddress;
                    WORD* entries = reinterpret_cast< WORD* >( reinterpret_cast< std::uint8_t* >( reloc_dir ) + sizeof( IMAGE_BASE_RELOCATION ) );
                    DWORD count = ( reloc_dir->SizeOfBlock - sizeof( IMAGE_BASE_RELOCATION ) ) / sizeof( WORD );
                    for ( DWORD i = 0; i < count; ++i ) {
                        WORD type = ( entries[ i ] >> 12 ) & 0xF;
                        WORD offset = entries[ i ] & 0xFFF;
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

            // 4. Resolve imports from manifest
            std::vector<HMODULE> loaded_modules;
            for ( const auto& imp : manifest.imports ) {
                HMODULE h_mod = LoadLibraryA( imp.dll.c_str( ) );
                loaded_modules.push_back( h_mod );
                for ( const auto& entry : imp.entries ) {
                    void* addr = nullptr;
                    if ( entry.name.empty( ) ) {
                        addr = GetProcAddress( h_mod, reinterpret_cast< LPCSTR >( static_cast< std::uintptr_t >( entry.ordinal ) ) );
                    }
                    else {
                        addr = GetProcAddress( h_mod, entry.name.c_str( ) );
                    }
                    *reinterpret_cast< void** >( allocated + entry.rva ) = addr;
                }
            }
            if ( !loaded_modules.empty( ) ) {
                result.import_modules = new HMODULE[ loaded_modules.size( ) ];
                result.import_module_count = loaded_modules.size( );
                std::memcpy( result.import_modules, loaded_modules.data( ), loaded_modules.size( ) * sizeof( HMODULE ) );
            }

            // 5. Apply section permissions from manifest
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

            // 6. TLS callbacks from manifest
            using tls_callback_t = VOID( WINAPI* )( PVOID, DWORD, PVOID );
            for ( auto rva : manifest.tls_callbacks ) {
                auto cb = reinterpret_cast< tls_callback_t >( allocated + rva );
                cb( allocated, DLL_PROCESS_ATTACH, nullptr );
            }

            // 7. DllMain( DLL_PROCESS_ATTACH ) from manifest
            using dllmain_t = BOOL( WINAPI* )( HINSTANCE, DWORD, LPVOID );
            if ( manifest.entry_point != 0 ) {
                auto entry = reinterpret_cast< dllmain_t >( allocated + manifest.entry_point );
                entry( reinterpret_cast< HINSTANCE >( allocated ), DLL_PROCESS_ATTACH, nullptr );
            }

            result.ok = true;
            return result;
        }
        VIRTUALIZE_MARK( load_mapped );

        VIRTUALIZE void* get_export( mapped_module_t& mod, const char* export_name ) {
            if ( !mod.ok || !mod.base ) return nullptr;
            for ( const auto& [name, rva, ordinal] : mod.manifest.exports ) {
                if ( name == export_name ) {
                    return mod.base + rva;
                }
            }
            return nullptr;
        }
        VIRTUALIZE_MARK( get_export );

        VIRTUALIZE void free_mapped( mapped_module_t& mod ) {
            if ( !mod.ok || !mod.base ) return;

            using dllmain_t = BOOL( WINAPI* )( HINSTANCE, DWORD, LPVOID );
            if ( mod.manifest.entry_point != 0 ) {
                auto entry = reinterpret_cast< dllmain_t >( mod.base + mod.manifest.entry_point );
                entry( reinterpret_cast< HINSTANCE >( mod.base ), DLL_PROCESS_DETACH, nullptr );
            }

            using tls_callback_t = VOID( WINAPI* )( PVOID, DWORD, PVOID );
            for ( auto rva : mod.manifest.tls_callbacks ) {
                auto cb = reinterpret_cast< tls_callback_t >( mod.base + rva );
                cb( mod.base, DLL_PROCESS_DETACH, nullptr );
            }

            VirtualFree( mod.base, 0, MEM_RELEASE );
            mod.base = nullptr;
            for ( size_t i = 0; i < mod.import_module_count; ++i ) {
                if ( mod.import_modules[ i ] ) FreeLibrary( mod.import_modules[ i ] );
            }
            delete[ ] mod.import_modules;
            mod.import_modules = nullptr;
            mod.import_module_count = 0;
            mod.ok = false;
        }
        VIRTUALIZE_MARK( free_mapped );
    }

    VIRTUALIZE auth_response_t authenticate_and_load( const std::string& key, const std::string& module_name, pe::mapped_module_t& out_module ) {
        auth_response_t response = { };

        std::string hwid = generate_hwid( );

        // --- Auth first ---
        std::ostringstream auth_builder;
        auth_builder << "/?key=" << key << "&hwid=" << hwid;
        std::string auth_path_str = auth_builder.str( );
        std::wstring w_auth_path( auth_path_str.begin( ), auth_path_str.end( ) );
        std::string auth_raw;
        if ( !http_get( L"http://mc.novacaine.wtf:1053", w_auth_path, auth_raw ) ) {
            response.message = "Failed to contact authentication server.";
            return response;
        }
        if ( !parse_json_bool( auth_raw, "success", response.success ) || !response.success ) {
            response.message = "Authentication failed: key invalid or expired.";
            return response;
        }
        if ( !parse_json_uint64( auth_raw, "start_unix", response.start_unix ) ||
            !parse_json_uint64( auth_raw, "expiry_unix", response.expiry_unix ) ) {
            response.message = "Invalid auth response.";
            return response;
        }
        std::uint64_t now = static_cast< std::uint64_t >( std::chrono::duration_cast< std::chrono::seconds >( std::chrono::system_clock::now( ).time_since_epoch( ) ).count( ) );
        if ( now > response.expiry_unix ) {
            response.success = false;
            response.message = "Key has expired.";
            return response;
        }

        // --- Download & decrypt module ---
        std::vector<std::uint8_t> decrypted_bytes;
        if ( !download_and_decrypt_module( key, hwid, module_name, decrypted_bytes ) ) {
            response.success = false;
            response.message = "Failed to download or decrypt module.";
            return response;
        }

        // --- Parse manifest + Map PE into memory ---
        if ( decrypted_bytes.size( ) < 4 ) {
            response.success = false;
            response.message = "Module response too short (missing manifest header).";
            return response;
        }
        std::uint32_t manifest_len = 0;
        std::memcpy( &manifest_len, decrypted_bytes.data( ), 4 );
        if ( decrypted_bytes.size( ) < 4 + manifest_len ) {
            response.success = false;
            std::ostringstream msg;
            msg << "Module response truncated (manifest incomplete). bytes="
                << decrypted_bytes.size( ) << " manifest_len=" << manifest_len;
            response.message = msg.str( );
            return response;
        }

        std::string manifest_text( reinterpret_cast< const char* >( decrypted_bytes.data( ) + 4 ), manifest_len );
        std::vector<std::uint8_t> mapped_bytes( decrypted_bytes.begin( ) + 4 + manifest_len, decrypted_bytes.end( ) );

        auto manifest = pe::parse_manifest( manifest_text );
        out_module = pe::load_mapped( mapped_bytes, manifest );

        if ( !out_module.ok ) {
            response.success = false;
            response.message = "Failed to map module into memory.";
            return response;
        }

        return response;
    }
    VIRTUALIZE_MARK( authenticate_and_load );
}