/*
 * test_vfat_entry.cpp  -  host-side known-answer test for CVFATEntry's 14
 * reconstructed methods (include/vfat_entry.h). See that header's own comment
 * for full ground-truth addresses/provenance.
 *
 * ComputeChecksum()/GetAliasChecksum(const u8*)/GetAliasChecksum()/
 * IsLongNameBitArrayEmpty() (both overloads) constants below were computed by
 * a small standalone Python model of the real recurrence/loop, then
 * cross-checked directly against the real extracted machine code executed via
 * this project's direct-execution-oracle technique (mmap+PROT_EXEC), NOT
 * hand-computed -- 60000 randomized oracle trials (0 mismatches) plus 3 exact
 * spot-checks (name="FOO"/ext="TXT" -> 101, name="ABCDEFGHIJ" clamped to 8
 * chars/ext="Z" -> 40, both-empty-padded-to-all-spaces -> 247) all confirmed
 * against the real .text bytes before being written here.
 */

#include <cstdio>
#include <cstring>
#include <stdint.h>

#include "vfat_entry.h"

struct CVFATEntryTestHooks {
	static void SetShortName(CVFATEntry &e, const unsigned char *ptr, uint32_t len)
	{
		e.WriteU32(0x04, (uint32_t)(uintptr_t)ptr);
		e.WriteU32(0x0c, len);
	}
	static void SetShortExt(CVFATEntry &e, const unsigned char *ptr, uint32_t len)
	{
		e.WriteU32(0x14, (uint32_t)(uintptr_t)ptr);
		e.WriteU32(0x1c, len);
	}
	static void SetSlot(CVFATEntry &e, unsigned i, uint32_t v)
	{
		e.WriteU32(0x74 + 0x20 * i, v);
	}
	static void ClearAll(CVFATEntry &e) { memset(e.mRaw, 0, sizeof e.mRaw); }
	static void SetU8(CVFATEntry &e, unsigned off, unsigned char v) { e.WriteU8(off, v); }
	static void SetU32(CVFATEntry &e, unsigned off, uint32_t v) { e.WriteU32(off, v); }
};

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CVFATEntry known-answer test\n");
	printf("=============================\n");

	printf("[1] hardcoded constants\n");
	{
		check("GetMaxNumEntryForLongName() == 20", CVFATEntry::GetMaxNumEntryForLongName() == 20);
		check("GetMaxCharPerEntry() == 13", CVFATEntry::GetMaxCharPerEntry() == 13);
		check("GetMaxCharForLongName() == 260", CVFATEntry::GetMaxCharForLongName() == 260);
		check("GetMaxCharForLongName() == 20*13 (cross-check)",
		      CVFATEntry::GetMaxCharForLongName() ==
		      CVFATEntry::GetMaxNumEntryForLongName() * CVFATEntry::GetMaxCharPerEntry());
		check("GetLongNameMark() == 0x40", CVFATEntry::GetLongNameMark() == 0x40);
	}

	printf("[2] ComputeChecksum() recurrence\n");
	{
		CVFATEntry e;
		check("ComputeChecksum(0,0) == 0", e.ComputeChecksum(0, 0) == 0);
		check("ComputeChecksum(0xff,1) == 0", e.ComputeChecksum(0xff, 1) == 0);
		check("ComputeChecksum(0x01,0x02) == 0x82", e.ComputeChecksum(0x01, 0x02) == 0x82);
	}

	printf("[3] GetAliasChecksum(const u8*) -- 11-byte buffer form\n");
	{
		CVFATEntry e;
		const unsigned char name11[11] = {
			'F', 'O', 'O', ' ', ' ', ' ', ' ', ' ', 'T', 'X', 'T'
		};
		check("GetAliasChecksum(\"FOO     TXT\") == 101",
		      e.GetAliasChecksum(name11) == 101);
	}

	printf("[4] GetAliasChecksum() -- own short-name/short-ext fields, real space-padding\n");
	{
		CVFATEntry e;
		const unsigned char foo[3] = { 'F', 'O', 'O' };
		const unsigned char txt[3] = { 'T', 'X', 'T' };
		CVFATEntryTestHooks::SetShortName(e, foo, 3);
		CVFATEntryTestHooks::SetShortExt(e, txt, 3);
		check("GetAliasChecksum() name='FOO' ext='TXT' == 101 (matches [3]'s "
		      "space-padded equivalent)",
		      e.GetAliasChecksum() == 101);

		const unsigned char longname[10] = {
			'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'
		};
		const unsigned char z[1] = { 'Z' };
		CVFATEntryTestHooks::SetShortName(e, longname, 10); /* real len clamps to 8 */
		CVFATEntryTestHooks::SetShortExt(e, z, 1);
		check("GetAliasChecksum() name len10 (clamped to 8) ext len1 == 40",
		      e.GetAliasChecksum() == 40);

		CVFATEntryTestHooks::SetShortName(e, longname, 0);
		CVFATEntryTestHooks::SetShortExt(e, z, 0);
		check("GetAliasChecksum() name len0 ext len0 (all-space) == 247",
		      e.GetAliasChecksum() == 247);
	}

	printf("[5] GetCurrentSlotIndex/GetCurrentAliasChecksum/GetOutputCodePage/"
	       "GetCurrentNumForShortNameExt raw field reads\n");
	{
		CVFATEntry e;
		CVFATEntryTestHooks::ClearAll(e);
		CVFATEntryTestHooks::SetU8(e, 0x2ec, 7);
		CVFATEntryTestHooks::SetU8(e, 0x2ed, 0x55);
		CVFATEntryTestHooks::SetU32(e, 0x2f0, 3); /* CCodePage::EPage value */
		CVFATEntryTestHooks::SetU32(e, 0x2f8, 42);
		check("GetCurrentSlotIndex() == 7", e.GetCurrentSlotIndex() == 7);
		check("GetCurrentAliasChecksum() == 0x55", e.GetCurrentAliasChecksum() == 0x55);
		check("GetOutputCodePage() == 3", e.GetOutputCodePage() == 3);
		check("GetCurrentNumForShortNameExt() == 42", e.GetCurrentNumForShortNameExt() == 42);
	}

	printf("[6] HasValidLongNameExt()/OnLongNameChanged()/OnLongExtChanged() shared field\n");
	{
		CVFATEntry e;
		CVFATEntryTestHooks::ClearAll(e);
		check("HasValidLongNameExt() == 0 initially", e.HasValidLongNameExt() == 0);
		e.OnLongNameChanged();
		check("HasValidLongNameExt() == 1 after OnLongNameChanged()",
		      e.HasValidLongNameExt() == 1);

		CVFATEntryTestHooks::ClearAll(e);
		e.OnLongExtChanged();
		check("HasValidLongNameExt() == 1 after OnLongExtChanged() (SAME field)",
		      e.HasValidLongNameExt() == 1);
	}

	printf("[7] IsLongNameBitArrayEmpty() / IsLongNameBitArrayEmpty(count)\n");
	{
		CVFATEntry e;
		CVFATEntryTestHooks::ClearAll(e);
		check("IsLongNameBitArrayEmpty() true on an all-zero object",
		      e.IsLongNameBitArrayEmpty() == true);
		check("IsLongNameBitArrayEmpty(0) true (vacuous)",
		      e.IsLongNameBitArrayEmpty(0) == true);
		check("IsLongNameBitArrayEmpty(20) true (all-zero, real max count)",
		      e.IsLongNameBitArrayEmpty(20) == true);

		CVFATEntryTestHooks::SetSlot(e, 5, 0xdeadbeef);
		check("IsLongNameBitArrayEmpty() false once slot 5 is nonzero",
		      e.IsLongNameBitArrayEmpty() == false);
		check("IsLongNameBitArrayEmpty(5) true (slot 5 not yet in range [0,5))",
		      e.IsLongNameBitArrayEmpty(5) == true);
		check("IsLongNameBitArrayEmpty(6) false (slot 5 now in range [0,6))",
		      e.IsLongNameBitArrayEmpty(6) == false);

		CVFATEntryTestHooks::ClearAll(e);
		CVFATEntryTestHooks::SetSlot(e, 19, 1);
		check("IsLongNameBitArrayEmpty() false (last real slot, index 19)",
		      e.IsLongNameBitArrayEmpty() == false);
		check("IsLongNameBitArrayEmpty(19) true (slot 19 not in range [0,19))",
		      e.IsLongNameBitArrayEmpty(19) == true);
	}

	printf("\n%s (%d check%s failed)\n", g_fail ? "FAILED" : "PASSED",
	       g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
