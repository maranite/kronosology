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
