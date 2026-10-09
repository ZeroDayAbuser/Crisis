#pragma once
namespace streamproof {
	class c_streamproof {
	public:
		bool setup( ) {
			m_pdb = std::make_unique< pdb::c_pdb >( obf( "win32kfull.sys" ) );
			if ( !m_pdb || !m_pdb->load( ) ) {
				logging::print( obf( "failed to load win32kfull.sys pdb" ) );
				return false;
			}

			m_gre_protect_sprite_content = m_pdb->get_symbol_address( obf( "GreProtectSpriteContent" ) );
			if ( !m_gre_protect_sprite_content ) {
				logging::print( obf( "failed to find GreProtectSpriteContent" ) );
				return false;
			}

			m_protected_window = find_overlay_window( );
			if ( !m_protected_window ) {
				logging::print( obf( "failed to find overlay window" ) );
				return false;
			}

			return true;
		}

		bool hide_overlay( bool enabled ) {
			return apply_protection( m_protected_window, enabled );
		}

	private:
		std::unique_ptr< pdb::c_pdb > m_pdb { };
		std::uint64_t m_gre_protect_sprite_content { };
		std::uint64_t m_stub_page { };
		HWND m_protected_window { };

		static HWND find_overlay_window( ) {
			HWND result = nullptr;
			EnumWindows( [ ]( HWND handle, LPARAM parameter ) -> BOOL {
				wchar_t class_name[ 64 ] { };
				GetClassNameW( handle, class_name, _countof( class_name ) );
				if ( wcscmp( class_name, obf( L"SystemTray_Main" ) ) == 0 ) {
					*reinterpret_cast< HWND* >( parameter ) = handle;
					return FALSE;
				}
				return TRUE;
				}, reinterpret_cast< LPARAM >( &result ) );
			return result;
		}

		bool apply_protection( HWND window_handle, bool enabled ) {
			const std::uint64_t flags = enabled ? 0x11u : 0x00u;
			const auto result = g_syscall->call_kernel(
				static_cast< uintptr_t >( m_gre_protect_sprite_content ),
				static_cast< std::uint64_t >( 0 ),
				static_cast< std::uint64_t >( reinterpret_cast< uintptr_t >( window_handle ) ),
				static_cast< std::uint64_t >( 1 ),
				flags
			);

			return result != 0;
		}
	};
}