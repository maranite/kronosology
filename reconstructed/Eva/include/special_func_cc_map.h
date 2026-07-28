/*
 * special_func_cc_map.h  -  CSpecialFuncCCMap, a flat 0x4f (79)-byte data class holding
 * the MIDI-channel + CC-number assignment for every "special function" (Program Up/Down,
 * Song Start/Punch, Tap Tempo, Octave Up/Down, Ribbon Lock, 8x RT Knob, 8x Pad Func, all
 * 8 joystick-lock variants, SW1/SW2 Func, Inc/Dec Func, Chord SW, Drum Track Enable,
 * Aftertouch Lock) that can be remote-controlled via an incoming MIDI CC message.
 *
 * Found via a fresh `nm -C` class-inventory sweep (2026-07-28) looking for the next
 * CStorageConverterBase-shaped opportunity: a large family of near-identical tiny
 * accessors. CSpecialFuncCCMap (108 nm entries, 94% Get-star/Set-star by name) and its thin
 * wrapper CGlobal (123 entries, same names at +0x602c) are exactly that shape, and are
 * genuine config/settings data -- NOT part of the `CForm`/`CSK` mode-UI or `CESxxxTask`
 * "CSK model layer" family PLAN.md/es_common.h already rule out of scope (confirmed:
 * `CESGlobalTask::SetChordSwCCAssign(unsigned char, unsigned char const*, EEditSource)`,
 * .text+0x08c64450, forwards straight through `CGlobal` at `this+0x84` to here -- the
 * task class is the out-of-scope caller, this data class is not).
 *
 * REAL LAYOUT, confirmed field-by-field from every one of the 100 plain accessor bodies
 * below (all `*param_1 = (char)this[N];` / `this[N] = *param_1; this[0x4e] = 1;`, `this`
 * decompiled as a 1-byte-element pointer i.e. plain byte-offset arithmetic -- Ghidra
 * never recovered a real struct for this class):
 *
 *   +0x00/+0x01  ProgramUp    MIDI channel / CC assign
 *   +0x02/+0x03  ProgramDown  "
 *   +0x04/+0x05  SongStart    "
 *   +0x06/+0x07  SongPunch    "
 *   +0x08/+0x09  TapTempo     "
 *   +0x0a/+0x0b  OctaveUp     "
 *   +0x0c/+0x0d  OctaveDown   "
 *   +0x0e/+0x0f  RibbonLock   "
 *   +0x10..+0x1f RTKnobFunc[0..7], 2 bytes/slot (chan, cc) -- array accessors, index
 *                clamped to [0,7] in ground truth (`idx = param_1 < 8 ? param_1 : 7;`)
 *   +0x20..+0x2f PadFunc[0..7], same 2-byte/slot array shape as RTKnobFunc
 *   +0x30/+0x31  JSXLock      MIDI channel / CC assign
 *   +0x32/+0x33  JSYLock      "
 *   +0x34/+0x35  JSPYLock     "
 *   +0x36/+0x37  JSMYLock     "
 *   +0x38/+0x39  JSXRibLock   "
 *   +0x3a/+0x3b  JSYRibLock   "
 *   +0x3c/+0x3d  JSPYRibLock  "
 *   +0x3e/+0x3f  JSMYRibLock  "
 *   +0x40/+0x41  SW1Func      "
 *   +0x42/+0x43  SW2Func      "
 *   +0x44/+0x45  IncFunc      "
 *   +0x46/+0x47  DecFunc      "
 *   +0x48/+0x49  ChordSw      "
 *   +0x4a/+0x4b  DTrackEnable "
 *   +0x4c/+0x4d  AftertouchLock "
 *   +0x4e        "dirty" flag -- every Set* sets it to 1; Save()/Load() clear it to 0
 *                after a successful FMApi round-trip. Confirmed part of the SAVED blob
 *                too (Save()/Load() both use size 0x4f = 79, i.e. offsets 0x00..0x4e
 *                inclusive), which looks like an odd design choice on Korg's part but is
 *                transcribed as-is, not "corrected".
 *
 * Default values, from ResetAssignments() (.text+0x08a42cf0, fully mechanical, 32 pairs
 * of `this[chan]=0x10; this[cc]=0xff;` -- reconstructed verbatim below via a loop):
 * every slot defaults to MIDI channel 0x10 (=16, an out-of-range/"global channel"
 * sentinel -- real MIDI channels are 0-15) and CC 0xff. HasMatchingMapping()'s own real
 * body confirms the convention: it treats `(char)ccAssign < 0` (i.e. 0x80-0xff, which
 * includes the 0xff default) as "this slot has no CC assigned, skip it" -- so 0xff is a
 * real sentinel, not just this project's inference.
 *
 * DEFERRED (not reconstructed this pass, all confirmed real via their own decompile):
 *   Save()/Load()/Update() -- real FMApi file I/O against a `K:\PRELOAD\SFCCMAP.BIN`
 *     path (recovered from the ground truth's own SIMD-strlen-then-append idiom) plus a
 *     `CStorage::GetInstance()`/`CPreloadFile::GetSumData()` checksum dependency chain --
 *     same "real device file I/O, out of scope" call this project already made for
 *     CFMApiInstance/CFileMan's own Save/Load-shaped methods elsewhere.
 *   HasMatchingMapping(unsigned char chan, char cc, ESpecialCCMapFunction&, unsigned&,
 *     char const*&) -- a real, fully decompilable ~30-slot linear scan (first slot
 *     matching (chan,cc) wins) used by a MIDI-learn-style reverse lookup, but its only
 *     call site is inside the out-of-scope CESxxxTask "CSK model layer" (real-time CC
 *     dispatch); not reconstructed to keep this batch's risk of a mistranscribed 30-way
 *     branch low for zero in-scope benefit.
 *   GetMappingName()'s own real string table contents (`SpecialFunctionMappingStringTable`)
 *     -- the lookup logic itself (`*out = table[index];`) IS reconstructed below against
 *     an `extern` table, since the real UI label strings were not independently verified.
 */

#ifndef SPECIAL_FUNC_CC_MAP_H
#define SPECIAL_FUNC_CC_MAP_H

typedef int ESpecialCCMapFunction;

class CSpecialFuncCCMap {
public:
	/* .text+0x08a42e40, 167 bytes. Real body: zero the whole 0x4f-byte object (byte/
	 * word/dword-granularity zeroing loop, alignment-aware in ground truth -- collapsed
	 * here to a plain loop, behaviorally identical), then ResetAssignments(). Ground
	 * truth first tries Load() and skips the reset if it succeeds; Load() itself is
	 * deferred (see file header), so this reconstruction always takes the "no saved
	 * settings" path -- documented divergence, not a bug.
	 */
	void Initialize();

	/* .text+0x08a42cf0, 336 bytes. Sets all 32 (chan,cc) slot pairs to their default
	 * (0x10, 0xff), marks dirty, and calls DownloadAllToSTG().
	 */
	void ResetAssignments();

	/* .text+0x08a42220, 2198 bytes. Pushes every slot's current value to the real STG
	 * engine via USTGAPIGlobal::UpdateGlobalParameter(paramId, subIndex, value) --
	 * mechanical, one call pair (channel id, cc id) per slot in this[] offset order.
	 * `const` in ground truth's own demangled name (never writes `this`).
	 */
	void DownloadAllToSTG() const;

	/* .text+0x08a438e0, 18 bytes. `*out = SpecialFunctionMappingStringTable[fn];` --
	 * table itself not reconstructed, see file header.
	 */
	void GetMappingName(ESpecialCCMapFunction fn, const char *&out) const;

	void GetProgramUpMIDIChannel(unsigned char *out) const;
	void SetProgramUpMIDIChannel(const char *in);
	void GetProgramUpCCAssign(char *out) const;
	void SetProgramUpCCAssign(const char *in);

	void GetProgramDownMIDIChannel(unsigned char *out) const;
	void SetProgramDownMIDIChannel(const char *in);
	void GetProgramDownCCAssign(char *out) const;
	void SetProgramDownCCAssign(const char *in);

	void GetSongStartMIDIChannel(unsigned char *out) const;
	void SetSongStartMIDIChannel(const char *in);
	void GetSongStartCCAssign(char *out) const;
	void SetSongStartCCAssign(const char *in);

	void GetSongPunchMIDIChannel(unsigned char *out) const;
	void SetSongPunchMIDIChannel(const char *in);
	void GetSongPunchCCAssign(char *out) const;
	void SetSongPunchCCAssign(const char *in);

	void GetTapTempoMIDIChannel(unsigned char *out) const;
	void SetTapTempoMIDIChannel(const char *in);
	void GetTapTempoCCAssign(char *out) const;
	void SetTapTempoCCAssign(const char *in);

	void GetOctaveUpMIDIChannel(unsigned char *out) const;
	void SetOctaveUpMIDIChannel(const char *in);
	void GetOctaveUpCCAssign(char *out) const;
	void SetOctaveUpCCAssign(const char *in);

	void GetOctaveDownMIDIChannel(unsigned char *out) const;
	void SetOctaveDownMIDIChannel(const char *in);
	void GetOctaveDownCCAssign(char *out) const;
	void SetOctaveDownCCAssign(const char *in);

	void GetRibbonLockMIDIChannel(unsigned char *out) const;
	void SetRibbonLockMIDIChannel(const char *in);
	void GetRibbonLockCCAssign(char *out) const;
	void SetRibbonLockCCAssign(const char *in);

	/* index clamped to [0,7] in ground truth, matched below */
	void GetRTKnobFuncMIDIChannel(unsigned idx, unsigned char *out) const;
	void SetRTKnobFuncMIDIChannel(unsigned idx, const unsigned char *in);
	void GetRTKnobFuncCCAssign(unsigned idx, char *out) const;
	void SetRTKnobFuncCCAssign(unsigned idx, const char *in);

	void GetPadFuncMIDIChannel(unsigned idx, unsigned char *out) const;
	void SetPadFuncMIDIChannel(unsigned idx, const unsigned char *in);
	void GetPadFuncCCAssign(unsigned idx, char *out) const;
	void SetPadFuncCCAssign(unsigned idx, const char *in);

	void GetJSXLockMIDIChannel(unsigned char *out) const;
	void SetJSXLockMIDIChannel(const char *in);
	void GetJSXLockCCAssign(char *out) const;
	void SetJSXLockCCAssign(const char *in);

	void GetJSYLockMIDIChannel(unsigned char *out) const;
	void SetJSYLockMIDIChannel(const char *in);
	void GetJSYLockCCAssign(char *out) const;
	void SetJSYLockCCAssign(const char *in);

	void GetJSPYLockMIDIChannel(unsigned char *out) const;
	void SetJSPYLockMIDIChannel(const char *in);
	void GetJSPYLockCCAssign(char *out) const;
	void SetJSPYLockCCAssign(const char *in);

	void GetJSMYLockMIDIChannel(unsigned char *out) const;
	void SetJSMYLockMIDIChannel(const char *in);
	void GetJSMYLockCCAssign(char *out) const;
	void SetJSMYLockCCAssign(const char *in);

	void GetJSXRibLockMIDIChannel(unsigned char *out) const;
	void SetJSXRibLockMIDIChannel(const char *in);
	void GetJSXRibLockCCAssign(char *out) const;
	void SetJSXRibLockCCAssign(const char *in);

	void GetJSYRibLockMIDIChannel(unsigned char *out) const;
	void SetJSYRibLockMIDIChannel(const char *in);
	void GetJSYRibLockCCAssign(char *out) const;
	void SetJSYRibLockCCAssign(const char *in);

	void GetJSPYRibLockMIDIChannel(unsigned char *out) const;
	void SetJSPYRibLockMIDIChannel(const char *in);
	void GetJSPYRibLockCCAssign(char *out) const;
	void SetJSPYRibLockCCAssign(const char *in);

	void GetJSMYRibLockMIDIChannel(unsigned char *out) const;
	void SetJSMYRibLockMIDIChannel(const char *in);
	void GetJSMYRibLockCCAssign(char *out) const;
	void SetJSMYRibLockCCAssign(const char *in);

	void GetSW1FuncMIDIChannel(unsigned char *out) const;
	void SetSW1FuncMIDIChannel(const char *in);
	void GetSW1FuncCCAssign(char *out) const;
	void SetSW1FuncCCAssign(const char *in);

	void GetSW2FuncMIDIChannel(unsigned char *out) const;
	void SetSW2FuncMIDIChannel(const char *in);
	void GetSW2FuncCCAssign(char *out) const;
	void SetSW2FuncCCAssign(const char *in);

	void GetIncFuncMIDIChannel(unsigned char *out) const;
	void SetIncFuncMIDIChannel(const char *in);
	void GetIncFuncCCAssign(char *out) const;
	void SetIncFuncCCAssign(const char *in);

	void GetDecFuncMIDIChannel(unsigned char *out) const;
	void SetDecFuncMIDIChannel(const char *in);
	void GetDecFuncCCAssign(char *out) const;
	void SetDecFuncCCAssign(const char *in);

	void GetChordSwMIDIChannel(unsigned char *out) const;
	void SetChordSwMIDIChannel(const char *in);
	void GetChordSwCCAssign(char *out) const;
	void SetChordSwCCAssign(const char *in);

	void GetDTrackEnableMIDIChannel(unsigned char *out) const;
	void SetDTrackEnableMIDIChannel(const char *in);
	void GetDTrackEnableCCAssign(char *out) const;
	void SetDTrackEnableCCAssign(const char *in);

	void GetAftertouchLockMIDIChannel(unsigned char *out) const;
	void SetAftertouchLockMIDIChannel(const char *in);
	void GetAftertouchLockCCAssign(char *out) const;
	void SetAftertouchLockCCAssign(const char *in);

	/* Real total object size, confirmed by Save()/Load()'s own literal `0x4f`. */
	static const unsigned kSize = 0x4f;

private:
	unsigned char mSlots[kSize];
};

/* Real string table backing GetMappingName(); contents not independently verified
 * (see file header) -- provided by whichever translation unit reconstructs the real
 * UI label strings.
 */
extern const char *SpecialFunctionMappingStringTable[];

#endif /* SPECIAL_FUNC_CC_MAP_H */
