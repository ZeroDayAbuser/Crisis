#pragma once

namespace exception {
	extern "C" void exception_callout( exception::data_request_t* request );

	// The assembler stub copies 0x38 bytes into and out of its own frame slot, and the
	// kernel writes the response there using its own sizeof(data_request_t). If the two
	// ever disagree the excess lands on the stub's saved rbx and return address, so the
	// caller fails its stack cookie check long after the write - 0xC0000409, process
	// gone with no dump and no message. Pin the size here so it cannot drift silently.
	static_assert( sizeof( exception::data_request_t ) == 0x38,
		"data_request_t changed size - update the frame slot in communication.asm" );

	class c_exception {
	public:
		std::uint64_t m_eprocess;
		std::uint64_t m_base_address;
		std::uint64_t m_directory_table_base;

		bool setup( ) {
			if ( !is_running( ) ) {
				logging::print( obf( "driver is not answering." ) );
				return false;
			}

			return true;
		}

		bool attach( std::uint32_t process_id ) {
			this->m_eprocess = get_eprocess( process_id );
			if ( !m_eprocess ) {
				logging::print( obf( "failed to find process." ) );
				return false;
			}

			logging::print( obf( "found process: 0x%llx" ), m_eprocess );
			this->m_base_address = get_base_address( process_id );
			if ( !m_base_address ) {
				logging::print( obf( "failed to find base address." ) );
				return false;
			}

			logging::print( obf( "found base address: 0x%llx" ), m_base_address );
			this->m_directory_table_base = get_directory_table_base( process_id );
			if ( !m_directory_table_base ) {
				logging::print( obf( "failed to find directory table base." ) );
				return false;
			}

			logging::print( obf( "found directory table base: 0x%llx" ), m_directory_table_base );
			return true;
		}

		std::uint64_t get_eprocess( std::uint32_t process_id ) {
			exception::data_request_t request {};
			request.m_type = static_cast< std::uint32_t >(
				exception::e_data_type::get_eprocess );
			request.m_process_id = process_id;
			return submit( request ) ? request.m_result : 0;
		}

		std::uint64_t get_base_address( std::uint32_t process_id ) {
			exception::data_request_t request {};
			request.m_type = static_cast< std::uint32_t >(
				exception::e_data_type::get_base_address );
			request.m_process_id = process_id;
			return submit( request ) ? request.m_result : 0;
		}

		std::uint64_t get_directory_table_base( std::uint32_t process_id ) {
			exception::data_request_t request {};
			request.m_type = static_cast< std::uint32_t >(
				exception::e_data_type::get_directory_table_base );
			request.m_process_id = process_id;
			return submit( request ) ? request.m_result : 0;
		}

		std::uint64_t translate_linear( std::uint32_t process_id, std::uint64_t address ) {
			exception::data_request_t request {};
			request.m_type = static_cast< std::uint32_t >(
				exception::e_data_type::translate_linear );
			request.m_process_id = process_id;
			request.m_address = address;
			return submit( request ) ? request.m_result : 0;
		}

		bool read_memory( std::uint32_t process_id, std::uint64_t address,
			void* buffer, std::size_t size )
		{
			if ( !buffer || !size ) return false;
			exception::data_request_t request {};
			request.m_type = static_cast< std::uint32_t >(
				exception::e_data_type::read_memory );
			request.m_process_id = process_id;
			request.m_address = address;
			request.m_buffer = reinterpret_cast< std::uint64_t >( buffer );
			request.m_size = size;
			return submit( request ) && request.m_result == size;
		}

		bool write_memory( std::uint32_t process_id, std::uint64_t address,
			const void* buffer, std::size_t size )
		{
			if ( !buffer || !size ) return false;
			exception::data_request_t request {};
			request.m_type = static_cast< std::uint32_t >(
				exception::e_data_type::write_memory );
			request.m_process_id = process_id;
			request.m_address = address;
			request.m_buffer = reinterpret_cast< std::uint64_t >( buffer );
			request.m_size = size;
			return submit( request ) && request.m_result == size;
		}

	private:
		bool submit( exception::data_request_t& request ) {
			request.m_magic = exception::k_magic;
			request.m_status = 0x7FFFFFFF;
			request.m_result = 0;
			exception_callout( &request );
			return !request.m_status;
		}

		bool is_running( ) {
			exception::data_request_t request {};
			request.m_type = static_cast< std::uint32_t >(
				exception::e_data_type::is_running );
			submit( request );
			return request.m_status == 0 &&
				request.m_result == exception::k_alive;
		}

		void benchmark( std::uint32_t process_id, std::uint64_t address ) {
		std::uint64_t read_buffer = 0;
		std::uint64_t ops = 0;
		std::uint64_t fails = 0;

		for ( auto i = 0; i < 1000; i++ )
			read_memory( process_id, address, &read_buffer, sizeof( read_buffer ) );

		const auto start = std::chrono::steady_clock::now( );
		const auto deadline = start + std::chrono::seconds( 1 );

		while ( std::chrono::steady_clock::now( ) < deadline ) {
			for ( auto i = 0; i < 1000; i++ ) {
				if ( read_memory( process_id, address, &read_buffer, sizeof( read_buffer ) ) )
					++ops;
				else
					++fails;
			}
		}

const auto elapsed = std::chrono::duration<double>(
	std::chrono::steady_clock::now( ) - start ).count( );
const auto rps = static_cast< std::uint64_t >( static_cast< double >( ops ) / elapsed );

logging::print( obf( "benchmark: %s reads/sec, %s fail" ),
	utility::add_commas( rps ).c_str( ), utility::add_commas( fails ).c_str( ) );
logging::print( obf( "benchmark: %.2fs elapsed, %s ok" ), elapsed,
	utility::add_commas( ops ).c_str( ) );
}

	};
}