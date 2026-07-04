// SPDX-License-Identifier: GPL-2.0
/*
 * new_delete.cpp  -  see include/oa_new_delete.h.
 * Ground-truthed offsets: operator new/new[]/delete/delete[]
 * .text+0x118d10/0x118d20/0x118d30/0x118d40 (15 bytes each);
 * stg_kmalloc/stg_kfree .text+0x118d60/0x118d80 (20/15 bytes).
 */

#include "oa_new_delete.h"

/*
 * __cxa_pure_virtual (.text+0x118d00 in OA_real.ko, immediately before
 * operator new at +0x118d10) -- confirmed real: a single `ret`
 * instruction, no-op. The real ELF has this defined locally (not an
 * unresolved kernel/libgcc symbol), matching the Itanium C++ ABI's own
 * "safety net" contract (called only if a pure virtual is somehow
 * invoked without an override -- should never happen in correct code).
 */
extern "C" void __cxa_pure_virtual()
{
}

extern "C" void *stg_kmalloc(oa_size_t size)
{
	return __kmalloc(size, OA_GFP_KERNEL);
}

extern "C" void stg_kfree(void *ptr)
{
	kfree(ptr);
}

void *operator new(oa_size_t size)
{
	return stg_kmalloc(size);
}

void *operator new[](oa_size_t size)
{
	return stg_kmalloc(size);
}

/*
 * Deliberately no sized-deallocation overloads (`operator delete(void*,
 * oa_size_t)` etc.) -- the real ELF symbol table has only the unsized forms
 * (`_ZdlPv`/`_ZdaPv`), confirming this OA.ko build predates/doesn't use
 * C++14 sized deallocation. A modern host g++ warns about this
 * (-Wsized-deallocation); that warning is itself corroborating evidence,
 * not a defect to silence by adding overloads that don't exist for real.
 */
void operator delete(void *ptr)
{
	stg_kfree(ptr);
}

void operator delete[](void *ptr)
{
	stg_kfree(ptr);
}
