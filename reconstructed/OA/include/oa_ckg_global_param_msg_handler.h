// SPDX-License-Identifier: GPL-2.0
#ifndef OA_CKG_GLOBAL_PARAM_MSG_HANDLER_H
#define OA_CKG_GLOBAL_PARAM_MSG_HANDLER_H

#include "oa_ckg_module_param_msg_handler.h"	/* CKGEngine::ms_poKGParamEdit, CKGParamEdit */

/*
 * oa_ckg_global_param_msg_handler.h  -  CKGGlobalParamMsgHandler
 *
 * A THIRD, structurally distinct sibling in the "CKG*ParamMsgHandler"
 * family alongside CKGModuleParamMsgHandler and CKGCommonParamMsgHandler
 * (`.text+0x3c7660`..`.text+0x3c79ba`, 27 real `Set*` methods + ctor +
 * HandleMessage, confirmed via direct disassembly of every method). NOT
 * the same convention as either sibling: no CSPREngine SysEx-shadow gate,
 * no m_liveRecord/m_defaultRecordA/m_defaultRecordB triple, no KARMA-perf
 * record lookup of any kind (no GetKarmaPerfGlobal/ShouldStoreToBackup
 * equivalent exists in this class at all). Global performance parameters
 * are session-wide settings (MIDI channel, velocity curve, external pad
 * config...), not per-KARMA-module/per-Combi records, so the whole
 * checked-write/shadow-write machinery the other two siblings share
 * simply does not apply here.
 *
 * === CKGGlobalParamMsg (2nd arg to every method below) ===
 *   +0x00  m_index  int  per-call array index, only read by the methods
 *                        whose own field is itself an array (Controller/
 *                        PadCCNo, MIDIFilter's bit-index, ExternalPad
 *                        Channel/Assign/Velocity). Every non-array field
 *                        method never reads this offset at all.
 *   +0x04  m_value  int  the value being set, read at whatever width/
 *                        signedness the destination field needs -- same
 *                        "real truncating register-width choice, not a
 *                        modelling simplification" convention as the
 *                        other two siblings.
 * This is genuinely the whole struct -- no m_kind/m_bankId/
 * m_karmaIndexOrSentinel fields exist here (nothing in this class ever
 * reads past +0x4).
 *
 * === CKGGlobalParamMsgHandler ===
 *   +0x00  _vtablePtr  void*  install-only, see the Module sibling's own
 *                             comment on this field.
 *   +0x04  m_globalData  unsigned char*  the real global-settings data
 *                             chunk every method below writes into
 *                             directly at a fixed byte offset (or a
 *                             small per-call-indexed array within it) --
 *                             own layout/owner out of scope, byte offsets
 *                             below are ground truth, not modelled.
 * Real ctor: `_vtablePtr = &_ZTV24CKGGlobalParamMsgHandler[8]` (matches
 * every other *MsgHandler class's own install-only convention) -- 7
 * bytes, deliberately deferred, same "confirmed real, out of scope"
 * treatment as HandleMessage()/dtor.
 *
 * === Per-method shapes -- NO shared skeleton, each is its own small
 * standalone body (confirmed via direct disassembly of all 28) ===
 * The overwhelming majority are one of 3 trivial shapes with no gate, no
 * shadow write, no universal Notify call of any kind:
 *   (a) plain field write only, nothing else -- SetGlobalVelocityCurve,
 *       SetAfterTouchCurve, SetConvertPosition, SetControllerCCNo,
 *       SetPadCCNo, SetExternalPadChannel, SetExternalPadAssign,
 *       SetExternalPadVelocity.
 *   (b) plain field write, then a CKGParamEdit::SendXxx()/SetXxx() call
 *       and/or a real free-function side effect, no suppression check of
 *       any kind (unlike the other two siblings' universal
 *       CKGEngine::ms_poInstance[0xb0] gate -- this class never reads
 *       that byte at all) -- SetGlobalKeyTranspose, SetMIDIChannel,
 *       SetForceKarmaOff, SetForceDTOff, SetProgDrumTrackMIDIOutputChannel,
 *       SetProgDrumTrackEnableMIDIOutput.
 *   (c) `setne`-style plain boolean field write only -- SetRecieveExt
 *       Commands, SetVectorMIDIOut, SetPadsMIDIOut, SetAutoKarmaProg,
 *       SetAutoKarmaCombi, SetEnableKarmaModuleToMIDIOut,
 *       SetSendStartStopInProgCombi.
 * Real outliers, each traced individually:
 *   - SetLocalOn / SetNoteReceive: confirmed real no-ops, entire body is
 *     a bare `ret` (1 byte each, `.text+0x3c7750`/`.text+0x3c7760`).
 *   - SetLocalControllerMIDICh: NO field write of any kind -- just
 *     `SendGlobalMIDICh((signed char)msg->m_value)` +
 *     `SPRMain_RenewMIDIChannel()`, same 2 calls SetMIDIChannel makes,
 *     minus that method's own field write.
 *   - SetMIDIFilter: a real per-bit set/clear on a single byte field
 *     keyed by `msg->m_index` (0-7) -- `globalData[4] |= 1<<index` when
 *     `msg->m_value` is nonzero, `globalData[4] &= ~(1<<index)`
 *     otherwise. Ground truth implements the clear side via a `rol`-of-
 *     0xfffffffe idiom (a compiler-generated `~(1<<n)` equivalent for
 *     n<8, not a distinct behavior) -- rendered here as plain `&~(1<<n)`,
 *     semantically identical.
 *   - SetMIDIClockSource: the one method with a real conditional gate --
 *     when `msg->m_value` (the clock-source enum) is nonzero, checks
 *     `SKSTGGate_IsMidiPortAvailable(CKarmaGlobal::
 *     GetMIDIClockPortForSource((ESyncClockSource)msg->m_value))`; if
 *     that reports unavailable, the ENTIRE method is a no-op (no field
 *     write, no Send). Otherwise (source is 0, OR the port is
 *     available): `globalData[0xc..0xf]` (dword) = msg->m_value, then
 *     `SendMIDIClockSource((bool)(msg->m_value != 0))`.
 *   - SetEnableMIDIInToKarmaModule: NO field write of any kind -- just
 *     `CKGParamEdit::SetEnableMIDIInToKarmaModule((bool)(msg->m_value !=
 *     0))` (confirmed real target name, NOT prefixed "Send" like every
 *     other CKGParamEdit call in this whole 3-class family -- verified
 *     directly via the real mangled symbol, not a naming inconsistency
 *     introduced here).
 *
 * `HandleMessage()`/ctor: real dispatcher/plumbing, same "confirmed real,
 * deliberately deferred" treatment as every other *MsgHandler class in
 * this project.
 */
struct CKGGlobalParamMsg {
	int m_index;	/* +0x0 */
	int m_value;	/* +0x4 */
};

/*
 * CKarmaGlobal::GetMIDIClockPortForSource() / SKSTGGate_IsMidiPortAvailable()
 * -- real dependencies of SetMIDIClockSource()'s own outlier gate, own
 * bodies out of scope. GetMIDIClockPortForSource's own real disassembly
 * shows no separate `this` pointer setup (single-register-in, single-
 * register-out) -- modelled as a static method, same convention used
 * elsewhere in this project when a member function's own call site never
 * loads a distinct `this`.
 */
enum ESyncClockSource { eSyncClockSource_Placeholder = 0 };
enum eSTGMidiPort { eSTGMidiPort_Placeholder = 0 };

struct CKarmaGlobal {
	static eSTGMidiPort GetMIDIClockPortForSource(ESyncClockSource source);
};
bool SKSTGGate_IsMidiPortAvailable(eSTGMidiPort port) __attribute__((regparm(3)));
void SPRMain_RenewMIDIChannel(void) __attribute__((regparm(3)));
void SPRMain_SetAllKARMAAndDrumTrack(bool) __attribute__((regparm(3)));
void SPRMain_ProcessProgDrumTrackMIDIOutputChannel(int) __attribute__((regparm(3)));
void SPRMain_ProcessEnableProgDrumTrackMIDIOutput(bool) __attribute__((regparm(3)));

struct CKGGlobalParamMsgHandler {
	void *_vtablePtr;			/* +0x0, install-only */
	unsigned char *m_globalData;		/* +0x4 */

	void SetGlobalKeyTranspose(CKGGlobalParamMsg *msg);
	void SetGlobalVelocityCurve(CKGGlobalParamMsg *msg);
	void SetAfterTouchCurve(CKGGlobalParamMsg *msg);
	void SetMIDIChannel(CKGGlobalParamMsg *msg);
	void SetLocalControllerMIDICh(CKGGlobalParamMsg *msg);
	void SetLocalOn(CKGGlobalParamMsg *msg);
	void SetNoteReceive(CKGGlobalParamMsg *msg);
	void SetConvertPosition(CKGGlobalParamMsg *msg);
	void SetMIDIClockSource(CKGGlobalParamMsg *msg);
	void SetMIDIFilter(CKGGlobalParamMsg *msg);
	void SetControllerCCNo(CKGGlobalParamMsg *msg);
	void SetPadCCNo(CKGGlobalParamMsg *msg);
	void SetRecieveExtCommands(CKGGlobalParamMsg *msg);
	void SetVectorMIDIOut(CKGGlobalParamMsg *msg);
	void SetPadsMIDIOut(CKGGlobalParamMsg *msg);
	void SetForceKarmaOff(CKGGlobalParamMsg *msg);
	void SetForceDTOff(CKGGlobalParamMsg *msg);
	void SetAutoKarmaProg(CKGGlobalParamMsg *msg);
	void SetAutoKarmaCombi(CKGGlobalParamMsg *msg);
	void SetEnableKarmaModuleToMIDIOut(CKGGlobalParamMsg *msg);
	void SetEnableMIDIInToKarmaModule(CKGGlobalParamMsg *msg);
	void SetSendStartStopInProgCombi(CKGGlobalParamMsg *msg);
	void SetExternalPadChannel(CKGGlobalParamMsg *msg);
	void SetExternalPadAssign(CKGGlobalParamMsg *msg);
	void SetExternalPadVelocity(CKGGlobalParamMsg *msg);
	void SetProgDrumTrackMIDIOutputChannel(CKGGlobalParamMsg *msg);
	void SetProgDrumTrackEnableMIDIOutput(CKGGlobalParamMsg *msg);
};

#endif /* OA_CKG_GLOBAL_PARAM_MSG_HANDLER_H */
