// SPDX-License-Identifier: GPL-2.0
#ifndef OA_SEQ_LED_H
#define OA_SEQ_LED_H

/*
 * oa_seq_led.h  -  TurnOnSeqLed() and its exported wrapper family: the
 * real front-panel transport LEDs (Start/Stop-Red, Start/Stop-Green,
 * Rec, Pause, FF, Rew).
 *
 * Ground truth (nm addresses against /home/share/Decomp/OA.ko_Decomp/
 * OA.ko): `TurnOnSeqLed(eSeqLEDId, bool)` (`.text+0x10f430`, 77 bytes)
 * is the real, single implementation -- confirmed via full disassembly
 * -- forwarding to the already-real `CSTGFrontPanel::SetLED`/`ResetLED`
 * (oa_setup_global_resources.h) under a real `pushf;cli`/`popf`
 * critical section (modeled via this project's already-established
 * `stg_local_irq_save`/`stg_local_irq_restore` accessors, oa_comport.h
 * -- same real x86 privileged-instruction pair, confirmed real via
 * comport.cpp's own disassembly cross-check).
 *
 * `SKSTGGate_TurnOnXxxLED(bool)` (`.text+0x3499a0`..`0x349a60`, 12-17
 * bytes each) are the real "Synth Kernel/sequencer gate" entry points
 * every one of these ultimately forwards through -- each just loads a
 * fixed `eSeqLEDId` constant and tail-calls `TurnOnSeqLed`.
 * `SPROutGate_TurnOnXxxLED(bool)` (`.text+0x34ba20`..`0x34bac0`, 12
 * bytes each) are a SECOND, parallel "sample playback/recorder gate"
 * entry point family that simply tail-calls the matching `SKSTGGate_*`
 * function -- pure passthrough trampolines, confirmed via full
 * disassembly (both families found while surveying OA.ko broadly for
 * remaining hardware-integration candidates, batch 2026-07-25).
 *
 * `eSeqLEDId` mapping (ground-truthed from `TurnOnSeqLed`'s own
 * disassembly): 0=StartStopRed, 1=StartStopGreen, 2=Rec, 3=Pause,
 * 4=FF, 5=Rew -- but id 0 and 1 are a REAL, CONFIRMED NO-OP in
 * `TurnOnSeqLed` (its own range check only maps ids 2-5 to an
 * `eSTGLEDCode`; ids 0/1 fall through to an early return with no
 * `SetLED`/`ResetLED` call at all). Reproduced faithfully -- NOT a
 * transcription mistake, confirmed by re-tracing the real jump table
 * twice. The real Start/Stop-Red/Green LEDs are presumably driven
 * through a DIFFERENT, not-yet-reconstructed path (plausibly the
 * `Flash*` timed-blink family in `CSTGTempoUtils`, deliberately NOT
 * pursued this pass -- see HARDWARE_REVIEW_LOG.md) rather than this
 * simple on/off one.
 *
 * `eSTGLEDCode` mapping used by the 4 live ids (confirmed): Rec=0x17,
 * Pause=0x1a, FF=0x1c, Rew=0x1b.
 */

#include "oa_setup_global_resources.h"
#include "oa_comport.h"

extern "C" {

void TurnOnSeqLed(int seqLedId, bool on) __attribute__((regparm(3)));

void SKSTGGate_TurnOnStartStopRedLed(bool on);
void SKSTGGate_TurnOnStartStopGreenLed(bool on);
void SKSTGGate_TurnOnRecLED(bool on);
void SKSTGGate_TurnOnPauseLED(bool on);
void SKSTGGate_TurnOnFFLED(bool on);
void SKSTGGate_TurnOnRewLED(bool on);

void SPROutGate_TurnOnStartStopRedLed(bool on);
void SPROutGate_TurnOnStartStopGreenLed(bool on);
void SPROutGate_TurnOnRecLED(bool on);
void SPROutGate_TurnOnPauseLED(bool on);
void SPROutGate_TurnOnFFLED(bool on);
void SPROutGate_TurnOnRewLED(bool on);

}

#endif /* OA_SEQ_LED_H */
