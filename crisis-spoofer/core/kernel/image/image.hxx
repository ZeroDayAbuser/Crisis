#pragma once

namespace image {
    std::uint64_t m_image_base{};
    std::uint64_t m_image_size{};

    bool contains( void* address ) {
        if ( !m_image_base || !m_image_size )
            return false;

        return ( address >= reinterpret_cast< void* >( m_image_base ) ) &&
            ( address < reinterpret_cast< void* >( m_image_base + m_image_size ) );
    }

    namespace callback {
        bool remove_debug_callback( ) {
            auto rtlp_debug_print_callback_list = kernel::get_rtlp_debug_print_callback_list( );
            if ( !rtlp_debug_print_callback_list ) {
                disk::print( obf( "could not find debug callback list" ) );
                return false;
            }

            auto debug_print_callback = rtlp_debug_print_callback_list->m_flink;
            while ( debug_print_callback != rtlp_debug_print_callback_list ) {
                auto debug_print_callback_next = debug_print_callback->m_flink;
                auto debug_print_callback_entry = CONTAINING_RECORD( debug_print_callback,
                    rtlp_debug_print_entry_t, m_list_entry );

                auto callback = debug_print_callback_entry->m_callback;
                if ( image::contains( callback ) ) {
                    debug_print_callback->m_blink->m_flink = debug_print_callback->m_flink;
                    debug_print_callback->m_flink->m_blink = debug_print_callback->m_blink;

                    disk::print( obf( "removed debug callback -> 0x%llx" ), callback );
                    return true;
                }

                debug_print_callback = debug_print_callback_next;
            }

            return false;
        }

        void __stdcall load_image_callback( unicode_string_t* image_name, void* process_id, image_info_t* image_info ) {
            remove_debug_callback( );

            auto file_name = std::wcsrchr( image_name->m_buffer, obf( L'\\' ) );
            if ( file_name )
                file_name++;
            else
                file_name = image_name->m_buffer;

            if ( !std::wcscmp( file_name, obf( L"EasyAntiCheat_EOS.sys" ) ) ) {
                m_image_base = reinterpret_cast< std::uint64_t >( image_info->m_image_base );
                m_image_size = image_info->m_image_size;

                disk::print( obf( "easyanticheat_eos.sys -> 0x%llx, size -> 0x%x" ),
                    m_image_base, m_image_size );

#if instr_emulator
                exception::emulator::add( m_image_base, m_image_size );
                exception::emulator::arm_from_load( m_image_base );
#endif
            }
        }

        bool create( ) {
            if ( !kernel::ps_set_load_image_notify_routine(
                reinterpret_cast< void* >( load_image_callback ) ) ) {
                disk::print( obf( "failed to register callback" ) );
                return false;
            }

            disk::print( obf( "registered callback -> 0x%llx" ), load_image_callback );
            return true;
        }

        bool unregister( ) {
            kernel::ps_remove_load_image_notify_routine(
                reinterpret_cast< void* >( load_image_callback ) );
            return true;
        }
    }
}