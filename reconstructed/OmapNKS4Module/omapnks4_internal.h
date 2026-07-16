// SPDX-License-Identifier: GPL-2.0
/*
 * omapnks4_internal.h  -  module-private declarations: the STG/RTAI framework
 * layer, kernel imports, singletons and the cross-file helpers.
 *
 * Symbols prefixed stg_ / rtwrap_ are the shared "STG" real-time abstraction layer
 * (a thin veneer over RTAI + the Linux kernel) that every Korg STG module links.
 * They are imported here, not redefined.  CSTGThread likewise comes from that layer.
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
 * rtai_*.ko export names), confirmed 2026-07-16. */
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
void COmapNKS4Driver_NotifyTransmittedCommandComplete(void);
void COmapNKS4Driver_Cleanup(void);
void COmapNKS4VideoAPI_Initialize(void);
void COmapNKS4Driver_Initialize(unsigned int maxWritePacketInts);
/* usb.cpp - needed by main.cpp to construct this module's struct usb_driver
 * (probe/disconnect fields) - see main.cpp's own comment for the full derivation. */
int  OmapNKS4Probe(struct usb_interface *intf);
void OmapNKS4Disconnect(void);
void CleanupOmapNKS4Driver(void);
int  OmapNKS4ProcInitialize(void);
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
/* proc_dir_entry's read_proc/write_proc field offsets (+0x3c/+0x40) - computed from
 * linux-kronos's real struct proc_dir_entry layout (low_ino/namelen/name/mode/
 * nlink/uid/gid/size/proc_iops/proc_fops/next/parent/subdir/data, each field's real
 * typedef size confirmed via linux/types.h - mode_t/nlink_t/uid_t/gid_t are all 4
 * bytes on this kernel), not independently confirmed via disassembly the way the
 * struct usb_driver fields were - standard x86-32 struct layout math, not a blind
 * guess, but flagged as lower-confidence than the disassembly-verified fixes
 * elsewhere this session. */
void *make_proc_entry(const char *name);
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
struct CSTGThread {
	int CreateRealTimeWithCPUAffinity(stg_thread_fn fn, void *arg,
					  int priority, int stack, void *cpumask);
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
