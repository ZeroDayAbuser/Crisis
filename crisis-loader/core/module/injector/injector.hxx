#pragma once

namespace module {
	c_module::c_module( const std::string& image_path, std::uint32_t process_id )
		: m_process_id( process_id ) {
		load_image( image_path );
	}

	c_module::c_module( std::vector<std::uint8_t> image, std::uint32_t process_id )
		: m_process_id( process_id ), m_image( std::move( image ) ) {
		initialize_image( );
	}

	c_module::~c_module( ) {
		(void)discard_image( );
	}

	bool c_module::discard_image( ) {
		m_nt_headers = nullptr;
		if ( !m_image.empty( ) ) SecureZeroMemory( m_image.data( ), m_image.size( ) );
		std::vector< std::uint8_t >( ).swap( m_image );
		return m_image.empty( ) && m_nt_headers == nullptr;
	}

	bool c_module::is_dll( ) const {
		if ( !m_nt_headers )
			return false;
		return ( m_nt_headers->FileHeader.Characteristics & IMAGE_FILE_DLL ) != 0;
	}

	bool c_module::inject( LPVOID reserved ) {
		auto process_handle = OpenProcess( PROCESS_ALL_ACCESS, FALSE, m_process_id );
		if ( !process_handle ) {
			logging::print( obf( "OpenProcess failed: 0x%x" ), GetLastError( ) );
			return false;
		}

		logging::print( obf( "opened process: 0x%llx" ), process_handle );

		const auto mapped = load_stub(
			process_handle,
			m_image.data( ),
			m_image.size( ),
			true,
			true,
			true,
			true,
			DLL_PROCESS_ATTACH,
			reserved );
		const auto discarded = discard_image( );
		if ( !mapped || !discarded ) {
			CloseHandle( process_handle );
			logging::print( obf( "manual mapper failed" ) );
			return false;
		}

		CloseHandle( process_handle );
		return true;
	}

	bool c_module::inject( const void* parameter, std::size_t parameter_size ) {
		if ( !parameter || !parameter_size )
			return false;

		auto process_handle = OpenProcess( PROCESS_ALL_ACCESS, FALSE, m_process_id );
		if ( !process_handle ) {
			logging::print( obf( "OpenProcess failed: 0x%x" ), GetLastError( ) );
			return false;
		}

		auto remote_parameter = VirtualAllocEx(
			process_handle, nullptr, parameter_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
		if ( !remote_parameter || !WriteProcessMemory(
			process_handle, remote_parameter, parameter, parameter_size, nullptr ) ) {
			if ( remote_parameter )
				VirtualFreeEx( process_handle, remote_parameter, 0, MEM_RELEASE );
			CloseHandle( process_handle );
			return false;
		}

		const auto mapped = load_stub(
			process_handle, m_image.data( ), m_image.size( ), true, true, true, true,
			DLL_PROCESS_ATTACH, remote_parameter );
		const auto discarded = discard_image( );
		VirtualFreeEx( process_handle, remote_parameter, 0, MEM_RELEASE );
		CloseHandle( process_handle );
		return mapped && discarded;
	}

	bool c_module::cleanup( ) {
		HANDLE process_handle = OpenProcess(
			PROCESS_VM_OPERATION | PROCESS_VM_WRITE,
			FALSE,
			m_process_id );

		if ( m_mapped_base ) {
			DWORD old_protect = 0;
			VirtualProtectEx(
				process_handle,
				reinterpret_cast< void* >( m_mapped_base ),
				0x1000,
				PAGE_READWRITE,
				&old_protect );

			std::array<std::uint8_t, 0x1000> empty_header { };
			if ( !WriteProcessMemory(
				process_handle,
				reinterpret_cast< void* >( m_mapped_base ),
				empty_header.data( ),
				empty_header.size( ),
				nullptr ) ) {
				logging::print( obf( "failed to erase pe header at 0x%llx (pid: %d): 0x%x" ),
					m_mapped_base, m_process_id, GetLastError( ) );
			}

			DWORD unused = 0;
			if ( old_protect ) {
				VirtualProtectEx(
					process_handle,
					reinterpret_cast< void* >( m_mapped_base ),
					0x1000,
					old_protect,
					&unused );
			}
		}

		if ( process_handle )
			CloseHandle( process_handle );

		// VAD replacement no longer needed — NtMapViewOfSection produces a naturally
		// MEM_MAPPED VAD (VadMapped type 0, real ControlArea/Subsection chain).

		logging::print( obf( "injection complete!\n" ) );
		return true;
	}

	bool c_module::unload( ) {
		if ( !m_mapped_base )
			return true;

		// The DLL lives in a section-backed view — VirtualFreeEx(MEM_RELEASE) would
		// fail with ERROR_INVALID_PARAMETER on a mapped view.  Use NtUnmapViewOfSection.
		using NtUnmapViewOfSection_t = NTSTATUS( NTAPI* )( HANDLE, PVOID );
		const auto NtUnmapViewOfSection = reinterpret_cast< NtUnmapViewOfSection_t >(
			GetProcAddress( GetModuleHandleA( obf( "ntdll.dll" ) ), obf( "NtUnmapViewOfSection" ) ) );

		HANDLE process_handle = OpenProcess( PROCESS_VM_OPERATION, FALSE, m_process_id );
		if ( !process_handle )
			return false;

		bool result = false;
		if ( NtUnmapViewOfSection ) {
			const auto status = NtUnmapViewOfSection(
				process_handle, reinterpret_cast< void* >( m_mapped_base ) );
			result = ( status >= 0 );
		}

		CloseHandle( process_handle );

		if ( result )
			m_mapped_base = 0;

		return result;
	}

	std::optional<std::uint64_t> c_module::get_base( ) const {
		return m_mapped_base
			? std::optional<std::uint64_t>{ m_mapped_base }
		: std::nullopt;
	}

	bool c_module::load_stub(
		HANDLE  process_handle,
		BYTE* image_data,
		SIZE_T  file_size,
		bool    clear_header,
		bool    clear_non_needed_sections,
		bool    adjust_protections,
		bool    seh_exception_support,
		DWORD   reason,
		LPVOID  reserved ) {
		if ( !process_handle || !image_data || file_size < sizeof( IMAGE_DOS_HEADER ) )
			return false;

		auto dos_header = reinterpret_cast< IMAGE_DOS_HEADER* >( image_data );
		if ( dos_header->e_magic != IMAGE_DOS_SIGNATURE ) {
			logging::print( obf( "invalid dos signature" ) );
			return false;
		}

		if ( dos_header->e_lfanew <= 0 ||
			static_cast< SIZE_T >( dos_header->e_lfanew ) + sizeof( IMAGE_NT_HEADERS ) > file_size ) {
			logging::print( obf( "invalid nt signature" ) );
			return false;
		}

		auto nt_header = reinterpret_cast< IMAGE_NT_HEADERS* >( image_data + dos_header->e_lfanew );
		auto file_header = &nt_header->FileHeader;
		auto opt_header = &nt_header->OptionalHeader;

#ifdef _WIN64
		constexpr auto current_arch = IMAGE_FILE_MACHINE_AMD64;
#else
		constexpr auto current_arch = IMAGE_FILE_MACHINE_I386;
#endif

		if ( file_header->Machine != current_arch ) {
			logging::print( obf( "image architecture mismatch (0x%x != 0x%x)" ),
				current_arch, file_header->Machine );
			return false;
		}

		// ── NtCreateSection / NtMapViewOfSection ─────────────────────────────────────
		// Allocating via VirtualAllocEx creates a MEM_PRIVATE VAD, which anti-cheat
		// scanners flag.  Instead, create an anonymous pagefile-backed section and map
		// a view into the target process: the resulting VAD is naturally VadMapped
		// (type 0, PrivateMemory=0) and NtQueryVirtualMemory returns MEM_MAPPED with a
		// real ControlArea/Subsection chain — no VAD patching needed at all.
		//
		// Two views are created:
		//   local_view  (current process, PAGE_READWRITE)  — written via memcpy
		//   target_base (target process,  PAGE_EXECUTE_READWRITE) — shellcode runs here
		//
		// Both views share the same physical pages (anonymous section, no CoW), so
		// memcpy to local_view appears immediately in target_base, and shellcode writes
		// (relocations, import patching) are visible in local_view.  We memset the
		// header / unwanted sections via local_view, then unmap it.  target_base
		// remains as the DLL's permanent MEM_MAPPED home.

		using NtCreateSection_t = NTSTATUS( NTAPI* )( PHANDLE, ACCESS_MASK,
			POBJECT_ATTRIBUTES, PLARGE_INTEGER, ULONG, ULONG, HANDLE );
		using NtMapViewOfSection_t = NTSTATUS( NTAPI* )( HANDLE, HANDLE, PVOID*,
			ULONG_PTR, SIZE_T, PLARGE_INTEGER, PSIZE_T, DWORD, ULONG, ULONG );
		using NtUnmapViewOfSection_t = NTSTATUS( NTAPI* )( HANDLE, PVOID );

		const auto ntdll = GetModuleHandleA( obf( "ntdll.dll" ) );
		const auto NtCreateSection = reinterpret_cast< NtCreateSection_t >(
			GetProcAddress( ntdll, obf( "NtCreateSection" ) ) );
		const auto NtMapViewOfSection = reinterpret_cast< NtMapViewOfSection_t >(
			GetProcAddress( ntdll, obf( "NtMapViewOfSection" ) ) );
		const auto NtUnmapViewOfSection = reinterpret_cast< NtUnmapViewOfSection_t >(
			GetProcAddress( ntdll, obf( "NtUnmapViewOfSection" ) ) );

		if ( !NtCreateSection || !NtMapViewOfSection || !NtUnmapViewOfSection ) {
			logging::print( obf( "failed to resolve NT section functions" ) );
			return false;
		}

		LARGE_INTEGER section_size { };
		section_size.QuadPart = static_cast< LONGLONG >( opt_header->SizeOfImage );

		HANDLE h_section = nullptr;
		NTSTATUS status = NtCreateSection(
			&h_section, SECTION_ALL_ACCESS, nullptr,
			&section_size, PAGE_EXECUTE_READWRITE, SEC_COMMIT, nullptr );
		if ( status < 0 || !h_section ) {
			logging::print( obf( "NtCreateSection failed: 0x%x" ), status );
			return false;
		}

		// local read-write view — we write image data here; shared pages appear in target
		PVOID  local_view = nullptr;
		SIZE_T local_size = 0;
		status = NtMapViewOfSection( h_section, GetCurrentProcess( ), &local_view,
			0, 0, nullptr, &local_size, 2 /* ViewUnmap */, 0, PAGE_READWRITE );
		if ( status < 0 || !local_view ) {
			logging::print( obf( "NtMapViewOfSection (local) failed: 0x%x" ), status );
			CloseHandle( h_section );
			return false;
		}

		// remote execute-readwrite view — shellcode applies relocations/imports here
		PVOID  target_base_ptr = nullptr;
		SIZE_T remote_size = 0;
		status = NtMapViewOfSection( h_section, process_handle, &target_base_ptr,
			0, 0, nullptr, &remote_size, 2 /* ViewUnmap */, 0, PAGE_EXECUTE_READWRITE );
		if ( status < 0 || !target_base_ptr ) {
			logging::print( obf( "NtMapViewOfSection (remote) failed: 0x%x" ), status );
			NtUnmapViewOfSection( GetCurrentProcess( ), local_view );
			CloseHandle( h_section );
			return false;
		}

		// section handle no longer needed — both views hold internal references
		CloseHandle( h_section );

		auto target_base = reinterpret_cast< BYTE* >( target_base_ptr );
		auto local_base = reinterpret_cast< BYTE* >( local_view );

		BYTE* mapping_data_alloc = nullptr;
		void* shellcode_alloc = nullptr;

		const auto cleanup_on_failure = [ & ] {
			if ( shellcode_alloc )
				VirtualFreeEx( process_handle, shellcode_alloc, 0, MEM_RELEASE );
			if ( mapping_data_alloc )
				VirtualFreeEx( process_handle, mapping_data_alloc, 0, MEM_RELEASE );
			NtUnmapViewOfSection( GetCurrentProcess( ), local_view );
			NtUnmapViewOfSection( process_handle, target_base );
			};

		const auto fail_with_error = [ & ]( const char* message ) -> bool {
			logging::print( message, GetLastError( ) );
			cleanup_on_failure( );
			return false;
			};

		// Server-mapped payloads are exactly SizeOfImage and already VA-laid-out.
		// On-disk PEs are usually smaller and need PointerToRawData -> VA copies.
		const auto image_size = static_cast< SIZE_T >( opt_header->SizeOfImage );
		const bool pre_mapped = file_size == image_size;
		auto section_header = IMAGE_FIRST_SECTION( nt_header );
		if ( pre_mapped ) {
			std::memcpy( local_base, image_data, image_size );
		}
		else {
			const auto header_size = file_size < 0x1000 ? file_size : static_cast< SIZE_T >( 0x1000 );
			std::memcpy( local_base, image_data, header_size );

			for ( UINT idx = 0; idx != file_header->NumberOfSections; ++idx, ++section_header ) {
				if ( !section_header->SizeOfRawData )
					continue;

				const auto raw_end =
					static_cast< SIZE_T >( section_header->PointerToRawData ) +
					section_header->SizeOfRawData;
				if ( raw_end > file_size ) {
					logging::print( obf( "section %d raw data out of bounds" ), idx );
					cleanup_on_failure( );
					return false;
				}

				std::memcpy(
					local_base + section_header->VirtualAddress,
					image_data + section_header->PointerToRawData,
					section_header->SizeOfRawData );
			}
		}

		manual_mapping_data_t mapping_data { };
		mapping_data.m_load_library_a = LoadLibraryA;
		mapping_data.m_get_proc_address = GetProcAddress;
#ifdef _WIN64
		mapping_data.m_rtl_add_function_table = RtlAddFunctionTable;
#else
		seh_exception_support = false;
#endif
		mapping_data.m_base = target_base;
		mapping_data.m_reason = reason;
		mapping_data.m_reserved = reserved;
		mapping_data.m_seh_support = seh_exception_support;

		mapping_data_alloc = reinterpret_cast< BYTE* >( VirtualAllocEx(
			process_handle, nullptr, sizeof( manual_mapping_data_t ),
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) );
		if ( !mapping_data_alloc )
			return fail_with_error( obf( "failed to allocate mapping data: 0x%x" ) );

		if ( !WriteProcessMemory(
			process_handle, mapping_data_alloc,
			&mapping_data, sizeof( manual_mapping_data_t ), nullptr ) )
			return fail_with_error( obf( "failed to write mapping data: 0x%x" ) );

		shellcode_alloc = VirtualAllocEx(
			process_handle, nullptr, 0x1000,
			MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
		if ( !shellcode_alloc )
			return fail_with_error( obf( "failed to allocate shellcode: 0x%x" ) );

		if ( !WriteProcessMemory(
			process_handle, shellcode_alloc,
			manual_mapping_shellcode, 0x1000, nullptr ) )
			return fail_with_error( obf( "failed to write shellcode: 0x%x" ) );

		auto thread_handle = CreateRemoteThread(
			process_handle, nullptr, 0,
			reinterpret_cast< LPTHREAD_START_ROUTINE >( shellcode_alloc ),
			mapping_data_alloc, 0, nullptr );
		if ( !thread_handle )
			return fail_with_error( obf( "failed to create remote thread: 0x%x" ) );

		// Keep thread_handle open — closing it immediately (as before) made early
		// thread death completely undetectable, causing the watchdog to spin the
		// full timeout before giving up instead of failing fast.

		HINSTANCE mapped_module = nullptr;
		const auto watchdog_start = std::chrono::steady_clock::now( );
		while ( !mapped_module ) {
			// Fast-path: remote thread has already exited.  Do one final read of
			// the mapping data and decide success/failure immediately rather than
			// waiting for the watchdog.
			if ( WaitForSingleObject( thread_handle, 0 ) == WAIT_OBJECT_0 ) {
				manual_mapping_data_t final_data { };
				ReadProcessMemory( process_handle, mapping_data_alloc,
					&final_data, sizeof( final_data ), nullptr );
				CloseHandle( thread_handle );
				thread_handle = nullptr;
				if ( !final_data.m_module ) {
					logging::print( obf( "remote thread exited before setting module handle" ) );
					cleanup_on_failure( );
					return false;
				}
				mapped_module = final_data.m_module;
				break;
			}

			// Secondary: target process died (crash / external termination).
			DWORD exit_code = 0;
			GetExitCodeProcess( process_handle, &exit_code );
			if ( exit_code != STILL_ACTIVE ) {
				logging::print( obf( "target process exited during injection" ), m_process_id );
				CloseHandle( thread_handle );
				cleanup_on_failure( );
				return false;
			}

			manual_mapping_data_t checked_data { };
			if ( !ReadProcessMemory(
				process_handle, mapping_data_alloc,
				&checked_data, sizeof( checked_data ), nullptr ) ) {
				CloseHandle( thread_handle );
				return fail_with_error( obf( "failed to read mapping data: 0x%x" ) );
			}

			mapped_module = checked_data.m_module;
			if ( mapped_module == reinterpret_cast< HINSTANCE >( 0x505050 ) ) {
				logging::print( obf( "mapped seh support unavailable - aborting injection" ) );
				if ( thread_handle ) { CloseHandle( thread_handle ); thread_handle = nullptr; }
				cleanup_on_failure( );
				return false;
			}

			if ( std::chrono::steady_clock::now( ) - watchdog_start > std::chrono::seconds( 60 ) ) {
				logging::print( obf( "remote mapper timed out (60s watchdog)" ) );
				CloseHandle( thread_handle );
				cleanup_on_failure( );
				return false;
			}

			Sleep( 10 );
		}

		if ( thread_handle ) { CloseHandle( thread_handle ); thread_handle = nullptr; }

		logging::print( obf( "mapped image: 0x%x" ), opt_header->SizeOfImage );

		// zero PE header and unwanted sections via local view.
		// Because both views share the same physical pages (no CoW for anonymous sections),
		// these writes are immediately visible in the target process without WriteProcessMemory.
		if ( clear_header )
			std::memset( local_base, 0, 0x1000 );

		if ( clear_non_needed_sections ) {
			section_header = IMAGE_FIRST_SECTION( nt_header );
			for ( UINT idx = 0; idx != file_header->NumberOfSections; ++idx, ++section_header ) {
				if ( !section_header->Misc.VirtualSize )
					continue;

				const auto section_name = reinterpret_cast< char* >( section_header->Name );
				const auto clear_section =
					( !seh_exception_support && strcmp( section_name, obf( ".pdata" ) ) == 0 ) ||
					strcmp( section_name, obf( ".rsrc" ) ) == 0 ||
					strcmp( section_name, obf( ".reloc" ) ) == 0;

				if ( clear_section )
					std::memset( local_base + section_header->VirtualAddress,
						0, section_header->Misc.VirtualSize );
			}
		}

		// per-section page protections on the remote view (per-view, does not affect local_view)
		if ( adjust_protections ) {
			section_header = IMAGE_FIRST_SECTION( nt_header );
			for ( UINT idx = 0; idx != file_header->NumberOfSections; ++idx, ++section_header ) {
				if ( !section_header->Misc.VirtualSize )
					continue;

				DWORD new_protect = PAGE_READONLY;
				if ( section_header->Characteristics & IMAGE_SCN_MEM_WRITE )
					new_protect = PAGE_READWRITE;
				else if ( section_header->Characteristics & IMAGE_SCN_MEM_EXECUTE )
					new_protect = PAGE_EXECUTE_READ;

				DWORD old = 0;
				VirtualProtectEx(
					process_handle,
					target_base + section_header->VirtualAddress,
					section_header->Misc.VirtualSize,
					new_protect, &old );
			}

			DWORD old = 0;
			VirtualProtectEx(
				process_handle,
				target_base,
				IMAGE_FIRST_SECTION( nt_header )->VirtualAddress,
				PAGE_READONLY, &old );
		}

		// wipe and release the temporary shellcode/data allocations
		std::array< BYTE, 0x1000 > zero_page { };
		WriteProcessMemory( process_handle, shellcode_alloc, zero_page.data( ), 0x1000, nullptr );
		VirtualFreeEx( process_handle, shellcode_alloc, 0, MEM_RELEASE );
		VirtualFreeEx( process_handle, mapping_data_alloc, 0, MEM_RELEASE );

		// unmap our local staging view — the remote view is the DLL's permanent MEM_MAPPED home
		NtUnmapViewOfSection( GetCurrentProcess( ), local_view );

		m_mapped_base = reinterpret_cast< std::uint64_t >( target_base );
		return true;
	}

	bool c_module::load_image( const std::string& image_path ) {
		std::ifstream file( image_path, std::ios::binary | std::ios::ate );
		if ( !file ) {
			logging::print( obf( "failed to open image file: %s" ), image_path.c_str( ) );
			return false;
		}

		const auto size = file.tellg( );
		if ( size <= 0 ) {
			logging::print( obf( "image file is empty: %s" ), image_path.c_str( ) );
			return false;
		}

		m_image.resize( static_cast< std::size_t >( size ) );
		file.seekg( 0, std::ios::beg );
		if ( !file.read( reinterpret_cast< char* >( m_image.data( ) ), size ) ) {
			logging::print( obf( "failed to read image file: %s" ), image_path.c_str( ) );
			m_image.clear( );
			return false;
		}

		logging::print( obf( "loaded image: %s (%llu bytes)" ),
			image_path.c_str( ), m_image.size( ) );
		return initialize_image( );
	}

	bool c_module::initialize_image( ) {
		if ( m_image.size( ) < sizeof( IMAGE_DOS_HEADER ) ) {
			logging::print( obf( "invalid image buffer (size: %llu)" ), m_image.size( ) );
			return false;
		}

		auto dos_header = reinterpret_cast< IMAGE_DOS_HEADER* >( m_image.data( ) );
		if ( dos_header->e_magic != IMAGE_DOS_SIGNATURE ) {
			logging::print( obf( "invalid dos signature" ) );
			return false;
		}

		const auto nt_offset = static_cast< std::size_t >( dos_header->e_lfanew );
		if ( nt_offset + sizeof( IMAGE_NT_HEADERS ) > m_image.size( ) ) {
			logging::print( obf( "invalid nt header offset: 0x%llx" ), nt_offset );
			return false;
		}

		m_nt_headers = reinterpret_cast< IMAGE_NT_HEADERS* >( m_image.data( ) + nt_offset );
		if ( m_nt_headers->Signature != IMAGE_NT_SIGNATURE ) {
			logging::print( obf( "invalid nt signature" ) );
			m_nt_headers = nullptr;
			return false;
		}

		return true;
	}
}