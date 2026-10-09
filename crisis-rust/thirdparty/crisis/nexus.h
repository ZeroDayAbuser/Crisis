#pragma once
#include "../../../crisis-bypass/core/nexus/runtime.h"

namespace bypass {
namespace nexus {

	inline bool attach( ) { return g_api.m_nexus_attach( ); }
	inline void detach( ) { if ( g_api.m_nexus_detach ) g_api.m_nexus_detach( ); }

	inline bool configure( const Nexus::Config& cfg ) { return g_api.m_nexus_configure( &cfg ); }

	inline bool add_section( std::uintptr_t local, std::uintptr_t remote, std::size_t size,
		Nexus::Policy policy, const char* name = nullptr ) {
		return g_api.m_nexus_add_section( local, remote, size,
			static_cast< std::uint32_t >( policy ), name );
	}
	inline bool add_heap_range( std::uintptr_t address, std::size_t size ) {
		return g_api.m_nexus_add_heap_range( address, size );
	}
	inline bool add_override( std::uintptr_t address, const void* data, std::size_t size,
		const char* name = nullptr ) {
		return g_api.m_nexus_add_override( address, data, size, name );
	}

	inline std::uint32_t create_plan( const char* name ) { return g_api.m_nexus_create_plan( name ); }
	inline bool plan_add_range( std::uint32_t plan, std::uintptr_t address, std::size_t size ) {
		return g_api.m_nexus_plan_add_range( plan, address, size );
	}
	inline bool plan_remove_range( std::uint32_t plan, std::uintptr_t address, std::size_t size ) {
		return g_api.m_nexus_plan_remove_range( plan, address, size );
	}
	inline bool plan_set_ranges( std::uint32_t plan, const Nexus::VolatileRange* ranges, std::size_t count ) {
		return g_api.m_nexus_plan_set_ranges( plan, ranges, count );
	}
	inline bool plan_save( std::uint32_t plan, const wchar_t* path ) {
		return g_api.m_nexus_plan_save( plan, path );
	}
	inline bool plan_load( const wchar_t* path, std::uint32_t& out_plan ) {
		return g_api.m_nexus_plan_load( path, &out_plan );
	}

	inline std::uint32_t create_object( const char* name, std::uintptr_t root ) {
		return g_api.m_nexus_create_object( name, root );
	}
	inline bool object_set_root( std::uint32_t object, std::uintptr_t root ) {
		return g_api.m_nexus_object_set_root( object, root );
	}
	inline std::uint32_t object_add_edge( std::uint32_t object, std::uint32_t parent, std::size_t offset,
		std::uint32_t every = 1, const char* name = nullptr, bool nullable = false ) {
		return g_api.m_nexus_object_add_edge( object, parent, offset, every, name, nullable );
	}
	inline std::uintptr_t object_edge_target( std::uint32_t object, std::uint32_t edge ) {
		return g_api.m_nexus_object_edge_target( object, edge );
	}
	inline bool object_validate( std::uint32_t object, bool force = false ) {
		return g_api.m_nexus_object_validate( object, force );
	}
	inline bool object_seed( std::uint32_t object, std::uint32_t plan, std::uint32_t edge = Nexus::InvalidPointerEdge ) {
		return g_api.m_nexus_object_seed( object, plan, edge );
	}
	inline bool object_set_cadence( std::uint32_t object, std::uintptr_t address, Nexus::RefreshCadence cadence ) {
		return g_api.m_nexus_object_set_cadence( object, address, static_cast< std::uint32_t >( cadence ) );
	}

	inline std::uint64_t begin_epoch( ) { return g_api.m_nexus_begin_epoch( ); }
	inline std::uint64_t current_epoch( ) { return g_api.m_nexus_current_epoch( ); }
	inline bool object_refresh( std::uint32_t object, bool force = false, bool validate = true ) {
		return g_api.m_nexus_object_refresh( object, force, validate );
	}

	inline bool begin_call( std::uint32_t plan, const Nexus::CallOptions& options ) {
		return g_api.m_nexus_begin_call( plan, &options );
	}
	inline Nexus::CallReport end_call( std::uint64_t invoke_ticks = 0 ) {
		Nexus::CallReport report {};
		g_api.m_nexus_end_call( invoke_ticks, &report );
		return report;
	}

	inline bool begin_batch( ) { return g_api.m_nexus_begin_batch( ); }
	inline bool prepare_plan( std::uint32_t plan ) { return g_api.m_nexus_prepare_plan( plan ); }
	inline Nexus::BatchReport end_batch( ) {
		Nexus::BatchReport report {};
		g_api.m_nexus_end_batch( &report );
		return report;
	}

	inline bool reset( ) { return g_api.m_nexus_reset( ); }

	inline Nexus::Profile profile( ) {
		Nexus::Profile out {};
		g_api.m_nexus_get_profile( &out );
		return out;
	}
	inline std::size_t copy_fault_events( Nexus::FaultEvent* out, std::size_t capacity ) {
		return g_api.m_nexus_copy_fault_events( out, capacity );
	}
	inline std::size_t copy_trace_events( Nexus::TraceEvent* out, std::size_t capacity ) {
		return g_api.m_nexus_copy_trace_events( out, capacity );
	}
	inline std::size_t copy_call_reports( Nexus::CallReport* out, std::size_t capacity ) {
		return g_api.m_nexus_copy_call_reports( out, capacity );
	}
	inline std::size_t copy_plan_summaries( Nexus::PlanSummary* out, std::size_t capacity ) {
		return g_api.m_nexus_copy_plan_summaries( out, capacity );
	}
	inline std::size_t copy_object_summaries( Nexus::ObjectSummary* out, std::size_t capacity ) {
		return g_api.m_nexus_copy_object_summaries( out, capacity );
	}
	inline std::size_t copy_plan_pages( std::uint32_t plan, Nexus::PlanPageInfo* out, std::size_t capacity ) {
		return g_api.m_nexus_copy_plan_pages( plan, out, capacity );
	}
	inline std::size_t copy_object_pages( std::uint32_t object, Nexus::ObjectPageInfo* out, std::size_t capacity ) {
		return g_api.m_nexus_copy_object_pages( object, out, capacity );
	}
	inline std::size_t copy_pointer_edges( std::uint32_t object, Nexus::PointerEdgeInfo* out, std::size_t capacity ) {
		return g_api.m_nexus_copy_pointer_edges( object, out, capacity );
	}

	inline void set_benchmark( bool enabled ) { g_api.m_nexus_set_benchmark( enabled ); }
	inline bool get_benchmark( ) { return g_api.m_nexus_get_benchmark( ); }
	inline std::size_t copy_benchmark_samples( Nexus::BenchmarkSample* out, std::size_t capacity ) {
		return g_api.m_nexus_copy_benchmark_samples( out, capacity );
	}
	inline Nexus::BenchmarkSummary benchmark_summary( ) {
		Nexus::BenchmarkSummary out {};
		g_api.m_nexus_get_benchmark_summary( &out );
		return out;
	}

}
}
