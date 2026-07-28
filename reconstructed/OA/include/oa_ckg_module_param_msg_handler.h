// SPDX-License-Identifier: GPL-2.0
#ifndef OA_CKG_MODULE_PARAM_MSG_HANDLER_H
#define OA_CKG_MODULE_PARAM_MSG_HANDLER_H

#include "oa_engine_init.h"	/* CKGBankManager, CSPREngine */

/*
 * oa_ckg_module_param_msg_handler.h  -  CKGModuleParamMsgHandler: the KARMA
 * module-parameter "checked write" dispatch family. Discovered while
 * surveying for the next dense cluster after the STG value-getter family
 * (2026-07-28) -- 131 real `Set*`/`GetKarma*`/`ShouldStoreToBackup` methods
 * plus a real `HandleMessage` dispatcher, `.text+0x3c90f0`..`.text+0x3d05ac`,
 * all `T` (global, not weak/COMDAT) linkage.
 *
 * This is the WRITE-side sibling of the already-reconstructed
 * `CKGSeqBackupModuleParam` READ-side class (src/engine/karma_seq_backup.cpp,
 * oa_karma_seq_backup.h) -- both classes access the exact same real
 * per-module KARMA-perf record layout (confirmed by cross-referencing every
 * field offset/shift/mask below directly against CKGSeqBackupModuleParam's
 * own already-verified `Set*()` bodies, which read the identical offsets;
 * that class's own header explicitly documents "m_default consulted
 * unconditionally... by SetSwName/SetKnobName, the Sw/Knob-assignment value
 * groups, and the RTParam group's A/B/C/D/Min/Max/Value fields", the exact
 * same field subset this class's own disassembly shows performing a DUAL
 * shadow write below). No field offset in this header was hand-derived from
 * scratch -- every one was cross-checked against that sibling file's
 * ground truth before being used here.
 *
 * === CKGModuleParamMsg (2nd arg to every method below) ===
 * Opaque incoming message, real fields confirmed by offset only:
 *   +0x00  m_kind             int  bank-type tag, read only by
 *                                  GetKarmaModule(): 1=Program, 2=Seq,
 *                                  anything else (incl. the real
 *                                  fallthrough case 0)=Combi -- inferred
 *                                  from which CKGBankManager::Get*Karma
 *                                  PerfModule() overload it dispatches to.
 *   +0x04  m_bankId           int  raw eSTGCombiBankId/eSTGProgramBankId
 *                                  value, read only by GetKarmaModule()'s
 *                                  own kind==1(Program)/else(Combi)
 *                                  branches (the kind==2/Seq branch
 *                                  doesn't use it at all). Declared `int`
 *                                  here and cast at each use site --
 *                                  CKGBankManager's own two callee
 *                                  declarations (oa_engine_init.h) carry
 *                                  the real enum types for correct
 *                                  mangling.
 *   +0x08  m_karmaIndexOrSentinel int  GetKarmaPerfModuleForSeqBackup()-only:
 *                                  a Seq-Karma-perf index, or the sentinel
 *                                  0xffff meaning "use CKGBankManager's own
 *                                  default index instead".
 *   +0x0c  m_deviceIndex      int  KARMA module/device id. Read as a byte
 *                                  (movzx) everywhere it feeds a Send*()
 *                                  call's first argument or the SysEx
 *                                  fallback's own device-id argument; read
 *                                  as a full dword only by
 *                                  GetKarmaModule()/GetKarmaPerfModuleFor
 *                                  SeqBackup() (`* 0x2e8` record-stride
 *                                  multiply).
 *   +0x10  m_index            unsigned int  per-call dynamic slot/knob/
 *                                  switch/chord-note/dynMIDI-mapping index
 *                                  -- the SAME conceptual field as
 *                                  CKGSeqBackupModuleParam's own m_index,
 *                                  just supplied per-message here instead
 *                                  of pre-populated on `this`. Used both as
 *                                  a ctx-indexed record-array index (when
 *                                  the target field lives in a per-slot
 *                                  record) and, unrelatedly, as the SysEx
 *                                  fallback's own module-array index arg.
 *   +0x18  m_value            int  the value being set. Read at whatever
 *                                  width/signedness the destination field
 *                                  and the Send*() call's own parameter
 *                                  type need (byte/word, signed/unsigned) --
 *                                  the underlying storage is a full dword,
 *                                  narrower reads are real truncating
 *                                  register-width choices in ground truth,
 *                                  not a modelling simplification.
 */
struct CKGModuleParamMsg {
	int m_kind;			/* +0x0 */
	int m_bankId;			/* +0x4 */
	int m_karmaIndexOrSentinel;	/* +0x8 */
	int m_deviceIndex;		/* +0xc */
	unsigned int m_index;		/* +0x10 */
	int reserved_0x14;		/* +0x14, never observed read */
	int m_value;			/* +0x18 */
};

/*
 * === CKGModuleParamMsgHandler ===
 *   +0x00  _vtablePtr        void*  install-only placeholder, same
 *                                   "real vtable pointer, not modelled as
 *                                   C++ `virtual`" convention as
 *                                   CSTGControlMsgHandler/CSTGDrumPadClient
 *                                   (see oa_control_msg_handler.h) --
 *                                   `HandleMessage()`'s own presence
 *                                   strongly implies a real dispatch
 *                                   vtable, but nothing in this batch's own
 *                                   113 methods dispatches through it.
 *   +0x04  m_liveRecord      byte*  the "live" per-module KARMA-perf record
 *                                   -- every method's unconditional,
 *                                   always-executed primary write target
 *                                   (matches CKGSeqBackupModuleParam's own
 *                                   `m_source`).
 *   +0x08  m_defaultRecordA  byte*  "default" shadow record copy #1.
 *                                   Non-NULL-tested as part of the
 *                                   SysEx-override-attempt gate (see
 *                                   ShouldAttemptSysExShadowWrite() below),
 *                                   and, when the gate passes AND the SysEx
 *                                   lookup misses, receives a second,
 *                                   conditional write mirroring the primary
 *                                   one (matches CKGSeqBackupModuleParam's
 *                                   own `m_default`).
 *   +0x0c  m_defaultRecordB  byte*  "default" shadow record copy #2 --
 *                                   receives a THIRD write, identical in
 *                                   shape to m_defaultRecordA's, but ONLY
 *                                   for the field subset
 *                                   CKGSeqBackupModuleParam's own header
 *                                   already flags as "m_default consulted
 *                                   unconditionally": SetValue/MinValue/
 *                                   MaxValue, SetKnob/Polarity (+ both
 *                                   ForModuleControl siblings), SetSwName/
 *                                   SetKnobName, and the (out-of-scope-for-
 *                                   this-batch) SetKnob*Value/SetSw*Value/
 *                                   SetScene/SetLinkedSceneId families.
 *                                   Every other field writes m_defaultRecordA
 *                                   only.
 *   +0x10  m_moduleIndex     int    passed as CSPRSysExBufManager::
 *                                   GetValue()'s own 6th argument
 *                                   (`this->0x10` in ground truth) --
 *                                   presumably this handler instance's own
 *                                   fixed KARMA-module slot number.
 *
 * === Shared control-flow skeleton (Shape B: the 85 "checked write"
 * methods reconstructed this batch) ===
 * Every one of these 85 methods -- confirmed via a byte-for-byte
 * (register-allocation aside) diff across many independently-checked
 * instances (SetTranspose/SetOutputCh/SetKeyTop/SetModPercent/SetGenCC/
 * SetSeed/SetValue/SetSwName/SetSceneIsLinked, spanning every
 * fixed-offset/ctx-indexed/bitfield/single-shadow/dual-shadow
 * combination) -- follows the exact same 3-part shape:
 *   1. Unconditional primary write: field(m_liveRecord) = msg->m_value
 *      (or, for a bitfield, a read-modify-write against m_liveRecord).
 *   2. IF ShouldAttemptSysExShadowWrite(): call CSPRSysExBufManager's
 *      GetValue() through CSPRMIDIMsgProcessor::ms_poSysExPlayBuf; if it
 *      reports a miss (returns 0), set CKGUIMsgProcessor::ms_poInstance's
 *      +0x74 flag and repeat the same field write against m_defaultRecordA
 *      (and, for the dual-shadow field subset, m_defaultRecordB too).
 *   3. IF CKGEngine::ms_poInstance's +0xb0 "edit suppressed" byte is 0:
 *      call the field's own CKGParamEdit::SendXxx() (through
 *      CKGEngine::ms_poKGParamEdit) with (msg->m_deviceIndex, msg->m_value)
 *      for a 2-arg Send, or (msg->m_deviceIndex, msg->m_index,
 *      msg->m_value) for a 3-arg one, then call
 *      CKGUIMsgProcessor::ms_poInstance->NotifyAfterEdit(). A handful of
 *      fields (SetGE/SetTZoneBypass/SetSwName/SetKnobName) skip the
 *      Send*() call but still call NotifyAfterEdit() when unsuppressed.
 * Steps 2 and 3 always both run in sequence (2 is not an early-return
 * alternative to 3) -- confirmed by every fallback block's own trailing
 * `jmp` landing exactly on step 3's own entry point, not on the function's
 * `ret`.
 *
 * Shape A (the 24 SetModified{Knob1-8,Sw1-8Value,Sw1-8Status} methods) is
 * simpler: an unconditional ctx-indexed (stride 10) field write against
 * m_liveRecord ONLY -- no CSPREngine/CKGUIMsgProcessor/CKGEngine check, no
 * Send*() call, no shadow write of any kind. Matches
 * CKGSeqBackupModuleParam's own SetModified* read-side siblings exactly
 * (same stride-10 array, same 0x294/0x295/0x296.. offsets).
 *
 * === Deliberately excluded this batch (documented, not reconstructed) ===
 * SetKnob1Value..SetKnob8Value / SetSw1Value..SetSw8Value (Shape
 * C/D: same Shape-B skeleton PLUS a `CKGParamEdit::GetRTParmBufferSelectId()`
 * indirection and, for Knob, a conditional `SKSTGGate_
 * NotifyKarmaSliderPosition()` tail call -- fully traced, not yet written
 * up, good next-batch target). SetScene/SetLinkedSceneId (real
 * idx-dependent packed-nibble/multi-branch logic, ~0x2c1/0x24d bytes,
 * same complexity class as the STG family's own SetLinkedSceneId-style
 * outliers -- SetLinkedSceneId's read-side semantics ARE already known
 * via CKGSeqBackupModuleParam::SetLinkedSceneId's own hand-reconstructed
 * body, only the write-side's own dual-branch shadow copy needs tracing).
 * `HandleMessage()`/ctor/dtor: real dispatcher/plumbing, same
 * "confirmed real, deliberately deferred" treatment as every other
 * *MsgHandler class in this project (see oa_control_msg_handler.h).
 */
struct CKGModuleParamMsgHandler {
	void *_vtablePtr;		/* +0x0, install-only, see comment above */
	unsigned char *m_liveRecord;	/* +0x4 */
	unsigned char *m_defaultRecordA;/* +0x8 */
	unsigned char *m_defaultRecordB;/* +0xc */
	int m_moduleIndex;		/* +0x10 */

	void SetGE(const CKGModuleParamMsg *msg);
	void SetSolo(const CKGModuleParamMsg *msg);
	void SetInputCh(const CKGModuleParamMsg *msg);
	void SetOutputCh(const CKGModuleParamMsg *msg);
	void SetKeyTop(const CKGModuleParamMsg *msg);
	void SetKeyBottom(const CKGModuleParamMsg *msg);
	void SetRxBend(const CKGModuleParamMsg *msg);
	void SetRxAfter(const CKGModuleParamMsg *msg);
	void SetRxDamper(const CKGModuleParamMsg *msg);
	void SetRxJSYP(const CKGModuleParamMsg *msg);
	void SetRxJSYM(const CKGModuleParamMsg *msg);
	void SetRxRibbon(const CKGModuleParamMsg *msg);
	void SetRxOther(const CKGModuleParamMsg *msg);
	void SetTxBend(const CKGModuleParamMsg *msg);
	void SetTxCCA(const CKGModuleParamMsg *msg);
	void SetTxCCB(const CKGModuleParamMsg *msg);
	void SetTxEnv1(const CKGModuleParamMsg *msg);
	void SetTxEnv2(const CKGModuleParamMsg *msg);
	void SetTxEnv3(const CKGModuleParamMsg *msg);
	void SetTxNote(const CKGModuleParamMsg *msg);
	void SetTxWaveform(const CKGModuleParamMsg *msg);
	void SetTranspose(const CKGModuleParamMsg *msg);
	void SetCollapse(const CKGModuleParamMsg *msg);
	void SetForceRangeWrap(const CKGModuleParamMsg *msg);
	void SetTZoneBypass(const CKGModuleParamMsg *msg);
	void SetDelayTime(const CKGModuleParamMsg *msg);
	void SetDelayMode(const CKGModuleParamMsg *msg);
	void SetRun(const CKGModuleParamMsg *msg);
	void SetKbdInZone(const CKGModuleParamMsg *msg);
	void SetKbdOutZone(const CKGModuleParamMsg *msg);
	void SetQuantize(const CKGModuleParamMsg *msg);
	void SetThru(const CKGModuleParamMsg *msg);
	void SetRootPosition(const CKGModuleParamMsg *msg);
	void SetGenCC(const CKGModuleParamMsg *msg);
	void SetGenCCValue(const CKGModuleParamMsg *msg);
	void SetNoteTrig(const CKGModuleParamMsg *msg);
	void SetNoteLatch(const CKGModuleParamMsg *msg);
	void SetEnv1Trig(const CKGModuleParamMsg *msg);
	void SetEnv2Trig(const CKGModuleParamMsg *msg);
	void SetEnv3Trig(const CKGModuleParamMsg *msg);
	void SetEnv1Latch(const CKGModuleParamMsg *msg);
	void SetEnv2Latch(const CKGModuleParamMsg *msg);
	void SetEnv3Latch(const CKGModuleParamMsg *msg);
	void SetClkAdvMode(const CKGModuleParamMsg *msg);
	void SetClkAdvSize(const CKGModuleParamMsg *msg);
	void SetClkAdvCtrig(const CKGModuleParamMsg *msg);
	void SetClkAdvVSence(const CKGModuleParamMsg *msg);
	void SetTrigModule(const CKGModuleParamMsg *msg);
	void SetModPercent(const CKGModuleParamMsg *msg);
	void SetModCutoff(const CKGModuleParamMsg *msg);
	void SetKIZoneTrans(const CKGModuleParamMsg *msg);
	void SetKOZoneTrans(const CKGModuleParamMsg *msg);
	void SetRndRhythm(const CKGModuleParamMsg *msg);
	void SetRndDuration(const CKGModuleParamMsg *msg);
	void SetRndNote(const CKGModuleParamMsg *msg);
	void SetRndCluster(const CKGModuleParamMsg *msg);
	void SetRndVelocity(const CKGModuleParamMsg *msg);
	void SetRndPan(const CKGModuleParamMsg *msg);
	void SetRndDrum(const CKGModuleParamMsg *msg);
	void SetRndWaveform(const CKGModuleParamMsg *msg);
	void SetSeed(const CKGModuleParamMsg *msg);
	void SetFreezeLoop(const CKGModuleParamMsg *msg);
	void SetFreezeRetrig(const CKGModuleParamMsg *msg);
	void SetUseGChAlso(const CKGModuleParamMsg *msg);
	void SetNoteMap(const CKGModuleParamMsg *msg);
	void SetNoteMapTranspose(const CKGModuleParamMsg *msg);
	void SetNoteMapOnMode(const CKGModuleParamMsg *msg);
	void SetNoteMapChdTrack(const CKGModuleParamMsg *msg);
	void SetNoteMapKbdTrack(const CKGModuleParamMsg *msg);
	void SetUseNoteOffs(const CKGModuleParamMsg *msg);
	void SetQuantizeWindow(const CKGModuleParamMsg *msg);
	void SetLinkToDT(const CKGModuleParamMsg *msg);
	void SetValue(const CKGModuleParamMsg *msg);
	void SetMinValue(const CKGModuleParamMsg *msg);
	void SetMaxValue(const CKGModuleParamMsg *msg);
	void SetKnob(const CKGModuleParamMsg *msg);
	void SetPolarity(const CKGModuleParamMsg *msg);
	void SetValueForModuleControl(const CKGModuleParamMsg *msg);
	void SetMinValueForModuleControl(const CKGModuleParamMsg *msg);
	void SetMaxValueForModuleControl(const CKGModuleParamMsg *msg);
	void SetKnobForModuleControl(const CKGModuleParamMsg *msg);
	void SetPolarityForModuleControl(const CKGModuleParamMsg *msg);
	void SetSceneIsLinked(const CKGModuleParamMsg *msg);
	void SetRTCIsLinked(const CKGModuleParamMsg *msg);
	void SetSwName(const CKGModuleParamMsg *msg);
	void SetKnobName(const CKGModuleParamMsg *msg);

	void SetModifiedKnob1(const CKGModuleParamMsg *msg);
	void SetModifiedKnob2(const CKGModuleParamMsg *msg);
	void SetModifiedKnob3(const CKGModuleParamMsg *msg);
	void SetModifiedKnob4(const CKGModuleParamMsg *msg);
	void SetModifiedKnob5(const CKGModuleParamMsg *msg);
	void SetModifiedKnob6(const CKGModuleParamMsg *msg);
	void SetModifiedKnob7(const CKGModuleParamMsg *msg);
	void SetModifiedKnob8(const CKGModuleParamMsg *msg);
	void SetModifiedSw1Value(const CKGModuleParamMsg *msg);
	void SetModifiedSw2Value(const CKGModuleParamMsg *msg);
	void SetModifiedSw3Value(const CKGModuleParamMsg *msg);
	void SetModifiedSw4Value(const CKGModuleParamMsg *msg);
	void SetModifiedSw5Value(const CKGModuleParamMsg *msg);
	void SetModifiedSw6Value(const CKGModuleParamMsg *msg);
	void SetModifiedSw7Value(const CKGModuleParamMsg *msg);
	void SetModifiedSw8Value(const CKGModuleParamMsg *msg);
	void SetModifiedSw1Status(const CKGModuleParamMsg *msg);
	void SetModifiedSw2Status(const CKGModuleParamMsg *msg);
	void SetModifiedSw3Status(const CKGModuleParamMsg *msg);
	void SetModifiedSw4Status(const CKGModuleParamMsg *msg);
	void SetModifiedSw5Status(const CKGModuleParamMsg *msg);
	void SetModifiedSw6Status(const CKGModuleParamMsg *msg);
	void SetModifiedSw7Status(const CKGModuleParamMsg *msg);
	void SetModifiedSw8Status(const CKGModuleParamMsg *msg);

	bool ShouldStoreToBackup(const CKGModuleParamMsg *msg);
	void *GetKarmaModule(const CKGModuleParamMsg *msg);
	void *GetKarmaPerfModuleForSeqBackup(const CKGModuleParamMsg *msg);

	/* Shared skeleton helpers -- not real ground-truth functions of their
	 * own, just factored-out copies of the byte-identical shared blocks
	 * every Shape-B method's own body repeats (see header comment above). */
	bool ShouldAttemptSysExShadowWrite() const;
	bool SysExShadowWriteIsNeeded(const CKGModuleParamMsg *msg) const;
};

/*
 * CKGEngine -- the KARMA/UI edit-suppression singleton. `+0xb0` gates
 * every Shape-B method's own CKGParamEdit::SendXxx()+NotifyAfterEdit()
 * pair ("edit suppressed" byte, guessed from behaviour/class name -- real
 * meaning not confirmed beyond the byte offset). `ms_poKGParamEdit` is the
 * real `CKGParamEdit` instance every SendXxx() call dispatches through.
 */
struct CKGParamEdit;
struct CKGEngine {
	static unsigned char *ms_poInstance;
	static CKGParamEdit *ms_poKGParamEdit;
};

/*
 * CKGParamEdit -- the real target of every Shape-B method's own SendXxx()
 * call. Opaque stand-in, same "declare only the real methods actually
 * called from this batch, bodies stay genuinely unresolved" convention as
 * CKGBankManager (oa_engine_init.h) -- own class layout entirely out of
 * scope here.
 */
struct CKGParamEdit {
#include "oa_ckg_param_edit_send_decls.inc"
};

/*
 * CKGUIMsgProcessor -- the UI-notification singleton. `+0x6c` (a real
 * dword "current record-buffer UI mode" value, tested against the
 * literal 4 and the {8,9,10} range) and `+0x74` (a real byte flag, set
 * when a SysEx-shadow write actually happens) are read/written directly
 * as raw offsets, same "unsigned char *ms_poInstance" idiom as
 * CSPREngine/CKGBankManager; `NotifyAfterEdit()` is a real instance
 * method called through it (`((CKGUIMsgProcessor*)ms_poInstance)->
 * NotifyAfterEdit()`, matching oa_karma_seq_backup.h's own established
 * cast-through-raw-pointer idiom) -- own body genuinely out of scope.
 */
struct CKGUIMsgProcessor {
	static unsigned char *ms_poInstance;
	void NotifyAfterEdit();
};

/*
 * CSPRMIDIMsgProcessor::ms_poSysExPlayBuf -- the real `this` for every
 * Shape-B method's own SysEx-shadow-write attempt
 * (CSPRSysExBufManager::GetValue()). Declared as its own tiny singleton
 * holder, same convention as the others above.
 */
struct CSPRSysExBufManager;
struct CSPRMIDIMsgProcessor {
	static CSPRSysExBufManager *ms_poSysExPlayBuf;
};

/*
 * CSPRSysExBufManager::GetValue() -- real 8-argument (`i,i,i,i,i,i,i,long*`)
 * instance method, own body out of scope (generic SysEx record-buffer
 * lookup infrastructure, not part of this batch). Every Shape-B call site
 * passes the exact same first-2-args/last-arg shape (`0x6d`, a signed
 * byte read from `CKGBankManager::ms_poInstance[0x97c747]`, ..., `5`,
 * `&local`) -- see SysExShadowWriteIsNeeded()'s own body in the .cpp.
 */
struct CSPRSysExBufManager {
	char GetValue(int a, int b, int c, int d, int e, int f, int g, long *out);
};

#endif /* OA_CKG_MODULE_PARAM_MSG_HANDLER_H */
