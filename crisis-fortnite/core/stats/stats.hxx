#pragma once

namespace stats {
    class c_stats {
    public:
        std::wstring m_api_key;


        ~c_stats( );

        void setup( );


        std::optional<br_player_stats_t> get_player(
            const std::string& display_name,
            const std::string& platform_name = "" );





        std::optional< player_brief_t > brief(
            const std::string& display_name );
        std::optional< std::uint32_t > lookup_kills(
            const std::string& display_name,
            mode_t mode = mode_t::overall );
        std::optional< std::uint32_t > lookup_deaths(
            const std::string& display_name,
            mode_t mode = mode_t::overall );
        std::optional< double > lookup_winrate(
            const std::string& display_name,
            mode_t mode = mode_t::overall );
        void invalidate_player( const std::string& display_name );
        bool download_weapons( );

        bool load_weapons( );
        static std::uint32_t icon_id_for( const std::string& path );
        std::vector<weapon_disk_t> inventory( );
        std::optional<double> weapon_damage( const std::string& name );
        std::optional<std::uint32_t> weapon_magazine_size(
            const std::string& name );
        std::uint32_t weapon_icon_id( const std::string& name );
        // fnv1a hash of an absolute icon path. same string always yields the
        // same id, so we can register once and reference by id anywhere.




        // 0 = no icon on disk / no match. otherwise the id passed to
        // bypass::overlay::image( ... ).

    private:
        using brief_map_t = std::unordered_map<std::string, player_brief_t>;

        std::shared_mutex m_player_mutex;
        std::unordered_map<std::string, br_player_stats_t> m_player_cache;

        std::shared_mutex m_weapon_mutex;
        std::unordered_map<std::string, weapon_disk_t> m_weapon_index;

        std::thread m_prime_thread;
        std::atomic<bool> m_running{ false };
		std::atomic<std::shared_ptr<const brief_map_t>> m_brief;

		static constexpr auto prime_sleep = ch::milliseconds( 100 );


        // target size we resample icons to. same size as we draw them at, so
        // runtime interpolation cost stays near zero. bump if you want the
        // stored asset to be usable at larger sizes.
        static constexpr UINT icon_target_size = 64;

        // strip anything that can't live in a windows filename. keeps
        // ascii letters/digits and folds spaces to '_'.
        void publish_brief(
            const std::string& display_name,
            const player_brief_t& entry );
        void prime_players( );
        static const br_mode_stats_t& mode_ref(
            const br_player_stats_t& player,
            mode_t mode );
        void rebuild_index_locked(
            const std::vector< weapon_disk_t >& records );
        static void register_weapon_icons(
            const std::vector< weapon_disk_t >& records );
        static const char* directory_path( );
        static const char* weapons_path( );
        static const char* weapons_html_path( );
        static const char* icons_directory_path( );
        static std::string sanitize_filename( const std::string& name );
        static bool save_resized_png(
            const std::string& source_bytes,
            const std::wstring& output_path );
        static std::string fetch_and_resize_icon(
            const std::string& weapon_name,
            const std::string& icon_source );
        static bool write_weapons(
            const std::vector< weapon_disk_t >& records );
        static bool read_weapons( std::vector< weapon_disk_t >& output );
        static std::string platform_to_account_type(
            const std::string& platform_name );
        std::optional< br_player_stats_t > fetch_player(
            const std::string& display_name,
            const std::string& platform_name = "" );
        static std::string download_weapons_html( );
        static void save_weapons_html( const std::string& html );
        std::vector< weapon_info_t > fetch_weapons( );
        static br_mode_stats_t parse_mode_stats(
            const std::string& block );
        static std::optional< br_player_stats_t > parse_player_stats(
            const std::string& body );
        static std::string extract_td(
            const std::string& row,
            std::size_t& position );
        static std::vector< weapon_info_t > parse_weapons_page(
            const std::string& html );
    };
}
