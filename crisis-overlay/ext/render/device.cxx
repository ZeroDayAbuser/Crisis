#include <deps/includes.h>
#include "device.hxx"
#include <d2d1helper.h>
#include <dwmapi.h>

#pragma comment( lib, "d2d1.lib" )
#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "dwmapi.lib" )
#pragma comment( lib, "dwrite.lib" )
#pragma comment( lib, "dxgi.lib" )

namespace render {
	c_device::~c_device( ) {
		shutdown( );
	}

	bool c_device::initialize( c_window& window ) {
		if ( m_device || !window.handle( ) )
			return false;

		m_window = &window;
		m_width  = window.width( );
		m_height = window.height( );

		constexpr D3D_FEATURE_LEVEL feature_levels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0
		};
		auto flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined( _DEBUG )
		flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		auto result = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			flags,
			feature_levels,
			ARRAYSIZE( feature_levels ),
			D3D11_SDK_VERSION,
			m_device.GetAddressOf( ),
			nullptr,
			m_device_context.GetAddressOf( )
		);
		if ( FAILED( result ) ) {
			result = D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP,
				nullptr,
				flags & ~D3D11_CREATE_DEVICE_DEBUG,
				feature_levels,
				ARRAYSIZE( feature_levels ),
				D3D11_SDK_VERSION,
				m_device.GetAddressOf( ),
				nullptr,
				m_device_context.GetAddressOf( )
			);
		}
		if ( FAILED( result ) ) {
			shutdown( );
			return false;
		}

		Microsoft::WRL::ComPtr< IDXGIDevice > dxgi_device;
		if ( FAILED( m_device.As( &dxgi_device ) ) ) {
			shutdown( );
			return false;
		}

		D2D1_FACTORY_OPTIONS factory_options{ };
#if defined( _DEBUG )
		factory_options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
		result = D2D1CreateFactory( D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_options, m_d2d_factory.GetAddressOf( ) );
		if ( FAILED( result ) ) {
			shutdown( );
			return false;
		}

		if ( FAILED( m_d2d_factory->CreateDevice( dxgi_device.Get( ), m_d2d_device.GetAddressOf( ) ) ) ||
			// Must use NONE here: ENABLE_MULTITHREADED_OPTIMIZATIONS is incompatible
			// with GDI-compatible render targets (D3D11_RESOURCE_MISC_GDI_COMPATIBLE).
			// That flag defers GPU work in ways that break IDXGISurface1::GetDC, which
			// end_frame() calls to hand the rendered frame to UpdateLayeredWindow.
			FAILED( m_d2d_device->CreateDeviceContext( D2D1_DEVICE_CONTEXT_OPTIONS_NONE, m_d2d_context.GetAddressOf( ) ) ) ) {
			shutdown( );
			return false;
		}

		result = DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof( IDWriteFactory ),
			reinterpret_cast< IUnknown** >( m_text_factory.GetAddressOf( ) )
		);
		if ( SUCCEEDED( result ) ) {
			result = m_text_factory->CreateCustomRenderingParams(
				1.8f,
				0.5f,
				0.0f,
				DWRITE_PIXEL_GEOMETRY_FLAT,
				DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
				m_text_rendering_params.GetAddressOf( )
			);
		}
		if ( FAILED( result ) || !create_target( ) ) {
			shutdown( );
			return false;
		}

		return true;
	}

	void c_device::shutdown( ) {
		if ( m_d2d_context )
			m_d2d_context->SetTarget( nullptr );

		m_custom_collection.Reset( );
		m_font_paths.clear( );
		m_font_names.clear( );
		m_font_name_map.clear( );
		m_font_registrations.clear( );
		m_font_revision = 0;
		m_target.Reset( );
		m_text_rendering_params.Reset( );
		m_text_factory.Reset( );
		m_d2d_context.Reset( );
		m_d2d_device.Reset( );
		m_d2d_factory.Reset( );
		m_render_texture.Reset( );
		m_device_context.Reset( );
		m_device.Reset( );
		m_window = nullptr;
		m_width  = 0;
		m_height = 0;
	}

	bool c_device::create_target( ) {
		// D3D11_RESOURCE_MISC_GDI_COMPATIBLE is what makes IDXGISurface1::GetDC
		// work on this texture in end_frame() -- that's how the rendered frame
		// reaches UpdateLayeredWindow without a DXGI swap chain at all.
		D3D11_TEXTURE2D_DESC desc{ };
		desc.Width            = static_cast< UINT >( m_width );
		desc.Height           = static_cast< UINT >( m_height );
		desc.MipLevels        = 1;
		desc.ArraySize        = 1;
		desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage            = D3D11_USAGE_DEFAULT;
		desc.BindFlags        = D3D11_BIND_RENDER_TARGET;
		desc.MiscFlags        = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;

		if ( FAILED( m_device->CreateTexture2D( &desc, nullptr, m_render_texture.GetAddressOf( ) ) ) )
			return false;

		Microsoft::WRL::ComPtr< IDXGISurface > surface;
		if ( FAILED( m_render_texture.As( &surface ) ) )
			return false;

		// PREMULTIPLIED -- UpdateLayeredWindow's AC_SRC_ALPHA blend mode
		// (see end_frame()) expects premultiplied BGRA, same as what D2D writes
		// here. Real per-pixel alpha, blended by GDI against whatever is actually
		// behind this window on screen.
		auto properties = D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED )
		);
		if ( FAILED( m_d2d_context->CreateBitmapFromDxgiSurface( surface.Get( ), &properties, m_target.GetAddressOf( ) ) ) )
			return false;

		m_d2d_context->SetTarget( m_target.Get( ) );
		m_d2d_context->SetUnitMode( D2D1_UNIT_MODE_PIXELS );
		m_d2d_context->SetAntialiasMode( D2D1_ANTIALIAS_MODE_PER_PRIMITIVE );
		// Layered overlays do not have a stable RGB subpixel background, so
		// grayscale coverage produces cleaner glyph edges than ClearType.
		m_d2d_context->SetTextAntialiasMode( D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE );
		m_d2d_context->SetTextRenderingParams( m_text_rendering_params.Get( ) );
		return true;
	}

	bool c_device::resize( int width, int height ) {
		if ( width <= 0 || height <= 0 )
			return true;

		m_d2d_context->SetTarget( nullptr );
		m_target.Reset( );
		m_render_texture.Reset( );

		m_width  = width;
		m_height = height;
		return create_target( );
	}

	bool c_device::begin_frame( color_t clear ) {
		if ( !m_window || !m_d2d_context )
			return false;

		if ( m_window->consume_resize( ) && !resize( m_window->width( ), m_window->height( ) ) )
			return false;

		// A successful begin/end frame cycle clears the residual failure
		// code so it only reflects the most recent event, not history.
		m_end_frame_failure = end_frame_failure_t::none;
		m_last_hresult = S_OK;

		m_d2d_context->BeginDraw( );
		m_d2d_context->SetTransform( D2D1::Matrix3x2F::Identity( ) );
		m_d2d_context->Clear( D2D1::ColorF( clear.r, clear.g, clear.b, clear.a ) );
		return true;
	}

	bool c_device::reinitialize( ) {
		// Full device rebuild — used when end_frame() reports create_target
		// or end_draw failure whose root cause is a lost D3D device
		// (fullscreen-exclusive game grabbing the display is the common
		// trigger). Tears down every D3D/D2D/DWrite object except the
		// c_window pointer, then walks back through initialize()'s setup
		// and re-registers any fonts that were live.
		if ( !m_window ) return false;
		auto* saved_window = m_window;
		// Save the (client_name, path) pairs BEFORE the teardown wipes
		// them. register_font uses the client_name as the collection key —
		// icon fonts like "Font Awesome" get registered under a client
		// name that differs from the family name embedded in the file, so
		// re-registering with the file's family name breaks every glyph
		// lookup.
		const auto saved_regs = m_font_registrations;

		m_target.Reset( );
		m_text_rendering_params.Reset( );
		m_custom_collection.Reset( );
		m_text_factory.Reset( );
		m_d2d_context.Reset( );
		m_d2d_device.Reset( );
		m_d2d_factory.Reset( );
		m_render_texture.Reset( );
		m_device_context.Reset( );
		m_device.Reset( );
		m_font_names.clear( );
		m_font_name_map.clear( );
		m_font_paths.clear( );
		m_font_registrations.clear( );
		m_font_revision = 0;
		m_end_frame_failure = end_frame_failure_t::none;
		m_last_hresult = S_OK;
		m_window = nullptr;

		if ( !initialize( *saved_window ) )
			return false;

		for ( const auto& [ name, path ] : saved_regs )
			(void)register_font( name, path );
		return true;
	}

	bool c_device::end_frame( ) {
		if ( !m_d2d_context || !m_render_texture || !m_window || !m_window->handle( ) ) {
			m_end_frame_failure = end_frame_failure_t::preconditions;
			return false;
		}

		auto result = m_d2d_context->EndDraw( );
		if ( result == D2DERR_RECREATE_TARGET ) {
			m_d2d_context->SetTarget( nullptr );
			m_target.Reset( );
			m_render_texture.Reset( );
			if ( !create_target( ) ) {
				// create_target uses m_device; if D3D device itself is
				// lost, this fails and the ONLY recovery is a full
				// device rebuild (handled by the render worker on the
				// next frame, keyed off this failure code).
				m_end_frame_failure = end_frame_failure_t::create_target;
				m_last_hresult = m_device ? m_device->GetDeviceRemovedReason( ) : E_FAIL;
				return false;
			}
			return true;
		}
		if ( FAILED( result ) ) {
			m_end_frame_failure = end_frame_failure_t::end_draw;
			m_last_hresult = result;
			return false;
		}

		Microsoft::WRL::ComPtr< IDXGISurface1 > surface;
		result = m_render_texture.As( &surface );
		if ( FAILED( result ) ) {
			m_end_frame_failure = end_frame_failure_t::as_surface;
			m_last_hresult = result;
			return false;
		}

		HDC source_dc = nullptr;
		result = surface->GetDC( FALSE, &source_dc );
		if ( FAILED( result ) ) {
			m_end_frame_failure = end_frame_failure_t::get_dc;
			m_last_hresult = result;
			return false;
		}

		RECT window_rect{ };
		GetWindowRect( m_window->handle( ), &window_rect );

		POINT position{ window_rect.left, window_rect.top };
		POINT source_origin{ 0, 0 };
		SIZE  size{ m_width, m_height };
		BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

		HDC screen_dc = GetDC( nullptr );
		auto updated  = UpdateLayeredWindow(
			m_window->handle( ),
			screen_dc,
			&position,
			&size,
			source_dc,
			&source_origin,
			0,
			&blend,
			ULW_ALPHA
		);
		const auto ulw_err = updated ? 0u : GetLastError( );
		ReleaseDC( nullptr, screen_dc );
		surface->ReleaseDC( nullptr );

		if ( !updated ) {
			m_end_frame_failure = end_frame_failure_t::update_layered_window;
			m_last_hresult = HRESULT_FROM_WIN32( ulw_err );
			return false;
		}
		return true;
	}

	std::wstring c_device::lower( std::wstring_view value ) {
		std::wstring result( value );
		for ( auto& ch : result )
			ch = static_cast< wchar_t >( ::towlower( ch ) );
		return result;
	}

	bool c_device::has_font( std::wstring_view name ) const {
		return m_font_names.find( lower( name ) ) != m_font_names.end( );
	}

	IDWriteFontCollection* c_device::font_collection_for( std::wstring_view family ) const {
		return has_font( family ) ? m_custom_collection.Get( ) : nullptr;
	}

	std::wstring c_device::extract_family_name( const std::wstring& path ) const {
		Microsoft::WRL::ComPtr< IDWriteFactory5 > factory5;
		if ( FAILED( m_text_factory.As( &factory5 ) ) )
			return {};

		Microsoft::WRL::ComPtr< IDWriteFontFile > file;
		if ( FAILED( factory5->CreateFontFileReference( path.c_str( ), nullptr, file.GetAddressOf( ) ) ) )
			return {};

		Microsoft::WRL::ComPtr< IDWriteFontSetBuilder1 > builder;
		if ( FAILED( factory5->CreateFontSetBuilder( builder.GetAddressOf( ) ) ) )
			return {};

		if ( FAILED( builder->AddFontFile( file.Get( ) ) ) )
			return {};

		Microsoft::WRL::ComPtr< IDWriteFontSet > set;
		if ( FAILED( builder->CreateFontSet( set.GetAddressOf( ) ) ) )
			return {};

		Microsoft::WRL::ComPtr< IDWriteFontCollection1 > coll;
		if ( FAILED( factory5->CreateFontCollectionFromFontSet( set.Get( ), coll.GetAddressOf( ) ) ) )
			return {};

		if ( coll->GetFontFamilyCount( ) == 0 )
			return {};

		Microsoft::WRL::ComPtr< IDWriteFontFamily > family;
		if ( FAILED( coll->GetFontFamily( 0, family.GetAddressOf( ) ) ) )
			return {};

		Microsoft::WRL::ComPtr< IDWriteLocalizedStrings > names;
		if ( FAILED( family->GetFamilyNames( names.GetAddressOf( ) ) ) )
			return {};

		UINT32 idx = 0;
		BOOL exists = FALSE;
		names->FindLocaleName( L"en-us", &idx, &exists );
		if ( !exists )
			idx = 0;

		UINT32 len = 0;
		if ( FAILED( names->GetStringLength( idx, &len ) ) )
			return {};

		std::wstring result( len, L'\0' );
		if ( FAILED( names->GetString( idx, result.data( ), len + 1 ) ) )
			return {};

		return result;
	}

	std::wstring c_device::resolve_font_name( std::wstring_view name ) const {
		const auto it = m_font_name_map.find( lower( name ) );
		return it != m_font_name_map.end( ) ? it->second : std::wstring( name );
	}

	bool c_device::register_font( std::wstring_view name, std::wstring_view path ) {
		if ( !m_text_factory || name.empty( ) || path.empty( ) )
			return false;

		const auto key = lower( name );
		if ( m_font_names.find( key ) != m_font_names.end( ) )
			return false;

		std::wstring resolved( path );
		if ( GetFileAttributesW( resolved.c_str( ) ) == INVALID_FILE_ATTRIBUTES )
			return false;

		auto actual_name = extract_family_name( resolved );

		m_font_paths.push_back( resolved );
		m_font_names.insert( key );

		if ( !rebuild_font_collection( ) ) {
			m_font_paths.pop_back( );
			m_font_names.erase( key );
			return false;
		}

		if ( !actual_name.empty( ) )
			m_font_name_map[ key ] = std::move( actual_name );
		m_font_registrations.emplace_back( std::wstring( name ), resolved );
		++m_font_revision;

		return true;
	}

	bool c_device::rebuild_font_collection( ) {
		Microsoft::WRL::ComPtr< IDWriteFactory5 > factory5;
		if ( FAILED( m_text_factory.As( &factory5 ) ) )
			return false;

		Microsoft::WRL::ComPtr< IDWriteFontSetBuilder1 > builder;
		if ( FAILED( factory5->CreateFontSetBuilder( builder.GetAddressOf( ) ) ) )
			return false;

		for ( const auto& path : m_font_paths ) {
			Microsoft::WRL::ComPtr< IDWriteFontFile > file;
			if ( FAILED( factory5->CreateFontFileReference( path.c_str( ), nullptr, file.GetAddressOf( ) ) ) )
				return false;

			if ( FAILED( builder->AddFontFile( file.Get( ) ) ) )
				return false;
		}

		Microsoft::WRL::ComPtr< IDWriteFontSet > set;
		if ( FAILED( builder->CreateFontSet( set.GetAddressOf( ) ) ) )
			return false;

		Microsoft::WRL::ComPtr< IDWriteFontCollection1 > collection;
		if ( FAILED( factory5->CreateFontCollectionFromFontSet( set.Get( ), collection.GetAddressOf( ) ) ) )
			return false;

		m_custom_collection = std::move( collection );
		return true;
	}
}
