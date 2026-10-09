#pragma once

namespace exception {

	void ( *ke_bugcheck_ex_org )(
		unsigned int code,
		std::uint64_t p1,
		std::uint64_t p2,
		std::uint64_t p3,
		std::uint64_t p4
		) = nullptr;

	bool is_emulated_address( std::uint64_t address ) {
		return emulator::find( address ) != nullptr;
	}

	void ke_bugcheck_ex(
		unsigned int code,
		std::uint64_t p1,
		std::uint64_t p2,
		std::uint64_t p3,
		std::uint64_t p4 ) {

		const auto rsp = reinterpret_cast< std::uint64_t >( _AddressOfReturnAddress( ) );

		const auto prcb = kernel::ke_get_current_prcb( );

		m_nested_bugcheck = is_bugcheck_active( ) ||
			( prcb && prcb->m_dpc_routine_active ) ||
			InterlockedCompareExchange( &m_recovery_active, 1, 0 ) != 0;

		if ( code == 0x109 ) {
			if ( is_unbacked_thread( ) )
				terminate_unbacked_thread_force( );
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
			return;
		}

		if ( !is_safe_bugcheck_context( ) ) {
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
			return;
		}

		switch ( code ) {
		case 0x3b: {
			if ( is_emulated_address( p2 ) ) {
				ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
				break;
			}
			//if ( p1 == 0xc000001d ) {
			//	const auto ctx = reinterpret_cast< PCONTEXT >( p3 );
			//	if ( ctx && hook::on_exception( ctx ) ) {
			//		kernel::rtl_restore_context( ctx, nullptr );
			//		__assume( false );
			//	}
			//	unwind_syscall( rsp );
			//	unwind_frame( rsp );
			//}
			unwind_syscall( rsp );
			if ( !is_ntoskrnl_address( p2 ) )
				unwind_frame( rsp );
			terminate_unbacked_thread( );
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
		} break;
		case 0x139: {
			if ( p1 == 2 )
				unwind_from_trap_frame( p2 );
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
		} break;
		case 0xe9: {
			unwind_frame( rsp );
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
		} break;
		case 0x50: {
			if ( is_emulated_address( p1 ) ) {
				ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
				break;
			}
			unwind_frame( rsp );
			terminate_unbacked_thread( );
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
		} break;
		case 0x109: {
			unwind_frame( rsp );
			terminate_unbacked_thread( );
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
		} break;
		case 0xf7: {
			unwind_frame( rsp );
			terminate_unbacked_thread( );
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
		} break;
		case 0xd1: {
			unwind_frame( rsp );
			terminate_unbacked_thread( );
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
		} break;
		case 0x7f: {
			unwind_frame( rsp );
			terminate_unbacked_thread( );
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
		} break;
		case 0x5: {
			unwind_frame( rsp );
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
		} break;
		case 0x101: {
			unwind_clock_watchdog( rsp, p3 );
			unwind_frame( rsp );
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
		} break;
		case 0x411: {
			unwind_frame( rsp );
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
		} break;
		case 0xfc: {
			if ( is_emulated_address( p1 ) ) {
				ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
				break;
			}

			paging::pt_entries_t entries;
			if ( !m_nested_bugcheck &&
				 paging::hyperspace_entries( entries, p1 ) &&
				 !entries.m_pde.hard.page_size &&
				 entries.m_pte.hard.present &&
				 entries.m_pte.hard.no_execute ) {

				pte fixed = entries.m_pte;
				fixed.hard.no_execute = 0;
				const auto pt_phys =
					( entries.m_pde.hard.pfn << paging::page_shift ) +
					( virt_addr_t{ p1 }.pte_index * sizeof( pte ) );
				if ( paging::dpm::write_physical( pt_phys, &fixed, sizeof( pte ) ) ) {
					kernel::ke_flush_entire_tb( true, true );
					InterlockedExchange( &m_recovery_active, 0 );
					return;
				}
			}

			unwind_syscall( rsp );
			unwind_frame( rsp );
			terminate_unbacked_thread( );
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
		} break;
		case 0xa: {
			if ( is_unbacked_kernel_address( p4 ) || is_ntoskrnl_address( p4 ) )
				unwind_frame( rsp );
			terminate_unbacked_thread( );
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
		} break;

		case 0xbe: {
			// No handling to do - complying is the only option, the fault is already
			// fatal. What we do need is to name the writer. p1 is the address that was
			// written, p2 the PTE, p3 the trap frame, and the faulting instruction is
			// therefore in the trap frame's Rip. Classify all three so the next dump
			// says which image wrote, instead of leaving it to be inferred from a
			// broken unwind nothing can resolve.
			const auto classify = [ ]( std::uint64_t address ) -> const char* {
				if ( !address ) return "0";
				if ( is_own_address( address ) ) return "crisis-spoofer";
				if ( is_ntoskrnl_address( address ) ) return "ntoskrnl";
				if ( is_unbacked_kernel_address( address ) ) return "unbacked";
				return "module";
			};

			std::uint64_t fault_rip = 0;
			static const auto s_trap_rip =
				pdb::get_member( obf( "_KTRAP_FRAME" ), obf( "Rip" ) );
			if ( p3 && s_trap_rip && kernel::mm_is_address_valid(
					reinterpret_cast< void* >( p3 ) ) ) {
				__try {
					fault_rip = *reinterpret_cast< const std::uint64_t* >(
						reinterpret_cast< const std::uint8_t* >( p3 ) + s_trap_rip );
				}
				__except ( EXCEPTION_EXECUTE_HANDLER ) {
					fault_rip = 0;
				}
			}

			disk::print( obf( "be: wrote 0x%llx [%s] pte=0x%llx rip=0x%llx [%s] trap=0x%llx" ),
				p1, classify( p1 ), p2, fault_rip, classify( fault_rip ), p3 );

			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
		} break;
		case 0x1e: {
			if ( is_emulated_address( p2 ) ) {
				ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
				break;
			}
			if ( p1 == 0xc000001dul &&
				 ( hook::find_entry( p2 ) ||
				   hook::find_entry_by_handler( p2 ) ) )
				unwind_syscall( rsp );
			unwind_frame( rsp );
			terminate_unbacked_thread( );
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
		} break;
		default:
			ke_bugcheck_ex_org( code, p1, p2, p3, p4 );
			break;
		}
	}

	bool kdp_report(
		std::uint64_t trap_frame,
		std::uint64_t exc_frame,
		PEXCEPTION_RECORD exc_rec,
		PCONTEXT ctx,
		KPROCESSOR_MODE processor_mode,
		bool first_chance )
	{
		if ( !exc_rec || !ctx )
			return false;

		if ( processor_mode != KernelMode )
			return false;

		if ( is_own_address( static_cast< std::uint64_t >( ctx->Rip ) ) )
			return false;

		if ( communication::on_communication_callback( ctx, exc_rec, trap_frame ) )
			return true;

		const auto region = current_stack_region( );
		if ( region.m_low &&
			 static_cast< std::uint64_t >( ctx->Rsp ) < region.m_low + 0x2000 )
			return false;

		if ( ctx->Dr6 & 0x1 ) {
			if ( emulator::on_seed_trap( ctx ) )
				return true;

			if ( emulator::on_return( ctx ) )
				return true;
		}

		if ( exc_rec->ExceptionCode == 0x80000004u ) {
			if ( emulator::on_single_step( ctx ) )
				return true;
		}

		const auto rip = static_cast< std::uintptr_t >( ctx->Rip );
		if ( dtb::on_cr3_fault( ctx, exc_rec ) )
			return true;

		if ( exc_rec->ExceptionCode == STATUS_PRIVILEGED_INSTRUCTION ||
			 exc_rec->ExceptionCode == STATUS_ILLEGAL_INSTRUCTION ) {
			if ( hook::find_entry( static_cast< std::uint64_t >( ctx->Rip ) ) )
				return false;
		}

		if ( exc_rec->ExceptionCode == STATUS_ACCESS_VIOLATION &&
			 exc_rec->NumberParameters >= 2 ) {
			const auto fault_type  = exc_rec->ExceptionInformation[ 0 ];
			const auto faulting_va = exc_rec->ExceptionInformation[ 1 ];
			if ( fault_type == EXCEPTION_EXECUTE_FAULT ) {
				if ( hook::on_execute_fault( faulting_va, ctx ) )
					return true;
				return false;
			}
		}

		if ( exc_rec->ExceptionCode == STATUS_PRIVILEGED_INSTRUCTION )
			return patchguard::on_privileged_instruction( ctx, rip );

		//std::uint64_t image_base = 0;
		//if ( !lookup_function_entry( static_cast< std::uint64_t >( ctx->Rip ), &image_base ) &&
		//	 patchguard::claim_dispatcher_exception( ctx ) )
		//	return true;

		return false;
	}

	// KdTrap(ExceptionFrame, TrapFrame, ExceptionRecord, ContextRecord, FirstChance, a6)
	// KiDispatchException passes the exception frame FIRST - these were swapped.
	char kd_trap(
		std::uint64_t     exc_frame,
		std::uint64_t     trap_frame,
		PEXCEPTION_RECORD exc_rec,
		PCONTEXT          ctx,
		char              first_chance,
		char              a6 )
	{
		UNREFERENCED_PARAMETER( a6 );

		handler_enter( );

		char result = false;

		if ( exc_rec && ctx &&
			 exc_rec->ExceptionCode == STATUS_ILLEGAL_INSTRUCTION &&
			 !is_own_address( static_cast< std::uint64_t >( ctx->Rip ) ) ) {
			if ( emulator::on_seed_trap( ctx ) ||
				 hook::on_exception( ctx ) ||
				 hook::find_entry_by_handler( ctx->Rip ) )
				result = true;
		}

		if ( !result &&
			 kdp_report( trap_frame, exc_frame, exc_rec, ctx, KernelMode, first_chance ) )
			result = true;

		if ( !result && exc_rec && ctx &&
			 exc_rec->ExceptionCode == 0x80000003u &&
			 exc_rec->NumberParameters &&
			 ( exc_rec->ExceptionInformation[ 0 ] == 1 ||
			   ( exc_rec->ExceptionInformation[ 0 ] >= 3 &&
			     exc_rec->ExceptionInformation[ 0 ] <= 5 ) ) ) {
			++ctx->Rip;
			result = true;
		}

		handler_leave( );
		return result;
	}

	// KdTrap(ExceptionFrame, TrapFrame, ExceptionRecord, ContextRecord, FirstChance, a6)
	char kd_stub(
		std::uint64_t     exc_frame,
		std::uint64_t     trap_frame,
		PEXCEPTION_RECORD exc_rec,
		PCONTEXT          ctx,
		std::uint8_t      first_chance,
		std::uint8_t      a6 )
	{
		UNREFERENCED_PARAMETER( a6 );

		handler_enter( );

		char result = false;

		if ( exc_rec && ctx &&
			 exc_rec->ExceptionCode == STATUS_ILLEGAL_INSTRUCTION &&
			 !is_own_address( static_cast< std::uint64_t >( ctx->Rip ) ) ) {
			if ( emulator::on_seed_trap( ctx ) ||
				 hook::on_exception( ctx ) ||
				 hook::find_entry_by_handler( ctx->Rip ) )
				result = true;
		}

		if ( !result &&
			 kdp_report( trap_frame, exc_frame, exc_rec, ctx, KernelMode, first_chance ) )
			result = true;

		if ( !result && exc_rec && ctx &&
			 exc_rec->ExceptionCode == 0x80000003u &&
			 exc_rec->NumberParameters &&
			 ( exc_rec->ExceptionInformation[ 0 ] == 1 ||
			   ( exc_rec->ExceptionInformation[ 0 ] >= 3 &&
			     exc_rec->ExceptionInformation[ 0 ] <= 5 ) ) ) {
			++ctx->Rip;
			result = true;
		}

		handler_leave( );
		return result;
	}

	std::uint64_t resolve_ke_bugcheck_ex( ) {
		static const auto s_mca      = pdb::get_symbol( obf( "KiMcaDeferredRecoveryService" ) );
		static const auto s_bugcheck = pdb::get_symbol( obf( "KeBugCheckEx" ) );
		const auto fn_va = s_mca;
		if ( !fn_va )
			return s_bugcheck;

		const auto base = kernel::m_image_base;
		const auto size = kernel::m_image_size;

		const auto* p = reinterpret_cast< const std::uint8_t* >( fn_va );
		constexpr std::size_t max_walk = 64;

		for ( std::size_t i = 0; i < max_walk; ++i ) {
			if ( p[ i ] != 0xE9 )
				continue;

			const auto jmp_va = fn_va + i;
			const auto rel32  = *reinterpret_cast< const std::int32_t* >( p + i + 1 );
			const auto target = static_cast< std::uint64_t >(
				static_cast< std::int64_t >( jmp_va + 5 ) + rel32 );

			if ( base && size && target >= base && target < base + size )
				return target;
		}

		return s_bugcheck;
	}

	bool ( *ke_freeze_execution_org )(
		std::uint64_t,
		std::uint64_t ) = nullptr;

	bool ke_freeze_execution(
		std::uint64_t a1,
		std::uint64_t a2 )
	{
		if ( is_debugger_present( ) || is_bugcheck_active( ) )
			return ke_freeze_execution_org( a1, a2 );

		return false;
	}

	void claim_user_exception_route( ) {
		const auto flag = pdb::get_symbol( obf( "KdIgnoreUmExceptions" ) );
		if ( !flag ) {
			disk::print( obf( "exception: KdIgnoreUmExceptions not resolved\n" ) );
			return;
		}

		*reinterpret_cast< volatile std::uint8_t* >( flag ) = 0;

		disk::print( obf( "exception: KdIgnoreUmExceptions cleared at 0x%llx\n" ), flag );
	}

	bool create( ) {
		// KdIgnoreUmExceptions is exactly what the name says: while it is set, user-mode
		// exceptions never reach KdTrap. On a boot with no kernel debugger it is set, so
		// the ring-3 lgdt carrying every communication request faults straight through to
		// KiUserExceptionDispatcher, this handler never sees it, and the caller dies on an
		// unhandled exception - which is why the channel works on some boots and not
		// others. Nothing else in this driver ever touched it. Clearing it makes the route
		// deterministic instead of a property of the boot.
		claim_user_exception_route( );
		static const auto kd_stub_addr = pdb::get_symbol( obf( "KdpStub" ) );
		static const auto kd_trap_addr = pdb::get_symbol( obf( "KdpTrap" ) );
		const auto ke_freeze_execution_addr = pdb::get_symbol( obf( "KeFreezeExecution" ) );

		m_watched[ 0 ].m_base = module::find_base(
			obf( L"nvlddmkm.sys" ), &m_watched[ 0 ].m_size );
		m_watched[ 1 ].m_base = module::find_base(
			obf( L"dxgmms2.sys" ), &m_watched[ 1 ].m_size );

		// Resolved once here and never re-resolved: this list is consulted from the
		// bugcheck path, where a symbol lookup can fail and silently drop the guard.
		// Only genuine thread entry points belong here - a thread's start address can
		// never be a DPC/APC dispatcher routine, so matching those only yields
		// false positives that skip terminating a thread we should have terminated.
		m_dispatchers[ 0 ] = pdb::get_symbol( obf( "ExpWorkerThread" ) );
		m_dispatchers[ 1 ] = pdb::get_symbol( obf( "KiIdleLoop" ) );

		patchguard::create( );
		dtb::prepare( );

		if ( const auto halt = pdb::get_symbol( obf( "HaliHaltSystem" ) ) ) {
			if ( paging::set_page_protection( halt, page_protection::readwrite_execute ) )
				disk::print( obf( "enabled write access -> 0x%llx" ), halt );
			else
				disk::print( obf( "warning: could not enable write -> 0x%llx" ), halt );
		}

		auto ke_bugcheck_ex_addr = resolve_ke_bugcheck_ex( );
		auto state = paging::pph::create_hook( ke_bugcheck_ex_addr, &ke_bugcheck_ex_org );
		if ( !paging::pph::enable_hook( state, ke_bugcheck_ex ) ) return false;
		if ( !paging::pph::swap_to_hooked( state ) ) return false;

		ke_bugcheck_ex_addr = kernel::get_export( kernel::m_ntoskrnl_base, obf( "KeBugCheckEx" ) );
		state = paging::pph::create_hook( ke_bugcheck_ex_addr, &ke_bugcheck_ex_org );
		if ( !paging::pph::enable_hook( state, ke_bugcheck_ex ) ) return false;
		if ( !paging::pph::swap_to_hooked( state ) ) return false;

		state = paging::pph::create_hook( kd_stub_addr );
		if ( !state ) return false;
		if ( !paging::pph::enable_hook( state, kd_stub ) ) return false;
		if ( !paging::pph::swap_to_hooked( state ) ) return false;

		state = paging::pph::create_hook( kd_trap_addr );
		if ( !state ) return false;
		if ( !paging::pph::enable_hook( state, kd_trap ) ) return false;
		if ( !paging::pph::swap_to_hooked( state ) ) return false;

		//state = paging::pph::create_hook( ke_freeze_execution_addr, &ke_freeze_execution_org );
		//if ( !state ) return false;
		//if ( !paging::pph::enable_hook( state, ke_freeze_execution ) ) return false;
		//if ( !paging::pph::swap_to_hooked( state ) ) return false;

		return true;
	}
}
