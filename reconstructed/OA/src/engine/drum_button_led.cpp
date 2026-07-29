// SPDX-License-Identifier: GPL-2.0
/*
 * drum_button_led.cpp  -  see oa_drum_button_led.h for full ground-truth
 * provenance and the 3-layer call chain this file completes.
 */

#include "oa_drum_button_led.h"

void CDrumButtonLED::initialize()
{
	mState = 0;
}

void CDrumButtonLED::start()
{
	if (mState == 0)
		M3RPPRGlue_TurnOnLED();
}

void CDrumButtonLED::stop()
{
	M3RPPRGlue_TurnOffLED();
}

void CDrumButtonLED::wakeup()
{
	mState = 0;
	M3RPPRGlue_TurnOnLED();
}

void CDrumButtonLED::sleep()
{
	mState = 1;
	M3RPPRGlue_BlinkLED();
}

void M3RPPRGlue_TurnOnLED(void)
{
	CSPRUIMsgSender::TurnOnDrumTrackLED();
}

void M3RPPRGlue_TurnOffLED(void)
{
	CSPRUIMsgSender::TurnOffDrumTrackLED();
}

void M3RPPRGlue_BlinkLED(void)
{
	CSPRUIMsgSender::BlinkDrumTrackLED();
}

/*
 * Each of the 3 senders below builds an IDENTICAL 28-byte CSKMessage
 * payload -- differing only in cmdId (0x2e/0x2f/0x30). Ground truth's
 * own stack-local layout, transcribed exactly (Ghidra's own local_2c..
 * local_14 declaration order/offsets, not simplified): (u16)0x1c@+0x00,
 * (u16)2@+0x02, (u32)4@+0x04, (u32)cmdId@+0x08, then a genuine 4-byte
 * GAP@+0x0c that ground truth's own disassembly never assigns (no
 * local variable occupies it -- real stack layout jumps from local_24
 * straight to local_1c, 8 bytes apart despite local_24 being only 4
 * bytes), then 3 more zeroed u32s @+0x10/+0x14/+0x18. Zeroed here rather
 * than left as genuine uninitialized stack garbage (the real target
 * behavior) purely so this reconstruction has no undefined behavior of
 * its own -- functionally immaterial since nothing downstream of
 * SKSTGGate_SendToUI is modeled in this project.
 */
static void SendDrumTrackLEDMessage(unsigned int cmdId)
{
	CSKMessage msg;
	for (unsigned int i = 0; i < sizeof(msg.raw); i++)
		msg.raw[i] = 0;
	*(unsigned short *)(msg.raw + 0x00) = 0x1c;
	*(unsigned short *)(msg.raw + 0x02) = 2;
	*(unsigned int *)(msg.raw + 0x04) = 4;
	*(unsigned int *)(msg.raw + 0x08) = cmdId;
	/* +0x0c: real gap, left zeroed (see comment above). */
	*(unsigned int *)(msg.raw + 0x10) = 0;
	*(unsigned int *)(msg.raw + 0x14) = 0;
	*(unsigned int *)(msg.raw + 0x18) = 0;
	SKSTGGate_SendToUI(&msg);
}

void CSPRUIMsgSender::TurnOnDrumTrackLED()
{
	SendDrumTrackLEDMessage(0x2e);
}

void CSPRUIMsgSender::TurnOffDrumTrackLED()
{
	SendDrumTrackLEDMessage(0x2f);
}

void CSPRUIMsgSender::BlinkDrumTrackLED()
{
	SendDrumTrackLEDMessage(0x30);
}
