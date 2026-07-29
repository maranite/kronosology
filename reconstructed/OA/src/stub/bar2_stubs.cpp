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
#include "oa_control_msg_handler.h"
#include "oa_keybed_init.h"

/* NOTE: CSTGMultisampleBank/CUUID/CSTGKLEG/CSTGPatch/
 * CSTGInstalledEXProducts stubs live in bar2_stubs_auth.cpp instead,
 * a SEPARATE translation unit -- this project has two pre-existing,
 * mutually incompatible declaration ecosystems for several shared
 * class names (oa_types.h's minimal CSTGGlobal/etc. vs oa_global.h's
 * fuller ones, already flagged sec 10.48) that cannot both be
 * included in one TU. */

/* ---- Model classes (10 "instrument model" ctors, sec 10.13/10.57) ----
 * All ten ctors are real now, batch 42 -- see src/engine/voice_models.cpp
 * (also the new CSTGVoiceModel base class + CSTGVoiceModelManager::Register,
 * oa_engine_init.h/oa_engine.h). Each model's own Initialize()/
 * ProcessSubRate()/ProcessAudioRate() (vtable slots 2/18/19 -- the only
 * three of 21 real virtual methods any currently-reachable code in this
 * project dispatches) are also real for CSTGOffModel (confirmed literally
 * 1-byte `ret` bodies in ground truth, see voice_models.cpp); the same
 * three methods on the other nine models are confirmed real, substantial
 * (332-2097 bytes) genuine per-model DSP init/audio-tick bodies -- out of
 * scope per the sec 10.185 policy, given the safe no-op stand-ins right
 * below (matching the CSetListEQ::SetBand/CSTGControllerInfo::SetPerfSwitch
 * precedent: the CALLER -- here, each model's own real ctor installing a
 * correctly-shaped vtable, plus the already-real CSTGVoiceModelManager::
 * ProcessSubRate/ProcessAudioRate dispatch loop -- is fully real; only the
 * DSP callee itself is deferred). */
extern "C" void OA_VoiceModel_PCM_Initialize(void *) {}
extern "C" void OA_VoiceModel_PCM_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_PCM_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_AnalogSync_Initialize(void *) {}
extern "C" void OA_VoiceModel_AnalogSync_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_AnalogSync_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Organ_Initialize(void *) {}
extern "C" void OA_VoiceModel_Organ_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Organ_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Plucked_Initialize(void *) {}
extern "C" void OA_VoiceModel_Plucked_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Plucked_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_MS20_Initialize(void *) {}
extern "C" void OA_VoiceModel_MS20_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_MS20_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Polysix_Initialize(void *) {}
extern "C" void OA_VoiceModel_Polysix_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Polysix_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_VPM_Initialize(void *) {}
extern "C" void OA_VoiceModel_VPM_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_VPM_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Piano_Initialize(void *) {}
extern "C" void OA_VoiceModel_Piano_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Piano_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_EP_Initialize(void *) {}
extern "C" void OA_VoiceModel_EP_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_EP_ProcessAudioRate(void *, unsigned int) {}
/*
 * CSTGPianoModel::RescanPianoTypes() (batch 60 investigation,
 * `.text+0x1f57e0`, 365 bytes) -- deliberately left stubbed, a "small
 * byte count, disproportionate new-class scope" case (same class of call
 * as batch 43's CIFXEffectSlot cluster / batch 57's UpdateGlobalTune):
 * a per-piano-type filesystem scan loop (128 iterations) that touches
 * FOUR brand-new classes this project has never declared --
 * `CSTGPianoTypes` (`ReadPianoTypeInfo`/`GetPianoTypeFileName`/
 * `AddPianoType`), `CFileStream::Exists`, `CSTGPianoModelPatch::
 * LoadPianoType`, and `CPianoOsc::CopyMultisampleInfo` -- plus a raw
 * vtable dispatch (`call *0xd8(%edx)`) on a not-yet-typed sub-object,
 * and a `CSTGHeapManager::sInstance`-relative real-filesystem-path
 * genuine SSD file I/O (`CSTGBankMemory::AllocAligned` + a second raw
 * vtable dispatch at `call *0x44(%ecx)`). Out of scope for a single
 * stub-sweep pick -- a future batch with more time budget for new-class
 * work should start here.
 */
void CSTGPianoModel::RescanPianoTypes() {}

/* ---- Engine subsystem managers (engine.cpp, sec 10.13/10.58) ---- */
/* CSTGVoiceModelManager::~CSTGVoiceModelManager() is real now, sec
 * 10.147 -- see managers.cpp. */
void CSTGEffectManager::Initialize() {}
/* CSTGEffectManager::RunEffects() is real now, batch 49 -- see
 * src/engine/effect_manager_run_effects.cpp (its own dedicated TU).
 * Its own newly-discovered confirmed-real, deliberately deferred
 * dependencies -- CSTGPerformanceVarsManager::RunEffects() (real too,
 * see global.cpp) and CSTGPerformance::RunEffects(CSTGPerformanceVars*)
 * (a genuine audio-DSP callee, stubbed below, see oa_engine_init.h for
 * the full derivation). */
/* CSTGHDRManager::Initialize() is real now, batch 22 -- see
 * hdr_manager_init.cpp. */
/* ProcessCommands()/Initialize() (CSTGMonitorMixer/CSTGHDRFileWriter/
 * CSTGSamplingDaemon/CSTGFileCloser/CSTGCDWorker) are real now, sec
 * 10.144 -- see managers.cpp. ProcessCommands() calls three siblings;
 * two (ProcessSamplerCommands batch 50, ProcessPlaybackCommands batch
 * 51 -- see hdr_sampler_commands.cpp/hdr_playback_commands.cpp) are now
 * real too -- one (ProcessHDRRecord) still deferred, stubbed below. */
/* CSTGHDRManager::ProcessRecordCommands() is real now, batch 15 -- see
 * src/engine/hdr_record_track.cpp (also introduces CSTGRecordTrack::
 * Start()/Pause()/Stop(), StandbyRec() deliberately deferred).
 * CSTGHDRManager::ProcessSamplerCommands() is real now too, batch 50 --
 * see src/engine/hdr_sampler_commands.cpp (also introduces CSTGSampler::
 * StandbyDisk()/StandbyRAM()/Start(bool)/Stop(), all four confirmed real
 * but given deliberately deferred no-op bodies there, not here).
 * CSTGHDRManager::ProcessPlaybackCommands() is real now too, batch 51 --
 * see src/engine/hdr_playback_commands.cpp. ProcessHDRRecord() itself
 * remains deferred (below) -- a genuine per-track audio-DSP peak-meter
 * loop, precisely characterized in oa_engine.h's own class comment but
 * blocked on three still-unreconstructed DSP callees. */
void CSTGHDRManager::ProcessHDRRecord() {}
void CSTGMonitorMixer::RunMonitors() {}
/* CSTGFileOpener::Initialize() is real now, batch 63 -- see
 * src/engine/file_opener_events.cpp. CSTGFileOpener::ProcessCommands()
 * is real now too, batch 64ish (2026-07-25) -- see the same file. This
 * REVISES the older "unrecovered vtable/PTM table" blocking note that
 * used to cover all five file-daemon ProcessCommands() siblings: fresh
 * disassembly found this one (and CSTGFileCloser's/CSTGHDRFileWriter's,
 * below/managers.cpp) is actually a plain fixed-slot vtable dispatch on
 * an untyped payload object, the same already-established idiom as
 * CSTGEffectRackVars::UpdateDModRoutings() (oa_global.h) -- not a real
 * PTM/function-pointer table at all. */
/* CSTGFileCloser::ProcessCommands() is real now too, same batch -- see
 * managers.cpp (right after CSTGFileCloser::Initialize()). */
/* CSTGHDRFileReader::Initialize()/CSTGStreamingFileReader::Initialize()
 * are real now too, sec 10.151 -- see managers.cpp.
 * CSTGHDRFileWriter::ProcessCommands() is real now too, same batch as
 * CSTGFileOpener/CSTGFileCloser above -- see managers.cpp.
 * CSTGHDRFileReader::ProcessCommands()/CSTGStreamingFileReader::
 * ProcessCommands() are real now too, batch 2026-07-25 -- see
 * managers.cpp. This REVISES the older "both dispatch through an
 * unrecovered `TSTGArrayManager<T>::indexArray` function-pointer table"
 * blocking note: fresh disassembly (cross-checked against Initialize()'s
 * own already-real field writes) found `indexArray` only ever holds
 * `CSTGPlaybackEvent*`/data -- the SAME usage `playback_buffer_events.cpp`
 * already established, never function pointers. The genuine per-command
 * dispatch table for both classes is a SEPARATE, self-contained set of
 * `{funcptr,adj}` pairs baked directly into each object's own instance
 * data by its ctor/Initialize() (never virtual, `adj` always 0), and
 * every one of the 9 resolved targets (6 for CSTGHDRFileReader, 3 for
 * CSTGStreamingFileReader) is now a real, reconstructed sibling method --
 * see oa_engine.h's own class comments for the full per-tag mapping. */
/* USTGHDRUtils::ConvertWaveToSTGSamples() is real now, batch 26 -- see
 * src/engine/wave_sample_convert.cpp. Convert44100WaveToSTGSamples()
 * (its own 44100Hz-source-only sibling, see oa_engine.h's USTGHDRUtils
 * class comment for the full deferral reasoning -- a genuine
 * fractional-phase resampler, x87-heavy across real branches) stays a
 * deliberately deferred stub; ConvertWaveToSTGSamples() itself never
 * calls it except in the untested-so-far 44100Hz path. */
unsigned long USTGHDRUtils::Convert44100WaveToSTGSamples(float *, bool, bool, char *, bool, bool,
                                                          unsigned long, CSTGPlaybackEvent *,
                                                          unsigned long) { return 0; }
/* CSTGCDWorker_InitializeBuffer is real now, sec 10.148 -- see
 * managers.cpp (right after CSTGCDWorker::Initialize(), sec 10.144). */
/* CSTGCDWorker::ProcessCommands() is real now, sec 10.158 -- see
 * managers.cpp (also its own newly-discovered dependency,
 * CSTGHDRCircularBuffer, a brand-new fully-reconstructed class -- see
 * oa_engine.h). */
/* CSTGSamplingDaemon::ProcessCommands() is real now, sec 10.160 -- see
 * managers.cpp (right after CSTGCDWorker::ProcessCommands()). All FIVE of
 * its former deferred file-daemon siblings (CSTGFileOpener/CSTGFileCloser/
 * CSTGHDRFileWriter/CSTGHDRFileReader/CSTGStreamingFileReader::
 * ProcessCommands()) are real now (batch 64ish + batch 2026-07-25 -- see
 * file_opener_events.cpp/managers.cpp) -- the whole cluster's old
 * "unrecovered PTM table" blocking note is fully retired; every dispatch
 * turned out to be either a fixed vtable slot on an untyped payload, or a
 * per-instance `{funcptr,adj}` table baked into the object's own data by
 * its ctor/Initialize(), never a genuinely unrecovered lookup table. */
/* CSTGMidiPortManager::Initialize() is real now, sec 10.230/
 * MASTER_REFERENCE -- see src/engine/midi_port_manager.cpp (the root fix
 * for the CSTGMidiQueueWriter::Write() ringCtl-NULL crash).
 * CSTGMidiPortManager::~CSTGMidiPortManager() is real now too, batch 57 --
 * see the same file (also names CSTGMidiOutPort's own +0x5 `flags` byte
 * for the first time, oa_engine_init.h). */
/* CSTGMidiInPort::ReceiveSysEx(const unsigned char*, unsigned int) --
 * confirmed real (.text+0xf63f0, 291 bytes), declared in oa_engine.h so
 * callers link against its real mangled name, but deliberately not
 * implemented there -- needed as a link target now that
 * CSTGMidiInPortGeneric::Receive (src/engine/midi_in_port.cpp, 2026-07-24)
 * calls it for real on incoming 0xF0/continuation SysEx bytes. Stubbed
 * here as a no-op: with no physical MIDI hardware generating real SysEx
 * traffic in a VM, this path is never exercised by kronos_vm boot-testing
 * anyway. */
void CSTGMidiInPort::ReceiveSysEx(const unsigned char *, unsigned int) {}
/* CSTGMidiPortManager::WriteSTGMidiOutQueue()/NotifyNKS4TestMode() are
 * real now, batch 12 -- see src/engine/midi_port_manager.cpp (its own
 * dedicated TU, not linked by test_global.cpp/test_engine.cpp/
 * test_global_ctor.cpp, each of which keeps its own pre-existing local
 * mock for both symbols untouched -- same "give it its own TU"
 * technique already used for CSTGMidiQueueWriter::Write, sec 10.83).
 * NotifyNKS4TestMode()'s own newly-discovered dependency,
 * CSTGMidiQueue::Reset(), is real now too -- see midi_queue.cpp. */
/* CSTGVoiceAllocator::Initialize() is real now, sec 10.157 -- see
 * managers.cpp (also CSTGVoice::CSTGVoice(unsigned short), a brand-new
 * class this same pass gives its own full definition in oa_engine.h --
 * see there for the confirmed field list and _ZTV9CSTGVoice's own
 * zero-filled placeholder vtable, defined in managers.cpp). */
/* CSTGVoiceAllocator::~CSTGVoiceAllocator()/CSTGMessageProcessor::
 * ~CSTGMessageProcessor() are real now, sec 10.147 -- see managers.cpp. */
void CSTGAudioBusManager::MixPerformanceOutputs() {}
/* CSTGAudioBusManager::LRBusIndivMirror() is real now, sec 10.153 -- see
 * src/engine/audio_bus_manager.cpp. */
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
/* CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors() is
 * real now, batch 53 -- see src/engine/tone_adjust_descriptors.cpp
 * (also homes the three newly-discovered STGProgramParams/
 * STGCommonStepSeqParams/CSTGParamDescriptor::sTypical99ToFloatParamDesc
 * external data-table stand-ins and the new STGToneAdjustCommonParams
 * 37-entry table storage). */
/* CSTGMultisampleBankManager::Initialize()/CSTGPCMPrecacheManager::
 * Initialize() are both real now, sec 10.149/10.144 -- see
 * setup_global_resources.cpp. */
void CSTGPCMPrecacheManager::AfterProcess() {}
/* CSTGPCMPrecacheManager::Reset(bool, bool, unsigned long) is real now,
 * sec 10.154 -- see src/init/setup_global_resources.cpp. */

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
 * real, sec 10.80 -- see src/engine/global.cpp. CSTGAudioInputMixerBase's
 * own four setters (SetHDRBus/SetFXCtrlBus/SetOutputBus/SetPan) are now
 * real too, sec 10.150 -- see src/engine/audio_input_mixer.cpp (its own
 * dedicated translation unit; test_engine.cpp/test_global.cpp/
 * test_global_ctor.cpp all keep their own pre-existing mocks for these,
 * untouched, matching the CSTGMidiQueueWriter::Write precedent). Their
 * own three newly-discovered confirmed-real, deliberately deferred
 * dependencies (CSTGPan::CalculateMonoPanCoeffs, CBusChangeStateMachine::
 * StartBusChange, CSTGBusInfo::GetSignalSelectionForBusType) are all
 * real now, sec 10.151 -- see src/engine/audio_input_mixer.cpp. */
/* CSTGAudioInput::UseSettings() is real now, batch 18 -- see
 * src/engine/audio_input_use_settings.cpp (its own dedicated translation
 * unit; test_engine.cpp/test_global.cpp/test_global_ctor.cpp all keep
 * their own pre-existing mocks for this symbol untouched). */
/* Sec 10.97's own confirmed-real, deliberately deferred externs.
 * CSetListSlot::Activate is now real (sec 10.141). */
/* SendPerfChangeToMidiOut is now real, sec 10.98 -- see
 * src/engine/global.cpp. Its own confirmed-real, deliberately deferred
 * dependencies still need link-satisfying mocks here. */
/* USTGAliasBankTypes::ConvertAliasPgmBankToMidiBank/ConvertCombiBankToMidiBank/
 * ConvertMidiBankToCombiBank/ConvertMidiBankToAliasProgramBank are all real
 * now, sec 10.152 -- see src/engine/alias_bank_convert.cpp. */
/* SKSTGGate_ShouldSyncExternalClock() is real now, sec 10.148 -- see
 * src/engine/sk_stg_gate.cpp (a genuinely new class, CTimerManager,
 * declared minimally/opaquely there -- most of its own dozen-plus
 * methods are NOT reconstructed in this pass). CTimerManager::
 * ShouldSyncExternalClock() itself is real now too, sec 10.151 -- see
 * src/engine/sk_stg_gate.cpp (no stub body here any more, multiple
 * definition otherwise). */
/* CSTGParamsOwner::ValidateParamChange (sec 10.92) -- confirmed real,
 * deliberately deferred; see src/engine/global.cpp for the real
 * callers now reconstructed. */
void CSTGParamsOwner::ValidateParamChange(CSTGMessageContext &, unsigned long, const CValue &) {}
/* CSTGParamsOwner::UseDefaults() (sec 10.228) -- confirmed real,
 * deliberately deferred (needs the not-yet-recovered CSTGParamDescriptor
 * table); see oa_global.h's own comment on this method and
 * src/engine/global.cpp's CSTGGlobal::Initialize() for the real caller,
 * now reconstructed as a direct (non-virtual) call to this. */
void CSTGParamsOwner::UseDefaults() {}
/* CSTGControllerRTData::SetAudioInSolo(unsigned int, bool) is real now,
 * batch 57 -- see src/engine/controller_rt_data_set_audio_in_solo.cpp
 * (its own dedicated TU; test_engine.cpp/test_global.cpp/
 * test_global_ctor.cpp all keep their own pre-existing mocks for this
 * symbol untouched, matching the WriteSTGMidiOutQueue precedent). Also
 * gives global.cpp's own ResolveCurrentPerformance() external linkage so
 * this file could reuse it instead of duplicating the 3-way mode-dispatch
 * formula.
 * CSTGControllerRTData::ResetSendKnobsJumpCatch() is real now too, batch
 * 57 -- see src/engine/controller_rt_data_reset_send_knobs_jump_catch.cpp
 * (its own dedicated TU, same reason). Five newly-discovered confirmed-
 * real, deliberately deferred sibling callees (own bodies not
 * reconstructed) defined in that same new file. */
/* CSTGComPort::RTAIInterruptHandler is real now, batch 48 -- see
 * src/init/comport.cpp (a thin forwarder to the already-real
 * HandleInterrupt()/ComPortServiceLoop -- see oa_comport.h's own
 * updated comment for the full derivation). */
/* CSTGCombi::CSTGCombi() is real now, batch 45 -- see
 * src/engine/combi_ctor.cpp (resolves the sibling this file's own
 * GetPatchStaticCosts-area comment, and program_ctor.cpp's own header
 * comment, both explicitly deferred for a future batch). */
/* CSTGControllerRTData::CSTGControllerRTData() is real now, sec 10.155 --
 * see src/engine/controller_rt_data_ctor.cpp. CSTGControllerRTData::
 * Initialize()/RequestAnalogInputPositions() reconstructed for real, sec
 * 10.88 -- see src/engine/controller_rt_data_init.cpp.
 * OnExtModeKnobAssignChange()/OnExtModeSliderAssignChange() are real now
 * too, sec 10.161 -- see src/engine/global.cpp + the new
 * src/engine/cc_info_table.cpp (CSTGCCInfo::sCCInfoTable). */
void CSTGControllerRTData::OnExtModeSetChange() {}
/* CSTGControllerInfo::SendUnsolicitedUIParam(unsigned int, unsigned int,
 * long, int) is real now, batch 60 -- see
 * src/engine/controller_info_send_unsolicited_ui_param.cpp. */
void CSTGControllerRTData::OnPerformanceActivate(CSTGPerformance &) {}
/* CSTGControllerRTData::SendKarmaCCToKG(int, unsigned char) is real now,
 * round 44 (2026-07-29) -- see
 * src/engine/controller_rt_data_send_karma_cc.cpp. */
/* CSTGControllerInfo::AnalogControllerHandler -- REAL body now, batch 65,
 * see src/engine/controller_info_analog_handler.cpp (own header comment
 * has the full confirmed device-code dispatch). Superseded the deliberate
 * no-op stub that lived here.
 *
 * ButtonPressHandler(unsigned int, bool) -- REAL body now, batch 66, see
 * src/engine/controller_info_button_handler.cpp (own header comment has
 * the full confirmed three-`.rodata`-table dispatch). Superseded the
 * deliberate no-op stub that lived here. Its own deferred callees
 * (`HandleEditInContextButton`, `ProcessMixerSwitchPress`,
 * `SetMixerKnobMode`, `SetSoloSelected`, `ResetAllKnobCCs`,
 * `ResetAllExtModeControllers`, the weak `NotifyParam(unsigned int,
 * long)`) are deliberately NOT stubbed here -- same convention as the 22
 * `AnalogXxxHandler` methods `AnalogControllerHandler` calls: they remain
 * genuinely unresolved OA.ko-internal symbols (see `nm -u`), matching
 * the real binary's own "not yet load-bearing" state for this front-
 * panel-button subsystem. */
/* CSTGKeybedInterface::SetLED -- REAL body now, batch 64, see
 * src/init/keybed_interface.cpp. Superseded the deliberate no-op stub
 * this file carried since batch 63 -- the class's ~20-method
 * wire-protocol driver is now mostly reconstructed (oa_keybed_init.h). */
/* CLoadBalancer::BalanceStaticLoad()/BalanceStaticLoadHelper(...) and
 * CSTGSlotVoiceData::EnableSlot() are real now, batch 18 -- see
 * src/engine/load_balancer_static.cpp. Their own fourth cluster sibling,
 * CSTGSlotVoiceData::GetPatchStaticCosts(unsigned int, unsigned long*,
 * unsigned long*) const, is STILL confirmed real, deliberately deferred
 * (real vtable DISPATCH through the not-yet-reconstructed
 * CIFXEffectSlot/CMFXEffectSlot cluster, sec 10.157) -- calling into it
 * from BalanceStaticLoadHelper is safe.
 *
 * Batch 43 investigated this whole cluster in depth (per the task
 * briefing's explicit ask, following up on batch 42's
 * RunVoiceModelStaticFront/StaticBack/RunVoiceModelFeedback finding) and
 * confirmed it is NOT yet tractable via the sec 10.185/10.193
 * hand-crafted-vtable technique, for reasons materially different from
 * (and larger than) the ten Model ctors this same technique already
 * closed out: `readelf -SW` confirms a much bigger real hierarchy than
 * previously assumed -- `CEffectSlotBase` (0x84/33 slots),
 * `CEffectSlot` (0x88/34 slots), `CIFXEffectSlot` (0x88/34, same count
 * as CEffectSlot -- no new virtuals of its own), `CMFXEffectSlot`
 * (0x88/34), and a FOURTH, previously unflagged sibling,
 * `CTFXEffectSlot` (0x88/34) -- plus a small 6-slot mixin,
 * `CSTGEffectSlotMsgHandler`. `CIFXEffectSlot::CIFXEffectSlot()` itself
 * (.text+0x8d0e0, 77 bytes) is small and mechanical (vtable-install +
 * 9 field writes, no dispatch) -- confirmed tractable in isolation.
 * `CMFXEffectSlot` has NO out-of-line ctor at all (fully inlined into
 * `CSTGProgram::CSTGProgram()`'s own body -- 5 field writes per
 * instance, 3 instances). The REAL blocker is `CSTGProgram::
 * CSTGProgram()` (.text+0xa4c00, 328 bytes) itself: unlike the Model
 * cluster's single base class, this ctor installs TWO separate vtable
 * pointers at fixed offsets (`+0x0` = vtable-for-`CSTGPerformance`+8,
 * `+0x4` = vtable-for-`CSTGEffectRack`+8) -- genuine C++ MULTIPLE
 * inheritance, meaning `CSTGPerformance`/`CSTGEffectRack` (each their
 * own 0x98/38-slot real vtable, `readelf` confirmed) would ALSO need
 * correctly-shaped vtables, not just `CSTGProgram`'s own, before this
 * ctor is safe -- a materially bigger structural lift than any single
 * base class the hand-crafted-vtable technique has closed so far, and
 * multiple-inheritance vtable layout (secondary-base thunks/`this`
 * adjustment) has its own real correctness risk this project hasn't
 * exercised yet. `ChangeProgram(CSTGProgram*)` (a separate stub, see
 * below) was ALSO re-investigated as a possible smaller entry point:
 * its own one vtable dispatch (`call *0xe0(%ecx)` on `this`'s OWN
 * vtable, `_ZTV15CSTGProgramSlot`/its two derived siblings) resolves via
 * `readelf -rW` to `CSTGProgramModeProgramSlot::GetChordSource() const`
 * -- a real, tiny (27-byte base impl at .text+0xa95c0, 11-byte weak
 * per-derived-class thunks), non-DSP getter, genuinely tractable BUT
 * would require growing `g_programSlotVtable` (global.cpp, currently 10
 * native-pointer slots, only slot 7 populated) out to at least 57
 * slots to safely reach slot ~54 -- doable, just not attempted this
 * batch given time already spent on the DEAX cipher work below. Full
 * derivation (readelf output, exact slot offsets, symbol names) is
 * recorded in the agent-memory workflow doc rather than repeated here;
 * a future batch should start from THAT, not re-run this investigation
 * from scratch.
 *
 * UPDATE (batch 44, sec 10.195): `CSTGProgram::CSTGProgram()` itself IS
 * now real (see src/engine/program_ctor.cpp) -- batch 43's own
 * `readelf -SW` vtable-size estimate above turned out to be wrong for
 * `CSTGEffectRack` (a direct `nm -CS` re-check this batch found it is
 * 0x60/24 slots, not 0x98/38 -- that larger number belonged to
 * `CSTGPerformance`, a different class also needed here), and a fresh,
 * exhaustive project-wide grep for every already-real caller of
 * `CSTGProgram`/`CSTGCombi`/`CSTGPerformance`/`CSTGEffectRack`/
 * `CIFXEffectSlot`/`CMFXEffectSlot`/`CTFXEffectSlot` confirmed NONE of
 * them genuinely dispatch through any of these classes' vtables today
 * (every path that eventually would is itself still a bare-`{}` stub:
 * `EnterActivatingState`, `GetPatchStaticCosts` right below,
 * `RunVoiceModelStaticFront`/`StaticBack`/`RunVoiceModelFeedback`,
 * `CSetListEQ::Initialize`, `CSTGEffectManager::RunEffects`) -- making
 * the sec 10.185/10.193 hand-crafted-vtable technique safe to apply
 * here too, just with TWO base vtables instead of one, exactly as this
 * comment's own prior batch speculated might be possible.
 *
 * UPDATE (batch 45): `CSTGCombi::CSTGCombi()` is now real too -- see
 * src/engine/combi_ctor.cpp. Same shape as CSTGProgram (shares its
 * entire sub-object list through CSTGAudioInput@+0xae7 byte-for-byte),
 * but a fresh disassembly (not the guess this comment previously carried)
 * found SIXTEEN embedded `CSTGProgramSlot`s, not fifteen, at a confirmed
 * 0xe8-byte stride, in place of CSTGProgram's own `CSTGCommonLFO`/
 * `CSTGToneAdjust` tail. `GetPatchStaticCosts`/`RunVoiceModelStaticFront`/
 * `StaticBack`/`RunVoiceModelFeedback`/`GetTotalStaticCosts` immediately
 * below remain correctly deferred -- THEIR OWN bodies are what would
 * genuinely dispatch through these now-real-but-still-zero-filled
 * vtables, a real crash risk until reconstructed for real.
 *
 * UPDATE (batch 47): `ChangeProgram(CSTGProgram*)` itself is now real too
 * -- see global.cpp. CORRECTION of this comment's own batch-43 claim
 * above: a fresh, independent `readelf -r` re-derivation this batch found
 * `call *0xe0(%ecx)` does NOT resolve to `GetChordSource() const` as
 * speculated -- it resolves to `ProcessPreviousSVDOnProgramChange
 * (CSTGSlotVoiceData*)`, a DIFFERENT real virtual (confirmed via each
 * class's own `_ZTV*` relocation table read directly, not inferred).
 * That function turned out to call only ALREADY-REAL siblings
 * (`CSTGSlotVoiceData::SetIsDying()`/`FreeSlotVoiceData(bool)`, both real
 * since sec 10.140/batch 17) -- no DSP callee at all, unlike this
 * cluster's other members. The two per-class vtable slots (previously a
 * single SHARED `g_programSlotVtable`, safe only because nothing past
 * slot 7 was ever read) were split into `g_programModeProgramSlotVtable`/
 * `g_programModeDrumTrackSlotVtable` so slot 56 can hold each class's own
 * real target (an override for the program-mode slot, the base impl for
 * the drum-track slot, confirmed different via `readelf -r`). The two
 * callees `ChangeProgram()` DOES still call for real
 * (`CSTGSlotVoiceData::Setup()`/`CSTGProgramSlot::CompleteLoadProgram()`,
 * 3652/859 bytes) are genuine "load a program into a voice" DSP/setup
 * routines, confirmed via their own disassembly -- both stubbed
 * immediately below, matching the reconstruct-caller-DSP-stub-callee
 * pattern (`ChangeProgram()` itself is the caller, now fully real). */
void CSTGSlotVoiceData::GetPatchStaticCosts(unsigned int, unsigned long *, unsigned long *) const {}
/* CSTGPerformance::RunEffects(CSTGPerformanceVars*) (batch 49) -- confirmed
 * real, deliberately deferred: genuine audio-DSP effect processing (SSE
 * stereo-pan smoothing, CSTGEffectRack::RunEffects, CSetListEQ::Run,
 * CSTGEffectRackVars::ApplyDModTickDelay), out of scope per the sec 10.185
 * policy. Its own real caller, CSTGPerformanceVarsManager::RunEffects(),
 * IS reconstructed for real -- see oa_global.h/global.cpp. */
void CSTGPerformance::RunEffects(CSTGPerformanceVars *) {}
/* CSTGSmoother::FinalizeSmoother(void*, bool) is real now, batch 57 --
 * see src/engine/smoother_finalize.cpp (its own dedicated TU; four
 * sibling verify/ files keep their own pre-existing mocks for this
 * symbol untouched). Its own newly-discovered confirmed-real,
 * deliberately deferred callee (own body not reconstructed, genuine
 * audio-DSP dispatch, confirmed unreachable from any currently-real
 * caller): CSTGSmootherMapping_DispatchSmoothedValue, defined in that
 * same new file. */
/* CSTGChannelValues::Reset() is real now, batch 18 -- see
 * src/engine/channel_values_reset.cpp (its own dedicated translation
 * unit; test_engine.cpp/test_global.cpp/test_global_ctor.cpp all keep
 * their own pre-existing mocks for this symbol untouched -- test_global.cpp's
 * own mock is load-bearing call-tracking). */
/* CSTGChannelValues::SetControllerValue() is real now, batch 15 -- see
 * src/engine/channel_values_set_controller_value.cpp. */
/* Sec 10.101's own confirmed-real, deliberately deferred externs. */
void CSTGControllerRTData::HandleControllerChange(int, unsigned char, bool, bool) {}
void CSTGControllerInfo::SetPerfSwitch(int, bool) {}
/* CSetListEQ::SetBand(unsigned int, float) (batch 41, ground truth
 * .text+0x2025b0) confirmed real: genuine SSE/x87 EQ-coefficient DSP
 * (SSE broadcast + CSTGEQ::CalculatePeakingBeta + peaking-coefficient
 * math) -- out of scope per the sec 10.185 audio-DSP policy. Its own
 * caller, CSetList::Activate(), IS reconstructed for real (see
 * src/engine/global.cpp) -- this no-op is exactly the safe stand-in for
 * a confirmed-real-but-deferred callee, matching the SetPerfSwitch
 * precedent right above. */
void CSetListEQ::SetBand(unsigned int, float) {}
void CSTGControllerRTData::ResetKnobsJumpCatch() {}
void CSTGControllerRTData::ResetSlidersJumpCatch() {}
void CSTGControllerRTData::ResetRTKKnobSmoothers() {}
/* CSTGControllerRTData::SetControllerAssignment() is real now, batch 16
 * -- see src/engine/controller_rt_data_set_assignment.cpp.
 * CSTGSlotVoiceData::UpdateAllActiveMIDIFilters() is real now too, same
 * batch -- see src/engine/slot_voice_data_midi_filters.cpp
 * (UpdateMIDIFilterAndResendAllCCs() deliberately deferred there). */
/* CSTGControllerRTData::sInstance storage now lives in
 * src/engine/controller_rt_data_ctor.cpp, sec 10.155 (moved there
 * alongside the real ctor, matching the CSTGFrontPanelSmoothers/
 * CSTGHDRMiniModel precedent of homing sInstance storage in the same
 * TU as the real ctor rather than here). */
/* CSTGDrumKitData::CSTGDrumKitData() is real now, batch 23 -- see
 * src/engine/drum_kit_data.cpp. */
/* CSTGFrontPanelSmoothers::CSTGFrontPanelSmoothers() is real now, sec
 * 10.153 -- see src/engine/front_panel_smoothers.cpp. */
/* CSTGGlobal::InitializePerformances() is real now, batch 54 -- see
 * src/engine/init_performances.cpp. Its own genuinely-deferred filesystem-
 * I/O/DSP-stub-callee sub-parts follow, all newly declared this batch
 * (oa_global.h): CKorgPreloadFile::Load() (genuine SSD file I/O), and
 * PopulateDefaultProgramSlotTemplates() (the still-untraced two-nested-loop
 * default-performance-data block -- see InitializePerformances()'s own
 * class comment for the full derivation of what this represents).
 * CSTGProgramBank::Initialize()/GetPatchSize() are real now too, batch 61
 * -- see src/engine/program_bank_init.cpp (that file's own header
 * comment corrects a stale "DSP/filesystem, out of scope" claim this
 * comment previously made about them -- see oa_global.h's
 * CSTGProgramBank class comment). CSTGProgram::Initialize() (415B) and
 * CSTGProgram::Copy() (8620B, newly discovered this batch as
 * CSTGProgramBank::Initialize()'s own 127-iteration loop callee) remain
 * deliberately deferred -- their own no-op bodies are DELIBERATELY kept
 * here rather than moved into program_bank_init.cpp, so
 * test_program_bank_init.cpp can supply its own local call-tracking
 * mocks for both without a multiple-definition conflict. */
int CKorgPreloadFile::Load() { return 1; }
void CSTGProgram::Initialize(unsigned int, unsigned int, unsigned int) {}
void CSTGProgram::Copy(CSTGProgram *, unsigned int, unsigned int, unsigned int) {}
void CSTGGlobal::PopulateDefaultProgramSlotTemplates() {}
/* CSTGProgramBank::ChangeBankType() -- confirmed real caller found
 * 2026-07-25 (CSTGControlMsgHandler::SetProgramBankTypeHandler, see
 * oa_control_msg_handler.h), own body still deliberately deferred (same
 * reasoning as CSTGProgram::Initialize/Copy just above). */
void CSTGProgramBank::ChangeBankType(unsigned int) {}
/* CSTGMessageProcessor::StartDownload()/EndDownload() -- confirmed real
 * callers found 2026-07-25 (CSTGControlMsgHandler::StartDownloadHandler/
 * EndDownloadHandler, see oa_control_msg_handler.h), own bodies still
 * deliberately deferred (same reasoning as ChangeBankType above). */
void CSTGMessageProcessor::StartDownload() {}
void CSTGMessageProcessor::EndDownload() {}
/* CSTGHDRMiniModel::CSTGHDRMiniModel() is real now, sec 10.155 -- see
 * src/engine/engine_init.cpp. */
void CSTGHDRMiniModel::Initialize() {}
/* CSTGHeapManager::Alloc(unsigned int) -- oa_setup_global_resources.h's
 * own local "static" ecosystem stand-in for the real instance method
 * CSTGHeapManager::Alloc(unsigned long) -- is real now, batch 17, see
 * src/mem/heap_manager_alloc_static.cpp (its own dedicated TU:
 * test_setup_global_resources.cpp carries its own load-bearing
 * call-counting mock of this exact symbol). */
/* CSTGHeldKeyList::Reset() reconstructed for real, sec 10.82 -- see
 * src/engine/global.cpp. CSTGHeldKeyList::CSTGHeldKeyList() is real now
 * too, sec 10.155 -- see src/engine/slot_voice_data_ctor.cpp. */
/* CSTGLFOTables::CSTGLFOTables() is real now, batch 28 -- see
 * src/engine/lfo_tables.cpp. */
/* CSTGMIDIClockSync::CSTGMIDIClockSync() is real now, batch 21 -- see
 * src/engine/midi_clock_sync.cpp (also its own newly-discovered
 * dependencies, CSTGMIDIClockSyncBase::Initialize() and the complete
 * CSTGIntMIDIClockSync class, same file). */
void CSTGMidiDispatcher::HandleController(unsigned char, unsigned char, unsigned char, int, int) {}
void CSTGMidiDispatcher::ResetAllControllers(unsigned char, bool) {}
/* CSTGMidiQueue::AllocReader() reconstructed for real, sec 10.82 -- see
 * src/engine/global.cpp. CSTGMidiQueueWriter::Write() reconstructed
 * for real, sec 10.83 -- see src/engine/midi_queue_writer.cpp (its own
 * separate translation unit, not global.cpp -- test_global.cpp's own
 * mock for this symbol is load-bearing for ~10 other UpdateXXX
 * assertions there, so the real body deliberately lives somewhere
 * those tests don't link, matching this project's per-unit file
 * convention). CSTGMidiQueue::GetNumWritableBytes() is now real too,
 * sec 10.150 -- see src/engine/midi_queue.cpp, a THIRD separate
 * translation unit (shares the same ringCtl memory Write() uses, but
 * kept apart from midi_queue_writer.cpp so it alone, not Write(), can
 * be linked into test_global.cpp -- its own mock footprint there was
 * tiny, never varied away from its 0 default anywhere in that file). */
/* placeholder-removed-below -- see
 * src/engine/global.cpp. CSTGPerformance::IsCurrentlyActive() is real
 * now too, sec 10.144 -- see managers.cpp. */
/* CSTGPerformanceVarsManager::Initialize() is real now, batch 53 -- see
 * src/engine/performance_vars_manager_init.cpp (sInstance storage moved
 * there too, alongside it, matching the CSTGFrontPanelSmoothers/
 * CSTGHDRMiniModel "home sInstance with the real ctor" precedent). Its
 * own four newly-discovered confirmed-real, deliberately deferred
 * sub-object Initialize()/ctor dependencies. CSTGAudioInputMixerBase's
 * own ctor, CSTGAudioInputMixer::Initialize(), CSTGMasterLRMixer::
 * Initialize(), and CSTGAudioInputMixerBase::SetSendBuses() are all real
 * now, batch 58 -- see src/engine/audio_input_mixer.cpp.
 * CSetListEQ::Initialize() is real now, batch 59 -- see
 * src/engine/set_list_eq_init.cpp.
 *
 * CSTGEffectRackVars::Initialize(CSTGPerformanceVars*) (batch 60
 * investigation, `.text+0xd0aa0`, 424 bytes) -- deliberately left
 * stubbed, a "small byte count, disproportionate new-class scope" case:
 * fourteen calls into THREE brand-new classes this project has never
 * declared -- `STGIFXSlotParams::Initialize(CSTGPerformanceVars*,
 * eSTGBusID, eSTGBusID)` (12x, at confirmed real per-slot offsets
 * `+0x0/+0x210/+0x420/.../+0x1cf0`, stride 0x210), `STGMFXSlotParams::
 * Initialize(...)` (2x, `+0x1a30`/`+0x1a30`... actually `+0x18c0`/
 * `+0x1a30`), and `STGEffectSlotVars::Initialize(...)` (2x, `+0x1ba0`/
 * `+0x1cf0`). All fourteen calls are regparm(3) `this`/`owner`/`busId1`
 * plus a fourth stack arg `busId2` -- confirmed real per-call constant
 * bus-ID pairs, not derived. Out of scope for a single stub-sweep pick
 * (three new classes' own `Initialize()` bodies would each need their
 * own investigation) -- a future batch with more time budget for
 * new-class work should start here; the call-site offsets/bus-ID pairs
 * above are already fully extracted, no need to re-disassemble this
 * caller from scratch.
 */
void CSTGEffectRackVars::Initialize(CSTGPerformanceVars *) {}
/* CSTGPlaybackEvent::CSTGPlaybackEvent() is real now, sec 10.150 -- see
 * src/engine/engine_init.cpp. Needs its own confirmed 40-byte vtable
 * placeholder, _ZTV17CSTGPlaybackEvent, declared below alongside its
 * siblings. */
/* CSTGProgram::CSTGProgram() is real now, batch 44 (sec 10.195) -- see
 * src/engine/program_ctor.cpp. CSTGCombi::CSTGCombi() is real now too,
 * batch 45 -- see src/engine/combi_ctor.cpp. Resolves the multiple-
 * inheritance cluster this file's own comment near GetPatchStaticCosts
 * documents batch 43 investigating and batch 44/45 closing out -- see
 * that comment (just below) for what's STILL deferred (everything that
 * actually DISPATCHES through the now-real-but-still-zero-filled
 * CSTGPerformance/CSTGEffectRack/CIFXEffectSlot/CMFXEffectSlot/
 * CTFXEffectSlot vtables). */
/* CSTGProgramModeProgramSlot/CSTGProgramModeDrumTrackSlot's own ctors +
 * Initialize()/OnUpdateGlobalMidiChannel/OnUpdateProgramDrumTrackMidiChannel
 * all reconstructed for real, sec 10.81/10.125/10.133 -- see
 * src/engine/global.cpp. CORRECTION (sec 10.153): this comment previously
 * (mis)claimed CSTGProgramSlot's OWN base ctor was included in that same
 * "all reconstructed" set -- it was NOT; only the two DERIVED ctors were.
 * CSTGProgramSlot::CSTGProgramSlot() itself is real now too, sec 10.153 --
 * see src/engine/program_slot_ctor.cpp. */
/* IsActive()/AccessActiveSlotVoiceData()/HasActiveSlotVoiceData()/
 * HasActiveVoices() reconstructed for real, sec 10.142 -- see
 * src/engine/global.cpp. CSTGProgramSlot::ChangeProgram(CSTGProgram*) is
 * real now too, batch 47 -- see global.cpp (also the two new real vtable
 * slot-56 implementations, ProgramSlot_/ProgramModeProgramSlot_
 * ProcessPreviousSVDOnProgramChange, and the g_programModeProgramSlotVtable/
 * g_programModeDrumTrackSlotVtable split, same file). Its own two
 * confirmed-real, deliberately deferred DSP/setup callees (own bodies not
 * reconstructed in this pass): */
void CSTGSlotVoiceData::Setup(CSTGProgramSlot *, CSTGProgram *, const CSTGChannelValues *) {}
void CSTGProgramSlot::CompleteLoadProgram(CSTGSlotVoiceData *) {}
/* CSTGRecordBuffer::CSTGRecordBuffer() is real now, sec 10.148 -- see
 * src/engine/engine_init.cpp (also corrects a real bug that promotion
 * uncovered: this class's true size is 0x301c bytes, not the 0x38 this
 * project had assumed before its ctor was ever disassembled). */
/* CSTGSamplingInterface::CSTGSamplingInterface() is real now, sec
 * 10.160 -- see src/engine/sampling_interface_ctor.cpp (also its own
 * confirmed real vtable, _ZTV21CSTGSamplingInterface, defined there). */
/* CSTGSequence::CSTGSequence() is real now, sec 10.153 -- see
 * src/engine/sequence_ctor.cpp. */
/* CSTGSlotVoiceData::CSTGSlotVoiceData() is real now, sec 10.155 -- see
 * src/engine/slot_voice_data_ctor.cpp (also its own newly-discovered
 * embedded-sub-object dependencies, CSTGMidiCCFilter::Initialize() and
 * CSTGHeldKeyList::CSTGHeldKeyList(), in the same file).
 * CSTGSlotVoiceData::Initialize(unsigned short) is real now, sec
 * 10.150 -- see src/engine/global.cpp. Its own dependency,
 * CSTGChannelValues::Initialize(), is real now too, sec 10.151 -- see
 * src/engine/global.cpp. Its own storage and newly-discovered
 * confirmed-real, deliberately deferred dependency (InitializeLongHand(),
 * substantially larger, own body not reconstructed this pass): */
unsigned char CSTGChannelValues::sTemplateReady;
unsigned char CSTGChannelValues::sTemplate[0x92c];
void CSTGChannelValues::InitializeLongHand() {}

/*
 * FIX (2026-07-27 systemic `.ctors` sweep, see oa_init.h's own comment
 * on this function's declaration): reproduces ground truth's real
 * static ctor for `CSTGChannelValues::sTemplate` (the compiler grouped
 * it under `_GLOBAL__I__ZN17CSTGChannelValues14sTemplateReadyE`'s own
 * symbol name, but its relocations target `sTemplate` itself) -- 121
 * 12-byte sub-records (120 in a loop over `sTemplate[0..0x59f]`, plus
 * one more "tail" record at `sTemplate+0x5a0`) each get `+0xa`/`+0xb`
 * set to 1; every other field these records touch is a redundant
 * zero-write already covered by plain BSS zero-init, not reproduced
 * here. Does NOT attempt to reproduce `InitializeLongHand()` itself
 * (still a confirmed-real, deliberately deferred stub, above) -- only
 * this independently real, independently ctors-confirmed seed pattern,
 * which ground truth's do_mod_ctors() applies at module load, strictly
 * before InitializeLongHand() could ever lazily run.
 */
extern "C" void ConstructChannelValuesTemplate(void)
{
	unsigned char *tmpl = CSTGChannelValues::sTemplate;
	for (unsigned int i = 0; i < 120; i++) {
		tmpl[i * 12 + 0xa] = 1;
		tmpl[i * 12 + 0xb] = 1;
	}
	tmpl[0x5a0 + 0xa] = 1;
	tmpl[0x5a0 + 0xb] = 1;
}
void CSTGSlotVoiceData::RunVoiceModelFeedback() {}
/*
 * UpdateGlobalTune(float) (batch 57 investigation, .text+0xb4860, 335
 * bytes) -- deliberately left stubbed, NOT a "too hard to determine"
 * case (behavior IS determined) but a "disproportionate new
 * infrastructure for one function" case, same class of call as batch
 * 43's own CIFXEffectSlot cluster deferral:
 *   - calls CSTGProgramSlot::UsesPatch(unsigned int, CSTGProgram*) const
 *     (real, not yet reconstructed) up to twice.
 *   - on a true result, constructs a REAL, fully-vtabled
 *     `CSTGPatchMessageContext` object on the stack (installs
 *     `vtable-for-CSTGPatchMessageContext + 8` via a genuine `R_386_32`
 *     relocation, confirmed via `objdump -dr` -- NOT a placeholder/
 *     zero-filled class; this project has no `CSTGPatchMessageContext`
 *     class at all yet, own fields beyond the vtable ptr not derived),
 *     then dispatches through vtable slot 53 (raw offset 0xd4) on `this`
 *     own `CSTGProgramSlot` object (`this->fieldAt(0x1488)` or
 *     `this->fieldAt(0x68)`/`this->fieldAt(0xa68)` depending on branch --
 *     TWO different embedded `CSTGProgramSlot` sub-objects, presumably
 *     per-layer/per-timbre).
 *   - slot 53 is past the current `g_programModeProgramSlotVtable`/
 *     `g_programModeDrumTrackSlotVtable` split's own populated range
 *     (currently only slot 56 -- ProcessPreviousSVDOnProgramChange --
 *     is real, sec 10.153/batch 47); would need its own `readelf -r`
 *     derivation and a THIRD populated slot, on top of the brand-new
 *     `CSTGPatchMessageContext` class this function alone would require.
 * Deferred to a future batch with more time budget for the new-class
 * work, rather than attempted piecemeal here.
 */
void CSTGSlotVoiceData::UpdateGlobalTune(float) {}
/* Sec 10.92's own confirmed-real, deliberately deferred externs.
 * EmergencyFreeAllVoices is now real (sec 10.138). CSTGSlotVoiceData::
 * FreeSlotVoiceData(bool) is real now too, batch 17 -- see
 * src/engine/slot_voice_data_free.cpp (also its own two newly-discovered
 * dependencies, CSTGSmoother::CancelAllSlotVoiceDataCCSmoothers() and
 * CSTGPerformanceVars::NotifyAllKeysAndPedalsReleased(), plus
 * CSTGSlotVoiceData::AreAllKeysAndPedalsReleased() const, all in the
 * same file). */
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
/* CSTGVoiceAllocator::StealVoice(CSTGVoice*) -- confirmed real
 * (`.text+0x52b50`, 945 bytes), deliberately deferred, same treatment as
 * StealVoiceList above -- see oa_engine.h and
 * src/engine/streaming_event_manager.cpp (its only real caller in this
 * reconstruction, CSTGStreamingEvent::HandleErrorReading()). */
void CSTGVoiceAllocator::StealVoice(CSTGVoice *) {}
/* CSTGSmoother::CSTGSmoother() ctor is real now, batch 22 -- see
 * src/engine/smoother_ctor.cpp. Initialize() reconstructed for real, sec
 * 10.86 -- see src/engine/smoother_init.cpp. */
/* CSTGSmoother::CancelAllSmoothers() is real now, sec 10.154 -- see
 * src/engine/smoother_cancel.cpp. CSTGSmoother::FinalizeAllSmoothers()
 * is real now too, batch 61 -- see src/engine/smoother_finalize_all.cpp
 * (confirmed a hybrid of CancelAllSmoothers()'s own unlink/free-list-push/
 * buffer-zero logic plus FinalizeSmoother(node, true)'s own dispatch
 * call, both already-real siblings). */
/* Sec 10.95's own confirmed-real, deliberately deferred externs.
 * CSTGPerformanceVars::SetIsDying() is real now, batch 19 -- see
 * src/engine/performance_vars_set_is_dying.cpp (also its own three
 * newly-discovered dependencies, CSTGSlotVoiceData::SetIsDying()/
 * CSTGMIDIClockSync::DisableActivePerfClock()/CSTGPerformance::
 * SetIsDying(CSTGPerformanceVars*), all real too, plus four further
 * confirmed-real, deliberately deferred externs that call needs,
 * stubbed below). */
void CSTGPerformanceVars::EnterActivatingState() {}
/* Three of the four batch-19 OnPerformanceDeactivate externs are real now:
 *   - CSTGAudioInput::OnPerformanceDeactivate() (batch 20) -- see
 *     src/engine/audio_input_use_settings.cpp (counterpart of UseSettings).
 *   - CSTGMessageProcessor::ClearUnsolicitedMessages() (batch 20) -- see
 *     src/engine/message_processor.cpp (also its sole dependency,
 *     CSTGDelayedMsgSender::Clear(), a new class -- see oa_engine.h).
 *   - CSTGControllerInfo::OnPerformanceDeactivate() (batch 36) -- see
 *     src/engine/controller_info_perf_deactivate.cpp. Calling its own
 *     still-stubbed CSTGControllerInfo::SetPerfSwitch() sibling (below) is
 *     fine -- SetPerfSwitch's real 539-byte body (vtable dispatch + jump
 *     table) is what's deferred, not this caller; the existing safe no-op
 *     stub is exactly the right stand-in for it, same as any other
 *     confirmed-real-but-deferred sibling call elsewhere in this project.
 *     Its own newly-discovered dependency, CSTGControllerRTData::
 *     ResetPerfSwitches(), is also real now (same file).
 * The remaining one is still deferred, blocked by real vtable/callback
 * dispatch INSIDE ITS OWN BODY (not mere complexity):
 *   - CSTGFrontPanelSmoothers::OnPerformanceDeactivate() (523 bytes) makes
 *     two indirect calls through a stack-cached callback pointer
 *     (`call *0x24(%esp)` twice). */
void CSTGFrontPanelSmoothers::OnPerformanceDeactivate() {}
/* CSTGStreamingEventManager::CSTGStreamingEventManager()/Initialize() are
 * real now, sec 10.158 -- see src/engine/streaming_event_manager.cpp
 * (also its own newly-discovered dependency, CSTGStreamingEvent, a
 * brand-new class -- see oa_engine_init.h). sInstance storage moved there
 * too (was previously in engine_init.cpp). */
/* CSTGVectorEGBase::CSTGVectorEGBase() is real now, sec 10.148 -- see
 * src/engine/vector_eg_ctors.cpp (also corrects a real speculative claim
 * in oa_engine_init.h's own header comment, sec 10.66 -- see there). */
void CSTGVoiceAllocator::StealAllVoices() {}
/* CSTGVoiceAllocator::FreeStolenVoices() -- confirmed real caller found
 * 2026-07-25 (CSTGControlMsgHandler::StealAllVoices, see
 * oa_control_msg_handler.h), own body still deliberately deferred (same
 * reasoning as StealAllVoices() just above). */
void CSTGVoiceAllocator::FreeStolenVoices() {}
/* CSTGWaveSeqData::Initialize()/CSetListBank::Initialize() reconstructed
 * for real, sec 10.84 -- see src/engine/global.cpp. */
/* CSTGWaveSeqGenerator::CSTGWaveSeqGenerator()/Init() are real now, sec
 * 10.152 -- see src/engine/waveseq_generator.cpp. */
/* CSTGWaveSequence::CSTGWaveSequence()/CSetList::CSetList() are real
 * now, batch 12 -- see src/engine/waveseq_setlist_init.cpp (also their
 * own confirmed real vtables, _ZTV16CSTGWaveSequence/_ZTV8CSetList,
 * defined there). Neither ctor has its OWN standalone symbol in
 * OA_real.ko -- both are fully inlined at their one call site in
 * CSTGGlobal::CSTGGlobal(), see that file's own header comment. */
/* CSetList::Activate() is real now, batch 41 -- see src/engine/global.cpp
 * (right alongside its sibling CSetListSlot::Activate()). Its own callee,
 * CSetListEQ::SetBand(), is a confirmed-real, deliberately-out-of-scope
 * audio-DSP no-op stub -- see below, near the other confirmed-real-but-
 * deferred callees (matches the CSTGControllerInfo::SetPerfSwitch
 * precedent, sec 10.187: promoting a caller is safe when its callee is a
 * confirmed-real, already-covered no-op sibling). */
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
 * silently papered over here.
 *
 * UPDATE (sec 10.225): that predicted crash arrived live for
 * `CSTGAudioManager` specifically -- `CSTGEngine::Initialize()`
 * genuinely dispatches through its own slot 0, unconditionally, on
 * every real boot. Fixed in `CSTGAudioManager::CSTGAudioManager()`
 * (managers.cpp): that investigation also found this class's `virtual
 * ~CSTGAudioManager()` declaration was itself a real ABI mismatch bug
 * (the real vtable has no destructor slot at all -- see oa_engine.h's
 * corrected class comment), so `_ZTV16CSTGAudioManager` -- this exact
 * placeholder -- is now fully DEAD: the class is no longer C++-virtual
 * at all, uses a plain explicit `_vtablePtr` member instead, and no
 * longer references this symbol anywhere. Removed rather than left as
 * misleading dead code. The other vtable placeholders below remain
 * genuinely unpopulated -- none of them has yet been confirmed reached
 * by a real dispatch.
 *
 * UPDATE (sec 10.227): that same predicted crash then arrived live for
 * `CSTGVectorEGXOnly`/`CSTGVectorEGXY`/`CSTGVectorEGCC`/
 * `CSTGVectorEGBase` (`CSTGVectorManager::Initialize()`'s own confirmed
 * slot-0 dispatch, sec 10.65, unconditionally on every real boot, same
 * as `CSTGAudioManager` above). Fixed the other way this project
 * established for a genuinely real single-slot vtable
 * (`CSTGAudioDriverInterface`, sec 10.225): all four classes are now
 * genuinely C++-polymorphic (real `virtual void Init()`, see
 * oa_engine_init.h/vector_eg_ctors.cpp), so the compiler emits these
 * exact four mangled vtable symbols itself -- the four manual
 * placeholders that used to live here are now fully DEAD and are
 * removed rather than left as misleading (and now link-conflicting)
 * dead code. */
unsigned char _ZTV15CSTGRecordEvent[40];
/* _ZTV14CSTGAudioEvent -- needed now that CSTGAudioEvent::CSTGAudioEvent()
 * is real (sec 10.149, engine_init.cpp) and references it directly, same
 * 40-byte confirmed size (readelf) as its own derived sibling above. */
unsigned char _ZTV14CSTGAudioEvent[40];
/* _ZTV17CSTGPlaybackEvent -- needed now that CSTGPlaybackEvent::
 * CSTGPlaybackEvent() is real (sec 10.150, engine_init.cpp) and
 * references it directly, same confirmed 40-byte size (readelf,
 * `vtable for CSTGPlaybackEvent`) as its CSTGAudioEvent/CSTGRecordEvent
 * siblings above. */
unsigned char _ZTV17CSTGPlaybackEvent[40];
/* _ZTV18CSTGStreamingEvent -- needed now that CSTGStreamingEvent::
 * CSTGStreamingEvent() is real (sec 10.158, streaming_event_manager.cpp)
 * and references it directly, same confirmed 40-byte size (nm -CS) as its
 * CSTGAudioEvent/CSTGRecordEvent/CSTGPlaybackEvent siblings above. */
unsigned char _ZTV18CSTGStreamingEvent[40];
/* _ZTV20CSTGIntMIDIClockSync -- needed now that CSTGMIDIClockSync::
 * CSTGMIDIClockSync() is real (batch 21, midi_clock_sync.cpp) and
 * installs it directly on its own embedded CSTGIntMIDIClockSync
 * sub-object. Real confirmed 40-byte size (readelf), 8 real slots -- ALL
 * 8 slot targets are themselves reconstructed for real in
 * midi_clock_sync.cpp too, but nothing in this project dispatches
 * through this vtable yet, so it stays a safe zero-filled placeholder
 * per this project's established "install vs dispatch" rule (sec
 * 10.153) -- see midi_clock_sync.cpp's own header comment for the real
 * slot -> method mapping if a future pass ever needs to populate it. */
extern "C" unsigned char _ZTV20CSTGIntMIDIClockSync[40];
unsigned char _ZTV20CSTGIntMIDIClockSync[40];

/* _ZTV20CSTGExtMIDIClockSync -- needed now that
 * CSTGMidiInPort::CSTGMidiInPort() installs it directly on its own
 * embedded CSTGExtMIDIClockSync sub-object (this pass, +0x108). Real
 * confirmed 40-byte size (readelf), 8 real slots -- SAME "install vs
 * dispatch" rule as _ZTV20CSTGIntMIDIClockSync above: 10 of the 8+2
 * (13 total, incl. the 2 non-virtual UpdateFilteredTempo/
 * UpdateDynamicThresholds) real methods behind it are reconstructed for
 * real in midi_clock_sync.cpp, but nothing in this project dispatches
 * through this vtable yet, so it stays a safe zero-filled placeholder.
 * See midi_clock_sync.cpp/oa_engine_init.h for the real slot -> method
 * mapping and the 3 still-deferred slot targets (ProcessClock/
 * MeasureJitter/EstimateTempoAndPredictNextClock, stubbed below). */
extern "C" unsigned char _ZTV20CSTGExtMIDIClockSync[40];
unsigned char _ZTV20CSTGExtMIDIClockSync[40];

/* STGAPIFrontPanelStatus::sInstance -- confirmed real static pointer,
 * already set by setup_global_resources.cpp; definition (storage) not
 * yet homed anywhere. */
unsigned char *STGAPIFrontPanelStatus::sInstance;

/* CSTGMidiInPort::StartSysEx()/ReceiveSysExData(unsigned char) --
 * confirmed real OA.ko-internal symbols (.text+0xf5bd0/0xf5ed0, 368/1297
 * bytes), confirmed real call targets from the new
 * CSTGMidiInPortSerial::ReceiveByte()/ReceiveBytes() (batch: physical
 * MIDI-IN UART parser, src/engine/midi_in_port_serial.cpp). Own body is
 * a substantial, separate SysEx capture/forward state machine (also
 * pulls in the not-yet-declared EndSysExScan()/EndSysEx()) --
 * deliberately deferred no-op stubs, matching this file's own stated
 * convention. Safe as a no-op for now: with an empty body, incoming
 * SysEx bytes are silently dropped by the byte parser rather than
 * captured, but the running-status/system-common message path (this
 * batch's actual reconstruction target) is entirely unaffected. */
void CSTGMidiInPort::StartSysEx() { }
void CSTGMidiInPort::ReceiveSysExData(unsigned char) { }

/* CSTGMidiInPort::Activate(CSTGMidiQueue*)/Deactivate() are now REAL --
 * see src/engine/midi_in_port_serial.cpp (this pass). The prior no-op
 * stubs here (`disproportionate separate cluster`) turned out tractable
 * once actually disassembled: 358/5 bytes, no new dependency beyond the
 * already-real CSTGMidiQueue::Initialize()/SetDesc() and the newly-real
 * CSTGExtMIDIClockSync::Initialize() above.
 *
 * CSTGExtMIDIClockSync::ProcessClock()/MeasureJitter()/
 * EstimateTempoAndPredictNextClock() -- CONFIRMED real OA.ko-internal
 * symbols (.text+0x68650/0x68480/0x68130, 174/460/737 bytes), examined
 * in full (ProcessClock/MeasureJitter) or by size only
 * (EstimateTempoAndPredictNextClock) this pass. Deliberately deferred:
 * MeasureJitter() is a genuine x87 `fucomi`/`fcmovbe`/`fcmovnbe`
 * median-of-3 conditional-move sort over a 32-entry float ring, high
 * transcription risk; EstimateTempoAndPredictNextClock() is the largest
 * method in the class and was not examined in detail;
 * ProcessClock()'s own producer (an 8-entry incoming-clock timestamp
 * ring at fieldAt(0x40), fed by something not yet identified anywhere
 * in this project) is itself unresolved. None of the 3 are reachable
 * from anything this project currently reconstructs -- see
 * oa_engine_init.h's class comment for the full writeup. Safe as
 * no-ops here: nothing dispatches into this vtable yet (see its own
 * comment above), and Activate()'s own real body calls ONLY
 * Initialize() (already real), never these three. */
void CSTGExtMIDIClockSync::ProcessClock() { }
void CSTGExtMIDIClockSync::MeasureJitter() { }
void CSTGExtMIDIClockSync::EstimateTempoAndPredictNextClock() { }

/* ---------------------------------------------------------------------
 * 2026-07-27: real insmod-ability regression fix. A dynamic-sweep
 * live-boot test (see re-decompiler agent memory,
 * oa_audioinputmixer_vtable_literal8_bug_2026-07-27.md) found that
 * current HEAD's OA.ko literally CANNOT `insmod` on its own: the
 * `CSTGControllerInfo::ButtonPressHandler`/`AnalogControllerHandler`
 * cluster (batches 65/66/68 + follow-ups) reference ~35 already-
 * documented "confirmed real, deliberately deferred" externs (own
 * header comments in oa_global.h/oa_engine.h/oa_engine_init.h/
 * oa_keybed_init.h/oa_control_msg_handler.h/oa_calibration.h all
 * already say so) that were never actually given the matching no-op
 * stub body this file's own convention requires -- a pure oversight
 * (each batch that added a new deferred extern to a header forgot the
 * matching bar2_stubs.cpp entry), not a design change and not a
 * regression from a previously-real implementation (`git log -S` on
 * every one of these confirms no prior definition ever existed
 * anywhere in this project's history). Every body below is EMPTY/safe-
 * default exactly like every other stub in this file -- see each
 * symbol's own declaration-site header comment (cited above) for why
 * that specific default was chosen; nothing here is a new derivation.
 * ------------------------------------------------------------------- */

/* PushMessage(void*) -- oa_calibration.h/oa_control_msg_handler.h's own
 * "confirmed real, deliberately deferred" extern (same convention as
 * CSTGMidiInPortUSB below). Safe as a no-op: callers build a reply
 * packet on the stack and fire-and-forget into this; dropping it means
 * the UI simply never receives that one solicited reply, no crash. */
void PushMessage(void *) { }

/* ApplyKeybedCalibration(int,short) -- oa_keybed_init.h's own comment:
 * ground truth returns a calibrated value OR the confirmed real 0xffff
 * "no calibration data" sentinel. Always returning that sentinel here
 * is the faithful safe default (matches a real keybed board that has
 * never had SetupKeybedCalibration/CleanupKeybedCalibration wired up),
 * not an invented behavior -- callers already handle this sentinel by
 * leaving the value uncalibrated. */
short ApplyKeybedCalibration(int, short) { return (short)0xffff; }

/* SetupNKS4Calibration(void*,int) -- oa_setup_global_resources.h's own
 * deferred extern, called from the calibration-panel setup path
 * (setup_global_resources.cpp). No-op: calibration setup simply doesn't
 * run, matching this file's own established convention. */
void SetupNKS4Calibration(void *, int) { }

/* CSTGControllerInfo -- ButtonPressHandler/AnalogControllerHandler's own
 * deliberately-deferred callees (oa_global.h, see each declaration's own
 * comment for the confirmed real address/size and dispatch context).
 * All void except the three "gate ahead of dispatch" methods, which
 * return bool: false ("UI edit mode did NOT intercept this event") is
 * the safe default -- it makes the caller fall through to the already-
 * real per-button/per-device dispatch tables instead of short-
 * circuiting them, per each method's own documented return-value
 * semantics. */
void CSTGControllerInfo::SetMixerKnobMode(int) { }
void CSTGControllerInfo::SetSoloSelected(bool) { }
void CSTGControllerInfo::ResetAllKnobCCs() { }
void CSTGControllerInfo::ResetAllExtModeControllers() { }
bool CSTGControllerInfo::HandleEditInContextButton(unsigned int, bool) { return false; }
void CSTGControllerInfo::ProcessMixerSwitchPress(unsigned int, bool) { }
void CSTGControllerInfo::ChangeControlSurfaceMode(int) { }
void CSTGControllerInfo::ProcessPerfSwitchPress(int, bool) { }
void CSTGControllerInfo::ResetSolo() { }
bool CSTGControllerInfo::HandleEditInContextKnob(unsigned int, unsigned short, unsigned short) { return false; }
bool CSTGControllerInfo::HandleEditInContextSlider(unsigned int, unsigned short, unsigned short) { return false; }
void CSTGControllerInfo::SendExtModeSliderEvent(int, unsigned int, bool) { }
void CSTGControllerInfo::SendExtModeKnobEvent(int, unsigned int, bool) { }
void CSTGControllerInfo::SetRTKModeKnob(unsigned short, unsigned short, bool, int, bool) { }
void CSTGControllerInfo::ResetRTKModeKnob(unsigned short) { }
void CSTGControllerInfo::ProcessJoystickY(unsigned short) { }
void CSTGControllerInfo::AnalogTempoHandler(unsigned short, unsigned short) { }
void CSTGControllerInfo::AnalogKnobSetListEQHandler(unsigned int, unsigned short, unsigned short) { }
void CSTGControllerInfo::AnalogSliderSetListEQHandler(unsigned int, unsigned short, unsigned short) { }
void CSTGControllerInfo::AnalogKnobTAHandler(unsigned int, unsigned short, unsigned short) { }
void CSTGControllerInfo::AnalogSliderTAHandler(unsigned int, unsigned short, unsigned short) { }
void CSTGControllerInfo::AnalogKnobAInHandler(unsigned int, unsigned short, unsigned short) { }
void CSTGControllerInfo::AnalogSliderAInHandler(unsigned int, unsigned short, unsigned short) { }

/* CSTGControllerRTData -- AnalogControllerHandler's own deferred nested-
 * class methods and siblings (oa_global.h, see each declaration's own
 * comment). CJumpCatch::CheckPosition/CPedalFilter::Filter/
 * CPitchBendFilter::Filter all return bool "did this filter accept/
 * catch the value" -- false is the safe default (caller treats it as
 * "not yet", never dereferences on the true-only side effects). */
void CSTGControllerRTData::CJumpCatch::UpdateStatus() { }
bool CSTGControllerRTData::CJumpCatch::CheckPosition(int, bool) { return false; }
bool CSTGControllerRTData::CPedalFilter::Filter(unsigned char) { return false; }
bool CSTGControllerRTData::CPitchBendFilter::Filter(unsigned short) { return false; }
void CSTGControllerRTData::SendCCToKG(unsigned char, unsigned char) { }
void CSTGControllerRTData::SendCCToKG(unsigned char, unsigned char, unsigned char) { }
void CSTGControllerRTData::HandleFootPedalChange(unsigned char) { }
void CSTGControllerRTData::HandleFootSwitchChange(bool) { }
void CSTGControllerRTData::SendUnsolControl2MessageToUI(int, int, int, int) { }

/* CSTGMidiOutPortSerial::CanTransmitHardware()/TransmitHardwareByte() --
 * oa_engine_init.h's own comment: BOTH still `__cxa_pure_virtual` in
 * ground truth's own vtable (no further-derived hardware-backend class
 * exists anywhere in OA.ko -- reaching this path is a genuine kernel
 * BUG() dead end on REAL hardware too, not a gap in this
 * reconstruction). Unlike a real pure-virtual thunk (which the C++ ABI
 * always resolves to *some* symbol, so real hardware insmods fine and
 * only BUG()s if this dead path is ever actually reached), these are
 * plain non-virtual methods here (see that header comment for why:
 * modeling them as real C++ virtual would insert a compiler vtable
 * pointer that corrupts this class's confirmed real field layout) --
 * so a genuinely missing definition would block insmod outright, a
 * strictly WORSE failure mode than ground truth's own "loads fine,
 * BUG()s only if reached". CanTransmitHardware() always returning false
 * is the closer-to-ground-truth choice: it makes this genuinely-dead
 * path stay dead (TransmitHardwareByte is never actually reached
 * through the real CanSendRealTime/CanSendRegular gates above it),
 * rather than inventing new "successful transmit" behavior. */
bool CSTGMidiOutPortSerial::CanTransmitHardware() const { return false; }
void CSTGMidiOutPortSerial::TransmitHardwareByte(unsigned char) { }

/* CSTGMidiInPortUSB::ReceivePacket(USBMidiPacket) -- oa_engine.h's own
 * comment: confirmed real and called (CKorgUsbAudioDriverMidiPorts::
 * CMidiPortPair::InputCallback()), but this class's own body/fields are
 * a disproportionate separate cluster deliberately not reconstructed.
 * No-op: an incoming USB-MIDI-class packet is silently dropped rather
 * than dispatched, matching this file's own established convention. */
void CSTGMidiInPortUSB::ReceivePacket(USBMidiPacket) { }

/* SKSTGGate_NotifyKarmaSliderPosition(int) -- called from
 * CKGModuleParamMsgHandler::SetKnob1-8Value (oa_ckg_module_param_msg_
 * handler.h) as an unconditional-argument (0) tail call gated on the
 * caller's own UI mode. Confirmed real in ground truth (`.text` symbol,
 * own body a genuinely separate KARMA-slider-UI subsystem, not part of
 * the checked-write dispatch family this batch reconstructs) -- link-
 * satisfying no-op stub only, same convention as every other
 * confirmed-real-but-out-of-scope dependency in this file. */
void SKSTGGate_NotifyKarmaSliderPosition(int) { }
