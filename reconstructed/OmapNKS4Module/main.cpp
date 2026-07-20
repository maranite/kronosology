// SPDX-License-Identifier: GPL-2.0
/*
 * main.c  -  module init/exit and the two service threads.
 *
 *   OmapNKS4Init : bring up the C++ runtime, grab an RTAI SRQ, register the USB driver,
 *                  wait for probe, then start /proc, the interrupt transfer, configure
 *                  the panel, and launch the worker threads + active-sense RT thread.
 *   OmapNKS4Exit : reverse of the above.
 *   ProcessMsgRoutine   : video message-processor thread (drains the LCD event ring).
 *   ShutdownSSDRoutine  : on a panel power-button event, flush/stop the internal SSD
 *                         (SCSI sync) then tell the panel to shut down.
 *
 * The C++ runtime (init_cpp_support / cleanup_cpp_support, operator new/delete over
 * stg_kmalloc, __cxa_pure_virtual) is provided by the shared STG cpp-support shim.
 */

#include "omapnks4_internal.h"

/* Module init/exit boilerplate, normally from <linux/init.h>/<linux/module.h> -
 * unusable here for the same reason every other real kernel header is avoided in
 * this C++ translation unit (see omapnks4.h's own comment on <linux/usb.h>/
 * <linux/types.h>). `expected initializer before 'OmapNKS4Init'` (a genuine parse
 * error, confirmed via a real Kbuild build attempt, 2026-07-15) was `__init`
 * simply being an undeclared identifier sitting between the return type and
 * function name.
 *
 * __init/__exit: real section placement (this build's own ELF already has
 * .init.text/.exit.text sections - confirmed via Ghidra's own segment list when
 * this .ko was loaded for decompilation) so cold init/exit code can be reclaimed
 * after module load, same as the real kernel macros.
 *
 * module_init/module_exit: the real kernel's simplified LKM-build expansion
 * (`-DMODULE`, which Kbuild always passes for an out-of-tree module) is exactly
 * "the loader calls init_module()/cleanup_module() by fixed name" - reproduced
 * directly as thin wrapper functions rather than the alias-attribute form the
 * real macro also supports, to sidestep any static-linkage subtlety with GCC's
 * `alias` attribute targeting a `static` function.
 *
 * MODULE_DESCRIPTION/AUTHOR: real macros emit tagged strings into the .modinfo
 * ELF section for `modinfo`/depmod tooling - genuinely cosmetic, not load-bearing
 * for the module's actual function, so left as no-ops rather than replicating the
 * exact section-tag format.
 *
 * MODULE_LICENSE is NOT cosmetic, unlike the above - confirmed the hard way on
 * real hardware (2026-07): a no-op MODULE_LICENSE means the compiled .ko has no
 * "license=" entry in .modinfo at all, and the kernel only grants a loading
 * module visibility into EXPORT_SYMBOL_GPL()-exported symbols if it declares a
 * GPL-compatible license. STGEnabler.ko/RTAI export nearly everything this
 * module needs (stg_*, rtwrap_*, the C++ runtime shim - operator new/delete,
 * init_cpp_support, ...) via EXPORT_SYMBOL_GPL, apparently - without a real
 * license declared, insmod fails at symbol-resolution time with "Unknown
 * symbol" for essentially every extern this file declares (~40 of them,
 * confirmed via dmesg against a real STGEnabler.ko/rtai_*.ko already loaded),
 * and init_module() never runs at all. Real kernel macro equivalent: emit a
 * static const char[] "license=<value>" into the .modinfo section, same as
 * <linux/module.h>'s own MODULE_INFO/MODULE_LICENSE. */
#define __init __attribute__((__section__(".init.text")))
#define __exit __attribute__((__section__(".exit.text")))
#define module_init(fn) extern "C" int init_module(void) { return fn(); }
#define module_exit(fn) extern "C" void cleanup_module(void) { fn(); }
#define MODULE_LICENSE(x) \
	static const char __module_license[] __attribute__((used, section(".modinfo"))) = "license=" x
#define MODULE_DESCRIPTION(x)
#define MODULE_AUTHOR(x)

/* VM-testing affordance, 2026-07-17: hand-rolled module_param(int), same
 * "reproduce just what's needed" convention as the macros above (real
 * <linux/moduleparam.h> pulls in the same C++-unparseable header chain).
 * kernel_param's real layout (include/linux/moduleparam.h) is just
 * {name, perm, flags, set, get, arg} - param_set_int/param_get_int are
 * ordinary EXPORT_SYMBOL()-exported core kernel functions (kernel/params.c),
 * always present, no STGEnabler/RTAI dependency. Default 0: completely inert
 * on real hardware, and on a VM boot where the operator wants the real
 * dummy_hcd/gadget path exercised instead - only insmod ...
 * vm_virtual_probe=1 enables the self-injection below. */
struct kernel_param {
	const char *name;
	unsigned short perm;
	unsigned short flags;
	int (*set)(const char *val, struct kernel_param *kp);
	int (*get)(char *buffer, struct kernel_param *kp);
	union { void *arg; };
};
extern "C" int param_set_int(const char *val, struct kernel_param *kp);
extern "C" int param_get_int(char *buffer, struct kernel_param *kp);

int sVmVirtualProbe;
static const char __param_str_vm_virtual_probe[] = "vm_virtual_probe";
static struct kernel_param __param_vm_virtual_probe
	__attribute__((used, section("__param"), aligned(sizeof(void *)))) =
	{ __param_str_vm_virtual_probe, 0644, 0, param_set_int, param_get_int,
	  { &sVmVirtualProbe } };

/* VM-only, 2026-07-18: separate opt-in for video.cpp's concurrency stress test
 * (vm_virtual_probe_stress_test_video) - deliberately its OWN module parameter
 * rather than piggybacking on vm_virtual_probe alone, so every existing
 * vm_virtual_probe=1 boot test this README already documents keeps behaving
 * exactly as before; the stress test only runs when BOTH are set
 * (vm_virtual_probe=1 vm_video_stress=1). Default 0: completely inert
 * otherwise, same convention as sVmVirtualProbe above. */
int sVmVideoStress;
static const char __param_str_vm_video_stress[] = "vm_video_stress";
static struct kernel_param __param_vm_video_stress
	__attribute__((used, section("__param"), aligned(sizeof(void *)))) =
	{ __param_str_vm_video_stress, 0644, 0, param_set_int, param_get_int,
	  { &sVmVideoStress } };

/* Definition for the extern declared in omapnks4_internal.h - see that declaration's
 * own comment. Zero-initialized; real initialization happens via init_completion()
 * (not yet added - see docs/gaps.md), matching the pattern struct completion needs
 * before its first wait_for_completion()/complete() use. */
unsigned char sProbeComplete[0x10];

/* This module's own struct usb_driver object - the real architectural gap flagged
 * throughout this session's Kbuild work ("never constructed anywhere in this
 * codebase"). Ground truth: OmapNKS4Init's real call site
 * (`stg_usb_register_driver(driver=EAX, owner=EDX, mod_name=ECX)`,
 * OmapNKS4Module.ko 3.2.2 disassembly) loads EAX with a fixed static address
 * (0x1afa0); read_memory at that address, cross-checked field-by-field against
 * linux-kronos's real <linux/usb.h>/<linux/device.h> struct usb_driver layout:
 *
 *   +0x00 name        = "OmapNKS4"                  (confirmed: read_memory @0x1a946)
 *   +0x04 probe       = OmapNKS4Probe                (confirmed: matches its real @0x11180)
 *   +0x08 disconnect  = OmapNKS4Disconnect            (confirmed: matches its real @0x10ed0)
 *   +0x0c..+0x20 ioctl/suspend/resume/reset_resume/pre_reset/post_reset = NULL
 *                       (confirmed zero in the real binary - this driver implements none)
 *   +0x24 id_table    = &sOmapNKS4UsbDeviceIdTable   (confirmed: matches read_memory @0x1b040)
 *   +0x28..+0x6f      dynids(0xc)/drvwrap(0x38)/bitfields(4) - confirmed zero in the
 *                       real binary (usb_register_driver() populates these itself at
 *                       registration time, not something the driver author sets) -
 *                       total struct size 0x70 (112) bytes, computed from
 *                       linux-kronos's real struct sizes (usb_dynids=12,
 *                       usbdrv_wrap=struct device_driver(52)+int(4)=56, +4 for the
 *                       trailing bitfields), not guessed/over-provisioned.
 *
 * id_table's one real entry (ground truth: read_memory @0x1b040, cross-checked
 * against linux-kronos's real struct usb_device_id field offsets):
 *   match_flags=0x0383 (VENDOR|PRODUCT|INT_CLASS|INT_SUBCLASS|INT_PROTOCOL),
 *   idVendor=0x0944, idProduct=0x1005, bInterfaceClass=0xff, bInterfaceSubClass=0xff,
 *   bInterfaceProtocol=0x00, all other fields (bcdDevice_lo/hi, bDeviceClass/
 *   SubClass/Protocol, driver_info) confirmed zero. A second, all-zero entry
 *   terminates the table (standard kernel convention, matches the real memory
 *   dump's trailing zeros).
 */
unsigned char sOmapNKS4UsbDriver[0x70];
static unsigned char sOmapNKS4UsbDeviceIdTable[2][0x14]; /* struct usb_device_id[2], 0x14B each */

/* Structural note (full-coverage re-audit, 2026-07-18): this function itself
 * has NO counterpart in the real binary. Fresh disassembly of OmapNKS4Init@
 * 0x18d06 shows it loading `EAX=0x1afa0` (sOmapNKS4UsbDriver's address) as a
 * bare immediate directly before `CALL stg_usb_register_driver`, with zero
 * calls to anything resembling a setup/init helper in between (confirmed via
 * get_function_info's own callee list for OmapNKS4Init: no such function is
 * ever called). Ground truth's struct is compile-time-initialized static
 * data (an ordinary `struct usb_driver x = { .name = ..., .probe = ..., ... };`
 * in Korg's real source) with no runtime population code at all - this
 * reconstruction's runtime-zero-then-poke approach produces byte-identical
 * memory content before first use (every field value here was independently
 * read_memory-confirmed against the real 0x1afa0/0x1b040 dumps, see this
 * function's own field comments above), so it is behaviorally faithful, just
 * structured differently (a function + call, where ground truth has neither).
 * Documented rather than restructured into a static initializer: doing so
 * safely would require modeling real relocations (function pointers, a
 * nested table address) inside a raw byte array, a materially higher-risk
 * change for a purely cosmetic/structural difference with no runtime-visible
 * effect. */
static void init_omap_nks4_usb_driver(void)
{
	for (unsigned i = 0; i < sizeof(sOmapNKS4UsbDriver); i++)
		sOmapNKS4UsbDriver[i] = 0;
	*(const char **)(sOmapNKS4UsbDriver + 0x00) = "OmapNKS4";
	*(void **)(sOmapNKS4UsbDriver + 0x04) = (void *)OmapNKS4Probe;
	*(void **)(sOmapNKS4UsbDriver + 0x08) = (void *)OmapNKS4Disconnect;
	*(void **)(sOmapNKS4UsbDriver + 0x24) = sOmapNKS4UsbDeviceIdTable;

	for (unsigned i = 0; i < sizeof(sOmapNKS4UsbDeviceIdTable); i++)
		((unsigned char *)sOmapNKS4UsbDeviceIdTable)[i] = 0;
	unsigned char *e = sOmapNKS4UsbDeviceIdTable[0];
	*(unsigned short *)(e + 0x00) = 0x0383; /* match_flags */
	*(unsigned short *)(e + 0x02) = 0x0944; /* idVendor */
	*(unsigned short *)(e + 0x04) = 0x1005; /* idProduct */
	e[0x0d] = 0xff; /* bInterfaceClass */
	e[0x0e] = 0xff; /* bInterfaceSubClass */
	/* sOmapNKS4UsbDeviceIdTable[1] stays all-zero - the terminator entry */
}

extern "C" {
void init_cpp_support(void);
void cleanup_cpp_support(void);
/* Real signature (ground truth: create_thread@0x18c71 disassembly, 2026-07-16)
 * is (name, fn, running_flag) - NOT (fn, name) as previously assumed, and it
 * takes a THIRD int* argument it sets to 1 itself and the spawned thread's
 * exit path checks. See its definition below for the full real behavior
 * (a real local completion + kernel_thread(), not an external STG call at
 * all - "create_thread" doesn't appear in the stock module's own import
 * list, it's compiled in). */
int  create_thread(const char *name, int (*fn)(void *), int *running);
int  kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);
void wait_for_completion(void *completion);

int  COmapNKS4Driver_Configure(void);
void COmapNKS4Driver_HandleOutputSysReq(void); /* driver.cpp - the SRQ handler, see below */

/* Real Linux 2.6.32 kernel primitives not already declared in omapnks4_internal.h -
 * same opaque-void*-for-unparseable-real-types pattern used throughout this
 * session (see that header's own comment block for the established precedent).
 * `unsigned long`/`int`/`void*` stand in for jiffies/pid_t/task-related types this
 * file never needs to interpret, only pass through. */
int  rt_request_srq(unsigned int label, void (*handler)(void), void *rt_handler);
long wait_for_completion_timeout(void *completion, unsigned long timeout);
void daemonize(const char *name, ...);
/* block_all_signals() is NOT a real external call in this binary - ground truth
 * (fresh disassembly, 2026-07-18, OmapNKS4Module.ko@0x1048c-0x104b5 and its
 * ShutdownSSDRoutine twin @0x105ac-0x105d5) shows the classic open-coded
 * "block every signal" idiom fully inlined instead:
 *   spin_lock_irq(&current->sighand->siglock);
 *   current->blocked.sig[1] = ~0; current->blocked.sig[0] = ~0;  (sigfillset)
 *   recalc_sigpending();
 *   spin_unlock_irq(&current->sighand->siglock);
 * A previous session's `void block_all_signals(void);` extern called with zero
 * arguments doesn't exist under that name/signature in this kernel at all (the
 * real 2.6.32 block_all_signals() is a 3-arg NFS-lockd notifier helper with
 * completely different semantics) - this would have failed symbol resolution
 * at insmod, or if it happened to resolve, done nothing resembling "block every
 * signal". Task-struct offsets confirmed via disassembly: sighand @ +0x2a0,
 * blocked.sig[0]/[1] @ +0x2a4/+0x2a8; sighand_struct.siglock @ +0x504. */
void _spin_lock_irq(void *lock);
void _spin_unlock_irq(void *lock);
void recalc_sigpending(void);
void complete_and_exit(void *completion, int exit_code);
void msleep(unsigned int msecs);
void mutex_lock(void *lock);
void mutex_unlock(void *lock);
void *scsi_host_lookup(int index);
void *scsi_device_lookup(void *shost);
void scsi_device_set_state(void *sdev, int state);
void scsi_device_put(void *sdev);
void scsi_host_put(void *shost);
/* real STGEnabler.c signature: (struct task_struct*, int policy, struct sched_param*) -
 * opaque void* for the first (this module never dereferences a named field of it).
 * The third arg is NOT opaque - see ProcessMsgRoutine/ShutdownSSDRoutine's own call
 * sites below for why a real struct sched_param is required. */
int  stg_sched_setscheduler(void *task, int policy, void *param);
/* Real kernel layout (`struct sched_param { int sched_priority; };`, <linux/sched.h>) -
 * a single int, safe to reproduce directly without pulling in that header (matching
 * this file's own established "reproduce just the layout" pattern for wait_queue_head_t/
 * struct completion above the extern "C" block). Ground truth (fresh Ghidra disassembly,
 * 2026-07-17, ProcessMsgRoutine@0x10450 and ShutdownSSDRoutine@0x10570): both threads
 * construct `struct sched_param{.sched_priority=2}` on the stack and pass its address -
 * NOT NULL, which this reconstruction previously passed and which real-kernel
 * sched_setscheduler() NULL-derefs on immediately (`param->sched_priority`), confirmed
 * via a real "BUG: unable to handle kernel NULL pointer dereference... __sched_setscheduler"
 * oops on a live VM boot test (the first test ever to actually run either thread routine -
 * every earlier test failed/unloaded before OmapNKS4Init reached create_thread()). */
struct sched_param { int sched_priority; };
void *__kmalloc(unsigned int size, unsigned int flags);
extern unsigned int cpu_khz;
void *rt_whoami(void);
unsigned int ksize(void *p);
unsigned int hweight32(unsigned int x);
/* FIX (goal: clean VM-bootable build, 2026-07-17): `cpu_online_map` itself
 * (a raw exported symbol) is confirmed real ground truth for the actual
 * shipped Kronos kernel (see stg_num_online_cpus's own comment below) -
 * but this project's own /home/build/linux-kronos build tree, used for
 * VM-boot-test compilation, does not export it as a standalone symbol on
 * this specific kernel build/config (confirmed via a real "Unknown symbol
 * cpu_online_map" insmod failure). This kernel's own real replacement,
 * `cpu_online_mask` (`include/linux/cpumask.h`: `extern const struct
 * cpumask *const cpu_online_mask`), IS exported - opaque-typed here as a
 * raw `unsigned int *` rather than pulling in the full `<linux/cpumask.h>`
 * header (this project's own established convention for C++ translation
 * units against this kernel's C-only headers, see omapnks4.h's own
 * documented reason for avoiding <linux/usb.h>/<linux/types.h> the same
 * way). `struct cpumask` is a bitmap sized for this kernel's NR_CPUS; this
 * project's own target (Kronos 1-3 hardware, max 4 logical CPUs) fits
 * entirely within that bitmap's first 32-bit word, so reading it as a
 * plain `unsigned int` and applying the SAME hweight32() primitive
 * ground truth already uses is exact, not an approximation. */
extern unsigned int *cpu_online_mask;
/* Real Linux 2.6.32 wait-queue primitive (EXPORT_SYMBOL'd in kernel/sched.c) -
 * ground truth: ProcessMsgRoutine/ShutdownSSDRoutine both open-code the classic
 * pre-2.6.35 wait_event_timeout() expansion (prepare_to_wait()/schedule_timeout()/
 * finish_wait() - already declared in omapnks4_internal.h, same as submit.cpp's
 * use of them). autoremove_wake_function is the real DEFINE_WAIT() default wake
 * callback each wait_queue_t construction below points at, same as submit.cpp's
 * own struct omap_wait_entry usage. */
int  autoremove_wake_function(void *wait, unsigned int mode, int sync, void *key);
}

/* ===================================================================== *
 *  "STG cpp-support shim" primitives - CONFIRMED (2026-07-16) to not be a
 *  separate external layer at all for this module: none of stg_kmalloc/
 *  stg_kfree/stg_msleep/stg_get_cpu_khz/init_cpp_support/operator new/
 *  operator delete appear in the stock OmapNKS4Module.ko's own import list
 *  (readelf/objdump against the real 3.2.2 binary) - they're small LOCAL
 *  functions compiled directly into the module, ground-truthed here via
 *  fresh Ghidra disassembly of each one:
 *    stg_kmalloc(size) -> __kmalloc(size, 0xd0)   (0xd0 = GFP_KERNEL on this
 *      kernel: __GFP_WAIT|__GFP_IO|__GFP_FS = 0x10|0x40|0x80)
 *    stg_kfree(p)       -> kfree(p)
 *    stg_msleep(ms)     -> msleep(ms)
 *    stg_get_cpu_khz()  -> return cpu_khz (the real kernel's own global, not
 *      a function call in the real binary at all)
 *    operator new(size) -> stg_kmalloc(size); operator delete(p) -> stg_kfree(p)
 *    init_cpp_support() -> empty (confirmed: a single `ret`, no .ctors walk -
 *      this compiled binary has no global C++ objects needing runtime init)
 *  cleanup_cpp_support() in the real binary walks __DTOR_END__ then calls a
 *  further "stg_cpp_exit()" - CORRECTED 2026-07-18 (full-coverage re-audit):
 *  an earlier note here claimed reproducing this call "isn't worth the risk"
 *  of a new unresolved extern. That was wrong on the facts: stg_cpp_exit is
 *  ALSO a real local function inside this .ko (0x17f30, single `ret`, same
 *  category as the four above), not an extern at all - zero risk, now called
 *  unconditionally from cleanup_cpp_support() exactly as ground truth does
 *  (see stg_cpp_exit's own definition below). */
extern "C" {
void *stg_kmalloc(unsigned int size) { return __kmalloc(size, 0xd0); }
void  stg_kfree(void *p) { kfree(p); }
void  stg_msleep(unsigned int ms) { msleep(ms); }
unsigned int stg_get_cpu_khz(void) { return cpu_khz; }
/* Ground truth (fresh Ghidra decompile, 2026-07-17): stg_ksize/stg_is_linux_context/
 * stg_hweight32/stg_num_online_cpus are ALSO real, local functions inside this .ko
 * (0x17fd0-0x18020), same as the four above - not external STG-framework imports
 * either, contrary to this file's own header comment (now corrected). Each is a
 * thin wrapper over a real kernel/RTAI primitive. */
unsigned int stg_ksize(void *p) { return ksize(p); }
/* Same "am I called from Linux/non-RT context, or a real RT task" sentinel check
 * as rtwrap_pthread_join's (rtwrap.cpp) - rt_whoami()'s priority field ==
 * 0x7fffffff means the caller isn't a real RTAI task. */
int stg_is_linux_context(void)
{
	void *self = rt_whoami();
	return *(int *)((unsigned char *)self + 0x1c) == 0x7fffffff;
}
unsigned int stg_hweight32(unsigned int x) { return hweight32(x); }
/* Ground truth: real disassembly shows this calls the exact same hweight32()
 * primitive as stg_hweight32 above, over the kernel's real cpu_online_map bitmask -
 * i.e. "count of set bits in the online-CPU mask", the standard num_online_cpus()
 * definition from this kernel era (pre-cpumask_t rewrite).
 * FIX (goal: clean VM-bootable build, 2026-07-17): reads through
 * `cpu_online_mask` instead of `cpu_online_map` - see that extern's own
 * declaration comment above for why; same real value, real exported
 * symbol on this build tree. */
unsigned int stg_num_online_cpus(void) { return hweight32(*cpu_online_mask); }
void init_cpp_support(void) { }
/* Ground truth (fresh decompile, 2026-07-18, stg_cpp_exit@0x17f30): a real
 * LOCAL function inside this .ko (not an STGEnabler/RTAI extern - same
 * "compiled directly into the module" category as stg_kmalloc/stg_msleep
 * above), whose entire body is `{ return; }` (a single `ret`, 1 byte).
 * cleanup_cpp_support's previous no-op simplification was justified by
 * "adding a new unresolved extern isn't worth the risk" - that reasoning was
 * factually wrong: this is not an extern at all, it's a trivial local no-op
 * with zero link risk, so ground truth's own unconditional call to it (after
 * the, also empty in this binary, __DTOR_END__ walk) is now reproduced
 * exactly rather than skipped. */
void stg_cpp_exit(void) { }
void cleanup_cpp_support(void) { stg_cpp_exit(); }
}
void *operator new(unsigned int size) { return stg_kmalloc(size); }
void  operator delete(void *p) { stg_kfree(p); }

/* Ground truth: create_thread@0x18c71 real disassembly, 2026-07-16. Not an
 * external STG call (absent from the stock module's own import list) - a
 * real local synchronous "spawn + wait for the new thread to signal it's
 * up" helper. Builds a real (16-byte, same struct-completion storage
 * convention as sProbeComplete) completion object ON ITS OWN STACK, passes
 * it to kernel_thread() as the spawned thread's `arg`, and blocks on it -
 * the spawned thread's own routine is responsible for calling
 * complete(arg) early (see ProcessMsgRoutine/ShutdownSSDRoutine below) to
 * release this wait. flags=0xe00 confirmed via disassembly (CLONE_* bits
 * kernel_thread passes through unchanged). */
/* Real struct completion init (matches DECLARE_COMPLETION/init_completion's
 * own expansion: done=0, wait_queue_head_t{lock=0, list={&self,&self}}) -
 * factored out after a real kernel oops on real hardware, 2026-07-16:
 * sProbeComplete was declared and complete()'d/waited-on but never actually
 * initialized this way, so its wait_queue_head_t's list pointers were just
 * zero-BSS instead of self-linked, and wait_for_completion_timeout's real
 * implementation (wait_for_common) NULL-derefs walking a list that was
 * never a valid empty list. Every raw-byte-buffer completion object in this
 * file must be run through this before its first complete()/wait_for_*(). */
static inline void init_completion_struct(unsigned char *completion)
{
	*(int *)(completion + 0) = 0;			/* done = 0 */
	*(int *)(completion + 4) = 0;			/* wait_queue_head_t.lock = 0 */
	*(void **)(completion + 8)  = completion + 8;	/* wait list, self-linked */
	*(void **)(completion + 12) = completion + 8;
}

static int create_thread_impl(const char *name, int (*fn)(void *), int *running)
{
	unsigned char completion[0x10];
	init_completion_struct(completion);
	*running = 1;
	int pid = kernel_thread(fn, completion, 0xe00);
	if (pid < 0) {
		/* Ground truth (fresh disassembly + read_memory, 2026-07-18,
		 * create_thread@0x18c71): the real format string at 0x00019a58 is
		 * "%s: create_thread() failed. err %ld\n" - NO "<6>OmapNKS4:" prefix,
		 * unlike every other printk in this file. This is create_thread()'s
		 * own message, not routed through the module's usual log-prefix
		 * convention. Previous "<6>OmapNKS4:" prefix here was an unverified
		 * guess-by-analogy with the rest of the file - confirmed wrong. */
		printk("%s: create_thread() failed. err %ld\n", name, pid);
		*running = 0;
		return -1;
	}
	wait_for_completion(completion);
	if (*running == 0) {
		/* Ground truth (read_memory @0x0001a884): "%s thread failed in some
		 * way\n" - also no "<6>OmapNKS4:" prefix, confirmed via raw bytes. */
		printk("%s thread failed in some way\n", name);
		return -1;
	}
	return 0;
}
extern "C" int create_thread(const char *name, int (*fn)(void *), int *running)
{
	return create_thread_impl(name, fn, running);
}

/* Ground truth (OA.ko's own already-boot-tested `stg_get_current_task()`, see
 * kronosology/reconstructed/OA/src/stub/bar2_stubs_c.cpp's own extensive comment on
 * why this exact form - the real, EXPORT_PER_CPU_SYMBOL'd kernel symbol name
 * `per_cpu__current_task`, not a hardcoded offset, so it stays correct across
 * kernel rebuilds). Reused verbatim rather than re-deriving - this module's own
 * OmapNKS4Init/ProcessMsgRoutine/ShutdownSSDRoutine disassembly independently
 * confirms the exact same `mov %fs:<offset>,%reg` shape for "current" that
 * OA.ko's version was written to fix. */
static inline void *stg_get_current_task_nks4(void)
{
	void *current_task;
	asm volatile("mov %%fs:per_cpu__current_task, %0" : "=r"(current_task));
	return current_task;
}

/* Real, fully-inlined block_all_signals() idiom - see the extern block comment
 * above for the ground truth this reproduces. */
static inline void block_all_signals_nks4(void *current_task)
{
	unsigned char *task = (unsigned char *)current_task;
	void *siglock = *(void **)(task + 0x2a0);	/* current->sighand */
	siglock = (unsigned char *)siglock + 0x504;	/* &current->sighand->siglock */
	_spin_lock_irq(siglock);
	*(unsigned int *)(task + 0x2a8) = 0xffffffff;	/* blocked.sig[1] = ~0 */
	*(unsigned int *)(task + 0x2a4) = 0xffffffff;	/* blocked.sig[0] = ~0 */
	recalc_sigpending();
	_spin_unlock_irq(siglock);
}

/* Real, fully-inlined wait_event_timeout() expansion - see prepare_to_wait's own
 * extern comment (omapnks4_internal.h) for ground truth. wait_queue_t layout (the
 * real DEFINE_WAIT() stack object): unsigned flags(4) + struct task_struct *task(4)
 * + wait_queue_func_t func(4) + struct list_head task_list(8) = 20 bytes,
 * self-linked like every other list_head this codebase already models this way
 * (sProbeComplete etc. above). Same layout as submit.cpp's own struct
 * omap_wait_entry - not shared across translation units since each file already
 * has its own established "opaque byte buffer" convention for kernel structs. */
static inline void init_wait_entry_nks4(unsigned char *wait, void *task)
{
	*(unsigned int *)(wait + 0)  = 0;					/* flags */
	*(void **)(wait + 4)         = task;					/* task */
	*(void **)(wait + 8)         = (void *)autoremove_wake_function;	/* func */
	*(void **)(wait + 12) = wait + 12;					/* task_list, self-linked */
	*(void **)(wait + 16) = wait + 12;
}

/* wait_queue_head_t-sized storage (12 bytes: spinlock_t + list_head), same
 * convention as usb.cpp's sCommandWaitQueue/sVideoWaitQueue/sReadWaitQueue.
 * Ground truth: OmapNKS4Exit's own __wake_up() call sites target these exact
 * addresses (0x1af60/0x1af7c in the real binary), and they sit immediately
 * before sMsgThreadComplete/sSsdThreadComplete respectively in the real .bss
 * layout - confirmed via fresh disassembly, 2026-07-18. Initialized via
 * __init_waitqueue_head() in OmapNKS4Init, below. */
static unsigned char sVideoMsgWaitQueue[0xc];
static unsigned char sShutdownSSDWaitQueue[0xc];
static const char sMainWaitQueueLockKeyDummy[1] = {0};

/* extern, not static: real, shared definitions live in usb.cpp - see its own
 * comments on each of these three for why (duplicate-static bug, confirmed
 * on real hardware, 2026-07-16). */
extern int sDriverState;
extern int sSTG2NKS4SrqNumber;
extern void *sInterruptURB;

/* thread state / wake flags */
int sProcessMsgThreadRunning, sVideoMsgSignalled;
int sShutdownSSDThreadRunning, sShutdownSSDSignaled;
static int sIsSSDReadyToShutdown;

/* Completion objects for the two service threads' exit-join sync - same 16-byte
 * struct-completion storage convention as sProbeComplete.
 *
 * RESOLVED 2026-07-17 (fresh disassembly of `OmapNKS4Exit@0x18f1d`): the "race"
 * this comment used to flag doesn't actually exist. `arg` (see create_thread_impl
 * above) is a completion object allocated ON THE STACK of the *calling* thread's
 * `create_thread()` invocation, used exactly once to synchronize "the new kernel
 * thread has started" back to the caller - it is a structurally different piece of
 * memory from `sMsgThreadComplete`/`sSsdThreadComplete` (static globals) in every
 * possible execution, not just in practice. There is no reuse of one object for two
 * purposes, so no race is possible between "thread started" and "thread exited"
 * signals - the original worry was unfounded.
 *
 * What WAS wrong, and is now fixed: the exit-time timeout. `OmapNKS4Exit`'s two
 * `wait_for_completion_timeout()` calls (one per thread) both use the literal
 * immediate `MOV EDX,0x7d0` (2000 jiffies = 2 seconds at this kernel's 1000 Hz
 * timer) immediately before each call - not 10000 (10 seconds) as this file
 * previously guessed by analogy with `sProbeComplete`'s real (and different)
 * 10000-jiffy probe-wait timeout. The two timeouts are simply unrelated constants
 * in the real binary; guessing they'd match was the error. */
static unsigned char sMsgThreadComplete[0x10];
static unsigned char sSsdThreadComplete[0x10];

int ProcessMsgRoutine(void *arg);
int ShutdownSSDRoutine(void *arg);

/* ===================================================================== */

/* Ground truth (fresh disassembly + search_strings, 2026-07-18, OmapNKS4Init@
 * 0x18d06): EVERY printk in this function - and, per the same pass, in
 * OmapNKS4ProcWrite/OmapNKS4ProcWriteProgress/OmapNKS4ProcInitialize/
 * OmapNKS4ProcDone (procfs.cpp) too - carries a real "<6>OmapNKS4:%s: line
 * %d: <message>\n" format, with the function's own name string and a real
 * embedded source-line-number literal as the first two args (the same
 * convention already established/fixed for WriteCallback's "%s: line %d:
 * ERROR..." messages in usb.cpp, and for the SubmitNKS4CommandWrite/
 * WaitForNKS4CommandWrite messages in submit.cpp). This file's printks had
 * never been checked against this convention before - confirmed via
 * search_strings against the real .rodata pool (0x1a29c-0x1a460) and
 * cross-checked instruction-by-instruction. Line numbers below are the real
 * embedded immediates, Korg's own original source line numbers (meaningless
 * to us as line numbers, but part of the exact byte-for-byte message). */
#define OMAPNKS4INIT_FN "OmapNKS4Init"

static int __init OmapNKS4Init(void)
{
	init_cpp_support();		/* run C++ global constructors */
	printk("<6>OmapNKS4:%s: line %d: OmapNKS4Init: enter\n", OMAPNKS4INIT_FN, 0x571);
	sDriverState = 0;
	/* Real completion-struct init, not just BSS-zero - see
	 * init_completion_struct's own comment for why this is load-bearing
	 * (confirmed via a real kernel oops on real hardware, 2026-07-16). Must
	 * happen before stg_usb_register_driver below, since the probe callback
	 * that complete()s sProbeComplete can fire as soon as registration
	 * succeeds. */
	init_completion_struct(sProbeComplete);
	init_completion_struct(sMsgThreadComplete);
	init_completion_struct(sSsdThreadComplete);
	/* Same "must be genuinely initialized, not just BSS-zero" requirement as the
	 * completions above applies to these two wait queues - see their own
	 * declaration comment. */
	__init_waitqueue_head(sVideoMsgWaitQueue, (void *)sMainWaitQueueLockKeyDummy);
	__init_waitqueue_head(sShutdownSSDWaitQueue, (void *)sMainWaitQueueLockKeyDummy);
	init_omap_nks4_usb_driver();

	/* Ground truth (fresh disassembly, 2026-07-15, OmapNKS4Init@0x18d06):
	 * label=0x4e4b5334 ("NKS4" packed as a 4-char tag, matching the same
	 * ownerTag convention OA's own daemon_lifecycle.cpp uses, e.g. "OACD..."),
	 * handler=COmapNKS4Driver_HandleOutputSysReq (confirmed: real address
	 * 0x14a80 matches that function exactly), rt_handler=NULL. */
	sSTG2NKS4SrqNumber = rt_request_srq(0x4e4b5334u, COmapNKS4Driver_HandleOutputSysReq, 0);
	if (sSTG2NKS4SrqNumber < 1) {
		printk("<6>OmapNKS4:%s: line %d: OmapNKS4Init: could not get srq!\n", OMAPNKS4INIT_FN, 0x578);
		goto fail;
	}
	if (stg_usb_register_driver((struct usb_driver *)sOmapNKS4UsbDriver, &__this_module,
	                             "OmapNKS4") != 0) {
		printk("<6>OmapNKS4:%s: line %d: OmapNKS4Init: Cannot register nks4 usb driver!\n",
		       OMAPNKS4INIT_FN, 0x581);
		goto cleanup;
	}

	/* VM-only, vm_virtual_probe=1: synthesize a virtual NKS4 board and call the
	 * real OmapNKS4Probe() directly, on this same thread, before the wait below
	 * even starts. NOT a workqueue/second-module handoff - a real (separate,
	 * insmod-time) OmapNKS4ProbeInject.ko was tried first and failed with a
	 * genuine kernel "gave up waiting for init of module OmapNKS4Module."
	 * message: the kernel's own module loader refuses to resolve a symbol
	 * exported by a module still in MODULE_STATE_COMING (i.e. still inside its
	 * own init_module(), which is exactly this function) - confirmed on a live
	 * VM boot test, 2026-07-17. Calling OmapNKS4Probe() inline sidesteps that
	 * entirely: OmapNKS4Probe()'s own complete(&sProbeComplete) at the end just
	 * pre-satisfies the completion the wait below is about to check - a
	 * well-defined complete-before-wait ordering, not a race. */
	if (sVmVirtualProbe) {
		printk("<6>OmapNKS4: vm_virtual_probe=1, synthesizing a virtual NKS4 board\n");
		vm_virtual_probe_inject();
	}

	/* probe() runs from the USB core and completes this; wait for it. Ground truth
	 * (fresh disassembly): timeout = 0x2710 = 10000 (jiffies).
	 *
	 * Ground truth ALSO brackets this wait in a pair of rdtsc() reads and two
	 * further printks ("Waited %lu cycles..." / "current = %llu, before =
	 * %llu") - entirely missing from this reconstruction until now (fresh
	 * disassembly, 2026-07-18: the real string pool has both formats at
	 * 0x1a354/0x1a3a8, and OmapNKS4Init's own decompile shows the real
	 * printk args exactly: name, line, (after_tsc_lo - before_tsc_lo) as the
	 * "%lu cycles" value, then the full 64-bit after/before pair for the
	 * second line). "cycles" is a crude, truncated low-dword-only delta,
	 * faithfully reproduced as such rather than "improved" to a real 64-bit
	 * subtraction - that's what the real binary computes. */
	unsigned long long tsc_before, tsc_after;
	asm volatile("rdtsc" : "=A"(tsc_before));
	wait_for_completion_timeout(sProbeComplete, 10000);
	asm volatile("rdtsc" : "=A"(tsc_after));
	printk("<6>OmapNKS4:%s: line %d: Waited %lu cycles for OmapNKS4Probe(). driver state is %d\n",
	       OMAPNKS4INIT_FN, 0x58a,
	       (unsigned long)((unsigned int)tsc_after - (unsigned int)tsc_before), sDriverState);
	printk("<6>OmapNKS4:%s: line %d: current = %llu, before = %llu\n",
	       OMAPNKS4INIT_FN, 0x58b, tsc_after, tsc_before);
	if (sDriverState != 1) {
		printk("<6>OmapNKS4:%s: line %d: OmapNKS4Init: probe failed\n", OMAPNKS4INIT_FN, 0x58f);
		goto cleanup;
	}

	/* Ground truth (fresh disassembly, 2026-07-18, OmapNKS4Init@0x18e72-0x18e7c):
	 * OmapNKS4ProcInitialize()'s return value is NOT checked at all in the real
	 * binary - it's called unconditionally and execution falls straight through
	 * to the interrupt-URB submit regardless of success/failure (no TEST/JZ
	 * between the two CALLs). A previous session's `if (... != 0) goto cleanup;`
	 * was a plausible-looking but ground-truth-contradicted defensive check;
	 * removed to match the real (arguably sloppy, but that's what the real
	 * driver does) control flow exactly. The 1000ms "DIAG" sleep and interrupt-
	 * URB field dump that used to sit here were a same-session timing
	 * experiment, never grounded in disassembly evidence, and are also absent
	 * from the real binary (confirmed: only 10 bytes / two instructions
	 * separate the OmapNKS4ProcInitialize() and stg_usb_submit_urb() call sites
	 * in the real code - no room for a msleep or extra printk) - removed. */
	OmapNKS4ProcInitialize();

	/* mem_flags=0xd0 (GFP_KERNEL) - ground truth: real OmapNKS4Init@0x18e77
	 * disassembly - this is the interrupt URB's initial submit, running in
	 * process context (init_module's own calling context), unlike the
	 * atomic-context resubmit inside InterruptCallback or the command-write
	 * submit paths (both GFP_ATOMIC=0x20 in the real binary). All of these
	 * were previously hardcoded 0 - not blocking submission (confirmed:
	 * rc==0 either way in live testing), but a genuine deviation from
	 * ground truth, confirmed 2026-07-16. */
	if (vm_usb_submit_urb(sInterruptURB, 0xd0) != 0) {	/* start the interrupt-IN xfer */
		/* Ground truth (search_strings @0x1a418): real text is "error
		 * stg_usb_submit_urb for interrupt xfer", not "error submitting
		 * interrupt xfer" - both the wording and the %s/%d prefix were
		 * wrong before this pass. */
		printk("<6>OmapNKS4:%s: line %d: error stg_usb_submit_urb for interrupt xfer\n",
		       OMAPNKS4INIT_FN, 0x598);
		goto cleanup;
	}
	/* REMOVED (Opus review, 2026-07-18): this "TEMP diagnostic" printk does
	 * not exist in ground truth - fresh disassembly of OmapNKS4Init@0x18d06
	 * proceeds straight from the interrupt-URB submit success path to
	 * COmapNKS4Driver_Configure(), no printk in between. Same class of finding
	 * as the other "TEMP diagnostic"/"DIAG" printks removed this pass. */
	if (COmapNKS4Driver_Configure() != 0) {
		printk("<6>OmapNKS4:%s: line %d: Problem configuring OmapNKS4 in Init\n",
		       OMAPNKS4INIT_FN, 0x5a1);
		goto cleanup;
	}

	/* TEMPORARY DIAGNOSTIC (2026-07-19 rate-study continuation session) - not
	 * ground-truthed, remove once the hang below is root-caused. Ungated
	 * (matches this file's existing PMR#/SSD# diagnostic convention) since
	 * these three calls always run, VM or real hardware, and this is exactly
	 * the gap a 20-run hang-rate study (README.md "Hang-rate study") plus a
	 * live QEMU-monitor capture of a real hang (run_20260719_150641) narrowed
	 * the recurring 45%-of-runs stall down to: that capture showed both
	 * worker threads already alive and ticking normally (proving
	 * create_thread() x2 below had already succeeded and returned), yet
	 * "vm_virtual_probe_inject_event"'s own first printk ("installer support
	 * enabled", usb.cpp) - the very next tracked milestone - was never
	 * reached in ANY of the 9 hung runs. The only code between "both worker
	 * threads alive" and that printk is CActiveSenseThread_Setup() (a real
	 * RTAI real-time task creation - a DIFFERENT earlier session refuted this
	 * exact function as the cause, but against an older, simpler main.cpp
	 * before the video-stress/event-injection additions changed what runs
	 * concurrently with it) and the bare `if (sVmVirtualProbe)` guard/call
	 * itself. These markers will show, on the next live hang, whether
	 * execution ever gets past CActiveSenseThread_Setup() at all. */
	printk("<6>OmapNKS4: DIAG about to create_thread(kOmapNKS4MsgRoutine)\n");
	create_thread("kOmapNKS4MsgRoutine", ProcessMsgRoutine, &sProcessMsgThreadRunning);
	printk("<6>OmapNKS4: DIAG kOmapNKS4MsgRoutine create_thread() returned, running=%d\n", sProcessMsgThreadRunning);
	create_thread("kShutdownSSDRoutine", ShutdownSSDRoutine, &sShutdownSSDThreadRunning);
	printk("<6>OmapNKS4: DIAG kShutdownSSDRoutine create_thread() returned, running=%d\n", sShutdownSSDThreadRunning);
	printk("<6>OmapNKS4: DIAG about to call CActiveSenseThread_Setup()\n");
	CActiveSenseThread_Setup();
	printk("<6>OmapNKS4: DIAG CActiveSenseThread_Setup() returned\n");

	/* VM-only, 2026-07-17 continuation of the vm_virtual_probe work above: the
	 * board is now fully probed/configured/running (COmapNKS4Driver_Configure()
	 * succeeded, both worker threads + the active-sense thread are up) - the
	 * same point a real device would already be sending interrupt-IN traffic
	 * for actual front-panel activity. Feed one synthetic runtime event
	 * through the real InterruptCallback()/ReceiveEventBuffer() decode path
	 * here, so this test also covers "board stays running and correctly
	 * processes runtime traffic", not just the boot/probe/configure sequence
	 * the two functions above already proved. See
	 * vm_virtual_probe_inject_event()'s own comment (usb.cpp) for the exact
	 * packet layout and ground-truth byte patterns it's based on. */
	if (sVmVirtualProbe) {
		vm_virtual_probe_inject_event();
		/* SetLCDBrightness/ResetModule (command.cpp): neither is on this
		 * module's own real boot/configure path (see
		 * vm_virtual_probe_test_setters()'s own comment, usb.cpp), so
		 * nothing above would exercise their new vm_usb_submit_urb
		 * dispatch coverage - call them directly here instead. */
		vm_virtual_probe_test_setters();
		/* VM-only, 2026-07-18: real-concurrency video-ring stress test (see
		 * video.cpp's own definition) - runs only when vm_video_stress=1 is
		 * ALSO set (checked internally); the board is fully configured and
		 * kOmapNKS4MsgRoutine is already alive at this point, exactly the
		 * real single-consumer configuration this test needs. */
		vm_virtual_probe_stress_test_video();
	}

	/* TEMPORARY DIAGNOSTIC (2026-07-19 continuation session) - not ground-truthed,
	 * remove before final commit. Confirms whether OmapNKS4Init's own bare `return 0`
	 * is actually reached and whether control genuinely leaves this function. */
	printk("<6>OmapNKS4: DIAG OmapNKS4Init about to return 0\n");
	return 0;

cleanup:
	CleanupOmapNKS4Driver();
fail:
	cleanup_cpp_support();
	return -1;
}

static void __exit OmapNKS4Exit(void)
{
	CActiveSenseThread_Cleanup();

	/* Ground truth (fresh disassembly, 2026-07-18, OmapNKS4Exit@0x18f2c-0x18f8f):
	 * a real __wake_up(q, mode=3 [TASK_INTERRUPTIBLE|TASK_UNINTERRUPTIBLE],
	 * nr_exclusive=1, key=NULL) IS present between setting each running flag to 0
	 * and its join-wait, targeting the SAME wait_queue_head_t object each
	 * thread's own ProcessMsgRoutine/ShutdownSSDRoutine wait_event_timeout() loop
	 * waits on (confirmed: identical addresses on both sides) - kicking the
	 * thread out of its sleep immediately rather than waiting for the loop's own
	 * short timeout to lapse. A previous session's polling-loop simplification
	 * for those two wait loops (since fixed - see their own comments) made this
	 * __wake_up() a no-op and it was passed queue=0/NULL to match; now real. */
	sProcessMsgThreadRunning = 0;
	__wake_up(sVideoMsgWaitQueue, 3, 1, 0);
	wait_for_completion_timeout(sMsgThreadComplete, 2000);	/* join the msg thread */

	sShutdownSSDThreadRunning = 0;
	__wake_up(sShutdownSSDWaitQueue, 3, 1, 0);
	wait_for_completion_timeout(sSsdThreadComplete, 2000);	/* join the ssd thread */

	CleanupOmapNKS4Driver();
	cleanup_cpp_support();
}

/* ===================================================================== */

/* Video message-processor: wake on SignalVideoMessageProcessor(), drain the ring.
 * Ground truth (fresh disassembly, 2026-07-18, OmapNKS4Module.ko@0x104d8-0x1053d):
 * the real code does NOT poll - it open-codes the classic pre-2.6.35
 * wait_event_timeout(sVideoMsgWaitQueue, sVideoMsgSignalled, 3) expansion (a real
 * wait-queue sleep with prepare_to_wait() called every loop iteration, per that
 * macro's real 2.6.32 form - not an inefficiency, the actual compiled shape).
 * timeout=3 jiffies is a tight safety-net poll granularity in case a wakeup is
 * ever missed; the real wake normally comes from __wake_up() (SignalVideo-
 * MessageProcessor / OmapNKS4Exit), not from this timeout lapsing. */
int ProcessMsgRoutine(void *arg)
{
	daemonize("kOmapNKS4MsgRoutine");
	struct sched_param sp; sp.sched_priority = 2;
	stg_sched_setscheduler(stg_get_current_task_nks4(), 2 /* SCHED_RR */, &sp);
	block_all_signals_nks4(stg_get_current_task_nks4());
	/* `arg` is create_thread's own transient on-stack completion object (see
	 * its definition above) - completing THIS, not a separate static, is what
	 * actually releases create_thread's wait and lets OmapNKS4Init proceed.
	 * Ground truth: real disassembly shows this complete() call using the
	 * exact same register the incoming `arg` parameter arrived in, untouched
	 * since function entry (confirmed on real hardware, 2026-07-16 - the
	 * previous version of this function ignored `arg` entirely and completed
	 * sMsgThreadComplete here too, which is wrong: that object is for
	 * OmapNKS4Exit's separate join-wait, completed below via
	 * complete_and_exit, not this startup signal). */
	complete(arg);

	/* TEMPORARY DIAGNOSTIC (2026-07-19 continuation session) - not ground-truthed,
	 * remove before final commit. Confirms whether this thread's schedule_timeout()
	 * call ever genuinely blocks (large TSC delta between prints, consistent with a
	 * real ~3-jiffy sleep) vs spins (schedule_timeout returns near-instantly, so the
	 * outer while() re-enters this loop thousands of times/sec - tiny TSC deltas,
	 * print counter racing to its cap almost immediately). Capped at 40 total prints
	 * so a genuine spin can't flood/starve the console further. */
	static int sDiagPmrPrints;
	/* NOTE: deliberately unsigned int (32-bit), not unsigned long long - a 64-bit/
	 * 64-bit or 64-bit/32-bit division here lowers to a libgcc __udivdi3 call, which
	 * does NOT exist in-kernel (confirmed the hard way: an earlier version of this
	 * diagnostic using `unsigned long long diag_khz` and `diag_delta / diag_khz`
	 * produced a real "OmapNKS4Module: Unknown symbol __udivdi3" insmod failure -
	 * the module never loaded at all, which would have been silently misread as
	 * "the hang is fixed" by the test script's milestone matching. Printing the raw
	 * cycle delta and khz separately and doing the ms math by hand avoids any
	 * in-kernel 64-bit division entirely). */
	unsigned int diag_khz = stg_get_cpu_khz();
	printk("<6>OmapNKS4: DIAG ProcessMsgRoutine alive, cpu_khz=%u, entering main loop\n", diag_khz);

	while (sProcessMsgThreadRunning) {
		if (!sVideoMsgSignalled) {
			unsigned char wait[20];
			long timeout = 3;	/* ground truth: real EBX=0x3, @0x10503 */
			init_wait_entry_nks4(wait, stg_get_current_task_nks4());
			for (;;) {
				prepare_to_wait(sVideoMsgWaitQueue, wait, 2 /* TASK_UNINTERRUPTIBLE */);
				if (sVideoMsgSignalled)
					break;
				unsigned long long diag_before = omapnks4_rdtsc();
				timeout = schedule_timeout(timeout);
				if (sDiagPmrPrints < 40) {
					unsigned long long diag_delta = omapnks4_rdtsc() - diag_before;
					unsigned int diag_delta_hi = (unsigned int)(diag_delta >> 32);
					unsigned int diag_delta_lo = (unsigned int)diag_delta;
					sDiagPmrPrints++;
					printk("<6>OmapNKS4: DIAG PMR#%d schedule_timeout returned %ld, delta_hi=%u delta_lo=%u cycles, khz=%u\n",
					       sDiagPmrPrints, timeout, diag_delta_hi, diag_delta_lo, diag_khz);
					if (sDiagPmrPrints == 40)
						printk("<6>OmapNKS4: DIAG PMR suppressing further prints (cap reached)\n");
				}
				if (!timeout)
					break;
			}
			finish_wait(sVideoMsgWaitQueue, wait);
		}
		sVideoMsgSignalled = 0;
		if (!sProcessMsgThreadRunning)
			break;
		OmapNKS4VideoAPIProcessEvents();
	}
	sProcessMsgThreadRunning = 0;
	complete_and_exit(sMsgThreadComplete, 0);
	return 0;
}

/*
 * SSD-shutdown thread: when the panel signals a power-off (SignalShutdownSSD), make
 * the internal SSD safe by walking the SCSI hosts, flushing/quiescing each device
 * (scsi_device_set_state + the device's ->shutdown), then sleeping and telling the
 * panel firmware to power down.
 */
int ShutdownSSDRoutine(void *arg)
{
	daemonize("kShutdownSSDRoutine");
	struct sched_param sp; sp.sched_priority = 2;
	stg_sched_setscheduler(stg_get_current_task_nks4(), 2 /* SCHED_RR */, &sp);
	block_all_signals_nks4(stg_get_current_task_nks4());
	/* see ProcessMsgRoutine's own comment above - same fix, same reasoning. */
	complete(arg);

	/* TEMPORARY DIAGNOSTIC (2026-07-19 continuation session) - not ground-truthed,
	 * remove before final commit. See ProcessMsgRoutine's own identical diagnostic
	 * above for the reasoning; same spin-vs-block check, this thread's 10000-jiffy
	 * (~10s) timeout. */
	static int sDiagSsdPrints;
	/* unsigned int, not unsigned long long - see ProcessMsgRoutine's identical note
	 * above (real __udivdi3 insmod failure hit once by dividing 64-bit values in our
	 * own code; avoided here by never dividing, only printing raw values). */
	unsigned int diag_ssd_khz = stg_get_cpu_khz();
	printk("<6>OmapNKS4: DIAG ShutdownSSDRoutine alive, cpu_khz=%u, entering main loop\n", diag_ssd_khz);

	for (;;) {
		/* Real wait_event_timeout(sShutdownSSDWaitQueue, sShutdownSSDSignaled,
		 * 10000) - ground truth (fresh disassembly, 2026-07-18,
		 * OmapNKS4Module.ko@0x105f0-0x10658): same real wait-queue mechanism as
		 * ProcessMsgRoutine's fix above, not a poll loop. 10000 (jiffies) is
		 * ground-truthed via the real MOV EBX,0x2710 immediate at this call site. */
		if (!sShutdownSSDSignaled) {
			unsigned char wait[20];
			long timeout = 10000;
			init_wait_entry_nks4(wait, stg_get_current_task_nks4());
			for (;;) {
				prepare_to_wait(sShutdownSSDWaitQueue, wait, 2 /* TASK_UNINTERRUPTIBLE */);
				if (sShutdownSSDSignaled)
					break;
				unsigned long long diag_ssd_before = omapnks4_rdtsc();
				timeout = schedule_timeout(timeout);
				if (sDiagSsdPrints < 20) {
					unsigned long long diag_ssd_delta = omapnks4_rdtsc() - diag_ssd_before;
					unsigned int diag_ssd_delta_hi = (unsigned int)(diag_ssd_delta >> 32);
					unsigned int diag_ssd_delta_lo = (unsigned int)diag_ssd_delta;
					sDiagSsdPrints++;
					printk("<6>OmapNKS4: DIAG SSD#%d schedule_timeout returned %ld, delta_hi=%u delta_lo=%u cycles, khz=%u\n",
					       sDiagSsdPrints, timeout, diag_ssd_delta_hi, diag_ssd_delta_lo, diag_ssd_khz);
					if (sDiagSsdPrints == 20)
						printk("<6>OmapNKS4: DIAG SSD suppressing further prints (cap reached)\n");
				}
				if (!timeout)
					break;
			}
			finish_wait(sShutdownSSDWaitQueue, wait);
		}
		if (!sShutdownSSDThreadRunning) {
			sShutdownSSDThreadRunning = 0;
			complete_and_exit(sSsdThreadComplete, 0);
			return 0;
		}
		if (sIsSSDReadyToShutdown || !sShutdownSSDSignaled)
			continue;

		/* Stop the internal SSD's SCSI device - ground truth corrected via fresh
		 * disassembly, 2026-07-17 (`ShutdownSSDRoutine@0x10570`). Two real bugs in
		 * the previous version of this block, both now fixed:
		 *
		 * 1. This is NOT "walk every host, shut down every device found" (the old
		 *    open-ended `for (host = 0; ; host++)` model). The real code tries
		 *    exactly FOUR fixed SCSI host indices in order - 0, then 1, then 2,
		 *    then 3 (four distinct `scsi_host_lookup(N)` call sites at fixed
		 *    immediates in the disassembly, not a loop) - and shuts down only the
		 *    FIRST one that yields a device at channel 0/id 0/lun 0, then jumps
		 *    straight back to the outer wait loop. If none of the four hosts have
		 *    a device, it falls through to the sleep/signal-clear tail having
		 *    shut down nothing.
		 * 2. `scsi_device_set_state`'s real values are SDEV_CANCEL=3 then
		 *    SDEV_DEL=4 (confirmed against this kernel tree's
		 *    include/scsi/scsi_device.h: SDEV_CREATED=1, SDEV_RUNNING=2,
		 *    SDEV_CANCEL=3, SDEV_DEL=4, SDEV_QUIESCE=5, SDEV_OFFLINE=6) - the
		 *    previous guess (6 then 1) was wrong on both the values AND which
		 *    named state they corresponded to. CANCEL-then-DEL is exactly the
		 *    same two-step sequence the kernel's own __scsi_remove_device() uses -
		 *    this routine is essentially a manual, inlined scsi_remove_device().
		 * CORRECTION (re-verification pass, 2026-07-17): two real bugs found in
		 * the block below via fresh disassembly, both now fixed:
		 *  1. Both shutdown_fn(...) calls previously passed the wrong argument.
		 *     The first call's real disassembly is `LEA EAX,[EBX+0xb0]` (EBX=sdev)
		 *     immediately before `CALL EDX` - it passes `sdev+0xb0` (the embedded
		 *     device struct), NOT `dev` (the function-pointer lookup object
		 *     itself). The second call's real disassembly is `MOV EAX,EBX` before
		 *     `CALL EDX` - it passes `sdev` itself, NOT the computed `bus`. In
		 *     both cases the function-pointer LOOKUP (via `dev`/`bus`) was already
		 *     correct; only the call's own argument was wrong - a real instance of
		 *     this project's recurring "function pointer resolved via one object,
		 *     but a DIFFERENT related object is passed as the argument" pattern.
		 *  2. `sScsiShutdownLock` was a placeholder static buffer, not a real
		 *     lock object. Ground truth: `mutex_lock`/`mutex_unlock` operate on
		 *     `shost + 0x30` - a field embedded in the looked-up `Scsi_Host`
		 *     itself (`LEA EAX,[EDI+0x30]`, EDI=shost), not a module-private
		 *     static. Removed the placeholder; using the real field instead. */
		for (int host = 0; host < 4; host++) {
			void *shost = scsi_host_lookup(host);
			if (!shost)
				continue;
			void *sdev = scsi_device_lookup(shost);
			if (!sdev) {
				scsi_host_put(shost);
				continue;
			}
			mutex_lock((char *)shost + 0x30);
			scsi_device_set_state(sdev, 3 /* SDEV_CANCEL */);
			{
				void *dev = ((void **)sdev)[0x3e];
				if (dev) {
					void (*shutdown_fn)(void *) =
						*(void (**)(void *))((char *)dev + 0x1c);
					if (shutdown_fn) shutdown_fn((char *)sdev + 0xb0);
				}
			}
			scsi_device_set_state(sdev, 4 /* SDEV_DEL */);
			{
				void *bus = *(void **)(*(void **)sdev);
				bus = *(void **)((char *)bus + 0x60);
				void (*shutdown_fn)(void *) =
					*(void (**)(void *))((char *)bus + 0x3c);
				if (shutdown_fn) shutdown_fn(sdev);
			}
			mutex_unlock((char *)shost + 0x30);
			scsi_device_put(sdev);
			scsi_host_put(shost);
			break;	/* only the first host with a device is shut down */
		}

		/* msleep(1000) - ground-truthed via the literal `MOV EAX,0x3e8` immediate
		 * immediately before this call (was msleep(500), an unverified guess). */
		msleep(1000);
		sShutdownSSDSignaled = 0;
		/* CORRECTION (re-verification pass, 2026-07-17): the literal 0 argument
		 * here was confirmed WRONG via fresh disassembly - the real call site is
		 * `MOV EAX,[0x1b6b4]` immediately before the call, i.e. it loads a global
		 * (bss, address 0x1b6b4 in the real binary) and passes that, not a
		 * constant. The global's own symbol name/real meaning couldn't be
		 * resolved this pass (unreadable statically from an uninitialized bss
		 * address in the .ko) - left as an honestly-flagged extern rather than a
		 * guessed literal. COmapNKS4Driver_ShutDown's own parameter meaning is
		 * still unconfirmed - see its definition in driver.cpp.
		 *
		 * FIX (goal: clean VM-bootable build, 2026-07-17): this is a real
		 * internal-to-this-module global (BSS, address 0x1b6b4 sits well
		 * inside OmapNKS4Module.ko's own image, not an external kernel/RTAI
		 * symbol) - it must be DEFINED here, not left as a dangling extern
		 * with no definition anywhere (an "Unknown symbol" insmod failure
		 * waiting to happen). No write site to this global was found this
		 * pass, so it's modeled as a plain static int, zero-initialized by
		 * BSS semantics same as the real binary's own uninitialized data
		 * section - matches the real load site's own observed behavior
		 * (nothing in this reconstruction has ever seen a non-zero read). */
		static int sOmapNKS4DriverShutdownArg;
		COmapNKS4Driver_ShutDown(sOmapNKS4DriverShutdownArg);
		sIsSSDReadyToShutdown = 1;
	}
}

module_init(OmapNKS4Init);
module_exit(OmapNKS4Exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Korg Kronos OMAP NKS4 front-panel USB driver");
MODULE_AUTHOR("Korg (reconstructed)");
