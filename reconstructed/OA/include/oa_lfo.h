// SPDX-License-Identifier: GPL-2.0
#ifndef OA_LFO_H
#define OA_LFO_H

/*
 * oa_lfo.h  -  CSTGLFO, the per-voice-component front-panel LFO
 * (rate/waveform/shape/fade/delay/key-sync/MIDI-tempo-sync, all AMS-
 * modulatable), plus the small pieces of its real base class
 * CSTGLFOBase this cluster's own methods call into.
 *
 * Ground truth: `CSTGLFO::*` / `CSTGLFOBase::*`, OA_real.ko
 * `.text+0x0013ebf0`..`.text+0x001315c0` (main contiguous cluster)
 * plus the trivial Get* /dtor/`HandlesCC` accessors, each in its own
 * `.text._ZN[K]7CSTGLFO...` COMDAT section (weak linkage, same
 * layout convention as CSTGADSRBase -- see oa_adsr_base.h). Every
 * body transcribed from `objdump -dr -M intel` against the real
 * `OA.ko` object directly (not the Ghidra decompiler), same as
 * CSTGADSRBase.
 *
 * === Scope of THIS pass (64 of CSTGLFO's 83 real methods) ===
 *
 * CSTGLFO is genuinely larger and more heterogeneous than
 * CSTGADSRBase: on top of the same Get* /Update* /AMS-propagation
 * shape, it owns waveform selection, MIDI-tempo-sync, and a fade/
 * delay envelope, several of which recompute a discrete "knob index"
 * via a shared x87 idiom (fucomip-based branch senses very similar in
 * shape to the one a native-execution harness caught being backwards
 * on the FIRST hand-derivation of CSTGADSRBase's own
 * ApplyIntensityBlend -- see oa_adsr_base.h's file header). Rather
 * than risk shipping an unverified guess at THAT idiom's branch
 * senses for this cluster too, this pass deliberately reconstructs
 * only the methods whose control flow is either straight-line or
 * gated on plain integer/pointer comparisons (no float branch-sense
 * risk) -- 64 of 83 methods, all independently confirmed safe by a
 * full instruction-level read of each one (documented per-function
 * below). The remaining 19 are DELIBERATELY DEFERRED to a follow-up
 * pass that should use the same mmap-and-execute-the-real-bytes
 * native harness CSTGADSRBase's own PrecomputeAttackTimePlusCC pass
 * used, rather than hand-derive x87 branch senses a second time:
 *
 *   - 6 share one "knob-index blend" x87 idiom (near-identical
 *     instruction sequence, offsets differ): UpdateFrequency,
 *     UpdateFrequencyFine, UpdateFade, UpdateDelay,
 *     HandleUpdateFrequency, HandleUpdateTempoPeriod.
 *   - PrecomputeData, HandleCC (the two big per-tick/per-CC dispatch
 *     bodies -- also depend on the knob-index idiom above).
 *   - The 6-method ToneAdjust* family (ToneAdjustFreqRelative,
 *     ToneAdjustFadeRelative, ToneAdjustDelayRelative,
 *     ToneAdjustStop, ToneAdjustWaveform, ToneAdjustShape).
 *   - GetKeySyncMasterLFO, ProcessSubRate, InitVoice,
 *     SetSubRateParamsOnRestart (CSTGLFO's own override).
 *   - PrecomputeFreqPlusCC, PrecomputeBaseNoteAndTempo,
 *     PrecomputeDelayTicks, PrecomputeFade.
 *
 * Also NOT modeled (real, but out of THIS class's own scope): the
 * three large CSTGLFOBase methods this cluster's DEFERRED methods
 * would call into (SetupSubrateLFO, UpdateRandomValue, Restart) --
 * none of the 64 methods reconstructed here call them, so no extern
 * dependency is introduced. The `_ZThn12_*` this-adjusting thunks
 * (CSTGLFO genuinely multiply-inherits CSTGParamsOwner at offset 0
 * and CSTGLFOBase at offset +0xc, confirmed via the real dtor's own
 * two vtable-pointer writes -- see below) are pure calling-convention
 * plumbing (`sub eax,0xc; jmp <primary entry>`), not reconstructed
 * as distinct symbols, same "install vs dispatch" treatment as
 * CSTGADSRBase's own placeholder vtable.
 *
 * === Real per-instance field layout (confirmed via every Get* and
 * Update* /HandleWaveformChanged/HandleStopChanged access in the 64
 * methods below) ===
 *
 *   +0x00  vtable ptr (manual placeholder, see _ZTV7CSTGLFO below)
 *   +0x04  int    _unconfirmedFlags  (matches CSTGADSRBase's own
 *                 field at this offset; no reader confirmed in this
 *                 cluster either)
 *   +0x08  CSTGComponentSlotInfo*  _slotInfo  (reused from
 *                 oa_adsr_base.h -- IDENTICAL offsets confirmed
 *                 independently by this cluster's own
 *                 subRateBaseIndex/precomputedSlotIndex reads)
 *   +0x10  int8_t startPhase          (CSTGLFOBase::UpdateStartPhase
 *                 writes this at the ADJUSTED `this+0xc`, i.e. THIS
 *                 field is actually the CSTGLFOBase sub-object's own
 *                 +0x4 -- see the multiple-inheritance note above;
 *                 modeled here as a plain CSTGLFO field at the real
 *                 total-object offset it lands on)
 *   +0x11  int    shape                (both value+displayValue getter)
 *   +0x15  int    shapeAMSIntensity    (both value+displayValue)
 *   +0x19  int8_t shapeAMSSource       (value only)
 *   +0x1a  int    offset               (both value+displayValue)
 *   +0x1e  int    frequencyFine        (both value+displayValue)
 *   +0x23  int8_t waveform             (value only; real range 0..17,
 *                 see HandleWaveformChanged)
 *   +0x24  uint8_t frequency           (value only; a discrete 0..99
 *                 "rate knob" index -- NOT a float, confirmed by
 *                 GetFrequency's own `movzx`)
 *   +0x25  uint8_t delay               (value only, 0..99 knob index)
 *   +0x26  uint8_t fade                (value only, 0..99 knob index)
 *   +0x27  uint8_t flags: bit0=stop, bit1=keySync, bit2=midiTempoSync
 *                 (GetStop/GetKeySync/GetMIDITempoSync each mask a
 *                 different bit of this ONE byte)
 *   +0x28  int8_t midiTempoSyncBaseNote (value only, indexes
 *                 CSTGTempo::sBaseNoteClockTable, 11 entries)
 *   +0x29  uint8_t midiTempoSyncTimes   (value only)
 *   +0x2a  int    frequencyAMS1Intensity     (both value+displayValue)
 *   +0x2e  int8_t frequencyAMS1Source        (value only)
 *   +0x2f  int    frequencyAMS2Intensity     (both value+displayValue)
 *   +0x33  int8_t frequencyAMS2Source        (value only)
 *   +0x34  int    frequencyAMS1ModIntensity  (both value+displayValue)
 *   +0x38  int8_t frequencyAMS1ModSource     (value only)
 *   (total confirmed size >= 0x39 bytes)
 *
 * === Two DISTINCT per-note working structures (do not confuse) ===
 *
 * 1. STGLFOPrecomputed, located the SAME way as CSTGADSRBase's own
 *    STGADSRBasePrecomputed (`ctx.precomputedBaseOffset +
 *    _slotInfo->precomputedSlotIndex`) -- a scratch copy for the
 *    note currently being processed by the CURRENT patch message.
 *    Only the two offsets this pass's own methods touch are named;
 *    the rest (this class plainly has many more, used by the
 *    deferred PrecomputeData/HandleCC/Precompute* family) are left
 *    as unrecovered padding rather than guessed.
 *
 * 2. STGLFOSubRateParamsSlice, located via the SAME "quad table"
 *    formula CSTGADSRBase's own PropagateAMSSourceAddress uses
 *    (`*(CSTGVoiceModelManager::sInstance+4) + _slotInfo->
 *    subRateBaseIndex + ((note&3)+(note>>2)*0xcc0)*4`) -- ONE such
 *    slice per ACTIVE VOICE (not per component instance), which is
 *    why every Update* propagation method walks
 *    `ctx.activeVoiceListTable` and writes the SAME new value into
 *    every active voice's own slice. This is the type
 *    `STGLFOSubRateParamsSlice*` that CSTGLFOBase's own
 *    SetupSubrateLFO/Restart/UpdateRandomValue (out of scope) and
 *    CSTGLFO's own GetOutput/UpdateOutput/AdvanceFadeEnv (in scope)
 *    all operate on directly.
 */

#include "oa_global.h"
#include "oa_engine.h"
#include "oa_adsr_base.h"	/* reuses CSTGComponentSlotInfo / CSTGPatchMessageContext
				 * verbatim -- both confirmed identical-layout in this
				 * cluster's own disassembly (see field comments above). */
#include "oa_engine_init.h"	/* CSTGLFOTables, STGLFOSubRateParams (the InitializeQuad-
				 * populated default block), CSTGMIDIClockSync. */

/*
 * Per-note scratch struct, see file header. Only the 2 offsets this
 * pass's methods read/write are named.
 */
struct STGLFOPrecomputed {
	unsigned char _unrecovered_head[0x2c];	/* +0x00..+0x2b, unconfirmed --
						 * populated by the deferred
						 * PrecomputeData/Precompute* family. */
	int shapeRaw;				/* +0x2c, confirmed: UpdateShape's own
						 * mirror write, read by nothing in
						 * this pass (consumed by deferred
						 * PrecomputeData/HandleCC). */
	unsigned char flags;			/* +0x30, confirmed: bit1 = "wants CC
						 * modulation" (SetWantsCCMod). */
	unsigned char _unrecovered_tail[3];	/* +0x31..+0x33, unconfirmed. */
};

/*
 * Per-active-voice working slice, see file header. Only the offsets
 * this pass's own methods touch are named; this class plainly has
 * many more fields (waveform table pointers etc, several already
 * named by lfo_stepseq_quad.cpp's own InitializeQuad) used by the
 * deferred methods -- left unrecovered rather than guessed.
 */
/*
 * `packed`: this project's usual "typed field + raw unsigned char gap
 * array" idiom relies on the compiler placing each typed field at
 * NATURAL alignment right after its preceding gap -- true for every
 * other such struct in this codebase because they only ever mix
 * 4-byte-or-smaller types. This one is the first to also carry a
 * genuinely target-pointer-width field (`lfoTables`, stored as
 * `unsigned int` -- see its own comment below for why NOT `void*`);
 * without `packed`, a stray 8-byte-aligned host type earlier in the
 * struct would silently shift every later confirmed offset. `packed`
 * makes the struct's actual byte layout unconditionally match the
 * comments regardless of host/target, which is what every raw-offset
 * access in lfo_component.cpp (and this struct's own confirmed
 * offsets) assumes.
 */
struct __attribute__((packed)) STGLFOSubRateParamsSlice {
	float output;			/* +0x000, GetOutput/UpdateOutput/AdvanceFadeEnv */
	signed char startPhase;		/* +0x004, CSTGLFOBase::UpdateStartPhase */
	unsigned char _gap1[0xdb];	/* +0x005..+0x0df, unconfirmed */
	float fadeRate;			/* +0x0e0, AdvanceFadeEnv (per-tick increment) */
	unsigned char _gap2[0xc];	/* +0x0e4..+0x0ef, unconfirmed */
	float fadeProgress;		/* +0x0f0, AdvanceFadeEnv (0..1, clamped) */
	unsigned char _gap3[0xc];	/* +0x0f4..+0x0ff, unconfirmed */
	int stop;			/* +0x100, HandleStopChanged/UpdateStop (0/-1 bool) */
	unsigned char _gap4[0xc];	/* +0x104..+0x10f, unconfirmed */
	float freq;			/* +0x110, UpdateFrequency/UpdateFrequencyFine/
					 * HandleUpdateFrequency (deferred; offset
					 * confirmed by their own propagation loop) */
	unsigned char _gap5[0xc];	/* +0x114..+0x11f, unconfirmed */
	int midiTempoSync;		/* +0x120, UpdateMIDITempoSync (0/-1 bool) */
	unsigned char _gap6[0xc];	/* +0x124..+0x12f, unconfirmed */
	float tempoRate;		/* +0x130, HandleUpdateTempoPeriod (deferred) */
	unsigned char _gap7[0xc];	/* +0x134..+0x13f, unconfirmed */
	int tempoDenom;			/* +0x140, HandleUpdateTempoPeriod (deferred) */
	unsigned char _gap8[0x4c];	/* +0x144..+0x18f, unconfirmed */
	/* +0x190: real target field is a 4-byte pointer
	 * (InitializeQuad default / HandleWaveformChanged's own
	 * CSTGLFOTables sub-table pointer). Stored as `unsigned int`
	 * (raw bit pattern, "ToU32"-style -- same convention
	 * lfo_stepseq_quad.cpp already established for this exact
	 * field) rather than `void*` so this struct's byte layout is
	 * identical on the real 32-bit target and a 64-bit host verify
	 * build; cast through `reinterpret_cast<void*>(uintptr_t)` at
	 * call sites that need to dereference it (none do in this
	 * pass -- only written, never read back as a pointer). */
	unsigned int lfoTables;	/* +0x190 */
	unsigned char _gap9[0xc];	/* +0x194..+0x19f, unconfirmed */
	int waveformFlagsA;		/* +0x1a0, HandleWaveformChanged */
	unsigned char _gap10[0xc];	/* +0x1a4..+0x1af, unconfirmed */
	float offset;			/* +0x1b0, UpdateOffset */
	unsigned char _gap11[0xc];	/* +0x1b4..+0x1bf, unconfirmed */
	int shape;			/* +0x1c0, UpdateShape */
	unsigned char _gap12[0xc];	/* +0x1c4..+0x1cf, unconfirmed */
	unsigned int waveformConstD;	/* +0x1d0, HandleWaveformChanged (literal 0x3f800000
					 * = 1.0f in the "extended waveform" branch) */
	unsigned char _gap13[0xc];	/* +0x1d4..+0x1df, unconfirmed */
	unsigned int waveformMaskedE;	/* +0x1e0, HandleWaveformChanged (= this->+0x170 & 0x3fffffff) */
	unsigned char _gap14[0xc];	/* +0x1e4..+0x1ef, unconfirmed */
	unsigned int waveformFlagsF;	/* +0x1f0, HandleWaveformChanged (GetRandomFlagsForWaveform
					 * result, or 0 for a table waveform) */
	unsigned char _tailPad[0x200];	/* generous tail padding -- real total size not
					 * independently confirmed; every write in this
					 * pass stays within this bound. */
};

/*
 * External per-component-class registration tables (STGLFOParams,
 * sMessageHandlers, sValueGetters) -- confirmed real via relocation
 * (GetParamDescriptors/GetMessageHandlers/GetValueGetters), contents
 * out of scope (same "framework table, not modeled" precedent as
 * CSTGADSRBase's own paramConstantsTable).
 */
extern "C" unsigned char STGLFOParams[1092];
extern "C" unsigned char _ZN7CSTGLFO16sMessageHandlersE[168];
extern "C" unsigned char _ZN7CSTGLFO13sValueGettersE[168];

/* CSTGTables::kLFOKnobToFreq (101 floats) / kLFOFadeTimeSeconds (100
 * floats) -- confirmed real via CalculateFreq/UpdateFade relocations. */
namespace CSTGTables {
extern float kLFOKnobToFreq[101];
extern float kLFOFadeTimeSeconds[100];
}

/* CSTGTempo::sBaseNoteClockTable (11 uint16 entries) -- confirmed real
 * via HandleUpdateTempoPeriod's relocation (deferred method; declared
 * here since it's part of this class's own confirmed dependency set). */
namespace CSTGTempo {
extern unsigned short sBaseNoteClockTable[11];
}

/*
 * CSTGLFOBase's 3 small real methods this cluster calls into
 * (UpdateStartPhase/CalculateFreq/GetRandomFlagsForWaveform) are
 * declared in oa_engine_init.h (extending the pre-existing
 * CSTGLFOBase struct there) rather than redeclared here -- see that
 * header for the confirmed-real behavior of each:
 *   - UpdateStartPhase: `slice->startPhase = (int8_t)newVal.value`,
 *     confirmed effectively static (no CSTGLFOBase instance field
 *     touched). CSTGLFO::UpdateStartPhase forwards `this+0xc` (its
 *     own CSTGLFOBase sub-object) as the slice pointer.
 *   - CalculateFreq: linear-interpolates CSTGTables::kLFOKnobToFreq
 *     at index `(note + slice[+0x18]) & 0xff` between adjacent
 *     entries (blend weight `slice[+0x12]`, a float), scaled by
 *     `CSTGAudioBusManager::sInstance->busGainReciprocal` and a fixed
 *     `.rodata.cst4` constant. No branches at all in the real body --
 *     lowest-risk function in this whole cluster.
 *   - GetRandomFlagsForWaveform: pure 6-entry table lookup for
 *     waveform IDs 12..17, 0 outside that range (real table at
 *     `.rodata+0x4c264`, opaque payload, same treatment as
 *     HandlesCC's own table below).
 */

/*
 * Real vtable data (0xf0 bytes / 60 slots incl. offset-to-top + RTTI,
 * confirmed via `readelf -sW` against `_ZTV7CSTGLFO`). Zero-filled
 * placeholder -- see CSTGADSRBase's own file header for the "install
 * vs dispatch" rationale this project applies uniformly. Ground truth
 * genuinely multiply-inherits (primary base CSTGParamsOwner at offset
 * 0, secondary base CSTGLFOBase at offset +0xc -- confirmed by the
 * real destructor's own TWO vptr writes, `this[+0xc] = &_ZTV7CSTGLFO
 * [0xe0]` and `this[+0] = &_ZTV15CSTGParamsOwner[8]`, the latter being
 * the standard Itanium-ABI "reset to the immediate base's own
 * construction vtable" step every multi-level destructor performs);
 * modeled here as a single standalone (non-inheriting) class per this
 * project's established convention, same as CSTGADSRBase.
 */
extern "C" unsigned char _ZTV7CSTGLFO[0xf0];
/* _ZTV15CSTGParamsOwner already declared in oa_global.h (definition
 * lives in program_ctor.cpp, the first reconstructed TU to need it --
 * NOT redeclared/redefined here to avoid a duplicate-symbol link
 * error). */

class CSTGLFO {
public:
	/* No ground-truth CSTGLFO::CSTGLFO() symbol was found in
	 * OA_real.ko (likely inlined into an out-of-scope voice-model
	 * array constructor) -- this ctor's vtable install is inferred
	 * from the confirmed-real destructor's own reset-to-derived-class
	 * shape (mirror image of D0/D1's `this[+0]=&_ZTV15CSTGParamsOwner
	 * [8]`), NOT independently confirmed for the live/constructed
	 * state. Field defaults are UNKNOWN (no ctor body to read) and
	 * deliberately left uninitialized rather than guessed -- unlike
	 * CSTGADSRBase's own ctor, which had a real body to transcribe. */
	CSTGLFO() { *reinterpret_cast<void **>(this) = _ZTV7CSTGLFO + 8; }
	~CSTGLFO();

	int _unconfirmedFlags;			/* +0x04 */
	CSTGComponentSlotInfo *_slotInfo;	/* +0x08 */
	signed char startPhase;			/* +0x10 */
	int shape;				/* +0x11 */
	int shapeAMSIntensity;			/* +0x15 */
	signed char shapeAMSSource;		/* +0x19 */
	int offset;				/* +0x1a */
	int frequencyFine;			/* +0x1e */
	signed char waveform;			/* +0x23 */
	unsigned char frequency;		/* +0x24 */
	unsigned char delay;			/* +0x25 */
	unsigned char fade;			/* +0x26 */
	unsigned char flags;			/* +0x27, bit0=stop bit1=keySync bit2=midiTempoSync */
	signed char midiTempoSyncBaseNote;	/* +0x28 */
	unsigned char midiTempoSyncTimes;	/* +0x29 */
	int frequencyAMS1Intensity;		/* +0x2a */
	signed char frequencyAMS1Source;	/* +0x2e */
	int frequencyAMS2Intensity;		/* +0x2f */
	signed char frequencyAMS2Source;	/* +0x33 */
	int frequencyAMS1ModIntensity;		/* +0x34 */
	signed char frequencyAMS1ModSource;	/* +0x38 */

	/* ---- Virtual-slot overrides ---- */
	bool HandlesCC(unsigned char cc) const;
	void InitAMSSourceAddresses(CSTGVoice &voice);
	/* SetSubRateParamsOnRestart(STGLFOSubRateParamsSlice*, CSTGVoice*,
	 * bool) deliberately NOT declared here -- deferred, see file
	 * header (shares no risk with the knob-index idiom, but was not
	 * read in this pass; left for the same follow-up). */
	static void InitializeQuad(void *unused, STGLFOSubRateParams *quad);

	/* Registration accessors, all trivial constant/table-pointer
	 * returns -- CSTGADSRBase doesn't override these 6 (resolves to
	 * an out-of-scope base default instead); CSTGLFO does. */
	static int GetId();
	static const char *GetName();
	static int GetNumParams();
	static const void *GetParamDescriptors();
	static const void *GetMessageHandlers();
	static const void *GetValueGetters();

	/* ---- Quad/sub-rate ---- */
	STGLFOSubRateParamsSlice *GetOutput(int note, int subSlot);
	void UpdateOutput(STGLFOSubRateParamsSlice *slice, float in, bool active);
	void AdvanceFadeEnv(STGLFOSubRateParamsSlice *slice, unsigned int ticks);
	void PrepareSubRateAddressFixupTable(CSTGSubRateAddressFixupTable &table, unsigned long note);

	/* ---- Update* family (field store [+ display-predicate gate] +
	 * per-active-voice propagation) ---- */
	void UpdateShape(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateShapeAMSIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateOffset(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateKeySync(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateMIDITempoSync(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateMIDITempoSyncTimes(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateMIDITempoSyncBaseNote(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateStartPhase(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateWaveform(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void SetWantsCCMod(CSTGPatchMessageContext &ctx, bool wantsIt);

	/* HandleUpdateTempoPeriod(CSTGPatchMessageContext&) -- confirmed
	 * real, called unconditionally by both UpdateMIDITempoSyncTimes
	 * and UpdateMIDITempoSyncBaseNote above after their own field
	 * store. DELIBERATELY DEFERRED (shares the risky knob-index x87
	 * idiom, see file header) -- declared but not defined in this
	 * pass, same "confirmed real call, callee out of scope" treatment
	 * as e.g. CSTGVoice::GetAMSSourceAddress (oa_engine.h). Neither
	 * caller is exercised by this pass's own verify test, so no link
	 * dependency is introduced there. */
	void HandleUpdateTempoPeriod(CSTGPatchMessageContext &ctx);

	void UpdateFrequencyAMS1Intensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateFrequencyAMS2Intensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateFrequencyAMS1ModIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateFrequencyAMS1ModSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateFrequencyAMS2Source(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateFrequencyAMS1Source(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateShapeAMSSource(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);

	/* ---- Handle* family (waveform/stop change side effects) ---- */
	void HandleWaveformChanged(CSTGPatchMessageContext &ctx, int waveform);
	void HandleStopChanged(CSTGPatchMessageContext &ctx);

	/* ---- Get* family -- see file header; all 26 write
	 * CSTGParamsOwner::sValueGetterTemp and return a reference. ---- */
	STGConvertedParam &GetWaveform(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetStartPhase(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetShape(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetShapeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetShapeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFrequency(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFrequencyFine(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetStop(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOffset(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeySync(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFade(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDelay(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFrequencyAMS1Source(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFrequencyAMS1Intensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFrequencyAMS1ModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFrequencyAMS1ModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFrequencyAMS2Source(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFrequencyAMS2Intensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMIDITempoSync(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMIDITempoSyncBaseNote(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMIDITempoSyncTimes(CSTGPatchMessageContext &ctx);
};

#endif
