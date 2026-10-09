#pragma once
#include <Windows.h>
#include <sddl.h>
#include <cstring>
#include <cstdio>
#include <cstdint>

#pragma comment( lib, "advapi32.lib" )

namespace transport {
	constexpr std::uint32_t k_magic = 0x54325343;
	constexpr std::uint32_t k_version = 2;
	constexpr std::uint32_t k_capacity = 4096;
	constexpr std::uint32_t k_capacity_mask = k_capacity - 1;
	constexpr std::size_t k_payload = 0x1000;
	constexpr std::uint32_t k_spin_iters = 16384;
	constexpr std::uint64_t k_heartbeat_ms = 100;
	constexpr std::uint64_t k_dead_ms = 2000;
	constexpr std::uint32_t k_wait_ms = 4;

	constexpr wchar_t k_sddl_global[] = L"D:(A;;GA;;;WD)(A;;GA;;;AN)(A;;GA;;;BA)(A;;GA;;;SY)";
	constexpr wchar_t k_sddl_session[] = L"D:(A;;GA;;;WD)(A;;GA;;;AN)(A;;GA;;;BA)(A;;GA;;;SY)";

#pragma pack( push, 4 )
	struct handshake_t {
		std::uint32_t magic;
		std::uint32_t version;
		std::uint32_t generation;
		std::uint32_t server_pid;
		std::uint64_t heartbeat;
		std::uint32_t flags;
	};
#pragma pack( pop )

	static_assert( sizeof( handshake_t ) == 0x1C, "handshake layout" );

	enum packet_state : LONG {
		packet_empty = 0,
		packet_pending = 1,
		packet_complete = 2,
		packet_abandoned = 3
	};

	struct packet_t {
		volatile LONG status;
		std::uint32_t command;
		std::uint64_t addr;
		std::uint64_t buffer;
		std::uint64_t size;
		std::uint64_t sequence;
		std::uint64_t result;
		std::uint8_t  payload[ k_payload ];
	};

	struct section_t {
		handshake_t     handshake;
		std::uint32_t   reserved;
		volatile LONG   stop;
		volatile LONG   ready;
		volatile LONG64 write_index;
		volatile LONG64 read_index;
		packet_t        packets[ k_capacity ];
	};

	inline std::size_t length_of( const wchar_t* text ) {
		std::size_t length = 0;
		while ( text && text[ length ] )
			++length;
		return length;
	}

	inline std::uint64_t hash_bytes( const void* data, std::size_t size, std::uint64_t seed ) {
		const auto* bytes = static_cast< const std::uint8_t* >( data );
		for ( std::size_t i = 0; i < size; ++i ) {
			seed ^= bytes[ i ];
			seed *= 1099511628211ull;
		}
		return seed;
	}

	inline std::uint64_t machine_seed( ) {
		std::uint64_t seed = 1469598103934665603ull;

		HKEY key = nullptr;
		if ( RegOpenKeyExW( HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography",
			0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key ) == ERROR_SUCCESS ) {
			wchar_t guid[ 64 ]{ };
			DWORD size = sizeof( guid ) - sizeof( wchar_t );
			if ( RegQueryValueExW( key, L"MachineGuid", nullptr, nullptr,
				reinterpret_cast< LPBYTE >( guid ), &size ) == ERROR_SUCCESS ) {
				guid[ 63 ] = 0;
				seed = hash_bytes( guid, length_of( guid ) * sizeof( wchar_t ), seed );
			}
			RegCloseKey( key );
		}

		DWORD serial = 0;
		if ( GetVolumeInformationW( L"C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0 ) )
			seed = hash_bytes( &serial, sizeof( serial ), seed );

		using query_t = LONG( NTAPI* )( ULONG, PVOID, ULONG, PULONG );
		static const auto query = reinterpret_cast< query_t >( GetProcAddress(
			GetModuleHandleW( L"ntdll.dll" ), "NtQuerySystemInformation" ) );
		if ( query ) {
			std::uint8_t info[ 128 ]{ };
			ULONG length = 0;
			if ( query( 3, info, sizeof( info ), &length ) >= 0 )
				seed = hash_bytes( info, sizeof( std::uint64_t ), seed );
		}

		return seed;
	}

	inline void build_name( wchar_t* buffer, std::size_t count, const wchar_t* scope,
		const wchar_t* category, bool is_event ) {
		swprintf_s( buffer, count, is_event ? L"%ls%s.evt" : L"%ls%s", scope, category );
	}

	class security_t {
	public:
		bool build( const wchar_t* sddl ) {
			if ( m_descriptor )
				return true;
			if ( !sddl )
				return false;
			return ConvertStringSecurityDescriptorToSecurityDescriptorW(
				sddl, SDDL_REVISION_1, &m_descriptor, nullptr ) != 0;
		}

		~security_t( ) {
			if ( m_descriptor ) {
				LocalFree( m_descriptor );
				m_descriptor = nullptr;
			}
		}

		SECURITY_ATTRIBUTES* pointer( ) {
			m_attributes.nLength = sizeof( m_attributes );
			m_attributes.lpSecurityDescriptor = m_descriptor;
			m_attributes.bInheritHandle = FALSE;
			return &m_attributes;
		}

	private:
		PSECURITY_DESCRIPTOR m_descriptor = nullptr;
		SECURITY_ATTRIBUTES m_attributes{ };
	};

	class channel_t {
	public:
		~channel_t( ) { close( ); }

		bool create( const wchar_t* scope, const wchar_t* category, std::uint64_t salt,
			const wchar_t* sddl ) {
			close( );

			security_t security;
			m_security_ok = security.build( sddl );

			wchar_t section_name[ 128 ]{ };
			wchar_t event_name[ 128 ]{ };
			build_name( section_name, _countof( section_name ), scope, category, false );
			build_name( event_name, _countof( event_name ), scope, category, true );

			m_name_hash = hash_bytes( section_name,
				length_of( section_name ) * sizeof( wchar_t ), 1469598103934665603ull );

			m_mapping = CreateFileMappingW( INVALID_HANDLE_VALUE, security.pointer( ),
				PAGE_READWRITE, 0, sizeof( section_t ), section_name );
			if ( !m_mapping ) {
				m_last_error = GetLastError( );
				m_last_stage = 1;
				return false;
			}

			if ( GetLastError( ) == ERROR_ALREADY_EXISTS ) {
				m_last_error = ERROR_ALREADY_EXISTS; m_last_stage = 2;
				close( );
				return false;
			}

			m_section = reinterpret_cast< section_t* >( MapViewOfFile( m_mapping,
				FILE_MAP_ALL_ACCESS, 0, 0, sizeof( section_t ) ) );
			if ( !m_section ) {
				m_last_error = GetLastError( );
				m_last_stage = 3;
				close( );
				return false;
			}

			m_idle = CreateEventW( security.pointer( ), TRUE, FALSE, event_name );
			if ( !m_idle ) {
				m_last_error = GetLastError( );
				m_last_stage = 4;
				close( );
				return false;
			}

			std::memset( m_section, 0, sizeof( section_t ) );

			m_section->handshake.magic = k_magic;
			m_section->handshake.version = k_version;
			m_section->handshake.generation = static_cast< std::uint32_t >(
				GetTickCount64( ) ) ^ GetCurrentProcessId( );
			m_section->handshake.server_pid = GetCurrentProcessId( );
			m_section->handshake.heartbeat = GetTickCount64( );

			m_generation = m_section->handshake.generation;
			m_owner = true;
			return true;
		}

		bool open( const wchar_t* scope, const wchar_t* category, std::uint64_t salt ) {
			( void )salt;
			close( );

			wchar_t section_name[ 128 ]{ };
			wchar_t event_name[ 128 ]{ };
			build_name( section_name, _countof( section_name ), scope, category, false );
			build_name( event_name, _countof( event_name ), scope, category, true );

			m_mapping = OpenFileMappingW( FILE_MAP_READ | FILE_MAP_WRITE, FALSE, section_name );
			if ( !m_mapping ) {
				m_last_error = GetLastError( );
				m_name_hash = hash_bytes( section_name,
					length_of( section_name ) * sizeof( wchar_t ), 1469598103934665603ull );
				return false;
			}

			m_section = reinterpret_cast< section_t* >( MapViewOfFile( m_mapping,
				FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof( section_t ) ) );
			if ( !m_section ) {
				m_last_error = GetLastError( );
				close( );
				return false;
			}

			if ( m_section->handshake.magic != k_magic ||
				 m_section->handshake.version != k_version ) {
				m_last_error = 0xE0000001u;
				close( );
				return false;
			}

			m_generation = m_section->handshake.generation;

			m_idle = OpenEventW( EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, event_name );
			if ( !m_idle ) {
				m_last_error = GetLastError( );
				m_name_hash = hash_bytes( event_name,
					length_of( event_name ) * sizeof( wchar_t ), 1469598103934665603ull );
				close( );
				return false;
			}

			return true;
		}

		void close( ) {
			if ( m_idle ) {
				CloseHandle( m_idle );
				m_idle = nullptr;
			}
			if ( m_section ) {
				UnmapViewOfFile( m_section );
				m_section = nullptr;
			}
			if ( m_mapping ) {
				CloseHandle( m_mapping );
				m_mapping = nullptr;
			}
			m_owner = false;
			m_generation = 0;
		}

		void abandon( ) {
			m_mapping = nullptr;
			m_idle = nullptr;
			m_section = nullptr;
			m_owner = false;
			m_generation = 0;
		}

		bool valid( ) const {
			return m_section != nullptr;
		}

		section_t* section( ) const {
			return m_section;
		}

		std::uint32_t last_stage( ) const {
			return m_last_stage;
		}

		DWORD last_error( ) const {
			return m_last_error;
		}

		std::uint64_t name_hash( ) const {
			return m_name_hash;
		}

		bool security_ok( ) const {
			return m_security_ok;
		}

		void beat( ) const {
			if ( m_section )
				m_section->handshake.heartbeat = GetTickCount64( );
		}

		bool alive( ) const {
			if ( !m_section )
				return false;
			if ( m_section->handshake.magic != k_magic ||
				 m_section->handshake.version != k_version )
				return false;
			if ( m_section->handshake.generation != m_generation )
				return false;

			const auto now = GetTickCount64( );
			const auto beat = m_section->handshake.heartbeat;
			return now >= beat && ( now - beat ) <= k_dead_ms;
		}

		void signal( ) const {
			if ( m_idle )
				SetEvent( m_idle );
		}

		void wait( std::uint32_t timeout_ms ) const {
			if ( m_idle && WaitForSingleObject( m_idle, timeout_ms ) == WAIT_OBJECT_0 )
				ResetEvent( m_idle );
		}

	private:
		HANDLE      m_mapping = nullptr;
		HANDLE      m_idle = nullptr;
		section_t*  m_section = nullptr;
		bool        m_owner = false;
		std::uint32_t m_generation = 0;
		DWORD         m_last_error = 0;
		std::uint32_t m_last_stage = 0;
		std::uint64_t m_name_hash = 0;
		bool          m_security_ok = false;
	};
}
