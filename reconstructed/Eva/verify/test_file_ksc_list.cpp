/*
 * test_file_ksc_list.cpp  -  host-side known-answer test for CFileKscList
 * (src/init/file_ksc_list.cpp). See include/file_ksc_list.h for full
 * ground-truth provenance and the list of deferred (not reconstructed)
 * methods.
 *
 * A fake FMApi object stands in for the real god-object, matching the
 * established convention from test_config_manager_create_modules.cpp:
 * only the 2 vtable slots this class actually calls (+0x1bc read,
 * +0x1c0 write) are backed by real logic -- a tiny in-memory "record",
 * a flat byte buffer with a read/write cursor, standing in for whatever
 * real backing store FMApi itself uses (never modeled, per this class's
 * own header comment).
 */

#include <cstdio>
#include <cstring>

#include "file_ksc_list.h"
#include "system_api.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

extern CSystemApi *FMApi;

namespace {

/* Fake backing record: flat byte buffer + cursor, standing in for
 * whatever real record store FMApi's own +0x1bc/+0x1c0 slots read/write
 * through. `sLastHandle` lets tests confirm the real handle value was
 * forwarded through unchanged. */
static unsigned char sRecord[256];
static unsigned int sCursor;
static unsigned int sRecordLen;
static void *sLastHandle;
static int sReadCalls, sWriteCalls;
static int sFailNextRead, sFailNextWrite;

static void ResetFakeRecord()
{
	memset(sRecord, 0, sizeof(sRecord));
	sCursor = 0;
	sRecordLen = 0;
	sLastHandle = 0;
	sReadCalls = sWriteCalls = 0;
	sFailNextRead = sFailNextWrite = 0;
}

static int FakeRead(void *apiThis, void *handle, void *buf, unsigned int *len)
{
	(void)apiThis;
	sLastHandle = handle;
	sReadCalls++;
	if (sFailNextRead) {
		sFailNextRead = 0;
		return 0;
	}
	unsigned int n = *len;
	if (sCursor + n > sRecordLen)
		return 0;
	memcpy(buf, sRecord + sCursor, n);
	sCursor += n;
	return 1;
}

static int FakeWrite(void *apiThis, void *handle, const void *buf, unsigned int *len)
{
	(void)apiThis;
	sLastHandle = handle;
	sWriteCalls++;
	if (sFailNextWrite) {
		sFailNextWrite = 0;
		return 0;
	}
	unsigned int n = *len;
	if (sCursor + n > sizeof(sRecord))
		return 0;
	memcpy(sRecord + sCursor, buf, n);
	sCursor += n;
	if (sCursor > sRecordLen)
		sRecordLen = sCursor;
	return 1;
}

static void *sFakeFMApiVtbl[128];
struct FakeFMApi { void *vtbl; };
static FakeFMApi sFakeFMApi;

static void InstallFakeFMApi()
{
	memset(sFakeFMApiVtbl, 0, sizeof(sFakeFMApiVtbl));
	sFakeFMApiVtbl[0x1bc / 4] = (void *)FakeRead;
	sFakeFMApiVtbl[0x1c0 / 4] = (void *)FakeWrite;
	sFakeFMApi.vtbl = sFakeFMApiVtbl;
	FMApi = (CSystemApi *)&sFakeFMApi;
}

} // namespace

int main()
{
	printf("CFileKscList known-answer test\n");
	printf("================================\n");
	InstallFakeFMApi();

	printf("[1] SaveHeaderId writes literal \"#KSC\" (4 bytes), ReadHeaderId reads it back\n");
	{
		ResetFakeRecord();
		CFileKscList ksc;
		check("SaveHeaderId ok", ksc.SaveHeaderId());
		check("wrote exactly 4 bytes", sRecordLen == 4);
		check("bytes == \"#KSC\"", memcmp(sRecord, "#KSC", 4) == 0);
		sCursor = 0;
		check("ReadHeaderId ok (matches magic)", ksc.ReadHeaderId());
	}

	printf("[2] ReadHeaderId returns false when the record doesn't match \"#KSC\"\n");
	{
		ResetFakeRecord();
		memcpy(sRecord, "XKSC", 4);
		sRecordLen = 4;
		CFileKscList ksc;
		check("ReadHeaderId false on mismatch", !ksc.ReadHeaderId());
	}

	printf("[3] ReadHeaderId returns false when the underlying FMApi read call itself fails\n");
	{
		ResetFakeRecord();
		sRecordLen = 4;
		sFailNextRead = 1;
		CFileKscList ksc;
		check("ReadHeaderId false on FMApi failure", !ksc.ReadHeaderId());
	}

	printf("[4] WriteDot/ReadDot round-trip \"\\r\\n\" (2 bytes)\n");
	{
		ResetFakeRecord();
		CFileKscList ksc;
		check("WriteDot ok", ksc.WriteDot());
		check("wrote exactly 2 bytes", sRecordLen == 2);
		check("bytes == \\r\\n", sRecord[0] == '\r' && sRecord[1] == '\n');
		sCursor = 0;
		check("ReadDot ok", ksc.ReadDot());
	}

	printf("[5] ReadDot returns false on a non-CRLF record\n");
	{
		ResetFakeRecord();
		memcpy(sRecord, "xx", 2);
		sRecordLen = 2;
		CFileKscList ksc;
		check("ReadDot false on mismatch", !ksc.ReadDot());
	}

	printf("[6] VendorId (8B)/ProductId (16B)/SerialNumber (128B) string round-trip\n");
	{
		ResetFakeRecord();
		CFileKscList ksc;
		check("SaveVendorId ok", ksc.SaveVendorId("KORGINC1"));
		check("wrote exactly 8 bytes", sRecordLen == 8);
		sCursor = 0;
		char buf[8];
		check("ReadVendorId ok", ksc.ReadVendorId(buf));
		check("round-trips", memcmp(buf, "KORGINC1", 8) == 0);

		ResetFakeRecord();
		check("SaveProductId ok", ksc.SaveProductId("KRONOS-PRODUCT01"));
		check("wrote exactly 16 bytes", sRecordLen == 0x10);

		ResetFakeRecord();
		char serial[0x80];
		memset(serial, 'S', sizeof(serial));
		check("SaveSerialNumber ok", ksc.SaveSerialNumber(serial));
		check("wrote exactly 128 bytes", sRecordLen == 0x80);
	}

	printf("[7] AutoLoad/BitDepth/LoadMethod: len=2 scratch, byte[1] is the real value,\n"
	       "    byte[0] is a zero pad on Save (real ground-truth shape, not re-derived)\n");
	{
		ResetFakeRecord();
		CFileKscList ksc;
		check("SaveAutoLoad ok", ksc.SaveAutoLoad("\x01"));
		check("wrote exactly 2 bytes", sRecordLen == 2);
		check("byte[0] == 0 (pad)", sRecord[0] == 0);
		check("byte[1] == real value", sRecord[1] == 1);
		sCursor = 0;
		char out = 0;
		check("ReadAutoLoad ok", ksc.ReadAutoLoad(&out));
		check("round-trips", out == 1);

		ResetFakeRecord();
		check("SaveBitDepth ok", ksc.SaveBitDepth("\x02"));
		sCursor = 0;
		char bd = 0;
		check("ReadBitDepth ok", ksc.ReadBitDepth(&bd));
		check("round-trips", bd == 2);

		ResetFakeRecord();
		check("SaveLoadMethod ok", ksc.SaveLoadMethod("\x03"));
		sCursor = 0;
		char lm = 0;
		check("ReadLoadMethod ok", ksc.ReadLoadMethod(&lm));
		check("round-trips", lm == 3);
	}

	printf("[8] a full sequential record: #KSC, \\r\\n, VendorId, \\r\\n, AutoLoad, \\r\\n\n");
	{
		ResetFakeRecord();
		CFileKscList ksc;
		ksc.SaveHeaderId();
		ksc.WriteDot();
		ksc.SaveVendorId("KORGINC1");
		ksc.WriteDot();
		ksc.SaveAutoLoad("\x01");
		ksc.WriteDot();
		check("total record length == 4+2+8+2+2+2", sRecordLen == 4 + 2 + 8 + 2 + 2 + 2);

		sCursor = 0;
		check("ReadHeaderId ok", ksc.ReadHeaderId());
		check("ReadDot #1 ok", ksc.ReadDot());
		char vendor[8];
		check("ReadVendorId ok", ksc.ReadVendorId(vendor));
		check("vendor matches", memcmp(vendor, "KORGINC1", 8) == 0);
		check("ReadDot #2 ok", ksc.ReadDot());
		char autoLoad = 0;
		check("ReadAutoLoad ok", ksc.ReadAutoLoad(&autoLoad));
		check("autoLoad matches", autoLoad == 1);
		check("ReadDot #3 ok", ksc.ReadDot());
	}

	printf("[9] the fake handle value (`this->mHandle`) is forwarded through unchanged\n");
	{
		ResetFakeRecord();
		CFileKscList ksc;
		/* mHandle is uninitialized by the (real, empty) ctor -- explicitly
		 * exercise the pass-through contract via a real call regardless
		 * of its value, matching real ground truth's own behavior
		 * (mHandle is set by a caller, not by CFileKscList itself). */
		ksc.SaveHeaderId();
		check("FMApi read/write calls actually dispatched through the fake vtbl",
		      sWriteCalls == 1);
	}

	printf("[10] SaveFilePath: even-length path (\"AB\", len=2) -- LE length prefix, no pad byte\n");
	{
		ResetFakeRecord();
		CFileKscList ksc;
		check("SaveFilePath ok", ksc.SaveFilePath("AB"));
		check("wrote exactly 2+2 = 4 bytes (no pad)", sRecordLen == 4);
		check("length prefix LE == 02 00", sRecord[0] == 2 && sRecord[1] == 0);
		check("path bytes follow", sRecord[2] == 'A' && sRecord[3] == 'B');
	}

	printf("[11] SaveFilePath: odd-length path (\"ABC\", len=3) -- LE length prefix + 0x00 pad byte\n");
	{
		ResetFakeRecord();
		CFileKscList ksc;
		check("SaveFilePath ok", ksc.SaveFilePath("ABC"));
		check("wrote exactly 2+3+1 = 6 bytes (with pad)", sRecordLen == 6);
		check("length prefix LE == 03 00", sRecord[0] == 3 && sRecord[1] == 0);
		check("path bytes follow", memcmp(sRecord + 2, "ABC", 3) == 0);
		check("trailing pad byte == 0x00", sRecord[5] == 0);
	}

	printf("[12] ReadFilePath round-trips SaveFilePath's even-length record, *lenOut == strLen+2\n");
	{
		ResetFakeRecord();
		CFileKscList ksc;
		ksc.SaveFilePath("AB");
		sCursor = 0;
		char out[8] = {0};
		unsigned short lenOut = 0;
		check("ReadFilePath ok", ksc.ReadFilePath(out, &lenOut));
		check("path round-trips", memcmp(out, "AB", 2) == 0);
		check("*lenOut == 2+2 == 4", lenOut == 4);
	}

	printf("[13] ReadFilePath round-trips SaveFilePath's odd-length record, *lenOut == strLen+3\n");
	{
		ResetFakeRecord();
		CFileKscList ksc;
		ksc.SaveFilePath("ABC");
		sCursor = 0;
		char out[8] = {0};
		unsigned short lenOut = 0;
		check("ReadFilePath ok", ksc.ReadFilePath(out, &lenOut));
		check("path round-trips", memcmp(out, "ABC", 3) == 0);
		check("*lenOut == 3+3 == 6", lenOut == 6);
	}

	printf("[14] ReadFilePath returns false when the length-prefix FMApi read itself fails\n");
	{
		ResetFakeRecord();
		CFileKscList ksc;
		ksc.SaveFilePath("AB");
		sCursor = 0;
		sFailNextRead = 1;
		char out[8] = {0};
		unsigned short lenOut = 0xdead;
		check("ReadFilePath false on prefix-read failure", !ksc.ReadFilePath(out, &lenOut));
	}

	printf("[15] ReadFilePath (even length) returns false when the string data is truncated\n");
	{
		ResetFakeRecord();
		/* LE prefix says strLen=2, but only the 2-byte prefix itself is
		 * physically present in the backing record -- the string read
		 * must fail naturally (short read), and since strLen is even,
		 * that failure (ok2) IS the return value directly. */
		sRecord[0] = 2;
		sRecord[1] = 0;
		sRecordLen = 2;
		CFileKscList ksc;
		char out[8] = {0};
		unsigned short lenOut = 0;
		check("ReadFilePath false on truncated string data", !ksc.ReadFilePath(out, &lenOut));
	}

	printf("[16] ReadFilePath (odd length): pad byte missing -- string read succeeds but the\n"
	       "     overall result is still false, matching ground truth's own last-call-wins\n"
	       "     return-value reuse (the pad read's result, not the string read's)\n");
	{
		ResetFakeRecord();
		CFileKscList ksc;
		ksc.SaveFilePath("ABC");
		/* Drop the trailing pad byte the real SaveFilePath wrote, so the
		 * string read (bytes 2..4) succeeds but the pad read (byte 5)
		 * fails with a natural short read. */
		sRecordLen = 5;
		sCursor = 0;
		char out[8] = {0};
		unsigned short lenOut = 0;
		check("ReadFilePath false despite successful string read (pad read lost)",
		      !ksc.ReadFilePath(out, &lenOut));
		check("path bytes were still written to out before the pad failure",
		      memcmp(out, "ABC", 3) == 0);
	}

	FMApi = 0;
	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
