/*
 * comm_driver.cpp  -  see include/comm_driver.h.
 *
 * Both getInstance() overloads transcribed from the Ghidra decompile:
 *   getInstance(char**)  Decomp/EVA_Decomp/eva_export/functions/getInstance@08e4f6e0.c
 *   getInstance()        Decomp/EVA_Decomp/eva_export/functions/getInstance@08e4f250.c
 *
 * setupfifoname() upgraded from Tier-B stub to Tier A, Stage 6 breadth sweep
 * 2026-07-25 -- see comm_driver.h's own header comment for the full behavior
 * writeup (including the real no-NULL-check crash bug) and
 * Decomp/EVA_Decomp/eva_export/functions/setupfifoname@08e4f310.c for the source
 * decompile this was transcribed from.
 */

#include "app_mode.h"
#include "comm_driver.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

CCommDriver *CCommDriver::singleton = 0;

/* Real per-argv-entry parser. bVar9 (always 0 in the real disassembly -- a
 * direction flag the compiler emitted for a generic byte-copy idiom that's
 * never actually taken in reverse here) collapses every "(uint)bVar9*-2+1"
 * pointer step in the original to a plain +1; verified index-by-index against
 * the real decompile before collapsing, same license as this project's other
 * unrolled-loop transcriptions.
 */
void CCommDriver::setupfifoname(char **argv)
{
	mLcdFifoPath = 0;
	mEventFifoPath = 0;
	mCommandFifoPath = 0;

	for (char **p = argv; *p != 0; ++p) {
		char *arg = *p;
		size_t len = strlen(arg);
		char *dup = new char[len + 1];
		if (dup == 0)
			continue;
		strcpy(dup, arg);

		/* REAL BUG, confirmed at the raw-disassembly level: strchr()'s
		 * result is dereferenced with no NULL check. See comm_driver.h's
		 * header comment. Preserved exactly as found.
		 */
		char *eq = strchr(dup, '=');
		*eq = '\0';
		char *value = eq + 1;
		const char *name = dup;

		if (strcmp(name, "NKS4_LCDFIFO") == 0) {
			if (Eva_IsSimulation()) {
				char *v = new char[strlen(value) + 1];
				strcpy(v, value);
				mLcdFifoPath = v;
			}
		} else if (strcmp(name, "NKS4_EVENTSFIFO") == 0) {
			if (Eva_IsSimulation() || Eva_IsSimulationSVGA()) {
				char *v = new char[strlen(value) + 1];
				strcpy(v, value);
				mEventFifoPath = v;
			}
		} else if (strcmp(name, "NKS4_COMMANDSFIFO") == 0) {
			if (Eva_IsSimulation()) {
				char *v = new char[strlen(value) + 1];
				strcpy(v, value);
				mCommandFifoPath = v;
			}
		}

		delete[] dup;
	}

	/* Real fallback defaults, one real fixed string apiece -- the real binary
	 * packs these as raw dword stores into freshly `new[]`'d buffers (a
	 * classic GCC string-literal-inlining artifact); reproduced here as plain
	 * strcpy() from a real string literal, functionally identical, sizes
	 * checked byte-for-byte against the real allocation sizes (0x13/0x12/0x14).
	 */
	if (mLcdFifoPath == 0 && Eva_IsSimulation()) {
		mLcdFifoPath = new char[19];
		strcpy(mLcdFifoPath, "/tmp/evaclientfifo");
	}
	if (mEventFifoPath == 0 && (Eva_IsSimulation() || Eva_IsSimulationSVGA())) {
		mEventFifoPath = new char[18];
		strcpy(mEventFifoPath, "/tmp/evaeventfifo");
	}
	if (mCommandFifoPath == 0 && Eva_IsSimulation()) {
		mCommandFifoPath = new char[20];
		strcpy(mCommandFifoPath, "/tmp/evacommandfifo");
	}
}

CCommDriver::CCommDriver(char **argv)
{
	mLcdFd = -1;
	mCommandFd = -1;
	mEventFd = -1;

	setupfifoname(argv);

	if (mLcdFifoPath != 0) {
		mLcdFd = open(mLcdFifoPath, O_WRONLY);
		if (mLcdFd < 0)
			fprintf(stderr, "LCD fifo \"%s\" open error\n", mLcdFifoPath);
	}
	if (mCommandFifoPath != 0) {
		mCommandFd = open(mCommandFifoPath, O_WRONLY);
		if (mCommandFd < 0)
			fprintf(stderr, "Command fifo \"%s\" open error\n", mCommandFifoPath);
	}
	if (mEventFifoPath != 0) {
		mEventFd = open(mEventFifoPath, O_NONBLOCK);
		if (mEventFd < 0)
			fprintf(stderr, "Event fifo \"%s\" open error\n", mEventFifoPath);
	}
}

CCommDriver *CCommDriver::getInstance(char **argv)
{
	if (singleton != 0)
		return singleton;

	CCommDriver *self = new CCommDriver(argv);
	singleton = self;
	return self;
}

CCommDriver *CCommDriver::getInstance()
{
	if (singleton != 0)
		return singleton;

	/* Real, unconditional process abort -- not a caller error to "fix" here.
	 * Any code path that reaches this overload assumes getInstance(argv) (the
	 * real boot-path constructor call from main()) already ran.
	 */
	fwrite("CCommDriver init error\n", 1, 0x17, stderr);
	exit(1);
}
