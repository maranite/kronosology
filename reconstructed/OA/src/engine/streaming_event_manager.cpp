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

/* Round 67 (solo): byte-identical D0/D1 pair (.text+0x5aafa0/0x5aafb0),
 * restores the base class' own vtable pointer, `_ZTV14CSTGAudioEvent+8` --
 * same idiom + `volatile` requirement as the already-real
 * `CSTGPlaybackEvent::~CSTGPlaybackEvent()` (playback_event_methods.cpp).
 */
CSTGStreamingEvent::~CSTGStreamingEvent()
{
	*(volatile unsigned int *)this = ToU32(_ZTV14CSTGAudioEvent + 8);
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

/*
 * CSTGStreamingEvent::HandleErrorReading() (`.text+0xd2070`, 22 bytes,
 * batch 2026-07-25) confirmed real: forwards `voice` (+0x70) to
 * `CSTGVoiceAllocator::sInstance->StealVoice()` (oa_engine.h, deliberately
 * deferred). This is also the real resolved target of
 * `CSTGHDRFileReader`/`CSTGStreamingFileReader::ProcessCommandError()`'s
 * own vtable-slot-5 dispatch (`call *0x14(vtbl)`) on a
 * `CSTGPlaybackEvent*`/`CSTGStreamingEvent*` respectively -- confirmed via
 * `readelf -r` resolution against `.rodata._ZTV18CSTGStreamingEvent`/
 * `.rodata._ZTV17CSTGPlaybackEvent` (CSTGPlaybackEvent's own override is a
 * confirmed bare `ret`, already real -- playback_event_methods.cpp).
 * Reproduced as a direct call rather than a redundant runtime vtable read,
 * matching this project's established precedent (see
 * `CSTGPlaybackBuffer::RemoveEvent`'s own vtable-slot-7 treatment).
 */
void CSTGStreamingEvent::HandleErrorReading()
{
	unsigned char *self = (unsigned char *)this;
	CSTGVoice *voice = (CSTGVoice *)FromU32(*(unsigned int *)(self + 0x70));
	CSTGVoiceAllocator::sInstance->StealVoice(voice);
}

/*
 * CSTGStreamingEvent::CloseFileDescriptorsIfNecessary() (`.text+0xd2330`,
 * 86 bytes, batch 2026-07-25) confirmed real: iterates `fds[0..fdCount)`
 * (`+0x24`, `+0x1c`), pushing each non-null, `fdsEnabled`-gated (`+0x94`,
 * re-read every iteration -- a real, faithfully-preserved quirk) entry as
 * `{fd, 0}` onto `CSTGFileCloser::sInstance`'s own first embedded ring --
 * the SAME target/shape every other file-daemon `ProcessCommands()`
 * sibling already pushes into (managers.cpp).
 */
void CSTGStreamingEvent::CloseFileDescriptorsIfNecessary()
{
	unsigned char *self = (unsigned char *)this;
	unsigned char fdCount = self[0x1c];

	for (unsigned char i = 0; i < fdCount; i++) {
		if (*(unsigned int *)(self + 0x94) == 0)
			continue;

		unsigned int fd = *(unsigned int *)(self + 0x24 + i * 4);
		if (fd == 0)
			continue;

		unsigned char *fc = (unsigned char *)CSTGFileCloser::sInstance;
		unsigned int fcCursor = *(unsigned int *)(fc + 0x4);
		unsigned char *fcEntry = FromU32(*(unsigned int *)(fc + 0x0)) + fcCursor * 8;
		*(unsigned int *)(fcEntry + 0) = fd;
		*(unsigned int *)(fcEntry + 4) = 0;
		*(unsigned int *)(fc + 0x4) = (fcCursor + 1) % *(unsigned int *)(fc + 0xc);
	}
}

/*
 * CSTGStreamingEventManager::ReturnFreeEvent(CSTGStreamingEvent*)
 * (`.text+0xd1e10`, 320 bytes, batch 2026-07-25) confirmed real via
 * objdump -dr. Reentrant-safe via a nesting counter (`field14c38`): ONLY
 * the OUTERMOST call (depth transitions 0->1) acquires the global
 * CLI-disabling lock (`rtwrap_global_save_flags_and_cli()`, genuine RTAI
 * hal primitive DEFINED INSIDE OA.ko itself at `.text+0x119890` -- NOT
 * forwarded to an external RTAI module symbol like every other
 * `rtwrap_*` wrapper; see bar2_stubs_c.cpp for the real body) -- but
 * `event->CloseFileDescriptorsIfNecessary()` itself is called
 * UNCONDITIONALLY on EVERY call, nested or not (confirmed via the real
 * control flow: both the depth==1 and depth>1 paths fall through to the
 * same call site). Symmetrically, only the call whose POST-decrement
 * depth reaches 0 releases the lock (`rtwrap_global_restore_flags()`).
 * `field14c3c`, if it equals `event` on entry, is unconditionally reset
 * to 0 first (real, confirmed touch; exact high-level purpose -- some
 * kind of "pending signal for this specific event" marker, given
 * `AddSoundingEvent()`'s own use of the same field -- not independently
 * determined, faithfully reproduced regardless).
 *
 * After that, `event` is unlinked from the `field14c24`/`field14c28`-
 * headed doubly-linked "sounding events" list (the SAME list
 * `AddSoundingEvent()`, sec 10.145, already threads through) and pushed
 * onto the `freeListHead`/`freeListTail` free list (tail-append,
 * symmetric with `GetFreeEvent()`'s own already-real pop-from-head).
 * Both lists use the SAME real convention `Initialize()` above already
 * establishes: a node's "link identity" is the ADDRESS of its own
 * `+0x30` field (`&node->f30`), not the node's base pointer -- `+0x30`
 * holds the NEXT node's link-address, `+0x34` the PREV node's
 * link-address, `+0x3c` a fixed "owning list head slot" back-pointer.
 */
extern "C" unsigned int rtwrap_global_save_flags_and_cli(void);
extern "C" void rtwrap_global_restore_flags(unsigned int flags);

void CSTGStreamingEventManager::ReturnFreeEvent(CSTGStreamingEvent *event)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *ev = (unsigned char *)event;
	unsigned int evAddr = ToU32(ev);

	unsigned int depth = *(unsigned int *)(base + 0x14c38) + 1;
	*(unsigned int *)(base + 0x14c38) = depth;

	if (depth == 1)
		*(unsigned int *)(base + 0x14c34) = rtwrap_global_save_flags_and_cli();

	if (*(unsigned int *)(base + 0x14c3c) == evAddr)
		*(unsigned int *)(base + 0x14c3c) = 0;

	event->CloseFileDescriptorsIfNecessary();

	depth = *(unsigned int *)(base + 0x14c38) - 1;
	*(unsigned int *)(base + 0x14c38) = depth;
	if (depth == 0)
		rtwrap_global_restore_flags(*(unsigned int *)(base + 0x14c34));

	/* Unlink `event` from the sounding-events list. field14c24 (head) /
	 * field14c28 (tail) hold LINK-ADDRESSES (&event->f30), not event
	 * base pointers -- see class comment above. */
	unsigned int selfLink = ToU32(ev + 0x30);

	if (selfLink == *(unsigned int *)(base + 0x14c24)) {
		*(unsigned int *)(base + 0x14c24) = *(unsigned int *)(ev + 0x30);
		if (selfLink == *(unsigned int *)(base + 0x14c28))
			*(unsigned int *)(base + 0x14c28) = *(unsigned int *)(ev + 0x34);
	} else if (selfLink == *(unsigned int *)(base + 0x14c28)) {
		*(unsigned int *)(base + 0x14c28) = *(unsigned int *)(ev + 0x34);
	}

	unsigned int prevLink = *(unsigned int *)(ev + 0x34);
	if (prevLink)
		*(unsigned int *)FromU32(prevLink) = *(unsigned int *)(ev + 0x30);
	unsigned int nextLink = *(unsigned int *)(ev + 0x30);
	if (nextLink)
		*(unsigned int *)(FromU32(nextLink) + 4) = prevLink;

	*(unsigned int *)(ev + 0x30) = 0;
	*(unsigned int *)(ev + 0x34) = 0;
	*(unsigned int *)(ev + 0x3c) = 0;
	*(unsigned int *)(base + 0x14c2c) -= 1;

	/* Push `event` onto the free list, tail-append -- the SAME
	 * link-address idiom Initialize() above already establishes
	 * (`linkNode = &event->f30`). */
	unsigned int linkNode = ToU32(ev + 0x30);
	unsigned int oldTail = *(unsigned int *)(base + 0x14c1c);
	if (oldTail == 0) {
		*(unsigned int *)(base + 0x14c18) = linkNode;
	} else {
		*(unsigned int *)(ev + 0x34) = oldTail;
		unsigned int nextOfOldTail = *(unsigned int *)FromU32(oldTail);
		*(unsigned int *)(ev + 0x30) = nextOfOldTail;
		if (nextOfOldTail)
			*(unsigned int *)(FromU32(nextOfOldTail) + 4) = linkNode;
		*(unsigned int *)FromU32(oldTail) = linkNode;
	}
	*(unsigned int *)(base + 0x14c1c) = linkNode;
	*(unsigned int *)(ev + 0x3c) = ToU32(base + 0x14c18);
	*(unsigned int *)(base + 0x14c20) += 1;
}
