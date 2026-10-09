#pragma once

namespace allocation {
	enum class e_protection : std::uint8_t {
		read_only,
		read_write,
		read_execute,
		read_write_execute,
	};

	struct backing_pages_t {
		std::uint64_t              m_va   {};
		std::uint32_t              m_size {};
		std::vector<std::uint64_t> m_pfns {};
	};

	struct page_map_t {
		std::uint64_t m_pte_pa       {};
		std::uint64_t m_original_pte {};
		std::uint64_t m_installed_pte {};
	};

	struct allocation_t {
		std::uint64_t             m_va              {};
		std::uint32_t             m_size            {};
		std::uint32_t             m_pages           {};
		std::vector<page_map_t>   m_page_maps       {};
		std::uint64_t             m_backing_va      {};
		std::uint32_t             m_backing_size    {};
		std::uint64_t             m_pt_backing_va   {};
		std::uint32_t             m_pt_backing_size {};
		std::uint64_t             m_pde_pa          {};
		std::uint64_t             m_original_pde    {};
		e_protection              m_section_prot    { e_protection::read_write };
		bool valid( ) const { return m_va && m_pages && m_backing_va; }
	};

	using large_allocation_t = allocation_t;

	class c_allocator {
	public:
		bool initialize( ) {
			if ( m_initialized )
				return true;

			m_ntoskrnl_base = g_scanner->get_symbol( obf( "ntoskrnl_base" ) );
			if ( !m_ntoskrnl_base )
				return false;

			m_mm_alloc_pages   = g_scanner->get_symbol( obf( "MmAllocateIndependentPages" ) );
			m_mm_free_pages    = g_scanner->get_symbol( obf( "MmFreeIndependentPages" ) );
			m_mm_get_phys_addr = g_scanner->get_symbol( obf( "MmGetPhysicalAddress" ) );
			if ( !m_mm_alloc_pages || !m_mm_free_pages || !m_mm_get_phys_addr )
				return false;

			m_initialized = true;
			return true;
		}

		std::optional<allocation_t> allocate_large_page( std::size_t size ) {
			if ( !m_initialized && !initialize( ) )
				return std::nullopt;

			const auto aligned = static_cast<std::uint32_t>( ( size + 0xFFFu ) & ~0xFFFu );
			const auto pages   = aligned >> paging::page_shift;
			if ( !pages )
				return std::nullopt;

			auto backing = allocate_backing_pages( pages );
			if ( !backing ) {
				logging::print( obf( "alloc: allocate_backing_pages(%u) failed" ), pages );
				return std::nullopt;
			}

			candidate_t candidate {};
			const bool have_candidate = find_first_candidate( aligned, candidate );
			if ( !have_candidate ) {
				return build_direct_allocation( *backing, aligned, pages );
			}

			std::uint64_t pt_backing_va   = 0;
			std::uint32_t pt_backing_size = 0;
			if ( candidate.m_needs_split ) {
				if ( !split_large_pde( candidate, pt_backing_va, pt_backing_size ) ) {
					logging::print( obf( "alloc: split_2mb failed at 0x%llx" ), candidate.m_pde_va );
					free_backing_pages( backing->m_va, backing->m_size );
					return std::nullopt;
				}
			}

			allocation_t alloc {};
			alloc.m_va              = candidate.m_target_va;
			alloc.m_size            = aligned;
			alloc.m_pages           = pages;
			alloc.m_backing_va      = backing->m_va;
			alloc.m_backing_size    = backing->m_size;
			alloc.m_pt_backing_va   = pt_backing_va;
			alloc.m_pt_backing_size = pt_backing_size;
			alloc.m_pde_pa          = candidate.m_pde_pa;
			alloc.m_original_pde    = candidate.m_original_pde;
			alloc.m_section_prot    = candidate.m_section_prot;
			alloc.m_page_maps.reserve( pages );

			for ( std::uint32_t i = 0; i < pages; i++ ) {
				const auto target_va = candidate.m_target_va + i * paging::page_4kb_size;

				const auto pt_pa = resolve_pt_pa( target_va );
				if ( !pt_pa ) {
					rollback( alloc );
					return std::nullopt;
				}

				pte original {};
				if ( !memory::read_physical( pt_pa, &original, sizeof( pte ) ) ) {
					rollback( alloc );
					return std::nullopt;
				}

				const auto backing_pt_pa = resolve_pt_pa( backing->m_va + i * paging::page_4kb_size );
				if ( !backing_pt_pa ) {
					rollback( alloc );
					return std::nullopt;
				}
				pte source {};
				if ( !memory::read_physical( backing_pt_pa, &source, sizeof( pte ) ) ) {
					rollback( alloc );
					return std::nullopt;
				}

				pte installed = source;
				installed.hard.pfn = backing->m_pfns[ i ];
				apply_protection( installed, alloc.m_section_prot );

				if ( !memory::write_physical( pt_pa, &installed, sizeof( pte ) ) ) {
					rollback( alloc );
					return std::nullopt;
				}

				alloc.m_page_maps.push_back( {
					.m_pte_pa       = pt_pa,
					.m_original_pte = *reinterpret_cast<std::uint64_t*>( &original ),
					.m_installed_pte = *reinterpret_cast<std::uint64_t*>( &installed ),
				} );
			}

			return alloc;
		}

		bool set_protection( allocation_t& alloc, e_protection prot ) {
			if ( !alloc.valid( ) )
				return false;

			for ( auto& info : alloc.m_page_maps ) {
				pte entry {};
				*reinterpret_cast<std::uint64_t*>( &entry ) = info.m_installed_pte;
				apply_protection( entry, prot );
				info.m_installed_pte = *reinterpret_cast<std::uint64_t*>( &entry );
				if ( !memory::write_physical( info.m_pte_pa, &entry, sizeof( pte ) ) )
					return false;
			}
			return true;
		}

		bool write( allocation_t& alloc, std::uint32_t offset, const void* data, std::size_t size ) {
			if ( !alloc.valid( ) || offset + size > alloc.m_size )
				return false;

			set_protection( alloc, e_protection::read_write );
			const auto ok = memory::write_virtual( alloc.m_va + offset,
				const_cast<void*>( data ), size );
			set_protection( alloc, alloc.m_section_prot );
			return ok;
		}

		bool free( allocation_t& alloc ) {
			if ( !alloc.valid( ) )
				return false;

			set_protection( alloc, e_protection::read_write );

			auto zeros = std::make_unique<std::uint8_t[]>( paging::page_4kb_size );
			std::memset( zeros.get( ), 0, paging::page_4kb_size );
			for ( std::uint32_t i = 0; i < alloc.m_pages; i++ )
				memory::write_virtual( alloc.m_va + i * paging::page_4kb_size,
					zeros.get( ), paging::page_4kb_size );

			for ( auto& info : alloc.m_page_maps )
				memory::write_physical( info.m_pte_pa,
					&info.m_original_pte, sizeof( std::uint64_t ) );

			if ( alloc.m_original_pde && alloc.m_pde_pa )
				memory::write_physical( alloc.m_pde_pa,
					&alloc.m_original_pde, sizeof( std::uint64_t ) );

			if ( alloc.m_pt_backing_va )
				free_backing_pages( alloc.m_pt_backing_va, alloc.m_pt_backing_size );

			free_backing_pages( alloc.m_backing_va, alloc.m_backing_size );

			alloc = {};
			return true;
		}

	private:
		struct candidate_t {
			std::uint64_t m_target_va    {};
			std::uint64_t m_pde_va       {};
			std::uint64_t m_pde_pa       {};
			std::uint64_t m_original_pde {};
			e_protection  m_section_prot {};
			bool          m_needs_split  {};
		};

		static void apply_protection( pte& entry, e_protection prot ) {
			entry.hard.present         = 1;
			entry.hard.user_supervisor = 0;
			entry.hard.global          = 0;
			switch ( prot ) {
			case e_protection::read_only:
				entry.hard.read_write = 0; entry.hard.no_execute = 1; break;
			case e_protection::read_write:
				entry.hard.read_write = 1; entry.hard.no_execute = 1; break;
			case e_protection::read_execute:
				entry.hard.read_write = 0; entry.hard.no_execute = 0; break;
			case e_protection::read_write_execute:
				entry.hard.read_write = 1; entry.hard.no_execute = 0; break;
			}
		}

		static e_protection detect_section_prot( std::uint32_t chr ) {
			const bool w = ( chr & 0x80000000 ) != 0;
			const bool x = ( chr & 0x20000000 ) != 0;
			if ( w && x )  return e_protection::read_write_execute;
			if ( w && !x ) return e_protection::read_write;
			if ( !w && x ) return e_protection::read_execute;
			return e_protection::read_only;
		}

		std::optional<allocation_t> build_direct_allocation(
			const backing_pages_t& backing,
			std::uint32_t aligned,
			std::uint32_t pages
		) {
			allocation_t alloc {};
			alloc.m_va           = backing.m_va;
			alloc.m_size         = aligned;
			alloc.m_pages        = pages;
			alloc.m_backing_va   = backing.m_va;
			alloc.m_backing_size = backing.m_size;
			alloc.m_section_prot = e_protection::read_write;
			alloc.m_page_maps.reserve( pages );

			for ( std::uint32_t i = 0; i < pages; i++ ) {
				const auto page_va = backing.m_va + i * paging::page_4kb_size;
				const auto pt_pa   = resolve_pt_pa( page_va );
				if ( !pt_pa ) {
					free_backing_pages( backing.m_va, backing.m_size );
					return std::nullopt;
				}

				pte current {};
				if ( !memory::read_physical( pt_pa, &current, sizeof( pte ) ) ) {
					free_backing_pages( backing.m_va, backing.m_size );
					return std::nullopt;
				}

				const auto raw = *reinterpret_cast<std::uint64_t*>( &current );
				alloc.m_page_maps.push_back( {
					.m_pte_pa        = pt_pa,
					.m_original_pte  = raw,
					.m_installed_pte = raw,
				} );
			}
			return alloc;
		}

		bool find_first_candidate( std::uint32_t aligned_size, candidate_t& out ) {
			dos_header_t dos {};
			if ( !memory::read_virtual( m_ntoskrnl_base, &dos, sizeof( dos ) ) )
				return false;

			nt_headers_t nt {};
			const auto nt_va = m_ntoskrnl_base + dos.m_lfanew;
			if ( !memory::read_virtual( nt_va, &nt, sizeof( nt ) ) )
				return false;

			const auto sections_va = nt_va + 0x18 + nt.m_size_of_optional_header;

			for ( auto i = 0; i < nt.m_number_of_sections; i++ ) {
				section_header_t sec {};
				if ( !memory::read_virtual(
					sections_va + i * sizeof( section_header_t ), &sec, sizeof( sec ) ) )
					continue;

				if ( sec.m_size_of_raw_data != 0 )
					continue;

				const auto vsize = static_cast<std::uint32_t>( sec.m_virtual_size ) & ~0xFFFu;
				if ( vsize < aligned_size )
					continue;

				const auto section_base = m_ntoskrnl_base + sec.m_virtual_address;
				const auto target = ( section_base + 0xFFFu ) & ~0xFFFull;

				auto entries = g_paging->hyperspace_entries( target );
				if ( !entries || !entries->m_pd || !entries->m_pd->hard.present )
					continue;

				if ( entries->m_pd->hard.page_size ) {
					out.m_target_va    = target;
					out.m_pde_va       = target & ~paging::page_2mb_mask;
					out.m_pde_pa       = entries->m_pd_pa;
					out.m_original_pde = *reinterpret_cast<std::uint64_t*>( &*entries->m_pd );
					out.m_section_prot = detect_section_prot( sec.m_characteristics );
					out.m_needs_split  = true;
					return true;
				}

				if ( entries->m_pt && entries->m_pt->hard.present ) {
					out.m_target_va    = target;
					out.m_pde_va       = 0;
					out.m_pde_pa       = 0;
					out.m_original_pde = 0;
					out.m_section_prot = detect_section_prot( sec.m_characteristics );
					out.m_needs_split  = false;
					return true;
				}
			}

			return false;
		}

		bool split_large_pde( const candidate_t& c,
			std::uint64_t& pt_backing_va, std::uint32_t& pt_backing_size ) {
			auto pt_backing = allocate_backing_pages( 1 );
			if ( !pt_backing || pt_backing->m_pfns.empty( ) )
				return false;

			const auto new_pt_pfn = pt_backing->m_pfns[ 0 ];
			const auto base_pfn   = ( c.m_original_pde >> 12 ) & 0xFFFFFFFFFULL;

			const std::uint64_t low_flags  = ( c.m_original_pde & 0xFFFULL ) & ~( 1ULL << 7 );
			const std::uint64_t high_flags = c.m_original_pde & 0xFFF0000000000000ULL;

			std::uint64_t ptes[ 512 ] {};
			for ( std::uint32_t i = 0; i < 512; i++ )
				ptes[ i ] = low_flags | ( ( base_pfn + i ) << 12 ) | high_flags;

			const auto new_pt_pa = new_pt_pfn << 12;
			if ( !memory::write_physical( new_pt_pa, ptes, sizeof( ptes ) ) ) {
				free_backing_pages( pt_backing->m_va, pt_backing->m_size );
				return false;
			}

			std::uint64_t new_pde = c.m_original_pde;
			new_pde &= ~( 0xFFFFFFFFFULL << 12 );
			new_pde |= ( new_pt_pfn << 12 );
			new_pde &= ~( 1ULL << 7 );
			new_pde &= ~( 1ULL << 63 );

			if ( !memory::write_physical( c.m_pde_pa, &new_pde, sizeof( std::uint64_t ) ) ) {
				free_backing_pages( pt_backing->m_va, pt_backing->m_size );
				return false;
			}

			pt_backing_va   = pt_backing->m_va;
			pt_backing_size = pt_backing->m_size;
			return true;
		}

		std::uint64_t resolve_pt_pa( std::uint64_t va ) {
			auto entries = g_paging->hyperspace_entries( va );
			if ( !entries || !entries->m_pd || entries->m_pd->hard.page_size )
				return 0;
			virt_addr_t v { va };
			return ( entries->m_pd->hard.pfn << paging::page_shift ) + v.pte_index * sizeof( pte );
		}

		std::optional<backing_pages_t> allocate_backing_pages( std::uint32_t page_count ) {
			const auto size = static_cast<std::uint64_t>( page_count ) * paging::page_4kb_size;

			const auto va = g_syscall->call_kernel<std::uint64_t>(
				m_mm_alloc_pages,
				size,
				static_cast<std::uint64_t>( -1 ) );
			if ( !va )
				return std::nullopt;

			std::vector<std::uint64_t> pfns;
			pfns.reserve( page_count );
			for ( std::uint32_t i = 0; i < page_count; i++ ) {
				const auto page_va = va + i * paging::page_4kb_size;
				const auto pa = g_syscall->call_kernel<std::uint64_t>(
					m_mm_get_phys_addr, page_va );
				if ( !pa ) {
					g_syscall->call_kernel( m_mm_free_pages, va, size );
					return std::nullopt;
				}
				pfns.push_back( pa >> paging::page_shift );
			}

			return backing_pages_t {
				.m_va   = va,
				.m_size = static_cast<std::uint32_t>( size ),
				.m_pfns = std::move( pfns ),
			};
		}

		void free_backing_pages( std::uint64_t va, std::uint32_t size ) {
			if ( va && size )
				g_syscall->call_kernel( m_mm_free_pages, va,
					static_cast<std::uint64_t>( size ) );
		}

		void rollback( allocation_t& alloc ) {
			for ( auto& info : alloc.m_page_maps )
				memory::write_physical( info.m_pte_pa,
					&info.m_original_pte, sizeof( std::uint64_t ) );
			alloc.m_page_maps.clear( );

			if ( alloc.m_original_pde && alloc.m_pde_pa )
				memory::write_physical( alloc.m_pde_pa,
					&alloc.m_original_pde, sizeof( std::uint64_t ) );

			if ( alloc.m_pt_backing_va )
				free_backing_pages( alloc.m_pt_backing_va, alloc.m_pt_backing_size );

			if ( alloc.m_backing_va )
				free_backing_pages( alloc.m_backing_va, alloc.m_backing_size );

			alloc.m_backing_va      = 0;
			alloc.m_backing_size    = 0;
			alloc.m_pt_backing_va   = 0;
			alloc.m_pt_backing_size = 0;
			alloc.m_pde_pa          = 0;
			alloc.m_original_pde    = 0;
		}

		bool          m_initialized      { false };
		std::uint64_t m_ntoskrnl_base    {};
		std::uint64_t m_mm_alloc_pages   {};
		std::uint64_t m_mm_free_pages    {};
		std::uint64_t m_mm_get_phys_addr {};
	};
}
