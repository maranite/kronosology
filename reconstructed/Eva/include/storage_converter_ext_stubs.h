/*
 * storage_converter_ext_stubs.h  -  minimal, LINKABLE stand-ins for the
 * external, not-yet-modeled classes that the real ExtXXXXtoIntYYYY/
 * IntXXXXtoExtYYYY conversion bodies (storage_format_converters.cpp) call
 * into. Found 2026-07-28 while tracing the real conversion payload for the
 * ~32-class storage-format-converter family (storage_format_converters.h).
 *
 * Every one of these classes is a genuine, separate, much larger subsystem
 * of its own (the real in-memory representations of CDrumKit/CWaveSeq/
 * CPCMProg/CMOSSProg/CGlobal/CSong/CSongControl/CCombi programs/patches, plus
 * CStorageMap/CFileKge/CMemoryAccessor utility classes) -- reconstructing any
 * of them for real is squarely out of scope for this batch (which is about
 * the storage-converter family's OWN conversion logic, not the data models
 * being converted). Same "declare the minimum viable slice, no-op body,
 * clearly flagged" discipline already established for HAL_DisableInterrupts()/
 * HAL_EnableInterrupts() (prog_converter.h/.cpp) and CZ::StrCmpIgnoreCase
 * (cz_util.h) -- these exist ONLY so the converter bodies that genuinely call
 * them can compile and link; their OWN real bodies are not reconstructed and
 * not guessed at (no-op / neutral-default stub bodies only).
 *
 * A future batch that takes on any of these classes for real should replace
 * the matching stub here with the genuine reconstruction and drop it from
 * this file; do not duplicate.
 */

#ifndef STORAGE_CONVERTER_EXT_STUBS_H
#define STORAGE_CONVERTER_EXT_STUBS_H

// ---- per-format in-memory program/patch representations ------------------
// Each converter's Ext{X}toInt{Y} body does a raw memcpy of external-format
// bytes into a buffer it then treats as one of these classes' own object
// layout, calling ClipParams() (a real, confirmed, no-argument instance
// method -- ground truth clamps freshly-migrated fields to valid ranges
// after the copy) to sanitize it. None of these classes' own real fields are
// modeled; ClipParams() is a linkable no-op, not the real clamp logic.

class CDrumKit {
public:
	void ClipParams() {}
};

class CWaveSeq {
public:
	void ClipParams() {}
};

class CPCMProg {
public:
	void ClipParams() {}
};

class CMOSSProg {
public:
	void ClipParams() {}
};

// CCombi: also forward-declared (incomplete, intentionally) by
// stg_unsol_msg_handler.h for an unrelated, still-deferred purpose -- that
// forward declaration and this full definition never appear in the same
// translation unit today, but if that ever changes, only ONE full definition
// may survive (standard C++ ODR); prefer keeping this one, since it is the
// first place CCombi needs real callable methods.
class CCombi {
public:
	void ClipParams() {}
};

// ---- CGlobal: 2 sub-block initializers called by CGlobalConverter::
// Ext0000toInt0002 (storage_format_converters.cpp) after zeroing a handful
// of migration-time flag/gap fields. Real bodies not modeled.
//
// UPDATE (2026-07-28, special_func_cc_map.h/cglobal.h batch): CGlobal is no longer
// a pure stub -- a real embedded CSpecialFuncCCMap sub-object at +0x602c was traced
// and reconstructed (a completely independent decompile that, as a nice cross-check,
// agrees to the byte with this file's own long-standing "+0x607b" migration-field
// offset: 0x602c + CSpecialFuncCCMap::kSize(0x4f) == 0x607b). The full `class CGlobal`
// definition -- including these same 2 no-op stub methods, kept identical -- now lives
// in cglobal.h; only ONE definition may exist per C++ ODR, so it is included here
// rather than redeclared.
#include "cglobal.h"

// ---- CSongControl: called by CSongControlConverter::Int0000toExt0000's
// variant-flag branch (storage_format_converters.cpp). Real bodies not
// modeled.
class CSongControl {
public:
	void Initialize() {}
	void ApplyNonEventRelated(const CSongControl &) {}
};

// ---- CSong: called by CSongConverter::Ext0003toInt0003
// (storage_format_converters.cpp). Real body not modeled.
class CSong {
public:
	void CopySongTimbreSet(const CSong &) {}
};

// ---- CDrumTrackCommonParam: called by CProgCombiSongCommonConverter::
// ConvertToCurrent (storage_format_converters.cpp), on a sub-object placed
// immediately after the fixed-size block it just memcpy'd. Real body not
// modeled.
class CDrumTrackCommonParam {
public:
	void Initialize() {}
};

// ---- CStorageMap: a real, confirmed-live V3-program-bank remap facility
// consulted by CPCMProgConverter::Ext0005toInt0005 / CMOSSProgConverter::
// Ext0005toInt0005 (storage_format_converters.cpp) right after their own
// ClipParams() call. `ShouldRemapV3Order()` gates whether remapping happens
// at all; ground truth's own real predicate/table are not modeled, so this
// stub conservatively always reports "no remap needed" (false) -- matching
// the project's convention of a NEUTRAL default for an unmodeled external
// gate, never a fabricated "always remap" behavior that would silently
// mutate caller data based on invented logic.
class CStorageMap {
public:
	static bool ShouldRemapV3Order() { return false; }
	static void RemapParamProgramBankNo(unsigned char bankId, unsigned char subId,
	                                     unsigned char *outBankId, unsigned char *outSubId)
	{
		// Real body (a lookup table) not modeled; identity passthrough is the
		// only behaviorally-safe stand-in for a call that, per the guard
		// above, is never actually reached by ShouldRemapV3Order()==false.
		*outBankId = bankId;
		*outSubId = subId;
	}
};

// CStorage: the real, unmodeled global-object-storage singleton consulted by
// CSysExGlobal::GetObjectPointer() (sysex_objects_ge_region.cpp, round 42).
// Ground truth's own decompile shows two bare, unchained calls
// (`CStorage::GetInstance(); CStorage::GetGlobal();`, both results
// discarded, real declared return type genuinely `void` -- same "call
// discards the result" shape already confirmed for
// CSysExSongControl::GetObjectPointer, sysex_control_objects.h) -- modeled
// here as the minimum viable no-op slice, not a real singleton/global
// accessor pair.
class CStorage {
public:
	static CStorage *GetInstance() { return 0; }
	void *GetGlobal() { return 0; }
};

// ---- CFileKge: GE (Global Effects) file-format helper consulted by
// CGEConverter::Int0000toExt0000 / CGETemplateConverter::Int0000toExt0000
// (storage_format_converters.cpp) to compute a trailing "item code" appended
// after the exported payload (see CMemoryAccessor::WriteBig32Bit below).
// `EType` distinguishes the two real call sites (CGEConverter passes 0,
// CGETemplateConverter passes 1 -- confirmed via each call site's own literal
// immediate operand, real enumerator names not recovered). Real body (the
// actual code/checksum computation) not modeled; returns 0.
class CFileKge {
public:
	enum EType { kType0 = 0, kType1 = 1 };
	static unsigned long GetItemCode(EType, unsigned char *, unsigned long) { return 0; }
};

// ---- EProgParamBankID / CProgDrumTrackData: called by CProgAncestorConverter::
// ConvertToCurrent (storage_format_converters.cpp), 2026-07-28 follow-up
// batch. `EProgParamBankID` is ALSO independently declared, file-scoped, in
// stg_unsol_msg_handler.cpp (same enumerator-less "reserved=0" shape, same
// real dependency discovered independently there) -- same "only one full
// definition may survive if these TUs are ever merged" caveat already
// flagged on CCombi above; the two never appear in the same translation unit
// today. Ground truth's own call passes the literal immediate 0x10 for the
// bank argument; no real enumerator name recovered for it.
enum EProgParamBankID { eProgParamBankIDReserved = 0 };

class CProgDrumTrackData {
public:
	void Initialize(EProgParamBankID, unsigned char) {}
};

// ---- CMemoryAccessor: a raw-memory-access utility class. WriteBig32Bit
// was added for the CGEConverter/CGETemplateConverter Int0000toExt0000
// pair above; ReadLittle16Bit/WriteLittle16Bit added round 46 (2026-07-29,
// solo) for CFileKscList::ReadFilePath/SaveFilePath's own length-prefix
// protocol (file_ksc_list.h/.cpp) -- both real, tiny, self-contained
// (`.text+0x838dd40`/`0x838dd60`, 17/18 bytes, zero relocations/calls),
// exact shape confirmed via direct `objdump -dr`.
class CMemoryAccessor {
public:
	static void WriteBig32Bit(unsigned char *dst, unsigned long value)
	{
		dst[0] = static_cast<unsigned char>(value >> 24);
		dst[1] = static_cast<unsigned char>(value >> 16);
		dst[2] = static_cast<unsigned char>(value >> 8);
		dst[3] = static_cast<unsigned char>(value);
	}

	static unsigned short ReadLittle16Bit(const unsigned char *src)
	{
		return static_cast<unsigned short>(src[0] | (src[1] << 8));
	}

	static void WriteLittle16Bit(unsigned char *dst, unsigned short value)
	{
		dst[0] = static_cast<unsigned char>(value);
		dst[1] = static_cast<unsigned char>(value >> 8);
	}
};

#endif // STORAGE_CONVERTER_EXT_STUBS_H
