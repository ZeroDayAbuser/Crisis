#pragma once

namespace pdb {
	const std::uint8_t* m_data;
	std::uint32_t       m_size;
	std::uint64_t       m_delta {};

#pragma pack(push, 1)
	struct pdb_blob_header_t {
		std::uint32_t magic;
		std::uint32_t version;
		std::uint32_t count;
		std::uint32_t data_size;
	};
#pragma pack(pop)

	static constexpr std::uint32_t k_magic = 0x42445043u;
	static constexpr std::uint32_t k_version = 1u;

	static bool str_eq( const char* a, std::uint8_t alen, const char* b ) {
		if ( !b ) return false;
		std::uint8_t blen = 0;
		while ( b[ blen ] && blen < 255 ) ++blen;
		if ( alen != blen ) return false;
		return memcmp( a, b, alen ) == 0;
	}

	std::uint64_t lookup( std::uint8_t type, const char* key, const char* sub ) {
		if ( !m_data || m_size < sizeof( pdb_blob_header_t ) ) return 0;
		const auto* hdr = reinterpret_cast< const pdb_blob_header_t* >( m_data );
		if ( hdr->magic != k_magic || hdr->version != k_version ) return 0;

		const std::uint8_t* ptr = m_data + sizeof( pdb_blob_header_t );
		const std::uint8_t* end = m_data + hdr->data_size;

		while ( ptr + 2 <= end ) {
			const auto etype = ptr[ 0 ];
			const auto name_len = ptr[ 1 ];
			ptr += 2;
			if ( ptr + name_len + sizeof( std::uint64_t ) > end ) break;
			const char* ename = reinterpret_cast< const char* >( ptr );
			ptr += name_len;
			const auto value = *reinterpret_cast< const std::uint64_t* >( ptr );
			ptr += sizeof( std::uint64_t );

			if ( etype != type ) continue;

			if ( type == 0 ) {
				if ( str_eq( ename, name_len, key ) ) return value + m_delta;
			}
			else if ( type == 2 ) {
				if ( str_eq( ename, name_len, key ) ) return value;
			}
			else {
				std::uint8_t sep = 0;
				for ( std::uint8_t i = 0; i + 1 < name_len; ++i )
					if ( ename[ i ] == ':' && ename[ i + 1 ] == ':' ) { sep = i; break; }
				if ( !sep ) continue;
				if ( str_eq( ename, sep, key ) &&
					str_eq( ename + sep + 2,
						static_cast< std::uint8_t >( name_len - sep - 2 ), sub ) )
					return value;
			}
		}
		return 0;
	}

	std::uint64_t get_symbol( const char* name ) {
		return lookup( 0, name, nullptr );
	}

	const char* get_symbol_name_by_address(
		std::uint64_t address,
		std::uint64_t* value_out,
		std::uint8_t*  length_out ) {
		if ( !m_data || m_size < sizeof( pdb_blob_header_t ) ) return nullptr;
		const auto* hdr = reinterpret_cast< const pdb_blob_header_t* >( m_data );
		if ( hdr->magic != k_magic || hdr->version != k_version ) return nullptr;

		const std::uint8_t* ptr = m_data + sizeof( pdb_blob_header_t );
		const std::uint8_t* end = m_data + hdr->data_size;

		std::uint64_t best = 0;
		const char*   best_name = nullptr;
		std::uint8_t  best_len = 0;

		while ( ptr + 2 <= end ) {
			const auto etype    = ptr[ 0 ];
			const auto name_len = ptr[ 1 ];
			ptr += 2;
			if ( ptr + name_len + sizeof( std::uint64_t ) > end ) break;
			const char* ename = reinterpret_cast< const char* >( ptr );
			ptr += name_len;
			const auto value = *reinterpret_cast< const std::uint64_t* >( ptr );
			ptr += sizeof( std::uint64_t );

			if ( etype != 0 ) continue;
			const auto adjusted = value + m_delta;
			if ( adjusted > address || adjusted < best ) continue;

			best      = adjusted;
			best_name = ename;
			best_len  = static_cast< std::uint8_t >( name_len );
		}

		if ( !best_name ) return nullptr;
		if ( value_out ) *value_out = best;
		if ( length_out ) *length_out = best_len;
		return best_name;
	}

	std::uint64_t blob_ntoskrnl_base( ) {
		if ( !m_data || m_size < sizeof( pdb_blob_header_t ) ) return 0;
		const auto* hdr = reinterpret_cast< const pdb_blob_header_t* >( m_data );
		if ( hdr->magic != k_magic || hdr->version != k_version ) return 0;

		const std::uint8_t* ptr = m_data + sizeof( pdb_blob_header_t );
		const std::uint8_t* end = m_data + hdr->data_size;

		while ( ptr + 2 <= end ) {
			const auto etype    = ptr[ 0 ];
			const auto name_len = ptr[ 1 ];
			ptr += 2;
			if ( ptr + name_len + sizeof( std::uint64_t ) > end ) break;
			const char* ename = reinterpret_cast< const char* >( ptr );
			ptr += name_len;
			const auto value = *reinterpret_cast< const std::uint64_t* >( ptr );
			ptr += sizeof( std::uint64_t );

			if ( etype == 0 && str_eq( ename, name_len, "ntoskrnl_base" ) )
				return value;
		}
		return 0;
	}

	void rebase( std::uint64_t runtime_base ) {
		const auto blob_base = blob_ntoskrnl_base( );
		m_delta = ( runtime_base && blob_base ) ? ( runtime_base - blob_base ) : 0;
	}

	std::uint64_t get_member( const char* type_name, const char* member_name ) {
		return lookup( 1, type_name, member_name );
	}

	std::uint64_t get_struct_size( const char* type_name ) {
		return lookup( 2, type_name, nullptr );
	}

	std::uint64_t get_bit_position( const char* type_name, const char* member_name ) {
		return lookup( 3, type_name, member_name );
	}

	std::uint64_t get_function_param_count( const char* function_name ) {
		if ( !m_data || m_size < sizeof( pdb_blob_header_t ) ) return 0;
		const auto* hdr = reinterpret_cast< const pdb_blob_header_t* >( m_data );
		if ( hdr->magic != k_magic || hdr->version != k_version ) return 0;

		const std::uint8_t* ptr = m_data + sizeof( pdb_blob_header_t );
		const std::uint8_t* end = m_data + hdr->data_size;

		while ( ptr + 2 <= end ) {
			const auto etype    = ptr[ 0 ];
			const auto name_len = ptr[ 1 ];
			ptr += 2;
			if ( ptr + name_len + sizeof( std::uint64_t ) > end ) break;
			const char* ename = reinterpret_cast< const char* >( ptr );
			ptr += name_len;
			const auto value = *reinterpret_cast< const std::uint64_t* >( ptr );
			ptr += sizeof( std::uint64_t );

			if ( etype != 4 ) continue;
			if ( str_eq( ename, name_len, function_name ) ) return value;
		}

		return 0;
	}

	void get_function_param_name(
		const char*   function_name,
		std::uint64_t index,
		char*         out,
		std::size_t   out_size ) {
		if ( !out || !out_size ) return;
		out[ 0 ] = '\0';
		if ( !m_data || m_size < sizeof( pdb_blob_header_t ) ) return;
		const auto* hdr = reinterpret_cast< const pdb_blob_header_t* >( m_data );
		if ( hdr->magic != k_magic || hdr->version != k_version ) return;

		const std::uint8_t* ptr = m_data + sizeof( pdb_blob_header_t );
		const std::uint8_t* end = m_data + hdr->data_size;

		while ( ptr + 2 <= end ) {
			const auto etype    = ptr[ 0 ];
			const auto name_len = ptr[ 1 ];
			ptr += 2;
			if ( ptr + name_len + sizeof( std::uint64_t ) > end ) break;
			const char* ename = reinterpret_cast< const char* >( ptr );
			ptr += name_len;
			const auto value = *reinterpret_cast< const std::uint64_t* >( ptr );
			ptr += sizeof( std::uint64_t );

			if ( etype != 5 || value != index ) continue;

			std::uint8_t sep = 0;
			for ( std::uint8_t i = 0; i + 1 < name_len; ++i )
				if ( ename[ i ] == ':' && ename[ i + 1 ] == ':' ) { sep = i; break; }
			if ( !sep ) continue;
			if ( !str_eq( ename, sep, function_name ) ) continue;

			const auto sub_len = static_cast< std::uint8_t >( name_len - sep - 2 );
			const auto copy = sub_len < out_size - 1 ? sub_len : out_size - 1;
			memcpy( out, ename + sep + 2, copy );
			out[ copy ] = '\0';
			return;
		}
	}
}