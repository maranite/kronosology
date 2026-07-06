// SPDX-License-Identifier: GPL-2.0
/*
 * smoother_init.cpp  -  CSTGSmoother::Initialize() (sec 10.86).
 *
 * Deliberately a separate translation unit from global.cpp: this
 * symbol's mock in test_engine_init.cpp is a load-bearing call-counter
 * for CSTGEngine::Initialize()'s own dispatch verification, matching
 * the same reasoning as midi_queue_writer.cpp/waveseq_setlist_init.cpp/
 * alias_bank_init.cpp (sec 10.83-10.85).
 *
 * UPDATE (batch 22): CSTGSmoother::CSTGSmoother() is now real too (see
 * src/engine/smoother_ctor.cpp) -- it independently CONFIRMS this
 * comment's own long-standing prediction that +0xf004/+0xf008/+0xf00c
 * are zeroed by the real ctor (now proven via fresh disassembly, not
 * just inferred from this method's own field usage), and additionally
 * zeroes three more list-management fields (+0xf010/+0xf014/+0xf018)
 * this comment didn't previously need to mention.
 */

#include "oa_engine_init.h"
#include "oa_bank_memory.h"
#include "oa_setup_global_resources.h"

static unsigned char *FromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}
static unsigned int ToU32(unsigned char *p)
{
	return (unsigned int)(unsigned long)p;
}

/*
 * CSTGSmoother::Initialize() (.text+0x2a2c0 in OA_real.ko) -- confirmed
 * real:
 *   1. Allocates a 0x3c00-byte (15360) buffer via
 *      CSTGBankMemory::AllocAligned(0x3c00, 0x10), stores it at +0xf000,
 *      zeroes it.
 *   2. Builds a real intrusive doubly-linked FREE LIST of all 320
 *      embedded "SmootherMapping" sub-objects (each 0xc0/192 bytes,
 *      matching CSTGSmoother's own -- still deferred -- constructor's
 *      own confirmed stride), head/tail/count at +0xf004/+0xf008/
 *      +0xf00c. Each mapping object's own +0x0 gets its own 16-bit
 *      index written first; the list-link fields themselves live at
 *      a +0xb0 offset WITHIN each mapping object (next=+0xb0, prev=
 *      +0xb4, owner=+0xbc, relative to the mapping object's own base --
 *      i.e. the embedded link sub-object starts at mapping+0xb0).
 *      Confirmed byte-for-byte identical push-FRONT insertion shape to
 *      CSTGWaveSeqManager::Initialize()'s own already-reconstructed
 *      free-list build (sec 10.62) -- reused that template directly,
 *      just with these different field offsets substituted.
 *   3. Zeroes +0xf01c/+0xf020.
 *   4. Computes `(int)(0.04 * CSTGCPUInfo::sInstance->field8)` (the
 *      confirmed real 0.04 double constant, extracted directly from
 *      .rodata.cst8, not guessed) and stores it at +0xf024.
 */
void CSTGSmoother::Initialize()
{
	unsigned char *base = (unsigned char *)this;

	unsigned char *buf = CSTGBankMemory::AllocAligned(0x3c00, 0x10);
	*(unsigned int *)(base + 0xf000) = ToU32(buf);
	for (unsigned int i = 0; i < 0x3c00; i++)
		buf[i] = 0;

	for (unsigned int i = 0; i < 0x140; i++) {
		unsigned char *mapping = base + i * 0xc0;
		*(unsigned short *)mapping = (unsigned short)i;
		unsigned char *link = mapping + 0xb0;
		unsigned int linkAddr = ToU32(link);

		unsigned int oldHead = *(unsigned int *)(base + 0xf004);
		if (oldHead == 0) {
			*(unsigned int *)(base + 0xf008) = linkAddr; /* first insertion == permanent tail */
		} else {
			unsigned char *head = FromU32(oldHead);
			unsigned int headPrev = *(unsigned int *)(head + 0x4);
			*(unsigned int *)(link + 0x4) = headPrev;
			if (headPrev != 0)
				*(unsigned int *)FromU32(headPrev) = linkAddr;
			*(unsigned int *)(head + 0x4) = linkAddr;
			*(unsigned int *)(link + 0x0) = oldHead;
		}
		*(unsigned int *)(base + 0xf004) = linkAddr;
		*(unsigned int *)(link + 0xc) = ToU32(base + 0xf004);
		*(unsigned int *)(base + 0xf00c) += 1;
	}

	*(unsigned int *)(base + 0xf01c) = 0;
	*(unsigned int *)(base + 0xf020) = 0;

	double cyclesPerTick = CSTGCPUInfo::sInstance->field8;
	*(int *)(base + 0xf024) = (int)(0.04 * cyclesPerTick);
}
