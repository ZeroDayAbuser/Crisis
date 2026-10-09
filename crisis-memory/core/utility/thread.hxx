#pragma once

namespace utility::thread {
	using f_NtCreateThreadEx = NTSTATUS( NTAPI* )(
		PHANDLE, ACCESS_MASK, PVOID, HANDLE,
		PVOID, PVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, PVOID );
	using f_NtQIT = NTSTATUS( NTAPI* )(
		HANDLE, ULONG, PVOID, ULONG, PULONG );
	using f_NtGetNextThread = NTSTATUS( NTAPI* )(
		HANDLE, HANDLE, ACCESS_MASK, ULONG, ULONG, PHANDLE );

	inline PVOID find_pool_worker_start( ) noexcept {
		const auto ntdll_base = reinterpret_cast< ULONG64 >(
			GetModuleHandleW( obf( L"ntdll.dll" ) ) );
		if ( !ntdll_base ) return nullptr;

		const auto dos = reinterpret_cast< IMAGE_DOS_HEADER* >( ntdll_base );
		const auto nt = reinterpret_cast< IMAGE_NT_HEADERS64* >(
			ntdll_base + dos->e_lfanew );
		const ULONG64 ntdll_end = ntdll_base + nt->OptionalHeader.SizeOfImage;

		const auto mod = reinterpret_cast< HMODULE >( ntdll_base );
		const auto NtQIT = reinterpret_cast< f_NtQIT >(
			GetProcAddress( mod, obf( "NtQueryInformationThread" ) ) );
		const auto NtGNT = reinterpret_cast< f_NtGetNextThread >(
			GetProcAddress( mod, obf( "NtGetNextThread" ) ) );
		if ( !NtQIT || !NtGNT ) return nullptr;

		PVOID  result = nullptr;
		HANDLE prev = nullptr;
		HANDLE cur = nullptr;

		while ( NtGNT( GetCurrentProcess( ), prev,
			THREAD_QUERY_INFORMATION, 0, 0, &cur ) >= 0 ) {
			if ( prev ) { CloseHandle( prev ); prev = nullptr; }

			PVOID sa = nullptr;
			if ( NtQIT( cur, 9, &sa, sizeof( sa ), nullptr ) >= 0 && sa ) {
				const auto addr = reinterpret_cast< ULONG64 >( sa );
				if ( addr > ntdll_base && addr < ntdll_end ) {
					result = sa;
					CloseHandle( cur );
					cur = nullptr;
					break;
				}
			}

			prev = cur;
			cur = nullptr;
		}

		if ( prev ) CloseHandle( prev );
		return result;
	}

	static HANDLE create(
		LPTHREAD_START_ROUTINE fn,
		LPVOID                 arg,
		DWORD                  flags = 0,
		LPDWORD                tid = nullptr ) noexcept
	{
		static const auto ntdll = reinterpret_cast< HMODULE >(
			GetModuleHandleW( obf( L"ntdll.dll" ) ) );
		static const auto NtCreateThEx = reinterpret_cast< f_NtCreateThreadEx >(
			ntdll ? GetProcAddress( ntdll, obf( "NtCreateThreadEx" ) ) : nullptr );

		static const PVOID fake_start = find_pool_worker_start( );
		if ( !NtCreateThEx || !fake_start )
			return CreateThread( nullptr, 0, fn, arg, flags, tid );

		constexpr ULONG k_suspended = 0x1;
		HANDLE h = nullptr;
		if ( NtCreateThEx( &h, THREAD_ALL_ACCESS, nullptr,
			GetCurrentProcess( ), fake_start, nullptr,
			k_suspended, 0, 0, 0, nullptr ) < 0 || !h )
			return CreateThread( nullptr, 0, fn, arg, flags, tid );

		CONTEXT ctx {};
		ctx.ContextFlags = CONTEXT_FULL;
		if ( !GetThreadContext( h, &ctx ) ) {
			TerminateThread( h, 0 ); CloseHandle( h ); return nullptr;
		}
		ctx.Rcx = reinterpret_cast< ULONG64 >( fn );
		ctx.Rdx = reinterpret_cast< ULONG64 >( arg );
		if ( !SetThreadContext( h, &ctx ) ) {
			TerminateThread( h, 0 ); CloseHandle( h ); return nullptr;
		}

		if ( !( flags & CREATE_SUSPENDED ) )
			ResumeThread( h );

		if ( tid ) *tid = GetThreadId( h );
		return h;
	}
}
