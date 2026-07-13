// SPDX-License-Identifier: GPL-2.0
/*
 * RTAIVirtualDriver.c  -  software substitute for the real RTAI stack
 * (rtai_hal.ko / rtai_sched.ko / rtai_sem.ko / rtai_fifos.ko / rtai_ndbg.ko /
 * rtai_debug.ko) that OA.ko's own reconstructed `rtwrap_*` wrapper layer
 * (src/init/rtwrap.cpp), daemon lifecycle (src/init/daemon_lifecycle.cpp,
 * src/init/stg_daemons.cpp) and RTAI-FIFO layer (src/init/rtfifo_init.cpp,
 * src/engine/push_unsolicited_message.cpp) call directly.
 *
 * WHAT THIS REPLACES AND WHY
 * ---------------------------
 * MASTER_REFERENCE.md sec 10.211-10.214 (read those sections for the full
 * live-diagnosis trail) established, via repeated live GDB/QEMU-monitor
 * investigation on the dedicated `kronosvm` sandbox, that genuine RTAI
 * cannot be reliably brought up under QEMU-TCG on this project's target
 * kernel (2.6.32.11-korg): `rtai_hal.ko`'s own I-pipe/APIC hardware-timer
 * takeover either hangs deterministically right after "PIPELINE layers"
 * (sec 10.212 -- a genuine, unrecoverable timer-interrupt deadlock caused
 * by a TCG-timing race between a delayed native tick and RTAI's own
 * next-interrupt-deadline calibration, confirmed via the CPU-time-pinned-
 * over-time diagnostic, NOT a bug in this project's own code), or stalls
 * silently one dependency level deeper inside `rtai_sem.ko`/`rtai_ndbg.ko`/
 * `rtai_fifos.ko` even when the first hang is avoided (sec 10.214), or
 * causes an outright kernel panic in RTAI's own scheduler init
 * (`resched_task` BUG, "Attempted to kill the idle task!") when QEMU's
 * `-icount` deterministic-timing mode is used to dodge the first hang
 * (sec 10.214 -- confirmed HARMFUL, not merely untested). None of this is
 * fixable at this project's own layer -- it is entirely inside stock,
 * unbuilt-by-this-project RTAI/kernel internals.
 *
 * Per the standing project policy (MASTER_REFERENCE.md sec 10.185, "RTAI
 * substitution... a first-class, encouraged technique"), this module is a
 * from-scratch, VM-appropriate SUBSTITUTE for the 26 real RTAI symbols
 * OA.ko's own reconstructed callers actually invoke -- NOT a reproduction
 * of real RTAI's internal behavior. It is architecturally the same
 * pattern as this project's three existing hardware virtual drivers
 * (AT88VirtualChip.ko stands in for OmapNKS4Module.ko's AT88SC/NV2AC chip
 * access; OmapNKS4VirtualDriver.ko and KorgUsbAudioVirtualDriver.ko stand
 * in for their own real counterparts), just applied one level deeper --
 * to RTAI itself, rather than to a hardware peripheral OA.ko talks to
 * through RTAI.
 *
 * Because this module deliberately NEVER attempts genuine hard-real-time
 * scheduling -- no I-pipe domain registration, no hardware-timer takeover,
 * no APIC reprogramming -- it entirely SIDESTEPS the exact QEMU-TCG timing
 * hazard sec 10.212-10.214 diagnosed. Every one of the 26 symbols below is
 * implemented purely with ordinary Linux kernel primitives (kthread_create/
 * wake_up_process/kthread_stop, struct semaphore, a workqueue, kzalloc/
 * kfree, plain no-ops for the IRQ-affinity family) that behave completely
 * normally whether the guest is real hardware or QEMU-TCG.
 *
 * WHAT GUARANTEE THIS DELIBERATELY DOES NOT PROVIDE
 * ---------------------------------------------------
 * Genuine hard-real-time scheduling latency/determinism. A "task" here is
 * an ordinary preemptible Linux kthread, not a hard-RT thread running
 * outside the Linux scheduler's normal jitter; rt_task_suspend()/
 * rt_task_resume() can only meaningfully gate a task BEFORE it starts
 * running its entry function (see rt_task_suspend()'s own comment below)
 * since a plain kthread has no external-preemption primitive equivalent to
 * real RTAI's; rt_typed_sem_init()'s "mutex mode" (recursive/errorcheck)
 * nuance is not reproduced, only "start available" is (see its own
 * comment). None of this matters for this module's actual purpose: per
 * sec 10.185, real-time audio/DSP fidelity is already explicitly out of
 * scope for this project's VM-testing effort. This module's only job is
 * to let OA.ko's own structural init/daemon-lifecycle logic run to
 * completion (or fail on its own real logic, not on missing RTAI symbols)
 * in an environment where genuine hard-RT was never achievable anyway.
 *
 * A GENUINE, PRE-EXISTING LIMITATION THIS MODULE DOES NOT AND CANNOT FIX
 * -------------------------------------------------------------------------
 * `rtwrap_pthread_create()` (reconstructed/OA/src/init/rtwrap.cpp) passes
 * `rt_task_init()` a hardcoded ground-truth address constant
 * (`RTWRAP_THREAD_TRAMPOLINE = (void *)0x118e80`) as the RT task's own
 * entry point -- that file's own header comment already documents this as
 * an intentionally-unreconstructed internal OA.ko trampoline function,
 * modeled only as an opaque address because nothing in that file calls it
 * directly. This module's `rt_task_init()` faithfully does what real RTAI
 * would do with whatever entry pointer it is given -- spawn a kthread that
 * eventually calls it -- but it cannot make that specific hardcoded
 * address be a real, valid function in THIS project's freshly-linked
 * OA.ko (its `.text` layout differs from the original ground-truth
 * binary the constant was extracted from). Any live boot that reaches the
 * point where a daemon's kernel thread is actually resumed and jumps to
 * that address is expected to fault there -- a pre-existing OA.ko-side
 * reconstruction gap, not a bug introduced by this module. See this
 * module's own README.md for the same note.
 *
 * Build: kernel-only Kbuild module, plain C (matches STGEnabler.c's own
 * documented reason for avoiding C++ against this ancient kernel's
 * headers). See Makefile.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/cpumask.h>
#include <linux/string.h>
#include <linux/param.h>
#include <linux/interrupt.h>
#include <asm/atomic.h>
#include <stdarg.h>

/* ========================================================================= *
 *  1. rt_task_* -- a "task" here is an ordinary kthread wrapped in a
 *     small private control block. See oa/src/init/rtwrap.cpp's own
 *     `rtwrap_pthread_create` for the exact real caller-side protocol this
 *     has to satisfy: it allocates the RT_TASK block itself (via
 *     rtheap_alloc, below), writes ITS OWN bookkeeping fields at
 *     task+0x08/+0x5b0/+0x5b4/+0x5b8 BEFORE calling rt_task_init(), and
 *     (only after rt_task_init() returns 0) writes task+0x580 and calls
 *     rt_task_resume() AFTER this function has already returned. This
 *     module's own private state therefore must not collide with any of
 *     those caller-owned offsets -- it is stashed at task+0x00, a field no
 *     confirmed real caller anywhere in this project ever reads or writes.
 * ========================================================================= */

#define RTV_TASK_MAGIC	0x52545631u	/* "RTV1" */

struct rtv_task {
	unsigned int		magic;
	struct task_struct	*kthread;
	void			(*entry)(long);
	long			data;
	unsigned int		priority;
	int			cpu;
	atomic_t		suspended;	/* 1 = not yet allowed to run entry() */
	int			stopping;
	struct completion	started;
};

static int rtv_thread_fn(void *arg)
{
	struct rtv_task *t = (struct rtv_task *)arg;

	complete(&t->started);

	/*
	 * Real RTAI tasks are created SUSPENDED; rt_task_resume() is what
	 * makes them runnable. kthread_create() already leaves a freshly
	 * created kthread parked (it only starts truly running after its
	 * first wake_up_process()), which lines up with that naturally --
	 * but stay cooperative here too in case rt_task_suspend() is ever
	 * called before the very first resume lands.
	 */
	while (!kthread_should_stop() && !t->stopping &&
	       atomic_read(&t->suspended)) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (!kthread_should_stop() && !t->stopping &&
		    atomic_read(&t->suspended))
			schedule_timeout(HZ / 10);
		set_current_state(TASK_RUNNING);
	}

	if (!kthread_should_stop() && !t->stopping)
		t->entry(t->data);

	/*
	 * Real RTAI task entry functions are conventionally infinite
	 * service loops, torn down via rt_task_delete() rather than by
	 * returning. Stay parked here if entry() ever does return anyway,
	 * so kthread_stop() (the only thing that should end this thread)
	 * still behaves correctly.
	 */
	while (!kthread_should_stop())
		schedule_timeout_interruptible(HZ);

	return 0;
}

static struct rtv_task *rtv_task_get(void *task)
{
	struct rtv_task *t;

	if (!task)
		return NULL;
	t = *(struct rtv_task **)task;
	if (!t || t->magic != RTV_TASK_MAGIC)
		return NULL;
	return t;
}

/*
 * rt_task_init(task, entry, data, stackSize, priority, usesFpu, signalFn) --
 * confirmed regparm(3) (register-passed), see reconstructed/OA/src/init/
 * rtwrap.cpp's own header comment. `task` is caller-allocated (via
 * rtheap_alloc, below) and is what `rtwrap_pthread_create` calls the
 * RT_TASK block; this substitute creates (but does not yet start) a real
 * kthread and stashes a pointer to its own private bookkeeping at
 * task+0x00. stackSize/usesFpu/signalFn are accepted for ABI fidelity but
 * not meaningfully used: a kthread already has its own private kernel
 * stack (not sized from `stackSize`), and there is no genuine hard-RT FPU
 * save/restore or asynchronous "task deleted" signal mechanism to model
 * in a plain kthread -- neither is read back by any confirmed real caller
 * in this project's own reconstruction, only rt_task_init()'s RETURN
 * VALUE is (see rtwrap_pthread_create's own real disassembly-derived
 * control flow).
 */
int rt_task_init(void *task, void (*entry)(long), long data,
		  unsigned int stackSize, unsigned int priority,
		  int usesFpu, void (*signalFn)(void))
{
	struct rtv_task *t;

	(void)stackSize;
	(void)usesFpu;
	(void)signalFn;

	if (!task || !entry)
		return -EINVAL;

	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (!t)
		return -ENOMEM;

	t->magic = RTV_TASK_MAGIC;
	t->entry = entry;
	t->data = data;
	t->priority = priority;
	t->cpu = -1;
	t->stopping = 0;
	atomic_set(&t->suspended, 1);
	init_completion(&t->started);

	t->kthread = kthread_create(rtv_thread_fn, t, "rtv_task/%p", task);
	if (IS_ERR(t->kthread)) {
		long err = PTR_ERR(t->kthread);
		kfree(t);
		return err ? (int)err : -EINVAL;
	}

	/*
	 * Publish. Safe: rtwrap_pthread_create() writes task+0x08/+0x5b0/
	 * +0x5b4/+0x5b8 BEFORE calling rt_task_init(), and only writes
	 * task+0x580 (and calls rt_task_resume()) AFTER rt_task_init()
	 * returns 0 -- task+0x00 is untouched by any of that.
	 */
	*(struct rtv_task **)task = t;

	return 0;
}
EXPORT_SYMBOL(rt_task_init);

/*
 * rt_task_delete(task) -- confirmed regparm(3) (register-passed). Tears
 * the kthread down and frees this module's own private state. Safe no-op
 * (returns 0) if `task` isn't one of our own tagged tasks -- this is
 * deliberate: rtwrap_pthread_cancel()'s own real body (rtwrap.cpp) calls
 * rt_task_delete(rt_whoami()) when given a NULL taskHandle, and
 * rt_whoami() (below) returns a static "Linux context" sentinel that was
 * never created via rt_task_init().
 */
int rt_task_delete(void *task)
{
	struct rtv_task *t;

	if (!task)
		return -EINVAL;

	t = rtv_task_get(task);
	if (!t)
		return 0;

	t->stopping = 1;
	atomic_set(&t->suspended, 0);
	if (t->kthread) {
		wake_up_process(t->kthread);
		kthread_stop(t->kthread);
	}

	*(struct rtv_task **)task = NULL;
	kfree(t);
	return 0;
}
EXPORT_SYMBOL(rt_task_delete);

/*
 * rt_task_resume(task) -- confirmed STACK-passed (regparm(0)), see
 * rtwrap.cpp. Marks the task runnable and wakes its kthread (the first
 * call is what actually starts it executing its entry() function).
 */
__attribute__((regparm(0))) int rt_task_resume(void *task)
{
	struct rtv_task *t = rtv_task_get(task);

	if (!t)
		return -EINVAL;

	atomic_set(&t->suspended, 0);
	if (t->kthread)
		wake_up_process(t->kthread);
	return 0;
}
EXPORT_SYMBOL(rt_task_resume);

/*
 * rt_task_suspend(task) -- confirmed STACK-passed (regparm(0)). Best-
 * effort only, and documented as such rather than silently assumed
 * complete: this substitute can gate a task BEFORE it first starts
 * running its entry() function (the suspended-check loop in
 * rtv_thread_fn above), but -- unlike genuine RTAI hard-real-time
 * preemption -- cannot forcibly interrupt an entry() already in the
 * middle of running; a plain Linux kthread has no external-preemption
 * primitive equivalent to real RTAI's. None of this project's own
 * reconstructed callers (rtwrap_task_suspend, used from audio_start.cpp)
 * are on the init/boot-path this module's own VM boot test is scoped to
 * validate, so this simplification does not affect that goal.
 */
__attribute__((regparm(0))) int rt_task_suspend(void *task)
{
	struct rtv_task *t = rtv_task_get(task);

	if (!t)
		return -EINVAL;

	atomic_set(&t->suspended, 1);
	return 0;
}
EXPORT_SYMBOL(rt_task_suspend);

/*
 * rt_whoami(void) -- confirmed REGISTER-passed (regparm(3), trivially --
 * zero arguments). Real RTAI: returns the calling context's own current
 * RT_TASK (or a sentinel identifying the Linux servant when called from
 * ordinary Linux context). Since this substitute never enters genuine
 * hard-RT context AT ALL, every caller is always "Linux context" by
 * construction -- always return a static sentinel whose word at +0x1c
 * equals RT_SCHED_LINUX_PRIORITY (0x7fffffff), matching
 * stg_is_linux_context()'s own confirmed real check
 * (reconstructed/OA/src/init/startup_helpers.cpp). Sized a full 0x600
 * bytes and zero-initialized otherwise so that rtwrap_pthread_cancel()'s
 * own real body -- which, given a NULL taskHandle, calls
 * rt_task_delete(rt_whoami()) and then unconditionally reads this same
 * pointer+0x5b8 as a "raw allocation to free" -- stays safely in-bounds:
 * rt_task_delete() recognizes this sentinel isn't one of our own tagged
 * tasks (magic check above) and is a no-op, and the +0x5b8 read is 0,
 * which rtheap_free() (below) treats as a safe no-op too.
 */
static unsigned char g_rtv_linux_ctx_task[0x600] __attribute__((aligned(8)));

void *rt_whoami(void)
{
	return g_rtv_linux_ctx_task;
}
EXPORT_SYMBOL(rt_whoami);

/*
 * rt_set_runnable_on_cpuid(task, cpuId) -- confirmed STACK-passed
 * (regparm(0)). Pins the underlying kthread to the given CPU via the
 * genuine GPL-only set_cpus_allowed_ptr() -- this module is itself
 * GPL-licensed (see bottom of file), so unlike the proprietary OA.ko
 * caller chain (which must go through STGEnabler.ko's stg_set_cpus_allowed
 * shim instead), it may call the real kernel primitive directly. Silently
 * ignored for an invalid task or out-of-range/offline cpuId, matching
 * this module's project-wide "never crash on a caller's malformed
 * argument" convention.
 */
__attribute__((regparm(0))) void rt_set_runnable_on_cpuid(void *task, unsigned int cpuId)
{
	struct rtv_task *t = rtv_task_get(task);

	if (!t)
		return;

	t->cpu = (int)cpuId;
	if (t->kthread && cpuId < (unsigned int)nr_cpu_ids && cpu_online(cpuId))
		set_cpus_allowed_ptr(t->kthread, cpumask_of(cpuId));
}
EXPORT_SYMBOL(rt_set_runnable_on_cpuid);

/*
 * clear_debug_traps_in_rt_task(task) -- confirmed REGISTER-passed
 * (regparm(3)). Real RTAI: clears hardware debug-register (DR0-DR7)
 * breakpoints previously installed in the target RT_TASK's own saved
 * register state (installed by the separate rtwrap_set_debug_traps_in_
 * rt_task() OA-internal helper, which is NOT one of this module's 26
 * target symbols -- it never calls out to real RTAI at all per this
 * project's own reconstruction, see oa_cpu_affinity.h). This substitute
 * never installs any hardware debug trap in the first place (no genuine
 * hard-RT debug facility is modeled here), so clearing one is always a
 * safe no-op -- the same "not needed for structural boot progress"
 * reasoning sec 10.185 already applies to audio DSP, extended here to a
 * debug-trap facility that has nothing to do with audio either.
 */
void clear_debug_traps_in_rt_task(void *task)
{
	(void)task;
}
EXPORT_SYMBOL(clear_debug_traps_in_rt_task);

/* ========================================================================= *
 *  2. rtheap_* / rtai_global_heap -- real RTAI's own real-time-safe
 *     allocator, confirmed REGISTER-passed (regparm(3)). A VM substitute
 *     does not need real-time allocation guarantees (sec 10.185's own
 *     "audio-DSP-grade fidelity is not the goal" reasoning applies
 *     equally to allocator determinism) -- plain kzalloc/kfree is a safe,
 *     adequate substitute. `heap` is accepted and ignored; every real
 *     caller in this project's own reconstruction only ever passes
 *     `&rtai_global_heap`'s own address through opaquely (rtwrap.cpp),
 *     never dereferences its contents.
 * ========================================================================= */

/*
 * rtai_global_heap -- confirmed a DATA symbol, not a function: every real
 * call site in rtwrap.cpp takes its ADDRESS (`&rtai_global_heap`) and
 * passes that straight through to rtheap_alloc()/rtheap_free() as an
 * opaque heap-descriptor pointer, never dereferencing its contents. Real
 * RTAI's own global-heap descriptor size/layout was not independently
 * determined by this project (rtwrap.cpp's own extern declares it only as
 * `unsigned char rtai_global_heap[1]`) -- matched here with a small,
 * concretely-sized array purely so the symbol exists and is safely
 * addressable; this substitute's own rtheap_alloc/rtheap_free never read
 * or write through it.
 */
unsigned char rtai_global_heap[4] __attribute__((aligned(4)));
EXPORT_SYMBOL(rtai_global_heap);

void *rtheap_alloc(void *heap, unsigned int size, int flags)
{
	(void)heap;
	(void)flags;

	/*
	 * Real RTAI hard-RT allocation contexts (where GFP_KERNEL would be
	 * unsafe) never arise here -- this substitute never enters genuine
	 * RTAI/I-pipe hard-RT context at all (see file header), which is
	 * exactly the class of hazard CLAUDE.md's own documented
	 * "filp_open/GFP_KERNEL blocked by RTAI in init_module context"
	 * finding warns about for the REAL stack. Plain GFP_KERNEL is safe.
	 */
	return kzalloc(size, GFP_KERNEL);
}
EXPORT_SYMBOL(rtheap_alloc);

void rtheap_free(void *heap, void *ptr)
{
	(void)heap;
	kfree(ptr);	/* kfree(NULL) is always a safe no-op. */
}
EXPORT_SYMBOL(rtheap_free);

/* ========================================================================= *
 *  3. rt_typed_sem_init / rt_sem_wait / rt_sem_wait_if / rt_sem_signal /
 *     rt_sem_delete -- confirmed STACK-passed (regparm(0)), see
 *     rtwrap.cpp. `sem` is caller-owned storage (embedded inside OA-
 *     internal structures sized for the real, much larger RTAI SEM
 *     struct) -- an ordinary `struct semaphore` (~20 bytes on 32-bit)
 *     placed there fits comfortably and needs no separate allocation.
 * ========================================================================= */

/*
 * rt_typed_sem_init(sem, value, type) -- real RTAI's own `type` selects
 * counting/binary/resource-semaphore semantics, and for the "resource
 * semaphore" kind (type==3) `value` is NOT a literal starting count in
 * ground truth -- reconstructed/OA/src/init/rtwrap.cpp's own
 * rtwrap_pthread_mutex_init() passes `mode` (a recursive/errorcheck
 * BEHAVIOR encoding, -1/0/1) through this same argument. Interpreting it
 * literally as a starting count would start a "normal" mutex (mode==0) at
 * count==0, i.e. already locked -- deadlocking the very first
 * rt_sem_wait()/pthread_mutex_lock() call on it. This substitute always
 * starts a type==3 semaphore AVAILABLE (count=1), matching real RTAI's
 * own "a freshly-initialized resource semaphore is unlocked" behavior;
 * the mode's recursive/errorcheck nuance itself is a deliberate,
 * documented simplification, not reproduced (see README.md). Every other
 * confirmed real call site (cond init: value=0, type=1; per-task sem:
 * value=0, type=5) passes a literal, safe-to-honor starting count
 * directly.
 */
__attribute__((regparm(0))) int rt_typed_sem_init(void *sem, int value, int type)
{
	int initcount;

	if (!sem)
		return -EINVAL;

	if (type == 3)
		initcount = 1;
	else
		initcount = (value > 0) ? value : 0;

	sema_init((struct semaphore *)sem, initcount);
	return 0;
}
EXPORT_SYMBOL(rt_typed_sem_init);

__attribute__((regparm(0))) int rt_sem_wait(void *sem)
{
	if (!sem)
		return -EINVAL;
	down((struct semaphore *)sem);
	return 0;
}
EXPORT_SYMBOL(rt_sem_wait);

/*
 * rt_sem_wait_if(sem) -- real RTAI: non-blocking wait, "try, and return
 * immediately if it would block." down_trylock() returns 0 on success
 * (lock acquired) and nonzero on failure (would block) -- translated here
 * to RTAI's own confirmed real polarity (rtwrap_pthread_mutex_destroy()'s
 * own real body, rtwrap.cpp: `if (rt_sem_wait_if(mutex) >= 0) { signal;
 * delete; }`, i.e. success is a non-negative return): 0 on success, -1 on
 * failure.
 */
__attribute__((regparm(0))) int rt_sem_wait_if(void *sem)
{
	if (!sem)
		return -EINVAL;
	return down_trylock((struct semaphore *)sem) ? -1 : 0;
}
EXPORT_SYMBOL(rt_sem_wait_if);

__attribute__((regparm(0))) int rt_sem_signal(void *sem)
{
	if (!sem)
		return -EINVAL;
	up((struct semaphore *)sem);
	return 0;
}
EXPORT_SYMBOL(rt_sem_signal);

/*
 * rt_sem_delete(sem) -- this substitute's rt_typed_sem_init() never
 * dynamically allocates anything beyond the caller-owned `sem` storage
 * itself (a plain placement sema_init()), so there is nothing to free --
 * a safe no-op, matching real RTAI's own common usage pattern of a
 * statically- or caller-allocated SEM struct.
 */
__attribute__((regparm(0))) int rt_sem_delete(void *sem)
{
	(void)sem;
	return 0;
}
EXPORT_SYMBOL(rt_sem_delete);

/* ========================================================================= *
 *  4. rt_request_srq / rt_free_srq / rt_pend_linux_srq -- real RTAI's
 *     "software request queue" mechanism, used to signal from hard-RT
 *     context back into normal Linux context. Since this substitute never
 *     runs anything in genuine hard-RT context to begin with, there is no
 *     real hard/soft context split left to preserve -- rt_pend_linux_srq()
 *     simply schedules the registered handler on a dedicated workqueue
 *     (always deferred to safe process context, regardless of the
 *     calling context, which if anything is CLOSER to real SRQ's own
 *     intent -- "defer a hard-RT-context call into normal Linux context"
 *     -- than a synchronous call would be). Confirmed REGISTER-passed
 *     (regparm(3)), see daemon_lifecycle.cpp's own header comment.
 * ========================================================================= */

#define RTV_MAX_SRQ	32

struct rtv_srq {
	int			in_use;
	void			(*handler)(void);
	struct work_struct	work;
};

static struct rtv_srq rtv_srq_table[RTV_MAX_SRQ];
static DEFINE_SPINLOCK(rtv_srq_lock);
static struct workqueue_struct *rtv_srq_wq;

static void rtv_srq_work_fn(struct work_struct *w)
{
	struct rtv_srq *s = container_of(w, struct rtv_srq, work);
	void (*h)(void);

	h = s->handler;
	if (h)
		h();
}

/*
 * rt_request_srq(label, handler, rt_handler) -- `label`/`rt_handler` are
 * accepted for ABI fidelity but unused (real RTAI's `label` is an
 * informational tag; `rt_handler`, the optional hard-RT-context callback,
 * has no meaning here since no hard-RT context ever exists). Returns a
 * 1-based slot index on success: every confirmed real caller
 * (SetupDaemon, daemon_lifecycle.cpp) checks `srq <= 0` for failure, so a
 * genuine allocation must always be > 0, matching real RTAI's own
 * "0 is reserved" convention.
 */
int rt_request_srq(unsigned int label, void (*handler)(void), void *rt_handler)
{
	unsigned long flags;
	int i;

	(void)label;
	(void)rt_handler;

	if (!handler)
		return -EINVAL;

	spin_lock_irqsave(&rtv_srq_lock, flags);
	for (i = 0; i < RTV_MAX_SRQ; i++) {
		if (!rtv_srq_table[i].in_use) {
			rtv_srq_table[i].in_use = 1;
			rtv_srq_table[i].handler = handler;
			spin_unlock_irqrestore(&rtv_srq_lock, flags);
			INIT_WORK(&rtv_srq_table[i].work, rtv_srq_work_fn);
			return i + 1;
		}
	}
	spin_unlock_irqrestore(&rtv_srq_lock, flags);
	return -EBUSY;
}
EXPORT_SYMBOL(rt_request_srq);

void rt_free_srq(unsigned int srq)
{
	unsigned long flags;
	int idx = (int)srq - 1;

	if (idx < 0 || idx >= RTV_MAX_SRQ)
		return;

	/*
	 * Guarantee no pending/running work item for this slot survives
	 * before the slot is recycled, mirroring real RTAI's own
	 * rt_free_srq() contract (the label is safe to reuse once it
	 * returns). Must be called before taking rtv_srq_lock below --
	 * cancel_work_sync() can sleep.
	 */
	cancel_work_sync(&rtv_srq_table[idx].work);

	spin_lock_irqsave(&rtv_srq_lock, flags);
	rtv_srq_table[idx].in_use = 0;
	rtv_srq_table[idx].handler = NULL;
	spin_unlock_irqrestore(&rtv_srq_lock, flags);
}
EXPORT_SYMBOL(rt_free_srq);

void rt_pend_linux_srq(unsigned int srq)
{
	int idx = (int)srq - 1;

	if (idx < 0 || idx >= RTV_MAX_SRQ || !rtv_srq_table[idx].in_use)
		return;

	queue_work(rtv_srq_wq, &rtv_srq_table[idx].work);
}
EXPORT_SYMBOL(rt_pend_linux_srq);

/* ========================================================================= *
 *  5. rtf_create / rtf_destroy / rtf_put_if -- real RTAI FIFOs, a fixed-
 *     capacity byte ring buffer per minor number, confirmed STACK-passed
 *     (regparm(0) / cdecl), see oa_rtfifo_init.h's own header comment
 *     (sec 10.90's real ABI-mismatch fix). A plain kzalloc'd ring buffer
 *     + spinlock is an adequate substitute -- OA.ko's own confirmed real
 *     usage (stg_rtfifo_init()/PushUnsolicitedMessage()) only needs
 *     create/destroy/non-blocking-put semantics, never a real /dev/rtfN
 *     userspace character-device interface.
 * ========================================================================= */

#define RTV_MAX_FIFO	32

struct rtv_fifo {
	int			created;
	unsigned int		size;
	unsigned char		*buf;
	unsigned int		head;
	unsigned int		count;
	spinlock_t		lock;
};

static struct rtv_fifo rtv_fifo_table[RTV_MAX_FIFO];

__attribute__((regparm(0))) int rtf_create(unsigned int fifo, int size)
{
	unsigned char *buf;

	if (fifo >= RTV_MAX_FIFO || size <= 0)
		return -EINVAL;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (rtv_fifo_table[fifo].created)
		kfree(rtv_fifo_table[fifo].buf);

	spin_lock_init(&rtv_fifo_table[fifo].lock);
	rtv_fifo_table[fifo].buf = buf;
	rtv_fifo_table[fifo].size = (unsigned int)size;
	rtv_fifo_table[fifo].head = 0;
	rtv_fifo_table[fifo].count = 0;
	rtv_fifo_table[fifo].created = 1;
	return 0;
}
EXPORT_SYMBOL(rtf_create);

/*
 * rtf_destroy(fifo) -- real RTAI rtf_destroy() is a safe no-op / returns
 * 0 even when the given minor was never created; OA.ko's own confirmed
 * real our_fifo_setup() unconditionally calls rtf_destroy() FIRST,
 * discarding the return value, specifically to tear down any stale FIFO
 * before (re)creating it (reconstructed/OA/src/init/rtfifo_init.cpp).
 */
__attribute__((regparm(0))) int rtf_destroy(unsigned int fifo)
{
	if (fifo >= RTV_MAX_FIFO)
		return -EINVAL;

	if (rtv_fifo_table[fifo].created) {
		kfree(rtv_fifo_table[fifo].buf);
		rtv_fifo_table[fifo].buf = NULL;
		rtv_fifo_table[fifo].created = 0;
	}
	return 0;
}
EXPORT_SYMBOL(rtf_destroy);

/*
 * rtf_put_if(fifo, buf, size) -- non-blocking, all-or-nothing write:
 * real RTAI's own "put IF there's room" semantics. Returns the number of
 * bytes written (== size) on success, 0 if there was insufficient free
 * space (matching real RTAI's own convention -- no partial writes).
 * OA.ko's only confirmed real caller (PushUnsolicitedMessage(),
 * src/engine/push_unsolicited_message.cpp) discards the return value
 * entirely, so this is verified correct by construction for that path;
 * implemented fully regardless, for any future caller.
 */
__attribute__((regparm(0))) int rtf_put_if(unsigned int fifo, const void *buf, int size)
{
	struct rtv_fifo *f;
	unsigned long flags;
	const unsigned char *src = (const unsigned char *)buf;
	unsigned int i, pos;

	if (fifo >= RTV_MAX_FIFO || size < 0 || (!buf && size > 0))
		return -EINVAL;

	f = &rtv_fifo_table[fifo];
	if (!f->created)
		return -EINVAL;

	spin_lock_irqsave(&f->lock, flags);
	if (f->count + (unsigned int)size > f->size) {
		spin_unlock_irqrestore(&f->lock, flags);
		return 0;
	}
	for (i = 0; i < (unsigned int)size; i++) {
		pos = (f->head + f->count) % f->size;
		f->buf[pos] = src[i];
		f->count++;
	}
	spin_unlock_irqrestore(&f->lock, flags);
	return size;
}
EXPORT_SYMBOL(rtf_put_if);

/* ========================================================================= *
 *  6. rt_request_irq / rt_assign_irq_to_cpu / rt_release_irq /
 *     rt_shutdown_irq / rt_startup_irq -- real RTAI IRQ-affinity/
 *     management primitives. Confirmed REGISTER-passed (regparm(3)),
 *     pure "arg never moved out of eax/edx" passthroughs in ground
 *     truth (rtwrap.cpp's own header comment) -- `rt_request_irq`
 *     itself confirmed via a direct disassembly of ground truth's own
 *     `rtwrap_request_irq` (a pure one-arg-marshalled forward to this
 *     exact symbol, MASTER_REFERENCE.md sec 10.237).
 *
 *     `rt_assign_irq_to_cpu`/`rt_shutdown_irq`/`rt_startup_irq` stay
 *     safe no-ops (matching this substitute's own established
 *     "-smp 1, no real hardware IRQ *affinity* to manage" rationale --
 *     CPU pinning and interrupt masking are optimizations, not
 *     correctness-load-bearing for anything this project's own boot
 *     sequence currently reaches).
 *
 *     `rt_request_irq`/`rt_release_irq` are NOT safe no-ops, unlike
 *     their siblings above: OA.ko's own real `CSTGComPort` UART driver
 *     (the keybed serial handshake, sec 10.237) has no polling fallback
 *     anywhere in its own confirmed reconstruction -- a byte the keybed
 *     board (real or, in this VM, `kronos-keybed-superio`'s companion
 *     UART) sends back can ONLY ever reach `CSTGComPort::
 *     OnByteReceived()` via a genuine firing interrupt. A no-op
 *     `rt_request_irq` (as this file used to treat the whole IRQ family)
 *     would make CSTGComPort::Initialize()'s own real `if
 *     (rtwrap_request_irq(...) != 0) return 0;` check fail 100% of the
 *     time, for EVERY caller of CSTGComPort, regardless of any hardware
 *     question -- confirmed live on `kronosvm` before this fix (boot
 *     reached `OA_DEBUG_MARKER 14` then `insmod: ... -1 Operation not
 *     permitted`, zero bytes ever transmitted since Initialize() failed
 *     before CSTGKeybedInterface_Startup ever queued the probe byte).
 *
 *     Implemented as a real Linux `request_irq()`/`free_irq()` wrapper,
 *     adapting the RTAI-style callback ABI (`void(*)(unsigned int,
 *     void*)`, no return value) to the kernel's own `irqreturn_t(int,
 *     void*)` handler signature via a small fixed-size table indexed by
 *     IRQ line. Always requests `IRQF_SHARED`: the keybed UART's own
 *     legacy ISA IRQ line (4, shared with COM1 in real PC IRQ routing)
 *     may legitimately already be in use by the kernel's own serial8250
 *     console driver, and ground truth's own real `rt_request_irq`
 *     4th argument (`flags`) is a confirmed literal 0 at
 *     CSTGComPort::Initialize()'s only real call site -- this
 *     substitute chooses the flag value itself rather than trusting a
 *     caller-supplied 0 (which would mean "not shared" and fail
 *     outright on a genuinely shared legacy IRQ line).
 * ========================================================================= */

#define RTAI_VIRT_MAX_IRQ 16
static void (*rtai_virt_irq_handler[RTAI_VIRT_MAX_IRQ])(unsigned int, void *);
static void *rtai_virt_irq_dev[RTAI_VIRT_MAX_IRQ];

static irqreturn_t rtai_virt_irq_trampoline(int irq, void *dev_id)
{
	if ((unsigned int)irq < RTAI_VIRT_MAX_IRQ && rtai_virt_irq_handler[irq]) {
		rtai_virt_irq_handler[irq]((unsigned int)irq, dev_id);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

int rt_request_irq(unsigned int irq, void (*handler)(unsigned int, void *), void *dev,
		    unsigned int flags)
{
	int ret;

	(void)flags;
	if (irq >= RTAI_VIRT_MAX_IRQ || !handler)
		return -EINVAL;

	rtai_virt_irq_handler[irq] = handler;
	rtai_virt_irq_dev[irq] = dev;
	ret = request_irq(irq, rtai_virt_irq_trampoline, IRQF_SHARED, "RTAIVirtualDriver", dev);
	if (ret) {
		rtai_virt_irq_handler[irq] = NULL;
		rtai_virt_irq_dev[irq] = NULL;
	}
	return ret;
}
EXPORT_SYMBOL(rt_request_irq);

int rt_assign_irq_to_cpu(unsigned int irq, unsigned int cpu)
{
	(void)irq;
	(void)cpu;
	return 0;
}
EXPORT_SYMBOL(rt_assign_irq_to_cpu);

int rt_release_irq(unsigned int irq)
{
	if (irq < RTAI_VIRT_MAX_IRQ && rtai_virt_irq_handler[irq]) {
		free_irq(irq, rtai_virt_irq_dev[irq]);
		rtai_virt_irq_handler[irq] = NULL;
		rtai_virt_irq_dev[irq] = NULL;
	}
	return 0;
}
EXPORT_SYMBOL(rt_release_irq);

int rt_shutdown_irq(unsigned int irq)
{
	(void)irq;
	return 0;
}
EXPORT_SYMBOL(rt_shutdown_irq);

int rt_startup_irq(unsigned int irq)
{
	(void)irq;
	return 0;
}
EXPORT_SYMBOL(rt_startup_irq);

/* ========================================================================= *
 *  7. rt_printk -- real RTAI's hard-RT-context-safe printk (buffers to a
 *     ring, flushed later, so it can be called somewhere a genuine
 *     printk() would not be safe). Confirmed STACK-passed/asmlinkage
 *     (regparm(0)) -- sec 10.87's own real ABI-mismatch fix. Since this
 *     substitute never runs anything in genuine hard-RT context, a
 *     trivial forward to the real kernel printk() is entirely safe.
 * ========================================================================= */

__attribute__((regparm(0))) void rt_printk(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vprintk(fmt, args);
	va_end(args);
}
EXPORT_SYMBOL(rt_printk);

/* ========================================================================= *
 *  8. BONUS, outside the official 26-symbol list: rt_linux_use_fpu /
 *     rt_set_oneshot_mode / start_rt_timer.
 *
 *     Discovered while working out this module's own live-boot load
 *     order, not part of OA.ko's own `nm -u` (these three are absent from
 *     it -- OA.ko never calls them directly). `STGEnabler.ko`
 *     (reconstructed/STGEnabler/STGEnabler.c, an existing, already-built
 *     companion module) has its own `init_module()`
 *     (`STGEnabler_init()`) that UNCONDITIONALLY calls `stg_rtai_setup()`,
 *     which calls all three of these real RTAI symbols directly. On real
 *     hardware this is fine (`STGEnabler.ko` always loads AFTER the real
 *     RTAI stack, per the established sec 10.208 load order). In THIS
 *     module's VM substitute configuration, where no real `rtai_*.ko` is
 *     loaded at all, `STGEnabler.ko` would otherwise fail its OWN insmod
 *     outright on these three unresolved symbols -- which would then
 *     cascade into every OTHER companion module that depends on
 *     `STGEnabler.ko`'s own `stg_*` exports (`stg_set_cpus_allowed`, etc),
 *     and ultimately into `OA.ko` itself. Providing trivial substitutes
 *     for these three here (and loading THIS module BEFORE
 *     `STGEnabler.ko` -- see README.md's corrected load order) closes
 *     that gap. No-ops are adequate for the same reason the IRQ-affinity
 *     family above are: there is no genuine RTAI hardware-timer to
 *     configure in this substitute at all.
 * ========================================================================= */

void rt_linux_use_fpu(int use_fpu)
{
	(void)use_fpu;
}
EXPORT_SYMBOL(rt_linux_use_fpu);

void rt_set_oneshot_mode(void)
{
}
EXPORT_SYMBOL(rt_set_oneshot_mode);

int start_rt_timer(int period)
{
	(void)period;
	return 0;
}
EXPORT_SYMBOL(start_rt_timer);

/* ========================================================================= *
 *  Module init / exit.
 * ========================================================================= */

static int __init RTAIVirtualDriverInit(void)
{
	printk(KERN_INFO "RTAIVirtualDriver: loading (software substitute for "
	       "rtai_hal.ko/rtai_sched.ko/rtai_sem.ko/rtai_fifos.ko/rtai_ndbg.ko/"
	       "rtai_debug.ko -- see README.md; NOT genuine hard-real-time, "
	       "VM/QEMU-TCG boot-testing use only)\n");

	/*
	 * The static rt_whoami() "Linux context" sentinel: only the word
	 * at +0x1c (RT_SCHED_LINUX_PRIORITY) is ever read by any confirmed
	 * real caller (stg_is_linux_context(), startup_helpers.cpp); the
	 * rest of the block is left zeroed.
	 */
	*(unsigned int *)(g_rtv_linux_ctx_task + 0x1c) = 0x7fffffffu;

	rtv_srq_wq = create_singlethread_workqueue("rtv_srq_wq");
	if (!rtv_srq_wq) {
		printk(KERN_ERR "RTAIVirtualDriver: workqueue alloc failed\n");
		return -ENOMEM;
	}

	return 0;
}

static void __exit RTAIVirtualDriverExit(void)
{
	int i;

	if (rtv_srq_wq) {
		flush_workqueue(rtv_srq_wq);
		destroy_workqueue(rtv_srq_wq);
	}

	for (i = 0; i < RTV_MAX_FIFO; i++) {
		if (rtv_fifo_table[i].created) {
			kfree(rtv_fifo_table[i].buf);
			rtv_fifo_table[i].buf = NULL;
			rtv_fifo_table[i].created = 0;
		}
	}

	printk(KERN_INFO "RTAIVirtualDriver: unloaded\n");
}

module_init(RTAIVirtualDriverInit);
module_exit(RTAIVirtualDriverExit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Software substitute for the real RTAI stack (stand-in for "
		    "rtai_hal.ko/rtai_sched.ko/rtai_sem.ko/rtai_fifos.ko/rtai_ndbg.ko/"
		    "rtai_debug.ko, for VM/QEMU-TCG boot testing where genuine RTAI "
		    "hard-real-time cannot be brought up -- see README.md)");
MODULE_AUTHOR("Korg (reconstructed)");
