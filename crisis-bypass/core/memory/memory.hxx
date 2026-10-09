#pragma once
#include <functional>

namespace memory {
	std::function<bool( std::uint64_t, void*, std::size_t )> m_read_physical;
	std::function<bool( std::uint64_t, void*, std::size_t )> m_write_physical;

	bool read_physical( std::uint64_t addr, void* buffer, std::size_t size ) {
		if ( !utility::is_physical_address_valid( addr, size ) ) return false;
		if ( !m_read_physical ) return false;
		return m_read_physical( addr, buffer, size );
	}

	bool write_physical( std::uint64_t addr, void* buffer, std::size_t size ) {
		if ( !utility::is_physical_address_valid( addr, size ) ) return false;
		if ( !m_write_physical ) return false;
		return m_write_physical( addr, buffer, size );
	}

	bool read_virtual( std::uint64_t addr, void* buffer, std::size_t size ) {
		if ( !buffer && size ) return false;

		auto output = static_cast< std::uint8_t* >( buffer );
		while ( size ) {
			const auto physical = g_paging->translate_linear( addr );
			if ( !physical ) return false;

			const auto page_remaining = paging::page_4kb_size - ( addr & paging::page_4kb_mask );
			const auto chunk = ( std::min )( size, static_cast< std::size_t >( page_remaining ) );

			if ( !read_physical( physical, output, chunk ) ) return false;

			addr   += chunk;
			output += chunk;
			size   -= chunk;
		}
		return true;
	}

	bool write_virtual( std::uint64_t addr, void* buffer, std::size_t size ) {
		if ( !buffer && size ) return false;

		auto input = static_cast< std::uint8_t* >( buffer );
		while ( size ) {
			const auto physical = g_paging->translate_linear( addr );
			if ( !physical ) return false;

			const auto page_remaining = paging::page_4kb_size - ( addr & paging::page_4kb_mask );
			const auto chunk = ( std::min )( size, static_cast< std::size_t >( page_remaining ) );

			if ( !write_physical( physical, input, chunk ) ) return false;

			addr  += chunk;
			input += chunk;
			size  -= chunk;
		}
		return true;
	}

	bool read_system( std::uint64_t addr, void* buffer, std::size_t size ) {
		if ( !buffer && size ) return false;
		const auto old_dtb = g_paging->attach_system( );

		auto output = static_cast< std::uint8_t* >( buffer );
		while ( size ) {
			const auto physical = g_paging->translate_linear( addr );
			if ( !physical ) {
				g_paging->attach_process( *old_dtb );
				return false;
			}

			const auto page_remaining = paging::page_4kb_size - ( addr & paging::page_4kb_mask );
			const auto chunk = ( std::min ) ( size, static_cast< std::size_t >( page_remaining ) );

			if ( !read_physical( physical, output, chunk ) ) {
				g_paging->attach_process( *old_dtb );
				return false;
			}

			addr += chunk;
			output += chunk;
			size -= chunk;
		}

		g_paging->attach_process( *old_dtb );
		return true;
	}

	bool write_system( std::uint64_t addr, void* buffer, std::size_t size ) {
		if ( !buffer && size ) return false;
		const auto old_dtb = g_paging->attach_system( );

		auto input = static_cast< std::uint8_t* >( buffer );
		while ( size ) {
			const auto physical = g_paging->translate_linear( addr );
			if ( !physical ) {
				g_paging->attach_process( *old_dtb );
				return false;
			}

			const auto page_remaining = paging::page_4kb_size - ( addr & paging::page_4kb_mask );
			const auto chunk = ( std::min ) ( size, static_cast< std::size_t >( page_remaining ) );

			if ( !write_physical( physical, input, chunk ) ) {
				g_paging->attach_process( *old_dtb );
				return false;
			}

			addr += chunk;
			input += chunk;
			size -= chunk;
		}

		g_paging->attach_process( *old_dtb );
		return true;
	}

	template<typename T>
	bool read_virtual( std::uint64_t addr, T& out ) {
		return read_virtual( addr, &out, sizeof( T ) );
	}

	template<typename T>
	bool write_virtual( std::uint64_t addr, T val ) {
		return write_virtual( addr, &val, sizeof( T ) );
	}

	template<typename T = std::uint64_t>
	T read_pa( std::uint64_t pa ) {
		T val {};
		read_physical( pa, &val, sizeof( T ) );
		return val;
	}

	template<typename ret_t = std::uint64_t, typename addr_t>
	ret_t read( addr_t va ) {
		std::uint64_t va64;
		if constexpr ( std::is_pointer_v<addr_t> )
			va64 = reinterpret_cast< std::uint64_t >( va );
		else
			va64 = static_cast< std::uint64_t >( va );
		ret_t ret {};
		read_virtual( va64, &ret, sizeof( ret ) );
		return ret;
	}

	template<typename val_t, typename addr_t>
	bool write( addr_t va, val_t val ) {
		std::uint64_t va64;
		if constexpr ( std::is_pointer_v<addr_t> )
			va64 = reinterpret_cast< std::uint64_t >( va );
		else
			va64 = static_cast< std::uint64_t >( va );
		return write_virtual( va64, &val, sizeof( val_t ) );
	}

	std::uint64_t pattern_scan(
		std::uint64_t base,
		std::uintptr_t size,
		const char* pattern ) {

		std::vector< int > pat;
		for ( const char* p = pattern; *p; ) {
			while ( *p == ' ' ) ++p;
			if ( !*p ) break;
			if ( *p == '?' ) {
				pat.push_back( -1 );
				while ( *p == '?' ) ++p;
			}
			else {
				char* end {};
				pat.push_back( static_cast< int >( std::strtol( p, &end, 16 ) ) );
				p = end;
			}
		}

		const auto pat_size = pat.size( );
		if ( pat_size == 0 || size < pat_size )
			return 0;

		constexpr std::uintptr_t chunk = 0x1000;
		std::vector< std::uint8_t > buf( chunk + pat_size - 1 );

		for ( auto idx = 0; idx + pat_size <= size; ) {
			const auto scan_len = min( chunk, size - idx );
			auto extra = min( pat_size - 1, size - idx - scan_len );

			if ( !read_virtual( base + idx, buf.data( ), scan_len ) ) {
				idx += scan_len;
				continue;
			}
			if ( extra > 0 && !read_virtual( base + idx + scan_len, buf.data( ) + scan_len, extra ) )
				extra = 0;

			const auto readable = scan_len + extra;
			for ( std::uintptr_t j = 0; j + pat_size <= readable && idx + j + pat_size <= size; ++j ) {
				bool match = true;
				for ( std::uintptr_t k = 0; k < pat_size; ++k ) {
					if ( pat[ k ] != -1 && buf[ j + k ] != static_cast< std::uint8_t >( pat[ k ] ) ) {
						match = false;
						break;
					}
				}
				if ( match )
					return base + idx + j;
			}
			idx += scan_len;
		}
		return 0;
	}
}
