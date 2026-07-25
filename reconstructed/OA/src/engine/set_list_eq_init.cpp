// SPDX-License-Identifier: GPL-2.0
/*
 * set_list_eq_init.cpp  -  CSetListEQ::Initialize(unsigned int) (batch 59,
 * ground truth `.text+0x202320`, 360 bytes).
 *
 * Confirmed real, two independent parts:
 *
 * 1) A module-static, run-ONCE (guarded by a `.bss` bool -- ground truth
 *    `cmpb $0x0,<flag>` / `jne <skip>` / ... / `movb $0x1,<flag>` at the
 *    very end of the guarded block) 9-band peaking-EQ coefficient table
 *    build: for each of 9 fixed center frequencies (`kBandFreq[9]`, a
 *    confirmed real `.rodata` int array -- extracted directly via
 *    `readelf -x` on its own dedicated section,
 *    `.rodata._ZZN14USetListEQInfo11GetBandFreqEjE9kBandFreq`, independent
 *    of any call-site inference: `{63, 125, 250, 500, 1000, 2000, 4000,
 *    8000, 16000}` Hz -- a standard 9-band graphic-EQ ISO center-frequency
 *    ladder), calls the already-real `CSTGEQ::CalculatePeakingBeta(freq,
 *    1.0f, &omega)` (sec 10.178, eq_coefficients.cpp) and stores BOTH the
 *    `omega` byproduct and the function's own `beta` return value into a
 *    shared, module-static 9x2-float table (confirmed real: each
 *    `outOmega` pointer argument and the immediately-following
 *    `fstps` storing the return value are 4 bytes apart in ground truth's
 *    own `.bss` layout, one 8-byte `{omega, beta}` pair per band, 9*8=72
 *    bytes total -- independently confirmed against `CSetListEQ::
 *    SetBand()`'s own still-deferred body needing exactly this shape of
 *    per-band coefficient data, per that method's own header comment).
 *
 * 2) UNCONDITIONAL per-call instance setup (runs every time, not gated by
 *    the guard above): `this->+0x0 = count` (the same 0/1 double-buffer
 *    slot index `CSTGAudioInputMixer::Initialize`/`CSTGMasterLRMixer::
 *    Initialize` above receive, sec 10.150/batch 58), `this->+0x4` cached
 *    to a pointer into `CSTGAudioBusManager::sEffectThreadBusSets` at
 *    slot `count*120 + 12` -- BYTE-IDENTICAL formula and target slot to
 *    `CSTGMasterLRMixer::Initialize()`'s own confirmed `+0x14` field
 *    (batch 58, src/engine/audio_input_mixer.cpp) -- the same confirmed
 *    "current master bus" pointer, cached independently by three sibling
 *    classes rather than shared.
 */

#include "oa_global.h"
#include "oa_engine.h"

static unsigned int ToU32(void *p)
{
	return (unsigned int)(unsigned long)p;
}

/*
 * IntToFloat() -- x87 `fildl` (int-to-float load), matching ground
 * truth's own instruction exactly. Deliberately NOT a plain C `(float)`
 * cast: this kernel build is `-msoft-float` (no libgcc float-conversion
 * helpers linked into a freestanding module), so a plain cast silently
 * pulls in `__floatsisf` -- confirmed the hard way, caught by a real
 * `nm -u` regression (98 -> 99 unresolved symbols) against the .ko build
 * only, invisible to the host KAT since libgcc is trivially present
 * there. Same rationale as audio_input_mixer.cpp's own FMul/FAdd/
 * FLess/FLessEq primitives (sec 10.151).
 */
static inline float IntToFloat(int v)
{
	float result;
	__asm__ __volatile__("fildl %1\n\tfstps %0" : "=m" (result) : "m" (v));
	return result;
}

/* kBandFreq[9] -- confirmed real .rodata, see this file's own header
 * comment for the readelf -x derivation. */
static const int kBandFreq[9] = {
	63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000
};

/* Module-static 9-band {omega, beta} coefficient table + its own
 * confirmed real "already built" guard flag -- shared across every
 * CSetListEQ instance (matches ground truth's own single `.bss` table,
 * not per-instance storage). */
static bool s_bandTableBuilt;
static float s_bandOmega[9];
static float s_bandBeta[9];

void CSetListEQ::Initialize(unsigned int count)
{
	if (!s_bandTableBuilt) {
		for (int i = 0; i < 9; i++) {
			float freq = IntToFloat(kBandFreq[i]);
			s_bandBeta[i] = CSTGEQ::CalculatePeakingBeta(freq, 1.0f, &s_bandOmega[i]);
		}
		s_bandTableBuilt = true;
	}

	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)(base + 0x0) = count;
	unsigned int off = count * 120 * 0x80 + 12 * 0x80;
	*(unsigned int *)(base + 0x4) = ToU32(CSTGAudioBusManager::sEffectThreadBusSets + off);
}
