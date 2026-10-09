#pragma once
#define development_environment true // enable to disable the authentication system

#include <chrono>
#include <ctime>
#include <cmath>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
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
#include <psapi.h>
#include <dia2.h>
#include <diacreate.h>
#include "../../crisis-bypass/deps/data.h"
#include <array>
#include <unordered_map>
#include <atomic>
#include <numbers>
#include <unordered_set>
#include <random>
#include <unordered_map>
#include <aclapi.h>
#include <winevt.h>
#include <shared_mutex>
#include <signal.h>
#include <shellapi.h>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "diaguids.lib")
#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "Synchronization.lib")
#pragma comment(lib, "wevtapi.lib")
namespace ch = std::chrono;

#include <deps/ia32/ia32.h>
#include <ext/obfuscation/obfuscation.h>
#include <core/utility/utility.hxx>
#include <core/utility/dbgmon.hxx>
#include <ext/backend/backend.hxx>

#include <deps/pdb/pdb.hxx>
auto g_pdb = std::make_shared<pdb::c_pdb>( obf( "ntoskrnl.exe" ) );

#include <core/module/shared/transport.hxx>
#include <core/module/module.h>
inline auto g_memory = std::make_shared< module::c_memory >( );
inline auto g_overlay = std::make_shared< module::c_overlay >( );

#include <ext/service/driver/driver.hxx>
auto g_driver = std::make_shared<driver::c_driver>( );

#include <ext/service/bytes.h>
#include <ext/service/service.hxx>
#include <ext/service/startup.hxx>
auto g_service = std::make_shared<service::c_service>( );

#include <core/memory/kernel/paging.hxx>
auto g_paging = std::make_unique< paging::c_paging >( );

#include <core/memory/memory.hxx>
#include <core/communication/request.h>
#include <core/communication/communication.hxx>
#include <core/memory/kernel/syscall.hxx>
auto g_syscall = std::make_unique< syscall::c_syscall >( );

#include <core/memory/kernel/kernel.hxx>
#include <core/memory/kernel/cow.hxx>
auto g_cow = std::make_unique< cow::c_cow >( );

#include <core/memory/kernel/process.hxx>
#include <core/module/vad.hxx>

#include <core/bypass/bypass.hxx>
auto g_bypass = std::make_shared<bypass::c_bypass>( );

#include <ext/service/cache/cache.hxx>

#include <core/module/injector/stub.hxx>
#include <core/module/injector/injector.hxx>
#include <core/module/shared/shared.hxx>

#include <core/memory/mapper/spoofer.h>
#include <core/memory/mapper/mapper.hxx>

#include <core/utility/traces/events.hxx>
#include <core/utility/traces/journal.hxx>
auto g_usn_journal = std::make_unique< usn::c_journal >( );

#include <core/utility/traces/prefetch.hxx>
auto g_prefetch = std::make_unique< prefetch::c_prefetch >( );

#include <core/memory/mapping/mapping.hxx>
auto g_mapping = std::make_unique< mapping::c_kernel_mapping >( );

#include <core/memory/streamproof/streamproof.hxx>
auto g_streamproof = std::make_unique< streamproof::c_streamproof >( );

#include <core/utility/crash.hxx>
#include <core/auth/games.hxx>

struct rgb_t {
	int r;
	int g;
	int b;
};

static void enable_virtual_terminal( ) {
	HANDLE handle = GetStdHandle( STD_OUTPUT_HANDLE );

	DWORD mode = 0;

	if ( GetConsoleMode( handle, &mode ) )
		SetConsoleMode( handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING );
}

static int clamp_color( int value ) {
	if ( value < 0 )
		return 0;

	if ( value > 255 )
		return 255;

	return value;
}

static rgb_t get_logo_color(
	float x,
	float y,
	float width,
	float height
) {
	float nx = x / width;
	float ny = y / height;

	/*
		Tuned toward the original logo:
		upper-left  -> bright magenta
		upper-right -> violet
		lower-right -> deeper blue
	*/

	float r = 220.0f
		- ( nx * 145.0f )
		- ( ny * 30.0f );

	float g = 65.0f
		- ( nx * 25.0f )
		- ( ny * 10.0f );

	float b = 251.0f
		- ( nx * 10.0f )
		- ( ny * 5.0f );

	return {
		clamp_color( static_cast< int >( r ) ),
		clamp_color( static_cast< int >( g ) ),
		clamp_color( static_cast< int >( b ) )
	};
}

static void set_rgb( const rgb_t& color ) {
	std::cout
		<< "\x1b[38;2;"
		<< color.r << ';'
		<< color.g << ';'
		<< color.b << 'm';
}

static void print_logo( ) {
	static const std::vector<std::string> logo = {
		R"(             ++++++++++++*******************#      )",
		R"(            +++++++++++++******************        )",
		R"(           +++++++++++++******************         )",
		R"(          +++++++++++++******************          )",
		R"(         +++++++++++++*****************#           )",
		R"(        *************+*****************            )",
		R"(       ++++*********                               )",
		R"(      +++++++++++++                                )",
		R"(     +++++++++++++                                 )",
		R"(    +++++++++++++                                  )",
		R"(                                                   )",
		R"(    ++++++++++++*                                  )",
		R"(     ++++++++++***                                 )",
		R"(      ++++++++*****                                )",
		R"(       ++***********                               )",
		R"(        ***********#********##########             )",
		R"(         +++*****************###########           )",
		R"(          +*****************#############          )",
		R"(           *****************##############         )",
		R"(            ***************################        )",
		R"(             *************#################%%      )",
		R"(                                                   )"
	};

	std::size_t width = 0;

	for ( const auto& line : logo ) {
		if ( line.size( ) > width )
			width = line.size( );
	}

	const float height = static_cast< float >( logo.size( ) );

	for ( std::size_t y = 0; y < logo.size( ); ++y ) {
		const auto& line = logo[ y ];

		for ( std::size_t x = 0; x < line.size( ); ++x ) {
			char ch = line[ x ];

			if ( ch == ' ' ) {
				std::cout << ' ';
				continue;
			}

			auto color = get_logo_color(
				static_cast< float >( x ),
				static_cast< float >( y ),
				static_cast< float >( width ),
				height
			);

			set_rgb( color );

			std::cout << ch;
		}

		std::cout << "\x1b[0m\n";
	}

	std::cout << "\x1b[0m";
}
