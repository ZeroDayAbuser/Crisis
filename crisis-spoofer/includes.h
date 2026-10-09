#pragma once
#include <intrin.h>
#include <string.h>
#include <ntddk.h>
#include <stdarg.h>
#include <stdio.h>

#include <deps/types.h>
#include <deps/ia32/ia32.h>
#include <deps/intrinsic.hxx>
#include <deps/pdb/pdb.h>
#include <deps/entry.h>

#include <ext/obfuscation/obfuscation.h>
#include <ext/hde/hde.h>
#include <Zydis/Zydis.h>

#include <core/kernel/kernel.hxx>
#include <core/kernel/exports.hxx>
#include <core/kernel/debugging/debugging.hxx>
#include <core/kernel/module/module.hxx>

#include <core/memory/physical.hxx>
#include <core/paging/pagetables.h>
#include <core/paging/dpm/dpm.hxx>

#include <core/paging/paging.hxx>
#include <core/paging/privatize.hxx>
#include <core/paging/shadow/shadow.hxx>

#include <core/memory/ssdt.hxx>
#include <core/memory/memory.hxx>

#include <core/paging/pph/pph.hxx>
#include <core/paging/pth/pth.hxx>
#include <core/hooks/exception/framework/detour.hxx>
#include <core/hooks/exception/framework/emulator.hxx>
#include <core/hooks/exception/framework/cr3.hxx>

#include <core/kernel/image/image.hxx>
#include <core/kernel/debugging/exceptions.hxx>

#include <core/hooks/exception/utility/utility.hxx>
#include <core/hooks/exception/framework/patchguard.hxx>
#include <core/hooks/exception/framework/communication/request.h>
#include <core/hooks/exception/framework/communication/communication.hxx>
#include <core/hooks/exception/exception.hxx>

#include <core/hooks/etw/etw.hxx>
#include <core/hooks/etw/tracker.hxx>

#include <core/kernel/registry/registry.hxx>
#include <core/hooks/handlers/kernel.hxx>
#include <core/hooks/handlers/user.hxx>

#include <core/components/motherboard.hxx>
#include <core/components/firmware.hxx>
#include <core/components/monitor.hxx>
#include <core/components/boot.hxx>
#include <core/components/tpm.hxx>