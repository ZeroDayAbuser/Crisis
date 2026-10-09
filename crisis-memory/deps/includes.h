#pragma once
#include <stdio.h>
#include <string>
#include <vector>
#include <Windows.h>
#include <ext/obfuscation/obfuscation.h>
#include <atomic>
#include <memory>
#include <numbers>
#include <new>
#include <dwrite_3.h>
#pragma comment(lib, "Synchronization.lib")

#include <core\utility\utility.hxx>
#include <core\utility\thread.hxx>
#include <core\memory\memory.hxx>
memory::c_mapped_memory* g_memory = nullptr;

#include <core\communication\shared\transport.hxx>
#include <core\communication\shared\structs.h>
#include <core\communication\commands\commands.hxx>
#include <core\communication\shared\shared.hxx>
communication::c_communication* g_communication = nullptr;