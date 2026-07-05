// SPDX-License-Identifier: GPL-2.0
/*
 * bar2_stubs.cpp -- Bar 2 (2026-07-02): deliberately minimal, safe stub
 * bodies for every symbol OA.ko's own init_module()/UpdateXXX/etc. call
 * chains reference that has NOT yet been individually reconstructed.
 *
 * THIS FILE IS NOT A RECONSTRUCTION PASS. Every body here is a
 * deliberate no-op/safe-default stand-in, written ONLY to let OA.ko
 * link completely and actually insmod in kronos_vm (the project's own
 * "Bar 2" goal -- see PLAN.md) -- matching the same "clearly-labeled
 * interop stub, not a claim of real behavior" precedent already used
 * for AT88VirtualChip.ko/KorgUsbAudioVirtualDriver.ko. As each of
 * these is properly reconstructed in a future pass (real disassembly,
 * real KAT), its stub body here should be DELETED, not edited in
 * place -- the real implementation belongs in its own subsystem file,
 * matching this project's established per-unit file convention.
 *
 * Determined via a direct symbol-table diff against the confirmed-
 * ground-truth OA_real.ko: of ~270 unresolved symbols in a from-
 * scratch Kbuild, ~32 are genuine external kernel/RTAI/other-module
 * dependencies that the real binary ALSO leaves undefined (these
 * auto-resolve at real insmod time once RTAI/AT88VirtualChip.ko/
 * KorgUsbAudioVirtualDriver.ko are loaded first, per the project's own
 * confirmed boot sequence, sec 10.41/10.43/10.44) -- NOT stubbed here.
 * The remainder (this file) are symbols the REAL binary defines
 * internally that this project's own reconstruction hasn't reached
 * yet.
 */

#include "oa_global.h"
#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"
#include "oa_audio_start.h"
#include "oa_comport.h"
#include "oa_crypto.h"

/* NOTE: CSTGMultisampleBank/CUUID/CSTGKLEG/CSTGPatch/
 * CSTGInstalledEXProducts stubs live in bar2_stubs_auth.cpp instead,
 * a SEPARATE translation unit -- this project has two pre-existing,
 * mutually incompatible declaration ecosystems for several shared
 * class names (oa_types.h's minimal CSTGGlobal/etc. vs oa_global.h's
 * fuller ones, already flagged sec 10.48) that cannot both be
 * included in one TU. */

/* ---- Model classes (10 "instrument model" ctors, sec 10.13/10.57) ---- */
CSTGOffModel::CSTGOffModel() {}
CSTGPCMModel::CSTGPCMModel() {}
CSTGAnalogSyncModel::CSTGAnalogSyncModel() {}
CSTGOrganModel::CSTGOrganModel() {}
CSTGPluckedModel::CSTGPluckedModel() {}
CSTGMS20Model::CSTGMS20Model() {}
CSTGPolysixModel::CSTGPolysixModel() {}
CSTGVPMModel::CSTGVPMModel() {}
CSTGPianoModel::CSTGPianoModel() {}
CSTGEPModel::CSTGEPModel() {}
void CSTGPianoModel::RescanPianoTypes() {}

/* ---- Engine subsystem managers (engine.cpp, sec 10.13/10.58) ---- */
/* CSTGVoiceModelManager::~CSTGVoiceModelManager() is real now, sec
 * 10.147 -- see managers.cpp. */
void CSTGEffectManager::Initialize() {}
void CSTGEffectManager::RunEffects() {}
void CSTGHDRManager::Initialize() {}
/* ProcessCommands()/Initialize() (CSTGMonitorMixer/CSTGHDRFileWriter/
 * CSTGSamplingDaemon/CSTGFileCloser/CSTGCDWorker) are real now, sec
 * 10.144 -- see managers.cpp. ProcessCommands() calls three still-
 * deferred siblings, stubbed below. */
void CSTGHDRManager::ProcessPlaybackCommands() {}
void CSTGHDRManager::ProcessRecordCommands() {}
void CSTGHDRManager::ProcessSamplerCommands() {}
void CSTGHDRManager::ProcessHDRRecord() {}
void CSTGMonitorMixer::RunMonitors() {}
void CSTGFileOpener::Initialize() {}
void CSTGFileOpener::ProcessCommands() {}
void CSTGFileCloser::ProcessCommands() {}
void CSTGHDRFileReader::Initialize() {}
void CSTGHDRFileReader::ProcessCommands() {}
void CSTGHDRFileWriter::ProcessCommands() {}
void CSTGStreamingFileReader::Initialize(unsigned long) {}
void CSTGStreamingFileReader::ProcessCommands() {}
/* CSTGCDWorker_InitializeBuffer is real now, sec 10.148 -- see
 * managers.cpp (right after CSTGCDWorker::Initialize(), sec 10.144). */
void CSTGCDWorker::ProcessCommands() {}
void CSTGSamplingDaemon::ProcessCommands() {}
void CSTGMidiPortManager::Initialize() {}
CSTGMidiPortManager::~CSTGMidiPortManager() {}
void CSTGMidiPortManager::WriteSTGMidiOutQueue(const unsigned char *, unsigned int) {}
void CSTGMidiPortManager::NotifyNKS4TestMode() {}
void CSTGVoiceAllocator::Initialize() {}
/* CSTGVoiceAllocator::~CSTGVoiceAllocator()/CSTGMessageProcessor::
 * ~CSTGMessageProcessor() are real now, sec 10.147 -- see managers.cpp. */
void CSTGAudioBusManager::MixPerformanceOutputs() {}
void CSTGAudioBusManager::LRBusIndivMirror() {}
/* CSTGAudioEvent::CSTGAudioEvent() is real now, sec 10.149 -- see
 * engine_init.cpp. */
/* CSTGAudioManager::ASKThreadRoutine(void*)/AudioManagerThreadRoutine(void*)
 * and CSTGAudioThread::AudioTickLoopRoutine(void*) are real now, sec
 * 10.149 -- see src/init/audio_start.cpp. AudioTickLoopRoutine(void*)
 * itself forwards into a new no-arg overload, confirmed real but
 * deliberately deferred (own body, .text+0x5dfa0, 141 bytes,
 * substantially larger -- see oa_audio_start.h's own header comment). */
void CSTGAudioThread::AudioTickLoopRoutine() {}
/* SKMain_Run() -- confirmed real (`_Z10SKMain_Runv`, .text+0x340ca0),
 * called from the now-real ASKThreadRoutine(void*) above. Declared
 * plain `extern "C"` matching the SAME-family SKMain_Initialize's own
 * already-established convention (sec 10.145) -- an internal-
 * consistency choice for THIS reconstruction's own linkage, not a claim
 * that the real binary's own SKMain_Run happens to be un-mangled (it
 * isn't: the real symbol is `_Z10SKMain_Runv`, a plain C++ free
 * function -- irrelevant here since this project never links against
 * OA_real.ko directly, only needs its own call sites to agree). */
extern "C" void SKMain_Run() {}
void CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors() {}
/* CSTGMultisampleBankManager::Initialize()/CSTGPCMPrecacheManager::
 * Initialize() are both real now, sec 10.149/10.144 -- see
 * setup_global_resources.cpp. */
void CSTGPCMPrecacheManager::AfterProcess() {}
void CSTGPCMPrecacheManager::Reset(bool, bool, unsigned long) {}

/* ---- Remaining engine/manager/model stubs, batch 2 ---- */
/* CEmergencyStealer::~CEmergencyStealer() is real now, sec 10.148 -- see
 * managers.cpp (right after CEmergencyStealer::CEmergencyStealer()). */
/* CEffectorDatabase::~CEffectorDatabase() is real now, sec 10.148 -- see
 * managers.cpp. Its own ctor/Register()/etc. are still NOT reconstructed,
 * own body far out of scope for this pass. */
/* CSTGASK::Initialize() is real now, sec 10.145 (see
 * setup_global_resources.cpp) -- a pure forward to SKMain_Initialize(),
 * confirmed real, deliberately deferred (own body substantially larger,
 * not reconstructed in this pass). */
extern "C" void SKMain_Initialize(void *) {}
/* CSTGAudioInput's own ctor + 9 UpdateXXX methods reconstructed for
 * real, sec 10.80 -- see src/engine/global.cpp. */
void CSTGAudioInputMixerBase::SetHDRBus(unsigned int, int) {}
void CSTGAudioInputMixerBase::SetFXCtrlBus(unsigned int, int) {}
void CSTGAudioInputMixerBase::SetOutputBus(unsigned int, int) {}
void CSTGAudioInputMixerBase::SetPan(unsigned int, float) {}
void CSTGAudioInput::UseSettings() {}
/* Sec 10.97's own confirmed-real, deliberately deferred externs.
 * CSetListSlot::Activate is now real (sec 10.141). */
/* SendPerfChangeToMidiOut is now real, sec 10.98 -- see
 * src/engine/global.cpp. Its own confirmed-real, deliberately deferred
 * dependencies still need link-satisfying mocks here. */
void USTGAliasBankTypes::ConvertAliasPgmBankToMidiBank(int, char &, char &) {}
void USTGAliasBankTypes::ConvertCombiBankToMidiBank(int, char &, char &) {}
/* Sec 10.99's own confirmed-real, deliberately deferred externs. */
void USTGAliasBankTypes::ConvertMidiBankToCombiBank(char, char, int &) {}
void USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(char, char, int &) {}
/* SKSTGGate_ShouldSyncExternalClock() is real now, sec 10.148 -- see
 * src/engine/sk_stg_gate.cpp (a genuinely new class, CTimerManager,
 * declared minimally/opaquely there -- most of its own dozen-plus
 * methods are NOT reconstructed in this pass). CTimerManager::
 * ShouldSyncExternalClock() itself is a real symbol DEFINED INSIDE
 * OA_real.ko (`.text+0x347210`, a `T` symbol, confirmed via nm -- NOT an
 * external kernel/other-module dependency), so it needs the same
 * trivial link-satisfying stub body as any other confirmed-real,
 * deliberately-deferred OA-internal symbol (matching SKMain_Initialize
 * just above), not a genuinely external dependency left permanently
 * unresolved. */
bool CTimerManager::ShouldSyncExternalClock() { return false; }
/* CSTGParamsOwner::ValidateParamChange (sec 10.92) -- confirmed real,
 * deliberately deferred; see src/engine/global.cpp for the real
 * callers now reconstructed. */
void CSTGParamsOwner::ValidateParamChange(CSTGMessageContext &, unsigned long, const CValue &) {}
void CSTGControllerRTData::SetAudioInSolo(unsigned int, bool) {}
void CSTGControllerRTData::ResetSendKnobsJumpCatch() {}
void CSTGComPort::RTAIInterruptHandler(unsigned int, void *) {}
CSTGCombi::CSTGCombi() {}
CSTGControllerRTData::CSTGControllerRTData() {}
/* CSTGControllerRTData::Initialize()/RequestAnalogInputPositions()
 * reconstructed for real, sec 10.88 -- see
 * src/engine/controller_rt_data_init.cpp. */
void CSTGControllerRTData::OnExtModeKnobAssignChange(unsigned int) {}
void CSTGControllerRTData::OnExtModeSetChange() {}
void CSTGControllerInfo::SendUnsolicitedUIParam(unsigned int, unsigned int, long, int) {}
void CSTGControllerRTData::OnPerformanceActivate(CSTGPerformance &) {}
void CLoadBalancer::BalanceStaticLoad() {}
void CSTGSmoother::FinalizeSmoother(void *, bool) {}
void CSTGChannelValues::Reset() {}
void CSTGChannelValues::SetControllerValue(unsigned char, const CSTGControllerValue &) {}
void CSTGControllerRTData::OnExtModeSliderAssignChange(unsigned int) {}
/* Sec 10.101's own confirmed-real, deliberately deferred externs. */
void CSTGControllerRTData::HandleControllerChange(int, unsigned char, bool, bool) {}
void CSTGControllerInfo::SetPerfSwitch(int, bool) {}
void CSTGControllerRTData::ResetKnobsJumpCatch() {}
void CSTGControllerRTData::ResetSlidersJumpCatch() {}
void CSTGControllerRTData::ResetRTKKnobSmoothers() {}
void CSTGControllerRTData::SetControllerAssignment(void *, signed char, bool) {}
CSTGControllerRTData *CSTGControllerRTData::sInstance;
CSTGDrumKitData::CSTGDrumKitData() {}
CSTGFrontPanelSmoothers::CSTGFrontPanelSmoothers() {}
void CSTGGlobal::InitializePerformances() {}
CSTGHDRMiniModel::CSTGHDRMiniModel() {}
void CSTGHDRMiniModel::Initialize() {}
unsigned int CSTGHeapManager::Alloc(unsigned int) { return 0; }
/* CSTGHeldKeyList::Reset() reconstructed for real, sec 10.82 -- see
 * src/engine/global.cpp. */
CSTGLFOTables::CSTGLFOTables() {}
CSTGMIDIClockSync::CSTGMIDIClockSync() {}
void CSTGMidiDispatcher::HandleController(unsigned char, unsigned char, unsigned char, int, int) {}
void CSTGMidiDispatcher::ResetAllControllers(unsigned char, bool) {}
unsigned int CSTGMidiQueue::GetNumWritableBytes() const { return 0; }
/* CSTGMidiQueue::AllocReader() reconstructed for real, sec 10.82 -- see
 * src/engine/global.cpp. CSTGMidiQueueWriter::Write() reconstructed
 * for real, sec 10.83 -- see src/engine/midi_queue_writer.cpp (its own
 * separate translation unit, not global.cpp -- test_global.cpp's own
 * mock for this symbol is load-bearing for ~10 other UpdateXXX
 * assertions there, so the real body deliberately lives somewhere
 * those tests don't link, matching this project's per-unit file
 * convention). */
/* placeholder-removed-below -- see
 * src/engine/global.cpp. CSTGPerformance::IsCurrentlyActive() is real
 * now too, sec 10.144 -- see managers.cpp. */
void CSTGPerformanceVarsManager::Initialize() {}
unsigned char CSTGPerformanceVarsManager::sInstance[12];
CSTGPlaybackEvent::CSTGPlaybackEvent() {}
CSTGProgram::CSTGProgram() {}
/* CSTGProgramSlot/CSTGProgramModeProgramSlot/CSTGProgramModeDrumTrackSlot's
 * ctors + Initialize()/OnUpdateGlobalMidiChannel/
 * OnUpdateProgramDrumTrackMidiChannel all reconstructed for real, sec
 * 10.81/10.125/10.133 -- see src/engine/global.cpp. */
CSTGProgramSlot::CSTGProgramSlot() {}
/* IsActive()/AccessActiveSlotVoiceData()/HasActiveSlotVoiceData()/
 * HasActiveVoices() reconstructed for real, sec 10.142 -- see
 * src/engine/global.cpp. */
void CSTGProgramSlot::ChangeProgram(CSTGProgram *) {}
/* CSTGRecordBuffer::CSTGRecordBuffer() is real now, sec 10.148 -- see
 * src/engine/engine_init.cpp (also corrects a real bug that promotion
 * uncovered: this class's true size is 0x301c bytes, not the 0x38 this
 * project had assumed before its ctor was ever disassembled). */
CSTGSamplingInterface::CSTGSamplingInterface() {}
CSTGSequence::CSTGSequence() {}
CSTGSlotVoiceData::CSTGSlotVoiceData() {}
void CSTGSlotVoiceData::Initialize(unsigned short) {}
void CSTGSlotVoiceData::RunVoiceModelFeedback() {}
void CSTGSlotVoiceData::UpdateGlobalTune(float) {}
/* Sec 10.92's own confirmed-real, deliberately deferred externs.
 * EmergencyFreeAllVoices is now real (sec 10.138). */
void CSTGSlotVoiceData::FreeSlotVoiceData(bool) {}
/* CSTGVoiceAllocator::EmergencyFreeVoiceList(void*) is real now, sec
 * 10.149 -- see managers.cpp. Its own two newly-discovered confirmed-
 * real, deliberately deferred siblings (own bodies not reconstructed
 * in this pass): */
void CSTGVoiceAllocator::FreeVoice(CSTGVoice *) {}
void CSTGVoiceAllocator::DoPendingMoveVoices() {}
/* Sec 10.93's own confirmed-real, deliberately deferred externs. */
void CSTGSlotVoiceData::RunVoiceModelStaticFront(unsigned int) {}
void CSTGSlotVoiceData::RunVoiceModelStaticBack(unsigned int) {}
/* Sec 10.94's own confirmed-real, deliberately deferred externs.
 * Steal is now real (sec 10.140). */
void CSTGSlotVoiceData::GetTotalStaticCosts(unsigned long *, unsigned long *) const {}
void CSTGVoiceAllocator::StealVoiceList(void *) {}
/* CSTGSmoother::CSTGSmoother() ctor still deliberately deferred (the
 * real body constructs 320 sub-objects with 4 different embedded
 * MessageContext vtable relocations each -- a much larger task, out of
 * scope for this pass). Initialize() reconstructed for real, sec
 * 10.86 -- see src/engine/smoother_init.cpp. */
CSTGSmoother::CSTGSmoother() {}
void CSTGSmoother::CancelAllSmoothers() {}
/* Sec 10.95's own confirmed-real, deliberately deferred externs. */
void CSTGSmoother::FinalizeAllSmoothers() {}
void CSTGPerformanceVars::SetIsDying() {}
void CSTGPerformanceVars::EnterActivatingState() {}
CSTGStreamingEventManager::CSTGStreamingEventManager() {}
void CSTGStreamingEventManager::Initialize(unsigned short, unsigned long) {}
/* CSTGVectorEGBase::CSTGVectorEGBase() is real now, sec 10.148 -- see
 * src/engine/vector_eg_ctors.cpp (also corrects a real speculative claim
 * in oa_engine_init.h's own header comment, sec 10.66 -- see there). */
void CSTGVoiceAllocator::StealAllVoices() {}
/* CSTGWaveSeqData::Initialize()/CSetListBank::Initialize() reconstructed
 * for real, sec 10.84 -- see src/engine/global.cpp. */
CSTGWaveSeqGenerator::CSTGWaveSeqGenerator() {}
void CSTGWaveSeqGenerator::Init() {}
CSTGWaveSequence::CSTGWaveSequence() {}
CSetList::CSetList() {}
void CSetList::Activate() {}
/* CStartupFile::CStartupFile(const char*)/~CStartupFile() are real now,
 * sec 10.148 -- see src/engine/startup_file.cpp. */
/* USTGAliasBankTypes::InitializeAliasBanks() reconstructed for real,
 * sec 10.85 -- see src/engine/global.cpp. */

/* ---- Vtables -- matching the ALREADY-DECLARED `extern "C" unsigned
 * char _ZTVxxx[]` type used elsewhere in this project (sec 10.58/10.60/
 * 10.66's own "extern C byte-array trick"), sized to match OA_real.ko's
 * own confirmed real vtable byte sizes (readelf ground truth). Left
 * zero-initialized (a NULL vtable slot 0) rather than populated with
 * real function pointers -- sufficient for Bar 2 (OA.ko linking and
 * insmod'ing); if any of these vtables are ever genuinely DISPATCHED
 * through before their real virtual methods are reconstructed, that
 * would show up as a real crash to investigate at that point, not
 * silently papered over here. */
extern "C" unsigned char _ZTV16CSTGAudioManager[20];
unsigned char _ZTV16CSTGAudioManager[20];
extern "C" unsigned char _ZTV14CSTGVectorEGCC[12];
unsigned char _ZTV14CSTGVectorEGCC[12];
extern "C" unsigned char _ZTV17CSTGVectorEGXOnly[12];
unsigned char _ZTV17CSTGVectorEGXOnly[12];
extern "C" unsigned char _ZTV14CSTGVectorEGXY[12];
unsigned char _ZTV14CSTGVectorEGXY[12];
/* _ZTV16CSTGVectorEGBase -- needed now that CSTGVectorEGBase::
 * CSTGVectorEGBase() is real (sec 10.148, vector_eg_ctors.cpp) and
 * references it directly, same placeholder treatment as its three
 * derived siblings just above. */
extern "C" unsigned char _ZTV16CSTGVectorEGBase[12];
unsigned char _ZTV16CSTGVectorEGBase[12];
unsigned char _ZTV15CSTGRecordEvent[40];
/* _ZTV14CSTGAudioEvent -- needed now that CSTGAudioEvent::CSTGAudioEvent()
 * is real (sec 10.149, engine_init.cpp) and references it directly, same
 * 40-byte confirmed size (readelf) as its own derived sibling above. */
unsigned char _ZTV14CSTGAudioEvent[40];

/* STGAPIFrontPanelStatus::sInstance -- confirmed real static pointer,
 * already set by setup_global_resources.cpp; definition (storage) not
 * yet homed anywhere. */
unsigned char *STGAPIFrontPanelStatus::sInstance;
