// SPDX-License-Identifier: GPL-2.0
/*
 * test_ckg_engine.cpp  -  KAT for CKGEngine (see ../src/engine/ckg_engine.cpp).
 *
 * Standalone test binary, same "test provides its own mocks for every
 * external dependency" convention as every other verify/test_*.cpp in
 * this project -- does NOT link ckg_midi_msg_handler.cpp/
 * ckg_ui_msg_sender.cpp/ckg_module_param_handler.cpp (which already
 * provide REAL bodies for several of CKGEngine's own dependencies
 * elsewhere in the real .ko build) to keep this test's own surface
 * self-contained and independent.
 *
 * Every expected value below was independently re-derived from the raw
 * x86 disassembly (register-by-register, the same notes used to write
 * ckg_engine.cpp itself) rather than copied from the C++ under test --
 * see each check's own comment for the specific real instruction(s) it
 * encodes.
 */

#include <cstdio>
#include <cstring>
#include "oa_ckg_midi_msg_handler.h"
#include "oa_internal.h"	/* placement operator new(size_t, void*) */

static int g_fail;
static void check(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-56s %ld (0x%lx)\n", label, got, (unsigned long)got); return; }
	printf("  FAIL  %-56s got=%ld(0x%lx) want=%ld(0x%lx)\n", label, got, (unsigned long)got, want, (unsigned long)want);
	g_fail++;
}
static void checkp(const char *label, const void *got, const void *want)
{
	if (got == want) { printf("  ok    %-56s %p\n", label, got); return; }
	printf("  FAIL  %-56s got=%p want=%p\n", label, got, want);
	g_fail++;
}

/* ==================== mock singleton storage ==================== */

#define BANKSZ 0x97c800
static unsigned char g_bankBuf[BANKSZ];
unsigned char *CKGBankManager::ms_poInstance = g_bankBuf;

static unsigned char g_sharedBuf[0x15000];

unsigned char *CKGEngine::ms_poInstance;
unsigned char *CKGRTCHandler::ms_poInstance;
unsigned char *CKGUIMsgProcessor::ms_poInstance;
unsigned char *CSKMIDIMsgProcessor::ms_poInstance;
unsigned char *CKGMIDIMsgProcessor::ms_poInstance;
CKGParamEdit *CKGEngine::ms_poKGParamEdit;
CKGTimerManager *CKGEngine::ms_poKGTimerManager;
unsigned char *CKGEngine::ms_poKGEventDisplayManager;

/* ==================== extern "C" KARMA-library mocks ==================== */
static int g_lastArg0, g_lastArg1;
static bool g_lastBoolArg;
extern "C" {
void RT_pe_select_KorgX2100(void *, unsigned char, int, bool) {}
void KS_get_rtcm_name_for_ge(short, char *out) { if (out) out[0] = 0; }
static int g_rtRunCalls;
void RT_run(unsigned char, unsigned char) { g_rtRunCalls++; }
void RT_timbre_thru(unsigned char, unsigned char) {}
void BirthOfKarma(void) {}
void KS_set_ge_load_options(unsigned char v) { g_lastArg0 = v; }
void KS_set_ge_load_use_rtc_model(bool v) { g_lastBoolArg = v; }
void KS_set_ge_load_reset_scenes(bool) {}
void InitScheduler(long, long, long) {}
void RT_sync_mode(unsigned char) {}
void KS_sync_mode_x9100(unsigned char) {}
void KS_set_enable_midi_in_to_karma(bool) {}
void KM_process_before_tx_cc(void) {}
short KS_get_rte_val_ge(unsigned char module, int display, unsigned char rt)
{ return (short)(0x1000 + module * 0x100 + display * 0x40 + rt); }
short KS_get_rte_min_ge(unsigned char module, int display, unsigned char rt)
{ return (short)(0x2000 + module * 0x100 + display * 0x40 + rt); }
short KS_get_rte_max_ge(unsigned char module, int display, unsigned char rt)
{ return (short)(0x3000 + module * 0x100 + display * 0x40 + rt); }
void RT_pe_rand_capture(unsigned char, long *out) { *out = 0x11223344; }
void KS_update_rtc_display_value(unsigned char) {}
static bool g_autoAssignResult = true;
bool KS_rtc_auto_assign_names(int select, int location) { g_lastArg0 = select; g_lastArg1 = location; return g_autoAssignResult; }
void KS_pe_write(void) {}
static bool g_timbreThru;
bool KS_get_timbre_thru(unsigned char) { return g_timbreThru; }
static int g_channelInCalls;
void RT_channel_in(short, short, short, short, short) { g_channelInCalls++; }
void RT_bnd_range_thru(unsigned char, char, char) {}
int KGOutGate_GetChannelInCombi(int) { return 0; }
int KGOutGate_GetChannelInSong(int) { return 0; }
void RT_midi_filt_in_tch(unsigned char, unsigned char) {}
void RT_midi_filt_in_bnd(unsigned char, unsigned char) {}
void RT_midi_filt_in_sus(unsigned char, unsigned char) {}
void RT_midi_filt_in_cc1(unsigned char, unsigned char) {}
void RT_midi_filt_in_cc2(unsigned char, unsigned char) {}
void RT_midi_filt_in_ctl(unsigned char, unsigned char) {}
void RT_sysex_in(unsigned char *, long) {}
void KS_get_rtp_name_string(unsigned char, unsigned char, char *out, unsigned char) { if (out) out[0] = 0; }
static unsigned char g_lastSentMsg[8];
static int g_lastSentLen;
void KGOutGate_SendToSoundEngine(unsigned char *bytes, unsigned short len)
{ g_lastSentLen = len; if (len <= 8) __builtin_memcpy(g_lastSentMsg, bytes, len); }
bool KGOutGate_IsLocatingZeroInSeq(void) { return false; }
void RT_real_time_in(short) {}
void RT_spp_in(short, short) {}
void KS_start_precount(void) {}
static int g_karmaOnArg = -1;
void RT_karma_on(unsigned char on) { g_karmaOnArg = on; }
void KS_reset_sst(bool) {}
void KS_rtc_revert_all_buffers(void) {}
void KS_rtc_revert_one_buffer(int) {}
void KS_rtc_compare_one_scene(int, unsigned char, bool) {}
void KS_rtc_compare_one_control(int, unsigned char, unsigned char, bool) {}
void KS_clear_scheduler(void) {}
void RT_stop_and_rpt_damp(void) {}
void SchedulerTask(void) {}
static int g_notifySlidersCalls;
void SKSTGGate_NotifyKarmaAllSlidersPosition(void) { g_notifySlidersCalls++; }
static bool g_moduleRunning[4];
bool KS_is_module_running(unsigned char m) { return m < 4 && g_moduleRunning[m]; }
}

/* ==================== class-method mocks ==================== */
unsigned char *CKGBankManager::GetGenEffect(int, int) { return g_sharedBuf + 0x100; }
void CKGBankManager::InitializePerfData() {}
void CKGBankManager::SetupInitUserGEForUI() {}
unsigned char *CKGBankManager::GetSeqKarmaPerfCommon(unsigned int) { return g_sharedBuf + 0x1000; }
unsigned char *CKGBankManager::GetSeqKarmaPerfModule(unsigned int) { return g_sharedBuf + 0x2000; }
void CKGBankManager::ResetKarmaPerfForSeq() {}
static eSTGMsgPerfType g_lastRenewType;
void CKGBankManager::RenewBackupKarmaPerf(eSTGMsgPerfType type) { g_lastRenewType = type; }
static int g_lastGECatA = -1, g_lastGECatB = -1;
void CKGBankManager::SetGECategoryToSharedMemory(int a, int b) { g_lastGECatA = a; g_lastGECatB = b; }

static int g_rtcChangePerfCalls;
void CKGRTCHandler::ChangePerformance() { g_rtcChangePerfCalls++; }
int CKGRTCHandler::GetDestinationModule(int module) { return module; }
static int g_resetChordCalls;
void CKGRTCHandler::ResetMIDIChordTrigger() { g_resetChordCalls++; }

static int g_clearSoloCalls, g_resendSoloCalls;
void CKGParamEdit::ClearSoloStatus() { g_clearSoloCalls++; }
void CKGParamEdit::ResendSoloStatus() { g_resendSoloCalls++; }

CKGTimerManager::CKGTimerManager() {}
static int g_timerProcessCalls, g_timerStartSyncCalls, g_timerStopSyncCalls;
void CKGTimerManager::Process() { g_timerProcessCalls++; }
void CKGTimerManager::ChangePerformance() {}
void CKGTimerManager::StopSync() { g_timerStopSyncCalls++; }
void CKGTimerManager::StartSync() { g_timerStartSyncCalls++; }

void CKGEventDisplayManager::Initialize() {}
static int g_evtIdleCalls;
void CKGEventDisplayManager::Idle() { g_evtIdleCalls++; }

static int g_trunAllOffCalls;
void CSKMIDIMsgProcessor::TrunAllNotesFromKeyboardOff() { g_trunAllOffCalls++; }

static int g_resetKarmaCCCalls;
void CKGMIDIMsgProcessor::ResetKarmaGeneratedCCValue() { g_resetKarmaCCCalls++; }
static int g_processTimbreThruCalls;
void CKGMIDIMsgProcessor::ProcessTimbreThruChannelMessage(int, unsigned char, char, char, bool)
{ g_processTimbreThruCalls++; }

static int g_updateRTCModelNameCalls;
void CKGUIMsgSender::UpdateRTCModelName() { g_updateRTCModelNameCalls++; }
static int g_lastResetCtrlBufArg = -1;
void CKGUIMsgSender::ResetValuesInControlBuffer(int a) { g_lastResetCtrlBufArg = a; }
static int g_resetCurrentSceneCalls;
void CKGUIMsgSender::ResetCurrentScene() { g_resetCurrentSceneCalls++; }

/* CSTGBankMemory::AllocAligned() -- real impl lives in
 * src/mem/bank_memory.cpp (not linked into this standalone test), so a
 * simple bump-allocator mock is provided instead, same convention as
 * every other test that needs it (see e.g. test_ckg_midi_msg_handler.cpp). */
static unsigned char g_bankMemPool[0x4000];
static unsigned long g_bankMemOff;
unsigned char *CSTGBankMemory::AllocAligned(unsigned int size, unsigned int alignment)
{
	unsigned long p = (unsigned long)(g_bankMemPool + g_bankMemOff);
	unsigned long aligned = (p + alignment - 1) & ~(unsigned long)(alignment - 1);
	g_bankMemOff = (aligned - (unsigned long)g_bankMemPool) + size;
	return (unsigned char *)aligned;
}

/* IsEditedPerf()/CheckAndSendTimbreBendRange() are DEFERRED (declared,
 * not defined) in ckg_engine.cpp itself -- SendChangePerformanceToEngine()/
 * Idle() still call them, so this test provides simple mocks. */
static bool g_editedPerfResult;
bool CKGEngine::IsEditedPerf() { return g_editedPerfResult; }
static int g_checkBendRangeCalls;
void CKGEngine::CheckAndSendTimbreBendRange() { g_checkBendRangeCalls++; }

/* SendChangeGEToEngine() is DEFERRED (declared, not defined) in
 * ckg_engine.cpp itself -- UpdateUserGE() below still calls it for
 * modules whose type falls in range, so this test provides its own
 * mock (does not conflict with the real production build, which never
 * links this test file). */
static int g_sendChangeGECalls;
void CKGEngine::SendChangeGEToEngine(int a, int b, bool) { g_lastArg0 = a; g_lastArg1 = b; g_sendChangeGECalls++; }

/* ==================== test scaffolding ==================== */

static void setup()
{
	__builtin_memset(g_bankBuf, 0, sizeof(g_bankBuf));
	__builtin_memset(g_sharedBuf, 0, sizeof(g_sharedBuf));
	/* bankMgr[+8] is a packed 32-bit pointer on the real target -- a
	 * native 8-byte write here would overlap bankMgr[+12..+15], so
	 * write only the 4 bytes ckg_engine.cpp's own SharedMemBase()
	 * actually reads (via its ToU32()/FromU32() convention). */
	*(unsigned int *)(g_bankBuf + 8) = (unsigned int)(unsigned long)g_sharedBuf;
	CKGUIMsgProcessor::ms_poInstance = g_bankBuf; /* +0x5c cast target, only needs to be a valid pointer */
	CSKMIDIMsgProcessor::ms_poInstance = g_bankBuf;
	CKGMIDIMsgProcessor::ms_poInstance = g_bankBuf;
	CKGRTCHandler::ms_poInstance = g_bankBuf;
}

int main()
{
	setup();

	/* ---- ctor/dtor ---- */
	{
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		check("ctor: field0==1", eng->m_field0, 1);
		check("ctor: editSuppressed==1", eng->m_editSuppressed, 1);
		check("ctor: field14==0", eng->m_field14, 0);
		check("ctor: globalChannel==0", eng->m_globalChannel, 0);
		check("ctor: bendRangeDirty==0", eng->m_bendRangeDirty, 0);
		check("ctor: bendRangeLo[0]==0x7f", eng->m_bendRangeLo[0], 0x7f);
		check("ctor: bendRangeLo[15]==0x7f", eng->m_bendRangeLo[15], 0x7f);
		check("ctor: bendRangeHi[7]==0x7f", eng->m_bendRangeHi[7], 0x7f);
		checkp("ctor: ms_poInstance==this", CKGEngine::ms_poInstance, raw);
		checkp("ctor: ms_poKGParamEdit != null", CKGEngine::ms_poKGParamEdit, CKGEngine::ms_poKGParamEdit);
		if (!CKGEngine::ms_poKGParamEdit) { printf("  FAIL  ms_poKGParamEdit is null\n"); g_fail++; }
		if (!CKGEngine::ms_poKGTimerManager) { printf("  FAIL  ms_poKGTimerManager is null\n"); g_fail++; }
		if (!CKGEngine::ms_poKGEventDisplayManager) { printf("  FAIL  ms_poKGEventDisplayManager is null\n"); g_fail++; }

		eng->~CKGEngine();
		checkp("dtor: ms_poInstance cleared", CKGEngine::ms_poInstance, (void *)0);
	}

	/* ---- GetKarmaMode(): m_perfType==1 (Program) branches on the
	 * KorgX2100 self-referential pointer check: bankMgr[0]==bankMgr+
	 * 0x385522. Independently derived: `*(void**)bankMgr` is compared
	 * against the CONSTANT address `bankMgr+0x385522` -- to hit the
	 * "isKorgX2100" branch in the test, the mock must actually store
	 * that exact value at bankMgr[0]. */
	{
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();

		eng->m_perfType = 0;
		check("GetKarmaMode: perfType=Combi(0) -> 1", eng->GetKarmaMode(), 1);

		eng->m_perfType = 2;
		check("GetKarmaMode: perfType=Song(2) -> 2", eng->GetKarmaMode(), 2);

		eng->m_perfType = 1;
		*(unsigned int *)g_bankBuf = 0x1234; /* not the KorgX2100 template */
		check("GetKarmaMode: perfType=Program, not KorgX2100 -> 0", eng->GetKarmaMode(), 0);

		*(unsigned int *)g_bankBuf = (unsigned int)(unsigned long)g_bankBuf + 0x385522;
		check("GetKarmaMode: perfType=Program, IS KorgX2100 -> 3", eng->GetKarmaMode(), 3);
		*(unsigned int *)g_bankBuf = 0;

		eng->~CKGEngine();
	}

	/* ---- IsKarmaOn() / ShouldKeepKarmaPerformance() / HaveAllModulesStopped() ---- */
	{
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		unsigned char common[16] = {0};

		eng->m_currentCommon = 0;
		check("IsKarmaOn: null common -> false", eng->IsKarmaOn(), 0);

		eng->m_currentCommon = common;
		g_bankBuf[0x97c7bb] = 1;
		check("IsKarmaOn: bankMgr[0x97c7bb]!=0 -> false", eng->IsKarmaOn(), 0);
		g_bankBuf[0x97c7bb] = 0;

		common[2] = (unsigned char)-1; /* signed byte < 0 */
		check("IsKarmaOn: common[2] signed<0 -> true", eng->IsKarmaOn(), 1);
		common[2] = 5;
		check("IsKarmaOn: common[2] signed>=0 -> false", eng->IsKarmaOn(), 0);

		g_bankBuf[0x97c7bd] = 1;
		check("ShouldKeepKarmaPerformance: bankMgr[0x97c7bd]!=0 -> false", eng->ShouldKeepKarmaPerformance(), 0);
		g_bankBuf[0x97c7bd] = 0;
		eng->m_perfType = 1;
		check("ShouldKeepKarmaPerformance: perfType==1 -> true", eng->ShouldKeepKarmaPerformance(), 1);
		eng->m_perfType = 0;
		check("ShouldKeepKarmaPerformance: perfType!=1 -> false", eng->ShouldKeepKarmaPerformance(), 0);

		__builtin_memset(g_moduleRunning, 0, sizeof(g_moduleRunning));
		check("HaveAllModulesStopped: none running -> true", eng->HaveAllModulesStopped(), 1);
		g_moduleRunning[2] = true;
		check("HaveAllModulesStopped: module2 running -> false", eng->HaveAllModulesStopped(), 0);
		g_moduleRunning[2] = false;
		g_moduleRunning[3] = true;
		check("HaveAllModulesStopped: module3 running -> false", eng->HaveAllModulesStopped(), 0);
		__builtin_memset(g_moduleRunning, 0, sizeof(g_moduleRunning));

		eng->~CKGEngine();
	}

	/* ---- GetRealInputChannel/GetRealOutputChannel: 0x10 sentinel ->
	 * m_globalChannel fallback; module>=m_numModules -> use module 0's
	 * own record instead of module*0x2e8 offset (real quirk). ---- */
	{
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		unsigned char modules[2 * 0x2e8] = {0};
		eng->m_currentModule = modules;
		eng->m_numModules = 2;
		eng->m_globalChannel = 9;

		modules[2] = 5;                 /* module 0 input channel = 5 */
		modules[3] = 0x10;               /* module 0 output channel = sentinel */
		modules[0x2e8 + 2] = 0x10;       /* module 1 input channel = sentinel */
		modules[0x2e8 + 3] = 7;          /* module 1 output channel = 7 */

		check("GetRealInputChannel(0)==5", eng->GetRealInputChannel(0), 5);
		check("GetRealOutputChannel(0)==globalChannel(sentinel)", eng->GetRealOutputChannel(0), 9);
		check("GetRealInputChannel(1)==globalChannel(sentinel)", eng->GetRealInputChannel(1), 9);
		check("GetRealOutputChannel(1)==7", eng->GetRealOutputChannel(1), 7);
		check("GetRealInputChannel(5) out-of-range uses module0 rec", eng->GetRealInputChannel(5), 5);

		eng->~CKGEngine();
	}

	/* ---- IsTimbreZoneThru/IsTimbreThruParam: bit 1 / bit 2 of
	 * per-module record byte +0x126; out-of-range module -> true. ---- */
	{
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		unsigned char modules[0x2e8] = {0};
		eng->m_currentModule = modules;
		eng->m_numModules = 1;

		modules[0x126] = 0;
		check("IsTimbreZoneThru: bit1 clear -> false", eng->IsTimbreZoneThru(0), 0);
		check("IsTimbreThruParam: bit2 clear -> false", eng->IsTimbreThruParam(0), 0);
		modules[0x126] = 0x02;
		check("IsTimbreZoneThru: bit1 set -> true", eng->IsTimbreZoneThru(0), 1);
		modules[0x126] = 0x04;
		check("IsTimbreThruParam: bit2 set -> true", eng->IsTimbreThruParam(0), 1);
		check("IsTimbreZoneThru: module out of range -> true", eng->IsTimbreZoneThru(9), 1);
		check("IsTimbreThruParam: module out of range -> true", eng->IsTimbreThruParam(9), 1);

		eng->~CKGEngine();
	}

	/* ---- ShouldForceTimbreZoneBypass(): gated on m_field10!=0,
	 * flagsChannel in {0,2}, bankMgr[0x97c7bb]==0, common!=null &&
	 * common[2]<0 (signed), m_numModules>0, and a matching module whose
	 * own real output channel == `channel` with bit 0x2 set at
	 * rec[0x126]. ---- */
	{
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		unsigned char common[16] = {0};
		unsigned char modules[0x2e8] = {0};
		eng->m_currentCommon = common;
		eng->m_currentModule = modules;
		eng->m_numModules = 1;
		eng->m_globalChannel = 3;
		common[2] = (unsigned char)-1;
		modules[3] = 5;      /* output channel 5 */
		modules[0x126] = 2;  /* bypass-eligible flag bit set */

		eng->m_field10 = 0;
		check("ShouldForceTimbreZoneBypass: field10==0 -> false", eng->ShouldForceTimbreZoneBypass(5, 0), 0);

		eng->m_field10 = 1;
		check("ShouldForceTimbreZoneBypass: flagsChannel=1 (not 0/2) -> false", eng->ShouldForceTimbreZoneBypass(5, 1), 0);
		check("ShouldForceTimbreZoneBypass: match, flagsChannel=0 -> true", eng->ShouldForceTimbreZoneBypass(5, 0), 1);
		check("ShouldForceTimbreZoneBypass: match, flagsChannel=2 -> true", eng->ShouldForceTimbreZoneBypass(5, 2), 1);
		check("ShouldForceTimbreZoneBypass: channel mismatch -> false", eng->ShouldForceTimbreZoneBypass(6, 0), 0);

		modules[0x126] = 0; /* bit clear */
		check("ShouldForceTimbreZoneBypass: bit clear -> false", eng->ShouldForceTimbreZoneBypass(5, 0), 0);
		modules[0x126] = 2;

		common[2] = 0; /* signed >= 0, karma effectively off */
		check("ShouldForceTimbreZoneBypass: common[2]>=0 -> false", eng->ShouldForceTimbreZoneBypass(5, 0), 0);

		eng->~CKGEngine();
	}

	/* ---- SendChannelMessage: m_field0!=0 -> no-op; else dispatch on
	 * bankMgr[0x20] (single-dereference raw byte, not the shared blob)
	 * between RT_channel_in() and ProcessTimbreThruChannelMessage(). ---- */
	{
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		eng->m_field0 = 1;
		g_channelInCalls = 0;
		g_processTimbreThruCalls = 0;
		eng->SendChannelMessage(0xb0, 1, 2, 3);
		check("SendChannelMessage: field0!=0 -> no calls", g_channelInCalls + g_processTimbreThruCalls, 0);

		eng->m_field0 = 0;
		g_bankBuf[0x20] = 0;
		eng->SendChannelMessage(0xb0, 1, 2, 3);
		check("SendChannelMessage: bankMgr[0x20]==0 -> ProcessTimbreThruChannelMessage", g_processTimbreThruCalls, 1);
		check("SendChannelMessage: bankMgr[0x20]==0 -> no RT_channel_in", g_channelInCalls, 0);

		g_bankBuf[0x20] = 1;
		eng->SendChannelMessage(0xb0, 1, 2, 3);
		check("SendChannelMessage: bankMgr[0x20]!=0 -> RT_channel_in", g_channelInCalls, 1);
		g_bankBuf[0x20] = 0;

		eng->~CKGEngine();
	}

	/* ---- DoRandomCapture(): type 0..3 splat RT_pe_rand_capture()'s
	 * 32-bit result's 4 bytes big-endian-order into 2 independent
	 * locations (per-module record + shared blob), at fixed per-type
	 * offsets; type==4 is a pure "mark dirty" no-op. Oracle value
	 * 0x11223344 -> bytes {0x11,0x22,0x33,0x44} at [off..off+3]. */
	{
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		unsigned char modules[0x2e8] = {0};
		eng->m_currentModule = modules;

		eng->DoRandomCapture(0);
		check("DoRandomCapture(0): rec[0x1a]==0x11", modules[0x1a], 0x11);
		check("DoRandomCapture(0): rec[0x1b]==0x22", modules[0x1b], 0x22);
		check("DoRandomCapture(0): rec[0x1c]==0x33", modules[0x1c], 0x33);
		check("DoRandomCapture(0): rec[0x1d]==0x44", modules[0x1d], 0x44);
		check("DoRandomCapture(0): shared[0x90b4]==0x11", g_sharedBuf[0x90b4], 0x11);
		check("DoRandomCapture(0): shared[0x90b7]==0x44", g_sharedBuf[0x90b7], 0x44);

		eng->DoRandomCapture(4);
		check("DoRandomCapture(4): shared[0x7222] marked dirty", g_sharedBuf[0x7222], 1);

		eng->~CKGEngine();
	}

	/* ---- StoreGERTParmMinMaxToBank(): per-module, per-rtParam (0..0x1f)
	 * val/min/max for display 0 at rec+rt*8+{0x24,0x20,0x22} and display
	 * 1 at rec+rt*8+{0x19a,0x196,0x198}. Oracle mock returns a distinct
	 * value per (module,display,rt) so mixed-up offsets would be caught. */
	{
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		unsigned char modules[0x2e8] = {0};
		eng->m_currentModule = modules;
		eng->m_numModules = 1;

		eng->StoreGERTParmMinMaxToBank();
		int rt = 3;
		short expVal0 = (short)(0x1000 + 0 * 0x100 + 0 * 0x40 + rt);
		short expMin0 = (short)(0x2000 + 0 * 0x100 + 0 * 0x40 + rt);
		short expMax0 = (short)(0x3000 + 0 * 0x100 + 0 * 0x40 + rt);
		short expVal1 = (short)(0x1000 + 0 * 0x100 + 1 * 0x40 + rt);
		check("StoreGERTParmMinMaxToBank: rt3 val(disp0)", *(short *)(modules + rt * 8 + 0x24), expVal0);
		check("StoreGERTParmMinMaxToBank: rt3 min(disp0)", *(short *)(modules + rt * 8 + 0x20), expMin0);
		check("StoreGERTParmMinMaxToBank: rt3 max(disp0)", *(short *)(modules + rt * 8 + 0x22), expMax0);
		check("StoreGERTParmMinMaxToBank: rt3 val(disp1)", *(short *)(modules + rt * 8 + 0x19a), expVal1);

		eng->~CKGEngine();
	}

	/* ---- SetBendRange()/CheckAndSendTimbreBendRange(): SetBendRange
	 * writes both arrays + marks dirty (CheckAndSendTimbreBendRange
	 * itself is DEFERRED, not exercised here). ---- */
	{
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		eng->SetBendRange(2, 5, 9);
		check("SetBendRange: lo[2]==5", eng->m_bendRangeLo[2], 5);
		check("SetBendRange: hi[2]==9", eng->m_bendRangeHi[2], 9);
		check("SetBendRange: dirty flag set", eng->m_bendRangeDirty, 1);
		eng->~CKGEngine();
	}

	/* ---- NotifyEndProcessPerformanceChangeOfSTG(): fixed literal
	 * 5-byte message {0xb0,0x79,0x04,0x05,0xff}. ---- */
	{
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		eng->NotifyEndProcessPerformanceChangeOfSTG();
		check("NotifyEndProcessPerformanceChangeOfSTG: len==5", g_lastSentLen, 5);
		check("...byte0==0xb0", g_lastSentMsg[0], 0xb0);
		check("...byte1==0x79", g_lastSentMsg[1], 0x79);
		check("...byte4==0xff", g_lastSentMsg[4], 0xff);
		eng->~CKGEngine();
	}

	/* ---- UpdateUserGE(): loop over modules whose type is in [a,b] only
	 * runs when the buffer is NOT the KorgX2100 template; always calls
	 * SetGECategoryToSharedMemory(a,b) regardless. ---- */
	{
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		unsigned char modules[2 * 0x2e8] = {0};
		eng->m_currentModule = modules;
		eng->m_numModules = 2;
		*(short *)(modules + 0) = 5;          /* module 0 type = 5, in [1,10] */
		*(short *)(modules + 0x2e8) = 20;      /* module 1 type = 20, out of [1,10] */

		*(unsigned int *)g_bankBuf = 0; /* not KorgX2100 */
		g_lastGECatA = g_lastGECatB = -1;
		eng->UpdateUserGE(1, 10);
		check("UpdateUserGE: SetGECategoryToSharedMemory(a)", g_lastGECatA, 1);
		check("UpdateUserGE: SetGECategoryToSharedMemory(b)", g_lastGECatB, 10);

		*(unsigned int *)g_bankBuf = (unsigned int)(unsigned long)g_bankBuf + 0x385522; /* IS KorgX2100 */
		g_lastGECatA = g_lastGECatB = -1;
		eng->UpdateUserGE(1, 10);
		check("UpdateUserGE: KorgX2100 still calls SetGECategoryToSharedMemory", g_lastGECatA, 1);
		*(unsigned int *)g_bankBuf = 0;

		eng->~CKGEngine();
	}

	/* ---- CopyCurrentParameterToSharedMemory(): 3 real memcpy segments
	 * (shared+0x8e9c<-common, 0x1fe bytes; shared+0x909a<-modules,
	 * m_numModules*0x2e8 bytes; per-module shared+i*0x154+0x6dd0<-
	 * modules+i*0x2e8+0x294, 0x50 bytes each, ONLY when m_numModules==4). */
	{
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		unsigned char common[0x200];
		unsigned char modules[4 * 0x2e8];
		for (unsigned i = 0; i < sizeof(common); i++) common[i] = (unsigned char)(i + 1);
		for (unsigned i = 0; i < sizeof(modules); i++) modules[i] = (unsigned char)(i * 3 + 7);
		eng->m_currentCommon = common;
		eng->m_currentModule = modules;
		eng->m_numModules = 4;

		eng->CopyCurrentParameterToSharedMemory();
		check("CopyShared: segment1 first byte", g_sharedBuf[0x8e9c], common[0]);
		check("CopyShared: segment1 last byte (0x1fe bytes)", g_sharedBuf[0x8e9c + 0x1fd], common[0x1fd]);
		check("CopyShared: segment2 first byte", g_sharedBuf[0x909a], modules[0]);
		check("CopyShared: segment2 last byte", g_sharedBuf[0x909a + 4 * 0x2e8 - 1], modules[4 * 0x2e8 - 1]);
		check("CopyShared: segment3 module0 first byte", g_sharedBuf[0x6dd0], modules[0x294]);
		check("CopyShared: segment3 module1 first byte", g_sharedBuf[0x6dd0 + 0x154], modules[0x2e8 + 0x294]);
		check("CopyShared: segment3 module3 last byte", g_sharedBuf[0x6dd0 + 3 * 0x154 + 0x4f], modules[3 * 0x2e8 + 0x294 + 0x4f]);

		__builtin_memset(g_sharedBuf + 0x6dd0, 0xAA, 0x154 * 4);
		eng->m_numModules = 3; /* != 4 -> segment 3 must NOT run */
		eng->CopyCurrentParameterToSharedMemory();
		check("CopyShared: segment3 skipped when numModules!=4", g_sharedBuf[0x6dd0], 0xAA);

		eng->~CKGEngine();
	}

	/* ---- DoAutoAssignRTName(): copies FROM the live m_currentCommon/
	 * m_currentModule record INTO the Seq-backup slot (GetSeqKarmaPerf*
	 * result), the reverse of what the method name alone might suggest --
	 * confirmed via the real disassembly's own read-from/write-to
	 * registers, independently re-checked while writing this test. */
	{
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		unsigned char common[0x40];
		unsigned char modules[0x2e8];
		for (unsigned i = 0; i < sizeof(common); i++) common[i] = (unsigned char)(0x50 + i);
		for (unsigned i = 0; i < sizeof(modules); i++) modules[i] = (unsigned char)(0x90 + i);
		eng->m_currentCommon = common;
		eng->m_currentModule = modules;
		eng->m_numModules = 1;
		eng->m_perfType = 2; /* Song, required for the copy to fire */

		unsigned char *seqCommon = ((CKGBankManager *)CKGBankManager::ms_poInstance)->GetSeqKarmaPerfCommon(0); /* mock: g_sharedBuf+0x1000 */
		__builtin_memset(seqCommon, 0, 0x40);
		g_autoAssignResult = true;
		g_sharedBuf[0x7222] = 0;
		eng->DoAutoAssignRTName(0);
		check("DoAutoAssignRTName(module=0): copies common[+4] -> seq[+4]", seqCommon[4], common[4]);
		check("DoAutoAssignRTName(module=0): copies common[+0x1c] -> seq[+0x1c]", seqCommon[0x1c], common[0x1c]);
		check("DoAutoAssignRTName: marks shared[0x7222] dirty", g_sharedBuf[0x7222], 1);

		g_autoAssignResult = false;
		g_sharedBuf[0x7222] = 0;
		eng->DoAutoAssignRTName(0);
		check("DoAutoAssignRTName: KS_rtc_auto_assign_names false -> no-op", g_sharedBuf[0x7222], 0);
		g_autoAssignResult = true;

		eng->~CKGEngine();
	}

	/* ---- Initialize()+Idle(): end-to-end smoke test, no crash, real
	 * field wiring confirmed. ---- */
	{
		setup();
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		/* Real ground truth stores bankMgr[+0]/[+4]/[+8] as 3
		 * independent 4-byte packed pointer slots (the real i386
		 * target's own pointer width) -- ckg_engine.cpp's own
		 * Initialize()/SharedMemBase() read them via this project's
		 * established ToU32()/FromU32() convention specifically to
		 * avoid an 8-byte host pointer write here overlapping an
		 * adjacent slot, so this mock matches with 4-byte writes too. */
		*(unsigned int *)g_bankBuf = (unsigned int)(unsigned long)(g_sharedBuf + 0x3000);
		*(unsigned int *)(g_bankBuf + 4) = (unsigned int)(unsigned long)(g_sharedBuf + 0x4000);
		*(unsigned int *)(g_bankBuf + 8) = (unsigned int)(unsigned long)g_sharedBuf;
		eng->Initialize();
		checkp("Initialize: m_currentCommon wired from bankMgr[0]", eng->m_currentCommon, g_sharedBuf + 0x3000);
		checkp("Initialize: m_currentModule wired from bankMgr[4]", eng->m_currentModule, g_sharedBuf + 0x4000);
		check("Initialize: m_numModules==4", eng->m_numModules, 4);
		check("Initialize: m_field14==4", eng->m_field14, 4);
		int idleBefore = g_timerProcessCalls;
		eng->Idle();
		check("Idle: CKGTimerManager::Process() invoked", g_timerProcessCalls, idleBefore + 1);
		eng->~CKGEngine();
	}

	printf(g_fail ? "\n%d CHECK(S) FAILED\n" : "\nALL CHECKS PASSED\n", g_fail);
	return g_fail ? 1 : 0;
}
