// SPDX-License-Identifier: GPL-2.0
/*
 * lfo_component.cpp  -  CSTGLFO (64 of 83 real methods) + the 3 small
 * CSTGLFOBase methods this cluster calls into. See include/oa_lfo.h
 * for the full field-layout table, the two distinct per-note working
 * structures, and the exact list of the 19 methods deliberately
 * deferred to a follow-up native-execution-harness pass.
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
	LocatePrecomputed(this, ctx)->_unrecovered_head[0x28] = waveform; /* precomp+0x28,
		confirmed real write, not independently named (outside this
		struct's 2 confirmed fields -- see oa_lfo.h). */
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
