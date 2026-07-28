/*
 * storage_format_converters.cpp  -  see include/storage_format_converters.h
 * for the full derivation. Every body below was produced by a scripted
 * objdump -dr -M intel -> Python classifier (not hand-transcribed) that
 * grouped all 59 ValidateExtXXXX symbols in the binary by instruction shape;
 * see that header's "Formula legend" for what each cited formula means.
 */

#include "storage_format_converters.h"

#include <cstring>
#include <stdint.h>

// ---- CPCMProgConverter / CMOSSProgConverter ----
// Ext0005toInt0005: memcpy(internal,external,size); ClipParams(); then, if
// CStorageMap::ShouldRemapV3Order(), remap the (subId@+0xa80, bankId@+0xa81)
// byte pair through CStorageMap::RemapParamProgramBankNo() and write the
// remapped values back. Confirmed via fresh disassembly of both classes'
// bodies independently (not assumed identical from one sample).

void CPCMProgConverter::Ext0005toInt0005(const CConvertStorageParam &param) const
{
	std::memcpy(param.m_internalBuf, param.m_externalBuf, param.m_size);

	unsigned char *base = reinterpret_cast<unsigned char *>(param.m_internalBuf);
	reinterpret_cast<CPCMProg *>(base)->ClipParams();

	if (CStorageMap::ShouldRemapV3Order()) {
		unsigned char bankId = base[0xa81];
		unsigned char subId = base[0xa80];
		unsigned char outBankId = 0, outSubId = 0;
		CStorageMap::RemapParamProgramBankNo(bankId, subId, &outBankId, &outSubId);
		base[0xa81] = outBankId;
		base[0xa80] = outSubId;
	}
}

void CPCMProgConverter::Int0005toExt0005(const CConvertStorageParam &param) const
{
	std::memcpy(param.m_externalBuf, param.m_internalBuf, param.m_size);
}

bool CMOSSProgConverter::ValidateExt0004(const CConvertStorageParam &) const
{
	return false;
}

void CMOSSProgConverter::Ext0004toInt0005(const CConvertStorageParam &) const
{
}

void CMOSSProgConverter::Ext0005toInt0005(const CConvertStorageParam &param) const
{
	std::memcpy(param.m_internalBuf, param.m_externalBuf, param.m_size);

	unsigned char *base = reinterpret_cast<unsigned char *>(param.m_internalBuf);
	reinterpret_cast<CMOSSProg *>(base)->ClipParams();

	if (CStorageMap::ShouldRemapV3Order()) {
		unsigned char bankId = base[0xa81];
		unsigned char subId = base[0xa80];
		unsigned char outBankId = 0, outSubId = 0;
		CStorageMap::RemapParamProgramBankNo(bankId, subId, &outBankId, &outSubId);
		base[0xa81] = outBankId;
		base[0xa80] = outSubId;
	}
}

void CMOSSProgConverter::Int0005toExt0005(const CConvertStorageParam &param) const
{
	std::memcpy(param.m_externalBuf, param.m_internalBuf, param.m_size);
}

// ---- CCombiConverter ----

bool CCombiConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x1e56;
}

bool CCombiConverter::ValidateExt0001(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x1e76 || param.m_extFormatId == 0x1e7a;
}

bool CCombiConverter::ValidateExt0002(const CConvertStorageParam &param) const
{
	if (param.m_variantFlag == 0)
		return param.m_extFormatId == 0x2c18;
	return param.m_extFormatId == 0x1e76;
}

bool CCombiConverter::ValidateExt0003(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x1e82 || param.m_extFormatId == 0x1e76;
}

// .text+0x08df7940, 37B. IDENTITY (export direction, own literal copy).
void CCombiConverter::Int0003toExt0003(const CConvertStorageParam &param) const
{
	std::memcpy(param.m_externalBuf, param.m_internalBuf, param.m_size);
}

// ---- CDrumKitConverter ----

bool CDrumKitConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x2c18;
}

bool CDrumKitConverter::ValidateExt0001(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x5618;
}

/* .text+0x08dfc720, 15B: identical constant to ValidateExt0001 above --
 * confirmed byte-for-byte from ground truth, not a transcription slip.
 */
bool CDrumKitConverter::ValidateExt0002(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x5618;
}

bool CDrumKitConverter::ValidateExt0003(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x9618;
}

// .text+0x08dfc740, 37B. IDENTITY (export direction, own literal copy).
void CDrumKitConverter::Int0003toExt0003(const CConvertStorageParam &param) const
{
	std::memcpy(param.m_externalBuf, param.m_internalBuf, param.m_size);
}

// .text+0x08dfc770, 47B. IDENTITY: memcpy(internal,external,size) then CDrumKit::ClipParams().
void CDrumKitConverter::Ext0003toInt0003(const CConvertStorageParam &param) const
{
	std::memcpy(param.m_internalBuf, param.m_externalBuf, param.m_size);
	reinterpret_cast<CDrumKit *>(param.m_internalBuf)->ClipParams();
}

/* .text+0x08dfc620, 218B, local symbol
 * (_ZL17ConvertPartTo0003P9CInstPartPK13CInstPart0000, demangled
 * ConvertPartTo0003(CInstPart*, CInstPart0000 const*)). RESOLVED 2026-07-28
 * follow-up batch (was deferred, see header comment). Migrates one "InstPart"
 * sub-structure from the legacy CInstPart0000 layout (17 bytes touched,
 * relative offsets 0x00-0x10) to the current CInstPart layout (33 bytes
 * touched, relative offsets 0x00-0x20). Transcribed literally (CInstPart's
 * own field semantics not recovered beyond what each op implies):
 *   dst+0x00       = (dst+0x00 & 0xfe) | (src+0x00 & 0x01)  -- 1-bit merge,
 *                     preserving 7 existing high bits already in the
 *                     destination buffer
 *   dst+0x01..0x04 = "KORG" (literal ASCII constant, .rodata+0x8fd3ff4)
 *   dst+0x05..0x08 = 0                                       (.rodata+0x8fd3ff8)
 *   dst+0x09..0x0c = 0                                       (.rodata+0x8fd3ffc)
 *   dst+0x0d..0x0e = "MS" (literal ASCII constant, .rodata+0x8fd4000)
 *   dst+0x0f       = 0                                       (.rodata+0x8fd4002)
 *   dst+0x10       = src+0x01 (one real source byte, folded into this
 *                     otherwise-all-constant 16-byte inserted field)
 *   dst+0x12..0x13 = src+0x02..0x03 (word copy; dst+0x11 untouched -- real gap)
 *   dst+0x14       = src+0x04
 *   dst+0x15       = ground truth's own 2-step read-modify-write:
 *                     tmp = (dst+0x15 & 0xf0) | (src+0x05 & 0x0f);
 *                     dst+0x15 = (tmp & 0x7f) | (src+0x05 & 0x80);
 *   dst+0x16..0x20 = src+0x06..0x10 (11 bytes, 1:1 byte copy)
 * dst+0x01..0x0f is a 15-byte gap the old format didn't have at all (no
 * source bytes read there except the one src+0x01 reuse at dst+0x10) --
 * consistent with a newly-inserted default "bank name" field ("KORG" +
 * zero-padding + "MS" + zero), the same MIGRATE "insert new defaulted
 * fields" shape as CRegionConverter/CGlobalConverter above. Identical
 * constants/shape independently confirmed reused by
 * CWaveSeqConverter::Ext0000toInt0001 below.
 */
static void ConvertPartTo0003(unsigned char *dst, const unsigned char *src)
{
	dst[0x00] = (dst[0x00] & 0xfe) | (src[0x00] & 0x01);

	dst[0x01] = 'K'; dst[0x02] = 'O'; dst[0x03] = 'R'; dst[0x04] = 'G';
	dst[0x05] = 0; dst[0x06] = 0; dst[0x07] = 0; dst[0x08] = 0;
	dst[0x09] = 0; dst[0x0a] = 0; dst[0x0b] = 0; dst[0x0c] = 0;
	dst[0x0d] = 'M'; dst[0x0e] = 'S'; dst[0x0f] = 0;
	dst[0x10] = src[0x01];

	std::memcpy(dst + 0x12, src + 0x02, sizeof(uint16_t));
	dst[0x14] = src[0x04];

	unsigned char merged = (dst[0x15] & 0xf0) | (src[0x05] & 0x0f);
	dst[0x15] = (merged & 0x7f) | (src[0x05] & 0x80);

	std::memcpy(dst + 0x16, src + 0x06, 0x0b);  // dst+0x16..0x20 <- src+0x06..0x10
}

/* .text+0x08dfce50, 292B. MIGRATE, RESOLVED 2026-07-28 follow-up (was
 * deferred pending ConvertPartTo0003 above). Copies a 24-byte (6-dword)
 * header block unconditionally (dst[0x00..0x17] = src[0x00..0x17]), then for
 * each of 128 "keys" (old per-key stride 0xac=172B, new per-key stride
 * 0x12c=300B, both key arrays starting at +0x18) converts 8 InstPart
 * sub-structures via ConvertPartTo0003 (old per-part stride 0x12, new
 * per-part stride 0x22) followed by a plain 24-byte trailing-block copy
 * (relative +0x90..+0xa8 old / +0x110..+0x128 new, contiguous right after
 * the 8th InstPart in both layouts) that ConvertPartTo0003 is not involved
 * in. Finishes with a tail-call to CDrumKit::ClipParams(), same as the
 * sibling Ext0003toInt0003 above.
 */
void CDrumKitConverter::Ext0002toInt0003(const CConvertStorageParam &param) const
{
	unsigned char *dst = reinterpret_cast<unsigned char *>(param.m_internalBuf);
	const unsigned char *src = reinterpret_cast<const unsigned char *>(param.m_externalBuf);

	std::memcpy(dst, src, 0x18);

	static const unsigned oldPartOff[8] = { 0x18, 0x2a, 0x3c, 0x4e, 0x60, 0x72, 0x84, 0x96 };
	static const unsigned newPartOff[8] = { 0x18, 0x3a, 0x5c, 0x7e, 0xa0, 0xc2, 0xe4, 0x106 };
	const unsigned oldStride = 0xac;
	const unsigned newStride = 0x12c;

	for (unsigned key = 0; key < 0x80; ++key) {
		const unsigned char *srcKey = src + key * oldStride;
		unsigned char *dstKey = dst + key * newStride;

		for (int part = 0; part < 8; ++part)
			ConvertPartTo0003(dstKey + newPartOff[part], srcKey + oldPartOff[part]);

		std::memcpy(dstKey + 0x120 + 8, srcKey + 0xa0 + 8, 0x18);
	}

	reinterpret_cast<CDrumKit *>(dst)->ClipParams();
}

// ---- CWaveSeqConverter ----

bool CWaveSeqConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x4a8;
}

bool CWaveSeqConverter::ValidateExt0001(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x8a8;
}

// .text+0x08dfcfa0, 13B. THUNK -> CStorageConverterBase::Int0000toExt0000.
void CWaveSeqConverter::Int0001toExt0001(const CConvertStorageParam &param) const
{
	Int0000toExt0000(param);
}

// .text+0x08dfcfb0, 47B. IDENTITY: memcpy(internal,external,size) then CWaveSeq::ClipParams().
void CWaveSeqConverter::Ext0001toInt0001(const CConvertStorageParam &param) const
{
	std::memcpy(param.m_internalBuf, param.m_externalBuf, param.m_size);
	reinterpret_cast<CWaveSeq *>(param.m_internalBuf)->ClipParams();
}

/* .text+0x08dfcfe0, 355B. MIGRATE, RESOLVED 2026-07-28 follow-up (was
 * deferred pending identification of 4 .rodata constants -- turned out to be
 * the exact same "KORG"/"MS" constants CDrumKitConverter's ConvertPartTo0003
 * uses, see that function's own comment above). Copies a 40-byte (10-dword)
 * header block (dst[0x00..0x27] = src[0x00..0x27]), then for each of 64
 * "steps" (old stride 0x12=18B, new stride 0x22=34B, both starting at +0x28)
 * migrates one step, same overall shape as ConvertPartTo0003:
 *   d+0x00       = (d+0x00 & 0xfc) | (s+0x00 & 0x03)  -- 2-bit merge
 *   d+0x01..0x0c = "KORG" + 8 zero bytes (same .rodata constants)
 *   d+0x0d..0x0f = "MS" + zero
 *   d+0x10       = s+0x01 (one real source byte folded into the inserted field)
 *   d+0x12..0x13 = s+0x02..0x03 (word; d+0x11 untouched, real gap)
 *   d+0x14       = s+0x04
 *   d+0x15       = 2-step read-modify-write merge from s+0x05, identical
 *                  shape to ConvertPartTo0003's dst+0x15
 *   d+0x16..0x17 = s+0x06..0x07 (word)
 *   d+0x18..0x1b = s+0x08..0x0b (4 bytes; d+0x1c/s+0x0c real gap, untouched)
 *   d+0x1d..0x21 = s+0x0d..0x11 (5 bytes)
 * Finishes with a tail-call to CWaveSeq::ClipParams().
 */
void CWaveSeqConverter::Ext0000toInt0001(const CConvertStorageParam &param) const
{
	unsigned char *dst = reinterpret_cast<unsigned char *>(param.m_internalBuf);
	const unsigned char *src = reinterpret_cast<const unsigned char *>(param.m_externalBuf);

	std::memcpy(dst, src, 0x28);

	const unsigned oldStride = 0x12;
	const unsigned newStride = 0x22;

	for (unsigned step = 0; step < 0x40; ++step) {
		const unsigned char *s = src + 0x28 + step * oldStride;
		unsigned char *d = dst + 0x28 + step * newStride;

		d[0x00] = (d[0x00] & 0xfc) | (s[0x00] & 0x03);

		unsigned char nameSrcByte = s[0x01];
		d[0x01] = 'K'; d[0x02] = 'O'; d[0x03] = 'R'; d[0x04] = 'G';
		d[0x05] = 0; d[0x06] = 0; d[0x07] = 0; d[0x08] = 0;
		d[0x09] = 0; d[0x0a] = 0; d[0x0b] = 0; d[0x0c] = 0;
		d[0x0d] = 'M'; d[0x0e] = 'S'; d[0x0f] = 0;
		d[0x10] = nameSrcByte;

		std::memcpy(d + 0x12, s + 0x02, sizeof(uint16_t));
		d[0x14] = s[0x04];

		unsigned char b5 = s[0x05];
		unsigned char merged = (d[0x15] & 0xf0) | (b5 & 0x0f);
		d[0x15] = (merged & 0x7f) | (b5 & 0x80);

		std::memcpy(d + 0x16, s + 0x06, sizeof(uint16_t));
		d[0x18] = s[0x08];
		d[0x19] = s[0x09];
		d[0x1a] = s[0x0a];
		d[0x1b] = s[0x0b];
		d[0x1d] = s[0x0d];
		d[0x1e] = s[0x0e];
		d[0x1f] = s[0x0f];
		d[0x20] = s[0x10];
		d[0x21] = s[0x11];
	}

	reinterpret_cast<CWaveSeq *>(dst)->ClipParams();
}

// ---- CGlobalConverter ----

bool CGlobalConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x602a;
}

bool CGlobalConverter::ValidateExt0001(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x602c;
}

bool CGlobalConverter::ValidateExt0002(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x6084;
}

// .text+0x08dfd180, 13B. THUNK -> CStorageConverterBase::Int0000toExt0000.
void CGlobalConverter::Int0002toExt0002(const CConvertStorageParam &param) const
{
	Int0000toExt0000(param);
}

// .text+0x08dfd190, 13B. THUNK -> CStorageConverterBase::Ext0000toInt0000.
void CGlobalConverter::Ext0002toInt0002(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

/* .text+0x08dfd1a0, 139B. MIGRATE: a fixed-size (0x602c, NOT param.m_size --
 * this legacy format's size is a compile-time constant) copy of the whole
 * legacy CGlobal layout into the new one, then zero 4 migration-time fields
 * (+0x607b byte, +0x607c byte, +0x607d dword, +0x6081 word) and mask 2 flag
 * bytes (+0x607b &= 0xfe, +0x607c &= 0xf8). Ground truth inlines the fixed-
 * size copy as a hand-unrolled rep-movsd-plus-alignment-fixup sequence
 * (a standard compiler idiom for a constant-length memcpy of unknown
 * alignment); reproduced here as plain std::memcpy, behavior-identical.
 */
void CGlobalConverter::Ext0001toInt0002(const CConvertStorageParam &param) const
{
	unsigned char *dst = reinterpret_cast<unsigned char *>(param.m_internalBuf);
	const unsigned char *src = reinterpret_cast<const unsigned char *>(param.m_externalBuf);

	std::memcpy(dst, src, 0x602c);

	dst[0x607b] = 0;
	dst[0x607c] = 0;
	std::memset(dst + 0x607d, 0, sizeof(uint32_t));
	std::memset(dst + 0x6081, 0, sizeof(uint16_t));

	dst[0x607b] &= 0xfe;
	dst[0x607c] &= 0xf8;
}

/* .text+0x08dfd230, 155B. MIGRATE: same fixed-size-copy/zero-4-fields shape
 * as Ext0001toInt0002 above (size 0x602a here, matching this method's own
 * ValidateExt0000 magic), but re-initializes 2 sub-blocks via CGlobal::
 * InitializeSetListParams()/InitializeDrumTrackParams() (called on the
 * freshly-copied internal object) BETWEEN the zero step and the final 2-byte
 * mask, instead of masking immediately.
 */
void CGlobalConverter::Ext0000toInt0002(const CConvertStorageParam &param) const
{
	unsigned char *dst = reinterpret_cast<unsigned char *>(param.m_internalBuf);
	const unsigned char *src = reinterpret_cast<const unsigned char *>(param.m_externalBuf);

	std::memcpy(dst, src, 0x602a);

	dst[0x607b] = 0;
	dst[0x607c] = 0;
	std::memset(dst + 0x607d, 0, sizeof(uint32_t));
	std::memset(dst + 0x6081, 0, sizeof(uint16_t));

	CGlobal *global = reinterpret_cast<CGlobal *>(dst);
	global->InitializeSetListParams();
	global->InitializeDrumTrackParams();

	dst[0x607b] &= 0xfe;
	dst[0x607c] &= 0xf8;
}

// ---- CGEConverter ----

/* .text+0x08dfd2d0, 25B. Ground truth computes the candidate via a branchless
 * `cmp byte[+0x1a],1; sbb eax,eax; and eax,0xffffffe0; add eax,0x9f0` sequence
 * rather than a real branch. Evaluated with correct 32-bit wraparound:
 *   m_variantFlag == 0: eax = 0xffffffff & 0xffffffe0 = 0xffffffe0;
 *                        + 0x9f0 wraps to 0x9d0
 *   m_variantFlag == 1: eax = 0 & 0xffffffe0 = 0; + 0x9f0 = 0x9f0
 * i.e. exactly FLAG2(m_variantFlag, 0x9d0, 0x9f0) -- reproduced as a plain
 * branch, behavior-identical.
 */
bool CGEConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	unsigned long candidate = (param.m_variantFlag == 0) ? 0x9d0ul : 0x9f0ul;
	return param.m_extFormatId == candidate;
}

// .text+0x08dfd2f0, 37B. IDENTITY (own literal copy, not a thunk).
void CGEConverter::Ext0000toInt0000(const CConvertStorageParam &param) const
{
	std::memcpy(param.m_internalBuf, param.m_externalBuf, param.m_size);
}

/* .text+0x08dfd320, 98B. Real: compute a CFileKge "item code" over the
 * internal buffer (skipping its first 0x20 bytes, and 0x20 bytes shorter, if
 * param.m_variantFlag is set -- a header the GE item-code computation
 * doesn't cover in the "variant" case), memcpy(external,internal,size), then
 * append the item code as a big-endian dword immediately after the copied
 * region via CMemoryAccessor::WriteBig32Bit. Confirmed: the memcpy re-reads
 * param's own fields fresh (not the possibly variantFlag-adjusted locals),
 * so the adjustment ONLY affects the item-code computation, never the copy.
 */
void CGEConverter::Int0000toExt0000(const CConvertStorageParam &param) const
{
	unsigned char *codeSrc = reinterpret_cast<unsigned char *>(param.m_internalBuf);
	unsigned long codeLen = param.m_size;
	if (param.m_variantFlag != 0) {
		codeSrc += 0x20;
		codeLen -= 0x20;
	}
	unsigned long itemCode = CFileKge::GetItemCode(CFileKge::kType0, codeSrc, codeLen);

	std::memcpy(param.m_externalBuf, param.m_internalBuf, param.m_size);

	CMemoryAccessor::WriteBig32Bit(
		reinterpret_cast<unsigned char *>(param.m_externalBuf) + param.m_size, itemCode);
}

// ---- CGETemplateConverter ----

bool CGETemplateConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x10584;
}

// .text+0x08dfd3a0, 37B. IDENTITY (own literal copy, not a thunk).
void CGETemplateConverter::Ext0000toInt0000(const CConvertStorageParam &param) const
{
	std::memcpy(param.m_internalBuf, param.m_externalBuf, param.m_size);
}

// .text+0x08dfd3d0, 98B. Same shape as CGEConverter::Int0000toExt0000, but
// unconditional (no variantFlag adjustment observed) and CFileKge::EType
// kType1 (CGEConverter passes kType0) -- confirmed via each call site's own
// literal immediate operand for the enum argument.
void CGETemplateConverter::Int0000toExt0000(const CConvertStorageParam &param) const
{
	unsigned long itemCode = CFileKge::GetItemCode(
		CFileKge::kType1,
		reinterpret_cast<unsigned char *>(param.m_internalBuf),
		param.m_size);

	std::memcpy(param.m_externalBuf, param.m_internalBuf, param.m_size);

	CMemoryAccessor::WriteBig32Bit(
		reinterpret_cast<unsigned char *>(param.m_externalBuf) + param.m_size, itemCode);
}

// ---- CSongDescConverter ----

bool CSongDescConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x40;
}

// ---- CPatternDescConverter ----

bool CPatternDescConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x1c;
}

// ---- CCueListConverter ----

bool CCueListConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x4;
}

// ---- CRegionConverter ----

bool CRegionConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x7c;
}

bool CRegionConverter::ValidateExt0001(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x130;
}

// .text+0x08dfd620, 13B. THUNK -> CStorageConverterBase::Int0000toExt0000.
void CRegionConverter::Int0001toExt0001(const CConvertStorageParam &param) const
{
	Int0000toExt0000(param);
}

// .text+0x08dfd490, 37B. IDENTITY (own literal copy, not a thunk).
void CRegionConverter::Ext0001toInt0001(const CConvertStorageParam &param) const
{
	std::memcpy(param.m_internalBuf, param.m_externalBuf, param.m_size);
}

/* .text+0x08dfd560, 177B. MIGRATE: old (Ext0000) and new (Int0001) CRegion
 * layouts agree byte-for-byte for their first 0x68 bytes (raw offsets,
 * CRegion's own real field semantics not recovered). The new layout inserts
 * 0xb4 (180) zero-initialized bytes at old offset 0x68 (ground truth: a
 * `rep stos` zero-fill, confirmed 45 dwords = 0xb4 bytes), then the old
 * struct's own tail fields (0x68..0x7c) are copied forward to new offset
 * 0x11c..0x130.
 */
void CRegionConverter::Ext0000toInt0001(const CConvertStorageParam &param) const
{
	unsigned char *dst = reinterpret_cast<unsigned char *>(param.m_internalBuf);
	const unsigned char *src = reinterpret_cast<const unsigned char *>(param.m_externalBuf);

	std::memcpy(dst, src, 0x68);
	std::memset(dst + 0x68, 0, 0xb4);

	std::memcpy(dst + 0x11c, src + 0x68, sizeof(uint32_t));
	std::memcpy(dst + 0x120, src + 0x6c, sizeof(uint32_t));
	dst[0x124] = src[0x70];
	dst[0x125] = src[0x71];
	std::memcpy(dst + 0x126, src + 0x72, sizeof(uint16_t));
	std::memcpy(dst + 0x128, src + 0x74, sizeof(uint32_t));
	std::memcpy(dst + 0x12c, src + 0x78, sizeof(uint32_t));
}

/* .text+0x08dfd4c0, 156B. MIGRATE: the exact reverse of Ext0000toInt0001
 * above -- copies the common 0x68-byte block, then maps the new layout's
 * relocated tail fields (0x11c..0x130) back to the old struct's original
 * positions (0x68..0x7c), silently dropping the 0xb4-byte gap of new-only
 * fields entirely (faithful data loss on downgrade to the old external
 * format, not a bug).
 */
void CRegionConverter::Int0000toExt0000(const CConvertStorageParam &param) const
{
	const unsigned char *src = reinterpret_cast<const unsigned char *>(param.m_internalBuf);
	unsigned char *dst = reinterpret_cast<unsigned char *>(param.m_externalBuf);

	std::memcpy(dst, src, 0x68);

	std::memcpy(dst + 0x68, src + 0x11c, sizeof(uint32_t));
	std::memcpy(dst + 0x6c, src + 0x120, sizeof(uint32_t));
	dst[0x70] = src[0x124];
	dst[0x71] = src[0x125];
	std::memcpy(dst + 0x72, src + 0x126, sizeof(uint16_t));
	std::memcpy(dst + 0x74, src + 0x128, sizeof(uint32_t));
	std::memcpy(dst + 0x78, src + 0x12c, sizeof(uint32_t));
}

// ---- CSongControlConverter ----

bool CSongControlConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x1490;
}

/* .text+0x08dfd640, 101B. Real: if (!param.m_variantFlag), a plain fixed-size
 * (0x1490) memcpy(external,internal) suffices (old/new CSongControl layouts
 * agree byte-for-byte). Otherwise, ground truth does NOT copy at all --
 * instead it re-derives the external-format object via CSongControl::
 * Initialize() (reset to defaults) followed by ApplyNonEventRelated() with
 * the internal object as the source, a genuine "can't losslessly represent
 * this version's event-related state in the old format, so rebuild only the
 * non-event-related fields" migration path.
 */
void CSongControlConverter::Int0000toExt0000(const CConvertStorageParam &param) const
{
	void *src = param.m_internalBuf;
	void *dst = param.m_externalBuf;

	if (param.m_variantFlag == 0) {
		std::memcpy(dst, src, 0x1490);
		return;
	}

	CSongControl *dstControl = reinterpret_cast<CSongControl *>(dst);
	dstControl->Initialize();
	dstControl->ApplyNonEventRelated(*reinterpret_cast<const CSongControl *>(src));
}

// ---- the 5 "event" converters ----
// All 5 bodies are byte-identical in ground truth (confirmed independently for
// each address, not assumed from one sample): return param.m_extFormatId ==
// param.m_size -- same shape as CStorageConverterBase::ValidateExt0000
// (storage_converter_base.cpp). CORRECTED 2026-07-28: previously written as
// `(unsigned long)param.m_externalBuf` under the pre-correction field/offset
// mapping; ground truth reads raw offset +0x04 (confirmed fresh via
// CMidiEventConverter::ValidateExt0000's own disassembly: `mov edx,[eax+0x4]`),
// which this project's struct now names m_size -- see storage_converter_base.h's
// own correction note for the full derivation.

bool CMidiEventConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == param.m_size;
}

bool CMasterEventConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == param.m_size;
}

bool CAudioEventConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == param.m_size;
}

bool CAutomationEventConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == param.m_size;
}

bool CPatternEventConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == param.m_size;
}

// ---- CSetListConverter ----

bool CSetListConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x10f28;
}

// ---- CSongConverter ----

bool CSongConverter::ValidateExt0000(const CConvertStorageParam &) const
{
	return false;
}

bool CSongConverter::ValidateExt0001(const CConvertStorageParam &) const
{
	return false;
}

bool CSongConverter::ValidateExt0002(const CConvertStorageParam &) const
{
	return false;
}

bool CSongConverter::ValidateExt0003(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x3314;
}

/* .text+0x08e03200, 107B. Real: memset(internal, 0, 0x3314) (clear the whole
 * destination song object first), CSong::CopySongTimbreSet(*external) on the
 * internal object, memcpy(internal+0x1e84, external+0x1e84, 0x1490) (a
 * second, adjacent block -- ground truth's own rep-movsd count is 0x524
 * DWORDS, i.e. 0x524*4 = 0x1490 bytes, not 0x524 bytes -- confirmed via the
 * instruction's own DWORD operand size), then CCombi::ClipParams() on the
 * internal object. CSong genuinely embeds a CCombi/timbre-set-shaped
 * sub-object at +0x1e84 (ground truth's own tail-call target resolves to
 * CCombi::ClipParams independently of the CopySongTimbreSet call, not a
 * transcription mistake).
 */
void CSongConverter::Ext0003toInt0003(const CConvertStorageParam &param) const
{
	unsigned char *dst = reinterpret_cast<unsigned char *>(param.m_internalBuf);
	const unsigned char *src = reinterpret_cast<const unsigned char *>(param.m_externalBuf);

	std::memset(dst, 0, 0x3314);

	reinterpret_cast<CSong *>(dst)->CopySongTimbreSet(*reinterpret_cast<const CSong *>(src));

	std::memcpy(dst + 0x1e84, src + 0x1e84, 0x1490);

	reinterpret_cast<CCombi *>(dst)->ClipParams();
}

// ---- CProgCombiSongCommonConverter ----

/* .text+0x08df42f0, 107B. Real: memcpy(dst,src,0x50c) (ground truth inlines
 * this as a hand-unrolled rep-movsd-plus-alignment-fixup sequence with a
 * confirmed `shr ecx,2` byte-to-dword-count derivation -- i.e. 0x50c IS
 * already a byte count, unlike CSongConverter::Ext0003toInt0003's fixed-
 * dword-count rep movs above), then CDrumTrackCommonParam::Initialize() on
 * the sub-object placed immediately after the copied block (dst+0x50c).
 */
void CProgCombiSongCommonConverter::ConvertToCurrent(CProgCombiSongCommon *dst,
                                                       const CProgCombiSongCommon0000 *src)
{
	std::memcpy(dst, src, 0x50c);
	reinterpret_cast<CDrumTrackCommonParam *>(
		reinterpret_cast<unsigned char *>(dst) + 0x50c)->Initialize();
}

// ---- CProgAncestorConverter ----
// RESOLVED 2026-07-28 follow-up batch (was deliberately out-of-scope for the
// prior pass, not blocked). Both overloads share one implementation body
// (confirmed byte-identical shape/offsets via fresh disassembly of each).

static void ConvertProgAncestorToCurrent(unsigned char *dst, const unsigned char *src)
{
	CProgCombiSongCommonConverter::ConvertToCurrent(
		reinterpret_cast<CProgCombiSongCommon *>(dst),
		reinterpret_cast<const CProgCombiSongCommon0000 *>(src));

	std::memcpy(dst + 0x518, src + 0x50c, 0x1fe);
	std::memcpy(dst + 0x716, src + 0x70a, 0x2e8);
	std::memcpy(dst + 0x9fe, src + 0x9f2, 0x82);

	reinterpret_cast<CProgDrumTrackData *>(dst + 0xa80)->Initialize(
		static_cast<EProgParamBankID>(0x10), 0);
}

void CProgAncestorConverter::ConvertToCurrent(CProgAncestor *dst, const CProgAncestor0000 *src)
{
	ConvertProgAncestorToCurrent(reinterpret_cast<unsigned char *>(dst),
	                              reinterpret_cast<const unsigned char *>(src));
}

void CProgAncestorConverter::ConvertToCurrent(CProgAncestor *dst,
                                                const CProgAncestor0003OASYS *src)
{
	ConvertProgAncestorToCurrent(reinterpret_cast<unsigned char *>(dst),
	                              reinterpret_cast<const unsigned char *>(src));
}
