// SPDX-License-Identifier: GPL-2.0
/*
 * daemon_lifecycle.cpp -- the STG background-daemon kernel-thread
 * lifecycle (batch 40): SetupDaemon/SetupDecryptDaemon (ground truth
 * `SetupDaemon.clone.0` @.text+0x11ce30 / `SetupDecryptDaemon.clone.0`
 * @.text+0x11c970), setup_stg_daemons/cleanup_stg_daemons (ground truth
 * .init.text, 518/666 bytes) and setup_stg_decrypt_daemons/
 * cleanup_stg_decrypt_daemons (.init.text/.text, 285/278 bytes).
 *
 * This cluster was flagged since batch 34/35 (sec 10.182/10.183) and
 * scoped in increasing detail by batch 38/39 (sec 10.189/10.190) before
 * this batch's own full derivation. Batch 39's own note that "SetupDaemon
 * does not depend on rtwrap_pthread_create, it uses plain kernel_thread()"
 * was correct, but its claim that ONE shared `SetupDaemon.clone.0` helper
 * serves BOTH setup_stg_daemons and setup_stg_decrypt_daemons was wrong --
 * ground truth actually has TWO separate (structurally parallel but
 * distinctly-shaped) helpers: `SetupDaemon.clone.0` (7 args, used by the
 * 7-entry general daemon cluster, ALSO registers an RTAI SRQ) and
 * `SetupDecryptDaemon.clone.0` (5 args, used by the 4-entry decrypt
 * daemon cluster, no SRQ at all). Confirmed via ground truth's own
 * Ghidra-analyzed decompile (/home/share/Decomp/oa_export/functions/
 * SetupDaemon.clone.0@0012ce30.c and SetupDecryptDaemon.clone.0@0012c970.c
 * -- that export's own image base is +0x10000 over this project's
 * ground-truth OA.ko, sec 10.182's own established correction) plus a
 * from-scratch `objdump -dr` re-derivation of both helpers AND both
 * `setup_stg_*`/`cleanup_stg_*` callers directly against
 * /home/share/Decomp/OA.ko_Decomp/OA.ko (needed because naive raw-byte
 * PC32-relocation arithmetic across an .init.text -> .text call is NOT
 * reliable -- .init.text and .text are laid out far apart at Ghidra's own
 * analyzed addresses, e.g. 0x005a2c1f vs 0x0012ce30, not adjacent-with-
 * zero-gap as a naive same-section-style computation would assume).
 *
 * See include/oa_daemons.h for the full per-struct field derivation.
 */

#include "oa_daemons.h"

/* No libc/cstring in this freestanding (-ffreestanding -fno-builtin)
 * target build (established project convention -- see e.g.
 * setup_global_resources.cpp's own "memset-equivalent" comments); a tiny
 * local zeroing helper stands in for memset(). */
static inline void ZeroBytes(void *p, unsigned int n)
{
	unsigned char *b = (unsigned char *)p;
	for (unsigned int i = 0; i < n; i++)
		b[i] = 0;
}

/* ---------------------------------------------------------------------
 * RTAI/kernel externs. All confirmed `U` (undefined) in ground truth
 * OA.ko -- genuine kernel/RTAI primitives resolved at insmod time, not
 * OA-internal. Every call site below passes all of its arguments
 * directly in eax/edx/ecx with no stack marshaling in the real
 * disassembly, i.e. plain regparm(3) (this file's own default), no
 * special attribute needed (unlike batch 39's rt_sem_wait/rt_task_
 * suspend/etc family, which ARE stack-passed).
 * ------------------------------------------------------------------- */
extern "C" __attribute__((regparm(0))) void rt_printk(const char *fmt, ...);
extern "C" unsigned long msecs_to_jiffies(unsigned int msecs);
extern "C" void __init_waitqueue_head(void *q, void *key);
extern "C" int  rt_request_srq(unsigned int label, void (*handler)(void), void *rt_handler);
extern "C" void rt_free_srq(unsigned int srq);
extern "C" long kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);
extern "C" void wait_for_completion(void *completion);
extern "C" long wait_for_completion_timeout(void *completion, unsigned long timeout);
extern "C" void __wake_up(void *q, unsigned int mode, int nr_exclusive, void *key);

/* ---------------------------------------------------------------------
 * Pointer <-> packed-u32 helpers (this project's established per-file
 * convention -- host is 64-bit, the real target is 32-bit).
 * ------------------------------------------------------------------- */
static inline unsigned int ToU32(const void *p)
{
	return (unsigned int)(unsigned long)p;
}
static inline void *FromU32(unsigned int v)
{
	return (void *)(unsigned long)v;
}

/* ---------------------------------------------------------------------
 * The two per-daemon-cluster .bss arrays. gStgDaemons' own STORAGE
 * already lives in src/init/stg_daemons.cpp (sec 10.183, batch 35) --
 * this file only gets it via oa_daemons.h's `extern` declaration, it
 * must NOT redefine it here (both are always linked together into the
 * same OA.ko -- confirmed by a real `ld` "multiple definition" error the
 * first time this file defined it too). gStgDecryptDaemons is new this
 * batch, its storage belongs here.
 * ------------------------------------------------------------------- */
STGDecryptDaemonWatch gStgDecryptDaemons[STG_DECRYPT_DAEMON_COUNT];

static_assert(sizeof(STGDaemonWatch) == 0x60,
	      "STGDaemonWatch must match the ground-truth 0x60 stride");
static_assert(sizeof(STGDecryptDaemonWatch) == 0x94,
	      "STGDecryptDaemonWatch must match the ground-truth 0x94 stride");

/* Dummy lock_class_key placeholders passed to __init_waitqueue_head.
 * Address-only, never dereferenced by any function reconstructed here
 * (sec 10.152's own "small opaque placeholder, real size not
 * independently determined" convention) -- ground truth uses ONE fixed
 * .bss address per cluster (0x107a60 for the general cluster, 0x1077b0
 * for the decrypt cluster) across all of that cluster's own
 * __init_waitqueue_head calls. */
static unsigned char sDaemonLockKeyDummy[4];
static unsigned char sDecryptDaemonLockKeyDummy[4];

/* The two kernel-thread entry trampolines (ground truth `.text+0x11ccc0`
 * for the general cluster, `.text+0x11c820` for the decrypt cluster) are
 * real internal (`t`) functions this batch does NOT reconstruct -- they
 * are only ever passed to kernel_thread() as an opaque function-pointer
 * VALUE by the code reconstructed here, never called. Modeled as raw
 * ground-truth address constants, exactly matching the established
 * `RTWRAP_THREAD_TRAMPOLINE` precedent (rtwrap.cpp, sec 10.190/batch 39).
 */
static int (*const DAEMON_THREAD_TRAMPOLINE)(void *) = (int (*)(void *))0x11ccc0;
static int (*const DECRYPT_DAEMON_THREAD_TRAMPOLINE)(void *) = (int (*)(void *))0x11c820;

/* The 18 daemon-specific MainRoutine/SRQHandler functions (11
 * MainRoutines + 7 SRQHandlers) are likewise real, confirmed (`T`/`t` in
 * ground truth `nm`), not-yet-reconstructed functions whose ADDRESSES
 * (not bodies) are all setup_stg_daemons/setup_stg_decrypt_daemons store
 * into the control blocks above. None of them is ever called by any
 * function reconstructed in this file (the trampolines that WOULD
 * dispatch to them are themselves out of scope, see above), so -- to
 * avoid manufacturing 18 new placeholder stub bodies just to take their
 * address -- they are modeled the same way as the two trampolines: raw
 * ground-truth address constants (`nm OA.ko`, all confirmed real `T`
 * symbols). */
static void (*const StreamingReadMainRoutine)(void) = (void (*)(void))0x118c10;
static void (*const FileOpenMainRoutine)(void)      = (void (*)(void))0x118bd0;
static void (*const FileReadMainRoutine)(void)      = (void (*)(void))0x118bf0;
static void (*const FileWriteMainRoutine)(void)     = (void (*)(void))0x118c30;
static void (*const FileCloseMainRoutine)(void)     = (void (*)(void))0x118c50;
static void (*const SamplingMainRoutine)(void)      = (void (*)(void))0x118c70;
static void (*const CDAudioMainRoutine)(void)       = (void (*)(void))0x118c90;
static void (*const Decrypt0MainRoutine)(void)      = (void (*)(void))0x118cb0;
static void (*const Decrypt1MainRoutine)(void)      = (void (*)(void))0x118cc0;
static void (*const Decrypt2MainRoutine)(void)      = (void (*)(void))0x118cd0;
static void (*const Decrypt3MainRoutine)(void)      = (void (*)(void))0x118ce0;

static void (*const StreamingReadSRQHandler)(void) = (void (*)(void))0x11cdf0;
static void (*const FileOpenSRQHandler)(void)      = (void (*)(void))0x11d0b0;
static void (*const FileReadSRQHandler)(void)      = (void (*)(void))0x11d070;
static void (*const FileWriteSRQHandler)(void)     = (void (*)(void))0x11d030;
static void (*const FileCloseSRQHandler)(void)     = (void (*)(void))0x11cff0;
static void (*const SamplingSRQHandler)(void)      = (void (*)(void))0x11cfb0;
static void (*const CDAudioSRQHandler)(void)       = (void (*)(void))0x11cf70;

/* ---------------------------------------------------------------------
 * SetupDaemon -- ground truth `SetupDaemon.clone.0`, `.text+0x11ce30`,
 * 296 bytes. Populates one general-cluster control block and spawns its
 * Linux kernel thread.
 *
 *     this[0x00]=name; this[0x08]=p4; this[0x0c]=p5; this[0x10]=0;
 *     this[0x14]=msecs_to_jiffies(timeoutMs); this[0x18]=0;
 *     this[0x1c]=0x32; this[0x20]=0;
 *     __init_waitqueue_head(this+0x28, dummyKey);          // wakeQueue
 *     this[0x34]=0; __init_waitqueue_head(this+0x38, dummyKey); // completion1.wait
 *     this[0x44]=0; __init_waitqueue_head(this+0x48, dummyKey); // completion2.wait
 *     this[0x58]=mainRoutine; this[0x5c]=srqHandler;
 *     srq = rt_request_srq(ownerTag, srqHandler, NULL);
 *     this[0x24] = srq;
 *     if (srq <= 0) return -1;                     // never spawns the thread
 *     this[0x10]=1;
 *     pid = kernel_thread(DAEMON_THREAD_TRAMPOLINE, this, 0xe00);
 *     if (pid <= 0) {
 *         rt_printk("CreateThread(%s): failed. err %ld\n", name, pid);
 *         this[0x10]=0; this[0x54]=-1; return -2;
 *     }
 *     wait_for_completion(this+0x34);               // completion1
 *     if (this[0x10] == 0) {
 *         rt_printk("CreateThread(%s) thread failed in some way, pid %ld\n", name, pid);
 *         this[0x54]=-2; return -2;
 *     }
 *     this[0x54]=pid; return 0;
 *
 * Confirmed real quirk (preserved faithfully): the two failure paths
 * store DIFFERENT sentinel values into this[0x54] (-1 vs -2) despite
 * both returning the SAME -2 overall status; this[0x54] is otherwise
 * unused by any function reconstructed in this cluster.
 * ------------------------------------------------------------------- */
static int SetupDaemon(STGDaemonWatch *d, const char *name, unsigned int ownerTag,
			int p4, int p5, unsigned int timeoutMs,
			void (*mainRoutine)(void), void (*srqHandler)(void))
{
	d->name = ToU32(name);
	d->p4 = p4;
	d->p5 = p5;
	d->running = 0;
	d->jiffiesTimeout = (unsigned int)msecs_to_jiffies(timeoutMs);
	d->lastTick = 0;
	d->timeout = 0x32;
	d->field_20 = 0;

	__init_waitqueue_head(d->wakeQueue, sDaemonLockKeyDummy);
	ZeroBytes(d->completion1, sizeof(d->completion1));
	__init_waitqueue_head(&d->completion1[4] /* wait sub-field, +0x38 */, sDaemonLockKeyDummy);
	ZeroBytes(d->completion2, sizeof(d->completion2));
	__init_waitqueue_head(&d->completion2[4] /* wait sub-field, +0x48 */, sDaemonLockKeyDummy);

	d->mainRoutine = ToU32((void *)mainRoutine);
	d->srqHandler = ToU32((void *)srqHandler);

	int srq = rt_request_srq(ownerTag, srqHandler, 0);
	d->srq = (unsigned int)srq;
	if (srq <= 0)
		return -1;

	d->running = 1;
	long pid = kernel_thread(DAEMON_THREAD_TRAMPOLINE, d, 0xe00);
	if (pid <= 0) {
		rt_printk("CreateThread(%s): failed. err %ld\n", name, pid);
		d->running = 0;
		d->status = -1;
		return -2;
	}

	wait_for_completion(d->completion1);
	if (d->running == 0) {
		rt_printk("CreateThread(%s) thread failed in some way, pid %ld\n", name, pid);
		d->status = -2;
		return -2;
	}

	d->status = (int)pid;
	return 0;
}

/* ---------------------------------------------------------------------
 * SetupDecryptDaemon -- ground truth `SetupDecryptDaemon.clone.0`,
 * `.text+0x11c970`, 292 bytes. Same shape as SetupDaemon but no SRQ
 * registration at all, and FOUR `struct completion`s instead of one
 * standalone queue + two completions. See include/oa_daemons.h for the
 * struct field derivation.
 * ------------------------------------------------------------------- */
static int SetupDecryptDaemon(STGDecryptDaemonWatch *d, const char *name, int index,
			       unsigned int timeoutMs, void (*mainRoutine)(void))
{
	d->name = ToU32(name);
	d->index = index;
	d->typeTag = 2;
	d->running = 0;
	ZeroBytes(&d->_unk18, sizeof(d->_unk18));
	ZeroBytes(&d->_unk20, sizeof(d->_unk20));

	d->mainRoutine = ToU32((void *)mainRoutine);
	d->jiffiesTimeout = (unsigned int)msecs_to_jiffies(timeoutMs);
	d->waitTimeout = 0x32;

	ZeroBytes(d->completion0, sizeof(d->completion0));
	__init_waitqueue_head(&d->completion0[4], sDecryptDaemonLockKeyDummy);
	ZeroBytes(d->completion1, sizeof(d->completion1));
	__init_waitqueue_head(&d->completion1[4], sDecryptDaemonLockKeyDummy);
	ZeroBytes(d->completion2, sizeof(d->completion2));
	__init_waitqueue_head(&d->completion2[4], sDecryptDaemonLockKeyDummy);
	ZeroBytes(d->completion3, sizeof(d->completion3));
	__init_waitqueue_head(&d->completion3[4], sDecryptDaemonLockKeyDummy);

	d->running = 1;
	long pid = kernel_thread(DECRYPT_DAEMON_THREAD_TRAMPOLINE, d, 0xe00);
	if (pid <= 0) {
		rt_printk("CreateThread(%s): failed. err %ld\n", name, pid);
		d->running = 0;
		d->status = -1;
		return -2;
	}

	wait_for_completion(d->completion1);
	if (d->running == 0) {
		rt_printk("CreateThread(%s) thread failed in some way, pid %ld\n", name, pid);
		d->status = -2;
		return -2;
	}

	d->status = (int)pid;
	return 0;
}

/* ---------------------------------------------------------------------
 * setup_stg_daemons -- ground truth `.init.text`, 518 bytes. Zero-inits
 * the 7-entry gStgDaemons array's own +0x24/srq (to -1, matching the
 * real "no srq yet" sentinel already zero from .bss init anyway -- kept
 * as an explicit store for fidelity), then calls SetupDaemon 7 times.
 * On the FIRST failure, calls cleanup_stg_daemons() and returns -1
 * (short-circuiting `&&` chain in ground truth -- later daemons are
 * never even attempted once one fails).
 * ------------------------------------------------------------------- */
extern "C" int setup_stg_daemons(void)
{
	for (int i = 0; i < STG_DAEMON_COUNT; i++) {
		gStgDaemons[i].running = 0;
		gStgDaemons[i].srq = (unsigned int)-1;
	}

	if (SetupDaemon(&gStgDaemons[2], "OAStreamingReader", 0x53745265, 2, 3, 2,
			 StreamingReadMainRoutine, StreamingReadSRQHandler) == 0 &&
	    SetupDaemon(&gStgDaemons[0], "OAFileOpener", 0x46694f70, 1, 0, 2,
			 FileOpenMainRoutine, FileOpenSRQHandler) == 0 &&
	    SetupDaemon(&gStgDaemons[1], "OAFileReader", 0x46695265, 1, 0, 2,
			 FileReadMainRoutine, FileReadSRQHandler) == 0 &&
	    SetupDaemon(&gStgDaemons[3], "OAFileWriter", 0x46695772, 1, 0, 4,
			 FileWriteMainRoutine, FileWriteSRQHandler) == 0 &&
	    SetupDaemon(&gStgDaemons[4], "OAFileCloser", 0x4669436c, 1, 0, 4,
			 FileCloseMainRoutine, FileCloseSRQHandler) == 0 &&
	    SetupDaemon(&gStgDaemons[5], "OASampling", 0x53616d70, 1, 0, 4,
			 SamplingMainRoutine, SamplingSRQHandler) == 0 &&
	    SetupDaemon(&gStgDaemons[6], "OACDAudio", 0x43444175, 1, 0, 4,
			 CDAudioMainRoutine, CDAudioSRQHandler) == 0) {
		return 0;
	}

	cleanup_stg_daemons();
	return -1;
}

/* ---------------------------------------------------------------------
 * cleanup_stg_daemons -- ground truth `.text+0x11d0f0`, 666 bytes.
 * Read back as a plain loop (sec 10.183's own "recover a compiler-
 * unrolled loop" technique): per entry, free the SRQ if one was
 * registered, then if the servant thread is running, wake it (via its
 * standalone wakeQueue) and wait (with a 2000-jiffy/0x7d0 timeout, return
 * value discarded, matching ground truth) for its exit completion.
 * ------------------------------------------------------------------- */
extern "C" void cleanup_stg_daemons(void)
{
	for (int i = 0; i < STG_DAEMON_COUNT; i++) {
		STGDaemonWatch *d = &gStgDaemons[i];

		if (d->srq != (unsigned int)-1) {
			unsigned int oldSrq = d->srq;
			d->srq = (unsigned int)-1;
			rt_free_srq(oldSrq);
		}

		if (d->running != 0) {
			d->running = 0;
			__wake_up(d->wakeQueue, 3, 1, 0);
			wait_for_completion_timeout(d->completion2, 0x7d0);
		}
	}
}

/* ---------------------------------------------------------------------
 * setup_stg_decrypt_daemons -- ground truth `.init.text+0x43a`, 285
 * bytes. Pre-sets each entry's own running(+0x10)=0 and reserved24
 * (+0x24)=-1 (explicit stores, matching ground truth exactly -- this is
 * the ONLY place reserved24 is ever written), then calls
 * SetupDecryptDaemon 4 times, short-circuiting + cleaning up on the
 * first failure exactly like setup_stg_daemons.
 * ------------------------------------------------------------------- */
extern "C" int setup_stg_decrypt_daemons(void)
{
	for (int i = 0; i < STG_DECRYPT_DAEMON_COUNT; i++) {
		gStgDecryptDaemons[i].running = 0;
		gStgDecryptDaemons[i].reserved24 = -1;
	}

	if (SetupDecryptDaemon(&gStgDecryptDaemons[0], "Decrypt0", 0, 2, Decrypt0MainRoutine) == 0 &&
	    SetupDecryptDaemon(&gStgDecryptDaemons[1], "Decrypt1", 1, 2, Decrypt1MainRoutine) == 0 &&
	    SetupDecryptDaemon(&gStgDecryptDaemons[2], "Decrypt2", 2, 2, Decrypt2MainRoutine) == 0 &&
	    SetupDecryptDaemon(&gStgDecryptDaemons[3], "Decrypt3", 3, 4, Decrypt3MainRoutine) == 0) {
		rt_printk("setup_stg_decrypt_daemons success\n");
		return 0;
	}

	rt_printk("setup_stg_decrypt_daemons failed\n");
	cleanup_stg_decrypt_daemons();
	return -1;
}

/* ---------------------------------------------------------------------
 * cleanup_stg_decrypt_daemons -- ground truth `.text+0x11caa0`, 278
 * bytes. Same shape as cleanup_stg_daemons but no SRQ (none registered)
 * and wakes completion0 (not a standalone queue) since every decrypt
 * slot is a full `struct completion`.
 * ------------------------------------------------------------------- */
extern "C" void cleanup_stg_decrypt_daemons(void)
{
	for (int i = 0; i < STG_DECRYPT_DAEMON_COUNT; i++) {
		STGDecryptDaemonWatch *d = &gStgDecryptDaemons[i];

		if (d->running != 0) {
			d->running = 0;
			__wake_up(&d->completion0[4] /* wait sub-field, +0x2c */, 3, 1, 0);
			wait_for_completion_timeout(d->completion2, 0x7d0);
		}
	}
}
