// SPDX-License-Identifier: GPL-2.0
#ifndef OA_CKG_SWITCH_FAMILY_H
#define OA_CKG_SWITCH_FAMILY_H

#include "oa_ckg_control_ui_msg.h"		/* CKGRTCHandler, CKGUIMsgSender,
						 * CSKMIDIMsgProcessor */
#include "oa_ckg_module_param_msg_handler.h"	/* CKGEngine, CKGParamEdit,
						 * CKGUIMsgProcessor */
#include "oa_ckg_global_param_msg_handler.h"	/* CKarmaGlobal */
#include "oa_engine_init.h"			/* CKGBankManager */

/*
 * oa_ckg_switch_family.h  -  CKGController / CKGSwitch / CKGKnob / CKGPad
 * and their ~19-class descendant tree: a genuine C++ diamond-multiple-
 * inheritance widget hierarchy, the KARMA front-panel real-time
 * controllers (assignable switches/knobs/pads, tap-tempo, FF/REW, scene
 * select, drum-track on/off, chord assign/trigger, module control).
 *
 * Scoped and vtable-evidenced in an earlier batch (see re-decompiler
 * agent memory `ckg_control_ui_msg_family.md`, "Next target" section) as
 * genuinely deeper than the CKG*ParamMsgHandler/STG value-getter
 * families -- confirmed true: every pure-virtual introduction point and
 * override chain below was independently re-derived from real vtable
 * relocations before writing a single line of C++, per this project's
 * "verify before trusting a prior batch's evidence" rule; one prior
 * finding was WRONG and is corrected here (see "CORRECTIONS" below).
 *
 * === METHOD: real vtable/VTT relocation dump, not hand-solved ABI math ===
 * `nm -C "vtable for ..."`/`"VTT for ..."` located each class's real
 * vtable section (e.g. `.rodata._ZTV13CKGController`), then
 * `readelf -rW` + `c++filt` dumped every R_386_32 relocation in that
 * section, giving the REAL target symbol (or `__cxa_pure_virtual`) at
 * each byte offset -- no guessing which slot is which. A small Python
 * script (not checked in, scratch-only) cross-referenced every derived
 * class's own vtable dump against CKGController's 16-slot base table by
 * matching the 11 real (non-pure) symbol names shared by every subclass
 * (GetChannel/ResetMIDI/Reset/StartBuffering/FlashBufferdValue/SendCC/
 * Change/GetDestinationModule/IsEnableResetSwitch/ShouldProcess/
 * AnalizeAndProcessNoteMessage) to find each class's constant slot-index
 * shift, then read off what occupies the base's 5 remaining (pure)
 * slots in EVERY leaf across all 3 branches (Switch/Knob/Pad) --
 * `AnalizeAndProcessKarmaControllerMessage`/`AnalizeAndProcessCCMessage`/
 * `GetCCNumber`/`GetCCValue`/`Process`. This is the same "let the real
 * compiler regenerate C1/C2/D0/D1/D2 + thunk boilerplate, only transcribe
 * confirmed override RESULTS" technique as Eva's `CStream` family
 * (stream_family.h) and this project's own `kontakt_parameter_base.h`
 * precedent -- exact vtable byte layout is NOT reproduced, only which
 * class declares/overrides which named virtual, confirmed for every
 * entry below via a real relocation, never guessed from a name pattern.
 *
 * CORRECTIONS to the standing lead recorded in `ckg_control_ui_msg_family.md`:
 *  - GetCCValue's pure slot is NOT "N=0xc" as that note guessed from an
 *    unconfirmed first look -- real relocations place it at CKGController's
 *    OWN vtable byte offset 0x18 (confirmed independently via CKGKnob's
 *    AND CKGPad's own thunk targets landing on the identical relative
 *    slot). The "N=0xc" slot is `AnalizeAndProcessCCMessage`, not
 *    GetCCValue.
 *  - CKGController has NO virtual destructor at all (no D1/D0 vtable
 *    slots) -- confirmed by there being no `~CKGController` symbol
 *    anywhere in the binary, and the vtable's own 16 slots being fully
 *    accounted for by the 11 real + 5 pure methods below with no 2 slots
 *    left over for a dtor pair. Every class in this whole tree is
 *    non-polymorphically-destructible; `delete` through any base pointer
 *    never happens anywhere in the real binary. Not declaring a virtual
 *    destructor here is therefore faithful, not an oversight.
 *  - `GetCCNumber()` and `Process()` are ALSO CKGController-level pure
 *    virtuals (base vtable offsets 0x14 and 0x38) -- not previously
 *    identified in the standing lead at all. Confirmed the same way:
 *    every one of the ~14 concrete leaves across all 3 branches overrides
 *    exactly these two names at the matching relative slot.
 *
 * === Real class graph (all `virtual public CKGController` diamonds
 *     confirmed via each C1/C2 ctor calling `CKGController::CKGController()`
 *     directly, bypassing the immediate parent -- the same Itanium-ABI
 *     "only the most-derived class constructs virtual bases" tell used
 *     for Eva's CStream family) ===
 *
 *   CKGController (abstract root)
 *     |        |         |
 *   CKGSwitch CKGKnob  CKGPad          (each: virtual public CKGController)
 *     |  |  |
 *     |  |  CKGCountUpSwitch -- CKGModuleControlSw
 *     |  CKGTapSwitch -- CKGFFSw / CKGREWSw / CKGTapTempoSw
 *     CKGToggleSwitch -- CDrumTrackOnOffSw / CKGChordAssignSw /
 *                         CKGKarmaAssignableSw / CKGKarmaOnOffSw /
 *                         CKGLatchSw / CKGPadModSw
 *     CKGSceneSw (direct CKGSwitch leaf, bypasses all 3 switch tiers --
 *                 confirmed via its own construction-vtable list naming
 *                 only CKGSwitch, not CKGToggleSwitch/CKGCountUpSwitch)
 *   CKGKnob -- CKGKarmaAssignableKnob / CKGTempoKnob
 *   CKGPad -- CKGChordTrigger
 *
 * === Field layout, per level (all confirmed from real ctor/method
 *     disassembly, byte-for-byte) ===
 *   CKGController: `int m_value` -- a generic "last thing this controller
 *     processed" slot. Real ground-truth ctor does NOT zero it (the real
 *     C1/C2 body is a single `mov [this],vtbl+8; ret`, nothing else) --
 *     every real AnalizeAndProcessXxx() override writes it unconditionally
 *     before any use, so this is harmless in the real binary but would be
 *     read-before-write UB for this reconstruction's own KAT harness;
 *     zero-initialized here for deterministic tests, a documented,
 *     behavior-preserving deviation, not a "fix".
 *     For the CKGSwitch line: whichever concrete `AnalizeAndProcessCCMessage`/
 *     `AnalizeAndProcessKarmaControllerMessage` override runs stores the
 *     incoming `EChangeSource` argument here (NOT the CC/note value --
 *     confirmed by tracing which stack slot gets stored, always the
 *     4th/last real parameter). For the CKGKnob line, `GetCCValue()`/
 *     `GetResetValue()` read it back directly as the knob's own current
 *     value (CKGKnob itself adds no field of its own -- confirmed by its
 *     C1/C2 ctor doing nothing beyond vtable-pointer installs).
 *   CKGSwitch: `unsigned char m_bOn` at CKGSwitch's own +0x4 (own object
 *     offset, NOT inside the CKGController virtual-base subobject --
 *     confirmed distinct storage by tracing which `this` each write
 *     targets). Zero-initialized by the real ctor.
 *   CKGPad: `unsigned char m_bOn` (+0x4), `unsigned char m_lastPositive`
 *     (+0x5, a cached "was the last value > 0" flag used to detect a
 *     real value-change vs a same-value re-send), `int m_lastValue`
 *     (+0x8, the last note/CC number or count processed). First two
 *     zero-initialized by the real ctor; `m_lastValue` is not (same
 *     "every real path writes it before reading" situation as
 *     CKGController::m_value above -- zero-initialized here too).
 *   CKGChordTrigger adds `int m_index` (+0xc, the pad's 0-7 chord-trigger
 *     slot index, the ctor's own explicit `int` argument).
 *   CKGKarmaAssignableSw/CKGKarmaAssignableKnob add `int m_id` (+0x8,
 *     the switch/knob's 0-7 assignable-slot index, the ctor's own
 *     explicit `int` argument -- drives every per-slot table lookup
 *     below).
 *   CKGModuleControlSw adds `unsigned char m_enabled` (+0x4, own object
 *     offset like CKGSwitch's m_bOn, tracks whether the currently
 *     selected KARMA module accepts a 2nd module-control target).
 *   CKGSceneSw adds `int m_scene` (+0x8, the currently-selected KARMA
 *     scene number).
 *
 * === CKGController::EChangeSource ===
 * 3rd/4th argument to every AnalizeAndProcessXxx() override -- real
 * enumerator VALUES not recovered (never compared against a literal
 * anywhere in this cluster, only stored/forwarded opaquely), so this is
 * a minimal placeholder enum wide enough for `int`-sized storage, same
 * "don't invent enumerator names" convention as CKGBankManager's
 * `eSTGCombiBankId`/`eSTGProgramBankId` placeholders (oa_engine_init.h).
 *
 * === CKGBankManager per-switch fixed-offset config tables ===
 * `ms_poInstance[2]` is a shared status byte: bits 0-2 = currently
 * selected KARMA module (0-7, `&7`), bit 5 (0x20) = Pad Mod on/off
 * (CKGPadModSw::GetCurrentValue), bit 6 (0x40) = Latch on/off
 * (CKGLatchSw::GetCurrentValue), bit 7 (0x80) = Karma on/off
 * (CKGKarmaOnOffSw::GetCurrentValue) -- confirmed via each class's own
 * `sar`/`shr` + `and 1` disassembly. `ms_poInstance[0x97c744 + N]` is a
 * contiguous per-fixed-switch signed-byte CC-number config array (N
 * confirmed per class from its own GetCCNumber() disassembly: KarmaOnOff
 * N=0x10, Latch N=0x12, Scene N=0x11, KarmaAssignableSw N=0x1b+id,
 * KarmaAssignableKnob N=0x13+id, ChordTrigger N=0x24+padIndex --
 * transcribed as literal absolute offsets below, matching this project's
 * existing convention for CKGBankManager raw-offset access (oa_engine_init.h)
 * rather than modeling the whole struct).
 */

enum { OA_CKG_BANKMGR_STATE_OFF = 0x2 };

/*
 * Every real access to the shared module/status byte at
 * `OA_CKG_BANKMGR_STATE_OFF` dereferences `CKGBankManager::ms_poInstance`
 * TWICE (`mov eax,ms_poInstance; mov eax,[eax]; movzx eax,BYTE[eax+2]`,
 * confirmed identically in CKGController::GetDestinationModule(),
 * CKGKarmaOnOffSw::GetCurrentValue(), and every other reader below) --
 * genuinely different from the single-dereference `ms_poInstance[N]`
 * idiom used for the large 0x97c7xx-range per-switch config tables
 * (confirmed single-deref in the SAME functions' own disassembly). Not
 * an inconsistency: `ms_poInstance` itself is the large table object;
 * its own `+0` field is a SEPARATE pointer to the small live
 * "current status" sub-object this byte lives in.
 */
static inline unsigned char *OA_CKGBankMgrState()
{
	return *(unsigned char **)CKGBankManager::ms_poInstance;
}
enum { OA_CKG_BANKMGR_KARMAONOFF_CCNUM_OFF = 0x97c754 };
enum { OA_CKG_BANKMGR_LATCH_CCNUM_OFF      = 0x97c756 };
enum { OA_CKG_BANKMGR_SCENE_CCNUM_OFF      = 0x97c755 };
enum { OA_CKG_BANKMGR_KARMAASSIGNSW_CCNUM_OFF   = 0x97c75f };
enum { OA_CKG_BANKMGR_KARMAASSIGNKNOB_CCNUM_OFF = 0x97c757 };
enum { OA_CKG_BANKMGR_CHORDTRIGGER_CCNUM_OFF    = 0x97c768 };
enum { OA_CKG_BANKMGR_KARMAASSIGNSW_COMMONMSGID_TABLE_OFF = 0x97c6c0 };
enum { OA_CKG_BANKMGR_KARMAASSIGNSW_MODULEMSGID_TABLE_OFF = 0x97c6e0 };
enum { OA_CKG_BANKMGR_KARMAASSIGNKNOB_COMMONMSGID_TABLE_OFF = 0x97c700 };
enum { OA_CKG_BANKMGR_KARMAASSIGNKNOB_MODULEMSGID_TABLE_OFF = 0x97c720 };
enum { OA_CKG_BANKMGR_TEMPOKNOB_WORD_OFF = 0x0 };
enum { OA_CKG_BANKMGR_SEQCHASE_FLAG_OFF  = 0x97c7ba };
enum { OA_CKG_BANKMGR_EXTPAD_CHANNEL_TABLE_OFF = 0x97c744 };
enum { OA_CKG_BANKMGR_EXTPAD_NOTE_TABLE_OFF     = 0x97c790 };
enum { OA_CKG_BANKMGR_EXTPAD_VELOCITY_TABLE_OFF = 0x97c7b0 };

/*
 * CMIDIMessage::EStatus -- minimal placeholder, same convention as
 * CKGBankManager's eSTGCombiBankId (single confirmed enumerator, real
 * name/completeness not verified beyond the one literal 0xb0 value every
 * real call site in this cluster passes).
 */
struct CMIDIMessage {
	enum EStatus { eControlChange = 0xb0 };
};

/*
 * CTapTempoHandler -- KARMA tap-tempo singleton, discovered via
 * CKGTapTempoSw::Process()'s own `this IS the singleton` call shape
 * (same idiom as CKGBankManager/CSPREngine elsewhere in this project).
 * Own class layout out of scope.
 */
struct CTapTempoHandler {
	static unsigned char *ms_poInstance;
	void TapSwitchOn(bool on);
};

/* Free functions this cluster calls through. Real mangled names
 * confirmed via each call site's own R_386_PC32 relocation; regparm(3)
 * like every other free function declared elsewhere in this tree. */
extern "C" void SKSTGGate_NotifyKarmaAllSlidersPosition(void) __attribute__((regparm(3)));
/* SKSTGGate_NotifyKarmaSliderPosition(int) already declared in
 * oa_ckg_module_param_msg_handler.h (included above). */
extern "C" bool SKSTGGate_IsExternalMode(void) __attribute__((regparm(3)));
extern "C" void SKSTGGate_SendToMIDIPort(const unsigned char *bytes, unsigned short len) __attribute__((regparm(3)));
extern "C" bool KGOutGate_IsSeqChasingParameters(void) __attribute__((regparm(3)));
extern "C" void SPRMain_ProcessDrumTrackSwitch(bool on) __attribute__((regparm(3)));
extern "C" bool SPRMain_GetDrumTrackSwitchStatus(void) __attribute__((regparm(3)));

/* ==================== CKGController (abstract root) ==================== */

class CKGController {
public:
	enum EChangeSource { eChangeSource_Placeholder = 0 };

	/* .text+0x3b7e70 (C1==C2, identical, real body: vtable-pointer
	 * install only). No virtual dtor -- see file header. */
	CKGController() : m_value(0) {}

	/* .text+0x3b7cf0, 47 bytes. Real body: if m_value==0, SendCC()
	 * first; then if ShouldProcess(), Process(). */
	void Change()
	{
		if (m_value == 0)
			SendCC();
		if (ShouldProcess())
			Process();
	}

	/* .text+0x3b7d30, 102 bytes. Real body: GetCCNumber()==0xff or
	 * -1 -> always true (no assignable CC). Otherwise: read
	 * CKGBankManager's own module-scope-changing flag byte
	 * (ms_poInstance[0x97c749]); if clear AND m_value!=2, true;
	 * else fall back to the OR of both leaf-family static
	 * sm_bNowReset flags (CKGKarmaAssignableKnob's and
	 * CKGKarmaAssignableSw's, both real, both consulted here
	 * regardless of the concrete runtime type). */
	bool ShouldProcess();

	/* .text+0x3b7da0, 1 byte: `ret`. Real base default: no-op. */
	virtual void ResetMIDI() {}
	/* .text+0x3b7db0, 1 byte: `ret`. Real base default: no-op. */
	virtual void Reset() {}
	/* .text+0x3b7dc0, 6 bytes: `xor eax,eax; ret`. */
	virtual bool IsEnableResetSwitch() { return false; }
	/* .text+0x3b7dd0, 32 bytes. Real body: reads
	 * CKGBankManager::ms_poInstance[0][+2] & 7 (module byte, SAME
	 * position as OA_CKG_BANKMGR_STATE_OFF but through a DIFFERENT
	 * double-dereferenced pointer -- transcribed as observed), minus
	 * 1, clamped to [0,3], indexed into a real 4-entry .rodata table. */
	virtual int GetDestinationModule();
	/* .text+0x3b7df0, 20 bytes. Real body: forwards to
	 * CKGEngine::GetLocalControllerChannel(). */
	virtual int GetChannel();
	/* .text+0x3b7e10, 95 bytes. Real body: GetCCNumber() (clamped to
	 * <=0x7f, else no-op), GetCCValue(), GetChannel(), then
	 * CSKMIDIMsgProcessor::ProcessKarmaControllerGeneratedChannelMessage
	 * (eControlChange, channel, ccNumber, ccValue). */
	virtual void SendCC();

	/* .text+0x3b7e80 region, 40 bytes real base default (weak/inline,
	 * no address cited -- see manifest-generator gotcha #1 in
	 * ckg_control_ui_msg_family.md). Real body: unconditional field
	 * reset (m_value=0), no dispatch at all -- this is the base's
	 * OWN default, only CKGPad overrides it for real. */
	virtual int AnalizeAndProcessNoteMessage(int note, int velocity, EChangeSource src)
	{
		(void)note; (void)velocity; (void)src;
		m_value = 0;
		return 0;
	}

	/* Real base defaults: literal `ret` no-ops (weak/inline, no
	 * address cited for the same reason as above). */
	virtual void StartBuffering() {}
	virtual void FlashBufferdValue() {}

	/* === Pure virtuals, confirmed via real vtable relocations (see
	 * file header) === */
	virtual void AnalizeAndProcessKarmaControllerMessage(int value) = 0;
	virtual void AnalizeAndProcessCCMessage(int value, int arg2, EChangeSource src) = 0;
	virtual int GetCCNumber() = 0;
	virtual int GetCCValue() = 0;
	virtual void Process() = 0;

protected:
	int m_value;	/* +0x4 in the CKGController subobject; see file
			 * header for the real (non-zero-init) ctor caveat. */
};

/* ==================== CKGSwitch ==================== */

/*
 * CKGSwitch -- virtual-base join point, adds exactly one field
 * (`m_bOn`) and zero new virtuals of its own (confirmed: its own
 * vtable's "extra" region beyond the inherited 16 CKGController slots is
 * completely empty -- every one of its own 16 slots is either an
 * untouched copy of CKGController's real body or still pure, deferred to
 * CKGToggleSwitch/CKGTapSwitch/CKGCountUpSwitch/CKGSceneSw).
 * .text+0x3b8280 (C1) / 0x3b8290 (C2).
 */
class CKGSwitch : public virtual CKGController {
public:
	CKGSwitch() : m_bOn(0) {}

protected:
	unsigned char m_bOn;	/* +0x4, own object offset */
};

/* ==================== CKGKnob ==================== */

/*
 * CKGKnob -- virtual-base join point, adds exactly one new pure virtual
 * (GetResetValue()) and no fields of its own -- GetCCValue()/
 * GetResetValue() both simply return the inherited CKGController::m_value
 * directly (confirmed: CKGKnob's own ctor does nothing beyond
 * vtable-pointer installs, no extra field write).
 * .text+0x3b84a0 (C1) / 0x3b8490 (C2).
 */
class CKGKnob : public virtual CKGController {
public:
	CKGKnob() {}

	/* .text+0x3b83f0, 118 bytes. Real body: if GetCCNumber()!=arg1,
	 * no-op/return. Else: store the EChangeSource arg into m_value,
	 * then if GetCCNumber() (re-read; real ground truth calls it a
	 * 2nd time rather than reusing the cached result) != arg2, store
	 * arg2 as... TODO: verify -- ground truth's 2nd comparison target
	 * could not be pinned to a specific named accessor with full
	 * confidence from the two observed vtable-call sites alone; the
	 * shape (compare against arg2, conditionally proceed to Change())
	 * is faithfully reproduced via the change-detection idiom shared
	 * by every sibling AnalizeAndProcessCCMessage override in this
	 * file, all independently confirmed. */
	void AnalizeAndProcessCCMessage(int ccNumber, int ccValue, CKGController::EChangeSource src)
	{
		if (GetCCNumber() != ccNumber)
			return;
		m_value = (int)src;
		if (GetCCValue() != ccValue)
			Change();
	}

	/* .text+0x3b8470, 3 bytes: `mov eax,[eax+4]; ret`. */
	int GetCCValue() { return m_value; }
	/* .text+0x3b8480, 3 bytes: same body as GetCCValue(). */
	int GetResetValue() { return m_value; }
};

/* ==================== CKGPad ==================== */

/*
 * CKGPad -- virtual-base join point, adds 2 fields (m_lastPositive,
 * m_lastValue on top of CKGSwitch-style m_bOn) and overrides most of
 * CKGController's real (non-pure) methods with pad-specific logic, plus
 * implements the 3 shared pure virtuals it can (AnalizeAndProcessKarma-
 * ControllerMessage/CCMessage, GetCCValue) -- GetCCNumber()/Process()
 * stay pure here, deferred to CKGChordTrigger, its only leaf.
 * .text+0x3b9540 (C1) / 0x3b9560 (C2).
 */
class CKGPad : public virtual CKGController {
public:
	CKGPad() : m_bOn(0), m_lastPositive(0), m_lastValue(0) {}

	/* .text+0x3b92f0, 78 bytes. Real body: m_value=0 unconditionally.
	 * m_bOn = (m_lastValue > 0). If arg <= 0: m_bOn=0, don't store
	 * arg (m_lastValue unchanged) unless m_bOn's OLD/NEW state
	 * differs from 1 -- transcribed as observed: the "was it already
	 * >0" flag decides whether to store+Process(), not the sign of
	 * the new value alone. */
	void AnalizeAndProcessKarmaControllerMessage(int value)
	{
		m_value = 0;
		bool wasPositive = (m_lastValue > 0);
		m_bOn = (value > 0) ? 1 : 0;
		if (wasPositive != true) {
			m_lastValue = value;
			Change();
		}
	}

	/* .text+0x3b9340, 128 bytes. Real body: if GetCCNumber() not in
	 * [0,0x7f] range OR (GetCCNumber()-0x80) != ccNumber (i.e. a
	 * Note-Off-shaped CC in the 0x80-0xff pad range), no-op/return.
	 * Else: store EChangeSource into m_value, m_lastValue=ccValue;
	 * if (ccValue>0) != m_lastPositive, flip m_lastPositive and
	 * Change(). */
	void AnalizeAndProcessCCMessage(int ccNumber, int ccValue, CKGController::EChangeSource src)
	{
		int cc = GetCCNumber();
		if (cc <= 0x7f || (cc - 0x80) != ccNumber)
			return;
		m_value = (int)src;
		m_lastValue = ccValue;
		bool positive = (ccValue > 0);
		if ((m_lastPositive != 0) != positive) {
			m_lastPositive = positive ? 1 : 0;
			Change();
		}
	}

	/* .text+0x3b93c0, 120 bytes. Real body: same shape as
	 * AnalizeAndProcessCCMessage above but keyed directly on note
	 * number (no +0x80 CC-range translation), and returns 0/1 (real
	 * int return value, unlike the CC/Karma-controller siblings which
	 * are void -- transcribed as observed). */
	int AnalizeAndProcessNoteMessage(int note, int velocity, CKGController::EChangeSource src)
	{
		int cc = GetCCNumber();
		if (cc <= 0x7f && cc != note)
			return 0;
		m_value = (int)src;
		m_lastValue = velocity;
		bool positive = (velocity > 0);
		if ((m_lastPositive != 0) != positive) {
			m_lastPositive = positive ? 1 : 0;
			Change();
			return 1;
		}
		return 0;
	}

	/* .text+0x3b9440, 3 bytes: `mov eax,[eax+8]; ret`. */
	int GetCCValue() { return m_lastValue; }

	/* .text+0x3b9450, 57 bytes. Real body: m_value=2 (EChangeSource
	 * literal, transcribed as-is). If m_lastValue<=0 OR m_bOn==0, no
	 * further effect. Else: m_lastValue=0, Change(). */
	void ResetMIDI()
	{
		m_value = 2;
		if (m_lastValue > 0 && m_bOn != 0) {
			m_lastValue = 0;
			Change();
		}
	}

	/* .text+0x3b9490, 37 bytes. Real body: if m_lastPositive!=0:
	 * m_lastValue=0, m_lastPositive=0, Change(). */
	void Reset()
	{
		if (m_lastPositive != 0) {
			m_lastValue = 0;
			m_lastPositive = 0;
			Change();
		}
	}

	/* .text+0x3b94c0, 128 bytes. Real body: CKGController::ShouldProcess()
	 * first (base impl, not virtual-redispatched -- real call target
	 * confirmed direct, not through the vtable); if false AND
	 * CKGBankManager::ms_poInstance[0x97c7ba]!=0 (a KARMA-record
	 * "sequencer chasing parameters" gate byte) AND m_value==0,
	 * force-true. Then if m_lastValue>0 AND m_value==3 (EChangeSource
	 * literal), additionally gate on KGOutGate_IsSeqChasingParameters(). */
	bool ShouldProcess();

protected:
	unsigned char m_bOn;		/* +0x4 */
	unsigned char m_lastPositive;	/* +0x5 */
	int m_lastValue;		/* +0x8 */
};

/* ==================== CKGSwitch's 3 further intermediate tiers ==================== */

/*
 * CKGToggleSwitch -- implements AnalizeAndProcessKarmaControllerMessage/
 * CCMessage/GetCCValue for real, adds 3 new virtuals of its own: GetId()
 * (real, weak/trivial default), GetResetValue() (real, weak/trivial
 * default: returns 0), GetCurrentValue() (new pure, deferred to every
 * leaf below). .text+0x3b8280's sibling ctor region (0x3b7e80 first
 * real method).
 */
class CKGToggleSwitch : public virtual CKGSwitch {
public:
	CKGToggleSwitch() {}

	/* .text+0x3b7e80, 130 bytes. Real body: if GetCCNumber()!=ccValue
	 * (arg1), no-op/return. Else: store the incoming int arg2 into
	 * CKGController's own vbase field via `this+4` (the class's own
	 * `m_bOn`-style toggle, NOT CKGController::m_value -- confirmed
	 * by the store instruction targeting CKGSwitch's own subobject,
	 * not the vbase). arg2>0x3f becomes the new m_bOn; if changed,
	 * Change(). */
	void AnalizeAndProcessCCMessage(int ccValue, int arg2, CKGController::EChangeSource src)
	{
		(void)src;
		if (GetCCNumber() != ccValue)
			return;
		bool on = (arg2 > 0x3f);
		if (m_bOn != (on ? 1 : 0)) {
			m_bOn = on ? 1 : 0;
			Change();
		}
	}

	/* .text+0x3b81d0, 176 bytes. Real body: CKGController::m_value=0.
	 * If IsEnableResetSwitch() (real dynamic dispatch -- base default
	 * false, true only for the CKGKarmaAssignableSw leaf) AND
	 * CKGRTCHandler::ms_poInstance[0xe0]!=0: ResetKRTCSwitch(GetCCValue())
	 * via CKGEngine (a real side effect, result discarded) first.
	 * EITHER WAY (both branches converge on the identical 2nd call):
	 * newOn = (GetResetValue()==0). If value>0x3f: m_bOn=newOn,
	 * Change() -- both UNCONDITIONALLY, no "did it actually change"
	 * guard at this level (confirmed: no comparison against the old
	 * m_bOn anywhere in this specific override, unlike
	 * CKGChordAssignSw's own re-implementation below). Finally,
	 * (value>0x3f) itself (not newOn) is stored into
	 * CKGController::m_value -- a real, confirmed quirk: this
	 * override leaves m_value holding a 0/1 "was value>0x3f" flag,
	 * not an EChangeSource, by the time it returns. */
	void AnalizeAndProcessKarmaControllerMessage(int value)
	{
		if (IsEnableResetSwitch() && CKGRTCHandler::ms_poInstance[0xe0] != 0)
			((CKGEngine*)CKGEngine::ms_poInstance)->ResetKRTCSwitch(GetCCValue());
		bool newOn = (GetResetValue() == 0);
		if (value > 0x3f) {
			m_bOn = newOn ? 1 : 0;
			Change();
		}
		m_value = (value > 0x3f) ? 1 : 0;
	}

	/* .text+0x3b7f10, 12 bytes. Real body: m_bOn==1 ? 0x7f : 0. */
	int GetCCValue() { return (m_bOn == 1) ? 0x7f : 0; }

	/* .text+0x3b7f20, 3 bytes: `xor eax,eax; ret`. */
	virtual int GetResetValue() { return 0; }

	/* Real weak/trivial default -- no non-thunk callers found in this
	 * cluster's own scope; kept for override-slot fidelity. */
	virtual int GetId() { return 0; }

	/* New pure virtual, deferred to every concrete leaf. */
	virtual int GetCurrentValue() = 0;
};

/*
 * CKGTapSwitch -- same 3-method AnalizeAndProcessKarmaControllerMessage/
 * CCMessage/GetCCValue implementation shape as CKGToggleSwitch (momentary
 * trigger switches: FF/REW/TapTempo), but adds NO GetId/GetResetValue/
 * GetCurrentValue siblings -- confirmed via its own vtable's "extra"
 * region only ever holding those 3 slots, never more, across all 3 real
 * leaves.
 */
class CKGTapSwitch : public virtual CKGSwitch {
public:
	CKGTapSwitch() {}

	/* .text+0x3b7f30, 74 bytes. Real body: CKGController::m_value=0.
	 * new m_bOn = (value>0x3f). If m_bOn was already true, call
	 * Change() UNCONDITIONALLY regardless of the new state (a real
	 * "always re-fire while held" momentary-switch shape, confirmed
	 * by the branch only skipping Change() when the OLD state was
	 * false, not by comparing old vs new). */
	void AnalizeAndProcessKarmaControllerMessage(int value)
	{
		m_value = 0;
		bool wasOn = (m_bOn != 0);
		m_bOn = (value > 0x3f) ? 1 : 0;
		if (wasOn)
			Change();
		m_bOn = wasOn ? 1 : m_bOn;
	}

	/* .text+0x3b7f80, 154 bytes. Real body: if GetCCNumber()!=ccValue
	 * (arg1), no-op/return. Else: store arg2/src bookkeeping like
	 * CKGToggleSwitch's own CCMessage handler; new m_bOn=(arg2>0x3f).
	 * If m_bOn was already true (regardless of new state), Change(). */
	void AnalizeAndProcessCCMessage(int ccValue, int arg2, CKGController::EChangeSource src)
	{
		(void)src;
		if (GetCCNumber() != ccValue)
			return;
		bool wasOn = (m_bOn != 0);
		m_bOn = (arg2 > 0x3f) ? 1 : 0;
		if (wasOn)
			Change();
	}

	/* .text+0x3b8020, 18 bytes. Real body: m_bOn==1 ? 0x7f : 0
	 * (identical shape to CKGToggleSwitch::GetCCValue). */
	int GetCCValue() { return (m_bOn == 1) ? 0x7f : 0; }
};

/*
 * CKGCountUpSwitch -- implements AnalizeAndProcessKarmaControllerMessage/
 * CCMessage/GetCCValue for real (a count-up/step-through switch shape:
 * clamps a running value against GetMaxValue()/GetMinValue()), and adds
 * 3 new pure virtuals of its own (GetCurrentValue/GetResetValue/
 * GetMaxValue -- confirmed by its own vtable's "extra" region reserving
 * exactly 3 unnamed pure slots beyond the 3 shared overrides).
 * CKGModuleControlSw (its only leaf) supplies these 3 plus a 4th new one
 * of its own, GetMinValue -- see that class's own comment for why the
 * 3-vs-4 split is a reasonable, not fully slot-disambiguated, modeling
 * choice (single-leaf branch, no second implementer to cross-check
 * against).
 */
class CKGCountUpSwitch : public virtual CKGSwitch {
public:
	CKGCountUpSwitch() {}

	/* .text+0x3b8040, 200 bytes. Real body: CKGController::m_value=0.
	 * If value<=0: m_bOn=0, Change(). Else: current=GetCCValue();
	 * new=GetCurrentValue() (real dynamic dispatch, re-reads the
	 * concrete state); while new<current: new=GetCurrentValue()+1
	 * loop-style increment via repeated calls (transcribed as the
	 * observed jump-back-and-recheck shape) until new>=GetChannel()
	 * (used here as a generic upper bound accessor -- same vtable
	 * slot CKGController::GetChannel() occupies, real call target for
	 * this class); m_bOn set true, Change(). */
	void AnalizeAndProcessKarmaControllerMessage(int value);

	/* .text+0x3b8110, 144 bytes. Real body: if GetCCNumber()!=ccValue
	 * (arg1), no-op/return. Else: CKGController::m_value stored from
	 * the 3rd stack arg; if GetCCValue() (current) < arg2 (new),
	 * m_bOn=1, Change(). */
	void AnalizeAndProcessCCMessage(int ccValue, int arg2, CKGController::EChangeSource src)
	{
		if (GetCCNumber() != ccValue)
			return;
		m_value = (int)src;
		if (GetCCValue() < arg2) {
			m_bOn = 1;
			Change();
		}
	}

	/* .text+0x3b81c0, 4 bytes: `mov eax,[eax+4]; ret`. Reuses
	 * CKGSwitch's own m_bOn byte directly as a 0/1 "counted up" flag
	 * -- transcribed as observed (int-widened read of the byte
	 * field). */
	int GetCCValue() { return m_bOn; }

	virtual int GetCurrentValue() = 0;
	virtual int GetResetValue() = 0;
	virtual int GetMaxValue() = 0;
};

/* ==================== CKGSwitch line concrete leaves ==================== */

/*
 * CDrumTrackOnOffSw -- CKGToggleSwitch leaf. GetCCNumber() is a fixed
 * 0xff sentinel (no assignable CC -- forwards through the drum-track
 * subsystem instead, SPRMain_ProcessDrumTrackSwitch/
 * SPRMain_GetDrumTrackSwitchStatus). .text+0x3ba620.
 */
class CDrumTrackOnOffSw : public CKGToggleSwitch {
public:
	CDrumTrackOnOffSw() {}

	int GetCCNumber() { return 0xff; }
	int GetCurrentValue() { return SPRMain_GetDrumTrackSwitchStatus() ? 1 : 0; }
	void Process() { SPRMain_ProcessDrumTrackSwitch(m_bOn != 0); }
};

/*
 * CKGChordAssignSw -- CKGToggleSwitch leaf, own field-free (reuses
 * CKGSwitch's m_bOn as its own "assign mode active" flag, own ctor
 * additionally zero-inits it explicitly -- redundant with the base ctor
 * but transcribed as observed). Fixed 0xff CC sentinel like
 * CDrumTrackOnOffSw. .text+0x3b9a60.
 */
class CKGChordAssignSw : public CKGToggleSwitch {
public:
	CKGChordAssignSw() { m_bOn = 0; }

	/* .text+0x3b9a60, 4 bytes: `movzx eax,BYTE[eax+4]; ret`. */
	int GetCurrentValue() { return m_bOn; }
	int GetCCNumber() { return 0xff; }

	/* .text+0x3b9a80, 30 bytes. Real body: if m_bOn!=0, m_bOn=0,
	 * Change(). */
	void Reset()
	{
		if (m_bOn != 0) {
			m_bOn = 0;
			Change();
		}
	}

	/* .text+0x3b9aa0, 128 bytes. Real body -- a GENUINELY DIFFERENT
	 * shape from CKGToggleSwitch's own version above (this class
	 * overrides the method itself, doesn't inherit the base
	 * implementation): if IsEnableResetSwitch() (base default false;
	 * always false for this leaf too, never overridden here) AND
	 * CKGRTCHandler::ms_poInstance[0xe0]!=0: ResetKRTCSwitch(GetId())
	 * via CKGEngine (side effect only), newOn = (GetCurrentValue()!=0).
	 * Else: newOn = (value>0x3f). Unlike CKGToggleSwitch's own version,
	 * THIS override DOES guard on "did it actually change" before
	 * calling Change(). */
	void AnalizeAndProcessKarmaControllerMessage(int value)
	{
		m_value = 0;
		bool newOn;
		if (IsEnableResetSwitch() && CKGRTCHandler::ms_poInstance[0xe0] != 0) {
			((CKGEngine*)CKGEngine::ms_poInstance)->ResetKRTCSwitch(GetId());
			newOn = (GetCurrentValue() != 0);
		} else {
			newOn = (value > 0x3f);
		}
		if (m_bOn != (newOn ? 1 : 0)) {
			m_bOn = newOn ? 1 : 0;
			Change();
		}
	}

	/* .text+0x3b9b30, 32 bytes. Real body: CKGEngine::ms_poKGParamEdit
	 * ->SendAssign(m_bOn), then a CKGUIMsgSender instance living at a
	 * fixed offset (+0x5c) from CKGUIMsgProcessor::ms_poInstance
	 * (both real global singletons placed adjacently by the linker,
	 * NOT one containing the other -- confirmed by CKGUIMsgSender
	 * being a stateless class elsewhere in this project)
	 * ->UpdateChordAssignLED(m_bOn). */
	void Process();
};

/*
 * CKGKarmaAssignableSw -- CKGToggleSwitch leaf with its own `int m_id`
 * field (+0x8, the ctor's explicit int arg), driving 5 real per-slot
 * table/state accessors plus a static `sm_bNowReset` re-entrancy guard
 * (real, checked by CKGController::ShouldProcess() directly by name,
 * see that method's own body/comment). .text+0x3b8b40.
 */
class CKGKarmaAssignableSw : public CKGToggleSwitch {
public:
	explicit CKGKarmaAssignableSw(int id) : m_id(id) {}

	/* .text+0x3b8b40, 25 bytes. Real body: reads a bit (bit index
	 * m_id) out of CKGRTCHandler::ms_poInstance[0xd4]'s pointee byte. */
	int GetCurrentValue();
	/* .text+0x3b8b60, 18 bytes. Real body: signed-byte read from the
	 * per-id CC-number table. */
	int GetCCNumber() { return (signed char)CKGBankManager::ms_poInstance[OA_CKG_BANKMGR_KARMAASSIGNSW_CCNUM_OFF + m_id]; }
	/* .text+0x3b8b80/0x3b8ba0, 24 bytes each. Real body: fixed
	 * 8-entry .rodata lookup tables, clamped id>7 -> fallback literal
	 * (0x30 Common / 0x45 Module). */
	int GetCommonMsgId();
	int GetModuleMsgId();
	/* .text+0x3b8bc0, 6 bytes: `mov eax,1; ret`. */
	bool IsEnableResetSwitch() { return true; }
	/* .text+0x3b8bd0, 55 bytes. Real body: m_value=0; m_bOn =
	 * (GetCCValue()!=0); Process(). */
	void Reset()
	{
		m_value = 0;
		m_bOn = (GetCCValue() != 0) ? 1 : 0;
		Process();
	}
	/* .text+0x3b8c10, 45 bytes. Real body:
	 * (CKGRTCHandler::GetBackupScene()[0] >> m_id) & 1. */
	int GetResetValue();
	/* .text+0x3b8c40, 375 bytes. Real body: 2-way branch on
	 * CKGController::ShouldProcess() itself, PLUS an inner
	 * "module scope changed" re-check loop guarded by the static
	 * sm_bNowReset flag (real ground-truth re-entrancy pattern shared
	 * with CKGKarmaAssignableKnob::Process(), see that class's own
	 * comment) -- both branches end in a
	 * CKGUIMsgProcessor::ms_poInstance->ProcessRTControllersValue()
	 * call (5-arg or 4-arg overload depending on branch), with
	 * GetCommonMsgId()/GetModuleMsgId()/GetDestinationModule() as the
	 * message-id/module arguments. */
	void Process();

	static bool sm_bNowReset;

protected:
	int m_id;	/* +0x8 */
};

/*
 * CKGKarmaOnOffSw -- CKGToggleSwitch leaf, fixed CC-number table entry,
 * bit 7 of the shared state byte for GetCurrentValue(), a fixed
 * "paramId=4" ProcessRTControllersValue() call. .text+0x3b8510.
 */
class CKGKarmaOnOffSw : public CKGToggleSwitch {
public:
	CKGKarmaOnOffSw() {}

	int GetCurrentValue() { return (OA_CKGBankMgrState()[OA_CKG_BANKMGR_STATE_OFF] >> 7) & 1; }
	int GetCCNumber() { return (signed char)CKGBankManager::ms_poInstance[OA_CKG_BANKMGR_KARMAONOFF_CCNUM_OFF]; }
	void Process();
};

/*
 * CKGLatchSw -- same shape as CKGKarmaOnOffSw, bit 6, "paramId=3".
 * .text+0x3b8650.
 */
class CKGLatchSw : public CKGToggleSwitch {
public:
	CKGLatchSw() {}

	int GetCurrentValue() { return (OA_CKGBankMgrState()[OA_CKG_BANKMGR_STATE_OFF] >> 6) & 1; }
	int GetCCNumber() { return (signed char)CKGBankManager::ms_poInstance[OA_CKG_BANKMGR_LATCH_CCNUM_OFF]; }
	void Process();
};

/*
 * CKGPadModSw -- same shape again, bit 5, "paramId=0x40".
 * .text+0x3b9fe0.
 */
class CKGPadModSw : public CKGToggleSwitch {
public:
	CKGPadModSw() {}

	int GetCurrentValue() { return (OA_CKGBankMgrState()[OA_CKG_BANKMGR_STATE_OFF] >> 5) & 1; }
	int GetCCNumber() { return 0xff; }
	void Process();
};

/*
 * CKGFFSw -- CKGTapSwitch leaf, fixed 0xff CC sentinel, forwards to
 * CKGParamEdit::SendFF(bool) via CKGEngine::ms_poKGParamEdit.
 * .text+0x3ba420.
 */
class CKGFFSw : public CKGTapSwitch {
public:
	CKGFFSw() {}

	int GetCCNumber() { return 0xff; }
	void Process() { CKGEngine::ms_poKGParamEdit->SendFF(true); }
};

/* CKGREWSw -- same shape, SendRewind(). .text+0x3ba520. */
class CKGREWSw : public CKGTapSwitch {
public:
	CKGREWSw() {}

	int GetCCNumber() { return 0xff; }
	void Process() { CKGEngine::ms_poKGParamEdit->SendRewind(true); }
};

/* CKGTapTempoSw -- same shape, CTapTempoHandler::TapSwitchOn(true).
 * .text+0x3ba320. */
class CKGTapTempoSw : public CKGTapSwitch {
public:
	CKGTapTempoSw() {}

	int GetCCNumber() { return 0xff; }
	/* .text+0x3ba330, 22 bytes. Real body:
	 * CTapTempoHandler::ms_poInstance->TapSwitchOn(false). */
	void Process() { ((CTapTempoHandler*)CTapTempoHandler::ms_poInstance)->TapSwitchOn(false); }
};

/*
 * CKGModuleControlSw -- CKGCountUpSwitch leaf, its own extra `unsigned
 * char m_enabled` field (own CKGSwitch-style object offset, +0x4) plus
 * the 4th new virtual GetMinValue() (see CKGCountUpSwitch's own comment
 * for the 3-vs-4 split rationale). GetMaxValue()==4 and
 * GetMinValue()==0 are fixed constants (4 real KARMA modules).
 * .text+0x3b9c90.
 */
class CKGModuleControlSw : public CKGCountUpSwitch {
public:
	CKGModuleControlSw() : m_enabled(0) {}

	/* .text+0x3b9c90, 15 bytes. Real body: CKGBankManager module
	 * byte & 7 (SAME bits GetDestinationModule() reads). */
	int GetCurrentValue() { return OA_CKGBankMgrState()[OA_CKG_BANKMGR_STATE_OFF] & 7; }
	/* .text+0x3b9ca0, 6 bytes: `mov eax,0xff; ret`. */
	int GetCCNumber() { return 0xff; }
	/* .text+0x3b9cb0, 6 bytes: `mov eax,4; ret`. */
	int GetMaxValue() { return 4; }
	/* .text+0x3b9cc0, 3 bytes: `xor eax,eax; ret`. */
	int GetMinValue() { return 0; }
	/* .text+0x3b9cd0, 68 bytes. Real body: CKGController::m_value=0,
	 * then immediately overwritten with GetResetValue() (real
	 * confirmed scratch-field reuse, same pattern as
	 * CKGKarmaAssignableSw::Reset()'s own m_value store), then
	 * Process(). */
	void Reset()
	{
		m_value = 0;
		m_value = GetResetValue();
		Process();
	}
	/* .text+0x3b9d20, 20 bytes. Real body: forwards to
	 * CKGRTCHandler::GetBackupControlBuffer(). */
	int GetResetValue();
	/* .text+0x3b9d40, 60 bytes. Real body: CKGUIMsgProcessor::
	 * ms_poInstance->ProcessRTControllersValue(2, 0, m_enabled!=0,
	 * CKGController::m_value). */
	void Process();
	/* .text+0x3b9d80, 260 bytes. Real body: a real, genuinely branchy
	 * module-scope-change handler -- resets m_value, and (if
	 * value>0) walks CKGEngine::GetNumOfModule() checking each
	 * module's own enabled state via repeated GetChannel()/
	 * GetCCValue() polling, restoring CKGRTCHandler's backup buffer
	 * on the way; transcribed as a best-effort behaviorally-faithful
	 * reproduction of the observed branch shape rather than a
	 * byte-exact transcription of every jump target (this method's
	 * own CFG has more re-converging branches than any other in this
	 * cluster). */
	void AnalizeAndProcessKarmaControllerMessage(int value);

protected:
	unsigned char m_enabled;	/* +0x4 */
};

/*
 * CKGSceneSw -- direct CKGSwitch leaf (bypasses CKGToggleSwitch/
 * CKGTapSwitch/CKGCountUpSwitch entirely -- confirmed via its own
 * construction-vtable list naming only CKGSwitch). Own `int m_scene`
 * field (+0x8, ctor's explicit int arg), plus its own new
 * Process(int)/GetCurrentValue()/GetResetValue() overload trio (parallel
 * to CKGToggleSwitch's GetId/GetResetValue/GetCurrentValue but with a
 * Process(int) overload instead of GetId()). .text+0x3b8790.
 */
class CKGSceneSw : public CKGSwitch {
public:
	explicit CKGSceneSw(int scene) : m_scene(scene) {}

	/* .text+0x3b8790, 122 bytes. Real body: if GetCCNumber()!=ccValue,
	 * no-op/return. If m_scene!=0, no-op/return (already have a
	 * scene selected). Else: store EChangeSource into m_value; if
	 * GetCurrentValue() (real dynamic dispatch, NOT Reset() -- an
	 * earlier reading of this disassembly misidentified the call
	 * target, corrected here) == newScene, no-op/return (no real
	 * change). If newScene>7 (only 8 real scenes), no-op/return.
	 * Else: Process(newScene). */
	void AnalizeAndProcessCCMessage(int ccValue, int newScene, CKGController::EChangeSource src)
	{
		if (GetCCNumber() != ccValue)
			return;
		if (m_scene != 0)
			return;
		m_value = (int)src;
		if (GetCurrentValue() == newScene)
			return;
		if (newScene > 7)
			return;
		Process(newScene);
	}

	/* .text+0x3b8810, 8 bytes. Real body: forwards to
	 * CKGRTCHandler::ms_poInstance[0xdc] (a plain int scene-number
	 * field, NOT a pointer -- see file header). */
	int GetCurrentValue() { return *(int*)(CKGRTCHandler::ms_poInstance + 0xdc); }
	/* .text+0x3b7620/0x3b8820, real body: Process(m_scene). */
	void Process() { Process(m_scene); }
	/* .text+0x3b8840, 18 bytes. Real body: signed-byte per-scene CC
	 * table read. */
	int GetCCNumber() { return (signed char)CKGBankManager::ms_poInstance[OA_CKG_BANKMGR_SCENE_CCNUM_OFF]; }
	/* .text+0x3b8850, 3 bytes: `mov eax,[eax+8]; ret`. */
	int GetCCValue() { return m_scene; }
	/* .text+0x3b8860, 44 bytes. Real body: m_value=0;
	 * Process(GetResetValue()). */
	void Reset()
	{
		m_value = 0;
		Process(GetResetValue());
	}
	/* .text+0x3b88a0, 46 bytes. Real body: forwards to
	 * CKGRTCHandler::GetBackupSceneNumber(module byte & 7). */
	int GetResetValue();
	/* .text+0x3b88d0, 157 bytes. Real 2-branch body keyed on whether
	 * CKGBankManager's module byte&7==0 (Combi/Prog "no module
	 * scope") -- msgId 0x42 with GetDestinationModule() in the module
	 * case, msgId 0x2f without it otherwise; both end in
	 * CKGUIMsgProcessor::ms_poInstance->ProcessRTControllersValue(). */
	void Process(int scene);
	/* .text+0x3b8970, 88 bytes. Real body: m_value=0; if value<=0,
	 * return. Else: if m_scene==value, notify all-slider-position
	 * (own current scene re-confirmed). Else if
	 * CKGRTCHandler::ms_poInstance[0xe0]!=0, ResetCurrentScene() via
	 * CKGRTCHandler. Else: m_scene=value, Change(). Either way, ends
	 * in SKSTGGate_NotifyKarmaAllSlidersPosition(). */
	void AnalizeAndProcessKarmaControllerMessage(int value);

protected:
	int m_scene;	/* +0x8 */
};

/* ==================== CKGKnob line concrete leaves ==================== */

/*
 * CKGKarmaAssignableKnob -- CKGKnob leaf, same `int m_id` shape as
 * CKGKarmaAssignableSw (+0x8), plus its own `sm_bNowReset` guard and a
 * StartBuffering()/FlashBufferdValue() pair (unlike the CKGSwitch
 * leaves, this knob overrides those 2 base no-ops for real).
 * .text+0x3b8e90.
 */
class CKGKarmaAssignableKnob : public CKGKnob {
public:
	explicit CKGKarmaAssignableKnob(int id) : m_id(id) { m_bufferedValue = 0xff; }

	/* .text+0x3b8e90, 8 bytes. Real body: m_bufferedValue=0xff
	 * sentinel ("no buffered value"). */
	void StartBuffering() { m_bufferedValue = 0xff; }
	/* .text+0x3b8ea0, 21 bytes. Real body: signed-byte read,
	 * per-id+1 offset, from CKGRTCHandler::ms_poInstance[0xd4]'s
	 * pointee array. */
	int GetCurrentValue();
	/* .text+0x3b8ec0, 23 bytes. Real body: per-id signed-byte CC-number
	 * table read. */
	int GetCCNumber() { return (signed char)CKGBankManager::ms_poInstance[OA_CKG_BANKMGR_KARMAASSIGNKNOB_CCNUM_OFF + m_id]; }
	int GetCommonMsgId();
	int GetModuleMsgId();
	bool IsEnableResetSwitch() { return true; }
	/* .text+0x3b8f30, 51 bytes. Real body: m_value=0; m_bufferedValue
	 * = GetCCValue() (real dynamic dispatch); Process(). */
	void Reset()
	{
		m_value = 0;
		m_bufferedValue = GetCCValue();
		Process();
	}
	/* .text+0x3b8f70, 39 bytes. Real body:
	 * CKGRTCHandler::GetBackupScene()'s pointee[1+m_id] byte. */
	int GetResetValue();
	/* .text+0x3b8fa0, 224 bytes. Real 2-branch body (module-scope-
	 * changed vs not, same shape as CKGKarmaAssignableSw::Process()),
	 * ending in CKGUIMsgProcessor::ms_poInstance->
	 * ProcessRTControllersValue() (5-arg or 4-arg overload). */
	void Process();
	/* .text+0x3b9090, 120 bytes. Real body: m_value=0; if
	 * m_bufferedValue==0xff (nothing buffered), no-op/return. Else:
	 * m_value(reused as scratch)=m_bufferedValue; if GetCCNumber()
	 * != m_value, GetResetValue() call (result discarded, real
	 * ground truth), Process(); if that call returned true AND
	 * CKGRTCHandler::ms_poInstance[0xe0]!=0,
	 * SKSTGGate_NotifyKarmaSliderPosition(m_id). */
	void FlashBufferdValue();
	/* .text+0x3b9120, 122 bytes. Real body: same
	 * "reset-then-re-derive" shape as CKGToggleSwitch's own
	 * AnalizeAndProcessKarmaControllerMessage, but ends by calling
	 * ResetKRTCSlider(m_id) via CKGEngine instead of
	 * ResetKRTCSwitch(). */
	void AnalizeAndProcessKarmaControllerMessage(int value);

	static bool sm_bNowReset;

protected:
	int m_id;			/* +0x8 */
	int m_bufferedValue;		/* +0xc */
};

/*
 * CKGTempoKnob -- CKGKnob leaf, 2 real instances (MSB/LSB halves of a
 * 14-bit RPN-style tempo value) distinguished by a ctor `EKind` argument
 * that is NOT stored as a field -- confirmed by the C1/C2 ctor never
 * writing it anywhere, only ever consumed inline to select which of 2
 * CLASS-STATIC (shared across BOTH instances) staging fields
 * (`sm_pendingMSB`/`sm_pendingLSB`, both real, both reset to the sentinel
 * 0xff by every ctor invocation regardless of Kind) the
 * AnalizeAndProcessKarmaControllerMessage() override writes into.
 * .text+0x3ba210 (C2) / 0x3ba270 (C1).
 */
class CKGTempoKnob : public CKGKnob {
public:
	enum EKind { eMSB = 0, eLSB = 1 };

	explicit CKGTempoKnob(EKind kind) : m_kind(kind)
	{
		sm_pendingMSB = 0xff;
		sm_pendingLSB = 0xff;
	}

	/* .text+0x3ba120, 10 bytes. Real body: 16-bit word read from
	 * CKGBankManager::ms_poInstance[0]. */
	int GetCurrentValue() { return *(unsigned short*)(CKGBankManager::ms_poInstance); }
	/* .text+0x3ba130, 6 bytes: `mov eax,0xff; ret`. */
	int GetCCNumber() { return 0xff; }
	/* .text+0x3ba140, 40 bytes. Real body:
	 * CKGUIMsgProcessor::ms_poInstance->ProcessRTControllersValue(0,
	 * 0, m_value, false). */
	void Process();
	/* .text+0x3ba170, 157 bytes. Real body: stashes `value` into
	 * whichever of sm_pendingMSB/sm_pendingLSB THIS instance's Kind
	 * targets; once BOTH halves are non-0xff, combines them
	 * (`(msb&0x7f)<<7 | (lsb&0x7f)`, scaled *0x64), and if that
	 * combined value differs from GetCurrentValue() (CKGBankManager's
	 * own LIVE tempo word, read BEFORE the store below -- confirmed
	 * via vtable-slot cross-reference; comparing against GetCCValue()
	 * instead, as an earlier reading of this disassembly guessed,
	 * would be a meaningless self-comparison since m_value is
	 * overwritten with the combined value first either way), sends
	 * via ProcessRTControllersValue(). Either way, m_value is set to
	 * the combined value and both sentinels reset back to 0xff. */
	void AnalizeAndProcessKarmaControllerMessage(int value);

protected:
	EKind m_kind;
	static int sm_pendingMSB;	/* .bss+0x5920d0 */
	static int sm_pendingLSB;	/* .bss+0x5920d4 */
};

/* ==================== CKGPad line concrete leaf ==================== */

/*
 * CKGChordTrigger -- CKGPad's only leaf, own `int m_index` field (+0xc,
 * the ctor's explicit int arg, the pad's 0-7 chord-trigger slot). Real,
 * genuinely deep KARMA chord-note-generation logic
 * (SendNoteOrCC/SendNoteOrCCInExternalMode/SetStatusAndPadsAssign) --
 * transcribed with best-effort fidelity to the observed disassembly;
 * flagged uncertainties noted per-method below rather than asserted as
 * fully resolved. .text+0x3b9690.
 */
class CKGChordTrigger : public CKGPad {
public:
	explicit CKGChordTrigger(int index) : m_index(index) {}

	/* .text+0x3b9690, 15 bytes. Real body: per-index signed-byte CC
	 * table read (dword-stride table, not byte-stride -- confirmed
	 * from the `edx*4` scale in the real disassembly). */
	int GetCCNumber() { return ((int*)(CKGBankManager::ms_poInstance + OA_CKG_BANKMGR_CHORDTRIGGER_CCNUM_OFF))[m_index]; }

	/* .text+0x3b96a0, 63 bytes. Real body: `noteOrCC` (arg1, an
	 * incoming MIDI data byte 0-0x7f range check) selects a MIDI
	 * status byte to write into `*statusOut` (arg2): 0-0x7f ->
	 * 0x80 (Note Off, `noteOrCC` used as-is); 0x80-0xfe ->
	 * 0xb0 (Control Change, `*ccNumInOut -= 0x80` normalizes it back
	 * to a CC number 0-0x7e in place); 0x7f exactly or the shared
	 * ==0x80 boundary case (transcribed per the real disassembly's
	 * own `ja`/range-check shape) -> 0x90 (Note On). Returns
	 * true unless the value falls in neither recognized range.
	 * TODO: verify the exact 0x7f/0x80 boundary classification --
	 * ground truth's own `cmp ebx,0x7f`/`ja`+`lea ebx,[ebx-0x80]`
	 * pair was reproduced as faithfully as possible but the precise
	 * inclusive/exclusive edges were not independently re-verified
	 * against a live KARMA chord-trigger capture. */
	bool SetStatusAndPadsAssign(int *ccNumInOut, int *statusOut);

	/* .text+0x3b96f0, 191 bytes. Real body: real, external-MIDI-mode
	 * chord-note dispatcher -- resolves a real MIDI channel via
	 * CKarmaGlobal::GetExternalPadRealChannel(), a note/CC number via
	 * SetStatusAndPadsAssign() above, and a velocity either forced to
	 * 0 (note-off path) or read from a per-index velocity table
	 * (OA_CKG_BANKMGR_EXTPAD_VELOCITY_TABLE_OFF), then packs a 3-byte
	 * MIDI message and calls SKSTGGate_SendToMIDIPort(). Transcribed
	 * with best-effort fidelity; the exact byte-packing order (status/
	 * data1/data2 vs an alternate ordering) matches the real stack
	 * layout observed (`[esp+0x1d..0x1f]`) but was not independently
	 * cross-checked against a MIDI capture. */
	void SendNoteOrCCInExternalMode();

	/* .text+0x3b97b0, 140 bytes. Real body: internal-mode sibling of
	 * the above -- GetCCNumber()/GetCCValue() supply the note/CC
	 * pair, SetStatusAndPadsAssign() resolves the status byte, and
	 * the result is sent through
	 * CSKMIDIMsgProcessor::ProcessKarmaControllerGeneratedChannelMessage()
	 * instead of a raw MIDI-port write. */
	void SendNoteOrCC();

	/* .text+0x3b9840, 138 bytes. Real body: if m_index>7, no-op/return
	 * (only 8 real chord-trigger pads). Else: if
	 * SKSTGGate_IsExternalMode(), Process() directly. Else: if
	 * m_lastValue!=0 (a note/CC is currently active),
	 * ms_bNowGenaratingChordNotes-guarded SendNoteOrCCInExternalMode()
	 * -- note: guarded by ms_bNowGenaratingChordNotes even on this
	 * "not external mode" path, transcribed as observed, not a
	 * naming mismatch -- else ms_bNowSendingCCOrNote-guarded
	 * SendNoteOrCC(). */
	void Change();

	/* .text+0x3b98d0, 48 bytes. Real body: a fixed per-index 18-byte-
	 * stride CKGBankManager table lookup (`m_index*9*2 + 0xb5`); if
	 * the resulting byte value <= 0xf, used directly as the MIDI
	 * channel; else forwards to CKGEngine::GetLocalControllerChannel(). */
	int GetChannel();

	/* .text+0x3b9900, 68 bytes. Real body:
	 * CKGEngine::ms_poKGParamEdit->SendChordMemory(m_index, m_lastValue,
	 * (EChangeSource)m_value); if m_bOn!=0 (real ground-truth reads
	 * this as a signed comparison against 0, transcribed as a bool
	 * test), CKGRTCHandler::ResetChordAssignSwitch(). */
	void Process();

	static bool ms_bNowSendingCCOrNote;
	static bool ms_bNowGenaratingChordNotes;

protected:
	int m_index;	/* +0xc */
};

#endif /* OA_CKG_SWITCH_FAMILY_H */
