// SPDX-License-Identifier: GPL-2.0
/*
 * cpu_affinity.cpp  -  CSTGThread::CreateRealTimeWithCPUAffinity(). See
 * oa_cpu_affinity.h for the full ground-truthing details.
 *
 * Faithful, instruction-level reconstruction from a full objdump
 * disassembly + relocation trace of the real method
 * (`.text+0x40a30`, 202 bytes) in OA_real.ko, including its variable-
 * length stack allocation for the RTAI pthread-attr object (modeled
 * with `alloca()` to match the real disassembly's own technique
 * exactly, rather than a fixed-size guess).
 *
 * BUG FIX (batch 39): this function's own `rtwrap_pthread_create()`
 * result check was inverted (`if (!createResult) return 0;`, i.e.
 * treated a ZERO return as failure) -- harmless while
 * `rtwrap_pthread_create` was still a stub always returning 0 (every
 * call self-consistently looked like "immediate failure"), but exactly
 * backwards relative to ground truth's REAL polarity (0 = success),
 * now that `rtwrap_pthread_create` has a real body (src/init/rtwrap.cpp).
 * Confirmed directly from THIS function's own real disassembly
 * (`.text+0x40a9a` calls `rtwrap_pthread_create`, saves its return in
 * `edi`, then `.text+0x40aa9: test edi,edi; je 0x40ac0` -- jumps to the
 * debug-trap-install/CPU-pinning SUCCESS path when the return value IS
 * ZERO; a nonzero return falls through to the early `return 0`/failure
 * path instead), independently cross-checked against
 * `rtwrap_pthread_create`'s own real body (returns 0 only after
 * `rt_task_init` succeeds and `*out` has been written; returns a
 * nonzero error/sentinel on every failure path, `*out` untouched).
 * Fixed here to `if (createResult) return 0;`. The matching
 * `test_cpu_affinity.cpp` mock's own success/failure return values were
 * flipped to match (0 = success, nonzero = failure).
 */

#include "oa_cpu_affinity.h"

char CSTGThread::CreateRealTimeWithCPUAffinity(void *(*entryFn)(void *), int priority,
						unsigned int cpuId, void *arg)
{
	/* Confirmed real: size queried at runtime, not hardcoded (RTAI's
	 * own pthread-attr layout can vary by build); rounded up with the
	 * exact confirmed constants (+0x1e, then 16-byte-aligned). */
	unsigned int attrSize = (get_sizeof_rtwrap_pthread_attr() + 0x1e) & ~0xfU;
	/* __builtin_alloca, not <alloca.h>'s alloca() -- avoids the same
	 * kind of libc-header/oa_internal.h declaration conflict this
	 * project already hit once with <cstring> (see stgheap_init.cpp);
	 * GCC's own builtin needs no header at all. */
	void *attrRaw = __builtin_alloca(attrSize + 0x10); /* +0x10: room
							     * for the confirmed
							     * real +0x13-then-
							     * align-down
							     * sub-alignment
							     * step below. */
	void *attr = (void *)(((unsigned long)attrRaw + 0x13) & ~0xfUL);

	rtwrap_pthread_attr_init(attr);
	rtwrap_pthread_attr_setrtpriority(attr, priority);
	/* Confirmed literal, not derived from any argument. */
	rtwrap_pthread_attr_setstacksize(attr, 0x5000);

	void *createResult = rtwrap_pthread_create(this, attr, entryFn, arg);
	rtwrap_pthread_attr_destroy(attr);

	if (createResult)
		return 0;

	debugTrapsInstalled = 1;
	if (rtwrap_set_debug_traps_in_rt_task(taskHandle) == 0) {
		rtwrap_set_runnable_on_cpuid(taskHandle, cpuId);
		return 1;
	}

	/* Confirmed real: debug-trap installation failed -- tear the
	 * just-created thread back down. The real disassembly re-checks
	 * `debugTrapsInstalled != 0` before doing so (faithfully
	 * reproduced, even though it was just set to 1 immediately above
	 * and so is unconditionally true here). */
	if (debugTrapsInstalled) {
		rtwrap_clear_debug_traps_in_rt_task(taskHandle);
		rtwrap_pthread_cancel(taskHandle);
		debugTrapsInstalled = 0;
	}
	return 0;
}
