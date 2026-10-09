#pragma once
#include <chrono>
#include <ctime>
#include <cmath>
#include <vector>
#include <Windows.h>
#include <tlhelp32.h>
#include <winternl.h>
#define _NTDEF_
#include <ntsecapi.h>
#undef _NTDEF_
#include <cstdint>
#include <thread>
#include <functional>
#include <string>
#include <memory>
#include <array>
#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include <optional>
#include <algorithm>
#include <type_traits>
#include <mutex>
#include <fstream>
#include <intrin.h>
#include <psapi.h>

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "Synchronization.lib")

#include <deps/ia32/ia32.h>
#include <ext/obfuscation/obfuscation.h>

#include <core/utility/utility.hxx>
#include <deps/pdb/pdb.h>
#include <deps/data.h>

#include <core/mapping/transport.hxx>
#include <core/mapping/memory/command.h>
#include <core/mapping/memory/memory.hxx>
auto g_memory = std::make_shared<shared::c_shared>( );

#include <core/mapping/overlay/commands.h>
#include <core/mapping/overlay/overlay.hxx>
auto g_overlay = std::make_shared<overlay::c_overlay>( );

#include <core/mapping/mapping.hxx>
auto g_vtable_data = std::make_unique<mapping::c_mapping>( );

#include <core/memory/internals/paging.hxx>
auto g_paging = std::make_unique<paging::c_paging>( );

#include <core/memory/memory.hxx>
#include <core/memory/internals/syscall.hxx>
auto g_syscall = std::make_unique<syscall::c_syscall>( );
#include <core/memory/process/process.hxx>
#include <core/memory/process/allocator.hxx>
auto g_allocator = std::make_unique<process::c_allocator>( );

#include <core/memory/allocation.hxx>
auto g_kalloc = std::make_unique<allocation::c_allocator>( );

#include <core/memory/process/handle_scan.hxx>
#include <core/memory/process/caller/caller.hxx>
auto g_caller = std::make_unique<caller::c_caller>( );

#include <core/memory/protection/protection.hxx>
#include <routines.hxx>