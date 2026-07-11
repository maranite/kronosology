// SPDX-License-Identifier: GPL-2.0
/*
 * test_keybed_debounce.cpp  -  host-side known-answer test for
 * CSTGKeybedKeyDebounceFilter_Initialize() (src/init/keybed_debounce.cpp,
 * batch 38).
 *
 * Links src/init/keybed_debounce.cpp directly (a brand-new TU no other
 * verify/ file links). Provides its own local `CSTGAudioBusManager::
 * sInstance` storage + a plain instance (not the real ctor/managers.cpp)
 * so this test stays isolated, matching this project's own established
 * convention for classes with heavy real-construction side effects
 * elsewhere (test_keybed_init.cpp/test_comport.cpp do the same for
 * CSTGComPort).
 */

#include <cstdio>
#include <cstring>

#include "oa_engine.h"

CSTGAudioBusManager *CSTGAudioBusManager::sInstance;
/* Mock ctor -- the real one lives in managers.cpp, not linked here
 * (matches test_audio_bus_manager.cpp's/test_midi_clock_sync.cpp's own
 * established convention for this exact class). */
CSTGAudioBusManager::CSTGAudioBusManager() {}

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

extern "C" void CSTGKeybedKeyDebounceFilter_Initialize(unsigned char *filter);

int main()
{
	static unsigned char filter[0xa20];
	memset(filter, 0xcc, sizeof(filter));

	CSTGAudioBusManager busMgr;
	memset(&busMgr, 0, sizeof(busMgr));
	busMgr.busGainScale = 1500.0f; /* the confirmed real-hardware constant */
	CSTGAudioBusManager::sInstance = &busMgr;

	printf("[1] zeroes the +0x10 flag of all 128 20-byte key records\n");
	CSTGKeybedKeyDebounceFilter_Initialize(filter);
	int allZero = 1;
	for (int i = 0; i < 128; i++)
		if (filter[i * 0x14 + 0x10] != 0)
			allZero = 0;
	check_eq("all 128 debounce flags cleared", allZero, 1);

	printf("[2] neighboring bytes within each record are left untouched (poison survives)\n");
	check_eq("record 0 byte +0x00 still poisoned", filter[0x00], 0xcc);
	check_eq("record 5 byte +0x05 still poisoned", filter[5 * 0x14 + 0x05], 0xcc);

	printf("[3] +0xa10 gets (int)(busGainScale * 50.0f * 0.001f) == 75\n");
	int window;
	memcpy(&window, filter + 0xa10, sizeof(window));
	check_eq("debounce window value", window, 75);

	printf("[4] a different busGainScale changes the computed window\n");
	memset(filter, 0xcc, sizeof(filter));
	busMgr.busGainScale = 2000.0f;
	CSTGKeybedKeyDebounceFilter_Initialize(filter);
	memcpy(&window, filter + 0xa10, sizeof(window));
	check_eq("debounce window scales with busGainScale", window, 100);

	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("All checks passed.\n");
	return 0;
}
