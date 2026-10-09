#pragma once

namespace process {
	enum class page_protection : std::uint8_t {
		read_only,
		read_write,
		read_exec,
		read_write_exec,
	};

	struct pml4_meta_t {
		std::uint64_t m_pml4e_pa   {};
		std::uint64_t m_pdpt_va    {};
		std::uint64_t m_pd_va      {};
		std::uint64_t m_pt_va      {};
	};

	struct allocated_region_t {
		std::uint64_t   m_base_va      {};
		std::size_t     m_count        {};
		page_protection m_prot         {};
		std::uint64_t   m_backing_va   {};
		std::uint32_t   m_backing_size {};
		pml4_meta_t     m_pml4_meta    {};
		bool valid( ) const { return m_base_va && m_count && m_backing_va; }
	};

	inline std::uint64_t mmpfn_stride( ) {
		static const auto value = [ ] {
			const auto off_u4 = g_scanner ? g_scanner->get_member( obf( "_MMPFN" ), obf( "u4" ) ) : 0;
			return off_u4 ? static_cast< std::uint64_t >( off_u4 ) + sizeof( std::uint64_t ) : 0ull;
		}( );
		return value;
	}

	inline std::uint64_t mmpfn_pte_address( ) {
		static const auto value = g_scanner
			? static_cast< std::uint64_t >( g_scanner->get_member( obf( "_MMPFN" ), obf( "PteAddress" ) ) )
			: 0ull;
		return value;
	}

	class c_allocator {
		std::uint64_t m_process_peb {};

	public:
		bool initialize( std::uint64_t process_peb ) {
			m_process_peb = process_peb;
			return true;
		}

		std::unique_ptr< allocated_region_t > allocate_in_module_gap(
			std::size_t size,
			page_protection prot = page_protection::read_write_exec
		) {
			if ( !size || !m_process_peb )
				return {};

			static const auto mm_alloc_fn    = g_scanner->get_symbol( obf( "MmAllocateIndependentPages" ) );
			static const auto mm_phys_fn     = g_scanner->get_symbol( obf( "MmGetPhysicalAddress" ) );
			static const auto mm_free_fn     = g_scanner->get_symbol( obf( "MmFreeIndependentPages" ) );
			static const auto mm_pfn_db_ptr  = g_scanner->get_symbol( obf( "MmPfnDatabase" ) );
			if ( !mm_alloc_fn || !mm_phys_fn || !mm_free_fn || !mm_pfn_db_ptr )
				return {};

			if ( !mmpfn_stride( ) || !mmpfn_pte_address( ) ) {
				logging::print( obf( "allocator: _MMPFN offsets unavailable - aborting" ) );
				return {};
			}

			std::uint64_t pfn_db = 0;
			if ( !memory::read_system( mm_pfn_db_ptr, &pfn_db, sizeof( pfn_db ) ) || !pfn_db )
				return {};

			const auto sys_dtb     = g_paging->m_system_dtb;
			const auto sys_dtb_pfn = sys_dtb >> paging::page_shift;
			std::uint64_t pte_base = 0;
			for ( std::uint32_t i = 256; i < 512 && !pte_base; ++i ) {
				pml4e entry {};
				if ( !memory::read_physical( sys_dtb + i * sizeof( pml4e ), &entry, sizeof( entry ) ) )
					continue;
				if ( entry.hard.present && entry.hard.pfn == sys_dtb_pfn )
					pte_base = 0xFFFF000000000000ull | ( static_cast< std::uint64_t >( i ) << 39 );
			}
			if ( !pte_base )
				return {};

			const auto aligned    = ( size + paging::page_4kb_mask ) & ~paging::page_4kb_mask;
			const auto count      = static_cast< std::uint32_t >( aligned >> paging::page_shift );
			const auto backing_sz = static_cast< std::uint32_t >( count * paging::page_4kb_size );

			const auto gap_va = find_module_image_gap( count );
			if ( !gap_va )
				return {};

			const auto backing_va = g_syscall->call_kernel< std::uint64_t >(
				mm_alloc_fn,
				static_cast< std::uint64_t >( backing_sz ),
				static_cast< std::uint64_t >( -1 ) );
			if ( !backing_va )
				return {};

			auto zero_buf = std::make_unique< std::uint8_t[ ] >( paging::page_4kb_size );
			std::memset( zero_buf.get( ), 0, paging::page_4kb_size );

			for ( std::uint32_t i = 0; i < count; ++i ) {
				const auto page_va      = gap_va + i * paging::page_4kb_size;
				const auto phys_backing = g_syscall->call_kernel< std::uint64_t >(
					mm_phys_fn, backing_va + i * paging::page_4kb_size );
				if ( !phys_backing ) {
					free_pages( mm_free_fn, backing_va, backing_sz );
					return {};
				}

				memory::write_physical( phys_backing, zero_buf.get( ), paging::page_4kb_size );

				const auto pte_pa = get_pte_pa_for_module_page( page_va );
				if ( !pte_pa ) {
					free_pages( mm_free_fn, backing_va, backing_sz );
					return {};
				}

				pte entry {};
				entry.hard.pfn = phys_backing >> paging::page_shift;
				apply_protection( entry, prot );
				memory::write_physical( *pte_pa, &entry, sizeof( pte ) );

				const auto pte_kernel_va = pte_base + ( page_va >> paging::page_shift ) * 8;
				const auto pfn           = phys_backing >> paging::page_shift;
				memory::write_system( pfn_db + pfn * mmpfn_stride( ) + mmpfn_pte_address( ),
					const_cast< std::uint64_t* >( &pte_kernel_va ), sizeof( pte_kernel_va ) );

				const auto pt_pfn        = *pte_pa >> paging::page_shift;
				const auto pt_used_addr  = pfn_db + pt_pfn * mmpfn_stride( ) + 0x12;
				std::uint16_t used_ptes  = 0;
				memory::read_system( pt_used_addr, &used_ptes, sizeof( used_ptes ) );
				++used_ptes;
				memory::write_system( pt_used_addr, &used_ptes, sizeof( used_ptes ) );
			}

			return std::make_unique< allocated_region_t >( allocated_region_t {
				.m_base_va      = gap_va,
				.m_count        = count,
				.m_prot         = prot,
				.m_backing_va   = backing_va,
				.m_backing_size = backing_sz,
				.m_pml4_meta    = {},
			} );
		}

		std::unique_ptr< allocated_region_t > allocate_memory( std::size_t size,
			page_protection prot = page_protection::read_write ) {
			if ( !size )
				return {};

			const auto aligned = ( size + paging::page_4kb_mask ) & ~paging::page_4kb_mask;
			const auto count   = static_cast< std::uint32_t >( aligned >> paging::page_shift );
			if ( !count || count > 512 )
				return {};

			static const auto mm_alloc_fn = g_scanner->get_symbol( obf( "MmAllocateIndependentPages" ) );
			static const auto mm_phys_fn  = g_scanner->get_symbol( obf( "MmGetPhysicalAddress" ) );
			static const auto mm_free_fn  = g_scanner->get_symbol( obf( "MmFreeIndependentPages" ) );
			if ( !mm_alloc_fn || !mm_phys_fn || !mm_free_fn )
				return {};

			const auto slot = find_free_pml4e( );
			if ( !slot ) {
				logging::print( obf( "alloc: no free pml4e slot in target" ) );
				return {};
			}

			const auto base_va    = static_cast< std::uint64_t >( *slot ) << 39;
			const auto backing_sz = static_cast< std::uint32_t >( count * paging::page_4kb_size );

			const auto backing_va = g_syscall->call_kernel< std::uint64_t >(
				mm_alloc_fn,
				static_cast< std::uint64_t >( backing_sz ),
				static_cast< std::uint64_t >( -1 ) );
			if ( !backing_va )
				return {};

			auto zero_buf = std::make_unique< std::uint8_t[ ] >( paging::page_4kb_size );
			std::memset( zero_buf.get( ), 0, paging::page_4kb_size );

			std::vector< std::uint64_t > pfns( count );
			for ( std::uint32_t i = 0; i < count; ++i ) {
				const auto pa = g_syscall->call_kernel< std::uint64_t >(
					mm_phys_fn, backing_va + i * paging::page_4kb_size );
				if ( !pa ) {
					free_pages( mm_free_fn, backing_va, backing_sz );
					return {};
				}
				memory::write_physical( pa, zero_buf.get( ), paging::page_4kb_size );
				pfns[ i ] = pa >> paging::page_shift;
			}

			const auto [ pdpt_va, pdpt_pa ] = alloc_pt_page( mm_alloc_fn, mm_phys_fn, mm_free_fn, zero_buf.get( ) );
			if ( !pdpt_va ) {
				free_pages( mm_free_fn, backing_va, backing_sz );
				return {};
			}

			const auto [ pd_va, pd_pa ] = alloc_pt_page( mm_alloc_fn, mm_phys_fn, mm_free_fn, zero_buf.get( ) );
			if ( !pd_va ) {
				free_pages( mm_free_fn, pdpt_va, paging::page_4kb_size );
				free_pages( mm_free_fn, backing_va, backing_sz );
				return {};
			}

			const auto [ pt_va, pt_pa ] = alloc_pt_page( mm_alloc_fn, mm_phys_fn, mm_free_fn, zero_buf.get( ) );
			if ( !pt_va ) {
				free_pages( mm_free_fn, pd_va, paging::page_4kb_size );
				free_pages( mm_free_fn, pdpt_va, paging::page_4kb_size );
				free_pages( mm_free_fn, backing_va, backing_sz );
				return {};
			}

			auto cleanup_all = [ & ]( ) {
				free_pages( mm_free_fn, pt_va,   paging::page_4kb_size );
				free_pages( mm_free_fn, pd_va,   paging::page_4kb_size );
				free_pages( mm_free_fn, pdpt_va, paging::page_4kb_size );
				free_pages( mm_free_fn, backing_va, backing_sz );
			};

			for ( std::uint32_t i = 0; i < count; ++i ) {
				const auto backing_page_va = backing_va + i * paging::page_4kb_size;
				auto backing_entries = g_paging->hyperspace_entries( backing_page_va );
				if ( !backing_entries || !backing_entries->m_pd || backing_entries->m_pd->hard.page_size ) {
					cleanup_all( );
					return {};
				}
				virt_addr_t bv { backing_page_va };
				const auto backing_pte_pa = ( backing_entries->m_pd->hard.pfn << paging::page_shift ) +
					bv.pte_index * sizeof( pte );
				pte source {};
				if ( !memory::read_physical( backing_pte_pa, &source, sizeof( pte ) ) ) {
					cleanup_all( );
					return {};
				}
				pte entry = source;
				entry.hard.pfn = pfns[ i ];
				apply_protection( entry, prot );
				memory::write_physical( pt_pa + i * sizeof( pte ), &entry, sizeof( pte ) );
			}

			pde pd_entry {};
			pd_entry.hard.page_size       = 0;
			pd_entry.hard.pfn             = pt_pa >> paging::page_shift;
			apply_protection( pd_entry, prot );

			memory::write_physical( pd_pa, &pd_entry, sizeof( pde ) );

			pdpte pdpt_entry {};
			pdpt_entry.hard.page_size       = 0;
			pdpt_entry.hard.pfn             = pd_pa >> paging::page_shift;
			apply_protection( pdpt_entry, prot );
			memory::write_physical( pdpt_pa, &pdpt_entry, sizeof( pdpte ) );

			const auto pml4e_pa = g_paging->m_target_dtb +
				static_cast< std::uint64_t >( *slot ) * sizeof( pml4e );
			pml4e pml4_entry {};
			pml4_entry.hard.pfn             = pdpt_pa >> paging::page_shift;
			apply_protection( pml4_entry, prot );
			memory::write_physical( pml4e_pa, &pml4_entry, sizeof( pml4e ) );

			return std::make_unique< allocated_region_t >( allocated_region_t {
				.m_base_va      = base_va,
				.m_count        = count,
				.m_prot         = prot,
				.m_backing_va   = backing_va,
				.m_backing_size = backing_sz,
				.m_pml4_meta    = {
					.m_pml4e_pa = pml4e_pa,
					.m_pdpt_va  = pdpt_va,
					.m_pd_va    = pd_va,
					.m_pt_va    = pt_va,
				},
			} );
		}

		bool free_memory( const allocated_region_t& region, bool clear_pml4e = true ) {
			if ( !region.valid( ) )
				return false;

			static const auto mm_free_fn = g_scanner->get_symbol( obf( "MmFreeIndependentPages" ) );
			if ( !mm_free_fn )
				return false;

			const bool is_image_gap = ( region.m_pml4_meta.m_pml4e_pa == 0 );

			if ( is_image_gap ) {
				static const auto mm_pfn_db_ptr = g_scanner->get_symbol( obf( "MmPfnDatabase" ) );
				std::uint64_t pfn_db = 0;
				if ( mm_pfn_db_ptr )
					memory::read_system( mm_pfn_db_ptr, &pfn_db, sizeof( pfn_db ) );

				std::uint64_t zero = 0;
				for ( std::uint32_t i = 0; i < region.m_count; ++i ) {
					const auto page_va = region.m_base_va + i * paging::page_4kb_size;
					const auto pte_pa  = get_pte_pa_for_module_page( page_va );
					if ( !pte_pa )
						continue;

					memory::write_physical( *pte_pa, &zero, sizeof( zero ) );

					if ( pfn_db ) {
						const auto pt_pfn       = *pte_pa >> paging::page_shift;
						const auto pt_used_addr = pfn_db + pt_pfn * mmpfn_stride( ) + 0x12;
						std::uint16_t used_ptes = 0;
						memory::read_system( pt_used_addr, &used_ptes, sizeof( used_ptes ) );
						if ( used_ptes )
							--used_ptes;
						memory::write_system( pt_used_addr, &used_ptes, sizeof( used_ptes ) );
					}
				}
			} else {
				if ( clear_pml4e ) {
					std::uint64_t zero = 0;
					memory::write_physical( region.m_pml4_meta.m_pml4e_pa, &zero, sizeof( std::uint64_t ) );
				}

				if ( region.m_pml4_meta.m_pt_va )
					free_pages( mm_free_fn, region.m_pml4_meta.m_pt_va,   paging::page_4kb_size );
				if ( region.m_pml4_meta.m_pd_va )
					free_pages( mm_free_fn, region.m_pml4_meta.m_pd_va,   paging::page_4kb_size );
				if ( region.m_pml4_meta.m_pdpt_va )
					free_pages( mm_free_fn, region.m_pml4_meta.m_pdpt_va, paging::page_4kb_size );
			}

			if ( region.m_backing_va && region.m_backing_size )
				free_pages( mm_free_fn, region.m_backing_va, region.m_backing_size );

			return true;
		}

	private:
		std::optional< std::uint64_t > get_pte_pa_for_module_page( std::uint64_t va ) {
			virt_addr_t v { va };

			const auto pml4_pa = g_paging->m_target_dtb + v.pml4e_index * sizeof( pml4e );
			auto pml4 = g_paging->read_pt_entry< pml4e >( pml4_pa );
			if ( !pml4 || !pml4->hard.present )
				return std::nullopt;

			const auto pdpt_pa = ( pml4->hard.pfn << paging::page_shift ) + v.pdpte_index * sizeof( pdpte );
			auto pdpt = g_paging->read_pt_entry< pdpte >( pdpt_pa );
			if ( !pdpt || !pdpt->hard.present || pdpt->hard.page_size )
				return std::nullopt;

			const auto pd_pa = ( pdpt->hard.pfn << paging::page_shift ) + v.pde_index * sizeof( pde );
			auto pd = g_paging->read_pt_entry< pde >( pd_pa );
			if ( !pd || !pd->hard.present || pd->hard.page_size )
				return std::nullopt;

			return ( pd->hard.pfn << paging::page_shift ) + v.pte_index * sizeof( pte );
		}

		std::uint64_t find_module_image_gap( std::uint32_t needed_pages ) {
			if ( !m_process_peb || !needed_pages )
				return 0;

			peb64_t peb {};
			if ( !memory::read_virtual( m_process_peb, &peb, sizeof( peb ) ) || !peb.m_ldr )
				return 0;

			peb_ldr_data64_t ldr {};
			if ( !memory::read_virtual( peb.m_ldr, &ldr, sizeof( ldr ) ) )
				return 0;

			const auto list_head = peb.m_ldr +
				offsetof( peb_ldr_data64_t, m_in_load_order_module_list );
			auto current = ldr.m_in_load_order_module_list.m_flink;

			for ( std::size_t n = 0; current && current != list_head && n < 0x200; ++n ) {
				ldr_data_table_entry64_t entry {};
				if ( !memory::read_virtual( current, &entry, sizeof( entry ) ) )
					break;

				const auto base = entry.m_dll_base;
				const auto size = entry.m_size_of_image;

				if ( base && size >= needed_pages * paging::page_4kb_size && size <= 0x4000000 ) {
					std::uint64_t run_start = 0;
					std::uint32_t run_count = 0;

					for ( std::uint64_t off = 0; off + paging::page_4kb_size <= size;
					      off += paging::page_4kb_size ) {
						const auto va     = base + off;
						const auto pte_pa = get_pte_pa_for_module_page( va );

						if ( !pte_pa ) {
							run_start = 0;
							run_count = 0;
							continue;
						}

						std::uint64_t slot_raw = 0;
						if ( !memory::read_physical( *pte_pa, &slot_raw, sizeof( slot_raw ) ) ) {
							run_start = 0;
							run_count = 0;
							continue;
						}

						if ( slot_raw == 0 ) {
							if ( !run_start )
								run_start = va;
							if ( ++run_count >= needed_pages )
								return run_start;
						} else {
							run_start = 0;
							run_count = 0;
						}
					}
				}

				const auto next = entry.m_in_load_order_links.m_flink;
				if ( next == current ) break;
				current = next;
			}

			return 0;
		}

		std::optional< std::uint8_t > find_free_pml4e( ) {
			for ( std::uint16_t i = 1; i <= 255; ++i ) {
				const auto pa = g_paging->m_target_dtb + i * sizeof( pml4e );
				pml4e entry {};
				if ( !memory::read_physical( pa, &entry, sizeof( entry ) ) )
					continue;
				if ( !entry.hard.present )
					return static_cast< std::uint8_t >( i );
			}
			return std::nullopt;
		}

		std::pair< std::uint64_t, std::uint64_t > alloc_pt_page(
			std::uint64_t mm_alloc_fn,
			std::uint64_t mm_phys_fn,
			std::uint64_t mm_free_fn,
			void*         zero_buf
		) {
			const auto va = g_syscall->call_kernel< std::uint64_t >(
				mm_alloc_fn,
				static_cast< std::uint64_t >( paging::page_4kb_size ),
				static_cast< std::uint64_t >( -1 ) );
			if ( !va )
				return {};

			const auto pa = g_syscall->call_kernel< std::uint64_t >( mm_phys_fn, va );
			if ( !pa ) {
				free_pages( mm_free_fn, va, paging::page_4kb_size );
				return {};
			}

			memory::write_physical( pa, zero_buf, paging::page_4kb_size );
			return { va, pa };
		}

		static void free_pages( std::uint64_t mm_free_fn, std::uint64_t va, std::size_t size ) {
			if ( va && size )
				g_syscall->call_kernel( mm_free_fn, va, static_cast< std::uint64_t >( size ) );
		}

		template <typename pt>
		static void apply_protection( pt& entry, page_protection prot ) {
			entry.hard.present         = 1;
			entry.hard.user_supervisor = 1;
			switch ( prot ) {
			case page_protection::read_only:
				entry.hard.read_write = 0;
				entry.hard.no_execute = 1;
				break;
			case page_protection::read_write:
				entry.hard.read_write = 1;
				entry.hard.no_execute = 1;
				break;
			case page_protection::read_exec:
				entry.hard.read_write = 0;
				entry.hard.no_execute = 0;
				break;
			case page_protection::read_write_exec:
				entry.hard.read_write = 1;
				entry.hard.no_execute = 0;
				break;
			}
		}
	};
}
