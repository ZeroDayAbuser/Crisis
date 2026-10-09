#pragma once

//
// Entry-point tracer.
//
// One job: follow the protected driver's entry point and write down what it does
// -- the instruction stream, the calls it makes (resolved to symbols and
// parameters), and the machine-level instructions that betray an anti-cheat
// technique. Execution is followed with the one trap mechanism that leaves the
// image untouched: a DR execute breakpoint on the entry, then RFLAGS.TF for the
// duration of the entry. No page bytes, no PTE, no shadow tree.
//
// Everything the trace produces is queued and flushed to disk by a worker, never
// printed per instruction from the traced context.
//

#define instr_emulator false

#include <core/hooks/exception/utility/module.hxx>
#include <core/hooks/exception/framework/emulator/dr.hxx>
#include <core/hooks/exception/framework/emulator/callout.hxx>

#if instr_emulator

namespace exception {
	namespace emulator {
		inline bool          m_print_trace { true };
		inline std::uint32_t m_print_stride = 1;
		inline bool          m_log_special { true };
		inline std::uint64_t m_log_limit {};
		inline std::uint64_t m_log_rotate { 100000000ull };
		inline std::uint64_t m_log_rotate_mb {};
		inline bool          m_log_symbols { true };
		inline bool          m_log_privileged { true };
		//
		// This is the full per-instruction stream. It is off by default because it is
		// large and it costs time in the traced context; the call trace and the
		// detectors are what carry the review, and this is for the rare case where
		// every instruction is wanted.
		//
		inline bool          m_log_every_instruction { false };

		namespace log {
			static constexpr std::size_t k_capacity      = 65536;
			static constexpr std::size_t k_mask          = k_capacity - 1;
			static constexpr std::size_t k_tail_capacity = 8192;
			static constexpr std::size_t k_tail_mask     = k_tail_capacity - 1;

			struct record_t {
				std::uint64_t m_rip;
			};

			inline record_t*         m_buffer {};
			inline volatile LONG64   m_head {};
			inline std::uint64_t     m_tail {};
			inline std::uint64_t     m_written {};
			inline std::uint64_t     m_instructions {};
			inline bool              m_truncated {};
			inline std::uint64_t     m_tail_ring[ k_tail_capacity ] {};
			inline volatile LONG64   m_tail_head {};
			inline ZydisDecoder      m_decoder {};
			inline ZydisFormatter    m_formatter {};
			inline bool              m_codec_ready {};

			void ensure_codec( ) {
				if ( m_codec_ready )
					return;
				ZydisDecoderInit( &m_decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64 );
				ZydisFormatterInit( &m_formatter, ZYDIS_FORMATTER_STYLE_INTEL );
				m_codec_ready = true;
			}

			bool format_rip( std::uint64_t rip, char* out, std::size_t out_size ) {
				if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( rip ) ) ||
					 !kernel::mm_is_address_valid( reinterpret_cast< void* >( rip + 14 ) ) )
					return false;

				ensure_codec( );

				ZydisDecodedInstruction instruction {};
				ZydisDecodedOperand     operands[ ZYDIS_MAX_OPERAND_COUNT ] {};
				if ( !ZYAN_SUCCESS( ZydisDecoderDecodeFull( &m_decoder,
					reinterpret_cast< const void* >( rip ), 15, &instruction, operands ) ) )
					return false;

				return ZYAN_SUCCESS( ZydisFormatterFormatInstruction( &m_formatter,
					&instruction, operands, instruction.operand_count_visible,
					out, out_size, rip, nullptr ) ) != 0;
			}

			inline std::uint64_t m_ann_base {};
			inline std::uint64_t m_ann_size {};
			inline bool          m_ann_ntoskrnl {};
			inline char          m_ann_name[ 64 ] {};
			inline std::uint64_t m_ann_sym_value {};
			inline char          m_ann_sym[ 128 ] {};

			//
			// `nt!ZwOpenFile+0x…` / `module+0x…` / raw. The module and symbol are
			// cached so a run of instructions in the same function costs one walk.
			//
			void annotate( std::uint64_t rip, char* out, std::size_t out_size ) {
				if ( !rip || rip < m_ann_base || rip >= m_ann_base + m_ann_size ) {
					const auto info = module::find( rip );
					m_ann_base      = info.m_base;
					m_ann_size      = info.m_size;
					m_ann_ntoskrnl  = info.m_is_ntoskrnl;
					m_ann_name[ 0 ] = 0;
					m_ann_sym_value = 0;
					m_ann_sym[ 0 ]  = 0;
					if ( info.m_name && info.m_name_length ) {
						std::size_t i = 0;
						for ( ; i < sizeof( m_ann_name ) - 1 && i < info.m_name_length; ++i )
							m_ann_name[ i ] = static_cast< char >( info.m_name[ i ] );
						m_ann_name[ i ] = 0;
					}
				}

				if ( m_ann_ntoskrnl ) {
					if ( !m_ann_sym_value || rip < m_ann_sym_value ||
						 rip >= m_ann_sym_value + 0x2000 ) {
						std::uint64_t value = 0;
						std::uint8_t  length = 0;
						const auto* sym =
							pdb::get_symbol_name_by_address( rip, &value, &length );
						m_ann_sym_value = value;
						m_ann_sym[ 0 ]  = 0;
						if ( sym && length ) {
							std::size_t i = 0;
							for ( ; i < sizeof( m_ann_sym ) - 1 && i < length; ++i )
								m_ann_sym[ i ] = sym[ i ];
							m_ann_sym[ i ] = 0;
						}
					}
					if ( m_ann_sym[ 0 ] )
						_snprintf( out, out_size, "nt!%s+0x%llx",
							m_ann_sym, rip - m_ann_sym_value );
					else
						_snprintf( out, out_size, "nt+0x%llx", rip - m_ann_base );
				}
				else if ( m_ann_name[ 0 ] ) {
					_snprintf( out, out_size, "%s+0x%llx", m_ann_name, rip - m_ann_base );
				}
				else {
					_snprintf( out, out_size, "0x%llx", rip );
				}
			}

			//
			// Batched ring flush. PASSIVE only; the worker in this file drives it.
			//
			void flush( ) {
				if ( !m_buffer )
					return;
				if ( KeGetCurrentIrql( ) > PASSIVE_LEVEL )
					return;

				const auto head = static_cast< std::uint64_t >( m_head );
				if ( m_tail >= head )
					return;

				if ( !m_print_trace ) {
					m_tail = head;
					return;
				}

				const auto rotate_bytes = m_log_rotate_mb * 1024ull * 1024ull;
				const bool rotate_by_size = rotate_bytes && m_written >= rotate_bytes;
				const bool rotate_by_count = m_log_rotate && m_instructions >= m_log_rotate;

				if ( rotate_by_size || rotate_by_count ) {
					char header[ 192 ] {};
					_snprintf( header, sizeof( header ) - 1,
						"=== trace rotate: fresh window after %llu instr / %llu MB ===\n",
						static_cast< unsigned long long >( m_instructions ),
						static_cast< unsigned long long >( m_written / ( 1024ull * 1024ull ) ) );
					disk::rotate( header );
					m_instructions = 0;
					m_written = 0;
				}

				if ( m_log_limit && m_written >= m_log_limit ) {
					if ( !m_truncated ) {
						m_truncated = true;
						static const char message[] =
							"[x] trace truncated: limit reached\n";
						disk::append( message, sizeof( message ) - 1 );
					}
					m_tail = head;
					return;
				}

				static char   batch[ 32768 ] {};
				std::size_t   used  = 0;
				std::uint32_t index = 0;

				while ( m_tail < head ) {
					const auto& record = m_buffer[ m_tail & k_mask ];
					const auto rip = record.m_rip;
					++m_tail;

					if ( ( index++ % m_print_stride ) != 0 )
						continue;

					char line[ 200 ] {};
					if ( !format_rip( rip, line, sizeof( line ) ) )
						continue;

					char ann[ 160 ] {};
					annotate( rip, ann, sizeof( ann ) );

					if ( used + sizeof( line ) + sizeof( ann ) + 64 >= sizeof( batch ) ) {
						disk::append( batch, used );
						m_written += used;
						used = 0;
						if ( m_log_limit && m_written >= m_log_limit )
							break;
					}

					const auto written = _snprintf( batch + used,
						sizeof( batch ) - used - 2, "[x] 0x%llx %s %s\n",
						rip, ann, line );
					if ( written > 0 ) {
						used += static_cast< std::size_t >( written );
						++m_instructions;
					}
				}

				if ( used ) {
					disk::append( batch, used );
					m_written += used;
				}
			}

			void dump_tail( ) {
				if ( KeGetCurrentIrql( ) > PASSIVE_LEVEL )
					return;

				const auto total = static_cast< std::uint64_t >( m_tail_head );
				if ( !total )
					return;

				const auto count = total < k_tail_capacity ? total : k_tail_capacity;
				disk::append( "[x] ---- tail ----\n", 18 );

				static char batch[ 32768 ] {};
				std::size_t used = 0;

				for ( std::uint64_t i = 0; i < count; ++i ) {
					const auto slot = ( total - count + i ) & k_tail_mask;
					const auto tail_rip = m_tail_ring[ slot ];

					char line[ 200 ] {};
					if ( !format_rip( tail_rip, line, sizeof( line ) ) )
						continue;

					char ann[ 160 ] {};
					annotate( tail_rip, ann, sizeof( ann ) );

					if ( used + sizeof( line ) + sizeof( ann ) + 64 >= sizeof( batch ) ) {
						disk::append( batch, used );
						used = 0;
					}

					const auto written = _snprintf( batch + used,
						sizeof( batch ) - used - 2, "[x] tail 0x%llx %s %s\n",
						tail_rip, ann, line );
					if ( written > 0 )
						used += static_cast< std::size_t >( written );
				}

				if ( used )
					disk::append( batch, used );
			}

			void dump_wide( const char* tag, std::uint64_t va, std::size_t max_chars ) {
				if ( KeGetCurrentIrql( ) > PASSIVE_LEVEL )
					return;
				if ( !va || !kernel::mm_is_address_valid( reinterpret_cast< void* >( va ) ) )
					return;
				if ( max_chars > 96 )
					max_chars = 96;

				char buffer[ 384 ] {};
				int  used = 0;

				__try {
					used = _snprintf( buffer, sizeof( buffer ) - 1,
						"[x] %s @0x%llx:", tag, va );
					if ( used < 0 )
						used = 0;

					const auto* p = reinterpret_cast< const wchar_t* >( va );
					for ( std::size_t i = 0; i < max_chars && p[ i ]; ++i ) {
						if ( used >= static_cast< int >( sizeof( buffer ) ) - 8 )
							break;
						const auto written = _snprintf( buffer + used,
							sizeof( buffer ) - used - 1, "%c",
							static_cast< char >( p[ i ] ) );
						if ( written > 0 )
							used += written;
					}
				}
				__except ( EXCEPTION_EXECUTE_HANDLER ) {
					return;
				}

				strcat( buffer, "\n" );
				disk::append_discovery( buffer, strlen( buffer ) );
			}

			//
			// A review line. Queued through disk::print so a burst of detectors or
			// callouts is flushed by the log worker rather than opening the file from
			// the traced context.
			//
			template< class... args_t >
			void discover( const char* format, args_t... va_args ) {
				char buffer[ 384 ] = "";
				_snprintf( buffer, sizeof( buffer ) - 2, format, va_args... );
				disk::print( obf( "%s" ), buffer );
			}

			void push( std::uint64_t rip ) {
				if ( !m_buffer )
					return;

				const auto idx = static_cast< std::uint64_t >( m_head );
				m_buffer[ idx & k_mask ].m_rip = rip;
				_InterlockedIncrement64( &m_head );

				const auto tail_idx = static_cast< std::uint64_t >( m_tail_head );
				m_tail_ring[ tail_idx & k_tail_mask ] = rip;
				_InterlockedIncrement64( &m_tail_head );
			}
		}

		inline bool          m_worker_started {};
		inline volatile LONG m_stop {};

		void flush_worker( void* ) {
			std::int64_t interval = -100000;
			while ( !m_stop ) {
				kernel::ke_delay_execution_thread( 0, false, &interval );
				log::flush( );
			}
			log::flush( );
			kernel::ps_terminate_system_thread( 0 );
		}

		void start_worker( ) {
			if ( m_worker_started )
				return;

			void* handle = nullptr;
			const auto status = kernel::ps_create_system_thread(
				&handle, THREAD_ALL_ACCESS, nullptr, nullptr, nullptr,
				reinterpret_cast< void* >( flush_worker ), nullptr );
			if ( !NT_SUCCESS( status ) ) {
				disk::print( obf( "[x] flush worker failed 0x%08x" ),
					static_cast< unsigned int >( status ) );
				return;
			}

			if ( handle )
				kernel::zw_close( handle );

			m_worker_started = true;
			disk::print( obf( "[x] flush worker started" ) );
		}

		static constexpr std::size_t k_max_regions = 4;

		struct region_t {
			std::uint64_t m_base;
			std::uint64_t m_size;
			bool          m_active;
		};

		inline region_t            m_regions[ k_max_regions ] {};
		inline std::size_t         m_count {};
		inline std::uint64_t       m_seed_va {};
		inline std::uint64_t       m_trace_owner {};
		inline std::uint64_t       m_driver_object {};
		inline bool                m_seed_miss_logged {};
		//
		// Default: step the entry. The alternative -- leave the entry native and
		// pick the trace up at its return -- is the stealthy mode for a run where
		// the target's own timing checks matter more than the instruction stream.
		//
		inline bool                m_trace_after_entry { false };
		inline bool                m_instrument { true };
		inline bool                m_seed_at_load { true };
		inline std::uint64_t       m_return_va {};
		inline bool                m_return_armed {};
		inline void*               m_traced_thread {};
		inline std::uint64_t       m_privileged_hits {};
		inline std::uint64_t       m_symbol_hits {};
		inline std::uint64_t       m_special_hits {};
		inline int                 m_pending_special {};
		inline std::uint64_t       m_pending_special_rip {};
		inline std::uint64_t       m_pending_rax {};
		inline std::uint64_t       m_pending_rcx {};
		inline std::uint64_t       m_seed_method {};

		region_t* find( std::uint64_t va ) {
			for ( std::size_t i = 0; i < m_count; ++i ) {
				auto& r = m_regions[ i ];
				if ( r.m_active && va >= r.m_base && va < r.m_base + r.m_size )
					return &r;
			}
			return nullptr;
		}

		//
		// How far from an armed address a DR trap's Rip may land. A DR execute
		// breakpoint is documented as a fault, but Windows delivers it as a trap, so
		// Rip can be the instruction *after* the breakpoint.
		//
		static constexpr std::uint64_t k_dr_report_slack = 16;

		bool dr_arm_for( std::uint64_t owner, std::uint64_t va,
			const char* tag, bool single = true ) {
			return owner &&
				dr::acquire( owner, va, tag, dr::kind::execute, single ) >= 0;
		}

		void dr_release_for( std::uint64_t owner, const char* tag ) {
			if ( owner )
				dr::release_tag( owner, tag );
		}

		bool dr_arm( std::uint64_t va, const char* tag, bool single = true ) {
			return dr_arm_for( dr::current_owner( ), va, tag, single );
		}

		void dr_release( const char* tag ) {
			dr_release_for( dr::current_owner( ), tag );
		}

		inline const char* k_tag_seed    = "seed";
		inline const char* k_tag_return  = "ret";
		inline const char* k_tag_callout = "callout";

		void arm_return( std::uint64_t va ) {
			m_return_armed = true;
			m_return_va = va;
			dr_arm( va, k_tag_return );
		}

		void disarm_return( ) {
			m_return_armed = false;
			dr_release( k_tag_return );
		}

		void unseed( ) {
			if ( m_seed_method == 1 )
				dr_release_for( m_trace_owner, k_tag_seed );
			m_seed_method = 0;
		}

		void stop_trace( const char* reason ) {
			log::discover( obf( "[x] trace stop: %s" ), reason );

			unseed( );
			m_return_armed = false;
			dr::release_owner( m_trace_owner );
			dr::print_stats( );
			m_trace_owner = 0;
			m_traced_thread = nullptr;

			log::dump_tail( );
			log::flush( );
		}

		std::uint64_t image_entry( std::uint64_t base ) {
			auto* dos = reinterpret_cast< dos_header_t* >( base );
			if ( !dos->is_valid( ) )
				return 0;

			auto* nt = reinterpret_cast< nt_headers_t* >( base + dos->m_lfanew );
			if ( !nt->is_valid( ) )
				return 0;

			return base + nt->m_address_of_entry_point;
		}

		//
		// The trap. DR0 with R/W = 00 and LEN = 00 fires on the fetch of this exact
		// instruction and needs nothing else: no page bytes, no PTE, no shadow tree.
		//
		bool seed( std::uint64_t va ) {
			if ( !va || m_seed_method )
				return false;

			if ( !m_instrument ) {
				disk::print( obf( "[x] instrumentation disabled: nothing armed at 0x%llx" ), va );
				return false;
			}

			if ( !m_seed_at_load ) {
				disk::print( obf( "[x] no seed during load; entry 0x%llx left untouched" ), va );
				return false;
			}

			m_seed_va = va;
			m_trace_owner = dr::current_owner( );

			dr_arm_for( m_trace_owner, va, k_tag_seed );

			if ( __readdr( 0 ) == va && ( __readdr( 7 ) & 1ull ) ) {
				m_seed_method = 1;
				disk::print( obf( "[x] armed(dr0) 0x%llx dr7=0x%llx" ),
					va, dr::dr7_for( m_trace_owner ) );
				return true;
			}

			dr_release_for( m_trace_owner, k_tag_seed );
			m_trace_owner = 0;

			disk::print( obf( "[x] dr0 unavailable; entry 0x%llx left untouched" ), va );
			return false;
		}

		//
		// Arm the entry trap from the load-image callback. That callback runs on the
		// same thread that is about to execute DriverEntry, which is what makes the
		// per-thread DR0 slot valid here.
		//
		void arm_from_load( std::uint64_t base ) {
			if ( !m_instrument || !m_seed_at_load || m_seed_method )
				return;

			const auto entry = image_entry( base );
			if ( !entry ) {
				disk::print( obf( "[x] arm_from_load: no entry for base=0x%llx" ), base );
				return;
			}

			if ( seed( entry ) )
				disk::print( obf( "[x] armed from load 0x%llx" ), entry );
		}

		bool on_seed_trap( PCONTEXT ctx ) {
			if ( !ctx || m_seed_method != 1 )
				return false;

			const auto seed_delta = ctx->Rip >= m_seed_va
				? ctx->Rip - m_seed_va : m_seed_va - ctx->Rip;

			if ( seed_delta > k_dr_report_slack ) {
				if ( !m_seed_miss_logged ) {
					m_seed_miss_logged = true;
					disk::print(
						obf( "[x] seed not matched: rip=0x%llx seeded=0x%llx dr6=0x%llx" ),
						ctx->Rip, m_seed_va,
						static_cast< std::uint64_t >( __readdr( 6 ) ) );
				}
				return false;
			}

			//
			// DriverEntry( PDRIVER_OBJECT, PUNICODE_STRING ) -- at the trap Rcx is
			// the driver object. The trap identifies the traced thread, which owns
			// every slot the trace takes from here on.
			//
			m_traced_thread = kernel::ps_get_current_thread( );
			m_trace_owner = reinterpret_cast< std::uint64_t >( m_traced_thread );
			m_driver_object = ctx->Rcx;

			dr::note_trap( m_trace_owner );
			disk::print( obf( "[x] seed trap rip=0x%llx seeded=0x%llx delta=%lld driver=0x%llx" ),
				ctx->Rip, m_seed_va, static_cast< long long >( seed_delta ), m_driver_object );

			unseed( );

			if ( m_trace_after_entry ) {
				ctx->EFlags &= ~0x100;
				disk::print( obf( "[x] entry left native, tf off" ) );
				return true;
			}

			if ( KeGetCurrentIrql( ) <= APC_LEVEL ) {
				log::discover( obf( "[x] stepping 0x%llx" ), ctx->Rip );
				ctx->EFlags |= 0x100;
			}

			return true;
		}

		//
		// Privileged / global instructions. Cheap opcode table, and it names the
		// technique directly: CR accesses and MSRs are how a kernel-mode component
		// manipulates the machine, invlpg/wbinvd are TLB/cache control, cli/sti and
		// swapgs are interrupt/segment state, in/out are port IO, and vm* is
		// hypervisor interaction.
		//
		void detect_privileged( PCONTEXT ctx ) {
			if ( !m_log_privileged || !ctx )
				return;

			if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( ctx->Rip ) ) )
				return;
			if ( ( ctx->Rip & 0xFFF ) > ( 0x1000 - 16 ) )
				return;

			const auto* p = reinterpret_cast< const std::uint8_t* >( ctx->Rip );
			const char* what = nullptr;

			if ( p[ 0 ] == 0x0F ) {
				if ( p[ 1 ] == 0x22 )      what = "mov cr,reg";
				else if ( p[ 1 ] == 0x20 ) what = "mov reg,cr";
				else if ( p[ 1 ] == 0x30 ) what = "wrmsr";
				else if ( p[ 1 ] == 0x32 ) what = "rdmsr";
				else if ( p[ 1 ] == 0x09 ) what = "wbinvd";
				else if ( p[ 1 ] == 0x01 ) {
					if ( p[ 2 ] == 0xF8 )      what = "swapgs";
					else if ( p[ 2 ] == 0xF9 ) what = "rdtscp";
					else if ( ( p[ 2 ] & 0xC7 ) == 0x07 ) what = "invlpg";
					else if ( ( p[ 2 ] & 0xC7 ) == 0x02 ) what = "lgdt";
					else if ( ( p[ 2 ] & 0xC7 ) == 0x03 ) what = "lidt";
					else if ( ( p[ 2 ] & 0xC7 ) == 0x06 ) what = "clts";
					else if ( p[ 2 ] == 0x33 ) what = "rdpmc";
				}
				else if ( ( p[ 1 ] & 0xF8 ) == 0x78 ) what = "vmx";
			}
			else if ( p[ 0 ] == 0xFA ) what = "cli";
			else if ( p[ 0 ] == 0xFB ) what = "sti";
			else if ( p[ 0 ] == 0xEC || p[ 0 ] == 0xED ||
					  p[ 0 ] == 0xEE || p[ 0 ] == 0xEF ) what = "port io";

			//
			// A segment-prefixed access names kernel-structure awareness -- gs: is
			// the KPCR, fs: is the TEB.
			//
			const bool gs_access = ( p[ 0 ] == 0x65 );
			const bool fs_access = ( p[ 0 ] == 0x64 );

			if ( !what && !gs_access && !fs_access )
				return;

			//
			// rdmsr/wrmsr take the index in ECX, and which MSR it is says what the
			// component is doing: LSTAR/FMASK/STAR is syscall and hook
			// infrastructure, the GS_BASE family is segment plumbing, TSC/APERF/MPERF
			// is timing.
			//
			const bool is_rdmsr = ( p[ 0 ] == 0x0F && p[ 1 ] == 0x32 );
			const bool is_wrmsr = ( p[ 0 ] == 0x0F && p[ 1 ] == 0x30 );

			const char* msr = nullptr;
			if ( is_rdmsr || is_wrmsr ) {
				switch ( static_cast< std::uint32_t >( ctx->Rcx ) ) {
				case 0x10:       msr = "IA32_TSC"; break;
				case 0x1B:       msr = "IA32_APIC_BASE"; break;
				case 0x48:       msr = "IA32_SPEC_CTRL"; break;
				case 0x49:       msr = "IA32_PRED_CMD"; break;
				case 0xE7:       msr = "IA32_MPERF"; break;
				case 0xE8:       msr = "IA32_APERF"; break;
				case 0xC0000080: msr = "IA32_EFER"; break;
				case 0xC0000081: msr = "IA32_STAR"; break;
				case 0xC0000082: msr = "IA32_LSTAR"; break;
				case 0xC0000083: msr = "IA32_CSTAR"; break;
				case 0xC0000084: msr = "IA32_FMASK"; break;
				case 0xC0000100: msr = "IA32_FS_BASE"; break;
				case 0xC0000101: msr = "IA32_GS_BASE"; break;
				case 0xC0000102: msr = "IA32_KERNEL_GS_BASE"; break;
				default: break;
				}
			}

			const auto cap = 64u;
			if ( m_privileged_hits++ >= cap )
				return;

			if ( msr )
				log::discover( obf( "[x] %s %s rip=0x%llx rcx=0x%llx rdx=0x%llx" ),
					what, msr, ctx->Rip, ctx->Rcx, ctx->Rdx );
			else
				log::discover( obf( "[x] %s rip=0x%llx rax=0x%llx rcx=0x%llx rdx=0x%llx" ),
					what ? what : ( gs_access ? "gs-access" : "fs-access" ),
					ctx->Rip, ctx->Rax, ctx->Rcx, ctx->Rdx );
		}

		//
		// RIP-relative data access, resolved to a symbol. A read of KdDebuggerEnabled,
		// KiKvaShadow, PsLoadedModuleList or MmPfnDatabase names the technique
		// outright, and the same is true of any ntoskrnl global or another module's
		// data.
		//
		void detect_symbol_read( PCONTEXT ctx ) {
			if ( !m_log_symbols || !ctx )
				return;

			if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( ctx->Rip ) ) )
				return;
			if ( ( ctx->Rip & 0xFFF ) > ( 0x1000 - 16 ) )
				return;

			const auto* p = reinterpret_cast< const std::uint8_t* >( ctx->Rip );
			if ( !exception::hook::has_rip_relative_operand( p ) )
				return;

			const auto length = exception::hook::decode_insn_len( p );
			if ( length < 5 || length > 15 )
				return;

			std::uint32_t disp = 0;
			std::memcpy( &disp, p + ( length - 4 ), sizeof( disp ) );

			const auto target = static_cast< std::uint64_t >(
				static_cast< std::int64_t >( ctx->Rip + length ) +
				static_cast< std::int32_t >( disp ) );

			if ( find( target ) || !kernel::mm_is_address_valid( reinterpret_cast< void* >( target ) ) )
				return;

			std::uint64_t value = 0;
			std::uint8_t  length2 = 0;
			const auto* name = pdb::get_symbol_name_by_address( target, &value, &length2 );
			if ( !name || !length2 )
				return;

			if ( m_symbol_hits++ >= 128 )
				return;

			log::discover( obf( "[x] symbol-read %.*s+0x%llx rip=0x%llx" ),
				static_cast< int >( length2 ), name,
				value ? target - value : 0, ctx->Rip );
		}

		const char* special_name( int kind ) {
			switch ( kind ) {
			case 1: return obf( "rdtsc" );
			case 2: return obf( "rdtscp" );
			case 3: return obf( "cpuid" );
			case 4: return obf( "xgetbv" );
			default: return obf( "special" );
			}
		}

		//
		// Timing and feature queries. The pending/actual pair gives the value the
		// target saw, which is how a TSC or CPUID check is reconstructed.
		//
		void detect_special( PCONTEXT ctx ) {
			if ( !m_log_special )
				return;

			if ( m_pending_special ) {
				log::discover(
					obf( "[x] %s 0x%llx in.rax=0x%llx in.rcx=0x%llx out.rax=0x%llx out.rbx=0x%llx out.rcx=0x%llx out.rdx=0x%llx" ),
					special_name( m_pending_special ), m_pending_special_rip,
					m_pending_rax, m_pending_rcx,
					ctx->Rax, ctx->Rbx, ctx->Rcx, ctx->Rdx );
				m_pending_special = 0;
			}

			if ( m_special_hits >= 256 )
				return;
			if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( ctx->Rip ) ) )
				return;

			const auto* p = reinterpret_cast< const std::uint8_t* >( ctx->Rip );
			int kind = 0;
			if ( p[ 0 ] == 0x0F && p[ 1 ] == 0x31 )
				kind = 1;
			else if ( p[ 0 ] == 0x0F && p[ 1 ] == 0x01 && p[ 2 ] == 0xF9 )
				kind = 2;
			else if ( p[ 0 ] == 0x0F && p[ 1 ] == 0xA2 )
				kind = 3;
			else if ( p[ 0 ] == 0x0F && p[ 1 ] == 0x01 && p[ 2 ] == 0xD0 )
				kind = 4;

			if ( !kind )
				return;

			++m_special_hits;
			m_pending_special = kind;
			m_pending_special_rip = ctx->Rip;
			m_pending_rax = ctx->Rax;
			m_pending_rcx = ctx->Rcx;
		}

		//
		// The call trace. `module!symbol @addr(parameter = value, ...)` with the
		// parameter names out of the PDB, plus the hand-off search: if the call is
		// one that registers a routine or queues work, that routine runs on a thread
		// we hold no trap on, so it gets its own slot.
		//
		void on_call( PCONTEXT ctx ) {
			std::uint64_t symbol_value = 0;
			std::uint8_t  symbol_length = 0;
			const auto* symbol = pdb::get_symbol_name_by_address(
				ctx->Rip, &symbol_value, &symbol_length );

			char name[ 128 ] {};
			if ( symbol && symbol_length ) {
				const auto length = symbol_length < 127 ? symbol_length : 127;
				for ( std::size_t i = 0; i < length; ++i )
					name[ i ] = symbol[ i ];
			}

			if ( strstr_lowercase( name, obf( "registryvalue" ) ) ) {
				log::dump_wide( "reg-path", ctx->Rdx, 96 );
				log::dump_wide( "reg-value", ctx->R8, 96 );
			}

			const auto info = module::find( ctx->Rip );

			if ( !info.m_is_ntoskrnl || !name[ 0 ] ) {
				char where[ 192 ] {};
				module::format( ctx->Rip, where, sizeof( where ) );
				log::discover(
					obf( "[x] call %s rcx=0x%llx rdx=0x%llx r8=0x%llx r9=0x%llx" ),
					where, ctx->Rcx, ctx->Rdx, ctx->R8, ctx->R9 );
				return;
			}

			char where[ 192 ] {};
			module::format( ctx->Rip, where, sizeof( where ) );

			char text[ 512 ] = {};
			int  used = _snprintf( text, sizeof( text ) - 1, "%s @0x%llx(",
				where, ctx->Rip );

			const auto count = pdb::get_function_param_count( name );
			const auto bounded = count < 10 ? count : 10;

			if ( bounded == 0 ) {
				used += _snprintf( text + used, sizeof( text ) - used,
					"rcx=0x%llx, rdx=0x%llx, r8=0x%llx, r9=0x%llx",
					ctx->Rcx, ctx->Rdx, ctx->R8, ctx->R9 );
			}

			for ( std::uint64_t i = 0; i < bounded; ++i ) {
				if ( used < 0 || static_cast< std::size_t >( used ) >= sizeof( text ) - 48 )
					break;

				char pname[ 64 ] = {};
				pdb::get_function_param_name( name, i, pname, sizeof( pname ) );
				if ( !pname[ 0 ] )
					_snprintf( pname, sizeof( pname ), "p%llu", i );

				std::uint64_t value = 0;
				if ( i == 0 ) value = ctx->Rcx;
				else if ( i == 1 ) value = ctx->Rdx;
				else if ( i == 2 ) value = ctx->R8;
				else if ( i == 3 ) value = ctx->R9;
				else {
					const auto slot = ctx->Rsp + 0x28 + ( i - 4 ) * sizeof( std::uint64_t );
					if ( kernel::mm_is_address_valid( reinterpret_cast< void* >( slot ) ) )
						value = *reinterpret_cast< const std::uint64_t* >( slot );
				}

				const auto written = _snprintf( text + used, sizeof( text ) - used,
					"%s%s=0x%llx", i ? ", " : "", pname, value );
				if ( written < 0 )
					break;
				used += written;
			}

			if ( used >= 0 && static_cast< std::size_t >( used ) < sizeof( text ) - 1 )
				text[ used++ ] = ')';
			text[ used ] = '\0';

			log::discover( obf( "[x] call %s" ), text );

			const auto continuation = callout::follow( name, ctx );

			if ( continuation && find( continuation ) ) {
				dr_arm_for( m_trace_owner, continuation, k_tag_callout, false );
				log::discover(
					obf( "[x] callout into region 0x%llx" ), continuation );
			}
		}

		bool on_return( PCONTEXT ctx ) {
			if ( !ctx || !m_return_armed )
				return false;

			const auto return_delta = ctx->Rip >= m_return_va
				? ctx->Rip - m_return_va : m_return_va - ctx->Rip;

			if ( kernel::ps_get_current_thread( ) != m_traced_thread ||
				 return_delta > k_dr_report_slack ) {
				disarm_return( );
				return false;
			}

			disarm_return( );
			dr::note_trap( m_trace_owner );

			if ( find( ctx->Rip ) ) {
				log::discover( obf( "[x] ret 0x%llx rax=0x%llx" ),
					ctx->Rip, ctx->Rax );
				ctx->EFlags |= 0x100;
			}

			return true;
		}

		bool on_single_step( PCONTEXT ctx ) {
			if ( !ctx )
				return false;

			//
			// Only ever touch a #DB we caused. If nothing of ours is armed the
			// exception belongs to somebody else and is left alone.
			//
			if ( !m_seed_method && !m_return_armed && !m_traced_thread )
				return false;

			//
			// Fallback routing: if kdp_report's Dr6 branch did not claim the seed it
			// has to be recognised here or the trap is never torn down.
			//
			if ( on_seed_trap( ctx ) )
				return true;

			if ( KeGetCurrentIrql( ) > APC_LEVEL ) {
				ctx->EFlags &= ~0x100;
				stop_trace( "irql" );
				return true;
			}

			if ( find( ctx->Rip ) ) {
				detect_privileged( ctx );
				detect_symbol_read( ctx );
				detect_special( ctx );
				if ( m_log_every_instruction )
					log::push( ctx->Rip );
				ctx->EFlags |= 0x100;
				return true;
			}

			if ( kernel::mm_is_address_valid( reinterpret_cast< void* >( ctx->Rsp ) ) ) {
				const auto return_address =
					*reinterpret_cast< const std::uint64_t* >( ctx->Rsp );

				if ( find( return_address ) ) {
					on_call( ctx );
					arm_return( return_address );
					ctx->EFlags &= ~0x100;
					return true;
				}
			}

			disarm_return( );
			ctx->EFlags &= ~0x100;

			log::discover( obf( "[x] region-exit rip=0x%llx" ), ctx->Rip );
			stop_trace( "region-exit" );
			return true;
		}

		void add( std::uint64_t base, std::uint64_t size ) {
			if ( !base || !size || m_count >= k_max_regions )
				return;

			auto& region = m_regions[ m_count++ ];
			region.m_base   = base;
			region.m_size   = size;
			region.m_active = true;

			if ( !log::m_buffer ) {
				log::m_buffer = reinterpret_cast< log::record_t* >(
					kernel::mm_allocate_independent_pages(
						sizeof( log::record_t ) * log::k_capacity ) );
			}

			start_worker( );

			disk::print( obf( "[x] tracing base=0x%llx size=0x%llx entry=0x%llx" ),
				base, size, image_entry( base ) );
		}
	}
}

#else

namespace exception {
	namespace emulator {
		inline void* find( std::uint64_t ) { return nullptr; }
		inline void  add( std::uint64_t, std::uint64_t ) { }
		inline void  arm_from_load( std::uint64_t ) { }
		inline bool  on_seed_trap( PCONTEXT ) { return false; }
		inline bool  on_return( PCONTEXT ) { return false; }
		inline bool  on_single_step( PCONTEXT ) { return false; }
	}
}

#endif
