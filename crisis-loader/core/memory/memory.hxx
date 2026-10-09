#pragma once
#include <functional>

namespace memory {
	std::function<bool( std::uint64_t, void*, std::size_t )> m_read_physical =
		[ ]( std::uint64_t addr, void* buffer, std::size_t size ) -> bool {
		return g_driver->read_physical_memory( addr, buffer, size );
		};
	std::function<bool( std::uint64_t, void*, std::size_t )> m_write_physical =
		[ ]( std::uint64_t addr, void* buffer, std::size_t size ) -> bool {
		return g_driver->write_physical_memory( addr, buffer, size );
		};

	bool read_physical( std::uint64_t addr, void* buffer, std::size_t size ) {
		if ( !utility::is_physical_address_valid( addr, size ) )
			return false;
		return m_read_physical( addr, buffer, size );
	}

	bool write_physical( std::uint64_t addr, void* buffer, std::size_t size ) {
		if ( !utility::is_physical_address_valid( addr, size ) )
			return false;
		return m_write_physical( addr, buffer, size );
	}

	bool read_virtual( std::uint64_t addr, void* buffer, std::size_t size ) {
		if ( !buffer && size )
			return false;

		auto output = static_cast< std::uint8_t* >( buffer );
		while ( size ) {
			const auto physical = g_paging->translate_linear( addr );
			if ( !physical )
				return false;

			const auto page_remaining = paging::page_4kb_size -
				( addr & paging::page_4kb_mask );
			const auto chunk = ( std::min ) (
				size, static_cast< std::size_t >( page_remaining ) );

			if ( !read_physical( physical, output, chunk ) )
				return false;

			addr += chunk;
			output += chunk;
			size -= chunk;
		}

		return true;
	}

	bool write_virtual( std::uint64_t addr, void* buffer, std::size_t size ) {
		if ( !buffer && size )
			return false;

		auto input = static_cast< std::uint8_t* >( buffer );
		while ( size ) {
			const auto physical = g_paging->translate_linear( addr );
			if ( !physical )
				return false;

			const auto page_remaining = paging::page_4kb_size -
				( addr & paging::page_4kb_mask );
			const auto chunk = ( std::min ) (
				size, static_cast< std::size_t >( page_remaining ) );

			if ( !write_physical( physical, input, chunk ) )
				return false;

			addr += chunk;
			input += chunk;
			size -= chunk;
		}

		return true;
	}

	template <typename T>
	bool read_virtual( std::uint64_t addr, T& out ) {
		return read_virtual( addr, &out, sizeof( T ) );
	}

	template <typename T>
	bool write_virtual( std::uint64_t addr, T val ) {
		return write_virtual( addr, &val, sizeof( T ) );
	}

	template <typename ret_t = std::uint64_t, typename addr_t>
	ret_t read( addr_t va ) {
		std::uint64_t va64;
		if constexpr ( std::is_pointer_v<addr_t> )
			va64 = reinterpret_cast< std::uint64_t >( va );
		else if constexpr ( std::is_integral_v<addr_t> )
			va64 = static_cast< std::uint64_t >( va );
		else
			static_assert( std::is_pointer_v<addr_t> || std::is_integral_v<addr_t>,
				"addr_t must be pointer or integral" );

		ret_t ret {};
		read_virtual( va64, &ret, sizeof( ret ) );
		return ret;
	}

	template <typename val_t, typename addr_t>
	bool write( addr_t va, val_t val ) {
		std::uint64_t va64;
		if constexpr ( std::is_pointer_v<addr_t> )
			va64 = reinterpret_cast< std::uint64_t >( va );
		else if constexpr ( std::is_integral_v<addr_t> )
			va64 = static_cast< std::uint64_t >( va );
		else
			static_assert( std::is_pointer_v<addr_t> || std::is_integral_v<addr_t>,
				"addr_t must be pointer or integral" );

		return write_virtual( va64, &val, sizeof( val_t ) );
	}

	template <typename ret_t = std::uint64_t, typename addr_t>
	ret_t read_pa( addr_t va ) {
		std::uint64_t va64;
		if constexpr ( std::is_pointer_v<addr_t> )
			va64 = reinterpret_cast< std::uint64_t >( va );
		else if constexpr ( std::is_integral_v<addr_t> )
			va64 = static_cast< std::uint64_t >( va );
		else
			static_assert( std::is_pointer_v<addr_t> || std::is_integral_v<addr_t>,
				"addr_t must be pointer or integral" );

		ret_t ret {};
		read_physical( va64, &ret, sizeof( ret ) );
		return ret;
	}

	template <typename val_t, typename addr_t>
	bool write_pa( addr_t va, val_t val ) {
		std::uint64_t va64;
		if constexpr ( std::is_pointer_v<addr_t> )
			va64 = reinterpret_cast< std::uint64_t >( va );
		else if constexpr ( std::is_integral_v<addr_t> )
			va64 = static_cast< std::uint64_t >( va );
		else
			static_assert( std::is_pointer_v<addr_t> || std::is_integral_v<addr_t>,
				"addr_t must be pointer or integral" );

		return write_physical( va64, &val, sizeof( val_t ) );
	}
}
