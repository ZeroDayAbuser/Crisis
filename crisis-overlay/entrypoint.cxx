#include <deps/includes.h>
#include <DbgHelp.h>
#pragma comment( lib, "dbghelp.lib" )

namespace {
    // Linker-provided symbol whose address is this module's base. Set by the
    // loader script regardless of whether the module was loaded via the OS
    // loader or manual-mapped, so this is a reliable "who am I" pointer
    // even without a LDR_DATA_TABLE_ENTRY.
    extern "C" IMAGE_DOS_HEADER __ImageBase;

    // Cached RUNTIME_FUNCTION table so we can Rtl-Delete it at shutdown.
    PRUNTIME_FUNCTION g_registered_pdata = nullptr;

    // Register this manually-mapped module's .pdata so RtlLookupFunctionEntry
    // can find the RUNTIME_FUNCTION for any RIP inside our .text. Without
    // this, exceptions raised inside the overlay walk into a dispatcher that
    // can't unwind our frames and takes a __fastfail - no SEH, no VEH, no
    // WER, no dump.
    //
    // The manual-mapper's shellcode (crisis-loader/core/module/injector/stub.hxx)
    // already does this via data->m_seh_support before calling DllMain, but
    // duplicating it here is cheap insurance: if the stub ever regresses,
    // gets built with SEH disabled, or the pointer to ntdll's
    // RtlAddFunctionTable in the target session drifts, our own DllMain
    // still hooks the unwind path.
    //
    // RtlAddFunctionTable is idempotent enough that a second successful
    // call for the same table is a no-op / accepted duplicate - no harm
    // if the stub already registered.
    bool register_exception_tables( void* module_base ) {
        const auto* base = reinterpret_cast< const std::uint8_t* >( module_base );
        const auto* dos  = reinterpret_cast< const IMAGE_DOS_HEADER* >( base );
        if ( dos->e_magic != IMAGE_DOS_SIGNATURE ) {
            utility::print( "seh: module base has no MZ - manual map wiped headers early?" );
            return false;
        }

        const auto* nt = reinterpret_cast< const IMAGE_NT_HEADERS64* >(
            base + dos->e_lfanew );
        if ( nt->Signature != IMAGE_NT_SIGNATURE ) {
            utility::print( "seh: module has no PE signature" );
            return false;
        }

        const auto& pdata_dir =
            nt->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXCEPTION ];
        if ( !pdata_dir.Size || !pdata_dir.VirtualAddress ) {
            utility::print( "seh: module has no .pdata (rebuild with /SAFESEH or check linker)" );
            return false;
        }

        auto* rf = reinterpret_cast< PRUNTIME_FUNCTION >(
            const_cast< std::uint8_t* >( base + pdata_dir.VirtualAddress ) );
        const DWORD count = pdata_dir.Size / sizeof( RUNTIME_FUNCTION );
        if ( !count ) {
            utility::print( "seh: .pdata has zero entries" );
            return false;
        }

        if ( !::RtlAddFunctionTable( rf, count, reinterpret_cast< DWORD64 >( base ) ) ) {
            utility::print( "seh: RtlAddFunctionTable failed (already registered?)" );
            return false;
        }

        g_registered_pdata = rf;
        return true;
    }

    void unregister_exception_tables( ) {
        if ( g_registered_pdata ) {
            ::RtlDeleteFunctionTable( g_registered_pdata );
            g_registered_pdata = nullptr;
        }
    }

    // Fallback dump writer for crashes when WER isn't configured to write
    // local dumps for explorer.exe. Writes a MiniDump next to the overlay's
    // known-good file path.
    LONG WINAPI overlay_unhandled_exception_filter( EXCEPTION_POINTERS* ep ) {
        utility::print(
            "seh: unhandled exception 0x%08x at rip=0x%p, "
            "attempting minidump write",
            ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode  : 0u,
            ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr );

        // Location: %ProgramData%\crisis\overlay_crash_<pid>_<tick>.dmp
        char dir_path[ MAX_PATH ]  = "C:\\ProgramData\\crisis";
        char dump_path[ MAX_PATH ] = { };
        CreateDirectoryA( dir_path, nullptr );
        std::snprintf( dump_path, sizeof( dump_path ),
            "%s\\overlay_crash_%lu_%llu.dmp",
            dir_path,
            GetCurrentProcessId( ),
            static_cast< unsigned long long >( GetTickCount64( ) ) );

        const HANDLE file = CreateFileA(
            dump_path,
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr );
        if ( file == INVALID_HANDLE_VALUE ) {
            utility::print( "seh: could not open dump file '%s' (err %lu)",
                dump_path, GetLastError( ) );
            return EXCEPTION_CONTINUE_SEARCH;
        }

        MINIDUMP_EXCEPTION_INFORMATION mei { };
        mei.ThreadId          = GetCurrentThreadId( );
        mei.ExceptionPointers = ep;
        mei.ClientPointers    = FALSE;

        const auto type = static_cast< MINIDUMP_TYPE >(
            MiniDumpWithDataSegs        |
            MiniDumpWithHandleData      |
            MiniDumpWithUnloadedModules |
            MiniDumpWithFullMemoryInfo  |
            MiniDumpWithThreadInfo );

        const BOOL ok = MiniDumpWriteDump(
            GetCurrentProcess( ),
            GetCurrentProcessId( ),
            file,
            type,
            ep ? &mei : nullptr,
            nullptr,
            nullptr );

        CloseHandle( file );

        if ( ok )
            utility::print( "seh: wrote minidump to %s", dump_path );
        else
            utility::print( "seh: MiniDumpWriteDump failed err=0x%lx", GetLastError( ) );

        // Let WER / OS take it from here (still terminates the process).
        return EXCEPTION_CONTINUE_SEARCH;
    }

    void shutdown( ) {
        utility::print( "shutting down." );

        if ( g_communication ) {
            g_communication->shutdown( );
            g_communication->~c_communication( );
            VirtualFree( g_communication, 0, MEM_RELEASE );
            g_communication = nullptr;
        }

        if ( g_render ) {
            g_render->shutdown( );
            g_render->~c_render( );
            VirtualFree( g_render, 0, MEM_RELEASE );
            g_render = nullptr;
        }

        if ( g_window ) {
            g_window->detach( );
            g_window->~c_window( );
            VirtualFree( g_window, 0, MEM_RELEASE );
            g_window = nullptr;
        }
    }
}

__declspec( dllexport ) bool __stdcall DllMain(
    HMODULE module,
    std::uint32_t reason,
    void* context ) {
    if ( reason == DLL_PROCESS_ATTACH ) {
        DisableThreadLibraryCalls( module );

        // Register unwind tables FIRST so any exception during subsequent
        // setup dispatches through our .pdata and produces a real dump.
        // Prefer the loader-provided `module` handle over &__ImageBase since
        // manual-mappers sometimes hand back a base whose linker-embedded
        // ImageBase doesn't match our load address.
        register_exception_tables(
            module ? reinterpret_cast< void* >( module )
                   : reinterpret_cast< void* >( &__ImageBase ) );

        // Install a local unhandled-exception filter that writes a minidump
        // to %ProgramData%\crisis regardless of whether WER is configured
        // for explorer.exe. Chained - we don't stomp any prior filter, we
        // save and forward.
        SetUnhandledExceptionFilter( overlay_unhandled_exception_filter );

		auto window = VirtualAlloc(
			nullptr,
			sizeof( window::c_window ),
			MEM_COMMIT | MEM_RESERVE,
			PAGE_READWRITE
		);

		if ( !window )
			return 0;

		g_window = new ( window ) window::c_window { };
		if ( !g_window->attach( ) ) {
			utility::print( "could not attach to window." );
			shutdown( );
			return 0;
		}

		auto renderer = VirtualAlloc(
			nullptr,
			sizeof( render::c_render ),
			MEM_COMMIT | MEM_RESERVE,
			PAGE_READWRITE
		);

		if ( !renderer ) {
			shutdown( );
			return 0;
		}

		g_render = new ( renderer ) render::c_render { };
		if ( !g_render->setup( *g_window ) ) {
			utility::print( "could not setup renderer." );
			shutdown( );
			return 0;
		}

		auto communication = VirtualAlloc(
			nullptr,
			sizeof( communication::c_communication ),
			MEM_COMMIT | MEM_RESERVE,
			PAGE_READWRITE
		);

		if ( !communication ) {
			shutdown( );
			return 0;
		}

		g_communication = new ( communication ) communication::c_communication { };
		if ( !g_communication->setup( ) ) {
			utility::print( "could not setup communication." );
			shutdown( );
			return 0;
		}
    }

    if ( reason == DLL_PROCESS_DETACH ) {
        shutdown( );
        unregister_exception_tables( );
    }

    return true;
}
