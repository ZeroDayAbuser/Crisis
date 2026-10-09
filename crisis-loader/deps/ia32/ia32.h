#pragma once
#pragma once

#define image_dos_signature     0x5A4D
#define image_nt_signature      0x4550
#define image_nt_optional_hdr32	0x10B
#define image_nt_optional_hdr64	0x20B

#define image_dir_export		0
#define image_dir_import		1
#define image_dir_resource		2
#define image_dir_exception		3
#define image_dir_security		4
#define image_dir_relocs		5
#define image_dir_debug			6
#define image_dir_arch			7
#define image_dir_globalptr		8
#define image_dir_tls			9
#define image_dir_load_config	10
#define image_dir_bound_import	11
#define image_dir_iat			12
#define image_dir_delay_import	13
#define image_dir_com_desc		14
#define image_dir_entries		16

#define image_reloc_absolute	0
#define image_reloc_high		1
#define image_reloc_low			2
#define image_reloc_highlow		3
#define image_reloc_highadj		4
#define image_reloc_specific_5	5
#define image_reloc_reserved	6
#define image_reloc_specific_7	7
#define image_reloc_specific_8	8
#define image_reloc_specific_9	9
#define image_reloc_dir64		10
#define image_reloc_offset(x)	(x & 0xFFF)

struct export_directory_t {
    std::int32_t m_characteristics;
    std::int32_t m_time_date_stamp;
    std::int16_t m_major_version;
    std::int16_t m_minor_version;
    std::int32_t m_name;
    std::int32_t m_base;
    std::int32_t m_number_of_functions;
    std::int32_t m_number_of_names;
    std::int32_t m_address_of_functions;
    std::int32_t m_address_of_names;
    std::int32_t m_address_of_names_ordinals;
};

typedef union _virt_addr_t {
    std::uintptr_t value;
    struct {
        std::uint64_t offset : 12;        // 0:11
        std::uint64_t pte_index : 9;      // 12:20
        std::uint64_t pde_index : 9;      // 21:29
        std::uint64_t pdpte_index : 9;    // 30:38
        std::uint64_t pml4e_index : 9;    // 39:47
        std::uint64_t reserved : 16;      // 48:63
    };
    struct {
        std::uint64_t offset_4kb : 12;    // 4KB page offset
        std::uint64_t pt_offset : 9;
        std::uint64_t pd_offset : 9;
        std::uint64_t pdpt_offset : 9;
        std::uint64_t pml4_offset : 9;
        std::uint64_t reserved2 : 16;
    };
    struct {
        std::uint64_t offset_2mb : 21;    // 2MB page offset
        std::uint64_t pd_offset2 : 9;
        std::uint64_t pdpt_offset2 : 9;
        std::uint64_t pml4_offset2 : 9;
        std::uint64_t reserved3 : 16;
    };
    struct {
        std::uint64_t offset_1gb : 30;    // 1GB page offset
        std::uint64_t pdpt_offset3 : 9;
        std::uint64_t pml4_offset3 : 9;
        std::uint64_t reserved4 : 16;
    };
} virt_addr_t, * pvirt_addr_t;

typedef union _pml4e {
    std::uint64_t value;
    struct {
        std::uint64_t present : 1;                   // Must be 1 if valid
        std::uint64_t read_write : 1;               // Write access control
        std::uint64_t user_supervisor : 1;           // User/supervisor access control
        std::uint64_t page_write_through : 1;        // Write-through caching
        std::uint64_t cached_disable : 1;            // Cache disable
        std::uint64_t accessed : 1;                  // Set when accessed
        std::uint64_t ignored0 : 1;                  // Ignored
        std::uint64_t large_page : 1;               // Reserved (must be 0)
        std::uint64_t ignored1 : 4;                 // Ignored
        std::uint64_t pfn : 36;                     // Physical frame number
        std::uint64_t reserved : 4;                 // Reserved for software
        std::uint64_t ignored2 : 11;                // Ignored
        std::uint64_t no_execute : 1;               // No-execute bit
    } hard;
} pml4e, * ppml4e;

typedef union _pdpte {
    std::uint64_t value;
    struct {
        std::uint64_t present : 1;                   // Must be 1 if valid
        std::uint64_t read_write : 1;               // Write access control
        std::uint64_t user_supervisor : 1;           // User/supervisor access control
        std::uint64_t page_write_through : 1;        // Write-through caching
        std::uint64_t cached_disable : 1;            // Cache disable
        std::uint64_t accessed : 1;                  // Set when accessed
        std::uint64_t dirty : 1;                    // Set when written to (1GB pages)
        std::uint64_t page_size : 1;                // 1=1GB page, 0=points to page directory
        std::uint64_t ignored1 : 4;                 // Ignored
        std::uint64_t pfn : 36;                     // Physical frame number
        std::uint64_t reserved : 4;                 // Reserved for software
        std::uint64_t ignored2 : 11;                // Ignored
        std::uint64_t no_execute : 1;               // No-execute bit
    } hard;
} pdpte, * ppdpte;

typedef union _pde {
    std::uint64_t value;
    struct {
        std::uint64_t present : 1;                   // Must be 1 if valid
        std::uint64_t read_write : 1;               // Write access control
        std::uint64_t user_supervisor : 1;           // User/supervisor access control
        std::uint64_t page_write_through : 1;        // Write-through caching
        std::uint64_t cached_disable : 1;            // Cache disable
        std::uint64_t accessed : 1;                  // Set when accessed
        std::uint64_t dirty : 1;                    // Set when written to (2MB pages)
        std::uint64_t page_size : 1;                // 1=2MB page, 0=points to page table
        std::uint64_t global : 1;                   // Global page (if CR4.PGE=1)
        std::uint64_t ignored1 : 3;                 // Ignored
        std::uint64_t pfn : 36;                     // Physical frame number
        std::uint64_t reserved : 4;                 // Reserved for software
        std::uint64_t ignored2 : 11;                // Ignored
        std::uint64_t no_execute : 1;               // No-execute bit
    } hard;
} pde, * ppde;

typedef union _pte {
    std::uint64_t value;
    struct {
        std::uint64_t present : 1;                   // Must be 1 if valid
        std::uint64_t read_write : 1;               // Write access control
        std::uint64_t user_supervisor : 1;           // User/supervisor access control
        std::uint64_t page_write_through : 1;        // Write-through caching
        std::uint64_t cached_disable : 1;            // Cache disable
        std::uint64_t accessed : 1;                  // Set when accessed
        std::uint64_t dirty : 1;                    // Set when written to
        std::uint64_t pat : 1;                      // Page Attribute Table bit
        std::uint64_t global : 1;                   // Global page
        std::uint64_t ignored1 : 3;                 // Ignored
        std::uint64_t pfn : 36;                     // Physical frame number
        std::uint64_t reserved : 4;                 // Reserved for software
        std::uint64_t ignored2 : 7;                 // Ignored
        std::uint64_t protection_key : 4;           // Protection key
        std::uint64_t no_execute : 1;               // No-execute bit
    } hard;
} pte, * ppte;

typedef union _cr3 {
    std::uint64_t flags;

    struct {
        std::uint64_t pcid : 12;
        std::uint64_t pfn : 36;
        std::uint64_t reserved_1 : 12;
        std::uint64_t reserved_2 : 3;
        std::uint64_t pcid_invalidate : 1;
    };
} cr3, * pcr3;

struct rtl_process_module_information_t {
    HANDLE  m_section;
    void* m_mapped_base;
    void* m_image_base;
    uint32_t m_image_size;
    uint32_t m_flags;
    uint16_t m_load_order_index;
    uint16_t m_init_order_index;
    uint16_t m_load_count;
    uint16_t m_offset_to_file_name;
    uint8_t  m_full_path[ 256 ];
};

struct rtl_process_modules_t {
    uint32_t    m_count;
    rtl_process_module_information_t m_modules[ 1 ];
};

struct unicode_string_t {
    std::uint16_t m_length;
    std::uint16_t m_maximum_length;
    wchar_t* m_buffer;
};

struct list_entry_t {
    list_entry_t* m_flink;
    list_entry_t* m_blink;
};

struct balanced_links_t {
    void* m_parent;             // 0x00
    void* m_left;               // 0x08
    void* m_right;              // 0x10
    std::uint8_t m_balance;     // 0x18
    std::uint8_t m_reserved[ 3 ]; // 0x19-0x1B
    std::uint32_t m_pad;        // 0x1C
}; // size: 0x20

struct avl_table_t {
    balanced_links_t m_balanced_root;    // 0x00-0x20
    void* m_ordered_pointer;             // 0x20
    std::uint32_t m_which_ordered_element; // 0x28
    std::uint32_t m_number_generic_table_elements; // 0x2C
    std::uint32_t m_depth_of_tree;       // 0x30
    std::uint32_t m_pad1;                // 0x34
    void* m_restart_key;                 // 0x38
    std::uint32_t m_delete_count;        // 0x40
    std::uint32_t m_pad2;                // 0x44
    void* m_compare_routine;             // 0x48
    void* m_allocate_routine;            // 0x50
    void* m_free_routine;                // 0x58
    void* m_table_context;               // 0x60
}; // size: 0x68

struct piddb_cache_entry_t {
    list_entry_t m_list;           // 0x00-0x10
    unicode_string_t m_driver_name;  // 0x10-0x1C
    std::uint32_t m_timestamp;     // 0x20
    std::int32_t m_load_status;    // 0x24
    std::uint8_t m_shim_data[ 16 ];// 0x28-0x38
}; // size: 0x38

struct _DEVICE_OBJECT {
    std::uint16_t m_type;
    std::uint16_t m_size;
    std::uint32_t m_reference_count;
    long          m_busy;
    void*         m_driver_object;
    void*         m_next_device;
    void*         m_attached_device;
    void*         m_vpb;
    void*         m_device_object;
};

struct _DRIVER_OBJECT {
    std::uint16_t m_type;
    std::uint16_t m_size;
    void*         m_device_object;
    void*         m_driver_start;
    std::uint32_t m_driver_size;
    void*         m_driver_section;
    void*         m_driver_extension;
    void*         m_driver_name;
    void*         mhardware_database;
    void*         mfast_io_dispatch;
    void*         mdriver_init;
    void*         mdriver_start_io;
    void*         mdriver_unload;
    void*         mmajor_function[ 0x28 ];
};

struct _KLDR_DATA_TABLE_ENTRY {
    list_entry_t m_in_load_order_links;
    void* m_exception_table;
    std::uint32_t m_exception_table_size;
    void* m_ntdll_base;
    void* m_check_sum;
    std::uint32_t m_maybe1;
    void* m_section_base;
    void* mmapped_view;
    std::uint16_t m_image_flags;
    std::uint16_t m_load_order_index;
    std::uint16_t m_init_order_index;
    std::uint16_t m_load_count;
    std::uint16_t m_base_name_offset;
    std::uint16_t m_load_reason;
    void* m_module_handle;
    list_entry_t m_entry;
    std::uint32_t m_maybe2;
    void* md_dll_base;
    void* mcheck_sum;
    std::uint32_t m_maybe3;
    std::uint32_t m_time_date_stamp;
    unicode_string_t m_full_dll_name;
    unicode_string_t m_base_dll_name;
    std::uint8_t m_load_reason_pad[ 0x20 ];
};

enum pe_magic_t {
    dos_header = 0x5a4d,
    nt_headers = 0x4550,
    opt_header = 0x020b
};

struct data_directory_t {
    std::int32_t m_virtual_address;
    std::int32_t m_size;

    template< class type_t >

    type_t as_rva(
        std::uint64_t rva
    ) {
        return reinterpret_cast< type_t >( rva + m_virtual_address );
    }
};

struct dos_header_t {
    std::int16_t m_magic;
    std::int16_t m_cblp;
    std::int16_t m_cp;
    std::int16_t m_crlc;
    std::int16_t m_cparhdr;
    std::int16_t m_minalloc;
    std::int16_t m_maxalloc;
    std::int16_t m_ss;
    std::int16_t m_sp;
    std::int16_t m_csum;
    std::int16_t m_ip;
    std::int16_t m_cs;
    std::int16_t m_lfarlc;
    std::int16_t m_ovno;
    std::int16_t m_res0[ 0x4 ];
    std::int16_t m_oemid;
    std::int16_t m_oeminfo;
    std::int16_t m_res1[ 0xa ];
    std::int32_t m_lfanew;

    constexpr bool is_valid( ) {
        return m_magic == pe_magic_t::dos_header;
    }
};

struct nt_headers_t {
    std::int32_t m_signature;
    std::int16_t m_machine;
    std::int16_t m_number_of_sections;
    std::int32_t m_time_date_stamp;
    std::int32_t m_pointer_to_symbol_table;
    std::int32_t m_number_of_symbols;
    std::int16_t m_size_of_optional_header;
    std::int16_t m_characteristics;

    std::int16_t m_magic;
    std::int8_t m_major_linker_version;
    std::int8_t m_minor_linker_version;
    std::int32_t m_size_of_code;
    std::int32_t m_size_of_initialized_data;
    std::int32_t m_size_of_uninitialized_data;
    std::int32_t m_address_of_entry_point;
    std::int32_t m_base_of_code;
    std::uint64_t m_image_base;
    std::int32_t m_section_alignment;
    std::int32_t m_file_alignment;
    std::int16_t m_major_operating_system_version;
    std::int16_t m_minor_operating_system_version;
    std::int16_t m_major_image_version;
    std::int16_t m_minor_image_version;
    std::int16_t m_major_subsystem_version;
    std::int16_t m_minor_subsystem_version;
    std::int32_t m_win32_version_value;
    std::int32_t m_size_of_image;
    std::int32_t m_size_of_headers;
    std::int32_t m_check_sum;
    std::int16_t m_subsystem;
    std::int16_t m_dll_characteristics;
    std::uint64_t m_size_of_stack_reserve;
    std::uint64_t m_size_of_stack_commit;
    std::uint64_t m_size_of_heap_reserve;
    std::uint64_t m_size_of_heap_commit;
    std::int32_t m_loader_flags;
    std::int32_t m_number_of_rva_and_sizes;

    data_directory_t m_export_table;
    data_directory_t m_import_table;
    data_directory_t m_resource_table;
    data_directory_t m_exception_table;
    data_directory_t m_certificate_table;
    data_directory_t m_base_relocation_table;
    data_directory_t m_debug;
    data_directory_t m_architecture;
    data_directory_t m_global_ptr;
    data_directory_t m_tls_table;
    data_directory_t m_load_config_table;
    data_directory_t m_bound_import;
    data_directory_t m_iat;
    data_directory_t m_delay_import_descriptor;
    data_directory_t m_clr_runtime_header;
    data_directory_t m_reserved;

    constexpr bool is_valid( ) {
        return m_signature == pe_magic_t::nt_headers
            && m_magic == pe_magic_t::opt_header;
    }
};

#pragma pack(push, 1)
struct system_handle_table_entry_info_ex_t {
    void* m_object;
    std::uint64_t m_unique_process_id;
    std::uint64_t m_handle_value;
    std::uint32_t m_granted_access;
    std::uint16_t m_creator_back_trace_index;
    std::uint16_t m_object_type_index;
    std::uint32_t m_handle_attributes;
    std::uint32_t m_reserved;
};

struct system_handle_information_ex_t {
    std::uint64_t m_number_of_handles;
    std::uint64_t m_reserved;
    system_handle_table_entry_info_ex_t m_handles[ 1 ];
};
#pragma pack(pop)


struct hash_bucket_entry_t {
    list_entry_t* m_flink;
    unicode_string_t m_driver_name;
};


union ps_protection_t {
    uint8_t level;
    struct {
        uint8_t type : 3;
        uint8_t audit : 1;
        uint8_t signer : 4;
    };
};

enum ps_protected_type : uint8_t {
    ps_protected_none = 0,
    ps_protected_light = 1,
    ps_protected_full = 2,
};

enum ps_protected_signer : uint8_t {
    ps_signer_none = 0,
    ps_signer_authenticode = 1,
    ps_signer_code_gen = 2,
    ps_signer_antimalware = 3,
    ps_signer_lsa = 4,
    ps_signer_windows = 5,
    ps_signer_win_tcb = 6,
    ps_signer_win_system = 7,
};

struct section_header_t {
    char m_name[ 0x8 ];
    union {
        std::int32_t m_physical_address;
        std::int32_t m_virtual_size;
    };
    std::int32_t m_virtual_address;
    std::int32_t m_size_of_raw_data;
    std::int32_t m_pointer_to_raw_data;
    std::int32_t m_pointer_to_relocations;
    std::int32_t m_pointer_to_line_numbers;
    std::int16_t m_number_of_relocations;
    std::int16_t m_number_of_line_numbers;
    std::int32_t m_characteristics;
};

struct mi_active_pfn_t {
    std::uint64_t m_page_frame : 40;
    std::uint64_t m_priority : 8;
    std::uint64_t m_color : 16;
};

struct mmpte_hardware_t {
    std::uint64_t m_valid : 1;
    std::uint64_t m_write : 1;
    std::uint64_t m_owner : 1;
    std::uint64_t m_write_through : 1;
    std::uint64_t m_cache_disable : 1;
    std::uint64_t m_accessed : 1;
    std::uint64_t m_dirty : 1;
    std::uint64_t m_large_page : 1;
    std::uint64_t m_global : 1;
    std::uint64_t m_copy_on_write : 1;
    std::uint64_t m_prototype : 1;
    std::uint64_t m_reserved0 : 1;
    std::uint64_t m_page_frame_number : 36;
    std::uint64_t m_reserved1 : 4;
    std::uint64_t m_software_ws_index : 11;
    std::uint64_t m_no_execute : 1;
};

struct mmpte_t {
    union {
        mmpte_hardware_t m_hard;
        std::uint64_t m_value;
    };
};

struct mipfnblink_t {
    union {
        std::uint64_t m_blink : 40;
        std::uint64_t m_type_size : 24;
    };
};

struct mi_pfn_ulong5_t {
    union {
        struct {
            std::uint32_t m_modified_write_count : 16;
            std::uint32_t m_shared_count : 16;
        };
        std::uint32_t m_entire_field;
    };
};

struct mmpfnentry1_t {
    std::uint8_t m_page_color : 6;
    std::uint8_t m_modified : 1;
    std::uint8_t m_read_in_progress : 1;
};

struct mmpfnentry3_t {
    std::uint8_t priority : 3;
    std::uint8_t on_protected_standby : 1;
    std::uint8_t in_page_error : 1;
    std::uint8_t system_charged_page : 1;
    std::uint8_t removal_requested : 1;
    std::uint8_t parity_error : 1;
};

typedef struct rtl_balanced_node_t {
    union {
        struct rtl_balanced_node_t* m_children[ 2 ];
        struct {
            struct rtl_balanced_node_t* m_left;
            struct rtl_balanced_node_t* m_right;
        };
    };

#define RTL_BALANCED_NODE_RESERVED_PARENT_MASK 3

    union {
        std::uint8_t m_red : 1;
        std::uint8_t m_balance : 2;
        std::uint64_t m_parent_value;
    };
};


struct single_list_entry_t {
    single_list_entry_t* m_next;
};

struct mmpfn_t {
    union {
        list_entry_t m_list_entry;
        rtl_balanced_node_t m_tree_node;
        struct {
            union {
                union {
                    single_list_entry_t m_next_slist_pfn;
                    void* m_next;
                    struct {
                        std::uint64_t m_flink : 40;
                        std::uint64_t m_node_flink_low : 24;
                    };
                    mi_active_pfn_t m_active;
                } m_u1;

                union {
                    mmpte_t* m_pte_address;
                    std::uint64_t m_pte_long;
                };

                mmpte_t m_original_pte;
            };
        };
    };

    mipfnblink_t m_u2;

    union {
        union {
            struct {
                std::uint16_t m_reference_count;
                mmpfnentry1_t m_e1;
                mmpfnentry3_t m_e3;
            };
            struct {
                std::uint16_t m_reference_count2;
            } m_e2;
            struct {
                std::uint32_t m_entire_field;
            } m_e4;
        };
    } m_u3;

    mi_pfn_ulong5_t m_u5;

    union {
        union {
            struct {
                std::uint64_t m_pte_frame : 40;
                std::uint64_t m_resident_page : 1;
                std::uint64_t m_unused1 : 1;
                std::uint64_t m_unused2 : 1;
                std::uint64_t m_partition : 10;
                std::uint64_t m_file_only : 1;
                std::uint64_t m_pfn_exists : 1;
                std::uint64_t m_node_flink_high : 5;
                std::uint64_t m_page_identity : 3;
                std::uint64_t m_prototype_pte : 1;
            };
            std::uint64_t m_entire_field;
        };
    } m_u4;
};

union ularge_integer_t {
    struct {
        std::uint32_t  m_low_part;                                                      //0x0
        std::uint32_t  m_high_part;                                                     //0x4
    };
    struct {
        std::uint32_t  m_low_part;                                                      //0x0
        std::uint32_t  m_high_part;                                                     //0x4
    } u;                                                                    //0x0
    std::uint64_t m_quad_part;                                                     //0x0
};

struct physical_memory_range_t {
    ularge_integer_t m_base_page;
    ularge_integer_t m_page_count;
}; // Size: 0x010

#pragma pack(push, 1)
struct system_build_version_information_t {
    USHORT m_layer_index;
    USHORT m_layer_version_count;
    ULONG  m_field_04;
    ULONG  m_field_08;
    ULONG  m_field_0c;
    ULONG  m_field_10;
    CHAR   m_string1[ 128 ];
    CHAR   m_string2[ 128 ];
    CHAR   m_string3[ 128 ];
    CHAR   m_string4[ 128 ];
    CHAR   m_string5[ 26 ];
    CHAR   m_string6[ 16 ];
    BYTE   m_padding[ 2 ];
    ULONG  m_field_240;
};

struct unicode_string64_t {
    USHORT  m_length;
    USHORT  m_maximum_length;
    ULONG   m_padding;
    ULONG64 m_buffer;
};

struct fake_version_struct_t {
    BYTE               m_header[ 0x10 ];
    unicode_string64_t m_us1;
    unicode_string64_t m_us5;
    unicode_string64_t m_us6;
    unicode_string64_t m_us2;
    unicode_string64_t m_us3;
    unicode_string64_t m_us4;
    BYTE               m_pad[ 0x320 - 0x70 ];
    ULONG              m_field_320;
};
#pragma pack(pop)

typedef struct {

    uint32_t characts;
    uint32_t time_stamp_date;
    uint16_t major_ver;
    uint16_t minor_ver;
    uint32_t name;
    uint32_t base;
    uint32_t funcs_num;
    uint32_t names_num;
    uint32_t funcs_addr;
    uint32_t names_addr;
    uint32_t ordinals_addr;

} image_export_dir_t;

typedef struct {

    union {

        uint32_t characts;
        uint32_t original;

    } thunk;

    uint32_t time_date_stamp;
    uint32_t chain;
    uint32_t name;
    uint32_t first_thunk;

} image_import_desc_t;

typedef struct {

    union {
        uint64_t m_forwarder_string;
        uint64_t m_function;
        uint64_t m_ordinal;
        uint64_t m_address_of_data;
    } m_u1;

} image_thunk_data_t;

typedef struct {

    uint16_t	m_hint;
    char		m_name[ 1 ];

} image_import_name_t;

typedef struct {

    uint32_t va, size;

} image_base_reloc_t;


struct import_descriptor_t {
    union {
        std::uint32_t m_characteristics;
        std::uint32_t m_original_first_thunk;
    };
    std::uint32_t m_time_date_stamp;
    std::uint32_t m_forwarder_chain;
    std::uint32_t m_name;
    std::uint32_t m_first_thunk;
};

struct list_entry64_t {
    std::uint64_t m_flink;
    std::uint64_t m_blink;
};

struct peb64_t {
    std::uint8_t m_reserved[ 0x18 ];
    std::uint64_t m_ldr;
};

struct peb_ldr_data64_t {
    std::uint8_t m_reserved[ 0x10 ];
    list_entry64_t m_in_load_order_module_list;
};

struct ldr_data_table_entry64_t {
    list_entry64_t m_in_load_order_links;
    list_entry64_t m_in_memory_order_links;
    list_entry64_t m_in_initialization_order_links;
    std::uint64_t m_dll_base;
    std::uint64_t m_entry_point;
    std::uint32_t m_size_of_image;
    std::uint32_t m_padding;
    unicode_string64_t m_full_dll_name;
    unicode_string64_t m_base_dll_name;
};

static_assert( offsetof( peb64_t, m_ldr ) == 0x18 );
static_assert( offsetof( peb_ldr_data64_t,
    m_in_load_order_module_list ) == 0x10 );
static_assert( offsetof( ldr_data_table_entry64_t, m_dll_base ) == 0x30 );
static_assert( offsetof( ldr_data_table_entry64_t, m_base_dll_name ) == 0x58 );

struct kapc_state_t {
    list_entry_t m_apc_list_head[ 2 ];
    void* m_process;
    std::uint8_t m_kernel_apc_in_progress;
    std::uint8_t m_kernel_apc_pending;
    std::uint8_t m_user_apc_pending;
    std::uint8_t m_pad;
}; // Size: 0x40


struct image_delayload_descriptor_t {
    union {
        std::uint32_t all_attributes;
        struct {
            std::uint32_t m_rva_based : 1;
            std::uint32_t m_reverved_attributes : 1;
        };
    };

    std::uint32_t m_dll_name_rva;
    std::uint32_t m_module_handle_rva;
    std::uint32_t m_import_address_table_rva;
    std::uint32_t m_import_name_table_rva;
    std::uint32_t m_bound_import_address_table_rva;
    std::uint32_t m_unload_information_table_rva;
    std::uint32_t m_time_date_stamp;
};


typedef struct {

    uint16_t machine, number_of_sections;
    uint32_t time_stamp;
    uint32_t symbol_table, number_of_symbols;
    uint16_t optional_head_sz, characts;

} nt_image_file_head_t;

typedef struct {

    uint8_t name[ 8 ];

    union {

        uint32_t physical;
        uint32_t size;

    } misc;

    uint32_t va;
    uint32_t raw_size, raw_data;
    uint32_t relocs_data;
    uint32_t linenumbers;
    uint16_t relocs_cnt;
    uint16_t linenumbers_cnt;
    uint32_t characts;

} nt_image_section_head_t;

typedef struct {

    uint32_t va, size;

} nt_image_data_dir_t;

typedef struct {

    uint16_t	magic;
    uint8_t     major_linker_ver;
    uint8_t     minor_linker_ver;
    uint32_t    size_of_code;
    uint32_t    size_of_init_data;
    uint32_t    size_of_uninit_data;
    uint32_t    entry_point;
    uint32_t    base_of_code;
    uint64_t    image_base;
    uint32_t    section_align;
    uint32_t    file_align;
    uint16_t    major_os_ver;
    uint16_t    minor_os_ver;
    uint16_t    major_image_ver;
    uint16_t    minor_image_ver;
    uint16_t    major_subsystem_ver;
    uint16_t    minor_subsystem_ver;
    uint32_t    win32_ver;
    uint32_t    size_of_image;
    uint32_t    size_of_headers;
    uint32_t    checksum;
    uint16_t    subsystem;
    uint16_t    dll_characts;
    uint64_t    size_of_stack_reserve;
    uint64_t    size_of_stack_commit;
    uint64_t    size_of_heap_reserve;
    uint64_t    size_of_heap_commit;
    uint32_t    loader_flags;
    uint32_t    number_of_rva_and_sizes;

    nt_image_data_dir_t data_directory[ image_dir_entries ];

} nt_image_optional_head_t;

typedef struct {

    uint16_t magic;
    uint16_t last_page_bytes;
    uint16_t pages_cnt;
    uint16_t relocs;
    uint16_t header_sz;
    uint16_t min_alloc;
    uint16_t max_alloc;
    uint16_t ss;
    uint16_t sp;
    uint16_t checksum;
    uint16_t ip;
    uint16_t cs;
    uint16_t lfarlc;
    uint16_t overlays;
    uint16_t reserved[ 4 ];
    uint16_t oem_id;
    uint16_t oem_info;
    uint16_t reserved2[ 10 ];
    int32_t  lfanew;

} nt_image_dos_head_t;

typedef struct {

    uint32_t characts;
    uint32_t time_stamp_date;
    uint16_t major_ver;
    uint16_t minor_ver;
    uint32_t name;
    uint32_t base;
    uint32_t funcs_num;
    uint32_t names_num;
    uint32_t funcs_addr;
    uint32_t names_addr;
    uint32_t ordinals_addr;

} nt_image_export_dir_t;

typedef struct {

    union {

        uint32_t characts;
        uint32_t original;

    } thunk;

    uint32_t time_date_stamp;
    uint32_t chain;
    uint32_t name;
    uint32_t first_thunk;

} nt_image_import_desc_t;

typedef struct {

    union {
        uint64_t forwarder_string;
        uint64_t function;
        uint64_t ordinal;
        uint64_t address_of_data;
    } u1;

} nt_image_thunk_data_t;

typedef struct {

    uint16_t	hint;
    char		name[ 1 ];

} nt_image_import_name_t;

struct rtl_critical_section_t {
    void* m_debug_info;
    std::int32_t m_lock_count;
    std::int32_t m_recursion_count;
    void* m_owning_thread;
    void* m_lock_semaphore;
    std::uint32_t m_spin_count;
};

struct ldr_data_table_entry_t {
    list_entry_t m_in_load_order_module_list;
    list_entry_t m_in_memory_order_module_list;
    list_entry_t m_in_initialization_order_module_list;
    void* m_dll_base;
    void* m_entry_point;
    std::uint32_t m_size_of_image;
    unicode_string_t m_full_dll_name;
    unicode_string_t m_base_dll_name;
    std::uint32_t m_flags;
    std::uint16_t m_load_count;
    std::uint16_t m_tls_index;
    list_entry_t m_hash_links;
    void* m_section_pointer;
    std::uint32_t m_check_sum;
    std::uint32_t m_time_date_stamp;
};

struct peb_ldr_data_t {
    std::uint32_t m_length;
    bool m_initialized;
    void* m_ss_handle;
    list_entry_t m_module_list_load_order;
    list_entry_t m_module_list_memory_order;
    list_entry_t m_module_list_in_it_order;
};

struct peb_t {
    std::uint8_t m_inherited_address_space;
    std::uint8_t m_read_image_file_exec_options;
    std::uint8_t m_being_debugged;
    std::uint8_t m_bit_field;

    struct {
        std::uint32_t m_image_uses_large_pages : 1;
        std::uint32_t m_is_protected_process : 1;
        std::uint32_t m_is_legacy_process : 1;
        std::uint32_t m_is_image_dynamically_relocated : 1;
        std::uint32_t m_spare_bits : 4;
    };

    void* m_mutant;
    void* m_image_base_address;
    peb_ldr_data_t* m_ldr;
    void* m_process_parameters;
    void* m_subsystem_data;
    void* m_process_heap;
    rtl_critical_section_t* m_fast_peb_lock;
    void* m_atl_thunk_slist_ptr;
    void* m_ifeo_key;

    struct {
        std::uint32_t m_process_in_job : 1;
        std::uint32_t m_process_initializing : 1;
        std::uint32_t m_reserved_bits0 : 30;
    } m_cross_process_flags;

    union {
        void* m_kernel_callback_table;
        void* m_user_shared_info_ptr;
    };

    std::uint32_t m_system_reserved[ 1 ];
    std::uint32_t m_spare_ulong;
    void* m_free_list;
    std::uint32_t m_tls_expansion_counter;
    void* m_tls_bitmap;
    std::uint32_t m_tls_bitmap_bits[ 2 ];
    void* m_read_only_shared_memory_base;
    void* m_hotpatch_information;
    void** m_read_only_static_server_data;
    void* m_ansi_code_page_data;
    void* m_oem_code_page_data;
    void* m_unicode_case_table_data;
    std::uint32_t m_number_of_processors;
    std::uint32_t m_nt_global_flag;
    std::int64_t m_critical_section_timeout;
    std::uint32_t m_heap_segment_reserve;
    std::uint32_t m_heap_segment_commit;
    std::uint32_t m_heap_decomit_total_free_threshold;
    std::uint32_t m_heap_decomit_free_block_threshold;
    std::uint32_t m_number_of_heaps;
    std::uint32_t m_maximum_number_of_heaps;
    void** m_process_heaps;
    void* m_gdi_shared_handle_table;
    void* m_process_starter_helper;
    std::uint32_t m_gdi_dc_attribute_list;
    rtl_critical_section_t* m_loader_lock;
    std::uint32_t m_os_major_version;
    std::uint32_t m_os_minor_version;
    std::uint16_t m_os_build_number;
    std::uint16_t m_os_csd_version;
    std::uint32_t m_os_platform_id;
    std::uint32_t m_image_subsystem;
    std::uint32_t m_image_subsystem_major_version;
    std::uint32_t m_image_subsystem_minor_version;
    std::uint32_t m_image_process_affinity_mask;
    std::uint32_t m_gdi_handle_buffer[ 34 ];
    void* m_post_process_init_routine;
    void* m_tls_expansion_bitmap;
    std::uint32_t m_tls_expansion_bitmap_bits[ 32 ];
    std::uint32_t m_session_id;
    std::uint64_t m_app_compat_flags;
    std::uint64_t m_app_compat_flags_user;
    void* m_p_shim_data;
    void* m_app_compat_info;
    unicode_string_t m_csd_version;
    void* m_activation_context_data;
    void* m_process_assembly_storage_map;
    void* m_system_default_activation_context_data;
    void* m_system_assembly_storage_map;
    std::uint32_t m_minimum_stack_commit;
    void* m_fls_callback;
    list_entry_t m_fls_list_head;
    void* m_fls_bitmap;
    std::uint32_t m_fls_bitmap_bits[ 4 ];
    std::uint32_t m_fls_high_index;
    void* m_wer_registration_data;
    void* m_wer_ship_assert_ptr;
};