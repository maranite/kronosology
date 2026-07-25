// SPDX-License-Identifier: GPL-2.0
/*
 * oa_heap.h  -  accessors into the STG heap-managed global region.
 *
 * CSTGHeapManager owns a relocated block of STG globals; subsystems reach their state
 * through two recovered idioms.  Kept here so the heap-internal offsets live in one place
 * rather than being copied across the auth translation units.
 */

#ifndef OA_HEAP_H
#define OA_HEAP_H

#include "oa_types.h"

/*
 * Base of the heap-managed global sub-region (the bank manager, the PCM
 * precache manager, and the installed-products registry all live at fixed
 * offsets from here).
 *
 * CORRECTED (2026-07-01, found while reconstructing the SO:/PT:/PC: /proc/.oacmd
 * commands): every real call site that resolves this base -- confirmed in
 * CSTGKLMManager::AuthorizeMultisampleBank (klm_manager.cpp's own real
 * disassembly), ProcessOACmd's "AU:" handler, and ProcessOACmd's CB:/CL/
 * SO:/PC: handlers alike -- guards not against a NULL CSTGHeapManager::sInstance,
 * but against a SPECIFIC sentinel value, -44 (0xFFFFFFD4). An earlier version
 * of this header checked `!heap` instead, which is wrong: if sInstance is
 * ever genuinely 0 the real code does NOT special-case it and would
 * dereference offset 0x38, so 0xFFFFFFD4 is the one deliberate
 * "heap not up yet" marker, not "any falsy pointer".
 */
/*
 * WORKAROUND (2026-07-24): both functions below used to dereference the
 * CSTGHeapManager object directly for heapBase/each handle-table entry's
 * own `offset` field. A live kronos_vm boot proved that data isn't
 * reliably readable by any caller other than CSTGHeapManager::
 * Initialize()/Alloc() themselves -- see heap_manager.cpp's own file
 * comment for the full investigation (offset formulas independently
 * verified correct; simple dead-store elimination and cross-TU
 * visibility both ruled out; a GDB hardware-watchpoint attempt never
 * fired across a full boot). Both now read the reliable captured-value
 * snapshots those functions maintain outside the object instead.
 */
extern "C" unsigned long CSTGHeapManager_GetCapturedHeapBase(void);
extern "C" unsigned int CSTGHeapManager_GetCapturedOffset(unsigned int slot);

static inline char *oa_heap_base(void)
{
	char *heap = (char *)CSTGHeapManager::sInstance;
	if (heap == (char *)(long)-44)		/* 0xFFFFFFD4 sentinel: heap not yet up */
		return 0;
	/* Slot 1 (the very first CSTGHeapManager::Alloc() call of the whole
	 * boot) is what the real `*(heap+0x38)` term structurally resolves
	 * to -- see setup_global_resources.cpp's own local_heap_base()
	 * comment for the derivation. */
	return (char *)(CSTGHeapManager_GetCapturedOffset(1) +
			(unsigned int)CSTGHeapManager_GetCapturedHeapBase());
}

/* Resolve a heap "slot" number (< 100000) to its region base.  Product authorization-entry
 * tables and the installed-product array are each addressed this way. */
static inline char *oa_heap_region(unsigned int slot)
{
	if (slot >= 100000)
		return 0;
	return (char *)(CSTGHeapManager_GetCapturedOffset(slot) +
			(unsigned int)CSTGHeapManager_GetCapturedHeapBase());
}

/* Multisample bank manager singleton, at a fixed offset from the heap base.
 * Recovered independently from CSTGKLMManager's authorization path
 * (klm_manager.cpp) and ProcessOACmd's LM/LD/CM/CD dispatch
 * (process_oacmd.cpp), both of which compute this same offset -- promoted
 * here per this header's own purpose (one home for recovered heap offsets). */
static inline struct CSTGMultisampleBankManager *oa_multisample_bank_manager(void)
{
	return (struct CSTGMultisampleBankManager *)(oa_heap_base() + 0x60524);
}

/* PCM precache manager singleton, at a fixed offset from the heap base.
 * Recovered from ProcessOACmd's "PC:" /proc/.oacmd handler
 * (process_oacmd.cpp). */
static inline struct CSTGPCMPrecacheManager *oa_pcm_precache_manager(void)
{
	return (struct CSTGPCMPrecacheManager *)(oa_heap_base() + 0x6a54c);
}

#endif /* OA_HEAP_H */
