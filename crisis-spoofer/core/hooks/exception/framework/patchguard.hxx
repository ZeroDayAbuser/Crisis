#pragma once

namespace exception {
	namespace patchguard {
		inline std::uint64_t m_patchguard_wp {};
		inline std::uint64_t m_patchguard_epilogue {};
		inline volatile long m_patchguard_logged {};
		inline volatile long m_abort_logged {};
		inline volatile long m_emulate_logged {};
		inline volatile long m_claim_logged {};
		inline volatile long m_dispatcher_claims {};

		bool is_dispatcher_frame( std::uint64_t address ) {
			if ( !address || !is_ntoskrnl_address( address ) )
				return false;

			std::uint64_t image_base = 0;
			const auto entry = lookup_function_entry( address, &image_base );
			if ( !entry || image_base != kernel::m_ntoskrnl_base )
				return false;

			const auto* fn = reinterpret_cast< const runtime_function_t* >( entry );
			if ( !fn->m_unwind_data )
				return false;

			const auto begin = image_base + fn->m_begin_address;
			const auto end   = image_base + fn->m_end_address;
			if ( address < begin || address >= end )
				return false;

			for ( const auto dispatcher : m_dispatchers )
				if ( dispatcher && begin == dispatcher )
					return true;

			return false;
		}

		bool is_call_return_site( std::uint64_t address ) {
			if ( address < 8 )
				return false;

			const auto* p = reinterpret_cast< const std::uint8_t* >( address );

			if ( !kernel::mm_is_address_valid( const_cast< std::uint8_t* >( p - 8 ) ) )
				return false;

			if ( p[ -5 ] == 0xE8 )
				return true;

			if ( p[ -2 ] == 0xFF && ( p[ -1 ] & 0x38 ) == 0x10 )
				return true;

			if ( p[ -3 ] == 0xFF && ( p[ -2 ] & 0x38 ) == 0x10 )
				return true;

			return false;
		}

		bool matches_signature(
			const std::uint8_t* data,
			const std::uint8_t* pattern,
			std::size_t        length )
		{
			for ( std::size_t i = 0; i < length; ++i )
				if ( data[ i ] != pattern[ i ] )
					return false;

			return true;
		}

		void resolve_patchguard_sites( ) {
			const auto base = kernel::m_ntoskrnl_base;
			const auto size = kernel::m_ntoskrnl_size;
			if ( !base || size < 0x4000 )
				return;

			static const std::uint8_t wp_signature[ ] = {
				0x41, 0x0F, 0x20, 0xC4, 0x49, 0x8B, 0xC4, 0x4C, 0x89, 0xA5,
				0x98, 0x00, 0x00, 0x00, 0x48, 0x0F, 0xBA, 0xF0, 0x10, 0x0F,
				0x22, 0xC0 };

			static const std::uint8_t epilogue_tail[ ] = {
				0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x5F, 0x5E,
				0x5D, 0xC3 };

			const auto* image = reinterpret_cast< const std::uint8_t* >( base );

			for ( std::size_t i = 0; i + sizeof( wp_signature ) <= size; ++i ) {
				if ( !matches_signature( image + i, wp_signature,
						 sizeof( wp_signature ) ) )
					continue;

				const auto write = i + sizeof( wp_signature ) - 3;
				m_patchguard_wp = base + write;

				constexpr std::size_t scan_span = 0x2000;
				for ( std::size_t j = write; j + sizeof( epilogue_tail ) <= size &&
						 j - write < scan_span; ++j ) {
					if ( !matches_signature( image + j, epilogue_tail,
							 sizeof( epilogue_tail ) ) )
						continue;

					auto start = j;
					if ( start >= 7 && image[ start - 7 ] == 0x48 &&
						 image[ start - 6 ] == 0x81 && image[ start - 5 ] == 0xC4 ) {
						start -= 7;

						if ( start >= 8 && image[ start - 8 ] == 0x48 &&
							 image[ start - 7 ] == 0x8B && image[ start - 6 ] == 0x9C &&
							 image[ start - 5 ] == 0x24 )
							start -= 8;
					}

					m_patchguard_epilogue = base + start;
					break;
				}

				break;
			}
		}

		std::uint64_t find_routine_epilogue( std::uint64_t from, std::uint64_t rsp ) {
			if ( !from || !rsp ||
				 !kernel::mm_is_address_valid( reinterpret_cast< void* >( from ) ) )
				return 0;

			std::uint64_t stack_low = 0, stack_high = 0;
			if ( !thread_stack_bounds( stack_low, stack_high ) )
				return 0;

			constexpr std::size_t k_max_scan = 0x2000;
			std::uint64_t         page_end   = from;

			for ( std::size_t i = 0; i + 32 < k_max_scan; ++i ) {
				const auto address = from + i;
				if ( address >= page_end ) {
					if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( address ) ) )
						break;
					page_end = ( address + 0x1000 ) & ~static_cast< std::uint64_t >( 0xFFF );
				}

				const auto* p = reinterpret_cast< const std::uint8_t* >( address );
				if ( *p != 0xC3 )
					continue;

				auto          back = address;
				std::size_t   pops = 0;
				std::uint64_t add  = 0;

				for ( std::size_t n = 0; n < 24 && back > from + 1; ++n ) {
					const auto* q = reinterpret_cast< const std::uint8_t* >( back );

					if ( back - 2 >= from && q[ -2 ] == 0x41 &&
						 q[ -1 ] >= 0x58 && q[ -1 ] <= 0x5F ) {
						back -= 2; ++pops; continue;
					}
					if ( q[ -1 ] >= 0x58 && q[ -1 ] <= 0x5F ) {
						back -= 1; ++pops; continue;
					}
					if ( q[ -1 ] == 0xC9 ) {
						back -= 1; ++pops; continue;
					}
					break;
				}

				if ( back < from + 8 )
					continue;

				const auto* r = reinterpret_cast< const std::uint8_t* >( back );
				if ( r[ -7 ] == 0x48 && r[ -6 ] == 0x81 && r[ -5 ] == 0xC4 ) {
					std::uint32_t imm = 0;
					std::memcpy( &imm, r - 4, sizeof( imm ) );
					add  = imm;
					back -= 7;
				}
				else if ( r[ -4 ] == 0x48 && r[ -3 ] == 0x83 && r[ -2 ] == 0xC4 ) {
					add  = r[ -1 ];
					back -= 4;
				}
				else if ( r[ -8 ] == 0x48 && r[ -7 ] == 0x8D && r[ -6 ] == 0xA4 &&
						  r[ -5 ] == 0x24 ) {
					std::uint32_t imm = 0;
					std::memcpy( &imm, r - 4, sizeof( imm ) );
					add  = imm;
					back -= 8;
				}
				else if ( r[ -5 ] == 0x48 && r[ -4 ] == 0x8D && r[ -3 ] == 0x64 &&
						  r[ -2 ] == 0x24 ) {
					add  = r[ -1 ];
					back -= 5;
				}
				else {
					continue;
				}

				const auto landed = rsp + add + ( pops * sizeof( std::uint64_t ) );
				if ( ( landed & 7 ) || landed < stack_low + 0x100 ||
					 landed > stack_high - sizeof( std::uint64_t ) )
					continue;

				const auto ret = *reinterpret_cast< const std::uint64_t* >( landed );

				if ( is_valid_unwind_target( ret ) )
					return back;

				if ( image::contains( reinterpret_cast< void* >( ret ) ) &&
					 kernel::mm_is_address_valid( reinterpret_cast< void* >( ret ) ) )
					return back;
			}

			return 0;
		}

		bool abort_patchguard_check( PCONTEXT ctx ) {
			if ( !ctx || !m_patchguard_wp || !m_patchguard_epilogue )
				return false;

			if ( static_cast< std::uint64_t >( ctx->Rip ) != m_patchguard_wp )
				return false;

			{
				std::uint64_t stack_low = 0, stack_high = 0;
				if ( !thread_stack_bounds( stack_low, stack_high ) )
					return false;

				const auto* p = reinterpret_cast< const std::uint8_t* >(
					m_patchguard_epilogue );

				std::uint64_t add      = 0;
				std::size_t   consumed = 0;
				if ( p[ 0 ] == 0x48 && p[ 1 ] == 0x81 && p[ 2 ] == 0xC4 ) {
					std::uint32_t imm = 0;
					std::memcpy( &imm, p + 3, sizeof( imm ) );
					add      = imm;
					consumed = 7;
				}
				else if ( p[ 0 ] == 0x48 && p[ 1 ] == 0x83 && p[ 2 ] == 0xC4 ) {
					add      = p[ 3 ];
					consumed = 4;
				}
				else {
					return false;
				}

				std::size_t pops = 0;
				auto        at   = consumed;
				while ( at + 1 < 64 ) {
					if ( p[ at ] == 0x41 && p[ at + 1 ] >= 0x58 && p[ at + 1 ] <= 0x5F ) {
						at += 2; ++pops; continue;
					}
					if ( p[ at ] >= 0x58 && p[ at ] <= 0x5F ) {
						at += 1; ++pops; continue;
					}
					if ( p[ at ] == 0xC9 ) {
						at += 1; ++pops; continue;
					}
					break;
				}

				if ( !pops || p[ at ] != 0xC3 )
					return false;

				const auto landed = static_cast< std::uint64_t >( ctx->Rsp ) + add +
					( pops * sizeof( std::uint64_t ) );
				if ( ( landed & 7 ) || landed < stack_low + 0x100 ||
					 landed > stack_high - sizeof( std::uint64_t ) )
					return false;

				if ( !is_valid_unwind_target(
						 *reinterpret_cast< const std::uint64_t* >( landed ) ) ) {
					if ( InterlockedCompareExchange( &m_patchguard_logged, 1, 0 ) == 0 )
						disk::print( obf( "patchguard epilogue 0x%llx failed the frame proof (rsp=0x%llx); not diverting" ),
							m_patchguard_epilogue, static_cast< std::uint64_t >( ctx->Rsp ) );
					return false;
				}
			}

			ctx->Rip = static_cast< DWORD64 >( m_patchguard_epilogue );
			ctx->Rax = 0xC0000225;

			if ( InterlockedCompareExchange( &m_patchguard_logged, 1, 0 ) == 0 )
				disk::print( obf( "aborted patchguard check wp=0x%llx epilogue=0x%llx" ),
					m_patchguard_wp, m_patchguard_epilogue );

			return true;
		}

		// Skipping a WP-clearing mov cr0 leaves CR0.WP set, so whatever write the caller
		// had queued behind it faults on a read-only page. At raised IRQL that is not a
		// recoverable violation - MiRaisedIrqlFault turns it into 0xBE (Arg4 0xF,
		// "write to a large page with the PTE Write bit clear"). So never classify a
		// cr0 write as foreign unless we can positively bound ntoskrnl first; without
		// both bounds is_ntoskrnl_address() cannot see nt at all, and PatchGuard's own
		// write-protect toggle reads as foreign and gets skipped.
		bool can_bound_ntoskrnl( ) {
			return kernel::m_ntoskrnl_base && kernel::m_ntoskrnl_size;
		}

		bool is_foreign_cr0_write( std::uint64_t rip ) {
			if ( !can_bound_ntoskrnl( ) || is_known_image_address( rip ) )
				return false;

			if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( rip ) ) ||
				 !kernel::mm_is_address_valid( reinterpret_cast< void* >( rip + 4 ) ) )
				return false;

			const auto* instruction = reinterpret_cast< const std::uint8_t* >( rip );
			if ( instruction[ 0 ] == 0x0F && instruction[ 1 ] == 0x22 &&
				 ( instruction[ 2 ] & 0x38 ) == 0 )
				return true;

			if ( ( instruction[ 0 ] & 0xF0 ) == 0x40 && instruction[ 1 ] == 0x0F &&
				 instruction[ 2 ] == 0x22 && ( instruction[ 3 ] & 0x38 ) == 0 )
				return true;

			return false;
		}

		bool is_foreign_cr0_memory_write( std::uint64_t rip ) {
			if ( !can_bound_ntoskrnl( ) || is_known_image_address( rip ) )
				return false;

			if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( rip ) ) ||
				 !kernel::mm_is_address_valid( reinterpret_cast< void* >( rip + 8 ) ) )
				return false;

			const auto* p = reinterpret_cast< const std::uint8_t* >( rip );
			const std::size_t at = ( p[ 0 ] & 0xF0 ) == 0x40 ? 1 : 0;

			if ( p[ at ] != 0x0F || p[ at + 1 ] != 0x22 || ( p[ at + 2 ] & 0x38 ) != 0 )
				return false;

			return ( p[ at + 2 ] & 0xC0 ) != 0xC0;
		}

		std::size_t foreign_cr0_write_length( std::uint64_t rip ) {
			if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( rip ) ) ||
				 !kernel::mm_is_address_valid( reinterpret_cast< void* >( rip + 8 ) ) )
				return 0;

			const auto* p = reinterpret_cast< const std::uint8_t* >( rip );
			std::size_t at = ( p[ 0 ] & 0xF0 ) == 0x40 ? 1 : 0;

			if ( p[ at ] != 0x0F || p[ at + 1 ] != 0x22 || ( p[ at + 2 ] & 0x38 ) != 0 )
				return 0;

			const auto mod = p[ at + 2 ] >> 6;
			const auto rm  = p[ at + 2 ] & 7;

			if ( mod == 3 )
				return at + 3;

			std::size_t length = at + 3;

			if ( rm == 4 ) {
				const auto sib = p[ length ];
				length += 1;
				if ( ( sib & 7 ) == 5 && mod == 0 )
					length += 4;
			}
			else if ( rm == 5 && mod == 0 ) {
				length += 4;
			}

			if ( mod == 1 )
				length += 1;
			else if ( mod == 2 )
				length += 4;

			return length;
		}

		bool skip_foreign_cr0_write( PCONTEXT ctx ) {
			if ( !ctx )
				return false;

			if ( ( ctx->SegCs & 3 ) != 0 )
				return false;

			const auto rip = static_cast< std::uint64_t >( ctx->Rip );
			if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( rip ) ) ||
				 !kernel::mm_is_address_valid( reinterpret_cast< void* >( rip + 4 ) ) )
				return false;

			const auto* instruction = reinterpret_cast< const std::uint8_t* >( rip );
			std::size_t index = 0;
			std::uint8_t rex = 0;
			if ( ( instruction[ 0 ] & 0xF0 ) == 0x40 ) {
				rex = instruction[ 0 ];
				index = 1;
			}

			if ( !( instruction[ index ] == 0x0F && instruction[ index + 1 ] == 0x22 &&
				 ( instruction[ index + 2 ] & 0xC0 ) == 0xC0 &&
				 ( instruction[ index + 2 ] & 0x38 ) == 0 ) )
				return false;

			if ( handler_nested( ) ) {
				ctx->Rip = rip + index + 3;
				if ( KeGetCurrentIrql( ) == PASSIVE_LEVEL &&
					 InterlockedCompareExchange( &m_emulate_logged, 1, 0 ) == 0 )
					disk::print( obf( "skipped nested mov cr0 rip=0x%llx" ), rip );
				return true;
			}

			const auto rm = ( instruction[ index + 2 ] & 0x07 ) |
				( ( rex & 0x01 ) ? 8 : 0 );

			std::uint64_t value = 0;
			switch ( rm ) {
			case 0:  value = ctx->Rax; break;
			case 1:  value = ctx->Rcx; break;
			case 2:  value = ctx->Rdx; break;
			case 3:  value = ctx->Rbx; break;
			case 4:  value = ctx->Rsp; break;
			case 5:  value = ctx->Rbp; break;
			case 6:  value = ctx->Rsi; break;
			case 7:  value = ctx->Rdi; break;
			case 8:  value = ctx->R8;  break;
			case 9:  value = ctx->R9;  break;
			case 10: value = ctx->R10; break;
			case 11: value = ctx->R11; break;
			case 12: value = ctx->R12; break;
			case 13: value = ctx->R13; break;
			case 14: value = ctx->R14; break;
			case 15: value = ctx->R15; break;
			default: return false;
			}

			ctx->Rip = rip + index + 3;

			if ( KeGetCurrentIrql( ) == PASSIVE_LEVEL &&
				 InterlockedCompareExchange( &m_emulate_logged, 1, 0 ) == 0 )
				disk::print( obf( "skipped unwritable mov cr0 rip=0x%llx value=0x%llx" ),
					rip, value );
			return true;
		}

		bool validate_frame_target( std::uint64_t rip, std::uint64_t rsp ) {
			if ( !rip || !rsp || ( rsp & 0xF ) != 0 )
				return false;

			std::uint64_t stack_low = 0, stack_high = 0;
			if ( !thread_stack_bounds( stack_low, stack_high ) )
				return false;

			if ( rsp < stack_low + 0x100 || rsp > stack_high )
				return false;

			if ( rsp <= reinterpret_cast< std::uint64_t >( _AddressOfReturnAddress( ) ) )
				return false;

			std::uint64_t image_base = 0;
			if ( !lookup_function_entry( rip, &image_base ) || !image_base )
				return false;

			return true;
		}

		bool claim_dispatcher_exception( PCONTEXT ctx ) {
			if ( !ctx )
				return false;

			if ( m_nested_bugcheck || handler_nested( ) )
				return false;

			if ( !is_unbacked_thread( ) )
				return false;

			if ( m_dispatcher_claims >= 64 )
				return false;

			const auto region = current_stack_region( );
			if ( !region.m_high )
				return false;

			auto scan = ( static_cast< std::uint64_t >( ctx->Rsp ) + 7 )
				& ~static_cast< std::uint64_t >( 7 );
			constexpr std::size_t max_entries = 256;

			for ( std::size_t i = 0;
				i < max_entries && scan + sizeof( std::uint64_t ) <= region.m_high;
				++i, scan += sizeof( std::uint64_t ) )
			{
				const auto candidate = *reinterpret_cast< const std::uint64_t* >( scan );

				if ( !is_dispatcher_frame( candidate ) )
					continue;

				if ( !is_call_return_site( candidate ) )
					continue;

				if ( !validate_frame_target( candidate,
						scan + sizeof( std::uint64_t ) ) )
					continue;

				if ( InterlockedCompareExchange( &m_claim_logged, 1, 0 ) == 0 )
					disk::print( obf( "claimed dynamic routine -> 0x%llx" ), candidate );

				InterlockedIncrement( &m_dispatcher_claims );

				ctx->Rip = candidate;
				ctx->Rsp = scan + sizeof( std::uint64_t );
				return true;
			}

			return false;
		}

		bool abort_deferred_protocol( PCONTEXT ctx ) {
			if ( !ctx )
				return false;

			if ( m_nested_bugcheck || handler_nested( ) )
				return false;

			const bool worker   = is_worker_thread( );
			const bool unbacked = is_unbacked_thread( );
			const auto irql     = KeGetCurrentIrql( );

			if ( worker || !unbacked || irql > APC_LEVEL ) {
				static volatile long s_declined {};
				if ( InterlockedCompareExchange( &s_declined, 1, 0 ) == 0 )
					disk::print( obf( "deferred protocol declined: worker=%u unbacked=%u irql=%u" ),
						worker ? 1u : 0u, unbacked ? 1u : 0u,
						static_cast< unsigned int >( irql ) );
				return false;
			}

			std::uint64_t stack_low = 0, stack_high = 0;
			thread_stack_bounds( stack_low, stack_high );
			if ( !stack_low || !stack_high )
				return false;

			const auto fault_rip = static_cast< std::uint64_t >( ctx->Rip );

			auto scan = ( static_cast< std::uint64_t >( ctx->Rsp ) + 7 )
				& ~static_cast< std::uint64_t >( 7 );
			constexpr std::size_t max_entries = 256;

			for ( std::size_t i = 0;
				i < max_entries && scan + sizeof( std::uint64_t ) <= stack_high;
				++i, scan += sizeof( std::uint64_t ) )
			{
				const auto candidate = *reinterpret_cast< const std::uint64_t* >( scan );

				if ( !is_dispatcher_frame( candidate ) )
					continue;

				if ( !is_call_return_site( candidate ) )
					continue;

				const auto target_rsp = scan + sizeof( std::uint64_t );

				if ( !validate_frame_target( candidate, target_rsp ) )
					continue;

				ctx->Rax = 0;
				ctx->Rip = candidate;
				ctx->Rsp = target_rsp;

				if ( KeGetCurrentIrql( ) == PASSIVE_LEVEL &&
					 InterlockedCompareExchange( &m_abort_logged, 1, 0 ) == 0 ) {
					std::uint64_t name_value = 0;
					std::uint8_t  name_length = 0;
					const auto name = pdb::get_symbol_name_by_address(
						candidate, &name_value, &name_length );

					disk::print( obf( "pg-abort rip=0x%llx -> 0x%llx %.*s+0x%llx" ),
						fault_rip,
						candidate,
						static_cast< int >( name_length ),
						name ? name : "?",
						name_value ? candidate - name_value : 0 );
				}

				return true;
			}

			return false;
		}

		bool on_privileged_instruction( PCONTEXT ctx, std::uint64_t rip ) {
			if ( !ctx )
				return false;

			if ( is_foreign_cr0_write( rip ) ) {
				if ( is_foreign_cr0_memory_write( rip ) ) {
					const auto length = foreign_cr0_write_length( rip );
					if ( length ) {
						ctx->Rip = rip + length;

						static volatile long s_skip_logged {};
						if ( InterlockedCompareExchange( &s_skip_logged, 1, 0 ) == 0 )
							disk::print( obf( "skipped memory-form mov cr0 rip=0x%llx" ), rip );

						return true;
					}
				}

				const auto epilogue = find_routine_epilogue( rip, ctx->Rsp );
				if ( epilogue ) {
					ctx->Rip = epilogue;

					if ( InterlockedCompareExchange( &m_patchguard_logged, 1, 0 ) == 0 )
						disk::print( obf( "pg divert wp=0x%llx -> epilogue=0x%llx" ),
							rip, epilogue );
					return true;
				}

				static std::uint64_t s_decline_logged {};
				if ( InterlockedCompareExchange(
					reinterpret_cast< volatile LONG* >( &s_decline_logged ), 1, 0 ) == 0 )
					disk::print( obf( "pg divert declined: no epilogue for wp=0x%llx (first 8 bytes %02x %02x %02x %02x %02x %02x %02x %02x)" ),
						rip,
						reinterpret_cast< const std::uint8_t* >( rip )[ 0 ],
						reinterpret_cast< const std::uint8_t* >( rip )[ 1 ],
						reinterpret_cast< const std::uint8_t* >( rip )[ 2 ],
						reinterpret_cast< const std::uint8_t* >( rip )[ 3 ],
						reinterpret_cast< const std::uint8_t* >( rip )[ 4 ],
						reinterpret_cast< const std::uint8_t* >( rip )[ 5 ],
						reinterpret_cast< const std::uint8_t* >( rip )[ 6 ],
						reinterpret_cast< const std::uint8_t* >( rip )[ 7 ] );
			}
			else {
				static std::uint64_t s_gate_logged {};
				if ( InterlockedCompareExchange(
					reinterpret_cast< volatile LONG* >( &s_gate_logged ), 1, 0 ) == 0 )
					disk::print( obf( "pg divert declined: gate refused 0x%llx" ), rip );
			}

			if ( abort_patchguard_check( ctx ) )
				return true;

			// Skipping a WP-clearing mov cr0 leaves CR0.WP set, so the write the caller
			// had queued behind it faults on a read-only page. At raised IRQL that is
			// not recoverable - MiRaisedIrqlFault turns it into 0xBE with Arg4 0xF, which
			// is exactly what was observed: PatchGuard repairing HaliHaltSystem
			// (nt+0x4BE190, .text, 2 MB read-only page) on an ExpWorkerThread at
			// DISPATCH_LEVEL, faulting in the memcpy loop that follows its CR0 clear.
			// The worker-thread branch below skipped without ever classifying the RIP,
			// so nt's own write-protect toggle was being skipped too. Skipping is only
			// defensible against a writer we have positively identified as foreign.
			const auto foreign = is_foreign_cr0_write( rip );

			if ( is_worker_thread( ) ) {
				if ( foreign && skip_foreign_cr0_write( ctx ) )
					return true;
			}

			if ( foreign ) {
				if ( abort_deferred_protocol( ctx ) )
					return true;

				terminate_unbacked_thread( );
				if ( skip_foreign_cr0_write( ctx ) )
					return true;

				return false;
			}

			return false;
		}

		void create( ) {
			resolve_patchguard_sites( );
		}
	}
}
