// SPDX-License-Identifier: GPL-2.0
/*
 * sequence_ctor.cpp  -  CSTGSequence::CSTGSequence() (sec 10.153,
 * `.text+0xcbfd0`, 546 bytes).
 *
 * Deliberately a separate translation unit from global.cpp: the ctor's
 * own mock is only referenced by test_global_ctor.cpp (confirmed via
 * `grep -l CSTGSequence::CSTGSequence verify/`), matching this project's
 * established "keep a newly-promoted ctor out of any TU whose test mocks
 * depend on it staying empty" convention.
 *
 * See oa_global.h's own header comment on CSTGSequence for the full
 * confirmed field layout (base CSTGCombi ctor, own vtable install, 16x
 * CSTGHDRTrack sub-objects at a 0x2c stride, then one differently-typed
 * CSTGMetronomeSettings slot at the same stride).
 */

#include "oa_global.h"

/* Sized to the real confirmed 0x9c/0x60/0x60-byte vtables (nm -CS /
 * objdump -sr), fixed batch 55 (sec 10.230) -- see oa_global.h's own
 * header comment on these three symbols for why (were previously an
 * undersized 12-byte placeholder each, the proximate cause of the
 * sec 10.229 hand-off crash). */
unsigned char _ZTV12CSTGSequence[0x9c];
unsigned char _ZTV12CSTGHDRTrack[0x60];
unsigned char _ZTV21CSTGMetronomeSettings[0x60];

static unsigned int ToU32(void *p)
{
	return (unsigned int)(unsigned long)p;
}

CSTGSequence::CSTGSequence() : CSTGCombi()
{
	unsigned char *base = (unsigned char *)this;

	*(unsigned int *)base = ToU32(_ZTV12CSTGSequence + 8);

	for (unsigned int i = 0; i < 16; i++) {
		unsigned char *track = base + 0x19e7 + i * 0x2c;
		*(unsigned int *)track = ToU32(_ZTV12CSTGHDRTrack + 8);
		track[0x4] = 0;
		track[0x5] = 0;
		track[0x6] = 0;
	}

	unsigned char *metronome = base + 0x19e7 + 16 * 0x2c; /* == +0x1ca7 */
	*(unsigned int *)metronome = ToU32(_ZTV21CSTGMetronomeSettings + 8);
	metronome[0x4] = 0;
}
