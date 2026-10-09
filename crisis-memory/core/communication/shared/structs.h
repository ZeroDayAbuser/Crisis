#pragma once

namespace communication {
    constexpr std::uint32_t capacity = transport::k_capacity;
    constexpr std::uint32_t capacity_mask = transport::k_capacity_mask;
    constexpr std::size_t packet_payload_size = transport::k_payload;
    constexpr std::uint64_t max_mapping_ranges = 64;

    enum class command_id : std::uint32_t {
        none,
        map_memory,
        read_memory,
        write_memory,
        query_mapping
    };

    struct mapping_range_t {
        std::uint64_t base_pa{};
        std::uint64_t size{};
        std::uint64_t va_base{};
    };

    struct mapping_info_t {
        std::uint64_t physical_mask{};
        std::uint64_t max_ranges{};
        std::uint64_t range_count{};
        mapping_range_t ranges[ max_mapping_ranges ]{};
    };

    struct command_t {
        command_id id{ command_id::none };
        std::uint64_t addr{ };
        void* buffer{ };
        std::size_t size{ };
    };

    using packet_t = transport::packet_t;
    using shared_state_t = transport::section_t;
}
