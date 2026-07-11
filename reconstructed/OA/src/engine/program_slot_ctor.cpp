// SPDX-License-Identifier: GPL-2.0
/*
 * program_slot_ctor.cpp  -  CSTGToneAdjust::CSTGToneAdjust() (sec 10.153,
 * `.text+0xc76e0`, 247 bytes) and CSTGProgramSlot::CSTGProgramSlot() (sec
 * 10.153, `.text+0xabf80`, 219 bytes).
 *
 * Deliberately a separate translation unit from global.cpp: matches this
 * project's own established convention of keeping a newly-promoted ctor
 * out of the TU whose test mocks depend on it staying empty. Confirmed
 * via `grep -l CSTGProgramSlot::CSTGProgramSlot verify/` -- three
 * files (test_global_ctor.cpp/test_engine.cpp/test_global.cpp) each keep
 * their OWN independent empty mock of this ctor; none of them link
 * global.cpp's real CSTGProgramModeProgramSlot/CSTGProgramModeDrumTrackSlot
 * ctors through to a linked-in real base ctor today, so leaving all three
 * untouched is safe (no multiple-definition collision: this file is only
 * added to the real Kbuild .ko link, never to those host test binaries).
 *
 * See oa_global.h's own header comments on CSTGToneAdjust/CSTGProgramSlot
 * for the full confirmed field layout.
 */

#include "oa_global.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

unsigned char _ZTV14CSTGToneAdjust[12];
/* Sized to the real confirmed 0xf0-byte/60-slot vtable (nm -CS), fixed
 * batch 45 -- see oa_global.h's own header comment on this symbol for
 * why (was previously an undersized 12-byte placeholder). */
unsigned char _ZTV15CSTGProgramSlot[0xf0];

static unsigned int ToU32(void *p)
{
	return (unsigned int)(unsigned long)p;
}

CSTGToneAdjust::CSTGToneAdjust()
{
	unsigned char *base = (unsigned char *)this;

	*(unsigned int *)base = ToU32(_ZTV14CSTGToneAdjust + 8);
	for (unsigned int off = 0x4; off <= 0x24; off++)
		base[off] = 0;
	for (unsigned int off = 0x45; off <= 0x67; off += 2)
		*(unsigned short *)(base + off) = 0;
}

/*
 * CSTGProgramSlot::CSTGProgramSlot() -- see oa_global.h's own header
 * comment for the vtable/redundancy notes. Real instruction order:
 * install vtable ptr, zero a batch of scattered byte fields (some of
 * which -- +0x9/+0x30 -- get zeroed a SECOND time a few instructions
 * later; a real, confirmed, functionally-inert double-write, collapsed
 * to a single C statement each below since writing 0 twice is
 * observationally identical to writing it once), placement-construct
 * the embedded CSTGToneAdjust at +0x7f, then a second batch of
 * byte/dword/float writes (three floats set to 1.0f: +0x31/+0x73/+0x77/
 * +0x7b -- FOUR total, not three; +0x35 gets a real confirmed 4-byte
 * packed constant 0x3c010204, not a float).
 */
CSTGProgramSlot::CSTGProgramSlot()
{
	unsigned char *base = (unsigned char *)this;

	*(unsigned int *)base = ToU32(_ZTV15CSTGProgramSlot + 8);
	base[0x9] = 0;
	base[0xa] = 0;
	base[0xd] = 0;
	base[0x11] = 0;
	base[0x12] = 0;
	base[0x13] = 0;
	base[0x26] = 0;
	base[0x27] = 0;
	base[0x30] = 0;
	base[0x47] = 0;
	for (unsigned int off = 0x5c; off <= 0x6e; off++)
		base[off] = 0;

	new (base + 0x7f) CSTGToneAdjust();

	base[0x10] = 0;
	base[0x43] &= 0xfd;
	base[0xb] = 0;
	*(unsigned int *)(base + 0x5) = 0;
	base[0x4] = 0;
	*(unsigned int *)(base + 0x6f) = 0;
	*(float *)(base + 0x73) = 1.0f;
	*(float *)(base + 0x77) = 1.0f;
	*(float *)(base + 0x7b) = 1.0f;
	*(float *)(base + 0x31) = 1.0f;
	*(unsigned int *)(base + 0x39) = 0;
	*(unsigned int *)(base + 0x35) = 0x3c010204;
	base[0x45] |= 0x80;
}
