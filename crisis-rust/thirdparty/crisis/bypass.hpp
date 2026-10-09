#pragma once
#include "../../../crisis-bypass/core/mapping/mapping.hxx"

namespace bypass {
	inline mapping::vtable_t g_api {};

	inline bool open_shared_data( int retries = 100, DWORD delay_ms = 100 ) {
		mapping::c_mapping m;
		return m.open_consumer( g_api, retries, delay_ms );
	}

	inline bool initialize( const wchar_t* process, const wchar_t* product ) { return g_api.m_initialize( process, product ); }
	inline bool create( ) { return g_api.m_create( ); }
	inline void cleanup( ) { if ( g_api.m_cleanup ) g_api.m_cleanup( ); }

	namespace target {
		inline std::uint32_t m_process_pid {};
		inline HWND          m_window_handle {};
		inline std::uint64_t m_eprocess {};
		inline std::uint64_t m_process_peb {};
		inline std::uint64_t m_base_address {};
		inline std::uint64_t m_directory_table_base {};

		inline bool setup( ) {
			if ( !g_api.m_setup_target( ) ) return false;
			m_process_pid = g_api.m_get_process_id( );
			m_window_handle = static_cast< HWND >( g_api.m_get_window_handle( ) );
			m_base_address = g_api.m_get_base_address( );
			m_directory_table_base = g_api.m_get_directory_table_base( );
			m_process_peb = g_api.m_get_process_peb( );
			m_eprocess = g_api.m_get_eprocess( );
			return true;
		}

		inline std::uint64_t translate_linear( std::uint64_t virt_addr,
			std::uint32_t* page_size = nullptr ) {
			return g_api.m_translate_linear( virt_addr, page_size );
		}

		inline bool read_memory( std::uint64_t va, void* buf, std::size_t sz ) {
			return g_api.m_read_memory( va, buf, sz );
		}
		inline bool write_memory( std::uint64_t va, void* buf, std::size_t sz ) {
			return g_api.m_write_memory( va, buf, sz );
		}

		inline std::uintptr_t get_module_base( const wchar_t* module_name ) {
			return g_api.m_get_module_base( module_name );
		}
		inline std::uintptr_t get_module_export( std::uintptr_t module_base,
			const char* export_name ) {
			return g_api.m_get_module_export( module_base, export_name );
		}
		inline std::uint64_t  pattern_scan( std::uint64_t module_base,
			const char* pattern,
			int offset, bool relative ) {
			return g_api.m_pattern_scan( module_base, pattern, offset, relative );
		}
		inline void benchmark_rps( ) {
			if ( g_api.m_benchmark_rps ) g_api.m_benchmark_rps( );
		}

		inline constexpr std::uint64_t stack_param_base = 0x0000800000000000ull;

		template< std::size_t N >
		struct stack_wstring_view_t {
			const wchar_t* ptr;
			std::size_t    length;
			wchar_t        chars[ N ];

			explicit stack_wstring_view_t( const wchar_t( &str )[ N ] )
				: ptr( reinterpret_cast< const wchar_t* >(
					stack_param_base + offsetof( stack_wstring_view_t, chars ) ) )
				, length( N - 1 ) {
				std::memcpy( chars, str, N * sizeof( wchar_t ) );
			}
		};

		template< std::size_t N >
		stack_wstring_view_t( const wchar_t( & )[ N ] ) -> stack_wstring_view_t< N >;

		struct allocated_region_t {
			std::uint64_t m_base_address;
			std::uint64_t m_mapped_address;
			std::size_t   m_size;
		};

		inline std::optional<allocated_region_t> allocate_virtual( std::size_t ) {
			return std::nullopt;
		}
		inline bool free_virtual( const std::optional<allocated_region_t>& ) {
			return false;
		}
	}

	inline std::atomic<std::uint64_t> g_ops_counter { 0 };

	template <typename ret_t = std::uint64_t, typename addr_t>
	ret_t read( addr_t va ) {
		std::uint64_t va64;
		if constexpr ( std::is_pointer_v<addr_t> )
			va64 = reinterpret_cast< std::uint64_t >( va );
		else
			va64 = static_cast< std::uint64_t >( va );
		ret_t ret {};
		target::read_memory( va64, &ret, sizeof( ret ) );
		g_ops_counter.fetch_add( 1, std::memory_order_relaxed );
		return ret;
	}

	template <typename val_t, typename addr_t>
	bool write( addr_t va, val_t val ) {
		std::uint64_t va64;
		if constexpr ( std::is_pointer_v<addr_t> )
			va64 = reinterpret_cast< std::uint64_t >( va );
		else
			va64 = static_cast< std::uint64_t >( va );
		const bool ok = target::write_memory( va64, &val, sizeof( val_t ) );
		g_ops_counter.fetch_add( 1, std::memory_order_relaxed );
		return ok;
	}

	namespace mouse {
		inline bool move( double x, double y, bool use_virtual_desktop = false ) {
			return g_api.m_mouse_move( x, y, use_virtual_desktop );
		}
		inline bool click( ) { return g_api.m_mouse_click( ); }
		inline bool click_at( double x, double y, bool use_virtual_desktop = false ) {
			return g_api.m_mouse_click_at( x, y, use_virtual_desktop );
		}
	}

	namespace overlay {
		enum class text_align_t : std::uint32_t { left, center, right };

		enum class text_weight_t : std::uint32_t {
			thin = 100, light = 300, regular = 400, medium = 500,
			semi_bold = 600, bold = 700, black = 900
		};

		struct color_t { std::uint8_t r {}, g {}, b {}, a { 255 }; };
		struct point_t { float x {}, y {}; };
		struct rect_t { float x {}, y {}, width {}, height {}; };

		struct skeleton_segment_t { point_t from {}, to {}; };

		struct monitor_info_t {
			std::int32_t left {}, top {}, right {}, bottom {};
			bool         is_primary {};
		};

		struct aurora_blob_t {
			float   x {}, y {}, radius {};
			color_t color {};
		};

		struct gradient_rectangle_t {
			rect_t  area {};
			color_t color_a {}, color_b {};
			float   corner_radius { 0.0f };
		};

		inline bool clear( ) { return g_api.m_overlay_clear( ); }
		inline bool present( ) { return g_api.m_overlay_present( ); }
		inline bool minimize( ) { return g_api.m_overlay_minimize( ); }
		inline bool bring_to_top( ) { return g_api.m_overlay_bring_to_top( ); }

		inline bool line( point_t from, point_t to, color_t c, float thickness ) {
			return g_api.m_overlay_line(
				from.x, from.y, to.x, to.y,
				c.r, c.g, c.b, c.a, thickness );
		}
		inline bool line_async( point_t from, point_t to, color_t c, float thickness ) {
			return g_api.m_overlay_line_async(
				from.x, from.y, to.x, to.y,
				c.r, c.g, c.b, c.a, thickness );
		}
		inline bool rectangle( rect_t area, color_t c, float thickness ) {
			return g_api.m_overlay_rectangle(
				area.x, area.y, area.width, area.height,
				c.r, c.g, c.b, c.a, thickness );
		}
		inline bool filled_rectangle( rect_t area, color_t c ) {
			return g_api.m_overlay_filled_rectangle(
				area.x, area.y, area.width, area.height,
				c.r, c.g, c.b, c.a );
		}
		inline bool filled_rectangle_async( rect_t area, color_t c ) {
			return g_api.m_overlay_filled_rectangle_async(
				area.x, area.y, area.width, area.height,
				c.r, c.g, c.b, c.a );
		}
		inline bool circle( point_t center, float radius, color_t c, float thickness ) {
			return g_api.m_overlay_circle(
				center.x, center.y, radius,
				c.r, c.g, c.b, c.a, thickness );
		}
		inline bool filled_circle( point_t center, float radius, color_t c ) {
			return g_api.m_overlay_filled_circle(
				center.x, center.y, radius,
				c.r, c.g, c.b, c.a );
		}
		inline bool text( const wchar_t* value, rect_t area, color_t c, float size,
			text_weight_t weight, const wchar_t* font,
			text_align_t alignment, bool wrap ) {
			return g_api.m_overlay_text(
				value,
				area.x, area.y, area.width, area.height,
				c.r, c.g, c.b, c.a,
				size,
				static_cast< std::uint32_t >( weight ),
				font,
				static_cast< std::uint32_t >( alignment ),
				wrap );
		}
		inline bool text_async( const wchar_t* value, rect_t area, color_t c, float size,
			text_weight_t weight, const wchar_t* font,
			text_align_t alignment, bool wrap = false ) {
			return g_api.m_overlay_text_async(
				value,
				area.x, area.y, area.width, area.height,
				c.r, c.g, c.b, c.a,
				size,
				static_cast< std::uint32_t >( weight ),
				font,
				static_cast< std::uint32_t >( alignment ),
				wrap );
		}
		inline bool text_ex( const wchar_t* value, rect_t area, color_t c, float size,
			text_weight_t weight, const wchar_t* font,
			text_align_t alignment, bool wrap = false ) {
			return text( value, area, c, size, weight, font, alignment, wrap );
		}
		inline bool interactive_area( rect_t area ) {
			return g_api.m_overlay_interactive_area(
				area.x, area.y, area.width, area.height );
		}
		inline bool register_font( const wchar_t* name, const wchar_t* path ) {
			return g_api.m_overlay_register_font( name, path );
		}
		
		
		
		
		
		
		inline bool register_image( std::uint32_t id, const wchar_t* path ) {
			return g_api.m_overlay_register_image( id, path );
		}
		inline bool image( std::uint32_t id, rect_t area,
			color_t tint = { 255, 255, 255, 255 },
			float opacity = 1.f, float corner_radius = 0.f ) {
			return g_api.m_overlay_image(
				id, area.x, area.y, area.width, area.height,
				tint.r, tint.g, tint.b, tint.a,
				opacity, corner_radius );
		}
		inline bool rounded_rectangle( rect_t area, float radius, color_t c,
			float thickness = 1.0f ) {
			return g_api.m_overlay_rounded_rectangle(
				area.x, area.y, area.width, area.height,
				radius, c.r, c.g, c.b, c.a, thickness );
		}
		inline bool filled_rounded_rectangle( rect_t area, float radius, color_t c ) {
			return g_api.m_overlay_filled_rounded_rectangle(
				area.x, area.y, area.width, area.height,
				radius, c.r, c.g, c.b, c.a );
		}
		inline bool aurora( rect_t area, float corner_radius, float sigma,
			float grain_strength,
			const aurora_blob_t* blobs, std::uint32_t blob_count ) {
			return g_api.m_overlay_aurora(
				area.x, area.y, area.width, area.height,
				corner_radius, sigma, grain_strength,
				reinterpret_cast< const float* >( blobs ), blob_count );
		}
		inline bool frosted_panel( rect_t area, float corner_radius, float sigma,
			color_t tint ) {
			return g_api.m_overlay_frosted_panel(
				area.x, area.y, area.width, area.height,
				corner_radius, sigma,
				tint.r, tint.g, tint.b, tint.a );
		}
		inline bool gradient_rectangle( rect_t area, color_t color_a, color_t color_b,
			float corner_radius = 0.0f ) {
			return g_api.m_overlay_gradient_rectangle(
				area.x, area.y, area.width, area.height,
				color_a.r, color_a.g, color_a.b, color_a.a,
				color_b.r, color_b.g, color_b.b, color_b.a,
				corner_radius );
		}
		inline bool gradient_rectangle_h( rect_t area, color_t color_a, color_t color_b,
			float corner_radius = 0.0f ) {
			return g_api.m_overlay_gradient_rectangle_h(
				area.x, area.y, area.width, area.height,
				color_a.r, color_a.g, color_a.b, color_a.a,
				color_b.r, color_b.g, color_b.b, color_b.a,
				corner_radius );
		}
		inline bool push_clip( rect_t area ) {
			return g_api.m_overlay_push_clip(
				area.x, area.y, area.width, area.height );
		}
		inline bool pop_clip( ) { return g_api.m_overlay_pop_clip( ); }

		inline bool skeleton_async( const skeleton_segment_t* segments, std::uint32_t count,
			color_t color, float thickness = 1.5f, bool outline = true ) {
			return g_api.m_overlay_skeleton(
				reinterpret_cast< const float* >( segments ), count,
				color.r, color.g, color.b, color.a,
				thickness, outline );
		}

		inline bool set_monitor( std::uint32_t index ) {
			return g_api.m_overlay_set_monitor( index );
		}
		inline bool get_size( std::int32_t& width, std::int32_t& height ) {
			return g_api.m_overlay_get_size( &width, &height );
		}

		inline std::vector<monitor_info_t> enumerate_monitors( ) {
			std::int32_t buf[ 16 * 5 ] {};
			const auto count = g_api.m_overlay_enumerate_monitors( buf, 16 );
			std::vector<monitor_info_t> result;
			result.reserve( count );
			for ( std::uint32_t i = 0; i < count; ++i ) {
				auto& m = result.emplace_back( );
				m.left = buf[ i * 5 + 0 ];
				m.top = buf[ i * 5 + 1 ];
				m.right = buf[ i * 5 + 2 ];
				m.bottom = buf[ i * 5 + 3 ];
				m.is_primary = buf[ i * 5 + 4 ] != 0;
			}
			return result;
		}

		inline bool push_opacity_layer( rect_t, float ) { return false; }
		inline void pop_layer( ) { }

		constexpr color_t from_hex( std::uint32_t rgb, float a = 1.0f ) {
			return {
				static_cast< std::uint8_t >( ( ( rgb >> 16 ) & 0xff ) / 255.0f ),
				static_cast< std::uint8_t >( ( ( rgb >> 8 ) & 0xff ) / 255.0f ),
				static_cast< std::uint8_t >( ( ( rgb >> 0 ) & 0xff ) / 255.0f ),
				static_cast< std::uint8_t >( a )
			};
		}
	}
}
