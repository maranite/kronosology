// SPDX-License-Identifier: GPL-2.0
/*
 * smoother_cancel.cpp  -  CSTGSmoother::CancelAllSmoothers() (sec 10.154).
 *
 * Deliberately its own translation unit, matching the WriteSTGMidiOutQueue/
 * CSTGAudioInputMixerBase precedent: three sibling test files
 * (test_engine.cpp/test_global_ctor.cpp/test_global.cpp) each carry their
 * own load-bearing call-counter mock for this exact symbol -- test_global.cpp's
 * own `g_cancelAllSmoothersCalls` is exercised by real UpdateXXX call-order
 * assertions there. None of those three files link this new TU.
 *
 * NOTE: CSTGSmoother::CSTGSmoother() itself remains a deliberately deferred
 * stub (bar2_stubs.cpp) -- see smoother_init.cpp's own header note. This
 * means the `+0xf000/+0xf004/+0xf008/+0xf00c/+0xf010/+0xf014/+0xf018/+0xf01c`
 * fields this function reads/writes are not actually established by any
 * real ctor yet either -- same documented gap Initialize() already flagged,
 * not silently papered over here.
 *
 * CSTGSmoother::CancelAllSmoothers() (.text+0x2a3d0 in OA_real.ko, 375
 * bytes) confirmed: walks the real doubly-linked "active smoothers" list
 * anchored at `+0xf010` (head)/`+0xf014` (tail)/`+0xf018` (count) -- the
 * SAME list CancelAllCCSmoothers() (sec 10.130) walks read-only via
 * FinalizeSmoother() for CC-typed entries only -- unconditionally removing
 * and finalizing EVERY entry (hence "All", vs CC-only). For each node
 * (always the current head, `+0xf010`):
 *   1. Unlinks it from the active list: `activeHead = node->next` always;
 *      if `node->prev != 0`, `node->prev->next = node->next` (mirrors the
 *      standard unlink, defensively checked even though a genuine head
 *      node's own `prev` should already be 0); if `node->next != 0`,
 *      `node->next->prev = node->prev`; if `node` was also the tail
 *      (`node == activeTail`), clears `activeTail = 0` first. Zeroes the
 *      node's own `next`/`prev`/`owner` fields (`+0x0`/`+0x4`/`+0xc`) and
 *      decrements `+0xf018`.
 *   2. Pushes the SAME node onto the FRONT of the free list (`+0xf004`
 *      head/`+0xf008` tail/`+0xf00c` count) -- byte-for-byte the same
 *      push-front template already confirmed real in
 *      `CSTGSmoother::Initialize()` (sec 10.86), just applied to a single
 *      node instead of a build-time loop: if the free list was empty,
 *      `freeTail = node`; otherwise splices `node` in before the current
 *      free-list head, fixing up that head's own `prev` pointer. Either
 *      way, `freeHead = node`, `node->owner = &this->freeHead` (a real,
 *      confirmed self-referential "owner" pointer, matching Initialize()'s
 *      own `*(link+0xc) = ToU32(base+0xf004)`), and `+0xf00c` incremented.
 *   3. Zeroes TWO dwords in the SEPARATE `+0xf000` buffer (a real,
 *      independently-allocated `CSTGBankMemory::AllocAligned(0x3c00, 0x10)`
 *      pool per Initialize() -- NOT the 320 embedded mapping sub-objects,
 *      which live directly on `this` starting at `+0x0`): `node->+0x8` is
 *      a pointer to the node's own owning "mapping" sub-object, whose
 *      `+0x0` field is a confirmed real 16-bit index (`0..0x13f`). This
 *      function zeroes the buffer slots at LOGICAL indices `2*mappingIdx`
 *      and `2*mappingIdx+1` (a "current value"/"target value" dword pair
 *      per smoother, matching the CSTGFrontPanelSmoothers precedent, sec
 *      10.153, for a 4-way-interleaved SoA layout -- confirmed here to use
 *      96-byte rows of 4 slots x 4 bytes, not that class's own 128-byte
 *      rows: `slot = (logicalIdx>>2)*0x60 + (logicalIdx&3)*4`). 320
 *      mappings x 2 slots / 4 slots-per-row x 96 bytes/row = 0x3c00 bytes,
 *      exactly matching Initialize()'s own confirmed buffer size -- an
 *      independent cross-check that this addressing scheme is right.
 * Repeats for the new `activeHead` until the list is empty, then
 * unconditionally zeroes `+0xf01c` (a confirmed real final field, own
 * further meaning not determined -- Initialize() also zeroes it, sec
 * 10.86) and returns.
 */

#include "oa_engine_init.h"

static unsigned char *SmootherFromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}
static unsigned int SmootherToU32(unsigned char *p)
{
	return (unsigned int)(unsigned long)p;
}

void CSTGSmoother::CancelAllSmoothers()
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *buf = SmootherFromU32(*(unsigned int *)(base + 0xf000));

	unsigned int node = *(unsigned int *)(base + 0xf010);

	while (node != 0) {
		unsigned char *n = SmootherFromU32(node);

		/* --- unlink `n` from the active doubly-linked list --- */
		if (node == *(unsigned int *)(base + 0xf014))
			*(unsigned int *)(base + 0xf014) = 0; /* was also the tail */

		unsigned int nNext = *(unsigned int *)(n + 0x0);
		unsigned int nPrev = *(unsigned int *)(n + 0x4);
		*(unsigned int *)(base + 0xf010) = nNext;
		if (nPrev != 0)
			*(unsigned int *)(SmootherFromU32(nPrev)) = nNext;
		if (nNext != 0)
			*(unsigned int *)(SmootherFromU32(nNext) + 0x4) = nPrev;
		*(unsigned int *)(n + 0x0) = 0;
		*(unsigned int *)(n + 0x4) = 0;
		*(unsigned int *)(n + 0xc) = 0;
		*(unsigned int *)(base + 0xf018) -= 1;

		/* --- push `n` onto the front of the free list --- */
		unsigned int oldFreeHead = *(unsigned int *)(base + 0xf004);
		if (oldFreeHead == 0) {
			*(unsigned int *)(base + 0xf008) = node; /* freeTail = n */
		} else {
			unsigned char *fh = SmootherFromU32(oldFreeHead);
			unsigned int fhPrev = *(unsigned int *)(fh + 0x4);
			*(unsigned int *)(n + 0x4) = fhPrev;
			if (fhPrev != 0)
				*(unsigned int *)(SmootherFromU32(fhPrev)) = node;
			*(unsigned int *)(fh + 0x4) = node;
			*(unsigned int *)(n + 0x0) = oldFreeHead;
		}
		*(unsigned int *)(base + 0xf004) = node;
		*(unsigned int *)(n + 0xc) = SmootherToU32(base + 0xf004);
		*(unsigned int *)(base + 0xf00c) += 1;

		/* --- zero the two interleaved buffer slots for this mapping --- */
		unsigned char *mapping = SmootherFromU32(*(unsigned int *)(n + 0x8));
		unsigned int mappingIdx = *(unsigned short *)mapping;
		for (unsigned int k = 0; k < 2; k++) {
			unsigned int logicalIdx = mappingIdx * 2 + k;
			unsigned int slot = (logicalIdx >> 2) * 0x60 + (logicalIdx & 3) * 4;
			*(unsigned int *)(buf + slot) = 0;
		}

		node = *(unsigned int *)(base + 0xf010);
	}

	*(unsigned int *)(base + 0xf01c) = 0;
}
