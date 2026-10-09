#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include <cstring>
#include <optional>
#include <type_traits>
#include <vector>
#include <thirdparty/crisis/bypass.hpp>
#include <thirdparty/crisis/nexus.h>

namespace nexus {

	struct registers_t {
		std::uint64_t rax {};
		std::uint64_t rbx {};
		std::uint64_t rcx {};
		std::uint64_t rdx {};
		std::uint64_t rsi {};
		std::uint64_t rdi {};
		std::uint64_t rbp {};
		std::uint64_t r8 {};
		std::uint64_t r9 {};
		std::uint64_t r10 {};
		std::uint64_t r11 {};
		std::uint64_t r12 {};
		std::uint64_t r13 {};
		std::uint64_t r14 {};
		std::array< std::uint64_t, 2 > xmm0 {};
	};

	static_assert( offsetof( registers_t, rax ) == 0x00 );
	static_assert( offsetof( registers_t, rbx ) == 0x08 );
	static_assert( offsetof( registers_t, rcx ) == 0x10 );
	static_assert( offsetof( registers_t, rdx ) == 0x18 );
	static_assert( offsetof( registers_t, rsi ) == 0x20 );
	static_assert( offsetof( registers_t, rdi ) == 0x28 );
	static_assert( offsetof( registers_t, rbp ) == 0x30 );
	static_assert( offsetof( registers_t, r8 ) == 0x38 );
	static_assert( offsetof( registers_t, r9 ) == 0x40 );
	static_assert( offsetof( registers_t, r10 ) == 0x48 );
	static_assert( offsetof( registers_t, r11 ) == 0x50 );
	static_assert( offsetof( registers_t, r12 ) == 0x58 );
	static_assert( offsetof( registers_t, r13 ) == 0x60 );
	static_assert( offsetof( registers_t, r14 ) == 0x68 );
	static_assert( offsetof( registers_t, xmm0 ) == 0x70 );
	static_assert( sizeof( registers_t ) == 0x80 );

	class function_t {
	public:
		function_t( const char* name, std::uintptr_t address );

		[[nodiscard]] bool valid( ) const;
		[[nodiscard]] bool call(
			registers_t& registers,
			Nexus::CallMode mode = Nexus::CallMode::FaultDrivenFresh,
			Nexus::CallReport* report = nullptr ) const;

		[[nodiscard]] std::uintptr_t address( ) const;
		[[nodiscard]] Nexus::PlanId plan( ) const;

	private:
		std::uintptr_t m_address {};
		Nexus::PlanId m_plan { Nexus::InvalidPlan };
	};

}
