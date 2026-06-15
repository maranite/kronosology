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

/* Base of the heap-managed global sub-region (the bank manager and the installed-products
 * registry live at fixed offsets from here).  0 when the heap singleton is not yet up. */
static inline char *oa_heap_base(void)
{
	char *heap = (char *)CSTGHeapManager::sInstance;
	if (!heap)
		return 0;
	return (char *)(*(unsigned int *)(heap + 0x38) + *(unsigned int *)(heap + 0x1e8498));
}

/* Resolve a heap "slot" number (< 100000) to its region base.  Product authorization-entry
 * tables and the installed-product array are each addressed this way. */
static inline char *oa_heap_region(unsigned int slot)
{
	char *heap = (char *)CSTGHeapManager::sInstance;
	if (slot >= 100000)
		return 0;
	return (char *)(*(unsigned int *)(heap + 0x24 + slot * 0x14) +
			*(unsigned int *)(heap + 0x1e8498));
}

#endif /* OA_HEAP_H */
