/*
 * test_memory_accessor.cpp  -  host-side known-answer test for
 * CMemoryAccessor's full 12-method Read/Write{Big,Little}{16,24,32}Bit
 * family (see include/storage_converter_ext_stubs.h's own round-43
 * comment for provenance).
 */

#include <cstdio>

#include "storage_converter_ext_stubs.h"

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got == want) {
		printf("  ok    %-48s 0x%lx\n", label, got);
		return;
	}
	printf("  FAIL  %-48s got=0x%lx want=0x%lx\n", label, got, want);
	g_fail++;
}

int main()
{
	printf("CMemoryAccessor known-answer test\n");
	printf("==================================\n");

	unsigned char buf[4];

	{
		unsigned char src[4] = { 0x12, 0x34, 0x56, 0x78 };
		check_eq("ReadBig32Bit", CMemoryAccessor::ReadBig32Bit(src), 0x12345678);
		CMemoryAccessor::WriteBig32Bit(buf, 0x12345678);
		check_eq("WriteBig32Bit byte 0", buf[0], 0x12);
		check_eq("WriteBig32Bit byte 3", buf[3], 0x78);
	}
	{
		unsigned char src[3] = { 0x12, 0x34, 0x56 };
		check_eq("ReadBig24Bit", CMemoryAccessor::ReadBig24Bit(src), 0x123456);
		unsigned char b3[3] = { 0 };
		CMemoryAccessor::WriteBig24Bit(b3, 0x123456);
		check_eq("WriteBig24Bit byte 0", b3[0], 0x12);
		check_eq("WriteBig24Bit byte 2", b3[2], 0x56);
	}
	{
		unsigned char src[2] = { 0x12, 0x34 };
		check_eq("ReadBig16Bit", CMemoryAccessor::ReadBig16Bit(src), 0x1234);
		unsigned char b2[2] = { 0 };
		CMemoryAccessor::WriteBig16Bit(b2, 0x1234);
		check_eq("WriteBig16Bit byte 0", b2[0], 0x12);
		check_eq("WriteBig16Bit byte 1", b2[1], 0x34);
	}
	{
		unsigned char src[4] = { 0x78, 0x56, 0x34, 0x12 };
		check_eq("ReadLittle32Bit", CMemoryAccessor::ReadLittle32Bit(src), 0x12345678);
		CMemoryAccessor::WriteLittle32Bit(buf, 0x12345678);
		check_eq("WriteLittle32Bit byte 0", buf[0], 0x78);
		check_eq("WriteLittle32Bit byte 3", buf[3], 0x12);
	}
	{
		unsigned char src[3] = { 0x56, 0x34, 0x12 };
		check_eq("ReadLittle24Bit", CMemoryAccessor::ReadLittle24Bit(src), 0x123456);
		unsigned char b3[3] = { 0 };
		CMemoryAccessor::WriteLittle24Bit(b3, 0x123456);
		check_eq("WriteLittle24Bit byte 0", b3[0], 0x56);
		check_eq("WriteLittle24Bit byte 2", b3[2], 0x12);
	}
	{
		unsigned char src[2] = { 0x34, 0x12 };
		check_eq("ReadLittle16Bit", CMemoryAccessor::ReadLittle16Bit(src), 0x1234);
		unsigned char b2[2] = { 0 };
		CMemoryAccessor::WriteLittle16Bit(b2, 0x1234);
		check_eq("WriteLittle16Bit byte 0", b2[0], 0x34);
		check_eq("WriteLittle16Bit byte 1", b2[1], 0x12);
	}

	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
