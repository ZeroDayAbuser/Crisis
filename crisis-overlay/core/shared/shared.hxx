#pragma once

extern render::c_render* g_render;

namespace communication {
    namespace commands {
        inline bool dispatch( const command_t& command ) {
            if ( !g_render )
                return false;

            switch ( command.id ) {
			case command_id::render_clear:
				g_render->clear( );
				return true;
			case command_id::render_present:
				g_render->present( );
				return true;
            case command_id::render_clear_color:
                if ( command.size < sizeof( color_t ) || !command.buffer )
                    return false;
                g_render->clear_color( *reinterpret_cast< color_t* >( command.buffer ) );
                return true;
            case command_id::render_line:
                if ( command.size < sizeof( line_request_t ) || !command.buffer )
                    return false;
                return g_render->line( *reinterpret_cast< line_request_t* >( command.buffer ) );
            case command_id::render_rectangle:
                if ( command.size < sizeof( rectangle_request_t ) || !command.buffer )
                    return false;
                return g_render->rectangle( *reinterpret_cast< rectangle_request_t* >( command.buffer ), false );
            case command_id::render_filled_rectangle:
                if ( command.size < sizeof( rectangle_request_t ) || !command.buffer )
                    return false;
                return g_render->rectangle( *reinterpret_cast< rectangle_request_t* >( command.buffer ), true );
            case command_id::render_circle:
                if ( command.size < sizeof( circle_request_t ) || !command.buffer )
                    return false;
                return g_render->circle( *reinterpret_cast< circle_request_t* >( command.buffer ), false );
            case command_id::render_filled_circle:
                if ( command.size < sizeof( circle_request_t ) || !command.buffer )
                    return false;
                return g_render->circle( *reinterpret_cast< circle_request_t* >( command.buffer ), true );
            case command_id::render_text:
                if ( command.size < sizeof( text_request_t ) || !command.buffer )
                    return false;
                return g_render->text( *reinterpret_cast< text_request_t* >( command.buffer ) );
            case command_id::render_interactive_area:
                if ( command.size < sizeof( rect_t ) || !command.buffer )
                    return false;
                g_render->interactive_area( *reinterpret_cast< rect_t* >( command.buffer ) );
                return true;
            case command_id::render_minimize:
                g_render->minimize( );
                return true;
            case command_id::render_register_font:
                if ( command.size < sizeof( font_register_request_t ) || !command.buffer )
                    return false;
                return g_render->register_font( *reinterpret_cast< font_register_request_t* >( command.buffer ) );
            case command_id::render_bring_to_top:
                g_render->bring_to_top( );
                return true;
            case command_id::render_set_monitor:
                if ( command.size < sizeof( set_monitor_request_t ) || !command.buffer )
                    return false;
                return g_render->set_monitor(
                    reinterpret_cast< set_monitor_request_t* >( command.buffer )->index
                );
            case command_id::render_get_size:
                // Writes directly into command.buffer, which is packet.payload
                // in shared memory -- the client reads it back from the same
                // packet after request() sees packet_complete.
                if ( command.size < sizeof( size_response_t ) || !command.buffer )
                    return false;
                {
                    auto& response = *reinterpret_cast< size_response_t* >( command.buffer );
                    response.width = g_render->width( );
                    response.height = g_render->height( );
                }
                return true;
            case command_id::render_register_image:
                if ( command.size < sizeof( image_register_request_t ) || !command.buffer )
                    return false;
                return g_render->register_image( *reinterpret_cast< image_register_request_t* >( command.buffer ) );
            case command_id::render_image:
                if ( command.size < sizeof( image_request_t ) || !command.buffer )
                    return false;
                return g_render->image( *reinterpret_cast< image_request_t* >( command.buffer ) );
            case command_id::render_rounded_rectangle:
                if ( command.size < sizeof( rounded_rectangle_request_t ) || !command.buffer )
                    return false;
                return g_render->rounded_rectangle( *reinterpret_cast< rounded_rectangle_request_t* >( command.buffer ), false );
            case command_id::render_filled_rounded_rectangle:
                if ( command.size < sizeof( rounded_rectangle_request_t ) || !command.buffer )
                    return false;
                return g_render->rounded_rectangle( *reinterpret_cast< rounded_rectangle_request_t* >( command.buffer ), true );
            case command_id::render_aurora:
                if ( command.size < sizeof( aurora_request_t ) || !command.buffer )
                    return false;
                return g_render->aurora( *reinterpret_cast< aurora_request_t* >( command.buffer ) );
            case command_id::render_frosted_panel:
                if ( command.size < sizeof( frosted_panel_request_t ) || !command.buffer )
                    return false;
                return g_render->frosted_panel( *reinterpret_cast< frosted_panel_request_t* >( command.buffer ) );
            case command_id::render_skeleton:
                if ( command.size < sizeof( skeleton_request_t ) || !command.buffer )
                    return false;
                return g_render->skeleton( *reinterpret_cast< skeleton_request_t* >( command.buffer ) );
            case command_id::render_push_clip:
                if ( command.size < sizeof( rect_t ) || !command.buffer )
                    return false;
                return g_render->push_clip( *reinterpret_cast< rect_t* >( command.buffer ) );
            case command_id::render_pop_clip:
                g_render->pop_clip_aa( );
                return true;
            case command_id::render_gradient_rectangle:
                if ( command.size < sizeof( gradient_rectangle_request_t ) || !command.buffer )
                    return false;
                return g_render->gradient_rectangle(
                    *reinterpret_cast< gradient_rectangle_request_t* >( command.buffer ), false );
            case command_id::render_gradient_rectangle_h:
                if ( command.size < sizeof( gradient_rectangle_request_t ) || !command.buffer )
                    return false;
                return g_render->gradient_rectangle(
                    *reinterpret_cast< gradient_rectangle_request_t* >( command.buffer ), true );
            case command_id::render_ping:
                // Handshake reply — producer sends this once at startup to
                // verify this consumer recognises the new command_id set.
                // Trivial success so packet.result becomes 1.
                return true;
            case command_id::render_scene3d:
                if ( command.size < sizeof( scene3d_request_t ) || !command.buffer )
                    return false;
                return g_render->scene3d( *reinterpret_cast< scene3d_request_t* >( command.buffer ) );
            case command_id::render_batch: {
                if ( !command.buffer || !command.size )
                    return true;
                const auto* base   = static_cast< const std::uint8_t* >( command.buffer );
                const auto  total  = command.size;
                std::size_t cursor = 0;
                std::size_t record_count = 0;
                std::size_t fail_count   = 0;
                std::uint32_t first_fail_cmd  = 0;
                std::uint32_t first_fail_size = 0;
                bool all_ok = true;
                bool saw_present = false;

                // Hold the render mutex ONCE for the whole batch. push()
                // used to lock/unlock per item — 200-300 lock ops per
                // frame under packet pressure, contending with the render
                // worker. With the scope active the batch takes it once
                // and every sub-dispatch reuses it via recursive_mutex.
                // The scope is scoped-out cleanly via the guard below.
                g_render->begin_batch_scope( );
                struct batch_guard_t {
                    ~batch_guard_t( ) { g_render->end_batch_scope( ); }
                } batch_guard;

                while ( cursor + sizeof( batch_record_t ) <= total ) {
                    batch_record_t hdr {};
                    std::memcpy( &hdr, base + cursor, sizeof( hdr ) );
                    cursor += sizeof( batch_record_t );
                    if ( cursor + hdr.size > total ) {
                        static bool logged = false;
                        if ( !logged ) {
                            logged = true;
                            utility::print( "batch: TRUNCATED record cmd=%u size=%u cursor=%zu total=%zu record_count=%zu",
                                (unsigned)hdr.command, (unsigned)hdr.size,
                                cursor, (size_t)total, record_count );
                        }
                        return false;
                    }
                    void* sub_buffer = hdr.size
                        ? const_cast< std::uint8_t* >( base + cursor )
                        : nullptr;
                    if ( static_cast< command_id >( hdr.command ) == command_id::render_present )
                        saw_present = true;
                    const bool ok = dispatch( {
                        static_cast< command_id >( hdr.command ),
                        sub_buffer,
                        static_cast< std::size_t >( hdr.size )
                    } );
                    if ( !ok ) {
                        if ( !fail_count ) { first_fail_cmd = hdr.command; first_fail_size = hdr.size; }
                        ++fail_count;
                    }
                    all_ok = all_ok && ok;
                    cursor += hdr.size;
                    ++record_count;
                }

                // Only log a batch if a sub-dispatch actually failed —
                // batching itself is confirmed working, and per-batch
                // stats otherwise just spam the log.
                if ( fail_count ) {
                    static std::uint64_t failed_batches = 0;
                    ++failed_batches;
                    if ( failed_batches == 1 || failed_batches % 300 == 0 )
                        utility::print( "batch failed #%llu: total=%zu records=%zu fails=%zu first_fail=(cmd=%u,size=%u)",
                            (unsigned long long)failed_batches, (size_t)total, record_count,
                            fail_count, (unsigned)first_fail_cmd, (unsigned)first_fail_size );
                }
                return all_ok;
            }
            default:
                return false;
            }
        }
    }

    }

#include <core/shared/communication.hxx>
