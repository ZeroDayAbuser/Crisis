#pragma once

namespace exception {
	namespace emulator {
		namespace dr {
			static constexpr std::size_t k_slots      = 4;
			static constexpr std::size_t k_max_owners = 8;

			enum class kind : std::uint8_t {
				execute    = 0,
				write      = 1,
				read_write = 3
			};

			struct slot_t {
				std::uint64_t m_va {};
				const char*   m_tag {};
				std::uint8_t  m_kind {};
				bool          m_active {};
			};

			struct owner_t {
				std::uint64_t m_thread {};
				slot_t        m_slots[ k_slots ] {};
				std::uint64_t m_programmed[ k_slots ] {};
				std::uint64_t m_dr7 {};
				std::uint32_t m_active {};
				std::uint64_t m_traps {};
			};

			inline owner_t       m_owners[ k_max_owners ] {};
			inline std::uint32_t m_owner_count {};
			inline std::uint32_t m_active_total {};
			inline std::uint64_t m_high_water {};
			inline std::uint64_t m_traps_total {};
			inline std::uint64_t m_refused {};
			inline bool          m_refusal_logged {};

			inline std::uint64_t current_owner( ) {
				return reinterpret_cast< std::uint64_t >(
					kernel::ps_get_current_thread( ) );
			}

			owner_t* find_owner( std::uint64_t thread ) {
				for ( std::size_t i = 0; i < m_owner_count; ++i )
					if ( m_owners[ i ].m_thread == thread )
						return &m_owners[ i ];
				return nullptr;
			}

			owner_t* owner( std::uint64_t thread ) {
				if ( !thread )
					return nullptr;

				if ( auto* found = find_owner( thread ) )
					return found;

				if ( m_owner_count < k_max_owners ) {
					auto& created = m_owners[ m_owner_count++ ];
					created = {};
					created.m_thread = thread;
					return &created;
				}

				for ( std::size_t i = 0; i < m_owner_count; ++i ) {
					if ( m_owners[ i ].m_active )
						continue;

					auto& recycled = m_owners[ i ];
					recycled = {};
					recycled.m_thread = thread;
					return &recycled;
				}

				return nullptr;
			}

			inline void write_dr( std::size_t index, std::uint64_t value ) {
				switch ( index ) {
					case 0: __writedr( 0, value ); break;
					case 1: __writedr( 1, value ); break;
					case 2: __writedr( 2, value ); break;
					case 3: __writedr( 3, value ); break;
					default: break;
				}
			}

			std::uint64_t build_dr7( const owner_t& state ) {
				std::uint64_t control = 0;

				for ( std::size_t i = 0; i < k_slots; ++i ) {
					if ( !state.m_slots[ i ].m_active )
						continue;

					const auto rw = static_cast< std::uint64_t >(
						state.m_slots[ i ].m_kind ) & 3ull;

					control |= rw << ( 16 + ( i * 4 ) );
					control |= 1ull << ( i * 2 );
				}

				return control;
			}

			std::uint64_t dr7_for( std::uint64_t thread ) {
				auto* state = find_owner( thread );
				return state ? state->m_dr7 : 0;
			}

			bool program( std::uint64_t thread ) {
				auto* state = find_owner( thread );
				if ( !state )
					return false;

				if ( thread != current_owner( ) )
					return false;

				for ( std::size_t i = 0; i < k_slots; ++i ) {
					const auto va = state->m_slots[ i ].m_active
						? state->m_slots[ i ].m_va : 0ull;

					if ( va == state->m_programmed[ i ] )
						continue;

					write_dr( i, va );
					state->m_programmed[ i ] = va;
				}

				state->m_dr7 = build_dr7( *state );
				__writedr( 7, state->m_dr7 );
				__writedr( 6, 0 );
				return true;
			}

			std::int32_t acquire( std::uint64_t thread, std::uint64_t va,
				const char* tag, kind type = kind::execute, bool single = true ) {
				if ( !thread || !va )
					return -1;

				auto* state = owner( thread );
				if ( !state ) {
					++m_refused;
					return -1;
				}

				for ( std::size_t i = 0; i < k_slots; ++i ) {
					if ( state->m_slots[ i ].m_active &&
						 state->m_slots[ i ].m_va == va ) {
						state->m_slots[ i ].m_tag = tag;
						return static_cast< std::int32_t >( i );
					}
				}

				if ( single && tag ) {
					for ( std::size_t i = 0; i < k_slots; ++i ) {
						if ( !state->m_slots[ i ].m_active ||
							 !state->m_slots[ i ].m_tag )
							continue;

						const char* have = state->m_slots[ i ].m_tag;
						const char* want = tag;
						while ( *have && *want && *have == *want ) {
							++have;
							++want;
						}
						if ( *have || *want )
							continue;

						state->m_slots[ i ].m_va = va;
						state->m_slots[ i ].m_kind =
							static_cast< std::uint8_t >( type );
						program( thread );
						return static_cast< std::int32_t >( i );
					}
				}

				for ( std::size_t i = 0; i < k_slots; ++i ) {
					if ( state->m_slots[ i ].m_active )
						continue;

					state->m_slots[ i ].m_va = va;
					state->m_slots[ i ].m_tag = tag;
					state->m_slots[ i ].m_kind =
						static_cast< std::uint8_t >( type );
					state->m_slots[ i ].m_active = true;

					++state->m_active;
					++m_active_total;
					if ( m_active_total > m_high_water )
						m_high_water = m_active_total;

					program( thread );
					return static_cast< std::int32_t >( i );
				}

				++m_refused;
				if ( !m_refusal_logged ) {
					m_refusal_logged = true;
					disk::print(
						obf( "[x::dr] refused: 4 slots already held on 0x%llx (tag=%s)" ),
						thread, tag );
				}
				return -1;
			}

			std::uint32_t release_tag( std::uint64_t thread, const char* tag ) {
				auto* state = find_owner( thread );
				if ( !state || !tag )
					return 0;

				std::uint32_t released = 0;

				for ( std::size_t i = 0; i < k_slots; ++i ) {
					if ( !state->m_slots[ i ].m_active || !state->m_slots[ i ].m_tag )
						continue;

					const char* have = state->m_slots[ i ].m_tag;
					const char* want = tag;
					while ( *have && *want && *have == *want ) {
						++have;
						++want;
					}
					if ( *have || *want )
						continue;

					state->m_slots[ i ] = {};
					--state->m_active;
					if ( m_active_total )
						--m_active_total;
					++released;
				}

				if ( released )
					program( thread );

				return released;
			}

			std::uint32_t release_owner( std::uint64_t thread ) {
				auto* state = find_owner( thread );
				if ( !state )
					return 0;

				const auto released = state->m_active;

				for ( auto& slot : state->m_slots )
					slot = {};

				state->m_active = 0;
				m_active_total = released > m_active_total
					? 0u : m_active_total - released;

				program( thread );
				return released;
			}

			std::uint32_t active( std::uint64_t thread ) {
				auto* state = find_owner( thread );
				return state ? state->m_active : 0;
			}

			void note_trap( std::uint64_t thread ) {
				++m_traps_total;
				if ( auto* state = find_owner( thread ) )
					++state->m_traps;
			}
		}
	}
}
