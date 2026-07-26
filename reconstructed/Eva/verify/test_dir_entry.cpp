/*
 * test_dir_entry.cpp  -  host-side known-answer test for CDirEntry's 9 new
 * predicate/accessor methods (src/editor/dir_entry.cpp), reconstructed while
 * investigating CBatchDiskMainTask::PreloadDir() (2026-07-26). See
 * dir_entry.h's own header comment for ground-truth addresses.
 *
 * Checks:
 *   [1] Default-constructed CDirEntry: IsEmpty()==1 (mUnknown5c==1 by ctor),
 *       IsDeleted()==0, IsReserved()==false, IsLabel()==false, IsDir()==false,
 *       IsParentDir()==false, IsCurrentDir()==false, HasValidLongNameExt()==
 *       false, GetName()==NULL, GetExt()==NULL (all consistent -- a freshly
 *       constructed entry has never been populated by a real directory scan).
 *   [2] Friend-poked mUnknown50/mUnknown58/mUnknown60/mUnknown64 exercise
 *       every predicate's real bit-test/field shape (IsReserved's nibble==0xf
 *       sentinel, IsLabel's 0x8 bit, IsDir/IsParentDir/IsCurrentDir's shared
 *       0x10 bit gated by mUnknown60/mUnknown64).
 *   [3] Friend-poked CZ::mOpaque (via CZTestHooks) exercises
 *       HasValidLongNameExt()'s real OR-of-2-fields logic and GetName()/
 *       GetExt()'s real vtable-dispatched short-vs-long selection, including
 *       the FIX this batch made to PTR__CDirEntry_08e81908[2] (was
 *       EvaVTableStub, now the real HasValidLongNameExt forwarder) --
 *       without the fix this check would see EAX garbage instead of a real
 *       true/false answer.
 */

#include <cstdio>
#include <stdint.h>
#include <cstring>

#include "dir_entry.h"
#include "cz_util.h"

struct DirEntryTestHooks {
	static void SetFlags(CDirEntry &e, unsigned char v) { e.mUnknown50 = v; }
	static void SetDeleted(CDirEntry &e, int v) { e.mUnknown58 = v; }
	static void SetEmpty(CDirEntry &e, int v) { e.mUnknown5c = v; }
	static void SetParentFlag(CDirEntry &e, int v) { e.mUnknown60 = v; }
	static void SetCurrentFlag(CDirEntry &e, int v) { e.mUnknown64 = v; }
	static CZ &ShortName(CDirEntry &e) { return e.mShortName; }
	static CZ &ShortExt(CDirEntry &e) { return e.mShortExt; }
	static CZ &LongName(CDirEntry &e) { return e.mLongName; }
	static CZ &LongExt(CDirEntry &e) { return e.mLongExt; }
};

struct CZTestHooks {
	static void SetRawPtr(CZ &z, uint32_t v) { memcpy(z.mOpaque + 0, &v, sizeof v); }
	static void SetRawFlag(CZ &z, uint32_t v) { memcpy(z.mOpaque + 8, &v, sizeof v); }
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
	printf("CDirEntry known-answer test\n");
	printf("============================\n");

	printf("[1] default-constructed state\n");
	{
		CDirEntry e;
		check("IsEmpty() == 1", e.IsEmpty() == 1);
		check("IsDeleted() == 0", e.IsDeleted() == 0);
		check("IsReserved() == false", e.IsReserved() == false);
		check("IsLabel() == false", e.IsLabel() == false);
		check("IsDir() == false", e.IsDir() == false);
		check("IsParentDir() == false", e.IsParentDir() == false);
		check("IsCurrentDir() == false", e.IsCurrentDir() == false);
		check("HasValidLongNameExt() == false", e.HasValidLongNameExt() == false);
		check("GetName() == NULL", e.GetName() == 0);
		check("GetExt() == NULL", e.GetExt() == 0);
	}

	printf("[2] flag-byte predicates\n");
	{
		CDirEntry e;
		DirEntryTestHooks::SetFlags(e, 0x0f); /* reserved sentinel */
		check("IsReserved() true on nibble==0xf", e.IsReserved() == true);
		check("IsDir() false (bit 0x10 not set)", e.IsDir() == false);

		DirEntryTestHooks::SetFlags(e, 0x08);
		check("IsLabel() true on bit 0x8", e.IsLabel() == true);
		check("IsReserved() false (nibble != 0xf)", e.IsReserved() == false);

		DirEntryTestHooks::SetFlags(e, 0x10);
		check("IsDir() true on bit 0x10", e.IsDir() == true);
		check("IsParentDir() false (mUnknown60==0)", e.IsParentDir() == false);
		check("IsCurrentDir() false (mUnknown64==0)", e.IsCurrentDir() == false);

		DirEntryTestHooks::SetParentFlag(e, 1);
		check("IsParentDir() true once mUnknown60!=0 AND bit 0x10 set",
		      e.IsParentDir() == true);

		DirEntryTestHooks::SetCurrentFlag(e, 1);
		check("IsCurrentDir() true once mUnknown64!=0 AND bit 0x10 set",
		      e.IsCurrentDir() == true);

		DirEntryTestHooks::SetDeleted(e, 1);
		check("IsDeleted() == 1 after poke", e.IsDeleted() == 1);

		DirEntryTestHooks::SetEmpty(e, 0);
		check("IsEmpty() == 0 after poke", e.IsEmpty() == 0);
	}

	printf("[3] HasValidLongNameExt()/GetName()/GetExt() real vtable dispatch\n");
	{
		CDirEntry e;
		/* Neither long field populated -> false, short name selected. */
		CZTestHooks::SetRawPtr(DirEntryTestHooks::ShortName(e), 0xdeadbe00);
		check("HasValidLongNameExt() false (both long fields still 0)",
		      e.HasValidLongNameExt() == false);
		check("GetName() returns the SHORT name pointer",
		      (uintptr_t)e.GetName() == 0xdeadbe00);

		/* Populate mLongName's own flag field -> HasValidLongNameExt() true,
		 * GetName() must now switch to the LONG name pointer (real vtable
		 * dispatch through the slot this batch fixed -- would silently
		 * return the wrong answer, or crash on EvaVTableStub's garbage EAX,
		 * if PTR__CDirEntry_08e81908[2] were still the generic stub).
		 */
		CZTestHooks::SetRawFlag(DirEntryTestHooks::LongName(e), 1);
		CZTestHooks::SetRawPtr(DirEntryTestHooks::LongName(e), 0xcafef00d);
		check("HasValidLongNameExt() true once mLongName's flag field is set",
		      e.HasValidLongNameExt() == true);
		check("GetName() switches to the LONG name pointer",
		      (uintptr_t)e.GetName() == 0xcafef00d);

		/* HasValidLongNameExt() is a single shared query (OR of both long
		 * fields) -- it is already true from mLongName above, so GetExt()
		 * ALSO reads the long ext pointer even though mLongExt's own flag
		 * field is still 0. This matches ground truth's real shape exactly
		 * (one shared boolean query feeds both GetName() and GetExt()).
		 */
		CZTestHooks::SetRawPtr(DirEntryTestHooks::ShortExt(e), 0x11112222);
		CZTestHooks::SetRawPtr(DirEntryTestHooks::LongExt(e), 0x33334444);
		check("GetExt() follows the SAME shared HasValidLongNameExt() answer "
		      "(already true from mLongName -- picks LONG ext, not short)",
		      (uintptr_t)e.GetExt() == 0x33334444);
	}

	printf("[4] HasValidLongNameExt()/GetExt() on a fresh instance (mLongExt's "
	       "OWN flag field, independent of mLongName)\n");
	{
		CDirEntry e;
		CZTestHooks::SetRawPtr(DirEntryTestHooks::ShortExt(e), 0x11112222);
		check("GetExt() SHORT while neither long field is set",
		      (uintptr_t)e.GetExt() == 0x11112222);

		CZTestHooks::SetRawFlag(DirEntryTestHooks::LongExt(e), 1);
		CZTestHooks::SetRawPtr(DirEntryTestHooks::LongExt(e), 0x33334444);
		check("HasValidLongNameExt() true once mLongExt's OWN flag field is set",
		      e.HasValidLongNameExt() == true);
		check("GetExt() switches to LONG once mLongExt's own flag is set",
		      (uintptr_t)e.GetExt() == 0x33334444);
	}

	printf("\n%s (%d check%s failed)\n", g_fail ? "FAILED" : "PASSED",
	       g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
