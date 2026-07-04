// SPDX-License-Identifier: GPL-2.0
/*
 * wave_seq_manager.cpp  -  CSTGWaveSeqManager::CSTGWaveSeqManager()/
 * Initialize(). See oa_engine_init.h's own file comment for the full
 * ground-truthing detail (confirmed layout, the CSTGWaveSeqGenerator
 * array, the two rtwrap mutexes, and the size cross-check against
 * engine_init.cpp's own allocation).
 *
 * Ground-truthed via readelf+objdump (`-j .text`) against OA_real.ko:
 *   CSTGWaveSeqManager::CSTGWaveSeqManager()  .text+0x87100, 222 bytes
 *   CSTGWaveSeqManager::Initialize()          .text+0x871e0, 151 bytes
 */

#include "oa_engine_init.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void);
extern "C" void *rtwrap_malloc(unsigned int size);
extern "C" void rtwrap_pthread_mutex_init(void *mutex, void *attr);

/* CSTGWaveSeqManager::sInstance is already defined in engine_init.cpp
 * (sec 10.58, which pre-declared this class's shape before its own
 * constructor/Initialize() were reconstructed) -- NOT redefined here. */
void **CSTGWaveSeqGenerator::sMutex;

/* Host/target pointer-width fix (this project's established pattern):
 * the real target's mutex fields are plain 32-bit pointers, but a
 * native host pointer is 8 bytes on a 64-bit build -- storing one
 * directly via `*(void**)` would silently overrun a 4-byte target
 * field (caught here, sec 10.64, before it became a real bug: an
 * earlier draft of this exact function did exactly that). */
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

CSTGWaveSeqManager::CSTGWaveSeqManager()
{
	unsigned char *this_ = reinterpret_cast<unsigned char *>(this);

	/* 200 (0xe100/0x120) CSTGWaveSeqGenerator sub-objects, confirmed
	 * default-constructed in place. */
	for (unsigned int off = 0; off < 0xe100; off += 0x120)
		new (this_ + off) CSTGWaveSeqGenerator();

	*(unsigned int *)(this_ + 0xe104) = 0;
	*(unsigned int *)(this_ + 0xe100) = 0;
	*(unsigned int *)(this_ + 0xe108) = 0;
	*(unsigned int *)(this_ + 0xe110) = 0;
	*(unsigned int *)(this_ + 0xe10c) = 0;
	*(unsigned int *)(this_ + 0xe114) = 0;
	*(unsigned int *)(this_ + 0xe11c) = 0;
	*(unsigned int *)(this_ + 0xe118) = 0;
	*(unsigned int *)(this_ + 0xe120) = 0;

	void *mutex1 = rtwrap_malloc(get_sizeof_rtwrap_pthread_mutex());
	*(unsigned int *)(this_ + 0xe12c) = ToU32(mutex1);
	rtwrap_pthread_mutex_init(mutex1, 0);
	void *mutex2 = rtwrap_malloc(get_sizeof_rtwrap_pthread_mutex());
	*(unsigned int *)(this_ + 0xe130) = ToU32(mutex2);
	rtwrap_pthread_mutex_init(mutex2, 0);

	sInstance = this;

	this_[0xe124] = 0;
	*(unsigned short *)(this_ + 0xe126) = 0;
	this_[0xe125] = 0;
	*(unsigned short *)(this_ + 0xe128) = 0;
}

void CSTGWaveSeqManager::Initialize()
{
	unsigned char *this_ = reinterpret_cast<unsigned char *>(this);

	/* Real intrusive doubly-linked "available generator" list, built
	 * with a genuine push-FRONT insertion (confirmed instruction by
	 * instruction -- NOT the same insertion order as CSTGHeapManager's
	 * own free-list build, sec 10.59, which appends at the tail; only
	 * the 3-field next/prev/owner node convention is shared). The
	 * FIRST insertion (empty-list case) never writes the new node's
	 * own "next" field (+0x0) at all -- confirmed real, left at
	 * whatever CSTGWaveSeqGenerator's own constructor set it to
	 * (presumably already 0), preserved verbatim rather than "fixed"
	 * by also zeroing it here. */
	for (unsigned int i = 0; i < 200; i++) {
		unsigned char *gen = this_ + i * 0x120;
		unsigned int genAddr = (unsigned int)(unsigned long)gen;

		((CSTGWaveSeqGenerator *)gen)->Init();

		unsigned int oldHead = *(unsigned int *)(this_ + 0xe100);
		if (oldHead == 0) {
			*(unsigned int *)(this_ + 0xe104) = genAddr; /* first insertion == permanent tail */
		} else {
			unsigned char *head = (unsigned char *)(unsigned long)oldHead;
			unsigned int headPrev = *(unsigned int *)(head + 0x4);
			*(unsigned int *)(gen + 0x4) = headPrev;
			if (headPrev != 0)
				*(unsigned int *)(unsigned long)headPrev = genAddr;
			*(unsigned int *)(head + 0x4) = genAddr;
			*(unsigned int *)(gen + 0x0) = oldHead;
		}
		*(unsigned int *)(this_ + 0xe100) = genAddr;
		*(unsigned int *)(gen + 0xc) = (unsigned int)(unsigned long)(this_ + 0xe100);
		*(unsigned int *)(this_ + 0xe108) += 1;
	}

	/* Real "address of the singleton pointer" idiom (see
	 * oa_engine_init.h's own note on CSTGWaveSeqGenerator::sMutex). */
	CSTGWaveSeqGenerator::sMutex = (void **)(this_ + 0xe130);
}
