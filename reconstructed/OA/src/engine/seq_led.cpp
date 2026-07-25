// SPDX-License-Identifier: GPL-2.0
/*
 * seq_led.cpp  -  TurnOnSeqLed() + the SKSTGGate_/SPROutGate_ transport
 * LED wrapper families. See include/oa_seq_led.h for the full
 * ground-truth provenance and the confirmed real id/LED-code mapping
 * (including the confirmed-real no-op for ids 0/1).
 */

#include "oa_seq_led.h"

void TurnOnSeqLed(int seqLedId, bool on)
{
	int ledCode;
	switch (seqLedId) {
	case 2: ledCode = 0x17; break;	/* Rec */
	case 3: ledCode = 0x1a; break;	/* Pause */
	case 4: ledCode = 0x1c; break;	/* FF */
	case 5: ledCode = 0x1b; break;	/* Rew */
	default: return;		/* ids 0/1 (StartStopRed/Green): confirmed real no-op */
	}

	unsigned long savedFlags = stg_local_irq_save();
	if (on)
		CSTGFrontPanel::sInstance->SetLED(ledCode);
	else
		CSTGFrontPanel::sInstance->ResetLED(ledCode);
	stg_local_irq_restore(savedFlags);
}

void SKSTGGate_TurnOnStartStopRedLed(bool on) { TurnOnSeqLed(0, on); }
void SKSTGGate_TurnOnStartStopGreenLed(bool on) { TurnOnSeqLed(1, on); }
void SKSTGGate_TurnOnRecLED(bool on) { TurnOnSeqLed(2, on); }
void SKSTGGate_TurnOnPauseLED(bool on) { TurnOnSeqLed(3, on); }
void SKSTGGate_TurnOnFFLED(bool on) { TurnOnSeqLed(4, on); }
void SKSTGGate_TurnOnRewLED(bool on) { TurnOnSeqLed(5, on); }

void SPROutGate_TurnOnStartStopRedLed(bool on) { SKSTGGate_TurnOnStartStopRedLed(on); }
void SPROutGate_TurnOnStartStopGreenLed(bool on) { SKSTGGate_TurnOnStartStopGreenLed(on); }
void SPROutGate_TurnOnRecLED(bool on) { SKSTGGate_TurnOnRecLED(on); }
void SPROutGate_TurnOnPauseLED(bool on) { SKSTGGate_TurnOnPauseLED(on); }
void SPROutGate_TurnOnFFLED(bool on) { SKSTGGate_TurnOnFFLED(on); }
void SPROutGate_TurnOnRewLED(bool on) { SKSTGGate_TurnOnRewLED(on); }
