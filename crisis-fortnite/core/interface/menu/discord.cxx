#include <deps/includes.h>
namespace discord {

	inline constexpr std::uint32_t op_handshake = 0;
	inline constexpr std::uint32_t op_frame = 1;
	inline constexpr std::uint32_t op_close = 2;
	inline constexpr std::uint32_t op_ping = 3;
	inline constexpr std::uint32_t op_pong = 4;
	inline constexpr std::uint32_t max_frame_size = 64 * 1024;

	inline bool read_exact( HANDLE pipe, void* destination, std::size_t size ) {
		auto* output = static_cast< std::uint8_t* >( destination );
		while ( size ) {
			DWORD read = 0;
			if ( !ReadFile( pipe, output, static_cast< DWORD >( size ), &read, nullptr ) || !read )
				return false;
			output += read;
			size -= read;
		}
		return true;
	}

	inline bool write_exact( HANDLE pipe, const void* source, std::size_t size ) {
		const auto* input = static_cast< const std::uint8_t* >( source );
		while ( size ) {
			DWORD written = 0;
			if ( !WriteFile( pipe, input, static_cast< DWORD >( size ), &written, nullptr ) || !written )
				return false;
			input += written;
			size -= written;
		}
		return true;
	}

	inline bool write_frame( HANDLE pipe, std::uint32_t opcode, std::string_view payload ) {
		const std::uint32_t header[ 2 ] = { opcode, static_cast< std::uint32_t >( payload.size( ) ) };
		return write_exact( pipe, header, sizeof( header ) )
			&& write_exact( pipe, payload.data( ), payload.size( ) );
	}

	inline bool read_frame( HANDLE pipe, std::uint32_t& opcode, std::string& payload ) {
		std::uint32_t header[ 2 ] {};
		if ( !read_exact( pipe, header, sizeof( header ) ) || header[ 1 ] > max_frame_size )
			return false;

		opcode = header[ 0 ];
		payload.resize( header[ 1 ] );
		return !header[ 1 ] || read_exact( pipe, payload.data( ), payload.size( ) );
	}

	inline std::optional<std::string> json_string( std::string_view json, std::string_view key ) {
		const std::string needle = '"' + std::string( key ) + '"';
		const auto key_pos = json.find( needle );
		if ( key_pos == std::string_view::npos )
			return std::nullopt;

		std::size_t pos = json.find( ':', key_pos + needle.size( ) );
		if ( pos == std::string_view::npos )
			return std::nullopt;
		while ( ++pos < json.size( ) && std::isspace( static_cast< unsigned char >( json[ pos ] ) ) ) { }
		if ( pos == json.size( ) || json[ pos ] != '"' )
			return std::nullopt;

		std::string value;
		for ( ++pos; pos < json.size( ); ++pos ) {
			const char ch = json[ pos ];
			if ( ch == '"' )
				return value;
			if ( ch != '\\' || ++pos == json.size( ) )
				value.push_back( ch );
			else {
				switch ( json[ pos ] ) {
				case '"': value.push_back( '"' ); break;
				case '\\': value.push_back( '\\' ); break;
				case '/':  value.push_back( '/' ); break;
				case 'b':  value.push_back( '\b' ); break;
				case 'f':  value.push_back( '\f' ); break;
				case 'n':  value.push_back( '\n' ); break;
				case 'r':  value.push_back( '\r' ); break;
				case 't':  value.push_back( '\t' ); break;
				default:   return std::nullopt;
				}
			}
		}
		return std::nullopt;
	}

	inline std::wstring utf8_to_wide( std::string_view value ) {
		if ( value.empty( ) )
			return {};
		const int size = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, value.data( ),
			static_cast< int >( value.size( ) ), nullptr, 0 );
		if ( !size )
			return {};
		std::wstring output( size, L'\0' );
		return MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, value.data( ),
			static_cast< int >( value.size( ) ), output.data( ), size ) ? output : std::wstring {};
	}

	std::optional<profile_t> current_profile( ) {
		HANDLE pipe = INVALID_HANDLE_VALUE;
		for ( std::uint32_t index = 0; index != 10; ++index ) {
			const std::wstring name = L"\\\\.\\pipe\\discord-ipc-" + std::to_wstring( index );
			pipe = CreateFileW( name.c_str( ), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
			if ( pipe != INVALID_HANDLE_VALUE )
				break;
		}
		if ( pipe == INVALID_HANDLE_VALUE )
			return std::nullopt;

		const std::string handshake = "{\"v\":1,\"client_id\":\"1466562560278007869\"}";
		if ( !write_frame( pipe, op_handshake, handshake ) ) {
			CloseHandle( pipe );
			return std::nullopt;
		}

		std::optional<profile_t> profile;
		for ( std::uint32_t frame = 0; frame != 8; ++frame ) {
			std::uint32_t opcode = 0;
			std::string payload;
			if ( !read_frame( pipe, opcode, payload ) )
				break;
			if ( opcode == op_ping ) {
				if ( !write_frame( pipe, op_pong, payload ) )
					break;
				continue;
			}
			if ( opcode == op_close )
				break;
			const auto event = opcode == op_frame ? json_string( payload, "evt" ) : std::nullopt;
			if ( !event || *event != "READY" )
				continue;

			const auto id = json_string( payload, "id" );
			const auto username = json_string( payload, "username" );
			if ( !id || !username )
				break;

			std::uint64_t numeric_id = 0;
			const auto [end, error] = std::from_chars( id->data( ), id->data( ) + id->size( ), numeric_id );
			if ( error != std::errc {} || end != id->data( ) + id->size( ) )
				break;

			const auto avatar = json_string( payload, "avatar" );
			const auto global_name = json_string( payload, "global_name" );
			const std::string avatar_path = avatar && !avatar->empty( )
				? "https://cdn.discordapp.com/avatars/" + *id + "/" + *avatar
				+ ( avatar->rfind( "a_", 0 ) == 0 ? ".gif" : ".png" )
				: "https://cdn.discordapp.com/embed/avatars/" + std::to_string( ( numeric_id >> 22 ) % 6 ) + ".png";
			const std::string& display_name = global_name && !global_name->empty( ) ? *global_name : *username;
			profile = profile_t { utf8_to_wide( *id ), utf8_to_wide( *username ),
				utf8_to_wide( display_name ), utf8_to_wide( avatar_path ) };
			break;
		}

		CloseHandle( pipe );
		return profile;
	}
}
