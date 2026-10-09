#pragma once

//
// Callout follower.
//
// We trace one thread. The gap that leaves is execution the traced thread
// *hands off*: it registers a routine, or queues work, and something else runs
// it later, on another thread. This finds those hand-offs at the call site.
//
// How it reads the routine, given the x64 convention (rcx, rdx, r8, r9, then the
// shadow space at [rsp+0x28], [rsp+0x30], ... at the callee's first
// instruction):
//   * arg_reg   - the routine is passed directly in one of the four registers;
//   * stack     - it is the fifth argument or later, so it is in the caller's
//                 shadow space and we read it off the stack -- quite literally
//                 following the stack to the new execution;
//   * reg_deref - the API takes a structure and the routine is a field in it
//                 (ExQueueWorkItem's WORK_QUEUE_ITEM, for instance).
//
// The registering site is [rsp] at the call, i.e. the in-region address that
// asked for the callout, so a continuation can be attributed back to the code
// that created it.
//
// Two deliberate limits:
//   * only routines passed *directly* (or through a structure we can
//     dereference) are visible. A routine assembled at runtime, or read out of
//     an uninitialised item later, is not.
//   * a slot recorded for another thread is pending until dr::program() runs
//     for that owner, i.e. until something in that thread's context passes
//     through this module. Arming another thread from here needs a write into
//     its saved debug state -- that is the next step, and the per-thread tables
//     in dr.hxx are the bookkeeping it will need so the four registers cannot
//     overflow.
//

namespace exception {
	namespace emulator {
		namespace callout {
			enum class source : std::uint8_t {
				arg_reg   = 0,
				stack     = 1,
				reg_deref = 2
			};

			struct desc_t {
				const char*  m_match;
				source       m_source;
				std::uint8_t m_reg;
				std::uint8_t m_offset;
				const char*  m_note;
			};

			static constexpr std::uint32_t k_max_continuations = 32;

			struct continuation_t {
				std::uint64_t m_target {};
				std::uint64_t m_from {};
				std::uint64_t m_thread {};
				const char*   m_note {};
			};

			inline continuation_t m_continuations[ k_max_continuations ] {};
			inline std::uint32_t  m_continuation_count {};
			inline std::uint64_t  m_total_seen {};

			// Match strings are lowercase; strstr_lowercase() compares that way.
			inline const desc_t m_table[] = {
				{ "pscreatesystemthread",                source::stack,     0, 0x30, "system thread start routine" },
				{ "ioqueueworkitem",                     source::arg_reg,   1, 0x00, "work item, pool thread" },
				{ "ioqueueworkitemex",                   source::arg_reg,   1, 0x00, "work item, pool thread" },
				{ "exqueueworkitem",                     source::reg_deref, 0, 0x10, "work item, pool thread" },
				{ "keinitializedpc",                     source::arg_reg,   1, 0x00, "dpc, dispatch level" },
				{ "keipigenericcall",                    source::arg_reg,   0, 0x00, "ipi broadcast, IPI level" },
				{ "pssetcreateprocessnotifyroutineex",   source::arg_reg,   1, 0x00, "process notify callback" },
				{ "pssetcreateprocessnotifyroutine",     source::arg_reg,   0, 0x00, "process notify callback" },
				{ "pssetloadimagenotifyroutine",         source::arg_reg,   0, 0x00, "image notify callback" },
				{ "pssetcreatethreadnotifyroutineex",    source::arg_reg,   1, 0x00, "thread notify callback" },
				{ "pssetcreatethreadnotifyroutine",      source::arg_reg,   0, 0x00, "thread notify callback" },
				{ "cmregistercallback",                  source::arg_reg,   0, 0x00, "registry callback" },
				{ "keregisterbugcheckcallback",          source::reg_deref, 0, 0x08, "bugcheck callback record" },
				{ "keinitializeapc",                     source::arg_reg,   2, 0x00, "apc normal routine" }
			};

			inline std::uint64_t read_qword( std::uint64_t address ) {
				if ( !address || !kernel::mm_is_address_valid(
					reinterpret_cast< void* >( address ) ) )
					return 0;

				return *reinterpret_cast< const std::uint64_t* >( address );
			}

			inline std::uint64_t arg_register( PCONTEXT ctx, std::uint8_t index ) {
				switch ( index ) {
					case 0: return ctx->Rcx;
					case 1: return ctx->Rdx;
					case 2: return ctx->R8;
					case 3: return ctx->R9;
					default: return 0;
				}
			}

			bool record( std::uint64_t target, std::uint64_t from,
				std::uint64_t thread, const char* note ) {
				for ( std::uint32_t i = 0; i < m_continuation_count; ++i )
					if ( m_continuations[ i ].m_target == target )
						return false;

				if ( m_continuation_count >= k_max_continuations )
					return false;

				auto& entry = m_continuations[ m_continuation_count++ ];
				entry.m_target = target;
				entry.m_from   = from;
				entry.m_thread = thread;
				entry.m_note   = note;

				disk::print(
					obf( "[x::callout] %s target=0x%llx from=0x%llx thread=0x%llx" ),
					note, target, from, thread );
				return true;
			}

			inline bool looks_like_callout_param( const char* pname ) {
				return strstr_lowercase( pname, "routine" ) ||
					strstr_lowercase( pname, "callback" ) ||
					strstr_lowercase( pname, "notify" );
			}

			inline bool looks_like_code( std::uint64_t value ) {
				return value >= 0xFFFF800000000000ull &&
					kernel::mm_is_address_valid( reinterpret_cast< void* >( value ) );
			}

			std::uint64_t follow_from_pdb( const char* name, PCONTEXT ctx ) {
				const auto count = pdb::get_function_param_count( name );
				const auto bounded = count < 10 ? count : 10;

				for ( std::uint64_t i = 0; i < bounded; ++i ) {
					char pname[ 64 ] = {};
					pdb::get_function_param_name( name, i, pname, sizeof( pname ) );

					if ( !pname[ 0 ] || !looks_like_callout_param( pname ) )
						continue;

					std::uint64_t value = 0;

					if ( i == 0 ) value = ctx->Rcx;
					else if ( i == 1 ) value = ctx->Rdx;
					else if ( i == 2 ) value = ctx->R8;
					else if ( i == 3 ) value = ctx->R9;
					else value = read_qword(
						ctx->Rsp + 0x28 + ( i - 4 ) * sizeof( std::uint64_t ) );

					if ( !looks_like_code( value ) )
						continue;

					return value;
				}

				return 0;
			}

			std::uint64_t follow( const char* name, PCONTEXT ctx ) {
				if ( !name || !name[ 0 ] || !ctx )
					return 0;

				std::uint64_t target = 0;
				const char*   note   = nullptr;

				for ( const auto& desc : m_table ) {
					if ( !strstr_lowercase( name, desc.m_match ) )
						continue;

					if ( desc.m_source == source::arg_reg )
						target = arg_register( ctx, desc.m_reg );
					else if ( desc.m_source == source::stack )
						target = read_qword( ctx->Rsp + desc.m_offset );
					else
						target = read_qword(
							arg_register( ctx, desc.m_reg ) + desc.m_offset );

					note = desc.m_note;
					break;
				}

				if ( !target ) {
					target = follow_from_pdb( name, ctx );
					if ( target )
						note = "callout (pdb parameter)";
				}

				if ( !looks_like_code( target ) )
					return 0;

				++m_total_seen;

				record( target, read_qword( ctx->Rsp ),
					dr::current_owner( ),
					note ? note : "callout" );
				return target;
			}

			std::uint32_t count( ) {
				return m_continuation_count;
			}

			const continuation_t* at( std::uint32_t index ) {
				return index < m_continuation_count ? &m_continuations[ index ] : nullptr;
			}
		}
	}
}
