// SPDX-License-Identifier: GPL-2.0
/*
 * controller_rt_data_ctor.cpp  -  CSTGControllerRTData::CSTGControllerRTData()
 * (sec 10.155, `.text+0xd250`, 475 bytes).
 *
 * Deliberately a separate translation unit from controller_rt_data_init.cpp/
 * global.cpp: test_global_ctor.cpp keeps its own load-bearing call-counter
 * mock for this exact ctor symbol (`g_controllerRtCalls++`), matching the
 * established per-unit convention for any promoted symbol with an existing
 * mock elsewhere (confirmed via a repo-wide grep for this exact ctor
 * symbol across the verify test directory -- only test_global_ctor.cpp
 * defines it, and that file does not link this TU).
 *
 * Ground-truthed via a full objdump -d -r disassembly: a genuinely
 * self-contained, straight-line ctor -- no calls, no branches, no vtable
 * install (CSTGControllerRTData has no vtable; confirmed via `nm -C
 * OA.ko | grep "vtable for CSTGControllerRTData"`, no match), just a
 * fixed sequence of immediate field stores plus one `sInstance = this`
 * self-registration in the middle of the sequence (a confirmed real,
 * harmless ordering quirk, preserved verbatim). Class layout otherwise
 * remains opaque (oa_global.h's own established convention for this
 * class) -- only the ctor's own confirmed writes are modeled here via
 * raw offset arithmetic.
 *
 * Two confirmed real read-modify-write quirks, both preserved bug-for-bug
 * (the real disassembly reads the pre-existing byte before masking it,
 * rather than writing a fixed constant -- meaningful only because this
 * object is placement-constructed onto reused bank memory whose prior
 * contents aren't otherwise guaranteed zero):
 *   - `+0x21 &= 0xfc`  (clears bits 0-1, preserves bits 2-7)
 *   - `+0x2f &= 0xf0`  (clears the low nibble, preserves the high nibble)
 * and one confirmed real OR-mask (sets bit 0, preserves the rest):
 *   - `+0x49 |= 0x01`
 *
 * The 17 confirmed `{0xff, 0xff, 0x00}` triples from +0x54 to +0x86 are a
 * real repeating per-entry pattern (byte semantics not independently
 * determined -- modeled as a flat loop rather than named fields, matching
 * this project's established "declare the shape, defer interpretation"
 * treatment). `sInstance = this` is confirmed to happen strictly between
 * the 9th and 10th triple (after +0x6e, before +0x6f) -- reproduced at
 * that exact point rather than hoisted to the top/bottom of the ctor.
 */

#include "oa_global.h"

/* Storage moved here from bar2_stubs.cpp (sec 10.155), matching the
 * CSTGFrontPanelSmoothers/CSTGHDRMiniModel precedent of homing
 * `sInstance` storage in the same TU as the real ctor. */
CSTGControllerRTData *CSTGControllerRTData::sInstance;

CSTGControllerRTData::CSTGControllerRTData()
{
	unsigned char *self = (unsigned char *)this;

	self[0x0c] = 0;
	self[0x0d] = 0;
	self[0x0e] = 0;
	self[0x0f] = 0;
	self[0x10] = 0;
	*(unsigned short *)(self + 0x12) = 0x2000;
	self[0x16] = 0;

	/* First 9 of the 17 confirmed {0xff, 0xff, 0x00} triples,
	 * +0x54..+0x6e. */
	for (int i = 0; i < 9; i++) {
		unsigned char *g = self + 0x54 + i * 3;
		g[0] = 0xff;
		g[1] = 0xff;
		g[2] = 0x00;
	}

	/* Confirmed real ordering quirk: sInstance is set HERE, in the
	 * middle of the 17-triple run, not at ctor entry/exit. */
	CSTGControllerRTData::sInstance = this;

	/* Remaining 8 triples, +0x6f..+0x86. */
	for (int i = 9; i < 17; i++) {
		unsigned char *g = self + 0x54 + i * 3;
		g[0] = 0xff;
		g[1] = 0xff;
		g[2] = 0x00;
	}

	self[0x00] = 0;
	self[0x01] = 0;
	self[0x02] = 0;
	self[0x03] = 0;
	self[0x04] = 0x7f;
	self[0x05] = 0x01;
	*(unsigned int *)(self + 0x08) = 0;

	/* Confirmed real AND-mask RMW -- clears bits 0-1, preserves 2-7. */
	self[0x21] &= 0xfc;

	self[0x26] = 0;
	*(unsigned short *)(self + 0x24) = 0;
	*(unsigned short *)(self + 0x22) = 0;
	*(unsigned int *)(self + 0x3c) = 0;
	*(unsigned int *)(self + 0x38) = 0;
	*(unsigned int *)(self + 0x34) = 0;
	*(unsigned int *)(self + 0x30) = 0;

	/* Confirmed real AND-mask RMW -- clears the low nibble, preserves
	 * the high nibble. */
	self[0x2f] &= 0xf0;

	*(unsigned int *)(self + 0x4c) = 0x3f800000; /* 1.0f */
	*(unsigned int *)(self + 0x50) = 0x3f000000; /* 0.5f */

	self[0x27] = 0;
	self[0x2a] = 0;
	self[0x29] = 0;
	self[0x28] = 0;
	self[0x15] = 0;
	self[0x14] = 0;
	self[0x1d] = 0;
	self[0x1e] = 0;
	self[0x1f] = 0;
	self[0x20] = 0x40;
	*(unsigned short *)(self + 0x18) = 0x200;
	*(unsigned short *)(self + 0x1a) = 0x200;
	self[0x1c] = 0;
	self[0x2b] = 0;
	self[0x2c] = 0;
	self[0x2d] = 0x02;
	self[0x2e] = 0x06;
	self[0x40] = 0;
	self[0x41] = 0;
	self[0x42] = 0;
	self[0x43] = 0;
	self[0x44] = 0;
	self[0x45] = 0;
	self[0x46] = 0;
	self[0x47] = 0;
	self[0x48] = 0;

	/* Confirmed real OR-mask RMW -- sets bit 0, preserves the rest. */
	self[0x49] |= 0x01;
}
