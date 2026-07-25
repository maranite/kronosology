// SPDX-License-Identifier: GPL-2.0
/*
 * front_panel_handlers.cpp  -  CSTGFrontPanel::HandleSwitchEvent/
 * HandleTouchPanel/HandleRotary/HandleAnalogController, plus the free
 * function ShortInvertNkS4AnalogValue.
 *
 * These are the real front-panel event dispatch entry points a physical
 * button/touch/rotary/knob press routes through on a real Kronos --
 * `CSTGOmapNKSMsgHandler::ProcessNextNKSEvent` (not reconstructed in
 * this pass) calls them directly off the NKS4 USB event stream. A
 * companion project (KronosScreenRemoteDaemon's nks4_inject.ko) calls
 * these same five symbols by resolved runtime address to inject
 * remote-control events on real hardware -- its own header comment
 * (nks4_inject_module/nks4_inject.c, lines 1-168) independently
 * ground-truths the exact calling convention below via objdump -dr and
 * was cross-checked against this pass's own disassembly; the two agree
 * byte-for-byte.
 *
 * Ground-truthed via `objdump -dr -M intel` against the real
 * OA.ko (MD5 955636c2b11a70a1dbecefaaa7bd4f80), using nm's function
 * addresses (NOT Ghidra's -- see docs/modules/OA.ko.md's "Address
 * mapping" section: Ghidra's COMDAT-stacked addresses differ from nm's
 * for this binary; nm 0xc0f0/0xc1c0/0xc2e0/0xc320/0x2077f0 map to file
 * offset 0xb390+nm via `file_offset = 0xb390 + nm_symbol_value`).
 * Ghidra's own decompile of these five functions (available at
 * /home/share/Decomp/oa_export/functions/Handle*.c and
 * ShortInvertNkS4AnalogValue@002177f0.c for cross-reference) is
 * abbreviated/lossy for HandleSwitchEvent, HandleAnalogController, and
 * HandleRotary (drops the dispatch-table arithmetic and, in
 * HandleRotary's case, drops the `delta` store into the outgoing
 * message entirely) -- this reconstruction is transcribed from the raw
 * disassembly, not the decompiler output, precisely because of that gap.
 *
 * Calling convention (regparm(3), confirmed via the caller-independent
 * `push ebp; mov ebp,esp` prologues + register-argument usage in each
 * function's own body -- consistent with nks4_inject.c's own
 * caller-side trace):
 *   HandleSwitchEvent(eSTGButtonCode, bool)        EAX=this EDX=code   CL=pressed
 *   HandleTouchPanel(eNKS4TouchPanelEventType,int) EAX=this EDX=type   ECX=coord (v_adc|(h_adc<<8))
 *   HandleRotary(int)                              EAX=this EDX=delta
 *   HandleAnalogController(eSTGAnalogDeviceCode,uchar,ushort)
 *                                                   EAX=this EDX=device_code ECX=param2 [stack]=param3
 *   ShortInvertNkS4AnalogValue(uchar,uchar,ushort*,ushort*)
 *                                                   EAX=byte0 EDX=byte1 ECX=&out_hi [stack]=&out_lo
 * All four CSTGFrontPanel methods notably do NOT use `this` for
 * anything except as an implicit argument slot -- confirmed real: `eax`
 * is clobbered by the very first instruction in each (loading
 * `CPowerOffTimer::sInstance`) and never read again. All real state
 * they touch instead comes off `CSTGGlobal::sInstance`/
 * `CSTGControllerRTData::sInstance`/`CSTGMidiPortManager::sInstance`
 * (global singletons), or, for HandleTouchPanel specifically, off `ebx`
 * which is loaded from the ORIGINAL `this` (EAX) before it gets
 * clobbered -- HandleTouchPanel is the one function of the four that
 * genuinely reads/writes its own object's fields (the +0x104/+0x105/
 * +0x106 onscreen-touchpad-mode state `CSTGFrontPanel::Initialize()`,
 * engine_startup_bits.cpp, already establishes).
 *
 * CSTGFrontPanel::HandleSwitchEvent (.text+0xc0f0, 192 bytes... wait,
 * confirmed 0xd0=208 bytes to the next symbol, close enough to the
 * exporter's reported 192 -- likely includes trailing alignment NOPs
 * not part of the exporter's byte count):
 *   Sets CPowerOffTimer::sInstance's flag byte to 1 unconditionally.
 *   If CSTGMidiPortManager::sInstance's flag byte is 0 (MIDI-port-
 *   manager not ready), returns immediately -- no dispatch at all.
 *   Otherwise resolves a target CSTGControllerInfo sub-object (see
 *   ResolveControllerInfoTarget below) and tail-calls
 *   CSTGControllerInfo::ButtonPressHandler(target, code, pressed) --
 *   `code`(EDX)/`pressed`(ECX, zero-extended) pass through UNCHANGED
 *   from HandleSwitchEvent's own entry registers the entire way, since
 *   nothing in the resolution arithmetic touches edx/ecx.
 *
 * CSTGFrontPanel::HandleAnalogController (.text+0xc320, 214 bytes) is
 * structurally IDENTICAL to HandleSwitchEvent -- same CPowerOffTimer
 * set, same CSTGMidiPortManager gate, the exact same
 * ResolveControllerInfoTarget arithmetic (byte-for-byte identical
 * immediates/multipliers), differing only in: `param3`(ushort) arrives
 * on the stack ([ebp+0x8], since regparm(3) is full after
 * this/device_code/param2) and is grabbed into a callee-saved register
 * BEFORE `this`/EAX gets clobbered, then re-pushed onto the outgoing
 * call's own stack slot for CSTGControllerInfo::
 * AnalogControllerHandler(target, deviceCode, param2, param3).
 *
 * ResolveControllerInfoTarget (inlined identically into both of the
 * above, not a separate real symbol): reads `CSTGGlobal::sInstance+
 * 0x684` as a 3-way mode selector (0/1/2), then combines two more
 * CSTGGlobal fields (`+0x68c`/`+0x690` scaled by a large multiplier,
 * `+0x698`/`+0x69c`/`+0x6a0` masked to 7 bits and scaled by a second
 * multiplier) into a byte offset from CSTGGlobal::sInstance, landing on
 * a CSTGControllerInfo sub-object embedded `+0xad3` into a per-mode
 * target object. The three modes' final offsets before the common
 * `+0xad3` are `+3`/`+6`/`+0` -- CONFIRMED (not guessed) to correspond
 * to `CSTGProgram+3`/`CSTGCombi+6`/`CSTGSequence+0` respectively, per
 * oa_global.h's own pre-existing CSTGControllerInfo comment (cross-
 * referencing CSTGPerformance::SetIsDying's identical `+0xad3` target).
 * A fourth path (mode-0's own `CSTGGlobal+0x698 == 0xfffe` sentinel)
 * skips the scaled-index math entirely and uses a single fixed offset
 * (`+0x2976e33`) instead -- presumably a "no active program slot yet"
 * fallback object; not independently identified beyond that it's real
 * and reachable. The specific multipliers (0x67603, 0xcec, 0xcf381,
 * 0x19e7, 0x1cad) and base offsets (0x132e4d0, 0x1c77f10, 0x27cd024)
 * are preserved as literal immediates -- they are almost certainly
 * `trackIndex * sizeof(CSTGProgram-ish-slot) + arrayBase` computations,
 * but the underlying array/struct isn't independently reconstructed in
 * this pass, so no named field/stride is invented for them (matching
 * this project's established "preserve real offset math, don't guess
 * struct fields" convention, e.g. front_panel_smoothers.cpp).
 *
 * CSTGFrontPanel::HandleTouchPanel (.text+0xc1c0, 285 bytes): the one
 * function of the four with real per-instance state and a genuinely
 * branchy body. Gates on `this[0x104]` (onscreen-touchpad-mode flag,
 * CSTGFrontPanel::Initialize() zeroes it) and `event_type`
 * (1=down,2=up,3=drag -- matching nks4_inject.c's TOUCH command docs):
 *   - touchpad mode OFF, or event_type==3 (drag): straight to the
 *     unconditional PushUnsolicitedMessage at the end, no KARMA side
 *     effect.
 *   - touchpad mode ON and event_type==2 (up), OR mode ON and
 *     event_type is neither 1 nor 2: "cleanup" path -- if
 *     `this[0x105]` (active flag) is set, calls
 *     CSTGControllerRTData::sInstance->SendKarmaCCToKG(ccNo =
 *     (signed char)this[0x106] + 0x1c, value = 0) (a KARMA CC
 *     "release"), clears the active flag, falls through to the message.
 *   - touchpad mode ON and event_type==1 (down): if already active
 *     (`this[0x105]` set), just sends the message (no re-trigger).
 *     Otherwise computes `this[0x106] = (int8_t)((coord>>8 & 0xff) -
 *     0xd) * this[0x10c]` (the confirmed 0.03448275849223137f constant
 *     from CSTGFrontPanel::Initialize(), engine_startup_bits.cpp) --
 *     this byte becomes the KARMA CC number's low-order contribution
 *     via `+0x1c` at send time. Range-checks the coord's LOW byte:
 *     `(uint8_t)(coord - 0x40) > 0x80` skips the KARMA send (message
 *     still goes out). If in range, computes `value = 0x7f -
 *     (int16_t)((coord&0xff - 0x40) * this[0x108])` (the OTHER
 *     confirmed Initialize() constant, 0.9921875f = 127/128) and calls
 *     SendKarmaCCToKG(ccNo = (int8_t)this[0x106] + 0x1c, value), then
 *     sets the active flag.
 *   - Unconditional tail: builds a 20-byte PushUnsolicitedMessage
 *     packet `{u16 size=0x14, u16 source=1, u32 reserved=0, u32
 *     subtype=0x11, u32 eventType, u32 coord}` and sends it -- this
 *     `{size,source=1,reserved=0,subtype,payload...}` shape matches
 *     several other confirmed-real PushUnsolicitedMessage packets
 *     already documented elsewhere in this project (oa_global.h's own
 *     "16-byte"/"20-byte"/"24-byte" notes).
 *
 * CSTGFrontPanel::HandleRotary (.text+0xc2e0, 60 bytes): trivial --
 * CPowerOffTimer set, then an UNCONDITIONAL 16-byte PushUnsolicitedMessage
 * packet `{u16 size=0x10, u16 source=1, u32 reserved=0, u32 subtype=0xd,
 * u32 delta}` (same header shape as HandleTouchPanel's, one fewer
 * payload dword). No gating, no dispatch table, no state touched. NOTE:
 * Ghidra's own decompile of this function (oa_export) fails to surface
 * the final `mov [esp+0xc],edx` store of `delta` into the outgoing
 * packet -- confirmed present and load-bearing via raw disassembly; a
 * transcription from the decompiler output alone would have silently
 * dropped the rotary delta.
 *
 * ShortInvertNkS4AnalogValue (.text+0x2077f0, 73 bytes, plain regparm(3)
 * free function, no `this`): converts a raw 2-byte NKS4 analog sample
 * (byte0/byte1) into the (out_hi, out_lo) pair HandleAnalogController's
 * real caller feeds it as (param2, param3). Bit-twiddling confirmed via
 * raw disassembly (matches nks4_inject.c's own independent trace):
 *   val = ((byte0*4) & 0x3f8) | ((byte1>>6) & 3);
 *   *out_lo = val;
 *   if (byte0 & 1) { val |= 4; *out_lo = val; }
 *   *out_hi = val >> 3;
 *   if (*out_lo != 0x200) *out_lo = 0x3ff - *out_lo;
 * (the `!= 0x200` special case is a real, confirmed quirk -- reproduced
 * verbatim, not "cleaned up").
 *
 * ---------------------------------------------------------------------
 * SetLED/SetLEDBlinking/ResetLED, HandleKeyOn/HandleKeyOff (batch-63
 * un-triaged candidates 1-3, added in a later pass; see oa_setup_
 * global_resources.h's own CSTGFrontPanel comment for the confirmed
 * addresses/sizes/shape summary).
 *
 * CSTGFrontPanel::HandleKeyOn (.text+0xbf00, 367 bytes) confirmed shape:
 *   1. CPowerOffTimer::sInstance flag = 1 (unconditional, same as every
 *      other handler in this file).
 *   2. Gate on CSTGMidiPortManager::sInstance's ready flag -- returns
 *      immediately if not ready (no state touched at all, not even the
 *      per-key table).
 *   3. noteRaw = sext8(rtd[0x29] + rtd[0x28] + rtd[0x2a]) + keyNum,
 *      where rtd = CSTGControllerRTData::sInstance and the three summed
 *      bytes are unnamed (own semantic meaning not determined beyond
 *      "some combination of octave-shift/transpose", real field names
 *      not recovered).
 *   4. Range-fold noteRaw into a real 0-127 MIDI note number IF it
 *      overflows above 127 or goes negative -- confirmed via a genuine
 *      div/mod-by-12 reciprocal-multiply sequence in the real
 *      disassembly (preserves the note's pitch class while folding it
 *      into range). This reconstruction computes the SAME final low
 *      BYTE (the only part of the result any real caller downstream
 *      ever reads -- confirmed, since every consumer of the folded
 *      value only ever reads `dl`/`al`) via plain C division/modulo
 *      instead of hand-transcribing the exact x87-free integer
 *      reciprocal-multiply trick the compiler chose -- a confirmed-
 *      honest simplification (same class of simplification as
 *      `CSTGCPUInfo`'s `1.0f/x` for `rcpss`, engine_startup_bits.cpp),
 *      NOT verified against real hardware since a front-panel key
 *      transpose/octave sum extreme enough to hit this fold is a rare
 *      edge case -- see HARDWARE_REVIEW_LOG.md.
 *   5. this[4+keyNum] = noteFinal; this[0x84+keyNum] = channel (from
 *      CSTGGlobal::sInstance+0x6b9, the SAME confirmed real MIDI-channel
 *      field UpdateMIDIChannel writes, oa_global.h) -- a per-key "what
 *      note/channel did we actually send" record, confirmed read back
 *      by HandleKeyOff below.
 *   6. STGAPIFrontPanelStatus::sInstance[STGAPI_OFF_MIDI_ECHO0/1] =
 *      {noteFinal, velocity} -- confirmed: both are the SAME final
 *      (post-fold) values, not the raw pre-fold sum (traced carefully:
 *      the real code's `edi`/`edx` registers get overwritten with the
 *      folded result on every path, including the two fold branches,
 *      before this store).
 *   7. Sends a real 5-byte message `{channel|0x90, noteFinal, velocity,
 *      1, 0xfe}` via CSTGMidiPortManager::sInstance+0x208's embedded
 *      CSTGMidiQueueWriter (the SAME confirmed embedding/idiom
 *      global.cpp's SendGlobalMidiMessage() already uses).
 *
 * CSTGFrontPanel::HandleKeyOff (.text+0xc070, 126 bytes) confirmed
 * shape: same CPowerOffTimer set + CSTGMidiPortManager gate, then reads
 * BACK this[0x84+keyNum]/this[4+keyNum] (the channel/note HandleKeyOn
 * recorded) -- no fold logic, no CSTGGlobal/CSTGControllerRTData
 * touched at all. Sends a real 5-byte Note-Off message
 * `{0x80|storedChannel, storedNote, velocity, 1, 0xfe}` via the SAME
 * CSTGMidiPortManager+0x208 embedded writer. `velocity` (arg2, ECX) is
 * NOT looked up from any table -- passed straight from the caller.
 */

#include "oa_setup_global_resources.h"
#include "oa_keybed_init.h" /* CSTGKeybedInterface_sInstance() */

/* Same per-TU local declaration convention already used by every other
 * PushUnsolicitedMessage caller in this project (global.cpp,
 * slot_voice_data_free.cpp, engine_startup_bits2.cpp,
 * performance_vars_set_is_dying.cpp, load_balancer_static.cpp) -- no
 * shared header declares it. */
extern "C" void PushUnsolicitedMessage(void *msg);

static void *ResolveControllerInfoTarget(unsigned char *global)
{
	unsigned int mode = *(unsigned int *)(global + 0x684);
	unsigned int idx;
	unsigned int scaled;

	if (mode == 1) {
		/* CSTGCombi+6 */
		idx = *(unsigned int *)(global + 0x69c) & 0x7f;
		scaled = *(unsigned int *)(global + 0x690) * 0xcf381u;
		idx = idx * 0x19e7u + scaled + 0x1c77f10u;
		return global + idx + 6;
	}
	if (mode == 2) {
		/* CSTGSequence+0 */
		idx = *(unsigned int *)(global + 0x6a0) * 0x1cadu;
		return global + idx + 0x27cd024u;
	}

	/* mode 0 (CSTGProgram+3), including the 0xfffe sentinel case */
	idx = *(unsigned int *)(global + 0x698);
	if (idx == 0xfffeu)
		return global + 0x2976e33u;

	idx &= 0x7f;
	scaled = *(unsigned int *)(global + 0x68c) * 0x67603u;
	idx = idx * 0xcecu + scaled + 0x132e4d0u;
	return global + idx + 3;
}

void CSTGFrontPanel::HandleSwitchEvent(unsigned int code, bool pressed)
{
	*(unsigned char *)CPowerOffTimer::sInstance = 1;
	if (*(unsigned char *)CSTGMidiPortManager::sInstance == 0)
		return;

	unsigned char *global = (unsigned char *)CSTGGlobal::sInstance;
	void *target = ResolveControllerInfoTarget(global);
	((CSTGControllerInfo *)((unsigned char *)target + 0xad3))
		->ButtonPressHandler(code, pressed);
}

void CSTGFrontPanel::HandleAnalogController(unsigned int deviceCode,
					     unsigned char param2, unsigned short param3)
{
	*(unsigned char *)CPowerOffTimer::sInstance = 1;
	if (*(unsigned char *)CSTGMidiPortManager::sInstance == 0)
		return;

	unsigned char *global = (unsigned char *)CSTGGlobal::sInstance;
	void *target = ResolveControllerInfoTarget(global);
	((CSTGControllerInfo *)((unsigned char *)target + 0xad3))
		->AnalogControllerHandler(deviceCode, param2, param3);
}

void CSTGFrontPanel::HandleRotary(int delta)
{
	*(unsigned char *)CPowerOffTimer::sInstance = 1;

	unsigned char msg[16];
	*(unsigned short *)(msg + 0x0) = 0x10;
	*(unsigned short *)(msg + 0x2) = 1;
	*(unsigned int *)(msg + 0x4) = 0;
	*(unsigned int *)(msg + 0x8) = 0xd;
	*(unsigned int *)(msg + 0xc) = (unsigned int)delta;
	PushUnsolicitedMessage(msg);
}

void CSTGFrontPanel::HandleTouchPanel(unsigned int eventType, int coord)
{
	unsigned char *self = (unsigned char *)this;

	*(unsigned char *)CPowerOffTimer::sInstance = 1;

	if (self[0x104] != 0) {
		if (eventType == 1) {
			if (self[0x105] == 0) {
				int hAdc = (coord >> 8) & 0xff;
				int z = (int)((float)(hAdc - 0xd) * *(float *)(self + 0x10c));
				self[0x106] = (unsigned char)z;

				unsigned int rangeCheck = (unsigned char)((coord & 0xff) - 0x40);
				if (rangeCheck <= 0x80) {
					int vAdc = coord & 0xff;
					int scaled = (int)((float)(vAdc - 0x40) * *(float *)(self + 0x108));
					unsigned char value = (unsigned char)(0x7f - scaled);
					int ccNo = (signed char)self[0x106] + 0x1c;
					CSTGControllerRTData::sInstance->SendKarmaCCToKG(ccNo, value);
					self[0x105] = 1;
				}
			}
			goto push_message;
		}
		if (eventType != 2)
			goto push_message;
	}

	if (self[0x105] != 0) {
		int ccNo = (signed char)self[0x106] + 0x1c;
		CSTGControllerRTData::sInstance->SendKarmaCCToKG(ccNo, 0);
		self[0x105] = 0;
	}

push_message:
	{
		unsigned char msg[20];
		*(unsigned short *)(msg + 0x0) = 0x14;
		*(unsigned short *)(msg + 0x2) = 1;
		*(unsigned int *)(msg + 0x4) = 0;
		*(unsigned int *)(msg + 0x8) = 0x11;
		*(unsigned int *)(msg + 0xc) = eventType;
		*(unsigned int *)(msg + 0x10) = (unsigned int)coord;
		PushUnsolicitedMessage(msg);
	}
}

void ShortInvertNkS4AnalogValue(unsigned char byte0, unsigned char byte1,
				 unsigned short *outHi, unsigned short *outLo)
{
	unsigned short val = (unsigned short)(((byte0 * 4) & 0x3f8) | ((byte1 >> 6) & 3));
	*outLo = val;
	if (byte0 & 1) {
		val = (unsigned short)(val | 4);
		*outLo = val;
	}
	*outHi = (unsigned short)(val >> 3);
	if (*outLo != 0x200)
		*outLo = (unsigned short)(0x3ff - *outLo);
}

static void SendFrontPanelKeyMidiMessage(unsigned char status, unsigned char note,
					  unsigned char velocity)
{
	unsigned char msg[5];
	msg[0] = status;
	msg[1] = note;
	msg[2] = velocity;
	msg[3] = 1;
	msg[4] = 0xfe;

	CSTGMidiQueueWriter *writer =
		(CSTGMidiQueueWriter *)((unsigned char *)CSTGMidiPortManager::sInstance + 0x208);
	writer->Write(msg, 5, false);
}

void CSTGFrontPanel::SetLED(unsigned int code)
{
	if ((code - 0x49u) <= 1u) {
		CSTGKeybedInterface *kb =
			reinterpret_cast<CSTGKeybedInterface *>(CSTGKeybedInterface_sInstance());
		kb->SetLED(code, 1);
		return;
	}
	unsigned int packed = ((code & 0xffu) << 8) | ((code >> 8) & 0xffu);
	OmapNKS4OutputFifo_WriteCommand((int)(packed | 0x1500000u));
}

void CSTGFrontPanel::SetLEDBlinking(unsigned int code)
{
	if ((code - 0x49u) <= 1u) {
		CSTGKeybedInterface *kb =
			reinterpret_cast<CSTGKeybedInterface *>(CSTGKeybedInterface_sInstance());
		kb->SetLED(code, 2);
		return;
	}
	unsigned int packed = ((code & 0xffu) << 8) | ((code >> 8) & 0xffu);
	OmapNKS4OutputFifo_WriteCommand((int)(packed | 0x1510000u));
}

void CSTGFrontPanel::ResetLED(unsigned int code)
{
	if ((code - 0x49u) <= 1u) {
		CSTGKeybedInterface *kb =
			reinterpret_cast<CSTGKeybedInterface *>(CSTGKeybedInterface_sInstance());
		kb->SetLED(code, 0);
		return;
	}
	unsigned int packed = ((code & 0xffu) << 8) | ((code >> 8) & 0xffu);
	OmapNKS4OutputFifo_WriteCommand((int)(packed | 0x1520000u));
}

void CSTGFrontPanel::HandleKeyOn(unsigned char keyNum, unsigned char velocity)
{
	unsigned char *self = (unsigned char *)this;

	*(unsigned char *)CPowerOffTimer::sInstance = 1;
	if (*(unsigned char *)CSTGMidiPortManager::sInstance == 0)
		return;

	unsigned char *global = (unsigned char *)CSTGGlobal::sInstance;
	unsigned char *rtd = (unsigned char *)CSTGControllerRTData::sInstance;
	unsigned char channel = global[0x6b9];

	/* rtd[0x28]/[0x29]/[0x2a]: three unnamed CSTGControllerRTData bytes,
	 * real semantics not determined -- see this file's own header
	 * comment. */
	signed char transposeSum = (signed char)(rtd[0x29] + rtd[0x28] + rtd[0x2a]);
	int noteRaw = (int)(short)transposeSum + (int)keyNum;
	unsigned char noteFinal;

	if (noteRaw > 0x7f) {
		/* Fold down into the top octave [0x74..0x7f], preserving the
		 * note's pitch class -- see header comment re: div/mod
		 * simplification. */
		int v = noteRaw - 8;
		int q = v / 12;
		noteFinal = (unsigned char)((v - q * 12) + 0x74);
	} else if (noteRaw < 0) {
		int q = noteRaw / 12;
		int r = noteRaw - q * 12;
		noteFinal = (r == 0) ? 0 : (unsigned char)(noteRaw + 12);
	} else {
		noteFinal = (unsigned char)noteRaw;
	}

	self[4 + keyNum] = noteFinal;
	self[0x84 + keyNum] = channel;

	unsigned char *panel = STGAPIFrontPanelStatus::sInstance;
	panel[STGAPI_OFF_MIDI_ECHO0] = noteFinal;
	panel[STGAPI_OFF_MIDI_ECHO1] = velocity;

	SendFrontPanelKeyMidiMessage((unsigned char)(channel | 0x90), noteFinal, velocity);
}

void CSTGFrontPanel::HandleKeyOff(unsigned char keyNum, unsigned char velocity)
{
	unsigned char *self = (unsigned char *)this;

	*(unsigned char *)CPowerOffTimer::sInstance = 1;
	if (*(unsigned char *)CSTGMidiPortManager::sInstance == 0)
		return;

	unsigned char storedChannel = self[0x84 + keyNum];
	unsigned char storedNote = self[4 + keyNum];

	SendFrontPanelKeyMidiMessage((unsigned char)(storedChannel | 0x80), storedNote, velocity);
}
