#pragma once

#include "device.hxx"
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace render {
	using image_t = Microsoft::WRL::ComPtr< ID2D1Bitmap1 >;

	class c_renderer {
	public:
		c_renderer( ) = default;
		~c_renderer( );

		c_renderer( const c_renderer& ) = delete;
		c_renderer& operator=( const c_renderer& ) = delete;

		bool initialize( c_device& device );
		void shutdown( );
		bool begin_frame( color_t clear );
		bool end_frame( );
		int width( ) const;
		int height( ) const;

		void line( point_t from, point_t to, color_t color, float thickness = 1.0f );
		void skeleton( const communication::skeleton_request_t& request, color_t color, color_t outline_color );
		void rectangle( rect_t area, color_t color, float thickness = 1.0f );
		void filled_rectangle( rect_t area, color_t color );
		void rounded_rectangle( rect_t area, float radius, color_t color, float thickness = 1.0f );
		void filled_rounded_rectangle( rect_t area, float radius, color_t color );
		// Two-stop linear gradient fills. `color_a` maps to the start edge
		// (top / left), `color_b` maps to the end edge (bottom / right).
		// corner_radius > 0 clips to a rounded rectangle, identical to
		// the filled_rounded_rectangle convention.
		void filled_gradient_rectangle  ( rect_t area, color_t color_a, color_t color_b, float corner_radius = 0.0f );
		void filled_gradient_rectangle_h( rect_t area, color_t color_a, color_t color_b, float corner_radius = 0.0f );
		void circle( point_t center, float radius, color_t color, float thickness = 1.0f );
		void arc( point_t center, float radius, float start_angle, float sweep_angle, color_t color, float thickness = 1.0f );
		void filled_circle( point_t center, float radius, color_t color );
		image_t load_image( std::wstring_view path );
		// Plain draw — DrawBitmap with the bitmap's own RGB and alpha,
		// modulated by `opacity`. Callers see the original PNG colours.
		void draw_image( const image_t& image, rect_t area, float opacity = 1.0f );
		// Tinted draw — FillOpacityMask paints the `tint` colour through
		// the bitmap's alpha channel. RGB of the source bitmap is IGNORED
		// entirely, so the output is a solid-tint silhouette with zero
		// original-image colour bleed. `opacity` multiplies the tint's
		// own alpha so the shape can fade in/out (e.g. sidebar reveal
		// animation) while staying a solid colour.
		void draw_image( const image_t& image, rect_t area, color_t tint, float opacity = 1.0f );
		// Blit a bitmap this renderer does NOT own (e.g. the scene3d
		// off-screen render target). Same as draw_image(opacity) but
		// takes a raw pointer so callers don't have to hand out a
		// ComPtr just for a one-shot draw.
		void draw_raw_bitmap( ID2D1Bitmap1* bitmap, rect_t area, float opacity = 1.0f );
		void push_opacity( float opacity );
		void pop_opacity( );
		bool push_rounded_clip( rect_t area, float radius );
		void pop_clip( );
		bool push_axis_aligned_clip( rect_t area );
		void pop_axis_aligned_clip( );
		void interactive_area( rect_t area );
		void minimize_window( );
		void bring_to_top( );
		void text( std::wstring_view value, rect_t area, float font_size, color_t color, text_align_t alignment = text_align_t::left, std::wstring_view font_family = L"Segoe UI", DWRITE_FONT_WEIGHT font_weight = DWRITE_FONT_WEIGHT_BOLD, bool wrap = false );
		size_t measure_text( std::wstring_view value, float font_size, std::wstring_view font_family = L"Segoe UI", DWRITE_FONT_WEIGHT font_weight = DWRITE_FONT_WEIGHT_BOLD, float maximum_width = 16384.0f );

		// --- blur / texture helpers ------------------------------------------
		// A soft colored blob for generate_aurora(): a radial-gradient splash of
		// `color` fading to transparent over `radius`, centered at `center`.
		struct aurora_blob_t {
			point_t center;
			float radius = 0.0f;
			color_t color;
		};

		// Cache-once Gaussian blur of a static bitmap (a loaded asset or a
		// generated texture) into a new image_t drawn every frame via
		// draw_image(). Output is the same pixel size as `source`. Call it once
		// (guard on the returned handle) -- re-blurring identical content per
		// frame is wasted fill. sigma is the blur standard deviation in pixels
		// (visible radius ~= 3 * sigma).
		image_t bake_blur( const image_t& source, float sigma );

		// Cache-once soft drop-shadow / outer-glow sprite for a rounded rect.
		// The returned bitmap is padded by ceil( 3 * sigma ) on every side to
		// hold the halo, so draw it with draw_image() at
		// ( panel.x - pad, panel.y - pad ) plus any offset, BEHIND the panel.
		image_t bake_soft_shadow( float width, float height, float radius, float sigma, color_t color );

		// Generate a cached frosted aurora / mesh-gradient backdrop: soft colored
		// blobs melted together by a wide Gaussian blur, finished with fine grain
		// (which also dithers out 8-bit banding). Rendered through a 16-bit float
		// effect graph for band-free gradients. Bake once; regenerate only on
		// resize or palette change. grain_strength 0 disables the grain layer.
		image_t generate_aurora( int width, int height, const std::vector< aurora_blob_t >& blobs, float sigma, float grain_strength = 0.06f );

		// Frosted-glass panel over the overlay's OWN already-drawn content: grabs
		// the current frame under `area`, blurs it, and composites it under a
		// rounded clip with fine grain, a translucent `tint`, and a crisp light
		// edge. Runs live each frame -- call it after drawing the content that
		// should show through, before drawing the panel's own foreground.
		void draw_frosted_panel( rect_t area, float radius, float sigma, color_t tint );

	private:
		struct text_format_key_t {
			std::wstring family;
			float size = 0.0f;
			DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
			text_align_t alignment = text_align_t::left;
			bool wrap = false;

			bool operator==( const text_format_key_t& other ) const {
				return family == other.family && size == other.size && weight == other.weight &&
					alignment == other.alignment && wrap == other.wrap;
			}
		};

		struct text_format_key_hash_t {
			std::size_t operator()( const text_format_key_t& key ) const;
		};

		struct text_layout_key_t {
			std::wstring value;
			text_format_key_t format;
			float width = 0.0f;
			float height = 0.0f;

			bool operator==( const text_layout_key_t& other ) const {
				return value == other.value && format == other.format &&
					width == other.width && height == other.height;
			}
		};

		struct text_layout_key_hash_t {
			std::size_t operator()( const text_layout_key_t& key ) const;
		};

		struct cached_text_layout_t {
			Microsoft::WRL::ComPtr< IDWriteTextLayout > layout;
			Microsoft::WRL::ComPtr< ID2D1Geometry > outline_geometry;
			std::uint64_t last_used = 0;
		};

		// Overlays constant-alpha fractal-noise grain on the currently bound
		// target within [x, x+width] x [y, y+height]. The alpha is forced
		// constant on purpose: raw turbulence carries a noisy alpha channel,
		// which on this AC_SRC_ALPHA layered window would punch random
		// transparent pinholes straight through to the desktop.
		void stipple_grain( float x, float y, float width, float height, float strength );

		ID2D1SolidColorBrush* brush( color_t color );
		ID2D1SolidColorBrush* outline_brush( color_t color );
		D2D1_RECT_F native_rect( rect_t area ) const;
		IDWriteTextFormat* text_format( const text_format_key_t& key );
		cached_text_layout_t* text_layout( text_layout_key_t key );
		Microsoft::WRL::ComPtr< ID2D1Geometry > build_outline_geometry( IDWriteTextLayout* layout );
		void clear_text_cache( );

		c_device* m_device = nullptr;
		Microsoft::WRL::ComPtr< ID2D1SolidColorBrush > m_brush;
		Microsoft::WRL::ComPtr< ID2D1SolidColorBrush > m_outline_brush;
		Microsoft::WRL::ComPtr< ID2D1StrokeStyle >     m_round_cap_style;
		Microsoft::WRL::ComPtr< ID2D1Geometry > m_clip_geometry;
		std::unordered_map< text_format_key_t, Microsoft::WRL::ComPtr< IDWriteTextFormat >, text_format_key_hash_t > m_text_formats;
		std::unordered_map< text_layout_key_t, cached_text_layout_t, text_layout_key_hash_t > m_text_layouts;
		std::uint64_t m_text_cache_clock = 0;
		std::uint64_t m_font_revision = 0;
		bool m_clip_active = false;
		std::size_t m_opacity_layer_count = 0;
		// One entry per logical push request. Invalid clips are recorded as
		// false so their matching pop cannot remove an enclosing valid clip.
		std::vector< bool > m_aa_clip_stack;
	};
}
