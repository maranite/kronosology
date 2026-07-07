// SPDX-License-Identifier: GPL-2.0
/*
 * oa_daemons.h  -  the STG background-daemon watchdog control blocks and
 * the RTAI-side "kick a timed-out daemon's Linux servant" routine
 * (batch 35, sec 10.183).
 *
 * signal_timed_out_daemons() (ground truth, 435 bytes) is a periodic
 * watchdog called from the engine tick (CSTGEngine, engine.cpp:121). It
 * scans a fixed array of 7 daemon control blocks; for each, if more than
 * `timeout` ticks have elapsed since the block was last serviced, it
 * re-stamps `lastTick` to "now" and pends the block's RTAI SRQ
 * (rt_pend_linux_srq) to wake that daemon's Linux-side servant thread.
 *
 * Control-block layout (confirmed via the .bss offsets in
 * signal_timed_out_daemons' disassembly -- array base .bss+0x1077d8,
 * stride 0x60, 7 entries):
 *     +0x00  lastTick   u32  tick of last service (compared against now)
 *     +0x04  timeout    u32  max ticks allowed before a kick
 *     +0x08  (unknown, not touched by signal_timed_out_daemons)
 *     +0x0c  srq        u32  RTAI SRQ handle passed to rt_pend_linux_srq
 *     +0x10..+0x5f       (unknown; populated by setup_stg_daemons et al.,
 *                         still stubbed -- preserved as opaque padding so
 *                         the 0x60 stride stays exact for later batches)
 *
 * Only lastTick/timeout/srq are confirmed; the rest is opaque padding kept
 * solely to hold the stride. The array (gStgDaemons) is shared with the
 * not-yet-reconstructed setup_stg_daemons/setup_stg_decrypt_daemons/
 * cleanup_stg_daemons family, which populate timeout/srq at startup; with
 * those still stubbed the blocks are zero-initialized, so this watchdog is
 * inert-but-faithful until they are reconstructed.
 */

#ifndef OA_DAEMONS_H
#define OA_DAEMONS_H

#define STG_DAEMON_COUNT 7

struct STGDaemonWatch {
	unsigned int  lastTick;			/* +0x00 */
	unsigned int  timeout;			/* +0x04 */
	unsigned int  field_08;			/* +0x08 (unknown) */
	unsigned int  srq;			/* +0x0c */
	unsigned char rest[0x60 - 0x10];	/* +0x10..+0x5f (unknown padding) */
};

/* The .bss daemon-watchdog array (base .bss+0x1077d8 in ground truth). */
extern STGDaemonWatch gStgDaemons[STG_DAEMON_COUNT];

/* RTAI: pend the Linux-side service request identified by `srq`, waking a
 * servant thread from hard-RT context. External (`U`) in the real OA.ko. */
extern "C" void rt_pend_linux_srq(unsigned int srq);

/* The watchdog itself. */
extern "C" void signal_timed_out_daemons(void);

#endif /* OA_DAEMONS_H */
