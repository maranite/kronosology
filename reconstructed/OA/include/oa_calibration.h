// SPDX-License-Identifier: GPL-2.0
#ifndef OA_CALIBRATION_H
#define OA_CALIBRATION_H

/*
 * oa_calibration.h  -  CSTGCalibrationMsgHandler: the front-panel/keybed
 * analog-controller calibration state machine (joystick X/Y, vector
 * joystick, touch screen, ribbon controller, half-damper pedal,
 * aftertouch).
 *
 * Ground truth: `CSTGCalibrationMsgHandler::*`, OA_real.ko `.text+0xdcc60`
 * .. `.text+0xdf128` (nm addresses; the Ghidra-export `oa_export/functions/`
 * filenames for this cluster carry the project's already-documented
 * `+0x10000` offset, see docs/modules/OA.ko.md's "Address mapping"
 * section). Every function's real body was transcribed from the Ghidra
 * decompile in `oa_export/functions/`, then spot-verified against a raw
 * `objdump -dr -M intel` disassembly for `StartJSXCalibration`,
 * `EndAftertouchCalibration`, `EndTouchScreenCalibration`,
 * `ResetController`, `ResetDamper`, and `HandleKeybedCalibrationResult`
 * (including its real 18-entry jump table at `.rodata+0x4b31c`) -- the
 * decompile's VALUES matched the disassembly exactly in every spot
 * check (unlike `HandleRotary`/`front_panel_handlers.cpp`, this cluster's
 * decompile did not drop any stores), so the remaining 18 bodies are
 * transcribed directly from decompile text with high confidence, not
 * independently re-disassembled instruction-by-instruction.
 *
 * Not a real C++-virtual dispatch target in this project: the real
 * object DOES install a vtable pointer (`PTR__CSTGCalibrationMsgHandler_
 * 006c21f0`) at construction, but nothing in this cluster (or anywhere
 * else in this project so far) DISPATCHES through it -- matching the
 * established "install vs dispatch" rule (sec 10.153/10.225): modeled
 * as a raw untyped pointer field, not a real `virtual` base, to avoid
 * the CSTGAudioManager-class host/target vtable-corruption bug already
 * hit and fixed elsewhere in this project.
 *
 * `sMsgHandler` (`.bss+0x9e680`, 144 bytes = 18 real {fn,ctx} pairs,
 * `ctx` always NULL in ground truth) IS installed with real function
 * pointers here (matching the same rule: population is safe, nothing
 * dispatches through it yet). It is presumably read by a not-yet-
 * reconstructed generic message-dispatch framework shared with sibling
 * `*MsgHandler` classes elsewhere in OA.ko (e.g. `CSTGPatchMsgHandler`,
 * seen via relocations while disassembling this cluster) -- out of
 * scope here.
 *
 * `PushMessage` (ground truth `.text+0x116660`, 499 bytes, regparm
 * `void*` arg in EAX) is a genuinely separate, disproportionate cluster
 * from the already-real `PushUnsolicitedMessage` (`push_unsolicited_
 * message.cpp`) -- shares the SAME `.bss+0x106de0` "messages enabled"
 * gate flag per its own prologue, but is otherwise not reconstructed;
 * declared extern only, matching this project's established "confirmed
 * real, deliberately deferred" convention (same treatment as
 * `CSTGMidiInPortUSB`, oa_engine.h).
 */

extern "C" {
void PushMessage(void *msg) __attribute__((regparm(3)));
}

/*
 * 12-byte "solicited reply" packet PushMessage expects (confirmed via
 * raw disassembly at 4 independent call sites: StartJSXCalibration's
 * hardware-path failure return uses PushUnsolicitedMessage instead, but
 * every Start/End/Cancel/Reset* "reply to the UI" tail in this cluster
 * builds exactly this 12-byte shape).  `_pad2`/`echoTag` are never
 * varied in ground truth: `_pad2` is allocated but never written at any
 * call site (genuine uninitialized stack bytes on real hardware; zeroed
 * here for host-KAT determinism), `echoTag` is ALWAYS the literal
 * constant `0xc` (same value as `size` -- real semantic meaning not
 * independently confirmed, plausibly echoes an originating command's
 * own message-type byte).
 */
struct STGCalibrationReplyMsg {
	unsigned short size;    /* +0x0, always 0xc (=sizeof this struct) */
	unsigned short _pad2;   /* +0x2, never written in ground truth -- zeroed here */
	unsigned int echoTag;   /* +0x4, always 0xc in every observed call site */
	int result;             /* +0x8, 0 = success, -1 = "not applicable"/fail */
};

/*
 * 24-byte "unsolicited UI" packet, same PushUnsolicitedMessage shape
 * already established elsewhere in this project (oa_global.h's
 * SendUnsolGlobalMessageToUI, front_panel_handlers.cpp) -- this
 * cluster's own variant, confirmed via raw disassembly at ResetController/
 * ResetDamper/EndAftertouchCalibration/HandleKeybedCalibrationResult.
 */
struct STGCalibrationUnsolMsg {
	unsigned short size;      /* +0x0, always 0x18 */
	unsigned short source;    /* +0x2, always 1 */
	unsigned int reserved;    /* +0x4, always 0 */
	unsigned int subtype;     /* +0x8, always 0x12 (matches the terminal/idle sCalibrationOp value) */
	unsigned int deviceCode;  /* +0xc, eSTGAnalogDeviceCode */
	unsigned int value;       /* +0x10 */
	unsigned int scanCode;    /* +0x14, eSTGNKS4AnalogScanCode (or 0) */
};

/*
 * sMsgHandler dispatch-table entry shape (real, ground-truthed via
 * CSTGCalibrationMsgHandler::CSTGCalibrationMsgHandler()): `ctx` is
 * always installed as a literal 0 -- every one of the 18 real target
 * functions ignores `this`/context entirely, same convention already
 * established for CSTGFrontPanel's front-panel handlers.
 */
struct STGCalibrationMsgHandlerEntry {
	void (*fn)();
	void *ctx;
};

class CSTGCalibrationMsgHandler {
public:
	static CSTGCalibrationMsgHandler *sInstance;
	static STGCalibrationMsgHandlerEntry sMsgHandler[18]; /* .bss+0x9e680, real */

	void *_vtablePtr;        /* +0x0, install-only placeholder, see header comment */
	void *_msgHandlerTable;  /* +0x4, always &sMsgHandler */
	unsigned char _replyTag; /* +0x8, default 0x12 */

	CSTGCalibrationMsgHandler();

	/* Joystick X (controller id 5) */
	static void StartJSXCalibration();
	static void EndJSXCalibration();
	static void CancelJSXCalibration();
	/* Joystick Y (controller id 7) */
	static void StartJSYCalibration();
	static void EndJSYCalibration();
	static void CancelJSYCalibration();
	/* Vector joystick -- touch-panel-only, no keybed-hardware variant */
	static void StartVectorCalibration();
	static void EndVectorCalibration();
	/* Touch screen -- no gating on sCalibrationOp at all (real quirk) */
	static void StartTouchScreenCalibration();
	static void EndTouchScreenCalibration();
	/* Ribbon controller X (controller id 8) */
	static void StartRibbonXCalibration();
	static void EndRibbonXCalibration();
	static void CancelRibbonXCalibration();
	/* Half-damper pedal -- polarity auto-detect, no keybed-hardware variant */
	static void StartHalfDamperCalibration();
	static void EndHalfDamperCalibration();
	/* Aftertouch (controller id 9) */
	static void StartAftertouchCalibration();
	static void EndAftertouchCalibration();
	static void CancelAftertouchCalibration();

	/* Analog-sample feed: the real regparm(2) worker (EDX=deviceCode,
	 * CX=rawValue); `CSTGCalibrationMsgHandler_ProcessCalibration` below
	 * is the real regparm(3) C-linkage trampoline external callers use
	 * (`this`/EAX unused, forwards EDX/ECX unchanged). */
	static void ProcessCalibration(int deviceCode, short rawValue);

	/* SimpleReply(int) -- sends a 12-byte reply with the given result
	 * verbatim, no sCalibrationOp gating. Confirmed real but no caller
	 * reconstructed anywhere in this project yet. */
	static void SimpleReply(int result);

	/* Async ack from the keybed board (arrives via a not-yet-
	 * reconstructed CSTGKeybedInterface receive path) that an
	 * End/CancelCalibration hardware round-trip finished. */
	static void HandleKeybedCalibrationResult(bool success);

	/* Direct analog-controller reset entry (own caller not
	 * reconstructed anywhere in this project yet). */
	static void ResetController(unsigned int deviceCode, unsigned int scanCode,
				     unsigned char param3, unsigned short param4);
	static void ResetDamper();
};

extern "C" void CSTGCalibrationMsgHandler_ProcessCalibration(int deviceCode, short rawValue)
	__attribute__((regparm(3)));

#endif /* OA_CALIBRATION_H */
