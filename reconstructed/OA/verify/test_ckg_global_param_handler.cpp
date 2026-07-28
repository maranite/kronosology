// SPDX-License-Identifier: GPL-2.0
/*
 * test_ckg_global_param_handler.cpp  -  KAT for CKGGlobalParamMsgHandler
 * (see ../src/engine/ckg_global_param_handler.cpp).
 *
 * Every one of the 27 methods is exercised individually since this class
 * has no shared skeleton to isolate via a gate. Expected values computed
 * independently here from the ground-truth offset/width facts documented
 * in the header, not by re-using the .cpp file's own C expressions.
 */

#include <cstdio>
#include <cstring>
#include "oa_ckg_global_param_msg_handler.h"

/* host-only mock storage/bodies for every out-of-scope singleton/dependency */
unsigned char *CKGEngine::ms_poInstance;
CKGParamEdit *CKGEngine::ms_poKGParamEdit;

static int g_resetLocalControllerCalls;
void CKGEngine::ResetLocalController() { g_resetLocalControllerCalls++; }

static int g_sendCalls;
static long g_lastSendArg;
void CKGParamEdit::SendGlobalTranspose(signed char v) { g_sendCalls++; g_lastSendArg = v; }
void CKGParamEdit::SendGlobalMIDICh(signed char v) { g_sendCalls++; g_lastSendArg = v; }
void CKGParamEdit::SendMIDIClockSource(bool v) { g_sendCalls++; g_lastSendArg = v; }
void CKGParamEdit::SendForceKarmaOff(bool v) { g_sendCalls++; g_lastSendArg = v; }
void CKGParamEdit::SetEnableMIDIInToKarmaModule(bool v) { g_sendCalls++; g_lastSendArg = v; }

static int g_renewMIDIChannelCalls;
void SPRMain_RenewMIDIChannel(void) { g_renewMIDIChannelCalls++; }
static int g_setAllKarmaCalls;
static bool g_lastSetAllKarmaArg;
void SPRMain_SetAllKARMAAndDrumTrack(bool v) { g_setAllKarmaCalls++; g_lastSetAllKarmaArg = v; }
static int g_progDrumChanCalls;
static int g_lastProgDrumChanArg;
void SPRMain_ProcessProgDrumTrackMIDIOutputChannel(int v) { g_progDrumChanCalls++; g_lastProgDrumChanArg = v; }
static int g_progDrumEnableCalls;
static bool g_lastProgDrumEnableArg;
void SPRMain_ProcessEnableProgDrumTrackMIDIOutput(bool v) { g_progDrumEnableCalls++; g_lastProgDrumEnableArg = v; }

static bool g_portAvailable;
static eSTGMidiPort g_lastPortArg;
bool SKSTGGate_IsMidiPortAvailable(eSTGMidiPort port) { g_lastPortArg = port; return g_portAvailable; }
eSTGMidiPort CKarmaGlobal::GetMIDIClockPortForSource(ESyncClockSource) { return (eSTGMidiPort)7; }

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-45s %ld\n", label, got); return; }
	printf("  FAIL  %-45s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x200
static unsigned char g_data[BUFSZ];

int main(void)
{
	printf("CKGGlobalParamMsgHandler known-answer test\n");
	printf("========================================================================\n");

	CKGGlobalParamMsgHandler h;
	memset(&h, 0, sizeof(h));
	h.m_globalData = g_data;

	CKGGlobalParamMsg msg;

	/* --- (a) plain field writes --- */
	memset(g_data, 0, BUFSZ);
	msg.m_index = 0; msg.m_value = 0x11;
	h.SetGlobalVelocityCurve(&msg);
	check_eq("SetGlobalVelocityCurve", g_data[0x1], 0x11);

	msg.m_value = 0x22;
	h.SetAfterTouchCurve(&msg);
	check_eq("SetAfterTouchCurve", g_data[0x2], 0x22);

	msg.m_value = 0x12345678;
	h.SetConvertPosition(&msg);
	check_eq("SetConvertPosition", *(int *)(g_data + 0x8), 0x12345678);

	msg.m_index = 3; msg.m_value = 0x55;
	h.SetControllerCCNo(&msg);
	check_eq("SetControllerCCNo", g_data[0x10 + 3], 0x55);

	msg.m_index = 2; msg.m_value = 0x11223344;
	h.SetPadCCNo(&msg);
	check_eq("SetPadCCNo", *(int *)(g_data + 0x24 + 2 * 4), 0x11223344);

	msg.m_index = 4; msg.m_value = 0x66;
	h.SetExternalPadChannel(&msg);
	check_eq("SetExternalPadChannel", g_data[0x44 + 4], 0x66);

	msg.m_index = 1; msg.m_value = 0x77889900;
	h.SetExternalPadAssign(&msg);
	check_eq("SetExternalPadAssign", *(int *)(g_data + 0x4c + 1 * 4), 0x77889900);

	msg.m_index = 5; msg.m_value = 0x7f;
	h.SetExternalPadVelocity(&msg);
	check_eq("SetExternalPadVelocity", g_data[0x6c + 5], 0x7f);

	/* --- (c) setne-style plain booleans --- */
	struct { const char *name; int off; void (CKGGlobalParamMsgHandler::*fn)(CKGGlobalParamMsg*); } boolTests[] = {
		{"SetRecieveExtCommands", 0x74, &CKGGlobalParamMsgHandler::SetRecieveExtCommands},
		{"SetVectorMIDIOut", 0x75, &CKGGlobalParamMsgHandler::SetVectorMIDIOut},
		{"SetPadsMIDIOut", 0x76, &CKGGlobalParamMsgHandler::SetPadsMIDIOut},
		{"SetAutoKarmaProg", 0x79, &CKGGlobalParamMsgHandler::SetAutoKarmaProg},
		{"SetAutoKarmaCombi", 0x7a, &CKGGlobalParamMsgHandler::SetAutoKarmaCombi},
		{"SetEnableKarmaModuleToMIDIOut", 0x7b, &CKGGlobalParamMsgHandler::SetEnableKarmaModuleToMIDIOut},
		{"SetSendStartStopInProgCombi", 0x7d, &CKGGlobalParamMsgHandler::SetSendStartStopInProgCombi},
	};
	for (auto &t : boolTests) {
		msg.m_value = 0;
		(h.*t.fn)(&msg);
		char label1[64]; snprintf(label1, sizeof(label1), "%s false", t.name);
		check_eq(label1, g_data[t.off], 0);
		msg.m_value = 5;	/* nonzero, not just 1 -- confirms setne semantics not raw truncation */
		(h.*t.fn)(&msg);
		char label2[64]; snprintf(label2, sizeof(label2), "%s true", t.name);
		check_eq(label2, g_data[t.off], 1);
	}

	/* --- (b) field write + Send/free-function side effect --- */
	memset(g_data, 0, BUFSZ);
	g_resetLocalControllerCalls = g_sendCalls = g_renewMIDIChannelCalls = 0;
	msg.m_value = -5;
	h.SetGlobalKeyTranspose(&msg);
	check_eq("SetGlobalKeyTranspose field", (signed char)g_data[0x0], -5);
	check_eq("SetGlobalKeyTranspose ResetLocalController called", g_resetLocalControllerCalls, 1);
	check_eq("SetGlobalKeyTranspose Send called", g_sendCalls, 1);
	check_eq("SetGlobalKeyTranspose Send arg", g_lastSendArg, -5);

	g_resetLocalControllerCalls = g_sendCalls = g_renewMIDIChannelCalls = 0;
	msg.m_value = 9;
	h.SetMIDIChannel(&msg);
	check_eq("SetMIDIChannel field", g_data[0x3], 9);
	check_eq("SetMIDIChannel ResetLocalController called", g_resetLocalControllerCalls, 1);
	check_eq("SetMIDIChannel Send called", g_sendCalls, 1);
	check_eq("SetMIDIChannel RenewMIDIChannel called", g_renewMIDIChannelCalls, 1);

	g_sendCalls = 0;
	msg.m_value = 1;
	h.SetForceKarmaOff(&msg);
	check_eq("SetForceKarmaOff field", g_data[0x77], 1);
	check_eq("SetForceKarmaOff Send called", g_sendCalls, 1);

	g_setAllKarmaCalls = 0;
	msg.m_value = 1;
	h.SetForceDTOff(&msg);
	check_eq("SetForceDTOff field", g_data[0x78], 1);
	check_eq("SetForceDTOff SetAllKARMAAndDrumTrack called", g_setAllKarmaCalls, 1);
	check_eq("SetForceDTOff arg true", g_lastSetAllKarmaArg, true);

	g_data[0x7e] = 0xff;
	g_progDrumChanCalls = 0;
	msg.m_value = 0x5;
	h.SetProgDrumTrackMIDIOutputChannel(&msg);
	check_eq("SetProgDrumTrackMIDIOutputChannel low nibble", g_data[0x7e] & 0xf, 5);
	check_eq("SetProgDrumTrackMIDIOutputChannel high nibble preserved", g_data[0x7e] & 0xf0, 0xf0);
	check_eq("SetProgDrumTrackMIDIOutputChannel free fn called", g_progDrumChanCalls, 1);
	check_eq("SetProgDrumTrackMIDIOutputChannel free fn arg == raw value", g_lastProgDrumChanArg, 5);

	g_data[0x7e] = 0x0f;
	g_progDrumEnableCalls = 0;
	msg.m_value = 1;
	h.SetProgDrumTrackEnableMIDIOutput(&msg);
	check_eq("SetProgDrumTrackEnableMIDIOutput bit4 set", (g_data[0x7e] >> 4) & 1, 1);
	check_eq("SetProgDrumTrackEnableMIDIOutput low nibble preserved", g_data[0x7e] & 0xf, 0xf);
	check_eq("SetProgDrumTrackEnableMIDIOutput free fn called", g_progDrumEnableCalls, 1);

	/* --- real no-ops --- */
	memset(g_data, 0xAA, BUFSZ);
	msg.m_value = 42;
	h.SetLocalOn(&msg);
	h.SetNoteReceive(&msg);
	bool unchanged = true;
	for (unsigned int i = 0; i < BUFSZ; i++) if (g_data[i] != 0xAA) unchanged = false;
	check_eq("SetLocalOn/SetNoteReceive real no-op: data untouched", unchanged, true);

	/* --- SetLocalControllerMIDICh: no field write, same 2-call tail --- */
	memset(g_data, 0xBB, BUFSZ);
	g_sendCalls = g_renewMIDIChannelCalls = 0;
	msg.m_value = 3;
	h.SetLocalControllerMIDICh(&msg);
	unchanged = true;
	for (unsigned int i = 0; i < BUFSZ; i++) if (g_data[i] != 0xBB) unchanged = false;
	check_eq("SetLocalControllerMIDICh: no field write", unchanged, true);
	check_eq("SetLocalControllerMIDICh Send called", g_sendCalls, 1);
	check_eq("SetLocalControllerMIDICh RenewMIDIChannel called", g_renewMIDIChannelCalls, 1);

	/* --- SetMIDIFilter: per-bit set/clear keyed by m_index --- */
	memset(g_data, 0, BUFSZ);
	g_data[0x4] = 0xFF;
	msg.m_index = 3; msg.m_value = 0;
	h.SetMIDIFilter(&msg);
	check_eq("SetMIDIFilter clear bit3", (g_data[0x4] >> 3) & 1, 0);
	check_eq("SetMIDIFilter clear: neighbors preserved", g_data[0x4] | 0x8, 0xFF);

	g_data[0x4] = 0;
	msg.m_index = 5; msg.m_value = 1;
	h.SetMIDIFilter(&msg);
	check_eq("SetMIDIFilter set bit5", (g_data[0x4] >> 5) & 1, 1);
	check_eq("SetMIDIFilter set: neighbors preserved", g_data[0x4] & ~0x20, 0);

	/* --- SetMIDIClockSource: real conditional gate --- */
	memset(g_data, 0, BUFSZ);
	g_sendCalls = 0;
	msg.m_value = 0;	/* zero source: always writes, port check skipped */
	h.SetMIDIClockSource(&msg);
	check_eq("SetMIDIClockSource value=0: field written", *(int *)(g_data + 0xc), 0);
	check_eq("SetMIDIClockSource value=0: Send called", g_sendCalls, 1);
	check_eq("SetMIDIClockSource value=0: Send arg false", g_lastSendArg, false);

	*(int *)(g_data + 0xc) = -1;
	g_sendCalls = 0;
	g_portAvailable = true;
	msg.m_value = 3;	/* nonzero, port available: writes + Send */
	h.SetMIDIClockSource(&msg);
	check_eq("SetMIDIClockSource port-available: field written", *(int *)(g_data + 0xc), 3);
	check_eq("SetMIDIClockSource port-available: Send called", g_sendCalls, 1);
	check_eq("SetMIDIClockSource port-available: Send arg true", g_lastSendArg, true);

	*(int *)(g_data + 0xc) = 0x77777777;
	g_sendCalls = 0;
	g_portAvailable = false;
	msg.m_value = 4;	/* nonzero, port UNAVAILABLE: entire method is a no-op */
	h.SetMIDIClockSource(&msg);
	check_eq("SetMIDIClockSource port-unavailable: field NOT written", *(int *)(g_data + 0xc), 0x77777777);
	check_eq("SetMIDIClockSource port-unavailable: no Send", g_sendCalls, 0);

	/* --- SetEnableMIDIInToKarmaModule: no field write, one real Set-not-Send call --- */
	memset(g_data, 0xCC, BUFSZ);
	g_sendCalls = 0;
	msg.m_value = 1;
	h.SetEnableMIDIInToKarmaModule(&msg);
	unchanged = true;
	for (unsigned int i = 0; i < BUFSZ; i++) if (g_data[i] != 0xCC) unchanged = false;
	check_eq("SetEnableMIDIInToKarmaModule: no field write", unchanged, true);
	check_eq("SetEnableMIDIInToKarmaModule: call fired", g_sendCalls, 1);
	check_eq("SetEnableMIDIInToKarmaModule: arg true", g_lastSendArg, true);

	printf("========================================================================\n");
	if (g_fail) {
		printf("%d CHECK(S) FAILED\n", g_fail);
		return 1;
	}
	printf("ALL CHECKS PASSED\n");
	return 0;
}
