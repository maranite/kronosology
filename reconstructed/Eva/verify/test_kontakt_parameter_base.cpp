/*
 * test_kontakt_parameter_base.cpp  -  host-side known-answer test for
 * CKontaktParameter / CKontaktIndexedParameter / CKontaktDynamicParameter
 * (src/convert/kontakt_parameter_base.cpp), 2026-07-28.
 *
 * Exercises the real "name"/"value" attribute-pair dispatch logic documented
 * in kontakt_parameter_base.h. NOTE: the real "name" case calls libxml2's
 * xmlStrdup(), which this host build's libxml2_host_stubs.cpp (see that
 * file's own header -- same situation test_kontakt_xml.cpp's own tests
 * already flag) stubs out to always return NULL, since this host has no
 * i386 libxml2 to link a real one against. Feeding AddAttribute() a real
 * "name" attribute end-to-end would therefore leave mAllocatedName NULL and
 * crash the very next StringIndex(mList, NULL) call -- not a bug in the
 * reconstruction, a host-only test-harness limitation. Worked around here
 * exactly like a real "name" attribute already landed would leave things:
 * each testable subclass exposes a tiny test-only SeedName() setter that
 * plants a real string directly into the (protected) mAllocatedName field,
 * then AddAttribute() is driven with ONLY the "value" attribute -- the real
 * production dispatch logic under test (StringIndex resolution + virtual
 * call) is identical either way, only how mAllocatedName got populated
 * differs.
 */

#include <cstdio>
#include <cstring>

#include "kontakt_parameter_base.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

static const char *kFields[] = { "volume", "pan", "outRouting_", 0 };

class CTestableParameter : public CKontaktParameter {
public:
	CTestableParameter() : CKontaktParameter(kFields), lastIndex(999), lastValue(0) {}
	virtual void UnidentifiedPureVirtual_slot8() {}
	virtual void AddParameter(unsigned int index, const unsigned char *value)
	{
		lastIndex = index;
		lastValue = value;
	}
	void SeedName(const char *n) { mAllocatedName = (unsigned char *)n; }
	unsigned int lastIndex;
	const unsigned char *lastValue;
};

class CTestableIndexedParameter : public CKontaktIndexedParameter {
public:
	CTestableIndexedParameter() : CKontaktIndexedParameter(kFields), lastIndex(999), lastSuffix(999), lastValue(0) {}
	virtual void UnidentifiedPureVirtual_slot8() {}
	virtual void AddIndexedParameter(unsigned int index, unsigned int suffix, const unsigned char *value)
	{
		lastIndex = index;
		lastSuffix = suffix;
		lastValue = value;
	}
	void SeedName(const char *n) { mAllocatedName = (unsigned char *)n; }
	unsigned int lastIndex, lastSuffix;
	const unsigned char *lastValue;
};

static const char *kDynFields[] = { "sourceText", "persistent_var_", 0 };

class CTestableDynamicParameter : public CKontaktDynamicParameter {
public:
	CTestableDynamicParameter() : CKontaktDynamicParameter(kDynFields), lastIndex(999), lastValue(0) { lastSuffix[0] = 0; }
	virtual void UnidentifiedPureVirtual_slot8() {}
	virtual void AddDynamicParameter(unsigned int index, const char *suffix, const unsigned char *value)
	{
		lastIndex = index;
		strncpy(lastSuffix, suffix, sizeof(lastSuffix) - 1);
		lastSuffix[sizeof(lastSuffix) - 1] = 0;
		lastValue = value;
	}
	void SeedName(const char *n) { mAllocatedName = (unsigned char *)n; }
	unsigned int lastIndex;
	char lastSuffix[0x20];
	const unsigned char *lastValue;
};

int main(void)
{
	printf("CKontaktParameter/IndexedParameter/DynamicParameter known-answer test\n");
	printf("========================================================================\n");

	printf("[1] CKontaktParameter::AddAttribute -- \"value\" dispatch (see file header re: \"name\"/xmlStrdup)\n");
	{
		CTestableParameter p;
		p.SeedName("pan");
		p.AddAttribute(0, (const unsigned char *)"value", (const unsigned char *)"0.5");
		check("stashed name \"pan\" + \"value\"=0.5 -> AddParameter(1, \"0.5\")",
		      p.lastIndex == 1 && p.lastValue != 0 && strcmp((const char *)p.lastValue, "0.5") == 0);
	}
	{
		CTestableParameter p;
		p.AddAttribute(0, (const unsigned char *)"somethingElse", (const unsigned char *)"x");
		check("attribute name other than name/value is a no-op", p.lastIndex == 999);
	}
	{
		CTestableParameter p;
		p.SeedName("unknownField");
		p.AddAttribute(0, (const unsigned char *)"value", (const unsigned char *)"1");
		check("unresolvable field name -> no dispatch", p.lastIndex == 999);
	}
	{
		CTestableParameter p;
		p.SeedName("pan");
		p.CKontaktParameter::AddParameter((const unsigned char *)"0.75");
		check("AddParameter(value) overload re-resolves the stashed name", p.lastIndex == 1 && strcmp((const char *)p.lastValue, "0.75") == 0);
	}
	{
		/* the "name" case itself: only checkable as "doesn't crash and
		 * doesn't dispatch anything" on this host (see file header) --
		 * real behavior (xmlStrdup + store) is exercised in
		 * test_kontakt_xml.cpp's own AddAttribute-adjacent coverage. */
		CTestableParameter p;
		p.AddAttribute(0, (const unsigned char *)"name", (const unsigned char *)"pan");
		check("\"name\" attribute alone dispatches nothing yet", p.lastIndex == 999);
	}

	printf("[2] CKontaktIndexedParameter::AddAttribute -- numeric-suffix dispatch\n");
	{
		CTestableIndexedParameter p;
		p.SeedName("outRouting_3");
		p.AddAttribute(0, (const unsigned char *)"value", (const unsigned char *)"7");
		check("\"outRouting_3\" -> index 2, suffix 3, value \"7\"",
		      p.lastIndex == 2 && p.lastSuffix == 3 && strcmp((const char *)p.lastValue, "7") == 0);
	}
	{
		CTestableIndexedParameter p;
		p.SeedName("volume");
		p.AddAttribute(0, (const unsigned char *)"value", (const unsigned char *)"1.0");
		check("exact match (no numeric suffix) -> index 0, suffix 0",
		      p.lastIndex == 0 && p.lastSuffix == 0);
	}

	printf("[3] CKontaktDynamicParameter::AddAttribute -- text-suffix dispatch\n");
	{
		CTestableDynamicParameter p;
		p.SeedName("persistent_var_ABC"); /* list entry itself is "persistent_var_" -- suffix follows the underscore */
		p.AddAttribute(0, (const unsigned char *)"value", (const unsigned char *)"42");
		check("\"persistent_var_ABC\" -> index 1, suffix \"ABC\"",
		      p.lastIndex == 1 && strcmp(p.lastSuffix, "ABC") == 0 && strcmp((const char *)p.lastValue, "42") == 0);
	}

	printf("[4] base default virtuals (unresolved sibling) -- no-op, doesn't crash\n");
	{
		CTestableParameter p;
		/* explicit scope to bypass the test override and hit the real
		 * base class's own 1-byte no-op default body */
		p.CKontaktParameter::AddParameter(0, (const unsigned char *)"ignored");
		check("CKontaktParameter::AddParameter default completes", true);
	}

	printf("\n%s\n", g_fail == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
	return g_fail == 0 ? 0 : 1;
}
