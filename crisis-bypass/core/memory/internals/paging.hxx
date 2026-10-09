#pragma once
#include <intrin.h>
#include <thread>

namespace memory {
	bool read_physical( std::uint64_t addr, void* buffer, std::size_t size );
	bool write_physical( std::uint64_t addr, void* buffer, std::size_t size );
	bool read_virtual( std::uint64_t addr, void* buffer, std::size_t size );
	bool write_virtual( std::uint64_t addr, void* buffer, std::size_t size );
	bool read_system( std::uint64_t addr, void* buffer, std::size_t size );
	bool write_system( std::uint64_t addr, void* buffer, std::size_t size );
}

namespace paging {
	constexpr auto page_4kb_size = 0x1000ull;
	constexpr auto page_2mb_size = 0x200000ull;
	constexpr auto page_1gb_size = 0x40000000ull;

	constexpr auto page_shift = 12ull;
	constexpr auto page_2mb_shift = 21ull;
	constexpr auto page_1gb_shift = 30ull;

	constexpr auto page_4kb_mask = 0xFFFull;
	constexpr auto page_2mb_mask = 0x1FFFFFull;
	constexpr auto page_1gb_mask = 0x3FFFFFFFull;

	struct pt_entries_t {
		std::optional<pml4e> m_pml4; std::uint64_t m_pml4_pa {};
		std::optional<pdpte> m_pdpt; std::uint64_t m_pdpt_pa {};
		std::optional<pde>   m_pd;   std::uint64_t m_pd_pa {};
		std::optional<pte>   m_pt;   std::uint64_t m_pt_pa {};
		std::uint64_t m_phys_addr {};
		std::uint64_t m_page_size {};
	};

	struct pfn_database_t {
		static constexpr std::uint64_t m_mmpfn_size = 0x30;
		std::unique_ptr< std::uint8_t[ ] > m_pfn_data;
	};

	class c_paging {
	public:
		std::uint64_t m_system_dtb {};
		std::uint64_t m_target_dtb {};

		c_paging( ) { }
		~c_paging( ) { }

		bool setup( ) {
			auto buf = std::make_unique<std::uint8_t[ ]>( 0x1000 );
			for ( std::uint64_t page = 0; page < 0x20000000; page += 0x1000 ) {
				if ( !memory::read_physical( page, buf.get( ), 0x1000 ) ) continue;

				if ( buf[ 0 ] == 0xE9 && buf[ 1 ] == 0x4D &&
					buf[ 2 ] == 0x06 && buf[ 3 ] == 0x00 ) {
					auto system_dtb = *reinterpret_cast< std::uint64_t* >( buf.get( ) + 0xA0 );
					if ( system_dtb && ( system_dtb & 0xFFF ) == 0 ) {
						logging::print( obf( "found system dtb: 0x%llx" ), system_dtb );
						this->m_target_dtb = this->m_system_dtb = system_dtb;
						return true;
					}
				}
			}
			return false;
		}

		template<typename T>
		std::optional<T> read_pt_entry( std::uint64_t phys_addr ) {
			T entry {};
			if ( !memory::read_physical( phys_addr, &entry, sizeof( T ) ) )
				return std::nullopt;
			return entry;
		}

		bool write_pt_entry( std::uint64_t phys_addr, std::uint64_t value ) {
			return memory::write_physical( phys_addr, &value, sizeof( value ) );
		}

		std::optional<std::uint64_t> attach_system( ) {
			const auto prev = this->m_target_dtb;
			this->m_target_dtb = this->m_system_dtb;
			return std::optional<std::uint64_t>( prev );
		}

		std::optional<std::uint64_t> attach_process( std::uint64_t new_dtb ) {
			const auto prev = this->m_target_dtb;
			this->m_target_dtb = new_dtb;
			return std::optional<std::uint64_t>( prev );
		}

		std::uint64_t translate_linear( std::uint64_t virt_addr, std::uint32_t* page_size = nullptr ) {
			auto ctx = hyperspace_entries( virt_addr );
			if ( !ctx ) return 0;
			if ( page_size ) *page_size = static_cast< std::uint32_t >( ctx->m_page_size );
			return ctx->m_phys_addr;
		}

		std::optional<pt_entries_t> hyperspace_entries( std::uint64_t virt_addr ) {
			virt_addr_t va { virt_addr };
			pt_entries_t ctx {};

			ctx.m_pml4_pa = m_target_dtb + ( va.pml4e_index * sizeof( pml4e ) );
			auto pml4 = read_pt_entry<pml4e>( ctx.m_pml4_pa );
			if ( !pml4 || !pml4->hard.present ) return std::nullopt;
			ctx.m_pml4 = pml4;

			ctx.m_pdpt_pa = ( ctx.m_pml4->hard.pfn << 12 ) + ( va.pdpte_index * sizeof( pdpte ) );
			auto pdpt = read_pt_entry<pdpte>( ctx.m_pdpt_pa );
			if ( !pdpt || !pdpt->hard.present ) return std::nullopt;
			ctx.m_pdpt = pdpt;

			if ( ctx.m_pdpt->hard.page_size ) {
				ctx.m_page_size = page_1gb_size;
				ctx.m_phys_addr = ( ctx.m_pdpt->hard.pfn << 12 ) + ( virt_addr & page_1gb_mask );
				return ctx;
			}

			ctx.m_pd_pa = ( ctx.m_pdpt->hard.pfn << 12 ) + ( va.pde_index * sizeof( pde ) );
			auto pd = read_pt_entry<pde>( ctx.m_pd_pa );
			if ( !pd || !pd->hard.present ) return std::nullopt;
			ctx.m_pd = pd;

			if ( ctx.m_pd->hard.page_size ) {
				ctx.m_page_size = page_2mb_size;
				ctx.m_phys_addr = ( ctx.m_pd->hard.pfn << 12 ) + ( virt_addr & page_2mb_mask );
				return ctx;
			}

			ctx.m_pt_pa = ( ctx.m_pd->hard.pfn << page_shift ) + ( va.pte_index * sizeof( pte ) );
			auto pt = read_pt_entry<pte>( ctx.m_pt_pa );
			if ( !pt || !pt->hard.present ) return std::nullopt;
			ctx.m_pt = pt;

			ctx.m_page_size = page_4kb_size;
			ctx.m_phys_addr = ( ctx.m_pt->hard.pfn << 12 ) + ( virt_addr & page_4kb_mask );
			return ctx;
		}

		std::uint64_t find_accessible_page( ) {
			std::uint64_t base_address = 0;
			for ( auto pml4_idx = 1u; pml4_idx < 256u && !base_address; ++pml4_idx ) {
				const auto pml4_pa = m_target_dtb + ( pml4_idx * sizeof( pml4e ) );
				const auto pml4_entry = read_pt_entry<pml4e>( pml4_pa );
				if ( !pml4_entry || !pml4_entry->hard.present ) continue;

				for ( auto pdpt_idx = 0u; pdpt_idx < 512u && !base_address; ++pdpt_idx ) {
					const auto pdpt_pa = ( pml4_entry->hard.pfn << page_shift ) + ( pdpt_idx * sizeof( pdpte ) );
					const auto pdpt_entry = read_pt_entry<pdpte>( pdpt_pa );
					if ( !pdpt_entry || !pdpt_entry->hard.present ) continue;
					if ( pdpt_entry->hard.page_size ) continue;

					for ( auto pd_idx = 0u; pd_idx < 512u && !base_address; ++pd_idx ) {
						const auto pd_pa = ( pdpt_entry->hard.pfn << page_shift ) + ( pd_idx * sizeof( pde ) );
						const auto pd_entry = read_pt_entry<pde>( pd_pa );
						if ( !pd_entry || !pd_entry->hard.present ) continue;
						if ( pd_entry->hard.page_size ) continue;

						for ( auto pt_idx = 0u; pt_idx < 512u; ++pt_idx ) {
							const auto pt_pa = ( pd_entry->hard.pfn << page_shift ) + ( pt_idx * sizeof( pte ) );
							const auto pt_entry = read_pt_entry<pte>( pt_pa );
							if ( !pt_entry || !pt_entry->hard.present ) continue;

							auto candidate = ( static_cast< std::uint64_t >( pml4_idx ) << 39 ) |
								( static_cast< std::uint64_t >( pdpt_idx ) << 30 ) |
								( static_cast< std::uint64_t >( pd_idx ) << 21 ) |
								( static_cast< std::uint64_t >( pt_idx ) << page_shift );

							auto buffer = std::make_unique< std::uint8_t[ ] >( 0x1000 );
							if ( !memory::read_virtual( candidate, buffer.get( ), 0x1000 ) )
								continue;

							const auto* qwords = reinterpret_cast< const std::uint64_t* >( buffer.get( ) );
							if ( !std::any_of( qwords, qwords + 0x200, [ ]( std::uint64_t q ) { return q != 0; } ) )
								continue;

							const auto ctx = hyperspace_entries( candidate );
							if ( !ctx || !ctx->m_phys_addr ) continue;

							const auto expected_pa = ( pt_entry->hard.pfn << page_shift );
							if ( ctx->m_phys_addr != expected_pa ) continue;

							base_address = candidate;
							break;
						}
					}
				}
			}

			return base_address;
		}

		std::uint32_t find_free_pml4( bool high_address = false ) {
			const auto range_start = high_address ? 256u : 1u;
			const auto range_end = high_address ? 512u : 256u;
			for ( auto idx = range_start; idx < range_end; ++idx ) {
				pml4e entry {};
				if ( memory::read_physical( m_target_dtb + ( idx * sizeof( pml4e ) ),
					&entry, sizeof( entry ) ) && entry.hard.present )
					continue;
				return idx;
			}

			return static_cast< std::uint32_t >( -1 );
		}

		std::uint64_t get_pfn_addr( std::uint64_t base_address ) {
			static constexpr std::uint64_t mmpfn_size = 0x30;

			if ( !m_pfn_database ) {
				if ( !memory::read_system( m_mm_pfn_database, &m_pfn_database, sizeof( m_pfn_database ) ) )
					return { };
			}

			const auto phys_addr = translate_linear( base_address );
			const auto pfn = phys_addr >> page_shift;
			return m_pfn_database + pfn * mmpfn_size;
		}

		std::unique_ptr< std::uint8_t[ ] > get_pfn_data( std::uint64_t base_address ) {
			static constexpr std::uint64_t mmpfn_size = 0x30;

			if ( !m_pfn_database ) {
				if ( !memory::read_system( m_mm_pfn_database, &m_pfn_database, sizeof( m_pfn_database ) ) )
					return { };
			}

			const auto pfn_addr = get_pfn_addr( base_address );
			if ( !pfn_addr )
				return { };

			auto pfn_database = std::make_unique< std::uint8_t[ ] >( mmpfn_size );
			if ( !memory::read_system( pfn_addr, pfn_database.get( ), mmpfn_size ) )
				return { };

			return pfn_database;
		}

		bool configure_dtb_scan( std::uint64_t mm_pfn_database,
			std::uint64_t off_mmpfn_u4, std::uint64_t size_mmpfn ) {
			shared::mapping_info_t info {};
			if ( !mm_pfn_database || !size_mmpfn || !g_memory->query_mapping( info ) )
				return false;

			m_mm_pfn_database = mm_pfn_database;
			m_off_mmpfn_u4 = off_mmpfn_u4;
			m_size_mmpfn = size_mmpfn;
			m_pfn_database = 0;
			m_pfn_entry_page_va = 0;
			m_pfn_entry_page_pa = 0;
			m_physical_mask = info.physical_mask;
			m_scan_ranges.clear( );
			const auto count = ( std::min )( info.range_count,
				( std::min )( info.max_ranges, shared::max_mapping_ranges ) );
			for ( std::uint64_t i = 0; i < count; ++i ) {
				if ( info.ranges[ i ].size )
					m_scan_ranges.push_back( { info.ranges[ i ].base_pa, info.ranges[ i ].size } );
			}
			return m_physical_mask && !m_scan_ranges.empty( );
		}

		std::uint64_t find_dtb( std::uint64_t module_va, std::uint64_t peb_va = 0,
			std::chrono::milliseconds watchdog = std::chrono::seconds( 5 ) ) {
			if ( m_scan_ranges.empty( ) || !m_mm_pfn_database || !m_size_mmpfn ) return 0;
			const auto deadline = std::chrono::steady_clock::now( ) + watchdog;
			const virt_addr_t module { module_va };
			return scan_physical_memory( module, peb_va, deadline );
		}

		bool get_pfn_pte_frame( std::uint64_t pfn, std::uint64_t& pte_frame ) {
			if ( !m_pfn_database ) {
				if ( !memory::read_system( m_mm_pfn_database, &m_pfn_database, sizeof( m_pfn_database ) ) )
					return false;
			}
			if ( m_pfn_database > UINT64_MAX - m_off_mmpfn_u4 )
				return false;
			const auto database_u4 = m_pfn_database + m_off_mmpfn_u4;
			if ( pfn > ( UINT64_MAX - database_u4 ) / m_size_mmpfn )
				return false;

			const auto u4_va = database_u4 + pfn * m_size_mmpfn;
			const auto page_va = u4_va & ~page_4kb_mask;
			if ( page_va != m_pfn_entry_page_va ) {
				const auto page_pa = translate_linear( page_va );
				if ( !page_pa || !is_address_valid( page_pa, page_4kb_size ) )
					return false;
				m_pfn_entry_page_va = page_va;
				m_pfn_entry_page_pa = page_pa;
			}

			std::uint64_t u4 = 0;
			const auto u4_pa = m_pfn_entry_page_pa + ( u4_va & page_4kb_mask );
			if ( !read_scan_entry( u4_pa, u4 ) ) return false;
			pte_frame = u4 & 0x000000FFFFFFFFFFULL;
			return true;
		}

		struct scan_range_t { std::uint64_t base; std::uint64_t size; };
		std::vector< scan_range_t > m_scan_ranges;
		std::uint64_t m_physical_mask {};
		std::uint64_t m_mm_pfn_database {};
		std::uint64_t m_pfn_database {};
		std::uint64_t m_off_mmpfn_u4 {};
		std::uint64_t m_size_mmpfn {};
		std::uint64_t m_pfn_entry_page_va {};
		std::uint64_t m_pfn_entry_page_pa {};

	private:
		bool is_address_valid( std::uint64_t pa, std::size_t size ) const {
			if ( !size || !m_physical_mask || pa > UINT64_MAX - ( size - 1 ) ) return false;
			const auto end = pa + size - 1;
			if ( ( pa & ~m_physical_mask ) || ( end & ~m_physical_mask ) ) return false;
			for ( const auto& range : m_scan_ranges ) {
				if ( !range.size || range.base > UINT64_MAX - ( range.size - 1 ) ) continue;
				if ( pa >= range.base && end <= range.base + range.size - 1 ) return true;
			}
			return false;
		}

		template< typename entry_t >
		bool read_scan_entry( std::uint64_t pa, entry_t& entry ) const {
			return is_address_valid( pa, sizeof( entry ) ) &&
				memory::read_physical( pa, &entry, sizeof( entry ) );
		}

		bool validate_chain( std::uint64_t cr3, const virt_addr_t& va ) const {
			pml4e pml4 {};
			if ( !read_scan_entry( cr3 + va.pml4e_index * sizeof( pml4e ), pml4 ) ||
				!pml4.hard.present || !pml4.hard.pfn ) return false;
			pdpte pdpt {};
			if ( !read_scan_entry( ( pml4.hard.pfn << 12 ) + va.pdpte_index * sizeof( pdpte ), pdpt ) ||
				!pdpt.hard.present || !pdpt.hard.pfn ) return false;
			if ( pdpt.hard.page_size ) return true;
			pde pd {};
			if ( !read_scan_entry( ( pdpt.hard.pfn << 12 ) + va.pde_index * sizeof( pde ), pd ) ||
				!pd.hard.present || !pd.hard.pfn ) return false;
			if ( pd.hard.page_size ) return true;
			pte pt {};
			return read_scan_entry( ( pd.hard.pfn << 12 ) + va.pte_index * sizeof( pte ), pt ) &&
				pt.hard.present && pt.hard.pfn;
		}

		std::uint64_t scan_physical_memory( const virt_addr_t& module_va, std::uint64_t peb_va,
			std::chrono::steady_clock::time_point deadline ) {
			for ( const auto& range : m_scan_ranges ) {
				if ( range.base > UINT64_MAX - range.size ) continue;
				const auto end = range.base + range.size;
				for ( auto candidate = ( range.base + page_4kb_mask ) & ~page_4kb_mask;
					candidate < end && end - candidate >= page_4kb_size; candidate += page_4kb_size ) {
					const auto candidate_pfn = candidate >> 12;
					if ( ( candidate_pfn & 0xFFF ) == 0 &&
						std::chrono::steady_clock::now( ) >= deadline )
						return 0;
					std::uint64_t pte_frame = 0;
					if ( !get_pfn_pte_frame( candidate_pfn, pte_frame ) || pte_frame != candidate_pfn ) continue;
					if ( !validate_chain( candidate, module_va ) ) continue;
					if ( peb_va && !validate_chain( candidate, virt_addr_t { peb_va } ) ) continue;

					const auto previous = m_target_dtb;
					m_target_dtb = candidate;
					const auto module_pa = translate_linear( module_va.value );
					m_target_dtb = previous;
					std::uint16_t magic = 0;
					if ( !module_pa || !read_scan_entry( module_pa, magic ) || magic != 0x5A4D ) continue;
					return candidate;
				}
			}
			return 0;
		}
	};
}
