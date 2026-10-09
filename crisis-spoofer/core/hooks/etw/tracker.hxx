#pragma once

namespace etw {
    static constexpr std::uint32_t g_index_cap  = 0x1000u;
    static constexpr std::uint32_t g_seen_longs = g_index_cap / 32u;
    static constexpr std::uint64_t g_dump_delay = 60ULL * 60ULL * 10'000'000ULL;

    static std::uint32_t*  m_indices    = nullptr;
    static volatile long*  m_seen       = nullptr;
    static volatile long   m_count      = 0;
    static volatile long   m_active     = 0;
    static volatile long   m_dumped     = 0;
    static std::uint64_t   m_start_time = 0;

    void setup( ) {
        m_indices = kernel::mm_allocate_independent_pages< std::uint32_t* >(
            g_index_cap * sizeof( std::uint32_t ) );

        m_seen = kernel::mm_allocate_independent_pages< volatile long* >(
            paging::page_4kb_size );

        if ( !m_indices || !m_seen )
            return;

        RtlZeroMemory( const_cast< long* >( m_seen ), paging::page_4kb_size );

        LARGE_INTEGER t{};
        KeQuerySystemTime( &t );
        m_start_time = static_cast< std::uint64_t >( t.QuadPart );
        _InterlockedExchange( &m_active, 1 );
    }

    void dump_to_disk( ) {
        const auto n = static_cast< std::uint32_t >( m_count );
        disk::print( obf( "recoreded %u systems calls:" ), n );
        for ( std::uint32_t idx = 0; idx < n; ++idx )
            disk::print( obf( " -> 0x%04x" ), m_indices[ idx ] );
		disk::print( obf( "finished recording!" ) );
    }

    void track( std::uint32_t index ) {
        if ( !m_active )
            return;

        if ( etw::get_hook( index ) != static_cast< std::size_t >( -1 ) )
            return;

        LARGE_INTEGER t{};
        KeQuerySystemTime( &t );
        if ( static_cast< std::uint64_t >( t.QuadPart ) - m_start_time >= g_dump_delay ) {
            if ( !_InterlockedCompareExchange( &m_dumped, 1, 0 ) ) {
                _InterlockedExchange( &m_active, 0 );
                dump_to_disk( );
            }
            return;
        }

        const auto long_idx = index >> 5u;
        const auto long_bit = 1L << ( index & 31u );

        if ( m_seen[ long_idx ] & long_bit )
            return;

        if ( _InterlockedOr( m_seen + long_idx, long_bit ) & long_bit )
            return;

        const auto slot = _InterlockedIncrement( &m_count ) - 1L;
        if ( slot < static_cast< long >( g_index_cap ) )
            m_indices[ static_cast< std::uint32_t >( slot ) ] = index;
    }
}
