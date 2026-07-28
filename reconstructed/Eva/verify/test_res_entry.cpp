/*
 * test_res_entry.cpp  -  host-side known-answer test for
 * STriplet/CResInfo/CResEntry/CResEntryEx (src/base/res_entry.cpp). See
 * include/res_entry.h for full ground-truth provenance.
 *
 * A fake CSystemApi vtable is installed (Api+0x94 soft-assert) so the raw calls
 * this code makes are exercised for real, same convention as
 * test_partition_table.cpp/test_tempo.cpp.
 */

#include <cstdio>
#include <cstring>

#include "res_entry.h"
#include "system_api.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

extern CSystemApi *Api;

struct ResEntryTestHooks {
	static unsigned Extra(const CResEntryEx &e) { return e.mExtra; }
	static unsigned short Index(const CResEntry &e) { return e.mIndex; }
	static int Pos(const CResEntry &e) { return e.mPos; }
	static int Size(const CResEntry &e) { return e.mSize; }
	static const CResInfo &Info(const CResEntry &e) { return e.mInfo; }
};

static int g_assertCount;

extern "C" void FakeApiAssert(void *, const char *, const char *file, int line)
{
	g_assertCount++;
	printf("      (soft-assert fired: %s:%d)\n", file, line);
}

static void *g_fakeVtable[0x94 / 4 + 1];
struct FakeApiObj { void *vtbl; } g_fakeApiObj;

static void InstallFakeApi()
{
	for (unsigned i = 0; i < sizeof(g_fakeVtable) / sizeof(g_fakeVtable[0]); ++i)
		g_fakeVtable[i] = 0;
	g_fakeVtable[0x94 / 4] = (void *)FakeApiAssert;
	g_fakeApiObj.vtbl = g_fakeVtable;
	Api = (CSystemApi *)&g_fakeApiObj;
}

int main()
{
	InstallFakeApi();

	printf("STriplet/CResInfo/CResEntry/CResEntryEx known-answer test\n");
	printf("===========================================================\n");

	printf("[1] CResInfo defaults + name handling\n");
	{
		CResInfo info;
		check("mResName all zero", info.mResName[0] == 0 && info.mResName[17] == 0);
		check("mTail[0..4] == 0xff", info.mTail[0] == 0xff && info.mTail[4] == 0xff);
		check("mTail[5] == 0", info.mTail[5] == 0);
		check("SizeOf() == 0x18", CResInfo::SizeOf() == 0x18);

		info.SetResName("HelloWorldThisIsLong");  /* > 17 chars, must truncate+NUL */
		check("SetResName truncates to 17 + NUL", strlen(info.mResName) == 17 &&
			info.mResName[17] == 0);
		check("SetResName content", strncmp(info.mResName, "HelloWorldThisIsL", 17) == 0);

		info.ResetResName();
		check("ResetResName zeroes name", info.mResName[0] == 0);
	}

	printf("[2] CResInfo Serialize/Deserialize round trip\n");
	{
		unsigned char buf[0x18];
		for (int i = 0; i < 0x18; ++i)
			buf[i] = 0;
		strcpy((char *)buf, "SampleBank01");
		buf[0x12] = 0x11; buf[0x13] = 0x22; buf[0x14] = 0x33;
		buf[0x15] = 0x44; buf[0x16] = 0x55; buf[0x17] = 0x66;

		CResInfo info;
		info.Deserialize(buf);
		check("Deserialize name", strcmp(info.mResName, "SampleBank01") == 0);
		check("Deserialize mTail[0..5]",
			info.mTail[0] == 0x11 && info.mTail[1] == 0x22 && info.mTail[2] == 0x33 &&
			info.mTail[3] == 0x44 && info.mTail[4] == 0x55 && info.mTail[5] == 0x66);

		unsigned char out[0x18];
		std::memset(out, 0xcc, sizeof(out));
		info.Serialize(out);
		check("Serialize round trip", std::memcmp(out, buf, 0x18) == 0);

		CResInfo fromBuf(buf);
		check("ctor(buf) matches Deserialize", std::memcmp(&fromBuf, &info, sizeof(CResInfo)) == 0);
	}

	printf("[3] CResEntry ctor overloads + STriplet/flag mapping\n");
	{
		STriplet id;
		id.b0 = 1; id.b1 = 2; id.b2 = 3;

		CResEntry a(id, "Bank01", (unsigned char)0xaa, (unsigned char)0xbb, 100, 200);
		check("mInfo name", strcmp(ResEntryTestHooks::Info(a).mResName, "Bank01") == 0);
		check("id -> mTail[0..2]", ResEntryTestHooks::Info(a).mTail[0] == 1 &&
			ResEntryTestHooks::Info(a).mTail[1] == 2 && ResEntryTestHooks::Info(a).mTail[2] == 3);
		check("flags -> mTail[3..4]", ResEntryTestHooks::Info(a).mTail[3] == 0xaa &&
			ResEntryTestHooks::Info(a).mTail[4] == 0xbb);
		check("mIndex sentinel 0xffff", ResEntryTestHooks::Index(a) == 0xffff);
		check("mPos/mSize set", ResEntryTestHooks::Pos(a) == 100 && ResEntryTestHooks::Size(a) == 200);

		CResEntry b(id, "Bank02", (unsigned short)42, (unsigned char)0xcc, (unsigned char)0xdd);
		check("mIndex set from ushort overload", ResEntryTestHooks::Index(b) == 42);
		check("mPos/mSize sentinel -1", ResEntryTestHooks::Pos(b) == -1 && ResEntryTestHooks::Size(b) == -1);
		check("flags overload2 -> mTail[3..4]", ResEntryTestHooks::Info(b).mTail[3] == 0xcc &&
			ResEntryTestHooks::Info(b).mTail[4] == 0xdd);
	}

	printf("[4] CResEntry Reset/Copy/operator=\n");
	{
		STriplet id; id.b0 = 9; id.b1 = 8; id.b2 = 7;
		CResEntry src(id, "Src", (unsigned char)1, (unsigned char)2, 10, 20);

		CResEntry dst(id, "Placeholder", (unsigned char)0, (unsigned char)0, 0, 0);
		dst = src;
		check("operator= copies name", strcmp(ResEntryTestHooks::Info(dst).mResName, "Src") == 0);
		check("operator= copies id", ResEntryTestHooks::Info(dst).mTail[0] == 9 &&
			ResEntryTestHooks::Info(dst).mTail[2] == 7);
		check("operator= copies mPos/mSize", ResEntryTestHooks::Pos(dst) == 10 &&
			ResEntryTestHooks::Size(dst) == 20);

		dst.Reset();
		check("Reset sentinels mIndex", ResEntryTestHooks::Index(dst) == 0xffff);
		check("Reset sentinels mPos/mSize", ResEntryTestHooks::Pos(dst) == -1 &&
			ResEntryTestHooks::Size(dst) == -1);
		check("Reset clears name", ResEntryTestHooks::Info(dst).mResName[0] == 0);
		check("Reset fills mTail[0..4]=0xff",
			ResEntryTestHooks::Info(dst).mTail[0] == 0xff && ResEntryTestHooks::Info(dst).mTail[4] == 0xff);

		CResEntry copyCtor(src);
		check("copy-ctor matches src name", strcmp(ResEntryTestHooks::Info(copyCtor).mResName, "Src") == 0);
		check("copy-ctor matches src id", ResEntryTestHooks::Info(copyCtor).mTail[1] == 8);

		CResEntry selfAssignTest(src);
		selfAssignTest = selfAssignTest;
		check("self-assignment is safe",
			strcmp(ResEntryTestHooks::Info(selfAssignTest).mResName, "Src") == 0);
	}

	printf("[5] CResEntryEx: mExtra + delegating ctors\n");
	{
		STriplet id; id.b0 = 5; id.b1 = 6; id.b2 = 7;

		CResEntryEx exA(id, "ExBank", (unsigned char)1, (unsigned char)2, 30, 40, 0xdeadbeefU);
		check("7-arg ctor sets mExtra", ResEntryTestHooks::Extra(exA) == 0xdeadbeefU);
		check("7-arg ctor delegates STriplet", ResEntryTestHooks::Info(exA).mTail[0] == 5);

		CResEntryEx exB(id, "ExBank2", (unsigned char)1, (unsigned char)2, 30, 40);
		check("6-arg (no extra) ctor zeroes mExtra", ResEntryTestHooks::Extra(exB) == 0);

		CResEntryEx exC(id, "ExBank3", (unsigned short)7, (unsigned char)1, (unsigned char)2, 0x1234U);
		check("6-arg (ushort+extra) ctor sets mExtra", ResEntryTestHooks::Extra(exC) == 0x1234U);
		check("6-arg (ushort+extra) ctor sets mIndex", ResEntryTestHooks::Index(exC) == 7);

		CResEntryEx exD(id, "ExBank4", (unsigned short)9, (unsigned char)1, (unsigned char)2);
		check("5-arg (ushort, no extra) ctor zeroes mExtra", ResEntryTestHooks::Extra(exD) == 0);
	}

	printf("[6] CResEntryEx copy/CopyEx/operator=/Reset\n");
	{
		STriplet id; id.b0 = 11; id.b1 = 12; id.b2 = 13;
		CResEntryEx src(id, "ExSrc", (unsigned char)1, (unsigned char)2, 1, 2, 0x99U);

		CResEntryEx exCopy(src);
		check("Ex copy-ctor copies mExtra", ResEntryTestHooks::Extra(exCopy) == 0x99U);
		check("Ex copy-ctor copies name", strcmp(ResEntryTestHooks::Info(exCopy).mResName, "ExSrc") == 0);

		CResEntry plain(id, "Plain", (unsigned char)3, (unsigned char)4, 5, 6);
		CResEntryEx exFromPlain(plain);
		check("Ex(CResEntry const&) ctor zeroes mExtra", ResEntryTestHooks::Extra(exFromPlain) == 0);
		check("Ex(CResEntry const&) ctor copies name",
			strcmp(ResEntryTestHooks::Info(exFromPlain).mResName, "Plain") == 0);

		CResEntryEx exAssign(id, "Placeholder", (unsigned char)0, (unsigned char)0, 0, 0, 0U);
		exAssign = src;
		check("Ex operator=(Ex const&) copies mExtra", ResEntryTestHooks::Extra(exAssign) == 0x99U);

		CResEntryEx exAssign2(id, "Placeholder2", (unsigned char)0, (unsigned char)0, 0, 0, 0xffffU);
		exAssign2 = plain;
		check("Ex operator=(CResEntry const&) zeroes mExtra", ResEntryTestHooks::Extra(exAssign2) == 0);

		CResEntryEx exCopyEx(id, "Placeholder3", (unsigned char)0, (unsigned char)0, 0, 0, 0U);
		exCopyEx.CopyEx(src);
		check("CopyEx(Ex const&) copies mExtra", ResEntryTestHooks::Extra(exCopyEx) == 0x99U);

		exCopyEx.Reset();
		check("Ex Reset() zeroes mExtra", ResEntryTestHooks::Extra(exCopyEx) == 0);
		check("Ex Reset() also resets base (mIndex sentinel)", ResEntryTestHooks::Index(exCopyEx) == 0xffff);
	}

	printf("\n%s (%d failed)\n", g_fail == 0 ? "PASSED" : "FAILED", g_fail);
	return g_fail == 0 ? 0 : 1;
}
