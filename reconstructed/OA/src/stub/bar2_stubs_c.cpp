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
/* cleanup_cpp_support @0x118... (ground truth OA.ko, 57 bytes) -- RESOLVED
 * as a deliberate, documented no-op VIRTUAL SUBSTITUTE (batch 41, sec
 * 10.185 policy), not left as an open "still deferred" TODO any more.
 * Fully disassembled (batch 34 scout):
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
 * WHAT THIS REPLACES: OA_real.ko's own static-C++-destructor teardown at
 * module unload (registered via `-fno-use-cxa-atexit`'s `.dtors`
 * mechanism, since no crtstuff/`__DTOR_LIST__`/`__DTOR_END__` boundary
 * symbols are linked into a freestanding kernel module here or in ground
 * truth).
 *
 * WHY A FAITHFUL WALK CAN'T (USEFULLY) RUN HERE: binding portable C to a
 * `.dtors` SECTION symbol needs a linker-script boundary symbol GNU ld
 * does NOT auto-synthesize for a dot-prefixed section name like `.dtors`
 * (the `__start_SECNAME`/`__stop_SECNAME` auto-synthesis GNU ld provides
 * only fires for section names that are themselves valid C identifiers);
 * building one would mean adding a custom linker-script fragment to this
 * project's shared Kbuild -- real infra risk for a list that is currently
 * EMPTY ANYWAY: confirmed via `objdump -h OA.ko | grep dtors` / `readelf -x
 * .dtors OA.ko` on THIS project's own from-scratch build that no `.dtors`
 * section is emitted at all (zero global C++ objects in this
 * reconstruction currently have a non-trivial destructor needing
 * registration). A faithful walk over an empty list plus a call to a
 * confirmed no-op (`stg_cpp_exit`) is bit-for-bit indistinguishable from
 * this function's own current no-op body -- so the existing empty `{}` IS
 * the faithful behavior for this reconstruction's PRESENT state, not
 * merely a placeholder standing in for unfinished work.
 *
 * WHAT THIS SUBSTITUTE DELIBERATELY DOES NOT GUARANTEE: if a FUTURE batch
 * gives some class a real non-trivial destructor for a genuine global
 * static object, this build would then start emitting a non-empty
 * `.dtors` section of its own -- and this no-op would silently skip
 * running those destructors at `rmmod` time (a static-teardown gap, not a
 * boot/insmod-time hazard; kernel modules don't generally depend on C++
 * static destructors running for correctness, since the whole module's
 * memory is reclaimed on unload regardless). Revisit this decision at
 * that point rather than assuming it stays vacuously correct forever.
 * stg_cpp_exit itself stays an unimplemented confirmed no-op (1-byte
 * ret) -- not given its own extern here since nothing in this
 * reconstruction calls it (this substitute never reaches that call). */
extern "C" void cleanup_cpp_support() {}
/* setup_stg_daemons/cleanup_stg_daemons/setup_stg_decrypt_daemons/
 * cleanup_stg_decrypt_daemons -- the full daemon kernel-thread lifecycle,
 * promoted to real bodies (batch 40, src/init/daemon_lifecycle.cpp).
 * batch 39's own guess that a SINGLE shared `SetupDaemon.clone.0` helper
 * serves BOTH the general and decrypt daemon clusters was WRONG -- ground
 * truth actually has TWO separate helpers (`SetupDaemon.clone.0`, 7 args
 * + SRQ registration, and `SetupDecryptDaemon.clone.0`, 5 args, no SRQ);
 * see daemon_lifecycle.cpp's own header comment and include/oa_daemons.h
 * for the full derivation. */
/* signal_timed_out_daemons + its tick source GetSTGTickCount promoted to
 * real bodies in src/init/stg_daemons.cpp + src/engine/tick_count.cpp
 * (batch 35, sec 10.183). */
/* stg_log_startup_error + its guard stg_is_linux_context promoted to real
 * bodies in src/init/startup_helpers.cpp (batch 34, sec 10.182). */
/*
 * load_global_resources: real now, batch 52 -- see
 * src/init/load_global_resources.cpp. Reconstructed under the sec 10.203
 * init-path-priority directive (this function sits directly on
 * init_module()'s own step 11, so its earlier "kept DEFERRED, return 0"
 * stub -- while safe (no wild call) -- was exactly the kind of
 * init-path gap that directive now prioritizes closing). Its own genuine
 * filesystem/ROM-bank-scan callees (`CSTGMultisampleBankManager::
 * StartupInitializeROMBank`/`StartupInitializeRAMBank`/`ScanFileSystem`,
 * `CSTGInstalledEXProducts::Initialize`) are deliberately deferred
 * safe-default stubs there (oa_types.h); `CSTGKLMManager::
 * AuthorizeBuiltins` was ALREADY real (klm_manager.cpp) but never
 * actually wired up until this batch, since the old stub never called
 * it at all. */

/* ---- AT88 auth-chip wrapper layer (cm_ and nv2ac_ prefixed
 * functions, confirmed real OA-internal wrappers over
 * stgNV2AC_sync_cmd/stgNV2AC_sync_read_cmd, exported by
 * AT88VirtualChip.ko -- own bodies not reconstructed) ----
 * cm_GetRandomBytes/cm_SetChallengeParams promoted to real bodies in
 * src/auth/atmel_primitives.cpp (batch 38, both small -- 15/18 bytes,
 * pure forwarder + pointer-cache respectively). cm_AuthenEncryptMAC
 * (real name `fFfFfFfFfFfF11`, .text+0x4f4210, 1575 bytes) is now real
 * too (batch 43, src/auth/atmel_deax.cpp) -- its own shared per-byte
 * helper (`bzzzzzzzzzzzt12`, .text+0x4f3d00) turned out to already have
 * a hardware-validated reference implementation elsewhere in this
 * project (AT88VirtualChip/chip_state.cpp's own `deax_step()`/
 * `deax_compute_challenges()`, from the earlier AT88 hardware-extraction
 * phase) -- ported rather than re-derived; see atmel_deax.cpp's own
 * header comment for the full derivation and the two still-deferred
 * sibling functions (`fFfFfFfFfFfF13`/`fFfFfFfFfFfF1C`) this does NOT
 * cover. */
int cm_ComputeChallenge(const unsigned char *, int, unsigned char *) { return -1; }
/* cm_ReadUserZone is real now, batch 46 -- see src/auth/atmel_zone_io.cpp
 * (confirmed real ground-truth identity: fFfFfFfFfFfF1C). */
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
 * derivation. `rtwrap_pthread_create` is ALSO now a real body in
 * src/init/rtwrap.cpp (batch 39 -- promoting it also uncovered and
 * fixed a real return-value-polarity bug in cpu_affinity.cpp's own
 * `CreateRealTimeWithCPUAffinity()`, see that function's header
 * comment). Still deferred here: rtwrap_request_irq/rtwrap_set_debug_
 * traps_in_rt_task (both already have safe non-bare-`{}` stub bodies
 * below, not part of the "smallest remaining" bare-`{}` tally). */
extern "C" unsigned int get_sizeof_rtwrap_pthread_attr(void) { return 64; }
extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void) { return 24; }
extern "C" unsigned int get_sizeof_rtwrap_pthread_cond(void) { return 24; }
extern "C" int get_pthread_recursive_attr_constant(void) { return 1; }
extern "C" void *rtwrap_malloc(unsigned int) { return 0; }
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

/* fFfFfFfFfFfF13 is real now, batch 46 -- see
 * src/auth/atmel_zone_io.cpp (also its sibling fFfFfFfFfFfF1C, aliased
 * cm_ReadUserZone right above). */

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
