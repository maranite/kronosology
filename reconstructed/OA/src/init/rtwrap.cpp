// SPDX-License-Identifier: GPL-2.0
/*
 * rtwrap.cpp -- real bodies for the `rtwrap_*` RTAI wrapper layer
 * (batch 37). Ground-truthed via a full objdump disassembly of
 * `.text+0x118f00`..`.text+0x1198a0` in
 * /home/share/Decomp/OA.ko_Decomp/OA.ko (the whole `rtwrap_*`/
 * `get_sizeof_rtwrap_*` namespace lives in this one contiguous run).
 *
 * These are genuine, tiny OA-internal forwarders over real RTAI kernel
 * primitives (`rt_sem_wait`, `rt_task_init`, `rtheap_alloc`, etc) --
 * every one of those primitives is confirmed `U` (undefined) in ground
 * truth too, i.e. real external symbols this project has NEVER had a
 * local definition for and isn't adding one now: on real Kronos
 * hardware they're resolved at insmod time against the real RTAI
 * modules (rtai_hal.ko/rtai_sched.ko/rtai_lxrt.ko), exactly like
 * `filp_open`/`vmalloc` are resolved against the stock kernel. Per the
 * sec 10.185 policy update this is NOT a case that needs an RTAI
 * "virtual substitute" -- there is no dispatch/scheduling logic on
 * OA.ko's OWN side to fake; these functions merely marshal arguments
 * and forward. RTAI itself not being loaded in a VM sandbox (no real
 * hardware, no real rtai_lxrt.ko) is an already-documented, expected
 * limitation (sec 10.185 point 3), not something this file works
 * around.
 *
 * Every one of these 22 functions is declared `void`-returning
 * (matching signatures already established, pre-dating this batch, by
 * every real caller across the codebase -- audio_start.cpp,
 * cpu_affinity.cpp, comport.cpp, comport_init.cpp, managers.cpp,
 * engine.cpp, wave_seq_manager.cpp, streaming_event_manager.cpp,
 * vector_manager.cpp, slot_voice_data_ctor.cpp, engine_startup_bits.cpp
 * -- and ~15 verify/test_*.cpp mocks). Ground truth's own real bodies
 * return int/void* result codes from several of these (e.g.
 * rt_sem_wait's timeout status), but NO caller anywhere in this
 * reconstruction inspects those return values (they're void-declared
 * end to end already) -- consistent with sec 10.154's own precedent
 * ("pointer-mangled-type mismatches... already pre-existing... left
 * as-is") for a project-wide-consistent, zero-observable-behavior-
 * impact simplification: real side effects (which RTAI primitive gets
 * called, with which marshaled arguments) are still 100% faithfully
 * reproduced; only a discarded status code is dropped, and dropping it
 * changes nothing since nothing was ever going to read it.
 */

extern "C" {

/* Real RTAI externs this file forwards to -- all confirmed `U` in
 * ground truth OA.ko (never OA-internal). */
void *rt_whoami(void);
int   rt_task_delete(void *task);
int   rt_task_suspend(void *task);
int   rt_sem_wait(void *sem);
int   rt_sem_wait_if(void *sem);
int   rt_sem_signal(void *sem);
int   rt_sem_delete(void *sem);
int   rt_typed_sem_init(void *sem, int value, int type);
void  rt_set_runnable_on_cpuid(void *task, unsigned int cpuId);
void  clear_debug_traps_in_rt_task(void *task);
int   rt_shutdown_irq(unsigned int irq);
int   rt_release_irq(unsigned int irq);
int   rt_assign_irq_to_cpu(unsigned int irq, unsigned int cpu);
int   rt_startup_irq(unsigned int irq);
void  rtheap_free(void *heap, void *ptr);

/* RTAI's own global heap descriptor (rtai_lxrt.ko), address-only use
 * here (passed straight through to rtheap_free) -- real size/layout
 * not independently determined, same "opaque placeholder" convention
 * as this project's other not-fully-modeled externals. */
extern unsigned char rtai_global_heap[1];

/* ---- Simple direct forwarders (irq family + set_runnable/clear_debug_traps) ---- */

void rtwrap_shutdown_irq(unsigned int irq)
{
	rt_shutdown_irq(irq);
}

void rtwrap_release_irq(unsigned int irq)
{
	rt_release_irq(irq);
}

void rtwrap_assign_irq_to_cpu(unsigned int irq, unsigned int cpu)
{
	rt_assign_irq_to_cpu(irq, cpu);
}

void rtwrap_startup_irq(unsigned int irq)
{
	rt_startup_irq(irq);
}

void rtwrap_set_runnable_on_cpuid(void *taskHandle, unsigned int cpuId)
{
	rt_set_runnable_on_cpuid(taskHandle, cpuId);
}

void rtwrap_clear_debug_traps_in_rt_task(void *taskHandle)
{
	clear_debug_traps_in_rt_task(taskHandle);
}

/* ---- Free (rtheap_free against the RTAI global heap) ---- */

void rtwrap_free(void *ptr)
{
	rtheap_free(&rtai_global_heap, ptr);
}

/* ---- pthread_mutex_* ---- */

/*
 * Confirmed real mode derivation (ground truth `.text+0x119410`):
 * attr==NULL -> mode=0 (default/"normal", RTAI counting-semaphore
 * mode). Otherwise reads *attr (see rtwrap_pthread_mutexattr_settype's
 * own confirmed bit layout below): bit0 ("normal" type) set -> mode=0;
 * bit0 clear (i.e. recursive-bit2 or errorcheck-bit1 set) -> mode=1 if
 * bit1 (errorcheck) is CLEAR (recursive case), else mode=-1
 * (0xffffffff, errorcheck case). Always calls
 * rt_typed_sem_init(mutex, mode, 3) (type 3 = confirmed literal).
 */
void rtwrap_pthread_mutex_init(void *mutex, void *attr)
{
	int mode = 0;
	if (attr) {
		unsigned int f = *(unsigned int *)attr;
		if ((f & 1) == 0)
			mode = (f & 2) ? -1 : 1;
	}
	rt_typed_sem_init(mutex, mode, 3);
}

/*
 * Confirmed real (`.text+0x119450`): only signals+deletes the
 * semaphore if a non-blocking wait on it succeeds first (real
 * ground-truth guards against destroying a mutex someone else is
 * actively holding/blocked on -- `rt_sem_wait_if` returning negative
 * skips both calls entirely).
 */
void rtwrap_pthread_mutex_destroy(void *mutex)
{
	if (rt_sem_wait_if(mutex) >= 0) {
		rt_sem_signal(mutex);
		rt_sem_delete(mutex);
	}
}

void rtwrap_pthread_mutex_lock(void *mutex)
{
	rt_sem_wait(mutex);
}

void rtwrap_pthread_mutex_unlock(void *mutex)
{
	rt_sem_signal(mutex);
}

/* ---- pthread_mutexattr_* ---- */

void rtwrap_pthread_mutexattr_init(void *attr)
{
	*(unsigned int *)attr = 1;
}

/*
 * Confirmed real (`.text+0x1193b0`): (flags & ~7) | bit, bit selected
 * by `type` -- 0 -> bit0 (0x1, "normal"/timed), 1 -> bit2 (0x4,
 * recursive -- matches `get_pthread_recursive_attr_constant() == 1`,
 * already used at every recursive-attr call site in this codebase), 2
 * -> bit1 (0x2, errorcheck). Any other `type` is invalid in ground
 * truth (returns -EINVAL); dropped since void, no caller here ever
 * passes anything but 0/1/2.
 */
void rtwrap_pthread_mutexattr_settype(void *attr, int type)
{
	unsigned int f = *(unsigned int *)attr & ~7u;
	if (type == 1)
		f |= 4;
	else if (type == 2)
		f |= 2;
	else if (type == 0)
		f |= 1;
	*(unsigned int *)attr = f;
}

/* Confirmed real: true no-op, ground truth is `xor eax,eax; ret` only. */
void rtwrap_pthread_mutexattr_destroy(void *)
{
}

/* ---- pthread_cond_init ---- */

/*
 * Confirmed real (`.text+0x1195d0`): the `attr` argument is NEVER
 * read -- always `rt_typed_sem_init(cond, 0, 1)` regardless.
 */
void rtwrap_pthread_cond_init(void *cond, void *)
{
	rt_typed_sem_init(cond, 0, 1);
}

/* ---- pthread_attr_* ---- */

/*
 * Confirmed real (`.text+0x119080`): four fixed dwords -- matches
 * `get_sizeof_rtwrap_pthread_attr() == 16` exactly (4 dwords).
 */
void rtwrap_pthread_attr_init(void *attr)
{
	unsigned int *a = (unsigned int *)attr;
	a[0] = 0x2000;
	a[1] = 1;
	a[2] = 0xf4240;
	a[3] = 1;
}

/*
 * Confirmed real (`.text+0x1190b0`): valid priority range is [1..140]
 * (bounds-checked via `(unsigned)(priority-1) <= 139`); stores
 * `140 - priority` at attr+0xc on success. Field +0x4 is
 * unconditionally set to 1 regardless of validity. Out-of-range
 * priority leaves attr+0xc untouched (real fn returns -EINVAL;
 * dropped since void).
 */
void rtwrap_pthread_attr_setrtpriority(void *attr, int priority)
{
	unsigned int *a = (unsigned int *)attr;
	a[1] = 1;
	if ((unsigned int)(priority - 1) <= 139u)
		a[3] = 140 - priority;
}

/*
 * Confirmed real (`.text+0x1190e0`): overwrites attr+0x0 (the SAME
 * field `rtwrap_pthread_attr_init` sets to 0x2000) with the raw stack
 * size -- a real, faithfully-preserved quirk, not a bug to "fix" by
 * writing elsewhere.
 */
void rtwrap_pthread_attr_setstacksize(void *attr, unsigned int stackSize)
{
	*(unsigned int *)attr = stackSize;
}

/* Confirmed real: true no-op, ground truth is `xor eax,eax; ret` only. */
void rtwrap_pthread_attr_destroy(void *)
{
}

/* ---- whoami / task_suspend ---- */

void *rtwrap_whoami(void)
{
	return rt_whoami();
}

void rtwrap_task_suspend(void *task)
{
	rt_task_suspend(task);
}

/*
 * Confirmed real (`.text+0x119280`): if `taskHandle` is NULL, cancels
 * the CALLING task instead (`rt_whoami()`), matching the real RTAI
 * "current task" self-reference idiom used throughout this cluster.
 * After `rt_task_delete`, frees the task's own rtwrap-allocation
 * bookkeeping pointer stored at task+0x5b8 -- this offset is the exact
 * same field `rtwrap_pthread_create`'s own (still-deferred) real body
 * populates with the raw `rtheap_alloc` pointer (ground-truthed from
 * that function's own disassembly: `mov %eax,0x5b8(%ebx)` right after
 * the alloc call, where %eax is rtheap_alloc's raw return and %ebx is
 * the 64-byte-aligned task block derived from it) -- so this is
 * self-consistent with that sibling's own real layout even though
 * `rtwrap_pthread_create` itself hasn't been promoted to a real body
 * yet. Not a new wild-pointer risk in this reconstruction's own
 * reachable call graph: `rtwrap_pthread_create` currently always
 * returns 0/NULL (bar2_stubs_c.cpp), so no caller in this build can
 * currently obtain a non-NULL `taskHandle` from it -- every real
 * invocation reachable today takes the `rt_whoami()` self-cancel path,
 * and (as documented in cpu_affinity.cpp/oa_cpu_affinity.h) that
 * caller's own chain is itself still gated behind still-stubbed
 * daemon-lifecycle code, so this dereference is not live on any
 * currently-exercised path -- only on real hardware with real RTAI and
 * a real `rtwrap_pthread_create`, exactly the intended operating
 * environment.
 */
void rtwrap_pthread_cancel(void *taskHandle)
{
	void *task = taskHandle ? taskHandle : rt_whoami();
	rt_task_delete(task);
	void *rawAlloc = *(void **)((unsigned char *)task + 0x5b8);
	rtheap_free(&rtai_global_heap, rawAlloc);
}

} /* extern "C" */
