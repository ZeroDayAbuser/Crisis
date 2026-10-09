#pragma once

namespace spoofer {
    struct entry_t {
        std::uint64_t       m_image_base;
        std::uint64_t       m_image_size;
        std::uint64_t       m_ntoskrnl_base;
        std::uint64_t       m_directory_table_base;
        const char*         m_product_name;
        const std::uint8_t* m_pdb_blob;
        std::uint32_t       m_pdb_blob_size;
        std::uint32_t       m_pad;
    };
}
