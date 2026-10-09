#pragma once

namespace utility {
inline auto print( const char* fmt, ... ) -> void {
	char msg[ 512 ] {};
	va_list args;
	va_start( args, fmt );
	vsnprintf( msg, sizeof( msg ), fmt, args );
	va_end( args );

	SYSTEMTIME t {};
	GetLocalTime( &t );

	char out[ 600 ] {};
	snprintf( out, sizeof( out ),
		"[%02d:%02d:%02d] [crisis-memory] %s\n",
		t.wHour, t.wMinute, t.wSecond, msg );

	OutputDebugStringA( out );
}
}