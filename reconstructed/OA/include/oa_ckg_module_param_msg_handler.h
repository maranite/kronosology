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
 * === Shape C/D: SetKnob1Value..SetKnob8Value / SetSw1Value..SetSw8Value
 * (batch 2, 2026-07-28) ===
 * Same Shape-B skeleton (reuses ShouldAttemptSysExShadowWrite()/
 * SysExShadowWriteIsNeeded(), same dual-shadow rule -- both ctx-indexed via
 * `idx*9` AND m_default-based on the read side, confirmed against
 * CKGSeqBackupModuleParam::SetKnob1Value..8Value/SetSw1Value..8Value, so
 * every one of these 16 gets a defaultRecordB write too), but with the
 * Send call preceded by a `CKGParamEdit::GetRTParmBufferSelectId(msg->
 * m_deviceIndex)` indirection whose result feeds the Send call's own first
 * argument INSTEAD of the raw device index. Field: byte at
 * `m_liveRecord + msg->m_index*9 + (0x149..0x150)` for KnobN (N=1..8,
 * plain byte, no bitfield); `m_liveRecord + msg->m_index*9 + 0x148`,
 * bit(N-1), for SwN (N=1..8) -- ground truth ORs the msg->m_value byte
 * SHIFTED left by (N-1) directly into the cleared bit position without
 * masking to a single bit first (`(*p & ~(1<<(N-1))) | (value<<(N-1))`,
 * truncated to a byte by the store) -- preserved verbatim, not "cleaned up"
 * to `(value&1)<<(N-1)`, since ground truth genuinely doesn't mask.
 * SendKnob(selectId, (int)msg->m_index, 0, msg->m_value, false) for Knob;
 * SendAssignableSwitch(selectId, (int)msg->m_index, 0,
 * (bool)(msg->m_value != 0), false) for Sw -- confirmed via 2 independent
 * disassemblies each (Knob1Value+Knob2's own identical shape by symbol
 * size; Sw1Value/Sw2Value/Sw8Value cross-checked for the shift amount).
 * Knob-only: after NotifyAfterEdit(), a tail call to
 * `SKSTGGate_NotifyKarmaSliderPosition(0)` UNLESS
 * `CKGUIMsgProcessor::ms_poInstance`'s own `+0x6c` mode is exactly 1 (Sw
 * has no such tail call at all).
 *
 * === SetScene / SetLinkedSceneId (batch 2, 2026-07-28) ===
 * Both deviate from the Shape-B skeleton's usual ordering. Full
 * disassembly: SetScene @.text+0x3ceaf0 (0x2c1 bytes), SetLinkedSceneId
 * @.text+0x3ce0d0 (0x24d bytes).
 *
 * SetScene: primary write (m_liveRecord[0x127] = msg->m_value, dual-shadow
 * on a SysEx miss, matching CKGSeqBackupModuleParam::SetScene's own fixed
 * 0x127 offset) happens FIRST, unconditionally, same as every Shape-B
 * field. Then CKGEngine::ms_poInstance[0xb0] (suppressed) gates EVERYTHING
 * else -- if suppressed, returns immediately (no LinkedSceneId-array sync,
 * no Send, no Notify; every other Shape-B method still calls Send+Notify
 * unconditionally, gated only on suppression, but never skips a shadow/
 * array-sync step this way). If not suppressed: updates the SAME packed-
 * nibble array SetLinkedSceneId itself owns (offset 0x2e4, low/high nibble
 * by odd/even index, 3-bit `& 0x7` value) but keyed off a DIFFERENT index
 * than msg->m_index -- a "current scene" byte read via a double
 * indirection through CKGBankManager (`*(byte*)(*(byte**)
 * CKGBankManager::ms_poInstance + 0x135)`), written to m_liveRecord always
 * and to both shadow records too when the primary write's own SysEx
 * lookup missed. Then a genuinely NEW field at `this+0x14` (not touched by
 * any other method in this family) is tested: if non-NULL, returns without
 * ever calling Send/Notify at all -- semantics unconfirmed beyond "acts as
 * a Send-suppression guard", modelled as `void *m_pendingSceneSendGuard`.
 * If NULL: `SendScene(GetRTParmBufferSelectId(msg->m_deviceIndex),
 * (unsigned char)msg->m_value, false)` + `NotifyAfterEdit()`.
 *
 * SetLinkedSceneId: inverted relative to SetScene -- the packed-nibble
 * array write (0x2e4, keyed off msg->m_index this time, matching
 * CKGSeqBackupModuleParam::SetLinkedSceneId's own read-side formula
 * exactly: byte index `msg->m_index/2` -- real signed-division-by-2
 * idiom in ground truth, safe for msg->m_index's real range -- nibble
 * selected by `msg->m_index & 1`) is the UNCONDITIONAL primary write here
 * (to m_liveRecord always, to both shadow records when this method's own
 * SysEx lookup missed), done BEFORE the suppression check. The simple
 * `m_liveRecord[0x127]` field (SAME offset SetScene owns, presumably a
 * "last linked/current" mirror) is instead the one gated behind
 * CKGEngine::ms_poInstance[0xb0]==0, alongside
 * `SendLinkedSceneID((unsigned char)msg->m_deviceIndex, (unsigned char)
 * msg->m_index, (unsigned char)msg->m_value)` + NotifyAfterEdit() -- no
 * GetRTParmBufferSelectId() indirection for this one, deviceIndex passed
 * raw. No `this+0x14` guard here (that field is SetScene-only).
 *
 * `HandleMessage()`/ctor/dtor: real dispatcher/plumbing, same
 * "confirmed real, deliberately deferred" treatment as every other
 * *MsgHandler class in this project (see oa_control_msg_handler.h).
 */
void SKSTGGate_NotifyKarmaSliderPosition(int deviceIndex) __attribute__((regparm(3)));

struct CKGModuleParamMsgHandler {
	void *_vtablePtr;		/* +0x0, install-only, see comment above */
	unsigned char *m_liveRecord;	/* +0x4 */
	unsigned char *m_defaultRecordA;/* +0x8 */
	unsigned char *m_defaultRecordB;/* +0xc */
	int m_moduleIndex;		/* +0x10 */
	/* +0x14, SetScene-only real field -- non-NULL suppresses SendScene()/
	 * NotifyAfterEdit() entirely. Own purpose unconfirmed beyond that,
	 * see SetScene's own header comment above. */
	void *m_pendingSceneSendGuard;

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

	/* Shape C/D: RTParm-indirected Knob/Sw value group, see header comment */
	void SetKnob1Value(const CKGModuleParamMsg *msg);
	void SetKnob2Value(const CKGModuleParamMsg *msg);
	void SetKnob3Value(const CKGModuleParamMsg *msg);
	void SetKnob4Value(const CKGModuleParamMsg *msg);
	void SetKnob5Value(const CKGModuleParamMsg *msg);
	void SetKnob6Value(const CKGModuleParamMsg *msg);
	void SetKnob7Value(const CKGModuleParamMsg *msg);
	void SetKnob8Value(const CKGModuleParamMsg *msg);
	void SetSw1Value(const CKGModuleParamMsg *msg);
	void SetSw2Value(const CKGModuleParamMsg *msg);
	void SetSw3Value(const CKGModuleParamMsg *msg);
	void SetSw4Value(const CKGModuleParamMsg *msg);
	void SetSw5Value(const CKGModuleParamMsg *msg);
	void SetSw6Value(const CKGModuleParamMsg *msg);
	void SetSw7Value(const CKGModuleParamMsg *msg);
	void SetSw8Value(const CKGModuleParamMsg *msg);

	/* real idx-dependent packed-nibble/multi-branch outliers, see header
	 * comment */
	void SetScene(const CKGModuleParamMsg *msg);
	void SetLinkedSceneId(const CKGModuleParamMsg *msg);

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
/* eSTGMsgPerfType is declared in oa_engine_init.h (included above) --
 * both CKGBankManager::ChangeMode() and CKGEngine::ChangePerformance()
 * below need it, and oa_engine_init.h is the lower header in this
 * include chain. */

struct CKGParamEdit;
struct CKGEngine {
	static unsigned char *ms_poInstance;
	static CKGParamEdit *ms_poKGParamEdit;

	/*
	 * ResetLocalController() -- real instance method, discovered while
	 * reconstructing CKGGlobalParamMsgHandler::SetGlobalKeyTranspose()/
	 * SetMIDIChannel() (src/engine/ckg_global_param_handler.cpp), called
	 * through ms_poInstance as `this` (same cast-through-raw-pointer
	 * idiom as every other singleton here). Own body out of scope.
	 */
	void ResetLocalController();

	/*
	 * More real instance methods, discovered while reconstructing
	 * CKGControlMsgHandler (src/engine/ckg_control_msg_handler.cpp,
	 * oa_ckg_control_ui_msg.h) -- same cast-through-`ms_poInstance`
	 * idiom as ResetLocalController() above. `+0xb0` is the SAME
	 * "edit suppressed" byte the ParamMsgHandler family's own
	 * ShouldAttemptSysExShadowWrite() gates on; Start()/Stop() write
	 * it directly (0/1) rather than reading it.
	 */
	void WritePerformance();
	void DoCurrentDump();
	void DoCompare();
	void ChangePerformance(eSTGMsgPerfType type, bool arg2);
	void UpdateGEInfo(int a);	/* single-int overload -- distinct
					 * from CKGUIMsgSender's own 2-int
					 * UpdateGEInfo(int,int) */
	void SendShutUp();
	void UpdateUserGE(int a, int b);
	void DoInitModule(int arg);
	void DoAutoAssignRTName(int arg);
	void DoRandomCapture(long arg);
	void DoClearRTCSetup(long arg);
	void DoAutoRTCSetup(long arg);
	void OpenGECategoryPopup();
	void CloseGECategoryPopup(bool arg);
	void UpdateRTCDisplay(int arg);
	void UpdateRTCModelName(int arg);

	/*
	 * 4 more real instance methods, discovered while reconstructing
	 * the CKGController/CKGSwitch/CKGKnob/CKGPad diamond-inheritance
	 * widget hierarchy (oa_ckg_switch_family.h) -- same
	 * cast-through-`ms_poInstance` idiom as every other method above.
	 */
	int GetLocalControllerChannel();
	void ResetKRTCSwitch(int ccNumber);
	void ResetKRTCSlider(int id);
	int GetNumOfModule();

	/*
	 * 6 more real instance methods, discovered while reconstructing the
	 * CSKMIDIMsgHandler/CSKSpecialMsgHandler/CSKSysExMsgHandler MIDI
	 * dispatch family (oa_ckg_midi_msg_handler.h) -- same
	 * cast-through-`ms_poInstance` idiom as every other method above.
	 * `+0x14` (CSKSpecialMsgHandler::ProcessResetAllControllerMessage's
	 * own raw dword write, 2 or 3 depending on case) is a real field
	 * accessed directly as a raw offset rather than through a method,
	 * same convention as `+0xb0` above.
	 */
	bool ShouldKeepKarmaPerformance();
	void ClearScheduler();
	void KarmaTurnOnWhenFinishDump();
	void KarmaTurnOffWhenStartDump();
	void SendCCOffsetBack();
	void SetBendRange(int range, unsigned int arg2, unsigned int arg3);

	/*
	 * 2 more real instance methods, discovered reconstructing
	 * CSKMIDIInMsgHandler::Process()/SendChannelMessageToKarmaEngine()
	 * (oa_ckg_midi_msg_handler.h) -- same cast-through-`ms_poInstance`
	 * idiom. Own bodies out of scope.
	 */
	void SendChannelMessage(unsigned char statusType, unsigned char channel,
				 signed char data1, signed char data2);
	bool ShouldForceTimbreZoneBypass(int channel, int flagsChannel);

	/*
	 * A real, plain static pointer (own relocation
	 * `_ZN9CKGEngine26ms_poKGEventDisplayManagerE`, same class-scope-
	 * static idiom as `ms_poInstance` itself, NOT an offset field on
	 * the instance) -- discovered reconstructing CSKMIDIInMsgHandler::
	 * NotifyNoteEventToUI() (oa_ckg_midi_msg_handler.h), whose own
	 * CKGEventDisplayManager type is declared there (this header's own
	 * include chain sits below it, hence `unsigned char *` here rather
	 * than the real pointer type; cast at each use site).
	 */
	static unsigned char *ms_poKGEventDisplayManager;
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
 * literal 4 and the {8,9,10} range -- shadow write is attempted when mode
 * is OUTSIDE {8,9,10} and skipped when mode is inside it, see the bug-fix
 * comment on ShouldAttemptSysExShadowWrite()'s own body in the .cpp for how
 * this was confirmed) and `+0x74` (a real byte flag, set
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

	/*
	 * Two more real overloads/methods, discovered while reconstructing
	 * CKGCommonParamMsgHandler (src/engine/ckg_common_param_handler.cpp):
	 * SetTempo() calls a 2-arg NotifyAfterEdit(bool, int) overload
	 * instead of the 0-arg one every other Shape-B method uses (real
	 * mangled `Ebi`, confirmed distinct from `Ev`); SetScene()'s own
	 * 4-module linked-scene broadcast loop calls SendModuleSceneMessage
	 * (real mangled `Eii`) once per real KARMA module whose own
	 * `+0x2e4` byte has bit 0x8 set.
	 */
	void NotifyAfterEdit(bool immediate, int value);
	void SendModuleSceneMessage(int moduleIndex, int sceneId);

	/*
	 * 2 more real overloads, discovered while reconstructing the
	 * CKGController/CKGSwitch/CKGKnob/CKGPad diamond-inheritance
	 * widget hierarchy (oa_ckg_switch_family.h) -- every concrete
	 * switch/knob/scene leaf's own Process() forwards through one of
	 * these two (4-arg vs 5-arg, real mangled `Eiiib`/`Eiiiib`) to
	 * push its own current state to the front-panel UI. Own body out
	 * of scope.
	 */
	void ProcessRTControllersValue(int msgId, int arg2, int arg3, bool changed);
	void ProcessRTControllersValue(int msgId, int arg2, int arg3, int arg4, bool changed);
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
struct CSKParameterChangeMessage;
struct CSPRSysExBufManager {
	char GetValue(int a, int b, int c, int d, int e, int f, int g, long *out);

	/*
	 * SetValue(CSKParameterChangeMessage*) -- real 1-arg overload,
	 * discovered while reconstructing CSKSysExMsgHandler::
	 * Store{KG,SPR,STG}ParamChange() (oa_ckg_midi_msg_handler.h) -- all
	 * three call it identically (own body out of scope, generic SysEx
	 * record-buffer write infrastructure, sibling of GetValue() above).
	 */
	void SetValue(CSKParameterChangeMessage *msg);
};

#endif /* OA_CKG_MODULE_PARAM_MSG_HANDLER_H */
