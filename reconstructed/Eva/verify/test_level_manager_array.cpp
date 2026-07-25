/*
 * test_level_manager_array.cpp  -  host-side known-answer test for
 * CLevelManagerArray::Add()/Find() (src/base/scheduler.cpp, Stage 6, 2026-07-25).
 *
 * Drives the reconstructed Add()/Find() directly against a synthetic
 * COmegaPtrArray-shaped buffer + a handful of fake CLevelManager-shaped byte blocks,
 * checking the real behavior traced from the decompile:
 *   - Add() appends via the real COmegaPtrArray base, then sifts the new element left
 *     while its own +0xc level number is smaller than its predecessor's -- the array
 *     stays sorted ascending by level regardless of insertion order.
 *   - Find() linear-scans for the first element whose +0xc level field matches,
 *     returns NULL if none does.
 */

#include <cstdio>
#include <cstring>
#include "level_manager_array.h"
#include "omega_ptr_array.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Minimal fake CLevelManager -- only the one field Add()/Find() actually touch (+0xc,
 * the level number). Padded to a plausible size; Add()/Find() never read past +0xc.
 */
struct FakeLevelManager {
	char pad[0xc];
	int  level;
	char pad2[0x30];
};

static FakeLevelManager *make_level(int level)
{
	FakeLevelManager *lm = new FakeLevelManager();
	memset(lm, 0, sizeof(*lm));
	lm->level = level;
	return lm;
}

int main(void)
{
	printf("CLevelManagerArray::Add()/Find() known-answer test\n");
	printf("====================================================\n");

	printf("[1] Add() in ascending order stays ascending\n");
	{
		COmegaPtrArray arr;
		for (int lvl = 0; lvl <= 6; lvl++)
			CLevelManagerArray::Add(&arr, (CLevelManager *)make_level(lvl));

		void **raw = *(void ***)((char *)&arr + 0x14);
		int count = *(int *)((char *)&arr + 0xc);
		check("count == 7", count == 7);
		bool sorted = true;
		for (int i = 0; i < count; i++)
			if (((FakeLevelManager *)raw[i])->level != i)
				sorted = false;
		check("array reads back 0,1,2,3,4,5,6", sorted);
	}

	printf("[2] Add() in descending insertion order still sorts ascending\n");
	{
		COmegaPtrArray arr;
		for (int lvl = 6; lvl >= 0; lvl--)
			CLevelManagerArray::Add(&arr, (CLevelManager *)make_level(lvl));

		void **raw = *(void ***)((char *)&arr + 0x14);
		int count = *(int *)((char *)&arr + 0xc);
		check("count == 7", count == 7);
		bool sorted = true;
		for (int i = 0; i < count; i++)
			if (((FakeLevelManager *)raw[i])->level != i)
				sorted = false;
		check("array reads back 0,1,2,3,4,5,6 (sift-up worked)", sorted);
	}

	printf("[3] Add() with a scrambled insertion order (matches real InsertLevel() "
	       "call sequence in mains.cpp: MMainXxx wrappers don't insert levels "
	       "in numeric order in general)\n");
	{
		int order[] = {3, 0, 6, 1, 5, 2, 4};
		COmegaPtrArray arr;
		for (int i = 0; i < 7; i++)
			CLevelManagerArray::Add(&arr, (CLevelManager *)make_level(order[i]));

		void **raw = *(void ***)((char *)&arr + 0x14);
		int count = *(int *)((char *)&arr + 0xc);
		check("count == 7", count == 7);
		bool sorted = true;
		for (int i = 0; i < count; i++)
			if (((FakeLevelManager *)raw[i])->level != i)
				sorted = false;
		check("array reads back 0,1,2,3,4,5,6 regardless of insertion order", sorted);
	}

	printf("[4] Find() locates an inserted level\n");
	{
		COmegaPtrArray arr;
		for (int lvl = 0; lvl <= 6; lvl++)
			CLevelManagerArray::Add(&arr, (CLevelManager *)make_level(lvl));

		CLevelManager *found3 = CLevelManagerArray::Find(&arr, 3);
		check("Find(3) non-null", found3 != 0);
		check("Find(3) returns the level-3 element",
		      found3 != 0 && ((FakeLevelManager *)found3)->level == 3);

		CLevelManager *found0 = CLevelManagerArray::Find(&arr, 0);
		check("Find(0) returns the level-0 element (first array slot)",
		      found0 != 0 && ((FakeLevelManager *)found0)->level == 0);

		CLevelManager *found6 = CLevelManagerArray::Find(&arr, 6);
		check("Find(6) returns the level-6 element (last array slot)",
		      found6 != 0 && ((FakeLevelManager *)found6)->level == 6);
	}

	printf("[5] Find() on an absent level returns NULL\n");
	{
		COmegaPtrArray arr;
		for (int lvl = 0; lvl <= 6; lvl++)
			CLevelManagerArray::Add(&arr, (CLevelManager *)make_level(lvl));

		CLevelManager *found = CLevelManagerArray::Find(&arr, 99);
		check("Find(99) == NULL", found == 0);
	}

	printf("[6] Find() on an empty array returns NULL (real InsertLevel()'s own "
	       "first-ever call path)\n");
	{
		COmegaPtrArray arr;
		CLevelManager *found = CLevelManagerArray::Find(&arr, 0);
		check("Find(0) on empty array == NULL", found == 0);
	}

	printf("[7] Add() first element (index 0) needs no sift -- doesn't touch "
	       "arr[-1]\n");
	{
		COmegaPtrArray arr;
		CLevelManagerArray::Add(&arr, (CLevelManager *)make_level(42));
		void **raw = *(void ***)((char *)&arr + 0x14);
		int count = *(int *)((char *)&arr + 0xc);
		check("count == 1", count == 1);
		check("single element reads back 42",
		      count == 1 && ((FakeLevelManager *)raw[0])->level == 42);
	}

	printf("\n%d checks failed\n", g_fail);
	return g_fail != 0;
}
