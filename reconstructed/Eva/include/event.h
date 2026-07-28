/*
 * event.h  -  CEvent/CLinkedEvent, the tiny tagged-buffer handle every CEvBuffersPool
 * chunk is wrapped in -- Stage 6 breadth sweep, 2026-07-25, reconstructed alongside
 * ev_buffers_pool.h to unblock CClientCommServer (client_comm_server.h).
 *
 * CEvent (2 fields, 8 bytes) -- confirmed field-for-field from `CEvent::~CEvent()`
 * (.text+0x08182520, 43 bytes, the ONLY CEvent method with its own out-of-line body;
 * everything else about this class this pass needed is inline/POD-shaped) and from
 * CClientCommServer's own ctor/dtor, which duplicate this exact same tag-sign-bit
 * check inline against its OWN embedded (not heap `CEvent`-allocated) tag/buffer pair
 * rather than calling through a real CEvent object -- see client_comm_server.h's own
 * field-layout comment (`mEvTag`/`mEvBuf` at +0x20/+0x24). The real `CEvent::~CEvent()`
 * disassembly is:
 *
 *   edx = this->mTag
 *   if (edx < 0) CEvBuffersPool::Free(&CEvent::sm_oEvBuffersPool, this->mBuf)
 *   (else: nothing -- no buffer was ever allocated for this event)
 *
 * i.e. bit 31 of `mTag` is an ownership flag: an event that never allocated a pool
 * buffer (or already released it) carries a small non-negative tag (the real code's
 * own "0xf" ctor-time placeholder, or plain 0); a real allocated-and-locked event's tag
 * is always some `0x8000000a`-shaped value (class-code 0x0a in the low byte, ownership
 * bit 31 set, payload-length byte at bits 16-23 -- confirmed by
 * `CClientCommServer::ComputeCRCByte()`'s own `(unsigned)mEvTag >> 16` length
 * extraction). This reconstruction keeps that same raw int32 tag rather than
 * decomposing it into named bitfields, matching every real caller's own convention of
 * reading/writing the whole word directly (`and eax,0xff00; or eax,0x8000000a`, never a
 * bitfield store).
 *
 * CLinkedEvent : public CEvent, adds one more pointer (`mNext`, the free-list-style
 * forward link `CEventsQueue`/`CRTRouter` use to chain events) -- confirmed by
 * `CClientCommServer::SendToSysExLink()`/`TXData()`/`RetryTXPacket()`/
 * `OnProcessRetry()`, all of which reinterpret_cast their own embedded
 * `{mEvTag, mEvBuf}` pair (12 bytes total incl. the trailing `mUnknown28`/`mNext` slot)
 * as a `CLinkedEvent*` when calling `CSexServiceTask::TransmitSysEx(CLinkedEvent*,
 * unsigned char)` -- i.e. `CLinkedEvent` is exactly `CEvent` plus one trailing pointer,
 * nothing more, laid out contiguously.
 *
 * CORRECTION (2026-07-28, CParamTracer family pass, see param_tracer.h): the prior
 * version of this comment mis-typed `CLinkedEvent::sm_oEventsPool` (.bss+0x0930a2b8)
 * as a `CEvBuffersPool`-shaped object and claimed it was unused/out of scope. Its real
 * ground-truth type is `CEventsPool` (events_pool.h) -- an entirely different, simpler
 * class (a static 2048-slot freelist of embedded 12-byte `CLinkedEvent`-shaped nodes,
 * NOT a two-tier malloc'd-chunk allocator like CEvBuffersPool) -- confirmed by
 * `nm -C`'s own `_ZN11CEventsPool11GetNewEventEv` symbol taking `this` as a real
 * parameter, and by every one of its real callers (`CParamTracer::AppendSingleParam`/
 * `AppendAllParams`/`AppendParams`/`AppendParamsDontCareAddr` -- reconstructed this
 * pass -- plus the sibling `CControllerTracer`/`CCtrlAndParamTracer`/`CNoteTracer`/
 * `CEventsQueue`/`CRTRouter`, not yet reconstructed, seen only via xref trace) loading
 * the SAME literal address `mov DWORD PTR [esp],0x930a2b8` immediately before the
 * call -- i.e. `sm_oEventsPool` genuinely is wired up and IS the single pool instance
 * backing every heap `CLinkedEvent` node this project has traced so far.
 */

#ifndef EVENT_H
#define EVENT_H

#include "ev_buffers_pool.h"
#include "events_pool.h"

class CEvent {
public:
	/* Real ctor(s) not traced this pass (every real caller this pass reached
	 * builds its own tag/buf pair inline rather than through CEvent's own
	 * constructor -- see client_comm_server.h). Default-constructs to the
	 * "no buffer owned" state so the dtor below is always safe to run.
	 */
	CEvent() : mTag(0), mBuf(0) {}

	/* .text+0x08182520, 43 bytes -- real, see header comment. */
	~CEvent()
	{
		if (mTag < 0)
			sm_oEvBuffersPool.Free(mBuf);
	}

	/* .bss+0x0930a2a0. The single pool instance backing every CEvent/
	 * CClientCommServer-embedded-event buffer this pass reached.
	 */
	static CEvBuffersPool sm_oEvBuffersPool;

protected:
	int   mTag; /* +0x00 */
	void *mBuf; /* +0x04 */
};

class CLinkedEvent : public CEvent {
public:
	CLinkedEvent() : mNext(0) {}

	/* .bss+0x0930a2b8 -- the real, wired-up freelist pool backing every heap
	 * CLinkedEvent node this project has traced so far (CParamTracer/
	 * CControllerTracer's own Append* family, CEventsQueue, CRTRouter). See
	 * events_pool.h and this file's header comment correction above.
	 */
	static CEventsPool sm_oEventsPool;

	/* Real callers (CParamTracer::AppendSingleParam et al) build a fresh node's
	 * `{mEvTag, mEvBuf}`-sized tag word directly (no separate accessor exists in
	 * the real binary -- every real call site just does `mov [node],tagWord`
	 * against the raw pointer) then push it onto a `CLinkedEvent*&` list head via
	 * `node->mNext = head; head = node;`. Exposed here as plain public methods
	 * rather than re-deriving a raw-pointer idiom at every call site. */
	void SetTag(int tag) { mTag = tag; }
	int GetTag() const { return mTag; }
	void SetNext(CLinkedEvent *next) { mNext = next; }
	CLinkedEvent *GetNext() const { return (CLinkedEvent *)mNext; }

	/* CEventsPool::GetNewEvent() pops nodes off its freelist by writing
	 * directly through this same 12-byte {mTag,mBuf,mNext} layout -- grant it
	 * field access rather than exposing mNext publicly. */
	friend class CEventsPool;

private:
	void *mNext; /* +0x08 */
};

#endif /* EVENT_H */
