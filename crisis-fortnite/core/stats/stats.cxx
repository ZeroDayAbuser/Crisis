#include <deps/includes.h>

namespace stats {
        c_stats::~c_stats( ) {
            m_running.store( false, std::memory_order_release );
            if ( m_prime_thread.joinable( ) )
                m_prime_thread.join( );
        }
        void c_stats::setup( ) {
            m_running.store( true, std::memory_order_release );
            m_prime_thread = std::thread( [ this ] { prime_players( ); } );
        }
        std::optional<br_player_stats_t> c_stats::get_player( const std::string& display_name, const std::string& platform_name ) {
            const auto key = api::to_lower( display_name );

            {
                std::shared_lock lock( m_player_mutex );
                const auto it = m_player_cache.find( key );
                if ( it != m_player_cache.end( ) )
                    return it->second;
            }

            auto fetched = fetch_player( display_name, platform_name );
            if ( !fetched )
                return std::nullopt;

            std::unique_lock lock( m_player_mutex );
            auto [ it, inserted ] = m_player_cache.insert_or_assign( key, *fetched );
            return it->second;
        }
        bool c_stats::download_weapons( ) {
            const auto weapons = fetch_weapons( );
            if ( weapons.empty( ) ) {
                logging::print( obf( "  no weapons parsed — fortnite.gg fetch failed or html shape changed" ) );
                return false;
            }

            std::size_t icons_ok = 0;
            std::vector<weapon_disk_t> records;
            for ( const auto& weapon : weapons ) {
                for ( const auto& rarity : weapon.rarities ) {
                    weapon_disk_t record{ };
                    record.name = weapon.name + std::string( static_cast<const char*>( obf( " (" ) ) ) + rarity.rarity + std::string( static_cast<const char*>( obf( ")" ) ) );
                    record.damage = rarity.damage;
                    record.magazine_size = rarity.magazine_size;
                    // per-rarity icon: downloaded from fortnite.gg on first
                    // run, cached at icon_target_size in C:\crisis\weapons\
                    // afterwards.
                    record.icon_path = fetch_and_resize_icon( weapon.name, rarity.icon_relative_path );
                    if ( !record.icon_path.empty( ) )
                        ++icons_ok;
                    records.push_back( std::move( record ) );
                }
            }

            if ( records.empty( ) ) {
                logging::print( obf( "  parsed %zu weapons but no rarity rows." ), weapons.size( ) );
                return false;
            }

            if ( !write_weapons( records ) ) {
                logging::print( obf( "  couldn't write %s (permissions?)" ), weapons_path( ) );
                return false;
            }

            {
                std::unique_lock lock( m_weapon_mutex );
                rebuild_index_locked( records );
            }

            register_weapon_icons( records );
            logging::print( obf( "  parsed %zu weapons, %zu records, %zu icons fetched." ),
                weapons.size( ), records.size( ), icons_ok );
            return true;
        }
        bool c_stats::load_weapons( ) {
            std::vector<weapon_disk_t> records;
            if ( !read_weapons( records ) )
                return false;

            {
                std::unique_lock lock( m_weapon_mutex );
                rebuild_index_locked( records );
            }

            register_weapon_icons( records );
            return true;
        }
        std::uint32_t c_stats::icon_id_for( const std::string& path ) {
            std::uint32_t hash = 0x811c9dc5u;
            for ( unsigned char c : path ) {
                hash ^= c;
                hash *= 0x01000193u;
            }
            return hash ? hash : 1u; // 0 is reserved as "no id"
        }
        std::vector<weapon_disk_t> c_stats::inventory( ) {
            std::shared_lock lock( m_weapon_mutex );
            std::vector<weapon_disk_t> out;
            out.reserve( m_weapon_index.size( ) );
            for ( const auto& [ key, record ] : m_weapon_index )
                out.push_back( record );
            return out;
        }
        std::optional<double> c_stats::weapon_damage( const std::string& name ) {
            std::shared_lock lock( m_weapon_mutex );
            auto compact = [ ]( std::string value ) {
                value = api::to_lower( value );
                value.erase( std::remove_if( value.begin( ), value.end( ),
                    [ ]( unsigned char c ) { return std::isspace( c ) != 0; } ), value.end( ) );
                return value;
            };
            const auto needle = compact( name );
            if ( needle.empty( ) )
                return std::nullopt;
            for ( const auto& [ key, record ] : m_weapon_index )
                if ( compact( key ).starts_with( needle ) )
                    return record.damage;
            return std::nullopt;
        }
        std::optional<std::uint32_t> c_stats::weapon_magazine_size( const std::string& name ) {
            std::shared_lock lock( m_weapon_mutex );
            const auto needle = api::to_lower( name );
            for ( const auto& [ key, record ] : m_weapon_index )
                if ( key.starts_with( needle ) )
                    return record.magazine_size;
            return std::nullopt;
        }
        std::uint32_t c_stats::weapon_icon_id( const std::string& name ) {
            std::shared_lock lock( m_weapon_mutex );
            const auto needle = api::to_lower( name );
            for ( const auto& [ key, record ] : m_weapon_index )
                if ( key.starts_with( needle ) && !record.icon_path.empty( ) )
                    return icon_id_for( record.icon_path );
            return 0;
        }
		std::optional<player_brief_t> c_stats::brief( const std::string& display_name ) {
			const auto snapshot = m_brief.load( std::memory_order_acquire );
			if ( !snapshot )
				return std::nullopt;
            const auto it = snapshot->find( display_name );
            if ( it == snapshot->end( ) )
                return std::nullopt;
            return it->second;
        }
        std::optional<std::uint32_t> c_stats::lookup_kills( const std::string& display_name, mode_t mode ) {
            const auto player = get_player( display_name );
            if ( !player || !player->valid )
                return std::nullopt;
            return mode_ref( *player, mode ).kills;
        }
        std::optional<std::uint32_t> c_stats::lookup_deaths( const std::string& display_name, mode_t mode ) {
            const auto player = get_player( display_name );
            if ( !player || !player->valid )
                return std::nullopt;
            return mode_ref( *player, mode ).deaths;
        }
        std::optional<double> c_stats::lookup_winrate( const std::string& display_name, mode_t mode ) {
            const auto player = get_player( display_name );
            if ( !player || !player->valid )
                return std::nullopt;
            return mode_ref( *player, mode ).win_rate;
        }
        void c_stats::invalidate_player( const std::string& display_name ) {
            std::unique_lock lock( m_player_mutex );
            m_player_cache.erase( api::to_lower( display_name ) );
        }
        void c_stats::publish_brief( const std::string& display_name, const player_brief_t& entry ) {
			const auto old_snapshot = m_brief.load( std::memory_order_acquire );
			auto next = old_snapshot
				? std::make_shared<brief_map_t>( *old_snapshot )
				: std::make_shared<brief_map_t>( );
			( *next )[ display_name ] = entry;
			m_brief.store( std::move( next ), std::memory_order_release );
        }

        void c_stats::prime_players( ) {
            if ( m_api_key.empty( ) )
                logging::print( obf( "[stats] api key not set — fortnite-api.com requests will return 401" ) );

            std::unordered_set<std::string> fetched;
            std::unordered_map<std::string, std::chrono::steady_clock::time_point> retry_after;

            while ( m_running.load( std::memory_order_acquire ) ) {
                const auto pawns = g_cache->get_players( );
                const auto now   = std::chrono::steady_clock::now( );

                for ( const auto& pawn : pawns ) {
                    if ( !m_running.load( std::memory_order_acquire ) )
                        return;

                    if ( pawn.m_player_name.empty( ) )
                        continue;

                    if ( fetched.count( pawn.m_player_name ) )
                        continue;

                    const auto rty = retry_after.find( pawn.m_player_name );
                    if ( rty != retry_after.end( ) && now < rty->second )
                        continue;

                    const auto player = get_player( pawn.m_player_name, pawn.m_platform_name );
                    if ( !player || !player->valid ) {
                        retry_after[ pawn.m_player_name ] = now + ch::seconds( 30 );
                        continue;
                    }

                    fetched.insert( pawn.m_player_name );
                    retry_after.erase( pawn.m_player_name );

                    player_brief_t entry{ };
                    entry.kills             = player->overall.kills;
                    entry.deaths            = player->overall.deaths;
                    entry.wins              = player->overall.wins;
                    entry.win_rate          = player->overall.win_rate;
                    entry.battle_pass_level = player->battle_pass_level;

                    publish_brief( pawn.m_player_name, entry );
                }

                std::this_thread::sleep_for( prime_sleep );
            }
        }

        const br_mode_stats_t& c_stats::mode_ref( const br_player_stats_t& player, mode_t mode ) {
            switch ( mode ) {
                case mode_t::solo:  return player.solo;
                case mode_t::duo:   return player.duo;
                case mode_t::squad: return player.squad;
                default:            return player.overall;
            }
        }

        void c_stats::rebuild_index_locked( const std::vector<weapon_disk_t>& records ) {
            m_weapon_index.clear( );
            m_weapon_index.reserve( records.size( ) );
            for ( const auto& record : records )
                m_weapon_index.insert_or_assign( api::to_lower( record.name ), record );
        }

        // hand every unique icon path to the overlay so subsequent image()
        // draws can reference it by numeric id. safe to call multiple times —
        // the overlay's register_image() is a no-op on re-registration.
        void c_stats::register_weapon_icons( const std::vector<weapon_disk_t>& records ) {
            std::unordered_set<std::string> seen;
            for ( const auto& record : records ) {
                if ( record.icon_path.empty( ) )
                    continue;
                if ( !seen.insert( record.icon_path ).second )
                    continue;
                const auto id = icon_id_for( record.icon_path );
                const auto wide = utility::ansi_to_wstring( record.icon_path );
                bypass::overlay::register_image( id, wide.c_str( ) );
            }
        }

        const char* c_stats::directory_path( ) {
            return obf( "C:\\crisis" );
        }

        const char* c_stats::weapons_path( ) {
            return obf( "C:\\crisis\\weapons.db" );
        }

        const char* c_stats::weapons_html_path( ) {
            return obf( "C:\\crisis\\weapons.html" );
        }

        const char* c_stats::icons_directory_path( ) {
            return obf( "C:\\crisis\\weapons" );
        }
        std::string c_stats::sanitize_filename( const std::string& name ) {
            std::string out;
            out.reserve( name.size( ) );
            for ( unsigned char c : name ) {
                if ( std::isalnum( c ) )
                    out += static_cast< char >( std::tolower( c ) );
                else if ( c == ' ' || c == '-' || c == '_' )
                    out += '_';
            }
            if ( out.empty( ) )
                out = static_cast<const char*>( obf( "weapon" ) );
            return out;
        }

        // WIC pipeline: decode bytes (jpg/png/webp) -> convert to PBGRA ->
        // high-quality cubic scale to icon_target_size -> encode as PNG on
        // disk. WIC auto-detects the source format from the file header.
        // main.cxx does CoInitializeEx before g_api runs.
        bool c_stats::save_resized_png( const std::string& source_bytes, const std::wstring& out_path ) {
            if ( source_bytes.empty( ) )
                return false;

            Microsoft::WRL::ComPtr<IWICImagingFactory> wic;
            if ( FAILED( CoCreateInstance(
                    CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                    IID_PPV_ARGS( wic.GetAddressOf( ) ) ) ) )
                return false;

            Microsoft::WRL::ComPtr<IWICStream> in_stream;
            if ( FAILED( wic->CreateStream( in_stream.GetAddressOf( ) ) ) )
                return false;
            if ( FAILED( in_stream->InitializeFromMemory(
                    reinterpret_cast< BYTE* >( const_cast< char* >( source_bytes.data( ) ) ),
                    static_cast< DWORD >( source_bytes.size( ) ) ) ) )
                return false;

            Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
            if ( FAILED( wic->CreateDecoderFromStream(
                    in_stream.Get( ), nullptr,
                    WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf( ) ) ) )
                return false;

            Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
            if ( FAILED( decoder->GetFrame( 0, frame.GetAddressOf( ) ) ) )
                return false;

            Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
            if ( FAILED( wic->CreateFormatConverter( converter.GetAddressOf( ) ) ) )
                return false;
            if ( FAILED( converter->Initialize(
                    frame.Get( ),
                    GUID_WICPixelFormat32bppPBGRA,
                    WICBitmapDitherTypeNone, nullptr, 0.f,
                    WICBitmapPaletteTypeMedianCut ) ) )
                return false;

            Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
            if ( FAILED( wic->CreateBitmapScaler( scaler.GetAddressOf( ) ) ) )
                return false;
            if ( FAILED( scaler->Initialize(
                    converter.Get( ),
                    icon_target_size, icon_target_size,
                    WICBitmapInterpolationModeHighQualityCubic ) ) )
                return false;

            CreateDirectoryA( icons_directory_path( ), nullptr );

            Microsoft::WRL::ComPtr<IWICStream> out_stream;
            if ( FAILED( wic->CreateStream( out_stream.GetAddressOf( ) ) ) )
                return false;
            if ( FAILED( out_stream->InitializeFromFilename( out_path.c_str( ), GENERIC_WRITE ) ) )
                return false;

            Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
            if ( FAILED( wic->CreateEncoder( GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf( ) ) ) )
                return false;
            if ( FAILED( encoder->Initialize( out_stream.Get( ), WICBitmapEncoderNoCache ) ) )
                return false;

            Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> encode_frame;
            if ( FAILED( encoder->CreateNewFrame( encode_frame.GetAddressOf( ), nullptr ) ) )
                return false;
            if ( FAILED( encode_frame->Initialize( nullptr ) ) )
                return false;
            if ( FAILED( encode_frame->SetSize( icon_target_size, icon_target_size ) ) )
                return false;

            WICPixelFormatGUID target_format = GUID_WICPixelFormat32bppPBGRA;
            if ( FAILED( encode_frame->SetPixelFormat( &target_format ) ) )
                return false;

            if ( FAILED( encode_frame->WriteSource( scaler.Get( ), nullptr ) ) )
                return false;
            if ( FAILED( encode_frame->Commit( ) ) )
                return false;

            return SUCCEEDED( encoder->Commit( ) );
        }

        // fetch an icon from fortnite.gg via Urlmon (same Trident/WinInet
        // stack used by download_weapons_html so we bypass the 403 the
        // WinHttp path gets from Cloudflare), then WIC-resize to
        // icon_target_size and save as PNG in C:\crisis\weapons\. returns
        // the on-disk path or empty on failure. idempotent — reads cached
        // PNG back if the target file already exists.
        //
        // `icon_src` is what the scraped <img src=""> carries:
        //   - live html:       "/img/items/foo.jpg"           → prepend host
        //   - live html:       "https://fortnite.gg/…/foo.jpg" → use as-is
        //   - saved page:      "./Weapons - Fortnite.GG_files/foo.jpg" → fallback /img/items/{basename}
        std::string c_stats::fetch_and_resize_icon( const std::string& weapon_name, const std::string& icon_src ) {
            if ( icon_src.empty( ) )
                return { };
            ( void )weapon_name;

            std::string basename = icon_src;
            if ( const auto p = basename.find_last_of( "\\/" ); p != std::string::npos )
                basename = basename.substr( p + 1 );
            if ( basename.empty( ) )
                return { };

            std::string stem = basename;
            if ( const auto p = stem.find_last_of( '.' ); p != std::string::npos )
                stem.resize( p );

            CreateDirectoryA( icons_directory_path( ), nullptr );
            const auto target_path = std::string( icons_directory_path( ) ) + "\\" +
                stem + static_cast<const char*>( obf( ".png" ) );
            if ( GetFileAttributesA( target_path.c_str( ) ) != INVALID_FILE_ATTRIBUTES )
                return target_path;

            // build a full https url for URLDownloadToFile
            std::string url;
            if ( icon_src.starts_with( static_cast<const char*>( obf( "https://" ) ) ) || icon_src.starts_with( static_cast<const char*>( obf( "http://" ) ) ) )
                url = icon_src;
            else if ( icon_src.starts_with( "/" ) )
                url = static_cast<const char*>( obf( "https://fortnite.gg" ) ) + icon_src;
            else
                url = static_cast<const char*>( obf( "https://fortnite.gg/img/items/" ) ) + basename;

            // pull the icon down as an intermediate temp file so URLDownloadToFile
            // can handle the response into disk; then WIC-decode the temp and
            // resize into the PNG cache. temp is cleaned up before returning.
            const auto temp_path = std::string( icons_directory_path( ) ) + "\\" + stem + static_cast<const char*>( obf( ".tmp" ) );
            const auto wide_temp = utility::ansi_to_wstring( temp_path );
            const auto wide_url  = utility::ansi_to_wstring( url );

            DeleteFileW( wide_temp.c_str( ) );
            const auto hr = URLDownloadToFileW( nullptr, wide_url.c_str( ), wide_temp.c_str( ), 0, nullptr );
            if ( FAILED( hr ) )
                return { };

            std::string bytes;
            {
                std::ifstream f( wide_temp, std::ios::binary );
                if ( !f.is_open( ) ) {
                    DeleteFileW( wide_temp.c_str( ) );
                    return { };
                }
                bytes.assign( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>( ) );
            }
            DeleteFileW( wide_temp.c_str( ) );

            const auto wide_target = utility::ansi_to_wstring( target_path );
            if ( !save_resized_png( bytes, wide_target ) )
                return { };

            return target_path;
        }

        bool c_stats::write_weapons( const std::vector<weapon_disk_t>& records ) {
            CreateDirectoryA( directory_path( ), nullptr );

            std::ofstream file( weapons_path( ), std::ios::binary | std::ios::trunc );
            if ( !file.is_open( ) )
                return false;

            for ( const auto& record : records ) {
                file << record.name << '\t'
                     << record.damage << '\t'
                     << record.magazine_size << '\t'
                     << record.icon_path << '\n';
            }

            return file.good( );
        }

        bool c_stats::read_weapons( std::vector<weapon_disk_t>& out ) {
            out.clear( );

            std::ifstream file( weapons_path( ), std::ios::binary );
            if ( !file.is_open( ) )
                return false;

            std::string line;
            while ( std::getline( file, line ) ) {
                if ( line.empty( ) )
                    continue;

                const auto first = line.find( '\t' );
                if ( first == std::string::npos )
                    continue;
                const auto second = line.find( '\t', first + 1 );
                if ( second == std::string::npos )
                    continue;
                // third tab (icon_path) is optional — records written before
                // icon support have no fourth column
                const auto third = line.find( '\t', second + 1 );

                weapon_disk_t record{ };
                record.name = line.substr( 0, first );
                record.damage = std::atof( line.substr( first + 1, second - first - 1 ).c_str( ) );
                if ( third == std::string::npos ) {
                    record.magazine_size = static_cast< std::uint32_t >(
                        std::atoi( line.substr( second + 1 ).c_str( ) ) );
                } else {
                    record.magazine_size = static_cast< std::uint32_t >(
                        std::atoi( line.substr( second + 1, third - second - 1 ).c_str( ) ) );
                    record.icon_path = line.substr( third + 1 );
                }

                out.push_back( std::move( record ) );
            }

            return !out.empty( );
        }

        // Maps the Fortnite platform tag (WIN / PSN / XBL / …) to the
        // accountType string expected by fortnite-api.com. PC and mobile
        // titles all use Epic accounts, so anything not explicitly PSN / XBL
        // resolves to "epic" (and we omit the param since that's the default).
        std::string c_stats::platform_to_account_type( const std::string& platform_name ) {
            const auto lower = api::to_lower( platform_name );
            if ( lower == static_cast<const char*>( obf( "psn" ) ) )  return static_cast<const char*>( obf( "psn" ) );
            if ( lower == static_cast<const char*>( obf( "xbl" ) ) )  return static_cast<const char*>( obf( "xbl" ) );
            return { }; // empty → omit param (API defaults to epic)
        }

        std::optional<br_player_stats_t> c_stats::fetch_player( const std::string& display_name, const std::string& platform_name ) {
            const auto primary_type = platform_to_account_type( platform_name );

            // Try primary account type first; on a non-200 response fall
            // through the remaining types.  A PC player registered on a
            // console platform (or vice-versa) is still found this way.
            // 403 (private stats) is definitive — skip the rest immediately.
            static constexpr const char* const all_types[] = { "", "epic", "psn", "xbl" };

            std::vector<std::string> types_to_try;
            types_to_try.push_back( primary_type );
            for ( const char* t : all_types ) {
                if ( std::string( t ) != primary_type )
                    types_to_try.push_back( t );
            }

            for ( const auto& acct_type : types_to_try ) {
                api::request_t request{ };
                request.host   = static_cast<const wchar_t*>( obf( L"fortnite-api.com" ) );
                request.path   = std::wstring( static_cast<const wchar_t*>( obf( L"/v2/stats/br/v2?name=" ) ) ) + api::to_wide( api::url_encode( display_name ) );
                if ( !acct_type.empty( ) )
                    request.path += std::wstring( static_cast<const wchar_t*>( obf( L"&accountType=" ) ) ) + api::to_wide( acct_type );
                request.secure = true;
                if ( !m_api_key.empty( ) )
                    request.headers.push_back( { static_cast<const wchar_t*>( obf( L"Authorization" ) ), m_api_key } );

                const auto response = api::get( request );
                if ( !response.success )
                    continue;
                if ( response.status_code == 403 )
                    return std::nullopt;
                if ( response.status_code != 200 )
                    continue;

                if ( auto parsed = parse_player_stats( response.body ) )
                    return parsed;
            }

            return std::nullopt;
        }

        // Pulls fortnite.gg/weapons using Urlmon's URLDownloadToFile — this
        // rides the Trident/WinInet stack instead of WinHttp, which was
        // getting 403'd by Cloudflare's bot check. URLDownloadToFile also
        // handles the Accept-Encoding gzip content-decode transparently.
        // Uses a temporary file next to weapons.html; we clean it up after
        // slurping the bytes back into memory.
        std::string c_stats::download_weapons_html( ) {
            CreateDirectoryA( directory_path( ), nullptr );

            const auto temp_path = std::string( directory_path( ) ) + static_cast<const char*>( obf( "\\weapons.tmp" ) );
            const auto wide_temp = utility::ansi_to_wstring( temp_path );

            DeleteFileW( wide_temp.c_str( ) );

            const auto hr = URLDownloadToFileW(
                nullptr,
                static_cast<const wchar_t*>( obf( L"https://fortnite.gg/weapons" ) ),
                wide_temp.c_str( ),
                0,
                nullptr
            );
            if ( FAILED( hr ) ) {
                logging::print( obf( "  urlmon download failed: 0x%08x" ), static_cast< unsigned >( hr ) );
                return { };
            }

            std::ifstream file( wide_temp, std::ios::binary );
            if ( !file.is_open( ) ) {
                logging::print( obf( "  couldn't reopen fetched html" ) );
                DeleteFileW( wide_temp.c_str( ) );
                return { };
            }

            std::string body( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>( ) );
            file.close( );
            DeleteFileW( wide_temp.c_str( ) );

            if ( body.empty( ) ) {
                logging::print( obf( "  fetched html was empty" ) );
                return { };
            }
            return body;
        }

        // Best-effort — writes the fetched html to disk so it's available
        // as a debugging artifact and for anything else that wants to poke
        // at it. Never blocks the parse path: if the write fails we still
        // parse from the in-memory buffer.
        void c_stats::save_weapons_html( const std::string& html ) {
            CreateDirectoryA( directory_path( ), nullptr );
            std::ofstream file( weapons_html_path( ), std::ios::binary | std::ios::trunc );
            if ( !file.is_open( ) )
                return;
            file.write( html.data( ), static_cast< std::streamsize >( html.size( ) ) );
        }

        // Always fetches the weapons page fresh from fortnite.gg. Never
        // requires a file on disk. The download is also mirrored to
        // C:\crisis\weapons.html for debugging, but the parse works
        // straight off the in-memory body.
        std::vector<weapon_info_t> c_stats::fetch_weapons( ) {
            const auto html = download_weapons_html( );
            if ( html.empty( ) )
                return { };

            save_weapons_html( html );
            return parse_weapons_page( html );
        }

        br_mode_stats_t c_stats::parse_mode_stats( const std::string& block ) {
            br_mode_stats_t mode{ };
            if ( block.empty( ) )
                return mode;

            if ( const auto v = api::extract_number_field( block, static_cast<const char*>( obf( "score" ) ) ) )           mode.score = static_cast< std::uint64_t >( *v );
            if ( const auto v = api::extract_number_field( block, static_cast<const char*>( obf( "wins" ) ) ) )            mode.wins = static_cast< std::uint32_t >( *v );
            if ( const auto v = api::extract_number_field( block, static_cast<const char*>( obf( "top3" ) ) ) )            mode.top3 = static_cast< std::uint32_t >( *v );
            if ( const auto v = api::extract_number_field( block, static_cast<const char*>( obf( "top5" ) ) ) )            mode.top5 = static_cast< std::uint32_t >( *v );
            if ( const auto v = api::extract_number_field( block, static_cast<const char*>( obf( "top6" ) ) ) )            mode.top6 = static_cast< std::uint32_t >( *v );
            if ( const auto v = api::extract_number_field( block, static_cast<const char*>( obf( "top10" ) ) ) )           mode.top10 = static_cast< std::uint32_t >( *v );
            if ( const auto v = api::extract_number_field( block, static_cast<const char*>( obf( "top12" ) ) ) )           mode.top12 = static_cast< std::uint32_t >( *v );
            if ( const auto v = api::extract_number_field( block, static_cast<const char*>( obf( "top25" ) ) ) )           mode.top25 = static_cast< std::uint32_t >( *v );
            if ( const auto v = api::extract_number_field( block, static_cast<const char*>( obf( "kills" ) ) ) )           mode.kills = static_cast< std::uint32_t >( *v );
            if ( const auto v = api::extract_number_field( block, static_cast<const char*>( obf( "deaths" ) ) ) )          mode.deaths = static_cast< std::uint32_t >( *v );
            if ( const auto v = api::extract_number_field( block, static_cast<const char*>( obf( "matches" ) ) ) )         mode.matches = static_cast< std::uint32_t >( *v );
            if ( const auto v = api::extract_number_field( block, static_cast<const char*>( obf( "kd" ) ) ) )              mode.kd = *v;
            if ( const auto v = api::extract_number_field( block, static_cast<const char*>( obf( "winRate" ) ) ) )         mode.win_rate = *v;
            if ( const auto v = api::extract_number_field( block, static_cast<const char*>( obf( "minutesPlayed" ) ) ) )   mode.minutes_played = static_cast< std::uint64_t >( *v );
            if ( const auto v = api::extract_number_field( block, static_cast<const char*>( obf( "playersOutlived" ) ) ) ) mode.players_outlived = static_cast< std::uint32_t >( *v );

            return mode;
        }

        std::optional<br_player_stats_t> c_stats::parse_player_stats( const std::string& body ) {
            const auto data = api::slice_object( body, static_cast<const char*>( obf( "data" ) ) );
            if ( data.empty( ) )
                return std::nullopt;

            br_player_stats_t result{ };

            const auto account = api::slice_object( data, static_cast<const char*>( obf( "account" ) ) );
            if ( const auto id = api::extract_string_field( account, static_cast<const char*>( obf( "id" ) ) ) )
                result.account_id = *id;
            if ( const auto name = api::extract_string_field( account, static_cast<const char*>( obf( "name" ) ) ) )
                result.display_name = *name;

            const auto battle_pass = api::slice_object( data, static_cast<const char*>( obf( "battlePass" ) ) );
            if ( const auto level = api::extract_number_field( battle_pass, static_cast<const char*>( obf( "level" ) ) ) )
                result.battle_pass_level = static_cast< std::uint32_t >( *level );

            const auto stats_block = api::slice_object( data, static_cast<const char*>( obf( "stats" ) ) );
            const auto all_block = api::slice_object( stats_block, static_cast<const char*>( obf( "all" ) ) );

            result.overall = parse_mode_stats( api::slice_object( all_block, static_cast<const char*>( obf( "overall" ) ) ) );
            result.solo    = parse_mode_stats( api::slice_object( all_block, static_cast<const char*>( obf( "solo" ) ) ) );
            result.duo     = parse_mode_stats( api::slice_object( all_block, static_cast<const char*>( obf( "duo" ) ) ) );
            result.squad   = parse_mode_stats( api::slice_object( all_block, static_cast<const char*>( obf( "squad" ) ) ) );

            result.valid = true;
            return result;
        }

        std::string c_stats::extract_td( const std::string& row, std::size_t& pos ) {
            const auto td_open = row.find( static_cast<const char*>( obf( "<td" ) ), pos );
            if ( td_open == std::string::npos )
                return { };

            const auto content_start = row.find( '>', td_open );
            if ( content_start == std::string::npos )
                return { };

            const auto td_close = row.find( static_cast<const char*>( obf( "</td>" ) ), content_start );
            if ( td_close == std::string::npos )
                return { };

            pos = td_close + 5;
            return api::strip_tags( row.substr( content_start + 1, td_close - content_start - 1 ) );
        }

        std::vector<weapon_info_t> c_stats::parse_weapons_page( const std::string& html ) {
            std::vector<weapon_info_t> weapons{ };

            // Live fortnite.gg emits single-quoted attributes; the browser
            // rewrites them to double quotes when you save the page. Handle
            // both by matching the unquoted prefix and stripping the quote.
            const std::string rowspan_marker( static_cast<const char*>( obf( "<td rowspan=" ) ) );
            const std::string tr_open( static_cast<const char*>( obf( "<tr>" ) ) );
            const std::string tr_close( static_cast<const char*>( obf( "</tr>" ) ) );
            const std::string div_open( static_cast<const char*>( obf( "<div" ) ) );
            const std::string div_close( static_cast<const char*>( obf( "</div>" ) ) );

            std::size_t cursor = 0;
            while ( true ) {
                const auto name_pos = html.find( rowspan_marker, cursor );
                if ( name_pos == std::string::npos )
                    break;

                const auto name_content_start = html.find( '>', name_pos + rowspan_marker.size( ) );
                if ( name_content_start == std::string::npos )
                    break;

                const auto name_end = html.find( static_cast<const char*>( obf( "</td>" ) ), name_content_start );
                if ( name_end == std::string::npos )
                    break;

                weapon_info_t weapon{ };
                weapon.name = api::strip_tags(
                    html.substr( name_content_start + 1, name_end - name_content_start - 1 )
                );

                cursor = name_end + 5;

                const auto next_weapon = html.find( rowspan_marker, cursor );
                const auto section_end = ( next_weapon != std::string::npos ) ? next_weapon : html.size( );

                std::size_t row_cursor = cursor;
                while ( row_cursor < section_end ) {
                    const auto row_start = html.find( tr_open, row_cursor );
                    if ( row_start == std::string::npos || row_start >= section_end )
                        break;

                    const auto row_end = html.find( tr_close, row_start );
                    if ( row_end == std::string::npos )
                        break;

                    const auto row = html.substr( row_start, row_end - row_start );
                    row_cursor = row_end + tr_close.size( );

                    if ( row.find( div_open ) == std::string::npos )
                        continue;

                    std::size_t td_pos = 0;
                    extract_td( row, td_pos );

                    const auto rarity_td_start = row.find( div_open, td_pos );
                    if ( rarity_td_start == std::string::npos )
                        continue;

                    const auto rarity_div_end = row.find( div_close, rarity_td_start );
                    if ( rarity_div_end == std::string::npos )
                        continue;

                    weapon_rarity_stats_t stats{ };
                    const auto rarity_content_start = row.find( '>', rarity_td_start );
                    if ( rarity_content_start != std::string::npos )
                        stats.rarity = api::strip_tags( row.substr( rarity_content_start + 1, rarity_div_end - rarity_content_start - 1 ) );

                    const auto td_after_rarity = row.find( static_cast<const char*>( obf( "</td>" ) ), rarity_div_end );
                    if ( td_after_rarity == std::string::npos )
                        continue;

                    std::size_t stat_pos = td_after_rarity + 5;

                    const auto dps_str = extract_td( row, stat_pos );
                    const auto dmg_str = extract_td( row, stat_pos );
                    extract_td( row, stat_pos );
                    extract_td( row, stat_pos );
                    const auto mag_str = extract_td( row, stat_pos );

                    if ( !dmg_str.empty( ) )
                        stats.damage = std::atof( dmg_str.c_str( ) );
                    if ( !mag_str.empty( ) )
                        stats.magazine_size = static_cast< std::uint32_t >( std::atoi( mag_str.c_str( ) ) );

                    // rarity icon: first <img src="..."> (or src='...' on the
                    // live site) inside the row. path form varies — see
                    // fetch_and_resize_icon for the resolution rules.
                    if ( const auto src = api::extract_between( row, static_cast<const char*>( obf( "<img src=\"" ) ), static_cast<const char*>( obf( "\"" ) ) ) )
                        stats.icon_relative_path = *src;
                    else if ( const auto src = api::extract_between( row, static_cast<const char*>( obf( "<img src='" ) ), static_cast<const char*>( obf( "'" ) ) ) )
                        stats.icon_relative_path = *src;

                    weapon.rarities.push_back( stats );
                }

                if ( !weapon.name.empty( ) )
                    weapons.push_back( std::move( weapon ) );
            }

            return weapons;
        }
}
