/*
 * test_comm_driver.cpp  -  host-side known-answer test for
 * CCommDriver::setupfifoname() (src/ipc/comm_driver.cpp, Stage 6 breadth sweep,
 * 2026-07-25).
 *
 * `CommDriverTestHooks` (friended in comm_driver.h) calls the real, private
 * setupfifoname() directly on raw (non-constructed) storage -- the real
 * CCommDriver ctor's own open() calls are irrelevant to this function and would
 * just spam stderr with "fifo open error" against nonexistent host paths, so
 * this test bypasses the ctor entirely rather than tolerate that noise.
 *
 * Eva_IsSimulation()/Eva_IsSimulationSVGA() are the REAL accessors (linked from
 * objs/init/app_mode.o, same as any other verify/ KAT -- see app_mode.h's own
 * header comment for why that pair lives in its own TU rather than
 * eva_main.cpp, which the Makefile's verify rule deliberately excludes from
 * every verify binary). This test drives them by writing s_eAppMode (the real
 * global both accessors read) directly, exactly like main()'s own argv[0]-
 * basename detection does.
 */

#include <cstdio>
#include <cstring>
#include "comm_driver.h"
#include "app_mode.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

struct CommDriverTestHooks {
	static void CallSetup(CCommDriver *d, char **argv) { d->setupfifoname(argv); }
	static const char *Lcd(CCommDriver *d) { return d->mLcdFifoPath; }
	static const char *Event(CCommDriver *d) { return d->mEventFifoPath; }
	static const char *Command(CCommDriver *d) { return d->mCommandFifoPath; }
};

/* Raw, non-constructed CCommDriver storage -- setupfifoname() zeroes its own
 * 3 pointer fields as its first act (matches the real disassembly), so no
 * pre-init is needed; mLcdFd/mEventFd/mCommandFd (untouched by
 * setupfifoname()) are never read by this test.
 */
static CCommDriver *rawDriver()
{
	static char storage[sizeof(CCommDriver)];
	return reinterpret_cast<CCommDriver *>(storage);
}

int main()
{
	/* NOTE: none of the argv[] arrays below include a bare program-name-style
	 * entry (no '=') -- doing so hits the real no-NULL-check bug documented in
	 * comm_driver.h's header comment and crashes setupfifoname() itself, same
	 * as it would in the real binary. Real argv[0] is passed through unchanged
	 * to this function in the real ctor; this test isolates the KEY=VALUE
	 * parsing behavior deliberately, not the crash.
	 */

	/* 1. Real hardware mode (s_eAppMode = 0): argv carries all 3 real keys, but
	 * none should be honored -- LCD/COMMAND require Eva_IsSimulation() and
	 * EVENT requires either simulation flag, all false here. REAL FINDING: the
	 * hardcoded-default fallback is gated the *same* way (every one of the 3
	 * `if (mXxxPath == 0 && Eva_IsSimulation() [|| SVGA])` guards also requires
	 * simulation mode), so in real hardware mode all 3 fields stay NULL
	 * forever, not "argv ignored, defaults used" as originally assumed while
	 * writing this test (caught by this exact check failing/segfaulting on
	 * the first run) -- CCommDriver is effectively simulator-only; on real
	 * hardware its ctor becomes a real, faithfully-derived total no-op (all 3
	 * fds stay -1), matching this project's own established finding that
	 * real-hardware IPC goes through the separate USTGUserAPI/rtf-fifo
	 * substrate instead (see README.md's Stage 1/2/4 writeups).
	 */
	{
		s_eAppMode = 0;
		char *argv[] = {
			(char *)"NKS4_LCDFIFO=/custom/lcd",
			(char *)"NKS4_EVENTSFIFO=/custom/event",
			(char *)"NKS4_COMMANDSFIFO=/custom/cmd",
			0
		};
		CCommDriver *d = rawDriver();
		CommDriverTestHooks::CallSetup(d, argv);
		check("hw mode: argv values ignored, LCD stays NULL (no defaults on real hw)",
		      CommDriverTestHooks::Lcd(d) == 0);
		check("hw mode: argv values ignored, EVENT stays NULL (no defaults on real hw)",
		      CommDriverTestHooks::Event(d) == 0);
		check("hw mode: argv values ignored, COMMAND stays NULL (no defaults on real hw)",
		      CommDriverTestHooks::Command(d) == 0);
	}

	/* 2. Simulation mode (s_eAppMode = 1): all 3 real keys present -> all 3
	 * argv values honored, no defaults used.
	 */
	{
		s_eAppMode = 1;
		char *argv[] = {
			(char *)"NKS4_LCDFIFO=/custom/lcd",
			(char *)"NKS4_EVENTSFIFO=/custom/event",
			(char *)"NKS4_COMMANDSFIFO=/custom/cmd",
			0
		};
		CCommDriver *d = rawDriver();
		CommDriverTestHooks::CallSetup(d, argv);
		check("sim mode: LCD honors argv value",
		      strcmp(CommDriverTestHooks::Lcd(d), "/custom/lcd") == 0);
		check("sim mode: EVENT honors argv value",
		      strcmp(CommDriverTestHooks::Event(d), "/custom/event") == 0);
		check("sim mode: COMMAND honors argv value",
		      strcmp(CommDriverTestHooks::Command(d), "/custom/cmd") == 0);
	}

	/* 3. SVGA simulation mode (s_eAppMode = 2): EVENT is the only field gated
	 * on Eva_IsSimulationSVGA() too (both its argv-assignment AND its default
	 * fallback check `Eva_IsSimulation() || Eva_IsSimulationSVGA()`) -- LCD/
	 * COMMAND require plain Eva_IsSimulation() (mode==1 specifically) for
	 * BOTH their argv-assignment and their default-fallback checks, so in
	 * SVGA mode they stay NULL entirely (same real finding as block 1: no
	 * defaults either, not "falls back to default" as block 1's own original
	 * mistake assumed -- fixed the same way here before this was ever run).
	 */
	{
		s_eAppMode = 2;
		char *argv[] = {
			(char *)"NKS4_LCDFIFO=/custom/lcd",
			(char *)"NKS4_EVENTSFIFO=/custom/event",
			(char *)"NKS4_COMMANDSFIFO=/custom/cmd",
			0
		};
		CCommDriver *d = rawDriver();
		CommDriverTestHooks::CallSetup(d, argv);
		check("SVGA mode: LCD does NOT honor argv (needs plain sim), stays NULL",
		      CommDriverTestHooks::Lcd(d) == 0);
		check("SVGA mode: EVENT honors argv value (SVGA also gates EVENT)",
		      strcmp(CommDriverTestHooks::Event(d), "/custom/event") == 0);
		check("SVGA mode: COMMAND does NOT honor argv (needs plain sim), stays NULL",
		      CommDriverTestHooks::Command(d) == 0);
	}

	/* 4. Unrecognized argv entries (no matching NAME) are silently ignored;
	 * simulation mode with zero real keys present still yields all 3 defaults.
	 */
	{
		s_eAppMode = 1;
		char *argv[] = {
			(char *)"SOME_OTHER_KEY=whatever",
			0
		};
		CCommDriver *d = rawDriver();
		CommDriverTestHooks::CallSetup(d, argv);
		check("sim mode, no real keys: LCD falls to default",
		      strcmp(CommDriverTestHooks::Lcd(d), "/tmp/evaclientfifo") == 0);
		check("sim mode, no real keys: EVENT falls to default",
		      strcmp(CommDriverTestHooks::Event(d), "/tmp/evaeventfifo") == 0);
		check("sim mode, no real keys: COMMAND falls to default",
		      strcmp(CommDriverTestHooks::Command(d), "/tmp/evacommandfifo") == 0);
	}

	/* 5. argv with only argv[0] and no other entries -- NOT exercised here on
	 * purpose: the real function unconditionally derefs strchr()'s result with
	 * no NULL check (see comm_driver.h's header comment), so a bare argv[0]
	 * with no '=' would crash setupfifoname() itself, exactly like it would in
	 * the real binary. That is the real, confirmed-at-the-disassembly-level
	 * bug this test's own header comment documents -- deliberately not
	 * "worked around" here either, matching this project's own "preserve bugs
	 * as found" convention. Every argv array above always carries an '='-
	 * bearing entry for exactly this reason.
	 */

	printf("%d check(s) failed\n", g_fail);
	return g_fail ? 1 : 0;
}
