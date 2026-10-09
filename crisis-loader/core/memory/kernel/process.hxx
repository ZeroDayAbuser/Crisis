#pragma once

namespace process {
	static std::uint64_t m_links_offset = 0;
	static std::uint64_t m_pid_offset = 0;
	static std::uint64_t m_peb_offset = 0;
	static std::uint64_t m_section_base_offset = 0;
	static std::uint64_t m_dtb_offset = 0;
	static std::uint64_t m_thread_list_head_offset = 0;
	static std::uint64_t m_thread_list_entry_offset = 0;
	static std::uint64_t m_thread_cid_offset = 0;
	static std::uint64_t m_thread_cycle_time_offset = 0;
	static std::uint64_t m_thread_start_address_offset = 0;

	std::optional<std::uint64_t> get_thread_list_head_offset( ) {
		if ( !m_thread_list_head_offset )
			m_thread_list_head_offset = g_pdb->get_struct_member(
				obf( "_EPROCESS" ), obf( "ThreadListHead" ) );
		return m_thread_list_head_offset
			? std::optional<std::uint64_t>{ m_thread_list_head_offset } : std::nullopt;
	}

	std::optional<std::uint64_t> get_thread_list_entry_offset( ) {
		if ( !m_thread_list_entry_offset )
			m_thread_list_entry_offset = g_pdb->get_struct_member(
				obf( "_ETHREAD" ), obf( "ThreadListEntry" ) );
		return m_thread_list_entry_offset
			? std::optional<std::uint64_t>{ m_thread_list_entry_offset } : std::nullopt;
	}

	std::optional<std::uint64_t> get_thread_cid_offset( ) {
		if ( !m_thread_cid_offset )
			m_thread_cid_offset = g_pdb->get_struct_member(
				obf( "_ETHREAD" ), obf( "Cid" ) );
		return m_thread_cid_offset
			? std::optional<std::uint64_t>{ m_thread_cid_offset } : std::nullopt;
	}

	std::optional<std::uint64_t> get_thread_cycle_time_offset( ) {
		if ( !m_thread_cycle_time_offset )
			m_thread_cycle_time_offset = g_pdb->get_struct_member(
				obf( "_KTHREAD" ), obf( "CycleTime" ) );
		return m_thread_cycle_time_offset
			? std::optional<std::uint64_t>{ m_thread_cycle_time_offset } : std::nullopt;
	}

	std::optional<std::uint64_t> get_thread_start_address_offset( ) {
		if ( !m_thread_start_address_offset )
			m_thread_start_address_offset = g_pdb->get_struct_member(
				obf( "_ETHREAD" ), obf( "Win32StartAddress" ) );
		return m_thread_start_address_offset
			? std::optional<std::uint64_t>{ m_thread_start_address_offset } : std::nullopt;
	}

	std::optional<std::uint64_t> get_links_offset( ) {
		if ( !m_links_offset )
			m_links_offset = g_pdb->get_struct_member(
				obf( "_EPROCESS" ), obf( "ActiveProcessLinks" ) );
		return m_links_offset
			? std::optional<std::uint64_t>{ m_links_offset } : std::nullopt;
	}

	std::optional<std::uint64_t> get_pid_offset( ) {
		if ( !m_pid_offset )
			m_pid_offset = g_pdb->get_struct_member(
				obf( "_EPROCESS" ), obf( "UniqueProcessId" ) );
		return m_pid_offset
			? std::optional<std::uint64_t>{ m_pid_offset } : std::nullopt;
	}

	std::optional<std::uint64_t> get_peb_offset( ) {
		if ( !m_peb_offset )
			m_peb_offset = g_pdb->get_struct_member(
				obf( "_EPROCESS" ), obf( "Peb" ) );
		return m_peb_offset
			? std::optional<std::uint64_t>{ m_peb_offset } : std::nullopt;
	}

	std::optional<std::uint64_t> get_section_base_offset( ) {
		if ( !m_section_base_offset )
			m_section_base_offset = g_pdb->get_struct_member(
				obf( "_EPROCESS" ), obf( "SectionBaseAddress" ) );
		return m_section_base_offset
			? std::optional<std::uint64_t>{ m_section_base_offset } : std::nullopt;
	}

	std::optional<std::uint64_t> get_dtb_offset( ) {
		if ( !m_dtb_offset )
			m_dtb_offset = g_pdb->get_struct_member(
				obf( "_KPROCESS" ), obf( "DirectoryTableBase" ) );
		return m_dtb_offset
			? std::optional<std::uint64_t>{ m_dtb_offset } : std::nullopt;
	}

	std::optional<std::uint64_t>
		find_eprocess( std::uint32_t target_pid ) {
		const auto list_head = g_pdb->get_symbol_address(
			obf( "PsActiveProcessHead" )
		);
		if ( !list_head )
			return std::nullopt;

		const auto links_offset = get_links_offset( );
		const auto pid_offset = get_pid_offset( );
		if ( !links_offset || !pid_offset )
			return std::nullopt;

		std::uint64_t current = 0;
		if ( !memory::read_virtual( list_head, &current, sizeof( current ) ) )
			return std::nullopt;

		for ( auto idx = 0;
			current && current != list_head && idx < 0x1000;
			++idx ) {
			const auto eprocess = current - *links_offset;

			std::uint64_t cur_pid = 0;
			if ( !memory::read_virtual( eprocess + *pid_offset,
				&cur_pid, sizeof( cur_pid ) ) )
				break;

			if ( static_cast< std::uint32_t >( cur_pid ) == target_pid )
				return eprocess;

			if ( !memory::read_virtual( current, &current, sizeof( current ) ) )
				break;
		}

		logging::print( obf( "could not find process." ) );
		return std::nullopt;
	}

	std::optional<std::uint64_t>
		find_peb( std::uint64_t eprocess ) {
		if ( !eprocess ) return std::nullopt;

		const auto offset = get_peb_offset( );
		if ( !offset ) return std::nullopt;

		std::uint64_t peb = 0;
		if ( !memory::read_virtual( eprocess + *offset, &peb, sizeof( peb ) ) )
			return std::nullopt;

		return peb ? std::optional<std::uint64_t>{ peb } : std::nullopt;
	}

	std::optional<std::uint64_t>
		find_base_address( std::uint64_t eprocess ) {
		if ( !eprocess ) return std::nullopt;

		const auto offset = get_section_base_offset( );
		if ( !offset ) return std::nullopt;

		std::uint64_t base = 0;
		if ( !memory::read_virtual( eprocess + *offset, &base, sizeof( base ) ) )
			return std::nullopt;

		return base ? std::optional<std::uint64_t>{ base } : std::nullopt;
	}

	std::optional<std::uint64_t>
		find_dtb( std::uint64_t eprocess ) {
		if ( !eprocess ) return std::nullopt;

		const auto offset = get_dtb_offset( );
		if ( !offset ) return std::nullopt;

		std::uint64_t dtb = 0;
		if ( !memory::read_virtual( eprocess + *offset, &dtb, sizeof( dtb ) ) )
			return std::nullopt;

		const auto aligned = dtb & ~paging::page_4kb_mask;
		return aligned ? std::optional<std::uint64_t>{ aligned } : std::nullopt;
	}

	std::optional<std::uintptr_t>
		get_process_module( std::optional< std::uint64_t > eprocess, const wchar_t* module_name ) {
		if ( !module_name || !*module_name )
			return std::nullopt;

		const auto process_peb = find_peb( *eprocess );
		const auto process_dtb = find_dtb( *eprocess );
		if ( !process_peb || !process_dtb )
			return std::nullopt;

		const auto previous_dtb = g_paging->attach_process( *process_dtb );
		std::optional<std::uintptr_t> result = std::nullopt;

		peb64_t peb {};
		if ( memory::read_virtual( *process_peb, &peb, sizeof( peb ) ) &&
			peb.m_ldr ) {
			peb_ldr_data64_t ldr {};
			if ( memory::read_virtual( peb.m_ldr, &ldr, sizeof( ldr ) ) ) {
				const auto list_head = peb.m_ldr + offsetof(
					peb_ldr_data64_t, m_in_load_order_module_list );
				auto current = ldr.m_in_load_order_module_list.m_flink;

				for ( std::size_t count = 0;
					current && current != list_head && count < 0x1000;
					++count ) {
					ldr_data_table_entry64_t entry {};
					if ( !memory::read_virtual( current, &entry, sizeof( entry ) ) )
						break;

					const auto& base_name = entry.m_base_dll_name;
					if ( base_name.m_buffer && base_name.m_length &&
						!( base_name.m_length % sizeof( wchar_t ) ) &&
						base_name.m_length < MAX_PATH * sizeof( wchar_t ) ) {
						wchar_t name_buffer[ MAX_PATH ] {};
						if ( memory::read_virtual( base_name.m_buffer, name_buffer,
							base_name.m_length ) ) {
							name_buffer[ base_name.m_length / sizeof( wchar_t ) ] = L'\0';
							if ( !_wcsicmp( name_buffer, module_name ) ) {
								result = static_cast< std::uintptr_t >( entry.m_dll_base );
								break;
							}
						}
					}

					const auto next = entry.m_in_load_order_links.m_flink;
					if ( next == current )
						break;
					current = next;
				}
			}
		}

		if ( previous_dtb )
			g_paging->attach_process( *previous_dtb );
		return result;
	}
}