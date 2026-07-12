// SPDX-License-Identifier: GPL-2.0
/*
 * test_playback_buffer_events.cpp  -  KAT for
 * ../src/engine/playback_buffer_events.cpp's reconstructed
 * CSTGPlaybackBuffer methods: batch 24's six (EventBufferStartLocationUpdated/
 * SetCurrentReadEvent/AdvanceToNextFillEvent/HandleAdvanceCancelledEvent/
 * AddEvent/AdvanceReadPosition, plus the one CSTGDiskCostManager method
 * (UpdateHDRBufferWaterMarks, engine_startup_bits2.cpp) the last of these
 * conditionally calls), plus batch 25's RemoveEvent()/EventFileError()
 * (sections [7]/[8]) and playback_event_methods.cpp's
 * CSTGPlaybackEvent::HandleFileClosed() (section [9], the one real
 * integration point between the two files -- this test now links
 * playback_event_methods.cpp too).
 *
 * Links src/engine/managers.cpp directly (for the embedded
 * CSTGHDRCircularBuffer's own already-real AdvanceReadPosition()/
 * ReturnUnusedFillBytes(), and for CSTGDiskCostManager::sInstance's own
 * storage) -- the "mocks needed to link managers.cpp" block below is
 * copied verbatim from test_engine_startup_bits2.cpp (same file, same
 * requirement) since this test links the exact same managers.cpp object.
 * Also links engine_startup_bits2.cpp directly (for
 * CSTGDiskCostManager::UpdateHDRBufferWaterMarks/Initialize) and
 * src/mem/bank_memory.cpp (CSTGBankMemory, needed transitively by both).
 *
 * Every object whose own address gets round-tripped through a packed
 * 32-bit field (CSTGPlaybackBuffer itself, every CSTGPlaybackEvent,
 * CSTGDiskCostManager, the TSTGArrayManager<CSTGPlaybackEvent>::sInstance
 * index array) is mmap32()/MAP_32BIT-backed, per this project's own
 * standing sec 10.156/10.157 pointer-width discipline.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/mman.h>
#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"
#include "oa_bank_memory.h"

/* ---- mocks needed to link managers.cpp (verbatim from
 * test_engine_startup_bits2.cpp -- same object file, same requirement) ---- */
static int g_mutexInitCalls;
/* Sec 10.225: CSTGAudioDriverInterface::sInstance + the KorgUsbAudio*
 * externs CSTGAudioDriverInterfaceKorgUsb::Initialize()/Start()/
 * KeepSynchronized() now call directly (managers.cpp, which this file
 * links) -- link-satisfying host mocks only, same treatment as the
 * RTAI wrappers below. */
CSTGAudioDriverInterface *CSTGAudioDriverInterface::sInstance;
extern "C" int   KorgUsbAudioInitialize(void) { return 0; }
extern "C" int   KorgUsbAudioInitialized(void) { return 0; }
extern "C" int   KorgUsbAudioStart(void) { return 0; }
extern "C" void *KorgUsbAudioInput(void) { return 0; }
extern "C" void  KorgUsbAudioInputDone(void) { }
extern "C" void *KorgUsbAudioOutput(void) { return 0; }
extern "C" void  KorgUsbAudioOutputDone(void) { }
extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void) { return 24; }
extern "C" void *rtwrap_malloc(unsigned int size) { return malloc(size); }
extern "C" void rtwrap_pthread_mutex_init(void *, void *) { g_mutexInitCalls++; }
void CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors() { }
unsigned char *ResolveActivePerformanceVarsManagerRaw() { return 0; }
extern "C" void *__kmalloc(unsigned long size, unsigned int) { return malloc(size); }
void CSTGHDRManager::ProcessPlaybackCommands() { }
/* CSTGHDRManager::ProcessRecordCommands() is real (hdr_record_track.cpp,
 * linked directly by this file for CSTGRecordTrack::Initialize()) --
 * NOT mocked here, unlike test_engine_startup_bits2.cpp which doesn't
 * link that file. */
void CSTGHDRManager::ProcessSamplerCommands() { }
static int g_mutexattrCalls;
extern "C" void rtwrap_pthread_mutexattr_init(void *) { g_mutexattrCalls++; }
extern "C" int  get_pthread_recursive_attr_constant(void) { return 1; }
static int g_condInitCalls;
extern "C" unsigned int get_sizeof_rtwrap_pthread_cond(void) { return 24; }
extern "C" void rtwrap_pthread_cond_init(void *, void *) { g_condInitCalls++; }
/* CSTGAudioManager::~CSTGAudioManager() is now real (managers.cpp,
 * linked directly by this test, sec 10.225 -- no longer virtual, no
 * mock needed here any more). */
extern "C" void rtwrap_pthread_mutex_destroy(void *) { }
extern "C" void rtwrap_free(void *) { }
extern "C" void rtwrap_pthread_mutexattr_settype(void *, int) { g_mutexattrCalls++; }
extern "C" void rtwrap_pthread_mutexattr_destroy(void *) { g_mutexattrCalls++; }
extern "C" void rtwrap_pthread_mutex_lock(void *) { }
extern "C" void rtwrap_pthread_mutex_unlock(void *) { }
void CSTGVoiceAllocator::FreeVoice(CSTGVoice *) { }
void CSTGVoiceAllocator::DoPendingMoveVoices() { }
float gAllPlusHeadroom[4]  = { -99.0f, -99.0f, -99.0f, -99.0f };
float gAllMinusHeadroom[4] = {  99.0f,  99.0f,  99.0f,  99.0f };
unsigned char *STGAPIFrontPanelStatus::sInstance;
template<> TSTGArrayManager<CSTGRecordBuffer> *TSTGArrayManager<CSTGRecordBuffer>::sInstance = 0;
unsigned char CSTGAudioBusManager::sGlobalBusSet[34 * 0x80];
/* TSTGArrayManager<T>::sInstance's own generic (non-specialized) template
 * definition lives only in engine_init.cpp (not linked here) -- this file
 * links managers.cpp/playback_buffer_events.cpp directly and needs its
 * own explicit-specialization storage for this one instantiation, same
 * precedent as TSTGArrayManager<CSTGRecordBuffer> above (sec 10.160).
 * NOTE: playback_buffer_events.cpp itself must NOT also define this --
 * it IS linked alongside engine_init.cpp in the real .ko build, where
 * engine_init.cpp's own generic template line already provides it
 * (a real "multiple definition" ld error the first time this was tried,
 * batch 24). */
template<> TSTGArrayManager<CSTGPlaybackEvent> *TSTGArrayManager<CSTGPlaybackEvent>::sInstance = 0;
static unsigned int g_onlineCpus = 2, g_khz = 1500000;
extern "C" unsigned int stg_num_online_cpus(void) { return g_onlineCpus; }
extern "C" unsigned int stg_get_cpu_khz(void) { return g_khz; }
/* Link-satisfying only: this file links the whole of engine_startup_bits2.cpp
 * (for CSTGDiskCostManager::UpdateHDRBufferWaterMarks) which also contains
 * CLoadBalancer::Initialize()/CSTGCommonLFO::Initialize()/CSTGCommonStepSeq::
 * Initialize(), none of which this test calls. */
CSTGCPUInfo *CSTGCPUInfo::sInstance;
void CSTGLFOBase::InitializeQuad(STGLFOSubRateParams *) { }
void CSTGStepSeqBase::InitializeQuad(STGStepSeqSubRateParams *) { }
/* Link-satisfying only: audio_input_mixer.cpp's own SetOutputBus()/
 * CBusChangeStateMachine::StartBusChange() (pulled in for
 * CSTGAudioInputMixerBase::Initialize(), needed by
 * CSTGCDAudioPlay::Initialize()) reference this real static pointer;
 * nothing in this test's own call paths reaches either of those methods. */
unsigned char CSTGPerformanceVarsManager::sInstance[12];
/* Needed now that this file also links playback_event_methods.cpp
 * (batch 25, for RemoveEvent()/EventFileError()'s own real
 * CSTGPlaybackEvent::Reset() dependency) -- ~CSTGPlaybackEvent()
 * references this symbol directly; same 40-byte confirmed size
 * (readelf) as every other sibling in this vtable family. */
unsigned char _ZTV14CSTGAudioEvent[40];
/* CSTGPlaybackEvent::HandleFileOpened()'s own CSTGFile_GetFileSize()
 * dependency -- not otherwise mocked in this file, and not exercised by
 * this test (only RemoveEvent()/EventFileError()/Reset() are). */
extern "C" unsigned int CSTGFile_GetFileSize(void *) { return 0; }

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

int main(void)
{
	printf("playback_buffer_events known-answer test\n");
	printf("=========================================================\n");

	unsigned char *pool = mmap32(0x200000);
	CSTGBankMemory::Initialize(pool, 0x200000);

	/* Real CSTGPlaybackBuffer::Initialize(totalSize) sets up the embedded
	 * CSTGHDRCircularBuffer (readPos == bufferBase, bufferEnd ==
	 * bufferBase+totalSize, availableFillBytes == totalSize) plus the
	 * ring (+0x34 alloc'd buffer, +0x40 == 0xfa1 capacity, +0x38/+0x3c
	 * write/read cursors implicitly 0 -- ctor already zeroed them). */
	unsigned char *pbBuf = mmap32(0x1000);
	memset(pbBuf, 0xcc, 0x1000);
	CSTGPlaybackBuffer *pb = new (pbBuf) CSTGPlaybackBuffer();
	pb->Initialize(0x2000ul);

	CSTGHDRCircularBuffer *cb = (CSTGHDRCircularBuffer *)pbBuf;
	unsigned int bufferBase = cb->bufferBase;

	/* Give TSTGArrayManager<CSTGPlaybackEvent>::sInstance's own indexArray
	 * a real small backing table -- AdvanceToNextFillEvent() dereferences
	 * it directly. */
	unsigned char *arrMgrBuf = mmap32(0x1000);
	memset(arrMgrBuf, 0, 0x1000);
	TSTGArrayManager<CSTGPlaybackEvent>::sInstance = (TSTGArrayManager<CSTGPlaybackEvent> *)arrMgrBuf;
	unsigned char *indexArrayBuf = mmap32(0x1000);
	memset(indexArrayBuf, 0, 0x1000);
	TSTGArrayManager<CSTGPlaybackEvent>::sInstance->indexArray = ToU32(indexArrayBuf);
	/* Batch 25: RemoveEvent()/EventFileError()'s own free-list push also
	 * needs a real bucketArray + modulus (BuildArrayManager's own real
	 * shape, engine_init.cpp -- modulus == count+1). */
	unsigned char *bucketArrayBuf = mmap32(0x1000);
	memset(bucketArrayBuf, 0, 0x1000);
	TSTGArrayManager<CSTGPlaybackEvent>::sInstance->bucketArray = ToU32(bucketArrayBuf);
	TSTGArrayManager<CSTGPlaybackEvent>::sInstance->modulus = 65;	/* matches capacity+1, arbitrary but nonzero */

	/* Three fake CSTGPlaybackEvent objects (real confirmed size 0x68). */
	unsigned char *eventBufs[3];
	for (int i = 0; i < 3; i++) {
		eventBufs[i] = mmap32(0x68);
		memset(eventBufs[i], 0, 0x68);
		*(unsigned short *)(eventBufs[i] + 0x4) = (unsigned short)i;	/* array id */
	}
	CSTGPlaybackEvent *evtA = (CSTGPlaybackEvent *)eventBufs[0];
	CSTGPlaybackEvent *evtB = (CSTGPlaybackEvent *)eventBufs[1];
	CSTGPlaybackEvent *evtC = (CSTGPlaybackEvent *)eventBufs[2];

	printf("[1] EventBufferStartLocationUpdated -- only touches readPos when\n");
	printf("    the event argument IS the current read event\n");
	{
		*(unsigned int *)((unsigned char *)pbBuf + 0x4c) = ToU32(evtA);
		char newLoc[4];
		pb->EventBufferStartLocationUpdated(evtA, newLoc);
		check_eq("readPos updated to newLoc (event == current)", cb->readPos, ToU32(newLoc));

		cb->readPos = bufferBase;	/* reset */
		pb->EventBufferStartLocationUpdated(evtB, newLoc);
		check_eq("readPos UNCHANGED (event != current)", cb->readPos, bufferBase);
	}

	printf("[2] SetCurrentReadEvent\n");
	{
		*(unsigned int *)(pbBuf + 0x4c) = 0;	/* no current event yet */
		cb->readPos = bufferBase;

		/* [2a] first-ever assignment: no old event to retire. */
		*(unsigned int *)((unsigned char *)evtA + 0x40) = 0;	/* no start-loc yet */
		pb->SetCurrentReadEvent(evtA);
		check_eq("this->+0x4c == evtA", *(unsigned int *)(pbBuf + 0x4c), ToU32(evtA));
		check_eq("evtA->+0x8 == 2 (current)", *(unsigned int *)((unsigned char *)evtA + 0x8), 2);
		check_eq("readPos unchanged (evtA->+0x40 == 0)", cb->readPos, bufferBase);

		/* [2b] displace evtA with evtB: evtA->+0x8 != 3, evtA->+0x54 !=
		 * evtB, so evtA->+0x8 becomes 3 AND evtA->+0x16 becomes 1;
		 * evtB->+0x40 non-zero updates readPos. */
		char startLocB[4];
		*(unsigned int *)((unsigned char *)evtB + 0x40) = ToU32(startLocB);
		*(unsigned int *)((unsigned char *)evtA + 0x54) = 0;	/* != evtB */
		pb->SetCurrentReadEvent(evtB);
		check_eq("evtA->+0x8 retired to 3", *(unsigned int *)((unsigned char *)evtA + 0x8), 3);
		check_eq("evtA->+0x16 flagged (evtB != evtA->+0x54)", ((unsigned char *)evtA)[0x16], 1);
		check_eq("this->+0x4c == evtB", *(unsigned int *)(pbBuf + 0x4c), ToU32(evtB));
		check_eq("evtB->+0x8 == 2 (current)", *(unsigned int *)((unsigned char *)evtB + 0x8), 2);
		check_eq("readPos updated to evtB's own +0x40", cb->readPos, ToU32(startLocB));

		/* [2c] displace evtB with evtC, but this time evtB->+0x54 IS
		 * evtC -- the +0x16 flag write must be SKIPPED (state still
		 * retires to 3 regardless). */
		((unsigned char *)evtB)[0x16] = 0;
		*(unsigned int *)((unsigned char *)evtB + 0x54) = ToU32(evtC);
		*(unsigned int *)((unsigned char *)evtC + 0x40) = 0;
		pb->SetCurrentReadEvent(evtC);
		check_eq("evtB->+0x8 retired to 3", *(unsigned int *)((unsigned char *)evtB + 0x8), 3);
		check_eq("evtB->+0x16 SKIPPED (evtC == evtB->+0x54)", ((unsigned char *)evtB)[0x16], 0);

		/* [2d] re-selecting the SAME already-current event is a no-op
		 * on the retire path (old == new, skip block entirely), but
		 * still re-marks state 2 and re-checks the start-loc mirror. */
		*(unsigned int *)((unsigned char *)evtC + 0x8) = 2;
		cb->readPos = bufferBase;
		pb->SetCurrentReadEvent(evtC);
		check_eq("evtC->+0x8 still 2 (no retire-self)", *(unsigned int *)((unsigned char *)evtC + 0x8), 2);
		check_eq("readPos unchanged (evtC->+0x40 == 0)", cb->readPos, bufferBase);

		/* [2e] the "already state 3" skip: an old event already marked
		 * finished must NOT have its own +0x16 touched again. */
		unsigned char *evtOldBuf = mmap32(0x68);
		memset(evtOldBuf, 0, 0x68);
		CSTGPlaybackEvent *evtOld = (CSTGPlaybackEvent *)evtOldBuf;
		*(unsigned int *)(evtOldBuf + 0x8) = 3;	/* already finished */
		evtOldBuf[0x16] = 0;
		*(unsigned int *)(pbBuf + 0x4c) = ToU32(evtOld);
		pb->SetCurrentReadEvent(evtA);
		check_eq("already-finished old event's +0x16 left untouched", evtOldBuf[0x16], 0);
	}

	printf("[3] AdvanceToNextFillEvent\n");
	{
		unsigned short *ring = (unsigned short *)FromU32(*(unsigned int *)(pbBuf + 0x34));
		unsigned int *indexArray = (unsigned int *)FromU32(TSTGArrayManager<CSTGPlaybackEvent>::sInstance->indexArray);
		indexArray[7] = ToU32(evtB);

		/* [3a] fillEvent == NULL: early return, nothing changes. */
		*(unsigned int *)(pbBuf + 0x48) = 0;
		*(unsigned int *)(pbBuf + 0x38) = 5;
		*(unsigned int *)(pbBuf + 0x3c) = 2;
		pb->AdvanceToNextFillEvent();
		check_eq("fillEvent still 0 (early return)", *(unsigned int *)(pbBuf + 0x48), 0);
		check_eq("read cursor untouched", *(unsigned int *)(pbBuf + 0x3c), 2);

		/* [3b] write cursor == read cursor: ring exhausted, clear +0x48. */
		*(unsigned int *)(pbBuf + 0x48) = ToU32(evtA);
		*(unsigned int *)(pbBuf + 0x38) = 3;
		*(unsigned int *)(pbBuf + 0x3c) = 3;
		pb->AdvanceToNextFillEvent();
		check_eq("+0x48 cleared (ring exhausted)", *(unsigned int *)(pbBuf + 0x48), 0);

		/* [3c] normal advance: ring[readIdx] == 7 -> indexArray[7] == evtB. */
		*(unsigned int *)(pbBuf + 0x48) = ToU32(evtA);
		*(unsigned int *)(pbBuf + 0x38) = 9;
		*(unsigned int *)(pbBuf + 0x3c) = 4;
		ring[4] = 7;
		pb->AdvanceToNextFillEvent();
		check_eq("read cursor advanced to 5", *(unsigned int *)(pbBuf + 0x3c), 5);
		check_eq("+0x48 == indexArray[ring[4]] == evtB", *(unsigned int *)(pbBuf + 0x48), ToU32(evtB));
	}

	printf("[4] HandleAdvanceCancelledEvent\n");
	{
		unsigned int fillCarryBefore = cb->fillCarry;
		/* ReturnUnusedFillBytes() (managers.cpp) decrements
		 * availableReadBytes (+0x20), NOT availableFillBytes (+0x24) --
		 * confirmed real (both this test's first draft and a plain
		 * reading of the field names got this backwards). */
		unsigned int availBefore = cb->availableReadBytes;

		/* [4a] event not in state 3: no-op. */
		*(unsigned int *)((unsigned char *)evtA + 0x8) = 2;
		*(unsigned int *)((unsigned char *)evtA + 0x44) = 10;
		pb->HandleAdvanceCancelledEvent(evtA);
		check_eq("state!=3: +0x44 left untouched", *(unsigned int *)((unsigned char *)evtA + 0x44), 10);
		check_eq("state!=3: fillCarry untouched", cb->fillCarry, fillCarryBefore);

		/* [4b] event in state 3, remainingFrames == 0: no-op. */
		*(unsigned int *)((unsigned char *)evtA + 0x8) = 3;
		*(unsigned int *)((unsigned char *)evtA + 0x44) = 0;
		pb->HandleAdvanceCancelledEvent(evtA);
		check_eq("remainingFrames==0: fillCarry untouched", cb->fillCarry, fillCarryBefore);

		/* [4c] real path: 10 frames * 4 bytes/frame == 40 bytes returned. */
		((unsigned char *)evtA)[0x1d] = 4;
		*(unsigned int *)((unsigned char *)evtA + 0x44) = 10;
		pb->HandleAdvanceCancelledEvent(evtA);
		check_eq("evtA->+0x44 cleared", *(unsigned int *)((unsigned char *)evtA + 0x44), 0);
		check_eq("fillCarry += 40", cb->fillCarry, fillCarryBefore + 40);
		check_eq("availableReadBytes -= 40", cb->availableReadBytes, availBefore - 40);
	}

	printf("[5] AddEvent\n");
	{
		unsigned short *ring = (unsigned short *)FromU32(*(unsigned int *)(pbBuf + 0x34));

		/* [5a] no current fill event: evtC becomes it directly. */
		*(unsigned int *)(pbBuf + 0x48) = 0;
		pbBuf[0x44] = 0;
		*(unsigned int *)(pbBuf + 0x38) = 3;	/* must stay untouched on this branch */
		pb->AddEvent(evtC);
		check_eq("+0x48 == evtC", *(unsigned int *)(pbBuf + 0x48), ToU32(evtC));
		check_eq("+0x44 flag set", pbBuf[0x44], 1);
		check_eq("write cursor untouched on this branch", *(unsigned int *)(pbBuf + 0x38), 3);
		check_eq("evtC->+0x8 == 1 (queued)", *(unsigned int *)((unsigned char *)evtC + 0x8), 1);
		check_eq("evtC->+0x30 == this", *(unsigned int *)((unsigned char *)evtC + 0x30), ToU32(pb));
		check_eq("evtC->+0x2c mirrors circbuf field00", ((unsigned char *)evtC)[0x2c],
			 *(unsigned int *)(pbBuf + 0x0));

		/* [5b] a fill event already exists: push evtB's own array-id (1)
		 * into the ring at the write cursor, advance it mod capacity. */
		*(unsigned int *)(pbBuf + 0x48) = ToU32(evtA);
		*(unsigned int *)(pbBuf + 0x38) = *(unsigned int *)(pbBuf + 0x40) - 1;	/* capacity-1, exercises wrap */
		pb->AddEvent(evtB);
		check_eq("ring[capacity-1] == evtB's own id (1)", ring[*(unsigned int *)(pbBuf + 0x40) - 1], 1);
		check_eq("write cursor wrapped to 0", *(unsigned int *)(pbBuf + 0x38), 0);
		check_eq("+0x48 untouched on this branch", *(unsigned int *)(pbBuf + 0x48), ToU32(evtA));
		check_eq("evtB->+0x8 == 1 (queued)", *(unsigned int *)((unsigned char *)evtB + 0x8), 1);
	}

	printf("[6] AdvanceReadPosition\n");
	{
		unsigned char *dcmBuf = mmap32(0x1000);
		memset(dcmBuf, 0, 0x1000);
		CSTGDiskCostManager *dcm = (CSTGDiskCostManager *)dcmBuf;
		CSTGDiskCostManager::sInstance = dcm;
		*(unsigned int *)(dcmBuf + 0x14) = 100;	/* starting watermark */

		unsigned int readPosBefore = cb->readPos;
		unsigned int availBefore = cb->availableReadBytes;
		/* Prime availableReadBytes so the embedded AdvanceReadPosition()
		 * call itself has real, non-zero, non-clamped behavior. */
		cb->availableReadBytes = 1000;
		availBefore = 1000;

		/* [6a] updateWaterMarks == false: only the embedded call runs. */
		pb->AdvanceReadPosition(50, false);
		check_eq("readPos advanced by 50", cb->readPos, readPosBefore + 50);
		check_eq("availableReadBytes -= 50", cb->availableReadBytes, availBefore - 50);
		check_eq("watermark untouched (flag false)", *(unsigned int *)(dcmBuf + 0x14), 100);

		/* [6b] updateWaterMarks == true, new availableReadBytes (950)
		 * EXCEEDS the current watermark (100): watermark bumped up. */
		pb->AdvanceReadPosition(0, true);
		check_eq("watermark bumped to new availableReadBytes",
			 *(unsigned int *)(dcmBuf + 0x14), cb->availableReadBytes);

		/* [6c] watermark already above the (unchanged) available count:
		 * no update (plain "keep the max" idiom). */
		*(unsigned int *)(dcmBuf + 0x14) = 999999;
		pb->AdvanceReadPosition(0, true);
		check_eq("watermark NOT lowered", *(unsigned int *)(dcmBuf + 0x14), 999999);
	}

	printf("[7] RemoveEvent -- batch 25\n");
	{
		unsigned int wcBefore = TSTGArrayManager<CSTGPlaybackEvent>::sInstance->writeCursor;
		unsigned int *bucket = (unsigned int *)FromU32(TSTGArrayManager<CSTGPlaybackEvent>::sInstance->bucketArray);
		unsigned int fillCarryBefore = cb->fillCarry;

		/* [7a] evtA->+0x16 != 0: early return, no side effects at all. */
		((unsigned char *)evtA)[0x16] = 1;
		*(unsigned int *)((unsigned char *)evtA + 0x44) = 999;
		pb->RemoveEvent(evtA);
		check_eq("+0x16!=0: fillCarry untouched", cb->fillCarry, fillCarryBefore);
		check_eq("+0x16!=0: writeCursor untouched", TSTGArrayManager<CSTGPlaybackEvent>::sInstance->writeCursor, wcBefore);

		/* [7b] real removal: evtA is also the current read event
		 * (this->+0x4c), evtA->+0x44/+0x1d combine for the reclaimed
		 * byte count, and evtA->+0x10 must SURVIVE the Reset() call
		 * that clears everything else (the save/restore quirk). */
		((unsigned char *)evtA)[0x16] = 0;
		*(unsigned int *)((unsigned char *)evtA + 0x44) = 10;	/* remainingFrames */
		((unsigned char *)evtA)[0x1d] = 4;			/* bytesPerFrame */
		*(unsigned int *)((unsigned char *)evtA + 0x10) = 0xdeadbeef;
		*(unsigned int *)(pbBuf + 0x4c) = ToU32(evtA);		/* current read event == evtA */

		pb->RemoveEvent(evtA);

		check_eq("fillCarry += 40 (10 frames * 4 bytes)", cb->fillCarry, fillCarryBefore + 40);
		check_eq("this->+0x4c cleared (was evtA)", *(unsigned int *)(pbBuf + 0x4c), 0);
		check_eq("evtA fully Reset() (e.g. +0x44 cleared)", *(unsigned int *)((unsigned char *)evtA + 0x44), 0);
		check_eq("evtA->+0x10 SURVIVES the Reset() call (save/restore quirk)",
			 *(unsigned int *)((unsigned char *)evtA + 0x10), 0xdeadbeef);
		check_eq("evtA pushed onto the free list at the old writeCursor", bucket[wcBefore], ToU32(evtA));
		check_eq("writeCursor advanced by 1", TSTGArrayManager<CSTGPlaybackEvent>::sInstance->writeCursor, wcBefore + 1);

		/* [7c] this->+0x4c did NOT match the removed event: left alone. */
		*(unsigned int *)(pbBuf + 0x4c) = ToU32(evtC);
		((unsigned char *)evtB)[0x16] = 0;
		((unsigned char *)evtB)[0x1d] = 1;
		*(unsigned int *)((unsigned char *)evtB + 0x44) = 0;
		pb->RemoveEvent(evtB);
		check_eq("this->+0x4c untouched (didn't match evtB)", *(unsigned int *)(pbBuf + 0x4c), ToU32(evtC));
	}

	printf("[8] EventFileError -- batch 25\n");
	{
		unsigned short *fillRing = (unsigned short *)FromU32(*(unsigned int *)(pbBuf + 0x34));
		unsigned int *indexArray = (unsigned int *)FromU32(TSTGArrayManager<CSTGPlaybackEvent>::sInstance->indexArray);
		indexArray[9] = ToU32(evtB);

		/* [8a] event is NOT the current fill event: no
		 * AdvanceToNextFillEvent side effect, straight to removal. */
		*(unsigned int *)(pbBuf + 0x48) = ToU32(evtC);	/* current fill event != evtA */
		((unsigned char *)evtA)[0x16] = 0;
		((unsigned char *)evtA)[0x1d] = 2;
		*(unsigned int *)((unsigned char *)evtA + 0x44) = 0;
		pb->EventFileError(evtA);
		check_eq("current fill event untouched (didn't match evtA)",
			 *(unsigned int *)(pbBuf + 0x48), ToU32(evtC));

		/* [8b] event IS the current fill event: the inline "advance to
		 * next fill event" prelude runs first (ring[readIdx]==9 ->
		 * indexArray[9]==evtB becomes the new +0x48), THEN the shared
		 * removal logic still runs on the ORIGINAL event argument
		 * (evtC), not the newly-advanced one. */
		*(unsigned int *)(pbBuf + 0x48) = ToU32(evtC);
		*(unsigned int *)(pbBuf + 0x38) = 20;	/* writeCursor != readCursor -> real advance */
		*(unsigned int *)(pbBuf + 0x3c) = 6;
		fillRing[6] = 9;
		((unsigned char *)evtC)[0x16] = 0;
		((unsigned char *)evtC)[0x1d] = 3;
		*(unsigned int *)((unsigned char *)evtC + 0x44) = 5;
		pb->EventFileError(evtC);
		check_eq("advance-prelude ran: +0x48 == evtB now", *(unsigned int *)(pbBuf + 0x48), ToU32(evtB));
		check_eq("advance-prelude ran: read cursor advanced to 7", *(unsigned int *)(pbBuf + 0x3c), 7);
		check_eq("evtC (the ORIGINAL argument) still got Reset()", *(unsigned int *)((unsigned char *)evtC + 0x44), 0);

		/* [8c] event->+0x16 != 0: early return (no removal at all),
		 * even when it IS the current fill event -- but the prelude
		 * still runs first (it's checked independently, before the
		 * +0x16 guard). */
		*(unsigned int *)(pbBuf + 0x48) = ToU32(evtB);
		((unsigned char *)evtB)[0x16] = 1;
		unsigned int fillCarrySnapshot = cb->fillCarry;
		pb->EventFileError(evtB);
		check_eq("+0x16!=0: fillCarry untouched by the removal step", cb->fillCarry, fillCarrySnapshot);
	}

	printf("[9] HandleFileClosed -- integration with the real RemoveEvent()\n");
	{
		CSTGAudioEvent *baseA = (CSTGAudioEvent *)evtA;
		*(unsigned int *)((unsigned char *)evtA + 0x30) = ToU32(pb);	/* owner back-ref */
		((unsigned char *)evtA)[0x16] = 0;

		/* [9a] state != 3: no-op (RemoveEvent not reached). */
		baseA->field8 = 2;
		unsigned int wcBefore = TSTGArrayManager<CSTGPlaybackEvent>::sInstance->writeCursor;
		evtA->HandleFileClosed();
		check_eq("state!=3: writeCursor untouched", TSTGArrayManager<CSTGPlaybackEvent>::sInstance->writeCursor, wcBefore);

		/* [9b] state == 3: dispatches to the real RemoveEvent(), which
		 * pushes evtA onto the free list. */
		baseA->field8 = 3;
		*(unsigned int *)((unsigned char *)evtA + 0x44) = 0;
		evtA->HandleFileClosed();
		check_eq("state==3: writeCursor advanced (real RemoveEvent ran)",
			 TSTGArrayManager<CSTGPlaybackEvent>::sInstance->writeCursor, wcBefore + 1);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("All checks passed.\n");
	return 0;
}
