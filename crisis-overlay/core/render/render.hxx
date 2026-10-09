#pragma once

#include <ext/render/scene3d.hxx>

#pragma comment( lib, "winmm.lib" )
extern "C" __declspec( dllimport ) UINT WINAPI timeBeginPeriod( UINT );
extern "C" __declspec( dllimport ) UINT WINAPI timeEndPeriod  ( UINT );

namespace render {
    class c_render {
    public:
        c_render( ) = default;
        ~c_render( ) {
            shutdown( );
        }

        c_render( const c_render& ) = delete;
        c_render& operator=( const c_render& ) = delete;

        bool setup( window::c_window& window ) {
            if ( m_thread )
                return true;

            m_window = &window;
            m_running.store( true );
            m_thread = utility::thread::create( thread_entry, this );
            if ( !m_thread ) {
                m_running.store( false );
                m_window = nullptr;
                return false;
            }

            // Elevate priority so UpdateLayeredWindow and the m_frame_cv
            // wake both complete without scheduler-induced latency.
            // ABOVE_NORMAL (not TIME_CRITICAL) because the render thread
            // does real GPU work and blocking it at TIME_CRITICAL level
            // can starve DWM and other system compositors.
            SetThreadPriority( m_thread, THREAD_PRIORITY_ABOVE_NORMAL );

            return true;
        }

		void shutdown( ) {
			m_running.store( false );
			m_frame_cv.notify_all( );

            if ( m_thread ) {
                const auto exited = WaitForSingleObject( m_thread, 5000 ) == WAIT_OBJECT_0;
                if ( exited ) {
                    CloseHandle( m_thread );
                    m_thread = nullptr;
                    clear( );
                    m_window = nullptr;
                }
                else {
                    m_thread = nullptr;
                }
            }
            else {
                clear( );
                m_window = nullptr;
            }
        }

		void clear( ) {
			if ( t_in_batch_scope ) {
				m_items.clear( );
				return;
			}
			std::lock_guard lock( m_mutex );
			m_items.clear( );
		}

		void present( ) {
			if ( t_in_batch_scope ) {
				std::swap( m_items, m_last_complete );
				m_items.clear( );
				++m_frame_generation;
				m_frame_cv.notify_one( );
				return;
			}
			{
				std::lock_guard lock( m_mutex );
				std::swap( m_items, m_last_complete );
				m_items.clear( );
				++m_frame_generation;
			}
			m_frame_cv.notify_one( );
		}

		// Batch-scope helpers so the communication worker can hold
		// m_mutex ONCE across an entire render_batch dispatch instead
		// of acquiring/releasing it per push. Re-entrancy is via a
		// thread-local flag: push()/clear()/present()/etc all check
		// t_in_batch_scope; if set, the caller already holds the lock
		// and they skip their own acquire.
		//
		// Only the comm worker calls these; the thread-local scoping
		// keeps other threads (render worker) unaffected — their
		// push/clear/present paths still take the mutex normally.
		void begin_batch_scope( ) {
			m_mutex.lock( );
			t_in_batch_scope = true;
		}
		void end_batch_scope( )   {
			t_in_batch_scope = false;
			m_mutex.unlock( );
		}

        void clear_color( const communication::color_t& color ) {
            if ( t_in_batch_scope ) {
                m_clear = convert( color );
                return;
            }
            std::lock_guard lock( m_mutex );
            m_clear = convert( color );
        }

        bool line( const communication::line_request_t& request ) {
            draw_item_t item{};
            item.type = draw_type_t::line;
            item.line = request;
            return push( item );
        }

        bool skeleton( const communication::skeleton_request_t& request ) {
            if ( request.count == 0 || request.count > communication::skeleton_segment_capacity )
                return false;

            draw_item_t item{};
            item.type = draw_type_t::skeleton;
            item.skeleton = request;
            return push( item );
        }

        bool rectangle( const communication::rectangle_request_t& request, bool filled ) {
            draw_item_t item{};
            item.type = filled ? draw_type_t::filled_rectangle : draw_type_t::rectangle;
            item.rectangle = request;
            return push( item );
        }

        bool circle( const communication::circle_request_t& request, bool filled ) {
            draw_item_t item{};
            item.type = filled ? draw_type_t::filled_circle : draw_type_t::circle;
            item.circle = request;
            return push( item );
        }

        bool text( const communication::text_request_t& request ) {
            draw_item_t item{};
            item.type = draw_type_t::text;
            // text_request_t already carries value[text_capacity] and
            // font[font_name_capacity] as fixed inline wchar arrays; the
            // struct copy above stamps them into the draw item verbatim.
            // The prior std::wstring dup (item.text_value.assign / item.
            // text_font.assign) allocated on the comm worker's heap while
            // it held the render mutex under batch_scope — hundreds of
            // heap ops per text-heavy frame, all wasted duplication.
            // Draw side reads item.text.value/font directly (see draw()).
            item.text = request;
            return push( std::move( item ) );
        }

        void interactive_area( const communication::rect_t& area ) {
            if ( t_in_batch_scope ) {
                m_interactive_area = convert( area );
                m_interactive_area_dirty = true;
                return;
            }
            std::lock_guard lock( m_mutex );
            m_interactive_area = convert( area );
            m_interactive_area_dirty = true;
        }

        void minimize( ) {
            m_minimize_requested.store( true );
        }

        void bring_to_top( ) {
            m_bring_to_top_requested.store( true );
        }

        // Reposition the overlay onto the monitor at `index` in the deterministic
        // ordering enumerate_monitors() produces (primary first, then by top-left
        // corner). Runs the SetWindowPos on the requesting worker thread, which is
        // safe because the render thread only reads window.width/height via
        // consume_resize() -> refresh_size(), and refresh_size() will pick up the
        // new dimensions and trigger a swap chain resize on the next frame.
        bool set_monitor( std::uint32_t index ) {
            if ( !m_window )
                return false;

            return m_window->set_monitor_by_index( index );
        }

        // Current overlay drawing-surface size in physical pixels, as last
        // applied by attach()/set_monitor_by_index(). Client-facing get_size
        // request reads through here instead of GetSystemMetrics so it always
        // matches whichever monitor the overlay is actually on.
        std::int32_t width( ) const {
            return m_window ? m_window->width( ) : 0;
        }

        std::int32_t height( ) const {
            return m_window ? m_window->height( ) : 0;
        }

        bool register_font( const communication::font_register_request_t& request ) {
            if ( !request.name[ 0 ] || !request.path[ 0 ] )
                return false;

            std::wstring name( request.name );
            std::wstring key( name );
            for ( auto& ch : key )
                ch = static_cast< wchar_t >( ::towlower( ch ) );

            std::unique_lock lock( m_font_mutex );
            if ( !m_registered_fonts.insert( key ).second )
                return false;

            if ( GetFileAttributesW( request.path ) == INVALID_FILE_ATTRIBUTES ) {
                m_registered_fonts.erase( key );
                return false;
            }

            pending_font_t pending{};
            pending.name = std::move( name );
            pending.path.assign( request.path );

            const auto id = ++m_next_font_id;
            pending.id = id;
            m_pending_fonts.push_back( std::move( pending ) );

            while ( m_completed_fonts.find( id ) == m_completed_fonts.end( ) && m_running.load( ) ) {
                if ( m_font_cv.wait_for( lock, std::chrono::milliseconds( 500 ) ) == std::cv_status::timeout )
                    break;
            }

            const auto it = m_completed_fonts.find( id );
            if ( it == m_completed_fonts.end( ) ) {
                m_registered_fonts.erase( key );
                return false;
            }

            const auto ok = it->second;
            m_completed_fonts.erase( it );
            if ( !ok )
                m_registered_fonts.erase( key );
            return ok;
        }

        // Ask the render thread to WIC-decode `path` into an ID2D1Bitmap and
        // stash it under `id`. Blocks until the render thread has either
        // committed the load or rejected the file — same pending-queue pattern
        // register_font uses, since c_renderer::load_image needs the device
        // context that only the render thread owns.
        bool register_image( const communication::image_register_request_t& request ) {
            if ( !request.id || !request.path[ 0 ] )
                return false;

            std::unique_lock lock( m_image_mutex );
            if ( m_registered_images.contains( request.id ) )
                return true;

            if ( GetFileAttributesW( request.path ) == INVALID_FILE_ATTRIBUTES )
                return false;

            pending_image_t pending{};
            pending.id = request.id;
            pending.path.assign( request.path );
            const auto ticket = ++m_next_image_ticket;
            pending.ticket = ticket;
            m_pending_images.push_back( std::move( pending ) );

            while ( m_completed_images.find( ticket ) == m_completed_images.end( ) && m_running.load( ) ) {
                if ( m_image_cv.wait_for( lock, std::chrono::milliseconds( 500 ) ) == std::cv_status::timeout )
                    break;
            }

            const auto it = m_completed_images.find( ticket );
            if ( it == m_completed_images.end( ) )
                return false;

            const auto ok = it->second;
            m_completed_images.erase( it );
            if ( ok ) {
                m_registered_images.insert( request.id );
                m_image_paths[ request.id ] = std::wstring( request.path );
            }
            return ok;
        }

        bool image( const communication::image_request_t& request ) {
            draw_item_t item{};
            item.type  = draw_type_t::image;
            item.image = request;
            return push( item );
        }

        bool rounded_rectangle( const communication::rounded_rectangle_request_t& request, bool filled ) {
            draw_item_t item{};
            item.type = filled ? draw_type_t::filled_rounded_rectangle : draw_type_t::rounded_rectangle;
            item.rounded_rectangle = request;
            return push( item );
        }

        bool gradient_rectangle( const communication::gradient_rectangle_request_t& request, bool horizontal ) {
            draw_item_t item{};
            item.type = horizontal ? draw_type_t::gradient_filled_rectangle_h : draw_type_t::gradient_filled_rectangle;
            item.gradient_rect = request;
            return push( item );
        }

        bool aurora( const communication::aurora_request_t& request ) {
            draw_item_t item{};
            item.type = draw_type_t::aurora;
            item.aurora_item = request;
            return push( item );
        }

        bool frosted_panel( const communication::frosted_panel_request_t& request ) {
            draw_item_t item{};
            item.type = draw_type_t::frosted_panel;
            item.frosted = request;
            return push( item );
        }

        bool scene3d( const communication::scene3d_request_t& request ) {
            draw_item_t item{};
            item.type = draw_type_t::scene3d;
            item.scene3d = request;
            return push( item );
        }

        bool push_clip( communication::rect_t area ) {
            draw_item_t item{};
            item.type = draw_type_t::push_aa_clip;
            item.clip_rect = area;
            return push( item );
        }

        void pop_clip_aa( ) {
            draw_item_t item{};
            item.type = draw_type_t::pop_aa_clip;
            push( item );
        }

    private:
        enum class draw_type_t {
            line,
            rectangle,
            filled_rectangle,
            circle,
            filled_circle,
            text,
            image,
            rounded_rectangle,
            filled_rounded_rectangle,
            aurora,
            frosted_panel,
            skeleton,
            push_aa_clip,
            pop_aa_clip,
            gradient_filled_rectangle,    // color_a top  → color_b bottom
            gradient_filled_rectangle_h,  // color_a left → color_b right
            scene3d,                      // render built-in mesh, blit as image
        };

        struct draw_item_t {
            draw_type_t type{};
            communication::line_request_t line{};
            communication::rectangle_request_t rectangle{};
            communication::circle_request_t circle{};
            communication::text_request_t text{};
            // No auxiliary text_value/text_font: text_request_t's inline
            // wchar arrays (value[text_capacity], font[font_name_capacity])
            // are the source of truth. Removing the dup eliminates two
            // heap allocs per text push under the batch mutex, which under
            // heavy text load (hundreds of text items per frame) was
            // measurable pressure on the shared render lock. draw() reads
            // them straight out of item.text.
            communication::image_request_t image{};
            communication::rounded_rectangle_request_t rounded_rectangle{};
            communication::aurora_request_t aurora_item{};
            communication::frosted_panel_request_t frosted{};
            communication::skeleton_request_t skeleton{};
            communication::rect_t clip_rect{};
            communication::gradient_rectangle_request_t gradient_rect{};
            communication::scene3d_request_t scene3d{};
        };

        struct pending_font_t {
            std::uint64_t id{};
            std::wstring name{};
            std::wstring path{};
        };

        struct pending_image_t {
            std::uint64_t ticket{};
            std::uint32_t id{};
            std::wstring  path{};
        };

        HANDLE m_thread = nullptr;
        window::c_window* m_window = nullptr;
        std::atomic_bool m_running = false;
        std::atomic_bool m_minimize_requested = false;
        std::atomic_bool m_bring_to_top_requested = false;
		// Fast std::mutex + std::condition_variable. Batch-scope
		// reentrancy is handled by t_in_batch_scope (thread-local
		// flag) instead of by upgrading to recursive_mutex — the
		// recursive variant is measurably slower per-lock on Windows
		// and condition_variable_any adds its own hand-off overhead
		// on wait_for, both of which showed up as render-worker
		// stutter under packet load.
		std::mutex m_mutex;
		std::condition_variable m_frame_cv;
		inline static thread_local bool t_in_batch_scope = false;
		std::vector< draw_item_t > m_items;        // frame being built by the worker
		std::vector< draw_item_t > m_last_complete; // last fully-committed frame, read by render thread
		std::uint64_t m_frame_generation = 0;
        color_t m_clear = color_t::from_rgba( 0, 0, 0, 0 );
        rect_t m_interactive_area{};
        bool m_interactive_area_dirty = false;

        std::mutex m_font_mutex;
        std::condition_variable m_font_cv;
        std::vector< pending_font_t > m_pending_fonts;
        std::unordered_map< std::uint64_t, bool > m_completed_fonts;
        std::unordered_set< std::wstring > m_registered_fonts;
        std::uint64_t m_next_font_id = 0;

        // image cache: id -> D2D bitmap. only the render thread inserts, so
        // renderer.draw can read m_images without locking as long as the
        // pending path doesn't insert mid-frame. we handle that by only
        // inserting inside process_pending_images(), which the worker calls
        // strictly before draw() each iteration.
        std::mutex m_image_mutex;
        std::condition_variable m_image_cv;
        std::vector< pending_image_t > m_pending_images;
        std::unordered_map< std::uint64_t, bool > m_completed_images;
        std::unordered_set< std::uint32_t > m_registered_images;
        std::unordered_map< std::uint32_t, image_t > m_images;
        // id -> source path. Populated on successful register_image so a
        // device-loss rebuild can re-queue every previously registered
        // image for re-decode against the fresh D2D device. Without this,
        // the client's own m_registered_images set (in crisis-bypass)
        // still contains the id, so its next register_image call short-
        // circuits and the overlay is stuck with no bitmap for that id.
        std::unordered_map< std::uint32_t, std::wstring > m_image_paths;
        std::uint64_t m_next_image_ticket = 0;
        image_t m_aurora_cache;

        // Off-screen 3D scene — built-in humanoid mesh rendered into its
        // own D3D texture and blitted into the D2D pass as an image at
        // draw() time. Lives on the render worker's thread; tied to the
        // same c_device so a device-loss rebuild tears it down and
        // recreates it in the render worker's rebuild path.
        c_scene3d m_scene3d;

        // Try to swap the built-in procedural humanoid for a real asset.
        // Called on initial scene3d init and again after every successful
        // device rebuild — the mesh's D3D buffers are tied to the live
        // device so rebuilding invalidates them just like the shaders and
        // RTVs. Silent failure is fine; the procedural fallback stays.
        void try_load_preview_mesh( c_device& device ) {
            loaded_mesh_t mesh;
            if ( load_glb( device,
                    L"C:\\crisis\\models\\the_omega_tier_100_skin_fortnite_3d_model.glb",
                    mesh ) ) {
                m_scene3d.set_mesh( std::move( mesh ) );
                utility::print( "preview mesh loaded" );
            }
            // load_glb already logged the specific failure reason; no
            // additional message needed on the failure path.
        }

        static DWORD WINAPI thread_entry( void* context ) {
            auto self = reinterpret_cast< c_render* >( context );
            return self ? self->worker( ) : 0;
        }

        DWORD worker( ) {
            timeBeginPeriod( 1 );
            auto com_initialized = SUCCEEDED( CoInitializeEx( nullptr, COINIT_MULTITHREADED ) );

            c_window window;
            c_device device;
            c_renderer renderer;

            auto initialized = m_window && window.attach( *m_window );
            if ( initialized )
                initialized = device.initialize( window );
            if ( initialized )
                initialized = renderer.initialize( device );
            if ( initialized ) {
                // Scene3d failure is non-fatal — the 3D preview just won't
                // render, but the rest of the pipeline (D2D, images, text)
                // stays functional.
                if ( !m_scene3d.initialize( device ) )
                    utility::print( "scene3d init FAILED — 3D preview disabled" );
                else
                    try_load_preview_mesh( device );
            }

            if ( !initialized ) {
                utility::print( "failed to initialize render framework\n" );
                m_scene3d.shutdown( );
                renderer.shutdown( );
                device.shutdown( );
                window.destroy( );
                if ( com_initialized )
                    CoUninitialize( );
                m_running.store( false );
                timeEndPeriod( 1 );
                return 0;
            }

            utility::print( "render framework initialized\n" );

			std::uint64_t rendered_generation = 0;
			std::uint64_t frames_committed = 0;
			std::uint64_t end_frame_fails = 0;
			std::uint64_t rebuild_attempts = 0;
			auto last_present = std::chrono::steady_clock::now( );
			bool idle_frame_cleared = false;
			const char*   exit_reason = "poll returned false";

			// The render worker is now RESILIENT to end_frame failures.
			// A game going fullscreen (Fortnite exclusive-fullscreen is
			// the common trigger here) causes end_frame to fail —
			// D2DERR_RECREATE_TARGET → create_target() fails because
			// the D3D device itself is lost, or UpdateLayeredWindow
			// fails while the display is being taken over. Previously
			// any single such failure `break`ed the loop and left the
			// window with no compositor. Now we log the specific
			// failure, attempt a full device rebuild for device-loss
			// categories, and keep spinning; transient failures resolve
			// on their own within a frame or two.
			while ( m_running.load( ) && window.poll( ) ) {
                process_pending_fonts( device );
                process_pending_images( renderer );

				bool clear_only = false;
				{
					std::unique_lock lock( m_mutex );
					m_frame_cv.wait_for(
						lock, std::chrono::milliseconds( 8 ), [ & ] {
							return !m_running.load( ) ||
								m_frame_generation != rendered_generation;
						} );
					if ( !m_running.load( ) ) { exit_reason = "m_running false"; break; }
					if ( m_frame_generation != rendered_generation ) {
						rendered_generation = m_frame_generation;
						last_present = std::chrono::steady_clock::now( );
						idle_frame_cleared = false;
					} else {
						const auto idle_for = std::chrono::steady_clock::now( ) - last_present;
						if ( idle_frame_cleared || idle_for < std::chrono::seconds( 1 ) )
							continue;

						// Remove both the retained frame and an abandoned partial frame,
						// then commit transparency once. The next client present resumes
						// normal rendering without an idle-time presentation loop.
						m_items.clear( );
						m_last_complete.clear( );
						idle_frame_cleared = true;
						clear_only = true;
					}
				}

                if ( !renderer.begin_frame( color_t::from_rgba( 0, 0, 0, 0 ) ) ) {
                    // begin_frame is a lightweight setter; a failure here
                    // means preconditions went bad. Skip and try again.
                    Sleep( 16 );
                    continue;
                }

				if ( !clear_only )
					draw( renderer );

                if ( !renderer.end_frame( ) ) {
                    ++end_frame_fails;
                    const auto why = device.last_end_frame_failure( );
                    const auto hr  = device.last_hresult( );

					// WRONG_STATE is a malformed draw-state error, not device
					// loss. Rebuilding for it only repeats the bad frame and
					// needlessly reloads every registered bitmap.
					const bool device_lost =
						hr == DXGI_ERROR_DEVICE_REMOVED ||
						hr == DXGI_ERROR_DEVICE_RESET ||
						hr == DXGI_ERROR_DEVICE_HUNG ||
						hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
					const bool needs_rebuild =
						why == c_device::end_frame_failure_t::create_target ||
						( why == c_device::end_frame_failure_t::end_draw && device_lost );
                    if ( needs_rebuild ) {
                        ++rebuild_attempts;
                        utility::print( "device rebuild attempt #%llu (why=%d hr=0x%08X)",
                            (unsigned long long)rebuild_attempts, (int)why, (unsigned)hr );
                        // scene3d holds D3D resources on the OLD device —
                        // must tear down BEFORE device.reinitialize so we
                        // don't leak orphaned RTVs/textures on the dead
                        // device. Recreated after the fresh init below.
                        m_scene3d.shutdown( );
                        renderer.shutdown( );
                        // Every ID2D1Bitmap in the image cache and the
                        // aurora cache is tied to the OLD D3D device;
                        // using them after reinit would crash on the
                        // next draw. Drop them, but SNAPSHOT the id→path
                        // map first so we can re-queue every image for
                        // decode against the fresh device. The client's
                        // register_image would otherwise short-circuit
                        // on its own registered-ids cache and never ask
                        // us to reload.
                        std::unordered_map< std::uint32_t, std::wstring > paths_to_restore;
                        {
                            std::lock_guard lock( m_mutex );
                            m_items.clear( );
                            m_last_complete.clear( );
                        }
                        {
                            std::lock_guard ilock( m_image_mutex );
                            paths_to_restore = m_image_paths;
                            m_images.clear( );
						}
                        m_aurora_cache.Reset( );
                        if ( device.reinitialize( ) && renderer.initialize( device ) ) {
                            if ( !m_scene3d.initialize( device ) )
                                utility::print( "scene3d re-init FAILED after device rebuild" );
                            else
                                try_load_preview_mesh( device );
                            // Re-queue every previously registered image
                            // as a NEW pending item. The render worker
                            // (this loop) picks them up on the next
                            // iteration via process_pending_images and
                            // decodes each into a fresh D2D bitmap. A zero
                            // ticket suppresses request/reply bookkeeping.
                            std::size_t restored = 0;
                            {
                                std::lock_guard ilock( m_image_mutex );
                                for ( const auto& [ id, path ] : paths_to_restore ) {
                                    pending_image_t pending{};
                                    pending.id = id;
                                    pending.path = path;
									pending.ticket = 0; // internal reload; no waiter/completion
									m_pending_images.push_back( std::move( pending ) );
									++restored;
                                }
                            }
                        } else {
							utility::print( "device rebuild #%llu FAILED", (unsigned long long)rebuild_attempts );
                            Sleep( 100 );
                        }
                    }
                    else {
                        // Transient failures (UpdateLayeredWindow while
                        // the desktop is briefly unavailable, GetDC
                        // hiccup). Back off one frame and try again.
                        Sleep( 16 );
                    }
                    continue;
                }

				++frames_committed;
			}

			utility::print( "render worker EXITING: %s (frames_committed=%llu, gen=%llu, rendered_gen=%llu, end_frame_fails=%llu, rebuilds=%llu)",
				exit_reason,
				(unsigned long long)frames_committed,
				(unsigned long long)m_frame_generation,
				(unsigned long long)rendered_generation,
				(unsigned long long)end_frame_fails,
				(unsigned long long)rebuild_attempts );

            cancel_pending_fonts( );
            cancel_pending_images( );
            m_aurora_cache.Reset( );
            // scene3d holds D3D resources on the live device — tear
            // down before device.shutdown() so its ComPtrs release
            // against a still-alive parent.
            m_scene3d.shutdown( );
            renderer.shutdown( );
            device.shutdown( );
            window.destroy( );

            if ( com_initialized )
                CoUninitialize( );

            m_running.store( false );
            m_font_cv.notify_all( );
            m_image_cv.notify_all( );
            timeEndPeriod( 1 );
            return 0;
        }

        bool push( draw_item_t item ) {
            // Take by value + move-into vector. draw_item_t no longer
            // owns any heap-backed members (see comment on the text
            // fields above) — it's a POD-ish blob with a few unions of
            // fixed-size request structs, so the move is a pure memcpy
            // and push() is allocation-free even under text-heavy load.
            if ( t_in_batch_scope ) {
                m_items.push_back( std::move( item ) );
                return true;
            }
            std::lock_guard lock( m_mutex );
            m_items.push_back( std::move( item ) );
            return true;
        }

        void process_pending_fonts( c_device& device ) {
            std::vector< pending_font_t > pending;
            {
                std::lock_guard lock( m_font_mutex );
                pending.swap( m_pending_fonts );
            }

            if ( pending.empty( ) )
                return;

            {
                std::lock_guard lock( m_font_mutex );
                for ( auto& entry : pending ) {
                    const auto ok = device.register_font( entry.name, entry.path );
                    m_completed_fonts[ entry.id ] = ok;
                }
            }
            m_font_cv.notify_all( );
        }

        void cancel_pending_fonts( ) {
            {
                std::lock_guard lock( m_font_mutex );
                for ( auto& entry : m_pending_fonts )
                    m_completed_fonts[ entry.id ] = false;
                m_pending_fonts.clear( );
            }
            m_font_cv.notify_all( );
        }

        void process_pending_images( c_renderer& renderer ) {
            std::vector< pending_image_t > pending;
            {
                std::lock_guard lock( m_image_mutex );
                pending.swap( m_pending_images );
            }

            if ( pending.empty( ) )
                return;

            {
                std::lock_guard lock( m_image_mutex );
                for ( auto& entry : pending ) {
                    // idempotent: skip decode if id already resolved
                    if ( m_images.contains( entry.id ) ) {
						if ( entry.ticket )
							m_completed_images[ entry.ticket ] = true;
                        continue;
                    }

					auto bitmap = renderer.load_image( entry.path );
					const auto ok = static_cast< bool >( bitmap );
					if ( ok ) {
						m_images.emplace( entry.id, std::move( bitmap ) );
						m_registered_images.insert( entry.id );
						m_image_paths[ entry.id ] = entry.path;
					}
					if ( entry.ticket )
						m_completed_images[ entry.ticket ] = ok;
                }
            }
            m_image_cv.notify_all( );
        }

        void cancel_pending_images( ) {
            {
                std::lock_guard lock( m_image_mutex );
				for ( auto& entry : m_pending_images ) {
					if ( entry.ticket )
						m_completed_images[ entry.ticket ] = false;
				}
                m_pending_images.clear( );
            }
            m_image_cv.notify_all( );
        }

        void draw( c_renderer& renderer ) {
            std::vector< draw_item_t > items;
            color_t clear{};
            rect_t interactive_area{};
            auto interactive_area_dirty = false;
            {
                std::lock_guard lock( m_mutex );
                // O(1) swap instead of a deep copy: the previous
                // implementation heap-copied every draw_item_t (each
                // ~500 bytes with two std::wstring members) every
                // single frame, which added dozens of KB of alloc
                // per frame and multiplied by 60-240 Hz. draw() only
                // fires once per new frame_generation, so we don't
                // need m_last_complete to stay populated across
                // frames — the next present() fills it again.
                items.swap( m_last_complete );
                clear = m_clear;
                interactive_area = m_interactive_area;
                interactive_area_dirty = m_interactive_area_dirty;
                m_interactive_area_dirty = false;
            }

            if ( interactive_area_dirty )
                renderer.interactive_area( interactive_area );

            if ( m_minimize_requested.exchange( false ) )
                renderer.minimize_window( );

            if ( m_bring_to_top_requested.exchange( false ) )
                renderer.bring_to_top( );

            if ( clear.a > 0.0f )
                renderer.filled_rectangle(
                    { 0.0f, 0.0f, static_cast< float >( renderer.width( ) ), static_cast< float >( renderer.height( ) ) },
                    clear
                );

            for ( const auto& item : items ) {
                switch ( item.type ) {
                case draw_type_t::line:
                    renderer.line( convert( item.line.from ), convert( item.line.to ), convert( item.line.color ), item.line.thickness );
                    break;
                case draw_type_t::skeleton:
                    renderer.skeleton(
                        item.skeleton,
                        convert( item.skeleton.color ),
                        convert( item.skeleton.outline_color ) );
                    break;
                case draw_type_t::rectangle:
                    renderer.rectangle( convert( item.rectangle.area ), convert( item.rectangle.color ), item.rectangle.thickness );
                    break;
                case draw_type_t::filled_rectangle:
                    renderer.filled_rectangle( convert( item.rectangle.area ), convert( item.rectangle.color ) );
                    break;
                case draw_type_t::circle:
                    renderer.circle( convert( item.circle.center ), item.circle.radius, convert( item.circle.color ), item.circle.thickness );
                    break;
                case draw_type_t::filled_circle:
                    renderer.filled_circle( convert( item.circle.center ), item.circle.radius, convert( item.circle.color ) );
                    break;
                case draw_type_t::text: {
                    // Read strings straight out of the fixed-size wchar
                    // arrays in item.text (no per-frame std::wstring dup
                    // on the render side either). wcsnlen bounds the
                    // scan at the wire capacity so a producer that
                    // forgot the trailing null cannot walk past the
                    // struct.
                    const wchar_t* val_ptr = item.text.value;
                    const std::size_t val_len =
                        wcsnlen( val_ptr, communication::text_capacity );
                    const wchar_t* fnt_ptr =
                        item.text.font[ 0 ] ? item.text.font : L"Segoe UI";
                    const std::size_t fnt_len =
                        wcsnlen( fnt_ptr, communication::font_name_capacity );
                    renderer.text(
                        std::wstring_view( val_ptr, val_len ),
                        convert( item.text.area ),
                        item.text.size,
                        convert( item.text.color ),
                        convert( item.text.alignment ),
                        std::wstring_view( fnt_ptr, fnt_len ),
                        convert( item.text.weight ),
                        item.text.wrap
                    );
                    break;
                }
                case draw_type_t::image: {
                    const auto it = m_images.find( item.image.id );
                    if ( it == m_images.end( ) )
                        break;

                    const auto area = convert( item.image.area );
                    const auto rounded =
                        item.image.corner_radius > 0.5f &&
                        area.width  > 1.0f &&
                        area.height > 1.0f;
                    const auto clipped = rounded &&
                        renderer.push_rounded_clip( area, item.image.corner_radius );

                    // Tint dispatch:
                    //   • Default tint (white 255,255,255,any-alpha) →
                    //     plain DrawBitmap. Full source colours, opacity
                    //     modulates alpha as usual.
                    //   • Any other tint → FillOpacityMask silhouette.
                    //     The tint colour REPLACES the bitmap's RGB
                    //     entirely; opacity multiplies the tint alpha
                    //     so fade animations still work but no source
                    //     colour ever bleeds through.
                    const auto& t = item.image.tint;
                    if ( t.r == 255 && t.g == 255 && t.b == 255 ) {
                        renderer.draw_image( it->second, area,
                            item.image.opacity * ( t.a / 255.0f ) );
                    } else {
                        renderer.draw_image( it->second, area,
                            convert( t ), item.image.opacity );
                    }

                    if ( clipped )
                        renderer.pop_clip( );
                    break;
                }
                case draw_type_t::rounded_rectangle:
                    renderer.rounded_rectangle(
                        convert( item.rounded_rectangle.area ),
                        item.rounded_rectangle.radius,
                        convert( item.rounded_rectangle.color ),
                        item.rounded_rectangle.thickness );
                    break;
                case draw_type_t::filled_rounded_rectangle:
                    renderer.filled_rounded_rectangle(
                        convert( item.rounded_rectangle.area ),
                        item.rounded_rectangle.radius,
                        convert( item.rounded_rectangle.color ) );
                    break;
                case draw_type_t::aurora: {
                    const auto& req = item.aurora_item;

                    auto bake = [ & ]( ) {
                        std::vector< c_renderer::aurora_blob_t > blobs;
                        blobs.reserve( req.blob_count );
                        for ( std::uint32_t i = 0; i < req.blob_count && i < communication::max_aurora_blobs; ++i ) {
                            blobs.push_back( {
                                { req.blobs[ i ].x - req.area.x, req.blobs[ i ].y - req.area.y },
                                req.blobs[ i ].radius,
                                convert( req.blobs[ i ].color )
                            } );
                        }
                        return renderer.generate_aurora(
                            static_cast< int >( req.area.width ),
                            static_cast< int >( req.area.height ),
                            blobs, req.sigma, req.grain_strength );
                    };

                    if ( !m_aurora_cache )
                        m_aurora_cache = bake( );

                    if ( m_aurora_cache ) {
                        const auto area = convert( req.area );
                        const auto clipped = req.corner_radius > 0.5f &&
                            renderer.push_rounded_clip( area, req.corner_radius );
                        renderer.draw_image( m_aurora_cache, area );
                        if ( clipped )
                            renderer.pop_clip( );
                    }
                    break;
                }
                case draw_type_t::frosted_panel:
                    renderer.draw_frosted_panel(
                        convert( item.frosted.area ),
                        item.frosted.corner_radius,
                        item.frosted.sigma,
                        convert( item.frosted.tint ) );
                    break;
                case draw_type_t::push_aa_clip:
                    renderer.push_axis_aligned_clip( convert( item.clip_rect ) );
                    break;
                case draw_type_t::pop_aa_clip:
                    renderer.pop_axis_aligned_clip( );
                    break;
                case draw_type_t::gradient_filled_rectangle:
                    renderer.filled_gradient_rectangle(
                        convert( item.gradient_rect.area ),
                        convert( item.gradient_rect.color_a ),
                        convert( item.gradient_rect.color_b ),
                        item.gradient_rect.corner_radius );
                    break;
                case draw_type_t::gradient_filled_rectangle_h:
                    renderer.filled_gradient_rectangle_h(
                        convert( item.gradient_rect.area ),
                        convert( item.gradient_rect.color_a ),
                        convert( item.gradient_rect.color_b ),
                        item.gradient_rect.corner_radius );
                    break;
                case draw_type_t::scene3d: {
                    // Render mesh into scene3d's off-screen texture, then
                    // blit that texture into the D2D pass at `area`. The
                    // scene3d bitmap is a stable ID2D1Bitmap1 backed by a
                    // shared DXGI surface — DrawBitmap composites it as
                    // premultiplied-alpha exactly like the loaded images.
                    if ( !m_scene3d.bitmap( ) )
                        break;

                    const auto& req = item.scene3d;
                    color_t tint = convert( req.tint );
                    // Opacity multiplies the tint alpha so the whole
                    // silhouette can fade in/out; premultiplied-alpha
                    // scaling in the shader lifts opacity correctly
                    // across RGB too.
                    tint.a *= req.opacity;

                    if ( !m_scene3d.render_frame(
                            req.rotation_x, req.rotation_y, req.rotation_z, tint ) )
                        break;

                    renderer.draw_raw_bitmap(
                        m_scene3d.bitmap( ),
                        convert( req.area ),
                        1.0f );
                    break;
                }
                }
            }

        }

        static color_t convert( const communication::color_t& color ) {
            return color_t::from_rgba( color.r, color.g, color.b, color.a );
        }

        static point_t convert( const communication::point_t& point ) {
            return { point.x, point.y };
        }

        static rect_t convert( const communication::rect_t& rect ) {
            return { rect.x, rect.y, rect.width, rect.height };
        }

        static DWRITE_FONT_WEIGHT convert( communication::text_weight_t weight ) {
            return static_cast< DWRITE_FONT_WEIGHT >( static_cast< std::uint32_t >( weight ) );
        }

        static text_align_t convert( communication::text_align_t alignment ) {
            switch ( alignment ) {
            case communication::text_align_t::center:
                return text_align_t::center;
            case communication::text_align_t::right:
                return text_align_t::right;
            default:
                return text_align_t::left;
            }
        }
    };
}
