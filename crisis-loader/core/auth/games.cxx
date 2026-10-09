#include "games.hxx"

#include <algorithm>
#include <array>

namespace loader_games {
	namespace {
		constexpr std::array modules {
			module_t { "fortnite", "Fortnite", "crisis-fortnite.dll" },
			module_t { "rust", "Rust", "crisis-rust.dll" }
		};
	}

	std::span< const module_t > catalog( ) noexcept {
		return modules;
	}

	const module_t* find( const std::string_view id ) noexcept {
		const auto module = std::find_if( modules.begin( ), modules.end( ),
			[ id ]( const module_t& candidate ) { return candidate.id == id; } );
		return module == modules.end( ) ? nullptr : &*module;
	}

	std::vector< const module_t* > entitled_modules( const std::span< const std::string > entitlements ) {
		std::vector< const module_t* > result;
		result.reserve( std::min( modules.size( ), entitlements.size( ) ) );
		for ( const auto& module : modules ) {
			if ( std::find( entitlements.begin( ), entitlements.end( ), module.id ) != entitlements.end( ) )
				result.push_back( &module );
		}
		return result;
	}
}
