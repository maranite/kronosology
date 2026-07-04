// SPDX-License-Identifier: GPL-2.0
/*
 * waveseq_setlist_init.cpp  -  CSTGWaveSeqData::Initialize()/
 * CSetListBank::Initialize() (sec 10.84).
 *
 * Deliberately a SEPARATE translation unit from global.cpp: both
 * symbols' mocks in test_global.cpp/test_global_ctor.cpp/test_engine.cpp
 * are load-bearing call-counting checks for CSTGGlobal::Initialize()'s
 * OWN dispatch (not these methods' internals), and the real bodies
 * below genuinely dispatch through hundreds of sub-objects' own vtable
 * pointers -- exercising that safely needs dedicated, purpose-built
 * memory setup, not a quick patch to the existing large shared tests.
 * Matches this project's own established per-unit file convention
 * (e.g. midi_queue_writer.cpp, sec 10.83).
 */

#include "oa_global.h"

/*
 * CSTGWaveSeqData::Initialize()/CSetListBank::Initialize() (sec 10.84,
 * .text+0x81860/0x2014c0 in OA_real.ko) -- confirmed real, IDENTICAL
 * shape: a loop over N embedded sub-objects (stride confirmed literal
 * per class), dispatching each one's own vtable slot 7 (`call
 * *0x1c(%edx)`, the SAME slot CSTGProgramSlot::Initialize() dispatches
 * through -- reuses CallVtableSlot7 rather than a separate helper).
 * CSTGWaveSeqData: 598 (0x256) sub-objects, stride 0xd14 (3348 bytes).
 * CSetListBank: 128 (0x80) sub-objects, stride 0x834 (2100 bytes).
 */
void CSTGWaveSeqData::Initialize()
{
	unsigned char *base = (unsigned char *)this;
	for (unsigned int i = 0; i < 0x256; i++)
		CallVtableSlot7(base + i * 0xd14);
}

void CSetListBank::Initialize()
{
	unsigned char *base = (unsigned char *)this;
	for (unsigned int i = 0; i < 0x80; i++)
		CallVtableSlot7(base + i * 0x834);
}
