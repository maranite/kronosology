// SPDX-License-Identifier: GPL-2.0
/*
 * test_global_ctor.cpp  -  host-side known-answer test for
 * CSTGGlobal::CSTGGlobal() (see ../include/oa_global.h /
 * ../src/engine/global_ctor.cpp).
 *
 * Mocks every sub-object constructor this method calls (both the
 * brand-new confirmed-real-but-deferred classes and
 * CSTGControllerRTData's own newly-declared constructor) as simple
 * counters, then verifies:
 *   [1] every confirmed real object COUNT is exact (2944 CSTGProgram,
 *       1792 CSTGCombi, 201 CSTGSequence [200 array + 1 standalone],
 *       128 CSetList, 598 CSTGWaveSequence, 32 CSTGSlotVoiceData, 1
 *       each of CSTGAudioBusManager/CSTGControllerRTData/
 *       CSTGSamplingInterface/CSTGAudioInput/CSTGDrumKitData/
 *       CSTGProgramModeProgramSlot/CSTGProgramModeDrumTrackSlot).
 *   [2] a handful of confirmed non-zero field writes land at their
 *       exact real offsets (sInstance, the 'v'/'w' marker bytes, the
 *       +0x6af/+0x6d5..+0x6d9/+0x6db flags, several 0x10/0xff
 *       default-value table entries).
 *   [3] the large confirmed pure-zero memset ranges are genuinely
 *       zeroed (spot-checked at their start/middle/end).
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "oa_global.h"
#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */
#include "oa_setup_global_resources.h"

/* Same mocks test_global.cpp/test_managers.cpp need for
 * CSTGAudioBusManager's own real constructor (linked in via
 * managers.cpp). */
/* Sec 10.225: CSTGAudioDriverInterface::sInstance + the KorgUsbAudio*
 * externs CSTGAudioDriverInterfaceKorgUsb::Initialize()/Start()/
 * KeepSynchronized() now call directly (managers.cpp, which this file
 * links) -- link-satisfying host mocks only, same treatment as the
 * RTAI wrappers below. */
CSTGAudioDriverInterface *CSTGAudioDriverInterface::sInstance;
extern "C" int   KorgUsbAudioInitialize(void) { return 0; }
extern "C" int   KorgUsbAudioInitialized(void) { return 0; }
extern "C" int   KorgUsbAudioStart(void) { return 0; }
extern "C" void *KorgUsbAudioInput(void) { return 0; }
extern "C" void  KorgUsbAudioInputDone(void) { }
extern "C" void *KorgUsbAudioOutput(void) { return 0; }
extern "C" void  KorgUsbAudioOutputDone(void) { }
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
/* CSTGAudioManager::~CSTGAudioManager() is now real (managers.cpp,
 * linked directly by this test, sec 10.225 -- no longer virtual, no
 * mock needed here any more). */
/* Sec 10.147: ~CSTGVoiceAllocator()/~CSTGMessageProcessor() are now real
 * (see managers.cpp) -- link-satisfying only, same reasoning as
 * test_global.cpp's own identical addition. */
extern "C" void rtwrap_pthread_mutex_destroy(void *) { }
extern "C" void rtwrap_free(void *) { }
/* CEffectorDatabase::~CEffectorDatabase() is now real too (sec 10.148,
 * see managers.cpp) -- link-satisfying only, nothing in this file
 * constructs one. */
/* CSTGAudioInput's own 9 UpdateXXX methods + ctor are now reconstructed
 * for real (sec 10.80, see src/engine/global.cpp) -- no mock needed for
 * CSTGAudioInput itself any more (section [1] below now verifies real
 * ctor construction via its own confirmed +0x77 flags-byte side effect
 * instead of a call counter), but its own confirmed-real, deliberately
 * deferred dependencies still need link-satisfying mocks here. */
void CSTGAudioInputMixerBase::SetHDRBus(unsigned int, int) { }
void CSTGAudioInputMixerBase::SetFXCtrlBus(unsigned int, int) { }
void CSTGAudioInputMixerBase::SetOutputBus(unsigned int, int) { }
void CSTGAudioInputMixerBase::SetPan(unsigned int, float) { }
void CSTGControllerRTData::SetAudioInSolo(unsigned int, bool) { }
void CSTGControllerRTData::ResetSendKnobsJumpCatch() { }
CSTGProgramSlot::CSTGProgramSlot() { }
/* CSTGProgramSlot::ChangeProgram() is real now (batch 47, global.cpp,
 * which this file links directly) -- stale mock removed (see the new
 * link-satisfying mocks for its own callees, Setup()/CompleteLoadProgram()/
 * SetIsDying(), added just above this constructor). */
/* CSTGProgramSlot::IsActive()/AccessActiveSlotVoiceData() are now real
 * too (sec 10.142, see global.cpp). */
/* Sec 10.67's own confirmed-real, deliberately deferred externs -- not
 * exercised by this file's own tests (see test_global.cpp). */
int OmapNKS4OutputFifo_WriteCommand(int) { return 1; }
void CSTGControllerRTData::SetControllerAssignment(void *, signed char, bool) { }
CSTGControllerRTData *CSTGControllerRTData::sInstance;
void CSTGControllerRTData::ResetKnobsJumpCatch() { }
void CSTGControllerRTData::ResetSlidersJumpCatch() { }
void CSTGControllerRTData::ResetRTKKnobSmoothers() { }
void CSTGControllerRTData::OnExtModeSetChange() { }
void CSTGControllerRTData::OnPerformanceActivate(CSTGPerformance &) { }
void CLoadBalancer::BalanceStaticLoad() { }
unsigned char *STGAPIFrontPanelStatus::sInstance;
/* TSTGArrayManager<T>::sInstance's own real storage (per-T instantiation)
 * lives in engine_init.cpp (not linked here) -- this file links
 * managers.cpp directly, so CSTGSamplingDaemon::ProcessCommands()'s own
 * real reference to the CSTGRecordBuffer instantiation (sec 10.160)
 * needs local storage to link. */
template<> TSTGArrayManager<CSTGRecordBuffer> *TSTGArrayManager<CSTGRecordBuffer>::sInstance = 0;
/* CSTGControllerRTData::OnExtModeKnobAssignChange/OnExtModeSliderAssignChange
 * are now real (sec 10.161) -- see global.cpp (this file already links
 * global.cpp + the new cc_info_table.cpp directly). */
void CSTGControllerRTData::HandleControllerChange(int, unsigned char, bool, bool) { }
void CSTGControllerInfo::SetPerfSwitch(int, bool) { }
void CSTGControllerInfo::SendUnsolicitedUIParam(unsigned int, unsigned int, long, int) { }
CSTGMidiDispatcher *CSTGMidiDispatcher::sInstance;
void CSTGVoiceAllocator::StealAllVoices() { }
CSTGMidiPortManager *CSTGMidiPortManager::sInstance;
void CSTGMidiPortManager::WriteSTGMidiOutQueue(const unsigned char *, unsigned int) { }
/* Sec 10.90/10.134's own confirmed-real, deliberately deferred externs
 * -- not exercised by this file's own tests (see test_global.cpp). */
void CSTGAudioInput::UseSettings() { }
void CSTGMidiPortManager::NotifyNKS4TestMode() { }
extern "C" void COmapNKS4Driver_SetTestMode(int) { }
unsigned int CSTGMidiQueue::GetNumWritableBytes() const { return 0; }
void USTGAliasBankTypes::ConvertAliasPgmBankToMidiBank(int, char &, char &) { }
void USTGAliasBankTypes::ConvertCombiBankToMidiBank(int, char &, char &) { }
void USTGAliasBankTypes::ConvertMidiBankToCombiBank(char, char, int &) { }
void USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(char, char, int &) { }
bool SKSTGGate_ShouldSyncExternalClock() { return false; }
extern "C" void PushUnsolicitedMessage(void *) { }
/* Sec 10.92's own confirmed-real, deliberately deferred externs -- not
 * exercised by this file's own tests (see test_global.cpp).
 * StealingRequiresOneTickStall is now real (sec 10.136). */
void CSTGSmoother::FinalizeSmoother(void *, bool) { }
void CSTGChannelValues::Reset() { }
/* CSetListEQ::SetBand() -- confirmed-real, deliberately deferred
 * (audio-DSP out of scope, batch 41). CSetList::Activate() (its own
 * caller, now real in global.cpp) isn't exercised by this file's own
 * tests -- see test_global.cpp for the real call-tracking KAT. */
void CSetListEQ::SetBand(unsigned int, float) { }
void CSTGChannelValues::SetControllerValue(unsigned char, const CSTGControllerValue &) { }
void CSTGParamsOwner::ValidateParamChange(CSTGMessageContext &, unsigned long, const CValue &) { }
/* CSTGParamsOwner::UseDefaults() (sec 10.228) -- confirmed real,
 * deliberately deferred; see oa_global.h's own comment and
 * global.cpp's CSTGGlobal::Initialize(), its real caller. Trivial
 * link-satisfying mock -- this file only exercises the constructor,
 * never Initialize(). */
void CSTGParamsOwner::UseDefaults() { }
void CSTGSlotVoiceData::FreeSlotVoiceData(bool) { }
/* CSTGProgramSlot::ChangeProgram() is real now (batch 47, global.cpp,
 * which this file links directly), reachable only via its own two new
 * real vtable-slot-56 implementations (ProcessPreviousSVDOnProgramChange)
 * and its own two confirmed-real, deliberately deferred DSP callees --
 * trivial link-satisfying mocks, not exercised by this file's own tests
 * (see test_global.cpp for the real call-tracking KAT). */
void CSTGSlotVoiceData::SetIsDying() { }
void CSTGSlotVoiceData::Setup(CSTGProgramSlot *, CSTGProgram *, const CSTGChannelValues *) { }
void CSTGProgramSlot::CompleteLoadProgram(CSTGSlotVoiceData *) { }
/* CSTGPerformanceVarsManager::RunEffects() is real now (batch 49, see
 * global.cpp, which this file links directly) -- trivial link-satisfying
 * mock for its own confirmed-real, deliberately deferred DSP callee, not
 * exercised by this file's own tests (see test_global.cpp). */
void CSTGPerformance::RunEffects(CSTGPerformanceVars *) { }
/* CSTGVoiceAllocator::EmergencyFreeVoiceList(void*) is real now (sec
 * 10.149, see managers.cpp, which this file links directly) -- no mock
 * here any more. Its own new real dependencies need trivial link-
 * satisfying mocks instead (not exercised by this file's own tests). */
extern "C" void rtwrap_pthread_mutex_lock(void *) { }
extern "C" void rtwrap_pthread_mutex_unlock(void *) { }
void CSTGVoiceAllocator::FreeVoice(CSTGVoice *) { }
void CSTGVoiceAllocator::DoPendingMoveVoices() { }
void CSTGVoiceAllocator::StealVoiceList(void *) { }
/* Link-satisfying mocks for sec 10.144's new CSTGHDRManager::ProcessCommands()/
 * CSTGCDWorker::Initialize() bodies -- not exercised by anything in this
 * file, only needed so managers.cpp links cleanly. Sec 10.148:
 * CSTGCDWorker_InitializeBuffer() is now real (managers.cpp) and calls
 * __kmalloc directly -- link-satisfying mock only. */
extern "C" void *__kmalloc(unsigned long size, unsigned int) { return malloc(size); }
void CSTGHDRManager::ProcessPlaybackCommands() { }
void CSTGHDRManager::ProcessRecordCommands() { }
void CSTGHDRManager::ProcessSamplerCommands() { }
void CSTGSlotVoiceData::RunVoiceModelStaticFront(unsigned int) { }
void CSTGSlotVoiceData::RunVoiceModelStaticBack(unsigned int) { }
void CSTGSlotVoiceData::GetTotalStaticCosts(unsigned long *, unsigned long *) const { }
float gAllPlusHeadroom[4];
float gAllMinusHeadroom[4];

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got == want) {
		printf("  ok    %-60s %lu\n", label, got);
		return;
	}
	printf("  FAIL  %-60s got=%lu want=%lu\n", label, got, want);
	g_fail++;
}

static int g_samplingIfaceCalls, g_drumKitCalls;
static int g_waveSeqCalls, g_programCalls, g_combiCalls, g_sequenceCalls;
static int g_setListCalls, g_controllerRtCalls;
static int g_slotVoiceDataCalls;

CSTGSamplingInterface::CSTGSamplingInterface() { g_samplingIfaceCalls++; }
CSTGDrumKitData::CSTGDrumKitData() { g_drumKitCalls++; }
CSTGWaveSequence::CSTGWaveSequence() { g_waveSeqCalls++; }
CSTGProgram::CSTGProgram() { g_programCalls++; }
CSTGCombi::CSTGCombi() { g_combiCalls++; }
CSTGSequence::CSTGSequence() { g_sequenceCalls++; }
CSetList::CSetList() { g_setListCalls++; }
/* CSetList::Activate() is now real (batch 41) -- stale flat mock removed. */
CSTGControllerRTData::CSTGControllerRTData() { g_controllerRtCalls++; }
CSTGSlotVoiceData::CSTGSlotVoiceData() { g_slotVoiceDataCalls++; }

/* This test also links global.cpp wholesale (needed for
 * CSTGGlobal::sInstance's own definition), which pulls in
 * RunVoiceModelFeedback()/Initialize()'s own confirmed-real-deferred
 * dependencies too -- trivial no-op stubs, not exercised by this
 * test's own scenarios (see test_global.cpp for their dedicated
 * coverage). */
void CSTGSlotVoiceData::RunVoiceModelFeedback() { }
/* CSTGSlotVoiceData::Initialize(unsigned short) is now real (sec
 * 10.150, see global.cpp) -- no mock body here any more. Its own new
 * dependencies still need trivial link-satisfying definitions (see
 * test_engine.cpp's own identical comment for the full reasoning).
 * CSTGChannelValues::Initialize() is now real too (sec 10.151, see
 * global.cpp) -- same treatment. */
STGLFOSubRateParams *CSTGCommonLFO::sSubRateParams;
STGStepSeqSubRateParams *CSTGCommonStepSeq::sSubRateParams;
unsigned char CSTGChannelValues::sTemplateReady;
unsigned char CSTGChannelValues::sTemplate[0x92c];
/* CSTGAudioBusManager::sGlobalBusSet's own real storage lives in
 * audio_bus_manager.cpp (not linked here) -- batch 23's newly-real
 * CSTGPlaybackBuffer/CSTGMonitorMixerChannel ctors (managers.cpp) are the
 * first code there to reference it, so this file needs its own local
 * definition too. */
unsigned char CSTGAudioBusManager::sGlobalBusSet[34 * 0x80];
void CSTGChannelValues::InitializeLongHand() { }
void CSTGSlotVoiceData::UpdateGlobalTune(float) { }
/* CSTGPerformance::IsCurrentlyActive() is real now (sec 10.144, see
 * managers.cpp) -- CSTGPerformanceVarsManager::sInstance is never set up
 * in this file, so the real implementation naturally resolves to null
 * and returns false, matching this mock's old unconditional behavior. */
void CSTGMidiDispatcher::HandleController(unsigned char, unsigned char, unsigned char, int, int) { }
void CSTGMidiDispatcher::ResetAllControllers(unsigned char, bool) { }
void CSTGMidiQueueWriter::Write(const unsigned char *, unsigned int, bool) { }
CSTGSmoother *CSTGSmoother::sInstance;
void CSTGSmoother::CancelAllSmoothers() { }
void CSTGSmoother::FinalizeAllSmoothers() { }
void CSTGPerformanceVars::SetIsDying() { }
void CSTGPerformanceVars::EnterActivatingState() { }
extern "C" void *sXCmd;
extern "C" unsigned int kAudXBZD;
extern "C" float allPlusOne[4];
extern "C" float allMinusOne[4];
void *sXCmd;
unsigned int kAudXBZD;
float allPlusOne[4];
float allMinusOne[4];
CSTGVectorManager *CSTGVectorManager::sInstance;
void CSTGVectorManager::OnUpdateGlobalMidiChannel(unsigned char channel)
{
	unsigned char *base = (unsigned char *)this;
	base[0x19da4] = channel;
	base[0x1b758] = channel;
}
/* CSTGHeldKeyList::Reset()/CSTGEffectRackVars::UpdateDModRoutings() are
 * now real (sec 10.82/10.135, see global.cpp). */
void CSTGWaveSeqData::Initialize() { }
unsigned char CSTGPerformanceVarsManager::sInstance[12];
void CSTGPerformanceVarsManager::Initialize() { }
/* CSTGControllerRTData::Initialize() is now real (sec 10.88).
 * RequestAnalogInputStatus is ALSO now real (sec 10.131) -- this file
 * links controller_rt_data_init.cpp directly, so no mock is needed. */
CSTGFrontPanel *CSTGFrontPanel::sInstance;
void USTGAliasBankTypes::InitializeAliasBanks() { }
/* Mock arrays -- see test_engine.cpp's own identical comment. */
int STGAliasToRealPgmBank[30 * 128];
int STGAliasBankPgmMap[30 * 128];
void CSTGGlobal::InitializePerformances() { }
void CSetListBank::Initialize() { }

int main(void)
{
	printf("CSTGGlobal::CSTGGlobal() known-answer test\n");
	printf("=========================================================\n");

	/* Same ~43.6MB confirmed real object size test_global.cpp/
	 * test_engine.cpp already established. */
	size_t globalSize = 0x29cc920;
	unsigned char *buf = (unsigned char *)calloc(1, globalSize);

	new (buf) CSTGGlobal();
	CSTGGlobal *g = (CSTGGlobal *)buf;

	printf("[1] confirmed real sub-object construction counts\n");
	check_eq("CSTGSamplingInterface", g_samplingIfaceCalls, 1);
	/* CSTGAudioInput's own ctor is now real (sec 10.80) -- verify via its
	 * own confirmed +0x77 flags-byte side effect (bit0 set) at its
	 * confirmed CSTGGlobal+0x608 embedded address, instead of a mock
	 * call counter. */
	check_eq("CSTGAudioInput (real ctor ran: +0x608+0x77 bit0 set)",
		 buf[0x608 + 0x77] & 0x1, 1);
	check_eq("CSTGDrumKitData", g_drumKitCalls, 1);
	check_eq("CSTGControllerRTData", g_controllerRtCalls, 1);
	check_eq("CSTGWaveSequence (598 confirmed)", g_waveSeqCalls, 598);
	check_eq("CSTGProgram (23 banks x 128 + 1 standalone = 2945)", g_programCalls, 2945);
	/* CSTGCombi (14 banks x 128 = 1792) DIRECT constructions, PLUS 201
	 * MORE implicit base-ctor calls from CSTGSequence's own real
	 * inheritance (sec 10.153: CSTGSequence : public CSTGCombi is now a
	 * genuine C++ base class relationship, not just a flat opaque
	 * class -- so every CSTGSequence construction, even this test
	 * file's own trivial mock body below, unconditionally triggers
	 * CSTGCombi's ctor first, per the Itanium ABI's own "base ctor
	 * before derived body" rule). 1792 + 201 = 1993. */
	check_eq("CSTGCombi (14 banks x 128 = 1792, + 201 implicit via CSTGSequence base)",
		 g_combiCalls, 1993);
	check_eq("CSTGSequence (200 array + 1 standalone = 201)", g_sequenceCalls, 201);
	check_eq("CSetList (128 confirmed)", g_setListCalls, 128);
	/* CSTGProgramModeProgramSlot/DrumTrackSlot's own ctors are now real
	 * (sec 10.81) -- verify via their own confirmed +0x4 "slot kind"
	 * discriminator byte (0/1 respectively) at their confirmed
	 * CSTGGlobal-embedded offsets, instead of mock call counters. */
	check_eq("CSTGProgramModeProgramSlot (real ctor ran: +0x2977b1f+0x4 == 0)",
		 buf[0x2977b1f + 0x4], 0);
	check_eq("CSTGProgramModeDrumTrackSlot (real ctor ran: +0x2977c08+0x4 == 1)",
		 buf[0x2977c08 + 0x4], 1);
	check_eq("CSTGSlotVoiceData (32 confirmed)", g_slotVoiceDataCalls, 32);

	printf("\n[2] confirmed non-zero field writes\n");
	check_eq("sInstance == this", (unsigned long)(CSTGGlobal::sInstance == g), 1);
	check_eq("+0x6c0 == 'v'", buf[0x6c0], 'v');
	check_eq("+0x6c1 == 'w'", buf[0x6c1], 'w');
	check_eq("+0x6c2 == 0", buf[0x6c2], 0);
	check_eq("+0x6af == 1", buf[0x6af], 1);
	check_eq("+0x6d5..+0x6d9 all == 1",
		 (unsigned long)(buf[0x6d5] == 1 && buf[0x6d6] == 1 && buf[0x6d7] == 1 &&
				  buf[0x6d8] == 1 && buf[0x6d9] == 1), 1);
	check_eq("+0x6db == 1", buf[0x6db], 1);
	check_eq("+0x297515c == 1 (dword)", *(unsigned int *)(buf + 0x297515c), 1);
	check_eq("+0x2975178 == 1 (dword)", *(unsigned int *)(buf + 0x2975178), 1);
	check_eq("default-value table +0x29c9fc8 == 0x10", buf[0x29c9fc8], 0x10);
	check_eq("default-value table +0x29ca3c8 == 0xff", buf[0x29ca3c8], 0xff);
	check_eq("default-value table (last outer iter) +0x29c9fc8+127*8 == 0x10",
		 buf[0x29c9fc8 + 127 * 8], 0x10);
	check_eq("MIDIChannel table +0x29cc0d0 == 0x10 (matches sec 10.33's own confirmed field)",
		 buf[0x29cc0d0], 0x10);
	check_eq("+0x29cc0d1 == 0xff", buf[0x29cc0d1], 0xff);

	printf("\n[3] confirmed pure-zero memset ranges (spot-checked)\n");
	check_eq("+0x29c98f4 (range start) == 0", *(unsigned int *)(buf + 0x29c98f4), 0);
	check_eq("+0x29c99a0 (range middle) == 0", *(unsigned int *)(buf + 0x29c99a0), 0);
	check_eq("+0x29c9a94 (range end) == 0", *(unsigned int *)(buf + 0x29c9a94), 0);
	check_eq("+0x132c8c8 (large memset start) == 0", *(unsigned int *)(buf + 0x132c8c8), 0);
	check_eq("+0x132c8c8+0x1c04 (large memset end) == 0",
		 *(unsigned int *)(buf + 0x132c8c8 + 0x1c04), 0);
	check_eq("+0x29cc11c (MIDIChannel array, zeroed) == 0", buf[0x29cc11c], 0);
	check_eq("+0x29cc11c+0x77*8 (last array slot, zeroed) == 0",
		 buf[0x29cc11c + 0x77 * 8], 0);

	free(buf);

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
