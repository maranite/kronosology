// SPDX-License-Identifier: GPL-2.0
/*
 * test_front_panel_msg_handler.cpp  -  host-side KAT for
 * CSTGFrontPanelMsgHandler (src/init/front_panel_msg_handler.cpp).
 *
 * Deliberately self-contained: links ONLY front_panel_msg_handler.cpp.
 * Mocks CSTGFrontPanel::SetLED/ResetLED/SetLEDBlinking/SetLED16Bits/Beep
 * (own real bodies are exercised separately by
 * test_front_panel_key_handlers.cpp) so this test isolates exactly the
 * "unwrap param, forward" glue this class is responsible for, plus the
 * constructor's table-population correctness.
 */

#include <cstdio>
#include <cstring>
#include <cstdint>
#include "oa_front_panel_msg_handler.h"	/* pulls in oa_internal.h's own placement-new */

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

/* ---- link-satisfying mocks ---- */
CSTGFrontPanel *CSTGFrontPanel::sInstance;

static int g_setLedCalls, g_resetLedCalls, g_blinkCalls, g_set16Calls, g_beepCalls;
static unsigned int g_lastCode, g_lastBlinkCode, g_lastResetCode;
static unsigned long g_last16Bits;

void CSTGFrontPanel::SetLED(unsigned int code) { g_setLedCalls++; g_lastCode = code; }
void CSTGFrontPanel::ResetLED(unsigned int code) { g_resetLedCalls++; g_lastResetCode = code; }
void CSTGFrontPanel::SetLEDBlinking(unsigned int code) { g_blinkCalls++; g_lastBlinkCode = code; }
void CSTGFrontPanel::SetLED16Bits(unsigned long m) { g_set16Calls++; g_last16Bits = m; }
void CSTGFrontPanel::Beep() { g_beepCalls++; }

/* CSTGFrontPanel's own real ctor/HandleKeyOn/etc. are not needed --
 * this test never constructs a real CSTGFrontPanel object, only points
 * CSTGFrontPanel::sInstance at a plain buffer. */

int main()
{
	static unsigned char frontPanelBuf[8];
	CSTGFrontPanel::sInstance = (CSTGFrontPanel *)frontPanelBuf;

	CSTGFrontPanelMsgHandler handler; /* runs the real ctor, populates sMsgHandler */
	CSTGFrontPanelMsgHandler *h = &handler;

	printf("== constructor: install-only fields + table population ==\n");
	check_eq("_vtablePtr installed (non-owning placeholder) is null", (long)(intptr_t)h->_vtablePtr, 0);
	check_eq("_msgHandlerTable == &sMsgHandler",
		 (long)((void *)h->_msgHandlerTable == (void *)CSTGFrontPanelMsgHandler::sMsgHandler), 1);
	check_eq("_replyTag == 0x05", h->_replyTag, 0x05);
	check_eq("sInstance == this", (long)(h == CSTGFrontPanelMsgHandler::sInstance), 1);

	for (int i = 0; i < 5; i++) {
		char label[64];
		snprintf(label, sizeof(label), "sMsgHandler[%d].fn is non-null", i);
		check_eq(label, (long)(CSTGFrontPanelMsgHandler::sMsgHandler[i].fn != nullptr), 1);
		snprintf(label, sizeof(label), "sMsgHandler[%d].ctx == NULL", i);
		check_eq(label, (long)(intptr_t)CSTGFrontPanelMsgHandler::sMsgHandler[i].ctx, 0);
	}

	printf("== wrapper forwarding ==\n");

	STGMsgDataOneParam p;

	printf("[1] SetLED(param, source) forwards param->value unchanged, source ignored\n");
	p.value = 0x49;
	g_setLedCalls = 0;
	h->SetLED(&p, 0x1234);
	check_eq("SetLED calls CSTGFrontPanel::SetLED once", g_setLedCalls, 1);
	check_eq("SetLED forwards code", (long)g_lastCode, 0x49);

	printf("[2] ResetLED(param, source) forwards param->value unchanged\n");
	p.value = 0x4a;
	g_resetLedCalls = 0;
	h->ResetLED(&p, 0);
	check_eq("ResetLED calls CSTGFrontPanel::ResetLED once", g_resetLedCalls, 1);
	check_eq("ResetLED forwards code", (long)g_lastResetCode, 0x4a);

	printf("[3] SetLEDBlinking(param, source) forwards param->value unchanged\n");
	p.value = 0x1234;
	g_blinkCalls = 0;
	h->SetLEDBlinking(&p, 0);
	check_eq("SetLEDBlinking calls CSTGFrontPanel::SetLEDBlinking once", g_blinkCalls, 1);
	check_eq("SetLEDBlinking forwards code", (long)g_lastBlinkCode, 0x1234);

	printf("[4] SetLED16Bits(param, source) forwards param->value unchanged\n");
	p.value = 0xdeadbeef;
	g_set16Calls = 0;
	h->SetLED16Bits(&p, 0);
	check_eq("SetLED16Bits calls CSTGFrontPanel::SetLED16Bits once", g_set16Calls, 1);
	check_eq("SetLED16Bits forwards value", (long)(unsigned int)g_last16Bits, (long)0xdeadbeef);

	printf("[5] Beep(param, source) ignores param entirely\n");
	g_beepCalls = 0;
	h->Beep(nullptr, 0);
	check_eq("Beep calls CSTGFrontPanel::Beep once", g_beepCalls, 1);

	if (g_fail) {
		printf("\n%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("\nall checks passed\n");
	return 0;
}
