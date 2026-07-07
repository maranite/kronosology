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
 * several callers -- redeclared here with matching signatures) ---- */
extern "C" void *CSTGFile_Open(const char *, int) { return 0; }
extern "C" int CSTGFile_Seek(void *, int, int) { return -1; }
extern "C" int CSTGFile_Write(void *, const void *, unsigned int) { return -1; }
extern "C" int CSTGFile_Close(void *) { return 0; }
extern "C" unsigned int CSTGFile_GetFileSize(void *) { return 0; }
extern "C" int CSTGFile_Read(void *, void *, unsigned int) { return -1; }
extern "C" int CSTGFile_FileExists(const char *) { return 0; }
extern "C" unsigned char *CSTGFile_ReadFileIntoNewBuffer(const char *, unsigned int *outLen) { if (outLen) *outLen = 0; return 0; }
extern "C" void CSTGFile_FreeReadBuffer(unsigned char *) {}

/* ---- Misc hardware/init C functions, confirmed real (own bodies not
 * reconstructed) ---- */
extern "C" void CSTGDrumPadInterface_Initialize() {}
extern "C" void CSTGDrumPadInterface_Cleanup() {}
extern "C" void CSTGKeybedKeyDebounceFilter_Initialize() {}
extern "C" void *CSTGSharedMemory_CreateMidiShareHeader() { return 0; }
extern "C" void SCalibrationData_LoadCalibrationFile() {}
/* GetInstalledRAM/IncProgressBar/SetInstalledOptions/init_cpp_support
 * promoted to real bodies in src/init/startup_helpers.cpp (sec 10.179). */

/* ---- Startup/daemon lifecycle helpers, confirmed real (init_module's
 * own confirmed call chain, sec 10.17 -- own bodies not reconstructed) ---- */
extern "C" void cleanup_cpp_support() {}
extern "C" int setup_stg_daemons() { return 0; }
extern "C" void cleanup_stg_daemons() {}
extern "C" int setup_stg_decrypt_daemons() { return 0; }
extern "C" void signal_timed_out_daemons() {}
extern "C" void stg_log_startup_error(const char *) {}
extern "C" int load_global_resources() { return 0; }

/* ---- AT88 auth-chip wrapper layer (cm_ and nv2ac_ prefixed
 * functions, confirmed real OA-internal wrappers over
 * stgNV2AC_sync_cmd/stgNV2AC_sync_read_cmd, exported by
 * AT88VirtualChip.ko -- own bodies not reconstructed) ---- */
void cm_AuthenEncryptMAC(const unsigned char *, const unsigned char *, const unsigned char *, unsigned char *, unsigned char *) {}
int cm_ComputeChallenge(const unsigned char *, int, unsigned char *) { return -1; }
void cm_GetRandomBytes(unsigned char *, int) {}
int cm_ReadUserZone(int, int, unsigned char *) { return -1; }
void cm_SetChallengeParams(const char *, const char *, const char *) {}
int cm_SetUserZone(int) { return -1; }
int nv2ac_dispatch_cmd(void) { return -1; }
int nv2ac_enable_cipher(unsigned char, const unsigned char *, const unsigned char *) { return -1; }
int nv2ac_enable_encrypt(unsigned char, const unsigned char *, const unsigned char *) { return -1; }

/* ---- rtwrap_* RTAI wrapper layer -- signatures matching this
 * project's own already-established declarations (oa_cpu_affinity.h/
 * oa_comport.h) or, where undeclared elsewhere, a reasonable regparm-
 * safe signature ---- */
extern "C" unsigned int get_sizeof_rtwrap_pthread_attr(void) { return 64; }
extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void) { return 24; }
extern "C" unsigned int get_sizeof_rtwrap_pthread_cond(void) { return 24; }
extern "C" int get_pthread_recursive_attr_constant(void) { return 1; }
extern "C" void *rtwrap_malloc(unsigned int) { return 0; }
extern "C" void rtwrap_free(void *) {}
extern "C" void rtwrap_pthread_mutex_init(void *, void *) {}
extern "C" void rtwrap_pthread_mutex_destroy(void *) {}
/* rtwrap_pthread_mutex_lock/_unlock -- confirmed real (plain, unmangled
 * `T` symbols in OA_real.ko, same rtwrap_* family), newly needed by
 * sec 10.149's real CSTGVoiceAllocator::EmergencyFreeVoiceList(). */
extern "C" void rtwrap_pthread_mutex_lock(void *) {}
extern "C" void rtwrap_pthread_mutex_unlock(void *) {}
/* rtwrap_whoami/rtwrap_task_suspend -- confirmed real (plain, unmangled
 * `T` symbols), newly needed by sec 10.149's real
 * CSTGAudioManager::ASKThreadRoutine(void*) (src/init/audio_start.cpp). */
extern "C" void rtwrap_whoami(void) {}
extern "C" void rtwrap_task_suspend(void) {}
extern "C" void rtwrap_pthread_mutexattr_init(void *) {}
extern "C" void rtwrap_pthread_mutexattr_settype(void *, int) {}
extern "C" void rtwrap_pthread_mutexattr_destroy(void *) {}
extern "C" void rtwrap_pthread_cond_init(void *, void *) {}
extern "C" void rtwrap_pthread_attr_init(void *) {}
extern "C" void rtwrap_pthread_attr_setrtpriority(void *, int) {}
extern "C" void rtwrap_pthread_attr_setstacksize(void *, unsigned int) {}
extern "C" void *rtwrap_pthread_create(void *, void *, void *(*)(void *), void *) { return 0; }
extern "C" void rtwrap_pthread_attr_destroy(void *) {}
extern "C" int rtwrap_set_debug_traps_in_rt_task(void *) { return -1; }
extern "C" void rtwrap_set_runnable_on_cpuid(void *, unsigned int) {}
extern "C" void rtwrap_clear_debug_traps_in_rt_task(void *) {}
extern "C" void rtwrap_pthread_cancel(void *) {}
extern "C" void rtwrap_shutdown_irq(unsigned int) {}
extern "C" void rtwrap_release_irq(unsigned int) {}
extern "C" int rtwrap_request_irq(unsigned int, void (*)(unsigned int, void *), void *, unsigned int) { return -1; }
extern "C" void rtwrap_assign_irq_to_cpu(unsigned int, unsigned int) {}
extern "C" void rtwrap_startup_irq(unsigned int) {}

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
extern "C" unsigned char stg_inb(unsigned int) { return 0; }
extern "C" void stg_outb(unsigned int, unsigned char) {}
extern "C" unsigned long stg_local_irq_save() { return 0; }
extern "C" void stg_local_irq_restore(unsigned long) {}
extern "C" unsigned int stg_num_online_cpus() { return 1; }
extern "C" unsigned int stg_get_cpu_khz() { return 1000000; }
extern "C" unsigned int stg_cpumask_of_cpu(unsigned int cpu) { return 1u << cpu; }
extern "C" void stg_set_cpus_allowed(void *, unsigned int) {}

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
