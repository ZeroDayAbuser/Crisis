#pragma once

namespace pdb {
    class ComInitializer {
    public:
        ComInitializer( ) {
            HRESULT hr = CoInitialize( NULL );
            initialized = SUCCEEDED( hr );
        }
        ~ComInitializer( ) {
            if ( initialized ) {
                CoUninitialize( );
            }
        }
    private:
        bool initialized;
    };

    static ComInitializer g_com_init;

    struct pdb_info {
        DWORD signature;
        GUID guid;
        DWORD age;
        char pdb_file_name[ 1 ];
    };

    class c_pdb {
    public:
        std::string m_module_bare;
        std::uint64_t m_module_base;
        std::uint32_t m_module_size;
        std::string m_pdb_path;
        std::vector<std::pair<std::string, std::uint64_t>> m_symbols;

        c_pdb( const char* module_name ) : m_module_name( module_name ) {
            std::string s = module_name;
            auto slash = s.find_last_of( "\\/" );
            if ( slash != std::string::npos ) {
                m_module_bare = s.substr( slash + 1 );
            }
            else {
                m_module_bare = s;
            }

            m_pe_path = get_pe_path( module_name );
        }

        ~c_pdb( ) {
            cleanup( );
        }

        bool load( ) {
            this->m_module_base = utility::get_kernel_module( m_module_bare.c_str( ) );
            if ( !m_module_base ) {
                std::string lower = m_module_bare;
                std::transform( lower.begin( ), lower.end( ), lower.begin( ),
                    [ ]( unsigned char c ) { return std::tolower( c ); } );
                logging::print( obf( "failed to find %s" ), lower.c_str( ) );
                return false;
            }

            this->m_module_size = utility::get_kernel_size( m_module_bare.c_str( ) );
            if ( !m_module_size ) {
                std::string lower = m_module_bare;
                std::transform( lower.begin( ), lower.end( ), lower.begin( ),
                    [ ]( unsigned char c ) { return std::tolower( c ); } );
                logging::print( obf( "failed to find %s size" ), lower.c_str( ) );
                return false;
            }

            m_pdb_path = find_cached_pdb( );
            if ( m_pdb_path.empty( ) ) {
                m_pdb_path = download_pdb( m_pe_path );
                if ( m_pdb_path.empty( ) ) {
                    return false;
                }
            }

            CComPtr<IDiaDataSource> pSource;
            HRESULT hr = CoCreateInstance( CLSID_DiaSource, NULL, CLSCTX_INPROC_SERVER,
                IID_IDiaDataSource, ( void** )&pSource );

            if ( FAILED( hr ) ) {
                const wchar_t* dia_versions[ ] = { L"msdia140.dll", L"msdia120.dll", L"msdia110.dll" };
                for ( const auto& dll : dia_versions ) {
                    hr = NoRegCoCreate( dll, CLSID_DiaSource, IID_IDiaDataSource, ( void** )&pSource );
                    if ( SUCCEEDED( hr ) ) break;
                }
                if ( FAILED( hr ) ) {
                    logging::print( obf( "failed to create DIA data source: 0x%08x" ), hr );
                    return false;
                }
            }

            std::wstring wide_path( m_pdb_path.begin( ), m_pdb_path.end( ) );
            hr = pSource->loadDataFromPdb( wide_path.c_str( ) );
            if ( FAILED( hr ) ) {
                logging::print( obf( "cached PDB load failed (0x%08x), re-downloading..." ), hr );
                DeleteFileA( m_pdb_path.c_str( ) );
                m_pdb_path = download_pdb( m_pe_path );
                if ( m_pdb_path.empty( ) ) return false;

                wide_path = std::wstring( m_pdb_path.begin( ), m_pdb_path.end( ) );
                hr = pSource->loadDataFromPdb( wide_path.c_str( ) );
                if ( FAILED( hr ) ) {
                    logging::print( obf( "failed to load re-downloaded PDB: 0x%08x" ), hr );
                    return false;
                }
            }

            CComPtr<IDiaSession> pSession;
            hr = pSource->openSession( &pSession );
            if ( FAILED( hr ) ) {
                logging::print( obf( "failed to open DIA session: 0x%08x" ), hr );
                return false;
            }

            CComPtr<IDiaSymbol> pGlobal;
            hr = pSession->get_globalScope( &pGlobal );
            if ( FAILED( hr ) ) {
                logging::print( obf( "failed to get global scope: 0x%08x" ), hr );
                return false;
            }

            m_pSource = pSource.Detach( );
            m_pSession = pSession.Detach( );
            m_pGlobal = pGlobal.Detach( );

            if ( !enumerate_symbols( ) ) {
                logging::print( obf( "failed to enumerate symbols" ) );
                cleanup( );
                return false;
            }

            enumerate_all_members( );
            enumerate_prototypes( );

            return true;
        }

        std::uint64_t get_symbol_address( const char* symbol_name ) {
            for ( const auto& symbol : m_symbols ) {
                if ( symbol.first == symbol_name )
                    return this->m_module_base + symbol.second;
            }
            return 0;
        }

        std::uint64_t find_member_in_udt( IDiaSymbol* pUDT, const std::wstring& member_name, LONG base_offset ) {
            CComPtr<IDiaEnumSymbols> pEnumData;
            if ( SUCCEEDED( pUDT->findChildren( SymTagData, member_name.c_str( ), nsfCaseInsensitive, &pEnumData ) ) && pEnumData ) {
                CComPtr<IDiaSymbol> pMember;
                ULONG celt = 0;
                if ( SUCCEEDED( pEnumData->Next( 1, &pMember, &celt ) ) && celt == 1 && pMember ) {
                    LONG offset = 0;
                    if ( SUCCEEDED( pMember->get_offset( &offset ) ) )
                        return static_cast< std::uint64_t >( base_offset + offset );
                }
            }

            CComPtr<IDiaEnumSymbols> pEnumBases;
            if ( FAILED( pUDT->findChildren( SymTagBaseClass, nullptr, nsNone, &pEnumBases ) ) || !pEnumBases )
                return 0;

            CComPtr<IDiaSymbol> pBase;
            ULONG celt = 0;
            while ( SUCCEEDED( pEnumBases->Next( 1, &pBase, &celt ) ) && celt == 1 ) {
                LONG base_off = 0;
                pBase->get_offset( &base_off );

                CComPtr<IDiaSymbol> pBaseType;
                if ( SUCCEEDED( pBase->get_type( &pBaseType ) ) && pBaseType ) {
                    auto result = find_member_in_udt( pBaseType, member_name, base_offset + base_off );
                    if ( result ) return result;
                }
                pBase.Release( );
            }

            return 0;
        }

        std::uint64_t get_bit_position( const char* type_name, const char* member_name ) {
            const auto key = std::string( type_name ) + "::" + member_name;
            auto it = m_bitpos_cache.find( key );
            if ( it != m_bitpos_cache.end( ) ) return it->second;

            if ( !m_pGlobal ) return 0;

            std::wstring w_type( type_name, type_name + strlen( type_name ) );
            std::wstring w_member( member_name, member_name + strlen( member_name ) );

            CComPtr<IDiaEnumSymbols> pEnumTypes;
            if ( FAILED( m_pGlobal->findChildren( SymTagUDT, w_type.c_str( ), nsfCaseInsensitive, &pEnumTypes ) ) || !pEnumTypes )
                return 0;

            CComPtr<IDiaSymbol> pUDT;
            ULONG celt = 0;
            if ( FAILED( pEnumTypes->Next( 1, &pUDT, &celt ) ) || celt != 1 || !pUDT )
                return 0;

            CComPtr<IDiaEnumSymbols> pEnumData;
            if ( FAILED( pUDT->findChildren( SymTagData, w_member.c_str( ), nsfCaseInsensitive, &pEnumData ) ) || !pEnumData )
                return 0;

            CComPtr<IDiaSymbol> pMember;
            if ( FAILED( pEnumData->Next( 1, &pMember, &celt ) ) || celt != 1 || !pMember )
                return 0;

            DWORD bit_pos = 0;
            if ( FAILED( pMember->get_bitPosition( &bit_pos ) ) )
                return 0;

            const auto result = static_cast<std::uint64_t>( bit_pos );
            m_bitpos_cache.emplace( key, result );
            return result;
        }

        std::uint64_t get_struct_size( const char* type_name ) {
            const auto key = std::string( type_name );
            auto it = m_size_cache.find( key );
            if ( it != m_size_cache.end( ) ) return it->second;

            if ( !m_pGlobal ) return 0;

            std::wstring w_type( type_name, type_name + strlen( type_name ) );

            CComPtr<IDiaEnumSymbols> pEnumTypes;
            if ( FAILED( m_pGlobal->findChildren( SymTagUDT, w_type.c_str( ), nsfCaseInsensitive, &pEnumTypes ) ) || !pEnumTypes )
                return 0;

            CComPtr<IDiaSymbol> pUDT;
            ULONG celt = 0;
            if ( FAILED( pEnumTypes->Next( 1, &pUDT, &celt ) ) || celt != 1 || !pUDT )
                return 0;

            ULONGLONG size = 0;
            if ( FAILED( pUDT->get_length( &size ) ) )
                return 0;

            const auto result = static_cast<std::uint64_t>( size );
            if ( result ) m_size_cache.emplace( key, result );
            return result;
        }

        std::uint64_t get_struct_member( const char* type_name, const char* member_name ) {
            const auto key = std::string( type_name ) + "::" + member_name;
            auto it = m_member_cache.find( key );
            if ( it != m_member_cache.end( ) ) return it->second;

            if ( !m_pGlobal ) return 0;

            std::wstring w_type( type_name, type_name + strlen( type_name ) );
            std::wstring w_member( member_name, member_name + strlen( member_name ) );

            CComPtr<IDiaEnumSymbols> pEnumTypes;
            if ( FAILED( m_pGlobal->findChildren( SymTagUDT, w_type.c_str( ), nsfCaseInsensitive, &pEnumTypes ) ) || !pEnumTypes )
                return 0;

            CComPtr<IDiaSymbol> pUDT;
            ULONG celt = 0;
            if ( FAILED( pEnumTypes->Next( 1, &pUDT, &celt ) ) || celt != 1 || !pUDT )
                return 0;

            const auto result = find_member_in_udt( pUDT, w_member, 0 );
            if ( result ) m_member_cache.emplace( key, result );
            return result;
        }

        void dump_struct_members( const char* type_name ) {
            if ( !m_pGlobal ) return;
            std::wstring w_type( type_name, type_name + strlen( type_name ) );

            CComPtr<IDiaEnumSymbols> pEnumTypes;
            if ( FAILED( m_pGlobal->findChildren( SymTagUDT, w_type.c_str( ), nsfCaseInsensitive, &pEnumTypes ) ) || !pEnumTypes )
                return;

            CComPtr<IDiaSymbol> pUDT;
            ULONG celt = 0;
            if ( FAILED( pEnumTypes->Next( 1, &pUDT, &celt ) ) || celt != 1 || !pUDT )
                return;

            CComPtr<IDiaEnumSymbols> pChildren;
            if ( FAILED( pUDT->findChildren( SymTagNull, nullptr, nsNone, &pChildren ) ) || !pChildren )
                return;

            CComPtr<IDiaSymbol> pChild;
            while ( SUCCEEDED( pChildren->Next( 1, &pChild, &celt ) ) && celt == 1 ) {
                BSTR name = nullptr;
                LONG offset = 0;
                pChild->get_name( &name );
                pChild->get_offset( &offset );
                if ( name ) {
                    char buf[ 256 ]{};
                    size_t n = 0;
                    wcstombs_s( &n, buf, name, sizeof( buf ) - 1 );
                    logging::print( obf( "  [0x%x] %s" ), offset, buf );
                    SysFreeString( name );
                }
                pChild.Release( );
            }
        }

        std::uint64_t get_symbol_rva( const char* symbol_name ) {
            return get_symbol_address( symbol_name );
        }

        std::vector<std::uint8_t> compress_to_memory( ) {
            std::vector<std::uint8_t> blob( sizeof( pdb_header_t ), 0 );
            std::uint32_t count = 0;

            auto append_entry = [&]( std::uint8_t type, const std::string& name, std::uint64_t value ) {
                const auto name_len = static_cast<std::uint8_t>(
                    ( std::min )( name.size( ), std::size_t{ 255 } ) );
                blob.push_back( type );
                blob.push_back( name_len );
                blob.insert( blob.end( ), name.data( ), name.data( ) + name_len );
                const auto* vb = reinterpret_cast<const std::uint8_t*>( &value );
                blob.insert( blob.end( ), vb, vb + sizeof( std::uint64_t ) );
                ++count;
            };

            const std::string base_key = "ntoskrnl_base";
            append_entry( 0, base_key, m_module_base );

            for ( const auto& sym : m_symbols )
                append_entry( 0, sym.first, m_module_base + sym.second );

            for ( const auto& [key, val] : m_member_cache )
                append_entry( 1, key, val );

            for ( const auto& [key, val] : m_size_cache )
                append_entry( 2, key, val );

            for ( const auto& [key, val] : m_bitpos_cache )
                append_entry( 3, key, val );

            for ( const auto& [name, params] : m_proto_cache ) {
                append_entry( 4, name, static_cast< std::uint64_t >( params.size( ) ) );
                for ( std::size_t i = 0; i < params.size( ); ++i )
                    append_entry( 5, name + "::" + params[ i ],
                        static_cast< std::uint64_t >( i ) );
            }

            auto* hdr = reinterpret_cast<pdb_header_t*>( blob.data( ) );
            hdr->magic     = 0x42445043u;
            hdr->version   = 1u;
            hdr->count     = count;
            hdr->data_size = static_cast<std::uint32_t>( blob.size( ) );

            return blob;
        }

    private:
        struct pdb_header_t {
            std::uint32_t magic;
            std::uint32_t version;
            std::uint32_t count;
            std::uint32_t data_size;
        };

        std::map<std::string, std::uint64_t> m_member_cache;
        std::map<std::string, std::uint64_t> m_size_cache;
        std::map<std::string, std::uint64_t> m_bitpos_cache;
        std::map<std::string, std::vector<std::string>> m_proto_cache;

        std::string m_module_name;
        std::string m_pe_path;

        IDiaDataSource* m_pSource = nullptr;
        IDiaSession* m_pSession = nullptr;
        IDiaSymbol* m_pGlobal = nullptr;

        std::string get_pe_path( const char* module_name ) {
            return std::string( std::getenv( "systemroot" ) ) + "\\System32\\" + module_name;
        }

        std::string get_module_prefix( ) {
            std::string prefix = m_module_bare;
            auto dot = prefix.rfind( '.' );
            if ( dot != std::string::npos )
                prefix = prefix.substr( 0, dot );
            for ( auto& c : prefix )
                c = static_cast< char >( tolower( static_cast< unsigned char >( c ) ) );
            return prefix;
        }

        std::string find_cached_pdb( ) {
            char sz_dir[ MAX_PATH ] = { };
            if ( !GetCurrentDirectoryA( sizeof( sz_dir ), sz_dir ) )
                return "";

            std::string pattern = std::string( sz_dir ) + "\\" + get_module_prefix( ) + "_*.pdb";

            WIN32_FIND_DATAA fd = { };
            HANDLE h = FindFirstFileA( pattern.c_str( ), &fd );
            if ( h == INVALID_HANDLE_VALUE )
                return "";

            std::string cached = std::string( sz_dir ) + "\\" + fd.cFileName;
            FindClose( h );

            std::ifstream v( cached, std::ios::binary | std::ios::ate );
            if ( !v.is_open( ) || v.tellg( ) < 1024 )
                return "";

            return cached;
        }

        std::string download_pdb( const std::string& pe_path ) {
            if ( pe_path.empty( ) ) {
                logging::print( obf( "pe path is empty" ) );
                return "";
            }

            char sz_download_dir[ MAX_PATH ] = { };
            if ( !GetCurrentDirectoryA( sizeof( sz_download_dir ), sz_download_dir ) ) {
                logging::print( obf( "failed to get current directory" ) );
                return "";
            }

            std::string download_path = sz_download_dir;
            if ( download_path.back( ) != '\\' )
                download_path += "\\";

            std::ifstream file( pe_path, std::ios::binary | std::ios::ate );
            if ( !file.is_open( ) ) {
                return "";
            }

            auto size = file.tellg( );
            if ( size <= 0 ) {
                logging::print( obf( "PE file is empty" ) );
                return "";
            }

            file.seekg( 0, std::ios::beg );
            std::vector<char> buffer( size );
            if ( !file.read( buffer.data( ), size ) ) {
                logging::print( obf( "failed to read PE file" ) );
                return "";
            }

            if ( buffer.size( ) < sizeof( IMAGE_DOS_HEADER ) ) {
                logging::print( obf( "file too small" ) );
                return "";
            }

            auto p_dos = reinterpret_cast< IMAGE_DOS_HEADER* >( buffer.data( ) );
            if ( p_dos->e_magic != IMAGE_DOS_SIGNATURE ) {
                logging::print( obf( "invalid DOS signature" ) );
                return "";
            }

            if ( buffer.size( ) < static_cast< size_t >( p_dos->e_lfanew ) + sizeof( IMAGE_NT_HEADERS ) ) {
                logging::print( obf( "invalid PE structure" ) );
                return "";
            }

            auto p_nt = reinterpret_cast< IMAGE_NT_HEADERS* >( buffer.data( ) + p_dos->e_lfanew );
            if ( p_nt->Signature != IMAGE_NT_SIGNATURE ) {
                logging::print( obf( "invalid NT signature" ) );
                return "";
            }

            bool is_x86 = ( p_nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 );
            auto p_opt32 = reinterpret_cast< IMAGE_OPTIONAL_HEADER32* >( &p_nt->OptionalHeader );
            auto p_opt64 = reinterpret_cast< IMAGE_OPTIONAL_HEADER64* >( &p_nt->OptionalHeader );
            auto image_size = is_x86 ? p_opt32->SizeOfImage : p_opt64->SizeOfImage;

            auto image_buffer = std::make_unique<BYTE[ ]>( image_size );
            if ( !image_buffer ) {
                logging::print( obf( "failed to allocate memory" ) );
                return "";
            }

            auto headers_size = is_x86 ? p_opt32->SizeOfHeaders : p_opt64->SizeOfHeaders;
            std::memcpy( image_buffer.get( ), buffer.data( ), headers_size );

            auto p_section = IMAGE_FIRST_SECTION( p_nt );
            for ( UINT i = 0; i < p_nt->FileHeader.NumberOfSections; ++i, ++p_section ) {
                if ( p_section->SizeOfRawData ) {
                    std::memcpy( image_buffer.get( ) + p_section->VirtualAddress,
                        buffer.data( ) + p_section->PointerToRawData,
                        p_section->SizeOfRawData );
                }
            }

            IMAGE_DATA_DIRECTORY* p_data_dir = is_x86
                ? &p_opt32->DataDirectory[ IMAGE_DIRECTORY_ENTRY_DEBUG ]
                : &p_opt64->DataDirectory[ IMAGE_DIRECTORY_ENTRY_DEBUG ];

            if ( !p_data_dir->Size ) {
                logging::print( obf( "no debug directory found" ) );
                return "";
            }

            auto p_debug_dir = reinterpret_cast< IMAGE_DEBUG_DIRECTORY* >(
                image_buffer.get( ) + p_data_dir->VirtualAddress );

            if ( p_debug_dir->Type != IMAGE_DEBUG_TYPE_CODEVIEW ) {
                logging::print( obf( "not CodeView debug type" ) );
                return "";
            }

            auto pdb_info_ptr = reinterpret_cast< pdb_info* >(
                image_buffer.get( ) + p_debug_dir->AddressOfRawData );

            if ( pdb_info_ptr->signature != 0x53445352 ) {
                logging::print( obf( "invalid PDB signature" ) );
                return "";
            }

            wchar_t w_guid[ 100 ] = { };
            if ( !StringFromGUID2( pdb_info_ptr->guid, w_guid, 100 ) ) {
                logging::print( obf( "failed to convert GUID" ) );
                return "";
            }

            char a_guid[ 100 ] = { };
            size_t l_guid = 0;
            wcstombs_s( &l_guid, a_guid, w_guid, sizeof( a_guid ) );

            char guid_filtered[ 256 ] = { };
            for ( size_t i = 0; i < l_guid; ++i ) {
                char c = a_guid[ i ];
                if ( ( c >= '0' && c <= '9' ) || ( c >= 'A' && c <= 'F' ) || ( c >= 'a' && c <= 'f' ) )
                    guid_filtered[ strlen( guid_filtered ) ] = c;
            }

            char age[ 16 ] = { };
            sprintf_s( age, "%X", pdb_info_ptr->age );

            std::string url = "https://msdl.microsoft.com/download/symbols/";
            url += pdb_info_ptr->pdb_file_name;
            url += "/";
            url += guid_filtered;
            url += age;
            url += "/";
            url += pdb_info_ptr->pdb_file_name;

            std::string pdb_path = download_path + get_module_prefix( ) + "_" + guid_filtered + age + ".pdb";
            HRESULT hr = URLDownloadToFileA( NULL, url.c_str( ), pdb_path.c_str( ), NULL, NULL );
            if ( FAILED( hr ) ) {
                logging::print( obf( "download failed: 0x%08x" ), hr );
                return "";
            }

            std::ifstream verify( pdb_path, std::ios::binary | std::ios::ate );
            if ( !verify.is_open( ) || verify.tellg( ) < 1024 ) {
                logging::print( obf( "downloaded PDB is invalid or too small" ) );
                DeleteFileA( pdb_path.c_str( ) );
                return "";
            }
            verify.close( );

            return pdb_path;
        }

        void enumerate_prototypes( ) {
            if ( !m_pGlobal ) return;

            auto collect = [ & ]( IDiaSymbol* pFunc ) {
                BSTR bname = nullptr;
                if ( FAILED( pFunc->get_name( &bname ) ) || !bname ) return;

                char func_name[ 1024 ] = { };
                size_t n = 0;
                wcstombs_s( &n, func_name, bname, sizeof( func_name ) - 1 );
                SysFreeString( bname );

                if ( n == 0 || func_name[ 0 ] == '\0' ) return;

                CComPtr<IDiaSymbol> pType;
                if ( FAILED( pFunc->get_type( &pType ) ) || !pType ) return;

                DWORD tag = 0;
                if ( FAILED( pType->get_symTag( &tag ) ) || tag != SymTagFunctionType ) return;

                std::vector<std::string> names;

                CComPtr<IDiaEnumSymbols> pParams;
                if ( SUCCEEDED( pFunc->findChildren( SymTagData, nullptr, nsNone, &pParams ) ) && pParams ) {
                    CComPtr<IDiaSymbol> pParam;
                    ULONG c = 0;
                    while ( SUCCEEDED( pParams->Next( 1, &pParam, &c ) ) && c == 1 ) {
                        DWORD kind = 0;
                        if ( SUCCEEDED( pParam->get_dataKind( &kind ) ) && kind == DataIsParam ) {
                            BSTR pn = nullptr;
                            if ( SUCCEEDED( pParam->get_name( &pn ) ) && pn ) {
                                char pbuf[ 256 ] = { };
                                size_t m = 0;
                                wcstombs_s( &m, pbuf, pn, sizeof( pbuf ) - 1 );
                                SysFreeString( pn );
                                if ( m ) names.emplace_back( pbuf );
                            }
                        }
                        pParam.Release( );
                    }
                }

                if ( names.empty( ) ) {
                    CComPtr<IDiaEnumSymbols> pArgs;
                    if ( SUCCEEDED( pType->findChildren( SymTagFunctionArgType, nullptr, nsNone, &pArgs ) ) && pArgs ) {
                        CComPtr<IDiaSymbol> pArg;
                        ULONG ac = 0;
                        while ( SUCCEEDED( pArgs->Next( 1, &pArg, &ac ) ) && ac == 1 ) {
                            char pbuf[ 32 ] = { };
                            _snprintf( pbuf, sizeof( pbuf ) - 1, "p%zu", names.size( ) );
                            names.emplace_back( pbuf );
                            pArg.Release( );
                        }
                    }
                }

                if ( names.empty( ) ) return;
                m_proto_cache.emplace( func_name, std::move( names ) );
            };

            CComPtr<IDiaEnumSymbols> pCompilands;
            if ( SUCCEEDED( m_pGlobal->findChildren( SymTagCompiland, nullptr, nsNone, &pCompilands ) ) && pCompilands ) {
                CComPtr<IDiaSymbol> pCompiland;
                ULONG c = 0;
                while ( SUCCEEDED( pCompilands->Next( 1, &pCompiland, &c ) ) && c == 1 ) {
                    CComPtr<IDiaEnumSymbols> pFuncs;
                    if ( SUCCEEDED( pCompiland->findChildren( SymTagFunction, nullptr, nsNone, &pFuncs ) ) && pFuncs ) {
                        CComPtr<IDiaSymbol> pFunc;
                        ULONG fc = 0;
                        while ( SUCCEEDED( pFuncs->Next( 1, &pFunc, &fc ) ) && fc == 1 ) {
                            collect( pFunc );
                            pFunc.Release( );
                        }
                    }
                    pCompiland.Release( );
                }
            }
        }

        void enumerate_all_members( ) {
            if ( !m_pGlobal ) return;

            CComPtr<IDiaEnumSymbols> pEnumUDTs;
            if ( FAILED( m_pGlobal->findChildren( SymTagUDT, nullptr, nsNone, &pEnumUDTs ) ) || !pEnumUDTs )
                return;

            CComPtr<IDiaSymbol> pUDT;
            ULONG celt = 0;
            while ( SUCCEEDED( pEnumUDTs->Next( 1, &pUDT, &celt ) ) && celt == 1 ) {
                BSTR udt_bname = nullptr;
                if ( FAILED( pUDT->get_name( &udt_bname ) ) || !udt_bname ) {
                    pUDT.Release( );
                    continue;
                }

                char type_name[ 256 ] = {};
                size_t tn = 0;
                wcstombs_s( &tn, type_name, udt_bname, sizeof( type_name ) - 1 );
                SysFreeString( udt_bname );

                if ( tn == 0 || type_name[ 0 ] == '\0' ) {
                    pUDT.Release( );
                    continue;
                }

                const std::string type_key( type_name );

                ULONGLONG udt_size = 0;
                if ( SUCCEEDED( pUDT->get_length( &udt_size ) ) && udt_size > 0 )
                    m_size_cache.emplace( type_key, static_cast<std::uint64_t>( udt_size ) );

                CComPtr<IDiaEnumSymbols> pEnumMembers;
                if ( SUCCEEDED( pUDT->findChildren( SymTagData, nullptr, nsNone, &pEnumMembers ) ) && pEnumMembers ) {
                    CComPtr<IDiaSymbol> pMember;
                    ULONG mcelt = 0;
                    while ( SUCCEEDED( pEnumMembers->Next( 1, &pMember, &mcelt ) ) && mcelt == 1 ) {
                        BSTR mem_bname = nullptr;
                        if ( FAILED( pMember->get_name( &mem_bname ) ) || !mem_bname ) {
                            pMember.Release( );
                            continue;
                        }

                        char mem_name[ 256 ] = {};
                        size_t mn = 0;
                        wcstombs_s( &mn, mem_name, mem_bname, sizeof( mem_name ) - 1 );
                        SysFreeString( mem_bname );

                        if ( mn == 0 || mem_name[ 0 ] == '\0' ) {
                            pMember.Release( );
                            continue;
                        }

                        const auto member_key = type_key + "::" + mem_name;

                        LONG offset = 0;
                        if ( SUCCEEDED( pMember->get_offset( &offset ) ) )
                            m_member_cache.emplace( member_key, static_cast<std::uint64_t>( static_cast<std::uint32_t>( offset ) ) );

                        DWORD loc_type = 0;
                        DWORD bit_pos  = 0;
                        if ( SUCCEEDED( pMember->get_locationType( &loc_type ) ) &&
                             loc_type == LocIsBitField &&
                             SUCCEEDED( pMember->get_bitPosition( &bit_pos ) ) )
                            m_bitpos_cache.emplace( member_key, static_cast<std::uint64_t>( bit_pos ) );

                        pMember.Release( );
                    }
                }

                pUDT.Release( );
            }
        }

        bool enumerate_symbols( ) {
            if ( !m_pGlobal ) return false;

            // Pass 1: lexical tree (private functions like MiAllocateVad, MmQueryVirtualMemory, …)
            {
                CComPtr<IDiaEnumSymbols> pEnumSymbols;
                HRESULT hr = m_pGlobal->findChildren( SymTagNull, NULL, nsNone, &pEnumSymbols );
                if ( FAILED( hr ) ) {
                    logging::print( obf( "failed to enumerate: 0x%08x" ), hr );
                    return false;
                }

                CComPtr<IDiaSymbol> pSymbol;
                ULONG celt = 0;
                while ( SUCCEEDED( pEnumSymbols->Next( 1, &pSymbol, &celt ) ) && celt == 1 ) {
                    BSTR name;
                    if ( SUCCEEDED( pSymbol->get_name( &name ) ) ) {
                        DWORD rva = 0;
                        if ( SUCCEEDED( pSymbol->get_relativeVirtualAddress( &rva ) ) && rva ) {
                            char symbol_name[ 1024 ] = { };
                            size_t converted = 0;
                            wcstombs_s( &converted, symbol_name, name, sizeof( symbol_name ) - 1 );
                            if ( converted > 0 && symbol_name[ 0 ] != '\0' )
                                m_symbols.emplace_back( symbol_name, rva );
                        }
                        SysFreeString( name );
                    }
                    pSymbol.Release( );
                }
            }

            // Pass 2: public symbol stream (exported functions like ExAllocatePoolWithTag,
            // MmGetPhysicalAddress, etc. live here and are NOT in the lexical tree above).
            {
                CComPtr<IDiaEnumSymbols> pPub;
                if ( SUCCEEDED( m_pGlobal->findChildren( SymTagPublicSymbol, NULL, nsNone, &pPub ) ) && pPub ) {
                    CComPtr<IDiaSymbol> pSym;
                    ULONG c = 0;
                    while ( SUCCEEDED( pPub->Next( 1, &pSym, &c ) ) && c == 1 ) {
                        BSTR bname;
                        if ( SUCCEEDED( pSym->get_name( &bname ) ) ) {
                            DWORD rva = 0;
                            if ( SUCCEEDED( pSym->get_relativeVirtualAddress( &rva ) ) && rva ) {
                                char sym_buf[ 1024 ] = { };
                                size_t n = 0;
                                wcstombs_s( &n, sym_buf, bname, sizeof( sym_buf ) - 1 );
                                if ( n > 0 && sym_buf[ 0 ] != '\0' ) {
                                    // Private symbol (pass 1) takes priority; only add if not seen yet
                                    bool exists = false;
                                    for ( const auto& s : m_symbols )
                                        if ( s.first == sym_buf ) { exists = true; break; }
                                    if ( !exists )
                                        m_symbols.emplace_back( sym_buf, rva );
                                }
                            }
                            SysFreeString( bname );
                        }
                        pSym.Release( );
                    }
                }
            }

            // Pass 3: EAT fallback — parse the on-disk PE export table.
            // Catches any symbol the DIA passes above missed (e.g. forwarders or
            // imports that appear as public symbols on some DIA versions).
            enumerate_exports( );

            return !m_symbols.empty( );
        }

        // Walk the on-disk PE's export address table and add any names not yet in m_symbols.
        void enumerate_exports( ) {
            std::ifstream f( m_pe_path, std::ios::binary | std::ios::ate );
            if ( !f.is_open( ) ) return;

            const auto file_size = static_cast< std::size_t >( f.tellg( ) );
            if ( file_size < sizeof( IMAGE_DOS_HEADER ) ) return;
            f.seekg( 0 );

            std::vector<char> buf( file_size );
            if ( !f.read( buf.data( ), file_size ) ) return;

            auto* dos = reinterpret_cast< IMAGE_DOS_HEADER* >( buf.data( ) );
            if ( dos->e_magic != IMAGE_DOS_SIGNATURE ) return;

            auto nt_off = static_cast< std::size_t >( dos->e_lfanew );
            if ( nt_off + sizeof( IMAGE_NT_HEADERS64 ) > file_size ) return;

            auto* nt = reinterpret_cast< IMAGE_NT_HEADERS64* >( buf.data( ) + nt_off );
            if ( nt->Signature != IMAGE_NT_SIGNATURE ) return;
            if ( nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ) return;

            auto& exp_dir_entry = nt->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT ];
            if ( !exp_dir_entry.VirtualAddress || !exp_dir_entry.Size ) return;

            // helper: RVA → file offset (raw)
            auto rva_to_off = [ & ]( DWORD rva ) -> std::size_t {
                auto* sec = IMAGE_FIRST_SECTION( nt );
                for ( WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec ) {
                    if ( rva >= sec->VirtualAddress &&
                         rva <  sec->VirtualAddress + sec->Misc.VirtualSize )
                        return sec->PointerToRawData + ( rva - sec->VirtualAddress );
                }
                return static_cast< std::size_t >( -1 );
            };

            auto exp_off = rva_to_off( exp_dir_entry.VirtualAddress );
            if ( exp_off == static_cast< std::size_t >( -1 ) ||
                 exp_off + sizeof( IMAGE_EXPORT_DIRECTORY ) > file_size ) return;

            auto* exp = reinterpret_cast< IMAGE_EXPORT_DIRECTORY* >( buf.data( ) + exp_off );

            auto names_off   = rva_to_off( exp->AddressOfNames );
            auto ordinals_off= rva_to_off( exp->AddressOfNameOrdinals );
            auto funcs_off   = rva_to_off( exp->AddressOfFunctions );
            if ( names_off    == static_cast< std::size_t >( -1 ) ||
                 ordinals_off == static_cast< std::size_t >( -1 ) ||
                 funcs_off    == static_cast< std::size_t >( -1 ) ) return;

            auto* names     = reinterpret_cast< DWORD*  >( buf.data( ) + names_off    );
            auto* ordinals  = reinterpret_cast< WORD*   >( buf.data( ) + ordinals_off );
            auto* functions = reinterpret_cast< DWORD*  >( buf.data( ) + funcs_off    );

            for ( DWORD i = 0; i < exp->NumberOfNames; ++i ) {
                auto name_off = rva_to_off( names[ i ] );
                if ( name_off == static_cast< std::size_t >( -1 ) ) continue;
                const char* fn_name = buf.data( ) + name_off;

                WORD ord = ordinals[ i ];
                if ( ord >= exp->NumberOfFunctions ) continue;
                DWORD fn_rva = functions[ ord ];
                if ( !fn_rva ) continue;

                // skip forwarder RVAs (they point inside the export directory)
                if ( fn_rva >= exp_dir_entry.VirtualAddress &&
                     fn_rva <  exp_dir_entry.VirtualAddress + exp_dir_entry.Size ) continue;

                bool exists = false;
                for ( const auto& s : m_symbols )
                    if ( s.first == fn_name ) { exists = true; break; }
                if ( !exists )
                    m_symbols.emplace_back( fn_name, static_cast< std::uint64_t >( fn_rva ) );
            }
        }

        void cleanup( ) {
            if ( m_pGlobal ) { m_pGlobal->Release( );  m_pGlobal = nullptr; }
            if ( m_pSession ) { m_pSession->Release( ); m_pSession = nullptr; }
            if ( m_pSource ) { m_pSource->Release( );  m_pSource = nullptr; }
        }
    };
}