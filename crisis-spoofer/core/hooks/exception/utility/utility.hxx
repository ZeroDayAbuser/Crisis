#pragma once

namespace exception {
	struct watched_module_t {
		std::uint64_t m_base {};
		std::uint64_t m_size {};
	};

	inline watched_module_t m_watched[ 2 ] {};

	inline std::uint64_t m_dispatchers[ 8 ] {};

	inline volatile long m_nested_bugcheck {};

	inline volatile long m_handler_depth[ 64 ] {};

	static constexpr std::uint64_t k_aux_stack_span = 0x4000;

	struct stack_region_t {
		std::uint64_t m_low {};
		std::uint64_t m_high {};
	};

	inline volatile long m_recovery_active {};

	bool thread_stack_bounds( std::uint64_t& low, std::uint64_t& high );
	bool is_executable_kernel_address( std::uint64_t address ) {
		if ( address < reinterpret_cast< std::uint64_t >( MmSystemRangeStart ) )
			return false;

		const auto image_base = kernel::m_image_base;
		const auto image_size = kernel::m_image_size;

		if ( address < image_base || address >= image_base + image_size )
			return false;

		const auto dos = reinterpret_cast< dos_header_t* >( image_base );
		if ( !dos->is_valid( ) )
			return false;

		const auto nt = reinterpret_cast< nt_headers_t* >( image_base + dos->m_lfanew );
		if ( !nt->is_valid( ) )
			return false;

		auto section = reinterpret_cast< section_header_t* >(
			reinterpret_cast< std::uintptr_t >( nt ) +
			nt->m_size_of_optional_header + 0x18 );

		for ( std::uint16_t i = 0; i < nt->m_number_of_sections; ++i ) {
			const auto section_start =
				reinterpret_cast< std::uint64_t >( dos ) + section[ i ].m_virtual_address;
			const auto section_size =
				static_cast< std::uint64_t >(
					max( section[ i ].m_virtual_size, section[ i ].m_size_of_raw_data ) );
			const auto section_end = section_start + section_size;

			if ( address < section_start || address >= section_end )
				continue;

			return ( section[ i ].m_characteristics & 0x20000000 ) != 0;
		}

		return false;
	}

	bool is_ntoskrnl_address( std::uint64_t address ) {
		const auto base = kernel::m_ntoskrnl_base;
		const auto size = kernel::m_ntoskrnl_size;
		if ( !base || !size )
			return false;
		return address >= base && address < base + size;
	}

	bool is_unbacked_kernel_address( std::uint64_t address ) {
		if ( address < reinterpret_cast< std::uint64_t >( MmSystemRangeStart ) )
			return false;
		if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( address ) ) )
			return false;
		return !is_ntoskrnl_address( address ) && !is_executable_kernel_address( address );
	}

	bool is_own_address( std::uint64_t address ) {
		const auto base = kernel::m_image_base;
		const auto size = kernel::m_image_size;
		if ( !base || !size )
			return false;
		return address >= base && address < base + size;
	}

	bool is_known_image_address( std::uint64_t address ) {
		return is_ntoskrnl_address( address ) || is_own_address( address );
	}

	void handler_enter( ) {
		const auto prcb = kernel::ke_get_current_prcb( );
		if ( !prcb || prcb->m_number >= 64 )
			return;

		InterlockedIncrement( &m_handler_depth[ prcb->m_number ] );
	}

	void handler_leave( ) {
		const auto prcb = kernel::ke_get_current_prcb( );
		if ( !prcb || prcb->m_number >= 64 )
			return;

		InterlockedDecrement( &m_handler_depth[ prcb->m_number ] );
	}

	bool handler_nested( ) {
		const auto prcb = kernel::ke_get_current_prcb( );
		if ( !prcb || prcb->m_number >= 64 )
			return false;

		return m_handler_depth[ prcb->m_number ] > 1;
	}

	stack_region_t current_stack_region( ) {
		stack_region_t region { };

		static const auto s_dpc_stack = pdb::get_member( obf( "_KPRCB" ), obf( "DpcStack" ) );
		static const auto s_isr_stack = pdb::get_member( obf( "_KPRCB" ), obf( "IsrStack" ) );

		const auto* prcb = reinterpret_cast< const std::uint8_t* >(
			kernel::ke_get_current_prcb( ) );
		const auto here = reinterpret_cast< std::uint64_t >( _AddressOfReturnAddress( ) );

		if ( prcb && s_dpc_stack ) {
			const auto base = *reinterpret_cast< const std::uint64_t* >( prcb + s_dpc_stack );
			if ( base && here < base && here >= base - k_aux_stack_span ) {
				region.m_low = base - k_aux_stack_span;
				region.m_high = base;
				return region;
			}
		}

		if ( prcb && s_isr_stack ) {
			const auto base = *reinterpret_cast< const std::uint64_t* >( prcb + s_isr_stack );
			if ( base && here < base && here >= base - k_aux_stack_span ) {
				region.m_low = base - k_aux_stack_span;
				region.m_high = base;
				return region;
			}
		}

		std::uint64_t low = 0, high = 0;
		thread_stack_bounds( low, high );
		region.m_low = low;
		region.m_high = high;
		return region;
	}

	bool is_watched_address( std::uint64_t address ) {
		for ( const auto& module : m_watched )
			if ( module.m_base && module.m_size &&
				address >= module.m_base && address < module.m_base + module.m_size )
				return true;

		return false;
	}

	std::uint32_t thread_start_offset( ) {
		static std::uint32_t cached = 0;
		static bool resolved = false;
		if ( resolved )
			return cached;

		resolved = true;

		cached = static_cast< std::uint32_t >(
			pdb::get_member( obf( "_ETHREAD" ), obf( "Win32StartAddress" ) ) );
		if ( !cached )
			cached = kernel::get_thread_start_address_offset( );

		return cached;
	}

	bool is_unbacked_thread( ) {
		const auto offset = thread_start_offset( );
		if ( !offset )
			return true;

		const auto thread = reinterpret_cast< const std::uint8_t* >(
			kernel::ps_get_current_thread( ) );
		if ( !thread )
			return true;

		const auto start = *reinterpret_cast< const std::uint64_t* >(
			thread + offset );

		if ( !start )
			return true;

		if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( start ) ) )
			return true;

		if ( is_own_address( start ) )
			return false;

		if ( is_watched_address( start ) )
			return false;

		module::info_t info { };
		if ( module::find_by_address( start, &info ) )
			return false;

		return true;
	}

	bool is_worker_thread( ) {
		const auto offset = thread_start_offset( );
		if ( !offset )
			return false;

		const auto thread = reinterpret_cast< const std::uint8_t* >(
			kernel::ps_get_current_thread( ) );
		if ( !thread )
			return false;

		const auto start = *reinterpret_cast< const std::uint64_t* >(
			thread + offset );
		if ( !start )
			return false;

		// ExpWorkerThread and KiIdleLoop, both resolved once in exception::create().
		// Do not re-resolve symbols here - this runs on the bugcheck path.
		for ( const auto dispatcher : m_dispatchers )
			if ( dispatcher && start == dispatcher )
				return true;

		// Independent of the cached list. If create() failed to resolve the pool entry
		// point, the cached check alone reports every worker as an ordinary thread.
		const auto worker = pdb::get_symbol( obf( "ExpWorkerThread" ) );
		return worker && start == worker;
	}

	// Termination is gone on purpose, and this is a baseline change rather than another
	// guard. Every call site already unwinds, so ending the thread added nothing it did
	// not already do - while PsTerminateSystemThread on a thread that turns out to be an
	// executive pool worker skips ExpWorkerThread's rundown and bugchecks 0xE9
	// ACTIVE_EX_WORKER_THREAD_TERMINATION (Arg1 = that ETHREAD), which is the crash we
	// keep coming back to. The guard meant to prevent it depended on ExpWorkerThread
	// resolving at init; when it did not, every worker looked ordinary and got killed.
	// No exception handler can end a kernel thread it does not own safely, so we stop
	// trying and record what would have been ended instead.
	void terminate_current_thread( ) {
		static volatile long logged = 0;
		if ( InterlockedCompareExchange( &logged, 1, 0 ) != 0 )
			return;

		disk::print( obf( "terminate suppressed ps=0x%llx worker=%u unbacked=%u" ),
			reinterpret_cast< std::uint64_t >( kernel::ps_get_current_thread( ) ),
			is_worker_thread( ) ? 1u : 0u,
			is_unbacked_thread( ) ? 1u : 0u );
	}

	void terminate_unbacked_thread( ) {
		if ( is_worker_thread( ) )
			return;
		if ( !is_unbacked_thread( ) )
			return;
		terminate_current_thread( );
	}

	void terminate_unbacked_thread_force( ) {
		if ( !is_unbacked_thread( ) )
			return;
		terminate_current_thread( );
	}

	std::uint64_t lookup_function_entry( std::uint64_t control_pc, std::uint64_t* image_base ) {
		static auto fn_address = 0ull;
		if ( !fn_address ) {
			fn_address = pdb::get_symbol( obf( "RtlLookupFunctionEntry" ) );
			if ( !fn_address ) return 0;
		}

		using function_t = runtime_function_t * ( * )( std::uint64_t, std::uint64_t*, void* );
		return reinterpret_cast< std::uint64_t >(
			reinterpret_cast< function_t >( fn_address )(
				control_pc, image_base, nullptr ) );
	}

	bool is_valid_unwind_target( std::uint64_t address ) {
		if ( !is_known_image_address( address ) )
			return false;

		if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( address ) ) )
			return false;

		std::uint64_t image_base = 0;
		const auto entry = lookup_function_entry( address, &image_base );
		if ( !entry || !image_base )
			return false;

		if ( image_base != kernel::m_ntoskrnl_base &&
			image_base != kernel::m_image_base )
			return false;

		const auto* fn = reinterpret_cast< const runtime_function_t* >( entry );
		if ( !fn->m_unwind_data )
			return false;

		const auto begin = image_base + fn->m_begin_address;
		const auto end = image_base + fn->m_end_address;
		return address >= begin && address < end;
	}

	bool is_module_backed_frame( std::uint64_t address ) {
		if ( !address )
			return false;

		std::uint64_t image_base = 0;
		const auto entry = lookup_function_entry( address, &image_base );
		if ( !entry || !image_base )
			return false;

		const auto* fn = reinterpret_cast< const runtime_function_t* >( entry );
		if ( !fn->m_unwind_data )
			return false;

		const auto begin = image_base + fn->m_begin_address;
		const auto end = image_base + fn->m_end_address;
		if ( address < begin || address >= end )
			return false;

		module::info_t info { };
		return module::find_by_address( address, &info );
	}

	bool restore_validated_frame(
		std::uint64_t rip,
		std::uint64_t rsp,
		bool          module_backed = false ) {
		static const auto s_restore = pdb::get_symbol( obf( "RtlRestoreContext" ) );

		if ( !s_restore || !rsp || ( rsp & 7 ) != 0 )
			return false;

		if ( KeGetCurrentIrql( ) != PASSIVE_LEVEL )
			return false;

		if ( m_nested_bugcheck || handler_nested( ) )
			return false;

		if ( is_worker_thread( ) )
			return false;

		if ( !is_unbacked_thread( ) )
			return false;

		if ( module_backed ? !is_module_backed_frame( rip ) : !is_valid_unwind_target( rip ) )
			return false;

		{
			std::uint64_t stack_low = 0, stack_high = 0;
			thread_stack_bounds( stack_low, stack_high );
			if ( !stack_low || !stack_high )
				return false;

			if ( rsp < stack_low + 0x100 || rsp >= stack_high )
				return false;

			if ( rsp <= reinterpret_cast< std::uint64_t >( _AddressOfReturnAddress( ) ) )
				return false;
		}

		//{
		//	std::uint64_t image_base = 0;
		//	if ( !lookup_function_entry( rip, &image_base ) || !image_base )
		//		return false;

		//	if ( image_base != kernel::m_ntoskrnl_base &&
		//		 image_base != kernel::m_image_base )
		//		return false;
		//}

		const auto prcb = kernel::ke_get_current_prcb( );
		if ( !prcb || prcb->m_dpc_routine_active )
			return false;

		CONTEXT ctx = { };
		ctx.ContextFlags = CONTEXT_FULL;
		kernel::rtl_capture_context( &ctx );

		ctx.ContextFlags = CONTEXT_FULL;
		ctx.Rip = rip;
		ctx.Rsp = rsp;

		using function_t = void( * )( PCONTEXT, PEXCEPTION_RECORD );
		reinterpret_cast< function_t >( s_restore )( &ctx, nullptr );
		return false;
	}

	void recover_from_stack( std::uint64_t scan_rsp ) {
		std::uint64_t stack_low = 0, stack_high = 0;
		thread_stack_bounds( stack_low, stack_high );
		if ( !stack_low || !stack_high )
			return;

		auto scan = ( scan_rsp + 7 ) & ~static_cast< std::uint64_t >( 7 );
		constexpr std::size_t max_entries = 256;

		for ( std::size_t i = 0;
			i < max_entries && scan + sizeof( std::uint64_t ) <= stack_high;
			++i, scan += sizeof( std::uint64_t ) )
		{
			const auto candidate = *reinterpret_cast< const std::uint64_t* >( scan );

			if ( !is_module_backed_frame( candidate ) )
				continue;

			restore_validated_frame( candidate, scan + sizeof( std::uint64_t ), true );
		}
	}

	void unwind_from_trap_frame( std::uint64_t trap_frame ) {
		static const auto s_rsp = pdb::get_member( obf( "_KTRAP_FRAME" ), obf( "Rsp" ) );
		if ( !s_rsp || !trap_frame )
			return;

		if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( trap_frame ) ) ||
			!kernel::mm_is_address_valid( reinterpret_cast< void* >(
				trap_frame + s_rsp + sizeof( std::uint64_t ) ) ) )
			return;

		const auto fault_rsp = *reinterpret_cast< const std::uint64_t* >(
			reinterpret_cast< const std::uint8_t* >( trap_frame ) + s_rsp );

		recover_from_stack( fault_rsp );
	}


	bool thread_stack_bounds( std::uint64_t& low, std::uint64_t& high ) {
		static const auto s_initial = pdb::get_member( obf( "_KTHREAD" ), obf( "InitialStack" ) );
		static const auto s_limit = pdb::get_member( obf( "_KTHREAD" ), obf( "StackLimit" ) );

		low = 0;
		high = 0;

		const auto thread = reinterpret_cast< std::uint64_t >(
			kernel::ps_get_current_thread( ) );
		if ( thread ) {
			if ( s_limit )
				low = *reinterpret_cast< const std::uint64_t* >( thread + s_limit );
			if ( s_initial )
				high = *reinterpret_cast< const std::uint64_t* >( thread + s_initial );
		}

		return low && high;
	}

	bool is_bugcheck_active( ) {
		static const auto s_active = pdb::get_symbol( obf( "KiBugCheckActive" ) );
		if ( !s_active )
			return false;

		return ( *reinterpret_cast< const volatile std::uint32_t* >( s_active ) & 3 ) != 0;
	}

	bool is_debugger_present( ) {
		static const auto s_not_present = pdb::get_symbol( obf( "KdDebuggerNotPresent" ) );

		if ( !s_not_present )
			return true;

		return *reinterpret_cast< const volatile std::uint8_t* >( s_not_present ) == 0;
	}

	bool is_safe_bugcheck_context( ) {
		if ( KeGetCurrentIrql( ) != PASSIVE_LEVEL )
			return false;

		return true;
	}

	bool is_syscall_return_address( std::uint64_t address ) {
		static const auto s_base = pdb::get_symbol( obf( "KiSystemServiceCopyEnd" ) );
		if ( !s_base )
			return false;
		return address > s_base && address < s_base + 0x800;
	}

	void unwind_syscall( std::uint64_t scan_rsp ) {
		std::uint64_t stack_low = 0, stack_high = 0;
		thread_stack_bounds( stack_low, stack_high );

		auto scan = ( scan_rsp + 7 ) & ~static_cast< std::uint64_t >( 7 );
		constexpr std::size_t max_entries = 256;

		for ( std::size_t i = 0;
			i < max_entries && scan + sizeof( std::uint64_t ) <= stack_high;
			++i, scan += sizeof( std::uint64_t ) )
		{
			const auto candidate = *reinterpret_cast< const std::uint64_t* >( scan );

			if ( !is_syscall_return_address( candidate ) )
				continue;

			restore_validated_frame( candidate, scan + sizeof( std::uint64_t ) );
		}
	}

	void unwind_clock_watchdog( std::uint64_t fault_rsp, std::uint64_t stuck_prcb ) {
		UNREFERENCED_PARAMETER( stuck_prcb );

		std::uint64_t stack_low = 0, stack_high = 0;
		thread_stack_bounds( stack_low, stack_high );

		if ( fault_rsp < stack_low || fault_rsp >= stack_high )
			return;

		auto scan = ( fault_rsp + sizeof( std::uint64_t ) + 7 )
			& ~static_cast< std::uint64_t >( 7 );
		constexpr std::size_t max_entries = 64;

		for ( std::size_t i = 0;
			i < max_entries && scan + sizeof( std::uint64_t ) <= stack_high;
			++i, scan += sizeof( std::uint64_t ) )
		{
			const auto candidate = *reinterpret_cast< const std::uint64_t* >( scan );

			if ( !is_ntoskrnl_address( candidate ) )
				continue;

			restore_validated_frame( candidate, scan + sizeof( std::uint64_t ) );
		}
	}

	void unwind_frame( std::uint64_t fault_rsp ) {
		std::uint64_t stack_low = 0, stack_high = 0;
		thread_stack_bounds( stack_low, stack_high );

		if ( fault_rsp >= stack_low && fault_rsp < stack_high ) {
			auto scan = ( fault_rsp + 7 ) & ~static_cast< std::uint64_t >( 7 );
			constexpr std::size_t max_entries = 32;

			for ( std::size_t i = 0;
				i < max_entries && scan + sizeof( std::uint64_t ) <= stack_high;
				++i, scan += sizeof( std::uint64_t ) )
			{
				const auto candidate = *reinterpret_cast< const std::uint64_t* >( scan );

				if ( !is_executable_kernel_address( candidate ) )
					continue;

				restore_validated_frame( candidate, scan + sizeof( std::uint64_t ) );
			}
		}
	}

}