/*
 * comm_driver.cpp  -  see include/comm_driver.h.
 *
 * Both overloads transcribed from the Ghidra decompile:
 *   getInstance(char**)  Decomp/EVA_Decomp/eva_export/functions/getInstance@08e4f6e0.c
 *   getInstance()        Decomp/EVA_Decomp/eva_export/functions/getInstance@08e4f250.c
 *
 * CCommDriver's constructor body (what it does with argv) is Stage 2 -- declared but
 * not implemented here; only the singleton bring-up/guard logic (Stage 1) is done.
 */

#include "comm_driver.h"

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

CCommDriver *CCommDriver::singleton = 0;

namespace {
/* .text+0x08e4f310, 681 bytes -- Tier-B link-stub, not reconstructed (genuinely
 * deeper argv-parsing/path-building substrate, out of scope for this pass). Real
 * job is to fill in the 3 fifo path fields from argv; this stub leaves them null
 * (the ctor pre-zeroes them below, since the real function's own contract is just
 * "populate these 3 pointers", and null is that function's own real answer when
 * argv carries no matching path -- every path is null-checked before open() either
 * way, so this is a safe, real-shaped stub output, not a fabricated behavior).
 */
void setupfifoname(CCommDriver * /*self*/, char ** /*argv*/) {}
} // namespace

CCommDriver::CCommDriver(char **argv)
{
	mLcdFifoPath = 0;
	mEventFifoPath = 0;
	mCommandFifoPath = 0;
	mLcdFd = -1;
	mCommandFd = -1;
	mEventFd = -1;

	setupfifoname(this, argv);

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
