#include <deps/includes.h>

namespace cache {

	std::int32_t c_cache::find_offset( const wchar_t* path, const wchar_t* field ) {
		auto object = engine::u_object::find_object<engine::u_object*>( path );
		if ( !object ) {
			logging::print( obf( "failed to find object for %ls." ), field );
			return -1;
		}

		auto string_library = m_string_library.load( );
		auto field_name = string_library->conv_wstring_to_name( field );
		if ( !field_name ) {
			logging::print( obf( "failed to convert string to name for %ls." ), field );
			return -1;
		}

		auto decrypt_pointer = [ ]( std::uint64_t enc ) -> std::uint64_t {
			if ( ( enc & 0x1ULL ) == 0 ) return enc;
			const std::uint64_t v0 = ( enc & 0xFFFFFFFFFFFFFFFEULL );
			const std::uint64_t v1 = ( v0 * 0x5C05890A2CB5EB0BULL );
			const std::uint64_t v2 = ( v1 + 0xC6B5786A895020D0ULL );
			const std::uint64_t v3 = ( v2 ^ 0x8FACD9B2CDEC62AULL );
			const std::uint32_t v4 = ( static_cast< std::uint32_t >( v3 ) & 0xFFFEULL );
			const std::uint64_t v5 = ( v3 >> 0x28ULL );
			const std::uint32_t v6 = ( static_cast< std::uint32_t >( v5 ) & 0xFF0000ULL );
			const std::uint64_t v7 = ( v3 >> 0x18ULL );
			const std::uint32_t v8 = ( static_cast< std::uint32_t >( v7 ) & 0xFF000000ULL );
			const std::uint64_t v9 = ( v3 >> 0x8ULL );
			const std::uint64_t v10 = ( v9 & 0xFF00000000ULL );
			const std::uint64_t v11 = ( v10 + v8 );
			const std::uint64_t v12 = ( v11 + v6 );
			const std::uint64_t v13 = ( v3 << 0x8ULL );
			const std::uint64_t v14 = ( v13 & 0xFF0000000000ULL );
			const std::uint64_t v15 = ( v3 << 0x18ULL );
			const std::uint64_t v16 = ( v15 & 0xFF000000000000ULL );
			const std::uint64_t v17 = ( v16 + v14 );
			const std::uint64_t v18 = ( v3 << 0x28ULL );
			const std::uint64_t v19 = ( v18 & 0xFF00000000000000ULL );
			const std::uint64_t v20 = ( v4 + 0xD5DE066BB4C9427AULL );
			const std::uint64_t v21 = ( v20 + v19 );
			const std::uint64_t v22 = ( v21 + v17 );
			const std::uint64_t v23 = ( v22 + v12 );
			return v23;
			};

		const auto target_index = field_name.comparison_index;
		const auto object_address = reinterpret_cast< std::uintptr_t >( object );
		auto field_pointer = decrypt_pointer( bypass::read<std::uint64_t>( object_address + 0x88 ) );

		while ( utility::is_valid_pointer( field_pointer ) ) {
			auto comparison_index = bypass::read<std::uint32_t>( field_pointer + 0x18 );
			if ( comparison_index && comparison_index == target_index )
				break;
			field_pointer = decrypt_pointer( bypass::read<std::uint64_t>( field_pointer + 0x8 ) );
		}

		if ( !utility::is_valid_pointer( field_pointer ) ) {
			logging::print( obf( "failed to find field pointer for %ls." ), field );
			return -1;
		}

		const auto encrypted_offset_internal = bypass::read<std::uint32_t>( field_pointer + 0x64 );
		const std::uint32_t v0 = ( encrypted_offset_internal * 0x94EE03ULL );
		const std::uint32_t v1 = ( v0 + 0x6E98A46ULL );
		return static_cast< std::int32_t >( v1 );
	}
}