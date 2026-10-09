#include <deps/includes.h>
#include "renderer.hxx"
#include <d2d1helper.h>
#include <d2d1_1helper.h>
#include <d2d1effects.h>
// D2D1 Tint effect (CLSID_D2D1Tint + D2D1_TINT_PROP_*) shipped in
// Direct2D 1.2 (Windows 8.1 SDK) inside d2d1effects_2.h; the older
// d2d1effects.h above only declares the 1.1 effects.
#include <d2d1effects_2.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <wincodec.h>

#pragma comment( lib, "windowscodecs.lib" )
// dxguid.lib defines the CLSID_D2D1* effect GUID symbols referenced below.
// Without it the effect CLSIDs link as unresolved externals.
#pragma comment( lib, "dxguid.lib" )

namespace render {
	namespace {
		// Called once when a layout enters the cache. Captures shaped glyph runs
		// as vector geometry at origin (0,0). Later frames only translate and draw
		// that geometry -- no per-frame font rasterization or glyph outline rebuild.
		class c_outline_geometry_builder : public IDWriteTextRenderer {
		public:
			explicit c_outline_geometry_builder( ID2D1Factory* factory ) : m_factory( factory ) { }

			HRESULT STDMETHODCALLTYPE QueryInterface( REFIID iid, void** object ) override {
				if ( !object )
					return E_POINTER;
				*object = nullptr;
				if ( IsEqualIID( iid, __uuidof( IUnknown ) ) ||
					IsEqualIID( iid, __uuidof( IDWritePixelSnapping ) ) ||
					IsEqualIID( iid, __uuidof( IDWriteTextRenderer ) ) ) {
					*object = static_cast< IDWriteTextRenderer* >( this );
					return S_OK;
				}
				return E_NOINTERFACE;
			}
			ULONG STDMETHODCALLTYPE AddRef( ) override { return 1; }
			ULONG STDMETHODCALLTYPE Release( ) override { return 1; }

			HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled( void*, BOOL* is_disabled ) override {
				*is_disabled = FALSE;
				return S_OK;
			}
			HRESULT STDMETHODCALLTYPE GetCurrentTransform( void*, DWRITE_MATRIX* transform ) override {
				if ( !transform )
					return E_POINTER;
				*transform = DWRITE_MATRIX{ 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
				return S_OK;
			}
			HRESULT STDMETHODCALLTYPE GetPixelsPerDip( void*, FLOAT* pixels_per_dip ) override {
				*pixels_per_dip = 1.0f;
				return S_OK;
			}

			HRESULT STDMETHODCALLTYPE DrawGlyphRun(
				void*,
				FLOAT baseline_x,
				FLOAT baseline_y,
				DWRITE_MEASURING_MODE,
				const DWRITE_GLYPH_RUN* glyph_run,
				const DWRITE_GLYPH_RUN_DESCRIPTION*,
				IUnknown* ) override
			{
				Microsoft::WRL::ComPtr< ID2D1PathGeometry > geometry;
				if ( FAILED( m_factory->CreatePathGeometry( geometry.GetAddressOf( ) ) ) )
					return E_FAIL;

				Microsoft::WRL::ComPtr< ID2D1GeometrySink > sink;
				if ( FAILED( geometry->Open( sink.GetAddressOf( ) ) ) )
					return E_FAIL;

				const auto outline_result = glyph_run->fontFace->GetGlyphRunOutline(
					glyph_run->fontEmSize,
					glyph_run->glyphIndices,
					glyph_run->glyphAdvances,
					glyph_run->glyphOffsets,
					glyph_run->glyphCount,
					glyph_run->isSideways,
					( glyph_run->bidiLevel % 2 ) != 0,
					sink.Get( ) );
				if ( FAILED( outline_result ) )
					return outline_result;

				if ( FAILED( sink->Close( ) ) )
					return E_FAIL;

				Microsoft::WRL::ComPtr< ID2D1TransformedGeometry > transformed;
				if ( FAILED( m_factory->CreateTransformedGeometry(
					geometry.Get( ),
					D2D1::Matrix3x2F::Translation( baseline_x, baseline_y ),
					transformed.GetAddressOf( ) ) ) )
					return E_FAIL;

				m_geometries.push_back( std::move( transformed ) );
				return S_OK;
			}

			Microsoft::WRL::ComPtr< ID2D1Geometry > finish( ) const {
				Microsoft::WRL::ComPtr< ID2D1Geometry > result;
				if ( m_geometries.empty( ) )
					return result;

				std::vector< ID2D1Geometry* > geometries;
				geometries.reserve( m_geometries.size( ) );
				for ( const auto& geometry : m_geometries )
					geometries.push_back( geometry.Get( ) );

				Microsoft::WRL::ComPtr< ID2D1GeometryGroup > group;
				if ( SUCCEEDED( m_factory->CreateGeometryGroup(
					D2D1_FILL_MODE_WINDING,
					geometries.data( ),
					static_cast< UINT32 >( geometries.size( ) ),
					group.GetAddressOf( )
				) ) )
					result = std::move( group );
				return result;
			}

			HRESULT STDMETHODCALLTYPE DrawUnderline( void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown* ) override { return S_OK; }
			HRESULT STDMETHODCALLTYPE DrawStrikethrough( void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown* ) override { return S_OK; }
			HRESULT STDMETHODCALLTYPE DrawInlineObject( void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown* ) override { return S_OK; }

		private:
			ID2D1Factory* m_factory;
			std::vector< Microsoft::WRL::ComPtr< ID2D1Geometry > > m_geometries;
		};
	}

	std::size_t c_renderer::text_format_key_hash_t::operator()( const text_format_key_t& key ) const {
		auto combine = []( std::size_t& seed, std::size_t value ) {
			seed ^= value + 0x9e3779b9u + ( seed << 6 ) + ( seed >> 2 );
		};

		std::size_t result = std::hash< std::wstring >{}( key.family );
		combine( result, std::hash< float >{}( key.size ) );
		combine( result, std::hash< int >{}( static_cast< int >( key.weight ) ) );
		combine( result, std::hash< int >{}( static_cast< int >( key.alignment ) ) );
		combine( result, std::hash< bool >{}( key.wrap ) );
		return result;
	}

	std::size_t c_renderer::text_layout_key_hash_t::operator()( const text_layout_key_t& key ) const {
		auto combine = []( std::size_t& seed, std::size_t value ) {
			seed ^= value + 0x9e3779b9u + ( seed << 6 ) + ( seed >> 2 );
		};

		std::size_t result = std::hash< std::wstring >{}( key.value );
		combine( result, text_format_key_hash_t{}( key.format ) );
		combine( result, std::hash< float >{}( key.width ) );
		combine( result, std::hash< float >{}( key.height ) );
		return result;
	}

	c_renderer::~c_renderer( ) {
		shutdown( );
	}

	bool c_renderer::initialize( c_device& device ) {
		if ( m_device || !device.context( ) )
			return false;

		m_device = &device;
		m_font_revision = device.font_revision( );
		if ( FAILED( device.context( )->CreateSolidColorBrush( D2D1::ColorF( 1.0f, 1.0f, 1.0f, 1.0f ), m_brush.GetAddressOf( ) ) ) ||
			FAILED( device.context( )->CreateSolidColorBrush( D2D1::ColorF( 0.0f, 0.0f, 0.0f, 1.0f ), m_outline_brush.GetAddressOf( ) ) ) ) {
			m_device = nullptr;
			return false;
		}

		// Pre-build the round-cap stroke style once so arc() doesn't create it per call.
		if ( auto* fac = device.factory( ) ) {
			D2D1_STROKE_STYLE_PROPERTIES props = D2D1::StrokeStyleProperties( );
			props.startCap = D2D1_CAP_STYLE_ROUND;
			props.endCap   = D2D1_CAP_STYLE_ROUND;
			props.dashCap  = D2D1_CAP_STYLE_ROUND;
			fac->CreateStrokeStyle( props, nullptr, 0, m_round_cap_style.GetAddressOf( ) );
		}

		return true;
	}

	void c_renderer::shutdown( ) {
		m_clip_active = false;
		m_opacity_layer_count = 0;
		m_aa_clip_stack.clear( );
		m_clip_geometry.Reset( );
		clear_text_cache( );
		m_round_cap_style.Reset( );
		m_brush.Reset( );
		m_outline_brush.Reset( );
		m_device = nullptr;
	}

	bool c_renderer::begin_frame( color_t clear ) {
		if ( !m_device )
			return false;
		m_clip_active = false;
		m_opacity_layer_count = 0;
		m_aa_clip_stack.clear( );

		if ( m_font_revision != m_device->font_revision( ) ) {
			clear_text_cache( );
			m_font_revision = m_device->font_revision( );
		}
		return m_device->begin_frame( clear );
	}

	bool c_renderer::end_frame( ) {
		if ( !m_device )
			return false;

		// Never let a malformed or truncated command batch leave D2D's
		// clip/layer stack unbalanced; D2D reports that later as WRONG_STATE.
		while ( !m_aa_clip_stack.empty( ) )
			pop_axis_aligned_clip( );
		if ( m_clip_active )
			pop_clip( );
		while ( m_opacity_layer_count )
			pop_opacity( );
		return m_device->end_frame( );
	}

	int c_renderer::width( ) const {
		return m_device ? m_device->width( ) : 0;
	}

	int c_renderer::height( ) const {
		return m_device ? m_device->height( ) : 0;
	}

	ID2D1SolidColorBrush* c_renderer::brush( color_t color ) {
		if ( !m_brush )
			return nullptr;

		m_brush->SetColor( D2D1::ColorF( color.r, color.g, color.b, color.a ) );
		return m_brush.Get( );
	}

	ID2D1SolidColorBrush* c_renderer::outline_brush( color_t color ) {
		if ( !m_outline_brush )
			return nullptr;

		m_outline_brush->SetColor( D2D1::ColorF( color.r, color.g, color.b, color.a ) );
		return m_outline_brush.Get( );
	}

	D2D1_RECT_F c_renderer::native_rect( rect_t area ) const {
		return D2D1::RectF( area.x, area.y, area.x + area.width, area.y + area.height );
	}

	void c_renderer::line( point_t from, point_t to, color_t color, float thickness ) {
		if ( !m_device || thickness <= 0.0f )
			return;

		m_device->context( )->DrawLine( D2D1::Point2F( from.x, from.y ), D2D1::Point2F( to.x, to.y ), brush( color ), thickness );
	}

	void c_renderer::skeleton(
		const communication::skeleton_request_t& request,
		color_t color,
		color_t outline_color ) {
		if ( !m_device || request.thickness <= 0.0f || request.count == 0 )
			return;

		auto* factory = m_device->factory( );
		Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
		Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
		if ( !factory || FAILED( factory->CreatePathGeometry( geometry.GetAddressOf( ) ) ) ||
			FAILED( geometry->Open( sink.GetAddressOf( ) ) ) )
			return;

		const auto count = ( std::min )(
			static_cast<std::size_t>( request.count ),
			communication::skeleton_segment_capacity );
		for ( std::size_t i = 0; i < count; ++i ) {
			const auto& segment = request.segments[ i ];
			sink->BeginFigure(
				D2D1::Point2F( segment.from.x, segment.from.y ),
				D2D1_FIGURE_BEGIN_HOLLOW );
			sink->AddLine( D2D1::Point2F( segment.to.x, segment.to.y ) );
			sink->EndFigure( D2D1_FIGURE_END_OPEN );
		}
		if ( FAILED( sink->Close( ) ) )
			return;

		auto* context = m_device->context( );
		if ( request.outline )
			context->DrawGeometry( geometry.Get( ), brush( outline_color ), request.thickness + 1.0f );
		context->DrawGeometry( geometry.Get( ), brush( color ), request.thickness );
	}

	void c_renderer::rectangle( rect_t area, color_t color, float thickness ) {
		if ( !m_device || thickness <= 0.0f )
			return;

		m_device->context( )->DrawRectangle( native_rect( area ), brush( color ), thickness );
	}

	void c_renderer::filled_rectangle( rect_t area, color_t color ) {
		if ( m_device )
			m_device->context( )->FillRectangle( native_rect( area ), brush( color ) );
	}

	void c_renderer::rounded_rectangle( rect_t area, float radius, color_t color, float thickness ) {
		if ( !m_device || thickness <= 0.0f )
			return;

		auto rounded = D2D1::RoundedRect( native_rect( area ), radius, radius );
		m_device->context( )->DrawRoundedRectangle( rounded, brush( color ), thickness );
	}

	void c_renderer::filled_rounded_rectangle( rect_t area, float radius, color_t color ) {
		if ( !m_device )
			return;

		auto rounded = D2D1::RoundedRect( native_rect( area ), radius, radius );
		m_device->context( )->FillRoundedRectangle( rounded, brush( color ) );
	}

	// ── Two-stop linear gradient fills ──────────────────────────────────────
	// Vertical variant  (color_a top  → color_b bottom).
	// Horizontal variant (color_a left → color_b right).
	// Creates the gradient stop collection and brush inline — these are tiny
	// COM objects; creation cost is negligible compared to the blur passes
	// used by aurora / frosted_panel.
	void c_renderer::filled_gradient_rectangle( rect_t area, color_t color_a, color_t color_b, float corner_radius ) {
		if ( !m_device || area.width <= 0.0f || area.height <= 0.0f )
			return;

		auto* ctx = m_device->context( );
		const D2D1_GRADIENT_STOP stops[ 2 ] = {
			{ 0.0f, D2D1::ColorF( color_a.r, color_a.g, color_a.b, color_a.a ) },
			{ 1.0f, D2D1::ColorF( color_b.r, color_b.g, color_b.b, color_b.a ) }
		};

		Microsoft::WRL::ComPtr< ID2D1GradientStopCollection > stop_col;
		if ( FAILED( ctx->CreateGradientStopCollection(
				stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP,
				stop_col.GetAddressOf( ) ) ) )
			return;

		Microsoft::WRL::ComPtr< ID2D1LinearGradientBrush > grad;
		if ( FAILED( ctx->CreateLinearGradientBrush(
				D2D1::LinearGradientBrushProperties(
					D2D1::Point2F( area.x,              area.y              ),
					D2D1::Point2F( area.x,              area.y + area.height ) ),
				stop_col.Get( ), grad.GetAddressOf( ) ) ) )
			return;

		const auto r = native_rect( area );
		if ( corner_radius > 0.5f )
			ctx->FillRoundedRectangle( D2D1::RoundedRect( r, corner_radius, corner_radius ), grad.Get( ) );
		else
			ctx->FillRectangle( r, grad.Get( ) );
	}

	void c_renderer::filled_gradient_rectangle_h( rect_t area, color_t color_a, color_t color_b, float corner_radius ) {
		if ( !m_device || area.width <= 0.0f || area.height <= 0.0f )
			return;

		auto* ctx = m_device->context( );
		const D2D1_GRADIENT_STOP stops[ 2 ] = {
			{ 0.0f, D2D1::ColorF( color_a.r, color_a.g, color_a.b, color_a.a ) },
			{ 1.0f, D2D1::ColorF( color_b.r, color_b.g, color_b.b, color_b.a ) }
		};

		Microsoft::WRL::ComPtr< ID2D1GradientStopCollection > stop_col;
		if ( FAILED( ctx->CreateGradientStopCollection(
				stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP,
				stop_col.GetAddressOf( ) ) ) )
			return;

		Microsoft::WRL::ComPtr< ID2D1LinearGradientBrush > grad;
		if ( FAILED( ctx->CreateLinearGradientBrush(
				D2D1::LinearGradientBrushProperties(
					D2D1::Point2F( area.x,              area.y ),
					D2D1::Point2F( area.x + area.width, area.y ) ),
				stop_col.Get( ), grad.GetAddressOf( ) ) ) )
			return;

		const auto r = native_rect( area );
		if ( corner_radius > 0.5f )
			ctx->FillRoundedRectangle( D2D1::RoundedRect( r, corner_radius, corner_radius ), grad.Get( ) );
		else
			ctx->FillRectangle( r, grad.Get( ) );
	}

	void c_renderer::circle( point_t center, float radius, color_t color, float thickness ) {
		if ( !m_device || radius <= 0.0f || thickness <= 0.0f )
			return;

		m_device->context( )->DrawEllipse( D2D1::Ellipse( D2D1::Point2F( center.x, center.y ), radius, radius ), brush( color ), thickness );
	}

	void c_renderer::arc( point_t center, float radius, float start_angle, float sweep_angle, color_t color, float thickness ) {
		if ( !m_device || radius <= 0.0f || thickness <= 0.0f || std::abs( sweep_angle ) <= 0.0001f )
			return;

		auto* factory = m_device->factory( );
		Microsoft::WRL::ComPtr< ID2D1PathGeometry > geometry;
		Microsoft::WRL::ComPtr< ID2D1GeometrySink > sink;
		if ( !factory || FAILED( factory->CreatePathGeometry( geometry.GetAddressOf( ) ) ) ||
			FAILED( geometry->Open( sink.GetAddressOf( ) ) ) )
			return;

		const auto end_angle = start_angle + sweep_angle;
		const auto start = D2D1::Point2F(
			center.x + std::cos( start_angle ) * radius,
			center.y + std::sin( start_angle ) * radius
		);
		const auto end = D2D1::Point2F(
			center.x + std::cos( end_angle ) * radius,
			center.y + std::sin( end_angle ) * radius
		);
		sink->BeginFigure( start, D2D1_FIGURE_BEGIN_HOLLOW );
		sink->AddArc( D2D1::ArcSegment(
			end,
			D2D1::SizeF( radius, radius ),
			0.0f,
			sweep_angle >= 0.0f ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
			std::abs( sweep_angle ) > 3.14159265358979323846f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL
		) );
		sink->EndFigure( D2D1_FIGURE_END_OPEN );
		if ( FAILED( sink->Close( ) ) )
			return;

		m_device->context( )->DrawGeometry( geometry.Get( ), brush( color ), thickness, m_round_cap_style.Get( ) );
	}

	void c_renderer::filled_circle( point_t center, float radius, color_t color ) {
		if ( !m_device || radius <= 0.0f )
			return;

		m_device->context( )->FillEllipse( D2D1::Ellipse( D2D1::Point2F( center.x, center.y ), radius, radius ), brush( color ) );
	}

	image_t c_renderer::load_image( std::wstring_view path ) {
		// WIC decodes into PBGRA. That single format serves both draw
		// paths:
		//   • Plain draw_image() uses DrawBitmap, which honours RGB and
		//     alpha as-is → full-colour blit.
		//   • Tinted draw_image() uses FillOpacityMask, which reads only
		//     the alpha channel and ignores RGB → solid-tint silhouette.
		// No mask preprocessing is required for the silhouette path
		// because FillOpacityMask does not sample RGB at all.
		image_t image;
		if ( !m_device || path.empty( ) )
			return image;

		std::wstring resolved_path( path );
		if ( resolved_path.size( ) < 2 || resolved_path[ 1 ] != L':' ) {
			wchar_t module_path[ MAX_PATH ]{ };
			auto length = GetModuleFileNameW( nullptr, module_path, MAX_PATH );
			if ( !length || length == MAX_PATH )
				return image;

			resolved_path.assign( module_path, length );
			auto separator = resolved_path.find_last_of( L"\\/" );
			if ( separator == std::wstring::npos )
				return image;

			resolved_path.resize( separator + 1 );
			resolved_path.append( path );
		}

		Microsoft::WRL::ComPtr< IWICImagingFactory > factory;
		Microsoft::WRL::ComPtr< IWICBitmapDecoder > decoder;
		Microsoft::WRL::ComPtr< IWICBitmapFrameDecode > frame;
		Microsoft::WRL::ComPtr< IWICFormatConverter > converter;

		if ( FAILED( CoCreateInstance( CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( factory.GetAddressOf( ) ) ) ) ||
			FAILED( factory->CreateDecoderFromFilename( resolved_path.c_str( ), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf( ) ) ) ||
			FAILED( decoder->GetFrame( 0, frame.GetAddressOf( ) ) ) ||
			FAILED( factory->CreateFormatConverter( converter.GetAddressOf( ) ) ) ||
			FAILED( converter->Initialize( frame.Get( ), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom ) ) )
			return image;

		if ( FAILED( m_device->context( )->CreateBitmapFromWicBitmap(
			converter.Get( ), nullptr, image.GetAddressOf( ) ) ) )
			image.Reset( );
		return image;
	}

	void c_renderer::draw_raw_bitmap( ID2D1Bitmap1* bitmap, rect_t area, float opacity ) {
		if ( !m_device || !bitmap || area.width <= 0.0f || area.height <= 0.0f || opacity <= 0.0f )
			return;
		if ( opacity > 1.0f )
			opacity = 1.0f;
		auto destination = native_rect( area );
		m_device->context( )->DrawBitmap(
			bitmap,
			&destination,
			opacity,
			D2D1_INTERPOLATION_MODE_LINEAR,
			nullptr,
			nullptr
		);
	}

	void c_renderer::draw_image( const image_t& image, rect_t area, float opacity ) {
		if ( !m_device || !image || area.width <= 0.0f || area.height <= 0.0f || opacity <= 0.0f )
			return;

		if ( opacity > 1.0f )
			opacity = 1.0f;

		auto destination = native_rect( area );
		m_device->context( )->DrawBitmap(
			image.Get( ),
			&destination,
			opacity,
			D2D1_INTERPOLATION_MODE_LINEAR,
			nullptr,
			nullptr
		);
	}

	void c_renderer::draw_image( const image_t& image, rect_t area, color_t tint, float opacity ) {
		if ( !m_device || !image || area.width <= 0.0f || area.height <= 0.0f || opacity <= 0.0f )
			return;

		if ( opacity > 1.0f )
			opacity = 1.0f;

		auto* ctx = m_device->context( );

		// FillOpacityMask paints a solid-colour brush through the bitmap's
		// ALPHA channel — RGB of the bitmap is ignored entirely. That's
		// exactly what we want for a recoloured silhouette: whatever shape
		// the PNG has, drawn as a flat `tint` colour, no source-colour
		// bleed possible because the RGB channels never contribute.
		//
		// FillOpacityMask requires aliased antialiasing. Violating that
		// requirement is deferred until EndDraw as D2DERR_WRONG_STATE.
		//
		// `render::color_t` stores channels as FLOAT in [0,1]; `opacity` is
		// already 0-1 and clamped above. Simple float multiply preserves
		// the fractional alpha needed during fade animations (an earlier
		// version cast through uint8_t and rounded 0.25 → 0.0, which
		// silently blanked the logo whenever the sidebar was mid-fade).
		color_t effective_tint = tint;
		effective_tint.a = tint.a * opacity;

		const auto destination = native_rect( area );
		const auto src_size = image->GetSize( );
		if ( src_size.width <= 0.0f || src_size.height <= 0.0f )
			return;
		D2D1_RECT_F source = { 0.0f, 0.0f, src_size.width, src_size.height };

		const auto previous_antialias_mode = ctx->GetAntialiasMode( );
		ctx->SetAntialiasMode( D2D1_ANTIALIAS_MODE_ALIASED );
		ctx->FillOpacityMask( image.Get( ), brush( effective_tint ), &destination, &source );
		ctx->SetAntialiasMode( previous_antialias_mode );
	}

	void c_renderer::push_opacity( float opacity ) {
		if ( !m_device )
			return;

		opacity = ( std::max )( 0.0f, ( std::min )( 1.0f, opacity ) );
		D2D1_LAYER_PARAMETERS1 parameters{ };
		parameters.contentBounds = D2D1::InfiniteRect( );
		parameters.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
		parameters.maskTransform = D2D1::Matrix3x2F::Identity( );
		parameters.opacity = opacity;
		parameters.layerOptions = D2D1_LAYER_OPTIONS1_NONE;
		m_device->context( )->PushLayer( parameters, nullptr );
		++m_opacity_layer_count;
	}

	void c_renderer::pop_opacity( ) {
		if ( !m_device || !m_opacity_layer_count )
			return;
		m_device->context( )->PopLayer( );
		--m_opacity_layer_count;
	}

	bool c_renderer::push_rounded_clip( rect_t area, float radius ) {
		if ( !m_device || m_clip_active || area.width <= 0.0f || area.height <= 0.0f || radius < 0.0f )
			return false;

		auto* factory = m_device->factory( );
		if ( !factory )
			return false;

		auto native = native_rect( area );
		auto rounded_rect = D2D1::RoundedRect( native, radius, radius );

		Microsoft::WRL::ComPtr<ID2D1RoundedRectangleGeometry> rounded_geom;
		if ( FAILED( factory->CreateRoundedRectangleGeometry( &rounded_rect, &rounded_geom ) ) )
			return false;

		m_clip_geometry = rounded_geom; // Safely assigns to ID2D1Geometry

		D2D1_LAYER_PARAMETERS1 parameters { };
		parameters.contentBounds = D2D1::InfiniteRect( );
		parameters.geometricMask = m_clip_geometry.Get( );
		parameters.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
		parameters.maskTransform = D2D1::Matrix3x2F::Identity( );
		parameters.opacity = 1.0f;
		parameters.layerOptions = D2D1_LAYER_OPTIONS1_NONE;

		m_device->context( )->PushLayer( parameters, nullptr );
		m_clip_active = true;
		return true;
	}

	void c_renderer::pop_clip( ) {
		if ( !m_device || !m_clip_active )
			return;

		m_device->context( )->PopLayer( );
		m_clip_active = false;
		m_clip_geometry.Reset( );
	}

	bool c_renderer::push_axis_aligned_clip( rect_t area ) {
		const auto valid = m_device &&
			std::isfinite( area.x ) && std::isfinite( area.y ) &&
			std::isfinite( area.width ) && std::isfinite( area.height ) &&
			area.width > 0.0f && area.height > 0.0f;
		m_aa_clip_stack.push_back( valid );
		if ( !valid )
			return false;

		// Use D2D's native axis-aligned clip which supports unlimited nesting
		// (unlike PushLayer which was previously guarded by m_clip_active).
		auto native = native_rect( area );
		m_device->context( )->PushAxisAlignedClip( native, D2D1_ANTIALIAS_MODE_ALIASED );
		return true;
	}

	void c_renderer::pop_axis_aligned_clip( ) {
		if ( !m_device || m_aa_clip_stack.empty( ) )
			return;

		const auto pushed = m_aa_clip_stack.back( );
		m_aa_clip_stack.pop_back( );
		if ( pushed )
			m_device->context( )->PopAxisAlignedClip( );
	}

	void c_renderer::interactive_area( rect_t area ) {
		if ( m_device )
			m_device->set_interactive_area( area );
	}

	void c_renderer::minimize_window( ) {
		if ( m_device )
			m_device->minimize_window( );
	}

	void c_renderer::bring_to_top( ) {
		if ( m_device )
			m_device->bring_to_top( );
	}

	IDWriteTextFormat* c_renderer::text_format( const text_format_key_t& key ) {
		const auto found = m_text_formats.find( key );
		if ( found != m_text_formats.end( ) )
			return found->second.Get( );

		const auto resolved_family = m_device->resolve_font_name( key.family );
		auto* collection = m_device->font_collection_for( key.family );
		Microsoft::WRL::ComPtr< IDWriteTextFormat > format;
		if ( FAILED( m_device->text_factory( )->CreateTextFormat(
			resolved_family.c_str( ), collection, key.weight,
			DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
			key.size, L"en-us", format.GetAddressOf( )
		) ) )
			return nullptr;

		format->SetTextAlignment(
			key.alignment == text_align_t::center ? DWRITE_TEXT_ALIGNMENT_CENTER :
			key.alignment == text_align_t::right ? DWRITE_TEXT_ALIGNMENT_TRAILING :
			DWRITE_TEXT_ALIGNMENT_LEADING );
		format->SetParagraphAlignment( DWRITE_PARAGRAPH_ALIGNMENT_CENTER );
		format->SetWordWrapping( key.wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP );

		auto [ it, inserted ] = m_text_formats.emplace( key, std::move( format ) );
		return inserted ? it->second.Get( ) : nullptr;
	}

	Microsoft::WRL::ComPtr< ID2D1Geometry > c_renderer::build_outline_geometry( IDWriteTextLayout* layout ) {
		Microsoft::WRL::ComPtr< ID2D1Geometry > result;
		if ( !layout || !m_device )
			return result;

		auto* factory = m_device->factory( );
		if ( !factory )
			return result;

		c_outline_geometry_builder builder( factory );
		if ( SUCCEEDED( layout->Draw( nullptr, &builder, 0.0f, 0.0f ) ) )
			result = builder.finish( );
		return result;
	}

	c_renderer::cached_text_layout_t* c_renderer::text_layout( text_layout_key_t key ) {
		const auto found = m_text_layouts.find( key );
		if ( found != m_text_layouts.end( ) ) {
			found->second.last_used = ++m_text_cache_clock;
			return &found->second;
		}

		auto* format = text_format( key.format );
		if ( !format )
			return nullptr;

		cached_text_layout_t cached;
		if ( FAILED( m_device->text_factory( )->CreateTextLayout(
			key.value.data( ), static_cast< UINT32 >( key.value.size( ) ), format,
			key.width, key.height, cached.layout.GetAddressOf( )
		) ) )
			return nullptr;

		// Outline geometry INTENTIONALLY skipped on the cache miss path.
		// build_outline_geometry runs IDWriteFontFace::GetGlyphRunOutline
		// over every shaped glyph (full vector tessellation) plus a stack
		// of CreatePathGeometry / CreateTransformedGeometry / CreateGeometry-
		// Group COM allocs per unique string — 1-5 ms per unique text at
		// steady state. Under text-heavy load with dynamic HUD strings
		// (HP:75, Distance:42m, player names, coords) every frame is a
		// cache miss and the miss cost stacks to 100-500 ms per frame,
		// producing the observed 1-4 s render-thread freezes under
		// packet stress. text() now stamps the outline as a cheap 4-pass
		// DrawTextLayout (uses DWrite's glyph atlas — the stamped glyphs
		// are all atlas hits after the first) which is orders of magnitude
		// cheaper and visually equivalent for the overlay's stroked-HUD
		// text. cached.outline_geometry stays default-initialized (null).
		cached.last_used = ++m_text_cache_clock;

		// Evict the oldest 25% of entries in one shot (nth_element is O(n)) rather
		// than calling min_element (also O(n)) on every single insert. This amortises
		// the scan cost across k_evict_count inserts instead of paying it every time.
		constexpr std::size_t k_max_layouts  = 2048;
		constexpr std::size_t k_evict_count  = 512;
		if ( m_text_layouts.size( ) >= k_max_layouts ) {
			using map_t = decltype( m_text_layouts );
			std::vector< std::pair< std::uint64_t, map_t::iterator > > order;
			order.reserve( m_text_layouts.size( ) );
			for ( auto it = m_text_layouts.begin( ); it != m_text_layouts.end( ); ++it )
				order.emplace_back( it->second.last_used, it );
			std::nth_element( order.begin( ), order.begin( ) + k_evict_count, order.end( ),
				[]( const auto& a, const auto& b ) { return a.first < b.first; } );
			for ( std::size_t i = 0; i < k_evict_count; ++i )
				m_text_layouts.erase( order[ i ].second );
		}

		auto [ it, inserted ] = m_text_layouts.emplace( std::move( key ), std::move( cached ) );
		return inserted ? &it->second : nullptr;
	}

	void c_renderer::clear_text_cache( ) {
		m_text_layouts.clear( );
		m_text_formats.clear( );
		m_text_cache_clock = 0;
	}

	void c_renderer::text( std::wstring_view value, rect_t area, float font_size, color_t color, text_align_t alignment, std::wstring_view font_family, DWRITE_FONT_WEIGHT font_weight, bool wrap ) {
		if ( !m_device || value.empty( ) || font_size <= 0.0f )
			return;

		auto* ctx = m_device->context( );
		text_layout_key_t key{
			std::wstring( value ),
			text_format_key_t{ std::wstring( font_family ), font_size, font_weight, alignment, wrap },
			area.width,
			area.height
		};
		auto* cached = text_layout( std::move( key ) );
		if ( !cached )
			return;

		auto* fill_brush = brush( color );
		if ( !fill_brush )
			return;

		const color_t outline_color{ 0.0f, 0.0f, 0.0f, color.a };
		auto* stroke_brush = outline_brush( outline_color );
		auto* const layout = cached->layout.Get( );
		constexpr auto opt = D2D1_DRAW_TEXT_OPTIONS_NONE;

		if ( !stroke_brush ) {
			ctx->DrawTextLayout( D2D1::Point2F( area.x, area.y ), layout, fill_brush, opt );
			return;
		}

		// 4-pass outline stamp at +/-1 px offsets, then the fill on top.
		// Replaces the old DrawGeometry/FillGeometry path that consumed
		// a per-unique-string tessellated outline built by
		// IDWriteFontFace::GetGlyphRunOutline — the single dominant cost
		// in the render pipeline under dynamic-HUD load. All five draws
		// share the same IDWriteTextLayout so the glyph rasters are hit
		// once in DWrite's atlas and reused for each stamp; the extra
		// cost is 4 D2D DrawTextLayout submissions (a few us each), not
		// four full glyph pipelines. Visual result matches the prior
		// 1.6 px stroked+filled geometry to within a pixel.
		ctx->DrawTextLayout( D2D1::Point2F( area.x - 1.0f, area.y        ), layout, stroke_brush, opt );
		ctx->DrawTextLayout( D2D1::Point2F( area.x + 1.0f, area.y        ), layout, stroke_brush, opt );
		ctx->DrawTextLayout( D2D1::Point2F( area.x,        area.y - 1.0f ), layout, stroke_brush, opt );
		ctx->DrawTextLayout( D2D1::Point2F( area.x,        area.y + 1.0f ), layout, stroke_brush, opt );
		ctx->DrawTextLayout( D2D1::Point2F( area.x,        area.y        ), layout, fill_brush,   opt );
	}

	size_t c_renderer::measure_text( std::wstring_view value, float font_size, std::wstring_view font_family, DWRITE_FONT_WEIGHT font_weight, float maximum_width ) {
		if ( !m_device || value.empty( ) || font_size <= 0.0f )
			return { };

		// Route through the layout cache — same text+format at the same maximum_width
		// produces identical metrics so reusing the cached layout is correct.
		text_layout_key_t key{
			std::wstring( value ),
			text_format_key_t{ std::wstring( font_family ), font_size, font_weight, text_align_t::left, true },
			maximum_width,
			16384.0f
		};
		auto* cached = text_layout( std::move( key ) );
		if ( !cached )
			return { };

		DWRITE_TEXT_METRICS metrics{ };
		if ( FAILED( cached->layout->GetMetrics( &metrics ) ) )
			return { };

		return { metrics.widthIncludingTrailingWhitespace, metrics.height };
	}

	void c_renderer::stipple_grain( float x, float y, float width, float height, float strength ) {
		if ( !m_device || strength <= 0.0f || width <= 0.0f || height <= 0.0f )
			return;

		auto* ctx = m_device->context( );

		Microsoft::WRL::ComPtr< ID2D1Effect > noise;
		if ( FAILED( ctx->CreateEffect( CLSID_D2D1Turbulence, noise.GetAddressOf( ) ) ) )
			return;
		noise->SetValue( D2D1_TURBULENCE_PROP_BASE_FREQUENCY, D2D1_VECTOR_2F{ 0.22f, 0.22f } );
		noise->SetValue( D2D1_TURBULENCE_PROP_NUM_OCTAVES, static_cast< UINT32 >( 2 ) );
		noise->SetValue( D2D1_TURBULENCE_PROP_SEED, static_cast< INT32 >( 1337 ) );
		noise->SetValue( D2D1_TURBULENCE_PROP_NOISE, D2D1_TURBULENCE_NOISE_TURBULENCE );
		noise->SetValue( D2D1_TURBULENCE_PROP_SIZE, D2D1_VECTOR_2F{ width, height } );

		// Turbulence output is infinite -- bound it to the region we will draw.
		Microsoft::WRL::ComPtr< ID2D1Effect > crop;
		if ( FAILED( ctx->CreateEffect( CLSID_D2D1Crop, crop.GetAddressOf( ) ) ) )
			return;
		crop->SetInputEffect( 0, noise.Get( ) );
		crop->SetValue( D2D1_CROP_PROP_RECT, D2D1_VECTOR_4F{ 0.0f, 0.0f, width, height } );

		// Collapse the RGB noise to a single luminance channel and, crucially,
		// discard the noisy input alpha (alpha column zeroed) in favor of a small
		// constant output alpha. Straight alpha mode so the color is not scaled
		// by that alpha until it composites over the target.
		Microsoft::WRL::ComPtr< ID2D1Effect > grade;
		if ( FAILED( ctx->CreateEffect( CLSID_D2D1ColorMatrix, grade.GetAddressOf( ) ) ) )
			return;
		grade->SetInputEffect( 0, crop.Get( ) );
		const D2D1_MATRIX_5X4_F matrix = {
			0.33f, 0.33f, 0.33f, 0.0f,
			0.33f, 0.33f, 0.33f, 0.0f,
			0.34f, 0.34f, 0.34f, 0.0f,
			0.00f, 0.00f, 0.00f, 0.0f,
			0.00f, 0.00f, 0.00f, strength
		};
		grade->SetValue( D2D1_COLORMATRIX_PROP_COLOR_MATRIX, matrix );
		grade->SetValue( D2D1_COLORMATRIX_PROP_ALPHA_MODE, D2D1_COLORMATRIX_ALPHA_MODE_STRAIGHT );

		ctx->DrawImage( grade.Get( ), D2D1::Point2F( x, y ) );
	}

	image_t c_renderer::bake_blur( const image_t& source, float sigma ) {
		image_t result;
		if ( !m_device || !source || sigma <= 0.0f )
			return result;

		auto* ctx = m_device->context( );
		const auto size = source->GetPixelSize( );
		if ( !size.width || !size.height )
			return result;

		// TARGET so we can render into it, and drawable as an effect input.
		// Premultiplied BGRA matches the layered-window compositing path.
		const auto props = D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET,
			D2D1::PixelFormat( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED )
		);
		if ( FAILED( ctx->CreateBitmap( size, nullptr, 0, &props, result.GetAddressOf( ) ) ) )
			return { };

		Microsoft::WRL::ComPtr< ID2D1Effect > blur;
		if ( FAILED( ctx->CreateEffect( CLSID_D2D1GaussianBlur, blur.GetAddressOf( ) ) ) )
			return { };
		blur->SetInput( 0, source.Get( ) );
		blur->SetValue( D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, sigma );
		blur->SetValue( D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD );
		blur->SetValue( D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, D2D1_GAUSSIANBLUR_OPTIMIZATION_QUALITY );

		// High-precision effect intermediates for a band-free blur.
		D2D1_RENDERING_CONTROLS previous_controls{ };
		ctx->GetRenderingControls( &previous_controls );
		D2D1_RENDERING_CONTROLS hq_controls = previous_controls;
		hq_controls.bufferPrecision = D2D1_BUFFER_PRECISION_16BPC_FLOAT;
		ctx->SetRenderingControls( hq_controls );

		// Redirect drawing to the cache WITHOUT a nested BeginDraw -- we are
		// already inside the frame's BeginDraw/EndDraw. Save the real target,
		// draw the effect, restore it exactly (SetTarget(nullptr) is not a
		// restore -- it leaves the context targetless).
		Microsoft::WRL::ComPtr< ID2D1Image > previous_target;
		ctx->GetTarget( previous_target.GetAddressOf( ) );
		ctx->SetTarget( result.Get( ) );
		ctx->Clear( D2D1::ColorF( 0.0f, 0.0f, 0.0f, 0.0f ) );
		ctx->DrawImage( blur.Get( ) );
		ctx->SetTarget( previous_target.Get( ) );

		ctx->SetRenderingControls( previous_controls );
		return result;
	}

	image_t c_renderer::bake_soft_shadow( float width, float height, float radius, float sigma, color_t color ) {
		image_t result;
		if ( !m_device || width <= 0.0f || height <= 0.0f || sigma <= 0.0f )
			return result;

		auto* ctx = m_device->context( );

		// Pad every side by ~3 sigma so the soft halo isn't clipped.
		const UINT32 pad = static_cast< UINT32 >( std::ceil( sigma * 3.0f ) );
		const UINT32 out_w = static_cast< UINT32 >( std::ceil( width ) ) + pad * 2;
		const UINT32 out_h = static_cast< UINT32 >( std::ceil( height ) ) + pad * 2;

		const auto props = D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET,
			D2D1::PixelFormat( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED )
		);

		image_t silhouette;
		if ( FAILED( ctx->CreateBitmap( D2D1::SizeU( out_w, out_h ), nullptr, 0, &props, silhouette.GetAddressOf( ) ) ) ||
			FAILED( ctx->CreateBitmap( D2D1::SizeU( out_w, out_h ), nullptr, 0, &props, result.GetAddressOf( ) ) ) )
			return { };

		D2D1_RENDERING_CONTROLS previous_controls{ };
		ctx->GetRenderingControls( &previous_controls );
		D2D1_RENDERING_CONTROLS hq_controls = previous_controls;
		hq_controls.bufferPrecision = D2D1_BUFFER_PRECISION_16BPC_FLOAT;
		ctx->SetRenderingControls( hq_controls );

		Microsoft::WRL::ComPtr< ID2D1Image > previous_target;
		ctx->GetTarget( previous_target.GetAddressOf( ) );

		// 1) Draw the sharp silhouette centered in the padded bitmap.
		ctx->SetTarget( silhouette.Get( ) );
		ctx->Clear( D2D1::ColorF( 0.0f, 0.0f, 0.0f, 0.0f ) );
		Microsoft::WRL::ComPtr< ID2D1SolidColorBrush > fill;
		if ( SUCCEEDED( ctx->CreateSolidColorBrush( D2D1::ColorF( color.r, color.g, color.b, color.a ), fill.GetAddressOf( ) ) ) ) {
			const auto shape = D2D1::RoundedRect(
				D2D1::RectF( static_cast< float >( pad ), static_cast< float >( pad ),
					static_cast< float >( pad ) + width, static_cast< float >( pad ) + height ),
				radius, radius );
			ctx->FillRoundedRectangle( shape, fill.Get( ) );
		}

		// 2) Blur the silhouette (SOFT border so the halo is real transparency,
		//    not a dark ring) into the result.
		Microsoft::WRL::ComPtr< ID2D1Effect > blur;
		if ( SUCCEEDED( ctx->CreateEffect( CLSID_D2D1GaussianBlur, blur.GetAddressOf( ) ) ) ) {
			blur->SetInput( 0, silhouette.Get( ) );
			blur->SetValue( D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, sigma );
			blur->SetValue( D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_SOFT );
			blur->SetValue( D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, D2D1_GAUSSIANBLUR_OPTIMIZATION_QUALITY );

			ctx->SetTarget( result.Get( ) );
			ctx->Clear( D2D1::ColorF( 0.0f, 0.0f, 0.0f, 0.0f ) );
			ctx->DrawImage( blur.Get( ) );
		}

		ctx->SetTarget( previous_target.Get( ) );
		ctx->SetRenderingControls( previous_controls );
		return result;
	}

	image_t c_renderer::generate_aurora( int width, int height, const std::vector< aurora_blob_t >& blobs, float sigma, float grain_strength ) {
		image_t cache;
		if ( !m_device || width <= 0 || height <= 0 || blobs.empty( ) )
			return cache;

		auto* ctx = m_device->context( );
		const auto size = D2D1::SizeU( static_cast< UINT32 >( width ), static_cast< UINT32 >( height ) );

		const auto float_props = D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET,
			D2D1::PixelFormat( DXGI_FORMAT_R16G16B16A16_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED )
		);
		const auto cache_props = D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET,
			D2D1::PixelFormat( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED )
		);

		image_t blobs_bmp;
		if ( FAILED( ctx->CreateBitmap( size, nullptr, 0, &float_props, blobs_bmp.GetAddressOf( ) ) ) ||
			FAILED( ctx->CreateBitmap( size, nullptr, 0, &cache_props, cache.GetAddressOf( ) ) ) )
			return { };

		D2D1_RENDERING_CONTROLS previous_controls{ };
		ctx->GetRenderingControls( &previous_controls );
		D2D1_RENDERING_CONTROLS hq_controls = previous_controls;
		hq_controls.bufferPrecision = D2D1_BUFFER_PRECISION_16BPC_FLOAT;
		ctx->SetRenderingControls( hq_controls );

		Microsoft::WRL::ComPtr< ID2D1Image > previous_target;
		ctx->GetTarget( previous_target.GetAddressOf( ) );

		ctx->SetTarget( blobs_bmp.Get( ) );
		ctx->Clear( D2D1::ColorF( 0.0f, 0.0f, 0.0f, 0.0f ) );
		for ( const auto& blob : blobs ) {
			if ( blob.radius <= 0.0f )
				continue;

			const D2D1_GRADIENT_STOP stops[ 2 ] = {
				{ 0.0f, D2D1::ColorF( blob.color.r, blob.color.g, blob.color.b, blob.color.a ) },
				{ 1.0f, D2D1::ColorF( blob.color.r, blob.color.g, blob.color.b, 0.0f ) }
			};
			Microsoft::WRL::ComPtr< ID2D1GradientStopCollection > collection;
			if ( FAILED( ctx->CreateGradientStopCollection( stops, 2, D2D1_GAMMA_1_0, D2D1_EXTEND_MODE_CLAMP, collection.GetAddressOf( ) ) ) )
				continue;

			Microsoft::WRL::ComPtr< ID2D1RadialGradientBrush > brush;
			if ( FAILED( ctx->CreateRadialGradientBrush(
				D2D1::RadialGradientBrushProperties(
					D2D1::Point2F( blob.center.x, blob.center.y ),
					D2D1::Point2F( 0.0f, 0.0f ),
					blob.radius, blob.radius ),
				collection.Get( ), brush.GetAddressOf( ) ) ) )
				continue;

			ctx->FillEllipse(
				D2D1::Ellipse( D2D1::Point2F( blob.center.x, blob.center.y ), blob.radius, blob.radius ),
				brush.Get( ) );
		}

		Microsoft::WRL::ComPtr< ID2D1Effect > blur;
		if ( FAILED( ctx->CreateEffect( CLSID_D2D1GaussianBlur, blur.GetAddressOf( ) ) ) ) {
			ctx->SetTarget( previous_target.Get( ) );
			ctx->SetRenderingControls( previous_controls );
			return { };
		}

		blur->SetInput( 0, blobs_bmp.Get( ) );
		blur->SetValue( D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, sigma );
		blur->SetValue( D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD );
		blur->SetValue( D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, D2D1_GAUSSIANBLUR_OPTIMIZATION_QUALITY );

		ctx->SetTarget( cache.Get( ) );
		ctx->Clear( D2D1::ColorF( 0.0f, 0.0f, 0.0f, 0.0f ) );
		ctx->DrawImage( blur.Get( ) );

		if ( grain_strength > 0.0f )
			stipple_grain( 0.0f, 0.0f, static_cast< float >( width ), static_cast< float >( height ), grain_strength );

		ctx->SetTarget( previous_target.Get( ) );
		ctx->SetRenderingControls( previous_controls );
		return cache;
	}

	void c_renderer::draw_frosted_panel( rect_t area, float radius, float sigma, color_t tint ) {
		if ( !m_device || area.width <= 0.0f || area.height <= 0.0f || sigma <= 0.0f )
			return;

		auto* ctx = m_device->context( );
		const int target_w = m_device->width( );
		const int target_h = m_device->height( );

		// Integer region of the current frame under the panel, clamped to the
		// target (CopyFromRenderTarget takes integer, in-bounds coordinates).
		int left = ( std::max )( 0, static_cast< int >( std::floor( area.x ) ) );
		int top = ( std::max )( 0, static_cast< int >( std::floor( area.y ) ) );
		int right = ( std::min )( target_w, static_cast< int >( std::ceil( area.x + area.width ) ) );
		int bottom = ( std::min )( target_h, static_cast< int >( std::ceil( area.y + area.height ) ) );
		if ( right <= left || bottom <= top )
			return;

		const UINT32 region_w = static_cast< UINT32 >( right - left );
		const UINT32 region_h = static_cast< UINT32 >( bottom - top );

		// Snapshot what the overlay has already drawn under the panel this frame.
		const auto snap_props = D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_NONE,
			D2D1::PixelFormat( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED )
		);
		image_t snapshot;
		if ( FAILED( ctx->CreateBitmap( D2D1::SizeU( region_w, region_h ), nullptr, 0, &snap_props, snapshot.GetAddressOf( ) ) ) )
			return;

		const auto destination = D2D1::Point2U( 0, 0 );
		const auto source = D2D1::RectU( static_cast< UINT32 >( left ), static_cast< UINT32 >( top ),
			static_cast< UINT32 >( left ) + region_w, static_cast< UINT32 >( top ) + region_h );
		if ( FAILED( snapshot->CopyFromRenderTarget( &destination, ctx, &source ) ) )
			return;

		Microsoft::WRL::ComPtr< ID2D1Effect > blur;
		if ( FAILED( ctx->CreateEffect( CLSID_D2D1GaussianBlur, blur.GetAddressOf( ) ) ) )
			return;
		blur->SetInput( 0, snapshot.Get( ) );
		blur->SetValue( D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, sigma );
		blur->SetValue( D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD );
		blur->SetValue( D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, D2D1_GAUSSIANBLUR_OPTIMIZATION_QUALITY );

		D2D1_RENDERING_CONTROLS previous_controls{ };
		ctx->GetRenderingControls( &previous_controls );
		D2D1_RENDERING_CONTROLS hq_controls = previous_controls;
		hq_controls.bufferPrecision = D2D1_BUFFER_PRECISION_16BPC_FLOAT;
		ctx->SetRenderingControls( hq_controls );

		if ( push_rounded_clip( area, radius ) ) {
			// The blurred snapshot maps back to the exact pixels it came from.
			ctx->DrawImage( blur.Get( ), D2D1::Point2F( static_cast< float >( left ), static_cast< float >( top ) ) );
			stipple_grain( static_cast< float >( left ), static_cast< float >( top ),
				static_cast< float >( region_w ), static_cast< float >( region_h ), 0.05f );
			filled_rounded_rectangle( area, radius, tint );
			pop_clip( );
		}

		ctx->SetRenderingControls( previous_controls );

		// Crisp light edge on top of the frosted fill sells the glass surface.
		rounded_rectangle( area, radius, color_t{ 1.0f, 1.0f, 1.0f, 0.14f }, 1.0f );
	}
}
