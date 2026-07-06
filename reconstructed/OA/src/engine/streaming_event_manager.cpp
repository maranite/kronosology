// SPDX-License-Identifier: GPL-2.0
/*
 * streaming_event_manager.cpp  -  CSTGStreamingEvent::CSTGStreamingEvent()
 * (`.text+0xd2090`, 72 bytes) and CSTGStreamingEventManager's own ctor
 * (`.text+0xd1b40`, 156 bytes) and Initialize() (`.text+0xd1be0`, 200
 * bytes) -- sec 10.158. See oa_engine_init.h for the full confirmed field
 * layout of both classes.
 *
 * Kept in its own translation unit (not managers.cpp, not engine_init.cpp)
 * specifically so `CSTGStreamingEventManager::sInstance`'s storage can move
 * here without colliding with verify/test_engine_init.cpp's own pre-existing
 * mock ctor/Initialize() for this exact class (that test deliberately mocks
 * every dependency and does NOT link managers.cpp -- see that file's own
 * header comment -- so it needed its own local sInstance storage added
 * instead, matching the CLoadBalancer/CSTGDiskCostManager precedent already
 * used there for other classes whose real bodies live elsewhere).
 */

#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void);
extern "C" void *rtwrap_malloc(unsigned int size);
extern "C" void rtwrap_pthread_mutex_init(void *mutex, void *attr);

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

CSTGStreamingEventManager *CSTGStreamingEventManager::sInstance;

/*
 * CSTGStreamingEvent::CSTGStreamingEvent(): base ctor, own vtable-pointer
 * overwrite (standard Itanium "+8 skip offset-to-top/RTTI" convention),
 * self-pointer stored at +0x38 (packed 32-bit -- this is the very field
 * CSTGStreamingEventManager::Initialize() below reads back as
 * `oldTailPtr`/etc, so it MUST be a real, dereferenceable address once
 * this object lives inside a MAP_32BIT-backed CSTGStreamingEventManager),
 * three zeroed dwords at +0x30/+0x34/+0x3c, an embedded
 * CSTGHDRCircularBuffer base-object ctor at +0x40, and a final AND-mask
 * clearing bit 4 of the flag byte at +0xd1 (confirmed real, an
 * inherited/preceding sub-object must already have set other bits there --
 * same class of evidence as CSTGMetronome's own leading AND-mask, sec
 * 10.13).
 */
CSTGStreamingEvent::CSTGStreamingEvent()
{
	new (this) CSTGAudioEvent();
	*(unsigned int *)this = ToU32(_ZTV18CSTGStreamingEvent + 8);

	unsigned char *self = (unsigned char *)this;
	*(unsigned int *)(self + 0x38) = ToU32(self);
	*(unsigned int *)(self + 0x30) = 0;
	*(unsigned int *)(self + 0x34) = 0;
	*(unsigned int *)(self + 0x3c) = 0;
	new (self + 0x40) CSTGHDRCircularBuffer();
	self[0xd1] &= 0xef;
}

CSTGStreamingEventManager::CSTGStreamingEventManager()
{
	/* events[401] is default-constructed automatically (a real C++ array
	 * member) -- matches the confirmed clean 0xd4-byte-stride loop with
	 * nothing else interleaved between elements. */
	unsigned char *p = (unsigned char *)this;

	*(unsigned int *)(p + 0x14c1c) = 0;
	*(unsigned int *)(p + 0x14c18) = 0;
	*(unsigned int *)(p + 0x14c20) = 0;
	*(unsigned int *)(p + 0x14c28) = 0;
	*(unsigned int *)(p + 0x14c24) = 0;
	*(unsigned int *)(p + 0x14c2c) = 0;

	unsigned int mutexSize = get_sizeof_rtwrap_pthread_mutex();
	void *mutex = rtwrap_malloc(mutexSize);
	*(unsigned int *)(p + 0x14c30) = ToU32(mutex);
	rtwrap_pthread_mutex_init(mutex, 0);

	*(unsigned int *)(p + 0x14c38) = 0;

	CSTGStreamingEventManager::sInstance = this;
}

/*
 * Initialize(numEvents, size): builds a singly-linked free list threading
 * every populated event's own +0x30 field (see CSTGStreamingEvent's class
 * comment), "insert at tail" style -- same owner-back-pointer idiom
 * (+0x3c = a fixed address of this manager's own freeListHead slot, stored
 * into EVERY node) already confirmed for CSTGSmoother's/
 * CSTGFrontPanelSmoothers' own free lists and CSTGVoiceAllocator::
 * Initialize()'s three lists (sec 10.157).
 *
 * Real, faithfully-preserved quirk: the `if (nextOfOldTail) ...` inner
 * write is DEAD in practice for this simple sequential-append loop
 * (nextOfOldTail is always 0 the first time any given tail slot is read
 * here, since nothing else in this loop ever writes a non-zero value into
 * a node's own +0x30 before the NEXT iteration reads it) -- reproduced
 * anyway rather than collapsed away, matching the sec 10.157 "harmless
 * redundant real write, left uncollapsed" precedent (this is the same
 * generic "insert at tail" list-node shape, apparently reused/inlined from
 * a shared idiom elsewhere in this binary).
 */
void CSTGStreamingEventManager::Initialize(unsigned short numEvents, unsigned long size)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int headSlotAddr = ToU32(base + 0x14c18);
	unsigned int perEventArg = size * 2;

	*(unsigned int *)(base + 0x14c40) = perEventArg;

	for (unsigned short i = 0; i < numEvents; i++) {
		unsigned char *ev = (unsigned char *)&events[i];

		*(unsigned short *)(ev + 0x4) = i;

		CSTGHDRCircularBuffer *circBuf = (CSTGHDRCircularBuffer *)(ev + 0x40);
		circBuf->Initialize(perEventArg, false, 0xe);
		/* Confirmed real: overwrites the just-initialized circular
		 * buffer's own +0x0 field with this entry's own array index --
		 * see CSTGHDRCircularBuffer's own class comment (oa_engine.h). */
		*(unsigned int *)(ev + 0x40) = i;

		unsigned int linkNode = ToU32(ev + 0x30);
		unsigned int oldTail = *(unsigned int *)(base + 0x14c1c);
		if (oldTail) {
			*(unsigned int *)(ev + 0x34) = oldTail;
			unsigned int nextOfOldTail = *(unsigned int *)FromU32(oldTail);
			*(unsigned int *)(ev + 0x30) = nextOfOldTail;
			if (nextOfOldTail)
				*(unsigned int *)(FromU32(nextOfOldTail) + 4) = linkNode;
			*(unsigned int *)FromU32(oldTail) = linkNode;
		} else {
			*(unsigned int *)(base + 0x14c18) = linkNode;
		}
		*(unsigned int *)(base + 0x14c1c) = linkNode;
		*(unsigned int *)(ev + 0x3c) = headSlotAddr;
		*(unsigned int *)(base + 0x14c20) += 1;
	}

	*(unsigned int *)(base + 0x14c3c) = 0;
}
