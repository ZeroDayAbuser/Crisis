#pragma once

namespace paging {
	template <typename va_t>
	bool hide_pages( va_t address, std::size_t size, bool lock_page = false ) {
		const auto page_mask = paging::page_4kb_size - 1;
		const auto aligned_size = ( size + page_mask ) & ~page_mask;
		const auto page_count = aligned_size >> paging::page_shift;
		const auto va = reinterpret_cast< std::uint64_t >( ( void* )address );

		for ( auto idx = 0; idx < page_count; ++idx ) {
			const auto current_va = va + ( idx << paging::page_shift );
			const auto current_pa = kernel::mm_get_physical_address( current_va );
			if ( !current_pa )
				continue;

			auto mm_pfn_database = kernel::get_mm_pfn_database( );
			if ( !mm_pfn_database )
				continue;

			auto pfn_entry = &mm_pfn_database[ current_pa >> paging::page_shift ];
			if ( !pfn_entry )
				continue;

			if ( lock_page ) {
				
				if ( !kernel::mi_lock_page_table_page( pfn_entry, 3 ) )
					return false;
			}
			else {
				if ( kernel::mark_pages_bad( current_pa, paging::page_4kb_size ) )
					pfn_entry->m_u4.m_file_only = 1;
				
				pfn_entry->m_u3.m_e3.parity_error = 1;
			}
		}

		return true;
	}

	bool hyperspace_entries( pt_entries_t& entries, std::uint64_t addr ) {
		virt_addr_t va{ addr };
		if ( !dpm::read_physical( m_directory_table_base + ( va.pml4e_index * sizeof( pml4e ) ),
			&entries.m_pml4e, sizeof( pml4e ) ) )
			return false;

		if ( !entries.m_pml4e.hard.present )
			return false;

		if ( !dpm::read_physical( ( entries.m_pml4e.hard.pfn << page_shift ) + ( va.pdpte_index * sizeof( pdpte ) ),
			&entries.m_pdpte, sizeof( pdpte ) ) )
			return false;

		if ( !entries.m_pdpte.hard.present )
			return false;

		if ( entries.m_pdpte.hard.page_size ) {
			return true;
		}

		if ( !dpm::read_physical( ( entries.m_pdpte.hard.pfn << page_shift ) + ( va.pde_index * sizeof( pde ) ),
			&entries.m_pde, sizeof( pde ) ) )
			return false;

		if ( !entries.m_pde.hard.present )
			return false;

		if ( entries.m_pde.hard.page_size ) {
			return true;
		}

		if ( !dpm::read_physical( ( entries.m_pde.hard.pfn << page_shift ) + ( va.pte_index * sizeof( pte ) ),
			&entries.m_pte, sizeof( pte ) ) )
			return false;

		if ( !entries.m_pte.hard.present )
			return false;

		return true;
	}

	bool translate_linear( std::uint64_t va, std::uint64_t* pa = nullptr, std::uint32_t* page_size = nullptr ) {
		pt_entries_t pt_entries;
		if ( !hyperspace_entries( pt_entries, va ) )
			return false;

		if ( !pt_entries.m_pml4e.hard.present )
			return false;

		if ( !pt_entries.m_pdpte.hard.present )
			return false;

		if ( pt_entries.m_pdpte.hard.page_size ) {
			if ( page_size ) *page_size = page_1gb_size;
			if ( pa ) {
				auto pfn_masked = pt_entries.m_pdpte.hard.pfn & 0x3FFFFF;
				*pa = ( pfn_masked << page_shift ) | ( va & page_1gb_mask );
			}
			return true;
		}

		if ( !pt_entries.m_pde.hard.present )
			return false;

		if ( pt_entries.m_pde.hard.page_size ) {
			if ( page_size ) *page_size = page_2mb_size;
			if ( pa ) {
				auto pfn_masked = pt_entries.m_pde.hard.pfn & 0x7FFFFFFF;
				*pa = ( pfn_masked << page_shift ) | ( va & page_2mb_mask );
			}
			return true;
		}

		if ( !pt_entries.m_pte.hard.present )
			return false;

		if ( page_size ) *page_size = page_4kb_size;
		if ( pa ) {
			*pa = ( pt_entries.m_pte.hard.pfn << page_shift ) | ( va & page_4kb_mask );
		}
		return true;
	}

	std::uint64_t get_pte_address( std::uint64_t virtual_address ) {
		_virt_addr_t va{ virtual_address };

		pt_entries_t pt_entries;
		if ( !hyperspace_entries( pt_entries, virtual_address ) )
			return 0;

		if ( pt_entries.m_pdpte.hard.page_size ) {
			kernel::dbg_print( obf( "1GB Large Pages are not supported!\n" ) );
			return 0;
		}

		if ( pt_entries.m_pde.hard.page_size ) {
			kernel::dbg_print( obf( "2MB Large Pages are not supported!\n" ) );
			return 0;
		}

		auto pte_address = kernel::mm_get_virtual_for_physical(
			( pt_entries.m_pde.hard.pfn << 12 ) + ( va.pte_index * sizeof( pte ) ) );
		if ( !pte_address )
			return 0;

		return pte_address;
	}

	bool set_page_protection( std::uint64_t va, page_protection protection, bool user_accessible = false ) {
		pt_entries_t entries;
		if ( !hyperspace_entries( entries, va ) )
			return false;

		virt_addr_t virt_addr{ va };

		if ( entries.m_pdpte.hard.page_size ) {
			if ( !entries.m_pdpte.hard.present )
				return false;

			pdpte pdpt_entry = entries.m_pdpte;
			pdpt_entry.hard.user_supervisor = user_accessible ? 1 : 0;

			switch ( protection ) {
				case page_protection::readwrite_execute:
					pdpt_entry.hard.read_write = 1;
					pdpt_entry.hard.no_execute = 0;
					break;
				case page_protection::readwrite:
					pdpt_entry.hard.read_write = 1;
					pdpt_entry.hard.no_execute = 1;
					break;
				case page_protection::read_execute:
					pdpt_entry.hard.read_write = 0;
					pdpt_entry.hard.no_execute = 0;
					break;
				case page_protection::readonly:
					pdpt_entry.hard.read_write = 0;
					pdpt_entry.hard.no_execute = 1;
					break;
				case page_protection::inaccessible:
					pdpt_entry.hard.present = 0;
					break;
			}

			auto pdpt_phys = ( entries.m_pml4e.hard.pfn << page_shift ) + ( virt_addr.pdpte_index * sizeof( pdpte ) );
			if ( !dpm::write_physical( pdpt_phys, &pdpt_entry, sizeof( pdpte ) ) )
				return false;

			kernel::flush_caches( va );
			return true;
		}

		if ( entries.m_pde.hard.page_size ) {
			if ( !entries.m_pde.hard.present )
				return false;

			pde pd_entry = entries.m_pde;
			pd_entry.hard.user_supervisor = user_accessible ? 1 : 0;

			switch ( protection ) {
				case page_protection::readwrite_execute:
					pd_entry.hard.read_write = 1;
					pd_entry.hard.no_execute = 0;
					break;
				case page_protection::readwrite:
					pd_entry.hard.read_write = 1;
					pd_entry.hard.no_execute = 1;
					break;
				case page_protection::read_execute:
					pd_entry.hard.read_write = 0;
					pd_entry.hard.no_execute = 0;
					break;
				case page_protection::readonly:
					pd_entry.hard.read_write = 0;
					pd_entry.hard.no_execute = 1;
					break;
				case page_protection::inaccessible:
					pd_entry.hard.present = 0;
					break;
			}

			auto pd_phys = ( entries.m_pdpte.hard.pfn << page_shift ) + ( virt_addr.pde_index * sizeof( pde ) );
			if ( !dpm::write_physical( pd_phys, &pd_entry, sizeof( pde ) ) )
				return false;

			kernel::flush_caches( va );
			return true;
		}

		if ( !entries.m_pte.hard.present )
			return false;

		pte pt_entry = entries.m_pte;
		pt_entry.hard.user_supervisor = user_accessible ? 1 : 0;

		switch ( protection ) {
			case page_protection::readwrite_execute:
				pt_entry.hard.read_write = 1;
				pt_entry.hard.no_execute = 0;
				break;
			case page_protection::readwrite:
				pt_entry.hard.read_write = 1;
				pt_entry.hard.no_execute = 1;
				break;
			case page_protection::read_execute:
				pt_entry.hard.read_write = 0;
				pt_entry.hard.no_execute = 0;
				break;
			case page_protection::readonly:
				pt_entry.hard.read_write = 0;
				pt_entry.hard.no_execute = 1;
				break;
			case page_protection::inaccessible:
				pt_entry.hard.present = 0;
				break;
		}

		auto pt_phys = ( entries.m_pde.hard.pfn << page_shift ) + ( virt_addr.pte_index * sizeof( pte ) );
		if ( !dpm::write_physical( pt_phys, &pt_entry, sizeof( pte ) ) )
			return false;

		kernel::flush_caches( va );
		return true;
	}


	bool split_2mb_to_4kb( std::uint64_t address ) {
		_virt_addr_t va{ address };

		pt_entries_t entries;
		if ( !hyperspace_entries( entries, address ) )
			return false;

		if ( !entries.m_pde.hard.present || !entries.m_pde.hard.page_size )
			return false;

		auto new_pte_va = kernel::mm_allocate_independent_pages( paging::page_4kb_size );
		if ( !new_pte_va ) {
			return false;
		}

		auto new_pte = reinterpret_cast< pte* >( new_pte_va );
		auto pde_pfn = entries.m_pde.hard.pfn;
		for ( auto idx = 0; idx < 512; idx++ ) {
			new_pte[ idx ].value = 0;
			new_pte[ idx ].hard.present = 1;
			new_pte[ idx ].hard.read_write = 1;
			new_pte[ idx ].hard.user_supervisor = entries.m_pde.hard.user_supervisor;
			new_pte[ idx ].hard.page_write_through = entries.m_pde.hard.page_write_through;
			new_pte[ idx ].hard.cached_disable = entries.m_pde.hard.cached_disable;
			new_pte[ idx ].hard.accessed = 0;
			new_pte[ idx ].hard.dirty = 0;
			new_pte[ idx ].hard.pat = 0;
			new_pte[ idx ].hard.global = entries.m_pde.hard.global;
			new_pte[ idx ].hard.no_execute = 0;
			new_pte[ idx ].hard.pfn = pde_pfn + idx;
		}

		auto new_pt_phys = kernel::mm_get_physical_address( new_pte_va );
		if ( !new_pt_phys )
			return false;

		pde new_pde;
		new_pde.value = 0;
		new_pde.hard.present = 1;
		new_pde.hard.read_write = 1;
		new_pde.hard.user_supervisor = entries.m_pde.hard.user_supervisor;
		new_pde.hard.page_write_through = entries.m_pde.hard.page_write_through;
		new_pde.hard.cached_disable = entries.m_pde.hard.cached_disable;
		new_pde.hard.accessed = 0;
		new_pde.hard.page_size = 0;
		new_pde.hard.no_execute = 0;
		new_pde.hard.pfn = new_pt_phys >> 12;

		pml4e pml4_entry;
		if ( !dpm::read_physical( m_directory_table_base + ( va.pml4e_index * sizeof( pml4e ) ),
			&pml4_entry, sizeof( pml4e ) ) )
			return false;

		pdpte pdpt_entry;
		if ( !dpm::read_physical( ( pml4_entry.hard.pfn << 12 ) + ( va.pdpte_index * sizeof( pdpte ) ),
			&pdpt_entry, sizeof( pdpte ) ) )
			return false;

		if ( !dpm::write_physical( ( pdpt_entry.hard.pfn << 12 ) + ( va.pde_index * sizeof( pde ) ),
			&new_pde, sizeof( pde ) ) )
			return false;

		__invlpg( reinterpret_cast< void* >( address ) );
		return true;
	}

	bool split_1gb_to_4kb( std::uint64_t address ) {
		_virt_addr_t va{ address };

		pt_entries_t entries;
		if ( !hyperspace_entries( entries, address ) )
			return false;

		if ( !entries.m_pdpte.hard.present || !entries.m_pdpte.hard.page_size )
			return false;

		auto new_pd_va = kernel::mm_allocate_independent_pages( paging::page_4kb_size );
		if ( !new_pd_va )
			return false;

		auto new_pd = reinterpret_cast< pde* >( new_pd_va );
		auto pdpte_pfn = entries.m_pdpte.hard.pfn;
		for ( auto pd_idx = 0; pd_idx < 512; pd_idx++ ) {
			auto new_pt_va = kernel::mm_allocate_independent_pages( paging::page_4kb_size );
			if ( !new_pt_va )
				return false;

			auto new_pt = reinterpret_cast< pte* >( new_pt_va );
			auto pd_pfn = pdpte_pfn + ( pd_idx * 512 );
			for ( auto pt_idx = 0; pt_idx < 512; pt_idx++ ) {
				new_pt[ pt_idx ].hard.present = 1;
				new_pt[ pt_idx ].hard.read_write = 1;
				new_pt[ pt_idx ].hard.user_supervisor = entries.m_pdpte.hard.user_supervisor;
				new_pt[ pt_idx ].hard.page_write_through = entries.m_pdpte.hard.page_write_through;
				new_pt[ pt_idx ].hard.cached_disable = entries.m_pdpte.hard.cached_disable;
				new_pt[ pt_idx ].hard.no_execute = 0;
				new_pt[ pt_idx ].hard.accessed = 0;
				new_pt[ pt_idx ].hard.dirty = 0;
				new_pt[ pt_idx ].hard.pat = 0;
				new_pt[ pt_idx ].hard.pfn = pd_pfn + pt_idx;
			}

			auto new_pt_phys = kernel::mm_get_physical_address( new_pt_va );
			if ( !new_pt_phys )
				return false;

			new_pd[ pd_idx ].hard.present = 1;
			new_pd[ pd_idx ].hard.read_write = 1;
			new_pd[ pd_idx ].hard.user_supervisor = entries.m_pdpte.hard.user_supervisor;
			new_pd[ pd_idx ].hard.page_write_through = entries.m_pdpte.hard.page_write_through;
			new_pd[ pd_idx ].hard.cached_disable = entries.m_pdpte.hard.cached_disable;
			new_pd[ pd_idx ].hard.accessed = 0;
			new_pd[ pd_idx ].hard.page_size = 0;
			new_pd[ pd_idx ].hard.no_execute = 0;
			new_pd[ pd_idx ].hard.pfn = new_pt_phys >> 12;
		}

		auto new_pd_phys = kernel::mm_get_physical_address( new_pd_va );
		if ( !new_pd_phys )
			return false;

		pdpte new_pdpte{ };
		new_pdpte.hard.present = 1;
		new_pdpte.hard.read_write = 1;
		new_pdpte.hard.user_supervisor = entries.m_pdpte.hard.user_supervisor;
		new_pdpte.hard.page_write_through = entries.m_pdpte.hard.page_write_through;
		new_pdpte.hard.cached_disable = entries.m_pdpte.hard.cached_disable;
		new_pdpte.hard.no_execute = 0;
		new_pdpte.hard.accessed = 0;
		new_pdpte.hard.page_size = 0;
		new_pdpte.hard.pfn = new_pd_phys >> 12;

		pml4e pml4_entry;
		if ( !dpm::read_physical( m_directory_table_base + ( va.pml4e_index * sizeof( pml4e ) ),
			&pml4_entry, sizeof( pml4e ) ) )
			return false;

		if ( !dpm::write_physical( ( pml4_entry.hard.pfn << 12 ) + ( va.pdpte_index * sizeof( pdpte ) ),
			&new_pdpte, sizeof( pdpte ) ) )
			return false;

		__invlpg( reinterpret_cast< void* >( address ) );
		return true;
	}

	std::uint32_t find_free_pml4_slot( std::uint64_t system_pml4_pa ) {
		for ( std::uint32_t idx = 257; idx < 511; idx++ ) {
			pml4e entry{ };
			if ( !dpm::read_physical(
				system_pml4_pa + ( idx * sizeof( pml4e ) ),
				&entry, sizeof( pml4e ) ) )
				continue;

			if ( !entry.hard.present ) {
				kernel::dbg_print(
					obf( "[paging] free PML4 slot: %u\n" ), idx );
				return idx;
			}
		}

		kernel::dbg_print( obf( "[paging] no free PML4 slot\n" ) );
		return static_cast< std::uint32_t >( -1 );
	}

	std::uint32_t get_pml4_self_reference( ) {
		const auto physical_page = m_directory_table_base >> paging::page_shift;

		for ( std::uint32_t idx = 0; idx < 512; idx++ ) {
			pml4e entry{ };
			if ( !dpm::read_physical(
				m_directory_table_base + ( idx * sizeof( pml4e ) ),
				&entry, sizeof( pml4e ) ) )
				continue;

			if ( entry.hard.present && entry.hard.pfn == physical_page ) {
				kernel::dbg_print(
					obf( "[paging] PML4 self-reference at index: %u\n" ), idx );
				return idx;
			}
		}

		kernel::dbg_print( obf( "[paging] PML4 self-reference not found\n" ) );
		return static_cast< std::uint32_t >( -1 );
	}

	std::uint64_t create_virtual_address( std::uint32_t pml4_index, bool use_high_address ) {
		auto additional_offset = static_cast< std::uint64_t >( kernel::rand( ) % 512 ) << 21;

		auto get_pml4e = [ &pml4_index ]( ) {
			return static_cast< std::uint64_t >( pml4_index ) << 39;
			};

		std::uint64_t virtual_address;
		if ( use_high_address ) {
			virtual_address = 0xFFFF000000000000ULL | get_pml4e( ) | additional_offset;
		}
		else {
			virtual_address = get_pml4e( );
		}

		return virtual_address;
	}
}