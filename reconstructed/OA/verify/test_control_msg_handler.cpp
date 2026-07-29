// SPDX-License-Identifier: GPL-2.0
/*
 * test_control_msg_handler.cpp  -  host-side KAT for CSTGControlMsgHandler
 * (src/init/control_msg_handler.cpp).
 *
 * Deliberately self-contained: links ONLY control_msg_handler.cpp. Every
 * external dependency (CSTGGlobal/CSTGControllerRTData/CSTGMidiDispatcher/
 * CSTGMidiPortManager/CSTGMessageProcessor/CPowerOffTimer/CSTGFrontPanel/
 * CSTGVoiceAllocator/CSTGDiskCostManager/CSTGAudioManager/CSTGCPUInfo/
 * CSTGProgramBank/CLoadBalancer/STGAPIFrontPanelStatus/the keybed free
 * functions) is call-tracked via a local mock instead of linking the
 * real reconstruction -- same pattern as test_power_off_timer.cpp.
 * CSTGAudioDriverInterface is the one REAL `virtual` base in this
 * project (oa_engine.h) -- exercised via a minimal local concrete
 * subclass (TestAudioDriver) that records which vtable slot fired.
 */

#include <cstdio>
#include <cstring>
#include "oa_control_msg_handler.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}
static void section(const char *name) { printf("-- %s --\n", name); }

/* ---- link-satisfying globals ---- */
CSTGControllerRTData *CSTGControllerRTData::sInstance;
CSTGGlobal *CSTGGlobal::sInstance;
CSTGMidiPortManager *CSTGMidiPortManager::sInstance;
CSTGMessageProcessor *CSTGMessageProcessor::sInstance;
CPowerOffTimer *CPowerOffTimer::sInstance;
CSTGFrontPanel *CSTGFrontPanel::sInstance;
CSTGVoiceAllocator *CSTGVoiceAllocator::sInstance;
CSTGDiskCostManager *CSTGDiskCostManager::sInstance;
CSTGAudioManager *CSTGAudioManager::sInstance;
CSTGCPUInfo *CSTGCPUInfo::sInstance;
CLoadBalancer *CLoadBalancer::sInstance;
CSTGAudioDriverInterface *CSTGAudioDriverInterface::sInstance;
unsigned char *STGAPIFrontPanelStatus::sInstance;

/* CSTGMidiDispatcher only needs sInstance -- declared in oa_global.h,
 * touched only via raw offset cast, same treatment as the others. */
CSTGMidiDispatcher *CSTGMidiDispatcher::sInstance;

/* CSTGAudioDriverInterface's constructor and pure virtual dtor still
 * need SOME definition for TestAudioDriver's own ctor/dtor to link
 * against (matches what managers.cpp already does for the real build).
 * All virtuals -- pure AND non-pure -- are overridden in TestAudioDriver
 * below so nothing needs a base-class virtual-method definition too. */
CSTGAudioDriverInterface::CSTGAudioDriverInterface() { }
CSTGAudioDriverInterface::~CSTGAudioDriverInterface() { }
/* Out-of-line base-class virtual definitions -- needed purely so THIS
 * translation unit becomes the "key function" TU that emits
 * CSTGAudioDriverInterface's own vtable/typeinfo (required for
 * TestAudioDriver's base subobject construction/RTTI to link, even
 * though every one of these is fully overridden by TestAudioDriver
 * below and never actually called). */
int CSTGAudioDriverInterface::Initialize() { return 0; }
void CSTGAudioDriverInterface::Start() { }
void CSTGAudioDriverInterface::Reset() { }
void CSTGAudioDriverInterface::WriteAudioOuts() { }
void CSTGAudioDriverInterface::KeepSynchronized() { }
unsigned int CSTGAudioDriverInterface::GetNumDriverOutputChannels() const { return 0; }
unsigned int CSTGAudioDriverInterface::GetNumDriverInputChannels() const { return 0; }
void CSTGAudioDriverInterface::IncrementSTGDMABufferCounter() { }
void CSTGAudioDriverInterface::IncrementDriverDMABufferCounter() { }
bool CSTGAudioDriverInterface::STGRequiredToFillAnotherDMABuffer() const { return false; }

static int g_muteAll, g_unmuteAll, g_muteOut, g_unmuteOut;
class TestAudioDriver : public CSTGAudioDriverInterface {
public:
	int Initialize() override { return 0; }
	void Start() override { }
	void Reset() override { }
	void *GetAudioInputFromDriver() override { return 0; }
	void WriteAudioOutsAndWait() override { }
	void WriteAudioOuts() override { }
	void KeepSynchronized() override { }
	unsigned int GetNumDriverOutputChannels() const override { return 0; }
	unsigned int GetNumDriverInputChannels() const override { return 0; }
	void MuteAllAudio() override { g_muteAll++; }
	void UnmuteAllAudio() override { g_unmuteAll++; }
	void MuteAudioOutputs() override { g_muteOut++; }
	void UnmuteAudioOutputs() override { g_unmuteOut++; }
	void MuteAudioInputs() override { }
	void UnmuteAudioInputs() override { }
	void MuteAudioOutput(unsigned int) override { }
	void UnmuteAudioOutput(unsigned int) override { }
	void MuteAudioInput(unsigned int) override { }
	void UnmuteAudioInput(unsigned int) override { }
	void IncrementSTGDMABufferCounter() override { }
	void IncrementDriverDMABufferCounter() override { }
	bool STGRequiredToFillAnotherDMABuffer() const override { return false; }
};

/* ---- CSTGGlobal method mocks ---- */
static int g_setModeCalls; static int g_lastMode; static unsigned int g_lastModeSource;
void CSTGGlobal::SetMode(int mode, unsigned int source)
{ g_setModeCalls++; g_lastMode = mode; g_lastModeSource = source; }

static int g_beginPerfCalls; static int g_lastPerfMode;
static unsigned int g_lastPerfV1, g_lastPerfV2, g_lastPerfSrc;
void CSTGGlobal::BeginPerformanceChange(int mode, unsigned int v1, unsigned int v2, unsigned int src)
{ g_beginPerfCalls++; g_lastPerfMode = mode; g_lastPerfV1 = v1; g_lastPerfV2 = v2; g_lastPerfSrc = src; }

static int g_beginSlotCalls; static unsigned int g_lastSlotSet, g_lastSlotSlot, g_lastSlotSrc;
void CSTGGlobal::BeginSetListSlotChange(unsigned int v1, unsigned int v2, unsigned int src)
{ g_beginSlotCalls++; g_lastSlotSet = v1; g_lastSlotSlot = v2; g_lastSlotSrc = src; }

static int g_useGlobalCalls; static bool g_lastUseGlobal;
void CSTGGlobal::SetUseGlobalAudioInputSettings(bool v) { g_useGlobalCalls++; g_lastUseGlobal = v; }

static int g_testModeCalls; static bool g_lastTestMode;
void CSTGGlobal::SetNKS4TestModeFlag(bool v) { g_testModeCalls++; g_lastTestMode = v; }

static int g_splitLayerCalls; static bool g_lastSplitLayer;
void CSTGGlobal::SetSplitLayerWorkState(bool v) { g_splitLayerCalls++; g_lastSplitLayer = v; }

static int g_editCtxCalls; static int g_lastEditType; static unsigned int g_lastEditValue;
void CSTGGlobal::SetEditInContextState(int type, unsigned int value)
{ g_editCtxCalls++; g_lastEditType = type; g_lastEditValue = value; }

/* ---- CSTGControllerRTData mock ---- */
static int g_karmaCalls; static int g_lastCcNo; static unsigned char g_lastCcVal;
void CSTGControllerRTData::SendKarmaCCToKG(int ccNo, unsigned char value)
{ g_karmaCalls++; g_lastCcNo = ccNo; g_lastCcVal = value; }

/* ---- CPowerOffTimer mocks ---- */
static int g_updTimeoutCalls; static unsigned int g_lastTimeoutVal;
void CPowerOffTimer::UpdateTimeoutValue(unsigned int v) { g_updTimeoutCalls++; g_lastTimeoutVal = v; }
static int g_prepCompleteCalls;
void CPowerOffTimer::PowerOffPrepComplete() { g_prepCompleteCalls++; }
static int g_beginLongCalls;
void CPowerOffTimer::BeginLongProcess() { g_beginLongCalls++; }
static int g_endLongCalls;
void CPowerOffTimer::EndLongProcess() { g_endLongCalls++; }

/* ---- CSTGVoiceAllocator mocks ---- */
static int g_stealCalls, g_freeStolenCalls;
void CSTGVoiceAllocator::StealAllVoices() { g_stealCalls++; }
void CSTGVoiceAllocator::FreeStolenVoices() { g_freeStolenCalls++; }

/* ---- CSTGMessageProcessor mocks ---- */
static int g_startDlCalls, g_endDlCalls;
void CSTGMessageProcessor::StartDownload() { g_startDlCalls++; }
void CSTGMessageProcessor::EndDownload() { g_endDlCalls++; }

/* ---- CSTGProgramBank mock ---- */
static int g_changeBankCalls; static unsigned int g_lastBankType; static void *g_lastBankThis;
void CSTGProgramBank::ChangeBankType(unsigned int bankType)
{ g_changeBankCalls++; g_lastBankType = bankType; g_lastBankThis = this; }

/* ---- extern "C" mocks ---- */
static int g_pushCalls; static unsigned char g_lastPushBuf[32]; static int g_lastPushSize;
extern "C" void PushMessage(void *msg)
{
	g_pushCalls++;
	unsigned short sz = *(unsigned short *)msg;
	g_lastPushSize = sz;
	if (sz > sizeof(g_lastPushBuf)) sz = sizeof(g_lastPushBuf);
	memcpy(g_lastPushBuf, msg, sz);
}

static int g_writeCmdCalls; static int g_lastCmd;
extern "C" int OmapNKS4OutputFifo_WriteCommand(int command)
{ g_writeCmdCalls++; g_lastCmd = command; return 0; }

static int g_printkCalls; static char g_lastPrintk[128];
extern "C" __attribute__((regparm(0))) void rt_printk(const char *fmt, ...)
{ g_printkCalls++; strncpy(g_lastPrintk, fmt, sizeof(g_lastPrintk) - 1); }

static int g_startScanCalls;
extern "C" void COmapNKS4Driver_StartScanning(void) { g_startScanCalls++; }

static unsigned char g_keybedBuf[0x0c00];
extern "C" unsigned char *CSTGKeybedInterface_sInstance(void) { return g_keybedBuf; }

static int g_sendByteCalls; static unsigned char g_lastByte;
extern "C" void CSTGKeybedInterface_SendByte(unsigned char v) { g_sendByteCalls++; g_lastByte = v; }

static int g_enterKcmCalls, g_exitKcmCalls;
extern "C" void CSTGKeybedInterface_EnterKeyCheckMode(void) { g_enterKcmCalls++; }
extern "C" void CSTGKeybedInterface_ExitKeyCheckMode(void) { g_exitKcmCalls++; }

static int g_usbPortCalls; static int g_lastUsbPort; static bool g_lastUsbEnable;
extern "C" void CSTGKeybedInterface_EnableUSBPort(int port, bool enable)
{ g_usbPortCalls++; g_lastUsbPort = port; g_lastUsbEnable = enable; }

static int g_rearLedCalls; static bool g_lastRearLed;
extern "C" void CSTGKeybedInterface_EnableRearLED(bool enable) { g_rearLedCalls++; g_lastRearLed = enable; }

/* ---- fixtures ---- */
static unsigned char g_globalBuf[0x2000];
static unsigned char g_ctrlRtBuf[0x60];
static unsigned char g_midiDispBuf[0x200];
static unsigned char g_midiPortBuf[0x220];
static unsigned char g_msgProcBuf[0x1040];
static unsigned char g_potBuf[28];
static unsigned char g_panelBuf[0x29200];
static unsigned char g_frontPanelBuf[0x200];
static unsigned char g_loadBalBuf[0x100];
static unsigned char g_diskMgrBuf[72];
static unsigned char g_cpuInfoBuf[0x24];
static unsigned char g_audioMgrBuf[0x100];
static TestAudioDriver g_audioDriver;
static CSTGDrumPadInterface g_drumPad;

static void resetFixture()
{
	memset(g_globalBuf, 0, sizeof(g_globalBuf));
	CSTGGlobal::sInstance = (CSTGGlobal *)g_globalBuf;
	memset(g_ctrlRtBuf, 0, sizeof(g_ctrlRtBuf));
	CSTGControllerRTData::sInstance = (CSTGControllerRTData *)g_ctrlRtBuf;
	memset(g_midiDispBuf, 0, sizeof(g_midiDispBuf));
	CSTGMidiDispatcher::sInstance = (CSTGMidiDispatcher *)g_midiDispBuf;
	memset(g_midiPortBuf, 0, sizeof(g_midiPortBuf));
	CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)g_midiPortBuf;
	memset(g_msgProcBuf, 0, sizeof(g_msgProcBuf));
	CSTGMessageProcessor::sInstance = (CSTGMessageProcessor *)g_msgProcBuf;
	memset(g_potBuf, 0, sizeof(g_potBuf));
	CPowerOffTimer::sInstance = (CPowerOffTimer *)g_potBuf;
	memset(g_panelBuf, 0, sizeof(g_panelBuf));
	STGAPIFrontPanelStatus::sInstance = g_panelBuf;
	memset(g_frontPanelBuf, 0, sizeof(g_frontPanelBuf));
	CSTGFrontPanel::sInstance = (CSTGFrontPanel *)g_frontPanelBuf;
	memset(g_loadBalBuf, 0, sizeof(g_loadBalBuf));
	CLoadBalancer::sInstance = (CLoadBalancer *)g_loadBalBuf;
	memset(g_diskMgrBuf, 0, sizeof(g_diskMgrBuf));
	CSTGDiskCostManager::sInstance = (CSTGDiskCostManager *)g_diskMgrBuf;
	memset(g_cpuInfoBuf, 0, sizeof(g_cpuInfoBuf));
	*(float *)(g_cpuInfoBuf + 0x10) = 1.0f; /* field10 = identity scale for easy math */
	CSTGCPUInfo::sInstance = (CSTGCPUInfo *)g_cpuInfoBuf;
	memset(g_audioMgrBuf, 0, sizeof(g_audioMgrBuf));
	CSTGAudioManager::sInstance = (CSTGAudioManager *)g_audioMgrBuf;
	memset(g_keybedBuf, 0, sizeof(g_keybedBuf));
	CSTGVoiceAllocator::sInstance = (CSTGVoiceAllocator *)0x1; /* never dereferenced */
	CSTGAudioDriverInterface::sInstance = &g_audioDriver;
	CSTGDrumPadInterface::sInstance = &g_drumPad;

	g_muteAll = g_unmuteAll = g_muteOut = g_unmuteOut = 0;
	g_setModeCalls = g_beginPerfCalls = g_beginSlotCalls = 0;
	g_useGlobalCalls = g_testModeCalls = g_splitLayerCalls = g_editCtxCalls = 0;
	g_karmaCalls = 0;
	g_updTimeoutCalls = g_prepCompleteCalls = g_beginLongCalls = g_endLongCalls = 0;
	g_stealCalls = g_freeStolenCalls = 0;
	g_startDlCalls = g_endDlCalls = 0;
	g_changeBankCalls = 0;
	g_pushCalls = g_writeCmdCalls = g_printkCalls = g_startScanCalls = 0;
	g_sendByteCalls = g_enterKcmCalls = g_exitKcmCalls = g_usbPortCalls = g_rearLedCalls = 0;
}

int main()
{
	section("SetModeHandler");
	{
		resetFixture();
		STGControlMsgDataModeChange p = { 2, 0x11 };
		CSTGControlMsgHandler h;
		h.SetModeHandler(&p, 0);
		check_eq("mode<=2 calls SetMode", g_setModeCalls, 1);
		check_eq("mode forwarded", g_lastMode, 2);
		check_eq("source forwarded", (long)g_lastModeSource, 0x11);

		resetFixture();
		STGControlMsgDataModeChange p2 = { 3, 0 };
		CSTGControlMsgHandler h2;
		h2.SetModeHandler(&p2, 0);
		check_eq("mode>2 does not call SetMode", g_setModeCalls, 0);
	}

	section("PerformanceChgHandler");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		STGControlMsgDataPerformanceChange valid0 = { 0, 0xd, 0x7f, 5 };
		h.PerformanceChgHandler(&valid0, 0);
		check_eq("type0 valid calls Begin", g_beginPerfCalls, 1);
		check_eq("type0 mode", g_lastPerfMode, 0);
		check_eq("type0 v1", (long)g_lastPerfV1, 0xd);
		check_eq("type0 v2", (long)g_lastPerfV2, 0x7f);
		check_eq("type0 src", (long)g_lastPerfSrc, 5);

		g_beginPerfCalls = 0;
		STGControlMsgDataPerformanceChange invalid0 = { 0, 0xe, 0x7f, 0 };
		h.PerformanceChgHandler(&invalid0, 0);
		check_eq("type0 invalid v1 skips", g_beginPerfCalls, 0);

		STGControlMsgDataPerformanceChange valid1sentinel = { 1, 0, 0xfffe, 0 };
		h.PerformanceChgHandler(&valid1sentinel, 0);
		check_eq("type1 sentinel valid calls Begin", g_beginPerfCalls, 1);

		g_beginPerfCalls = 0;
		STGControlMsgDataPerformanceChange valid2 = { 2, 0, 0xc7, 0 };
		h.PerformanceChgHandler(&valid2, 0);
		check_eq("type2 valid calls Begin", g_beginPerfCalls, 1);

		g_beginPerfCalls = 0;
		STGControlMsgDataPerformanceChange invalid2 = { 2, 1, 0xc7, 0 };
		h.PerformanceChgHandler(&invalid2, 0);
		check_eq("type2 invalid (arg!=0) skips", g_beginPerfCalls, 0);
	}

	section("SelectSetListSlotHandler");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		STGControlMsgDataSetListSlotChange p = { 0x7f, 0x7f, 3 };
		h.SelectSetListSlotHandler(&p, 0);
		check_eq("valid calls BeginSetListSlotChange", g_beginSlotCalls, 1);
		check_eq("set forwarded", (long)g_lastSlotSet, 0x7f);
		check_eq("slot forwarded", (long)g_lastSlotSlot, 0x7f);

		g_beginSlotCalls = 0;
		STGControlMsgDataSetListSlotChange bad = { 0x80, 0, 0 };
		h.SelectSetListSlotHandler(&bad, 0);
		check_eq("out-of-range set skips", g_beginSlotCalls, 0);
	}

	section("SliderCC18EnableHandler (bit0 set/clear, other bits preserved)");
	{
		resetFixture();
		g_ctrlRtBuf[0x49] = 0xAA; /* other bits set */
		CSTGControlMsgHandler h;
		STGMsgDataOneParam p1 = { 1 };
		h.SliderCC18EnableHandler(&p1, 0);
		check_eq("bit0 set, others preserved", g_ctrlRtBuf[0x49], 0xAB);
		STGMsgDataOneParam p0 = { 0 };
		h.SliderCC18EnableHandler(&p0, 0);
		check_eq("bit0 clear, others preserved", g_ctrlRtBuf[0x49], 0xAA);
	}

	section("ProgramChangeEnableHandler / SysExFilerModeEnableHandler");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		STGMsgDataOneParam p1 = { 5 };
		h.ProgramChangeEnableHandler(&p1, 0);
		check_eq("MidiDispatcher+0xa2", g_midiDispBuf[0xa2], 1);
		STGMsgDataOneParam p0 = { 0 };
		h.SysExFilerModeEnableHandler(&p0, 0);
		check_eq("MidiPortManager+2", g_midiPortBuf[2], 0);
	}

	section("NKS4TestModeEnableHandler / UseGlobalAudioInputSettings");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		STGMsgDataOneParam p1 = { 9 };
		h.NKS4TestModeEnableHandler(&p1, 0);
		check_eq("SetNKS4TestModeFlag(true)", g_testModeCalls, 1);
		check_eq("value true", g_lastTestMode, true);
		h.UseGlobalAudioInputSettings(&p1, 0);
		check_eq("SetUseGlobalAudioInputSettings(true)", g_useGlobalCalls, 1);
	}

	section("SetProgramBankTypeHandler");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		STGMsgDataTwoParam p = { 6, 2 }; /* bankId=6, bankType=2 */
		h.SetProgramBankTypeHandler(&p, 0);
		check_eq("ChangeBankType called", g_changeBankCalls, 1);
		check_eq("bankType forwarded", (long)g_lastBankType, 2);
		void *expected = (void *)((unsigned long)g_globalBuf + 0x132e4d0u + 6u * 0x67603u);
		check_eq("this = CSTGGlobal+0x132e4d0+6*0x67603", (long)g_lastBankThis, (long)expected);
	}

	section("SetChordAssignState / 6 karma-CC transport switches");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		STGMsgDataOneParam pOn = { 1 }, pOff = { 0 };

		h.SetChordAssignState(&pOn, 0);
		check_eq("ChordAssign ccNo", g_lastCcNo, 0x02);
		check_eq("ChordAssign value on", g_lastCcVal, 0x7f);

		h.SetPauseSwitch(&pOn, 0);
		check_eq("Pause ccNo", g_lastCcNo, 0x2d);
		h.SetFFSwitch(&pOn, 0);
		check_eq("FF ccNo", g_lastCcNo, 0x25);
		h.SetRewSwitch(&pOn, 0);
		check_eq("Rew ccNo", g_lastCcNo, 0x26);
		h.SetLocateSwitch(&pOn, 0);
		check_eq("Locate ccNo", g_lastCcNo, 0x2c);
		h.SetRecSwitch(&pOn, 0);
		check_eq("Rec ccNo", g_lastCcNo, 0x2b);
		h.SetPlayStopSwitch(&pOff, 0);
		check_eq("PlayStop ccNo", g_lastCcNo, 0x2a);
		check_eq("PlayStop value off", g_lastCcVal, 0x00);
		check_eq("karma calls total", g_karmaCalls, 7);
	}

	section("EnableSendingMidiParams / EnableReceivingMidiParams / SetEditInContextState / SetSplitLayerWorkState");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		STGMsgDataOneParam p1 = { 1 };
		h.EnableSendingMidiParams(&p1, 0);
		check_eq("MessageProcessor+0x56", g_msgProcBuf[0x56], 1);
		h.EnableReceivingMidiParams(&p1, 0);
		check_eq("MessageProcessor+0x57", g_msgProcBuf[0x57], 1);
		STGMsgDataTwoParam pe = { 3, 0x99 };
		h.SetEditInContextState(&pe, 0);
		check_eq("edit ctx type", g_lastEditType, 3);
		check_eq("edit ctx value", (long)g_lastEditValue, 0x99);
		h.SetSplitLayerWorkState(&p1, 0);
		check_eq("SetSplitLayerWorkState(true)", g_splitLayerCalls, 1);
	}

	section("EnableAudioMetering / EnableReceivingMidi / EnableOnScreenTouchPads");
	{
		resetFixture();
		g_globalBuf[0x6db] = 1;
		CSTGControlMsgHandler h;
		STGMsgDataOneParam p0 = { 0 };
		h.EnableAudioMetering(&p0, 0);
		check_eq("Global+0x6db cleared", g_globalBuf[0x6db], 0);
		STGMsgDataOneParam p1 = { 1 };
		h.EnableReceivingMidi(&p1, 0);
		check_eq("MidiPortManager+0", g_midiPortBuf[0], 1);
		h.EnableOnScreenTouchPads(&p1, 0);
		check_eq("FrontPanel+0x104", g_frontPanelBuf[0x104], 1);
	}

	section("OMAP NKS4 hardware command handlers");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		STGMsgDataOneParam pv = { 0x42 };

		h.SetLCDBrightness(&pv, 0);
		check_eq("LCDBrightness cmd", g_lastCmd, (int)0xc7420000u);

		h.ResetOMAPModules(&pv, 0);
		check_eq("ResetOMAPModules cmd", g_lastCmd, (int)0x06420000u);

		h.SetRearLEDState(&pv, 0); /* hwVer != 3 default -> OMAP command path */
		check_eq("SetRearLEDState (non-NKS4) cmd", g_lastCmd, (int)0x0a420000u);

		h.EnableAfterTouch(&pv, 0);
		check_eq("EnableAfterTouch cmd (low byte)", g_lastCmd, (int)0x10000042u);

		g_writeCmdCalls = 0;
		h.DeactivateEncoderAccelerator(&pv, 0);
		check_eq("DeactivateEncoderAccelerator ignores param", g_lastCmd, (int)0x0f000000u);
		check_eq("DeactivateEncoderAccelerator calls once", g_writeCmdCalls, 1);
	}

	section("ForceErPShutdown");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		STGMsgDataOneParam pv = { 0x1234 };
		h.ForceErPShutdown(&pv, 0);
		check_eq("cmd word", g_lastCmd, (int)0x09001234u);
		check_eq("printk called once", g_printkCalls, 1);
		check_eq("printk string matches", strcmp(g_lastPrintk, "We would have powered off here!!!\n") == 0, 1);
	}

	section("ErP long-process bookkeeping");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		STGMsgDataOneParam p0 = { 0 };
		h.ErPNotifySystemActivity(&p0, 0);
		check_eq("PowerOffTimer activity flag", g_potBuf[0], 1);

		STGMsgDataOneParam pv = { 300 };
		h.UpdateErPTimeout(&pv, 0);
		check_eq("UpdateTimeoutValue forwarded", (long)g_lastTimeoutVal, 300);

		h.ReadyForErPShutdown(&p0, 0);
		check_eq("PowerOffPrepComplete", g_prepCompleteCalls, 1);
		h.BeginLongErPActivity(&p0, 0);
		check_eq("BeginLongProcess", g_beginLongCalls, 1);
		h.EndLongErPActivity(&p0, 0);
		check_eq("EndLongProcess", g_endLongCalls, 1);

		STGMsgDataOneParam pOn = { 1 };
		h.SetSendingBulkDump(&pOn, 0);
		check_eq("bulk dump on: midi flag", g_midiPortBuf[3], 1);
		check_eq("bulk dump on: BeginLongProcess", g_beginLongCalls, 2);
		STGMsgDataOneParam pOff = { 0 };
		h.SetSendingBulkDump(&pOff, 0);
		check_eq("bulk dump off: midi flag", g_midiPortBuf[3], 0);
		check_eq("bulk dump off: EndLongProcess", g_endLongCalls, 2);
	}

	section("Keybed hardware handlers");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		STGMsgDataOneParam pOn = { 1 };
		h.TakeOverKeybedComm(&pOn, 0);
		check_eq("keybed gate1 set", g_keybedBuf[KEYBED_OFF_ENQUEUE_GATE1], 1);

		STGMsgDataOneParam pByte = { 0x7f };
		h.SendKeybedByte(&pByte, 0);
		check_eq("SendByte forwarded", g_lastByte, 0x7f);

		h.EnterKeyCheckMode(&pOn, 0);
		check_eq("param!=0 -> Enter", g_enterKcmCalls, 1);
		check_eq("param!=0 -> Enter (Exit not called)", g_exitKcmCalls, 0);
		STGMsgDataOneParam pOff = { 0 };
		h.EnterKeyCheckMode(&pOff, 0);
		check_eq("param==0 -> Exit", g_exitKcmCalls, 1);

		h.ReenableUSBPorts(&pOn, 0);
		check_eq("EnableUSBPort called twice", g_usbPortCalls, 2);
		check_eq("last call port=1", g_lastUsbPort, 1);
		check_eq("last call enable=true", g_lastUsbEnable, true);

		g_panelBuf[STGAPI_OFF_NKS4_HW_VERSION] = 3;
		h.SetRearLEDState(&pOn, 0);
		check_eq("hwVer==3 -> EnableRearLED, not OMAP cmd", g_rearLedCalls, 1);
	}

	section("StartSTG");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		h.StartSTG(0, 0);
		check_eq("COmapNKS4Driver_StartScanning called", g_startScanCalls, 1);
		check_eq("MidiPortManager+0 set", g_midiPortBuf[0], 1);
		check_eq("LoadBalancer+0xa4 set", g_loadBalBuf[0xa4], 1);
		check_eq("keybed gate2 set", g_keybedBuf[KEYBED_OFF_DISPATCH_GATE2], 1);
	}

	section("MuteDAC / MuteADC (real virtual dispatch)");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		STGMsgDataOneParam pOn = { 1 }, pOff = { 0 };
		h.MuteDAC(&pOn, 0);
		check_eq("MuteDAC(1) -> MuteAllAudio", g_muteAll, 1);
		h.MuteDAC(&pOff, 0);
		check_eq("MuteDAC(0) -> UnmuteAllAudio", g_unmuteAll, 1);
		h.MuteADC(&pOn, 0);
		check_eq("MuteADC(1) -> MuteAudioOutputs (real quirk)", g_muteOut, 1);
		h.MuteADC(&pOff, 0);
		check_eq("MuteADC(0) -> UnmuteAudioOutputs (real quirk)", g_unmuteOut, 1);
	}

	section("I2C debug commands (neutered on shipping firmware)");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		STGControlMsgDataTwoParam p2 = { 1, 2 };
		h.ReadI2CDevice(&p2, 0);
		check_eq("ReadI2CDevice pushes a reply", g_pushCalls, 1);
		check_eq("reply size", g_lastPushSize, 0x10);
		check_eq("reply subtype", *(unsigned int *)(g_lastPushBuf + 8), 0x13);

		g_pushCalls = 0;
		STGControlMsgDataThreeParam p3 = { 1, 2, 3 };
		h.WriteI2CDevice(&p3, 0);
		check_eq("WriteI2CDevice never pushes", g_pushCalls, 0);
	}

	section("ReadCPUUsagePeak / ReadFXUsagePeak / ReadDiskThroughputPeak");
	{
		resetFixture();
		CSTGControlMsgHandler h;

		/* CSTGAudioManager+0xc is an array of per-core stats pointers,
		 * indexed by (coreId+8). Point core 0's slot at a small local
		 * stats buffer. */
		static unsigned char coreStats[0x16f0];
		memset(coreStats, 0, sizeof(coreStats));
		*(int *)(coreStats + 0x16e8) = 5; /* raw accumulator */
		/* Index with the SAME native-pointer-width stride the code
		 * under test uses (`((unsigned char **)(audioMgr+0xc))[idx]`)
		 * -- NOT a hand-computed 4-byte stride. On the real 32-bit
		 * target this stride naturally IS 4 bytes (matching ground
		 * truth's own `ecx*4`); on this 64-bit host build it's 8.
		 * Using the wrong stride here (an earlier version of this
		 * fixture did) segfaults -- caught via a live crash, not
		 * anticipated in advance. */
		((void **)(g_audioMgrBuf + 0xc))[0 + 8] = coreStats;

		STGControlMsgDataOneParam pCore = { 0 };
		h.ReadCPUUsagePeak(&pCore, 0);
		check_eq("CPU peak pushes reply", g_pushCalls, 1);
		check_eq("CPU peak reply size", g_lastPushSize, 0x14);
		check_eq("CPU peak value = 5*100 (scale=1.0)",
			 *(unsigned int *)(g_lastPushBuf + 0x10), 500);
		check_eq("CPU peak coreId echoed", *(unsigned int *)(g_lastPushBuf + 0xc), 0);
		check_eq("CPU accumulator reset", *(int *)(coreStats + 0x16e8), 0);
		check_eq("CPU sentinel reset", *(unsigned int *)(coreStats + 0x16ec), 0xffffffffu);

		static unsigned char fxStats[0x50];
		memset(fxStats, 0, sizeof(fxStats));
		*(int *)(fxStats + 0x48) = 7;
		*(void **)(g_audioMgrBuf + 0x3c) = fxStats;
		g_pushCalls = 0;
		h.ReadFXUsagePeak(0, 0);
		check_eq("FX peak pushes reply", g_pushCalls, 1);
		check_eq("FX peak reply size", g_lastPushSize, 0x10);
		check_eq("FX peak value = 7*100", *(unsigned int *)(g_lastPushBuf + 0xc), 700);
		check_eq("FX accumulator reset", *(int *)(fxStats + 0x48), 0);

		*(float *)(g_diskMgrBuf + 0x34) = 12.9f;
		g_pushCalls = 0;
		h.ReadDiskThroughputPeak(0, 0);
		check_eq("Disk peak pushes reply", g_pushCalls, 1);
		check_eq("Disk peak truncates toward zero", *(unsigned int *)(g_lastPushBuf + 0xc), 12);
		check_eq("Disk peak accumulator reset", *(float *)(g_diskMgrBuf + 0x34) == 0.0f, 1);
	}

	section("StealAllVoices / StartDownloadHandler / EndDownloadHandler");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		h.StealAllVoices(0, 0);
		check_eq("StealAllVoices calls Steal", g_stealCalls, 1);
		check_eq("StealAllVoices calls FreeStolen", g_freeStolenCalls, 1);
		check_eq("StealAllVoices pushes reply", g_pushCalls, 1);

		h.StartDownloadHandler(0, 0);
		check_eq("StartDownload forwarded", g_startDlCalls, 1);
		h.EndDownloadHandler(0, 0);
		check_eq("EndDownload forwarded", g_endDlCalls, 1);
	}

	section("Constructor -- table install spot checks");
	{
		resetFixture();
		CSTGControlMsgHandler h;
		check_eq("sInstance installed", (long)CSTGControlMsgHandler::sInstance, (long)&h);
		check_eq("replyTag default", h._replyTag, 0x36);
		check_eq("msgHandlerTable points at sMsgHandler",
			 (long)h._msgHandlerTable, (long)&CSTGControlMsgHandler::sMsgHandler);
		check_eq("slot 0x68/4 = &SetLCDBrightness (install-only pointer non-null)",
			 CSTGControlMsgHandler::sMsgHandler[0x68 / 4] != 0, 1);
		check_eq("slot 0x10/4 = shared fallback (non-null)",
			 CSTGControlMsgHandler::sMsgHandler[0x10 / 4] != 0, 1);
	}

	section("CSTGDrumPadInterface::Run (round 67, confirmed empty)");
	{
		CSTGDrumPadInterface::Run();
		check_eq("Run(): confirmed-empty body doesn't crash", 1, 1);
	}

	section("~CSTGControlMsgHandler (round 69): resets _vtablePtr to base's slot");
	{
		/* Reading `->_vtablePtr` AFTER an explicit dtor call is technically
		 * UB (object lifetime already ended) -- GCC's optimizer proved
		 * this at -O2 and compiled the dtor to a bare `ret`. Read the raw
		 * bytes instead, matching the project's own established pattern
		 * (test_managers.cpp's CSTGStreamingEvent dtor check). */
		resetFixture();
		unsigned char *buf = new unsigned char[sizeof(CSTGControlMsgHandler)];
		CSTGControlMsgHandler *h = new (buf) CSTGControlMsgHandler();
		check_eq("ctor installed own vtable slot",
			 *(long *)buf, (long)(_ZTV21CSTGControlMsgHandler + 8));
		h->~CSTGControlMsgHandler();
		check_eq("dtor resets to shared CSTGMessageHandler base vtable slot",
			 *(long *)buf, (long)(_ZTV18CSTGMessageHandler + 8));
		delete[] buf;
	}

	section("CSTGMessageHandler (round 71): SendMidiParam + own dtor");
	{
		/* CSTGMessageHandler itself has no declared data members (every
		 * subclass supplies its own layout) -- allocate a plain raw
		 * buffer rather than sizeof(CSTGMessageHandler) (which would be
		 * 1 byte, not enough to hold the 4-byte vtable-pointer write). */
		CSTGMessageHandler::SendMidiParam((void *)0);
		check_eq("SendMidiParam(NULL) doesn't crash (confirmed empty body)", 1, 1);

		unsigned char buf[16];
		CSTGMessageHandler *h = reinterpret_cast<CSTGMessageHandler *>(buf);
		h->~CSTGMessageHandler();
		check_eq("dtor resets to its own vtable slot",
			 *(long *)buf, (long)(_ZTV18CSTGMessageHandler + 8));
	}

	printf("%s (%d failed)\n", g_fail ? "FAILED" : "PASSED", g_fail);
	return g_fail ? 1 : 0;
}
