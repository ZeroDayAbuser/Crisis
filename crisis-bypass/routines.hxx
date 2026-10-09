#pragma once
static std::wstring  g_process_name;
static std::wstring  g_product_name;
static std::uint32_t g_process_id = 0;
static HWND          g_window_handle = nullptr;
static std::uint64_t g_eprocess = 0;
static std::uint64_t g_process_peb = 0;
static std::uint64_t g_base_address = 0;
static std::uint64_t g_directory_table_base = 0;

static bool initialize( const wchar_t* process, const wchar_t* product ) {
	g_process_name = process ? process : L"";
	g_product_name = product ? product : L"";
	return true;
}

static bool create( ) {
	if ( !g_memory->is_connected( ) ) {
		if ( !g_memory->open( ) ) {
			logging::print( obf( "failed to open memory channel" ) );
			return false;
		}

		memory::m_read_physical = [ ]( std::uint64_t a, void* b, std::size_t s ) {
			return g_memory->read( a, b, s );
		};

		memory::m_write_physical = [ ]( std::uint64_t a, void* b, std::size_t s ) {
			return g_memory->write( a, b, s );
		};
	}

	if ( !g_overlay->is_connected( ) ) {
		if ( !g_overlay->open( ) ) {
			logging::print( obf( "failed to open overlay channel" ) );
			return false;
		}
	}

	return true;
}

static void cleanup( ) {
	g_memory->close( );
	g_overlay->close( );
}

static bool setup_target( ) {
	obf_bogus_16( );
	const auto format_name = utility::format_name(
		utility::wstring_to_ansi( g_process_name )
	);

	obf_bogus_16( );
	logging::print( obf( "searching for %s.." ), format_name.c_str( ) );
	obf_bogus_256( );

	obf_bogus_64( );
	obf_indirect_branch;

	logging::print( obf( "press f7 when %s loaded.." ),
		format_name.c_str( ) );

	while ( true ) {
		if ( GetAsyncKeyState( VK_F7 ) & 0x8000 )
			break;
		std::this_thread::sleep_for( std::chrono::microseconds( 500 ) );
	}

	obf_bogus_64( );
	obf_indirect_branch;

	g_process_id = utility::find_pid( g_process_name.c_str( ) );
	g_window_handle = utility::find_window_for_pid( g_process_id );
	process::g_target_window = g_window_handle;

	const auto eprocess = process::find_eprocess( g_process_id );
	if ( !eprocess ) return false;
	g_eprocess = *eprocess;

	handle_scan::scan( g_eprocess );

	obf_bogus_16( );
	logging::print( obf( "found process: 0x%llx" ), g_eprocess );
	obf_bogus_256( );

	obf_bogus_64( );
	const auto off_section_base = g_scanner->get_member( obf( "_EPROCESS" ), obf( "SectionBaseAddress" ) );
	if ( !memory::read_virtual( g_eprocess + off_section_base, &g_base_address, sizeof( std::uint64_t ) ) )
		return false;

	logging::print( obf( "found base address: 0x%llx" ), g_base_address );

	obf_bogus_64( );
	const auto off_peb = g_scanner->get_member( obf( "_EPROCESS" ), obf( "Peb" ) );
	if ( !memory::read_virtual( g_eprocess + off_peb, &g_process_peb, sizeof( std::uint64_t ) ) )
		return false;

	obf_bogus_16( );
	logging::print( obf( "found process peb: 0x%llx" ), g_process_peb );
	obf_bogus_256( );

	g_directory_table_base = g_paging->find_dtb( g_base_address, g_process_peb );
	if ( !g_directory_table_base ) {
		const auto off_dtb = g_scanner->get_member( obf( "_KPROCESS" ), obf( "DirectoryTableBase" ) );
		if ( !memory::read_virtual( g_eprocess + off_dtb, &g_directory_table_base, sizeof( std::uint64_t ) ) )
			return false;
		logging::print( obf( "found user directory table base: 0x%llx" ), g_directory_table_base );
	}
	else
		logging::print( obf( "found directory table base: 0x%llx" ), g_directory_table_base );

	g_paging->attach_process( g_directory_table_base );
	logging::print( obf( "found %s successfully!" ), format_name.c_str( ) );

	if ( !g_allocator->initialize( g_process_peb ) ) {
		logging::print( obf( "failed to initialize allocator!" ) );
		return false;
	}

	if ( !g_caller->setup( g_process_id, g_process_peb ) ) {
		logging::print( obf( "failed to initialize caller!" ) );
		return false;
	}

	g_caller->set_min_interval_ms( 0 );

	{
		const auto k32     = GetModuleHandleA( obf( "kernel32.dll" ) );
		const auto get_pid = reinterpret_cast< std::uint64_t >(
			GetProcAddress( k32, obf( "GetCurrentProcessId" ) ) );

		if ( get_pid ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );

			if ( !process::is_target_alive( ) && g_caller->worker_alive( ) ) {
				logging::print( obf( "worker not healthy after setup." ) );
				return true;
			}

			g_caller->reset_counters( );
			const auto probe = g_caller->call_function(
				get_pid, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, nullptr, 0 );

			if ( probe != static_cast< std::uint64_t >( g_process_id ) ) {
				logging::print( obf( "worker probe failed got 0x%llx, expected %u" ),
					probe, g_process_id );
				return true;
			}
		}
	}

	return true;
}

static uint64_t call_function(
	uint64_t func,
	uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
	uint64_t a5, uint64_t a6, uint64_t a7, uint64_t a8,
	uint64_t a9, uint64_t a10,
	void*    stack_params,
	size_t   stack_params_size )
{
	if ( !g_caller || !g_caller->is_initialized( ) )
		return 0;
	return g_caller->call_function(
		func,
		a1, a2, a3, a4, a5, a6, a7, a8, a9, a10,
		stack_params, stack_params_size );
}

static uint32_t  get_process_id( )            { return g_process_id; }
static void*     get_window_handle( )          { return g_window_handle; }
static uint64_t  get_base_address( )           { return g_base_address; }
static uint64_t  get_directory_table_base( )   { return g_directory_table_base; }
static uint64_t  get_process_peb( )            { return g_process_peb; }
static uint64_t  get_eprocess( )               { return g_eprocess; }

static uint64_t translate_linear( uint64_t va, uint32_t* page_size_out ) {
	return g_paging->translate_linear( va, page_size_out );
}

static bool read_memory( uint64_t va, void* buf, size_t size ) {
	auto remaining = size;
	auto src_va = va;
	auto dst = reinterpret_cast< std::uint8_t* >( buf );

	while ( remaining > 0 ) {
		const auto page_offset = src_va & paging::page_4kb_mask;
		const auto chunk = min( remaining, paging::page_4kb_size - page_offset );

		std::uint32_t page_size = 0;
		const auto phys = translate_linear( src_va, &page_size );
		if ( !phys ) return false;

		if ( !memory::read_physical( phys, dst, chunk ) )
			return false;

		src_va += chunk;
		dst += chunk;
		remaining -= chunk;
	}
	return true;
}

static bool write_memory( uint64_t va, void* buf, size_t size ) {
	auto remaining = size;
	auto dst_va = va;
	auto src = reinterpret_cast< const std::uint8_t* >( buf );

	while ( remaining > 0 ) {
		const auto page_offset = dst_va & paging::page_4kb_mask;
		const auto chunk = min( remaining, paging::page_4kb_size - page_offset );

		std::uint32_t page_size = 0;
		const auto phys = translate_linear( dst_va, &page_size );
		if ( !phys ) return false;

		if ( !memory::write_physical( phys, const_cast< std::uint8_t* >( src ), chunk ) )
			return false;

		dst_va += chunk;
		src += chunk;
		remaining -= chunk;
	}
	return true;
}

static uintptr_t get_module_base( const wchar_t* name ) {
	return process::get_module_base( g_process_peb, name );
}

static uintptr_t get_module_export( uintptr_t base, const char* name ) {
	return process::get_module_export( base, name );
}

static uint64_t pattern_scan( uint64_t base, const char* pattern, int offset, bool relative ) {
	return process::pattern_scan( base, pattern, offset, relative );
}

static void benchmark_rps( ) { 
	LARGE_INTEGER freq {}, t0 {}, t1 {};
	QueryPerformanceFrequency( &freq );

	std::uint64_t dummy = 0;
	QueryPerformanceCounter( &t0 );
	for ( auto i = 0; i < 1000; ++i )
		memory::read_virtual( g_base_address, &dummy, 8 );
	QueryPerformanceCounter( &t1 );

	const auto ns = ( t1.QuadPart - t0.QuadPart ) * 1'000'000'000LL / freq.QuadPart;
	logging::print( obf( "benchmark: %lld ns/op  (~%lld ops/sec)" ),
		ns / 1000, freq.QuadPart * 1000LL / ( t1.QuadPart - t0.QuadPart ) );
}

static bool mouse_move( double x, double y, bool vdesk )         {  return true; }
static bool mouse_click( )                                        {  return true; }
static bool mouse_click_at( double x, double y, bool vdesk )     {  return true; }

namespace cvt {
	static overlay::color_t col( uint8_t r, uint8_t g, uint8_t b, uint8_t a ) { return { r, g, b, a }; }
	static overlay::point_t pt( float x, float y )                             { return { x, y }; }
	static overlay::rect_t  rc( float x, float y, float w, float h )           { return { x, y, w, h }; }
}

static bool overlay_clear( )           { return g_overlay->clear( ); }
static bool overlay_present( )         { return g_overlay->present( ); }
static bool overlay_minimize( )        { return g_overlay->minimize( ); }
static bool overlay_bring_to_top( )    { return g_overlay->bring_to_top( ); }

static bool overlay_line( float fx, float fy, float tx, float ty,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a, float thickness ) {
	return g_overlay->line( cvt::pt( fx, fy ), cvt::pt( tx, ty ), cvt::col( r, g, b, a ), thickness );
}

static bool overlay_rectangle( float x, float y, float w, float h,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a, float thickness ) {
	return g_overlay->rectangle( cvt::rc( x, y, w, h ), cvt::col( r, g, b, a ), thickness );
}

static bool overlay_filled_rectangle( float x, float y, float w, float h,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a ) {
	return g_overlay->filled_rectangle( cvt::rc( x, y, w, h ), cvt::col( r, g, b, a ) );
}

static bool overlay_circle( float cx, float cy, float radius,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a, float thickness ) {
	return g_overlay->circle( cvt::pt( cx, cy ), radius, cvt::col( r, g, b, a ), thickness );
}

static bool overlay_filled_circle( float cx, float cy, float radius,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a ) {
	return g_overlay->filled_circle( cvt::pt( cx, cy ), radius, cvt::col( r, g, b, a ) );
}

static bool overlay_text( const wchar_t* value,
	float x, float y, float w, float h,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a,
	float size, uint32_t weight, const wchar_t* font, uint32_t align, bool wrap ) {
	return g_overlay->text( value, cvt::rc( x, y, w, h ), cvt::col( r, g, b, a ),
		size,
		static_cast< overlay::text_weight_t >( weight ),
		font,
		static_cast< overlay::text_align_t >( align ),
		wrap );
}

static bool overlay_interactive_area( float x, float y, float w, float h ) {
	return g_overlay->interactive_area( cvt::rc( x, y, w, h ) );
}

static bool overlay_register_font( const wchar_t* name, const wchar_t* path ) {
	return g_overlay->register_font( name, path );
}

static bool overlay_register_image( uint32_t id, const wchar_t* path ) {
	return g_overlay->register_image( id, path );
}

static bool overlay_image( uint32_t id,
	float x, float y, float w, float h,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a,
	float opacity, float corner_radius ) {
	return g_overlay->image( id, cvt::rc( x, y, w, h ), cvt::col( r, g, b, a ), opacity, corner_radius );
}

static bool overlay_rounded_rectangle( float x, float y, float w, float h,
	float radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float thickness ) {
	return g_overlay->rounded_rectangle( cvt::rc( x, y, w, h ), radius, cvt::col( r, g, b, a ), thickness );
}

static bool overlay_filled_rounded_rectangle( float x, float y, float w, float h,
	float radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a ) {
	return g_overlay->filled_rounded_rectangle( cvt::rc( x, y, w, h ), radius, cvt::col( r, g, b, a ) );
}

static bool overlay_aurora( float x, float y, float w, float h,
	float corner_radius, float sigma, float grain,
	const float* blob_data, uint32_t blob_count ) {
	if ( blob_count > 8 ) blob_count = 8;
	overlay::aurora_blob_t blobs[ 8 ] {};
	for ( uint32_t i = 0; i < blob_count; ++i ) {
		const float* d = blob_data + i * 7;
		blobs[ i ] = { d[ 0 ], d[ 1 ], d[ 2 ],
			{ static_cast< uint8_t >( d[ 3 ] ), static_cast< uint8_t >( d[ 4 ] ),
			  static_cast< uint8_t >( d[ 5 ] ), static_cast< uint8_t >( d[ 6 ] ) } };
	}
	return g_overlay->aurora( cvt::rc( x, y, w, h ), corner_radius, sigma, grain, blobs, blob_count );
}

static bool overlay_frosted_panel( float x, float y, float w, float h,
	float corner_radius, float sigma, uint8_t r, uint8_t g, uint8_t b, uint8_t a ) {
	return g_overlay->frosted_panel( cvt::rc( x, y, w, h ), corner_radius, sigma, cvt::col( r, g, b, a ) );
}

static bool overlay_gradient_rectangle( float x, float y, float w, float h,
	uint8_t ar, uint8_t ag, uint8_t ab, uint8_t aa,
	uint8_t br, uint8_t bg, uint8_t bb, uint8_t ba, float corner_radius ) {
	return g_overlay->gradient_rectangle( cvt::rc( x, y, w, h ),
		cvt::col( ar, ag, ab, aa ), cvt::col( br, bg, bb, ba ), corner_radius );
}

static bool overlay_gradient_rectangle_h( float x, float y, float w, float h,
	uint8_t ar, uint8_t ag, uint8_t ab, uint8_t aa,
	uint8_t br, uint8_t bg, uint8_t bb, uint8_t ba, float corner_radius ) {
	return g_overlay->gradient_rectangle_h( cvt::rc( x, y, w, h ),
		cvt::col( ar, ag, ab, aa ), cvt::col( br, bg, bb, ba ), corner_radius );
}

static bool overlay_push_clip( float x, float y, float w, float h ) {
	return g_overlay->push_clip( cvt::rc( x, y, w, h ) );
}

static bool overlay_pop_clip( ) { return g_overlay->pop_clip( ); }

static bool overlay_skeleton( const float* seg_data, uint32_t count,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a, float thickness, bool outline ) {
	const auto col     = cvt::col( r, g, b, a );
	const auto out_col = cvt::col( 0, 0, 0, a );
	for ( uint32_t i = 0; i < count; ++i ) {
		const float* s    = seg_data + i * 4;
		const auto   from = cvt::pt( s[ 0 ], s[ 1 ] );
		const auto   to   = cvt::pt( s[ 2 ], s[ 3 ] );
		if ( outline )
			g_overlay->line( from, to, out_col, thickness + 1.5f );
		g_overlay->line( from, to, col, thickness );
	}
	return true;
}

static bool overlay_set_monitor( uint32_t index ) {
	return g_overlay->set_monitor( index );
}

static bool overlay_get_size( int32_t* width, int32_t* height ) {
	if ( !width || !height ) return false;
	return g_overlay->get_size( *width, *height );
}

static uint32_t overlay_enumerate_monitors( int32_t* buf, uint32_t capacity ) {
	const auto monitors = overlay::c_overlay::enumerate_monitors( );
	if ( !buf ) return static_cast< uint32_t >( monitors.size( ) );
	const auto count = ( std::min )( capacity / 5, static_cast< uint32_t >( monitors.size( ) ) );
	for ( uint32_t i = 0; i < count; ++i ) {
		const auto& m = monitors[ i ];
		buf[ i * 5 + 0 ] = m.bounds.left;
		buf[ i * 5 + 1 ] = m.bounds.top;
		buf[ i * 5 + 2 ] = m.bounds.right;
		buf[ i * 5 + 3 ] = m.bounds.bottom;
		buf[ i * 5 + 4 ] = m.is_primary ? 1 : 0;
	}
	return static_cast< uint32_t >( monitors.size( ) );
}

static bool overlay_scene3d( float x, float y, float w, float h,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a,
	float rot_x, float rot_y, float rot_z, float opacity ) {
	return g_overlay->scene3d(
		overlay::rect_t { x, y, w, h },
		overlay::color_t { r, g, b, a },
		rot_x, rot_y, rot_z, opacity );
}

static bool overlay_line_async( float fx, float fy, float tx, float ty,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a, float thickness ) {
	return overlay_line( fx, fy, tx, ty, r, g, b, a, thickness );
}

static bool overlay_filled_rectangle_async( float x, float y, float w, float h,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a ) {
	return overlay_filled_rectangle( x, y, w, h, r, g, b, a );
}

static bool overlay_text_async( const wchar_t* value,
	float x, float y, float w, float h,
	uint8_t r, uint8_t g, uint8_t b, uint8_t a,
	float size, uint32_t weight, const wchar_t* font, uint32_t align, bool wrap ) {
	return overlay_text( value, x, y, w, h, r, g, b, a, size, weight, font, align, wrap );
}
