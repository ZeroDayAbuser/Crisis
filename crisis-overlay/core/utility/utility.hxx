#pragma once

struct ACCENT_POLICY {
    DWORD accent_state;
    DWORD accent_flags;
    DWORD gradient_color;
    DWORD animation_id;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    DWORD attrib;
    PVOID pv_data;
    SIZE_T cb_data;
};

namespace utility {
    inline auto print( const char* fmt, ... ) -> void {
        char buffer[ 512 ]{};

        va_list args;
        va_start( args, fmt );
        vsnprintf( buffer, sizeof( buffer ), fmt, args );
        va_end( args );

        SYSTEMTIME t {};
        GetLocalTime( &t );

        char out[ 600 ]{};
        snprintf( out, sizeof( out ),
            "[%02d:%02d:%02d] [crisis-overlay] %s\n",
            t.wHour, t.wMinute, t.wSecond, buffer );

        OutputDebugStringA( out );
    }

    inline RECT get_area_and_size( HWND hwnd ) {
        RECT rect;
        if ( GetClientRect( hwnd, &rect ) ) {
            POINT top_left = { rect.left, rect.top };
            POINT bottom_right = { rect.right, rect.bottom };

            ClientToScreen( hwnd, &top_left );
            ClientToScreen( hwnd, &bottom_right );

            rect.left = top_left.x;
            rect.top = top_left.y;
            rect.right = bottom_right.x;
            rect.bottom = bottom_right.y;
        }
        else {
            rect = { 0, 0, 0, 0 };
        }

        return rect;
    }
}
