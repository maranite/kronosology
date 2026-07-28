/* special_func_cc_map.cpp - see include/special_func_cc_map.h for full provenance. */

#include "special_func_cc_map.h"
#include "ustg_api_wrappers.h"
#include <string.h>

/* Real string table contents not independently verified (see special_func_cc_map.h's
 * own header comment) -- provides a linkable definition only. A future pass that
 * recovers the real `SpecialFunctionMappingStringTable` data (ground truth's own
 * `PTR_s_*_091fc6xx`-style string pointers, visible in HasMatchingMapping's own
 * decompile) should replace this placeholder array in place.
 */
const char *SpecialFunctionMappingStringTable[] = {
	"Program Up", "Program Down", "Song Start", "Song Punch", "Tap Tempo",
	"Octave Up", "Octave Down", "Ribbon Lock", "RT Knob", "Pad Func",
	"JSX Lock", "JSY Lock", "JS +Y Lock", "JS -Y Lock",
	"JSX Rib Lock", "JSY Rib Lock", "JS +Y Rib Lock", "JS -Y Rib Lock",
	"SW1 Func", "SW2 Func", "Inc Func", "Dec Func", "Chord SW",
	"Drum Track Enable", "Aftertouch Lock"
};

/* ---- slot byte offsets (see header comment table) ---- */
enum {
	kOffProgramUp = 0x00,
	kOffProgramDown = 0x02,
	kOffSongStart = 0x04,
	kOffSongPunch = 0x06,
	kOffTapTempo = 0x08,
	kOffOctaveUp = 0x0a,
	kOffOctaveDown = 0x0c,
	kOffRibbonLock = 0x0e,
	kOffRTKnobFunc = 0x10, /* array base, 2 bytes/slot, 8 slots -> 0x10..0x1f */
	kOffPadFunc = 0x20,    /* array base, 2 bytes/slot, 8 slots -> 0x20..0x2f */
	kOffJSXLock = 0x30,
	kOffJSYLock = 0x32,
	kOffJSPYLock = 0x34,
	kOffJSMYLock = 0x36,
	kOffJSXRibLock = 0x38,
	kOffJSYRibLock = 0x3a,
	kOffJSPYRibLock = 0x3c,
	kOffJSMYRibLock = 0x3e,
	kOffSW1Func = 0x40,
	kOffSW2Func = 0x42,
	kOffIncFunc = 0x44,
	kOffDecFunc = 0x46,
	kOffChordSw = 0x48,
	kOffDTrackEnable = 0x4a,
	kOffAftertouchLock = 0x4c,
	kOffDirty = 0x4e
};

/* Fixed scalar slot accessor pair, matching ground truth's own
 * `*out = (T)this[chanOff/ccOff]; this[ccOff-or-chanOff] = in; this[kOffDirty] = 1;`
 * shape exactly (every one of the 23 named-slot Set* bodies sets the dirty flag).
 */
#define DEFINE_SLOT_ACCESSORS(Name, ChanOff, CcOff)                                    \
	void CSpecialFuncCCMap::Get##Name##MIDIChannel(unsigned char *out) const           \
	{                                                                                    \
		*out = mSlots[ChanOff];                                                         \
	}                                                                                    \
	void CSpecialFuncCCMap::Set##Name##MIDIChannel(const char *in)                     \
	{                                                                                    \
		unsigned char v = (unsigned char)*in;                                           \
		mSlots[kOffDirty] = 1;                                                          \
		mSlots[ChanOff] = v;                                                            \
	}                                                                                    \
	void CSpecialFuncCCMap::Get##Name##CCAssign(char *out) const                       \
	{                                                                                    \
		*out = (char)mSlots[CcOff];                                                     \
	}                                                                                    \
	void CSpecialFuncCCMap::Set##Name##CCAssign(const char *in)                        \
	{                                                                                    \
		unsigned char v = (unsigned char)*in;                                           \
		mSlots[kOffDirty] = 1;                                                          \
		mSlots[CcOff] = v;                                                              \
	}

DEFINE_SLOT_ACCESSORS(ProgramUp, kOffProgramUp, kOffProgramUp + 1)
DEFINE_SLOT_ACCESSORS(ProgramDown, kOffProgramDown, kOffProgramDown + 1)
DEFINE_SLOT_ACCESSORS(SongStart, kOffSongStart, kOffSongStart + 1)
DEFINE_SLOT_ACCESSORS(SongPunch, kOffSongPunch, kOffSongPunch + 1)
DEFINE_SLOT_ACCESSORS(TapTempo, kOffTapTempo, kOffTapTempo + 1)
DEFINE_SLOT_ACCESSORS(OctaveUp, kOffOctaveUp, kOffOctaveUp + 1)
DEFINE_SLOT_ACCESSORS(OctaveDown, kOffOctaveDown, kOffOctaveDown + 1)
DEFINE_SLOT_ACCESSORS(RibbonLock, kOffRibbonLock, kOffRibbonLock + 1)
DEFINE_SLOT_ACCESSORS(JSXLock, kOffJSXLock, kOffJSXLock + 1)
DEFINE_SLOT_ACCESSORS(JSYLock, kOffJSYLock, kOffJSYLock + 1)
DEFINE_SLOT_ACCESSORS(JSPYLock, kOffJSPYLock, kOffJSPYLock + 1)
DEFINE_SLOT_ACCESSORS(JSMYLock, kOffJSMYLock, kOffJSMYLock + 1)
DEFINE_SLOT_ACCESSORS(JSXRibLock, kOffJSXRibLock, kOffJSXRibLock + 1)
DEFINE_SLOT_ACCESSORS(JSYRibLock, kOffJSYRibLock, kOffJSYRibLock + 1)
DEFINE_SLOT_ACCESSORS(JSPYRibLock, kOffJSPYRibLock, kOffJSPYRibLock + 1)
DEFINE_SLOT_ACCESSORS(JSMYRibLock, kOffJSMYRibLock, kOffJSMYRibLock + 1)
DEFINE_SLOT_ACCESSORS(SW1Func, kOffSW1Func, kOffSW1Func + 1)
DEFINE_SLOT_ACCESSORS(SW2Func, kOffSW2Func, kOffSW2Func + 1)
DEFINE_SLOT_ACCESSORS(IncFunc, kOffIncFunc, kOffIncFunc + 1)
DEFINE_SLOT_ACCESSORS(DecFunc, kOffDecFunc, kOffDecFunc + 1)
DEFINE_SLOT_ACCESSORS(ChordSw, kOffChordSw, kOffChordSw + 1)
DEFINE_SLOT_ACCESSORS(DTrackEnable, kOffDTrackEnable, kOffDTrackEnable + 1)
DEFINE_SLOT_ACCESSORS(AftertouchLock, kOffAftertouchLock, kOffAftertouchLock + 1)

#undef DEFINE_SLOT_ACCESSORS

/* RTKnobFunc / PadFunc: 8-element arrays, 2 bytes/slot, index clamped to [0,7] --
 * matches ground truth's own `uVar1 = 7; if (param_1 < 8) uVar1 = param_1;` idiom.
 */
static inline unsigned ClampIndex8(unsigned idx)
{
	return idx < 8 ? idx : 7;
}

void CSpecialFuncCCMap::GetRTKnobFuncMIDIChannel(unsigned idx, unsigned char *out) const
{
	*out = mSlots[kOffRTKnobFunc + ClampIndex8(idx) * 2];
}

void CSpecialFuncCCMap::SetRTKnobFuncMIDIChannel(unsigned idx, const unsigned char *in)
{
	unsigned char v = *in;
	mSlots[kOffDirty] = 1;
	mSlots[kOffRTKnobFunc + ClampIndex8(idx) * 2] = v;
}

void CSpecialFuncCCMap::GetRTKnobFuncCCAssign(unsigned idx, char *out) const
{
	*out = (char)mSlots[kOffRTKnobFunc + 1 + ClampIndex8(idx) * 2];
}

void CSpecialFuncCCMap::SetRTKnobFuncCCAssign(unsigned idx, const char *in)
{
	unsigned char v = (unsigned char)*in;
	mSlots[kOffDirty] = 1;
	mSlots[kOffRTKnobFunc + 1 + ClampIndex8(idx) * 2] = v;
}

void CSpecialFuncCCMap::GetPadFuncMIDIChannel(unsigned idx, unsigned char *out) const
{
	*out = mSlots[kOffPadFunc + ClampIndex8(idx) * 2];
}

void CSpecialFuncCCMap::SetPadFuncMIDIChannel(unsigned idx, const unsigned char *in)
{
	unsigned char v = *in;
	mSlots[kOffDirty] = 1;
	mSlots[kOffPadFunc + ClampIndex8(idx) * 2] = v;
}

void CSpecialFuncCCMap::GetPadFuncCCAssign(unsigned idx, char *out) const
{
	*out = (char)mSlots[kOffPadFunc + 1 + ClampIndex8(idx) * 2];
}

void CSpecialFuncCCMap::SetPadFuncCCAssign(unsigned idx, const char *in)
{
	unsigned char v = (unsigned char)*in;
	mSlots[kOffDirty] = 1;
	mSlots[kOffPadFunc + 1 + ClampIndex8(idx) * 2] = v;
}

/* .text+0x08a42cf0 - 32 (chan,cc) pairs, all defaulting to (0x10, 0xff), then marks
 * dirty and re-downloads to STG. Ground truth writes each pair as two literal byte
 * stores; expressed here as a loop over the same 32 offsets for maintainability --
 * behaviorally identical (order doesn't matter, all 32 offsets are disjoint bytes).
 */
void CSpecialFuncCCMap::ResetAssignments()
{
	for (unsigned off = 0x00; off < kOffDirty; off += 2) {
		mSlots[off] = 0x10;
		mSlots[off + 1] = 0xff;
	}
	mSlots[kOffDirty] = 1;
	DownloadAllToSTG();
}

/* .text+0x08a42e40 - zero the whole object, then ResetAssignments(). Ground truth
 * tries Load() first and skips the reset on success; Load() is deferred (see header),
 * so this always takes the "no saved settings" path.
 */
void CSpecialFuncCCMap::Initialize()
{
	memset(mSlots, 0, sizeof(mSlots));
	ResetAssignments();
}

/* .text+0x08a42220 - mechanical push of every slot to the STG engine. Ground truth's
 * own per-slot (channel-param-id, cc-param-id) pairs, offset order, transcribed
 * directly from DownloadAllToSTG@08a42220.c.
 */
void CSpecialFuncCCMap::DownloadAllToSTG() const
{
	USTGAPIGlobal::UpdateGlobalParameter(0x4a, 0, (int)mSlots[0x10]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4b, 0, (char)mSlots[0x11]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4a, 1, (int)mSlots[0x12]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4b, 1, (char)mSlots[0x13]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4a, 2, (int)mSlots[0x14]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4b, 2, (char)mSlots[0x15]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4a, 3, (int)mSlots[0x16]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4b, 3, (char)mSlots[0x17]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4a, 4, (int)mSlots[0x18]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4b, 4, (char)mSlots[0x19]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4a, 5, (int)mSlots[0x1a]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4b, 5, (char)mSlots[0x1b]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4a, 6, (int)mSlots[0x1c]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4b, 6, (char)mSlots[0x1d]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4a, 7, (int)mSlots[0x1e]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4b, 7, (char)mSlots[0x1f]);

	USTGAPIGlobal::UpdateGlobalParameter(0x4c, 0, (int)mSlots[0x20]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4d, 0, (char)mSlots[0x21]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4c, 1, (int)mSlots[0x22]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4d, 1, (char)mSlots[0x23]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4c, 2, (int)mSlots[0x24]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4d, 2, (char)mSlots[0x25]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4c, 3, (int)mSlots[0x26]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4d, 3, (char)mSlots[0x27]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4c, 4, (int)mSlots[0x28]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4d, 4, (char)mSlots[0x29]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4c, 5, (int)mSlots[0x2a]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4d, 5, (char)mSlots[0x2b]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4c, 6, (int)mSlots[0x2c]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4d, 6, (char)mSlots[0x2d]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4c, 7, (int)mSlots[0x2e]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4d, 7, (char)mSlots[0x2f]);

	USTGAPIGlobal::UpdateGlobalParameter(0x3a, 0, (int)mSlots[0x00]);
	USTGAPIGlobal::UpdateGlobalParameter(0x3b, 0, (char)mSlots[0x01]);
	USTGAPIGlobal::UpdateGlobalParameter(0x3c, 0, (int)mSlots[0x02]);
	USTGAPIGlobal::UpdateGlobalParameter(0x3d, 0, (char)mSlots[0x03]);
	USTGAPIGlobal::UpdateGlobalParameter(0x3e, 0, (int)mSlots[0x04]);
	USTGAPIGlobal::UpdateGlobalParameter(0x3f, 0, (char)mSlots[0x05]);
	USTGAPIGlobal::UpdateGlobalParameter(0x40, 0, (int)mSlots[0x06]);
	USTGAPIGlobal::UpdateGlobalParameter(0x41, 0, (char)mSlots[0x07]);
	USTGAPIGlobal::UpdateGlobalParameter(0x42, 0, (int)mSlots[0x08]);
	USTGAPIGlobal::UpdateGlobalParameter(0x43, 0, (char)mSlots[0x09]);
	USTGAPIGlobal::UpdateGlobalParameter(0x44, 0, (int)mSlots[0x0a]);
	USTGAPIGlobal::UpdateGlobalParameter(0x45, 0, (char)mSlots[0x0b]);
	USTGAPIGlobal::UpdateGlobalParameter(0x46, 0, (int)mSlots[0x0c]);
	USTGAPIGlobal::UpdateGlobalParameter(0x47, 0, (char)mSlots[0x0d]);
	USTGAPIGlobal::UpdateGlobalParameter(0x48, 0, (int)mSlots[0x0e]);
	USTGAPIGlobal::UpdateGlobalParameter(0x49, 0, (char)mSlots[0x0f]);

	USTGAPIGlobal::UpdateGlobalParameter(0x4e, 0, (int)mSlots[0x30]);
	USTGAPIGlobal::UpdateGlobalParameter(0x4f, 0, (char)mSlots[0x31]);
	USTGAPIGlobal::UpdateGlobalParameter(0x50, 0, (int)mSlots[0x32]);
	USTGAPIGlobal::UpdateGlobalParameter(0x51, 0, (char)mSlots[0x33]);
	USTGAPIGlobal::UpdateGlobalParameter(0x52, 0, (int)mSlots[0x34]);
	USTGAPIGlobal::UpdateGlobalParameter(0x53, 0, (char)mSlots[0x35]);
	USTGAPIGlobal::UpdateGlobalParameter(0x54, 0, (int)mSlots[0x36]);
	USTGAPIGlobal::UpdateGlobalParameter(0x55, 0, (char)mSlots[0x37]);
	USTGAPIGlobal::UpdateGlobalParameter(0x56, 0, (int)mSlots[0x38]);
	USTGAPIGlobal::UpdateGlobalParameter(0x57, 0, (char)mSlots[0x39]);
	USTGAPIGlobal::UpdateGlobalParameter(0x58, 0, (int)mSlots[0x3a]);
	USTGAPIGlobal::UpdateGlobalParameter(0x59, 0, (char)mSlots[0x3b]);
	USTGAPIGlobal::UpdateGlobalParameter(0x5a, 0, (int)mSlots[0x3c]);
	USTGAPIGlobal::UpdateGlobalParameter(0x5b, 0, (char)mSlots[0x3d]);
	USTGAPIGlobal::UpdateGlobalParameter(0x5c, 0, (int)mSlots[0x3e]);
	USTGAPIGlobal::UpdateGlobalParameter(0x5d, 0, (char)mSlots[0x3f]);
	USTGAPIGlobal::UpdateGlobalParameter(0x5e, 0, (int)mSlots[0x40]);
	USTGAPIGlobal::UpdateGlobalParameter(0x5f, 0, (char)mSlots[0x41]);
	USTGAPIGlobal::UpdateGlobalParameter(0x60, 0, (int)mSlots[0x42]);
	USTGAPIGlobal::UpdateGlobalParameter(0x61, 0, (char)mSlots[0x43]);
	USTGAPIGlobal::UpdateGlobalParameter(0x62, 0, (int)mSlots[0x44]);
	USTGAPIGlobal::UpdateGlobalParameter(0x63, 0, (char)mSlots[0x45]);
	USTGAPIGlobal::UpdateGlobalParameter(0x64, 0, (int)mSlots[0x46]);
	USTGAPIGlobal::UpdateGlobalParameter(0x65, 0, (char)mSlots[0x47]);
	USTGAPIGlobal::UpdateGlobalParameter(0x66, 0, (int)mSlots[0x48]);
	USTGAPIGlobal::UpdateGlobalParameter(0x67, 0, (char)mSlots[0x49]);
	USTGAPIGlobal::UpdateGlobalParameter(0x68, 0, (int)mSlots[0x4a]);
	USTGAPIGlobal::UpdateGlobalParameter(0x69, 0, (char)mSlots[0x4b]);
	USTGAPIGlobal::UpdateGlobalParameter(0x6a, 0, (int)mSlots[0x4c]);
	USTGAPIGlobal::UpdateGlobalParameter(0x6b, 0, (char)mSlots[0x4d]);
}

/* .text+0x08a438e0 - real string table content not independently verified, see
 * file header; lookup logic itself is trivial and faithfully reproduced.
 */
void CSpecialFuncCCMap::GetMappingName(ESpecialCCMapFunction fn, const char *&out) const
{
	out = SpecialFunctionMappingStringTable[fn];
}
