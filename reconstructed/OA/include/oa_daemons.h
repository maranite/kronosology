// SPDX-License-Identifier: GPL-2.0
/*
 * oa_daemons.h  -  the STG background-daemon watchdog control blocks and
 * the RTAI-side "kick a timed-out daemon's Linux servant" routine
 * (batch 35, sec 10.183), EXTENDED (batch 40) with the full daemon
 * lifecycle: SetupDaemon/SetupDecryptDaemon (the shared per-daemon
 * kernel_thread-spawning helper, ground truth `SetupDaemon.clone.0`/
 * `SetupDecryptDaemon.clone.0`) plus setup_stg_daemons/cleanup_stg_daemons/
 * setup_stg_decrypt_daemons/cleanup_stg_decrypt_daemons (src/init/
 * daemon_lifecycle.cpp).
 *
 * signal_timed_out_daemons() (ground truth, 435 bytes) is a periodic
 * watchdog called from the engine tick (CSTGEngine, engine.cpp:121). It
 * scans a fixed array of 7 daemon control blocks; for each, if more than
 * `timeout` ticks have elapsed since the block was last serviced, it
 * re-stamps `lastTick` to "now" and pends the block's RTAI SRQ
 * (rt_pend_linux_srq) to wake that daemon's Linux-side servant thread.
 *
 * CORRECTED/EXTENDED layout (batch 40): sec 10.183 only knew fields at
 * offsets +0x00/+0x04/+0x0c (relative to what it called the array base)
 * because it only had signal_timed_out_daemons' own reads to go on. This
 * batch fully disassembled `SetupDaemon.clone.0` (ground truth
 * `.text+0x11ce30`, 296 bytes -- the shared helper `setup_stg_daemons`
 * calls once per daemon to populate a control block AND spawn its Linux
 * kernel thread) and found the TRUE per-entry struct starts 0x18 bytes
 * BEFORE where sec 10.183 pointed `gStgDaemons` -- i.e. lastTick/timeout/
 * srq are NOT at the struct's own +0x00/+0x04/+0x0c, they are at
 * +0x18/+0x1c/+0x24 of the FULL 0x60-byte struct. Confirmed independently
 * two ways: (a) `setup_stg_daemons`' own real base pointer (0x1077c0) is
 * exactly gStgDaemons' base (0x1077d8) minus 0x18; (b) SetupDaemon.clone.0
 * zeroes this[+0x18]=0 (matches lastTick's initial-zero state) and writes
 * this[+0x1c]=0x32 (a watchdog timeout constant -- NOT a "priority" as an
 * earlier working guess had it) and this[+0x24]=rt_request_srq(...)'s
 * return value (matches `srq` exactly). Sec 10.183's own field NAMES and
 * their RELATIVE spacing were already correct; only their absolute
 * position within the full struct was off by a constant -0x18 (because
 * that pass never saw the leading fields SetupDaemon itself populates).
 * Adding the new leading fields does NOT change lastTick/timeout/srq's
 * offsets as seen through the struct's own field names (compiler-computed,
 * transparent to every existing caller/test), so this extension is
 * backward compatible with every existing gStgDaemons[i].lastTick/
 * .timeout/.srq reference (signal_timed_out_daemons, test_stg_daemons.cpp).
 *
 * Full 0x60-byte STGDaemonWatch layout (general 7-daemon cluster --
 * "OAStreamingReader"/"OAFileOpener"/"OAFileReader"/"OAFileWriter"/
 * "OAFileCloser"/"OASampling"/"OACDAudio"):
 *     +0x00  name          packed char* (daemon name string, ground
 *                           truth .rodata.str1.1)
 *     +0x04  (never read/written by SetupDaemon/cleanup/watchdog)
 *     +0x08  p4             int   small caller-supplied constant (2 or 1
 *                            across the 7 real call sites); meaning not
 *                            recovered
 *     +0x0c  p5             int   ditto (3 or 0); meaning not recovered
 *     +0x10  running         int   0 initially; 1 once kernel_thread()
 *                            launches; cleared by cleanup_stg_daemons
 *     +0x14  jiffiesTimeout u32   msecs_to_jiffies(p6) result (p6 is 2 or
 *                            4 across the real call sites)
 *     +0x18  lastTick       u32   <-- EXISTING (sec 10.183), unchanged
 *     +0x1c  timeout        u32   <-- EXISTING (sec 10.183), unchanged --
 *                            SetupDaemon always writes the literal
 *                            constant 0x32 here
 *     +0x20  field_20       u32   (unknown, zeroed by SetupDaemon; was
 *                            mis-cited as "+0x08" under the old
 *                            lastTick-relative addressing -- same real
 *                            byte, renamed to its TRUE offset)
 *     +0x24  srq            u32   <-- EXISTING (sec 10.183), unchanged --
 *                            rt_request_srq(ownerTag, srqHandler, 0)'s
 *                            return value, or -1 on failure
 *     +0x28  wakeQueue[0xc]        a standalone wait_queue_head_t (12B, no
 *                            leading "done" flag) -- woken by
 *                            cleanup_stg_daemons (__wake_up) to signal the
 *                            servant thread to exit
 *     +0x34  completion1[0x10]     `struct completion` (done u32 @+0x34,
 *                            wait_queue_head_t @+0x38, 12B) -- waited on
 *                            by SetupDaemon (wait_for_completion) as the
 *                            servant's "I'm alive" startup signal
 *     +0x44  completion2[0x10]     another `struct completion` (done @
 *                            +0x44, wait @+0x48) -- waited on with a
 *                            2000-jiffy timeout by cleanup_stg_daemons
 *                            (wait_for_completion_timeout) as the
 *                            servant's "I've exited" shutdown signal
 *     +0x54  status         int   pid on success; -1 (kernel_thread
 *                            failed) or -2 (completion-not-ready failure)
 *                            otherwise -- write-only, never read by any
 *                            function reconstructed in this cluster
 *     +0x58  mainRoutine    packed fn ptr -- the daemon-specific work
 *                            routine (e.g. StreamingReadMainRoutine);
 *                            stored only, never dispatched by any
 *                            function reconstructed this batch (the
 *                            shared kernel-thread trampoline that WOULD
 *                            call it, ground truth `.text+0x11ccc0`, is
 *                            out of scope -- modeled as an opaque address
 *                            constant, see daemon_lifecycle.cpp)
 *     +0x5c  srqHandler     packed fn ptr -- also passed directly as
 *                            rt_request_srq's `handler` argument
 *
 * The array (gStgDaemons) is populated by setup_stg_daemons (batch 40,
 * src/init/daemon_lifecycle.cpp) via SetupDaemon.
 */

#ifndef OA_DAEMONS_H
#define OA_DAEMONS_H

#define STG_DAEMON_COUNT 7
#define STG_DECRYPT_DAEMON_COUNT 4

struct STGDaemonWatch {
	unsigned int  name;			/* +0x00 */
	unsigned int  _unk04;			/* +0x04 */
	int           p4;			/* +0x08 */
	int           p5;			/* +0x0c */
	int           running;			/* +0x10 */
	unsigned int  jiffiesTimeout;		/* +0x14 */
	unsigned int  lastTick;			/* +0x18 */
	unsigned int  timeout;			/* +0x1c */
	unsigned int  field_20;			/* +0x20 (unknown) */
	unsigned int  srq;			/* +0x24 */
	unsigned char wakeQueue[0xc];		/* +0x28..+0x33 */
	unsigned char completion1[0x10];	/* +0x34..+0x43 */
	unsigned char completion2[0x10];	/* +0x44..+0x53 */
	int           status;			/* +0x54 */
	unsigned int  mainRoutine;		/* +0x58 */
	unsigned int  srqHandler;		/* +0x5c */
};

/* The .bss daemon-watchdog array (base .bss+0x1077c0 in ground truth --
 * i.e. gStgDaemons[i].lastTick lands at the ground-truth address sec
 * 10.183 originally called the array's own base, 0x1077d8; see the file
 * header above). */
extern STGDaemonWatch gStgDaemons[STG_DAEMON_COUNT];

/*
 * Decrypt-daemon cluster (4 entries: "Decrypt0".."Decrypt3"), 0x94-byte
 * stride, ground truth `SetupDecryptDaemon.clone.0` (`.text+0x11c970`,
 * 292 bytes). Structurally parallel to STGDaemonWatch but: no SRQ
 * registration at all (decrypt daemons are plain kernel threads, not
 * RTAI-SRQ-driven servants) and FOUR `struct completion`s instead of one
 * standalone queue + two completions.
 *
 *     +0x00  name         packed char* ("Decrypt0".."Decrypt3")
 *     +0x04  (never referenced)
 *     +0x08  typeTag       int  constant 2 (meaning not recovered)
 *     +0x0c  index         int  daemon ordinal 0..3
 *     +0x10  running       int  0 initially; 1 once kernel_thread()
 *                          launches; cleared by cleanup_stg_decrypt_daemons
 *     +0x14  jiffiesTimeout u32 msecs_to_jiffies(timeoutMs) (timeoutMs is
 *                          2 for Decrypt0-2, 4 for Decrypt3)
 *     +0x18  (zeroed by SetupDecryptDaemon)
 *     +0x1c  waitTimeout   u32  constant 0x32 (meaning not recovered,
 *                          mirrors the general cluster's own +0x1c)
 *     +0x20  (zeroed by SetupDecryptDaemon)
 *     +0x24  reserved24    int  pre-set to -1 by setup_stg_decrypt_daemons
 *                          ITSELF (SetupDecryptDaemon never touches it) --
 *                          an apparent srq-shaped sentinel that's never
 *                          populated with a working value anywhere in
 *                          this cluster; meaning not recovered
 *     +0x28  completion0[0x10]  woken by cleanup (shutdown signal)
 *     +0x38  completion1[0x10]  waited on by SetupDecryptDaemon (startup
 *                          "I'm alive" signal)
 *     +0x48  completion2[0x10]  waited-with-2000-jiffy-timeout by cleanup
 *                          (shutdown "I've exited" signal)
 *     +0x58  completion3[0x10]  never referenced by any function this
 *                          batch reconstructs (role unrecovered --
 *                          presumably used by the not-yet-reconstructed
 *                          signal_decrypt_daemon/wait_process_completion/
 *                          signal_timed_out_decrypt_daemons siblings,
 *                          nm-confirmed real `T` symbols at
 *                          .text+0x11cbd0/+0x11cc80/+0x11cc70)
 *     +0x68  status        int  pid on success; -1 (kernel_thread failed)
 *                          or -2 (completion-not-ready failure) otherwise
 *     +0x6c  mainRoutine   packed fn ptr (Decrypt0MainRoutine..
 *                          Decrypt3MainRoutine); stored only, never
 *                          dispatched here (the decrypt-specific kernel-
 *                          thread trampoline, ground truth
 *                          `.text+0x11c820`, is out of scope, modeled as
 *                          an opaque address constant)
 *     +0x70..+0x93  unknown trailing padding (preserves the 0x94 stride)
 */
struct STGDecryptDaemonWatch {
	unsigned int  name;			/* +0x00 */
	unsigned int  _unk04;			/* +0x04 */
	int           typeTag;			/* +0x08 */
	int           index;			/* +0x0c */
	int           running;			/* +0x10 */
	unsigned int  jiffiesTimeout;		/* +0x14 */
	unsigned int  _unk18;			/* +0x18 */
	unsigned int  waitTimeout;		/* +0x1c */
	unsigned int  _unk20;			/* +0x20 */
	int           reserved24;		/* +0x24 */
	unsigned char completion0[0x10];	/* +0x28..+0x37 */
	unsigned char completion1[0x10];	/* +0x38..+0x47 */
	unsigned char completion2[0x10];	/* +0x48..+0x57 */
	unsigned char completion3[0x10];	/* +0x58..+0x67 */
	int           status;			/* +0x68 */
	unsigned int  mainRoutine;		/* +0x6c */
	unsigned char _tail[0x94 - 0x70];	/* +0x70..+0x93 */
};

extern STGDecryptDaemonWatch gStgDecryptDaemons[STG_DECRYPT_DAEMON_COUNT];

/* RTAI: pend the Linux-side service request identified by `srq`, waking a
 * servant thread from hard-RT context. External (`U`) in the real OA.ko. */
extern "C" void rt_pend_linux_srq(unsigned int srq);

/* The watchdog itself. */
extern "C" void signal_timed_out_daemons(void);

/*
 * signal_daemon(daemonIndex) (batch 51, ground truth `signal_daemon`,
 * `.text+0x11d3c0`, 50 bytes) -- the single-daemon counterpart to
 * signal_timed_out_daemons()'s own full-table sweep: re-stamps ONE
 * gStgDaemons[] entry's lastTick to "now" and pends its RTAI SRQ,
 * on demand rather than only when the periodic watchdog notices a
 * timeout. Confirmed real body (register-level, `objdump -dr`):
 *   gStgDaemons[daemonIndex].lastTick = GetSTGTickCount();
 *   rt_pend_linux_srq(gStgDaemons[daemonIndex].srq);
 * (the `daemonIndex*0x60` stride -- `lea edx,[ebx+ebx*2]; shl edx,5`,
 * i.e. `ebx*3*32` -- is byte-identical to STGDaemonWatch's own
 * confirmed size, and the two field offsets used, ground-truth
 * `0x1077d8`/`0x1077e4`, are exactly `gStgDaemons` base + 0x18/+0x24 --
 * lastTick/srq, matching this header's own already-established layout
 * exactly). First confirmed real caller: `CSTGHDRManager::
 * ProcessPlaybackCommands()` (batch 51, see hdr_playback_commands.cpp),
 * which always passes literal 0 (daemon index 0, "OAStreamingReader").
 * No bounds check on `daemonIndex` -- faithfully unchecked, matching
 * ground truth exactly (an out-of-range index would read/write past
 * `gStgDaemons[STG_DAEMON_COUNT-1]`, same as the real target).
 */
extern "C" void signal_daemon(unsigned int daemonIndex);

/* Daemon lifecycle (batch 40, src/init/daemon_lifecycle.cpp). Signatures
 * match ground truth's own init_module()/oa_init.h callers exactly. */
extern "C" int  setup_stg_daemons(void);
extern "C" void cleanup_stg_daemons(void);
extern "C" int  setup_stg_decrypt_daemons(void);
extern "C" void cleanup_stg_decrypt_daemons(void);

#endif /* OA_DAEMONS_H */
