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
 * CCommDriver's own constructor body (what it actually does with argv) is Stage 2 --
 * not yet reconstructed, declared here only as far as getInstance() needs.
 */

#ifndef COMM_DRIVER_H
#define COMM_DRIVER_H

class CCommDriver {
public:
	/* .text+0x08e4f5d0, 242 bytes -- reconstructed (see comm_driver.cpp). Opens 3
	 * fifo paths setupfifoname() fills in (LCD/Command/Event); any that stays null
	 * is silently skipped (real behavior, not a bug -- setupfifoname() itself is a
	 * Tier-B link-stub in this pass, so all 3 stay null and the ctor becomes a
	 * real, faithfully-derived no-op given that specific input).
	 */
	CCommDriver(char **argv);

	static CCommDriver *getInstance(char **argv);
	static CCommDriver *getInstance();

private:
	static CCommDriver *singleton;

	char *mLcdFifoPath;    /* +0x00 */
	char *mEventFifoPath;  /* +0x04 */
	char *mCommandFifoPath; /* +0x08 */
	int   mLcdFd;           /* +0x0c */
	int   mEventFd;          /* +0x10 */
	int   mCommandFd;        /* +0x14 */
};

#endif /* COMM_DRIVER_H */
