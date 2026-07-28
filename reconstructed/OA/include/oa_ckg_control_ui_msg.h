// SPDX-License-Identifier: GPL-2.0
#ifndef OA_CKG_CONTROL_UI_MSG_H
#define OA_CKG_CONTROL_UI_MSG_H

#include "oa_ckg_module_param_msg_handler.h"	/* CKGEngine, CKGParamEdit */
#include "oa_engine_init.h"			/* CKGBankManager */

/*
 * oa_ckg_control_ui_msg.h  -  CKGControlMsgHandler + CKGUIMsgSender, the
 * KARMA UI control-message dispatch/send pair. Found while surveying for
 * the next dense cluster after the CKG*ParamMsgHandler family's closure
 * (2026-07-28): a sibling "CKGControlMsg"/"CKGUIMsgSender" convention,
 * structurally distinct from all three already-reconstructed
 * ParamMsgHandler classes -- no CSPREngine gate, no KARMA-perf record,
 * no checked-write skeleton. Instead:
 *
 *   - CKGControlMsgHandler::Xxx(const CKGControlMsg*) methods are plain
 *     UI-triggered *actions* (change song/combi/program, start/stop,
 *     RTC/GE housekeeping) that forward almost entirely into CKGEngine/
 *     CKGBankManager/CKGRTCHandler/CKGParamEdit -- this class holds
 *     almost no state of its own beyond 4 static "currently dumping X"
 *     guard booleans.
 *   - CKGUIMsgSender::Xxx(...) methods build a fixed-shape CSKMessage
 *     payload on the stack and hand it to the free function
 *     `KGOutGate_SendMessageToUI()` -- the send-side counterpart to
 *     however the UI layer originally *receives* CKGControlMsg/
 *     CKGModuleParamMsg/CKGCommonParamMsg traffic (out of scope; this
 *     project only sees the kernel side).
 *
 * `CKGControlMsgHandler::HandleMessage(CSKMessage*)` (real address NOT
 * cited here as a ".text+0x" literal on purpose -- the manifest
 * generator's own address heuristic would falsely credit it as done
 * just from the mention; see re-decompiler agent memory for the real
 * offset if resuming this deferral), 1181 bytes, a ~40-case jump-table
 * dispatcher over `msg[+0x8]`, most
 * cases inlining logic that duplicates -- not calls -- this class's own
 * separately-addressed Change-family/SetMode methods) is deliberately NOT
 * reconstructed this batch: it is a single large, self-contained
 * function with no dependents among the methods below, a clean
 * standalone target for a follow-up batch. Full disassembly already
 * transcribed and available in this batch's own working notes (see
 * re-decompiler agent memory) -- not a "ran out of time, forgot where I
 * was" deferral. `SharedMemProgramDump`/`SharedMemCombiDump`/
 * `SharedMemSongDump` are deferred alongside it for the same reason plus
 * one more: all three call `CKGProgramDownloader::HandleProgramDownload`/
 * `CKGCombiDownloader::HandleCombiDownload` with `this` reinterpreted
 * from `CKGControlMsg::m_mode` itself (a real, confirmed pointer-in-an-
 * int-field smuggling idiom -- NOT a `ms_poInstance`-style singleton
 * call, disproving an initial assumption) and, for the Combi/Song
 * variants, a 3rd `eSTGMsgPerfType` argument whose register/stack
 * position could not be pinned down with confidence from the two
 * observed call sites alone (regparm(3) has only 3 GP registers total,
 * already consumed by `this`+2 explicit args, yet no stack spill is
 * visible in either disassembly window) -- logged in DECOMPILE_ERRORS.md
 * rather than shipped as a guess.
 *
 * === CKGControlMsg ===
 * Opaque incoming message, 2 known int fields (offset-only, real field
 * names unconfirmed -- every consumer method reads one or both without
 * any accompanying accessor or comment revealing intended semantics):
 *   +0x00  m_mode   int  ChangeProgram/ChangeCombi/ChangeSong/
 *                        SetSendingBulkDump never read this at all;
 *                        SetMode()'s own dispatch key (0/1/2, translated
 *                        through a small fixed lookup table -- raw
 *                        0->eSTGMsgPerfType_Program, 1->eSTGMsgPerfType_
 *                        Combi, 2->eSTGMsgPerfType_Song, anything >2
 *                        ignored -- table contents read directly from
 *                        ground truth .rodata, not guessed);
 *                        NotifyUIOperation()'s own 3-way switch
 *                        (0/1/2 -> WritePerformance/DoCompare/nothing,
 *                        real ja-then-jump-table shape, not an if-chain);
 *                        NotifyGECategoryPopupStatus() as a plain
 *                        zero/nonzero test.
 *   +0x04  m_value  int  ChangeProgram/ChangeCombi/ChangeSong: 0xffff
 *                        sentinel guard (skip entirely unless equal);
 *                        NotifyGECategoryPopupStatus(): compared to 1 to
 *                        pick CloseGECategoryPopup(bool)'s own argument;
 *                        SetSendingBulkDump(): read as a byte, forwarded
 *                        as CKGParamEdit::SendForceKarmaOff()'s argument.
 */
struct CKGControlMsg {
	int m_mode;	/* +0x0 */
	int m_value;	/* +0x4 */
};

/* eSTGMsgPerfType is declared in oa_ckg_module_param_msg_handler.h
 * (CKGEngine::ChangePerformance() needs it and this header must not
 * include back). */

/*
 * CSKMessage -- opaque outbound UI-notification payload, the type
 * `KGOutGate_SendMessageToUI()`/`ASKPushMessage()` both take by pointer.
 * This batch never needs to READ one back (only builds and sends), so it
 * is modelled as a flat byte buffer sized to the largest shape actually
 * written (CKGUIMsgSender::SendCommonParamMessage/SendModuleParamMessage,
 * 0x30 bytes incl. padding) rather than a named-field struct -- two
 * genuinely different payload shapes are packed into it depending on
 * which CKGUIMsgSender method is building it (a "ParameterChangeMessage"
 * shape used by the Common/ModuleParam-forwarding methods, and a smaller
 * plain "UI action" shape used by everything else), and forcing both
 * into one named layout would be less faithful than writing each
 * method's own real byte offsets directly (see the .cpp). This mirrors
 * the project's existing convention for opaque cross-subsystem payloads
 * (e.g. CKGModuleParamMsg's own header comment, or CSTGSysExBuffer)
 * where the *sender's* offsets are what's confirmed, not a full class.
 */
struct CSKMessage {
	unsigned char raw[0x30];
};

/* Free functions this batch's methods call through. Real mangled names
 * confirmed via each call site's own R_386_PC32 relocation; both use
 * this module's global `-mregparm=3` convention like every other free
 * function declared elsewhere in this tree (oa_calibration.h etc). */
extern "C" void KGOutGate_SendMessageToUI(const CSKMessage *msg, bool immediate) __attribute__((regparm(3)));
extern "C" void KGOutGate_StopSendingToMIDIPort(bool stop) __attribute__((regparm(3)));
extern "C" void SPRMain_SetAllKARMAAndDrumTrack(bool enable) __attribute__((regparm(3)));

/*
 * CKGRTCHandler -- real-time-controller (KARMA RTC) singleton, discovered
 * via CKGControlMsgHandler::RestoreRTCBackupValue()/ResetAllRTCValue()'s
 * own `this IS the singleton` call shape (same idiom as CKGBankManager/
 * CSPREngine elsewhere in this tree). Own class layout out of scope.
 */
struct CKGRTCHandler {
	static unsigned char *ms_poInstance;
	void ChangePerformance();
	void ResetAllScene();

	/*
	 * 5 more real instance methods, discovered while reconstructing
	 * the CKGController/CKGSwitch/CKGKnob/CKGPad diamond-inheritance
	 * widget hierarchy (oa_ckg_switch_family.h) -- same
	 * cast-through-`ms_poInstance` idiom as above. GetBackupScene()/
	 * GetBackupControlBuffer() both return a raw byte-array pointer
	 * into this singleton's own backup-buffer storage (real callers
	 * read individual bits/bytes out of the returned pointer, own
	 * buffer layout otherwise out of scope). `+0xd4`
	 * (GetBackupScene()'s own field), `+0xdc` (a plain `int` current-
	 * scene-number field, NOT a pointer -- confirmed distinct from
	 * +0xd4 by CKGSceneSw::GetCurrentValue() reading it directly with
	 * no further dereference), `+0xe0`/`+0xe1` (2 real flag bytes,
	 * read directly as raw offsets by several AnalizeAndProcessXxx
	 * overrides in oa_ckg_switch_family.h, same convention as
	 * CKGEngine::ms_poInstance[0xb0] elsewhere in this project) are
	 * accessed via `ms_poInstance[N]` at each real call site rather
	 * than modeled as named fields here.
	 */
	unsigned char *GetBackupScene();
	int GetBackupSceneNumber(int module);
	unsigned char *GetBackupControlBuffer();
	void ResetCurrentScene();
	void ResetCurrentControlBuffer();
	void ResetChordAssignSwitch();

	/*
	 * 2 more real instance methods, discovered while reconstructing
	 * CSKSpecialMsgHandler::ProcessProgramChangeMessage() (oa_ckg_midi_
	 * msg_handler.h) -- same cast-through-`ms_poInstance` idiom as every
	 * other method above.
	 */
	void FlashBufferdValue();
	void StartBuffering();

	/*
	 * 2 more real instance methods, discovered reconstructing
	 * CSKMIDIInMsgHandler::CheckNoteMessageAndTriggerPad()/
	 * NotifyCCToKarmaController() (oa_ckg_midi_msg_handler.h) -- same
	 * cast-through-`ms_poInstance` idiom. Both forward a raw channel-
	 * message tuple into the KARMA RTC engine; `EChangeSource` here is
	 * NOT a per-controller "who changed me" tag like CKGController's
	 * own enum of the same name (a real, textually distinct type by
	 * mangling -- `N13CKGController13EChangeSourceE` -- but reused
	 * here since both real call sites pass literal small ints 1/2
	 * through it and no other CKGController::EChangeSource value is
	 * ever observed at these two call sites). Own return-value/body
	 * semantics otherwise out of scope.
	 */
	bool AnalizeAndProcessNoteMessage(int channel, int statusType, int note, int velocity,
					   int changeSource);
	void AnalizeAndProcessCCMessage(int channel, int statusType, int data1, int data2,
					 int changeSource);

	/*
	 * 2 more real instance methods, discovered while reconstructing
	 * CKGEngine (oa_ckg_module_param_msg_handler.h,
	 * src/engine/ckg_engine.cpp) -- same cast-through-`ms_poInstance`
	 * idiom as every other method above. Own bodies out of scope.
	 */
	int GetDestinationModule(int module);
	void ResetMIDIChordTrigger();
};

/*
 * CKGMIDIMsgProcessor -- KARMA-generated-CC-value tracker, a DIFFERENT
 * class from the already-declared `CSKMIDIMsgProcessor` below (both real,
 * both singletons, confirmed via two distinct relocations --
 * `_ZN19CKGMIDIMsgProcessor13ms_poInstanceE` vs
 * `_ZN19CSKMIDIMsgProcessor13ms_poInstanceE`). Previously a 3-method
 * opaque stand-in here; now a full real class (13/13 methods) in
 * oa_ckg_midi_msg_handler.h (included below), reached once its own 6
 * dependency classes (CKGMIDIOutMsgHandler and its 5 children) became
 * real types themselves -- see that header's own "=== CKGMIDIMsgProcessor
 * ===" section. This file's own call sites (ResetKarmaGeneratedCCValue/
 * KillAllDyingNotes/StoreCCMessage) still resolve correctly since the
 * real class is visible by the time this file's own
 * `#include "oa_ckg_midi_msg_handler.h"` line (below) is reached, same as
 * every other type declared past that point.
 */
struct CMIDIMessage;

/*
 * CSKMIDIMsgProcessor -- previously a 6-method opaque stand-in here (the
 * whole owning-and-pumping class for CSKMIDIPortMsgHandler/
 * CSKMIDILocalCtrlMsgHandler/CSKSpecialMsgHandler/CSKMIDIKarmaCtrlMsgHandler/
 * CSKPadNoteByMIDIPortMsgHandler/CSKPadNoteByLocalCtrlMsgHandler). Now a
 * full real class in oa_ckg_midi_msg_handler.h (included below), reached
 * once those 6 dependencies became real classes themselves -- see that
 * header's own "=== CSKMIDIMsgProcessor ===" section. All of THIS file's
 * own call sites (ProcessLocalControlChannelMessage/
 * ProcessKarmaControllerGeneratedChannelMessage/KillAllDyingNotes/
 * LeaveDownloadMode/StoreDyingNoteInfoFor{MIDPort,STG}) still resolve
 * correctly since the real class is visible by the time this file's own
 * `#include "oa_ckg_midi_msg_handler.h"` line (below) is reached, same as
 * every other type declared past that point.
 */

/*
 * CSKMIDIInMsgHandler -- forward-declared only here; the real class (with
 * ms_bShouldStopSendingNoteOnsToSTG as a genuine static member, used below
 * by SetSendingBulkDump() around KGOutGate_StopSendingToMIDIPort()) lives
 * in oa_ckg_midi_msg_handler.h, reconstructed in a later batch. Any TU
 * that references the static field must also include that header.
 */
class CSKMIDIInMsgHandler;

/*
 * CMIDIFlowParamHolder -- MIDI-flow-parameter singleton, only its
 * Start() entry point used here (FinishLoadingGEsAndTemplates()'s own
 * tail call). Own class layout out of scope.
 */
struct CMIDIFlowParamHolder {
	static unsigned char *ms_poThis;
	void Start();

	/*
	 * 2 more real instance methods, discovered while reconstructing
	 * CSKSpecialMsgHandler::ProcessProgramChangeMessage()/
	 * ProcessResetAllControllerMessage() (oa_ckg_midi_msg_handler.h) --
	 * same cast-through-`ms_poThis` idiom as Start() above. Own bodies
	 * out of scope.
	 */
	void SetCurrentVoiceMode();
	void ChangePerformance();

	/*
	 * Real instance methods discovered reconstructing
	 * CSKMIDIInMsgHandler (oa_ckg_midi_msg_handler.h) -- same
	 * cast-through-`ms_poThis` idiom as Start() above. Own bodies out
	 * of scope; `EStatus` real enumerator names unconfirmed beyond the
	 * 2 literal values (1/0) ProcessForDyingNote() passes.
	 */
	enum EStatus { eStatus_0 = 0, eStatus_1 = 1 };
	void SetStatus(EStatus status);
	int GetVoiceMode();
	int GetNumOfKARMAModule();
	int GetKARMARealInputChannel(int module);
	int GetKARMARealOutputChannel(int module);
	int GetRealInputLocalControllerChannel(int module);
	bool IsKARMAOn();
	bool IsKARMATimbreThruInternalAction(int module);
	int GetLocalControlChannel();

	/*
	 * Real instance methods discovered reconstructing
	 * CSKMIDILocalCtrlMsgHandler (oa_ckg_midi_msg_handler.h) -- same
	 * cast-through-`ms_poThis` idiom, all per-timbre (0-15) getters for
	 * the KARMA multi-timbral combi routing engine. Own bodies out of
	 * scope.
	 */
	int GetTimbreChannel(int timbre);
	int GetTimbreStatus(int timbre);
	int GetTimbreTranspose(int timbre);
	bool IsEnableTimbreNoteOn(int timbre);
	int GetTimbreBottomKey(int timbre);
	int GetTimbreTopKey(int timbre);
	int GetTimbreLowVelocity(int timbre);
	int GetTimbreHighVelocity(int timbre);
	bool IsEnableTimbrePitchBend(int timbre);
	bool IsEnableTimbreAftertouch(int timbre);
	bool IsEnableTimbreCC(int ccNumber, int timbre);
	bool IsKARMATimbreThru(int module);
	int GetCurrentTrackStatus();
};

/*
 * CSKSpecialMsgHandler/CSKMIDIMsgHandler/CSKSysExMsgHandler -- the real,
 * full MIDI-message dispatch classes (previously only CSKSpecialMsgHandler
 * existed here as a 1-field opaque stand-in). Declared in their own header
 * because CKGUIMsgSender below needs CSKSpecialMsgHandler's static gate
 * flag; that header is included here (rather than the other way around)
 * specifically so it can see CKGRTCHandler/CKGMIDIMsgProcessor/
 * CSKMIDIMsgProcessor/CMIDIFlowParamHolder/CKGEngine/CKGBankManager/
 * CSPREngine, all already declared above at this point in the file --
 * oa_ckg_midi_msg_handler.h's own #include of this header is a no-op
 * (include-guard skip) when reached from here, standard two-header mutual
 * reference resolved by processing order, not a real circular dependency.
 */
#include "oa_ckg_midi_msg_handler.h"

/*
 * === CKGControlMsgHandler ===
 * Class region starts just before `.text+0x3c7ed0` (the ctor, see the
 * .cpp) and runs up to (not cited as a literal range here, see comment
 * above about HandleMessage). Real static state: 4 file-scope
 * booleans guarding re-entrancy while a bulk (song/combi/program) dump is
 * in flight, all zero-initialized by the real ctor. Plus one real
 * PER-INSTANCE byte field at +0x4 (`m_savedDumpFlag`, confirmed by
 * SetSendingBulkDump()'s own disassembly reading/writing `this+4`
 * directly) -- a snapshot of CKGBankManager's own dump-in-progress flag,
 * saved when a bulk dump starts and restored when it ends. Not
 * initialized by the real ctor (only the vtable pointer and the 4
 * static booleans are) -- left equally uninitialized here to match.
 *
 * `HandleMessage(CSKMessage*)` and the three `SharedMem*Dump()` methods
 * deliberately not declared/implemented this batch (see header comment
 * above) -- absent here entirely rather than declared-but-undefined,
 * matching this project's "not-yet-done methods are simply absent"
 * convention.
 */
struct CKGControlMsgHandler {
	void *_vtablePtr;		/* +0x0, install-only, same convention
					 * as CKGModuleParamMsgHandler --
					 * nothing in this batch dispatches
					 * through it */
	unsigned char m_savedDumpFlag;	/* +0x4, see comment above */

	static bool ms_bIsNowDumpingSong;
	static bool ms_bIsNowDumpingCombi;
	static bool ms_bIsNowDumpingProg;
	static bool ms_bIsNowProcessingSoftPedalMessage;

	CKGControlMsgHandler();

	void ChangeProgram(const CKGControlMsg *msg);
	void ChangeCombi(const CKGControlMsg *msg);
	void ChangeSong(const CKGControlMsg *msg);
	void SetMode(const CKGControlMsg *msg);
	void Start(const CKGControlMsg *msg);
	void Stop(const CKGControlMsg *msg);
	void FinishLoadingGEsAndTemplates(const CKGControlMsg *msg);
	void RestoreRTCBackupValue(const CKGControlMsg *msg);
	void ResetAllRTCValue(const CKGControlMsg *msg);
	void NoteMapTableOctaveReplicate(const CKGControlMsg *msg);
	void ResetNoteMapTable(const CKGControlMsg *msg);
	void NotifyUIOperation(const CKGControlMsg *msg);
	void NotifyDiagnosticMode(const CKGControlMsg *msg);
	void ExecPageMenuCommand(const CKGControlMsg *msg);
	void UpdateRTCDisplay(const CKGControlMsg *msg);
	void UpdateRTCModelName(const CKGControlMsg *msg);
	void NotifyGECategoryPopupStatus(const CKGControlMsg *msg);
	void PrepareResetBuffer(const CKGControlMsg *msg);
	void UpdateUserGEs(const CKGControlMsg *msg);
	void UpdateUserTemplates(const CKGControlMsg *msg);
	void UpdateGEInfo(const CKGControlMsg *msg);
	void UpdateSoftPedalStatus(const CKGControlMsg *msg);
	void SetSendingBulkDump(const CKGControlMsg *msg);
};

/*
 * === CKGUIMsgSender ===
 * Starts at `.text+0x3c84e0` (ctor), ends at `.text+0x3c90a0`
 * (UpdateSoftPedalStatus, this class's last method -- CKGModuleParamMsgHandler's
 * own region begins immediately after and is NOT part of this class).
 * Stateless (trivial `ret`-only ctor, no fields) -- every method just
 * builds a CSKMessage on the stack and hands it to
 * KGOutGate_SendMessageToUI(). Two families:
 *
 *  (a) SendCommonParamMessage/SendModuleParamMessage and the six
 *      UpdateCommonParam/SetCommonParamMax/SetCommonParamMin/
 *      RefreshCommonParam/DimOnCommonParam/SetModuleParamMax/
 *      SetModuleParamMin/UpdateModuleParam/DimOnModuleParam wrappers --
 *      "ParameterChangeMessage" shape, msg_class 0x24 (Common) or 0x28
 *      (Module), sub_type always 5, a per-method literal opcode
 *      (2=Update/3=SetMax-or-Min/4=Refresh/5=Dim -- SetMax and SetMin
 *      share literal 3 for Common but 2/3 differ for Module, transcribed
 *      exactly per-method, not inferred from a naming pattern), and a
 *      real conditional: sent with `immediate=true` when opcode==5 (Dim)
 *      OR (`immediate=false` AND NONE of the 3 "now dumping" guards are
 *      set) -- when a dump guard IS set and opcode!=5, the message is
 *      silently dropped (no send at all). This is the one place in this
 *      batch where CKGUIMsgSender reads CKGControlMsgHandler's own
 *      static guards directly.
 *  (b) Everything else -- fixed msg_class 0x14 ("UI action"), always
 *      sent unconditionally with `immediate=false`, a per-method literal
 *      sub-opcode at the shape's own +0x14 slot. UpdateKarmaInitialInfo/
 *      UpdateRTCModelName use a slightly different sub-shape (2 trailing
 *      zero dwords, no CKGEngine::ms_poInstance+0xa0 pointer field).
 */
struct CKGUIMsgSender {
	CKGUIMsgSender();

	void SendCommonParamMessage(int msgId, long value, int operation, long arg4);
	void SendModuleParamMessage(int msgId, long deviceIndex, long value, int operation, long arg5);
	void UpdateCommonParam(int msgId, long value, long arg3, bool dummy);
	void SetCommonParamMax(int msgId, long value, long arg3);
	void SetCommonParamMin(int msgId, long value, long arg3);
	void RefreshCommonParam(int msgId, long value);
	void DimOnCommonParam(int msgId, long value, bool dim);
	void SetModuleParamMax(int msgId, long deviceIndex, long value, long arg4);
	void SetModuleParamMin(int msgId, long deviceIndex, long value, long arg4);
	void UpdateModuleParam(int msgId, long deviceIndex, long value, long arg4, bool dummy);
	void DimOnModuleParam(int msgId, long deviceIndex, long value, bool dim);

	void UpdateChordAssignLED(bool on);
	void ChangeGE(long geIndex);
	void ChangePerformance(bool dummy);
	void UpdateDynamicMIDIAction(long action);
	void ResetValuesInControlBuffer(int arg);
	void ResetCurrentScene();
	void UpdateNoteMapTable(long arg);
	void UpdateGERTParamNames();
	void UpdateKarmaInitialInfo();
	void NotifyPadStatus(int a, int b);
	void UpdateNoteMapSmallTable(int a, long b);
	void UpdateRTCModelName();
	void UpdateAutoClockSource(bool on);
	void UpdateSceneChangeCaption(int caption);
	void UpdateValuesForScene();
	void UpdateGEInfo(int a, int b);
	void UpdateSoftPedalStatus(bool on);
};

#endif /* OA_CKG_CONTROL_UI_MSG_H */
