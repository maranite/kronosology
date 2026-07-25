/*
 * eva_types.h  -  shared low-level types for the Eva boot-path reconstruction (Stage 0/1).
 *
 * Eva is a normal dynamically-linked x86-32 userspace ELF (GCC 4.5.0 era), not a
 * freestanding kernel module -- see PLAN.md's "What's different from OA.ko" section
 * before assuming any OA.ko convention carries over unchanged.
 *
 * Only the minimal faithful shapes needed to compile the Stage-1 boot-path translation
 * units are declared here; classes get filled in incrementally as later stages need more
 * of their real layout. Offsets/sizes recovered from Eva's own (unstripped) symbol table
 * and Ghidra decompile export (/home/share/Decomp/EVA_Decomp/eva_export/).
 */

#ifndef EVA_TYPES_H
#define EVA_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* CSTGHandle -- shared-memory handle wrapper.
 * USTGUserAPI::Connect() constructs one on the stack (mode/id = 1) and calls
 * Access() on it to attach; Access() (.text+0x08e31e80, src/ipc/stg_handle.cpp)
 * is Tier A -- opens /proc/.shm, ioctl's for offset/size, mmap()s, caches by
 * mode/id in CSTGHandleCache::sCachedHandleInfo. Eva's Connect() path
 * double-calls it (once on the stack handle, once on the resulting pointer,
 * reusing the returned address as if it were itself a mode/id) -- faithfully
 * preserved rather than resolved further.
 *
 * GetSize() (.text+0x08e32090) and Release() (.text+0x08e31ff0) are this
 * class's other two real methods -- both Tier A, Stage 2 IPC substrate
 * (2026-07-25). Release() decrements the cache entry's refcount, munmap()ing
 * and zeroing the entry only when the count drops to zero; GetSize() returns
 * the cached size, populating the cache via a fresh /proc/.shm ioctl(0x65) if
 * this is the first query for that mode/id. Declared as real members (not
 * free functions) so the mangled names match the original.
 */
struct CSTGHandle {
	uint32_t mode; /* local_10[0] = 1 in USTGUserAPI::Connect -- exact field name/meaning TBD */

	void *Access() const;
	int GetSize() const;
	void Release() const;
};

#endif /* EVA_TYPES_H */
