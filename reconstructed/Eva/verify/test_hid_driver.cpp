/*
 * test_hid_driver.cpp  -  host-side known-answer test for CHIDDriver
 * (src/hw/hid_driver.cpp), Stage 6 breadth sweep, 2026-07-25.
 *
 * Found via the project's broad `nm -C` class-inventory sweep -- CHIDDriver is
 * MMainHIDDriver's own direct-construction target (mains.cpp), constructed
 * unconditionally every boot before any config-table gating. See hid_driver.h's
 * own header comment for the full ground-truth vtable/layout writeup.
 *
 * Drives GetEvent()/GetKeyboardEvent() against a real pipe(2) fd standing in for
 * the real evdev fd (HIDDriverTestHooks lets this test set mFd directly, bypassing
 * KeyboardIsConnected()'s own /sys/class/input scan, which depends on real
 * hardware nodes not present on this host) -- exercises the real scancode decode
 * (s_kucMappingTable) and the real modifier-bitmask assembly end to end, not just
 * against synthetic byte buffers.
 */

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "hid_driver.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

struct HIDDriverTestHooks {
	static void SetFd(CHIDDriver &d, int fd) { d.mFd = fd; }
	static int Fd(const CHIDDriver &d) { return d.mFd; }
	static unsigned char Modifiers(const CHIDDriver &d) { return d.mModifiers; }
};

/* Writes one raw `struct input_event`-shaped 16-byte record into `fd`, matching
 * HIDRawInputEvent's own real layout (hid_driver.h).
 */
static void write_raw_event(int fd, short type, unsigned short code, int value)
{
	HIDRawInputEvent ev;
	ev.tv_sec = 0;
	ev.tv_usec = 0;
	ev.type = type;
	ev.code = code;
	ev.value = value;
	ssize_t n = write(fd, &ev, sizeof(ev));
	(void)n;
}

int main()
{
	printf("CHIDDriver known-answer test\n");
	printf("=============================\n");

	printf("[1] ctor/dtor + trivial no-op methods\n");
	{
		CHIDDriver d("HIDDriver", "Events", "Commands");
		check("ctor leaves mFd == -1", HIDDriverTestHooks::Fd(d) == -1);
		check("Open() returns 0", d.Open(0) == 0);
		check("Close() returns 0", d.Close(0) == 0);
		check("GetDriverClass() returns 10", d.GetDriverClass() == 10);
		check("ReadOvercurrentCondition() returns 0", d.ReadOvercurrentCondition() == 0);
		check("EnableAfterOvercurrent() returns 1", d.EnableAfterOvercurrent() == 1);
		/* SetLeds()/SetTypematicRateDelay()/PutEvent() are real unconditional
		 * no-ops in the ground truth -- calling them here just proves they
		 * don't crash/misbehave, matching the real binary's own behavior. */
		d.SetLeds(1);
		d.SetTypematicRateDelay(5);
		HIDUsbKeybEvent dummy;
		d.PutEvent(dummy);
		check("(no crash from the 3 real no-op methods)", true);
	}

	printf("[2] GetEvent(): real scancode decode via s_kucMappingTable + modifier "
	       "bitmask assembly, driven over a real pipe(2) fd\n");
	{
		CHIDDriver d("HIDDriver", "Events", "Commands");
		int fds[2];
		if (pipe(fds) != 0) {
			check("pipe() failed -- skipping [2]", false);
		} else {
			HIDDriverTestHooks::SetFd(d, fds[0]);

			/* scancode 2 (KEY_1) -> s_kucMappingTable[2] == 0x11 (ground-truth
			 * .rodata byte, hid_driver.cpp). value=1 == key-down. */
			write_raw_event(fds[1], 1 /* EV_KEY */, 2, 1);
			HIDUsbKeybEvent evt;
			memset(&evt, 0, sizeof(evt));
			int ok = d.GetEvent(&evt);
			check("GetEvent() returns 1 for a real EV_KEY record", ok == 1);
			check("keycode == s_kucMappingTable[2] == 0x11", evt.keycode == 0x11);

			/* scancode 0x1d (Ctrl), key-down -> modifier bit 0x01 set. */
			write_raw_event(fds[1], 1, 0x1d, 1);
			ok = d.GetEvent(&evt);
			check("Ctrl key-down sets modifier bit 0x01",
			      (HIDDriverTestHooks::Modifiers(d) & 0x01) != 0);

			/* same scancode, key-up (value==0) -> bit clears again. */
			write_raw_event(fds[1], 1, 0x1d, 0);
			ok = d.GetEvent(&evt);
			check("Ctrl key-up clears modifier bit 0x01",
			      (HIDDriverTestHooks::Modifiers(d) & 0x01) == 0);

			/* EV_SYN (type != 1) records are real ground-truth no-ops. */
			write_raw_event(fds[1], 0 /* EV_SYN */, 0, 0);
			ok = d.GetEvent(&evt);
			check("non-EV_KEY record: GetEvent() returns 0", ok == 0);

			close(fds[0]);
			close(fds[1]);
		}
	}

	printf("[3] GetEvent() with mFd == -1: real ground-truth no-op\n");
	{
		CHIDDriver d("HIDDriver", "Events", "Commands");
		HIDUsbKeybEvent evt;
		memset(&evt, 0, sizeof(evt));
		int ok = d.GetEvent(&evt);
		check("GetEvent() returns 0 when mFd < 0", ok == 0);
	}

	printf("[4] GetKeyboardEvent(): dispatches through mVtbl[5] (its own "
	       "GetEvent), relays keycode+modifiers -- confirms the byte-exact real "
	       "vtable is wired correctly, not just independently-correct methods\n");
	{
		CHIDDriver d("HIDDriver", "Events", "Commands");
		int fds[2];
		if (pipe(fds) != 0) {
			check("pipe() failed -- skipping [4]", false);
		} else {
			HIDDriverTestHooks::SetFd(d, fds[0]);
			/* scancode 3 (KEY_2) -> s_kucMappingTable[3] == 0x12. */
			write_raw_event(fds[1], 1, 3, 1);

			AlphaKeybEvt out;
			memset(&out, 0, sizeof(out));
			bool ok = d.GetKeyboardEvent(&out);
			check("GetKeyboardEvent() returns true (dispatched real GetEvent "
			      "via mVtbl[5], not a stub)", ok);
			check("relayed keycode == s_kucMappingTable[3] == 0x12",
			      out.keycode == 0x12);
			check("relayed modifiers == mModifiers (0, nothing held)",
			      out.modifiers == 0);

			close(fds[0]);
			close(fds[1]);
		}
	}

	printf("\n%d checks failed\n", g_fail);
	return g_fail != 0;
}
