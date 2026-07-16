// SPDX-License-Identifier: GPL-2.0
/*
 * test_driver_receive_event_buffer.cpp - known-answer tests for
 * COmapNKS4Driver::ReceiveEventBuffer, covering the two real bugs found and fixed
 * this session (2026-07-15) via a fresh Ghidra decompile of ReceiveEventBuffer@0x13360
 * (OmapNKS4Module.ko 3.2.2):
 *
 *   1. The op==1(Button)/idx==0x71 branch (ReadPortConfiguration's response wake-up)
 *      was missing the opcode contribution to its reg-echo word.
 *   2. The aftertouch (op==3) event byte-packing was low/high swapped, and the
 *      test-mode two-event branch was an unimplemented stub.
 *
 * See KronosNKS4/docs/gaps.md for the full derivation. Uses host_stubs.cpp's
 * recording stubs (SendNKS4EventToLinuxReader, CSTGOmapNKS4Fifos::sInstance) to
 * observe what ReceiveEventBuffer actually does without a real kernel.
 */

#include "../omapnks4_internal.h"
#include <cstdio>
#include <cstring>

extern unsigned int host_stub_last_reader_word;
extern int host_stub_reader_call_count;
extern bool host_stub_calibration_passthrough;
extern bool host_stub_filter_allow;

static int g_fail;

static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	if (got == want) {
		std::printf("  ok    %-55s 0x%08x\n", label, got);
		return;
	}
	std::printf("  FAIL  %-55s got=0x%08x want=0x%08x\n", label, got, want);
	g_fail++;
}

// Build a 4-byte record [dataLo,dataHi,idx,opcode] followed by a Sync (0x87)
// terminator, and feed it through ReceiveEventBuffer.
static void feed_one_record(unsigned char dLo, unsigned char dHi, unsigned char idx,
                             unsigned char op)
{
	unsigned char buf[8] = {dLo, dHi, idx, op, 0, 0, 0, 0x87};
	COmapNKS4Driver_sInstance.ReceiveEventBuffer(buf, 2);
}

static unsigned int last_fifo_word(void)
{
	struct CSTGOmapNKS4Fifos &f = CSTGOmapNKS4Fifos::sInstance;
	unsigned int idx = (f.inputFifo.dwWriteIndex - 1) & 0xff;
	return f.inputFifo.pRing[idx];
}

int main(void)
{
	std::printf("OmapNKS4Module driver.cpp ReceiveEventBuffer known-answer test\n");
	std::printf("================================================================\n");

	host_stub_filter_allow = true;
	host_stub_calibration_passthrough = true;

	// ---- Bug 1: ReadPortConfiguration echo (op=1/idx=0x71) ------------------------
	// Ground truth @0x137f3: EAX = 0x01710000 | dataHi<<8 | dataLo (the opcode is
	// hardcoded into the immediate, NOT computed as idx<<16 like every other branch).
	host_stub_reader_call_count = 0;
	feed_one_record(0xaa, 0xbb, 0x71, 0x01);
	check_eq("Button/idx=0x71 wakes SendNKS4EventToLinuxReader",
	         (unsigned int)host_stub_reader_call_count, 1u);
	check_eq("Button/idx=0x71 reg echo (top 16 bits)",
	         host_stub_last_reader_word >> 16, 0x0171u);
	check_eq("Button/idx=0x71 full word", host_stub_last_reader_word, 0x0171bbaau);

	// Sanity: a Button event at idx=0x50 (ordinary button, not the 0x71 echo case)
	// must NOT wake the reader.
	host_stub_reader_call_count = 0;
	feed_one_record(0x01, 0x00, 0x50, 0x01);
	check_eq("Button/idx=0x50 does not wake the reader",
	         (unsigned int)host_stub_reader_call_count, 0u);

	// ---- Bug 2a: aftertouch non-test-mode byte packing -----------------------------
	// Ground truth: word bits[0:7] = (val>>2)&0xff, bits[8:15] = (v<<6)&0xff (this
	// port had them swapped before the fix). idx=7 triggers the sAfterTouch table
	// remap path in the real driver, so use idx=1 here to exercise the plain
	// passthrough path with calibration as identity (host_stub_calibration_passthrough).
	COmapNKS4Driver_sInstance.fTestMode = false;
	feed_one_record(0x34, 0x12, 0x01, 0x03); // raw dLo|dHi<<8 = 0x1234, calibrated v = 0x1234 (passthrough)
	{
		// v = 0x1234 (as short, positive). val = v. word = idx<<16 | (val>>2)&0xff |
		// ((v<<6)&0xff)<<8 | 0x3000000.
		int v = 0x1234;
		int val = v;
		unsigned int expect = (1u << 16) | ((unsigned int)(val >> 2) & 0xff) |
		                       (((unsigned int)(v << 6) & 0xff) << 8) | 0x3000000u;
		check_eq("Aftertouch (non-test-mode) byte packing", last_fifo_word(), expect);
	}

	// ---- Bug 2b: aftertouch test-mode two-event emission ---------------------------
	COmapNKS4Driver_sInstance.fTestMode = true;
	unsigned int wi_before = CSTGOmapNKS4Fifos::sInstance.inputFifo.dwWriteIndex;
	feed_one_record(0x34, 0x12, 0x01, 0x03);
	unsigned int wi_after = CSTGOmapNKS4Fifos::sInstance.inputFifo.dwWriteIndex;
	check_eq("Aftertouch (test-mode) emits exactly 2 events", wi_after - wi_before, 2u);
	{
		struct CSTGOmapNKS4Fifos &f = CSTGOmapNKS4Fifos::sInstance;
		unsigned int w1 = f.inputFifo.pRing[wi_before & 0xff];
		unsigned int w2 = f.inputFifo.pRing[(wi_before + 1) & 0xff];

		// Event 1 (0x61): raw10 = (dLo<<2)|(dHi>>6); word bits[0:7]=raw10>>8,
		// bits[8:15]=raw10&0xff.
		unsigned int raw10 = ((unsigned int)0x34 << 2) | (0x12 >> 6);
		unsigned int expect1 = (1u << 16) | ((raw10 >> 8) & 0xff) | ((raw10 & 0xff) << 8) |
		                       0x61000000u;
		check_eq("Aftertouch test-mode event 1 (0x61, raw)", w1, expect1);

		// Event 2 (0x62): calibrated val/v (passthrough -> 0x1234), bits[0:7]=high
		// byte of val, bits[8:15]=low byte of v.
		int v = 0x1234, val = v;
		unsigned int expect2 = (1u << 16) | (((unsigned int)val >> 8) & 0xff) |
		                       (((unsigned int)(unsigned char)v) << 8) | 0x62000000u;
		check_eq("Aftertouch test-mode event 2 (0x62, calibrated)", w2, expect2);
	}
	COmapNKS4Driver_sInstance.fTestMode = false;

	std::printf("%s: %d failure(s)\n", g_fail ? "FAILED" : "PASSED", g_fail);
	return g_fail ? 1 : 0;
}
