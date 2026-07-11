// SPDX-License-Identifier: GPL-2.0
/*
 * test_korgusbaudio_stub.cpp  -  host-side known-answer tests for
 * korgusbaudio_stub.cpp. See korgusbaudio_stub.h for which signatures are
 * disassembly-confirmed against the real KorgUsbAudioDriver.ko binary vs.
 * inferred by naming/shape symmetry.
 */

#include <cstdio>
#include <cstring>
#include "../korgusbaudio_stub.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-70s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-70s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

int main(void)
{
	printf("KorgUsbAudioVirtualDriver known-answer test\n");
	printf("============================================\n");

	printf("[1] Before Initialize(): Start()/Done() report \"not initialized\" (3),\n"
	       "    matching the real driver's own confirmed status code\n");
	check_eq("KorgUsbAudioStart() before init returns 3", KorgUsbAudioStart(), 3);
	check_eq("KorgUsbAudioDone() before init returns 3", KorgUsbAudioDone(), 3);
	check_eq("KorgUsbAudioInitialized() before init is false", KorgUsbAudioInitialized(), 0);

	printf("[2] Initialize() then Start(): both report success (0)\n");
	check_eq("KorgUsbAudioInitialize() returns 0", KorgUsbAudioInitialize(), 0);
	check_eq("KorgUsbAudioInitialized() is now true", KorgUsbAudioInitialized(), 1);
	check_eq("KorgUsbAudioStart() after init returns 0", KorgUsbAudioStart(), 0);
	check_eq("KorgUsbAudioDone() after start returns 0", KorgUsbAudioDone(), 0);

	printf("[3] Output()/Input() return valid, distinct, in-bounds buffer pointers;\n"
	       "    OutputDone()/InputDone() advance the ring index\n");
	void *out1 = KorgUsbAudioOutput();
	void *in1  = KorgUsbAudioInput();
	check_eq("Output() and Input() pointers are distinct", out1 != in1, 1);
	KorgUsbAudioOutputDone();
	void *out2 = KorgUsbAudioOutput();
	check_eq("OutputDone() advances the ring so Output() returns a different slot",
		 out1 != out2, 1);
	KorgUsbAudioInputDone();
	void *in2 = KorgUsbAudioInput();
	check_eq("InputDone() advances the ring so Input() returns a different slot",
		 in1 != in2, 1);

	printf("[4] Starving checks always report \"not starving\" -- no real codec\n"
	       "    ever fills/drains these rings, so callers must not spin forever\n");
	check_eq("KorgUsbAudioInputStarving() == 0", KorgUsbAudioInputStarving(), 0);
	check_eq("KorgUsbAudioOutputStarving() == 0", KorgUsbAudioOutputStarving(), 0);

	printf("[5] MIDI family: always report ready/success\n");
	check_eq("KorgUsbMidiInitialize() == 0", KorgUsbMidiInitialize(), 0);
	check_eq("KorgUsbMidiInitialized() == 1", KorgUsbMidiInitialized(), 1);
	check_eq("KorgUsbMidiOutputCanSend(0) == 1", KorgUsbMidiOutputCanSend(0), 1);
	check_eq("KorgUsbMidiOutput(...) == 0", KorgUsbMidiOutput(0, "x", 1), 0);
	check_eq("KorgUsbRealtimeMidiOutputCanSend(0) == 1",
		 KorgUsbRealtimeMidiOutputCanSend(0), 1);
	check_eq("KorgUsbRealtimeMidiOutput(...) == 0",
		 KorgUsbRealtimeMidiOutput(0, "x", 1), 0);
	check_eq("KorgUsbMidiDone() == 0", KorgUsbMidiDone(), 0);

	printf("[6] USBMidiAccessory_SetDrumPadClient: trivial VM stand-in (see\n"
	       "    korgusbaudio_stub.h's own header comment for the real-hardware-\n"
	       "    home discrepancy this is deliberately papering over) -- always\n"
	       "    reports success for both a real pointer and NULL (unregister)\n");
	int dummyQueue;
	check_eq("USBMidiAccessory_SetDrumPadClient(&dummyQueue) == 0",
		 USBMidiAccessory_SetDrumPadClient(&dummyQueue), 0);
	check_eq("USBMidiAccessory_SetDrumPadClient(NULL) == 0",
		 USBMidiAccessory_SetDrumPadClient(0), 0);

	printf("============================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
