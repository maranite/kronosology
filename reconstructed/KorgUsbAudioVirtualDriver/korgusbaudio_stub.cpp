// SPDX-License-Identifier: GPL-2.0
/*
 * korgusbaudio_stub.cpp  -  no-op/always-ready implementations of the
 * KorgUsbAudio (etc.) & KorgUsbMidi (etc.) family, standing in for the real
 * KorgUsbAudioDriver.ko so OA.ko can insmod and run without a real USB
 * audio codec present. See README.md for the full rationale and the
 * important finding that init_module's own hard-fail gate (step 13,
 * CSTGAudioManager_StartAudioEngine) does NOT actually depend on any of
 * these functions' return values -- the real gate is three internal
 * CSTGThread::CreateRealTimeWithCPUAffinity calls, confirmed via
 * disassembly of CSTGAudioManager::StartAudioEngine() itself (see
 * MASTER_REFERENCE.md sec 10.37). This module exists to satisfy OA.ko's
 * link-time symbol dependency and provide plausible behavior if/when
 * these functions are actually called at runtime, not to gate boot.
 */

#include "korgusbaudio_stub.h"

static KorgUsbAudioStubState g_state;

int KorgUsbAudioInitialize(void)
{
	g_state.initialized = 1;
	return 0;
}

int KorgUsbAudioInitialized(void)
{
	return g_state.initialized;
}

int KorgUsbAudioStart(void)
{
	if (!g_state.initialized)
		return 3;	/* matches the real driver's "not initialized" code */
	g_state.started = 1;
	return 0;
}

int KorgUsbAudioDone(void)
{
	if (!g_state.initialized || !g_state.started)
		return 3;
	return 0;
}

void *KorgUsbAudioOutput(void)
{
	return g_state.outputBuf + (g_state.outputIndex % sizeof(g_state.outputBuf));
}

void *KorgUsbAudioInput(void)
{
	return g_state.inputBuf + (g_state.inputIndex % sizeof(g_state.inputBuf));
}

void KorgUsbAudioOutputDone(void)
{
	g_state.outputIndex++;
}

void KorgUsbAudioInputDone(void)
{
	g_state.inputIndex++;
}

int KorgUsbAudioInputStarving(void)
{
	/* No real codec ever fills the input ring; always report "not
	 * starving" so any caller checking this doesn't spin/log
	 * indefinitely on a condition that will never resolve. */
	return 0;
}

int KorgUsbAudioOutputStarving(void)
{
	return 0;
}

const char *KorgUsbAudioErrorString(int)
{
	return "KorgUsbAudioVirtualDriver: no error (stub)";
}

int KorgUsbAudioFormatSize(int)
{
	return 4;	/* plausible: 32-bit samples, not disassembly-confirmed */
}

const char *KorgUsbAudioFormatString(int)
{
	return "KorgUsbAudioVirtualDriver: stub format";
}

void KorgUsbAudioPrintIndices(void)
{
}

int KorgUsbMidiInitialize(void) { return 0; }
int KorgUsbMidiInitialized(void) { return 1; }
int KorgUsbMidiDone(void) { return 0; }
int KorgUsbMidiOutput(int, const void *, unsigned int) { return 0; }
int KorgUsbMidiOutputCanSend(int) { return 1; }
int KorgUsbRealtimeMidiOutput(int, const void *, unsigned int) { return 0; }
int KorgUsbRealtimeMidiOutputCanSend(int) { return 1; }

/* See korgusbaudio_stub.h's own header comment for the "wrong real-hardware
 * home, pragmatic VM stand-in" discrepancy note. Trivial: just remembers
 * the pointer (so a caller reading it back would see what it set), always
 * reports success -- no real drum-pad hardware to register with in a VM. */
static void *g_drumPadClientQueue;
int USBMidiAccessory_SetDrumPadClient(void *queue)
{
	g_drumPadClientQueue = queue;
	return 0;
}
