#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>
#include <ctime>
#include <vector>
#include <Windows.h>
#include <shellapi.h>
#include <timeapi.h>
#include <tlhelp32.h>
#include <fstream>
#include <winternl.h>
#ifndef PNTSTATUS
typedef NTSTATUS* PNTSTATUS;
#endif
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
#include <urlmon.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <wrl/client.h>
#include <unicorn/unicorn.h>
#include <unicorn/x86.h>
#include <Zydis/Zydis.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment( lib, "ws2_32.lib" )
#pragma comment( lib, "winmm.lib" )
#pragma comment( lib, "winhttp.lib" )
#pragma comment( lib, "windowscodecs.lib" )
namespace ch = std::chrono;

#include <ext/obfuscation/obfuscation.h>
#include <ext/bypass/bypass.h>
// [nexus removed] #include <ext/bypass/nexus.h> — Nexus API surface is gone.

#include <core/casino/client.hxx>
#include <core/profile/client.hxx>

#include <core/interface/overlay.h>
#include <core/utility/utility.hxx>

#include <core/engine/enums.h>
#include <core/engine/offsets.h>
#include <core/engine/structs.h>
#include <core/engine/emulators/string.hxx>
#include <core/engine/emulators/fname.hxx>
#include <core/engine/classes/classes.h>
#include <core/engine/classes/kismet.h>

#include <core/interface/config.h>
#include <core/utility/priority.h>

#include <core/cache/actors.h>
#include <core/cache/cache.hxx>
extern std::unique_ptr< cache::c_cache > g_cache;
#include <core/cache/game.hxx>

#include <core/stats/thread/cache.hxx>

#include <core/stats/types.h>
#include <core/stats/api.hxx>
#include <core/stats/stats.hxx>
extern std::unique_ptr< stats::c_stats > g_api;

#include <core/interface/menu/menu.hxx>
#include <core/system/system.hxx>
extern std::unique_ptr< xp_system::c_system > g_system;

#include <core/interface/aimbot/curve/curve.hxx>
#include <core/interface/aimbot/prediction.hxx>
#include <core/interface/aimbot/aimbot.hxx>
extern std::unique_ptr< aimbot::c_aimbot > g_aimbot;

#include <core/interface/visuals/esp/text.hxx>
#include <core/interface/visuals/esp/boxes.hxx>
#include <core/interface/visuals/esp/skeleton.hxx>
#include <core/interface/visuals/esp/movement.hxx>
#include <core/interface/visuals/esp/weapon.hxx>
#include <core/interface/visuals/esp/health.hxx>
extern std::unique_ptr< gui::esp::health::c_health_thread > g_health_thread;

#include <core/stats/thread/thread.hxx>
extern std::unique_ptr< player_stats::c_player_stats_thread > g_stats;

#include <core/interface/visuals/esp/inventory.hxx>
#include <core/interface/visuals/esp/tracers.hxx>
#include <core/interface/visuals/esp/threat_arrow.hxx>
#include <core/interface/visuals/player.hxx>
#include <core/interface/visuals/world.hxx>
#include <core/interface/widgets/widgets.hxx>
#include <core/interface/interface.hxx>