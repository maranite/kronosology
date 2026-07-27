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

/* CValue -- opaque wire-format tagged value used by several real USTGAPIXxx methods
 * (ustg_api_wrappers.h's own header comment first flagged this boundary; USTGAPIKLM's
 * own GetProductInfo()/GetProductItemInfo() decoded it further, 2026-07-27). Real
 * full field-level layout/semantics are still NOT reconstructed anywhere in this
 * project (same "not modeled, no reconstructed code needs more than this" boundary
 * as STGMessage itself) -- but two concrete serialization RULES real code applies
 * directly to it ARE decoded, both confirmed from real disassembly, not guessed:
 *
 *   - a fixed "scalar dword" encoding (USTGAPIKLM::GetProductInfo, writing a
 *     product's identifier): byte 0 = tag (1), byte 1 = length-in-bytes (4),
 *     bytes 2-3 = 0 padding, bytes 4-7 = the raw dword payload. 8 bytes total.
 *   - a self-describing variable-length blob copy (USTGAPIKLM::GetProductItemInfo,
 *     and independently the 4 CValue-taking USTGAPIXxx methods
 *     ustg_api_wrappers.h defers): `size = *(byte at src+1) + 4`, i.e. the same
 *     byte-1-is-a-length-prefix rule, just copied verbatim via memcpy() instead of
 *     field-by-field -- confirms the length-prefix convention is CValue's own
 *     general wire shape, not specific to the scalar-dword case above.
 *
 * Declared here as a plain byte buffer (not a real class with named fields) sized
 * with headroom over the largest concrete encoding any reconstructed caller
 * touches so far (the 20-byte item-value case in USTGAPIKLM) -- purely so callers
 * can hold/pass one by reference; NOT a claim about CValue's real C++ layout.
 */
struct CValue {
	unsigned char raw[24];
};

/* Writes the "scalar dword" CValue encoding described above directly into `dest`. */
void WriteCValueDword(CValue &dest, unsigned int value);

/* memcpy()s a self-describing CValue-shaped byte blob (size = *(src+1) + 4) from
 * `src` into `dest`. `src` need not itself be a CValue -- real callers pass a raw
 * pointer into a larger record (see USTGAPIKLM::GetProductItemInfo).
 */
void CopyCValueBlob(CValue &dest, const void *src);

#endif /* EVA_TYPES_H */
