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
 * nothing more, laid out contiguously. `CLinkedEvent::sm_oEventsPool` (a SEPARATE
 * static `CEvBuffersPool`-shaped object at .bss+0x0930a2b8, own
 * `_GLOBAL__I_sm_oEventsPool` constructor at .text+0x0807fba0) is a different pool
 * instance for whatever allocates real heap `CLinkedEvent` NODE objects elsewhere
 * (CEventsQueue et al) -- out of scope here, since nothing this pass reconstructs ever
 * calls through it; only `CEvent::sm_oEvBuffersPool` (the pool
 * `CClientCommServer`'s own ctor uses) is wired up.
 */

#ifndef EVENT_H
#define EVENT_H

#include "ev_buffers_pool.h"

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

	/* .bss+0x0930a2b8 -- a DIFFERENT pool instance, out of scope, see header
	 * comment. Declared for shape/symbol-table completeness only; nothing
	 * reconstructed this pass calls through it.
	 */
	static CEvBuffersPool sm_oEventsPool;

private:
	void *mNext; /* +0x08 */
};

#endif /* EVENT_H */
