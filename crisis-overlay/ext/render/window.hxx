#pragma once

#include "types.hxx"
#include <core/render/window.hxx>
#include <Windows.h>
#include <string_view>

namespace render {
	class c_window {
	public:
		c_window( ) = default;
		~c_window( );

		c_window( const c_window& ) = delete;
		c_window& operator=( const c_window& ) = delete;

		bool create( int width, int height, std::wstring_view title );
		bool create_overlay( std::wstring_view title );
		bool attach( ::window::c_window& window );
		void destroy( );
		bool poll( );
		bool consume_resize( );
		void set_click_through( bool enabled );
		void set_interactive_area( rect_t area );
		void minimize( );
		void bring_to_top( );

		HWND handle( ) const { return m_handle; }
		int width( ) const { return m_width; }
		int height( ) const { return m_height; }

	private:
		static LRESULT CALLBACK window_proc( HWND window, UINT message, WPARAM wparam, LPARAM lparam );

		HWND m_handle = nullptr;
		HINSTANCE m_instance = nullptr;
		int m_width = 0;
		int m_height = 0;
		bool m_resize_pending = false;
		bool m_closed = false;
		bool m_overlay = false;
		bool m_borrowed = false;
		::window::c_window* m_borrowed_window = nullptr;
		rect_t m_interactive_area{ };
	};
}
