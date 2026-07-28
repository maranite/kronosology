// SPDX-License-Identifier: GPL-2.0
/*
 * lfo_component.cpp  -  CSTGLFO (86 of 87 real methods, 2 passes) +
 * the 3 small CSTGLFOBase methods pass 1 calls into. See
 * include/oa_lfo.h for the full field-layout table, the two distinct
 * per-note working structures, and pass 2's own native-execution-
 * harness derivation of the shared BlendKnobIndex() idiom (below).
 * ProcessSubRate is the one method still genuinely deferred -- see
 * oa_lfo.h's file header for why.
 */

#include "oa_lfo.h"

extern "C" unsigned char _ZTV7CSTGLFO[0xf0] = { 0 };
/* _ZTV15CSTGParamsOwner's storage lives in program_ctor.cpp -- see
 * oa_lfo.h's own comment. */

/* CSTGTables::kLFOKnobToFreq/kLFOFadeTimeSeconds and
 * CSTGTempo::sBaseNoteClockTable are opaque, out-of-scope framework
 * tables (same treatment as CSTGADSRBase's own paramConstantsTable) --
 * storage provided here since this is the first reconstructed TU to
 * reference them; real contents not modeled. */
float CSTGTables::kLFOKnobToFreq[101];
float CSTGTables::kLFOFadeTimeSeconds[100];
unsigned short CSTGTempo::sBaseNoteClockTable[11];

namespace {

/* Host/target pointer-width fix, same convention as
 * lfo_stepseq_quad.cpp's own file-local copy (separate TU, so not
 * shared directly). */
inline unsigned int ToU32(void *p) { return static_cast<unsigned int>(reinterpret_cast<unsigned long>(p)); }

/* Real 3-entry HandlesCC lookup table (`.rodata+0x4c120`, CC 0x4c..
 * 0x4e / 76..78) -- opaque payload, same "confirmed real, contents
 * not modeled" treatment as CSTGADSRBase's own kHandlesCCTable. */
extern const bool kLFOHandlesCCTable[3];

inline bool CallDisplayPredicate(CSTGPatchMessageContext &ctx)
{
	typedef bool (*Fn)(CSTGPatchMessageContext *);
	Fn fn = *reinterpret_cast<Fn *>(ctx._vtablePtr);
	return fn(&ctx);
}

inline STGLFOPrecomputed *LocatePrecomputed(const CSTGLFO *self, const CSTGPatchMessageContext &ctx)
{
	return reinterpret_cast<STGLFOPrecomputed *>(
		ctx.precomputedBaseOffset + self->_slotInfo->precomputedSlotIndex);
}

/* Shared "quad table" base every per-active-voice slice is addressed
 * from -- identical formula to CSTGADSRBase's own
 * PropagateAMSSourceAddress (oa_adsr_base.h), confirmed independently
 * by this cluster's own GetOutput/InitAMSSourceAddresses/every
 * Update* propagation loop. */
inline char *QuadTableBase(const CSTGLFO *self)
{
	CSTGVoiceModelManager *vmm = CSTGVoiceModelManager::sInstance;
	return *reinterpret_cast<char **>(reinterpret_cast<char *>(vmm) + 4)
		+ self->_slotInfo->subRateBaseIndex;
}

inline unsigned int SliceIndex(unsigned short note)
{
	return (note & 3) + (note >> 2) * 0xcc0;
}

inline STGLFOSubRateParamsSlice *SliceFor(char *quadTableBase, unsigned short note)
{
	return reinterpret_cast<STGLFOSubRateParamsSlice *>(quadTableBase + SliceIndex(note) * 4);
}

inline void *RawFieldAt(STGLFOSubRateParamsSlice *slice, int offset)
{
	return reinterpret_cast<char *>(slice) + offset;
}

/* First active-voice-list entry for this component instance, or
 * nullptr -- identical formula/null-table treatment to CSTGADSRBase's
 * own PropagateAMSSourceAddress (real ground truth substitutes a
 * shared `sEmptyActiveVoiceList` sentinel object here instead of a
 * null check, but that sentinel's own head slot is itself always
 * null, so returning nullptr directly is behaviorally identical and
 * avoids modeling an unused extern -- same simplification ADSR's own
 * reconstruction already made). */
inline void *FirstActiveVoiceEntry(const CSTGPatchMessageContext &ctx)
{
	char *table = static_cast<char *>(ctx.activeVoiceListTable);
	if (!table)
		return nullptr;
	void **headSlot = reinterpret_cast<void **>(table + ctx.componentSlotIndex * 12 + 0x44);
	return *headSlot;
}

/* Propagate a raw 32-bit value (int or float bit-pattern, copied
 * verbatim -- matches ground truth's own untyped `mov`/`fstp` stores)
 * into every active voice's own slice at a fixed offset. Covers
 * UpdateShape/UpdateShapeAMSIntensity/UpdateOffset/UpdateMIDITempoSync/
 * the 3 non-source FrequencyAMS*Intensity updates. */
void PropagateRawToVoices(const CSTGLFO *self, CSTGPatchMessageContext &ctx, int offset, unsigned int value)
{
	char *quadTableBase = QuadTableBase(self);
	void *entry = FirstActiveVoiceEntry(ctx);
	while (entry) {
		CSTGVoice *voice = *reinterpret_cast<CSTGVoice **>(static_cast<char *>(entry) + 8);
		unsigned short note = *reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(voice) + 4);
		STGLFOSubRateParamsSlice *slice = SliceFor(quadTableBase, note);
		*reinterpret_cast<unsigned int *>(RawFieldAt(slice, offset)) = value;
		entry = *reinterpret_cast<void **>(entry);
	}
}

/* Propagate a resolved AMS-source-address pointer into every active
 * voice's own slice at a fixed offset. Covers the 4
 * FrequencyAMS*Source/ShapeAMSSource updates. */
void PropagateAMSAddressToVoices(const CSTGLFO *self, CSTGPatchMessageContext &ctx, int offset, signed char amsSource)
{
	char *quadTableBase = QuadTableBase(self);
	void *entry = FirstActiveVoiceEntry(ctx);
	while (entry) {
		CSTGVoice *voice = *reinterpret_cast<CSTGVoice **>(static_cast<char *>(entry) + 8);
		unsigned short note = *reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(voice) + 4);
		STGLFOSubRateParamsSlice *slice = SliceFor(quadTableBase, note);
		*reinterpret_cast<void **>(RawFieldAt(slice, offset)) = voice->GetAMSSourceAddress(amsSource);
		entry = *reinterpret_cast<void **>(entry);
	}
}

} // namespace

/* ==================================================================
 * Destruction. No ground-truth CSTGLFO::CSTGLFO() was found (see
 * oa_lfo.h); the destructor IS real and confirmed (D1/D2 share one
 * COMDAT body, D0 a separate trivial one) -- but ground truth's own
 * dtor resets the vtable pointers to the IMMEDIATE BASE classes'
 * construction vtables (`this[+0xc]=&_ZTV7CSTGLFO[0xe0]`,
 * `this[+0]=&_ZTV15CSTGParamsOwner[8]`), the standard multi-level
 * Itanium-ABI unwind step -- NOT this project's own single-vtable
 * "install vs dispatch" model. Reproduced here in the simplified
 * single-vtable shape (matching the ctor's own inferred install)
 * rather than the two real writes, consistent with modeling CSTGLFO
 * as a standalone non-inheriting class throughout. ==================
 */
CSTGLFO::~CSTGLFO()
{
	*reinterpret_cast<void **>(this) = _ZTV7CSTGLFO + 8;
}

extern "C" void _ZN7CSTGLFOD0Ev(void *self)
{
	*reinterpret_cast<void **>(self) = _ZTV7CSTGLFO + 8;
}

/* ==================================================================
 * CSTGLFOBase's 3 small real methods (declared in oa_engine_init.h)
 * ================================================================== */

void CSTGLFOBase::UpdateStartPhase(STGLFOSubRateParamsSlice *slice, STGConvertedParam &newVal)
{
	slice->startPhase = static_cast<signed char>(newVal.value);
}

float CSTGLFOBase::CalculateFreq(const STGLFOSubRateParamsSlice *slice, unsigned char note)
{
	/* Real body reads two fields of `slice` at +0x18 (int8 fine
	 * offset, added to `note`) and +0x12 (float interpolation
	 * weight) -- neither is one of this struct's named offsets (both
	 * land inside the still-unrecovered +0x005..+0x0e0 span), so
	 * they're addressed by raw offset here rather than invented
	 * field names. No branches in the real body. */
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(slice);
	unsigned char idx = static_cast<unsigned char>(note + *reinterpret_cast<const signed char *>(raw + 0x18));
	float lo = CSTGTables::kLFOKnobToFreq[idx];
	float hi = CSTGTables::kLFOKnobToFreq[idx + 1];
	float weight = *reinterpret_cast<const float *>(raw + 0x12);
	float interpolated = lo + (hi - lo) * weight;
	return interpolated * CSTGAudioBusManager::sInstance->busGainReciprocal * 8.0f;
}

int CSTGLFOBase::GetRandomFlagsForWaveform(int waveform)
{
	static const int kRandomFlagsTable[6] = { 0, 0, 0, 0, 0, 0 }; /* real table at
		.rodata+0x4c264, contents not modeled -- see oa_lfo.h. */
	unsigned int idx = static_cast<unsigned int>(waveform - 0xc);
	if (idx > 5)
		return 0;
	return kRandomFlagsTable[idx];
}

/* ==================================================================
 * Virtual-slot overrides
 * ================================================================== */

bool CSTGLFO::HandlesCC(unsigned char cc) const
{
	unsigned int idx = static_cast<unsigned int>(cc - 0x4c);
	if (idx > 2)
		return false;
	return kLFOHandlesCCTable[idx];
}

void CSTGLFO::InitAMSSourceAddresses(CSTGVoice &voice)
{
	char *quadTableBase = QuadTableBase(this);
	unsigned short note = *reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(&voice) + 4);
	STGLFOSubRateParamsSlice *slice = SliceFor(quadTableBase, note);

	*reinterpret_cast<void **>(RawFieldAt(slice, 0x10)) = voice.GetAMSSourceAddress(frequencyAMS1Source);
	*reinterpret_cast<void **>(RawFieldAt(slice, 0x30)) = voice.GetAMSSourceAddress(frequencyAMS2Source);
	*reinterpret_cast<void **>(RawFieldAt(slice, 0x50)) = voice.GetAMSSourceAddress(frequencyAMS1ModSource);
	*reinterpret_cast<void **>(RawFieldAt(slice, 0x70)) = voice.GetAMSSourceAddress(shapeAMSSource);
}

/* CSTGLFO's own override just forwards to CSTGLFOBase::InitializeQuad
 * -- confirmed real: the body only reads its FIRST explicit formal
 * (EDX), the second ("STGLFOSubRateParams* quad" per the mangled
 * signature) is never touched. Transcribed exactly as observed rather
 * than "corrected" -- see oa_lfo.h. */
void CSTGLFO::InitializeQuad(void *unused, STGLFOSubRateParams * /*quad*/)
{
	CSTGLFOBase::InitializeQuad(reinterpret_cast<STGLFOSubRateParams *>(unused));
}

/* ==================================================================
 * Registration accessors -- all trivial constant/table-pointer
 * returns, confirmed real via relocation.
 * ================================================================== */

int CSTGLFO::GetId() { return 0xe; }
const char *CSTGLFO::GetName() { return "LFO"; /* real target: .rodata.str1.1+0x1a3b */ }
int CSTGLFO::GetNumParams() { return 0x15; }
const void *CSTGLFO::GetParamDescriptors() { return STGLFOParams; }
const void *CSTGLFO::GetMessageHandlers() { return _ZN7CSTGLFO16sMessageHandlersE; }
const void *CSTGLFO::GetValueGetters() { return _ZN7CSTGLFO13sValueGettersE; }

/* ==================================================================
 * Quad/sub-rate
 * ================================================================== */

STGLFOSubRateParamsSlice *CSTGLFO::GetOutput(int note, int subSlot)
{
	char *quadTableBase = QuadTableBase(this);
	unsigned short n = static_cast<unsigned short>(note);
	return reinterpret_cast<STGLFOSubRateParamsSlice *>(
		quadTableBase + SliceIndex(n) * 4 + subSlot * 16);
}

void CSTGLFO::UpdateOutput(STGLFOSubRateParamsSlice *slice, float in, bool active)
{
	slice->output = active ? (in * slice->fadeProgress + slice->offset) : 0.0f;
}

void CSTGLFO::AdvanceFadeEnv(STGLFOSubRateParamsSlice *slice, unsigned int ticks)
{
	float progress = static_cast<float>(ticks) * slice->fadeRate + slice->fadeProgress;
	slice->fadeProgress = (progress > 1.0f) ? 1.0f : progress;
}

/* Appends 5 entries (NOT 8, unlike CSTGADSRBase's own 8-per-call
 * version) -- confirmed real: `entries[count+i] = (note+K)>>2` for
 * K in {0x10,0x30,0x50,0x70,0x90}, `count += 5`. The 4 K values
 * matching InitAMSSourceAddresses' own 4 AMS-address offsets
 * (0x10/0x30/0x50/0x70); the 5th (0x90) has no matching write in this
 * cluster's own InitAMSSourceAddresses -- confirmed real, consumer
 * not identified within this pass's scope. */
void CSTGLFO::PrepareSubRateAddressFixupTable(CSTGSubRateAddressFixupTable &table, unsigned long note)
{
	static const unsigned int kOffsets[5] = { 0x10, 0x30, 0x50, 0x70, 0x90 };
	unsigned short count = table.count;
	for (int i = 0; i < 5; i++)
		table.entries[count + i] = static_cast<unsigned int>(note + kOffsets[i]) >> 2;
	table.count = static_cast<unsigned short>(count + 5);
}

/* ==================================================================
 * Update* family
 * ================================================================== */

void CSTGLFO::UpdateShape(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	shape = newVal.value;
	LocatePrecomputed(this, ctx)->shapeRaw = shape;
	if (!CallDisplayPredicate(ctx))
		return;
	PropagateRawToVoices(this, ctx, 0x1c0, static_cast<unsigned int>(shape));
}

void CSTGLFO::UpdateShapeAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	shapeAMSIntensity = newVal.value;
	PropagateRawToVoices(this, ctx, 0x80, static_cast<unsigned int>(shapeAMSIntensity));
}

void CSTGLFO::UpdateOffset(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	offset = newVal.value;
	PropagateRawToVoices(this, ctx, 0x1b0, static_cast<unsigned int>(offset));
}

void CSTGLFO::UpdateKeySync(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	(void)ctx;
	bool set = newVal.value != 0;
	flags = static_cast<unsigned char>((flags & ~0x2) | (set ? 0x2 : 0));
}

void CSTGLFO::UpdateMIDITempoSync(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	bool set = newVal.value != 0;
	flags = static_cast<unsigned char>((flags & ~0x4) | (set ? 0x4 : 0));
	PropagateRawToVoices(this, ctx, 0x120, set ? 0xffffffffu : 0u);
}

/* Both store their own field then forward unconditionally to
 * HandleUpdateTempoPeriod -- confirmed real, callee deliberately
 * deferred (declared, not defined -- see its own declaration comment
 * in oa_lfo.h). Neither is exercised by this pass's own verify test. */
void CSTGLFO::UpdateMIDITempoSyncTimes(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	midiTempoSyncTimes = static_cast<unsigned char>(newVal.value);
	HandleUpdateTempoPeriod(ctx);
}
void CSTGLFO::UpdateMIDITempoSyncBaseNote(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	midiTempoSyncBaseNote = static_cast<signed char>(newVal.value);
	HandleUpdateTempoPeriod(ctx);
}

void CSTGLFO::UpdateStartPhase(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	(void)ctx;
	/* Real body: `CSTGLFOBase::UpdateStartPhase(this+0xc, newVal)` --
	 * forwards to the secondary CSTGLFOBase sub-object embedded at
	 * offset +0xc (see oa_lfo.h's multiple-inheritance note). This
	 * project doesn't model that sub-object as a real base, so the
	 * forward targets `this->startPhase` directly (the SAME real
	 * total-object field CSTGLFOBase::UpdateStartPhase would have
	 * written via the adjusted pointer). */
	startPhase = static_cast<signed char>(newVal.value);
}

void CSTGLFO::UpdateWaveform(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	waveform = static_cast<signed char>(newVal.value);
	if (!CallDisplayPredicate(ctx))
		return;
	LocatePrecomputed(this, ctx)->waveform = waveform; /* precomp+0x28, confirmed real
		write -- now a named field, see oa_lfo.h (follow-up pass). */
	HandleWaveformChanged(ctx, waveform);
}

void CSTGLFO::SetWantsCCMod(CSTGPatchMessageContext &ctx, bool wantsIt)
{
	STGLFOPrecomputed *p = LocatePrecomputed(this, ctx);
	p->flags = static_cast<unsigned char>((p->flags & ~0x2) | (wantsIt ? 0x2 : 0));
}

void CSTGLFO::UpdateFrequencyAMS1Intensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	frequencyAMS1Intensity = newVal.value;
	PropagateRawToVoices(this, ctx, 0x20, static_cast<unsigned int>(frequencyAMS1Intensity));
}
void CSTGLFO::UpdateFrequencyAMS2Intensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	frequencyAMS2Intensity = newVal.value;
	PropagateRawToVoices(this, ctx, 0x40, static_cast<unsigned int>(frequencyAMS2Intensity));
}
void CSTGLFO::UpdateFrequencyAMS1ModIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	frequencyAMS1ModIntensity = newVal.value;
	PropagateRawToVoices(this, ctx, 0x60, static_cast<unsigned int>(frequencyAMS1ModIntensity));
}

void CSTGLFO::UpdateFrequencyAMS1ModSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	frequencyAMS1ModSource = static_cast<signed char>(newVal.value);
	PropagateAMSAddressToVoices(this, ctx, 0x50, frequencyAMS1ModSource);
}
void CSTGLFO::UpdateFrequencyAMS2Source(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	frequencyAMS2Source = static_cast<signed char>(newVal.value);
	PropagateAMSAddressToVoices(this, ctx, 0x30, frequencyAMS2Source);
}
void CSTGLFO::UpdateFrequencyAMS1Source(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	frequencyAMS1Source = static_cast<signed char>(newVal.value);
	PropagateAMSAddressToVoices(this, ctx, 0x10, frequencyAMS1Source);
}
void CSTGLFO::UpdateShapeAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	shapeAMSSource = static_cast<signed char>(newVal.value);
	PropagateAMSAddressToVoices(this, ctx, 0x70, shapeAMSSource);
}

/* ==================================================================
 * Handle* family -- both pure integer/pointer control flow (no float
 * branch-sense risk), see oa_lfo.h for why that distinction mattered
 * for this pass's scoping.
 * ================================================================== */

void CSTGLFO::HandleWaveformChanged(CSTGPatchMessageContext &ctx, int waveformArg)
{
	char *quadTableBase = QuadTableBase(this);
	unsigned int tablesPtr; /* raw 32-bit pointer bit-pattern -- see
				  * STGLFOSubRateParamsSlice::lfoTables' own
				  * comment (oa_lfo.h) for why not `void*`. */
	int flagsA;
	int flagsF;
	bool maskedBranch = false;

	if (waveformArg <= 0xb) {
		/* Table waveform (0..11): CSTGLFOTables::sInstance's own
		 * per-waveform 516-byte (0x200+4) sub-table. */
		tablesPtr = ToU32(reinterpret_cast<char *>(CSTGLFOTables::sInstance) + waveformArg * 516);
		bool inSpecialSet = (waveformArg == 8 || waveformArg == 2 || waveformArg == 0xa || waveformArg == 9);
		flagsA = (inSpecialSet || waveformArg == 0xb) ? -1 : 0;
		flagsF = 0;
	} else {
		/* "Random" waveform family (12..17), or beyond -- shared
		 * fixed sub-table offset, matching InitializeQuad's own
		 * default. */
		tablesPtr = ToU32(reinterpret_cast<char *>(CSTGLFOTables::sInstance) + 0x408);
		flagsA = -1;
		flagsF = CSTGLFOBase::GetRandomFlagsForWaveform(waveformArg);
		maskedBranch = (flagsF & 0x2) != 0;
	}

	void *entry = FirstActiveVoiceEntry(ctx);
	while (entry) {
		CSTGVoice *voice = *reinterpret_cast<CSTGVoice **>(static_cast<char *>(entry) + 8);
		unsigned short note = *reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(voice) + 4);
		STGLFOSubRateParamsSlice *slice = SliceFor(quadTableBase, note);
		slice->lfoTables = tablesPtr;
		slice->waveformFlagsA = flagsA;
		slice->waveformFlagsF = flagsF;
		if (maskedBranch) {
			slice->waveformMaskedE = *reinterpret_cast<unsigned int *>(RawFieldAt(slice, 0x170)) & 0x3fffffffu;
			slice->waveformConstD = 0x3f800000u; /* 1.0f */
		}
		entry = *reinterpret_cast<void **>(entry);
	}
}

void CSTGLFO::HandleStopChanged(CSTGPatchMessageContext &ctx)
{
	if (!CallDisplayPredicate(ctx))
		return;

	STGLFOPrecomputed *p = LocatePrecomputed(this, ctx);
	bool wasSet = (p->flags & 0x1) != 0;
	unsigned int propagated = wasSet ? 0xffffffffu : 0u;

	char *quadTableBase = QuadTableBase(this);
	void *entry = FirstActiveVoiceEntry(ctx);
	while (entry) {
		CSTGVoice *voice = *reinterpret_cast<CSTGVoice **>(static_cast<char *>(entry) + 8);
		unsigned short note = *reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(voice) + 4);
		STGLFOSubRateParamsSlice *slice = SliceFor(quadTableBase, note);
		slice->stop = static_cast<int>(propagated);
		entry = *reinterpret_cast<void **>(entry);
	}
}

/* ==================================================================
 * Get* family -- all write CSTGParamsOwner::sValueGetterTemp and
 * return a reference (same shared-scratch idiom as CSTGADSRBase's own
 * Get* family, oa_adsr_base.h). The 5 fields with a confirmed
 * "both value+displayValue" real body are marked; every AMSSource
 * accessor (int8, sign-extended) and the 3 flag-bit accessors
 * (Stop/KeySync/MIDITempoSync) only ever touch `.value`.
 * ================================================================== */

STGConvertedParam &CSTGLFO::GetWaveform(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = waveform;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetStartPhase(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = startPhase;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetShape(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = shape;
	CSTGParamsOwner::sValueGetterTemp.displayValue = shape;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetShapeAMSSource(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = shapeAMSSource;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetShapeAMSIntensity(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = shapeAMSIntensity;
	CSTGParamsOwner::sValueGetterTemp.displayValue = shapeAMSIntensity;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetFrequency(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = frequency;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetFrequencyFine(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = frequencyFine;
	CSTGParamsOwner::sValueGetterTemp.displayValue = frequencyFine;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetStop(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = flags & 0x1;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetOffset(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = offset;
	CSTGParamsOwner::sValueGetterTemp.displayValue = offset;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetKeySync(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = (flags >> 1) & 0x1;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetFade(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = fade;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetDelay(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = delay;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetFrequencyAMS1Source(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = frequencyAMS1Source;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetFrequencyAMS1Intensity(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = frequencyAMS1Intensity;
	CSTGParamsOwner::sValueGetterTemp.displayValue = frequencyAMS1Intensity;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetFrequencyAMS1ModSource(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = frequencyAMS1ModSource;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetFrequencyAMS1ModIntensity(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = frequencyAMS1ModIntensity;
	CSTGParamsOwner::sValueGetterTemp.displayValue = frequencyAMS1ModIntensity;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetFrequencyAMS2Source(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = frequencyAMS2Source;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetFrequencyAMS2Intensity(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = frequencyAMS2Intensity;
	CSTGParamsOwner::sValueGetterTemp.displayValue = frequencyAMS2Intensity;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetMIDITempoSync(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = (flags >> 2) & 0x1;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetMIDITempoSyncBaseNote(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = midiTempoSyncBaseNote;
	return CSTGParamsOwner::sValueGetterTemp;
}
STGConvertedParam &CSTGLFO::GetMIDITempoSyncTimes(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = midiTempoSyncTimes;
	return CSTGParamsOwner::sValueGetterTemp;
}

namespace {
const bool kLFOHandlesCCTable[3] = { false, false, false }; /* real table at
	.rodata+0x4c120, contents not modeled -- see file header. */
} // namespace

/* ==================================================================
 * Follow-up pass: the 19-deferred-method "knob-index blend" x87
 * idiom, pinned down with a native-execution harness (mmaps the real
 * CSTGLFO::UpdateFade bytes from OA.ko, patches its 3 .rodata
 * relocations, executes it directly against ~135 (fade, V) sample
 * pairs). Confirmed formula:
 *
 *   int BlendKnobIndex(float v, int knob, int maxKnob = 99) {
 *       float scale = (v >= 0.0f) ? float(maxKnob - knob) : float(knob);
 *       return knob + (int)(v * scale);   // truncate toward zero
 *   }
 *
 * 133/135 harness runs matched exactly; the 2 mismatches are a known
 * x87-extended-precision-vs-SSE-strict-precision rounding difference
 * exactly AT an integer boundary (e.g. fade=49, v=0.9f: real ground
 * truth's x87 multiply rounds (0.9f as stored)*50 fractionally above
 * 45.0 in its 80-bit intermediate before truncating, host SSE strict
 * float32 arithmetic rounds fractionally below 45.0) -- not a logic
 * bug, and irrelevant to THIS reconstruction since lfo_component.o
 * itself compiles under the SAME x87-default flags as ground truth
 * (no `-msse2` override in the Makefile for this TU), so the emitted
 * code for the formula above should reproduce the same 80-bit
 * intermediate rounding ground truth's own compiler did.
 *
 * This ONE primitive, composed 1x (UpdateFade/PrecomputeFade -- no
 * CC-mod stage) or 2x in sequence (BlendKnobIndex(ccMod,
 * BlendKnobIndex(v, knob))) accounts for EVERY one of this pass's
 * "knob-index blend" methods: UpdateFrequency/UpdateFrequencyFine/
 * HandleUpdateFrequency/PrecomputeFreqPlusCC/HandleCC's frequency
 * branch (2x, knob=frequency), UpdateDelay/PrecomputeDelayTicks/
 * HandleCC's delay branch/ToneAdjustDelayRelative (2x, knob=delay),
 * PrecomputeData's own inline fade section (2x, knob=fade -- the ONE
 * surprising case, see PrecomputeData's own comment below),
 * PrecomputeBaseNoteAndTempo/HandleUpdateTempoPeriod (1x, negated
 * input, knob=midiTempoSyncBaseNote, maxKnob=9 not 99 -- same
 * instruction shape, different immediate, confirmed via a second,
 * independent objdump read of both functions).
 * ================================================================== */

namespace {

inline int BlendKnobIndex(float v, int knob, int maxKnob = 99)
{
	float scale = (v >= 0.0f) ? static_cast<float>(maxKnob - knob) : static_cast<float>(knob);
	return knob + static_cast<int>(v * scale);
}

/* Real .rodata.cst4/.rodata.cst8 constants this cluster's own
 * relocations point at, resolved to their actual IEEE754 values by
 * reading the real OA.ko bytes directly (same "resolve the opaque
 * constant" precedent as CalculateFreq's own `8.0f`, oa_lfo.h) --
 * `.rodata.cst4+0x5d4` = 1.0f (used directly as a literal below,
 * `1.0f / x`, not worth a named constant), `.rodata.cst4+0x5d8` =
 * 9.313225746154785e-10f (= exactly 2^-30, a Q30 fixed-point-to-float
 * tick scale), `.rodata.cst8+0x230` = 0.032 (a double, delay-ticks
 * seconds-per-tick-ish scale). */
constexpr float kLFOTempoPeriodScale = 9.313225746154785e-10f;
constexpr double kLFODelayTicksScale = 0.032;

/* pc->flags bit1 ("wants CC modulation", SetWantsCCMod) gates whether
 * the blend's 2nd stage reads a real per-parameter AMS-intensity
 * constant from ctx.paramConstantsTable or a hardcoded 0.0f -- shared
 * by every Update* (or PrecomputeData) call site below (HandleCC and the
 * Precompute*PlusCC family instead take their own 2nd-stage value
 * directly as a parameter, no table read). */
inline float ResolveTableCCMod(CSTGPatchMessageContext &ctx, STGLFOPrecomputed *pc, int tableOffset)
{
	if ((pc->flags & 0x2) == 0)
		return 0.0f;
	return *reinterpret_cast<float *>(static_cast<char *>(ctx.paramConstantsTable) + tableOffset);
}

/* ---- Frequency: BlendKnobIndex(ccMod, BlendKnobIndex(V, frequency))
 * -> CSTGLFOBase::CalculateFreq(this+0xc, idx) -> pc->freqResult. ---- */
float ComputeFreqBlend(CSTGLFO *self, STGLFOPrecomputed *pc, float ccMod)
{
	int idx1 = BlendKnobIndex(pc->freqV, self->frequency);
	int idx2 = BlendKnobIndex(ccMod, idx1);
	auto *lfoBaseThis = reinterpret_cast<const STGLFOSubRateParamsSlice *>(reinterpret_cast<char *>(self) + 0xc);
	float freq = CSTGLFOBase::CalculateFreq(lfoBaseThis, static_cast<unsigned char>(idx2));
	pc->freqResult = freq;
	return freq;
}

void PropagateFreqToVoices(CSTGLFO *self, CSTGPatchMessageContext &ctx, float freq)
{
	char *quadTableBase = QuadTableBase(self);
	void *entry = FirstActiveVoiceEntry(ctx);
	while (entry) {
		CSTGVoice *voice = *reinterpret_cast<CSTGVoice **>(static_cast<char *>(entry) + 8);
		unsigned short note = *reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(voice) + 4);
		STGLFOSubRateParamsSlice *slice = SliceFor(quadTableBase, note);
		slice->freq = freq;
		entry = *reinterpret_cast<void **>(entry);
	}
}

/* ---- Delay: BlendKnobIndex(ccMod, BlendKnobIndex(V, delay)) -> scale
 * by kLFODelayTicksScale and busGainReciprocal -> pc->delayTicks.
 * Never broadcast to any active voice by any of this pass's callers. ---- */
void ComputeDelayBlend(CSTGLFO *self, STGLFOPrecomputed *pc, float ccMod)
{
	int idx1 = BlendKnobIndex(pc->delayV, self->delay);
	int idx2 = BlendKnobIndex(ccMod, idx1);
	double scaled = static_cast<double>(idx2) * kLFODelayTicksScale
		* static_cast<double>(CSTGAudioBusManager::sInstance->busGainReciprocal);
	pc->delayTicks = static_cast<int>(static_cast<float>(scaled));
}

/* ---- Fade: BlendKnobIndex(ccMod, BlendKnobIndex(V, fade)); idx==0 is
 * a special "instant" case (fadeIsZeroFlag=1, fadeSeconds=0); else a
 * reciprocal table lookup -> pc->fadeSeconds/fadeIsZeroFlag. `ccMod`
 * is 0.0f at every single-stage call site (UpdateFade/PrecomputeFade/
 * ToneAdjustFadeRelative -- BlendKnobIndex(0.0f, x) == x always, so
 * passing 0.0f here is exactly equivalent to their real 1-stage
 * bodies, not an approximation) and a real per-tick CC value only at
 * PrecomputeData's own 2-stage call site. Never broadcast to any
 * active voice by any of this pass's callers either. ---- */
void ComputeFadeBlend(CSTGLFO *self, STGLFOPrecomputed *pc, float ccMod)
{
	int idx1 = BlendKnobIndex(pc->fadeV, self->fade);
	int idx2 = BlendKnobIndex(ccMod, idx1);
	if (idx2 == 0) {
		pc->fadeIsZeroFlag = 1.0f;
		pc->fadeSeconds = 0.0f;
		return;
	}
	float scaled = CSTGTables::kLFOFadeTimeSeconds[idx2] * CSTGAudioBusManager::sInstance->busGainReciprocal;
	int truncated = static_cast<int>(scaled);
	pc->fadeSeconds = 1.0f / static_cast<float>(truncated);
	pc->fadeIsZeroFlag = 0.0f;
}

/* ---- MIDI-tempo-sync base note: BlendKnobIndex(-V, baseNote, 9) (no
 * CC-mod 2nd stage in EITHER real caller) -> period/reciprocal.
 * PrecomputeBaseNoteAndTempo and HandleUpdateTempoPeriod both inline
 * this same computation with 2 confirmed-DIFFERENT final-reciprocal
 * instruction sequences (`fdivr .rodata.cst4[1.0]` vs `fld1;fdiv`) --
 * both compute the mathematically identical `1.0f/period`, so sharing
 * this one helper is faithful, not a simplification. ---- */
int BlendBaseNoteAndComputePeriod(const CSTGLFO *self, float negatedV, float *outPeriod, float *outReciprocal)
{
	int idx = BlendKnobIndex(negatedV, self->midiTempoSyncBaseNote, 9);
	int rawTicks = static_cast<int>(self->midiTempoSyncTimes) * static_cast<int>(CSTGTempo::sBaseNoteClockTable[idx]);
	float period = static_cast<float>(rawTicks) * kLFOTempoPeriodScale;
	*outPeriod = period;
	*outReciprocal = 1.0f / period;
	return idx;
}

void PropagateStopToVoices(const CSTGLFO *self, CSTGPatchMessageContext &ctx, const STGLFOPrecomputed *pc)
{
	int propagated = ((pc->flags & 0x1) != 0) ? -1 : 0;
	char *quadTableBase = QuadTableBase(self);
	void *entry = FirstActiveVoiceEntry(ctx);
	while (entry) {
		CSTGVoice *voice = *reinterpret_cast<CSTGVoice **>(static_cast<char *>(entry) + 8);
		unsigned short note = *reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(voice) + 4);
		STGLFOSubRateParamsSlice *slice = SliceFor(quadTableBase, note);
		slice->stop = propagated;
		entry = *reinterpret_cast<void **>(entry);
	}
}

/* Locates this voice's own STGLFOPrecomputed instance via a
 * voice-reachable formula -- used by InitVoice/SetSubRateParamsOnRestart,
 * neither of which receives a CSTGPatchMessageContext (so the usual
 * `ctx.precomputedBaseOffset + slotInfo->precomputedSlotIndex` formula,
 * LocatePrecomputed() above, isn't available). Confirmed real via both
 * functions' own identical instruction sequence (voice->+0x8->+0x4 byte
 * *5*512 + voice->+0xc + slotInfo->precomputedSlotIndex + 0x68);
 * intermediate fields not independently named elsewhere in this
 * project -- raw offset access, same "confirmed real, struct not fully
 * modeled" precedent used throughout this cluster. */
STGLFOPrecomputed *LocatePrecomputedViaVoice(const CSTGLFO *self, const CSTGVoice *voice)
{
	const unsigned char *voiceRaw = reinterpret_cast<const unsigned char *>(voice);
	const unsigned char *voiceSub8 = *reinterpret_cast<unsigned char *const *>(voiceRaw + 0x8);
	unsigned int base = static_cast<unsigned int>(voiceSub8[4]) * 5u * 512u
		+ *reinterpret_cast<const unsigned int *>(voiceRaw + 0xc);
	return reinterpret_cast<STGLFOPrecomputed *>(base + self->_slotInfo->precomputedSlotIndex + 0x68);
}

} // namespace

/* ==================================================================
 * ShouldDelayCompensateRestart / GetKeySyncMasterLFO -- pure
 * integer/pointer control flow, zero float instructions in either
 * real body (bundled into the deferred set only via their dependency
 * on the knob-index-blend-adjacent methods below, not for any
 * branch-sense risk of their own). GetKeySyncMasterLFO's several
 * intermediate CSTGVoice/CSTGGlobal sub-structures (+0x64, +0x30,
 * +0x34, and their own +0x8/+0xb/+0x38/+0x46/+0x8c fields) are not
 * independently named elsewhere in this project -- raw offset access,
 * transcribed mechanically from a single careful objdump read (NOT
 * independently re-verified with the native harness the way the float
 * idiom above was -- lower confidence than the rest of this pass).
 * ================================================================== */

bool CSTGLFO::ShouldDelayCompensateRestart(CSTGVoice *voice)
{
	return GetKeySyncMasterLFO(voice) == nullptr;
}

STGLFOSubRateParamsSlice *CSTGLFO::GetKeySyncMasterLFO(CSTGVoice *voice)
{
	unsigned char *voiceRaw = reinterpret_cast<unsigned char *>(voice);
	unsigned char *sub64 = *reinterpret_cast<unsigned char **>(voiceRaw + 0x64);
	unsigned char *sentinel = *reinterpret_cast<unsigned char **>(
		reinterpret_cast<unsigned char *>(CSTGGlobal::sInstance) + 0x29c9fa8);

	auto SliceForNoteObj = [this](unsigned char *noteObj) -> STGLFOSubRateParamsSlice * {
		char *quadTableBase = QuadTableBase(this);
		unsigned short note = *reinterpret_cast<unsigned short *>(noteObj + 0x4);
		return SliceFor(quadTableBase, note);
	};
	auto KeySyncPath = [&]() -> STGLFOSubRateParamsSlice * {
		if (*reinterpret_cast<short *>(sub64 + 0x46) <= 0)
			return nullptr;
		unsigned char *sub34 = *reinterpret_cast<unsigned char **>(voiceRaw + 0x34);
		unsigned char *found = *reinterpret_cast<unsigned char **>(sub34 + 0x8c);
		if (!found || found == sentinel)
			return nullptr;
		return SliceForNoteObj(sub34);
	};

	if (sub64[0xb] == 2) {
		unsigned char *sub30 = *reinterpret_cast<unsigned char **>(voiceRaw + 0x30);
		unsigned char *found = *reinterpret_cast<unsigned char **>(sub30 + 0x8c);
		if (found && found != sentinel)
			return SliceForNoteObj(sub30);
		/* fall through to the SAME keySync-flag test every non-mode2
		 * entry also goes through, matching ground truth's own
		 * fallthrough jump target. */
	}
	if ((flags & 0x2) != 0)
		return KeySyncPath();

	/* Neither mode2-with-valid-target nor keySync: walk this voice's
	 * own 12-byte-stride "note group" list looking for another live
	 * member (mode != 4 and its own +0x8c set). */
	unsigned char groupIdx = sub64[0x8];
	unsigned char *groupBase = *reinterpret_cast<unsigned char **>(voiceRaw + 0xc)
		+ static_cast<unsigned int>(groupIdx) * 12u;
	unsigned char *node = *reinterpret_cast<unsigned char **>(groupBase + 0x44);
	if (!node)
		return nullptr;
	unsigned char *candidate = *reinterpret_cast<unsigned char **>(node + 0x8);
	if (candidate == voiceRaw)
		return nullptr;
	for (;;) {
		int mode = *reinterpret_cast<int *>(candidate + 0x38);
		unsigned char *found = (mode != 4) ? *reinterpret_cast<unsigned char **>(candidate + 0x8c) : nullptr;
		if (mode != 4 && found)
			return SliceForNoteObj(candidate);
		node = *reinterpret_cast<unsigned char **>(node);
		if (!node)
			return nullptr;
		candidate = *reinterpret_cast<unsigned char **>(node + 0x8);
		if (candidate == voiceRaw)
			return nullptr;
	}
}

/* ==================================================================
 * InitVoice / SetSubRateParamsOnRestart -- same lower-confidence
 * transcription note as GetKeySyncMasterLFO above (mechanical,
 * single-read, not harness-verified). Both call the 4 CSTGLFOBase
 * methods declared-only in oa_engine_init.h (SetupSubrateLFO/Restart/
 * SetSubRateParamsOnRestart -- confirmed real calls, bodies out of
 * this cluster's scope). Real ground truth's InitVoice ALSO makes an
 * unconditional virtual call through this project's placeholder
 * (never-dispatched) CSTGLFO vtable partway through (slot 0x80/4=32,
 * args this+voice) -- omitted here, matching the file header's own
 * "install vs dispatch" precedent for that same vtable.
 * ================================================================== */

void CSTGLFO::InitVoice(CSTGVoice &voice, CSTGVoiceInitialState &)
{
	char *quadTableBase = QuadTableBase(this);
	unsigned short note = *reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(&voice) + 4);
	STGLFOSubRateParamsSlice *slice = SliceFor(quadTableBase, note);
	STGLFOPrecomputed *pc = LocatePrecomputedViaVoice(this, &voice);

	*reinterpret_cast<int *>(RawFieldAt(slice, 0x20)) = frequencyAMS1Intensity;
	*reinterpret_cast<int *>(RawFieldAt(slice, 0x40)) = frequencyAMS2Intensity;
	*reinterpret_cast<int *>(RawFieldAt(slice, 0x60)) = frequencyAMS1ModIntensity;
	*reinterpret_cast<int *>(RawFieldAt(slice, 0x80)) = shapeAMSIntensity;

	unsigned char *voiceSub64 = *reinterpret_cast<unsigned char **>(reinterpret_cast<char *>(&voice) + 0x64);
	unsigned char flagsArg = voiceSub64[9];
	auto *lfoBaseThis = reinterpret_cast<CSTGLFOBase *>(reinterpret_cast<char *>(this) + 0xc);
	lfoBaseThis->SetupSubrateLFO(slice, &voice, pc->waveform, flagsArg);

	slice->stop = ((pc->flags & 0x1) != 0) ? -1 : 0;
	slice->shape = pc->shapeRaw;
	slice->freq = pc->freqResult;
	slice->tempoRate = pc->tempoPeriod;
	slice->tempoDenom = *reinterpret_cast<int *>(&pc->tempoReciprocal);

	lfoBaseThis->Restart(slice, &voice, true);
}

void CSTGLFO::SetSubRateParamsOnRestart(STGLFOSubRateParamsSlice *slice, CSTGVoice *voice, bool arg)
{
	STGLFOSubRateParamsSlice *master = GetKeySyncMasterLFO(voice);
	unsigned char *sliceRaw = reinterpret_cast<unsigned char *>(slice);
	if (master) {
		unsigned char *masterRaw = reinterpret_cast<unsigned char *>(master);
		static const int kCopiedOffsets[] = { 0xc0, 0xd0, 0x170, 0x1e0, 0x150, 0x180, 0xf0, 0xe0 };
		for (int off : kCopiedOffsets)
			*reinterpret_cast<unsigned int *>(sliceRaw + off) = *reinterpret_cast<unsigned int *>(masterRaw + off);
		return;
	}

	STGLFOPrecomputed *pc = LocatePrecomputedViaVoice(this, voice);
	*reinterpret_cast<unsigned int *>(sliceRaw + 0xc0) = 0;
	*reinterpret_cast<int *>(sliceRaw + 0xd0) = pc->delayTicks;
	*reinterpret_cast<float *>(sliceRaw + 0xe0) = pc->fadeSeconds;
	*reinterpret_cast<float *>(sliceRaw + 0xf0) = pc->fadeIsZeroFlag;

	auto *lfoBaseThis = reinterpret_cast<CSTGLFOBase *>(reinterpret_cast<char *>(this) + 0xc);
	lfoBaseThis->SetSubRateParamsOnRestart(slice, voice, arg);
}

/* ==================================================================
 * UpdateStop -- field store (this->flags bit0) + pc mirror + gated
 * propagation, same shape as the already-reconstructed UpdateKeySync/
 * UpdateMIDITempoSync. Genuinely missing from BOTH the original
 * 64-method pass AND the 19-method deferred list in oa_lfo.h's file
 * header (a real gap in that earlier survey, not a duplicate) --
 * zero float instructions, no branch-sense risk.
 * ================================================================== */

void CSTGLFO::UpdateStop(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	flags = static_cast<unsigned char>((flags & ~0x1u) | ((newVal.value != 0) ? 0x1u : 0u));

	STGLFOPrecomputed *pc = LocatePrecomputed(this, ctx);
	bool stopVal = (flags & 0x1) != 0;
	pc->flags = static_cast<unsigned char>((pc->flags & ~0x1u) | (stopVal ? 0x1u : 0u));

	if (CallDisplayPredicate(ctx))
		PropagateStopToVoices(this, ctx, pc);
}

/* ==================================================================
 * UpdateFrequency / UpdateFrequencyFine / HandleUpdateFrequency --
 * all 3 share the IDENTICAL recompute tail (confirmed via a byte-level
 * diff of all 3 real bodies): live-update gate, then
 * ComputeFreqBlend + PropagateFreqToVoices with `ccMod` sourced from
 * ctx.paramConstantsTable+0x394. UpdateFrequency stores the incoming
 * value as `this->frequency` (the byte knob BlendKnobIndex itself
 * reads); UpdateFrequencyFine stores it as `this->frequencyFine`
 * instead (a plain mirror -- confirmed NOT read by the blend, which
 * still uses `this->frequency` unchanged) but performs the EXACT SAME
 * recompute regardless; HandleUpdateFrequency stores nothing at all,
 * just re-triggers the recompute (called elsewhere, e.g. after a
 * waveform/tempo-sync change makes the cached frequency stale).
 * ================================================================== */

void CSTGLFO::UpdateFrequency(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	frequency = static_cast<unsigned char>(newVal.value);
	if (!CallDisplayPredicate(ctx))
		return;
	STGLFOPrecomputed *pc = LocatePrecomputed(this, ctx);
	float ccMod = ResolveTableCCMod(ctx, pc, 0x394);
	float freq = ComputeFreqBlend(this, pc, ccMod);
	PropagateFreqToVoices(this, ctx, freq);
}

void CSTGLFO::UpdateFrequencyFine(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	frequencyFine = newVal.value;
	if (!CallDisplayPredicate(ctx))
		return;
	STGLFOPrecomputed *pc = LocatePrecomputed(this, ctx);
	float ccMod = ResolveTableCCMod(ctx, pc, 0x394);
	float freq = ComputeFreqBlend(this, pc, ccMod);
	PropagateFreqToVoices(this, ctx, freq);
}

void CSTGLFO::HandleUpdateFrequency(CSTGPatchMessageContext &ctx)
{
	if (!CallDisplayPredicate(ctx))
		return;
	STGLFOPrecomputed *pc = LocatePrecomputed(this, ctx);
	float ccMod = ResolveTableCCMod(ctx, pc, 0x394);
	float freq = ComputeFreqBlend(this, pc, ccMod);
	PropagateFreqToVoices(this, ctx, freq);
}

/* ==================================================================
 * UpdateFade / UpdateDelay -- the ORIGINAL harness target (UpdateFade)
 * and its structural sibling. Both read (never write) their own "V"
 * precomputed field; UpdateDelay additionally has the 2nd CC-mod
 * blend stage UpdateFade does NOT (confirmed: UpdateFade's real body
 * has no `test .../cmovne` at all). Neither propagates to any active
 * voice (confirmed: no active-voice-list walk in either real body).
 * ================================================================== */

void CSTGLFO::UpdateFade(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	fade = static_cast<unsigned char>(newVal.value);
	if (!CallDisplayPredicate(ctx))
		return;
	STGLFOPrecomputed *pc = LocatePrecomputed(this, ctx);
	ComputeFadeBlend(this, pc, 0.0f);
}

void CSTGLFO::UpdateDelay(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	delay = static_cast<unsigned char>(newVal.value);
	if (!CallDisplayPredicate(ctx))
		return;
	STGLFOPrecomputed *pc = LocatePrecomputed(this, ctx);
	float ccMod = ResolveTableCCMod(ctx, pc, 0x3ac);
	ComputeDelayBlend(this, pc, ccMod);
}

/* ==================================================================
 * HandleUpdateTempoPeriod -- confirmed real, called unconditionally by
 * both UpdateMIDITempoSyncTimes/UpdateMIDITempoSyncBaseNote (already
 * implemented, see above). Reuses freqV (pc->+0x1c) as its own "V"
 * input -- genuinely the SAME field the frequency recompute path
 * reads, confirmed via 2 independent objdump reads (this function AND
 * PrecomputeData's own inline base-note section below both read
 * `[esi+0x1c]`, no field ever named "tempoV" separately exists).
 * ================================================================== */

void CSTGLFO::HandleUpdateTempoPeriod(CSTGPatchMessageContext &ctx)
{
	if (!CallDisplayPredicate(ctx))
		return;
	STGLFOPrecomputed *pc = LocatePrecomputed(this, ctx);
	float period, reciprocal;
	int idx = BlendBaseNoteAndComputePeriod(this, -pc->freqV, &period, &reciprocal);
	pc->baseNoteBlendIndex = idx;
	pc->tempoPeriod = period;
	pc->tempoReciprocal = reciprocal;

	char *quadTableBase = QuadTableBase(this);
	void *entry = FirstActiveVoiceEntry(ctx);
	while (entry) {
		CSTGVoice *voice = *reinterpret_cast<CSTGVoice **>(static_cast<char *>(entry) + 8);
		unsigned short note = *reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(voice) + 4);
		STGLFOSubRateParamsSlice *slice = SliceFor(quadTableBase, note);
		slice->tempoRate = period;
		slice->tempoDenom = *reinterpret_cast<int *>(&reciprocal);
		entry = *reinterpret_cast<void **>(entry);
	}
}

/* ==================================================================
 * The 4 Precompute*(STGLFOPrecomputed*[, float]) primitives --
 * per-note-message-processing counterparts to the Update* methods
 * above, called from PrecomputeData (this pass) and HandleCC's own
 * per-CC-tick dispatch (this pass). PrecomputeFreqPlusCC/
 * PrecomputeDelayTicks take their "ccMod" directly as a parameter
 * (no paramConstantsTable read); PrecomputeFade has no 2nd stage at
 * all (ccMod=0.0f, matching UpdateFade); PrecomputeBaseNoteAndTempo
 * has no 2nd stage either (see HandleUpdateTempoPeriod's own comment).
 * ================================================================== */

void CSTGLFO::PrecomputeFreqPlusCC(STGLFOPrecomputed *pc, float ccValue)
{
	ComputeFreqBlend(this, pc, ccValue);
}

void CSTGLFO::PrecomputeBaseNoteAndTempo(STGLFOPrecomputed *pc)
{
	float period, reciprocal;
	int idx = BlendBaseNoteAndComputePeriod(this, -pc->freqV, &period, &reciprocal);
	pc->baseNoteBlendIndex = idx;
	pc->tempoPeriod = period;
	pc->tempoReciprocal = reciprocal;
}

void CSTGLFO::PrecomputeDelayTicks(STGLFOPrecomputed *pc, float ccValue)
{
	ComputeDelayBlend(this, pc, ccValue);
}

void CSTGLFO::PrecomputeFade(STGLFOPrecomputed *pc)
{
	ComputeFadeBlend(this, pc, 0.0f);
}

/* ==================================================================
 * PrecomputeData -- the big per-patch-message baseline-establishment
 * pass: resets all 3 "V" fields to 0.0f, mirrors this->flags bit0/
 * waveform/shape into pc, then recomputes frequency, base-note/tempo,
 * fade, and delay in that order using the JUST-ZEROED V fields (so
 * each blend's own 1st stage degenerates to "= knob unchanged" by
 * construction -- confirmed via the real body's own x87 stack reuse
 * of the SAME already-0.0-valued register across all 4 recomputes,
 * not a simplification made here).
 *
 * One genuinely surprising, independently-confirmed (2 separate reads
 * of the real bytes) fact: PrecomputeData's own inline fade section
 * reads its 2nd-stage "wants CC mod" constant from
 * `ctx.paramConstantsTable+0x3ac` -- the SAME offset UpdateDelay/
 * ToneAdjustDelayRelative/HandleCC's delay branch all use for DELAY's
 * own AMS-intensity constant, not a separate fade-specific slot (which
 * would be the naive expectation given standalone UpdateFade/
 * PrecomputeFade/ToneAdjustFadeRelative never read any such constant
 * at all). Transcribed exactly as observed rather than "corrected" to
 * match the naive expectation.
 * ================================================================== */

void CSTGLFO::PrecomputeData(CSTGPatchMessageContext &ctx)
{
	STGLFOPrecomputed *pc = LocatePrecomputed(this, ctx);

	pc->freqV = 0.0f;
	pc->delayV = 0.0f;
	pc->fadeV = 0.0f;

	pc->flags = static_cast<unsigned char>((pc->flags & ~0x1u) | (flags & 0x1u));
	pc->waveform = waveform;
	pc->shapeRaw = shape;

	float freqCcMod = ResolveTableCCMod(ctx, pc, 0x394);
	ComputeFreqBlend(this, pc, freqCcMod);

	float period, reciprocal;
	int idx = BlendBaseNoteAndComputePeriod(this, -pc->freqV, &period, &reciprocal);
	pc->baseNoteBlendIndex = idx;
	pc->tempoPeriod = period;
	pc->tempoReciprocal = reciprocal;

	/* Fade section -- SHARES the delay AMS-mod-intensity table offset
	 * (0x3ac), see file comment above. */
	float fadeCcMod = ResolveTableCCMod(ctx, pc, 0x3ac);
	ComputeFadeBlend(this, pc, fadeCcMod);

	float delayCcMod = ResolveTableCCMod(ctx, pc, 0x3ac);
	ComputeDelayBlend(this, pc, delayCcMod);
}

/* ==================================================================
 * HandleCC -- per-CC dispatch. Real body only implements 2 of the 3
 * CCs HandlesCC's own table nominally covers (0x4c/76="frequency"/
 * "Rate", 0x4e/78="Delay"); 0x4d/77 ("Fade", per HandlesCC's own
 * 3-entry span 0x4c..0x4e) is NOT dispatched at all here -- consistent
 * with `kLFOHandlesCCTable`'s real contents being unmodeled rather
 * than a bug (the table entry for 0x4d may simply be `false` in
 * ground truth). `ccVal.field4` is the incoming CC's own converted
 * float value, used as the 2nd-stage blend input directly (no
 * paramConstantsTable read, unlike the Update* (or PrecomputeData)
 * equivalents above).
 * ================================================================== */

void CSTGLFO::HandleCC(CSTGPatchMessageContext &ctx, unsigned char cc, const CSTGControllerValue &ccVal)
{
	if (cc != 0x4c && cc != 0x4e)
		return;

	STGLFOPrecomputed *pc = LocatePrecomputed(this, ctx);
	float ccMod = ((pc->flags & 0x2) != 0) ? ccVal.field4 : 0.0f;

	if (cc == 0x4e) {
		ComputeDelayBlend(this, pc, ccMod);
		return;
	}

	float freq = ComputeFreqBlend(this, pc, ccMod);
	PropagateFreqToVoices(this, ctx, freq);
}

/* ==================================================================
 * ToneAdjust* family -- per-note override: writes the PRECOMPUTED "V"/
 * raw slot directly (the delta/override value itself, not a persistent
 * instance knob) then immediately recomputes, mirroring the Update*
 * family's roles exactly reversed (Update* stores the KNOB and reads a
 * pre-existing V; ToneAdjust* stores the V and reads a pre-existing
 * KNOB). ToneAdjustFreqRelative is the one exception with a live-update
 * gate AND an unconditional trailing HandleUpdateTempoPeriod call
 * (confirmed real -- both control-flow paths converge on the same
 * call site) -- surprising but faithfully transcribed rather than
 * "corrected", same policy as PrecomputeData's own surprising +0x3ac
 * reuse above.
 * ================================================================== */

void CSTGLFO::ToneAdjustFreqRelative(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	STGLFOPrecomputed *pc = LocatePrecomputed(this, ctx);
	pc->freqV = *reinterpret_cast<float *>(&newVal.value);

	if (CallDisplayPredicate(ctx)) {
		float ccMod = ResolveTableCCMod(ctx, pc, 0x394);
		float freq = ComputeFreqBlend(this, pc, ccMod);
		PropagateFreqToVoices(this, ctx, freq);
	}
	HandleUpdateTempoPeriod(ctx);
}

void CSTGLFO::ToneAdjustFadeRelative(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	STGLFOPrecomputed *pc = LocatePrecomputed(this, ctx);
	pc->fadeV = *reinterpret_cast<float *>(&newVal.value);
	ComputeFadeBlend(this, pc, 0.0f);
}

void CSTGLFO::ToneAdjustDelayRelative(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	STGLFOPrecomputed *pc = LocatePrecomputed(this, ctx);
	pc->delayV = *reinterpret_cast<float *>(&newVal.value);
	float ccMod = ResolveTableCCMod(ctx, pc, 0x3ac);
	ComputeDelayBlend(this, pc, ccMod);
}

void CSTGLFO::ToneAdjustStop(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	STGLFOPrecomputed *pc = LocatePrecomputed(this, ctx);
	unsigned char *newValRaw = reinterpret_cast<unsigned char *>(&newVal);
	/* newVal.value==-1 OR bit0 of newVal+0x14 set -> "unspecified",
	 * use this->flags bit0 (the persistent stop state) instead of the
	 * incoming value -- +0x14's real meaning beyond this one bit is
	 * unconfirmed (STGConvertedParam's own +0x14..+0x17 span, still
	 * unrecovered in oa_global.h). */
	bool useCurrent = (newVal.value == -1) || ((newValRaw[0x14] & 1) != 0);
	bool stopVal = useCurrent ? ((flags & 0x1) != 0) : (newVal.value != 0);
	pc->flags = static_cast<unsigned char>((pc->flags & ~0x1u) | (stopVal ? 0x1u : 0u));
	if (CallDisplayPredicate(ctx))
		PropagateStopToVoices(this, ctx, pc);
}

void CSTGLFO::ToneAdjustWaveform(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	STGLFOPrecomputed *pc = LocatePrecomputed(this, ctx);
	unsigned char *newValRaw = reinterpret_cast<unsigned char *>(&newVal);
	int wf = ((newValRaw[0x14] & 1) != 0) ? static_cast<int>(waveform) : newVal.value;
	pc->waveform = wf;
	HandleWaveformChanged(ctx, wf);
}

void CSTGLFO::ToneAdjustShape(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	STGLFOPrecomputed *pc = LocatePrecomputed(this, ctx);
	unsigned char *newValRaw = reinterpret_cast<unsigned char *>(&newVal);
	int value = ((newValRaw[0x14] & 1) != 0) ? shape : newVal.value;
	pc->shapeRaw = value;

	char *quadTableBase = QuadTableBase(this);
	void *entry = FirstActiveVoiceEntry(ctx);
	while (entry) {
		CSTGVoice *voice = *reinterpret_cast<CSTGVoice **>(static_cast<char *>(entry) + 8);
		unsigned short note = *reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(voice) + 4);
		STGLFOSubRateParamsSlice *slice = SliceFor(quadTableBase, note);
		slice->shape = value;
		entry = *reinterpret_cast<void **>(entry);
	}
}
