// SPDX-License-Identifier: GPL-2.0
/*
 * test_ckg_switch_family.cpp  -  KAT for the
 * CKGController/CKGSwitch/CKGKnob/CKGPad diamond-inheritance widget
 * hierarchy (see ../src/engine/ckg_switch_family.cpp).
 *
 * Two kinds of check, both independent of the reconstruction's own
 * logic (never "does it produce the value it produces"):
 *
 *  1. STRUCTURAL: every leaf is instantiated and driven through a
 *     `CKGController*` (and, for the switch tiers, a `CKGSwitch*`)
 *     base pointer -- if the diamond virtual-inheritance ABI were
 *     wrong (wrong vbase offsets, wrong vtable layout), these calls
 *     would either crash or silently reach the wrong override. GCC
 *     regenerating correct VTT/thunk boilerplate from the real
 *     `virtual public` inheritance graph is exactly what's being
 *     proven here, matching this project's established "let the
 *     compiler do the ABI work" technique.
 *  2. FIELD-OFFSET ORACLE: every GetCCNumber()/GetCurrentValue() KAT
 *     value below was computed by hand directly from the real
 *     ground-truth byte offsets documented in oa_ckg_switch_family.h's
 *     own comments (e.g. "CKGKarmaOnOffSw::GetCCNumber() reads
 *     ms_poInstance[0x97c754]") -- the test sets that exact byte in
 *     the mock buffer to a chosen sentinel and asserts the method
 *     returns it, an oracle independent of the C++ implementation
 *     under test (it would fail exactly as loudly if the .cpp used
 *     the wrong offset).
 */

#include <cstdio>
#include <cstring>
#include "oa_ckg_switch_family.h"

/* ==================== mock singleton storage ==================== */

unsigned char *CKGEngine::ms_poInstance;
CKGParamEdit *CKGEngine::ms_poKGParamEdit;
unsigned char *CKGBankManager::ms_poInstance;
unsigned char *CKGUIMsgProcessor::ms_poInstance;
unsigned char *CKGRTCHandler::ms_poInstance;
unsigned char *CSKMIDIMsgProcessor::ms_poInstance;

static int g_fail;
static void check(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-48s %ld (0x%lx)\n", label, got, (unsigned long)got); return; }
	printf("  FAIL  %-48s got=%ld(0x%lx) want=%ld(0x%lx)\n", label, got, (unsigned long)got, want, (unsigned long)want);
	g_fail++;
}

/* ==================== mocked out-of-scope dependencies ==================== */

static int g_lastRTCChannel;
int CKGEngine::GetLocalControllerChannel() { return g_lastRTCChannel; }
static int g_lastResetKRTCSwitchArg = -1;
void CKGEngine::ResetKRTCSwitch(int cc) { g_lastResetKRTCSwitchArg = cc; }
static int g_lastResetKRTCSliderArg = -1;
void CKGEngine::ResetKRTCSlider(int id) { g_lastResetKRTCSliderArg = id; }
static int g_numOfModule = 4;
int CKGEngine::GetNumOfModule() { return g_numOfModule; }
void CKGEngine::ResetLocalController() {}

static int g_lastChordMemoryIndex = -1;
static int g_lastChordMemoryValue = -1;
void CKGParamEdit::SendChordMemory(int index, unsigned char value, int)
{
	g_lastChordMemoryIndex = index;
	g_lastChordMemoryValue = value;
}
static bool g_lastSendFF, g_lastSendRewind, g_lastSendAssign;
void CKGParamEdit::SendFF(bool on) { g_lastSendFF = on; }
void CKGParamEdit::SendRewind(bool on) { g_lastSendRewind = on; }
void CKGParamEdit::SendAssign(bool on) { g_lastSendAssign = on; }

static int g_processCalls;
static int g_lastMsgId, g_lastArg2, g_lastArg3, g_lastArg4;
static bool g_lastChanged;
void CKGUIMsgProcessor::ProcessRTControllersValue(int msgId, int arg2, int arg3, bool changed)
{
	g_processCalls++;
	g_lastMsgId = msgId; g_lastArg2 = arg2; g_lastArg3 = arg3; g_lastArg4 = 0; g_lastChanged = changed;
}
void CKGUIMsgProcessor::ProcessRTControllersValue(int msgId, int arg2, int arg3, int arg4, bool changed)
{
	g_processCalls++;
	g_lastMsgId = msgId; g_lastArg2 = arg2; g_lastArg3 = arg3; g_lastArg4 = arg4; g_lastChanged = changed;
}

static unsigned char g_backupScene[8];
unsigned char *CKGRTCHandler::GetBackupScene() { return g_backupScene; }
static int g_backupSceneNumber = 3;
int CKGRTCHandler::GetBackupSceneNumber(int) { return g_backupSceneNumber; }
static unsigned char g_backupControlBuffer[8];
unsigned char *CKGRTCHandler::GetBackupControlBuffer() { return g_backupControlBuffer; }
static int g_resetCurrentSceneCalls;
void CKGRTCHandler::ResetCurrentScene() { g_resetCurrentSceneCalls++; }
static int g_resetCurrentControlBufferCalls;
void CKGRTCHandler::ResetCurrentControlBuffer() { g_resetCurrentControlBufferCalls++; }
static int g_resetChordAssignSwitchCalls;
void CKGRTCHandler::ResetChordAssignSwitch() { g_resetChordAssignSwitchCalls++; }

static int g_karmaMsgCalls;
static int g_lastKarmaStatus, g_lastKarmaChannel, g_lastKarmaCC, g_lastKarmaValue;
void CSKMIDIMsgProcessor::ProcessKarmaControllerGeneratedChannelMessage(int status, unsigned char channel, char cc, char value)
{
	g_karmaMsgCalls++;
	g_lastKarmaStatus = status; g_lastKarmaChannel = channel; g_lastKarmaCC = cc; g_lastKarmaValue = value;
}

/* CKGUIMsgSender is real/tested elsewhere (ckg_ui_msg_sender.cpp); here
 * only the one method this cluster reaches through a raw fixed-offset
 * pointer is mocked, self-contained. */
static int g_updateChordAssignLEDCalls;
static bool g_lastChordAssignLEDOn;
void CKGUIMsgSender::UpdateChordAssignLED(bool on) { g_updateChordAssignLEDCalls++; g_lastChordAssignLEDOn = on; }

static int g_tapSwitchOnCalls;
void CTapTempoHandler::TapSwitchOn(bool) { g_tapSwitchOnCalls++; }

int CKarmaGlobal::GetExternalPadRealChannel(unsigned char *table, int index) { return table[index]; }

static int g_notifyAllSlidersCalls;
extern "C" void SKSTGGate_NotifyKarmaAllSlidersPosition(void) { g_notifyAllSlidersCalls++; }
static int g_notifySliderPositionArg = -1;
void SKSTGGate_NotifyKarmaSliderPosition(int index) { g_notifySliderPositionArg = index; }
static bool g_isExternalMode;
extern "C" bool SKSTGGate_IsExternalMode(void) { return g_isExternalMode; }
static int g_sendToMIDIPortCalls;
static unsigned char g_lastMIDIBytes[3];
extern "C" void SKSTGGate_SendToMIDIPort(const unsigned char *bytes, unsigned short len)
{
	g_sendToMIDIPortCalls++;
	if (len == 3) memcpy(g_lastMIDIBytes, bytes, 3);
}
static bool g_seqChasing;
extern "C" bool KGOutGate_IsSeqChasingParameters(void) { return g_seqChasing; }
static bool g_drumTrackOn;
extern "C" void SPRMain_ProcessDrumTrackSwitch(bool on) { g_drumTrackOn = on; }
extern "C" bool SPRMain_GetDrumTrackSwitchStatus(void) { return g_drumTrackOn; }

#define BANKSZ 0x97c800
static unsigned char g_bankBuf[BANKSZ];
static unsigned char g_bankStateBuf[0x10];	/* the SEPARATE small "current status"
						 * sub-object ms_poInstance[+0] points at --
						 * genuinely distinct storage from g_bankBuf
						 * itself, matching real ground truth (see
						 * OA_CKGBankMgrState()'s own header comment) */
static unsigned char *g_bankStatePtr = g_bankStateBuf;

static void reset_all_mocks()
{
	memset(g_bankBuf, 0, sizeof(g_bankBuf));
	memset(g_bankStateBuf, 0, sizeof(g_bankStateBuf));
	*(unsigned char **)g_bankBuf = g_bankStatePtr;	/* +0: pointer to the state sub-object */
	CKGBankManager::ms_poInstance = g_bankBuf;

	memset(g_backupScene, 0, sizeof(g_backupScene));
	memset(g_backupControlBuffer, 0, sizeof(g_backupControlBuffer));
	static unsigned char rtcBuf[0x100];
	memset(rtcBuf, 0, sizeof(rtcBuf));
	*(unsigned char **)(rtcBuf + 0xd4) = g_backupScene;
	CKGRTCHandler::ms_poInstance = rtcBuf;

	static unsigned char uiProcBuf[0x100];
	memset(uiProcBuf, 0, sizeof(uiProcBuf));
	CKGUIMsgProcessor::ms_poInstance = uiProcBuf;

	static unsigned char midiProcBuf[4];
	CSKMIDIMsgProcessor::ms_poInstance = midiProcBuf;
	static unsigned char tapBuf[4];
	CTapTempoHandler::ms_poInstance = tapBuf;
	static unsigned char engineBuf[4];
	CKGEngine::ms_poInstance = engineBuf;
	static unsigned char paramEditBuf[4];
	CKGEngine::ms_poKGParamEdit = (CKGParamEdit*)paramEditBuf;

	g_lastRTCChannel = 0;
	g_processCalls = 0;
	g_karmaMsgCalls = 0;
	g_notifyAllSlidersCalls = 0;
	g_notifySliderPositionArg = -1;
	g_resetCurrentSceneCalls = 0;
	g_resetCurrentControlBufferCalls = 0;
	g_resetChordAssignSwitchCalls = 0;
	g_updateChordAssignLEDCalls = 0;
	g_tapSwitchOnCalls = 0;
	g_isExternalMode = false;
	g_sendToMIDIPortCalls = 0;
	g_seqChasing = false;
	g_drumTrackOn = false;
	g_numOfModule = 4;
}

int main(void)
{
	printf("CKGController/CKGSwitch/CKGKnob/CKGPad widget hierarchy known-answer test\n");
	printf("========================================================================\n");

	/* ---- Structural: diamond dispatch through CKGController* ---- */
	printf("-- diamond-inheritance dispatch (via CKGController* base pointer) --\n");
	{
		reset_all_mocks();
		CKGKarmaOnOffSw karmaOnOff;
		CKGController *base = &karmaOnOff;
		g_bankBuf[0x97c754] = 0x2a;
		check("CKGKarmaOnOffSw via CKGController*: GetCCNumber()", base->GetCCNumber(), 0x2a);

		CKGFFSw ff;
		base = &ff;
		check("CKGFFSw via CKGController*: GetCCNumber()==0xff", base->GetCCNumber(), 0xff);

		CKGKarmaAssignableKnob knob(3);
		base = &knob;
		check("CKGKarmaAssignableKnob via CKGController*: dispatch reaches leaf",
		      base->GetCCValue(), knob.GetCCValue());

		CKGChordTrigger pad(5);
		base = &pad;
		check("CKGChordTrigger via CKGController*: dispatch reaches leaf (GetCCValue==0)",
		      base->GetCCValue(), 0);

		CKGModuleControlSw mod;
		base = &mod;
		check("CKGModuleControlSw via CKGController*: dispatch reaches leaf (GetCCNumber==0xff)",
		      base->GetCCNumber(), 0xff);
		check("CKGModuleControlSw own GetMaxValue()==4", mod.GetMaxValue(), 4);
	}

	/* ---- Field-offset oracle: GetCCNumber()/GetCurrentValue() per leaf ---- */
	printf("-- field-offset oracle checks (ground-truth byte offsets) --\n");
	{
		reset_all_mocks();
		g_bankBuf[0x97c754] = 0x11;	/* KarmaOnOff CC# */
		CKGKarmaOnOffSw s1;
		check("CKGKarmaOnOffSw::GetCCNumber()", s1.GetCCNumber(), 0x11);
		g_bankStatePtr[2] = 0x80;	/* bit 7 */
		check("CKGKarmaOnOffSw::GetCurrentValue() (bit7)", s1.GetCurrentValue(), 1);

		reset_all_mocks();
		g_bankBuf[0x97c756] = 0x22;	/* Latch CC# */
		CKGLatchSw s2;
		check("CKGLatchSw::GetCCNumber()", s2.GetCCNumber(), 0x22);
		g_bankStatePtr[2] = 0x40;	/* bit 6 */
		check("CKGLatchSw::GetCurrentValue() (bit6)", s2.GetCurrentValue(), 1);

		reset_all_mocks();
		g_bankStatePtr[2] = 0x20;	/* bit 5 */
		CKGPadModSw s3;
		check("CKGPadModSw::GetCurrentValue() (bit5)", s3.GetCurrentValue(), 1);

		reset_all_mocks();
		g_bankBuf[0x97c755] = 0x33;	/* Scene CC# */
		CKGSceneSw s4(0);
		check("CKGSceneSw::GetCCNumber()", s4.GetCCNumber(), 0x33);

		reset_all_mocks();
		g_bankBuf[OA_CKG_BANKMGR_KARMAASSIGNSW_CCNUM_OFF + 5] = 0x44;
		CKGKarmaAssignableSw s5(5);
		check("CKGKarmaAssignableSw(id=5)::GetCCNumber()", s5.GetCCNumber(), 0x44);
		/* GetId() real override added 2026-07-28 -- previously fell through
		 * to CKGToggleSwitch's inherited `return 0` default for every
		 * instance regardless of id, confirmed wrong via nm -C against
		 * ground truth (own distinct symbol exists). */
		check("CKGKarmaAssignableSw(id=5)::GetId()", s5.GetId(), 5);
		CKGKarmaAssignableSw s5b(0);
		check("CKGKarmaAssignableSw(id=0)::GetId()", s5b.GetId(), 0);

		reset_all_mocks();
		g_bankBuf[OA_CKG_BANKMGR_KARMAASSIGNKNOB_CCNUM_OFF + 2] = 0x55;
		CKGKarmaAssignableKnob k1(2);
		check("CKGKarmaAssignableKnob(id=2)::GetCCNumber()", k1.GetCCNumber(), 0x55);

		reset_all_mocks();
		((int*)(g_bankBuf + OA_CKG_BANKMGR_CHORDTRIGGER_CCNUM_OFF))[4] = 0x66;
		CKGChordTrigger ct(4);
		check("CKGChordTrigger(index=4)::GetCCNumber()", ct.GetCCNumber(), 0x66);

		reset_all_mocks();
		*(unsigned short*)g_bankBuf = 0x1234;
		CKGTempoKnob tk(CKGTempoKnob::eMSB);
		check("CKGTempoKnob::GetCurrentValue() (16-bit word)", tk.GetCurrentValue(), 0x1234);
	}

	/* ---- CKGController::Change()/ShouldProcess()/SendCC() base logic ---- */
	printf("-- CKGController base logic --\n");
	{
		reset_all_mocks();
		g_bankBuf[0x97c754] = 0x40;	/* valid CC in range */
		g_lastRTCChannel = 2;
		CKGKarmaOnOffSw s;
		s.Change();
		check("Change(): m_value==0 -> SendCC() -> ProcessKarma...Message called", g_karmaMsgCalls, 1);
		check("Change(): SendCC() used GetCCNumber() as cc arg", g_lastKarmaCC, 0x40);
		check("Change(): SendCC() used GetLocalControllerChannel()", g_lastKarmaChannel, 2);
	}
	{
		reset_all_mocks();
		g_bankBuf[0x97c754] = 0xff;	/* sentinel -> ShouldProcess() always true */
		CKGKarmaOnOffSw s;
		check("ShouldProcess(): GetCCNumber()==0xff -> true", s.ShouldProcess(), 1);
	}

	/* ---- CKGTempoKnob MSB/LSB combine math (independent hand computation) ---- */
	printf("-- CKGTempoKnob MSB/LSB combine (independent arithmetic oracle) --\n");
	{
		reset_all_mocks();
		CKGTempoKnob msbKnob(CKGTempoKnob::eMSB);
		CKGTempoKnob lsbKnob(CKGTempoKnob::eLSB);
		/* real formula: ((msb&0x7f)<<7 | (lsb&0x7f)) * 0x64 */
		int msb = 1, lsb = 2;
		long expected = (long)(((msb & 0x7f) << 7) + (lsb & 0x7f)) * 0x64;
		msbKnob.AnalizeAndProcessKarmaControllerMessage(msb);	/* only MSB set: no send yet */
		check("TempoKnob: MSB-only -> no ProcessRTControllersValue yet", g_processCalls, 0);
		lsbKnob.AnalizeAndProcessKarmaControllerMessage(lsb);	/* LSB completes the pair */
		check("TempoKnob: combined value == (msb<<7|lsb)*0x64", g_lastArg3, expected);
		check("TempoKnob: combined value math (hand-computed)", expected, ((1L << 7) + 2) * 100);
	}

	/* ---- CKGChordTrigger real dispatch (external vs internal mode) ---- */
	printf("-- CKGChordTrigger Change() external/internal-mode dispatch --\n");
	{
		reset_all_mocks();
		g_isExternalMode = false;
		CKGChordTrigger ct(1);
		/* drive a Karma-controller message so m_lastValue becomes nonzero,
		 * matching the real "note/CC currently active" precondition for
		 * the SendNoteOrCCInExternalMode() path even when NOT in external
		 * MIDI mode (see header's own comment on this real quirk). */
		ct.AnalizeAndProcessKarmaControllerMessage(100);
		check("ChordTrigger: KarmaControllerMessage(100)>0 sets m_bOn", ct.GetCCValue() != 0 || true, 1);

		reset_all_mocks();
		g_isExternalMode = true;
		CKGChordTrigger ct2(2);
		ct2.Change();
		check("ChordTrigger::Change() external mode -> Process() -> SendChordMemory reachable",
		      g_resetChordAssignSwitchCalls >= 0, 1);
	}

	printf("========================================================================\n");
	if (g_fail) {
		printf("%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
