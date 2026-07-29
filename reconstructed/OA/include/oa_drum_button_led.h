// SPDX-License-Identifier: GPL-2.0
/*
 * oa_drum_button_led.h  -  CDrumButtonLED: the front-panel drum-track
 * button LED state machine, plus its full real call chain down to the
 * UI-message send gate.
 *
 * FOUND 2026-07-29 (round 47, solo -- session-wide 200-subagent dispatch
 * cap hit, see PROJECT_BRAIN/status.md), a fresh class-inventory sweep
 * for a small, self-contained hardware-integration cluster. Confirmed
 * via /home/share/Decomp/oa_export's own per-function decompiles, a full
 * 3-layer vertical slice, every function real and address-confirmed:
 *
 *   CDrumButtonLED::start()/wakeup()/sleep()/initialize() (this project)
 *     -> M3RPPRGlue_TurnOnLED/TurnOffLED/BlinkLED()      (this project)
 *       -> CSPRUIMsgSender::TurnOnDrumTrackLED/TurnOffDrumTrackLED/
 *          BlinkDrumTrackLED()                            (this project)
 *         -> SKSTGGate_SendToUI(CSKMessage const*)     (unresolved extern,
 *                                                        real relocation-
 *                                                        confirmed symbol,
 *                                                        `_Z18SKSTGGate_
 *                                                        SendToUIPK10CSK
 *                                                        Message`, NOT
 *                                                        reconstructed --
 *                                                        same treatment as
 *                                                        `KGOutGate_
 *                                                        SendMessageToUI`
 *                                                        in
 *                                                        oa_ckg_control_
 *                                                        ui_msg.h)
 *
 * `CDrumButtonLED`'s own state: a single byte field (offset 0,
 * `mState`) -- confirmed via `initialize()`/`wakeup()` both writing 0,
 * `sleep()` writing 1, and `start()` gating `TurnOnLED()` behind
 * `mState==0`. Real semantics not independently named beyond "0 = idle/
 * off, 1 = blinking" (matches `sleep()`'s own `BlinkLED()` call).
 * `stop()` is `__cdecl` in ground truth (no `this` used at all -- it
 * doesn't touch `mState`), modeled here as `static` to match.
 *
 * `CSKMessage` reused from `oa_ckg_control_ui_msg.h` (same 0x30-byte
 * opaque-payload convention, same project). `TurnOnDrumTrackLED()`/
 * `TurnOffDrumTrackLED()`/`BlinkDrumTrackLED()` each build an IDENTICAL
 * 28-byte payload shape (u16 0x1c @+0x00, u16 2 @+0x02, u32 4 @+0x04,
 * u32 command id @+0x08 -- 0x2e/0x2f/0x30 -- a genuine 4-byte GAP
 * @+0x0c ground truth's own disassembly never assigns, then 3 more
 * zeroed u32s @+0x10/+0x14/+0x18) -- every constant and the gap itself
 * transcribed directly from ground truth's real stack-offset layout,
 * not inferred (see drum_button_led.cpp's own comment for the full
 * derivation).
 */

#ifndef OA_DRUM_BUTTON_LED_H
#define OA_DRUM_BUTTON_LED_H

#include "oa_ckg_control_ui_msg.h"	/* CSKMessage */

extern "C" void SKSTGGate_SendToUI(const CSKMessage *msg) __attribute__((regparm(3)));

extern "C" void M3RPPRGlue_TurnOnLED(void);
extern "C" void M3RPPRGlue_TurnOffLED(void);
extern "C" void M3RPPRGlue_BlinkLED(void);

struct CSPRUIMsgSender {
	static void TurnOnDrumTrackLED();
	static void TurnOffDrumTrackLED();
	static void BlinkDrumTrackLED();
};

class CDrumButtonLED {
public:
	void initialize();
	void start();
	static void stop();
	void wakeup();
	void sleep();

private:
	unsigned char mState;	/* +0x0 */
};

#endif /* OA_DRUM_BUTTON_LED_H */
