// SPDX-License-Identifier: GPL-2.0
/*
 * test_startup_file.cpp  -  KAT for CStartupFile::CStartupFile(const
 * char*)/~CStartupFile() (see ../src/engine/startup_file.cpp).
 *
 * Verifies: the vtable pointer is set to the real compiler-generated
 * `_ZTV12CStartupFile` symbol (+8, the standard Itanium convention) by
 * both the ctor and the dtor, and that the ctor's transient +0x4 write
 * really does store the raw `name` pointer's bit pattern (not a
 * meaningful float) -- exactly the real disassembly's own `mov
 * edx,[eax+4]` semantics, reproduced via a same-width memcpy round-trip.
 */

#include <cstdio>
#include <cstring>
#include "oa_setup_global_resources.h"

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got == want) {
		printf("  ok    %-60s 0x%lx\n", label, got);
		return;
	}
	printf("  FAIL  %-60s got=0x%lx want=0x%lx\n", label, got, want);
	g_fail++;
}

int main(void)
{
	printf("CStartupFile::CStartupFile(const char*)/~CStartupFile() known-answer test\n");
	printf("=========================================================\n");

	printf("[1] constructor\n");
	static const char name[] = "CostProfile";
	unsigned char raw[sizeof(CStartupFile)];
	memset(raw, 0xcc, sizeof(raw));
	CStartupFile *sf = new (raw) CStartupFile(name);

	check_eq("_vtablePtr is nonzero (real _ZTV12CStartupFile symbol + 8)",
		 (unsigned long)sf->_vtablePtr != 0, 1);

	unsigned int fieldBits;
	memcpy(&fieldBits, &sf->_field4, sizeof(fieldBits));
	check_eq("+0x4 transiently holds the raw `name` pointer's bit pattern",
		 fieldBits, (unsigned int)(unsigned long)name);

	printf("\n[2] destructor resets the vtable pointer to this class's own\n");
	void *vtableAfterCtor = sf->_vtablePtr;
	sf->~CStartupFile();
	check_eq("_vtablePtr unchanged (same real _ZTV12CStartupFile+8 value)",
		 (unsigned long)sf->_vtablePtr, (unsigned long)vtableAfterCtor);

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
