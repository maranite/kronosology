// SPDX-License-Identifier: GPL-2.0
/*
 * oa_global.h  -  CSTGGlobal, the STG engine's central "global state" object.
 * Stage 3 (PLAN.md: "CSTGEngine, CSTGGlobal, the managers").
 *
 * CSTGGlobal has ~195 methods and a 3124-byte constructor -- far too large
 * to reconstruct in one pass. This header currently declares only the
 * singleton pointer plus the handful of methods CSTGEngine's own
 * (fully reconstructed) tick methods call directly, per the project's
 * bottom-up, bounded-batch approach (PLAN.md guiding principle #2/#3).
 *
 * CSTGGlobal's object is confirmed to be enormous (multiple confirmed field
 * offsets land around 0x29c9900-0x29c9fc0, i.e. ~43.6 MB into the object --
 * it almost certainly embeds large audio/sequencer buffers directly rather
 * than holding pointers to them). Given that, and that its full layout is
 * nowhere close to being recovered, methods here access confirmed fields via
 * raw `(unsigned char *)this + OFFSET` arithmetic rather than a struct
 * layout -- the same treatment already used for CSTGVoiceModel/
 * CSTGMultisampleBank before their full layouts were known.
 *
 * `RunVoiceModelFeedback()` (.text+0x4690, 123 bytes) is now
 * reconstructed (sec 10.55) -- see global.cpp. It walks a confirmed
 * intrusive linked list (head at `+0x29c9900`) calling a not-yet-
 * reconstructed sibling method (`CSTGSlotVoiceData::
 * RunVoiceModelFeedback`) on each qualifying node, where qualification
 * is gated by a real vtable dispatch (slot 0x1a, identity not
 * confirmed) through one of two confirmed pointer fields depending on
 * a confirmed flags byte -- modeled as an opaque indirect call through
 * that confirmed slot, matching this project's established
 * `CCostProfile`/`CSTGAudioDriverInterface` precedent for an
 * unidentified-but-confirmed-real vtable slot.
 *
 * `SetCurrentModeTempo(float)` (.text+0x4b20, 90 bytes, sec 10.117) is
 * now fully reconstructed -- see its own declaration below and
 * global.cpp for the confirmed FPCMOV-based clamp direction (previously
 * deferred twice for this exact ambiguity, now fully resolved).
 */

#ifndef OA_GLOBAL_H
#define OA_GLOBAL_H

struct CSTGPerformance;	/* forward decl, real definition in oa_engine_init.h */
struct CSTGPerformanceVars;	/* forward decl, real definition in oa_engine_init.h */

/* Confirmed real, deliberately deferred extern (sec 10.67) -- takes a
 * single int hardware command code in eax (regparm), called from
 * CSTGGlobal::UpdateAudioClockSource. CORRECTED (2026-07-04): the real
 * function (confirmed via OmapNKS4Module.ko's own disassembly, a ring-
 * buffer-backed FIFO write returning success/failure) returns `int`,
 * not `void` -- harmless for this specific caller since the return
 * value is discarded either way (same generated call site either way,
 * since nothing reads `eax` afterward), but corrected for accuracy. */
extern "C" int OmapNKS4OutputFifo_WriteCommand(int command);

/* Confirmed real, deliberately deferred extern (sec 10.98, relocation
 * from CSTGGlobal::SendPerfChangeToMidiOut) -- a real C++-mangled
 * (not extern "C") global function, no parameters. Confirmed called
 * TWICE in a row in one real code path with its return value discarded
 * the second time -- a genuine quirk, reproduced verbatim. */
bool SKSTGGate_ShouldSyncExternalClock();

/*
 * STGConvertedParam / CSTGMessageContext -- two types used pervasively
 * across CSTGGlobal's ~150 `UpdateXXX(CSTGMessageContext&,
 * STGConvertedParam&)` message-handler methods (one per settable global
 * parameter; this is how the "front panel knob turned" / "MIDI CC
 * received" / etc. message path ultimately updates state). Declared here
 * only with the specific fields actually confirmed by reading handler
 * bodies -- both types are always passed by reference from callers this
 * project hasn't reconstructed, so a full/exact size was never needed,
 * only correct relative offsets for the fields real handlers touch.
 *
 *   STGConvertedParam.value   (+0x00): confirmed via at least 4 handlers
 *     (UpdateMuteMode, UpdateRearPanelControllerReset,
 *     UpdateTmbrTrkOscTransposeType, UpdateUserAllNoteScale) -- the
 *     already-converted incoming parameter value.
 *   CSTGMessageContext.index  (+0x04): confirmed via UpdateUserAllNoteScale
 *     (used as an array index) and UpdatePadFuncMIDIChannel (bound-checked
 *     against 7 before use) -- likely a channel/slot/index selecting which
 *     instance of a per-N-thing parameter this update targets.
 */
struct STGConvertedParam {
	int value;			/* +0x00, confirmed */
	unsigned char _unrecovered[4];	/* real size unconfirmed; small placeholder
					 * since nothing here needs sizeof() to be exact */
};

/*
 * CValue -- confirmed real (relocation from CSTGGlobal::
 * ValidateParamChange, sec 10.92), always passed by const reference in
 * every confirmed caller so far; own layout not needed since it's never
 * dereferenced by any reconstructed code yet, only forwarded opaquely.
 */
struct CValue { unsigned char _unrecovered[4]; };

struct CSTGMessageContext {
	unsigned char _unrecovered_head[4];	/* +0x00..+0x03, unconfirmed */
	unsigned int  index;			/* +0x04, confirmed (see comment above) */
	unsigned char _unrecovered_mid[8];	/* +0x08..+0x0f, unconfirmed */
	unsigned int  responseCode;		/* +0x10, confirmed: UpdateSPDIFSampleRate
						 * conditionally writes the literal 6 here
						 * (see global.cpp) -- real meaning/name not
						 * confirmed, "responseCode" is descriptive
						 * only */
	unsigned char _unrecovered_tail14;	/* +0x14, unconfirmed */
	/* +0x15, confirmed (sec 10.92): ValidateParamChange clears this
	 * byte before forwarding to CSTGParamsOwner::ValidateParamChange
	 * whenever `index` is within a per-paramId bound -- real meaning
	 * not independently determined, "clampFlag" is descriptive only. */
	unsigned char clampFlag;
	unsigned char _unrecovered_tail16[2];	/* +0x16..+0x17, real size still
						 * unconfirmed beyond this point */
};

/*
 * CSTGCCInfo -- a brand-new class this pass (sec 10.161), holding a single
 * static 120-entry x 10-byte MIDI-CC info table (`sCCInfoTable`, one entry
 * per CC number 0..119, `.bss`, confirmed real size 0x4b0 = 1200 bytes) plus
 * a small derived scalar (`sNumVoiceModelCCs`).
 *
 * In the real binary, `sCCInfoTable` is NOT `.rodata` -- it's `.bss`,
 * populated by a 7734-byte "global constructor keyed to
 * CSTGCCInfo::sCCInfoTable" (`.text+0x277d0`) that runs once at module
 * init. Confirmed via a full disassembly + relocation dump (2219
 * instructions) that this ctor is 100% straight-line code: no branches, no
 * calls, only `movb`/`mov`/`lea`/`movzbl`/`ret`. It reads
 * `sNumVoiceModelCCs` exactly ONCE at entry (guaranteed 0 at that point --
 * `.bss` is zero at module-init time and nothing else writes this field
 * first), then a run of `lea N(%eax),%edx` + `mov %dl,OFFSET` writes 18
 * sequential index values (0..17) into the `+9` byte of 18 specific
 * entries (CC 1, 2, 5, 7, 11, 16, 64, 65, 70-79 -- exactly the MIDI CCs
 * conventionally used for "voice model" / filter-envelope control:
 * mod wheel, breath, portamento time, volume, expression, general
 * purpose 1, sustain, portamento on/off, sound controllers 1-10), and
 * finally writes the terminal count (18 = 0x12) back to
 * `sNumVoiceModelCCs`. Every other write is a plain literal `movb
 * $imm,OFFSET` into `sCCInfoTable` -- confirmed via a small Python
 * simulator (parses the disassembly+relocations, walks the 1098 total
 * writes) that every one of the table's 1200 bytes is written AT MOST
 * once (zero collisions) and 102 bytes are never written at all (left at
 * their `.bss` zero default). This means the ctor's own entire effect is
 * a single deterministic, fully compile-time-computable byte array --
 * reproduced here as a plain initialized `static` array rather than as
 * 1098 lines of transliterated store instructions, since both are
 * behaviorally identical (confirmed via the same simulation) and the
 * array form is vastly more reviewable. `sCCInfoTable` is NOT declared
 * `const`, matching the real symbol's mutable `.bss`/`.data` linkage,
 * even though no other function anywhere in the ~14MB binary is
 * confirmed to write to it (a whole-binary relocation scan against
 * `CSTGCCInfo::sCCInfoTable`/`sNumVoiceModelCCs` found only this one
 * ctor as a writer; ~50 other methods across a dozen classes read from
 * it, none of them reconstructed yet beyond the two below).
 *
 * Per-entry field semantics are only partially confirmed (this pass's
 * two consumers, `OnExtModeKnobAssignChange`/`OnExtModeSliderAssignChange`
 * below, only ever read byte +0 of an entry): byte 0 is a per-CC
 * "default/current value" (cross-checked against real MIDI conventions --
 * CC7/volume and CC11/expression both read 0x7f=127, CC10/pan reads
 * 0x40=64, the standard center value, CC64/sustain reads 0). Byte +9 is
 * the confirmed "voice model CC index" described above (0xff is NOT the
 * not-applicable sentinel here -- it's plain 0, indistinguishable from
 * legitimate index 0 by value alone; whatever reader distinguishes them
 * must use another field, not yet identified). Bytes +1..+8 are left
 * undocumented (not exercised by either consumer below) -- kept as an
 * opaque flat byte array rather than named struct fields to avoid
 * implying confirmed semantics this pass didn't verify, matching this
 * project's established convention for partially-recovered layouts
 * (see `CSTGControllerRTData` below).
 */
class CSTGCCInfo {
public:
	static unsigned char sCCInfoTable[1200];	/* 120 entries x 10 bytes */
	static unsigned char sNumVoiceModelCCs;
};

/*
 * CSTGControllerRTData -- like CSTGAudioBusManager before it, accessed by
 * CSTGGlobal via a computed `this+OFFSET` pointer (here, `+0x10`,
 * confirmed via a `lea`). **The "why does this alias" question
 * UpdateFootswitchPolarity's own earlier comment left open is now
 * CONFIRMED, not just theorized**: `CSTGGlobal::Initialize()` (sec
 * 10.55) itself calls `CSTGControllerRTData::Initialize(this =
 * CSTGGlobal_this + 0x10)` -- a genuine embedded sub-object at that
 * fixed offset, not a separately-heap-allocated pointer reinterpreted.
 * Declared minimally, with only the confirmed fields/methods actually
 * used so far (see UpdateFootswitchPolarity / CSTGGlobal::Initialize
 * in global.cpp).
 */
class CSTGControllerRTData {
public:
	/* Confirmed real (via relocation, called from CSTGGlobal's own
	 * constructor at +0x10, sec 10.56), own body not reconstructed in
	 * this pass. Declared explicitly (rather than relying on an
	 * implicit compiler-synthesized default) so CSTGGlobal::
	 * CSTGGlobal()'s own construction of this embedded sub-object
	 * links against the real confirmed symbol, not a silent no-op. */
	CSTGControllerRTData();

	/* SetFootSwitchPolarity(eControllerPolarity) (.text+0xd5b0, 4 bytes)
	 * confirmed: a single byte store at +0x1, no conversion. Real enum
	 * type not confirmed -- represented as a plain `int` parameter
	 * (regparm passes it in a register regardless of the real enum's
	 * declared width, so this doesn't affect the confirmed behavior). */
	void SetFootSwitchPolarity(int polarity);

	/* Initialize() (.text+0xd5a0, sec 10.88) reconstructed for real --
	 * a trivial tail-call to RequestAnalogInputPositions(). See
	 * src/engine/controller_rt_data_init.cpp. */
	void Initialize();

	/* RequestAnalogInputPositions() (.text+0xd470, sec 10.88)
	 * reconstructed for real -- see src/engine/
	 * controller_rt_data_init.cpp. */
	void RequestAnalogInputPositions();

	/*
	 * SetControllerAssignment (.text+0x1c800, 322 bytes, called from
	 * UpdateFootSwitchAssign/UpdateFootPedalAssign, sec 10.67/10.69)
	 * batch 16 (sec 10.163): fully reconstructed -- see
	 * src/engine/controller_rt_data_set_assignment.cpp for the complete
	 * confirmed shape (three curVal ranges, the `kControllerCCIdTable`/
	 * `kSetControllerAssignmentNotifyTable` tables, and the curVal==0x13
	 * float-compare quirk). Real mangled signature
	 * (`_ZN20CSTGControllerRTData23SetControllerAssignmentE
	 * R11TPackedEnumI17eControllerAssignES1_b`) is
	 * `(TPackedEnum<eControllerAssign>&, TPackedEnum<eControllerAssign>,
	 * bool)` -- arg1 is a REFERENCE. **Callers differ**: at
	 * `UpdateFootSwitchAssign`'s call site, arg1 is confirmed IDENTICAL
	 * to `this` itself (a self-reference to +0x0). At
	 * `UpdateFootPedalAssign`'s own call site, arg1 is instead `this+0x13`
	 * -- a DIFFERENT field, NOT the same as the call's own `this`
	 * (confirmed via direct register tracing, not assumed) -- so this
	 * is NOT always a self-reference; represented with a plain `void*`
	 * parameter (rather than a not-yet-modeled
	 * `TPackedEnum<eControllerAssign>&`) precisely because its target
	 * varies by caller. */
	void SetControllerAssignment(void *selfRef, signed char newValue, bool flag);

	/*
	 * kControllerCCIdTable (sec 10.163, `.rodata+0x334`, 18 bytes) --
	 * confirmed real: maps a "knob/slider assignable" `curVal` (1..0x12)
	 * to a MIDI CC id via `kControllerCCIdTable[curVal-1]` (confirmed via
	 * the real `movzx eax,byte[edx-1]` addressing, addend -1 folded into
	 * the relocation). Values (1,2,4,5,7,8,10,11,12,13,16,18,65,66,67,82,
	 * 91,93) all fit the real 7-bit MIDI CC domain. */
	static const unsigned char kControllerCCIdTable[18];

	/* SetAudioInSolo(unsigned int slot, bool solo)/ResetSendKnobsJumpCatch()
	 * (sec 10.80, confirmed via CSTGAudioInput::UpdateSolo/UpdateBusSelect's
	 * own tail calls) -- own bodies not reconstructed in this pass. */
	void SetAudioInSolo(unsigned int slot, bool solo);
	void ResetSendKnobsJumpCatch();

	/*
	 * sInstance (sec 10.69, confirmed via a direct relocation from
	 * UpdateKnobFaderMode) -- a real static singleton pointer, DISTINCT
	 * from the embedded `CSTGGlobal+0x10` sub-object used everywhere
	 * else in this file. Not yet confirmed where/how it's set (no
	 * assignment site found in this pass); declared only because
	 * UpdateKnobFaderMode reads it directly. */
	static CSTGControllerRTData *sInstance;

	/*
	 * ResetAllJumpCatch() (sec 10.129, .text+0x1d080, 78 bytes)
	 * confirmed: resolves the active `CSTGPerformanceVarsManager` via
	 * the SAME shared `ResolveActivePerformanceVarsManagerRaw()` idiom
	 * used throughout this cluster; if its own `fieldAt(0x23d1) == 2`
	 * ("mgr flag"), calls three newly-discovered, confirmed real,
	 * deliberately deferred siblings on `this`: `ResetKnobsJumpCatch()`,
	 * `ResetSlidersJumpCatch()`, `ResetRTKKnobSmoothers()` -- own bodies
	 * not reconstructed in this pass. No-op otherwise.
	 */
	void ResetAllJumpCatch();
	void ResetKnobsJumpCatch();
	void ResetSlidersJumpCatch();
	void ResetRTKKnobSmoothers();

	/* OnExtModeSetChange() (sec 10.71, confirmed via relocation from
	 * UpdateExtSetSelect) confirmed real, deliberately deferred extern
	 * -- called on the SAME static `sInstance` singleton pointer
	 * UpdateKnobFaderMode's own ResetAllJumpCatch call uses (confirmed
	 * via a `mov reg, ds:sInstance` load-the-pointer-value idiom, NOT
	 * the "address of the singleton pointer" `lea` idiom used
	 * elsewhere in this project) -- own body not reconstructed in this
	 * pass. */
	void OnExtModeSetChange();

	/*
	 * ResetPerfSwitches() (batch 36, `.text+0xd430`, 57 bytes, confirmed
	 * via relocation from `CSTGControllerInfo::OnPerformanceDeactivate()`,
	 * below) reconstructed for real -- a straight-line run of byte/word
	 * field zeroing (`+0x14/+0x15/+0x1c..+0x1f/+0x27..+0x2a` all zeroed;
	 * `+0x20` set to the confirmed non-zero literal `0x40`; `+0x18`/`+0x1a`
	 * (each a `WORD`) both set to `0x0200`), no branches, no calls -- one
	 * of the fields it zeroes (`+0x14`) is exactly the flag
	 * `OnPerformanceDeactivate()` itself gates on. See
	 * src/engine/controller_info_perf_deactivate.cpp.
	 */
	void ResetPerfSwitches();

	/* OnPerformanceActivate(CSTGPerformance&) (sec 10.102, confirmed
	 * via relocation from CSTGGlobal::CompletePerformanceActivation)
	 * confirmed real, deliberately deferred extern -- own body not
	 * reconstructed. */
	void OnPerformanceActivate(CSTGPerformance &perf);

	/*
	 * NotifySoloChange() (sec 10.107, .text+0x1d710, 170 bytes)
	 * confirmed: resolves "the current performance object" via the SAME
	 * confirmed real 3-way mode dispatch as `ResolveCurrentPerformance()`
	 * (sec 10.77) -- byte-for-byte identical strides/literals, a strong
	 * independent cross-check of that helper -- then makes a genuine
	 * VIRTUAL call (vtable slot 0x1b/27, not the direct non-virtual
	 * `IsCurrentlyActive()` call `UpdateVJSAssignment()` uses) with no
	 * further arguments. Ignores its own `this` entirely, operating
	 * purely via the global `CSTGGlobal::sInstance` singleton.
	 */
	void NotifySoloChange();

	/*
	 * The four `OnExtModeXXXAssignChange` notification methods (sec
	 * 10.72, confirmed via relocation from the 8 `UpdateExtXXXCCAssign`/
	 * `UpdateExtXXXMidiChannel` handlers below): each pair (CCAssign +
	 * MidiChannel) shares the SAME notification target, called with
	 * `ctx.index` as its single argument (the real parameter types are
	 * enums -- `eMixerKnob`/`eMixerPlayMuteSwitch`/`eMixerSelectSwitch`/
	 * `eFader` -- represented here as `unsigned int`, matching this
	 * project's established convention for register-passed enum
	 * params). Called on the SAME static `sInstance` pointer
	 * `OnExtModeSetChange` uses (same `mov reg, ds:sInstance`
	 * load-the-pointer-value idiom, confirmed via a full disassembly of
	 * all 8 callers, not a spot check).
	 *
	 * `OnExtModeKnobAssignChange`/`OnExtModeSliderAssignChange` (sec
	 * 10.161, `.text+0x19dc0`/`.text+0x19eb0`, 238 bytes each) are now
	 * fully reconstructed -- see `global.cpp`, right after
	 * `OnExtModePlayMuteSwitchAssignChange`/`OnExtModeSelectSwitchAssignChange`
	 * below. Unlike those two (a fixed two-call sequence ignoring
	 * `this`), these two are a genuinely different, larger shape:
	 * (1) look up the CC currently assigned to `index` via the SAME
	 * per-mode table `CSTGGlobal::UpdateExtAssign()` writes
	 * (`CSTGGlobal::sInstance + 0x29ca3c8 + mode*8 + index` for Knob,
	 * `+0x29cbc48 + mode*9 + index` for Slider, `mode` read from
	 * `CSTGGlobal+0x29cc0c8`) -- 0xff means "unassigned", early return,
	 * no further effect; (2) read `CSTGCCInfo::sCCInfoTable[cc]`'s own
	 * byte 0 (a per-CC default/current value) and write it into
	 * `STGAPIFrontPanelStatus::sInstance[index + 0x90b]` (Knob) /
	 * `[index + 0x923]` (Slider) for UI display; (3) if
	 * `this->fieldAt(0x2b) != 4`, stop here (no message, no jump-catch
	 * update) -- a confirmed real gate, not yet independently named;
	 * (4) otherwise, if the CC value's own bit7 is clear, mirror it
	 * into a per-index 3-byte-stride array at `this+0x56+index*3`
	 * (Knob) / `this+0x6e+index*3` (Slider); (5) branch on
	 * `CSTGGlobal+0x29c9fc0` (the SAME flag `UpdateKnobFaderMode`'s
	 * `ResetAllJumpCatch` gate reads, sec 10.129) -- if clear, just set
	 * a per-index "already matched" byte to 1 at `this+0x54+index*3`
	 * (Knob) / `this+0x6c+index*3` (Slider); if set, treat
	 * `this+0x50+index*3` (Knob) / `this+0x60+index*3` (Slider) as a
	 * small per-index "jump catch" record and update its own `+4`
	 * (Knob) / `+0xc` (Slider) tri-state byte by comparing its `+5`
	 * (Knob) / `+0xd` (Slider) byte against its `+6` (Knob) / `+0xe`
	 * (Slider) byte -- `0xff`->`0xff`, equal->`1`, else a SIGNED 8-bit
	 * `>` comparison selects `2`/`0` (confirmed via the real `setg`
	 * instruction operating on the raw byte-width sub-registers, NOT
	 * an unsigned/zero-extended compare -- matters if either byte is
	 * ever >=0x80). **Confirmed real overlap, not two independent
	 * fields**: `this+0x50+index*3`'s own `+6` byte (Knob) IS
	 * `this+0x56+index*3` (Slider: `this+0x60+index*3`'s `+0xe` byte IS
	 * `this+0x6e+index*3`) -- the EXACT SAME byte step (4)'s mirror
	 * store just wrote (when bit7 was clear). So in the common case
	 * (bit7 clear), this step's own "equal"/">" comparison is really
	 * "does the CC's brand new mirrored value equal/exceed byte `+5`
	 * (Knob) / `+0xd` (Slider), a SEPARATE persistent 'previous value'
	 * byte this function itself never writes" -- i.e. a genuine
	 * "did the assignment's effective value change since last time"
	 * check, not a comparison of two independently-settable fields.
	 * (6) always finishes (whenever step 3's gate
	 * passed) by building a 20-byte `PushUnsolicitedMessage()` packet
	 * (`{u16 0x14, u16 1, u32 0, u32 0xe (Knob) / 0xf (Slider), u32
	 * index, u32 ccValue}`), the SAME tagged-message shape used
	 * throughout `global.cpp`. */
	void OnExtModeKnobAssignChange(unsigned int index);

	/*
	 * OnExtModePlayMuteSwitchAssignChange(unsigned int) (sec 10.126,
	 * .text+0x19fd0, 44 bytes) and OnExtModeSelectSwitchAssignChange(
	 * unsigned int) (.text+0x19fa0, 44 bytes) confirmed: both ignore
	 * their own `this` entirely (like `NotifySoloChange` above),
	 * instead: (1) zero a byte at `STGAPIFrontPanelStatus::sInstance +
	 * index + 0x913` (PlayMute) / `+0x91b` (Select); (2) call the SAME
	 * confirmed real `CSTGControllerInfo::SendUnsolicitedUIParam()`
	 * (sec 10.126) with `(9, index, 0, 1)` for PlayMute / `(10, index,
	 * 0, 1)` for Select -- the ONLY difference between the two
	 * functions being that one literal param-ID constant and the one
	 * field-offset constant.
	 */
	void OnExtModePlayMuteSwitchAssignChange(unsigned int index);
	void OnExtModeSelectSwitchAssignChange(unsigned int index);
	void OnExtModeSliderAssignChange(unsigned int index);

	/* HandleControllerChange(eControllerAssign, unsigned char, bool,
	 * bool) (sec 10.101, confirmed via relocation from CSTGGlobal::
	 * ProcessCCSpecialMapping) confirmed real, deliberately deferred
	 * extern -- own body not reconstructed. `eControllerAssign` modeled
	 * as `int`, matching this project's established convention for
	 * that not-independently-defined enum (e.g.
	 * `RemoveExtCCFunctionAssignment`, sec 10.90). */
	void HandleControllerChange(int assign, unsigned char value, bool flag1, bool flag2);

	unsigned char _unrecovered_head[1];	/* +0x00, unconfirmed (see SetControllerAssignment above) */
	unsigned char footSwitchPolarity;	/* +0x01, confirmed (see SetFootSwitchPolarity above) */
	/* +0x13 (SetControllerAssignment's own arg1 target in
	 * UpdateFootPedalAssign), +0x15 (a confirmed real gate flag), and
	 * +0x18 (a confirmed real dword) are all real but accessed via raw
	 * offset arithmetic in global.cpp rather than named members here,
	 * matching this project's established convention for a class whose
	 * full layout isn't independently recovered -- NOT modeled as
	 * struct members to avoid implying a false byte-exact layout
	 * between +0x02 and +0x18. */
};

/*
 * The following classes are all confirmed real (via relocation, each
 * called directly from `CSTGGlobal::Initialize()`, sec 10.55) but
 * none of their own bodies are reconstructed in this pass -- same
 * "declare the shape, defer the body" treatment used throughout this
 * project for large sub-dependencies. Only the exact confirmed
 * `Initialize`/`InitializeAliasBanks` signatures needed to link
 * `CSTGGlobal::Initialize()` correctly are declared.
 */
struct CSTGWaveSeqData {
	void Initialize();
};

/*
 * CSTGMidiCCFilter -- confirmed real (sec 10.155), a brand-new class:
 * embedded within CSTGSlotVoiceData at +0x1db4 (confirmed via
 * CSTGSlotVoiceData::CSTGSlotVoiceData()'s own call site, this pass --
 * the outer ctor zeroes all 4 dwords of this sub-object immediately
 * before calling Initialize() on it). A 128-bit (4-dword) MIDI
 * CC-number bitmask. Initialize() (.text+0xd05b0, 45 bytes) confirmed:
 * ORs in bits 0..119 of the mask (loop bound 0x78 == 120 -- a confirmed
 * real "not quite the full 128 bits" bound, not a transcription
 * rounding; the top 8 bits of the last dword are left however the
 * caller already zeroed them). Own Set(const CSTGProgramSlot*)
 * (.text+0xd05e0, 1206 bytes) is a much larger, not-yet-reconstructed
 * sibling -- out of scope this pass.
 */
struct CSTGMidiCCFilter {
	void Initialize();
	unsigned int bits[4]; /* +0x0..+0xc */
};

struct CSTGProgramSlot;	/* forward decl, real definition further below (needed by Setup() below) */
struct CSTGProgram;	/* forward decl, real definition further below (needed by Setup() below) */
struct CSTGChannelValues;	/* forward decl, real definition in oa_engine_init.h (needed by Setup() below) */

struct CSTGSlotVoiceData {
	/* Confirmed real (via relocation, called 32 times from
	 * CSTGGlobal's own constructor, sec 10.56), declared explicitly
	 * (not left to an implicit compiler-synthesized default) so that
	 * construction links against the real confirmed symbol.
	 *
	 * CSTGSlotVoiceData::CSTGSlotVoiceData() (sec 10.155, `.text+0xb2fd0`,
	 * 701 bytes) fully reconstructed -- see
	 * src/engine/slot_voice_data_ctor.cpp for the full confirmed field
	 * map (mutex pair, a 121-entry x 12-byte "voice slot" array at
	 * +0x1488, the embedded CSTGMidiCCFilter at +0x1db4, the embedded
	 * CSTGHeldKeyList at +0x1e80, two 0x6c00-byte AllocAligned buffers,
	 * and several scattered scalar fields). Confirms two fields already
	 * on record from EmergencyFreeAllVoices()/Steal() (sec 10.138/
	 * 10.140): +0x44/+0x50 (the two linked-list heads those methods
	 * operate on) are BOTH zeroed here too, independent cross-
	 * confirmation of their real meaning. */
	CSTGSlotVoiceData();

	/*
	 * Initialize(unsigned short) (`.text+0xb3290`, 100 bytes) fully
	 * reconstructed (see global.cpp): confirmed real regparm(3)
	 * signature: this=eax, arg=edx (a plain integer slot index, 0-31,
	 * confirmed via CSTGGlobal::Initialize's own loop counter). Real
	 * parameter type not confirmed -- `unsigned short` chosen only
	 * because the mangled name's `t` component demands it
	 * (`_ZN17CSTGSlotVoiceData10InitializeEt`), not because the
	 * semantic width was independently verified.
	 *
	 * Confirmed shape: stores `slotIndex` verbatim at `+0x0` (16-bit),
	 * then decomposes it as `quadIndex = slotIndex >> 2` / `subIndex =
	 * slotIndex & 3` (the SAME quad/lane decomposition already
	 * established elsewhere in this project for the 4-wide SIMD "quad"
	 * voice architecture, see oa_quad.h) and computes two pointers into
	 * the shared LFO/step-sequencer sub-rate-parameter pools (`CSTGCommonLFO::
	 * sSubRateParams + quadIndex*0x250 + subIndex*4` and
	 * `CSTGCommonStepSeq::sSubRateParams + quadIndex*0x100 + subIndex*4`
	 * -- the multiplier-by-4-after-add compiler pattern the real
	 * disassembly uses is algebraically `quadIndex*stride +
	 * subIndex*4`, confirmed by `0x94*4 == 0x250` and `0x40*4 == 0x100`,
	 * exactly matching each pool's own already-confirmed per-quad
	 * stride), stored at `+0x1480`/`+0x1484` respectively. Finally calls
	 * `CSTGChannelValues::Initialize()` (newly discovered, confirmed
	 * real, deliberately deferred extern) on an embedded sub-object at
	 * `+0x1488`.
	 */
	void Initialize(unsigned short slotIndex);

	/* Confirmed real (via relocation from CSTGGlobal::
	 * RunVoiceModelFeedback, sec 10.55) -- own body not reconstructed;
	 * the real class's actual field layout is unrecovered (see
	 * RunVoiceModelFeedback's own comment in global.cpp for the
	 * confirmed offsets it reads on the OWNER side, +0xb6b/+0xb6f/
	 * +0xb73/+0xe2 -- not modeled here as real members of THIS class
	 * since ownership isn't confirmed). */
	void RunVoiceModelFeedback();

	/* UpdateGlobalTune(float) (sec 10.75, confirmed via relocation
	 * from CSTGGlobal::UpdateMasterTune) confirmed real, deliberately
	 * deferred extern -- own body not reconstructed in this pass. */
	void UpdateGlobalTune(float tune);

	/*
	 * EmergencyFreeAllVoices() (sec 10.138, .text+0xb4510, 48 bytes,
	 * confirmed via relocation from CSTGGlobal::
	 * EmergencyFreeDyingSlotVoiceData) confirmed: two calls to
	 * `CSTGVoiceAllocator::sInstance->EmergencyFreeVoiceList()`, on
	 * `this+0x44` and `this+0x50` respectively (two separate linked-list
	 * head fields within this object).
	 *
	 * FreeSlotVoiceData(bool) (sec 10.92, confirmed via relocation from
	 * CSTGGlobal::EmergencyFreeDyingSlotVoiceData) is real now, batch 17
	 * -- see src/engine/slot_voice_data_free.cpp (its own dedicated TU:
	 * this symbol has three separate pre-existing mocks in
	 * test_engine.cpp/test_global_ctor.cpp/test_global.cpp, the latter
	 * load-bearing across several call-count assertions).
	 */
	void EmergencyFreeAllVoices();
	void FreeSlotVoiceData(bool flag);

	/*
	 * AreAllKeysAndPedalsReleased() const (batch 17, `.text+0xb3b50`, 33
	 * bytes, confirmed via relocation from `CSTGPerformanceVars::
	 * NotifyAllKeysAndPedalsReleased()`) confirmed: `false` if `+0x2888`
	 * is set; `false` if `+0x1790 > 0x4f` (unsigned); else
	 * `+0x17a8 <= 0x3f` (unsigned). The EXACT same 3-field check is also
	 * inlined (no `call`) directly inside `FreeSlotVoiceData(bool)` at
	 * its own separate call site -- see slot_voice_data_free.cpp. */
	bool AreAllKeysAndPedalsReleased() const;

	/* SetIsDying() is real now, batch 19 (`.text+0xb3c50`, 15 bytes,
	 * confirmed via relocation from `CSTGPerformanceVars::SetIsDying()`)
	 * -- see src/engine/performance_vars_set_is_dying.cpp: idempotent,
	 * only takes effect the FIRST time (`+0x40 == 0`): sets `+0x40 = 1`
	 * (the SAME "dying" flag byte `EmergencyFreeDyingSlotVoiceData`/
	 * `StealDyingSlotVoiceDatasForCost`/`UpdateAllActiveMIDIFilters`
	 * already read) and `+0x41 = 0` (the SAME flag `Steal()` sets to 1,
	 * sec 10.140 -- this clears it back). */
	void SetIsDying();

	/* RunVoiceModelStaticFront(unsigned int)/RunVoiceModelStaticBack(
	 * unsigned int) (sec 10.93, confirmed via relocation from
	 * CSTGGlobal's own same-named methods below) confirmed real,
	 * deliberately deferred externs -- own bodies not reconstructed. */
	void RunVoiceModelStaticFront(unsigned int param);
	void RunVoiceModelStaticBack(unsigned int param);

	/* GetTotalStaticCosts(unsigned long*, unsigned long*) const (sec
	 * 10.94, confirmed via relocation from CSTGGlobal::
	 * StealDyingSlotVoiceDatasForCost) confirmed real, deliberately
	 * deferred extern -- own body not reconstructed. Fresh disassembly
	 * this pass (batch 18) confirms it independently INLINES the exact
	 * same real vtable-dispatch pattern GetPatchStaticCosts() has as its
	 * own standalone body (two `call *0x68(%edx)`-then-`call
	 * *0x2x(%edx)` sequences through a per-slot `+0x38`-indexed
	 * `CIFXEffectSlot`/`CMFXEffectSlot`-shaped table at `+0xb6b`), NOT a
	 * call to GetPatchStaticCosts() itself -- another instance of this
	 * project's own "an inlined helper is not the same as a call to the
	 * shared helper" finding (sec 10.163). Still blocked on the same
	 * not-yet-reconstructed effect-slot classes either way. */
	void GetTotalStaticCosts(unsigned long *out1, unsigned long *out2) const;

	/*
	 * GetPatchStaticCosts(unsigned int, unsigned long*, unsigned long*)
	 * const (batch 18, `.text+0xb5650`, 170 bytes, confirmed via
	 * relocation from the newly-reconstructed
	 * CLoadBalancer::BalanceStaticLoadHelper below) -- confirmed real,
	 * deliberately deferred: `this->fieldAt(0x34)->UsesPatch(busIndex,
	 * this->fieldAt(0x5))`-gated (a `CSTGProgramSlot::UsesPatch` call,
	 * not yet reconstructed), then on a match does TWO separate real
	 * vtable dispatches (`call *0x68(%edx)` then `call *0x24(%edx)` for
	 * out1, `call *0x68(%edx)` then `call *0x28(%edx)` for out2) through
	 * `this->fieldAt(0x38)[busIndexByte]`, a per-slot table at
	 * `+0xb6b` -- the SAME `CIFXEffectSlot`x10+`CMFXEffectSlot` cluster
	 * already blocking `CSTGProgram::CSTGProgram()` (sec 10.157). Real
	 * vtable DISPATCH per the sec 10.153 "install vs dispatch" rule --
	 * own body not reconstructed, left as a bare no-op stub in
	 * bar2_stubs.cpp; calling into it from BalanceStaticLoadHelper is
	 * safe (established "calling a still-deferred stub is fine"
	 * precedent).
	 */
	void GetPatchStaticCosts(unsigned int busIndex, unsigned long *out1, unsigned long *out2) const;

	/*
	 * EnableSlot() (batch 18, `.text+0xb3b80`, 101 bytes, confirmed via
	 * relocation from the newly-reconstructed
	 * CLoadBalancer::BalanceStaticLoad below) fully reconstructed -- see
	 * src/engine/load_balancer_static.cpp. No-op unless
	 * `fieldAt(0x28c4)` (dword) is nonzero -- the SAME "cost-accounting
	 * lock/reason" flag `FreeSlotVoiceData(bool)`/`BalanceStaticLoad`
	 * already read (sec 10.164/batch 18); this method's own job is to
	 * CLEAR it back to 0. Also sets bit 0x80 on `fieldAt(0x34)`'s own
	 * sub-object `+0x45` byte, and sends a `PushUnsolicitedMessage` with
	 * opcode `0x14`, param1 = `fieldAt(0x34)`'s own `+0x4` byte
	 * (zero-extended), param2 = `1` -- same 20-byte tagged-message shape
	 * already established in `OnExtModeKnobAssignChange`/
	 * `OnExtModeSliderAssignChange` (sec 10.161, global.cpp).
	 */
	void EnableSlot();

	/*
	 * Steal() (sec 10.140, .text+0xb3c60, 56 bytes, confirmed via
	 * relocation from CSTGGlobal::StealDyingSlotVoiceDatasForCost)
	 * confirmed: sets `fieldAt(0x42)`/`fieldAt(0x41)` to `1` (two
	 * confirmed flag bytes, same offsets `EmergencyFreeAllVoices`, sec
	 * 10.138, leaves untouched), then calls a newly-discovered
	 * confirmed real, deliberately deferred sibling,
	 * `CSTGVoiceAllocator::StealVoiceList()`, twice -- on `this+0x44`
	 * and `this+0x50` (the SAME two linked-list head fields
	 * `EmergencyFreeAllVoices` operates on, via its own analogous
	 * `EmergencyFreeVoiceList`).
	 */
	void Steal();

	/*
	 * UpdateAllActiveMIDIFilters() (sec 10.163, `.text+0xb8a50`, 624
	 * bytes, confirmed via relocation from `CSTGControllerRTData::
	 * SetControllerAssignment`'s own unconditional commit path) fully
	 * reconstructed -- see src/engine/slot_voice_data_midi_filters.cpp.
	 * Confirmed real: ignores `this` ENTIRELY (same "operates purely via
	 * a global singleton" shape as `NotifySoloChange`, sec 10.107) --
	 * modeled as `static` since the real disassembly never references
	 * any `this`-derived value anywhere in its body. Walks the SAME
	 * 16-entry, 12-byte-stride `CSTGGlobal::sInstance+0x29c990c` active-
	 * voice-data-node table already confirmed for `CSTGProgramSlot::
	 * ResolveActiveVoiceDataNode()` (sec 10.142, global.cpp) -- for each
	 * non-null node, dereferences its own `+0x8` payload field (NO
	 * separate null check on the payload itself, matching
	 * `ResolveActiveVoiceDataNode`'s own callers' identical lack of a
	 * payload null check -- a confirmed "node non-null implies payload
	 * non-null" invariant, not a reconstruction omission) and, if the
	 * payload's `+0x40` byte is 0, calls the payload's own
	 * `UpdateMIDIFilterAndResendAllCCs()`.
	 */
	static void UpdateAllActiveMIDIFilters();

	/*
	 * UpdateMIDIFilterAndResendAllCCs() (`.text+0xb8610`, 1075 bytes) --
	 * confirmed real, deliberately deferred: substantially larger than
	 * `UpdateAllActiveMIDIFilters()` itself and out of scope for this
	 * pass. Own no-op body given directly in
	 * src/engine/slot_voice_data_midi_filters.cpp (not bar2_stubs.cpp,
	 * which is never linked into any verify/ binary -- matching the
	 * `CSTGRecordTrack::StandbyRec()` precedent, sec 10.162).
	 */
	void UpdateMIDIFilterAndResendAllCCs();

	/*
	 * Setup(CSTGProgramSlot*, CSTGProgram*, const CSTGChannelValues*)
	 * (batch 47, `.text+0xb7250`, 0xe44/3652 bytes) -- confirmed real
	 * regparm(3) signature (this=eax, arg1=edx=slot, arg2=ecx=program,
	 * arg3=[stack]=channelValues -- a genuine 4th argument beyond
	 * regparm(3)'s 3 register slots) via `CSTGProgramSlot::
	 * ChangeProgram()`'s own real call site (global.cpp) and a SECOND
	 * confirmed real caller, `CSTGProgramSlot::
	 * LoadCombiTrackForPerformanceChangeEv` (not reconstructed in this
	 * project, out of scope). Confirmed real, deliberately deferred:
	 * substantially larger "load a program into a voice" DSP/parameter
	 * setup routine, out of scope per the sec 10.185 audio-DSP policy --
	 * own no-op body in bar2_stubs.cpp, matching the established
	 * reconstruct-caller-DSP-stub-callee pattern (ChangeProgram() itself
	 * is the caller reconstructed for real this batch).
	 */
	void Setup(CSTGProgramSlot *slot, CSTGProgram *program, const CSTGChannelValues *channelValues);
};
struct CSTGProgram;	/* forward decl, real definition further below */

/*
 * CSTGToneAdjust (sec 10.153, `.text+0xc76e0`, 247 bytes) -- confirmed
 * real, embedded at `CSTGProgramSlot+0x7f` (see that class's own ctor
 * below). Own vtable (`_ZTV14CSTGToneAdjust`) is confirmed installed but
 * never DISPATCHED THROUGH by anything reconstructed in this pass
 * (`SetSliderValue()`, the one method that calls through its slot 0, is
 * NOT reconstructed) -- a zero-filled placeholder is safe, same
 * treatment as this project's other not-yet-dispatched vtables. Ctor
 * writes 33 confirmed zero bytes at +0x4..+0x24, then 16 confirmed zero
 * WORDS at every odd 2-byte-stride offset from +0x45 to +0x67 -- a real,
 * confirmed gap at +0x25..+0x44 (untouched), not a transcription
 * omission. Declared opaque (no named fields) matching CSTGProgramSlot's
 * own established convention -- only the ctor's own writes are modeled.
 */
extern "C" unsigned char _ZTV14CSTGToneAdjust[12];
struct CSTGToneAdjust {
	CSTGToneAdjust();
};

/*
 * CSTGProgramSlot (sec 10.81) -- confirmed real base class of BOTH
 * CSTGProgramModeProgramSlot and CSTGProgramModeDrumTrackSlot (each
 * derived ctor's own first instruction is a call to
 * `CSTGProgramSlot::CSTGProgramSlot()`, the standard Itanium "call the
 * base ctor first" pattern, matching this project's own already-
 * established `CCostProfile : public CStartupFile`/`CSTGVectorEGXOnly
 * : public CSTGVectorEGBase` precedent, sec 10.60/10.66). Declared as
 * a real C++ base (not a manual-offset raw-pointer trick) since single
 * inheritance keeps the derived classes' own already-confirmed field
 * offsets unchanged (Itanium ABI places the base sub-object at offset
 * 0). `IsActive()`/`AccessActiveSlotVoiceData()` are confirmed real (via
 * direct, non-virtual PC32 relocations from CSTGProgramModeDrumTrackSlot::
 * ChangeDrumTrackProgram and the not-yet-reconstructed
 * OnUpdateGlobalMidiChannel/OnUpdateProgramDrumTrackMidiChannel) but
 * deliberately deferred externs -- own bodies not reconstructed in this
 * pass. `ChangeProgram()` is now real too (batch 47, see its own
 * declaration below).
 *
 * CSTGProgramSlot::CSTGProgramSlot() itself (sec 10.153, `.text+0xabf80`,
 * 219 bytes) is confirmed real now -- see program_slot_ctor.cpp. Installs
 * this class's OWN real vtable (`_ZTV15CSTGProgramSlot`, zero-filled
 * placeholder -- never dispatched by anything reconstructed here, only
 * ever by the two derived ctors, which unconditionally OVERWRITE this
 * field right afterward with their own class-specific
 * `g_programModeProgramSlotVtable[2]`/`g_programModeDrumTrackSlotVtable[2]`
 * (batch 47 split these out of a single shared `g_programSlotVtable`,
 * see global.cpp's own comment) -- a confirmed real, functionally-inert
 * redundancy, not a conflict), zeroes ~30 confirmed byte/dword fields,
 * and placement-constructs an embedded `CSTGToneAdjust` sub-object at
 * `this+0x7f` (see that class above).
 *
 * DRIVE-BY FIX (batch 45): this placeholder was declared as only 12
 * bytes, but a direct `nm -CS` re-check (not `readelf -SW`) finds the
 * REAL vtable is 0xf0 bytes (240, 60 slots) -- the same class of
 * too-short-hand-crafted-vtable risk sec 10.186 found for `CCostProfile`.
 * Harmless at the time (nothing reconstructed dispatched through it yet)
 * but corrected then (batch 45 was about to add sixteen MORE
 * placement-new instances of this exact class in `combi_ctor.cpp`) to
 * remove the landmine for whichever future batch promoted `ChangeProgram`
 * -- exactly this batch (47), which needed the correctly-sized slot 56
 * (byte offset 0xe0/0xe8) this fix made available. No file under verify/
 * has its own local copy of this symbol (confirmed via `grep -rln`).
 */
extern "C" unsigned char _ZTV15CSTGProgramSlot[0xf0];
struct CSTGProgramSlot {
	/* +0x0..+0x3, real class has a vtable (Initialize() dispatches
	 * through slot 7 -- .text+0x1c/4 -- not independently named in
	 * this pass, modeled via CallVtableSlot7() below rather than a
	 * real virtual method). Deliberately NOT a typed `void*` member --
	 * the real field is a packed 32-bit pointer, but a native host
	 * pointer is 8 bytes; a typed member here would get its own upper
	 * half clobbered by any subsequent raw `((unsigned char*)this)[N]`
	 * write for N<8 (the exact host/target pointer-width hazard this
	 * project has hit many times elsewhere, sec 10.55/10.56/10.58/etc)
	 * -- accessed instead via CallVtableSlot7's own ToU32/FromU32-style
	 * truncation, matching every other packed-pointer field in this
	 * codebase. */
	CSTGProgramSlot();

	/*
	 * IsActive()/AccessActiveSlotVoiceData()/HasActiveSlotVoiceData()/
	 * HasActiveVoices() (sec 10.142, `.text+0xac660`-`.text+0xac710`,
	 * 32/32/32/48 bytes) confirmed: all four share one exact prefix --
	 * `idx = this->fieldAt(4)` (a byte), then a 12-byte-stride lookup
	 * into a table at `CSTGGlobal::sInstance + 0x29c990c` (the SAME
	 * `idx*12` scale -- `lea eax,[eax+eax*2]; shl eax,2` -- already
	 * confirmed for the by-MIDI-channel bucket table at
	 * `+0x29c99cc`, sec 10.125's own `OnUpdateGlobalMidiChannel`
	 * comment; this table sits exactly 16 entries/192 bytes BEFORE
	 * that one, same 12-byte-stride shape, but indexed by THIS slot's
	 * own `fieldAt(4)` rather than a MIDI channel -- confirmed a
	 * genuinely separate table by the differing index source, not
	 * assumed identical). Each table entry is a list-NODE pointer
	 * (NULL if this slot has no linked voice data at all) whose own
	 * `+0x8` field is the actual `CSTGSlotVoiceData*` payload -- the
	 * exact node/payload split already confirmed for `UpdateMasterTune`'s
	 * list (sec 10.75: "the node is NOT itself the CSTGSlotVoiceData
	 * object... target read from the node's own +0x8"), now confirmed
	 * a second time via this independent family. Sharing this exact
	 * prefix is modeled as a private `ResolveActiveVoiceDataNode()`
	 * helper (returns the node, or null). Every field involved here
	 * (the table entry, the node's own `+0x8`, the payload's own
	 * `+0x34`) is the real target's packed 32-bit pointer, read/
	 * compared via explicit `unsigned int`/truncated-`this` handling
	 * (never a native 8-byte host pointer dereference) -- the same
	 * host/target pointer-width hazard this project has hit many
	 * times elsewhere (sec 10.55/10.56/10.58/etc):
	 * - `IsActive()`: null node -> false; else compare `this` against
	 *   the payload's own `+0x34` field (a confirmed real "owning
	 *   CSTGProgramSlot*" back-pointer).
	 * - `AccessActiveSlotVoiceData()`: null node -> null; else the
	 *   payload pointer itself (node's own `+0x8`).
	 * - `HasActiveSlotVoiceData()`: true iff the node itself is
	 *   non-null -- a real, faithfully-preserved quirk: this does NOT
	 *   check the payload pointer at all, only whether anything is
	 *   linked into this slot's bucket in the first place (so it can
	 *   be true even where `AccessActiveSlotVoiceData()` would return
	 *   null, if a node is linked with a null payload -- not
	 *   independently observed to actually happen, but the real
	 *   disassembly draws this distinction deliberately).
	 * - `HasActiveVoices()`: null node OR null payload -> false; else
	 *   true iff `*(unsigned short *)(payload+0x4c) +
	 *   *(unsigned short *)(payload+0x58) != 0` (two confirmed real
	 *   16-bit fields on the payload, summed via a real 16-bit `add`;
	 *   plausibly a static+dynamic voice-count pair, flagged as
	 *   speculation not fact since no independent confirmation of
	 *   either field's exact meaning exists yet).
	 * None of these four were previously declared/used except
	 * `IsActive()`/`AccessActiveSlotVoiceData()` (already relied on by
	 * two real call sites in `UpdateAudioTrackLevelJumpCatch`-adjacent
	 * code, sec 10.9x) -- `HasActiveSlotVoiceData()`/`HasActiveVoices()`
	 * are added here purely because the real binary's own symbol table
	 * confirms them as this family's siblings, not because anything
	 * calls them yet.
	 */
	bool IsActive() const;
	void *AccessActiveSlotVoiceData() const;
	bool HasActiveSlotVoiceData() const;
	bool HasActiveVoices() const;

	/*
	 * ChangeProgram(CSTGProgram*) (batch 47, `.text+0xac530`, 300 bytes)
	 * is now real -- see global.cpp for the full confirmed shape
	 * (finalizes all smoothers, builds a scratch local `CSTGChannelValues`,
	 * looks up this slot's own previous active voice data via
	 * `ResolveActiveVoiceDataNode()` above and, if found, dispatches a
	 * REAL vtable slot -- `ProcessPreviousSVDOnProgramChange`, see
	 * `CallVtableSlot56`/the two real per-class implementations in
	 * global.cpp -- to decide whether to carry its channel values
	 * forward, stores `newProgram` at `+0x5`, then hands off to
	 * `CSTGGlobal::GetFreeSlotVoiceData()` (already real) and the two
	 * confirmed-real-but-deliberately-deferred DSP callees `Setup()`/
	 * `CompleteLoadProgram()` below).
	 */
	void ChangeProgram(CSTGProgram *newProgram);

	/*
	 * GetProperMidiChannel() const (confirmed real via a relocation
	 * from ProcessPerformanceChange, sec 10.109/10.110; .text+0xac090,
	 * 29 bytes) confirmed: returns `this->fieldAt(0x10)` (a byte, the
	 * slot's own assigned MIDI channel) directly, UNLESS that byte is
	 * the confirmed real sentinel `0x10` (16), in which case it instead
	 * returns `CSTGGlobal::sInstance->fieldAt(0x6b8)` (the SAME global
	 * MIDI channel field this whole cluster reads/writes elsewhere,
	 * e.g. `PreprocessPerformanceChange`, sec 10.95).
	 */
	unsigned char GetProperMidiChannel() const;

	/*
	 * CompleteLoadProgram(CSTGSlotVoiceData*) (batch 47, `.text+0xac1d0`,
	 * 0x35b/859 bytes) -- confirmed real regparm(3) (this=eax=slot,
	 * arg1=edx=slotVoiceData) via `ChangeProgram()`'s own real call site
	 * above and a SECOND confirmed real caller, `CSTGProgramSlot::
	 * LoadCombiTrackForPerformanceChangeEv` (not reconstructed in this
	 * project, out of scope). Confirmed real, deliberately deferred --
	 * substantially larger than `ChangeProgram()` itself and out of scope
	 * per the sec 10.185 audio-DSP policy -- own no-op body in
	 * bar2_stubs.cpp, matching the established reconstruct-caller-DSP-
	 * stub-callee pattern.
	 */
	void CompleteLoadProgram(CSTGSlotVoiceData *slotVoiceData);
};

/*
 * Real vtable slot 7 dispatch (`call *0x1c(%edx)`), same raw-indirect-
 * dispatch treatment as CallVtableSlot2 above -- CSTGProgramSlot's own
 * real vtable layout/RTTI isn't independently reconstructed.
 *
 * Reads the object's own +0x0 vtable pointer as a packed 32-bit field
 * (ToU32/FromU32-style truncation, inlined here since those helpers are
 * file-local to global.cpp) rather than a native `void**` -- see
 * CSTGProgramSlot's own comment above for why.
 */
static inline void CallVtableSlot7(void *obj)
{
	typedef void (*Fn)(void *);
	unsigned int vtablePacked = *(unsigned int *)obj;
	void **vtable = (void **)(unsigned long)vtablePacked;
	Fn fn = (Fn)vtable[7];
	fn(obj);
}

/* Test-overridable, per-class vtable placeholders used by each derived
 * ctor below -- see their own definitions in global.cpp for why host
 * tests that exercise Initialize()'s (or, since batch 47, ChangeProgram()'s)
 * real vtable dispatch must repoint these at mmap(MAP_32BIT)'d buffers
 * first. Split from a single shared `g_programSlotVtable` in batch 47
 * once slot 56 (ChangeProgram()'s own real `ProcessPreviousSVDOnProgramChange`
 * dispatch) needed to differ per class -- every other slot, including
 * slot 7, is unaffected by the split. */
extern void **g_programModeProgramSlotVtable;
extern void **g_programModeDrumTrackSlotVtable;

/* The two real per-class slot-56 implementations (global.cpp) -- exposed
 * (not `static`) so a host test that needs a fresh, correctly-sized,
 * mmap32'd vtable buffer of its own (rather than relying on the
 * file-scope default arrays' own address, which is not guaranteed to
 * survive 32-bit truncation once a prior scenario's own mmap32'd
 * override has been unmapped) can install these exact real function
 * pointers directly. See test_global.cpp's own [54] scenario. */
bool ProgramSlot_ProcessPreviousSVDOnProgramChange(void *self, CSTGSlotVoiceData *svd);
bool ProgramModeProgramSlot_ProcessPreviousSVDOnProgramChange(void *self, CSTGSlotVoiceData *svd);

struct CSTGProgramModeProgramSlot : public CSTGProgramSlot {
	/* Confirmed real (via relocation, called from CSTGGlobal's own
	 * constructor, sec 10.56), declared explicitly for the same
	 * linkage reason as CSTGSlotVoiceData's above. */
	CSTGProgramModeProgramSlot();
	void Initialize(unsigned int arg);

	/*
	 * OnUpdateGlobalMidiChannel(unsigned int) (sec 10.125, .text+0xb9800,
	 * 283 bytes) confirmed: stores `channel` into `this->fieldAt(0x10)`,
	 * then (only `IsActive()` && `AccessActiveSlotVoiceData()` non-null)
	 * moves the returned `CSTGSlotVoiceData`-like object (`vd`) from
	 * whatever per-channel "bucket" it's currently linked into (if any,
	 * `vd->fieldAt(0x20)`) to the NEW channel's own bucket.
	 *
	 * Each bucket is a confirmed real `{head, tail, count}` triple
	 * (12-byte stride) in a table at `CSTGGlobal+0x29c99cc`, indexed by
	 * MIDI channel. Each `vd`'s own intrusive list node is two link
	 * fields, `+0x14`/`+0x18` (confirmed real, NOT the node's base
	 * address -- container `head`/`tail` slots store `&vd->fieldAt(0x14)`
	 * as the node's own "handle").
	 *
	 * UNLINK (only if `vd->fieldAt(0x20)` -- the old container -- is
	 * non-null): confirmed real head-check and tail-check are NOT
	 * mutually exclusive branches (both always run in sequence, matching
	 * the real disassembly's own explicit re-join after a head match --
	 * correctly handles a single-node bucket, which is simultaneously
	 * head and tail). If tail-matched, only `container->tail` updates;
	 * OTHERWISE a real generic doubly-linked-list middle-unlink runs
	 * (bridging `vd->fieldAt(0x18)`'s own `+0` field and
	 * `vd->fieldAt(0x14)`'s own `+4` field around `vd`), REGARDLESS of
	 * whether the head-check ALSO matched (a real, faithfully-preserved
	 * redundant-looking real quirk for the head-of-multi-node case, not
	 * simplified away). Then zeroes `vd`'s own `+0x14`/`+0x18`/`+0x20`
	 * and decrements the OLD bucket's `count`.
	 *
	 * INSERT (always runs, into the NEW channel's own bucket): a
	 * confirmed real PUSH-FRONT insertion -- the new node always becomes
	 * `bucket->head`. If the bucket was empty, sets `bucket->tail` to the
	 * new node too (falls through to the shared head-set/count-increment
	 * tail below) -- confirmed real quirk: `vd->fieldAt(0x14)` is left
	 * UNTOUCHED (potentially stale/uninitialized) on this specific path,
	 * only ever written on the non-empty-bucket path below, preserved
	 * verbatim, not defensively zeroed. If non-empty, splices the new
	 * node in before the old head (`vd->fieldAt(0x18)` = old head's own
	 * `+4` field; that field's own `+0`, if non-null, and the old head's
	 * own `+4` field are both updated to point at the new node;
	 * `vd->fieldAt(0x14)` = old head). Either way: `bucket->head` = new
	 * node, `vd->fieldAt(0x20)` = the bucket's own address, `bucket->count`
	 * incremented.
	 */
	void OnUpdateGlobalMidiChannel(unsigned int channel);
};
struct CSTGProgramModeDrumTrackSlot : public CSTGProgramSlot {
	/* Confirmed real (via relocation, called from CSTGGlobal's own
	 * constructor, sec 10.56), declared explicitly for the same
	 * linkage reason as CSTGSlotVoiceData's above. */
	CSTGProgramModeDrumTrackSlot();
	void Initialize(unsigned int arg);

	/*
	 * OnUpdateProgramDrumTrackMidiChannel(unsigned int) (sec 10.133,
	 * .text+0xb9510, 283 bytes, confirmed via relocation from
	 * CSTGGlobal::UpdateProgramDrumTrackMidiChannel) confirmed: BYTE-
	 * FOR-BYTE IDENTICAL control flow and field offsets to
	 * CSTGProgramModeProgramSlot::OnUpdateGlobalMidiChannel above (same
	 * intrusive bucket-list unlink/insert at the SAME shared bucket
	 * table, `CSTGGlobal::sInstance + 0x29c99cc`, indexed by the new
	 * channel) -- only the base-class methods it calls differ in
	 * address (CSTGProgramSlot::IsActive()/AccessActiveSlotVoiceData(),
	 * confirmed via relocation to be the exact same base-class methods,
	 * not overrides). See CSTGProgramModeProgramSlot::
	 * OnUpdateGlobalMidiChannel's own implementation comment in
	 * global.cpp for the full shape detail -- not repeated here.
	 */
	void OnUpdateProgramDrumTrackMidiChannel(unsigned int channel);

	/* ChangeDrumTrackProgram(CSTGProgram*, CSTGProgram*) (sec 10.81,
	 * confirmed): stores arg1 (the new program) at +0xe8, then calls
	 * the confirmed-real CSTGProgramSlot::ChangeProgram(arg2). */
	void ChangeDrumTrackProgram(CSTGProgram *newProgram, CSTGProgram *arg2);
};

/*
 * CSTGHeldKeyList -- confirmed real (sec 10.78, via relocation from
 * CSTGGlobal::UpdateMIDIChannel, called on an address computed as
 * `(some pointer)+0x1e80` -- CONFIRMED this pass (sec 10.155) to be the
 * embedded sub-object offset within CSTGSlotVoiceData, via that class's
 * own ctor call site).
 *
 * CSTGHeldKeyList::CSTGHeldKeyList() (sec 10.155, `.text+0xa2470`, 96
 * bytes) fully reconstructed -- see src/engine/slot_voice_data_ctor.cpp.
 * Confirmed: builds a 128-node, 0x14-byte-stride array (next@+0x0,
 * prev@+0x4, a self-pointer at +0xc, +0x10 zeroed -- matching Reset()'s
 * own already-confirmed next/prev/count layout exactly, an independent
 * cross-check of that method's own field map) followed immediately by a
 * 12-byte head/tail/count trailer at +0xa00/+0xa04/+0xa08 (128*0x14 ==
 * 0xa00, confirmed exact) -- total confirmed size 0xa0c bytes. A second,
 * confirmed-real pass re-zeroes every node's own +0x0 a second time
 * (functionally inert, preserved as a genuine double-write quirk).
 */
struct CSTGHeldKeyList {
	CSTGHeldKeyList();
	void Reset();
};

/*
 * CSTGEffectRackVars -- confirmed real (sec 10.78, via relocation
 * from CSTGGlobal::UpdateMIDIChannel, called on an address computed
 * as `(the resolved active CSTGPerformanceVarsManager)+0x20`). Own
 * layout not reconstructed beyond what UpdateDModRoutings() itself
 * touches.
 *
 * UpdateDModRoutings() (sec 10.135, .text+0xd0e70, 32 bytes) confirmed:
 * a small pointer-chase followed by a raw virtual dispatch -- reads
 * `this->fieldAt(0x2114)` (a pointer), reads THAT object's own
 * `fieldAt(0x23d4)` (a second pointer -- confirmed real numerically,
 * but NOT asserted to be the same field/meaning as
 * `CSTGPerformanceVarsManager`'s own `+0x23d4` elsewhere in this
 * codebase, since this is a different, not-fully-modeled object graph),
 * then calls that resolved object's own vtable slot `0x84/4 == 33`,
 * passing the ORIGINAL `this` as an explicit second argument (confirmed
 * via register allocation: `edx` is set to the original `this` at
 * function entry and never overwritten before the call). Modeled here
 * as a raw function-pointer cast, matching this project's established
 * "raw vtable-slot call" idiom for not-fully-typed virtual dispatch
 * (e.g. `NotifySoloChange`, sec 10.123).
 */
struct CSTGEffectRackVars {
	void UpdateDModRoutings();
};

/*
 * CSTGMidiQueueWriter -- confirmed real (sec 10.78, via relocation
 * from several UpdateXXX handlers, called on
 * `CSTGMidiPortManager::sInstance + 0x208` -- a real embedded
 * sub-object, not a separately-allocated singleton). `Write()`'s own
 * confirmed regparm(3) signature is `(const unsigned char*, unsigned
 * int, bool)`. Own layout not reconstructed.
 */
/*
 * CSTGMidiQueueWriter (sec 10.83) -- confirmed real layout (readelf/
 * objdump against OA_real.ko, .text+0x401a0): a genuine multi-reader
 * lock-free ring buffer writer with drop-on-full semantics (real-time-
 * safe: never blocks, silently discards the write if there isn't
 * room). +0x0 and +0x4 are both packed 32-bit pointer fields
 * (host/target width handled the same ToU32/FromU32 way as everywhere
 * else in this codebase), not typed C++ pointer members.
 *   +0x0  ringCtl -- pointer to a shared ring-buffer metadata struct:
 *           +0x8         capacity mask (bufSize - 1)
 *           +0xc         write cursor (monotonic byte count, never
 *                        wraps itself; wrapped position = cursor & mask)
 *           +0x10+i*4    reader i's own read cursor (monotonic)
 *           +0x20        active reader count (byte)
 *   +0x4  buffer base pointer (the actual ring buffer's own storage)
 */
struct CSTGMidiQueueWriter {
	void Write(const unsigned char *data, unsigned int length, bool flag);
};
struct CSTGPerformanceVarsManager {
	/*
	 * CORRECTED (sec 10.71): `sInstance`'s real symbol size is
	 * confirmed 12 bytes (via `readelf`), NOT a plain 4-byte pointer
	 * as first assumed. Confirmed real structure, from
	 * `UpdatePerfChangeHoldTime`/`UpdateExtSetSelect`'s own
	 * disassembly (both independently reference the SAME
	 * `sInstance+8` byte): two `CSTGPerformanceVarsManager*` pointer
	 * slots (`+0x0`/`+0x4`) plus a 4-byte "active index" selector
	 * (`+0x8`, only its low byte read) choosing which of the two is
	 * currently active. Declared as an opaque byte buffer (not a
	 * proper struct) since it's accessed via raw offset arithmetic,
	 * matching this project's established convention -- also still
	 * used as the literal `this` for `Initialize()` via `&sInstance`
	 * (sec 10.55/10.56's own "address of the singleton" call), meaning
	 * `Initialize()` itself is what populates these three sub-fields,
	 * not independently confirmed in this pass.
	 *
	 * `sInstance[9]` (sec 10.153, confirmed via `CSTGAudioBusManager::
	 * LRBusIndivMirror()`'s own disassembly): a DIFFERENT single byte
	 * from `sInstance[8]`'s "active perf-vars slot" toggle -- a 0/1
	 * "current double-buffer half" selector for
	 * `CSTGAudioBusManager::sEffectThreadBusSets`. Not confirmed who
	 * writes it (out of scope for this pass); only that
	 * LRBusIndivMirror() reads it.
	 */
	static unsigned char sInstance[12];
	void Initialize();

	/*
	 * AllocPerformanceVars() (confirmed real via a relocation from
	 * ProcessPerformanceChange, sec 10.109/10.110, called with
	 * `this=&sInstance` -- the SAME "address of the singleton" idiom as
	 * `Initialize()`; .text+0xba4b0, 274 bytes) confirmed: toggles
	 * `sInstance[8]` (0<->1) and selects the OTHER
	 * `CSTGPerformanceVarsManager*` slot (`sInstance[0]`/`sInstance[4]`)
	 * than whichever was active. Reads that manager's own `+0x23d1`
	 * "active state" byte (the SAME field used throughout this
	 * cluster):
	 *   - `0`: returns the manager as-is, no further effect.
	 *   - `5`: recomputes `+0x2400` as the LARGER of its current value
	 *     and `+0x23fc / 36.0f` (a confirmed real float constant,
	 *     exactly `1.0f/36.0f`'s own bit pattern -- not a coincidence,
	 *     see the `1.0f`/`1.0f/36.0f` reset defaults below).
	 *   - anything else (non-zero, non-5): forcibly resets `+0x23d1` to
	 *     `5`. If the ORIGINAL state was `<= 1` (i.e. `1`, since `0` is
	 *     already handled), updates a confirmed real "active count"
	 *     field at `STGAPIFrontPanelStatus::sInstance+0x1094` (counting
	 *     how many of `sInstance[0]`/`sInstance[4]`'s own managers have
	 *     `+0x23d1 > 1`) -- followed by a confirmed real but
	 *     UNREACHABLE `PushUnsolicitedMessage` block (a 16-byte message
	 *     built from `+0x240c`): by this point `+0x23d1` was ALREADY
	 *     forcibly set to `5` above, so the real code's own guard for
	 *     this block (`+0x23d1 > 1`) is always true and always skips
	 *     it -- reproduced faithfully as genuinely dead code, not
	 *     removed, matching this project's "preserve real quirks"
	 *     convention. Either way, resets `+0x23fc` to `1.0f` and
	 *     `+0x2400` to `1.0f/36.0f`.
	 * In all non-`0` cases, finally zeroes `+0x23d8` before returning
	 * the manager.
	 */
	CSTGPerformanceVars *AllocPerformanceVars();

	/*
	 * StealAllDyingPerformanceVars() (sec 10.114, called with
	 * `this=&sInstance` -- the same "address of the singleton" idiom as
	 * `Initialize()`/`AllocPerformanceVars()`; .text+0xba5d0, 385 bytes)
	 * confirmed: despite its loop-shaped disassembly, this ALWAYS
	 * processes exactly the OTHER manager slot from `sInstance[8]`'s
	 * own selector (`(selector+1)&1`) -- never the currently-active
	 * one, and never both (the apparent loop is confirmed to terminate
	 * after exactly one real iteration, given the two-value toggle
	 * mechanism; this function never persists any selector change back
	 * to `sInstance[8]`, unlike `AllocPerformanceVars`).
	 *
	 * No-op if that slot's own `+0x23d1` "active state" byte is `<=2`
	 * (SIGNED, so also true for any negative value). Otherwise calls
	 * `CSTGMidiDispatcher::sInstance->StealingRequiresOneTickStall()`
	 * (newly discovered, confirmed real, deliberately deferred), then
	 * walks the SAME confirmed real "active slot voice data" list at
	 * `CSTGGlobal::sInstance+0x29c9900` `FreeVoicelessDyingSlots` (sec
	 * 10.111) also walks, marking `+0x42=1` on every voice data whose
	 * own `+0x28c8` byte matches the manager's own `+0x23d0` group id
	 * (a "please steal me" flag, presumably consumed elsewhere).
	 *
	 * Then dispatches on the SAME `+0x23d1` state read earlier:
	 *   - `5`: takes the larger of `+0x23fc`/`+0x2400` directly (no
	 *     `/36.0f` division this time, unlike `AllocPerformanceVars`'s
	 *     own analogous state==5 handling, sec 10.112).
	 *   - anything else (confirmed `>2` and `!=5` at this point):
	 *     forcibly resets `+0x23d1` to `5`, then a confirmed-real but
	 *     PROVABLY UNREACHABLE block (the outer `>2` gate and this
	 *     branch's own `!=5` already exclude every value the block's
	 *     own `<=1` guard would need) mirrors `AllocPerformanceVars`'s
	 *     own dead "active count" + `PushUnsolicitedMessage` logic --
	 *     reproduced faithfully, never executed, matching sec 10.112's
	 *     precedent. Either way resets BOTH `+0x23fc` and `+0x2400` to
	 *     `1.0f` (not `1.0f`/`1.0f/36.0f` like `AllocPerformanceVars`'s
	 *     own reset -- a genuine, confirmed difference).
	 * Finally zeroes `+0x23d8` and sets (not clears) `+0x240c=1`.
	 */
	void StealAllDyingPerformanceVars();
};
/*
 * USTGAliasBankTypes (sec 10.85) -- confirmed real (readelf, both
 * WEAK/global 3840-entry `int[]` arrays, 30 "alias banks" x 128
 * entries each): `InitializeAliasBanks()`'s own confirmed real bank-ID
 * mapping table (30 blocks, NOT a simple 1:1 -- real bank 7 is aliased
 * by 9 distinct alias banks, blocks 7-15) is reproduced as a literal
 * constant array rather than a formula, matching this project's own
 * "no guessed simplification" convention.
 */
struct USTGAliasBankTypes {
	static void InitializeAliasBanks();

	/* ConvertAliasPgmBankToMidiBank/ConvertCombiBankToMidiBank (sec
	 * 10.98, confirmed via relocation from CSTGGlobal::
	 * SendPerfChangeToMidiOut) -- reconstructed for real, sec 10.152,
	 * see src/engine/alias_bank_convert.cpp (a dedicated TU, matching
	 * alias_bank_init.cpp's own precedent -- test_engine.cpp/
	 * test_global.cpp/test_global_ctor.cpp all keep their own
	 * pre-existing call-counter mocks for these, untouched). Both
	 * called with the bank id as an eax-passed "static this" (the same
	 * instance-less calling idiom already confirmed for
	 * `TranslateAudioInputParamId`, sec 10.90), not a real object
	 * pointer. */
	static void ConvertAliasPgmBankToMidiBank(int bankId, char &out1, char &out2);
	static void ConvertCombiBankToMidiBank(int bankId, char &out1, char &out2);

	/* ConvertMidiBankToCombiBank/ConvertMidiBankToAliasProgramBank (sec
	 * 10.99, confirmed via relocation from CSTGGlobal::
	 * HandleMidiBankAndPerformanceChange) -- the inverse direction of
	 * the two conversions above, reconstructed for real, sec 10.152,
	 * see src/engine/alias_bank_convert.cpp. Both called the same
	 * instance-less "static this" way, with the two `char` MIDI bank
	 * bytes SIGN-EXTENDED before the call (confirmed via the real
	 * `movsx` instructions, not zero-extended like every other byte
	 * parameter in this cluster) -- irrelevant to the reconstructed
	 * body itself, which only ever compares against small positive
	 * byte constants. */
	static void ConvertMidiBankToCombiBank(char midiBankMsb, char midiBankLsb, int &outBankId);
	static void ConvertMidiBankToAliasProgramBank(char midiBankMsb, char midiBankLsb, int &outBankId);

	/*
	 * IncrementCombiIndex/IncrementAliasProgramIndex/DecrementCombiIndex/
	 * DecrementAliasProgramIndex (sec 10.127, .text+0x257c0-0x258 68,
	 * 46/46/55/55 bytes, confirmed via relocation from CSTGGlobal::
	 * IncrementPerformance/DecrementPerformance) confirmed: each takes
	 * (bankId, index, bankId& out, index& out) via the same
	 * instance-less "static this" idiom; the 4th param is confirmed
	 * real to be passed on the STACK (not a register), since regparm(3)
	 * only covers the first 3.
	 *
	 * Increment: if `index+1 <= 0x7f` (127), just increments `index`
	 * (bank unchanged). Otherwise wraps `index` to 0 and increments
	 * `bankId`, itself wrapping to 0 if the incremented bank would reach
	 * the confirmed real bank count (`0x1f`/31 for AliasProgram, `0xe`/14
	 * for Combi).
	 *
	 * Decrement: if `index != 0`, just decrements `index` (bank
	 * unchanged). Otherwise sets `index` to the confirmed real max
	 * (`0x7f`/127, UNCONDITIONALLY, even before checking whether the
	 * bank itself needs to wrap) and decrements `bankId` -- wrapping to
	 * the confirmed real LAST bank index (`0x1e`/30 for AliasProgram,
	 * `0xd`/13 for Combi) if `bankId` was already 0.
	 */
	static void IncrementCombiIndex(int bankId, unsigned int index, int &outBankId, unsigned int &outIndex);
	static void IncrementAliasProgramIndex(int bankId, unsigned int index, int &outBankId, unsigned int &outIndex);
	static void DecrementCombiIndex(int bankId, unsigned int index, int &outBankId, unsigned int &outIndex);
	static void DecrementAliasProgramIndex(int bankId, unsigned int index, int &outBankId, unsigned int &outIndex);

	/*
	 * GetAliasPgmBankMapping(eSTGAliasProgramBankId, unsigned int,
	 * eSTGProgramBankId&, unsigned int&) (confirmed real via a
	 * relocation from ProcessPerformanceChange, sec 10.109/10.110;
	 * .text+0x252e0, 54 bytes) confirmed: if `index == 0xfffe` (the
	 * SAME sentinel used throughout this cluster), sets `outBankId=0`/
	 * `outIndex=0xfffe` and returns. Otherwise indexes the SAME
	 * already-declared `STGAliasToRealPgmBank`/`STGAliasBankPgmMap`
	 * tables (below) at `bankId*128 + index` for `outBankId`/`outIndex`
	 * respectively -- the same combined-index formula, just applied to
	 * TWO parallel tables from one lookup rather than one.
	 */
	static void GetAliasPgmBankMapping(int bankId, unsigned int index, int &outBankId, unsigned int &outIndex);
};
extern "C" int STGAliasToRealPgmBank[30 * 128];
extern "C" int STGAliasBankPgmMap[30 * 128];
struct CSetListBank {
	void Initialize();
};

/*
 * The following 7 classes are all confirmed real (via relocation, each
 * default-constructed directly by CSTGGlobal's own constructor, sec
 * 10.56) but none of their own bodies are reconstructed in this pass
 * -- same "declare the shape, defer the body" treatment used
 * throughout this project. Only a default constructor is declared for
 * each, matching the exact confirmed mangled name so
 * CSTGGlobal::CSTGGlobal() links correctly. Confirmed real per-object
 * sizes/counts (from the constructor's own loop bounds, all exact
 * clean divisions, not estimated): 2944 CSTGProgram (23 banks x 128,
 * matching the real Kronos program-bank architecture) at 0xcec bytes
 * each; 1792 CSTGCombi (14 banks x 128) at 0x19e7 bytes each; 200
 * CSTGSequence (no banking) at 0x1cad bytes each; 128 CSetList at
 * 0x834 bytes each; 598 CSTGWaveSequence at 0xd14 bytes each.
 *
 * CSTGSamplingInterface::CSTGSamplingInterface() itself is now real
 * (batch 13) -- see src/engine/sampling_interface_ctor.cpp for the
 * confirmed field list. Real confirmed size 0x570 bytes (the gap
 * between this sub-object's own `+0x98` placement and the next one,
 * CSTGAudioInput at `+0x608`, sec 10.56) -- highest field this ctor
 * itself touches is +0x56d, comfortably inside that bound. Has a real
 * (but not yet virtually-dispatched-through) vtable, confirmed 0x60
 * bytes via readelf (`vtable for CSTGSamplingInterface`), matching the
 * same "ParamsOwner"-style message-handler interface shape as
 * CSTGAudioInput/CSTGControllerInfo (dozens of Handle*() methods, none
 * reconstructed here -- only the ctor, which never dispatches through
 * its own vtable, satisfies the sec 10.153 "install vs dispatch" safe
 * case).
 */
struct CSTGSamplingInterface {
	static CSTGSamplingInterface *sInstance;
	CSTGSamplingInterface();
	unsigned char _unrecovered[0x570];
};
/*
 * CSTGAudioInput -- reconstructed (sec 10.80). Real confirmed layout
 * (readelf/objdump against OA_real.ko, addresses 0xc9b30-0xc9ea0):
 *   +0x00        vtable ptr (real class has a vtable, _ZTV14CSTGAudioInput;
 *                modeled as an explicit raw field, not a real C++ vtable,
 *                matching this project's own CStartupFile/CCostProfile
 *                precedent -- nothing here needs virtual dispatch)
 *   +0x04..+0x1b level[6] (int, one per audio input slot 0-5)
 *   +0x1c..+0x33 pan[6] (float)
 *   +0x34..+0x4b send1Level[6] (int)
 *   +0x4c..+0x63 send2Level[6] (int)
 *   +0x64..+0x69 busSelect[6] (unsigned char, an eSTGAPIBusIDOut value)
 *   +0x6a..+0x6f fxControlBus[6] (unsigned char, an eSTGAPIFXCtrlBus value)
 *   +0x70..+0x75 hdrBus[6] (unsigned char, an eSTGAPIHDRBus value)
 *   +0x76        muteMask (unsigned char, one bit per slot, bit0=slot0)
 *   +0x77        flags (bit0 always set by the ctor; bit1 = "performance
 *                active", set/cleared by OnPerformanceActivate/Deactivate,
 *                not yet reconstructed -- gates every Update*'s live-mixer
 *                push below)
 * The ctor only zeroes +0x64..+0x76 (19 bytes) -- +0x04..+0x63 are left
 * as whatever memory already held, a genuine confirmed quirk, preserved
 * verbatim rather than "fixed".
 *
 * Each UpdateXXX always stores locally first (guarded only by the real
 * `index <= 5` bounds check), then, only if flags bit1 is set, ALSO
 * pushes the change live: resolves the active `CSTGPerformanceVarsManager`
 * (the SAME `sInstance[8]`-selector idiom already confirmed at sec
 * 10.71/10.55, replicated here as a free function since this isn't a
 * CSTGGlobal method) and either writes directly into a per-slot region
 * at `*(mgr+8) + index*0x90` (Level/Send1Level/Send2Level/Mute), or calls
 * a `CSTGAudioInputMixerBase` method with `mgr` reinterpreted directly as
 * the mixer object (Pan/HDRBus/FXControlBus/BusSelect) -- a genuinely
 * different addressing pattern per handler, confirmed independently for
 * each from its own disassembly, not assumed to generalize.
 */
unsigned char *ResolveActivePerformanceVarsManagerRaw();

/* Result pair from CSTGPan::CalculateMonoPanCoeffs -- confirmed real via
 * CSTGAudioInputMixerBase::SetPan's own disassembly: two floats stored
 * back-to-back at the per-bus mixer-state array's own `+0x0`/`+0x4`
 * (see below). Real per-field semantics (e.g. "left"/"right" gain) not
 * independently confirmed -- named only by position. */
struct STGMonoPanCoeffs { float coeff0; float coeff4; };

/*
 * CSTGPan::CalculateMonoPanCoeffs(STGMonoPanCoeffs&, float, float)
 * (sec 10.151, `.text+0x24e30`, 104 bytes) -- see
 * src/engine/audio_input_mixer.cpp for the full confirmed x87 FPU
 * derivation (a real equal-power-ish quadratic pan law, verified via
 * its own center-pan sqrt(2)/2 continuity check).
 */
struct CSTGPan {
	static void CalculateMonoPanCoeffs(STGMonoPanCoeffs &out, float scale, float pan);
};

/* CBusChangeStateMachine -- confirmed real per-bus embedded state
 * machine (sec 10.150): CSTGAudioInputMixerBase::SetOutputBus calls
 * StartBusChange() as a genuine MEMBER method (not a free function) on
 * an instance embedded in `this->fieldAt(0xc) + busIndex*0x10` (a
 * separate array from the `+0x8` mixer-state array below). Confirmed
 * regparm(3) call: this=eax, arg1(eSTGBusID)=edx (from
 * `STGAPIOutToPhysBusId[value]`), arg2(eSTGBusType)=ecx (from
 * `STGAPIOutToBusType[value]`), arg3(unsigned int)=stack, confirmed
 * real constant `0x38`.
 * StartBusChange() itself is now real (sec 10.151, `.text+0x462c0`, 67
 * bytes) -- see src/engine/audio_input_mixer.cpp for the full confirmed
 * 0x10-byte-stride field layout and its real dependency on
 * `CSTGPerformanceVarsManager::sInstance[8]` (sec 10.71's own confirmed
 * "active perf-vars slot selector" byte) as a cheap per-call epoch
 * check.
 */
struct CBusChangeStateMachine {
	void StartBusChange(int busId, int busType, unsigned int arg3);
	/*
	 * Reset(eSTGBusID, eSTGBusType) (batch 22, `.text+0x46290`, 35
	 * bytes) confirmed real, branch/call-free: sets a default
	 * `busId`/`busType` pair (`0x20`/`dl`, `cl` -- i.e. `+0xa`=0x20,
	 * `+0xb`=busType arg) and re-arms the "started"/"changeToken"
	 * pair (`+0x0`=1, `+0x4`=1, `+0xc`=2). Called only from
	 * `CSTGAudioInputMixerBase::Initialize()`'s own per-entry setup
	 * loop with `busId=0x20, busType=0` (reading its own just-written
	 * `+0xa`/`+0xb` defaults straight back).
	 */
	void Reset(int busId, int busType);
};

/*
 * STGEQCoefficients -- 5-float output struct written by
 * CSTGEQ::CalculatePeakingCoefficients()/CalculateShelvingCoefficients()
 * (batch 30, sec 10.178). Confirmed real: both real functions write
 * exactly `(%reg)`/`+0x4`/`+0x8`/`+0xc`/`+0x10`, five packed floats, no
 * gaps. Field ROLE (not just position) is a strong but NOT independently
 * proven inference: five outputs from a filter-coefficient function
 * strongly suggests the classic normalized biquad {b0,b1,b2,a1,a2} (a0
 * implicit 1.0) layout, matching this project's own "named by position"
 * precedent (`STGMonoPanCoeffs`, sec 10.151) -- but the actual per-branch
 * formulas derived this batch do NOT line up sign-for-sign with the
 * textbook RBJ cookbook peaking-EQ formulas (e.g. `a1`/`b1` end up
 * OPPOSITE-signed for some branches here, not same-signed as the
 * cookbook), so this is a customized/Korg-specific coefficient set, not
 * a literal cookbook transcription. Named by position, semantics
 * unconfirmed beyond that.
 */
struct STGEQCoefficients { float b0; float b1; float b2; float a1; float a2; };

/*
 * eEQShelvingType -- the third argument of
 * CSTGEQ::CalculateShelvingCoefficients(), tested only as `eax==0` vs
 * `eax!=0` in the real disassembly (batch 30). Strong (but not
 * mechanically cross-checked against a real caller -- no caller is
 * reconstructed yet) inference: this selects between the same two
 * "shelf types" already named by the ground-truth mangled symbols
 * `CalculateLowShelfBeta`/`CalculateHighShelfBeta`, so 0/nonzero are
 * named accordingly here.
 */
enum eEQShelvingType { kEQLowShelf = 0, kEQHighShelf = 1 };

/*
 * CSTGEQ -- a plain static-method math-utility class (no fields, no
 * vtable, matching `CSTGPan` above), confirmed via `nm -C OA.ko | grep
 * CSTGEQ::` (only method symbols, no `vtable for`/`typeinfo for`).
 * `CSetListEQ::Initialize()`/`SetBand()` (both still deferred -- `SetBand`
 * is now DECLARED, see the `CSetListEQ` struct below `CSetList`, but its
 * own body stays an out-of-scope no-op per the sec 10.185 audio-DSP
 * policy) call these -- sec 10.177 identified this whole 5-function
 * cluster as the
 * TRUE root blocker of the entire `CSetListEQ`/`CSTGEffectRack`
 * subsystem (`CalculatePeakingBeta()` alone is called 9x from
 * `CSetListEQ::Initialize()`). Batch 30 (sec 10.178) reconstructs all
 * five via whole-function verbatim x87 inline-asm transcription (two of
 * the five use real `fptan`; the coefficient functions do heavy
 * multi-register x87 stack shuffling across real branches, not
 * reducible to this project's single-input/output x87 primitive-wrapper
 * convention, sec 10.117/10.151) -- see src/engine/eq_coefficients.cpp
 * for the full per-function derivation, including a from-scratch Python
 * x87-stack emulator used to numerically cross-check every branch of
 * the two coefficient functions before transcribing them.
 */
struct CSTGEQ {
	/* (`.text+0x247e0`, 34 bytes) beta = tan(2*pi*freq/48000 / 2). */
	static float CalculateLowShelfBeta(float freq);
	/* (`.text+0x24810`, 40 bytes) beta = tan((pi - 2*pi*freq/48000) / 2). */
	static float CalculateHighShelfBeta(float freq);
	/*
	 * (`.text+0x24910`, 68 bytes) omega = freq*2*pi/48000, written to
	 * *outOmega as a byproduct (confirmed real: `fsts (%eax)`, no pop,
	 * before the register is reused for the tan() computation itself);
	 * returns tan(min(omega/(2*bw), 1.49225652f)) -- the clamp constant
	 * avoids the tan() singularity as its argument approaches pi/2.
	 */
	static float CalculatePeakingBeta(float freq, float bw, float *outOmega);
	/*
	 * (`.text+0x24960`, 160 bytes) three float inputs (position-named
	 * `p0`/`p1`/`p2` -- `p2` is the one `fcos`'d internally, so it reads
	 * as a raw angle, not a pre-computed cosine) + output struct.
	 * Branches on `p0 >= 1.0`. See eq_coefficients.cpp for the full
	 * per-branch derivation.
	 */
	static void CalculatePeakingCoefficients(float p0, float p1, float p2,
	                                          STGEQCoefficients *out);
	/*
	 * (`.text+0x24840`, 199 bytes) branches on `gain >= 1.0` (2 cases)
	 * crossed with `type == kEQLowShelf` (2 cases) -- 4 total paths, the
	 * type-based pair converging on two shared tail blocks in the real
	 * object code (a compiler tail-merge, reproduced here as two shared
	 * asm labels rather than 4 duplicated tails -- behaviorally
	 * identical, see eq_coefficients.cpp).
	 */
	static void CalculateShelvingCoefficients(float gain, float beta, eEQShelvingType type,
	                                           STGEQCoefficients *out);
};

/*
 * CSTGBusInfo::GetSignalSelectionForBusType(int) (sec 10.151,
 * `.text+0x258a0`, 24 bytes) confirmed: a plain 2-entry `{1, 2}` lookup
 * table (raw `.rodata` ints, independently confirmed to carry NO
 * relocation at that byte range) indexed by `busType - 3`; any busType
 * outside {3, 4} returns 0. See src/engine/audio_input_mixer.cpp.
 */
struct CSTGBusInfo {
	static int GetSignalSelectionForBusType(int busType);
};

/*
 * CSTGAudioInputMixerBase -- fully reconstructed (sec 10.150, see
 * src/engine/audio_input_mixer.cpp), a separate translation unit from
 * global.cpp (matching CSTGMidiQueueWriter's own established
 * "own dedicated KAT, existing mocks elsewhere untouched" precedent,
 * sec 10.83): test_engine.cpp/test_global.cpp/test_global_ctor.cpp all
 * keep their own PRE-EXISTING call-counting mocks for these four
 * methods (load-bearing for ~20 CSTGAudioInput-focused assertions each
 * -- rewiring all of them onto the real bodies below is a separate,
 * larger task, deliberately out of scope this pass); the real bodies
 * are instead exercised directly by their own new
 * verify/test_audio_input_mixer.cpp.
 *
 * Confirmed real layout (raw offset arithmetic, `this` always
 * reinterpreted directly as the mixer object per the `CSTGAudioInput`
 * comment above -- never separately allocated/sized in this
 * reconstruction):
 *   +0x00  vtable ptr (own real vtable; SetFXCtrlBus/SetHDRBus each
 *          dispatch a RAW indirect call through slot 3, `call *0xc(%esi)`
 *          -- not this project's own C++ virtual mechanism, matching the
 *          established raw-vtable-dispatch convention, sec 10.149)
 *   +0x08  mixerStateArray -- pointer to a per-bus, 0x90-byte-stride
 *          array (confirmed via SetPan/SetFXCtrlBus/SetHDRBus all
 *          computing `busIndex*9*16 == busIndex*0x90` identically):
 *            +0x00/+0x04  pan coefficients (float pair, SetPan)
 *            +0x50/+0x54/+0x58  HDR bus signal-selection state (SetHDRBus)
 *            +0x68  FX-control bus routing result (SetFXCtrlBus)
 *            +0x6c  HDR bus routing result (SetHDRBus)
 *   +0x0c  busChangeArray -- pointer to a per-bus, 0x10-byte-stride
 *          array of embedded CBusChangeStateMachine instances
 *          (SetOutputBus only)
 */
class CSTGAudioInputMixerBase {
public:
	/*
	 * All three pointer-shaped fields below are packed 32-bit values
	 * (`unsigned int`), NOT native C++ pointer members -- matching this
	 * project's established `CSTGMidiQueueWriter`-style convention
	 * (ToU32/FromU32) for any class reinterpreted directly onto raw
	 * target memory at fixed byte offsets. A native `void*`/`T*` member
	 * here would be 8 bytes on this 64-bit host but 4 bytes on the real
	 * 32-bit target, silently shifting every subsequent field's own
	 * offset (`_gap4`/`mixerStateArray`/`busChangeArray` would all land
	 * at the WRONG byte offsets on a 64-bit host) -- caught via a real
	 * segfault in this class's own dedicated KAT before landing on this
	 * fix, not by re-reading the disassembly a second time.
	 */
	unsigned int vtablePtr32;		/* +0x0 */
	unsigned char _gap4[4];			/* +0x4, unrecovered */
	unsigned int mixerStateArray32;		/* +0x8 */
	unsigned int busChangeArray32;		/* +0xc */

	/* Confirmed real (CSTGAudioInput::UpdateHDRBus/UpdateFXControlBus/
	 * UpdateBusSelect/UpdatePan's own tail calls). See the class-level
	 * comment above for the full confirmed shape and file placement. */
	void SetHDRBus(unsigned int busIndex, int value);
	void SetFXCtrlBus(unsigned int busIndex, int value);
	void SetOutputBus(unsigned int busIndex, int value);
	void SetPan(unsigned int busIndex, float value);

	/*
	 * Initialize(unsigned int count) (batch 22, `.text+0x68a80`, 342
	 * bytes) confirmed. Writes `count` (truncated to a byte) into
	 * `_gap4[0]`, allocates+zeroes a `count*0x90`-byte `mixerStateArray`,
	 * then for each entry: computes `CSTGPan::CalculateMonoPanCoeffs(
	 * coeffs, 1.0f, 0.5f)` (the SAME constant inputs every iteration --
	 * recomputed identically each time, faithfully preserved, not
	 * hoisted) into `+0x0`/`+0x4`, and sets `+0x60`/`+0x64`/`+0x68`/
	 * `+0x6c`/`+0x70`/`+0x74` to `&sGlobalBusSet[0]`/`[32]`/`[32]`/`[32]`/
	 * `[32]`/`[32]` respectively (bus 0, then five copies of bus 32 --
	 * a confirmed real, if slightly redundant-looking, default). Then
	 * allocates a `count*0x10`-byte `busChangeArray` (`operator new[]`,
	 * NOT `CSTGBankMemory::AllocAligned` -- a real, confirmed
	 * asymmetry) and default-initializes each `CBusChangeStateMachine`
	 * entry's own `+0x0`/`+0x8`/`+0x9`/`+0xa`/`+0xb`/`+0xc` fields
	 * directly (NOT via a call to `Reset()`) to `0`/`0x20`/`0`/`0x20`/
	 * `0`/`0` -- deliberately leaving `+0x4` untouched (a real,
	 * confirmed gap vs. `Reset()`'s own four-field write). Finally, IF
	 * `count != 0`, loops over each mixerState/busChange PAIR again:
	 * duplicates `mixerState[i]+0x0..+0xf` to `+0x10..+0x1f` (re-zeroing
	 * `+0x18` after) and `+0x20..+0x2f` to `+0x30..+0x3f` (pure 16-byte
	 * data movement via `movaps` -- the already-known "SSE used for
	 * wide copy, not real vector math" case, safe to model as a plain
	 * struct copy), then calls `busChangeArray[i].Reset(busId=
	 * busChangeArray[i]'s own +0xa byte, busType=+0xb byte)` -- i.e.
	 * `Reset(0x20, 0)` given the defaults just written above.
	 */
	void Initialize(unsigned int count);
};

struct CSTGAudioInput {
	void *_vtablePtr;	/* +0x0 */
	CSTGAudioInput();
	void UpdateBusSelect(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateFXControlBus(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateHDRBus(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateLevel(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateMute(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdatePan(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateSend1Level(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateSend2Level(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateSolo(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * OnUseGlobalSettingsChanged() (sec 10.134, .text+0xca410, 155
	 * bytes, confirmed via relocation from CSTGGlobal::
	 * SetUseGlobalAudioInputSettings) confirmed: compares a "desired"
	 * bit (either the global-settings-enabled flag at `CSTGGlobal::
	 * sInstance+0x680`, or, if that's clear, this object's own
	 * `fieldAt(0x77) & 1`) against the "current" bit (bit 1 of
	 * `CSTGGlobal::sInstance+0x67f`) -- a no-op if they already match.
	 * On a real transition: if turning ON global settings (desired==1),
	 * clears this object's own `fieldAt(0x77)` bit 1 and calls
	 * `UseSettings()` on the GLOBAL DEFAULT input object at
	 * `CSTGGlobal::sInstance+0x608` (a second, separate embedded
	 * CSTGAudioInput); if turning OFF (desired==0), clears the global
	 * bit-1 at `+0x67f` instead and calls `UseSettings()` on `this` (the
	 * PER-INSTANCE object). Either way, finally checks
	 * `CSTGControllerRTData::sInstance->fieldAt(0x2b) == 7` and, if so,
	 * calls the confirmed-real `ResetAllJumpCatch()` (sec 10.129) --
	 * this is a 6th real call site for that function, in addition to
	 * the 5 already known (sec 10.133's own note on why every call
	 * site matters for testing this specific dependency).
	 */
	void OnUseGlobalSettingsChanged();

	/*
	 * UseSettings() (sec 10.134, confirmed via relocation from
	 * OnUseGlobalSettingsChanged above, called on either `this` or the
	 * global default object at CSTGGlobal::sInstance+0x608) confirmed
	 * real, deliberately deferred extern -- own body not reconstructed
	 * in this pass.
	 */
	void UseSettings();

	/* OnPerformanceDeactivate() (batch 20, `.text+0xc9f00`, 39 bytes,
	 * confirmed via relocation from `CSTGPerformance::SetIsDying` --
	 * called there on the embedded sub-object at `+0xae7`). Real now --
	 * see src/engine/audio_input_use_settings.cpp (the exact counterpart
	 * to UseSettings(): clears the same `+0x77`/`+0x67f` bit1 gate
	 * UseSettings sets). */
	void OnPerformanceDeactivate();
};
/*
 * CSTGDrumKitData::CSTGDrumKitData() (batch 23, `.text+0xa0940`, 925
 * bytes) confirmed real -- see src/engine/drum_kit_data.cpp for the full
 * derivation (a 273 (drum-kit patch) x 129 (MIDI note) x 8 (velocity
 * zone) table of legacy multisample-bank UUID records, ~17.3MB).
 * Confirmed MINIMUM real size 0x1143529 -- NOT simply
 * `sizeof(vtable ptr) + 273*0x10302` (0x1143526): the last note record's
 * own 8th velocity-zone sub-record deliberately overflows 3 bytes past
 * the nominal array end (see that file's own header comment on the
 * per-record 0x19-byte sub-record stride vs. the 0x202-byte record
 * stride not dividing evenly) -- a real, confirmed quirk of the compiled
 * loop, not a reconstruction bug. Declared size rounded up to 0x1143530.
 */
class CSTGDrumKitData {
public:
	CSTGDrumKitData();
	unsigned char _unrecovered[0x1143530];
};
/*
 * CSTGWaveSequence::CSTGWaveSequence() -- NOT a standalone symbol in
 * OA_real.ko (confirmed absent from the whole symbol table, unlike
 * every other model/manager ctor in this project) -- fully INLINED at
 * its one call site, CSTGGlobal::CSTGGlobal()'s own 598-entry array
 * loop (`.text+0x3910`: `movl $0x8,(%ecx)` with relocation
 * `_ZTV16CSTGWaveSequence`, see global_ctor.cpp). Real ctor effect is
 * therefore fully confirmed and IS a real body now (see
 * waveseq_setlist_init.cpp): the standard Itanium vtable-pointer
 * install only (`_ZTV16CSTGWaveSequence+8`), nothing else -- every
 * other byte global_ctor.cpp's own loop writes for each entry
 * (+0x5/+0x13 zero bytes, the 64-entry inner zero-fill) is
 * CSTGGlobal's OWN ctor code, not part of this sub-object's ctor.
 * `_ZTV16CSTGWaveSequence` sized to its confirmed real 96 bytes
 * (readelf, `vtable for CSTGWaveSequence`), not a generic 12-byte
 * placeholder, since the true size is independently known here.
 */
extern "C" unsigned char _ZTV16CSTGWaveSequence[96];
struct CSTGWaveSequence { CSTGWaveSequence(); };

/*
 * CIFXEffectSlot::CIFXEffectSlot() (batch 44, `.text+0x8d0e0`, 77 bytes)
 * confirmed real: standard Itanium vtable-pointer install
 * (`_ZTV14CIFXEffectSlot+8`) plus a handful of confirmed scalar field
 * writes (+0x4/+0x5/+0x6 zero bytes, +0x8 word = 1, +0x9a zero byte,
 * +0x98/+0x99 zero bytes, +0x9b byte = 0x19, +0x9c float = 64.0f,
 * +0xa0/+0xa4 zeroed dwords) -- no dispatch, no further sub-object
 * construction. `CIFXEffectSlot`'s own real vtable is 0x88 bytes (34
 * slots, `readelf`/`nm -CS` confirmed) -- left zero-filled per this
 * project's established "install vs dispatch" rule (sec 10.153): every
 * caller that would genuinely DISPATCH through this vtable (`CSTGEffectRack::
 * GetTotalAlgorithmCost` via `CEffectSlotBase::GetAlgorithmCost`,
 * `CSTGEffectManager::RunEffects`, etc) remains an untouched bare-`{}`
 * stub in this project as of this batch, so nothing reachable ever
 * reads a function pointer out of it yet. Declared opaque (no named
 * fields), matching `CSTGProgramSlot`/`CSTGToneAdjust`'s own established
 * convention -- only the ctor's own writes are modeled. `CSTGProgram::
 * CSTGProgram()` (see below) places twelve of these back-to-back at a
 * confirmed real 0xa8-byte stride.
 */
extern "C" unsigned char _ZTV14CIFXEffectSlot[0x88];
struct CIFXEffectSlot { CIFXEffectSlot(); };

/*
 * CSTGVectorMotion::CSTGVectorMotion() (batch 44, `.text+0x73090`, 76
 * bytes) confirmed real: vtable-pointer install (`_ZTV16CSTGVectorMotion+8`),
 * nine confirmed zeroed bytes at +0x4/+0x5/.../+0xb then a scattered
 * set (+0x2e/+0x38/+0x42/+0x4c/+0x56, stride 10 apart), plus FOUR
 * confirmed dword writes of the same packed constant `0x3b810204` at
 * +0x15/+0x19/+0x1d/+0x21 (stride 4 -- real, not float; matches this
 * project's own "raw packed constant, not a float" precedent already
 * used for `CSTGProgramSlot`'s own +0x35 field). No dispatch. Own
 * vtable 0x60 bytes (24 slots) -- zero-filled placeholder, same "install
 * vs dispatch" reasoning as `CIFXEffectSlot` above (nothing reconstructed
 * in this project dispatches through it).
 */
extern "C" unsigned char _ZTV16CSTGVectorMotion[0x60];
struct CSTGVectorMotion { CSTGVectorMotion(); };

/*
 * CSTGProgram::CSTGProgram() (batch 44, `.text+0xa4c00`, 328 bytes)
 * confirmed real -- see src/engine/program_ctor.cpp for the full
 * derivation. Genuine C++ MULTIPLE inheritance: installs TWO base
 * vtable pointers at +0x0 (`_ZTV15CSTGPerformance+8`) and +0x4
 * (`_ZTV14CSTGEffectRack+8`), each a confirmed real 0x98/0x60-byte
 * vtable (readelf/nm -CS) -- both left zero-filled placeholders per
 * this project's established "install vs dispatch" rule: EVERY
 * already-real, already-linked caller that would eventually dispatch
 * through either vtable (`CSTGPerformanceVars::EnterActivatingState`
 * via `CSTGEffectRack::GetTotalAlgorithmCost`, `CSTGSlotVoiceData::
 * GetPatchStaticCosts`/`GetTotalStaticCosts`/`RunVoiceModelStaticFront`/
 * `StaticBack`/`RunVoiceModelFeedback`, `CSetListEQ::Initialize`,
 * `CSTGEffectManager::RunEffects`) remains a bare-`{}` stub in this
 * project as of this batch -- independently re-confirmed batch 44 via a
 * project-wide grep before promoting this ctor (see MASTER_REFERENCE.md
 * sec 10.195 for the full caller-by-caller audit). Installs its OWN
 * derived vtable (`_ZTV11CSTGProgram+8`, 0x98 bytes) over the
 * `CSTGPerformance` base pointer near the end (standard Itanium
 * "derived ctor overwrites the vtable ptr the base ctor just installed"
 * pattern), same technique the ten `CSTGVoiceModel`-derived Model ctors
 * already established (sec 10.193/batch 42), just applied to TWO base
 * vtables instead of one -- confirming the sec 10.185 policy DOES
 * extend to this multiple-inheritance shape, contrary to batch 43's own
 * (correctly cautious, but as it turns out not load-bearing) concern.
 */
extern "C" unsigned char _ZTV15CSTGPerformance[0x98];
extern "C" unsigned char _ZTV14CSTGEffectRack[0x60];
extern "C" unsigned char _ZTV11CSTGProgram[0x98];
/* CMFXEffectSlot/CTFXEffectSlot/CSTGEffectBalance/CSTGCommonEffectLFO
 * have NO out-of-line ctor at all in ground truth (confirmed via a
 * whole-symbol-table grep, zero hits) -- each is fully inlined as a
 * handful of direct field writes inside `CSTGProgram::CSTGProgram()`'s
 * own body (matching the `CSTGWaveSequence`/`CSetList` "no standalone
 * symbol" precedent, sec 10.159), so only their vtable placeholders are
 * needed here, no struct/ctor declarations. */
extern "C" unsigned char _ZTV14CMFXEffectSlot[0x88];
extern "C" unsigned char _ZTV14CTFXEffectSlot[0x88];
extern "C" unsigned char _ZTV17CSTGEffectBalance[0x60];
extern "C" unsigned char _ZTV19CSTGCommonEffectLFO[0x60];
/* CSTGParamsOwner/CSTGStepSeqBase/CSTGCommonStepSeq similarly have no
 * separate ctor CALLED from CSTGProgram::CSTGProgram() -- their vtable
 * pointers are written directly via raw offset arithmetic (see
 * program_ctor.cpp), not via a placement-new sub-object construction.
 * `CSTGParamsOwner`'s own class is already declared further below in
 * this file (confirmed real via `CSTGGlobal::ValidateParamChange`,
 * sec 10.92) -- only its vtable placeholder is new here.
 * `CSTGStepSeqBase`/`CSTGCommonStepSeq` are declared in oa_engine_init.h
 * (their own static `Initialize()`/`sSubRateParams` pool-holder
 * members, sec ~10.170) -- likewise, only the vtable placeholders (a
 * DIFFERENT concern from those static members) are added here. */
extern "C" unsigned char _ZTV15CSTGParamsOwner[0x60];
extern "C" unsigned char _ZTV15CSTGStepSeqBase[0xc];
extern "C" unsigned char _ZTV17CSTGCommonStepSeq[0x6c];
struct CSTGProgram { CSTGProgram(); };
/*
 * CSTGCombi::CSTGCombi() (batch 45, `.text+0x8fb40`, 730 bytes) confirmed
 * real -- see src/engine/combi_ctor.cpp for the full derivation. Genuine
 * C++ multiple inheritance, same two base vtables as CSTGProgram
 * (`CSTGPerformance@+0x0`, `CSTGEffectRack@+0x4`), and the SAME sub-object
 * list up through `CSTGAudioInput@+0xae7` byte-for-byte (all already real
 * since batch 44) -- then diverges: overwrites +0x0 with its OWN derived
 * vtable (`_ZTV9CSTGCombi+8`, confirmed real 0x9c/39 slots), then
 * placement-constructs SIXTEEN embedded `CSTGProgramSlot` sub-objects at a
 * confirmed real 0xe8-byte stride (`+0xb63..+0x18fb`), each patched
 * afterward with its own zero-based index byte at +0x4, then one final
 * zeroed byte at `+0x19e3` (immediately past the 16th slot's own extent --
 * NOT a 17th sub-object, no ctor call there). Cross-checks cleanly against
 * `CSTGSequence`'s own already-confirmed layout below: its `CSTGHDRTrack`
 * array begins at `+0x19e7`, exactly 4 bytes after this ctor's own last
 * write, consistent with `CSTGCombi`'s own object footprint ending right
 * there and `CSTGSequence`'s derived tail picking up cleanly afterward.
 */
extern "C" unsigned char _ZTV9CSTGCombi[0x9c];
struct CSTGCombi { CSTGCombi(); };
/*
 * CSTGSequence::CSTGSequence() (sec 10.153, `.text+0xcbfd0`, 546 bytes)
 * confirmed real: calls the base `CSTGCombi::CSTGCombi()` first (Itanium
 * "base ctor first" pattern, matching CSTGProgramSlot's own derived-ctor
 * precedent -- `CSTGCombi::CSTGCombi()` is confirmed real too now, batch
 * 45, see src/engine/combi_ctor.cpp), installs its own vtable
 * (`_ZTV12CSTGSequence`, zero-
 * filled placeholder, never dispatched by anything reconstructed here),
 * then a confirmed real 44-byte-stride (`0x2c`) array of 16 embedded
 * "CSTGHDRTrack" sub-objects at `+0x19e7..+0x1c7b` (each: install its
 * OWN vtable `_ZTV12CSTGHDRTrack`, zero 3 bytes at +0x4/+0x5/+0x6 -- the
 * remaining 41 bytes of each 44-byte slot are a real, confirmed gap, not
 * independently touched by this ctor), and finally ONE more slot at the
 * SAME stride (`+0x1ca7`) holding a DIFFERENT vtable
 * (`_ZTV21CSTGMetronomeSettings`) with only ONE zero byte (+0x4) --
 * confirmed a real, deliberate variation (not a 17th CSTGHDRTrack), kept
 * as a separate write rather than folded into the loop. All sub-objects
 * modeled via raw offset writes (no named C++ sub-object types), matching
 * CSTGSequence's/CSTGProgramSlot's own established "opaque, no named
 * fields" convention -- none of these vtables are ever genuinely
 * dispatched through by anything reconstructed in this pass.
 */
extern "C" unsigned char _ZTV12CSTGSequence[12];
extern "C" unsigned char _ZTV12CSTGHDRTrack[12];
extern "C" unsigned char _ZTV21CSTGMetronomeSettings[12];
struct CSTGSequence : public CSTGCombi { CSTGSequence(); };
/* Activate() (sec 10.102, confirmed via relocation from CSTGGlobal::
 * CompletePerformanceActivation) confirmed real, deliberately deferred
 * extern -- own body not reconstructed. A DIFFERENT object from
 * `CSetListSlot` (sec 10.97) -- confirmed via the real disassembly
 * resolving both at genuinely different offsets in the same call site
 * (`+0x293374c`, idx-only stride, vs. `CSetListSlot`'s own
 * `+0x2933750 + idx*0x834 + idx2*0x10`). */
/*
 * CSetList::CSetList() -- same "no standalone symbol, fully inlined"
 * situation as CSTGWaveSequence::CSTGWaveSequence() just above: absent
 * from the whole symbol table, fully inlined at its own one call site
 * in CSTGGlobal::CSTGGlobal()'s 128-entry array loop (`.text+0x3a38`:
 * `movl $0x8,(%ecx)` with relocation `_ZTV8CSetList`). Real ctor effect
 * confirmed and now a real body (waveseq_setlist_init.cpp): vtable-
 * pointer install only (`_ZTV8CSetList+8`) -- the loop's own 128-entry
 * inner zero-fill and trailing byte are CSTGGlobal's OWN ctor code.
 * `_ZTV8CSetList` sized to its confirmed real 96 bytes (readelf,
 * `vtable for CSetList`), matching the CSTGWaveSequence precedent just
 * above.
 */
extern "C" unsigned char _ZTV8CSetList[96];
/*
 * CSetListEQ -- a plain (no vtable, no fields modeled beyond what
 * `CSetList::Activate()` writes into it) embedded sub-object type,
 * confirmed real but with its OWN body genuinely out of scope: see
 * `SetBand()` below. Not independently constructed anywhere in this
 * reconstruction -- always accessed as a raw sub-object at a fixed
 * offset inside a `CSTGPerformanceVarsManager` (`mgr+0x2160`), matching
 * the `CSTGEffectRackVars`/`CSTGAudioInputMixerBase` "reinterpret a
 * manager sub-region as a different class" idiom already used
 * elsewhere in this cluster (sec 10.90/10.101/oa_global.h comments
 * above).
 */
struct CSetListEQ {
	/*
	 * SetBand(unsigned int band, float gain) (batch 41, ground truth
	 * `.text+0x2025b0`, confirmed calling convention: `this` in %eax,
	 * `band` in %edx, `gain` on the stack at [ebp+8]) is genuine SSE/x87
	 * EQ-coefficient DSP (SSE broadcast + `CSTGEQ::CalculatePeakingBeta`
	 * + peaking-coefficient computation) -- out of scope per the sec
	 * 10.185 audio-DSP policy. Confirmed real, deliberately deferred:
	 * own body not reconstructed (safe no-op stub in bar2_stubs.cpp,
	 * matching the `SetPerfSwitch`/`CSTGControllerInfo` precedent for a
	 * confirmed-real-but-deferred callee).
	 */
	void SetBand(unsigned int band, float gain);
};
/*
 * CSetList::Activate() (batch 41, sec 10.192, ground truth
 * `.text+0x2012e0`, 266 bytes) reconstructed for real -- see
 * src/engine/global.cpp (right after the sibling `CSetListSlot::
 * Activate()`) for the full confirmed body. Resolves the active
 * `CSTGPerformanceVarsManager` via the shared
 * `ResolveActivePerformanceVarsManagerRaw()` helper, treats
 * `mgr+0x2160` as an embedded `CSetListEQ`, writes two of `this`'s own
 * fields into it (`+0x828`-gated mute-gain float at `+0x2168`, a raw
 * dword copy of `+0x82c` at `+0x2170`), then calls `SetBand()` nine
 * times (band 0..8, gain = `this`'s own nine contiguous floats at
 * `+0x804..+0x824`).
 */
struct CSetList { CSetList(); void Activate(); };

/*
 * CSTGControllerInfo -- confirmed real (relocation from CSTGGlobal::
 * ProcessCCSpecialMapping, sec 10.101), a sub-object embedded at a
 * confirmed real fixed offset (`+0xad3`) within a per-mode "target"
 * region (`CSTGProgram+3`/`CSTGCombi+6`/`CSTGSequence+0`, all
 * cross-confirmed against `UpdateVJSXAssignment`'s own mode
 * resolution, sec 10.77). `SetPerfSwitch` itself confirmed real,
 * deliberately deferred extern -- own body not reconstructed. Real
 * `ePerfSwitch` enum modeled as `int` (project convention).
 */
extern "C" unsigned char _ZTV18CSTGControllerInfo[0x60];
struct CSTGControllerInfo {
	/*
	 * CSTGControllerInfo::CSTGControllerInfo() (batch 44, `.text+0x90cb0`,
	 * 71 bytes) confirmed real: vtable-pointer install
	 * (`_ZTV18CSTGControllerInfo+8`) plus seventeen confirmed zeroed
	 * bytes at +0x4..+0x13 (contiguous run, one instruction per byte in
	 * the real disassembly) -- no dispatch. Own vtable 0x60 bytes (24
	 * slots), zero-filled placeholder (same "install vs dispatch"
	 * reasoning as the sibling classes above -- `SetPerfSwitch`/
	 * `SendUnsolicitedUIParam`/`OnPerformanceDeactivate` below are all
	 * non-virtual, confirmed via direct PC32 relocations, so nothing
	 * dispatches through this vtable either). Confirmed real object
	 * size 0x14 (20) bytes, independently cross-checked against
	 * `CSTGProgram::CSTGProgram()`'s own stride to the next embedded
	 * sub-object (`CSTGAudioInput` at +0xae7 == +0xad3 + 0x14 exactly).
	 */
	CSTGControllerInfo();
	void SetPerfSwitch(int perfSwitch, bool value);

	/*
	 * SendUnsolicitedUIParam(unsigned int, unsigned int, long,
	 * eSTGMidiSource) (sec 10.126, confirmed via relocation from
	 * CSTGControllerRTData::OnExtModePlayMuteSwitchAssignChange/
	 * OnExtModeSelectSwitchAssignChange) confirmed real, deliberately
	 * deferred extern -- own body not reconstructed. Confirmed STATIC
	 * (no implicit `this`): both real call sites use all 3 registers
	 * (eax/edx/ecx) plus one stack slot for the 4 explicit parameters,
	 * with nothing left over for a `this` pointer. Real `eSTGMidiSource`
	 * enum modeled as `int` (project convention for not-independently-
	 * defined enums).
	 */
	static void SendUnsolicitedUIParam(unsigned int paramId, unsigned int value,
					    long arg3, int midiSource);

	/* OnPerformanceDeactivate() (batch 19, `.text+0x92a90`, 106 bytes,
	 * confirmed via relocation from `CSTGPerformance::SetIsDying` --
	 * called there on the embedded sub-object at `+0xad3`); reconstructed
	 * for real in batch 36 -- see src/engine/
	 * controller_info_perf_deactivate.cpp for the full confirmed control
	 * flow (reads `CSTGControllerRTData::sInstance`'s own `+0x14`/`+0x15`
	 * flags, conditionally calls `this->SetPerfSwitch()` on each, then
	 * unconditionally calls `CSTGControllerRTData::sInstance->
	 * ResetPerfSwitches()` and clears four more `sInstance` fields
	 * directly). */
	void OnPerformanceDeactivate();
};

/*
 * CSetListSlot -- confirmed real (relocation from CSTGGlobal::
 * ProcessSetListSlotOnlyChange, sec 10.97): the per-slot record already
 * confirmed at `+0x2933740 + idx*0x834 + idx2*0x10` (sec 10.90/10.92,
 * originally accessed only via raw byte offsets) is reinterpreted
 * DIRECTLY as a `CSetListSlot*` for this one call -- i.e. that record
 * IS (or aliases) a real `CSetListSlot` object, not merely adjacent
 * data. `Activate()` itself is a confirmed-real, deliberately deferred
 * extern.
 */
struct CSetListSlot {
	/*
	 * Activate() (sec 10.141, .text+0x201490, 33 bytes) confirmed:
	 * resolves the active `CSTGPerformanceVarsManager` via the same
	 * shared `ResolveActivePerformanceVarsManagerRaw()` helper used
	 * throughout this codebase, then copies two of this object's own
	 * fields into it: `this->fieldAt(4)` -> `mgr->fieldAt(0x23f0)`,
	 * `this->fieldAt(0xc)` -> `mgr->fieldAt(0x23e0)`.
	 */
	void Activate();

	/*
	 * BeginActivation() (confirmed real via a relocation from
	 * ProcessPerformanceChange, sec 10.109/10.110; .text+0x201470, 25
	 * bytes) confirmed: writes `this->fieldAt(0x8)` (a byte) into
	 * `ResolveActivePerformanceVarsManagerRaw()`'s own resolved
	 * manager, at that manager's `+0x23ec` byte.
	 */
	void BeginActivation();
};

/*
 * CSTGPerfChangeRequest (sec 10.90) -- confirmed real 0x1c-byte stack
 * struct, ground-truthed independently via BOTH `BeginPerformanceChange`
 * and `BeginSetListSlotChange`'s own construction sequences (each builds
 * one on its stack frame and passes it to `SubmitPerfChangeRequest`),
 * with every field's role agreeing exactly between the two:
 *   +0x0 tag: 0 = BeginPerformanceChange, 1 = BeginSetListSlotChange
 *     (confirmed real discriminator, not guessed -- the two constructors
 *     write literal 0 vs 1 here and nothing else differs structurally)
 *   +0x4 mode: eGlobalMode for BeginPerformanceChange; always 0 for
 *     BeginSetListSlotChange (which has no mode parameter)
 *   +0x8/+0xc: two generic value slots (BeginPerformanceChange's own 2nd/
 *     3rd params; BeginSetListSlotChange's own 1st/2nd params -- same
 *     struct slots, different source values, confirmed via both
 *     constructors using the identical offsets)
 *   +0x10 source: eSTGPerformanceChangeSource (both constructors' own
 *     last parameter)
 *   +0x14/+0x18: confirmed always zeroed by both constructors; real
 *     meaning not independently determined (not yet observed written by
 *     anything other than 0 in either confirmed caller).
 * `SubmitPerfChangeRequest`/`ProcessPerfChangeRequest`/
 * `DoesPerfChangeRequestMatchType`/`GetPerformanceIdFromPerfChangeRequest`/
 * etc. (sec 10.13's original CSTGGlobal method survey) all take this by
 * reference -- their own bodies are a separate, not-yet-reconstructed
 * task; only the two `Begin*` producers are done in this pass.
 */
struct CSTGPerfChangeRequest {
	unsigned char tag;
	unsigned int mode;
	unsigned int value1;
	unsigned int value2;
	unsigned int source;
	unsigned int field14;
	unsigned int field18;
};

/*
 * CPerformanceId (sec 10.92) -- confirmed real 3-byte struct, ground-
 * truthed via GetPerformanceIdFromPerfChangeRequest's own construction:
 * its tag==0 branch writes a 16-bit value spanning `byte0`/`byte1`
 * simultaneously (a single `mov WORD PTR [ecx], dx` -- reproduced here
 * as two explicit byte writes from a packed 16-bit source, exactly
 * equivalent on this little-endian target) plus one more byte at `+0x2`;
 * its tag==1 branch writes all three bytes individually -- both branches
 * agree on the same 3 field offsets.
 */
struct CPerformanceId {
	unsigned char byte0;
	unsigned char byte1;
	unsigned char byte2;
};

/*
 * CSTGControllerValue (sec 10.92) -- confirmed real 0xc-byte struct,
 * ground-truthed via ResetAllControllers's own construction (the only
 * confirmed caller so far). Field `fieldB`'s own confirmed real quirk:
 * the real disassembly ORs it with `3` BEFORE ever explicitly
 * initializing it -- i.e. it genuinely reads whatever was already on
 * the stack at that address, not a bug in this reconstruction.
 *
 * CORRECTED/EXTENDED (sec 10.115, via CSTGMidiDispatcher::
 * PerfChangeControllerReset's own construction): `field0`, despite its
 * `unsigned int` declared type here (kept for source compatibility with
 * ResetAllControllers's own existing `= 0` usage), is confirmed to also
 * hold a raw FLOAT bit pattern in this second confirmed real
 * constructor -- reinterpret-cast through when writing/reading it as a
 * float, matching this project's established "raw bytes over asserted
 * semantics" convention for fields with more than one confirmed use.
 * Also independently reconfirms `fieldB`'s own uninitialized-read quirk,
 * this time via `(stale | 1) & ~2` rather than `| 3` -- a DIFFERENT
 * exact bit operation at a different call site, both real.
 */
struct CSTGControllerValue {
	unsigned int field0;
	float field4;
	unsigned short field8;
	unsigned char fieldA;
	unsigned char fieldB;
};

/*
 * CSTGParamsOwner -- confirmed real (relocation from CSTGGlobal::
 * ValidateParamChange, sec 10.92): the real call site passes CSTGGlobal's
 * own `this` UNADJUSTED as the callee's `this`, strongly suggesting
 * CSTGGlobal genuinely inherits from this class -- NOT modeled as a real
 * C++ base class here, to avoid disturbing every already-confirmed
 * absolute offset elsewhere in this file (this project doesn't currently
 * model any of CSTGGlobal's own base classes). Called instead via a
 * same-address `reinterpret_cast`, which reproduces an identical `this`
 * pointer value and therefore identical real behavior for this one
 * forwarding call. Own body a separate, not-yet-reconstructed task.
 */
class CSTGParamsOwner {
public:
	void ValidateParamChange(CSTGMessageContext &ctx, unsigned long paramId, const CValue &value);
};

/* GetSTGTickCount (ground-truth `GetSTGTickCount`, 16 bytes, extern "C") --
 * returns the STG tick counter, a u32 field deep inside the CSTGGlobal
 * singleton: `*(u32 *)((char *)CSTGGlobal::sInstance + 0x29c9fa8)` (adjacent
 * to the +0x29c9fa0 field lfo_stepseq_quad.cpp already reads). Body in
 * src/engine/tick_count.cpp (batch 35, sec 10.183); the daemon watchdog
 * signal_timed_out_daemons() is its primary caller. */
extern "C" unsigned int GetSTGTickCount(void);

class CSTGGlobal {
public:
	static CSTGGlobal *sInstance;

	/* Confirmed to exist (called via placement-new from
	 * setup_global_resources, init_module step 8) -- own body IS now
	 * reconstructed, see global_ctor.cpp (sec 10.56). */
	CSTGGlobal();

	/* .text+0x93b0, 74 bytes -- fully reconstructed, see global.cpp. */
	void IncrementMicrosecondCount();

	/*
	 * The following four are the smallest, cleanly-confirmed members of
	 * CSTGGlobal's ~150-strong `UpdateXXX` message-handler family --
	 * picked smallest-first, same methodology as the manager
	 * constructors. All four write into CSTGGlobal's own confirmed
	 * ~43.6MB-in field range (`+0x29c9d98`..`+0x29cc119`, consistent
	 * with the file header's documented `0x29c9900-0x29c9fc0` landing
	 * zone) via raw offset arithmetic, matching this file's established
	 * convention for a class whose layout is nowhere near recovered
	 * enough for named struct fields to make sense.
	 *   UpdateMuteMode()                  .text+0x1060  (9 bytes)
	 *   UpdateRearPanelControllerReset()  .text+0x1040  (12 bytes)
	 *   UpdateTmbrTrkOscTransposeType()   .text+0x1050  (12 bytes)
	 *   UpdateUserAllNoteScale()          .text+0xda0   (13 bytes)
	 * See global.cpp for the exact confirmed field offset each writes.
	 */
	void UpdateMuteMode(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateRearPanelControllerReset(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateTmbrTrkOscTransposeType(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * UpdateLRBusIndivAssign() (.text+0x2890, 20 bytes) confirmed: a
	 * DIFFERENT shape from the four above -- it doesn't write a
	 * CSTGGlobal field directly at all. It computes `(unsigned char
	 * *)this + 4` (confirmed via a `lea`, i.e. a genuine computed
	 * address, not a pointer load) and calls
	 * `CSTGAudioBusManager::SetLRBusIndivAssign(int)` (also newly
	 * reconstructed this batch, see oa_engine.h) on it, passing
	 * `param.value`. Why `CSTGGlobal+4` aliases as a
	 * `CSTGAudioBusManager*` is NOT determined in this pass (the "real"
	 * CSTGAudioBusManager singleton is separately allocated per
	 * `Initialize()`'s construction table, sec 10.13 -- this is
	 * confirmed to be a DIFFERENT pointer) -- flagged as an open
	 * question rather than asserted as "an embedded sub-object". */
	void UpdateLRBusIndivAssign(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateUserAllNoteScale(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * UpdateSPDIFSampleRate() (.text+0xf90, 23 bytes) confirmed: a
	 * DIFFERENT shape again -- writes into the MESSAGE CONTEXT (ctx),
	 * not `this`, and writes a literal constant (6), not `param.value`.
	 * Only fires when `this->flagAt(0x6ac) == 0` AND `param.value != 0`;
	 * otherwise a no-op. Real semantics of "flagAt(0x6ac)" and the
	 * significance of the constant `6` aren't confirmed -- represented
	 * factually (the confirmed condition and the confirmed literal
	 * write), not guessed at.
	 */
	void UpdateSPDIFSampleRate(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * TranslateAudioInputParamId(unsigned int) (.text+0x9340, 21 bytes)
	 * confirmed: a pure, `this`-independent lookup (the disassembly
	 * uses EAX -- the register that would hold an explicit first
	 * argument under -mregparm=3 if there were no implicit `this` --
	 * directly as the parameter, confirming this behaves as if static
	 * despite being a real `CSTGGlobal::` member per its mangled name).
	 * `idx = paramId - 2`; if `idx > 7` (unsigned compare, covers
	 * paramId < 2 too) return `12`; else return a confirmed 8-entry
	 * `.rodata` table `{13,12,15,16,12,12,49,50}` indexed by `idx`.
	 */
	static int TranslateAudioInputParamId(unsigned int paramId);

	/* SetSplitLayerWorkState(bool) (.text+0x9740, 7 bytes) confirmed:
	 * the smallest CSTGGlobal method found so far -- a direct byte
	 * store, no conversion needed since the bool argument's low byte is
	 * already the value to store. */
	void SetSplitLayerWorkState(bool state);

	/*
	 * UpdateFootswitchPolarity() (.text+0x28f0, 29 bytes) confirmed:
	 * another delegation, this time CONDITIONAL. Only calls
	 * `CSTGControllerRTData::SetFootSwitchPolarity(int)` (newly declared
	 * this batch, see above) on a pointer computed as `this+0x10` when
	 * `this->flagAt(0x6ae) != 0`; otherwise a no-op. Same "why does
	 * CSTGGlobal+0x10 alias as a CSTGControllerRTData*" open question as
	 * UpdateLRBusIndivAssign's `this+4` -- not determined, not guessed.
	 */
	void UpdateFootswitchPolarity(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * The `UpdateXXXMIDIChannel` family -- 23 methods, ALL confirmed
	 * (via a full programmatic disassembly scan of every one, not a
	 * spot check) to share the EXACT same 30-byte instruction shape as
	 * `UpdateSongPunchMIDIChannel` (the first one found, sec 10.33):
	 * `writeOffset` and `writeOffset+1` are two SEPARATE, independent
	 * byte fields (confirmed by re-reading the disassembly carefully
	 * after an initial mistaken "reads its own old value before
	 * overwrite" read didn't match a KAT round-trip for the first one --
	 * see MASTER_REFERENCE.md sec 10.33 for that correction)
	 * -- `writeOffset+1` is read-only here (an index selector,
	 * presumably set by other, not-yet-reconstructed code) and
	 * `writeOffset` is write-only here (the new channel value). Writes
	 * `param.value` to `writeOffset`; then, only if the `writeOffset+1`
	 * selector is non-negative as a signed byte, ALSO writes the same
	 * new value into an 8-byte-stride array at
	 * `+0x29cc11d + selector*8` -- the SAME shared array base confirmed
	 * identical across all 23, strongly suggesting one common
	 * per-logical-channel table indexed by whichever "current selector"
	 * each of these 23 fields separately tracks.
	 *
	 * The ORIGINAL binary duplicates this logic in each of the 23
	 * functions independently (confirmed: 23 separate .text addresses,
	 * not one shared routine with 23 callers) -- this reconstruction
	 * factors it into one shared private helper for maintainability,
	 * which every one of the 23 public methods below calls with its own
	 * confirmed `writeOffset`.
	 */
private:
	void UpdateMIDIChannelField(unsigned int writeOffset, STGConvertedParam &param);
public:
	void UpdateSongPunchMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);       /* .text+0x1280, +0x29cc0d0 */
	void UpdateRibbonLockMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);       /* .text+0x1540, +0x29cc0d8 */
	void UpdateJSYLockMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);          /* .text+0x16a0, +0x29cc0fc */
	void UpdateIncFuncMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);          /* .text+0x1cd0, +0x29cc10e */
	void UpdateSongStartMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);        /* .text+0x11d0, +0x29cc0ce */
	void UpdateChordSwMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);          /* .text+0x1e30, +0x29cc112 */
	void UpdateOctaveUpMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);         /* .text+0x13e0, +0x29cc0d4 */
	void UpdateAftertouchLockMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);   /* .text+0x1fa0, +0x29cc116 */
	void UpdateProgramUpMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);        /* .text+0x1070, +0x29cc0ca */
	void UpdateSW2FuncMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);          /* .text+0x1c20, +0x29cc10c */
	void UpdateJSPYRibLockMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);      /* .text+0x1a10, +0x29cc106 */
	void UpdateJSXLockMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);          /* .text+0x15f0, +0x29cc0fa */
	void UpdateJSYRibLockMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);       /* .text+0x1960, +0x29cc104 */
	void UpdateProgramDownMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);      /* .text+0x1120, +0x29cc0cc */
	void UpdateTapTempoMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);         /* .text+0x1330, +0x29cc0d2 */
	void UpdateJSMYRibLockMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);      /* .text+0x1ac0, +0x29cc108 */
	void UpdateDTrackEnableMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);     /* .text+0x1ef0, +0x29cc114 */
	void UpdateJSXRibLockMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);       /* .text+0x18b0, +0x29cc102 */
	void UpdateSW1FuncMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);          /* .text+0x1b70, +0x29cc10a */
	void UpdateJSMYLockMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);         /* .text+0x1800, +0x29cc100 */
	void UpdateDecFuncMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);          /* .text+0x1d80, +0x29cc110 */
	void UpdateOctaveDownMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);       /* .text+0x1490, +0x29cc0d6 */
	void UpdateJSPYLockMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);         /* .text+0x1750, +0x29cc0fe */

	/*
	 * UpdateRTKnobFuncMIDIChannel()/UpdatePadFuncMIDIChannel() (sec
	 * 10.67, .text+0x2050/0x2180, 52/53 bytes) confirmed: a genuine
	 * VARIANT of the `UpdateXXXMIDIChannel` family above -- shares its
	 * "mirror into the shared 8-byte-stride array at +0x29cc11d,
	 * indexed by a selector byte" tail, but everything upstream of
	 * that differs: instead of one fixed `writeOffset`, this variant
	 * is INDEXED by `ctx.index` (confirmed real field, +0x4, already
	 * documented above) against an 8-slot per-value array AND an
	 * 8-slot parallel selector array, gated by a confirmed bounds
	 * check (`ctx.index > 7` -> no-op, unsigned compare) the plain
	 * MIDI-channel family doesn't have. Confirmed via a full
	 * disassembly of both, not a spot check.
	 */
private:
	void UpdateIndexedMIDIChannelField(unsigned int valueArrayOffset,
					    unsigned int selectorArrayOffset,
					    CSTGMessageContext &ctx, STGConvertedParam &param);
public:
	void UpdateRTKnobFuncMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);  /* .text+0x2050, +0x29cc0da/+0x29cc0e2 */
	void UpdatePadFuncMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);      /* .text+0x2180, +0x29cc0ea/+0x29cc0f2 */

	/*
	 * UpdateAudioClockSource() (sec 10.67, .text+0x23b0, 52 bytes)
	 * confirmed: gated by `this->flagAt(0x6ae)` (the same confirmed
	 * "initialized" flag UpdateFootswitchPolarity/Initialize already
	 * use); maps `param.value` (1 -> 0x7800000, 2 -> 0x7020000, else
	 * -> 0x7000000) to a hardware command code and calls the
	 * confirmed-real, deliberately deferred extern
	 * `OmapNKS4OutputFifo_WriteCommand(int)` -- real semantics of the
	 * three command codes not independently confirmed beyond their
	 * literal values.
	 */
	void UpdateAudioClockSource(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * UpdateFootSwitchAssign() (sec 10.67, .text+0x2910, 52 bytes)
	 * confirmed: gated by `this->flagAt(0x6ae)`; looks up
	 * `param.value` (sign-extended byte) in a confirmed real 55-entry
	 * `.rodata` table (`kFootSwitchAssignMap`, extracted directly from
	 * the binary, not guessed) and calls
	 * `CSTGControllerRTData::SetControllerAssignment` on the embedded
	 * `this+0x10` sub-object, passing a self-reference to that SAME
	 * object as arg1 (confirmed: the call's arg1 register is set to
	 * the identical value as the call's own `this`), the table lookup
	 * as arg2, and a literal `false` as arg3.
	 */
	void UpdateFootSwitchAssign(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * UpdateFootPedalAssign() (sec 10.69, .text+0x2950, 73 bytes)
	 * confirmed: gated by `this->flagAt(0x6ae)`; looks up
	 * `param.value` in a DIFFERENT confirmed real 32-entry `.rodata`
	 * table (`kFootPedalAssignMap`, extracted directly from the
	 * binary), always writes the raw lookup into `this->fieldAt(0x18)`
	 * (a dword), but passes `0` instead when `this->flagAt(0x15) == 0`.
	 * Calls the SAME `SetControllerAssignment` as
	 * `UpdateFootSwitchAssign`, but on a DIFFERENT arg1 target
	 * (`this+0x13`, NOT the call's own `this+0x10` -- confirmed NOT a
	 * self-reference here, see `SetControllerAssignment`'s own comment
	 * above) and with a literal `true` for arg3 (vs `false` for
	 * FootSwitch).
	 */
	void UpdateFootPedalAssign(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * UpdateKnobFaderMode() (sec 10.69, .text+0x28b0, 63 bytes)
	 * confirmed: bool-converts `param.value`, swaps it into
	 * `this->fieldAt(0x29c9fc0)` (keeping the OLD value for comparison
	 * -- a genuine read-before-write, unlike every other bool-store
	 * handler so far). If `this->flagAt(0x6ae) != 0` AND the value
	 * actually CHANGED, calls
	 * `CSTGControllerRTData::sInstance->ResetAllJumpCatch()` -- both
	 * newly confirmed-real, deliberately deferred additions to
	 * `CSTGControllerRTData` (a real static singleton pointer, DISTINCT
	 * from the embedded `this+0x10` sub-object used everywhere else in
	 * this file; see that class's own declaration above).
	 */
	void UpdateKnobFaderMode(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * UpdateConvertPosition() (sec 10.70, .text+0x22b0, 76 bytes)
	 * confirmed: always writes `param.value` to `this->fieldAt(0x6b0)`
	 * at the end, but FIRST conditionally calls a newly confirmed-real,
	 * deliberately deferred extern, `CSTGVoiceAllocator::sInstance->
	 * StealAllVoices()` -- ONLY when `this->flagAt(0x6ae) != 0` (the
	 * usual "initialized" gate) AND a SECOND newly confirmed-real
	 * singleton's own flag is clear: `CSTGMessageProcessor::
	 * sInstance->flagAt(0x48) == 0`. `CSTGMessageProcessor` itself is
	 * already a confirmed real class in this project (oa_engine.h,
	 * sec 10.5x) -- this is the first confirmed use of its own
	 * `+0x48` field.
	 */
	void UpdateConvertPosition(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * UpdateUserOctaveScale() (sec 10.70, .text+0xd50, 79 bytes)
	 * confirmed: `ctx.index` is divided by 12 (confirmed real signed
	 * division via a magic-multiply constant, `0x2aaaaaab`, the
	 * standard division-by-12 idiom -- represented here with plain C
	 * `/`/`%` since they produce identical results for the observed
	 * input domain) into a quotient (truncated to a byte) and
	 * remainder; writes `param.value` into a confirmed real 2D table
	 * at `this->fieldAt(0x29c9a98 + remainder*4 + byteQuotient*48)` --
	 * i.e. a `[byteQuotient][remainder]` layout, 12 dword entries per
	 * row (48-byte stride), consistent with the handler's own name
	 * (12 = semitones/octave, `remainder` = note-within-octave,
	 * `quotient` = which octave-scale table).
	 */
	void UpdateUserOctaveScale(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * `ResolveActivePerformanceVarsManager()` -- a shared private
	 * helper factored out for `UpdatePerfChangeHoldTime`/
	 * `UpdateExtSetSelect` (sec 10.71), both of which independently
	 * reference the exact same `CSTGPerformanceVarsManager::sInstance`
	 * sub-fields (confirmed via a full disassembly of both, not a spot
	 * check) -- see `CSTGPerformanceVarsManager::sInstance`'s own
	 * corrected declaration above for the confirmed real 12-byte
	 * layout this resolves.
	 */
private:
	static CSTGPerformanceVarsManager *ResolveActivePerformanceVarsManager();
public:
	/*
	 * UpdatePerfChangeHoldTime() (sec 10.71, .text+0xfe0, 89 bytes)
	 * confirmed: converts `param.value` (as a float) times
	 * `CSTGAudioBusManager::sInstance->fieldAt(4)` (also a float) to
	 * an integer via truncation (the real `fisttp` instruction always
	 * truncates toward zero regardless of FPU rounding mode, matching
	 * C's own float-to-int truncation semantics exactly); ALWAYS
	 * stores the result into `this->fieldAt(0x6e0)`. Then, only if
	 * `this->flagAt(0x6ae) != 0`, resolves the active
	 * `CSTGPerformanceVarsManager` (see the shared helper above) and,
	 * only if ITS `flagAt(0x23d1) == 2` AND `flagAt(0x23dc) == 0`,
	 * ALSO stores the SAME converted value into ITS `fieldAt(0x23e0)`.
	 */
	void UpdatePerfChangeHoldTime(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * UpdateExtSetSelect() (sec 10.71, .text+0x2430, 95 bytes)
	 * confirmed: always stores `param.value` (as a byte) into
	 * `this->fieldAt(0x29cc0c8)`. Real branch structure, preserved
	 * exactly (a genuine quirk, not simplified): if NOT initialized
	 * (`flagAt(0x6ae) == 0`), unconditionally sets bit 0 of
	 * `this->fieldAt(0x29cc0c9)` and returns. If initialized AND that
	 * bit is ALREADY set, re-sets it (a redundant but real no-op) and
	 * returns. Otherwise (initialized, bit clear), resolves the active
	 * `CSTGPerformanceVarsManager` and, ONLY if its `flagAt(0x23d1) ==
	 * 2`, calls `CSTGControllerRTData::sInstance->
	 * OnExtModeSetChange()` and returns WITHOUT setting the bit --
	 * confirmed via the real jump target (skips the bit-set path
	 * entirely in this one branch) -- otherwise falls through to set
	 * the bit anyway.
	 */
	void UpdateExtSetSelect(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * The 8 `UpdateExtXXXCCAssign`/`UpdateExtXXXMidiChannel` handlers
	 * (sec 10.72, all 122 bytes) confirmed: a real, SHARED shape across
	 * all 8 (confirmed via a full disassembly of each, not a spot
	 * check), differing only in a per-pair write offset, a per-family
	 * array stride, and the specific `OnExtModeXXXAssignChange` call
	 * target:
	 *   `slotIndex = this->fieldAt(0x29cc0c8)` (the SAME "active ext
	 *   set" field `UpdateExtSetSelect` writes, sec 10.71 -- confirmed
	 *   real cross-handler coupling, not coincidental); `slot = this +
	 *   slotIndex*STRIDE + ctx.index` (STRIDE is 8 for the Knob/
	 *   PlayMuteSwitch/SelectSwitch families, but 9 for Slider --
	 *   confirmed via the real `lea` instruction's own multiplier,
	 *   likely reflecting a genuine hardware count difference: one more
	 *   physical slider than knob/switch on the real front panel);
	 *   `slot->fieldAt(WRITE_OFFSET) = (byte)param.value` (always).
	 *   Then the EXACT SAME confirmed real branch/quirk structure as
	 *   `UpdateExtSetSelect` above (same `+0x29cc0c9` bit0, same
	 *   `ResolveActivePerformanceVarsManager()` gate, same "one branch
	 *   skips the bit-set" quirk) -- factored into a shared private
	 *   helper below, parameterized by (writeOffset, stride, the
	 *   notification call).
	 */
private:
	template <typename NotifyFn>
	void UpdateExtAssign(unsigned int writeOffset, unsigned int stride,
			      CSTGMessageContext &ctx, STGConvertedParam &param,
			      NotifyFn notify);
public:
	void UpdateExtKnobCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);           /* .text+0x2790, write +0x29ca3c8, stride 8 */
	void UpdateExtKnobMidiChannel(CSTGMessageContext &ctx, STGConvertedParam &param);        /* .text+0x2810, write +0x29c9fc8, stride 8 */
	void UpdateExtPlayMuteSwitchCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param); /* .text+0x2690, write +0x29cabc8, stride 8 */
	void UpdateExtPlayMuteSwitchMidiChannel(CSTGMessageContext &ctx, STGConvertedParam &param); /* .text+0x2710, write +0x29ca7c8, stride 8 */
	void UpdateExtSelectSwitchCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);   /* .text+0x2590, write +0x29cb3c8, stride 8 */
	void UpdateExtSelectSwitchMidiChannel(CSTGMessageContext &ctx, STGConvertedParam &param); /* .text+0x2610, write +0x29cafc8, stride 8 */
	void UpdateExtSliderCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);         /* .text+0x2490, write +0x29cbc48, stride 9 */
	void UpdateExtSliderMidiChannel(CSTGMessageContext &ctx, STGConvertedParam &param);      /* .text+0x2510, write +0x29cb7c8, stride 9 */

	/*
	 * The 3 `UpdateXXXDisable` handlers (sec 10.73, 124/125/128 bytes)
	 * confirmed: a real SHARED shape across all 3 (confirmed via a
	 * full disassembly of each, not a spot check) -- each toggles a
	 * confirmed 2-BIT mask within `this->fieldAt(0x6d4)` based on
	 * whether `param.value != 0` (the disassembly's own multi-
	 * instruction shift/OR sequence is confirmed algebraically
	 * equivalent to the simple formula `flags = (flags & ~MASK) |
	 * (bit ? MASK : 0)`, verified by working through each handler's
	 * own bit arithmetic by hand): MFX toggles bits 1+4 (mask 0x12),
	 * IFX toggles bits 0+3 (mask 0x09), TFX toggles bits 2+5 (mask
	 * 0x24) -- three DIFFERENT non-overlapping 2-bit pairs within the
	 * same byte, consistent with three independent effect-bus enable
	 * flags packed into one status byte.
	 *
	 * Then, only if `this->flagAt(0x6ae) != 0` (initialized), sends a
	 * real 3-byte MIDI Control Change message via
	 * `CSTGMidiPortManager::sInstance->WriteSTGMidiOutQueue()`:
	 * `{0xB0 | this->fieldAt(0x6b8), ccNumber, value}` -- a genuine
	 * MIDI status byte (0xBn = Control Change on channel n, n taken
	 * from a confirmed real per-CSTGGlobal field), a per-handler
	 * literal CC number (MFX=0x5e, IFX=0x5c, TFX=0x5f), and a value of
	 * `0x7f` when `param.value == 0` or `0x00` otherwise (the standard
	 * MIDI on/off convention, confirmed via the real `cmp/sbb` idiom).
	 * Factored into a shared private helper, `UpdateFXDisable()`,
	 * parameterized by (mask, ccNumber).
	 */
private:
	void UpdateFXDisable(unsigned char mask, unsigned char ccNumber,
			      CSTGMessageContext &ctx, STGConvertedParam &param);
public:
	void UpdateMFXDisable(CSTGMessageContext &ctx, STGConvertedParam &param); /* .text+0x3020, mask 0x12, cc 0x5e */
	void UpdateIFXDisable(CSTGMessageContext &ctx, STGConvertedParam &param); /* .text+0x2f20, mask 0x09, cc 0x5c */
	void UpdateTFXDisable(CSTGMessageContext &ctx, STGConvertedParam &param); /* .text+0x2fa0, mask 0x24, cc 0x5f */

	/*
	 * The `UpdateXXXCCAssign` family (sec 10.74, 22 confirmed members
	 * at exactly 141 bytes, confirmed via a full disassembly of every
	 * one, not a spot check) -- the real CC-assign COUNTERPART to the
	 * already-reconstructed `UpdateXXXMIDIChannel` family (sec 10.33/
	 * 10.34): each `UpdateXXXCCAssign`'s own `selectorOffset` is
	 * confirmed to be EXACTLY `writeOffset+1` of its correspondingly-
	 * named `UpdateXXXMIDIChannel` handler -- i.e. CCAssign WRITES the
	 * very field MIDIChannel only ever READ as a selector, confirming
	 * real, designed coupling between the two families (CCAssign
	 * registers "which CC number is currently assigned"; MIDIChannel
	 * later consults that same field to also update a shared 8-byte-
	 * stride mirror array at `+0x29cc11d`, sec 10.33).
	 *
	 * Real confirmed logic, genuinely richer than any prior `UpdateXXX`
	 * batch -- a 120-slot (0x78) claim table at `+0x29cc11c`, 8 bytes/
	 * slot (`+0x0` claimed-flag, `+0x1` value byte, `+0x2` free-flag,
	 * `+0x4` a per-handler literal "tag" dword identifying which
	 * logical control owns that slot):
	 *   1. If `(signed char)param.value <= 0x77` (119): scan all 120
	 *      slots; for each one whose free-flag==0 AND tag==THIS
	 *      handler's own tag, clear its claimed-flag (un-claims any
	 *      slot this SAME handler previously claimed, before
	 *      potentially claiming a new one below) -- confirmed real,
	 *      NOT gated by whether `param.value==0xff` (runs even on the
	 *      deassign path, just skipped when `param.value` is a large
	 *      unsigned-as-negative value > 0x77).
	 *   2. If `param.value != 0xff`: writes `param.value` into this
	 *      handler's own `selectorOffset`, then claims slot
	 *      `[param.value]` (sets claimed-flag=1, free-flag=0, tag=THIS
	 *      handler's tag, value byte = `this->fieldAt(selectorOffset-1)`
	 *      -- the paired MIDIChannel field's CURRENT value).
	 *   3. Else (`param.value == 0xff`, "deassign"): reads the CURRENT
	 *      selector; if it's a valid previous assignment (`(signed
	 *      char)oldSelector >= 0`), sets the selector to `0xff` and
	 *      clears that old slot's claimed-flag. Otherwise (already
	 *      sentinel-like), CONFIRMED via the real jump target to fall
	 *      through into the EXACT SAME step-2 code with `param.value`
	 *      still `0xff` (an idempotent "claim slot 0xff" no-op, not a
	 *      distinct third code path -- the original binary literally
	 *      jumps into the middle of the assign block, reusing it).
	 *
	 * Factored into one shared private helper, `UpdateCCAssign()`,
	 * parameterized by (selectorOffset, tag).
	 */
private:
	void UpdateCCAssign(unsigned int selectorOffset, unsigned int tag,
			     CSTGMessageContext &ctx, STGConvertedParam &param);
public:
	void UpdateProgramUpCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);        /* selector +0x29cc0cb, tag 0x14 */
	void UpdateProgramDownCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);      /* selector +0x29cc0cd, tag 0x15 */
	void UpdateSongStartCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);        /* selector +0x29cc0cf, tag 0x16 */
	void UpdateSongPunchCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);        /* selector +0x29cc0d1, tag 0x17 */
	void UpdateTapTempoCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);         /* selector +0x29cc0d3, tag 0x18 */
	void UpdateOctaveUpCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);         /* selector +0x29cc0d5, tag 0x04 */
	void UpdateOctaveDownCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);       /* selector +0x29cc0d7, tag 0x03 */
	void UpdateRibbonLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);       /* selector +0x29cc0d9, tag 0x09 */
	void UpdateJSXLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);          /* selector +0x29cc0fb, tag 0x05 */
	void UpdateJSYLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);          /* selector +0x29cc0fd, tag 0x06 */
	void UpdateJSPYLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);         /* selector +0x29cc0ff, tag 0x07 */
	void UpdateJSMYLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);         /* selector +0x29cc101, tag 0x08 */
	void UpdateJSXRibLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);       /* selector +0x29cc103, tag 0x0a */
	void UpdateJSYRibLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);       /* selector +0x29cc105, tag 0x0b */
	void UpdateJSPYRibLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);      /* selector +0x29cc107, tag 0x0c */
	void UpdateJSMYRibLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);      /* selector +0x29cc109, tag 0x0d */
	void UpdateSW1FuncCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);          /* selector +0x29cc10b, tag 0x24 */
	void UpdateSW2FuncCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);          /* selector +0x29cc10d, tag 0x25 */
	void UpdateIncFuncCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);          /* selector +0x29cc10f, tag 0x3e */
	void UpdateDecFuncCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);          /* selector +0x29cc111, tag 0x3f */
	void UpdateDTrackEnableCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);     /* selector +0x29cc115, tag 0x41 */
	void UpdateAftertouchLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);   /* selector +0x29cc117, tag 0x0e */

	/*
	 * UpdateChordSwCCAssign() (sec 10.75, .text+0x1e50, 149 bytes)
	 * confirmed: the 23rd real member of the `UpdateXXXCCAssign`
	 * family above (selector +0x29cc113, tag 0x40) -- identical shape,
	 * 8 bytes larger only because the real compiled code contains one
	 * extra, confirmed BEHAVIORALLY-REDUNDANT store (re-reads
	 * `param.value` fresh and re-writes the identical byte already
	 * written moments earlier by the shared assign path) -- verified
	 * by tracing that nothing modifies `param` in between, so the
	 * final observable state is identical to calling the shared
	 * `UpdateCCAssign()` helper directly; not given a special-cased
	 * implementation since there's no behavioral difference to model.
	 */
	void UpdateChordSwCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * UpdateProgramChangeEnable()/UpdateBankChangeEnable() (sec 10.75,
	 * .text+0xdb0/0xe50, 155 bytes each) confirmed: a real SHARED shape
	 * -- bool-converts `param.value`, stores it to a per-handler byte
	 * field (`+0x6d5`/`+0x6d6`), and, ONLY when the NEW value is
	 * `false` (turning the feature OFF), zeroes 32 confirmed real byte
	 * fields on `CSTGMidiDispatcher::sInstance` (an interleaved pair of
	 * 16-byte ranges, `+0x30..+0x3f` and `+0x50..+0x5f` -- confirmed
	 * real, likely a per-MIDI-channel "last received program/bank"
	 * cache, cleared on disable but left untouched on enable). Factored
	 * into a shared private helper, `UpdateChangeEnable()`.
	 */
private:
	void UpdateChangeEnable(unsigned int flagOffset, CSTGMessageContext &ctx, STGConvertedParam &param);
public:
	void UpdateProgramChangeEnable(CSTGMessageContext &ctx, STGConvertedParam &param); /* .text+0xdb0, flag +0x6d5 */
	void UpdateBankChangeEnable(CSTGMessageContext &ctx, STGConvertedParam &param);    /* .text+0xe50, flag +0x6d6 */

	/*
	 * UpdateMasterTune() (sec 10.75, .text+0x3810, 147 bytes) confirmed:
	 * always stores `param.value` (reinterpreted as a float on the
	 * receiving end) into `this->fieldAt(0x6bc)`, unconditionally --
	 * even before the `+0x6ae` "initialized" gate is checked. Only if
	 * initialized, walks 16 confirmed real intrusive linked lists (an
	 * array of 16 list-head pointers at `+0x29c99cc` on the
	 * `CSTGGlobal::sInstance` singleton, 12-byte stride -- confirmed
	 * real via the disassembly's own `i*3` index-then-`*4` scale,
	 * i.e. `i*12`), calling a newly confirmed-real, deliberately
	 * deferred extern, `CSTGSlotVoiceData::UpdateGlobalTune(float)`,
	 * on each list node. Each list node's own real shape is confirmed
	 * as: `next` pointer at the node's own `+0x0`, and the actual
	 * `CSTGSlotVoiceData*` call target read from the node's own `+0x8`
	 * -- i.e. the node is NOT itself the `CSTGSlotVoiceData` object,
	 * it's a separate list-node structure holding a pointer to one
	 * (own real field layout not independently recovered beyond these
	 * two confirmed offsets). Additionally, when the loop index `i`
	 * equals `CSTGGlobal::sInstance->byteAt(0x6b8)` (the "currently
	 * active slot" field, already confirmed real elsewhere in this
	 * project -- confirmed read from `sInstance`, not `this`, via the
	 * disassembly's own explicit re-fetch of the singleton pointer
	 * right before this read), ALSO walks a SEPARATE SPECIAL list at
	 * `CSTGGlobal::sInstance->
	 * fieldAt(0x29c9a8c)` the exact same way.
	 */
	void UpdateMasterTune(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * UpdateRTKnobFuncCCAssign()/UpdatePadFuncCCAssign() (sec 10.76,
	 * .text+0x2090/0x21c0, 226 bytes each) confirmed: the INDEXED
	 * variant of the `UpdateXXXCCAssign` family above, the direct
	 * CC-assign counterpart to `UpdateRTKnobFuncMIDIChannel`/
	 * `UpdatePadFuncMIDIChannel` (sec 10.68) the exact same way the
	 * plain `UpdateXXXCCAssign` family pairs with the plain
	 * `UpdateXXXMIDIChannel` family. Shares the SAME 120-slot claim
	 * table and overall algorithm as the plain family, with three
	 * confirmed differences:
	 *   1. Also bound-checked against `ctx.index > 7` (matching the
	 *      indexed MIDIChannel variant's own confirmed bounds check).
	 *   2. The "selector" field is an 8-entry array indexed by
	 *      `ctx.index` (`selectorArrayOffset + ctx.index`) -- the SAME
	 *      array `UpdateXXXFuncMIDIChannel` reads as its own selector
	 *      (RTKnob: `+0x29cc0e2`; Pad: `+0x29cc0f2`) -- rather than one
	 *      fixed per-handler field.
	 *   3. The claim "tag" is DYNAMIC (`ctx.index + tagBase`, confirmed
	 *      real literal per family: RTKnob `+0x1c`, Pad `+0x36`) rather
	 *      than one fixed per-handler constant.
	 * The paired "value byte" copied into each claimed slot is read
	 * from the SAME `valueArrayOffset` array `UpdateXXXFuncMIDIChannel`
	 * itself writes (RTKnob: `+0x29cc0da`; Pad: `+0x29cc0ea`).
	 * Factored into a shared private helper, `UpdateIndexedCCAssign()`.
	 */
private:
	void UpdateIndexedCCAssign(unsigned int valueArrayOffset, unsigned int selectorArrayOffset,
				    unsigned int tagBase, CSTGMessageContext &ctx, STGConvertedParam &param);
public:
	void UpdateRTKnobFuncCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param); /* value +0x29cc0da, selector +0x29cc0e2, tagBase 0x1c */
	void UpdatePadFuncCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param);    /* value +0x29cc0ea, selector +0x29cc0f2, tagBase 0x36 */

	/*
	 * UpdateVJSXAssignment()/UpdateVJSYAssignment() (sec 10.77,
	 * .text+0x2aa0/0x29a0, 254 bytes each) confirmed: a real SHARED
	 * shape (confirmed via a full disassembly of both -- identical
	 * instruction-for-instruction except for the per-handler selector
	 * field, +0x6c0 for X vs +0x6c1 for Y).
	 *
	 * Always stores `param.value` (as a byte) into the per-handler
	 * selector field, even BEFORE the `+0x6ae` "initialized" gate.
	 * Only if initialized, resolves "the current performance object"
	 * via a real, confirmed 3-way mode dispatch on
	 * `this->fieldAt(0x684)`:
	 *   mode==1: a `CSTGCombi`-object address (index `this->fieldAt
	 *     (0x69c) & 0x7f`, bank `this->fieldAt(0x690)`, confirmed
	 *     real per-object/per-bank strides `0x19e7`/`0xcf381` --
	 *     `0x19e7` independently cross-checks CSTGCombi's own
	 *     already-confirmed size, sec 10.56).
	 *   mode==2: a `CSTGSequence`-object address (index `this->
	 *     fieldAt(0x6a0)`, no banking -- confirmed real stride
	 *     `0x1cad`, independently cross-checks CSTGSequence's own
	 *     already-confirmed size, sec 10.56).
	 *   default (mode 0 or other): a `CSTGProgram`-object address
	 *     UNLESS `this->fieldAt(0x698) == 0xfffe` (a confirmed real
	 *     sentinel, in which case a single FIXED special address is
	 *     used instead -- likely a safety-default program object);
	 *     otherwise index `this->fieldAt(0x698) & 0x7f`, bank
	 *     `this->fieldAt(0x68c)`, confirmed real strides `0xcec`/
	 *     `0x67603` (`0xcec` independently cross-checks CSTGProgram's
	 *     own already-confirmed size, sec 10.56).
	 * Calls `CSTGPerformance::IsCurrentlyActive()` (a real, direct
	 * non-virtual call, NOT a vtable dispatch, confirmed real,
	 * deliberately deferred extern) on the resolved address, cast to
	 * `CSTGPerformance*` (the +3/+6 byte adjustments confirmed real in
	 * the address computation are likely a base-class-subobject
	 * offset within the real `CSTGProgram`/`CSTGCombi`/`CSTGSequence`
	 * class hierarchies -- not independently confirmed beyond their
	 * literal values). If NOT active, or if the just-stored selector
	 * field is negative as a signed byte, returns. Otherwise sends a
	 * real MIDI controller message via `CSTGMidiDispatcher::sInstance
	 * ->HandleController()`.
	 *
	 * Factored into one shared private helper, `UpdateVJSAssignment()`,
	 * plus a shared static `ResolveCurrentPerformance()`.
	 */
private:
	void UpdateVJSAssignment(unsigned int selectorFieldOffset,
				  CSTGMessageContext &ctx, STGConvertedParam &param);
public:
	void UpdateVJSXAssignment(CSTGMessageContext &ctx, STGConvertedParam &param); /* selector +0x6c0 */
	void UpdateVJSYAssignment(CSTGMessageContext &ctx, STGConvertedParam &param); /* selector +0x6c1 */

	/*
	 * UpdateProgramDrumTrackMidiChannel() (sec 10.78, .text+0x2300,
	 * 167 bytes) confirmed: if initialized, sends `ResetAllControllers`
	 * for the OLD channel (`+0x6ba`) first -- gated by
	 * `CSTGMessageProcessor::sInstance->flagAt(0x48)`: when clear, ALSO
	 * calls `CSTGSmoother::sInstance->CancelAllSmoothers()` and
	 * `CSTGVoiceAllocator::sInstance->StealAllVoices()` first. Then
	 * ALWAYS stores the new channel and calls the confirmed embedded
	 * `CSTGProgramModeDrumTrackSlot` sub-object's own
	 * `OnUpdateProgramDrumTrackMidiChannel()`. Finally, if initialized,
	 * sends a SECOND `ResetAllControllers` for the NEW channel.
	 */
	void UpdateProgramDrumTrackMidiChannel(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * UpdateKeyTranspose() (sec 10.78, .text+0x2e50, 194 bytes)
	 * confirmed: if NOT initialized, just stores `param.value` and
	 * returns. If initialized, conditionally calls `CSTGVoiceAllocator
	 * ::sInstance->StealAllVoices()` (gated by `CSTGMessageProcessor::
	 * sInstance->flagAt(0x48) == 0`), stores the new value, then
	 * (after a confirmed real redundant re-check of the initialized
	 * flag) sends a real 5-byte SysEx-like message via a newly
	 * confirmed-real extern, `CSTGMidiQueueWriter::Write()`, on the
	 * embedded queue-writer sub-object at `CSTGMidiPortManager::
	 * sInstance + 0x208`: `{channelByte, 0x79, 0x09, 0x05, 0xff}`.
	 */
	void UpdateKeyTranspose(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * UpdateLocalControl(bool) (sec 10.78, .text+0x8b20, 207 bytes)
	 * confirmed: NOT a `(CSTGMessageContext&, STGConvertedParam&)`
	 * handler -- takes a plain `bool` (confirmed real signature from
	 * its own mangled name). If not initialized, just stores the bool
	 * and returns. If initialized, conditionally steals voices (same
	 * gate as `UpdateKeyTranspose`), sends `ResetAllControllers` for
	 * the current channel, stores the bool, and (if still initialized)
	 * sends the same kind of 5-byte message as `UpdateKeyTranspose`
	 * but with byte[2] encoding the bool (`5` if true, `6` if false --
	 * confirmed via the real `sbb`/`not`/`add` idiom), then calls
	 * `CSTGControllerRTData::sInstance->ResetAllJumpCatch()` (sec
	 * 10.69's own extern).
	 */
	void UpdateLocalControl(bool state);

	/*
	 * UpdateLocalOn() (sec 10.78, .text+0x30a0, 215 bytes) confirmed:
	 * CONFIRMED, via a full disassembly comparison, to be functionally
	 * IDENTICAL to `UpdateLocalControl(param.value != 0)` -- the only
	 * difference is the bool-vs-int-to-5/6 conversion idiom
	 * (`neg`/`add` here vs `sbb`/`not`/`add` there), which produce the
	 * exact same result for a 0/1 input. Not given its own
	 * independent body.
	 */
	void UpdateLocalOn(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * UpdateMIDIChannel() (sec 10.78, .text+0x2ce0, 362 bytes) --
	 * the LARGEST and LAST `UpdateXXX` handler, confirmed real,
	 * genuinely substantial:
	 *
	 * If initialized: gated by the same `CSTGMessageProcessor`
	 * flag as the other handlers above -- when CLEAR, walks the
	 * confirmed 16-entry per-MIDI-channel list array at `+0x29c99cc`
	 * (the SAME array `UpdateMasterTune`, sec 10.75, already
	 * confirmed -- here indexed by the OLD channel value, not walked
	 * in full), calling `CSTGSmoother::CancelAllSmoothers()` and
	 * `CSTGVoiceAllocator::StealAllVoices()` first, then for each list
	 * node whose confirmed `+0x8` payload pointer is non-null, calls
	 * a newly confirmed-real extern, `CSTGHeldKeyList::Reset()`, on
	 * `payload+0x1e80`. Either way, sends `ResetAllControllers` for
	 * the OLD channel.
	 *
	 * ALWAYS (initialized or not): stores the new channel
	 * (`+0x6b8`), calls a newly confirmed-real extern,
	 * `CSTGVectorManager::OnUpdateGlobalMidiChannel()` (on the
	 * `CSTGVectorManager` singleton, sec 10.64), and, UNLESS
	 * `this->fieldAt(0x684) == 2` (the SAME "mode" field
	 * `UpdateVJSXAssignment`/`UpdateVJSYAssignment` use, sec 10.77),
	 * ALSO updates a second confirmed channel field (`+0x6b9`). Then
	 * calls the confirmed embedded `CSTGProgramModeProgramSlot`
	 * sub-object's own `OnUpdateGlobalMidiChannel()`.
	 *
	 * If (still) initialized: resolves the active
	 * `CSTGPerformanceVarsManager` (the same shared helper used by
	 * `UpdatePerfChangeHoldTime`/`UpdateExtSetSelect`, sec 10.71) and,
	 * if its `flagAt(0x23d1) == 2`, calls a newly confirmed-real
	 * extern, `CSTGEffectRackVars::UpdateDModRoutings()`, on
	 * `mgr+0x20`; otherwise sends the SAME kind of 5-byte message as
	 * `UpdateKeyTranspose`/`UpdateLocalControl`, with byte[2] fixed at
	 * `0x08`.
	 */
	void UpdateMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * The following twelve are back to the simple, no-branch shape of
	 * the very first batch (sec 10.31) -- raw or bool-converted direct
	 * field writes, no delegation/context/selector complications. Found
	 * by re-scanning the FULL method-size list rather than just the
	 * first pass's short excerpt (see MASTER_REFERENCE.md sec 10.34).
	 *
	 * Eight raw stores (no conversion):
	 *   UpdateSeqParamMidiOutMode()   .text+0xfd0   byte  @ +0x6dd
	 *   UpdateAfterTouchCurve()       .text+0xd30   dword @ +0x29c9f9c
	 *   UpdateBankMap()               .text+0xf80   dword @ +0x6e4
	 *   UpdateVelocityCurve()         .text+0xd20   dword @ +0x29c9f98
	 *   UpdateSeqTrackMidiOutMode()   .text+0xfc0   byte  @ +0x6dc
	 *   UpdateVectorMIDIOut()         .text+0xfb0   byte  @ +0x6c2
	 *   UpdateNoteReceive()           .text+0xd40   dword @ +0x6b4
	 *   UpdateDamperPolarity()        .text+0xf70   dword @ +0x29c9fbc
	 *
	 * Four bool-converted stores (via `setne`, a real conversion, same
	 * shape as UpdateRearPanelControllerReset/UpdateTmbrTrkOscTransposeType,
	 * sec 10.31) -- confirmed to be four CONSECUTIVE flag bytes:
	 *   UpdateCombiChangeEnable()       .text+0xef0   byte(bool) @ +0x6d7
	 *   UpdateAftertouchChangeEnable()  .text+0xf00   byte(bool) @ +0x6d8
	 *   UpdateControlChangeEnable()     .text+0xf10   byte(bool) @ +0x6d9
	 *   UpdateSysExEnable()             .text+0xf20   byte(bool) @ +0x6da
	 */
	void UpdateSeqParamMidiOutMode(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateAfterTouchCurve(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateBankMap(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateVelocityCurve(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateSeqTrackMidiOutMode(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateVectorMIDIOut(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateNoteReceive(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateDamperPolarity(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateCombiChangeEnable(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateAftertouchChangeEnable(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateControlChangeEnable(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateSysExEnable(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * UpdateHeadroom() (.text+0xf30, 53 bytes) confirmed: reads
	 * `param.value` AS A FLOAT (via `fld`, not as the `int` every other
	 * handler so far has used -- `STGConvertedParam.value`'s 4 bytes are
	 * evidently reinterpreted per-parameter-type, int or float, and this
	 * is the first handler in this project confirmed to use the float
	 * interpretation) and broadcasts it into `gAllPlusHeadroom[0..3]`,
	 * and its negation into `gAllMinusHeadroom[0..3]` -- the SAME two
	 * module globals `CSTGAudioBusManager`'s constructor resets to
	 * `{1,1,1,1}`/`{-1,-1,-1,-1}` (sec 10.24, see managers.cpp). No
	 * calls, no unresolved branches -- a clean, self-contained handler
	 * despite not being in the `UpdateXXX` family's usual raw-int shape.
	 */
	void UpdateHeadroom(CSTGMessageContext &ctx, STGConvertedParam &param);

	/* .text+0x4690 -- reconstructed, see global.cpp (sec 10.55). */
	void RunVoiceModelFeedback();

	/*
	 * SetCurrentModeTempo(float) (sec 10.117, .text+0x4b20, 90 bytes)
	 * confirmed: computes `clamp(log2(ratio), -16.0f, 16.0f)` and stores
	 * it at `+0x29c9fa4`, where `ratio` is:
	 *   - `1.0f/120.0f` (a confirmed real, separately-stored SINGLE-
	 *     precision `.rodata` constant, bit-exact with the C literal)
	 *     if `tempo < 1.0f` -- a confirmed real quirk: `tempo/120` is
	 *     never computed at all in this case; the function substitutes
	 *     the fixed ratio as if `tempo` were exactly `1.0`, avoiding
	 *     ever taking `log2` of a smaller-than-`1/120` value.
	 *   - otherwise, `tempo * (1.0/120.0)` computed with a DOUBLE-
	 *     precision constant (a separate confirmed real `.rodata.cst8`
	 *     value) but rounded down to `float` before the `log2` (matching
	 *     the real FPU sequence's own store-then-reload) -- modeled via
	 *     a small inline-asm helper (`MulRoundToFloat`) for exact
	 *     rounding fidelity, verified bit-for-bit against a
	 *     `libm`-based double-precision reference for representative
	 *     tempo values before shipping.
	 * `log2` itself has no libm dependency available in a kernel build,
	 * so it's computed via a direct x87 `fyl2x` inline-asm helper
	 * (`FYL2X`), matching the real disassembly's own instruction
	 * exactly. The previously-unresolved "which FPCMOV clamps which
	 * direction" ambiguity (sec ~10.9x) is fully resolved: the lower
	 * clamp uses an `fstp st(1)`-based conditional replace (confirmed,
	 * via domain analysis, to be UNREACHABLE in practice -- both of
	 * `ratio`'s own possible sources keep `log2(ratio) >= ~-6.9`, well
	 * above `-16.0` -- but reproduced faithfully as real defensive
	 * code, not removed), and the upper clamp uses a genuine `fcmovnbe`.
	 */
	void SetCurrentModeTempo(float tempo);

	/* .text+0x8340, 267 bytes -- reconstructed, see global.cpp (sec
	 * 10.55). Constructs/initializes the 32-slot voice-data array, the
	 * embedded CSTGControllerRTData sub-object, and several sibling
	 * managers (CSTGWaveSeqData/CSTGProgramModeProgramSlot/
	 * CSTGProgramModeDrumTrackSlot/CSTGPerformanceVarsManager/
	 * USTGAliasBankTypes/CSetListBank/InitializePerformances), none of
	 * which are reconstructed themselves in this pass -- see this
	 * header's own declarations above. */
	void Initialize();

	/* Confirmed real (via relocation from Initialize), own body not
	 * reconstructed in this pass. */
	void InitializePerformances();

	/*
	 * Two confirmed real, genuinely EMPTY handlers (sec 10.67) -- the
	 * smallest possible `UpdateXXX` shape, a single `ret` and nothing
	 * else (1 byte, WEAK linkage, each in its own COMDAT-style
	 * section -- likely once-inlined-then-outlined trivial overrides).
	 * Not stubs of convenience; independently confirmed via direct
	 * disassembly that the real binary's own bodies are empty too. */
	void UpdateMIDIClockSource(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateShowMSWSDKitGraphics(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * Nine confirmed real, IDENTICALLY-shaped thin thunks (sec 10.67,
	 * all 21 bytes): adjust `this` by `+0x608` (the CSTGAudioInput
	 * sub-object global_ctor.cpp already confirmed embedding there,
	 * sec 10.56) and tail-call the correspondingly-named
	 * `CSTGAudioInput::UpdateXXX` method with the same (ctx, param)
	 * arguments -- confirmed via a full disassembly of each of the 9,
	 * not a spot check. `CSTGAudioInput`'s own 9 methods are
	 * confirmed-real, deliberately deferred externs (that class's own
	 * body is a separate, not-yet-reconstructed task).
	 */
	void UpdateAudioInputBusSelect(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateAudioInputFXControlBus(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateAudioInputHDRBus(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateAudioInputLevel(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateAudioInputMute(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateAudioInputPan(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateAudioInputSend1Level(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateAudioInputSend2Level(CSTGMessageContext &ctx, STGConvertedParam &param);
	void UpdateAudioInputSolo(CSTGMessageContext &ctx, STGConvertedParam &param);

	/*
	 * Batch (sec 10.90): 13 more of the ~37 CSTGGlobal methods left
	 * after the Bar 2 pivot (sec 10.13's original ~195-method survey),
	 * picked directly off the real binary's own method list, smallest
	 * first (matching this project's established `UpdateXXX`-batch
	 * convention). Each individually ground-truthed via objdump +
	 * relocation lookups against OA_real.ko, not guessed from name
	 * alone.
	 */

	/* ~CSTGGlobal() / D0Ev (.text+0x3800, 15 bytes) confirmed: the
	 * Itanium ABI "deleting destructor" -- calls the complete-object
	 * destructor (D1Ev, .text+0x3180, 1664 bytes -- a confirmed real
	 * mirror of the 3124-byte constructor, sec 10.56, deliberately
	 * deferred as its own separate future task) and, notably, does
	 * NOT call `operator delete` afterward -- confirmed via the full
	 * 15-byte instruction sequence, not an omission on this project's
	 * part. Preserved as a real quirk (this class is a singleton,
	 * likely never actually `delete`d through this path). */
	~CSTGGlobal();

	/* SetCurrentEditInContextTimbreSolo(unsigned int, bool) (.text+0x9710,
	 * 42 bytes) confirmed: sets or clears bit `index` of a 16-bit field
	 * at `+0x29cc4e4` depending on the bool. The real disassembly uses
	 * a `rol`-based bit-clear idiom; reproduced here with the
	 * algebraically equivalent `&= ~bit` (no behavioral difference,
	 * unlike cases elsewhere in this file where a quirk changes real
	 * observable behavior). */
	void SetCurrentEditInContextTimbreSolo(unsigned int index, bool solo);

	/* CompleteDeferredExtModeChange() (.text+0x8cc0, 48 bytes) confirmed:
	 * if bit 0 of `+0x29cc0c9` (the same "ext mode change deferred" flag
	 * `UpdateExtSetSelect`/`CheckDeferExtModeChange` share) is set, calls
	 * `CSTGControllerRTData::sInstance->OnExtModeSetChange()` (already
	 * declared/deferred, sec 10.71) and clears the bit. */
	void CompleteDeferredExtModeChange();

	/* ShouldDeferExtModeChange() const (.text+0x8c30, 50 bytes)
	 * confirmed: `true` if `+0x6ae` ("initialized") is clear OR bit 0
	 * of `+0x29cc0c9` is already set; otherwise resolves the active
	 * `CSTGPerformanceVarsManager` (shared helper, sec 10.71) and
	 * returns whether ITS `+0x23d1 != 2`. */
	bool ShouldDeferExtModeChange() const;

	/* CheckDeferExtModeChange() (.text+0x8c70, 74 bytes) confirmed: the
	 * real "check-and-set" companion to the two methods above -- if
	 * NOT initialized, or the deferred-bit is already set, or the
	 * active `CSTGPerformanceVarsManager`'s `+0x23d1 != 2`: (re-)sets
	 * bit 0 of `+0x29cc0c9` and returns `false`. Otherwise (initialized,
	 * bit clear, mgr state == 2): returns `true` WITHOUT setting the
	 * bit -- confirmed via the real jump target skipping the bit-set
	 * path entirely in that one branch (the same "jump into the middle"
	 * quirk style already confirmed for `UpdateExtSetSelect`, sec
	 * 10.71). */
	bool CheckDeferExtModeChange();

	/* RemoveExtCCFunctionAssignment(unsigned int tag) (.text+0x8cf0, 57
	 * bytes) confirmed: a standalone, externally-parameterized version
	 * of the SAME 120-slot claim-table "un-claim by tag" scan already
	 * confirmed inline inside `UpdateCCAssign()`'s own step 1 (sec
	 * 10.74) -- clears the claimed-flag of every slot whose free-flag
	 * is 0 (occupied) AND whose tag dword matches. Kept as its own
	 * separate implementation (not refactored to share code with
	 * `UpdateCCAssign`) since the real binary has these as two
	 * genuinely separate `.text` addresses, matching this project's
	 * "preserve real structure" precedent (e.g. `SendFXDisableCCToMidiOut`
	 * below vs. `UpdateFXDisable`'s own inlined copy of the same MIDI
	 * send). Real (not yet confirmed) caller not identified in this
	 * pass. */
	void RemoveExtCCFunctionAssignment(unsigned int tag);

	/* SendFXDisableCCToMidiOut(unsigned char ccNumber, bool enabled)
	 * (.text+0x8bf0, 61 bytes) confirmed: a standalone, externally-
	 * parameterized version of the SAME 3-byte MIDI CC send already
	 * confirmed inline inside `UpdateFXDisable()` (sec 10.73) --
	 * `{0xB0 | this->fieldAt(0x6b8), ccNumber, enabled ? 0x00 : 0x7f}`
	 * via `CSTGMidiPortManager::sInstance->WriteSTGMidiOutQueue()`. Kept
	 * as its own separate implementation for the same "preserve real
	 * structure" reason as `RemoveExtCCFunctionAssignment` above. Real
	 * caller not identified in this pass. */
	void SendFXDisableCCToMidiOut(unsigned char ccNumber, bool enabled);

	/* BeginPerformanceChange(eGlobalMode, unsigned int, unsigned int,
	 * eSTGPerformanceChangeSource) / BeginSetListSlotChange(unsigned int,
	 * unsigned int, eSTGPerformanceChangeSource) (.text+0x66c0/0x6670,
	 * 64/65 bytes) confirmed: both build a `CSTGPerfChangeRequest` (see
	 * its own declaration above for the confirmed field layout, ground-
	 * truthed via these exact two functions) on the stack and forward it
	 * to `SubmitPerfChangeRequest`. */
	void BeginPerformanceChange(int mode, unsigned int value1, unsigned int value2,
				     unsigned int source);
	void BeginSetListSlotChange(unsigned int value1, unsigned int value2,
				     unsigned int source);

	/*
	 * SubmitPerfChangeRequest(CSTGPerfChangeRequest&) (sec 10.116,
	 * .text+0x6320, 834 bytes) confirmed: the real top-level entry
	 * point for this whole cluster -- every `Begin*`/`Increment*`/
	 * `Decrement*`/`SetMode` producer ultimately calls this.
	 *
	 * If `request.source == 2`, stamps `request.field14`/`field18`
	 * with the current 64-bit microsecond counter
	 * (`IncrementMicrosecondCount`'s own confirmed counter at
	 * `+0x29c9fa8`/`+0x29c9fac`, sec ~10.5x), then compares the request
	 * against a "compare slot" -- the confirmed `+0x297514c` slot if
	 * `+0x2975185` (the pending flag) is clear, else `+0x2975168` --
	 * and, if `tag`/`mode`(when `tag==0`)/`value1`/`value2` all match
	 * AND the elapsed microseconds since that slot's own stamped time
	 * is `<= 0x95` (149us), SILENTLY DROPS the request (a real hardware
	 * debounce for near-simultaneous duplicate requests) and returns.
	 *
	 * Otherwise: if `+0x2975184` (busy flag) is set, OR the real MIDI
	 * output queue (`CSTGMidiPortManager::sInstance+0x208`, reinterpreted
	 * as `CSTGMidiQueue` for this one call -- see its own declaration
	 * above) reports `<= 0x4f` (79) writable bytes, queues the request
	 * into the confirmed `+0x2975168` pending slot and sets
	 * `+0x2975185` (the SAME "busy, queue it" behavior
	 * `StartPendingPerformanceChange`, sec 10.105, later drains).
	 * Otherwise, forwards directly to `ProcessPerfChangeRequest()` (sec
	 * 10.104) -- which itself performs the exact same "copy `request`
	 * into `+0x297514c`" as its own first step, so this function
	 * doesn't need to duplicate that copy before calling it (the same
	 * "just call the function that already does this" pattern as
	 * `StartPendingPerformanceChange`'s own relationship to
	 * `ProcessPerfChangeRequest`).
	 */
	void SubmitPerfChangeRequest(CSTGPerfChangeRequest &request);

	/* GetIsSetListActiveAndSeqPerfType() (.text+0x9400, 67 bytes)
	 * confirmed: `false` if `+0x6a4` is clear; otherwise reads a 2D
	 * table index from `+0x6a5` (clamped to 0 if `>= 0x80`) and `+0x6a6`,
	 * and returns whether `this->fieldAt(0x2933750 + idx*0x834 +
	 * idx2*0x10) == 2`. */
	bool GetIsSetListActiveAndSeqPerfType();

	/* SetUseGlobalAudioInputSettings(bool) (.text+0x9360, 68 bytes)
	 * confirmed: always stores the bool to `+0x680`; if initialized
	 * (`+0x6ae`) AND the active `CSTGPerformanceVarsManager`'s
	 * `+0x23d1 == 2`, resolves a `CSTGAudioInput*` from that manager's
	 * own `+0x23d4` field (a confirmed real 32-bit pointer slot, `+0xae7`
	 * further offset) and calls its `OnUseGlobalSettingsChanged()`. */
	void SetUseGlobalAudioInputSettings(bool useGlobal);

	/* ShouldAllowMidiPerformanceChange() const (.text+0x4c20, 75 bytes)
	 * confirmed: `false` if `CSTGMidiDispatcher::sInstance->fieldAt(0xa2)
	 * == 0`; otherwise selects one of two confirmed per-`this` slots
	 * (`+0x297514c` or `+0x2975168`, chosen by `this->fieldAt(0x2975185)
	 * == 0`) and returns `true` unless that slot's own first byte is 0
	 * AND its `+0x4` dword is 0 AND its `+0xc` dword == 0xfffe. */
	bool ShouldAllowMidiPerformanceChange() const;

	/* SendUnsolGlobalMessageToUI(int, int, int, eSTGMidiSource) (.text+
	 * 0x8ad0, 75 bytes) confirmed: builds a real 24-byte tagged message
	 * (`{u16 tag=0x18, u16 lowSource, u32 1, u32 0, u32 p2, u32 p1, u32
	 * p3}`) and forwards it to `PushUnsolicitedMessage` (already
	 * confirmed real/declared, sec 10.59). */
	void SendUnsolGlobalMessageToUI(int p1, int p2, int p3, unsigned int source);

	/* SetNKS4TestModeFlag(bool) (.text+0x4710, 90 bytes) confirmed: if
	 * the new value differs from `+0x6ac`, ALWAYS calls the confirmed-
	 * real, deliberately deferred extern `COmapNKS4Driver_SetTestMode(bool)`
	 * first (regardless of direction), then stores the new value; if the
	 * new value is `true`, additionally calls
	 * `CSTGMidiPortManager::sInstance->NotifyNKS4TestMode()` (declared
	 * above, own body deferred). */
	void SetNKS4TestModeFlag(bool testMode);

	/*
	 * Batch (sec 10.92): 10 more CSTGGlobal methods, continuing the
	 * sec 10.90 batch's own "performance change" cluster now that
	 * `CSTGPerfChangeRequest`'s shape is ground-truthed.
	 */

	/* ValidateParamChange(CSTGMessageContext&, unsigned long, CValue
	 * const&) (.text+0x2c80, 86 bytes) confirmed: for paramId 0x26/0x27,
	 * bound-checks `ctx.index` against a per-paramId literal (0xbf/0x7f);
	 * if in range, clears `ctx.clampFlag` and forwards to
	 * `CSTGParamsOwner::ValidateParamChange` (declared above); if out of
	 * range, sets `ctx.responseCode = 2` and returns WITHOUT forwarding.
	 * Any other paramId forwards unconditionally. */
	void ValidateParamChange(CSTGMessageContext &ctx, unsigned long paramId, const CValue &value);

	/* RepeatLastPerformanceChange() (.text+0x6700, 108 bytes) confirmed:
	 * if `+0x2975184` ("already repeating"?) is set, no-op. Otherwise
	 * selects one of the SAME two confirmed per-`this` slots
	 * `ShouldAllowMidiPerformanceChange` uses (`+0x297514c`/`+0x2975168`,
	 * chosen by `+0x2975185`), copies it VERBATIM as a
	 * `CSTGPerfChangeRequest` (confirmed: both are ground-truthed to be
	 * the exact same 0x1c-byte shape), forces `field14 = 3`, and
	 * resubmits it -- i.e. these slots are real "last submitted
	 * performance change" save records. */
	void RepeatLastPerformanceChange();

	/* ResetAllControllers() (.text+0x9150, 110 bytes) confirmed: builds
	 * one `CSTGControllerValue` (see its own declaration above,
	 * including the confirmed real uninitialized-read quirk) and calls
	 * the not-yet-reconstructed `HandleController` (declared below,
	 * deliberately deferred) three times with CC numbers 0x5c/0x5e/0x5f
	 * -- the SAME three CC numbers `UpdateIFXDisable`/`UpdateMFXDisable`/
	 * `UpdateTFXDisable` already use (sec 10.73), confirming this
	 * resets exactly those three effect-bus controllers. */
	void ResetAllControllers();

	/* DoesPerfChangeRequestMatchType(CSTGPerfChangeRequest const&,
	 * eSTGPerformanceType) const (.text+0x5330, 134 bytes) confirmed:
	 * for `tag==0`, looks up the SAME confirmed real per-slot "type"
	 * table at `+0x2933750` that `GetIsSetListActiveAndSeqPerfType`
	 * (sec 10.90) already reads (`[value1_clamped*0x834 +
	 * value2*0x10]`), comparing it to `type`; for `tag==1`, compares
	 * `type` against either a fixed literal `1` or `this->fieldAt(0x64 +
	 * (mode-1)*4)` (a dword), depending on `mode`. */
	bool DoesPerfChangeRequestMatchType(const CSTGPerfChangeRequest &req, unsigned int type) const;

	/* GetPerformanceIdFromPerfChangeRequest(CSTGPerfChangeRequest const&,
	 * CPerformanceId*) const (.text+0x53c0, 138 bytes) confirmed: for
	 * `tag==0`, reads a 3-byte `CPerformanceId` directly out of the SAME
	 * per-slot record `DoesPerfChangeRequestMatchType` indexes into
	 * (`+0x2933740 + idx`, 0x10 bytes before that function's own "type"
	 * byte -- confirming the type byte is itself `record+0x10`, part of
	 * a larger per-slot record), always returning `true`. For `tag==1`:
	 * `false` if `value2 == 0xfffe`; otherwise builds a `CPerformanceId`
	 * from `value1`/`value2` plus a mode-dependent `byte0` (either a
	 * literal `1` or `this->fieldAt(0x64 + (mode-1)*4)` read as a BYTE
	 * this time, not a dword -- the SAME confirmed field
	 * `DoesPerfChangeRequestMatchType` reads as a dword, a genuine
	 * partial-width reuse of the same table). */
	bool GetPerformanceIdFromPerfChangeRequest(const CSTGPerfChangeRequest &req, CPerformanceId *out) const;

	/* NotifyKarmaPerformanceChange() (.text+0x5720, 150 bytes) confirmed:
	 * builds a real 5-byte MIDI-shaped message (`{channel|0xc0, valA,
	 * valB, 0x15, 0xfe}`, valA/valB mode-dependent) and forwards it to
	 * `CSTGMidiPortManager::sInstance`'s own embedded `CSTGMidiQueueWriter`
	 * at `+0x208` (the SAME confirmed embedding `midi_dispatcher.cpp`
	 * already uses) via `Write(msg, 5, false)`. */
	void NotifyKarmaPerformanceChange();

	/* SendProgramChangeToMidiOut(unsigned char, unsigned char, unsigned
	 * char) (.text+0x8a30, 150 bytes) confirmed: if `+0x6d6`
	 * (`UpdateBankChangeEnable`'s own confirmed flag, sec 10.75) is set,
	 * sends two real 3-byte MIDI Control Change messages (Bank Select
	 * MSB=CC0, LSB=CC32 -- standard MIDI convention, confirmed via the
	 * literal `0x20` CC number) carrying `p1`/`p2`; ALWAYS (regardless of
	 * that flag) sends a real 2-byte MIDI Program Change message
	 * (`{channel|0xc0, p3}`) -- all three via the already-confirmed
	 * `CSTGMidiPortManager::sInstance->WriteSTGMidiOutQueue()`. */
	void SendProgramChangeToMidiOut(unsigned char p1, unsigned char p2, unsigned char p3);

	/* EmergencyFreeDyingSlotVoiceData() (.text+0x49b0, 154 bytes)
	 * confirmed: scans the intrusive list at `+0x29c9904` (a DIFFERENT,
	 * adjacent field from `RunVoiceModelFeedback`'s own confirmed
	 * `+0x29c9900` head, sec 10.55) for the FIRST node whose
	 * `(fieldAt(0x34)->byteAt(0x43) >> 1) & 1` matches an outer "group"
	 * counter (0, then -- only if group 0's full scan finds nothing --
	 * 1 again over the SAME list) AND whose own `byteAt(0x40)` is set;
	 * on a match, calls `EmergencyFreeAllVoices()` unconditionally and
	 * `FreeSlotVoiceData(true)` UNLESS a confirmed real combination of
	 * two other fields (`fieldAt(0x4c)+fieldAt(0x58)` both as u16 sums
	 * nonzero, AND `byteAt(0x41)` set) says otherwise -- then returns
	 * immediately (processes at most ONE node per call, not a bulk
	 * sweep). */
	void EmergencyFreeDyingSlotVoiceData();

	/* SetTune(float) (.text+0x4b80, 157 bytes) confirmed: always stores
	 * the raw float bits to `+0x6bc`; if initialized (`+0x6ae`), reuses
	 * the EXACT SAME list-walk + "also walk the active slot's special
	 * list" logic as `UpdateMasterTune` (sec 10.75/global.cpp's own
	 * `UpdateGlobalTuneOnList` helper) -- confirmed via identical
	 * relocations (`CSTGGlobal::sInstance`, `CSTGSlotVoiceData::
	 * UpdateGlobalTune`) at every corresponding instruction. */
	void SetTune(float tune);

	/* SetMode(eGlobalMode, eSTGPerformanceChangeSource) (.text+0x8450,
	 * 157 bytes) confirmed: builds a `CSTGPerfChangeRequest` with
	 * `tag=0` and a mode-dependent `(value1, value2)` pair read from
	 * three confirmed real field pairs (`+0x688/+0x694` default,
	 * `+0x690/+0x69c` mode==1, a literal `0`/`+0x6a0` mode==2) and
	 * forwards it to `SubmitPerfChangeRequest` -- the same producer
	 * shape as `BeginPerformanceChange`/`BeginSetListSlotChange` (sec
	 * 10.90), just with the value pair resolved internally instead of
	 * taken as parameters. */
	void SetMode(int mode, unsigned int source);

	/*
	 * HandleController(unsigned int, CSTGControllerValue const&) (sec
	 * 10.118, .text+0x8d70, 983 bytes) confirmed: dispatches on
	 * `ccNumber` (real MIDI CC numbers `0x5c`/`0x5e`/`0x5f`/`0x7a` = 92/
	 * 94/95/122, all "channel mode"-family CCs; everything else no-ops)
	 * using two confirmed-real derived values:
	 *   - `cl` = `(value.fieldB & 2) != 0` OR (when that's false)
	 *     `value.fieldA == 6` (both computed the same way regardless of
	 *     `ccNumber`).
	 *   - `doSecondary` = `value.fieldA` is NOT `3`, `4`, or `5`.
	 *
	 * `ccNumber` 0x5c/0x5e/0x5f each toggle a distinct PAIR of bits in
	 * `+0x6d4` (the SAME packed-flags byte this cluster's other
	 * `UpdateXXX` handlers already use) -- bit pair (0,3) for 0x5c,
	 * (1,4) for 0x5e, (2,5) for 0x5f:
	 *   - if `!cl`: `primary = (value.field8 == 0)`; sets the PRIMARY
	 *     bit to `primary`, and ALSO the SECONDARY bit to `primary` if
	 *     `doSecondary`.
	 *   - if `cl`: reads the SECONDARY bit's CURRENT value and copies
	 *     it into the PRIMARY bit (ignoring `value.field8` entirely).
	 *   Either way, pushes a confirmed real 24-byte
	 *   `PushUnsolicitedMessage` (`{0x18, (short)value.fieldA, 1, 0, 0,
	 *   TYPE, primaryOrSecondaryValue}`, `TYPE` = 4/5/6 respectively).
	 *
	 * `ccNumber == 0x7a` (122) is a DIFFERENT shape entirely: no-op if
	 * `cl`; otherwise computes `above3f = (signed)value.field8 > 0x3f`
	 * (a confirmed SIGNED comparison, unlike the `==0` checks above),
	 * and if `+0x6ae` is set: calls `CSTGVoiceAllocator::sInstance->
	 * StealAllVoices()` ONLY if `CSTGMessageProcessor::sInstance->
	 * fieldAt(0x48) == 0` (NOT an either/or dispatch -- execution always
	 * falls through to the same next step regardless), then
	 * UNCONDITIONALLY calls `CSTGMidiDispatcher::sInstance->
	 * ResetAllControllers(this->fieldAt(0x6b9), false)`, stores
	 * `above3f` at `+0x6af`, sends a real 5-byte MIDI message
	 * (`{this->fieldAt(0x6b8)|0xb0, 0x79, 6-above3f, 5, 0xff}`) via
	 * `CSTGMidiPortManager::sInstance+0x208`'s embedded
	 * `CSTGMidiQueueWriter`, and finally (also unconditional on this
	 * sub-path, unrelated to the message-processor gate above) calls
	 * `CSTGControllerRTData::sInstance->ResetAllJumpCatch()`. Either way
	 * (whether `+0x6ae` was set or not), finally pushes a real 24-byte
	 * message (SAME shape as
	 * above, `TYPE=0x15`, last field `above3f`).
	 */
	void HandleController(unsigned int ccNumber, const CSTGControllerValue &value);

	/*
	 * RunVoiceModelStaticFront(unsigned int)/RunVoiceModelStaticBack(
	 * unsigned int) (sec 10.93, .text+0x4530/0x45e0, 164 bytes each)
	 * confirmed: the SAME confirmed vtable-slot-0x68 dispatch through
	 * `+0xb6b`/`+0xb6f` (gated by `+0xb73` bits 0/1) that
	 * `RunVoiceModelFeedback` already established (sec 10.55) --
	 * except: (1) only visits payloads whose `+0x28dc`/`+0x28dd` (Front)
	 * or `+0x28de`/`+0x28df` (Back) byte matches the `param` argument;
	 * (2) tests a DIFFERENT bit of the dispatched object's own `+0xe1`
	 * byte -- Front tests bit 6 (`& 0x40`), Back tests the SIGN bit
	 * (`(signed char)+0xe1 < 0`), a confirmed real distinction between
	 * the two, not a copy-paste error; (3) on a qualifying payload,
	 * calls the correspondingly-named `CSTGSlotVoiceData::
	 * RunVoiceModelStaticFront`/`Back(param)` (declared above,
	 * deliberately deferred) instead of `RunVoiceModelFeedback()`. Both
	 * confirmed to read the list head from `CSTGGlobal::sInstance`, not
	 * `this` (the same "always re-fetch sInstance" quirk already seen
	 * elsewhere in this file).
	 */
	void RunVoiceModelStaticFront(unsigned int param);
	void RunVoiceModelStaticBack(unsigned int param);

	/*
	 * Batch (sec 10.94): 3 more CSTGGlobal methods.
	 */

	/* HandleMidiPerformanceChange(unsigned char) (.text+0x84f0, 202
	 * bytes) confirmed: `false`-equivalent no-op if
	 * `CSTGMidiDispatcher::sInstance->byteAt(0xa2) == 0` (the SAME gate
	 * `ShouldAllowMidiPerformanceChange` already established, sec
	 * 10.90). Otherwise selects the SAME confirmed dual-slot
	 * (`+0x297514c`/`+0x2975168`) as several other methods in this
	 * cluster; if `slot[0] != 0`, submits a `CSTGPerfChangeRequest`
	 * with `tag=1, mode=0, value1=slot+8, value2=param, source=2`.
	 * Otherwise (`slot[0]==0`): no-op if `slot+4==0 AND slot+0xc==
	 * 0xfffe`; else, no-op if `slot+4` (as `mode`) is `> 1`; else
	 * submits `tag=0, mode=slot+4, value1=slot+8, value2=param,
	 * source=2`. */
	void HandleMidiPerformanceChange(unsigned char param);

	/* StealDyingSlotVoiceDatasForCost(unsigned long) (.text+0x4a50, 203
	 * bytes) confirmed: `0` immediately if `targetCost==0`. Otherwise,
	 * for each of 2 groups (SAME `+0x29c9904`-list group-bit filter as
	 * `EmergencyFreeDyingSlotVoiceData`, sec 10.92), walks the list
	 * (stopping early once the running cost total reaches `targetCost`)
	 * looking for payloads that are "dying" (`+0x40`), NOT
	 * `fieldAt(0x28c4)==1` (a new confirmed real "protected/locked"-
	 * shaped flag), whose sub-object's `+0xd` byte is `1` or `2`, and
	 * whose OWN `+0x42` is clear -- on a match, calls
	 * `GetTotalStaticCosts()` (declared above, deferred); if the
	 * returned sum is nonzero, adds it to the running total and calls
	 * `Steal()` (declared above, deferred). Returns the final running
	 * total. */
	unsigned long StealDyingSlotVoiceDatasForCost(unsigned long targetCost);

	/* FreeSlotVoiceData(CSTGSlotVoiceData*) (.text+0x48d0, 216 bytes)
	 * confirmed: real intrusive doubly-linked-list unlink (node's own
	 * `+0x24`/`+0x28` next/prev pair) from whichever of TWO possible
	 * list heads (`+0x29c9900`, `RunVoiceModelFeedback`'s own head, sec
	 * 10.55; `+0x29c9904`, `EmergencyFreeDyingSlotVoiceData`'s own head,
	 * sec 10.92) the node happens to be the current head of -- confirmed
	 * real quirk: BOTH head checks/updates can fire for the same call
	 * (a node CAN apparently be the head of both at once), not an
	 * either/or branch. After unlinking (and clearing node's own
	 * `+0x24`/`+0x28`/`+0x30`), appends the node to a THIRD, SEPARATE
	 * intrusive list (a real "free list": head `+0x29c98f4`, tail
	 * `+0x29c98f8`, via the node's own DIFFERENT `+0x4`/`+0x8` link
	 * pair), storing the free-list head's own address into the node's
	 * `+0x10` field (the same "owner pointer" convention already
	 * confirmed for `CSTGWaveSeqManager`/`CSTGHeapManager`, sec
	 * 10.59/10.62), and adjusts two SEPARATE confirmed real counters
	 * (`+0x29c9908` decremented, `+0x29c98fc` incremented). */
	void FreeSlotVoiceData(CSTGSlotVoiceData *node);

	/* PreprocessPerformanceChange() (sec 10.95, .text+0x4d90, 263
	 * bytes) confirmed: no-op if `CSTGMessageProcessor::sInstance->
	 * byteAt(0x48)` is set (the SAME field `UpdateConvertPosition`
	 * already reads, sec 10.70). Unless a confirmed real global (`sXCmd`)
	 * is non-null AND its own `+0x5` dword equals a literal magic
	 * constant (`0x22fb39cc`), resets the confirmed real
	 * `allPlusOne`/`allMinusOne` float arrays to `{0.7f}`/`{-0.2f}`
	 * (DIFFERENT literals from `UpdateHeadroom`'s own runtime-value
	 * broadcast) and `kAudXBZD = 0x1f`. Always: sets
	 * `+0x2975184` (the SAME "repeating" flag `RepeatLastPerformanceChange`
	 * reads, sec 10.92), calls `CSTGSmoother::sInstance->
	 * FinalizeAllSmoothers()` (new deferred extern), calls
	 * `CSTGPerformanceVars::SetIsDying()` on the resolved active manager
	 * (reusing `ResolveActivePerformanceVarsManager`), sets
	 * `CSTGMidiDispatcher::sInstance`'s own `+0x0` byte and
	 * `CSTGMessageProcessor::sInstance`'s own `+0x54` byte, and sends a
	 * fixed literal 5-byte MIDI-shaped message
	 * (`{channel|0xb0,0x79,0x03,0x05,0xfe}`) via the SAME
	 * `CSTGMidiPortManager::sInstance+0x208` embedded queue writer that
	 * `NotifyKarmaPerformanceChange` uses (sec 10.92). */
	void PreprocessPerformanceChange();

	/* IsSetListSlotChangeOnly(CSTGPerfChangeRequest const&) (sec 10.96,
	 * .text+0x4c70, 286 bytes) confirmed: `false` unless the active
	 * `CSTGPerformanceVarsManager`'s `+0x23d1==2` AND `+0x6a4!=0` (the
	 * SAME gate `GetIsSetListActiveAndSeqPerfType` uses, sec 10.90) AND
	 * `req.tag!=0` AND `req.value1` (clamped >=0x80 to 0) equals
	 * `+0x6a5` (the SAME "idx" field `GetIsSetListActiveAndSeqPerfType`
	 * reads). Then indexes the SAME confirmed real per-slot record at
	 * `+0x2933740 + idx_clamped*0x834 + req.value2*0x10` that
	 * `DoesPerfChangeRequestMatchType`/`GetPerformanceIdFromPerfChangeRequest`
	 * (sec 10.92) already use, and requires the record's own `+0x10`
	 * "type" byte to match a mode-dependent literal (`1` default, `2`
	 * for mode==2, `0` for mode==1 -- a beautiful independent
	 * cross-confirmation of `SetMode`'s own mode encoding, sec 10.92)
	 * AND the record's `+0x11`/`+0x12` bytes to equal the SAME
	 * mode-dependent field pairs `SetMode` itself reads FROM
	 * (`+0x688`/`+0x694` default, `+0x690`/`+0x69c` mode==1, only
	 * `+0x6a0` for mode==2, matching `SetMode`'s own single-field
	 * mode==2 case exactly). */
	bool IsSetListSlotChangeOnly(const CSTGPerfChangeRequest &req);

	/* ProcessSetListSlotOnlyChange() (sec 10.97, .text+0x5d20, 280
	 * bytes) confirmed: resolves the SAME per-slot record
	 * (`+0x2933750 + idx*0x834 + idx2*0x10`, sourced this time from two
	 * "pending request" fields `+0x2975154`/`+0x2975158`) already
	 * established by sec 10.90/10.92/10.96. **Confirmed real (sec
	 * 10.98): `+0x2975154`/`+0x2975158`/`+0x297515c` are NOT independent
	 * fields at all -- they ARE `CSTGPerfChangeRequest`'s own
	 * `value1`/`value2`/`source` members, since `+0x297514c` (the
	 * dual-slot "A" request already confirmed elsewhere, sec 10.90) plus
	 * `0x8`/`0xc`/`0x10` lands exactly on those three addresses.** Also
	 * writes `idx2` back into `+0x6a6` (the SAME field
	 * `GetIsSetListActiveAndSeqPerfType` reads, sec 10.90 -- confirming
	 * this function is a real PRODUCER for that state, not just another
	 * reader). If `this->fieldAt(0x684)==2` (mode 2), first resolves a
	 * `CSTGSequence` object (`+0x27cd024 + idx684a0*0x1cad`, the SAME
	 * stride/base `UpdateVJSXAssignment`'s own mode==2 case uses, sec
	 * 10.77) and dispatches its own vtable slot 0x1e (identity not
	 * confirmed) with a byte read from the per-slot record's own `+0x3`
	 * field (a newly confirmed offset within that same record).
	 * Either way, calls `CSetListSlot::Activate()` (declared above,
	 * deferred) on the per-slot record reinterpreted directly as a
	 * `CSetListSlot*`. Finally, based on a THIRD new confirmed real
	 * field (`+0x297515c`): if it's `1` or `2`, sends a real 24-byte
	 * `PushUnsolicitedMessage` (the SAME confirmed shape as
	 * `SendUnsolGlobalMessageToUI`, sec 10.90); if it's `0` or `1`,
	 * additionally calls `SendPerfChangeToMidiOut` (declared below,
	 * deferred) on the request stored at the confirmed `+0x297514c`
	 * slot. */
	void ProcessSetListSlotOnlyChange();

	/*
	 * SendPerfChangeToMidiOut(CSTGPerfChangeRequest const&) (sec 10.98,
	 * .text+0x57c0, 489 bytes) confirmed:
	 *   `req.tag != 0` (SetListSlotChange): no-op unless `+0x6d5`
	 *   (ProgramChangeEnable, sec 10.75) is set. If so, and `+0x6d6`
	 *   (BankChangeEnable) is ALSO set, sends a real 6-byte Bank Select
	 *   MSB/LSB pair -- confirmed real quirk: the MSB's own VALUE byte
	 *   is ALWAYS a literal `0`, never derived from `req` (only the LSB
	 *   carries `req.value1`). Always (regardless of BankChangeEnable)
	 *   sends a 2-byte Program Change `{channel|0xc0, req.value2}`.
	 *
	 *   `req.tag == 0`: dispatches on `req.mode`:
	 *   - mode 0: no-op unless `+0x6d5` set; else converts
	 *     `req.value1` via `USTGAliasBankTypes::
	 *     ConvertAliasPgmBankToMidiBank` and sends the SAME Bank
	 *     Select(gated on `+0x6d6`, this time both MSB/LSB VALUE bytes
	 *     ARE the two conversion outputs, unlike the tag!=0 case's
	 *     literal-0 MSB) + Program Change pair.
	 *   - mode 1: no-op unless BOTH `+0x6d5` AND `+0x6d7` (a THIRD
	 *     confirmed real enable flag) are set; else converts
	 *     `req.value1` via `ConvertCombiBankToMidiBank` and sends the
	 *     EXACT SAME message pair as mode 0 (confirmed via the real
	 *     binary literally jumping into mode 0's own shared code after
	 *     a real argument-slot swap that cancels out semantically --
	 *     reproduced here as one shared static helper,
	 *     `SendBankSelectAndProgramChange`, matching the real code's
	 *     own sharing rather than an artificial refactor).
	 *   - mode 2: if `req.value2 > 0x7f`, calls
	 *     `SKSTGGate_ShouldSyncExternalClock()` (declared above) and
	 *     returns (result discarded). Otherwise calls it too; if it
	 *     returns `false`, sends a real 2-byte MIDI System Common "Song
	 *     Select" message (`{0xf3, req.value2}`); either way, calls
	 *     `SKSTGGate_ShouldSyncExternalClock()` a SECOND time before
	 *     returning (confirmed real quirk -- its own result is
	 *     discarded both times it's called in this specific branch).
	 *   - any other mode: no-op.
	 */
	void SendPerfChangeToMidiOut(const CSTGPerfChangeRequest &request);

	/*
	 * HandleMidiBankAndPerformanceChange(unsigned char, unsigned char,
	 * unsigned char) (sec 10.99, .text+0x85c0, 327 bytes) confirmed:
	 * no-op if `CSTGMidiDispatcher::sInstance->+0xa2==0` (the same real
	 * gate used throughout this cluster). A real, confirmed quirk: this
	 * function checks TWO DIFFERENT dual-slot selections in the SAME
	 * call -- an initial early-return check against "slotA" (the usual
	 * `+0x2975185==0 -> +0x297514c` mapping), THEN, regardless of that
	 * check's outcome, builds the actual request from "slotB" -- the
	 * OPPOSITE selection (`+0x2975185==0 -> +0x2975168`) -- not a bug,
	 * two genuinely independent gates.
	 *   - slotB[0]!=0: submits `tag=1, mode=0, value1=(p1!=0 ?
	 *     slotB+8 : p2), value2=p3, source=2`.
	 *   - slotB[0]==0, slotB+4==1: converts `(p1,p2)` via
	 *     `ConvertMidiBankToCombiBank` (declared above) and submits
	 *     `tag=0, mode=1, value1=<converted>, value2=p3, source=2`.
	 *   - slotB[0]==0, slotB+4==0: same shape via
	 *     `ConvertMidiBankToAliasProgramBank`, `mode=0`.
	 *   - slotB[0]==0, slotB+4 anything else: no-op.
	 */
	void HandleMidiBankAndPerformanceChange(unsigned char p1, unsigned char p2, unsigned char p3);

	/*
	 * GetFreeSlotVoiceData() (sec 10.100, .text+0x4770, 345 bytes)
	 * confirmed: the real inverse of `FreeSlotVoiceData` (sec 10.94) --
	 * pops the head of the SAME free list (`+0x29c98f4`/`+0x29c98f8`),
	 * a genuine doubly-linked-list removal (mirroring that function's
	 * own insertion in reverse, including clearing the same 3 fields
	 * and decrementing/incrementing the SAME two counters). **Confirmed
	 * real hazard, reproduced faithfully rather than guarded**: if
	 * `+0x29c98fc` (free count) is nonzero but `+0x29c98f4` (free
	 * head) is somehow already 0 -- an invariant violation that
	 * shouldn't occur in practice -- the real code dereferences a
	 * near-NULL address. Reads a further field from the popped node
	 * (own real meaning not independently confirmed -- plausibly a
	 * self-pointer, since it's immediately used with the SAME
	 * `+0x24`/`+0x28`/`+0x30` active-list field offsets `FreeSlotVoiceData`
	 * itself uses) and inserts it into the active list (`+0x29c9900`),
	 * the exact mirror of that function's own removal.
	 *
	 * If the free list is empty, falls back to stealing a dying voice
	 * via the SAME group-bit scan `EmergencyFreeDyingSlotVoiceData`
	 * uses (sec 10.92): on a match, calls `EmergencyFreeAllVoices()`
	 * always, and `FreeSlotVoiceData(true)` (which populates the free
	 * list) unless a specific sum/flag combination says otherwise --
	 * then retries the WHOLE function from the top. **Confirmed real
	 * hazard, also reproduced faithfully**: if that specific
	 * "no-`FreeSlotVoiceData`-call" combination is hit and nothing
	 * else populates the free list, or if neither group yields any
	 * match at all, the real code retries indefinitely (or hits the
	 * near-NULL hazard above) -- a genuine potential infinite loop in
	 * the original firmware, not a defect introduced by this
	 * reconstruction.
	 *
	 * UPDATE (batch 47): the return value is a small (~0x40-byte)
	 * free-list/active-list bookkeeping NODE, confirmed via
	 * `ChangeProgram()`'s own real disassembly (oa_global.h ->
	 * global.cpp) dereferencing the returned pointer's own `+0x8` field
	 * to reach the ACTUAL `CSTGSlotVoiceData*` payload -- the SAME
	 * node/payload split already established for
	 * `ResolveActiveVoiceDataNode()` (global.cpp), now confirmed a
	 * second, independent time via this completely different table/
	 * free-list family. This function's own `CSTGSlotVoiceData*` return
	 * type is a pre-existing, harmless type looseness (nothing before
	 * batch 47 ever dereferenced the return value's own fields, and
	 * `test_global.cpp`'s own [38] scenario already mocks the returned
	 * node at exactly its real small size, ~0x40 bytes, not a full
	 * `CSTGSlotVoiceData`) -- left as-is rather than renamed, to avoid
	 * unnecessary churn; `ChangeProgram()` is simply the first real
	 * caller to know to read the extra `+0x8` indirection.
	 */
	CSTGSlotVoiceData *GetFreeSlotVoiceData();

	/*
	 * ProcessCCSpecialMapping(unsigned char, unsigned char, unsigned
	 * char) (sec 10.101, .text+0x91c0, 371 bytes) confirmed: `p1` (a
	 * CC-assign table index) bound-checked against `0x77`. Indexes the
	 * SAME confirmed real 120-slot CC-assign claim table at
	 * `+0x29cc11c` (sec 10.74/10.90) -- no-op (`return 0`) if the
	 * slot's claimed-flag is clear, or if the slot's own value byte
	 * doesn't match `p2` (UNLESS the value byte is the literal `0x10`
	 * AND `p2` equals `this->fieldAt(0x6b8)` -- a real, confirmed
	 * "channel wildcard" exception).
	 *
	 * If the slot's free-flag is clear (occupied): dispatches through
	 * `CSTGControllerRTData::HandleControllerChange` (declared above,
	 * deferred) using the slot's own tag dword, `p3`, and two literal
	 * `false` args, returning `1`.
	 *
	 * Otherwise, resolves a mode-dependent "target" address (the SAME
	 * confirmed real per-mode bases `ProcessSetListSlotOnlyChange`/
	 * `UpdateVJSXAssignment` already use: `CSTGProgram+3` default,
	 * `CSTGCombi+6` mode==1, `CSTGSequence+0` mode==2, or a fixed
	 * `+0x2976e33` literal special-case when `this->fieldAt(0x698)==
	 * 0xfffe`), writes the slot's own tag dword's low byte into
	 * `target+0xadf`, and calls `CSTGControllerInfo::SetPerfSwitch`
	 * (declared above, deferred) on the embedded sub-object at
	 * `target+0xad3` with a literal `2` and `p3 > 0x3f`, returning `1`.
	 */
	int ProcessCCSpecialMapping(unsigned char p1, unsigned char p2, unsigned char p3);

	/*
	 * CompletePerformanceActivation() (sec 10.102, .text+0x4ea0, 385
	 * bytes) confirmed: sets `+0x2975184` to `3` (the SAME "repeating"
	 * state field `RepeatLastPerformanceChange`/`PreprocessPerformanceChange`
	 * use, sec 10.90/10.95, confirming a third real value beyond 0/1).
	 * If `+0x6a4 != 0`, resolves the SAME per-slot record base as
	 * `DoesPerfChangeRequestMatchType` (sec 10.92) and calls TWO
	 * confirmed real `Activate()` methods: `CSetList::Activate()` on a
	 * DIFFERENT idx-only-strided object (`+0x293374c`), and
	 * `CSetListSlot::Activate()` (sec 10.97) on the SAME per-slot
	 * record. Also saves `CSTGPerformanceVarsManager::+0x23d4`'s own
	 * pointer field (the SAME field `SetUseGlobalAudioInputSettings`
	 * uses, sec 10.90) for a later, UNCONDITIONAL call -- **confirmed
	 * real quirk, preserved verbatim**: that saved pointer is used by
	 * `CSTGControllerRTData::OnPerformanceActivate()` (new deferred
	 * extern) regardless of whether the `+0x6a4` branch above actually
	 * ran, meaning it's genuinely uninitialized garbage on the
	 * `+0x6a4==0` path in the real binary.
	 *
	 * Always: clears the SAME `+0x29cc0c9` bit0 "ext mode deferred"
	 * flag as `CompleteDeferredExtModeChange` (calling the identical
	 * `OnExtModeSetChange()` if set), sends the SAME 5-byte "Karma"-
	 * shaped MIDI message as `NotifyKarmaPerformanceChange` (sec
	 * 10.92, same mode-dependent valA/valB computation), calls
	 * `OnPerformanceActivate()` as described above, then sends a
	 * SECOND 5-byte message matching `PreprocessPerformanceChange`'s
	 * own shape (sec 10.95) but with a confirmed real DIFFERENT literal
	 * at `+2` (`0x04` here vs `0x03` there).
	 */
	void CompletePerformanceActivation();

	/*
	 * IncrementPerformance()/DecrementPerformance() (sec 10.103,
	 * .text+0x8710/0x88a0, 400 bytes each) confirmed: the SAME
	 * dispatcher gate + two-different-dual-slots quirk as
	 * `HandleMidiBankAndPerformanceChange` (sec 10.99). If slotB[0]!=0
	 * (`tag=1` producer): a real saturating inc/dec over slotB's own
	 * `value1`(+8)/`value2`(+0xc) pair, confirmed real bounds `0x7e`
	 * (Increment) mirrored by `0`/`0x7f` (Decrement) -- e.g. Increment:
	 * if `value2<=0x7e`, `value2++`; else if `value1<=0x7e`,
	 * `value1++, value2=0`; else both reset to `0`. Decrement mirrors
	 * this exactly (`value2!=0` -> `value2--`; else `value1!=0` ->
	 * `value1--, value2=0x7f`; else both reset to `0x7f`).
	 *
	 * If slotB[0]==0: dispatches through `IncrementCombiIndex`/
	 * `IncrementAliasProgramIndex` (or the `Decrement` pair) based on
	 * slotB's own `+4` mode selector, mirroring
	 * `HandleMidiBankAndPerformanceChange`'s own mode encoding
	 * (`0`=AliasProgram, `1`=Combi, anything else=no-op).
	 *
	 * Both submit with `source=1` -- a confirmed real DISTINCTION from
	 * every other producer in this cluster, which all use `source=2`.
	 */
	void IncrementPerformance();
	void DecrementPerformance();

	/*
	 * ProcessPerfChangeRequest(CSTGPerfChangeRequest const&) (sec
	 * 10.104, .text+0x5e40, 578 bytes) confirmed: copies the ENTIRE
	 * incoming request verbatim into the confirmed real `+0x297514c`
	 * dual-slot ("slotA") address (sec 10.90) -- i.e. this IS the real
	 * function that populates that slot for everything else in this
	 * cluster to read. Calls `IsSetListSlotChangeOnly()` (sec 10.96) on
	 * the copy:
	 *   - if `true`: resolves the SAME per-slot `CSetListSlot` record
	 *     as `ProcessSetListSlotOnlyChange`/`ProcessCCSpecialMapping`
	 *     (sec 10.97/10.101, including the SAME mode==2 vtable-slot-0x1e
	 *     dispatch through a resolved `CSTGSequence` and the SAME
	 *     `+0x6a6` write), calls `CSetListSlot::Activate()`, then --
	 *     based on the just-copied `source` field -- sends the SAME
	 *     24-byte `PushUnsolicitedMessage` shape (if `source` is `1` or
	 *     `2`) and/or calls `SendPerfChangeToMidiOut()` (sec 10.98, if
	 *     `source` is `0` or `1`) on the copy -- the exact same
	 *     "push-then-maybe-send" pattern `ProcessSetListSlotOnlyChange`
	 *     already established.
	 *   - if `false`: runs the EXACT SAME "headroom reset + deactivate"
	 *     sequence as `PreprocessPerformanceChange` (sec 10.95,
	 *     confirmed byte-identical at the instruction level, not just
	 *     similar-shaped -- factored into one shared helper,
	 *     `RunPerformanceDeactivateSequence`, rather than duplicated).
	 */
	void ProcessPerfChangeRequest(const CSTGPerfChangeRequest &req);

	/*
	 * StartPendingPerformanceChange() (sec 10.105, .text+0x6090, 642
	 * bytes) confirmed: no-op if `+0x2975184` ("repeating"/busy state,
	 * sec 10.90/10.95/10.102) is set, or if `+0x2975185` (a "pending"
	 * flag -- the SAME byte the dual-slot selector reads elsewhere in
	 * this cluster) is clear. Otherwise clears that pending flag and
	 * copies a SECOND confirmed real request-shaped record at
	 * `+0x2975168` (all 7 fields, same layout as `CSTGPerfChangeRequest`)
	 * into the confirmed `+0x297514c` slot. **Confirmed real: every
	 * instruction from that point on is IDENTICAL to
	 * `ProcessPerfChangeRequest`'s own body** (same
	 * `IsSetListSlotChangeOnly` dispatch, same both branches) -- since
	 * `ProcessPerfChangeRequest` ITSELF starts by copying its own `req`
	 * argument into that exact same `+0x297514c` slot, this function is
	 * modeled simply as a call to `ProcessPerfChangeRequest` with the
	 * `+0x2975168` record reinterpreted as the argument, rather than
	 * duplicating ~500 bytes of identical logic.
	 */
	void StartPendingPerformanceChange();

	/*
	 * GetMostRecentlyRequestedPerformanceIdForType(eSTGPerformanceType,
	 * CPerformanceId*) const (sec 10.106, .text+0x5450, 706 bytes)
	 * confirmed: checks the confirmed real "pending" slot (`+0x2975168`,
	 * gated by `+0x2975185`, sec 10.90/10.105) first, then the confirmed
	 * real "current" slot (`+0x297514c`) as a fallback, then a THIRD,
	 * fully literal-field fallback if neither slot matches. Each slot's
	 * own "does this match `type`" check reuses the SAME per-slot
	 * `+0x2933750` type-table lookup (tag!=0) or the SAME `+0x64+m*4`
	 * dword-table lookup (tag==0, `m=mode-1`) that
	 * `DoesPerfChangeRequestMatchType` (sec 10.92) already established
	 * -- but each slot's own separate "resolve the actual
	 * `CPerformanceId`" step (once matched) uses a CONFIRMED REAL
	 * OPPOSITE `m<=1`/`m>1` direction for its own `byte0` computation
	 * compared to the match-check step (table for `m<=1` when
	 * checking a match; literal `1` for `m<=1` when resolving) -- a
	 * genuine, independently-confirmed asymmetry, not a transcription
	 * error. The final all-literal fallback (reached only if neither
	 * slot's own type matches) returns real fixed field values keyed
	 * directly off `type` (`0`/`1`/`2`), `false` for anything else
	 * (or if a confirmed real `0xfffe` sentinel is hit for `type==1`).
	 */
	bool GetMostRecentlyRequestedPerformanceIdForType(unsigned int type, CPerformanceId *out) const;

	/*
	 * SetEditInContextState(eSTGEditInContextType, unsigned int) (sec
	 * 10.107, .text+0x9450, 701 bytes) confirmed: always stores `type`
	 * to `+0x29cc4dc` and `value` to `+0x29cc4e0`. If `type==0`, returns
	 * immediately (no further effect). Otherwise, the real disassembly
	 * copies a 16-bit flags word from `CSTGControllerRTData::sInstance
	 * +0x22` into `this->word(0x29cc4e4)` (the SAME field
	 * `SetCurrentEditInContextTimbreSolo` uses, sec 10.90) ONE BIT AT A
	 * TIME across 16 nearly-identical conditional set/clear blocks --
	 * **confirmed algebraically equivalent to a single 16-bit copy**
	 * (every one of the 16 bits is unconditionally overwritten during
	 * the pass, so the destination's own prior value never survives),
	 * modeled here as that direct copy rather than the mechanical
	 * bit-by-bit reproduction. Then zeroes the source word and calls
	 * `CSTGControllerRTData::sInstance->NotifySoloChange()` (declared
	 * above, deferred).
	 */
	void SetEditInContextState(int type, unsigned int value);

	/*
	 * CompletePerformanceChange() (sec 10.108, .text+0x5030, 756 bytes)
	 * confirmed: always clears `+0x2975184` (the "busy/repeating" flag
	 * used throughout this cluster, sec 10.90/10.95/10.102/10.105).
	 * Reads the confirmed real `source` field at `+0x297515c` (`slotA`'s
	 * own `source`, sec 10.90/10.104): if `source-1 <= 1` (i.e. `source`
	 * is `1` or `2`), sends a real `PushUnsolicitedMessage` first --
	 * either a 24-byte shape (`slotA.tag != 0`: `{0x18, 1, 0, 0x1f,
	 * value1, value2, 3}`) or a 28-byte shape (`slotA.tag == 0`:
	 * `{0x1c, 1, 0, 0, resolvedType, value1, value2, 3}`, where
	 * `resolvedType` reuses the SAME `m<=1 ? table(+0x64+m*4) : 1`
	 * resolve direction `GetPerformanceIdFromPerfChangeRequest` (sec
	 * 10.92) already established). Regardless of `source`, always then
	 * clears `CSTGMidiDispatcher::sInstance`'s own `+0x0` byte and
	 * `CSTGMessageProcessor::sInstance`'s own `+0x54` byte (the SAME
	 * two flags `PreprocessPerformanceChange`/`CompletePerformanceActivation`
	 * set, sec 10.95/10.102 -- this function is their real "undo"),
	 * calls `CSTGControllerRTData::sInstance->ResetAllJumpCatch()` (sec
	 * 10.69), and zeroes a confirmed real 48-dword range
	 * (`+0x29138..+0x291f4`) of `STGAPIFrontPanelStatus::sInstance`.
	 */
	void CompletePerformanceChange();

	/*
	 * ProcessPerformanceChange() (sec 10.113, .text+0x59b0, 873 bytes)
	 * confirmed: sets `+0x2975184` (busy flag) to `2` (not `1`, unlike
	 * every other producer in this cluster); calls
	 * `ResolveActivePerformanceVarsManagerRaw()->FreeVoicelessDyingSlots()`;
	 * allocates `newPerf = CSTGPerformanceVarsManager::sInstance.AllocPerformanceVars()`
	 * (the confirmed "address of the singleton" idiom).
	 *
	 * Resolves a `(category, val1, val2)` triple one of two ways:
	 *   - `slotA.tag != 0`: clamps `slotA.value1` to `<0x80`, resolves
	 *     the per-slot `+0x2933750`-based type table (sec 10.90/10.92)
	 *     to get a signed `recordType`; if `recordType` is `0`, `1`, or
	 *     `2` (as an UNSIGNED comparison -- a negative byte value does
	 *     NOT match this, since sign-extension makes it huge unsigned),
	 *     `category` comes from a THIRD confirmed real 3-entry dword
	 *     table at `+0x58` (`+0x58/+0x5c/+0x60`, distinct from the
	 *     already-confirmed `+0x64`-based 2-entry table elsewhere in
	 *     this cluster); otherwise `category=0`. Also stamps
	 *     `+0x6a4=1`, `+0x6a5=slotA.value1`, `+0x6a6=slotA.value2`, and
	 *     reads `val1`/`val2` from the resolved `+0x2933740`-based
	 *     record's own `+0x11`/`+0x12` bytes (the SAME packed
	 *     `CPerformanceId`-shaped record `GetMostRecentlyRequestedPerformanceIdForType`,
	 *     sec 10.106, already established).
	 *   - `slotA.tag == 0`: `category=slotA.mode`, `val1=slotA.value1`,
	 *     `val2=slotA.value2` directly, and `+0x6a4=0`.
	 *
	 * Both setups then converge on ONE shared category dispatch
	 * (confirmed via the real code literally jumping into the middle of
	 * the `tag!=0` path's own dispatch logic for the `tag==0` case --
	 * modeled here as ordinary shared control flow, not literal gotos):
	 *   - `category==1`: stores `+0x684=1`, `+0x690=val1`, `+0x69c=val2`
	 *     (the confirmed mode==1/`CSTGCombi` field pair).
	 *   - `category==2`: stores `+0x684=2`, `+0x6a0=val2`; ADDITIONALLY
	 *     (confirmed real, category==2 ONLY) resolves a `CSTGProgramSlot`-
	 *     shaped address via one of two confirmed formulas depending on
	 *     `+0x6a4` (a further `+0x27cdb07`/`+0xe8`-strided sub-table when
	 *     `+0x6a4==0`, or the per-slot record's own `+3` byte otherwise),
	 *     calls `GetProperMidiChannel()` on it, and stores the result at
	 *     `+0x6b9`.
	 *   - `category==0` (default): stores `+0x688=val1`, `+0x684=0`,
	 *     `+0x694=val2`, then calls `GetAliasPgmBankMapping(val1, val2,
	 *     &+0x68c, &tmp)` and stores `+0x698=tmp` (extending the
	 *     already-confirmed `+0x688/+0x694`-vs-`+0x68c/+0x698` "raw
	 *     alias value vs. converted real value" field-pair convention).
	 *
	 * For `category` 0 or 1 (i.e. NOT the `category==2` GetProperMidiChannel
	 * step above), `+0x6b9` is instead simply copied from `+0x6b8` (the
	 * cluster's own recurring "current global MIDI channel" field).
	 *
	 * Either way, if `source` (`+0x297515c`) is `> 1`, calls
	 * `CSTGMidiDispatcher::sInstance->PerfChangeControllerReset()`
	 * directly; if `source <= 1`, calls `SendPerfChangeToMidiOut(*slotA)`
	 * (sec 10.98) FIRST, then the same `PerfChangeControllerReset()`.
	 *
	 * Either way, if `val2 == 0xfffe` (the sentinel), calls
	 * `CSTGPerformanceVarsManager::sInstance.StealAllDyingPerformanceVars()`
	 * (the SAME "address of the singleton" idiom) before continuing.
	 *
	 * Then resolves the actual `BeginActivation()` target address by
	 * `category` (0/1/2 -> `CSTGProgram`/`CSTGCombi`/`CSTGSequence`-shaped
	 * literal-base-plus-stride addresses, the SAME convention already
	 * established throughout this cluster -- mode==0 additionally checks
	 * `+0x698==0xfffe` and substitutes the confirmed fixed literal
	 * `+0x2976e33` address if so, using `+0x68c`/`+0x694` otherwise),
	 * and calls `newPerf->BeginActivation(target, (bool)+0x6a4)`. If
	 * `+0x6a4 != 0`, ALSO calls `CSetListSlot::BeginActivation()` on the
	 * per-slot record at `+0x2933740+offset+0x10` (the SAME `CSetListSlot`
	 * addressing already established, sec 10.90/10.97).
	 */
	void ProcessPerformanceChange();
};

#endif /* OA_GLOBAL_H */
