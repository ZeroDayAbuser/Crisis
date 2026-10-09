#include <deps/includes.h>

std::vector< std::uint8_t > tostring_emu::make_input_buffer(
    std::span< const std::uint8_t > bytes ) {
    return { bytes.begin( ), bytes.end( ) };
}

namespace tostring_emu {
	std::uint64_t align_down( const std::uint64_t value, const std::uint64_t alignment ) {
		return value & ~( alignment - 1 );
	}
	std::uint64_t align_up( const std::uint64_t value, const std::uint64_t alignment ) {
		return ( value + alignment - 1 ) & ~( alignment - 1 );
	}
	std::string uc_error( const char* prefix, const uc_err err ) {
		std::ostringstream ss;
		ss << prefix << ": " << uc_strerror( err );
		return ss.str( );
	}
	bool resolve_tostring_layout(
		const std::uint64_t function,
		tostring_layout& out,
		std::string& error )
	{
		constexpr std::size_t scan_size = 0x500;
		std::array< std::uint8_t, scan_size > code {};

		if ( !bypass::target::read_memory( function, code.data( ), code.size( ) ) ) {
			error = "failed to read FName::ToString for Zydis scan";
			return false;
		}

		ZydisDecoder decoder {};
		if ( !ZYAN_SUCCESS( ZydisDecoderInit(
			&decoder,
			ZYDIS_MACHINE_MODE_LONG_64,
			ZYDIS_STACK_WIDTH_64 ) ) ) {
			error = "ZydisDecoderInit failed";
			return false;
		}

		struct instruction_record
		{
			std::uint64_t address {};
			ZydisDecodedInstruction instruction {};
			std::array< ZydisDecodedOperand, ZYDIS_MAX_OPERAND_COUNT > operands {};
		};

		std::vector< instruction_record > instructions {};
		instructions.reserve( 128 );

		std::size_t offset {};
		while ( offset < code.size( ) && instructions.size( ) < 256 ) {
			instruction_record rec {};
			rec.address = function + offset;

			const auto status = ZydisDecoderDecodeFull(
				&decoder,
				code.data( ) + offset,
				code.size( ) - offset,
				&rec.instruction,
				rec.operands.data( )
			);

			if ( !ZYAN_SUCCESS( status ) || !rec.instruction.length )
				break;

			instructions.push_back( rec );
			offset += rec.instruction.length;
		}

		const auto rip_target = [ ]( const instruction_record& rec,
			const ZydisDecodedOperand& op,
			std::uint64_t& target ) -> bool {
				if ( op.type != ZYDIS_OPERAND_TYPE_MEMORY ||
					op.mem.base != ZYDIS_REGISTER_RIP )
					return false;

				ZyanU64 absolute {};
				if ( !ZYAN_SUCCESS( ZydisCalcAbsoluteAddress(
					&rec.instruction,
					&op,
					rec.address,
					&absolute ) ) )
					return false;

				target = static_cast< std::uint64_t >( absolute );
				return true;
			};

		std::uint64_t guard {};
		std::size_t guard_cmp_index = static_cast< std::size_t >( -1 );

		// First find the lazy-init guard: CMP byte ptr [RIP+...], 0.  We also
		// verify that the same byte is written with 1 shortly afterwards.
		for ( std::size_t i = 0; i < instructions.size( ); ++i ) {
			const auto& rec = instructions[ i ];
			if ( rec.instruction.mnemonic != ZYDIS_MNEMONIC_CMP )
				continue;

			std::uint64_t candidate {};
			bool has_zero_imm = false;
			bool has_byte_mem = false;

			for ( std::uint8_t j = 0; j < rec.instruction.operand_count; ++j ) {
				const auto& op = rec.operands[ j ];
				if ( op.type == ZYDIS_OPERAND_TYPE_IMMEDIATE &&
					!op.imm.is_relative && op.imm.value.u == 0 )
					has_zero_imm = true;

				if ( op.type == ZYDIS_OPERAND_TYPE_MEMORY && op.size == 8 &&
					rip_target( rec, op, candidate ) )
					has_byte_mem = true;
			}

			if ( !has_zero_imm || !has_byte_mem )
				continue;

			bool writes_one = false;
			const auto end = min( instructions.size( ), i + 24 );
			for ( std::size_t k = i + 1; k < end; ++k ) {
				const auto& next = instructions[ k ];
				if ( next.instruction.mnemonic != ZYDIS_MNEMONIC_MOV )
					continue;

				std::uint64_t write_target {};
				bool same_target = false;
				bool imm_one = false;

				for ( std::uint8_t j = 0; j < next.instruction.operand_count; ++j ) {
					const auto& op = next.operands[ j ];
					if ( op.type == ZYDIS_OPERAND_TYPE_MEMORY && op.size == 8 ) {
						std::uint64_t absolute {};
						if ( rip_target( next, op, absolute ) && absolute == candidate )
							same_target = true;
					}
					if ( op.type == ZYDIS_OPERAND_TYPE_IMMEDIATE &&
						!op.imm.is_relative && op.imm.value.u == 1 )
						imm_one = true;
				}

				if ( same_target && imm_one ) {
					writes_one = true;
					break;
				}
			}

			if ( writes_one ) {
				guard = candidate;
				guard_cmp_index = i;
				break;
			}
		}

		if ( !guard ) {
			error = "failed to resolve ToString lazy-init guard";
			return false;
		}

		std::uint64_t state {};

		// The initializer receives &state as its first argument.  Find a
		// RIP-relative LEA into RCX between the guard check and the CALL.
		const auto search_end = min( instructions.size( ), guard_cmp_index + 24 );
		for ( std::size_t i = guard_cmp_index + 1; i < search_end; ++i ) {
			const auto& rec = instructions[ i ];
			if ( rec.instruction.mnemonic != ZYDIS_MNEMONIC_LEA )
				continue;

			bool dst_rcx = false;
			std::uint64_t candidate {};
			for ( std::uint8_t j = 0; j < rec.instruction.operand_count; ++j ) {
				const auto& op = rec.operands[ j ];
				if ( op.type == ZYDIS_OPERAND_TYPE_REGISTER &&
					op.reg.value == ZYDIS_REGISTER_RCX )
					dst_rcx = true;
				else if ( op.type == ZYDIS_OPERAND_TYPE_MEMORY )
					rip_target( rec, op, candidate );
			}

			if ( !dst_rcx || !candidate )
				continue;

			bool call_follows = false;
			const auto call_end = min( instructions.size( ), i + 5 );
			for ( std::size_t k = i + 1; k < call_end; ++k ) {
				if ( instructions[ k ].instruction.mnemonic == ZYDIS_MNEMONIC_CALL ) {
					call_follows = true;
					break;
				}
			}

			if ( call_follows ) {
				state = candidate;
				break;
			}
		}

		if ( !state ) {
			error = "failed to resolve ToString state";
			return false;
		}

		// This is an extra sanity check for the layout seen in current builds:
		// the guard sits just before the state object.  We do not require a
		// particular delta, but expose it for diagnostics.
		out.state = state;
		out.guard = guard;
		out.guard_delta = state >= guard ? state - guard : 0;
		return true;
	}
		struct emulator::impl {
			uc_engine* uc {};
			// Persistent hook handles — registered once per run, removed at the
			// start of the next run (or in the destructor).
			uc_hook    m_code_hook {};
			uc_hook    m_mem_hook {};
			bool       m_hooks_active {};
			ZydisDecoder decoder {};
			run_config config {};
			call_handler_t call_handler {};
			std::unordered_map< std::uint64_t, mapped_region > mapped_by_remote {};
			std::unordered_map< std::uint64_t, std::uint64_t > emu_page_to_remote {};
			std::string error {};

			// Remove active hooks without touching the engine.
			void release_hooks( ) {
				if ( !m_hooks_active )
					return;
				if ( m_code_hook ) { uc_hook_del( uc, m_code_hook ); m_code_hook = 0; }
				if ( m_mem_hook ) { uc_hook_del( uc, m_mem_hook ); m_mem_hook = 0; }
				m_hooks_active = false;
			}

			// Prepare the engine for another emulation run:
			// - remove old hooks,
			// - unmap every region that was mapped during the previous run.
			// Called instead of uc_close/uc_open so the engine object is reused.
			void reset_for_reuse( ) {
				release_hooks( );

				// Unmap every page tracked during the previous run.
				for ( const auto& [emu_page, _] : emu_page_to_remote )
					uc_mem_unmap( uc, emu_page, page_size );
				emu_page_to_remote.clear( );
				mapped_by_remote.clear( );

				// Unmap the emulated stack (not tracked in emu_page_to_remote).
				if ( config.stack_size > 0 ) {
					uc_mem_unmap( uc,
						align_down( config.stack_base ),
						align_up( config.stack_size ) );
				}

				error.clear( );
			}

			static bool is_plausible_user_pointer( const std::uint64_t address ) {
				return address >= 0x10000 &&
					address < 0x0000800000000000ull;
			}

			bool remote_to_emu( const std::uint64_t remote, std::uint64_t& emu ) const {
				if ( config.image_base &&
					remote >= config.image_base &&
					remote < config.image_base + config.image_size ) {
					emu = emu_image_base + ( remote - config.image_base );
					return true;
				}

				for ( const auto& [_, region] : mapped_by_remote ) {
					if ( remote >= region.remote_base &&
						remote < region.remote_base + region.size ) {
						emu = region.emu_base + ( remote - region.remote_base );
						return true;
					}
				}

				return false;
			}

			bool emu_to_remote( const std::uint64_t emu, std::uint64_t& remote ) const {
				if ( config.image_base &&
					emu >= emu_image_base &&
					emu < emu_image_base + config.image_size ) {
					remote = config.image_base + ( emu - emu_image_base );
					return true;
				}

				for ( const auto& [_, region] : mapped_by_remote ) {
					if ( emu >= region.emu_base &&
						emu < region.emu_base + region.size ) {
						remote = region.remote_base + ( emu - region.emu_base );
						return true;
					}
				}

				return false;
			}

			bool map_page_identity( const std::uint64_t remote ) {
				if ( !is_plausible_user_pointer( remote ) )
					return false;

				const std::uint64_t page = align_down( remote );
				if ( emu_page_to_remote.contains( page ) )
					return true;

				std::array< std::uint8_t, page_size > bytes {};
				if ( !bypass::target::read_memory( page, bytes.data( ), bytes.size( ) ) )
					return false;

				const uc_err map_err = uc_mem_map( uc, page, page_size, UC_PROT_ALL );
				if ( map_err != UC_ERR_OK ) {
					error = uc_error( "uc_mem_map identity page failed", map_err );
					return false;
				}

				const uc_err write_err = uc_mem_write( uc, page, bytes.data( ), bytes.size( ) );
				if ( write_err != UC_ERR_OK ) {
					error = uc_error( "uc_mem_write identity page failed", write_err );
					return false;
				}

				emu_page_to_remote[ page ] = page;
				return true;
			}

			bool map_page_for_remote( const std::uint64_t remote ) {
				std::uint64_t emu {};
				if ( !remote_to_emu( remote, emu ) )
					return false;

				const std::uint64_t remote_page = align_down( remote );
				const std::uint64_t emu_page = align_down( emu );

				if ( emu_page_to_remote.contains( emu_page ) )
					return true;

				std::array< std::uint8_t, page_size > page {};
				if ( !bypass::target::read_memory( remote_page, page.data( ), page.size( ) ) ) {
					error = "failed to read remote page 0x" + std::format( "{:X}", remote_page );
					return false;
				}

				const uc_err map_err = uc_mem_map( uc, emu_page, page_size, UC_PROT_ALL );
				if ( map_err != UC_ERR_OK ) {
					error = uc_error( "uc_mem_map code page failed", map_err );
					return false;
				}

				const uc_err write_err = uc_mem_write( uc, emu_page, page.data( ), page.size( ) );
				if ( write_err != UC_ERR_OK ) {
					error = uc_error( "uc_mem_write code page failed", write_err );
					return false;
				}

				emu_page_to_remote[ emu_page ] = remote_page;
				return true;
			}

			bool map_explicit_region( const mapped_region& input ) {
				mapped_region region = input;
				if ( !region.size )
					region.size = region.bytes.size( );

				region.size = align_up( region.size );
				if ( !region.emu_base )
					region.emu_base = align_down( region.remote_base );

				const uc_err map_err = uc_mem_map(
					uc,
					align_down( region.emu_base ),
					region.size,
					region.writable ? UC_PROT_ALL : ( UC_PROT_READ | UC_PROT_EXEC )
				);

				if ( map_err != UC_ERR_OK ) {
					error = uc_error( "uc_mem_map data region failed", map_err );
					return false;
				}

				if ( !region.bytes.empty( ) ) {
					const uc_err write_err = uc_mem_write(
						uc,
						region.emu_base,
						region.bytes.data( ),
						region.bytes.size( )
					);

					if ( write_err != UC_ERR_OK ) {
						error = uc_error( "uc_mem_write data region failed", write_err );
						return false;
					}
				}

				for ( std::uint64_t offset = 0; offset < region.size; offset += page_size )
					emu_page_to_remote[ align_down( region.emu_base + offset ) ] = align_down( region.remote_base + offset );

				mapped_by_remote[ region.remote_base ] = std::move( region );
				return true;
			}

			bool setup_stack( ) {
				const std::uint64_t stack_base = align_down( config.stack_base );
				const std::uint64_t stack_size = align_up( config.stack_size );

				uc_err err = uc_mem_map( uc, stack_base, stack_size, UC_PROT_ALL );
				if ( err != UC_ERR_OK ) {
					error = uc_error( "uc_mem_map stack failed", err );
					return false;
				}

				std::uint64_t rsp = stack_base + stack_size - 0x80;
				err = uc_reg_write( uc, UC_X86_REG_RSP, &rsp );
				if ( err != UC_ERR_OK ) {
					error = uc_error( "uc_reg_write rsp failed", err );
					return false;
				}

				return true;
			}

			bool write_initial_registers( ) {
				std::uint64_t rcx = config.rcx;
				std::uint64_t rdx = config.rdx;
				std::uint64_t r8 = config.r8;
				std::uint64_t r9 = config.r9;

				if ( !remote_to_emu( rcx, rcx ) ) rcx = config.rcx;
				if ( !remote_to_emu( rdx, rdx ) ) rdx = config.rdx;
				if ( !remote_to_emu( r8, r8 ) ) r8 = config.r8;
				if ( !remote_to_emu( r9, r9 ) ) r9 = config.r9;

				const auto write = [ & ]( int reg, const std::uint64_t value ) {
					return uc_reg_write( uc, reg, &value ) == UC_ERR_OK;
					};

				if ( !write( UC_X86_REG_RCX, rcx ) ||
					!write( UC_X86_REG_RDX, rdx ) ||
					!write( UC_X86_REG_R8, r8 ) ||
					!write( UC_X86_REG_R9, r9 ) ) {
					error = "failed to write argument registers";
					return false;
				}

				return true;
			}

			static void hook_code( uc_engine* uc, uint64_t address, uint32_t size, void* user_data ) {
				auto self = static_cast< impl* >( user_data );
				try {

					std::uint64_t remote {};
					if ( !self->emu_to_remote( address, remote ) )
						return;

					std::array< std::uint8_t, 16 > bytes {};
					if ( uc_mem_read( uc, address, bytes.data( ), std::min<std::uint32_t>( size, bytes.size( ) ) ) != UC_ERR_OK )
						return;

					ZydisDecodedInstruction instruction {};
					ZydisDecodedOperand operands[ ZYDIS_MAX_OPERAND_COUNT ] {};
					const auto status = ZydisDecoderDecodeFull(
						&self->decoder,
						bytes.data( ),
						bytes.size( ),
						&instruction,
						operands
					);

					if ( !ZYAN_SUCCESS( status ) )
						return;

					if ( instruction.mnemonic == ZYDIS_MNEMONIC_RET ) {
						std::uint64_t rsp {};
						std::uint64_t return_address {};
						uc_reg_read( uc, UC_X86_REG_RSP, &rsp );
						uc_mem_read( uc, rsp, &return_address, sizeof( return_address ) );

						if ( return_address == stop_address ) {
							uc_emu_stop( uc );
						}
						return;
					}

					if ( instruction.mnemonic != ZYDIS_MNEMONIC_CALL )
						return;

					std::uint64_t target {};
					bool has_target = false;

					for ( std::uint8_t i = 0; i < instruction.operand_count; ++i ) {
						const auto& op = operands[ i ];
						if ( op.type == ZYDIS_OPERAND_TYPE_IMMEDIATE && op.imm.is_relative ) {
							ZyanU64 absolute {};
							if ( ZYAN_SUCCESS( ZydisCalcAbsoluteAddress( &instruction, &op, remote, &absolute ) ) ) {
								target = static_cast< std::uint64_t >( absolute );
								has_target = true;
							}
							break;
						}
					}

					if ( !has_target )
						return;

					const std::uint64_t return_address = address + instruction.length;
					const bool in_image =
						self->config.image_base &&
						target >= self->config.image_base &&
						target < self->config.image_base + self->config.image_size;

					if ( in_image ) {
						self->map_page_for_remote( target );
						return;
					}

					bool handled = false;
					if ( self->call_handler )
						handled = self->call_handler( target, return_address );

					if ( !handled ) {
						std::uint64_t rsp {};
						uc_reg_read( uc, UC_X86_REG_RSP, &rsp );
						uc_mem_read( uc, rsp, &target, sizeof( target ) );
						uc_reg_write( uc, UC_X86_REG_RIP, &return_address );
					}
				}
				catch ( ... ) {
					self->error = "exception in Unicorn code hook";
					uc_emu_stop( uc );
				}
			}

			static bool hook_mem( uc_engine* uc, uc_mem_type, uint64_t address, int, int64_t, void* user_data ) {
				auto self = static_cast< impl* >( user_data );
				try {
					std::uint64_t remote {};
					if ( self->emu_to_remote( align_down( address ), remote ) )
						return self->map_page_for_remote( remote );

					if ( self->map_page_identity( address ) )
						return true;

					self->error = "unmapped memory access at 0x" + std::format( "{:X}", address );
					return false;
				}
				catch ( ... ) {
					self->error = "exception in Unicorn memory hook";
					uc_emu_stop( uc );
					return false;
				}
			}
		};
		emulator::emulator( ):
			m_impl( new impl {} ) {
		}
		emulator::~emulator( ) {
			if ( m_impl ) {
				if ( m_impl->uc ) {
					m_impl->release_hooks( );
					uc_close( m_impl->uc );
				}
				delete m_impl;
			}
		}
		bool emulator::init( std::string& error ) {
			if ( m_impl->uc )
				return true;

			const uc_err err = uc_open( UC_ARCH_X86, UC_MODE_64, &m_impl->uc );
			if ( err != UC_ERR_OK ) {
				error = uc_error( "uc_open failed", err );
				return false;
			}

			ZydisDecoderInit( &m_impl->decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64 );
			return true;
		}
		void emulator::set_call_handler( call_handler_t handler ) {
			m_impl->call_handler = std::move( handler );
		}
		run_result emulator::run( const run_config& config ) {
			run_result result {};

			if ( m_impl->uc ) {
				m_impl->release_hooks( );
				uc_close( m_impl->uc );
				m_impl->uc = nullptr;
			}

			std::string init_error {};
			if ( !init( init_error ) ) {
				result.error = init_error;
				return result;
			}

			m_impl->config = config;
			// mapped_by_remote and emu_page_to_remote were cleared by reset_for_reuse;
			// clear them here too in case this is the very first run.
			m_impl->mapped_by_remote.clear( );
			m_impl->emu_page_to_remote.clear( );
			m_impl->error.clear( );

			if ( !m_impl->setup_stack( ) ) {
				result.error = m_impl->error;
				return result;
			}

			for ( const auto& region : config.mapped_data ) {
				if ( !m_impl->map_explicit_region( region ) ) {
					result.error = m_impl->error;
					return result;
				}
			}

			if ( !m_impl->map_page_for_remote( config.function ) ) {
				result.error = m_impl->error;
				return result;
			}

			std::uint64_t entry {};
			if ( !m_impl->remote_to_emu( config.function, entry ) ) {
				result.error = "failed to translate function address";
				return result;
			}

			std::uint64_t rsp {};
			if ( const auto err = uc_reg_read( m_impl->uc, UC_X86_REG_RSP, &rsp ); err != UC_ERR_OK ) {
				result.error = uc_error( "uc_reg_read rsp failed", err );
				return result;
			}
			rsp -= sizeof( stop_address );
			if ( const auto err = uc_mem_write( m_impl->uc, rsp, &stop_address, sizeof( stop_address ) );
				err != UC_ERR_OK ) {
				result.error = uc_error( "uc_mem_write return address failed", err );
				return result;
			}
			if ( const auto err = uc_reg_write( m_impl->uc, UC_X86_REG_RSP, &rsp ); err != UC_ERR_OK ) {
				result.error = uc_error( "uc_reg_write rsp failed", err );
				return result;
			}

			if ( !m_impl->write_initial_registers( ) ) {
				result.error = m_impl->error;
				return result;
			}

			// Do not install a per-instruction UC_HOOK_CODE here.  Dynamic
			// code pages are demand-mapped by the invalid-memory hook, and
			// uc_emu_start's `until` address handles the synthetic return.
			// Keeping Zydis/std::function work out of Unicorn's hot callback
			// path also avoids ABI/re-entrancy problems seen as host fast-fail
			// (0xC0000409) on some Unicorn builds.
			if ( const auto err = uc_hook_add( m_impl->uc, &m_impl->m_mem_hook, UC_HOOK_MEM_INVALID,
				reinterpret_cast< void* >( &impl::hook_mem ), m_impl, 1, 0 ); err != UC_ERR_OK ) {
				result.error = uc_error( "uc_hook_add memory failed", err );
				return result;
			}
			m_impl->m_hooks_active = true;

			std::array< std::uint8_t, 16 > entry_bytes {};
			const auto entry_read = uc_mem_read(
				m_impl->uc, entry, entry_bytes.data( ), entry_bytes.size( ) );
			if ( entry_read != UC_ERR_OK ) {
				result.error = uc_error( "uc_mem_read entry failed", entry_read );
				return result;
			}

			logging::print(
				"[f_name] entry bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
				entry_bytes[ 0 ], entry_bytes[ 1 ], entry_bytes[ 2 ], entry_bytes[ 3 ],
				entry_bytes[ 4 ], entry_bytes[ 5 ], entry_bytes[ 6 ], entry_bytes[ 7 ] );
			logging::print( "[f_name] entering uc_emu_start entry=0x%llx", entry );
			const uc_err err = uc_emu_start(
				m_impl->uc,
				entry,
				stop_address,
				config.timeout_us,
				config.max_instructions
			);
			logging::print( "[f_name] uc_emu_start returned %u", static_cast< unsigned >( err ) );

			if ( const auto read_err = uc_reg_read( m_impl->uc, UC_X86_REG_RIP, &result.rip );
				read_err != UC_ERR_OK ) {
				result.error = uc_error( "uc_reg_read rip failed", read_err );
				return result;
			}
			if ( const auto read_err = uc_reg_read( m_impl->uc, UC_X86_REG_RAX, &result.rax );
				read_err != UC_ERR_OK ) {
				result.error = uc_error( "uc_reg_read rax failed", read_err );
				return result;
			}

			for ( const auto& [remote_base, region] : m_impl->mapped_by_remote ) {
				if ( !region.writable )
					continue;

				auto& output = result.mapped_output[ remote_base ];
				output.resize( region.size );
				if ( const auto read_err = uc_mem_read(
					m_impl->uc, region.emu_base, output.data( ), output.size( ) );
					read_err != UC_ERR_OK ) {
					result.error = uc_error( "uc_mem_read output failed", read_err );
					return result;
				}
			}

			if ( err != UC_ERR_OK ) {
				result.error = !m_impl->error.empty( )
					? m_impl->error
					: uc_error( "uc_emu_start failed", err );
				return result;
			}

			result.ok = true;
			return result;
		}
}
