#include <deps/includes.h>

namespace engine::emulator {
		std::string wide_to_utf8_lossy( const wchar_t* text, const std::int32_t count ) {
			if ( !text || count <= 0 )
				return {};

			const std::int32_t length = text[ count - 1 ] == L'\0' ? count - 1 : count;
			if ( length <= 0 )
				return {};

			const int required = WideCharToMultiByte(
				CP_UTF8,
				0,
				text,
				length,
				nullptr,
				0,
				nullptr,
				nullptr
			);

			if ( required <= 0 )
				return {};

			std::string out( required, '\0' );
			WideCharToMultiByte(
				CP_UTF8,
				0,
				text,
				length,
				out.data( ),
				required,
				nullptr,
				nullptr
			);

			return out;
		}
		std::string fname_to_string( const f_name& name, bool display_errors ) {
			//if ( !name.comparison_index )
				return {};

			static std::mutex to_string_cache_mutex {};
			static std::mutex to_string_emulator_mutex {};
			static std::unordered_map<std::uint64_t, std::string> to_string_cache {};

			const auto cache_key =
				( static_cast< std::uint64_t >( name.comparison_index ) << 32 ) |
				static_cast< std::uint64_t >( name.number );

			{
				std::lock_guard lock( to_string_cache_mutex );
				if ( const auto it = to_string_cache.find( cache_key ); it != to_string_cache.end( ) )
					return it->second;
			}

			const auto cache_result = [ & ]( std::string value ) {
				std::lock_guard lock( to_string_cache_mutex );
				to_string_cache.try_emplace( cache_key, value );
				return value;
				};

			std::lock_guard emulator_lock( to_string_emulator_mutex );

			{
				std::lock_guard lock( to_string_cache_mutex );
				if ( const auto it = to_string_cache.find( cache_key ); it != to_string_cache.end( ) )
					return it->second;
			}

			constexpr std::uint64_t region_base = 0x500000000000;
			constexpr std::uint64_t fname_offset = 0x00;
			constexpr std::uint64_t fstring_offset = 0x100;
			constexpr std::uint64_t buffer_offset = 0x200;
			constexpr std::uint32_t buffer_chars = 512;
			constexpr std::uint64_t region_size = 0x2000;

			std::vector<std::uint8_t> bytes( region_size );
			std::memcpy( bytes.data( ) + fname_offset, &name, sizeof( name ) );

			emu_fstring output {};
			output.data = region_base + buffer_offset;
			output.count = 0;
			output.capacity = buffer_chars;

			std::memcpy( bytes.data( ) + fstring_offset, &output, sizeof( output ) );

			static tostring_emu::emulator emu {};
			static bool emu_initialized = false;

			if ( !emu_initialized ) {
				std::string init_error {};
				logging::print( "[f_name] before Unicorn initialization" );
				if ( !emu.init( init_error ) ) {
					if ( display_errors )
						printf( "[f_name] ToString emulator init failed: %s\n", init_error.c_str( ) );
					return {};
				}

				emu_initialized = true;
				logging::print( "[f_name] Unicorn initialization completed" );
			}

			tostring_emu::run_config cfg {};
			cfg.function = bypass::target::m_base_address + offsets::to_string;
			cfg.image_base = bypass::target::m_base_address;
			cfg.image_size = 0x20000000;
			cfg.timeout_us = 500000;
			cfg.max_instructions = 500000;
			cfg.rcx = region_base + fname_offset;
			cfg.rdx = region_base + fstring_offset;

			tostring_emu::mapped_region region {};
			region.remote_base = region_base;
			region.emu_base = region_base;
			region.size = region_size;
			region.bytes = std::move( bytes );
			region.writable = true;
			cfg.mapped_data.push_back( std::move( region ) );

			tostring_emu::tostring_layout layout {};
			std::string layout_error {};
			if ( !tostring_emu::resolve_tostring_layout( cfg.function, layout, layout_error ) ) {
				if ( display_errors )
					logging::print( "[f_name] %s", layout_error.c_str( ) );
				return cache_result( "" );
			}

			logging::print(
				"[f_name] auto layout state=0x%llx guard=0x%llx delta=0x%llx",
				layout.state,
				layout.guard,
				layout.guard_delta
			);

			// Map the live state object into the emulated image at the same
			// module-relative address.  No FNamePool/chunk-array constants are
			// needed: the initialized live state already contains its pointers.
			constexpr std::uint64_t state_map_size = 0x10000;
			const auto state_remote_base = tostring_emu::align_down(
				min( layout.state, layout.guard )
			);

			if ( state_remote_base < cfg.image_base ) {
				if ( display_errors )
					logging::print( "[f_name] resolved ToString state is outside the image" );
				return cache_result( "" );
			}

			const auto state_emu_base = tostring_emu::emu_image_base +
				( state_remote_base - cfg.image_base );
			const auto guard_offset = layout.guard - state_remote_base;

			std::vector< std::uint8_t > to_string_state( state_map_size );
			if ( guard_offset >= to_string_state.size( ) ) {
				if ( display_errors )
					logging::print( "[f_name] resolved ToString guard is outside the mapped state window" );
				return cache_result( "" );
			}

			if ( !bypass::target::read_memory(
				state_remote_base,
				to_string_state.data( ),
				to_string_state.size( ) ) ) {
				if ( display_errors )
					logging::print( "[f_name] failed to read auto-resolved ToString state" );
				return cache_result( "" );
			}

			// Mark the lazy-init guard as initialized so Unicorn does not run
			// the constructor/registration path.
			to_string_state[ guard_offset ] = 1;

			tostring_emu::mapped_region to_string_state_region {};
			to_string_state_region.remote_base = state_remote_base;
			to_string_state_region.emu_base = state_emu_base;
			to_string_state_region.size = to_string_state.size( );
			to_string_state_region.bytes = std::move( to_string_state );
			to_string_state_region.writable = true;
			cfg.mapped_data.push_back( std::move( to_string_state_region ) );

			logging::print( "[f_name] before Unicorn run index=%u number=%u",
				name.comparison_index, name.number );
			auto result = emu.run( cfg );
			logging::print( "[f_name] after Unicorn run index=%u number=%u ok=%d",
				name.comparison_index, name.number, result.ok ? 1 : 0 );
			if ( !result.ok ) {
				if ( display_errors ) {
					printf(
						"[f_name] ToString emulator failed: %s (rip=0x%llx rax=0x%llx)\n",
						result.error.c_str( ),
						result.rip,
						result.rax
					);
				}
				return cache_result( "" );
			}

			const auto region_it = result.mapped_output.find( region_base );
			if ( region_it == result.mapped_output.end( ) ||
				region_it->second.size( ) < buffer_offset + buffer_chars * sizeof( wchar_t ) ) {
				if ( display_errors )
					printf( "[f_name] ToString emulator produced no output buffer\n" );
				return cache_result( "" );
			}

			emu_fstring final_string {};
			std::memcpy( &final_string, region_it->second.data( ) + fstring_offset, sizeof( final_string ) );

			if ( final_string.count < 0 ||
				final_string.count > static_cast< std::int32_t >( buffer_chars ) ||
				final_string.data != region_base + buffer_offset ) {
				if ( display_errors ) {
					printf(
						"[f_name] ToString emulator returned invalid FString data=0x%llx count=%d capacity=%d\n",
						final_string.data,
						final_string.count,
						final_string.capacity
					);
				}
				return cache_result( "" );
			}

			const auto wide = reinterpret_cast< const wchar_t* >(
				region_it->second.data( ) + buffer_offset
				);

			auto resolved = wide_to_utf8_lossy( wide, final_string.count );
			return cache_result( std::move( resolved ) );
		}
}
