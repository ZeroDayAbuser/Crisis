#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#  include <winsock2.h>
#endif
#include <Windows.h>
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace mapping {
	static constexpr char     k_name[ ] = "Local\\crisis.bypass";
	static constexpr uint32_t k_magic = 0x53495243u;
	static constexpr uint32_t k_version = 3u;
	static constexpr uint32_t k_section_size = 0x10000u;

	struct vtable_t {
		uint32_t m_magic;
		uint32_t m_version;
		uint32_t m_size;
		uint32_t m_pad;

		bool      ( *m_initialize )( const wchar_t*, const wchar_t* );
		bool      ( *m_create )( );
		void      ( *m_cleanup )( );
		bool      ( *m_setup_target )( );
		uint32_t( *m_get_process_id )( );
		void* ( *m_get_window_handle )( );
		uint64_t( *m_get_base_address )( );
		uint64_t( *m_get_directory_table_base )( );
		uint64_t( *m_get_process_peb )( );
		uint64_t( *m_get_eprocess )( );
		uint64_t( *m_translate_linear )( uint64_t, uint32_t* );
		bool      ( *m_read_memory )( uint64_t, void*, size_t );
		bool      ( *m_write_memory )( uint64_t, void*, size_t );
		uintptr_t( *m_get_module_base )( const wchar_t* );
		uintptr_t( *m_get_module_export )( uintptr_t, const char* );
		uint64_t( *m_pattern_scan )( uint64_t, const char*, int, bool );
		void      ( *m_benchmark_rps )( );
		bool      ( *m_mouse_move )( double, double, bool );
		bool      ( *m_mouse_click )( );
		bool      ( *m_mouse_click_at )( double, double, bool );
		bool      ( *m_overlay_clear )( );
		bool      ( *m_overlay_present )( );
		bool      ( *m_overlay_minimize )( );
		bool      ( *m_overlay_bring_to_top )( );
		bool      ( *m_overlay_line )( float, float, float, float, uint8_t, uint8_t, uint8_t, uint8_t, float );
		bool      ( *m_overlay_line_async )( float, float, float, float, uint8_t, uint8_t, uint8_t, uint8_t, float );
		bool      ( *m_overlay_rectangle )( float, float, float, float, uint8_t, uint8_t, uint8_t, uint8_t, float );
		bool      ( *m_overlay_filled_rectangle )( float, float, float, float, uint8_t, uint8_t, uint8_t, uint8_t );
		bool      ( *m_overlay_filled_rectangle_async )( float, float, float, float, uint8_t, uint8_t, uint8_t, uint8_t );
		bool      ( *m_overlay_circle )( float, float, float, uint8_t, uint8_t, uint8_t, uint8_t, float );
		bool      ( *m_overlay_filled_circle )( float, float, float, uint8_t, uint8_t, uint8_t, uint8_t );
		bool      ( *m_overlay_text )( const wchar_t*, float, float, float, float, uint8_t, uint8_t, uint8_t, uint8_t, float, uint32_t, const wchar_t*, uint32_t, bool );
		bool      ( *m_overlay_text_async )( const wchar_t*, float, float, float, float, uint8_t, uint8_t, uint8_t, uint8_t, float, uint32_t, const wchar_t*, uint32_t, bool );
		bool      ( *m_overlay_interactive_area )( float, float, float, float );
		bool      ( *m_overlay_register_font )( const wchar_t*, const wchar_t* );
		bool      ( *m_overlay_register_image )( uint32_t, const wchar_t* );
		bool      ( *m_overlay_image )( uint32_t, float, float, float, float, uint8_t, uint8_t, uint8_t, uint8_t, float, float );
		bool      ( *m_overlay_rounded_rectangle )( float, float, float, float, float, uint8_t, uint8_t, uint8_t, uint8_t, float );
		bool      ( *m_overlay_filled_rounded_rectangle )( float, float, float, float, float, uint8_t, uint8_t, uint8_t, uint8_t );
		bool      ( *m_overlay_aurora )( float, float, float, float, float, float, float, const float*, uint32_t );
		bool      ( *m_overlay_frosted_panel )( float, float, float, float, float, float, uint8_t, uint8_t, uint8_t, uint8_t );
		bool      ( *m_overlay_gradient_rectangle )( float, float, float, float, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, float );
		bool      ( *m_overlay_gradient_rectangle_h )( float, float, float, float, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, float );
		bool      ( *m_overlay_push_clip )( float, float, float, float );
		bool      ( *m_overlay_pop_clip )( );
		bool      ( *m_overlay_skeleton )( const float*, uint32_t, uint8_t, uint8_t, uint8_t, uint8_t, float, bool );
		bool      ( *m_overlay_set_monitor )( uint32_t );
		bool      ( *m_overlay_get_size )( int32_t*, int32_t* );
		uint32_t( *m_overlay_enumerate_monitors )( int32_t*, uint32_t );
		bool      ( *m_overlay_scene3d )( float, float, float, float,
			uint8_t, uint8_t, uint8_t, uint8_t, float, float, float, float );

		// v3 additions.
		// Invoke a function inside the target process by hijacking the
		// currently-configured execution hook slot. `func` is the absolute
		// VA to call; a1..a4 land in rcx/rdx/r8/r9, a5..a10 in the
		// [rsp+0x20..0x48] Windows-x64 shadow-then-stack region.
		//
		// stack_params / stack_params_size lets the caller stash a blob
		// (e.g. a wide string, an OBJECT_ATTRIBUTES, a UNICODE_STRING) in
		// the region and pass placeholder VAs relative to `stack_param_base`
		// (0x0000800000000000) as arguments - the bypass rewrites those
		// placeholders to real region-VAs on the target side. Returns the
		// callee's rax, or 0 on failure / timeout.
		uint64_t  ( *m_call_function )(
			uint64_t func,
			uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
			uint64_t a5, uint64_t a6, uint64_t a7, uint64_t a8,
			uint64_t a9, uint64_t a10,
			void*    stack_params,
			size_t   stack_params_size );
	};

	inline bool vtable_valid( const vtable_t& vtable ) {
		if ( vtable.m_magic != k_magic || vtable.m_version != k_version )
			return false;
		if ( vtable.m_size < sizeof( vtable_t ) )
			return false;
		return vtable.m_initialize
			&& vtable.m_create
			&& vtable.m_read_memory
			&& vtable.m_write_memory;
	}

	class c_mapping {
	public:
		c_mapping( ) = default;
		~c_mapping( ) { close( ); }

		bool publish( const vtable_t& vt ) {
			close( );
			m_hmap = CreateFileMappingA( INVALID_HANDLE_VALUE, nullptr,
				PAGE_READWRITE, 0, k_section_size, k_name );
			if ( !m_hmap ) return false;
			m_view = MapViewOfFile( m_hmap, FILE_MAP_WRITE, 0, 0, k_section_size );
			if ( !m_view ) { close( ); return false; }
			memset( m_view, 0, k_section_size );
			memcpy( m_view, &vt, sizeof( vtable_t ) );
			return true;
		}

		bool open_consumer( vtable_t& out, int retries = 100, DWORD delay_ms = 100 ) {
			for ( int i = 0; i < retries; ++i ) {
				HANDLE h = OpenFileMappingA( FILE_MAP_READ, FALSE, k_name );
				if ( h ) {
					const void* v = MapViewOfFile( h, FILE_MAP_READ, 0, 0, 0 );
					CloseHandle( h );
					if ( v ) {
						MEMORY_BASIC_INFORMATION mbi {};
						const auto view_bytes = VirtualQuery( v, &mbi, sizeof( mbi ) )
							? mbi.RegionSize : 0;
						vtable_t parsed {};
						if ( view_bytes >= sizeof( uint32_t ) * 3 ) {
							const auto* raw = static_cast< const uint8_t* >( v );
							parsed.m_magic = *reinterpret_cast< const uint32_t* >( raw );
							parsed.m_version = *reinterpret_cast< const uint32_t* >( raw + 4 );
							parsed.m_size = *reinterpret_cast< const uint32_t* >( raw + 8 );
							const auto copy = ( parsed.m_size < sizeof( vtable_t ) )
								? parsed.m_size : sizeof( vtable_t );
							if ( copy <= view_bytes )
								memcpy( &parsed, v, copy );
						}
						UnmapViewOfFile( const_cast< void* >( v ) );
						if ( vtable_valid( parsed ) ) {
							out = parsed;
							return true;
						}
					}
				}
				Sleep( delay_ms );
			}
			return false;
		}

		void close( ) {
			if ( m_view ) { UnmapViewOfFile( m_view ); m_view = nullptr; }
			if ( m_hmap ) { CloseHandle( m_hmap );    m_hmap = nullptr; }
		}

	private:
		HANDLE m_hmap = nullptr;
		void* m_view = nullptr;
	};
}