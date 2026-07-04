// SPDX-License-Identifier: GPL-2.0
/*
 * test_engine.cpp  -  host-side known-answer tests for CSTGEngine's
 * fully-reconstructed methods and CSTGGlobal::IncrementMicrosecondCount
 * (Stage 3, see include/oa_engine.h / include/oa_global.h).
 *
 * CSTGEngine's tick methods are pure "call these singletons in this exact
 * order" dispatchers -- there's no arithmetic to check, so the KAT here is
 * a call-order log: mock implementations of every manager method record
 * their name into a shared log, and each test asserts the log matches the
 * exact sequence confirmed by disassembly. This is the same spirit as the
 * bank-memory/quad-list hand-traced KATs, applied to control flow instead
 * of arithmetic.
 *
 * CSTGGlobal's real object is confirmed to be enormous (fields land around
 * +0x29c9fb8, ~43.6MB in) -- the mock CSTGGlobal instance here is a real
 * heap buffer sized to cover that, not a stack object.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "oa_engine.h"
#include "oa_global.h"
#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"

/* Real kernel-only APIs and a not-yet-reconstructed static method, mocked
 * only so CSTGVoiceModelManager's constructor (used by test [1]) links on
 * the host -- see test_managers.cpp for the same treatment in more detail. */
extern "C" unsigned int CSTGCDWorker_InitializeBuffer(void *) { return 0; }
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

/* Real module-global data CSTGAudioBusManager's constructor touches (used
 * by test [3]'s `CSTGAudioBusManager abm` below) -- mocked here purely for
 * host linking, same treatment as the RTAI wrappers above. See
 * test_managers.cpp for the KAT that actually checks these values. */
float gAllPlusHeadroom[4];
float gAllMinusHeadroom[4];

static int g_fail;
static char g_log[4096];

static void log_call(const char *name)
{
	strcat(g_log, name);
	strcat(g_log, ";");
}

/* Fake-vtable traps for CSTGVoiceModelManager::ProcessSubRate/
 * ProcessAudioRate's own real per-entry virtual dispatch (sec
 * 10.137) -- installed at slots 0x48/4 and 0x4c/4 on a fake vtable
 * for test [1]'s single populated array entry. */
static void VoiceModelSubRateTrap(void *, unsigned int tick)
{ char b[32]; sprintf(b, "ProcessSubRate(%u)", tick); log_call(b); }
static void VoiceModelAudioRateTrap(void *, unsigned int tick)
{ char b[32]; sprintf(b, "ProcessAudioRate(%u)", tick); log_call(b); }
/* ~CSTGVoiceModelManager() is now real (sec 10.147, see managers.cpp):
 * it walks the same +0x30 array/+0x58 count as ProcessSubRate/
 * ProcessAudioRate above, but dispatches vtable slot +0x4/4 == 1 on each
 * non-null entry -- test [5] below reuses test [1]'s single populated
 * fake-vtable entry, so that entry's own vtable needs a trap installed
 * at slot 1 too. */
static void VoiceModelDtorEntryTrap(void *)
{ log_call("VoiceModelManager::entryDtor"); }

static void check_log(const char *label, const char *expected)
{
	if (strcmp(g_log, expected) == 0) {
		printf("  ok    %s\n", label);
	} else {
		printf("  FAIL  %s\n", label);
		printf("        got : %s\n", g_log);
		printf("        want: %s\n", expected);
		g_fail++;
	}
	g_log[0] = '\0';
}

/* ---- mock manager method bodies (opaque classes declared in oa_engine.h) ---- */
/* ProcessSubRate/ProcessAudioRate are now real (sec 10.137, see
 * managers.cpp) -- test [1] below gives vmm a real populated array
 * entry with a fake vtable trapping these two slots instead. */
/* ~CSTGVoiceModelManager() is now real (sec 10.147, see managers.cpp) --
 * no mock body here any more, see the VoiceModelDtorEntryTrap comment
 * above for how test [5]'s reused vmm object drives it safely. */
void CSTGEffectManager::RunEffects()             { log_call("CSTGEffectManager::RunEffects"); }
void CSTGAudioBusManager::MixPerformanceOutputs(){ log_call("MixPerformanceOutputs"); }
void CSTGAudioBusManager::LRBusIndivMirror()     { log_call("LRBusIndivMirror"); }
void CSTGHDRManager::ProcessHDRRecord()          { log_call("ProcessHDRRecord"); }
/* ProcessCommands() is real now (sec 10.144, see managers.cpp) -- calls
 * these three confirmed-real, still-deferred siblings in order. */
void CSTGHDRManager::ProcessPlaybackCommands()   { log_call("HDRManager::ProcessPlaybackCommands"); }
void CSTGHDRManager::ProcessRecordCommands()     { log_call("HDRManager::ProcessRecordCommands"); }
void CSTGHDRManager::ProcessSamplerCommands()    { log_call("HDRManager::ProcessSamplerCommands"); }
void CSTGMonitorMixer::RunMonitors()             { log_call("RunMonitors"); }
void CSTGFileOpener::ProcessCommands()           { log_call("FileOpener::ProcessCommands"); }
void CSTGFileCloser::ProcessCommands()           { log_call("FileCloser::ProcessCommands"); }
void CSTGHDRFileReader::ProcessCommands()        { log_call("HDRFileReader::ProcessCommands"); }
void CSTGHDRFileWriter::ProcessCommands()        { log_call("HDRFileWriter::ProcessCommands"); }
void CSTGStreamingFileReader::ProcessCommands()  { log_call("StreamingFileReader::ProcessCommands"); }
void CSTGCDWorker::ProcessCommands()             { log_call("CDWorker::ProcessCommands"); }
void CSTGSamplingDaemon::ProcessCommands()       { log_call("SamplingDaemon::ProcessCommands"); }
CSTGMidiPortManager::~CSTGMidiPortManager()      { log_call("~CSTGMidiPortManager"); }
void CSTGMidiPortManager::WriteSTGMidiOutQueue(const unsigned char *, unsigned int) { }
/* ~CSTGMessageProcessor() is now real (sec 10.147, see managers.cpp) --
 * no mock body here any more. Its own confirmed +0x64/+0x68 pointer
 * fields are zeroed by test [5] before the real destructor runs (see
 * that test), so the real body's null checks make it a genuine, silent
 * no-op there -- no log_call to expect. */
/* CSTGAudioDriverInterface::~CSTGAudioDriverInterface() is now defined in
 * managers.cpp (empty -- pure virtual destructors still need a body).
 * MockAudioDriverInterface's destructor below logs for this test; C++
 * destructor chaining fires the (silent) base one too, which is why only
 * one log entry shows up per virtual `delete`. */
CSTGAudioManager::~CSTGAudioManager()            { log_call("~CSTGAudioManager"); }
/* CSTGAudioInput's own 9 UpdateXXX methods are now reconstructed for
 * real (sec 10.80, see src/engine/global.cpp, linked into this test via
 * the Makefile) -- no mock needed for CSTGAudioInput itself any more,
 * but its own confirmed-real, deliberately deferred dependencies still
 * need link-satisfying mocks here (not exercised by this file's own
 * tests -- see test_global.cpp for the KATs that actually exercise
 * these). */
void CSTGAudioInputMixerBase::SetHDRBus(unsigned int, int) { }
void CSTGAudioInputMixerBase::SetFXCtrlBus(unsigned int, int) { }
void CSTGAudioInputMixerBase::SetOutputBus(unsigned int, int) { }
void CSTGAudioInputMixerBase::SetPan(unsigned int, float) { }
void CSTGControllerRTData::SetAudioInSolo(unsigned int, bool) { }
void CSTGControllerRTData::ResetSendKnobsJumpCatch() { }
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
void CSTGChannelValues::SetControllerValue(unsigned char, const CSTGControllerValue &) { }
void CSTGParamsOwner::ValidateParamChange(CSTGMessageContext &, unsigned long, const CValue &) { }
void CSTGSlotVoiceData::FreeSlotVoiceData(bool) { }
void CSTGVoiceAllocator::EmergencyFreeVoiceList(void *) { }
void CSTGSlotVoiceData::RunVoiceModelStaticFront(unsigned int) { }
void CSTGSlotVoiceData::RunVoiceModelStaticBack(unsigned int) { }
void CSTGSlotVoiceData::GetTotalStaticCosts(unsigned long *, unsigned long *) const { }
void CSTGVoiceAllocator::StealVoiceList(void *) { }
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
unsigned char *STGAPIFrontPanelStatus::sInstance;
void CSetList::Activate() { }
void CSTGControllerRTData::OnExtModeKnobAssignChange(unsigned int) { }
void CSTGControllerRTData::OnExtModeSliderAssignChange(unsigned int) { }
void CSTGControllerRTData::HandleControllerChange(int, unsigned char, bool, bool) { }
void CSTGControllerInfo::SetPerfSwitch(int, bool) { }
void CSTGControllerInfo::SendUnsolicitedUIParam(unsigned int, unsigned int, long, int) { }
CSTGMidiDispatcher *CSTGMidiDispatcher::sInstance;
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
/* CSTGProgramSlot::IsActive()/AccessActiveSlotVoiceData() are now real
 * too (sec 10.142, see global.cpp). */
/* CSTGHeldKeyList::Reset()/CSTGEffectRackVars::UpdateDModRoutings() are
 * now real (sec 10.82/10.135, see global.cpp). */
void CSTGVoiceAllocator::StealAllVoices() { }
/* ~CSTGVoiceAllocator() is now real (sec 10.147, see managers.cpp) -- no
 * mock body here any more. It calls the SAME rtwrap_pthread_mutex_destroy/
 * rtwrap_free mocks below (already used by CPowerOffTimer's teardown in
 * ~CSTGEngine), so test [5]'s expected log now shows a SECOND
 * "rtwrap_pthread_mutex_destroy;rtwrap_free;" pair where "~CSTGVoiceAllocator"
 * used to appear. */
CLoadBalancer::~CLoadBalancer()                  { log_call("~CLoadBalancer"); }
void CLoadBalancer::BalanceStaticLoad() { }
/* CEmergencyStealer's own destructor is now declared (sec 10.59) --
 * still not reconstructed, but any CLoadBalancer destruction (even
 * this mock's) implicitly chains into it, so it needs a link-time
 * body here too. */
CEmergencyStealer::~CEmergencyStealer()          { }
/* CEffectorDatabase's own confirmed-real, deliberately deferred dtor
 * (sec 10.147) -- link-satisfying only, msgProc's own +0x64 is zeroed
 * below so the real ~CSTGMessageProcessor() never actually calls this
 * in this test. */
CEffectorDatabase::~CEffectorDatabase()          { }
/* CSTGGlobal::RunVoiceModelFeedback() is now REAL (sec 10.55, see
 * global.cpp) -- with the zeroed CSTGGlobal buffer scenario [3] below
 * allocates, its list head at +0x29c9900 is empty, so it produces no
 * observable side effect (a genuine no-op on an empty list, not a
 * missing mock). See test_global.cpp for its own dedicated coverage. */

/* This test links global.cpp wholesale, so every symbol it references
 * needs SOME definition to link even though CSTGGlobal::Initialize()
 * itself is never called by this test's own scenarios (that's
 * test_global.cpp's job) -- trivial no-op stubs, not exercised here. */
void CSTGWaveSeqData::Initialize() {}
void CSTGSlotVoiceData::Initialize(unsigned short) {}
void CSTGSlotVoiceData::RunVoiceModelFeedback() {}
/* CSTGProgramModeProgramSlot/CSTGProgramModeDrumTrackSlot's own
 * Initialize()/ctors are now real (sec 10.81, see global.cpp) -- their
 * shared CSTGProgramSlot base class's own confirmed-real, deliberately
 * deferred dependencies still need link-satisfying mocks here. */
CSTGProgramSlot::CSTGProgramSlot() {}
void CSTGProgramSlot::ChangeProgram(CSTGProgram *) {}
unsigned char CSTGPerformanceVarsManager::sInstance[12];
void CSTGPerformanceVarsManager::Initialize() {}
/* CSTGControllerRTData::Initialize()'s own real body now lives in
 * controller_rt_data_init.cpp (sec 10.88, not linked into this test) --
 * this file doesn't exercise it specifically, so a trivial mock still
 * satisfies the linker. Its own confirmed-real dependency also needs a
 * link-satisfying mock here in case anything else pulls it in. */
void CSTGControllerRTData::Initialize() {}
CSTGFrontPanel *CSTGFrontPanel::sInstance;
void CSTGFrontPanel::RequestAnalogInputStatus(unsigned int) {}
void USTGAliasBankTypes::InitializeAliasBanks() {}
/* Mock arrays -- alias_bank_init.cpp's own real definitions are
 * deliberately not linked here so InitializeAliasBanks() stays mockable
 * (see that file's own header comment); GetAliasPgmBankMapping (sec
 * 10.110) still needs SOME definition to link against. */
int STGAliasToRealPgmBank[30 * 128];
int STGAliasBankPgmMap[30 * 128];
void CSTGGlobal::InitializePerformances() {}
void CSetListBank::Initialize() {}

extern "C" void rtwrap_pthread_mutex_destroy(void *) { log_call("rtwrap_pthread_mutex_destroy"); }
extern "C" void rtwrap_free(void *)                  { log_call("rtwrap_free"); }
extern "C" void signal_timed_out_daemons(void)       { log_call("signal_timed_out_daemons"); }

/* A real (derived, non-abstract) implementation so `delete
 * CSTGAudioDriverInterface::sInstance` in the destructor has a concrete
 * vtable to dispatch through. */
class MockAudioDriverInterface : public CSTGAudioDriverInterface {
public:
	virtual ~MockAudioDriverInterface() { log_call("~CSTGAudioDriverInterface"); }
};

int main(void)
{
	printf("CSTGEngine / CSTGGlobal::IncrementMicrosecondCount known-answer test\n");
	printf("======================================================================\n");

	printf("[1] CSTGEngine::RunAudioTick calls ProcessSubRate then ProcessAudioRate, same arg\n");
	CSTGVoiceModelManager vmm;
	CSTGVoiceModelManager::sInstance = &vmm;
	/* ProcessSubRate/ProcessAudioRate are now real (sec 10.137): give
	 * vmm's own confirmed +0x30 array/+0x58 count fields one real entry
	 * with a fake vtable, so the real per-entry dispatch loop has
	 * something valid to call through. */
	unsigned char *vmmRaw = (unsigned char *)&vmm;
	static void *vmmFakeItem[1];
	static void *vmmFakeVtable[20];
	for (int i = 0; i < 20; i++)
		vmmFakeVtable[i] = 0;
	vmmFakeVtable[0x48 / 4] = (void *)VoiceModelSubRateTrap;
	vmmFakeVtable[0x4c / 4] = (void *)VoiceModelAudioRateTrap;
	/* Slot 1 (offset +4) is what the now-real ~CSTGVoiceModelManager()
	 * dispatches (sec 10.147) -- test [5] reuses this same vmm/entry. */
	vmmFakeVtable[4 / 4] = (void *)VoiceModelDtorEntryTrap;
	vmmFakeItem[0] = vmmFakeVtable;
	*(void **)(vmmRaw + 0x30) = vmmFakeItem;
	*(short *)(vmmRaw + 0x58) = 1;
	/* Heap-allocated and deliberately never destroyed within this test: the
	 * real destructor (correctly, matching disassembly) does not null out
	 * most singleton pointers, so letting this object's destructor run
	 * automatically at scope exit would tear down test [5]'s singletons a
	 * second time against already-dangling pointers. */
	CSTGEngine *engine = new CSTGEngine();
	engine->RunAudioTick(42);
	check_log("RunAudioTick(42)", "ProcessSubRate(42);ProcessAudioRate(42);");

	printf("[2] CSTGEngine::RunEffects calls CSTGEffectManager::RunEffects\n");
	CSTGEffectManager em;
	CSTGEffectManager::sInstance = &em;
	engine->RunEffects();
	check_log("RunEffects", "CSTGEffectManager::RunEffects;");

	printf("[3] CSTGEngine::PostAudioTick's exact confirmed call order\n");
	CSTGAudioBusManager abm; CSTGAudioBusManager::sInstance = &abm;
	CSTGHDRManager hdr; CSTGHDRManager::sInstance = &hdr;
	CSTGMonitorMixer mon; CSTGMonitorMixer::sInstance = &mon;
	/* CSTGGlobal's real object is confirmed to reach ~43.6MB in confirmed
	 * field offsets -- allocate a real heap buffer that size, zeroed. */
	size_t globalSize = 0x29c9fc0;
	unsigned char *globalBuf = (unsigned char *)calloc(1, globalSize);
	CSTGGlobal::sInstance = (CSTGGlobal *)globalBuf;
	engine->PostAudioTick();
	check_log("PostAudioTick",
		  "MixPerformanceOutputs;ProcessHDRRecord;"
		  "RunMonitors;LRBusIndivMirror;signal_timed_out_daemons;");
	/* IncrementMicrosecondCount() is REAL (not mocked) -- verify its
	 * confirmed 667/667/666 phase-cycle counter actually ran once. */
	unsigned int usecLo = *(unsigned int *)(globalBuf + 0x29c9fb0);
	if (usecLo == 0x29b)
		printf("  ok    IncrementMicrosecondCount really ran (usecLo == 0x29b)\n");
	else {
		printf("  FAIL  IncrementMicrosecondCount usecLo got=0x%x want=0x29b\n", usecLo);
		g_fail++;
	}
	/* And the second, separately-inlined counter at +0x29c9fa8 incremented by 1. */
	unsigned int counter2 = *(unsigned int *)(globalBuf + 0x29c9fa8);
	if (counter2 == 1)
		printf("  ok    second inline counter incremented to 1\n");
	else {
		printf("  FAIL  second inline counter got=%u want=1\n", counter2);
		g_fail++;
	}

	printf("[4] CSTGEngine::RunFileDaemonSynchronization's exact confirmed call order\n");
	CSTGFileOpener fo; CSTGFileOpener::sInstance = &fo;
	CSTGFileCloser fc; CSTGFileCloser::sInstance = &fc;
	CSTGHDRFileReader hfr; CSTGHDRFileReader::sInstance = &hfr;
	CSTGHDRFileWriter hfw; CSTGHDRFileWriter::sInstance = &hfw;
	CSTGStreamingFileReader sfr; CSTGStreamingFileReader::sInstance = &sfr;
	CSTGCDWorker cdw; CSTGCDWorker::sInstance = &cdw;
	CSTGSamplingDaemon sd; CSTGSamplingDaemon::sInstance = &sd;
	engine->RunFileDaemonSynchronization();
	check_log("RunFileDaemonSynchronization",
		  "HDRManager::ProcessPlaybackCommands;HDRManager::ProcessRecordCommands;"
		  "HDRManager::ProcessSamplerCommands;FileOpener::ProcessCommands;"
		  "HDRFileReader::ProcessCommands;HDRFileWriter::ProcessCommands;"
		  "FileCloser::ProcessCommands;StreamingFileReader::ProcessCommands;"
		  "CDWorker::ProcessCommands;SamplingDaemon::ProcessCommands;");

	printf("[5] CSTGEngine destructor's exact confirmed teardown order\n");
	CSTGMidiPortManager mpm; CSTGMidiPortManager::sInstance = &mpm;
	/* The real destructor calls `operator delete()` on this (confirmed via
	 * disassembly -- CPowerOffTimer is real heap-`new`'d in Initialize()),
	 * so the mock instance must be heap-allocated too, not a stack buffer. */
	unsigned char *potBuf = (unsigned char *)::operator new(0x20);
	memset(potBuf, 0, 0x20);
	*(void **)(potBuf + 0x18) = (void *)0x1234;	/* fake mutex pointer */
	CPowerOffTimer::sInstance = (CPowerOffTimer *)potBuf;
	CSTGMessageProcessor msgProc;
	/* The real ctor only sets sInstance (sec 10.147) -- +0x64/+0x68
	 * (the ctor's own confirmed-but-not-reconstructed CEffectorDatabase*
	 * and second pointer) are otherwise uninitialized stack bytes here;
	 * zero them so the now-real destructor's null checks make it a
	 * genuine, safe no-op rather than dereferencing stack garbage. */
	memset((void *)&msgProc, 0, sizeof(msgProc));
	CSTGMessageProcessor::sInstance = &msgProc;
	MockAudioDriverInterface *adi = new MockAudioDriverInterface();
	CSTGAudioDriverInterface::sInstance = adi;
	CSTGAudioManager *am = new CSTGAudioManager();
	CSTGAudioManager::sInstance = am;
	CSTGVoiceAllocator va; CSTGVoiceAllocator::sInstance = &va;
	CLoadBalancer *lb = new CLoadBalancer();
	CLoadBalancer::sInstance = lb;
	CSTGVoiceModelManager::sInstance = &vmm;	/* re-arm from test [1] */

	CSTGEngine *heapEngine = new CSTGEngine();
	g_log[0] = '\0';	/* clear the constructor's own log noise (none expected, but be safe) */
	delete heapEngine;
	/* ~CSTGVoiceModelManager()/~CSTGMessageProcessor()/~CSTGVoiceAllocator()
	 * are now real (sec 10.147): the first fires the reused vmm entry's
	 * own trapped vtable slot 1 ("VoiceModelManager::entryDtor" in place
	 * of the old flat "~CSTGVoiceModelManager" mock log); the second is a
	 * genuine silent no-op against msgProc's zeroed +0x64/+0x68 fields (no
	 * log entry at all, since none of its real logic calls log_call); the
	 * third calls the SAME rtwrap_pthread_mutex_destroy/rtwrap_free mocks
	 * CPowerOffTimer's own teardown already uses, so that pair now shows up
	 * a second time in place of the old flat "~CSTGVoiceAllocator" mock log. */
	check_log("~CSTGEngine",
		  "~CSTGMidiPortManager;rtwrap_pthread_mutex_destroy;rtwrap_free;"
		  "VoiceModelManager::entryDtor;~CSTGAudioDriverInterface;"
		  "~CSTGAudioManager;rtwrap_pthread_mutex_destroy;rtwrap_free;~CLoadBalancer;");

	free(globalBuf);

	printf("======================================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
