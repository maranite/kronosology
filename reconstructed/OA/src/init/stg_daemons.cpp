// SPDX-License-Identifier: GPL-2.0
/*
 * stg_daemons.cpp  -  signal_timed_out_daemons(), the STG background-daemon
 * watchdog (batch 35, sec 10.183). See include/oa_daemons.h for the
 * control-block layout and subsystem context.
 *
 * Ground truth (`objdump -dr` of /home/share/Decomp/OA.ko_Decomp/OA.ko,
 * symbol `signal_timed_out_daemons`, 435 bytes). The compiler unrolled the
 * 7-entry scan and hoisted each block's "kick" body out of line, but the
 * semantics are a plain loop:
 *
 *     now = GetSTGTickCount();                         // ebx, read once
 *     for (i = 0; i < 7; i++)                          // fully unrolled
 *         if ((u32)(now - d[i].lastTick) > d[i].timeout) {   // `ja`, strict >
 *             d[i].lastTick = GetSTGTickCount();       // FRESH tick, not `now`
 *             rt_pend_linux_srq(d[i].srq);
 *         }
 *
 * Confirmed quirks preserved:
 *   - the elapsed compare is an UNSIGNED strict `>` (`ja` in the scan,
 *     `jbe` on the out-of-line re-entry) -- so a daemon whose elapsed time
 *     exactly equals its timeout does NOT fire;
 *   - the elapsed value is a wrapping 32-bit subtraction (`now - lastTick`
 *     as u32), tolerant of the tick counter wrapping;
 *   - `now` (the loop reference) is read ONCE up front, but each kick
 *     re-reads the tick via a SECOND GetSTGTickCount() call to stamp
 *     lastTick -- the reset value can therefore differ slightly from `now`.
 *     Reproduced exactly (two distinct calls), not folded into one.
 */

#include "oa_daemons.h"
#include "oa_global.h"		/* GetSTGTickCount */

/* The .bss watchdog array. Zero-initialized, exactly like the real global
 * before setup_stg_daemons (still stubbed) populates timeout/srq. */
STGDaemonWatch gStgDaemons[STG_DAEMON_COUNT];

/* Layout guard: the 0x60 stride is load-bearing (shared with the daemon
 * setup/cleanup family). */
static_assert(sizeof(STGDaemonWatch) == 0x60,
	      "STGDaemonWatch must match the ground-truth 0x60 stride");

extern "C" void signal_timed_out_daemons(void)
{
	unsigned int now = GetSTGTickCount();

	for (int i = 0; i < STG_DAEMON_COUNT; i++) {
		STGDaemonWatch *d = &gStgDaemons[i];
		if ((unsigned int)(now - d->lastTick) > d->timeout) {
			d->lastTick = GetSTGTickCount();
			rt_pend_linux_srq(d->srq);
		}
	}
}

/*
 * signal_daemon(daemonIndex) (batch 51, `.text+0x11d3c0`, 50 bytes) --
 * see oa_daemons.h for the full derivation. Unconditional single-entry
 * kick, no timeout check, no bounds check -- unlike the watchdog sweep
 * above, this always fires.
 */
extern "C" void signal_daemon(unsigned int daemonIndex)
{
	STGDaemonWatch *d = &gStgDaemons[daemonIndex];
	d->lastTick = GetSTGTickCount();
	rt_pend_linux_srq(d->srq);
}
