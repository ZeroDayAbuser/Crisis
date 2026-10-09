#pragma once
#include <mutex>

#pragma comment( lib, "dwmapi.lib" )

namespace window {
    // Physical rect of a display, in the caller's DPI context. We always populate
    // this while pinned to per-monitor V2, so the numbers are true pixels.
    struct monitor_info_t {
        HMONITOR handle{ nullptr };
        RECT     bounds{ };
        bool     is_primary{ false };
    };

    inline std::vector< monitor_info_t > enumerate_monitors( ) {
        std::vector< monitor_info_t > monitors;
        EnumDisplayMonitors( nullptr, nullptr,
            [ ]( HMONITOR handle, HDC, LPRECT rect, LPARAM lp ) -> BOOL {
                auto* list = reinterpret_cast< std::vector< monitor_info_t >* >( lp );
                MONITORINFO info{ sizeof( info ) };
                if ( !GetMonitorInfoW( handle, &info ) )
                    return TRUE;

                monitor_info_t entry{ };
                entry.handle = handle;
                entry.bounds = info.rcMonitor;
                entry.is_primary = ( info.dwFlags & MONITORINFOF_PRIMARY ) != 0;
                list->push_back( entry );
                return TRUE;
            },
            reinterpret_cast< LPARAM >( &monitors )
        );

        // Deterministic ordering across processes: primary first, then by top-left
        // corner. Both the overlay and the client run enumerate_monitors and index
        // by the same rule, so a "monitor 0" request means the same physical panel
        // on both sides regardless of the order Windows happened to yield them in.
        std::sort( monitors.begin( ), monitors.end( ),
            [ ]( const monitor_info_t& a, const monitor_info_t& b ) {
                if ( a.is_primary != b.is_primary )
                    return a.is_primary;
                if ( a.bounds.top != b.bounds.top )
                    return a.bounds.top < b.bounds.top;
                return a.bounds.left < b.bounds.left;
            } );

        return monitors;
    }

	class c_window {
	public:
		bool attach( ) {
            std::lock_guard< std::mutex > lock( m_mutex );
            if ( m_window_handle )
                return true;

            // Pin this thread to per-monitor V2 BEFORE any window / monitor
            // metric call. The overlay is a DLL and the host process (whatever
            // it happens to be injected into) may default to DPI-unaware; a
            // GetWindowRect / GetSystemMetrics from an unaware caller returns
            // the display's virtualized dimensions instead of the true pixel
            // grid, so the swap chain ends up smaller than the monitor and
            // DWM upscales every drawn point downward-and-right by the DPI
            // scale factor -- the visible offset the aim snapline had. All
            // subsequent screen-space math in this class assumes physical
            // pixels, so this call has to succeed and precede everything.
            SetThreadDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );

            EnumWindows( [ ]( HWND h, LPARAM lp ) -> BOOL {
                wchar_t cls[ 64 ]{};
                GetClassNameW( h, cls, _countof( cls ) );
                if ( wcscmp( cls, L"SystemTray_Main" ) == 0 ) {
                    *reinterpret_cast< HWND* >( lp ) = h;
                    return false;
                }
                return true;
                }, reinterpret_cast< LPARAM >( &m_window_handle ) );

            if ( !m_window_handle ) {
                utility::print( "could not find window" );
                return false;
            }
            DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_NONE;
            DwmSetWindowAttribute( m_window_handle, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof( backdrop ) );

            ACCENT_POLICY accent{ 0, 0, 0, 0 };
            WINDOWCOMPOSITIONATTRIBDATA data{ 19, &accent, sizeof( accent ) };
            auto set_window_composition_attribute = reinterpret_cast< BOOL( WINAPI* )( HWND, WINDOWCOMPOSITIONATTRIBDATA* ) >(
                GetProcAddress( GetModuleHandleW( L"user32.dll" ), "SetWindowCompositionAttribute" )
                );
            if ( set_window_composition_attribute )
                set_window_composition_attribute( m_window_handle, &data );

            // Default to the primary monitor. set_monitor_by_index can move us
            // elsewhere at runtime once the client asks. Prior version used
            // GetDesktopWindow's rect, but a DPI-unaware caller reading that
            // rect returns virtualized coords -- which is exactly what caused
            // the drawn-position offset. Reading MonitorInfo from a per-monitor
            // handle avoids that ambiguity even if some other code path elsewhere
            // hasn't pinned DPI awareness yet.
            const auto monitors = enumerate_monitors( );
            if ( monitors.empty( ) )
                return false;

            m_monitors = monitors;

            // Strip every non-client element the hijacked SystemTray_Main was
            // created with. Without this the window keeps its default caption
            // / border, its CLIENT area sits below-and-inside the caption, and
            // every pixel we push through D2D lands offset down by the caption
            // height (~30px) and right by the border width. WS_POPUP alone
            // means the client area coincides exactly with the window area,
            // so drawing at pixel (X, Y) hits screen pixel (X, Y) with no
            // per-monitor caption offset to cancel out.
            SetWindowLongW( m_window_handle, GWL_STYLE, WS_POPUP );
            SetWindowLongW(
                m_window_handle,
                GWL_EXSTYLE,
                WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT
            );

            // Style changes only take effect on the next frame-change SetWindowPos.
            // apply_monitor() calls SetWindowPos with SWP_FRAMECHANGED so client
            // area recomputes to match the newly-stripped styles before we build
            // the render texture against window.width()/height().
            apply_monitor( 0 );

            // No SetLayeredWindowAttributes call. c_device (ext/render/device.cxx)
            // pushes every rendered frame onto this window with
            // UpdateLayeredWindow(..., ULW_ALPHA), which does real per-pixel
            // alpha blending against whatever is actually behind the window --
            // that's the only transparency mechanism this window needs, and it
            // works on a hijacked window exactly as well as a fresh one (unlike
            // DirectComposition, which needs WS_EX_NOREDIRECTIONBITMAP set at
            // CreateWindowEx time -- impossible to retrofit onto SystemTray_Main,
            // which explorer.exe created). WS_EX_LAYERED above is still required
            // for UpdateLayeredWindow to have any effect; calling
            // SetLayeredWindowAttributes on top of that would just set a static
            // colorkey/constant-alpha attribute that fights the per-pixel one.
            UpdateWindow( m_window_handle );
            ShowWindow( m_window_handle, SW_SHOW );

            utility::print( "setup window: 0x%llx", m_window_handle );
            return true;
		}

        void detach( ) {
            std::lock_guard< std::mutex > lock( m_mutex );
            m_window_handle = nullptr;
            m_left = 0;
            m_top = 0;
            m_width = 0;
            m_height = 0;
            m_monitor_index = 0;
            m_monitors.clear( );
        }

        // Reposition the overlay onto the monitor at `index` in the deterministic
        // ordering built by enumerate_monitors(). The client and this side agree
        // on the same index -> same physical monitor, so passing an index across
        // shared memory is enough for both to converge on the same target panel
        // without shipping HMONITOR handles (which aren't stable across
        // processes anyway).
        bool set_monitor_by_index( std::uint32_t index ) {
            std::lock_guard< std::mutex > lock( m_mutex );
            if ( !m_window_handle )
                return false;

            // Called from the shared-memory worker thread, which is separate
            // from the one that ran attach() -- SetThreadDpiAwarenessContext
            // only affects the calling thread, so re-pin here or EnumDisplay-
            // Monitors below returns virtualized bounds on a non-100%
            // display, and the target monitor's rect is off by the DPI scale.
            SetThreadDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );

            m_monitors = enumerate_monitors( );
            if ( index >= m_monitors.size( ) )
                return false;

            return apply_monitor( index );
        }

        std::uint32_t monitor_index( ) const {
            std::lock_guard< std::mutex > lock( m_mutex );
            return m_monitor_index;
        }

        HWND handle( ) const {
            std::lock_guard< std::mutex > lock( m_mutex );
            return m_window_handle;
        }

        int left( ) const {
            std::lock_guard< std::mutex > lock( m_mutex );
            return m_left;
        }

        int top( ) const {
            std::lock_guard< std::mutex > lock( m_mutex );
            return m_top;
        }

        int width( ) const {
            std::lock_guard< std::mutex > lock( m_mutex );
            return m_width;
        }

        int height( ) const {
            std::lock_guard< std::mutex > lock( m_mutex );
            return m_height;
        }

        bool refresh_size( ) {
            std::lock_guard< std::mutex > lock( m_mutex );
            if ( !m_window_handle )
                return false;

            auto rect = utility::get_area_and_size( m_window_handle );
            auto width = rect.right - rect.left;
            auto height = rect.bottom - rect.top;

            // SystemTray_Main is owned by explorer.exe, not us -- nothing stops
            // explorer's own taskbar/tray layout logic from resizing it out from
            // under us at any time (e.g. back toward its original icon-column
            // dimensions on a side-docked taskbar, which is narrow and tall --
            // matching exactly the kind of shrink reported here). Passively
            // accepting whatever size we observe would propagate that unwanted
            // shrink through consume_resize() -> c_device::resize() and then
            // right back onto the window via UpdateLayeredWindow's psize,
            // locking it in. Re-assert our own bounds instead of following it.
            if ( m_monitor_index < m_monitors.size( ) ) {
                const auto& target = m_monitors[ m_monitor_index ];
                const auto expected_width = target.bounds.right - target.bounds.left;
                const auto expected_height = target.bounds.bottom - target.bounds.top;
                if ( width != expected_width || height != expected_height ) {
                    apply_monitor( m_monitor_index );
                    rect = utility::get_area_and_size( m_window_handle );
                    width = rect.right - rect.left;
                    height = rect.bottom - rect.top;
                }
            }

            auto resized = width != m_width || height != m_height;
            m_width = width;
            m_height = height;
            return resized;
        }

	private:
		HWND m_window_handle = nullptr;
        int m_left = 0;
        int m_top = 0;
        int m_width = 0;
        int m_height = 0;
        std::uint32_t m_monitor_index = 0;
        std::vector< monitor_info_t > m_monitors{ };
        mutable std::mutex m_mutex{ };

        bool apply_monitor( std::uint32_t index ) {
            if ( index >= m_monitors.size( ) )
                return false;

            const auto& target = m_monitors[ index ];
            const auto width = target.bounds.right - target.bounds.left;
            const auto height = target.bounds.bottom - target.bounds.top;

            // SWP_FRAMECHANGED forces a WM_NCCALCSIZE / non-client recompute after
            // the GWL_STYLE change in attach() so the client area actually shrinks
            // to zero-margin. Without it the caption/border metrics stick until
            // the next natural size change and the offset returns.
            if ( !SetWindowPos(
                m_window_handle,
                HWND_TOPMOST,
                target.bounds.left,
                target.bounds.top,
                width,
                height,
                SWP_NOACTIVATE | SWP_FRAMECHANGED
            ) )
                return false;

            m_monitor_index = index;
            m_left = target.bounds.left;
            m_top = target.bounds.top;

            // Report the true CLIENT area, not the outer window rect. With
            // WS_POPUP these should coincide, but reading GetClientRect is
            // the definitive answer -- if any residual DWM extension or
            // borderless-hint frame remains, this is what the swap chain
            // and the client's coordinate system need to match.
            RECT client{ };
            if ( GetClientRect( m_window_handle, &client ) ) {
                m_width = client.right - client.left;
                m_height = client.bottom - client.top;
            } else {
                m_width = width;
                m_height = height;
            }

            return true;
        }

	};
}
