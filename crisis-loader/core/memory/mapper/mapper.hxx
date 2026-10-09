#pragma once

namespace mapper {
    struct reloc_entry_t {
        std::uint32_t m_to_rva;
        std::uint32_t m_size;
        struct {
            std::uint16_t m_offset : 12;
            std::uint16_t m_type : 4;
        } m_item[ 1 ];
    };

    class c_mapper {
    public:
        c_mapper( ) = default;
        c_mapper( const std::string& image_path ) {
            load_image( image_path );
        }

        c_mapper( std::vector<std::uint8_t> image ) : m_dependency( std::move( image ) ) {
            initialize_image( );
        }

        bool map( ) {
            if ( !m_nt_headers )
                return false;

            const auto image_size = static_cast< std::size_t >( m_nt_headers->m_size_of_image );
            m_mapped_base = g_cow->allocate( image_size );
            if ( !m_mapped_base )
                return false;

            m_mapped_image.assign( image_size, 0 );

            const auto headers_size = static_cast< std::size_t >( m_nt_headers->m_size_of_headers );
            if ( headers_size > m_dependency.size( ) || headers_size > m_mapped_image.size( ) ) {
                logging::print( obf( "invalid image headers size" ) );
                g_cow->destroy( m_mapped_base );
                m_mapped_base = 0;
                return false;
            }

            std::memcpy( m_mapped_image.data( ), m_dependency.data( ), headers_size );

            if ( !map_sections( ) ) {
                logging::print( obf( "could not map sections" ) );
                g_cow->destroy( m_mapped_base );
                m_mapped_base = 0;
                return false;
            }

            if ( !relocate( ) ) {
                logging::print( obf( "could not map relocations" ) );
                g_cow->destroy( m_mapped_base );
                m_mapped_base = 0;
                return false;
            }

            if ( !map_delayed_imports( ) ) {
                g_cow->destroy( m_mapped_base );
                m_mapped_base = 0;
                return false;
            }

            if ( !map_imports( ) ) {
                g_cow->destroy( m_mapped_base );
                m_mapped_base = 0;
                return false;
            }

            g_cow->write( m_mapped_base, m_mapped_image.data( ), m_mapped_image.size( ) );

            logging::print( obf( "injected module: 0x%llx (0x%x)" ), m_mapped_base, image_size );
            return true;
        }

		bool launch( std::string product_name ) {
			if ( !m_mapped_base || !g_cow )
				return false;

			auto pdb_blob = g_pdb->compress_to_memory( );
			if ( pdb_blob.empty( ) )
				return false;

			const auto blob_pages_va = kernel::mm_allocate_independent_pages( pdb_blob.size( ) );
			if ( !blob_pages_va ) {
				g_cow->destroy( m_mapped_base );
				m_mapped_base = 0;
				return false;
			}
			kernel::memcpy( blob_pages_va, pdb_blob.data( ), pdb_blob.size( ) );

			spoofer::entry_t entry { };
			entry.m_product_name         = product_name.c_str( );
			entry.m_image_base           = m_mapped_base;
			entry.m_image_size           = m_mapped_image.size( );
			entry.m_ntoskrnl_base        = g_pdb->m_module_base;
			entry.m_directory_table_base = g_paging->m_directory_table_base;
			entry.m_pdb_blob             = reinterpret_cast< const std::uint8_t* >( blob_pages_va );
			entry.m_pdb_blob_size        = static_cast< std::uint32_t >( pdb_blob.size( ) );

			auto entry_pointer = g_cow->allocate( sizeof( entry ) );
			if ( !entry_pointer ) {
				g_cow->destroy( m_mapped_base );
				m_mapped_base = 0;
				return false;
			}

			g_cow->write( entry_pointer, &entry, sizeof( entry ) );

			g_cow->protect( m_mapped_base, m_mapped_image.size( ),
				paging::e_pt_protection::read_write_execute );
			kernel::flush_tlb_all( );

			const auto entry_point = get_entry_point( );
			if ( !entry_point ) {
				g_cow->destroy( entry_pointer );
				g_cow->destroy( m_mapped_base );
				m_mapped_base = 0;
				return false;
			}

			const auto result = g_syscall->call_kernel< bool >( entry_point, entry_pointer );
			if ( !result ) {
				logging::print( obf( "failed to open spoofer module." ) );
				logging::print( obf( "please contact support or restart your computer.\n" ) );
				g_cow->destroy( entry_pointer );
				g_cow->destroy( m_mapped_base );
				m_mapped_base = 0;
				return false;
			}

			std::vector< BYTE > empty_buffer( 0x1000 );
			if ( empty_buffer.empty( ) )
				return false;

			g_cow->write( m_mapped_base, empty_buffer.data( ), 0x1000 );
			return true;
		}

        bool cleanup( ) {
            if ( m_mapped_image.empty( ) )
                return true;

            std::fill(
                m_mapped_image.begin( ),
                m_mapped_image.end( ),
                0
            );

            m_mapped_image.clear( );

            kernel::memcpy(
                reinterpret_cast< void* >( m_mapped_base ),
                m_mapped_image.data( ),
                m_mapped_image.size( )
            );

            return true;
        }

    private:
        nt_headers_t* m_nt_headers{ nullptr };
        std::uint64_t m_mapped_base{ };
        std::vector<std::uint8_t> m_dependency{ };
        std::vector<std::uint8_t> m_mapped_image{ };

        std::uint64_t get_entry_point( ) const {
            if ( !m_nt_headers )
                return 0;

            const auto entry_rva = static_cast< std::uint32_t >( m_nt_headers->m_address_of_entry_point );
            if ( !entry_rva )
                return 0;

            return m_mapped_base + entry_rva;
        }

        bool relocate( ) {
            const auto delta_offset = m_mapped_base - get_image_base( );
            if ( !delta_offset )
                return true;

            if ( is_reloc_stripped( ) ) {
                logging::print( obf( "dependency relocations stripped" ) );
                return false;
            }

            auto reloc_dir = get_reloc_directory( );
            if ( !reloc_dir || !reloc_dir->m_virtual_address || !reloc_dir->m_size ) {
                logging::print( obf( "could not find relocation directory" ) );
                return false;
            }

            auto reloc_entry = mapped_rva_to_va< reloc_entry_t* >(
                static_cast< std::uint32_t >( reloc_dir->m_virtual_address )
            );
            if ( !reloc_entry ) {
                logging::print( obf( "could not find relocation entry" ) );
                return false;
            }

            const auto reloc_end = reinterpret_cast< reloc_entry_t* >(
                reinterpret_cast< std::uint8_t* >( reloc_entry ) + reloc_dir->m_size
                );

            while ( reloc_entry < reloc_end && reloc_entry->m_size ) {
                const auto record_count = ( reloc_entry->m_size - 8 ) >> 1;
                for ( auto idx = 0u; idx < record_count; idx++ ) {
                    const std::uint16_t offset = reloc_entry->m_item[ idx ].m_offset;
                    const std::uint16_t type = reloc_entry->m_item[ idx ].m_type;

                    if ( type == image_reloc_absolute )
                        continue;

                    if ( type != image_reloc_dir64 ) {
                        logging::print( obf( "unsupported relocation type: %u" ), type );
                        return false;
                    }

                    auto reloc_addr = mapped_rva_to_va< std::uint8_t* >( reloc_entry->m_to_rva );
                    if ( !reloc_addr )
                        return false;

                    auto reloc_va = reinterpret_cast< std::uint64_t* >( reloc_addr + offset );
                    *reloc_va += delta_offset;
                }

                reloc_entry = reinterpret_cast< reloc_entry_t* >(
                    reinterpret_cast< std::uint8_t* >( reloc_entry ) + reloc_entry->m_size
                    );
            }

            return true;
        }

        bool map_imports( ) {
            auto import_table = get_import_table( );
            if ( !import_table || !import_table->m_virtual_address || !import_table->m_size )
                return true;

            auto import_desc = mapped_rva_to_va< import_descriptor_t* >(
                static_cast< std::uint32_t >( import_table->m_virtual_address )
            );
            if ( !import_desc )
                return false;

            while ( import_desc->m_name ) {
                auto module_name = mapped_rva_to_va< const char* >( import_desc->m_name );
                if ( !module_name ) {
                    logging::print( obf( "could not resolve import name" ) );
                    return false;
                }

                auto first_thunk = mapped_rva_to_va< nt_image_thunk_data_t* >( import_desc->m_first_thunk );
                auto orig_first_thunk = mapped_rva_to_va< nt_image_thunk_data_t* >(
                    import_desc->m_original_first_thunk ? import_desc->m_original_first_thunk : import_desc->m_first_thunk
                );
                if ( !orig_first_thunk || !first_thunk ) {
                    logging::print( obf( "could not resolve thunk data" ) );
                    return false;
                }

                while ( first_thunk->u1.address_of_data ) {
                    if ( IMAGE_SNAP_BY_ORDINAL64( orig_first_thunk->u1.ordinal ) ) {
                        const auto ordinal = IMAGE_ORDINAL64( orig_first_thunk->u1.ordinal );
                        first_thunk->u1.function = g_pdb->get_symbol_address( reinterpret_cast< const char* >( ordinal ) );
                        if ( !first_thunk->u1.function ) {
                            logging::print( obf( "could not resolve ordinal %llu:%s" ), ordinal, module_name );
                            return false;
                        }
                    }
                    else {
                        auto ibn = mapped_rva_to_va< nt_image_import_name_t* >(
                            static_cast< std::uint32_t >( orig_first_thunk->u1.address_of_data )
                        );
                        if ( !ibn ) {
                            logging::print( obf( "could not get IBN" ) );
                            return false;
                        }

                        first_thunk->u1.function = g_pdb->get_symbol_address( ibn->name );
                        if ( !first_thunk->u1.function ) {
                            logging::print( obf( "could not resolve function %s:%s" ), ibn->name, module_name );
                            return false;
                        }
                    }

                    orig_first_thunk++;
                    first_thunk++;
                }

                import_desc++;
            }

            return true;
        }

        bool map_delayed_imports( ) {
            auto delay_descriptor = get_delay_import_descriptor( );
            if ( !delay_descriptor || !delay_descriptor->m_virtual_address || !delay_descriptor->m_size )
                return true;

            auto delay_desc = mapped_rva_to_va< image_delayload_descriptor_t* >(
                static_cast< std::uint32_t >( delay_descriptor->m_virtual_address )
            );
            if ( !delay_desc )
                return false;

            while ( delay_desc->m_dll_name_rva ) {
                auto module_name = mapped_rva_to_va< const char* >( delay_desc->m_dll_name_rva );
                if ( !module_name ) {
                    logging::print( obf( "could not resolve delayed import name" ) );
                    return false;
                }

                auto first_thunk = mapped_rva_to_va< nt_image_thunk_data_t* >( delay_desc->m_import_address_table_rva );
                auto orig_first_thunk = mapped_rva_to_va< nt_image_thunk_data_t* >( delay_desc->m_import_name_table_rva );
                if ( !orig_first_thunk || !first_thunk ) {
                    logging::print( obf( "could not resolve delayed thunk data" ) );
                    return false;
                }

                while ( first_thunk->u1.address_of_data ) {
                    if ( IMAGE_SNAP_BY_ORDINAL64( orig_first_thunk->u1.ordinal ) ) {
                        const auto ordinal = IMAGE_ORDINAL64( orig_first_thunk->u1.ordinal );
                        first_thunk->u1.function = g_pdb->get_symbol_address( reinterpret_cast< const char* >( ordinal ) );
                    }
                    else {
                        auto ibn = mapped_rva_to_va< nt_image_import_name_t* >(
                            static_cast< std::uint32_t >( orig_first_thunk->u1.address_of_data )
                        );
                        if ( !ibn ) {
                            logging::print( obf( "could not get delayed IBN" ) );
                            return false;
                        }

                        first_thunk->u1.function = g_pdb->get_symbol_address( ibn->name );
                    }

                    if ( !first_thunk->u1.function ) {
                        logging::print( obf( "could not resolve delayed function" ) );
                        return false;
                    }

                    orig_first_thunk++;
                    first_thunk++;
                }

                delay_desc++;
            }

            return true;
        }

        bool map_sections( ) {
            const auto section_count = get_section_count( );
            for ( auto idx = 0u; idx < section_count; idx++ ) {
                auto section = get_section( idx );
                if ( !section || !section->m_virtual_address || !section->m_size_of_raw_data )
                    continue;

                const auto virtual_address = static_cast< std::size_t >( section->m_virtual_address );
                const auto raw_size = static_cast< std::size_t >( section->m_size_of_raw_data );
                const auto raw_offset = static_cast< std::size_t >( section->m_pointer_to_raw_data );

                if ( virtual_address + raw_size > m_mapped_image.size( ) ||
                    raw_offset + raw_size > m_dependency.size( ) ) {
                    logging::print( obf( "invalid section bounds -> %s" ), section->m_name );
                    return false;
                }

                auto section_buffer = m_mapped_image.data( ) + virtual_address;
                auto source_data = m_dependency.data( ) + raw_offset;
                std::memcpy( section_buffer, source_data, raw_size );
            }

            return true;
        }

        bool load_image( const std::string& image_path ) {
            std::ifstream file( image_path, std::ios::binary | std::ios::ate );
            if ( !file ) {
                logging::print( obf( "failed to open image: %s" ), image_path.c_str( ) );
                return false;
            }

            const auto size = file.tellg( );
            if ( size <= 0 ) {
                logging::print( obf( "image is empty: %s" ), image_path.c_str( ) );
                return false;
            }

            m_dependency.resize( static_cast< std::size_t >( size ) );
            file.seekg( 0, std::ios::beg );
            if ( !file.read( reinterpret_cast< char* >( m_dependency.data( ) ), size ) ) {
                logging::print( obf( "failed to read image: %s" ), image_path.c_str( ) );
                m_dependency.clear( );
                return false;
            }

            return initialize_image( );
        }

        bool initialize_image( ) {
            if ( m_dependency.size( ) < sizeof( dos_header_t ) ) {
                logging::print( obf( "invalid image buffer" ) );
                return false;
            }

            auto dos_header = reinterpret_cast< dos_header_t* >( m_dependency.data( ) );
            if ( !dos_header->is_valid( ) ) {
                logging::print( obf( "invalid dos signature" ) );
                return false;
            }

            if ( dos_header->m_lfanew < 0 ) {
				logging::print( obf( "invalid nt header offset" ) );
				return false;
			}

			const auto nt_offset = static_cast< std::size_t >( dos_header->m_lfanew );
			if ( nt_offset > m_dependency.size( ) ||
				sizeof( nt_headers_t ) > m_dependency.size( ) - nt_offset ) {
                logging::print( obf( "invalid nt header offset" ) );
                return false;
            }

            m_nt_headers = reinterpret_cast< nt_headers_t* >( m_dependency.data( ) + nt_offset );
            if ( !m_nt_headers->is_valid( ) ) {
                logging::print( obf( "invalid nt headers" ) );
                m_nt_headers = nullptr;
                return false;
            }

            return true;
        }

        std::uint64_t get_image_base( ) const {
            return m_nt_headers ? m_nt_headers->m_image_base : 0;
        }

        bool is_reloc_stripped( ) const {
            return m_nt_headers && ( m_nt_headers->m_characteristics & IMAGE_FILE_RELOCS_STRIPPED ) != 0;
        }

        std::uint16_t get_section_count( ) const {
            return m_nt_headers ? static_cast< std::uint16_t >( m_nt_headers->m_number_of_sections ) : 0;
        }

        data_directory_t* get_data_directory( const std::size_t index ) {
            if ( !m_nt_headers )
                return nullptr;

            switch ( index ) {
                case image_dir_export:
                    return &m_nt_headers->m_export_table;
                case image_dir_import:
                    return &m_nt_headers->m_import_table;
                case image_dir_resource:
                    return &m_nt_headers->m_resource_table;
                case image_dir_exception:
                    return &m_nt_headers->m_exception_table;
                case image_dir_security:
                    return &m_nt_headers->m_certificate_table;
                case image_dir_relocs:
                    return &m_nt_headers->m_base_relocation_table;
                case image_dir_debug:
                    return &m_nt_headers->m_debug;
                case image_dir_arch:
                    return &m_nt_headers->m_architecture;
                case image_dir_globalptr:
                    return &m_nt_headers->m_global_ptr;
                case image_dir_tls:
                    return &m_nt_headers->m_tls_table;
                case image_dir_load_config:
                    return &m_nt_headers->m_load_config_table;
                case image_dir_bound_import:
                    return &m_nt_headers->m_bound_import;
                case image_dir_iat:
                    return &m_nt_headers->m_iat;
                case image_dir_delay_import:
                    return &m_nt_headers->m_delay_import_descriptor;
                case image_dir_com_desc:
                    return &m_nt_headers->m_clr_runtime_header;
                default:
                    return nullptr;
            }
        }

        data_directory_t* get_reloc_directory( ) {
            return get_data_directory( image_dir_relocs );
        }

        data_directory_t* get_import_table( ) {
            return get_data_directory( image_dir_import );
        }

        data_directory_t* get_delay_import_descriptor( ) {
            return get_data_directory( image_dir_delay_import );
        }

        section_header_t* get_section( const std::size_t index ) {
            if ( !m_nt_headers || index >= get_section_count( ) )
                return nullptr;

            auto section_base = reinterpret_cast< std::uint8_t* >( m_nt_headers ) +
                sizeof( nt_headers_t );
            return reinterpret_cast< section_header_t* >( section_base ) + index;
        }

        template < typename t >
        t mapped_rva_to_va( const std::uint32_t rva ) {
            if ( rva >= m_mapped_image.size( ) )
                return nullptr;

            return reinterpret_cast< t >( m_mapped_image.data( ) + rva );
        }

        template < typename t >
        t raw_rva_to_va( const std::uint32_t rva ) {
            if ( rva >= m_dependency.size( ) )
                return nullptr;

            return reinterpret_cast< t >( m_dependency.data( ) + rva );
        }
    };
}
