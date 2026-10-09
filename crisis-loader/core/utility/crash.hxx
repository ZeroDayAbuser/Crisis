#pragma once
#include <consoleapi.h>

namespace crash {
    void terminate( ) {
		g_service->unload( );
		g_driver->unload( );
		g_overlay->close( );
		g_memory->close( );
	}

    long crash_handler( EXCEPTION_POINTERS* exception_pointers ) {
        const auto* context = exception_pointers->ContextRecord;
        char message[ 1024 ];
        sprintf( message,
            "Oops! Something went wrong!\n\n"
                "The service encountered an unexpected error and needs to close.\n\n"
                "Quick fixes to try:\n"
                "  � Restart the service\n"
                "  � Rollback recent updates to the service\n"
                "  � Check if your antivirus is interfering\n\n"
                "Still having trouble? We're here to help!\n"
                "Contact support through the tickets section.\n\n"
                "Crash Details:\n"
                "Build: %s %s\n"
                "Error: 0x%08X\n"
                "Fault: %p\n"
                "Registers: RIP=%016llX RSP=%016llX\n"
                "           RDI=%016llX RSI=%016llX\n"
                "           RBX=%016llX RDX=%016llX\n"
                "           RCX=%016llX RAX=%016llX\n"
                "           RBP=%016llX",
            __DATE__, __TIME__,
            exception_pointers->ExceptionRecord->ExceptionCode,
            exception_pointers->ExceptionRecord->ExceptionAddress,
            context->Rip, context->Rsp,
            context->Rdi, context->Rsi,
            context->Rbx, context->Rdx,
            context->Rcx, context->Rax,
            context->Rbp
        );
        terminate( );
        MessageBoxA( 0, message, "caught exception", MB_ICONERROR | MB_OK );
        return EXCEPTION_EXECUTE_HANDLER;
    }

    bool unload_handler( std::uint32_t signal ) {
        terminate( );
        return true;
    }

	void exit_handler( ) {
		g_service->unload( );
		g_driver->unload( );
		g_overlay->close( );
		g_memory->close( );
	}

	void signal_handler( int ) {
		g_service->unload( );
		g_driver->unload( );
		g_overlay->close( );
		g_memory->close( );
		ExitProcess( 0xcea );
	}
}