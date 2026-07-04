// SPDX-License-Identifier: GPL-2.0
/*
 * vector_manager.cpp  -  CSTGVectorManager::CSTGVectorManager(). See
 * oa_engine_init.h's own file comment for the full ground-truthing
 * detail (confirmed layout, the three CSTGVectorEG* classes, and the
 * size cross-check against engine_init.cpp's own allocation).
 *
 * Ground-truthed via readelf+objdump (`-j .text`) against OA_real.ko:
 *   CSTGVectorManager::CSTGVectorManager()  .text+0x73710, 3279 bytes
 *
 * `CSTGVectorManager::Initialize()` (.text+0x743e0, 2350 bytes) is a
 * SEPARATE, comparably-sized task (real virtual dispatch -- `call
 * DWORD PTR [edx]` -- across all ~868 embedded vector-EG objects, not
 * mechanical field population like the constructor) -- deliberately
 * out of scope for this pass, left as a confirmed-real, deliberately
 * deferred extern.
 */

#include "oa_engine_init.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void);
extern "C" void *rtwrap_malloc(unsigned int size);
extern "C" void rtwrap_pthread_mutex_init(void *mutex, void *attr);

/* CSTGVectorManager::sInstance is already defined in engine_init.cpp
 * (sec 10.58), matching CSTGWaveSeqManager/CSTGMidiDispatcher's own
 * treatment (sec 10.62/10.63) -- NOT redefined here. */

/* Host/target pointer-width fix (this project's established pattern):
 * the real target's mutex fields are plain 32-bit pointers, but a
 * native host pointer is 8 bytes on a 64-bit build. */
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

CSTGVectorManager::CSTGVectorManager()
{
	unsigned char *p = reinterpret_cast<unsigned char *>(this);

	/* 400x CSTGVectorEGXOnly (0x88/136 bytes each), a real loop. */
	for (unsigned int off = 0; off < 0xd480; off += 0x88)
		new (p + off) CSTGVectorEGXOnly();

	/* 400x CSTGVectorEGXY (0x7c/124 bytes each), a real loop. */
	for (unsigned int off = 0xd480; off < 0x19640; off += 0x7c)
		new (p + off) CSTGVectorEGXY();

	/* 17x CSTGVectorEGCC, compiler-unrolled (not a loop in the real binary). */
	for (unsigned int off = 0x19640; off < 0x19db0; off += 0x70)
		new (p + off) CSTGVectorEGCC();

	/* 16x CSTGVectorEGXOnly, compiler-unrolled. */
	for (unsigned int off = 0x19db0; off < 0x1a630; off += 0x88)
		new (p + off) CSTGVectorEGXOnly();

	/* 16x CSTGVectorEGXY, compiler-unrolled. */
	for (unsigned int off = 0x1a630; off < 0x1adf0; off += 0x7c)
		new (p + off) CSTGVectorEGXY();

	/* Confirmed real zeroed region. */
	for (unsigned int off = 0x1adf0; off < 0x1af70; off += 4)
		*(unsigned int *)(p + off) = 0;

	/* +0x1af70..+0x1aff4 (132 bytes): confirmed real gap, never
	 * written anywhere in this constructor -- preserved verbatim. */

	/* 17x CSTGVectorEGCC, compiler-unrolled. */
	for (unsigned int off = 0x1aff4; off < 0x1b764; off += 0x70)
		new (p + off) CSTGVectorEGCC();

	/* 16x CSTGVectorEGXOnly, compiler-unrolled. */
	for (unsigned int off = 0x1b764; off < 0x1bfe4; off += 0x88)
		new (p + off) CSTGVectorEGXOnly();

	/* 16x CSTGVectorEGXY, compiler-unrolled. */
	for (unsigned int off = 0x1bfe4; off < 0x1c7a4; off += 0x7c)
		new (p + off) CSTGVectorEGXY();

	/* Confirmed real zeroed region. */
	for (unsigned int off = 0x1c7a4; off < 0x1c924; off += 4)
		*(unsigned int *)(p + off) = 0;

	/* +0x1c924..+0x1c9ac (136 bytes): confirmed real gap, never
	 * written anywhere in this constructor -- preserved verbatim. */

	/* Confirmed real zeroed region (right before the 3 mutex fields). */
	for (unsigned int off = 0x1c9ac; off < 0x1c9dc; off += 4)
		*(unsigned int *)(p + off) = 0;

	void *mutex1 = rtwrap_malloc(get_sizeof_rtwrap_pthread_mutex());
	*(unsigned int *)(p + 0x1c9dc) = ToU32(mutex1);
	rtwrap_pthread_mutex_init(mutex1, 0);
	void *mutex2 = rtwrap_malloc(get_sizeof_rtwrap_pthread_mutex());
	*(unsigned int *)(p + 0x1c9e0) = ToU32(mutex2);
	rtwrap_pthread_mutex_init(mutex2, 0);
	void *mutex3 = rtwrap_malloc(get_sizeof_rtwrap_pthread_mutex());
	*(unsigned int *)(p + 0x1c9e4) = ToU32(mutex3);
	rtwrap_pthread_mutex_init(mutex3, 0);

	sInstance = this;
}

/*
 * OnUpdateGlobalMidiChannel(unsigned char) (sec 10.124): see
 * oa_engine_init.h for the full confirmed shape.
 */
void CSTGVectorManager::OnUpdateGlobalMidiChannel(unsigned char channel)
{
	unsigned char *base = (unsigned char *)this;
	base[0x19da4] = channel;
	base[0x1b758] = channel;
}
