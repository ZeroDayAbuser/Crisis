#pragma once

namespace bypass {
	bool create( );
	void cleanup( );

	class c_config {
	public:
		c_config( std::wstring process_name, std::wstring product_name ): m_process_name( process_name ), m_product_name( product_name ) { }

		void enable_spoofer( ) { this->m_spoofer_module = true; }
		void enable_overlay( ) { this->m_overlay_module = true; }
		void enable_mouse( ) { this->m_mouse_module = true; }

		std::wstring get_product_name( ) { return m_product_name; }
		std::wstring get_process_name( ) { return m_process_name; }
		bool is_spoofer_enabled( ) const { return m_spoofer_module; }
		bool is_overlay_enabled( ) const { return m_overlay_module; }
		bool is_mouse_enabled( ) const { return m_mouse_module; }

	private:
		std::wstring m_product_name { };
		std::wstring m_process_name { };
		bool m_overlay_module { };
		bool m_mouse_module { };
		bool m_spoofer_module { };
	};

	void initialize( c_config config );

	struct allocated_region_t {
		std::uint64_t m_base_address;
		std::uint64_t m_mapped_address;
		std::size_t   m_size;
	};

	namespace target {
		using allocated_region_t = ::bypass::allocated_region_t;

		extern std::uint32_t m_process_pid;
		extern HWND m_window_handle;
		extern std::uint64_t m_eprocess;
		extern std::uint64_t m_process_peb;
		extern std::uint64_t m_base_address;
		extern std::uint64_t m_directory_table_base;

		bool setup( );

		std::uint64_t translate_linear( std::uint64_t virt_addr, std::uint32_t* page_size = nullptr );
		std::optional<allocated_region_t> allocate_virtual( std::size_t size );
		bool free_virtual( const std::optional<allocated_region_t>& region );

		std::uintptr_t get_module_base( const wchar_t* module_name );
		std::uintptr_t get_module_export( std::uintptr_t module_base, const char* export_name );
		std::uint64_t pattern_scan( std::uint64_t module_base, const char* pattern, int offset = 0, bool relative = false );

		bool read_memory( std::uint64_t va, void* buf, std::size_t size );
		bool write_memory( std::uint64_t va, void* buf, std::size_t size );
		void benchmark_rps( );

		std::vector<std::pair<std::uint32_t, void*>> enumerate_threads( );
		std::pair<std::uint32_t, void*> select_injection_thread( );
		void*  lookup_thread( std::uint32_t thread_id );
		bool   suspend_thread( void* ethread );
		bool   resume_thread( void* ethread );
		bool   get_context_thread( void* ethread, CONTEXT* ctx );
		bool   set_context_thread( void* ethread, CONTEXT* ctx );

	}

	namespace mouse {
		bool move( double x, double y, bool use_virtual_desktop = false );
		bool click( );
	}

	namespace overlay {
		enum class text_align_t : std::uint32_t {
			left,
			center,
			right
		};

		enum class text_weight_t : std::uint32_t {
			thin = 100,
			light = 300,
			regular = 400,
			medium = 500,
			semi_bold = 600,
			bold = 700,
			black = 900
		};

		struct color_t {
			std::uint8_t r {};
			std::uint8_t g {};
			std::uint8_t b {};
			std::uint8_t a { 255 };
		};

		struct point_t {
			float x {};
			float y {};
		};

		struct rect_t {
			float x {};
			float y {};
			float width {};
			float height {};
		};

		struct skeleton_segment_t {
			point_t from {};
			point_t to {};
		};

		struct monitor_info_t {
			std::int32_t left {};
			std::int32_t top {};
			std::int32_t right {};
			std::int32_t bottom {};
			bool         is_primary {};
		};

		struct aurora_blob_t {
			float   x {};
			float   y {};
			float   radius {};
			color_t color {};
		};

		bool clear( );
		bool minimize( );
		bool bring_to_top( );

		bool line( point_t from, point_t to, color_t color, float thickness );
		bool rectangle( rect_t area, color_t color, float thickness );
		bool filled_rectangle( rect_t area, color_t color );

		bool circle( point_t center, float radius, color_t color, float thickness );
		bool filled_circle( point_t center, float radius, color_t color );

		bool text(
			const wchar_t* value,
			rect_t area,
			color_t color,
			float size,
			text_weight_t weight,
			const wchar_t* font,
			text_align_t alignment,
			bool wrap
		);

		bool interactive_area( rect_t area );
		bool register_font( const wchar_t* name, const wchar_t* path );
		bool register_image( std::uint32_t id, const wchar_t* path );

		bool image(
			std::uint32_t id,
			rect_t area,
			color_t tint = { 255, 255, 255, 255 },
			float opacity = 1.f,
			float corner_radius = 0.f
		);

		bool rounded_rectangle( rect_t area, float radius, color_t color, float thickness = 1.0f );
		bool filled_rounded_rectangle( rect_t area, float radius, color_t color );
		bool aurora( rect_t area, float corner_radius, float sigma, float grain_strength, const aurora_blob_t* blobs, std::uint32_t blob_count );
		bool frosted_panel( rect_t area, float corner_radius, float sigma, color_t tint );
		bool text_ex( const wchar_t* value, rect_t area, color_t color, float size, text_weight_t weight, const wchar_t* font, text_align_t alignment, bool wrap = false );
		bool line_async( point_t from, point_t to, color_t color, float thickness );
		bool skeleton_async( const skeleton_segment_t* segments, std::uint32_t count, color_t color, float thickness = 1.5f, bool outline = true );
		bool filled_rectangle_async( rect_t area, color_t color );
		bool text_async( const wchar_t* value, rect_t area, color_t color, float size, text_weight_t weight, const wchar_t* font, text_align_t alignment, bool wrap = false );

		bool push_opacity_layer( rect_t, float ) { return false; }
		void pop_layer( ) { }

		bool set_monitor( std::uint32_t index );
		std::vector< monitor_info_t > enumerate_monitors( );
		bool get_size( std::int32_t& width, std::int32_t& height );

		constexpr color_t from_hex( uint32_t rgb, float a = 1.0f ) {
			return {
				static_cast< std::uint8_t >( ( ( rgb >> 16 ) & 0xff ) / 255.0f ),
				static_cast< std::uint8_t >( ( ( rgb >> 8 ) & 0xff ) / 255.0f ),
				static_cast< std::uint8_t >( ( ( rgb >> 0 ) & 0xff ) / 255.0f ),
				 static_cast< std::uint8_t >( a )
			};
		}
	}

	template <typename ret_t = std::uint64_t, typename addr_t>
	ret_t read( addr_t va ) {
		std::uint64_t va64;
		if constexpr ( std::is_pointer_v<addr_t> )
			va64 = reinterpret_cast< std::uint64_t >( va );
		else if constexpr ( std::is_integral_v<addr_t> )
			va64 = static_cast< std::uint64_t >( va );
		else
			static_assert( std::is_pointer_v<addr_t> || std::is_integral_v<addr_t>,
				"addr_t must be pointer or integral" );
		ret_t ret {};
		target::read_memory( va64, &ret, sizeof( ret ) );
		return ret;
	}

	template <typename val_t, typename addr_t>
	bool write( addr_t va, val_t val ) {
		std::uint64_t va64;
		if constexpr ( std::is_pointer_v<addr_t> )
			va64 = reinterpret_cast< std::uint64_t >( va );
		else if constexpr ( std::is_integral_v<addr_t> )
			va64 = static_cast< std::uint64_t >( va );
		else
			static_assert( std::is_pointer_v<addr_t> || std::is_integral_v<addr_t>,
				"addr_t must be pointer or integral" );

		return target::write_memory( va64, &val, sizeof( val_t ) );
	}
}
