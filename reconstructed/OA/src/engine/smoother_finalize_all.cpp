// SPDX-License-Identifier: GPL-2.0
/*
 * smoother_finalize_all.cpp  -  CSTGSmoother::FinalizeAllSmoothers()
 * (batch 61, ground truth `.text+0x2d6c0`, 407 bytes).
 *
 * Deliberately its own translation unit: test_global_ctor.cpp/
 * test_engine.cpp/test_global.cpp each carry their own load-bearing
 * local mock for this exact symbol (test_global.cpp's own
 * `g_finalizeAllSmoothersCalls` is exercised by real call-order
 * assertions there) -- matching the CancelAllSmoothers/FinalizeSmoother
 * precedent. None of those three files link this new TU.
 *
 * Confirmed via direct disassembly to be a HYBRID of two already-real
 * sibling functions' own logic, both cross-checked byte-for-byte:
 *   - The active-list unlink + free-list push (per iteration) is
 *     IDENTICAL to `CSTGSmoother::CancelAllSmoothers()`'s own loop body
 *     (smoother_cancel.cpp) -- same head/tail special cases, same
 *     `+0xf010`/`+0xf014`/`+0xf018` active-list fields, same
 *     `+0xf004`/`+0xf008`/`+0xf00c` free-list push-front template.
 *   - The buffer-zero step (two interleaved dwords in the `+0xf000`
 *     pool, `slot = (logicalIdx>>2)*0x60 + (logicalIdx&3)*4` for
 *     `logicalIdx = mappingIdx*2 + {0,1}`) is IDENTICAL to
 *     CancelAllSmoothers()'s own step 3 -- same formula, same
 *     independently-cross-checked 0x60-byte-row/320-mapping/0x3c00-byte
 *     pool layout.
 *   - Confirmed NEW relative to both siblings: between the free-list
 *     push and the buffer-zero step, this function ALWAYS (unlike
 *     `FinalizeSmoother()`, where the same call is conditional on its
 *     own `flag` argument) calls `CSTGSmootherMapping::
 *     DispatchSmoothedValue(float, float, bool)` on the node's own
 *     `+0x8` mapping pointer -- confirmed via disassembly to be the
 *     EXACT SAME call shape already documented in smoother_finalize.cpp
 *     (`this=mapping`, `a=mapping[0x4]`, `b=mapping[0x8]`, both floats on
 *     the stack, bool literal `true` in EDX) -- i.e. this function is
 *     effectively "call `FinalizeSmoother(node, true)` for every node on
 *     the active list, PLUS also zero its two buffer slots (which
 *     `FinalizeSmoother()` itself never does)". Reuses the already-
 *     declared `CSTGSmootherMapping_DispatchSmoothedValue` extern
 *     (smoother_finalize.cpp), still a confirmed real, deliberately
 *     deferred genuine audio-DSP callee -- its own no-op body remains in
 *     bar2_stubs_c.cpp, unchanged.
 *
 * Loop terminates when the active list (`+0xf010`) is empty, then
 * unconditionally zeroes `+0xf01c` (the same "iterator cursor" field
 * Initialize()/CancelAllSmoothers()/FinalizeSmoother() all touch) and
 * returns -- matching CancelAllSmoothers()'s own final step exactly.
 */

#include "oa_engine_init.h"

extern "C" void CSTGSmootherMapping_DispatchSmoothedValue(void *mapping, float a, float b, bool c);

static unsigned char *FinAllFromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}
static unsigned int FinAllToU32(unsigned char *p)
{
	return (unsigned int)(unsigned long)p;
}

void CSTGSmoother::FinalizeAllSmoothers()
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *buf = FinAllFromU32(*(unsigned int *)(base + 0xf000));

	unsigned int node = *(unsigned int *)(base + 0xf010);

	while (node != 0) {
		unsigned char *n = FinAllFromU32(node);

		/* --- unlink `n` from the active doubly-linked list (identical
		 * to CancelAllSmoothers()) --- */
		if (node == *(unsigned int *)(base + 0xf014))
			*(unsigned int *)(base + 0xf014) = 0; /* was also the tail */

		unsigned int nNext = *(unsigned int *)(n + 0x0);
		unsigned int nPrev = *(unsigned int *)(n + 0x4);
		*(unsigned int *)(base + 0xf010) = nNext;
		if (nPrev != 0)
			*(unsigned int *)(FinAllFromU32(nPrev)) = nNext;
		if (nNext != 0)
			*(unsigned int *)(FinAllFromU32(nNext) + 0x4) = nPrev;
		*(unsigned int *)(n + 0x0) = 0;
		*(unsigned int *)(n + 0x4) = 0;
		*(unsigned int *)(n + 0xc) = 0;
		*(unsigned int *)(base + 0xf018) -= 1;

		/* --- push `n` onto the front of the free list (identical to
		 * CancelAllSmoothers()) --- */
		unsigned int oldFreeHead = *(unsigned int *)(base + 0xf004);
		if (oldFreeHead == 0) {
			*(unsigned int *)(base + 0xf008) = node; /* freeTail = n */
		} else {
			unsigned char *fh = FinAllFromU32(oldFreeHead);
			unsigned int fhPrev = *(unsigned int *)(fh + 0x4);
			*(unsigned int *)(n + 0x4) = fhPrev;
			if (fhPrev != 0)
				*(unsigned int *)(FinAllFromU32(fhPrev)) = node;
			*(unsigned int *)(fh + 0x4) = node;
			*(unsigned int *)(n + 0x0) = oldFreeHead;
		}
		*(unsigned int *)(base + 0xf004) = node;
		*(unsigned int *)(n + 0xc) = FinAllToU32(base + 0xf004);
		*(unsigned int *)(base + 0xf00c) += 1;

		/* --- NEW relative to CancelAllSmoothers(): always dispatch the
		 * final value (matches FinalizeSmoother(node, true)'s own step 3) --- */
		unsigned char *mapping = FinAllFromU32(*(unsigned int *)(n + 0x8));
		float a = *(float *)(mapping + 0x4);
		float b = *(float *)(mapping + 0x8);
		CSTGSmootherMapping_DispatchSmoothedValue(mapping, a, b, true);

		/* --- zero the two interleaved buffer slots for this mapping
		 * (identical to CancelAllSmoothers()) --- */
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
