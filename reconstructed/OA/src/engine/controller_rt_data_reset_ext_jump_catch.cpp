// SPDX-License-Identifier: GPL-2.0
/*
 * controller_rt_data_reset_ext_jump_catch.cpp  -  CSTGControllerRTData::
 * OnEndDownload()/ResetExtKnobJumpCatch(unsigned int)/
 * ResetExtSliderJumpCatch(unsigned int) (round 65, solo). See
 * include/oa_global.h's own header comment above these 3 declarations.
 *
 * OnEndDownload: zeroes this+0x30..0x3c (4 dwords) and clears the low
 * nibble of this+0x2f, leaving the high nibble untouched.
 *
 * ResetExtKnobJumpCatch(idx)/ResetExtSliderJumpCatch(idx): both read one
 * byte from the real STGAPIFrontPanelStatus::sInstance table (already
 * established convention, oa_setup_global_resources.h) at a fixed base
 * (+0x90b for knob, +0x923 for slider) offset by idx, and if that byte is
 * a valid 7-bit value (<0x80) store it into this's own per-slot table
 * (3-byte stride, base +0x56/+0x54 for knob, +0x6e/+0x6c for slider).
 * Then, gated on CSTGGlobal::sInstance's own byte at the well-established
 * `+0x29c9fc0` offset (already used elsewhere in this project, e.g.
 * oa_adsr_base.h's UpdateXxx family): if that gate byte is 0, just marks
 * the slot "needs catch" (byte=1) and returns; otherwise compares the
 * slot's own current vs. target byte fields and sets a tri-state jump-
 * catch flag (0xff/0/2) matching the real ground-truth arithmetic exactly
 * (`(target <= current) * 2`).
 */

#include "oa_global.h"
#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"

void CSTGControllerRTData::OnEndDownload()
{
	unsigned char *self = (unsigned char *)this;
	*(unsigned int *)(self + 0x3c) = 0;
	*(unsigned int *)(self + 0x38) = 0;
	*(unsigned int *)(self + 0x34) = 0;
	*(unsigned int *)(self + 0x30) = 0;
	self[0x2f] = (unsigned char)(self[0x2f] & 0xf0);
}

void CSTGControllerRTData::ResetExtKnobJumpCatch(unsigned int idx)
{
	unsigned char *self = (unsigned char *)this;
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;

	unsigned char raw = STGAPIFrontPanelStatus::sInstance[0x90b + idx];
	if (raw < 0x80)
		self[0x56 + idx * 3] = raw;

	if (g[0x29c9fc0u] == 0) {
		self[0x54 + idx * 3] = 1;
		return;
	}

	unsigned char *slot = self + 0x50 + idx * 3;
	signed char cur = (signed char)slot[5];
	if (cur == -1) {
		slot[4] = 0xff;
		return;
	}
	signed char target = (signed char)slot[6];
	if (cur != target) {
		slot[4] = (unsigned char)((target <= cur) ? 2 : 0);
		return;
	}
	slot[4] = 1;
}

void CSTGControllerRTData::ResetExtSliderJumpCatch(unsigned int idx)
{
	unsigned char *self = (unsigned char *)this;
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;

	unsigned char raw = STGAPIFrontPanelStatus::sInstance[0x923 + idx];
	if (raw < 0x80)
		self[0x6e + idx * 3] = raw;

	if (g[0x29c9fc0u] == 0) {
		self[0x6c + idx * 3] = 1;
		return;
	}

	unsigned char *slot = self + 0x60 + idx * 3;
	signed char cur = (signed char)slot[0xd];
	if (cur == -1) {
		slot[0xc] = 0xff;
		return;
	}
	signed char target = (signed char)slot[0xe];
	if (cur != target) {
		slot[0xc] = (unsigned char)((target <= cur) ? 2 : 0);
		return;
	}
	slot[0xc] = 1;
}
