// SPDX-License-Identifier: GPL-2.0
/*
 * test_drumpad_init.cpp  -  host-side known-answer test for
 * CSTGDrumPadInterface_Initialize()/_Cleanup() and ConstructDrumPadClient()
 * (src/init/drumpad_init.cpp).
 *
 * Links src/init/drumpad_init.cpp directly. Mocks the one real external
 * it calls (USBMidiAccessory_SetDrumPadClient) to confirm:
 *  - Initialize() forwards the module's own real CSTGDrumPadClient
 *    singleton address (non-NULL, stable across calls) and passes the
 *    mock's return value straight through as its own return value.
 *  - Cleanup() forwards a NULL pointer (unregister), return value
 *    discarded (void).
 *  - ConstructDrumPadClient() (the real do_mod_ctors() substitute, see
 *    oa_init.h) installs a REAL, non-NULL vtable pointer into the
 *    singleton's first word -- this is the exact literal-8-vs-relocation
 *    bug (see drumpad_init.cpp's own header comment) this pass fixed:
 *    before the fix, nothing installed this at all.
 *
 * drumpad_init.cpp now pulls in oa_control_msg_handler.h (for
 * CSTGDrumPadClient/CSTGGlobal/CSTGMessageProcessor) -- this test
 * provides its own local out-of-line definitions for the 2 singleton
 * pointers it doesn't otherwise need, matching test_control_msg_handler.cpp's
 * own established "isolated host test defines its own sInstance" pattern.
 */

#include <cstdio>
#include "oa_control_msg_handler.h"

CSTGGlobal *CSTGGlobal::sInstance;
CSTGMessageProcessor *CSTGMessageProcessor::sInstance;

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

int USBMidiAccessory_SetDrumPadClient(void *client)
{
	g_setDrumPadClientCalls++;
	g_lastQueueArg = client;
	return g_setDrumPadClientReturn;
}

int CSTGDrumPadInterface_Initialize(void);
void CSTGDrumPadInterface_Cleanup(void);
void ConstructDrumPadClient(void);

} // extern "C"

int main()
{
	printf("[1] Initialize() registers a non-NULL, stable client pointer\n");
	g_setDrumPadClientReturn = 0x1234;
	int rc = CSTGDrumPadInterface_Initialize();
	check_eq("SetDrumPadClient call count", g_setDrumPadClientCalls, 1);
	check_eq("Initialize() client arg is non-NULL", g_lastQueueArg != 0, 1);
	check_eq("Initialize() return value passes through", rc, 0x1234);
	void *firstClient = g_lastQueueArg;

	printf("[2] a second Initialize() call reuses the SAME client address\n");
	CSTGDrumPadInterface_Initialize();
	check_eq("client address stable across calls", g_lastQueueArg == firstClient, 1);

	printf("[3] Cleanup() unregisters with a NULL client pointer\n");
	g_setDrumPadClientCalls = 0;
	CSTGDrumPadInterface_Cleanup();
	check_eq("SetDrumPadClient call count", g_setDrumPadClientCalls, 1);
	check_eq("Cleanup() passes NULL", g_lastQueueArg == 0, 1);

	printf("[4] ConstructDrumPadClient() installs a real, non-NULL vtable pointer\n");
	check_eq("vtable ptr is NULL before ConstructDrumPadClient()",
	         *(void **)firstClient == 0, 1);
	ConstructDrumPadClient();
	check_eq("vtable ptr is non-NULL after ConstructDrumPadClient()",
	         *(void **)firstClient != 0, 1);

	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("All checks passed.\n");
	return 0;
}
