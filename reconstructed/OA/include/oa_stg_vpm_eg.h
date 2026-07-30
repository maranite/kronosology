// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_VPM_EG_H
#define OA_STG_VPM_EG_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_vpm_eg.h  -  CSTGVPMEG's value-getter family: all 5 real
 * weak-symbol ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGVPMEG is the VPM engine's own envelope-generator patch component
 * -- confirmed genuinely fresh via a word-boundary grep before starting,
 * no pre-existing struct or ctor anywhere in this project.
 *
 * Dialect: mixed. The AMS1LevelModSource and AMS1TimeModSource fields
 * are plain fixed-K signed bytes read directly off this -- despite the
 * "AMS1" naming implying a runtime-selected slot alongside their own
 * Intensity siblings, they are NOT ctx-indexed. The matching
 * *ModIntensity siblings ARE ctx-indexed: `mov edx,[edx+0x4]` with no
 * lea premultiply at all, straight into a SIB-scaled field load
 * `[eax+edx*4+K]` -- effective stride 4, same bare-stride-4 shape as
 * CSTGMultiFilter2Pole's own GetLFOIntensity/GetLFOJSminusYIntensity, no
 * new decoder shape needed. This is the first class in the family where
 * only the Intensity half of a Source/Intensity AMS pair is ctx-indexed
 * while the Source half stays fixed -- every prior class with this bare
 * stride-4 shape -- CSTGMultiFilter2Pole and CSTGEG -- had BOTH halves
 * of each pair ctx-indexed together.
 *
 * GetTriggerAtNoteOn uses the family's established mask-only single-bit
 * bitfield shape -- no shift instruction, bit 0 -- single-write only.
 *
 * Field-shape summary:
 *   - Plain 8-bit signed field: single-writes value only -- the
 *     AMS1LevelModSource/AMS1TimeModSource pair.
 *   - ctx-indexed 32-bit field, bare stride-4 SIB scale, no lea
 *     premultiply: dual-writes value and displayValue -- the
 *     AMS1LevelModIntensity/AMS1TimeModIntensity pair.
 *   - Mask-only single-bit boolean, bit 0: single-writes value only --
 *     TriggerAtNoteOn.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGVPMEG {
	STGConvertedParam &GetAMS1LevelModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMS1LevelModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMS1TimeModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMS1TimeModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTriggerAtNoteOn(CSTGPatchMessageContext &ctx);

	/* .text+0x1b80f0..0x1b8130, round 75 -- the matching Update* writer
	 * half of the 5 getters above, same field offsets (cross-checked
	 * against the already-confirmed getters, not a fresh derivation).
	 * See stg_vpm_eg_updaters.cpp for the per-method shape notes. */
	void UpdateAMS1LevelModSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateAMS1LevelModIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateAMS1TimeModSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateAMS1TimeModIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateTriggerAtNoteOn(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);

	/* .text+0x5b89f0..0x5b8a80, round 76 -- boilerplate component-
	 * registration accessors (GetId/GetNumParams/GetParamDescriptors/
	 * GetMessageHandlers/GetValueGetters, same shape already established
	 * for every other STG component class, e.g. CSTGLFO/oa_lfo.h) plus 3
	 * more real, unambiguous AMS accessors. `GetName()` (own address
	 * 0x5b8a00) and `GetAMSLevelModIntensity(unsigned char)` (own address
	 * 0x5b8a80, longdouble/x87 return) deliberately left pending this
	 * round -- GetName's target string address didn't resolve cleanly in
	 * this export's strings.csv (falls inside a `.rodata.cst8` constant-
	 * pool region per objdump -h, not the expected string section; not
	 * worth guessing), and the Intensity accessor needs more care for its
	 * x87 return convention than this round's other items. */
	static int GetId();
	static int GetNumParams();
	static const void *GetParamDescriptors();
	static const void *GetMessageHandlers();
	static const void *GetValueGetters();

	/* .text+0x5b8a50, 8 bytes. Bit 0 of the same TriggerAtNoteOn field
	 * GetTriggerAtNoteOn()/UpdateTriggerAtNoteOn() already use (+0x4f) --
	 * `param_1` (the real 2nd argument) confirmed unused. */
	bool TriggersAtNoteOn(int unused);

	/* .text+0x5b8a60, 7 bytes. Real signature is a single byte argument
	 * (an AMS-source index) -- Ghidra mis-split it as 2 params
	 * (`undefined4 param_1` unused, `byte param_2` unread); the real body
	 * reads DL, the LOW BYTE OF THE FIRST real argument (EDX per this
	 * project's regparm(3) convention), not ECX/param_2. */
	bool StateHasLevelAMS(unsigned char index);

	/* .text+0x5b8a70, 5 bytes. Same "index argument genuinely ignored"
	 * shape as UpdateAMS1LevelModSource's own header note above -- this
	 * class only has ONE shared Level-Mod-Source field (+0x3e), not a
	 * per-index array, so any index value returns the same field. */
	int GetAMSLevelModSource(unsigned char index);

	/* .text+0x5b8a90, round 77 -- same "index ignored, one shared field"
	 * shape as GetAMSLevelModSource() above, this time reading the
	 * AMS1TimeModSource field (+0x2d). */
	int GetAMSTimeModSource(unsigned char index);

	/* .text+0x5b8d20 (D1) / 0x5b8d30 (D0), round 77 -- byte-identical
	 * real bodies (confirmed both ground-truth addresses decompile
	 * identically): reset vptr to the shared, install-only
	 * `PTR__CSTGParamsOwner_006c04a8` placeholder, no free()/
	 * HAL_DisableInterrupts() in either variant -- same "opaque
	 * placeholder, no real vtable pointer needed, both D1/D0 collapse to
	 * one body" treatment already established for CSTGKeyTrack/
	 * CSTGPatch/CSTGMultibandDelay/CSTGProgramModeDrumTrackSlot/
	 * CSTGWaveSequence (this exact family of vtable-install-only
	 * classes). */
	~CSTGVPMEG();
};

#endif
