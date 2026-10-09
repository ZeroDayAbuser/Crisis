#pragma once

namespace thread {

    enum status_t : std::uint32_t {
        status_idle         = 0,
        status_initializing = 1,
        status_executing    = 2,
        status_complete     = 3
    };

    struct exec_context_t {
        volatile std::uint32_t m_status { status_idle };
        std::uint32_t          m_pad    { 0 };
        std::uint64_t          m_result { 0 };
    };

    struct thread_context_t {
        CONTEXT m_context  { };
        bool    m_executed { false };
        bool    m_result   { false };
    };

    inline std::vector<std::uint8_t> build_shellcode(
        std::uint64_t entry_point,
        std::uint64_t first_arg,
        std::uint64_t exec_fort_va
    ) {
        const auto flags_va  = exec_fort_va + offsetof( exec_context_t, m_status );
        const auto result_va = exec_fort_va + offsetof( exec_context_t, m_result );

        std::vector<std::uint8_t> sc;
        sc.reserve( 128 );

        auto emit_u64 = [&]( std::uint64_t v ) {
            for ( int i = 0; i < 8; i++ ) sc.push_back( std::uint8_t( v >> ( i * 8 ) ) );
        };

        auto write_status = [&]( std::uint32_t s ) {
            sc.push_back( 0x48 ); sc.push_back( 0xB8 ); emit_u64( flags_va );
            sc.push_back( 0xC7 ); sc.push_back( 0x00 );
            sc.push_back( std::uint8_t( s ) ); sc.push_back( 0 ); sc.push_back( 0 ); sc.push_back( 0 );
        };

        sc.push_back( 0x50 );
        sc.push_back( 0x51 );
        sc.push_back( 0x52 );
        sc.push_back( 0x41 ); sc.push_back( 0x50 );
        sc.push_back( 0x41 ); sc.push_back( 0x51 );
        sc.push_back( 0x41 ); sc.push_back( 0x52 );
        sc.push_back( 0x41 ); sc.push_back( 0x53 );

        sc.push_back( 0x48 ); sc.push_back( 0x89 ); sc.push_back( 0xE5 );
        sc.push_back( 0x48 ); sc.push_back( 0x83 ); sc.push_back( 0xE4 ); sc.push_back( 0xF0 );
        sc.push_back( 0x48 ); sc.push_back( 0x83 ); sc.push_back( 0xEC ); sc.push_back( 0x20 );

        write_status( status_initializing );

        sc.push_back( 0x48 ); sc.push_back( 0xB9 ); emit_u64( first_arg );
        sc.push_back( 0xBA ); sc.push_back( 0x01 ); sc.push_back( 0 ); sc.push_back( 0 ); sc.push_back( 0 );
        sc.push_back( 0x45 ); sc.push_back( 0x31 ); sc.push_back( 0xC0 );

        write_status( status_executing );

        sc.push_back( 0x48 ); sc.push_back( 0xB8 ); emit_u64( entry_point );
        sc.push_back( 0xFF ); sc.push_back( 0xD0 );

        sc.push_back( 0x48 ); sc.push_back( 0xB9 ); emit_u64( result_va );
        sc.push_back( 0x48 ); sc.push_back( 0x89 ); sc.push_back( 0x01 );

        write_status( status_complete );

        sc.push_back( 0xEB ); sc.push_back( 0xFE );

        return sc;
    }
}
