#pragma once

#include "../../../crisis-bypass/core/mapping/mapping.hxx"

namespace bypass {
	struct c_bypass : mapping::vtable_t {
		bool resolve( int retries = 50, DWORD delay_ms = 100 ) {
			mapping::c_mapping map;
			return map.open_consumer( *this, retries, delay_ms );
		}

		bool valid( ) const {
			return mapping::vtable_valid( *this );
		}
	};
}
