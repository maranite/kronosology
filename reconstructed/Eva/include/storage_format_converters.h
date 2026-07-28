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
 *   BUFID         : return param.m_extFormatId == param.m_size;
 *                   (same odd base-class-default shape as
 *                   CStorageConverterBase::ValidateExt0000 -- literal transcription.
 *                   CORRECTED 2026-07-28: was `(unsigned long)param.m_externalBuf`
 *                   under the pre-correction m_externalBuf/m_size offset mapping --
 *                   see storage_converter_base.h's own correction note; ground
 *                   truth reads raw offset +0x04, unchanged, only the field NAME
 *                   at that offset changed.)
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
#include "storage_converter_ext_stubs.h"

/*
 * 2026-07-28 follow-up batch (ExtXXXXtoIntYYYY payload trace): reconstructed
 * the real, tractable subset of this family's actual conversion bodies (not
 * just the ValidateExtXXXX predicates from the prior batch). Traced via
 * `objdump -dr -M intel` per method, same as before, but this payload turned
 * out NOT to be a scriptable matrix like CStorageConverterBase's own -- sizes
 * range from 1-byte no-ops up to ~5.9KB (CCombiConverter::Ext0002toInt0003),
 * and the small/medium end is real, individually-meaningful per-format logic
 * (field-by-field struct migrations with inserted/relocated fields, fixed-size
 * legacy-format copies with post-copy sanitization, file-trailer checksums) --
 * much closer to OA.ko's STG value-getter family than to the base class's own
 * uniform 256-method matrix. Reconstructed here: every method under ~0x200
 * bytes across this whole family (26 methods + 1 new CStorageConverterBase
 * method), following 3 recurring real shapes:
 *
 *   - THUNK: a tail-call (jmp) to CStorageConverterBase::Ext0000toInt0000 or
 *     ::Int0000toExt0000 -- confirmed via each thunk's own jmp target.
 *   - IDENTITY: the class's own literal copy of the base class's identity-copy
 *     body (memcpy(dst,src,param.m_size)), sometimes followed by a tail-call
 *     to that concrete format's own ClipParams()/InitializeXxxParams() (see
 *     storage_converter_ext_stubs.h for the not-modeled target classes) and,
 *     for CPCMProgConverter/CMOSSProgConverter, a further conditional
 *     CStorageMap::RemapParamProgramBankNo() call.
 *   - MIGRATE: genuine field-by-field struct migration between old/new
 *     layouts (raw-offset access, target class's own real field semantics not
 *     recovered -- same "declare uncertain fields clearly" discipline as
 *     scsi_driver_base.h).
 *
 * Also fixed a real, confirmed bug this same pass: CConvertStorageParam's
 * m_externalBuf/m_size fields were swapped relative to their true offsets --
 * see storage_converter_base.h's own correction note for the full derivation
 * (found precisely because THIS payload, unlike the prior batch's
 * ValidateExtXXXX-only work, is the first code to use both fields together in
 * one call).
 *
 * NOT reconstructed this pass (real, precisely-scoped deferrals):
 *   - CDrumKitConverter::Ext0002toInt0003 (.text+0x08dfce50, 292B): a genuine
 *     128-iteration loop over 8 CInstPart sub-structures per iteration,
 *     converted via a not-yet-reconstructed static helper `ConvertPartTo0003`
 *     (.text+0x08dfc620, local/non-exported symbol) -- needs that helper
 *     reconstructed first.
 *   - CWaveSeqConverter::Ext0000toInt0001 (.text+0x08dfcfe0, 355B): a real
 *     field migration whose tail references 4 unidentified static default-
 *     value words/dwords at .rodata+0x8fd3ff4..0x8fd4002 -- needs those
 *     identified first (likely a default-oscillator preset table).
 *   - CProgAncestorConverter::ConvertToCurrent (2 overloads, .text+0x08df4360/
 *     0x08df44c0, 345B each): real, tractable (3 fixed-size memcpy blocks with
 *     a consistent +0xc-byte-per-field forward shift between old/new layout,
 *     same shape as CRegionConverter's MIGRATE bodies below, plus a
 *     CProgDrumTrackData::Initialize(EProgParamBankID, unsigned char)
 *     tailcall) but kept out of this batch to bound scope around the
 *     CConvertStorageParam-based Ext/Int family specifically.
 *   - CSongConverter::AdvanceDestination(long) (.text+0x08e00c50, 23B): a real,
 *     small (this[0]-=n; this[4]+=n; return this[0]>=0) cursor-advance helper,
 *     but operates on CSongConverter's OWN instance state (2 fields never
 *     otherwise used/needed by anything reconstructed in this project) rather
 *     than on a CConvertStorageParam -- adding those fields speculatively,
 *     for a method whose only real caller (GetConvertedAllMIDITracksSize et
 *     al) is itself unreconstructed, was judged not worth it this pass.
 *   - Every CCombiConverter/CKontaktXxx/CSoundFontConverter/CAKAIConverter/
 *     CFSConverter/CMIDITrackConverter/CAudioTrackConverter body over ~0x200
 *     bytes (up to CSoundFontConverter::AddSample at 0x14c7=5319B) -- real,
 *     substantial DSP/import-format algorithmic logic (ratio tables, FIR
 *     coefficients, envelope tightening, room mapping, per-oscillator sample
 *     assembly), Tier-B scale work for a dedicated future batch, not "the
 *     matrix" this investigation set out to characterize.
 *
 * CPCMProgConverter's own 11 remaining ValidateExtXXXX (deferred by the prior
 * batch, m_size/param+0xc-as-context-pointer shape) stay deferred -- the
 * m_externalBuf/m_size correction above clarifies WHICH field they dereference
 * (m_externalBuf, not a special dual-use of m_size) but does not identify the
 * ~0xa00+-byte object's own real C++ type, so CProgConverter::Open()'s own
 * session-context-pointer identity remains unresolved.
 */

// -- CProgConverter's own 2 sibling "program" sub-converters --------------
// CPCMProgConverter: 4 of its 6 ValidateExtXXXX (.text+0x08df4620..0x08df47a0)
// dereference param.m_externalBuf as a session-context pointer (see
// storage_converter_base.h's m_externalBuf comment) -- still deferred. Its
// remaining 2 methods (Ext0005toInt0005/Int0005toExt0005) are real
// IDENTITY-shaped conversion bodies and ARE reconstructed this pass.
//
// CMOSSProgConverter: same split -- 4 ValidateExtXXXX deferred (context-
// pointer shape), ValidateExt0004 (plain FALSE, prior batch) plus
// Ext0004toInt0005 (no-op)/Ext0005toInt0005/Int0005toExt0005 (IDENTITY-shaped,
// this pass) reconstructed.
class CPCMProgConverter : public CStorageConverterBase {
public:
	// .text+0x08df4f60, 119B. IDENTITY: memcpy(internal,external,size) then
	// CPCMProg::ClipParams(), then (if CStorageMap::ShouldRemapV3Order())
	// remap the program-bank/sub-bank byte pair at +0xa80/+0xa81 via
	// CStorageMap::RemapParamProgramBankNo().
	void Ext0005toInt0005(const CConvertStorageParam &param) const;
	// .text+0x08df47e0, 37B. IDENTITY (export direction): memcpy(external,internal,size).
	void Int0005toExt0005(const CConvertStorageParam &param) const;
	// ValidateExt0000..0003,0005 (.text+0x08df4620/4660/46a0/46e0/4760) -- DEFERRED,
	// param.m_externalBuf-as-context-pointer shape (see header comment above).
};

class CMOSSProgConverter : public CStorageConverterBase {
public:
	bool ValidateExt0004(const CConvertStorageParam &) const;  // .text+0x08e07d30, 3B: FALSE
	// .text+0x08e07d20, 1B (bare `ret`). Real: unconditional no-op.
	void Ext0004toInt0005(const CConvertStorageParam &) const;
	// .text+0x08df6620, 119B. IDENTITY, same shape as CPCMProgConverter::Ext0005toInt0005.
	void Ext0005toInt0005(const CConvertStorageParam &param) const;
	// .text+0x08df5c50, 37B. IDENTITY (export direction).
	void Int0005toExt0005(const CConvertStorageParam &param) const;
	// ValidateExt0000..0003,0005 (.text+0x08df5b10/5b40/5b80/5bd0/5c20) -- DEFERRED,
	// same param.m_externalBuf-as-context-pointer shape as CPCMProgConverter above.
};

// -- CProgConverter's own 2 real callers of CStorageConverterBase::Open() --
class CCombiConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08df78c0, 15B: MAGIC(0x1e56)
	bool ValidateExt0001(const CConvertStorageParam &param) const;  // .text+0x08df78d0, 30B: OR2(0x1e76,0x1e7a)
	bool ValidateExt0002(const CConvertStorageParam &param) const;  // .text+0x08df78f0, 35B: FLAG2(variant,0x2c18,0x1e76)
	bool ValidateExt0003(const CConvertStorageParam &param) const;  // .text+0x08df7920, 30B: OR2(0x1e82,0x1e76)
	// .text+0x08df7940, 37B. IDENTITY (export direction).
	void Int0003toExt0003(const CConvertStorageParam &param) const;
	// Ext0000toInt0003/Ext0001toInt0003/Ext0002toInt0003/Ext0003toInt0003
	// (.text+0x08dfa4c0/0x08df90c0/0x08df7970/0x08dfb840, 0x1379-0xddf B each)
	// -- DEFERRED, real substantial conversion logic, out of this pass's scope
	// (see header comment).
};

class CDrumKitConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfc700, 15B: MAGIC(0x2c18)
	bool ValidateExt0001(const CConvertStorageParam &param) const;  // .text+0x08dfc710, 15B: MAGIC(0x5618)
	bool ValidateExt0002(const CConvertStorageParam &param) const;  // .text+0x08dfc720, 15B: MAGIC(0x5618)
	bool ValidateExt0003(const CConvertStorageParam &param) const;  // .text+0x08dfc730, 15B: MAGIC(0x9618)
	// .text+0x08dfc740, 37B. IDENTITY (export direction).
	void Int0003toExt0003(const CConvertStorageParam &param) const;
	// .text+0x08dfc770, 47B. IDENTITY: memcpy(internal,external,size) then CDrumKit::ClipParams().
	void Ext0003toInt0003(const CConvertStorageParam &param) const;
	// Ext0002toInt0003 (.text+0x08dfce50, 292B) -- DEFERRED, needs the
	// not-yet-reconstructed static ConvertPartTo0003 helper (see header comment).
};

class CWaveSeqConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfcf80, 15B: MAGIC(0x4a8)
	bool ValidateExt0001(const CConvertStorageParam &param) const;  // .text+0x08dfcf90, 15B: MAGIC(0x8a8)
	// .text+0x08dfcfa0, 13B. THUNK -> CStorageConverterBase::Int0000toExt0000.
	void Int0001toExt0001(const CConvertStorageParam &param) const;
	// .text+0x08dfcfb0, 47B. IDENTITY: memcpy(internal,external,size) then CWaveSeq::ClipParams().
	void Ext0001toInt0001(const CConvertStorageParam &param) const;
	// Ext0000toInt0001 (.text+0x08dfcfe0, 355B) -- DEFERRED, needs 4 unidentified
	// static default-value table entries identified first (see header comment).
};

class CGlobalConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd150, 15B: MAGIC(0x602a)
	bool ValidateExt0001(const CConvertStorageParam &param) const;  // .text+0x08dfd160, 15B: MAGIC(0x602c)
	bool ValidateExt0002(const CConvertStorageParam &param) const;  // .text+0x08dfd170, 15B: MAGIC(0x6084)
	// .text+0x08dfd180, 13B. THUNK -> CStorageConverterBase::Int0000toExt0000.
	void Int0002toExt0002(const CConvertStorageParam &param) const;
	// .text+0x08dfd190, 13B. THUNK -> CStorageConverterBase::Ext0000toInt0000.
	void Ext0002toInt0002(const CConvertStorageParam &param) const;
	// .text+0x08dfd1a0, 139B. MIGRATE: fixed-size (0x602c) copy of the legacy
	// CGlobal layout, zero 4 migration-time fields, mask 2 flag bytes.
	void Ext0001toInt0002(const CConvertStorageParam &param) const;
	// .text+0x08dfd230, 155B. MIGRATE: same shape as Ext0001toInt0002 (fixed
	// size 0x602a) but re-initializes 2 sub-blocks via CGlobal::
	// InitializeSetListParams()/InitializeDrumTrackParams() instead of just masking.
	void Ext0000toInt0002(const CConvertStorageParam &param) const;
};

class CGEConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd2d0, 25B: MASKSEL -> FLAG2(variant,0x9d0,0x9f0)
	// .text+0x08dfd2f0, 37B. IDENTITY: memcpy(internal,external,size) -- this
	// class's own literal copy of the base class's identity-copy body, not a thunk.
	void Ext0000toInt0000(const CConvertStorageParam &param) const;
	// .text+0x08dfd320, 98B. Real: compute a CFileKge "item code" over
	// (internal[+-0x20 if variantFlag], size[+-0x20]), memcpy(external,internal,
	// size), then append the item code as a big-endian dword right after the
	// copied region (CMemoryAccessor::WriteBig32Bit).
	void Int0000toExt0000(const CConvertStorageParam &param) const;
};

class CGETemplateConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd390, 15B: MAGIC(0x10584)
	// .text+0x08dfd3a0, 37B. IDENTITY (own literal copy, not a thunk).
	void Ext0000toInt0000(const CConvertStorageParam &param) const;
	// .text+0x08dfd3d0, 98B. Same shape as CGEConverter::Int0000toExt0000, but
	// unconditional (no variantFlag adjustment) and CFileKge::EType kType1 (vs
	// CGEConverter's kType0) -- confirmed via each call site's own literal
	// immediate operand.
	void Int0000toExt0000(const CConvertStorageParam &param) const;
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
	// .text+0x08dfd620, 13B. THUNK -> CStorageConverterBase::Int0000toExt0000.
	void Int0001toExt0001(const CConvertStorageParam &param) const;
	// .text+0x08dfd490, 37B. IDENTITY (own literal copy, not a thunk).
	void Ext0001toInt0001(const CConvertStorageParam &param) const;
	// .text+0x08dfd560, 177B / .text+0x08dfd4c0, 156B (mirror pair). MIGRATE:
	// old (Ext0000/Int0000) and new (Int0001) CRegion layouts agree byte-for-
	// byte for their first 0x68 bytes; the new layout inserts 0xb4 (180) bytes
	// of brand-new, zero-initialized fields at old offset 0x68, shifting the
	// old struct's own tail fields (0x68..0x7c) forward to new offset
	// 0x11c..0x130. Int0000toExt0000 is the exact reverse (drops the new-only
	// gap entirely on downgrade -- faithful data loss, not a bug).
	void Ext0000toInt0001(const CConvertStorageParam &param) const;
	void Int0000toExt0000(const CConvertStorageParam &param) const;
};

class CSongControlConverter : public CStorageConverterBase {
public:
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08dfd630, 15B: MAGIC(0x1490)
	// .text+0x08dfd640, 101B. Real: if (!param.m_variantFlag) plain
	// memcpy(external,internal,0x1490); else re-derive via CSongControl::
	// Initialize()+ApplyNonEventRelated() on the external-format object
	// instead of copying (a real, meaningful "can't losslessly downgrade,
	// rebuild non-event state instead" migration path).
	void Int0000toExt0000(const CConvertStorageParam &param) const;
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
	// .text+0x08e03200, 107B. Real: memset(internal,0,0x3314), then
	// CSong::CopySongTimbreSet(*external) on the internal object, then
	// memcpy(internal+0x1e84, external+0x1e84, 0x1490), then CCombi::
	// ClipParams() on the internal object (CSong embeds a CCombi/timbre-set-
	// shaped sub-object at +0x1e84 -- real relationship, not a mistake; ground
	// truth's own tail-call target address resolves there independently).
	void Ext0003toInt0003(const CConvertStorageParam &param) const;
	// AdvanceDestination(long) (.text+0x08e00c50, 23B) and 12 further large
	// (0xd7-0xf27B) methods (ExtXXXXtoIntYYYY bodies, ConvertAll*Tracks,
	// GetConverted*Size, ctor/dtor, etc.) not reconstructed -- see header
	// comment for AdvanceDestination's own specific deferral reason.
};

// -- CProgConverter's own shared "common" sub-block converter -------------
// Called directly by CProgAncestorConverter::ConvertToCurrent (itself
// deferred, see header comment) -- reconstructed here since it is small,
// self-contained, and needed to make sense of that caller's own disassembly.
// CProgCombiSongCommon/CProgCombiSongCommon0000 are opaque (never
// dereferenced by name, only as raw memcpy src/dst) -- forward-declared only.
class CProgCombiSongCommon;
class CProgCombiSongCommon0000;

class CProgCombiSongCommonConverter {
public:
	// .text+0x08df42f0, 107B. Real: memcpy(dst,src,0x50c) (old/new layouts
	// agree byte-for-byte for this whole block), then CDrumTrackCommonParam::
	// Initialize() on the sub-object placed immediately after it (dst+0x50c).
	static void ConvertToCurrent(CProgCombiSongCommon *dst,
	                              const CProgCombiSongCommon0000 *src);
};

#endif // STORAGE_FORMAT_CONVERTERS_H
