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
#include "oa_bank_memory.h"		/* CSTGBankMemory::AllocAligned(), used by
					 * CSKMIDIInMsgHandler's own ctor to
					 * placement-allocate its embedded
					 * CSKSysExMsgHandler (see below). */

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
 * confirmed via its own vtable sharing the same 0x08-0x40 prefix) was
 * DELIBERATELY NOT reconstructed in this original batch -- deeper (33
 * methods, real dying-note-tracking array arithmetic, a 922-byte ctor)
 * and was left as this project's own next continuation target. That
 * continuation happened in a LATER 2026-07-28 batch (see this file's own
 * "UPDATE" section further down) -- CSKMIDIInMsgHandler and its 5 real
 * children are now fully reconstructed below; the 1-field opaque
 * stand-in mentioned here no longer exists.
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
/* CSKMIDIInMsgHandler's own free-function dependencies (real mangled
 * names confirmed the same way as above). `SPRMain_KeyboardOn` is a
 * plain C++ global function (not extern "C") -- its natural mangling
 * already matches the real symbol `_Z18SPRMain_KeyboardOnbiii`, no
 * `asm()` alias needed. */
extern "C" void SKSTGGate_StartMonitorSTGQueue(void) __attribute__((regparm(3)));
extern "C" bool SKSTGGate_EndMonitorSTGQueue(void) __attribute__((regparm(3)));
extern "C" void SPRMain_RecAutomationTrackMessage(int statusType, int data1, int data2, int channel)
	__attribute__((regparm(3)));
extern "C" void SPRMain_RecMIDITrackMessage(int statusType, int data1, int data2, int channel)
	__attribute__((regparm(3)));
extern bool SPRMain_KeyboardOn(bool onOff, int note, int channel, int velocity) __attribute__((regparm(3)));

/*
 * CKGEventDisplayManager -- UI note-event notifier, reached via
 * CKGEngine::ms_poKGEventDisplayManager (own real static, see
 * oa_ckg_module_param_msg_handler.h). Own class layout out of scope.
 */
struct CKGEventDisplayManager {
	void NoteOn(int note);
	void NoteOff(int note);
};

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

/*
 * === UPDATE 2026-07-28 (later batch): CSKMIDIInMsgHandler and its 5
 * children reconstructed ===
 *
 * Fresh `objdump -r` sweep against `.rodata._ZTV*` (same ground-truth
 * object as above) resolved the full real class graph, correcting this
 * header's own earlier "deliberately not reconstructed" note:
 *
 *   CSKMIDIMsgHandler (15 slots, above)
 *     -> CSKMIDIInMsgHandler (50 slots: 15 inherited + 35 new, 8 of
 *        which stay PURE at this level -- genuinely abstract)
 *          -> CSKMIDIPortMsgHandler (50 slots, all overrides, no new
 *             slots)
 *               -> CSKPadNoteByMIDIPortMsgHandler (50 slots, overrides
 *                  only dtor + ShouldNotifyToKarmaController +
 *                  CheckNoteMessageAndTriggerPad)
 *          -> CSKMIDILocalCtrlMsgHandler (64 slots: 50 inherited + 14
 *             new)
 *               -> CSKMIDIKarmaCtrlMsgHandler (64 slots, overrides only
 *                  dtor + ShouldNotifyToKarmaController +
 *                  CheckNoteMessageAndTriggerPad)
 *               -> CSKPadNoteByLocalCtrlMsgHandler (64 slots, same 3
 *                  overrides as CSKMIDIKarmaCtrlMsgHandler)
 *
 * No virtual inheritance anywhere in this sub-tree (no VTT/_ZTC
 * symbols) -- plain single inheritance throughout, unlike the CKGSwitch
 * widget family. Every real vtable-dispatch call in the raw disassembly
 * below was resolved via `rodata_offset = call_offset + 8` (see this
 * header's own "*this + N" gotcha above) against each class's own
 * captured relocation dump -- an easy off-by-slot trap the first pass
 * through this batch actually fell into twice (misreading which named
 * method a given `call [edx+N]` really targets) before insisting on a
 * full, explicit call_off->rodata_off->name table per class instead of
 * eyeballing individual offsets.
 *
 * === Object layout (CSKMIDIInMsgHandler, shared by every child) ===
 * All fields below confirmed directly from CSKMIDIInMsgHandler's own
 * 922-byte constructor's real zero-init sequence (`.text+0x344210`),
 * cross-checked against every method that reads/writes each offset:
 *   +0xc    unsigned char m_noteDownCount[128]  -- per-note held-down
 *                                                   counter, index = MIDI
 *                                                   note number
 *   +0x8c   int           m_noteOnCount         -- total currently-held
 *                                                   note count
 *   +0x90   CSKSysExMsgHandler *m_sysExHandler  -- heap-allocated via
 *                                                   CSTGBankMemory::
 *                                                   AllocAligned(sizeof
 *                                                   (CSKSysExMsgHandler),
 *                                                   0x10) + placement-new,
 *                                                   never freed
 *   +0x94   bool          m_bDamperOn
 *   +0x95   bool          m_bSostenutoOn
 *   +0x96   bool          m_softPedal
 *   +0x98   unsigned int  m_lastNotePerChannel[16]  -- init 0xff
 *   +0xd8   CDyingNoteInfo m_dyingNoteMIDIPort[16]  -- per-channel, own
 *                                                       0x84-byte opaque
 *                                                       type (see below)
 *   +0x918  CDyingNoteInfo m_dyingNoteSTG[16]
 *   +0x115c unsigned int  m_bypassKarmaNoteOnEvent[128]  -- raw 4-byte
 *                                                            event
 *                                                            snapshot,
 *                                                            index = note
 *   +0x135c int           m_dyingDamperTicks[16]   -- per-channel;
 *                                                       CheckDyingDamper()
 *                                                       tests >0
 *   +0x139c unsigned char m_dyingDamperFlag[16]    -- set by
 *                                                       CheckDyingDamper(),
 *                                                       consumed by
 *                                                       CSKMIDILocalCtrlMsgHandler
 *                                                       ::SendDyingDamperMessageToMIDIPort()
 *   +0x13ac unsigned char m_noteOnHoldCount[128]   -- Process()'s own
 *                                                       per-note counter
 *                                                       (STG-side gate,
 *                                                       distinct array
 *                                                       from
 *                                                       m_noteDownCount)
 *   +0x142c unsigned char m_extNoteOnChecker[128]  -- CSKMIDILocalCtrlMsgHandler
 *                                                       only (Regist/
 *                                                       UnRegist/
 *                                                       IsSendingNoteOnToExt,
 *                                                       PLUS reused
 *                                                       channel-indexed
 *                                                       [0..15] by
 *                                                       CheckDuplicateMessage()
 *                                                       for ChannelAftertouch
 *                                                       dup-suppression --
 *                                                       a real, confirmed
 *                                                       dual use of the
 *                                                       same storage, not
 *                                                       a transcription
 *                                                       error)
 *   +0x14ac int           m_perNoteTimbreTranspose[128][16]  -- CSKMIDILocalCtrlMsgHandler
 *                                                       only, zeroed by
 *                                                       its own ctor;
 *                                                       row=note (0-127),
 *                                                       col=timbre
 *                                                       (0-15); see the
 *                                                       2 lower-confidence
 *                                                       methods below
 *
 * === CDyingNoteInfo ===
 * Own class, size confirmed exactly 0x84 (132) bytes from both arrays'
 * channel stride AND `KillAllDyingNotes()`'s own `rep movs` (ecx=0x21
 * dwords = 0x84 bytes). Real mangled non-virtual (regparm(3): this=EAX,
 * explicit arg=EDX) methods only -- own internal layout out of scope.
 */
class CDyingNoteInfo {
	unsigned char m_opaque[0x84];
public:
	void Initialize();
	void TurnOn(int note);
	void TurnOff(int note);
	bool IsNoteOn(int note);
	bool IsAnyNotesOn();
};

/*
 * === CSKMIDIInMsgHandler ===
 * Class region `.text+0x353370`..`.text+0x3549b8`. Real direct child of
 * CSKMIDIMsgHandler, genuinely ABSTRACT: 8 of its own 35 new vtable
 * slots stay pure at this level (AnalizeAndSetParameter,
 * SendChannelMessageToMIDIPort, ShouldSendChannelMessageToMIDIPort,
 * ShouldNotifyToKarmaController, CheckGlobalParameterPreSendToKarmaEngine,
 * CheckGlobalParameterPreSendToSTG, ConvertPreMIDINote,
 * NotifyNoteCountToUI), each implemented by both
 * CSKMIDIPortMsgHandler and CSKMIDILocalCtrlMsgHandler independently.
 * No `virtual ~CSKMIDIMsgHandler()` exists at the base -- this class is
 * the FIRST one in the tree to add a real virtual destructor (extends
 * the vtable by 2 slots, D1/D0), which is why every child below needs no
 * hand-written destructor at all -- g++ regenerates the standard
 * install-vtable-ptr-then-call-base-dtor boilerplate matching ground
 * truth's own D1/D0 bodies automatically once the inheritance chain is
 * declared correctly.
 */
class CSKMIDIInMsgHandler : public CSKMIDIMsgHandler {
public:
	unsigned char m_noteDownCount[128];		/* +0xc */
	int m_noteOnCount;				/* +0x8c */
	CSKSysExMsgHandler *m_sysExHandler;		/* +0x90 */
	bool m_bDamperOn;				/* +0x94 */
	bool m_bSostenutoOn;				/* +0x95 */
	bool m_softPedal;				/* +0x96 */
	unsigned int m_lastNotePerChannel[16];		/* +0x98 */
	CDyingNoteInfo m_dyingNoteMIDIPort[16];	/* +0xd8 */
	CDyingNoteInfo m_dyingNoteSTG[16];		/* +0x918 */
	unsigned int m_bypassKarmaNoteOnEvent[128];	/* +0x115c */
	int m_dyingDamperTicks[16];			/* +0x135c */
	unsigned char m_dyingDamperFlag[16];		/* +0x139c */
	unsigned char m_noteOnHoldCount[128];		/* +0x13ac */

	static bool ms_bShouldStopSendingNoteOnsToSTG;

	/* .text+0x354210, 922 bytes. */
	CSKMIDIInMsgHandler();
	virtual ~CSKMIDIInMsgHandler() {}

	/* .text+0x353400, 56 bytes. */
	virtual bool ShouldSendChannelMessageToKarmaEngine();
	/* .text+0x353440, 116 bytes. */
	virtual void StoreNoteEvent();
	/* .text+0x3534d0, 59 bytes. */
	virtual void CheckDamperStatus();
	/* .text+0x353510, 59 bytes. */
	virtual void CheckSostenutoStatus();
	/* .text+0x353550, 1 byte -- empty. */
	virtual void NotifyDamperStatusToUI();
	/* .text+0x353560, 1 byte -- empty. */
	virtual void NotifySostenutoStatusToUI();
	/* .text+0x353570, 8 bytes. */
	virtual bool IsDamperOn();
	/* .text+0x353580, 8 bytes. */
	virtual bool IsSostenutoOn();
	/* .text+0x353590, 6 bytes. */
	virtual bool CheckDuplicateMessage();
	/* .text+0x3535a0, 134 bytes, regparm(3). */
	virtual bool AnalizeAndProcessNoteOffWhilePerformanceChange(unsigned char *buf, int len);
	/* .text+0x353630, 41 bytes. */
	virtual void ReserveBypassKARMANoteOnEvent(int note);
	/* .text+0x353670, 176 bytes. */
	virtual bool CheckBypassKARMANoteOnEvent(int note);
	/* .text+0x353730, 216 bytes. */
	virtual bool CheckDyingNoteForMIDIPort();
	/* .text+0x353820, 67 bytes. */
	virtual void ProcessForDyingNote();
	/* .text+0x353870, 227 bytes. */
	virtual bool IsEnableViaRPPR();
	/* .text+0x353970, 235 bytes. */
	virtual void NotifyNoteEventToUI();
	/* .text+0x353a60, 100 bytes. */
	virtual void CheckSoftPedalStatus();
	/* .text+0x353ad0, 352 bytes. */
	virtual bool ShouldRecChannelMessageToSequencer();
	/* .text+0x353c40, 316 bytes. */
	virtual bool ShouldSendChannelMessageToSTG();
	/* .text+0x353d90, 60 bytes. */
	virtual void SendChannelMessageToKarmaEngine();
	/* .text+0x353dd0, 95 bytes. */
	virtual bool CheckNoteMessageAndTriggerPad();
	/* .text+0x353e30, 81 bytes. */
	virtual void NotifyCCToKarmaController();
	/* .text+0x353e90, 652 bytes. */
	virtual void Process();
	/* .text+0x354130, 209 bytes, regparm(3). */
	virtual bool AnalizeAndProcess(unsigned char *buf, int len);
	/* .text+0x3545b0, 631 bytes. */
	void KillAllDyingNotes();
	/* .text+0x354830, 1 byte -- empty, non-virtual (own overload,
	 * distinct from the base class's virtual same-named method). */
	void StoreDyingNoteInfoForSTG(CMIDIMessage *msg) { (void)msg; }
	/* .text+0x354840, 1 byte -- empty, non-virtual. */
	void StoreDyingNoteInfoForMIDPort(CMIDIMessage *msg) { (void)msg; }
	/* .text+0x354850, 46 bytes. */
	void ClearKeyboardStatus();
	/* .text+0x354880, 312 bytes. */
	void CheckDyingDamper();

	/* === Pure virtuals -- no body at this level, first implemented by
	 * CSKMIDIPortMsgHandler / CSKMIDILocalCtrlMsgHandler below === */
	virtual bool AnalizeAndSetParameter(unsigned char *buf, int len) = 0;
	virtual void SendChannelMessageToMIDIPort() = 0;
	virtual bool ShouldSendChannelMessageToMIDIPort() = 0;
	virtual bool ShouldNotifyToKarmaController() = 0;
	virtual bool CheckGlobalParameterPreSendToKarmaEngine() = 0;
	virtual bool CheckGlobalParameterPreSendToSTG() = 0;
	virtual void ConvertPreMIDINote() = 0;
	virtual void NotifyNoteCountToUI() = 0;

	/* .text+0x353390, 93 bytes -- NOT pure (real base body), overridden
	 * only by the 2 CSKPadNoteBy* leaves via a different vtable slot
	 * layer up the tree (CSKMIDIPortMsgHandler/CSKMIDILocalCtrlMsgHandler
	 * keep this exact base body unchanged, confirmed by both classes'
	 * own vtable relocations still pointing at this symbol). */
	virtual void ProcessPadTriggerNote();
};

/*
 * === CSKMIDIPortMsgHandler ===
 * Class region `.text+0x355a30`..`.text+0x355d3f` (ctor). Direct child
 * of CSKMIDIInMsgHandler, implements all 8 inherited pure virtuals, adds
 * no new vtable slots of its own.
 */
class CSKMIDIPortMsgHandler : public CSKMIDIInMsgHandler {
public:
	/* .text+0x355d10, 47 bytes. */
	CSKMIDIPortMsgHandler();

	/* .text+0x355a30, 3 bytes. */
	virtual bool ShouldSendChannelMessageToMIDIPort();
	/* .text+0x355a40, 1 byte -- empty. */
	virtual void SendChannelMessageToMIDIPort();
	/* .text+0x355a50, 1 byte -- empty. */
	virtual void ConvertPreMIDINote();
	/* .text+0x355a70, 93 bytes. */
	virtual bool CheckGlobalParameterPreSendToSTG();
	/* .text+0x355ae0, 37 bytes. */
	virtual bool ShouldNotifyToKarmaController();
	/* .text+0x355b10, 178 bytes, regparm(3). */
	virtual bool AnalizeAndSetParameter(unsigned char *buf, int len);
	/* .text+0x355bd0, 42 bytes. */
	virtual void NotifyNoteCountToUI();
	/* .text+0x355c20, 223 bytes. */
	virtual bool CheckGlobalParameterPreSendToKarmaEngine();

	/* .text+0x355a60, 1 byte -- empty; own vtable slot inherited from
	 * CSKMIDIMsgHandler (not CSKMIDIInMsgHandler's own extension). */
	virtual void ConvertPreMIDIAfterTouch();
};

/*
 * === CSKPadNoteByMIDIPortMsgHandler ===
 * Class region `.text+0x355c00`..`.text+0x355c1f`. Leaf, overrides only
 * the 2 pad-note slots (both trivial `return false`).
 */
class CSKPadNoteByMIDIPortMsgHandler : public CSKMIDIPortMsgHandler {
public:
	/* .text+0x355c00, 3 bytes. */
	virtual bool ShouldNotifyToKarmaController();
	/* .text+0x355c10, 3 bytes. */
	virtual bool CheckNoteMessageAndTriggerPad();
};

/*
 * === CSKMIDILocalCtrlMsgHandler ===
 * Class region `.text+0x3549c0`..`.text+0x345a0`. Direct child of
 * CSKMIDIInMsgHandler, implements the same 8 inherited pure virtuals
 * (independently from CSKMIDIPortMsgHandler) AND adds 14 genuinely new
 * vtable slots of its own (inherited unchanged by both
 * CSKMIDIKarmaCtrlMsgHandler and CSKPadNoteByLocalCtrlMsgHandler below).
 *
 * 2 methods below (both SendChannelMessageInCombiOtherTimbreToMIDIPort
 * overloads) are a LOWER-CONFIDENCE reconstruction than everything else
 * in this file: real control flow was traced from raw disassembly
 * (confirmed calls, branch conditions, and the real
 * `m_perNoteTimbreTranspose[note][timbre]` addressing math), but the
 * exact per-branch register-to-parameter mapping was not independently
 * re-verified against a second read the way every other method in this
 * batch was (see this header's own "3 real transcription bugs" section
 * above for why that 2nd pass matters) -- flagged here explicitly rather
 * than silently presented as equally solid. Left in (not stubbed to a
 * no-op) because leaving either slot pure would make this whole class,
 * plus its own CSKMIDIKarmaCtrlMsgHandler and
 * CSKPadNoteByLocalCtrlMsgHandler children, non-instantiable.
 */
class CSKMIDILocalCtrlMsgHandler : public CSKMIDIInMsgHandler {
public:
	unsigned char m_extNoteOnChecker[128];			/* +0x142c */
	int m_perNoteTimbreTranspose[128][16];			/* +0x14ac */

	/* .text+0x3458d0, 245 bytes. */
	CSKMIDILocalCtrlMsgHandler();

	/* === Overrides of CSKMIDIInMsgHandler's 8 pure virtuals (any
	 * declaration order -- these occupy already-fixed inherited vtable
	 * slots, not new ones) === */
	/* .text+0x345010, 107 bytes, regparm(3). */
	virtual bool AnalizeAndSetParameter(unsigned char *buf, int len);
	/* .text+0x345170, 61 bytes. */
	virtual void SendChannelMessageToMIDIPort();
	/* .text+0x345380, 181 bytes. */
	virtual bool ShouldSendChannelMessageToMIDIPort();
	/* .text+0x344ec0, 13 bytes. */
	virtual bool ShouldNotifyToKarmaController();
	/* .text+0x344e40, 13 bytes. */
	virtual bool CheckGlobalParameterPreSendToKarmaEngine();
	/* .text+0x344e50, 13 bytes. */
	virtual bool CheckGlobalParameterPreSendToSTG();
	/* .text+0x344e60, 69 bytes. */
	virtual void ConvertPreMIDINote();
	/* .text+0x345080, 42 bytes. */
	virtual void NotifyNoteCountToUI();

	/* === Overrides of 3 more CSKMIDIInMsgHandler REAL (non-pure)
	 * virtuals (also already-fixed inherited slots) === */
	/* .text+0x3450f0, 84 bytes. */
	virtual bool CheckDuplicateMessage();
	/* .text+0x3450b0, 23 bytes. */
	virtual void NotifyDamperStatusToUI();
	/* .text+0x3450d0, 23 bytes. */
	virtual void NotifySostenutoStatusToUI();

	/* === 14 genuinely NEW virtual slots (rodata 0xd0-0x104), inherited
	 * unchanged by CSKMIDIKarmaCtrlMsgHandler and
	 * CSKPadNoteByLocalCtrlMsgHandler below -- declaration order here
	 * MUST match real vtable order (confirmed via `objdump -r`), unlike
	 * every override group above. === */
	/* .text+0x344ed0, 28 bytes. */
	virtual void InitializeExtNoteOnChecker();
	/* .text+0x344f40, 51 bytes. */
	virtual void CopyNoteOnStatus(unsigned char *dst);
	/* .text+0x344f80, 31 bytes. */
	virtual bool IsKeyboardAllOff();
	/* .text+0x344fb0, 94 bytes. */
	virtual void ClearNoteStatus();
	/* .text+0x345830, 156 bytes. */
	virtual bool IsNotThruKarma(int channel);
	/* .text+0x345790, 146 bytes. */
	virtual unsigned int GetKarmaControlledChannelPat(bool includeAllModules);
	/* .text+0x3451b0, 444 bytes -- LOWER CONFIDENCE, see class comment. */
	virtual void SendChannelMessageInCombiOtherTimbreToMIDIPort();
	/* .text+0x3454e0, 371 bytes -- LOWER CONFIDENCE, see class comment. */
	virtual void SendChannelMessageInCombiOtherTimbreToMIDIPort(int timbre, bool applySustainFilter);
	/* .text+0x3449c0, 1090 bytes. */
	virtual void SendDyingDamperMessageToMIDIPort();
	/* .text+0x345440, 145 bytes. */
	virtual bool CheckGlobalParameterPreSendToMIDIPort();
	/* .text+0x345670, 262 bytes. */
	virtual bool CheckTimbreParameterPreSendToMIDIPort(int timbre);
	/* .text+0x344ef0, 14 bytes. */
	virtual void RegistExtNoteOn(int note);
	/* .text+0x344f00, 28 bytes. */
	virtual void UnRegistExtNoteOn(int note);
	/* .text+0x344f20, 21 bytes. */
	virtual bool IsSendingNoteOnToExt(int note);
};

/*
 * === CSKMIDIKarmaCtrlMsgHandler ===
 * Class region `.text+0x345a00`..`.text+0x345a2f`. Leaf, overrides only
 * the 2 pad-note slots.
 */
class CSKMIDIKarmaCtrlMsgHandler : public CSKMIDILocalCtrlMsgHandler {
public:
	/* .text+0x345a00, 33 bytes. */
	CSKMIDIKarmaCtrlMsgHandler();

	/* .text+0x3459d0, 3 bytes. */
	virtual bool ShouldNotifyToKarmaController();
	/* .text+0x3459e0, 24 bytes. */
	virtual bool CheckNoteMessageAndTriggerPad();
};

/*
 * === CSKPadNoteByLocalCtrlMsgHandler ===
 * Class region `.text+0x345150`..`.text+0x34516f`. Leaf, overrides only
 * the 2 pad-note slots (both trivial `return false`, same shape as
 * CSKPadNoteByMIDIPortMsgHandler above).
 */
class CSKPadNoteByLocalCtrlMsgHandler : public CSKMIDILocalCtrlMsgHandler {
public:
	/* .text+0x345150, 3 bytes. */
	virtual bool ShouldNotifyToKarmaController();
	/* .text+0x345160, 3 bytes. */
	virtual bool CheckNoteMessageAndTriggerPad();
};

#endif /* OA_CKG_MIDI_MSG_HANDLER_H */
