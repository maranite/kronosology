// SPDX-License-Identifier: GPL-2.0
/*
 * test_playback_subrate.cpp  -  KAT for ../src/engine/playback_subrate.cpp
 * (batch 27): CSTGPlaybackBuffer::ProcessSubRate().
 *
 * Deliberately links ONLY playback_subrate.cpp -- all four of its real
 * external dependencies (CSTGPlaybackEvent::GetDispositionForReadAttempt/
 * USTGHDRUtils::ConvertWaveToSTGSamples/the embedded CSTGHDRCircularBuffer::
 * AdvanceReadPosition/CSTGDiskCostManager::UpdateHDRBufferWaterMarks) are
 * mocked locally with call-recording so every branch of this function's own
 * substantial state machine can be driven deterministically, independent of
 * those other classes' own already-KAT'd real bodies (test_playback_event_methods/
 * test_wave_sample_convert/test_playback_buffer_events cover those). This
 * matches the project's own established "give a dependency-heavy function
 * its own dedicated, fully-mocked TU" precedent.
 *
 * Every object whose address is round-tripped through a packed 32-bit field
 * (CSTGPlaybackBuffer itself, every CSTGPlaybackEvent, the ring source
 * buffer, TSTGArrayManager's own backing arrays) is mmap32()/MAP_32BIT-backed
 * per this project's own standing sec 10.156/10.157 pointer-width discipline.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_engine.h"
#include "oa_engine_init.h"

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s 0x%lx\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%lx)\n", want);
}

static unsigned char *mmap32(unsigned long size)
{
	return (unsigned char *)mmap(0, size, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

/* ---- required globals (real storage, per this project's own explicit-
 * specialization/static-member convention -- see playback_buffer_events.cpp's
 * own header comment for why the generic TSTGArrayManager<T>::sInstance
 * template definition doesn't cover this file, since engine_init.cpp isn't
 * linked here). ---- */
unsigned char CSTGAudioBusManager::sGlobalBusSet[34 * 0x80];
template<> TSTGArrayManager<CSTGPlaybackEvent> *TSTGArrayManager<CSTGPlaybackEvent>::sInstance = 0;
CSTGDiskCostManager *CSTGDiskCostManager::sInstance;

/* ---- mocked external dependencies ---- */
static unsigned int g_dispositionCalls, g_dispositionLastPos, g_dispositionReturn;
unsigned int CSTGPlaybackEvent::GetDispositionForReadAttempt(unsigned int pos) const
{
	g_dispositionCalls++;
	g_dispositionLastPos = pos;
	return g_dispositionReturn;
}

static unsigned int g_convertCalls;
static float *g_convertLastDest;
static bool g_convertLastStereoOut, g_convertLastResamplerFlag, g_convertLastSourceStereo, g_convertLastByteSwap;
static char *g_convertLastSrc;
static unsigned long g_convertLastCount, g_convertLastReserved9;
static CSTGPlaybackEvent *g_convertLastEvent;
static unsigned long g_convertReturn;
unsigned long USTGHDRUtils::ConvertWaveToSTGSamples(float *dest, bool stereoInterleavedOutput,
                                                     bool resamplerReservedFlag, char *src,
                                                     bool sourceIsStereo, bool needsByteSwap,
                                                     unsigned long count, CSTGPlaybackEvent *event,
                                                     unsigned long reservedArg9)
{
	g_convertCalls++;
	g_convertLastDest = dest;
	g_convertLastStereoOut = stereoInterleavedOutput;
	g_convertLastResamplerFlag = resamplerReservedFlag;
	g_convertLastSrc = src;
	g_convertLastSourceStereo = sourceIsStereo;
	g_convertLastByteSwap = needsByteSwap;
	g_convertLastCount = count;
	g_convertLastEvent = event;
	g_convertLastReserved9 = reservedArg9;
	return g_convertReturn;
}

/* Deliberately test-local simplified semantics (no wraparound of readPos
 * itself -- ProcessSubRate() never lets readPos cross bufferEnd without
 * separately updating it via the +0x40 startLoc path, so a plain linear
 * advance is sufficient to verify THIS function's own call/argument
 * bookkeeping; the ring's own internal wraparound arithmetic is already
 * covered by test_playback_buffer_events.cpp/test_hdr_manager_init.cpp). */
static unsigned int g_advanceCalls;
static unsigned long g_advanceLastN;
void CSTGHDRCircularBuffer::AdvanceReadPosition(unsigned long n)
{
	g_advanceCalls++;
	g_advanceLastN = n;
	readPos += n;
	availableReadBytes -= n;
}

static unsigned int g_waterMarkCalls;
static unsigned long g_waterMarkLastN;
static const CSTGHDRCircularBuffer *g_waterMarkLastBuf;
void CSTGDiskCostManager::UpdateHDRBufferWaterMarks(unsigned long n, const CSTGHDRCircularBuffer *buf)
{
	g_waterMarkCalls++;
	g_waterMarkLastN = n;
	g_waterMarkLastBuf = buf;
}

/* ---- fixture helpers ---- */
static unsigned char *NewPlaybackBuffer(unsigned char *ringBuf, unsigned int ringBase, unsigned int ringEnd,
                                         unsigned int readPos)
{
	(void)ringBuf;	/* kept as a parameter purely for call-site readability */
	unsigned char *pbBuf = mmap32(0x100);
	memset(pbBuf, 0, 0x100);
	CSTGHDRCircularBuffer *ring = (CSTGHDRCircularBuffer *)pbBuf;
	ring->bufferBase = ringBase;
	ring->bufferEnd = ringEnd;
	ring->readPos = readPos;
	ring->availableReadBytes = 100000;	/* generous, never the limiting factor here */
	return pbBuf;
}

static unsigned char *NewEvent(void)
{
	unsigned char *evt = mmap32(0x68);
	memset(evt, 0, 0x68);
	return evt;
}

int main(void)
{
	printf("playback_subrate (CSTGPlaybackBuffer::ProcessSubRate) known-answer test\n");
	printf("=========================================================\n");

	printf("[1] event's own remaining budget (+0x50) fully covers the tick:\n");
	printf("    exits BEFORE ever calling GetDispositionForReadAttempt/ConvertWaveToSTGSamples\n");
	{
		unsigned char *ringBuf = mmap32(0x1000);
		unsigned char *pbBuf = NewPlaybackBuffer(ringBuf, ToU32(ringBuf), ToU32(ringBuf) + 0x1000, ToU32(ringBuf));
		CSTGPlaybackBuffer *pb = (CSTGPlaybackBuffer *)pbBuf;
		unsigned char *destTarget = mmap32(0x1000);
		*(unsigned int *)(pbBuf + 0x54) = ToU32(destTarget);	/* +0x30 == 0 -> use +0x54 */

		unsigned char *evt = NewEvent();
		*(unsigned int *)(evt + 0x8) = 2;		/* state == current */
		*(unsigned int *)(evt + 0x50) = 0x20;		/* >= threshold(0x20, mono) */
		*(unsigned int *)(pbBuf + 0x4c) = ToU32(evt);

		g_dispositionCalls = 0;
		g_convertCalls = 0;
		g_advanceCalls = 0;
		g_waterMarkCalls = 0;

		pb->ProcessSubRate();

		check_eq("GetDispositionForReadAttempt NEVER called", g_dispositionCalls, 0);
		check_eq("ConvertWaveToSTGSamples NEVER called", g_convertCalls, 0);
		check_eq("event->+0x50 decremented to 0", *(unsigned int *)(evt + 0x50), 0);
	}

	printf("[2] main conversion path (no ring wraparound), mono/+0x51==0 -> threshold 0x20,\n");
	printf("    exactly reaches threshold in one chunk -> single pass, clean exit\n");
	{
		unsigned char *ringBuf = mmap32(0x1000);
		for (int i = 0; i < 0x1000; i++)
			ringBuf[i] = (unsigned char)i;
		unsigned int ringBase = ToU32(ringBuf);
		unsigned char *pbBuf = NewPlaybackBuffer(ringBuf, ringBase, ringBase + 0x1000, ringBase + 0x32);
		CSTGPlaybackBuffer *pb = (CSTGPlaybackBuffer *)pbBuf;
		unsigned char *destTarget = mmap32(0x1000);
		*(unsigned int *)(pbBuf + 0x54) = ToU32(destTarget);
		pbBuf[0x50] = 0;	/* needsByteSwap = false */
		pbBuf[0x51] = 0;	/* sourceIsStereo = false, threshold = 0x20 */
		pbBuf[0x52] = 1;	/* stereoInterleavedOutput = true */
		*(unsigned int *)(pbBuf + 0x48) = 1;	/* fillPending -> needsWaterMarkUpdate */

		unsigned char *evt = NewEvent();
		*(unsigned int *)(evt + 0x8) = 2;
		*(unsigned int *)(evt + 0x50) = 10;		/* grab = min(0x20,10) = 10 */
		*(unsigned int *)(evt + 0x44) = 22;		/* windowThreshold == budgetAfterGrab(22): no clamp */
		evt[0x1d] = 2;					/* bytesPerFrame */
		*(unsigned int *)(pbBuf + 0x4c) = ToU32(evt);

		g_dispositionCalls = 0; g_dispositionReturn = 0;	/* outcome 0: neither 1, 2 nor 3 */
		g_convertCalls = 0; g_convertReturn = 22;		/* consumedThisCall == 22 (fits in a byte) */
		g_advanceCalls = 0; g_waterMarkCalls = 0;

		pb->ProcessSubRate();

		check_eq("GetDispositionForReadAttempt called once", g_dispositionCalls, 1);
		check_eq("...with pos == budgetAfterGrab (0x20-10=22)", g_dispositionLastPos, 22);
		check_eq("ConvertWaveToSTGSamples called once (single pass)", g_convertCalls, 1);
		check_eq("...count == convertCount (22, unclamped)", g_convertLastCount, 22);
		check_eq("...dest == destBase + grab(10) floats", ToU32(g_convertLastDest), ToU32((float *)destTarget + 10));
		check_eq("...src == sConvertBuffer", ToU32((void *)g_convertLastSrc), ToU32(CSTGPlaybackBuffer::sConvertBuffer));
		check_eq("...stereoInterleavedOutput == +0x52", (unsigned long)g_convertLastStereoOut, 1);
		check_eq("...resamplerReservedFlag hardcoded true", (unsigned long)g_convertLastResamplerFlag, 1);
		check_eq("...sourceIsStereo == +0x51", (unsigned long)g_convertLastSourceStereo, 0);
		check_eq("...needsByteSwap == +0x50", (unsigned long)g_convertLastByteSwap, 0);
		check_eq("...reservedArg9 hardcoded 0", g_convertLastReserved9, 0);
		check_eq("...event forwarded unchanged", ToU32(g_convertLastEvent), ToU32(evt));
		check_eq("sConvertBuffer[0..43] == ring[0x32..0x75] (no wrap, single memcpy)",
			 memcmp(CSTGPlaybackBuffer::sConvertBuffer, ringBuf + 0x32, 44) == 0, 1);
		check_eq("event->+0x44 decremented by consumedThisCall (22-22=0)", *(unsigned int *)(evt + 0x44), 0);
		check_eq("AdvanceReadPosition called with consumedThisCall*bytesPerFrame (22*2=44)", g_advanceLastN, 44);
		check_eq("UpdateHDRBufferWaterMarks called (fillPending was set)", g_waterMarkCalls, 1);
		check_eq("...with this ring's own availableReadBytes", g_waterMarkLastN, ((CSTGHDRCircularBuffer *)pbBuf)->availableReadBytes);
		check_eq("this->+0x44 flag left at 0 (was already 0 going in)", pbBuf[0x44], 0);
	}

	printf("[3] +0x44 nonzero at entry: cleared, but STILL forces needsWaterMarkUpdate\n");
	{
		unsigned char *ringBuf = mmap32(0x1000);
		unsigned int ringBase = ToU32(ringBuf);
		unsigned char *pbBuf = NewPlaybackBuffer(ringBuf, ringBase, ringBase + 0x1000, ringBase);
		CSTGPlaybackBuffer *pb = (CSTGPlaybackBuffer *)pbBuf;
		pbBuf[0x30] = 1;	/* use the FIXED default bus this time */
		pbBuf[0x44] = 7;	/* nonzero -> forces needsWaterMarkUpdate, then gets cleared */
		*(unsigned int *)(pbBuf + 0x48) = 0;	/* fillPending == false on its own */

		unsigned char *evt = NewEvent();
		*(unsigned int *)(evt + 0x8) = 2;
		*(unsigned int *)(evt + 0x50) = 0x20;	/* covers the whole tick: early exit, no call reached */
		*(unsigned int *)(pbBuf + 0x4c) = ToU32(evt);

		g_waterMarkCalls = 0;
		pb->ProcessSubRate();

		check_eq("+0x44 cleared", pbBuf[0x44], 0);
		/* Early-exit path (budgetAfterGrab==0) returns before the water-mark
		 * call site is ever reached -- needsWaterMarkUpdate being true here
		 * has no observable effect in THIS scenario, only verifying the
		 * clear itself and that +0x30!=0 didn't crash (destBase computed
		 * from sGlobalBusSet, never dereferenced since no call is reached). */
		check_eq("water mark call not reached (early exit)", g_waterMarkCalls, 0);
	}

	printf("[4] ring wraparound: copyByteCount spans past bufferEnd, continues from bufferBase\n");
	{
		unsigned char ringBuf[16];
		for (int i = 0; i < 16; i++)
			ringBuf[i] = (unsigned char)(0x10 + i);
		unsigned char *ringPage = mmap32(0x1000);
		memcpy(ringPage, ringBuf, 16);
		unsigned int base = ToU32(ringPage);
		unsigned char *pbBuf = NewPlaybackBuffer(ringPage, base, base + 16, base + 12);	/* 4 bytes until wrap */
		CSTGPlaybackBuffer *pb = (CSTGPlaybackBuffer *)pbBuf;
		unsigned char *destTarget = mmap32(0x1000);
		*(unsigned int *)(pbBuf + 0x54) = ToU32(destTarget);
		pbBuf[0x51] = 1;	/* stereo -> threshold 0x40 */

		unsigned char *evt = NewEvent();
		*(unsigned int *)(evt + 0x8) = 2;
		*(unsigned int *)(evt + 0x50) = 5;		/* grab = 5 */
		*(unsigned int *)(evt + 0x44) = 5;		/* convertCount clamps to 5 (== budgetAfterGrab(0x40-5=59)? no: */
		evt[0x1d] = 2;					/* bytesPerFrame = 2 -> copyByteCount = 5*2 = 10 */
		*(unsigned int *)(pbBuf + 0x4c) = ToU32(evt);

		g_dispositionReturn = 3;	/* outcome==3 after this chunk: ends the function via case_outcome3 */
		g_convertReturn = 5;
		g_advanceCalls = 0;

		pb->ProcessSubRate();

		/* copyByteCount = 10, bytesUntilWrap = bufferEnd-readPos = 4:
		 * first 4 bytes from readPos (offsets 12..15 == 0x1c,0x1d,0x1e,0x1f),
		 * remaining 6 bytes wrap to bufferBase (offsets 0..5 == 0x10..0x15). */
		unsigned char expect[10] = { 0x1c, 0x1d, 0x1e, 0x1f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15 };
		check_eq("wraparound copy matches expected byte sequence",
			 memcmp(CSTGPlaybackBuffer::sConvertBuffer, expect, 10) == 0, 1);
	}

	printf("[5] outcome == 3: event->+0x38 bookkeeping via the carry == consumed-convertCount identity\n");
	{
		unsigned char *ringBuf = mmap32(0x1000);
		unsigned int base = ToU32(ringBuf);
		unsigned char *pbBuf = NewPlaybackBuffer(ringBuf, base, base + 0x1000, base);
		CSTGPlaybackBuffer *pb = (CSTGPlaybackBuffer *)pbBuf;
		unsigned char *destTarget = mmap32(0x1000);
		*(unsigned int *)(pbBuf + 0x54) = ToU32(destTarget);
		pbBuf[0x51] = 0;	/* threshold = 0x20 */

		unsigned char *evt = NewEvent();
		*(unsigned int *)(evt + 0x8) = 2;
		*(unsigned int *)(evt + 0x50) = 8;		/* grab = min(0x20,8) = 8; budgetAfterGrab = 0x18 (24) */
		*(unsigned int *)(evt + 0x44) = 24;		/* == budgetAfterGrab: no clamp, convertCount = 24 */
		evt[0x1d] = 1;					/* bytesPerFrame = 1 -> copyByteCount = 24 */
		*(unsigned int *)(evt + 0x38) = 1000;		/* windowSize */
		*(unsigned int *)(pbBuf + 0x4c) = ToU32(evt);

		g_dispositionReturn = 3;
		g_convertReturn = 24;

		pb->ProcessSubRate();

		/* consumed == grab+convertCount == 8+24 == 32; carry == consumed-convertCount == 8;
		 * rem == threshold-carry == 0x20-8 == 24; new +0x38 == rem*bytesPerFrame(1) +
		 * (windowSize(1000) - copyByteCount(24)) == 24 + 976 == 1000. */
		check_eq("event->+0x38 updated per the outcome==3 formula", *(unsigned int *)(evt + 0x38), 1000);
	}

	printf("[6] outcome == 2, nextEvt != NULL: this->+0x4c retargeted, nextEvt->+0x38 updated\n");
	{
		unsigned char *ringBuf = mmap32(0x1000);
		unsigned int base = ToU32(ringBuf);
		unsigned char *pbBuf = NewPlaybackBuffer(ringBuf, base, base + 0x1000, base);
		CSTGPlaybackBuffer *pb = (CSTGPlaybackBuffer *)pbBuf;
		unsigned char *destTarget = mmap32(0x1000);
		*(unsigned int *)(pbBuf + 0x54) = ToU32(destTarget);
		pbBuf[0x51] = 0;	/* threshold = 0x20 */

		unsigned char *nextEvt = NewEvent();
		nextEvt[0x1d] = 3;	/* nextBytesPerFrame */

		unsigned char *evt = NewEvent();
		*(unsigned int *)(evt + 0x8) = 2;
		*(unsigned int *)(evt + 0x50) = 8;		/* grab=8, budgetAfterGrab=24 */
		*(unsigned int *)(evt + 0x44) = 24;		/* no clamp, convertCount=24 */
		evt[0x1d] = 1;
		*(unsigned int *)(evt + 0x54) = ToU32(nextEvt);
		*(unsigned int *)(evt + 0x48) = 50;		/* maxReadBytes */
		*(unsigned int *)(evt + 0x38) = 30;		/* windowSize */
		*(unsigned int *)(evt + 0x3c) = 10;		/* consumed (event's own) */
		*(unsigned int *)(pbBuf + 0x4c) = ToU32(evt);

		g_dispositionReturn = 2;
		g_convertReturn = 24;

		pb->ProcessSubRate();

		/* consumed(call-local) == 8+24 == 32. span = windowSize(30)-evtConsumed(10) == 20.
		 * avail = maxReadBytes(50)-span(20) == 30 (non-negative, kept as-is).
		 * chunks = avail(30)/bytesPerFrame(1) == 30.
		 * delta = (threshold(0x20=32) - chunks(30) - consumed(32)) * nextBytesPerFrame(3)
		 *       = (32-30-32)*3 == (-30)*3 == -90 (wraps as unsigned 32-bit). */
		check_eq("event->+0x8 retired to 3", *(unsigned int *)(evt + 0x8), 3);
		check_eq("this->+0x4c retargeted to nextEvt", *(unsigned int *)(pbBuf + 0x4c), ToU32(nextEvt));
		check_eq("nextEvt->+0x8 == 2 (current)", *(unsigned int *)(nextEvt + 0x8), 2);
		unsigned int expectDelta = (unsigned int)((32 - 30 - 32) * 3);
		check_eq("nextEvt->+0x38 == 0 + delta (wrapping arithmetic, faithful)",
			 *(unsigned int *)(nextEvt + 0x38), expectDelta);
	}

	printf("[7] outcome == 2, nextEvt == NULL: this->+0x4c simply cleared\n");
	{
		unsigned char *ringBuf = mmap32(0x1000);
		unsigned int base = ToU32(ringBuf);
		unsigned char *pbBuf = NewPlaybackBuffer(ringBuf, base, base + 0x1000, base);
		CSTGPlaybackBuffer *pb = (CSTGPlaybackBuffer *)pbBuf;
		unsigned char *destTarget = mmap32(0x1000);
		*(unsigned int *)(pbBuf + 0x54) = ToU32(destTarget);
		pbBuf[0x51] = 0;

		unsigned char *evt = NewEvent();
		*(unsigned int *)(evt + 0x8) = 2;
		*(unsigned int *)(evt + 0x50) = 8;
		*(unsigned int *)(evt + 0x44) = 24;
		evt[0x1d] = 1;
		*(unsigned int *)(evt + 0x54) = 0;	/* no next event */
		*(unsigned int *)(pbBuf + 0x4c) = ToU32(evt);

		g_dispositionReturn = 2;
		g_convertReturn = 24;

		pb->ProcessSubRate();

		check_eq("this->+0x4c cleared (no nextEvt)", *(unsigned int *)(pbBuf + 0x4c), 0);
	}

	printf("[8] outcome == 1, fieldC == 4 (ctor default): recycled onto the free list,\n");
	printf("    then chases +0x54 to make nextEvt current\n");
	{
		unsigned char *ringBuf = mmap32(0x1000);
		unsigned int base = ToU32(ringBuf);
		unsigned char *pbBuf = NewPlaybackBuffer(ringBuf, base, base + 0x1000, base);
		CSTGPlaybackBuffer *pb = (CSTGPlaybackBuffer *)pbBuf;
		unsigned char *destTarget = mmap32(0x1000);
		*(unsigned int *)(pbBuf + 0x54) = ToU32(destTarget);
		pbBuf[0x51] = 0;

		unsigned char *bucketBuf = mmap32(0x1000);
		memset(bucketBuf, 0, 0x1000);
		TSTGArrayManager<CSTGPlaybackEvent> arrMgr;
		memset(&arrMgr, 0, sizeof(arrMgr));
		arrMgr.bucketArray = ToU32(bucketBuf);
		arrMgr.writeCursor = 3;
		arrMgr.modulus = 65;
		unsigned char *arrMgrBuf = mmap32(0x1000);
		memcpy(arrMgrBuf, &arrMgr, sizeof(arrMgr));
		TSTGArrayManager<CSTGPlaybackEvent>::sInstance = (TSTGArrayManager<CSTGPlaybackEvent> *)arrMgrBuf;

		unsigned char *nextEvt = NewEvent();
		char nextStartLoc[4];
		*(unsigned int *)(nextEvt + 0x40) = ToU32(nextStartLoc);

		unsigned char *evt = NewEvent();
		*(unsigned int *)(evt + 0x8) = 2;
		*(unsigned int *)(evt + 0x50) = 5;	/* grab = min(0x20,5) = 5 */
		*(unsigned int *)(evt + 0x44) = 27;	/* budgetAfterGrab = 0x20-5 = 27, no clamp */
		evt[0x1d] = 1;
		*(unsigned int *)(evt + 0xc) = 4;	/* CSTGAudioEvent::fieldC == 4 -> recycle */
		*(unsigned int *)(evt + 0x54) = ToU32(nextEvt);
		*(unsigned int *)(pbBuf + 0x4c) = ToU32(evt);

		g_dispositionReturn = 1;	/* outcome == 1: retire + advance */
		g_convertReturn = 27;		/* fully drains windowThreshold -> next disposition
						 * call never reached (threshold<=consumed exit) */

		unsigned int wcBefore = TSTGArrayManager<CSTGPlaybackEvent>::sInstance->writeCursor;
		unsigned int *bucket = (unsigned int *)FromU32(TSTGArrayManager<CSTGPlaybackEvent>::sInstance->bucketArray);

		pb->ProcessSubRate();

		/* CONFIRMED REAL QUIRK (re-derived from the raw disassembly, not
		 * assumed): recycle_event() sets evt->+0x8 = 0, but self->+0x4c
		 * still points at this SAME (just-recycled) event when
		 * advance_to_next immediately re-examines "curReadEvt" -- since
		 * recycling never touches self->+0x4c, curReadEvt IS evt here.
		 * Its state (0) != 3, so the retire-old-event block unconditionally
		 * forces it back to 3 a moment later. Net effect: evt->+0x8 ends up
		 * 3, NOT 0, even though it visibly passed through 0 in between. */
		check_eq("evt->+0x8 == 3 (curReadEvt-retire re-forces it, even though recycle_event set 0)",
			 *(unsigned int *)(evt + 0x8), 3);
		check_eq("evt pushed onto the free list at the old writeCursor", bucket[wcBefore], ToU32(evt));
		check_eq("writeCursor advanced by 1", TSTGArrayManager<CSTGPlaybackEvent>::sInstance->writeCursor, wcBefore + 1);
		check_eq("this->+0x4c retargeted to nextEvt", *(unsigned int *)(pbBuf + 0x4c), ToU32(nextEvt));
		check_eq("nextEvt->+0x8 == 2 (current)", *(unsigned int *)(nextEvt + 0x8), 2);
		check_eq("ring readPos updated to nextEvt's own +0x40 start location",
			 ((CSTGHDRCircularBuffer *)pbBuf)->readPos, ToU32(nextStartLoc));
	}

	printf("[9] outcome == 1, fieldC != 4: NOT recycled, chases +0x54 directly on the same event\n");
	{
		unsigned char *ringBuf = mmap32(0x1000);
		unsigned int base = ToU32(ringBuf);
		unsigned char *pbBuf = NewPlaybackBuffer(ringBuf, base, base + 0x1000, base);
		CSTGPlaybackBuffer *pb = (CSTGPlaybackBuffer *)pbBuf;
		unsigned char *destTarget = mmap32(0x1000);
		*(unsigned int *)(pbBuf + 0x54) = ToU32(destTarget);
		pbBuf[0x51] = 0;

		unsigned char *evt = NewEvent();
		*(unsigned int *)(evt + 0x8) = 2;
		*(unsigned int *)(evt + 0x50) = 5;
		*(unsigned int *)(evt + 0x44) = 27;
		evt[0x1d] = 1;
		*(unsigned int *)(evt + 0xc) = 99;	/* fieldC != 4 -> skip recycle_event entirely */
		*(unsigned int *)(evt + 0x54) = 0;	/* no next event: this->+0x4c gets cleared */
		*(unsigned int *)(pbBuf + 0x4c) = ToU32(evt);

		g_dispositionReturn = 1;
		g_convertReturn = 27;

		pb->ProcessSubRate();

		check_eq("evt->+0x8 stays 3 (recycle_event never ran, so no 0-overwrite)",
			 *(unsigned int *)(evt + 0x8), 3);
		check_eq("this->+0x4c cleared (no next event)", *(unsigned int *)(pbBuf + 0x4c), 0);
	}

	printf("[10] destBase selection: +0x30 != 0 picks the fixed &sGlobalBusSet[32] default\n");
	{
		unsigned char *ringBuf = mmap32(0x1000);
		unsigned int base = ToU32(ringBuf);
		unsigned char *pbBuf = NewPlaybackBuffer(ringBuf, base, base + 0x1000, base);
		CSTGPlaybackBuffer *pb = (CSTGPlaybackBuffer *)pbBuf;
		pbBuf[0x30] = 1;
		*(unsigned int *)(pbBuf + 0x54) = 0xdeadbeef;	/* must be IGNORED when +0x30 != 0 */
		pbBuf[0x51] = 0;

		unsigned char *evt = NewEvent();
		*(unsigned int *)(evt + 0x8) = 2;
		*(unsigned int *)(evt + 0x50) = 5;
		*(unsigned int *)(evt + 0x44) = 27;
		evt[0x1d] = 1;
		*(unsigned int *)(pbBuf + 0x4c) = ToU32(evt);

		g_dispositionReturn = 0;
		g_convertReturn = 27;

		pb->ProcessSubRate();

		check_eq("dest == &sGlobalBusSet[32] + grab(5) floats, NOT the +0x54 garbage pointer",
			 ToU32(g_convertLastDest), ToU32((float *)(CSTGAudioBusManager::sGlobalBusSet + 32 * 0x80) + 5));
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("All checks passed.\n");
	return 0;
}
