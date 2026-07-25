// SPDX-License-Identifier: GPL-2.0
/*
 * smoother_finalize.cpp  -  CSTGSmoother::FinalizeSmoother(TListLink<
 * CSTGSmootherMapping>*, bool) (batch 57).
 *
 * Deliberately its own translation unit, matching the CancelAllSmoothers/
 * WriteSTGMidiOutQueue precedent: FOUR sibling test files (test_engine.cpp/
 * test_global_ctor.cpp/test_global.cpp/test_slot_voice_data_free.cpp) each
 * carry their own load-bearing call-counter or fully-scripted local mock
 * for this exact symbol -- test_global.cpp's own `g_finalizeSmootherCalls`
 * and test_slot_voice_data_free.cpp's own scripted mock are both exercised
 * by real assertions there. None of those four files link this new TU.
 *
 * CSTGSmoother::FinalizeSmoother() (.text+0x2b590 in OA_real.ko, 331
 * bytes) confirmed via direct disassembly. This is the SHARED per-node
 * removal helper both already-real callers use (CSTGGlobal::
 * CancelAllCCSmoothers, global.cpp; CSTGSlotVoiceData::
 * CancelAllSlotVoiceDataCCSmoothers, slot_voice_data_free.cpp) -- both
 * existing real callers pass `flag=false`.
 *
 *   0. Cursor fixup: if `this->fieldAt(0xf01c)` (a confirmed real
 *      "iterator cursor" field, also touched by CancelAllSmoothers()/
 *      Initialize()) currently equals `node`, advances it to
 *      `node->next` FIRST -- a real "don't leave a live cursor dangling
 *      on the node about to be removed" fixup, confirmed via the real
 *      disassembly jumping back into step 1 on the SAME node afterward
 *      (not skipping/altering which node gets processed).
 *   1. GENERIC doubly-linked-list unlink via the node's own `+0xc`
 *      "owner" field (confirmed real, matches smoother_cancel.cpp's own
 *      `*(link+0xc) = ToU32(base+0xf004)`-style self-referential owner
 *      pointer): if `owner==0`, this node isn't currently on any list,
 *      skip straight to step 2. Otherwise `owner` points at a
 *      head/tail/count triple (3 consecutive dwords, e.g.
 *      `this+0xf004/+0xf008/+0xf00c` for the free list, or
 *      `this+0xf010/+0xf014/+0xf018` for the active list -- UNLIKE
 *      CancelAllSmoothers() which only ever unlinks from the fixed
 *      active-list anchor, this function's own real disassembly derives
 *      the anchor generically from the node's own `owner` field, so it
 *      correctly unlinks from WHICHEVER list currently owns the node):
 *      standard doubly-linked-list unlink (head/tail special cases via
 *      `*owner`/`*(owner+4)`, general case fixes up `prev->next`/
 *      `next->prev`), zeroes the node's own `next`/`prev`/`owner`
 *      (`+0x0`/`+0x4`/`+0xc`), decrements `*(owner+8)` (the list's own
 *      count).
 *   2. Push `node` onto the FRONT of `this`'s own free list
 *      (`+0xf004`/`+0xf008`/`+0xf00c`, hardcoded here -- NOT
 *      owner-relative) -- byte-for-byte the same push-front template
 *      already confirmed real in `Initialize()`/`CancelAllSmoothers()`.
 *   3. If `flag` is true: calls `CSTGSmootherMapping::
 *      DispatchSmoothedValue(float, float, bool)` on the node's own
 *      `+0x8` mapping pointer, with that mapping's own `+0x4`/`+0x8`
 *      floats and a literal `true` third argument (confirmed regparm(3):
 *      eax=this(mapping), edx=bool arg3=1 -- both float args go on the
 *      stack, never in a register, matching this project's own
 *      established x86 float-arg convention). NEITHER currently-real
 *      caller (CancelAllCCSmoothers/CancelAllSlotVoiceDataCCSmoothers)
 *      ever passes `flag=true`, so this branch is confirmed UNEXERCISED
 *      by any code this project has reconstructed so far --
 *      `DispatchSmoothedValue` itself is declared here as a confirmed
 *      real, deliberately deferred extern (own body not reconstructed,
 *      genuine audio-DSP smoother-output dispatch) rather than given a
 *      body in this pass.
 */

#include "oa_engine_init.h"

extern "C" void CSTGSmootherMapping_DispatchSmoothedValue(void *mapping, float a, float b, bool c);

static unsigned char *SmootherFinFromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}
static unsigned int SmootherFinToU32(unsigned char *p)
{
	return (unsigned int)(unsigned long)p;
}

void CSTGSmoother::FinalizeSmoother(void *nodeArg, bool flag)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *n = (unsigned char *)nodeArg;
	unsigned int node = SmootherFinToU32(n);

	/* --- step 0: cursor fixup --- */
	if (*(unsigned int *)(base + 0xf01c) == node)
		*(unsigned int *)(base + 0xf01c) = *(unsigned int *)(n + 0x0);

	/* --- step 1: generic unlink via node's own owner pointer --- */
	unsigned int ownerVal = *(unsigned int *)(n + 0xc);
	if (ownerVal != 0) {
		unsigned char *owner = SmootherFinFromU32(ownerVal);

		if (*(unsigned int *)owner == node) {
			/* node is the list head */
			unsigned int next = *(unsigned int *)(n + 0x0);
			bool alsoTail = (*(unsigned int *)(owner + 0x4) == node);
			*(unsigned int *)owner = next; /* head = next */
			if (alsoTail)
				*(unsigned int *)(owner + 0x4) = *(unsigned int *)(n + 0x4); /* tail = prev */
		} else if (*(unsigned int *)(owner + 0x4) == node) {
			/* node is the list tail (and not the head) */
			*(unsigned int *)(owner + 0x4) = *(unsigned int *)(n + 0x4); /* tail = prev */
		}
		/* general fixups (also applied after either special case above,
		 * matching the real disassembly's shared fallthrough): */
		unsigned int prev = *(unsigned int *)(n + 0x4);
		if (prev != 0)
			*(unsigned int *)(SmootherFinFromU32(prev) + 0x0) = *(unsigned int *)(n + 0x0);
		unsigned int next = *(unsigned int *)(n + 0x0);
		if (next != 0)
			*(unsigned int *)(SmootherFinFromU32(next) + 0x4) = prev;

		*(unsigned int *)(n + 0x0) = 0;
		*(unsigned int *)(n + 0x4) = 0;
		*(unsigned int *)(n + 0xc) = 0;
		*(unsigned int *)(owner + 0x8) -= 1;
	}

	/* --- step 2: push node onto the front of this->free list --- */
	unsigned int oldFreeHead = *(unsigned int *)(base + 0xf004);
	if (oldFreeHead == 0) {
		*(unsigned int *)(base + 0xf008) = node; /* freeTail = node */
	} else {
		unsigned char *fh = SmootherFinFromU32(oldFreeHead);
		unsigned int fhPrev = *(unsigned int *)(fh + 0x4);
		*(unsigned int *)(n + 0x4) = fhPrev;
		if (fhPrev != 0)
			*(unsigned int *)(SmootherFinFromU32(fhPrev) + 0x0) = node;
		*(unsigned int *)(fh + 0x4) = node;
		*(unsigned int *)(n + 0x0) = oldFreeHead;
	}
	*(unsigned int *)(base + 0xf004) = node;
	*(unsigned int *)(n + 0xc) = SmootherFinToU32(base + 0xf004);
	*(unsigned int *)(base + 0xf00c) += 1;

	/* --- step 3: confirmed real, deliberately deferred DSP callee --- */
	if (flag) {
		unsigned char *mapping = SmootherFinFromU32(*(unsigned int *)(n + 0x8));
		float a = *(float *)(mapping + 0x4);
		float b = *(float *)(mapping + 0x8);
		CSTGSmootherMapping_DispatchSmoothedValue(mapping, a, b, true);
	}
}

/* CSTGSmootherMapping::DispatchSmoothedValue(float, float, bool) --
 * confirmed real, deliberately deferred (own body not reconstructed):
 * genuine audio-DSP "push this smoother's now-final value to its target
 * parameter" dispatch, and confirmed unreachable from any currently-real
 * caller (see this file's own header comment). Deliberately only
 * DECLARED (not defined) here -- its own safe no-op body lives in
 * bar2_stubs_c.cpp instead, alongside this project's other cross-cutting
 * deferred no-ops, so this file's own dedicated KAT
 * (test_smoother_finalize.cpp) can supply its own call-counting mock
 * without a multiple-definition link conflict. */
