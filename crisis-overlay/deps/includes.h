#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Windows.h>
#include <ext/obfuscation/obfuscation.h>
#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <d2d1helper.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <dwrite.h>
#include <dwrite_3.h>
#include <dxgi1_2.h>
#include <tlhelp32.h>
#include <wincodec.h>
#include <windowsx.h>
#include <winternl.h>
#include <wrl/client.h>

#include <core/utility/utility.hxx>
#include <core/utility/thread.hxx>
#include <core/render/window.hxx>
#include <core/shared/transport.hxx>
#include <core/shared/command.h>
inline window::c_window* g_window = nullptr;

#include <ext/render/types.hxx>
#include <ext/render/window.hxx>
#include <ext/render/device.hxx>
#include <ext/render/renderer.hxx>
#include <ext/render/scene3d.hxx>
#include <ext/render/overlay.hxx>

#include <core/render/render.hxx>
inline render::c_render* g_render = nullptr;

#include <core/shared/shared.hxx>
inline communication::c_communication* g_communication = nullptr;
