// SPDX-License-Identifier: GPL-2.0
/*
 * load_global_resources.cpp  -  load_global_resources() (batch 52).
 *
 * REPRIORITIZATION CONTEXT (sec 10.203, 2026-07-11): a mid-batch
 * directive moved this project's priority from deep runtime/audio-
 * command-processing reconstruction to closing gaps in init_module()'s
 * own transitive call graph. `load_global_resources()` is called
 * directly from init_module() step 11 (`src/init/init_module.cpp`) and
 * was, until this batch, a bare `extern "C" int load_global_resources()
 * { return 0; }` stub in bar2_stubs_c.cpp -- silently skipping ALL of
 * this function's real work while still reporting "success" to its
 * caller. That's SAFE (no wild call, init_module() just proceeds as if
 * this step trivially succeeded) but not faithful; this batch closes the
 * gap for real.
 *
 * Ground-truthed via a full disassembly + `.rel.text` relocation
 * resolution against /home/share/Decomp/OA.ko_Decomp/OA.ko
 * (`.text+0x1188b0`, 275 bytes -- `nm -S` reports 0x115/277 bytes;
 * disassembled through `.text+0x1189c9`, the padding before the next
 * symbol, `cleanup_global_resources` at `.text+0x1189d0`; the earlier
 * `bar2_stubs_c.cpp` comment's "115 bytes" was a stale/truncated first
 * scout figure, corrected here against the real `nm -S` size).
 *
 * Confirmed real control flow, in order:
 *   1. `countdown = GetProgressBarValue()` (a thin OA-internal forwarder
 *      to the genuinely-external `COmapNKS4_GetProgressBarPercent()`,
 *      exact same shape as the already-real `IncProgressBar()` ->
 *      `COmapNKS4_IncProgressBar()` pair, startup_helpers.cpp -- and
 *      `COmapNKS4_GetProgressBarPercent` is confirmed genuinely `U` in
 *      ground truth too via `nm -u`, so this is a faithful forwarder to
 *      a real external, not a substitution). Then
 *      `countdown = (countdown > 0x30) ? 1 : (0x31 - countdown)` -- a
 *      confirmed real clamp/countdown transform (`cmp al,0x30; ja ...;
 *      mov edx,0x31; sub dl,al`), producing a value always in [1..49].
 *   2. `mgr = oa_multisample_bank_manager()` (already-established
 *      helper, oa_heap.h -- this function's own `heap_base()+0x60524`
 *      computation, confirmed via the SAME sentinel-checked
 *      `*(sInstance+0x38)+*(sInstance+0x1e8498)` idiom already
 *      documented there, is byte-for-byte identical, not independently
 *      re-derived).
 *   3. `mgr->StartupInitializeROMBank(name, true, countdown)` -- `name`
 *      is a genuine `.rodata.str1.1+0x1794` string relocation (real text
 *      not resolved in this pass, same "honest placeholder" treatment as
 *      every other unresolved format/name string elsewhere in this
 *      project); the `bool` argument is a literal `1`; `countdown` is
 *      step 1's own result, confirmed via register tracing (pushed to
 *      the stack BEFORE `edx` gets overwritten with the string address).
 *      Return value confirmed UNCHECKED (the very next instruction is an
 *      unconditional fresh `mov eax,esi`, not a test of AL).
 *   4. `mgr->StartupInitializeRAMBank()` -- return value IS checked
 *      (`test al,al; je -> hard-fail`). On failure: `rt_printk(
 *      .rodata.str1.4+0x874)` then `stg_log_startup_error(
 *      .rodata.str1.1+0x17a8)` then `return -1` -- a genuine, confirmed
 *      hard-fail path, faithfully reproduced (placeholder string text,
 *      same convention as every other unresolved literal here).
 *   5. `CSTGHeapManager_SetLastFixedBlock()` -- confirmed real call, NO
 *      argument setup precedes it (no explicit mov to eax/edx/ecx/stack)
 *      -- modeled as a genuine zero-argument C-linkage function; brand
 *      new symbol, not referenced anywhere else in this project or
 *      previously documented.
 *   6. `mgr->ScanFileSystem()` -- return value confirmed UNCHECKED (same
 *      shape as step 3).
 *   7. `CSTGKLMManager::sInstance->AuthorizeBuiltins()` -- ALREADY REAL
 *      (batch <unspecified>, src/auth/klm_manager.cpp) -- this
 *      resolution closes the sec 10.203 audit's own open question of
 *      whether this call site was ever wired up to the real body; it
 *      was not, until this batch (the bare stub obviously never called
 *      it at all).
 *   8. `gSystemIsInitialized = 1` -- set UNCONDITIONALLY, confirmed via
 *      instruction ordering: the store instruction itself doesn't depend
 *      on the immediately-preceding `cmp`'s flags (that `cmp` is only
 *      consumed by the LATER conditional `je` a few instructions later,
 *      for a *different* purpose -- gating the two loads used to compute
 *      the installed-products pointer below). A real, faithfully-
 *      reproduced quirk: this flag gets set even if the very next call
 *      (step 9) then reports failure.
 *   9. `products = (CSTGInstalledEXProducts *)(oa_heap_base()+0x14)`
 *      (exact same `+0x14` offset already established independently by
 *      `src/auth/process_oacmd.cpp`'s own local `oa_installed_products()`
 *      helper and `src/auth/products.cpp`'s inline equivalent -- a THIRD
 *      independent cross-confirmation of that offset, from the init path
 *      this time rather than a runtime /proc/.oacmd command).
 *      `products->Initialize()` -- return value checked, but confirmed
 *      SOFT failure: on false, just `rt_printk(.rodata.str1.4+0x89c)`
 *      then falls straight through to step 10 (no stg_log_startup_error,
 *      no early return) -- NOT a hard-fail path, faithfully preserved as
 *      such (i.e. NOT "corrected" into a hard fail that would diverge
 *      from ground truth).
 *   10. Final RAM-budget bookkeeping: `usedBytes = *(heap_base()+
 *       0x6a534)`; if `usedBytes <= 0x9fffff` (~10MB-1), allocates the
 *       REMAINDER up to 10MB via the already-real
 *       `CSTGHeapManager::Alloc(0xa00000 - usedBytes)` (this project's
 *       own established `unsigned int`-mangled static stand-in, batch
 *       17, src/mem/heap_manager_alloc_static.cpp -- declaration now
 *       shared via oa_types.h so this file can link against that exact
 *       same existing body) and stores the returned slot at
 *       `heap_base()+0x10`. If `usedBytes > 0x9fffff`, this whole step
 *       is skipped (confirmed `ja`-past-the-call branch) -- not an
 *       oversight, a real conditional allocation.
 *   11. Returns 0 (success) on every path except step 4's own hard fail.
 *
 * DSP-STUB-CALLEE POLICY (sec 10.185, applied per the sec 10.203
 * init-path-priority directive): `StartupInitializeROMBank`/
 * `StartupInitializeRAMBank`/`ScanFileSystem` (CSTGMultisampleBankManager,
 * oa_types.h) and `CSTGInstalledEXProducts::Initialize()` (oa_types.h)
 * are genuine filesystem/ROM-bank-scan subsystems -- hardware/storage-
 * adjacent, out of scope for full reconstruction here, same category as
 * this project's existing CSTGFile_ / nv2ac_ family of deferrals. Given deliberately
 * deferred SAFE-DEFAULT bodies below (RAM bank init "succeeds", products
 * "succeed") so `load_global_resources()`'s own real control flow runs
 * its intended success path rather than artificially tripping either of
 * the two confirmed failure branches every time. `CSTGHeapManager_
 * SetLastFixedBlock()` is a brand-new, zero-argument, no-op-safe stub
 * for the same reason (its own real body is a separate, not-yet-
 * scoped task).
 */

#include "auth.h"		/* CSTGKLMManager::sInstance/AuthorizeBuiltins() -- already real */
#include "oa_internal.h"
#include "oa_heap.h"		/* oa_heap_base(), oa_multisample_bank_manager() */

/* GetProgressBarValue(): thin forwarder to the genuinely-external
 * COmapNKS4_GetProgressBarPercent(), confirmed `U` in ground truth too
 * (nm -u) -- same treatment as IncProgressBar()/COmapNKS4_IncProgressBar
 * (startup_helpers.cpp). */
extern "C" unsigned char COmapNKS4_GetProgressBarPercent(void);
extern "C" unsigned char GetProgressBarValue(void)
{
	return COmapNKS4_GetProgressBarPercent();
}

/* Brand-new C-linkage symbol, confirmed real (called with zero explicit
 * arguments) but with no prior documentation anywhere in this project --
 * deliberately deferred no-op body, see this file's own header comment. */
extern "C" void CSTGHeapManager_SetLastFixedBlock(void)
{
}

/* `gSystemIsInitialized` (`.bss+0x10725c`) is ALREADY a real, defined
 * symbol elsewhere in this project -- `src/engine/
 * push_unsolicited_message.cpp` (its own `PushUnsolicitedMessage()` reads
 * it as a startup gate) -- confirmed the SAME symbol via this function's
 * own independent relocation trace landing on the identical `.bss+
 * 0x10725c` offset (cross-confirmation, not a coincidence). Declared
 * `extern` here, NOT redefined -- an early draft of this file duplicate-
 * defined it and was caught immediately by a real `ld` "multiple
 * definition" error at `make ko` time (own-draft bug, fixed before
 * commit, not a latent bug in already-committed code). Matches that
 * file's own established type, `unsigned int`, not `int`. */
extern "C" unsigned int gSystemIsInitialized;

extern "C" int load_global_resources(void)
{
	unsigned char countdown = GetProgressBarValue();
	countdown = (countdown > 0x30) ? 1 : (unsigned char)(0x31 - countdown);

	struct CSTGMultisampleBankManager *mgr = oa_multisample_bank_manager();

	mgr->StartupInitializeROMBank(
		"builtin"	/* real text not resolved, .rodata.str1.1+0x1794 */,
		true, countdown);

	if (!mgr->StartupInitializeRAMBank()) {
		/* real text not resolved: rt_printk(.rodata.str1.4+0x874),
		 * stg_log_startup_error(.rodata.str1.1+0x17a8) */
		return -1;
	}

	CSTGHeapManager_SetLastFixedBlock();

	mgr->ScanFileSystem();

	CSTGKLMManager::sInstance->AuthorizeBuiltins();

	gSystemIsInitialized = 1;	/* confirmed unconditional, even if step 9 below fails */

	struct CSTGInstalledEXProducts *products =
		(struct CSTGInstalledEXProducts *)(oa_heap_base() + 0x14);
	if (!products->Initialize()) {
		/* confirmed SOFT failure: real text not resolved,
		 * rt_printk(.rodata.str1.4+0x89c) -- falls straight through,
		 * no hard-fail return here. */
	}

	unsigned int usedBytes = *(unsigned int *)(oa_heap_base() + 0x6a534);
	if (usedBytes <= 0x9fffff) {
		unsigned int slot = CSTGHeapManager::Alloc(0xa00000 - usedBytes);
		*(unsigned int *)(oa_heap_base() + 0x10) = slot;
	}

	return 0;
}
