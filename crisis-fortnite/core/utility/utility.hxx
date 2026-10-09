#pragma once

namespace logging {
	template<typename... Args>
	static void print( const char* fmt, Args... args ) {
		char msg[ 512 ];
		sprintf_s( msg, fmt, args... );
		SYSTEMTIME t { };
		GetLocalTime( &t );
		char out[ 600 ];
		sprintf_s( out, obf( "[%02d:%02d:%02d] [crisis-external] %s\n" ), t.wHour, t.wMinute, t.wSecond, msg );
		OutputDebugStringA( out );
	}
}

namespace utility {
	__forceinline uintptr_t to_addr( const void* pointer ) {
		return reinterpret_cast< uintptr_t >( pointer );
	}

	__forceinline void* to_ptr( uintptr_t address ) {
		return reinterpret_cast< void* >( address );
	}

	__forceinline bool is_valid_pointer( uintptr_t address ) {
		return ( address >= 0x0000000000010000 && address < 0x00007FFFFFFEFFFF );
	}

	__forceinline bool is_valid_pointer( const void* pointer ) {
		return is_valid_pointer( to_addr( pointer ) );
	}

	__forceinline bool is_on_screen( double x, double y ) {
		return x >= 0.0 && x < static_cast< double >( gui::screen_width )
			&& y >= 0.0 && y < static_cast< double >( gui::screen_height );
	}

	__forceinline bool is_on_screen( const bypass::overlay::point_t& point ) {
		return is_on_screen( static_cast< double >( point.x ), static_cast< double >( point.y ) );
	}

	__forceinline double distance_2d( double x1, double y1, double x2, double y2 ) {
		const double dx = x2 - x1;
		const double dy = y2 - y1;
		return std::sqrt( dx * dx + dy * dy );
	}

	__forceinline double distance_to_center( double x, double y ) {
		return distance_2d( x, y,
			static_cast< double >( gui::screen_width_center ),
			static_cast< double >( gui::screen_height_center ) );
	}

	__forceinline bool in_fov_circle( double x, double y, double radius ) {
		return distance_to_center( x, y ) <= radius;
	}

	__forceinline bool in_fov_rect( double x, double y, double half_width, double half_height ) {
		const double cx = static_cast< double >( gui::screen_width_center );
		const double cy = static_cast< double >( gui::screen_height_center );
		return std::fabs( x - cx ) <= half_width && std::fabs( y - cy ) <= half_height;
	}

	static inline std::wstring ansi_to_wstring( const std::string& input, DWORD locale = 0 ) {
		if ( input.empty( ) ) return std::wstring( );

		int size_needed = MultiByteToWideChar( locale, 0, input.c_str( ), ( int ) input.length( ), nullptr, 0 );
		std::wstring result( size_needed, 0 );
		MultiByteToWideChar( locale, 0, input.c_str( ), ( int ) input.length( ), &result[ 0 ], size_needed );
		return result;
	}

	static inline std::string wstring_to_ansi( const std::wstring& input, DWORD locale = 0 ) {
		if ( input.empty( ) ) return std::string( );

		int size_needed = WideCharToMultiByte( locale, 0, input.c_str( ), ( int ) input.length( ), nullptr, 0, nullptr, nullptr );
		std::string result( size_needed, 0 );
		WideCharToMultiByte( locale, 0, input.c_str( ), ( int ) input.length( ), &result[ 0 ], size_needed, nullptr, nullptr );
		return result;
	}
}