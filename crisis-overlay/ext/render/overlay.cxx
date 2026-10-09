#include <deps/includes.h>
#include "overlay.hxx"
#include <utility>

namespace render {
	c_overlay::~c_overlay( ) {
		stop( );
	}

	bool c_overlay::start( draw_callback_t draw ) {
		if ( m_running.exchange( true ) )
			return true;
		set_draw_callback( std::move( draw ) );
		m_external_window = nullptr;
		m_borrowed_window.store( false );

		{
			std::lock_guard lock( m_start_mutex );
			m_start_complete = false;
			m_start_succeeded = false;
		}

		m_thread = std::thread( &c_overlay::run, this );
		std::unique_lock lock( m_start_mutex );
		m_start_condition.wait( lock, [ this ] { return m_start_complete; } );
		if ( !m_start_succeeded ) {
			lock.unlock( );
			stop( );
			return false;
		}
		return true;
	}

	bool c_overlay::start( ::window::c_window& window, draw_callback_t draw ) {
		if ( !window.handle( ) )
			return false;

		if ( m_running.exchange( true ) )
			return true;
		set_draw_callback( std::move( draw ) );
		m_external_window = &window;
		m_borrowed_window.store( true );

		{
			std::lock_guard lock( m_start_mutex );
			m_start_complete = false;
			m_start_succeeded = false;
		}

		m_thread = std::thread( &c_overlay::run, this );
		std::unique_lock lock( m_start_mutex );
		m_start_condition.wait( lock, [ this ] { return m_start_complete; } );
		if ( !m_start_succeeded ) {
			lock.unlock( );
			stop( );
			return false;
		}
		return true;
	}

	void c_overlay::set_draw_callback( draw_callback_t draw ) {
		std::lock_guard lock( m_draw_mutex );
		m_draw = std::move( draw );
	}

	void c_overlay::stop( ) {
		m_running.store( false );
		if ( auto window = m_window.load( ); window && !m_borrowed_window.load( ) )
			PostMessageW( window, WM_CLOSE, 0, 0 );

		if ( m_thread.joinable( ) && m_thread.get_id( ) != std::this_thread::get_id( ) )
			m_thread.join( );
		m_window.store( nullptr );
		m_borrowed_window.store( false );
	}

	void c_overlay::run( ) {
		auto com_initialized = SUCCEEDED( CoInitializeEx( nullptr, COINIT_MULTITHREADED ) );
		c_window window;
		c_device device;
		c_renderer renderer;
		auto initialized = m_external_window ? window.attach( *m_external_window ) : window.create_overlay( L"UDE" );
		if ( initialized ) {
			m_window.store( window.handle( ) );
			initialized = device.initialize( window );
		}
		if ( initialized )
			initialized = renderer.initialize( device );

		{
			std::lock_guard lock( m_start_mutex );
			m_start_succeeded = initialized;
			m_start_complete = true;
		}
		m_start_condition.notify_one( );

		while ( initialized && m_running.load( ) && window.poll( ) ) {
			if ( !renderer.begin_frame( color_t::from_rgba( 0, 0, 0, 0 ) ) )
				break;

			draw_callback_t draw;
			{
				std::lock_guard lock( m_draw_mutex );
				draw = m_draw;
			}
			if ( draw )
				draw( renderer );

			if ( !renderer.end_frame( ) )
				break;
		}

		m_running.store( false );
		m_window.store( nullptr );
		m_external_window = nullptr;
		m_borrowed_window.store( false );
		renderer.shutdown( );
		device.shutdown( );
		window.destroy( );
		if ( com_initialized )
			CoUninitialize( );
	}
}
