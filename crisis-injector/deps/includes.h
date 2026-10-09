#pragma once
#include <chrono>
#include <ctime>
#include <vector>
#include <Windows.h>
#include <tlhelp32.h>
#include <fstream>
#include <winternl.h>
#define _NTDEF_
#include <ntsecapi.h>
#undef _NTDEF_
#include <cstdint>
#include <DbgHelp.h>
#include <thread>
#include <functional>
#include <map>
#include <atlbase.h>
#include <string>
#include <memory>
#include <random>
#include <ntstatus.h>
#include <array>
#include <functional>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <numbers>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <complex>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <chrono>
#include <optional>
#include <winhttp.h>
#include <urlmon.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <wincodec.h>
#include <signal.h>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment( lib, "ws2_32.lib" )
#pragma comment( lib, "winhttp.lib" )
#pragma comment( lib, "windowscodecs.lib" )
namespace ch = std::chrono;

#include <deps/ia32/ia32.h>

#include <ext/obfuscation/obfuscation.h>
#include <ext/bypass/bypass.h>

#include <core/utility/utility.hxx>

#include <core/injector/image.hxx>
#include <core/injector/modules/modules.hxx>
#include <core/injector/injector.hxx>