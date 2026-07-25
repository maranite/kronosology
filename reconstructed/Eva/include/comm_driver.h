/*
 * comm_driver.h  -  CCommDriver singleton accessor (Stage 1 boot path).
 *
 * Two real overloads exist in the binary with genuinely different behavior -- not a
 * const/non-const pair, a real construct-vs-assert split:
 *   getInstance(char **argv)  -- real boot-path call (main()). Constructs the
 *                                singleton the first time, returns the cached one
 *                                after. This is the only place the singleton is
 *                                ever actually created.
 *   getInstance()             -- assert-style accessor used elsewhere in the binary
 *                                (not on main()'s own call path). If the singleton
 *                                hasn't been constructed yet, prints
 *                                "CCommDriver init error\n" to stderr and calls
 *                                exit(1) -- a real, unconditional process-abort,
 *                                faithfully preserved rather than softened. Any
 *                                caller of this overload implicitly assumes
 *                                getInstance(argv) already ran.
 *
 * setupfifoname(argv) -- upgraded from Tier-B stub to Tier A (Stage 6 breadth
 * sweep, 2026-07-25; .text+0x08e4f310, 681 bytes). Real per-argv-entry parser:
 * splits each "NAME=VALUE"-shaped argv string on '=' and, for exactly 3 real
 * names (NKS4_LCDFIFO / NKS4_EVENTSFIFO / NKS4_COMMANDSFIFO), conditionally
 * (gated on Eva_IsSimulation()/Eva_IsSimulationSVGA(), see app_mode.h) strdup's
 * the value into mLcdFifoPath/mEventFifoPath/mCommandFifoPath. Falls back to 3
 * real hardcoded default paths ("/tmp/evaclientfifo"/"/tmp/evaeventfifo"/
 * "/tmp/evacommandfifo") for any field still null after the argv scan, again
 * gated the same way per-field (the real per-class comment in comm_driver.cpp
 * has the exact gating asymmetry -- LCD/COMMAND check only Eva_IsSimulation(),
 * EVENT checks either simulation flag).
 *
 * REAL BUG, CONFIRMED AT THE RAW-DISASSEMBLY LEVEL (not a decompiler artifact --
 * `8e4f3b6: call strchr@plt` / `8e4f3bb: movb $0x0,(%eax)` with no intervening
 * test/je): every argv entry is unconditionally strchr()'d for '=' and the
 * result is dereferenced with NO NULL CHECK. Any argv entry that lacks '='
 * (e.g. argv[0], the program's own path/name) segfaults inside this function,
 * before CCommDriver does anything else -- i.e. before Eva has opened a single
 * fifo. Since real hardware demonstrably runs Eva successfully, real production
 * argv must contain at least one "NAME=VALUE"-shaped entry; this project could
 * not locate the real launch wrapper (inside the encrypted Eva.img, not present
 * in any extracted rootfs on this share) to confirm what it actually passes.
 * Flagged here, not "fixed" -- adding a NULL check the real binary doesn't have
 * would misrepresent the real function's own (crash-prone) contract. Any live
 * kronos_vm boot test of this reconstruction MUST invoke Eva with at least one
 * argv entry containing '=' (a real, not-fabricated input shape) to avoid
 * tripping this real bug -- see README.md's Stage 6 writeup.
 */

#ifndef COMM_DRIVER_H
#define COMM_DRIVER_H

class CCommDriver {
public:
	/* .text+0x08e4f5d0, 242 bytes -- reconstructed (see comm_driver.cpp). Opens 3
	 * fifo paths setupfifoname() fills in (LCD/Command/Event); any that stays null
	 * is silently skipped (real behavior, not a bug).
	 */
	CCommDriver(char **argv);

	static CCommDriver *getInstance(char **argv);
	static CCommDriver *getInstance();

private:
	static CCommDriver *singleton;

	/* .text+0x08e4f310, 681 bytes -- real, Tier A. See this header's own top
	 * comment for the full behavior writeup and the real no-NULL-check bug.
	 */
	void setupfifoname(char **argv);

	char *mLcdFifoPath;    /* +0x00 */
	char *mEventFifoPath;  /* +0x04 */
	char *mCommandFifoPath; /* +0x08 */
	int   mLcdFd;           /* +0x0c */
	int   mEventFd;          /* +0x10 */
	int   mCommandFd;        /* +0x14 */

	/* Friend accessor for verify/test_comm_driver.cpp -- same extraction
	 * pattern already used by ustg_user_api.h/level_manager_array.h.
	 */
	friend struct CommDriverTestHooks;
};

#endif /* COMM_DRIVER_H */
