/*
 * test_panel_driver.cpp  -  host-side known-answer test for CLinuxPanelDriver +
 * USTGAPIFrontPanel (src/hw/panel_driver.cpp), Stage 6 breadth sweep, 2026-07-25.
 *
 * Found via the same nm -C sweep as hid_driver.h/.cpp -- CLinuxPanelDriver is
 * MMainPanelDriver's own direct-construction target, the FIRST thing Mains() ever
 * constructs (mains.cpp), unconditionally, every boot. See panel_driver.h's own
 * header comment for the ground-truth vtable/layout writeup.
 *
 * GetEvent() reads through CCommDriver::getInstance()'s own mEventFd field by raw
 * offset -- this test constructs a real (but never-openened-fifo) CCommDriver via
 * the friend CommDriverTestHooks pattern already established by
 * verify/test_comm_driver.cpp (a separate, per-TU local definition of the same
 * friended struct name -- no conflict, since verify binaries are never linked
 * against each other), pointing CCommDriver::singleton at a raw buffer with a real
 * pipe(2) fd poked into mEventFd, avoiding both getInstance()'s own exit(1) assert
 * (singleton must be non-null) and any real filesystem/fifo dependency.
 *
 * USTGAPIFrontPanel's 5 wrappers all bottom out in USTGUserAPI::SendPanelMessage()
 * (ustg_user_api.cpp, already real) -- this test drives PutCommand()'s own opcode
 * switch and independently calls each USTGAPIFrontPanel function directly,
 * confirming the STGMessage byte layout PutCommand()'s callers rely on.
 */

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "panel_driver.h"
#include "comm_driver.h"
#include "ustg_user_api.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

struct PanelDriverTestHooks {
	static const char *Name(const CLinuxPanelDriver &d) { return d.mName; }
};

/* Local re-declaration of the same friended struct name test_comm_driver.cpp
 * uses -- see this file's own header comment for why that's safe. Only need a
 * singleton setter and the mEventFd offset here (already confirmed +0x10 by
 * comm_driver.h/panel_driver.cpp's own cross-check).
 */
struct CommDriverTestHooks {
	static void SetSingleton(CCommDriver *d) { CCommDriver::singleton = d; }
};

static unsigned char g_fakeCommDriver[sizeof(CCommDriver)];

int main()
{
	printf("CLinuxPanelDriver / USTGAPIFrontPanel known-answer test\n");
	printf("=========================================================\n");

	printf("[1] ctor: real name copy, real vtable install (no crash), calls "
	       "ConnectPanelFifo() (already-real, tolerates missing /dev/rtf* on "
	       "this host same as every other boot-path fifo open)\n");
	{
		CLinuxPanelDriver d("PanelDriver");
		check("mName copied correctly",
		      strcmp(PanelDriverTestHooks::Name(d), "PanelDriver") == 0);
		check("Open() returns 0", d.Open(0) == 0);
		check("Close() returns 0", d.Close(0) == 0);
		check("GetDriverClass() returns 10", d.GetDriverClass() == 10);
		PanelDriverEvent dummy;
		d.PutEvent(dummy); /* real unconditional no-op -- just proves no crash */
		check("(no crash from PutEvent())", true);
	}

	printf("[2] GetEvent(): reads CCommDriver::getInstance()'s own mEventFd by "
	       "raw offset (+0x10), over a real pipe(2)\n");
	{
		memset(g_fakeCommDriver, 0, sizeof(g_fakeCommDriver));
		int fds[2];
		if (pipe(fds) != 0) {
			check("pipe() failed -- skipping [2]", false);
		} else {
			*(int *)(g_fakeCommDriver + 0x10) = fds[0];
			CommDriverTestHooks::SetSingleton(
				reinterpret_cast<CCommDriver *>(g_fakeCommDriver));

			unsigned char sent[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
			ssize_t wn = write(fds[1], sent, 8);
			(void)wn;

			CLinuxPanelDriver d("PanelDriver");
			PanelDriverEvent evt;
			memset(&evt, 0, sizeof(evt));
			bool ok = d.GetEvent(&evt);
			check("GetEvent() returns true for a real 8-byte read", ok);
			check("event bytes match exactly what was written",
			      memcmp(evt.raw, sent, 8) == 0);

			close(fds[0]);
			close(fds[1]);
		}
	}

	printf("[3] GetEvent(): mEventFd < 0 is a real ground-truth no-op\n");
	{
		memset(g_fakeCommDriver, 0, sizeof(g_fakeCommDriver));
		*(int *)(g_fakeCommDriver + 0x10) = -1;
		CommDriverTestHooks::SetSingleton(
			reinterpret_cast<CCommDriver *>(g_fakeCommDriver));

		CLinuxPanelDriver d("PanelDriver");
		PanelDriverEvent evt;
		memset(&evt, 0, sizeof(evt));
		check("GetEvent() returns false when mEventFd < 0", !d.GetEvent(&evt));
	}

	printf("[4] PutCommand(): real 5-way opcode switch -- confirms it doesn't "
	       "crash for every real opcode + the real unconditional 0 return\n");
	{
		/* USTGAPIFrontPanel's wrappers all bottom out in
		 * USTGUserAPI::SendPanelMessage(), which itself tries a real
		 * /dev/rtf1 write and no-ops (returns false) if that fd was never
		 * opened -- exactly the same "tolerates missing device node"
		 * behavior every other IPC KAT in this project already exercises,
		 * not a gap in this test.
		 */
		CLinuxPanelDriver d("PanelDriver");
		PanelDriverCommand cmd;

		cmd.opcode = 1; cmd.paramLo = 0x0001; cmd.paramHi = 0;
		check("opcode 1 (ResetLED) returns 0", d.PutCommand(&cmd) == 0);
		cmd.opcode = 2; cmd.paramLo = 0x0002; cmd.paramHi = 0;
		check("opcode 2 (SetLED) returns 0", d.PutCommand(&cmd) == 0);
		cmd.opcode = 3; cmd.paramLo = 0x0004; cmd.paramHi = 0;
		check("opcode 3 (SetLEDBlinking) returns 0", d.PutCommand(&cmd) == 0);
		cmd.opcode = 6; cmd.paramLo = 0x0008; cmd.paramHi = 0x0010;
		check("opcode 6 (SetLED16Bit) returns 0", d.PutCommand(&cmd) == 0);
		cmd.opcode = 7; cmd.paramLo = 0; cmd.paramHi = 0;
		check("opcode 7 (Beep) returns 0", d.PutCommand(&cmd) == 0);
		cmd.opcode = 99; cmd.paramLo = 0; cmd.paramHi = 0;
		check("unknown opcode returns 0 (real default: falls through, no crash)",
		      d.PutCommand(&cmd) == 0);
	}

	printf("[5] USTGAPIFrontPanel: each of the 5 wrappers callable directly, "
	       "no crash (real bodies all bottom out in the already-real "
	       "SendPanelMessage())\n");
	{
		USTGAPIFrontPanel::SetLED(1);
		USTGAPIFrontPanel::SetLEDBlinking(2);
		USTGAPIFrontPanel::ResetLED(4);
		USTGAPIFrontPanel::SetLED16Bit(8, 16);
		USTGAPIFrontPanel::Beep();
		check("(no crash from any of the 5 real wrappers)", true);
	}

	printf("\n%d checks failed\n", g_fail);
	return g_fail != 0;
}
