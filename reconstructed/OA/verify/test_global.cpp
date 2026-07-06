// SPDX-License-Identifier: GPL-2.0
/*
 * test_global.cpp  -  host-side known-answer tests for CSTGGlobal's
 * `UpdateXXX(CSTGMessageContext&, STGConvertedParam&)` message-handler
 * family (see include/oa_global.h). A dedicated file, separate from
 * test_engine.cpp, since this family has ~150 members and is expected to
 * grow across future sessions -- keeping it apart from CSTGEngine's own
 * (much smaller, stable) test suite.
 *
 * CSTGGlobal's real object is confirmed to reach ~43.6MB in confirmed
 * field offsets (see oa_global.h) -- every test here allocates a real
 * heap buffer that size, zeroed, rather than a stack object.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_engine.h"
#include "oa_engine_init.h"

/* STGAPIFrontPanelStatus::sInstance (used by CompletePerformanceChange,
 * sec 10.108) -- declared locally rather than pulling in the whole
 * oa_setup_global_resources.h, which redeclares several types this file
 * already mocks on its own. */
struct STGAPIFrontPanelStatus {
	static unsigned char *sInstance;
};

/* Local minimal stand-in matching oa_setup_global_resources.h's own
 * CSTGFrontPanel (same mangled sInstance/RequestAnalogInputStatus
 * symbols) -- avoids including that header directly, which pulls in
 * oa_internal.h's own placement-new operator, conflicting with <new>
 * (already included above for the CSTGProgramModeProgramSlot tests). */
struct CSTGFrontPanel {
	static CSTGFrontPanel *sInstance;
	void RequestAnalogInputStatus(unsigned int deviceCode);
};

/*
 * Initialize()'s own confirmed real head/tail/count list-anchor fields
 * (+0x29c98f4/+0x29c98f8/+0x29c98fc) are genuinely 32-bit pointers on
 * the real target, tightly packed with no gaps -- but this is a
 * 64-bit host, where a plain calloc'd buffer's address (and offsets
 * into it) routinely exceed 32 bits, so an 8-byte host pointer write
 * at +0x29c98f4 would span into +0x29c98f8, and +0x29c98f8's own
 * write would span into +0x29c98fc, corrupting the tightly-packed
 * neighbor -- not a bug in the reconstruction (which correctly
 * matches the real target's own 4-byte-wide, ungapped field layout),
 * a test-harness concern only, caught via a real segfault while
 * building this test. Matches this project's own established
 * `mmap(..., MAP_32BIT, ...)` fix from test_setup_global_resources.cpp.
 */
static unsigned char *mmap32(unsigned long size)
{
	void *p = mmap(0, size, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	return (unsigned char *)p;
}

/* Linking managers.cpp in (needed for CSTGAudioBusManager::
 * SetLRBusIndivAssign, which UpdateLRBusIndivAssign calls) pulls in every
 * OTHER manager in that same translation unit too, and with it their own
 * host-mock requirements -- same mocks as test_managers.cpp, even though
 * this file's own tests don't exercise most of them. */
extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void) { return 24; }
extern "C" void *rtwrap_malloc(unsigned int size) { return malloc(size); }
extern "C" void rtwrap_pthread_mutex_init(void *, void *) { }
extern "C" void rtwrap_pthread_mutexattr_init(void *) { }
extern "C" int  get_pthread_recursive_attr_constant(void) { return 1; }
extern "C" void rtwrap_pthread_mutexattr_settype(void *, int) { }
extern "C" void rtwrap_pthread_mutexattr_destroy(void *) { }
extern "C" unsigned int get_sizeof_rtwrap_pthread_cond(void) { return 24; }
extern "C" void rtwrap_pthread_cond_init(void *, void *) { }
void CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors() { }
CSTGAudioManager::~CSTGAudioManager() { }
/* Sec 10.147: ~CSTGVoiceAllocator()/~CSTGMessageProcessor() are now real
 * (see managers.cpp) -- link-satisfying only, same reasoning as the
 * other manager mocks above (this file's own CSTGMessageProcessor::
 * sInstance/CSTGVoiceAllocator::sInstance usages are raw mmap32()
 * buffers cast to the pointer type, never a typed `delete`/destructor
 * call -- see the msgProc/voiceAlloc declarations throughout this file). */
extern "C" void rtwrap_pthread_mutex_destroy(void *) { }
extern "C" void rtwrap_free(void *) { }
/* CEffectorDatabase::~CEffectorDatabase() is now real too (sec 10.148,
 * see managers.cpp) -- link-satisfying only, nothing in this file
 * constructs one. */

/* Mocks for CSTGAudioInput's own confirmed-real, deliberately deferred
 * dependencies (sec 10.80). */
static int g_setHDRBusCalls, g_setFXCtrlBusCalls, g_setOutputBusCalls, g_setPanCalls;
static unsigned int g_lastMixerBusIndex;
static int g_lastMixerValue;
static float g_lastMixerPan;
void CSTGAudioInputMixerBase::SetHDRBus(unsigned int busIndex, int value)
{ g_setHDRBusCalls++; g_lastMixerBusIndex = busIndex; g_lastMixerValue = value; }
void CSTGAudioInputMixerBase::SetFXCtrlBus(unsigned int busIndex, int value)
{ g_setFXCtrlBusCalls++; g_lastMixerBusIndex = busIndex; g_lastMixerValue = value; }
void CSTGAudioInputMixerBase::SetOutputBus(unsigned int busIndex, int value)
{ g_setOutputBusCalls++; g_lastMixerBusIndex = busIndex; g_lastMixerValue = value; }
void CSTGAudioInputMixerBase::SetPan(unsigned int busIndex, float value)
{ g_setPanCalls++; g_lastMixerBusIndex = busIndex; g_lastMixerPan = value; }
static int g_setAudioInSoloCalls;
static unsigned int g_lastSoloSlot;
static bool g_lastSoloValue;
void CSTGControllerRTData::SetAudioInSolo(unsigned int slot, bool solo)
{ g_setAudioInSoloCalls++; g_lastSoloSlot = slot; g_lastSoloValue = solo; }
static int g_resetSendKnobsJumpCatchCalls;
void CSTGControllerRTData::ResetSendKnobsJumpCatch() { g_resetSendKnobsJumpCatchCalls++; }

/* Mocks for the sec 10.90 batch's own confirmed-real, deliberately
 * deferred dependencies. OnUseGlobalSettingsChanged is now real (sec
 * 10.134) -- its own dependency, UseSettings(), is mocked instead. */
static int g_useSettingsCalls;
static void *g_lastUseSettingsThis;
void CSTGAudioInput::UseSettings() { g_useSettingsCalls++; g_lastUseSettingsThis = this; }
static int g_notifyNKS4TestModeCalls;
void CSTGMidiPortManager::NotifyNKS4TestMode() { g_notifyNKS4TestModeCalls++; }
static int g_setTestModeCalls;
static bool g_lastTestModeValue;
extern "C" void COmapNKS4Driver_SetTestMode(int testMode)
{ g_setTestModeCalls++; g_lastTestModeValue = (testMode != 0); }
/* CSTGMidiQueue::GetNumWritableBytes() is now real (sec 10.150, see
 * src/engine/midi_queue_writer.cpp, added to this file's own Makefile
 * link line) -- no mock here any more. Its old hardcoded "always 0
 * writable bytes" behavior (never actually varied away from that
 * default anywhere in this file) is now reproduced via a tiny SHARED
 * fake ringCtl block (mask=0xffffffff, readerCount=0 -- makes the real
 * `(mask+1)-worstBacklog` formula wrap to exactly 0) that every
 * `midiPortMgr*` buffer below points its own `+0x208` field at, right
 * after allocation -- see SetupFakeRingCtl() and each call site. */
static unsigned char *g_fakeRingCtl;
static void SetupFakeRingCtl(unsigned char *midiPortMgr)
{
	if (!g_fakeRingCtl) {
		g_fakeRingCtl = mmap32(0x24);
		*(unsigned int *)(g_fakeRingCtl + 0x8) = 0xffffffff; /* mask */
		g_fakeRingCtl[0x20] = 0;                              /* readerCount */
	}
	*(unsigned int *)(midiPortMgr + 0x208) =
		(unsigned int)(unsigned long)g_fakeRingCtl;
}
/* Reads back what the real (now-real) SubmitPerfChangeRequest queues
 * into the confirmed pending slot -- the same observation point every
 * existing call site below uses in place of the old mock's own
 * g_lastPerfChangeRequest/g_submitPerfChangeRequestCalls, since the
 * MIDI queue is always reported congested (above) so every one of
 * these sites takes the real "queue into +0x2975168" branch. */
static CSTGPerfChangeRequest &PendingRequest(unsigned char *buf)
{ return *(CSTGPerfChangeRequest *)(buf + 0x2975168); }
/* CSetListSlot::Activate is now real (sec 10.141) -- see
 * src/engine/global.cpp; this file links global.cpp directly. Its own
 * real side effects (mgr->fieldAt(0x23f0)/fieldAt(0x23e0) copied from
 * the slot's own fields) are checked directly at each call site below
 * instead of a call counter. */
static int g_setListActivateCalls;
static void *g_lastSetListActivateThis;
void CSetList::Activate() { g_setListActivateCalls++; g_lastSetListActivateThis = this; }
static int g_onPerformanceActivateCalls;
static void *g_lastOnPerformanceActivateArg;
void CSTGControllerRTData::OnPerformanceActivate(CSTGPerformance &perf)
{ g_onPerformanceActivateCalls++; g_lastOnPerformanceActivateArg = &perf; }
/* NotifySoloChange is now real (sec 10.107) -- see src/engine/global.cpp.
 * Its own real vtable-slot-0x1b call target is tracked via a fake
 * vtable planted by the [45] test itself (this class isn't otherwise
 * modeled with a real vtable in this project). */
static int g_notifySoloChangeVtableCalls;
static void *g_lastNotifySoloChangeVtableThis;
static void NotifySoloChangeVtableFn(void *self)
{
	g_notifySoloChangeVtableCalls++;
	g_lastNotifySoloChangeVtableThis = self;
}
static int g_balanceStaticLoadCalls;
void CLoadBalancer::BalanceStaticLoad() { g_balanceStaticLoadCalls++; }
unsigned char *STGAPIFrontPanelStatus::sInstance;
/* SendPerfChangeToMidiOut is now real (sec 10.98) -- see
 * src/engine/global.cpp; its own confirmed-real, deliberately deferred
 * dependencies still need link-satisfying mocks here (not exercised by
 * this file's own tests yet). */
static int g_convertAliasPgmBankCalls, g_convertCombiBankCalls;
static int g_lastConvertBankId;
void USTGAliasBankTypes::ConvertAliasPgmBankToMidiBank(int bankId, char &out1, char &out2)
{ g_convertAliasPgmBankCalls++; g_lastConvertBankId = bankId; out1 = 0x11; out2 = 0x22; }
void USTGAliasBankTypes::ConvertCombiBankToMidiBank(int bankId, char &out1, char &out2)
{ g_convertCombiBankCalls++; g_lastConvertBankId = bankId; out1 = 0x33; out2 = 0x44; }
static int g_convertMidiToCombiCalls, g_convertMidiToAliasCalls;
static char g_lastMidiBankMsb, g_lastMidiBankLsb;
static int g_convertedBankIdReturn;
void USTGAliasBankTypes::ConvertMidiBankToCombiBank(char midiMsb, char midiLsb, int &outBankId)
{ g_convertMidiToCombiCalls++; g_lastMidiBankMsb = midiMsb; g_lastMidiBankLsb = midiLsb; outBankId = g_convertedBankIdReturn; }
void USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(char midiMsb, char midiLsb, int &outBankId)
{ g_convertMidiToAliasCalls++; g_lastMidiBankMsb = midiMsb; g_lastMidiBankLsb = midiLsb; outBankId = g_convertedBankIdReturn; }
static int g_shouldSyncCalls;
static bool g_shouldSyncReturn;
bool SKSTGGate_ShouldSyncExternalClock() { g_shouldSyncCalls++; return g_shouldSyncReturn; }
/* IncrementCombiIndex/IncrementAliasProgramIndex/DecrementCombiIndex/
 * DecrementAliasProgramIndex are now real (sec 10.127) -- see
 * src/engine/global.cpp; verified via their own real computed outputs
 * below, not a call-tracking mock. */
static int g_pushUnsolicitedMessageCalls;
static unsigned char g_lastUnsolicitedMessage[0x1c];
extern "C" void PushUnsolicitedMessage(void *msg)
{
	g_pushUnsolicitedMessageCalls++;
	/* Real messages in this cluster vary between 0x18 and 0x1c bytes
	 * (sec 10.98/10.108) with no separate size argument -- word0 is the
	 * confirmed real size marker, so read only that many bytes rather
	 * than risking an over-read past a caller's own smaller buffer. */
	unsigned short size = *(const unsigned short *)msg;
	if (size > sizeof(g_lastUnsolicitedMessage))
		size = sizeof(g_lastUnsolicitedMessage);
	memset(g_lastUnsolicitedMessage, 0, sizeof(g_lastUnsolicitedMessage));
	memcpy(g_lastUnsolicitedMessage, msg, size);
}

/* Mocks for the sec 10.92 batch's own confirmed-real, deliberately
 * deferred dependencies. StealingRequiresOneTickStall is now real (sec
 * 10.136) -- see src/engine/global.cpp; this file links global.cpp
 * directly. */
/* CancelAllCCSmoothers is now real (sec 10.130) -- see
 * src/engine/global.cpp. Its own confirmed real, deliberately deferred
 * dependency, FinalizeSmoother(), is mocked below with call tracking,
 * though every current test scenario keeps the smoother's own list
 * empty (no nodes to traverse), so it's never actually invoked yet. */
static int g_finalizeSmootherCalls;
void CSTGSmoother::FinalizeSmoother(void *, bool) { g_finalizeSmootherCalls++; }
static int g_channelValuesResetCalls;
static void *g_lastChannelValuesResetThis;
void CSTGChannelValues::Reset()
{ g_channelValuesResetCalls++; g_lastChannelValuesResetThis = this; }
/* SetPitchBend is now real (sec 10.128) -- see src/engine/global.cpp;
 * verified via its own real field writes (+0x5a0/+0x5a4/+0x5a8/+0x634),
 * not a call-tracking mock. */
static int g_setControllerValueCalls;
static void *g_lastSetControllerValueThis;
static unsigned char g_lastSetControllerValueCC;
static CSTGControllerValue g_lastSetControllerValueValue;
/* Full call history (not just the last call) -- PerfChangeControllerReset's
 * own test (sec 10.115, [50]) needs to inspect both calls made per
 * channel, not just whichever ran last. */
static unsigned char g_setControllerValueCCHistory[64];
static CSTGControllerValue g_setControllerValueHistory[64];
void CSTGChannelValues::SetControllerValue(unsigned char ccNumber, const CSTGControllerValue &value)
{
	g_lastSetControllerValueThis = this;
	g_lastSetControllerValueCC = ccNumber;
	g_lastSetControllerValueValue = value;
	if (g_setControllerValueCalls < 64) {
		g_setControllerValueCCHistory[g_setControllerValueCalls] = ccNumber;
		g_setControllerValueHistory[g_setControllerValueCalls] = value;
	}
	g_setControllerValueCalls++;
}
static int g_validateParamChangeCalls;
void CSTGParamsOwner::ValidateParamChange(CSTGMessageContext &, unsigned long, const CValue &)
{ g_validateParamChangeCalls++; }
static int g_freeSlotVoiceDataCalls;
static bool g_lastFreeSlotVoiceDataFlag;
/* Optional hook for [38]'s own GetFreeSlotVoiceData test: the real
 * FreeSlotVoiceData(true) call, when it happens, populates the free
 * list -- this lets that one test simulate the same effect so the
 * real retry loop actually terminates instead of spinning forever. */
static void (*g_freeSlotVoiceDataHook)(void *self);
/* EmergencyFreeAllVoices is now real (sec 10.138), and its own
 * dependency EmergencyFreeVoiceList is now ALSO real (sec 10.149, see
 * managers.cpp, linked directly into this binary) -- no mock of
 * EmergencyFreeVoiceList itself here any more (a stale mock here would
 * conflict with the real definition at link time). The real body's OWN
 * confirmed-real, deliberately-deferred dependencies (FreeVoice/
 * DoPendingMoveVoices) plus the rtwrap_pthread_mutex_lock/unlock pair it
 * calls are mocked here with counters instead, so the existing
 * assertions below can be rewritten to check the real body's own
 * observable effects (DoPendingMoveVoices is called EXACTLY once per
 * real EmergencyFreeVoiceList invocation, unconditionally, so it's the
 * proxy for "EmergencyFreeVoiceList really ran N times" that
 * g_emergencyFreeVoiceListCalls used to be under the old mock). */
static int g_mutexLockCalls, g_mutexUnlockCalls;
extern "C" void rtwrap_pthread_mutex_lock(void *) { g_mutexLockCalls++; }
extern "C" void rtwrap_pthread_mutex_unlock(void *) { g_mutexUnlockCalls++; }
static int g_freeVoiceCalls;
static void *g_lastFreeVoiceArg;
void CSTGVoiceAllocator::FreeVoice(CSTGVoice *voice)
{ g_freeVoiceCalls++; g_lastFreeVoiceArg = (void *)voice; }
static int g_doPendingMoveVoicesCalls;
void CSTGVoiceAllocator::DoPendingMoveVoices() { g_doPendingMoveVoicesCalls++; }
void CSTGSlotVoiceData::FreeSlotVoiceData(bool flag)
{
	g_freeSlotVoiceDataCalls++;
	g_lastFreeSlotVoiceDataFlag = flag;
	if (g_freeSlotVoiceDataHook)
		g_freeSlotVoiceDataHook(this);
}
static int g_runVoiceModelStaticFrontCalls, g_runVoiceModelStaticBackCalls;
static void *g_lastRunVoiceModelStaticThis;
static unsigned int g_lastRunVoiceModelStaticParam;
void CSTGSlotVoiceData::RunVoiceModelStaticFront(unsigned int param)
{ g_runVoiceModelStaticFrontCalls++; g_lastRunVoiceModelStaticThis = this; g_lastRunVoiceModelStaticParam = param; }
void CSTGSlotVoiceData::RunVoiceModelStaticBack(unsigned int param)
{ g_runVoiceModelStaticBackCalls++; g_lastRunVoiceModelStaticThis = this; g_lastRunVoiceModelStaticParam = param; }

static int g_getTotalStaticCostsCalls;
static unsigned long g_getTotalStaticCostsOut1, g_getTotalStaticCostsOut2;
void CSTGSlotVoiceData::GetTotalStaticCosts(unsigned long *out1, unsigned long *out2) const
{
	g_getTotalStaticCostsCalls++;
	*out1 = g_getTotalStaticCostsOut1;
	*out2 = g_getTotalStaticCostsOut2;
}
/* Steal is now real (sec 10.140) -- its own dependency,
 * CSTGVoiceAllocator::StealVoiceList, is mocked with call tracking
 * instead (same dangling-CSTGVoiceAllocator::sInstance reasoning as
 * EmergencyFreeVoiceList above applies here too -- safe since the
 * mock never dereferences its own `this`). */
static int g_stealVoiceListCalls;
void CSTGVoiceAllocator::StealVoiceList(void *) { g_stealVoiceListCalls++; }

/* Link-satisfying mocks for sec 10.144's new CSTGHDRManager::ProcessCommands()/
 * CSTGCDWorker::Initialize() bodies -- not exercised by anything in this
 * file, only needed so managers.cpp (linked here for CSTGGlobal's own
 * manager-constructor tests) links cleanly. Sec 10.148:
 * CSTGCDWorker_InitializeBuffer() is now real (managers.cpp) and calls
 * __kmalloc directly -- link-satisfying mock only. */
extern "C" void *__kmalloc(unsigned long size, unsigned int) { return malloc(size); }
void CSTGHDRManager::ProcessPlaybackCommands() { }
void CSTGHDRManager::ProcessRecordCommands() { }
void CSTGHDRManager::ProcessSamplerCommands() { }

/* CSTGProgramSlot's own confirmed-real, deliberately deferred
 * dependencies (sec 10.81). IsActive()/AccessActiveSlotVoiceData()/
 * HasActiveSlotVoiceData()/HasActiveVoices() are reconstructed for
 * real, sec 10.142 -- see src/engine/global.cpp; exercised directly
 * via real `CSTGGlobal::sInstance+0x29c990c` table/node/payload state
 * in section [27b] below, not mocked. */
CSTGProgramSlot::CSTGProgramSlot() {}
void CSTGProgramSlot::ChangeProgram(CSTGProgram *) {}

/* Mocks for RunVoiceModelFeedback/Initialize's own confirmed-real,
 * deliberately deferred dependencies (see oa_global.h). */
static int g_slotVoiceRunFeedbackCalls;
void CSTGSlotVoiceData::RunVoiceModelFeedback() { g_slotVoiceRunFeedbackCalls++; }
static int g_waveSeqInitCalls;
void CSTGWaveSeqData::Initialize() { g_waveSeqInitCalls++; }
/* CSTGSlotVoiceData::Initialize(unsigned short) is now real (sec
 * 10.150, see global.cpp) -- no mock body here any more, verified
 * directly on real fields below instead of a call counter (see the
 * [Initialize] scenario). Its own new dependency, CSTGChannelValues::
 * Initialize(), is now real too (sec 10.151, see global.cpp) -- no mock
 * body here any more either, verified directly on real fields below
 * (see the [Initialize] scenario's own updated checks). Its own
 * InitializeLongHand() dependency (normally bar2_stubs.cpp, not linked
 * into this file) is mocked with a counter, since ITS OWN real body is
 * out of scope -- the interesting confirmed behavior here is that it
 * fires EXACTLY ONCE process-wide no matter how many times Initialize()
 * itself is called. The two CSTGCommonLFO/CSTGCommonStepSeq sub-rate-pool
 * statics (normally defined in engine_startup_bits2.cpp, not linked into
 * this file) are pointed at small mmap32'd buffers so the real ctor's
 * own pointer arithmetic can be checked against a known base instead of
 * an arbitrary/uninitialized one. */
static int g_initLongHandCalls;
void CSTGChannelValues::InitializeLongHand() { g_initLongHandCalls++; }
/* Storage for CSTGChannelValues::sTemplateReady/sTemplate (normally
 * bar2_stubs.cpp, not linked into this file) and the two CSTGCommonLFO/
 * CSTGCommonStepSeq statics above (normally engine_startup_bits2.cpp,
 * also not linked here); the LFO/StepSeq pair is assigned to real
 * mmap32'd buffers at the one call site that exercises them (the
 * [Initialize] scenario). */
unsigned char CSTGChannelValues::sTemplateReady;
unsigned char CSTGChannelValues::sTemplate[0x92c];
STGLFOSubRateParams *CSTGCommonLFO::sSubRateParams;
STGStepSeqSubRateParams *CSTGCommonStepSeq::sSubRateParams;
/* CSTGProgramModeProgramSlot/CSTGProgramModeDrumTrackSlot's own ctors +
 * Initialize() are now real (sec 10.81, see global.cpp) -- the [Initialize]
 * scenario below placement-constructs both embedded sub-objects for
 * real (so their own real vtable pointer is set, since Initialize()
 * genuinely dispatches through it) instead of using call-counting mocks. */
unsigned char CSTGPerformanceVarsManager::sInstance[12];
static int g_perfVarsInitCalls;
static void *g_lastPerfVarsInitThis;
void CSTGPerformanceVarsManager::Initialize()
{
	g_perfVarsInitCalls++;
	g_lastPerfVarsInitThis = this;
}
/* CSTGControllerRTData::Initialize() is now real (sec 10.88).
 * RequestAnalogInputStatus is ALSO now real (sec 10.131) -- this file
 * links controller_rt_data_init.cpp directly, so verifying the real
 * body's own 19-call fan-out is done via its own forwarded dependency,
 * OmapNKS4OutputFifo_WriteCommand's own call counter, below. */
CSTGFrontPanel *CSTGFrontPanel::sInstance;
static int g_aliasBanksInitCalls;
void USTGAliasBankTypes::InitializeAliasBanks() { g_aliasBanksInitCalls++; }
/* Mock arrays -- see test_engine.cpp's own identical comment; this
 * file's own new [47] GetAliasPgmBankMapping test populates them
 * directly rather than relying on InitializeAliasBanks' real content. */
int STGAliasToRealPgmBank[30 * 128];
int STGAliasBankPgmMap[30 * 128];
static int g_initPerformancesCalls;
void CSTGGlobal::InitializePerformances() { g_initPerformancesCalls++; }
static int g_setListBankInitCalls;
void CSetListBank::Initialize() { g_setListBankInitCalls++; }

/* Mocks for sec 10.67's own confirmed-real, deliberately deferred
 * externs (see oa_global.h). */
static int g_omapWriteCommandCalls;
static int g_lastOmapWriteCommand;
int OmapNKS4OutputFifo_WriteCommand(int command)
{
	g_omapWriteCommandCalls++;
	g_lastOmapWriteCommand = command;
	return 1;
}
static int g_setControllerAssignmentCalls;
static void *g_lastSetControllerAssignmentThis;
static void *g_lastSetControllerAssignmentSelfRef;
static signed char g_lastSetControllerAssignmentValue;
static bool g_lastSetControllerAssignmentFlag;
void CSTGControllerRTData::SetControllerAssignment(void *selfRef, signed char newValue, bool flag)
{
	g_setControllerAssignmentCalls++;
	g_lastSetControllerAssignmentThis = this;
	g_lastSetControllerAssignmentSelfRef = selfRef;
	g_lastSetControllerAssignmentValue = newValue;
	g_lastSetControllerAssignmentFlag = flag;
}
CSTGControllerRTData *CSTGControllerRTData::sInstance;
/* ResetAllJumpCatch is now real (sec 10.129) -- see src/engine/global.cpp.
 * Its own 3 sub-calls (gated by the active CSTGPerformanceVarsManager's
 * own +0x23d1 flag) are tracked here; g_resetAllJumpCatchCalls is kept
 * as an alias of the first sub-call for existing call-site tests that
 * only care about "did ResetAllJumpCatch's real gate fire", not
 * updated to track "this" (ResetAllJumpCatch itself no longer has a
 * meaningful `this` beyond what it forwards to these calls). */
static int g_resetAllJumpCatchCalls;
static int g_resetKnobsJumpCatchCalls, g_resetSlidersJumpCatchCalls, g_resetRTKKnobSmoothersCalls;
static void *g_lastResetAllJumpCatchThis;
void CSTGControllerRTData::ResetKnobsJumpCatch()
{ g_resetAllJumpCatchCalls++; g_resetKnobsJumpCatchCalls++; g_lastResetAllJumpCatchThis = this; }
void CSTGControllerRTData::ResetSlidersJumpCatch()
{ g_resetSlidersJumpCatchCalls++; }
void CSTGControllerRTData::ResetRTKKnobSmoothers()
{ g_resetRTKKnobSmoothersCalls++; }
static int g_onExtModeSetChangeCalls;
static void *g_lastOnExtModeSetChangeThis;
void CSTGControllerRTData::OnExtModeSetChange()
{
	g_onExtModeSetChangeCalls++;
	g_lastOnExtModeSetChangeThis = this;
}
static int g_onExtModeAssignChangeCalls;
static const char *g_lastOnExtModeAssignChangeFamily;
static unsigned int g_lastOnExtModeAssignChangeIndex;
void CSTGControllerRTData::OnExtModeKnobAssignChange(unsigned int index)
{
	g_onExtModeAssignChangeCalls++;
	g_lastOnExtModeAssignChangeFamily = "Knob";
	g_lastOnExtModeAssignChangeIndex = index;
}
/* OnExtModePlayMuteSwitchAssignChange/OnExtModeSelectSwitchAssignChange
 * are now real (sec 10.126) -- see src/engine/global.cpp; verified via
 * STGAPIFrontPanelStatus's own real field write plus this mock for
 * their shared SendUnsolicitedUIParam dependency. */
static int g_sendUnsolicitedUIParamCalls;
static unsigned int g_lastSendUnsolicitedUIParamId;
static unsigned int g_lastSendUnsolicitedUIParamValue;
void CSTGControllerInfo::SendUnsolicitedUIParam(unsigned int paramId, unsigned int value, long, int)
{
	g_sendUnsolicitedUIParamCalls++;
	g_lastSendUnsolicitedUIParamId = paramId;
	g_lastSendUnsolicitedUIParamValue = value;
}
void CSTGControllerRTData::OnExtModeSliderAssignChange(unsigned int index)
{
	g_onExtModeAssignChangeCalls++;
	g_lastOnExtModeAssignChangeFamily = "Slider";
	g_lastOnExtModeAssignChangeIndex = index;
}
static int g_handleControllerChangeCalls;
static int g_lastHandleControllerChangeAssign;
static unsigned char g_lastHandleControllerChangeValue;
static bool g_lastHandleControllerChangeFlag1, g_lastHandleControllerChangeFlag2;
void CSTGControllerRTData::HandleControllerChange(int assign, unsigned char value, bool flag1, bool flag2)
{
	g_handleControllerChangeCalls++;
	g_lastHandleControllerChangeAssign = assign;
	g_lastHandleControllerChangeValue = value;
	g_lastHandleControllerChangeFlag1 = flag1;
	g_lastHandleControllerChangeFlag2 = flag2;
}
static int g_setPerfSwitchCalls;
static void *g_lastSetPerfSwitchThis;
static int g_lastSetPerfSwitchEnum;
static bool g_lastSetPerfSwitchValue;
void CSTGControllerInfo::SetPerfSwitch(int perfSwitch, bool value)
{
	g_setPerfSwitchCalls++;
	g_lastSetPerfSwitchThis = this;
	g_lastSetPerfSwitchEnum = perfSwitch;
	g_lastSetPerfSwitchValue = value;
}
static int g_stealAllVoicesCalls;
static void *g_lastStealAllVoicesThis;
void CSTGVoiceAllocator::StealAllVoices()
{
	g_stealAllVoicesCalls++;
	g_lastStealAllVoicesThis = this;
}
CSTGMidiPortManager *CSTGMidiPortManager::sInstance;
static int g_writeMidiOutQueueCalls;
static void *g_lastWriteMidiOutQueueThis;
static unsigned char g_lastMidiMsg[3];
static unsigned int g_lastMidiMsgLen;
void CSTGMidiPortManager::WriteSTGMidiOutQueue(const unsigned char *data, unsigned int length)
{
	g_writeMidiOutQueueCalls++;
	g_lastWriteMidiOutQueueThis = this;
	g_lastMidiMsgLen = length;
	for (unsigned int i = 0; i < length && i < 3; i++)
		g_lastMidiMsg[i] = data[i];
}
CSTGMidiDispatcher *CSTGMidiDispatcher::sInstance;
static int g_updateGlobalTuneCalls;
static void *g_lastUpdateGlobalTuneThis;
static float g_lastUpdateGlobalTuneValue;
void CSTGSlotVoiceData::UpdateGlobalTune(float tune)
{
	g_updateGlobalTuneCalls++;
	g_lastUpdateGlobalTuneThis = this;
	g_lastUpdateGlobalTuneValue = tune;
}
/* CSTGPerformance::IsCurrentlyActive() is real now (sec 10.144, see
 * managers.cpp) -- exercised via real CSTGPerformanceVarsManager::
 * sInstance/+0x23d1/+0x23d4 backing state in section [26] below, not
 * mocked. g_lastIsCurrentlyActiveThis is still tracked, but now by
 * observing which address IsCurrentlyActive() was actually invoked on
 * via a thin, test-only wrapper is unnecessary -- the real function
 * has no side effect to hook, so instead each check below sets up the
 * real "active" address it expects THIS call to resolve to and lets
 * the real gate logic decide whether to dispatch. */
static int g_handleControllerCalls;
static void *g_lastHandleControllerThis;
static unsigned char g_lastHandleControllerArg1, g_lastHandleControllerArg2, g_lastHandleControllerArg3;
static int g_lastHandleControllerSource, g_lastHandleControllerTarget;
void CSTGMidiDispatcher::HandleController(unsigned char arg1, unsigned char arg2, unsigned char arg3,
					   int source, int target)
{
	g_handleControllerCalls++;
	g_lastHandleControllerThis = this;
	g_lastHandleControllerArg1 = arg1;
	g_lastHandleControllerArg2 = arg2;
	g_lastHandleControllerArg3 = arg3;
	g_lastHandleControllerSource = source;
	g_lastHandleControllerTarget = target;
}
static int g_resetAllControllersCalls;
static void *g_lastResetAllControllersThis;
static unsigned char g_lastResetAllControllersChannel;
static bool g_lastResetAllControllersFlag;
void CSTGMidiDispatcher::ResetAllControllers(unsigned char channel, bool flag)
{
	g_resetAllControllersCalls++;
	g_lastResetAllControllersThis = this;
	g_lastResetAllControllersChannel = channel;
	g_lastResetAllControllersFlag = flag;
}
static int g_writeQueueCalls;
static void *g_lastWriteQueueThis;
static unsigned char g_lastQueueMsg[5];
static unsigned int g_lastQueueMsgLen;
void CSTGMidiQueueWriter::Write(const unsigned char *data, unsigned int length, bool)
{
	g_writeQueueCalls++;
	g_lastWriteQueueThis = this;
	g_lastQueueMsgLen = length;
	for (unsigned int i = 0; i < length && i < 5; i++)
		g_lastQueueMsg[i] = data[i];
}
CSTGSmoother *CSTGSmoother::sInstance;
static int g_cancelAllSmoothersCalls;
void CSTGSmoother::CancelAllSmoothers() { g_cancelAllSmoothersCalls++; }
static int g_finalizeAllSmoothersCalls;
void CSTGSmoother::FinalizeAllSmoothers() { g_finalizeAllSmoothersCalls++; }
static int g_setIsDyingCalls;
static void *g_lastSetIsDyingThis;
void CSTGPerformanceVars::SetIsDying() { g_setIsDyingCalls++; g_lastSetIsDyingThis = this; }
static int g_enterActivatingStateCalls;
static void *g_lastEnterActivatingStateThis;
void CSTGPerformanceVars::EnterActivatingState()
{ g_enterActivatingStateCalls++; g_lastEnterActivatingStateThis = this; }
extern "C" void *sXCmd;
extern "C" unsigned int kAudXBZD;
extern "C" float allPlusOne[4];
extern "C" float allMinusOne[4];
void *sXCmd;
unsigned int kAudXBZD;
float allPlusOne[4];
float allMinusOne[4];
/* OnUpdateProgramDrumTrackMidiChannel is now real (sec 10.133) -- see
 * src/engine/global.cpp; this file links global.cpp directly. */
CSTGVectorManager *CSTGVectorManager::sInstance;
/* OnUpdateGlobalMidiChannel is now real (sec 10.124) -- same body as
 * src/engine/vector_manager.cpp's own (duplicated here rather than
 * linking that whole file, since test_global.cpp treats
 * CSTGVectorManager as a raw-buffer mock, not a real constructed
 * object, and this method is small/self-contained enough not to need
 * the real ctor's own dependency chain). Verified via its own real
 * field writes at +0x19da4/+0x1b758, not a call-tracking mock. */
void CSTGVectorManager::OnUpdateGlobalMidiChannel(unsigned char channel)
{
	unsigned char *base = (unsigned char *)this;
	base[0x19da4] = channel;
	base[0x1b758] = channel;
}
/* OnUpdateGlobalMidiChannel is now real (sec 10.125) -- see
 * src/engine/global.cpp; verified via real bucket-list field checks,
 * not a call-tracking mock (see the extended UpdateMIDIChannel test
 * below). */
/* CSTGHeldKeyList::Reset()/CSTGEffectRackVars::UpdateDModRoutings() are
 * both now real (sec 10.82/10.135, see global.cpp). UpdateDModRoutings'
 * own real body makes a raw vtable-slot-33 call on a resolved object --
 * tracked here via a fake-vtable trap, matching the established pattern
 * for NotifySoloChange (sec 10.107)'s own test. */
static int g_dmodRoutingsVtableCalls;
static void *g_lastDmodRoutingsVtableThis;
static void *g_lastDmodRoutingsVtableArg1;
typedef void (*DmodRoutingsVtableFn)(void *, void *);
static void DmodRoutingsVtableTrap(void *self, void *arg1)
{
	g_dmodRoutingsVtableCalls++;
	g_lastDmodRoutingsVtableThis = self;
	g_lastDmodRoutingsVtableArg1 = arg1;
}
/* STGAPILR2IndivToPhysBusId's own real content is now homed directly in
 * managers.cpp (sec 10.132), linked into this binary directly. */
float gAllPlusHeadroom[4];
float gAllMinusHeadroom[4];

/* CSTGAudioInput's own 9 UpdateXXX methods are now reconstructed for
 * real (sec 10.80, see src/engine/global.cpp) -- section [12] below
 * exercises the CSTGGlobal-side thunks directly against the real
 * bodies rather than a call-counting mock. */

static int g_fail;

static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	if (got == want) {
		printf("  ok    %-60s 0x%x\n", label, got);
		return;
	}
	printf("  FAIL  %-60s got=0x%x want=0x%x\n", label, got, want);
	g_fail++;
}

int main(void)
{
	printf("CSTGGlobal UpdateXXX message-handler known-answer test\n");
	printf("=========================================================\n");

	size_t globalSize = 0x29cc920;	/* covers every confirmed offset below,
					 * including UpdateSongPunchMIDIChannel's
					 * old-value-indexed array up to index 0xff */
	unsigned char *buf = mmap32(globalSize);
	CSTGGlobal *g = (CSTGGlobal *)buf;

	printf("[1] UpdateMuteMode: raw int store (no bool conversion) at +0x29c9fc4\n");
	CSTGMessageContext ctx;
	memset(&ctx, 0, sizeof(ctx));
	STGConvertedParam param;
	param.value = 0x12345678;
	g->UpdateMuteMode(ctx, param);
	check_eq("+0x29c9fc4 == 0x12345678",
		 *(unsigned int *)(buf + 0x29c9fc4), 0x12345678);

	printf("[2] UpdateRearPanelControllerReset: bool-converted store at +0x29cc118\n");
	param.value = 0;
	g->UpdateRearPanelControllerReset(ctx, param);
	check_eq("value==0 -> byte 0", buf[0x29cc118], 0);
	param.value = 7;	/* any nonzero value must convert to exactly 1, not 7 */
	g->UpdateRearPanelControllerReset(ctx, param);
	check_eq("value==7 -> byte 1 (real bool conversion, not raw store)",
		 buf[0x29cc118], 1);

	printf("[3] UpdateTmbrTrkOscTransposeType: same shape, one field over at +0x29cc119\n");
	param.value = 0;
	g->UpdateTmbrTrkOscTransposeType(ctx, param);
	check_eq("value==0 -> byte 0", buf[0x29cc119], 0);
	param.value = -1;
	g->UpdateTmbrTrkOscTransposeType(ctx, param);
	check_eq("value==-1 -> byte 1", buf[0x29cc119], 1);
	check_eq("adjacent field +0x29cc118 unaffected by this call",
		 buf[0x29cc118], 1);	/* still whatever test [2] left it as */

	printf("[4] UpdateUserAllNoteScale: ctx.index-selected array store at +0x29c9d98\n");
	ctx.index = 3;
	param.value = 0x11111111;
	g->UpdateUserAllNoteScale(ctx, param);
	check_eq("table[3] == 0x11111111",
		 *(unsigned int *)(buf + 0x29c9d98 + 3 * 4), 0x11111111);
	ctx.index = 0;
	param.value = 0x22222222;
	g->UpdateUserAllNoteScale(ctx, param);
	check_eq("table[0] == 0x22222222",
		 *(unsigned int *)(buf + 0x29c9d98 + 0 * 4), 0x22222222);
	check_eq("table[3] unaffected by the table[0] write",
		 *(unsigned int *)(buf + 0x29c9d98 + 3 * 4), 0x11111111);

	printf("[5] UpdateLRBusIndivAssign: delegates to CSTGAudioBusManager::\n"
	       "    SetLRBusIndivAssign() on a pointer computed as this+4\n");
	param.value = 1;	/* index 1 into STGAPILR2IndivToPhysBusId (real value: 32) */
	g->UpdateLRBusIndivAssign(ctx, param);
	CSTGAudioBusManager *abm = (CSTGAudioBusManager *)(buf + 4);
	check_eq("physBusIdTableHead == STGAPILR2IndivToPhysBusId[1] (real value: 32)",
		 (unsigned int)abm->physBusIdTableHead, 32u);
	param.value = 0;
	g->UpdateLRBusIndivAssign(ctx, param);
	check_eq("physBusIdTableHead == STGAPILR2IndivToPhysBusId[0] (real value: 44)",
		 (unsigned int)abm->physBusIdTableHead, 44u);

	printf("[6] UpdateSPDIFSampleRate: writes ctx.responseCode (not param.value!),\n"
	       "    only when this->flagAt(0x6ac)==0 AND param.value!=0\n");
	memset(buf, 0, globalSize);	/* reset for a clean flag/state read */
	memset(&ctx, 0xcc, sizeof(ctx));
	ctx.responseCode = 0;
	param.value = 5;	/* nonzero */
	buf[0x6ac] = 0;		/* flag clear */
	g->UpdateSPDIFSampleRate(ctx, param);
	check_eq("flag==0, value!=0 -> responseCode set to literal 6 (not param.value==5)",
		 ctx.responseCode, 6);
	ctx.responseCode = 0;
	buf[0x6ac] = 1;		/* flag set -> must skip */
	g->UpdateSPDIFSampleRate(ctx, param);
	check_eq("flag!=0 -> no-op, responseCode stays 0", ctx.responseCode, 0);
	ctx.responseCode = 0;
	buf[0x6ac] = 0;
	param.value = 0;	/* value==0 -> must also skip */
	g->UpdateSPDIFSampleRate(ctx, param);
	check_eq("value==0 -> no-op, responseCode stays 0", ctx.responseCode, 0);

	printf("[7] TranslateAudioInputParamId: pure lookup, no `this` needed\n");
	static const struct { unsigned int paramId; int want; } kCases[] = {
		{0, 12}, {1, 12},		/* below range -> default 12 */
		{2, 13}, {3, 12}, {4, 15}, {5, 16}, {6, 12}, {7, 12}, {8, 49}, {9, 50},
		{10, 12}, {100, 12},		/* above range -> default 12 */
	};
	bool translateOk = true;
	for (auto &c : kCases) {
		int got = CSTGGlobal::TranslateAudioInputParamId(c.paramId);
		if (got != c.want) {
			printf("  FAIL  TranslateAudioInputParamId(%u) got=%d want=%d\n",
			       c.paramId, got, c.want);
			translateOk = false;
		}
	}
	check_eq("all 12 confirmed input/output pairs match", (unsigned int)translateOk, 1);

	printf("[8] SetSplitLayerWorkState: smallest CSTGGlobal method found so far --\n"
	       "    a direct byte store, no conversion\n");
	g->SetSplitLayerWorkState(true);
	check_eq("+0x29cc4e8 == 1 after SetSplitLayerWorkState(true)",
		 buf[0x29cc4e8], 1);
	g->SetSplitLayerWorkState(false);
	check_eq("+0x29cc4e8 == 0 after SetSplitLayerWorkState(false)",
		 buf[0x29cc4e8], 0);

	printf("[9] UpdateFootswitchPolarity: conditional delegation to\n"
	       "    CSTGControllerRTData::SetFootSwitchPolarity() via this+0x10\n");
	CSTGControllerRTData *rt = (CSTGControllerRTData *)(buf + 0x10);
	buf[0x6ae] = 0;		/* flag clear -> must skip */
	rt->footSwitchPolarity = 0xcc;
	param.value = 3;
	g->UpdateFootswitchPolarity(ctx, param);
	check_eq("flag==0 -> no-op, footSwitchPolarity untouched",
		 rt->footSwitchPolarity, 0xcc);
	buf[0x6ae] = 1;		/* flag set -> must delegate */
	g->UpdateFootswitchPolarity(ctx, param);
	check_eq("flag!=0 -> footSwitchPolarity == param.value (3)",
		 rt->footSwitchPolarity, 3);

	printf("[10] UpdateSongPunchMIDIChannel: +0x29cc0d0 (written) and +0x29cc0d1\n"
	       "     (read, a separate selector field) are confirmed independent --\n"
	       "     this function never reads back what it just wrote\n");
	buf[0x29cc0d1] = 0xff;	/* selector == -1 (signed) -> array write must be skipped */
	param.value = 5;
	g->UpdateSongPunchMIDIChannel(ctx, param);
	check_eq("channel field (+0x29cc0d0) == 5", buf[0x29cc0d0], 5);
	check_eq("selector (+0x29cc0d1) itself is untouched by this call", buf[0x29cc0d1], 0xff);
	check_eq("selector was negative -> array slot NOT written",
		 buf[0x29cc11d + 0xff * 8], 0);	/* still whatever calloc gave us: 0 */
	buf[0x29cc0d1] = 5;	/* now set the selector directly (a separate field) */
	param.value = 9;
	g->UpdateSongPunchMIDIChannel(ctx, param);
	check_eq("channel field == 9", buf[0x29cc0d0], 9);
	check_eq("selector (5) was non-negative -> array[5] == new value (9)",
		 buf[0x29cc11d + 5 * 8], 9);

	printf("[11] Batch of 12 simple no-branch handlers (8 raw stores, 4 bool-converted)\n");
	memset(buf, 0, globalSize);
	struct RawCase {
		const char *name;
		void (CSTGGlobal::*fn)(CSTGMessageContext &, STGConvertedParam &);
		unsigned int offset;
		int width;	/* 1 or 4 */
	};
	static const RawCase kRawCases[] = {
		{ "UpdateSeqParamMidiOutMode", &CSTGGlobal::UpdateSeqParamMidiOutMode, 0x6dd, 1 },
		{ "UpdateAfterTouchCurve",     &CSTGGlobal::UpdateAfterTouchCurve,     0x29c9f9c, 4 },
		{ "UpdateBankMap",             &CSTGGlobal::UpdateBankMap,             0x6e4, 4 },
		{ "UpdateVelocityCurve",       &CSTGGlobal::UpdateVelocityCurve,       0x29c9f98, 4 },
		{ "UpdateSeqTrackMidiOutMode", &CSTGGlobal::UpdateSeqTrackMidiOutMode, 0x6dc, 1 },
		{ "UpdateVectorMIDIOut",       &CSTGGlobal::UpdateVectorMIDIOut,       0x6c2, 1 },
		{ "UpdateNoteReceive",         &CSTGGlobal::UpdateNoteReceive,         0x6b4, 4 },
		{ "UpdateDamperPolarity",      &CSTGGlobal::UpdateDamperPolarity,      0x29c9fbc, 4 },
	};
	bool rawOk = true;
	for (auto &c : kRawCases) {
		param.value = (c.width == 1) ? 0x000000ab : 0x12345678;
		(g->*c.fn)(ctx, param);
		unsigned int got = (c.width == 1) ? buf[c.offset]
						   : *(unsigned int *)(buf + c.offset);
		unsigned int want = (c.width == 1) ? 0xab : 0x12345678u;
		if (got != want) {
			printf("  FAIL  %s: +0x%x got=0x%x want=0x%x\n", c.name, c.offset, got, want);
			rawOk = false;
		}
	}
	check_eq("all 8 raw-store handlers wrote their confirmed offset correctly",
		 (unsigned int)rawOk, 1);

	struct BoolCase {
		const char *name;
		void (CSTGGlobal::*fn)(CSTGMessageContext &, STGConvertedParam &);
		unsigned int offset;
	};
	static const BoolCase kBoolCases[] = {
		{ "UpdateCombiChangeEnable",      &CSTGGlobal::UpdateCombiChangeEnable,      0x6d7 },
		{ "UpdateAftertouchChangeEnable", &CSTGGlobal::UpdateAftertouchChangeEnable, 0x6d8 },
		{ "UpdateControlChangeEnable",    &CSTGGlobal::UpdateControlChangeEnable,    0x6d9 },
		{ "UpdateSysExEnable",            &CSTGGlobal::UpdateSysExEnable,            0x6da },
	};
	bool boolOk = true;
	for (auto &c : kBoolCases) {
		param.value = 0;
		(g->*c.fn)(ctx, param);
		if (buf[c.offset] != 0) {
			printf("  FAIL  %s: value=0 got=0x%x want=0\n", c.name, buf[c.offset]);
			boolOk = false;
		}
		param.value = 42;	/* nonzero, must convert to exactly 1 */
		(g->*c.fn)(ctx, param);
		if (buf[c.offset] != 1) {
			printf("  FAIL  %s: value=42 got=0x%x want=1 (real bool conversion)\n",
			       c.name, buf[c.offset]);
			boolOk = false;
		}
	}
	check_eq("all 4 bool-converted handlers wrote 0/1 (not raw value) at their offset",
		 (unsigned int)boolOk, 1);
	check_eq("the 4 bool flags are confirmed consecutive: +0x6d7..+0x6da all set",
		 (buf[0x6d7] && buf[0x6d8] && buf[0x6d9] && buf[0x6da]) ? 1 : 0, 1);

	printf("[12] The other 22 UpdateXXXMIDIChannel handlers -- same shape as\n"
	       "     UpdateSongPunchMIDIChannel (test [10]), confirmed via a full\n"
	       "     programmatic disassembly scan, not a spot check\n");
	struct MidiChanCase {
		const char *name;
		void (CSTGGlobal::*fn)(CSTGMessageContext &, STGConvertedParam &);
		unsigned int writeOffset;
	};
	static const MidiChanCase kMidiChanCases[] = {
		{ "UpdateRibbonLockMIDIChannel",      &CSTGGlobal::UpdateRibbonLockMIDIChannel,      0x29cc0d8 },
		{ "UpdateJSYLockMIDIChannel",         &CSTGGlobal::UpdateJSYLockMIDIChannel,         0x29cc0fc },
		{ "UpdateIncFuncMIDIChannel",         &CSTGGlobal::UpdateIncFuncMIDIChannel,         0x29cc10e },
		{ "UpdateSongStartMIDIChannel",       &CSTGGlobal::UpdateSongStartMIDIChannel,       0x29cc0ce },
		{ "UpdateChordSwMIDIChannel",         &CSTGGlobal::UpdateChordSwMIDIChannel,         0x29cc112 },
		{ "UpdateOctaveUpMIDIChannel",        &CSTGGlobal::UpdateOctaveUpMIDIChannel,        0x29cc0d4 },
		{ "UpdateAftertouchLockMIDIChannel",  &CSTGGlobal::UpdateAftertouchLockMIDIChannel,  0x29cc116 },
		{ "UpdateProgramUpMIDIChannel",       &CSTGGlobal::UpdateProgramUpMIDIChannel,       0x29cc0ca },
		{ "UpdateSW2FuncMIDIChannel",         &CSTGGlobal::UpdateSW2FuncMIDIChannel,         0x29cc10c },
		{ "UpdateJSPYRibLockMIDIChannel",     &CSTGGlobal::UpdateJSPYRibLockMIDIChannel,     0x29cc106 },
		{ "UpdateJSXLockMIDIChannel",         &CSTGGlobal::UpdateJSXLockMIDIChannel,         0x29cc0fa },
		{ "UpdateJSYRibLockMIDIChannel",      &CSTGGlobal::UpdateJSYRibLockMIDIChannel,      0x29cc104 },
		{ "UpdateProgramDownMIDIChannel",     &CSTGGlobal::UpdateProgramDownMIDIChannel,     0x29cc0cc },
		{ "UpdateTapTempoMIDIChannel",        &CSTGGlobal::UpdateTapTempoMIDIChannel,        0x29cc0d2 },
		{ "UpdateJSMYRibLockMIDIChannel",     &CSTGGlobal::UpdateJSMYRibLockMIDIChannel,     0x29cc108 },
		{ "UpdateDTrackEnableMIDIChannel",    &CSTGGlobal::UpdateDTrackEnableMIDIChannel,    0x29cc114 },
		{ "UpdateJSXRibLockMIDIChannel",      &CSTGGlobal::UpdateJSXRibLockMIDIChannel,      0x29cc102 },
		{ "UpdateSW1FuncMIDIChannel",         &CSTGGlobal::UpdateSW1FuncMIDIChannel,         0x29cc10a },
		{ "UpdateJSMYLockMIDIChannel",        &CSTGGlobal::UpdateJSMYLockMIDIChannel,        0x29cc100 },
		{ "UpdateDecFuncMIDIChannel",         &CSTGGlobal::UpdateDecFuncMIDIChannel,         0x29cc110 },
		{ "UpdateOctaveDownMIDIChannel",      &CSTGGlobal::UpdateOctaveDownMIDIChannel,      0x29cc0d6 },
		{ "UpdateJSPYLockMIDIChannel",        &CSTGGlobal::UpdateJSPYLockMIDIChannel,        0x29cc0fe },
	};
	bool midiChanOk = true;
	for (auto &c : kMidiChanCases) {
		/* selector negative -> array must NOT be touched */
		buf[c.writeOffset + 1] = 0xff;
		param.value = 0x11;
		(g->*c.fn)(ctx, param);
		if (buf[c.writeOffset] != 0x11) {
			printf("  FAIL  %s: write field got=0x%x want=0x11\n", c.name, buf[c.writeOffset]);
			midiChanOk = false;
		}
		/* selector valid -> both the field AND array[selector] get the value */
		buf[c.writeOffset + 1] = 2;
		param.value = 0x22;
		(g->*c.fn)(ctx, param);
		if (buf[c.writeOffset] != 0x22 || buf[0x29cc11d + 2 * 8] != 0x22) {
			printf("  FAIL  %s: field=0x%x array[2]=0x%x want both 0x22\n",
			       c.name, buf[c.writeOffset], buf[0x29cc11d + 2 * 8]);
			midiChanOk = false;
		}
	}
	check_eq("all 22 remaining MIDIChannel handlers match the confirmed shape",
		 (unsigned int)midiChanOk, 1);

	printf("[13] UpdateHeadroom: reads param.value AS A FLOAT (not int), broadcasts\n"
	       "     it (and its negation) into gAllPlusHeadroom/gAllMinusHeadroom\n");
	extern float gAllPlusHeadroom[4];
	extern float gAllMinusHeadroom[4];
	*(float *)&param.value = 2.5f;
	g->UpdateHeadroom(ctx, param);
	bool headroomOk = true;
	for (int i = 0; i < 4; i++) {
		if (gAllPlusHeadroom[i] != 2.5f || gAllMinusHeadroom[i] != -2.5f)
			headroomOk = false;
	}
	check_eq("gAllPlusHeadroom[0..3]==2.5f and gAllMinusHeadroom[0..3]==-2.5f",
		 (unsigned int)headroomOk, 1);

	printf("[14] RunVoiceModelFeedback: walks the intrusive list at +0x29c9900\n");
	{
		/* Mock "sub-object"s: a raw byte buffer with a manually-placed
		 * vtable pointer at offset 0 (matching this project's
		 * established pattern elsewhere, e.g. test_comport.cpp) rather
		 * than a C++ struct with an embedded pointer array, whose size
		 * is host-pointer-width-dependent and would misplace the
		 * confirmed +0xe2 gate byte. Slot 0x1a returns `self` so
		 * `result[0xe2]` reads this same buffer's own gate byte. */
		auto slot1a = [](void *self) -> void * { return self; };
		void *vtable[0x1b];
		for (int i = 0; i < 0x1b; i++)
			vtable[i] = 0;
		vtable[0x1a] = (void *)+slot1a;

		unsigned char gatePass[0xe3];
		memset(gatePass, 0, sizeof(gatePass));
		*(void ***)gatePass = vtable;
		gatePass[0xe2] = 1;

		/*
		 * Real, confirmed constraint this test design has to work
		 * around: the confirmed real offsets +0xb6b (pointer) and
		 * +0xb6f (pointer) are only 4 bytes apart on the real 32-bit
		 * target (each a 4-byte pointer there), but need 8 bytes each
		 * on this host -- meaning +0xb6b's own 8-byte write already
		 * spans INTO +0xb6f, and +0xb6f's own write spans into +0xb73
		 * (the flags byte). Both pairs physically overlap here, with
		 * no way to hold two independent valid host pointers in that
		 * tightly-packed span. This was caught the hard way (two
		 * earlier draft designs each segfaulted differently trying to
		 * populate both fields). Worked around by testing ONLY the
		 * combination that's both meaningful and safe: +0xb6b alone
		 * (its own 8-byte span ends exactly at +0xb73, so it does NOT
		 * clobber the flags byte -- confirmed no overlap) with the
		 * flags byte set to bit 0 ONLY, so the confirmed real
		 * short-circuit (bit 1 is only checked if bit 0 was even SET)
		 * means +0xb6f is never touched at all. A second node with
		 * `flags == 0` (no bits set, the all-zero memset default)
		 * exercises the "neither gate present" skip path without
		 * needing any pointer at all.
		 */
		unsigned char payload1[0xb74], payload2[0xb74];
		memset(payload1, 0, sizeof(payload1));
		memset(payload2, 0, sizeof(payload2));
		unsigned char sub1[0xb74 + 8], sub2[0xb74 + 8];
		memset(sub1, 0, sizeof(sub1));
		memset(sub2, 0, sizeof(sub2));
		*(unsigned char **)(payload1 + 0x38) = sub1;
		*(unsigned char **)(payload2 + 0x38) = sub2;

		/* sub1: bit-0 gate PASSES -> feedback triggered. */
		*(unsigned char **)(sub1 + 0xb6b) = gatePass;
		sub1[0xb73] = 1;

		/* sub2: flags == 0 (all-zero default) -> neither gate is even
		 * attempted -> feedback never triggered. */

		unsigned char node1[0x20], node2[0x20];
		*(unsigned char **)(node1 + 0x8) = payload1;
		*(unsigned char **)(node2 + 0x8) = payload2;
		*(unsigned char **)node1 = node2;  /* node1->next = node2 */
		*(unsigned char **)node2 = 0;      /* node2->next = NULL */
		*(unsigned char **)(buf + 0x29c9900) = node1;

		g_slotVoiceRunFeedbackCalls = 0;
		g->RunVoiceModelFeedback();
		check_eq("only the gate-passing node triggered feedback",
			 (unsigned int)g_slotVoiceRunFeedbackCalls, 1);

		*(unsigned char **)(buf + 0x29c9900) = 0; /* reset for later scenarios */
	}

	printf("[15] Initialize: confirmed control flow (vtable slot 7, 32-slot list, sub-inits)\n");
	{
		typedef void (*VtableSlot7Fn)(void *);
		static int slot7Calls;
		VtableSlot7Fn slot7 = [](void *) { slot7Calls++; };
		void *vtable[8];
		for (int i = 0; i < 8; i++)
			vtable[i] = 0;
		vtable[7] = (void *)slot7;
		*(void ***)buf = vtable;

		g_waveSeqInitCalls = 0;
		g_initLongHandCalls = 0;
		CSTGChannelValues::sTemplateReady = 0;
		for (unsigned int i = 0; i < sizeof(CSTGChannelValues::sTemplate); i++)
			CSTGChannelValues::sTemplate[i] = 0;
		/* Poison the LAST (31st) slot's own CSTGChannelValues sub-object
		 * (self+0x1488, see below) so the real Initialize()'s own 0x92c-byte
		 * copy from sTemplate can be verified directly, not inferred from a
		 * call counter. */
		unsigned char *lastSlotChannelValues =
			buf + 0x2977cf0 + 31 * 0x28e0 + 0x4 + 0x1488;
		for (unsigned int i = 0; i < sizeof(CSTGChannelValues::sTemplate); i++)
			lastSlotChannelValues[i] = 0xAB;
		/* CSTGSlotVoiceData::Initialize()'s own real body computes
		 * pointers into these two shared pools -- point them at real
		 * mmap32'd buffers (big enough for the largest confirmed real
		 * quadIndex*stride offset this scenario reaches, slotIndex 31
		 * -> quadIndex 7) so the real pointer arithmetic below can be
		 * checked against a known base. */
		unsigned char *lfoPoolBuf = mmap32(8 * 0x250);
		unsigned char *stepSeqPoolBuf = mmap32(8 * 0x100);
		CSTGCommonLFO::sSubRateParams = (STGLFOSubRateParams *)lfoPoolBuf;
		CSTGCommonStepSeq::sSubRateParams = (STGStepSeqSubRateParams *)stepSeqPoolBuf;
		g_perfVarsInitCalls = 0;
		g_aliasBanksInitCalls = g_initPerformancesCalls = g_setListBankInitCalls = 0;
		slot7Calls = 0;
		buf[0x67f] = 0;
		buf[0x6b8] = 5;
		buf[0x6ba] = 7;
		/* CSTGControllerRTData::Initialize() is now real and tail-calls
		 * into RequestAnalogInputPositions(), which dereferences
		 * CSTGFrontPanel::sInstance -- needs a valid (non-null) target. */
		unsigned char *frontPanelBuf = mmap32(0x10);
		CSTGFrontPanel::sInstance = (CSTGFrontPanel *)frontPanelBuf;
		g_omapWriteCommandCalls = 0;
		/* CSTGProgramModeProgramSlot/DrumTrackSlot's own Initialize() is
		 * now real and genuinely dispatches through ITS OWN (different)
		 * vtable slot 7 -- a plain `static` placeholder array's address
		 * isn't guaranteed to survive g_programSlotVtable's own 32-bit
		 * truncation on a 64-bit host (PIE puts static data above 4GB),
		 * so repoint it at an mmap32'd buffer before constructing either
		 * sub-object (see oa_global.h's own comment on g_programSlotVtable). */
		void **programSlotVtableBuf = (void **)mmap32(10 * sizeof(void *));
		typedef void (*Slot7TrapFn)(void *);
		static int programSlotTrapCalls;
		Slot7TrapFn programSlotTrap = [](void *) { programSlotTrapCalls++; };
		programSlotTrapCalls = 0;
		programSlotVtableBuf[0] = 0;
		programSlotVtableBuf[1] = 0;
		for (int i = 2; i < 10; i++)
			programSlotVtableBuf[i] = (void *)programSlotTrap;
		g_programSlotVtable = programSlotVtableBuf;

		/* CSTGProgramModeProgramSlot/DrumTrackSlot's own Initialize() is
		 * now real and genuinely dispatches through ITS OWN (different)
		 * vtable slot 7 -- placement-construct both embedded sub-objects
		 * for real so that dispatch has a valid vtable pointer to read. */
		new (buf + 0x2977b1f) CSTGProgramModeProgramSlot();
		new (buf + 0x2977c08) CSTGProgramModeDrumTrackSlot();

		g->Initialize();

		check_eq("CSTGProgramModeProgramSlot/DrumTrackSlot Initialize's own vtable slot 7 called twice",
			 (unsigned int)programSlotTrapCalls, 2);
		munmap(programSlotVtableBuf, 10 * sizeof(void *));

		check_eq("vtable slot 7 called once", (unsigned int)slot7Calls, 1);
		check_eq("CSTGWaveSeqData::Initialize called once", (unsigned int)g_waveSeqInitCalls, 1);
		check_eq("+0x67f bit 1 (0x2) set", buf[0x67f] & 0x2, 0x2);
		/* CSTGSlotVoiceData::Initialize() is now real (sec 10.150) --
		 * check its own confirmed real field writes on the LAST (31st)
		 * of the 32 constructed slots directly, a strictly stronger
		 * check than the old call counter (proves the real body
		 * actually ran with the right slot index and computed the
		 * right pool pointers, not just that some function was invoked
		 * 32 times). Its own dependency, CSTGChannelValues::
		 * Initialize(), is real too now (sec 10.151) -- checked directly
		 * on the last slot's own real field contents below, plus the
		 * confirmed real "lazy, process-wide, exactly once" semantics of
		 * its own InitializeLongHand() dependency (still out of scope,
		 * mocked with a counter). */
		check_eq("CSTGChannelValues::InitializeLongHand called exactly once "
			 "(lazy, process-wide) despite 32 Initialize() calls",
			 (unsigned int)g_initLongHandCalls, 1);
		check_eq("CSTGChannelValues::sTemplateReady flipped true after the first Initialize()",
			 (unsigned int)CSTGChannelValues::sTemplateReady, 1);
		{
			bool allZero = true;
			for (unsigned int i = 0; i < sizeof(CSTGChannelValues::sTemplate); i++)
				if (lastSlotChannelValues[i] != 0)
					allZero = false;
			check_eq("slot[31]'s own CSTGChannelValues sub-object (+0x1488) fully "
				 "overwritten by sTemplate (all-zero, since the mocked "
				 "InitializeLongHand leaves it so)",
				 (unsigned int)allZero, 1);
		}
		{
			unsigned char *lastSlot = buf + 0x2977cf0 + 31 * 0x28e0 + 0x4;
			check_eq("slot[31]'s own +0x0 == slotIndex (31)",
				 *(unsigned short *)lastSlot, 31);
			unsigned char *expectLfo = lfoPoolBuf + 7 * 0x250 + 3 * 4; /* quad=7, sub=3 */
			unsigned char *expectStepSeq = stepSeqPoolBuf + 7 * 0x100 + 3 * 4;
			/* Packed 32-bit fields (see global.cpp's own comment on
			 * this exact host/target width hazard) -- read back via
			 * a 4-byte `unsigned int`, not a native pointer. */
			unsigned int gotLfo = *(unsigned int *)(lastSlot + 0x1480);
			unsigned int gotStepSeq = *(unsigned int *)(lastSlot + 0x1484);
			check_eq("slot[31]'s +0x1480 == sSubRateParams + quadIndex*0x250 + subIndex*4",
				 gotLfo, (unsigned int)(unsigned long)expectLfo);
			check_eq("slot[31]'s +0x1484 == sSubRateParams + quadIndex*0x100 + subIndex*4",
				 gotStepSeq, (unsigned int)(unsigned long)expectStepSeq);
		}
		check_eq("list head/tail set (non-null)",
			 (unsigned int)(*(unsigned int *)(buf + 0x29c98f4) != 0 &&
					 *(unsigned int *)(buf + 0x29c98f8) != 0), 1);
		check_eq("list count == 32", *(unsigned int *)(buf + 0x29c98fc), 0x20);
		check_eq("CSTGProgramModeProgramSlot::Initialize got +0x6b8's value (5)",
			 buf[0x2977b1f + 0x10], 5);
		check_eq("CSTGProgramModeDrumTrackSlot::Initialize got +0x6ba's value (7)",
			 buf[0x2977c08 + 0x10], 7);
		check_eq("CSTGPerformanceVarsManager::Initialize called with &sInstance as this",
			 (unsigned int)(g_lastPerfVarsInitThis == &CSTGPerformanceVarsManager::sInstance), 1);
		check_eq("CSTGControllerRTData::Initialize -> RequestAnalogInputPositions -> "
			 "RequestAnalogInputStatus -> 19 real OmapNKS4OutputFifo_WriteCommand calls",
			 (unsigned int)g_omapWriteCommandCalls, 19);
		check_eq("  ...last call (device code 0x18) built the confirmed real command word",
			 (unsigned int)g_lastOmapWriteCommand, 0x1a00000 | (0x18 << 8));
		munmap(frontPanelBuf, 0x10);
		check_eq("USTGAliasBankTypes::InitializeAliasBanks called once",
			 (unsigned int)g_aliasBanksInitCalls, 1);
		check_eq("CSTGGlobal::InitializePerformances called once",
			 (unsigned int)g_initPerformancesCalls, 1);
		check_eq("CSetListBank::Initialize called once", (unsigned int)g_setListBankInitCalls, 1);
		check_eq("+0x6ae set to 1 (initialized flag)", buf[0x6ae], 1);
	}

	printf("\n[11] UpdateMIDIClockSource/UpdateShowMSWSDKitGraphics: confirmed real no-ops\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		memset(buf, 0xaa, globalSize);
		g->UpdateMIDIClockSource(ctx, param);
		g->UpdateShowMSWSDKitGraphics(ctx, param);
		int allUntouched = 1;
		for (size_t i = 0; i < globalSize; i++)
			if (buf[i] != 0xaa) { allUntouched = 0; break; }
		check_eq("object completely untouched by both no-op handlers", (unsigned int)allUntouched, 1);
	}

	printf("\n[12] The nine UpdateAudioInputXXX thunks: this+0x608, confirmed real\n"
	       "     via a full disassembly of each of the 9, not a spot check\n"
	       "     (checked against CSTGAudioInput's own now-real bodies, sec 10.80)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		unsigned char *sub = buf + 0x608;
		ctx.index = 2;

		/* Each case: the thunk under test, a distinct marker value, and
		 * where that marker should land inside the embedded +0x608
		 * CSTGAudioInput sub-object -- proves BOTH the this+0x608
		 * adjustment AND correct dispatch to the named real method, all
		 * in one shot, no mock bookkeeping needed. */
		memset(sub, 0, 0x80);
		param.value = 111;
		g->UpdateAudioInputLevel(ctx, param);
		check_eq("UpdateAudioInputLevel -> UpdateLevel (this+0x608, +0x4+idx*4)",
			 *(unsigned int *)(sub + 0x4 + 2 * 4), 111u);

		memset(sub, 0, 0x80);
		param.value = 222;
		g->UpdateAudioInputSend1Level(ctx, param);
		check_eq("UpdateAudioInputSend1Level -> UpdateSend1Level (+0x34+idx*4)",
			 *(unsigned int *)(sub + 0x34 + 2 * 4), 222u);

		memset(sub, 0, 0x80);
		param.value = 333;
		g->UpdateAudioInputSend2Level(ctx, param);
		check_eq("UpdateAudioInputSend2Level -> UpdateSend2Level (+0x4c+idx*4)",
			 *(unsigned int *)(sub + 0x4c + 2 * 4), 333u);

		memset(sub, 0, 0x80);
		param.value = 5;
		g->UpdateAudioInputBusSelect(ctx, param);
		check_eq("UpdateAudioInputBusSelect -> UpdateBusSelect (+0x64+idx)",
			 sub[0x64 + 2], 5);

		memset(sub, 0, 0x80);
		param.value = 4;
		g->UpdateAudioInputFXControlBus(ctx, param);
		check_eq("UpdateAudioInputFXControlBus -> UpdateFXControlBus (+0x6a+idx)",
			 sub[0x6a + 2], 4);

		memset(sub, 0, 0x80);
		param.value = 3;
		g->UpdateAudioInputHDRBus(ctx, param);
		check_eq("UpdateAudioInputHDRBus -> UpdateHDRBus (+0x70+idx)",
			 sub[0x70 + 2], 3);

		memset(sub, 0, 0x80);
		param.value = 1;
		g->UpdateAudioInputMute(ctx, param);
		check_eq("UpdateAudioInputMute -> UpdateMute (+0x76 bit idx)",
			 sub[0x76], (unsigned char)(1u << 2));

		memset(sub, 0, 0x80);
		*(float *)&param.value = 0.25f;
		g->UpdateAudioInputPan(ctx, param);
		check_eq("UpdateAudioInputPan -> UpdatePan (+0x1c+idx*4)",
			 *(unsigned int *)(sub + 0x1c + 2 * 4), *(unsigned int *)&param.value);

		g_setAudioInSoloCalls = 0;
		unsigned char *rtForSolo = mmap32(0x10);
		CSTGControllerRTData::sInstance = (CSTGControllerRTData *)rtForSolo;
		memset(sub, 0, 0x80);
		sub[0x77] |= 0x2; /* mark active -- UpdateSolo is a no-op unless active */
		param.value = 1;
		g->UpdateAudioInputSolo(ctx, param);
		check_eq("UpdateAudioInputSolo -> UpdateSolo (SetAudioInSolo called once)",
			 (unsigned int)g_setAudioInSoloCalls, 1);
		check_eq("  ...with the this+0x608-relative object's own ctx.index",
			 g_lastSoloSlot, 2u);
		munmap(rtForSolo, 0x10);
	}

	printf("\n[13] UpdateAudioClockSource: gated by +0x6ae, 3-way command mapping\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		memset(buf, 0, globalSize);
		g_omapWriteCommandCalls = 0; /* shared mock counter -- also used by
					      * [15]'s own RequestAnalogInputStatus
					      * check earlier in this file's run */

		param.value = 1;
		g->UpdateAudioClockSource(ctx, param);
		check_eq("no-op while +0x6ae == 0 (uninitialized)", (unsigned int)g_omapWriteCommandCalls, 0);

		buf[0x6ae] = 1;
		g->UpdateAudioClockSource(ctx, param);
		check_eq("param.value==1 -> 0x7800000", (unsigned int)g_lastOmapWriteCommand, 0x7800000u);

		param.value = 2;
		g->UpdateAudioClockSource(ctx, param);
		check_eq("param.value==2 -> 0x7020000", (unsigned int)g_lastOmapWriteCommand, 0x7020000u);

		param.value = 99;
		g->UpdateAudioClockSource(ctx, param);
		check_eq("param.value==other -> 0x7000000", (unsigned int)g_lastOmapWriteCommand, 0x7000000u);
		check_eq("exactly 3 calls reached the hardware command extern",
			 (unsigned int)g_omapWriteCommandCalls, 3);
	}

	printf("\n[14] UpdateFootSwitchAssign: gated by +0x6ae, real 55-entry .rodata table,\n"
	       "     self-referencing SetControllerAssignment call on this+0x10\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		memset(buf, 0, globalSize);

		param.value = 3;
		g->UpdateFootSwitchAssign(ctx, param);
		check_eq("no-op while +0x6ae == 0", (unsigned int)g_setControllerAssignmentCalls, 0);

		buf[0x6ae] = 1;
		g_setControllerAssignmentCalls = 0;
		param.value = 0;
		g->UpdateFootSwitchAssign(ctx, param);
		check_eq("table[0] == 0x00", (unsigned int)(unsigned char)g_lastSetControllerAssignmentValue, 0x00u);
		check_eq("this == this+0x10", (unsigned int)(g_lastSetControllerAssignmentThis == buf + 0x10), 1);
		check_eq("selfRef == this+0x10 (self-reference)",
			 (unsigned int)(g_lastSetControllerAssignmentSelfRef == buf + 0x10), 1);
		check_eq("flag == false", (unsigned int)g_lastSetControllerAssignmentFlag, 0);

		param.value = 5;
		g->UpdateFootSwitchAssign(ctx, param);
		check_eq("table[5] == 0x14", (unsigned int)(unsigned char)g_lastSetControllerAssignmentValue, 0x14u);

		param.value = 54;
		g->UpdateFootSwitchAssign(ctx, param);
		check_eq("table[54] (last entry) == 0x41", (unsigned int)(unsigned char)g_lastSetControllerAssignmentValue, 0x41u);
		check_eq("exactly 3 calls reached SetControllerAssignment", (unsigned int)g_setControllerAssignmentCalls, 3);
	}

	printf("\n[15] UpdateRTKnobFuncMIDIChannel/UpdatePadFuncMIDIChannel: ctx.index-bounded,\n"
	       "     mirrors into the SAME shared +0x29cc11d array as the plain MIDI-channel family\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		memset(buf, 0, globalSize);

		ctx.index = 8; /* out of bounds (must be <= 7) */
		param.value = 0x42;
		g->UpdateRTKnobFuncMIDIChannel(ctx, param);
		check_eq("ctx.index==8 (out of bounds) is a no-op", buf[0x29cc0da], 0);

		ctx.index = 3;
		buf[0x29cc0e2 + 3] = 9; /* selector byte for slot 3 */
		g->UpdateRTKnobFuncMIDIChannel(ctx, param);
		check_eq("value array (+0x29cc0da) written at ctx.index", buf[0x29cc0da + 3], 0x42);
		check_eq("mirrored into shared array at [selector*8]", buf[0x29cc11d + 9 * 8], 0x42);

		memset(buf, 0, globalSize);
		ctx.index = 2;
		buf[0x29cc0f2 + 2] = 0x80; /* negative signed byte -> selector < 0, no mirror */
		param.value = 0x55;
		g->UpdatePadFuncMIDIChannel(ctx, param);
		check_eq("value array (+0x29cc0ea) still written even with negative selector", buf[0x29cc0ea + 2], 0x55);
		check_eq("negative selector suppresses the shared-array mirror",
			 (unsigned int)buf[0x29cc11d], 0);
	}

	printf("\n[16] UpdateFootPedalAssign: gated by +0x6ae, DIFFERENT 32-entry table,\n"
	       "     always writes +0x18, conditionally zeroes via +0x15, arg1 == this+0x13 (NOT self)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		memset(buf, 0, globalSize);
		g_setControllerAssignmentCalls = 0;

		param.value = 1;
		g->UpdateFootPedalAssign(ctx, param);
		check_eq("no-op while +0x6ae == 0", (unsigned int)g_setControllerAssignmentCalls, 0);

		buf[0x6ae] = 1;
		g_setControllerAssignmentCalls = 0;
		param.value = 0;
		g->UpdateFootPedalAssign(ctx, param);
		check_eq("+0x18 always written to table[0]==0x00 regardless of +0x15",
			 (unsigned int)(*(int *)(buf + 0x18)), 0x00u);
		check_eq("+0x15==0 -> value passed is 0, not the table lookup",
			 (unsigned int)(unsigned char)g_lastSetControllerAssignmentValue, 0u);
		check_eq("this == this+0x10", (unsigned int)(g_lastSetControllerAssignmentThis == buf + 0x10), 1);
		check_eq("arg1 == this+0x13 (confirmed NOT a self-reference here)",
			 (unsigned int)(g_lastSetControllerAssignmentSelfRef == buf + 0x13), 1);
		check_eq("flag == true", (unsigned int)g_lastSetControllerAssignmentFlag, 1);

		buf[0x15] = 1;
		param.value = 4;
		g->UpdateFootPedalAssign(ctx, param);
		check_eq("+0x18 == table[4]==0x05", (unsigned int)(*(int *)(buf + 0x18)), 0x05u);
		check_eq("+0x15!=0 -> value passed IS the table lookup",
			 (unsigned int)(unsigned char)g_lastSetControllerAssignmentValue, 0x05u);

		param.value = 31;
		g->UpdateFootPedalAssign(ctx, param);
		check_eq("table[31] (last entry) == 0x2d",
			 (unsigned int)(unsigned char)g_lastSetControllerAssignmentValue, 0x2du);
		check_eq("exactly 3 calls reached SetControllerAssignment", (unsigned int)g_setControllerAssignmentCalls, 3);
	}

	printf("\n[17] UpdateKnobFaderMode: read-before-write change detection,\n"
	       "     ResetAllJumpCatch only fires when initialized AND value changed\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		memset(buf, 0, globalSize);
		g_resetAllJumpCatchCalls = 0;

		param.value = 1;
		g->UpdateKnobFaderMode(ctx, param);
		check_eq("+0x29c9fc0 stores the bool-converted value", buf[0x29c9fc0], 1);
		check_eq("no-op while +0x6ae == 0, even though value changed (0->1)",
			 (unsigned int)g_resetAllJumpCatchCalls, 0);

		buf[0x6ae] = 1;
		param.value = 1; /* same value again -- no change */
		g->UpdateKnobFaderMode(ctx, param);
		check_eq("no-op when value did NOT change (1->1)", (unsigned int)g_resetAllJumpCatchCalls, 0);

		CSTGControllerRTData::sInstance = (CSTGControllerRTData *)(buf + 0x1000);
		/* ResetAllJumpCatch's own real body (sec 10.129) gates its 3
		 * sub-calls on the active CSTGPerformanceVarsManager's own
		 * +0x23d1 flag == 2 -- set up a real, valid mgr for this. */
		unsigned char *mgr17 = mmap32(0x2400);
		memset(mgr17, 0, 0x2400);
		mgr17[0x23d1] = 2;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgr17;
		CSTGPerformanceVarsManager::sInstance[8] = 0;
		param.value = 0; /* actually changes (1->0) */
		g->UpdateKnobFaderMode(ctx, param);
		check_eq("+0x29c9fc0 updated to the new value", buf[0x29c9fc0], 0);
		check_eq("fires ResetAllJumpCatch when initialized AND value changed",
			 (unsigned int)g_resetAllJumpCatchCalls, 1);
		check_eq("called on CSTGControllerRTData::sInstance, not the embedded this+0x10",
			 (unsigned int)(g_lastResetAllJumpCatchThis == CSTGControllerRTData::sInstance), 1);
		/* Deliberately NOT munmap'd: CSTGPerformanceVarsManager::sInstance
		 * still points at mgr17, and later sections (e.g. UpdateLocalControl)
		 * also reach ResetAllJumpCatch's real gate check -- freeing this here
		 * left a dangling pointer, causing an intermittent (allocator-timing-
		 * dependent) use-after-free segfault found via repeated test runs. */
	}

	printf("\n[18] UpdateConvertPosition: conditional StealAllVoices, unconditional write\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		unsigned char *msgProc = mmap32(0x100);
		unsigned char *voiceAlloc = mmap32(0x100);
		CSTGMessageProcessor::sInstance = (CSTGMessageProcessor *)msgProc;
		CSTGVoiceAllocator::sInstance = (CSTGVoiceAllocator *)voiceAlloc;
		memset(buf, 0, globalSize);
		g_stealAllVoicesCalls = 0;

		param.value = 0x77;
		g->UpdateConvertPosition(ctx, param);
		check_eq("not initialized -> no StealAllVoices, but value still written",
			 (unsigned int)g_stealAllVoicesCalls, 0);
		check_eq("+0x6b0 written even when not initialized",
			 (unsigned int)(*(int *)(buf + 0x6b0)), 0x77u);

		buf[0x6ae] = 1;
		msgProc[0x48] = 0;
		param.value = 0x99;
		g->UpdateConvertPosition(ctx, param);
		check_eq("initialized + msgProc flag clear -> StealAllVoices fires",
			 (unsigned int)g_stealAllVoicesCalls, 1);
		check_eq("StealAllVoices called on CSTGVoiceAllocator::sInstance",
			 (unsigned int)(g_lastStealAllVoicesThis == voiceAlloc), 1);
		check_eq("+0x6b0 updated", (unsigned int)(*(int *)(buf + 0x6b0)), 0x99u);

		msgProc[0x48] = 1;
		param.value = 0xaa;
		g->UpdateConvertPosition(ctx, param);
		check_eq("initialized + msgProc flag set -> no StealAllVoices",
			 (unsigned int)g_stealAllVoicesCalls, 1);
		check_eq("+0x6b0 still updated", (unsigned int)(*(int *)(buf + 0x6b0)), 0xaau);

		munmap(msgProc, 0x100);
		munmap(voiceAlloc, 0x100);
	}

	printf("\n[19] UpdateUserOctaveScale: ctx.index/12 quotient/remainder 2D table\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		memset(buf, 0, globalSize);

		ctx.index = 0; /* quotient=0, remainder=0 */
		param.value = 0x1234;
		g->UpdateUserOctaveScale(ctx, param);
		check_eq("index=0 -> base 0x29c9a98", (unsigned int)(*(int *)(buf + 0x29c9a98)), 0x1234u);

		ctx.index = 13; /* quotient=1, remainder=1 */
		param.value = 0x5678;
		g->UpdateUserOctaveScale(ctx, param);
		check_eq("index=13 -> 0x29c9a98 + 1*48 + 1*4",
			 (unsigned int)(*(int *)(buf + 0x29c9a98 + 48 + 4)), 0x5678u);

		ctx.index = 35; /* quotient=2, remainder=11 */
		param.value = 0x9abc;
		g->UpdateUserOctaveScale(ctx, param);
		check_eq("index=35 -> 0x29c9a98 + 2*48 + 11*4",
			 (unsigned int)(*(int *)(buf + 0x29c9a98 + 96 + 44)), 0x9abcu);
	}

	printf("\n[20] UpdatePerfChangeHoldTime/UpdateExtSetSelect: shared active-manager\n"
	       "     resolution via CSTGPerformanceVarsManager::sInstance's real 12-byte layout\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		unsigned char *audioBus = mmap32(0x100);
		unsigned char *mgr0 = mmap32(0x24000);
		unsigned char *mgr1 = mmap32(0x24000);
		CSTGAudioBusManager::sInstance = (CSTGAudioBusManager *)audioBus;
		*(float *)(audioBus + 4) = 2.0f;
		/* Host/target pointer-width fix (this project's established
		 * pattern): sInstance's real slots are packed 32-bit fields --
		 * a native 8-byte host pointer write here would overlap the
		 * adjacent slot. */
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgr0;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 4) = (unsigned int)(unsigned long)mgr1;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 8) = 1; /* select mgr1 */
		memset(buf, 0, globalSize);

		*(float *)&param.value = 3.5f;
		g->UpdatePerfChangeHoldTime(ctx, param);
		check_eq("+0x6e0 == truncate(3.5*2.0)=7, always written", (unsigned int)(*(int *)(buf + 0x6e0)), 7u);
		check_eq("not initialized -> mgr1 field untouched", (unsigned int)(*(int *)(mgr1 + 0x23e0)), 0);

		buf[0x6ae] = 1;
		mgr1[0x23d1] = 2;
		mgr1[0x23dc] = 0;
		*(float *)&param.value = 2.5f;
		g->UpdatePerfChangeHoldTime(ctx, param);
		check_eq("initialized + mgr1 gates satisfied -> mgr1+0x23e0 == truncate(2.5*2.0)=5",
			 (unsigned int)(*(int *)(mgr1 + 0x23e0)), 5u);
		check_eq("mgr0 (NOT selected) left untouched", (unsigned int)(*(int *)(mgr0 + 0x23e0)), 0);

		mgr1[0x23dc] = 1; /* second gate now fails */
		*(float *)&param.value = 9.0f;
		g->UpdatePerfChangeHoldTime(ctx, param);
		check_eq("+0x6e0 still updates", (unsigned int)(*(int *)(buf + 0x6e0)), 18u);
		check_eq("mgr1+0x23e0 unchanged when second gate fails",
			 (unsigned int)(*(int *)(mgr1 + 0x23e0)), 5u);

		memset(buf, 0, globalSize);
		g_onExtModeSetChangeCalls = 0;
		mgr1[0x23d1] = 0;
		param.value = 0x42;
		g->UpdateExtSetSelect(ctx, param);
		check_eq("+0x29cc0c8 always written", buf[0x29cc0c8], 0x42);
		check_eq("not initialized -> bit0 unconditionally set", (unsigned int)(buf[0x29cc0c9] & 1), 1);
		check_eq("not initialized -> no OnExtModeSetChange", (unsigned int)g_onExtModeSetChangeCalls, 0);

		buf[0x29cc0c9] = 0;
		buf[0x6ae] = 1;
		mgr1[0x23d1] = 2;
		g->UpdateExtSetSelect(ctx, param);
		check_eq("initialized, bit clear, mgr flag==2 -> OnExtModeSetChange fires",
			 (unsigned int)g_onExtModeSetChangeCalls, 1);
		check_eq("...and bit0 is NOT set in this branch (confirmed real quirk)",
			 (unsigned int)(buf[0x29cc0c9] & 1), 0);

		mgr1[0x23d1] = 9; /* != 2 */
		g->UpdateExtSetSelect(ctx, param);
		check_eq("initialized, bit clear, mgr flag!=2 -> falls through to set bit0",
			 (unsigned int)(buf[0x29cc0c9] & 1), 1);
		check_eq("still only 1 OnExtModeSetChange call total", (unsigned int)g_onExtModeSetChangeCalls, 1);

		munmap(audioBus, 0x100);
		munmap(mgr0, 0x24000);
		munmap(mgr1, 0x24000);
	}

	printf("\n[21] The 8 UpdateExtXXXCCAssign/MidiChannel handlers: shared shape,\n"
	       "     per-family stride (8 vs 9 for Slider), per-pair shared notify target\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		unsigned char *mgr = mmap32(0x24000);
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgr;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 4) = 0;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 8) = 0; /* select slot 0 = mgr */
		unsigned char *panelStatus = mmap32(0x1000);
		STGAPIFrontPanelStatus::sInstance = panelStatus;

		struct {
			const char *label;
			void (CSTGGlobal::*fn)(CSTGMessageContext &, STGConvertedParam &);
			unsigned int writeOffset;
			unsigned int stride;
			const char *family;
			bool isReal;          /* PlayMuteSwitch/SelectSwitch (sec 10.126) */
			unsigned int panelOffset;
			unsigned int paramId;
		} cases[] = {
			{ "UpdateExtKnobCCAssign", &CSTGGlobal::UpdateExtKnobCCAssign, 0x29ca3c8, 8, "Knob", false, 0, 0 },
			{ "UpdateExtKnobMidiChannel", &CSTGGlobal::UpdateExtKnobMidiChannel, 0x29c9fc8, 8, "Knob", false, 0, 0 },
			{ "UpdateExtPlayMuteSwitchCCAssign", &CSTGGlobal::UpdateExtPlayMuteSwitchCCAssign, 0x29cabc8, 8, "PlayMuteSwitch", true, 0x913, 9 },
			{ "UpdateExtPlayMuteSwitchMidiChannel", &CSTGGlobal::UpdateExtPlayMuteSwitchMidiChannel, 0x29ca7c8, 8, "PlayMuteSwitch", true, 0x913, 9 },
			{ "UpdateExtSelectSwitchCCAssign", &CSTGGlobal::UpdateExtSelectSwitchCCAssign, 0x29cb3c8, 8, "SelectSwitch", true, 0x91b, 10 },
			{ "UpdateExtSelectSwitchMidiChannel", &CSTGGlobal::UpdateExtSelectSwitchMidiChannel, 0x29cafc8, 8, "SelectSwitch", true, 0x91b, 10 },
			{ "UpdateExtSliderCCAssign", &CSTGGlobal::UpdateExtSliderCCAssign, 0x29cbc48, 9, "Slider", false, 0, 0 },
			{ "UpdateExtSliderMidiChannel", &CSTGGlobal::UpdateExtSliderMidiChannel, 0x29cb7c8, 9, "Slider", false, 0, 0 },
		};

		for (auto &c : cases) {
			memset(buf, 0, globalSize);
			memset(panelStatus, 0xff, 0x1000);
			mgr[0x23d1] = 0;
			g_onExtModeAssignChangeCalls = 0;
			g_sendUnsolicitedUIParamCalls = 0;

			buf[0x29cc0c8] = 2; /* active ext-set slot index */
			ctx.index = 3;
			param.value = 0x5a;
			(g->*c.fn)(ctx, param);
			char label[128];
			snprintf(label, sizeof(label), "%s: write at slot=2, stride=%u, ctx.index=3", c.label, c.stride);
			check_eq(label, buf[2 * c.stride + 3 + c.writeOffset], 0x5a);
			check_eq("  ...not initialized -> bit0 unconditionally set",
				 (unsigned int)(buf[0x29cc0c9] & 1), 1);
			if (c.isReal) {
				check_eq("  ...no SendUnsolicitedUIParam call while uninitialized",
					 (unsigned int)g_sendUnsolicitedUIParamCalls, 0);
			} else {
				check_eq("  ...no notify call while uninitialized",
					 (unsigned int)g_onExtModeAssignChangeCalls, 0);
			}

			buf[0x29cc0c9] = 0;
			buf[0x6ae] = 1;
			mgr[0x23d1] = 2;
			(g->*c.fn)(ctx, param);
			if (c.isReal) {
				check_eq("  ...initialized + mgr flag==2 -> real SendUnsolicitedUIParam fires",
					 (unsigned int)g_sendUnsolicitedUIParamCalls, 1);
				check_eq("  ...with the confirmed real paramId",
					 g_lastSendUnsolicitedUIParamId, c.paramId);
				check_eq("  ...and value == ctx.index",
					 g_lastSendUnsolicitedUIParamValue, ctx.index);
				check_eq("  ...STGAPIFrontPanelStatus's own real byte zeroed",
					 panelStatus[ctx.index + c.panelOffset], 0);
			} else {
				check_eq("  ...initialized + mgr flag==2 -> notify fires",
					 (unsigned int)g_onExtModeAssignChangeCalls, 1);
				check_eq("  ...dispatched to the correct family",
					 (unsigned int)(strcmp(g_lastOnExtModeAssignChangeFamily, c.family) == 0), 1);
				check_eq("  ...notify arg == ctx.index",
					 g_lastOnExtModeAssignChangeIndex, ctx.index);
			}
			check_eq("  ...bit0 NOT set in this branch (same quirk as UpdateExtSetSelect)",
				 (unsigned int)(buf[0x29cc0c9] & 1), 0);
		}
		munmap(panelStatus, 0x1000);

		munmap(mgr, 0x24000);
	}

	printf("\n[22] UpdateMFXDisable/UpdateIFXDisable/UpdateTFXDisable: 2-bit mask toggle\n"
	       "     in +0x6d4, real 3-byte MIDI CC message when initialized\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		unsigned char *midiPortMgr = mmap32(0x300);
		CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)midiPortMgr;
		SetupFakeRingCtl(midiPortMgr);

		struct {
			const char *label;
			void (CSTGGlobal::*fn)(CSTGMessageContext &, STGConvertedParam &);
			unsigned char mask;
			unsigned char cc;
		} cases[] = {
			{ "UpdateMFXDisable", &CSTGGlobal::UpdateMFXDisable, 0x12, 0x5e },
			{ "UpdateIFXDisable", &CSTGGlobal::UpdateIFXDisable, 0x09, 0x5c },
			{ "UpdateTFXDisable", &CSTGGlobal::UpdateTFXDisable, 0x24, 0x5f },
		};

		for (auto &c : cases) {
			memset(buf, 0, globalSize);
			g_writeMidiOutQueueCalls = 0;
			buf[0x6d4] = 0xff; /* all bits set, including the OTHER two masks */
			buf[0x6b8] = 0x03; /* MIDI channel nibble */

			param.value = 0; /* bit=0 -> clear the mask */
			(g->*c.fn)(ctx, param);
			char label[96];
			snprintf(label, sizeof(label), "%s: param.value==0 clears mask 0x%02x", c.label, c.mask);
			check_eq(label, (unsigned int)(buf[0x6d4] & c.mask), 0);
			check_eq("  ...bits OUTSIDE the mask are untouched (still all 1s)",
				 (unsigned int)(buf[0x6d4] & (unsigned char)~c.mask), (unsigned int)(0xff & (unsigned char)~c.mask));
			check_eq("  ...not initialized -> no MIDI message sent",
				 (unsigned int)g_writeMidiOutQueueCalls, 0);

			buf[0x6ae] = 1;
			param.value = 0;
			(g->*c.fn)(ctx, param);
			check_eq("  ...initialized, param.value==0 -> MIDI msg sent", (unsigned int)g_writeMidiOutQueueCalls, 1);
			check_eq("  ...msg[0] == (channel | 0xb0)", (unsigned int)g_lastMidiMsg[0], (unsigned int)(0x03 | 0xb0));
			check_eq("  ...msg[1] == cc number", (unsigned int)g_lastMidiMsg[1], (unsigned int)c.cc);
			check_eq("  ...msg[2] == 0x7f (value==0 -> 'on')", (unsigned int)g_lastMidiMsg[2], 0x7fu);
			check_eq("  ...length == 3", g_lastMidiMsgLen, 3u);
			check_eq("  ...called on CSTGMidiPortManager::sInstance",
				 (unsigned int)(g_lastWriteMidiOutQueueThis == CSTGMidiPortManager::sInstance), 1);

			param.value = 7; /* bit=1 -> set the mask */
			(g->*c.fn)(ctx, param);
			check_eq("  ...param.value!=0 sets mask", (unsigned int)(buf[0x6d4] & c.mask), (unsigned int)c.mask);
			check_eq("  ...msg[2] == 0x00 (value!=0 -> 'off')", (unsigned int)g_lastMidiMsg[2], 0x00u);
		}

		munmap(midiPortMgr, 0x300); /* matches the mmap32(0x300) above --
					     * was a stale 0x10 left over from
					     * before sec 10.150 enlarged this
					     * allocation. Harmless in practice
					     * (Linux munmap() rounds the length
					     * up to a whole page either way, and
					     * nothing between here and the next
					     * section's own fresh mmap32()
					     * dereferences the freed buffer), but
					     * fixed for clarity/consistency. */
	}

	printf("\n[23] The 22 UpdateXXXCCAssign handlers: 120-slot claim table at\n"
	       "     +0x29cc11c, real coupling with the UpdateXXXMIDIChannel family's\n"
	       "     own selectorOffset field, deassign/reassign/cross-handler behavior\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		const unsigned int selOff = 0x29cc0d1;   /* SongPunch */
		const unsigned int tag = 0x17;
		auto slotAt = [&](unsigned int i) { return buf + 0x29cc11c + i * 8; };

		memset(buf, 0, globalSize);
		buf[selOff - 1] = 0x42; /* the paired MIDIChannel "value" field, read as valueByte */

		printf("  -- basic assign --\n");
		param.value = 10;
		g->UpdateSongPunchCCAssign(ctx, param);
		check_eq("selector field == 10", buf[selOff], 10);
		check_eq("slot[10] claimed-flag == 1", slotAt(10)[0], 1);
		check_eq("slot[10] free-flag == 0", slotAt(10)[2], 0);
		check_eq("slot[10] tag == 0x17", *(unsigned int *)(slotAt(10) + 4), tag);
		check_eq("slot[10] valueByte == base[selOff-1] (0x42)", slotAt(10)[1], 0x42);

		printf("  -- reassign to a different CC: scan clears the OLD slot --\n");
		param.value = 20;
		g->UpdateSongPunchCCAssign(ctx, param);
		check_eq("selector field == 20", buf[selOff], 20);
		check_eq("slot[20] claimed-flag == 1", slotAt(20)[0], 1);
		check_eq("slot[10] claimed-flag cleared by the scan pass", slotAt(10)[0], 0);

		printf("  -- cross-handler independence: a DIFFERENT tag's slot is untouched --\n");
		memset(buf, 0, globalSize);
		unsigned char *foreignSlot = slotAt(30);
		foreignSlot[0] = 1;      /* claimed */
		foreignSlot[2] = 0;      /* free-flag == 0 (would match a scan by tag) */
		*(unsigned int *)(foreignSlot + 4) = 0x14; /* ProgramUp's own tag, NOT SongPunch's */
		param.value = 10;
		g->UpdateSongPunchCCAssign(ctx, param);
		check_eq("foreign-tagged slot[30] untouched by SongPunch's own scan", foreignSlot[0], 1);

		printf("  -- real deassign: valid previous selector gets cleared --\n");
		memset(buf, 0, globalSize);
		param.value = 15;
		g->UpdateSongPunchCCAssign(ctx, param);
		check_eq("setup: slot[15] claimed", slotAt(15)[0], 1);
		param.value = 0xff;
		g->UpdateSongPunchCCAssign(ctx, param);
		check_eq("deassign: selector field reset to 0xff", buf[selOff], 0xff);
		check_eq("deassign: slot[15] claimed-flag cleared", slotAt(15)[0], 0);

		printf("  -- idempotent deassign: selector already sentinel-like falls through\n"
		       "     to the SAME assign code, claiming slot 0xff itself (confirmed real quirk) --\n");
		memset(buf, 0, globalSize);
		buf[selOff] = 0x80; /* already "negative" as a signed byte -- sentinel-like */
		param.value = 0xff;
		g->UpdateSongPunchCCAssign(ctx, param);
		check_eq("selector field == 0xff (re-set, not left at 0x80)", buf[selOff], 0xff);
		check_eq("slot[0xff] claimed-flag == 1 (assign code ran with freshByte=0xff)",
			 slotAt(0xff)[0], 1);
		check_eq("slot[0xff] tag == 0x17", *(unsigned int *)(slotAt(0xff) + 4), tag);

		printf("  -- boundary: param.value > 0x77 (signed-negative byte) skips the scan --\n");
		memset(buf, 0, globalSize);
		unsigned char *preExisting = slotAt(5);
		preExisting[0] = 1;
		preExisting[2] = 0;
		*(unsigned int *)(preExisting + 4) = tag; /* matches SongPunch's own tag */
		param.value = 0x78; /* (signed char)0x78 == 120 > 0x77 (119) -- skips the scan */
		g->UpdateSongPunchCCAssign(ctx, param);
		check_eq("param.value==0x78 (>0x77) skips the scan -- slot[5] still claimed",
			 preExisting[0], 1);
		check_eq("...but the assign path still runs (0x78 != 0xff)", buf[selOff], 0x78);
	}

	printf("\n[24] UpdateChordSwCCAssign (23rd CCAssign member), UpdateProgramChangeEnable/\n"
	       "     UpdateBankChangeEnable (shared CSTGMidiDispatcher cache clear), UpdateMasterTune\n"
	       "     (unconditional write + 16-list + special-list walk)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;

		printf("  -- UpdateChordSwCCAssign: same shape as the other 22 CCAssign members --\n");
		memset(buf, 0, globalSize);
		param.value = 12;
		g->UpdateChordSwCCAssign(ctx, param);
		check_eq("selector field (0x29cc113) == 12", buf[0x29cc113], 12);
		check_eq("slot[12] claimed", buf[0x29cc11c + 12 * 8], 1);
		check_eq("slot[12] tag == 0x40", *(unsigned int *)(buf + 0x29cc11c + 12 * 8 + 4), 0x40u);

		printf("  -- UpdateProgramChangeEnable/UpdateBankChangeEnable --\n");
		unsigned char *midiDisp = mmap32(0x100);
		CSTGMidiDispatcher::sInstance = (CSTGMidiDispatcher *)midiDisp;
		struct {
			const char *label;
			void (CSTGGlobal::*fn)(CSTGMessageContext &, STGConvertedParam &);
			unsigned int flagOffset;
		} enableCases[] = {
			{ "UpdateProgramChangeEnable", &CSTGGlobal::UpdateProgramChangeEnable, 0x6d5 },
			{ "UpdateBankChangeEnable", &CSTGGlobal::UpdateBankChangeEnable, 0x6d6 },
		};
		for (auto &c : enableCases) {
			memset(buf, 0, globalSize);
			memset(midiDisp, 0xaa, 0x100);

			param.value = 1;
			(g->*c.fn)(ctx, param);
			char label[96];
			snprintf(label, sizeof(label), "%s: enabling stores 1, does NOT clear cache", c.label);
			check_eq(label, buf[c.flagOffset], 1);
			check_eq("  ...dispatcher cache untouched while enabling", midiDisp[0x30], 0xaa);

			param.value = 0;
			(g->*c.fn)(ctx, param);
			check_eq("  ...disabling stores 0", buf[c.flagOffset], 0);
			int allCleared = 1;
			for (unsigned int i = 0; i < 16; i++)
				if (midiDisp[0x30 + i] != 0 || midiDisp[0x50 + i] != 0)
					allCleared = 0;
			check_eq("  ...disabling clears all 32 confirmed cache bytes", (unsigned int)allCleared, 1);
		}
		munmap(midiDisp, 0x100);

		printf("  -- UpdateMasterTune: unconditional write, gated list-walk, special list --\n");
		memset(buf, 0, globalSize);
		CSTGGlobal::sInstance = g;

		param.value = 0x99;
		g->UpdateMasterTune(ctx, param);
		check_eq("+0x6bc always written, even while not initialized",
			 (unsigned int)(*(int *)(buf + 0x6bc)), 0x99u);
		check_eq("not initialized -> no UpdateGlobalTune calls", (unsigned int)g_updateGlobalTuneCalls, 0);

		buf[0x6ae] = 1;
		buf[0x6b8] = 5; /* active slot */
		/* Host/target pointer-width fix (matching global.cpp's own
		 * UpdateMasterTune fix): every pointer field here is a
		 * truncated 32-bit value on the real target, so both the
		 * node/payload buffers and the list-head array entries must
		 * be written as `unsigned int`, not native host pointers. */
		unsigned char *payload = mmap32(0x10);
		unsigned char *node = mmap32(0x10);
		*(unsigned int *)(node + 0) = 0;          /* next = NULL (single-node list) */
		*(unsigned int *)(node + 8) = (unsigned int)(unsigned long)payload; /* payload pointer */
		*(unsigned int *)(buf + 0x29c99cc + 5 * 12) = (unsigned int)(unsigned long)node; /* list[5]'s head */

		unsigned char *specialPayload = mmap32(0x10);
		unsigned char *specialNode = mmap32(0x10);
		*(unsigned int *)(specialNode + 0) = 0;
		*(unsigned int *)(specialNode + 8) = (unsigned int)(unsigned long)specialPayload;
		*(unsigned int *)(buf + 0x29c9a8c) = (unsigned int)(unsigned long)specialNode;

		g_updateGlobalTuneCalls = 0;
		*(float *)&param.value = 4.5f;
		g->UpdateMasterTune(ctx, param);
		check_eq("list[5] (matching active slot) + special list both walked",
			 (unsigned int)g_updateGlobalTuneCalls, 2);

		munmap(payload, 0x10);
		munmap(node, 0x10);
		munmap(specialPayload, 0x10);
		munmap(specialNode, 0x10);
	}

	printf("\n[25] UpdateRTKnobFuncCCAssign/UpdatePadFuncCCAssign: indexed CCAssign\n"
	       "     variant, ctx.index-bounded, dynamic per-index tag\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		auto slotAt = [&](unsigned int i) { return buf + 0x29cc11c + i * 8; };

		struct {
			const char *label;
			void (CSTGGlobal::*fn)(CSTGMessageContext &, STGConvertedParam &);
			unsigned int valueOff, selOff, tagBase;
		} cases[] = {
			{ "UpdateRTKnobFuncCCAssign", &CSTGGlobal::UpdateRTKnobFuncCCAssign, 0x29cc0da, 0x29cc0e2, 0x1c },
			{ "UpdatePadFuncCCAssign", &CSTGGlobal::UpdatePadFuncCCAssign, 0x29cc0ea, 0x29cc0f2, 0x36 },
		};

		for (auto &c : cases) {
			memset(buf, 0, globalSize);
			ctx.index = 9; /* out of bounds */
			param.value = 5;
			(g->*c.fn)(ctx, param);
			char label[96];
			snprintf(label, sizeof(label), "%s: ctx.index==9 (out of bounds) is a no-op", c.label);
			check_eq(label, buf[c.selOff + 0], 0);

			ctx.index = 3;
			buf[c.valueOff + 3] = 0x55; /* paired value-array entry for index 3 */
			param.value = 10;
			(g->*c.fn)(ctx, param);
			check_eq("selector[ctx.index] == 10", buf[c.selOff + 3], 10);
			check_eq("slot[10] claimed", slotAt(10)[0], 1);
			check_eq("slot[10] valueByte == paired value-array entry", slotAt(10)[1], 0x55);
			check_eq("slot[10] freeFlag == 0", slotAt(10)[2], 0);
			check_eq("slot[10] tag == ctx.index + tagBase",
				 *(unsigned int *)(slotAt(10) + 4), 3 + c.tagBase);

			/* reassign at the same index: scan clears the OLD slot
			 * since it shares this index's own dynamic tag */
			param.value = 20;
			(g->*c.fn)(ctx, param);
			check_eq("reassign clears the old slot (same dynamic tag)", slotAt(10)[0], 0);
			check_eq("new slot claimed", slotAt(20)[0], 1);

			/* deassign */
			param.value = 0xff;
			(g->*c.fn)(ctx, param);
			check_eq("deassign: selector[ctx.index] reset to 0xff", buf[c.selOff + 3], 0xff);
			check_eq("deassign: slot[20] claimed-flag cleared", slotAt(20)[0], 0);
		}
	}

	printf("\n[26] UpdateVJSXAssignment/UpdateVJSYAssignment: 3-way mode dispatch\n"
	       "     to resolve the current CSTGPerformance, gated MIDI controller send\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		unsigned char *midiDisp = mmap32(0x10);
		CSTGMidiDispatcher::sInstance = (CSTGMidiDispatcher *)midiDisp;

		/* CSTGPerformance::IsCurrentlyActive() is real now (sec 10.144):
		 * resolves the active CSTGPerformanceVarsManager via the same
		 * sInstance[8]-selector idiom used throughout this file, then
		 * checks mgr->fieldAt(0x23d1)==2 && mgr->fieldAt(0x23d4)==this.
		 * Wire up a real manager buffer so each case below can control
		 * exactly which address reads as "currently active", the same
		 * role g_isCurrentlyActiveReturn/g_lastIsCurrentlyActiveThis
		 * used to play as test-only overrides. */
		unsigned char *perfVarsMgr26 = mmap32(0x23e0);
		memset(perfVarsMgr26, 0, 0x23e0);
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) =
			(unsigned int)(unsigned long)perfVarsMgr26;
		CSTGPerformanceVarsManager::sInstance[8] = 0;
#define SET_ACTIVE(addr) do { \
		perfVarsMgr26[0x23d1] = 2; \
		*(unsigned int *)(perfVarsMgr26 + 0x23d4) = (unsigned int)(unsigned long)(addr); \
	} while (0)
#define SET_INACTIVE() (perfVarsMgr26[0x23d1] = 0)

		struct {
			const char *label;
			void (CSTGGlobal::*fn)(CSTGMessageContext &, STGConvertedParam &);
			unsigned int selOff;
		} cases[] = {
			{ "UpdateVJSXAssignment", &CSTGGlobal::UpdateVJSXAssignment, 0x6c0 },
			{ "UpdateVJSYAssignment", &CSTGGlobal::UpdateVJSYAssignment, 0x6c1 },
		};

		for (auto &c : cases) {
			memset(buf, 0, globalSize);
			g_handleControllerCalls = 0;

			param.value = 0x50;
			(g->*c.fn)(ctx, param);
			char label[96];
			snprintf(label, sizeof(label), "%s: selector always written, even uninitialized", c.label);
			check_eq(label, buf[c.selOff], 0x50);
			check_eq("  ...no MIDI dispatch while uninitialized", (unsigned int)g_handleControllerCalls, 0);

			printf("  -- mode 0 (Program), progIdx != sentinel --\n");
			buf[0x6ae] = 1;
			*(int *)(buf + 0x684) = 0;
			*(int *)(buf + 0x698) = 5;  /* program index */
			*(int *)(buf + 0x68c) = 2;  /* program bank */
			SET_ACTIVE(buf + (5 * 0xcec + 2 * 0x67603 + 0x132e4d0 + 3));
			buf[0x6b9] = 3; /* MIDI channel */
			param.value = 0x22;
			(g->*c.fn)(ctx, param);
			check_eq("  ...HandleController fires when the mode-0 program address reads active",
				 (unsigned int)g_handleControllerCalls, 1);
			check_eq("  ...arg1 == channel", (unsigned int)g_lastHandleControllerArg1, 3u);
			check_eq("  ...arg2 == selector (0x22)", (unsigned int)g_lastHandleControllerArg2, 0x22u);
			check_eq("  ...arg3 == literal 0x40", (unsigned int)g_lastHandleControllerArg3, 0x40u);
			check_eq("  ...source == 1", g_lastHandleControllerSource, 1);
			check_eq("  ...target == -1", g_lastHandleControllerTarget, -1);

			printf("  -- mode 0, progIdx == 0xfffe sentinel -> fixed special address --\n");
			g_handleControllerCalls = 0;
			*(int *)(buf + 0x698) = 0xfffe;
			SET_ACTIVE(buf + 0x2976e33);
			(g->*c.fn)(ctx, param);
			check_eq("  ...resolves to the fixed sentinel address (fires only because "
				 "THAT exact address was armed active)",
				 (unsigned int)g_handleControllerCalls, 1);

			printf("  -- mode 1 (Combi) --\n");
			g_handleControllerCalls = 0;
			*(int *)(buf + 0x684) = 1;
			*(int *)(buf + 0x69c) = 7;  /* combi index */
			*(int *)(buf + 0x690) = 1;  /* combi bank */
			SET_ACTIVE(buf + (7 * 0x19e7 + 1 * 0xcf381 + 0x1c77f10 + 6));
			(g->*c.fn)(ctx, param);
			check_eq("  ...resolves to the mode-1 combi address",
				 (unsigned int)g_handleControllerCalls, 1);

			printf("  -- mode 2 (Sequence) --\n");
			g_handleControllerCalls = 0;
			*(int *)(buf + 0x684) = 2;
			*(int *)(buf + 0x6a0) = 11; /* sequence index */
			SET_ACTIVE(buf + (11 * 0x1cad + 0x27cd024));
			(g->*c.fn)(ctx, param);
			check_eq("  ...resolves to the mode-2 sequence address",
				 (unsigned int)g_handleControllerCalls, 1);

			printf("  -- gating: not active -> no dispatch; negative selector -> no dispatch --\n");
			g_handleControllerCalls = 0;
			SET_INACTIVE();
			(g->*c.fn)(ctx, param);
			check_eq("  ...IsCurrentlyActive()==false suppresses the MIDI send",
				 (unsigned int)g_handleControllerCalls, 0);

			SET_ACTIVE(buf + (11 * 0x1cad + 0x27cd024)); /* still mode 2 from above */
			param.value = 0x80; /* negative as a signed byte */
			(g->*c.fn)(ctx, param);
			check_eq("  ...negative selector suppresses the MIDI send",
				 (unsigned int)g_handleControllerCalls, 0);
		}
#undef SET_ACTIVE
#undef SET_INACTIVE
		munmap(perfVarsMgr26, 0x23e0);

		munmap(midiDisp, 0x10);
	}

	printf("\n[27] UpdateProgramDrumTrackMidiChannel/UpdateKeyTranspose/\n"
	       "     UpdateLocalControl+UpdateLocalOn/UpdateMIDIChannel (the last 5 UpdateXXX)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		CSTGMessageContext ctx;
		STGConvertedParam param;
		unsigned char *msgProc = mmap32(0x100);
		unsigned char *midiPortMgr = mmap32(0x300);
		unsigned char *smoother = mmap32(0x10);
		unsigned char *voiceAlloc = mmap32(0x10);
		unsigned char *controllerRt = mmap32(0x10);
		unsigned char *vectorMgr = mmap32(0x1c9e8); /* real CSTGVectorManager size (sec 10.64) --
							      * OnUpdateGlobalMidiChannel (sec 10.124) writes
							      * to +0x19da4/+0x1b758 */
		CSTGMessageProcessor::sInstance = (CSTGMessageProcessor *)msgProc;
		CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)midiPortMgr;
		SetupFakeRingCtl(midiPortMgr);
		CSTGSmoother::sInstance = (CSTGSmoother *)smoother;
		CSTGVoiceAllocator::sInstance = (CSTGVoiceAllocator *)voiceAlloc;
		CSTGControllerRTData::sInstance = (CSTGControllerRTData *)controllerRt;
		CSTGVectorManager::sInstance = (CSTGVectorManager *)vectorMgr;

		printf("  -- UpdateProgramDrumTrackMidiChannel --\n");
		memset(buf, 0, globalSize);
		msgProc[0x48] = 1; /* skip the smoother/steal cascade */
		g_resetAllControllersCalls = 0;
		buf[0x6ae] = 1;
		buf[0x6ba] = 5; /* old channel */
		param.value = 9;
		g->UpdateProgramDrumTrackMidiChannel(ctx, param);
		check_eq("new channel stored at +0x6ba", buf[0x6ba], 9);
		check_eq("embedded DrumTrackSlot's own +0x10 updated (real OnUpdateProgramDrumTrackMidiChannel, sec 10.133)",
			 buf[0x2977c08 + 0x10], 9);
		check_eq("ResetAllControllers called twice (old channel, then new)",
			 (unsigned int)g_resetAllControllersCalls, 2);

		printf("  -- UpdateKeyTranspose --\n");
		memset(buf, 0, globalSize);
		g_writeQueueCalls = 0;
		buf[0x6ae] = 0;
		param.value = 3;
		g->UpdateKeyTranspose(ctx, param);
		check_eq("not initialized -> stores value, no MIDI send", buf[0x6ad], 3);
		check_eq("  ...no MIDI message while uninitialized", (unsigned int)g_writeQueueCalls, 0);

		buf[0x6ae] = 1;
		buf[0x6b8] = 2;
		param.value = 7;
		g->UpdateKeyTranspose(ctx, param);
		check_eq("initialized -> stores value", buf[0x6ad], 7);
		check_eq("  ...sends a 5-byte MIDI message", (unsigned int)g_writeQueueCalls, 1);
		check_eq("  ...msg[0] == (channel | 0xb0)", (unsigned int)g_lastQueueMsg[0], (unsigned int)(2 | 0xb0));
		check_eq("  ...msg[1] == 0x79", (unsigned int)g_lastQueueMsg[1], 0x79u);
		check_eq("  ...msg[2] == 0x09", (unsigned int)g_lastQueueMsg[2], 0x09u);
		check_eq("  ...msg[3] == 0x05", (unsigned int)g_lastQueueMsg[3], 0x05u);
		check_eq("  ...msg[4] == 0xff", (unsigned int)g_lastQueueMsg[4], 0xffu);

		printf("  -- UpdateLocalControl(bool) / UpdateLocalOn (confirmed identical) --\n");
		memset(buf, 0, globalSize);
		buf[0x6ae] = 1;
		buf[0x6b8] = 4;
		buf[0x6b9] = 1;
		g_writeQueueCalls = 0;
		g_resetAllControllersCalls = 0;
		/* UpdateLocalControl's own real body unconditionally calls
		 * ResetAllJumpCatch(), which dereferences the active
		 * CSTGPerformanceVarsManager -- intervening sections (mgr0/mgr
		 * above) reassign then free their own buffers there, leaving
		 * CSTGPerformanceVarsManager::sInstance dangling by this point.
		 * Re-point it at a fresh, deliberately leaked buffer (same
		 * reasoning as [17]'s own comment on why these can't be
		 * munmap'd once sInstance may reference them later). */
		unsigned char *mgrLocalCtrl = mmap32(0x2400);
		memset(mgrLocalCtrl, 0, 0x2400);
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgrLocalCtrl;
		CSTGPerformanceVarsManager::sInstance[8] = 0;
		g->UpdateLocalControl(true);
		check_eq("state stored at +0x6af", buf[0x6af], 1);
		check_eq("ResetAllControllers fired (current channel)", (unsigned int)g_resetAllControllersCalls, 1);
		check_eq("MIDI message msg[2] == 5 for true", (unsigned int)g_lastQueueMsg[2], 5u);

		g->UpdateLocalControl(false);
		check_eq("state stored == 0", buf[0x6af], 0);
		check_eq("MIDI message msg[2] == 6 for false", (unsigned int)g_lastQueueMsg[2], 6u);

		memset(buf, 0, globalSize);
		buf[0x6ae] = 1;
		buf[0x6b8] = 4;
		buf[0x6b9] = 1;
		param.value = 1;
		g->UpdateLocalOn(ctx, param);
		check_eq("UpdateLocalOn(param.value=1) behaves exactly like UpdateLocalControl(true)",
			 buf[0x6af], 1);
		check_eq("  ...same msg[2]==5 encoding", (unsigned int)g_lastQueueMsg[2], 5u);

		printf("  -- UpdateMIDIChannel (largest handler) --\n");
		memset(buf, 0, globalSize);
		msgProc[0x48] = 1; /* skip the held-key-list cascade for this pass */
		g_resetAllControllersCalls = 0;
		/* buf was just memset to 0, so the real +0x29c990c table entry
		 * for every slot's own fieldAt(4)==0 is null -> IsActive()'s
		 * real early-return path fires naturally (no override needed). */
		buf[0x6ae] = 1;
		buf[0x684] = 0; /* mode != 2, so +0x6b9 also updates */
		unsigned char *perfMgr = mmap32(0x24000);
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)perfMgr;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 8) = 0;
		perfMgr[0x23d1] = 9; /* != 2 -> takes the MIDI-message branch, not UpdateDModRoutings */
		g_writeQueueCalls = 0;

		param.value = 6;
		g->UpdateMIDIChannel(ctx, param);
		check_eq("new channel stored at +0x6b8", buf[0x6b8], 6);
		check_eq("mode != 2 -> +0x6b9 also updated", buf[0x6b9], 6);
		check_eq("CSTGVectorManager's own +0x19da4 field updated", vectorMgr[0x19da4], 6);
		check_eq("  ...and +0x1b758", vectorMgr[0x1b758], 6);
		check_eq("embedded ProgramModeProgramSlot's own +0x10 channel field set",
			 buf[0x2977b1f + 0x10], 6);
		check_eq("mgr flag != 2 -> sends MIDI message, not UpdateDModRoutings",
			 (unsigned int)g_writeQueueCalls, 1);
		check_eq("  ...msg[2] == 0x08", (unsigned int)g_lastQueueMsg[2], 0x08u);
		check_eq("  ...UpdateDModRoutings NOT called", (unsigned int)g_dmodRoutingsVtableCalls, 0);

		printf("  -- UpdateMIDIChannel: mgr flag == 2 -> UpdateDModRoutings instead --\n");
		perfMgr[0x23d1] = 2;
		g_writeQueueCalls = 0;
		/* UpdateDModRoutings is called on (mgr+0x20)->fieldAt(0x2114)
		 * ->fieldAt(0x23d4)'s own vtable slot 33 (sec 10.135) -- build a
		 * real, valid object graph so the call doesn't dereference null. */
		unsigned char *dmodP1 = mmap32(0x2400);
		unsigned char *dmodP2 = mmap32(0x100);
		memset(dmodP1, 0, 0x2400);
		memset(dmodP2, 0, 0x100);
		*(unsigned int *)(perfMgr + 0x20 + 0x2114) = (unsigned int)(unsigned long)dmodP1;
		*(unsigned int *)(dmodP1 + 0x23d4) = (unsigned int)(unsigned long)dmodP2;
		void *dmodVtable[34];
		for (int i = 0; i < 34; i++)
			dmodVtable[i] = 0;
		dmodVtable[33] = (void *)DmodRoutingsVtableTrap;
		*(void ***)dmodP2 = dmodVtable;
		g_dmodRoutingsVtableCalls = 0;
		g->UpdateMIDIChannel(ctx, param);
		check_eq("UpdateDModRoutings fires when mgr flag == 2",
			 (unsigned int)g_dmodRoutingsVtableCalls, 1);
		check_eq("  ...on the resolved fieldAt(0x2114)->fieldAt(0x23d4) object",
			 (unsigned int)(g_lastDmodRoutingsVtableThis == dmodP2), 1u);
		check_eq("  ...with the original CSTGEffectRackVars (mgr+0x20) as arg1",
			 (unsigned int)(g_lastDmodRoutingsVtableArg1 == perfMgr + 0x20), 1u);
		check_eq("  ...no MIDI message sent in this branch", (unsigned int)g_writeQueueCalls, 0);
		/* dmodP1/dmodP2 deliberately NOT munmap'd -- perfMgr's own
		 * fieldAt(0x2134) keeps pointing at dmodP1 for the rest of the
		 * process's life, and perfMgr itself outlives this block; see
		 * sec 10.133's own established reasoning for this class of fix. */

		munmap(perfMgr, 0x24000);
		munmap(msgProc, 0x100);
		munmap(midiPortMgr, 0x300);
		munmap(smoother, 0x10);
		munmap(voiceAlloc, 0x10);
		munmap(controllerRt, 0x10);
		munmap(vectorMgr, 0x1c9e8);
	}

	printf("\n[27b] CSTGProgramModeProgramSlot::OnUpdateGlobalMidiChannel (sec 10.125)\n");
	{
		CSTGGlobal::sInstance = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		unsigned char *vd = mmap32(0x30);
		memset(vd, 0, 0x30);

		CSTGProgramModeProgramSlot *slot =
			(CSTGProgramModeProgramSlot *)(buf + 0x2977b1f);

#define P32(p) ((unsigned int)(unsigned long)(p))
#define FIELD32(p) (*(unsigned int *)(p))

		/* IsActive()/AccessActiveSlotVoiceData() are real now (sec
		 * 10.142): both resolve `slot->fieldAt(4)` (0 here, buf was
		 * just zeroed) through the real `CSTGGlobal::sInstance+
		 * 0x29c990c` table into a node whose own +0x8 is the payload,
		 * and IsActive() additionally requires the payload's own
		 * +0x34 back-pointer to equal `slot`. Wire up real state
		 * instead of the old test-only override flags. */
		unsigned char *node1 = mmap32(0x10);
		memset(node1, 0, 0x10);
		*(unsigned int *)(node1 + 0x8) = P32(vd);
		*(unsigned int *)(buf + 0x29c990c) = P32(node1);
		*(unsigned int *)(vd + 0x34) = P32(slot);

		printf("  -- insert into a previously-empty bucket --\n");
		slot->OnUpdateGlobalMidiChannel(6);
		unsigned char *bucket6 = buf + 0x29c99cc + 6 * 0xc;
		check_eq("channel field set", buf[0x2977b1f + 0x10], 6);
		check_eq("bucket6.head == &vd[0x14]", FIELD32(bucket6), P32(vd + 0x14));
		check_eq("bucket6.tail == &vd[0x14] too (single-node bucket)",
			 FIELD32(bucket6 + 4), P32(vd + 0x14));
		check_eq("bucket6.count == 1", FIELD32(bucket6 + 8), 1u);
		check_eq("vd->container == &bucket6", FIELD32(vd + 0x20), P32(bucket6));

		printf("  -- move to a different channel: unlinks from bucket6, relinks into bucket9 --\n");
		slot->OnUpdateGlobalMidiChannel(9);
		unsigned char *bucket9 = buf + 0x29c99cc + 9 * 0xc;
		check_eq("channel field updated", buf[0x2977b1f + 0x10], 9);
		check_eq("bucket6 emptied: head == 0", FIELD32(bucket6), 0u);
		check_eq("bucket6 emptied: tail == 0", FIELD32(bucket6 + 4), 0u);
		check_eq("bucket6.count decremented to 0", FIELD32(bucket6 + 8), 0u);
		check_eq("bucket9.head == &vd[0x14]", FIELD32(bucket9), P32(vd + 0x14));
		check_eq("bucket9.tail == &vd[0x14]", FIELD32(bucket9 + 4), P32(vd + 0x14));
		check_eq("bucket9.count == 1", FIELD32(bucket9 + 8), 1u);
		check_eq("vd->container == &bucket9", FIELD32(vd + 0x20), P32(bucket9));

		printf("  -- a SECOND slot joining the same bucket: real push-front splice --\n");
		unsigned char *vd2 = mmap32(0x30);
		memset(vd2, 0, 0x30);
		unsigned char *slot2Storage = mmap32(0x20);
		memset(slot2Storage, 0, 0x20);
		CSTGProgramModeProgramSlot *slot2 = (CSTGProgramModeProgramSlot *)slot2Storage;
		/* slot2's own fieldAt(4) is also 0 (freshly zeroed storage), so
		 * it resolves through the SAME table[0] entry as `slot` --
		 * repoint it at a second node/payload pair (real behavior:
		 * whichever object was linked into this global index most
		 * recently is what AccessActiveSlotVoiceData() resolves to). */
		unsigned char *node2 = mmap32(0x10);
		memset(node2, 0, 0x10);
		*(unsigned int *)(node2 + 0x8) = P32(vd2);
		*(unsigned int *)(buf + 0x29c990c) = P32(node2);
		*(unsigned int *)(vd2 + 0x34) = P32(slot2);
		slot2->OnUpdateGlobalMidiChannel(9);
		check_eq("bucket9.head now == &vd2[0x14] (new head, pushed to front)",
			 FIELD32(bucket9), P32(vd2 + 0x14));
		check_eq("bucket9.tail STILL == &vd[0x14] (original node, unmoved)",
			 FIELD32(bucket9 + 4), P32(vd + 0x14));
		check_eq("bucket9.count == 2", FIELD32(bucket9 + 8), 2u);
		check_eq("vd2's own +0x14 == old head (vd)", FIELD32(vd2 + 0x14), P32(vd + 0x14));
		check_eq("old head's (vd's) own +0x18 spliced to point at the new node (vd2)",
			 FIELD32(vd + 0x18), P32(vd2 + 0x14));

		printf("  -- inactive slot: only the channel field updates, no list touched --\n");
		/* table[0] now points at node2/vd2, whose own +0x34 back-
		 * pointer is slot2, not slot -- slot->IsActive() therefore
		 * returns false naturally, no override needed. */
		unsigned int bucket9CountBefore = *(unsigned int *)(bucket9 + 8);
		slot->OnUpdateGlobalMidiChannel(3);
		check_eq("channel field still updates even when inactive", buf[0x2977b1f + 0x10], 3);
		check_eq("bucket9 untouched (slot was inactive, no unlink attempted)",
			 *(unsigned int *)(bucket9 + 8), bucket9CountBefore);

		munmap(vd, 0x30);
		munmap(vd2, 0x30);
		munmap(node1, 0x10);
		munmap(node2, 0x10);
		munmap(slot2Storage, 0x20);
#undef P32
#undef FIELD32
	}

	printf("\n[27c] CSTGProgramSlot::IsActive/AccessActiveSlotVoiceData/"
	       "HasActiveSlotVoiceData/HasActiveVoices (sec 10.142)\n");
	{
		CSTGGlobal::sInstance = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

#define P32(p) ((unsigned int)(unsigned long)(p))

		/* mmap32'd (not stack-allocated): IsActive() compares this
		 * object's OWN address, truncated to the real target's packed
		 * 32-bit pointer width -- a stack address on this 64-bit host
		 * would silently lose bits on that truncation and never
		 * round-trip back to a matching compare. */
		unsigned char *slotStorage = mmap32(0x8);
		memset(slotStorage, 0, 0x8);
		CSTGProgramSlot *slot = (CSTGProgramSlot *)slotStorage;
		slotStorage[0x4] = 2; /* this slot's own table index */

		printf("  -- no node linked at all (table entry null) --\n");
		check_eq("IsActive() false", (unsigned int)slot->IsActive(), 0u);
		check_eq("AccessActiveSlotVoiceData() null",
			 (unsigned int)(slot->AccessActiveSlotVoiceData() == 0), 1u);
		check_eq("HasActiveSlotVoiceData() false",
			 (unsigned int)slot->HasActiveSlotVoiceData(), 0u);
		check_eq("HasActiveVoices() false",
			 (unsigned int)slot->HasActiveVoices(), 0u);

		printf("  -- node linked, but payload's own +0x34 back-pointer is a DIFFERENT slot --\n");
		unsigned char *node = mmap32(0x10);
		memset(node, 0, 0x10);
		unsigned char *payload = mmap32(0x60);
		memset(payload, 0, 0x60);
		*(unsigned int *)(node + 0x8) = P32(payload);
		*(unsigned int *)(buf + 0x29c990c + 2 * 12) = P32(node);
		unsigned char *otherSlotStorage = mmap32(0x8);
		memset(otherSlotStorage, 0, 0x8);
		*(unsigned int *)(payload + 0x34) = P32(otherSlotStorage);
		check_eq("IsActive() false (back-pointer mismatch)",
			 (unsigned int)slot->IsActive(), 0u);
		check_eq("AccessActiveSlotVoiceData() still returns the payload"
			 " regardless of the back-pointer check",
			 (unsigned int)(slot->AccessActiveSlotVoiceData() == payload), 1u);
		check_eq("HasActiveSlotVoiceData() true (a node IS linked)",
			 (unsigned int)slot->HasActiveSlotVoiceData(), 1u);
		check_eq("HasActiveVoices() false (both count fields still 0)",
			 (unsigned int)slot->HasActiveVoices(), 0u);

		printf("  -- back-pointer matches this slot -- IsActive() true --\n");
		*(unsigned int *)(payload + 0x34) = P32(slot);
		check_eq("IsActive() true", (unsigned int)slot->IsActive(), 1u);

		printf("  -- HasActiveVoices(): confirmed real fields +0x4c (u16) + +0x58 (u16) --\n");
		*(unsigned short *)(payload + 0x4c) = 0;
		*(unsigned short *)(payload + 0x58) = 0;
		check_eq("both zero -> false", (unsigned int)slot->HasActiveVoices(), 0u);
		*(unsigned short *)(payload + 0x4c) = 3;
		check_eq("+0x4c alone nonzero -> true", (unsigned int)slot->HasActiveVoices(), 1u);
		*(unsigned short *)(payload + 0x4c) = 0;
		*(unsigned short *)(payload + 0x58) = 5;
		check_eq("+0x58 alone nonzero -> true", (unsigned int)slot->HasActiveVoices(), 1u);

		printf("  -- node linked but payload pointer itself is null --\n");
		*(unsigned int *)(node + 0x8) = 0;
		check_eq("HasActiveSlotVoiceData() still true (node itself is non-null)",
			 (unsigned int)slot->HasActiveSlotVoiceData(), 1u);
		check_eq("AccessActiveSlotVoiceData() null", (unsigned int)(slot->AccessActiveSlotVoiceData() == 0), 1u);
		check_eq("HasActiveVoices() false (null payload)", (unsigned int)slot->HasActiveVoices(), 0u);
		/* IsActive() deliberately NOT exercised in this exact state: the
		 * real disassembly has no null check on the payload before
		 * reading its own +0x34 back-pointer (`mov eax,[edx+0x8];
		 * cmp ecx,[eax+0x34]` -- unconditional), so calling IsActive()
		 * here would dereference near-null memory, matching a real,
		 * faithfully-reproduced hazard rather than a test bug. Not
		 * independently observed to occur on the real hardware (a
		 * linked node with a null payload), so left undemonstrated
		 * rather than worked around. */

		munmap(node, 0x10);
		munmap(payload, 0x60);
		munmap(slotStorage, 0x8);
		munmap(otherSlotStorage, 0x8);
#undef P32
	}

	printf("\n[28] CSTGAudioInput -- ctor + 9 UpdateXXX handlers (sec 10.80)\n");
	{
		CSTGGlobal::sInstance = (CSTGGlobal *)buf;
		unsigned char *g = buf;

		unsigned char *aiStorage = mmap32(0x80);
		CSTGAudioInput *ai = (CSTGAudioInput *)aiStorage;
		/* Poison memory before construction so the "ctor only zeroes
		 * +0x64..+0x76" quirk is actually exercised, not accidentally
		 * satisfied by a zeroed mmap page. */
		memset(aiStorage, 0xAA, 0x80);
		new (ai) CSTGAudioInput();
		check_eq("ctor: +0x63 (just before the zeroed range) untouched",
			 aiStorage[0x63], 0xAA);
		check_eq("ctor: +0x64 zeroed", aiStorage[0x64], 0);
		check_eq("ctor: +0x76 zeroed", aiStorage[0x76], 0);
		check_eq("ctor: +0x77 bit0 set, bit1 cleared (0xAA -> 0xA9)",
			 aiStorage[0x77], (unsigned char)((0xAA | 0x1) & ~0x2));
		check_eq("ctor: +0x78 (just past the zeroed range) untouched",
			 aiStorage[0x78], 0xAA);

		CSTGMessageContext ctx;
		STGConvertedParam param;

		printf("  -- UpdateLevel/UpdateSend1Level/UpdateSend2Level (local-only, inactive) --\n");
		memset(aiStorage, 0, 0x80);
		ctx.index = 3;
		param.value = 12345;
		ai->UpdateLevel(ctx, param);
		check_eq("UpdateLevel: stored at +0x4+idx*4",
			 *(int *)(aiStorage + 0x4 + 3 * 4), 12345);
		param.value = 777;
		ai->UpdateSend1Level(ctx, param);
		check_eq("UpdateSend1Level: stored at +0x34+idx*4",
			 *(int *)(aiStorage + 0x34 + 3 * 4), 777);
		param.value = 888;
		ai->UpdateSend2Level(ctx, param);
		check_eq("UpdateSend2Level: stored at +0x4c+idx*4",
			 *(int *)(aiStorage + 0x4c + 3 * 4), 888);

		printf("  -- UpdateLevel, out-of-range index is a no-op --\n");
		memset(aiStorage, 0, 0x80);
		ctx.index = 6;
		param.value = 999;
		ai->UpdateLevel(ctx, param);
		check_eq("index>5: nothing written",
			 *(int *)(aiStorage + 0x4 + 6 * 4), 0);

		/* Set up an "active" performance so the live-mixer push path runs. */
		unsigned char *mgrRegion = mmap32(0x1000);
		unsigned char *mgrBase = mmap32(0x100);
		*(unsigned char **)(mgrBase + 8) = mgrRegion;
		CSTGPerformanceVarsManager::sInstance[8] = 1;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 1 * 4) =
			(unsigned int)(unsigned long)mgrBase;
		aiStorage[0x77] |= 0x2; /* mark active */

		printf("  -- UpdateLevel/Send1/Send2 (active): direct write at *(mgr+8)+idx*0x90+N --\n");
		memset(mgrRegion, 0, 0x1000);
		ctx.index = 2;
		param.value = 111;
		ai->UpdateLevel(ctx, param);
		check_eq("active UpdateLevel: *(mgr+8)+idx*0x90+0x78",
			 *(int *)(mgrRegion + 2 * 0x90 + 0x78), 111);
		param.value = 222;
		ai->UpdateSend1Level(ctx, param);
		check_eq("active UpdateSend1Level: +0x7c",
			 *(int *)(mgrRegion + 2 * 0x90 + 0x7c), 222);
		param.value = 333;
		ai->UpdateSend2Level(ctx, param);
		check_eq("active UpdateSend2Level: +0x80",
			 *(int *)(mgrRegion + 2 * 0x90 + 0x80), 333);

		printf("  -- UpdateMute --\n");
		memset(aiStorage, 0, 0x76); /* keep +0x76.. flags/active from above */
		memset(mgrRegion, 0, 0x1000);
		ctx.index = 4;
		param.value = 1;
		ai->UpdateMute(ctx, param);
		check_eq("UpdateMute: bit set at +0x76", aiStorage[0x76], (unsigned char)(1u << 4));
		check_eq("active UpdateMute: bool pushed to *(mgr+8)+idx*0x90+0x84",
			 mgrRegion[4 * 0x90 + 0x84], 1);
		param.value = 0;
		ai->UpdateMute(ctx, param);
		check_eq("UpdateMute: bit cleared at +0x76", aiStorage[0x76], 0);
		check_eq("active UpdateMute: bool cleared",
			 mgrRegion[4 * 0x90 + 0x84], 0);

		printf("  -- UpdateSolo (no local storage, purely a pass-through when active) --\n");
		g_setAudioInSoloCalls = 0;
		CSTGControllerRTData *rtSingleton = (CSTGControllerRTData *)mmap32(0x10);
		CSTGControllerRTData::sInstance = rtSingleton;
		ctx.index = 5;
		param.value = 1;
		ai->UpdateSolo(ctx, param);
		check_eq("UpdateSolo (active): SetAudioInSolo called once",
			 (unsigned int)g_setAudioInSoloCalls, 1);
		check_eq("  ...with the real slot index", g_lastSoloSlot, 5u);
		check_eq("  ...with solo=true", (unsigned int)g_lastSoloValue, 1u);
		aiStorage[0x77] &= ~0x2; /* mark inactive */
		g_setAudioInSoloCalls = 0;
		ai->UpdateSolo(ctx, param);
		check_eq("UpdateSolo (inactive): no call at all",
			 (unsigned int)g_setAudioInSoloCalls, 0);
		aiStorage[0x77] |= 0x2; /* restore active for the rest of this section */

		printf("  -- UpdateHDRBus/UpdateFXControlBus (local byte store + mixer call) --\n");
		memset(aiStorage + 0x64, 0, 0x13);
		g_setHDRBusCalls = g_setFXCtrlBusCalls = 0;
		ctx.index = 1;
		param.value = 3;
		ai->UpdateHDRBus(ctx, param);
		check_eq("UpdateHDRBus: local byte at +0x70+idx", aiStorage[0x70 + 1], 3);
		check_eq("  ...SetHDRBus called once", (unsigned int)g_setHDRBusCalls, 1);
		check_eq("  ...with the real slot index", g_lastMixerBusIndex, 1u);
		param.value = 2;
		ai->UpdateFXControlBus(ctx, param);
		check_eq("UpdateFXControlBus: local byte at +0x6a+idx", aiStorage[0x6a + 1], 2);
		check_eq("  ...SetFXCtrlBus called once", (unsigned int)g_setFXCtrlBusCalls, 1);

		printf("  -- UpdatePan (float, sign-dependent scaling before the mixer push) --\n");
		g_setPanCalls = 0;
		ctx.index = 0;
		*(float *)&param.value = 0.5f;
		ai->UpdatePan(ctx, param);
		check_eq("UpdatePan: local float stored at +0x1c+idx*4",
			 *(int *)&(*(float *)(aiStorage + 0x1c)), *(int *)&param.value);
		check_eq("  ...positive value passed through unmodified to SetPan",
			 *(int *)&g_lastMixerPan, *(int *)&param.value);
		*(float *)&param.value = -0.5f;
		ai->UpdatePan(ctx, param);
		check_eq("  ...negative value negated before SetPan",
			 g_lastMixerPan > 0.0f, true);

		printf("  -- UpdateBusSelect (local byte + mixer call + jump-catch tail) --\n");
		memset(aiStorage + 0x64, 0, 0x13);
		g_setOutputBusCalls = 0;
		g_resetSendKnobsJumpCatchCalls = 0;
		*(int *)(g + 0x684) = 0;              /* mode 0 (default) */
		*(unsigned int *)(g + 0x698) = 0xfffe; /* the literal-address special case */
		ctx.index = 2;
		param.value = 9;
		unsigned char *specialEntry = g + 0x2976e33u;
		((unsigned char *)rtSingleton)[0x2b] = 7;
		specialEntry[0xae5] = 2;   /* matches ctx.index */
		specialEntry[0xad7] = 0;
		ai->UpdateBusSelect(ctx, param);
		check_eq("UpdateBusSelect: local byte at +0x64+idx", aiStorage[0x64 + 2], 9);
		check_eq("  ...SetOutputBus called once", (unsigned int)g_setOutputBusCalls, 1);
		check_eq("  ...jump-catch tail fires when all 3 conditions hold",
			 (unsigned int)g_resetSendKnobsJumpCatchCalls, 1);

		g_resetSendKnobsJumpCatchCalls = 0;
		specialEntry[0xad7] = 1; /* flag byte nonzero -> tail must NOT fire */
		ai->UpdateBusSelect(ctx, param);
		check_eq("  ...tail suppressed when the +0xad7 flag byte is set",
			 (unsigned int)g_resetSendKnobsJumpCatchCalls, 0);

		munmap(aiStorage, 0x80);
		munmap(mgrRegion, 0x1000);
		munmap(mgrBase, 0x100);
		munmap(rtSingleton, 0x10);
	}

	printf("\n[29] Sec 10.90 batch: 13 more CSTGGlobal methods\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		printf("  -- SetCurrentEditInContextTimbreSolo: bit set/clear at +0x29cc4e4 --\n");
		g->SetCurrentEditInContextTimbreSolo(3, true);
		check_eq("bit 3 set", (*(unsigned short *)(buf + 0x29cc4e4)) & (1 << 3), 1 << 3);
		g->SetCurrentEditInContextTimbreSolo(5, true);
		check_eq("bit 3 still set after setting bit 5",
			 (*(unsigned short *)(buf + 0x29cc4e4)) & (1 << 3), 1 << 3);
		g->SetCurrentEditInContextTimbreSolo(3, false);
		check_eq("bit 3 cleared, bit 5 unaffected",
			 *(unsigned short *)(buf + 0x29cc4e4), (unsigned short)(1 << 5));

		printf("  -- CompleteDeferredExtModeChange/ShouldDeferExtModeChange/"
		       "CheckDeferExtModeChange --\n");
		unsigned char *mgr1 = mmap32(0x24000);
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgr1;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 4) = (unsigned int)(unsigned long)mgr1;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 8) = 0;

		buf[0x6ae] = 0;
		check_eq("ShouldDeferExtModeChange: not initialized -> true",
			 (unsigned int)g->ShouldDeferExtModeChange(), 1u);
		check_eq("CheckDeferExtModeChange: not initialized -> false, bit0 set",
			 (unsigned int)g->CheckDeferExtModeChange(), 0u);
		check_eq("  ...bit0 now set", (unsigned int)(buf[0x29cc0c9] & 1), 1u);

		buf[0x29cc0c9] = 0;
		buf[0x6ae] = 1;
		mgr1[0x23d1] = 9; /* != 2 */
		check_eq("ShouldDeferExtModeChange: initialized, bit clear, mgr!=2 -> true",
			 (unsigned int)g->ShouldDeferExtModeChange(), 1u);
		check_eq("CheckDeferExtModeChange: same state -> false, bit0 set",
			 (unsigned int)g->CheckDeferExtModeChange(), 0u);
		check_eq("  ...bit0 now set", (unsigned int)(buf[0x29cc0c9] & 1), 1u);

		buf[0x29cc0c9] = 0;
		mgr1[0x23d1] = 2;
		check_eq("ShouldDeferExtModeChange: initialized, bit clear, mgr==2 -> false",
			 (unsigned int)g->ShouldDeferExtModeChange(), 0u);
		check_eq("CheckDeferExtModeChange: same state -> true, bit0 NOT set",
			 (unsigned int)g->CheckDeferExtModeChange(), 1u);
		check_eq("  ...bit0 still clear", (unsigned int)(buf[0x29cc0c9] & 1), 0u);

		buf[0x29cc0c9] |= 1;
		check_eq("ShouldDeferExtModeChange: bit0 already set -> true",
			 (unsigned int)g->ShouldDeferExtModeChange(), 1u);
		check_eq("CheckDeferExtModeChange: bit0 already set -> false (re-set, no-op)",
			 (unsigned int)g->CheckDeferExtModeChange(), 0u);

		g_onExtModeSetChangeCalls = 0;
		buf[0x29cc0c9] |= 1;
		g->CompleteDeferredExtModeChange();
		check_eq("CompleteDeferredExtModeChange: bit set -> OnExtModeSetChange fires",
			 (unsigned int)g_onExtModeSetChangeCalls, 1u);
		check_eq("  ...bit0 cleared", (unsigned int)(buf[0x29cc0c9] & 1), 0u);
		g->CompleteDeferredExtModeChange();
		check_eq("  ...bit already clear -> no-op, still 1 call total",
			 (unsigned int)g_onExtModeSetChangeCalls, 1u);

		printf("  -- RemoveExtCCFunctionAssignment: un-claim by tag over the 120-slot table --\n");
		memset(buf + 0x29cc11c, 0, 0x78 * 8);
		unsigned char *slotA = buf + 0x29cc11c + 5 * 8;
		unsigned char *slotB = buf + 0x29cc11c + 9 * 8;
		slotA[0] = 1; slotA[2] = 0; *(unsigned int *)(slotA + 4) = 0x77;
		slotB[0] = 1; slotB[2] = 0; *(unsigned int *)(slotB + 4) = 0x88;
		g->RemoveExtCCFunctionAssignment(0x77);
		check_eq("matching-tag slot un-claimed", slotA[0], 0);
		check_eq("non-matching-tag slot untouched", slotB[0], 1);

		printf("  -- SendFXDisableCCToMidiOut: standalone 3-byte MIDI CC send --\n");
		/* Enlarged to 0x300 (from 0x100) and given a fake ringCtl at
		 * +0x208 (sec 10.150): SubmitPerfChangeRequest below now
		 * really dereferences fieldAt(0x208) via the now-real
		 * GetNumWritableBytes(), so this buffer must be big enough to
		 * hold that field and point it somewhere valid -- the old
		 * mock never touched `this` at all, so this was previously
		 * safe to under-allocate. */
		unsigned char *midiPortMgr = mmap32(0x300);
		CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)midiPortMgr;
		SetupFakeRingCtl(midiPortMgr);
		buf[0x6b8] = 3;
		g->SendFXDisableCCToMidiOut(0x5e, false);
		g->SendFXDisableCCToMidiOut(0x5e, true);

		printf("  -- BeginPerformanceChange/BeginSetListSlotChange -> SubmitPerfChangeRequest --\n");
		/* SubmitPerfChangeRequest is now real (sec 10.116); the MIDI
		 * queue always reports 0 writable bytes (g_numWritableBytesOverride's
		 * own default), so it always takes the real "congested -> queue
		 * into the pending slot" branch -- observe via PendingRequest(). */
		buf[0x2975184] = 0; /* not busy */
		buf[0x2975185] = 0;
		g->BeginPerformanceChange(2, 0x11, 0x22, 0x33);
		check_eq("SubmitPerfChangeRequest queued into the pending slot", buf[0x2975185], 1);
		check_eq("tag == 0 (BeginPerformanceChange)", (unsigned int)PendingRequest(buf).tag, 0u);
		check_eq("mode == 2", PendingRequest(buf).mode, 2u);
		check_eq("value1 == p2", PendingRequest(buf).value1, 0x11u);
		check_eq("value2 == p3", PendingRequest(buf).value2, 0x22u);
		check_eq("source == p4", PendingRequest(buf).source, 0x33u);
		check_eq("field14 == 0", PendingRequest(buf).field14, 0u);
		check_eq("field18 == 0", PendingRequest(buf).field18, 0u);

		buf[0x2975185] = 0;
		g->BeginSetListSlotChange(0x44, 0x55, 0x66);
		check_eq("SubmitPerfChangeRequest queued again", buf[0x2975185], 1);
		check_eq("tag == 1 (BeginSetListSlotChange)", (unsigned int)PendingRequest(buf).tag, 1u);
		check_eq("mode == 0 (no mode param)", PendingRequest(buf).mode, 0u);
		check_eq("value1 == p1", PendingRequest(buf).value1, 0x44u);
		check_eq("value2 == p2", PendingRequest(buf).value2, 0x55u);
		check_eq("source == p3", PendingRequest(buf).source, 0x66u);

		printf("  -- GetIsSetListActiveAndSeqPerfType --\n");
		memset(buf + 0x6a4, 0, 3);
		check_eq("+0x6a4 clear -> false", (unsigned int)g->GetIsSetListActiveAndSeqPerfType(), 0u);
		buf[0x6a4] = 1;
		buf[0x6a5] = 2;
		buf[0x6a6] = 1;
		buf[0x2933750 + 2 * 0x834 + (1 << 4)] = 2;
		check_eq("matching table entry -> true", (unsigned int)g->GetIsSetListActiveAndSeqPerfType(), 1u);
		buf[0x2933750 + 2 * 0x834 + (1 << 4)] = 9;
		check_eq("non-matching table entry -> false", (unsigned int)g->GetIsSetListActiveAndSeqPerfType(), 0u);

		printf("  -- SetUseGlobalAudioInputSettings --\n");
		unsigned char *aiStorage2 = mmap32(0x1000);
		memset(aiStorage2, 0, 0x1000);
		/* OnUseGlobalSettingsChanged (sec 10.134) is now real; give it a
		 * valid CSTGControllerRTData::sInstance too (its own tail check
		 * reads fieldAt(0x2b)) -- deliberately left allocated for the
		 * rest of the process's life, matching sec 10.133's own fix for
		 * this exact dangling-pointer hazard class. */
		unsigned char *rtForGlobalSettings = mmap32(0x30);
		memset(rtForGlobalSettings, 0, 0x30);
		rtForGlobalSettings[0x2b] = 0; /* != 7: ResetAllJumpCatch tail doesn't fire here */
		CSTGControllerRTData::sInstance = (CSTGControllerRTData *)rtForGlobalSettings;
		buf[0x6ae] = 0;
		g_useSettingsCalls = 0;
		g->SetUseGlobalAudioInputSettings(true);
		check_eq("+0x680 always set", buf[0x680], 1);
		check_eq("not initialized -> no OnUseGlobalSettingsChanged call",
			 (unsigned int)g_useSettingsCalls, 0u);

		buf[0x6ae] = 1;
		buf[0x67f] = 0x2; /* current bit (bit 1) set, so desiredBit(0) != currentBit(1) */
		mgr1[0x23d1] = 2;
		*(unsigned int *)(mgr1 + 0x23d4) = (unsigned int)((unsigned long)aiStorage2 - 0xae7);
		g->SetUseGlobalAudioInputSettings(false);
		check_eq("+0x680 == 0", buf[0x680], 0);
		check_eq("initialized + mgr==2 -> OnUseGlobalSettingsChanged fires -> UseSettings called",
			 (unsigned int)g_useSettingsCalls, 1u);
		check_eq("  ...on 'this' (desiredBit==0 branch), not the global +0x608 object",
			 (unsigned int)(g_lastUseSettingsThis == aiStorage2), 1u);
		check_eq("  ...global +0x67f bit 1 cleared", buf[0x67f] & 0x2, 0);

		printf("  -- ShouldAllowMidiPerformanceChange --\n");
		unsigned char *midiDisp2 = mmap32(0x200);
		CSTGMidiDispatcher::sInstance = (CSTGMidiDispatcher *)midiDisp2;
		midiDisp2[0xa2] = 0;
		check_eq("dispatcher+0xa2 == 0 -> false",
			 (unsigned int)g->ShouldAllowMidiPerformanceChange(), 0u);

		midiDisp2[0xa2] = 1;
		buf[0x2975185] = 0; /* selects the +0x297514c slot */
		unsigned char *slot = buf + 0x297514c;
		slot[0] = 1; /* slot[0] != 0 -> true regardless of the rest */
		check_eq("slot[0] != 0 -> true",
			 (unsigned int)g->ShouldAllowMidiPerformanceChange(), 1u);

		slot[0] = 0;
		*(unsigned int *)(slot + 4) = 0;
		*(unsigned int *)(slot + 0xc) = 0xfffe;
		check_eq("slot[0]==0, +4==0, +0xc==0xfffe -> false",
			 (unsigned int)g->ShouldAllowMidiPerformanceChange(), 0u);
		*(unsigned int *)(slot + 0xc) = 0x1234;
		check_eq("+0xc != 0xfffe -> true",
			 (unsigned int)g->ShouldAllowMidiPerformanceChange(), 1u);

		printf("  -- SendUnsolGlobalMessageToUI --\n");
		g_pushUnsolicitedMessageCalls = 0;
		g->SendUnsolGlobalMessageToUI(0x11223344, 0x55667788, (int)0x99aabbcc, 0x9);
		check_eq("PushUnsolicitedMessage called once", (unsigned int)g_pushUnsolicitedMessageCalls, 1u);
		check_eq("msg+0x0 == 0x18", *(unsigned short *)(g_lastUnsolicitedMessage + 0x0), 0x18);
		check_eq("msg+0x2 == low16(source)", *(unsigned short *)(g_lastUnsolicitedMessage + 0x2), 0x9);
		check_eq("msg+0x4 == 1", *(unsigned int *)(g_lastUnsolicitedMessage + 0x4), 1u);
		check_eq("msg+0x8 == 0", *(unsigned int *)(g_lastUnsolicitedMessage + 0x8), 0u);
		check_eq("msg+0xc == p2", *(unsigned int *)(g_lastUnsolicitedMessage + 0xc), 0x55667788u);
		check_eq("msg+0x10 == p1", *(unsigned int *)(g_lastUnsolicitedMessage + 0x10), 0x11223344u);
		check_eq("msg+0x14 == p3", *(unsigned int *)(g_lastUnsolicitedMessage + 0x14), 0x99aabbccu);

		printf("  -- SetNKS4TestModeFlag --\n");
		g_setTestModeCalls = 0;
		g_notifyNKS4TestModeCalls = 0;
		buf[0x6ac] = 0;
		g->SetNKS4TestModeFlag(false);
		check_eq("same value -> no-op, no SetTestMode call",
			 (unsigned int)g_setTestModeCalls, 0u);

		g->SetNKS4TestModeFlag(true);
		check_eq("SetTestMode called once", (unsigned int)g_setTestModeCalls, 1u);
		check_eq("  ...with true", (unsigned int)g_lastTestModeValue, 1u);
		check_eq("+0x6ac == 1", buf[0x6ac], 1);
		check_eq("NotifyNKS4TestMode fires when turning ON",
			 (unsigned int)g_notifyNKS4TestModeCalls, 1u);

		g->SetNKS4TestModeFlag(false);
		check_eq("SetTestMode called again", (unsigned int)g_setTestModeCalls, 2u);
		check_eq("+0x6ac == 0", buf[0x6ac], 0);
		check_eq("NotifyNKS4TestMode NOT called when turning OFF",
			 (unsigned int)g_notifyNKS4TestModeCalls, 1u);

		munmap(mgr1, 0x24000);
		munmap(midiPortMgr, 0x300); /* matches the mmap32(0x300) above --
					     * was a stale 0x100 left over from
					     * before sec 10.150 enlarged this
					     * allocation (the actual segfault
					     * fix for this dangling-pointer
					     * hazard is section [30]'s own new
					     * midiPortMgr30 setup, not this size
					     * correction -- see there). */
		munmap(aiStorage2, 0x1000);
		munmap(midiDisp2, 0x200);
	}

	printf("\n[30] Sec 10.92 batch: 10 more CSTGGlobal methods\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		/* This section was written back when CSTGMidiQueue::
		 * GetNumWritableBytes() was still a mock that never touched
		 * `this` (sec 10.92), so RepeatLastPerformanceChange/
		 * BeginPerformanceChange/BeginSetListSlotChange/SetMode below
		 * (all of which funnel into the now-real SubmitPerfChangeRequest,
		 * sec 10.116/10.150) never needed a valid
		 * CSTGMidiPortManager::sInstance of their own -- they silently
		 * relied on whatever a PRIOR, unrelated section happened to
		 * leave behind in that same global static. Give this section
		 * its own fresh ringCtl setup, exactly like every other section
		 * in this file that reaches the same real dependency chain
		 * (see [22]/[24]/[36]/[41]/[42]/[49]). */
		unsigned char *midiPortMgr30 = mmap32(0x300);
		CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)midiPortMgr30;
		SetupFakeRingCtl(midiPortMgr30);

		printf("  -- ValidateParamChange --\n");
		g_validateParamChangeCalls = 0;
		CSTGMessageContext ctx;
		memset(&ctx, 0, sizeof(ctx));
		CValue value;
		ctx.index = 0xc0; /* > 0xbf */
		g->ValidateParamChange(ctx, 0x26, value);
		check_eq("paramId 0x26, index out of range -> responseCode=2, no forward",
			 ctx.responseCode, 2u);
		check_eq("  ...ValidateParamChange NOT forwarded", (unsigned int)g_validateParamChangeCalls, 0u);

		ctx.index = 0xa0; /* <= 0xbf */
		ctx.clampFlag = 0xff;
		g->ValidateParamChange(ctx, 0x26, value);
		check_eq("paramId 0x26, index in range -> clampFlag cleared", ctx.clampFlag, 0);
		check_eq("  ...ValidateParamChange forwarded once", (unsigned int)g_validateParamChangeCalls, 1u);

		ctx.index = 5;
		g->ValidateParamChange(ctx, 0x99, value); /* any other paramId: unconditional forward */
		check_eq("other paramId -> always forwarded", (unsigned int)g_validateParamChangeCalls, 2u);

		printf("  -- RepeatLastPerformanceChange --\n");
		buf[0x2975184] = 0;
		buf[0x2975185] = 0; /* selects +0x297514c */
		CSTGPerfChangeRequest *savedSlot = (CSTGPerfChangeRequest *)(buf + 0x297514c);
		memset(savedSlot, 0, sizeof(*savedSlot));
		savedSlot->tag = 1;
		savedSlot->mode = 7;
		savedSlot->value1 = 0x11;
		savedSlot->value2 = 0x22;
		savedSlot->source = 0x33;
		savedSlot->field14 = 0x44; /* must be overwritten to 3 */
		g->RepeatLastPerformanceChange();
		check_eq("SubmitPerfChangeRequest queued into the pending slot", buf[0x2975185], 1);
		check_eq("copied verbatim: tag", (unsigned int)PendingRequest(buf).tag, 1u);
		check_eq("copied verbatim: mode", PendingRequest(buf).mode, 7u);
		check_eq("copied verbatim: value1", PendingRequest(buf).value1, 0x11u);
		check_eq("copied verbatim: value2", PendingRequest(buf).value2, 0x22u);
		check_eq("copied verbatim: source", PendingRequest(buf).source, 0x33u);
		check_eq("field14 forced to 3", PendingRequest(buf).field14, 3u);

		buf[0x2975184] = 1; /* "already repeating" -> no-op */
		buf[0x2975185] = 0;
		g->RepeatLastPerformanceChange();
		check_eq("no-op when already repeating: pending flag stays clear", buf[0x2975185], 0);

		printf("  -- ResetAllControllers --\n");
		/* HandleController is now real (sec 10.118). ResetAllControllers
		 * always sets value.fieldB's own bit1 (via `|3`), so `cl` is
		 * true for all 3 calls regardless of the uninitialized stack
		 * bits -- each of the 3 CC handlers (0x5c/0x5e/0x5f) therefore
		 * takes the "copy the secondary bit into the primary bit" path
		 * (ignoring value.field8 entirely), not the field8==0 path. */
		buf[0x6d4] = 0;
		g_pushUnsolicitedMessageCalls = 0;
		g->ResetAllControllers();
		check_eq("PushUnsolicitedMessage called 3 times (once per CC)",
			 (unsigned int)g_pushUnsolicitedMessageCalls, 3u);
		check_eq("+0x6d4 stays 0 (all secondary bits were already 0)", buf[0x6d4], 0);
		check_eq("last call (cc==0x5f/TFX): msg type == 6",
			 *(unsigned int *)(g_lastUnsolicitedMessage + 0x10), 6u);
		check_eq("  ...msg+0x2 == value.fieldA (1)",
			 *(short *)(g_lastUnsolicitedMessage + 0x2), 1);
		check_eq("  ...msg+0x14 == 0 (copied secondary bit, not field8==0)",
			 *(unsigned int *)(g_lastUnsolicitedMessage + 0x14), 0u);

		printf("  -- DoesPerfChangeRequestMatchType --\n");
		CSTGPerfChangeRequest req;
		memset(&req, 0, sizeof(req));
		req.tag = 0;
		req.value1 = 3;
		req.value2 = 2;
		buf[0x2933750 + 3 * 0x834 + (2 << 4)] = 7;
		check_eq("tag==0: matches real table entry", (unsigned int)g->DoesPerfChangeRequestMatchType(req, 7), 1u);
		check_eq("tag==0: non-match", (unsigned int)g->DoesPerfChangeRequestMatchType(req, 8), 0u);

		req.tag = 1;
		req.mode = 2; /* m = 1 <= 1 -> table lookup at +0x64+1*4 */
		*(unsigned int *)(buf + 0x64 + 4) = 42;
		check_eq("tag==1, mode 2: dword table match", (unsigned int)g->DoesPerfChangeRequestMatchType(req, 42), 1u);
		req.mode = 5; /* m = 4 > 1 -> compare against literal 1 */
		check_eq("tag==1, mode 5: literal-1 match", (unsigned int)g->DoesPerfChangeRequestMatchType(req, 1), 1u);
		check_eq("tag==1, mode 5: non-match", (unsigned int)g->DoesPerfChangeRequestMatchType(req, 2), 0u);

		printf("  -- GetPerformanceIdFromPerfChangeRequest --\n");
		CPerformanceId pid;
		memset(&pid, 0xff, sizeof(pid));
		req.tag = 0;
		req.value1 = 3;
		req.value2 = 2;
		unsigned char *record = buf + 0x2933740 + 3 * 0x834 + (2 << 4);
		*(unsigned short *)(record + 0x10) = 0x1234;
		record[0x12] = 0x56;
		check_eq("tag==0: returns true", (unsigned int)g->GetPerformanceIdFromPerfChangeRequest(req, &pid), 1u);
		check_eq("  ...byte0 == low(0x1234)", pid.byte0, 0x34);
		check_eq("  ...byte1 == high(0x1234)", pid.byte1, 0x12);
		check_eq("  ...byte2 == 0x56", pid.byte2, 0x56);

		req.tag = 1;
		req.value2 = 0xfffe;
		check_eq("tag==1, value2==0xfffe -> false", (unsigned int)g->GetPerformanceIdFromPerfChangeRequest(req, &pid), 0u);

		req.value2 = 0x77;
		req.value1 = 0x88;
		req.mode = 5; /* m=4>1 -> byte0 literal 1 */
		check_eq("tag==1, mode 5 -> true", (unsigned int)g->GetPerformanceIdFromPerfChangeRequest(req, &pid), 1u);
		check_eq("  ...byte0 == 1 (literal)", pid.byte0, 1);
		check_eq("  ...byte1 == value1", pid.byte1, 0x88);
		check_eq("  ...byte2 == value2", pid.byte2, 0x77);

		printf("  -- NotifyKarmaPerformanceChange --\n");
		g_writeQueueCalls = 0;
		buf[0x6b8] = 3; /* channel */
		*(int *)(buf + 0x684) = 0; /* default mode */
		*(unsigned int *)(buf + 0x68c) = 0xff; /* &0x3f|0x40 == 0x7f */
		buf[0x698] = 0x21;
		g->NotifyKarmaPerformanceChange();
		check_eq("Write called once", (unsigned int)g_writeQueueCalls, 1u);
		check_eq("length == 5", g_lastQueueMsgLen, 5u);
		check_eq("msg[0] == channel|0xc0", g_lastQueueMsg[0], (unsigned char)(3 | 0xc0));
		check_eq("msg[1] == (0x68c&0x3f)|0x40", g_lastQueueMsg[1], 0x7f);
		check_eq("msg[2] == byteAt(0x698)", g_lastQueueMsg[2], 0x21);
		check_eq("msg[3] == 0x15", g_lastQueueMsg[3], 0x15);
		check_eq("msg[4] == 0xfe", g_lastQueueMsg[4], 0xfe);

		printf("  -- SendProgramChangeToMidiOut --\n");
		g_writeMidiOutQueueCalls = 0;
		buf[0x6d6] = 0; /* BankChangeEnable clear -> only Program Change sent */
		g->SendProgramChangeToMidiOut(0x10, 0x20, 0x30);
		check_eq("BankChangeEnable clear -> only 1 WriteSTGMidiOutQueue call",
			 (unsigned int)g_writeMidiOutQueueCalls, 1u);
		check_eq("Program Change msg[0] == channel|0xc0", g_lastMidiMsg[0], (unsigned char)(3 | 0xc0));
		check_eq("Program Change msg[1] == p3", g_lastMidiMsg[1], 0x30);
		check_eq("Program Change length == 2", g_lastMidiMsgLen, 2u);

		g_writeMidiOutQueueCalls = 0;
		buf[0x6d6] = 1; /* BankChangeEnable set -> both Bank Select + Program Change */
		g->SendProgramChangeToMidiOut(0x10, 0x20, 0x30);
		check_eq("BankChangeEnable set -> 2 WriteSTGMidiOutQueue calls",
			 (unsigned int)g_writeMidiOutQueueCalls, 2u);

		printf("  -- EmergencyFreeDyingSlotVoiceData --\n");
		/* This section was added in sec 10.149 when EmergencyFreeVoiceList
		 * went from a flat call-counting mock to a real body -- but it
		 * never gave itself a valid CSTGVoiceAllocator::sInstance of its
		 * own (the real body needs one: CSTGSlotVoiceData::
		 * EmergencyFreeAllVoices() dispatches through
		 * `CSTGVoiceAllocator::sInstance->EmergencyFreeVoiceList()`,
		 * which reads the real `requirementsMutex` field at +0x44ea8).
		 * Without this, the test silently inherited whatever OTHER,
		 * unrelated section happened to set CSTGVoiceAllocator::sInstance
		 * to last (here: [29]'s own `voiceAlloc = mmap32(0x10)`, already
		 * munmap'd by this point) -- a dangling/undersized pointer,
		 * confirmed segfaulting at managers.cpp:666's `requirementsMutex`
		 * read. Fixed the same way as the analogous
		 * CSTGMidiPortManager::sInstance gap in section [30] above: give
		 * this section its own valid instance. Using the REAL ctor here
		 * (not a raw mmap32() buffer) rather than hand-picking a size,
		 * since managers.cpp's CSTGVoiceAllocator::CSTGVoiceAllocator()
		 * is already real, already linked, and already sets both
		 * `requirementsMutex` and `sInstance` correctly (same pattern
		 * test_managers.cpp's own [19] uses) -- every mock it needs
		 * (get_sizeof_rtwrap_pthread_mutex/rtwrap_malloc/
		 * rtwrap_pthread_mutex_init/mutexattr_*) is already defined near
		 * the top of this file for managers.cpp's own link requirements. */
		CSTGVoiceAllocator *voiceAllocEFD = new CSTGVoiceAllocator();
		(void)voiceAllocEFD;
		unsigned char *node0 = mmap32(0x10);
		unsigned char *voice0 = mmap32(0x60);
		unsigned char *sub0 = mmap32(0x50);
		*(unsigned int *)(node0 + 4) = 0;                  /* next = NULL */
		*(unsigned int *)(node0 + 8) = (unsigned int)(unsigned long)voice0;        /* payload */
		*(unsigned int *)(voice0 + 0x34) = (unsigned int)(unsigned long)sub0;
		sub0[0x43] = 0x00;                                     /* groupBit 0 */
		voice0[0x40] = 1;                                        /* "dying" */
		*(unsigned short *)(voice0 + 0x4c) = 0;
		*(unsigned short *)(voice0 + 0x58) = 0;                    /* sum==0 -> free */
		voice0[0x41] = 0;
		/* node0 wires TWO separate roles here, both confirmed real: (1)
		 * the OUTER dying-slot list rooted at buf+0x29c9904, which
		 * EmergencyFreeDyingSlotVoiceData() itself walks to LOCATE voice0
		 * as the dying CSTGSlotVoiceData; and (2) voice0's OWN embedded
		 * group-0 voice list (self+0x44), which EmergencyFreeAllVoices()
		 * walks via the now-real EmergencyFreeVoiceList() to actually
		 * FREE something. This second wiring was missing from the
		 * original sec 10.149 KAT -- voice0+0x44/+0x50 were left at
		 * mmap32()'s own zero-fill, so EmergencyFreeVoiceList's list was
		 * always empty and FreeVoice() was silently never exercised (the
		 * segfault fixed above always killed the process before
		 * execution ever reached this check, so the gap went unnoticed
		 * until now). Point voice0's own group-0 list at node0 too, so
		 * the walk actually finds one node (payload==voice0) and calls
		 * FreeVoice(voice0) once, matching this test's own stated intent
		 * below. */
		*(unsigned int *)(voice0 + 0x44) = (unsigned int)(unsigned long)node0;
		*(unsigned int *)(buf + 0x29c9904) = (unsigned int)(unsigned long)node0;
		g_doPendingMoveVoicesCalls = 0;
		g_freeVoiceCalls = 0;
		g_lastFreeVoiceArg = 0;
		g_freeSlotVoiceDataCalls = 0;
		g->EmergencyFreeDyingSlotVoiceData();
		check_eq("sum==0 -> EmergencyFreeAllVoices called (2 real EmergencyFreeVoiceList invocations)",
			 (unsigned int)g_doPendingMoveVoicesCalls, 2u);
		check_eq("  ...first list's one node -> FreeVoice(voice0) called once",
			 (unsigned int)g_freeVoiceCalls, 1u);
		check_eq("  ...with the confirmed real payload pointer",
			 (unsigned int)(unsigned long)g_lastFreeVoiceArg, (unsigned int)(unsigned long)voice0);
		check_eq("sum==0 -> FreeSlotVoiceData ALSO called", (unsigned int)g_freeSlotVoiceDataCalls, 1u);
		check_eq("  ...with flag true", (unsigned int)g_lastFreeSlotVoiceDataFlag, 1u);

		*(unsigned short *)(voice0 + 0x4c) = 5;
		voice0[0x41] = 1; /* sum!=0 AND byteAt(0x41)!=0 -> EmergencyFreeAllVoices only */
		g_doPendingMoveVoicesCalls = 0;
		g_freeVoiceCalls = 0;
		g_freeSlotVoiceDataCalls = 0;
		g->EmergencyFreeDyingSlotVoiceData();
		check_eq("sum!=0 + flag set -> EmergencyFreeAllVoices called (2 real EmergencyFreeVoiceList invocations)",
			 (unsigned int)g_doPendingMoveVoicesCalls, 2u);
		check_eq("  ...FreeSlotVoiceData NOT called", (unsigned int)g_freeSlotVoiceDataCalls, 0u);

		munmap(node0, 0x10);
		munmap(voice0, 0x60);
		munmap(sub0, 0x50);

		printf("  -- SetTune --\n");
		CSTGGlobal::sInstance = g;
		g_updateGlobalTuneCalls = 0;
		buf[0x6ae] = 0; /* not initialized -> field written, no list walk */
		g->SetTune(3.5f);
		{
			float want = 3.5f;
			check_eq("+0x6bc == raw float bits", *(unsigned int *)(buf + 0x6bc), *(unsigned int *)&want);
		}
		check_eq("not initialized -> no UpdateGlobalTune calls", (unsigned int)g_updateGlobalTuneCalls, 0u);

		unsigned char *tuneNode = mmap32(0x10);
		unsigned char *tuneVoice = mmap32(0x10);
		*(unsigned int *)(tuneNode + 0) = 0;
		*(unsigned int *)(tuneNode + 8) = (unsigned int)(unsigned long)tuneVoice;
		*(unsigned int *)(buf + 0x29c99cc) = (unsigned int)(unsigned long)tuneNode; /* list 0 */
		buf[0x6ae] = 1;
		buf[0x6b8] = 0; /* active slot 0 */
		g->SetTune(2.25f);
		check_eq("initialized -> UpdateGlobalTune called at least once",
			 (unsigned int)(g_updateGlobalTuneCalls > 0), 1u);
		{
			float want = 2.25f;
			check_eq("  ...with the real tune value", *(unsigned int *)&g_lastUpdateGlobalTuneValue,
				 *(unsigned int *)&want);
		}
		munmap(tuneNode, 0x10);
		munmap(tuneVoice, 0x10);

		printf("  -- SetMode --\n");
		buf[0x2975184] = 0; /* not busy; MIDI queue still reports congested by
				      * default, so SubmitPerfChangeRequest always queues
				      * into the pending slot -- same observable effect
				      * either way. */
		*(unsigned int *)(buf + 0x688) = 0x111;
		*(unsigned int *)(buf + 0x694) = 0x222;
		g->SetMode(0, 9);
		check_eq("mode 0 (default): value1 == +0x688", PendingRequest(buf).value1, 0x111u);
		check_eq("mode 0 (default): value2 == +0x694", PendingRequest(buf).value2, 0x222u);
		check_eq("tag == 0", (unsigned int)PendingRequest(buf).tag, 0u);
		check_eq("source == 9", PendingRequest(buf).source, 9u);

		*(unsigned int *)(buf + 0x690) = 0x333;
		*(unsigned int *)(buf + 0x69c) = 0x444;
		g->SetMode(1, 9);
		check_eq("mode 1: value1 == +0x690", PendingRequest(buf).value1, 0x333u);
		check_eq("mode 1: value2 == +0x69c", PendingRequest(buf).value2, 0x444u);

		*(unsigned int *)(buf + 0x6a0) = 0x555;
		g->SetMode(2, 9);
		check_eq("mode 2: value1 == 0 (literal)", PendingRequest(buf).value1, 0u);
		check_eq("mode 2: value2 == +0x6a0", PendingRequest(buf).value2, 0x555u);

		munmap(midiPortMgr30, 0x300);
	}

	printf("\n[31] RunVoiceModelStaticFront/RunVoiceModelStaticBack (sec 10.93)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);
		CSTGGlobal::sInstance = g;

		/* Same vtable/gate-buffer scaffolding as [14]'s RunVoiceModelFeedback
		 * test (same real host/target pointer-overlap constraint applies). */
		auto slot1a = [](void *self) -> void * { return self; };
		void *vtable[0x1b];
		for (int i = 0; i < 0x1b; i++)
			vtable[i] = 0;
		vtable[0x1a] = (void *)+slot1a;

		unsigned char gateFront[0xe3], gateBack[0xe3];
		memset(gateFront, 0, sizeof(gateFront));
		memset(gateBack, 0, sizeof(gateBack));
		*(void ***)gateFront = vtable;
		*(void ***)gateBack = vtable;
		gateFront[0xe1] = 0x40;        /* Front: bit 6 set */
		gateBack[0xe1] = 0x80;         /* Back: sign bit set */

		unsigned char payload1[0x28e0], payload2[0x28e0];
		memset(payload1, 0, sizeof(payload1));
		memset(payload2, 0, sizeof(payload2));
		unsigned char sub1[0xb74 + 8], sub2[0xb74 + 8];
		memset(sub1, 0, sizeof(sub1));
		memset(sub2, 0, sizeof(sub2));
		*(unsigned char **)(payload1 + 0x38) = sub1;
		*(unsigned char **)(payload2 + 0x38) = sub2;

		payload1[0x28dc] = 5;  /* matches param==5, Front's first id slot */
		payload1[0x28de] = 9;  /* matches param==9, Back's first id slot */
		*(unsigned char **)(sub1 + 0xb6b) = gateFront; /* also serves as gateBack via +0xe1 bit 7 check below */
		sub1[0xb73] = 1;

		payload2[0x28dc] = 0xff; /* does not match param==5 */
		payload2[0x28de] = 0xff; /* does not match param==9 */

		unsigned char node1[0x20], node2[0x20];
		*(unsigned char **)(node1 + 0x8) = payload1;
		*(unsigned char **)(node2 + 0x8) = payload2;
		*(unsigned char **)node1 = node2;
		*(unsigned char **)node2 = 0;
		*(unsigned char **)(buf + 0x29c9900) = node1;

		g_runVoiceModelStaticFrontCalls = 0;
		g->RunVoiceModelStaticFront(5);
		check_eq("Front: id-matching + bit6-gated node triggers static front",
			 (unsigned int)g_runVoiceModelStaticFrontCalls, 1u);
		check_eq("  ...called with the real param", g_lastRunVoiceModelStaticParam, 5u);
		check_eq("  ...on the matching payload", g_lastRunVoiceModelStaticThis == payload1, true);

		g_runVoiceModelStaticFrontCalls = 0;
		g->RunVoiceModelStaticFront(0xff);
		check_eq("Front: non-matching id (payload1) but payload2 also mismatches -> no call",
			 (unsigned int)g_runVoiceModelStaticFrontCalls, 0u);

		/* Reuse the same gate buffer for Back: +0xe1's bit 7 (sign bit)
		 * is ALSO set (0x40|0x80 would be needed to test both
		 * independently, but this project's `gateFront[0xe1]=0x40`
		 * above deliberately only sets bit 6 -- swap in gateBack, whose
		 * +0xe1 has bit 7 set instead, for the Back-specific check). */
		*(unsigned char **)(sub1 + 0xb6b) = gateBack;
		g_runVoiceModelStaticBackCalls = 0;
		g->RunVoiceModelStaticBack(9);
		check_eq("Back: id-matching + sign-bit-gated node triggers static back",
			 (unsigned int)g_runVoiceModelStaticBackCalls, 1u);
		check_eq("  ...called with the real param", g_lastRunVoiceModelStaticParam, 9u);

		*(unsigned char **)(buf + 0x29c9900) = 0; /* reset */
	}

	printf("\n[32] Sec 10.94 batch: HandleMidiPerformanceChange/StealDyingSlotVoiceDatasForCost/FreeSlotVoiceData\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		printf("  -- HandleMidiPerformanceChange --\n");
		/* Same CSTGMidiPortManager::sInstance gap as section [30]/
		 * EmergencyFreeDyingSlotVoiceData above: HandleMidiPerformanceChange
		 * reaches the now-real SubmitPerfChangeRequest/GetNumWritableBytes
		 * (sec 10.116/10.150), so this section needs its own valid ring
		 * setup rather than inheriting whatever a prior section left in
		 * that global static. */
		unsigned char *midiPortMgr32 = mmap32(0x300);
		CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)midiPortMgr32;
		SetupFakeRingCtl(midiPortMgr32);
		unsigned char *midiDisp3 = mmap32(0x200);
		CSTGMidiDispatcher::sInstance = (CSTGMidiDispatcher *)midiDisp3;
		midiDisp3[0xa2] = 0;
		buf[0x2975185] = 0;
		g->HandleMidiPerformanceChange(7);
		check_eq("dispatcher+0xa2==0 -> no-op (never reaches SubmitPerfChangeRequest)", buf[0x2975185], 0);

		midiDisp3[0xa2] = 1;
		buf[0x2975185] = 0; /* selects +0x297514c */
		unsigned char *hSlot = buf + 0x297514c;
		memset(hSlot, 0, 0x20);
		hSlot[0] = 1; /* slot[0]!=0 -> tag=1 path */
		*(unsigned int *)(hSlot + 8) = 0x77;
		g->HandleMidiPerformanceChange(7);
		/* source==2 here activates SubmitPerfChangeRequest's own real
		 * dedup/debounce check (sec 10.116) against compareSlot -- but
		 * compareSlot IS hSlot itself (pending flag was clear), and
		 * hSlot's own value2 (zeroed by the memset above) never
		 * matches this request's value2 (the passed-in param, 7), so
		 * the fields mismatch and the debounce never fires here. */
		check_eq("slot[0]!=0 -> submitted (queued into the pending slot)", buf[0x2975185], 1);
		check_eq("  ...tag == 1", (unsigned int)PendingRequest(buf).tag, 1u);
		check_eq("  ...mode == 0", PendingRequest(buf).mode, 0u);
		check_eq("  ...value1 == slot+8", PendingRequest(buf).value1, 0x77u);
		check_eq("  ...value2 == param", PendingRequest(buf).value2, 7u);
		check_eq("  ...source == 2", PendingRequest(buf).source, 2u);

		hSlot[0] = 0;
		*(unsigned int *)(hSlot + 4) = 0;
		*(unsigned int *)(hSlot + 0xc) = 0xfffe;
		g->HandleMidiPerformanceChange(7);
		check_eq("slot[0]==0, +4==0, +0xc==0xfffe -> no-op (own gate, pending flag unchanged)",
			 buf[0x2975185], 1);

		buf[0x2975185] = 0;
		*(unsigned int *)(hSlot + 4) = 1; /* mode == 1, valid */
		*(unsigned int *)(hSlot + 8) = 0x88;
		g->HandleMidiPerformanceChange(9);
		/* compareSlot is now +0x2975168 (pending flag was set by the
		 * first submission above) -- its own tag(1) mismatches this
		 * request's tag(0), so the debounce doesn't fire here either. */
		check_eq("slot[0]==0, mode<=1 -> submitted", buf[0x2975185], 1);
		check_eq("  ...tag == 0", (unsigned int)PendingRequest(buf).tag, 0u);
		check_eq("  ...mode == 1", PendingRequest(buf).mode, 1u);
		check_eq("  ...value1 == slot+8", PendingRequest(buf).value1, 0x88u);

		buf[0x2975185] = 0;
		*(unsigned int *)(hSlot + 4) = 5; /* mode > 1 -> no-op */
		g->HandleMidiPerformanceChange(9);
		check_eq("mode > 1 -> no-op (own gate, pending flag stays clear)", buf[0x2975185], 0);
		munmap(midiDisp3, 0x200);

		printf("  -- StealDyingSlotVoiceDatasForCost --\n");
		unsigned char *snode = mmap32(0x10);
		unsigned char *svoice = mmap32(0x2900);
		unsigned char *ssub = mmap32(0x50);
		memset(svoice, 0, 0x2900);
		*(unsigned int *)(snode + 4) = 0;
		*(unsigned int *)(snode + 8) = (unsigned int)(unsigned long)svoice;
		*(unsigned int *)(svoice + 0x34) = (unsigned int)(unsigned long)ssub;
		ssub[0x43] = 0x00;      /* groupBit 0 */
		svoice[0x40] = 1;         /* dying */
		*(unsigned int *)(svoice + 0x28c4) = 0; /* not protected */
		ssub[0xd] = 1;              /* qualifying sub-mode */
		svoice[0x42] = 0;
		*(unsigned int *)(buf + 0x29c9904) = (unsigned int)(unsigned long)snode;

		g_getTotalStaticCostsCalls = 0;
		g_stealVoiceListCalls = 0;
		g_getTotalStaticCostsOut1 = 30;
		g_getTotalStaticCostsOut2 = 20;
		unsigned long stolen = g->StealDyingSlotVoiceDatasForCost(40);
		check_eq("GetTotalStaticCosts called once", (unsigned int)g_getTotalStaticCostsCalls, 1u);
		check_eq("Steal called once (2 StealVoiceList calls)", (unsigned int)g_stealVoiceListCalls, 2u);
		check_eq("  ...Steal's own flag bytes set", (unsigned int)(svoice[0x41] == 1 && svoice[0x42] == 1), 1u);
		check_eq("returned accumulated cost == 50", (unsigned int)stolen, 50u);

		check_eq("targetCost==0 -> immediate 0, no calls", (unsigned int)g->StealDyingSlotVoiceDatasForCost(0), 0u);

		munmap(snode, 0x10);
		munmap(svoice, 0x2900);
		munmap(ssub, 0x50);

		printf("  -- FreeSlotVoiceData --\n");
		unsigned char *fnode = mmap32(0x40);
		memset(fnode, 0, 0x40);
		memset(buf + 0x29c9900, 0, 0x20); /* clear both list heads + free-list head/tail/counters */
		*(unsigned int *)(buf + 0x29c9900) = (unsigned int)(unsigned long)(fnode + 0x24); /* node is active-list head */
		*(unsigned int *)(buf + 0x29c9904) = 0; /* not dying-list head */
		*(unsigned int *)(fnode + 0x24) = 0; /* no next */
		*(unsigned int *)(fnode + 0x28) = 0; /* no prev */
		*(unsigned int *)(buf + 0x29c9908) = 5; /* active count, will be decremented */
		*(unsigned int *)(buf + 0x29c98fc) = 3; /* free count, will be incremented */
		*(unsigned int *)(buf + 0x29c98f4) = 0; /* free-list head, empty */

		g->FreeSlotVoiceData((CSTGSlotVoiceData *)fnode);
		check_eq("active-list head updated to node's own next (0)",
			 *(unsigned int *)(buf + 0x29c9900), 0u);
		check_eq("node's own +0x24/+0x28/+0x30 cleared", *(unsigned int *)(fnode + 0x24), 0u);
		check_eq("  ...+0x28", *(unsigned int *)(fnode + 0x28), 0u);
		check_eq("  ...+0x30", *(unsigned int *)(fnode + 0x30), 0u);
		check_eq("free-list head now points at node+4",
			 *(unsigned int *)(buf + 0x29c98f4), (unsigned int)(unsigned long)(fnode + 4));
		check_eq("free-list tail ALSO set (was empty)",
			 *(unsigned int *)(buf + 0x29c98f8), (unsigned int)(unsigned long)(fnode + 4));
		check_eq("node's own +0x10 == free-list head field's address",
			 *(unsigned int *)(fnode + 0x10), (unsigned int)(unsigned long)(buf + 0x29c98f4));
		check_eq("active count decremented", *(unsigned int *)(buf + 0x29c9908), 4u);
		check_eq("free count incremented", *(unsigned int *)(buf + 0x29c98fc), 4u);
		munmap(fnode, 0x40);
		munmap(midiPortMgr32, 0x300);
	}

	printf("\n[33] PreprocessPerformanceChange (sec 10.95)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		unsigned char *msgProc = mmap32(0x1040);
		memset(msgProc, 0, 0x1040);
		CSTGMessageProcessor::sInstance = (CSTGMessageProcessor *)msgProc;
		unsigned char *smoother = mmap32(0x10);
		CSTGSmoother::sInstance = (CSTGSmoother *)smoother;
		unsigned char *midiDisp4 = mmap32(0x10);
		CSTGMidiDispatcher::sInstance = (CSTGMidiDispatcher *)midiDisp4;
		unsigned char *midiPortMgr2 = mmap32(0x300);
		CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)midiPortMgr2;
		SetupFakeRingCtl(midiPortMgr2);
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = 0;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 4) = 0;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 8) = 0;

		msgProc[0x48] = 1;
		g_finalizeAllSmoothersCalls = 0;
		g->PreprocessPerformanceChange();
		check_eq("gate set (+0x48) -> no-op", (unsigned int)g_finalizeAllSmoothersCalls, 0u);

		msgProc[0x48] = 0;
		sXCmd = 0; /* sXCmd==0 -> headroom reset runs */
		allPlusOne[0] = 1.0f;
		allMinusOne[0] = -1.0f;
		kAudXBZD = 0;
		g_finalizeAllSmoothersCalls = 0;
		g_setIsDyingCalls = 0;
		g_writeQueueCalls = 0;
		buf[0x6b8] = 5;
		g->PreprocessPerformanceChange();
		{
			float want = 0.7f;
			check_eq("allPlusOne[0] reset to 0.7f", *(unsigned int *)&allPlusOne[0], *(unsigned int *)&want);
		}
		{
			float want = -0.2f;
			check_eq("allMinusOne[0] reset to -0.2f", *(unsigned int *)&allMinusOne[0], *(unsigned int *)&want);
		}
		check_eq("kAudXBZD == 0x1f", kAudXBZD, 0x1fu);
		check_eq("+0x2975184 (repeating flag) set", buf[0x2975184], 1);
		check_eq("FinalizeAllSmoothers called once", (unsigned int)g_finalizeAllSmoothersCalls, 1u);
		check_eq("SetIsDying called once", (unsigned int)g_setIsDyingCalls, 1u);
		check_eq("CSTGMidiDispatcher::sInstance+0x0 set", midiDisp4[0], 1);
		check_eq("CSTGMessageProcessor::sInstance+0x54 set", msgProc[0x54], 1);
		check_eq("Write called once", (unsigned int)g_writeQueueCalls, 1u);
		check_eq("msg[0] == channel|0xb0", g_lastQueueMsg[0], (unsigned char)(5 | 0xb0));
		check_eq("msg[1] == 0x79", g_lastQueueMsg[1], 0x79);
		check_eq("msg[2] == 0x03", g_lastQueueMsg[2], 0x03);
		check_eq("msg[3] == 0x05", g_lastQueueMsg[3], 0x05);
		check_eq("msg[4] == 0xfe", g_lastQueueMsg[4], 0xfe);
		check_eq("Write length == 5", g_lastQueueMsgLen, 5u);

		/* sXCmd non-null with matching magic -> headroom reset skipped */
		unsigned char sxcmdBuf[16];
		memset(sxcmdBuf, 0, sizeof(sxcmdBuf));
		*(unsigned int *)(sxcmdBuf + 5) = 0x22fb39cc;
		sXCmd = sxcmdBuf;
		allPlusOne[0] = 3.0f;
		kAudXBZD = 0x99;
		g->PreprocessPerformanceChange();
		{
			float want = 3.0f;
			check_eq("magic match -> allPlusOne left untouched",
				 *(unsigned int *)&allPlusOne[0], *(unsigned int *)&want);
		}
		check_eq("magic match -> kAudXBZD left untouched", kAudXBZD, 0x99u);

		munmap(msgProc, 0x1040);
		munmap(smoother, 0x10);
		munmap(midiDisp4, 0x10);
		munmap(midiPortMgr2, 0x300);
	}

	printf("\n[34] IsSetListSlotChangeOnly (sec 10.96)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		unsigned char *mgr2 = mmap32(0x24000);
		memset(mgr2, 0, 0x24000);
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgr2;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 4) = (unsigned int)(unsigned long)mgr2;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 8) = 0;

		CSTGPerfChangeRequest req;
		memset(&req, 0, sizeof(req));
		req.tag = 1;
		req.value1 = 3;  /* must match +0x6a5 */
		req.value2 = 2;

		mgr2[0x23d1] = 9; /* != 2 */
		check_eq("mgr gate fails -> false", (unsigned int)g->IsSetListSlotChangeOnly(req), 0u);

		mgr2[0x23d1] = 2;
		buf[0x6a4] = 0;
		check_eq("+0x6a4==0 -> false", (unsigned int)g->IsSetListSlotChangeOnly(req), 0u);

		buf[0x6a4] = 1;
		req.tag = 0;
		check_eq("req.tag==0 -> false", (unsigned int)g->IsSetListSlotChangeOnly(req), 0u);

		req.tag = 1;
		buf[0x6a5] = 9; /* mismatches req.value1==3 */
		check_eq("+0x6a5 mismatch -> false", (unsigned int)g->IsSetListSlotChangeOnly(req), 0u);

		buf[0x6a5] = 3;
		unsigned char *record = buf + 0x2933740 + 3 * 0x834 + (2 << 4);

		*(int *)(buf + 0x684) = 0; /* default mode -> expects record type 1 */
		record[0x10] = 9; /* wrong type */
		check_eq("default mode, wrong record type -> false", (unsigned int)g->IsSetListSlotChangeOnly(req), 0u);

		record[0x10] = 1;
		*(unsigned int *)(buf + 0x688) = 0x11;
		*(unsigned int *)(buf + 0x694) = 0x22;
		record[0x11] = 0x11;
		record[0x12] = 0x22;
		check_eq("default mode, all fields match -> true", (unsigned int)g->IsSetListSlotChangeOnly(req), 1u);

		record[0x12] = 0x99;
		check_eq("default mode, +0x12 mismatch -> false", (unsigned int)g->IsSetListSlotChangeOnly(req), 0u);

		*(int *)(buf + 0x684) = 2; /* mode 2 -> expects record type 2, only +0x12 checked */
		record[0x10] = 2;
		*(unsigned int *)(buf + 0x6a0) = 0x33;
		record[0x12] = 0x33;
		check_eq("mode 2, match -> true", (unsigned int)g->IsSetListSlotChangeOnly(req), 1u);

		*(int *)(buf + 0x684) = 1; /* mode 1 -> expects record type 0 */
		record[0x10] = 0;
		*(unsigned int *)(buf + 0x690) = 0x44;
		*(unsigned int *)(buf + 0x69c) = 0x55;
		record[0x11] = 0x44;
		record[0x12] = 0x55;
		check_eq("mode 1, match -> true", (unsigned int)g->IsSetListSlotChangeOnly(req), 1u);

		munmap(mgr2, 0x24000);
	}

	printf("\n[35] ProcessSetListSlotOnlyChange (sec 10.97)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		/* CSetListSlot::Activate is now real (sec 10.141) and
		 * dereferences the active CSTGPerformanceVarsManager -- the
		 * previous section's own mgr2 was freed at its own end, so
		 * CSTGPerformanceVarsManager::sInstance dangles here otherwise. */
		unsigned char *mgr35 = mmap32(0x24000);
		memset(mgr35, 0, 0x24000);
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgr35;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 4) = (unsigned int)(unsigned long)mgr35;
		CSTGPerformanceVarsManager::sInstance[8] = 0;

		/* Real, confirmed cross-connection caught while building this
		 * test: `+0x2975154`/`+0x2975158`/`+0x297515c` (idx/idx2/
		 * field15c) are NOT independent fields -- they're literally
		 * the SAME memory as the `+0x297514c` saved request's own
		 * `value1`/`value2`/`source` (offsets +0x8/+0xc/+0x10 relative
		 * to +0x297514c). Set up via the request struct directly
		 * (memset FIRST, then fields) to avoid clobbering. */
		*(int *)(buf + 0x684) = 0;                /* mode (not 2) */
		buf[0x6a5] = 7;
		/* SendPerfChangeToMidiOut is now real (sec 10.98) -- give it a
		 * real request (tag!=0) + its own ProgramChangeEnable gate so
		 * an actual call produces an observable WriteSTGMidiOutQueue
		 * side effect, distinguishable from ProcessSetListSlotOnlyChange
		 * simply not having called it at all. */
		CSTGPerfChangeRequest *savedRequest = (CSTGPerfChangeRequest *)(buf + 0x297514c);
		memset(savedRequest, 0, sizeof(*savedRequest));
		savedRequest->tag = 1;
		savedRequest->value1 = 3; /* idx */
		savedRequest->value2 = 2; /* idx2, AND the MIDI program number sent */
		savedRequest->source = 1; /* field15c: triggers BOTH branches */
		buf[0x6d5] = 1; /* ProgramChangeEnable */

		g_pushUnsolicitedMessageCalls = 0;
		g_writeMidiOutQueueCalls = 0;
		unsigned char *record = buf + 0x2933750 + 3 * 0x834 + (2 << 4);
		*(unsigned int *)(record + 4) = 0x55555555;
		*(unsigned int *)(record + 0xc) = 0x66666666;
		*(unsigned int *)(mgr35 + 0x23f0) = 0;
		*(unsigned int *)(mgr35 + 0x23e0) = 0;

		g->ProcessSetListSlotOnlyChange();
		check_eq("+0x6a6 written with idx2", buf[0x6a6], 2);
		check_eq("Activate's real effect: mgr+0x23f0 == record+4",
			 *(unsigned int *)(mgr35 + 0x23f0), 0x55555555u);
		check_eq("  ...mgr+0x23e0 == record+0xc",
			 *(unsigned int *)(mgr35 + 0x23e0), 0x66666666u);
		check_eq("field15c==1 -> PushUnsolicitedMessage called",
			 (unsigned int)g_pushUnsolicitedMessageCalls, 1u);
		check_eq("msg+0xc == +0x6a5", *(unsigned int *)(g_lastUnsolicitedMessage + 0xc), 7u);
		check_eq("msg+0x10 == idx2", *(unsigned int *)(g_lastUnsolicitedMessage + 0x10), 2u);
		check_eq("field15c==1 -> SendPerfChangeToMidiOut ALSO invoked (real Program Change sent)",
			 (unsigned int)g_writeMidiOutQueueCalls, 1u);
		check_eq("  ...program number == request's own value2", g_lastMidiMsg[1], 2);

		*(unsigned int *)(buf + 0x297515c) = 2; /* only push, no SendPerfChangeToMidiOut */
		g_pushUnsolicitedMessageCalls = 0;
		g_writeMidiOutQueueCalls = 0;
		g->ProcessSetListSlotOnlyChange();
		check_eq("field15c==2 -> push only", (unsigned int)g_pushUnsolicitedMessageCalls, 1u);
		check_eq("  ...no SendPerfChangeToMidiOut", (unsigned int)g_writeMidiOutQueueCalls, 0u);

		*(unsigned int *)(buf + 0x297515c) = 0; /* only SendPerfChangeToMidiOut */
		g_pushUnsolicitedMessageCalls = 0;
		g_writeMidiOutQueueCalls = 0;
		g->ProcessSetListSlotOnlyChange();
		check_eq("field15c==0 -> SendPerfChangeToMidiOut only",
			 (unsigned int)g_writeMidiOutQueueCalls, 1u);
		check_eq("  ...no push", (unsigned int)g_pushUnsolicitedMessageCalls, 0u);

		*(unsigned int *)(buf + 0x297515c) = 5; /* neither */
		*(unsigned int *)(mgr35 + 0x23f0) = 0;
		*(unsigned int *)(mgr35 + 0x23e0) = 0;
		g_pushUnsolicitedMessageCalls = 0;
		g_writeMidiOutQueueCalls = 0;
		g->ProcessSetListSlotOnlyChange();
		check_eq("field15c>=3 -> neither (but Activate still fires)",
			 (unsigned int)(g_pushUnsolicitedMessageCalls + g_writeMidiOutQueueCalls), 0u);
		check_eq("  ...Activate always fires regardless (real effect visible again)",
			 (unsigned int)(*(unsigned int *)(mgr35 + 0x23f0) == 0x55555555u &&
					 *(unsigned int *)(mgr35 + 0x23e0) == 0x66666666u), 1u);

		printf("  -- mode 2: extra vtable dispatch on the resolved CSTGSequence --\n");
		static int slot1eCalls;
		static void *lastSlot1eThis;
		static unsigned int lastSlot1eArg;
		slot1eCalls = 0;
		auto slot1e = [](void *self, unsigned int arg) {
			slot1eCalls++; lastSlot1eThis = self; lastSlot1eArg = arg;
		};
		void *vtable[0x1f];
		for (int i = 0; i < 0x1f; i++)
			vtable[i] = 0;
		vtable[0x1e] = (void *)+slot1e;
		unsigned char *seqObj = buf + 0x27cd024; /* seqIndex==0 */
		*(void ***)seqObj = vtable;

		*(unsigned int *)(buf + 0x6a0) = 0;    /* seqIndex 0 */
		*(int *)(buf + 0x684) = 2;               /* mode 2 */
		*(unsigned int *)(buf + 0x297515c) = 5; /* neither push nor send, isolate the vtable call */
		record[3] = 0x42;
		*(unsigned int *)(mgr35 + 0x23f0) = 0;
		*(unsigned int *)(mgr35 + 0x23e0) = 0;
		g->ProcessSetListSlotOnlyChange();
		check_eq("mode 2 -> vtable slot 0x1e dispatched once", (unsigned int)slot1eCalls, 1u);
		check_eq("  ...on the resolved CSTGSequence object", lastSlot1eThis == seqObj, true);
		check_eq("  ...with the record's own +0x3 byte", lastSlot1eArg, 0x42u);
		check_eq("  ...Activate still fires afterward (real effect visible again)",
			 (unsigned int)(*(unsigned int *)(mgr35 + 0x23f0) == 0x55555555u &&
					 *(unsigned int *)(mgr35 + 0x23e0) == 0x66666666u), 1u);
	}

	printf("\n[36] SendPerfChangeToMidiOut (sec 10.98)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		CSTGPerfChangeRequest req;
		memset(&req, 0, sizeof(req));

		printf("  -- tag != 0 (SetListSlotChange) --\n");
		req.tag = 1;
		req.value1 = 0x55;
		req.value2 = 0x66;
		buf[0x6d5] = 0; /* ProgramChangeEnable clear -> no-op */
		g_writeMidiOutQueueCalls = 0;
		g->SendPerfChangeToMidiOut(req);
		check_eq("ProgramChangeEnable clear -> no-op", (unsigned int)g_writeMidiOutQueueCalls, 0u);

		buf[0x6d5] = 1;
		buf[0x6d6] = 0; /* BankChangeEnable clear -> only Program Change */
		g->SendPerfChangeToMidiOut(req);
		check_eq("BankChangeEnable clear -> 1 call (Program Change only)",
			 (unsigned int)g_writeMidiOutQueueCalls, 1u);
		check_eq("  ...msg[0] == channel|0xc0", g_lastMidiMsg[0], (unsigned char)(0 | 0xc0));
		check_eq("  ...msg[1] == value2", g_lastMidiMsg[1], 0x66);
		check_eq("  ...length == 2", g_lastMidiMsgLen, 2u);

		buf[0x6d6] = 1; /* BankChangeEnable set -> Bank Select + Program Change */
		g_writeMidiOutQueueCalls = 0;
		g->SendPerfChangeToMidiOut(req);
		check_eq("BankChangeEnable set -> 2 calls", (unsigned int)g_writeMidiOutQueueCalls, 2u);

		printf("  -- mode 0 (tag==0) --\n");
		req.tag = 0;
		req.mode = 0;
		req.value1 = 0x77;
		req.value2 = 0x88;
		buf[0x6d5] = 0;
		g_writeMidiOutQueueCalls = 0;
		g->SendPerfChangeToMidiOut(req);
		check_eq("mode 0, ProgramChangeEnable clear -> no-op", (unsigned int)g_writeMidiOutQueueCalls, 0u);

		buf[0x6d5] = 1;
		buf[0x6d6] = 1;
		g_convertAliasPgmBankCalls = 0;
		g_writeMidiOutQueueCalls = 0;
		g->SendPerfChangeToMidiOut(req);
		check_eq("mode 0 -> ConvertAliasPgmBankToMidiBank called once",
			 (unsigned int)g_convertAliasPgmBankCalls, 1u);
		check_eq("  ...with req.value1 as bank id", g_lastConvertBankId, 0x77);
		check_eq("  ...2 WriteSTGMidiOutQueue calls (bank select + program change)",
			 (unsigned int)g_writeMidiOutQueueCalls, 2u);

		printf("  -- mode 1 (tag==0) --\n");
		req.mode = 1;
		buf[0x6d5] = 1;
		buf[0x6d7] = 0; /* third gate clear -> no-op */
		g_writeMidiOutQueueCalls = 0;
		g->SendPerfChangeToMidiOut(req);
		check_eq("mode 1, +0x6d7 clear -> no-op", (unsigned int)g_writeMidiOutQueueCalls, 0u);

		buf[0x6d7] = 1;
		buf[0x6d6] = 0;
		g_convertCombiBankCalls = 0;
		g_writeMidiOutQueueCalls = 0;
		g->SendPerfChangeToMidiOut(req);
		check_eq("mode 1, both gates set -> ConvertCombiBankToMidiBank called",
			 (unsigned int)g_convertCombiBankCalls, 1u);
		check_eq("  ...BankChangeEnable clear -> only 1 call (program change)",
			 (unsigned int)g_writeMidiOutQueueCalls, 1u);

		printf("  -- mode 2 (tag==0) --\n");
		req.mode = 2;
		req.value2 = 0x99; /* > 0x7f */
		g_shouldSyncCalls = 0;
		g_writeMidiOutQueueCalls = 0;
		g->SendPerfChangeToMidiOut(req);
		check_eq("value2 > 0x7f -> ShouldSyncExternalClock called once, no MIDI send",
			 (unsigned int)g_shouldSyncCalls, 1u);
		check_eq("  ...no WriteSTGMidiOutQueue", (unsigned int)g_writeMidiOutQueueCalls, 0u);

		req.value2 = 0x40; /* <= 0x7f */
		g_shouldSyncReturn = true;
		g_shouldSyncCalls = 0;
		g_writeMidiOutQueueCalls = 0;
		g->SendPerfChangeToMidiOut(req);
		check_eq("value2<=0x7f, shouldSync==true -> no Song Select sent",
			 (unsigned int)g_writeMidiOutQueueCalls, 0u);
		check_eq("  ...ShouldSyncExternalClock called TWICE (confirmed real quirk)",
			 (unsigned int)g_shouldSyncCalls, 2u);

		g_shouldSyncReturn = false;
		g_shouldSyncCalls = 0;
		g_writeMidiOutQueueCalls = 0;
		g->SendPerfChangeToMidiOut(req);
		check_eq("value2<=0x7f, shouldSync==false -> Song Select sent",
			 (unsigned int)g_writeMidiOutQueueCalls, 1u);
		check_eq("  ...msg[0] == 0xf3 (MIDI Song Select)", g_lastMidiMsg[0], 0xf3);
		check_eq("  ...msg[1] == value2", g_lastMidiMsg[1], 0x40);
		check_eq("  ...still called TWICE", (unsigned int)g_shouldSyncCalls, 2u);

		printf("  -- mode not in {0,1,2} --\n");
		req.mode = 9;
		g_writeMidiOutQueueCalls = 0;
		g_shouldSyncCalls = 0;
		g->SendPerfChangeToMidiOut(req);
		check_eq("unrecognized mode -> no-op", (unsigned int)(g_writeMidiOutQueueCalls + g_shouldSyncCalls), 0u);
	}

	printf("\n[37] HandleMidiBankAndPerformanceChange (sec 10.99)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		/* Same CSTGMidiPortManager::sInstance gap as sections [30]/[32]
		 * above: HandleMidiBankAndPerformanceChange reaches the now-real
		 * SubmitPerfChangeRequest/GetNumWritableBytes (sec 10.116/10.150). */
		unsigned char *midiPortMgr37 = mmap32(0x300);
		CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)midiPortMgr37;
		SetupFakeRingCtl(midiPortMgr37);
		unsigned char *midiDisp5 = mmap32(0x10);
		CSTGMidiDispatcher::sInstance = (CSTGMidiDispatcher *)midiDisp5;
		midiDisp5[0xa2] = 0;
		buf[0x2975185] = 0;
		g->HandleMidiBankAndPerformanceChange(1, 2, 3);
		check_eq("dispatcher gate clear -> no-op", buf[0x2975185], 0);
		midiDisp5[0xa2] = 1;

		buf[0x2975185] = 0; /* slotA = +0x297514c, slotB = +0x2975168 */
		unsigned char *slotA = buf + 0x297514c;
		unsigned char *slotB = buf + 0x2975168;
		memset(slotA, 0, 0x1c);
		memset(slotB, 0, 0x1c);

		slotA[0] = 0;
		*(unsigned int *)(slotA + 4) = 0;
		*(unsigned int *)(slotA + 0xc) = 0xfffe;
		g->HandleMidiBankAndPerformanceChange(1, 2, 3);
		check_eq("slotA early-return check -> no-op", buf[0x2975185], 0);

		*(unsigned int *)(slotA + 0xc) = 0; /* clear the early-return condition */
		slotB[0] = 1; /* slotB[0]!=0 -> tag=1 path */
		*(unsigned int *)(slotB + 8) = 0x77;
		g->HandleMidiBankAndPerformanceChange(0, 5, 9); /* p1==0 -> value1=p2 */
		/* source==2 activates the real debounce check (sec 10.116), but
		 * compareSlot here is slotA ({tag=0,...}), which mismatches this
		 * request's tag(1), so it doesn't fire. SubmitPerfChangeRequest's
		 * real body ALSO sets the pending flag as a side effect -- reset
		 * it back to 0 after each check below so the function's own
		 * internal slotA/slotB selection (which reads that same flag)
		 * doesn't unexpectedly swap for the next call, matching what the
		 * old mock's own side-effect-free behavior always preserved. */
		check_eq("slotB[0]!=0, p1==0 -> submitted", buf[0x2975185], 1);
		check_eq("  ...tag == 1", (unsigned int)PendingRequest(buf).tag, 1u);
		check_eq("  ...mode == 0", PendingRequest(buf).mode, 0u);
		check_eq("  ...value1 == p2 (p1==0)", PendingRequest(buf).value1, 5u);
		check_eq("  ...value2 == p3", PendingRequest(buf).value2, 9u);
		check_eq("  ...source == 2", PendingRequest(buf).source, 2u);
		buf[0x2975185] = 0;
		/* slotB IS the confirmed real queuing target (+0x2975168, when
		 * selector==0) -- the previous call's own real submission just
		 * overwrote it, so re-populate the fixture before reusing it as
		 * a source again. */
		slotB[0] = 1;
		*(unsigned int *)(slotB + 8) = 0x77;

		g->HandleMidiBankAndPerformanceChange(1, 5, 9); /* p1!=0 -> value1=slotB+8 */
		check_eq("slotB[0]!=0, p1!=0 -> value1 == slotB+8", PendingRequest(buf).value1, 0x77u);
		buf[0x2975185] = 0;

		printf("  -- slotB[0]==0: bank conversion paths --\n");
		slotB[0] = 0;
		*(unsigned int *)(slotB + 4) = 1; /* Combi conversion */
		g_convertMidiToCombiCalls = 0;
		g_convertedBankIdReturn = 0x99;
		g->HandleMidiBankAndPerformanceChange(10, 20, 30);
		check_eq("slotB+4==1 -> ConvertMidiBankToCombiBank called",
			 (unsigned int)g_convertMidiToCombiCalls, 1u);
		check_eq("  ...with p1/p2 as the midi bank bytes", (unsigned int)g_lastMidiBankMsb, 10u);
		check_eq("  ...", (unsigned int)g_lastMidiBankLsb, 20u);
		check_eq("  ...tag == 0", (unsigned int)PendingRequest(buf).tag, 0u);
		check_eq("  ...mode == 1", PendingRequest(buf).mode, 1u);
		check_eq("  ...value1 == converted bank id", PendingRequest(buf).value1, 0x99u);
		check_eq("  ...value2 == p3", PendingRequest(buf).value2, 30u);
		buf[0x2975185] = 0;

		*(unsigned int *)(slotB + 4) = 0; /* AliasProgram conversion */
		g_convertMidiToAliasCalls = 0;
		g_convertedBankIdReturn = 0x66;
		g->HandleMidiBankAndPerformanceChange(10, 20, 30);
		check_eq("slotB+4==0 -> ConvertMidiBankToAliasProgramBank called",
			 (unsigned int)g_convertMidiToAliasCalls, 1u);
		check_eq("  ...mode == 0", PendingRequest(buf).mode, 0u);
		check_eq("  ...value1 == converted bank id", PendingRequest(buf).value1, 0x66u);
		buf[0x2975185] = 0;

		*(unsigned int *)(slotB + 4) = 5; /* neither 0 nor 1 -> no-op */
		g->HandleMidiBankAndPerformanceChange(10, 20, 30);
		check_eq("slotB+4 not in {0,1} -> no-op", buf[0x2975185], 0);

		munmap(midiDisp5, 0x10);
		munmap(midiPortMgr37, 0x300);
	}

	printf("\n[38] GetFreeSlotVoiceData (sec 10.100)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		printf("  -- single-element free list (head == tail) --\n");
		unsigned char *node1 = mmap32(0x40);
		memset(node1, 0, 0x40);
		*(unsigned int *)(node1 + 0xc) = (unsigned int)(unsigned long)node1; /* self-pointer */
		unsigned int link1 = (unsigned int)(unsigned long)(node1 + 4);
		*(unsigned int *)(buf + 0x29c98f4) = link1; /* head */
		*(unsigned int *)(buf + 0x29c98f8) = link1; /* tail == head */
		*(unsigned int *)(buf + 0x29c98fc) = 1;      /* free count */
		*(unsigned int *)(buf + 0x29c9908) = 5;      /* active count, will increment */
		*(unsigned int *)(buf + 0x29c9900) = 0;      /* active list empty */

		CSTGSlotVoiceData *got = g->GetFreeSlotVoiceData();
		check_eq("returned the head's own link address", got == (CSTGSlotVoiceData *)(node1 + 4), true);
		check_eq("free head now 0", *(unsigned int *)(buf + 0x29c98f4), 0u);
		check_eq("free tail now 0 (was == head)", *(unsigned int *)(buf + 0x29c98f8), 0u);
		check_eq("free count decremented", *(unsigned int *)(buf + 0x29c98fc), 0u);
		check_eq("node's own +4/+8/+0x10 cleared", *(unsigned int *)(node1 + 4), 0u);
		check_eq("  ...+8", *(unsigned int *)(node1 + 8), 0u);
		check_eq("  ...+0x10", *(unsigned int *)(node1 + 0x10), 0u);
		check_eq("  ...+0xc (the self-pointer) is NOT cleared -- only read",
			 *(unsigned int *)(node1 + 0xc), (unsigned int)(unsigned long)node1);
		check_eq("active list head now points at node1+0x24",
			 *(unsigned int *)(buf + 0x29c9900), (unsigned int)(unsigned long)(node1 + 0x24));
		check_eq("active count incremented", *(unsigned int *)(buf + 0x29c9908), 6u);
		munmap(node1, 0x40);

		printf("  -- multi-element free list --\n");
		unsigned char *nodeA = mmap32(0x40), *nodeB = mmap32(0x40);
		memset(nodeA, 0, 0x40);
		memset(nodeB, 0, 0x40);
		*(unsigned int *)(nodeA + 0xc) = (unsigned int)(unsigned long)nodeA;
		*(unsigned int *)(nodeB + 0xc) = (unsigned int)(unsigned long)nodeB;
		unsigned int linkA = (unsigned int)(unsigned long)(nodeA + 4);
		unsigned int linkB = (unsigned int)(unsigned long)(nodeB + 4);
		/* nodeA is head, nodeB is tail; nodeA's own +4 (next) = linkB */
		*(unsigned int *)(nodeA + 4) = linkB;
		*(unsigned int *)(nodeB + 8) = linkA; /* nodeB's own prev = linkA */
		*(unsigned int *)(buf + 0x29c98f4) = linkA;
		*(unsigned int *)(buf + 0x29c98f8) = linkB;
		*(unsigned int *)(buf + 0x29c98fc) = 2;
		*(unsigned int *)(buf + 0x29c9900) = 0;

		got = g->GetFreeSlotVoiceData();
		check_eq("popped nodeA", got == (CSTGSlotVoiceData *)(nodeA + 4), true);
		check_eq("free head now points at nodeB", *(unsigned int *)(buf + 0x29c98f4), linkB);
		check_eq("free tail unchanged (nodeB)", *(unsigned int *)(buf + 0x29c98f8), linkB);
		check_eq("nodeB's own prev cleared (no predecessor left)",
			 *(unsigned int *)(nodeB + 8), 0u);
		check_eq("free count decremented to 1", *(unsigned int *)(buf + 0x29c98fc), 1u);
		munmap(nodeA, 0x40);
		munmap(nodeB, 0x40);

		printf("  -- free list empty: steal a dying voice --\n");
		unsigned char *snode = mmap32(0x10);
		unsigned char *svoice = mmap32(0x2900);
		unsigned char *ssub = mmap32(0x50);
		memset(svoice, 0, 0x2900);
		*(unsigned int *)(snode + 4) = 0;
		*(unsigned int *)(snode + 8) = (unsigned int)(unsigned long)svoice;
		*(unsigned int *)(svoice + 0x34) = (unsigned int)(unsigned long)ssub;
		ssub[0x43] = 0x00;      /* groupBit 0 */
		svoice[0x40] = 1;         /* dying */
		*(unsigned short *)(svoice + 0x4c) = 0;
		*(unsigned short *)(svoice + 0x58) = 0; /* sum==0 -> FreeSlotVoiceData(true) IS called */
		svoice[0x41] = 0;
		*(unsigned int *)(buf + 0x29c9904) = (unsigned int)(unsigned long)snode;
		*(unsigned int *)(buf + 0x29c98fc) = 0; /* free list empty -> triggers steal path */
		*(unsigned int *)(buf + 0x29c98f4) = 0;
		*(unsigned int *)(buf + 0x29c9900) = 0;

		/* Simulate FreeSlotVoiceData(true)'s own real effect: populate
		 * the free list with a fresh node so the real retry loop
		 * actually terminates (see this test file's own hook comment). */
		unsigned char *stolenNode = mmap32(0x40);
		memset(stolenNode, 0, 0x40);
		*(unsigned int *)(stolenNode + 0xc) = (unsigned int)(unsigned long)stolenNode;
		static unsigned char *hookNode;
		static unsigned char *hookBase;
		hookNode = stolenNode;
		hookBase = buf;
		g_freeSlotVoiceDataHook = [](void *) {
			unsigned int link = (unsigned int)(unsigned long)(hookNode + 4);
			*(unsigned int *)(hookBase + 0x29c98f4) = link;
			*(unsigned int *)(hookBase + 0x29c98f8) = link;
			*(unsigned int *)(hookBase + 0x29c98fc) = 1;
		};

		g_doPendingMoveVoicesCalls = 0;
		g_freeSlotVoiceDataCalls = 0;
		got = g->GetFreeSlotVoiceData();
		check_eq("stole a voice: EmergencyFreeAllVoices called (2 real EmergencyFreeVoiceList invocations)",
			 (unsigned int)g_doPendingMoveVoicesCalls, 2u);
		check_eq("  ...FreeSlotVoiceData(true) ALSO called (sum==0)",
			 (unsigned int)g_freeSlotVoiceDataCalls, 1u);
		check_eq("  ...with flag true", (unsigned int)g_lastFreeSlotVoiceDataFlag, 1u);
		check_eq("  ...then successfully returned the newly-freed node",
			 got == (CSTGSlotVoiceData *)(stolenNode + 4), true);

		g_freeSlotVoiceDataHook = 0;
		munmap(snode, 0x10);
		munmap(svoice, 0x2900);
		munmap(ssub, 0x50);
		munmap(stolenNode, 0x40);
	}

	printf("\n[39] ProcessCCSpecialMapping (sec 10.101)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		unsigned char *ctrlRT = mmap32(0x10);
		CSTGControllerRTData::sInstance = (CSTGControllerRTData *)ctrlRT;

		check_eq("p1 > 0x77 -> 0", (unsigned int)g->ProcessCCSpecialMapping(0x78, 0, 0), 0u);

		unsigned char *record = buf + 0x29cc11c + 5 * 8;
		memset(record, 0, 8);
		check_eq("claimed-flag clear -> 0", (unsigned int)g->ProcessCCSpecialMapping(5, 1, 1), 0u);

		record[0] = 1; /* claimed */
		record[1] = 7; /* value byte */
		check_eq("value mismatch (p2 != value, value != 0x10) -> 0",
			 (unsigned int)g->ProcessCCSpecialMapping(5, 9, 1), 0u);

		record[1] = 0x10; /* wildcard value */
		buf[0x6b8] = 3;
		check_eq("value==0x10, p2 != channel -> 0",
			 (unsigned int)g->ProcessCCSpecialMapping(5, 9, 1), 0u);

		printf("  -- occupied (free-flag clear): HandleControllerChange path --\n");
		record[1] = 7;
		record[2] = 0; /* occupied */
		*(unsigned int *)(record + 4) = 0x55;
		g_handleControllerChangeCalls = 0;
		int ret = g->ProcessCCSpecialMapping(5, 7, 0x77);
		check_eq("returns 1", (unsigned int)ret, 1u);
		check_eq("HandleControllerChange called once", (unsigned int)g_handleControllerChangeCalls, 1u);
		check_eq("  ...assign == record's own tag", (unsigned int)g_lastHandleControllerChangeAssign, 0x55u);
		check_eq("  ...value == p3", (unsigned int)g_lastHandleControllerChangeValue, 0x77u);
		check_eq("  ...both flags false", (unsigned int)(g_lastHandleControllerChangeFlag1 ||
								    g_lastHandleControllerChangeFlag2), 0u);

		printf("  -- free (free-flag set): SetPerfSwitch path, default mode --\n");
		record[2] = 1; /* free */
		*(int *)(buf + 0x684) = 0; /* default mode */
		*(unsigned int *)(buf + 0x698) = 0x123; /* not 0xfffe */
		*(unsigned int *)(buf + 0x68c) = 5;
		*(unsigned int *)(record + 4) = 0x99;
		unsigned char *expectedTarget = buf + (0x123 & 0x7f) * 0xcec + 5 * 0x67603 + 0x132e4d0 + 3;
		g_setPerfSwitchCalls = 0;
		ret = g->ProcessCCSpecialMapping(5, 7, 0x50); /* p3 > 0x3f */
		check_eq("returns 1", (unsigned int)ret, 1u);
		check_eq("target+0xadf == record's own tag low byte", expectedTarget[0xadf], 0x99);
		check_eq("SetPerfSwitch called once", (unsigned int)g_setPerfSwitchCalls, 1u);
		check_eq("  ...this == target+0xad3", g_lastSetPerfSwitchThis == (void *)(expectedTarget + 0xad3), true);
		check_eq("  ...perfSwitch == 2", g_lastSetPerfSwitchEnum, 2);
		check_eq("  ...value == (p3 > 0x3f)", (unsigned int)g_lastSetPerfSwitchValue, 1u);

		printf("  -- fieldAt(0x698)==0xfffe special-case address --\n");
		*(unsigned int *)(buf + 0x698) = 0xfffe;
		unsigned char *specialTarget = buf + 0x2976e33;
		g_setPerfSwitchCalls = 0;
		g->ProcessCCSpecialMapping(5, 7, 0x10); /* p3 <= 0x3f */
		check_eq("target == +0x2976e33", g_lastSetPerfSwitchThis == (void *)(specialTarget + 0xad3), true);
		check_eq("  ...value == (p3 > 0x3f) == false", (unsigned int)g_lastSetPerfSwitchValue, 0u);

		printf("  -- mode 1 (CSTGCombi) / mode 2 (CSTGSequence) target bases --\n");
		*(int *)(buf + 0x684) = 1;
		*(unsigned int *)(buf + 0x69c) = 2;
		*(unsigned int *)(buf + 0x690) = 1;
		unsigned char *combiTarget = buf + 2 * 0x19e7 + 1 * 0xcf381 + 0x1c77f10 + 6;
		g->ProcessCCSpecialMapping(5, 7, 0);
		check_eq("mode 1 -> CSTGCombi target", g_lastSetPerfSwitchThis == (void *)(combiTarget + 0xad3), true);

		*(int *)(buf + 0x684) = 2;
		*(unsigned int *)(buf + 0x6a0) = 3;
		unsigned char *seqTarget = buf + 3 * 0x1cad + 0x27cd024;
		g->ProcessCCSpecialMapping(5, 7, 0);
		check_eq("mode 2 -> CSTGSequence target", g_lastSetPerfSwitchThis == (void *)(seqTarget + 0xad3), true);

		munmap(ctrlRT, 0x10);
	}

	printf("\n[40] CompletePerformanceActivation (sec 10.102)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		unsigned char *ctrlRT2 = mmap32(0x10);
		CSTGControllerRTData::sInstance = (CSTGControllerRTData *)ctrlRT2;
		unsigned char *midiPortMgr3 = mmap32(0x300);
		CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)midiPortMgr3;
		SetupFakeRingCtl(midiPortMgr3);
		unsigned char *mgr3 = mmap32(0x24000);
		memset(mgr3, 0, 0x24000);
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgr3;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 4) = (unsigned int)(unsigned long)mgr3;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 8) = 0;
		unsigned char *perfObj = mmap32(0x10);
		*(unsigned int *)(mgr3 + 0x23d4) = (unsigned int)(unsigned long)perfObj;

		buf[0x6a4] = 1; /* run the CSetList/CSetListSlot Activate + perf-pointer path */
		buf[0x6a5] = 3;
		buf[0x6a6] = 2;
		buf[0x29cc0c9] = 1; /* ext-mode-deferred bit set */
		*(int *)(buf + 0x684) = 0; /* default mode */
		*(unsigned int *)(buf + 0x68c) = 5;
		buf[0x698] = 9;
		buf[0x6b8] = 4; /* channel */

		g_setListActivateCalls = 0;
		g_onExtModeSetChangeCalls = 0;
		g_onPerformanceActivateCalls = 0;
		g_writeQueueCalls = 0;

		unsigned char *setListObj = buf + 3 * 0x834 + 0x293374c;
		unsigned char *setListSlot = buf + 0x2933750 + 3 * 0x834 + (2 << 4);
		*(unsigned int *)(setListSlot + 4) = 0x11111111;
		*(unsigned int *)(setListSlot + 0xc) = 0x22222222;

		g->CompletePerformanceActivation();
		check_eq("+0x2975184 set to 3", buf[0x2975184], 3);
		check_eq("CSetList::Activate called once", (unsigned int)g_setListActivateCalls, 1u);
		check_eq("  ...on the real idx-only-strided object", g_lastSetListActivateThis == setListObj, true);
		check_eq("CSetListSlot::Activate's real effect: mgr+0x23f0 == slot+4",
			 *(unsigned int *)(mgr3 + 0x23f0), 0x11111111u);
		check_eq("  ...mgr+0x23e0 == slot+0xc",
			 *(unsigned int *)(mgr3 + 0x23e0), 0x22222222u);
		check_eq("OnExtModeSetChange fires (bit was set)", (unsigned int)g_onExtModeSetChangeCalls, 1u);
		check_eq("  ...bit cleared afterward", (unsigned int)(buf[0x29cc0c9] & 1), 0u);
		check_eq("OnPerformanceActivate called once", (unsigned int)g_onPerformanceActivateCalls, 1u);
		check_eq("  ...with the real resolved CSTGPerformance&",
			 g_lastOnPerformanceActivateArg == perfObj, true);
		check_eq("2 MIDI messages sent (Karma-shaped + Preprocess-shaped)",
			 (unsigned int)g_writeQueueCalls, 2u);

		munmap(ctrlRT2, 0x10);
		munmap(midiPortMgr3, 0x300);
		munmap(mgr3, 0x24000);
		munmap(perfObj, 0x10);
	}

	printf("\n[41] IncrementPerformance/DecrementPerformance (sec 10.103)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		/* Same CSTGMidiPortManager::sInstance gap as sections [30]/[32]/[37]
		 * above: IncrementPerformance/DecrementPerformance reach the
		 * now-real SubmitPerfChangeRequest/GetNumWritableBytes (sec
		 * 10.116/10.150). */
		unsigned char *midiPortMgr41 = mmap32(0x300);
		CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)midiPortMgr41;
		SetupFakeRingCtl(midiPortMgr41);
		unsigned char *midiDisp6 = mmap32(0x10);
		CSTGMidiDispatcher::sInstance = (CSTGMidiDispatcher *)midiDisp6;
		midiDisp6[0xa2] = 1;
		buf[0x2975185] = 0; /* slotA=+0x297514c, slotB=+0x2975168 */
		unsigned char *slotA = buf + 0x297514c;
		unsigned char *slotB = buf + 0x2975168;
		memset(slotA, 0, 0x1c);
		memset(slotB, 0, 0x1c);
		*(unsigned int *)(slotA + 0xc) = 0; /* clear the early-return trigger */

		printf("  -- Increment, slotB[0]!=0 (tag=1) --\n");
		/* source==1 here, so SubmitPerfChangeRequest's own real
		 * dedup/debounce check (only active for source==2) never
		 * engages -- no field-mismatch reasoning needed. Its real body
		 * DOES still set the pending flag as a side effect though, so
		 * reset it to 0 after each check to keep IncrementPerformance/
		 * DecrementPerformance's own internal slotA/slotB selection
		 * (which reads that same flag) from swapping between calls,
		 * matching what the old side-effect-free mock always preserved. */
		slotB[0] = 1;
		*(unsigned int *)(slotB + 8) = 10;   /* value1 */
		*(unsigned int *)(slotB + 0xc) = 20;  /* value2 <= 0x7e */
		g->IncrementPerformance();
		check_eq("value2<=0x7e -> value2++ only", PendingRequest(buf).value1, 10u);
		check_eq("  ...", PendingRequest(buf).value2, 21u);
		check_eq("source == 1 (confirmed distinct from other producers)",
			 PendingRequest(buf).source, 1u);
		buf[0x2975185] = 0;

		*(unsigned int *)(slotB + 0xc) = 0x7f; /* > 0x7e, value1 <= 0x7e */
		g->IncrementPerformance();
		check_eq("value2>0x7e, value1<=0x7e -> value1++, value2=0", PendingRequest(buf).value1, 11u);
		check_eq("  ...", PendingRequest(buf).value2, 0u);
		buf[0x2975185] = 0;

		/* The previous call's own real submission just overwrote
		 * slotB(==+0x2975168 here)'s own value2 back to 0 -- re-set it
		 * to 0x7f too so both fields are actually >0x7e for this check,
		 * matching what the test always intended. */
		*(unsigned int *)(slotB + 8) = 0x7f; /* both > 0x7e now */
		*(unsigned int *)(slotB + 0xc) = 0x7f;
		g->IncrementPerformance();
		check_eq("both > 0x7e -> both reset to 0", PendingRequest(buf).value1, 0u);
		check_eq("  ...", PendingRequest(buf).value2, 0u);
		buf[0x2975185] = 0;

		printf("  -- Decrement, slotB[0]!=0 (tag=1) --\n");
		*(unsigned int *)(slotB + 8) = 10;
		*(unsigned int *)(slotB + 0xc) = 20;
		g->DecrementPerformance();
		check_eq("value2!=0 -> value2-- only", PendingRequest(buf).value1, 10u);
		check_eq("  ...", PendingRequest(buf).value2, 19u);
		buf[0x2975185] = 0;

		*(unsigned int *)(slotB + 0xc) = 0;
		g->DecrementPerformance();
		check_eq("value2==0, value1!=0 -> value1--, value2=0x7f", PendingRequest(buf).value1, 9u);
		check_eq("  ...", PendingRequest(buf).value2, 0x7fu);
		buf[0x2975185] = 0;

		/* Same real-aliasing consideration as above: the previous call's
		 * own submission left value2 at 0x7f, not 0 -- re-set it so
		 * both fields are actually 0 for this check. */
		*(unsigned int *)(slotB + 8) = 0;
		*(unsigned int *)(slotB + 0xc) = 0;
		g->DecrementPerformance();
		check_eq("both 0 -> both reset to 0x7f", PendingRequest(buf).value1, 0x7fu);
		check_eq("  ...", PendingRequest(buf).value2, 0x7fu);
		buf[0x2975185] = 0;

		printf("  -- slotB[0]==0: bank conversion dispatch --\n");
		slotB[0] = 0;
		*(unsigned int *)(slotB + 4) = 1; /* Combi */
		*(unsigned int *)(slotB + 8) = 5;   /* bankId */
		*(unsigned int *)(slotB + 0xc) = 6; /* index */
		g->IncrementPerformance();
		check_eq("mode 1 -> IncrementCombiIndex's real computation: mode == 1",
			 PendingRequest(buf).mode, 1u);
		check_eq("  ...value1 == bankId (unchanged, index+1 <= 0x7f)",
			 PendingRequest(buf).value1, 5u);
		check_eq("  ...value2 == index+1", PendingRequest(buf).value2, 7u);
		buf[0x2975185] = 0;

		/* Same real-aliasing consideration as the request-submission
		 * tests above: re-set slotB's own source fields before the
		 * next call, since the prior submission may have overwritten
		 * this same physical slot. */
		*(unsigned int *)(slotB + 4) = 1;
		*(unsigned int *)(slotB + 8) = 5;
		*(unsigned int *)(slotB + 0xc) = 6;
		g->DecrementPerformance();
		check_eq("mode 1 -> DecrementCombiIndex's real computation: value1 == bankId (unchanged)",
			 PendingRequest(buf).value1, 5u);
		check_eq("  ...value2 == index-1", PendingRequest(buf).value2, 5u);
		buf[0x2975185] = 0;

		*(unsigned int *)(slotB + 4) = 0; /* AliasProgram */
		*(unsigned int *)(slotB + 8) = 5;
		*(unsigned int *)(slotB + 0xc) = 0x7f; /* index at the wrap boundary */
		g->IncrementPerformance();
		check_eq("mode 0 -> IncrementAliasProgramIndex's real computation: mode == 0",
			 PendingRequest(buf).mode, 0u);
		check_eq("  ...index wraps to 0, bankId increments (0x7f+1 > 0x7f)",
			 PendingRequest(buf).value2, 0u);
		check_eq("  ...value1 == bankId+1", PendingRequest(buf).value1, 6u);
		buf[0x2975185] = 0;

		*(unsigned int *)(slotB + 4) = 0;
		*(unsigned int *)(slotB + 8) = 0;
		*(unsigned int *)(slotB + 0xc) = 0; /* both 0 -> wrap-to-last-bank case */
		g->DecrementPerformance();
		check_eq("mode 0 -> DecrementAliasProgramIndex's real computation: index -> 0x7f",
			 PendingRequest(buf).value2, 0x7fu);
		check_eq("  ...bankId==0 -> wraps to the confirmed real last bank (0x1e)",
			 PendingRequest(buf).value1, 0x1eu);
		buf[0x2975185] = 0;

		*(unsigned int *)(slotB + 4) = 9; /* neither -> no-op */
		g->IncrementPerformance();
		g->DecrementPerformance();
		check_eq("slotB+4 not in {0,1} -> no-op for both", buf[0x2975185], 0);

		munmap(midiDisp6, 0x10);
		munmap(midiPortMgr41, 0x300);
	}

	printf("\n[42] ProcessPerfChangeRequest (sec 10.104)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		printf("  -- not a SetListSlotChangeOnly -> deactivate sequence --\n");
		unsigned char *mgr4 = mmap32(0x24000);
		memset(mgr4, 0, 0x24000);
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgr4;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 4) = (unsigned int)(unsigned long)mgr4;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 8) = 0;
		mgr4[0x23d1] = 9; /* != 2 -> IsSetListSlotChangeOnly returns false unconditionally */

		unsigned char *msgProc2 = mmap32(0x1040);
		memset(msgProc2, 0, 0x1040);
		CSTGMessageProcessor::sInstance = (CSTGMessageProcessor *)msgProc2;
		unsigned char *smoother2 = mmap32(0x10);
		CSTGSmoother::sInstance = (CSTGSmoother *)smoother2;
		unsigned char *midiDisp7 = mmap32(0x10);
		CSTGMidiDispatcher::sInstance = (CSTGMidiDispatcher *)midiDisp7;
		unsigned char *midiPortMgr4 = mmap32(0x300);
		CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)midiPortMgr4;
		SetupFakeRingCtl(midiPortMgr4);
		sXCmd = 0;

		CSTGPerfChangeRequest req;
		memset(&req, 0, sizeof(req));
		req.tag = 1;
		req.value1 = 0x11;
		req.value2 = 0x22;
		req.source = 0x33;
		req.field14 = 0x44;
		req.field18 = 0x55;

		g_writeQueueCalls = 0;
		g->ProcessPerfChangeRequest(req);
		check_eq("request copied verbatim into +0x297514c slot",
			 *(unsigned int *)(buf + 0x297514c + 8), 0x11u);
		check_eq("  ...+0xc", *(unsigned int *)(buf + 0x297514c + 0xc), 0x22u);
		check_eq("  ...+0x10", *(unsigned int *)(buf + 0x297514c + 0x10), 0x33u);
		check_eq("deactivate sequence ran: msg sent", (unsigned int)g_writeQueueCalls, 1u);
		check_eq("  ...msg[2] == 0x03 (Preprocess-shaped, not 0x04)", g_lastQueueMsg[2], 0x03);
		check_eq("+0x2975184 set to 1", buf[0x2975184], 1);
		check_eq("CSTGMessageProcessor+0x54 set", msgProc2[0x54], 1);

		printf("  -- IsSetListSlotChangeOnly==true -> Activate + push/send tail --\n");
		mgr4[0x23d1] = 2; /* active */
		buf[0x6a4] = 1;
		buf[0x6a5] = 3; /* must match req.value1 below */
		*(int *)(buf + 0x684) = 0; /* default mode */
		*(unsigned int *)(buf + 0x688) = 0x77;
		*(unsigned int *)(buf + 0x694) = 0x88;
		unsigned char *record2 = buf + 0x2933750 + 3 * 0x834 + (5 << 4);
		record2[0] = 1;    /* == IsSetListSlotChangeOnly's own "record[0x10]" (its record base is +0x2933740, 0x10 less than CSetListSlot's own +0x2933750 base) */
		record2[1] = 0x77;
		record2[2] = 0x88;
		*(unsigned int *)(record2 + 4) = 0x33333333;
		*(unsigned int *)(record2 + 0xc) = 0x44444444;

		memset(&req, 0, sizeof(req));
		req.tag = 1;
		req.value1 = 3;  /* idx, matches +0x6a5 */
		req.value2 = 5;  /* idx2 */
		req.source = 1;  /* triggers BOTH push and SendPerfChangeToMidiOut */

		*(unsigned int *)(mgr4 + 0x23f0) = 0;
		*(unsigned int *)(mgr4 + 0x23e0) = 0;
		g_pushUnsolicitedMessageCalls = 0;
		g_writeMidiOutQueueCalls = 0;
		buf[0x6d5] = 1; /* ProgramChangeEnable, so SendPerfChangeToMidiOut actually sends */
		g->ProcessPerfChangeRequest(req);
		check_eq("+0x6a6 written with idx2", buf[0x6a6], 5);
		check_eq("CSetListSlot::Activate's real effect: mgr+0x23f0 == record+4",
			 *(unsigned int *)(mgr4 + 0x23f0), 0x33333333u);
		check_eq("  ...mgr+0x23e0 == record+0xc",
			 *(unsigned int *)(mgr4 + 0x23e0), 0x44444444u);
		check_eq("source==1 -> push message sent", (unsigned int)g_pushUnsolicitedMessageCalls, 1u);
		check_eq("source==1 -> SendPerfChangeToMidiOut ALSO invoked",
			 (unsigned int)g_writeMidiOutQueueCalls, 1u);

		munmap(mgr4, 0x24000);
		munmap(msgProc2, 0x1040);
		munmap(smoother2, 0x10);
		munmap(midiDisp7, 0x10);
		munmap(midiPortMgr4, 0x300);
	}

	printf("\n[43] StartPendingPerformanceChange (sec 10.105)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		unsigned char *mgr5 = mmap32(0x24000);
		memset(mgr5, 0, 0x24000);
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgr5;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 4) = (unsigned int)(unsigned long)mgr5;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 8) = 0;
		mgr5[0x23d1] = 9; /* != 2 -> IsSetListSlotChangeOnly false -> deactivate sequence */

		unsigned char *msgProc3 = mmap32(0x1040);
		memset(msgProc3, 0, 0x1040);
		CSTGMessageProcessor::sInstance = (CSTGMessageProcessor *)msgProc3;
		unsigned char *smoother3 = mmap32(0x10);
		CSTGSmoother::sInstance = (CSTGSmoother *)smoother3;
		unsigned char *midiDisp8 = mmap32(0x10);
		CSTGMidiDispatcher::sInstance = (CSTGMidiDispatcher *)midiDisp8;
		unsigned char *midiPortMgr5 = mmap32(0x300);
		CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)midiPortMgr5;
		SetupFakeRingCtl(midiPortMgr5);
		sXCmd = 0;

		CSTGPerfChangeRequest *pendingSlot = (CSTGPerfChangeRequest *)(buf + 0x2975168);
		memset(pendingSlot, 0, sizeof(*pendingSlot));
		pendingSlot->tag = 1;
		pendingSlot->value1 = 0x66;
		pendingSlot->value2 = 0x77;
		pendingSlot->source = 0x88;

		buf[0x2975184] = 1; /* busy -> no-op */
		buf[0x2975185] = 1; /* pending set */
		g_writeQueueCalls = 0;
		g->StartPendingPerformanceChange();
		check_eq("busy flag set -> no-op", (unsigned int)g_writeQueueCalls, 0u);
		check_eq("  ...pending flag untouched", buf[0x2975185], 1);

		buf[0x2975184] = 0;
		buf[0x2975185] = 0; /* nothing pending -> no-op */
		g->StartPendingPerformanceChange();
		check_eq("nothing pending -> no-op", (unsigned int)g_writeQueueCalls, 0u);

		buf[0x2975185] = 1; /* pending set, not busy -> runs */
		g->StartPendingPerformanceChange();
		check_eq("pending flag cleared", buf[0x2975185], 0);
		check_eq("slotB copied into slotA: value1", *(unsigned int *)(buf + 0x297514c + 8), 0x66u);
		check_eq("  ...value2", *(unsigned int *)(buf + 0x297514c + 0xc), 0x77u);
		check_eq("  ...source", *(unsigned int *)(buf + 0x297514c + 0x10), 0x88u);
		check_eq("deactivate sequence ran (dispatched via ProcessPerfChangeRequest)",
			 (unsigned int)g_writeQueueCalls, 1u);

		munmap(mgr5, 0x24000);
		munmap(msgProc3, 0x1040);
		munmap(smoother3, 0x10);
		munmap(midiDisp8, 0x10);
		munmap(midiPortMgr5, 0x300);
	}

	printf("\n[44] GetMostRecentlyRequestedPerformanceIdForType (sec 10.106)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);
		CPerformanceId pid;

		printf("  -- pending flag clear -> falls straight to slotA --\n");
		buf[0x2975185] = 0;
		buf[0x297514c] = 1; /* slotA.tag != 0 */
		*(unsigned int *)(buf + 0x2975154) = 4; /* value1 */
		*(unsigned int *)(buf + 0x2975158) = 6; /* value2 */
		/* Real, confirmed overlap: the "type" byte
		 * (DoesPerfChangeRequestMatchType's own `+0x2933750+offset`,
		 * sec 10.92) and the packed id word's own LOW byte
		 * (GetPerformanceIdFromPerfChangeRequest's own `record+0x10`,
		 * same sec) are the EXACT SAME memory location (`record` base
		 * is 0x10 less than the type-table base) -- caught via a real
		 * test failure when these were set independently. Set the
		 * packed word so its own low byte doubles as the confirmed
		 * type. */
		unsigned char *record2A = buf + 0x2933740 + 4 * 0x834 + (6 << 4);
		*(unsigned short *)(record2A + 0x10) = 0x2207; /* low byte 0x07 == type 7 */
		record2A[0x12] = 0x33;

		memset(&pid, 0xff, sizeof(pid));
		bool ok = g->GetMostRecentlyRequestedPerformanceIdForType(7, &pid);
		check_eq("slotA match -> true", (unsigned int)ok, 1u);
		check_eq("  ...byte0 == low(0x2207)", pid.byte0, 0x07);
		check_eq("  ...byte1 == high(0x2207)", pid.byte1, 0x22);
		check_eq("  ...byte2 == 0x33", pid.byte2, 0x33);

		ok = g->GetMostRecentlyRequestedPerformanceIdForType(9, &pid);
		check_eq("slotA type mismatch, falls to literal fallback (type!=0,1,2) -> false",
			 (unsigned int)ok, 0u);

		printf("  -- neither slot matches -> literal type fallback --\n");
		*(unsigned short *)(record2A + 0x10) = 0x2255; /* type byte (low) no longer 0,1,2 */
		buf[0x297514c] = 0; /* slotA.tag == 0 now, uses the mode-based match check instead */
		/* mode=1 -> m=0 (<=1) -> slotA's own match check compares
		 * against a table value (+0x64) rather than a literal, so
		 * picking a sentinel there keeps slotA from ever matching any
		 * of the types tested below, forcing every one of them
		 * through to the literal fallback as intended. */
		*(unsigned int *)(buf + 0x2975150) = 1;
		*(unsigned int *)(buf + 0x64) = 99;

		*(unsigned int *)(buf + 0x694) = 0x99;
		*(unsigned int *)(buf + 0x688) = 0x88;
		ok = g->GetMostRecentlyRequestedPerformanceIdForType(1, &pid);
		check_eq("type==1 literal fallback -> true", (unsigned int)ok, 1u);
		check_eq("  ...byte0 == 1 (literal)", pid.byte0, 1);
		check_eq("  ...byte1 == +0x688", pid.byte1, 0x88);
		check_eq("  ...byte2 == +0x694", pid.byte2, 0x99);

		*(unsigned int *)(buf + 0x694) = 0xfffe;
		ok = g->GetMostRecentlyRequestedPerformanceIdForType(1, &pid);
		check_eq("type==1, +0x694==0xfffe sentinel -> false", (unsigned int)ok, 0u);

		*(unsigned int *)(buf + 0x6a0) = 0x77;
		ok = g->GetMostRecentlyRequestedPerformanceIdForType(2, &pid);
		check_eq("type==2 literal fallback -> true", (unsigned int)ok, 1u);
		check_eq("  ...byte0 == 2 (literal)", pid.byte0, 2);
		check_eq("  ...byte1 == 0 (literal)", pid.byte1, 0);
		check_eq("  ...byte2 == +0x6a0", pid.byte2, 0x77);

		*(unsigned int *)(buf + 0x69c) = 0x66;
		*(unsigned int *)(buf + 0x690) = 0x55;
		ok = g->GetMostRecentlyRequestedPerformanceIdForType(0, &pid);
		check_eq("type==0 literal fallback -> true", (unsigned int)ok, 1u);
		check_eq("  ...byte0 == 0 (literal)", pid.byte0, 0);
		check_eq("  ...byte1 == +0x690", pid.byte1, 0x55);
		check_eq("  ...byte2 == +0x69c", pid.byte2, 0x66);

		ok = g->GetMostRecentlyRequestedPerformanceIdForType(3, &pid);
		check_eq("type not in {0,1,2}, no slot match -> false", (unsigned int)ok, 0u);

		printf("  -- pending slot (tag!=0) matches, takes priority over slotA --\n");
		buf[0x2975185] = 1; /* pending flag set */
		buf[0x2975168] = 1; /* pending.tag != 0 */
		*(unsigned int *)(buf + 0x2975170) = 2; /* pending.value1 */
		*(unsigned int *)(buf + 0x2975174) = 3; /* pending.value2 */
		unsigned char *record2P = buf + 0x2933740 + 2 * 0x834 + (3 << 4);
		*(unsigned short *)(record2P + 0x10) = 0x440c; /* low byte 0x0c == type 12 */
		record2P[0x12] = 0x55;

		ok = g->GetMostRecentlyRequestedPerformanceIdForType(12, &pid);
		check_eq("pending slot match -> true", (unsigned int)ok, 1u);
		check_eq("  ...byte0 == low(0x440c)", pid.byte0, 0x0c);
		check_eq("  ...byte1 == high(0x440c)", pid.byte1, 0x44);
		check_eq("  ...byte2 == 0x55", pid.byte2, 0x55);
	}

	printf("\n[45] SetEditInContextState (sec 10.107)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);
		CSTGGlobal::sInstance = g;

		unsigned char *ctrlRT3 = mmap32(0x30);
		memset(ctrlRT3, 0, 0x30);
		CSTGControllerRTData::sInstance = (CSTGControllerRTData *)ctrlRT3;

		/* NotifySoloChange (sec 10.107) resolves "the current
		 * performance object" via the SAME confirmed real mode
		 * dispatch as ResolveCurrentPerformance() (sec 10.77); with
		 * buf zeroed, mode==0 and progIdx==0 (not the 0xfffe
		 * sentinel), landing at the confirmed default-mode formula
		 * 0*0xcec + 0*0x67603 + 0x132e4d0 + 3 = 0x132e4d3, then makes
		 * a real virtual call through THAT object's own vtable slot
		 * 0x1b -- plant a fake vtable there so the call is observable
		 * instead of crashing. */
		void *fakeVtable[0x1c];
		for (int i = 0; i < 0x1c; i++)
			fakeVtable[i] = 0;
		fakeVtable[0x1b] = (void *)NotifySoloChangeVtableFn;
		unsigned char *perfObj = buf + 0x132e4d3;
		*(void ***)perfObj = fakeVtable;

		g->SetEditInContextState(0, 0x42);
		check_eq("type==0: +0x29cc4dc still written", *(int *)(buf + 0x29cc4dc), 0);
		check_eq("  ...+0x29cc4e0 still written", *(unsigned int *)(buf + 0x29cc4e0), 0x42u);
		check_eq("  ...but no further effect: +0x29cc4e4 untouched",
			 *(unsigned short *)(buf + 0x29cc4e4), 0);
		check_eq("  ...and NotifySoloChange NOT called (type==0 early-returns)",
			 (unsigned int)g_notifySoloChangeVtableCalls, 0u);

		g_notifySoloChangeVtableCalls = 0;
		*(unsigned short *)(ctrlRT3 + 0x22) = 0xbeef;
		g->SetEditInContextState(3, 0x99);
		check_eq("type!=0: +0x29cc4dc set", *(int *)(buf + 0x29cc4dc), 3);
		check_eq("  ...+0x29cc4e0 set", *(unsigned int *)(buf + 0x29cc4e0), 0x99u);
		check_eq("  ...flags word copied from CSTGControllerRTData+0x22",
			 *(unsigned short *)(buf + 0x29cc4e4), 0xbeef);
		check_eq("  ...source word zeroed", *(unsigned short *)(ctrlRT3 + 0x22), 0);
		check_eq("NotifySoloChange's own real vtable call fired once",
			 (unsigned int)g_notifySoloChangeVtableCalls, 1u);
		check_eq("  ...on the confirmed resolved default-mode performance address",
			 (unsigned int)(g_lastNotifySoloChangeVtableThis == perfObj), 1u);

		munmap(ctrlRT3, 0x30);
	}

	printf("\n[46] CompletePerformanceChange (sec 10.108)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		unsigned char *midiDisp9 = mmap32(0x10);
		CSTGMidiDispatcher::sInstance = (CSTGMidiDispatcher *)midiDisp9;
		unsigned char *msgProc4 = mmap32(0x1040);
		CSTGMessageProcessor::sInstance = (CSTGMessageProcessor *)msgProc4;
		unsigned char *ctrlRT4 = mmap32(0x30);
		CSTGControllerRTData::sInstance = (CSTGControllerRTData *)ctrlRT4;
		unsigned char *panel = mmap32(0x29200);
		memset(panel, 0, 0x29200);
		STGAPIFrontPanelStatus::sInstance = panel;
		/* ResetAllJumpCatch's own real body (sec 10.129) needs a valid
		 * active CSTGPerformanceVarsManager with +0x23d1 == 2. */
		unsigned char *mgr46 = mmap32(0x2400);
		memset(mgr46, 0, 0x2400);
		mgr46[0x23d1] = 2;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgr46;
		CSTGPerformanceVarsManager::sInstance[8] = 0;

		printf("  -- source not in {1,2}: no message, just clears state --\n");
		buf[0x2975184] = 1;
		*(unsigned int *)(buf + 0x297515c) = 0; /* source == 0 */
		midiDisp9[0] = 1;
		msgProc4[0x54] = 1;
		g_resetAllJumpCatchCalls = 0;
		g_pushUnsolicitedMessageCalls = 0;
		for (unsigned int off = 0x29138; off <= 0x291f4; off += 4)
			*(unsigned int *)(panel + off) = 0xdeadbeef;

		g->CompletePerformanceChange();
		check_eq("busy flag cleared", buf[0x2975184], 0);
		check_eq("no message sent", (unsigned int)g_pushUnsolicitedMessageCalls, 0u);
		check_eq("MidiDispatcher+0x0 cleared", midiDisp9[0], 0);
		check_eq("MessageProcessor+0x54 cleared", msgProc4[0x54], 0);
		check_eq("ResetAllJumpCatch called", (unsigned int)g_resetAllJumpCatchCalls, 1u);
		check_eq("front panel status range zeroed", *(unsigned int *)(panel + 0x29138), 0u);
		check_eq("  ...last dword too", *(unsigned int *)(panel + 0x291f4), 0u);

		printf("  -- source in {1,2}, slotA.tag != 0: 0x18-byte message --\n");
		buf[0x2975184] = 1;
		*(unsigned int *)(buf + 0x297515c) = 1; /* source == 1 */
		buf[0x297514c] = 1; /* slotA.tag != 0 */
		*(unsigned int *)(buf + 0x2975154) = 0x11; /* value1 */
		*(unsigned int *)(buf + 0x2975158) = 0x22; /* value2 */
		g_pushUnsolicitedMessageCalls = 0;
		g->CompletePerformanceChange();
		check_eq("push called", (unsigned int)g_pushUnsolicitedMessageCalls, 1u);
		check_eq("  ...msg tag/size == 0x18", *(unsigned short *)(g_lastUnsolicitedMessage + 0x0), 0x18);
		check_eq("  ...msg+0x8 == 0x1f", *(unsigned int *)(g_lastUnsolicitedMessage + 0x8), 0x1fu);
		check_eq("  ...msg+0xc == value1", *(unsigned int *)(g_lastUnsolicitedMessage + 0xc), 0x11u);
		check_eq("  ...msg+0x10 == value2", *(unsigned int *)(g_lastUnsolicitedMessage + 0x10), 0x22u);
		check_eq("  ...msg+0x14 == 3", *(unsigned int *)(g_lastUnsolicitedMessage + 0x14), 3u);

		printf("  -- source in {1,2}, slotA.tag == 0: 0x1c-byte message --\n");
		buf[0x2975184] = 1;
		*(unsigned int *)(buf + 0x297515c) = 2; /* source == 2 */
		buf[0x297514c] = 0; /* slotA.tag == 0 */
		*(unsigned int *)(buf + 0x2975150) = 1; /* mode -> m=0 (<=1), table lookup */
		*(unsigned int *)(buf + 0x64) = 0x77; /* table[0x64+0] */
		*(unsigned int *)(buf + 0x2975154) = 0x33; /* value1 */
		*(unsigned int *)(buf + 0x2975158) = 0x44; /* value2 */
		g_pushUnsolicitedMessageCalls = 0;
		g->CompletePerformanceChange();
		check_eq("push called", (unsigned int)g_pushUnsolicitedMessageCalls, 1u);
		check_eq("  ...msg tag/size == 0x1c", *(unsigned short *)(g_lastUnsolicitedMessage + 0x0), 0x1c);
		check_eq("  ...msg+0xc == resolved table type", *(unsigned int *)(g_lastUnsolicitedMessage + 0xc), 0x77u);
		check_eq("  ...msg+0x10 == value1", *(unsigned int *)(g_lastUnsolicitedMessage + 0x10), 0x33u);
		check_eq("  ...msg+0x14 == value2", *(unsigned int *)(g_lastUnsolicitedMessage + 0x14), 0x44u);
		check_eq("  ...msg+0x18 == 3", *(unsigned int *)(g_lastUnsolicitedMessage + 0x18), 3u);

		munmap(midiDisp9, 0x10);
		munmap(msgProc4, 0x1040);
		munmap(ctrlRT4, 0x30);
		munmap(panel, 0x29200);
		/* mgr46 deliberately NOT munmap'd -- see [17]'s own identical
		 * comment on why CSTGPerformanceVarsManager::sInstance must not
		 * be left dangling. */
	}

	printf("\n[47] ProcessPerformanceChange helper methods (sec 10.110)\n");
	{
		printf("  -- CSetListSlot::BeginActivation --\n");
		unsigned char *mgr6 = mmap32(0x24000);
		memset(mgr6, 0, 0x24000);
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgr6;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 4) = (unsigned int)(unsigned long)mgr6;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 8) = 0;
		unsigned char *slotBuf = mmap32(0x10);
		memset(slotBuf, 0, 0x10);
		slotBuf[0x8] = 0x42;
		CSetListSlot *slot = (CSetListSlot *)slotBuf;
		slot->BeginActivation();
		check_eq("mgr+0x23ec set from slot+0x8", mgr6[0x23ec], 0x42);
		munmap(slotBuf, 0x10);
		munmap(mgr6, 0x24000);

		printf("  -- CSTGProgramSlot::GetProperMidiChannel --\n");
		unsigned char *progSlotBuf = mmap32(0x20);
		memset(progSlotBuf, 0, 0x20);
		CSTGProgramSlot *progSlot = (CSTGProgramSlot *)progSlotBuf;
		progSlotBuf[0x10] = 5;
		check_eq("non-sentinel channel returned directly", progSlot->GetProperMidiChannel(), 5);

		unsigned char *globalBuf = mmap32(0x10);
		memset(globalBuf, 0, 0x10);
		globalBuf[0x6b8] = 9;
		CSTGGlobal::sInstance = (CSTGGlobal *)globalBuf;
		progSlotBuf[0x10] = 0x10;
		check_eq("sentinel 0x10 -> falls back to CSTGGlobal+0x6b8", progSlot->GetProperMidiChannel(), 9);
		munmap(progSlotBuf, 0x20);
		munmap(globalBuf, 0x10);

		printf("  -- CSTGPerformanceVars::BeginActivation --\n");
		unsigned char *pvBuf = mmap32(0x2400);
		memset(pvBuf, 0, 0x2400);
		CSTGPerformanceVars *pv = (CSTGPerformanceVars *)pvBuf;
		/* +0x23d8 is a packed 32-bit pointer field in the real binary
		 * -- perfObj must itself be 32-bit-representable (mmap32) so a
		 * real (not host-truncated) round-trip can be verified. */
		unsigned char *perfBuf = mmap32(0x10);
		CSTGPerformance *perfObj = (CSTGPerformance *)perfBuf;
		g_enterActivatingStateCalls = 0;
		pvBuf[0x23d1] = 0; /* inactive -> EnterActivatingState fires */
		pv->BeginActivation(perfObj, true);
		check_eq("+0x23ec zeroed", pvBuf[0x23ec], 0);
		check_eq("+0x23d8 stores perf pointer (packed 32-bit)",
			 *(unsigned int *)(pvBuf + 0x23d8), (unsigned int)(unsigned long)perfObj);
		check_eq("+0x23dd stores flag", pvBuf[0x23dd], 1);
		check_eq("EnterActivatingState called (was inactive)", (unsigned int)g_enterActivatingStateCalls, 1u);

		pvBuf[0x23d1] = 1; /* already active -> no EnterActivatingState call */
		g_enterActivatingStateCalls = 0;
		pv->BeginActivation(perfObj, false);
		check_eq("EnterActivatingState NOT called (already active)",
			 (unsigned int)g_enterActivatingStateCalls, 0u);
		munmap(perfBuf, 0x10);
		munmap(pvBuf, 0x2400);

		printf("  -- USTGAliasBankTypes::GetAliasPgmBankMapping --\n");
		memset(STGAliasToRealPgmBank, 0, sizeof(STGAliasToRealPgmBank));
		memset(STGAliasBankPgmMap, 0, sizeof(STGAliasBankPgmMap));
		STGAliasToRealPgmBank[3 * 128 + 5] = 7;
		STGAliasBankPgmMap[3 * 128 + 5] = 5;
		int outBankId = -1;
		unsigned int outIndex = 0xdead;
		USTGAliasBankTypes::GetAliasPgmBankMapping(3, 5, outBankId, outIndex);
		check_eq("outBankId from STGAliasToRealPgmBank", (unsigned int)outBankId, 7u);
		check_eq("outIndex from STGAliasBankPgmMap", outIndex, 5u);

		outBankId = -1;
		outIndex = 0xdead;
		USTGAliasBankTypes::GetAliasPgmBankMapping(3, 0xfffe, outBankId, outIndex);
		check_eq("index==0xfffe sentinel -> outBankId=0", (unsigned int)outBankId, 0u);
		check_eq("  ...outIndex=0xfffe", outIndex, 0xfffeu);

		printf("  -- CSTGPerformanceVars::FreeVoicelessDyingSlots --\n");
		unsigned char *globalBuf2 = buf;
		memset(globalBuf2, 0, globalSize);
		CSTGGlobal::sInstance = (CSTGGlobal *)globalBuf2;
		unsigned char *lb = mmap32(0x10);
		CLoadBalancer::sInstance = (CLoadBalancer *)lb;

		unsigned char *pvBuf2 = mmap32(0x2400);
		memset(pvBuf2, 0, 0x2400);
		CSTGPerformanceVars *pv2 = (CSTGPerformanceVars *)pvBuf2;
		pvBuf2[0x23d0] = 5; /* group id */

		unsigned char *nodeA = mmap32(0x10), *nodeB = mmap32(0x10), *nodeC = mmap32(0x10);
		unsigned char *vdA = mmap32(0x2900), *vdB = mmap32(0x2900), *vdC = mmap32(0x2900);
		memset(vdA, 0, 0x2900); memset(vdB, 0, 0x2900); memset(vdC, 0, 0x2900);
		vdA[0x28c8] = 5; *(unsigned short *)(vdA + 0x4c) = 0; *(unsigned short *)(vdA + 0x58) = 0; /* voiceless, matching group -> freed */
		vdB[0x28c8] = 5; *(unsigned short *)(vdB + 0x4c) = 1; *(unsigned short *)(vdB + 0x58) = 0; /* has voices -> skipped */
		vdC[0x28c8] = 7; /* different group -> skipped */

		*(unsigned int *)(nodeA + 0x0) = (unsigned int)(unsigned long)nodeB;
		*(unsigned int *)(nodeA + 0x8) = (unsigned int)(unsigned long)vdA;
		*(unsigned int *)(nodeB + 0x0) = (unsigned int)(unsigned long)nodeC;
		*(unsigned int *)(nodeB + 0x8) = (unsigned int)(unsigned long)vdB;
		*(unsigned int *)(nodeC + 0x0) = 0;
		*(unsigned int *)(nodeC + 0x8) = (unsigned int)(unsigned long)vdC;
		*(unsigned int *)(globalBuf2 + 0x29c9900) = (unsigned int)(unsigned long)nodeA;

		printf("     -- +0x23d1 <= 2 -> no-op --\n");
		pvBuf2[0x23d1] = 2;
		g_freeSlotVoiceDataCalls = 0;
		g_balanceStaticLoadCalls = 0;
		pv2->FreeVoicelessDyingSlots();
		check_eq("no-op: FreeSlotVoiceData not called", (unsigned int)g_freeSlotVoiceDataCalls, 0u);
		check_eq("no-op: BalanceStaticLoad not called", (unsigned int)g_balanceStaticLoadCalls, 0u);

		printf("     -- active, walks list, frees only the voiceless matching-group node --\n");
		pvBuf2[0x23d1] = 3;
		void *lastFreedThis = 0;
		g_freeSlotVoiceDataHook = 0;
		g_freeSlotVoiceDataCalls = 0;
		g_balanceStaticLoadCalls = 0;
		pv2->FreeVoicelessDyingSlots();
		check_eq("exactly one FreeSlotVoiceData call", (unsigned int)g_freeSlotVoiceDataCalls, 1u);
		check_eq("  ...with flag false", (unsigned int)g_lastFreeSlotVoiceDataFlag, 0u);
		check_eq("BalanceStaticLoad called (freed something)", (unsigned int)g_balanceStaticLoadCalls, 1u);
		(void)lastFreedThis;

		printf("     -- nothing voiceless/matching -> no BalanceStaticLoad call --\n");
		vdA[0x4c] = 1; /* vdA no longer voiceless */
		g_freeSlotVoiceDataCalls = 0;
		g_balanceStaticLoadCalls = 0;
		pv2->FreeVoicelessDyingSlots();
		check_eq("no matches -> FreeSlotVoiceData not called", (unsigned int)g_freeSlotVoiceDataCalls, 0u);
		check_eq("no matches -> BalanceStaticLoad not called", (unsigned int)g_balanceStaticLoadCalls, 0u);

		munmap(nodeA, 0x10); munmap(nodeB, 0x10); munmap(nodeC, 0x10);
		munmap(vdA, 0x2900); munmap(vdB, 0x2900); munmap(vdC, 0x2900);
		munmap(pvBuf2, 0x2400);
		munmap(lb, 0x10);

		printf("  -- CSTGPerformanceVarsManager::AllocPerformanceVars --\n");
		unsigned char *panelBuf2 = mmap32(0x1098);
		memset(panelBuf2, 0, 0x1098);
		STGAPIFrontPanelStatus::sInstance = panelBuf2;

		unsigned char *slot0 = mmap32(0x2410);
		unsigned char *slot1 = mmap32(0x2410);
		memset(slot0, 0, 0x2410);
		memset(slot1, 0, 0x2410);
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)slot0;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 4) = (unsigned int)(unsigned long)slot1;
		CSTGPerformanceVarsManager::sInstance[8] = 0; /* currently selects slot0 -> toggle picks slot1 */

		CSTGPerformanceVars *result = ((CSTGPerformanceVarsManager *)CSTGPerformanceVarsManager::sInstance)->AllocPerformanceVars();
		check_eq("selector toggled 0->1", CSTGPerformanceVarsManager::sInstance[8], 1);
		check_eq("state==0 -> returns slot1 as-is", (unsigned int)((unsigned char *)result == slot1), 1u);
		check_eq("  ...no +0x23d8 write", *(unsigned int *)(slot1 + 0x23d8), 0u);

		printf("     -- state==5: recompute envelope, take the larger value --\n");
		CSTGPerformanceVarsManager::sInstance[8] = 0; /* toggles back to slot1 again */
		slot1[0x23d1] = 5;
		*(float *)(slot1 + 0x23fc) = 72.0f; /* 72/36 = 2.0, larger than current 0.5 */
		*(float *)(slot1 + 0x2400) = 0.5f;
		result = ((CSTGPerformanceVarsManager *)CSTGPerformanceVarsManager::sInstance)->AllocPerformanceVars();
		check_eq("state==5: +0x2400 updated to the larger ratio",
			 (unsigned int)(*(float *)(slot1 + 0x2400) == 2.0f), 1u);
		check_eq("  ...+0x23fc left untouched", (unsigned int)(*(float *)(slot1 + 0x23fc) == 72.0f), 1u);

		printf("     -- state==5: current value already larger -> unchanged --\n");
		CSTGPerformanceVarsManager::sInstance[8] = 0;
		slot1[0x23d1] = 5;
		*(float *)(slot1 + 0x23fc) = 36.0f; /* 36/36 = 1.0, smaller than current 5.0 */
		*(float *)(slot1 + 0x2400) = 5.0f;
		((CSTGPerformanceVarsManager *)CSTGPerformanceVarsManager::sInstance)->AllocPerformanceVars();
		check_eq("state==5: +0x2400 stays unchanged when not larger",
			 (unsigned int)(*(float *)(slot1 + 0x2400) == 5.0f), 1u);

		printf("     -- state==2 (>1): forcibly reset, no count-block update --\n");
		CSTGPerformanceVarsManager::sInstance[8] = 0;
		slot1[0x23d1] = 2;
		*(unsigned int *)(panelBuf2 + 0x1094) = 0xdead;
		((CSTGPerformanceVarsManager *)CSTGPerformanceVarsManager::sInstance)->AllocPerformanceVars();
		check_eq("state==2: +0x23d1 forced to 5", slot1[0x23d1], 5);
		check_eq("  ...+0x23fc reset to 1.0", (unsigned int)(*(float *)(slot1 + 0x23fc) == 1.0f), 1u);
		check_eq("  ...+0x2400 reset to 1/36", (unsigned int)(*(float *)(slot1 + 0x2400) == 1.0f / 36.0f), 1u);
		check_eq("  ...+0x23d8 zeroed", *(unsigned int *)(slot1 + 0x23d8), 0u);
		check_eq("  ...count-block NOT run (state>1)", *(unsigned int *)(panelBuf2 + 0x1094), 0xdeadu);

		printf("     -- state==1 (<=1): forcibly reset, count-block DOES run --\n");
		CSTGPerformanceVarsManager::sInstance[8] = 0;
		slot1[0x23d1] = 1;
		slot0[0x23d1] = 3; /* >1 -> counts */
		*(unsigned int *)(panelBuf2 + 0x1094) = 0xdead;
		g_pushUnsolicitedMessageCalls = 0;
		((CSTGPerformanceVarsManager *)CSTGPerformanceVarsManager::sInstance)->AllocPerformanceVars();
		check_eq("state==1: +0x23d1 forced to 5", slot1[0x23d1], 5);
		check_eq("  ...count-block DOES run: slot0(>1)+slot1(now 5>1) == 2",
			 *(unsigned int *)(panelBuf2 + 0x1094), 2u);
		check_eq("  ...PushUnsolicitedMessage still never called (dead code)",
			 (unsigned int)g_pushUnsolicitedMessageCalls, 0u);
		check_eq("  ...+0x23fc/+0x2400 still reset", (unsigned int)(*(float *)(slot1 + 0x23fc) == 1.0f), 1u);

		munmap(slot0, 0x2410);
		munmap(slot1, 0x2410);
		munmap(panelBuf2, 0x1098);
	}

	printf("\n[48] ProcessPerformanceChange (sec 10.113)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;

		unsigned char *mgrA = mmap32(0x24000);
		unsigned char *mgrB = mmap32(0x24000);
		unsigned char *ctrlRT5 = mmap32(0x30);
		unsigned char *midiDisp10 = mmap32(0x10);
		unsigned char *panelBuf3 = mmap32(0x1098);
		unsigned char *smoother4 = mmap32(0xf020);
		CSTGControllerRTData::sInstance = (CSTGControllerRTData *)ctrlRT5;
		CSTGMidiDispatcher::sInstance = (CSTGMidiDispatcher *)midiDisp10;
		STGAPIFrontPanelStatus::sInstance = panelBuf3;
		CSTGSmoother::sInstance = (CSTGSmoother *)smoother4; /* PerfChangeControllerReset's
								       * own CancelAllCCSmoothers call */
		CSTGGlobal::sInstance = g; /* FreeVoicelessDyingSlots/StealAllDyingPerformanceVars
					    * both read CSTGGlobal::sInstance+0x29c9900 */

		auto resetAll = [&]() {
			memset(buf, 0, globalSize);
			memset(mgrA, 0, 0x24000);
			memset(mgrB, 0, 0x24000);
			memset(ctrlRT5, 0, 0x30);
			memset(panelBuf3, 0, 0x1098);
			memset(smoother4, 0, 0xf020); /* CancelAllCCSmoothers (sec 10.130) reads
						       * +0xf010 unconditionally -- keep the list
						       * pointer null (empty list, no real nodes
						       * to traverse) for these scenarios. */
			*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgrA;
			*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 4) = (unsigned int)(unsigned long)mgrB;
			CSTGPerformanceVarsManager::sInstance[8] = 0;
			mgrA[0x23d1] = 9; /* >2: observable if StealAllDyingPerformanceVars runs
					   * (AllocPerformanceVars toggles sInstance[8] to 1, so
					   * StealAllDyingPerformanceVars, if called, always
					   * targets mgrA -- (1+1)&1==0). */
			mgrB[0x23d1] = 0; /* AllocPerformanceVars returns as-is, BeginActivation's own EnterActivatingState fires */
			g_writeMidiOutQueueCalls = 0;
			g_enterActivatingStateCalls = 0;
		};

		printf("  -- tag!=0, recordType>2 (category=0 default), source>1 --\n");
		resetAll();
		buf[0x297514c] = 1; /* slotA.tag != 0 */
		*(unsigned int *)(buf + 0x2975154) = 5; /* value1 */
		*(unsigned int *)(buf + 0x2975158) = 3; /* value2 */
		*(unsigned int *)(buf + 0x297515c) = 2; /* source > 1 */
		buf[0x6d5] = 1; /* enable midi out (to prove it's NOT used for source>1) */
		buf[0x2936084] = 9; /* recordType > 2 -> category defaults to 0 */
		buf[0x2936085] = 2; /* record[0x11] = val1 (alias bank id) */
		buf[0x2936086] = 10; /* record[0x12] = val2 (alias index) */
		buf[0x293608c] = 0x77; /* CSetListSlot's own +0x8 marker */
		STGAliasToRealPgmBank[2 * 128 + 10] = 4;
		STGAliasBankPgmMap[2 * 128 + 10] = 55;

		g->ProcessPerformanceChange();
		check_eq("+0x2975184 busy flag set to 2", buf[0x2975184], 2);
		check_eq("+0x6a5 = raw value1", buf[0x6a5], 5);
		check_eq("+0x6a6 = raw value2", buf[0x6a6], 3);
		check_eq("category=0: +0x684 mode = 0", *(unsigned int *)(buf + 0x684), 0u);
		check_eq("  ...+0x688 = val1", *(unsigned int *)(buf + 0x688), 2u);
		check_eq("  ...+0x694 = val2", *(unsigned int *)(buf + 0x694), 10u);
		check_eq("  ...+0x68c = converted bank", *(unsigned int *)(buf + 0x68c), 4u);
		check_eq("  ...+0x698 = converted index", *(unsigned int *)(buf + 0x698), 55u);
		check_eq("category!=2: +0x6b9 copied from +0x6b8", buf[0x6b9], buf[0x6b8]);
		check_eq("source>1: PerfChangeControllerReset ran (its own real per-channel panel fill)",
			 panelBuf3[2 * 0x80 + 0xb], 0xff);
		check_eq("source>1: SendPerfChangeToMidiOut NOT invoked",
			 (unsigned int)g_writeMidiOutQueueCalls, 0u);
		check_eq("val2(55)!=0xfffe: StealAllDyingPerformanceVars NOT invoked (mgrA untouched)",
			 mgrA[0x23d1], 9);
		check_eq("  ...+0x240c stays clear too", mgrA[0x240c], 0);
		check_eq("newPerf(mgrB)->BeginActivation ran: EnterActivatingState fired",
			 (unsigned int)g_enterActivatingStateCalls, 1u);
		check_eq("  ...+0x23dd flag == (+0x6a4!=0) == true", mgrB[0x23dd], 1);
		check_eq("+0x6a4!=0: CSetListSlot::BeginActivation ran -> mgrB[0x23ec] set",
			 mgrB[0x23ec], 0x77);

		printf("  -- source<=1: SendPerfChangeToMidiOut ALSO called --\n");
		resetAll();
		buf[0x297514c] = 1;
		*(unsigned int *)(buf + 0x2975154) = 5;
		*(unsigned int *)(buf + 0x2975158) = 3;
		*(unsigned int *)(buf + 0x297515c) = 0; /* source <= 1 */
		buf[0x6d5] = 1;
		buf[0x2936084] = 9;
		buf[0x2936085] = 2;
		buf[0x2936086] = 10;
		STGAliasToRealPgmBank[2 * 128 + 10] = 4;
		STGAliasBankPgmMap[2 * 128 + 10] = 55;

		g->ProcessPerformanceChange();
		check_eq("source<=1: SendPerfChangeToMidiOut invoked (writes to midi out queue)",
			 (unsigned int)(g_writeMidiOutQueueCalls > 0), 1u);
		check_eq("source<=1: PerfChangeControllerReset still ran (real panel fill again)",
			 panelBuf3[2 * 0x80 + 0xb], 0xff);

		printf("  -- val2==0xfffe: StealAllDyingPerformanceVars called --\n");
		/* record[0x12] is only ever read as a single BYTE in the
		 * tag!=0 path (0-255), so it can never literally equal the
		 * 16-bit sentinel 0xfffe there -- only the tag==0 path's own
		 * full 32-bit slotA.value2 can trigger this gate. */
		resetAll();
		buf[0x297514c] = 0; /* slotA.tag == 0 */
		*(unsigned int *)(buf + 0x2975150) = 0; /* mode -> category 0 */
		*(unsigned int *)(buf + 0x2975154) = 2; /* val1 (alias bank id) */
		*(unsigned int *)(buf + 0x2975158) = 0xfffe; /* val2 -- the sentinel itself */
		*(unsigned int *)(buf + 0x297515c) = 2;

		g->ProcessPerformanceChange();
		check_eq("val2==0xfffe: StealAllDyingPerformanceVars ran on mgrA (state 9!=5 -> forced to 5)",
			 mgrA[0x23d1], 5);
		check_eq("  ...+0x23fc/+0x2400 both reset to 1.0f (no /36.0f, unlike AllocPerformanceVars)",
			 (unsigned int)(*(float *)(mgrA + 0x23fc) == 1.0f && *(float *)(mgrA + 0x2400) == 1.0f), 1u);
		check_eq("  ...+0x240c set (not cleared)", mgrA[0x240c], 1);
		check_eq("  ...GetAliasPgmBankMapping's own sentinel path -> +0x698==0xfffe",
			 (unsigned int)(*(unsigned int *)(buf + 0x698) == 0xfffe), 1u);

		printf("  -- tag!=0, recordType<=2 -> category from +0x58 table (category=1) --\n");
		resetAll();
		buf[0x297514c] = 1;
		*(unsigned int *)(buf + 0x2975154) = 5;
		*(unsigned int *)(buf + 0x2975158) = 3;
		*(unsigned int *)(buf + 0x297515c) = 2;
		buf[0x2936084] = 1; /* recordType == 1 -> table lookup at +0x58+1*4 */
		buf[0x2936085] = 0x11; /* record[0x11] = val1 */
		buf[0x2936086] = 0x22; /* record[0x12] = val2 */
		*(unsigned int *)(buf + 0x58 + 1 * 4) = 1; /* category table -> category==1 */

		g->ProcessPerformanceChange();
		check_eq("category=1: +0x684 == 1", *(unsigned int *)(buf + 0x684), 1u);
		check_eq("  ...+0x690 = val1", *(unsigned int *)(buf + 0x690), 0x11u);
		check_eq("  ...+0x69c = val2", *(unsigned int *)(buf + 0x69c), 0x22u);
		check_eq("category!=2: +0x6b9 copied from +0x6b8 (no GetProperMidiChannel)",
			 buf[0x6b9], buf[0x6b8]);

		printf("  -- tag==0 path: category from mode directly, +0x6a4==0 --\n");
		resetAll();
		buf[0x297514c] = 0; /* slotA.tag == 0 */
		*(unsigned int *)(buf + 0x2975150) = 1; /* mode -> category */
		*(unsigned int *)(buf + 0x2975154) = 0x33; /* val1 */
		*(unsigned int *)(buf + 0x2975158) = 0x44; /* val2 */
		*(unsigned int *)(buf + 0x297515c) = 2;

		g->ProcessPerformanceChange();
		check_eq("tag==0: +0x6a4 == 0", buf[0x6a4], 0);
		check_eq("  ...category(mode)=1: +0x684 == 1", *(unsigned int *)(buf + 0x684), 1u);
		check_eq("  ...+0x690 = val1", *(unsigned int *)(buf + 0x690), 0x33u);
		check_eq("  ...+0x69c = val2", *(unsigned int *)(buf + 0x69c), 0x44u);
		check_eq("  ...+0x6a4==0: CSetListSlot::BeginActivation NOT called (mgrB[0x23ec] untouched)",
			 mgrB[0x23ec], 0);

		printf("  -- category not in {0,1,2}: all three field-write blocks skipped --\n");
		resetAll();
		buf[0x297514c] = 1;
		*(unsigned int *)(buf + 0x2975154) = 5;
		*(unsigned int *)(buf + 0x2975158) = 3;
		*(unsigned int *)(buf + 0x297515c) = 2;
		buf[0x2936084] = 2; /* recordType == 2 -> table lookup at +0x58+2*4 */
		*(unsigned int *)(buf + 0x58 + 2 * 4) = 9; /* category table gives an out-of-range value */
		*(unsigned int *)(buf + 0x684) = 0xdeadbeef; /* stale sentinel, must survive untouched */

		g->ProcessPerformanceChange();
		check_eq("category not in {0,1,2}: +0x684 left at its stale value",
			 *(unsigned int *)(buf + 0x684), 0xdeadbeefu);

		munmap(mgrA, 0x24000);
		munmap(mgrB, 0x24000);
		munmap(ctrlRT5, 0x30);
		munmap(midiDisp10, 0x10);
		munmap(panelBuf3, 0x1098);
		munmap(smoother4, 0xf020);
	}

	printf("\n[49] CSTGPerformanceVarsManager::StealAllDyingPerformanceVars (sec 10.114)\n");
	{
		unsigned char *mgr0 = mmap32(0x2410);
		unsigned char *mgr1 = mmap32(0x2410);
		unsigned char *midiDisp11 = mmap32(0x10);
		CSTGMidiDispatcher::sInstance = (CSTGMidiDispatcher *)midiDisp11;
		unsigned char *globalBuf3 = buf;
		memset(globalBuf3, 0, globalSize);
		CSTGGlobal::sInstance = (CSTGGlobal *)globalBuf3;

		auto reset = [&]() {
			memset(mgr0, 0, 0x2410);
			memset(mgr1, 0, 0x2410);
			memset(globalBuf3, 0, globalSize);
			*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgr0;
			*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 4) = (unsigned int)(unsigned long)mgr1;
			/* StealingRequiresOneTickStall (sec 10.136) is now real:
			 * writes CSTGGlobal::sInstance->fieldAt(0x29c9fa8)+1 into
			 * midiDisp11->fieldAt(0xa4) -- set a known sentinel so
			 * "called" vs "not called" is directly observable. */
			*(unsigned int *)(globalBuf3 + 0x29c9fa8) = 0x41414141;
			*(unsigned int *)(midiDisp11 + 0xa4) = 0;
		};

		printf("  -- selector==0: processes slot[1] only, slot[0] untouched --\n");
		reset();
		CSTGPerformanceVarsManager::sInstance[8] = 0;
		mgr0[0x23d1] = 9;
		mgr1[0x23d1] = 9;
		((CSTGPerformanceVarsManager *)CSTGPerformanceVarsManager::sInstance)->StealAllDyingPerformanceVars();
		check_eq("slot[1] (the OTHER slot) got processed: forced to state 5", mgr1[0x23d1], 5);
		check_eq("slot[0] (the SELECTED slot) left completely untouched", mgr0[0x23d1], 9);
		check_eq("StealingRequiresOneTickStall called once (real side effect: +0xa4 == global counter + 1)",
			 *(unsigned int *)(midiDisp11 + 0xa4), 0x41414142u);

		printf("  -- state<=2 (including negative): no-op --\n");
		reset();
		CSTGPerformanceVarsManager::sInstance[8] = 0;
		mgr1[0x23d1] = 2;
		((CSTGPerformanceVarsManager *)CSTGPerformanceVarsManager::sInstance)->StealAllDyingPerformanceVars();
		check_eq("state==2 -> no-op", mgr1[0x23d1], 2);
		check_eq("  ...StealingRequiresOneTickStall not called", *(unsigned int *)(midiDisp11 + 0xa4), 0u);

		reset();
		CSTGPerformanceVarsManager::sInstance[8] = 0;
		mgr1[0x23d1] = (unsigned char)-5; /* negative signed state */
		((CSTGPerformanceVarsManager *)CSTGPerformanceVarsManager::sInstance)->StealAllDyingPerformanceVars();
		check_eq("negative state -> also no-op", mgr1[0x23d1], (unsigned char)-5);

		printf("  -- active list: marks +0x42=1 on matching-group voice data --\n");
		reset();
		CSTGPerformanceVarsManager::sInstance[8] = 0;
		mgr1[0x23d1] = 9;
		mgr1[0x23d0] = 6; /* group id */
		unsigned char *nA = mmap32(0x10), *nB = mmap32(0x10);
		unsigned char *vA = mmap32(0x2900), *vB = mmap32(0x2900);
		memset(vA, 0, 0x2900); memset(vB, 0, 0x2900);
		vA[0x28c8] = 6; /* matches group */
		vB[0x28c8] = 7; /* does not match */
		*(unsigned int *)(nA + 0x0) = (unsigned int)(unsigned long)nB;
		*(unsigned int *)(nA + 0x8) = (unsigned int)(unsigned long)vA;
		*(unsigned int *)(nB + 0x0) = 0;
		*(unsigned int *)(nB + 0x8) = (unsigned int)(unsigned long)vB;
		*(unsigned int *)(globalBuf3 + 0x29c9900) = (unsigned int)(unsigned long)nA;
		((CSTGPerformanceVarsManager *)CSTGPerformanceVarsManager::sInstance)->StealAllDyingPerformanceVars();
		check_eq("matching-group voice data marked +0x42=1", vA[0x42], 1);
		check_eq("non-matching voice data left untouched", vB[0x42], 0);
		munmap(nA, 0x10); munmap(nB, 0x10); munmap(vA, 0x2900); munmap(vB, 0x2900);

		printf("  -- state==5: takes the larger of +0x23fc/+0x2400 directly (no /36.0f) --\n");
		reset();
		CSTGPerformanceVarsManager::sInstance[8] = 0;
		mgr1[0x23d1] = 5;
		*(float *)(mgr1 + 0x23fc) = 9.0f;
		*(float *)(mgr1 + 0x2400) = 2.0f;
		((CSTGPerformanceVarsManager *)CSTGPerformanceVarsManager::sInstance)->StealAllDyingPerformanceVars();
		check_eq("+0x23fc(9.0) > +0x2400(2.0) -> +0x2400 updated to 9.0",
			 (unsigned int)(*(float *)(mgr1 + 0x2400) == 9.0f), 1u);
		check_eq("  ...+0x23d1 still 5 (unchanged)", mgr1[0x23d1], 5);

		reset();
		CSTGPerformanceVarsManager::sInstance[8] = 0;
		mgr1[0x23d1] = 5;
		*(float *)(mgr1 + 0x23fc) = 1.0f;
		*(float *)(mgr1 + 0x2400) = 8.0f;
		((CSTGPerformanceVarsManager *)CSTGPerformanceVarsManager::sInstance)->StealAllDyingPerformanceVars();
		check_eq("+0x23fc(1.0) <= +0x2400(8.0) -> +0x2400 unchanged",
			 (unsigned int)(*(float *)(mgr1 + 0x2400) == 8.0f), 1u);

		printf("  -- state!=5 (>2): forced to 5, both floats reset to 1.0f, +0x240c set --\n");
		reset();
		CSTGPerformanceVarsManager::sInstance[8] = 0;
		mgr1[0x23d1] = 3;
		*(float *)(mgr1 + 0x23fc) = 42.0f;
		*(float *)(mgr1 + 0x2400) = 42.0f;
		((CSTGPerformanceVarsManager *)CSTGPerformanceVarsManager::sInstance)->StealAllDyingPerformanceVars();
		check_eq("state forced to 5", mgr1[0x23d1], 5);
		check_eq("  ...+0x23fc reset to 1.0f", (unsigned int)(*(float *)(mgr1 + 0x23fc) == 1.0f), 1u);
		check_eq("  ...+0x2400 reset to 1.0f (not 1/36 like AllocPerformanceVars)",
			 (unsigned int)(*(float *)(mgr1 + 0x2400) == 1.0f), 1u);
		check_eq("  ...+0x23d8 zeroed", *(unsigned int *)(mgr1 + 0x23d8), 0u);
		check_eq("  ...+0x240c set to 1", mgr1[0x240c], 1);

		munmap(mgr0, 0x2410);
		munmap(mgr1, 0x2410);
		munmap(midiDisp11, 0x10);
	}

	printf("\n[50] CSTGMidiDispatcher::PerfChangeControllerReset (sec 10.115)\n");
	{
		unsigned char *midiDisp12 = mmap32(0x100);
		unsigned char *mgrC = mmap32(0xc000);
		unsigned char *smoother5 = mmap32(0xf020);
		unsigned char *panelBuf4 = mmap32(0x1000);
		memset(midiDisp12, 0, 0x100);
		memset(mgrC, 0, 0xc000);
		memset(panelBuf4, 0, 0x1000);
		memset(smoother5, 0, 0xf020); /* CancelAllCCSmoothers (sec 10.130) reads
					       * +0xf010 unconditionally -- empty list. */
		CSTGMidiDispatcher *disp = (CSTGMidiDispatcher *)midiDisp12;
		CSTGSmoother::sInstance = (CSTGSmoother *)smoother5;
		STGAPIFrontPanelStatus::sInstance = panelBuf4;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgrC;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 4) = (unsigned int)(unsigned long)mgrC;
		CSTGPerformanceVarsManager::sInstance[8] = 0;

		/* Channel 0's own valueSlot (mgrC+0x29b0) -- pre-populated with
		 * an existing CSTGControllerValue, confirmed to be re-applied
		 * as-is (not recomputed) via SetPitchBend. */
		CSTGControllerValue existingPitchBend;
		existingPitchBend.field0 = 0x3f000000; /* arbitrary bit pattern */
		existingPitchBend.field4 = 0.25f;
		existingPitchBend.field8 = 0x1234;
		existingPitchBend.fieldA = 7;
		existingPitchBend.fieldB = 8;
		*(CSTGControllerValue *)(mgrC + 0x29b0) = existingPitchBend;

		/* Channel 0's mVal(<=0x40)/lVal(>0x40) controller bytes. */
		mgrC[0x2718] = 0x20; /* mVal <= 0x40 */
		mgrC[0x2730] = 0x60; /* lVal > 0x40 */

		/* Channel 15's own bytes, to confirm the loop reaches the last
		 * iteration too. */
		unsigned char *chan15 = mgrC + 15 * 0x92c;
		chan15[0x2718] = 0xff; /* sentinel */
		chan15[0x2730] = 0x50; /* >0x40 */

		g_channelValuesResetCalls = 0;
		g_setControllerValueCalls = 0;
		disp->PerfChangeControllerReset();
		/* CancelAllCCSmoothers is now real (sec 10.130) -- with
		 * smoother5's own list empty, it's a real, silent no-op;
		 * the Reset()/SetControllerValue counts below already
		 * confirm PerfChangeControllerReset's own loop ran. */
		check_eq("Reset() called once per channel (16 total)", (unsigned int)g_channelValuesResetCalls, 16);
		check_eq("SetControllerValue called twice per channel (32 total)",
			 (unsigned int)g_setControllerValueCalls, 32);

		check_eq("channel 15's Reset() ran on mgrC+15*0x92c+0x2410 (last iteration)",
			 (unsigned int)(g_lastChannelValuesResetThis == (mgrC + 15 * 0x92c + 0x2410)), 1u);

		/* Channel 0's own valueSlot (mgrC+0x29b0) was pre-populated
		 * above; SetPitchBend's own real implementation re-applies it
		 * as-is (not recomputed) to CSTGChannelValues's own
		 * +0x5a0/+0x5a4/+0x5a8/+0x634 fields, always with flag==true. */
		unsigned char *chan0Values = mgrC + 0x2410;
		check_eq("channel 0's real SetPitchBend: +0x5a0 == value.field0",
			 *(unsigned int *)(chan0Values + 0x5a0), existingPitchBend.field0);
		check_eq("  ...+0x5a4 == raw value.field4 bits",
			 *(unsigned int *)(chan0Values + 0x5a4), *(unsigned int *)&existingPitchBend.field4);
		check_eq("  ...+0x5a8 == packed field8+fieldA+fieldB dword",
			 *(unsigned int *)(chan0Values + 0x5a8), *(unsigned int *)((unsigned char *)&existingPitchBend + 8));
		check_eq("  ...+0x634 ALSO == value.field0 (flag==true)",
			 *(unsigned int *)(chan0Values + 0x634), existingPitchBend.field0);

		/* Channel 15's own valueSlot was never pre-populated (zeroed) --
		 * confirms the "re-applied as-is, not recomputed" behavior
		 * extends to the last loop iteration too. */
		unsigned char *chan15Values = mgrC + 15 * 0x92c + 0x2410;
		check_eq("channel 15's real SetPitchBend: +0x5a0 == 0 (zeroed valueSlot)",
			 *(unsigned int *)(chan15Values + 0x5a0), 0u);
		check_eq("  ...+0x634 == 0 too", *(unsigned int *)(chan15Values + 0x634), 0u);

		check_eq("this->byteAt(0x60)/(0x61) cleared (channel 0)", midiDisp12[0x60], 0);
		check_eq("  ...byteAt(0x61)", midiDisp12[0x61], 0);

		check_eq("channel 0: STGAPIFrontPanelStatus fill region set to 0xff",
			 panelBuf4[2 * 0x80 + 0xb], 0xff);
		check_eq("  ...last byte of the 120-byte fill also 0xff", panelBuf4[2 * 0x80 + 0xb + 0x77], 0xff);
		check_eq("  ...one past the fill region untouched", panelBuf4[2 * 0x80 + 0xb + 0x78], 0);

		check_eq("channel 15: mVal(0xff sentinel) stored at +ch*0x80+0x14b",
			 panelBuf4[15 * 0x80 + 0x14b], 0xff);
		check_eq("  ...lVal(0x50) stored at +ch*0x80+0x14d", panelBuf4[15 * 0x80 + 0x14d], 0x50);

		printf("  -- scaling formula, verified per-call via the full history --\n");
		/* channel 0 is calls [0] (mVal=0x20, cc=0x40) and [1] (lVal=0x60, cc=0x42). */
		check_eq("channel 0 call[0]: ccNumber == 0x40 (mVal)", g_setControllerValueCCHistory[0], 0x40);
		float expLowM = (float)(short)0x20 / 128.0f;
		float expCenterM = expLowM + expLowM - 1.0f;
		check_eq("  ...val<=0x40 (0x20): field0 == val/128.0f",
			 (unsigned int)(*(float *)&g_setControllerValueHistory[0].field0 == expLowM), 1u);
		check_eq("  ...field4 == 2*field0-1.0f",
			 (unsigned int)(g_setControllerValueHistory[0].field4 == expCenterM), 1u);
		check_eq("  ...field8 == raw mVal", g_setControllerValueHistory[0].field8, 0x20);
		check_eq("  ...fieldA == 1", g_setControllerValueHistory[0].fieldA, 1);

		check_eq("channel 0 call[1]: ccNumber == 0x42 (lVal)", g_setControllerValueCCHistory[1], 0x42);
		float expLowL = ((float)(0x60 - 1)) / 126.0f;
		float expCenterL = expLowL + expLowL - 1.0f;
		check_eq("  ...val>0x40 (0x60): field0 == (val-1)/126.0f",
			 (unsigned int)(*(float *)&g_setControllerValueHistory[1].field0 == expLowL), 1u);
		check_eq("  ...field4 == 2*field0-1.0f",
			 (unsigned int)(g_setControllerValueHistory[1].field4 == expCenterL), 1u);

		/* channel 15 is calls [30] (mVal=0xff sentinel, cc=0x40) and [31] (lVal=0x50, cc=0x42). */
		check_eq("channel 15 call[30]: val==0xff sentinel -> field0==0.0f",
			 (unsigned int)(*(float *)&g_setControllerValueHistory[30].field0 == 0.0f), 1u);
		check_eq("  ...field4==0.0f too", g_setControllerValueHistory[30].field4, 0.0f);

		munmap(midiDisp12, 0x100);
		munmap(mgrC, 0xc000);
		munmap(smoother5, 0xf020);
		munmap(panelBuf4, 0x1000);
	}

	printf("\n[51] CSTGGlobal::SetCurrentModeTempo (sec 10.117)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);
		float *out = (float *)(buf + 0x29c9fa4);

		g->SetCurrentModeTempo(120.0f);
		check_eq("tempo==120 (unity) -> log2(1.0) == 0.0", (unsigned int)(*out == 0.0f), 1u);

		g->SetCurrentModeTempo(240.0f);
		check_eq("tempo==240 (double) -> log2(2.0) == 1.0", (unsigned int)(*out == 1.0f), 1u);

		g->SetCurrentModeTempo(60.0f);
		check_eq("tempo==60 (half) -> log2(0.5) == -1.0", (unsigned int)(*out == -1.0f), 1u);

		g->SetCurrentModeTempo(30.0f);
		check_eq("tempo==30 (quarter) -> log2(0.25) == -2.0", (unsigned int)(*out == -2.0f), 1u);

		printf("  -- tempo<1.0: fixed ratio (1/120), tempo value ignored --\n");
		g->SetCurrentModeTempo(0.5f);
		float belowOneResult = *out;
		g->SetCurrentModeTempo(0.0f);
		check_eq("tempo==0.5 and tempo==0.0 give the IDENTICAL fixed result",
			 (unsigned int)(*out == belowOneResult), 1u);
		g->SetCurrentModeTempo(-5.0f);
		check_eq("tempo==-5.0 also gives the SAME fixed result (tempo value ignored)",
			 (unsigned int)(*out == belowOneResult), 1u);
		check_eq("  ...and it matches the confirmed real log2(1/120) bit pattern",
			 *(unsigned int *)&belowOneResult, 0xc0dd053fu);

		printf("  -- clamping --\n");
		g->SetCurrentModeTempo(100000000.0f); /* tempo/120 = 2^16 * ~12.7 -> clamps to 16.0 */
		check_eq("very large tempo clamps to +16.0", (unsigned int)(*out == 16.0f), 1u);

		g->SetCurrentModeTempo(7864320.0f); /* tempo/120 == exactly 2^16 -> boundary, not clamped */
		check_eq("tempo/120==2^16 exactly -> result == 16.0 (boundary, not clamp-triggered)",
			 (unsigned int)(*out == 16.0f), 1u);
	}

	printf("\n[52] CSTGGlobal::HandleController (sec 10.118)\n");
	{
		CSTGGlobal *g = (CSTGGlobal *)buf;
		memset(buf, 0, globalSize);

		unsigned char *ctrlRT6 = mmap32(0x30);
		unsigned char *midiDisp13 = mmap32(0x10);
		unsigned char *midiPortMgr6 = mmap32(0x300);
		unsigned char *msgProc2 = mmap32(0x1040);
		unsigned char *voiceAlloc = mmap32(0x10);
		CSTGControllerRTData::sInstance = (CSTGControllerRTData *)ctrlRT6;
		CSTGMidiDispatcher::sInstance = (CSTGMidiDispatcher *)midiDisp13;
		CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)midiPortMgr6;
		SetupFakeRingCtl(midiPortMgr6);
		CSTGMessageProcessor::sInstance = (CSTGMessageProcessor *)msgProc2;
		CSTGVoiceAllocator::sInstance = (CSTGVoiceAllocator *)voiceAlloc;
		/* ResetAllJumpCatch's own real body (sec 10.129) needs a valid
		 * active CSTGPerformanceVarsManager with +0x23d1 == 2. */
		unsigned char *mgr52 = mmap32(0x2400);
		memset(mgr52, 0, 0x2400);
		mgr52[0x23d1] = 2;
		*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgr52;
		CSTGPerformanceVarsManager::sInstance[8] = 0;

		CSTGControllerValue value;

		printf("  -- CC 0x5c (IFX), cl=false, doSecondary=true: bits 0+3 --\n");
		buf[0x6d4] = 0;
		memset(&value, 0, sizeof(value));
		value.fieldB = 0; /* fieldB&2==0 */
		value.fieldA = 1;  /* not 3/4/5/6 -> cl=false via fieldA!=6, doSecondary=true */
		value.field8 = 0;  /* ==0 -> primary=1 */
		g_pushUnsolicitedMessageCalls = 0;
		g->HandleController(0x5c, value);
		check_eq("field8==0 -> bits 0 and 3 both set", (unsigned int)(buf[0x6d4] & 0x09), 0x09u);
		check_eq("msg type == 4", *(unsigned int *)(g_lastUnsolicitedMessage + 0x10), 4u);
		check_eq("msg+0x14 == 1 (primary value)", *(unsigned int *)(g_lastUnsolicitedMessage + 0x14), 1u);

		value.field8 = 5; /* !=0 -> primary=0 */
		g->HandleController(0x5c, value);
		check_eq("field8!=0 -> bits 0 and 3 both cleared", (unsigned int)(buf[0x6d4] & 0x09), 0u);

		printf("  -- CC 0x5c, fieldA==4: doSecondary=false, only bit 0 --\n");
		buf[0x6d4] = 0x08; /* bit3 pre-set */
		value.fieldA = 4;
		value.field8 = 0; /* primary=1 */
		g->HandleController(0x5c, value);
		check_eq("only bit0 set, bit3 (pre-set) left untouched", buf[0x6d4], 0x09);

		printf("  -- CC 0x5c, cl=true (fieldB&2!=0): copies bit3 into bit0, ignores field8 --\n");
		buf[0x6d4] = 0x08; /* bit3 set, bit0 clear */
		value.fieldB = 2;
		value.fieldA = 1;
		value.field8 = 5; /* would give primary=0 if consulted -- must be ignored */
		g->HandleController(0x5c, value);
		check_eq("bit0 copied from bit3 (1), field8 ignored", (unsigned int)(buf[0x6d4] & 0x09), 0x09u);
		check_eq("msg+0x14 == 1 (copied bit value)", *(unsigned int *)(g_lastUnsolicitedMessage + 0x14), 1u);

		printf("  -- CC 0x5c, cl=true via fieldA==6 (fieldB&2==0) --\n");
		buf[0x6d4] = 0x00; /* bit3 clear this time */
		value.fieldB = 0;
		value.fieldA = 6;
		g->HandleController(0x5c, value);
		check_eq("bit0 copied from bit3 (0)", (unsigned int)(buf[0x6d4] & 0x09), 0u);

		printf("  -- CC 0x5e (MFX): bits 1+4 --\n");
		buf[0x6d4] = 0;
		value.fieldB = 0;
		value.fieldA = 1;
		value.field8 = 0;
		g->HandleController(0x5e, value);
		check_eq("bits 1 and 4 both set", (unsigned int)(buf[0x6d4] & 0x12), 0x12u);
		check_eq("msg type == 5", *(unsigned int *)(g_lastUnsolicitedMessage + 0x10), 5u);

		printf("  -- CC 0x5f (TFX): bits 2+5 --\n");
		buf[0x6d4] = 0;
		g->HandleController(0x5f, value);
		check_eq("bits 2 and 5 both set", (unsigned int)(buf[0x6d4] & 0x24), 0x24u);
		check_eq("msg type == 6", *(unsigned int *)(g_lastUnsolicitedMessage + 0x10), 6u);

		printf("  -- CC 0x7a (122), cl=true -> no-op --\n");
		memset(buf, 0, globalSize);
		value.fieldB = 2; /* cl=true */
		value.fieldA = 1;
		value.field8 = 100;
		g_pushUnsolicitedMessageCalls = 0;
		g->HandleController(0x7a, value);
		check_eq("cl=true -> entirely no-op", (unsigned int)g_pushUnsolicitedMessageCalls, 0u);

		printf("  -- CC 0x7a, cl=false, +0x6ae==0 --\n");
		value.fieldB = 0;
		value.fieldA = 1;
		value.field8 = 100; /* > 0x3f -> above3f=1 */
		buf[0x6ae] = 0;
		g_resetAllControllersCalls = 0;
		g_stealAllVoicesCalls = 0;
		g_resetAllJumpCatchCalls = 0;
		g_writeQueueCalls = 0;
		g->HandleController(0x7a, value);
		check_eq("+0x6ae==0: ResetAllControllers NOT called", (unsigned int)g_resetAllControllersCalls, 0u);
		check_eq("  ...StealAllVoices NOT called", (unsigned int)g_stealAllVoicesCalls, 0u);
		check_eq("  ...no MIDI message sent", (unsigned int)g_writeQueueCalls, 0u);
		check_eq("+0x6af set to above3f (1)", buf[0x6af], 1);
		check_eq("final push message type == 0x15", *(unsigned int *)(g_lastUnsolicitedMessage + 0x10), 0x15u);
		check_eq("  ...msg+0x14 == above3f (1)", *(unsigned int *)(g_lastUnsolicitedMessage + 0x14), 1u);

		printf("  -- CC 0x7a, cl=false, +0x6ae!=0, msgProc gate SET (skip StealAllVoices) --\n");
		buf[0x6ae] = 1;
		buf[0x6b9] = 7; /* resolved channel */
		buf[0x6b8] = 3; /* global channel */
		msgProc2[0x48] = 1;
		value.field8 = 10; /* <=0x3f -> above3f=0 */
		g_resetAllControllersCalls = 0;
		g_stealAllVoicesCalls = 0;
		g_resetAllJumpCatchCalls = 0;
		g_writeQueueCalls = 0;
		g->HandleController(0x7a, value);
		check_eq("msgProc gate set -> StealAllVoices NOT called", (unsigned int)g_stealAllVoicesCalls, 0u);
		check_eq("ResetAllControllers called once", (unsigned int)g_resetAllControllersCalls, 1u);
		check_eq("  ...with the resolved channel (+0x6b9)", g_lastResetAllControllersChannel, 7);
		check_eq("  ...flag == false", (unsigned int)g_lastResetAllControllersFlag, 0u);
		check_eq("ResetAllJumpCatch called (unconditional on this sub-path)",
			 (unsigned int)g_resetAllJumpCatchCalls, 1u);
		check_eq("+0x6af set to above3f (0)", buf[0x6af], 0);
		check_eq("MIDI message sent once", (unsigned int)g_writeQueueCalls, 1u);
		check_eq("  ...msg[0] == (channel|0xb0)", g_lastQueueMsg[0], (unsigned char)(3 | 0xb0));
		check_eq("  ...msg[1] == 0x79", g_lastQueueMsg[1], 0x79);
		check_eq("  ...msg[2] == 6-above3f (6)", g_lastQueueMsg[2], 6);
		check_eq("  ...msg[3] == 5", g_lastQueueMsg[3], 5);
		check_eq("  ...msg[4] == 0xff", g_lastQueueMsg[4], (unsigned char)0xff);

		printf("  -- CC 0x7a, +0x6ae!=0, msgProc gate CLEAR (StealAllVoices ALSO called) --\n");
		msgProc2[0x48] = 0;
		g_resetAllControllersCalls = 0;
		g_stealAllVoicesCalls = 0;
		g->HandleController(0x7a, value);
		check_eq("msgProc gate clear -> StealAllVoices called", (unsigned int)g_stealAllVoicesCalls, 1u);
		check_eq("  ...ResetAllControllers ALSO still called", (unsigned int)g_resetAllControllersCalls, 1u);

		printf("  -- other ccNumber: no-op --\n");
		g_pushUnsolicitedMessageCalls = 0;
		g->HandleController(0x10, value);
		g->HandleController(0x60, value);
		g->HandleController(0x79, value);
		g->HandleController(0x7b, value);
		check_eq("no branch matched -> never pushed a message",
			 (unsigned int)g_pushUnsolicitedMessageCalls, 0u);

		munmap(ctrlRT6, 0x30);
		munmap(midiDisp13, 0x10);
		munmap(midiPortMgr6, 0x300);
		munmap(msgProc2, 0x1040);
		munmap(voiceAlloc, 0x10);
		/* mgr52 deliberately NOT munmap'd -- see [17]'s own identical
		 * comment (this is currently the last section in the file, so
		 * it's not strictly needed here, but kept consistent so a
		 * future section added after this one doesn't reintroduce the
		 * same dangling-pointer hazard). */
	}

	munmap(buf, globalSize);

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
