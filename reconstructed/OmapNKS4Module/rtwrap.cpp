// SPDX-License-Identifier: GPL-2.0
/*
 * rtwrap.cpp  -  the "STG realtime wrapper" runtime layer.
 *
 * MAJOR FINDING (2026-07-17): this entire file's worth of functions
 * (`rtwrap_*`, `get_sizeof_*`, the C++ runtime shims, `CSTGThread`'s methods) were
 * previously assumed - by this project's own README and every other module's
 * documentation - to be part of the "shared STG framework" exported by
 * `STGEnabler.ko`/RTAI and merely *imported* by `OmapNKS4Module.ko`, not
 * reimplemented. That assumption is WRONG for this specific set of symbols: fresh
 * Ghidra analysis shows every `rtwrap_*` function and `CSTGThread::*` method has a
 * real, non-trivial size and a real address *inside* `OmapNKS4Module.ko` itself
 * (0x17f50-0x18c71 in the 89849-byte target), not a one-byte thunk/PLT-style entry
 * like the genuine externs (`rt_sem_wait`, `rt_task_init`, `printk`, etc., all at
 * 0x20000+, all exactly 1 byte in Ghidra's own function-size accounting - the
 * telltale sign of an unresolved-at-link-time symbol reference, not real code).
 *
 * The real picture: this whole "rtwrap" layer is a *statically-linked, per-module*
 * pthread-style API built directly on top of RTAI's own C primitives (`rt_task_init`,
 * `rt_typed_sem_init`, `rtheap_alloc`, ...) - almost certainly compiled from a shared
 * source file the Korg SDK links into every STG-based kernel module (OA.ko,
 * loadmod.ko, this one, ...) rather than a symbol genuinely imported at insmod time.
 * That explains why the project's various modules all show the *same* rtwrap_*
 * function names in their own symbol tables without ever importing them from
 * anywhere - they're each carrying their own private copy, not sharing one.
 *
 * This reconstruction should be revisited across the wider `kronosology/` project:
 * anywhere else "the stg_* / rtwrap_* layer is shared, so it's imported not
 * reimplemented" was assumed (e.g. this module's own README, `OA/`'s own docs) is
 * now suspect and worth re-checking the same way this file was found.
 */

#include "omapnks4_internal.h"

/* ========================================================================= *
 *  Real kernel/RTAI primitives this file's rtwrap_* layer sits directly on top
 *  of. Opaque-typed the same way this project already treats real kernel
 *  structs it can't parse (see usb.cpp/submit.cpp's own precedent) - `SEM`/
 *  `RT_TASK` are real RTAI structs this codebase never needs to lay out field-
 *  by-field, only pass pointers to/from.
 * ========================================================================= */
/* CORRECTION (re-verification pass, 2026-07-17): under this file's ambient
 * -mregparm=3 build flag, every extern in this block compiles to a
 * register-passed call unless individually overridden. Ground truth shows
 * that's WRONG for at least rt_sem_wait/rt_typed_sem_init/rt_task_resume/
 * rt_sem_broadcast/rt_sem_delete - the real RTAI primitives are called with
 * plain stack-passed (cdecl) arguments, confirmed via fresh disassembly at
 * their real call sites. rt_task_delete is the one exception actually
 * checked and confirmed to genuinely use EAX (regparm-style), so it's left
 * at the file's default. This was a real, SILENT ABI mismatch - it compiles
 * and links fine (only symbol names are checked at link time) but would
 * corrupt arguments at runtime, since the caller and callee would disagree
 * on where the argument actually is. `__attribute__((regparm(0)))` forces
 * genuine cdecl (no register args) for the functions confirmed to need it,
 * regardless of the file's own -mregparm=3.
 *
 * NOTE: this comment's own "at least" list above never actually got
 * rt_task_resume's declaration fixed below despite naming it - a real,
 * previously-unnoticed gap this same re-verification pass closed (see the
 * FOLLOW-UP below).
 *
 * FOLLOW-UP (adversarial re-verification pass #2, 2026-07-18): every
 * extern this comment's "NOT independently re-verified" paragraph used to
 * list has now actually been checked against its real call site(s) in this
 * exact 89849-byte OmapNKS4Module.ko (every rtwrap_* wrapper's own body was
 * disassembled and its outgoing argument setup - register MOV vs
 * `MOV [ESP+n],reg` stack store - read directly). Results:
 *
 *   NEEDED THE SAME regparm(0) FIX (confirmed stack-passed, ambient
 *   regparm(3) would have put them in registers - a real, silent ABI
 *   mismatch each, now fixed below):
 *     rt_sem_wait_if      - all 4 call sites (rtwrap_pthread_join/
 *                           mutex_destroy/barrier_destroy/cond_destroy)
 *                           push sem via `MOV [ESP],reg`.
 *     rt_sem_wait_timed   - rtwrap_pthread_mutex_lock_timed pushes ALL
 *                           THREE dwords (sem, delay_lo, delay_hi) to the
 *                           stack - notably even `sem` (a plain pointer
 *                           that would be register-eligible on its own)
 *                           is stack-passed, so this is genuine cdecl, not
 *                           just "64-bit arg forces stack from here on".
 *     rt_sem_wait_barrier - rtwrap_pthread_barrier_wait pushes sem to
 *                           [ESP].
 *     rt_sem_signal       - all 3 call sites (mutex_destroy/mutex_unlock/
 *                           cond_signal) push sem to [ESP].
 *     rt_cond_wait        - rtwrap_pthread_cond_wait pushes both cond and
 *                           mutex to [ESP]/[ESP+4].
 *     rt_cond_wait_timed  - rtwrap_rt_cond_wait_timed pushes all FOUR
 *                           dwords (cond, mutex, delay_hi, unused) to the
 *                           stack - again cond/mutex are stack-passed even
 *                           though they precede the 64-bit delay arg, so
 *                           this is full cdecl, not merely "64-bit-arg
 *                           fallout".
 *     rt_cond_wait_until  - rtwrap_pthread_cond_timedwait pushes all four
 *                           dwords (cond, mutex, until_lo, until_hi) to
 *                           the stack; same "even the leading pointers are
 *                           stack-passed" signature as rt_cond_wait_timed.
 *     rt_task_suspend     - rtwrap_task_suspend pushes task to [ESP].
 *     rt_task_resume      - rtwrap_task_resume (0x18970) AND
 *                           rtwrap_pthread_create's own rt_task_resume
 *                           call (0x183a1) BOTH push task to [ESP] - this
 *                           is the fix this comment's own paragraph above
 *                           already named but the declaration below never
 *                           received; now actually applied.
 *     rt_task_masked_unblock - rtwrap_task_wakeup_sleeping pushes task and
 *                           the literal mask (4) to [ESP]/[ESP+4].
 *     rt_get_priorities   - rtwrap_get_priorities pushes all three int*
 *                           pointers (max/min/rr) to [ESP]/[ESP+4]/[ESP+8].
 *     rt_get_time_cpuid   - rtwrap_rt_get_time_cpuid pushes the lone int
 *                           cpuid arg to [ESP] (would be EAX under plain
 *                           regparm(3)).
 *     start_rt_timer      - rtwrap_start_rt_timer pushes the lone int
 *                           period arg to [ESP].
 *     rt_set_runnable_on_cpuid - rtwrap_set_runnable_on_cpuid pushes both
 *                           task and cpumask to [ESP]/[ESP+4].
 *
 *   CONFIRMED ALREADY CORRECT AS AMBIENT regparm(3) (no override needed -
 *   either a clean register tail-forward, or a signature whose leading
 *   argument is a 64-bit `long long`, which GCC's regparm never assigns to
 *   registers in the first place, so plain ambient regparm(3) already
 *   compiles to the observed all-stack call with no explicit attribute
 *   needed):
 *     rt_task_init   - rtwrap_pthread_create's call passes task/fn/data in
 *                      EAX/EDX/ECX and stacksize/priority/uses_fpu/signal
 *                      on the stack in exactly that order - a textbook
 *                      regparm(3) 7-arg call, matching this file's
 *                      existing (unmarked) declaration exactly.
 *     rtheap_alloc   - rtwrap_pthread_create's own call (owner=EAX,
 *                      size=EDX=0x600,flags=ECX=0) independently confirms
 *                      the 3-arg regparm(3) shape directly from THIS
 *                      binary (previously only inferred by analogy from
 *                      OA.ko) - upgrades that finding from "borrowed" to
 *                      "directly verified".
 *     rtheap_free    - both call sites (rtwrap_pthread_cancel/join) pass
 *                      owner=EAX, p=EDX - confirmed 2-arg regparm(3).
 *     msleep         - all 3 call sites pass the ms count in EAX.
 *     rt_pend_linux_srq, rt_free_srq, rt_release_irq, rt_assign_irq_to_cpu,
 *     rt_startup_irq, rt_shutdown_irq - each wrapper is a zero-instruction
 *                      register tail-forward (no MOV at all between prologue
 *                      and CALL), meaning the incoming EAX/EDX register
 *                      arg(s) flow straight through unchanged - only
 *                      possible if the callee also expects register args.
 *     nano2count, nano2count_cpuid, rt_sleep - sole (or leading) argument
 *                      is a 64-bit `long long`; GCC's regparm attribute
 *                      never places a `long long` in a register on i386,
 *                      so the observed all-stack call sites are exactly
 *                      what plain ambient regparm(3) produces for these
 *                      signatures - no override needed (contrast with
 *                      rt_cond_wait_timed/_until/rt_sem_wait_timed above,
 *                      where the LEADING pointer args are ALSO stack-passed
 *                      - more than the 64-bit-arg fallout alone explains,
 *                      which is exactly why those three needed the explicit
 *                      fix and these three don't).
 *     rt_printk      - variadic; GCC never applies regparm to functions
 *                      with a `...` parameter, so this was never at risk.
 *
 *   INCONCLUSIVE / DEAD CODE - NOT fixed, flagged instead:
 *     rt_request_irq - rtwrap_request_irq has ZERO callers anywhere in
 *                      this binary (Ghidra confirms) and its own body only
 *                      forwards ONE value (loaded from its own [EBP+8], a
 *                      genuine incoming-stack-argument slot that shouldn't
 *                      exist at all if both of ITS OWN 2 params were
 *                      register-eligible under ambient regparm(3)) to a
 *                      single outgoing stack slot before calling
 *                      rt_request_irq - the second argument (`handler`) is
 *                      never set up at all. This is internally
 *                      inconsistent (neither "clean 2-arg register
 *                      tail-forward" nor "clean 2-arg cdecl forward") and,
 *                      being genuinely unreachable code, can't be
 *                      cross-checked against a second real call site the
 *                      way every fix above was. Left exactly as declared
 *                      (ambient regparm(3), 2 args) rather than guessed at;
 *                      this mirrors the project's own established
 *                      precedent of reproducing confirmed-dead code
 *                      faithfully instead of "fixing" something with no
 *                      way to validate the fix (see rtwrap_count2timespec's
 *                      infinite loop below, same reasoning).
 *
 * Also found and fixed in this same pass: rtwrap_pthread_join's poll loop
 * called `msleep(1)` in this reconstruction's source, but ground truth at
 * its real call site (0x18428) loads `MOV EAX,0xa` (10) before CALL msleep
 * - a plain value transcription bug, unrelated to calling convention,
 * corrected below where rtwrap_pthread_join is defined. */
extern "C" {
int  rt_task_init(void *task, void *fn, long data, int stack_size, int priority,
		   int uses_fpu, void (*signal)(void));
int  rt_typed_sem_init(void *sem, int value, int type) __attribute__((regparm(0)));
int  rt_sem_wait(void *sem) __attribute__((regparm(0)));
int  rt_sem_wait_if(void *sem) __attribute__((regparm(0)));
int  rt_sem_wait_timed(void *sem, long long delay) __attribute__((regparm(0)));
int  rt_sem_wait_barrier(void *sem) __attribute__((regparm(0)));
int  rt_sem_signal(void *sem) __attribute__((regparm(0)));
int  rt_sem_broadcast(void *sem) __attribute__((regparm(0)));
int  rt_sem_delete(void *sem) __attribute__((regparm(0)));
int  rt_cond_wait(void *cond, void *mutex) __attribute__((regparm(0)));
int  rt_cond_wait_timed(void *cond, void *mutex, long long delay, void *unused) __attribute__((regparm(0)));
int  rt_cond_wait_until(void *cond, void *mutex, long long until) __attribute__((regparm(0)));
int  rt_task_delete(void *task);
void rt_task_suspend(void *task) __attribute__((regparm(0)));
void rt_task_resume(void *task) __attribute__((regparm(0)));
int  rt_task_masked_unblock(void *task, unsigned int mask) __attribute__((regparm(0)));
void rt_sched_lock(void);
void rt_sched_unlock(void);
void *rt_whoami(void);
/* rt_request_irq: INCONCLUSIVE, left as ambient regparm(3) - see this
 * block's own header comment ("INCONCLUSIVE / DEAD CODE") for why this one
 * couldn't be confirmed either way. */
void rt_request_irq(unsigned int irq, void *handler);
void rt_release_irq(unsigned int irq);
void rt_assign_irq_to_cpu(unsigned int irq, unsigned long cpumask);
void rt_startup_irq(unsigned int irq);
void rt_shutdown_irq(unsigned int irq);
void rt_get_priorities(int *max, int *min, int *rr_quantum) __attribute__((regparm(0)));
long long rt_get_time_cpuid(int cpuid) __attribute__((regparm(0)));
long long nano2count_cpuid(long long nanos, int cpuid);
void start_rt_timer(int period) __attribute__((regparm(0)));
/* Return type corrected int (was void), 2026-07-18 CSTGThread pass: ground
 * truth @0x18220 leaves this external call's EAX result untouched all the
 * way through to rtwrap_set_debug_traps_in_rt_task's own RET, and
 * CSTGThread::CreateRealTimeWithCPUAffinity genuinely branches on that value
 * (@0x18bbb) - a real, load-bearing return, not dead register content. Not
 * otherwise touched: ground truth also shows this external called with
 * EDX=0x1b080 (a fixed internal buffer this file owns), which the current
 * zero-arg call site below doesn't reproduce - flagged here rather than
 * fixed, since no observed behavior depends on that buffer's contents and
 * it's outside this pass's CSTGThread-layout mandate. INDEPENDENTLY
 * RE-CONFIRMED (fresh full-body wrapper audit, 2026-07-18): raw bytes at
 * rtwrap_set_debug_traps_in_rt_task's own call site (0x18220: `ba 80 b0 01
 * 00` = `mov edx,0x1b080`, immediately before the call) reproduce this
 * exact finding byte-for-byte. Left unfixed for the same reason as before -
 * the buffer's real size/layout isn't independently known, and no branch in
 * this file depends on its contents, so any fabricated replacement buffer
 * would be an unverifiable guess rather than a real fix. */
int  set_debug_traps_in_rt_task(void);
void clear_debug_traps_in_rt_task(void);
void rt_printk(const char *fmt, ...);
/* CORRECTION (re-verification pass, 2026-07-17): both were declared/called
 * single-arg. Ground truth (rtwrap_malloc@0x180b0, rtwrap_pthread_create@
 * 0x182c0/0x18364) shows EAX is always a fixed constant (real address
 * 0x200ec) and the real size/pointer argument goes in EDX, i.e. these are
 * genuinely 2-argument functions, not 1-argument.
 *
 * FIX (goal: clean VM-bootable build, 2026-07-17): 0x200ec falls inside
 * this binary's own EXTERNAL segment (0x20000-0x2016f, confirmed via
 * Ghidra's own segment list for this .ko) - a genuinely external symbol,
 * not internal state, so leaving it as a plain dangling `extern` (no
 * definition anywhere) would be a real "Unknown symbol" insmod failure.
 * `&__this_module` (already declared in omapnks4_internal.h, already used
 * by this module's own USB driver registration in main.cpp) is the
 * leading candidate this comment already named, and it's the one real
 * kernel/Kbuild-generated symbol every module always has - defining
 * sRtheapOwnerToken as an initialized pointer to it resolves the link
 * without guessing at a fabricated address. */
void *sRtheapOwnerToken = &__this_module;

/* FIX (goal: clean VM-bootable build, 2026-07-17): the VM-substitute
 * implementation of this real RTAI primitive (RTAIVirtualDriver.ko, shared
 * infrastructure already relied on by OA.ko's own reconstruction) exports
 * `rtheap_alloc` as a real THREE-argument function - `(heap, size, flags)`,
 * regparm(3): EAX/EDX/ECX - established from OA.ko's own ground-truth call
 * sites, which this session's own re-verification pass didn't independently
 * re-check for a possible 3rd argument (it only confirmed EAX=owner-token
 * and EDX=size were real, not that ECX was absent). Rather than diverge
 * from shared, already-tested infrastructure, every call site below passes
 * the same 3-arg form, with 0 for `flags` (matching every real caller in
 * this reconstruction, none of which have a documented need for a non-zero
 * flags value). rtheap_free stays 2-arg, matching both this project's own
 * ground truth and RTAIVirtualDriver.ko's own real export. */
void *rtheap_alloc(void *owner, unsigned int size, int flags);
void rtheap_free(void *owner, void *p);
void schedule(void);
void msleep(unsigned int msecs);
void rt_set_runnable_on_cpuid(void *task, unsigned long cpumask) __attribute__((regparm(0)));
/* per-CPU RTAI hal state referenced directly by save/restore-flags below -
 * `per_cpu__cpu_number` is the standard kernel per-CPU current-processor-number
 * symbol (same %fs:-relative access pattern as `stg_get_current_task_nks4()`
 * elsewhere in this project); `rtai_cpu_lock` is RTAI's own per-CPU bitmask of
 * "Linux-mode-disabled" flags this pair of functions manipulates one bit at a
 * time (bit index = this CPU's number). */
extern int per_cpu__cpu_number;
/* CORRECTION (fresh disassembly pass, 2026-07-18): ground truth's `LOCK BTS`/
 * `LOCK BTR` against this symbol use a 32-bit register bit-index operand
 * (opcode `0F AB`/`0F B3`, no 0x66 size-override prefix - confirmed via raw
 * bytes at 0x18a6f/0x18abc/0x18adf: `f0 0f ab 15 78 00 02 00` etc.), i.e. this
 * is a real `unsigned long` (dword) bit-test target, not the `unsigned char`
 * this file previously declared. On this platform (Kronos: cpu index 0-3)
 * `cpu>>3`/`cpu&7` byte-array arithmetic and the CPU's real `cpu>>5`/`cpu&31`
 * dword-BTS addressing happen to compute the identical bit position (both
 * reduce to byte 0, bit `cpu`), so the old byte-sized reconstruction never
 * produced wrong RESULTS on this hardware - but the type was wrong and, more
 * importantly, the reconstruction performed the read-modify-write as a plain
 * non-atomic `*lockbyte |= ...`/`&= ~...` instead of the real atomic
 * LOCK BTS/BTR. Fixed below using the same `test_and_set_bit`/
 * `test_and_clear_bit` shape as Linux 2.6.32's own `arch/x86/include/asm/
 * bitops.h` (`LOCK_PREFIX "bts %2,%1\n\t sbb %0,%0"` / `"btr ..."` - matches
 * the disassembly instruction-for-instruction, including the SBB-based
 * old-bit extraction), hand-rolled locally rather than pulling in the real
 * kernel header (consistent with this module's established C++-header-
 * incompatibility workaround - see usb.cpp/submit.cpp's own precedent). */
extern unsigned long rtai_cpu_lock;

/* CORRECTION (fresh disassembly pass, 2026-07-18): this comment block already
 * (correctly) predicted `per_cpu__cpu_number` needs the same %fs:-relative
 * access as `stg_get_current_task_nks4_local()` below, but the two functions
 * that actually use it were never updated to match - they read it as a plain
 * global (`int cpu = per_cpu__cpu_number;`), which on this kernel's per-CPU
 * data model reads the wrong location entirely (a per-cpu symbol's link-time
 * value is an *offset* into the per-CPU area, meaningless as an absolute
 * address without the %fs segment override). Ground truth confirms real
 * `%fs:` prefixed loads at both call sites (`64 8b 15 f8 00 02 00` = FS-
 * override MOV EDX,[0x200f8] in rtwrap_global_save_flags_and_cli; `64 a1 f8
 * 00 02 00` = FS-override MOV EAX,[0x200f8] in rtwrap_global_restore_flags,
 * twice). Fixed below with a local helper mirroring
 * stg_get_current_task_nks4_local()'s own already-correct idiom. */
static inline int nks4_this_cpu_number(void)
{
	int cpu;
	asm volatile("mov %%fs:per_cpu__cpu_number, %0" : "=r"(cpu));
	return cpu;
}

/* Local re-implementations of the kernel's own atomic bit ops, matching
 * Linux 2.6.32 arch/x86/include/asm/bitops.h instruction-for-instruction
 * (verified against this binary's own disassembly - see the block comment
 * above rtwrap_global_save_flags_and_cli/_restore_flags below). Not pulled
 * in from a real kernel header for the same C++-parseability reason as the
 * rest of this file's hand-rolled kernel primitives. */
static inline int nks4_test_and_set_bit(int nr, volatile unsigned long *addr)
{
	int oldbit;
	asm volatile("lock; bts %2,%1\n\t"
		     "sbb %0,%0"
		     : "=r" (oldbit), "+m" (*addr)
		     : "Ir" (nr) : "memory");
	return oldbit;
}

static inline int nks4_test_and_clear_bit(int nr, volatile unsigned long *addr)
{
	int oldbit;
	asm volatile("lock; btr %2,%1\n\t"
		     "sbb %0,%0"
		     : "=r" (oldbit), "+m" (*addr)
		     : "Ir" (nr) : "memory");
	return oldbit;
}
/* Ground truth shows a 16-bit paired up/down counter (`_nano2count_cpuid`, two
 * bytes: an active-count byte and a generation/compare byte) gating whether
 * `nano2count_cpuid` (the RTAI mode-switch counter, NOT the function of the same
 * name above - same identifier, different real global, per Ghidra's own naming
 * collision here) needs to be re-armed. LOWER CONFIDENCE than the rest of this
 * file: the exact semantics of this specific counter pair are not fully pinned
 * down - modeled as faithfully as the decompile supports, flagged for anyone
 * revisiting RTAI mode-switch fidelity specifically. */
extern short _nano2count_cpuid;
}

static inline void *stg_get_current_task_nks4_local(void)
{
	void *current_task;
	asm volatile("mov %%fs:per_cpu__current_task, %0" : "=r"(current_task));
	return current_task;
}

/* ========================================================================= *
 *  C++ runtime shims - the non-array operator new(unsigned int)/delete(void*)
 *  are already defined in main.cpp (that file's own comment cites the same
 *  real import-list evidence); only the array forms were genuinely missing.
 *  stg_schedule is a thin wrapper over the real kernel `schedule()`.
 * ========================================================================= */
void *operator new[](unsigned int size) { return stg_kmalloc(size); }
void  operator delete[](void *p)        { stg_kfree(p); }

extern "C" void stg_schedule(void) { schedule(); }

/* ========================================================================= *
 *  rtwrap_pthread_* attr/mutex/cond/barrier opaque-struct sizes - real
 *  binary-confirmed constants, not guessed.
 * ========================================================================= */
extern "C" unsigned int get_sizeof_rt_task_struct(void)        { return 0x580; }
extern "C" unsigned int get_sizeof_pthread_barrier_t(void)     { return 0x30; }
extern "C" unsigned int get_sizeof_rtwrap_pthread_attr(void)   { return 0x10; }
extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void)  { return 0x30; }
extern "C" unsigned int get_sizeof_rtwrap_barrier(void)        { return 0x30; }
extern "C" unsigned int get_sizeof_rtwrap_pthread_cond(void)   { return 0x30; }
extern "C" int get_pthread_recursive_attr_constant(void)       { return 1; }

/* ========================================================================= *
 *  rtwrap_malloc/free - thin wrappers over the RT-safe heap allocator (a
 *  separate allocator from stg_kmalloc/kfree above - used for task control
 *  blocks that must be touched from real-time context).
 * ========================================================================= */
extern "C" void *rtwrap_malloc(unsigned int size) { return rtheap_alloc(sRtheapOwnerToken, size, 0); }
extern "C" void  rtwrap_free(void *p)              { rtheap_free(sRtheapOwnerToken, p); }

/* ========================================================================= *
 *  rtwrap_pthread_attr_* - a 16-byte attr block: [0]=stacksize (default 0x2000),
 *  [1]=has-rt-priority flag, [2]=default-something (1000000, likely a default
 *  period/quantum in ns - not independently confirmed), [3]=unused/reserved(1).
 * ========================================================================= */
extern "C" int rtwrap_pthread_attr_init(unsigned int *attr)
{
	attr[0] = 0x2000;
	attr[1] = 1;
	attr[2] = 1000000;
	attr[3] = 1;
	return 0;
}

extern "C" int rtwrap_pthread_attr_destroy(void *attr) { (void)attr; return 0; }

/* Ground truth: attr[1]=1 (has-rt-priority flag set unconditionally), attr[3]
 * (RTAI priority, lower=more urgent) = rtwrap_get_max_priority()-priority,
 * range-checked against [1,140] (0x8c=140, this platform's RTAI priority floor -
 * matches rtwrap_get_max_priority()'s own literal 0x8c). -EINVAL (0xffffffea)
 * for an out-of-range priority. */
extern "C" int rtwrap_pthread_attr_setrtpriority(unsigned int *attr, unsigned int priority)
{
	attr[1] = 1;
	if (priority - 1 < 0x8c) {
		attr[3] = 0x8c - priority;
		return 0;
	}
	return (int)0xffffffea;
}

extern "C" int rtwrap_pthread_attr_setstacksize(unsigned int *attr, unsigned int size)
{
	attr[0] = size;
	return 0;
}

/* ========================================================================= *
 *  rtwrap_pthread_create/join/cancel - the real, full RT-task-plus-completion-
 *  semaphore layer CreateRealTimeWithCPUAffinity (command.cpp^H^Hrealtime.cpp)
 *  routes through instead of a plain kernel_thread(). A single rt_task_struct
 *  (0x580 bytes, per get_sizeof_rt_task_struct above) is heap-allocated via
 *  rtheap_alloc, rounded up to a 64-byte boundary, with a private layout this
 *  file (and only this file) needs to know:
 *    +0x580  join semaphore (SEM, used as a one-shot "task has exited" latch)
 *    +0x5b0  the user fn pointer  (called by posix_wrapper_fun's trampoline)
 *    +0x5b4  the user arg pointer
 *    +0x5b8  the raw rtheap_alloc() pointer (for rtheap_free on join/cancel)
 *    +0x008  RTAI's own real RT_TASK fields start here (opaque to this file
 *            except for the two get_/rtwrap_get_* accessor offsets already
 *            reconstructed as free rtwrap_get_* functions above)
 * ========================================================================= */
extern "C" void posix_wrapper_fun(unsigned char *task);

extern "C" int rtwrap_pthread_create(void **out_task, unsigned int *attr,
				      void (*fn)(void *), void *arg)
{
	/* CORRECTION (re-verification pass, 2026-07-17): allocation size was
	 * 0x580+0x40=0x5c0. Ground truth passes 0x600 (EDX=0x600 at the real
	 * call site). Given the real task struct's fields extend to task+0x5bc
	 * plus up to 0x3f bytes of the alignment slop the `& ~0x3f` rounding
	 * below can consume, 0x5c0 under-allocated by up to 0x40 bytes - a real
	 * heap overrun risk, not just an imprecise round number. */
	void *raw = rtheap_alloc(sRtheapOwnerToken, 0x580 + 0x80, 0);
	if (!raw)
		return (int)0xfffffff4;	/* -ENOMEM */

	unsigned char *task = (unsigned char *)(((unsigned int)raw + 0x40) & ~0x3f);
	*(void **)(task + 0x5b8) = raw;
	*(void (**)(void))(task + 0x5b0) = fn;
	*(void **)(task + 0x5b4) = arg;
	*(int *)(task + 8) = 0;

	unsigned int stacksize = 0x2000;
	int priority = 0x3fffffff;
	if (attr) {
		priority = (int)attr[3];
		stacksize = attr[0];
	}

	int rc = rt_task_init(task, (void *)posix_wrapper_fun, (long)task,
			       (int)stacksize, priority, 1, 0);
	if (rc != 0) {
		rtheap_free(sRtheapOwnerToken, raw);
		return rc;
	}
	rt_typed_sem_init(task + 0x580, 0, 5 /* RES_SEM (binary, priority-inherit) */);
	rt_task_resume(task);
	*out_task = task;
	return 0;
}

/* The real RT-task trampoline: call the user fn, then release the join
 * semaphore and tear it down (so a subsequent rtwrap_pthread_join can only ever
 * observe it post-exit - it's created fresh per task, never reused).
 * CORRECTION (re-verification pass, 2026-07-17): the user fn was previously
 * called with no argument at all. Ground truth loads the stored arg
 * (task+0x5b4, set correctly by rtwrap_pthread_create) into EAX and calls
 * fn(arg) - the arg was being stored correctly but never actually
 * delivered, the same "parameter looks forwarded but isn't" pattern found
 * elsewhere in this project's re-verification pass. */
extern "C" void posix_wrapper_fun(unsigned char *task)
{
	(*(void (**)(void *))(task + 0x5b0))(*(void **)(task + 0x5b4));
	rt_sem_broadcast(task + 0x580);
	rt_sem_delete(task + 0x580);
}

/* Ground truth: dispatches on whether the CALLER is itself an RT task
 * (rt_whoami()'s own priority field == 0x7fffffff sentinel = "the Linux
 * kernel/non-RT context", per this file's own rtwrap_pthread_create priority
 * default above using that exact sentinel) - a non-RT (Linux-context) caller
 * must poll rt_sem_wait_if + msleep rather than block in rt_sem_wait, since a
 * real blocking RTAI semaphore wait from plain kernel/Linux context isn't
 * valid. Either way, once the task's exit is observed, its exit code (if the
 * caller wants it) is read from +0x424 (an RTAI RT_TASK convention this file
 * doesn't otherwise need), then the task is deleted and its heap block freed. */
extern "C" int rtwrap_pthread_join(unsigned char *task, void *exit_code_out)
{
	int rc;
	void *self = rt_whoami();

	if (*(int *)((unsigned char *)self + 0x1c) == 0x7fffffff) {
		/* CORRECTION (re-verification pass #2, 2026-07-18): ground truth
		 * at this poll loop's real call site (0x18428) loads
		 * MOV EAX,0xa before CALL msleep - msleep(10), not msleep(1). */
		while ((rc = rt_sem_wait_if(task + 0x580)) < 1)
			msleep(10);
	} else {
		rc = rt_sem_wait(task + 0x580);
	}
	if (exit_code_out)
		*(unsigned int *)exit_code_out = *(unsigned int *)(task + 0x424);

	int del_rc = rt_task_delete(task);
	rtheap_free(sRtheapOwnerToken, *(void **)(task + 0x5b8));
	return rc != 0 ? rc : del_rc;
}

extern "C" int rtwrap_pthread_cancel(unsigned char *task)
{
	if (!task)
		task = (unsigned char *)rt_whoami();
	int rc = rt_task_delete(task);
	rtheap_free(sRtheapOwnerToken, *(void **)(task + 0x5b8));
	return rc;
}

/* ========================================================================= *
 *  rtwrap_init_stack_for_depth_measurement / rtwrap_measure_stack_surplus -
 *  a classic "paint the stack with a canary byte, then measure how far the
 *  canary survives" high-water-mark stack usage diagnostic. Poisons every
 *  byte below the task's current stack pointer (minus a 1KB safety margin)
 *  with 0xf5, logs it via rt_printk; the surplus measurement counts
 *  contiguous 0xf5 bytes from the bottom to estimate how much stack margin is
 *  left unused. RT_TASK field offsets: +0x00 = current top-of-stack pointer,
 *  +0x18 = stack base, +0x20 = priority (both already used elsewhere in this
 *  file's own rtwrap_get_* accessors, consistent numbering).
 * ========================================================================= */
extern "C" void rtwrap_init_stack_for_depth_measurement(unsigned int *task)
{
	unsigned char *base = (unsigned char *)task[6];
	unsigned int size = task[0] - (unsigned int)base;

	rt_printk("rtwrap_init_stack_for_depth_measurement() thread %p (pri %d) stack @%p-%p sz:%u\n",
		  task, task[8], base, task[0], size);

	unsigned int poison_len = size - 0x400;
	for (unsigned int i = 0; i < poison_len; i++)
		base[i] = 0xf5;
}

extern "C" unsigned int rtwrap_measure_stack_surplus(unsigned int *task)
{
	unsigned char *base = (unsigned char *)task[6];
	unsigned int surplus = 0;

	if ((unsigned char *)task[0] == base)
		return 0;
	if (base[0] != 0xf5)
		return 0;
	unsigned int limit = task[0] - (unsigned int)base;
	while (base[surplus] == 0xf5) {
		surplus++;
		if (surplus >= limit)
			break;
	}
	return surplus;
}

/* ========================================================================= *
 *  rtwrap_pthread_mutexattr_* - a 1-word attr (bit-packed type in the low 3
 *  bits: 0=normal/fast, 1=recursive, 2=errorcheck; PTHREAD_MUTEX_* numbering
 *  matches glibc's own convention, reused here since the caller-visible
 *  constant (get_pthread_recursive_attr_constant()==1) matches PTHREAD_MUTEX_
 *  RECURSIVE's usual value 1 in that same convention).
 * ========================================================================= */
extern "C" int rtwrap_pthread_mutexattr_init(unsigned int *attr) { *attr = 1; return 0; }
extern "C" int rtwrap_pthread_mutexattr_destroy(void *attr) { (void)attr; return 0; }

extern "C" int rtwrap_pthread_mutexattr_settype(unsigned int *attr, int type)
{
	switch (type) {
	case 0: *attr = (*attr & ~7u) | 1; return 0;	/* normal/fast    */
	case 1: *attr = (*attr & ~7u) | 4; return 0;	/* recursive      */
	case 2: *attr = (*attr & ~7u) | 2; return 0;	/* errorcheck     */
	default: return (int)0xffffffea;		/* -EINVAL        */
	}
}

/* ========================================================================= *
 *  rtwrap_pthread_mutex_* - built directly on an RTAI counting semaphore
 *  (RES_SEM/priority-inherit if the recursive/errorcheck bit is clear and not
 *  the "no priority inherit" bit either - see the value>>1 encoding below).
 * ========================================================================= */
/* BUG FIX (fresh disassembly pass, 2026-07-18): the sem_type polarity below
 * was INVERTED. Ground truth @0x185f7-0x18604 (`and edx,2; cmp edx,1; sbb
 * ecx,ecx; and ecx,2; sub ecx,1`) computes: `(*attr & 2) == 0 -> sem_type=1`,
 * `(*attr & 2) != 0 -> sem_type=-1` - i.e. RECURSIVE (settype's case 1, which
 * clears bit1 while setting bit2=4, so `*attr & 2` reads 0) gets +1, and
 * ERRORCHECK (settype's case 2, sets bit1=2) gets -1. The previous
 * reconstruction had `(*attr & 2) ? 1 : -1`, exactly backwards. Verified via
 * raw bytes, not decompiler pseudo-c alone (the pseudo-c's branchless bit
 * arithmetic independently reduces to the same polarity, cross-checked). */
extern "C" int rtwrap_pthread_mutex_init(void *mutex, unsigned int *attr)
{
	int sem_type = 0;
	if (attr && !(*attr & 1))
		sem_type = (*attr & 2) ? -1 : 1;
	rt_typed_sem_init(mutex, sem_type, 3 /* PRIO_Q, RES_SEM-style */);
	return 0;
}

extern "C" int rtwrap_pthread_mutex_destroy(void *mutex)
{
	int rc = rt_sem_wait_if(mutex);
	if (rc < 0)
		return (int)0xfffffff0;	/* -EBUSY: someone still holds it */
	rt_sem_signal(mutex);
	rt_sem_delete(mutex);
	return 0;
}

extern "C" int rtwrap_pthread_mutex_lock_timed(void *mutex, long long delay, void *unused)
{
	(void)unused;
	int rc = rt_sem_wait_timed(mutex, delay);
	return (rc < 0xfffe) ? 0 : (int)0xffffffff;
}

extern "C" int rtwrap_pthread_mutex_lock(void *mutex)
{
	int rc = rt_sem_wait(mutex);
	return (rc > 0xfffd) ? (int)0xffffffea : 0;
}

extern "C" unsigned int rtwrap_pthread_mutex_unlock(void *mutex)
{
	int rc = rt_sem_signal(mutex);
	return ((unsigned int)rc >> 31) & 0xffffffea;
}

/* ========================================================================= *
 *  rtwrap_pthread_barrier_* - another RTAI semaphore, initialized with the
 *  participant count as its value and a "barrier" semaphore type (2).
 * ========================================================================= */
extern "C" int rtwrap_pthread_barrier_init(void *barrier, int count)
{
	if (count == 0)
		return (int)0xffffffea;	/* -EINVAL */
	rt_typed_sem_init(barrier, count, 2 /* barrier semaphore */);
	return 0;
}

/* BUG FIX (fresh disassembly pass, 2026-07-18): the rc==5 polarity below was
 * INVERTED. Ground truth @0x1875a-0x18769 preloads EAX=-EINVAL(0xffffffea),
 * then `cmp edx,5; mov edx,0; cmovnz eax,edx` - CMOVNZ only fires when
 * ZF==0, i.e. when rt_sem_delete()'s return value is NOT 5, at which point
 * EAX is overwritten with 0 (success); when it IS 5, the CMOVNZ is skipped
 * and EAX keeps the preloaded -EINVAL. I.e. `rc == 5` is the FAILURE case,
 * not the success case - the previous reconstruction had `(rc == 5) ? 0 :
 * -EINVAL`, exactly backwards. */
extern "C" int rtwrap_pthread_barrier_destroy(void *barrier)
{
	int rc = rt_sem_wait_if(barrier);
	if (rc < 0)
		return (int)0xfffffff0;	/* -EBUSY */
	rc = rt_sem_delete(barrier);
	return (rc == 5) ? (int)0xffffffea : 0;
}

extern "C" void rtwrap_pthread_barrier_wait(void *barrier) { rt_sem_wait_barrier(barrier); }

/* ========================================================================= *
 *  rtwrap_pthread_cond_* - an RTAI semaphore used as a plain condvar (value 0,
 *  a non-counting "gate").
 * ========================================================================= */
extern "C" int rtwrap_pthread_cond_init(void *cond)
{
	rt_typed_sem_init(cond, 0, 1 /* CNT_S, FIFO-queued */);
	return 0;
}

extern "C" int rtwrap_pthread_cond_destroy(void *cond)
{
	int rc = rt_sem_wait_if(cond);
	if (rc < 0)
		return (int)0xfffffff0;	/* -EBUSY */
	rt_sem_delete(cond);
	return 0;
}

extern "C" void rtwrap_pthread_cond_signal(void *cond)    { rt_sem_signal(cond); }
extern "C" void rtwrap_pthread_cond_broadcast(void *cond) { rt_sem_broadcast(cond); }
extern "C" void rtwrap_pthread_cond_wait(void *cond, void *mutex) { rt_cond_wait(cond, mutex); }

extern "C" int rtwrap_pthread_cond_timedwait(void *cond, void *mutex, long *abstime /* {sec, nsec} */)
{
	long long until = nano2count((long long)abstime[0] * 1000000000LL + abstime[1]);
	int rc = rt_cond_wait_until(cond, mutex, until);
	return (rc > 0xfffd) ? (int)0xffffff92 /* -ETIMEDOUT */ : 0;
}

extern "C" void rtwrap_rt_cond_wait_timed(void *cond, void *mutex, long long delay, void *unused)
{
	rt_cond_wait_timed(cond, mutex, delay, unused);
}

/* ========================================================================= *
 *  rtwrap_sched_lock/unlock, rtwrap_whoami, rtwrap_task_suspend/resume,
 *  rtwrap_sleep, rtwrap_task_wakeup_sleeping/is_task_sleeping - thin 1:1
 *  wrappers over the matching real RTAI primitives; the two raw-offset
 *  accessors read real (opaque) RT_TASK fields already established by this
 *  file's rtwrap_get_* family (+0x58 = suspend depth, +0xc bit 2 = "sleeping"
 *  state flag).
 * ========================================================================= */
extern "C" void  rtwrap_sched_lock(void)              { rt_sched_lock(); }
extern "C" void  rtwrap_sched_unlock(void)             { rt_sched_unlock(); }
extern "C" void *rtwrap_whoami(void)                   { return rt_whoami(); }
extern "C" void  rtwrap_task_suspend(void *task)       { rt_task_suspend(task); }
extern "C" void  rtwrap_task_resume(void *task)        { rt_task_resume(task); }
extern "C" unsigned int rtwrap_get_task_suspend_depth(unsigned char *task)
{
	return *(unsigned int *)(task + 0x58);
}
extern "C" void  rtwrap_sleep(long long count)         { rt_sleep(count); }
/* mask=4 - RTAI's own SEMAPHORE/"sleep" block-reason bit (matches
 * rtwrap_is_task_sleeping's own bit-2 check below). */
extern "C" void  rtwrap_task_wakeup_sleeping(void *task) { rt_task_masked_unblock(task, 4); }
extern "C" bool  rtwrap_is_task_sleeping(unsigned char *task)
{
	return (*(unsigned int *)(task + 0xc) & 4) != 0;
}

/* ========================================================================= *
 *  rtwrap_get_* raw RT_TASK-field accessors (all confirmed by real disassembly
 *  citations elsewhere in this file/project - repeated here as the single
 *  place that owns the RT_TASK offset knowledge this whole file depends on).
 * ========================================================================= */
extern "C" int  rtwrap_get_runnable_on_cpuid(unsigned char *task) { return *(int *)(task + 0x14); }
extern "C" int  rtwrap_get_sched_policy(unsigned char *task)      { return *(int *)(task + 0x24); }
extern "C" int  rtwrap_get_rr_quantum(unsigned char *task)        { return *(int *)(task + 0x50); }
extern "C" int  rtwrap_get_max_priority(void) { return 0x8c; /* 140 - this platform's RTAI priority floor */ }
extern "C" int  rtwrap_get_min_priority(void) { return 1; }
extern "C" void rtwrap_get_priorities(int *max, int *min, int *rr) { rt_get_priorities(max, min, rr); }
extern "C" void rtwrap_set_runnable_on_cpuid(void *task, unsigned long cpumask)
{
	rt_set_runnable_on_cpuid(task, cpumask);
}
extern "C" int  rtwrap_set_debug_traps_in_rt_task(void)   { return set_debug_traps_in_rt_task(); }
extern "C" void rtwrap_clear_debug_traps_in_rt_task(void) { clear_debug_traps_in_rt_task(); }
extern "C" void rtwrap_start_rt_timer(int period)         { start_rt_timer(period); }
extern "C" long long rtwrap_nano2count(long long nanos)   { return nano2count(nanos); }
extern "C" long long rtwrap_nano2count_cpuid(long long nanos, int cpuid)
{
	return nano2count_cpuid(nanos, cpuid);
}
extern "C" long long rtwrap_rt_get_time_cpuid(int cpuid) { return rt_get_time_cpuid(cpuid); }
/* Ground truth: the real binary's implementation is a bare infinite empty loop
 * (`do {} while(true)` with zero body) - i.e. genuinely never returns. This is
 * not a transcription gap; it's a real characteristic of the stock module
 * (likely dead/unfinished code Korg never completed or never actually calls -
 * no xref to this function was found anywhere else in the binary). Reproduced
 * faithfully rather than "fixed", since calling it for real would hang whatever
 * called it either way. */
extern "C" void rtwrap_count2timespec(void *count, void *timespec_out)
{
	(void)count; (void)timespec_out;
	for (;;) { }
}

/* ========================================================================= *
 *  rtwrap_request_irq/release_irq/assign_irq_to_cpu/startup_irq/shutdown_irq/
 *  pend_linux_srq - thin 1:1 wrappers over the matching RTAI primitives.
 * ========================================================================= */
extern "C" void rtwrap_request_irq(unsigned int irq, void *handler) { rt_request_irq(irq, handler); }
extern "C" void rtwrap_release_irq(unsigned int irq)                { rt_release_irq(irq); }
extern "C" void rtwrap_assign_irq_to_cpu(unsigned int irq, unsigned long cpumask)
{
	rt_assign_irq_to_cpu(irq, cpumask);
}
extern "C" void rtwrap_startup_irq(unsigned int irq)  { rt_startup_irq(irq); }
extern "C" void rtwrap_shutdown_irq(unsigned int irq) { rt_shutdown_irq(irq); }
extern "C" void rtwrap_pend_linux_srq(int srq)        { rt_pend_linux_srq(srq); }

/* ========================================================================= *
 *  rtwrap_global_save_flags_and_cli / rtwrap_global_restore_flags - RESOLVED
 *  (fresh disassembly pass, 2026-07-18, closing the item this project's
 *  2026-07-17 re-verification pass had explicitly left open). Both functions
 *  were fully re-disassembled byte-for-byte via `read_memory` (the MCP
 *  server's `get_disassembly` tool was unreliable for this address range -
 *  raw bytes were pulled and hand-decoded instead, cross-checked against the
 *  instructions `get_disassembly` DID manage to return along the way - full
 *  agreement everywhere both were available).
 *
 *  THE MECHANISM, now fully pinned down: this is a two-level lock -
 *   - `rtai_cpu_lock` (a real `unsigned long` bitmask, one bit per CPU,
 *     `test_and_set_bit(cpu, &rtai_cpu_lock)` / `test_and_clear_bit(...)`)
 *     is a per-CPU RECURSION GUARD: only the outermost
 *     save/restore on a given CPU actually touches the second level below;
 *     nested calls on the same CPU no-op straight through.
 *   - `_nano2count_cpuid` (the real RTAI mode-switch counter this file
 *     already declared, a 16-bit `[serving_lo][ticket_hi]` pair - Ghidra's
 *     separate `nano2count_cpuid` byte-symbol the previous pass suspected
 *     was "a second, distinctly-named global" is in fact THE SAME STORAGE,
 *     Ghidra's own low-byte alias for this identical 16-bit variable, per
 *     that decompile's own "Globals starting with '_' overlap smaller
 *     symbols at the same address" warning) backs a genuine cross-CPU
 *     TICKET SPINLOCK: `LOCK XADD word ptr [_nano2count_cpuid], 0x100` hands
 *     out the next ticket (pre-increment high byte) and returns the old
 *     pair; the caller spins (PAUSE + re-read the live low/"serving" byte)
 *     until serving==ticket. This serializes the handful of CPUs that are
 *     simultaneously the *outermost* holder on their own CPU into the
 *     global-cli critical section one at a time.
 *
 *  Exact ground truth per function (raw bytes, `read_memory` @0x18a60/@0x18aa0):
 *   rtwrap_global_save_flags_and_cli (0x18a60-0x18a9f, 64 bytes):
 *     9c 58 fa 25 00 02 00 00      pushf; pop eax; cli; and eax,0x200
 *     64 8b 15 f8 00 02 00         mov edx, fs:[per_cpu__cpu_number]
 *     f0 0f ab 15 78 00 02 00     lock bts [rtai_cpu_lock], edx
 *     19 d2 85 d2 74 03           sbb edx,edx; test edx,edx; jz +3 (0x18a80)
 *     c3                          ret                    ; nested: return as-is
 *     ba 00 01 00 00              mov edx,0x100          ; (0x18a80, ticket path)
 *     f0 66 0f c1 15 7c 00 02 00 lock xadd [_nano2count_cpuid], dx
 *     38 f2 74 0a                 cmp dl,dh; jz +0xa (0x18a9c)
 *     f3 90 8a 15 7c 00 02 00 eb f2   pause; mov dl,[_nano2count_cpuid_lo]; jmp back
 *     83 c8 01 c3                 or eax,1; ret
 *   rtwrap_global_restore_flags (0x18aa0-0x18b1b, ~124 bytes):
 *     8d 64 24 f0 89 44 24 0c     lea esp,[esp-0x10]; mov [esp+0xc],eax (save param)
 *     f0 0f ba 74 24 0c 00        lock btr [esp+0xc],0   ; test+clear local bit0
 *     19 c0 85 c0 74 23           sbb eax,eax; test eax,eax; jz +0x23 (0x18ad8)
 *     -- param&1 != 0 (this call was the OUTER save()): release --
 *     fa 64 a1 f8 00 02 00        cli; mov eax,fs:[per_cpu__cpu_number]
 *     f0 0f b3 05 78 00 02 00     lock btr [rtai_cpu_lock],eax
 *     19 c0 85 c0 75 46           sbb eax,eax; test eax,eax; jnz +0x46 (0x18b10)
 *     8b 4c 24 0c 85 c9 74 01 fb  mov ecx,[esp+0xc]; test ecx,ecx; jz+1; sti
 *     8d 64 24 10 c3              lea esp,[esp+0x10]; ret     (shared tail)
 *     fe 05 7c 00 02 00 eb b2     inc byte [_nano2count_cpuid_lo]; jmp shared tail
 *                                 (0x18b10 - note: PLAIN inc, no lock prefix -
 *                                 safe because the ticket lock already ensures
 *                                 at most one CPU is ever in this branch, and
 *                                 the CLI above blocks same-CPU reentrancy)
 *     -- param&1 == 0 (nested call on this CPU): re-acquire, mirrors save() --
 *     fa 64 a1 f8 00 02 00        cli; mov eax,fs:[per_cpu__cpu_number]  (0x18ad8)
 *     f0 0f ab 05 78 00 02 00     lock bts [rtai_cpu_lock],eax
 *     19 c0 85 c0 75 1b           sbb eax,eax; test eax,eax; jnz +0x1b (skip ticket)
 *     b8 00 01 00 00              mov eax,0x100
 *     f0 66 0f c1 05 7c 00 02 00 lock xadd [_nano2count_cpuid],ax
 *     38 e0 74 09 f3 90 a0 7c 00 02 00 eb f3   cmp al,ah; jz done; pause; reread; loop
 *     eb c0                       jmp shared tail
 *
 *  Real bugs this closes relative to the previous reconstruction:
 *   (a) lockbyte RMW is a genuine atomic `LOCK BTS`/`LOCK BTR` (via
 *       `test_and_set_bit`/`test_and_clear_bit`, matching Linux 2.6.32's own
 *       `arch/x86/include/asm/bitops.h` instruction-for-instruction), not a
 *       plain non-atomic `*byte |= ...` - FIXED below.
 *   (b) the "acquire" path is a real ticket-spinlock wait, not a bare
 *       `+= 0x100` - FIXED below (full XADD + spin-until-serving==ticket).
 *   (c) the release path's counter bump targets the LOW BYTE ONLY (`inc
 *       byte ptr`), not a 16-bit `_nano2count_cpuid += 1` - the previous
 *       16-bit add would incorrectly carry into the ticket (high) byte on a
 *       0xff->0x00 wrap of the serving byte. FIXED below.
 *   (d) the "unconditional full memory barrier (LOCK;UNLOCK) at function
 *       entry" the previous pass flagged as entirely missing was a
 *       MISDIAGNOSIS, not a real gap: Ghidra's decompiler renders the paired
 *       `LOCK BTR [stack-local],0` (testing/clearing bit 0 of the `flags`
 *       parameter's OWN stack copy) as a bare `LOCK();UNLOCK();` pair,
 *       because the actual bit-test instruction it wraps has no other
 *       decompiled side effect Ghidra chose to show. There is no separate
 *       fence - it IS the `flags & 1` test, just performed with a genuinely
 *       atomic (if pointlessly so, given a private stack slot) `LOCK BTR`
 *       instead of a plain `AND`/`TEST`. No fix needed; noted so no future
 *       pass re-flags it as missing.
 *   ALSO NEWLY FOUND this pass, not previously flagged at all:
 *   (e) `per_cpu__cpu_number` must be read via the same `%fs:`-relative
 *       per-CPU access this file's OWN header comment already said it
 *       needed (matching `stg_get_current_task_nks4_local()`'s established
 *       idiom just above) - ground truth shows real `MOV reg,FS:[...]`
 *       loads at every call site, but the previous code read it as a plain
 *       global (`int cpu = per_cpu__cpu_number;`), which is simply wrong on
 *       this kernel's per-CPU data model (the linked symbol value is an
 *       *offset*, not a usable address, without the segment override).
 *       FIXED below via the new `nks4_this_cpu_number()` helper.
 *   (f) both functions gate the second-level (ticket-lock) work with a
 *       local `cli` immediately before touching `rtai_cpu_lock`/the ticket
 *       word (both in the restore release path AND its mirrored re-acquire
 *       path) - entirely absent from the previous reconstruction. FIXED
 *       below (`asm volatile("cli")` at both sites, matching ground truth's
 *       instruction order exactly).
 *
 *  What's still open: the *why* behind this exact scheme (a ticket lock for
 *  cross-CPU serialization of "global cli", gated by a per-CPU recursion bit)
 *  is inferred from mechanism, not from any surviving RTAI source comment in
 *  this repo - plausible (RTAI's own `rt_global_save_flags_and_cli`/
 *  `rt_global_restore_flags` semantics genuinely need to serialize hard-IRQ-
 *  disable across CPUs), but not confirmed against upstream RTAI source
 *  (none is vendored in this repo to cross-check against).
 * ========================================================================= */
extern "C" unsigned int rtwrap_global_save_flags_and_cli(void)
{
	unsigned int flags;
	asm volatile("pushf; pop %0; cli" : "=r"(flags));
	unsigned int result = flags & 0x200;

	int cpu = nks4_this_cpu_number();
	if (nks4_test_and_set_bit(cpu, &rtai_cpu_lock))
		return result;	/* nested on this CPU - already held, no ticket work */

	/* First entry on this CPU: take a ticket on the global cross-CPU
	 * sequence lock. _nano2count_cpuid is [serving_lo][ticket_hi]; XADD
	 * hands out the next ticket (old high byte) and returns the old pair. */
	unsigned short old_pair;
	asm volatile("lock; xadd %0, %1"
		     : "=r"(old_pair), "+m"(_nano2count_cpuid)
		     : "0"((unsigned short)0x100)
		     : "memory");
	unsigned char ticket  = (unsigned char)(old_pair >> 8);
	unsigned char serving = (unsigned char)old_pair;
	while (serving != ticket) {
		asm volatile("pause");
		serving = *(volatile unsigned char *)&_nano2count_cpuid;
	}
	return result | 1;
}

extern "C" void rtwrap_global_restore_flags(unsigned int flags)
{
	if (!(flags & 1)) {
		/* Nested restore: mirrors save()'s own re-acquire + ticket-wait
		 * exactly (ground truth @0x18ad8), then falls into the shared
		 * STI tail below. */
		asm volatile("cli");
		int cpu = nks4_this_cpu_number();
		if (!nks4_test_and_set_bit(cpu, &rtai_cpu_lock)) {
			unsigned short old_pair;
			asm volatile("lock; xadd %0, %1"
				     : "=r"(old_pair), "+m"(_nano2count_cpuid)
				     : "0"((unsigned short)0x100)
				     : "memory");
			unsigned char ticket  = (unsigned char)(old_pair >> 8);
			unsigned char serving = (unsigned char)old_pair;
			while (serving != ticket) {
				asm volatile("pause");
				serving = *(volatile unsigned char *)&_nano2count_cpuid;
			}
		}
	} else {
		/* Outer restore: release our per-CPU claim, and if we really
		 * held it, hand the ticket to the next waiter by bumping the
		 * SERVING (low) byte only - a plain, non-LOCKed `inc byte ptr`
		 * in ground truth, safe because the ticket lock's own mutual
		 * exclusion guarantees at most one CPU is ever here, and the
		 * `cli` just below blocks same-CPU IRQ reentrancy. */
		asm volatile("cli");
		int cpu = nks4_this_cpu_number();
		if (nks4_test_and_clear_bit(cpu, &rtai_cpu_lock))
			*(volatile unsigned char *)&_nano2count_cpuid += 1;
	}
	if (flags & 0x200)
		asm volatile("sti");
}

/* CSTGThread's own CreateRealTimeWithCPUAffinity()/Delete()/Wait()/
 * GetMaxRealTimePriority() stay in realtime.cpp (that's where the class
 * definition itself lives) - NOT duplicated here to avoid a
 * multiple-definition link error. RESOLVED 2026-07-18: the per-instance
 * task-handle offset this comment used to flag as needing one more
 * disassembly pass is now pinned down (pTask @+0x00, bActive @+0x04 - see
 * omapnks4_internal.h's CSTGThread block comment for the evidence) and
 * realtime.cpp's CSTGThread now calls the real rtwrap_pthread_* primitives
 * below instead of the old kernel_thread()-based stand-in. Ground truth
 * confirms Delete()/Wait() both route through
 * rtwrap_clear_debug_traps_in_rt_task() + rtwrap_pthread_cancel()/
 * rtwrap_pthread_join() gated on that "bActive" byte, and
 * GetMaxRealTimePriority() is a plain tail-call to rtwrap_get_max_priority()
 * (defined above) - exactly as now implemented in realtime.cpp. */

/* swi(3) is the x86 `int 3` breakpoint trap - a debugger breakpoint hook, not
 * meaningful outside an attached kernel debugger. Modeled directly. */
extern "C" void STGBreakPoint(void)
{
	asm volatile("int3");
}
