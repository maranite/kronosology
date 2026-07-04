// SPDX-License-Identifier: GPL-2.0
/*
 * lfo_stepseq_quad.cpp  -  CSTGLFOBase::InitializeQuad(STGLFOSubRateParams*)
 * / CSTGStepSeqBase::InitializeQuad(STGStepSeqSubRateParams*), the two
 * confirmed-real dependencies sec 10.59's CSTGCommonLFO/CSTGCommonStepSeq::
 * Initialize() surfaced (one call per 32-block pool entry).
 *
 * Ground-truthed via readelf+objdump (`-j .text`) against OA_real.ko:
 *   CSTGLFOBase::InitializeQuad(...)      .text+0x130fb0, 199 bytes
 *   CSTGStepSeqBase::InitializeQuad(...)  .text+0x13a250, 181 bytes
 *
 * Both are pure field-population functions (no branches at all in
 * either function -- confirmed via a full instruction scan, not just
 * a spot check) writing fixed offsets from `this` (regparm arg1=eax)
 * to three shared singletons' own fixed sub-addresses:
 *   - `(char*)CSTGGlobal::sInstance + 0x29c9fa0`      (both functions,
 *     the SAME literal offset -- a real shared "controller RT data"-
 *     shaped region this project has seen referenced from other
 *     confirmed offsets before, e.g. CSTGControllerRTData at
 *     CSTGGlobal+0x10, sec 10.55/10.56; this specific sub-offset is
 *     not independently identified beyond its confirmed existence)
 *   - `(char*)CSTGLFOTables::sInstance + 0x408`        (LFO only)
 *   - `(char*)CSTGMIDIClockSync::sInstance + 0xa0`      (LFO only)
 *
 * CSTGLFOBase::InitializeQuad's own writes decompose cleanly into a
 * real double loop once regrouped by column (confirmed via exact
 * instruction-count/offset matching, not assumed): for j in 0..3, for
 * i in 0..4, `this[0x10 + i*0x20 + j*4] = ctrlRTData`; then per j:
 * `this[0x240+j*4] = 0`, `this[0x160+j*4] = midiClockSync`,
 * `this[0x190+j*4] = lfoTables`.
 *
 * CSTGStepSeqBase::InitializeQuad's own writes decompose into a
 * single loop, i in 0..3: `this[0x40+i*4] = 0`,
 * `this[0x60+i*4] = this[0x80+i*4] = this[0xb0+i*4] = ctrlRTData`,
 * `this[0xc0+i*4] = this[0xf0+i*4] = 0`.
 */

#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"

/* Storage for two singletons whose OWN constructors aren't
 * reconstructed yet (CSTGLFOTables::CSTGLFOTables()/CSTGMIDIClockSync::
 * CSTGMIDIClockSync() remain confirmed-real, deliberately deferred
 * externs) -- defined here since this is the first file in this
 * project to actually consume them. Whoever reconstructs those
 * constructors next should set `sInstance = this` there, matching
 * every other singleton in this codebase; until then these remain
 * NULL on a real target. */
CSTGLFOTables *CSTGLFOTables::sInstance;
CSTGMIDIClockSync *CSTGMIDIClockSync::sInstance;

/* Host/target pointer-width fix (this project's established pattern,
 * e.g. engine_init.cpp's TSTGArrayManager<T>): the real target stores
 * these as plain 32-bit pointer fields, but a native host pointer is
 * 8 bytes on a 64-bit build -- storing one directly into a 4-byte-
 * spaced target field would clobber its neighbor. Truncate explicitly. */
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

void CSTGLFOBase::InitializeQuad(STGLFOSubRateParams *quad)
{
	unsigned char *p = (unsigned char *)quad;
	unsigned int ctrlRTData = ToU32((unsigned char *)CSTGGlobal::sInstance + 0x29c9fa0);
	unsigned int lfoTables = ToU32((unsigned char *)CSTGLFOTables::sInstance + 0x408);
	unsigned int midiClockSync = ToU32((unsigned char *)CSTGMIDIClockSync::sInstance + 0xa0);

	for (unsigned int j = 0; j < 4; j++) {
		for (unsigned int i = 0; i < 5; i++)
			*(unsigned int *)(p + 0x10 + i * 0x20 + j * 4) = ctrlRTData;
		*(unsigned int *)(p + 0x240 + j * 4) = 0;
		*(unsigned int *)(p + 0x160 + j * 4) = midiClockSync;
		*(unsigned int *)(p + 0x190 + j * 4) = lfoTables;
	}
}

void CSTGStepSeqBase::InitializeQuad(STGStepSeqSubRateParams *quad)
{
	unsigned char *p = (unsigned char *)quad;
	unsigned int ctrlRTData = ToU32((unsigned char *)CSTGGlobal::sInstance + 0x29c9fa0);

	for (unsigned int i = 0; i < 4; i++) {
		*(unsigned int *)(p + 0x40 + i * 4) = 0;
		*(unsigned int *)(p + 0x60 + i * 4) = ctrlRTData;
		*(unsigned int *)(p + 0x80 + i * 4) = ctrlRTData;
		*(unsigned int *)(p + 0xb0 + i * 4) = ctrlRTData;
		*(unsigned int *)(p + 0xc0 + i * 4) = 0;
		*(unsigned int *)(p + 0xf0 + i * 4) = 0;
	}
}
