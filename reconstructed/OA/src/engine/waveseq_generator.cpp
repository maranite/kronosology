// SPDX-License-Identifier: GPL-2.0
/*
 * waveseq_generator.cpp  -  CSTGWaveSeqGenerator::CSTGWaveSeqGenerator()/
 * Init() (sec 10.152). A dedicated TU, matching wave_seq_manager.cpp's
 * own separation from CSTGWaveSeqManager's sibling class.
 */

#include "oa_engine_init.h"

unsigned char CSTGWaveSeqGenerator::sDummyAMS[4];

static unsigned int ToU32(unsigned char *p)
{
	return (unsigned int)(unsigned long)p;
}

/*
 * CSTGWaveSeqGenerator::CSTGWaveSeqGenerator() (.text+0x819a0, 193
 * bytes) confirmed: three self-anchored (empty) intrusive-list nodes
 * at +0x0/+0x8/+0xc, +0x10/+0x18/+0x1c, +0x20/+0x28/+0x2c (each
 * {prev=0, self=this, extra=0} in that field order, the same
 * "self-pointer anchors an empty list" idiom already confirmed
 * elsewhere in this project, e.g. CSTGHeapHandleEntry), plus a
 * straightforward zero-fill of the remaining scalar fields
 * (+0x30/+0x34/+0x38/+0x110) and two flag-bit clears (+0x64 &= 0xf7,
 * +0x78 &= 0xfd) on otherwise-uninitialized storage (this class lives
 * inside CSTGWaveSeqManager's own pre-allocated array, sec 10.62, not
 * freshly zeroed memory).
 */
CSTGWaveSeqGenerator::CSTGWaveSeqGenerator()
{
	unsigned char *self = (unsigned char *)this;

	*(unsigned int *)(self + 0x8) = ToU32(self);
	*(unsigned int *)(self + 0x0) = 0;
	*(unsigned int *)(self + 0x4) = 0;
	*(unsigned int *)(self + 0xc) = 0;

	*(unsigned int *)(self + 0x18) = ToU32(self);
	*(unsigned int *)(self + 0x10) = 0;
	*(unsigned int *)(self + 0x14) = 0;
	*(unsigned int *)(self + 0x1c) = 0;

	*(unsigned int *)(self + 0x28) = ToU32(self);
	*(unsigned int *)(self + 0x20) = 0;
	*(unsigned int *)(self + 0x24) = 0;
	*(unsigned int *)(self + 0x2c) = 0;

	self[0x70] = 0;
	*(unsigned int *)(self + 0x110) = 0;
	*(unsigned short *)(self + 0x38) = 0;
	*(unsigned short *)(self + 0x34) = 0;
	*(unsigned int *)(self + 0x30) = 0;

	self[0x64] &= 0xf7;
	self[0x78] &= 0xfd;

	*(unsigned int *)(self + 0xbc) = 0;
	*(unsigned int *)(self + 0xc0) = 0;
	*(unsigned int *)(self + 0xc4) = 0;
	*(unsigned int *)(self + 0xc8) = 0;
	*(unsigned int *)(self + 0xcc) = 0;
	*(unsigned int *)(self + 0xd0) = 0;
	*(unsigned int *)(self + 0xd4) = 0;
	*(unsigned int *)(self + 0xd8) = 0;
}

/*
 * CSTGWaveSeqGenerator::Init() (.text+0x81a70, 261 bytes) confirmed:
 * caches three "self+N" pointers at +0xbc/+0xc0/+0xc4 (derived from
 * +0xec/+0xe4/+0xe8 respectively) alongside masking two flag bytes at
 * +0x64 (&= 0xf0) and +0x78 (&= 0xe0) in place -- confirmed real,
 * though the exact bit meaning of the masked-off low bits isn't
 * independently determined -- a fourth self-pointer at +0xd4 (from
 * +0xdc) and a fifth at +0xd8 (from +0xe0), a long zero-fill run of
 * scalar/flag fields (+0x30/+0x3c/+0x3e/+0x40/+0x44/+0x48/+0x4c/+0x50/
 * +0x70/+0x71 &= 0xfe/+0x72/+0x74 &= 0xfd/+0x7a/+0x7c/+0x7e/+0x80/+0x82/
 * +0x100/+0xb8/+0x68/+0x6c, all confirmed zero), and finally stores the
 * ADDRESS of the shared `sDummyAMS` placeholder object into FIVE
 * separate pointer fields (+0xfc/+0xf8/+0xc8/+0xcc/+0xd0, confirmed via
 * five independent `R_386_32` relocations to the same symbol) -- the
 * real disassembly's own instruction order is preserved exactly below,
 * interleaving these writes with the +0xd8 self-pointer store rather
 * than grouping them.
 */
void CSTGWaveSeqGenerator::Init()
{
	unsigned char *self = (unsigned char *)this;

	*(unsigned int *)(self + 0xbc) = ToU32(self + 0xec);
	self[0x64] &= 0xf0;

	*(unsigned int *)(self + 0xc0) = ToU32(self + 0xe4);
	self[0x78] &= 0xe0;

	*(unsigned int *)(self + 0xc4) = ToU32(self + 0xe8);

	self[0x70] = 0;
	*(unsigned int *)(self + 0xd4) = ToU32(self + 0xdc);
	*(unsigned short *)(self + 0x3c) = 0;
	*(unsigned short *)(self + 0x3e) = 0;
	*(unsigned short *)(self + 0x40) = 0;
	*(unsigned int *)(self + 0x30) = 0;
	*(unsigned int *)(self + 0x44) = 0;
	*(unsigned int *)(self + 0x48) = 0;
	*(unsigned int *)(self + 0x4c) = 0;
	*(unsigned int *)(self + 0x50) = 0;
	*(unsigned short *)(self + 0x7a) = 0;
	self[0x71] &= 0xfe;
	self[0x74] &= 0xfd;
	*(unsigned short *)(self + 0x72) = 0;
	*(unsigned short *)(self + 0x7e) = 0;
	*(unsigned short *)(self + 0x80) = 0;
	*(unsigned short *)(self + 0x7c) = 0;
	*(unsigned short *)(self + 0x100) = 0;
	*(unsigned short *)(self + 0x82) = 0;

	*(unsigned int *)(self + 0xb8) = 0;
	*(unsigned int *)(self + 0xfc) = ToU32(CSTGWaveSeqGenerator::sDummyAMS);
	*(unsigned int *)(self + 0xf8) = ToU32(CSTGWaveSeqGenerator::sDummyAMS);
	*(unsigned int *)(self + 0xc8) = ToU32(CSTGWaveSeqGenerator::sDummyAMS);

	*(unsigned int *)(self + 0xd8) = ToU32(self + 0xe0);
	*(unsigned int *)(self + 0xcc) = ToU32(CSTGWaveSeqGenerator::sDummyAMS);
	*(unsigned int *)(self + 0xd0) = ToU32(CSTGWaveSeqGenerator::sDummyAMS);

	*(unsigned int *)(self + 0x6c) = 0;
	*(unsigned int *)(self + 0x68) = 0;
}
