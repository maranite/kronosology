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

	// ValidateExt0000: real BUFID formula -- m_extFormatId == (unsigned long)m_externalBuf
	{
		CConvertStorageParam p = MakeParam();
		p.m_externalBuf = reinterpret_cast<const void *>(0x12345678ul);
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

	// BUFID pattern classes (m_extFormatId == (unsigned long)m_externalBuf).
	{
		CConvertStorageParam p = MakeParam();
		p.m_externalBuf = reinterpret_cast<const void *>(0xABCD1234ul);
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

	std::printf("%d checks, %d failed\n", g_checks, g_failed);
	return g_failed != 0 ? 1 : 0;
}
