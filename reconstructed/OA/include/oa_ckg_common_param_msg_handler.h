// SPDX-License-Identifier: GPL-2.0
#ifndef OA_CKG_COMMON_PARAM_MSG_HANDLER_H
#define OA_CKG_COMMON_PARAM_MSG_HANDLER_H

#include "oa_ckg_module_param_msg_handler.h"	/* shared singletons: CKGEngine,
						 * CKGParamEdit, CKGUIMsgProcessor,
						 * CSPRMIDIMsgProcessor,
						 * CSPRSysExBufManager */

/*
 * oa_ckg_common_param_msg_handler.h  -  CKGCommonParamMsgHandler: the
 * "checked write" sibling of CKGModuleParamMsgHandler for KARMA COMMON
 * (Combi/Song-level, not per-module) performance parameters. Confirmed
 * structurally identical dispatch skeleton via direct disassembly
 * (`.text+0x3c1e10`..`.text+0x3c730e`, 76 real methods incl. HandleMessage/
 * dtor -- 72 in scope for this batch, HandleMessage/dtor deferred same as
 * every other *MsgHandler class) -- see oa_ckg_module_param_msg_handler.h's
 * own header comment for the shared 3-step skeleton
 * (ShouldAttemptSysExShadowWrite/SysExShadowWriteIsNeeded gate,
 * single-vs-dual-shadow rule, Send+Notify pair). Field offsets cross-
 * referenced the same way against the already-reconstructed READ-side
 * sibling `CKGSeqBackupCommonParam` (src/engine/karma_seq_backup.cpp,
 * oa_karma_seq_backup.h).
 *
 * === CKGCommonParamMsg (2nd arg to every method below) -- NOTE: a
 * genuinely different layout from CKGModuleParamMsg, not just a renamed
 * copy ===
 *   +0x00 m_kind                  int  bank-type tag for GetKarmaPerfCommon:
 *                                      0=Combi, 1=Program, 2=Seq, anything
 *                                      else=NULL (a REAL explicit
 *                                      fallback -- unlike
 *                                      CKGModuleParamMsgHandler::
 *                                      GetKarmaModule(), which treats
 *                                      "anything else" as Combi too, this
 *                                      one's own disassembly has a genuine
 *                                      4th arm that returns 0).
 *   +0x04  m_bankId                int  raw eSTGCombiBankId/eSTGProgramBankId,
 *                                       Program/Combi cases only, same as
 *                                       the Module sibling.
 *   +0x08  m_karmaIndexOrSentinel  int  GetKarmaPerfCommonForSeqBackup()-only,
 *                                       same 0xffff-sentinel convention as
 *                                       the Module sibling.
 *   +0x0c  m_index            unsigned int  THE single per-call index field
 *                                      -- unlike CKGModuleParamMsg (which
 *                                      splits deviceIndex/+0xc from a
 *                                      separate ctx-index/+0x10), Common
 *                                      has just ONE index field doing
 *                                      double duty as both the ctx-indexed
 *                                      record-array index (when the target
 *                                      field is per-slot) AND every
 *                                      SendXxx() call's own "index"
 *                                      argument (RTParam number, DynMIDI
 *                                      slot, ChordMem note number, Knob/Sw
 *                                      number...) -- confirmed by direct
 *                                      disassembly of every Send call site
 *                                      reusing the identical `[msg+0xc]`
 *                                      read for both purposes. No separate
 *                                      "deviceIndex" concept exists here --
 *                                      makes sense, Common params are not
 *                                      per-KARMA-module.
 *   +0x10  reserved_0x10      int  never observed read by any of the 72
 *                                      methods in this batch.
 *   +0x14  m_value            int  the value being set -- NOTE the
 *                                      offset (0x14, not Module's 0x18):
 *                                      Common's message struct is 4 bytes
 *                                      narrower, confirmed via every
 *                                      SetXxx's own `[edx+0x14]`/
 *                                      `[ebx+0x14]` read.
 */
struct CKGCommonParamMsg {
	int m_kind;			/* +0x0 */
	int m_bankId;			/* +0x4 */
	int m_karmaIndexOrSentinel;	/* +0x8 */
	unsigned int m_index;		/* +0xc */
	int reserved_0x10;		/* +0x10, never observed read */
	int m_value;			/* +0x14 */
};

/*
 * === CKGCommonParamMsgHandler ===
 * Same base layout as CKGModuleParamMsgHandler (+0x0 vtable, +0x4
 * m_liveRecord, +0x8 m_defaultRecordA, +0xc m_defaultRecordB, +0x10 an
 * index-like field fed as SysExShadowWriteIsNeeded()'s own 6th GetValue()
 * argument) -- confirmed via identical disassembly shape at every call
 * site. No `+0x14` guard field exists here (that was
 * CKGModuleParamMsgHandler::SetScene-only); this class's own SetScene
 * outlier is structured completely differently, see its dedicated header
 * comment below.
 *
 * === Single- vs dual-shadow-write rule (same test as the Module sibling,
 * re-verified independently here) ===
 * A field gets a SECOND shadow write (m_defaultRecordB) iff its
 * CKGSeqBackupCommonParam read-side counterpart reads through `m_default`
 * (SwName/KnobName, the RTParam A/B/C/D/Min/Max/Value group, and the
 * Knob1-8Value/Sw1-8Value group) -- every field of this batch's own 66
 * mechanical methods that is `m_default`-based also happens to be
 * ctx-indexed, so this re-confirms rather than extends the Module
 * sibling's own (more narrowly stated) rule. `SetScene` is the one
 * fixed-offset `m_default`-based field in the whole class and it too gets
 * a real dual shadow write (see its own comment below) -- consistent with
 * "any `m_default`-based field gets dual shadow, ctx-indexing aside".
 *
 * === Send-call convention -- A GENUINE DEVIATION from the Module
 * sibling, confirmed via many independent disassembly spot-checks ===
 * Every field's own index argument (when the Send target takes one) is
 * `msg->m_index` directly -- there is no per-device "buffer select"
 * indirection of any kind (`GetRTParmBufferSelectId()` is never called
 * anywhere in this class, unlike the Module sibling's own Knob/SwValue
 * group). Arity determines the shape:
 *   1 arg  -> (value)                          [no index concept at all --
 *                                                matches every 1-arg
 *                                                field's own read-side
 *                                                being NON-ctx-indexed]
 *   2 args -> (index, value)
 *   3 args -> (index, GROUP_CONST, value)       -- GROUP_CONST is a real
 *             0-based ordinal confirmed by direct disassembly for both
 *             3-arg families in this class: SendDynModule's
 *             A/B/C/D/Last quintet (0/1/2/3/4) and SendRTPModule's A/B/C/D
 *             quartet (0/1/2/3); SendChordMemNote/SendChordMemVelocity's
 *             own Note1..8[Vel] octets (0..7) follow the identical
 *             pattern. Every constant below was confirmed via its own
 *             disassembly (`mov ecx,N` immediately before the call), not
 *             inferred from the name ordering alone.
 *   5 args -> SetKnob1-8Value/SetSw1-8Value ONLY, see below.
 *   0 args (SwName/KnobName) -> Send skipped entirely, NotifyAfterEdit()
 *             still called when unsuppressed (same convention as the
 *             Module sibling's own SetGE/SetSwName/SetKnobName).
 *
 * SetKnob1-8Value / SetSw1-8Value: `SendKnob(0, (int)msg->m_index, 0,
 * (int)msg->m_value, false)` / `SendAssignableSwitch(0, (int)msg->m_index,
 * 0, (bool)(msg->m_value != 0), false)` -- structurally the SAME 5-arg
 * shape as the Module sibling's own Shape C/D group, but arg1 is a fixed
 * constant 0 here (no RTParm buffer-select indirection at all) and arg2
 * is `msg->m_index` directly instead of a select-id lookup result. No
 * `SKSTGGate_NotifyKarmaSliderPosition()` tail call for either group here
 * (that was Module's Knob-only addition, absent in every one of this
 * class's own 16 Knob/SwValue disassemblies).
 *
 * === SetTempo (non-const `CKGCommonParamMsg*` -- the ONE method in this
 * class whose own real signature omits `const`) ===
 * A genuine standalone outlier, not a shape of its own shared by any
 * other method. When edit is suppressed (CKGEngine::ms_poInstance[0xb0]
 * != 0): a plain unconditional `m_liveRecord[0..1] = (u16)msg->m_value`,
 * nothing else -- no CSPREngine gate, no Send, no Notify at all (the
 * ONLY method in this whole class that skips the SysEx-shadow gate
 * entirely on account of suppression rather than the gate's own
 * condition). When not suppressed: reads the CURRENT live tempo first
 * (before any write), calls the real free function
 * `KGOutGate_CheckAndSetTempoForOtherModule(msg->m_value)` -- if it
 * returns true, calls `SendTempo()` and then checks
 * `CTimerManager::ms_poInstance->ShouldSyncExternalClock()`; if that ALSO
 * returns true, the real record write is skipped entirely and control
 * falls through to the same "suppressed-notify" tail as the false-path
 * below. If ShouldSyncExternalClock() is false: writes the new tempo to
 * `m_liveRecord` and, unconditionally when `m_defaultRecordA` is non-NULL
 * (no SysEx-miss gate at all here, unlike every Shape-B field), to
 * `m_defaultRecordA` too (single shadow only, matching
 * CKGSeqBackupCommonParam::SetTempo's own `m_source`-based, non-`m_default`
 * read), then calls `NotifyAfterEdit(true, oldTempo)`. If
 * CheckAndSetTempoForOtherModule() itself returned false in the first
 * place: `msg->m_value` is OVERWRITTEN with the OLD tempo (a real
 * caller-visible side effect -- the reason the parameter is non-const)
 * and `NotifyAfterEdit(false, oldTempo)` is called, with NO record write
 * of any kind. `NotifyAfterEdit(bool, int)` is a real 2-arg overload,
 * confirmed distinct from every other method's own 0-arg
 * `NotifyAfterEdit()` call (different mangled name, `Ebi` vs `Ev`).
 *
 * === SetScene ===
 * A second genuine standalone outlier, structured completely differently
 * from CKGModuleParamMsgHandler::SetScene (no `+0x14` guard field exists
 * on this class at all). Primary write: `m_liveRecord[0x135]`'s own low 3
 * bits = `msg->m_value & 7` (matches CKGSeqBackupCommonParam::SetScene's
 * own `m_default`-based read at the identical offset+mask) -- gated
 * shadow write is a real DUAL write (both m_defaultRecordA[0x135] and
 * m_defaultRecordB[0x135]) despite this field being fixed-offset, not
 * ctx-indexed -- confirmed directly, not inferred from the general rule
 * (see the dual-shadow comment above). If
 * `CKGEngine::ms_poInstance[0xb0]` is non-zero (suppressed): returns
 * immediately, skipping Send/Notify AND the entire 4-module broadcast
 * loop below. If not suppressed: `SendScene(0, (unsigned char)msg->
 * m_value, false)` + `NotifyAfterEdit()`, THEN unconditionally (no
 * further suppression re-check) broadcasts the linked-scene id to up to 4
 * real KARMA modules via `CKGUIMsgProcessor::ms_poInstance->
 * SendModuleSceneMessage(moduleIndex, sceneId)`. Each module's own
 * 0x2e8-byte record (base = `*(unsigned char **)(CKGBankManager::
 * ms_poInstance + 0x4)`, module N at `base + N*0x2e8`) is skipped unless
 * its own `+0x2e4` byte has bit 0x8 set; when set, the scene id is read
 * from a packed-nibble table at the SAME `+0x2e4` base -- byte index
 * `(unsigned char)msg->m_value >> 1`, low nibble (masked to 3 bits) if
 * `msg->m_value & 1` is 0, high nibble (shifted right 4, then masked to 3
 * bits) if 1 -- the IDENTICAL packed-nibble idiom
 * CKGModuleParamMsgHandler::SetLinkedSceneId's own array uses (see that
 * class's own header comment), just consumed here instead of written.
 * `SendModuleSceneMessage()`'s own module-index argument is the real
 * 0-based loop counter (0..3), not derived from any per-module id field.
 *
 * `HandleMessage()`/dtor: real dispatcher/plumbing, same "confirmed real,
 * deliberately deferred" treatment as every other *MsgHandler class in
 * this project.
 */
/*
 * KGOutGate_CheckAndSetTempoForOtherModule() -- real free-function
 * dependency of SetTempo()'s own outlier logic (see its header comment
 * above), own body out of scope for this batch. CTimerManager itself is
 * already declared in oa_engine_init.h (see sk_stg_gate.cpp for its own
 * ShouldSyncExternalClock() consumer) -- reused here, not redeclared.
 */
bool KGOutGate_CheckAndSetTempoForOtherModule(int newTempo) __attribute__((regparm(3)));

struct CKGCommonParamMsgHandler {
	void *_vtablePtr;		/* +0x0, install-only, see the Module
					 * sibling's own comment on this field */
	unsigned char *m_liveRecord;	/* +0x4 */
	unsigned char *m_defaultRecordA;/* +0x8 */
	unsigned char *m_defaultRecordB;/* +0xc */
	int m_moduleIndex;		/* +0x10 */

#include "oa_ckg_common_param_msg_handler_decls.inc"

	void *GetKarmaPerfCommon(const CKGCommonParamMsg *msg);
	void *GetKarmaPerfCommonForSeqBackup(const CKGCommonParamMsg *msg);
	bool ShouldStoreToBackup(const CKGCommonParamMsg *msg);
	void SetTempo(CKGCommonParamMsg *msg);		/* real non-const, see comment above */
	void SetScene(const CKGCommonParamMsg *msg);
	void SetChordMemVelocity(const CKGCommonParamMsg *msg);	/* real no-op, empty body -- see .cpp */

	/* Shared skeleton helpers -- same "factored out of the byte-identical
	 * repeated blocks" convention as the Module sibling, not real
	 * ground-truth functions of their own. */
	bool ShouldAttemptSysExShadowWrite() const;
	bool SysExShadowWriteIsNeeded(const CKGCommonParamMsg *msg) const;
};

#endif /* OA_CKG_COMMON_PARAM_MSG_HANDLER_H */
