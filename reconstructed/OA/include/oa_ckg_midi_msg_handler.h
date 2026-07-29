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
 * static (own relocation, not offset through ms_poInstance) tested
 * for bit 0x40 ("precounting" gate) to pick which of the two location
 * getters to call. Own class layout otherwise out of scope; the 4
 * by-reference int outputs are opaque bar/beat/tick-shaped location
 * fields, real names unconfirmed.
 *
 * Round 62 batch (2026-07-29, solo, 13 methods): a fresh survey found
 * `CSPRClockHandler` has 83 `nm -C` methods total (SPR = sequencer
 * pattern recorder), of which these 13 are fully self-contained
 * (raw `this`-offset reads/writes plus the class's own statics, no
 * calls into other unreconstructed classes -- matching the CDDriverIO/
 * CPcgSaveInfo/CFileOperation scope discipline established in Eva this
 * session). `ms_oStatusMaster` WIDENED from `unsigned char` to
 * `unsigned int` here: ground truth's own `CopyStatusLocalToMaster`/
 * `CopyStatusMasterToLocal` bodies do genuine 4-byte reads/writes
 * through it, and Ghidra's own "Globals starting with '_' overlap
 * smaller symbols at the same address" warning on those two functions
 * CONFIRMS `_ms_oStatusMaster` is the same relocation as the already-
 * declared 1-byte `ms_oStatusMaster` -- not a guess. Existing callers
 * (`(ms_oStatusMaster & 0x40) == 0` in ckg_midi_msg_handler.cpp) are
 * unaffected: an `unsigned int & 0x40` reads the identical low-order
 * bit as the previous `unsigned char` did (verified by updating
 * `test_ckg_midi_msg_handler.cpp`'s own local definition to match --
 * `make verify` confirms it still passes).
 *
 * `ms_oStatusMasterTick`/`ms_oStatusLocalCopy` are 2 more real statics
 * this round's survey turned up, paired with `ms_oStatusMaster` as a
 * `CSPRTimerStatus`-shaped (bar,tick) status snapshot -- ground truth's
 * own `._0_4_`/`._4_4_` sub-range notation on `ms_oStatusLocalCopy`
 * confirms it's genuinely ONE 8-byte object, not two coincidentally-
 * adjacent globals; `ms_oStatusMaster`/`ms_oStatusMasterTick` are kept
 * as two SEPARATE statics since ground truth shows them as two
 * independently-named symbols (`ms_oStatusMaster`/`DAT_00b73a98`) with
 * no equivalent same-object proof.
 *
 * DEFERRED, 1 reason (70 of 83 methods): most of the rest forward
 * straight into other wholly-unreconstructed sibling classes
 * (`CSPRRTMIDIOutManager`, `CSPRRecorder`, `CSPRMetronome`,
 * `CSPREngine`) -- e.g. `SendMIDIClock`/`SendStop`/`SendStart`/
 * `NotifyClockToRecorder`/`ProcessMetroneme`/`InvokeSPREngine`, all
 * single-line __cdecl-to-__cdecl tail calls with no visible arguments
 * (genuinely void->void per both signatures, not a Ghidra mis-
 * recovery, but still out of scope until those sibling classes exist).
 */
struct CSPRTimerStatus {
	unsigned int bar;
	unsigned int tick;
};

struct CSeqEvent;

struct CSPRClockHandler {
	static unsigned char *ms_poInstance;
	static unsigned int ms_oStatusMaster;
	static unsigned int ms_oStatusMasterTick;	/* DAT_00b73a98 */
	static unsigned int ms_oStatusLocalCopy[2];	/* {bar, tick} */

	void GetCurrentLocation(int *a, int *b, int *c, int *d);
	void GetPrecountLocation(int *a, int *b, int *c, int *d);

	/* Round 62: a genuine C++ overload of GetCurrentLocation above --
	 * confirmed as a SEPARATE real symbol (different address/size,
	 * `nm -C`) from the 4-out-param version, not a duplicate. */
	void GetCurrentLocation(int *bar, int *tick);

	void DisableStop();
	void EnableStop();
	void DisableToProcessWhen1ClockUp();
	void EnableToProcessWhen1ClockUp();
	void InitializeTempo(int tempo);
	void ChangeTempoWhenStarting();
	void SetLocationInfoWhenStop(int value);
	void ModeOn();
	void CopyStatusLocalToMaster(CSPRTimerStatus *status);
	static void CopyStatusMasterToLocal();
	/* Real prototype takes only a single CSeqEvent*; ground truth's own
	 * C-level signature shows a second, entirely unread int parameter
	 * (a harmless Ghidra over-declaration, same "provably-unused extra
	 * formal parameter" shape already seen elsewhere in this project --
	 * not reproduced here). */
	bool HandleBarEventBackward(CSeqEvent *ev);
	/* .text+0x3b6900, 1 byte -- real body is an unconditional `return;`,
	 * a compiler-generated static-initializer stub for `ms_poInstance`'s
	 * own translation-unit init order, not meaningful application logic.
	 * Reproduced faithfully as a no-op. */
	static void _GLOBAL__I_ms_poInstance();
};

/*
 * CSKParameterChangeMessage -- a "reinterpret this+offset as a different
 * class" view: CSKSysExMsgHandler's own 0x20-byte SysEx scratch buffer
 * (at this+0x10) is passed AS `this` to these methods directly (confirmed
 * via disassembly -- `lea 0x10(%eax),%ebx; mov %ebx,%eax; call
 * CSKParameterChangeMessage::IsThisParamChage()`), not a separately
 * allocated object.
 *
 * Full 14-byte layout (all confirmed from `SetParameters()`'s own field
 * writes, `.text+0x341760`/`.text+0x3417f0`), a fixed-shape Korg "GE
 * (Global Effect?) parameter change" SysEx message:
 *   +0x00  0xf0            SOX, fixed (ctor only)
 *   +0x01  0x42            Korg manufacturer ID, fixed (ctor only)
 *   +0x02  globalChannel | 0x30   ("mode" high nibble | channel low
 *                                  nibble -- SetSourceSeq()/
 *                                  SetSourceSeqRestore()/ResetSourceSeq()
 *                                  instead OR the low nibble with
 *                                  0x80/0x90/0x30 directly, preserving it)
 *   +0x03  0x68            fixed function ID (ctor only)
 *   +0x04  kind byte (param1) -- IsThisParamChage() only recognizes
 *                                'm'(0x6d)/'C'(0x43)/'n'(0x6e)/'A'(0x41)
 *   +0x05  param2
 *   +0x06  param3
 *   +0x07  param4 (byte-overload SetParameters leaves this 0, ctor
 *                  default; the int-overload instead stores its own
 *                  4th param here -- see that overload's own comment)
 *   +0x08  param4 (byte-overload) / param5 (int-overload)
 *   +0x09  param5 (byte-overload) / param6 (int-overload)
 *   +0x0a  value bits 20:14 (7 bits, `(value>>14)&0x7f`)
 *   +0x0b  value bits 13:7  (7 bits, `(value>>7)&0x7f`)
 *   +0x0c  value bits 6:0   (7 bits, `value&0x7f`)
 *   +0x0d  0xf7            EOX, fixed (ctor only)
 * `SetValue(int)` rewrites only +0xa/+0xb/+0xc (the 3-byte value split);
 * `SetValue(CSeqEvent*)` is a distinct "build myself from a sequencer
 * event record" constructor-style overload -- `CSeqEvent`'s own layout is
 * out of scope, only ever read through as a raw byte buffer at fixed
 * offsets (event+9..+15, event+0x11..+0x15), matching the same
 * "reinterpret as raw bytes" idiom used throughout this file.
 */
struct CSeqEvent;

struct CSKParameterChangeMessage {
	unsigned char m_bytes[0xe];	/* +0x00, see layout above */

	/* .text+0x341740, 6 bytes -- fixed-byte init only (+0,+1,+3,+7,+0xd);
	 * +0x2/+0x4..+0xc are left as-is (real ground truth never zeroes
	 * them here -- every real caller always calls SetParameters()/
	 * SetValue() right after construction). */
	CSKParameterChangeMessage()
	{
		m_bytes[0x0] = 0xf0;
		m_bytes[0x1] = 0x42;
		m_bytes[0x3] = 0x68;
		m_bytes[0x7] = 0x00;
		m_bytes[0xd] = 0xf7;
	}

	/* .text+0x341760, 142 bytes, regparm(3). */
	void SetParameters(unsigned char param1, unsigned char param2, unsigned char param3,
			    unsigned char param4, unsigned char param5, int value);
	/* .text+0x3417f0, 126 bytes, regparm(3) -- same shape as the 5-byte
	 * overload above but with a 6th explicit int param (stored at
	 * +0x7, where the byte-overload instead leaves a fixed 0). */
	void SetParameters(int param1, int param2, int param3, int param4, int param5,
			    int param6, long value);
	/* .text+0x341870, 29 bytes, regparm(3) -- rewrites only the 3-byte
	 * value split (+0xa/+0xb/+0xc), leaving every other field alone. */
	void SetValue(int value)
	{
		m_bytes[0xa] = (unsigned char)((value >> 14) & 0x7f);
		m_bytes[0xb] = (unsigned char)((value >> 7) & 0x7f);
		m_bytes[0xc] = (unsigned char)(value & 0x7f);
	}
	/* .text+0x341890, 81 bytes. Real body: false unless +0x1==0x42 AND
	 * +0x2 == (CKGBankManager::ms_poInstance[0x97c747] | 0x30) AND
	 * +0x3==0x68 AND the +0x4 kind byte is one of 'm'/'C'/'n'/'A'. */
	bool IsThisParamChage();
	/* .text+0x3418f0, 47 bytes. Real body: reassembles the 3-byte value
	 * split back into a single int (mirrors SetValue(int)'s own split,
	 * plus a real sign-extension-shaped high-bit spread on the MSB
	 * byte's own bit 6 -- transcribed as observed). */
	unsigned int GetValue();
	/* .text+0x341920, 100 bytes, regparm(3). Real body: builds a whole
	 * new message directly from a `CSeqEvent`'s own raw bytes (fixed
	 * SOX/EOX at +0/+0xd like the ctor, but +0x1..+0xc all copied
	 * verbatim from `seqEvent+9..+15` and `seqEvent+0x11..+0x15` --
	 * `CSeqEvent`'s own field names are unconfirmed, only its raw byte
	 * layout at these fixed offsets is used here). */
	void SetValue(CSeqEvent *seqEvent);
	/* .text+0x341990, 14 bytes. Real body: (+0x2 & 0xf) | 0x80,
	 * preserving the channel low nibble. */
	void SetSourceSeq()
	{
		m_bytes[0x2] = (unsigned char)((m_bytes[0x2] & 0xf) | 0x80);
	}
	/* .text+0x3419a0, 14 bytes. Real body: (+0x2 & 0xf) | 0x90. */
	void SetSourceSeqRestore()
	{
		m_bytes[0x2] = (unsigned char)((m_bytes[0x2] & 0xf) | 0x90);
	}
	/* .text+0x3419b0, 14 bytes. Real body: (+0x2 & 0xf) | 0x30. */
	void ResetSourceSeq()
	{
		m_bytes[0x2] = (unsigned char)((m_bytes[0x2] & 0xf) | 0x30);
	}
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
 * CKGEventDisplayManager -- UI note/CC/bend-event notifier, reached via
 * CKGEngine::ms_poKGEventDisplayManager (own real static, see
 * oa_ckg_module_param_msg_handler.h). Previously a 2-method opaque
 * stand-in here; now fully reconstructed (15/15 methods) from all real
 * `objdump -dr` bodies, own class region starting at `.text+0x3b40a0`
 * (own real ctor's address; see this class's own last real method,
 * Idle() at `.text+0x3b5500`, for the other end -- deliberately NOT
 * citing "one past the end" here, since that address belongs to an
 * unrelated real function, KGMain_Initialize, not this class -- see
 * gen_oa_manifest.py's own documented gotcha about exactly this). No
 * vtable (plain class, confirmed: every consumer calls it as a direct,
 * non-virtual member function).
 *
 * The real struct's own field names are not recoverable from
 * disassembly alone (every access is raw dword-indexed arithmetic, no
 * symbol table for member offsets), so the whole object is represented
 * as one flat `int m_flat[]` array; every method below transcribes the
 * exact real index formula, not an invented "clean" reshaping.
 *
 * Real layout (DWORD indices, i.e. byte offset / 4):
 *   [0x000,0x280) m_flat[objectIndex*128+note]  -- live note-on
 *     reference count. 5 objectIndex rows: 0 = direct/local source,
 *     1-4 = GetNoteObjectIndex(module) for the 4 KARMA modules.
 *   [0x280,0x2c0) m_flat[module*16+groupIndex]  -- live CC/bend
 *     reference count (16-dword stride rows, only module 0-3 used).
 *     groupIndex = cc/8 for CC events, bendValue/1024 for pitch bend --
 *     confirmed via disasm both feed this SAME array.
 *   [0x2c0,0x388) m_flat[0x2c0+ring*20+objectIndex*4+(note>>5)] --
 *     10-slot (ring 0-9) ring buffer of 128-bit "note touched this
 *     write-window" bitmasks, one 4-dword bitmask per objectIndex.
 *   [0x388,~0x3b0) m_flat[0x388+ring*4+module] -- 10-slot ring buffer
 *     of 16-bit "CC/bend group touched this write-window" bitmasks,
 *     one dword per module (only the low 16 bits used).
 *   0x3b0 (+0xec0) "now" tick counter, written elsewhere (out of
 *     scope) -- Idle() only ever reads it.
 *   0x3b1 (+0xec4) last-serviced tick checkpoint for note aging.
 *   0x3b2 (+0xec8) last-serviced tick checkpoint for CC/bend aging.
 *   0x3b3 (+0xecc) NOTE ring WRITE cursor (which ring slot NoteOn/
 *     NoteOff currently mark into).
 *   0x3b4 (+0xed0) NOTE ring READ cursor (which ring slot
 *     CheckAndProcessNoteStatus ages out next); Initialize() seeds
 *     this to 1, every other dword to 0.
 *   0x3b5 (+0xed4) CC ring WRITE cursor.
 *   0x3b6 (+0xed8) CC ring READ cursor; also seeded to 1.
 * Everything Initialize() zeroes spans [0,0xedc) bytes = 0x3b7 dwords;
 * sized generously to 0x3c0 here.
 *
 * Also touches a FOREIGN object -- `CKGBankManager::ms_poInstance[+8]`,
 * the same "note-display sub-object" pointer already established in
 * CSKMIDIInMsgHandler::NotifyNoteEventToUI() above (own layout out of
 * scope):
 *   sub+0x723c  unsigned int[5][4]  -- UI-facing "note visibly on"
 *     bitmask, [objectIndex][note>>5], cleared bit-by-bit only once the
 *     matching m_flat[] reference count above reaches 0.
 *   sub+0x728c  unsigned int[4]     -- UI-facing "CC/bend group visibly
 *     on" bitmask, [module], same clear-on-zero-refcount idiom.
 */
struct CKGEventDisplayManager {
	enum {
		OA_KGEVTDISP_NOTE_DATA       = 0x000,
		OA_KGEVTDISP_CC_DATA         = 0x280,
		OA_KGEVTDISP_NOTE_RING       = 0x2c0,
		OA_KGEVTDISP_CC_RING         = 0x388,
		OA_KGEVTDISP_TICK_NOW        = 0x3b0,	/* +0xec0 */
		OA_KGEVTDISP_NOTE_CHECKPOINT = 0x3b1,	/* +0xec4 */
		OA_KGEVTDISP_CC_CHECKPOINT   = 0x3b2,	/* +0xec8 */
		OA_KGEVTDISP_NOTE_WCURSOR    = 0x3b3,	/* +0xecc */
		OA_KGEVTDISP_NOTE_RCURSOR    = 0x3b4,	/* +0xed0 */
		OA_KGEVTDISP_CC_WCURSOR      = 0x3b5,	/* +0xed4 */
		OA_KGEVTDISP_CC_RCURSOR      = 0x3b6,	/* +0xed8 */
		OA_KGEVTDISP_STATE_DWORDS    = 0x3c0,
	};
	int m_flat[OA_KGEVTDISP_STATE_DWORDS];

	/* real ctor is a bare `ret` -- no member init at all (matches
	 * placement into raw AllocAligned() storage; real callers always
	 * invoke Initialize() next). */
	CKGEventDisplayManager() {}

	/* Real, self-contained pure formula -- also independently confirmed
	 * INLINED verbatim into NoteOnByKarma/NoteOffByKarma below (same
	 * table load + same default), not just called through. */
	static int GetNoteObjectIndex(int module)
	{ return (unsigned int)module <= 3 ? module + 1 : 1; }

	void Initialize();
	void NoteOn(int note);
	void NoteOnByKarma(int module, int note);
	void NoteOff(int note);
	void NoteOffByKarma(int module, int note);
	/* direct-objectIndex overloads -- same bodies as the pair above,
	 * just with an explicit objectIndex parameter instead of an
	 * implicit 0 or a GetNoteObjectIndex(module) lookup. */
	void NoteOn(int objectIndex, int note);
	void NoteOff(int objectIndex, int note);
	void CCOnByKarma(int module, int cc);
	void BendOnByKarma(int module, int bendValue);
	void CCOn(int module, int cc);
	void CheckAndProcessNoteStatus();
	void CheckAndProcessCCStatus();
	void Idle();

private:
	/* Shared helpers -- ground truth duplicates this logic verbatim
	 * across 3 (Note) + 3 (CC/bend) near-identical real functions;
	 * factored here since all instances were confirmed byte-identical
	 * from this step onward, same "shared duplicate logic" convention
	 * used elsewhere in this project. Not real ground-truth symbols of
	 * their own -- no .text address. */
	void MarkNoteOn(int objectIndex, int note);
	void MarkNoteOff(int objectIndex, int note);
	void MarkCCOrBendOn(int module, int groupIndex);
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

/*
 * === CSKMIDIMsgProcessor ===
 * The top-level MIDI-in dispatch owner: constructs and owns one instance
 * of each of the 6 concrete leaves reconstructed above, and pumps the 3
 * real MIDI-in queues (local-control, MIDI-port, pads) every engine tick.
 * Previously a 6-method opaque stand-in in oa_ckg_control_ui_msg.h (moved
 * here now that its real dependencies -- CSKMIDIPortMsgHandler etc. --
 * are all real classes); that file's own stand-in is gone, replaced by a
 * pointer comment to here.
 *
 * All 9 free functions below are plain (non-`extern "C"`) global C++
 * functions -- confirmed via `nm` that their natural Itanium mangling
 * already matches ground truth exactly (`_Z21SKSTGGate_ReceivePadsPh` etc,
 * same idiom already established for `SPRMain_KeyboardOn` above), no
 * `asm()` alias needed.
 */
bool SKSTGGate_ReceivePads(unsigned char *buf) __attribute__((regparm(3)));
void SKSTGGate_ResistReadQueus(void) __attribute__((regparm(3)));
int SPROutGate_GetGlobalChannel(void) __attribute__((regparm(3)));
bool QueuesFilteredDuringPerfChange(void) __attribute__((regparm(3)));
void KGMain_ReceiveControllerMessage(unsigned char *buf) __attribute__((regparm(3)));
void SKMain_CheckAndProcessPreemption(void) __attribute__((regparm(3)));
void SPRMain_ReceiveControllerMessage(unsigned char *buf) __attribute__((regparm(3)));
bool SKSTGGate_ReceiveFromMIDIPortQueus(unsigned char *buf, unsigned int len, int *outLen)
	__attribute__((regparm(3)));
bool SKSTGGate_ReceiveFromLocalControlQueus(unsigned char *buf, unsigned int len, int *outLen)
	__attribute__((regparm(3)));

/*
 * Real field layout (all confirmed via the 217-byte ctor's own
 * `CSTGBankMemory::AllocAligned()`+placement-ctor sequence, cross-checked
 * against `objdump -r`):
 *   +0x00  CSKMIDIPortMsgHandler *m_port            AllocAligned(0x142c,0x10)
 *   +0x04  CSKMIDILocalCtrlMsgHandler *m_localCtrl  AllocAligned(0x34ac,0x10)
 *   +0x08  CSKSpecialMsgHandler *m_special          AllocAligned(0xc,0x10)
 *   +0x0c  CSKMIDIKarmaCtrlMsgHandler *m_karmaCtrl  AllocAligned(0x34ac,0x10)
 *   +0x10  CSKPadNoteByMIDIPortMsgHandler *m_padByPort   -- built via
 *          CSKMIDIPortMsgHandler's OWN ctor (AllocAligned(0x142c,0x10),
 *          same size), then the vptr slot is overwritten in place with
 *          `vtable for CSKPadNoteByMIDIPortMsgHandler`'s own address --
 *          a real, confirmed ground-truth idiom (construct via the
 *          cheaper parent ctor, then "reclassify" by poking the vptr),
 *          not a transcription guess. Modeled here via placement-new
 *          construction through the derived type directly, which
 *          produces the identical end state without needing to hand-poke
 *          a vptr.
 *   +0x14  CSKPadNoteByLocalCtrlMsgHandler *m_padByLocal  -- same trick,
 *          built from CSKMIDILocalCtrlMsgHandler's own 0x34ac allocation.
 *   +0x18  int   m_lastMsgKind    -- selector written before most
 *          dispatch calls (0=port, 1=local-control-shaped, or the real
 *          channel nibble in ReadMIDILocalControlQueue()'s own special
 *          case); exact enum semantics not independently confirmed
 *          beyond the literal values transcribed per call site.
 *   +0x1c  unsigned char *m_activeRawEvent -- points at whichever owned
 *          sub-handler's own +4 raw-event byte quad while a dispatch
 *          call is in flight (read back by
 *          GetNowProcessingNoteOffVelocity()), null otherwise.
 *   +0x20  int   m_lastMsgSentinel -- companion selector to +0x18
 *          (observed literal values: 0, -1, -2, or a raw data byte in
 *          ReadMIDILocalControlQueue()'s own 5-byte-message case); same
 *          "transcribed as observed, exact enum not confirmed" caveat.
 * `m_special` (+0x8) is deliberately NEVER deleted in the real
 * destructor (confirmed: the dtor's own field-walk skips +0x8 entirely)
 * -- a real, intentional-looking leak, not a modeling gap.
 */
class CSKMIDIMsgProcessor {
public:
	CSKMIDIPortMsgHandler *m_port;				/* +0x00 */
	CSKMIDILocalCtrlMsgHandler *m_localCtrl;		/* +0x04 */
	CSKSpecialMsgHandler *m_special;			/* +0x08 */
	CSKMIDIKarmaCtrlMsgHandler *m_karmaCtrl;		/* +0x0c */
	CSKPadNoteByMIDIPortMsgHandler *m_padByPort;		/* +0x10 */
	CSKPadNoteByLocalCtrlMsgHandler *m_padByLocal;		/* +0x14 */
	int m_lastMsgKind;					/* +0x18 */
	unsigned char *m_activeRawEvent;			/* +0x1c */
	int m_lastMsgSentinel;					/* +0x20 */

	static unsigned char *ms_poInstance;

	/* .text+0x340e70, 217 bytes. Also placement-constructs
	 * CMIDIFlowParamHolder's own singleton (own ctor sets its static
	 * ms_poThis, no local storage needed here) and calls
	 * SKSTGGate_ResistReadQueus(). */
	CSKMIDIMsgProcessor();
	/* .text+0x340f50, 117 bytes. Deletes CMIDIFlowParamHolder's
	 * singleton, then m_padByLocal/m_padByPort/m_karmaCtrl/m_localCtrl/
	 * m_port (in that order) through their own virtual destructors --
	 * NOT m_special, see class comment above. */
	~CSKMIDIMsgProcessor();

	/* .text+0x340fd0, 209 bytes. */
	void ReadMIDILocalControlQueue();
	/* .text+0x3410c0, 184 bytes. */
	void Process();
	/* .text+0x341180, 63 bytes. */
	void ReadMIDIPads();
	/* .text+0x3411c0, 100 bytes. */
	void ReadMIDIPortQueue();
	/* .text+0x341230, 34 bytes. */
	void ProcessMessageForDebug(unsigned char *buf, int len);
	/* .text+0x341260, 44 bytes. */
	void InitializeExtNoteOnChecker();
	/* .text+0x341290, 233 bytes. */
	void TrunAllNotesFromKeyboardOff();
	/* .text+0x341380, 93 bytes. */
	bool IsKeyboardAllOff();
	/* .text+0x3413f0, 21 bytes. */
	void LeaveDownloadMode();
	/* .text+0x341410, 18 bytes. */
	bool IsDamperOn();
	/* .text+0x341430, 89 bytes, regparm(3). Sets m_localCtrl's own raw
	 * event bytes then calls m_localCtrl->Process(). */
	void ProcessLocalControlChannelMessage(int status, unsigned char channel, char data1, char data2);
	/* .text+0x341490, 80 bytes, regparm(3). Same shape via m_port. */
	void ProcessMIDIPortChannelMessage(int status, unsigned char channel, char data1, char data2);
	/* .text+0x3414e0, 89 bytes, regparm(3). Same shape via m_karmaCtrl. */
	void ProcessKarmaControllerGeneratedChannelMessage(int status, unsigned char channel, char data1, char data2);
	/* .text+0x341540, 89 bytes, regparm(3). Same shape via m_padByLocal. */
	void ProcessPadNoteByLocalControlMessage(int status, unsigned char channel, char data1, char data2);
	/* .text+0x3415a0, 82 bytes, regparm(3). Same shape via m_padByPort. */
	void ProcessPadNoteByMIDIPortMessage(int status, unsigned char channel, char data1, char data2);
	/* .text+0x341600, 83 bytes. */
	void KillAllDyingNotes();
	/* .text+0x341660, 51 bytes, regparm(3). */
	void StoreDyingNoteInfoForSTG(CMIDIMessage *msg);
	/* .text+0x3416a0, 51 bytes, regparm(3). */
	void StoreDyingNoteInfoForMIDPort(CMIDIMessage *msg);
	/* .text+0x3416e0, 35 bytes, regparm(3). Real body: always returns
	 * true; *out is only actually written when m_activeRawEvent is
	 * non-null AND its status nibble is 0x80 (NoteOff). */
	bool GetNowProcessingNoteOffVelocity(int *out);
	/* .text+0x341710, 38 bytes. */
	void ResetNotesAfterStopSequencer();
};

/*
 * === CKGMIDIOutMsgHandler family (dependency of CKGMIDIMsgProcessor) ===
 * Discovered while reconstructing CKGMIDIMsgProcessor's own real 705-byte
 * ctor (it placement-constructs 4 distinct sub-handler objects, each of a
 * matching real type). Real inheritance/vtable confirmed via `objdump -r`
 * against `.rodata._ZTV*`:
 *   CKGMIDIOutMsgHandler            : public CSKMIDIMsgHandler  (adds 14
 *     new virtual slots at rodata 0x44-0x78)
 *   CKGMIDIKarmaGeneratedMsgHandler : public CKGMIDIOutMsgHandler
 *   CKGMIDITimbreThruMsgHandler     : public CKGMIDIOutMsgHandler
 *   CKGMIDIKarmaResetCCMsgHandler   : public CKGMIDIOutMsgHandler
 *   CKGBendRangeHandler             : public CSKMIDIMsgHandler DIRECTLY
 *     (own single new slot at rodata 0x44 only -- NOT a
 *     CKGMIDIOutMsgHandler child despite the similar role, confirmed by
 *     its own vtable being 16 slots total vs the other three's 30).
 *
 * CKGMIDIOutMsgHandler's own real ctor (565 bytes) stays genuinely out of
 * scope (it also calls CDyingNoteInfo::Initialize() 32x for the primary/
 * backup arrays below, not reproduced here -- this project's own ctor
 * stays deliberately minimal, matching the established convention that
 * fields are zero-initialized only where a reconstructed method actually
 * relies on it, see m_noteTransposeCache below). Every genuinely
 * out-of-scope slot gets a minimal INLINE, in-class stub body --
 * deliberately keeping it out of the manifest's name-based heuristic
 * (`ClassName::Method(...) {` out-of-line text is what that heuristic
 * matches; an in-class inline body never produces that shape, same
 * technique documented in ckg_control_ui_msg_family.md's manifest-
 * generator-gotcha section). Do not move these out-of-line.
 *
 * KillAllDyingNotes() (621 bytes) and SendExecToMIDIPortInCombi()/InSong()
 * (701/368 bytes) -- the class's own deepest real bodies -- ARE now
 * modeled (KillAllDyingNotes in an earlier batch; Combi/Song here).
 *
 * Process() (rodata 0x44) DOES get a real out-of-line body -- it's the
 * actual function invoked by 3 of CKGMIDIMsgProcessor's own methods via
 * `m_karmaGen/m_timbreThru/m_karmaResetCC->Process()`, and turned out
 * fully self-contained (only calls its own already-declared sibling
 * virtuals). `.text+0x3bb6a0`, 117 bytes.
 */
class CKGMIDIOutMsgHandler : public CSKMIDIMsgHandler {
public:
	/*
	 * Real ground-truth offset +0xc..+0x108c: 16 "primary" CDyingNoteInfo
	 * slots (indexed by MIDI channel, +0xc) immediately followed by 16
	 * "backup" slots (+0x84c) -- confirmed by CheckDyingNoteForMIDIPort()'s
	 * own index math (NoteOn always touches the backup slot; NoteOff reads
	 * the primary slot first, falling back to the backup) and by
	 * KillAllDyingNotes()'s own restore-then-reinit sweep over both
	 * arrays. This project's own CSKMIDIMsgHandler base models 4 bytes
	 * fewer than ground truth's own +0xc alignment (same already-known,
	 * non-blocking gap as CSKMIDIInMsgHandler's own m_noteDownCount
	 * comment) -- harmless here since every access below is symbolic
	 * (via these members), never through a raw absolute offset that
	 * would need to match ground truth's own byte address.
	 */
	CDyingNoteInfo m_dyingNoteInfo[16];
	CDyingNoteInfo m_dyingNoteInfoBackup[16];

	/* Real ground-truth offset +0x1090 (unit-index 0x424, right after
	 * m_dyingNoteInfoBackup's own +0x108c end -- 4 bytes of gap/unknown
	 * field in between not modeled, harmless for the same reason the
	 * +0xc/+0x84c gap above is). A per-(origNote,timbre) transpose cache,
	 * confirmed two ways: (1) SendExecToMIDIPortInCombi()'s own real
	 * `i + note*16 + 0x424` unit-index arithmetic at both its write site
	 * (NoteOn stores GetTimbreTranspose(i) here right before a successful
	 * send) and its read site (NoteOff reads THIS cache instead of calling
	 * GetTimbreTranspose(i) again, so the note it turns off matches
	 * whatever was actually turned on even if the timbre's transpose
	 * setting changed in between); (2) the real ctor's own 128-iteration
	 * zero-init loop at `.text+0x3bc255`..`0x3bc30c` (`mov eax,0x80` down
	 * to 0, zeroing 16 DWORDs -- one full timbre row -- per iteration
	 * starting at ebx+0x1090), independently confirming both the exact
	 * offset and the [128][16] shape. Zero-initialized here via the ctor's
	 * mem-initializer to match; the rest of the ctor stays deliberately
	 * minimal (see below). */
	int m_noteTransposeCache[128][16];

	CKGMIDIOutMsgHandler() : m_noteTransposeCache() {}

	void Process();

	/* Declared in the EXACT real vtable order (rodata 0x48-0x78), since
	 * slot ORDER (not name) is what keeps the leaves' own genuinely-
	 * reconstructed overrides dispatching correctly. All 13 now have real
	 * bodies (out-of-line, src/engine/ckg_midi_msg_handler.cpp).
	 * SendExecToMIDIPortInCombi()/InSong() were the last 2, reconstructed
	 * from previously-captured full disassembly (.text+0x3bbdf0/
	 * 0x3bbc80) -- see re-decompiler agent memory for the derivation. */
	virtual void SendChannelMessageToMIDIPort();
	virtual bool ShouldSendChannelMessageToMIDIPort();
	virtual bool ShouldSendChannelMessageToMIDIPortInEachMode() { return false; }
	virtual bool ShouldSendChannelMessageToSTG();
	virtual bool ShouldRecChannelMessageToSequencer();
	virtual void SendChannelMessageOfActiveTimbreToMIDIPort();
	virtual void SendExecToMIDIPortInProgram();
	virtual void SendExecToMIDIPortInCombi();
	virtual void SendExecToMIDIPortInSong();
	virtual bool CheckZoneOfNoteOn(int hiNote, int loNote, int hiVel, int loVel);
	virtual bool CheckZoneOfNoteOff(int hiNote, int loNote);
	virtual bool CheckDyingNoteForMIDIPort();
	virtual void ProcessForDyingNote();

	/* Non-virtual, called DIRECTLY (not through the vtable, confirmed
	 * via a plain R_386_PC32 relocation) by
	 * CKGMIDIMsgProcessor::KillAllDyingNotes() below. */
	void KillAllDyingNotes();
};

/* .text+0x3bb630 region (ctor 33 bytes; ShouldSendChannelMessageToMIDIPortInEachMode
 * 6 bytes at .text+0x3bb620; CheckDyingNoteForMIDIPort 6 bytes, own COMDAT
 * section). */
class CKGMIDIKarmaGeneratedMsgHandler : public CKGMIDIOutMsgHandler {
public:
	CKGMIDIKarmaGeneratedMsgHandler() {}
	virtual bool ShouldSendChannelMessageToMIDIPortInEachMode() { return true; }
	virtual bool CheckDyingNoteForMIDIPort() { return true; }
};

/* .text+0x3bc5d0 region (ctor 33 bytes; ShouldSendChannelMessageToMIDIPortInEachMode
 * .text+0x3bc5a0, 38 bytes). */
class CKGMIDITimbreThruMsgHandler : public CKGMIDIOutMsgHandler {
public:
	CKGMIDITimbreThruMsgHandler() {}
	virtual bool ShouldSendChannelMessageToMIDIPortInEachMode()
	{
		CMIDIFlowParamHolder *mf = (CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis;
		if (!mf->GetVoiceMode())
			return false;
		return mf->IsKARMAOn();
	}
};

/* .text+0x3bb670 region (ctor 33 bytes; ShouldSendChannelMessageToSTG
 * .text+0x3bb660, 3 bytes). */
class CKGMIDIKarmaResetCCMsgHandler : public CKGMIDIOutMsgHandler {
public:
	CKGMIDIKarmaResetCCMsgHandler() {}
	virtual bool ShouldSendChannelMessageToSTG() { return false; }
};

/* .text+0x3baf60, 33 bytes (ctor); .text+0x3baf50, 15 bytes (Process,
 * calls the already-real base SendChannelMessageToSTGWithCorrectLength()
 * -- rodata slot 0xc, confirmed via `call *0x4(vptr)`). */
class CKGBendRangeHandler : public CSKMIDIMsgHandler {
public:
	CKGBendRangeHandler() {}
	void Process() { SendChannelMessageToSTGWithCorrectLength(); }
};

/*
 * CKGCCResetHandler -- per-channel (0-15) KARMA-generated-CC tracker,
 * owned 16x by CKGMIDIMsgProcessor below. Own root class, no base (own
 * 12-slot vtable, `.rodata._ZTV17CKGCCResetHandler`). Only the ctor and
 * Initialize() -- both directly exercised by CKGMIDIMsgProcessor's own
 * real ctor -- are reconstructed here; the other 10 virtuals (e.g.
 * HandleNRPNMessage's own 381 bytes) stay out-of-scope inline stubs, a
 * separate not-yet-attempted class of their own.
 */
class CKGCCResetHandler {
public:
	/* AllocAligned(0xe0,0x10) sizes the WHOLE object including the
	 * compiler-inserted 4-byte vptr -- this buffer covers the
	 * remaining 0xe0-4 bytes, so real ground-truth offset X (X>=4,
	 * true for every field touched below) maps to m_raw[X-4]. Real
	 * field layout beyond what the ctor/Initialize() touch is out of
	 * scope. */
	unsigned char m_raw[0xe0 - 4];

	/* .text+0x3bb610, 13 bytes. Stores `index` at ground-truth +0xdc;
	 * rest of the real layout not modeled. */
	CKGCCResetHandler(int index) { *(int *)(m_raw + (0xdc - 4)) = index; }

	/* .text+0x3baf90, 76 bytes. */
	virtual void Initialize();

	/* Real vtable order (rodata 0xc-0x34), all 10 now real bodies
	 * (src/engine/ckg_midi_msg_handler.cpp). ConvertToneModifyToCC/
	 * ProcessNRPN/ProcessNRPNIncDec/AdjustNRPN return `int`, NOT `void`
	 * as the prior stub declared -- confirmed by their own callers
	 * checking/using the EAX return value (e.g. ProcessNRPN's `cmp
	 * eax,0xff` against ConvertToneModifyToCC's result). */
	virtual void InitializeControllerMembers();
	virtual void InitializeValue();
	virtual void StoreValue(CMIDIMessage *msg);
	virtual void ResetKarmaGeneratedValue();
	virtual void HandleccidResetAllController();
	virtual void HandleNRPNMessage();
	virtual int ProcessNRPN(int val);
	virtual int ProcessNRPNIncDec(int delta);
	virtual int ConvertToneModifyToCC(int val);
	virtual int AdjustNRPN(int which, int val);
	virtual void SendResetValue(int ccIndex);
};

/*
 * === CKGMIDIMsgProcessor ===
 * KARMA-generated-CC-value tracker + dispatcher -- a DIFFERENT class from
 * CSKMIDIMsgProcessor above (own singleton, confirmed via a distinct
 * relocation `_ZN19CKGMIDIMsgProcessor13ms_poInstanceE`). Previously a
 * 3-method opaque stand-in in oa_ckg_control_ui_msg.h
 * (ResetKarmaGeneratedCCValue/KillAllDyingNotes/StoreCCMessage, already
 * reached through ms_poInstance by real, already-committed callers) --
 * now the full 13/13 real class, reached once its 6 dependency classes
 * above became real types themselves.
 *
 * Real field layout (from the 705-byte ctor's own AllocAligned()+
 * placement-ctor sequence, objdump -r):
 *   +0x00 CKGMIDIKarmaGeneratedMsgHandler *m_karmaGen    Alloc(0x3090,0x10)
 *   +0x04 CKGMIDITimbreThruMsgHandler *m_timbreThru      Alloc(0x3090,0x10)
 *   +0x08 CKGMIDIKarmaResetCCMsgHandler *m_karmaResetCC  Alloc(0x3090,0x10)
 *   +0x0c CKGBendRangeHandler *m_bendRange               Alloc(0xc,0x10)
 *   +0x10..+0x4c CKGCCResetHandler *m_ccReset[16]        Alloc(0xe0,0x10)
 *     each, CKGCCResetHandler(i) i=0..15, immediately followed by a real
 *     vtable-slot-0 call (->Initialize()) right after construction.
 *   +0x50 unsigned char m_bSuspended  -- every Process*ChannelMessage()
 *     below is a full no-op while this is nonzero; ctor sets it to 0
 *     (enabled). No reconstructed method here ever sets it nonzero --
 *     whatever toggles it back on is out of scope.
 * Real dtor (.text+0x3baa10, 11 bytes) only clears ms_poInstance -- NONE
 * of the 20 owned sub-objects are ever deleted (confirmed: no further
 * instructions at all beyond the store+ret). A real, deliberate-looking
 * leak, same idiom as CSKMIDIMsgHandler::m_special elsewhere in this
 * file -- transcribed as-is, not "fixed".
 */
class CKGMIDIMsgProcessor {
public:
	CKGMIDIKarmaGeneratedMsgHandler *m_karmaGen;		/* +0x00 */
	CKGMIDITimbreThruMsgHandler *m_timbreThru;		/* +0x04 */
	CKGMIDIKarmaResetCCMsgHandler *m_karmaResetCC;		/* +0x08 */
	CKGBendRangeHandler *m_bendRange;			/* +0x0c */
	CKGCCResetHandler *m_ccReset[16];			/* +0x10..+0x4c */
	unsigned char m_bSuspended;				/* +0x50 */

	static unsigned char *ms_poInstance;

	/* .text+0x3ba740, 705 bytes. */
	CKGMIDIMsgProcessor();
	/* .text+0x3baa10, 11 bytes -- only clears ms_poInstance, see class
	 * comment above. */
	~CKGMIDIMsgProcessor() { ms_poInstance = 0; }

	/* .text+0x3baa20, 207 bytes, regparm(3). */
	void ProcessKarmaGeneratedChannelMessage(int statusType, unsigned char channel,
						  char data1, char data2, bool changeSource);
	/* .text+0x3bab00, 138 bytes, regparm(3). */
	void ProcessKarmaResetCCChannelMessage(int statusType, unsigned char channel,
						char data1, char data2);
	/* .text+0x3bab90, 220 bytes, regparm(3). */
	void ProcessTimbreThruChannelMessage(int statusType, unsigned char channel,
					      char data1, char data2, bool changeSource);
	/* .text+0x3bac80, 214 bytes, regparm(3). Reuses m_timbreThru (same
	 * sub-object as ProcessTimbreThruChannelMessage above), tagged
	 * with a fixed low-flags-nibble of 1 instead of that method's own
	 * CSKMIDIMsgProcessor::ms_poInstance[+0x18]-derived value --
	 * confirmed via disasm, not assumed from the name. */
	void ProcessResetControllerChannelMessage(int statusType, unsigned char channel,
						   char data1, char data2, bool changeSource);
	/* .text+0x3bad60, 67 bytes, regparm(3). No IsKarmaOn()/changeSource
	 * gating at all (unlike the 3 above) -- unconditionally sets
	 * m_bendRange's flags bit 0x10 and calls Process(). Real quirk:
	 * m_status is stored as (channel-0x20), NOT channel directly --
	 * confirmed via `lea edx,[edx-0x20]` on the raw register, not a
	 * transcription guess. */
	void ProcessKarmaGeneratedBendRangeChannelMessage(unsigned char channel, char data1);
	/* .text+0x3badb0, 25 bytes, regparm(3). msg's own raw byte 0's low
	 * nibble selects which of the 16 m_ccReset[] to forward to. */
	void StoreCCMessage(CMIDIMessage *msg);
	/* .text+0x3badd0, 104 bytes. For every MIDI channel 0-15 that is
	 * currently some KARMA module's real output channel (per
	 * CKGEngine::GetRealOutputChannel()), calls that channel's own
	 * m_ccReset[]->ResetKarmaGeneratedValue(). */
	void ResetKarmaGeneratedCCValue();
	/* .text+0x3bae40, 19 bytes. Direct 1-channel overload -- no module
	 * search. */
	void ResetKarmaGeneratedCCValue(int channel);
	/* .text+0x3bae60, 150 bytes. Unrolled real body -- all 16
	 * m_ccReset[]->InitializeValue() calls, transcribed as a loop
	 * (identical observable effect). */
	void InitializeCCValue();
	/* .text+0x3baf00, 19 bytes. */
	void InitializeCCValue(int channel);
	/* .text+0x3baf20, 37 bytes. Forces m_timbreThru's raw event to a
	 * fixed NoteOff-shaped quad THEN calls its own (non-virtual)
	 * CKGMIDIOutMsgHandler::KillAllDyingNotes() -- not a vtable call,
	 * confirmed via a direct R_386_PC32 relocation. */
	void KillAllDyingNotes();
};

#endif /* OA_CKG_MIDI_MSG_HANDLER_H */
