#pragma once

namespace prefetch {

    struct scca_hdr_t {
        uint32_t Version;       // offset  0: 17=XP, 23=Win7, 26=Win8, 30=Win10
        char     Sig[ 4 ];      // offset  4: "SCCA"
        uint32_t Unknown;       // offset  8
        uint32_t FileSize;      // offset 12
        wchar_t  ExeName[ 30 ]; // offset 16
        uint32_t Hash;          // offset 76
    };

    struct entry_t {
        std::wstring  ExeName;
        std::wstring  Path;
        uint32_t      Hash{};
        uint32_t      RunCount{};
        FILETIME      LastRun{};
        FILETIME      FileModified{};
    };

    using fn_decompress_ex_t = NTSTATUS( WINAPI* )( USHORT, PUCHAR, ULONG, PUCHAR, ULONG, PULONG, PVOID );

    // -----------------------------------------------------------------------
    // decompress_mam
    //
    // Handles both the plain MAM layout and the CRC-prefixed variant:
    //   Plain:    [ 'M','A','M', CompType, UncompSize(4), CompData... ]
    //   Prefixed: [ CRC(4), 'M','A','M', CompType, UncompSize(4), CompData... ]
    // -----------------------------------------------------------------------
    static std::vector<uint8_t> decompress_mam( const std::vector<uint8_t>& Data ) {
        if ( Data.size( ) < 8 )
            return {};

        size_t MamOff = 0;
        if ( Data[ 0 ] == 'M' && Data[ 1 ] == 'A' && Data[ 2 ] == 'M' ) {
            MamOff = 0;
        }
        else if ( Data.size( ) >= 12
            && Data[ 4 ] == 'M' && Data[ 5 ] == 'A' && Data[ 6 ] == 'M' ) {
            MamOff = 4;
        }
        else {
            return {};
        }

        uint8_t  CompType = Data[ MamOff + 3 ];
        uint32_t UncompSize = *reinterpret_cast< const uint32_t* >( Data.data( ) + MamOff + 4 );

        if ( UncompSize == 0 || UncompSize > 64 * 1024 * 1024 ) return {};

        uint16_t CompFormat{};
        switch ( CompType ) {
            case 0x02: CompFormat = 0x0002; break;
            case 0x03: CompFormat = 0x0003; break;
            case 0x04: CompFormat = 0x0004; break;
            default:   return {};
        }

        auto* Ntdll = GetModuleHandleW( L"ntdll.dll" );
        auto FnDecomp = reinterpret_cast< fn_decompress_ex_t >(
            GetProcAddress( Ntdll, "RtlDecompressBufferEx" ) );
        if ( !FnDecomp ) return {};

        std::vector<uint8_t> Ws( 65536 );
        std::vector<uint8_t> Out( UncompSize );
        ULONG FinalSize{};

        auto* CompData = const_cast< PUCHAR >( Data.data( ) ) + MamOff + 8;
        ULONG  CompSize = static_cast< ULONG >( Data.size( ) ) - static_cast< ULONG >( MamOff + 8 );

        NTSTATUS St = FnDecomp( CompFormat, Out.data( ), UncompSize,
            CompData, CompSize, &FinalSize, Ws.data( ) );

        if ( St != 0 || FinalSize == 0 ) return {};

        Out.resize( FinalSize );
        return Out;
    }

    static std::vector<uint8_t> read_pf( const std::wstring& Path ) {
        auto Hf = CreateFileW( Path.c_str( ), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
        if ( Hf == INVALID_HANDLE_VALUE ) return {};
        LARGE_INTEGER Size{};
        GetFileSizeEx( Hf, &Size );
        std::vector<uint8_t> Buf( static_cast< size_t >( Size.QuadPart ) );
        DWORD Read{};
        ReadFile( Hf, Buf.data( ), static_cast< DWORD >( Buf.size( ) ), &Read, nullptr );
        CloseHandle( Hf );
        Buf.resize( Read );
        return Buf;
    }

    static bool parse_scca( const std::vector<uint8_t>& Raw, entry_t& Out ) {
        if ( Raw.size( ) < sizeof( scca_hdr_t ) ) return false;
        const auto* Hdr = reinterpret_cast< const scca_hdr_t* >( Raw.data( ) );
        if ( Hdr->Sig[ 0 ] != 'S' || Hdr->Sig[ 1 ] != 'C' ||
            Hdr->Sig[ 2 ] != 'C' || Hdr->Sig[ 3 ] != 'A' )
            return false;

        Out.Hash = Hdr->Hash;
        Out.ExeName = std::wstring( Hdr->ExeName, wcsnlen( Hdr->ExeName, 30 ) );

        uint32_t LastRunOff{}, RunCountOff{};
        switch ( Hdr->Version ) {
            case 17: LastRunOff = 0x78; RunCountOff = 0x90; break;
            case 23: LastRunOff = 0x78; RunCountOff = 0x98; break;
            case 26: LastRunOff = 0x80; RunCountOff = 0x98; break;
            case 30: LastRunOff = 0x80; RunCountOff = 0xC8; break;
            default: LastRunOff = 0x80; RunCountOff = 0xC8; break;
        }

        if ( Raw.size( ) > LastRunOff + 8 )
            Out.LastRun = *reinterpret_cast< const FILETIME* >( Raw.data( ) + LastRunOff );
        if ( Raw.size( ) > RunCountOff + 4 )
            Out.RunCount = *reinterpret_cast< const uint32_t* >( Raw.data( ) + RunCountOff );

        return true;
    }

    // -----------------------------------------------------------------------
    // stop_sysmain / start_sysmain
    //
    // SysMain (Superfetch) holds an encrypted view of each .pf file while
    // running. Stopping it forces a plaintext flush to disk, making the files
    // readable. We restart it after enumerate() so the system isn't degraded.
    // -----------------------------------------------------------------------
    static bool stop_sysmain( ) {
        auto Scm = OpenSCManagerW( nullptr, nullptr, SC_MANAGER_CONNECT );
        if ( !Scm ) return false;
        auto Svc = OpenServiceW( Scm, L"SysMain",
            SERVICE_STOP | SERVICE_QUERY_STATUS );
        if ( !Svc ) { CloseServiceHandle( Scm ); return false; }

        SERVICE_STATUS Ss{};
        ControlService( Svc, SERVICE_CONTROL_STOP, &Ss );

        DWORD Elapsed = 0;
        while ( Elapsed < 8000 ) {
            QueryServiceStatus( Svc, &Ss );
            if ( Ss.dwCurrentState == SERVICE_STOPPED ) break;
            Sleep( 100 );
            Elapsed += 100;
        }

        CloseServiceHandle( Svc );
        CloseServiceHandle( Scm );
        return Ss.dwCurrentState == SERVICE_STOPPED;
    }

    static void start_sysmain( ) {
        auto Scm = OpenSCManagerW( nullptr, nullptr, SC_MANAGER_CONNECT );
        if ( !Scm ) return;
        auto Svc = OpenServiceW( Scm, L"SysMain", SERVICE_START );
        if ( Svc ) {
            StartServiceW( Svc, 0, nullptr );
            CloseServiceHandle( Svc );
        }
        CloseServiceHandle( Scm );
    }

    static bool sysmain_running( ) {
        auto Scm = OpenSCManagerW( nullptr, nullptr, SC_MANAGER_CONNECT );
        if ( !Scm ) return false;
        auto Svc = OpenServiceW( Scm, L"SysMain", SERVICE_QUERY_STATUS );
        if ( !Svc ) { CloseServiceHandle( Scm ); return false; }
        SERVICE_STATUS Ss{};
        QueryServiceStatus( Svc, &Ss );
        CloseServiceHandle( Svc );
        CloseServiceHandle( Scm );
        return Ss.dwCurrentState == SERVICE_RUNNING;
    }

    class c_prefetch {
    public:
        bool enumerate( ) {
            Entries_.clear( );

            const bool WasRunning = sysmain_running( );
            if ( WasRunning ) {
                if ( !stop_sysmain( ) )
                    logging::print( obf( "warning: sysmain did not stop cleanly" ) );
                Sleep( 300 );
            }

            WIN32_FIND_DATAW Fd{};
            auto Hf = FindFirstFileW( L"C:\\Windows\\Prefetch\\*.pf", &Fd );
            if ( Hf == INVALID_HANDLE_VALUE ) {
                if ( WasRunning ) start_sysmain( );
                return false;
            }

            DWORD Attempted{}, ReadFailed{}, DecompFailed{}, ParseFailed{};

            do {
                ++Attempted;
                std::wstring PfPath = std::wstring( L"C:\\Windows\\Prefetch\\" ) + Fd.cFileName;

                auto Raw = read_pf( PfPath );
                if ( Raw.size( ) < 8 ) { ++ReadFailed; continue; }

                const bool AllZero = std::all_of( Raw.begin( ),
                    Raw.begin( ) + min( Raw.size( ), size_t{ 16 } ),
                    [ ]( uint8_t B ) { return B == 0; } );
                if ( AllZero ) {
                    ++DecompFailed;
                    continue;
                }

                std::vector<uint8_t> Decompressed;

                const bool IsMamPlain = Raw[ 0 ] == 'M' && Raw[ 1 ] == 'A' && Raw[ 2 ] == 'M';
                const bool IsMamPrefixed = Raw.size( ) >= 12
                    && Raw[ 4 ] == 'M' && Raw[ 5 ] == 'A' && Raw[ 6 ] == 'M';
                const bool IsRawScca = Raw[ 0 ] == 'S' && Raw[ 1 ] == 'C'
                    && Raw[ 2 ] == 'C' && Raw[ 3 ] == 'A';

                if ( IsMamPlain || IsMamPrefixed ) {
                    Decompressed = decompress_mam( Raw );
                }
                else if ( IsRawScca ) {
                    Decompressed = Raw;
                }
                else {
                    logging::print( obf( "%ls unrecognised format -> raw[0-7]=%02X %02X %02X %02X %02X %02X %02X %02X" ),
                        Fd.cFileName,
                        Raw[ 0 ], Raw[ 1 ], Raw[ 2 ], Raw[ 3 ],
                        Raw[ 4 ], Raw[ 5 ], Raw[ 6 ], Raw[ 7 ] );
                    ++DecompFailed;
                    continue;
                }

                if ( Decompressed.empty( ) ) { ++DecompFailed; continue; }

                entry_t E{};
                E.Path = PfPath;
                E.FileModified = Fd.ftLastWriteTime;

                if ( parse_scca( Decompressed, E ) ) {
                    Entries_.emplace_back( std::move( E ) );
                }
                else {
                    ++ParseFailed;
                }
            } while ( FindNextFileW( Hf, &Fd ) );

            FindClose( Hf );

            if ( WasRunning ) {
                start_sysmain( );
            }

            return !Entries_.empty( );
        }

        bool wipe( ) {
            DWORD Found{}, Wiped{};

            WIN32_FIND_DATAW Fd{};
            auto Hf = FindFirstFileW( L"C:\\Windows\\Prefetch\\*.pf", &Fd );
            if ( Hf != INVALID_HANDLE_VALUE ) {
                do {
                    ++Found;
                    std::wstring PfPath = std::wstring( L"C:\\Windows\\Prefetch\\" ) + Fd.cFileName;
                    if ( DeleteFileW( PfPath.c_str( ) ) ) ++Wiped;
                } while ( FindNextFileW( Hf, &Fd ) );
                FindClose( Hf );
            }

            Hf = FindFirstFileW( L"C:\\Windows\\Prefetch\\*", &Fd );
            if ( Hf != INVALID_HANDLE_VALUE ) {
                do {
                    ++Found;
                    std::wstring PfPath = std::wstring( L"C:\\Windows\\Prefetch\\" ) + Fd.cFileName;
                    if ( DeleteFileW( PfPath.c_str( ) ) ) ++Wiped;
                } while ( FindNextFileW( Hf, &Fd ) );
                FindClose( Hf );
            }

            Entries_.clear( );
            return Wiped > 0;
        }

        const std::vector<entry_t>& entries( ) const { return Entries_; }

    private:
        std::vector<entry_t> Entries_;
    };
}