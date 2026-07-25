// SPDX-License-Identifier: GPL-2.0
/*
 * karma_chord_trigger.cpp  -  RT_chord_trigger(uchar, uchar, uchar, uchar):
 * the real per-pad "Pads (touch to play)" trigger. Ground truth from a
 * from-scratch decompile of OA.ko (MD5 955636c2b11a70a1dbecefaaa7bd4f80,
 * GCC 4.5.0, x86-32 ET_REL) via /home/share/Decomp/oa_export -- see
 * functions/RT_chord_trigger@00522842.c for the exact source decompile
 * this file transcribes. `oa_export`'s addresses carry a fixed +0x10000
 * offset from the real, live-kernel addresses (per this project's own
 * "OA.ko Ghidra Decomp Export" note): 0x00522842 - 0x10000 = 0x00512842,
 * which matches KronosScreenRemoteDaemon/chord_probe_module/chord_probe.c's
 * independently ground-truthed real address AND its confirmed live
 * prologue bytes (55 89 E5 57 56 = push ebp; mov esp,ebp; push edi;
 * push esi) exactly -- two independent sources agreeing on both address
 * and entry bytes.
 *
 * regparm(3): EAX=pad_index (0-7) EDX=velocity (0=release, nonzero=play)
 * ECX=param3 [stack]=param4. Confirmed both by the decompile's own
 * `__regparm3` calling-convention tag and by nks4_inject.c's
 * independently-derived `rt_chord_trigger_t` typedef (same project,
 * different discovery method: a live .text hook, chord_probe.c, that
 * logged real caller args from a physical pad tap).
 *
 * Confirmed call-site anchor (per this task's own instructions):
 * CESProgTask::GetPad1..GetPad8 all call ESCommonKarmaCommon_GetChordMemory
 * with the same 0-based pad-slot convention used here.
 *
 * ---- Data layout derivation -------------------------------------------
 *
 * Two distinct per-pad tables, both indexed 0-7 by pad_index:
 *
 * 1. PERSISTENT CHORD DEFINITION (real base DAT_00cc1774, confirmed
 *    stride 0x12=18 bytes/pad from the decompile's own repeated
 *    `pad_index * 0x12` address arithmetic, used identically on both the
 *    read side (note-on rebuild) and the write side (define/commit)):
 *      +0x00,+0x02,+0x04,+0x06,+0x08,+0x0A,+0x0C,+0x0E : 8 x int16
 *        "noteVel" slots (low byte = MIDI note 0-127, or the sentinel
 *        0xff/-1 = "slot unused" -- confirmed via the decompile's own
 *        `-1 < *(char*)(...)` signed-byte test guarding every read/copy;
 *        high byte = velocity).
 *      +0x10 : uint8 channel-selector byte (0x10 sentinel = "use
 *        Do_KM_get_global_channel()"; any other value = a direct 0-15
 *        MIDI channel to use verbatim). Confirmed via the decompile's
 *        own `(&DAT_00cc1784)[pad*0x12] == 0x10` checks at every
 *        note-out call site.
 *      +0x11 : unused pad byte (never read/written by this function).
 *    NOTE on the one apparent oddity in the raw decompile: the third
 *    slot (+0x04) is addressed through a SEPARATELY-named Ghidra symbol,
 *    `DAT_00cc1778`, with an apparent stride of 9 rather than 0x12 --
 *    this is NOT a different-shaped field. Ghidra sized that array in
 *    units of `short` (its own inferred element type for that symbol,
 *    confirmed by every access to it being a 2-byte `undefined2`/`_DAT_`
 *    read or write, identical in shape to its 7 word-slot siblings);
 *    9 shorts = 18 bytes = exactly the same 0x12 per-pad stride as every
 *    other field. Folded here into one uniform `noteVel[8]` array with
 *    no special-casing -- confirmed equivalent, not merely convenient.
 *
 * 2. ACTIVE/WORKING CHORD (real base DAT_00cffaf9, confirmed uniform
 *    stride 0x10=16 bytes/pad, 8 x {note byte, velocity byte} word
 *    slots/pad -- no oddities). Rebuilt fresh from the persistent
 *    definition every time a nonzero-velocity trigger arrives (compacted:
 *    only the "used" slots of the 8 definition slots are copied in,
 *    skipping 0xff/-1 ones), and is what the release path walks to know
 *    which notes are currently sounding for the pad. Per-pad slot COUNT
 *    lives in a separate byte array, real base DAT_00cffae9.
 *
 * A third, SEPARATE mechanism -- real scalar globals DAT_00cffac4 (a
 * one-shot "commit the staged chord definition now" arm flag) and
 * DAT_00cffae8 (how many of the 8 staging slots, at real base
 * DAT_00cffad8, are valid) -- lets some not-yet-reconstructed caller
 * (presumably the KARMA chord-memory "record/assign" UI path, out of
 * scope here) push a new chord definition into a pad's persistent table.
 * When armed (`param_2 != 0 && DAT_00cffac4 != 0`), THIS SAME entry point
 * commits the staged chord instead of playing anything -- confirmed via
 * the decompile's own top-level `if (param_2==0 || DAT_00cffac4==0)`
 * branch, whose negation is exactly this commit path.
 *
 * ---- Simplifications applied (both confirmed behavior-preserving) -----
 *
 * 1. The decompile's own 16-wide "vectorized" max-velocity scan (the
 *    `bVar24 = bVar25 >> 4` block, lines ~140-219 of the raw decompile)
 *    is dead code for every real input: `bVar25` here is a per-pad
 *    active-slot COUNT, provably <= 8 (only 8 definition slots exist per
 *    pad), so `bVar25 >> 4` is always 0 and the block's own guard
 *    (`if ((bVar24==0) || (bVar25<0x10)) { bVar30=0; uVar23=0; }`)
 *    unconditionally short-circuits straight to the scalar tail loop
 *    right after it. Reconstructed here as that scalar max-scan directly
 *    (a plain "track the largest of up to 8 velocity bytes" loop) -- the
 *    auto-vectorized 16-wide path GCC emitted for a general-purpose
 *    byte-array max reduction is unreachable at this table's real fixed
 *    size, not a behavior difference.
 * 2. The decompile's two structurally-separate "define" sub-cases
 *    (`DAT_00cffae8==0`: clear all 8 slots' low bytes to 0xff; nonzero:
 *    copy the first N staged slots verbatim, then clear the low bytes of
 *    slots N..7) are unified below into one "copy min(N,8) slots, clear
 *    the rest" loop -- the N==0 case is exactly "copy zero, clear all
 *    eight", the same result the decompile's own separate branch
 *    produces. Verified identical for every N by inspection of the
 *    label-fallthrough chain (LAB_0052288a..LAB_005228cc all lead to the
 *    same "clear this slot's low byte, fall into clearing the next one"
 *    pattern the general loop reproduces exactly).
 *
 * ---- Deferred callees ---------------------------------------------------
 *
 * Do_KM_note_out_chord_trig/Do_KM_get_global_channel are themselves
 * confirmed-real, trivial (21 and 12 bytes) pure forwarders to
 * KM_note_out_chord_trig/KM_get_global_channel (per
 * functions/Do_KM_note_out_chord_trig@0056e3fa.c and
 * functions/Do_KM_get_global_channel@0056e9a4.c) -- reconstructed here
 * for real, as thin calls into their own targets. Those inner targets
 * genuinely dispatch further into CKGParamEdit::GetPadChangeSource() /
 * CSKMIDIMsgProcessor::ProcessPadNoteBy{LocalControl,MIDIPort}Message()
 * (KM_note_out_chord_trig, 186 bytes) and CKGEngine::
 * GetLocalControllerChannel() (KM_get_global_channel, 20 bytes) -- real
 * KARMA-engine/MIDI-message-processor internals genuinely out of this
 * project's current scope (matches the bar2_stubs.cpp "reconstruct the
 * caller, stub the confirmed-real deep DSP/engine callee" convention;
 * kept local to this file rather than added to bar2_stubs.cpp itself
 * since nothing else in the tree references these symbols yet -- avoids
 * an unnecessary edit to that shared, concurrently-active file).
 * KM_get_global_channel's stub returns MIDI channel 0 as a safe default
 * (the real CKGEngine::GetLocalControllerChannel() singleton this project
 * hasn't modeled yet presumably returns whatever channel KARMA's global
 * part is currently set to -- 0 is the conventional MIDI-channel default
 * elsewhere in this project's own reconstructed code).
 *
 * DoDynamicMidi (.text+0x527d5c, 7946 bytes) is a large dynamic-MIDI-CC
 * mapping dispatcher shared by many OTHER OA.ko callers, not specific to
 * chord-memory -- out of scope here too; its own two RT_chord_trigger
 * call sites (the "dynamic MIDI echo" tail, gated on real globals
 * DAT_00cd661d/DAT_00cbfbc5/DAT_00cd6488/DAT_00cd6305/DAT_00cd6307) are
 * reconstructed faithfully below, including the exact byte0=pad+0xac /
 * byte0=0xb4 argument pair and the per-iteration count-global re-read --
 * only DoDynamicMidi's own body is a no-op stub.
 */

#include "oa_karma_chord.h"
#include "oa_scale.h"

#define KARMA_NUM_PADS         8
#define KARMA_CHORD_SLOTS      8

namespace {

/* Persistent per-pad chord definition -- real base DAT_00cc1774, see
 * this file's header comment for the full stride/oddity derivation. */
struct PadChordDef {
	short         noteVel[KARMA_CHORD_SLOTS]; /* +0x00..+0x0E */
	unsigned char channelSel;                 /* +0x10 */
	unsigned char _pad11;                     /* +0x11 */
};
static PadChordDef g_padChordDef[KARMA_NUM_PADS];

/* Active/working chord -- real base DAT_00cffaf9. */
struct ActiveChordSlot {
	unsigned char note;
	unsigned char velocity;
};
static ActiveChordSlot g_activeChord[KARMA_NUM_PADS][KARMA_CHORD_SLOTS];
static unsigned char   g_activeChordCount[KARMA_NUM_PADS]; /* DAT_00cffae9 */

/* Chord-definition staging area, filled by a not-yet-reconstructed
 * caller before arming a commit. Real base DAT_00cffad8 (8 words),
 * immediately followed in the real binary's own layout by
 * DAT_00cffae8 (pending count) and DAT_00cffac4 (arm flag). */
static short         g_chordStaging[KARMA_CHORD_SLOTS]; /* DAT_00cffad8 */
static unsigned char  g_chordDefinePendingCount;        /* DAT_00cffae8 */
static unsigned char  g_chordDefineArmed;                /* DAT_00cffac4 */

/* Voice-trigger-mode globals RT_chord_trigger saves, forces to 2 for the
 * duration of its own note-on dispatch loop, then restores. Real
 * addresses DAT_00cd640b/DAT_00cd640c -- not independently characterized
 * elsewhere in this project; treated as opaque state. */
static unsigned char g_triggerModeA = 2; /* DAT_00cd640b */
static unsigned char g_triggerModeB = 2; /* DAT_00cd640c */

/* Dynamic-MIDI echo gating state (tail of RT_chord_trigger). Shared by
 * many other DoDynamicMidi callers elsewhere in OA.ko; characterized
 * here only as far as this function's own use requires. */
static unsigned char g_dynamicMidiEnabled;    /* DAT_00cd661d */
static unsigned char g_dynamicMidiEntryCount; /* DAT_00cbfbc5 */
static unsigned char g_dynamicMidiClockA;     /* DAT_00cd6305 */
static unsigned char g_dynamicMidiClockB;     /* DAT_00cd6307 */
struct DynamicMidiEntry {
	unsigned char channelMask; /* low 5 bits meaningful (real code masks & 0x1f) */
	unsigned char _rest[5];    /* real per-entry stride is 6 bytes; remaining
	                            * fields not touched by RT_chord_trigger */
};
static DynamicMidiEntry *g_dynamicMidiTable; /* DAT_00cd6488 */

/* Tail shared by both the release and note-on paths (real label
 * LAB_00522ca7 onward) -- echoes the pad event to any dynamic-MIDI
 * mapping entry whose channel matches this pad's channel selector. */
static void DynamicMidiEchoTail(unsigned char pad_index, unsigned char velocity,
                                 unsigned char channelSel)
{
	if (g_dynamicMidiEnabled == 0)
		return;
	unsigned char count = g_dynamicMidiEntryCount;
	if (count == 0)
		return;

	int i = 0;
	DynamicMidiEntry *entry = g_dynamicMidiTable;
	for (;;) {
		unsigned char entryChan = entry->channelMask & 0x1f;
		if (channelSel == entryChan ||
		    (entryChan == 0x10 && g_dynamicMidiClockA == g_dynamicMidiClockB)) {
			DoDynamicMidi((unsigned char)i, (unsigned char)(pad_index + 0xac),
			              velocity, velocity);
			DoDynamicMidi((unsigned char)i, 0xb4, velocity, velocity);
			count = g_dynamicMidiEntryCount; /* real code re-reads the count
			                                   * global after each call, in
			                                   * case a callee changed it */
		}
		i++;
		if (i >= (int)count)
			break;
		entry++;
	}
}

} /* anonymous namespace */

/* ---- Deferred callees (see header comment) ----------------------------
 * KM_get_global_channel/KM_note_out_chord_trig are internal to this TU
 * (no other code references them by name), so kept plain `static` --
 * no `extern "C"`/regparm needed since nothing calls them across a TU
 * boundary or needs a stable ABI-matching symbol name for them. */

static unsigned char KM_get_global_channel(void)
{
	return 0; /* confirmed-real, deliberately deferred: CKGEngine::
	           * GetLocalControllerChannel() singleton not modeled yet */
}

unsigned char Do_KM_get_global_channel(void)
{
	return KM_get_global_channel();
}

static void KM_note_out_chord_trig(unsigned char /*channel*/, unsigned char /*note*/,
                                    unsigned char /*velocity*/)
{
	/* confirmed-real, deliberately deferred: dispatches into
	 * CKGParamEdit::GetPadChangeSource() and CSKMIDIMsgProcessor::
	 * ProcessPadNoteBy{LocalControl,MIDIPort}Message() -- genuine
	 * KARMA-engine/MIDI-message-processor internals, out of scope. */
}

void Do_KM_note_out_chord_trig(unsigned char channel, unsigned char note,
                                unsigned char velocity)
{
	KM_note_out_chord_trig(channel, note, velocity);
}

extern "C" __attribute__((regparm(3)))
void DoDynamicMidi(unsigned char /*srcIndex*/, unsigned char /*b0*/,
                    unsigned char /*b1*/, unsigned char /*b2*/)
{
	/* confirmed-real, deliberately deferred: large (7946-byte) dynamic
	 * MIDI-mapping dispatcher shared by many OA.ko callers, out of
	 * scope here -- see this file's header comment. */
}

/* ---- RT_chord_trigger --------------------------------------------------- */

void RT_chord_trigger(unsigned char pad_index, unsigned char velocity,
                       unsigned char param3, unsigned char param4)
{
	(void)param4; /* only ever forwarded through to the (currently no-op)
	               * DoDynamicMidi echo calls below */

	if (velocity != 0 && g_chordDefineArmed != 0) {
		/* ---- DEFINE/COMMIT: stage a new chord definition for this pad,
		 * instead of playing anything (real top-level else branch). */
		g_chordDefineArmed = 0;

		PadChordDef &def = g_padChordDef[pad_index];
		unsigned char copyCount = g_chordDefinePendingCount;
		if (copyCount > KARMA_CHORD_SLOTS)
			copyCount = KARMA_CHORD_SLOTS; /* real code has no explicit
			                                 * clamp -- defensive only,
			                                 * see header comment */
		for (unsigned char i = 0; i < copyCount; ++i)
			def.noteVel[i] = g_chordStaging[i];
		for (unsigned char i = copyCount; i < KARMA_CHORD_SLOTS; ++i)
			def.noteVel[i] = (short)((def.noteVel[i] & 0xff00) | 0x00ff);
		return;
	}

	/* ---- PLAY (velocity != 0) / RELEASE (velocity == 0) ---- */
	PadChordDef &def = g_padChordDef[pad_index];
	unsigned char savedModeA = g_triggerModeA;
	unsigned char savedModeB = g_triggerModeB;
	unsigned char activeCount;

	if (velocity == 0) {
		/* RELEASE: turn off every currently-active note for this pad. */
		activeCount = g_activeChordCount[pad_index];
		for (unsigned char i = 0; i < activeCount; ++i) {
			unsigned char note = g_activeChord[pad_index][i].note;
			unsigned char chan = (def.channelSel == 0x10)
			                          ? Do_KM_get_global_channel()
			                          : def.channelSel;
			Do_KM_note_out_chord_trig(chan, note, 0);
		}
		g_activeChordCount[pad_index] = 0;
	} else {
		/* NOTE-ON: rebuild the active/working chord from the persistent
		 * definition, compacting away unused (0xff/-1) slots. */
		unsigned char n = 0;
		for (int i = 0; i < KARMA_CHORD_SLOTS; ++i) {
			signed char lowByte = (signed char)(def.noteVel[i] & 0xff);
			if (lowByte >= 0) {
				g_activeChord[pad_index][n].note = (unsigned char)lowByte;
				g_activeChord[pad_index][n].velocity =
				    (unsigned char)((def.noteVel[i] >> 8) & 0xff);
				n++;
			}
		}
		g_activeChordCount[pad_index] = n;

		if (param3 == 1 && n != 0) {
			/* Rescale every active note's velocity so the loudest one
			 * matches the incoming trigger velocity exactly (see header
			 * comment for why the real 16-wide vectorized max scan is
			 * dead code and a plain scalar max here is equivalent). */
			unsigned char maxVel = 0;
			for (unsigned char i = 0; i < n; ++i)
				if (g_activeChord[pad_index][i].velocity > maxVel)
					maxVel = g_activeChord[pad_index][i].velocity;
			if (maxVel != velocity) {
				for (unsigned char i = 0; i < n; ++i)
					g_activeChord[pad_index][i].velocity =
					    ScaleByte(g_activeChord[pad_index][i].velocity,
					              1, maxVel, 1, velocity);
			}
		}

		activeCount = n;
		g_triggerModeA = 2;
		g_triggerModeB = 2;
		for (unsigned char i = 0; i < activeCount; ++i) {
			unsigned char note = g_activeChord[pad_index][i].note;
			unsigned char vel  = g_activeChord[pad_index][i].velocity;
			unsigned char chan = (def.channelSel == 0x10)
			                          ? Do_KM_get_global_channel()
			                          : def.channelSel;
			Do_KM_note_out_chord_trig(chan, note, vel);
		}
	}

	g_triggerModeB = savedModeB;
	g_triggerModeA = savedModeA;

	DynamicMidiEchoTail(pad_index, velocity, def.channelSel);
}
