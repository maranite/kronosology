/*
 * storage_format_converters.cpp  -  see include/storage_format_converters.h
 * for the full derivation. Every body below was produced by a scripted
 * objdump -dr -M intel -> Python classifier (not hand-transcribed) that
 * grouped all 59 ValidateExtXXXX symbols in the binary by instruction shape;
 * see that header's "Formula legend" for what each cited formula means.
 */

#include "storage_format_converters.h"

// ---- CMOSSProgConverter ----

bool CMOSSProgConverter::ValidateExt0004(const CConvertStorageParam &) const
{
	return false;
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

// ---- CWaveSeqConverter ----

bool CWaveSeqConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x4a8;
}

bool CWaveSeqConverter::ValidateExt0001(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x8a8;
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

// ---- CGETemplateConverter ----

bool CGETemplateConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x10584;
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

// ---- CSongControlConverter ----

bool CSongControlConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == 0x1490;
}

// ---- the 5 "event" converters ----
// All 5 bodies are byte-identical in ground truth (confirmed independently for
// each address, not assumed from one sample): return param.m_extFormatId ==
// (unsigned long)param.m_externalBuf -- same odd base-class-default shape as
// CStorageConverterBase::ValidateExt0000 (storage_converter_base.cpp).

bool CMidiEventConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == reinterpret_cast<unsigned long>(param.m_externalBuf);
}

bool CMasterEventConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == reinterpret_cast<unsigned long>(param.m_externalBuf);
}

bool CAudioEventConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == reinterpret_cast<unsigned long>(param.m_externalBuf);
}

bool CAutomationEventConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == reinterpret_cast<unsigned long>(param.m_externalBuf);
}

bool CPatternEventConverter::ValidateExt0000(const CConvertStorageParam &param) const
{
	return param.m_extFormatId == reinterpret_cast<unsigned long>(param.m_externalBuf);
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
