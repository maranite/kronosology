/*
 * test_res_family.cpp  -  host-side known-answer test for CResFamily
 * (res_family.h/.cpp), found + reconstructed 2026-07-27 fresh broad-survey pass (see
 * res_family.h's own file header for the full "size is not depth" narrow-scope
 * rationale -- deep business methods stay out of scope, same CZ-container boundary
 * CreateResourceFamilies() itself already has, config_manager.cpp).
 *
 * Exercises: ctor field values (confirmed against the real disassembly byte-for-byte,
 * poisoned memory beforehand so every checked byte proves the ctor actually wrote it),
 * PostKernelConstructor()'s real COmegaPtrArray heap-allocation + vtable-swap into
 * self+8, PostKernelDestructor()'s cleanup of self+4/self+8 (opaque deleting-dtor
 * dispatch) and self+0x10 (plain free, always NULL in this reconstruction's own
 * scope -- double-call safety), and the real g_atResFamilies[32] global array itself
 * (size/stride/construction-time field values for a spot-checked few slots).
 */

#include <cstdio>
#include <cstring>
#include <new>

#include "res_family.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main(void)
{
	printf("CResFamily known-answer test\n");
	printf("=============================\n");

	printf("[1] Real object size is 0x48 (72) bytes\n");
	check("sizeof(CResFamily) == 0x48", sizeof(CResFamily) == 0x48);

	printf("[2] Ctor writes match the real disassembly byte-for-byte\n");
	static unsigned char raw[sizeof(CResFamily)];
	memset(raw, 0xcc, sizeof(raw));
	CResFamily *fam = new (raw) CResFamily();
	unsigned char *b = raw;
	check("self+0x30 == 0", b[0x30] == 0);
	check("self+0x18 (dword) == 0", *(int *)(b + 0x18) == 0);
	check("self+0x1c (dword) == 0", *(int *)(b + 0x1c) == 0);
	check("self+0x10 (dword) == 0", *(int *)(b + 0x10) == 0);
	check("self+0x14 (dword) == 0", *(int *)(b + 0x14) == 0);
	check("self+0x20 == 0xff", b[0x20] == 0xff);
	check("self+0x24 (dword) == 1", *(int *)(b + 0x24) == 1);
	check("self+0x28 (dword) == 1", *(int *)(b + 0x28) == 1);
	check("self+0x2c (dword) == 1", *(int *)(b + 0x2c) == 1);
	check("self+0x4 (dword) == 0 (before PostKernelConstructor)", *(int *)(b + 4) == 0);
	check("self+0x8 (dword) == 0 (before PostKernelConstructor)", *(int *)(b + 8) == 0);

	printf("[3] PostKernelConstructor() heap-allocates a COmegaPtrArray into self+8\n");
	int rc = fam->PostKernelConstructor(0);
	check("PostKernelConstructor() returns 0", rc == 0);
	check("self+8 is now non-NULL", *(void **)(b + 8) != 0);
	check("self+4 is untouched (still 0)", *(void **)(b + 4) == 0);

	printf("[4] PostKernelDestructor() tears the self+8 array back down cleanly, "
	       "no crash, and re-zeroes both self+4/self+8\n");
	rc = fam->PostKernelDestructor(0);
	check("PostKernelDestructor() returns 0", rc == 0);
	check("self+4 zeroed", *(void **)(b + 4) == 0);
	check("self+8 zeroed", *(void **)(b + 8) == 0);

	printf("[5] PostKernelDestructor() is safe to call again with self+4/+8/+0x10 "
	       "all already NULL (no double-free)\n");
	rc = fam->PostKernelDestructor(0);
	check("second PostKernelDestructor() call also returns 0, no crash", rc == 0);

	fam->~CResFamily();

	printf("[6] The real global array g_atResFamilies[32] is genuinely constructed "
	       "(static init already ran before main())\n");
	check("g_atResFamilies[0]'s own ctor writes are visible",
	      *(int *)((char *)&g_atResFamilies[0] + 0x24) == 1);
	check("g_atResFamilies[31] (last of 32)'s own ctor writes are visible",
	      *(int *)((char *)&g_atResFamilies[31] + 0x24) == 1);
	check("g_atResFamilies[1] - g_atResFamilies[0] stride == 0x48 (72 bytes)",
	      (char *)&g_atResFamilies[1] - (char *)&g_atResFamilies[0] == 0x48);

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
