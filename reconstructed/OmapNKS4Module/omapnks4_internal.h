// SPDX-License-Identifier: GPL-2.0
/*
 * omapnks4_internal.h  -  module-private declarations: the STG/RTAI framework
 * layer, kernel imports, singletons and the cross-file helpers.
 *
 * CORRECTED 2026-07-17: this comment previously claimed stg_* / rtwrap_* are a
 * shared library imported from STGEnabler.ko/RTAI, not redefined here. Fresh
 * Ghidra analysis disproves that for BOTH prefixes: every stg_* and rtwrap_*
 * symbol (and CSTGThread's methods) has a real, non-trivial size and address
 * *inside* OmapNKS4Module.ko itself (0x17f50-0x18c71), not a 1-byte
 * unresolved-external thunk like the genuine kernel/RTAI imports below (printk,
 * rt_sem_wait, scsi_*, ...). The real picture: stg_* / rtwrap_* is a thin,
 * per-module statically-linked veneer over real RTAI/kernel primitives - each
 * STG module (this one, OA.ko, loadmod.ko, ...) carries its OWN compiled copy,
 * not a shared imported library. See rtwrap.cpp's own header comment for the
 * full writeup; main.cpp has the stg_* half (stg_kmalloc/kfree/msleep/
 * get_cpu_khz/ksize/is_linux_context/hweight32/num_online_cpus).
 */

#ifndef OMAPNKS4_INTERNAL_H
#define OMAPNKS4_INTERNAL_H

#include "omapnks4.h"

/* 1e6 ns/ms numerator; flNanosPerCycle = NANOS_PER_MS / cpu_khz  (DAT_0000af4c). */
#define OMAPNKS4_NANOS_PER_MS 1000000.0f

/* TSC read (the binary inlines RDTSC). */
static inline unsigned long long omapnks4_rdtsc(void)
{
	unsigned int lo, hi;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)hi << 32) | lo;
}

extern "C" {

/* ---- STG kernel/heap/timing veneer ------------------------------------- */
unsigned int stg_get_cpu_khz(void);
void        *stg_kmalloc(unsigned int size);
void         stg_kfree(void *p);
unsigned int stg_ksize(void *p);
void         stg_msleep(unsigned int ms);
int          stg_is_linux_context(void);
unsigned int stg_hweight32(unsigned int x);
unsigned int stg_num_online_cpus(void);

/* ---- rtwrap.cpp: the local pthread-style veneer over RTAI (see that file's
 * own header comment for the 2026-07-17 "not actually external" finding) ---- */
void stg_schedule(void);
unsigned int get_sizeof_rt_task_struct(void);
unsigned int get_sizeof_pthread_barrier_t(void);
unsigned int get_sizeof_rtwrap_pthread_attr(void);
unsigned int get_sizeof_rtwrap_pthread_mutex(void);
unsigned int get_sizeof_rtwrap_barrier(void);
unsigned int get_sizeof_rtwrap_pthread_cond(void);
int  get_pthread_recursive_attr_constant(void);
void *rtwrap_malloc(unsigned int size);
void  rtwrap_free(void *p);
int  rtwrap_pthread_attr_init(unsigned int *attr);
int  rtwrap_pthread_attr_destroy(void *attr);
int  rtwrap_pthread_attr_setrtpriority(unsigned int *attr, unsigned int priority);
int  rtwrap_pthread_attr_setstacksize(unsigned int *attr, unsigned int size);
/* fn's signature corrected from void(*)(void) to void(*)(void*) - re-verification
 * pass, 2026-07-17, see rtwrap.cpp's posix_wrapper_fun for why. */
int  rtwrap_pthread_create(void **out_task, unsigned int *attr, void (*fn)(void *), void *arg);
int  rtwrap_pthread_join(unsigned char *task, void *exit_code_out);
int  rtwrap_pthread_cancel(unsigned char *task);
void rtwrap_init_stack_for_depth_measurement(unsigned int *task);
unsigned int rtwrap_measure_stack_surplus(unsigned int *task);
int  rtwrap_pthread_mutexattr_init(unsigned int *attr);
int  rtwrap_pthread_mutexattr_destroy(void *attr);
int  rtwrap_pthread_mutexattr_settype(unsigned int *attr, int type);
int  rtwrap_pthread_mutex_init(void *mutex, unsigned int *attr);
int  rtwrap_pthread_mutex_destroy(void *mutex);
int  rtwrap_pthread_mutex_lock_timed(void *mutex, long long delay, void *unused);
int  rtwrap_pthread_mutex_lock(void *mutex);
unsigned int rtwrap_pthread_mutex_unlock(void *mutex);
int  rtwrap_pthread_barrier_init(void *barrier, int count);
int  rtwrap_pthread_barrier_destroy(void *barrier);
void rtwrap_pthread_barrier_wait(void *barrier);
int  rtwrap_pthread_cond_init(void *cond);
int  rtwrap_pthread_cond_destroy(void *cond);
void rtwrap_pthread_cond_signal(void *cond);
void rtwrap_pthread_cond_broadcast(void *cond);
void rtwrap_pthread_cond_wait(void *cond, void *mutex);
int  rtwrap_pthread_cond_timedwait(void *cond, void *mutex, long *abstime);
void rtwrap_rt_cond_wait_timed(void *cond, void *mutex, long long delay, void *unused);
void  rtwrap_sched_lock(void);
void  rtwrap_sched_unlock(void);
void *rtwrap_whoami(void);
void  rtwrap_task_suspend(void *task);
void  rtwrap_task_resume(void *task);
unsigned int rtwrap_get_task_suspend_depth(unsigned char *task);
void  rtwrap_sleep(long long count);
void  rtwrap_task_wakeup_sleeping(void *task);
bool  rtwrap_is_task_sleeping(unsigned char *task);
int  rtwrap_get_runnable_on_cpuid(unsigned char *task);
int  rtwrap_get_sched_policy(unsigned char *task);
int  rtwrap_get_rr_quantum(unsigned char *task);
int  rtwrap_get_max_priority(void);
int  rtwrap_get_min_priority(void);
void rtwrap_get_priorities(int *max, int *min, int *rr);
void rtwrap_set_runnable_on_cpuid(void *task, unsigned long cpumask);
/* Return type corrected int (was void), 2026-07-18 CSTGThread pass: ground
 * truth @0x18220 sets up EDX=(fixed internal debug-trap-table pointer) then
 * `call set_debug_traps_in_rt_task; ret` with no EAX write in between - the
 * external's EAX return flows straight through as this wrapper's own return.
 * Matters because CSTGThread::CreateRealTimeWithCPUAffinity actually tests
 * it (`test eax,eax; jnz <rollback>` @0x18bbb) to decide whether to tear the
 * just-created RT task back down again - a real, load-bearing return value,
 * not a benign leftover register. rtwrap_clear_debug_traps_in_rt_task's own
 * external call (@0x18210) shows the identical passthrough shape, but no
 * caller anywhere in this binary (Delete, Wait, or the Create rollback path)
 * ever tests its result, so it's left declared void - flagged, not fixed,
 * same as this project's other "nothing depends on it" findings. */
int  rtwrap_set_debug_traps_in_rt_task(void);
void rtwrap_clear_debug_traps_in_rt_task(void);
void rtwrap_start_rt_timer(int period);
long long rtwrap_nano2count(long long nanos);
long long rtwrap_nano2count_cpuid(long long nanos, int cpuid);
long long rtwrap_rt_get_time_cpuid(int cpuid);
void rtwrap_count2timespec(void *count, void *timespec_out);
void rtwrap_request_irq(unsigned int irq, void *handler);
void rtwrap_release_irq(unsigned int irq);
void rtwrap_assign_irq_to_cpu(unsigned int irq, unsigned long cpumask);
void rtwrap_startup_irq(unsigned int irq);
void rtwrap_shutdown_irq(unsigned int irq);
void rtwrap_pend_linux_srq(int srq);
unsigned int rtwrap_global_save_flags_and_cli(void);
void rtwrap_global_restore_flags(unsigned int flags);
void STGBreakPoint(void);

}  /* extern "C" */

/* kmalloc_buf(size): used throughout (usb.cpp, procfs.cpp) as a plain
 * "allocate a zeroed-ish buffer" helper, always with a single size argument -
 * the module's own README already flagged this as a "leaf helper... supply from
 * the STG framework" placeholder. No distinct real symbol was found for it
 * separately from stg_kmalloc (same signature, same role) while fixing usb.cpp's
 * Kbuild errors this session, so implemented directly as a thin wrapper rather
 * than left as an unresolved extern to a symbol that may not actually exist under
 * this name. */
static inline void *kmalloc_buf(unsigned int size) { return stg_kmalloc(size); }

extern "C" {

/* ---- RTAI primitives ----------------------------------------------------
 * NOT called through an "rtwrap_" veneer in the real binary - that prefix
 * turned out to be real (this same module has a whole local family of
 * rtwrap_pthread_ and rtwrap_set_ prefixed helpers, confirmed via Ghidra),
 * just not for these three operations: the stock module's own import list calls
 * rt_sleep/nano2count/rt_pend_linux_srq directly (their real, unprefixed
 * rtai_*.ko export names), confirmed 2026-07-16.
 *
 * CONFIRMED (adversarial re-verification pass #2, 2026-07-18): these three
 * needed no regparm(0) override, unlike several rt_-prefixed / nano2count_cpuid
 * externs declared inside rtwrap.cpp's own extern "C" block (see that
 * file's header comment for the full list of what DID need fixing).
 * nano2count/rt_sleep's real call sites (rtwrap_nano2count @0x18150,
 * rtwrap_sleep @0x189a0) push both halves of the 64-bit argument to the
 * stack - exactly what plain ambient regparm(3) already produces for a
 * function whose sole parameter is a `long long` (GCC never register-
 * assigns a 64-bit value on i386 regparm), so no override is needed.
 * rt_pend_linux_srq's real call site (rtwrap_pend_linux_srq @0x18a50) is a
 * zero-instruction register tail-forward (srq stays in EAX, no stack
 * setup at all) - confirmed register-passed, matching this declaration's
 * ambient regparm(3) as-is. rt_free_srq below was independently checked
 * too (CleanupOmapNKS4Driver's call site loads srq into EAX with no stack
 * setup) - also confirmed correct as ambient regparm(3), no override
 * needed. */
long long nano2count(long long nanos);
void rt_sleep(long long count);
void rt_pend_linux_srq(int srq);

/* ---- USB / kernel imports used across files ---------------------------- */
int  printk(const char *fmt, ...);

/* Real Linux 2.6.32 kernel primitives, declared with opaque void* args instead of
 * their real typed parameters (wait_queue_head_t*, struct completion*) - this
 * project's own established, already-boot-tested pattern for this exact class of
 * problem (see kronosology/reconstructed/OA/src/init/daemon_lifecycle.cpp, which
 * OA.ko - confirmed booting, MASTER_REFERENCE.md sec 10.237 - already uses these
 * exact signatures against the same kernel tree). wait_queue_head_t is 12 bytes on
 * this target (spinlock_t + list_head, both non-debug builds); struct completion is
 * 16 bytes (done:4 + wait_queue_head_t:12) - callers declare raw byte storage of
 * these sizes rather than the real (ipipe/spinlock-tainted, C++-unparseable) types,
 * matching daemon_lifecycle.cpp's own `wakeQueue[0xc]`/`completion1[0x10]` fields. */
void __wake_up(void *q, unsigned int mode, int nr_exclusive, void *key);
void __init_waitqueue_head(void *q, void *key);
/* Real blocking wait-queue primitives - ground truth (WaitForNKS4ReadEvent@0x12a40,
 * WaitForNKS4CommandWrite@0x128f0, WaitOnAtmelRead@0x12070 disassembly, 2026-07-18):
 * these three, plus `sleep_on_timeout` below, were previously assumed elided/
 * unrecoverable from the decompile and modeled as stg_msleep() polling
 * (submit.cpp's own prior comment: "schedule_timeout_wait/sleep_on_timeout: real
 * kernel primitives ... called with elided arguments in the original decompile").
 * That assumption was wrong for all three call sites - every argument (wait-queue
 * address, TASK state, jiffies budget) is directly visible in the raw disassembly.
 * See submit.cpp's own per-function comments for the exact values. `prepare_to_wait`/
 * `finish_wait` take a `wait_queue_t*` (this project's `struct omap_wait_entry`,
 * submit.cpp); `schedule_timeout` takes/returns jiffies remaining, same contract
 * this file's now-removed `schedule_timeout_wait()` shim already documented. */
long schedule_timeout(long timeout_jiffies);
int  prepare_to_wait(void *q, void *wait, int state);
void finish_wait(void *q, void *wait);
/* sleep_on_timeout(q, timeout): the OTHER real primitive - WaitOnAtmelRead's
 * entire body in the real binary is a single tail-call to this, no hand-rolled
 * loop at all (unlike the three above). Ground truth timeout is 0x7d0 = 2000
 * jiffies, not the previously-guessed 1000 (submit.cpp's own comment). */
long sleep_on_timeout(void *q, long timeout_jiffies);
void complete(void *completion);
void remove_proc_entry(const char *name, void *parent);
int  rt_free_srq(unsigned int srq);

/* struct completion is 16 bytes on this target (done:4 + wait_queue_head_t:12),
 * same storage convention as the wait queues above. usb.cpp's OmapNKS4Probe signals
 * this on success; main.cpp's OmapNKS4Init waits on it (NOT yet fixed - main.cpp's
 * own wait_for_completion_timeout()/complete() call sites are still missing their
 * real arguments too, a separate, not-yet-attempted part of this same gap - see
 * docs/gaps.md "Real Kbuild build attempt"). Declared here, shared, so usb.cpp's
 * fix and main.cpp's future fix reference the same real object. */
extern unsigned char sProbeComplete[0x10];

/* driver.cpp / video.cpp C-ABI wrappers used cross-file (by usb.cpp) - both are
 * defined inside those files' own `extern "C" { ... }` blocks, so this declaration
 * must have matching C linkage too (a plain C++-linkage forward declaration outside
 * this block, tried first, failed with "conflicting declaration ... with 'C'
 * linkage" - confirmed via a real Kbuild build attempt, 2026-07-15). Both were
 * genuinely missing implementations until this session; see their definitions'
 * own comments. */
void COmapNKS4Driver_ReceiveEventBuffer(NKS4Command *cmd, unsigned int numInts);
void COmapNKS4_SetMaxBulkOutMsgSize(unsigned int maxPacketSize);
/* Plain forward declarations for driver.cpp/video.cpp C-ABI wrappers usb.cpp calls -
 * all four already correctly defined, just never declared in the shared header
 * before this session's real Kbuild build attempt surfaced the gap. */
/* Ground truth (fresh disassembly, 2026-07-18, WriteCallback@0x10040): the real
 * call site loads EAX=urb->transfer_buffer (+0x40), EDX=urb->actual_length>>2
 * (+0x50, word count) right before this call - NOT a no-arg call as previously
 * declared/called. Matches COmapNKS4Driver::NotifyTransmittedCommandComplete's
 * own (NKS4Command*, unsigned int) member signature exactly. */
void COmapNKS4Driver_NotifyTransmittedCommandComplete(NKS4Command *cmd, unsigned int numWords);
void COmapNKS4Driver_Cleanup(void);
void COmapNKS4VideoAPI_Initialize(void);
void COmapNKS4Driver_Initialize(unsigned int maxWritePacketInts);
/* usb.cpp - needed by main.cpp to construct this module's struct usb_driver
 * (probe/disconnect fields) - see main.cpp's own comment for the full derivation. */
int  OmapNKS4Probe(struct usb_interface *intf);
/* VM-only self-injection (usb.cpp) - see main.cpp's own call-site comment
 * (OmapNKS4Init, guarded by the vm_virtual_probe module parameter) for why
 * this replaced an earlier separate-module design. */
int  vm_virtual_probe_inject(void);
/* VM-only, 2026-07-17 continuation: synthesizes ONE inbound interrupt-IN event
 * packet (a button + an aftertouch record) and feeds it through the real,
 * unmodified InterruptCallback()/ReceiveEventBuffer() decode path, once the
 * board is fully probed/configured and running - see usb.cpp's own definition
 * and main.cpp's call site (end of OmapNKS4Init) for the full rationale. */
void vm_virtual_probe_inject_event(void);
/* VM-only, 2026-07-17 continuation: calls the real COmapNKS4Command::
 * SetLCDBrightness()/ResetModule() directly, exercising vm_usb_submit_urb's
 * new dispatch coverage for both (usb.cpp) - neither is ever reached by this
 * module's own real boot/configure sequence, so nothing else would drive them
 * in a VM test. See usb.cpp's own definition for the full rationale. */
void vm_virtual_probe_test_setters(void);
/* VM-only, 2026-07-18: real-concurrency stress test for the video draw-builder
 * ring (video.cpp) - spawns real kernel threads that hammer InitLCDRegs/
 * XAxisByteSize/SendPixelDataRegion/SendFillData/UpdateColorPal concurrently
 * while the already-running kOmapNKS4MsgRoutine thread drains them, to
 * behaviorally validate the GetNextFreeFifoEvent/AddFifoEvent race-check and
 * pop_free_urb's locking under genuine SMP contention rather than disassembly
 * re-derivation alone. Gated behind its OWN module parameter (sVmVideoStress,
 * main.cpp) in addition to sVmVirtualProbe - completely separate opt-in from
 * the standard vm_virtual_probe=1 boot test this README already documents
 * extensively, so it never runs unless explicitly requested. See video.cpp's
 * own definition and the README's dated stress-test section for full design/
 * results. */
extern int sVmVideoStress;
void vm_virtual_probe_stress_test_video(void);
void OmapNKS4Disconnect(void);
void CleanupOmapNKS4Driver(void);
/* realtime.cpp's free-function C-ABI over CSTGOmapNKS4Fifos/*Fifo - see that file's
 * own comments for the ground truth and the rtwrap_pend_linux_srq-vs-rt_pend_linux_srq
 * distinction from the class methods above. */
void CSTGOmapNKS4Fifos_Initialize(int enable);
void OmapNKS4Fifos_TriggerOutputInterrupt(void);
int  OmapNKS4InputFifo_ReadCommand(unsigned int *out);
int  OmapNKS4OutputFifo_WriteCommand(unsigned int cmd);
int  OmapNKS4ProcInitialize(void);
void OmapNKS4ProcDone(void);
bool OmapNKS4ProcInitialized(void);
int  OmapNKS4ProcReadEvent(void);
void OmapNKS4VideoAPIProcessEvents(void);
void COmapNKS4Driver_ShutDown(unsigned short param);
void COmapNKS4Driver_SetInstallerSupportOn(int on);
void COmapNKS4Driver_SetNumberOfKeys(unsigned int n);
void COmapNKS4Driver_EnableShutdownByDriver(void);
unsigned char COmapNKS4Driver_GetProgressBarPercent(void);
void COmapNKS4Driver_IncProgressBar(void);
void COmapNKS4Driver_SetProgressBarPercent(unsigned char p);
unsigned char COmapNKS4Driver_AddToProgressBar(unsigned char amount);
void COmapNKS4Driver_GetOmapVersion(unsigned char *v, unsigned char *r);
int  COmapNKS4Driver_GetHardwareVersion(void);

/* Second free-function C-ABI family (no "Driver" in the name) - what
 * OmapVideoModule.ko's omapfb_ioctl actually calls for the progress-bar/title-screen
 * ioctls; each is a duplicate one-line forward to the same COmapNKS4Driver singleton
 * method as the COmapNKS4Driver_* family above. See driver.cpp. */
void COmapNKS4_SetProgressBarColor1(unsigned char c);
void COmapNKS4_SetProgressBarColor2(unsigned char c);
void COmapNKS4_SetProgressBarPercent(unsigned char p);
void COmapNKS4_IncProgressBar(void);
void COmapNKS4_AddToProgressBar(unsigned char a);
unsigned char COmapNKS4_GetProgressBarPercent(void);
int  COmapNKS4_GetTitleScreenVersion(void);

/* procfs.cpp's own externs - real libc/kernel primitives (well-known, standard
 * signatures - not opaque-typed like the wait-queue/completion pattern above,
 * since none of these take a C++-unparseable real struct type) plus this kernel's
 * spinlock_t-level primitives (`_spin_lock`/`_spin_unlock` - what spin_lock()'s own
 * PICK_SPINOP macro dispatches to on this ipipe-patched kernel, confirmed via the
 * exact error text seen fighting <linux/spinlock.h> earlier this session).
 *
 * Guarded by __KERNEL__ (unlike everything else in this file, which is safe to
 * expose to a host build too): on a real kernel build there's no libc, so these
 * must be declared; on the host `verify/` build these are real glibc functions
 * already declared (with slightly different but compatible signatures) by
 * <cstdio>/<cstring>, and redeclaring them here conflicts - confirmed via a real
 * `make verify` run, 2026-07-15, after adding these for the Kbuild fix. Nothing in
 * `verify/`'s two test binaries (command.cpp/driver.cpp) actually calls any of
 * these - only procfs.cpp does, and procfs.cpp isn't part of the host verify suite -
 * so guarding them costs nothing on the host side. */
#ifdef __KERNEL__
extern "C" {
int  sprintf(char *buf, const char *fmt, ...);
int  copy_from_user(void *to, const void *from, unsigned int n);
char *strstr(const char *haystack, const char *needle);
unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base);
unsigned long strcspn(const char *s, const char *reject);
void kfree(const void *p);
void _spin_lock(void *lock);
void _spin_unlock(void *lock);
/* _spin_lock_irqsave/_irqrestore - added during the 2026-07-17 re-verification
 * pass: OmapNKS4ProcReadEvent's real dequeue path uses this irqsave/irqrestore
 * pair, not the plain _spin_lock/_spin_unlock the add-side (OmapNKS4ProcAddEvent)
 * uses - a real, confirmed asymmetry between the two sides of this ring buffer,
 * not a transcription inconsistency. Same PICK_SPINOP-dispatched primitive
 * family as _spin_lock/_spin_unlock above; returns/takes the saved IRQ flags
 * word. */
unsigned long _spin_lock_irqsave(void *lock);
void _spin_unlock_irqrestore(void *lock, unsigned long flags);
/* Real kernel create_proc_entry (3-arg) - confirmed via the stock module's
 * own import list + OmapNKS4ProcInitialize's real disassembly, 2026-07-16:
 * NOT a fictional "_real" suffix wrapper, this is the actual, ordinary
 * kernel create_proc_entry() every driver uses. Named create_proc_entry
 * here (not create_proc_entry_real) precisely to match the real ksymtab
 * entry - this repo's own make_proc_entry() below is the 1-arg convenience
 * wrapper around it, deliberately renamed to avoid colliding with this
 * declaration. */
void *create_proc_entry(const char *name, unsigned int mode, void *parent);
}
#endif
/* proc_dir_entry's read_proc/write_proc field offsets: +0x38/+0x3c - CORRECTED
 * 2026-07-17 (was +0x3c/+0x40, off by exactly one dword). The original derivation's
 * struct-layout math assumed "mode_t/nlink_t/uid_t/gid_t are all 4 bytes on this
 * kernel" - wrong: linux-kronos's own arch/x86/include/asm/posix_types_32.h has
 * __kernel_mode_t and __kernel_nlink_t as `unsigned short` (2 bytes each), only
 * uid_t/gid_t are the 4-byte uid32/gid32 variants. That 2-fields-of-2-bytes
 * overcount is exactly the +4 error found. Now confirmed two independent ways:
 * (1) a host-compiled offsetof() test using the kernel's own exact typedefs gives
 * read_proc=0x38, write_proc=0x3c; (2) fresh disassembly of the real binary's own
 * `OmapNKS4ProcInitialize` (its four `create_proc_entry()` call sites) shows the
 * literal store pattern `MOV [EAX+0x38],<read_fn>; MOV [EAX+0x3c],<write_fn>`
 * immediately after each call - no longer struct-layout math alone, disassembly-
 * verified like the rest of this session's fixes. */
/* mode param added 2026-07-18: ground truth uses a real per-entry mode
 * (0666/0444), not a uniform 0644 - see procfs.cpp's own comment. */
void *make_proc_entry(const char *name, int mode);
void proc_set(void *entry, void *read_fn, void *write_fn);

/* submit.cpp/usb.cpp cross-file declarations, found missing via a real Kbuild
 * build attempt. */
void WaitForFreeBulkWriteURB(int type);
/* ApplyGenericCalibration is a real LOCAL function (confirmed via Ghidra,
 * 2026-07-16 - GCC function cloning, not an external OA.ko call as
 * previously guessed), private to submit.cpp - no extern declaration
 * needed here, see its definition there. */

}  /* extern "C" */

/* ---- C++ runtime (provided by the STG cpp-support shim) ---------------- */
void *operator new(unsigned int size);
void  operator delete(void *p);

/* Placement new - normally pulled in from <new>, unavailable in this freestanding
 * kernel C++ build (no libstdc++). video.cpp needs it for `new (&g_video)
 * COmapNKS4VideoAPI()`; the standard no-op-body definition (returns the supplied
 * storage unchanged) is correct here since it's just choosing the constructor
 * overload, not allocating anything - confirmed missing via a real Kbuild build
 * attempt, 2026-07-15 ("no matching function for call to operator new(sizetype,
 * COmapNKS4VideoAPI*)"). */
inline void *operator new(unsigned int, void *p) { return p; }

/* ---- CSTGThread: STG real-time thread base ----------------------------- */
typedef void *(*stg_thread_fn)(void *arg);
/*
 * Real per-instance layout, pinned down 2026-07-18 via disassembly of
 * CreateRealTimeWithCPUAffinity (@0x18b20), Delete (@0x18bf0), Wait
 * (@0x18c20) and GetMaxRealTimePriority (@0x18c50) in the correct
 * 89849-byte OmapNKS4Module.ko target - this was the blocking item the
 * README's "Continued RE, 2026-07-17 (session 2)" entry deferred.
 *
 * CSTGThread is NOT the empty method-only base this reconstruction
 * previously modeled (with omapnks4.h's CActiveSenseThread separately
 * declaring a "pVTable" field at its own offset 0x00 to cover for it). It
 * has two real data members of its own, and there is no vtable anywhere:
 * CActiveSenseThread's constructor (@0x17b50) writes bActive=0 at [this+4]
 * but never touches [this+0] at all - impossible if a vtable pointer lived
 * there, since every constructor of a polymorphic class must initialize it.
 * The previous "pVTable" label was simply wrong; that slot is CSTGThread's
 * own task-handle field, populated later by CreateRealTimeWithCPUAffinity,
 * not at construction time.
 *
 *   +0x00  pTask    - opaque RTAI task handle. CreateRealTimeWithCPUAffinity
 *                     passes EAX=this directly as rtwrap_pthread_create's
 *                     `void **out_task` argument (confirmed: `mov eax,esi`
 *                     where esi==this, immediately before `call
 *                     <rtwrap_pthread_create>` @0x18b84-0x18b89) - i.e.
 *                     rtwrap_pthread_create itself does `*(void**)this =
 *                     new_task`, using the CSTGThread object as its own
 *                     out-param storage. Every later access (the post-create
 *                     debug-trap/cpu-affinity setup @0x18bb4/0x18bc2, the
 *                     Create-failure rollback @0x18bd6/0x18bdd, Delete
 *                     @0x18c03/0x18c0a, Wait @0x18c33/0x18c3c) re-reads it
 *                     via a plain `mov eax,[this]`.
 *   +0x04  bActive  - byte flag. Cleared by the constructor, set to 1 only
 *                     on a fully successful Create (task spawned AND
 *                     rtwrap_set_debug_traps_in_rt_task succeeded), cleared
 *                     again by Delete/Wait/the Create-failure rollback path.
 *                     Delete (@0x18bfd) and Wait (@0x18c2d) both gate their
 *                     ENTIRE body behind `cmp byte[this+4],0 / jz <skip>` -
 *                     calling either on a never-started or already-stopped
 *                     thread is a real, deliberate no-op, not a
 *                     use-after-free waiting to happen.
 * Total size 8 (pTask + bActive + 3 bytes padding), consistent with
 * CActiveSenseThread's own first real field (qwNextTickCycles) starting
 * exactly at +0x08.
 */
struct CSTGThread {
	void         *pTask;	/* 0x00 - RTAI task handle, see block comment above */
	unsigned char bActive;	/* 0x04 */

	/*
	 * Real explicit parameter order/count (Ghidra, 0x18b20): (fn, priority,
	 * cpumask, arg) - NOT (fn, arg, priority, stack, cpumask) as this
	 * reconstruction previously guessed. There is no caller-supplied
	 * "stack size" parameter at all; the RT task's stack size is hardcoded
	 * to 0x5000 bytes internally (`rtwrap_pthread_attr_setstacksize(attr,
	 * 0x5000)` @0x18b73-0x18b7a - the immediate 0x5000 is loaded right
	 * into EDX before that call, not derived from any incoming argument).
	 *
	 * This whole module is built with `-mregparm=3` (see omapnks4.h's own
	 * header comment), so member functions get it ambiently - this=EAX,
	 * fn=EDX, priority=ECX; cpumask and arg are the two left over and go on
	 * the stack in that declaration order (cpumask at [ESP], arg at
	 * [ESP+4]). Confirmed both inside this function's own prologue
	 * (`mov eax,[edi]` -> saved and later used as cpumask via
	 * rtwrap_set_runnable_on_cpuid; `mov edi,[edi+4]` -> forwarded as-is to
	 * rtwrap_pthread_create's stack-passed `arg`) and at its sole caller,
	 * CActiveSenseThread_Setup (@0x17e10-0x17e2a): EDX=fn address 0x18fa0
	 * (OmapNKS4_ActiveSenseThreadEntry), ECX=GetMaxRealTimePriority()-10,
	 * [ESP]=0 (cpumask), [ESP+4]=EBX (the CActiveSenseThread object itself,
	 * passed as `arg` - it's what OmapNKS4_ActiveSenseThreadEntry casts
	 * back and calls ->ThreadRoutine() on).
	 */
	int CreateRealTimeWithCPUAffinity(stg_thread_fn fn, int priority,
					  unsigned long cpumask, void *arg);
	void Delete(void);
	void Wait(void);
	static int GetMaxRealTimePriority(void);
};

/* ========================================================================= *
 *  OmapNKS4 module globals / cross-file helpers
 * ========================================================================= */

/* the singleton panel-driver and video-API instances (mangled COmapNKS4*::sInstance) */
extern struct COmapNKS4Driver    COmapNKS4Driver_sInstance;
extern struct COmapNKS4VideoAPI  g_video;	/* == COmapNKS4VideoAPI::sInstance */

/* host->panel output path (submit.c).  A "command word" is opcode<<24 | reg<<16 |
 * dataHi<<8 | dataLo and is passed to the submit/wait helpers in EAX. */
int  SubmitNKS4CommandMultipleWriteNONBLOCKING(unsigned int *cmds, unsigned int nInts);
int  SubmitNKS4CommandWrite(unsigned int cmd);
int  SubmitOmapNKS4CmdBulkWrite(unsigned char command, unsigned char *data, unsigned int nBytes);
int  SubmitOmapNKS4BulkWrite(unsigned int *data, unsigned int nBytes);
int  WaitForNKS4CommandWrite(unsigned int cmd);	/* send cmd, block for write-complete */
int  WaitForNKS4ReadEvent(unsigned int *resp);	/* block for one response word         */
/* Real signature per submit.cpp's own definition (bool(int type), 0=command
 * queue/1=video queue) - this declaration previously said `(void)`, a genuine
 * mismatch (not a linkage issue) that left driver.cpp's 0-arg call sites
 * referencing a symbol nothing ever defined while submit.cpp's real 1-arg
 * definition sat unused - confirmed the hard way via insmod's "Unknown symbol
 * OmapNKS4WriteQueueNotFull" on real hardware, 2026-07-16. */
bool OmapNKS4WriteQueueNotFull(int type);

/* USB submit/alloc/free/deregister shims - real implementations in STGEnabler.c
 * (plain C, so it can safely include <linux/usb.h> and use the real struct urb
 * and struct usb_driver types; ground truth for these signatures).
 * `struct urb` is forward-declared opaquely here for the same reason
 * `struct usb_interface` is in usb.cpp - nothing in this module dereferences a
 * real named field of either, only raw offsets (see usb.cpp's own header comment
 * for the recovered offset table). `mem_flags` is declared `unsigned int` in place
 * of the real `gfp_t` (a plain integer flags type on this target; every real call
 * site here passes a literal 0). */
struct urb;
struct usb_interface; /* see usb.cpp's own forward declaration for why opaque */
/* URB free-list node - shared by usb.cpp (owns the pools) and submit.cpp (pops
 * nodes off them to submit). Moved here from usb.cpp after a real Kbuild build
 * attempt found submit.cpp needed the complete type, not just a forward
 * declaration. */
struct urb_node { struct urb_node *next, **pprev; };
/* extern "C": these are real STGEnabler.ko exports (plain C, confirmed via
 * readelf __ksymtab on the real 3.2.2 STGEnabler.ko) - without this these
 * compile with C++ mangled names that don't exist anywhere at insmod time
 * ("Unknown symbol _Z18stg_usb_free_urbP3urb" etc, confirmed on real
 * hardware, 2026-07-16), even though the real, correctly-named plain-C
 * symbol is right there and already loaded. */
extern "C" {
struct urb *stg_usb_alloc_urb(int iso_packets, unsigned int mem_flags);
void        stg_usb_free_urb(struct urb *urb);
int         stg_usb_submit_urb(struct urb *urb, unsigned int mem_flags);
}

/* VM-only: main.cpp's vm_virtual_probe module parameter + usb.cpp's wrapper over
 * stg_usb_submit_urb() - see usb.cpp's own vm_usb_submit_urb() comment for why a
 * real submit against the synthetic vm_virtual_probe device can't be allowed to
 * run unmodified. Every real stg_usb_submit_urb() call site in this module goes
 * through this wrapper instead, so real-hardware behavior (sVmVirtualProbe==0)
 * is bit-for-bit unchanged. */
extern int sVmVirtualProbe;
int vm_usb_submit_urb(struct urb *urb, unsigned int mem_flags);
/* VM-only: pending synthesized reply handoff between vm_usb_submit_urb (usb.cpp)
 * and WaitForNKS4ReadEvent (submit.cpp) - see both functions' own comments. */
extern unsigned int sVmPendingReply;
extern int sVmPendingReplyValid;
/* struct usb_driver / struct module - opaque, for the same C++-unparseable-real-
 * header reason as struct urb/usb_interface above. This module's own real
 * struct usb_driver object (ground truth: read_memory @0x1afa0, OmapNKS4Module.ko
 * 3.2.2 - see main.cpp's own comment for the full byte-level derivation and exact
 * field offsets, cross-checked against linux-kronos's real usb.h/device.h) is
 * defined in main.cpp and shared here so usb.cpp's CleanupOmapNKS4Driver can pass
 * it to stg_usb_deregister. `__this_module` is the real symbol Kbuild's
 * auto-generated <module>.mod.c defines for THIS_MODULE - referencing it directly
 * avoids hardcoding this one build's own fixed `.gnu.linkonce.this_module` address
 * (confirmed via ground truth to be EDX at the real call site, but that exact
 * address is naturally build-specific, unlike the symbol name). */
struct usb_driver;
struct module;
extern struct module __this_module;
extern unsigned char sOmapNKS4UsbDriver[0x70];
/* extern "C": same real-STGEnabler-export reasoning as stg_usb_alloc_urb/
 * free_urb/submit_urb above. */
extern "C" {
int  stg_usb_register_driver(struct usb_driver *driver, struct module *owner,
                              const char *mod_name);
void stg_usb_deregister(struct usb_driver *driver);
}

/* event delivery (procfs.c) */
void OmapNKS4ProcAddEvent(unsigned char ev);
void SendNKS4EventToLinuxReader(unsigned int cmd);

/* signals (submit.c / threads) */
void SignalAtmelReadComplete(void);
void SignalVideoMessageProcessor(void);
void SignalShutdownSSD(void);
void SetShutdownDelay(int delay);
void WaitOnAtmelRead(void);
int  SubmitOmapNKS4VideoWrite(unsigned int *data, unsigned int nBytes);

#endif /* OMAPNKS4_INTERNAL_H */
