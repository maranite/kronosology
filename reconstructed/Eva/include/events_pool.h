/*
 * events_pool.h  -  CEventsPool, the static freelist that backs every heap-allocated
 * CLinkedEvent node this project has traced so far. CParamTracer family pass,
 * 2026-07-28. Transcribed directly from objdump -dr -M intel disassembly of
 * CEventsPool::{CEventsPool,~CEventsPool,GetNewEvent}@.text+0x0807f8a0/0x0807fa70/
 * 0x0807fb10 (Decomp/EVA_Decomp/Eva).
 *
 * Real single instance: `CLinkedEvent::sm_oEventsPool` at .bss+0x0930a2b8 (own
 * `_GLOBAL__I_sm_oEventsPool` static constructor at .text+0x0807fba0) -- see event.h's
 * own corrected header comment for why an earlier pass mis-typed this global. Every
 * real caller traced so far (CParamTracer::AppendSingleParam/AppendAllParams/
 * AppendParams/AppendParamsDontCareAddr, reconstructed this pass; CControllerTracer/
 * CCtrlAndParamTracer/CNoteTracer/CEventsQueue/CRTRouter, seen only via xref, not yet
 * reconstructed) loads this exact literal address before calling GetNewEvent().
 *
 * SHAPE: NOT a CEvBuffersPool-style two-tier chunk allocator. This is a single fixed
 * arena of 2048 embedded 12-byte nodes (the exact size/layout of CEvent{mTag,mBuf} +
 * CLinkedEvent's own mNext -- see event.h), threaded into a singly-linked free list
 * through each node's own trailing 4 bytes (the same "unused-payload doubles as the
 * free-list link" trick CEvBuffersPool uses, just with the link at a fixed +8 offset
 * matching CLinkedEvent::mNext instead of a per-tier chunk-size-dependent offset).
 *
 * CEventsPool():  real ctor allocates one `new unsigned char[0x6004]` arena (0x6004 =
 * 4-byte node-count header + 2048*12 payload bytes), writes the node count (0x800 =
 * 2048) into that leading header word, then two passes over the 2048 nodes:
 *   pass 1 (init): each node gets mTag=0xf (the same "no buffer owned" placeholder
 *     CEvent's own ctor comment already documents), mBuf=0xeeeeeeee (poison, never
 *     read while free), mNext=0.
 *   pass 2 (link): walks nodes 1..2047, writing `node[i-1].mNext = &node[i]` (i.e.
 *     builds the chain via each node's OWN trailing field, not a separate index
 *     array), then explicitly zeroes the LAST node's mNext as the list terminator.
 * `this` layout confirmed directly from the ctor's own field stores: mPoolBase (+0x00,
 * arena base, == &node[0]) and mFreeList (+0x04, initially == mPoolBase, i.e. node[0]
 * is the head) are the only two fields GetNewEvent()/~CEventsPool() ever touch.
 *
 * GetNewEvent(): pop-front off mFreeList inside a HAL_DisableInterrupts()/
 * EnableInterrupts() critical section (mAllocCount/mHighWaterMark bookkeeping at
 * +0x08/+0x0c updated under the same lock, tracked here as plain ints -- nothing in
 * this pass's own traced call graph ever reads them back). The popped node's mNext is
 * cleared to 0 before returning it (matches every real caller's own expectation of a
 * "fresh" node). Real exhaustion path (free list empty): two Api soft diagnostics
 * (vtbl+0x94 ApiAssert, vtbl+0x90 warn: "CEventPool::GetNewEvent() - Pool of events is
 * exhausted" -- note the real log string says "CEventPool", singular "Event", a
 * developer typo; the real mangled class name is definitely `CEventsPool`, confirmed
 * via `_ZN11CEventsPool11GetNewEventEv`), omitted per this project's established
 * Api+0x90/+0x94 "soft, non-enforcing diagnostic" convention (see ev_buffers_pool.h) --
 * BUT unlike every other Api+0x90/+0x94 site in this project, this one does NOT fall
 * through afterward: the real disassembly ends in a bare `jmp $` with no further code,
 * a genuine ground-truth infinite loop/hang on exhaustion, not a soft assert. Preserved
 * as-is (`for (;;) {}`) since it's real, observable control flow, not diagnostic-only --
 * not reachable in practice with a 2048-slot pool.
 *
 * ~CEventsPool(): real teardown walks every node whose mTag is still the "owned"
 * pattern (mTag >= 0, i.e. bit 31 clear -- the same ownership-bit convention CEvent's
 * own dtor uses) and returns any still-attached CEvBuffersPool-owned mBuf via
 * `CEvBuffersPool::Free(&CEvent::sm_oEvBuffersPool, node.mBuf)` before finally
 * `delete[]`-ing the whole arena. Static-lifetime singleton in practice (this pass's
 * own traced call graph never destroys `sm_oEventsPool`); modeled for shape fidelity,
 * same convention as CEvBuffersPool's own dtor.
 */

#ifndef EVENTS_POOL_H
#define EVENTS_POOL_H

class CLinkedEvent;

class CEventsPool {
public:
	/* .text+0x0807f8a0, ~740 bytes. Builds the 2048-node arena + free list. */
	CEventsPool();

	/* .text+0x0807fa70. Returns any still-checked-out nodes' CEvBuffersPool
	 * buffers, then delete[]s the arena. See file header. */
	~CEventsPool();

	/* .text+0x0807fb10. Pop one node off the free list (HAL-interrupt-locked).
	 * See file header for the real, ground-truth exhaustion-path infinite
	 * loop this preserves as-is. */
	CLinkedEvent *GetNewEvent();

private:
	unsigned char *mPoolBase;    /* +0x00, 0x6004 bytes: 4-byte count header + 2048 x 12B nodes */
	void          *mFreeList;    /* +0x04, singly-linked via each node's own trailing 4 bytes */
	int            mAllocCount;  /* +0x08, checked-out count, never read back in this pass's call graph */
	int            mHighWaterMark; /* +0x0c, peak mAllocCount, same */

	friend struct EventsPoolTestHooks;
};

#endif /* EVENTS_POOL_H */
