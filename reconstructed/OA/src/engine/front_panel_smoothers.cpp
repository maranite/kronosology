// SPDX-License-Identifier: GPL-2.0
/*
 * front_panel_smoothers.cpp  -  CSTGFrontPanelSmoothers::CSTGFrontPanelSmoothers()
 * (`.text+0x1e850`, sec 10.153).
 *
 * Deliberately a separate translation unit from global.cpp/engine_init.cpp:
 * matches this project's established per-unit convention for any class
 * whose ctor allocates its own CSTGBankMemory pool (see smoother_init.cpp,
 * wave_seq_manager.cpp, etc.) -- test_engine_init.cpp's own MOCK_CTOR_ONLY
 * counter for this symbol is removed as part of this promotion (its only
 * use there was the ctor-call-count assertion, not a load-bearing state
 * mock -- confirmed via `grep -l CSTGFrontPanelSmoothers verify/`,
 * only that one file references it).
 *
 * See oa_engine_init.h's own header comment for the full confirmed field
 * layout and the "4-way interleaved" addressing scheme both allocated
 * buffers share.
 */

#include "oa_engine_init.h"
#include "oa_bank_memory.h"

static unsigned int ToU32(void *p)
{
	return (unsigned int)(unsigned long)p;
}

CSTGFrontPanelSmoothers *CSTGFrontPanelSmoothers::sInstance;

CSTGFrontPanelSmoothers::CSTGFrontPanelSmoothers()
{
	unsigned char *base = (unsigned char *)this;
	unsigned int i;

	/* Zero the two trailing 12-byte-stride arrays first (confirmed
	 * real instruction order -- these happen before either
	 * AllocAligned call). */
	for (i = 0; i < 63; i++) {
		unsigned char *e = base + 0x518 + i * 12;
		*(unsigned int *)(e + 0x0) = 0;
		*(unsigned int *)(e + 0x4) = 0;
		*(unsigned int *)(e + 0x8) = 0;
	}
	for (i = 0; i < 99; i++) {
		unsigned char *e = base + 0x80c + i * 12;
		*(unsigned int *)(e + 0x0) = 0;
		*(unsigned int *)(e + 0x4) = 0;
		*(unsigned int *)(e + 0x8) = 0;
	}

	/* sInstance is confirmed set BEFORE the first AllocAligned call --
	 * a real, harmless ordering quirk, preserved for faithfulness. */
	sInstance = this;

	unsigned char *knobBuf = CSTGBankMemory::AllocAligned(0x800, 0x10);
	knobSmootherBuf = ToU32(knobBuf);
	for (i = 0; i < 0x800; i++)
		knobBuf[i] = 0;

	unsigned char *eqBuf = CSTGBankMemory::AllocAligned(0xc80, 0x10);
	eqSmootherBuf = ToU32(eqBuf);
	for (i = 0; i < 0xc80; i++)
		eqBuf[i] = 0;

	/* 4-way interleaved re-init: element i lives at buf + (i>>2)*0x80 +
	 * (i&3)*4 -- confirmed via direct disassembly (see header comment).
	 * The knob buffer touches +0x60/+0x70 per element; the EQ buffer
	 * (fewer confirmed fields) does not -- a real, confirmed asymmetry
	 * between the two loops, not a transcription gap. */
	for (i = 0; i < 63; i++) {
		unsigned char *e = knobBuf + (i >> 2) * 0x80 + (i & 3) * 4;
		*(unsigned int *)(e + 0x00) = 0;
		*(unsigned int *)(e + 0x10) = 0;
		*(unsigned int *)(e + 0x20) = 0;
		*(unsigned int *)(e + 0x30) = 0;
		*(unsigned int *)(e + 0x40) = 0;
		*(unsigned int *)(e + 0x50) = 0xffffffff;
		*(unsigned int *)(e + 0x60) = 0;
		*(unsigned int *)(e + 0x70) = 0;
	}
	for (i = 0; i < 99; i++) {
		unsigned char *e = eqBuf + (i >> 2) * 0x80 + (i & 3) * 4;
		*(unsigned int *)(e + 0x00) = 0;
		*(unsigned int *)(e + 0x10) = 0;
		*(unsigned int *)(e + 0x20) = 0;
		*(unsigned int *)(e + 0x30) = 0;
		*(unsigned int *)(e + 0x40) = 0;
		*(unsigned int *)(e + 0x50) = 0xffffffff;
	}

	for (i = 0; i < 0x1f8; i++)
		_unrecovered1[i] = 0;
	for (i = 0; i < 0x318; i++)
		_unrecovered2[i] = 0;
}
