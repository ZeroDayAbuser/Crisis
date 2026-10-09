#include <deps\includes.h>
#include <csignal>

static bypass::startup_t              s_startup {};
std::unique_ptr<pdb_scanner_t>        g_scanner;
static std::unique_ptr<std::uint8_t[]> s_pdb_blob;

static DWORD WINAPI bypass_init( LPVOID ) {
	if ( !g_scanner ) {
		logging::print( obf( "pdb blob missing - aborting init" ) );
		return 1;
	}

	if ( !g_memory->open( ) ) {
		logging::print( obf( "failed to open memory channel" ) );
		return 1;
	}

	memory::m_read_physical = [ ]( std::uint64_t a, void* b, std::size_t s ) {
		return g_memory->read( a, b, s );
		};

	memory::m_write_physical = [ ]( std::uint64_t a, void* b, std::size_t s ) {
		return g_memory->write( a, b, s );
		};

	g_paging->m_system_dtb = s_startup.m_system_dtb;
	g_paging->m_target_dtb = s_startup.m_system_dtb;

	const auto mm_pfn  = g_scanner->get_symbol( obf( "MmPfnDatabase" ) );
	const auto off_u4  = g_scanner->get_member( obf( "_MMPFN" ), obf( "u4" ) );
	const auto sz_mmpfn = off_u4 + sizeof( std::uint64_t );

	if ( !mm_pfn || !off_u4 ) {
		logging::print( obf( "pdb symbols missing (pfn=0x%llx u4=0x%llx)" ), mm_pfn, off_u4 );
		return 1;
	}

	if ( !g_paging->configure_dtb_scan( mm_pfn, off_u4, sz_mmpfn ) ) {
		logging::print( obf( "configure_dtb_scan failed (pfn=0x%llx sz=%llu)" ),
			mm_pfn, sz_mmpfn );
		return 1;
	}

	if ( !g_syscall->setup( ) ) {
		logging::print( obf( "failed to setup syscall" ) );
		return 1;
	}

	if ( !g_overlay->open( ) ) {
		logging::print( obf( "failed to open overlay channel" ) );
		return 1;
	}

	const auto winlogon = utility::find_pid( obf( L"winlogon.exe" ) );
	const auto explorer  = utility::find_pid( obf( L"explorer.exe"  ) );

	obf_bogus_16( );

	mapping::vtable_t vtable { };
	vtable.m_magic   = mapping::k_magic;
	vtable.m_version = mapping::k_version;
	vtable.m_size    = sizeof( mapping::vtable_t );
	vtable.m_pad     = 0;

	vtable.m_initialize               = initialize;
	vtable.m_create                   = create;
	vtable.m_cleanup                  = cleanup;
	vtable.m_setup_target             = setup_target;
	vtable.m_get_process_id           = get_process_id;
	vtable.m_get_window_handle        = get_window_handle;
	vtable.m_get_base_address         = get_base_address;
	vtable.m_get_directory_table_base = get_directory_table_base;
	vtable.m_get_process_peb          = get_process_peb;
	vtable.m_get_eprocess             = get_eprocess;
	vtable.m_translate_linear         = translate_linear;
	vtable.m_read_memory              = read_memory;
	vtable.m_write_memory             = write_memory;
	vtable.m_get_module_base          = get_module_base;
	vtable.m_get_module_export        = get_module_export;
	vtable.m_pattern_scan             = pattern_scan;
	vtable.m_benchmark_rps            = benchmark_rps;
	vtable.m_mouse_move               = mouse_move;
	vtable.m_mouse_click              = mouse_click;
	vtable.m_mouse_click_at           = mouse_click_at;
	vtable.m_overlay_clear            = overlay_clear;
	vtable.m_overlay_present          = overlay_present;
	vtable.m_overlay_minimize         = overlay_minimize;
	vtable.m_overlay_bring_to_top     = overlay_bring_to_top;
	vtable.m_overlay_line             = overlay_line;
	vtable.m_overlay_line_async       = overlay_line_async;
	vtable.m_overlay_rectangle        = overlay_rectangle;
	vtable.m_overlay_filled_rectangle = overlay_filled_rectangle;
	vtable.m_overlay_filled_rectangle_async = overlay_filled_rectangle_async;
	vtable.m_overlay_circle           = overlay_circle;
	vtable.m_overlay_filled_circle    = overlay_filled_circle;
	vtable.m_overlay_text             = overlay_text;
	vtable.m_overlay_text_async       = overlay_text_async;
	vtable.m_overlay_interactive_area = overlay_interactive_area;
	vtable.m_overlay_register_font    = overlay_register_font;
	vtable.m_overlay_register_image   = overlay_register_image;
	vtable.m_overlay_image            = overlay_image;
	vtable.m_overlay_rounded_rectangle         = overlay_rounded_rectangle;
	vtable.m_overlay_filled_rounded_rectangle  = overlay_filled_rounded_rectangle;
	vtable.m_overlay_aurora           = overlay_aurora;
	vtable.m_overlay_frosted_panel    = overlay_frosted_panel;
	vtable.m_overlay_gradient_rectangle   = overlay_gradient_rectangle;
	vtable.m_overlay_gradient_rectangle_h = overlay_gradient_rectangle_h;
	vtable.m_overlay_push_clip        = overlay_push_clip;
	vtable.m_overlay_pop_clip         = overlay_pop_clip;
	vtable.m_overlay_skeleton         = overlay_skeleton;
	vtable.m_overlay_set_monitor      = overlay_set_monitor;
	vtable.m_overlay_get_size         = overlay_get_size;
	vtable.m_overlay_enumerate_monitors = overlay_enumerate_monitors;
	vtable.m_overlay_scene3d          = overlay_scene3d;
	vtable.m_call_function            = call_function;








	return g_vtable_data->publish( vtable );
}

BOOL APIENTRY DllMain( HMODULE, DWORD ul_reason_for_call, LPVOID lp_reserved ) {
	if ( !lp_reserved )
		return false;

	const auto startup = *static_cast< const bypass::startup_t* >( lp_reserved );
	if ( startup.m_magic != bypass::magic_code ||
		startup.m_version != bypass::version ||
		startup.m_size != sizeof( bypass::startup_t ) )
		return false;

	if ( startup.m_pdb_blob_size > 0 ) {
		const auto blob_src = static_cast<const std::uint8_t*>( lp_reserved ) + sizeof( bypass::startup_t );
		s_pdb_blob = std::make_unique<std::uint8_t[]>( startup.m_pdb_blob_size );
		std::memcpy( s_pdb_blob.get( ), blob_src, startup.m_pdb_blob_size );
		g_scanner = std::make_unique<pdb_scanner_t>( );
		g_scanner->init( s_pdb_blob.get( ), startup.m_pdb_blob_size );
	}

	s_startup = startup;
	CreateThread( nullptr, 0, bypass_init, nullptr, 0, nullptr );
	return TRUE;
}
