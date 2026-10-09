#pragma once

namespace exception {
	namespace communication {
		bool copy_from_user( std::uint64_t address, void* destination, std::size_t size ) {
			if ( !address || !destination || !size )
				return false;
			if ( address > k_user_max || address + size < address ||
				 address + size > k_user_max + 1 )
				return false;

			__try {
				std::memcpy( destination, reinterpret_cast< const void* >( address ), size );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
				InterlockedIncrement( &m_logged_fault );
				return false;
			}

			return true;
		}

		bool copy_to_user( std::uint64_t address, const void* source, std::size_t size ) {
			if ( !address || !source || !size )
				return false;
			if ( address > k_user_max || address + size < address ||
				 address + size > k_user_max + 1 )
				return false;

			__try {
				std::memcpy( reinterpret_cast< void* >( address ), source, size );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
				InterlockedIncrement( &m_logged_fault );
				return false;
			}

			return true;
		}

		bool decode_lgdt_operand( PCONTEXT ctx, std::uint64_t& operand_va, std::size_t& length ) {
			operand_va = 0;
			length = 0;

			const auto rip = static_cast< std::uint64_t >( ctx->Rip );
			if ( !rip || rip > k_user_max || rip + 16 > k_user_max + 1 )
				return false;

			__try {
				const auto* p = reinterpret_cast< const std::uint8_t* >( rip );

				const std::size_t at = ( ( p[ 0 ] & 0xF0 ) == 0x40 ) ? 1 : 0;
				const auto rex = ( at == 1 ) ? p[ 0 ] : std::uint8_t {};

				if ( p[ at ] != 0x0F || p[ at + 1 ] != 0x01 )
					return false;
				if ( ( ( p[ at + 2 ] >> 3 ) & 7 ) != 2 )
					return false;

				const auto mod = p[ at + 2 ] >> 6;
				const auto rm  = p[ at + 2 ] & 7;
				if ( mod == 3 )
					return false;

				std::size_t   cursor = at + 3;
				std::uint8_t  base   = 0;
				std::uint8_t  index  = 4;
				bool          no_base = false;
				bool          rip_based = false;
				std::int64_t  disp = 0;

				if ( rm == 4 ) {
					const auto sib = p[ cursor ];
					cursor += 1;
					base  = sib & 7;
					index = ( sib >> 3 ) & 7;
					if ( base == 5 && mod == 0 )
						no_base = true;
				}
				else if ( rm == 5 && mod == 0 ) {
					rip_based = true;
				}

				if ( rip_based || no_base ) {
					std::int32_t raw = 0;
					std::memcpy( &raw, p + cursor, sizeof( raw ) );
					disp = raw;
					cursor += 4;
				}
				else if ( mod == 1 ) {
					disp = static_cast< std::int8_t >( p[ cursor ] );
					cursor += 1;
				}
				else if ( mod == 2 ) {
					std::int32_t raw = 0;
					std::memcpy( &raw, p + cursor, sizeof( raw ) );
					disp = raw;
					cursor += 4;
				}

				std::uint64_t address = 0;

				if ( rip_based ) {
					address = rip + cursor;
				}
				else if ( no_base ) {
					address = 0;
				}
				else if ( rm == 4 ) {
					address = dtb::gpr( ctx, static_cast< std::uint8_t >(
						base | ( ( rex & 0x01 ) ? 8 : 0 ) ) );
					if ( index != 4 )
						address += dtb::gpr( ctx, static_cast< std::uint8_t >(
							index | ( ( rex & 0x02 ) ? 8 : 0 ) ) );
				}
				else {
					address = dtb::gpr( ctx, static_cast< std::uint8_t >(
						rm | ( ( rex & 0x01 ) ? 8 : 0 ) ) );
				}

				operand_va = address + static_cast< std::uint64_t >( disp );
				length = cursor;
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
				InterlockedIncrement( &m_logged_fault );
				return false;
			}

			return true;
		}

		std::uint64_t find_eprocess( std::uint64_t process_id ) {
			return dtb::find_eprocess( static_cast< std::uint32_t >( process_id ) );
		}

		bool is_linked( std::uint64_t eprocess ) {
			if ( !eprocess || !dtb::m_off_links )
				return false;

			const auto links = eprocess + dtb::m_off_links;

			__try {
				const auto flink = *reinterpret_cast< const std::uint64_t* >( links );
				const auto blink = *reinterpret_cast< const std::uint64_t* >( links + 8 );

				if ( !flink || !blink || flink == links || blink == links )
					return false;

				if ( *reinterpret_cast< const std::uint64_t* >( flink + 8 ) != links )
					return false;

				if ( *reinterpret_cast< const std::uint64_t* >( blink ) != links )
					return false;
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
				return false;
			}

			return true;
		}

		std::uint64_t stored_dtb( std::uint64_t eprocess ) {
			if ( !eprocess || !dtb::m_off_dtb )
				return 0;

			std::uint64_t value = 0;

			__try {
				value = *reinterpret_cast< const std::uint64_t* >(
					eprocess + dtb::m_off_pcb + dtb::m_off_dtb );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {
				return 0;
			}

			return value;
		}

		std::uint64_t resolved_dtb( std::uint64_t eprocess ) {
			const auto direct = dtb::read_object_dtb( eprocess );
			if ( direct )
				return direct;

			const auto fixed = dtb::fix( eprocess );
			if ( !fixed ) {
				static volatile long logged = 0;
				if ( InterlockedCompareExchange( &logged, 1, 0 ) == 0 )
					disk::print( obf( "communication cr3 for 0x%llx unresolved; scan found nothing" ),
						eprocess );
			}

			return fixed;
		}

		bool read_virtual( std::uint64_t cr3, std::uint64_t address,
			std::uint64_t user_buffer, std::size_t size )
		{
			std::uint8_t scratch[ 0x100 ];

			for ( std::size_t done = 0; done < size; ) {
				std::uint64_t pa = 0;
				if ( !dtb::translate( cr3, address + done, pa ) )
					return false;

				auto chunk = static_cast< std::size_t >(
					0x1000 - ( ( address + done ) & 0xFFF ) );
				if ( chunk > size - done )
					chunk = size - done;
				if ( chunk > sizeof( scratch ) )
					chunk = sizeof( scratch );

				if ( !dtb::read_physical( pa, scratch, chunk ) )
					return false;
				if ( !copy_to_user( user_buffer + done, scratch, chunk ) )
					return false;

				done += chunk;
			}

			return true;
		}

		bool write_virtual( std::uint64_t cr3, std::uint64_t address,
			std::uint64_t user_buffer, std::size_t size )
		{
			std::uint8_t scratch[ 0x100 ];

			for ( std::size_t done = 0; done < size; ) {
				std::uint64_t pa = 0;
				if ( !dtb::translate( cr3, address + done, pa ) )
					return false;

				auto chunk = static_cast< std::size_t >(
					0x1000 - ( ( address + done ) & 0xFFF ) );
				if ( chunk > size - done )
					chunk = size - done;
				if ( chunk > sizeof( scratch ) )
					chunk = sizeof( scratch );

				if ( !copy_from_user( user_buffer + done, scratch, chunk ) )
					return false;
				if ( !paging::dpm::write_physical( pa, scratch, chunk ) )
					return false;

				done += chunk;
			}

			return true;
		}

		// The walk over PsActiveProcessList and the proof of a candidate cr3 are the
		// two expensive parts of every request: ~200 list hops and a four-level walk
		// per call, repeated identically for a pid and a process that do not change
		// between requests. One entry each, keyed on the input they were computed
		// from, is enough to take both off the hot path.
// Per-processor, like dpm's page mappings. Plain globals here would be read and
// written from the exception handler on every CPU at once, and a torn pid/
// eprocess pair hands a wrong process - and therefore a wrong cr3 - to the
// physical read path. Indexed by processor there is no sharing to race on.
struct cache_t {
	std::uint32_t m_pid {};
	std::uint64_t m_eprocess {};
	std::uint64_t m_dtb_eprocess {};
	std::uint64_t m_dtb {};
};

cache_t m_cache[ 64 ] {};

cache_t& cache_for_current_processor( ) {
	auto processor = kernel::ke_get_current_processor_number( );
	if ( processor >= 64 )
		processor = 0;

	return m_cache[ processor ];
}

std::uint64_t eprocess_cached( std::uint32_t process_id ) {
	auto& cache = cache_for_current_processor( );

	if ( process_id && process_id == cache.m_pid && cache.m_eprocess )
		return cache.m_eprocess;

	const auto eprocess = find_eprocess( process_id );
	if ( eprocess ) {
		cache.m_pid = process_id;
		cache.m_eprocess = eprocess;
	}

	return eprocess;
}

std::uint64_t dtb_cached( std::uint64_t eprocess ) {
	auto& cache = cache_for_current_processor( );

	if ( eprocess && eprocess == cache.m_dtb_eprocess && cache.m_dtb )
		return cache.m_dtb;

	const auto cr3 = resolved_dtb( eprocess );
	if ( cr3 ) {
		cache.m_dtb_eprocess = eprocess;
		cache.m_dtb = cr3;
	}

	return cr3;
}

		void dispatch( data_request_t& request ) {
			const auto type = static_cast< e_data_type >( request.m_type );
			if ( type <= e_data_type::none || type > e_data_type::is_running ) {
				request.m_status = static_cast< std::int32_t >( e_data_status::bad_type );
				return;
			}

			if ( type == e_data_type::is_running ) {
				request.m_result = k_alive;
				return;
			}

			if ( !dtb::m_off_dtb || !dtb::m_ps_head || !dtb::m_off_links || !dtb::m_off_pid ) {
				request.m_status = static_cast< std::int32_t >( e_data_status::not_ready );
				return;
			}

			const auto eprocess = eprocess_cached( request.m_process_id );
			if ( !eprocess ) {
				request.m_status = static_cast< std::int32_t >( e_data_status::no_process );
				return;
			}

			switch ( type ) {
			case e_data_type::get_eprocess:
				request.m_result = eprocess;
				break;

			case e_data_type::get_base_address:
				request.m_result = dtb::process_module( eprocess );
				break;

			case e_data_type::get_directory_table_base:
				request.m_result = dtb_cached( eprocess );
				break;

			case e_data_type::translate_linear: {
				std::uint64_t pa = 0;
				const auto cr3 = dtb_cached( eprocess );
				if ( !cr3 || !dtb::translate( cr3, request.m_address, pa ) ) {
					request.m_status = static_cast< std::int32_t >( e_data_status::bad_address );
					return;
				}
				request.m_result = pa;
			} break;

			case e_data_type::read_memory: {
				if ( !request.m_size || request.m_size > k_transfer_limit ) {
					request.m_status = static_cast< std::int32_t >( e_data_status::too_large );
					return;
				}

				const auto cr3 = dtb_cached( eprocess );
				if ( !cr3 || !read_virtual( cr3, request.m_address, request.m_buffer,
						 static_cast< std::size_t >( request.m_size ) ) ) {
					request.m_status = static_cast< std::int32_t >( e_data_status::transfer_failed );
					return;
				}

				request.m_result = request.m_size;
			} break;

			case e_data_type::write_memory: {
				if ( !request.m_size || request.m_size > k_transfer_limit ) {
					request.m_status = static_cast< std::int32_t >( e_data_status::too_large );
					return;
				}

				const auto cr3 = dtb_cached( eprocess );
				if ( !cr3 || !write_virtual( cr3, request.m_address, request.m_buffer,
						 static_cast< std::size_t >( request.m_size ) ) ) {
					request.m_status = static_cast< std::int32_t >( e_data_status::transfer_failed );
					return;
				}

				request.m_result = request.m_size;
			} break;

			default:
				request.m_status = static_cast< std::int32_t >( e_data_status::bad_type );
				return;
			}

			request.m_status = static_cast< std::int32_t >( e_data_status::ok );
		}

				// The context write is the documented route and is sufficient on its own:
		// KiDispatchException calls KeContextToKframes after KdTrap returns non-zero,
		// so the new RIP reaches the trap frame without us touching it. The raw store
		// that used to follow wrote eight bytes into a kernel structure at an offset we
		// only infer, and a wrong guess lands on the frame that restores the caller's
		// RSP - which is how a benchmark ends in a stack-cookie failure inside a process
		// that never corrupted its own stack.
		void claim( PCONTEXT ctx, std::uint64_t, std::size_t length ) {
			const auto resumed = static_cast< std::uint64_t >( ctx->Rip ) + length;
			ctx->Rip = static_cast< DWORD64 >( resumed );
			// Context only, deliberately - see the note on claim().
		}

		bool on_communication_callback( PCONTEXT ctx, PEXCEPTION_RECORD exc_rec,
			std::uint64_t trap_frame ) {
			if ( !ctx || !exc_rec )
				return false;

			if ( exc_rec->ExceptionCode != STATUS_PRIVILEGED_INSTRUCTION )
				return false;

			if ( ( ctx->SegCs & 3 ) == 0 )
				return false;

			std::uint64_t operand_va = 0;
			std::size_t   length = 0;
			if ( !decode_lgdt_operand( ctx, operand_va, length ) )
				return false;

			data_request_t request {};
			if ( !copy_from_user( operand_va, &request, sizeof( request ) ) )
				return false;

			if ( request.m_magic != k_magic ) {
				if ( InterlockedCompareExchange( &m_logged_magic, 1, 0 ) == 0 )
					disk::print( obf( "communication unknown caller; rip=0x%llx operand=0x%llx" ),
						static_cast< std::uint64_t >( ctx->Rip ), operand_va );
				return false;
			}

			InterlockedIncrement( &m_requests );

			request.m_status = static_cast< std::int32_t >( e_data_status::ok );
			request.m_result = 0;

			dispatch( request );

			if ( request.m_status != static_cast< std::int32_t >( e_data_status::ok ) )
				InterlockedIncrement( &m_failures );

			if ( !copy_to_user( operand_va, &request, sizeof( request ) ) ) {
				InterlockedIncrement( &m_failures );
				claim( ctx, trap_frame, length );
				return true;
			}

			if ( InterlockedCompareExchange( &m_logged_served, 1, 0 ) == 0 )
				disk::print( obf( "communication initialized; operand=0x%llx" ),
					request.m_type, operand_va );

			claim( ctx, trap_frame, length );
			return true;
		}
	}
}
