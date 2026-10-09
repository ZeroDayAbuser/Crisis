#pragma once

namespace player_stats {
    struct icon_resolve_request_t {
        std::uint64_t          player_key;
        std::string            weapon_name;
        engine::a_fort_weapon* weapon_actor{ nullptr };
    };

    class c_player_stats_thread {
    public:





        ~c_player_stats_thread( );
        void start( );
        void stop( );
        void update_player_list(
            const std::vector< cache::c_player_pawn >& players );
        void push_icon_request( icon_resolve_request_t request );
    private:
        std::atomic<bool> m_running{ false };
        std::thread       m_thread;

        std::mutex                        m_snapshot_mutex;
        std::vector<cache::c_player_pawn> m_snapshot;
        std::chrono::steady_clock::time_point m_last_snapshot_update{};

        std::mutex                          m_icon_queue_mutex;
        std::vector<icon_resolve_request_t> m_icon_queue;

		static constexpr auto loop_period = ch::milliseconds( 100 );
        static constexpr auto brief_period = ch::seconds( 1 );
        static constexpr auto weapon_period = ch::milliseconds( 500 );




        void run( );
        static void publish(
            std::uint64_t player_key,
            std::shared_ptr< player_stats_cache_t > entry );
        static void prune_locked(
            const std::unordered_set< std::uint64_t >& active_keys );
        template <typename T>
        static void prune_local(
            std::unordered_map<std::uint64_t, T>& map,
            const std::unordered_set<std::uint64_t>& active_keys ) {
            for ( auto it = map.begin( ); it != map.end( ); )
                it = active_keys.count( it->first ) ? std::next( it ) : map.erase( it );
        }

        static void resolve_icon( const icon_resolve_request_t& req );
        static void prune_inventories( const std::unordered_set<std::uint64_t>& active_keys );
    };
}
