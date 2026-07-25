// SPDX-License-Identifier: GPL-2.0
/*
 * test_multisample_bank_access.cpp  -  host-side known-answer test for
 * CSTGMultisampleBankManager::AccessBank (see
 * ../src/auth/multisample_bank_access.cpp).
 *
 * Mocks CSTGMultisampleBankManager_FindBankRecord (the deliberately
 * deferred hash-lookup callee) and the oa_heap_region() dependency chain
 * (CSTGHeapManager::sInstance + the captured-snapshot accessors it reads
 * instead of dereferencing CSTGHeapManager directly, per oa_heap.h's own
 * documented WORKAROUND) so the real slot->address arithmetic can be
 * exercised end to end with a predictable result.
 */

#include <cstdio>
#include <cstring>
#include <cstdint>
#include "oa_types.h"
#include "oa_heap.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-40s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-40s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

char *CSTGHeapManager::sInstance;

extern "C" {

static unsigned long g_capturedBase;
static unsigned int  g_capturedOffset[8];
unsigned long CSTGHeapManager_GetCapturedHeapBase(void) { return g_capturedBase; }
unsigned int  CSTGHeapManager_GetCapturedOffset(unsigned int slot)
{
	return (slot < 8) ? g_capturedOffset[slot] : 0xDEADBEEF;
}

static void   *g_findBankRecordReturn;
static CSTGMultisampleBankManager *g_findBankRecordSelfSeen;
static const CSTGMultisampleBankUUID *g_findBankRecordUuidSeen;
static int     g_findBankRecordCalls;
void *CSTGMultisampleBankManager_FindBankRecord(CSTGMultisampleBankManager *self,
						 const CSTGMultisampleBankUUID *uuid)
{
	g_findBankRecordCalls++;
	g_findBankRecordSelfSeen = self;
	g_findBankRecordUuidSeen = uuid;
	return g_findBankRecordReturn;
}

} /* extern "C" */

int main(void)
{
	/* Non-sentinel heap base -- oa_heap_base() only checks sInstance
	 * against the -44 "not up yet" sentinel; the real address math reads
	 * the captured snapshot accessors instead (see oa_heap.h). */
	CSTGHeapManager::sInstance = (char *)(intptr_t)0x1000;
	g_capturedBase = 0x20000000;
	g_capturedOffset[0] = 0x100;	/* slot 0 */
	g_capturedOffset[1] = 0x200;	/* slot 1 -- also oa_heap_base()'s own slot */

	unsigned char self[0xa030] = { 0 };
	CSTGMultisampleBankManager *mgr = (CSTGMultisampleBankManager *)self;

	printf("[1] ROM bank UUID (all-zero, matches kROMBankUUID placeholder) -> ROM fast path:\n");
	{
		*(unsigned int *)(self + 0xa020) = 0;	/* romBankSlotIndex = slot 0 */
		g_findBankRecordCalls = 0;
		CSTGMultisampleBankUUID uuid;
		memset(&uuid, 0, sizeof(uuid));
		void *result = CSTGMultisampleBankManager::AccessBank(mgr, &uuid);
		check_eq("FindBankRecord NOT called", g_findBankRecordCalls, 0);
		check_eq("result address", (long)(intptr_t)result,
			 (long)(intptr_t)(g_capturedOffset[0] + g_capturedBase));
	}

	printf("\n[2] ROM bank UUID, different cached slot index:\n");
	{
		*(unsigned int *)(self + 0xa020) = 1;	/* romBankSlotIndex = slot 1 */
		g_findBankRecordCalls = 0;
		CSTGMultisampleBankUUID uuid;
		memset(&uuid, 0, sizeof(uuid));
		void *result = CSTGMultisampleBankManager::AccessBank(mgr, &uuid);
		check_eq("FindBankRecord NOT called", g_findBankRecordCalls, 0);
		check_eq("result address", (long)(intptr_t)result,
			 (long)(intptr_t)(g_capturedOffset[1] + g_capturedBase));
	}

	printf("\n[3] Non-ROM UUID, FindBankRecord finds nothing -> null:\n");
	{
		g_findBankRecordCalls = 0;
		g_findBankRecordReturn = 0;
		CSTGMultisampleBankUUID uuid;
		memset(&uuid, 0x42, sizeof(uuid));
		void *result = CSTGMultisampleBankManager::AccessBank(mgr, &uuid);
		check_eq("FindBankRecord called once", g_findBankRecordCalls, 1);
		check_eq("FindBankRecord got mgr", (long)(intptr_t)g_findBankRecordSelfSeen, (long)(intptr_t)mgr);
		check_eq("FindBankRecord got uuid ptr", (long)(intptr_t)g_findBankRecordUuidSeen, (long)(intptr_t)&uuid);
		check_eq("result is null", (long)(intptr_t)result, 0);
	}

	printf("\n[4] Non-ROM UUID, FindBankRecord finds a record with slot 1 in its +0x00 field:\n");
	{
		unsigned char record[4];
		*(unsigned int *)record = 1;	/* record+0x00: heap slot index */
		g_findBankRecordCalls = 0;
		g_findBankRecordReturn = record;
		CSTGMultisampleBankUUID uuid;
		memset(&uuid, 0x42, sizeof(uuid));
		void *result = CSTGMultisampleBankManager::AccessBank(mgr, &uuid);
		check_eq("result address", (long)(intptr_t)result,
			 (long)(intptr_t)(g_capturedOffset[1] + g_capturedBase));
	}

	printf("\n[5] Non-ROM UUID, record's slot index out of range (>= 100000) -> null:\n");
	{
		unsigned char record[4];
		*(unsigned int *)record = 100000;
		g_findBankRecordReturn = record;
		CSTGMultisampleBankUUID uuid;
		memset(&uuid, 0x42, sizeof(uuid));
		void *result = CSTGMultisampleBankManager::AccessBank(mgr, &uuid);
		check_eq("result is null (out of range)", (long)(intptr_t)result, 0);
	}

	/* NOTE: unlike oa_heap_base(), oa_heap_region() (and therefore
	 * AccessBank's own address resolution) never checks the -44
	 * "heap not up yet" sentinel against CSTGHeapManager::sInstance --
	 * confirmed both in the real disassembly (AccessBank reads
	 * CSTGHeapManager::sInstance directly with no sentinel comparison at
	 * all, unlike e.g. klm_manager.cpp's authorization path) and in
	 * oa_heap_region()'s own header comment/body (oa_heap.h) -- so no
	 * "heap not up yet" test case belongs here. */

	printf(g_fail ? "\nRESULT: %d check(s) FAILED\n" : "\nRESULT: all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
