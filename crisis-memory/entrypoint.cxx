#include <deps/includes.h>

namespace {
    void shutdown( ) {
        if ( g_communication ) {
            g_communication->shutdown( );
            g_communication->~c_communication( );
            VirtualFree( g_communication, 0, MEM_RELEASE );
            g_communication = nullptr;
        }

        if ( g_memory ) {
            g_memory->~c_mapped_memory( );
            VirtualFree( g_memory, 0, MEM_RELEASE );
            g_memory = nullptr;
        }
    }
}

__declspec( dllexport ) bool __stdcall DllMain(
    HMODULE module,
    std::uint32_t reason,
    void* context ) {
    if ( reason == DLL_PROCESS_ATTACH ) {
        DisableThreadLibraryCalls( module );

        auto memory = VirtualAlloc(
            nullptr,
            sizeof( memory::c_mapped_memory ),
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE
        );

        if ( !memory )
            return false;

        g_memory = new ( memory ) memory::c_mapped_memory{ };

        auto communication = VirtualAlloc(
            nullptr,
            sizeof( communication::c_communication ),
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE
        );

        if ( !communication ) {
            utility::print( "failed to allocate communication object" );
            return false;
        }

        g_communication = new ( communication ) communication::c_communication{ };

        int regs[ 4 ]{};
        __cpuid( regs, 0x80000000 );
        const auto max_ext = static_cast< unsigned >( regs[ 0 ] );
        auto phys_bits = 36u;
        if ( max_ext >= 0x80000008 ) {
            __cpuid( regs, 0x80000008 );
            const auto b = static_cast< unsigned >( regs[ 0 ] & 0xFF );
            if ( b >= 32 && b <= 52 )
                phys_bits = b;
        }

        g_memory->setup( ( phys_bits >= 63 )
            ? ~0ULL
            : ( ( 1ULL << phys_bits ) - 1ULL ) );

        return g_communication->setup( );
    }

    if ( reason == DLL_PROCESS_DETACH ) {
        shutdown( );
    }

    return true;
}
