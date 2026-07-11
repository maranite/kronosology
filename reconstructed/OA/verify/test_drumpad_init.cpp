// SPDX-License-Identifier: GPL-2.0
/*
 * test_drumpad_init.cpp  -  host-side known-answer test for
 * CSTGDrumPadInterface_Initialize()/_Cleanup() (src/init/drumpad_init.cpp,
 * batch 38).
 *
 * Links src/init/drumpad_init.cpp directly. Mocks the one real
 * external it calls (USBMidiAccessory_SetDrumPadClient) to confirm:
 *  - Initialize() forwards the module's own private receive-queue
 *    buffer address (non-NULL, stable across calls) and passes the
 *    mock's return value straight through as its own return value.
 *  - Cleanup() forwards a NULL pointer (unregister), return value
 *    discarded (void).
 */

#include <cstdio>

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

extern "C" {

static int g_setDrumPadClientCalls;
static void *g_lastQueueArg;
static int g_setDrumPadClientReturn;

int USBMidiAccessory_SetDrumPadClient(void *queue)
{
	g_setDrumPadClientCalls++;
	g_lastQueueArg = queue;
	return g_setDrumPadClientReturn;
}

int CSTGDrumPadInterface_Initialize(void);
void CSTGDrumPadInterface_Cleanup(void);

} // extern "C"

int main()
{
	printf("[1] Initialize() registers a non-NULL, stable queue pointer\n");
	g_setDrumPadClientReturn = 0x1234;
	int rc = CSTGDrumPadInterface_Initialize();
	check_eq("SetDrumPadClient call count", g_setDrumPadClientCalls, 1);
	check_eq("Initialize() queue arg is non-NULL", g_lastQueueArg != 0, 1);
	check_eq("Initialize() return value passes through", rc, 0x1234);
	void *firstQueue = g_lastQueueArg;

	printf("[2] a second Initialize() call reuses the SAME queue address\n");
	CSTGDrumPadInterface_Initialize();
	check_eq("queue address stable across calls", g_lastQueueArg == firstQueue, 1);

	printf("[3] Cleanup() unregisters with a NULL queue pointer\n");
	g_setDrumPadClientCalls = 0;
	CSTGDrumPadInterface_Cleanup();
	check_eq("SetDrumPadClient call count", g_setDrumPadClientCalls, 1);
	check_eq("Cleanup() passes NULL", g_lastQueueArg == 0, 1);

	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("All checks passed.\n");
	return 0;
}
