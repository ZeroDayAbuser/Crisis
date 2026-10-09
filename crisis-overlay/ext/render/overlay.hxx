#pragma once

#include "renderer.hxx"
#include <core/render/window.hxx>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace render {
	class c_overlay {
	public:
		using draw_callback_t = std::function< void( c_renderer& ) >;

		c_overlay( ) = default;
		~c_overlay( );

		c_overlay( const c_overlay& ) = delete;
		c_overlay& operator=( const c_overlay& ) = delete;

		bool start( draw_callback_t draw = { } );
		bool start( ::window::c_window& window, draw_callback_t draw = { } );
		void stop( );
		void set_draw_callback( draw_callback_t draw );
		bool running( ) const { return m_running.load( ); }

	private:
		void run( );

		std::thread m_thread;
		std::atomic_bool m_running = false;
		std::atomic< HWND > m_window = nullptr;
		std::mutex m_start_mutex;
		std::condition_variable m_start_condition;
		std::mutex m_draw_mutex;
		draw_callback_t m_draw;
		::window::c_window* m_external_window = nullptr;
		std::atomic_bool m_borrowed_window = false;
		bool m_start_complete = false;
		bool m_start_succeeded = false;
	};
}
