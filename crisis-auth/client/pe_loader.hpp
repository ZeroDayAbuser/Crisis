#pragma once

#include <windows.h>
#include <cstdint>
#include <vector>
#include <cstring>
#include <iostream>

namespace pe {

    static inline bool is_valid_pe( const std::uint8_t* data, size_t size ) {
        if ( size < sizeof( IMAGE_DOS_HEADER ) ) return false;
        auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>( data );
        if ( dos->e_magic != IMAGE_DOS_SIGNATURE ) return false;
        if ( size < dos->e_lfanew + sizeof( IMAGE_NT_HEADERS ) ) return false;
        auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>( data + dos->e_lfanew );
        if ( nt->Signature != IMAGE_NT_SIGNATURE ) return false;
        return true;
    }

    struct mapped_module_t {
        std::uint8_t* base = nullptr;
        HMODULE* import_modules = nullptr;
        size_t import_module_count = 0;
        bool ok = false;
    };

    static inline mapped_module_t manual_map( const std::vector<std::uint8_t>& raw_bytes ) {
        mapped_module_t result = { };

        if ( raw_bytes.empty( ) || !is_valid_pe( raw_bytes.data( ), raw_bytes.size( ) ) ) {
            std::cerr << "[PE] Invalid PE file\n";
            return result;
        }

        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>( raw_bytes.data( ) );
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>( raw_bytes.data( ) + dos->e_lfanew );
        const auto* optional = &nt->OptionalHeader;
        const auto* file_header = &nt->FileHeader;
        const auto* sections = IMAGE_FIRST_SECTION( nt );

        const size_t image_size = optional->SizeOfImage;
        const std::uintptr_t preferred_base = optional->ImageBase;

        std::uint8_t* allocated = reinterpret_cast<std::uint8_t*>(
            VirtualAlloc( nullptr, image_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) );
        if ( !allocated ) {
            std::cerr << "[PE] VirtualAlloc failed\n";
            return result;
        }

        result.base = allocated;

        std::memcpy( allocated, raw_bytes.data( ), optional->SizeOfHeaders );

        for ( WORD i = 0; i < file_header->NumberOfSections; ++i ) {
            const auto* sec = &sections[ i ];
            if ( sec->SizeOfRawData > 0 ) {
                std::uintptr_t dest = reinterpret_cast<std::uintptr_t>( allocated ) + sec->VirtualAddress;
                std::memcpy( reinterpret_cast<void*>( dest ), raw_bytes.data( ) + sec->PointerToRawData, sec->SizeOfRawData );
            }
        }

        auto* mapped_nt = reinterpret_cast<IMAGE_NT_HEADERS*>( allocated + dos->e_lfanew );
        mapped_nt->OptionalHeader.ImageBase = reinterpret_cast<std::uintptr_t>( allocated );

        const std::uintptr_t delta = reinterpret_cast<std::uintptr_t>( allocated ) - preferred_base;

        if ( delta != 0 && optional->DataDirectory[ IMAGE_DIRECTORY_ENTRY_BASERELOC ].Size > 0 ) {
            auto* reloc_dir = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                allocated + optional->DataDirectory[ IMAGE_DIRECTORY_ENTRY_BASERELOC ].VirtualAddress );

            while ( reloc_dir->VirtualAddress != 0 ) {
                std::uintptr_t page = reinterpret_cast<std::uintptr_t>( allocated ) + reloc_dir->VirtualAddress;
                WORD* entries = reinterpret_cast<WORD*>( reinterpret_cast<std::uint8_t*>( reloc_dir ) + sizeof( IMAGE_BASE_RELOCATION ) );
                DWORD count = ( reloc_dir->SizeOfBlock - sizeof( IMAGE_BASE_RELOCATION ) ) / sizeof( WORD );

                for ( DWORD i = 0; i < count; ++i ) {
                    WORD type = ( entries[ i ] >> 12 ) & 0xF;
                    WORD offset = entries[ i ] & 0xFFF;

                    if ( type == IMAGE_REL_BASED_DIR64 ) {
                        *reinterpret_cast<std::uint64_t*>( page + offset ) += delta;
                    }
                    else if ( type == IMAGE_REL_BASED_HIGHLOW ) {
                        *reinterpret_cast<std::uint32_t*>( page + offset ) += static_cast<std::uint32_t>( delta );
                    }
                    else if ( type == IMAGE_REL_BASED_HIGH ) {
                        *reinterpret_cast<std::uint16_t*>( page + offset ) += static_cast<std::uint16_t>( ( delta >> 16 ) & 0xFFFF );
                    }
                    else if ( type == IMAGE_REL_BASED_LOW ) {
                        *reinterpret_cast<std::uint16_t*>( page + offset ) += static_cast<std::uint16_t>( delta & 0xFFFF );
                    }
                }
                reloc_dir = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                    reinterpret_cast<std::uint8_t*>( reloc_dir ) + reloc_dir->SizeOfBlock );
            }
        }

        if ( optional->DataDirectory[ IMAGE_DIRECTORY_ENTRY_IMPORT ].Size > 0 ) {
            auto* import_desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
                allocated + optional->DataDirectory[ IMAGE_DIRECTORY_ENTRY_IMPORT ].VirtualAddress );

            std::vector<HMODULE> loaded_modules;

            while ( import_desc->Name != 0 ) {
                const char* mod_name = reinterpret_cast<const char*>( allocated + import_desc->Name );
                HMODULE h_mod = LoadLibraryA( mod_name );
                if ( !h_mod ) {
                    std::cerr << "[PE] Failed to load dependency: " << mod_name << "\n";
                }
                loaded_modules.push_back( h_mod );

                auto* thunk_ref = reinterpret_cast<std::uintptr_t*>( allocated + import_desc->OriginalFirstThunk );
                auto* func_ref = reinterpret_cast<std::uintptr_t*>( allocated + import_desc->FirstThunk );

                if ( !thunk_ref ) thunk_ref = func_ref;

                for ( ; *thunk_ref != 0; ++thunk_ref, ++func_ref ) {
                    if ( IMAGE_SNAP_BY_ORDINAL( *thunk_ref ) ) {
                        *func_ref = reinterpret_cast<std::uintptr_t>( GetProcAddress( h_mod, reinterpret_cast<LPCSTR>( IMAGE_ORDINAL( *thunk_ref ) ) ) );
                    }
                    else {
                        auto* import_by_name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>( allocated + *thunk_ref );
                        *func_ref = reinterpret_cast<std::uintptr_t>( GetProcAddress( h_mod, import_by_name->Name ) );
                    }
                }
                ++import_desc;
            }

            if ( !loaded_modules.empty( ) ) {
                result.import_modules = new HMODULE[ loaded_modules.size( ) ];
                result.import_module_count = loaded_modules.size( );
                std::memcpy( result.import_modules, loaded_modules.data( ), loaded_modules.size( ) * sizeof( HMODULE ) );
            }
        }

        for ( WORD i = 0; i < file_header->NumberOfSections; ++i ) {
            const auto* sec = &sections[ i ];
            if ( sec->SizeOfRawData == 0 && sec->Misc.VirtualSize == 0 ) continue;

            DWORD new_protect = PAGE_NOACCESS;
            if ( sec->Characteristics & IMAGE_SCN_MEM_EXECUTE ) {
                if ( sec->Characteristics & IMAGE_SCN_MEM_READ ) {
                    new_protect = ( sec->Characteristics & IMAGE_SCN_MEM_WRITE ) ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
                }
                else {
                    new_protect = PAGE_EXECUTE;
                }
            }
            else if ( sec->Characteristics & IMAGE_SCN_MEM_READ ) {
                new_protect = ( sec->Characteristics & IMAGE_SCN_MEM_WRITE ) ? PAGE_READWRITE : PAGE_READONLY;
            }
            else if ( sec->Characteristics & IMAGE_SCN_MEM_WRITE ) {
                new_protect = PAGE_READWRITE;
            }

            DWORD old_protect = 0;
            VirtualProtect( allocated + sec->VirtualAddress, sec->Misc.VirtualSize, new_protect, &old_protect );
        }

        if ( optional->DataDirectory[ IMAGE_DIRECTORY_ENTRY_TLS ].Size > 0 ) {
            auto* tls_dir = reinterpret_cast<IMAGE_TLS_DIRECTORY*>(
                allocated + optional->DataDirectory[ IMAGE_DIRECTORY_ENTRY_TLS ].VirtualAddress );
            auto* callbacks = reinterpret_cast<PIMAGE_TLS_CALLBACK*>( tls_dir->AddressOfCallBacks );
            if ( callbacks ) {
                while ( *callbacks ) {
                    ( *callbacks )( reinterpret_cast<void*>( allocated ), DLL_PROCESS_ATTACH, nullptr );
                    ++callbacks;
                }
            }
        }

        using dllmain_t = BOOL( WINAPI* )( HINSTANCE, DWORD, LPVOID );
        auto entry = reinterpret_cast<dllmain_t>( allocated + optional->AddressOfEntryPoint );
        if ( optional->AddressOfEntryPoint != 0 ) {
            entry( reinterpret_cast<HINSTANCE>( allocated ), DLL_PROCESS_ATTACH, nullptr );
        }

        result.ok = true;
        std::cout << "[PE] Module mapped at 0x" << std::hex << reinterpret_cast<std::uintptr_t>( allocated ) << std::dec << "\n";
        return result;
    }

    static inline void* get_export( mapped_module_t& mod, const char* export_name ) {
        if ( !mod.ok || !mod.base ) return nullptr;

        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>( mod.base );
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>( mod.base + dos->e_lfanew );
        auto* optional = &nt->OptionalHeader;

        if ( optional->DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT ].Size == 0 ) return nullptr;

        auto* export_dir = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(
            mod.base + optional->DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT ].VirtualAddress );

        auto* names = reinterpret_cast<DWORD*>( mod.base + export_dir->AddressOfNames );
        auto* funcs = reinterpret_cast<DWORD*>( mod.base + export_dir->AddressOfFunctions );
        auto* ords = reinterpret_cast<WORD*>( mod.base + export_dir->AddressOfNameOrdinals );

        for ( DWORD i = 0; i < export_dir->NumberOfNames; ++i ) {
            const char* name = reinterpret_cast<const char*>( mod.base + names[ i ] );
            if ( std::strcmp( name, export_name ) == 0 ) {
                return mod.base + funcs[ ords[ i ] ];
            }
        }
        return nullptr;
    }

    static inline void free_mapped( mapped_module_t& mod ) {
        if ( !mod.ok || !mod.base ) return;

        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>( mod.base );
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>( mod.base + dos->e_lfanew );
        auto* optional = &nt->OptionalHeader;
        if ( optional->AddressOfEntryPoint != 0 ) {
            using dllmain_t = BOOL( WINAPI* )( HINSTANCE, DWORD, LPVOID );
            auto entry = reinterpret_cast<dllmain_t>( mod.base + optional->AddressOfEntryPoint );
            entry( reinterpret_cast<HINSTANCE>( mod.base ), DLL_PROCESS_DETACH, nullptr );
        }

        if ( optional->DataDirectory[ IMAGE_DIRECTORY_ENTRY_TLS ].Size > 0 ) {
            auto* tls_dir = reinterpret_cast<IMAGE_TLS_DIRECTORY*>(
                mod.base + optional->DataDirectory[ IMAGE_DIRECTORY_ENTRY_TLS ].VirtualAddress );
            auto* callbacks = reinterpret_cast<PIMAGE_TLS_CALLBACK*>( tls_dir->AddressOfCallBacks );
            if ( callbacks ) {
                while ( *callbacks ) {
                    ( *callbacks )( reinterpret_cast<void*>( mod.base ), DLL_PROCESS_DETACH, nullptr );
                    ++callbacks;
                }
            }
        }

        VirtualFree( mod.base, 0, MEM_RELEASE );
        mod.base = nullptr;

        for ( size_t i = 0; i < mod.import_module_count; ++i ) {
            if ( mod.import_modules[ i ] ) FreeLibrary( mod.import_modules[ i ] );
        }
        delete[] mod.import_modules;
        mod.import_modules = nullptr;
        mod.import_module_count = 0;
        mod.ok = false;
    }

}
