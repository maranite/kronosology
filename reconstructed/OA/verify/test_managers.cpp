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
#include "oa_engine.h"

/* Real kernel-only APIs (RTAI mutex wrappers) and a not-yet-reconstructed
 * static method (CSTGToneAdjustDescriptor) -- mocked here purely so
 * CPowerOffTimer/CSTGVoiceModelManager's constructors link on the host,
 * same treatment as __kmalloc/kfree in test_new_delete.cpp. */
static int g_mutexInitCalls;
extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void) { return 24; }
extern "C" void *rtwrap_malloc(unsigned int size) { return malloc(size); }
extern "C" void rtwrap_pthread_mutex_init(void *, void *) { g_mutexInitCalls++; }
void CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors() { }

/* Link-satisfying mocks for sec 10.144's new small `Initialize()`/
 * `ProcessCommands()` bodies -- this file doesn't link global.cpp, so
 * ResolveActivePerformanceVarsManagerRaw() needs its own stub here (a
 * null "no active manager" default is fine: nothing in this file
 * exercises CSTGPerformance::IsCurrentlyActive()). */
unsigned char *ResolveActivePerformanceVarsManagerRaw() { return 0; }
extern "C" unsigned int CSTGCDWorker_InitializeBuffer(void *) { return 0; }
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

	printf("=====================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
