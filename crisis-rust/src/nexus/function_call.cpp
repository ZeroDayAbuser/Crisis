#include <src/nexus/function_call.hxx>

#include <Windows.h>

#ifdef _WIN64
extern "C" void nexus_invoke_registers(
	nexus::registers_t* registers,
	std::uintptr_t address );
#endif

namespace {

	bool executable_address( std::uintptr_t address ) {
		MEMORY_BASIC_INFORMATION memory {};
		if ( VirtualQuery( reinterpret_cast< const void* >( address ), &memory,
			sizeof( memory ) ) != sizeof( memory ) )
			return false;

		const DWORD protection = memory.Protect & 0xffu;
		return memory.State == MEM_COMMIT
			&& ( memory.Protect & PAGE_GUARD ) == 0
			&& ( protection == PAGE_EXECUTE
				|| protection == PAGE_EXECUTE_READ
				|| protection == PAGE_EXECUTE_READWRITE
				|| protection == PAGE_EXECUTE_WRITECOPY );
	}

}

namespace nexus {

	function_t::function_t( const char* name, std::uintptr_t address ) {
		if ( !name || !name[ 0 ] ) {
			SetLastError( ERROR_INVALID_PARAMETER );
			return;
		}
		if ( !executable_address( address ) ) {
			SetLastError( ERROR_INVALID_ADDRESS );
			return;
		}

		m_plan = bypass::nexus::create_plan( name );
		if ( m_plan == Nexus::InvalidPlan )
			return;
		m_address = address;
	}

	bool function_t::valid( ) const {
		return m_address && m_plan != Nexus::InvalidPlan;
	}

	bool function_t::call( registers_t& registers, Nexus::CallMode mode,
		Nexus::CallReport* report ) const {
#ifndef _WIN64
		static_cast< void >( registers );
		static_cast< void >( mode );
		static_cast< void >( report );
		SetLastError( ERROR_NOT_SUPPORTED );
		return false;
#else
		if ( !valid( ) ) {
			SetLastError( ERROR_INVALID_HANDLE );
			if ( report ) {
				*report = {};
				report->Error = ERROR_INVALID_HANDLE;
			}
			return false;
		}

		Nexus::CallOptions options {};
		options.Mode = mode;
		if ( !bypass::nexus::begin_call( m_plan, options ) ) {
			if ( report ) {
				*report = {};
				report->Error = GetLastError( );
			}
			return false;
		}
		nexus_invoke_registers( &registers, m_address );
		const auto result = bypass::nexus::end_call( );
		if ( report )
			*report = result;
		SetLastError( result.Succeeded ? ERROR_SUCCESS : result.Error );
		return result.Succeeded;
#endif
	}

	std::uintptr_t function_t::address( ) const {
		return m_address;
	}

	Nexus::PlanId function_t::plan( ) const {
		return m_plan;
	}

}
