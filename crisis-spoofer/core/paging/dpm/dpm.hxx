#pragma once

namespace phys {
    bool is_address_valid( std::uint64_t pa, std::size_t size );
}

namespace paging {
    namespace dpm {
        struct page_mapping_t {
            std::uint64_t m_virtual_address;
            pte* m_pte_address;
            std::uint64_t m_original_pfn;
        };

        page_mapping_t m_page_mappings[ 64 ] = {};
        bool m_is_initialized = false;

        pte* get_pte_for_address( std::uint64_t address ) {
            virt_addr_t va{ address };

            auto pml4_va = kernel::mm_get_virtual_for_physical( m_directory_table_base & ~0xf );
            if ( !pml4_va ) return nullptr;

            auto pml4 = reinterpret_cast< pml4e* >( pml4_va );
            auto pml4_entry = &pml4[ va.pml4e_index ];
            if ( !pml4_entry->hard.present ) return nullptr;

            auto pdpt_va = kernel::mm_get_virtual_for_physical( pml4_entry->hard.pfn << 12 );
            if ( !pdpt_va ) return nullptr;

            auto pdpt = reinterpret_cast< pdpte* >( pdpt_va );
            auto pdpt_entry = &pdpt[ va.pdpte_index ];
            if ( !pdpt_entry->hard.present ) return nullptr;
            if ( pdpt_entry->hard.page_size ) return nullptr;

            auto pd_va = kernel::mm_get_virtual_for_physical( pdpt_entry->hard.pfn << 12 );
            if ( !pd_va ) return nullptr;

            auto pd = reinterpret_cast< pde* >( pd_va );
            auto pd_entry = &pd[ va.pde_index ];
            if ( !pd_entry->hard.present ) return nullptr;
            if ( pd_entry->hard.page_size ) return nullptr;

            auto pt_va = kernel::mm_get_virtual_for_physical( pd_entry->hard.pfn << 12 );
            if ( !pt_va ) return nullptr;

            auto pt = reinterpret_cast< pte* >( pt_va );
            auto pt_entry = &pt[ va.pte_index ];
            if ( !pt_entry->hard.present ) return nullptr;
            return pt_entry;
        }

        bool initialize( ) {
            if ( m_is_initialized )
                return false;

            for ( auto i = 0; i < 64; i++ ) {
                m_page_mappings[ i ].m_virtual_address = kernel::mm_allocate_independent_pages( page_4kb_size );
                if ( !m_page_mappings[ i ].m_virtual_address )
                    return false;

                m_page_mappings[ i ].m_pte_address = get_pte_for_address( m_page_mappings[ i ].m_virtual_address );
                if ( !m_page_mappings[ i ].m_pte_address )
                    return false;

                m_page_mappings[ i ].m_original_pfn = m_page_mappings[ i ].m_pte_address->hard.pfn;
            }

            m_is_initialized = true;
            return true;
        }

        void cleanup( ) {
            if ( !m_is_initialized )
                return;

            for ( auto i = 0; i < 64; i++ ) {
                auto& mapping = m_page_mappings[ i ];
                if ( mapping.m_pte_address && mapping.m_virtual_address ) {
                    _disable( );
                    mapping.m_pte_address->hard.pfn = mapping.m_original_pfn;
                    __invlpg( reinterpret_cast< void* >( mapping.m_virtual_address ) );
                    _enable( );
                }

                if ( mapping.m_virtual_address )
                    mapping.m_virtual_address = 0;
            }

            memset( m_page_mappings, 0, sizeof( m_page_mappings ) );
            m_is_initialized = false;
        }

		bool read_physical( std::uint64_t physical_address, void* buffer, std::size_t size ) {
			if ( !m_is_initialized || size == 0 )
				return false;

			std::uint8_t* out = reinterpret_cast< std::uint8_t* >( buffer );
			std::uint64_t current_pa = physical_address;
			std::size_t remaining = size;

			while ( remaining > 0 ) {
				std::size_t offset = current_pa & page_4kb_mask;
				std::size_t chunk_size = page_4kb_size - offset;
				if ( chunk_size > remaining )
					chunk_size = remaining;

				// Never repoint a live PTE at a physical address the machine does not
				// have. A garbage cr3 - a torn cache, a wild translate - arrives here as
				// a nonsense pfn; the mapping still succeeds, the access then raises a
				// fault that cannot be caught, and the next delivery is 0x7F with
				// EXCEPTION_DOUBLE_FAULT. phys::is_address_valid already knows the range.
				if ( !phys::is_address_valid( current_pa, chunk_size ) )
					return false;

				auto processor = kernel::ke_get_current_processor_number( );
				if ( processor >= 64 ) processor = 0;
				auto& mapping = m_page_mappings[ processor ];

				if ( !mapping.m_pte_address || !mapping.m_virtual_address )
					return false;

				auto old_pfn = mapping.m_pte_address->hard.pfn;
				_disable( );
				mapping.m_pte_address->hard.pfn = current_pa >> page_shift;
				__invlpg( reinterpret_cast< void* >( mapping.m_virtual_address ) );

				auto source_ptr = reinterpret_cast< std::uint8_t* >( mapping.m_virtual_address ) + offset;

				__try {
					__movsb( out, source_ptr, chunk_size );
				}
				__except ( EXCEPTION_EXECUTE_HANDLER ) {
					mapping.m_pte_address->hard.pfn = old_pfn;
					__invlpg( reinterpret_cast< void* >( mapping.m_virtual_address ) );
					_enable( );
					return false;
				}

				mapping.m_pte_address->hard.pfn = old_pfn;
				__invlpg( reinterpret_cast< void* >( mapping.m_virtual_address ) );
				_enable( );

				out += chunk_size;
				current_pa += chunk_size;
				remaining -= chunk_size;
			}
			return true;
		}

		bool write_physical( std::uint64_t physical_address, void* buffer, std::size_t size ) {
			if ( !m_is_initialized || size == 0 )
				return false;

			std::uint8_t* in = reinterpret_cast< std::uint8_t* >( buffer );
			std::uint64_t current_pa = physical_address;
			std::size_t remaining = size;

			while ( remaining > 0 ) {
				std::size_t offset = current_pa & page_4kb_mask;
				std::size_t chunk_size = page_4kb_size - offset;
				if ( chunk_size > remaining )
					chunk_size = remaining;

				// Never repoint a live PTE at a physical address the machine does not
				// have. A garbage cr3 - a torn cache, a wild translate - arrives here as
				// a nonsense pfn; the mapping still succeeds, the access then raises a
				// fault that cannot be caught, and the next delivery is 0x7F with
				// EXCEPTION_DOUBLE_FAULT. phys::is_address_valid already knows the range.
				if ( !phys::is_address_valid( current_pa, chunk_size ) )
					return false;

				auto processor = kernel::ke_get_current_processor_number( );
				if ( processor >= 64 ) processor = 0;
				auto& mapping = m_page_mappings[ processor ];

				if ( !mapping.m_pte_address || !mapping.m_virtual_address )
					return false;

				auto old_pfn = mapping.m_pte_address->hard.pfn;
				_disable( );
				mapping.m_pte_address->hard.pfn = current_pa >> page_shift;
				mapping.m_pte_address->hard.read_write = 1;
				__invlpg( reinterpret_cast< void* >( mapping.m_virtual_address ) );

				auto dest_ptr = reinterpret_cast< std::uint8_t* >( mapping.m_virtual_address ) + offset;

				__try {
					__movsb( dest_ptr, in, chunk_size );
				}
				__except ( EXCEPTION_EXECUTE_HANDLER ) {
					mapping.m_pte_address->hard.pfn = old_pfn;
					__invlpg( reinterpret_cast< void* >( mapping.m_virtual_address ) );
					_enable( );
					return false;
				}

				mapping.m_pte_address->hard.pfn = old_pfn;
				__invlpg( reinterpret_cast< void* >( mapping.m_virtual_address ) );
				_enable( );

				in += chunk_size;
				current_pa += chunk_size;
				remaining -= chunk_size;
			}
			return true;
		}
    }
}
