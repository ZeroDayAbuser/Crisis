#pragma once

namespace mapping {
	static constexpr std::size_t mappings_per_pml4 = 512;

	struct pml4_slot_t {
		std::uint32_t m_pml4_index {};
		std::uint64_t m_pdpt_pa {};
		std::uint64_t m_va_base {};
		std::uint64_t m_pa_base {};
	};

	class c_kernel_mapping {
	public:
		std::optional<std::uint64_t> m_eprocess {};
		std::optional<std::uint64_t> m_directory_table_base {};
		std::uint64_t                m_physical_mask {};
		std::uint64_t                m_highest_pa {};
		std::uint64_t                m_installed_bytes {};
		std::uint32_t                m_slots_required {};
		std::uint64_t                m_max_pfn {};
		std::vector<pml4_slot_t>     m_slots {};
		physical_memory_range_t* m_physical_ranges = nullptr;

		bool setup( std::uint32_t process_id ) {
			m_eprocess = process::find_eprocess( process_id );
			if ( !m_eprocess ) {
				logging::print( obf( "failed to find eprocess (pid=%d)" ), process_id );
				return false;
			}

			m_directory_table_base = process::find_dtb( m_eprocess.value( ) );
			if ( !m_directory_table_base ) {
				logging::print( obf( "failed to find dtb (eprocess=0x%llx)" ), m_eprocess.value( ) );
				return false;
			}

			int regs[ 4 ] {};
			__cpuid( regs, 0x80000000 );
			const auto max_ext = static_cast< unsigned >( regs[ 0 ] );
			auto phys_bits = 36u;
			if ( max_ext >= 0x80000008 ) {
				__cpuid( regs, 0x80000008 );
				const auto b = static_cast< unsigned >( regs[ 0 ] & 0xFF );
				if ( b >= 32 && b <= 52 )
					phys_bits = b;
			}
			this->m_physical_mask = ( phys_bits >= 63 ) ? ~0ULL : ( ( 1ULL << phys_bits ) - 1ULL );

			const auto installed_bytes = query_installed_bytes( );
			this->m_installed_bytes = installed_bytes;
			this->m_highest_pa = installed_bytes;
			this->m_max_pfn = installed_bytes >> 12;

			const auto coverage_per_slot =
				static_cast< std::uint64_t >( mappings_per_pml4 ) *
				static_cast< std::uint64_t >( paging::page_1gb_size );

			this->m_physical_ranges = kernel::mm_get_physical_memory_ranges( );
			if ( !m_physical_ranges ) {
				logging::print( obf( "physical ranges unavailable, using installed size 0x%llx" ), installed_bytes );
				return finalize_coverage( coverage_per_slot );
			}

			std::uint64_t highest_pa = 0;
			auto range_count = 0u;
			const auto pa_ceiling = installed_bytes ? installed_bytes * 2
				: ( m_physical_mask ? m_physical_mask : ~0ULL );

			for ( auto i = 0u; i < 256u; ++i ) {
				physical_memory_range_t range {};
				kernel::memcpy( &range,
					reinterpret_cast< void* >( m_physical_ranges + i ),
					sizeof( range ) );

				if ( !range.m_base_page.m_quad_part && !range.m_page_count.m_quad_part ) break;

				// MmGetPhysicalMemoryRanges returns byte addresses / byte counts,
				// not PFNs. Shifting by 12 moves the 1MB-4GB run to 4GB+ and
				// drops the real above-4GB run, so kernel pages never get registered.
				const auto base_pa = static_cast< std::uint64_t >( range.m_base_page.m_quad_part );
				const auto range_bytes = static_cast< std::uint64_t >( range.m_page_count.m_quad_part );

				if ( !range_bytes )                         continue;
				if ( ( base_pa & ~m_physical_mask ) != 0 ) continue;
				if ( installed_bytes && base_pa > pa_ceiling ) continue;
				if ( base_pa > UINT64_MAX - range_bytes ) continue;

				++range_count;

				const auto mask_end = m_physical_mask ? m_physical_mask + 1 : UINT64_MAX;
				const auto clamped_end = min( base_pa + range_bytes, mask_end );
				if ( clamped_end > highest_pa ) highest_pa = clamped_end;
			}

			if ( installed_bytes && highest_pa > pa_ceiling )
				highest_pa = pa_ceiling;

			if ( installed_bytes )
				this->m_highest_pa = ( highest_pa >= installed_bytes / 2 ) ? highest_pa : installed_bytes;
			else
				this->m_highest_pa = highest_pa;

			return finalize_coverage( coverage_per_slot );
		}

		bool create( ) {
			const auto coverage_per_slot =
				static_cast< std::uint64_t >( mappings_per_pml4 ) *
				static_cast< std::uint64_t >( paging::page_1gb_size );

			logging::print( obf( "warning: mapping might crash some applications.." ) );
			Sleep( 1500 );

			const auto original_dtb = g_paging->attach_process( m_directory_table_base.value( ) );

			if ( !m_slots_required && m_highest_pa ) {
				const auto recovered = static_cast< std::uint32_t >(
					( m_highest_pa + coverage_per_slot - 1 ) / coverage_per_slot );
				m_slots_required = recovered ? recovered : 1u;
				logging::print( obf( "recovered slots required (%u) from highest address (0x%llx)" ),
					m_slots_required, m_highest_pa );
			}

			if ( !m_slots_required )
				logging::print( obf( "warning: mappings required is zero. search will not accept any candidates." ) );

			std::vector<std::uint32_t> free_indices;
			auto invalid_pa = 0u, read_fail = 0u, present_cnt = 0u, va_busy = 0u, vq_fail = 0u, free_found = 0u;
			char occupancy[ 257 ] {};
			occupancy[ 0 ] = '#';

			for ( auto search = 1u; search < 256u; ++search ) {
				pml4e entry {};
				const auto pa = m_directory_table_base.value( ) + ( search * sizeof( pml4e ) );
				if ( !utility::is_physical_address_valid( pa, sizeof( entry ) ) ) {
					++invalid_pa;
					occupancy[ search ] = 'I';
					continue;
				}

				if ( !memory::read_physical( pa, &entry, sizeof( entry ) ) ) {
					++read_fail;
					occupancy[ search ] = 'R';
					continue;
				}

				if ( entry.hard.present ) {
					++present_cnt;
					occupancy[ search ] = 'P';
					continue;
				}

				const auto slot_va = g_paging->create_virtual_address( search, 0 );
				MEMORY_BASIC_INFORMATION mbi {};
				const auto vq = VirtualQuery( reinterpret_cast< void* >( slot_va ), &mbi, sizeof( mbi ) );
				if ( vq && mbi.State != MEM_FREE ) {
					++va_busy;
					occupancy[ search ] = 'V';
					continue;
				}

				if ( !vq ) {
					++vq_fail;
					occupancy[ search ] = 'q';
					if ( vq_fail <= 4 )
						logging::print( obf( "failed to query pml4[%u] va=0x%llx gle=%lu" ),
							search, slot_va, GetLastError( ) );
				}
				else {
					occupancy[ search ] = '.';
				}

				++free_found;
				if ( free_indices.size( ) < m_slots_required )
					free_indices.push_back( search );
			}

			if ( free_indices.empty( ) ) {
				logging::print( obf( "no free pml4 slots found." ) );
				const auto saved = occupancy[ 128 ];
				occupancy[ 128 ] = '\0';
				logging::print( obf( "pml4 map 0-127:   %s" ), occupancy );
				occupancy[ 128 ] = saved;
				logging::print( obf( "pml4 map 128-255: %s" ), occupancy + 128 );
				logging::print( obf( "map key: #=idx0 I=invalid_pa R=read_fail P=present V=va_busy q=vq_fail(free) .=free" ) );
				g_paging->attach_process( original_dtb.value( ) );
				return false;
			}

			if ( free_indices.size( ) < m_slots_required )
				logging::print( obf( "warning: only %u of %u required slots available, clamping coverage" ),
					static_cast< std::uint32_t >( free_indices.size( ) ), m_slots_required );

			const auto slots_to_use = static_cast< std::uint32_t >( free_indices.size( ) );

			auto global_pa_idx = 0u;
			for ( auto slot = 0u; slot < slots_to_use; ++slot ) {
				const auto slot_pa_base = static_cast< std::uint64_t >( slot ) * coverage_per_slot;
				const auto pml4_idx = free_indices[ slot ];
				const auto slot_va = g_paging->create_virtual_address( pml4_idx, 0 );

				auto pdpt_va = kernel::allocate_pages( paging::page_4kb_size );
				if ( !pdpt_va ) {
					logging::print( obf( "failed to allocate pdpt for slot %u" ), slot );
					global_pa_idx += mappings_per_pml4;
					continue;
				}

				auto pdpt_pa = kernel::mm_get_physical_address( pdpt_va.value( ) );
				if ( !pdpt_pa ) {
					logging::print( obf( "failed to get pdpt pa for slot %u" ), slot );
					global_pa_idx += mappings_per_pml4;
					continue;
				}

				pml4e pml4_entry {};
				pml4_entry.hard.present = 1;
				pml4_entry.hard.read_write = 1;
				pml4_entry.hard.user_supervisor = 1;
				pml4_entry.hard.no_execute = 1;
				pml4_entry.hard.pfn = pdpt_pa >> 12;

				for ( auto pdpt_idx = 0u; pdpt_idx < mappings_per_pml4; ++pdpt_idx, ++global_pa_idx ) {
					const auto entry_va = slot_va + ( static_cast< std::uint64_t >( pdpt_idx ) * paging::page_1gb_size );
					const auto entry_pa = static_cast< std::uint64_t >( global_pa_idx ) * paging::page_1gb_size;

					logging::print_line( obf( "creating page table mappings... %d/%d" ),
						global_pa_idx + 1, slots_to_use * static_cast< std::uint32_t >( mappings_per_pml4 ) );

					if ( rand( ) & 1 ) {
						if ( !allocate_vad( entry_va, paging::page_1gb_size ) ) {
							logging::print( obf( "failed to insert 1gb vad for pdpt %u" ), pdpt_idx );
							g_paging->attach_process( original_dtb.value( ) );
							return false;
						}

						pdpte entry {};
						entry.hard.present = 1;
						entry.hard.read_write = 1;
						entry.hard.user_supervisor = 1;
						entry.hard.page_size = 1;
						entry.hard.no_execute = 1;
						entry.hard.accessed = 1;
						entry.hard.pfn = entry_pa >> 12;
						g_paging->write_pt_entry( pdpt_pa + ( pdpt_idx * sizeof( pdpte ) ), entry.value );
					}
					else {
						const auto pd_va = kernel::allocate_pages( paging::page_4kb_size );
						if ( !pd_va ) {
							logging::print( obf( "failed to allocate pd for pdpt %u" ), pdpt_idx );
							continue;
						}

						const auto pd_pa = kernel::mm_get_physical_address( pd_va.value( ) );
						if ( !pd_pa ) {
							logging::print( obf( "failed to get pd pa for pdpt %u" ), pdpt_idx );
							continue;
						}

						if ( !allocate_vad( entry_va, paging::page_1gb_size ) ) {
							logging::print( obf( "failed to insert 2mb vad for pdpt %u" ), pdpt_idx );
							g_paging->attach_process( original_dtb.value( ) );
							return false;
						}

						for ( auto pd_idx = 0u; pd_idx < 512u; ++pd_idx ) {
							const auto pde_pa = entry_pa + ( static_cast< std::uint64_t >( pd_idx ) * paging::page_2mb_size );

							pde entry {};
							entry.hard.present = 1;
							entry.hard.read_write = 1;
							entry.hard.user_supervisor = 1;
							entry.hard.page_size = 1;
							entry.hard.no_execute = 1;
							entry.hard.pfn = pde_pa >> 12;
							g_paging->write_pt_entry( pd_pa + ( pd_idx * sizeof( pde ) ), entry.value );
						}

						pdpte pd_entry {};
						pd_entry.hard.present = 1;
						pd_entry.hard.read_write = 1;
						pd_entry.hard.user_supervisor = 1;
						pd_entry.hard.no_execute = 1;
						pd_entry.hard.page_size = 0;
						pd_entry.hard.pfn = pd_pa >> 12;
						g_paging->write_pt_entry( pdpt_pa + ( pdpt_idx * sizeof( pdpte ) ), pd_entry.value );
					}
				}

				const auto pml4_entry_pa = m_directory_table_base.value( ) + ( pml4_idx * sizeof( pml4e ) );
				if ( !g_paging->write_pt_entry( pml4_entry_pa, pml4_entry.value ) ) {
					logging::print( obf( "failed to write pml4 for slot %u" ), slot );
					global_pa_idx += mappings_per_pml4;
					continue;
				}

				m_slots.push_back( { pml4_idx, pdpt_pa, slot_va, slot_pa_base } );
				if ( !register_slot_ranges( slot_pa_base, slot_va, coverage_per_slot ) ) {
					logging::print( obf( "failed to register physical ranges for slot %u." ), slot );
					g_paging->attach_process( original_dtb.value( ) );
					return false;
				}
			}

			if ( m_slots.empty( ) ) {
				logging::print_line( obf( "creating page table mappings... failed\n" ) );
				g_paging->attach_process( original_dtb.value( ) );
				return false;
			}

			const auto actual_ceiling = static_cast< std::uint64_t >( m_slots.size( ) ) * coverage_per_slot;
			if ( m_highest_pa > actual_ceiling ) {
				m_highest_pa = actual_ceiling;
				m_max_pfn = actual_ceiling >> 12;
			}

			{
				const auto installed_bytes = m_installed_bytes ? m_installed_bytes : m_highest_pa;
				const auto pa_ceiling = installed_bytes ? installed_bytes * 2 : m_highest_pa;

				std::uint64_t physical_total = 0, mapped_total = 0;
				if ( m_physical_ranges ) {
					for ( auto i = 0u; i < 256u; ++i ) {
						physical_memory_range_t range {};
						kernel::memcpy( &range,
							reinterpret_cast< void* >( m_physical_ranges + i ),
							sizeof( range ) );
						if ( !range.m_base_page.m_quad_part && !range.m_page_count.m_quad_part ) break;

						const auto base_pa = static_cast< std::uint64_t >( range.m_base_page.m_quad_part );
						const auto range_bytes = static_cast< std::uint64_t >( range.m_page_count.m_quad_part );
						if ( !range_bytes || ( pa_ceiling && base_pa > pa_ceiling ) ) continue;
						if ( base_pa > UINT64_MAX - range_bytes ) continue;

						physical_total += range_bytes;

						const auto range_end = base_pa + range_bytes;
						const auto clamped_end = min( range_end, actual_ceiling );
						if ( clamped_end > min( base_pa, actual_ceiling ) )
							mapped_total += clamped_end - min( base_pa, actual_ceiling );
					}
				}
				else if ( m_highest_pa ) {
					physical_total = m_highest_pa;
					mapped_total = min( m_highest_pa, actual_ceiling );
				}

				logging::print_line( obf( "created page table mappings... %.1f%% of memory\n\n" ),
					physical_total ? ( static_cast< double >( mapped_total ) / static_cast< double >( physical_total ) ) * 100.0 : 0.0 );
				MessageBeep( MB_ICONINFORMATION );
			}

			g_paging->attach_process( original_dtb.value( ) );
			kernel::flush_tlb_all( );
			return true;
		}

		std::uint64_t find_dtb(
			std::uint64_t module_base,
			std::uint64_t process_peb,
			std::chrono::milliseconds watchdog_timeout = std::chrono::seconds( 5 )
		) {
			if ( m_slots.empty( ) || !m_max_pfn )
				return 0;

			logging::print(
				obf( "scanning physical memory... this may take %s seconds" ),
				std::to_string( std::chrono::duration_cast< std::chrono::seconds >( watchdog_timeout ).count( ) ).c_str( )
			);

			const auto timer_start = std::chrono::steady_clock::now( );
			std::uint64_t directory_table_base = 0;

			const virt_addr_t module_va { .value = module_base };

			while ( std::chrono::steady_clock::now( ) - timer_start < watchdog_timeout ) {
				directory_table_base = scan_physical_memory( module_va, process_peb );
				if ( directory_table_base )
					break;

				std::this_thread::yield( );
			}

			const auto elapsed =
				std::chrono::duration_cast< std::chrono::milliseconds >(
					std::chrono::steady_clock::now( ) - timer_start
				).count( );

			if ( !directory_table_base ) {
				logging::print(
					obf( "watchdog timeout while scanning physical memory! (%.2fs)" ),
					static_cast< double >( elapsed ) / 1000.0
				);
				logging::print( obf( "try again later or contact support" ) );
				MessageBeep( MB_ICONERROR );
				return 0;
			}

			logging::print(
				obf( "directory table base: 0x%llx (in %.2fs)" ),
				directory_table_base,
				static_cast< double >( elapsed ) / 1000.0
			);

			return directory_table_base;
		}

	public:
		auto allocate_vad( std::uint64_t eprocess, std::uint64_t va, std::uint64_t size ) -> bool {
			constexpr std::uint32_t bit_no_change              = 3u;
			constexpr std::uint32_t bit_vad_type               = 4u;
			constexpr std::uint32_t bit_protection             = 7u;
			constexpr std::uint32_t bit_private_memory_always_set = 20u;
			constexpr std::uint32_t bit_private_memory         = 21u;

			const auto alloc_vad_addr  = g_pdb->get_symbol_address( obf( "MiAllocateVad" ) );
			const auto insert_vad_addr = g_pdb->get_symbol_address( obf( "MiInsertVad" ) );
			// On 19041+  "u" (VadFlags: VadType/Protection/PrivateMemory) moved to its
			// own union member at a lower offset than "u1" (VadFlags1: CommitCharge).
			// Prefer "u"; fall back to "u1" for older builds where it held both.
			const auto off_u  = g_pdb->get_struct_member( obf( "_MMVAD_SHORT" ), obf( "u" ) );
			const auto off_u1 = g_pdb->get_struct_member( obf( "_MMVAD_SHORT" ), obf( "u1" ) );
			const auto off_flags = off_u ? off_u : ( off_u1 ? off_u1 : 0x34 );
			if ( !alloc_vad_addr || !insert_vad_addr || !eprocess )
				return false;

			if ( eprocess < 0xFFFF000000000000ULL ) {
				logging::print( obf( "allocate_vad: bad eprocess 0x%llx" ), eprocess );
				return false;
			}

			if ( va & 0xFFFULL ) {
				logging::print( obf( "allocate_vad: va 0x%llx not page-aligned" ), va );
				return false;
			}

			const auto start = va;
			const auto end   = va + size - 1;

			const auto vad = g_syscall->call_kernel<std::uint64_t>(
				alloc_vad_addr,
				start,
				end,
				static_cast< std::uint8_t >( 1 )
			);

			if ( !vad ) {
				logging::print( obf( "could not allocate descriptor" ) );
				return false;
			}

			if ( vad < 0xFFFF000000000000ULL ) {
				logging::print( obf( "allocate_vad: MiAllocateVad returned 0x%llx (not kernel)" ), vad );
				return false;
			}

			const auto flags_pa = g_paging->translate_linear( vad + off_flags );
			if ( !flags_pa ) {
				logging::print( obf( "could not translate vad flags" ) );
				return false;
			}

			std::uint32_t flags {};
			if ( !g_driver->read_physical_memory( flags_pa, &flags, sizeof( flags ) ) )
				return false;

			flags &= ~( 0x1Fu << bit_protection );
			flags &= ~( 0x7u  << bit_vad_type );
			flags &= ~( 1u    << bit_no_change );
			flags &= ~( 1u    << bit_private_memory_always_set );
			flags |=  ( 0x4u  << bit_protection );
			// VadLargePages (5): MiCloneVads gate (VadFlags & 0x70) == 0x50 → skips MiBuildForkPte.
			// VadDevicePhysicalMemory (1) fell through the gate and crashed MiBuildForkPte because
			// these VADs have Subsection = 0 and PrivateMemory = 0.
			flags |=  ( 5u << bit_vad_type );
			// PrivateMemory = 1: NtQueryVirtualMemory returns MEM_PRIVATE without ever following
			// the Subsection chain, so no fake Subsection is needed for these mapping VADs.
			flags |=  ( 1u << bit_private_memory );

			if ( !g_driver->write_physical_memory( flags_pa, &flags, sizeof( flags ) ) )
				return false;

			g_syscall->call_kernel<void>( insert_vad_addr, vad, eprocess );
			return true;
		}

		auto allocate_vad( std::uint64_t va, std::uint64_t size ) -> bool {
			if ( !m_eprocess )
				return false;

			MEMORY_BASIC_INFORMATION mbi {};
			if ( VirtualQuery( reinterpret_cast< void* >( va ), &mbi, sizeof( mbi ) ) ) {
				if ( mbi.State != MEM_FREE ) {
					logging::print( obf( "address 0x%llx not free" ), va );
					return false;
				}
			}

			return allocate_vad( m_eprocess.value( ), va, size );
		}

	private:
		static std::uint64_t query_installed_bytes( ) {
			ULONGLONG installed_kb = 0;
			if ( GetPhysicallyInstalledSystemMemory( &installed_kb ) && installed_kb )
				return installed_kb * 1024ULL;

			logging::print( obf( "failed to get installed physical memory: %lu" ),
				GetLastError( ) );

			MEMORYSTATUSEX status { sizeof( status ) };
			if ( GlobalMemoryStatusEx( &status ) && status.ullTotalPhys )
				return status.ullTotalPhys;

			logging::print( obf( "failed to get global physical memory: %lu" ), GetLastError( ) );
			return 0;
		}

		bool finalize_coverage( std::uint64_t coverage_per_slot ) {
			if ( !m_highest_pa ) {
				logging::print( obf( "failed to determine physical memory size" ) );
				return false;
			}

			m_max_pfn = m_highest_pa >> 12;
			m_slots_required = static_cast< std::uint32_t >(
				( m_highest_pa + coverage_per_slot - 1 ) / coverage_per_slot );
			if ( !m_slots_required )
				m_slots_required = 1;

			return clamp_to_shared_capacity( coverage_per_slot );
		}

		bool register_slot_ranges( std::uint64_t slot_pa_base, std::uint64_t slot_va,
			std::uint64_t coverage_per_slot ) {
			if ( !coverage_per_slot || slot_pa_base > UINT64_MAX - coverage_per_slot )
				return false;

			return g_memory->map( slot_pa_base, reinterpret_cast< void* >( slot_va ),
				coverage_per_slot );
		}

		bool clamp_to_shared_capacity( std::uint64_t coverage_per_slot ) {
			module::mapping_info_t info {};
			if ( !g_memory->query_mapping( info ) ) {
				logging::print( obf( "warning: failed to query shared mapping info" ) );
				return true;
			}

			if ( info.physical_mask )
				m_physical_mask = info.physical_mask;

			const auto available_ranges = ( info.max_ranges > info.range_count )
				? info.max_ranges - info.range_count
				: 0;

			if ( !available_ranges ) {
				logging::print( obf( "no shared mapping ranges available" ) );
				m_slots_required = 0;
				m_highest_pa     = 0;
				m_max_pfn        = 0;
				return false;
			}

			if ( m_slots_required > available_ranges ) {
				logging::print( obf( "warning: shared mapping capacity limited: %u -> %llu" ),
					m_slots_required, available_ranges );

				m_slots_required = static_cast< std::uint32_t >( available_ranges );
				m_highest_pa = min( m_highest_pa, available_ranges * coverage_per_slot );
				m_max_pfn    = m_highest_pa >> 12;
			}

			return true;
		}

		__forceinline bool is_address_valid( std::uint64_t pa, std::size_t size ) const noexcept {
			if ( !m_physical_mask || !size )                      return false;
			if ( pa & ~m_physical_mask )                          return false;
			if ( ( ( pa + size - 1 ) & ~m_physical_mask ) != 0 ) return false;
			if ( m_max_pfn && ( pa >> 12 ) >= m_max_pfn )        return false;
			return true;
		}

		bool validate_chain( std::uint64_t cr3, const virt_addr_t& virt ) {
			if ( !is_address_valid( cr3, paging::page_4kb_size ) ) return false;

			auto pml4 = std::optional<pml4e>( memory::read_pa<pml4e>( cr3 + ( virt.pml4e_index * sizeof( pml4e ) ) ) );
			if ( !pml4 || !pml4->hard.present || !pml4->hard.pfn || pml4->hard.pfn >= m_max_pfn ) return false;

			const auto pdpte_pa = pml4->hard.pfn << 12;
			if ( !is_address_valid( pdpte_pa, paging::page_4kb_size ) ) return false;

			auto pdpt = std::optional<pdpte>( memory::read_pa<pdpte>( pdpte_pa + ( virt.pdpte_index * sizeof( pdpte ) ) ) );
			if ( !pdpt || !pdpt->hard.present || !pdpt->hard.pfn || pdpt->hard.pfn >= m_max_pfn ) return false;
			if ( pdpt->hard.page_size ) return true;

			const auto pde_pa = pdpt->hard.pfn << 12;
			if ( !is_address_valid( pde_pa, paging::page_4kb_size ) ) return false;

			auto pd = std::optional<pde>( memory::read_pa<pde>( pde_pa + ( virt.pde_index * sizeof( pde ) ) ) );
			if ( !pd || !pd->hard.present || !pd->hard.pfn || pd->hard.pfn >= m_max_pfn ) return false;
			if ( pd->hard.page_size ) return true;

			const auto pte_pa = pd->hard.pfn << 12;
			if ( !is_address_valid( pte_pa, paging::page_4kb_size ) ) return false;

			auto pt = std::optional<pte>( memory::read_pa<pte>( pte_pa + ( virt.pte_index * sizeof( pte ) ) ) );
			if ( !pt || !pt->hard.present || !pt->hard.pfn || pt->hard.pfn >= m_max_pfn ) return false;

			return true;
		}

		std::uint64_t scan_physical_memory( const virt_addr_t& module_va, const std::uint64_t process_peb ) {
			constexpr auto coverage_per_slot =
				static_cast< std::uint64_t >( mappings_per_pml4 ) *
				static_cast< std::uint64_t >( paging::page_1gb_size );

			for ( const auto& slot : m_slots ) {
				const auto slot_end_pa = min( slot.m_pa_base + coverage_per_slot, m_highest_pa );
				if ( slot.m_pa_base >= slot_end_pa ) continue;

				const auto page_count = ( slot_end_pa - slot.m_pa_base ) >> 12;

				for ( auto page = 0ULL; page < page_count; ++page ) {
					const auto candidate_pa  = slot.m_pa_base + ( page << 12 );
					const auto candidate_pfn = candidate_pa >> 12;

					if ( !is_address_valid( candidate_pa, paging::page_4kb_size ) ) continue;

					std::uint64_t pte_frame = 0;
					if ( !kernel::get_pfn_pte_frame( candidate_pfn, pte_frame ) ) continue;
					if ( pte_frame != candidate_pfn ) continue;

					if ( !validate_chain( candidate_pa, module_va ) ) continue;

					if ( process_peb ) {
						virt_addr_t peb_va { .value = process_peb };
						if ( !validate_chain( candidate_pa, peb_va ) ) continue;
					}

					auto mod_pml4 = std::optional<pml4e>( memory::read_pa<pml4e>(
						candidate_pa + ( module_va.pml4e_index * sizeof( pml4e ) ) ) );
					if ( !mod_pml4 || !mod_pml4->hard.present ) continue;

					auto mod_pdpt = std::optional<pdpte>( memory::read_pa<pdpte>(
						( mod_pml4->hard.pfn << 12 ) + ( module_va.pdpte_index * sizeof( pdpte ) ) ) );
					if ( !mod_pdpt || !mod_pdpt->hard.present ) continue;

					std::uint64_t mod_page_pa = 0;
					if ( mod_pdpt->hard.page_size ) {
						mod_page_pa = ( ( mod_pdpt->hard.pfn & 0x3FFFFF ) << 12 ) + ( module_va.value & paging::page_1gb_mask );
					}
					else {
						auto mod_pd = std::optional<pde>( memory::read_pa<pde>(
							( mod_pdpt->hard.pfn << 12 ) + ( module_va.pde_index * sizeof( pde ) ) ) );
						if ( !mod_pd || !mod_pd->hard.present ) continue;

						if ( mod_pd->hard.page_size ) {
							mod_page_pa = ( ( mod_pd->hard.pfn & 0x7FFFFFFF ) << 12 ) + ( module_va.value & paging::page_2mb_mask );
						}
						else {
							auto mod_pt = std::optional<pte>( memory::read_pa<pte>(
								( mod_pd->hard.pfn << 12 ) + ( module_va.pte_index * sizeof( pte ) ) ) );
							if ( !mod_pt || !mod_pt->hard.present ) continue;
							mod_page_pa = ( mod_pt->hard.pfn << 12 ) + ( module_va.value & paging::page_4kb_mask );
						}
					}

					if ( !is_address_valid( mod_page_pa, 2 ) ) continue;

					auto magic = std::optional<std::uint16_t>( memory::read_pa<std::uint16_t>( mod_page_pa ) );
					if ( !magic || *magic != 0x5A4D ) continue;

					return candidate_pa;
				}
			}
			return 0;
		}
	};
}
