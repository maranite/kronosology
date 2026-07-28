/*
 * test_long_binary_file.cpp  -  host-side known-answer test for
 * CLongBinaryFile (src/base/long_binary_file.cpp). See
 * include/long_binary_file.h for full ground-truth provenance.
 *
 * Links against src/base/file_operation_stub.cpp, a REAL host-functional
 * CFileOperation backed by stdio -- this test does genuine round-trip file
 * I/O against the build host filesystem.
 */

#include <cstdio>
#include <cstring>

#include "long_binary_file.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CLongBinaryFile known-answer test\n");

	const char *path = "/tmp/eva_test_long_binary_file.bin";

	{
		CLongBinaryFile f;
		f.Reset();
		SFilePointer *fp = f.Open(path, 0x1ff, true);
		check("Open(write) succeeds", fp != 0);

		f.WriteData(0x11223344, 4); /* little-endian by default (mByteOrder starts 0 via Reset) */
		f.WriteData(0xAABB, 2);
		f.WriteText("hello", 8); /* pads with 3 NULs */
		f.Close();
	}

	{
		CLongBinaryFile f;
		f.Reset();
		SFilePointer *fp = f.Open(path, 0, false);
		check("Open(read) succeeds", fp != 0);

		long long v1 = f.ReadData(4);
		check("ReadData(4) round-trips LE", v1 == 0x11223344);
		long long v2 = f.ReadData(2);
		check("ReadData(2) round-trips", v2 == 0xAABB);

		char buf[9];
		memset(buf, 'X', sizeof(buf));
		f.ReadText(buf, 8, sizeof(buf) - 1);
		check("ReadText reads back written text", strncmp(buf, "hello", 5) == 0);
		check("ReadText NUL-pad bytes present", buf[5] == '\0' && buf[6] == '\0' && buf[7] == '\0');

		unsigned long pos = f.Tell();
		check("Tell() after 4+2+8 bytes == 14", pos == 14);

		f.Close();
	}

	remove(path);

	printf("%s\n", g_fail ? "FAILED" : "all ok");
	return g_fail ? 1 : 0;
}
