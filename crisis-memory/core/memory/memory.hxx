#pragma once

namespace memory {
    struct range_t {
        std::uint64_t base_pa{};
        std::uint64_t size{};
        std::uint64_t va_base{};
    };

    struct request_t {
        std::uint64_t base_pa{};
        std::uint64_t size{};
    };

    class c_mapped_memory {
    public:
        static constexpr std::size_t max_ranges = 64;

    private:
        struct alignas( 64 ) range_cache_t {
            std::uint64_t base_pa{};
            std::uint64_t end_pa{};
            std::uint64_t va_base{};
        };

        alignas( 64 ) range_cache_t m_range_cache[ max_ranges ]{};
        std::size_t   m_range_count{};
        std::uint64_t m_physical_mask{};

        __forceinline auto lookup( std::uint64_t pa, std::size_t size ) const noexcept -> std::uint64_t {
            if ( !size || pa > UINT64_MAX - ( size - 1 ) ) return 0;
            if ( pa & ~m_physical_mask ) return 0;
            const auto pa_end = pa + size - 1;
            if ( pa_end & ~m_physical_mask ) return 0;

            for ( std::size_t i = 0; i < m_range_count; ++i ) {
                const auto& r = m_range_cache[ i ];
                if ( pa >= r.base_pa && pa_end < r.end_pa )
                    return r.va_base + ( pa - r.base_pa );
            }
            return 0;
        }

    public:
        auto setup( std::uint64_t physical_mask ) -> void {
            m_physical_mask = physical_mask;
            m_range_count = 0;
            for ( auto& r : m_range_cache ) r = {};
        }

        auto map_range( request_t req, std::uint64_t va_base ) -> bool {
            if ( m_range_count >= max_ranges ) return false;
            if ( !req.size || !va_base ) return false;
            if ( req.base_pa > UINT64_MAX - req.size ) return false;
            if ( va_base > UINT64_MAX - req.size ) return false;
            if ( req.base_pa & ~m_physical_mask ) return false;

            auto& slot = m_range_cache[ m_range_count ];
            slot.base_pa = req.base_pa;
            slot.end_pa = req.base_pa + req.size;
            slot.va_base = va_base;

            std::atomic_thread_fence( std::memory_order_release );
            ++m_range_count;
            return true;
        }

        auto unmap_range( std::uint64_t base_pa ) -> bool {
            for ( std::size_t i = 0; i < m_range_count; ++i ) {
                if ( m_range_cache[ i ].base_pa != base_pa ) continue;

                for ( auto j = i; j + 1 < m_range_count; ++j )
                    m_range_cache[ j ] = m_range_cache[ j + 1 ];

                m_range_cache[ --m_range_count ] = {};
                std::atomic_thread_fence( std::memory_order_release );
                return true;
            }
            return false;
        }

        auto has_range( std::uint64_t base_pa ) const noexcept -> bool {
            for ( std::size_t i = 0; i < m_range_count; ++i )
                if ( m_range_cache[ i ].base_pa == base_pa ) return true;
            return false;
        }

        auto range_count( ) const noexcept -> std::size_t { return m_range_count; }
        auto physical_mask( ) const noexcept -> std::uint64_t { return m_physical_mask; }
        auto range_at( std::size_t i ) const noexcept -> range_t {
            if ( i >= m_range_count ) return {};

            const auto& range = m_range_cache[ i ];
            return {
                range.base_pa,
                range.end_pa - range.base_pa,
                range.va_base
            };
        }

        template<typename type_t>
        __forceinline auto read( std::uint64_t pa ) const noexcept -> type_t {
            const auto va = lookup( pa, sizeof( type_t ) );
            if ( !va ) return type_t{};
            type_t result{};
            __try { result = *reinterpret_cast< const volatile type_t* >( va ); }
            __except ( EXCEPTION_EXECUTE_HANDLER ) { }
            return result;
        }

        template<typename type_t>
        __forceinline auto write( std::uint64_t pa, const type_t& value ) const noexcept -> bool {
            const auto va = lookup( pa, sizeof( type_t ) );
            if ( !va ) return false;
            bool ok = false;
            __try { *reinterpret_cast< volatile type_t* >( va ) = value; ok = true; }
            __except ( EXCEPTION_EXECUTE_HANDLER ) { }
            return ok;
        }

        template<typename type_t>
        __forceinline auto read( std::uint64_t pa, type_t* buf, std::size_t size ) const noexcept -> bool {
            const auto va = lookup( pa, size );
            if ( !va ) return false;
            bool ok = false;
            __try { std::memcpy( buf, reinterpret_cast< const void* >( va ), size ); ok = true; }
            __except ( EXCEPTION_EXECUTE_HANDLER ) { }
            return ok;
        }

        template<typename type_t>
        __forceinline auto write( std::uint64_t pa, const type_t* buf, std::size_t size ) const noexcept -> bool {
            const auto va = lookup( pa, size );
            if ( !va ) return false;
            bool ok = false;
            __try { std::memcpy( reinterpret_cast< void* >( va ), buf, size ); ok = true; }
            __except ( EXCEPTION_EXECUTE_HANDLER ) { }
            return ok;
        }
    };
}
