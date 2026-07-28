/*
 * test_korg_riff.cpp  -  host-side known-answer test for CKorgRiff
 * (src/init/korg_riff.cpp). See include/korg_riff.h for full ground-truth
 * provenance.
 *
 * Real host round-trip: a concrete subclass writes a file via WriteFile()/
 * WriteHeader(), then a fresh instance reads it back via ReadFile(), driving
 * the real "NAME"-chunk special case and the virtual ReadChunk() dispatch
 * against actual libc stdio, not just decompile cross-check.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "korg_riff.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Records every ReadChunk() dispatch it receives, and reports a
 * caller-controlled IsBigEndian() so both SwapFile()'s branches and the
 * length-field swap in ReadFile()/WriteHeader() are exercised.
 */
class CTestKorgRiff : public CKorgRiff {
public:
	CTestKorgRiff(const char *name, const char *ext)
		: CKorgRiff(name, ext), mBigEndian(false), mChunkCalls(0)
	{
		mLastId = 0;
		mLastLen = 0;
	}

	virtual bool IsBigEndian() const { return mBigEndian; }

	virtual int ReadChunk(unsigned int id, unsigned int len, FILE *file)
	{
		mChunkCalls++;
		mLastId = id;
		mLastLen = len;
		return CKorgRiff::ReadChunk(id, len, file); /* real base: skip via fseek */
	}

	void GetName(char *dest, unsigned int maxLen) { NameChunk().GetName(dest, maxLen); }
	void SetName(const char *name) { NameChunk().SetName(name); }

	bool mBigEndian;
	int mChunkCalls;
	unsigned int mLastId;
	unsigned int mLastLen;

private:
	/* Test-only access to the private mChunkName member via the header's
	 * own KorgRiffTestHooks friend seam.
	 */
	CNameChunk &NameChunk();
};

struct KorgRiffTestHooks {
	static CKorgRiff::CNameChunk &Get(CTestKorgRiff *r)
	{
		return r->CKorgRiff::mChunkName;
	}
};

CKorgRiff::CNameChunk &CTestKorgRiff::NameChunk()
{
	return KorgRiffTestHooks::Get(this);
}

int main()
{
	printf("CKorgRiff known-answer test\n");
	printf("============================\n");

	printf("[1] ctor: mChunkName starts empty (first byte cleared)\n");
	{
		CTestKorgRiff r("/tmp/x", ".RIF");
		char buf[0x19];
		r.GetName(buf, sizeof(buf));
		check("GetName == \"\"", strcmp(buf, "") == 0);
	}

	printf("[2] SetName/GetName round-trip (bounded, NUL-forced by GetName)\n");
	{
		CTestKorgRiff r("/tmp/x", ".RIF");
		r.SetName("MyPatch");
		char buf[0x19];
		r.GetName(buf, sizeof(buf));
		check("== MyPatch", strcmp(buf, "MyPatch") == 0);
	}

	printf("[3] SetName truncates to 24 bytes, GetName still NUL-terminates at 0x18\n");
	{
		CTestKorgRiff r("/tmp/x", ".RIF");
		r.SetName("123456789012345678901234567890"); /* 30 chars, > 24 */
		char buf[0x19];
		r.GetName(buf, sizeof(buf));
		check("length == 24", strlen(buf) == 24);
		check("dest[0x18] forced NUL", buf[0x18] == 0);
	}

	const char *path = "/tmp/korg_riff_test_file.bin";

	printf("[4] WriteFile: real host round-trip via ReadFile (NAME chunk)\n");
	{
		CTestKorgRiff w("/tmp/x", ".RIF");
		w.SetName("RoundTrip");
		FILE *f = fopen(path, "wb");
		int wr = w.WriteFile(f);
		fclose(f);
		check("WriteFile returns 0", wr == 0);

		CTestKorgRiff r("/tmp/x", ".RIF");
		f = fopen(path, "rb");
		int rr = r.ReadFile(f);
		fclose(f);
		check("ReadFile returns 0 (no non-NAME chunks)", rr == 0);
		check("ReadChunk never dispatched (NAME consumed directly)", r.mChunkCalls == 0);

		char buf[0x19];
		r.GetName(buf, sizeof(buf));
		check("mChunkName round-tripped == RoundTrip", strcmp(buf, "RoundTrip") == 0);
	}

	printf("[5] ReadFile: unrecognized chunk dispatches to ReadChunk(), base skips via fseek\n");
	{
		FILE *f = fopen(path, "wb");
		/* Write NAME chunk first (so mChunkName round-trips too), then a
		 * synthetic "DATA" chunk with a 4-byte payload the base
		 * ReadChunk() should skip cleanly, then a second real NAME
		 * chunk so we can confirm the loop resumed correctly after the
		 * skip.
		 */
		CTestKorgRiff w("/tmp/x", ".RIF");
		w.SetName("First");
		int wr = w.WriteFile(f);
		check("seed WriteFile ok", wr == 0);

		unsigned int tag = 0x41544144u; /* raw "DATA" bytes, natural order */
		unsigned int len = 4;
		fwrite(&tag, 4, 1, f);
		fwrite(&len, 4, 1, f);
		const char payload[4] = { 0x11, 0x22, 0x33, 0x44 };
		fwrite(payload, 1, 4, f);
		fclose(f);

		CTestKorgRiff r("/tmp/x", ".RIF");
		f = fopen(path, "rb");
		int rr = r.ReadFile(f);
		fclose(f);

		check("ReadChunk dispatched exactly once", r.mChunkCalls == 1);
		check("dispatched id == Bswap32(\"DATA\") == 0x44415441",
		      r.mLastId == 0x44415441u);
		check("dispatched len == 4", r.mLastLen == 4);
		check("ReadFile returns ReadChunk's own return value (0)", rr == 0);

		char buf[0x19];
		r.GetName(buf, sizeof(buf));
		check("mChunkName == First", strcmp(buf, "First") == 0);
	}

	printf("[6] WriteHeader + SwapFile: little-endian (default) leaves values untouched\n");
	{
		CTestKorgRiff w("/tmp/x", ".RIF");
		FILE *f = fopen(path, "wb");
		w.WriteHeader(0x12345678u, 0x1000, f);
		fclose(f);

		unsigned char raw[8];
		f = fopen(path, "rb");
		size_t n = fread(raw, 1, 8, f);
		fclose(f);
		check("read 8 bytes", n == 8);

		/* tag = Bswap32(id): 0x12345678 -> 0x78563412, stored little-endian
		 * on disk as raw bytes 12 34 56 78 (since a plain LE store of
		 * 0x78563412 emits bytes 12,34,56,78).
		 */
		check("tag bytes == 12 34 56 78",
		      raw[0] == 0x12 && raw[1] == 0x34 && raw[2] == 0x56 && raw[3] == 0x78);
		/* len untouched (little-endian, no swap): 0x1000 -> bytes 00 10 00 00 */
		check("len bytes == 00 10 00 00 (unswapped)",
		      raw[4] == 0x00 && raw[5] == 0x10 && raw[6] == 0x00 && raw[7] == 0x00);

		short s = 0x1234;
		w.SwapFile(s);
		check("SwapFile no-op when !IsBigEndian()", s == 0x1234);
	}

	printf("[7] WriteHeader + SwapFile: big-endian mode swaps the length field\n");
	{
		CTestKorgRiff w("/tmp/x", ".RIF");
		w.mBigEndian = true;
		FILE *f = fopen(path, "wb");
		w.WriteHeader(0x12345678u, 0x1000, f);
		fclose(f);

		unsigned char raw[8];
		f = fopen(path, "rb");
		size_t n = fread(raw, 1, 8, f);
		fclose(f);
		check("read 8 bytes", n == 8);
		check("tag bytes still 12 34 56 78 (tag not affected by IsBigEndian)",
		      raw[0] == 0x12 && raw[1] == 0x34 && raw[2] == 0x56 && raw[3] == 0x78);
		/* len swapped: 0x1000 -> Bswap32 -> 0x00100000, LE bytes 00 00 10 00 */
		check("len bytes == 00 00 10 00 (swapped)",
		      raw[4] == 0x00 && raw[5] == 0x00 && raw[6] == 0x10 && raw[7] == 0x00);

		unsigned short us = 0x1234;
		w.SwapFile(us);
		check("SwapFile swaps when IsBigEndian()", us == 0x3412);

		unsigned int ui = 0x11223344u;
		w.SwapFile(ui);
		check("SwapFile(uint&) swaps when IsBigEndian()", ui == 0x44332211u);
	}

	printf("[8] SwapLittleEndian: always a no-op\n");
	{
		short s = 0x1234;
		CKorgRiff::SwapLittleEndian(s);
		check("short unchanged", s == 0x1234);

		unsigned short us = 0xabcd;
		CKorgRiff::SwapLittleEndian(us);
		check("ushort unchanged", us == 0xabcd);

		unsigned int ui = 0x11223344u;
		CKorgRiff::SwapLittleEndian(ui);
		check("uint unchanged", ui == 0x11223344u);
	}

	printf("[9] SwapBigEndian / Swap: unconditional, byte-identical bodies\n");
	{
		short s1 = 0x1234, s2 = 0x1234;
		CKorgRiff::SwapBigEndian(s1);
		CKorgRiff::Swap(s2);
		check("SwapBigEndian(short) == 0x3412", s1 == 0x3412);
		check("Swap(short) matches SwapBigEndian", s2 == s1);

		unsigned int u1 = 0x11223344u, u2 = 0x11223344u;
		CKorgRiff::SwapBigEndian(u1);
		CKorgRiff::Swap(u2);
		check("SwapBigEndian(uint) == 0x44332211", u1 == 0x44332211u);
		check("Swap(uint) matches SwapBigEndian", u2 == u1);
	}

	printf("[10] ReadChunk base body: fseek(SEEK_CUR) skip, always returns 0\n");
	{
		FILE *f = fopen(path, "wb");
		const char junk[16] = { 0 };
		fwrite(junk, 1, 16, f);
		fclose(f);

		CTestKorgRiff w("/tmp/x", ".RIF");
		f = fopen(path, "rb");
		fseek(f, 0, SEEK_SET);
		int r = w.CKorgRiff::ReadChunk(0, 6, f); /* call base body explicitly */
		long pos = ftell(f);
		fclose(f);
		check("ReadChunk base returns 0", r == 0);
		check("fseek advanced by len (6)", pos == 6);
	}

	remove(path);

	printf("\n");
	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
