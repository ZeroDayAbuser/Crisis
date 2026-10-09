#pragma once

#include "../dependencies/crisis-casino/client.hxx"
#include <mutex>

namespace crisis::profile {
	struct leaderboard_entry_t {
		std::uint32_t placement = 0;
		std::wstring display_name;
		std::uint64_t lifetime_xp = 0;
		std::uint32_t level = 1;
		bool current_user = false;
	};

	struct earning_t {
		std::wstring event;
		std::wstring context;
		std::uint32_t amount = 0;
	};

	struct snapshot_t {
		bool valid = false;
		std::wstring display_name = L"User";
		std::wstring username = L"user";
		std::wstring discord_id;
		std::wstring role = L"Member";
		std::int64_t expires_unix = 0;
		std::uint64_t xp_balance = 0;
		std::uint64_t lifetime_xp = 0;
		std::uint64_t crisis_coins = 0;
		std::uint32_t level = 1;
		std::uint64_t level_start_xp = 0;
		std::uint64_t next_level_xp = 1000;
		float level_progress = 0.0f;
		std::uint32_t games_played = 0;
		std::uint32_t wins = 0;
		std::uint32_t kills = 0;
		std::uint32_t deaths = 0;
		std::uint32_t ranked_games = 0;
		std::uint32_t non_ranked_games = 0;
		std::uint32_t tournament_games = 0;
		std::uint32_t ranked_kills = 0;
		std::uint32_t non_ranked_kills = 0;
		std::uint32_t tournament_kills = 0;
		std::uint32_t accounts_used = 1;
		float average_game_minutes = 0.0f;
		float winrate = 0.0f;
		float kd = 0.0f;
		std::uint32_t placement = 0;
		std::uint32_t total_players = 0;
		std::uint32_t elimination_xp = 50;
		std::uint32_t victory_xp = 150;
		std::vector< leaderboard_entry_t > leaderboard;
		std::vector< earning_t > recent_earnings;
		std::vector< float > lifetime_progress;
	};

	namespace detail {
		struct internet_t {
			HINTERNET value = nullptr;
			internet_t( ) = default;
			explicit internet_t( HINTERNET h ) noexcept : value( h ) {}
			internet_t( const internet_t& ) = delete;
			internet_t& operator=( const internet_t& ) = delete;
			internet_t( internet_t&& other ) noexcept : value( other.value ) { other.value = nullptr; }
			internet_t& operator=( internet_t&& other ) noexcept {
				if ( this != &other ) { if ( value ) WinHttpCloseHandle( value ); value = other.value; other.value = nullptr; }
				return *this;
			}
			~internet_t( ) { if ( value ) WinHttpCloseHandle( value ); }
			explicit operator bool( ) const noexcept { return value != nullptr; }
		};

		inline std::string object_value( const std::string& json, std::string_view key ) {
			const auto at = casino::detail::value_at( json, key );
			if ( !at || json[ *at ] != '{' ) return {};
			std::size_t depth = 0; bool quoted = false; bool escaped = false;
			for ( std::size_t i = *at; i < json.size( ); ++i ) {
				const char ch = json[ i ];
				if ( quoted ) { if ( escaped ) escaped = false; else if ( ch == '\\' ) escaped = true; else if ( ch == '"' ) quoted = false; continue; }
				if ( ch == '"' ) { quoted = true; continue; }
				if ( ch == '{' ) ++depth;
				else if ( ch == '}' && --depth == 0 ) return json.substr( *at, i - *at + 1 );
			}
			return {};
		}

		inline std::vector< std::string > object_array( const std::string& json, std::string_view key ) {
			std::vector< std::string > output;
			const auto at = casino::detail::value_at( json, key );
			if ( !at || json[ *at ] != '[' ) return output;
			std::size_t pos = *at + 1;
			while ( pos < json.size( ) && json[ pos ] != ']' ) {
				while ( pos < json.size( ) && json[ pos ] != '{' && json[ pos ] != ']' ) ++pos;
				if ( pos >= json.size( ) || json[ pos ] == ']' ) break;
				const std::string tail = json.substr( pos );
				const std::string object = object_value( "{\"item\":" + tail, "item" );
				if ( object.empty( ) ) break;
				output.push_back( object ); pos += object.size( );
			}
			return output;
		}
	}

	class client_t {
	public:
		bool configure_from_environment( ) {
			base_url_ = casino::detail::env( "CRISIS_AUTH_URL" );
			player_token_ = casino::detail::env( "CRISIS_PLAYER_TOKEN" );
			if ( base_url_.empty( ) ) base_url_ = "https://wauth.aaddpp.lol";
			configured_ = casino::detail::valid_access_token( player_token_ );
			return configured_;
		}

		[[nodiscard]] bool configured( ) const { return configured_; }
		[[nodiscard]] snapshot_t snapshot( ) const { std::scoped_lock lock( mutex_ ); return snapshot_; }
		[[nodiscard]] std::string last_error( ) const { std::scoped_lock lock( mutex_ ); return last_error_; }

		bool refresh( ) { return request( L"GET", "/v1/player", {} ); }
		bool redeem_time( std::uint32_t hours ) {
			return request( L"POST", "/v1/player/subscription/redeem-xp", "{\"hours\":" + std::to_string( hours ) + "}" );
		}

		bool start_match( std::string_view mode = "non_ranked" ) {
			const std::string id = casino::detail::guid( );
			if ( id.empty( ) ) return false;
			const std::string body = "{\"match_id\":\"" + id + "\",\"request_id\":\"" + casino::detail::guid( )
				+ "\",\"mode\":\"" + casino::detail::json_escape( mode ) + "\"}";
			if ( !request( L"POST", "/v1/player/gameplay/match/start", body ) ) return false;
			std::scoped_lock lock( mutex_ ); match_id_ = id; return true;
		}

		bool elimination( ) {
			std::string match; { std::scoped_lock lock( mutex_ ); match = match_id_; }
			if ( match.empty( ) ) return false;
			return request( L"POST", "/v1/player/gameplay/elimination",
				"{\"match_id\":\"" + match + "\",\"event_id\":\"" + casino::detail::guid( ) + "\"}" );
		}

		bool end_match( bool won ) {
			std::string match; { std::scoped_lock lock( mutex_ ); match = match_id_; }
			if ( match.empty( ) ) return false;
			const bool ok = request( L"POST", "/v1/player/gameplay/match/end",
				"{\"match_id\":\"" + match + "\",\"event_id\":\"" + casino::detail::guid( )
				+ "\",\"won\":" + ( won ? "true" : "false" ) + "}" );
			if ( ok ) { std::scoped_lock lock( mutex_ ); match_id_.clear( ); }
			return ok;
		}

	private:
		bool request( const wchar_t* method, const std::string& path, const std::string& body ) {
			if ( !configured_ ) return fail( "Profile authentication is not configured" );
			const std::wstring url = casino::detail::utf8_to_wide( base_url_ );
			std::vector< wchar_t > mutable_url( url.begin( ), url.end( ) ); mutable_url.push_back( L'\0' );
			URL_COMPONENTS parts {}; parts.dwStructSize = sizeof( parts ); parts.dwHostNameLength = DWORD( -1 ); parts.dwUrlPathLength = DWORD( -1 );
			if ( !WinHttpCrackUrl( mutable_url.data( ), 0, 0, &parts ) || !parts.lpszHostName ) return fail( "Invalid profile server URL" );
			const bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
			std::wstring host( parts.lpszHostName, parts.dwHostNameLength ), normalized = host;
			std::transform( normalized.begin( ), normalized.end( ), normalized.begin( ), ::towlower );
			const bool loopback = normalized == L"localhost" || normalized == L"127.0.0.1" || normalized == L"::1" || normalized == L"[::1]";
			if ( !secure && ( parts.nScheme != INTERNET_SCHEME_HTTP || !loopback ) ) return fail( "Profile requests require HTTPS" );
			std::wstring base_path = parts.dwUrlPathLength ? std::wstring( parts.lpszUrlPath, parts.dwUrlPathLength ) : L"";
			if ( !base_path.empty( ) && base_path.back( ) == L'/' ) base_path.pop_back( );
			const std::wstring request_path = base_path + casino::detail::utf8_to_wide( path );
			detail::internet_t session( WinHttpOpen( L"crisis-profile/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
				WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0 ) );
			if ( !session ) return fail( "Unable to open profile HTTPS session" );
			WinHttpSetTimeouts( session.value, 3000, 3000, 5000, 7000 );
			detail::internet_t connection( WinHttpConnect( session.value, host.c_str( ), parts.nPort, 0 ) );
			detail::internet_t handle( connection ? WinHttpOpenRequest( connection.value, method, request_path.c_str( ), nullptr,
				WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0 ) : nullptr );
			if ( !handle ) return fail( "Unable to connect to profile server" );
			const std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer "
				+ casino::detail::utf8_to_wide( player_token_ ) + L"\r\n";
			if ( !WinHttpSendRequest( handle.value, headers.c_str( ), static_cast< DWORD >( -1L ),
				body.empty( ) ? WINHTTP_NO_REQUEST_DATA : const_cast< char* >( body.data( ) ), static_cast< DWORD >( body.size( ) ),
				static_cast< DWORD >( body.size( ) ), 0 ) || !WinHttpReceiveResponse( handle.value, nullptr ) ) return fail( "Profile server request failed" );
			DWORD status = 0, status_size = sizeof( status );
			WinHttpQueryHeaders( handle.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &status_size, nullptr );
			std::string json;
			for ( ;; ) {
				DWORD available = 0; if ( !WinHttpQueryDataAvailable( handle.value, &available ) || !available ) break;
				const auto before = json.size( ); json.resize( before + available ); DWORD read = 0;
				if ( !WinHttpReadData( handle.value, json.data( ) + before, available, &read ) ) break; json.resize( before + read );
			}
			if ( status < 200 || status >= 300 || !casino::detail::bool_value( json, "success" ) ) {
				const auto error = casino::detail::string_value( json, "error" ); return fail( error.empty( ) ? "Profile server rejected the request" : error );
			}
			snapshot_t next = parse( json ); next.valid = true;
			{ std::scoped_lock lock( mutex_ ); snapshot_ = std::move( next ); last_error_.clear( ); }
			return true;
		}

		static snapshot_t parse( const std::string& json ) {
			snapshot_t out;
			const auto n = [ & ]( std::string_view key, double fallback = 0 ) { return casino::detail::number_value( json, key, fallback ); };
			out.display_name = casino::detail::utf8_to_wide( casino::detail::string_value( json, "display_name" ) );
			out.username = casino::detail::utf8_to_wide( casino::detail::string_value( json, "username" ) );
			out.discord_id = casino::detail::utf8_to_wide( casino::detail::string_value( json, "discord_id" ) );
			out.role = casino::detail::utf8_to_wide( casino::detail::string_value( json, "role" ) );
			out.expires_unix = static_cast< std::int64_t >( n( "expires_unix" ) );
			out.xp_balance = static_cast< std::uint64_t >( n( "xp_balance" ) ); out.lifetime_xp = static_cast< std::uint64_t >( n( "lifetime_xp" ) );
			out.crisis_coins = static_cast< std::uint64_t >( n( "crisis_coins" ) );
			const std::string level = detail::object_value( json, "level" );
			out.level = static_cast< std::uint32_t >( casino::detail::number_value( level, "level", 1 ) );
			out.level_start_xp = static_cast< std::uint64_t >( casino::detail::number_value( level, "level_start_xp" ) );
			out.next_level_xp = static_cast< std::uint64_t >( casino::detail::number_value( level, "next_level_xp", 1000 ) );
			out.level_progress = static_cast< float >( casino::detail::number_value( level, "progress" ) );
			out.games_played = static_cast< std::uint32_t >( n( "games_played" ) ); out.wins = static_cast< std::uint32_t >( n( "wins" ) );
			out.kills = static_cast< std::uint32_t >( n( "kills" ) ); out.deaths = static_cast< std::uint32_t >( n( "deaths" ) );
			out.ranked_games = static_cast< std::uint32_t >( n( "ranked_games" ) ); out.non_ranked_games = static_cast< std::uint32_t >( n( "non_ranked_games" ) );
			out.tournament_games = static_cast< std::uint32_t >( n( "tournament_games" ) ); out.ranked_kills = static_cast< std::uint32_t >( n( "ranked_kills" ) );
			out.non_ranked_kills = static_cast< std::uint32_t >( n( "non_ranked_kills" ) ); out.tournament_kills = static_cast< std::uint32_t >( n( "tournament_kills" ) );
			out.accounts_used = static_cast< std::uint32_t >( n( "accounts_used", 1 ) ); out.average_game_minutes = static_cast< float >( n( "average_game_minutes" ) );
			out.winrate = static_cast< float >( n( "winrate" ) ); out.kd = static_cast< float >( n( "kd" ) );
			out.placement = static_cast< std::uint32_t >( n( "placement" ) ); out.total_players = static_cast< std::uint32_t >( n( "total_players" ) );
			const std::string rates = detail::object_value( json, "xp_rates" );
			out.elimination_xp = static_cast< std::uint32_t >( casino::detail::number_value( rates, "elimination", 50 ) );
			out.victory_xp = static_cast< std::uint32_t >( casino::detail::number_value( rates, "victory", 150 ) );
			for ( const auto& item : detail::object_array( json, "entries" ) ) out.leaderboard.push_back( {
				static_cast< std::uint32_t >( casino::detail::number_value( item, "placement" ) ),
				casino::detail::utf8_to_wide( casino::detail::string_value( item, "display_name" ) ),
				static_cast< std::uint64_t >( casino::detail::number_value( item, "lifetime_xp" ) ),
				static_cast< std::uint32_t >( casino::detail::number_value( item, "level", 1 ) ),
				casino::detail::bool_value( item, "current_user" ) } );
			for ( const auto& item : detail::object_array( json, "recent_xp_earnings" ) ) out.recent_earnings.push_back( {
				casino::detail::utf8_to_wide( casino::detail::string_value( item, "event" ) ),
				casino::detail::utf8_to_wide( casino::detail::string_value( item, "context" ) ),
				static_cast< std::uint32_t >( casino::detail::number_value( item, "amount" ) ) } );
			for ( const auto& item : detail::object_array( json, "lifetime_xp_progress" ) )
				out.lifetime_progress.push_back( static_cast< float >( casino::detail::number_value( item, "lifetime_xp" ) ) );
			if ( out.lifetime_progress.empty( ) ) out.lifetime_progress.push_back( static_cast< float >( out.lifetime_xp ) );
			return out;
		}

		bool fail( std::string message ) { std::scoped_lock lock( mutex_ ); last_error_ = std::move( message ); return false; }
		bool configured_ = false;
		std::string base_url_, player_token_;
		mutable std::mutex mutex_;
		snapshot_t snapshot_;
		std::string last_error_, match_id_;
	};
}
