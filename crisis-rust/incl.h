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
#include <array>
#include <unordered_map>
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
#include <optional>
#include <winhttp.h>
#include <urlmon.h>
#include <algorithm>
#include <wincodec.h>
#include <wrl/client.h>
#include <charconv>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <iterator>
#include <string_view>
#include <utility>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment( lib, "ws2_32.lib" )
#pragma comment( lib, "winhttp.lib" )
#pragma comment( lib, "windowscodecs.lib" )

namespace logging {
	template<typename... Args>
	static void print( const char* fmt, Args... args ) {
		char message[ 512 ] {};
		sprintf_s( message, fmt, args... );

		SYSTEMTIME time {};
		GetLocalTime( &time );

		char output[ 600 ] {};
		sprintf_s( output, "[%02d:%02d:%02d] [crisis-external] %s\n",
			time.wHour, time.wMinute, time.wSecond, message );
		OutputDebugStringA( output );
		std::fputs( output, stdout );
		std::fflush( stdout );
	}
}

#include <thirdparty/crisis/bypass.hpp>
#include <thirdparty/crisis/nexus.h>
