/*
 * test_storage_format_converters.cpp  -  host-side known-answer tests for the
 * 2026-07-28 CStorageConverterBase::Open()-caller-tracing batch:
 *   - CStorageConverterBase's own new ValidateExt0000..000F + Close()
 *     (storage_converter_base.h/.cpp)
 *   - the 32 concrete sibling ValidateExtXXXX overrides across ~18 classes
 *     (storage_format_converters.h/.cpp)
 *   - CProgConverter::Close() forwarding + its dtor pair (prog_converter.h/.cpp)
 *
 * Each ValidateExtXXXX check constructs a CConvertStorageParam with every
 * relevant field set to a value that should NOT match, confirms false, then
 * sets it to the confirmed real magic/constant and confirms true -- an
 * independent black-box check (does the boolean flip when the field crosses
 * the confirmed boundary), not a re-check of the generator's own classification.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "storage_converter_base.h"
#include "storage_format_converters.h"
#include "prog_converter.h"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(const char *name, bool cond)
{
	++g_checks;
	if (!cond) {
		++g_failed;
		std::printf("FAIL: %s\n", name);
	}
}

CConvertStorageParam MakeParam()
{
	CConvertStorageParam p;
	std::memset(&p, 0, sizeof p);
	return p;
}

// ---- CStorageConverterBase's own 16 ValidateExtXXXX + Close() ----

void TestBase()
{
	CStorageConverterBase conv;

	// ValidateExt0000: real BUFID formula -- m_extFormatId == m_size (CORRECTED
	// 2026-07-28: was m_externalBuf under the pre-correction offset mapping;
	// ground truth reads raw offset +0x04, which m_size now names).
	{
		CConvertStorageParam p = MakeParam();
		p.m_size = 0x12345678ul;
		p.m_extFormatId = 0x11111111ul;
		check("Base::ValidateExt0000 mismatch -> false", conv.ValidateExt0000(p) == false);
		p.m_extFormatId = 0x12345678ul;
		check("Base::ValidateExt0000 match -> true", conv.ValidateExt0000(p) == true);
	}

	// ValidateExt0001..000F: unconditional false, any input.
	{
		CConvertStorageParam p = MakeParam();
		p.m_extFormatId = 0xDEADBEEFul;
		check("Base::ValidateExt0001 always false", conv.ValidateExt0001(p) == false);
		check("Base::ValidateExt0002 always false", conv.ValidateExt0002(p) == false);
		check("Base::ValidateExt0003 always false", conv.ValidateExt0003(p) == false);
		check("Base::ValidateExt0004 always false", conv.ValidateExt0004(p) == false);
		check("Base::ValidateExt0005 always false", conv.ValidateExt0005(p) == false);
		check("Base::ValidateExt0006 always false", conv.ValidateExt0006(p) == false);
		check("Base::ValidateExt0007 always false", conv.ValidateExt0007(p) == false);
		check("Base::ValidateExt0008 always false", conv.ValidateExt0008(p) == false);
		check("Base::ValidateExt0009 always false", conv.ValidateExt0009(p) == false);
		check("Base::ValidateExt000A always false", conv.ValidateExt000A(p) == false);
		check("Base::ValidateExt000B always false", conv.ValidateExt000B(p) == false);
		check("Base::ValidateExt000C always false", conv.ValidateExt000C(p) == false);
		check("Base::ValidateExt000D always false", conv.ValidateExt000D(p) == false);
		check("Base::ValidateExt000E always false", conv.ValidateExt000E(p) == false);
		check("Base::ValidateExt000F always false", conv.ValidateExt000F(p) == false);
	}

	// Close(): real no-op -- just confirm it's callable and doesn't crash.
	conv.Close();
	check("Base::Close() callable no-op", true);
}

// ---- the 32 concrete sibling ValidateExtXXXX ----

void TestSiblings()
{
	// MAGIC(C) pattern classes.
	{
		CDrumKitConverter c;
		CConvertStorageParam p = MakeParam();
		p.m_extFormatId = 0;
		check("DrumKit::VE0000 mismatch", c.ValidateExt0000(p) == false);
		p.m_extFormatId = 0x2c18; check("DrumKit::VE0000 match", c.ValidateExt0000(p) == true);
		p.m_extFormatId = 0x5618; check("DrumKit::VE0001 match", c.ValidateExt0001(p) == true);
		check("DrumKit::VE0002 match (same const as VE0001)", c.ValidateExt0002(p) == true);
		p.m_extFormatId = 0x9618; check("DrumKit::VE0003 match", c.ValidateExt0003(p) == true);
		p.m_extFormatId = 0; check("DrumKit::VE0003 mismatch", c.ValidateExt0003(p) == false);
	}
	{
		CWaveSeqConverter c;
		CConvertStorageParam p = MakeParam();
		p.m_extFormatId = 0x4a8; check("WaveSeq::VE0000 match", c.ValidateExt0000(p) == true);
		p.m_extFormatId = 0x8a8; check("WaveSeq::VE0001 match", c.ValidateExt0001(p) == true);
		p.m_extFormatId = 0; check("WaveSeq::VE0001 mismatch", c.ValidateExt0001(p) == false);
	}
	{
		CGlobalConverter c;
		CConvertStorageParam p = MakeParam();
		p.m_extFormatId = 0x602a; check("Global::VE0000 match", c.ValidateExt0000(p) == true);
		p.m_extFormatId = 0x602c; check("Global::VE0001 match", c.ValidateExt0001(p) == true);
		p.m_extFormatId = 0x6084; check("Global::VE0002 match", c.ValidateExt0002(p) == true);
	}
	{
		CGETemplateConverter c;
		CConvertStorageParam p = MakeParam();
		p.m_extFormatId = 0x10584; check("GETemplate::VE0000 match", c.ValidateExt0000(p) == true);
		p.m_extFormatId = 0; check("GETemplate::VE0000 mismatch", c.ValidateExt0000(p) == false);
	}
	{
		CSongDescConverter c;
		CConvertStorageParam p = MakeParam();
		p.m_extFormatId = 0x40; check("SongDesc::VE0000 match", c.ValidateExt0000(p) == true);
	}
	{
		CPatternDescConverter c;
		CConvertStorageParam p = MakeParam();
		p.m_extFormatId = 0x1c; check("PatternDesc::VE0000 match", c.ValidateExt0000(p) == true);
	}
	{
		CCueListConverter c;
		CConvertStorageParam p = MakeParam();
		p.m_extFormatId = 0x4; check("CueList::VE0000 match", c.ValidateExt0000(p) == true);
		p.m_extFormatId = 0x5; check("CueList::VE0000 mismatch", c.ValidateExt0000(p) == false);
	}
	{
		CRegionConverter c;
		CConvertStorageParam p = MakeParam();
		p.m_extFormatId = 0x7c; check("Region::VE0000 match", c.ValidateExt0000(p) == true);
		p.m_extFormatId = 0x130; check("Region::VE0001 match", c.ValidateExt0001(p) == true);
	}
	{
		CSongControlConverter c;
		CConvertStorageParam p = MakeParam();
		p.m_extFormatId = 0x1490; check("SongControl::VE0000 match", c.ValidateExt0000(p) == true);
	}
	{
		CSetListConverter c;
		CConvertStorageParam p = MakeParam();
		p.m_extFormatId = 0x10f28; check("SetList::VE0000 match", c.ValidateExt0000(p) == true);
	}

	// BUFID pattern classes (m_extFormatId == m_size; CORRECTED 2026-07-28, was
	// m_externalBuf -- see TestBase()'s own note above).
	{
		CConvertStorageParam p = MakeParam();
		p.m_size = 0xABCD1234ul;
		p.m_extFormatId = 0;
		CMidiEventConverter a; check("MidiEvent::VE0000 mismatch", a.ValidateExt0000(p) == false);
		p.m_extFormatId = 0xABCD1234ul;
		check("MidiEvent::VE0000 match", a.ValidateExt0000(p) == true);

		CMasterEventConverter b; check("MasterEvent::VE0000 match", b.ValidateExt0000(p) == true);
		CAudioEventConverter d; check("AudioEvent::VE0000 match", d.ValidateExt0000(p) == true);
		CAutomationEventConverter e; check("AutomationEvent::VE0000 match", e.ValidateExt0000(p) == true);
		CPatternEventConverter f; check("PatternEvent::VE0000 match", f.ValidateExt0000(p) == true);
	}

	// CMOSSProgConverter::ValidateExt0004 -- always false (the one safe
	// method in an otherwise-deferred class).
	{
		CMOSSProgConverter c;
		CConvertStorageParam p = MakeParam();
		p.m_extFormatId = 0xDEADBEEFul;
		check("MOSSProg::VE0004 always false", c.ValidateExt0004(p) == false);
	}

	// CSongConverter: 3 always-false + 1 real magic compare.
	{
		CSongConverter c;
		CConvertStorageParam p = MakeParam();
		p.m_extFormatId = 0xDEADBEEFul;
		check("Song::VE0000 always false", c.ValidateExt0000(p) == false);
		check("Song::VE0001 always false", c.ValidateExt0001(p) == false);
		check("Song::VE0002 always false", c.ValidateExt0002(p) == false);
		check("Song::VE0003 mismatch", c.ValidateExt0003(p) == false);
		p.m_extFormatId = 0x3314; check("Song::VE0003 match", c.ValidateExt0003(p) == true);
	}

	// CCombiConverter: MAGIC / OR2 / FLAG2 mix.
	{
		CCombiConverter c;
		CConvertStorageParam p = MakeParam();
		p.m_extFormatId = 0x1e56; check("Combi::VE0000 match", c.ValidateExt0000(p) == true);
		p.m_extFormatId = 0; check("Combi::VE0000 mismatch", c.ValidateExt0000(p) == false);

		p.m_extFormatId = 0x1e76; check("Combi::VE0001 match A", c.ValidateExt0001(p) == true);
		p.m_extFormatId = 0x1e7a; check("Combi::VE0001 match B", c.ValidateExt0001(p) == true);
		p.m_extFormatId = 0; check("Combi::VE0001 mismatch", c.ValidateExt0001(p) == false);

		p.m_variantFlag = 0; p.m_extFormatId = 0x2c18;
		check("Combi::VE0002 flag0 match", c.ValidateExt0002(p) == true);
		p.m_extFormatId = 0x1e76;
		check("Combi::VE0002 flag0 wrong-branch-const mismatch", c.ValidateExt0002(p) == false);
		p.m_variantFlag = 1; p.m_extFormatId = 0x1e76;
		check("Combi::VE0002 flag1 match", c.ValidateExt0002(p) == true);
		p.m_extFormatId = 0x2c18;
		check("Combi::VE0002 flag1 wrong-branch-const mismatch", c.ValidateExt0002(p) == false);
		p.m_variantFlag = 0;

		p.m_extFormatId = 0x1e82; check("Combi::VE0003 match A", c.ValidateExt0003(p) == true);
		p.m_extFormatId = 0x1e76; check("Combi::VE0003 match B", c.ValidateExt0003(p) == true);
		p.m_extFormatId = 0; check("Combi::VE0003 mismatch", c.ValidateExt0003(p) == false);
	}

	// CGEConverter: MASKSEL -> FLAG2(m_variantFlag, 0x9d0, 0x9f0).
	{
		CGEConverter c;
		CConvertStorageParam p = MakeParam();
		p.m_variantFlag = 0; p.m_extFormatId = 0x9d0;
		check("GE::VE0000 flag0 match", c.ValidateExt0000(p) == true);
		p.m_extFormatId = 0x9f0;
		check("GE::VE0000 flag0 wrong-branch mismatch", c.ValidateExt0000(p) == false);
		p.m_variantFlag = 1; p.m_extFormatId = 0x9f0;
		check("GE::VE0000 flag1 match", c.ValidateExt0000(p) == true);
		p.m_extFormatId = 0x9d0;
		check("GE::VE0000 flag1 wrong-branch mismatch", c.ValidateExt0000(p) == false);
	}
}

// ---- 2026-07-28 follow-up batch: the real ExtXXXXtoIntYYYY/IntXXXXtoExtYYYY
// conversion payload (not just ValidateExtXXXX predicates). Each check is an
// INDEPENDENT black-box oracle: fill source/dest buffers with distinct,
// non-overlapping byte patterns computed here (not derived from the
// implementation under test), call the real method, then assert the exact
// expected byte layout -- same discipline as TestBase()/TestSiblings() above
// and test_storage_converter_base.cpp's own sentinel/copy check.

void FillPattern(unsigned char *buf, unsigned n, unsigned char base)
{
	for (unsigned i = 0; i < n; ++i)
		buf[i] = static_cast<unsigned char>(base + i);
}

void TestExtIntPayload()
{
	// -- plain identity-copy family: Ext direction (dst=internal,
	// src=external,n=size) and Int direction (dst=external,src=internal,
	// n=size) -- every one of these must move exactly `size` bytes from the
	// correct source offset to the correct destination offset. This is
	// exactly the shape the 2026-07-28 m_externalBuf/m_size offset
	// correction fixed -- these checks would have failed under the old
	// (buggy) offset mapping (wrong source pointer AND wrong byte count).
	{
		const unsigned N = 64;
		unsigned char src[N], dst[N];

		// Ext direction: CGEConverter::Ext0000toInt0000 (own literal copy).
		FillPattern(src, N, 0x10);
		std::memset(dst, 0xCC, N);
		CConvertStorageParam p = MakeParam();
		p.m_internalBuf = dst;
		p.m_externalBuf = src;
		p.m_size = N;
		CGEConverter().Ext0000toInt0000(p);
		check("GE::Ext0000toInt0000 copies external->internal", std::memcmp(dst, src, N) == 0);

		// Same shape, CRegionConverter::Ext0001toInt0001.
		FillPattern(src, N, 0x20);
		std::memset(dst, 0xCC, N);
		CRegionConverter().Ext0001toInt0001(p);
		check("Region::Ext0001toInt0001 copies external->internal", std::memcmp(dst, src, N) == 0);

		// Same shape, CGETemplateConverter::Ext0000toInt0000.
		FillPattern(src, N, 0x30);
		std::memset(dst, 0xCC, N);
		CGETemplateConverter().Ext0000toInt0000(p);
		check("GETemplate::Ext0000toInt0000 copies external->internal", std::memcmp(dst, src, N) == 0);

		// Int direction (export): dst=external, src=internal.
		unsigned char intBuf[N], extBuf[N];
		FillPattern(intBuf, N, 0x40);
		std::memset(extBuf, 0xCC, N);
		CConvertStorageParam q = MakeParam();
		q.m_internalBuf = intBuf;
		q.m_externalBuf = extBuf;
		q.m_size = N;

		CStorageConverterBase().Int0000toExt0000(q);
		check("Base::Int0000toExt0000 copies internal->external", std::memcmp(extBuf, intBuf, N) == 0);

		std::memset(extBuf, 0xCC, N);
		CCombiConverter().Int0003toExt0003(q);
		check("Combi::Int0003toExt0003 copies internal->external", std::memcmp(extBuf, intBuf, N) == 0);

		std::memset(extBuf, 0xCC, N);
		CDrumKitConverter().Int0003toExt0003(q);
		check("DrumKit::Int0003toExt0003 copies internal->external", std::memcmp(extBuf, intBuf, N) == 0);

		std::memset(extBuf, 0xCC, N);
		CPCMProgConverter().Int0005toExt0005(q);
		check("PCMProg::Int0005toExt0005 copies internal->external", std::memcmp(extBuf, intBuf, N) == 0);

		std::memset(extBuf, 0xCC, N);
		CMOSSProgConverter().Int0005toExt0005(q);
		check("MOSSProg::Int0005toExt0005 copies internal->external", std::memcmp(extBuf, intBuf, N) == 0);

		// THUNK -> base Int0000toExt0000: same observable behavior.
		std::memset(extBuf, 0xCC, N);
		CGlobalConverter().Int0002toExt0002(q);
		check("Global::Int0002toExt0002 thunk copies internal->external", std::memcmp(extBuf, intBuf, N) == 0);

		std::memset(extBuf, 0xCC, N);
		CRegionConverter().Int0001toExt0001(q);
		check("Region::Int0001toExt0001 thunk copies internal->external", std::memcmp(extBuf, intBuf, N) == 0);

		std::memset(extBuf, 0xCC, N);
		CWaveSeqConverter().Int0001toExt0001(q);
		check("WaveSeq::Int0001toExt0001 thunk copies internal->external", std::memcmp(extBuf, intBuf, N) == 0);

		// THUNK -> Ext0000toInt0000 (Ext direction).
		FillPattern(src, N, 0x50);
		std::memset(dst, 0xCC, N);
		CGlobalConverter().Ext0002toInt0002(p);
		check("Global::Ext0002toInt0002 thunk copies external->internal", std::memcmp(dst, src, N) == 0);
	}

	// -- no-op: CMOSSProgConverter::Ext0004toInt0005 must never touch the
	// destination buffer, regardless of source content.
	{
		unsigned char dst[16];
		std::memset(dst, 0xCC, sizeof dst);
		unsigned char src[16];
		FillPattern(src, sizeof src, 1);
		CConvertStorageParam p = MakeParam();
		p.m_internalBuf = dst;
		p.m_externalBuf = src;
		p.m_size = sizeof dst;
		CMOSSProgConverter().Ext0004toInt0005(p);
		bool untouched = true;
		for (unsigned i = 0; i < sizeof dst; ++i)
			if (dst[i] != 0xCC) untouched = false;
		check("MOSSProg::Ext0004toInt0005 real no-op leaves dst untouched", untouched);
	}

	// -- memcpy + ClipParams()/RemapParamProgramBankNo() family. ClipParams()
	// and CStorageMap::ShouldRemapV3Order() are unmodeled no-op/false stubs
	// (storage_converter_ext_stubs.h), so the only observable effect this
	// host test can assert is the memcpy itself, plus (for the PCM/MOSS pair)
	// that the +0xa80/+0xa81 bank/sub-bank bytes are left UNCHANGED by the
	// remap branch (since ShouldRemapV3Order() always reports false here).
	{
		const unsigned N = 64;
		unsigned char src[N], dst[N];
		CConvertStorageParam p = MakeParam();
		p.m_size = N;

		FillPattern(src, N, 0x60);
		std::memset(dst, 0xCC, N);
		p.m_internalBuf = dst; p.m_externalBuf = src;
		CDrumKitConverter().Ext0003toInt0003(p);
		check("DrumKit::Ext0003toInt0003 copies external->internal", std::memcmp(dst, src, N) == 0);

		FillPattern(src, N, 0x70);
		std::memset(dst, 0xCC, N);
		CWaveSeqConverter().Ext0001toInt0001(p);
		check("WaveSeq::Ext0001toInt0001 copies external->internal", std::memcmp(dst, src, N) == 0);

		// PCMProg/MOSSProg need a buffer big enough to reach +0xa80/+0xa81.
		const unsigned M = 0xa90;
		unsigned char *srcBig = new unsigned char[M];
		unsigned char *dstBig = new unsigned char[M];
		FillPattern(srcBig, M, 0x80);
		std::memset(dstBig, 0xCC, M);
		CConvertStorageParam q = MakeParam();
		q.m_internalBuf = dstBig; q.m_externalBuf = srcBig; q.m_size = M;

		CPCMProgConverter().Ext0005toInt0005(q);
		check("PCMProg::Ext0005toInt0005 copies external->internal", std::memcmp(dstBig, srcBig, M) == 0);
		check("PCMProg::Ext0005toInt0005 leaves bank byte as copied (no remap, stub reports false)",
		      dstBig[0xa80] == srcBig[0xa80]);
		check("PCMProg::Ext0005toInt0005 leaves sub-bank byte as copied (no remap, stub reports false)",
		      dstBig[0xa81] == srcBig[0xa81]);

		std::memset(dstBig, 0xCC, M);
		CMOSSProgConverter().Ext0005toInt0005(q);
		check("MOSSProg::Ext0005toInt0005 copies external->internal", std::memcmp(dstBig, srcBig, M) == 0);

		delete[] srcBig;
		delete[] dstBig;
	}

	// -- CGlobalConverter::Ext0001toInt0002 / Ext0000toInt0002: MIGRATE, fixed
	// legacy-format size (0x602c / 0x602a respectively), + 4 zeroed
	// migration-time fields at +0x607b/+0x607c/+0x607d(dword)/+0x6081(word),
	// masked afterward (0 & anything == 0, so the copied body's own content
	// there is irrelevant -- the fields must simply read back as 0).
	{
		const unsigned SRC_N = 0x602c;
		const unsigned DST_N = 0x6083; // must cover the tail fields past the copy
		unsigned char *src = new unsigned char[SRC_N];
		unsigned char *dst = new unsigned char[DST_N];
		FillPattern(src, SRC_N, 0x11);
		std::memset(dst, 0xCC, DST_N);

		CConvertStorageParam p = MakeParam();
		p.m_internalBuf = dst; p.m_externalBuf = src; p.m_size = SRC_N;
		CGlobalConverter().Ext0001toInt0002(p);

		check("Global::Ext0001toInt0002 copies the 0x602c legacy body",
		      std::memcmp(dst, src, SRC_N) == 0);
		check("Global::Ext0001toInt0002 zeroes +0x607b", dst[0x607b] == 0);
		check("Global::Ext0001toInt0002 zeroes +0x607c", dst[0x607c] == 0);
		check("Global::Ext0001toInt0002 zeroes +0x607d..+0x6081",
		      dst[0x607d] == 0 && dst[0x607e] == 0 && dst[0x607f] == 0 && dst[0x6080] == 0);
		check("Global::Ext0001toInt0002 zeroes +0x6081..+0x6083",
		      dst[0x6081] == 0 && dst[0x6082] == 0);

		std::memset(dst, 0xCC, DST_N);
		p.m_size = 0x602a;
		CGlobalConverter().Ext0000toInt0002(p);
		check("Global::Ext0000toInt0002 copies the 0x602a legacy body",
		      std::memcmp(dst, src, 0x602a) == 0);
		check("Global::Ext0000toInt0002 zeroes +0x607b", dst[0x607b] == 0);
		check("Global::Ext0000toInt0002 zeroes +0x607c", dst[0x607c] == 0);

		delete[] src;
		delete[] dst;
	}

	// -- CGEConverter::Int0000toExt0000 / CGETemplateConverter::Int0000toExt0000:
	// memcpy(external,internal,size) + a 4-byte big-endian item-code trailer
	// right after the copied region. CFileKge::GetItemCode() is an unmodeled
	// stub returning 0 (storage_converter_ext_stubs.h), so the only
	// independently-checkable fact is the trailer's VALUE (0, big-endian is
	// irrelevant for an all-zero value) and that the copy itself is correct
	// and the variantFlag adjustment doesn't corrupt the (unconditionally
	// re-read) memcpy args.
	{
		const unsigned N = 32;
		unsigned char intBuf[N];
		unsigned char extBuf[N + 4];
		FillPattern(intBuf, N, 0x90);
		std::memset(extBuf, 0xCC, sizeof extBuf);

		CConvertStorageParam p = MakeParam();
		p.m_internalBuf = intBuf; p.m_externalBuf = extBuf; p.m_size = N;
		p.m_variantFlag = 0;
		CGEConverter().Int0000toExt0000(p);
		check("GE::Int0000toExt0000 copies internal->external", std::memcmp(extBuf, intBuf, N) == 0);
		check("GE::Int0000toExt0000 appends 4-byte trailer",
		      extBuf[N] == 0 && extBuf[N + 1] == 0 && extBuf[N + 2] == 0 && extBuf[N + 3] == 0);

		std::memset(extBuf, 0xCC, sizeof extBuf);
		p.m_variantFlag = 1; // exercises the +0x20/-0x20 item-code-only adjustment path
		CGEConverter().Int0000toExt0000(p);
		check("GE::Int0000toExt0000 (variantFlag set) still copies internal->external unmodified",
		      std::memcmp(extBuf, intBuf, N) == 0);

		std::memset(extBuf, 0xCC, sizeof extBuf);
		p.m_variantFlag = 0;
		CGETemplateConverter().Int0000toExt0000(p);
		check("GETemplate::Int0000toExt0000 copies internal->external", std::memcmp(extBuf, intBuf, N) == 0);
		check("GETemplate::Int0000toExt0000 appends 4-byte trailer",
		      extBuf[N] == 0 && extBuf[N + 1] == 0 && extBuf[N + 2] == 0 && extBuf[N + 3] == 0);
	}

	// -- CSongControlConverter::Int0000toExt0000: variantFlag==0 -> plain
	// fixed-size (0x1490) memcpy; variantFlag!=0 -> Initialize()+
	// ApplyNonEventRelated() instead (both unmodeled no-op stubs), so the
	// destination must stay UNTOUCHED in that branch (independently
	// distinguishing the two real code paths).
	{
		const unsigned N = 0x1490;
		unsigned char *src = new unsigned char[N];
		unsigned char *dst = new unsigned char[N];
		FillPattern(src, N, 0x22);
		std::memset(dst, 0xCC, N);

		CConvertStorageParam p = MakeParam();
		p.m_internalBuf = src; p.m_externalBuf = dst; p.m_variantFlag = 0;
		CSongControlConverter().Int0000toExt0000(p);
		check("SongControl::Int0000toExt0000 (flag0) copies internal->external, 0x1490 bytes",
		      std::memcmp(dst, src, N) == 0);

		std::memset(dst, 0xCC, N);
		p.m_variantFlag = 1;
		CSongControlConverter().Int0000toExt0000(p);
		bool untouched = true;
		for (unsigned i = 0; i < N; ++i)
			if (dst[i] != 0xCC) untouched = false;
		check("SongControl::Int0000toExt0000 (flag1) takes the Initialize+ApplyNonEventRelated "
		      "path, does NOT memcpy (dst untouched by the no-op stubs)", untouched);

		delete[] src;
		delete[] dst;
	}

	// -- CRegionConverter::Ext0000toInt0001 / Int0000toExt0000: MIGRATE with a
	// real 0xb4-byte field-insertion/relocation shape.
	{
		const unsigned OLD_N = 0x7c; // old struct's own full extent (up to +0x78, 4B field)
		const unsigned NEW_N = 0x130; // new struct's own full extent (up to +0x12c, 4B field)
		unsigned char oldBuf[OLD_N];
		unsigned char newBuf[NEW_N];
		FillPattern(oldBuf, OLD_N, 0x33);
		std::memset(newBuf, 0xCC, NEW_N);

		CConvertStorageParam p = MakeParam();
		p.m_internalBuf = newBuf; p.m_externalBuf = oldBuf;
		CRegionConverter().Ext0000toInt0001(p);

		check("Region::Ext0000toInt0001 copies the common 0x68-byte header/body 1:1",
		      std::memcmp(newBuf, oldBuf, 0x68) == 0);
		check("Region::Ext0000toInt0001 zero-fills the inserted 0xb4-byte gap at +0x68",
		      newBuf[0x68] == 0 && newBuf[0x6c] == 0 && newBuf[0x11b] == 0);
		check("Region::Ext0000toInt0001 relocates old +0x68 dword -> new +0x11c",
		      std::memcmp(newBuf + 0x11c, oldBuf + 0x68, 4) == 0);
		check("Region::Ext0000toInt0001 relocates old +0x6c dword -> new +0x120",
		      std::memcmp(newBuf + 0x120, oldBuf + 0x6c, 4) == 0);
		check("Region::Ext0000toInt0001 relocates old +0x70 byte -> new +0x124",
		      newBuf[0x124] == oldBuf[0x70]);
		check("Region::Ext0000toInt0001 relocates old +0x71 byte -> new +0x125",
		      newBuf[0x125] == oldBuf[0x71]);
		check("Region::Ext0000toInt0001 relocates old +0x72 word -> new +0x126",
		      std::memcmp(newBuf + 0x126, oldBuf + 0x72, 2) == 0);
		check("Region::Ext0000toInt0001 relocates old +0x74 dword -> new +0x128",
		      std::memcmp(newBuf + 0x128, oldBuf + 0x74, 4) == 0);
		check("Region::Ext0000toInt0001 relocates old +0x78 dword -> new +0x12c",
		      std::memcmp(newBuf + 0x12c, oldBuf + 0x78, 4) == 0);

		// Mirror: Int0000toExt0000 must be the exact reverse.
		FillPattern(newBuf, NEW_N, 0x44);
		std::memset(oldBuf, 0xCC, OLD_N);
		CConvertStorageParam q = MakeParam();
		q.m_internalBuf = newBuf; q.m_externalBuf = oldBuf;
		CRegionConverter().Int0000toExt0000(q);

		check("Region::Int0000toExt0000 copies the common 0x68-byte header/body 1:1",
		      std::memcmp(oldBuf, newBuf, 0x68) == 0);
		check("Region::Int0000toExt0000 relocates new +0x11c dword -> old +0x68",
		      std::memcmp(oldBuf + 0x68, newBuf + 0x11c, 4) == 0);
		check("Region::Int0000toExt0000 relocates new +0x12c dword -> old +0x78",
		      std::memcmp(oldBuf + 0x78, newBuf + 0x12c, 4) == 0);
	}

	// -- CSongConverter::Ext0003toInt0003: memset(internal,0,0x3314) +
	// CopySongTimbreSet() (unmodeled no-op) + memcpy(internal+0x1e84,
	// external+0x1e84,0x1490) + ClipParams() (unmodeled no-op).
	{
		const unsigned N = 0x1e84 + 0x1490;
		unsigned char *src = new unsigned char[N];
		unsigned char *dst = new unsigned char[N];
		FillPattern(src, N, 0x55);
		std::memset(dst, 0xCC, N);

		CConvertStorageParam p = MakeParam();
		p.m_internalBuf = dst; p.m_externalBuf = src;
		CSongConverter().Ext0003toInt0003(p);

		bool headerZeroed = true;
		for (unsigned i = 0; i < 0x3314 && i < 0x1e84; ++i)
			if (dst[i] != 0) headerZeroed = false;
		check("Song::Ext0003toInt0003 zeroes the leading 0x3314-byte header region", headerZeroed);
		check("Song::Ext0003toInt0003 copies the +0x1e84..+0x1e84+0x1490 timbre-set block",
		      std::memcmp(dst + 0x1e84, src + 0x1e84, 0x1490) == 0);

		delete[] src;
		delete[] dst;
	}

	// -- CProgCombiSongCommonConverter::ConvertToCurrent: memcpy(dst,src,0x50c)
	// + Initialize() (unmodeled no-op) on the sub-object at dst+0x50c. Uses
	// raw byte buffers reinterpret_cast to the (intentionally opaque, never-
	// dereferenced) CProgCombiSongCommon/CProgCombiSongCommon0000 types.
	{
		const unsigned N = 0x50c;
		unsigned char src[N];
		unsigned char dst[N];
		FillPattern(src, N, 0x66);
		std::memset(dst, 0xCC, N);

		CProgCombiSongCommonConverter::ConvertToCurrent(
			reinterpret_cast<CProgCombiSongCommon *>(dst),
			reinterpret_cast<const CProgCombiSongCommon0000 *>(src));

		check("ProgCombiSongCommon::ConvertToCurrent copies 0x50c bytes",
		      std::memcmp(dst, src, N) == 0);
	}

	// -- CDrumKitConverter::Ext0002toInt0003: MIGRATE, RESOLVED 2026-07-28
	// follow-up (was deferred pending the ConvertPartTo0003 helper). Full-size
	// buffers: SRC_N (0x18 header + 128*0xac keys) == 0x5618, which is the
	// EXACT magic ValidateExt0002 checks for; DST_N (0x18 + 128*0x12c) ==
	// 0x9618, the exact magic ValidateExt0003 checks for -- strong independent
	// corroboration that the header/stride derivation is correct (these sizes
	// were derived purely from the loop bounds/strides seen in the
	// disassembly, not from the already-known ValidateExt magic constants).
	{
		const unsigned oldStride = 0xac, newStride = 0x12c;
		const unsigned oldPartOff[8] = { 0x18, 0x2a, 0x3c, 0x4e, 0x60, 0x72, 0x84, 0x96 };
		const unsigned newPartOff[8] = { 0x18, 0x3a, 0x5c, 0x7e, 0xa0, 0xc2, 0xe4, 0x106 };
		const unsigned SRC_N = 0x18 + 128u * oldStride;
		const unsigned DST_N = 0x18 + 128u * newStride;
		check("DrumKit::Ext0002toInt0003 derived SRC_N matches ValidateExt0002 magic 0x5618",
		      SRC_N == 0x5618);
		check("DrumKit::Ext0002toInt0003 derived DST_N matches ValidateExt0003 magic 0x9618",
		      DST_N == 0x9618);

		unsigned char *src = new unsigned char[SRC_N];
		unsigned char *dst = new unsigned char[DST_N];
		FillPattern(src, SRC_N, 0x11);
		std::memset(dst, 0xCC, DST_N);

		CConvertStorageParam p = MakeParam();
		p.m_internalBuf = dst; p.m_externalBuf = src;
		CDrumKitConverter().Ext0002toInt0003(p);

		check("DrumKit::Ext0002toInt0003 copies the 0x18-byte header",
		      std::memcmp(dst, src, 0x18) == 0);

		for (unsigned key = 0; key < 128; key += 127) { // spot-check first and last key
			// NOTE: oldPartOff[0]/newPartOff[0] == 0x18 already embeds the
			// header-skip baseline (ground truth's own literal `[edi+ebx+0x18]`
			// addressing) -- do NOT add 0x18 again here.
			const unsigned char *srcKey = src + key * oldStride;
			unsigned char *dstKey = dst + key * newStride;

			for (int part = 0; part < 8; part += 7) { // spot-check first and last part
				const unsigned char *sp = srcKey + oldPartOff[part];
				unsigned char *dp = dstKey + newPartOff[part];

				check("DrumKit::Ext0002toInt0003 InstPart dst+0x00 1-bit merge",
				      dp[0x00] == (static_cast<unsigned char>(0xCC) | (sp[0x00] & 0x01)));
				check("DrumKit::Ext0002toInt0003 InstPart inserts 'KORG'",
				      dp[0x01] == 'K' && dp[0x02] == 'O' && dp[0x03] == 'R' && dp[0x04] == 'G');
				check("DrumKit::Ext0002toInt0003 InstPart zeroes +0x05..+0x0c",
				      dp[0x05] == 0 && dp[0x08] == 0 && dp[0x09] == 0 && dp[0x0c] == 0);
				check("DrumKit::Ext0002toInt0003 InstPart inserts 'MS'+0",
				      dp[0x0d] == 'M' && dp[0x0e] == 'S' && dp[0x0f] == 0);
				check("DrumKit::Ext0002toInt0003 InstPart dst+0x10 <- src+0x01",
				      dp[0x10] == sp[0x01]);
				check("DrumKit::Ext0002toInt0003 InstPart dst+0x12..0x13 <- src+0x02..0x03",
				      dp[0x12] == sp[0x02] && dp[0x13] == sp[0x03]);
				check("DrumKit::Ext0002toInt0003 InstPart dst+0x14 <- src+0x04",
				      dp[0x14] == sp[0x04]);
				{
					unsigned char tmp = (static_cast<unsigned char>(0xCC) & 0xf0) | (sp[0x05] & 0x0f);
					unsigned char expect = (tmp & 0x7f) | (sp[0x05] & 0x80);
					check("DrumKit::Ext0002toInt0003 InstPart dst+0x15 bitfield merge",
					      dp[0x15] == expect);
				}
				check("DrumKit::Ext0002toInt0003 InstPart dst+0x16..0x20 <- src+0x06..0x10",
				      std::memcmp(dp + 0x16, sp + 0x06, 0x0b) == 0);
			}

			check("DrumKit::Ext0002toInt0003 trailing 0x18-byte block copy",
			      std::memcmp(dstKey + 0x120 + 8, srcKey + 0xa0 + 8, 0x18) == 0);
		}

		delete[] src;
		delete[] dst;
	}

	// -- CWaveSeqConverter::Ext0000toInt0001: MIGRATE, RESOLVED 2026-07-28
	// follow-up (same "KORG"/"MS" constants as ConvertPartTo0003 above).
	// SRC_N (0x28 + 64*0x12) == 0x4a8 (ValidateExt0000's own magic), DST_N
	// (0x28 + 64*0x22) == 0x8a8 (ValidateExt0001's own magic) -- same
	// independent corroboration as the DrumKit case above.
	{
		const unsigned oldStride = 0x12, newStride = 0x22;
		const unsigned SRC_N = 0x28 + 64u * oldStride;
		const unsigned DST_N = 0x28 + 64u * newStride;
		check("WaveSeq::Ext0000toInt0001 derived SRC_N matches ValidateExt0000 magic 0x4a8",
		      SRC_N == 0x4a8);
		check("WaveSeq::Ext0000toInt0001 derived DST_N matches ValidateExt0001 magic 0x8a8",
		      DST_N == 0x8a8);

		unsigned char *src = new unsigned char[SRC_N];
		unsigned char *dst = new unsigned char[DST_N];
		FillPattern(src, SRC_N, 0x21);
		std::memset(dst, 0xCC, DST_N);

		CConvertStorageParam p = MakeParam();
		p.m_internalBuf = dst; p.m_externalBuf = src;
		CWaveSeqConverter().Ext0000toInt0001(p);

		check("WaveSeq::Ext0000toInt0001 copies the 0x28-byte header",
		      std::memcmp(dst, src, 0x28) == 0);

		for (unsigned step = 0; step < 64; step += 63) { // spot-check first and last step
			const unsigned char *s = src + 0x28 + step * oldStride;
			unsigned char *d = dst + 0x28 + step * newStride;

			check("WaveSeq::Ext0000toInt0001 step dst+0x00 2-bit merge",
			      d[0x00] == (static_cast<unsigned char>(0xCC) | (s[0x00] & 0x03)));
			check("WaveSeq::Ext0000toInt0001 step inserts 'KORG'/'MS'",
			      d[0x01] == 'K' && d[0x04] == 'G' && d[0x0d] == 'M' && d[0x0e] == 'S' && d[0x0f] == 0);
			check("WaveSeq::Ext0000toInt0001 step dst+0x10 <- src+0x01",
			      d[0x10] == s[0x01]);
			check("WaveSeq::Ext0000toInt0001 step dst+0x12..0x13 <- src+0x02..0x03",
			      d[0x12] == s[0x02] && d[0x13] == s[0x03]);
			check("WaveSeq::Ext0000toInt0001 step dst+0x14 <- src+0x04",
			      d[0x14] == s[0x04]);
			{
				unsigned char tmp = (static_cast<unsigned char>(0xCC) & 0xf0) | (s[0x05] & 0x0f);
				unsigned char expect = (tmp & 0x7f) | (s[0x05] & 0x80);
				check("WaveSeq::Ext0000toInt0001 step dst+0x15 bitfield merge", d[0x15] == expect);
			}
			check("WaveSeq::Ext0000toInt0001 step dst+0x16..0x17 <- src+0x06..0x07",
			      d[0x16] == s[0x06] && d[0x17] == s[0x07]);
			check("WaveSeq::Ext0000toInt0001 step dst+0x18..0x1b <- src+0x08..0x0b",
			      std::memcmp(d + 0x18, s + 0x08, 4) == 0);
			check("WaveSeq::Ext0000toInt0001 step dst+0x1d..0x21 <- src+0x0d..0x11",
			      std::memcmp(d + 0x1d, s + 0x0d, 5) == 0);
		}

		delete[] src;
		delete[] dst;
	}

	// -- CProgAncestorConverter::ConvertToCurrent (both overloads): MIGRATE,
	// RESOLVED 2026-07-28 follow-up (was deliberately out of scope, not
	// blocked). Common-block delegation + 3 fixed-size +0xc-shifted memcpy
	// blocks; CProgDrumTrackData::Initialize() (unmodeled no-op stub) is not
	// independently checkable.
	{
		const unsigned N = 0xa80;
		unsigned char *src = new unsigned char[N];
		unsigned char *dst = new unsigned char[N];
		FillPattern(src, N, 0x77);
		std::memset(dst, 0xCC, N);

		CProgAncestorConverter::ConvertToCurrent(
			reinterpret_cast<CProgAncestor *>(dst),
			reinterpret_cast<const CProgAncestor0000 *>(src));

		check("ProgAncestor::ConvertToCurrent(0000) copies the 0x50c common block",
		      std::memcmp(dst, src, 0x50c) == 0);
		check("ProgAncestor::ConvertToCurrent(0000) migrates block1 (0x50c->0x518, 0x1fe)",
		      std::memcmp(dst + 0x518, src + 0x50c, 0x1fe) == 0);
		check("ProgAncestor::ConvertToCurrent(0000) migrates block2 (0x70a->0x716, 0x2e8)",
		      std::memcmp(dst + 0x716, src + 0x70a, 0x2e8) == 0);
		check("ProgAncestor::ConvertToCurrent(0000) migrates block3 (0x9f2->0x9fe, 0x82)",
		      std::memcmp(dst + 0x9fe, src + 0x9f2, 0x82) == 0);

		std::memset(dst, 0xCC, N);
		CProgAncestorConverter::ConvertToCurrent(
			reinterpret_cast<CProgAncestor *>(dst),
			reinterpret_cast<const CProgAncestor0003OASYS *>(src));

		check("ProgAncestor::ConvertToCurrent(0003OASYS) copies the 0x50c common block",
		      std::memcmp(dst, src, 0x50c) == 0);
		check("ProgAncestor::ConvertToCurrent(0003OASYS) migrates block1 (0x50c->0x518, 0x1fe)",
		      std::memcmp(dst + 0x518, src + 0x50c, 0x1fe) == 0);
		check("ProgAncestor::ConvertToCurrent(0003OASYS) migrates block2 (0x70a->0x716, 0x2e8)",
		      std::memcmp(dst + 0x716, src + 0x70a, 0x2e8) == 0);
		check("ProgAncestor::ConvertToCurrent(0003OASYS) migrates block3 (0x9f2->0x9fe, 0x82)",
		      std::memcmp(dst + 0x9fe, src + 0x9f2, 0x82) == 0);

		delete[] src;
		delete[] dst;
	}
}

// ---- CProgConverter::Close() forwarding + dtors ----
// Load()/Save() are NOT reconstructed this pass (prog_converter.h), so this
// covers only Close() (real) and the dtor pair (real, pure vptr pokes).

// Test-only subclass exposing CProgConverter's protected layout, same
// pattern as verify/test_scsi_driver_base.cpp's TestScsi.
class TestProgConverter : public CProgConverter {
public:
	TestProgConverter()
	{
		m_vptr0 = m_base4 = m_base8 = 0;
		m_pFormatConverter = 0;
		std::memset(&m_storedParam, 0, sizeof m_storedParam);
	}
	using CProgConverter::m_vptr0;
	using CProgConverter::m_base4;
	using CProgConverter::m_base8;
	using CProgConverter::m_pFormatConverter;
	using CProgConverter::m_storedParam;
};

void TestProgConverterCloseAndDtor()
{
	CStorageConverterBase sub;

	{
		TestProgConverter pc;
		// m_pFormatConverter == NULL: Close() must be a safe no-op.
		pc.Close();
		check("ProgConverter: null m_pFormatConverter -> Close() safe no-op",
		      pc.m_pFormatConverter == 0);
	}
	{
		TestProgConverter pc;
		pc.m_pFormatConverter = &sub;
		pc.Close();
		check("ProgConverter::Close() nulls m_pFormatConverter", pc.m_pFormatConverter == 0);
		// Idempotent: a 2nd Close() call must stay a safe no-op.
		pc.Close();
		check("ProgConverter::Close() idempotent (2nd call safe)", pc.m_pFormatConverter == 0);
	}
	{
		// ~CProgConverter() (D1): resets all 3 raw slots to the same literal
		// value (0x08fcc9c8, &vtable_for_CStorageConverterBase+8). Allocated
		// with `new` and explicitly dtor'd + freed via bare `operator
		// delete` (not a `delete` expression) so the reconstructed dtor body
		// is invoked exactly once, not twice.
		TestProgConverter *pc = new TestProgConverter();
		pc->~CProgConverter();
		void *const expect = reinterpret_cast<void *>(0x08fcc9c8ul);
		check("ProgConverter dtor (D1) resets m_vptr0", pc->m_vptr0 == expect);
		check("ProgConverter dtor (D1) resets m_base4", pc->m_base4 == expect);
		check("ProgConverter dtor (D1) resets m_base8", pc->m_base8 == expect);
		operator delete(pc);
	}
	{
		// DeletingDtor() (D0): same 3 resets, then frees the object itself
		// (real ground truth: HAL_DisableInterrupts()/free(this)/
		// HAL_EnableInterrupts()). Placement-new over raw malloc'd storage so
		// DeletingDtor()'s own internal std::free(this) is the only free --
		// nothing double-frees or leaks under a plain host build.
		void *mem = std::malloc(sizeof(TestProgConverter));
		TestProgConverter *pc = new (mem) TestProgConverter();
		pc->DeletingDtor(); // frees `mem` internally
		check("ProgConverter DeletingDtor (D0) ran without crashing", true);
	}
}

} // namespace

int main()
{
	TestBase();
	TestSiblings();
	TestProgConverterCloseAndDtor();
	TestExtIntPayload();

	std::printf("%d checks, %d failed\n", g_checks, g_failed);
	return g_failed != 0 ? 1 : 0;
}
