/*
 * cglobal.h  -  CGlobal, the real global/system settings singleton. Only the ONE
 * embedded sub-object this pass actually traced is modeled: a `CSpecialFuncCCMap` at
 * `this+0x602c` (confirmed from every one of the 101 forwarder bodies below, e.g.
 * `CGlobal::GetChordSwCCAssign(char *p) { CSpecialFuncCCMap::GetChordSwCCAssign(
 * (CSpecialFuncCCMap*)(this+0x602c), p); }`, GetChordSwCCAssign@08a0f1d0.c).
 *
 * CGlobal's REAL total size is NOT confirmed by this pass -- `CGlobal::Initialize()`
 * (.text+0x089fc2b0) is 59644 bytes and its own Ghidra decompile timed out; `CGlobal::
 * ResetExternalSetup()` (.text+0x08a0ad80) touches a wholly different, much bigger
 * per-preset array (`this[idx*0x62 + N]`, unrelated to the 0x602c offset here) that is
 * NOT modeled. 18 more CGlobal-only methods (GetProgCategoryName, InitializeDrumTrack
 * Params, InitializeSetListParams, ...) are real but likewise not reconstructed --
 * every one of them is a separate, unrelated sub-object this pass did not trace.
 * `mOpaqueHead`/`mSpecialFuncCCMap` below is therefore a MINIMUM-size stand-in (enough
 * to place the one sub-object this pass needs), not a faithful `sizeof(CGlobal)` --
 * do not `new CGlobal` or embed one by value anywhere real total size would matter;
 * every real CGlobal instance in ground truth is heap-allocated by the (out-of-scope)
 * `CGlobal::Initialize()` and reached only through a `CGlobal*` (e.g. the out-of-scope
 * `CESGlobalTask`'s own member at `+0x84`, es_common.h's sibling task classes).
 *
 * Found via the same fresh nm -C class-inventory sweep as special_func_cc_map.h
 * (2026-07-28); reconstructed together since CGlobal's own 101 relevant methods add
 * zero independent logic beyond "cast to CSpecialFuncCCMap* and forward the args".
 */

#ifndef CGLOBAL_H
#define CGLOBAL_H

#include "special_func_cc_map.h"

class CGlobal {
public:
	/* Stubs consolidated here from storage_converter_ext_stubs.h (2026-07-28, same
	 * batch as this file) to keep a single, ODR-legal `class CGlobal` definition --
	 * `CGlobalConverter::Ext0000toInt0002()` (storage_format_converters.cpp) calls
	 * both on a freshly-migrated object. Their own real bodies are not modeled here
	 * either (same "no-op, clearly flagged" status as before the consolidation).
	 * NICE CROSS-CHECK: that converter's own header comment independently found the
	 * next real CGlobal field starts at `+0x607b` -- exactly `0x602c + CSpecialFunc
	 * CCMap::kSize (0x4f)`, i.e. this file's own `mSpecialFuncCCMap` sub-object and
	 * that unrelated reconstruction's own byte-offset find agree on the boundary to
	 * the byte, from two completely independent decompiles.
	 */
	void InitializeSetListParams() {}
	void InitializeDrumTrackParams() {}

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

	/* Real offset of the embedded CSpecialFuncCCMap sub-object, confirmed from every
	 * forwarder body's own `this + 0x602c` cast.
	 */
	static const unsigned kSpecialFuncCCMapOffset = 0x602c;

private:
	unsigned char mOpaqueHead[kSpecialFuncCCMapOffset];
	CSpecialFuncCCMap mSpecialFuncCCMap;
	/* real CGlobal continues past here; not modeled, see file header */
};

#endif /* CGLOBAL_H */
