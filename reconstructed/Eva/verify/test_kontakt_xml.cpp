/*
 * test_kontakt_xml.cpp  -  host-side known-answer test for CKontaktXml
 * (src/convert/kontakt_xml.cpp), the Kontakt-import class-inventory sweep,
 * 2026-07-28.
 *
 * Covers every pure/leaf method reconstructed this pass -- the string/value
 * parsing helpers and StateString(). ProcessNode/ProcessNodes/Parse/SkipNode/
 * AddObject call real libxml2 xmlTextReader* APIs and are NOT exercised here:
 * this host build only has amd64 libxml2 available, not the -m32 build this
 * project targets (same "no cross-arch lib available for host KAT" situation
 * as any other externally-linked dependency in this project).
 */

#include <cstdio>
#include <cstring>

#include "kontakt_xml.h"

/* The libxml2 xmlTextReader* API that kontakt_xml.cpp's
 * ProcessNode/ProcessNodes/Parse/SkipNode/AddObject call into is stubbed out
 * (inertly) in src/convert/libxml2_host_stubs.cpp -- this host has no i386
 * (-m32) libxml2 to link against, only amd64. That stub TU is part of the
 * normal build's $(OBJ) set, so it's already linked into this test binary by
 * the Makefile; see that file for why. None of the checks below reach those
 * methods anyway -- see this file's own header comment. */

/* CKontaktXml declares one pure virtual (vtable slot+0x8, unidentified this
 * pass -- see kontakt_xml.h) so it can't be instantiated directly; this
 * test-only stub supplies a no-op override purely so test [11] below can
 * exercise the base class's own default AddAttribute()/AddObject() bodies
 * through a real instance. */
class CTestableKontaktXml : public CKontaktXml {
public:
	virtual void UnidentifiedPureVirtual_slot8() {}
};

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main(void)
{
	printf("CKontaktXml known-answer test\n");
	printf("==============================\n");

	printf("[1] StateString()\n");
	check("eOutside -> \"Outside\"", strcmp(CKontaktXml::StateString(CKontaktXml::eOutside), "Outside") == 0);
	check("eInside -> \"Inside\"", strcmp(CKontaktXml::StateString(CKontaktXml::eInside), "Inside") == 0);
	check("eDone -> \"Done\"", strcmp(CKontaktXml::StateString(CKontaktXml::eDone), "Done") == 0);
	check("out-of-range -> \"????\"", strcmp(CKontaktXml::StateString((CKontaktXml::KontaktState)99), "????") == 0);

	printf("[2] StringIndex(list, name) -- exact match\n");
	static const char *kList[] = { "Group", "Zone", "Sample", 0 };
	check("exact match \"Zone\" -> 1", CKontaktXml::StringIndex(kList, (const unsigned char *)"Zone") == 1);
	check("case-insensitive \"GROUP\" -> 0", CKontaktXml::StringIndex(kList, (const unsigned char *)"GROUP") == 0);
	check("no match -> -1", CKontaktXml::StringIndex(kList, (const unsigned char *)"Nope") == -1);
	static const char *kEmptyList[] = { 0 };
	check("empty (terminator-only) list -> -1", CKontaktXml::StringIndex(kEmptyList, (const unsigned char *)"x") == -1);

	printf("[3] StringIndex(list, name, unsigned int&) -- prefix + numeric suffix\n");
	unsigned int suffix = 999;
	check("\"Group3\" -> index 0, suffix 3",
	      CKontaktXml::StringIndex(kList, (const unsigned char *)"Group3", suffix) == 0 && suffix == 3);
	suffix = 999;
	check("\"Zone\" exact (no suffix) -> index 1, suffix untouched-from-0-init",
	      CKontaktXml::StringIndex(kList, (const unsigned char *)"Zone", suffix) == 1 && suffix == 0);
	suffix = 999;
	check("\"GroupX\" (non-numeric suffix) -> -1", CKontaktXml::StringIndex(kList, (const unsigned char *)"GroupX", suffix) == -1);
	suffix = 999;
	check("\"Sample12\" -> index 2, suffix 12",
	      CKontaktXml::StringIndex(kList, (const unsigned char *)"Sample12", suffix) == 2 && suffix == 12);

	printf("[4] StringIndex(list, name, char*, unsigned int) -- prefix + text suffix\n");
	char textSuffix[32];
	check("\"GroupAbc\" -> index 0, suffix \"Abc\"",
	      CKontaktXml::StringIndex(kList, (const unsigned char *)"GroupAbc", textSuffix, sizeof(textSuffix)) == 0
	      && strcmp(textSuffix, "Abc") == 0);
	check("\"Zone\" exact -> index 1, suffix stays \"\"",
	      CKontaktXml::StringIndex(kList, (const unsigned char *)"Zone", textSuffix, sizeof(textSuffix)) == 1
	      && textSuffix[0] == 0);

	printf("[5] StringsEqual() / BooleanValue()\n");
	check("StringsEqual case-insensitive match", CKontaktXml::StringsEqual((const unsigned char *)"Foo", "foo"));
	check("StringsEqual mismatch", !CKontaktXml::StringsEqual((const unsigned char *)"Foo", "bar"));
	check("BooleanValue(\"yes\") == true", CKontaktXml::BooleanValue((const unsigned char *)"yes") == true);
	check("BooleanValue(\"YES\") == true (case-insensitive)", CKontaktXml::BooleanValue((const unsigned char *)"YES") == true);
	check("BooleanValue(\"1\") == true", CKontaktXml::BooleanValue((const unsigned char *)"1") == true);
	check("BooleanValue(\"no\") == false", CKontaktXml::BooleanValue((const unsigned char *)"no") == false);
	check("BooleanValue(\"0\") == false", CKontaktXml::BooleanValue((const unsigned char *)"0") == false);
	check("BooleanValue(\"whatever\") == false (real: catch-all)", CKontaktXml::BooleanValue((const unsigned char *)"whatever") == false);

	printf("[6] Unsigned/Signed/FloatValue()\n");
	check("UnsignedValue(\"42\") == 42", CKontaktXml::UnsignedValue((const unsigned char *)"42") == 42);
	check("SignedValue(\"-7\") == -7", CKontaktXml::SignedValue((const unsigned char *)"-7") == -7);
	float f = CKontaktXml::FloatValue((const unsigned char *)"3.5");
	check("FloatValue(\"3.5\") == 3.5", f > 3.499f && f < 3.501f);

	printf("[7] VolumeLength/DirectoryLength/FileLength -- packed-record readers\n");
	{
		/* VolumeLength/DirectoryLength: skip 1 byte, read 3-digit decimal, pos += 4 */
		const unsigned char packed[] = { 'x', '0', '4', '2', 'Y', 'Y', 'Y' };
		unsigned int pos = 0;
		unsigned int v = CKontaktXml::VolumeLength(packed, pos);
		check("VolumeLength reads \"042\" == 42", v == 42);
		check("VolumeLength advances pos by 4", pos == 4);

		pos = 0;
		v = CKontaktXml::DirectoryLength(packed, pos);
		check("DirectoryLength (identical body) reads \"042\" == 42", v == 42);
		check("DirectoryLength advances pos by 4", pos == 4);
	}
	{
		/* FileLength: skip 6 bytes, read 3-digit decimal, pos += 12 */
		const unsigned char packed[] = { 'a', 'b', 'c', 'd', 'e', 'f', '1', '2', '3', 'Z', 'Z', 'Z' };
		unsigned int pos = 0;
		unsigned int v = CKontaktXml::FileLength(packed, pos);
		check("FileLength reads \"123\" == 123", v == 123);
		check("FileLength advances pos by 12", pos == 12);
	}

	printf("[8] Append() -- bounded copy-and-concatenate\n");
	{
		char dest[16];
		dest[0] = 0;
		const unsigned char src[] = "Hello, World!";
		unsigned int pos = 0;
		CKontaktXml::Append(src, pos, 5, dest, sizeof(dest));
		check("Append first 5 bytes (\"Hello\")", strcmp(dest, "Hello") == 0);
		check("Append advances pos by len (5)", pos == 5);
		CKontaktXml::Append(src, pos, 2, dest, sizeof(dest));
		check("Append concatenates next 2 bytes (\", \")", strcmp(dest, "Hello, ") == 0);
		check("Append advances pos again (7)", pos == 7);
	}

	printf("[9] AbsolutePath()\n");
	{
		char outBuf[64];
		CKontaktXml::AbsolutePath("/a/b/base.txt", "rel.txt", outBuf, sizeof(outBuf));
		check("relative path resolved against base's directory", strcmp(outBuf, "/a/b/rel.txt") == 0);

		CKontaktXml::AbsolutePath("/a/b/dir/", "rel.txt", outBuf, sizeof(outBuf));
		check("base already ending in '/' -> straight concat", strcmp(outBuf, "/a/b/dir/rel.txt") == 0);

		CKontaktXml::AbsolutePath("/a/b/base.txt", "/already/abs.txt", outBuf, sizeof(outBuf));
		check("already-absolute rel is used as-is", strcmp(outBuf, "/already/abs.txt") == 0);
	}

	printf("[10] RemoveNameExtension()\n");
	{
		char name[32];
		strcpy(name, "Sample.wav");
		CKontaktXml::RemoveNameExtension(name, 0);
		check("\"Sample.wav\" -> \"Sample\"", strcmp(name, "Sample") == 0);

		strcpy(name, "NoExtension");
		CKontaktXml::RemoveNameExtension(name, 0);
		check("no '.' present -> unchanged", strcmp(name, "NoExtension") == 0);
	}

	printf("[11] AddAttribute() default -- no-op\n");
	{
		/* Not independently checkable beyond "doesn't crash" -- real body is
		 * a plain `ret`. */
		CTestableKontaktXml x;
		x.AddAttribute(0, (const unsigned char *)"n", (const unsigned char *)"v");
		check("default AddAttribute() no-op completes", true);
	}

	printf("\n%s\n", g_fail == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
	return g_fail == 0 ? 0 : 1;
}
