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
void block_all_signals(void);
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
 * opaque void* for the first/third (this module never dereferences a named field of
 * either). */
int  stg_sched_setscheduler(void *task, int policy, void *param);
void *__kmalloc(unsigned int size, unsigned int flags);
extern unsigned int cpu_khz;
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
 *  further "stg_cpp_exit()" - NOT reproduced: this module (like the real
 *  one, per init_cpp_support being empty) has no global destructors needing
 *  it either, and adding a new unresolved "stg_cpp_exit" extern for a
 *  module-unload-path no-op isn't worth the risk; left as a no-op with this
 *  note rather than silently claimed identical to the real binary. */
extern "C" {
void *stg_kmalloc(unsigned int size) { return __kmalloc(size, 0xd0); }
void  stg_kfree(void *p) { kfree(p); }
void  stg_msleep(unsigned int ms) { msleep(ms); }
unsigned int stg_get_cpu_khz(void) { return cpu_khz; }
void init_cpp_support(void) { }
void cleanup_cpp_support(void) { }
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
		printk("<6>OmapNKS4:%s: create_thread() failed. err %ld\n", name, pid);
		*running = 0;
		return -1;
	}
	wait_for_completion(completion);
	if (*running == 0) {
		printk("<6>OmapNKS4:%s thread failed in some way\n", name);
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

/* Completion objects for the two service threads' startup/exit sync - same 16-byte
 * struct-completion storage convention as sProbeComplete. NOT independently
 * ground-truthed per-object (unlike sProbeComplete's 10000-jiffy timeout, confirmed
 * via OmapNKS4Init's own disassembly) - best-effort: each thread complete()s its own
 * object once at startup (matching decompiled ProcessMsgRoutine/ShutdownSSDRoutine),
 * and OmapNKS4Exit waits on the SAME object after signalling shutdown. This means
 * the exit-time wait could in principle observe the startup completion rather than
 * a genuine exit signal if both race - a known simplification, not a fully
 * ground-truthed thread-join protocol. See docs/gaps.md. */
static unsigned char sMsgThreadComplete[0x10];
static unsigned char sSsdThreadComplete[0x10];

int ProcessMsgRoutine(void *arg);
int ShutdownSSDRoutine(void *arg);

/* ===================================================================== */

static int __init OmapNKS4Init(void)
{
	init_cpp_support();		/* run C++ global constructors */
	printk("<6>OmapNKS4: OmapNKS4Init: enter\n");
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
	init_omap_nks4_usb_driver();

	/* Ground truth (fresh disassembly, 2026-07-15, OmapNKS4Init@0x18d06):
	 * label=0x4e4b5334 ("NKS4" packed as a 4-char tag, matching the same
	 * ownerTag convention OA's own daemon_lifecycle.cpp uses, e.g. "OACD..."),
	 * handler=COmapNKS4Driver_HandleOutputSysReq (confirmed: real address
	 * 0x14a80 matches that function exactly), rt_handler=NULL. */
	sSTG2NKS4SrqNumber = rt_request_srq(0x4e4b5334u, COmapNKS4Driver_HandleOutputSysReq, 0);
	if (sSTG2NKS4SrqNumber < 1) {
		printk("<6>OmapNKS4: could not get srq!\n");
		goto fail;
	}
	if (stg_usb_register_driver((struct usb_driver *)sOmapNKS4UsbDriver, &__this_module,
	                             "OmapNKS4") != 0) {
		printk("<6>OmapNKS4: Cannot register nks4 usb driver!\n");
		goto cleanup;
	}

	/* probe() runs from the USB core and completes this; wait for it. Ground truth
	 * (fresh disassembly): timeout = 0x2710 = 10000 (jiffies). */
	wait_for_completion_timeout(sProbeComplete, 10000);
	printk("<6>OmapNKS4: Waited for OmapNKS4Probe(). driver state is %d\n", sDriverState);
	if (sDriverState != 1) {
		printk("<6>OmapNKS4: probe failed\n");
		goto cleanup;
	}

	if (OmapNKS4ProcInitialize() != 0) { goto cleanup; }

	/* TEMP EXPERIMENT (2026-07-16 debug session) - stock's own extra setup
	 * code between USB probe success and its first CommunicationCheck call
	 * takes some nonzero wall-clock time to execute; this leaner
	 * reconstruction may reach the same point faster. If the panel's own
	 * OMAP firmware needs some settle time after USB enumeration before
	 * it'll answer protocol commands, stock could be satisfying that
	 * incidentally just by being slower to get there, while we race past it.
	 * Testing this directly - not grounded in any specific disassembly
	 * evidence, this is a timing hypothesis static analysis can't evaluate.
	 * Remove if this doesn't change the result. */
	printk("<6>OmapNKS4: DIAG sleeping 1000ms before CommunicationCheck (timing experiment)\n");
	msleep(1000);

	/* TEMP diagnostic (2026-07-16 debug session) - dump the interrupt URB's
	 * configured fields right before submission - remove once the
	 * comm-check timeout is root-caused. */
	printk("<6>OmapNKS4: DIAG sInterruptURB=%p pipe=0x%x len=%u interval=%u flags=0x%x\n",
	       sInterruptURB,
	       *(unsigned int *)((char *)sInterruptURB + 0x30),
	       *(unsigned int *)((char *)sInterruptURB + 0x50),
	       *(unsigned int *)((char *)sInterruptURB + 0x68),
	       *(unsigned int *)((char *)sInterruptURB + 0x3c));
	/* mem_flags=0xd0 (GFP_KERNEL) - ground truth: real OmapNKS4Init@0x18e77
	 * disassembly - this is the interrupt URB's initial submit, running in
	 * process context (init_module's own calling context), unlike the
	 * atomic-context resubmit inside InterruptCallback or the command-write
	 * submit paths (both GFP_ATOMIC=0x20 in the real binary). All of these
	 * were previously hardcoded 0 - not blocking submission (confirmed:
	 * rc==0 either way in live testing), but a genuine deviation from
	 * ground truth, confirmed 2026-07-16. */
	if (stg_usb_submit_urb(sInterruptURB, 0xd0) != 0) {	/* start the interrupt-IN xfer */
		printk("<6>OmapNKS4: error submitting interrupt xfer\n");
		goto cleanup;
	}
	printk("<6>OmapNKS4: DIAG interrupt submit rc=0\n");
	if (COmapNKS4Driver_Configure() != 0) {
		printk("<6>OmapNKS4: Problem configuring OmapNKS4 in Init\n");
		goto cleanup;
	}

	create_thread("kOmapNKS4MsgRoutine", ProcessMsgRoutine, &sProcessMsgThreadRunning);
	create_thread("kShutdownSSDRoutine", ShutdownSSDRoutine, &sShutdownSSDThreadRunning);
	CActiveSenseThread::Setup();
	return 0;

cleanup:
	CleanupOmapNKS4Driver();
fail:
	cleanup_cpp_support();
	return -1;
}

static void __exit OmapNKS4Exit(void)
{
	CActiveSenseThread::Cleanup();

	/* Ground truth shows a real __wake_up(0) between setting the running flag to 0
	 * and the join-wait, presumably to kick each thread out of its own
	 * wait_event()/wait_event_timeout() sleep so it re-checks the flag promptly.
	 * wait_event/wait_event_timeout are modeled here as plain polling loops (same
	 * simplification this file's own WaitForFreeBulkWriteURB already uses for an
	 * equivalent real-wait-queue case, see usb.cpp) rather than real wait-queue
	 * blocking, so there is no real wait queue object here to wake - the poll loop
	 * notices the flag change on its own next iteration regardless. __wake_up
	 * omitted rather than passed a wrong/unrelated object. */
	sProcessMsgThreadRunning = 0;
	wait_for_completion_timeout(sMsgThreadComplete, 10000);	/* join the msg thread */

	sShutdownSSDThreadRunning = 0;
	wait_for_completion_timeout(sSsdThreadComplete, 10000);	/* join the ssd thread */

	CleanupOmapNKS4Driver();
	cleanup_cpp_support();
}

/* ===================================================================== */

/* Video message-processor: wake on SignalVideoMessageProcessor(), drain the ring.
 * wait_event(sVideoMsgSignalled) modeled as a plain poll loop (stg_msleep), same
 * simplification already established in this file for WaitForFreeBulkWriteURB - see
 * OmapNKS4Exit's own comment on why. */
int ProcessMsgRoutine(void *arg)
{
	daemonize("kOmapNKS4MsgRoutine");
	stg_sched_setscheduler(stg_get_current_task_nks4(), 2 /* SCHED_RR */, 0);
	block_all_signals();
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

	while (sProcessMsgThreadRunning) {
		while (!sVideoMsgSignalled && sProcessMsgThreadRunning)
			stg_msleep(20);
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
	stg_sched_setscheduler(stg_get_current_task_nks4(), 2 /* SCHED_RR */, 0);
	block_all_signals();
	/* see ProcessMsgRoutine's own comment above - same fix, same reasoning. */
	complete(arg);

	for (;;) {
		/* wait_event_timeout(sShutdownSSDSignaled, 10000) - same poll-loop
		 * simplification as ProcessMsgRoutine above; 10000 is a best-effort
		 * placeholder (not independently ground-truthed for this specific call
		 * site the way OmapNKS4Init's probe-wait timeout was). */
		for (int waited = 0; !sShutdownSSDSignaled && waited < 10000; waited += 20)
			stg_msleep(20);
		if (!sShutdownSSDThreadRunning) {
			sShutdownSSDThreadRunning = 0;
			complete_and_exit(sSsdThreadComplete, 0);
			return 0;
		}
		if (sIsSSDReadyToShutdown || !sShutdownSSDSignaled)
			continue;

		/* flush + stop every SCSI device on every host (the internal SSD).
		 * scsi_device_set_state's real Linux 2.6.32 enum values: SDEV_QUIESCE=6,
		 * SDEV_OFFLINE=1 - not independently ground-truthed against this specific
		 * binary (this whole SSD-shutdown path is low priority relative to the
		 * NKS4 panel protocol itself - out of this session's main focus). A real
		 * per-thread lock object is needed for mutex_lock/unlock; declared here
		 * as a best-effort placeholder-sized (32 byte) buffer, not ground-truthed. */
		static unsigned char sScsiShutdownLock[32];
		for (int host = 0; ; host++) {
			void *shost = scsi_host_lookup(host);
			if (!shost)
				break;
			void *sdev = scsi_device_lookup(shost);
			if (sdev) {
				mutex_lock(sScsiShutdownLock);
				scsi_device_set_state(sdev, 6 /* SDEV_QUIESCE */);
				/* Ground truth (fresh disassembly, 2026-07-16): NOT plain
				 * external dev_shutdown()/bus_shutdown() calls - those
				 * genuinely don't appear anywhere in the stock module's
				 * import list. The real code walks raw struct offsets off
				 * `sdev` and calls a function pointer it finds there
				 * (matching the kernel's device_driver->shutdown(dev)
				 * callback convention), same "raw offset into an opaque
				 * kernel struct" technique already used throughout this
				 * codebase (e.g. struct usb_driver in this same file).
				 * Lower confidence than the panel-protocol fixes elsewhere
				 * this session - this path only fires on a real
				 * power-button event, not during normal driver bring-up,
				 * so it wasn't prioritized for deeper verification. */
				{
					void *dev = ((void **)sdev)[0x3e];
					if (dev) {
						void (*shutdown_fn)(void *) =
							*(void (**)(void *))((char *)dev + 0x1c);
						if (shutdown_fn) shutdown_fn(dev);
					}
				}
				scsi_device_set_state(sdev, 1 /* SDEV_OFFLINE */);
				{
					void *bus = *(void **)(*(void **)sdev);
					bus = *(void **)((char *)bus + 0x60);
					void (*shutdown_fn)(void *) =
						*(void (**)(void *))((char *)bus + 0x3c);
					if (shutdown_fn) shutdown_fn(bus);
				}
				mutex_unlock(sScsiShutdownLock);
				scsi_device_put(sdev);
			}
			scsi_host_put(shost);
		}

		msleep(500);
		sShutdownSSDSignaled = 0;
		/* COmapNKS4Driver_ShutDown's real parameter meaning is unconfirmed - see its
		 * definition in driver.cpp. 0 is a placeholder pending a real trace of this
		 * call site's ground truth. */
		COmapNKS4Driver_ShutDown(0);
		sIsSSDReadyToShutdown = 1;
	}
}

module_init(OmapNKS4Init);
module_exit(OmapNKS4Exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Korg Kronos OMAP NKS4 front-panel USB driver");
MODULE_AUTHOR("Korg (reconstructed)");
