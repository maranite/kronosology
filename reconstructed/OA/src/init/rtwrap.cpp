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

/*
 * Real RTAI externs this file forwards to -- all confirmed `U` in
 * ground truth OA.ko (never OA-internal).
 *
 * CALLING-CONVENTION SPLIT, confirmed by disassembling every call site
 * to each of these across rtwrap.cpp's own real bodies (batch 39):
 * ground truth's OWN caller code passes some of these via plain
 * register args (this OA.ko build's default `-mregparm=3`) and others
 * via the STACK regardless of how few args they take -- i.e. ground
 * truth's own RTAI header declared a SUBSET of these with an explicit
 * `regparm(0)` (cdecl) override, presumably because rtai_sched.ko's
 * LXRT scheduling primitives (semaphore wait/signal, task suspend/
 * resume, set-runnable) go through a different real-time dispatch path
 * than the plain administrative calls (task_delete, whoami, irq
 * management, debug-trap toggling), which stay plain regparm(3).
 * Evidence (each independently confirmed via >=1 real call site):
 *   - STACK-passed in ground truth (`rt_typed_sem_init`, called from
 *     `rtwrap_pthread_mutex_init` @.text+0x119410 with all 3 args
 *     placed at [esp]/[esp+4]/[esp+8], none in eax/edx/ecx even though
 *     3 args fit trivially in registers under regparm(3)): rt_sem_wait
 *     (`rtwrap_pthread_join` @.text+0x1191e0), rt_sem_wait_if
 *     (`rtwrap_pthread_mutex_destroy`/`rtwrap_pthread_join`), rt_sem_signal/
 *     rt_sem_delete (`rtwrap_pthread_mutex_destroy` @.text+0x119450),
 *     rt_typed_sem_init (as above + `rtwrap_pthread_create` @.text+0x1190f0),
 *     rt_task_suspend/rt_task_resume (`rtwrap_task_suspend`/
 *     `rtwrap_task_resume` @.text+0x119780/0x1197a0, single arg pushed
 *     to `[esp]` rather than left in `eax`), rt_set_runnable_on_cpuid
 *     (`rtwrap_set_runnable_on_cpuid` @.text+0x118f20, BOTH args at
 *     `[esp]`/`[esp+4]`).
 *   - REGISTER-passed (regparm(3), matches this file's un-annotated
 *     default, NO bug): rt_whoami, rt_task_delete (`rtwrap_pthread_cancel`/
 *     `rtwrap_pthread_join`, arg stays in `eax` through the call),
 *     rt_shutdown_irq/rt_release_irq/rt_assign_irq_to_cpu/rt_startup_irq
 *     (pure passthrough, arg never moved out of `eax`/`edx`),
 *     clear_debug_traps_in_rt_task, rt_task_init (3 args register +
 *     4 more on the stack -- textbook regparm(3) with >3 total args,
 *     `rtwrap_pthread_create`'s own real body, see below).
 * Getting this right matters here specifically because these
 * primitives are resolved against the REAL rtai_sched.ko on real
 * hardware (unlike a within-project-only mock), so a convention
 * mismatch would silently deliver garbage/uninitialized-stack args to
 * genuine kernel RT primitives instead of merely being an internal
 * inconsistency.
 */
void *rt_whoami(void);
int   rt_task_delete(void *task);
int   rt_task_suspend(void *task) __attribute__((regparm(0)));
int   rt_task_resume(void *task) __attribute__((regparm(0)));
int   rt_sem_wait(void *sem) __attribute__((regparm(0)));
int   rt_sem_wait_if(void *sem) __attribute__((regparm(0)));
int   rt_sem_signal(void *sem) __attribute__((regparm(0)));
int   rt_sem_delete(void *sem) __attribute__((regparm(0)));
int   rt_typed_sem_init(void *sem, int value, int type) __attribute__((regparm(0)));
void  rt_set_runnable_on_cpuid(void *task, unsigned int cpuId) __attribute__((regparm(0)));
void  clear_debug_traps_in_rt_task(void *task);
int   rt_shutdown_irq(unsigned int irq);
int   rt_release_irq(unsigned int irq);
int   rt_assign_irq_to_cpu(unsigned int irq, unsigned int cpu);
int   rt_startup_irq(unsigned int irq);
/* rt_request_irq: confirmed real (via relocation) sibling of the irq
 * family above -- see rtwrap_request_irq's own header comment (sec
 * 10.237) for the ground-truthing detail (`objdump -d -r` on ground
 * truth's own `rtwrap_request_irq`, a pure one-arg-marshalled forward
 * to this exact symbol). */
int   rt_request_irq(unsigned int irq, void (*handler)(unsigned int, void *), void *dev,
		      unsigned int flags);
void  rtheap_free(void *heap, void *ptr);
/* rtheap_alloc/rt_task_init are both confirmed register-passed
 * (regparm(3) default, no override) via `rtwrap_pthread_create`'s own
 * real disassembly -- see that function's header comment below. */
void *rtheap_alloc(void *heap, unsigned int size, int flags);
int   rt_task_init(void *task, void (*entry)(long), long data,
		    unsigned int stackSize, unsigned int priority,
		    int usesFpu, void (*signalFn)(void));

/* RTAI's own global heap descriptor (rtai_lxrt.ko), address-only use
 * here (passed straight through to rtheap_free) -- real size/layout
 * not independently determined, same "opaque placeholder" convention
 * as this project's other not-fully-modeled externals. */
extern unsigned char rtai_global_heap[1];

} /* extern "C" */

/*
 * This project's established pointer-width convention (sec 10.150 et
 * al): `rtwrap_pthread_create`'s own real target task-control block
 * packs THREE 32-bit pointer-sized fields only 4 bytes apart
 * (task+0x5b0/+0x5b4/+0x5b8 -- start routine, thread arg, raw alloc
 * pointer). A native 8-byte host `void*` write/read at any one of these
 * overlaps its neighbor's own bytes on this 64-bit host (confirmed by a
 * real KAT FAILED result during this batch, matching sec 10.181's own
 * "two real kernel pointer fields within 8 bytes of each other" gotcha
 * exactly) even though the real 32-bit target has no such overlap.
 * ToU32/FromU32 (explicit 32-bit store/zero-extend load) sidesteps this
 * unconditionally, on both the host KAT and the real `-m32` target.
 */
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static void *FromU32(unsigned int v) { return (void *)(unsigned long)v; }

extern "C" {

/* ---- Simple direct forwarders (irq family + set_runnable/clear_debug_traps) ---- */

/*
 * Confirmed real (`.text+0x119820`, sec 10.237): a pure one-arg-
 * marshalled forward to `rt_request_irq` -- see this file's own header
 * declaration comment above and oa_comport.h's own CSTGComPort::
 * Initialize() call site (the ONLY real caller in this reconstruction).
 */
int rtwrap_request_irq(unsigned int irq, void (*handler)(unsigned int, void *), void *dev,
			unsigned int flags)
{
	return rt_request_irq(irq, handler, dev, flags);
}

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

/* ---- Alloc/Free (rtheap_alloc/rtheap_free against the RTAI global heap) ---- */

/*
 * Confirmed real (`.text+0x118ee0`, real-hardware boot regression found
 * 2026-07-21): a pure two-arg-marshalled forward to `rtheap_alloc`,
 * mode fixed at 0 -- the exact same "address-only" `rtai_global_heap`
 * usage as this file's own `rtwrap_free` below. This function's real
 * counterpart was never promoted out of the `bar2_stubs_c.cpp`
 * always-NULL placeholder until now; every caller (CSTGSlotVoiceData::
 * CSTGSlotVoiceData(), CSTGGlobal's manager ctors, wave_seq_manager.cpp,
 * vector_manager.cpp, streaming_event_manager.cpp, engine_startup_bits.cpp)
 * hands the result straight to `rtwrap_pthread_mutex_init`/similar with
 * no NULL check, so the stub's unconditional NULL crashed the first
 * real-hardware boot attempt inside `rt_typed_sem_init` (NULL+0xc) --
 * the RTAI global heap itself was already live at that point (rtai_sched.ko's
 * own boot log: "RTAI[malloc]: global heap size = 2097152 bytes"), this
 * function just never actually called into it.
 */
void *rtwrap_malloc(unsigned int size)
{
	return rtheap_alloc(&rtai_global_heap, size, 0);
}

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
 * rtwrap_pthread_create(pthread_t *out, void *attr, void *(*start)(void*),
 * void *arg) -- confirmed real (`.text+0x1190f0`, 238 bytes), batch 39.
 * Promoted from its long-standing `{ return 0; }` stub (bar2_stubs_c.cpp)
 * now that the real calling conventions for its own RTAI dependencies
 * (rt_typed_sem_init/rt_task_init/rt_task_resume) are confirmed above.
 *
 * Real algorithm:
 *   1. `raw = rtheap_alloc(&rtai_global_heap, 0x600, 0)`; if NULL,
 *      returns -12 (0xfffffff4) immediately, `*out` untouched.
 *   2. `task = (raw + 0x40) & ~0x3f` (round up to the next 64-byte
 *      boundary -- the actual RT_TASK block lives inside the raw
 *      allocation, not AT its start).
 *   3. Stashes bookkeeping fields on the task block: `+0x5b8` = the RAW
 *      alloc pointer (needed later ONLY to `rtheap_free` it -- this is
 *      the exact field `rtwrap_pthread_cancel`'s own real body already
 *      reads, sec 10.188/batch 37), `+0x5b0` = `start`, `+0x5b4` = `arg`,
 *      `+0x8` = 0.
 *   4. Reads `attr` (if non-NULL): `stackSize = *(unsigned int*)attr`
 *      (attr+0x0, the SAME field `rtwrap_pthread_attr_setstacksize`
 *      overwrites) and `priorityWord = *(unsigned int*)((char*)attr+0xc)`
 *      (attr+0xc, the SAME field `rtwrap_pthread_attr_setrtpriority`
 *      populates as `140 - priority`). If `attr` is NULL, uses the
 *      confirmed real defaults `stackSize = 0x2000`,
 *      `priorityWord = 0x3fffffff` (RTAI's lowest-priority sentinel).
 *   5. `rt_task_init(task, (trampoline), (long)task, stackSize,
 *      priorityWord, usesFpu=1 (hardcoded), signal=0/NULL (hardcoded))`.
 *      On a NONZERO return: `rtheap_free(&rtai_global_heap, raw)`,
 *      returns that error code unchanged, `*out` untouched.
 *   6. On success (return 0): `rt_typed_sem_init(task+0x580, 0, 5)`
 *      (a per-task semaphore, type 5 -- distinct from the mutex's
 *      type-3 and cond's type-1 uses above), `rt_task_resume(task)`,
 *      `*out = task`, returns 0.
 *
 * The `trampoline` entry point WAS modeled as a literal constant
 * address, `.text+0x118e80` in ground truth -- a real internal function
 * (`T`) this project does not reconstruct (the actual RT thread entry
 * point, presumably dispatching through the `start`/`arg` fields this
 * function just stashed at task+0x5b0/+0x5b4 -- a separate, larger
 * task). That was faithful as long as nothing in this project's own
 * reconstruction ever actually RAN it (`rtwrap_pthread_create` only
 * passes the address through to `rt_task_init`, never calls it
 * directly) -- true right up until `RTAIVirtualDriver.ko` (this
 * project's own from-scratch RTAI substitute) got a real,
 * functioning `rt_task_init()`/`rt_task_resume()` (sec 10.190/batch 39
 * onward) that genuinely spawns a kthread and jumps straight to
 * whatever entry pointer it's given -- at which point the literal
 * `0x118e80` becomes exactly the same "kernel API will actually jump
 * to this address" hazard sec 10.234 already found and fixed once for
 * `kernel_thread()`'s own `DAEMON_THREAD_TRAMPOLINE`/
 * `DECRYPT_THREAD_TRAMPOLINE` (daemon_lifecycle.cpp).
 *
 * FIXED (sec 10.235, 2026-07-13, confirmed via live boot): live-tested
 * on kronosvm once `rtwrap_set_debug_traps_in_rt_task`'s own blocking
 * stub was fixed (bar2_stubs_c.cpp) -- `CreateRealTimeWithCPUAffinity()`
 * finally reached the point of actually spawning and resuming a real-
 * time kthread for the first time this project has ever recorded
 * (`OA_DEBUG_MARKER 14` fired), which promptly Oopsed jumping to the
 * literal `0x118e80` (`rtv_task/...` kthread, `EIP == CR2 == 0x118e80`,
 * `Bad EIP value` -- nothing valid lives at that address in THIS
 * project's freshly-linked `OA.ko`). Fixed with a real, minimal, SAFE
 * stand-in entry point (`RtwrapThreadTrampoline`, matching
 * `rt_task_init`'s own required `void(*)(long)` signature exactly),
 * following the sec 10.234 precedent to the letter: it does NOT
 * dispatch through the stashed `start`/`arg` fields at task+0x5b0/
 * +0x5b4, even though this function's own header comment above
 * documents that as the real trampoline's presumed job -- every
 * CURRENT caller of `CreateRealTimeWithCPUAffinity()`
 * (`CSTGAudioThread::AudioTickLoopRoutine`/`CSTGAudioManager::
 * ASKThreadRoutine`/`AudioManagerThreadRoutine`, `audio_start.cpp`) is
 * a genuine infinite real-time audio-DSP service loop -- out of scope
 * per sec 10.185 -- and `ASKThreadRoutine`'s own real body calls
 * `rtwrap_task_suspend(rtwrap_whoami())` on itself in a tight loop,
 * whose interaction with this from-scratch substitute's own
 * `rt_task_suspend()` semantics (an ordinary kthread, not a genuine
 * hard-RT task) is NOT verified safe -- dispatching to it risks
 * trading one live-boot Oops for a silent busy-loop/soft-lockup
 * hazard. Honestly inert (like the sec 10.234 daemon entries) is the
 * safe choice here, not a fabricated approximation of unreconstructed
 * audio-DSP logic.
 *
 * BUG FOUND AND FIXED while ground-truthing this function (batch 39):
 * `CSTGThread::CreateRealTimeWithCPUAffinity()` (cpu_affinity.cpp,
 * already-committed, reconstructed while this function was still a
 * stub) tested this function's return value backwards
 * (`if (!createResult) return 0;`, i.e. treated 0 as FAILURE) --
 * harmless while `rtwrap_pthread_create` always returned 0 (every call
 * looked like "immediate failure", self-consistently, matching that
 * function's own then-correct description of the STUBBED behavior),
 * but exactly inverted relative to ground truth's REAL polarity
 * (0 = success), confirmed independently from BOTH sides: this
 * function's own disassembly (esi, the return value, stays 0 only on
 * the success path) AND the real caller's own disassembly
 * (`.text+0x40a30`: `test edi,edi; je <success-path>` -- jumps to the
 * debug-trap-install/CPU-pinning success path when the return value
 * IS ZERO). See cpu_affinity.cpp's own fix + updated
 * test_cpu_affinity.cpp mock polarity.
 */
/*
 * Real, minimal, SAFE stand-in for the ground-truth `.text+0x118e80`
 * trampoline -- see the header comment above for why this deliberately
 * does NOT dispatch through the stashed start/arg fields. Matches
 * `rt_task_init`'s own required `void(*)(long)` signature exactly;
 * `taskArg` is the RT_TASK block pointer itself (`(long)task`, per
 * `rtwrap_pthread_create`'s own call below) -- unused here, but kept as
 * a named, typed parameter (not `void`) so the signature documents what
 * a real caller actually passes, matching this project's own
 * established convention for confirmed-real call-site shapes.
 */
static void RtwrapThreadTrampoline(long taskArg)
{
	(void)taskArg;
}
static void *const RTWRAP_THREAD_TRAMPOLINE = (void *)&RtwrapThreadTrampoline;

void *rtwrap_pthread_create(void *out, void *attr, void *(*start)(void *), void *arg)
{
	void *raw = rtheap_alloc(&rtai_global_heap, 0x600, 0);
	if (!raw)
		return (void *)(long)-12;

	unsigned char *task = (unsigned char *)(((unsigned long)raw + 0x40) & ~0x3ful);
	*(unsigned int *)(task + 0x5b8) = ToU32(raw);
	*(unsigned int *)(task + 0x5b0) = ToU32((void *)start);
	*(unsigned int *)(task + 0x5b4) = ToU32(arg);
	*(int *)(task + 0x8) = 0;

	unsigned int stackSize = 0x2000;
	unsigned int priorityWord = 0x3fffffff;
	if (attr) {
		stackSize = *(unsigned int *)attr;
		priorityWord = *(unsigned int *)((unsigned char *)attr + 0xc);
	}

	int ret = rt_task_init(task, (void (*)(long))RTWRAP_THREAD_TRAMPOLINE,
				(long)task, stackSize, priorityWord, 1, 0);
	if (ret != 0) {
		rtheap_free(&rtai_global_heap, raw);
		return (void *)(long)ret;
	}

	rt_typed_sem_init(task + 0x580, 0, 5);
	rt_task_resume(task);
	*(void **)out = task;
	return 0;
}

/*
 * Confirmed real (`.text+0x119280`): if `taskHandle` is NULL, cancels
 * the CALLING task instead (`rt_whoami()`), matching the real RTAI
 * "current task" self-reference idiom used throughout this cluster.
 * After `rt_task_delete`, frees the task's own rtwrap-allocation
 * bookkeeping pointer stored at task+0x5b8 -- confirmed self-consistent
 * with `rtwrap_pthread_create`'s own real layout just above (same field,
 * populated with the raw `rtheap_alloc` pointer).
 */
void rtwrap_pthread_cancel(void *taskHandle)
{
	void *task = taskHandle ? taskHandle : rt_whoami();
	rt_task_delete(task);
	void *rawAlloc = FromU32(*(unsigned int *)((unsigned char *)task + 0x5b8));
	rtheap_free(&rtai_global_heap, rawAlloc);
}

} /* extern "C" */
