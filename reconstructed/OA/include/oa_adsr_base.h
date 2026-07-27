// SPDX-License-Identifier: GPL-2.0
#ifndef OA_ADSR_BASE_H
#define OA_ADSR_BASE_H

/*
 * oa_adsr_base.h  -  CSTGADSRBase, a per-voice-component AMS-modulatable
 * 4-stage (Attack/Decay/Sustain/Release) envelope-time parameter block.
 *
 * Ground truth: `CSTGADSRBase::*`, OA_real.ko `.text+0x1964a0`..
 * `.text+0x197bb0` (main cluster, contiguous) plus 20 trivial Get*
 * accessors and 3 destructor entry points, each in its own
 * `.text._ZN12CSTGADSRBaseXXX` COMDAT section (weak linkage --
 * confirmed via `readelf -sW`, real file offsets computed from each
 * section's own `sh_offset`, not from the main cluster's contiguous
 * addressing). Every body transcribed from `objdump -dr -M intel`
 * against `OA_real.ko` directly (not the Ghidra decompiler).
 *
 * 66 real methods total (manifest: 305 CSTGOrganModelPatch-tier
 * cluster survey, picked for being small, fully self-contained, and
 * genuinely DSP-parameter-math-dense -- see PROJECT_BRAIN status.md
 * for the pick rationale). 65 of the 66 are reconstructed here;
 * `InitVoice` is the one deliberate exception -- see its own
 * declaration comment below for why.
 *
 * === Real per-instance field layout (confirmed exhaustively: every
 * offset below is read or written by at least one Get, Update,
 * ToneAdjust, PrecomputeData or HandleCC method, cross-checked
 * against the constructor's own literal field-default writes) ===
 *
 *   +0x00  vtable ptr (manual placeholder, see _ZTV12CSTGADSRBase below)
 *   +0x04  int    _unconfirmedFlags   -- ctor sets 0, no reader found
 *                 in this cluster; real meaning not determined.
 *   +0x08  CSTGComponentSlotInfo*  _slotInfo -- ctor sets nullptr;
 *                 populated by an out-of-scope base-class method
 *                 (CSTGComponent::SetupComponentOffsets, confirmed via
 *                 the real vtable dump below). Read by nearly every
 *                 method in this class to locate this component's
 *                 per-voice working data.
 *   +0x0c  float  attackTime     (ctor default 0.05f)
 *   +0x10  float  decayTime      (ctor default 0.3f)
 *   +0x14  float  sustainLevel   (ctor default 1.0f)
 *   +0x18  float  releaseTime    (ctor default 0.3f)
 *   +0x1c  int    attackTimeAMSIntensity
 *   +0x20  int    attackTimeAMSIntensityAMSIntensity
 *   +0x24  int8_t attackTimeAMSSource
 *   +0x25  int8_t attackTimeAMSIntensityAMSSource
 *   +0x26  int    decayTimeAMSIntensity
 *   +0x2a  int    decayTimeAMSIntensityAMSIntensity
 *   +0x2e  int8_t decayTimeAMSSource
 *   +0x2f  int8_t decayTimeAMSIntensityAMSSource
 *   +0x30  int    sustainLevelAMSIntensity
 *   +0x34  int    sustainLevelAMSIntensityAMSIntensity
 *   +0x38  int8_t sustainLevelAMSSource
 *   +0x39  int8_t sustainLevelAMSIntensityAMSSource
 *   +0x3a  int    releaseTimeAMSIntensity
 *   +0x3e  int    releaseTimeAMSIntensityAMSIntensity
 *   +0x42  int8_t releaseTimeAMSSource
 *   +0x43  int8_t releaseTimeAMSIntensityAMSSource
 *   (total confirmed size 0x44 bytes; all 20 AMS int/int8 fields
 *   ctor-defaulted to 0, matching a "no modulation" initial state)
 *
 * === The canonical "headroom-aware bipolar blend" formula ===
 *
 * The SAME 30-ish-instruction x87 shape appears, byte-identical
 * modulo field offsets, in all of: the 4 Precompute*PlusCC methods,
 * all 4 cl-dispatched branches of HandleCC, the tail of all 4 main
 * Update*Time methods, and the tail of all 12 ToneAdjust* methods.
 * Confirmed via a native-execution harness (same methodology as
 * param_convertor.cpp's own Taper family, sec batch 55): a throwaway
 * `gcc -m32` program mmaps the real, fully self-contained (zero
 * relocations) `PrecomputeAttackTimePlusCC` machine code and calls it
 * natively against ~200 (B,I,x) sample triples, comparing against a
 * candidate closed-form formula. First hand-derivation had the two
 * `jbe`/`ja` branch directions swapped (an easy mistake -- x87
 * `fucomip` branch-sense reads backwards from normal intuition) and
 * failed every sample; the corrected formula below matches exactly:
 *
 *   float a = (I >= 0.0f) ? (B + I * (1.0f - B)) : (B * (1.0f + I));
 *   float result = (x >= 0.0f) ? (a + x * (1.0f - a)) : (a * (1.0f + x));
 *
 * i.e. positive I boosts the base B toward 1.0 proportional to
 * remaining headroom; negative I cuts it toward 0.0 proportional to B
 * itself; then x performs an identical bipolar blend of `a` toward
 * 1.0 (x>0) or 0.0 (x<0). See ApplyIntensityBlend() below.
 *
 * === STGADSRBasePrecomputed -- the per-voice, per-component working
 * copy every method above reads/writes (located via
 * `ctx.precomputedBaseOffset + slotInfo->precomputedSlotIndex`) ===
 *
 *   +0x00..+0x0c  float[4]  plusCC        -- {attack,decay,sustain,release}
 *                 CC/tone-adjust-modulated blend RESULT (PrecomputeData's
 *                 own initial fill point; Precompute*PlusCC/HandleCC/
 *                 ToneAdjust* all overwrite these too)
 *   +0x10..+0x1c  float[4]  raw           -- {attack,decay,sustain,release}
 *                 raw (un-blended) value most recently stored (by
 *                 PrecomputeData from the instance fields, or by
 *                 ToneAdjust* from a per-note override)
 *   +0x20..+0x2c  int[4]    amsIntensity  -- raw copy of the matching
 *                 instance AMSIntensity field (PrecomputeData only)
 *   +0x30..+0x3c  float[4]  intensitySlot -- the "I" operand HandleCC
 *                 and Precompute*PlusCC read for the blend formula
 *                 above; PrecomputeData zero-initializes all 4
 *                 (populated for real by code outside this cluster)
 *
 * === Real vtable (confirmed via `objdump -r -j .rodata._ZTV12CSTGADSRBase`)
 * ===
 * CSTGADSRBase is genuinely polymorphic in ground truth, inheriting
 * (through an out-of-scope, giant CSTGComponent/CSTGParamsOwner
 * framework this project does not model as real C++ base classes --
 * same "manual vtable byte array" treatment as CSTGFrontPanelMsgHandler/
 * CStartupFile/CSTGSequence elsewhere in this project) a 48-slot
 * vtable. Only 5 slots are CSTGADSRBase's OWN overrides -- InitVoice
 * (0x7c), InitAMSSourceAddresses (0x88), PrecomputeData (0xa0),
 * HandlesCC (0xa4), HandleCC (0xa8) -- everything else resolves to
 * CSTGComponent/CSTGParamsOwner methods or `__cxa_pure_virtual`, none
 * of which this project reconstructs here. Modeled as a real,
 * standalone (non-inheriting) class with an explicit `_vtablePtr`
 * member the constructor points at a zero-filled placeholder array of
 * the confirmed real byte size (0xc8) -- "install vs dispatch": ground
 * truth's own object genuinely carries this pointer, but nothing in
 * this reconstruction dispatches through it.
 *
 * `InitVoice(CSTGVoice&, CSTGVoiceInitialState&)` is the one method in
 * this cluster deliberately NOT reconstructed: its own body makes an
 * UNCONDITIONAL virtual call through THIS SAME vtable at slot 0x80
 * (`CSTGComponent::InitVoiceMonoLegato`, confirmed via the dump
 * above) partway through, before its own field-copy tail. A call
 * through the zero-filled placeholder vtable would jump to address 0
 * at runtime -- fine for "install only" slots nothing calls back
 * into, but InitVoice calling into ITS OWN class's placeholder vtable
 * is a genuine self-dispatch, so a host verify test that invoked it
 * would crash. Consistent with this project's own precedent for
 * exactly this shape of problem (oa_engine.h's CSTGEngine::
 * Initialize()/PreAudioTick(), left unimplemented rather than
 * mis-modeling ~40 manager classes just to make the body "compile") --
 * simply not included here rather than stubbed.
 */

#include "oa_global.h"
#include "oa_engine.h"

/*
 * Minimal, not-independently-named per-component descriptor object
 * CSTGADSRBase::_slotInfo (+0x08) points to. Real type/full layout
 * unknown; only the two int16 fields this cluster's own methods read
 * are modeled (see field comments). Populated by an out-of-scope base
 * method (CSTGComponent::SetupComponentOffsets); left null by
 * CSTGADSRBase's own constructor.
 */
struct CSTGComponentSlotInfo {
	unsigned char _unrecovered_head[4];	/* +0x00..+0x03, unconfirmed */
	short subRateBaseIndex;			/* +0x04, confirmed: InitAMSSourceAddresses
						 * combines this with
						 * CSTGVoiceModelManager::sInstance's own
						 * +0x4 table pointer to locate this
						 * component's per-voice quad-slot base. */
	unsigned char _gap[2];			/* +0x06..+0x07, unconfirmed */
	short precomputedSlotIndex;		/* +0x08, confirmed: PrecomputeData/HandleCC/
						 * Update*Time/ToneAdjust* all combine this
						 * with CSTGPatchMessageContext::
						 * precomputedBaseOffset to locate this
						 * component's own STGADSRBasePrecomputed
						 * instance. */
};

/*
 * Minimal, not-independently-named per-patch-message "context" object
 * every non-trivial method here takes by reference. Real type/full
 * layout unknown beyond the 4 fields this cluster's own methods read;
 * genuinely polymorphic in ground truth (HandleCC/the 4 main
 * Update*Time methods make a real virtual call through slot 0 -- a
 * bool-returning predicate of unconfirmed name/purpose, gating
 * whether the per-voice display-value recompute tail runs at all).
 */
struct CSTGPatchMessageContext {
	void *_vtablePtr;			/* +0x00, confirmed real (genuine dispatch
						 * target, unlike most placeholder vtables
						 * elsewhere in this project -- see
						 * CallDisplayPredicate() below) */
	unsigned char _unrecovered_a[0x14];	/* +0x04..+0x17, unconfirmed */
	int componentSlotIndex;		/* +0x18, confirmed: active-voice-list index */
	unsigned char _unrecovered_b[4];	/* +0x1c..+0x1f, unconfirmed */
	void *activeVoiceListTable;		/* +0x20, confirmed: base of a 12-byte-stride
						 * per-component active-voice-list-head array */
	void *paramConstantsTable;		/* +0x24, confirmed: base pointer for the
						 * per-parameter fixed "G" float constants
						 * PrecomputeData/Update*Time/ToneAdjust*
						 * read at large fixed offsets (e.g. +0x370
						 * for Attack -- see .cpp for the full table) */
	char *precomputedBaseOffset;		/* +0x28, confirmed: added to
						 * CSTGComponentSlotInfo::precomputedSlotIndex
						 * to locate this component's
						 * NOTE: real target ABI is a 32-bit int added
						 * to a 32-bit pointer (both 4 bytes on the
						 * real 32-bit target); modeled here as
						 * `char *` directly rather than `int` so this
						 * struct round-trips correctly on a 64-bit
						 * host verify build too (this struct's exact
						 * byte layout is not independently confirmed
						 * against ground truth beyond field order/
						 * meaning -- see file header). Arithmetically
						 * identical to `(char*)(int)ptr` on the real
						 * 32-bit target.
						 * STGADSRBasePrecomputed instance */
};

/*
 * Per-voice, per-component working copy of this class's 4 envelope-
 * stage parameters -- see the file header comment above for the full
 * confirmed field table.
 */
struct STGADSRBasePrecomputed {
	float plusCC[4];	/* +0x00, order: attack,decay,sustain,release */
	float raw[4];		/* +0x10 */
	int amsIntensity[4];	/* +0x20 */
	float intensitySlot[4];/* +0x30 */
};

/*
 * Per-voice quad/sub-rate AMS-source-address table CSTGADSRBase::
 * InitializeQuad populates defaults into. Confirmed size/shape only
 * for the first 0x10 bytes of each of 8 confirmed 0x20-stride slots
 * (4 pointers, all defaulted to the SAME shared "no source" address,
 * `(char*)CSTGGlobal::sInstance + 0x29c9fa0`); the 4 UpdateXxx
 * AMSIntensity (non-double) methods separately confirm real writes
 * land at `slot[0]+0x10`/`slot[2]+0x10`/`slot[4]+0x10`/`slot[6]+0x10`
 * (raw int, not a pointer) for Attack/Decay/Sustain/Release
 * respectively -- real meaning of the even/odd slot split and the
 * remaining bytes not independently determined; kept as an opaque
 * byte blob and addressed via raw offsets in the .cpp rather than
 * over-fitting a named-field layout this cluster can't fully justify.
 */
struct STGADSRBaseSubRateParams {
	unsigned char _unrecovered[0x100];
};

/*
 * Fixup table CSTGADSRBase::PrepareSubRateAddressFixupTable appends 8
 * entries to per call (one per AMS-source slot -- matches
 * InitAMSSourceAddresses's own 8 GetAMSSourceAddress calls). Confirmed
 * real via direct disassembly (own body, not a relocation): `entries`
 * is a plain array of `unsigned int` quad-slot indices, `count` is a
 * running write cursor advanced by exactly 8 per call.
 */
struct CSTGSubRateAddressFixupTable {
	unsigned int *entries;	/* +0x00 */
	unsigned short count;	/* +0x04 */
};

/*
 * Real vtable data (0xc8 bytes / 50 slots incl. offset-to-top + RTTI,
 * confirmed via `readelf -sW` against `_ZTV12CSTGADSRBase`).
 * Zero-filled placeholder -- see file header comment's "install vs
 * dispatch" note.
 */
extern "C" unsigned char _ZTV12CSTGADSRBase[0xc8];

class CSTGADSRBase {
public:
	CSTGADSRBase();
	~CSTGADSRBase();

	int _unconfirmedFlags;			/* +0x04 */
	CSTGComponentSlotInfo *_slotInfo;	/* +0x08 */
	float attackTime;			/* +0x0c */
	float decayTime;			/* +0x10 */
	float sustainLevel;			/* +0x14 */
	float releaseTime;			/* +0x18 */
	int attackTimeAMSIntensity;		/* +0x1c */
	int attackTimeAMSIntensityAMSIntensity;/* +0x20 */
	signed char attackTimeAMSSource;	/* +0x24 */
	signed char attackTimeAMSIntensityAMSSource; /* +0x25 */
	int decayTimeAMSIntensity;		/* +0x26 */
	int decayTimeAMSIntensityAMSIntensity;	/* +0x2a */
	signed char decayTimeAMSSource;	/* +0x2e */
	signed char decayTimeAMSIntensityAMSSource; /* +0x2f */
	int sustainLevelAMSIntensity;		/* +0x30 */
	int sustainLevelAMSIntensityAMSIntensity; /* +0x34 */
	signed char sustainLevelAMSSource;	/* +0x38 */
	signed char sustainLevelAMSIntensityAMSSource; /* +0x39 */
	int releaseTimeAMSIntensity;		/* +0x3a */
	int releaseTimeAMSIntensityAMSIntensity; /* +0x3e */
	signed char releaseTimeAMSSource;	/* +0x42 */
	signed char releaseTimeAMSIntensityAMSSource; /* +0x43 */

	/* ---- Virtual-slot overrides (see class header comment) ---- */
	bool HandlesCC(unsigned char cc) const;
	void InitAMSSourceAddresses(CSTGVoice &voice);
	void PrecomputeData(CSTGPatchMessageContext &ctx);
	void HandleCC(CSTGPatchMessageContext &ctx, unsigned char cc, const CSTGControllerValue &value);
	/* InitVoice(CSTGVoice&, CSTGVoiceInitialState&) deliberately not
	 * declared here -- see file header comment. */

	/* ---- Quad/sub-rate address-table setup ----
	 * Both confirmed effectively STATIC (`this` never referenced --
	 * neither body touches any CSTGADSRBase instance field; same
	 * "instance-less" treatment as USTGParamConvertor's own methods,
	 * oa_engine.h) -- explicit args occupy EAX/EDX directly rather
	 * than EDX/ECX behind an unused EAX `this`. */
	static void InitializeQuad(STGADSRBaseSubRateParams *params);
	static void PrepareSubRateAddressFixupTable(CSTGSubRateAddressFixupTable &table, unsigned long note);

	/* ---- Precompute*PlusCC family: canonical blend, direct call ---- */
	static void PrecomputeAttackTimePlusCC(STGADSRBasePrecomputed *out, float x);
	static void PrecomputeDecayTimePlusCC(STGADSRBasePrecomputed *out, float x);
	static void PrecomputeSustainLevelPlusCC(STGADSRBasePrecomputed *out, float x);
	static void PrecomputeReleaseTimePlusCC(STGADSRBasePrecomputed *out, float x);

	/* ---- Update* family: persist new value + recompute display ---- */
	void UpdateAttackTime(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateDecayTime(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateSustainLevel(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateReleaseTime(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);

	void UpdateAttackTimeAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateDecayTimeAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateSustainLevelAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateReleaseTimeAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);

	void UpdateAttackTimeAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateDecayTimeAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateSustainLevelAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateReleaseTimeAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);

	void UpdateAttackTimeAMSIntensityAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateDecayTimeAMSIntensityAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateSustainLevelAMSIntensityAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateReleaseTimeAMSIntensityAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);

	void UpdateAttackTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateDecayTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateSustainLevelAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateReleaseTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);

	/* ---- ToneAdjust* family: per-note working-copy override ---- */
	void ToneAdjustAttackTime(CSTGPatchMessageContext &ctx, STGConvertedParam &scale);
	void ToneAdjustDecayTime(CSTGPatchMessageContext &ctx, STGConvertedParam &scale);
	void ToneAdjustSustainLevel(CSTGPatchMessageContext &ctx, STGConvertedParam &scale);
	void ToneAdjustReleaseTime(CSTGPatchMessageContext &ctx, STGConvertedParam &scale);

	void ToneAdjustAttackTimeRelative(CSTGPatchMessageContext &ctx, STGConvertedParam &scale);
	void ToneAdjustDecayTimeRelative(CSTGPatchMessageContext &ctx, STGConvertedParam &scale);
	void ToneAdjustSustainLevelRelative(CSTGPatchMessageContext &ctx, STGConvertedParam &scale);
	void ToneAdjustReleaseTimeRelative(CSTGPatchMessageContext &ctx, STGConvertedParam &scale);

	void ToneAdjustAttackTimeAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &scale);
	void ToneAdjustDecayTimeAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &scale);
	void ToneAdjustSustainLevelAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &scale);
	void ToneAdjustReleaseTimeAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &scale);

	/* ---- Get* family: all 20 write CSTGParamsOwner::sValueGetterTemp
	 * and return a reference to it -- see that field's own comment
	 * (oa_global.h) for the confirmed shared-scratch mechanism. ---- */
	STGConvertedParam &GetAttackTime(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDecayTime(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSustainLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseTime(CSTGPatchMessageContext &ctx);

	STGConvertedParam &GetAttackTimeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDecayTimeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSustainLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseTimeAMSSource(CSTGPatchMessageContext &ctx);

	STGConvertedParam &GetAttackTimeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDecayTimeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSustainLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseTimeAMSIntensity(CSTGPatchMessageContext &ctx);

	STGConvertedParam &GetAttackTimeAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDecayTimeAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSustainLevelAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseTimeAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);

	STGConvertedParam &GetAttackTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDecayTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSustainLevelAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
};

#endif
