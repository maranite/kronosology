// SPDX-License-Identifier: GPL-2.0
/*
 * init_module.cpp  -  see include/oa_init.h.
 * Ground-truthed offset: `.init.text+0x0`, 847 bytes exactly.
 *
 * Reconstructed from a complete `.rel.init.text` relocation resolution and
 * full disassembly trace (not the MASTER_REFERENCE.md sec 10.17 summary
 * table alone -- that table is right about the overall shape but glosses
 * over one detail corrected here, see step 2 below).
 *
 * CORRECTION to MASTER_REFERENCE.md sec 10.17's table: step 2 (the CPU
 * feature probe) is NOT purely soft. The real disassembly (`.init.text
 * +0x2b`..`+0x83`) checks CPUID(eax=1)'s ecx bits 0 and 9 (commonly
 * SSE3/SSSE3, exact bit-to-feature mapping not independently verified in
 * this pass). If bit 9 is clear, OR bit 9 is set but bit 0 was clear, this
 * is a genuine HARD FAIL: `stg_log_startup_error(0xa1)` then jump straight
 * to the final cleanup tail (`cleanup_cpp_support` + return -1) -- no
 * other subsystem has been set up yet, so no cascade is needed. Only when
 * BOTH bits are set does execution continue to step 3. This is represented
 * faithfully below via `cpu_features_ok()`; the exact feature semantics
 * (which named CPU feature each bit corresponds to) are NOT asserted with
 * confidence -- flagged as a real, bounded uncertainty rather than
 * papered over.
 *
 * Two functions use an INVERTED success convention, confirmed via
 * disassembly (`jne` to the success path, not `je`): `CSTGAudioManager_
 * StartAudioEngine()` and `CSTGKeybedInterface_Startup()` return NONZERO
 * for success, ZERO for failure -- the opposite of every other step
 * function here (which return 0 for success). Represented explicitly
 * below, not normalized away, since normalizing it silently would make
 * this reconstruction diverge from the real ABI other callers might rely
 * on if either function is ever called from elsewhere.
 *
 * The failure-path unwind is a genuine partial cascade, confirmed via
 * disassembly: each hard-fail `goto` enters the cascade at the label
 * matching how far setup actually got (e.g. failing step 16 cleans up
 * the drum pad and keybed FIRST, then falls through steps 13/12/11/...
 * cleanup in order) -- not a single blanket teardown. Reconstructed here
 * with `goto`, deliberately mirroring the real fall-through-cascade shape
 * rather than hiding it behind nested early-returns.
 */

#include "oa_init.h"
#include "oa_atmel.h"
#include "oa_stgheap_init.h"
#include "oa_shmemproc_init.h"
#include "oa_cmd_proc.h"
#include "oa_keybed_init.h"
#include "oa_audio_start.h"
#include "oa_rtfifo_init.h"

/*
 * The real code reads/writes the current task pointer via raw per-CPU
 * segment addressing (`mov ebx, fs:0x0` then `[ebx+0xbc]` -- the Linux
 * 2.6.32 `current_task` per-CPU idiom). Raw segment-register access isn't
 * portably host-testable, so it's represented here as an opaque accessor,
 * matching this project's established treatment of other host/target
 * divergences (e.g. the 32/64-bit pointer-width fixes elsewhere in this
 * tree). The real target build would use inline asm or the kernel's own
 * `current` macro here instead.
 */
extern "C" void *stg_get_current_task(void);

/*
 * CPUID(eax=1) ecx bits 0 and 9 -- see the correction note above. Exposed
 * as an extern rather than inlined raw `cpuid`/`test` so this file stays
 * host-compilable; the real target build executes the instructions
 * directly at this call site.
 */
extern "C" bool cpu_features_ok(void);

/*
 * Step 4's optional leftover-PID file. Real path/format string not
 * resolved in this pass (anonymous .rodata pool, same as every other
 * unresolved format-string constant below) -- represented via a small
 * helper rather than guessed-at literal text. `CSTGFile_Open`/`Close`
 * signatures match the already-confirmed real ABI established in
 * src/auth/products.cpp (handle-based, `void *`, NOT an int fd) --
 * `CSTGFile_GetFileSize`/`CSTGFile_Read` follow the same handle
 * convention by inference from their siblings, not independently
 * disassembly-confirmed in this pass. */
extern "C" void *CSTGFile_Open(const char *path, int mode);
extern "C" unsigned int CSTGFile_GetFileSize(void *handle);
extern "C" int  CSTGFile_Read(void *handle, void *buf, unsigned int size);
extern "C" int  CSTGFile_Close(void *handle);
extern "C" int  kill_proc_info(int sig, void *info, int pid);
extern "C" int  sscanf(const char *buf, const char *fmt, ...);

/* Two confirmed module-parameter-shaped globals read by this function
 * (raw absolute addresses `ds:0x10`/`ds:0x14`/`ds:0x18` in the
 * disassembly; exact symbol names not resolved -- anonymous/local BSS,
 * not exported). `param10` is read twice: once to conditionally log at
 * the very start, and again as `setup_global_resources`'s own argument. */
extern "C" int gModuleParam10;
extern "C" int gModuleParam14;
extern "C" int gModuleParam18;

/*
 * CRITICAL FIX (2026-07-03, found via a real Bar 2 boot-test crash):
 * the real kernel's own printk/rt_printk are declared `asmlinkage`
 * (`__attribute__((regparm(0)))` on x86 -- confirmed directly in this
 * kernel's own linux/kernel.h), meaning they ALWAYS use the standard
 * stack-based cdecl calling convention regardless of this file's own
 * `-mregparm=3` Kbuild default. Without this attribute here, GCC
 * assumes printk/rt_printk ALSO follow this file's regparm(3)
 * convention and passes the first argument in EAX instead of on the
 * stack -- a genuine ABI mismatch corrupting every argument the real
 * printk/rt_printk receive. This was invisible in every host KAT (none
 * of them call the real kernel printk), only surfacing the first time
 * this code ever actually ran in a real kernel: a deterministic,
 * reproducible "unable to handle kernel paging request" Oops
 * appeared, with NO printk output at all preceding it (not even this
 * function's own very first line) -- exactly the signature of a
 * garbage/misinterpreted format-string pointer. sscanf is NOT
 * `asmlinkage` in this kernel (confirmed in the same header), so it
 * does not need this fix.
 */
/*
 * CORRECTED (2026-07-04, found while auditing this project's own
 * unresolved-symbol list): every printk/rt_printk call site below was
 * declared/called with the RAW `.rodata` OFFSET NUMBER standing in for
 * the format string (`printk(0x85, ...)` etc.) -- a placeholder for
 * "the real message text isn't resolved", per this file's own history
 * (MASTER_REFERENCE.md sec 10.17). But the real disassembly confirms
 * each of these call sites has a genuine `R_386_32 .rodata.str1.1`
 * relocation immediately before the call (e.g. `mov eax,0x85` with a
 * relocation making eax a REAL, valid, absolute `.rodata` string
 * address at link time, NOT a bare literal 0x85) -- so passing the
 * literal small integer AS the pointer, as this file previously did,
 * would make the real kernel's own printk implementation dereference
 * a near-NULL "string" the moment init_module() ever actually ran on
 * real hardware (this was never caught because OA.ko has never
 * successfully insmod'd far enough to invoke init_module() at all --
 * unlike the regparm(0) fix above, which WAS caught by a real Bar 2
 * boot Oops). Restored the real, correct `const char *fmt, ...`
 * signature and replaced every call site's literal offset with an
 * honest placeholder string literal (real text still not resolved,
 * but now a genuinely valid, non-crashing pointer) -- same treatment
 * as this file's own already-correct `sscanf` format string above.
 */
extern "C" __attribute__((regparm(0))) int printk(const char *fmt, ...);
extern "C" __attribute__((regparm(0))) void rt_printk(const char *fmt, ...);
extern "C" void __const_udelay(unsigned long xloops);
extern "C" void oa_debug_marker(int n); /* TEMPORARY, see debug_marker.cpp */
extern "C" unsigned long long stg_rdtsc(void);

int init_module(void)
{
	/* Declared (not yet initialized) before any `goto` so the C++
	 * "jump bypasses variable initialization" rule isn't tripped by the
	 * step-2 failure path below, which must skip past both the
	 * assignment AND the later restore call entirely -- matching the
	 * real disassembly exactly (see the step-2 branch's own comment). */
	void *current;
	unsigned long originalCpuMask;

	oa_debug_marker(1);
	init_cpp_support();				/* step 1 */

	if (gModuleParam10 != 0)
		printk("OA: gModuleParam10=%d\n" /* real text not resolved, offset 0x85 */, gModuleParam10);

	oa_debug_marker(2);
	if (!cpu_features_ok()) {			/* step 2 -- see correction note above */
		/* Confirmed via disassembly: this failure path jumps
		 * straight to `cleanup_cpp_support`, SKIPPING the CPU-
		 * affinity restore below entirely -- at this point the CPU
		 * hasn't been pinned yet (that's step 3, below), so there's
		 * nothing to restore and `current`/`originalCpuMask` haven't
		 * even been read yet. A distinct, shallower failure label
		 * from the rest of the cascade, not an oversight. */
		stg_log_startup_error(0xa1);
		goto fail_before_pin;
	}

	/* current = fs:0x0 (this kernel's `current` idiom -- current_task's
	 * own per-cpu offset is 0, so this direct read IS `current`, not a
	 * separate per-cpu-area base as first assumed); originalCpuMask =
	 * current->cpus_allowed at +0xbc, saved here and restored on every
	 * exit path from here on (success epilogue and every remaining
	 * unwind label alike). */
	oa_debug_marker(3);
	current = stg_get_current_task();
	originalCpuMask = *(unsigned long *)((unsigned char *)current + 0xbc);

	/* step 3: pin to CPU 0 (confirmed hardcoded cpu id, not derived from
	 * `current`) for the duration of init */
	stg_set_cpus_allowed(current, stg_cpumask_of_cpu(0));

	oa_debug_marker(4);
	{						/* step 4: soft, file-missing-tolerant */
		void *handle = CSTGFile_Open(0, 0);
		if (handle != 0) {
			unsigned int size = CSTGFile_GetFileSize(handle);
			char buf[0x60];
			CSTGFile_Read(handle, buf, size);
			int pid = 0;
			sscanf(buf, "%d" /* real format string not resolved in this pass */, &pid);
			kill_proc_info(9, (void *)1, pid);
			CSTGFile_Close(handle);
		}
	}

	oa_debug_marker(5);
	if (InitializeSTGHeap() != 0) {			/* step 5 */
		printk("OA: InitializeSTGHeap failed\n" /* real text not resolved, offset 0xbe */);
		stg_log_startup_error(0xd7);
		goto fail_early;
	}

	IncProgressBar();
	oa_debug_marker(6);
	if (InitSharedMemProcInterface() != 0) {	/* step 6 */
		printk("OA: InitSharedMemProcInterface failed\n" /* real text not resolved, offset 0x78 */);
		stg_log_startup_error(0xe4);
		goto fail_heap;
	}

	oa_debug_marker(7);
	if (InitPcmModProcInterface() != 0) {		/* step 7 */
		printk("OA: InitPcmModProcInterface failed\n" /* real text not resolved, offset 0xa4 */);
		stg_log_startup_error(0xef);
		goto fail_shmem;
	}

	oa_debug_marker(8);
	if (setup_global_resources(gModuleParam10) != 0) {	/* step 8 */
		printk("OA: setup_global_resources failed\n" /* real text not resolved, offset 0xcc */);
		stg_log_startup_error(0xfd);
		goto fail_pcmproc;
	}

	IncProgressBar();
	{
		oa_debug_marker(9);
	int atmelResult = SetupAtmelForAuthorizations();	/* step 9 */
		if (atmelResult != 0) {
			printk("OA: SetupAtmelForAuthorizations failed, result=%d\n" /* real text not resolved, offset 0x10d */, atmelResult);
			stg_log_startup_error(0x12b);
			goto fail_globalres;
		}
	}

	oa_debug_marker(10);
	if (setup_stg_decrypt_daemons() != 0) {	/* step 10 */
		printk("OA: setup_stg_decrypt_daemons failed\n" /* real text not resolved, offset 0xf0 */);
		stg_log_startup_error(0x139);
		goto fail_globalres;
	}
	/* ~100-iteration settle delay for the decrypt daemons to come up,
	 * confirmed via the countdown loop's exact bound (0x64 = 100). */
	for (int i = 100; i > 0; i--)
		__const_udelay(0x418958);

	if (gModuleParam14 != 0) {
		SetInstalledOptions(0x20);
		printk("OA: gModuleParam14 set, InstalledOptions|=0x20\n" /* real text not resolved, offset 0x114 */);
	}
	oa_debug_marker(11);
	if (load_global_resources() != 0) {		/* step 11 */
		printk("OA: load_global_resources failed\n" /* real text not resolved, offset 0x13f */);
		/* confirmed: this one failure path skips the
		 * stg_log_startup_error call every other hard-fail branch
		 * makes -- not an omission here, matches the disassembly. */
		goto fail_globalres;
	}

	if (gModuleParam18 != 0) {
		SetInstalledOptions(0x10);
		printk("OA: gModuleParam18 set, InstalledOptions|=0x10\n" /* real text not resolved, offset 0x140 */);
	}
	oa_debug_marker(12);
	if (setup_stg_daemons() != 0) {			/* step 12 */
		printk("OA: setup_stg_daemons failed\n" /* real text not resolved, offset 0x15d */);
		stg_log_startup_error(0x139);
		goto fail_globalres;
	}

	IncProgressBar();
	oa_debug_marker(13);
	if (CSTGAudioManager_StartAudioEngine() == 0) {	/* step 13 -- INVERTED convention, see note above */
		printk("OA: CSTGAudioManager_StartAudioEngine failed\n" /* real text not resolved, offset 0x177 */);
		stg_log_startup_error(0x192);
		goto fail_daemons;
	}

	oa_debug_marker(14);
	if (CSTGKeybedInterface_Startup() == 0) {	/* step 14 -- INVERTED convention */
		stg_log_startup_error(0x1a0);
		goto fail_audio;
	}

	oa_debug_marker(15);
	CSTGDrumPadInterface_Initialize();		/* step 15 -- soft, result unchecked */

	oa_debug_marker(16);
	if (stg_rtfifo_init() != 0) {			/* step 16 */
		printk("OA: stg_rtfifo_init failed\n" /* real text not resolved, offset 0x1a7 */);
		stg_log_startup_error(0x1c0);
		CSTGDrumPadInterface_Cleanup();
		CSTGKeybedInterface_Cleanup();
		goto fail_audio;
	}

	oa_debug_marker(17);
	{						/* step 17: success */
		IncProgressBar();
		unsigned long long tsc = stg_rdtsc();
		rt_printk("OA: init_module succeeded, tsc_lo=%08x tsc_hi=%08x\n" /* real text not resolved, offset 0x160 */,
			  (unsigned int)tsc, (unsigned int)(tsc >> 32));
		CSTGAudioManager_EnableAudioManagerThread();

		stg_set_cpus_allowed(stg_get_current_task(), originalCpuMask);
		return 0;
	}

	/* Partial-unwind cascade: each label enters at the depth the real
	 * disassembly enters at for that failure point, then falls through
	 * every earlier step's own cleanup in order -- not a blanket
	 * teardown. Confirmed via the disassembly's own fall-through chain
	 * (each label's code is exactly the next label's entry point). */
fail_audio:
	CSTGAudioManager_StopAudioEngine();
fail_daemons:
	cleanup_stg_daemons();
fail_globalres:
	cleanup_global_resources();
fail_pcmproc:
	CleanupPcmModProcInterface();
fail_shmem:
	CleanupSharedMemProcInterface();
fail_heap:
	CleanupSharedHeap();
fail_early:
	stg_set_cpus_allowed(stg_get_current_task(), originalCpuMask);
fail_before_pin:
	cleanup_cpp_support();
	return -1;
}
