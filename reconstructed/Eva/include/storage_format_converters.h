/*
 * storage_format_converters.h  -  the concrete, per-file-format
 * CStorageConverterBase-derived converter classes (storage_converter_base.h),
 * found 2026-07-28 while tracing CStorageConverterBase::Open()'s 2 real
 * external callers (both land in CProgConverter::Open(), prog_converter.h).
 *
 * A fresh `nm -C` sweep found this is NOT a small family: ~32 concrete classes,
 * ~246 methods total (CSongConverter 18, CMIDITrackConverter 17,
 * CPCMProgConverter/CMOSSProgConverter 16 each, CAudioTrackConverter 14,
 * CDrumKitConverter/CCombiConverter 12 each, CFSConverter 11,
 * CSoundFontConverter/CGlobalConverter 10 each, CRegionConverter 9,
 * CWaveSeqConverter/CKontaktSampleConverter/CKontaktBankConverter 8 each,
 * CProgConverter/CKontaktMultiConverter/CKontaktInstrumentConverter 7 each,
 * CGETemplateConverter/CGEConverter/CAKAIConverter 6 each,
 * CSongControlConverter/CSetListConverter 5 each, CSongDescConverter/
 * CPatternEventConverter/CPatternDescConverter/CMidiEventConverter/
 * CMasterEventConverter/CCueListConverter/CAutomationEventConverter/
 * CAudioEventConverter 4 each, CProgAncestorConverter 2,
 * CProgramNumberConverter/CProgCombiSongCommonConverter 1 each) -- this
 * project's real Prog/Combi/Song/DrumKit/SetList/GE-global/Kontakt-import/
 * AKAI-import/SoundFont-import file-format version-migration toolbox.
 *
 * SCOPE OF THIS PASS: only the `ValidateExtXXXX(const CConvertStorageParam&)
 * const` overrides that are pure, straight-line "compare against a fixed
 * magic/format-tag constant" bodies -- 32 methods across 18 of these classes
 * (CStorageConverterBase's OWN 16 ValidateExtXXXX are reconstructed directly in
 * storage_converter_base.h/.cpp instead, being the base class). A scripted
 * `objdump -dr -M intel` -> Python classifier over all 59 ValidateExtXXXX
 * symbols in the whole binary split them cleanly into 48 "safe" (pure param-only
 * reads, no `this` dependency) vs 11 "deferred" (CPCMProgConverter/
 * CMOSSProgConverter only -- these additionally dereference param.m_size as a
 * pointer to an unidentified ~0xa00+ byte session/context object and read a
 * mode-flag byte at its own +0x9f2/+0x9fe; see storage_converter_base.h's
 * m_size field comment). NOT reconstructed this pass: every ExtXXXXtoIntYYYY
 * real conversion body in any of these classes (the actual field-by-field
 * migration logic -- large, e.g. individually hundreds of bytes each, a future
 * batch's work), every class's own ctor/dtor, and the 11 deferred
 * ValidateExtXXXX above.
 *
 * None of the 48 safe methods reference `this` at all (every one operates
 * purely on its `param` argument) -- there is no real layout dependency on
 * these classes' own data members, so they are declared here as plain public
 * non-virtual overrides of a `CStorageConverterBase` public base (matching
 * ground truth's real single inheritance -- confirmed via each class's own
 * vtable, e.g. `vtable for CCombiConverter` @ 0x08fcdce0) without needing to
 * model any base-class object layout (CStorageConverterBase itself is
 * deliberately non-virtual/stateless in this project's C++ model, see that
 * header's own note).
 *
 * Formula legend (each function's own comment cites which applies):
 *   MAGIC(C)      : return param.m_extFormatId == C;
 *   BUFID         : return param.m_extFormatId == (unsigned long)param.m_externalBuf;
 *                   (same odd base-class-default shape as
 *                   CStorageConverterBase::ValidateExt0000 -- literal transcription)
 *   FALSE         : return false;
 *   OR2(C1,C2)    : return param.m_extFormatId == C1 || param.m_extFormatId == C2;
 *   FLAG2(f,A,B)  : return param.m_extFormatId == (param.m_variantFlag == 0 ? A : B);
 *   MASKSEL(A,B)  : same shape as FLAG2 but computed via ground truth's own
 *                   branchless sbb/and/add sequence (CGEConverter::ValidateExt0000
 *                   only) -- reduces to FLAG2(m_variantFlag, 0x9d0, 0x9f0), verified
 *                   with correct 32-bit wraparound arithmetic, not re-derived by eye.
 */

#ifndef STORAGE_FORMAT_CONVERTERS_H
#define STORAGE_FORMAT_CONVERTERS_H

#include "storage_converter_base.h"

// -- CProgConverter's own 2 sibling "program" sub-converters --------------
// CPCMProgConverter: ALL 6 ValidateExtXXXX (.text+0x08df4620..0x08df47a0)
// dereference param.m_size as a session-context pointer (see
// storage_converter_base.h's m_size comment) -- fully deferred, not declared
// here at all this pass (nothing of it would be reconstructed).
//
// CMOSSProgConverter: 5 of its 6 ValidateExtXXXX have the same shape and stay
// deferred; only ValidateExt0004 is a plain FALSE and is reconstructed below.
// Both classes are otherwise real, non-trivial (16 nm -C symbols each) --
// not reconstructed further this pass.
class CMOSSProgConverter : public CStorageConverterBase {
public:
	bool ValidateExt0004(const CConvertStorageParam &) const;  // .text+0x08e07d30, 3B: FALSE
	// ValidateExt0000..0003,0005 (.text+0x08df5b10/5b40/5b80/5bd0/5c20) -- DEFERRED,
	// same param.m_size-as-context-pointer shape as CPCMProgConverter above.
};

// -- CProgConverter's own 2 real callers of CStorageConverterBase::Open() --
class CCombiConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08df78c0, 15B: MAGIC(0x1e56)
	bool ValidateExt0001(const CConvertStorageParam &param) const;  // .text+0x08df78d0, 30B: OR2(0x1e76,0x1e7a)
	bool ValidateExt0002(const CConvertStorageParam &param) const;  // .text+0x08df78f0, 35B: FLAG2(variant,0x2c18,0x1e76)
	bool ValidateExt0003(const CConvertStorageParam &param) const;  // .text+0x08df7920, 30B: OR2(0x1e82,0x1e76)
};

class CDrumKitConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfc700, 15B: MAGIC(0x2c18)
	bool ValidateExt0001(const CConvertStorageParam &param) const;  // .text+0x08dfc710, 15B: MAGIC(0x5618)
	bool ValidateExt0002(const CConvertStorageParam &param) const;  // .text+0x08dfc720, 15B: MAGIC(0x5618)
	bool ValidateExt0003(const CConvertStorageParam &param) const;  // .text+0x08dfc730, 15B: MAGIC(0x9618)
};

class CWaveSeqConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfcf80, 15B: MAGIC(0x4a8)
	bool ValidateExt0001(const CConvertStorageParam &param) const;  // .text+0x08dfcf90, 15B: MAGIC(0x8a8)
};

class CGlobalConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd150, 15B: MAGIC(0x602a)
	bool ValidateExt0001(const CConvertStorageParam &param) const;  // .text+0x08dfd160, 15B: MAGIC(0x602c)
	bool ValidateExt0002(const CConvertStorageParam &param) const;  // .text+0x08dfd170, 15B: MAGIC(0x6084)
};

class CGEConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd2d0, 25B: MASKSEL -> FLAG2(variant,0x9d0,0x9f0)
};

class CGETemplateConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd390, 15B: MAGIC(0x10584)
};

class CSongDescConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd440, 12B: MAGIC(0x40)
};

class CPatternDescConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd450, 12B: MAGIC(0x1c)
};

class CCueListConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd460, 12B: MAGIC(0x4)
};

class CRegionConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd470, 12B: MAGIC(0x7c)
	bool ValidateExt0001(const CConvertStorageParam &param) const;  // .text+0x08dfd480, 15B: MAGIC(0x130)
};

class CSongControlConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd630, 15B: MAGIC(0x1490)
};

// -- the 5 "event" converters: byte-identical bodies, independently confirmed
// (not assumed from one sample) -- see storage_format_converters.cpp.
class CMidiEventConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd6b0, 14B: BUFID
};

class CMasterEventConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd6c0, 14B: BUFID
};

class CAudioEventConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd6d0, 14B: BUFID
};

class CAutomationEventConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd6e0, 14B: BUFID
};

class CPatternEventConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08e02dc0, 14B: BUFID
};

class CSetListConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08e02dd0, 15B: MAGIC(0x10f28)
};

class CSongConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &) const;  // .text+0x08e09f30, 3B: FALSE
	bool ValidateExt0001(const CConvertStorageParam &) const;  // .text+0x08e09f40, 3B: FALSE
	bool ValidateExt0002(const CConvertStorageParam &) const;  // .text+0x08e09f50, 3B: FALSE
	bool ValidateExt0003(const CConvertStorageParam &param) const;  // .text+0x08e031f0, 15B: MAGIC(0x3314)
	// 14 further methods (ExtXXXXtoIntYYYY bodies, ctor/dtor, etc.) not reconstructed.
};

#endif // STORAGE_FORMAT_CONVERTERS_H
