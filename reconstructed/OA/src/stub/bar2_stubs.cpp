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
void CSTGPianoModel::RescanPianoTypes() {}

/* ---- Engine subsystem managers (engine.cpp, sec 10.13/10.58) ---- */
/* CSTGVoiceModelManager::~CSTGVoiceModelManager() is real now, sec
 * 10.147 -- see managers.cpp. */
void CSTGEffectManager::Initialize() {}
void CSTGEffectManager::RunEffects() {}
/* CSTGHDRManager::Initialize() is real now, batch 22 -- see
 * hdr_manager_init.cpp. */
/* ProcessCommands()/Initialize() (CSTGMonitorMixer/CSTGHDRFileWriter/
 * CSTGSamplingDaemon/CSTGFileCloser/CSTGCDWorker) are real now, sec
 * 10.144 -- see managers.cpp. ProcessCommands() calls three still-
 * deferred siblings, stubbed below. */
void CSTGHDRManager::ProcessPlaybackCommands() {}
/* CSTGHDRManager::ProcessRecordCommands() is real now, batch 15 -- see
 * src/engine/hdr_record_track.cpp (also introduces CSTGRecordTrack::
 * Start()/Pause()/Stop(), StandbyRec() deliberately deferred). */
void CSTGHDRManager::ProcessSamplerCommands() {}
void CSTGHDRManager::ProcessHDRRecord() {}
void CSTGMonitorMixer::RunMonitors() {}
void CSTGFileOpener::Initialize() {}
void CSTGFileOpener::ProcessCommands() {}
void CSTGFileCloser::ProcessCommands() {}
/* CSTGHDRFileReader::Initialize()/CSTGStreamingFileReader::Initialize()
 * are real now too, sec 10.151 -- see managers.cpp. Their own
 * ProcessCommands() siblings are still deferred (each dispatches
 * through a further not-yet-recovered ring-buffer/vtable-callback
 * pattern, same class of scope as CSTGHDRManager's own three still-
 * deferred sub-methods above). */
void CSTGHDRFileReader::ProcessCommands() {}
void CSTGHDRFileWriter::ProcessCommands() {}
void CSTGStreamingFileReader::ProcessCommands() {}
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
 * managers.cpp (right after CSTGCDWorker::ProcessCommands()). Its own
 * FIVE siblings (CSTGFileCloser/CSTGHDRFileReader/CSTGHDRFileWriter/
 * CSTGStreamingFileReader::ProcessCommands()) all still dispatch through
 * a not-yet-recovered vtable or pointer-to-member-function table and
 * remain deliberately stubbed below. */
void CSTGMidiPortManager::Initialize() {}
CSTGMidiPortManager::~CSTGMidiPortManager() {}
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
void CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors() {}
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
void CSTGControllerRTData::SetAudioInSolo(unsigned int, bool) {}
void CSTGControllerRTData::ResetSendKnobsJumpCatch() {}
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
void CSTGControllerInfo::SendUnsolicitedUIParam(unsigned int, unsigned int, long, int) {}
void CSTGControllerRTData::OnPerformanceActivate(CSTGPerformance &) {}
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
void CSTGSmoother::FinalizeSmoother(void *, bool) {}
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
void CSTGGlobal::InitializePerformances() {}
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
void CSTGPerformanceVarsManager::Initialize() {}
unsigned char CSTGPerformanceVarsManager::sInstance[12];
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
void CSTGSlotVoiceData::RunVoiceModelFeedback() {}
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
/* CSTGSmoother::CSTGSmoother() ctor is real now, batch 22 -- see
 * src/engine/smoother_ctor.cpp. Initialize() reconstructed for real, sec
 * 10.86 -- see src/engine/smoother_init.cpp. */
/* CSTGSmoother::CancelAllSmoothers() is real now, sec 10.154 -- see
 * src/engine/smoother_cancel.cpp. */
/* Sec 10.95's own confirmed-real, deliberately deferred externs.
 * CSTGPerformanceVars::SetIsDying() is real now, batch 19 -- see
 * src/engine/performance_vars_set_is_dying.cpp (also its own three
 * newly-discovered dependencies, CSTGSlotVoiceData::SetIsDying()/
 * CSTGMIDIClockSync::DisableActivePerfClock()/CSTGPerformance::
 * SetIsDying(CSTGPerformanceVars*), all real too, plus four further
 * confirmed-real, deliberately deferred externs that call needs,
 * stubbed below). */
void CSTGSmoother::FinalizeAllSmoothers() {}
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

/* STGAPIFrontPanelStatus::sInstance -- confirmed real static pointer,
 * already set by setup_global_resources.cpp; definition (storage) not
 * yet homed anywhere. */
unsigned char *STGAPIFrontPanelStatus::sInstance;
