#pragma once

namespace dbgmon {
    struct buffer_t {
        DWORD pid;
        char  msg[ 4096 - sizeof( DWORD ) ];
    };

    static constexpr const char* k_prefixes[] = {
        "[crisis-bypass]",
        "[crisis-overlay]",
        "[crisis-memory]",
        "[crisis-external]",
		"[crisis-injector]",
    };

    struct state_t {
        std::mutex                        mtx;
        std::vector< std::string >        pending;
        std::unordered_map< std::string,
            std::chrono::steady_clock::time_point > seen;
        std::atomic< bool >               ready { false };
    };
    inline state_t g_state;

    static std::string_view content_key( std::string_view s ) {
        if ( s.size( ) > 12 && s[ 0 ] == '[' && s[ 9 ] == ']' && s[ 10 ] == ' ' )
            return s.substr( 11 );
        return s;
    }

    static void monitor_thread( ) {
        HANDLE hmap = CreateFileMappingA(
            INVALID_HANDLE_VALUE, nullptr,
            PAGE_READWRITE, 0, static_cast< DWORD >( sizeof( buffer_t ) ),
            "DBWIN_BUFFER" );
        if ( !hmap ) { g_state.ready.store( true, std::memory_order_release ); return; }

        const auto* buf = static_cast< const buffer_t* >(
            MapViewOfFile( hmap, FILE_MAP_READ, 0, 0, 0 ) );
        if ( !buf ) {
            CloseHandle( hmap );
            g_state.ready.store( true, std::memory_order_release );
            return;
        }

        HANDLE hready = CreateEventA( nullptr, FALSE, FALSE, "DBWIN_BUFFER_READY" );
        HANDLE hdata  = CreateEventA( nullptr, FALSE, FALSE, "DBWIN_DATA_READY"   );

        if ( !hready || !hdata ) {
            if ( hready ) CloseHandle( hready );
            if ( hdata  ) CloseHandle( hdata  );
            UnmapViewOfFile( buf ); CloseHandle( hmap );
            g_state.ready.store( true, std::memory_order_release );
            return;
        }

        SetEvent( hready );
        g_state.ready.store( true, std::memory_order_release );

        while ( true ) {
            if ( WaitForSingleObject( hdata, 200 ) != WAIT_OBJECT_0 )
                continue;

            char snap[ sizeof( buffer_t::msg ) ];
            memcpy( snap, buf->msg, sizeof( snap ) );
            snap[ sizeof( snap ) - 1 ] = '\0';

            SetEvent( hready );

            bool match = false;
            for ( const auto* p : k_prefixes ) {
                if ( strstr( snap, p ) ) { match = true; break; }
            }
            if ( !match ) continue;

            auto len = static_cast< int >( strlen( snap ) );
            while ( len > 0 && ( snap[ len - 1 ] == '\n' || snap[ len - 1 ] == '\r' ) )
                snap[ --len ] = '\0';
            if ( len == 0 ) continue;

            std::string full( snap, static_cast< std::size_t >( len ) );
            std::string key( content_key( full ) );

            {
                std::lock_guard< std::mutex > lock( g_state.mtx );
                const auto now = std::chrono::steady_clock::now( );
                auto [ it, inserted ] = g_state.seen.emplace( key, now );
                if ( !inserted ) {
                    const float elapsed = std::chrono::duration< float >(
                        now - it->second ).count( );
                    if ( elapsed < 1.0f ) continue;
                    it->second = now;
                }
                g_state.pending.push_back( std::move( full ) );
            }
        }
    }

    inline std::vector< std::string > drain( ) {
        std::lock_guard< std::mutex > lock( g_state.mtx );
        return std::exchange( g_state.pending, { } );
    }

    struct tag_t { const char* text; const char* color; };

    static tag_t classify( std::string_view line ) {
        std::string lo( line.size( ), '\0' );
        for ( std::size_t i = 0; i < line.size( ); ++i )
            lo[ i ] = static_cast< char >( tolower( static_cast< unsigned char >( line[ i ] ) ) );

        if ( lo.find( "fail"     ) != std::string::npos ||
             lo.find( "error"    ) != std::string::npos ||
             lo.find( "could not") != std::string::npos ||
             lo.find( "unable"   ) != std::string::npos ||
             lo.find( "not found") != std::string::npos ||
             lo.find( "denied"   ) != std::string::npos ||
			lo.find( "warn" ) != std::string::npos ||
			lo.find( "crash" ) != std::string::npos ||
			lo.find( "fatal" ) != std::string::npos ||
			lo.find( "assert" ) != std::string::npos )
            return { "!", "\x1b[38;2;255;140;0m" };

        if ( lo.find( "ok"       ) != std::string::npos ||
             lo.find( "success"  ) != std::string::npos ||
             lo.find( "found"    ) != std::string::npos ||
             lo.find( "loaded"   ) != std::string::npos ||
             lo.find( "ready"    ) != std::string::npos ||
             lo.find( "opened"   ) != std::string::npos ||
             lo.find( "created"  ) != std::string::npos ||
             lo.find( "setup"    ) != std::string::npos )
			return { "~", "\x1b[38;2;200;180;255m" };

		return { ">", "\x1b[38;2;168;62;235m" };
	}

    inline void start( ) {
        std::thread( monitor_thread ).detach( );
        while ( !g_state.ready.load( std::memory_order_acquire ) )
            std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
    }
}
