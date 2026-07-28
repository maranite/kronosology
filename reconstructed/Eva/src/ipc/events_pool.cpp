/*
 * events_pool.cpp  -  CEventsPool. See events_pool.h for the full derivation.
 */

#include "events_pool.h"
#include "event.h"

extern void HAL_DisableInterrupts();
extern void HAL_EnableInterrupts();

/* Real single global instance -- .bss+0x0930a2b8. */
CEventsPool CLinkedEvent::sm_oEventsPool;

namespace {

/* One raw 12-byte node, laid out identically to CLinkedEvent (mTag, mBuf, mNext) --
 * kept as raw bytes here (rather than an embedded CLinkedEvent[]) purely so this file
 * doesn't need CLinkedEvent's constructor semantics for the bulk arena init; the real
 * ctor writes these same three raw dwords directly (0xf, 0xeeeeeeee poison, 0), never
 * going through CLinkedEvent::CLinkedEvent(). */
struct SRawNode {
	int   mTag;
	void *mBuf;
	void *mNext;
};

const unsigned kNodeCount = 0x800; /* 2048 */

} // namespace

CEventsPool::CEventsPool()
	: mPoolBase(0), mFreeList(0), mAllocCount(0), mHighWaterMark(0)
{
	/* Real: `new unsigned char[0x6004]` -- 4-byte node-count header + 2048*12
	 * payload bytes. The leading header word (0x800 = 2048) is written but
	 * never read back anywhere this pass's call graph reaches. */
	unsigned char *arena = new unsigned char[4 + kNodeCount * sizeof(SRawNode)];
	*(unsigned *)arena = kNodeCount;

	SRawNode *nodes = (SRawNode *)(arena + 4);
	mPoolBase = (unsigned char *)nodes;
	if (nodes == 0) {
		mFreeList = mPoolBase;
		return;
	}

	for (unsigned i = 0; i < kNodeCount; ++i) {
		nodes[i].mTag  = 0xf;
		nodes[i].mBuf  = (void *)0xeeeeeeee;
		nodes[i].mNext = 0;
	}
	for (unsigned i = 0; i + 1 < kNodeCount; ++i)
		nodes[i].mNext = &nodes[i + 1];
	nodes[kNodeCount - 1].mNext = 0;

	/* Real: mFreeList = mPoolBase (node[0] is the free-list head). */
	mFreeList = mPoolBase;
}

CEventsPool::~CEventsPool()
{
	/* Real: `if (mAllocCount != 0) ApiAssert(...)` (soft, "torn down with
	 * nodes still checked out" diagnostic, omitted per this project's
	 * established convention) -- either way, walks every node (backwards,
	 * last-to-first; order not observable) whose mTag is still negative
	 * (bit31 set, the SAME "owns a CEvBuffersPool buffer" convention
	 * CEvent::~CEvent() itself uses) and releases that buffer, then
	 * delete[]s the whole arena (recovered as mPoolBase-4, the node-count
	 * header slot). Static-lifetime singleton in practice -- never exercised
	 * by this pass's own traced call graph, modeled for shape fidelity only
	 * (same convention as CEvBuffersPool's own dtor). */
	if (mPoolBase != 0) {
		SRawNode *nodes = (SRawNode *)mPoolBase;
		for (unsigned i = 0; i < kNodeCount; ++i) {
			if (nodes[i].mTag >= 0)
				continue;
			CEvent::sm_oEvBuffersPool.Free(nodes[i].mBuf);
		}
	}
	delete[] (mPoolBase - 4);
}

CLinkedEvent *CEventsPool::GetNewEvent()
{
	HAL_DisableInterrupts();

	SRawNode *node = (SRawNode *)mFreeList;
	if (node != 0) {
		++mAllocCount;
		if (mAllocCount > mHighWaterMark)
			mHighWaterMark = mAllocCount;
		mFreeList = node->mNext;
		HAL_EnableInterrupts();
		node->mNext = 0;
		return (CLinkedEvent *)node;
	}

	HAL_EnableInterrupts();
	/* Real: pool exhausted. Two soft Api diagnostics (vtbl+0x94 ApiAssert
	 * "EvPool.cpp" line 0x58, vtbl+0x90 warn "CEventPool::GetNewEvent() -
	 * Pool of events is exhausted"), omitted per this project's established
	 * Api+0x90/+0x94 convention -- followed by a REAL ground-truth infinite
	 * loop (raw `jmp $`, no further code), unlike every other Api+0x90/+0x94
	 * site in this project. Preserved as-is; not reachable in practice. */
	for (;;) {}
}
