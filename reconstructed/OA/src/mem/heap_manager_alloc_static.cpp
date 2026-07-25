// SPDX-License-Identifier: GPL-2.0
/*
 * heap_manager_alloc_static.cpp  -  batch 17: CSTGHeapManager::
 * Alloc(unsigned int), this project's own LOCAL "static" stand-in for
 * the real instance method CSTGHeapManager::Alloc(unsigned long) (see
 * oa_heapmanager.h / src/mem/heap_manager.cpp for the full class + real
 * ground-truthing, and oa_setup_global_resources.h's own file comment
 * for why setup_global_resources.cpp can't include oa_heapmanager.h
 * directly -- the pre-existing CSTGGlobal ODR conflict).
 *
 * WORKAROUND (2026-07-24, superseding the original raw-offset
 * transliteration): a live kronos_vm boot proved this object's own
 * free-list/handle-table bookkeeping is NOT reliably readable by ANY
 * caller other than CSTGHeapManager::Initialize() itself -- see
 * heap_manager.cpp's own extensive file comment for the full
 * investigation trail (offset formulas independently verified correct;
 * simple dead-store elimination and cross-TU visibility both ruled out;
 * a GDB hardware-watchpoint attempt never fired across a full boot).
 * Delegates to `CSTGHeapManager_BumpAlloc()`, a trivial monotonic bump
 * allocator built entirely on shadow state outside the CSTGHeapManager
 * object, SHARED with heap_manager.cpp's own real `Alloc(unsigned long)`
 * overload (both overloads drawing from the same cursor/slot-counter is
 * what keeps their allocations from overlapping, regardless of which
 * overload a given caller happens to use -- `CSTGMidiQueue::Initialize()`
 * uses the real `Alloc(unsigned long)`, `setup_global_resources.cpp`
 * uses this stand-in, and both need to hand out non-overlapping regions
 * of the SAME arena).
 *
 * `oa_setup_global_resources.h`'s own local minimal stand-in:
 *   struct CSTGHeapManager {
 *       static char *sInstance;
 *       static unsigned int Alloc(unsigned int size);
 *   };
 * is only a raw pointer (not the real class layout, again to dodge the
 * ODR conflict) -- `Alloc(unsigned int)` here is therefore a SEPARATE,
 * differently-mangled symbol (`_ZN15CSTGHeapManager5AllocEj`) from the
 * real `_ZN15CSTGHeapManager5AllocEm` (`unsigned long`) already
 * reconstructed in heap_manager.cpp, NOT an overload conflict. `sInstance`
 * itself IS the same shared symbol either way (Itanium ABI static-data-
 * member mangling ignores the declared type), already defined once in
 * heap_manager.cpp.
 *
 * Deliberately its OWN translation unit, not setup_global_resources.cpp
 * itself: test_setup_global_resources.cpp links setup_global_resources.cpp
 * directly and carries its own load-bearing call-counting mock of this
 * exact symbol (deterministic per-call slot assignment) -- giving the
 * real body a separate file keeps that test's mock untouched, matching
 * this project's established "dedicated TU" precedent.
 */

#include "oa_setup_global_resources.h"

/* Shared bump-allocator core, defined in heap_manager.cpp -- see that
 * file's own comment for the full rationale. */
extern "C" unsigned int CSTGHeapManager_BumpAlloc(unsigned int size);

unsigned int CSTGHeapManager::Alloc(unsigned int size)
{
	unsigned char *heap = (unsigned char *)sInstance;
	if (heap == (unsigned char *)(long)-44) /* heap not up yet */
		return (unsigned int)-1;

	return CSTGHeapManager_BumpAlloc(size);
}
