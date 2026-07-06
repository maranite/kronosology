// SPDX-License-Identifier: GPL-2.0
/*
 * test_managers.cpp  -  host-side known-answer tests for the batch of small
 * manager constructors in src/engine/managers.cpp (Stage 3, see
 * MASTER_REFERENCE.md sec 10.13/10.14 for the confirmed construction table
 * these classes come from).
 *
 * Each test poisons the object's memory with a non-zero pattern before
 * construction, so a field the constructor is SUPPOSED to leave untouched
 * would show up as still-poisoned (0xcc) rather than accidentally reading
 * as zero and passing by coincidence.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>
#include <sys/mman.h>
#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_bank_memory.h"

/* CSTGAudioEvent's real ctor lives in engine_init.cpp (not linked here) --
 * this file's own new tests for CSTGStreamingEvent (sec 10.158) only need
 * the base sub-object's memory to be sane, not CSTGAudioEvent's own real
 * semantics (already covered by test_engine_init.cpp). Trivial
 * link-satisfying mock, same treatment as CSTGToneAdjustDescriptor::
 * InitializeCommonToneAdjustDescriptors() above. */
CSTGAudioEvent::CSTGAudioEvent() {}
/* _ZTV18CSTGStreamingEvent's own real storage lives in bar2_stubs.cpp (not
 * linked here) -- this file needs its own local definition to satisfy the
 * `extern "C"` declaration in oa_engine_init.h, same treatment
 * test_engine_init.cpp already uses for _ZTV14CSTGAudioEvent/
 * _ZTV17CSTGPlaybackEvent/_ZTV15CSTGRecordEvent. */
unsigned char _ZTV18CSTGStreamingEvent[40];

/* Real kernel-only APIs (RTAI mutex wrappers) and a not-yet-reconstructed
 * static method (CSTGToneAdjustDescriptor) -- mocked here purely so
 * CPowerOffTimer/CSTGVoiceModelManager's constructors link on the host,
 * same treatment as __kmalloc/kfree in test_new_delete.cpp. */
static int g_mutexInitCalls;
extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void) { return 24; }
extern "C" void *rtwrap_malloc(unsigned int size) { return malloc(size); }
extern "C" void rtwrap_pthread_mutex_init(void *, void *) { g_mutexInitCalls++; }
void CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors() { }

/* Sec 10.147: ~CSTGVoiceAllocator() is now real and calls these two --
 * link-satisfying only (this file's own tests never invoke a typed
 * `delete`/destructor on any of these classes, matching every other
 * test here's `delete[]`-on-raw-buffer convention, see its own class
 * comment above). */
extern "C" void rtwrap_pthread_mutex_destroy(void *) { }
extern "C" void rtwrap_free(void *) { }
/* Sec 10.147/10.148: ~CSTGMessageProcessor() calls the now-real
 * ~CEffectorDatabase() (managers.cpp) -- test [22] below exercises it
 * directly (this file never constructs one via ~CSTGMessageProcessor()
 * itself, see test [21]'s own header comment). */

/* Link-satisfying mocks for sec 10.144's new small `Initialize()`/
 * `ProcessCommands()` bodies -- this file doesn't link global.cpp, so
 * ResolveActivePerformanceVarsManagerRaw() needs its own stub here (a
 * null "no active manager" default is fine: nothing in this file
 * exercises CSTGPerformance::IsCurrentlyActive()). */
unsigned char *ResolveActivePerformanceVarsManagerRaw() { return 0; }
/* Sec 10.148: CSTGCDWorker_InitializeBuffer() is now real (managers.cpp)
 * and calls __kmalloc directly -- test [23] below calls it and checks
 * the real (size, flags) arguments plus the returned pointer. */
static int g_kmallocCalls;
static unsigned long g_lastKmallocSize;
static unsigned int g_lastKmallocFlags;
static void *g_lastKmallocReturn;
extern "C" void *__kmalloc(unsigned long size, unsigned int flags)
{
	g_kmallocCalls++;
	g_lastKmallocSize = size;
	g_lastKmallocFlags = flags;
	g_lastKmallocReturn = malloc(size);
	return g_lastKmallocReturn;
}
void CSTGHDRManager::ProcessPlaybackCommands() { }
void CSTGHDRManager::ProcessRecordCommands() { }
void CSTGHDRManager::ProcessSamplerCommands() { }

/* CSTGVoiceAllocator's recursive-mutex setup (see oa_engine.h's
 * CSTGVoiceAllocator comment) -- mocked purely for host linking, with a
 * call counter so the test can confirm each real step actually ran. */
static int g_mutexattrCalls;
extern "C" void rtwrap_pthread_mutexattr_init(void *) { g_mutexattrCalls++; }
extern "C" int  get_pthread_recursive_attr_constant(void) { return 1; }
static int g_condInitCalls;
extern "C" unsigned int get_sizeof_rtwrap_pthread_cond(void) { return 24; }
extern "C" void rtwrap_pthread_cond_init(void *, void *) { g_condInitCalls++; }
/* CSTGAudioManager has a real vtable (confirmed) and a NOT-reconstructed
 * destructor body (see oa_engine.h) -- a definition is required here
 * purely so its vtable links; this test never calls it (matches every
 * other test in this file: `delete[]` on the raw byte buffer, not the
 * typed object). */
CSTGAudioManager::~CSTGAudioManager() { }
extern "C" void rtwrap_pthread_mutexattr_settype(void *, int) { g_mutexattrCalls++; }
extern "C" void rtwrap_pthread_mutexattr_destroy(void *) { g_mutexattrCalls++; }

/* CSTGVoiceAllocator::EmergencyFreeVoiceList() is real now (sec 10.149,
 * see managers.cpp) -- its own confirmed-real, deliberately deferred
 * dependencies (FreeVoice/DoPendingMoveVoices) plus the rtwrap_*
 * mutex lock/unlock pair are mocked here with counters so test [24]
 * below can confirm each real step actually ran. */
static int g_mutexLockCalls, g_mutexUnlockCalls;
extern "C" void rtwrap_pthread_mutex_lock(void *) { g_mutexLockCalls++; }
extern "C" void rtwrap_pthread_mutex_unlock(void *) { g_mutexUnlockCalls++; }
static int g_freeVoiceCalls;
static void *g_lastFreeVoiceArg;
void CSTGVoiceAllocator::FreeVoice(CSTGVoice *voice)
{
	g_freeVoiceCalls++;
	g_lastFreeVoiceArg = (void *)voice;
}
static int g_doPendingMoveVoicesCalls;
void CSTGVoiceAllocator::DoPendingMoveVoices() { g_doPendingMoveVoicesCalls++; }

/* STGAPILR2IndivToPhysBusId's own real content is now homed directly in
 * managers.cpp (sec 10.132), linked into this binary directly -- no
 * local mock needed. gAllPlusHeadroom/gAllMinusHeadroom are poisoned to
 * non-zero/non-obvious values here so the test can tell "the constructor
 * actually wrote this" from "it happened to already look right". */
float gAllPlusHeadroom[4]  = { -99.0f, -99.0f, -99.0f, -99.0f };
float gAllMinusHeadroom[4] = {  99.0f,  99.0f,  99.0f,  99.0f };

static int g_fail;

static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	if (got == want) {
		printf("  ok    %-55s 0x%x\n", label, got);
		return;
	}
	printf("  FAIL  %-55s got=0x%x want=0x%x\n", label, got, want);
	g_fail++;
}

static void check_float(const char *label, float got, float want)
{
	if (got == want) {
		printf("  ok    %-55s %g\n", label, got);
		return;
	}
	printf("  FAIL  %-55s got=%g want=%g\n", label, got, want);
	g_fail++;
}

template <typename T>
static unsigned char *poison_and_construct(T **outObj)
{
	unsigned char *buf = new unsigned char[sizeof(T)];
	memset(buf, 0xcc, sizeof(T));
	*outObj = new (buf) T();
	return buf;
}

/* Counting override of the GLOBAL operator delete[] -- a pure spy, used
 * by test [22] below to confirm ~CEffectorDatabase()'s real conditional
 * `operator delete[]` call actually fires/doesn't fire. Deliberately
 * does NOT call the real `free()`: every other `delete[]` in this file
 * (poison_and_construct's own buffers, etc.) now leaks instead of
 * freeing, which is harmless in this short-lived test binary (the OS
 * reclaims everything on exit) and avoids a real crash class this test
 * hit directly while being written -- `arr` in test [22] is deliberately
 * a `mmap32()`-backed buffer (a real 32-bit-range address, matching the
 * real target's packed 32-bit pointer fields, this project's established
 * pattern e.g. test_engine_init.cpp), but `arr`'s own allocator is
 * `mmap`, not `malloc` -- calling glibc's `free()` on an `mmap`-backed
 * pointer is its own separate undefined behavior. A pure counting spy
 * sidesteps both hazards at once. */
static int g_deleteArrayCalls;
static void *g_lastDeletedArrayPtr;
void operator delete[](void *p) noexcept
{
	g_deleteArrayCalls++;
	g_lastDeletedArrayPtr = p;
}

extern "C" unsigned int CSTGCDWorker_InitializeBuffer(void *worker);

int main(void)
{
	printf("Stage-3 small-manager constructor known-answer test\n");
	printf("=====================================================\n");

	printf("[1] CSTGDiskCostManager: sInstance only, no field writes\n");
	CSTGDiskCostManager *dcm;
	unsigned char *dcmBuf = poison_and_construct(&dcm);
	check_eq("dcm == sInstance", (CSTGDiskCostManager*)dcm == CSTGDiskCostManager::sInstance, 1);
	/* every byte should still be poisoned -- the real ctor touches nothing */
	bool allPoisoned = true;
	for (unsigned int i = 0; i < sizeof(CSTGDiskCostManager); i++)
		if (dcmBuf[i] != 0xcc) allPoisoned = false;
	check_eq("all 72 bytes still poisoned (ctor writes nothing but sInstance)", allPoisoned, 1);
	delete[] dcmBuf;

	printf("[2] CSTGSamplingDaemon: zeroes +0x00/+0x04/+0x08\n");
	CSTGSamplingDaemon *sd;
	unsigned char *sdBuf = poison_and_construct(&sd);
	check_eq("+0x00", *(unsigned int *)(sdBuf+0x00), 0);
	check_eq("+0x04", *(unsigned int *)(sdBuf+0x04), 0);
	check_eq("+0x08", *(unsigned int *)(sdBuf+0x08), 0);
	check_eq("+0x0c still poisoned (untouched)", *(unsigned int *)(sdBuf+0x0c), 0xcccccccc);
	delete[] sdBuf;

	printf("[3] CSTGHDRFileReader: zeroes +0x00/+0x04/+0x08/+0x10\n");
	CSTGHDRFileReader *hfr;
	unsigned char *hfrBuf = poison_and_construct(&hfr);
	check_eq("+0x00", *(unsigned int *)(hfrBuf+0x00), 0);
	check_eq("+0x10", *(unsigned int *)(hfrBuf+0x10), 0);
	check_eq("+0x14 still poisoned", *(unsigned int *)(hfrBuf+0x14), 0xcccccccc);
	delete[] hfrBuf;

	printf("[4] CSTGHDRFileWriter: zeroes +0x00/+0x04/+0x08/+0x10 -- NOT +0x0c..+0x0f\n");
	CSTGHDRFileWriter *hfw;
	unsigned char *hfwBuf = poison_and_construct(&hfw);
	check_eq("+0x00", *(unsigned int *)(hfwBuf+0x00), 0);
	check_eq("+0x04", *(unsigned int *)(hfwBuf+0x04), 0);
	check_eq("+0x08", *(unsigned int *)(hfwBuf+0x08), 0);
	check_eq("+0x10", *(unsigned int *)(hfwBuf+0x10), 0);
	check_eq("+0x0c still poisoned (a real gap -- ctor does NOT touch it)",
		 *(unsigned int *)(hfwBuf+0x0c), 0xcccccccc);
	delete[] hfwBuf;

	printf("[5] CSTGStreamingFileReader: +0x10 = 0x8000, not zero\n");
	CSTGStreamingFileReader *sfr;
	unsigned char *sfrBuf = poison_and_construct(&sfr);
	check_eq("+0x10 == 0x8000", *(unsigned int *)(sfrBuf+0x10), 0x8000);
	check_eq("+0x18", *(unsigned int *)(sfrBuf+0x18), 0);
	check_eq("+0x1c", *(unsigned int *)(sfrBuf+0x1c), 0);
	check_eq("+0x20 still poisoned", *(unsigned int *)(sfrBuf+0x20), 0xcccccccc);
	delete[] sfrBuf;

	printf("[6] CSTGFileCloser: zeroes +0x00/+0x04/+0x08/+0x10/+0x14/+0x18\n");
	CSTGFileCloser *fc;
	unsigned char *fcBuf = poison_and_construct(&fc);
	check_eq("+0x14", *(unsigned int *)(fcBuf+0x14), 0);
	check_eq("+0x18", *(unsigned int *)(fcBuf+0x18), 0);
	check_eq("+0x1c still poisoned", *(unsigned int *)(fcBuf+0x1c), 0xcccccccc);
	delete[] fcBuf;

	printf("[7] CSTGMetronome: confirmed literals (0.25f x3, 0xac4, byte flags)\n");
	CSTGMetronome *met;
	unsigned char *metBuf = poison_and_construct(&met);
	check_eq("+0x00 poison ANDed with 0xfa (0xcc&0xfa=0xc8)", metBuf[0x00], 0xcc & 0xfa);
	check_eq("+0x0c == 0x30", metBuf[0x0c], 0x30);
	check_eq("+0x0d == 0x00", metBuf[0x0d], 0x00);
	check_eq("+0x14 == 0", *(unsigned int *)(metBuf+0x14), 0);
	check_float("+0x10 == 0.25f", *(float *)(metBuf+0x10), 0.25f);
	check_float("+0x18 == 0.25f", *(float *)(metBuf+0x18), 0.25f);
	check_float("+0x1c == 0.25f", *(float *)(metBuf+0x1c), 0.25f);
	check_eq("+0x20 == 0xac4", *(unsigned int *)(metBuf+0x20), 0xac4);
	check_eq("+0x24 == 1", metBuf[0x24], 1);
	check_eq("+0x28 == 0xffffffff", *(unsigned int *)(metBuf+0x28), 0xffffffff);
	delete[] metBuf;

	printf("[8] CSTGTempoUtils: zeroes most fields, but with real confirmed gaps\n");
	CSTGTempoUtils *tu;
	unsigned char *tuBuf = poison_and_construct(&tu);
	/* Confirmed touched byte ranges (mix of byte/dword writes -- not one
	 * uniform sweep, and NOT the entire object: bytes 2-3, 10-11, 25-27,
	 * 41-43, 53-55 are real gaps the constructor never writes). */
	static const struct { unsigned int off, len; } touched[] = {
		{0x00,2}, {0x04,4}, {0x08,2}, {0x0c,4}, {0x10,4}, {0x14,4},
		{0x18,1}, {0x1c,4}, {0x20,4}, {0x24,4}, {0x28,1}, {0x2c,4},
		{0x30,4}, {0x34,1}, {0x38,4},
	};
	bool touchedOk = true;
	for (unsigned int t = 0; t < sizeof(touched)/sizeof(touched[0]); t++)
		for (unsigned int i = 0; i < touched[t].len; i++)
			if (tuBuf[touched[t].off + i] != 0) touchedOk = false;
	check_eq("every confirmed-touched byte is zero", touchedOk, 1);
	static const unsigned int gaps[] = {2, 10, 25, 41, 53};
	bool gapsStillPoisoned = true;
	for (unsigned int g = 0; g < sizeof(gaps)/sizeof(gaps[0]); g++)
		if (tuBuf[gaps[g]] != 0xcc) gapsStillPoisoned = false;
	check_eq("confirmed gap bytes (2,10,25,41,53) still poisoned", gapsStillPoisoned, 1);
	delete[] tuBuf;

	printf("[9] CSTGCDWorker: zeroes header + ring-buffer control, NOT capacity\n");
	CSTGCDWorker *cdw;
	unsigned char *cdwBuf = poison_and_construct(&cdw);
	check_eq("+0x000", *(unsigned int *)(cdwBuf+0x000), 0);
	check_eq("+0x004 (byte)", cdwBuf[0x004], 0);
	check_eq("+0x00c", *(unsigned int *)(cdwBuf+0x00c), 0);
	check_eq("+0x010", *(unsigned int *)(cdwBuf+0x010), 0);
	check_eq("+0x020", *(unsigned int *)(cdwBuf+0x020), 0);
	check_eq("+0x224 (ring base)", *(unsigned int *)(cdwBuf+0x224), 0);
	check_eq("+0x228 (ring write-idx)", *(unsigned int *)(cdwBuf+0x228), 0);
	check_eq("+0x22c (ring read-idx)", *(unsigned int *)(cdwBuf+0x22c), 0);
	check_eq("+0x230", *(unsigned int *)(cdwBuf+0x230), 0);
	check_eq("+0x234 (capacity) still poisoned -- confirmed NOT ctor-set",
		 *(unsigned int *)(cdwBuf+0x234), 0xcccccccc);
	delete[] cdwBuf;

	printf("[10] CSTGFileOpener: header + 32 slots + ring control, NOT capacity\n");
	CSTGFileOpener *fo;
	unsigned char *foBuf = poison_and_construct(&fo);
	check_eq("+0x00 (header)", *(unsigned int *)(foBuf+0x00), 0);
	check_eq("+0x04 (header)", *(unsigned int *)(foBuf+0x04), 0);
	check_eq("+0x08 (header)", *(unsigned int *)(foBuf+0x08), 0);
	check_eq("+0x0c still poisoned", *(unsigned int *)(foBuf+0x0c), 0xcccccccc);
	bool allSlotsOk = true;
	for (int i = 0; i < 32; i++) {
		unsigned char *slot = foBuf + 0x10 + i * 0x10;
		if (*(unsigned int *)(slot+0x0) != 0) allSlotsOk = false;
		if (*(unsigned int *)(slot+0x4) != 0) allSlotsOk = false;
		if (*(unsigned int *)(slot+0x8) != 0) allSlotsOk = false;
		if (*(unsigned int *)(slot+0xc) != 0xcccccccc) allSlotsOk = false;
	}
	check_eq("all 32 slots: +0/+4/+8 zeroed, +0xc still poisoned", allSlotsOk, 1);
	check_eq("+0x210 (ring base)", *(unsigned int *)(foBuf+0x210), 0);
	check_eq("+0x214 (ring write-idx)", *(unsigned int *)(foBuf+0x214), 0);
	check_eq("+0x218 (ring read-idx)", *(unsigned int *)(foBuf+0x218), 0);
	check_eq("+0x21c (capacity) still poisoned -- confirmed NOT ctor-set",
		 *(unsigned int *)(foBuf+0x21c), 0xcccccccc);
	check_eq("sizeof(CSTGFileOpener) matches confirmed 544-byte total size",
		 sizeof(CSTGFileOpener), 544);
	delete[] foBuf;

	printf("[11] CPowerOffTimer: allocates+inits a real mutex at +0x18\n");
	CPowerOffTimer *pot;
	int mutexCallsBefore = g_mutexInitCalls;
	unsigned char *potBuf = poison_and_construct(&pot);
	check_eq("+0x00 (byte)", potBuf[0x00], 0);
	check_eq("+0x14", *(unsigned int *)(potBuf+0x14), 0);
	/* Read as a 32-bit value, not a native `void*` -- see the constructor
	 * in managers.cpp for why (the confirmed field is 4 bytes on the real
	 * 32-bit target; a native 8-byte host pointer wouldn't fit at all in
	 * this tightly-confirmed 28-byte object, and did overflow it before
	 * this fix, caught by -Warray-bounds). Since the low 32 bits of a
	 * 64-bit host pointer aren't independently freeable, this test
	 * deliberately doesn't round-trip through free() -- it's a short-lived
	 * test process, and the point here is the field gets written and
	 * mutex_init runs, not exercising a full host-side malloc/free cycle. */
	unsigned int mutexHandle = *(unsigned int *)(potBuf+0x18);
	check_eq("+0x18 (mutex handle) is non-zero", mutexHandle != 0, 1);
	check_eq("rtwrap_pthread_mutex_init was called once", g_mutexInitCalls - mutexCallsBefore, 1);
	check_eq("sizeof(CPowerOffTimer) matches confirmed 28-byte total size",
		 sizeof(CPowerOffTimer), 28);
	delete[] potBuf;

	printf("[12] CSTGAudioDriverInterfaceKorgUsb: channel counts + callback registration\n");
	/*
	 * Named-member access here, not raw offsets into a poisoned buffer:
	 * this class has a real inherited vtable pointer, 4 bytes on the
	 * confirmed 32-bit target but 8 bytes on this 64-bit host build, so
	 * the confirmed target-relative offsets (+0x38/+0x3c/+0x40/+0x44)
	 * don't land on the same fields here -- see the class comment in
	 * oa_engine.h. What's actually being checked is the confirmed VALUES
	 * each field gets, which are build-independent; the exact byte
	 * offsets are a target-build-only fact, not host-testable directly.
	 */
	CSTGAudioDriverInterfaceKorgUsb adi;
	check_eq("channelsIn == 6", adi.channelsIn, 6);
	check_eq("channelsOut == 6", adi.channelsOut, 6);
	check_eq("selfPtr == this (self-pointer)", adi.selfPtr == (void *)&adi, 1);
	check_eq("callbackFnPtr is non-null", adi.callbackFnPtr != nullptr, 1);

	printf("[13] CSTGVoiceModelManager: two big AllocAligned pools + zeroed range\n");
	CSTGVoiceModelManager *vmm;
	unsigned char *vmmBuf = poison_and_construct(&vmm);
	check_eq("+0x00 (pool ptr) is set (non-poison)",
		 *(unsigned int *)(vmmBuf+0x00) != 0xcccccccc, 1);
	check_eq("+0x04 (pool ptr) is set (non-poison)",
		 *(unsigned int *)(vmmBuf+0x04) != 0xcccccccc, 1);
	bool zeroRangeOk = true;
	for (unsigned int off = 0x08; off <= 0x2c; off += 4)
		if (*(unsigned int *)(vmmBuf+off) != 0) zeroRangeOk = false;
	check_eq("+0x08..+0x2c all zeroed", zeroRangeOk, 1);
	check_eq("+0x58 (word) == 0", *(unsigned short *)(vmmBuf+0x58), 0);
	check_eq("+0x30 still poisoned (a real gap)", *(unsigned int *)(vmmBuf+0x30), 0xcccccccc);
	check_eq("sizeof(CSTGVoiceModelManager) matches confirmed 92-byte total size",
		 sizeof(CSTGVoiceModelManager), 92);
	delete[] vmmBuf;

	printf("[14] CLoadBalancer: embedded CEmergencyStealer + zeroed tail range\n");
	CLoadBalancer *lb;
	unsigned char *lbBuf = poison_and_construct(&lb);
	check_eq("+0x00..+0x23 (embedded CEmergencyStealer) untouched by CLoadBalancer's own ctor",
		 *(unsigned int *)(lbBuf+0x00), 0xcccccccc);
	bool tailZeroOk = true;
	for (unsigned int off = 0x24; off <= 0xa0; off += 4)
		if (*(unsigned int *)(lbBuf+off) != 0) tailZeroOk = false;
	check_eq("+0x24..+0xa0 all zeroed", tailZeroOk, 1);
	check_eq("+0xa4 (byte) == 0", lbBuf[0xa4], 0);
	check_eq("sizeof(CLoadBalancer) matches confirmed 168-byte total size",
		 sizeof(CLoadBalancer), 168);
	delete[] lbBuf;

	printf("[15] CSTGMonitorMixer: the smallest manager constructor found so far --\n"
	       "     sInstance = this, nothing else\n");
	CSTGMonitorMixer mm;
	check_eq("sInstance == &mm", (unsigned int)(CSTGMonitorMixer::sInstance == &mm), 1);

	printf("[16] CSTGAudioBusManager: two float literals, a table lookup, and two\n"
	       "     module-global side effects (headroom arrays reset to +-1.0)\n");
	CSTGAudioBusManager abm;
	check_eq("busGainReciprocal == 0.0006666666595265269f",
		 (unsigned int)(abm.busGainReciprocal == 0.0006666666595265269f), 1);
	check_eq("busGainScale == 1500.0f", (unsigned int)(abm.busGainScale == 1500.0f), 1);
	check_eq("physBusIdTableHead == first dword of STGAPILR2IndivToPhysBusId (real value: 44)",
		 (unsigned int)abm.physBusIdTableHead, 44u);
	check_eq("sInstance == &abm", (unsigned int)(CSTGAudioBusManager::sInstance == &abm), 1);
	bool plusOk = gAllPlusHeadroom[0] == 1.0f && gAllPlusHeadroom[1] == 1.0f &&
		      gAllPlusHeadroom[2] == 1.0f && gAllPlusHeadroom[3] == 1.0f;
	bool minusOk = gAllMinusHeadroom[0] == -1.0f && gAllMinusHeadroom[1] == -1.0f &&
		       gAllMinusHeadroom[2] == -1.0f && gAllMinusHeadroom[3] == -1.0f;
	check_eq("gAllPlusHeadroom reset to {1,1,1,1}", (unsigned int)plusOk, 1);
	check_eq("gAllMinusHeadroom reset to {-1,-1,-1,-1}", (unsigned int)minusOk, 1);

	printf("[17] CSTGEffectManager: zeroed head untouched, +0x800 + 198-entry table\n"
	       "     zeroed, a real confirmed gap, then two 120.0f literals + 4 zeroed dwords\n");
	CSTGEffectManager *em;
	unsigned char *emBuf = poison_and_construct(&em);
	bool headStillPoisoned = true;
	for (unsigned int off = 0; off < 0x800; off++)
		if (emBuf[off] != 0xcc) headStillPoisoned = false;
	check_eq("+0x000..+0x7ff still poisoned (untouched by this ctor)",
		 (unsigned int)headStillPoisoned, 1);
	check_eq("+0x800 (zeroedCounter) == 0", *(unsigned int *)(emBuf+0x800), 0);
	bool tableZeroOk = true;
	for (unsigned int off = 0x804; off <= 0xb18; off += 4)
		if (*(unsigned int *)(emBuf+off) != 0) tableZeroOk = false;
	check_eq("+0x804..+0xb1b (198-entry table) all zeroed", (unsigned int)tableZeroOk, 1);
	bool gapStillPoisoned = true;
	for (unsigned int off = 0xb1c; off < 0xb64; off++)
		if (emBuf[off] != 0xcc) gapStillPoisoned = false;
	check_eq("+0xb1c..+0xb63 still poisoned (a real gap)", (unsigned int)gapStillPoisoned, 1);
	check_eq("+0xb64 (defaultTempoA) == 120.0f",
		 (unsigned int)(*(float *)(emBuf+0xb64) == 120.0f), 1);
	check_eq("+0xb68 (defaultTempoB) == 120.0f",
		 (unsigned int)(*(float *)(emBuf+0xb68) == 120.0f), 1);
	bool tailZeroOk2 = *(unsigned int *)(emBuf+0xb6c) == 0 &&
			   *(unsigned int *)(emBuf+0xb70) == 0 &&
			   *(unsigned int *)(emBuf+0xb74) == 0 &&
			   *(unsigned int *)(emBuf+0xb78) == 0;
	check_eq("+0xb6c/+0xb70/+0xb74/+0xb78 all zeroed", (unsigned int)tailZeroOk2, 1);
	check_eq("sInstance == em", (unsigned int)(CSTGEffectManager::sInstance == em), 1);
	check_eq("sizeof(CSTGEffectManager) matches confirmed minimum 2940-byte size",
		 sizeof(CSTGEffectManager), 0xb7c);
	delete[] emBuf;

	printf("[18] CSTGHDRManager: 16 opaque CSTGPlaybackBuffer (untouched by their own\n"
	       "     empty ctor), 16 opaque CSTGMonitorMixerChannel slots (confirmed\n"
	       "     172-vs-192-byte size/stride quirk: channels 0-14 get 3 tail dwords\n"
	       "     zeroed, channel 15 does not) -- this is a PARTIAL reconstruction,\n"
	       "     see oa_engine.h for everything confirmed-but-not-implemented past\n"
	       "     this class's own declared (much smaller than real) size\n");
	CSTGHDRManager *hdr;
	unsigned char *hdrBuf = poison_and_construct(&hdr);

	/* +0x000..+0x003 (_unrecovered_head) and +0x004..+0x583 (playbackBuffers[16],
	 * whose opaque sub-ctor is empty) are both untouched by this ctor. */
	bool headAndBuffersUntouched = true;
	for (unsigned int off = 0; off < 4 + 16 * 88; off++)
		if (hdrBuf[off] != 0xcc) headAndBuffersUntouched = false;
	check_eq("+0x000..+0x583 (head + playbackBuffers[16]) entirely untouched",
		 (unsigned int)headAndBuffersUntouched, 1);

	unsigned int gapBase = 4 + 16 * 88;	/* +0x584 */
	bool gapOk =
		*(unsigned int *)(hdrBuf + gapBase + 0x0c) == 0 &&
		*(unsigned int *)(hdrBuf + gapBase + 0x10) == 0 &&
		*(unsigned int *)(hdrBuf + gapBase + 0x14) == 0;
	check_eq("_unrecovered_gap[0xc]/[0x10]/[0x14] confirmed zeroed", (unsigned int)gapOk, 1);
	bool gapRestPoisoned =
		hdrBuf[gapBase + 0x00] == 0xcc && hdrBuf[gapBase + 0x04] == 0xcc &&
		hdrBuf[gapBase + 0x08] == 0xcc && hdrBuf[gapBase + 0x18] == 0xcc &&
		hdrBuf[gapBase + 0x1c] == 0xcc;
	check_eq("rest of _unrecovered_gap still poisoned (a real partial gap)",
		 (unsigned int)gapRestPoisoned, 1);

	unsigned int channelsBase = gapBase + 0x20;	/* +0x5a4 */
	bool allChannelsOk = true;
	for (int i = 0; i < 16; i++) {
		unsigned char *slot = hdrBuf + channelsBase + i * 0xc0;
		for (unsigned int off = 0; off < 0xac; off++)
			if (slot[off] != 0xcc) allChannelsOk = false;
		if (i < 15) {
			if (*(unsigned int *)(slot + 0xac) != 0) allChannelsOk = false;
			if (*(unsigned int *)(slot + 0xb0) != 0) allChannelsOk = false;
			if (*(unsigned int *)(slot + 0xb4) != 0) allChannelsOk = false;
			if (slot[0xb8] != 0xcc) allChannelsOk = false;	/* confirmed gap, channels 0-14 */
		} else {
			for (unsigned int off = 0xac; off < 0xc0; off++)
				if (slot[off] != 0xcc) allChannelsOk = false;	/* channel 15: NO tail zero at all */
		}
	}
	check_eq("all 16 channel slots match the confirmed 172-vs-192 quirk exactly",
		 (unsigned int)allChannelsOk, 1);

	check_eq("sInstance == hdr", (unsigned int)(CSTGHDRManager::sInstance == hdr), 1);
	check_eq("sizeof(CSTGHDRManager) matches this reconstruction's declared "
		 "(NOT the real ~101KB) size", sizeof(CSTGHDRManager), 0x11a4);
	delete[] hdrBuf;

	printf("[19] CSTGVoiceAllocator: two confirmed self-ref/owner-backref arrays,\n"
	       "     16 opaque CSTGSlotState (untouched by their own empty ctor), a\n"
	       "     real recursive mutex -- PARTIAL reconstruction (~281KB confirmed\n"
	       "     minimum size, two large regions' contents not reconstructed --\n"
	       "     see oa_engine.h)\n");
	int mutexInitCallsBefore = g_mutexInitCalls;
	int mutexattrCallsBefore = g_mutexattrCalls;
	CSTGVoiceAllocator *va;
	unsigned char *vaBuf = poison_and_construct(&va);

	unsigned char *node0 = vaBuf;
	bool node0Ok =
		*(unsigned int *)(node0 + 0x00) == 0 &&
		*(unsigned int *)(node0 + 0x04) == 0 &&
		*(unsigned int *)(node0 + 0x08) == 0 &&
		*(unsigned int *)(node0 + 0x24) == (unsigned int)(unsigned long)node0 &&
		*(unsigned int *)(node0 + 0x1c) == 0 &&
		*(unsigned int *)(node0 + 0x20) == 0 &&
		*(unsigned int *)(node0 + 0x28) == 0 &&
		*(unsigned int *)(node0 + 0x10) == 0 &&
		*(unsigned int *)(node0 + 0x0c) == 0 &&
		*(unsigned short *)(node0 + 0x14) == 0 &&
		*(unsigned short *)(node0 + 0x16) == 0;
	check_eq("selfRefNodes[0]: confirmed fields zeroed + self-pointer at +0x24",
		 (unsigned int)node0Ok, 1);
	unsigned char *node49 = vaBuf + 49 * 0x2c;
	check_eq("selfRefNodes[49]: self-pointer at +0x24 points at itself",
		 (unsigned int)(*(unsigned int *)(node49 + 0x24) == (unsigned int)(unsigned long)node49), 1);
	bool gap1Poisoned = vaBuf[50 * 0x2c] == 0xcc;
	check_eq("_unrecovered_gap1 still poisoned (a real gap)", (unsigned int)gap1Poisoned, 1);

	unsigned char *rec0 = vaBuf + 0x0bb8;
	unsigned int rec0Self = (unsigned int)(unsigned long)rec0;
	bool rec0Ok =
		*(unsigned int *)(rec0 + 0x24) == 0 && *(unsigned int *)(rec0 + 0x20) == 0 &&
		*(unsigned int *)(rec0 + 0x28) == 0 && *(unsigned int *)(rec0 + 0x34) == rec0Self &&
		*(unsigned int *)(rec0 + 0x2c) == 0 && *(unsigned int *)(rec0 + 0x30) == 0 &&
		*(unsigned int *)(rec0 + 0x38) == 0 && *(unsigned int *)(rec0 + 0x44) == rec0Self &&
		*(unsigned int *)(rec0 + 0x3c) == 0 && *(unsigned int *)(rec0 + 0x40) == 0 &&
		*(unsigned int *)(rec0 + 0x48) == 0 && *(unsigned int *)(rec0 + 0x54) == rec0Self &&
		*(unsigned int *)(rec0 + 0x4c) == 0 && *(unsigned int *)(rec0 + 0x50) == 0 &&
		*(unsigned int *)(rec0 + 0x58) == 0 && *(unsigned int *)(rec0 + 0x64) == rec0Self &&
		*(unsigned int *)(rec0 + 0x5c) == 0 && *(unsigned int *)(rec0 + 0x60) == 0 &&
		*(unsigned int *)(rec0 + 0x68) == 0 &&
		*(unsigned int *)(rec0 + 0x08) == 0 && *(unsigned int *)(rec0 + 0x0c) == 0;
	check_eq("ownerBackRefRecords[0]: confirmed fields zeroed + 4 owner back-pointers",
		 (unsigned int)rec0Ok, 1);
	unsigned char *rec399 = vaBuf + 0x0bb8 + 399 * 0x6c;
	check_eq("ownerBackRefRecords[399]: owner back-pointer at +0x34 points at its own base",
		 (unsigned int)(*(unsigned int *)(rec399 + 0x34) == (unsigned int)(unsigned long)rec399), 1);

	unsigned char *slotState0 = vaBuf + 0xb478;
	unsigned char *slotState15 = vaBuf + 0xb478 + 15 * 0x188c;
	bool slotStatesUntouched = slotState0[0] == 0xcc && slotState0[0x188b] == 0xcc &&
				   slotState15[0] == 0xcc && slotState15[0x188b] == 0xcc;
	check_eq("slotStates[0] and [15] entirely untouched (opaque sub-ctor is empty)",
		 (unsigned int)slotStatesUntouched, 1);

	bool bigArrayPoisoned = vaBuf[0x23d38] == 0xcc;
	bool tailPoisoned = vaBuf[0x3a7b8] == 0xcc;
	check_eq("_unrecovered_bigArray still poisoned (not reconstructed)",
		 (unsigned int)bigArrayPoisoned, 1);
	check_eq("_unrecovered_tail still poisoned (not reconstructed)",
		 (unsigned int)tailPoisoned, 1);

	check_eq("requirementsMutex is a real non-zero handle",
		 (unsigned int)(va->requirementsMutex != 0), 1);
	check_eq("rtwrap_pthread_mutex_init called once", g_mutexInitCalls - mutexInitCallsBefore, 1);
	check_eq("mutexattr init/settype/destroy all called (3 calls)",
		 g_mutexattrCalls - mutexattrCallsBefore, 3);
	check_eq("sInstance == va", (unsigned int)(CSTGVoiceAllocator::sInstance == va), 1);
	check_eq("sizeof(CSTGVoiceAllocator) matches the confirmed ~281KB minimum size exactly",
		 sizeof(CSTGVoiceAllocator), 0x44eac);
	delete[] vaBuf;

	printf("[20] CSTGAudioManager: two confirmed mutex+condvar pairs, four trailing\n"
	       "     scalars -- PARTIAL reconstruction (~17.3KB confirmed minimum size,\n"
	       "     one large middle region's contents not reconstructed -- see\n"
	       "     oa_engine.h). Real vtable: accessed via named members only, not raw\n"
	       "     buffer offsets (the vtable pointer's own width differs host-vs-target).\n");
	int mutexInitCallsBefore2 = g_mutexInitCalls;
	int condInitCallsBefore = g_condInitCalls;
	CSTGAudioManager *am = new CSTGAudioManager();
	check_eq("mutexCondFlag1 == 0", am->mutexCondFlag1, 0);
	check_eq("mutex1Handle is a real non-zero handle", (unsigned int)(am->mutex1Handle != 0), 1);
	check_eq("cond1Handle is a real non-zero handle", (unsigned int)(am->cond1Handle != 0), 1);
	check_eq("mutexCondFlag2 == 0", am->mutexCondFlag2, 0);
	check_eq("mutex2Handle is a real non-zero handle", (unsigned int)(am->mutex2Handle != 0), 1);
	check_eq("cond2Handle is a real non-zero handle", (unsigned int)(am->cond2Handle != 0), 1);
	check_eq("mutex1Handle != mutex2Handle (two distinct allocations)",
		 (unsigned int)(am->mutex1Handle != am->mutex2Handle), 1);
	check_eq("rtwrap_pthread_mutex_init called twice", g_mutexInitCalls - mutexInitCallsBefore2, 2);
	check_eq("rtwrap_pthread_cond_init called twice", g_condInitCalls - condInitCallsBefore, 2);
	check_eq("trailingCount == 0x100", am->trailingCount, 0x100);
	check_eq("trailingMask == 0xff", am->trailingMask, 0xff);
	check_float("trailingReciprocal == 0.00390625f (1/256)", am->trailingReciprocal, 0.00390625f);
	check_float("trailingUnity == 1.0f", am->trailingUnity, 1.0f);
	check_eq("sInstance == am", (unsigned int)(CSTGAudioManager::sInstance == am), 1);
	/* sizeof(CSTGAudioManager) includes the compiler-placed vtable pointer,
	 * which is 4 bytes on the confirmed 32-bit target but 8 on this 64-bit
	 * host -- subtracting sizeof(void*) isolates the reconstruction's own
	 * declared member data, which matches the real confirmed size on any
	 * build (the same host/target tolerance as every other vtabled class
	 * in this file). EXTENDED (2026-07-02, was 0x4558) while
	 * reconstructing the real CSTGAudioManager::StartAudioEngine() (see
	 * oa_audio_start.h) -- that method's own confirmed field writes go
	 * past this file's earlier boundary, to +0x456c, a genuine extension
	 * of the known minimum size, not an error in either value. */
	check_eq("sizeof(CSTGAudioManager) - sizeof(void*) matches the confirmed "
		 "0x4570-byte member data size exactly",
		 (unsigned int)(sizeof(CSTGAudioManager) - sizeof(void *)), 0x4570);
	delete am;	/* real typed delete -- exercises the mocked ~CSTGAudioManager() above */

	printf("[21] CSTGMessageProcessor: the most heterogeneous class in this codebase\n"
	       "     (664 relocations) -- PARTIAL reconstruction that implements only\n"
	       "     sInstance (confirmed to be set unusually early) and the confirmed\n"
	       "     minimum size; everything else (3 unsol-msg pairs, ~15 buffers, 14\n"
	       "     MsgHandlers, 198+8 effect/algorithm registrations) is confirmed to\n"
	       "     exist but explicitly not reconstructed -- see oa_engine.h\n");
	CSTGMessageProcessor *mp;
	unsigned char *mpBuf = poison_and_construct(&mp);
	bool mpEntirelyUntouched = true;
	for (unsigned int off = 0; off < sizeof(CSTGMessageProcessor); off++)
		if (mpBuf[off] != 0xcc) mpEntirelyUntouched = false;
	check_eq("entire object still poisoned (this reconstruction touches no member data)",
		 (unsigned int)mpEntirelyUntouched, 1);
	check_eq("sInstance == mp", (unsigned int)(CSTGMessageProcessor::sInstance == mp), 1);
	check_eq("sizeof(CSTGMessageProcessor) matches the confirmed 0x1040-byte "
		 "MINIMUM size (a lower bound, not asserted exact -- see oa_engine.h)",
		 sizeof(CSTGMessageProcessor), 0x1040);
	delete[] mpBuf;

	printf("[22] CEffectorDatabase::~CEffectorDatabase() (sec 10.148)\n");
	{
		/* `+0x0` is a packed 32-bit field on the real target -- `arr`
		 * must itself live in the low 32 bits of the address space, or
		 * the real function's own `(void*)(unsigned long)*(unsigned
		 * int*)this` truncate-then-reinflate round-trip (faithful to
		 * the real target, sec 10.148) recovers a bogus, unrelated
		 * 64-bit address on this host and crashes -- same mmap32()
		 * pattern already established elsewhere (e.g.
		 * test_engine_init.cpp's bankBuf). */
		int callsBefore = g_deleteArrayCalls;
		unsigned char *arr = (unsigned char *)mmap(0, 16, PROT_READ | PROT_WRITE,
							    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
		unsigned char cedb[4];
		*(unsigned int *)cedb = (unsigned int)(unsigned long)arr;
		((CEffectorDatabase *)cedb)->~CEffectorDatabase();
		check_eq("non-null +0x0: operator delete[] called once",
			 (unsigned int)(g_deleteArrayCalls == callsBefore + 1), 1);
		check_eq("non-null +0x0: operator delete[] called on the real pointer",
			 (unsigned int)(g_lastDeletedArrayPtr == arr), 1);

		callsBefore = g_deleteArrayCalls;
		*(unsigned int *)cedb = 0;
		((CEffectorDatabase *)cedb)->~CEffectorDatabase();
		check_eq("null +0x0: operator delete[] NOT called",
			 (unsigned int)(g_deleteArrayCalls == callsBefore), 1);
	}

	printf("\n[23] CSTGCDWorker_InitializeBuffer() (sec 10.148)\n");
	{
		unsigned int ret = CSTGCDWorker_InitializeBuffer((void *)0x12345678u);
		check_eq("__kmalloc called once", (unsigned int)(g_kmallocCalls == 1), 1);
		check_eq("__kmalloc size == 0xa00", (unsigned int)g_lastKmallocSize, 0xa00);
		check_eq("__kmalloc flags == 0xd1 (OA_GFP_KERNEL|__GFP_DMA)",
			 g_lastKmallocFlags, 0xd1);
		check_eq("return value == the __kmalloc'd pointer, truncated to 32 bits",
			 ret, (unsigned int)(unsigned long)g_lastKmallocReturn);
	}

	printf("\n[24] CSTGVoiceAllocator::Initialize() (sec 10.157): three confirmed\n"
	       "     doubly-linked free lists (400+400+50 nodes) + 200 CSTGVoice +\n"
	       "     per-voice 0x4000 buffer\n");
	{
		/*
		 * Every pointer this function stores into a packed 32-bit
		 * field and later reconstitutes (list links, voicePtrs[],
		 * the per-voice +0x60 buffer, and the two CSTGVoiceModelManager
		 * array bases) must itself live in the low 32 bits of the
		 * address space -- same MAP_32BIT precedent as test [22]'s own
		 * `arr` above, needed here for real (not just documented)
		 * since this function actually WALKS these lists, unlike
		 * test [19]'s ctor-only checks which only ever compare
		 * truncated values without dereferencing them.
		 */
		unsigned long arenaSize = 200UL * 0x100 + 200UL * 0x4000 + 0x10000;
		unsigned char *arena = (unsigned char *)mmap(0, arenaSize, PROT_READ | PROT_WRITE,
							      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
		CSTGBankMemory::Initialize(arena, arenaSize);

		unsigned char *arrB = (unsigned char *)mmap(0, 0x1000, PROT_READ | PROT_WRITE,
							     MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
		unsigned char *arrA = (unsigned char *)mmap(0, 0x1000, PROT_READ | PROT_WRITE,
							     MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
		unsigned char modelMgrBuf[92];
		memset(modelMgrBuf, 0, sizeof(modelMgrBuf));
		*(unsigned int *)(modelMgrBuf + 0x0) = (unsigned int)(unsigned long)arrB;
		*(unsigned int *)(modelMgrBuf + 0x4) = (unsigned int)(unsigned long)arrA;
		CSTGVoiceModelManager::sInstance = (CSTGVoiceModelManager *)modelMgrBuf;

		/*
		 * The CSTGVoiceAllocator object itself must ALSO be MAP_32BIT
		 * (Initialize() reconstitutes list-node pointers stored
		 * within itself) -- `poison_and_construct`'s plain heap
		 * `new[]` is not safe to reuse here, so this section builds
		 * its own buffer instead. Zero-initialized (mmap's own
		 * natural zero pages) rather than 0xcc-poisoned: several of
		 * the fields Initialize() reads as PRE-EXISTING state (the
		 * three lists' own head/tail/count triplets) are documented
		 * as a real gap the ctor itself never establishes -- on real
		 * hardware this object's own backing memory is understood to
		 * start zeroed (matching every other CSTGBankMemory-pool-
		 * backed global singleton in this project), so zero is the
		 * realistic precondition, not an arbitrary weakening of the
		 * test.
		 */
		unsigned char *va2Buf = (unsigned char *)mmap(0, sizeof(CSTGVoiceAllocator),
							       PROT_READ | PROT_WRITE,
							       MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
		CSTGVoiceAllocator *va2 = new (va2Buf) CSTGVoiceAllocator();
		va2->Initialize();

		/* ---- list1: ownerBackRefRecords[400], stride 0x6c ---- */
		check_eq("list1 count == 400", *(unsigned int *)(va2Buf + 0x3a7d8), 400u);
		check_eq("list1 head == &ownerBackRefRecords[0]",
			 *(unsigned int *)(va2Buf + 0x3a7d0),
			 (unsigned int)(unsigned long)(va2Buf + 0xbb8));
		check_eq("list1 tail == &ownerBackRefRecords[399]",
			 *(unsigned int *)(va2Buf + 0x3a7d4),
			 (unsigned int)(unsigned long)(va2Buf + 0xbb8 + 399 * 0x6c));
		{
			/* Walk the list head-to-tail; a tail-append build of
			 * 400 sequential nodes must visit them in order
			 * 0..399 (confirmed via each node's own +0x0 idx
			 * field, not just a bare hop count). */
			unsigned char *node = va2Buf + (unsigned long)(*(unsigned int *)(va2Buf + 0x3a7d0) -
									(unsigned int)(unsigned long)va2Buf);
			unsigned int hops = 0;
			bool orderOk = true;
			unsigned int ownerAddr = (unsigned int)(unsigned long)(va2Buf + 0x3a7d0);
			bool ownerOk = true;
			while (node) {
				if (*(unsigned short *)(node + 0x0) != hops) orderOk = false;
				if (*(unsigned int *)(node + 0x58) != ownerAddr) ownerOk = false;
				unsigned int nextU32 = *(unsigned int *)(node + 0x4c);
				hops++;
				if (nextU32 == 0) break;
				node = va2Buf + (nextU32 - (unsigned int)(unsigned long)va2Buf);
			}
			check_eq("list1 walk visits exactly 400 nodes in sequential idx order",
				 hops, 400u);
			check_eq("list1 walk: idx field matches walk order at every node",
				 (unsigned int)orderOk, 1);
			check_eq("list1 walk: owner back-pointer == &list1 head at every node",
				 (unsigned int)ownerOk, 1);
		}

		/* ---- list2: _unrecovered_bigArray[400], stride 0xe8 ---- */
		check_eq("list2 count == 400", *(unsigned int *)(va2Buf + 0x3a7cc), 400u);
		check_eq("list2 head == &_unrecovered_bigArray[0]",
			 *(unsigned int *)(va2Buf + 0x3a7c4),
			 (unsigned int)(unsigned long)(va2Buf + 0x23d38));
		check_eq("list2 tail == &_unrecovered_bigArray[399]",
			 *(unsigned int *)(va2Buf + 0x3a7c8),
			 (unsigned int)(unsigned long)(va2Buf + 0x23d38 + 399 * 0xe8));
		{
			unsigned char *node = va2Buf + (*(unsigned int *)(va2Buf + 0x3a7c4) -
							 (unsigned int)(unsigned long)va2Buf);
			unsigned int hops = 0;
			bool orderOk = true;
			while (node) {
				if (*(unsigned short *)(node + 0x0) != hops) orderOk = false;
				unsigned int nextU32 = *(unsigned int *)(node + 0xd8);
				hops++;
				if (nextU32 == 0) break;
				node = va2Buf + (nextU32 - (unsigned int)(unsigned long)va2Buf);
			}
			check_eq("list2 walk visits exactly 400 nodes in sequential idx order",
				 hops, 400u);
			check_eq("list2 walk: idx field matches walk order at every node",
				 (unsigned int)orderOk, 1);
		}

		/* ---- list3: selfRefNodes[50], stride 0x2c ---- */
		check_eq("list3 count == 50", *(unsigned int *)(va2Buf + 0x3a7c0), 50u);
		unsigned char *node3 = va2Buf + 3 * 0x2c;
		check_eq("list3 node[3].+0xc == arrB + 3*0x1a80",
			 *(unsigned int *)(node3 + 0xc),
			 (unsigned int)((unsigned long)arrB + 3 * 0x1a80));
		check_eq("list3 node[3].+0x10 == arrA + 3*0x3300",
			 *(unsigned int *)(node3 + 0x10),
			 (unsigned int)((unsigned long)arrA + 3 * 0x3300));
		check_eq("list3 node[3].+0x14 idx == 3", *(unsigned short *)(node3 + 0x14), 3);

		/* ---- 200 CSTGVoice + per-voice 0x4000 buffer ---- */
		unsigned int voice0 = va2->voicePtrs[0];
		unsigned int voice199 = va2->voicePtrs[199];
		check_eq("voicePtrs[0] non-null", (unsigned int)(voice0 != 0), 1);
		check_eq("voicePtrs[199] non-null", (unsigned int)(voice199 != 0), 1);
		check_eq("voicePtrs[0] != voicePtrs[199] (distinct allocations)",
			 (unsigned int)(voice0 != voice199), 1);
		unsigned char *v0 = (unsigned char *)(unsigned long)voice0;
		unsigned char *v199 = (unsigned char *)(unsigned long)voice199;
		check_eq("CSTGVoice[0].note (+0x4) == 0", *(unsigned short *)(v0 + 0x4), 0);
		check_eq("CSTGVoice[199].note (+0x4) == 199", *(unsigned short *)(v199 + 0x4), 199);
		check_eq("CSTGVoice[0] self-pointer at +0x98 == itself",
			 *(unsigned int *)(v0 + 0x98), (unsigned int)(unsigned long)v0);
		unsigned int buf0 = *(unsigned int *)(v0 + 0x60);
		unsigned int buf199 = *(unsigned int *)(v199 + 0x60);
		check_eq("CSTGVoice[0]'s +0x60 buffer non-null", (unsigned int)(buf0 != 0), 1);
		check_eq("CSTGVoice[199]'s +0x60 buffer non-null", (unsigned int)(buf199 != 0), 1);
		check_eq("CSTGVoice[0]/[199]'s +0x60 buffers are distinct",
			 (unsigned int)(buf0 != buf199), 1);

		/* ---- trailing zeroed table + scalars ---- */
		check_eq("+0x40b54 table[0][0] == 0", *(unsigned short *)(va2Buf + 0x40b54), 0);
		check_eq("+0x40b54 table[15][127] == 0",
			 *(unsigned short *)(va2Buf + 0x40b54 + 15 * 0x100 + 127 * 2), 0);
		check_eq("+0x41b54 rowFlag[15] == 0", *(unsigned short *)(va2Buf + 0x41b54 + 15 * 2), 0);
		check_eq("+0x44b22 (last trailing word) == 0", *(unsigned short *)(va2Buf + 0x44b22), 0);
	}

	printf("\n[25] CSTGHDRCircularBuffer: all nine methods (sec 10.158)\n");
	{
		unsigned long arenaSize = 64 * 1024;
		unsigned char *arena = (unsigned char *)mmap(0, arenaSize, PROT_READ | PROT_WRITE,
							      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
		CSTGBankMemory::Initialize(arena, arenaSize);

		/* Ctor: all-zero. Plain heap new[] is safe -- none of this
		 * class's own methods reconstitute-and-dereference their own
		 * pointer fields (pure address arithmetic throughout, see the
		 * class comment in oa_engine.h). */
		CSTGHDRCircularBuffer *cb;
		poison_and_construct(&cb);
		check_eq("ctor: field00 == 0", cb->field00, 0);
		check_eq("ctor: flagByte == 0", cb->flagByte, 0);
		check_eq("ctor: bufferBase == 0", cb->bufferBase, 0);
		check_eq("ctor: readPos == 0", cb->readPos, 0);
		check_eq("ctor: fillPos == 0", cb->fillPos, 0);
		check_eq("ctor: bufferEnd == 0", cb->bufferEnd, 0);
		check_eq("ctor: totalSize == 0", cb->totalSize, 0);
		check_eq("ctor: effectiveSize == 0", cb->effectiveSize, 0);
		/* Confirmed real gap: the ctor does NOT zero +0x20/+0x24 (every
		 * other scalar field is zeroed, but these two are only ever
		 * (re)set by Initialize()/Reset()/SetEffectiveSize(), all of
		 * which this class's own real callers always run before first
		 * use) -- still poisoned at 0xcc here, not a bug in this
		 * reconstruction. */
		check_eq("ctor: availableReadBytes still 0xcc (confirmed real gap)",
			 cb->availableReadBytes, 0xcccccccc);
		check_eq("ctor: availableFillBytes still 0xcc (confirmed real gap)",
			 cb->availableFillBytes, 0xcccccccc);
		check_eq("ctor: fillCarry == 0", cb->fillCarry, 0);
		check_eq("ctor: readCarry == 0", cb->readCarry, 0);

		cb->Initialize(0x100, true, 0x8);
		check_eq("Initialize: totalSize == 0x100", cb->totalSize, 0x100);
		check_eq("Initialize: effectiveSize == 0x100", cb->effectiveSize, 0x100);
		check_eq("Initialize: flagByte == 1", cb->flagByte, 1);
		check_eq("Initialize: bufferBase non-null", (unsigned int)(cb->bufferBase != 0), 1);
		check_eq("Initialize: readPos == bufferBase", cb->readPos, cb->bufferBase);
		check_eq("Initialize: fillPos == bufferBase", cb->fillPos, cb->bufferBase);
		check_eq("Initialize: bufferEnd == bufferBase+0x100", cb->bufferEnd, cb->bufferBase + 0x100);
		check_eq("Initialize: availableReadBytes == 0", cb->availableReadBytes, 0);
		check_eq("Initialize: availableFillBytes == 0x100", cb->availableFillBytes, 0x100);
		check_eq("Initialize: fillCarry == 0", cb->fillCarry, 0);
		check_eq("Initialize: readCarry == 0", cb->readCarry, 0);

		unsigned int base = cb->bufferBase;

		/* AdvanceFillPosition, no wrap: fillPos += n, availableFillBytes
		 * decremented by the FULL n unconditionally (no clamp -- real
		 * confirmed asymmetry vs. AdvanceReadPosition below). */
		cb->AdvanceFillPosition(0x30);
		check_eq("AdvanceFillPosition(0x30): fillPos == base+0x30", cb->fillPos, base + 0x30);
		check_eq("AdvanceFillPosition(0x30): availableFillBytes == 0xd0", cb->availableFillBytes, 0xd0);

		/* AdvanceFillPosition, WITH wrap: remaining-to-end (0x100-0x30=0xd0)
		 * <= n (0xe0), so fillPos wraps to base+(n-remaining) = base+0x10. */
		cb->AdvanceFillPosition(0xe0);
		check_eq("AdvanceFillPosition(0xe0) wraps: fillPos == base+0x10", cb->fillPos, base + 0x10);
		check_eq("AdvanceFillPosition(0xe0) wraps: availableFillBytes == 0xf0 (unclamped, underflowed)",
			 cb->availableFillBytes, (unsigned int)(0xd0 - 0xe0));

		cb->IncrementAvailableReadBytes(0x50);
		check_eq("IncrementAvailableReadBytes(0x50): availableReadBytes == 0x50",
			 cb->availableReadBytes, 0x50);

		/* AdvanceReadPosition, no wrap, n < availableReadBytes: full
		 * decrement, no clamp needed. */
		cb->AdvanceReadPosition(0x20);
		check_eq("AdvanceReadPosition(0x20): readPos == base+0x20", cb->readPos, base + 0x20);
		check_eq("AdvanceReadPosition(0x20): availableReadBytes == 0x30", cb->availableReadBytes, 0x30);
		check_eq("AdvanceReadPosition(0x20): fillCarry == 0x20", cb->fillCarry, 0x20);

		/* AdvanceReadPosition, n > availableReadBytes: confirmed clamp
		 * (cmova-based min) -- availableReadBytes can never go negative. */
		cb->AdvanceReadPosition(0x90);
		check_eq("AdvanceReadPosition(0x90) clamped: readPos == base+0xb0", cb->readPos, base + 0xb0);
		check_eq("AdvanceReadPosition(0x90) clamped: availableReadBytes == 0 (floor)",
			 cb->availableReadBytes, 0);
		check_eq("AdvanceReadPosition(0x90) clamped: fillCarry == 0x50 (0x20 + capped 0x30)",
			 cb->fillCarry, 0x50);

		cb->ReturnUnusedFillBytes(0x10);
		check_eq("ReturnUnusedFillBytes(0x10): availableReadBytes -= 0x10 (underflow, unclamped)",
			 cb->availableReadBytes, (unsigned int)(0 - 0x10));
		check_eq("ReturnUnusedFillBytes(0x10): fillCarry == 0x60", cb->fillCarry, 0x60);

		check_eq("readCarry still 0 before ReaderDaemonAdjust...()", cb->readCarry, 0);
		cb->ReaderDaemonAdjustAvailableFillBytes();
		check_eq("ReaderDaemonAdjust...(): readCarry == fillCarry (0x60)", cb->readCarry, cb->fillCarry);
		check_eq("ReaderDaemonAdjust...(): availableFillBytes increased by 0x60",
			 cb->availableFillBytes, (unsigned int)(0xd0 - 0xe0 + 0x60));

		cb->SetEffectiveSize(0x40);
		check_eq("SetEffectiveSize(0x40): effectiveSize == 0x40", cb->effectiveSize, 0x40);
		check_eq("SetEffectiveSize(0x40): availableFillBytes == 0x40 (full reset)",
			 cb->availableFillBytes, 0x40);
		check_eq("SetEffectiveSize(0x40): readPos == bufferBase (reset)", cb->readPos, base);
		check_eq("SetEffectiveSize(0x40): fillPos == bufferBase (reset)", cb->fillPos, base);
		check_eq("SetEffectiveSize(0x40): bufferEnd == base+0x40", cb->bufferEnd, base + 0x40);
		check_eq("SetEffectiveSize(0x40): availableReadBytes == 0", cb->availableReadBytes, 0);
		check_eq("SetEffectiveSize(0x40): totalSize UNCHANGED at 0x100 (only Reset/Initialize touch it)",
			 cb->totalSize, 0x100);

		cb->AdvanceFillPosition(0x10);
		cb->IncrementAvailableReadBytes(0x5);
		cb->Reset();
		check_eq("Reset(): readPos == bufferBase", cb->readPos, base);
		check_eq("Reset(): fillPos == bufferBase", cb->fillPos, base);
		check_eq("Reset(): availableReadBytes == 0", cb->availableReadBytes, 0);
		check_eq("Reset(): availableFillBytes == effectiveSize (0x40)", cb->availableFillBytes, 0x40);
		check_eq("Reset(): fillCarry == 0", cb->fillCarry, 0);
		check_eq("Reset(): readCarry == 0", cb->readCarry, 0);
		check_eq("Reset(): effectiveSize UNCHANGED at 0x40", cb->effectiveSize, 0x40);
	}

	printf("\n[26] CSTGCDWorker::ProcessCommands() (sec 10.158)\n");
	{
		/* CSTGHDRManager::sInstance is dereferenced directly as a plain
		 * native pointer (not a packed 32-bit field) -- a regular malloc
		 * buffer is fine here (no MAP_32BIT needed, see managers.cpp's
		 * own comment on this). Big enough to cover the embedded
		 * CSTGHDRCircularBuffer at +0x189c8..+0x189fc. */
		unsigned char *hdrBuf = new unsigned char[0x18a00];
		memset(hdrBuf, 0, 0x18a00);
		CSTGHDRManager::sInstance = (CSTGHDRManager *)hdrBuf;
		unsigned char *circBuf = hdrBuf + 0x189c8;

		/* The ring buffer's own base pointer IS a packed 32-bit field,
		 * reconstituted and dereferenced by ProcessCommands() -- needs
		 * MAP_32BIT (see managers.cpp's own comment on this). Two
		 * entries: [0] tag==0 (handled, size=0x30), [1] tag==7
		 * (confirmed real no-op, still consumes/advances). */
		unsigned char *ring = (unsigned char *)mmap(0, 0x1000, PROT_READ | PROT_WRITE,
							     MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
		ring[0 * 8 + 0] = 0;
		*(unsigned int *)(ring + 0 * 8 + 4) = 0x30;
		ring[1 * 8 + 0] = 7;
		*(unsigned int *)(ring + 1 * 8 + 4) = 0xdead;

		CSTGCDWorker *cdw;
		poison_and_construct(&cdw);	/* real ctor zeroes +0x228/+0x22c/+0x230 */
		unsigned char *cdwBase = (unsigned char *)cdw;
		*(unsigned int *)(cdwBase + 0x228) = (unsigned int)(unsigned long)ring;
		*(unsigned int *)(cdwBase + 0x234) = 0x81;	/* matches real Initialize()'s own constant */
		*(unsigned int *)(cdwBase + 0x22c) = 2;	/* producer: 2 entries enqueued */

		cdw->ProcessCommands();

		check_eq("ProcessCommands: consumer index (+0x230) advanced to 2",
			 *(unsigned int *)(cdwBase + 0x230), 2);
		check_eq("ProcessCommands: producer index (+0x22c) untouched",
			 *(unsigned int *)(cdwBase + 0x22c), 2);
		check_eq("ProcessCommands: tag==0 entry's size (0x30) reached IncrementAvailableReadBytes",
			 *(unsigned int *)(circBuf + 0x20), 0x30);
	}

	printf("\n[27] CSTGStreamingEventManager + CSTGStreamingEvent (sec 10.158)\n");
	{
		unsigned long arenaSize = 16 * 1024;
		unsigned char *arena = (unsigned char *)mmap(0, arenaSize, PROT_READ | PROT_WRITE,
							      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
		CSTGBankMemory::Initialize(arena, arenaSize);

		/* Initialize() reconstitutes-and-dereferences ITS OWN packed
		 * pointers (the free-list threading through events[i]'s own
		 * +0x30 field) -- the whole 0x14c44-byte object must be
		 * MAP_32BIT, same sec 10.157 rule as CSTGVoiceAllocator::
		 * Initialize()'s own test above (not just a ctor-only check). */
		unsigned char *semBuf = (unsigned char *)mmap(0, 0x14c44, PROT_READ | PROT_WRITE,
							       MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
		CSTGStreamingEventManager *sem = new (semBuf) CSTGStreamingEventManager();

		check_eq("ctor: sInstance == this",
			 (unsigned int)((unsigned long)CSTGStreamingEventManager::sInstance == (unsigned long)sem), 1);
		check_eq("ctor: freeListHead (+0x14c18) == 0", sem->freeListHead, 0);
		check_eq("ctor: count (+0x14c20) == 0", sem->count, 0);
		check_eq("ctor: mutexPtr32 non-null", (unsigned int)(sem->mutexPtr32 != 0), 1);

		/* Small values (NOT the real 401/0x10000 call-site constants --
		 * see oa_engine_init.h's own class comment for those) so the
		 * per-event CSTGHDRCircularBuffer::Initialize() allocations stay
		 * tiny; exercises the exact same code path with fewer
		 * iterations. */
		sem->Initialize(3, 0x40);

		check_eq("Initialize(3, 0x40): field14c40 == 0x80 (size*2)", sem->field14c40, 0x80);
		check_eq("Initialize(3, 0x40): count == 3", sem->count, 3);
		check_eq("Initialize(3, 0x40): field14c3c == 0", sem->field14c3c, 0);

		for (int i = 0; i < 3; i++) {
			unsigned char *ev = (unsigned char *)&sem->events[i];
			char label[64];
			snprintf(label, sizeof(label), "events[%d]: own index stamped at +0x4", i);
			check_eq(label, *(unsigned short *)(ev + 0x4), (unsigned int)i);
			snprintf(label, sizeof(label), "events[%d]: embedded CSTGHDRCircularBuffer field00 == %d (overwritten with index)", i, i);
			check_eq(label, *(unsigned int *)(ev + 0x40), (unsigned int)i);
			snprintf(label, sizeof(label), "events[%d]: embedded CSTGHDRCircularBuffer availableReadBytes == 0 (Initialize ran)", i);
			check_eq(label, *(unsigned int *)(ev + 0x40 + 0x20), 0);
			snprintf(label, sizeof(label), "events[%d]: embedded CSTGHDRCircularBuffer effectiveSize == 0x80", i);
			check_eq(label, *(unsigned int *)(ev + 0x40 + 0x1c), 0x80);
		}

		/* Walk the free list from head (event[0]) via its own +0x30
		 * "next" field, confirming sequential order 0,1,2, then
		 * confirming freeListTail == the last node's own +0x30 address
		 * and every node's own +0x3c "owner" back-pointer is the fixed
		 * freeListHead slot address. */
		unsigned int headSlotAddr = (unsigned int)((unsigned long)sem + 0x14c18);
		unsigned char *node0 = (unsigned char *)&sem->events[0] + 0x30;
		check_eq("freeListHead == &events[0]+0x30",
			 sem->freeListHead, (unsigned int)(unsigned long)node0);
		unsigned int next0 = *(unsigned int *)node0;
		unsigned char *node1 = (unsigned char *)&sem->events[1] + 0x30;
		check_eq("events[0]'s own +0x30 next == &events[1]+0x30", next0, (unsigned int)(unsigned long)node1);
		unsigned int next1 = *(unsigned int *)node1;
		unsigned char *node2 = (unsigned char *)&sem->events[2] + 0x30;
		check_eq("events[1]'s own +0x30 next == &events[2]+0x30", next1, (unsigned int)(unsigned long)node2);
		unsigned int next2 = *(unsigned int *)node2;
		check_eq("events[2]'s own +0x30 next == 0 (list terminator)", next2, 0);
		check_eq("freeListTail == &events[2]+0x30",
			 sem->freeListTail, (unsigned int)(unsigned long)node2);
		check_eq("events[0]'s own +0x3c owner == &manager->freeListHead",
			 *(unsigned int *)((unsigned char *)&sem->events[0] + 0x3c), headSlotAddr);
		check_eq("events[2]'s own +0x3c owner == &manager->freeListHead",
			 *(unsigned int *)((unsigned char *)&sem->events[2] + 0x3c), headSlotAddr);
	}

	printf("=====================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
