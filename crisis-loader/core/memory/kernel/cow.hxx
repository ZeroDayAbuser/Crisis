#pragma once

namespace cow {
	class c_cow {
		struct gap_t {
			std::uint64_t m_va;
			std::uint32_t m_size;
			std::uint32_t m_rva;
			std::uint32_t m_characteristics;
		};

		struct page_pfn_t {
			std::uint64_t m_pte_pa;
			std::uint64_t m_original_pte;
			std::uint64_t m_installed_pte;
			std::uint64_t m_backing_pfn;
		};

		struct allocation_t {
			std::uint64_t m_va;
			std::uint32_t m_size;
			std::uint32_t m_pages;
			paging::e_pt_protection m_rest_protect;
			std::vector<page_pfn_t> m_page_pfns;
			std::uint64_t m_backing_va;
			std::uint32_t m_backing_size;
			bool m_concealed = false;
		};

		std::vector<gap_t> m_gaps;
		std::vector<allocation_t> m_allocations;
		std::uint64_t m_ntoskrnl_base = 0;
		std::uint32_t m_size_of_image = 0;
		std::uint32_t m_alloc_offset = 0;
		const gap_t* m_active_gap = nullptr;
		bool m_initialized = false;

		static paging::e_pt_protection characteristics_to_protection( std::uint32_t chr ) {
			const bool exec = chr & 0x20000000;
			const bool write = chr & 0x80000000;
			if ( exec )
				return paging::e_pt_protection::read_write_execute;
			return write
				? paging::e_pt_protection::read_write
				: paging::e_pt_protection::none;
		}

		static const char* protection_name( paging::e_pt_protection p ) {
			switch ( p ) {
			case paging::e_pt_protection::read_write_execute: return "read-write execute";
			case paging::e_pt_protection::read_write:         return "read-write";
			case paging::e_pt_protection::none:               return "read-only";
			default:                                          return "inaccessible";
			}
		}

		bool set_range_protection( std::uint64_t va, std::uint32_t size, paging::e_pt_protection protect ) {
			const auto page_count = ( size + 0xFFF ) >> 12;
			for ( std::uint32_t i = 0; i < page_count; i++ )
				g_paging->set_page_protect( va + ( i * 0x1000ull ), protect );
			return true;
		}

		bool split_large_pde( std::uint64_t va ) {
			std::uint64_t pd_pa = 0;
			std::uint64_t pde_value = 0;
			g_paging->hyperspace_entries(
				va,
				nullptr, nullptr,
				[ &pd_pa, &pde_value ]( paging::pt_entries_t& c ) -> bool {
					if ( c.m_pd->hard.page_size ) {
						pd_pa = c.m_pd_pa;
						pde_value = c.m_pd->value;
					}
					return false;
				},
				nullptr
			);
			if ( !pd_pa )
				return false;

			auto pt_backing = kernel::allocate_backing_pages( 1 );
			if ( !pt_backing || pt_backing->m_pfns.empty( ) )
				return false;

			const auto new_pt_pfn = pt_backing->m_pfns[ 0 ];
			const auto base_pfn   = ( pde_value >> 12 ) & 0xFFFFFFFFFULL;

			const std::uint64_t low_flags  = ( pde_value & 0xFFFULL ) & ~( 1ULL << 7 );
			const std::uint64_t high_flags = pde_value & 0xFFF0000000000000ULL;

			std::uint64_t ptes[ 512 ] {};
			for ( std::uint32_t i = 0; i < 512; i++ )
				ptes[ i ] = low_flags | ( static_cast<std::uint64_t>( base_pfn + i ) << 12 ) | high_flags;

			const auto new_pt_pa = new_pt_pfn << 12;
			if ( !memory::write_physical( new_pt_pa, ptes, sizeof( ptes ) ) ) {
				kernel::free_backing_pages( pt_backing->m_va, pt_backing->m_size );
				return false;
			}

			std::uint64_t new_pde = pde_value;
			new_pde &= ~( 0xFFFFFFFFFULL << 12 );
			new_pde |= ( new_pt_pfn << 12 );
			new_pde &= ~( 1ULL << 7 );
			new_pde &= ~( 1ULL << 63 );

			if ( !memory::write_physical( pd_pa, &new_pde, sizeof( std::uint64_t ) ) ) {
				kernel::free_backing_pages( pt_backing->m_va, pt_backing->m_size );
				return false;
			}

			kernel::flush_tlb_all( );
			return true;
		}

		bool map_gap_range( std::uint64_t va, std::uint32_t size, allocation_t& alloc ) {
			const auto page_count = ( size + 0xFFF ) >> 12;

			auto backing = kernel::allocate_backing_pages( page_count );
			if ( !backing ) {
				logging::print( obf( "  map: allocate_backing_pages(%u) failed" ), page_count );
				return false;
			}

			alloc.m_backing_va   = backing->m_va;
			alloc.m_backing_size = backing->m_size;
			alloc.m_page_pfns.reserve( page_count );

			auto rollback = [ & ]( ) {
				unwind_installed_ptes( alloc );
				kernel::free_backing_pages( backing->m_va, backing->m_size );
				alloc.m_backing_va = 0;
				alloc.m_backing_size = 0;
			};

			auto resolve_pt_pa = [ this ]( std::uint64_t target_va, std::uint64_t& out_pt_pa, bool& out_pd_large ) {
				out_pt_pa = 0;
				out_pd_large = false;
				g_paging->hyperspace_entries(
					target_va,
					nullptr, nullptr,
					[ &out_pt_pa, &out_pd_large, target_va ]( paging::pt_entries_t& c ) -> bool {
						if ( c.m_pd->hard.page_size ) { out_pd_large = true; return false; }
						const std::uint64_t pte_index = ( target_va >> 12 ) & 0x1FF;
						out_pt_pa = ( c.m_pd->hard.pfn << 12 ) + ( pte_index * sizeof( pte ) );
						return false;
					},
					nullptr
				);
			};

			for ( std::uint32_t i = 0; i < page_count; i++ ) {
				const auto target_va = va + ( i * 0x1000ull );

				std::uint64_t pt_pa = 0;
				bool pd_was_large = false;
				resolve_pt_pa( target_va, pt_pa, pd_was_large );

				if ( !pt_pa && pd_was_large ) {
					if ( !split_large_pde( target_va ) ) {
						logging::print( obf( "  map: split_large_pde failed at 0x%llx" ), target_va );
						rollback( );
						return false;
					}
					resolve_pt_pa( target_va, pt_pa, pd_was_large );
				}

				if ( !pt_pa ) {
					logging::print( obf( "  map: no PT for 0x%llx (%s)" ),
						target_va, pd_was_large ? "pde still large after split" : "pde missing" );
					rollback( );
					return false;
				}

				pte original {};
				if ( !memory::read_physical( pt_pa, &original, sizeof( pte ) ) ) {
					rollback( );
					return false;
				}

				const auto src_va = backing->m_va + ( i * 0x1000ull );
				auto src_ctx = g_paging->hyperspace_entries( src_va );
				if ( !src_ctx || !src_ctx->m_pt ) {
					logging::print( obf( "  map: backing PT missing at 0x%llx" ), src_va );
					rollback( );
					return false;
				}
				pte source {};
				if ( !memory::read_physical( src_ctx->m_pt_pa, &source, sizeof( pte ) ) ) {
					rollback( );
					return false;
				}

				pte new_pte = source;
				new_pte.hard.pfn = backing->m_pfns[ i ];

				if ( !memory::write_physical( pt_pa, &new_pte, sizeof( pte ) ) ) {
					rollback( );
					return false;
				}

				page_pfn_t info {};
				info.m_pte_pa        = pt_pa;
				info.m_original_pte  = *reinterpret_cast<std::uint64_t*>( &original );
				info.m_installed_pte = *reinterpret_cast<std::uint64_t*>( &new_pte );
				info.m_backing_pfn   = backing->m_pfns[ i ];
				alloc.m_page_pfns.push_back( info );

				kernel::invalidate_tlb_entry( target_va );
			}

			kernel::flush_tlb_all( );
			return true;
		}

		void unwind_installed_ptes( allocation_t& alloc ) {
			for ( auto& info : alloc.m_page_pfns )
				memory::write_physical( info.m_pte_pa, &info.m_original_pte, sizeof( std::uint64_t ) );
			kernel::flush_tlb_all( );
			alloc.m_page_pfns.clear( );
		}

		allocation_t* find_allocation_mut( std::uint64_t va, std::size_t size ) {
			for ( auto& a : m_allocations )
				if ( va >= a.m_va && va + size <= a.m_va + a.m_size )
					return &a;
			return nullptr;
		}

		const allocation_t* find_allocation( std::uint64_t va, std::size_t size ) const {
			for ( const auto& a : m_allocations )
				if ( va >= a.m_va && va + size <= a.m_va + a.m_size )
					return &a;
			return nullptr;
		}

	public:
		c_cow( ) { }
		~c_cow( ) { }

		bool is_initialized( ) const {
			const auto base = g_pdb->m_module_base;
			if ( !base ) return false;

			dos_header_t dos {};
			if ( !memory::read_virtual( base, &dos, sizeof( dos ) ) ||
				dos.m_magic != pe_magic_t::dos_header )
				return false;

			nt_headers_t nt {};
			const auto nt_va = base + dos.m_lfanew;
			if ( !memory::read_virtual( nt_va, &nt, sizeof( nt ) ) ||
				nt.m_signature != pe_magic_t::nt_headers ||
				nt.m_magic != pe_magic_t::opt_header )
				return false;

			const auto section_count = static_cast< std::uint16_t >( nt.m_number_of_sections );
			const auto sections_va   = nt_va + 0x18 + nt.m_size_of_optional_header;

			std::uint64_t best_va   = 0;
			std::uint32_t best_size = 0;

			for ( auto idx = 0; idx < section_count; idx++ ) {
				section_header_t sec {};
				if ( !memory::read_virtual(
					sections_va + idx * sizeof( section_header_t ),
					&sec, sizeof( sec ) ) )
					continue;

				if ( !sec.m_virtual_address || sec.m_size_of_raw_data != 0 )
					continue;

				const auto usable = static_cast< std::uint32_t >( sec.m_virtual_size ) & ~0xFFFu;
				if ( usable < 0x1000 )
					continue;

				if ( usable > best_size ) {
					best_size = usable;
					best_va   = base + static_cast< std::uint64_t >( sec.m_virtual_address );
				}
			}

			if ( !best_va )
				return false;

			bool was_large = false;
			g_paging->hyperspace_entries(
				best_va,
				nullptr, nullptr,
				[ &was_large ]( paging::pt_entries_t& c ) -> bool {
					was_large = !!c.m_pd->hard.page_size;
					return false;
				},
				nullptr
			);

			return !was_large;
		}

		bool initialize( ) {
			if ( m_initialized )
				return true;

			m_ntoskrnl_base = g_pdb->m_module_base;
			if ( !m_ntoskrnl_base )
				return false;

			dos_header_t dos {};
			if ( !memory::read_virtual( m_ntoskrnl_base, &dos, sizeof( dos ) ) ||
				dos.m_magic != pe_magic_t::dos_header )
				return false;

			nt_headers_t nt {};
			const auto nt_va = m_ntoskrnl_base + dos.m_lfanew;
			if ( !memory::read_virtual( nt_va, &nt, sizeof( nt ) ) ||
				nt.m_signature != pe_magic_t::nt_headers ||
				nt.m_magic != pe_magic_t::opt_header )
				return false;

			m_size_of_image = static_cast< std::uint32_t >( nt.m_size_of_image );

			const auto section_count = static_cast< std::uint16_t >( nt.m_number_of_sections );
			const auto sections_va = nt_va + 0x18 + nt.m_size_of_optional_header;

			std::uint64_t total_bytes = 0;

			for ( auto idx = 0; idx < section_count; idx++ ) {
				section_header_t sec {};
				if ( !memory::read_virtual(
					sections_va + idx * sizeof( section_header_t ),
					&sec, sizeof( sec ) ) )
					continue;

				if ( !sec.m_virtual_address )
					continue;

				const auto rawsize = static_cast< std::uint32_t >( sec.m_size_of_raw_data );
				if ( rawsize != 0 )
					continue;

				const auto vsize = static_cast< std::uint32_t >( sec.m_virtual_size );
				const auto usable = vsize & ~0xFFFu;
				if ( usable < 0x1000 )
					continue;

				gap_t gap {};
				gap.m_rva = static_cast< std::uint32_t >( sec.m_virtual_address );
				gap.m_va = m_ntoskrnl_base + gap.m_rva;
				gap.m_size = usable;
				gap.m_characteristics = static_cast< std::uint32_t >( sec.m_characteristics );
				m_gaps.push_back( gap );
				total_bytes += usable;
			}

			if ( m_gaps.empty( ) ) {
				logging::print( obf( "no padding regions found in ntoskrnl." ) );
				return false;
			}

			m_active_gap = &m_gaps[ 0 ];
			for ( const auto& g : m_gaps )
				if ( g.m_size > m_active_gap->m_size )
					m_active_gap = &g;

			m_alloc_offset = 0;

			const auto protect = characteristics_to_protection( m_active_gap->m_characteristics );
			logging::print( obf( "found %s mb %s pages!" ),
				utility::add_commas( static_cast< double >( total_bytes ) / ( 1024.0 * 1024.0 ) ).c_str( ),
				protection_name( protect ) );

			m_initialized = true;
			return true;
		}

		std::uint64_t allocate( std::size_t size, paging::e_pt_protection protection = paging::e_pt_protection::none ) {
			if ( !m_initialized && !initialize( ) )
				return 0;

			if ( !m_active_gap )
				return 0;

			const auto aligned = static_cast< std::uint32_t >( ( size + 0xFFF ) & ~0xFFFull );
			const auto pages = aligned >> 12;

			if ( m_alloc_offset + aligned > m_active_gap->m_size ) {
				logging::print( obf( "not enough space (%u pages requested, %u available)." ),
					pages, ( m_active_gap->m_size - m_alloc_offset ) >> 12 );
				return 0;
			}

			const auto va = m_active_gap->m_va + m_alloc_offset;
			const auto gap_protect = characteristics_to_protection( m_active_gap->m_characteristics );

			allocation_t alloc {};
			alloc.m_va = va;
			alloc.m_size = aligned;
			alloc.m_pages = pages;
			alloc.m_rest_protect = protection == paging::e_pt_protection::none ? gap_protect : protection;
			alloc.m_concealed = false;

			if ( !map_gap_range( va, aligned, alloc ) ) {
				logging::print( obf( "failed to map pages at 0x%llx." ), va );
				return 0;
			}

			set_range_protection( va, aligned, alloc.m_rest_protect );

			m_allocations.push_back( std::move( alloc ) );
			m_alloc_offset += aligned;
			return va;
		}

		bool write( std::uint64_t va, const void* data, std::size_t size ) {
			auto alloc = find_allocation_mut( va, size );
			if ( !alloc )
				return false;

			const bool was_concealed = alloc->m_concealed;
			if ( was_concealed )
				reveal( alloc->m_va );

			const auto page_start = ( va & ~0xFFFull );
			const auto page_end = ( ( va + size + 0xFFF ) & ~0xFFFull );
			const auto span = static_cast< std::uint32_t >( page_end - page_start );

			set_range_protection( page_start, span, paging::e_pt_protection::read_write );
			const auto result = memory::write_virtual( va, const_cast< void* >( data ), size );
			set_range_protection( page_start, span, alloc->m_rest_protect );

			if ( was_concealed )
				conceal( alloc->m_va );

			return result;
		}

		bool read( std::uint64_t va, void* buffer, std::size_t size ) {
			return memory::read_virtual( va, buffer, size );
		}

		bool protect( std::uint64_t va, std::size_t size, paging::e_pt_protection prot ) {
			return set_range_protection( va, static_cast< std::uint32_t >( size ), prot );
		}

		template <typename ret_t = std::uint64_t, typename... args_t>
		ret_t execute( std::uint64_t va, args_t... args ) {
			auto alloc = find_allocation_mut( va, 1 );

			const bool was_concealed = alloc && alloc->m_concealed;
			if ( was_concealed )
				reveal( alloc->m_va );

			if ( alloc )
				set_range_protection( alloc->m_va, alloc->m_size,
					paging::e_pt_protection::read_write_execute );

			auto result = g_syscall->call_kernel<ret_t>( va, args... );

			if ( alloc )
				set_range_protection( alloc->m_va, alloc->m_size,
					alloc->m_rest_protect );

			if ( was_concealed )
				conceal( alloc->m_va );

			return result;
		}

		bool conceal( std::uint64_t va ) {
			auto alloc = find_allocation_mut( va, 1 );
			if ( !alloc || alloc->m_concealed )
				return alloc && alloc->m_concealed;

			for ( auto& info : alloc->m_page_pfns )
				memory::write_physical( info.m_pte_pa, &info.m_original_pte, sizeof( std::uint64_t ) );

			kernel::flush_tlb_all( );
			alloc->m_concealed = true;
			return true;
		}

		bool reveal( std::uint64_t va ) {
			auto alloc = find_allocation_mut( va, 1 );
			if ( !alloc || !alloc->m_concealed )
				return alloc && !alloc->m_concealed;

			for ( auto& info : alloc->m_page_pfns )
				memory::write_physical( info.m_pte_pa, &info.m_installed_pte, sizeof( std::uint64_t ) );

			kernel::flush_tlb_all( );
			alloc->m_concealed = false;
			return true;
		}

		bool is_concealed( std::uint64_t va ) const {
			const auto alloc = find_allocation( va, 1 );
			return alloc && alloc->m_concealed;
		}

		bool destroy( std::uint64_t va ) {
			auto it = std::find_if( m_allocations.begin( ), m_allocations.end( ),
				[ va ]( const allocation_t& a ) { return a.m_va == va; } );

			if ( it == m_allocations.end( ) )
				return false;

			auto alloc = *it;

			if ( alloc.m_concealed ) {
				for ( auto& info : alloc.m_page_pfns )
					memory::write_physical( info.m_pte_pa, &info.m_installed_pte, sizeof( std::uint64_t ) );
				kernel::flush_tlb_all( );
			}

			set_range_protection( alloc.m_va, alloc.m_size, paging::e_pt_protection::read_write );

			std::uint8_t zero_page[ 0x1000 ] {};
			for ( std::uint32_t i = 0; i < alloc.m_pages; i++ )
				memory::write_virtual( alloc.m_va + ( i * 0x1000ull ), zero_page, 0x1000 );

			for ( auto& info : alloc.m_page_pfns )
				memory::write_physical( info.m_pte_pa, &info.m_original_pte, sizeof( std::uint64_t ) );

			kernel::flush_tlb_all( );

			if ( alloc.m_backing_va )
				kernel::free_backing_pages( alloc.m_backing_va, alloc.m_backing_size );

			m_allocations.erase( it );

			bypass::target::flush_pt_cache( );
			return true;
		}

		void destroy( ) {
			for ( auto& alloc : m_allocations ) {
				if ( alloc.m_concealed ) {
					for ( auto& info : alloc.m_page_pfns )
						memory::write_physical( info.m_pte_pa, &info.m_installed_pte, sizeof( std::uint64_t ) );
				}

				set_range_protection( alloc.m_va, alloc.m_size, paging::e_pt_protection::read_write );
				std::uint8_t zero_page[ 0x1000 ] {};
				for ( std::uint32_t i = 0; i < alloc.m_pages; i++ )
					memory::write_virtual( alloc.m_va + ( i * 0x1000ull ), zero_page, 0x1000 );

				for ( auto& info : alloc.m_page_pfns )
					memory::write_physical( info.m_pte_pa, &info.m_original_pte, sizeof( std::uint64_t ) );
			}

			kernel::flush_tlb_all( );

			for ( auto& alloc : m_allocations )
				if ( alloc.m_backing_va )
					kernel::free_backing_pages( alloc.m_backing_va, alloc.m_backing_size );

			bypass::target::flush_pt_cache( );

			m_allocations.clear( );
			m_gaps.clear( );
			m_active_gap = nullptr;
			m_alloc_offset = 0;
			m_ntoskrnl_base = 0;
			m_size_of_image = 0;
			m_initialized = false;
		}
	};

}
