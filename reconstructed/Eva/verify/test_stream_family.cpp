/*
 * test_stream_family.cpp  -  host-side known-answer test for
 * CStream/CIn/COut/CInOut/CNullStr/CMemory (src/base/stream_family.cpp). See
 * include/stream_family.h for full ground-truth provenance.
 *
 * A fake CSystemApi vtable is installed (Api+0x94 soft-assert) so the raw calls
 * this code makes are exercised for real, same convention as
 * test_partition_table.cpp/test_bit_mask_l.cpp.
 */

#include <cstdio>
#include <cstring>

#include "stream_family.h"
#include "system_api.h"

extern CSystemApi *Api;

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

static int g_assertCount;
extern "C" void FakeApiAssert(void *, const char *, const char *file, int line)
{
	g_assertCount++;
	printf("      (soft-assert fired: %s:%d)\n", file, line);
}

static void *g_fakeVtable[0x9c / 4 + 1];
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

	printf("CStream/CIn/COut/CInOut/CNullStr/CMemory known-answer test\n");
	printf("============================================================\n");

	printf("[1] CNullStr -- null-sink size-computation stream\n");
	{
		CNullStr s;
		s.Open("x", CStream::eWrite);
		check("Open(eWrite) arms mState==5 (write)", g_assertCount == 0);
		char scratch[4] = { 0 };
		s.Write(scratch, 10);
		check("Write advances position", s.Tell() == 10);
		check("Write grows length", s.GetLength() == 10);
		s.Write(scratch, 5);
		check("second Write keeps advancing", s.Tell() == 15);
		check("length tracks max position", s.GetLength() == 15);

		g_assertCount = 0;
		s.Read(scratch, 1); /* wrong mode (write-armed, not read) -> soft-assert */
		check("Read while write-armed soft-asserts", g_assertCount == 1);

		CNullStr r;
		r.Open("x", CStream::eRead);
		g_assertCount = 0;
		r.Read(scratch, 4);
		check("Open(eRead) arms mState==4, Read ok", g_assertCount == 0);
		check("Read on empty null-stream reads 0 (clamped)", r.Tell() == 0);
	}

	printf("[2] CMemory -- fixed-buffer-backed stream, wrap (mode!=1)\n");
	{
		unsigned char raw[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
		CMemory m(raw, sizeof(raw), 0); /* mode 0: wrap caller's buffer, no copy */

		g_assertCount = 0;
		m.Open("x", CStream::eRead);
		check("Open(eRead) no assert", g_assertCount == 0);
		check("Open(eRead) sets length == capacity", m.GetLength() == 8);

		unsigned char out[8] = { 0 };
		m.Read(out, 4);
		check("Read copies real bytes", memcmp(out, raw, 4) == 0);
		check("Read advances position", m.Tell() == 4);

		m.Read(out + 4, 100); /* over-read, must clamp to remaining 4 bytes */
		check("over-read clamps to available bytes", m.Tell() == 8);
		check("clamped read copied the right tail bytes", memcmp(out, raw, 8) == 0);

		m.Close();
		check("Close resets position", m.Tell() == 0);
	}

	printf("[3] CMemory -- owned copy (mode==1)\n");
	{
		unsigned char raw[4] = { 0xaa, 0xbb, 0xcc, 0xdd };
		CMemory m(raw, sizeof(raw), 1); /* mode 1: allocate + memcpy own copy */

		g_assertCount = 0;
		m.Open("x", CStream::eWrite);
		check("Open(eWrite) no assert", g_assertCount == 0);

		unsigned char newData[4] = { 1, 2, 3, 4 };
		m.Write(newData, 4);
		check("Write into owned copy advances position", m.Tell() == 4);
		check("original caller buffer NOT mutated (owns a private copy)",
			raw[0] == 0xaa && raw[1] == 0xbb && raw[2] == 0xcc && raw[3] == 0xdd);

		m.Write(newData, 4); /* would overflow capacity 4, must clamp to 0 bytes */
		check("write past capacity clamps to 0 extra bytes", m.Tell() == 4);
	}

	printf("[4] CMemory::Seek via CStream::IsSought\n");
	{
		unsigned char raw[16];
		for (int i = 0; i < 16; i++) raw[i] = (unsigned char)i;
		CMemory m(raw, sizeof(raw), 0);
		m.Open("x", CStream::eRead); /* mLength = mCapacity = 16 */

		m.Seek(4, CStream::eSeekSet);
		check("eSeekSet moves to absolute position", m.Tell() == 4);

		m.Seek(2, CStream::eSeekCur);
		check("eSeekCur moves relative to current position", m.Tell() == 6);

		m.Seek(-3, CStream::eSeekCur);
		check("eSeekCur handles negative offsets", m.Tell() == 3);

		m.Seek(0, CStream::eSeekEnd);
		check("eSeekEnd(0) moves to end (== length)", m.Tell() == 16);

		g_assertCount = 0;
		m.Seek(100, CStream::eSeekSet); /* mState==4 (read-armed): clamps to GetLength() */
		check("read-armed seek past EOF clamps to length, no assert",
			m.Tell() == 16 && g_assertCount == 0);
	}

	printf("[5] CIn::Get / COut::Put single-byte wrappers\n");
	{
		unsigned char raw[3] = { 0x11, 0x22, 0x33 };
		CMemory m(raw, sizeof(raw), 0);
		m.Open("x", CStream::eRead);
		unsigned char b = 0;
		m.CIn::Get(b);
		check("CIn::Get reads one byte via Read()", b == 0x11 && m.Tell() == 1);

		CMemory w(raw, sizeof(raw), 0);
		w.Open("x", CStream::eWrite);
		w.COut::Put(0x77);
		check("COut::Put writes one byte via Write()", raw[0] == 0x77 && w.Tell() == 1);
	}

	printf("\n%s (%d failed)\n", g_fail ? "FAIL" : "PASS", g_fail);
	return g_fail ? 1 : 0;
}
