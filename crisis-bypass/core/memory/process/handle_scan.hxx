#pragma once

namespace handle_scan {

    struct entry_t {
        std::uint64_t m_pid;
        std::uint64_t m_handle;
        std::uint32_t m_access;
        bool          m_is_kernel;
    };

    inline std::vector<entry_t> m_results;

#pragma pack(push, 8)
    struct sys_handle_entry_t {
        void*          m_object;
        std::uint64_t  m_pid;
        std::uint64_t  m_handle;
        std::uint32_t  m_access;
        std::uint16_t  m_bt_idx;
        std::uint16_t  m_type_idx;
        std::uint32_t  m_attrs;
        std::uint32_t  m_pad;
    };

    struct sys_handle_info_t {
        std::uint64_t      m_count;
        std::uint64_t      m_reserved;
        sys_handle_entry_t m_entries[1];
    };
#pragma pack(pop)

    static constexpr std::uint32_t k_info_class_ext_handles = 0x40u;
    static constexpr std::uint32_t k_attr_kernel_handle     = 0x0200u;
    static constexpr std::uint32_t k_min_access             = 0x003Au;

    void scan( std::uint64_t target_eprocess ) {
        m_results.clear( );
        if ( !target_eprocess )
            return;

        using nt_qsi_t = NTSTATUS( NTAPI* )( ULONG, PVOID, ULONG, PULONG );
        static const auto fn = reinterpret_cast<nt_qsi_t>(
            GetProcAddress( GetModuleHandleA( obf( "ntdll.dll" ) ),
                            obf( "NtQuerySystemInformation" ) ) );
        if ( !fn )
            return;

        ULONG required = 0;
        fn( k_info_class_ext_handles, nullptr, 0, &required );
        required = ( required ? required : 0x100000u ) + 0x20000u;

        auto buf = std::make_unique<std::uint8_t[]>( required );
        ULONG written = 0;
        if ( fn( k_info_class_ext_handles, buf.get( ), required, &written ) < 0 )
            return;

        const auto* info   = reinterpret_cast<const sys_handle_info_t*>( buf.get( ) );
        const auto  target = reinterpret_cast<void*>( target_eprocess );

        for ( std::uint64_t i = 0; i < info->m_count; ++i ) {
            const auto& e = info->m_entries[ i ];
            if ( e.m_object != target )
                continue;
            if ( ( e.m_access & k_min_access ) != k_min_access &&
                  e.m_access != 0x001FFFFFu )
                continue;
            entry_t h{};
            h.m_pid       = e.m_pid;
            h.m_handle    = e.m_handle;
            h.m_access    = e.m_access;
            h.m_is_kernel = ( e.m_attrs & k_attr_kernel_handle ) != 0;
            m_results.push_back( h );
        }

        std::stable_sort( m_results.begin( ), m_results.end( ),
            []( const entry_t& a, const entry_t& b ) {
                auto score = []( const entry_t& e ) -> std::uint32_t {
                    std::uint32_t s = 0;
                    if ( e.m_access == 0x001FFFFFu ) s += 4;
                    else if ( ( e.m_access & k_min_access ) == k_min_access ) s += 2;
                    if ( !e.m_is_kernel ) s += 1;
                    return s;
                };
                return score( a ) > score( b );
            } );

        logging::print( obf( "handle_scan: %zu candidate(s) to game process" ),
            m_results.size( ) );
    }
}
