// SPDX-License-Identifier: GPL-2.0
/*
 * test_init_performances.cpp  -  host-side known-answer tests for
 * CSTGGlobal::InitializePerformances() (batch 54).
 *
 * Links src/engine/init_performances.cpp for real -- this includes the
 * REAL CKorgPreloadFile::CKorgPreloadFile()/~CKorgPreloadFile()/
 * CKorgProgBankFile::CKorgProgBankFile() ctors too, so filename
 * generation and vtable-pointer bookkeeping are exercised for real, not
 * mocked. The genuinely deferred callees (CKorgPreloadFile::Load(),
 * CSTGProgramBank::Initialize()/GetPatchSize(), CSTGProgram::Initialize(),
 * CSTGGlobal::PopulateDefaultProgramSlotTemplates(), CSTGGlobal::
 * SubmitPerfChangeRequest()) are deliberately MOCKED here with call-
 * tracking/scripted-return stubs rather than linked -- their real homes
 * (bar2_stubs.cpp for the first four, global.cpp -- not linked, too
 * heavy a dependency chain for this focused test -- for the last one)
 * give them either deliberately no-op/safe-default bodies or unrelated
 * heavy dependencies, matching this project's established "mock the
 * deferred/heavy callees, link the real caller" KAT technique.
 *
 * CSTGGlobal's own real object is confirmed to reach ~43.6MB in
 * confirmed field offsets (see oa_global.h/test_global.cpp) -- this
 * test allocates the SAME established mmap32() buffer size (0x29cc920),
 * zeroed, for `this`, which comfortably covers every offset this batch's
 * own reconstruction touches, including the 200-item block-4 array at
 * +0x27cd024 (last item's own extent ends at +0x2931aa7, well under the
 * buffer's own 0x29cc920 bound).
 *
 * UPDATE (sec 10.230/batch 55): block 4's own raw `vtbl[0x58/4]` fetch
 * was replaced with a direct `reinterpret_cast<CSTGSequence*>(obj)->
 * Initialize()` call (CSTGSequence::Initialize() is now real, see
 * sequence_combi_init.cpp) -- the old `install_vcall_array()`/
 * `vcall_tracker` scheme poked a fake vtable pointer into each array
 * element that the real code no longer even reads, the SAME masking
 * hazard sec 10.228/10.229 already found and fixed in
 * test_global.cpp/test_waveseq_setlist_init.cpp's own scenarios.
 * Replaced with a real, call-counting mock of `CSTGSequence::
 * Initialize()` itself (this test binary links init_performances.cpp
 * alone, so it must supply this symbol -- production's real body lives
 * in sequence_combi_init.cpp, not linked here).
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_setup_global_resources.h"

static unsigned char *mmap32(unsigned long size)
{
	void *p = mmap(0, size, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	return (unsigned char *)p;
}

unsigned char *STGAPIFrontPanelStatus::sInstance;

/* ---- global call-order sequencing (verifies block ordering) ---- */
static int g_seq;

/* ---- CKorgPreloadFile::Load() mock ---- */
static int g_loadCallIndex;
static bool g_loadShouldFail[23];
static char g_loadFilenames[23][12];
int CKorgPreloadFile::Load()
{
	int idx = g_loadCallIndex++;
	if (idx < 23) {
		strncpy(g_loadFilenames[idx], _name ? _name : "", 11);
		g_loadFilenames[idx][11] = 0;
	}
	bool fail = (idx >= 0 && idx < 23) ? g_loadShouldFail[idx] : false;
	return fail ? 0 : 1;
}

/* ---- CSTGProgramBank::Initialize()/GetPatchSize() mocks ---- */
static int g_bankInitCalls;
static unsigned int g_bankInitBankId[23];
static unsigned int g_bankInitType[23];
static bool g_bankInitFlag[23];
static unsigned long g_bankInitThis[23];
void CSTGProgramBank::Initialize(unsigned int bankId, unsigned int bankType, bool flag)
{
	if (g_bankInitCalls < 23) {
		g_bankInitBankId[g_bankInitCalls] = bankId;
		g_bankInitType[g_bankInitCalls] = bankType;
		g_bankInitFlag[g_bankInitCalls] = flag;
		g_bankInitThis[g_bankInitCalls] = (unsigned long)(void *)this;
	}
	g_bankInitCalls++;
}
static int g_getPatchSizeCalls;
static unsigned long g_getPatchSizeThis;
static unsigned int g_getPatchSizeResult = 0x1234;
unsigned int CSTGProgramBank::GetPatchSize() const
{
	g_getPatchSizeCalls++;
	g_getPatchSizeThis = (unsigned long)(const void *)this;
	return g_getPatchSizeResult;
}

/* ---- CSTGProgram::Initialize() mock ---- */
static int g_progInitCalls;
static unsigned int g_progInitBankId, g_progInitPatchSize, g_progInitVoiceModel;
static unsigned long g_progInitThis;
void CSTGProgram::Initialize(unsigned int bankId, unsigned int patchSize, unsigned int voiceModel)
{
	g_progInitCalls++;
	g_progInitBankId = bankId;
	g_progInitPatchSize = patchSize;
	g_progInitVoiceModel = voiceModel;
	g_progInitThis = (unsigned long)(void *)this;
}

/* ---- PopulateDefaultProgramSlotTemplates() mock ---- */
static int g_populateCalls;
static int g_populateSeq;
void CSTGGlobal::PopulateDefaultProgramSlotTemplates()
{
	g_populateCalls++;
	g_populateSeq = g_seq++;
}

/* ---- SubmitPerfChangeRequest() mock ---- */
static int g_submitCalls;
static int g_submitSeq;
static CSTGPerfChangeRequest g_submitReq;
void CSTGGlobal::SubmitPerfChangeRequest(CSTGPerfChangeRequest &request)
{
	g_submitCalls++;
	g_submitSeq = g_seq++;
	g_submitReq = request;
}

/* ---- the 200-item CSTGSequence::Initialize() array (block 4) ---- */
static int g_vcallCount;
static int g_vcallSeq = -1;
static void *g_vcallFirstObj, *g_vcallLastObj;
void CSTGSequence::Initialize()
{
	void *obj = (void *)this;
	if (g_vcallCount == 0) {
		g_vcallFirstObj = obj;
		g_vcallSeq = g_seq++;
	}
	g_vcallLastObj = obj;
	g_vcallCount++;
}

static unsigned char *g_selfBuf;
static const unsigned long kSelfSize = 0x29cc920;

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	bool ok = got == want;
	if (!ok) g_fail++;
	printf("  %s  %-64s 0x%lx\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok) printf("        (wanted 0x%lx)\n", want);
}
static void check_true(const char *label, bool ok)
{
	if (!ok) g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}
static void check_str(const char *label, const char *got, const char *want)
{
	bool ok = got && strcmp(got, want) == 0;
	if (!ok) g_fail++;
	printf("  %s  %-40s \"%s\"\n", ok ? "ok  " : "FAIL", label, got ? got : "(null)");
	if (!ok) printf("        (wanted \"%s\")\n", want);
}

static void reset_all(void)
{
	memset(g_selfBuf, 0, kSelfSize);

	memset(STGAPIFrontPanelStatus::sInstance, 0, 0x30000);

	g_seq = 0;
	g_loadCallIndex = 0;
	for (int i = 0; i < 23; i++) g_loadShouldFail[i] = false;
	memset(g_loadFilenames, 0, sizeof(g_loadFilenames));

	g_bankInitCalls = 0;
	memset(g_bankInitBankId, 0, sizeof(g_bankInitBankId));
	memset(g_bankInitType, 0, sizeof(g_bankInitType));
	memset(g_bankInitFlag, 0, sizeof(g_bankInitFlag));
	memset(g_bankInitThis, 0, sizeof(g_bankInitThis));

	g_getPatchSizeCalls = 0;
	g_getPatchSizeThis = 0;
	g_getPatchSizeResult = 0x1234;

	g_progInitCalls = 0;
	g_progInitBankId = g_progInitPatchSize = g_progInitVoiceModel = 0;
	g_progInitThis = 0;

	g_populateCalls = 0;
	g_populateSeq = -1;

	g_submitCalls = 0;
	g_submitSeq = -1;
	memset(&g_submitReq, 0, sizeof(g_submitReq));

	g_vcallCount = 0;
	g_vcallSeq = -1;
	g_vcallFirstObj = g_vcallLastObj = 0;
}

int main(void)
{
	printf("InitializePerformances test\n");
	printf("==============================================================\n");

	g_selfBuf = mmap32(kSelfSize);
	STGAPIFrontPanelStatus::sInstance = mmap32(0x30000);

	CSTGGlobal *self = (CSTGGlobal *)g_selfBuf;

	printf("[1] All banks succeed: filenames, bitmask, Initialize()/GetPatchSize()/CSTGProgram::Initialize() argument checks\n");
	{
		reset_all();

		self->InitializePerformances();

		check_eq("Load() called 23 times", g_loadCallIndex, 23);
		check_str("bank 0 filename (small-id path)", g_loadFilenames[0], "PROGA.BIN");
		check_str("bank 15 filename (small-id path, last)", g_loadFilenames[15], "PROGP.BIN");
		check_str("bank 16 filename (large-id path, first, doubled letter)", g_loadFilenames[16], "PROGAA.BIN");
		check_str("bank 22 filename (large-id path, last, doubled letter)", g_loadFilenames[22], "PROGGG.BIN");

		check_eq("CSTGProgramBank::Initialize() called 23 times", g_bankInitCalls, 23);
		check_eq("bank 0 Initialize() bankId arg", g_bankInitBankId[0], 0);
		check_eq("bank 0 Initialize() type arg == kBankInfo[0][0] default (1)", g_bankInitType[0], 1);
		check_true("bank 0 Initialize() flag arg == kBankInfo[0][1]!=0 (false)", g_bankInitFlag[0] == false);
		check_true("bank 0 Initialize() this == self+0x132e4d0",
			   g_bankInitThis[0] == (unsigned long)(g_selfBuf + 0x132e4d0u));
		check_eq("bank 6 Initialize() type arg == kBankInfo[6][0] default (0)", g_bankInitType[6], 0);
		check_true("bank 6 Initialize() flag arg == kBankInfo[6][1]!=0 (true)", g_bankInitFlag[6] == true);
		check_true("bank 6 Initialize() this == self+0x132e4d0+6*0x67603",
			   g_bankInitThis[6] == (unsigned long)(g_selfBuf + 0x132e4d0u + 6u * 0x67603u));
		check_true("bank 22 Initialize() this == self+0x132e4d0+22*0x67603",
			   g_bankInitThis[22] == (unsigned long)(g_selfBuf + 0x132e4d0u + 22u * 0x67603u));

		/* default kBankInfo[bankId][0]==0 -> bit SET; !=0 -> bit CLEARED. */
		unsigned int bitmask = *(unsigned int *)(STGAPIFrontPanelStatus::sInstance + 0x294f8);
		check_true("bank 0 (type=1, default) bit CLEARED", (bitmask & (1u << 0)) == 0);
		check_true("bank 1 (type=0, default) bit SET", (bitmask & (1u << 1)) != 0);
		check_true("bank 6 (type=0, default) bit SET", (bitmask & (1u << 6)) != 0);

		check_eq("GetPatchSize() called once", g_getPatchSizeCalls, 1);
		check_true("GetPatchSize() this == self+0x132e4d0+6*0x67603 (bank 6)",
			   g_getPatchSizeThis == (unsigned long)(g_selfBuf + 0x132e4d0u + 6u * 0x67603u));
		check_eq("CSTGProgram::Initialize() called once", g_progInitCalls, 1);
		check_eq("CSTGProgram::Initialize() bankId arg == literal 6", g_progInitBankId, 6);
		check_eq("CSTGProgram::Initialize() patchSize arg == GetPatchSize() result", g_progInitPatchSize, g_getPatchSizeResult);
		check_eq("CSTGProgram::Initialize() voiceModel arg == literal 1", g_progInitVoiceModel, 1);
		check_true("CSTGProgram::Initialize() this == self+0x2976e33",
			   g_progInitThis == (unsigned long)(g_selfBuf + 0x2976e33u));
	}

	printf("[2] Load() failure changes kBankInfo[bank][0] to CKorgProgBankFile::_field8 (2), flipping bitmask polarity\n");
	{
		reset_all();
		g_loadShouldFail[1] = true;	/* bank 1 default {0,0} -> forced fail */

		self->InitializePerformances();

		check_eq("bank 1 Initialize() type arg == CKorgProgBankFile::_field8 (2), not the default 0", g_bankInitType[1], 2);
		unsigned int bitmask = *(unsigned int *)(STGAPIFrontPanelStatus::sInstance + 0x294f8);
		check_true("bank 1 bit now CLEARED (type=2 != 0), opposite of scenario [1]'s SET",
			   (bitmask & (1u << 1)) == 0);
	}

	printf("[3] Block ordering: PopulateDefaultProgramSlotTemplates() -> 200-item vtable loop -> SubmitPerfChangeRequest()\n");
	{
		reset_all();

		self->InitializePerformances();

		check_eq("PopulateDefaultProgramSlotTemplates() called once", g_populateCalls, 1);
		check_eq("200-item vtable loop: exactly 200 calls", g_vcallCount, 200);
		check_true("200-item loop first obj == self+0x27cd024",
			   g_vcallFirstObj == (void *)(g_selfBuf + 0x27cd024u));
		check_true("200-item loop last obj == self+0x27cd024+199*0x1cad",
			   g_vcallLastObj == (void *)(g_selfBuf + 0x27cd024u + 199u * 0x1cadu));
		check_eq("SubmitPerfChangeRequest() called once", g_submitCalls, 1);
		check_true("order: Populate before 200-item loop", g_populateSeq < g_vcallSeq);
		check_true("order: 200-item loop before SubmitPerfChangeRequest", g_vcallSeq < g_submitSeq);

		check_eq("request.tag == 0", g_submitReq.tag, 0);
		check_eq("request.mode == 0", g_submitReq.mode, 0);
		check_eq("request.value1 == 0", g_submitReq.value1, 0);
		check_eq("request.value2 == 0", g_submitReq.value2, 0);
		check_eq("request.source == 3", g_submitReq.source, 3);
		check_eq("request.field14 == 0", g_submitReq.field14, 0);
		check_eq("request.field18 == 0", g_submitReq.field18, 0);
	}

	printf("==============================================================\n");
	if (g_fail) {
		printf("%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
