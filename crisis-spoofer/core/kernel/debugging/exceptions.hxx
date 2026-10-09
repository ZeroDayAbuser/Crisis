#pragma once

namespace debugging {
namespace exception {

    inline volatile long g_bugcheck_dumped  = 0;
    inline volatile long g_exception_dumped = 0;

    inline const char* nt_status_name( std::uint32_t code ) {
        switch ( code ) {
            case 0x80000003: return "STATUS_BREAKPOINT";
            case 0x80000004: return "STATUS_SINGLE_STEP";
            case 0xC0000005: return "STATUS_ACCESS_VIOLATION";
            case 0xC0000006: return "STATUS_IN_PAGE_ERROR";
            case 0xC000001D: return "STATUS_ILLEGAL_INSTRUCTION";
            case 0xC0000025: return "STATUS_NONCONTINUABLE_EXCEPTION";
            case 0xC000008C: return "STATUS_ARRAY_BOUNDS_EXCEEDED";
            case 0xC000008D: return "STATUS_FLOAT_DENORMAL_OPERAND";
            case 0xC000008E: return "STATUS_FLOAT_DIVIDE_BY_ZERO";
            case 0xC0000094: return "STATUS_INTEGER_DIVIDE_BY_ZERO";
            case 0xC0000095: return "STATUS_INTEGER_OVERFLOW";
            case 0xC0000096: return "STATUS_PRIVILEGED_INSTRUCTION";
            case 0xC00000FD: return "STATUS_STACK_OVERFLOW";
            case 0xC0000135: return "STATUS_DLL_NOT_FOUND";
            case 0xC0000409: return "STATUS_STACK_BUFFER_OVERRUN";
            case 0xC0000602: return "STATUS_FAIL_FAST_EXCEPTION";
            case 0xC0000420: return "STATUS_ASSERTION_FAILURE";
            default:         return "UNKNOWN_NT_STATUS";
        }
    }

    inline const char* bugcheck_name( unsigned int code ) {
        switch ( code ) {
            case 0x0A:  return "IRQL_NOT_LESS_OR_EQUAL";
            case 0x18:  return "REFERENCE_BY_POINTER";
            case 0x1A:  return "MEMORY_MANAGEMENT";
            case 0x1E:  return "KMODE_EXCEPTION_NOT_HANDLED";
            case 0x3B:  return "SYSTEM_SERVICE_EXCEPTION";
            case 0x3D:  return "INTERRUPT_EXCEPTION_NOT_HANDLED";
            case 0x4E:  return "PFN_LIST_CORRUPT";
            case 0x50:  return "PAGE_FAULT_IN_NONPAGED_AREA";
            case 0x7E:  return "SYSTEM_THREAD_EXCEPTION_NOT_HANDLED";
            case 0x8E:  return "KERNEL_MODE_EXCEPTION_NOT_HANDLED";
            case 0xC1:  return "SPECIAL_POOL_DETECTED_MEMORY_CORRUPTION";
            case 0xC2:  return "BAD_POOL_CALLER";
            case 0xC4:  return "DRIVER_VERIFIER_DETECTED_VIOLATION";
            case 0xC5:  return "DRIVER_CORRUPTED_EXPOOL";
            case 0xC9:  return "DRIVER_VERIFIER_IOMANAGER_VIOLATION";
            case 0xD1:  return "DRIVER_IRQL_NOT_LESS_OR_EQUAL";
            case 0xD5:  return "DRIVER_PAGE_FAULT_IN_FREED_SPECIAL_POOL";
            case 0xE9:  return "USER_MODE_HEALTH_MONITOR";
            case 0xF7:  return "DRIVER_OVERRAN_STACK_BUFFER";
            case 0xFC:  return "ATTEMPTED_EXECUTE_OF_NOEXECUTE_MEMORY";
            case 0x101: return "CLOCK_WATCHDOG_TIMEOUT";
            case 0x109: return "CRITICAL_STRUCTURE_CORRUPTION (PatchGuard)";
            case 0x10E: return "VIDEO_MEMORY_MANAGEMENT_INTERNAL";
            case 0x133: return "DPC_WATCHDOG_VIOLATION";
            case 0x139: return "KERNEL_SECURITY_CHECK_FAILURE";
            case 0x14C: return "FATAL_ABNORMAL_RESET_ERROR";
            case 0x18B: return "SECURE_KERNEL_ERROR";
            default:    return "UNKNOWN_BUGCHECK";
        }
    }

    inline const char* bugcheck_hint( unsigned int code ) {
        switch ( code ) {
            case 0x0A: return "invalid pointer or race dereferencing something at DPC/DISPATCH_LEVEL";
            case 0x1E: return "kernel-mode unhandled exception - see p1=code, p2=address, p3=info0, p4=info1";
            case 0x50: return "referenced a non-existent kernel page; hooked page unmapped mid-execution?";
            case 0x7E: return "system thread threw unhandled exception; p1=code p2=address p3=exception_record p4=context";
            case 0xC1: return "special pool corruption around one of our allocations";
            case 0xD1: return "same as 0xA but from a driver at DPC_LEVEL - check hook trampoline IRQL";
            case 0xE9: return "user-mode healthy monitor tripped by a monitored critical process";
            case 0xF7: return "GS cookie corruption - shellcode/trampoline overran its stack frame";
            case 0xFC: return "attempted execute of non-executable memory - PTE NX bit set on our hook page?";
            case 0x109: return "PatchGuard verified structure corruption - see p1 (validation identity), p2 (region)";
            case 0x133: return "watchdog: a CPU was stuck in DPC too long - long-running trampoline?";
            case 0x139: return "SEH/GS/CFG guard tripped - p1 identifies the check (0=stack cookie, 2=cast fail, 3=cfg)";
            default:    return "";
        }
    }

    inline bool safe_read( std::uint64_t va, void* dst, std::size_t sz ) {
        if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( va ) ) )
            return false;
        if ( sz == 0 ) return true;
        if ( !kernel::mm_is_address_valid( reinterpret_cast< void* >( va + sz - 1 ) ) )
            return false;
        __try {
            RtlCopyMemory( dst, reinterpret_cast< const void* >( va ), sz );
            return true;
        } __except ( EXCEPTION_EXECUTE_HANDLER ) {
            return false;
        }
    }

    inline void classify_address( std::uint64_t addr, char* out, std::size_t n ) {
        const auto our_base = kernel::m_image_base;
        const auto our_size = kernel::m_image_size;
        const auto nt_base  = kernel::m_ntoskrnl_base;
        const auto nt_size  = kernel::m_ntoskrnl_size;

        if ( our_base && addr >= our_base && addr < our_base + our_size ) {
            _snprintf( out, n, "crisis-spoofer+0x%llx (RVA=0x%llx)",
                addr - our_base, addr - our_base );
            return;
        }
        if ( nt_base && addr >= nt_base && addr < nt_base + nt_size ) {
            _snprintf( out, n, "ntoskrnl+0x%llx (RVA=0x%llx)",
                addr - nt_base, addr - nt_base );
            return;
        }
        if ( addr < 0x10000ull ) {
            _snprintf( out, n, "<null-region 0x%llx>", addr );
            return;
        }
        if ( addr < 0x00007FFFFFFFFFFFull ) {
            _snprintf( out, n, "user-space 0x%llx", addr );
            return;
        }
        _snprintf( out, n, "kernel 0x%llx <unknown module>", addr );
    }

    inline void emit_bytes( const char* label, std::uint64_t va, std::size_t n ) {
        std::uint8_t buf[ 64 ] = { };
        if ( n > sizeof( buf ) ) n = sizeof( buf );
        if ( !safe_read( va, buf, n ) ) {
            disk::print( obf( "  %s @ 0x%016llx : <unreadable>" ), label, va );
            return;
        }
        char line[ 320 ] = { };
        int off = _snprintf( line, sizeof( line ) - 1,
            "  %s @ 0x%016llx :", label, va );
        for ( std::size_t i = 0; i < n && off < static_cast< int >( sizeof( line ) ) - 4; ++i )
            off += _snprintf( line + off, sizeof( line ) - 1 - off, " %02x", buf[ i ] );
        disk::print( obf( "%s" ), line );
    }

    inline void emit_image_map( ) {
        disk::print( obf( "[image-map]" ) );
        disk::print( obf( "  crisis-spoofer : base=0x%016llx  size=0x%llx" ),
            kernel::m_image_base, kernel::m_image_size );
        disk::print( obf( "  ntoskrnl.exe   : base=0x%016llx  size=0x%llx" ),
            kernel::m_ntoskrnl_base, kernel::m_ntoskrnl_size );
        disk::print( obf( "  system-dtb (cr3): 0x%llx" ), __readcr3( ) );
    }

    inline void emit_hooks( ) {
        static const auto s_ke_bugcheck       = pdb::get_symbol( obf("KeBugCheckEx") );
        static const auto s_kdp_report        = pdb::get_symbol( obf("KdpReport") );
        static const auto s_obp               = pdb::get_symbol( obf("ObpCallPreOperationCallbacks") );
        static const auto s_psp               = pdb::get_symbol( obf("PspCallThreadNotifyRoutines") );
        static const auto s_mm_secure         = pdb::get_symbol( obf("MmSecureVirtualMemoryEx") );
        static const auto s_mi_cow            = pdb::get_symbol( obf("MiCopyOnWrite") );
        static const auto s_etw               = pdb::get_symbol( obf("EtwTraceSiloKernelEvent") );
        static const auto s_secure_boot       = pdb::get_symbol( obf("SeSecureBootQueryInformation") );
        static const auto s_halp_get_bios     = pdb::get_symbol( obf("HalpAcpiGetTableFromBios") );
        static const auto s_halp_copy_bios    = pdb::get_symbol( obf("HalpAcpiCopyBiosTable") );
        static const auto s_ki_syscall64      = pdb::get_symbol( obf("KiSystemCall64") );
        static const auto s_ke_sdt            = pdb::get_symbol( obf("KeServiceDescriptorTable") );
        disk::print( obf( "[hooks-installed-by-us]" ) );
        const auto emit = [ ]( const char* name, std::uint64_t va ) {
            if ( !va ) return;
            char cls[ 160 ] = { };
            classify_address( va, cls, sizeof( cls ) );
            disk::print( obf( "  %-40s @ %s" ), name, cls );
        };
        emit( "KeBugCheckEx",                    s_ke_bugcheck );
        emit( "KdpReport",                       s_kdp_report );
        emit( "ObpCallPreOperationCallbacks",    s_obp );
        emit( "PspCallThreadNotifyRoutines",     s_psp );
        emit( "MmSecureVirtualMemoryEx",         s_mm_secure );
        emit( "MiCopyOnWrite",                   s_mi_cow );
        emit( "EtwTraceSiloKernelEvent",         s_etw );
        emit( "SeSecureBootQueryInformation",    s_secure_boot );
        emit( "HalpAcpiGetTableFromBios",        s_halp_get_bios );
        emit( "HalpAcpiCopyBiosTable",           s_halp_copy_bios );
        emit( "KiSystemCall64 (etw-hook target)", s_ki_syscall64 );
        emit( "KeServiceDescriptorTable",         s_ke_sdt );
    }

    inline void emit_process( ) {
        const auto proc = kernel::ps_get_current_process( );
        char image_name[ 32 ] = "?";
        if ( proc ) {
            const auto off = kernel::get_image_file_name_offset( );
            if ( off ) {
                const char* src = reinterpret_cast< const char* >(
                    reinterpret_cast< std::uintptr_t >( proc ) + off );
                for ( std::size_t i = 0; i < 15 && src[ i ]; ++i )
                    image_name[ i ] = src[ i ];
            }
        }
        disk::print( obf( "[process-context]" ) );
        disk::print( obf( "  eprocess=0x%016llx  image=%s" ),
            reinterpret_cast< std::uint64_t >( proc ), image_name );
        disk::print( obf( "  pid=%llu  tid=%llu  irql=%u" ),
            reinterpret_cast< std::uint64_t >( PsGetCurrentProcessId( ) ),
            reinterpret_cast< std::uint64_t >( PsGetCurrentThreadId( ) ),
            static_cast< unsigned >( KeGetCurrentIrql( ) ) );
    }

    inline void emit_context( PCONTEXT ctx ) {
        disk::print( obf( "[registers]" ) );
        disk::print( obf( "  rax=0x%016llx  rbx=0x%016llx  rcx=0x%016llx  rdx=0x%016llx" ),
            ctx->Rax, ctx->Rbx, ctx->Rcx, ctx->Rdx );
        disk::print( obf( "  rsi=0x%016llx  rdi=0x%016llx  rbp=0x%016llx  rsp=0x%016llx" ),
            ctx->Rsi, ctx->Rdi, ctx->Rbp, ctx->Rsp );
        disk::print( obf( "  r8 =0x%016llx  r9 =0x%016llx  r10=0x%016llx  r11=0x%016llx" ),
            ctx->R8,  ctx->R9,  ctx->R10, ctx->R11 );
        disk::print( obf( "  r12=0x%016llx  r13=0x%016llx  r14=0x%016llx  r15=0x%016llx" ),
            ctx->R12, ctx->R13, ctx->R14, ctx->R15 );
        disk::print( obf( "  rip=0x%016llx  rflags=0x%08x" ),
            ctx->Rip, ctx->EFlags );
        disk::print( obf( "  cs=0x%04x ds=0x%04x es=0x%04x fs=0x%04x gs=0x%04x ss=0x%04x" ),
            ctx->SegCs, ctx->SegDs, ctx->SegEs, ctx->SegFs, ctx->SegGs, ctx->SegSs );
        disk::print( obf( "  cr0=0x%llx  cr2=0x%llx  cr3=0x%llx  cr4=0x%llx  cr8=0x%llx" ),
            static_cast< std::uint64_t >( __readcr0( ) ),
            static_cast< std::uint64_t >( __readcr2( ) ),
            static_cast< std::uint64_t >( __readcr3( ) ),
            static_cast< std::uint64_t >( __readcr4( ) ),
            static_cast< std::uint64_t >( __readcr8( ) ) );
    }

    inline void emit_stack_walk( std::uint64_t rsp ) {
        disk::print( obf( "[stack-return-candidates]" ) );
        std::uint64_t stack_low = 0, stack_high = 0;
        IoGetStackLimits( &stack_low, &stack_high );
        disk::print( obf( "  stack-range: 0x%016llx - 0x%016llx  (probed rsp=0x%016llx)" ),
            stack_low, stack_high, rsp );

        auto scan = ( rsp + 7 ) & ~static_cast< std::uint64_t >( 7 );
        int printed = 0;
        int scanned = 0;
        while ( scan + 8 <= stack_high && printed < 24 && scanned < 512 ) {
            ++scanned;
            std::uint64_t v = 0;
            if ( !safe_read( scan, &v, sizeof( v ) ) ) {
                scan += 8;
                continue;
            }
            if ( v >= reinterpret_cast< std::uint64_t >( MmSystemRangeStart ) ) {
                char cls[ 160 ] = { };
                classify_address( v, cls, sizeof( cls ) );
                disk::print( obf( "  [rsp+0x%03llx] 0x%016llx  %s" ),
                    scan - rsp, v, cls );
                ++printed;
            }
            scan += 8;
        }
    }

    inline void emit_rip_context( std::uint64_t rip ) {
        char cls[ 160 ] = { };
        classify_address( rip, cls, sizeof( cls ) );
        disk::print( obf( "[fault-site]" ) );
        disk::print( obf( "  rip=0x%016llx  %s" ), rip, cls );
        emit_bytes( "bytes-at-rip     ", rip,        32 );
        emit_bytes( "bytes-before-rip ", rip - 16,   16 );
    }

    inline void exception_dump(
        PEXCEPTION_RECORD rec,
        PCONTEXT          ctx,
        KPROCESSOR_MODE   mode,
        bool              first_chance )
    {
        if ( InterlockedCompareExchange( &g_exception_dumped, 1, 0 ) != 0 )
            return;
        if ( !rec || !ctx )
            return;

        disk::print( obf( "================================================================" ) );
        disk::print( obf( "[EXCEPTION-DUMP #1 : first kernel-mode exception this session]" ) );
        disk::print( obf( "================================================================" ) );

        char site[ 160 ] = { };
        classify_address( reinterpret_cast< std::uint64_t >( rec->ExceptionAddress ), site, sizeof( site ) );
        disk::print( obf( "[summary]" ) );
        disk::print( obf( "  code           : 0x%08x (%s)" ),
            rec->ExceptionCode, nt_status_name( rec->ExceptionCode ) );
        disk::print( obf( "  exception-addr : 0x%016llx  %s" ),
            reinterpret_cast< std::uint64_t >( rec->ExceptionAddress ), site );
        disk::print( obf( "  flags          : 0x%08x" ), rec->ExceptionFlags );
        disk::print( obf( "  first-chance   : %d" ), first_chance ? 1 : 0 );
        disk::print( obf( "  processor-mode : %s (%d)" ),
            mode == KernelMode ? "KernelMode" : "UserMode",
            static_cast< int >( mode ) );

        disk::print( obf( "[exception-parameters] count=%lu" ), rec->NumberParameters );
        for ( std::uint32_t i = 0;
              i < rec->NumberParameters && i < EXCEPTION_MAXIMUM_PARAMETERS;
              ++i )
        {
            const auto v = rec->ExceptionInformation[ i ];
            char cls[ 160 ] = { };
            classify_address( v, cls, sizeof( cls ) );
            disk::print( obf( "  info[%u] = 0x%016llx  %s" ), i, v, cls );
        }

        if ( rec->ExceptionCode == 0xC0000005 && rec->NumberParameters >= 2 ) {
            const auto access = rec->ExceptionInformation[ 0 ];
            const auto fault  = rec->ExceptionInformation[ 1 ];
            const char* kind =
                access == 0 ? "read" :
                access == 1 ? "write" :
                access == 8 ? "execute" : "unknown";
            disk::print( obf( "  av-decoded     : %s @ 0x%016llx" ), kind, fault );
        }

        emit_process( );
        emit_image_map( );
        emit_hooks( );
        emit_context( ctx );
        emit_rip_context( ctx->Rip );
        emit_stack_walk( ctx->Rsp );

        disk::print( obf( "[analysis-hints]" ) );
        const bool rip_in_ours =
            kernel::m_image_base && ctx->Rip >= kernel::m_image_base &&
            ctx->Rip < kernel::m_image_base + kernel::m_image_size;
        const bool rip_in_nt =
            kernel::m_ntoskrnl_base && ctx->Rip >= kernel::m_ntoskrnl_base &&
            ctx->Rip < kernel::m_ntoskrnl_base + kernel::m_ntoskrnl_size;
        disk::print( obf( "  rip-in-crisis-spoofer : %d" ), rip_in_ours ? 1 : 0 );
        disk::print( obf( "  rip-in-ntoskrnl       : %d" ), rip_in_nt ? 1 : 0 );
        if ( rip_in_ours )
            disk::print( obf( "  hint: fault inside our image at RVA=0x%llx - inspect the hook body at that offset" ),
                ctx->Rip - kernel::m_image_base );
        if ( rip_in_nt )
            disk::print( obf( "  hint: fault inside ntoskrnl RVA=0x%llx - check if this RVA matches a function we hooked (see hooks-installed-by-us)" ),
                ctx->Rip - kernel::m_ntoskrnl_base );
        disk::print( obf( "[EXCEPTION-DUMP-END]" ) );
        disk::print( obf( "================================================================" ) );
    }

    inline void bugcheck_dump(
        unsigned int  code,
        std::uint64_t p1,
        std::uint64_t p2,
        std::uint64_t p3,
        std::uint64_t p4 )
    {
        if ( InterlockedCompareExchange( &g_bugcheck_dumped, 1, 0 ) != 0 )
            return;

        CONTEXT ctx = { };
        ctx.ContextFlags = CONTEXT_FULL;
        kernel::rtl_capture_context( &ctx );

        disk::print( obf( "================================================================" ) );
        disk::print( obf( "[BUGCHECK-DUMP #1 : first KeBugCheckEx call intercepted]" ) );
        disk::print( obf( "================================================================" ) );

        disk::print( obf( "[summary]" ) );
        disk::print( obf( "  code : 0x%X (%s)" ), code, bugcheck_name( code ) );
        const char* hint = bugcheck_hint( code );
        if ( hint && *hint )
            disk::print( obf( "  hint : %s" ), hint );

        disk::print( obf( "[bugcheck-parameters]" ) );
        char cls[ 160 ] = { };
        classify_address( p1, cls, sizeof( cls ) );
        disk::print( obf( "  p1 = 0x%016llx  %s" ), p1, cls );
        classify_address( p2, cls, sizeof( cls ) );
        disk::print( obf( "  p2 = 0x%016llx  %s" ), p2, cls );
        classify_address( p3, cls, sizeof( cls ) );
        disk::print( obf( "  p3 = 0x%016llx  %s" ), p3, cls );
        classify_address( p4, cls, sizeof( cls ) );
        disk::print( obf( "  p4 = 0x%016llx  %s" ), p4, cls );

        if ( code == 0x3B && p1 ) {
            const auto exc_code = static_cast< std::uint32_t >( p1 >> 32 );
            disk::print( obf( "  p1-decoded : nt-status=0x%08x (%s)" ),
                exc_code, nt_status_name( exc_code ) );
            disk::print( obf( "  p2-decoded : faulting instruction va=0x%016llx" ), p2 );
        }

        if ( code == 0x50 ) {
            const char* kind = p2 == 0 ? "read"
                             : p2 == 1 ? "write"
                             : p2 == 8 ? "execute" : "?";
            disk::print( obf( "  p1-decoded : referenced va=0x%016llx" ), p1 );
            disk::print( obf( "  p2-decoded : access-type=%s" ), kind );
        }

        if ( code == 0x109 ) {
            disk::print( obf( "  p1-decoded : PatchGuard validation identity 0x%llx" ), p1 );
            disk::print( obf( "  p3-decoded : corrupted region VA 0x%llx" ), p3 );
            disk::print( obf( "  p4-decoded : corruption kind identifier 0x%llx" ), p4 );
        }

        if ( code == 0xD1 || code == 0x0A ) {
            disk::print( obf( "  p1-decoded : referenced va=0x%016llx" ), p1 );
            disk::print( obf( "  p2-decoded : irql at fault=%llu" ), p2 );
            const char* kind = p3 == 0 ? "read"
                             : p3 == 1 ? "write"
                             : p3 == 8 ? "execute" : "?";
            disk::print( obf( "  p3-decoded : access-type=%s" ), kind );
            disk::print( obf( "  p4-decoded : faulting instruction va=0x%016llx" ), p4 );
        }

        emit_process( );
        emit_image_map( );
        emit_hooks( );
        emit_context( &ctx );
        emit_rip_context( ctx.Rip );
        emit_stack_walk( ctx.Rsp );

        disk::print( obf( "[analysis-hints]" ) );
        auto in_ours = [ ]( std::uint64_t a ) {
            return kernel::m_image_base && a >= kernel::m_image_base &&
                   a < kernel::m_image_base + kernel::m_image_size;
        };
        auto in_nt = [ ]( std::uint64_t a ) {
            return kernel::m_ntoskrnl_base && a >= kernel::m_ntoskrnl_base &&
                   a < kernel::m_ntoskrnl_base + kernel::m_ntoskrnl_size;
        };
        if ( in_ours( ctx.Rip ) )
            disk::print( obf( "  bugcheck-caller inside our image RVA=0x%llx - shellcode faulted or hook trampoline broke" ),
                ctx.Rip - kernel::m_image_base );
        if ( in_ours( p2 ) )
            disk::print( obf( "  p2 points into our image RVA=0x%llx - PatchGuard likely spotted our modification here" ),
                p2 - kernel::m_image_base );
        if ( in_ours( p3 ) )
            disk::print( obf( "  p3 points into our image RVA=0x%llx" ),
                p3 - kernel::m_image_base );
        if ( in_nt( p2 ) )
            disk::print( obf( "  p2 points into ntoskrnl RVA=0x%llx - compare against hooks-installed-by-us above" ),
                p2 - kernel::m_ntoskrnl_base );
        if ( code == 0x109 )
            disk::print( obf( "  reminder: 0x109 is PatchGuard. If p3 falls inside a page you patched, PG caught a hash mismatch." ) );
        if ( code == 0x139 )
            disk::print( obf( "  reminder: 0x139 subcode in p1 - 0=stack-cookie, 2=cast-guard, 3=cfg, 5=cfg-write, most others are cookie corruption from a trampoline/shellcode overflow." ) );
        disk::print( obf( "[BUGCHECK-DUMP-END]" ) );
        disk::print( obf( "================================================================" ) );
    }

}
}
