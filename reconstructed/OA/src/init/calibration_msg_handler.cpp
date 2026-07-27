// SPDX-License-Identifier: GPL-2.0
/*
 * calibration_msg_handler.cpp  -  CSTGCalibrationMsgHandler, the front-
 * panel/keybed analog-controller calibration state machine.
 *
 * See include/oa_calibration.h for the full ground-truth provenance,
 * message-shape derivation, and the "install vs dispatch" vtable note.
 *
 * `sCalibrationOp` values observed in ground truth (real `.data+0x2ff8`):
 *   0x0  JSX in progress (keybed-hw path)      0x3  JSY in progress (keybed-hw path)
 *   0x4  JSX/JSY end awaiting keybed-hw ack     0x5  JSX/JSY cancel awaiting keybed-hw ack
 *   0x6  Vector in progress (touch-panel only)  0x8  TouchScreen in progress (unused by ProcessCalibration)
 *   0xa  RibbonX in progress                    0xb  RibbonX end awaiting keybed-hw ack
 *   0xc  RibbonX cancel awaiting keybed-hw ack   0xd  HalfDamper in progress
 *   0xf  Aftertouch in progress                 0x10 Aftertouch end awaiting keybed-hw ack
 *   0x11 Aftertouch cancel awaiting keybed-hw ack 0x12 idle/terminal (reply already sent)
 * States 0x1/0x2 are real (HandleKeybedCalibrationResult's own real
 * `.rodata` jump table at `.rodata+0x4b31c` has distinct entries for
 * them, confirmed via raw disassembly), but no setter for either value
 * appears anywhere in this cluster -- plausibly set by CSTGKeybedInterface's
 * own not-yet-reconstructed receive-side ack handling before it calls
 * HandleKeybedCalibrationResult. Modeled faithfully (the jump-table
 * behavior for 1/2 is fully reconstructed below) without claiming to
 * know their setter.
 */

#include "oa_calibration.h"
#include "oa_global.h"
#include "oa_setup_global_resources.h"
#include "oa_keybed_init.h"

extern "C" void PushUnsolicitedMessage(void *msg);

static unsigned char *FromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}

CSTGCalibrationMsgHandler *CSTGCalibrationMsgHandler::sInstance;
STGCalibrationMsgHandlerEntry CSTGCalibrationMsgHandler::sMsgHandler[18];

static int sCalibrationOp;             /* .data+0x2ff8 */
static int sRawXMin;                   /* .data+0x2ffc */
static int sRawYMin;                   /* .data+0x3000 */
static int sRawXMax;                   /* .bss+0x9e718 */
static int sRawYMax;                   /* .bss+0x9e71c */
static unsigned char sDamperCalibrator;/* half-damper polarity auto-detect state */
static short sDamperLastRawValue;      /* last raw sample seen while auto-detecting */
static int sDamperLastTimestamp;       /* GetSTGTickCount() at that sample */

/* ------------------------------------------------------------------ */
/* Reply/message helpers                                              */
/* ------------------------------------------------------------------ */

static void SendReply(int result)
{
	STGCalibrationReplyMsg msg;
	msg.size = 0xc;
	msg._pad2 = 0;
	msg.echoTag = 0xc;
	msg.result = result;
	sCalibrationOp = 0x12;
	PushMessage(&msg);
}

static void SendUnsolCalibrationMsg(unsigned int deviceCode, unsigned int value,
				     unsigned int scanCode)
{
	STGCalibrationUnsolMsg msg;
	msg.size = 0x18;
	msg.source = 1;
	msg.reserved = 0;
	msg.subtype = 0x12;
	msg.deviceCode = deviceCode;
	msg.value = value;
	msg.scanCode = scanCode;
	PushUnsolicitedMessage(&msg);
}

/* CSTGPerformanceVarsManager::sInstance[8]-selector resolve, same raw
 * pattern as CSTGGlobal::ResolveActivePerformanceVarsManager() (global.cpp,
 * sec 10.71) -- duplicated here rather than shared since that helper is
 * `private` to CSTGGlobal and this cluster is its own TU. Ghidra's own
 * decompile mislabeled the selector as a free-standing `DAT_007901a4`
 * global; the raw disassembly (relocation on the read instruction's own
 * displacement) confirms it is actually `sInstance+8`, matching the
 * already-established real layout exactly -- not a second, independent
 * global. */
static bool IsActivePerfVarsInFocus()
{
	unsigned char *slots = CSTGPerformanceVarsManager::sInstance;
	unsigned char selector = slots[8];
	unsigned int mgrAddr = *(unsigned int *)(slots + (unsigned int)selector * 4);
	unsigned char *mgr = FromU32(mgrAddr);
	return mgr[0x23d1] == 2;
}

static bool IsNKS4TestMode()
{
	return *((unsigned char *)CSTGGlobal::sInstance + 0x6ac) != 0;
}

/* Real quirk, confirmed at every Start* keybed-hardware-path call site:
 * "hardware present" is NOT `IsNKS4TestMode()` itself, but its negation
 * gated behind the raw touch-panel-mode byte at STGAPIFrontPanelStatus+
 * 0x29124==3 (checked first, independently, in every Start/End/Cancel
 * function -- NOT the same flag as CSTGGlobal+0x6ac). Kept as two
 * separate real checks rather than folded into one, matching ground
 * truth exactly. */
static bool IsTouchPanelOnlyMode()
{
	return *((unsigned char *)STGAPIFrontPanelStatus::sInstance + 0x29124) == 3;
}

/* ------------------------------------------------------------------ */
/* Constructor -- installs the vtable pointer (unused, see header),   */
/* the msgHandler table pointer, default reply tag, and the 18 real   */
/* dispatch entries.                                                   */
/* ------------------------------------------------------------------ */

CSTGCalibrationMsgHandler::CSTGCalibrationMsgHandler()
{
	_vtablePtr = 0; /* real vtable never dispatched through in this project, see header */
	_msgHandlerTable = &sMsgHandler;
	_replyTag = 0x12;
	sInstance = this;

	sMsgHandler[0]  = { StartJSXCalibration, 0 };
	sMsgHandler[1]  = { EndJSXCalibration, 0 };
	sMsgHandler[2]  = { CancelJSXCalibration, 0 };
	sMsgHandler[3]  = { StartJSYCalibration, 0 };
	sMsgHandler[4]  = { EndJSYCalibration, 0 };
	sMsgHandler[5]  = { CancelJSYCalibration, 0 };
	sMsgHandler[6]  = { StartVectorCalibration, 0 };
	sMsgHandler[7]  = { EndVectorCalibration, 0 };
	sMsgHandler[8]  = { StartTouchScreenCalibration, 0 };
	sMsgHandler[9]  = { EndTouchScreenCalibration, 0 };
	sMsgHandler[10] = { StartRibbonXCalibration, 0 };
	sMsgHandler[11] = { EndRibbonXCalibration, 0 };
	sMsgHandler[12] = { CancelRibbonXCalibration, 0 };
	sMsgHandler[13] = { StartHalfDamperCalibration, 0 };
	sMsgHandler[14] = { EndHalfDamperCalibration, 0 };
	sMsgHandler[15] = { StartAftertouchCalibration, 0 };
	sMsgHandler[16] = { EndAftertouchCalibration, 0 };
	sMsgHandler[17] = { CancelAftertouchCalibration, 0 };
}

/* ------------------------------------------------------------------ */
/* Joystick X (keybed controller id 5)                                */
/* ------------------------------------------------------------------ */

void CSTGCalibrationMsgHandler::StartJSXCalibration()
{
	if (!IsTouchPanelOnlyMode()) {
		sCalibrationOp = 0;
		CSTGKeybedInterface_StartCalibration(5);
		return;
	}
	sRawYMin = 0x3ff;
	sRawXMin = 0x3ff;
	sRawYMax = 0;
	sRawXMax = 0;
	sCalibrationOp = 0;
}

void CSTGCalibrationMsgHandler::EndJSXCalibration()
{
	if (sCalibrationOp != 0) {
		SendReply(-1);
		return;
	}
	if (!IsTouchPanelOnlyMode()) {
		sCalibrationOp = 4;
		CSTGKeybedInterface_EndCalibration();
		return;
	}
	unsigned char *panel = STGAPIFrontPanelStatus::sInstance;
	short xMin = (short)sRawXMin + 0x28;
	short xMax = (short)sRawXMax - 0x28;
	int mid = (sRawXMin + sRawXMax);
	mid = (short)((unsigned int)(mid - (mid >> 31)) >> 1);
	short lo = (short)mid - 0x20;
	short hi = (short)mid + 0x20;
	*(short *)(panel + 0x20) = xMin;
	*(short *)(panel + 0x26) = xMax;
	*(short *)(panel + 0x22) = lo;
	*(short *)(panel + 0x24) = hi;
	*(float *)(panel + 0x28) = 1.0f / (float)(lo - xMin);
	*(float *)(panel + 0x2c) = 1.0f / (float)(xMax - hi);
	SendReply(0);
}

void CSTGCalibrationMsgHandler::CancelJSXCalibration()
{
	if (sCalibrationOp != 0) {
		SendReply(-1);
		return;
	}
	if (!IsTouchPanelOnlyMode()) {
		sCalibrationOp = 5;
		CSTGKeybedInterface_CancelCalibration();
		return;
	}
	SendReply(0);
}

/* ------------------------------------------------------------------ */
/* Joystick Y (keybed controller id 7)                                */
/* ------------------------------------------------------------------ */

void CSTGCalibrationMsgHandler::StartJSYCalibration()
{
	if (!IsTouchPanelOnlyMode()) {
		sCalibrationOp = 3;
		CSTGKeybedInterface_StartCalibration(7);
		return;
	}
	sRawYMin = 0x3ff;
	sRawXMin = 0x3ff;
	sRawYMax = 0;
	sRawXMax = 0;
	sCalibrationOp = 3;
}

void CSTGCalibrationMsgHandler::EndJSYCalibration()
{
	if (sCalibrationOp != 3) {
		SendReply(-1);
		return;
	}
	if (!IsTouchPanelOnlyMode()) {
		sCalibrationOp = 4;
		CSTGKeybedInterface_EndCalibration();
		return;
	}
	unsigned char *panel = STGAPIFrontPanelStatus::sInstance;
	short yMin = (short)sRawYMin + 0x28;
	short yMax = (short)sRawYMax - 0x28;
	int mid = (sRawYMin + sRawYMax);
	mid = (short)((unsigned int)(mid - (mid >> 31)) >> 1);
	short lo = (short)mid - 0x28;
	short hi = (short)mid + 0x28;
	*(short *)(panel + 0x34) = yMin;
	*(short *)(panel + 0x3a) = yMax;
	*(short *)(panel + 0x36) = lo;
	*(short *)(panel + 0x38) = hi;
	*(float *)(panel + 0x3c) = 1.0f / (float)(lo - yMin);
	*(float *)(panel + 0x40) = 1.0f / (float)(yMax - hi);
	SendReply(0);
}

void CSTGCalibrationMsgHandler::CancelJSYCalibration()
{
	if (sCalibrationOp != 3) {
		SendReply(-1);
		return;
	}
	if (!IsTouchPanelOnlyMode()) {
		sCalibrationOp = 5;
		CSTGKeybedInterface_CancelCalibration();
		return;
	}
	SendReply(0);
}

/* ------------------------------------------------------------------ */
/* Vector joystick -- touch-panel only, no keybed-hardware variant     */
/* ------------------------------------------------------------------ */

void CSTGCalibrationMsgHandler::StartVectorCalibration()
{
	sRawYMin = 0x3ff;
	sRawXMin = 0x3ff;
	sRawYMax = 0;
	sRawXMax = 0;
	sCalibrationOp = 6;
}

void CSTGCalibrationMsgHandler::EndVectorCalibration()
{
	unsigned char *panel = STGAPIFrontPanelStatus::sInstance;
	if (sCalibrationOp == 6) {
		short xMinAdj = (short)sRawXMin + 0x28;
		short xMaxAdj = (short)sRawXMax - 0x28;
		short yMinAdj = (short)sRawYMin + 0x28;
		short yMaxAdj = (short)sRawYMax - 0x28;

		int midX = sRawXMax + sRawXMin;
		short midXs = (short)((unsigned int)(midX - (midX >> 31)) >> 1);
		short loX = midXs - 0x50;
		short hiX = midXs + 0x50;

		int midY = sRawYMax + sRawYMin;
		short midYs = (short)((unsigned int)(midY - (midY >> 31)) >> 1);
		short loY = midYs - 0x50;
		short hiY = midYs + 0x50;

		*(short *)(panel + 0x5c) = xMinAdj;
		*(short *)(panel + 0x62) = xMaxAdj;
		*(short *)(panel + 0x70) = yMinAdj;
		*(short *)(panel + 0x76) = yMaxAdj;
		*(short *)(panel + 0x5e) = loX;
		*(short *)(panel + 0x60) = hiX;
		*(short *)(panel + 0x72) = loY;
		*(short *)(panel + 0x74) = hiY;
		*(float *)(panel + 0x64) = 1.0f / (float)(loX - xMinAdj);
		*(float *)(panel + 0x68) = 1.0f / (float)(xMaxAdj - hiX);
		*(float *)(panel + 0x78) = 1.0f / (float)(loY - yMinAdj);
		*(float *)(panel + 0x7c) = 1.0f / (float)(yMaxAdj - hiY);
		SendReply(0);
		return;
	}
	SendReply(-1);
}

/* ------------------------------------------------------------------ */
/* Touch screen -- no sCalibrationOp gating at all (real quirk)        */
/* ------------------------------------------------------------------ */

void CSTGCalibrationMsgHandler::StartTouchScreenCalibration()
{
	sRawYMin = 0x3ff;
	sRawXMin = 0x3ff;
	sRawYMax = 0;
	sRawXMax = 0;
	sCalibrationOp = 8;
}

void CSTGCalibrationMsgHandler::EndTouchScreenCalibration()
{
	sCalibrationOp = 0x12;
	SendReply(-1);
}

/* ------------------------------------------------------------------ */
/* Ribbon controller X (keybed controller id 8)                        */
/* ------------------------------------------------------------------ */

void CSTGCalibrationMsgHandler::StartRibbonXCalibration()
{
	if (IsTouchPanelOnlyMode()) {
		sRawYMin = 0x3ff;
		sRawXMin = 0x3ff;
		sRawYMax = 0;
		sRawXMax = 0;
		sCalibrationOp = 10;
		return;
	}
	sCalibrationOp = 10;
	CSTGKeybedInterface_StartCalibration(8);
}

void CSTGCalibrationMsgHandler::EndRibbonXCalibration()
{
	unsigned char *panel = STGAPIFrontPanelStatus::sInstance;
	int xMinRaw = sRawXMin;

	if (sCalibrationOp != 10) {
		SendReply(-1);
		return;
	}
	if (IsTouchPanelOnlyMode()) {
		if (sRawXMax > 0x3a7)
			sRawXMax = 0x3a7;
		int xMaxRaw = sRawXMax;
		*(short *)(panel + 0x4e) = (short)sRawXMax;
		*(short *)(panel + 0x48) = (short)xMinRaw;
		sCalibrationOp = 0x12;
		int mid = xMaxRaw + xMinRaw;
		short mids = (short)((unsigned int)(mid - (mid >> 31)) >> 1);
		short lo = mids - 0x1e;
		short hi = mids + 0x1e;
		*(short *)(panel + 0x4a) = lo;
		*(short *)(panel + 0x4c) = hi;
		*(float *)(panel + 0x50) = 1.0f / (float)(lo - xMinRaw);
		*(float *)(panel + 0x54) = 1.0f / (float)(xMaxRaw - hi);
		SendReply(0);
		return;
	}
	sCalibrationOp = 0xb;
	CSTGKeybedInterface_EndCalibration();
}

void CSTGCalibrationMsgHandler::CancelRibbonXCalibration()
{
	if (sCalibrationOp != 10) {
		SendReply(-1);
		return;
	}
	if (!IsTouchPanelOnlyMode()) {
		sCalibrationOp = 0xc;
		CSTGKeybedInterface_CancelCalibration();
		return;
	}
	SendReply(0);
}

/* ------------------------------------------------------------------ */
/* Half-damper pedal -- polarity auto-detect, no keybed-hw variant     */
/* ------------------------------------------------------------------ */

void CSTGCalibrationMsgHandler::StartHalfDamperCalibration()
{
	sCalibrationOp = 0xd;
	sRawYMin = 0x3ff;
	sRawXMin = 0x3ff;
	sRawYMax = 0;
	sRawXMax = 0;
	sDamperCalibrator = 0;
}

void CSTGCalibrationMsgHandler::EndHalfDamperCalibration()
{
	unsigned char *panel = STGAPIFrontPanelStatus::sInstance;

	if (sCalibrationOp != 0xd) {
		SendReply(-1);
		return;
	}

	if (sDamperCalibrator != 0 &&
	    (unsigned int)(GetSTGTickCount() - sDamperLastTimestamp) > 0x1d) {
		int lo = sRawXMin, hi = sRawXMax;
		int polarity = *(int *)((unsigned char *)CSTGGlobal::sInstance + 0x29c9fbc);
		if (polarity == 0) {
			hi = sDamperLastRawValue;
			if (sDamperLastRawValue < sRawXMax)
				hi = sRawXMax;
		} else {
			lo = sDamperLastRawValue;
			if (sRawXMin < sDamperLastRawValue)
				lo = sRawXMin;
		}
		sRawXMax = hi;
		sRawXMin = lo;
		sDamperCalibrator = 0;
	}

	short xMax = (short)sRawXMax - 0x32;
	short xMin = (short)sRawXMin + 0x32;
	int mid = sRawXMin + sRawXMax;
	short mids = (short)((unsigned int)(mid - (mid >> 31)) >> 1);
	short lo = mids - 1;
	short hi = mids + 1;
	*(short *)(panel + 0x84) = xMin;
	*(short *)(panel + 0x8a) = xMax;
	*(short *)(panel + 0x86) = lo;
	*(short *)(panel + 0x88) = hi;
	*(float *)(panel + 0x8c) = 1.0f / (float)(lo - xMin);
	*(float *)(panel + 0x90) = 1.0f / (float)(xMax - hi);
	ResetDamper();
	SendReply(0);
}

/* ------------------------------------------------------------------ */
/* Aftertouch (keybed controller id 9)                                 */
/* ------------------------------------------------------------------ */

void CSTGCalibrationMsgHandler::StartAftertouchCalibration()
{
	if (!IsTouchPanelOnlyMode()) {
		sCalibrationOp = 0xf;
		CSTGKeybedInterface_StartCalibration(9);
		return;
	}
	sRawYMin = 0x3ff;
	sRawXMin = 0x3ff;
	sRawYMax = 0;
	sRawXMax = 0;
	sCalibrationOp = 0xf;
}

void CSTGCalibrationMsgHandler::EndAftertouchCalibration()
{
	unsigned char *panel = STGAPIFrontPanelStatus::sInstance;

	if (sCalibrationOp != 0xf) {
		SendReply(-1);
		return;
	}
	if (!IsTouchPanelOnlyMode()) {
		sCalibrationOp = 0x10;
		CSTGKeybedInterface_EndCalibration();
		return;
	}

	/* Temporarily force the aftertouch-curve table's own live cache to
	 * "uncalibrated" (0xffff) while re-applying it against the freshly
	 * calibrated raw range, then restore -- matches ground truth
	 * exactly (save/restore of panel+0xf8). */
	unsigned short saved = *(unsigned short *)(panel + 0xf8);
	*(unsigned short *)(panel + 0xf8) = 0xffff;
	short applied = CSTGKeybedInterface_ApplyCalibrationAndAfterTouchTable((short)sRawXMax);
	*(unsigned short *)(panel + 0xf8) = saved;

	bool outOfRange = (*(panel + 0x108a) == 0) ? (applied > 0x116) : (applied > 0x15b);

	short xMax = (short)sRawXMax - 0x14;
	short xMin = (short)sRawXMin + 0x14;
	int mid = sRawXMax + sRawXMin;
	short mids = (short)((unsigned int)(mid - (mid >> 31)) >> 1);
	short lo = mids + 1;
	short hi = mids - 1;
	*(short *)(panel + 0xe8) = xMin;
	*(short *)(panel + 0xee) = xMax;
	*(short *)(panel + 0xea) = hi;
	*(short *)(panel + 0xec) = lo;
	*(float *)(panel + 0xf0) = 1.0f / (float)(hi - xMin);
	*(float *)(panel + 0xf4) = 1.0f / (float)(xMax - lo);

	if (IsNKS4TestMode())
		SendUnsolCalibrationMsg(7, 0x3ff, 0);
	else if (IsActivePerfVarsInFocus())
		CSTGFrontPanel::sInstance->HandleAnalogController(7, 0, 0x3ff);

	SendReply(outOfRange ? -1 : 0);
}

void CSTGCalibrationMsgHandler::CancelAftertouchCalibration()
{
	if (sCalibrationOp != 0xf) {
		SendReply(-1);
		return;
	}
	if (!IsTouchPanelOnlyMode()) {
		sCalibrationOp = 0x11;
		CSTGKeybedInterface_CancelCalibration();
		return;
	}
	SendReply(0);
}

/* ------------------------------------------------------------------ */
/* Analog-sample feed (regparm(2) real worker: EDX=deviceCode, CX=raw) */
/* ------------------------------------------------------------------ */

void CSTGCalibrationMsgHandler::ProcessCalibration(int deviceCode, short rawValue)
{
	switch (sCalibrationOp) {
	case 0: /* JSX */
		if (deviceCode != 1)
			return;
		break;
	case 3: /* JSY */
		if (deviceCode != 2)
			return;
		if (rawValue <= sRawYMin) sRawYMin = rawValue;
		if (sRawYMax <= rawValue) sRawYMax = rawValue;
		return;
	case 6: /* Vector */
		if (deviceCode == 5) {
			break;
		} else if (deviceCode == 6) {
			if (rawValue <= sRawYMin) sRawYMin = rawValue;
			if (sRawYMax <= rawValue) sRawYMax = rawValue;
			return;
		} else {
			return;
		}
	case 10: /* RibbonX */
		if (deviceCode != 3)
			return;
		if (rawValue <= sRawXMin) sRawXMin = rawValue;
		if (rawValue > 0x3a7)
			return;
		if (sRawXMax <= rawValue) sRawXMax = rawValue;
		return;
	case 0xd: { /* HalfDamper -- polarity auto-detect */
		int now = GetSTGTickCount();
		int polarity;
		if (sDamperCalibrator == 0) {
			polarity = *(int *)((unsigned char *)CSTGGlobal::sInstance + 0x29c9fbc);
		} else if ((unsigned int)(now - sDamperLastTimestamp) < 0x1e) {
			polarity = *(int *)((unsigned char *)CSTGGlobal::sInstance + 0x29c9fbc);
		} else {
			polarity = *(int *)((unsigned char *)CSTGGlobal::sInstance + 0x29c9fbc);
			if (polarity == 0) {
				if (sRawXMax <= sDamperLastRawValue)
					sRawXMax = sDamperLastRawValue;
			} else if (sDamperLastRawValue <= sRawXMin) {
				sRawXMin = sDamperLastRawValue;
			}
		}
		sDamperCalibrator = 1;
		sDamperLastRawValue = rawValue;
		sDamperLastTimestamp = now;
		if (polarity == 0) {
			if (rawValue <= sRawXMin)
				sRawXMin = rawValue;
			return;
		}
		if (sRawXMax <= rawValue)
			sRawXMax = rawValue;
		return;
	}
	case 0xf: /* Aftertouch */
		if (deviceCode != 7)
			return;
		break;
	default:
		return; /* no-op: not currently sampling anything */
	}

	/* Common X-only running min/max tail (JSX/Vector-touch/Aftertouch) */
	if (rawValue <= sRawXMin) sRawXMin = rawValue;
	if (rawValue >= sRawXMax) sRawXMax = rawValue;
}

extern "C" void CSTGCalibrationMsgHandler_ProcessCalibration(int deviceCode, short rawValue)
{
	CSTGCalibrationMsgHandler::ProcessCalibration(deviceCode, rawValue);
}

/* ------------------------------------------------------------------ */
/* SimpleReply / ResetController / ResetDamper / HandleKeybedCalibrationResult */
/* ------------------------------------------------------------------ */

void CSTGCalibrationMsgHandler::SimpleReply(int result)
{
	SendReply(result);
}

/*
 * Real quirk, confirmed via raw disassembly: `scanCode` (arg2,
 * eSTGNKS4AnalogScanCode, arrives in ECX per regparm(3)) is loaded then
 * IMMEDIATELY overwritten -- ECX is reloaded from `param3` (the real
 * stack arg3) before either branch reads it. `scanCode` is therefore
 * genuinely dead in ground truth: both the HandleAnalogController call
 * and the outgoing message's `scanCode` field actually use `param3`,
 * never the formal `scanCode` argument. Reproduced faithfully (the
 * dead parameter is kept in the signature for ABI-documentation
 * purposes but is provably never read).
 */
void CSTGCalibrationMsgHandler::ResetController(unsigned int deviceCode, unsigned int scanCode,
						 unsigned char param3, unsigned short param4)
{
	(void)scanCode; /* confirmed dead in ground truth, see comment above */
	if (!IsNKS4TestMode()) {
		if (IsActivePerfVarsInFocus())
			CSTGFrontPanel::sInstance->HandleAnalogController(deviceCode, param3, param4);
		return;
	}
	SendUnsolCalibrationMsg(deviceCode, param4, param3);
}

void CSTGCalibrationMsgHandler::ResetDamper()
{
	int polarity = *(int *)((unsigned char *)CSTGGlobal::sInstance + 0x29c9fbc);

	if (polarity == 0) {
		if (IsNKS4TestMode()) {
			SendUnsolCalibrationMsg(0x1d, 0, 0x7f);
			return;
		}
		if (IsActivePerfVarsInFocus())
			CSTGFrontPanel::sInstance->HandleAnalogController(0x1d, 0x7f, 0);
		return;
	}

	if (IsNKS4TestMode()) {
		SendUnsolCalibrationMsg(0x1d, 0x3ff, 0);
		return;
	}
	if (IsActivePerfVarsInFocus())
		CSTGFrontPanel::sInstance->HandleAnalogController(0x1d, 0, 0x3ff);
}

/*
 * HandleKeybedCalibrationResult(bool) -- confirmed via raw disassembly
 * of the real 18-entry `.rodata+0x4b31c` jump table (indices 0..0x11,
 * anything above 0x11 unsigned is a plain no-op return -- matches
 * `sCalibrationOp` never legitimately exceeding 0x12, and even 0x12
 * itself lands on the no-op entry):
 *   {1,4,0xb}   -> jump target 0xded50: falls straight into the shared
 *                  tail (0xded02) with `esi` (the real `success` param,
 *                  untouched) still live -> reply is `success ? 0 : -1`.
 *   {2,5,0xc}   -> jump target 0xded38: explicitly `xor eax,eax` BEFORE
 *                  jumping into the tail's mid-point (0xded12), bypassing
 *                  the `esi`-based sbb computation entirely -> reply is
 *                  unconditionally success (0), `success` genuinely
 *                  ignored (confirmed real, not a modeling shortcut).
 *   {0x11}      -> jump target 0xdecd0: `mov esi,1` (forces success=true)
 *                  BEFORE falling into the SAME body as 0x10 below --
 *                  this makes 0x11's reply unconditionally success (0)
 *                  as a SIDE EFFECT of forcing esi=1, not because the
 *                  shared tail special-cases it.
 *   {0x10}      -> jump target 0xdecd5: the confirmed aftertouch-specific
 *                  NKS4TestMode/HandleAnalogController dispatch
 *                  (byte-identical constants -- deviceCode=7, param2=0,
 *                  param3=0x3ff -- to EndAftertouchCalibration's own
 *                  equivalent block), then falls into the SAME shared
 *                  tail (0xded02) as {1,4,0xb} with `esi` STILL the real
 *                  `success` param (nothing here forces it to 1) -- so
 *                  the reply genuinely IS `success ? 0 : -1` for this
 *                  state, exactly like {1,4,0xb}, NOT forced to 0.
 *                  (A prior pass here had this as "unconditionally
 *                  success (0)", conflating it with 0x11's forced-esi
 *                  side effect -- fixed 2026-07-27 after re-tracing
 *                  0xdecd5->0xded02 instruction-by-instruction and
 *                  finding no `xor eax,eax`/`mov esi,...` between them.)
 *   everything else -> plain no-op return, no reply sent
 */
void CSTGCalibrationMsgHandler::HandleKeybedCalibrationResult(bool success)
{
	switch (sCalibrationOp) {
	case 1:
	case 4:
	case 0xb:
		break;
	case 2:
	case 5:
	case 0xc:
		SendReply(0);
		return;
	case 0x11:
		success = true;
		/* fallthrough */
	case 0x10:
		if (IsNKS4TestMode())
			SendUnsolCalibrationMsg(7, 0x3ff, 0);
		else if (IsActivePerfVarsInFocus())
			CSTGFrontPanel::sInstance->HandleAnalogController(7, 0, 0x3ff);
		SendReply(success ? 0 : -1);
		return;
	default:
		return;
	}
	SendReply(success ? 0 : -1);
}
