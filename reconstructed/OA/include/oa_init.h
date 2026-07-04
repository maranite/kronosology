// SPDX-License-Identifier: GPL-2.0
/*
 * oa_init.h  -  init_module(): OA.ko's real insmod-time entry point.
 *
 * Ground-truthed offset: `.init.text+0x0`, 847 bytes exactly (ends at
 * `.init.text+0x34e`, immediately followed by unrelated `__init`-only
 * helpers `our_fifo_setup`/`stg_rtfifo_init`/`setup_stg_decrypt_daemons`/
 * `setup_stg_daemons`, which are separate functions colocated in the same
 * section, not part of `init_module` itself).
 *
 * This is a COMPLETELY SEPARATE function from `CSTGEngine::Initialize()`
 * (see oa_engine.h) and was never examined by this project before
 * MASTER_REFERENCE.md sec 10.17 -- `init_module` runs first, at `insmod`
 * time, and is what actually determines whether `insmod OA.ko` succeeds
 * or fails. `CSTGEngine::Initialize()` runs later, from inside one of
 * this function's own steps.
 *
 * Real structure (confirmed via a complete `.rel.init.text` relocation
 * resolution + full disassembly trace, not the summarized table alone):
 * a linear "call subsystem init -> check result -> hard-fail via a
 * partial-unwind cascade entering at the right depth, or continue" chain.
 * See init_module.cpp for the exact steps and a documented correction to
 * one detail in the MASTER_REFERENCE.md sec 10.17 summary table (the CPU
 * feature probe is NOT purely soft -- it has a real hard-fail path too).
 *
 * Every callee below is either already reconstructed elsewhere in this
 * project (see comments) or declared here as a confirmed-real,
 * not-yet-implemented extern -- same treatment as `CSTGEngine::
 * Initialize()`'s own manager singletons before Stage 3 existed. NOT
 * guessed at; each is confirmed to exist via `.rel.init.text`'s own
 * relocation entries, just not yet given a real C++ body.
 */

#ifndef OA_INIT_H
#define OA_INIT_H

extern "C" {

/*
 * Step 1 (can't fail). Every C++ freestanding kernel module in this
 * project declares its own copy of this pair (confirmed real symbols,
 * void-returning) rather than sharing an implementation -- same pattern
 * as `OmapNKS4Module/main.cpp`'s own declarations. Not implemented here;
 * running the real static-constructor array is its own bounded unit of
 * work, out of scope for the init_module skeleton itself.
 */
void init_cpp_support(void);
void cleanup_cpp_support(void);

/* Step 3. Already reconstructed (STGEnabler/STGEnabler.c, real
 * signatures confirmed there, EXPORT_SYMBOL'd -- real first parameter is
 * `struct task_struct *`; represented here as `void *` since init_module
 * never dereferences it, only passes the current-task pointer through,
 * matching this project's established host-build treatment of opaque
 * real-kernel types it doesn't need the real header for). */
unsigned long stg_cpumask_of_cpu(unsigned int cpu);
int stg_set_cpus_allowed(void *p, unsigned long mask);

/* Step 9 (hard-fail). Already reconstructed -- src/auth/atmel_setup.cpp,
 * declared in oa_atmel.h, included by init_module.cpp. */

/* Step 5 (hard-fail). Already reconstructed -- src/init/stgheap_init.cpp,
 * declared in oa_stgheap_init.h, included by init_module.cpp. */

/* Step 6 (hard-fail). Already reconstructed -- src/init/shmemproc_init.cpp,
 * declared in oa_shmemproc_init.h, included by init_module.cpp. Creates
 * /proc/.shm (CORRECTED from an earlier guess of /proc/.oacmd_shmem --
 * the real name was extracted directly from .rodata, not inferred). */

/* Step 7 (hard-fail). CONFIRMED (2026-07-02, via the real proc entry
 * name extracted from .rodata: ".oacmd") to be exactly the already-
 * reconstructed /proc/.oacmd registration -- src/auth/oa_cmd_proc.cpp,
 * declared in oa_cmd_proc.h, included by init_module.cpp. Was already
 * fully reconstructed from Stage 1, but its own header was missing an
 * `extern "C"` wrapper, so it compiled under a mangled C++ symbol name
 * that never matched this file's own (correct) extern "C" declaration
 * -- fixed in oa_cmd_proc.h itself, not by rewriting anything here. */

/* Step 8 (hard-fail). Allocates CSTGGlobal + the manager singletons
 * (oa_global.h / oa_engine.h) -- overlaps heavily with Stage 3's
 * in-progress, not-yet-complete work (CSTGGlobal's own constructor is
 * unimplemented; 21/~40 manager constructors done). Takes one argument,
 * confirmed to be the same raw value this function itself reads from an
 * unresolved absolute global at `.init.text`'s `ds:0x10` (likely a module
 * parameter; exact symbol name not resolved in this pass). */
int setup_global_resources(int param);
void cleanup_global_resources(void);

/* Step 10 (hard-fail). Confirmed to exist; its own body is a SEPARATE
 * `__init`-only function also in `.init.text` (`setup_stg_decrypt_daemons`,
 * `.init.text+0x43a`) that clones several named daemon threads
 * (Decrypt0-3MainRoutine, StreamingReadMainRoutine, FileOpenMainRoutine,
 * FileReadMainRoutine, FileWriteMainRoutine, SamplingMainRoutine,
 * CDAudioMainRoutine, FileCloseMainRoutine per the surrounding relocations)
 * -- not reconstructed in this pass, left as a confirmed-real extern. */
int setup_stg_decrypt_daemons(void);
void cleanup_stg_decrypt_daemons(void);

/* Step 11 (hard-fail). Confirmed to exist; not yet disassembled. */
int load_global_resources(void);

/* Step 12 (hard-fail). Own body also colocated in `.init.text`
 * (`setup_stg_daemons`, `.init.text+0x557`) -- clones several more named
 * daemons (each call site loads a 4-byte ASCII tag into `ecx` as part of
 * its thread-naming args; not decoded to exact names in this pass) --
 * not reconstructed here. */
int setup_stg_daemons(void);
void cleanup_stg_daemons(void);

/* Step 13 (hard-fail). Already reconstructed -- src/init/audio_start.cpp,
 * declared in oa_audio_start.h, included by init_module.cpp. The real
 * gate is a device-list-length-dependent loop of `CSTGThread::
 * CreateRealTimeWithCPUAffinity()` calls (one per real-time audio
 * thread) plus 2 fixed ones; the KorgUsbAudioDriver.ko surface (sec
 * 10.36) is called at the very end but its return value is discarded
 * unconditionally -- confirmed via full disassembly of the real
 * `CSTGAudioManager::StartAudioEngine()` body, see MASTER_REFERENCE.md
 * sec 10.50. NOTE THE INVERTED SUCCESS CONVENTION confirmed via
 * disassembly: this function returns NONZERO for success, ZERO for
 * failure -- the opposite of every other step function in this file. */

/* Step 14 (hard-fail). Already reconstructed -- src/init/keybed_init.cpp,
 * declared in oa_keybed_init.h, included by init_module.cpp. Confirmed
 * (via full disassembly + relocation resolution) to be OA.ko's OWN
 * serial-port keybed handshake -- NOT a call into OmapNKS4Module.ko's
 * C-ABI as originally assumed. CORRECTED (2026-07-02): scans only 6
 * `CSTGComPort::eComPortId` values (0-5), not "up to 7" as an earlier
 * pass's summary claimed -- the real loop-exit check (`cmp ebx,7`,
 * with `ebx` starting at 1 and incrementing per failure) computes
 * comPortId=6 but exits before ever calling Initialize() with it, a
 * genuine off-by-one in the real code (or an intentional 6-port
 * hardware limit), reproduced faithfully rather than "fixed". Calls
 * `CSTGComPort::Initialize()` (a 2561-byte real 8250/16550-style UART
 * driver, `.text+0xaa90`, NOT itself reconstructed) on each port,
 * sending a probe byte and busy-waiting (`__const_udelay`) for a real
 * keybed board to ACK. It will NOT succeed without a real (or
 * virtually-responding) serial keybed device -- see MASTER_REFERENCE.md
 * sec 10.36/10.40/10.49. SAME INVERTED SUCCESS CONVENTION as step 13
 * (nonzero = success). */

/* Step 15 (soft -- return value never checked). */
int CSTGDrumPadInterface_Initialize(void);
void CSTGDrumPadInterface_Cleanup(void);

/* Step 16 (hard-fail). Already reconstructed -- src/init/rtfifo_init.cpp,
 * declared in oa_rtfifo_init.h, included by init_module.cpp. Own body
 * also colocated in `.init.text` (`stg_rtfifo_init`, `.init.text+0x397`)
 * -- creates 6 RTAI FIFOs (minors 0,1,3,4,5,7, confirmed via each call's
 * distinct size argument: 0x400, 0x400, 0x400, 0x8000, 0x10000, 0x400)
 * via a small shared helper (`our_fifo_setup`, `.init.text+0x34f`,
 * wrapping `rtf_destroy`+`rtf_create`), then registers a char device
 * ("stg_direct", major 0x98) via `__register_chrdev`. Real RTAI infra,
 * matching this project's confirmed `rtai_fifos.ko` boot-time load
 * (sec 10.41). See MASTER_REFERENCE.md sec 10.51.
 * `stg_rtfifo_cleanup` itself (its own 381-byte body, .text+0x116ac0)
 * is NOT reconstructed here -- confirmed real, called by
 * stg_rtfifo_init's own failure path, left as an extern. */
void stg_rtfifo_cleanup(void);

/* Soft-path helpers seen in the disassembly, not yet characterized
 * beyond their call sites. */
void IncProgressBar(void);
void SetInstalledOptions(int code);
void stg_log_startup_error(int code);

} /* extern "C" */

/*
 * init_module()/cleanup_module() -- the real kernel module entry points.
 * See init_module.cpp for the faithful 17-step reconstruction.
 */
extern "C" int init_module(void);
extern "C" void cleanup_module(void);

#endif /* OA_INIT_H */
