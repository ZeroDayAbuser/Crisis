#pragma once

#include "types.hxx"
#include "window.hxx"
#include <d2d1_1.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dwrite_3.h>
#include <dxgi1_2.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <wrl/client.h>

namespace render {
	class c_device {
	public:
		c_device( ) = default;
		~c_device( );

		c_device( const c_device& ) = delete;
		c_device& operator=( const c_device& ) = delete;

		bool initialize( c_window& window );
		void shutdown( );
		bool begin_frame( color_t clear );
		bool end_frame( );

		// Full D3D+D2D device teardown-and-rebuild, used by the render
		// worker when end_frame() reports a device-loss category failure
		// (Fortnite going exclusive-fullscreen is the common trigger).
		// Preserves the c_window pointer and re-registers any fonts that
		// were live before the teardown so callers don't need to
		// re-register them.
		bool reinitialize( );

		// Categorised failure reason from the most recent end_frame()
		// failure. Cleared to `none` on any successful end_frame().
		enum class end_frame_failure_t {
			none,
			preconditions,           // ctx/target/window/handle went null
			end_draw,                // EndDraw returned a non-recreate HRESULT
			create_target,           // recreate-target path failed → device likely lost
			as_surface,              // m_render_texture.As<IDXGISurface1> failed
			get_dc,                  // IDXGISurface1::GetDC failed
			update_layered_window,   // UpdateLayeredWindow returned FALSE
		};
		end_frame_failure_t last_end_frame_failure( ) const { return m_end_frame_failure; }
		HRESULT             last_hresult( )            const { return m_last_hresult; }

		ID2D1DeviceContext* context( )      const { return m_d2d_context.Get( ); }
		ID2D1Factory1*      factory( )      const { return m_d2d_factory.Get( ); }
		IDWriteFactory*     text_factory( ) const { return m_text_factory.Get( ); }

		// D3D11 handles for the 3D scene renderer (render::c_scene3d). The
		// scene renders into its own off-screen ID3D11Texture2D and hands
		// the result back as an ID2D1Bitmap so it can be composited in the
		// existing D2D pass alongside every other draw. Both accessors
		// return the same live objects the D2D device/context are built on
		// so DXGI interop resource-shares "just work".
		ID3D11Device*        d3d_device( )  const { return m_device.Get( ); }
		ID3D11DeviceContext* d3d_context( ) const { return m_device_context.Get( ); }
		ID2D1Device*         d2d_device( )  const { return m_d2d_device.Get( ); }
		IDWriteFontCollection* font_collection( ) const { return m_custom_collection.Get( ); }
		IDWriteFontCollection* font_collection_for( std::wstring_view family ) const;
		bool register_font( std::wstring_view name, std::wstring_view path );
		bool has_font( std::wstring_view name ) const;
		std::uint64_t font_revision( ) const { return m_font_revision; }
		std::wstring resolve_font_name( std::wstring_view name ) const;
		int width( )  const { return m_width; }
		int height( ) const { return m_height; }
		void set_interactive_area( rect_t area ) { if ( m_window ) m_window->set_interactive_area( area ); }
		void minimize_window( ) { if ( m_window ) m_window->minimize( ); }
		void bring_to_top( )    { if ( m_window ) m_window->bring_to_top( ); }

	private:
		bool create_target( );
		bool resize( int width, int height );

		c_window* m_window = nullptr;
		int m_width  = 0;
		int m_height = 0;

		Microsoft::WRL::ComPtr< ID3D11Device >        m_device;
		Microsoft::WRL::ComPtr< ID3D11DeviceContext > m_device_context;

		// GDI-compatible render target, not a DXGI swap chain. Neither
		// DirectComposition (needs WS_EX_NOREDIRECTIONBITMAP, which can only
		// be set at CreateWindowEx time -- impossible on a window this DLL
		// didn't create) nor a plain HWND-bound swap chain (DWM doesn't
		// reliably carry its alpha channel through) work on the hijacked
		// window this overlay actually attaches to. UpdateLayeredWindow
		// works on any WS_EX_LAYERED window regardless of how it was
		// created, and does real per-pixel alpha compositing -- end_frame()
		// pulls an HDC off this texture via IDXGISurface1::GetDC and pushes
		// it with AC_SRC_ALPHA instead of calling Present().
		Microsoft::WRL::ComPtr< ID3D11Texture2D >        m_render_texture;
		Microsoft::WRL::ComPtr< ID2D1Factory1 >          m_d2d_factory;
		Microsoft::WRL::ComPtr< ID2D1Device >            m_d2d_device;
		Microsoft::WRL::ComPtr< ID2D1DeviceContext >     m_d2d_context;
		Microsoft::WRL::ComPtr< ID2D1Bitmap1 >           m_target;
		Microsoft::WRL::ComPtr< IDWriteFactory >         m_text_factory;
		Microsoft::WRL::ComPtr< IDWriteRenderingParams > m_text_rendering_params;
		Microsoft::WRL::ComPtr< IDWriteFontCollection1 > m_custom_collection;
		std::vector< std::wstring >                      m_font_paths;
		std::unordered_set< std::wstring >               m_font_names;
		std::unordered_map< std::wstring, std::wstring > m_font_name_map;
		// Client-provided (name, path) pairs. reinitialize() replays this
		// list so a device rebuild restores fonts under the SAME names
		// the client originally used (matters for icon fonts registered
		// as e.g. "Font Awesome" whose file family is different).
		std::vector< std::pair< std::wstring, std::wstring > > m_font_registrations;
		std::uint64_t m_font_revision = 0;

		end_frame_failure_t m_end_frame_failure = end_frame_failure_t::none;
		HRESULT             m_last_hresult      = S_OK;

		static std::wstring lower( std::wstring_view value );
		bool rebuild_font_collection( );
		std::wstring extract_family_name( const std::wstring& path ) const;
	};
}
