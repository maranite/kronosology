// SPDX-License-Identifier: GPL-2.0
/*
 * bar2_stubs_c.cpp -- Bar 2 (2026-07-02): see bar2_stubs.cpp's own
 * file header for the full rationale. Plain `extern "C"` functions and
 * data objects only -- C linkage means these can be declared here
 * with matching signatures without any of the class-redefinition
 * conflicts the C++ method stubs hit (bar2_stubs.cpp/
 * bar2_stubs_auth.cpp).
 *
 * The `rtwrap_*`/`stg_*`/`cm_*`/`nv2ac_*` families are confirmed real
 * OA-internal wrapper functions (over genuine RTAI primitives and the
 * AT88 auth chip respectively, sec 10.39/AT88VirtualChip's own
 * README) -- their REAL bodies are a separate, much larger
 * reconstruction task. Stubbed here with deliberately safe/inert
 * bodies (matching this file's own stated Bar-2-only scope) so OA.ko
 * can link and be insmod-tested; NOT a claim these wrappers are
 * reconstructed.
 */

#include "oa_atmel.h"
#include "oa_cpu_affinity.h"
#include "oa_comport.h"
#include "oa_crypto.h"

/* ---- CSTGFile_* (file I/O primitives, already declared locally in
 * several callers -- redeclared here with matching signatures) ----
 * The ENTIRE CSTGFile_* cluster is now real: Open/Close/Seek/
 * GetFileSize (src/init/file_io.cpp, sec 10.180), and Read/Write/
 * FileExists/ReadFileIntoNewBuffer/FreeReadBuffer (same file, sec
 * 10.181). Nothing left to stub here. */

/* ---- Misc hardware/init C functions, confirmed real (own bodies not
 * reconstructed) ---- */
extern "C" void *CSTGSharedMemory_CreateMidiShareHeader() { return 0; }
/* GetInstalledRAM/IncProgressBar/SetInstalledOptions/init_cpp_support
 * promoted to real bodies in src/init/startup_helpers.cpp (sec 10.179).
 * CSTGDrumPadInterface_Initialize/_Cleanup promoted to real bodies in
 * src/init/drumpad_init.cpp (batch 38). CSTGKeybedKeyDebounceFilter_
 * Initialize promoted to a real body in src/init/keybed_debounce.cpp
 * (batch 38) -- its OWN real signature takes an `unsigned char *filter`
 * arg (already correctly declared in oa_keybed_init.h/used at its call
 * site; only THIS file's own stub had silently dropped the arg).
 * SCalibrationData_LoadCalibrationFile promoted to a real body in
 * src/init/calibration_data.cpp (batch 38) -- SIGNATURE FIXED at the
 * same time from a bogus no-arg guess to `(unsigned char *panel)`,
 * see that file's own header comment. */

/* ---- Startup/daemon lifecycle helpers, confirmed real (init_module's
 * own confirmed call chain, sec 10.17 -- own bodies not reconstructed) ---- */
/* cleanup_cpp_support @0x118... (ground truth OA.ko, 57 bytes) -- DEFERRED,
 * fully disassembled (batch 34 scout) so the next attempt need not re-derive:
 *
 *     push %ebp; mov %esp,%ebp; and $-16,%esp; push %ebx; sub $0xc,%esp
 *     mov  .dtors+4, %eax          ; R_386_32 .dtors  -> eax = .dtors[1]
 *     test %eax,%eax ; je  L_exit  ; empty list -> straight to stg_cpp_exit
 *     mov  $.dtors+4, %ebx         ; R_386_32 .dtors  -> ebx = &.dtors[1]
 *   L_loop:
 *     lea  0x4(%ebx),%ebx          ; advance to next slot
 *     call *%eax                   ; call this destructor
 *     mov  (%ebx),%eax ; test %eax,%eax ; jne L_loop
 *   L_exit:
 *     call stg_cpp_exit            ; R_386_PC32 (1-byte `ret` no-op)
 *     epilogue; ret
 *
 * i.e. it walks the module's `.dtors` array from entry [1], calling each
 * function pointer until it hits the 0 terminator, then calls stg_cpp_exit.
 * The fini-side mirror of init_cpp_support (sec 10.179, a 1-byte `ret`).
 *
 * WHY STILL DEFERRED: the body references the `.dtors` SECTION symbol
 * directly (`R_386_32 .dtors`), a linker construct. Faithfully binding a
 * portable-C body to `.dtors` start in THIS freestanding kernel-module
 * build (no crtstuff, so no `__DTOR_LIST__`) needs either a module linker-
 * script boundary symbol or an in-section anchor -- the latter would ADD a
 * `.dtors` entry and change semantics. It is the UNLOAD path (not needed to
 * get OA.ko running), so promoting it correctly is a dedicated task, not a
 * smallest-first stub. stg_cpp_exit itself is a trivial confirmed no-op
 * (1-byte ret) but is only reachable through this function, so it rides
 * along whenever cleanup_cpp_support is promoted. */
extern "C" void cleanup_cpp_support() {}
/*
 * setup_stg_daemons/cleanup_stg_daemons/setup_stg_decrypt_daemons
 * (batch 38 scout, kept DEFERRED -- not just "not yet gotten to"):
 * disassembled setup_stg_decrypt_daemons (.init.text+0x43a, 285 bytes)
 * in full. It is NOT a simple populate-gStgDaemons[] loop (that struct,
 * sec 10.183, is the UNRELATED 7-entry/0x60-stride watchdog array) --
 * it pre-zeroes two fields (+0x10=0, +0x24=-1) across FOUR separate
 * 0x94-stride (148-byte) per-daemon control-block instances at a
 * DIFFERENT .bss base (0x107560), then calls a shared internal helper
 * (ground truth: `SetupDaemon`/its `.clone.0` variant, own real body
 * NOT yet reconstructed) four times with (index, name-string-ptr,
 * blockBase, priority, MainRoutine-function-pointer) for
 * Decrypt0..3MainRoutine, and on any failure calls
 * `cleanup_stg_decrypt_daemons` (a FOURTH not-yet-reconstructed
 * function). setup_stg_daemons (.init.text, 206 bytes) almost
 * certainly shares the same `SetupDaemon` helper for its own 7(?)
 * daemons. Real blocker: `SetupDaemon` itself is unreconstructed AND
 * depends on `rtwrap_pthread_create` actually working (still a stub,
 * `bar2_stubs_c.cpp` above -- returns NULL unconditionally), so even a
 * faithful `SetupDaemon` would be inert without that. This is a
 * multi-function (SetupDaemon + cleanup_stg_decrypt_daemons +
 * rtwrap_pthread_create's own real thread-trampoline target,
 * .text+0x118e80, already separately flagged in rtwrap.cpp) + new
 * 0x94-stride struct cluster -- correctly out of scope for a
 * smallest-first pass; a future batch should start with
 * `rtwrap_pthread_create` (already scoped in rtwrap.cpp's own header)
 * since every one of these daemon-lifecycle functions is downstream of
 * it working for real. */
extern "C" int setup_stg_daemons() { return 0; }
extern "C" void cleanup_stg_daemons() {}
extern "C" int setup_stg_decrypt_daemons() { return 0; }
/* signal_timed_out_daemons + its tick source GetSTGTickCount promoted to
 * real bodies in src/init/stg_daemons.cpp + src/engine/tick_count.cpp
 * (batch 35, sec 10.183). */
/* stg_log_startup_error + its guard stg_is_linux_context promoted to real
 * bodies in src/init/startup_helpers.cpp (batch 34, sec 10.182). */
/*
 * load_global_resources (batch 38 scout, kept DEFERRED): disassembled
 * in full (.text+0x1188b0, 115 bytes). Genuinely small itself, but
 * fans out into THREE not-yet-reconstructed classes in its very first
 * few calls -- `CSTGMultisampleBankManager::StartupInitializeROMBank`/
 * `StartupInitializeRAMBank`/`ScanFileSystem`, `CSTGKLMManager::
 * AuthorizeBuiltins`, `CSTGInstalledEXProducts::Initialize` -- plus
 * `CSTGHeapManager_SetLastFixedBlock` and a `gSystemIsInitialized`
 * global. Disproportionate structural cost for a smallest-first pick;
 * needs a dedicated future batch scoping those three classes first. */
extern "C" int load_global_resources() { return 0; }

/* ---- AT88 auth-chip wrapper layer (cm_ and nv2ac_ prefixed
 * functions, confirmed real OA-internal wrappers over
 * stgNV2AC_sync_cmd/stgNV2AC_sync_read_cmd, exported by
 * AT88VirtualChip.ko -- own bodies not reconstructed) ----
 * cm_GetRandomBytes/cm_SetChallengeParams promoted to real bodies in
 * src/auth/atmel_primitives.cpp (batch 38, both small -- 15/18 bytes,
 * pure forwarder + pointer-cache respectively). cm_AuthenEncryptMAC
 * (real name `fFfFfFfFfFfF11`, .text+0x4f4210, 1575 bytes) SCOUTED and
 * kept DEFERRED: a genuine bit/byte-level cipher-MAC engine, calling a
 * shared helper (`bzzzzzzzzzzzt12`, .text+0x4f3d00) roughly once per
 * input byte across three separate buffers (c1/kin/iv) -- a real crypto
 * core, not a forwarder, disproportionate for a smallest-first pick;
 * needs a dedicated future batch (start by reconstructing
 * `bzzzzzzzzzzzt12` first, then walk fFfFfFfFfFfF11's full byte-loop
 * structure against it). */
void cm_AuthenEncryptMAC(const unsigned char *, const unsigned char *, const unsigned char *, unsigned char *, unsigned char *) {}
int cm_ComputeChallenge(const unsigned char *, int, unsigned char *) { return -1; }
int cm_ReadUserZone(int, int, unsigned char *) { return -1; }
int cm_SetUserZone(int) { return -1; }
int nv2ac_dispatch_cmd(void) { return -1; }
int nv2ac_enable_cipher(unsigned char, const unsigned char *, const unsigned char *) { return -1; }
int nv2ac_enable_encrypt(unsigned char, const unsigned char *, const unsigned char *) { return -1; }

/* ---- rtwrap_* RTAI wrapper layer -- signatures matching this
 * project's own already-established declarations (oa_cpu_affinity.h/
 * oa_comport.h) or, where undeclared elsewhere, a reasonable regparm-
 * safe signature ----
 *
 * The 22 bare-`{}` forwarders that used to live here (rtwrap_free,
 * the pthread_mutex_* / mutexattr_* / cond_init / attr_* families,
 * rtwrap_whoami/task_suspend, rtwrap_pthread_cancel, and the irq
 * quartet) are now real bodies in src/init/rtwrap.cpp (batch 37) --
 * see that file's own header comment for the full ground-truth
 * derivation. Still deferred here: rtwrap_pthread_create (needs the
 * `.text+0x118e80` internal thread-trampoline target reconstructed
 * too, a larger task) and rtwrap_request_irq/rtwrap_set_debug_traps_
 * in_rt_task (both already have safe non-bare-`{}` stub bodies below,
 * not part of the "smallest remaining" bare-`{}` tally). */
extern "C" unsigned int get_sizeof_rtwrap_pthread_attr(void) { return 64; }
extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void) { return 24; }
extern "C" unsigned int get_sizeof_rtwrap_pthread_cond(void) { return 24; }
extern "C" int get_pthread_recursive_attr_constant(void) { return 1; }
extern "C" void *rtwrap_malloc(unsigned int) { return 0; }
extern "C" void *rtwrap_pthread_create(void *, void *, void *(*)(void *), void *) { return 0; }
extern "C" int rtwrap_set_debug_traps_in_rt_task(void *) { return -1; }
extern "C" int rtwrap_request_irq(unsigned int, void (*)(unsigned int, void *), void *, unsigned int) { return -1; }

/* ---- Low-level stg_* RTAI/CPU primitives (confirmed real, own bodies
 * not reconstructed) ---- */
extern "C" int cpu_features_ok() { return 1; }
extern "C" unsigned long long stg_rdtsc() { return 0; }
/*
 * FIXED (2026-07-03, real Bar 2 boot-test bug): this MUST return the
 * real `current` task pointer, not NULL -- `init_module()`'s own
 * confirmed real disassembly immediately dereferences the result at
 * +0xbc (`current->cpus_allowed`), so a NULL/stubbed return here reads
 * from kernel address 0xbc, which corrupts unrelated nearby kernel
 * state rather than cleanly crashing at the read itself (the actual
 * user-visible symptom was a deterministic, reproducible "unable to
 * handle kernel paging request" Oops much later, inside module_put()
 * during this same insmod). Confirmed real semantics (already
 * documented in init_module.cpp's own comment): `mov ebx, fs:0x0` is
 * this kernel's own `current_task` per-CPU idiom -- replicated here
 * with inline asm rather than pulling in full kernel headers (this
 * file deliberately stays header-light, matching this project's own
 * convention elsewhere in bar2_stubs_c.cpp).
 */
extern "C" void *stg_get_current_task()
{
	void *current_task;
	asm volatile("mov %%fs:0x0, %0" : "=r"(current_task));
	return current_task;
}
/*
 * stg_set_fs/stg_restore_fs (sec 10.181): the per-CPU thread_info
 * addr_limit save+set / restore pair -- this kernel's own inlined
 * `set_fs(KERNEL_DS)`/`set_fs(old)` idiom, confirmed real at EVERY
 * CSTGFile_Read/Write/ReadFileIntoNewBuffer call site in
 * src/init/file_io.cpp as the identical `mov %esp,%reg; and
 * $0xffffe000,%reg; mov 0x18(%reg),saved; movl $0xffffffff,0x18(%reg)`
 * (set) / `mov saved,0x18(%reg)` (restore) instruction pairs (this
 * kernel's THREAD_SIZE is 8KB, so `esp & ~0x1fff` locates the
 * thread_info at the base of the current kernel stack; +0x18 is
 * `addr_limit`). Same host/target divergence pattern as
 * stg_get_current_task() just above: this is genuine, safe-to-compile
 * inline asm that IS the real target behavior when this file links
 * into the real .ko, but is not safely EXECUTABLE on a host test
 * process (the computed address is live host stack memory, not a
 * kernel thread_info) -- so file_io.cpp's own host KAT
 * (verify/test_file_io.cpp) supplies its own separate, safe mock of
 * both functions rather than linking this definition, exactly as
 * verify/test_init_module.cpp already does for stg_get_current_task().
 */
extern "C" unsigned long stg_set_fs(unsigned long newLimit)
{
	unsigned long threadInfo;
	asm volatile("mov %%esp, %0\n\tand $0xffffe000, %0" : "=r"(threadInfo));
	unsigned long old = *(unsigned long *)(threadInfo + 0x18);
	*(unsigned long *)(threadInfo + 0x18) = newLimit;
	return old;
}
extern "C" void stg_restore_fs(unsigned long oldLimit)
{
	unsigned long threadInfo;
	asm volatile("mov %%esp, %0\n\tand $0xffffe000, %0" : "=r"(threadInfo));
	*(unsigned long *)(threadInfo + 0x18) = oldLimit;
}
/*
 * stg_inb/stg_outb/stg_local_irq_save/stg_local_irq_restore (batch 38):
 * confirmed to have NO standalone symbol anywhere in ground truth (sec
 * 10.188's "no standalone symbol" pattern) -- every one of their MANY
 * real call sites (already reconstructed in comport.cpp/
 * comport_init.cpp) has the actual x86 `in`/`out`/`pushf;cli`/`popf`
 * instructions inlined directly. Confirmed via a direct disassembly of
 * one already-reconstructed real caller (`CSTGComPort::Initialize()`,
 * .text+0xaa90): real `out dx,al`/`out 0x2e,al`/`out 0x4e,al` (0xEE/
 * 0xE6 opcodes) throughout, plus one real `pushf; cli; ...; popf`
 * sequence (.text+0xaccc..0xad06) matching this project's own
 * `stg_local_irq_save()`/`stg_local_irq_restore()` factoring exactly
 * (both halves of that single straight-line real sequence, just
 * modeled as two reusable calls instead of duplicated inline at every
 * one of comport.cpp/comport_init.cpp's many use sites). These four
 * are therefore not project-specific behavior to reverse-engineer --
 * they're the universal, unambiguous x86 port-I/O/IRQ-flag primitives
 * the Linux kernel's own `inb`/`outb`/`local_irq_save`/
 * `local_irq_restore` macros expand to on this arch. Same host/target
 * divergence as stg_get_current_task()/stg_set_fs() above: genuine,
 * correct inline asm for the real -m32 kernel-module target, NOT
 * safely executable in an unprivileged host test process (IN/OUT fault
 * without IOPL) -- so every host KAT that reaches CSTGComPort code
 * supplies its own safe mock instead of linking this definition
 * (already the case for every existing comport.cpp/comport_init.cpp
 * verify file, unchanged by this promotion).
 */
extern "C" unsigned char stg_inb(unsigned int port)
{
	unsigned char value;
	asm volatile("inb %w1, %b0" : "=a"(value) : "Nd"(port));
	return value;
}
extern "C" void stg_outb(unsigned int port, unsigned char value)
{
	asm volatile("outb %b0, %w1" : : "a"(value), "Nd"(port));
}
extern "C" unsigned long stg_local_irq_save()
{
	unsigned long flags;
	asm volatile("pushfl\n\tcli\n\tpopl %0" : "=r"(flags) :: "memory");
	return flags;
}
extern "C" void stg_local_irq_restore(unsigned long flags)
{
	asm volatile("pushl %0\n\tpopfl" :: "r"(flags) : "memory", "cc");
}
extern "C" unsigned int stg_num_online_cpus() { return 1; }
extern "C" unsigned int stg_get_cpu_khz() { return 1000000; }
extern "C" unsigned int stg_cpumask_of_cpu(unsigned int cpu) { return 1u << cpu; }
/*
 * stg_set_cpus_allowed: confirmed a GENUINE `U` (undefined external) in
 * ground truth OA.ko itself (batch 37 flagged this as a pre-existing
 * bug, fixed here) -- this file used to WRONGLY define a local no-op
 * body for it (making our OA.ko's own symbol a defined `T`, unlike
 * ground truth's `U`). Removed; declared (with the correct 2-arg
 * signature already used at every real call site) in oa_init.h, left
 * genuinely undefined here so it resolves at real insmod time exactly
 * like ground truth expects, same treatment as any other confirmed-`U`
 * external (filp_open/vmalloc/rt_pend_linux_srq/etc).
 */

/* ---- fFfFfFfFfFfF13 (AT88 zone-read wrapper, sec 10.x -- already
 * declared in oa_crypto.h, defined here) ---- */
extern "C" int fFfFfFfFfFfF13(unsigned int, unsigned int, unsigned char *) { return -1; }

/* ---- operator delete(void*, unsigned int) (sized deallocation
 * overload -- new_delete.cpp already defines the unsized form) ---- */
void operator delete(void *ptr, unsigned int) { extern void operator delete(void *); operator delete(ptr); }

/* ---- Data objects -- types matching each symbol's own ALREADY-
 * ESTABLISHED `extern "C"` declaration in its real caller (cdrom_check.cpp/
 * oa_cmd_proc.cpp/products.cpp/parse_auth.cpp/init_module.cpp), not
 * guessed independently. */
void *sCdromCommand;
void *sXCmd;
int sOACmdResult;
int sOACmdStatus;
void *PcmModuleMutex;
unsigned int kAudXBZD;
static const char kEmptyString[1] = { 0 };
const char *kAuthFileName = kEmptyString;
const char *kOptionsPath = kEmptyString;
extern "C" float allMinusOne[4];
float allMinusOne[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
extern "C" float allPlusOne[4];
float allPlusOne[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
int gModuleParam10;
int gModuleParam14;
int gModuleParam18;

/* STGAPILR2IndivToPhysBusId's own real content is now homed directly in
 * managers.cpp (sec 10.132), alongside its own real reader/writer. */
float gAllPlusHeadroom[4];
float gAllMinusHeadroom[4];
extern "C" unsigned char STGVJSAssignInfo[489];
unsigned char STGVJSAssignInfo[489];
