// SPDX-License-Identifier: GPL-2.0
/*
 * controller_info_analog_handler.cpp  -  CSTGControllerInfo::
 * AnalogControllerHandler(eSTGAnalogDeviceCode, unsigned short,
 * unsigned short) (batch 65, ground truth `.text+0x9a5f0`, 2274 bytes,
 * mangled `_ZN18CSTGControllerInfo23AnalogControllerHandlerE
 * 20eSTGAnalogDeviceCodett`).
 *
 * This is the real per-knob/slider/joystick/ribbon/vector/aftertouch/
 * value-wheel front-panel analog-controller-move dispatch entry point --
 * `CSTGFrontPanel::HandleAnalogController` (front_panel_handlers.cpp)
 * tail-calls it directly off the NKS4 USB event stream, on the "target"
 * `CSTGControllerInfo` sub-object `ResolveControllerInfoTarget` selects
 * (embedded in whichever program/combi/sequence is currently active).
 *
 * Ground-truthed via `objdump -dr -M intel` against the real OA.ko
 * (same MD5 as front_panel_handlers.cpp's own header, 955636c2b1...),
 * plus a `readelf -rW` dump of the four `.rodata` dispatch tables to
 * recover their real per-slot target symbols (Ghidra's own decompile of
 * this function collapses the pointer-to-member-function dispatch idiom
 * into unreadable pseudo-code -- this reconstruction is transcribed from
 * the raw disassembly, matching front_panel_handlers.cpp's own stated
 * reason for doing the same).
 *
 * ---------------------------------------------------------------------
 * DEVICE-CODE DISPATCH (confirmed via the real range-check sequence):
 *
 *   device 8-15  (knobIndex = device-8, 0-7): "knob" dispatch. Gated:
 *     - if `CSTGControllerRTData::sInstance[0x2f] & 8` ("busy"/UI-edit
 *       flag): `SendUnsolControl2MessageToUI(0xe, knobIndex, param2, 1)`,
 *       return -- UI notify only, no real value applied.
 *     - else if `CSTGGlobal::sInstance[0x29cc4dc] != 0` (the SAME
 *       "edit-in-context" field `SendUnsolicitedUIParam`,
 *       controller_info_send_unsolicited_ui_param.cpp, already gates
 *       on): `HandleEditInContextKnob(device, param2, param3)`; if it
 *       returns true, done; if false, fall through to the mode dispatch
 *       below anyway (confirmed via the real post-call branch).
 *     - else: reads the current knob-assignment "mode"
 *       (`CSTGControllerRTData::sInstance[0x2b]`, signed byte, 0-8) and
 *       calls the matching `AnalogKnobXxxHandler(knobIndex, param2,
 *       param3)` (real pointer-to-member-function table at
 *       `.rodata+0x480c0`, 9 entries -- see the per-slot map below).
 *
 *   device 16-23 (sliderIndex = device-16, 0-7): IDENTICAL shape to
 *     knobs, using `SendUnsolControl2MessageToUI(0xf, sliderIndex,
 *     param2, 1)`, `HandleEditInContextSlider`, and the SEPARATE
 *     `AnalogSliderXxxHandler` table at `.rodata+0x48060` (9 entries,
 *     same 9 assignment modes).
 *
 *   device 1-7 (index = device-1, 0-6): fixed one-to-one physical
 *     controllers, NOT mode-dispatched -- direct table lookup at
 *     `.rodata+0x48020` (7 entries: JoystickX/Y, RibbonX/Z, VectorX/Y,
 *     Aftertouch). Gated only on the SAME `[0x2f]&8` busy flag: if set,
 *     jumps into a small embedded (unnamed, no separate real symbol)
 *     7-way switch on `device` (`.rodata+0x47e80`, index 0-7, RibbonZ's
 *     own slot has NO write at all) writing the raw `param3` reading
 *     into a per-device `STGAPIFrontPanelStatus` UI-echo field (see
 *     `STGAPI_OFF_ANALOG_ECHO_*`, oa_setup_global_resources.h) and
 *     returning; if clear, calls `AnalogXxxHandler(param2, param3)`
 *     directly (no device-index argument -- these are physically single
 *     controllers).
 *
 *   device 0x19-0x1D (index = device-0x19, 0-4): same one-to-one shape
 *     as 1-7, direct table at `.rodata+0x47fe0` (5 entries:
 *     ValueSlider/Tempo/FootPedal/FootSwitch/Damper), called as
 *     `AnalogXxxHandler(param2, param3)` when the busy flag is clear.
 *     When the busy flag IS set, FOUR of the five devices are
 *     individually special-cased ahead of a generic fallback (all
 *     confirmed real, no DSP):
 *       - device==0x1a: `SendUnsolControl2MessageToUI(0xc, 0, param3, 1)`
 *       - device==0x1b: `SendUnsolControl2MessageToUI(0x15, 0, param3, 1)`
 *       - device==0x1c: `SendUnsolControl2MessageToUI(0x15, 1, param3, 1)`
 *       - device==0x1d (Damper): direct store, NO message --
 *         `STGAPIFrontPanelStatus[STGAPI_OFF_ANALOG_ECHO_DAMPER] =
 *         0x3ff - param3` (confirmed polarity-inverted continuous echo).
 *       - anything else in range (i.e. device==0x19):
 *         `SendUnsolControl2MessageToUI(0xf, 9, param2, 1)` (uses
 *         `param2`, NOT `param3`, unlike the other three -- confirmed by
 *         direct register trace, not an oversight).
 *
 *   device 0x18 ("Value" knob/jog-wheel): its own internal mode
 *     dispatch, see below.
 *
 *   device 0x1e: `SetControllerAssignment(selfRef=rtd+3, newValue=
 *     (param2!=0) ? rtd[8] : 0, notify=true)` on
 *     `CSTGControllerRTData::sInstance`, plus an unconditional
 *     `rtd[5] = (param2!=0)` store first. A FOURTH observed caller of
 *     `SetControllerAssignment` (oa_global.h's own comment already notes
 *     the target varies by caller) -- confirmed via direct register
 *     trace (`edx`=selfRef, NOT `eax`, since `SetControllerAssignment`
 *     is a real member function with `eax`=this=`rtd` itself).
 *
 *   anything else (out of all ranges): silent return, no-op.
 *
 * ---------------------------------------------------------------------
 * DEVICE 0x18 ("VALUE") MODE DISPATCH -- reads the SAME
 * `CSTGControllerRTData::sInstance[0x2b]` mode byte as knobs/sliders,
 * gated first by the SAME `[0x2f]&8` busy flag
 * (`SendUnsolControl2MessageToUI(0xf, 8, param2, 1)`, return, if set):
 *
 *   mode==4 ("Value as extended-mode fader"): confirmed REAL, fully
 *     reconstructed --
 *       - if `[0x2f]&2` ("jump-catch" sub-flag) set: looks up the CC
 *         currently assigned to knob-mode-row-0 (`CSTGCCInfo::
 *         sCCInfoTable[ CSTGGlobal[0x29cbc50 + mode*9] ]`'s own byte
 *         +0, `mode` = `CSTGGlobal[0x29cc0c8]`; 0xff = unassigned,
 *         silent return), range-clamps it into `rtd[0x86]` if <=0x7f,
 *         unconditionally calls `JumpCatch()->UpdateStatus()` then
 *         `SendExtModeSliderEvent(8, ccValue, true)`.
 *       - else: `JumpCatch()->CheckPosition(param2, true /-*flag*-/)`; if
 *         false, silent return; if true,
 *         `SendExtModeSliderEvent(8, param2, true)`.
 *
 *   mode==6 ("Tempo"): busy(`[0x2f]&2`) sub-path confirmed REAL --
 *     `rtd[0x85]=param2; JumpCatch()->UpdateStatus();
 *     SendUnsolControl2MessageToUI(0xf, 8, param2, 1)`. The NON-busy
 *     sub-path is a genuine float tempo-curve conversion of `param3`
 *     (`.rodata.cst4` float constants, `fisttp`-based fixed-point
 *     rounding, TWO different curve shapes selected by
 *     `rtd[0x48]==1`) feeding `JumpCatch()->CheckPosition()` and
 *     `CSTGControllerInfo::NotifySetListParam(0xf, tempoValue, false)`
 *     -- DSP-adjacent (a tempo/DSP-clock parameter curve), deliberately
 *     left as the local stub `ApplyValueKnobTempoCurve()` below, not
 *     independently traced this pass.
 *
 *   mode==8 ("SetListEQ"): the ENTIRE branch (busy and non-busy alike)
 *     depends on a float EQ-curve conversion of `param3` computed
 *     BEFORE the busy-flag check (the busy sub-path reuses the
 *     curve-derived byte, not raw `param2`) -- genuinely DSP (EQ
 *     parameter smoothing via `CSTGFrontPanelSmoothers::
 *     SetFPEQSmoother`/`SetListEQSmootherOutput`), left as the local
 *     stub `ApplyValueKnobEQCurve()` below in its entirety.
 *
 *   else (any other mode): busy(`[0x2f]&2`) sub-path confirmed REAL --
 *     identical shape to mode==6's own busy sub-path (`rtd[0x85]=param2;
 *     UpdateStatus(); SendUnsolControl2MessageToUI(0xf,8,param2,1)`).
 *     Non-busy sub-path: `JumpCatch()->CheckPosition(param2,
 *     Global[0x6af]!=0)`; if false, silent return; if true, a genuine
 *     effect-rack front-panel-parameter-smoother invocation
 *     (`CSTGFrontPanelSmoothers::EffectRackParamSmootherOutput`/
 *     `SetFPISmoother` + `CSTGControllerInfo::NotifyEffectSlotParam`) --
 *     DSP-adjacent, deliberately left as the local stub
 *     `ApplyValueKnobEffectRackEdit()` below (the `CheckPosition` GATE
 *     itself IS reconstructed for real; only the smoother invocation
 *     past that gate is stubbed).
 *
 * All three deferred stubs are named extractions of INLINED logic, not
 * real OA.ko symbols of their own -- same idiom as
 * `ResolveControllerInfoTarget` (front_panel_handlers.cpp). See
 * HARDWARE_REVIEW_LOG.md.
 *
 * ---------------------------------------------------------------------
 * FOUR `.rodata` POINTER-TO-MEMBER-FUNCTION TABLES -- each slot is an
 * 8-byte `{ptr, adj}` pair (Itanium ABI ptmf representation). Every one
 * of the 30 real+weak slots below has `adj`==0 (confirmed: raw
 * `.rodata` bytes are all-zero at every `+4` slot, only `+0` slots carry
 * an `R_386_32` relocation) -- so every real call's implicit `this` is
 * simply the enclosing `AnalogControllerHandler`'s own `this`, and this
 * reconstruction calls these as plain non-virtual member functions
 * rather than modeling a real pointer-to-member value.
 *
 *   `.rodata+0x48020` "handlers" (7 entries, direct index=device-1):
 *     0 AnalogJoystickXHandler   1 AnalogJoystickYHandler
 *     2 AnalogRibbonXHandler     3 AnalogRibbonZHandler
 *     4 AnalogVectorXHandler     5 AnalogVectorYHandler
 *     6 AnalogAftertouchHandler
 *
 *   `.rodata+0x47fe0` "handlers" (5 entries, direct index=device-0x19):
 *     0 AnalogValueSliderHandler 1 AnalogTempoHandler
 *     2 AnalogFootPedalHandler   3 AnalogFootSwitchHandler
 *     4 AnalogDamperHandler
 *
 *   `.rodata+0x48060` "sliderHandlers" (9 entries, index=mode 0-8):
 *     0 AnalogSliderT18Handler(WEAK/0)   1 AnalogSliderT916Handler(WEAK/0)
 *     2 AnalogSliderA18Handler(WEAK/0)   3 AnalogSliderA916Handler(WEAK/0)
 *     4 AnalogSliderExtHandler           5 AnalogSliderRTKHandler
 *     6 AnalogSliderTAHandler            7 AnalogSliderAInHandler
 *     8 AnalogSliderSetListEQHandler
 *
 *   `.rodata+0x480c0` "knobHandlers" (9 entries, index=mode 0-8):
 *     0 AnalogKnobT18Handler(WEAK/0)     1 AnalogKnobT916Handler(WEAK/0)
 *     2 AnalogKnobA18Handler(WEAK/0)     3 AnalogKnobA916Handler(WEAK/0)
 *     4 AnalogKnobExtHandler             5 AnalogKnobRTKHandler
 *     6 AnalogKnobTAHandler              7 AnalogKnobAInHandler
 *     8 AnalogKnobSetListEQHandler
 *
 * The 8 "WEAK/0" slots are confirmed weak-undefined in the real OA.ko
 * itself (see oa_global.h's own comment on the matching declarations) --
 * modeled here purely as calls through the declared (weak, undefined)
 * member functions, faithfully reproducing "resolves to a null call if
 * ever reached" rather than inventing a safe no-op.
 */

#include "oa_setup_global_resources.h"

/* ---------------------------------------------------------------------
 * Follow-up pass (2026-07-25): nine of the 22 deferred `AnalogXxxHandler`
 * callees reconstructed for real -- the physical one-to-one controllers
 * (joystick/ribbon/vector/foot-pedal/foot-switch/damper/value-slider),
 * genuinely hardware/UI parameter-update code, not DSP. Ground-truthed
 * via `objdump -dr -M intel` against the real OA.ko. All raw
 * `CSTGControllerRTData`/`CSTGGlobal`/`STGAPIFrontPanelStatus` byte
 * offsets below are UNCONFIRMED-NAME fields (single-purpose reads/writes
 * observed, no independent cross-reference to name them) -- accessed via
 * raw pointer arithmetic per this project's established convention for
 * fields not yet independently identified. See HARDWARE_REVIEW_LOG.md.
 *
 * `kDamperFilterTable` (real mangled `_ZZN18CSTGControllerInfo19
 * AnalogDamperHandlerEttE14s_akucAdToMidi`, a FUNCTION-LOCAL static
 * inside the real `AnalogDamperHandler`, `.rodata+0x47ee0`, 256 bytes,
 * confirmed real via raw-byte extraction) -- a 10-bit-to-7-bit "AD to
 * MIDI" conversion curve: 42 leading zero entries, a nonlinear ramp
 * 0x1e..0x7f over the next ~65 entries, then clamped flat at 0x7f for
 * the rest. Feeds
 * `CPedalFilter::Filter()` (itself a deferred extern).
 */
static const unsigned char kDamperFilterTable[256] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x1f, 0x21, 0x22, 0x24, 0x25,
	0x27, 0x28, 0x2a, 0x2b, 0x2d, 0x2e, 0x30, 0x32, 0x33, 0x35, 0x36, 0x38, 0x39, 0x3b, 0x3c, 0x3e,
	0x3f, 0x41, 0x42, 0x44, 0x46, 0x47, 0x49, 0x4a, 0x4c, 0x4d, 0x4f, 0x50, 0x52, 0x53, 0x55, 0x56,
	0x58, 0x5a, 0x5b, 0x5d, 0x5e, 0x60, 0x61, 0x63, 0x64, 0x66, 0x67, 0x69, 0x6b, 0x6c, 0x6e, 0x6f,
	0x71, 0x72, 0x74, 0x75, 0x77, 0x78, 0x7a, 0x7b, 0x7d, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
};

const unsigned char CSTGControllerRTData::kControllerLockFlagTable[11] = {
	0x00, 0x01, 0x06, 0x02, 0x04, 0x08, 0x09, 0x0e, 0x0a, 0x0c, 0x10,
};

/* ---------------------------------------------------------------------
 * Batch 68 (2026-07-25): six more of the 13 previously-deferred
 * `AnalogXxxHandler` callees reconstructed for real -- confirmed
 * genuinely hardware/UI (not DSP) via full disassembly of ALL 13
 * remaining candidates (not just these six): `AnalogJoystickXHandler`,
 * `AnalogAftertouchHandler`, and the "Ext"/"RTK" knob+slider mode-
 * dispatch pairs (`AnalogKnobExtHandler`/`AnalogSliderExtHandler`,
 * `AnalogKnobRTKHandler`/`AnalogSliderRTKHandler`). The other SEVEN
 * (`AnalogTempoHandler`, both SetListEQ handlers, both "TA"/Tone-Adjust
 * handlers, both "AIn"/Audio-Input handlers) are CONFIRMED DSP this
 * pass (not just carried forward as suspected) -- see oa_global.h's own
 * updated comment on the full 13-item list for the disassembly evidence
 * (a genuine `CSTGToneAdjust`/`CSTGFrontPanelSmoothers` virtual dispatch
 * in each, both entirely new and substantial unmodeled DSP-parameter
 * class hierarchies) -- deliberately left as plain deferred externs,
 * no bodies here.
 *
 * `ApplyRTKCurve` -- both `AnalogKnobRTKHandler`/`AnalogSliderRTKHandler`
 * share this exact "RTK" (Real Time Knob) bipolar curve: a flat 64
 * (center) dead-zone for raw values in [482,542], `127 - floor(raw *
 * 0.13049793243408203)` below it, `floor((1024-raw) * 0.13257262110710144)`
 * above it. The four float constants (`.rodata.cst4+0x294/0x298/0x29c/
 * 0x2a0/0x2a4`, five slots total) were extracted BYTE-EXACT via a
 * Python struct.unpack script reading the real OA.ko file directly (not
 * hand-transcribed), matching this project's established "script, don't
 * hand-transcribe" table-extraction discipline. This is genuine FPU
 * math (the real function uses x87 `fild`/`fmul`/`fisttp`) -- this file
 * needs the SAME `-mhard-float -msse2 -mfpmath=sse` per-object Makefile
 * override the other genuine-float TUs in this project already use;
 * SSE rounding vs the original's x87 sequence is not guaranteed
 * bit-identical, matching this project's established tolerance
 * elsewhere (e.g. `lfo_tables.cpp`) for float DSP-adjacent curves
 * compiled this way.
 */
static unsigned int ApplyRTKCurve(unsigned short raw)
{
	const float kLow = 482.0f;
	const float kHigh = 542.0f;
	const float kMax = 1024.0f;
	const float kLowScale = 0.13049793243408203f;
	const float kHighScale = 0.13257262110710144f;

	if ((float)raw < kLow)
		return (unsigned int)(127 - (int)(raw * kLowScale));
	if ((float)raw <= kHigh)
		return 0x40;
	return (unsigned int)(int)((kMax - (float)raw) * kHighScale);
}

/* Shared MIDI-send tail for AnalogJoystickXHandler's two identical real
 * call sites (`.text+0x98b20`/`0x98b80`) -- reads `CPitchBendFilter`'s
 * OWN state (its `Filter()` call is what set these, not this function)
 * and sends a 5-byte Pitch Bend message `{0xe0|channel, lsb, msb, 1,
 * 0xff}` via the established `CSTGMidiPortManager::sInstance+0x208`
 * embedded `CSTGMidiQueueWriter`. The `0xff` terminator (vs the note-
 * on/off family's `0xfe`) matches a 2-data-byte MIDI message; `0xfe`
 * matches 3-data-byte (front_panel_handlers.cpp's own note-on/off,
 * `0x90`/`0x80`, DOES send 2 data bytes -- terminator byte's exact
 * meaning is not independently confirmed beyond "differs by message
 * shape", flagged in HARDWARE_REVIEW_LOG.md). */
static void SendPitchBendFromFilterState(CSTGControllerRTData::CPitchBendFilter *filt)
{
	unsigned char msg[5];
	msg[0] = (unsigned char)(filt->channel | 0xe0);
	msg[1] = (unsigned char)(filt->value & 0x7f);
	msg[2] = (unsigned char)((filt->value >> 7) & 0x7f);
	msg[3] = 1;
	msg[4] = 0xff;
	CSTGMidiQueueWriter *writer = (CSTGMidiQueueWriter *)
		((unsigned char *)CSTGMidiPortManager::sInstance + 0x208);
	writer->Write(msg, 5, false);
}

/* AnalogAftertouchHandler(a, b) -- `.text+0x98260`, 160 bytes. Confirmed
 * real: `a` (param2) unused (only its low byte is stashed into two raw
 * `CSTGControllerRTData` fields, `+0x1c` always and `+0x1f` only on the
 * not-locked path -- neither independently named, see
 * HARDWARE_REVIEW_LOG.md). `b` (param3, the raw 10-bit aftertouch ADC
 * reading, only its low byte is actually used) drives the send. Locked
 * (kControllerLockFlagTable bit 0 set): STGAPI echo write only, no MIDI
 * send. Not locked: sends a 2-byte MIDI Channel Pressure message
 * `{0xd0|channel, value}` via the SAME `CSTGMidiPortManager::
 * sInstance+0x208` embedded writer `global.cpp`'s `SendGlobalMidiMessage`
 * already established, channel from `CSTGGlobal::sInstance+0x6b9` (the
 * SAME field `UpdateMIDIChannel` writes). The STGAPI echo write
 * (`STGAPI_OFF_ANALOG_ECHO_ATOUCH`, already an established constant)
 * happens on BOTH paths. */
void CSTGControllerInfo::AnalogAftertouchHandler(unsigned short a, unsigned short b)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *rtdBytes = (unsigned char *)rtd;

	rtdBytes[0x1c] = (unsigned char)a;

	unsigned char lockFlags =
		CSTGControllerRTData::kControllerLockFlagTable[(unsigned char)(signed char)rtdBytes[0x15]] |
		CSTGControllerRTData::kControllerLockFlagTable[(unsigned char)(signed char)rtdBytes[0x14]] |
		CSTGControllerRTData::kControllerLockFlagTable[(unsigned char)(signed char)rtdBytes[0x16]];

	if (!(lockFlags & 0x10)) {
		rtdBytes[0x1f] = (unsigned char)a;

		unsigned char channel = ((unsigned char *)CSTGGlobal::sInstance)[0x6b9];
		unsigned char msg[5];
		msg[0] = (unsigned char)(channel | 0xd0);
		msg[1] = (unsigned char)a;
		msg[2] = 0;
		msg[3] = 1;
		msg[4] = 0xff;
		CSTGMidiQueueWriter *writer = (CSTGMidiQueueWriter *)
			((unsigned char *)CSTGMidiPortManager::sInstance + 0x208);
		writer->Write(msg, 5, false);
	}

	*(unsigned short *)((unsigned char *)STGAPIFrontPanelStatus::sInstance + STGAPI_OFF_ANALOG_ECHO_ATOUCH) = b;
}

/* AnalogJoystickXHandler(a, b) -- `.text+0x98a60`, 416 bytes. Confirmed
 * real: `a` (param2) unused -- only `b` (param3, the raw 10-bit ADC
 * reading) matters, echoed raw to `STGAPI_OFF_ANALOG_ECHO_VECX`
 * (0xfe, the SAME already-established constant device-5 VectorX also
 * uses -- confirmed real, not a naming collision). Locked
 * (kControllerLockFlagTable bit 0): plain return, no echo, no send at
 * all (UNLIKE Aftertouch above, which still echoes when locked).
 *
 * Curve: exact center (`b==0x200`) -> `0x2000` (14-bit pitch-bend
 * center); within 0x200 of the max end (`0x3ff-b <= 0x200`) -> coarse
 * linear `((0x3ff-b)<<4) & 0xfff0`; otherwise -> a GCC signed-division-
 * by-constant idiom (magic multiplier `0x80402011`), translated here
 * LITERALLY instruction-for-instruction (not algebraically simplified)
 * to guarantee bit-exact results regardless of the actual intended
 * divisor -- matches this project's established "recognize but
 * literally transcribe" treatment of compiler division-by-constant
 * patterns.
 *
 * All three curve paths funnel into the SAME real 2-call sequence
 * (confirmed via control-flow tracing, not assumed): call
 * `CPitchBendFilter::Filter(curved)`; if true, send a Pitch Bend
 * message reading the filter's own post-call state (`SendPitchBend
 * FromFilterState` above); THEN unconditionally call `Filter(curved)`
 * a SECOND time (same object, same value) and send again if that
 * ALSO returns true. `CPitchBendFilter::Filter`'s own body is not
 * reconstructed (deferred extern, like `CJumpCatch`/`CPedalFilter`) --
 * why it can accept/reject the identical repeated call is unknown,
 * flagged in HARDWARE_REVIEW_LOG.md. */
void CSTGControllerInfo::AnalogJoystickXHandler(unsigned short, unsigned short b)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *rtdBytes = (unsigned char *)rtd;

	unsigned char lockFlags =
		CSTGControllerRTData::kControllerLockFlagTable[(unsigned char)(signed char)rtdBytes[0x15]] |
		CSTGControllerRTData::kControllerLockFlagTable[(unsigned char)(signed char)rtdBytes[0x14]] |
		CSTGControllerRTData::kControllerLockFlagTable[(unsigned char)(signed char)rtdBytes[0x16]];
	if (lockFlags & 1)
		return;

	*(unsigned short *)((unsigned char *)STGAPIFrontPanelStatus::sInstance + STGAPI_OFF_ANALOG_ECHO_VECX) = b;

	unsigned int curved;
	if (b == 0x200) {
		curved = 0x2000;
	} else {
		unsigned short dist = (unsigned short)(0x3ff - b);
		if (dist <= 0x200) {
			curved = ((unsigned int)dist << 4) & 0xfff0;
		} else {
			/* literal translation of the real GCC magic-multiply
			 * division idiom -- see header comment. */
			int t = (int)(((unsigned int)dist << 13) - dist - 0x3ffe00);
			long long prod = (long long)t * (long long)(int)0x80402011;
			int hi = (int)(prod >> 32);
			hi += t;
			hi >>= 8;
			int sign = t >> 31;
			hi -= sign;
			hi += 0x2000;
			curved = (unsigned short)hi;
		}
	}

	CSTGControllerRTData::CPitchBendFilter *filt = CSTGControllerRTData::PitchBendFilter();
	if (filt->Filter((unsigned short)curved))
		SendPitchBendFromFilterState(filt);
	if (filt->Filter((unsigned short)curved))
		SendPitchBendFromFilterState(filt);
}

/* AnalogKnobExtHandler(idx, a, b) -- `.text+0x9a1d0`, 240 bytes.
 * AnalogSliderExtHandler(idx, a, b) -- `.text+0x9a500`, 240 bytes.
 * Confirmed real (byte-identical shape between the two, only the
 * per-index `CJumpCatch` array base/stride and the assignment-table
 * base/stride differ): `b` (param3) entirely unused on both. "Ext"
 * mode knobs/sliders are reassignable per active Ext "set"
 * (`CSTGGlobal+0x29cc0c8`) to any MIDI CC, via the ALREADY-established
 * `UpdateExtKnobCCAssign`/`UpdateExtSliderCCAssign` write targets
 * (`+0x29ca3c8`/stride 8 for knobs, `+0x29cbc48`/stride 9 for sliders --
 * this reconstruction independently re-derives and CONFIRMS both, not
 * just reuses them from memory).
 *
 * busy2 (`CSTGControllerRTData+0x2f` bit 1) SET: re-sync path -- look up
 * the current CC assignment for this index in the active set; 0xff
 * means unassigned (no-op, return); else read that CC's committed
 * value from `CSTGCCInfo::sCCInfoTable[cc*10]` (the established "b0"
 * field), write it into the per-index `CJumpCatch` array's own
 * "target" byte (+2) if in 7-bit range, `UpdateStatus()`, then send
 * `SendExtModeKnobEvent`/`SendExtModeSliderEvent(idx, ccValue, true)`.
 *
 * busy2 CLEAR: live-movement path -- gate `a` (the raw knob/slider
 * movement) through the per-index `CJumpCatch::CheckPosition(a, true)`;
 * if true (physical position has caught up), send the event with the
 * RAW `a` value (not looked up from CCInfoTable).
 *
 * Per-index `CJumpCatch` array bases (stride 3, confirmed distinct from
 * the fixed single-instance `CJumpCatch()` accessor at `+0x84` used by
 * the direct-index handlers above): knobs `+0x54`, sliders `+0x6c` --
 * the SAME slider base `AnalogSliderRTKHandler` below also uses (one
 * shared per-slider-index jump-catch array across modes), separate from
 * the knob RTK array (also `+0x54`, shared with Ext -- knobs and
 * sliders each have their OWN one shared array, not per-mode-per-index
 * arrays).
 */
void CSTGControllerInfo::AnalogKnobExtHandler(unsigned int idx, unsigned short a, unsigned short)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *rtdBytes = (unsigned char *)rtd;
	CSTGControllerRTData::CJumpCatch *jc =
		(CSTGControllerRTData::CJumpCatch *)(rtdBytes + 0x54 + idx * 3);

	if (rtdBytes[0x2f] & 2) {
		unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
		unsigned char extSet = g[0x29cc0c8];
		unsigned char cc = *(g + 0x29ca3c8 + (unsigned int)extSet * 8 + idx);
		if (cc == 0xff)
			return;
		unsigned char ccValue = CSTGCCInfo::sCCInfoTable[(unsigned int)cc * 10];
		if (ccValue <= 0x7f)
			rtdBytes[0x54 + idx * 3 + 2] = ccValue;
		jc->UpdateStatus();
		SendExtModeKnobEvent((int)idx, ccValue, true);
		return;
	}

	if (jc->CheckPosition((unsigned short)a, true))
		SendExtModeKnobEvent((int)idx, a, true);
}

void CSTGControllerInfo::AnalogSliderExtHandler(unsigned int idx, unsigned short a, unsigned short)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *rtdBytes = (unsigned char *)rtd;
	CSTGControllerRTData::CJumpCatch *jc =
		(CSTGControllerRTData::CJumpCatch *)(rtdBytes + 0x6c + idx * 3);

	if (rtdBytes[0x2f] & 2) {
		unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
		unsigned char extSet = g[0x29cc0c8];
		unsigned char cc = *(g + 0x29cbc48 + (unsigned int)extSet * 9 + idx);
		if (cc == 0xff)
			return;
		unsigned char ccValue = CSTGCCInfo::sCCInfoTable[(unsigned int)cc * 10];
		if (ccValue <= 0x7f)
			rtdBytes[0x6c + idx * 3 + 2] = ccValue;
		jc->UpdateStatus();
		SendExtModeSliderEvent((int)idx, ccValue, true);
		return;
	}

	if (jc->CheckPosition((unsigned short)a, true))
		SendExtModeSliderEvent((int)idx, a, true);
}

/* AnalogKnobRTKHandler(idx, a, b) -- `.text+0x99670`, 288 bytes.
 * AnalogSliderRTKHandler(idx, a, b) -- `.text+0x986b0`, 288 bytes.
 * Confirmed real: `a` (param2) unused on BOTH -- the actual knob/slider
 * value read is `b` (param3, the raw ADC reading), converted through
 * the shared `ApplyRTKCurve` bipolar curve above. "RTK" (Real Time
 * Knob) controls are FIXED hardwired Karma CC sources (`index+0x14`,
 * an `eKarmaCCNo`), unlike the reassignable Ext-mode handlers above --
 * no per-set CC lookup here.
 *
 * The KNOB and SLIDER variants are NOT symmetric past the shared curve
 * (confirmed via disassembly, not assumed): Knob RTK's busy2 SET path
 * is a plain `ResetRTKModeKnob(idx)` call with NO curve computation at
 * all (the branch is taken before the curve code even runs); its
 * busy2-CLEAR path gates the curve through `CJumpCatch::CheckPosition`
 * (array base `+0x54`, SAME array `AnalogKnobExtHandler` above uses)
 * then calls `SetRTKModeKnob`. Slider RTK ALWAYS computes the curve
 * first, then on busy2 SET writes it into the per-index array's own
 * `+1` byte (a DIFFERENT field than Ext's own `+2` -- `CJumpCatch`
 * apparently has (at least) two independently-used mutable byte
 * fields), `UpdateStatus()`s, and sends a FIXED center value (0x40) via
 * `SendKarmaCCToKG`, not the just-computed curve; busy2 CLEAR gates the
 * curve through `CheckPosition` (array base `+0x6c`, SAME array
 * `AnalogSliderExtHandler` above uses) and sends the REAL curve value.
 *
 * `SetRTKModeKnob`'s real mangled signature (`...Ettb14eSTGMidiSourceb`,
 * 5 params: u16, u16, bool, eSTGMidiSource, bool) has a GCC IPA-CP
 * function CLONE at this call site (`.clone.11`) that appears to pass
 * only 2 of the 3 trailing args explicitly -- the third is presumably
 * constant-propagated into the clone body itself. This reconstruction
 * calls the FULL original signature with `true`/`1`/`true` for all
 * three trailing slots (matching this project's established "source=1
 * at every observed call site" inference elsewhere) -- the exact third
 * value is NOT independently confirmed, flagged in
 * HARDWARE_REVIEW_LOG.md. `SetRTKModeKnob`/`ResetRTKModeKnob` are
 * themselves deferred externs (own bodies not reconstructed), so this
 * ambiguity has no effect on this project's own compiled behavior. */
void CSTGControllerInfo::AnalogKnobRTKHandler(unsigned int idx, unsigned short, unsigned short b)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *rtdBytes = (unsigned char *)rtd;

	if (rtdBytes[0x2f] & 2) {
		ResetRTKModeKnob((unsigned short)idx);
		return;
	}

	unsigned int curved = ApplyRTKCurve(b);
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	CSTGControllerRTData::CJumpCatch *jc =
		(CSTGControllerRTData::CJumpCatch *)(rtdBytes + 0x54 + idx * 3);
	if (!jc->CheckPosition((unsigned short)curved, g[0x6af] != 0))
		return;
	SetRTKModeKnob((unsigned short)idx, (unsigned short)curved, true, 1, true);
}

void CSTGControllerInfo::AnalogSliderRTKHandler(unsigned int idx, unsigned short, unsigned short b)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *rtdBytes = (unsigned char *)rtd;
	unsigned int curved = ApplyRTKCurve(b);
	CSTGControllerRTData::CJumpCatch *jc =
		(CSTGControllerRTData::CJumpCatch *)(rtdBytes + 0x6c + idx * 3);

	if (rtdBytes[0x2f] & 2) {
		rtdBytes[0x6c + idx * 3 + 1] = (unsigned char)curved;
		jc->UpdateStatus();
		rtd->SendKarmaCCToKG((int)(idx + 0x14), 0x40);
		return;
	}

	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	if (!jc->CheckPosition((unsigned short)curved, g[0x6af] != 0))
		return;
	rtd->SendKarmaCCToKG((int)(idx + 0x14), (unsigned char)curved);
}

/* AnalogRibbonZHandler(a, b) -- `.text+0x97790`, 16 bytes. Confirmed
 * real: literal `ret`, no-op both parameters, matching
 * `AnalogControllerHandler`'s own already-established note that
 * RibbonZ's busy-flag-SET echo slot also has no write. */
void CSTGControllerInfo::AnalogRibbonZHandler(unsigned short, unsigned short) {}

/* AnalogFootSwitchHandler(a, b) -- `.text+0x97830`, 32 bytes. Confirmed
 * real: `this` (self) is discarded entirely -- calls straight through to
 * `CSTGControllerRTData::sInstance->HandleFootSwitchChange(a > 0x3f)`.
 * `b` unused. */
void CSTGControllerInfo::AnalogFootSwitchHandler(unsigned short a, unsigned short)
{
	CSTGControllerRTData::sInstance->HandleFootSwitchChange(a > 0x3f);
}

/* AnalogFootPedalHandler(a, b) -- `.text+0x97850`, 32 bytes. Confirmed
 * real: SAME "self discarded, calls through to rtd" shape as
 * AnalogFootSwitchHandler. `b` unused. */
void CSTGControllerInfo::AnalogFootPedalHandler(unsigned short a, unsigned short)
{
	CSTGControllerRTData::sInstance->HandleFootPedalChange((unsigned char)a);
}

/* AnalogJoystickYHandler(a, b) -- `.text+0x9b360`, 32 bytes. Confirmed
 * real: unlike the two above, `this` (self) IS used here -- tail-calls
 * the sibling `ProcessJoystickY(b)` on `self`. `a` unused. */
void CSTGControllerInfo::AnalogJoystickYHandler(unsigned short, unsigned short b)
{
	ProcessJoystickY(b);
}

/* AnalogValueSliderHandler(a, b) -- `.text+0x97ab0`, 80 bytes. Confirmed
 * real: `b` (param3) entirely unused. */
void CSTGControllerInfo::AnalogValueSliderHandler(unsigned short a, unsigned short)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *rtdBytes = (unsigned char *)rtd;

	if (rtdBytes[0x49] & 1)
		rtd->SendCCToKG(0x12, (unsigned char)a);
	rtd->SendUnsolControl2MessageToUI(0xf, 9, a, 1);
}

/* AnalogDamperHandler(a, b) -- `.text+0x977a0`, 144 bytes. Confirmed
 * real: `a` (param2) entirely unused, only `b` (param3, the pedal
 * position) matters. `CSTGGlobal+0x29c9fbc` gates a polarity inversion
 * (dword, nonzero -> invert) BEFORE the curve lookup; the STGAPI echo
 * write at the end always uses the RAW (non-inverted) `b`. */
void CSTGControllerInfo::AnalogDamperHandler(unsigned short, unsigned short b)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;

	int value = (int)b;
	if (*(unsigned int *)(g + 0x29c9fbc) != 0)
		value = 0x3ff - value;
	int scaled = value + 0x98;
	int clamped = (scaled <= 0x3ff) ? scaled : 0x3ff;
	unsigned char filtered = kDamperFilterTable[(unsigned int)(clamped >> 2) & 0xff];

	if (rtd->PedalFilter()->Filter(filtered)) {
		unsigned char *rtdBytes = (unsigned char *)CSTGControllerRTData::sInstance;
		rtd->SendCCToKG(rtdBytes[0xc], 0x40, filtered);
		*(unsigned short *)((unsigned char *)STGAPIFrontPanelStatus::sInstance + 0x106) =
			(unsigned short)(0x3ff - b);
	}
}

/* AnalogVectorYHandler(a, b) -- `.text+0x97870`, 144 bytes. Confirmed
 * real: `b` (param3) entirely unused, only `a` (param2) matters.
 * `CSTGGlobal+0x6c0`/`+0x6c1` are signed-byte "assignment" fields
 * (negative = unassigned, gates a no-op) -- SAME two fields
 * `AnalogVectorXHandler` reads, just used in the opposite roles between
 * the two handlers' own "direct send" sub-paths. */
void CSTGControllerInfo::AnalogVectorYHandler(unsigned short a, unsigned short)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *rtdBytes = (unsigned char *)rtd;
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;

	if (rtdBytes[0x2f] & 2) {
		/* busy2 SET: recenter BOTH axes' CCs to 0x40 if assigned. */
		signed char assignX = (signed char)g[0x6c0];
		signed char assignY = (signed char)g[0x6c1];
		if (assignX >= 0)
			rtd->SendCCToKG((unsigned char)assignX, 0x40);
		if (assignY >= 0)
			rtd->SendCCToKG((unsigned char)assignY, 0x40);
		return;
	}

	signed char assignY = (signed char)g[0x6c1];
	if (assignY < 0)
		return;
	rtd->SendCCToKG((unsigned char)assignY, (unsigned char)a);
}

/* AnalogVectorXHandler(a, b) -- `.text+0x97900`, 144 bytes. SAME shape
 * as AnalogVectorYHandler, mirrored: the direct-send sub-path uses
 * `CSTGGlobal+0x6c0` instead of `+0x6c1`. */
void CSTGControllerInfo::AnalogVectorXHandler(unsigned short a, unsigned short)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *rtdBytes = (unsigned char *)rtd;
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;

	if (rtdBytes[0x2f] & 2) {
		signed char assignX = (signed char)g[0x6c0];
		signed char assignY = (signed char)g[0x6c1];
		if (assignX >= 0)
			rtd->SendCCToKG((unsigned char)assignX, 0x40);
		if (assignY >= 0)
			rtd->SendCCToKG((unsigned char)assignY, 0x40);
		return;
	}

	signed char assignX = (signed char)g[0x6c0];
	if (assignX < 0)
		return;
	rtd->SendCCToKG((unsigned char)assignX, (unsigned char)a);
}

/* AnalogRibbonXHandler(a, b) -- `.text+0x97990`, 288 bytes. Confirmed
 * real: `b` doubles as a "ribbon touched" flag (`b == 0` means
 * released/untouched) AND (when nonzero) the raw position value.
 * `CSTGControllerRTData+0x14/0x15/0x16` are signed-byte indices into
 * `kControllerLockFlagTable`; `+0x20` is the ribbon's own last-sent
 * value; `+0x2f` bit 0 is a "ribbon active" latch (DISTINCT from the
 * busy/busy2 bits `ButtonPressHandler`/`AnalogControllerHandler`'s own
 * device-8..23 gates use); `CSTGGlobal+0x6ac` is a byte flag that, when
 * set, forces the touched-with-unchanged-value case to re-send anyway.
 * `STGAPIFrontPanelStatus+0x108`/`+0x10a` are written here as a raw
 * position/active-flag pair -- NOT necessarily the same-purpose fields
 * as the `STGAPI_OFF_ANALOG_ECHO_*` constants that happen to share the
 * 0x108 offset (those are written only from `AnalogControllerHandler`'s
 * own SEPARATE busy-flag-SET direct-echo path; this is the non-busy
 * direct-send path, most likely a different logical field at the same
 * numeric offset -- not claimed identical). */
void CSTGControllerInfo::AnalogRibbonXHandler(unsigned short a, unsigned short b)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *rtdBytes = (unsigned char *)rtd;
	unsigned char *stgapi = (unsigned char *)STGAPIFrontPanelStatus::sInstance;

	if (b == 0) {
		stgapi[0x10a] = 0;
		if (!(rtdBytes[0x2f] & 1))
			return;
		unsigned char combined = CSTGControllerRTData::kControllerLockFlagTable[(unsigned char)(signed char)rtdBytes[0x14]] |
					  CSTGControllerRTData::kControllerLockFlagTable[(unsigned char)(signed char)rtdBytes[0x15]] |
					  CSTGControllerRTData::kControllerLockFlagTable[(unsigned char)(signed char)rtdBytes[0x16]];
		if (combined & 8) {
			rtdBytes[0x2f] &= ~1;
			return;
		}
		rtdBytes[0x20] = 0x40;
		rtd->SendCCToKG(0x10, 0x40);
		rtdBytes[0x2f] &= ~1;
		return;
	}

	if (!(rtdBytes[0x2f] & 1)) {
		rtdBytes[0x20] = (unsigned char)a;
		rtd->SendCCToKG(0x10, (unsigned char)a);
		stgapi[0x10a] = 1;
		*(unsigned short *)(stgapi + 0x108) = (unsigned short)(0x3ff - b);
		rtdBytes[0x2f] |= 1;
		return;
	}

	/* +0x2f bit0 already set (touched last time too). */
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	if (g[0x6ac] == 0 && (unsigned short)rtdBytes[0x20] == a) {
		/* unchanged value, no forced re-send: just re-latch. */
		rtdBytes[0x2f] |= 1;
		return;
	}
	rtdBytes[0x20] = (unsigned char)a;
	rtd->SendCCToKG(0x10, (unsigned char)a);
	stgapi[0x10a] = 1;
	*(unsigned short *)(stgapi + 0x108) = (unsigned short)(0x3ff - b);
	rtdBytes[0x2f] |= 1;
}

/* ---------------------------------------------------------------------
 * Local, file-scope extractions of the three DSP-adjacent inlined
 * sub-branches this pass deliberately does not trace (see header
 * comment). These are NOT real OA.ko symbols -- just named stand-ins so
 * the real structural dispatch above them can still be written as
 * plain, readable C++ calls. Left as loud no-op stubs.
 */
static void ApplyValueKnobTempoCurve(unsigned short param3)
{
	(void)param3;
	/* TODO (DSP-adjacent, not reconstructed): real ground truth at
	 * .text+0x9958..0x9c18 computes a float tempo curve from param3
	 * (two shapes selected by CSTGControllerRTData::sInstance[0x48]),
	 * then CJumpCatch::CheckPosition(tempoValue, Global[0x6af]!=0);
	 * if true, CSTGControllerInfo::NotifySetListParam(0xf, tempoValue,
	 * false). See this file's own header comment. */
}

static void ApplyValueKnobEQCurve(unsigned short param2, unsigned short param3)
{
	(void)param2;
	(void)param3;
	/* TODO (DSP-adjacent, not reconstructed): real ground truth at
	 * .text+0x9ac18..0x9ad10 -- a float EQ curve from param3 feeding
	 * CSTGControllerRTData::ConvertKnobToControl()/
	 * CSTGFrontPanelSmoothers::SetFPEQSmoother()/
	 * CSTGControllerRTData::JumpCatch()->CheckPosition(), covering
	 * BOTH the busy and non-busy sub-paths (the busy path reuses the
	 * curve-derived value, not raw param2). See this file's own
	 * header comment. */
}

static void ApplyValueKnobEffectRackEdit(int position)
{
	(void)position;
	/* TODO (DSP-adjacent, not reconstructed): real ground truth at
	 * .text+0x9a7d6..0x9a839 -- constructs a
	 * CSTGFrontPanelSmoothers::EffectRackSmootherOutputArguments,
	 * calls SetFPISmoother()/EffectRackParamSmootherOutput(), then
	 * CSTGControllerInfo::NotifyEffectSlotParam(). Only reached after
	 * this file's own real CheckPosition() gate returns true. See
	 * this file's own header comment. */
}

/* Shared "Value knob busy-notify" idiom -- confirmed real, identical
 * shape at THREE real call sites (device-0x18's own outer [0x2f]&8
 * gate, mode==6/Tempo's [0x2f]&2 busy sub-path, and the default-mode
 * [0x2f]&2 busy sub-path): store the raw value into
 * CSTGControllerRTData::sInstance+0x85, update the embedded JumpCatch,
 * and send a UI-only notify (msgType 0xf, id 8). */
static void SendValueKnobBusyNotify(unsigned char *rtdBytes, unsigned short value)
{
	rtdBytes[0x85] = (unsigned char)value;
	CSTGControllerRTData::JumpCatch()->UpdateStatus();
	CSTGControllerRTData::sInstance->SendUnsolControl2MessageToUI(0xf, 8, value, 1);
}

/* device 0x18 ("Value") mode dispatch -- see header comment. */
static void HandleValueKnobDevice(CSTGControllerInfo *self, unsigned short param2, unsigned short param3)
{
	unsigned char *rtdBytes = (unsigned char *)CSTGControllerRTData::sInstance;

	if (rtdBytes[0x2f] & 8) {
		CSTGControllerRTData::sInstance->SendUnsolControl2MessageToUI(0xf, 8, param2, 1);
		return;
	}

	signed char mode = (signed char)rtdBytes[0x2b];
	bool busy2 = (rtdBytes[0x2f] & 2) != 0;

	if (mode == 4) {
		if (busy2) {
			unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
			unsigned int assignMode = *(unsigned int *)(g + 0x29cc0c8);
			unsigned char cc = *(unsigned char *)(g + assignMode * 9 + 0x29cbc50);
			if (cc == 0xff)
				return;
			unsigned char ccValue = CSTGCCInfo::sCCInfoTable[(unsigned int)cc * 10];
			if (ccValue <= 0x7f)
				rtdBytes[0x86] = ccValue;
			CSTGControllerRTData::JumpCatch()->UpdateStatus();
			self->SendExtModeSliderEvent(8, ccValue, true);
			return;
		}
		if (!CSTGControllerRTData::JumpCatch()->CheckPosition(param2, true))
			return;
		self->SendExtModeSliderEvent(8, param2, true);
		return;
	}

	if (mode == 6) {
		if (busy2) {
			SendValueKnobBusyNotify(rtdBytes, param2);
			return;
		}
		ApplyValueKnobTempoCurve(param3);
		return;
	}

	if (mode == 8) {
		ApplyValueKnobEQCurve(param2, param3);
		return;
	}

	/* default (any other mode) */
	if (busy2) {
		SendValueKnobBusyNotify(rtdBytes, param2);
		return;
	}
	unsigned char globalFlag = ((unsigned char *)CSTGGlobal::sInstance)[0x6af];
	if (!CSTGControllerRTData::JumpCatch()->CheckPosition(param2, globalFlag != 0))
		return;
	ApplyValueKnobEffectRackEdit(param2);
}

void CSTGControllerInfo::AnalogControllerHandler(unsigned int deviceCode, unsigned short param2, unsigned short param3)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *rtdBytes = (unsigned char *)rtd;

	if (deviceCode - 8 <= 7) {
		/* Knobs (8-15) */
		unsigned int knobIndex = deviceCode - 8;
		if (rtdBytes[0x2f] & 8) {
			rtd->SendUnsolControl2MessageToUI(0xe, (int)knobIndex, param2, 1);
			return;
		}
		if (*(unsigned int *)((unsigned char *)CSTGGlobal::sInstance + 0x29cc4dc) != 0) {
			if (HandleEditInContextKnob(deviceCode, param2, param3))
				return;
		}
		signed char mode = (signed char)rtdBytes[0x2b];
		switch (mode) {
		case 0: AnalogKnobT18Handler(knobIndex, param2, param3); break;
		case 1: AnalogKnobT916Handler(knobIndex, param2, param3); break;
		case 2: AnalogKnobA18Handler(knobIndex, param2, param3); break;
		case 3: AnalogKnobA916Handler(knobIndex, param2, param3); break;
		case 4: AnalogKnobExtHandler(knobIndex, param2, param3); break;
		case 5: AnalogKnobRTKHandler(knobIndex, param2, param3); break;
		case 6: AnalogKnobTAHandler(knobIndex, param2, param3); break;
		case 7: AnalogKnobAInHandler(knobIndex, param2, param3); break;
		case 8: AnalogKnobSetListEQHandler(knobIndex, param2, param3); break;
		default: break;
		}
		return;
	}

	if (deviceCode - 16 <= 7) {
		/* Sliders (16-23) */
		unsigned int sliderIndex = deviceCode - 16;
		if (rtdBytes[0x2f] & 8) {
			rtd->SendUnsolControl2MessageToUI(0xf, (int)sliderIndex, param2, 1);
			return;
		}
		if (*(unsigned int *)((unsigned char *)CSTGGlobal::sInstance + 0x29cc4dc) != 0) {
			if (HandleEditInContextSlider(deviceCode, param2, param3))
				return;
		}
		signed char mode = (signed char)rtdBytes[0x2b];
		switch (mode) {
		case 0: AnalogSliderT18Handler(sliderIndex, param2, param3); break;
		case 1: AnalogSliderT916Handler(sliderIndex, param2, param3); break;
		case 2: AnalogSliderA18Handler(sliderIndex, param2, param3); break;
		case 3: AnalogSliderA916Handler(sliderIndex, param2, param3); break;
		case 4: AnalogSliderExtHandler(sliderIndex, param2, param3); break;
		case 5: AnalogSliderRTKHandler(sliderIndex, param2, param3); break;
		case 6: AnalogSliderTAHandler(sliderIndex, param2, param3); break;
		case 7: AnalogSliderAInHandler(sliderIndex, param2, param3); break;
		case 8: AnalogSliderSetListEQHandler(sliderIndex, param2, param3); break;
		default: break;
		}
		return;
	}

	if (deviceCode == 0x18) {
		HandleValueKnobDevice(this, param2, param3);
		return;
	}

	if (deviceCode - 1 <= 6) {
		/* Fixed one-to-one controllers 1-7 */
		if (rtdBytes[0x2f] & 8) {
			switch (deviceCode) {
			case 1: *(unsigned short *)(STGAPIFrontPanelStatus::sInstance + STGAPI_OFF_ANALOG_ECHO_JOYX) = param3; break;
			case 2: *(unsigned short *)(STGAPIFrontPanelStatus::sInstance + STGAPI_OFF_ANALOG_ECHO_JOYY) = param3; break;
			case 3: *(unsigned short *)(STGAPIFrontPanelStatus::sInstance + STGAPI_OFF_ANALOG_ECHO_RIBX) = param3; break;
			case 4: /* RibbonZ: no echo write, confirmed real */ break;
			case 5: *(unsigned short *)(STGAPIFrontPanelStatus::sInstance + STGAPI_OFF_ANALOG_ECHO_VECX) = param3; break;
			case 6: *(unsigned short *)(STGAPIFrontPanelStatus::sInstance + STGAPI_OFF_ANALOG_ECHO_VECY) = param3; break;
			case 7: *(unsigned short *)(STGAPIFrontPanelStatus::sInstance + STGAPI_OFF_ANALOG_ECHO_ATOUCH) = param3; break;
			default: break;
			}
			return;
		}
		switch (deviceCode) {
		case 1: AnalogJoystickXHandler(param2, param3); break;
		case 2: AnalogJoystickYHandler(param2, param3); break;
		case 3: AnalogRibbonXHandler(param2, param3); break;
		case 4: AnalogRibbonZHandler(param2, param3); break;
		case 5: AnalogVectorXHandler(param2, param3); break;
		case 6: AnalogVectorYHandler(param2, param3); break;
		case 7: AnalogAftertouchHandler(param2, param3); break;
		default: break;
		}
		return;
	}

	if (deviceCode - 0x19 <= 4) {
		/* Fixed one-to-one controllers 0x19-0x1D */
		if (rtdBytes[0x2f] & 8) {
			switch (deviceCode) {
			case 0x1a:
				rtd->SendUnsolControl2MessageToUI(0xc, 0, param3, 1);
				return;
			case 0x1b:
				rtd->SendUnsolControl2MessageToUI(0x15, 0, param3, 1);
				return;
			case 0x1c:
				rtd->SendUnsolControl2MessageToUI(0x15, 1, param3, 1);
				return;
			case 0x1d:
				*(unsigned short *)(STGAPIFrontPanelStatus::sInstance + STGAPI_OFF_ANALOG_ECHO_DAMPER) =
					(unsigned short)(0x3ff - param3);
				return;
			default: /* 0x19 */
				rtd->SendUnsolControl2MessageToUI(0xf, 9, param2, 1);
				return;
			}
		}
		switch (deviceCode) {
		case 0x19: AnalogValueSliderHandler(param2, param3); break;
		case 0x1a: AnalogTempoHandler(param2, param3); break;
		case 0x1b: AnalogFootPedalHandler(param2, param3); break;
		case 0x1c: AnalogFootSwitchHandler(param2, param3); break;
		case 0x1d: AnalogDamperHandler(param2, param3); break;
		default: break;
		}
		return;
	}

	if (deviceCode == 0x1e) {
		rtdBytes[5] = (param2 != 0);
		signed char newValue = (param2 != 0) ? (signed char)rtdBytes[8] : 0;
		rtd->SetControllerAssignment(rtdBytes + 3, newValue, true);
		return;
	}

	/* out of range: silent no-op, confirmed real */
}
