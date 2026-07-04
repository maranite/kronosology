// SPDX-License-Identifier: GPL-2.0
/*
 * vector_manager_init.cpp  -  CSTGVectorManager::Initialize(). See
 * vector_manager.cpp's own file comment for the constructor's ground-
 * truthing detail (the array layout this function walks).
 *
 * Ground-truthed via readelf+objdump (`-j .text`) against OA_real.ko:
 *   CSTGVectorManager::Initialize()  .text+0x743e0, 2350 bytes
 *
 * Extracted via a full scripted pass over every relocated call and
 * every literal-immediate write in program order (matching the
 * constructor's own methodology at this scale), not hand-transcribed
 * line by line.
 *
 * CONFIRMED real structure -- five distinct phases:
 *
 *  1. 400x CSTGVectorEGXOnly[i] (i=0..399, the main array from the
 *     constructor): a real virtual dispatch through each object's own
 *     vtable slot 0, an index stamped at object+0x4 (a 16-bit field,
 *     = i), then a genuine intrusive doubly-linked-list PUSH-FRONT
 *     insertion of a node embedded at object+0x3c/+0x40 (next/prev)
 *     with an owner backpointer at object+0x48 -- the exact same
 *     next/prev/owner node convention already confirmed for
 *     CSTGWaveSeqGenerator (sec 10.62), here at a non-zero object
 *     offset since CSTGVectorEGXOnly has other fields before it.
 *     List head/tail/count live at manager+0x1c9ac/+0x1c9b0/+0x1c9b4.
 *
 *  2. 400x CSTGVectorEGXY[i] (i=0..399): identical shape to phase 1,
 *     same object-relative field offsets (+0x4 index, +0x3c/+0x40/
 *     +0x48 node/owner), different list (head/tail/count at
 *     manager+0x1c9b8/+0x1c9bc/+0x1c9c0).
 *
 *  3. A confirmed real 4-byte marker write (manager+0x1aff0 word = 0,
 *     +0x1aff2 byte = 0, +0x1aff3 byte = 1) between phase 2 and phase
 *     4 -- real, not independently understood beyond its confirmed
 *     value and location (right before the constructor's own
 *     "batch2" object range starts, at +0x1aff4).
 *
 *  4. 17x CSTGVectorEGCC[i] (i=0..16, the constructor's own "batch1"
 *     EGCC array at manager+0x19640, stride 0x70): vtable slot 0
 *     dispatch + a LITERAL (not computed) index stamp at object+0x4.
 *     NO list insertion -- confirmed real, EGCC never appears in
 *     either intrusive list this function builds.
 *
 *  5. 16 PAIRED iterations (i=0..15) over the constructor's own
 *     "batch1" EGXOnly (manager+0x19db0, stride 0x88) and EGXY
 *     (manager+0x1a630, stride 0x7c) arrays: for each i, both
 *     objects get a vtable slot 0 dispatch and an index stamp of
 *     `400+i` at object+0x4 -- CONFIRMED to share the exact same
 *     index value per pair (not two independent sequences) -- but,
 *     unlike phases 1/2, NEITHER gets inserted into any list. Also
 *     zeroes 4 parallel 16-entry WORD tables at manager+0x1af70/
 *     +0x1af90/+0x1afb0/+0x1afd0 (2-byte stride each, so table[i] at
 *     base+i*2), one entry per pair -- confirmed real, not
 *     independently understood beyond their confirmed location.
 *
 * CONFIRMED REAL ASYMMETRY, preserved verbatim: this function NEVER
 * touches the constructor's own "batch2" object ranges (the second
 * 17 EGCC / 16 EGXOnly / 16 EGXY built at +0x1aff4/+0x1b764/+0x1bfe4)
 * -- verified via an exhaustive scan of every relocated call and
 * every literal write in the whole 2350-byte function; none reference
 * those address ranges. Those objects are real (their own
 * constructors ran), just never vtable-dispatched, index-stamped, or
 * list-linked by this function. Not a gap in this pass's own
 * analysis -- a genuine feature of the real code, matching this
 * project's "preserve real quirks" policy.
 *
 * Tail: `CSTGVectorEGXOnly::sMutex = &manager->+0x1c9e0` (the SECOND
 * mutex the constructor allocated), `CSTGVectorEGXY::sMutex =
 * &manager->+0x1c9e4` (the THIRD) -- the same "address of the
 * singleton pointer" idiom already confirmed multiple times elsewhere
 * in this project (e.g. CSTGWaveSeqGenerator::sMutex, sec 10.62). The
 * FIRST mutex (+0x1c9dc) is never referenced by this function at all.
 */

#include "oa_engine_init.h"

void **CSTGVectorEGXOnly::sMutex;
void **CSTGVectorEGXY::sMutex;

/* Host/target pointer-width fix (this project's established pattern):
 * the real target's list-node fields and sMutex are plain 32-bit
 * pointers, but a native host pointer is 8 bytes on a 64-bit build. */
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

typedef void (*VtableSlot0Fn)(void *);

static void DispatchSlot0(void *obj)
{
	void **vtable = *(void ***)obj;
	((VtableSlot0Fn)vtable[0])(obj);
}

/* Push `node` (the object's own +0x3c/+0x40/+0x48 sub-fields) onto the
 * front of the intrusive list anchored at `head`/`tail`/`count`
 * (manager-relative fields), matching CSTGWaveSeqManager::Initialize()'s
 * own confirmed push-front algorithm (sec 10.62) exactly. */
static void ListPushFront(unsigned char *manager, unsigned int headOff,
			   unsigned int tailOff, unsigned int countOff,
			   unsigned char *obj)
{
	unsigned char *node = obj + 0x3c; /* next=+0, prev=+4 relative to node */
	unsigned int objAddr = ToU32(obj);
	unsigned int nodeAddr = ToU32(node);
	unsigned int *head = (unsigned int *)(manager + headOff);
	unsigned int *tail = (unsigned int *)(manager + tailOff);
	unsigned int *count = (unsigned int *)(manager + countOff);

	unsigned int oldHead = *head;
	if (oldHead == 0) {
		*tail = nodeAddr;
	} else {
		unsigned char *oldHeadObj = (unsigned char *)(unsigned long)oldHead;
		unsigned int oldHeadPrev = *(unsigned int *)(oldHeadObj + 4);
		*(unsigned int *)(node + 4) = oldHeadPrev;
		if (oldHeadPrev != 0)
			*(unsigned int *)(unsigned long)oldHeadPrev = nodeAddr;
		*(unsigned int *)(oldHeadObj + 4) = nodeAddr;
		*(unsigned int *)(node + 0) = oldHead;
	}
	*head = nodeAddr;
	*(unsigned int *)(obj + 0x48) = ToU32(head); /* owner = &manager->headOff */
	*count += 1;
	(void)objAddr;
}

void CSTGVectorManager::Initialize()
{
	unsigned char *p = reinterpret_cast<unsigned char *>(this);

	/* Phase 1: 400x CSTGVectorEGXOnly. */
	for (unsigned int i = 0; i < 400; i++) {
		unsigned char *obj = p + i * 0x88;
		DispatchSlot0(obj);
		*(unsigned short *)(obj + 0x4) = (unsigned short)i;
		ListPushFront(p, 0x1c9ac, 0x1c9b0, 0x1c9b4, obj);
	}

	/* Phase 2: 400x CSTGVectorEGXY. */
	for (unsigned int i = 0; i < 400; i++) {
		unsigned char *obj = p + 0xd480 + i * 0x7c;
		DispatchSlot0(obj);
		*(unsigned short *)(obj + 0x4) = (unsigned short)i;
		ListPushFront(p, 0x1c9b8, 0x1c9bc, 0x1c9c0, obj);
	}

	/* Phase 3: confirmed real marker write, not independently understood. */
	*(unsigned short *)(p + 0x1aff0) = 0;
	p[0x1aff2] = 0;
	p[0x1aff3] = 1;

	/* Phase 4: 17x CSTGVectorEGCC ("batch1" only) -- no list insertion. */
	for (unsigned int i = 0; i < 17; i++) {
		unsigned char *obj = p + 0x19640 + i * 0x70;
		DispatchSlot0(obj);
		*(unsigned short *)(obj + 0x4) = (unsigned short)i;
	}

	/* Phase 5: 16 paired iterations over "batch1" EGXOnly/EGXY -- shared
	 * index (400+i), no list insertion for either, plus 4 confirmed
	 * real zeroed WORD tables. */
	for (unsigned int i = 0; i < 16; i++) {
		unsigned short idx = (unsigned short)(400 + i);

		unsigned char *egXOnly = p + 0x19db0 + i * 0x88;
		DispatchSlot0(egXOnly);
		*(unsigned short *)(egXOnly + 0x4) = idx;

		unsigned char *egXY = p + 0x1a630 + i * 0x7c;
		DispatchSlot0(egXY);
		*(unsigned short *)(egXY + 0x4) = idx;

		*(unsigned short *)(p + 0x1af70 + i * 2) = 0;
		*(unsigned short *)(p + 0x1af90 + i * 2) = 0;
		*(unsigned short *)(p + 0x1afb0 + i * 2) = 0;
		*(unsigned short *)(p + 0x1afd0 + i * 2) = 0;
	}

	/* Tail: real "address of the singleton pointer" idiom. */
	CSTGVectorEGXOnly::sMutex = (void **)(p + 0x1c9e0);
	CSTGVectorEGXY::sMutex = (void **)(p + 0x1c9e4);
}
