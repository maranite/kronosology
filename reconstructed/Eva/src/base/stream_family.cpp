/*
 * stream_family.cpp  -  see include/stream_family.h.
 *
 * Only the three Api+0x94 soft-assert helpers need out-of-line bodies; every
 * other method in this cluster is defined inline in the header (matching this
 * project's own established convention for small classes, e.g. bit_mask_l.h).
 * Real per-class file-name literals confirmed via `objdump -s -j .rodata` at the
 * exact addresses each class's own assert calls reference:
 *   CStream/CIn/COut/CInOut asserts -> "DMStream.cpp" (.rodata+0x08e7efd8)
 *   CMemory asserts                 -> "Memory.cpp"   (.rodata+0x08e7f9bb)
 *   CNullStr asserts                -> "NullStr.cpp"  (.rodata+0x08e7f9c6)
 * Shared format string "Assertion failed in module %s, line %i.\n"
 * (.rodata+0x08e7beff) -- same string already established in partition_table.cpp/
 * bit_mask_l.h's own ApiAssert() calls.
 */

#include "stream_family.h"
#include "system_api.h"

extern CSystemApi *Api; /* mains.cpp */

namespace {

inline void ApiAssertImpl(const char *file, int line)
{
	typedef void (*Fn)(void *, const char *, const char *, int);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x94);
	fn(Api, "Assertion failed in module %s, line %i.\n", file, line);
}

} // namespace

void CStream::ApiAssertStream(int line) { ApiAssertImpl("DMStream.cpp", line); }
void CNullStr::ApiAssertNullStr(int line) { ApiAssertImpl("NullStr.cpp", line); }
void CMemory::ApiAssertMemory(int line) { ApiAssertImpl("Memory.cpp", line); }
