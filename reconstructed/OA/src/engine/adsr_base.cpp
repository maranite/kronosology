// SPDX-License-Identifier: GPL-2.0
/*
 * adsr_base.cpp  -  CSTGADSRBase (65 of 66 real methods; see
 * include/oa_adsr_base.h for the full ground-truth writeup, field
 * layout table, and the deliberately-deferred InitVoice explanation).
 */

#include "oa_adsr_base.h"

extern "C" unsigned char _ZTV12CSTGADSRBase[0xc8] = { 0 };
STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

namespace {

/*
 * The canonical "headroom-aware bipolar blend" shared by every
 * Precompute*PlusCC method, every HandleCC branch, every ToneAdjust*
 * method (base, intensity, x order), and every Update*Time main
 * method (base, x=G, intensity=existing-slot order -- see that
 * family's own comment below for why the last two args are swapped
 * there) -- see oa_adsr_base.h's file header for the native-harness
 * verification that pinned this formula down.
 */
inline float ApplyIntensityBlend(float base, float intensity, float x)
{
	float a = (intensity >= 0.0f) ? (base + intensity * (1.0f - base))
	                               : (base * (1.0f + intensity));
	return (x >= 0.0f) ? (a + x * (1.0f - a)) : (a * (1.0f + x));
}

/* Per-parameter fixed "G" float constant offsets within
 * CSTGPatchMessageContext::paramConstantsTable -- confirmed via
 * PrecomputeData/UpdateAttackTime.../ToneAdjustAttackTime... all
 * reading the SAME offset for the same parameter. */
inline float AttackG(const CSTGPatchMessageContext &ctx)
{
	return *reinterpret_cast<const float *>(
		static_cast<const char *>(ctx.paramConstantsTable) + 0x370);
}
inline float DecayG(const CSTGPatchMessageContext &ctx)
{
	return *reinterpret_cast<const float *>(
		static_cast<const char *>(ctx.paramConstantsTable) + 0x388);
}
inline float SustainG(const CSTGPatchMessageContext &ctx)
{
	return *reinterpret_cast<const float *>(
		static_cast<const char *>(ctx.paramConstantsTable) + 0x34c);
}
inline float ReleaseG(const CSTGPatchMessageContext &ctx)
{
	return *reinterpret_cast<const float *>(
		static_cast<const char *>(ctx.paramConstantsTable) + 0x364);
}

/* &precomp->plusCC[0] for THIS component instance within ctx -- the
 * `ctx.precomputedBaseOffset + this->_slotInfo->precomputedSlotIndex`
 * pointer chase every non-trivial method in this file performs. */
inline STGADSRBasePrecomputed *LocatePrecomputed(const CSTGADSRBase *self, const CSTGPatchMessageContext &ctx)
{
	return reinterpret_cast<STGADSRBasePrecomputed *>(
		ctx.precomputedBaseOffset + self->_slotInfo->precomputedSlotIndex);
}

/* Raw 32-bit reinterpret, matching this project's established "raw
 * bytes over asserted semantics" convention (oa_global.h). */
inline int AsBits(float f)
{
	int i;
	__builtin_memcpy(&i, &f, 4);
	return i;
}
inline float AsFloatBits(int i)
{
	float f;
	__builtin_memcpy(&f, &i, 4);
	return f;
}

/* Real STGConvertedParam field at +0x14, bit0: confirmed real via
 * ToneAdjustAttackTime's own `test BYTE PTR [ecx+0x14],0x1` -- set
 * means "ignore the passed-in scale.value and reaffirm the CURRENT
 * instance field instead" (falls within STGConvertedParam's existing
 * `_unrecovered_b[0x14..0x17]` gap, oa_global.h -- only bit0 of the
 * first byte is confirmed here, rest of the gap still unknown). */
inline bool WantsCurrentValue(const STGConvertedParam &p)
{
	return reinterpret_cast<const unsigned char *>(&p)[0x14] & 1;
}

/*
 * HandlesCC's real lookup table, `.rodata+0x7df68`, 6 bytes covering
 * cc in [0x46..0x4b]. Confirmed via `readelf -x .rodata`; matches
 * exactly the 4 controllers HandleCC itself dispatches (0x46, 0x48,
 * 0x49, 0x4b -- true) vs the 2 it silently ignores (0x47, 0x4a --
 * false).
 */
const bool kHandlesCCTable[6] = { true, false, true, true, false, true };

/*
 * Shared active-voice-list walk. `ctx.activeVoiceListTable
 * [ctx.componentSlotIndex]` is a real intrusive singly-linked list --
 * head at `table + index*12 + 0x44`, next pointer at each node's own
 * `+0x0`, live CSTGVoice* at each node's own `+0x8`. For each live
 * voice, computes `perVoiceQuadSlot` (matches InitAMSSourceAddresses'
 * own per-voice pointer exactly: `quadTableBase + slotInfo->
 * subRateBaseIndex + ((note&3) + (note>>2)*0xcc0) * 4`) and stores
 * `value` at `perVoiceQuadSlot + offset` -- either a raw int (plain
 * function pointer arithmetic result) or, when `amsSource != nullptr`,
 * re-resolves `voice->GetAMSSourceAddress(*amsSource)` per voice
 * instead of using a precomputed value (matches the 8 AMSSource-family
 * methods, which must re-resolve per-voice, not just broadcast one
 * shared value). No lambdas/templates -- kept as plain functions to
 * match this project's established kernel-target-compiled style.
 * Confirmed byte-identical list-walk shape across all 8 UpdateXxx
 * (AMSIntensityAMS)Source methods, both UpdateXxxAMSIntensity
 * families, and all 4 ToneAdjustXxxAMSIntensity methods (16 real
 * bodies total).
 */
void PropagateRawInt(const CSTGADSRBase *self, const CSTGPatchMessageContext &ctx, int offset, int value)
{
	char *table = static_cast<char *>(ctx.activeVoiceListTable);
	void *voiceEntry = nullptr;
	if (table) {
		void **headSlot = reinterpret_cast<void **>(table + ctx.componentSlotIndex * 12 + 0x44);
		voiceEntry = *headSlot;
	}
	/* else: real code falls back to the always-empty
	 * CSTGPatchMessageContext::sEmptyActiveVoiceList sentinel --
	 * equivalent here to simply not entering the loop. */
	if (!voiceEntry)
		return;

	/* Confirmed real: ground truth only computes the quad-table base
	 * (dereferences CSTGVoiceModelManager::sInstance and this->
	 * _slotInfo) AFTER confirming the list is non-empty -- an early
	 * `test esi,esi; je done` guards it in every real body. */
	CSTGVoiceModelManager *vmm = CSTGVoiceModelManager::sInstance;
	char *quadTableBase = *reinterpret_cast<char **>(reinterpret_cast<char *>(vmm) + 4)
		+ self->_slotInfo->subRateBaseIndex;

	while (voiceEntry) {
		CSTGVoice *voice = *reinterpret_cast<CSTGVoice **>(static_cast<char *>(voiceEntry) + 8);
		unsigned short note = *reinterpret_cast<unsigned short *>(
			reinterpret_cast<char *>(voice) + 4);
		unsigned int idx = (note & 3) + (note >> 2) * 0xcc0;
		char *quadSlot = quadTableBase + idx * 4;
		*reinterpret_cast<int *>(quadSlot + offset) = value;

		voiceEntry = *reinterpret_cast<void **>(voiceEntry); /* next */
	}
}

void PropagateAMSSourceAddress(const CSTGADSRBase *self, const CSTGPatchMessageContext &ctx, int offset, int amsSource)
{
	char *table = static_cast<char *>(ctx.activeVoiceListTable);
	void *voiceEntry = nullptr;
	if (table) {
		void **headSlot = reinterpret_cast<void **>(table + ctx.componentSlotIndex * 12 + 0x44);
		voiceEntry = *headSlot;
	}

	CSTGVoiceModelManager *vmm = CSTGVoiceModelManager::sInstance;
	char *quadTableBase = *reinterpret_cast<char **>(reinterpret_cast<char *>(vmm) + 4)
		+ self->_slotInfo->subRateBaseIndex;

	while (voiceEntry) {
		CSTGVoice *voice = *reinterpret_cast<CSTGVoice **>(static_cast<char *>(voiceEntry) + 8);
		unsigned short note = *reinterpret_cast<unsigned short *>(
			reinterpret_cast<char *>(voice) + 4);
		unsigned int idx = (note & 3) + (note >> 2) * 0xcc0;
		char *quadSlot = quadTableBase + idx * 4;
		*reinterpret_cast<void **>(quadSlot + offset) = voice->GetAMSSourceAddress(amsSource);

		voiceEntry = *reinterpret_cast<void **>(voiceEntry); /* next */
	}
}

} // namespace

/* ==================================================================
 * Construction / destruction
 * ================================================================== */

CSTGADSRBase::CSTGADSRBase()
	: _unconfirmedFlags(0), _slotInfo(nullptr),
	  attackTime(0.05f), decayTime(0.3f), sustainLevel(1.0f), releaseTime(0.3f),
	  attackTimeAMSIntensity(0), attackTimeAMSIntensityAMSIntensity(0),
	  attackTimeAMSSource(0), attackTimeAMSIntensityAMSSource(0),
	  decayTimeAMSIntensity(0), decayTimeAMSIntensityAMSIntensity(0),
	  decayTimeAMSSource(0), decayTimeAMSIntensityAMSSource(0),
	  sustainLevelAMSIntensity(0), sustainLevelAMSIntensityAMSIntensity(0),
	  sustainLevelAMSSource(0), sustainLevelAMSIntensityAMSSource(0),
	  releaseTimeAMSIntensity(0), releaseTimeAMSIntensityAMSIntensity(0),
	  releaseTimeAMSSource(0), releaseTimeAMSIntensityAMSSource(0)
{
	*reinterpret_cast<void **>(this) = _ZTV12CSTGADSRBase + 8;
}

/* D2 (base object dtor) and D1 (complete object dtor) share ONE real
 * body -- confirmed: identical `.text._ZN12CSTGADSRBaseD[12]Ev` COMDAT
 * section. D0, the deleting dtor, is a separate, trivial body doing
 * only the same vtable-ptr reset; no `operator delete` call was ever
 * confirmed. Neither touches any other field. */
CSTGADSRBase::~CSTGADSRBase()
{
	*reinterpret_cast<void **>(this) = _ZTV12CSTGADSRBase + 8;
}

/*
 * D0Ev (deleting destructor) -- real, separate Itanium-ABI symbol
 * ground truth carries (confirmed via `readelf -sW`) because the real
 * class is genuinely polymorphic with an accessible virtual
 * destructor slot. This project's own class deliberately has no
 * `virtual` destructor (manual placeholder-vtable approach, see file
 * header), so the compiler has no reason to synthesize this exact
 * symbol itself -- reproduced here as a free function forced onto the
 * real mangled name via `extern "C"`, matching D0's own confirmed
 * real body exactly (identical single vtable-ptr reset, no
 * `operator delete` call ever confirmed in ground truth either). Not
 * called from anywhere in this reconstruction; exists purely so a
 * symbol-table diff against ground truth comes back byte-exact.
 */
extern "C" void _ZN12CSTGADSRBaseD0Ev(void *self)
{
	*reinterpret_cast<void **>(self) = _ZTV12CSTGADSRBase + 8;
}

/* ==================================================================
 * Virtual-slot overrides
 * ================================================================== */

bool CSTGADSRBase::HandlesCC(unsigned char cc) const
{
	int idx = cc - 0x46;
	if (static_cast<unsigned>(idx) > 5)
		return false;
	return kHandlesCCTable[idx];
}

void CSTGADSRBase::InitAMSSourceAddresses(CSTGVoice &voice)
{
	CSTGVoiceModelManager *vmm = CSTGVoiceModelManager::sInstance;
	char *quadTableBase = *reinterpret_cast<char **>(reinterpret_cast<char *>(vmm) + 4)
		+ _slotInfo->subRateBaseIndex;

	unsigned short note = *reinterpret_cast<unsigned short *>(
		reinterpret_cast<char *>(&voice) + 4);
	unsigned int idx = (note & 3) + (note >> 2) * 0xcc0;
	char *quadSlot = quadTableBase + idx * 4;

	/* 8 GetAMSSourceAddress calls, one per AMS-source-carrying field --
	 * order and target offsets confirmed via direct disassembly. */
	*reinterpret_cast<void **>(quadSlot + 0x00) = voice.GetAMSSourceAddress(attackTimeAMSSource);
	*reinterpret_cast<void **>(quadSlot + 0x20) = voice.GetAMSSourceAddress(attackTimeAMSIntensityAMSSource);
	*reinterpret_cast<void **>(quadSlot + 0x40) = voice.GetAMSSourceAddress(decayTimeAMSSource);
	*reinterpret_cast<void **>(quadSlot + 0x60) = voice.GetAMSSourceAddress(decayTimeAMSIntensityAMSSource);
	*reinterpret_cast<void **>(quadSlot + 0x80) = voice.GetAMSSourceAddress(sustainLevelAMSSource);
	*reinterpret_cast<void **>(quadSlot + 0xa0) = voice.GetAMSSourceAddress(sustainLevelAMSIntensityAMSSource);
	*reinterpret_cast<void **>(quadSlot + 0xc0) = voice.GetAMSSourceAddress(releaseTimeAMSSource);
	*reinterpret_cast<void **>(quadSlot + 0xe0) = voice.GetAMSSourceAddress(releaseTimeAMSIntensityAMSSource);
}

/*
 * Fills a fresh STGADSRBasePrecomputed from this instance's own
 * persisted fields: raw values copied verbatim, AMS-intensity fields
 * copied verbatim (raw int mirror), the 4 "intensitySlot" scratch
 * floats zeroed, and plusCC seeded via the canonical blend with I
 * implicitly 0 (ground truth achieves this by reusing an already-on-
 * stack fldz 0.0 rather than a fresh per-param I load -- confirmed via
 * a full by-hand x87 stack trace; mathematically and behaviorally
 * identical to calling ApplyIntensityBlend with intensity=0, used
 * here for clarity).
 */
void CSTGADSRBase::PrecomputeData(CSTGPatchMessageContext &ctx)
{
	STGADSRBasePrecomputed *p = LocatePrecomputed(this, ctx);

	p->raw[0] = attackTime;
	p->raw[1] = decayTime;
	p->raw[2] = sustainLevel;
	p->raw[3] = releaseTime;

	p->amsIntensity[0] = attackTimeAMSIntensity;
	p->amsIntensity[1] = decayTimeAMSIntensity;
	p->amsIntensity[2] = sustainLevelAMSIntensity;
	p->amsIntensity[3] = releaseTimeAMSIntensity;

	p->intensitySlot[0] = 0.0f;
	p->intensitySlot[1] = 0.0f;
	p->intensitySlot[2] = 0.0f;
	p->intensitySlot[3] = 0.0f;

	p->plusCC[0] = ApplyIntensityBlend(attackTime, 0.0f, AttackG(ctx));
	p->plusCC[1] = ApplyIntensityBlend(decayTime, 0.0f, DecayG(ctx));
	p->plusCC[2] = ApplyIntensityBlend(sustainLevel, 0.0f, SustainG(ctx));
	p->plusCC[3] = ApplyIntensityBlend(releaseTime, 0.0f, ReleaseG(ctx));
}

/*
 * Live MIDI Sound Controller dispatch: cc 0x49/0x4b/0x46/0x48 (GM
 * "Sound Controller" numbers -- 0x49=73=Attack Time, 0x4b=75=Decay
 * Time GM-standard; 0x46=70 is repurposed here for Sustain Level,
 * which GM leaves undefined) recomputes that one parameter's plusCC
 * slot using the canonical blend with base equal to raw, unchanged, and intensity equal to
 * the existing, unchanged intensitySlot value; x is the live CC's own
 * converted float value. Confirmed real cc->param mapping via each
 * branch's own field-offset reads.
 */
void CSTGADSRBase::HandleCC(CSTGPatchMessageContext &ctx, unsigned char cc, const CSTGControllerValue &value)
{
	STGADSRBasePrecomputed *p = LocatePrecomputed(this, ctx);
	float x = value.field4;

	switch (cc) {
	case 0x49: /* Attack */
		p->plusCC[0] = ApplyIntensityBlend(p->raw[0], p->intensitySlot[0], x);
		break;
	case 0x4b: /* Decay */
		p->plusCC[1] = ApplyIntensityBlend(p->raw[1], p->intensitySlot[1], x);
		break;
	case 0x46: /* Sustain */
		p->plusCC[2] = ApplyIntensityBlend(p->raw[2], p->intensitySlot[2], x);
		break;
	case 0x48: /* Release */
		p->plusCC[3] = ApplyIntensityBlend(p->raw[3], p->intensitySlot[3], x);
		break;
	default:
		break;
	}
}

/* ==================================================================
 * Quad/sub-rate address-table setup, confirmed effectively static --
 * see the header comment on these two declarations
 * ================================================================== */

void CSTGADSRBase::InitializeQuad(STGADSRBaseSubRateParams *params)
{
	/* Shared "no AMS source" default address -- same
	 * `CSTGGlobal::sInstance + 0x29c9fa0` neighbourhood this project
	 * has already confirmed elsewhere (lfo_stepseq_quad.cpp). */
	char *defaultAddr = reinterpret_cast<char *>(CSTGGlobal::sInstance) + 0x29c9fa0;

	char *base = reinterpret_cast<char *>(params);
	for (int slot = 0; slot < 8; slot++) {
		void **s = reinterpret_cast<void **>(base + slot * 0x20);
		s[0] = defaultAddr;
		s[1] = defaultAddr;
		s[2] = defaultAddr;
		s[3] = defaultAddr;
	}
}

void CSTGADSRBase::PrepareSubRateAddressFixupTable(CSTGSubRateAddressFixupTable &table, unsigned long note)
{
	unsigned short cursor = table.count;
	unsigned int quadIndex = note >> 2;

	/* 8 successive entries, real byte-offset order confirmed via
	 * disassembly (NOT ascending -- faithfully preserved rather than
	 * "cleaned up"). */
	static const unsigned short kByteOffsets[8] = {
		0x00, 0x20, 0x40, 0x60, 0xc0, 0xe0, 0x80, 0xa0
	};
	for (int i = 0; i < 8; i++) {
		table.entries[static_cast<unsigned short>(cursor + i)] =
			(quadIndex * 0xcc0 + kByteOffsets[i]) >> 2;
	}
	table.count = static_cast<unsigned short>(cursor + 8);
}

/* ==================================================================
 * Precompute*PlusCC family -- confirmed effectively static, `this`
 * unused, out ptr in EDX, x on the stack -- float args never use
 * regparm slots. Harness-verified formula, see file header.
 * ================================================================== */

void CSTGADSRBase::PrecomputeAttackTimePlusCC(STGADSRBasePrecomputed *out, float x)
{
	out->plusCC[0] = ApplyIntensityBlend(out->raw[0], out->intensitySlot[0], x);
}
void CSTGADSRBase::PrecomputeDecayTimePlusCC(STGADSRBasePrecomputed *out, float x)
{
	out->plusCC[1] = ApplyIntensityBlend(out->raw[1], out->intensitySlot[1], x);
}
void CSTGADSRBase::PrecomputeSustainLevelPlusCC(STGADSRBasePrecomputed *out, float x)
{
	out->plusCC[2] = ApplyIntensityBlend(out->raw[2], out->intensitySlot[2], x);
}
void CSTGADSRBase::PrecomputeReleaseTimePlusCC(STGADSRBasePrecomputed *out, float x)
{
	out->plusCC[3] = ApplyIntensityBlend(out->raw[3], out->intensitySlot[3], x);
}

/* ==================================================================
 * Update* family -- persist the new value into `this`, then (only if
 * ctx's own virtual predicate at vtable slot 0 returns true) recompute
 * the live plusCC display value.
 * ================================================================== */

typedef bool (*CtxPredicateFn)(CSTGPatchMessageContext *);
inline bool CallDisplayPredicate(CSTGPatchMessageContext &ctx)
{
	CtxPredicateFn fn = *reinterpret_cast<CtxPredicateFn *>(ctx._vtablePtr);
	return fn(&ctx);
}

/*
 * NOTE on argument order below: ApplyIntensityBlend takes (base,
 * intensity, x); the 4 methods just below invoke it with the G
 * constant passed as `intensity` and the EXISTING intensitySlot value
 * passed as `x` -- i.e. G plays the role of the inner a-blend driver
 * and intensitySlot plays the role of the outer bipolar blend factor.
 * This matches ground truth's own real instruction order exactly
 * (verified by a full manual x87 stack trace of UpdateSustainLevel's
 * real body) and is genuinely swapped relative to every other family
 * in this file -- not a copy/paste slip, do not "fix" without
 * re-deriving the trace.
 */

void CSTGADSRBase::UpdateAttackTime(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	attackTime = AsFloatBits(newVal.value);
	if (!CallDisplayPredicate(ctx))
		return;
	STGADSRBasePrecomputed *p = LocatePrecomputed(this, ctx);
	p->raw[0] = attackTime;
	p->plusCC[0] = ApplyIntensityBlend(attackTime, AttackG(ctx), p->intensitySlot[0]);
}
void CSTGADSRBase::UpdateDecayTime(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	decayTime = AsFloatBits(newVal.value);
	if (!CallDisplayPredicate(ctx))
		return;
	STGADSRBasePrecomputed *p = LocatePrecomputed(this, ctx);
	p->raw[1] = decayTime;
	p->plusCC[1] = ApplyIntensityBlend(decayTime, DecayG(ctx), p->intensitySlot[1]);
}
void CSTGADSRBase::UpdateSustainLevel(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	sustainLevel = AsFloatBits(newVal.value);
	if (!CallDisplayPredicate(ctx))
		return;
	STGADSRBasePrecomputed *p = LocatePrecomputed(this, ctx);
	p->raw[2] = sustainLevel;
	p->plusCC[2] = ApplyIntensityBlend(sustainLevel, SustainG(ctx), p->intensitySlot[2]);
}
void CSTGADSRBase::UpdateReleaseTime(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	releaseTime = AsFloatBits(newVal.value);
	if (!CallDisplayPredicate(ctx))
		return;
	STGADSRBasePrecomputed *p = LocatePrecomputed(this, ctx);
	p->raw[3] = releaseTime;
	p->plusCC[3] = ApplyIntensityBlend(releaseTime, ReleaseG(ctx), p->intensitySlot[3]);
}

/* ---- AMSSource propagation, unconditional -- no display predicate:
 * store the new source id, then re-resolve GetAMSSourceAddress for
 * every currently-active voice and refresh its cached pointer. Target
 * offsets match InitAMSSourceAddresses' own table exactly. ---- */

void CSTGADSRBase::UpdateAttackTimeAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	attackTimeAMSSource = static_cast<signed char>(newVal.value);
	PropagateAMSSourceAddress(this, ctx, 0x00, attackTimeAMSSource);
}
void CSTGADSRBase::UpdateDecayTimeAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	decayTimeAMSSource = static_cast<signed char>(newVal.value);
	PropagateAMSSourceAddress(this, ctx, 0x40, decayTimeAMSSource);
}
void CSTGADSRBase::UpdateSustainLevelAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	sustainLevelAMSSource = static_cast<signed char>(newVal.value);
	PropagateAMSSourceAddress(this, ctx, 0x80, sustainLevelAMSSource);
}
void CSTGADSRBase::UpdateReleaseTimeAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	releaseTimeAMSSource = static_cast<signed char>(newVal.value);
	PropagateAMSSourceAddress(this, ctx, 0xc0, releaseTimeAMSSource);
}

void CSTGADSRBase::UpdateAttackTimeAMSIntensityAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	attackTimeAMSIntensityAMSSource = static_cast<signed char>(newVal.value);
	PropagateAMSSourceAddress(this, ctx, 0x20, attackTimeAMSIntensityAMSSource);
}
void CSTGADSRBase::UpdateDecayTimeAMSIntensityAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	decayTimeAMSIntensityAMSSource = static_cast<signed char>(newVal.value);
	PropagateAMSSourceAddress(this, ctx, 0x60, decayTimeAMSIntensityAMSSource);
}
void CSTGADSRBase::UpdateSustainLevelAMSIntensityAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	sustainLevelAMSIntensityAMSSource = static_cast<signed char>(newVal.value);
	PropagateAMSSourceAddress(this, ctx, 0xa0, sustainLevelAMSIntensityAMSSource);
}
void CSTGADSRBase::UpdateReleaseTimeAMSIntensityAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	releaseTimeAMSIntensityAMSSource = static_cast<signed char>(newVal.value);
	PropagateAMSSourceAddress(this, ctx, 0xe0, releaseTimeAMSIntensityAMSSource);
}

/* ---- AMSIntensity propagation, indexed, GATED behind the ctx
 * display predicate -- confirmed real, unlike every other propagation
 * family in this file): store the new intensity into BOTH this
 * instance's own field AND STGADSRBasePrecomputed::amsIntensity[n]
 * (keeps the precomp mirror in sync), then, only if the predicate
 * fires, propagate the raw int into every active voice's own
 * per-parameter quad-slot mirror at a fixed offset, 0x10/0x50/0x90/
 * 0xd0 -- confirmed real per-function literal, NOT derived from the
 * 0x20-stride AMSSource table above. ---- */

void CSTGADSRBase::UpdateAttackTimeAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	attackTimeAMSIntensity = newVal.value;
	if (!CallDisplayPredicate(ctx))
		return;
	LocatePrecomputed(this, ctx)->amsIntensity[0] = attackTimeAMSIntensity;
	PropagateRawInt(this, ctx, 0x10, attackTimeAMSIntensity);
}
void CSTGADSRBase::UpdateDecayTimeAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	decayTimeAMSIntensity = newVal.value;
	if (!CallDisplayPredicate(ctx))
		return;
	LocatePrecomputed(this, ctx)->amsIntensity[1] = decayTimeAMSIntensity;
	PropagateRawInt(this, ctx, 0x50, decayTimeAMSIntensity);
}
void CSTGADSRBase::UpdateSustainLevelAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	sustainLevelAMSIntensity = newVal.value;
	if (!CallDisplayPredicate(ctx))
		return;
	LocatePrecomputed(this, ctx)->amsIntensity[2] = sustainLevelAMSIntensity;
	PropagateRawInt(this, ctx, 0x90, sustainLevelAMSIntensity);
}
void CSTGADSRBase::UpdateReleaseTimeAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	releaseTimeAMSIntensity = newVal.value;
	if (!CallDisplayPredicate(ctx))
		return;
	LocatePrecomputed(this, ctx)->amsIntensity[3] = releaseTimeAMSIntensity;
	PropagateRawInt(this, ctx, 0xd0, releaseTimeAMSIntensity);
}

/* ---- AMSIntensityAMSIntensity propagation, unconditional -- no
 * display predicate): store the new value, propagate the raw int into
 * every active voice's quad-slot mirror at 0x30/0x70/0xb0/0xf0. ---- */

void CSTGADSRBase::UpdateAttackTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	attackTimeAMSIntensityAMSIntensity = newVal.value;
	PropagateRawInt(this, ctx, 0x30, attackTimeAMSIntensityAMSIntensity);
}
void CSTGADSRBase::UpdateDecayTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	decayTimeAMSIntensityAMSIntensity = newVal.value;
	PropagateRawInt(this, ctx, 0x70, decayTimeAMSIntensityAMSIntensity);
}
void CSTGADSRBase::UpdateSustainLevelAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	sustainLevelAMSIntensityAMSIntensity = newVal.value;
	PropagateRawInt(this, ctx, 0xb0, sustainLevelAMSIntensityAMSIntensity);
}
void CSTGADSRBase::UpdateReleaseTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	releaseTimeAMSIntensityAMSIntensity = newVal.value;
	PropagateRawInt(this, ctx, 0xf0, releaseTimeAMSIntensityAMSIntensity);
}

/* ==================================================================
 * ToneAdjust* family -- per-note, PRECOMP-ONLY working-copy override.
 * Never touches the persisted instance fields for the "absolute"/
 * "Relative" pairs -- only the AMSIntensity propagation variant reads
 * an instance field, and even then only as WantsCurrentValue's own
 * fallback source, never writing one back.
 * ================================================================== */

void CSTGADSRBase::ToneAdjustAttackTime(CSTGPatchMessageContext &ctx, STGConvertedParam &scale)
{
	STGADSRBasePrecomputed *p = LocatePrecomputed(this, ctx);
	float newRaw = WantsCurrentValue(scale) ? attackTime : AsFloatBits(scale.value);
	p->raw[0] = newRaw;
	p->plusCC[0] = ApplyIntensityBlend(newRaw, p->intensitySlot[0], AttackG(ctx));
}
void CSTGADSRBase::ToneAdjustDecayTime(CSTGPatchMessageContext &ctx, STGConvertedParam &scale)
{
	STGADSRBasePrecomputed *p = LocatePrecomputed(this, ctx);
	float newRaw = WantsCurrentValue(scale) ? decayTime : AsFloatBits(scale.value);
	p->raw[1] = newRaw;
	p->plusCC[1] = ApplyIntensityBlend(newRaw, p->intensitySlot[1], DecayG(ctx));
}
void CSTGADSRBase::ToneAdjustSustainLevel(CSTGPatchMessageContext &ctx, STGConvertedParam &scale)
{
	STGADSRBasePrecomputed *p = LocatePrecomputed(this, ctx);
	float newRaw = WantsCurrentValue(scale) ? sustainLevel : AsFloatBits(scale.value);
	p->raw[2] = newRaw;
	p->plusCC[2] = ApplyIntensityBlend(newRaw, p->intensitySlot[2], SustainG(ctx));
}
void CSTGADSRBase::ToneAdjustReleaseTime(CSTGPatchMessageContext &ctx, STGConvertedParam &scale)
{
	STGADSRBasePrecomputed *p = LocatePrecomputed(this, ctx);
	float newRaw = WantsCurrentValue(scale) ? releaseTime : AsFloatBits(scale.value);
	p->raw[3] = newRaw;
	p->plusCC[3] = ApplyIntensityBlend(newRaw, p->intensitySlot[3], ReleaseG(ctx));
}

void CSTGADSRBase::ToneAdjustAttackTimeRelative(CSTGPatchMessageContext &ctx, STGConvertedParam &scale)
{
	STGADSRBasePrecomputed *p = LocatePrecomputed(this, ctx);
	float newIntensity = AsFloatBits(scale.value);
	p->intensitySlot[0] = newIntensity;
	p->plusCC[0] = ApplyIntensityBlend(p->raw[0], newIntensity, AttackG(ctx));
}
void CSTGADSRBase::ToneAdjustDecayTimeRelative(CSTGPatchMessageContext &ctx, STGConvertedParam &scale)
{
	STGADSRBasePrecomputed *p = LocatePrecomputed(this, ctx);
	float newIntensity = AsFloatBits(scale.value);
	p->intensitySlot[1] = newIntensity;
	p->plusCC[1] = ApplyIntensityBlend(p->raw[1], newIntensity, DecayG(ctx));
}
void CSTGADSRBase::ToneAdjustSustainLevelRelative(CSTGPatchMessageContext &ctx, STGConvertedParam &scale)
{
	STGADSRBasePrecomputed *p = LocatePrecomputed(this, ctx);
	float newIntensity = AsFloatBits(scale.value);
	p->intensitySlot[2] = newIntensity;
	p->plusCC[2] = ApplyIntensityBlend(p->raw[2], newIntensity, SustainG(ctx));
}
void CSTGADSRBase::ToneAdjustReleaseTimeRelative(CSTGPatchMessageContext &ctx, STGConvertedParam &scale)
{
	STGADSRBasePrecomputed *p = LocatePrecomputed(this, ctx);
	float newIntensity = AsFloatBits(scale.value);
	p->intensitySlot[3] = newIntensity;
	p->plusCC[3] = ApplyIntensityBlend(p->raw[3], newIntensity, ReleaseG(ctx));
}

/* Unconditional propagation, no display predicate, same 0x10/0x50/
 * 0x90/0xd0 targets as the indexed Update*AMSIntensity family. */
void CSTGADSRBase::ToneAdjustAttackTimeAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &scale)
{
	int newVal = WantsCurrentValue(scale) ? attackTimeAMSIntensity : scale.value;
	PropagateRawInt(this, ctx, 0x10, newVal);
}
void CSTGADSRBase::ToneAdjustDecayTimeAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &scale)
{
	int newVal = WantsCurrentValue(scale) ? decayTimeAMSIntensity : scale.value;
	PropagateRawInt(this, ctx, 0x50, newVal);
}
void CSTGADSRBase::ToneAdjustSustainLevelAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &scale)
{
	int newVal = WantsCurrentValue(scale) ? sustainLevelAMSIntensity : scale.value;
	PropagateRawInt(this, ctx, 0x90, newVal);
}
void CSTGADSRBase::ToneAdjustReleaseTimeAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &scale)
{
	int newVal = WantsCurrentValue(scale) ? releaseTimeAMSIntensity : scale.value;
	PropagateRawInt(this, ctx, 0xd0, newVal);
}

/* ==================================================================
 * Get* family -- all 20 write CSTGParamsOwner::sValueGetterTemp and
 * return a reference to it (see that field's own comment, oa_global.h,
 * for the confirmed shared-scratch mechanism). The 4 base + 8
 * AMSIntensity and AMSIntensity-double getters do a RAW 32-bit copy into both
 * `.value` and `.displayValue` (float fields are bit-reinterpreted,
 * never numerically converted); the 8 AMSSource and AMSSource-double getters
 * sign-extend an int8 field into `.value` only -- confirmed: their real
 * bodies never touch `.displayValue`.
 * ================================================================== */

STGConvertedParam &CSTGADSRBase::GetAttackTime(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = AsBits(attackTime);
	CSTGParamsOwner::sValueGetterTemp.displayValue = CSTGParamsOwner::sValueGetterTemp.value;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGADSRBase::GetDecayTime(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = AsBits(decayTime);
	CSTGParamsOwner::sValueGetterTemp.displayValue = CSTGParamsOwner::sValueGetterTemp.value;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGADSRBase::GetSustainLevel(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = AsBits(sustainLevel);
	CSTGParamsOwner::sValueGetterTemp.displayValue = CSTGParamsOwner::sValueGetterTemp.value;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGADSRBase::GetReleaseTime(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = AsBits(releaseTime);
	CSTGParamsOwner::sValueGetterTemp.displayValue = CSTGParamsOwner::sValueGetterTemp.value;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGADSRBase::GetAttackTimeAMSSource(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = attackTimeAMSSource;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGADSRBase::GetDecayTimeAMSSource(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = decayTimeAMSSource;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGADSRBase::GetSustainLevelAMSSource(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = sustainLevelAMSSource;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGADSRBase::GetReleaseTimeAMSSource(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = releaseTimeAMSSource;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGADSRBase::GetAttackTimeAMSIntensity(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = attackTimeAMSIntensity;
	CSTGParamsOwner::sValueGetterTemp.displayValue = attackTimeAMSIntensity;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGADSRBase::GetDecayTimeAMSIntensity(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = decayTimeAMSIntensity;
	CSTGParamsOwner::sValueGetterTemp.displayValue = decayTimeAMSIntensity;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGADSRBase::GetSustainLevelAMSIntensity(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = sustainLevelAMSIntensity;
	CSTGParamsOwner::sValueGetterTemp.displayValue = sustainLevelAMSIntensity;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGADSRBase::GetReleaseTimeAMSIntensity(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = releaseTimeAMSIntensity;
	CSTGParamsOwner::sValueGetterTemp.displayValue = releaseTimeAMSIntensity;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGADSRBase::GetAttackTimeAMSIntensityAMSSource(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = attackTimeAMSIntensityAMSSource;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGADSRBase::GetDecayTimeAMSIntensityAMSSource(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = decayTimeAMSIntensityAMSSource;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGADSRBase::GetSustainLevelAMSIntensityAMSSource(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = sustainLevelAMSIntensityAMSSource;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGADSRBase::GetReleaseTimeAMSIntensityAMSSource(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = releaseTimeAMSIntensityAMSSource;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGADSRBase::GetAttackTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = attackTimeAMSIntensityAMSIntensity;
	CSTGParamsOwner::sValueGetterTemp.displayValue = attackTimeAMSIntensityAMSIntensity;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGADSRBase::GetDecayTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = decayTimeAMSIntensityAMSIntensity;
	CSTGParamsOwner::sValueGetterTemp.displayValue = decayTimeAMSIntensityAMSIntensity;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGADSRBase::GetSustainLevelAMSIntensityAMSIntensity(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = sustainLevelAMSIntensityAMSIntensity;
	CSTGParamsOwner::sValueGetterTemp.displayValue = sustainLevelAMSIntensityAMSIntensity;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGADSRBase::GetReleaseTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = releaseTimeAMSIntensityAMSIntensity;
	CSTGParamsOwner::sValueGetterTemp.displayValue = releaseTimeAMSIntensityAMSIntensity;
	return CSTGParamsOwner::sValueGetterTemp;
}
