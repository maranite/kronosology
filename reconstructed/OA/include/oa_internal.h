// SPDX-License-Identifier: GPL-2.0
/*
 * oa_internal.h  -  small freestanding primitives used inside the OA module (Stage 0).
 *
 * Kept header-only and libc-free so the reconstructed translation units build in the
 * kernel/STG freestanding environment (Linux 2.6.32 + RTAI, x86-32, no C++ runtime).
 */

#ifndef OA_INTERNAL_H
#define OA_INTERNAL_H

typedef __SIZE_TYPE__ oa_size_t;

/* Placement new — the STG runtime constructs sub-objects in place (see the KLM ctor). */
inline void *operator new(oa_size_t, void *p) throw() { return p; }
inline void  operator delete(void *, void *) throw() {}

/* 64-bit cycle counter; its low 32 bits become the per-boot authorization key. */
static inline unsigned long long rdtsc(void)
{
	unsigned int lo, hi;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)hi << 32) | lo;
}

/* Offset of a voice model's "extra" authorization word (vm[1].+0x4 in the decompile). */
#define VM_EXTRA_OFF 0x104

/* Provided by the kernel/STG runtime. */
extern "C" oa_size_t strlen(const char *s);

#endif /* OA_INTERNAL_H */
