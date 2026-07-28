// SPDX-License-Identifier: GPL-2.0
#ifndef OA_CKG_MIDI_MSG_HANDLER_H
#define OA_CKG_MIDI_MSG_HANDLER_H

#include "oa_ckg_control_ui_msg.h"	/* CKGEngine, CKGBankManager, CSPREngine,
					 * CKGRTCHandler, CKGMIDIMsgProcessor,
					 * CSKMIDIMsgProcessor, CMIDIFlowParamHolder --
					 * all already visible by the time this header
					 * is reached (see that header's own include
					 * comment right above where it pulls this
					 * file in). When included from anywhere else,
					 * this line pulls them in normally. */

/*
 * oa_ckg_midi_msg_handler.h  -  CSKMIDIMsgHandler / CSKSpecialMsgHandler /
 * CSKSysExMsgHandler, a real-time MIDI-in dispatch family found while
 * sweeping the ~64-class CKG-/CSK-prefixed KARMA cluster for the next tractable
 * slice after the CKGController/CKGSwitch/CKGKnob/CKGPad widget hierarchy
 * (2026-07-28). All 3 classes' vtables and inheritance confirmed via real
 * `objdump -r` relocation dumps against `.rodata._ZTV*` sections in
 * /home/share/Decomp/OA.ko_Decomp/OA.ko (an unstripped, unlinked ground-
 * truth object matching the Ghidra static export's own addressing, offset
 * by a confirmed constant -0x10000), not trusted from any prior summary.
 *
 * === Class graph ===
 * `CSKMIDIMsgHandler` is a real, single-inheritance ONLY (no virtual
 * bases, no VTT) abstract base: 15 real virtual methods, no virtual
 * destructor of its own (confirmed -- no dtor slot anywhere in its own
 * vtable region, no dtor symbol at all in `nm`). `CSKSysExMsgHandler`
 * derives from it directly (adds its own vtable extension at rodata
 * offset 0x44+, no dtor either) -- a plain, non-virtual public base.
 * `CSKSpecialMsgHandler` is a completely SEPARATE, unrelated root class
 * (own 5-slot vtable, no shared slots with CSKMIDIMsgHandler at all) that
 * happens to reuse the exact same "raw 4-byte MIDI event at offset +4"
 * field convention purely by coincidence of both classes processing MIDI
 * bytes, not by inheritance.
 *
 * `CSKMIDIInMsgHandler` (the OTHER real direct child of CSKMIDIMsgHandler,
 * confirmed via its own vtable sharing the same 0x08-0x40 prefix) is
 * DELIBERATELY NOT reconstructed this batch -- deeper (33 methods, real
 * dying-note-tracking array arithmetic, a 922-byte ctor) and is this
 * project's own next continuation target; the existing 1-field opaque
 * stand-in for it (oa_ckg_control_ui_msg.h, `ms_bShouldStopSendingNoteOnsToSTG`)
 * is left untouched so this batch's blast radius stays limited to the 3
 * classes below.
 *
 * === The "this IS in EAX, not ECX" gotcha ===
 * Ghidra labels every real method here `__thiscall`, which it uses
 * generically for "some register holds `this`" -- NOT the MSVC-specific
 * ECX convention. This whole project's actual ABI is GCC `regparm(3)`
 * (this project's own established convention, see CLAUDE.md/other
 * headers): for a non-static member function, `this` is the FIRST
 * regparm register, i.e. EAX, with any real explicit parameters in
 * EDX/ECX after it. Ghidra's decompiled C consistently shows `this` as an
 * unused declared parameter and the REAL body reading an `in_EAX`
 * pseudo-variable instead -- every method below was re-typed accordingly
 * (real explicit params, if any, in EDX/ECX; `this` used directly).
 *
 * === The "*this + N" virtual-call-offset gotcha ===
 * Every indirect call `(**(code**)(*this + N))()` seen in the raw
 * disassembly targets absolute `.rodata._ZTVxxx` offset `N + 8` (NOT `N`)
 * -- because the object's own vptr (`*this`) points directly at vtable
 * SLOT 0 itself (Itanium ABI: vptr = &vtable_array[2], skipping the
 * offset-to-top/typeinfo header at rodata offsets 0/4), and slot 0 sits
 * at rodata offset 0x08. Verified against CSKSpecialMsgHandler::
 * AnalizeAndProcess()'s own 3 calls (`*this+8`/`+0xc`/`+4` -> rodata
 * 0x10/0x14/0x0c -> ProcessProgramChangeMessage/ProcessPitchBendMessage/
 * ProcessResetAllControllerMessage, each matching real MIDI status-byte
 * semantics: 0xc0=ProgramChange, 0xe0=PitchBend, CC121=ResetAllControllers)
 * before being trusted for the rest of this file's more complex bodies.
 * All virtual calls below are written as plain C++ virtual-method calls
 * (relying on this project's own vtable layout matching the real one
 * method-for-method), not manual vtable pokes -- this offset math was
 * used only to VERIFY which override + is not itself present in the
 * reconstructed source.
 *
 * === CKGBankManager::ms_poInstance giant-offset gotcha (reconfirmed) ===
 * Ghidra's decompiled C for several methods here shows an invented
 * `s_aiFlags[CKGBankManager::ms_poInstance + N]` array-index expression.
 * This is a pure decompiler-display artifact (`s_aiFlags` is NOT a real
 * ELF symbol -- confirmed absent from `readelf -s`, a Ghidra-invented
 * label for the address `0x9652e0`). The real, disassembly-verified
 * semantics is a single raw-offset byte/int access through the SAME
 * giant opaque aggregate `CKGBankManager::ms_poInstance` already declared
 * (oa_engine_init.h) and used elsewhere in this project (e.g.
 * `ms_poInstance[0x97c749]` in oa_ckg_switch_family.h): real absolute
 * offset = `0x9652e0 + N`. Every occurrence below was recomputed this way
 * and cross-checked against 2 already-committed offsets in this project
 * (0x97c747, 0x97c749) that happen to fall in the same field cluster.
 *
 * Files: src/engine/ckg_midi_msg_handler.cpp,
 * verify/test_ckg_midi_msg_handler.cpp.
 */

/*
 * KGMidiEvent -- small POD event record MakeKGMidiEvent() builds: 4 raw
 * MIDI bytes (a verbatim copy of the object's own +0x4 event) plus one
 * trailing "kind" byte (0xfe/0xff sentinels seen, or a small lookup-table
 * index -- CSKMIDIMsgProcessor::ms_poInstance[0x20], real meaning not
 * further confirmed). Own consumer(s) out of scope.
 */
struct KGMidiEvent {
	unsigned char raw[4];	/* +0x00 */
	unsigned char kind;	/* +0x04 */
};

/*
 * CDrumTrackBankManager -- drum-track performance-bank singleton,
 * discovered via CSKSpecialMsgHandler::ProcessProgramChangeMessage()'s
 * own "this IS the singleton" call shape (same idiom as CKGBankManager/
 * CSPREngine elsewhere in this project). Own class layout out of scope.
 */
struct CDrumTrackBankManager {
	static unsigned char *ms_poInstance;
	void ChangeProgram(eSTGProgramBankId bankId, unsigned int index);
	void ChangeCombi(eSTGCombiBankId bankId, unsigned int index);
	void ChangeSeq(unsigned int index);
};

/*
 * CSPRClockHandler -- sequencer transport-position singleton, discovered
 * via CSKSysExMsgHandler::ShouldRecThisParameterChange()'s own "this IS
 * the singleton" call shape. `ms_oStatusMaster` is a SEPARATE plain
 * static byte (own relocation, not offset through ms_poInstance) tested
 * for bit 0x40 ("precounting" gate) to pick which of the two location
 * getters to call. Own class layout otherwise out of scope; the 4
 * by-reference int outputs are opaque bar/beat/tick-shaped location
 * fields, real names unconfirmed.
 */
struct CSPRClockHandler {
	static unsigned char *ms_poInstance;
	static unsigned char ms_oStatusMaster;
	void GetCurrentLocation(int *a, int *b, int *c, int *d);
	void GetPrecountLocation(int *a, int *b, int *c, int *d);
};

/*
 * CSKParameterChangeMessage -- a "reinterpret this+offset as a different
 * class" view: CSKSysExMsgHandler's own 0x20-byte SysEx scratch buffer
 * (at this+0x10) is passed AS `this` to these methods directly (confirmed
 * via disassembly -- `lea 0x10(%eax),%ebx; mov %ebx,%eax; call
 * CSKParameterChangeMessage::IsThisParamChage()`), not a separately
 * allocated object. Own class layout otherwise out of scope (10 pending
 * methods of its own per the manifest, a separate future candidate).
 */
struct CSKParameterChangeMessage {
	bool IsThisParamChage();
	unsigned int GetValue();
};

/* Free functions this family calls through. Real mangled names confirmed
 * via each call site's own R_386_PC32 relocation; regparm(3) like every
 * other free function declared elsewhere in this tree. */
extern "C" void SKSTGGate_SendToSTG(const unsigned char *bytes, unsigned short len) __attribute__((regparm(3)));
/* SKSTGGate_SendToMIDIPort() already declared identically in
 * oa_ckg_switch_family.h -- redeclared here (not included from there, see
 * this header's own top comment) since both headers' free-function extern
 * "C" declarations are compatible redeclarations, not a redefinition. */
extern "C" void SKSTGGate_SendToMIDIPort(const unsigned char *bytes, unsigned short len) __attribute__((regparm(3)));
extern "C" bool SKSTGGate_CheckVJSCCToMIDIPortFilter(int ccNumber, int channel) __attribute__((regparm(3)));
extern "C" void SPRMain_RecChannelMessage(int status, int arg2, int arg3, int channel) __attribute__((regparm(3)));
extern "C" unsigned char CAfterTouchConverter_ConvertPostMIDI(int curve, unsigned char value)
	asm("_ZN18CAfterTouchConverter14ConvertPostMIDIEih") __attribute__((regparm(3)));
extern "C" unsigned char CAfterTouchConverter_ConvertPreMIDI(int curve, unsigned char value)
	asm("_ZN18CAfterTouchConverter13ConvertPreMIDIEih") __attribute__((regparm(3)));
extern "C" unsigned char CVelocityConverter_ConvertPostMIDI(int curve, unsigned char value)
	asm("_ZN17CVelocityConverter14ConvertPostMIDIEih") __attribute__((regparm(3)));
extern "C" bool SPROutGate_IsEnableExclusive(void) __attribute__((regparm(3)));
extern "C" char SPROutGate_GetAutomationSysExEventKind(unsigned int channel, int *out) __attribute__((regparm(3)));
extern "C" void KGMain_ReceiveParameterChangeMessageFromMIDIPort(unsigned char *buf) __attribute__((regparm(3)));
extern "C" void KGMain_ReceiveParameterChangeMessageFromSeqEvent(unsigned char *buf) __attribute__((regparm(3)));
extern "C" void KGMain_ReceiveKarmaDisableInputMessage(unsigned char *buf, int len) __attribute__((regparm(3)));
extern "C" void SPRMain_ReceiveParameterChangeMessageFromMIDIPort(unsigned char *buf) __attribute__((regparm(3)));
extern "C" void SPRMain_ReceiveParameterChangeMessageFromSeqEvent(unsigned char *buf) __attribute__((regparm(3)));
extern "C" void SPRMain_ReceiveDrumTrackParameterChangeMessageFromMIDIPort(unsigned char *buf) __attribute__((regparm(3)));
extern "C" void SPRMain_ReceiveDrumTrackParameterChangeMessageFromSeqEvent(unsigned char *buf) __attribute__((regparm(3)));
extern "C" void SPRMain_RecInternalSysExMessage(unsigned char byte) __attribute__((regparm(3)));
extern "C" void SPRMain_RecSysExMessageOnAutomationTrack(int a, int b, int c) __attribute__((regparm(3)));
extern "C" void SPRMain_RecSysExMessageFromMIDIPort(unsigned char byte) __attribute__((regparm(3)));

/*
 * === CSKMIDIMsgHandler ===
 * Class region `.text+0x332e80`..`.text+0x343360` (ctor). Abstract base:
 * no data of its own beyond the shared 4-byte raw-event field every
 * method below reads/writes (`m_status`/`m_data1`/`m_data2`/`m_flags`,
 * offsets +0x4..+0x7 -- `m_flags`'s low nibble is the MIDI channel,
 * confirmed by every `& 0xf` mask below).
 */
class CSKMIDIMsgHandler {
public:
	unsigned char m_status;	/* +0x4 */
	unsigned char m_data1;		/* +0x5 */
	unsigned char m_data2;		/* +0x6 */
	unsigned char m_flags;		/* +0x7, low nibble = channel */

	CSKMIDIMsgHandler();

	/* .text+0x343360, 7 bytes -- vtable install only, no other body. */

	/* .text+0x3431f0, 49 bytes. */
	virtual void SendChannelMessageToSTG();
	/* .text+0x3431b0, 56 bytes. */
	virtual void SendChannelMessageToSTGWithCorrectLength();
	/* .text+0x343140, 97 bytes. */
	virtual void SendChannelMessageToMIDIPortWithCorrectLength();
	/* .text+0x343300, 79 bytes. */
	virtual void ConvertPostMIDIVelocity();
	/* .text+0x343290, 102 bytes. */
	virtual void ConvertPreMIDIAfterTouch();
	/* .text+0x343230, 88 bytes. */
	virtual void ConvertPostMIDIAfterTouch();
	/* .text+0x342e80, 68 bytes. */
	virtual void ConvertPostMIDINote();
	/* .text+0x342ed0, 35 bytes. */
	virtual void ConvertNoteOnVelocity0IntoNoteOff();
	/* .text+0x3430a0, 145 bytes. */
	virtual unsigned int CheckGlobalFilter();
	/* .text+0x342f50, 56 bytes -- CSWTCH_42 (5-entry .rodata table
	 * indexed by CC# - 0x11, real values {0x40,0,0x40,0x40,0x40}, read
	 * directly from ground truth). */
	virtual int CheckAndGetCorrectCCValue();
	/* .text+0x343000, 150 bytes. */
	virtual void RecChannelMessageToSequencer();
	/* .text+0x342f90, 36 bytes. */
	virtual unsigned char CheckPadsMIDIOutFilter();
	/* .text+0x342fe0, 23 bytes. */
	virtual void StoreDyingNoteInfoForSTG();
	/* .text+0x342fc0, 23 bytes. */
	virtual void StoreDyingNoteInfoForMIDPort();
	/* .text+0x342f00, 62 bytes. */
	virtual void MakeKGMidiEvent(KGMidiEvent &ev);
};

/*
 * === CSKSpecialMsgHandler ===
 * Class region `.text+0x345d40`..`.text+0x3461a0` (ctor). A separate root
 * (NOT derived from CSKMIDIMsgHandler), own 4-byte raw-event field at the
 * same +0x4..+0x7 offsets purely by convention, not inheritance. Handles
 * the small set of "special" (non-channel-voice) MIDI status bytes: CC
 * Reset-All-Controllers (0x79), Program Change, Pitch Bend.
 */
class CSKSpecialMsgHandler {
public:
	unsigned char m_status;	/* +0x4 */
	unsigned char m_data1;		/* +0x5 */
	unsigned char m_data2;		/* +0x6 */
	unsigned char m_flags;		/* +0x7 */

	/* SetSendingBulkDump()'s own gate flag (oa_ckg_control_ui_msg.h,
	 * already declared before this batch touched the class). */
	static bool m_NowHandlingSamplingPerformanceChange;

	/* ProcessProgramChangeMessage()'s own re-entrancy gate, set for the
	 * whole duration of a program/combi/seq performance-change cascade. */
	static bool m_NowHandlingPerformanceChangeBySTG;

	CSKSpecialMsgHandler();

	/* .text+0x345d40, 132 bytes. Copies the 4 raw event bytes from
	 * `msg` into this object, then dispatches by status nibble: 0xc0 ->
	 * ProcessProgramChangeMessage(), 0xe0 -> ProcessPitchBendMessage(),
	 * (0xb0, CC121) -> ProcessResetAllControllerMessage(). */
	virtual unsigned int AnalizeAndProcess(unsigned char *msg);
	/* .text+0x346010, 351 bytes -- ~40-case switch on the CC value
	 * (m_data2), only 6 cases real (3,4,5,6,0xd,0xe,0x1c), everything
	 * else falls through returning 0. */
	virtual unsigned int ProcessResetAllControllerMessage();
	/* .text+0x345e50, 419 bytes. */
	virtual unsigned int ProcessProgramChangeMessage();
	/* .text+0x345de0, 107 bytes. */
	virtual bool ProcessPitchBendMessage();
	/* .text+0x345dd0, 10 bytes. */
	virtual void MakeKGMidiEvent(KGMidiEvent &ev);
};

/*
 * === CSKSysExMsgHandler ===
 * Class region `.text+0x3461b0`..`.text+0x346b70`. Real, plain (non-
 * virtual) public base `CSKMIDIMsgHandler`, own vtable extension at
 * rodata offset 0x44+ (14 new slots), no virtual destructor of its own
 * either (confirmed same as the base). Own fields, all confirmed via
 * disassembly:
 *   +0xc   int             m_bufIndex   -- running write index into
 *                                          m_buf, wraps at 0x20 (CopyToBuffer)
 *   +0x10  unsigned char[0x20] m_buf    -- raw SysEx byte scratch buffer,
 *                                          also reinterpreted directly as
 *                                          a CSKParameterChangeMessage*
 *                                          by several methods (see that
 *                                          struct's own comment above)
 *   +0x30  bool            m_inSysEx    -- set on SOX (0xf0), cleared on
 *                                          EOX (0xf7)
 *   +0x34  int             m_lastBar    -- ShouldRecThisParameterChange()'s
 *   +0x38  int             m_lastBeat      own duplicate-suppression state
 * `sm_bNowHandlingProgramChange` is a real static (class-scope, not
 * per-instance) bool.
 */
class CSKSysExMsgHandler : public CSKMIDIMsgHandler {
public:
	int m_bufIndex;			/* +0xc */
	unsigned char m_buf[0x20];	/* +0x10 */
	bool m_inSysEx;			/* +0x30 */
	int m_lastBar;			/* +0x34 */
	int m_lastBeat;			/* +0x38 */

	static bool sm_bNowHandlingProgramChange;

	CSKSysExMsgHandler();

	/* .text+0x3461b0, 73 bytes -- loops `len` times calling the 1-byte
	 * AnalizeAndProcess(unsigned char) overload with each successive
	 * byte of `buf` (confirmed via raw disassembly: EDX reloaded with
	 * `buf[i]` before each virtual call, not shown by the naive
	 * decompile). */
	virtual unsigned int AnalizeAndProcess(unsigned char *buf, int len);
	/* .text+0x346330, 151 bytes. Per-byte SysEx state machine: gate on
	 * Analize(), copy to buffer, maybe record-to-sequencer, always
	 * forward except-parameter-change; on a complete EOX/SOX-bracketed
	 * frame, RecParameterChange() then maybe SendToMIDIPortParameterChange()
	 * + SendToOtherModules(). */
	virtual unsigned int AnalizeAndProcess(unsigned char eoxByte);
	/* .text+0x3462d0, 73 bytes -- SOX(0xf0) resets the buffer and
	 * returns true; while m_inSysEx, EOX(0xf7) returns true (closing the
	 * frame) and any other byte with the high bit set (a new status
	 * byte interrupting a SysEx frame) aborts it (false). */
	virtual bool Analize(unsigned char byte);
	/* .text+0x346200, 22 bytes. */
	virtual void CopyToBuffer(unsigned char byte);
	/* .text+0x3464f0, 24 bytes. */
	virtual void RecToSequencerExceptParameterChange(unsigned char byte);
	/* .text+0x3463e0, 257 bytes -- transport-position duplicate
	 * suppression + program-change-inside-SysEx detection (message
	 * type 4, subtype 8/9). */
	virtual bool ShouldRecThisParameterChange(CSKParameterChangeMessage *msg);
	/* .text+0x346510, 316 bytes. */
	virtual void RecParameterChange();
	/* .text+0x346660, 517 bytes -- dispatches by the buffer's own
	 * manufacturer-ID-shaped bytes (message "type" tags 'm'/'A'/'n'/'C'
	 * checked via CSKParameterChangeMessage::IsThisParamChage() +
	 * m_buf[5], plus a Korg-KARMA-disable-input literal-byte pattern
	 * fallback) to the right Receive-or-Send pair. */
	virtual void SendToOtherModules();
	/* .text+0x346220, 1 byte -- empty. */
	virtual void SendToMIDIPortParameterChange();
	/* .text+0x346230, 1 byte -- empty. */
	virtual void SendToMIDIPortExceptParameterChange(unsigned char byte);
	/* .text+0x346320, 15 bytes -- real ground truth is a tail call to
	 * SPROutGate_IsEnableExclusive() with no explicit return-value
	 * handling; the compiler leaves that call's own EAX result in place
	 * for the caller, so despite Ghidra decompiling this as `void`, the
	 * real return value IS SPROutGate_IsEnableExclusive()'s bool
	 * (confirmed via raw disassembly -- AnalizeAndProcess(unsigned char)
	 * tests this call's AL result as a real branch condition). */
	virtual bool ShouldSendToOtherModules();
	/* .text+0x346240, 34 bytes. */
	virtual void SendToSTG();
	/* .text+0x3462b0, 23 bytes. */
	virtual void StoreKGParamChange();
	/* .text+0x346290, 23 bytes. */
	virtual void StoreSPRParamChange();
	/* .text+0x346270, 23 bytes. */
	virtual void StoreSTGParamChange();

	/* Non-virtual members, own separate addresses (not in the vtable). */
	/* .text+0x3468b0, 8 bytes -- real ground truth is __cdecl with no
	 * `this` use at all (touches only the static flag), so declared
	 * `static` here rather than as a normal instance method. */
	static void StartRecording();
	/* .text+0x3468c0, 22 bytes. */
	bool IsThisEnableForX2100();
	/* .text+0x3468e0/0x356910/0x356940/0x356970, 45 bytes each --
	 * identical shape, differ only in the 1-byte literal compared
	 * against m_buf[4] ('m'/'A'/'n'/'C'). */
	bool IsThisKGParamChage();
	bool IsThisSPRParamChage();
	bool IsThisDrumTrackParamChage();
	bool IsThisSTGParamChage();
	/* .text+0x3469a0, 57 bytes -- the same Korg-KARMA-disable-input
	 * literal-byte pattern SendToOtherModules() checks inline. */
	bool IsThisKarmaDisableInput();
	/* .text+0x3469e0/0x356a50/0x356ac0, 99 bytes each -- identical
	 * shape (receive-from-port-or-seq, forward if consumed by seq or
	 * CSPREngine state >= 7). */
	void ProcessKGParamChange();
	void ProcessSPRParamChange();
	void ProcessDrumTrackaramChange();	/* sic -- real ground-truth name,
						 * missing the 'P' in "Param". */
	/* .text+0x346b30, 55 bytes. */
	void ProcessSTGParamChange();
	/* .text+0x346b70, 27 bytes. */
	void ProcessKarmaDisableInput();
};

#endif /* OA_CKG_MIDI_MSG_HANDLER_H */
