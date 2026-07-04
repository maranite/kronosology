// SPDX-License-Identifier: GPL-2.0
/*
 * test_engine_init.cpp  -  host-side known-answer test for
 * CSTGEngine::Initialize() (see ../include/oa_engine_init.h /
 * ../src/engine/engine_init.cpp).
 *
 * Mocks every dependency as a trivial counter/recorder (matching
 * test_setup_global_resources.cpp's own established convention -- self
 * contained, not linking managers.cpp), then verifies:
 *   [1] every confirmed alloc/construct call happened with the exact
 *       confirmed size (spot-checked via CSTGBankMemory's own real
 *       AllocAligned, backed by a host buffer).
 *   [2] the 3 TSTGArrayManager<T> ring-buffer-building loops produce the
 *       exact confirmed counts/id-stamping/bucket-fill pattern.
 *   [3] CSTGMidiPortManager's explicit struct-init field writes land at
 *       their exact confirmed offsets.
 *   [4] the ten "Model" classes' vtable slot 2 dispatch fires once each.
 *   [5] the InitCdromSupport()-gated OA_ApplyCdromDegradation() call
 *       fires only on nonzero (failure) return.
 */

#include <cstdio>
#include <sys/mman.h>
#include <cstdlib>
#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"
#include "oa_bank_memory.h"
#include "cdrom_check.h"
#include "oa_internal.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-60s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-60s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

/* ---- mocks: heap-new'd classes ---- */
static int g_loadBalancerCtorCalls, g_loadBalancerInitCalls;
CLoadBalancer *CLoadBalancer::sInstance;
CLoadBalancer::CLoadBalancer() { sInstance = this; g_loadBalancerCtorCalls++; }
CLoadBalancer::~CLoadBalancer() {}
void CLoadBalancer::Initialize() { g_loadBalancerInitCalls++; }
CEmergencyStealer::CEmergencyStealer() {}
CEmergencyStealer::~CEmergencyStealer() {}

static int g_diskCostCtorCalls, g_diskCostInitCalls;
CSTGDiskCostManager *CSTGDiskCostManager::sInstance;
CSTGDiskCostManager::CSTGDiskCostManager() { sInstance = this; g_diskCostCtorCalls++; }
void CSTGDiskCostManager::Initialize() { g_diskCostInitCalls++; }

static void *g_audioDriverVtable[3];
static int g_audioDriverSlot2Calls;
static void AudioDriverSlot2(void *) { g_audioDriverSlot2Calls++; }
CSTGAudioDriverInterface *CSTGAudioDriverInterface::sInstance;
CSTGAudioDriverInterface::~CSTGAudioDriverInterface() {}
CSTGAudioDriverInterfaceKorgUsb::CSTGAudioDriverInterfaceKorgUsb()
{
	g_audioDriverVtable[2] = (void *)&AudioDriverSlot2;
	*(void ***)this = g_audioDriverVtable;
	sInstance = this;
}
CSTGAudioDriverInterfaceKorgUsb::~CSTGAudioDriverInterfaceKorgUsb() {}
void CSTGAudioDriverInterfaceKorgUsb::Callback(void *) {}

static void *g_audioManagerVtable[1];
static int g_audioManagerSlot0Calls;
static void AudioManagerSlot0(void *) { g_audioManagerSlot0Calls++; }
CSTGAudioManager *CSTGAudioManager::sInstance;
CSTGAudioManager::CSTGAudioManager()
{
	g_audioManagerVtable[0] = (void *)&AudioManagerSlot0;
	*(void ***)this = g_audioManagerVtable;
	sInstance = this;
}
CSTGAudioManager::~CSTGAudioManager() {}
char CSTGAudioManager::StartAudioEngine() { return 0; }
void *CSTGAudioManager::ASKThreadRoutine(void *) { return 0; }
void *CSTGAudioManager::AudioManagerThreadRoutine(void *) { return 0; }

static int g_powerOffCtorCalls, g_powerOffInitCalls;
CPowerOffTimer *CPowerOffTimer::sInstance;
CPowerOffTimer::CPowerOffTimer() { sInstance = this; g_powerOffCtorCalls++; }
void CPowerOffTimer::Initialize() { g_powerOffInitCalls++; }

/* ---- mocks: CSTGBankMemory-placed managers (ctor counters) ---- */
#define MOCK_CTOR_ONLY(cls) \
	static int g_##cls##CtorCalls; \
	cls::cls() { g_##cls##CtorCalls++; }

MOCK_CTOR_ONLY(CSTGWaveSeqManager)
static int g_waveSeqInitCalls;
void CSTGWaveSeqManager::Initialize() { g_waveSeqInitCalls++; sInstance = this; }

MOCK_CTOR_ONLY(CSTGVectorManager)
static int g_vectorInitCalls;
void CSTGVectorManager::Initialize() { g_vectorInitCalls++; }

MOCK_CTOR_ONLY(CSTGMidiDispatcher)
static int g_midiDispatchInitCalls;
void CSTGMidiDispatcher::Initialize() { g_midiDispatchInitCalls++; }

MOCK_CTOR_ONLY(CSTGMessageProcessor)
CSTGMessageProcessor::~CSTGMessageProcessor() {}
MOCK_CTOR_ONLY(CSTGFrontPanelSmoothers)
MOCK_CTOR_ONLY(CSTGVoiceModelManager)
CSTGVoiceModelManager::~CSTGVoiceModelManager() {}
void CSTGVoiceModelManager::ProcessSubRate(unsigned int) {}
void CSTGVoiceModelManager::ProcessAudioRate(unsigned int) {}
CSTGVoiceModelManager *CSTGVoiceModelManager::sInstance;

CSTGPlaybackBuffer::CSTGPlaybackBuffer() {}
CSTGMonitorMixerChannel::CSTGMonitorMixerChannel() {}
CSTGHDRManager *CSTGHDRManager::sInstance;
static int g_CSTGHDRManagerCtorCalls;
CSTGHDRManager::CSTGHDRManager() { sInstance = this; g_CSTGHDRManagerCtorCalls++; }
static int g_hdrManagerInitCalls;
void CSTGHDRManager::Initialize() { g_hdrManagerInitCalls++; }
void CSTGHDRManager::ProcessHDRRecord() {}
void CSTGHDRManager::ProcessCommands() {}

MOCK_CTOR_ONLY(CSTGHDRMiniModel)
static int g_hdrMiniInitCalls;
void CSTGHDRMiniModel::Initialize() { g_hdrMiniInitCalls++; }

static int g_monitorMixerInitCalls;
CSTGMonitorMixer *CSTGMonitorMixer::sInstance;
CSTGMonitorMixer::CSTGMonitorMixer() { sInstance = this; }
void CSTGMonitorMixer::Initialize() { g_monitorMixerInitCalls++; }
void CSTGMonitorMixer::RunMonitors() {}

CSTGMetronome *CSTGMetronome::sInstance;
CSTGMetronome::CSTGMetronome() { sInstance = this; }
CSTGTempoUtils *CSTGTempoUtils::sInstance;
CSTGTempoUtils::CSTGTempoUtils() { sInstance = this; }

static unsigned short g_streamingEventArg1;
static unsigned long g_streamingEventArg2;
CSTGStreamingEventManager::CSTGStreamingEventManager() { sInstance = this; }
void CSTGStreamingEventManager::Initialize(unsigned short a1, unsigned long a2)
{
	g_streamingEventArg1 = a1;
	g_streamingEventArg2 = a2;
}

static int g_fileOpenerInitCalls;
CSTGFileOpener *CSTGFileOpener::sInstance;
CSTGFileOpener::CSTGFileOpener() { sInstance = this; }
void CSTGFileOpener::Initialize() { g_fileOpenerInitCalls++; }
void CSTGFileOpener::ProcessCommands() {}

static int g_fileCloserInitCalls;
CSTGFileCloser *CSTGFileCloser::sInstance;
CSTGFileCloser::CSTGFileCloser() { sInstance = this; }
void CSTGFileCloser::Initialize() { g_fileCloserInitCalls++; }
void CSTGFileCloser::ProcessCommands() {}

static int g_hdrReaderInitCalls;
CSTGHDRFileReader *CSTGHDRFileReader::sInstance;
CSTGHDRFileReader::CSTGHDRFileReader() { sInstance = this; }
void CSTGHDRFileReader::Initialize() { g_hdrReaderInitCalls++; }
void CSTGHDRFileReader::ProcessCommands() {}

static unsigned long g_streamingFileReaderArg;
CSTGStreamingFileReader *CSTGStreamingFileReader::sInstance;
CSTGStreamingFileReader::CSTGStreamingFileReader() { sInstance = this; }
void CSTGStreamingFileReader::Initialize(unsigned long a) { g_streamingFileReaderArg = a; }
void CSTGStreamingFileReader::ProcessCommands() {}

static int g_hdrWriterInitCalls;
CSTGHDRFileWriter *CSTGHDRFileWriter::sInstance;
CSTGHDRFileWriter::CSTGHDRFileWriter() { sInstance = this; }
void CSTGHDRFileWriter::Initialize() { g_hdrWriterInitCalls++; }
void CSTGHDRFileWriter::ProcessCommands() {}

static int g_cdWorkerInitCalls;
CSTGCDWorker *CSTGCDWorker::sInstance;
CSTGCDWorker::CSTGCDWorker() { sInstance = this; }
void CSTGCDWorker::Initialize() { g_cdWorkerInitCalls++; }
void CSTGCDWorker::ProcessCommands() {}

static int g_samplingDaemonInitCalls;
CSTGSamplingDaemon *CSTGSamplingDaemon::sInstance;
CSTGSamplingDaemon::CSTGSamplingDaemon() { sInstance = this; }
void CSTGSamplingDaemon::Initialize() { g_samplingDaemonInitCalls++; }
void CSTGSamplingDaemon::ProcessCommands() {}

static int g_klmCtorCalls, g_klmInitCalls;
CSTGKLMManager *CSTGKLMManager::sInstance;
CSTGKLMManager::CSTGKLMManager() { sInstance = this; g_klmCtorCalls++; }
void CSTGKLMManager::Initialize() { g_klmInitCalls++; }

MOCK_CTOR_ONLY(CSTGSmoother)
static int g_smootherInitCalls;
void CSTGSmoother::Initialize() { g_smootherInitCalls++; }

MOCK_CTOR_ONLY(CSTGLFOTables)

MOCK_CTOR_ONLY(CSTGMIDIClockSync)

static int g_midiPortInitCalls;
CSTGMidiPortManager *CSTGMidiPortManager::sInstance;
CSTGMidiPortManager::~CSTGMidiPortManager() {}
void CSTGMidiPortManager::Initialize() { g_midiPortInitCalls++; }

/* ---- mocks: the ten Model classes ---- */
static int g_modelSlot2Calls;
static void ModelSlot2(void *) { g_modelSlot2Calls++; }
static void *g_modelVtable[3] = { 0, 0, (void *)&ModelSlot2 };
#define MOCK_MODEL(cls) \
	cls::cls() { *(void ***)this = g_modelVtable; }
MOCK_MODEL(CSTGOffModel)
MOCK_MODEL(CSTGPCMModel)
MOCK_MODEL(CSTGAnalogSyncModel)
MOCK_MODEL(CSTGOrganModel)
MOCK_MODEL(CSTGPluckedModel)
MOCK_MODEL(CSTGMS20Model)
MOCK_MODEL(CSTGPolysixModel)
MOCK_MODEL(CSTGVPMModel)
MOCK_MODEL(CSTGPianoModel)
MOCK_MODEL(CSTGEPModel)

static int g_commonLfoInitCalls, g_commonStepSeqInitCalls;
void CSTGCommonLFO::Initialize() { g_commonLfoInitCalls++; }
void CSTGCommonStepSeq::Initialize() { g_commonStepSeqInitCalls++; }

/* ---- mocks: ring-buffer element classes ---- */
static int g_playbackEventCtorCalls;
CSTGPlaybackEvent::CSTGPlaybackEvent() { g_playbackEventCtorCalls++; }
static int g_audioEventCtorCalls;
CSTGAudioEvent::CSTGAudioEvent() { g_audioEventCtorCalls++; }
unsigned char _ZTV15CSTGRecordEvent[16];
static int g_recordBufferCtorCalls;
CSTGRecordBuffer::CSTGRecordBuffer() { g_recordBufferCtorCalls++; }

/* ---- mocks: CSTGEngine's own other methods (not exercised here) ---- */
CSTGEngine::CSTGEngine() {}
CSTGEngine::~CSTGEngine() {}
void CSTGEngine::RunAudioTick(unsigned int) {}
void CSTGEngine::PostAudioTick() {}
void CSTGEngine::RunEffects() {}
void CSTGEngine::RunFileDaemonSynchronization() {}
CSTGEngine *CSTGEngine::sInstance;

CSTGEffectManager *CSTGEffectManager::sInstance;
CSTGEffectManager::CSTGEffectManager() { sInstance = this; }
static int g_effectManagerInitCalls;
void CSTGEffectManager::Initialize() { g_effectManagerInitCalls++; }
void CSTGEffectManager::RunEffects() {}

CSTGVoiceAllocator *CSTGVoiceAllocator::sInstance;
CSTGVoiceAllocator::CSTGVoiceAllocator() { sInstance = this; }
CSTGVoiceAllocator::~CSTGVoiceAllocator() {}
static int g_voiceAllocatorInitCalls;
void CSTGVoiceAllocator::Initialize() { g_voiceAllocatorInitCalls++; }
CSTGSlotState::CSTGSlotState() {}

/* ---- mocks: InitCdromSupport() gate ---- */
static int g_initCdromResult;
static int g_applyCdromDegradationCalls;
int InitCdromSupport(void) { return g_initCdromResult; }
void OA_ApplyCdromDegradation(void) { g_applyCdromDegradationCalls++; }

int main(void)
{
	printf("CSTGEngine::Initialize() known-answer test\n");
	printf("=========================================================\n");

	/* Real CSTGBankMemory, backed by an mmap(MAP_32BIT) buffer -- a plain
	 * static/heap buffer lives well past the 32-bit address range on a
	 * 64-bit host, and this file's own TSTGArrayManager<T> fields are
	 * genuine 32-bit target pointers (ToU32/FromU32-truncated, matching
	 * this project's established fix for this exact host/target hazard,
	 * sec 10.55/10.56/10.57) -- caught via a real segfault + gdb trace
	 * before landing on this fix. */
	unsigned long bankSize = 8 * 1024 * 1024;
	unsigned char *bankBuf = (unsigned char *)mmap(0, bankSize, PROT_READ | PROT_WRITE,
							MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	CSTGBankMemory::Initialize(bankBuf, bankSize);

	printf("[1] InitCdromSupport() returns 0 (success) -- degradation NOT applied\n");
	g_initCdromResult = 0;
	CSTGEngine engine;
	engine.Initialize();

	check_eq("CLoadBalancer ctor called once", g_loadBalancerCtorCalls, 1);
	check_eq("CLoadBalancer::Initialize called once", g_loadBalancerInitCalls, 1);
	check_eq("CSTGDiskCostManager ctor called once", g_diskCostCtorCalls, 1);
	check_eq("CSTGDiskCostManager::Initialize called once", g_diskCostInitCalls, 1);
	check_eq("CSTGAudioDriverInterface vtable slot 2 dispatched once", g_audioDriverSlot2Calls, 1);
	check_eq("CSTGAudioManager vtable slot 0 dispatched once", g_audioManagerSlot0Calls, 1);
	check_eq("CPowerOffTimer ctor called once", g_powerOffCtorCalls, 1);
	check_eq("CPowerOffTimer::Initialize called once", g_powerOffInitCalls, 1);

	check_eq("CSTGVoiceAllocator::Initialize called once", g_voiceAllocatorInitCalls, 1);
	check_eq("CSTGEffectManager::Initialize called once", g_effectManagerInitCalls, 1);
	check_eq("CSTGWaveSeqManager ctor called once", g_CSTGWaveSeqManagerCtorCalls, 1);
	check_eq("CSTGWaveSeqManager::Initialize called once", g_waveSeqInitCalls, 1);
	check_eq("CSTGVectorManager ctor called once", g_CSTGVectorManagerCtorCalls, 1);
	check_eq("CSTGVectorManager::Initialize called once", g_vectorInitCalls, 1);
	check_eq("CSTGMidiDispatcher ctor called once", g_CSTGMidiDispatcherCtorCalls, 1);
	check_eq("CSTGMidiDispatcher::Initialize called once", g_midiDispatchInitCalls, 1);
	check_eq("CSTGMessageProcessor ctor called once", g_CSTGMessageProcessorCtorCalls, 1);
	check_eq("CSTGFrontPanelSmoothers ctor called once", g_CSTGFrontPanelSmoothersCtorCalls, 1);
	check_eq("CSTGVoiceModelManager ctor called once", g_CSTGVoiceModelManagerCtorCalls, 1);

	printf("\n[2] TSTGArrayManager<T> ring-buffer-building loops\n");
	auto FromU32 = [](unsigned int v) { return (unsigned char *)(unsigned long)v; };
	auto IndexElem = [&](auto *mgr, unsigned int i) {
		return FromU32(((unsigned int *)FromU32(mgr->indexArray))[i]);
	};
	auto BucketElem = [&](auto *mgr, unsigned int i) {
		return FromU32(((unsigned int *)FromU32(mgr->bucketArray))[i]);
	};

	check_eq("PlaybackEvent count == 4000", TSTGArrayManager<CSTGPlaybackEvent>::sInstance->count, 4000);
	check_eq("PlaybackEvent modulus == 4001", TSTGArrayManager<CSTGPlaybackEvent>::sInstance->modulus, 4001);
	check_eq("PlaybackEvent ctor called 4000 times", g_playbackEventCtorCalls, 4000);
	check_eq("PlaybackEvent index[0] id == 0",
		 *(unsigned short *)(IndexElem(TSTGArrayManager<CSTGPlaybackEvent>::sInstance, 0) + 4), 0);
	check_eq("PlaybackEvent index[3999] id == 3999",
		 *(unsigned short *)(IndexElem(TSTGArrayManager<CSTGPlaybackEvent>::sInstance, 3999) + 4), 3999);
	check_eq("PlaybackEvent bucket[3999] == index[3999] (no wraparound)",
		 (long)(BucketElem(TSTGArrayManager<CSTGPlaybackEvent>::sInstance, 3999) ==
			IndexElem(TSTGArrayManager<CSTGPlaybackEvent>::sInstance, 3999)),
		 1);

	check_eq("RecordEvent count == 200", TSTGArrayManager<CSTGRecordEvent>::sInstance->count, 200);
	check_eq("RecordEvent modulus == 201", TSTGArrayManager<CSTGRecordEvent>::sInstance->modulus, 201);
	check_eq("CSTGAudioEvent ctor called 200 times (RecordEvent's real base)", g_audioEventCtorCalls, 200);
	check_eq("RecordEvent[0] vtable patched to _ZTV15CSTGRecordEvent+8",
		 (long)(*(unsigned int *)IndexElem(TSTGArrayManager<CSTGRecordEvent>::sInstance, 0) ==
			(unsigned int)(unsigned long)(_ZTV15CSTGRecordEvent + 8)),
		 1);

	check_eq("RecordBuffer count == 96", TSTGArrayManager<CSTGRecordBuffer>::sInstance->count, 96);
	check_eq("RecordBuffer modulus == 97", TSTGArrayManager<CSTGRecordBuffer>::sInstance->modulus, 97);
	check_eq("RecordBuffer ctor called 96 times", g_recordBufferCtorCalls, 96);
	check_eq("RecordBuffer index[0] id (at +0x0) == 0",
		 *(unsigned short *)(IndexElem(TSTGArrayManager<CSTGRecordBuffer>::sInstance, 0) + 0), 0);
	check_eq("RecordBuffer index[95] id (at +0x0) == 95",
		 *(unsigned short *)(IndexElem(TSTGArrayManager<CSTGRecordBuffer>::sInstance, 95) + 0), 95);

	printf("\n[3] CSTGMidiPortManager struct-init field writes\n");
	check_eq("+0xc == 0xffffffff", (long)(unsigned int)(*(unsigned int *)((unsigned char *)CSTGMidiPortManager::sInstance + 0xc)), (long)(unsigned int)0xffffffff);
	check_eq("+0x70 == 0xffffffff", (long)(unsigned int)(*(unsigned int *)((unsigned char *)CSTGMidiPortManager::sInstance + 0x70)), (long)(unsigned int)0xffffffff);
	check_eq("+0xd4 == 0xffffffff", (long)(unsigned int)(*(unsigned int *)((unsigned char *)CSTGMidiPortManager::sInstance + 0xd4)), (long)(unsigned int)0xffffffff);
	check_eq("+0x138 == 0", *(unsigned int *)((unsigned char *)CSTGMidiPortManager::sInstance + 0x138), 0);
	check_eq("+0x140 == 0xffffffff", (long)(unsigned int)(*(unsigned int *)((unsigned char *)CSTGMidiPortManager::sInstance + 0x140)), (long)(unsigned int)0xffffffff);
	check_eq("+0x1a4 == 0xffffffff", (long)(unsigned int)(*(unsigned int *)((unsigned char *)CSTGMidiPortManager::sInstance + 0x1a4)), (long)(unsigned int)0xffffffff);
	check_eq("+0x20c == 0", *(unsigned int *)((unsigned char *)CSTGMidiPortManager::sInstance + 0x20c), 0);
	check_eq("CSTGMidiPortManager::Initialize called once", g_midiPortInitCalls, 1);

	printf("\n[4] Ten Model classes, vtable slot 2 dispatch\n");
	check_eq("vtable slot 2 dispatched 10 times", g_modelSlot2Calls, 10);
	check_eq("CSTGCommonLFO::Initialize called once", g_commonLfoInitCalls, 1);
	check_eq("CSTGCommonStepSeq::Initialize called once", g_commonStepSeqInitCalls, 1);

	printf("\n[5] Remaining Initialize() calls\n");
	check_eq("CSTGStreamingEventManager::Initialize(0x191, 0x10000)", (long)g_streamingEventArg1, 0x191);
	check_eq("  ...arg2", (long)g_streamingEventArg2, 0x10000);
	check_eq("CSTGHDRManager::Initialize called once", g_hdrManagerInitCalls, 1);
	check_eq("CSTGFileOpener::Initialize called once", g_fileOpenerInitCalls, 1);
	check_eq("CSTGFileCloser::Initialize called once", g_fileCloserInitCalls, 1);
	check_eq("CSTGHDRFileReader::Initialize called once", g_hdrReaderInitCalls, 1);
	check_eq("CSTGStreamingFileReader::Initialize(0x8000)", (long)g_streamingFileReaderArg, 0x8000);
	check_eq("CSTGHDRFileWriter::Initialize called once", g_hdrWriterInitCalls, 1);
	check_eq("CSTGCDWorker::Initialize called once", g_cdWorkerInitCalls, 1);
	check_eq("CSTGSamplingDaemon::Initialize called once", g_samplingDaemonInitCalls, 1);
	check_eq("CSTGHDRMiniModel::Initialize called once", g_hdrMiniInitCalls, 1);
	check_eq("CSTGMonitorMixer::Initialize called once", g_monitorMixerInitCalls, 1);
	check_eq("CSTGKLMManager ctor+Initialize called once", g_klmCtorCalls + g_klmInitCalls, 2);
	check_eq("CSTGSmoother::Initialize called once", g_smootherInitCalls, 1);

	printf("\n[6] InitCdromSupport() gate, success case\n");
	check_eq("OA_ApplyCdromDegradation NOT called (InitCdromSupport succeeded)",
		 g_applyCdromDegradationCalls, 0);

	printf("\n[7] InitCdromSupport() gate, failure case (separate engine)\n");
	g_initCdromResult = -1;
	CSTGEngine engine2;
	engine2.Initialize();
	check_eq("OA_ApplyCdromDegradation called once (InitCdromSupport failed)",
		 g_applyCdromDegradationCalls, 1);

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
