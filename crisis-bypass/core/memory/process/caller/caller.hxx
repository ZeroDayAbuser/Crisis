#pragma once

namespace caller {

	struct nt_object_attributes_t {
		std::uint32_t m_length {};
		HANDLE        m_root_directory {};
		PVOID         m_object_name {};
		std::uint32_t m_attributes {};
		PVOID         m_security_descriptor {};
		PVOID         m_security_quality_of_service {};
	};

	struct nt_client_id_t {
		HANDLE m_unique_process {};
		HANDLE m_unique_thread {};
	};

	static constexpr std::uint32_t k_process_all_access = 0x001FFFFFu;

	// ── ring: written by bypass, read by worker (and vice versa) ───────────
	//
	// Ring lives inside a g_allocator-stitched user page in the target.
	// Bypass writes command fields, atomically flips m_status to PENDING;
	// worker sees PENDING, flips to RUNNING, executes op, writes m_result,
	// flips to DONE. Bypass polls m_status == DONE, reads result, flips IDLE.
	// SHUTDOWN is a special status the bypass writes to ask the worker to
	// tailcall RtlExitUserThread.
	struct command_region_t {
		volatile std::uint32_t m_seq_bypass;   // +0x00  bypass increments after posting
		volatile std::uint32_t m_seq_worker;   // +0x04  worker increments after completing
		volatile std::uint32_t m_status;       // +0x08  0=IDLE 1=PENDING 2=RUNNING 3=DONE 4=SHUTDOWN
		std::uint32_t          m_op;           // +0x0C  0=CALL (phase 1a)
		std::uint32_t          m_flags;        // +0x10
		std::uint32_t          m_error;        // +0x14
		std::uint64_t          m_func_va;      // +0x18
		std::uint64_t          m_args[ 10 ];   // +0x20..0x68
		std::uint64_t          m_result;       // +0x70
		std::uint64_t          m_mem_addr;     // +0x78  reserved for READ/WRITE (phase 1b)
		std::uint32_t          m_mem_size;     // +0x80
		std::uint32_t          m_batch_count;  // +0x84  reserved for BATCH/SCAN (phase 1b)
		std::uint64_t          m_batch_va;     // +0x88
		std::uint64_t          m_tick_target;  // +0x90  reserved for CALL_ON_TICK (later)
		std::uint32_t          m_tick_offset;  // +0x98
		std::uint32_t          m_pad_9c;       // +0x9C
		std::int64_t           m_delay_100ns;  // +0xA0  NtDelayExecution interval (negative = relative time)
		std::uint64_t          m_magic;        // +0xA8  handshake cookie — worker rejects dispatch if mismatched
		std::uint64_t          m_reserved[ 10 ]; // +0xB0..0xFF
		std::uint8_t           m_buffer[ 0xF00 ]; // +0x100..0xFFF
	};
	static_assert( sizeof( command_region_t ) == 0x1000, "command_region_t must be one page" );
	static_assert( offsetof( command_region_t, m_delay_100ns ) == 0xA0, "delay slot must be at +0xA0" );
	static_assert( offsetof( command_region_t, m_magic ) == 0xA8, "magic slot must be at +0xA8" );

	static constexpr std::uint64_t k_magic = 0xCA11ED1EBADBABE1ull;

	static constexpr std::size_t   k_ring_size = sizeof( command_region_t );
	static constexpr std::size_t   k_worker_code_size = 0x1000;

	static constexpr std::uint32_t k_status_idle = 0;
	static constexpr std::uint32_t k_status_pending = 1;
	static constexpr std::uint32_t k_status_running = 2;
	static constexpr std::uint32_t k_status_done = 3;
	static constexpr std::uint32_t k_status_shutdown = 4;

	static constexpr std::uint32_t k_op_call = 0;

	static constexpr int           k_call_timeout_ms = 500;
	static constexpr int           k_shutdown_timeout_ms = 500;

	// Worker's alertable delay after a spin budget (~4096 PAUSEs, ~150µs)
	// runs out. Kept short so idle response latency stays sub-millisecond.
	// Negative = relative time in 100ns units. -100 = 10µs.
	static constexpr std::int64_t  k_worker_idle_delay_100ns = -100;

	// ── kernel-side one-shot stub: RtlCreateUserThread from kernel context ─
	struct kernel_stub_ctx_t {
		std::uint64_t m_rtl_create_thread {};
		std::uint64_t m_reserved {};
		HANDLE        m_process_handle {};
		std::uint64_t m_start_va {};
		std::uint64_t m_param_va {};
		HANDLE* m_thread_handle_out {};
		void* m_client_id_out {};
	};

	inline constexpr std::uint8_t k_kernel_stub[ ] = {
		0x53,
		0x48, 0x89, 0xCB,
		0x48, 0x83, 0xEC, 0x50,
		0x48, 0x8B, 0x4B, 0x10,
		0x33, 0xD2,
		0x45, 0x33, 0xC0,
		0x45, 0x33, 0xC9,
		0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00,
		0x48, 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00,
		0x48, 0x8B, 0x43, 0x18,
		0x48, 0x89, 0x44, 0x24, 0x30,
		0x48, 0x8B, 0x43, 0x20,
		0x48, 0x89, 0x44, 0x24, 0x38,
		0x48, 0x8B, 0x43, 0x28,
		0x48, 0x89, 0x44, 0x24, 0x40,
		0x48, 0x8B, 0x43, 0x30,
		0x48, 0x89, 0x44, 0x24, 0x48,
		0x48, 0x8B, 0x03,
		0xFF, 0xD0,
		0x48, 0x83, 0xC4, 0x50,
		0x5B,
		0xC3,
	};

	// ── kernel-side open-process stub: ObOpenObjectByPointer / KernelMode ───
	// Uses ObOpenObjectByPointer(KernelMode, OBJ_KERNEL_HANDLE) so OB callbacks
	// see a kernel handle and cannot strip access rights the way they do for
	// user-mode handles opened via ZwOpenProcess.
	//
	// Context layout (open_process_ctx_t, rcx on entry):
	//   +0x00  PsLookupProcessByProcessId VA
	//   +0x08  ObOpenObjectByPointer VA
	//   +0x10  ObDereferenceObject VA
	//   +0x18  &PsProcessType (symbol VA — stub dereferences once)
	//   +0x20  process id (u64)
	//   +0x28  access mask (u32) + 4 bytes pad
	//   +0x30  out: HANDLE (written on success)
	struct open_process_ctx_t {
		std::uint64_t m_ps_lookup_process {};
		std::uint64_t m_ob_open_object    {};
		std::uint64_t m_ob_deref_object   {};
		std::uint64_t m_ps_process_type   {};
		std::uint64_t m_process_id        {};
		std::uint32_t m_access            {};
		std::uint32_t m_pad               {};
		std::uint64_t m_out_handle        {};
	};

	static constexpr std::size_t k_open_process_stub_off = 0x80;

	inline constexpr std::uint8_t k_open_process_stub[ ] = {
		0x53,                                           // push rbx
		0x56,                                           // push rsi
		0x57,                                           // push rdi
		0x48, 0x83, 0xEC, 0x50,                         // sub  rsp, 0x50
		0x48, 0x89, 0xCB,                               // mov  rbx, rcx
		0x48, 0xC7, 0x44, 0x24, 0x38, 0x00, 0x00, 0x00, 0x00, // mov  qword [rsp+0x38], 0
		0x48, 0x8B, 0x4B, 0x20,                         // mov  rcx, [rbx+0x20]  (process id)
		0x48, 0x8D, 0x54, 0x24, 0x38,                   // lea  rdx, [rsp+0x38]  (&eprocess local)
		0x48, 0x8B, 0x03,                               // mov  rax, [rbx+0x00]  (PsLookupProcessByProcessId)
		0xFF, 0xD0,                                     // call rax
		0x85, 0xC0,                                     // test eax, eax
		0x0F, 0x88, 0x41, 0x00, 0x00, 0x00,             // js   done (+0x41 from next instr)
		0x48, 0x8B, 0x74, 0x24, 0x38,                   // mov  rsi, [rsp+0x38]  (eprocess)
		0x48, 0x8B, 0x43, 0x18,                         // mov  rax, [rbx+0x18]  (&PsProcessType)
		0x48, 0x8B, 0x00,                               // mov  rax, [rax]       (*PsProcessType)
		0x48, 0x89, 0x44, 0x24, 0x20,                   // mov  [rsp+0x20], rax  (5th arg: ObjectType)
		0x48, 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00, // mov  qword [rsp+0x28], 0 (KernelMode)
		0x48, 0x8D, 0x7B, 0x30,                         // lea  rdi, [rbx+0x30]  (&out_handle)
		0x48, 0x89, 0x7C, 0x24, 0x30,                   // mov  [rsp+0x30], rdi  (7th arg: &Handle)
		0x48, 0x89, 0xF1,                               // mov  rcx, rsi         (Object)
		0xBA, 0x00, 0x02, 0x00, 0x00,                   // mov  edx, 0x200       (OBJ_KERNEL_HANDLE)
		0x45, 0x33, 0xC0,                               // xor  r8d, r8d         (AccessState = null)
		0x44, 0x8B, 0x4B, 0x28,                         // mov  r9d, [rbx+0x28]  (DesiredAccess)
		0x48, 0x8B, 0x43, 0x08,                         // mov  rax, [rbx+0x08]  (ObOpenObjectByPointer)
		0xFF, 0xD0,                                     // call rax
		0x48, 0x89, 0xF1,                               // mov  rcx, rsi         (eprocess for deref)
		0x48, 0x8B, 0x43, 0x10,                         // mov  rax, [rbx+0x10]  (ObDereferenceObject)
		0xFF, 0xD0,                                     // call rax
		0x48, 0x83, 0xC4, 0x50,                         // done: add  rsp, 0x50
		0x5F,                                           // pop  rdi
		0x5E,                                           // pop  rsi
		0x5B,                                           // pop  rbx
		0xC3,                                           // ret
	};

	// ── OB callback: strips PROCESS_TERMINATE from handles to the protected process ──
	//
	// rcx = ignored (RegistrationContext from entry)
	// rdx = POB_PRE_OPERATION_INFORMATION
	//   +0x08  Object     (PVOID)  – EPROCESS being accessed
	//   +0x20  Parameters (PVOID)  – OB_PRE_CREATE_HANDLE_INFORMATION
	//                                  +0x00 DesiredAccess (ACCESS_MASK)
	//
	// The 8-byte EPROCESS immediate at +0x06 is patched by the setup stub.
	struct ob_setup_ctx_t {
		std::uint64_t m_ps_lookup_process {};   // +0x00
		std::uint64_t m_ob_deref_object   {};   // +0x08
		std::uint64_t m_process_id        {};   // +0x10
		std::uint64_t m_imm64_patch_va    {};   // +0x18  VA of EPROCESS slot in callback
		std::uint64_t m_ob_entry_va       {};   // +0x20  VA of _OB_CALLBACK_ENTRY
		std::uint64_t m_object_type_va    {};   // +0x28  *PsProcessType value
		std::uint64_t m_callback_va       {};   // +0x30  VA of callback shellcode
		std::uint64_t m_ctx_va            {};   // +0x38  VA of zero-filled context area
		std::uint64_t m_list_head_va      {};   // +0x40  &OBJECT_TYPE.CallbackList
		std::uint64_t m_out_status        {};   // +0x48  out: NTSTATUS from PsLookup
	};

	static constexpr std::size_t k_ob_callback_eprocess_off = 0x06;
	static constexpr std::size_t k_ob_callback_off          = 0x000;
	static constexpr std::size_t k_ob_entry_off             = 0x040;
	static constexpr std::size_t k_ob_ctx_off               = 0x080;
	static constexpr std::size_t k_ob_setup_stub_off        = 0x100;
	static constexpr std::size_t k_ob_region_size           = 0x400;

	inline constexpr std::uint8_t k_ob_callback[ ] = {
		0x53,                                                           // push rbx
		0x48, 0x89, 0xD3,                                               // mov  rbx, rdx
		0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // mov  rax, imm64  ← patch +0x06
		0x48, 0x39, 0x43, 0x08,                                         // cmp  [rbx+0x08], rax
		0x75, 0x07,                                                     // jne  +7
		0x48, 0x8B, 0x43, 0x20,                                         // mov  rax, [rbx+0x20]
		0x83, 0x20, 0xFE,                                               // and  dword [rax], 0xFE
		0x31, 0xC0,                                                     // xor  eax, eax
		0x5B,                                                           // pop  rbx
		0xC3,                                                           // ret
	};

	// Setup stub: PsLookupProcessByProcessId → patch callback imm64 → fill entry → list splice
	// Stack: 5 pushes (r12/r13/r14/rsi/rbx) + sub 0x50 = 16-byte aligned before each call.
	// js-done rel32 = 0x55 (133 - 48 = 85).
	inline constexpr std::uint8_t k_ob_setup_stub[ ] = {
		0x41, 0x54,                                     // push r12
		0x41, 0x55,                                     // push r13
		0x41, 0x56,                                     // push r14
		0x56,                                           // push rsi
		0x53,                                           // push rbx
		0x48, 0x83, 0xEC, 0x50,                         // sub  rsp, 0x50
		0x48, 0x89, 0xCB,                               // mov  rbx, rcx
		0x31, 0xC0,                                     // xor  eax, eax
		0x48, 0x89, 0x44, 0x24, 0x20,                   // mov  [rsp+0x20], rax
		0x48, 0x8B, 0x4B, 0x10,                         // mov  rcx, [rbx+0x10]
		0x48, 0x8D, 0x54, 0x24, 0x20,                   // lea  rdx, [rsp+0x20]
		0x48, 0x8B, 0x03,                               // mov  rax, [rbx]
		0xFF, 0xD0,                                     // call rax
		0x48, 0x89, 0x43, 0x48,                         // mov  [rbx+0x48], rax
		0x85, 0xC0,                                     // test eax, eax
		0x0F, 0x88, 0x55, 0x00, 0x00, 0x00,             // js   done (+0x55)
		0x4C, 0x8B, 0x64, 0x24, 0x20,                   // mov  r12, [rsp+0x20]
		0x48, 0x8B, 0x43, 0x18,                         // mov  rax, [rbx+0x18]
		0x4C, 0x89, 0x20,                               // mov  [rax], r12
		0x4C, 0x8B, 0x6B, 0x20,                         // mov  r13, [rbx+0x20]
		0x41, 0xC7, 0x45, 0x10, 0x03, 0x00, 0x00, 0x00, // mov  dword [r13+0x10], 3
		0x41, 0xC6, 0x45, 0x14, 0x01,                   // mov  byte  [r13+0x14], 1
		0x48, 0x8B, 0x43, 0x28,                         // mov  rax, [rbx+0x28]
		0x49, 0x89, 0x45, 0x18,                         // mov  [r13+0x18], rax
		0x48, 0x8B, 0x43, 0x30,                         // mov  rax, [rbx+0x30]
		0x49, 0x89, 0x45, 0x20,                         // mov  [r13+0x20], rax
		0x48, 0x8B, 0x43, 0x38,                         // mov  rax, [rbx+0x38]
		0x49, 0x89, 0x45, 0x30,                         // mov  [r13+0x30], rax
		0x48, 0x8B, 0x73, 0x40,                         // mov  rsi, [rbx+0x40]
		0x49, 0x89, 0x75, 0x00,                         // mov  [r13+0x00], rsi
		0x4C, 0x8B, 0x76, 0x08,                         // mov  r14, [rsi+0x08]
		0x4D, 0x89, 0x75, 0x08,                         // mov  [r13+0x08], r14
		0x4D, 0x89, 0x2E,                               // mov  [r14], r13
		0x4C, 0x89, 0x6E, 0x08,                         // mov  [rsi+0x08], r13
		0x4C, 0x89, 0xE1,                               // mov  rcx, r12
		0x48, 0x8B, 0x43, 0x08,                         // mov  rax, [rbx+0x08]
		0xFF, 0xD0,                                     // call rax
		0x48, 0x83, 0xC4, 0x50,                         // done: add rsp, 0x50
		0x5B,                                           // pop  rbx
		0x5E,                                           // pop  rsi
		0x41, 0x5E,                                     // pop  r14
		0x41, 0x5D,                                     // pop  r13
		0x41, 0x5C,                                     // pop  r12
		0xC3,                                           // ret
	};

	// ── worker shellcode: infinite dispatch loop, one thread creation total ─
	//
	// Layout (positions relative to start of the byte array):
	//   [+0x00]  push rbx
	//   [+0x01]  sub  rsp, 0x68
	//   [+0x05]  mov  rbx, imm64(ring_va)    ← placeholder at +0x07
	// poll (=+0x0F):
	//   [+0x0F]  mov  eax, [rbx+0x08]
	//   [+0x12]  cmp  eax, 1
	//   [+0x15]  je   dispatch  (rel8)
	//   [+0x17]  cmp  eax, 4
	//   [+0x1A]  jne  poll      (rel8, back)  ← handles neither pending nor shutdown
	//   [+0x1C]  jmp  shutdown  (rel32)       ← handles shutdown
	// (fallthrough from jne poll is not what we want; use two branches)
	//
	// Actually, simpler control flow:
	// poll:
	//   mov eax, [rbx+8]
	//   cmp eax, 1
	//   je  dispatch
	//   cmp eax, 4
	//   je  shutdown
	//   pause
	//   jmp poll
	//
	// dispatch:
	//   mov dword [rbx+8], 2
	//   ; load 10 args from ring
	//   ; call func
	//   ; store result
	//   mov dword [rbx+8], 3
	//   inc dword [rbx+4]
	//   jmp poll
	//
	// shutdown:
	//   mov dword [rbx+8], 3
	//   inc dword [rbx+4]
	//   add rsp, 0x68; pop rbx
	//   xor ecx, ecx
	//   mov rax, imm64(rtl_exit_va)
	//   jmp rax
	//
	// The two placeholders are patched at init time:
	//   k_worker_stub_ring_va_offset       ← where imm64(ring_va) lives
	//   k_worker_stub_rtl_exit_va_offset   ← where imm64(rtl_exit_va) lives

	inline std::vector< std::uint8_t > build_worker_shellcode(
		std::uint64_t ring_va,
		std::uint64_t rtl_exit_va,
		std::uint64_t sleep_va,
		std::uint64_t magic
	) {
		std::vector< std::uint8_t > code;
		code.reserve( 256 );

		auto emit = [ & ]( std::initializer_list< std::uint8_t > b ) {
			for ( auto x : b ) code.push_back( x );
			};
		auto emit64 = [ & ]( std::uint64_t v ) {
			for ( int i = 0; i < 8; ++i )
				code.push_back( static_cast< std::uint8_t >( v >> ( i * 8 ) ) );
			};
		auto patch32 = [ & ]( std::size_t at, std::int32_t rel ) {
			std::memcpy( code.data( ) + at, &rel, 4 );
			};

		// prologue — RSP alignment: entry mod 16 = 8, push rbx → 0, sub 0x60 (mod 16 = 0) keeps it 0.
		// Before `call rax` we need rsp mod 16 = 0 so the callee sees mod 16 = 8 per Win64 ABI.
		emit( { 0x53 } );                                 // push rbx
		emit( { 0x48, 0x83, 0xEC, 0x60 } );               // sub  rsp, 0x60
		emit( { 0x48, 0xBB } ); emit64( ring_va );        // mov  rbx, imm64(ring_va)

		// Hybrid poll: spin PAUSE ~4096 times catching pending work in ~150µs,
		// then fall through to Sleep(1) before starting a fresh spin budget.
		// Gives near-zero response latency when calls are arriving without
		// pinning a core when idle.
		const auto poll_off = code.size( );
		emit( { 0x41, 0xBF, 0x00, 0x10, 0x00, 0x00 } );   // mov  r15d, 0x1000  (4096 spin budget)

		const auto spin_off = code.size( );
		emit( { 0x8B, 0x43, 0x08 } );                     // mov  eax, [rbx+0x08]
		emit( { 0x83, 0xF8, 0x01 } );                     // cmp  eax, 1
		emit( { 0x0F, 0x84, 0x00, 0x00, 0x00, 0x00 } );   // je   dispatch (rel32 patched)
		const auto je_dispatch_at = code.size( ) - 4;
		emit( { 0x83, 0xF8, 0x04 } );                     // cmp  eax, 4
		emit( { 0x0F, 0x84, 0x00, 0x00, 0x00, 0x00 } );   // je   shutdown (rel32 patched)
		const auto je_shutdown_at = code.size( ) - 4;
		emit( { 0xF3, 0x90 } );                           // pause
		emit( { 0x41, 0xFF, 0xCF } );                     // dec  r15d
		{
			// jnz spin (rel8 backward)
			const auto here_after = code.size( ) + 2;
			const auto rel = static_cast< std::int32_t >( spin_off ) - static_cast< std::int32_t >( here_after );
			emit( { 0x75, static_cast< std::uint8_t >( rel & 0xFF ) } );
		}

		// spin exhausted → one alertable delay, then loop back to reset budget.
		emit( { 0xB9, 0x01, 0x00, 0x00, 0x00 } );         // mov  ecx, 1                       (Sleep(1ms))
		emit( { 0x48, 0xB8 } ); emit64( sleep_va );       // mov  rax, imm64(kernel32!Sleep)
		emit( { 0xFF, 0xD0 } );                           // call rax
		{
			emit( { 0xE9, 0x00, 0x00, 0x00, 0x00 } );     // jmp poll (rel32 backward — reset budget)
			const auto patch_at = code.size( ) - 4;
			patch32( patch_at, static_cast< std::int32_t >( poll_off ) - static_cast< std::int32_t >( code.size( ) ) );
		}

		// dispatch:
		const auto dispatch_off = code.size( );
		patch32( je_dispatch_at,
			static_cast< std::int32_t >( dispatch_off ) - ( static_cast< std::int32_t >( je_dispatch_at ) + 4 ) );

		emit( { 0xC7, 0x43, 0x08, 0x02, 0x00, 0x00, 0x00 } ); // mov dword [rbx+0x08], 2 (RUNNING)

		emit( { 0x48, 0xB8 } ); emit64( magic );          // mov  rax, imm64(magic)
		emit( { 0x48, 0x39, 0x83, 0xA8, 0x00, 0x00, 0x00 } ); // cmp [rbx+0xA8], rax
		emit( { 0x0F, 0x85, 0x00, 0x00, 0x00, 0x00 } );   // jne  reject
		const auto jne_reject_magic = code.size( ) - 4;

		emit( { 0x8B, 0x43, 0x0C } );                     // mov  eax, [rbx+0x0C]
		emit( { 0x85, 0xC0 } );                           // test eax, eax
		emit( { 0x0F, 0x85, 0x00, 0x00, 0x00, 0x00 } );   // jne  reject
		const auto jne_reject_op = code.size( ) - 4;

		emit( { 0x4C, 0x8B, 0x73, 0x18 } );               // mov  r14, [rbx+0x18]
		emit( { 0x4D, 0x85, 0xF6 } );                     // test r14, r14
		emit( { 0x0F, 0x84, 0x00, 0x00, 0x00, 0x00 } );   // je   reject
		const auto je_reject_null = code.size( ) - 4;

		emit( { 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00 } ); // mov rax, 0x0000400000000000
		emit( { 0x4C, 0x39, 0xF0 } );                     // cmp  rax, r14
		emit( { 0x0F, 0x8F, 0x00, 0x00, 0x00, 0x00 } );   // jg   reject
		const auto jg_reject_range = code.size( ) - 4;

		emit( { 0x48, 0x8B, 0x4B, 0x20 } );               // mov  rcx, [rbx+0x20]
		emit( { 0x48, 0x8B, 0x53, 0x28 } );               // mov  rdx, [rbx+0x28]
		emit( { 0x4C, 0x8B, 0x43, 0x30 } );               // mov  r8,  [rbx+0x30]
		emit( { 0x4C, 0x8B, 0x4B, 0x38 } );               // mov  r9,  [rbx+0x38]

		const std::pair< std::uint8_t, std::uint8_t > xfers[ ] = {
			{ 0x40, 0x20 }, { 0x48, 0x28 }, { 0x50, 0x30 },
			{ 0x58, 0x38 }, { 0x60, 0x40 }, { 0x68, 0x48 },
		};
		for ( const auto& [src, dst] : xfers ) {
			emit( { 0x4C, 0x8B, 0x53, src } );            // mov  r10, [rbx+src]
			emit( { 0x4C, 0x89, 0x54, 0x24, dst } );      // mov  [rsp+dst], r10
		}

		emit( { 0x4C, 0x89, 0xF0 } );                     // mov  rax, r14
		emit( { 0xFF, 0xD0 } );                           // call rax
		emit( { 0x48, 0x89, 0x43, 0x70 } );               // mov  [rbx+0x70], rax

		// respond:
		const auto respond_off = code.size( );
		emit( { 0xFF, 0x43, 0x04 } );                     // inc  dword [rbx+0x04]
		emit( { 0xC7, 0x43, 0x08, 0x03, 0x00, 0x00, 0x00 } ); // mov [rbx+0x08], 3 (DONE)
		{
			emit( { 0xE9, 0x00, 0x00, 0x00, 0x00 } );     // jmp poll (rel32)
			const auto patch_at = code.size( ) - 4;
			patch32( patch_at, static_cast< std::int32_t >( poll_off ) - static_cast< std::int32_t >( code.size( ) ) );
		}

		// reject:
		const auto reject_off = code.size( );
		patch32( jne_reject_magic,
			static_cast< std::int32_t >( reject_off ) - ( static_cast< std::int32_t >( jne_reject_magic ) + 4 ) );
		patch32( jne_reject_op,
			static_cast< std::int32_t >( reject_off ) - ( static_cast< std::int32_t >( jne_reject_op ) + 4 ) );
		patch32( je_reject_null,
			static_cast< std::int32_t >( reject_off ) - ( static_cast< std::int32_t >( je_reject_null ) + 4 ) );
		patch32( jg_reject_range,
			static_cast< std::int32_t >( reject_off ) - ( static_cast< std::int32_t >( jg_reject_range ) + 4 ) );
		emit( { 0xC7, 0x43, 0x14, 0x11, 0xCA, 0xAD, 0x0B } ); // mov dword [rbx+0x14], 0x0BADCA11
		{
			emit( { 0xE9, 0x00, 0x00, 0x00, 0x00 } );
			const auto patch_at = code.size( ) - 4;
			patch32( patch_at, static_cast< std::int32_t >( respond_off ) - static_cast< std::int32_t >( code.size( ) ) );
		}

		// shutdown:
		const auto shutdown_off = code.size( );
		patch32( je_shutdown_at,
			static_cast< std::int32_t >( shutdown_off ) - ( static_cast< std::int32_t >( je_shutdown_at ) + 4 ) );

		emit( { 0xFF, 0x43, 0x04 } );                     // inc  dword [rbx+0x04]
		emit( { 0xC7, 0x43, 0x08, 0x03, 0x00, 0x00, 0x00 } ); // mov [rbx+0x08], 3
		emit( { 0x48, 0x83, 0xC4, 0x60 } );               // add  rsp, 0x60
		emit( { 0x5B } );                                 // pop  rbx
		emit( { 0x31, 0xC9 } );                           // xor  ecx, ecx
		emit( { 0x48, 0xB8 } ); emit64( rtl_exit_va );    // mov  rax, imm64(rtl_exit_va)
		emit( { 0xFF, 0xE0 } );                           // jmp  rax

		while ( code.size( ) % 16 )
			code.push_back( 0xCC );
		return code;
	}

	class c_caller {
	public:
		bool setup( std::uint32_t process_id, std::uint64_t process_peb ) {
			if ( !m_rtl_exit_thread_va ) {
				const auto ntdll = GetModuleHandleA( obf( "ntdll.dll" ) );
				if ( !ntdll )
					return false;

				m_rtl_exit_thread_va = reinterpret_cast< std::uint64_t >(
					GetProcAddress( ntdll, obf( "RtlExitUserThread" ) ) );
				if ( !m_rtl_exit_thread_va )
					return false;

				const auto k32 = GetModuleHandleA( obf( "kernel32.dll" ) );
				if ( !k32 )
					return false;

				m_sleep_va = reinterpret_cast< std::uint64_t >(
					GetProcAddress( k32, obf( "Sleep" ) ) );
				if ( !m_sleep_va )
					return false;

				m_zw_open_process      = g_scanner->get_symbol( obf( "ZwOpenProcess" ) );
				m_rtl_create_thread    = g_scanner->get_symbol( obf( "RtlCreateUserThread" ) );
				m_zw_close             = g_scanner->get_symbol( obf( "ZwClose" ) );
				m_zw_dup               = g_scanner->get_symbol( obf( "ZwDuplicateObject" ) );
				m_nt_get_next_thread   = g_scanner->get_symbol( obf( "NtGetNextThread" ) );
				m_nt_query_info_thread = g_scanner->get_symbol( obf( "NtQueryInformationThread" ) );
				m_ps_lookup_process_va = g_scanner->get_symbol( obf( "PsLookupProcessByProcessId" ) );
				m_ob_open_object_va    = g_scanner->get_symbol( obf( "ObOpenObjectByPointer" ) );
				m_ob_deref_object_va   = g_scanner->get_symbol( obf( "ObDereferenceObject" ) );
				m_ps_process_type_va   = g_scanner->get_symbol( obf( "PsProcessType" ) );

				if ( !m_rtl_create_thread || !m_zw_close ||
					!m_ps_lookup_process_va || !m_ob_open_object_va ||
					!m_ob_deref_object_va   || !m_ps_process_type_va )
					return false;
			}

			m_gadget_va = find_user_gadget( process_peb );
			if ( !m_gadget_va ) {
				logging::print( obf( "caller: no gadget available in target modules" ) );
				return false;
			}

			if ( !m_stub_region.valid( ) ) {
				auto region = g_kalloc->allocate_large_page( sizeof( k_kernel_stub ) );
				if ( !region ) {
					logging::print( obf( "caller: kernel stub allocation failed" ) );
					return false;
				}
				m_stub_region = *region;

				if ( !g_kalloc->write( m_stub_region, 0, k_kernel_stub, sizeof( k_kernel_stub ) ) ) {
					logging::print( obf( "caller: kernel stub write failed" ) );
					g_kalloc->free( m_stub_region );
					m_stub_region = {};
					return false;
				}

				if ( !g_kalloc->write( m_stub_region, k_open_process_stub_off,
					k_open_process_stub, sizeof( k_open_process_stub ) ) ) {
					logging::print( obf( "caller: open process stub write failed" ) );
					g_kalloc->free( m_stub_region );
					m_stub_region = {};
					return false;
				}

				m_stub_va = m_stub_region.m_va;
			}

			m_process_id = process_id;
			m_initialized = true;

			if ( !init_worker( ) ) {
				m_initialized = false;
				return false;
			}
			return true;
		}

		~c_caller( ) {
			stop_watchdog( );
			shutdown_worker( );
			if ( m_stub_region.valid( ) )
				g_kalloc->free( m_stub_region );
		}

		bool is_initialized( ) const { return m_initialized && m_worker_alive; }

		std::uint64_t ring_buffer_va( ) const {
			return m_ring_region.m_base_va + offsetof( command_region_t, m_buffer );
		}

		std::uint64_t call_function(
			std::uint64_t func,
			std::uint64_t a1 = 0, std::uint64_t a2 = 0, std::uint64_t a3 = 0, std::uint64_t a4 = 0,
			std::uint64_t a5 = 0, std::uint64_t a6 = 0, std::uint64_t a7 = 0, std::uint64_t a8 = 0,
			std::uint64_t a9 = 0, std::uint64_t a10 = 0,
			void* stack_params = nullptr,
			std::size_t   stack_params_size = 0
		) {
			if ( !m_initialized || !m_worker_alive )
				return 0;

			std::unique_lock lock( m_mutex );

			wait_for_pacer( );

			if ( !m_worker_alive ) {
				if ( !reinit_worker_locked( ) ) {
					m_call_failures.fetch_add( 1, std::memory_order_relaxed );
					return 0;
				}
			}

			command_region_t cmd {};
			cmd.m_op = k_op_call;
			cmd.m_flags = 0;
			cmd.m_error = 0;
			cmd.m_func_va = func;
			const auto actual_va = ring_buffer_va( );
			constexpr std::uint64_t param_base = 0x0000800000000000ull;
			constexpr std::uint64_t param_end = param_base + sizeof( command_region_t::m_buffer );
			const auto fixup_va = [ & ]( std::uint64_t v ) -> std::uint64_t {
				return ( v >= param_base && v < param_end ) ? actual_va + ( v - param_base ) : v;
				};
			cmd.m_args[ 0 ] = fixup_va( a1 );  cmd.m_args[ 1 ] = fixup_va( a2 );
			cmd.m_args[ 2 ] = fixup_va( a3 );  cmd.m_args[ 3 ] = fixup_va( a4 );
			cmd.m_args[ 4 ] = fixup_va( a5 );  cmd.m_args[ 5 ] = fixup_va( a6 );
			cmd.m_args[ 6 ] = fixup_va( a7 );  cmd.m_args[ 7 ] = fixup_va( a8 );
			cmd.m_args[ 8 ] = fixup_va( a9 );  cmd.m_args[ 9 ] = fixup_va( a10 );
			cmd.m_result = 0;
			cmd.m_seq_bypass = ++m_seq_bypass_counter;
			cmd.m_delay_100ns = k_worker_idle_delay_100ns;
			cmd.m_magic = k_magic;

			const auto phys_backed = [ ]( std::uint64_t v ) -> bool {
				if ( v < 0x10000 || v > 0x7FFFFFFFFFFull ) return true;
				return g_paging->translate_linear( v ) != 0;
				};
			if ( !phys_backed( func ) ||
				!phys_backed( cmd.m_args[ 0 ] ) ||
				!phys_backed( cmd.m_args[ 1 ] ) ) {
				m_call_failures.fetch_add( 1, std::memory_order_relaxed );
				return 0;
			}

			const auto ring = m_ring_region.m_base_va;
			if ( !memory::write_virtual( ring + offsetof( command_region_t, m_op ),
				&cmd.m_op,
				sizeof( command_region_t ) - offsetof( command_region_t, m_op ) - sizeof( cmd.m_buffer ) ) ) {
				logging::print( obf( "call: ring write failed func=0x%llx" ), func );
				m_worker_alive = false;
				m_call_failures.fetch_add( 1, std::memory_order_relaxed );
				return 0;
			}

			if ( stack_params && stack_params_size > 0 &&
				stack_params_size <= sizeof( command_region_t::m_buffer ) ) {
				std::vector< std::uint8_t > fixed_blob(
					static_cast< const std::uint8_t* >( stack_params ),
					static_cast< const std::uint8_t* >( stack_params ) + stack_params_size );
				for ( std::size_t i = 0; i + 8 <= fixed_blob.size( ); i += 8 ) {
					std::uint64_t v;
					std::memcpy( &v, fixed_blob.data( ) + i, 8 );
					if ( v >= param_base && v < param_end ) {
						v = actual_va + ( v - param_base );
						std::memcpy( fixed_blob.data( ) + i, &v, 8 );
					}
				}
				if ( !memory::write_virtual( ring + offsetof( command_region_t, m_buffer ),
					fixed_blob.data( ), fixed_blob.size( ) ) ) {
					m_worker_alive = false;
					m_call_failures.fetch_add( 1, std::memory_order_relaxed );
					return 0;
				}
			}

			std::uint32_t seq_before = 0;
			memory::read_virtual( ring + offsetof( command_region_t, m_seq_worker ),
				&seq_before, sizeof( seq_before ) );

			const std::uint32_t pending = k_status_pending;
			if ( !memory::write_virtual( ring + offsetof( command_region_t, m_status ),
				const_cast< std::uint32_t* >( &pending ), sizeof( pending ) ) ) {
				m_worker_alive = false;
				m_call_failures.fetch_add( 1, std::memory_order_relaxed );
				return 0;
			}

			std::uint64_t result = 0;
			std::uint32_t error = 0;
			if ( !wait_for_worker( result, error ) ) {
				logging::print( obf( "call: worker timed out func=0x%llx" ), func );
				m_worker_alive = false;
				m_call_failures.fetch_add( 1, std::memory_order_relaxed );
				return 0;
			}

			std::uint32_t seq_after = 0;
			if ( memory::read_virtual( ring + offsetof( command_region_t, m_seq_worker ),
				&seq_after, sizeof( seq_after ) ) &&
				seq_after != seq_before + 1 ) {
				logging::print( obf( "call: seq mismatch expected=%u got=%u" ),
					seq_before + 1, seq_after );
				m_worker_alive = false;
				m_call_failures.fetch_add( 1, std::memory_order_relaxed );
				return 0;
			}

			if ( stack_params && stack_params_size > 0 &&
				stack_params_size <= sizeof( command_region_t::m_buffer ) ) {
				memory::read_virtual( ring + offsetof( command_region_t, m_buffer ),
					stack_params, stack_params_size );
			}

			const std::uint32_t idle = k_status_idle;
			memory::write_virtual( ring + offsetof( command_region_t, m_status ),
				const_cast< std::uint32_t* >( &idle ), sizeof( idle ) );

			m_call_successes.fetch_add( 1, std::memory_order_relaxed );
			return result;
		}

		std::uint64_t successes( ) const { return m_call_successes.load( std::memory_order_relaxed ); }
		std::uint64_t failures( ) const { return m_call_failures.load( std::memory_order_relaxed ); }
		void reset_counters( ) {
			m_call_successes.store( 0, std::memory_order_relaxed );
			m_call_failures.store( 0, std::memory_order_relaxed );
		}

		void set_min_interval_us( std::uint64_t microseconds ) {
			if ( !m_qpc_frequency.QuadPart )
				QueryPerformanceFrequency( &m_qpc_frequency );
			m_min_interval_ticks = ( m_qpc_frequency.QuadPart * microseconds ) / 1'000'000ULL;
		}
		void set_min_interval_ms( std::uint32_t milliseconds ) {
			set_min_interval_us( static_cast< std::uint64_t >( milliseconds ) * 1000ULL );
		}
		std::uint64_t min_interval_us( ) const {
			if ( !m_qpc_frequency.QuadPart || !m_min_interval_ticks ) return 0;
			return ( m_min_interval_ticks * 1'000'000ULL ) / m_qpc_frequency.QuadPart;
		}

		bool worker_alive( ) const { return m_worker_alive; }

	private:
		bool init_worker( ) {
			if ( m_worker_alive )
				return true;

			auto ring = g_allocator->allocate_in_module_gap( k_ring_size,
				process::page_protection::read_write );
			if ( !ring || !ring->valid( ) ) {
				logging::print( obf( "worker: ring alloc failed" ) );
				return false;
			}
			m_ring_region = *ring;

			auto worker_code = g_allocator->allocate_in_module_gap( k_worker_code_size,
				process::page_protection::read_write_exec );
			if ( !worker_code || !worker_code->valid( ) ) {
				logging::print( obf( "worker: code alloc failed" ) );
				g_allocator->free_memory( m_ring_region );
				m_ring_region = {};
				return false;
			}
			m_worker_region = *worker_code;

			std::vector< std::uint8_t > ring_init( k_ring_size, 0 );
			const std::int64_t delay_100ns = k_worker_idle_delay_100ns;
			std::memcpy( ring_init.data( ) + offsetof( command_region_t, m_delay_100ns ),
				&delay_100ns, sizeof( delay_100ns ) );
			const std::uint64_t magic = k_magic;
			std::memcpy( ring_init.data( ) + offsetof( command_region_t, m_magic ),
				&magic, sizeof( magic ) );

			if ( !memory::write_virtual( m_ring_region.m_base_va,
				ring_init.data( ), ring_init.size( ) ) ) {
				logging::print( obf( "worker: ring write failed" ) );
				g_allocator->free_memory( m_worker_region );
				g_allocator->free_memory( m_ring_region );
				m_worker_region = {};
				m_ring_region = {};
				return false;
			}

			auto shellcode = build_worker_shellcode( m_ring_region.m_base_va,
				m_rtl_exit_thread_va, m_sleep_va, k_magic );
			if ( shellcode.size( ) > k_worker_code_size ) {
				logging::print( obf( "worker: shellcode too large (%zu)" ), shellcode.size( ) );
				g_allocator->free_memory( m_worker_region );
				g_allocator->free_memory( m_ring_region );
				m_worker_region = {};
				m_ring_region = {};
				return false;
			}

			if ( !memory::write_virtual( m_worker_region.m_base_va,
				shellcode.data( ), shellcode.size( ) ) ) {
				logging::print( obf( "worker: shellcode write failed" ) );
				g_allocator->free_memory( m_worker_region );
				g_allocator->free_memory( m_ring_region );
				m_worker_region = {};
				m_ring_region = {};
				return false;
			}

			HANDLE         thread_handle {};
			nt_client_id_t thread_cid {};

			void* process = nullptr;
			const auto process_handle = open_target_process( process );
			if ( !process_handle ) {
				logging::print( obf( "worker: open_target_process failed" ) );
				g_allocator->free_memory( m_worker_region );
				g_allocator->free_memory( m_ring_region );
				m_worker_region = {};
				m_ring_region = {};
				return false;
			}

			kernel_stub_ctx_t ctx {};
			ctx.m_rtl_create_thread = m_rtl_create_thread;
			ctx.m_process_handle = process_handle;
			ctx.m_start_va = m_gadget_va;
			ctx.m_param_va = m_worker_region.m_base_va;
			ctx.m_thread_handle_out = &thread_handle;
			ctx.m_client_id_out = &thread_cid;

			g_kalloc->set_protection( m_stub_region, allocation::e_protection::read_execute );
			g_syscall->call_kernel( m_stub_va, &ctx );
			g_kalloc->set_protection( m_stub_region, m_stub_region.m_section_prot );

			if ( thread_handle )
				copy_tls_to_worker( process_handle, thread_handle );

			close_target_process( process_handle, process );

			if ( !thread_handle ) {
				logging::print( obf( "worker: RtlCreateUserThread failed" ) );
				g_allocator->free_memory( m_worker_region );
				g_allocator->free_memory( m_ring_region );
				m_worker_region = {};
				m_ring_region = {};
				return false;
			}

			m_worker_thread_handle = thread_handle;
			m_worker_alive = true;
			m_seq_bypass_counter = 0;

			start_watchdog( );

			return true;
		}

		void shutdown_worker( ) {
			stop_watchdog( );

			if ( !m_worker_alive )
				return;

			const auto ring = m_ring_region.m_base_va;
			const std::uint32_t shutdown = k_status_shutdown;
			memory::write_virtual( ring + offsetof( command_region_t, m_status ),
				const_cast< std::uint32_t* >( &shutdown ), sizeof( shutdown ) );

			const auto deadline = std::chrono::steady_clock::now( ) +
				std::chrono::milliseconds( k_shutdown_timeout_ms );
			bool clean = false;
			while ( std::chrono::steady_clock::now( ) < deadline ) {
				std::uint32_t status = 0;
				if ( !memory::read_virtual( ring + offsetof( command_region_t, m_status ),
					&status, sizeof( status ) ) )
					break;
				if ( status == k_status_done ) {
					clean = true;
					break;
				}
				std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );
			}

			if ( m_worker_thread_handle )
				g_syscall->call_kernel( m_zw_close, m_worker_thread_handle );
			m_worker_thread_handle = nullptr;

			if ( clean ) {
				free_worker_regions( );
			}
			else {
				const std::uint64_t dead_magic = 0;
				memory::write_virtual( ring + offsetof( command_region_t, m_magic ),
					const_cast< std::uint64_t* >( &dead_magic ), sizeof( dead_magic ) );

				if ( m_worker_region.valid( ) ) {
					g_allocator->free_memory( m_worker_region );
					m_worker_region = {};
				}
				m_ring_region = {};
			}

			m_worker_alive = false;
		}

		bool reinit_worker_locked( ) {
			shutdown_worker( );
			return init_worker( );
		}

		bool wait_for_worker( std::uint64_t& out_result, std::uint32_t& out_error ) {
			const auto ring = m_ring_region.m_base_va;
			const auto status_va = ring + offsetof( command_region_t, m_status );
			const auto result_va = ring + offsetof( command_region_t, m_result );
			const auto error_va = ring + offsetof( command_region_t, m_error );

			LARGE_INTEGER freq {}, t0 {};
			QueryPerformanceFrequency( &freq );
			QueryPerformanceCounter( &t0 );
			const auto deadline_ticks = t0.QuadPart +
				( freq.QuadPart * k_call_timeout_ms ) / 1000LL;

			std::uint32_t status = 0;
			int deadline_check = 0;
			int spin_count = 0;
			while ( true ) {
				_mm_pause( );
				if ( ++spin_count < 64 ) {
					if ( ( ++deadline_check & 0xFFF ) == 0 ) {
						LARGE_INTEGER now {};
						QueryPerformanceCounter( &now );
						if ( now.QuadPart >= deadline_ticks )
							return false;
					}
					continue;
				}
				spin_count = 0;

				if ( !memory::read_virtual( status_va, &status, sizeof( status ) ) )
					return false;
				if ( status == k_status_done ) {
					memory::read_virtual( result_va, &out_result, sizeof( out_result ) );
					memory::read_virtual( error_va, &out_error, sizeof( out_error ) );
					return true;
				}

				if ( ( ++deadline_check & 0x3F ) == 0 ) {
					LARGE_INTEGER now {};
					QueryPerformanceCounter( &now );
					if ( now.QuadPart >= deadline_ticks )
						return false;
				}
			}
		}

		void copy_tls_to_worker( HANDLE process_handle, HANDLE worker_handle ) {
			if ( !m_nt_get_next_thread || !m_nt_query_info_thread )
				return;

			struct thread_basic_info_t {
				std::uint32_t exit_status;
				std::uint32_t pad;
				std::uint64_t teb_base_address;
				std::uint64_t unique_process;
				std::uint64_t unique_thread;
				std::uint64_t affinity_mask;
				std::int32_t  priority;
				std::int32_t  base_priority;
			};

			HANDLE ref_thread = nullptr;
			g_syscall->call_kernel( m_nt_get_next_thread,
				process_handle,
				( HANDLE )nullptr,
				( std::uint64_t ) 0x1FFFFFull,
				( std::uint64_t ) 0,
				( std::uint64_t ) 0,
				&ref_thread );

			if ( !ref_thread )
				return;

			thread_basic_info_t ref_tbi {}, worker_tbi {};
			g_syscall->call_kernel( m_nt_query_info_thread,
				ref_thread, ( std::uint64_t ) 0,
				&ref_tbi, ( std::uint64_t ) sizeof( ref_tbi ),
				( void* )nullptr );
			g_syscall->call_kernel( m_nt_query_info_thread,
				worker_handle, ( std::uint64_t ) 0,
				&worker_tbi, ( std::uint64_t ) sizeof( worker_tbi ),
				( void* )nullptr );
			g_syscall->call_kernel( m_zw_close, ref_thread );

			if ( !ref_tbi.teb_base_address || !worker_tbi.teb_base_address )
				return;

			constexpr std::uint64_t k_tls_offset = 0x1480ull;
			constexpr std::size_t   k_tls_size = 64 * sizeof( std::uint64_t );
			std::uint8_t tls_buf[ k_tls_size ] {};
			if ( !memory::read_virtual( ref_tbi.teb_base_address + k_tls_offset,
				tls_buf, sizeof( tls_buf ) ) )
				return;
			memory::write_virtual( worker_tbi.teb_base_address + k_tls_offset,
				tls_buf, sizeof( tls_buf ) );

			std::uint64_t ext_tls = 0;
			if ( memory::read_virtual( ref_tbi.teb_base_address + 0x1788ull,
				&ext_tls, sizeof( ext_tls ) ) && ext_tls )
				memory::write_virtual( worker_tbi.teb_base_address + 0x1788ull,
					&ext_tls, sizeof( ext_tls ) );

			logging::print( obf( "caller: tls seeded ref_teb=0x%llx worker_teb=0x%llx" ),
				ref_tbi.teb_base_address, worker_tbi.teb_base_address );
		}

		void free_worker_regions( ) {
			if ( m_worker_region.valid( ) ) g_allocator->free_memory( m_worker_region );
			if ( m_ring_region.valid( ) ) g_allocator->free_memory( m_ring_region );
			m_worker_region = {};
			m_ring_region = {};
		}

		void start_watchdog( ) {
			m_watchdog_stop.store( false, std::memory_order_release );
			m_watchdog = std::thread( [ this ]( ) {
				while ( !m_watchdog_stop.load( std::memory_order_acquire ) ) {
					if ( !process::is_target_alive( ) ) {
						emergency_unstitch( );
						return;
					}
					std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
				}
				} );
		}

		void stop_watchdog( ) {
			m_watchdog_stop.store( true, std::memory_order_release );
			if ( m_watchdog.joinable( ) )
				m_watchdog.join( );
		}

		void emergency_unstitch( ) {
			std::unique_lock lock( m_mutex );

			if ( !m_worker_alive )
				return;

			m_worker_alive = false;
			m_initialized = false;
			m_worker_region = {};
			m_ring_region = {};
			m_worker_thread_handle = nullptr;

			logging::print( obf( "worker: released thread" ) );
		}

		void wait_for_pacer( ) {
			if ( !m_min_interval_ticks || !m_qpc_frequency.QuadPart )
				return;

			LARGE_INTEGER now {};
			QueryPerformanceCounter( &now );

			const auto last = m_last_call_ticks;
			const auto elapsed = now.QuadPart - static_cast< std::int64_t >( last );

			if ( last && elapsed >= 0 &&
				static_cast< std::uint64_t >( elapsed ) < m_min_interval_ticks )
			{
				const auto wait_ticks = m_min_interval_ticks - static_cast< std::uint64_t >( elapsed );
				const auto wait_us = ( wait_ticks * 1'000'000ULL ) / m_qpc_frequency.QuadPart;
				if ( wait_us )
					std::this_thread::sleep_for( std::chrono::microseconds( wait_us ) );
			}

			QueryPerformanceCounter( &now );
			m_last_call_ticks = static_cast< std::uint64_t >( now.QuadPart );
		}

		HANDLE try_duplicate_existing( ) {
			if ( !m_zw_dup || !m_stub_region.valid( ) || handle_scan::m_results.empty( ) )
				return nullptr;

			for ( const auto& entry : handle_scan::m_results ) {
				open_process_ctx_t owner_ctx {};
				owner_ctx.m_ps_lookup_process = m_ps_lookup_process_va;
				owner_ctx.m_ob_open_object    = m_ob_open_object_va;
				owner_ctx.m_ob_deref_object   = m_ob_deref_object_va;
				owner_ctx.m_ps_process_type   = m_ps_process_type_va;
				owner_ctx.m_process_id        = entry.m_pid;
				owner_ctx.m_access            = PROCESS_DUP_HANDLE;

				g_kalloc->set_protection( m_stub_region, allocation::e_protection::read_execute );
				g_syscall->call_kernel( m_stub_va + k_open_process_stub_off, &owner_ctx );
				g_kalloc->set_protection( m_stub_region, m_stub_region.m_section_prot );

				const auto owner_handle = reinterpret_cast<HANDLE>( owner_ctx.m_out_handle );
				if ( !owner_handle )
					continue;

				HANDLE dup_handle = nullptr;
				g_syscall->call_kernel(
					m_zw_dup,
					static_cast<std::uint64_t>( reinterpret_cast<std::uintptr_t>( owner_handle ) ),
					entry.m_handle,
					static_cast<std::uint64_t>( ~0ull ),
					reinterpret_cast<std::uint64_t>( &dup_handle ),
					static_cast<std::uint64_t>( 0 ),
					static_cast<std::uint64_t>( 0 ),
					static_cast<std::uint64_t>( 2 ) );

				g_syscall->call_kernel( m_zw_close, owner_handle );

				if ( dup_handle ) {
					logging::print( obf( "handle_scan: duplicated handle from pid %llu (km=%d)" ),
						entry.m_pid, ( int )entry.m_is_kernel );
					return dup_handle;
				}
			}

			return nullptr;
		}

		HANDLE open_target_process( void*& out_process ) {
			out_process = nullptr;

			const auto dup = try_duplicate_existing( );
			if ( dup )
				return dup;

			open_process_ctx_t ctx {};
			ctx.m_ps_lookup_process = m_ps_lookup_process_va;
			ctx.m_ob_open_object    = m_ob_open_object_va;
			ctx.m_ob_deref_object   = m_ob_deref_object_va;
			ctx.m_ps_process_type   = m_ps_process_type_va;
			ctx.m_process_id        = static_cast< std::uint64_t >( m_process_id );
			ctx.m_access            = k_process_all_access;

			g_kalloc->set_protection( m_stub_region, allocation::e_protection::read_execute );
			g_syscall->call_kernel( m_stub_va + k_open_process_stub_off, &ctx );
			g_kalloc->set_protection( m_stub_region, m_stub_region.m_section_prot );

			return reinterpret_cast< HANDLE >( ctx.m_out_handle );
		}

		void close_target_process( HANDLE handle, void* process ) {
			( void ) process;
			if ( handle )
				g_syscall->call_kernel( m_zw_close, handle );
		}

		std::uint64_t find_user_gadget( std::uint64_t process_peb ) {
			if ( !process_peb )
				return 0;

			peb64_t peb {};
			if ( !memory::read_virtual( process_peb, &peb, sizeof( peb ) ) || !peb.m_ldr )
				return 0;

			peb_ldr_data64_t ldr {};
			if ( !memory::read_virtual( peb.m_ldr, &ldr, sizeof( ldr ) ) )
				return 0;

			const auto list_head = peb.m_ldr +
				offsetof( peb_ldr_data64_t, m_in_load_order_module_list );
			auto current = ldr.m_in_load_order_module_list.m_flink;

			std::vector< std::uint8_t > page( 0x1000 );

			for ( std::size_t n = 0; current && current != list_head && n < 0x200; ++n ) {
				ldr_data_table_entry64_t entry {};
				if ( !memory::read_virtual( current, &entry, sizeof( entry ) ) )
					break;

				const auto base = entry.m_dll_base;
				const auto size = entry.m_size_of_image;

				if ( base && size >= 0x1000 && size <= 0x800000 ) {
					for ( std::uint64_t off = 0; off + 1 < size; off += 0x1000 ) {
						std::size_t chunk = 0x1000;
						if ( size - off < chunk )
							chunk = static_cast< std::size_t >( size - off );
						if ( !memory::read_virtual( base + off, page.data( ), chunk ) )
							continue;

						for ( std::size_t i = 0; i + 1 < chunk; ++i ) {
							if ( page[ i ] == 0xFF && page[ i + 1 ] == 0xE1 )
								return base + off + i;
						}
						for ( std::size_t i = 0; i + 1 < chunk; ++i ) {
							if ( page[ i ] == 0x51 && page[ i + 1 ] == 0xC3 )
								return base + off + i;
						}
					}
				}

				const auto next = entry.m_in_load_order_links.m_flink;
				if ( next == current ) break;
				current = next;
			}

			return 0;
		}

		bool                                m_initialized { false };
		bool                                m_worker_alive { false };
		std::uint32_t                       m_process_id {};
		std::uint32_t                       m_seq_bypass_counter { 0 };
		std::uint64_t                       m_rtl_exit_thread_va {};
		std::uint64_t                       m_sleep_va {};
		std::uint64_t                       m_zw_open_process {};
		std::uint64_t                       m_rtl_create_thread {};
		std::uint64_t                       m_zw_close {};
		std::uint64_t                       m_zw_dup {};
		std::uint64_t                       m_nt_get_next_thread {};
		std::uint64_t                       m_nt_query_info_thread {};
		std::uint64_t                       m_ps_lookup_process_va {};
		std::uint64_t                       m_ob_open_object_va {};
		std::uint64_t                       m_ob_deref_object_va {};
		std::uint64_t                       m_ps_process_type_va {};
		std::uint64_t                       m_gadget_va {};
		std::uint64_t                       m_stub_va {};
		allocation::large_allocation_t      m_stub_region {};
		process::allocated_region_t         m_ring_region {};
		process::allocated_region_t         m_worker_region {};
		HANDLE                              m_worker_thread_handle { nullptr };
		std::atomic< std::uint64_t >        m_call_successes { 0 };
		std::atomic< std::uint64_t >        m_call_failures { 0 };
		std::uint64_t                       m_min_interval_ticks { 0 };
		std::uint64_t                       m_last_call_ticks { 0 };
		LARGE_INTEGER                       m_qpc_frequency {};
		std::mutex                          m_mutex {};
		std::thread                         m_watchdog {};
		std::atomic< bool >                 m_watchdog_stop { false };
		allocation::large_allocation_t      m_ob_region {};
		std::uint64_t                       m_ob_entry_va {};
		std::uint64_t                       m_ob_list_head_va {};
		bool                               m_ob_registered { false };
	};
}