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
/* RT_run: real extern "C++" linkage (oa_rtparm_pe_table.h, round 52,
 * 2026-07-29 -- that header's own declaration is independently verified
 * against ground truth's own mangled relocation, unlike this block's
 * deliberate extern "C" convention for its enum-widened neighbors), so
 * defined here OUTSIDE the extern "C" block below to match. */
static int g_rtRunCalls;
void RT_run(unsigned char, unsigned char) { g_rtRunCalls++; }

extern "C" {
void RT_pe_select_KorgX2100(void *, unsigned char, int, bool) {}
void KS_get_rtcm_name_for_ge(short, char *out) { if (out) out[0] = 0; }
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
static int g_midiFiltCalls;
void RT_midi_filt_in_tch(unsigned char, unsigned char) { g_midiFiltCalls++; }
void RT_midi_filt_in_bnd(unsigned char, unsigned char) { g_midiFiltCalls++; }
void RT_midi_filt_in_sus(unsigned char, unsigned char) { g_midiFiltCalls++; }
void RT_midi_filt_in_cc1(unsigned char, unsigned char) { g_midiFiltCalls++; }
void RT_midi_filt_in_cc2(unsigned char, unsigned char) { g_midiFiltCalls++; }
void RT_midi_filt_in_ctl(unsigned char, unsigned char) { g_midiFiltCalls++; }
void RT_sysex_in(unsigned char *, long) {}
static int g_nameStringCalls;
static int g_lastNameStringA = -1, g_lastNameStringB = -1;
void KS_get_rtp_name_string(unsigned char a, unsigned char b, char *out, unsigned char)
{ g_nameStringCalls++; g_lastNameStringA = a; g_lastNameStringB = b; if (out) out[0] = 0; }
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

/* Deterministic formulaic mocks for the "per-RTParam table" cluster's
 * own KARMA-library externs -- each returns a value that encodes its own
 * input(s) so a KAT can recompute the expected result independently
 * (from the formula below, not from ckg_engine.cpp's own source). */
short KS_get_rtp_min_pe(unsigned char idx) { return (short)(0x100 + idx); }
short KS_get_rtp_max_pe(unsigned char idx) { return (short)(0x200 + idx); }
short KS_get_rtd_min_pe(unsigned char idx) { return (short)(0x300 + idx); }
short KS_get_rtd_max_pe(unsigned char idx) { return (short)(0x400 + idx); }
static unsigned char g_enabledBits[8];
unsigned char KS_get_rtp_enabled_bits(unsigned char idx) { return idx < 8 ? g_enabledBits[idx] : 0; }
static unsigned char g_bankMenu[8];
unsigned char KS_get_rtp_bank_menu_pe(unsigned char idx) { return idx < 8 ? g_bankMenu[idx] : 0; }
static unsigned char g_multiId[8];
unsigned char KS_get_rtp_multi_id_pe(unsigned char idx) { return idx < 8 ? g_multiId[idx] : 0; }
short KS_get_rtd_min_ge(unsigned char module, unsigned char ge) { return (short)(0x500 + module * 0x40 + ge); }
short KS_get_rtd_max_ge(unsigned char module, unsigned char ge) { return (short)(0x600 + module * 0x40 + ge); }
static unsigned char g_lastGeSelectModule, g_lastGeSelectArg3;
static GenEffect_pub *g_lastGeSelectPtr;
void RT_ge_select(unsigned char module, GenEffect_pub *ge, unsigned char arg3)
{ g_lastGeSelectModule = module; g_lastGeSelectPtr = ge; g_lastGeSelectArg3 = arg3; }
static int g_directPathCalls;
static bool g_lastDirectPathEnable;
void KGOutGate_NotifyEnableDirectPathForVectorCCToSoundEngine(bool enable)
{ g_directPathCalls++; g_lastDirectPathEnable = enable; }
}

/* ==================== class-method mocks ==================== */
unsigned char *CKGBankManager::GetGenEffect(int, int) { return g_sharedBuf + 0x100; }
void CKGBankManager::InitializePerfData() {}
void CKGBankManager::SetupInitUserGEForUI() {}
unsigned char *CKGBankManager::GetSeqKarmaPerfCommon(unsigned int) { return g_sharedBuf + 0x1000; }
unsigned char *CKGBankManager::GetSeqKarmaPerfModule(unsigned int) { return g_sharedBuf + 0x2000; }
unsigned char *CKGBankManager::GetSeqDefaultKarmaPerfCommon() { return g_sharedBuf + 0x3000; }
unsigned char *CKGBankManager::GetSeqDefaultKarmaPerfModule() { return g_sharedBuf + 0x4000; }
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
static int g_resetKarmaCCChannelCalls;
static int g_lastResetKarmaCCChannel = -1;
void CKGMIDIMsgProcessor::ResetKarmaGeneratedCCValue(int channel)
{ g_resetKarmaCCChannelCalls++; g_lastResetKarmaCCChannel = channel; }
static int g_processTimbreThruCalls;
void CKGMIDIMsgProcessor::ProcessTimbreThruChannelMessage(int, unsigned char, char, char, bool)
{ g_processTimbreThruCalls++; }

static int g_updateRTCModelNameCalls;
void CKGUIMsgSender::UpdateRTCModelName() { g_updateRTCModelNameCalls++; }
static int g_changeGECalls;
static long g_lastChangeGEArg = -1;
void CKGUIMsgSender::ChangeGE(long geIndex) { g_changeGECalls++; g_lastChangeGEArg = geIndex; }
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

/* ChangeValuesInBackupWhenChangingGE(int,CKarmaPerfCommon*,CKarmaPerfModule*)
 * is DEFERRED (declared, not defined) in ckg_engine.cpp itself --
 * SendChangeGEToEngine() (now real, see below) calls it on its own
 * m_perfType==2 path, so this test provides its own mock (does not
 * conflict with the real production build, which never links this test
 * file). */
static int g_changeBackupGECalls;
static int g_lastChangeBackupGEModule = -1;
static CKarmaPerfCommon *g_lastChangeBackupGECommon;
static CKarmaPerfModule *g_lastChangeBackupGEModulePtr;
void CKGEngine::ChangeValuesInBackupWhenChangingGE(int module, CKarmaPerfCommon *common, CKarmaPerfModule *rec)
{
	g_changeBackupGECalls++;
	g_lastChangeBackupGEModule = module;
	g_lastChangeBackupGECommon = common;
	g_lastChangeBackupGEModulePtr = rec;
}

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
	__builtin_memset(g_enabledBits, 0, sizeof(g_enabledBits));
	__builtin_memset(g_bankMenu, 0, sizeof(g_bankMenu));
	__builtin_memset(g_multiId, 0, sizeof(g_multiId));
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
	 * SetGECategoryToSharedMemory(a,b) regardless. Since SendChangeGEToEngine()
	 * is now real (no longer mocked), a module whose type falls in
	 * range exercises its full body -- including
	 * CopyCurrentParameterToSharedMemory(), which reads m_currentCommon,
	 * so this fixture must set it to a valid buffer. ---- */
	{
		setup();
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		unsigned char modules[2 * 0x2e8] = {0};
		unsigned char common[0x200] = {0};
		eng->m_currentModule = modules;
		eng->m_currentCommon = common;
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

	/* ---- DoRandomCaptureExec(arg): rec=m_currentModule+arg*0x2e8+0x1a,
	 * shared=SharedMemBase()+arg*0x2e8+0x90b4, both get value's 4 bytes
	 * big-endian-order (value=0x11223344 from the RT_pe_rand_capture
	 * mock). ---- */
	{
		setup();
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		unsigned char modules[2 * 0x2e8] = {0};
		eng->m_currentModule = modules;
		eng->m_numModules = 2;

		eng->DoRandomCaptureExec(1);
		unsigned char *rec = modules + 1 * 0x2e8 + 0x1a;
		unsigned char *shared = g_sharedBuf + 1 * 0x2e8 + 0x90b4;
		check("DoRandomCaptureExec: rec[0]==0x11", rec[0], 0x11);
		check("DoRandomCaptureExec: rec[1]==0x22", rec[1], 0x22);
		check("DoRandomCaptureExec: rec[2]==0x33", rec[2], 0x33);
		check("DoRandomCaptureExec: rec[3]==0x44", rec[3], 0x44);
		check("DoRandomCaptureExec: shared[0]==0x11", shared[0], 0x11);
		check("DoRandomCaptureExec: shared[3]==0x44", shared[3], 0x44);
		eng->~CKGEngine();
	}

	/* ---- RefreshPERTParmInfo(): idx=2 uses the enabled-bits-gated
	 * control/enabled byte-pair path; idx=5 uses the bank_menu==1
	 * force-all-enabled path (control bytes untouched). rtd/rtp sorted
	 * pairs are written unconditionally for both. ---- */
	{
		setup();
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();

		g_enabledBits[2] = 0x5;  /* bits 0,2 set */
		g_multiId[2] = 0x1;      /* bit 0 set only */
		g_bankMenu[5] = 1;

		eng->RefreshPERTParmInfo();

		unsigned char *rec2 = g_sharedBuf + 2 * 0x12;
		check("RefreshPERTParmInfo: idx2 rtdMin", *(short *)(rec2 + 0), 0x302);
		check("RefreshPERTParmInfo: idx2 rtdMax", *(short *)(rec2 + 2), 0x402);
		check("RefreshPERTParmInfo: idx2 rtpMin", *(short *)(rec2 + 4), 0x102);
		check("RefreshPERTParmInfo: idx2 rtpMax", *(short *)(rec2 + 6), 0x202);
		check("RefreshPERTParmInfo: idx2 control[0]==1(bit0 set,multiId bit0 set)", rec2[0xa], 1);
		check("RefreshPERTParmInfo: idx2 enabled[0]==0", rec2[0xe], 0);
		check("RefreshPERTParmInfo: idx2 control[1]==0(bit1 clear)", rec2[0xb], 0);
		check("RefreshPERTParmInfo: idx2 enabled[1]==1", rec2[0xf], 1);
		check("RefreshPERTParmInfo: idx2 control[2]==0(bit2 set,multiId bit2 clear)", rec2[0xc], 0);
		check("RefreshPERTParmInfo: idx2 enabled[2]==0", rec2[0x10], 0);
		check("RefreshPERTParmInfo: idx2 control[3]==0(bit3 clear)", rec2[0xd], 0);
		check("RefreshPERTParmInfo: idx2 enabled[3]==1", rec2[0x11], 1);

		unsigned char *rec5 = g_sharedBuf + 5 * 0x12;
		check("RefreshPERTParmInfo: idx5 rtdMin", *(short *)(rec5 + 0), 0x305);
		check("RefreshPERTParmInfo: idx5 bank_menu forces enabled[0]==1", rec5[0xe], 1);
		check("RefreshPERTParmInfo: idx5 bank_menu forces enabled[3]==1", rec5[0x11], 1);
		check("RefreshPERTParmInfo: idx5 bank_menu leaves control[0]==0(untouched)", rec5[0xa], 0);
		eng->~CKGEngine();
	}

	/* ---- SetPERTParmMinMax(a)/SetPERTParmControlModule(a): same
	 * formulas as RefreshPERTParmInfo() above, for a single idx. ---- */
	{
		setup();
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();

		eng->SetPERTParmMinMax(3);
		unsigned char *rec3 = g_sharedBuf + 3 * 0x12;
		check("SetPERTParmMinMax: rtdMin", *(short *)(rec3 + 0), 0x303);
		check("SetPERTParmMinMax: rtdMax", *(short *)(rec3 + 2), 0x403);
		check("SetPERTParmMinMax: rtpMin", *(short *)(rec3 + 4), 0x103);
		check("SetPERTParmMinMax: rtpMax", *(short *)(rec3 + 6), 0x203);

		g_enabledBits[4] = 0x9; /* bits 0,3 set */
		g_multiId[4] = 0x8;     /* bit 3 set only */
		eng->SetPERTParmControlModule(4);
		unsigned char *rec4 = g_sharedBuf + 4 * 0x12;
		check("SetPERTParmControlModule: control[0]==0(bit0 set,multiId bit0 clear)", rec4[0xa], 0);
		check("SetPERTParmControlModule: enabled[0]==0", rec4[0xe], 0);
		check("SetPERTParmControlModule: control[3]==1(bit3 set,multiId bit3 set)", rec4[0xd], 1);
		check("SetPERTParmControlModule: enabled[3]==0", rec4[0x11], 0);
		check("SetPERTParmControlModule: control[1]==0(bit1 clear)", rec4[0xb], 0);
		check("SetPERTParmControlModule: enabled[1]==1", rec4[0xf], 1);
		eng->~CKGEngine();
	}

	/* ---- SetGERTParmMinMax(module,ge): recBase=shared+ge*0x3c+
	 * module*0x780; display-0 block at +0xc0, display-1 block at
	 * +0x1ec0, sorted rtd pair at +0xc0+0/+2. ---- */
	{
		setup();
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();

		eng->SetGERTParmMinMax(1, 2);
		unsigned char *recBase = g_sharedBuf + 2 * 0x3c + 1 * 0x780;
		unsigned char *block0 = recBase + 0xc0;
		unsigned char *block1 = recBase + 0x1ec0;
		check("SetGERTParmMinMax: block0 val", *(short *)(block0 + 8), 0x1102);
		check("SetGERTParmMinMax: block0 min", *(short *)(block0 + 4), 0x2102);
		check("SetGERTParmMinMax: block0 max", *(short *)(block0 + 6), 0x3102);
		check("SetGERTParmMinMax: block1 val", *(short *)(block1 + 8), 0x1142);
		check("SetGERTParmMinMax: block1 min", *(short *)(block1 + 4), 0x2142);
		check("SetGERTParmMinMax: block1 max", *(short *)(block1 + 6), 0x3142);
		check("SetGERTParmMinMax: block0 rtd sorted min", *(short *)(block0 + 0), 0x542);
		check("SetGERTParmMinMax: block0 rtd sorted max", *(short *)(block0 + 2), 0x642);
		eng->~CKGEngine();
	}

	/* ---- RefreshGERTParmInfo(): loops module in [0,m_numModules),
	 * ge in [0,0x20), calling KS_get_rtp_name_string() then
	 * SetGERTParmMinMax() for every pair. ---- */
	{
		setup();
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		eng->m_numModules = 2;

		g_nameStringCalls = 0;
		eng->RefreshGERTParmInfo();
		check("RefreshGERTParmInfo: name_string called 2*0x20 times", g_nameStringCalls, 2 * 0x20);

		unsigned char *recBase = g_sharedBuf + 3 * 0x3c + 1 * 0x780;
		check("RefreshGERTParmInfo: (module=1,ge=3) block0 val via SetGERTParmMinMax",
		      *(short *)(recBase + 0xc0 + 8), 0x1000 + 0x100 + 3);
		eng->~CKGEngine();
	}

	/* ---- SendChangeGEToEngine(module,ge,arg3): default perfType (0)
	 * selects the 0x722c/0x7230 slot pair; loadOptions!=0 forces both
	 * useRtcModel/resetScenes false regardless of loadKind. arg3==true
	 * triggers CKGUIMsgSender::ChangeGE(); module<numModules triggers
	 * UpdateRTCModelName(). perfType==2 with the +0x74 guard clear
	 * dispatches through GetSeqDefaultKarmaPerf{Module,Common}(). ---- */
	{
		setup();
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		unsigned char modules[2 * 0x2e8] = {0};
		unsigned char common[0x200] = {0};
		eng->m_currentModule = modules;
		eng->m_currentCommon = common;
		eng->m_numModules = 2;
		modules[0 * 0x2e8 + 3] = 0x07; /* voiceModelType != 0x10 */

		*(unsigned int *)(g_sharedBuf + 0x722c) = 5; /* loadOptions != 0 */
		*(unsigned int *)(g_sharedBuf + 0x7230) = 3; /* loadKind (irrelevant, loadOptions!=0) */
		g_lastArg0 = -1;
		g_lastBoolArg = true;
		g_changeGECalls = 0;
		g_updateRTCModelNameCalls = 0;
		g_resetKarmaCCChannelCalls = 0;

		eng->SendChangeGEToEngine(0, 7, true);
		check("SendChangeGEToEngine: ResetKarmaGeneratedCCValue(module) -- voiceModelType!=0x10",
		      g_lastResetKarmaCCChannel, 0);
		check("SendChangeGEToEngine: KS_set_ge_load_options low byte", g_lastArg0, 5);
		check("SendChangeGEToEngine: loadOptions!=0 forces useRtcModel false", g_lastBoolArg, false);
		check("SendChangeGEToEngine: RT_ge_select module arg", g_lastGeSelectModule, 0);
		check("SendChangeGEToEngine: arg3==true calls ChangeGE()", g_changeGECalls, 1);
		check("SendChangeGEToEngine: ChangeGE(module) arg", g_lastChangeGEArg, 0);
		check("SendChangeGEToEngine: module<numModules calls UpdateRTCModelName()", g_updateRTCModelNameCalls, 1);
		check("SendChangeGEToEngine: perfType!=2 -- no ChangeValuesInBackupWhenChangingGE", g_changeBackupGECalls, 0);

		/* voiceModelType==0x10 -> ResetKarmaGeneratedCCValue(m_globalChannel) */
		modules[0 * 0x2e8 + 3] = 0x10;
		eng->m_globalChannel = 9;
		eng->SendChangeGEToEngine(0, 7, false);
		check("SendChangeGEToEngine: voiceModelType==0x10 remaps to m_globalChannel",
		      g_lastResetKarmaCCChannel, 9);
		eng->m_globalChannel = 0;
		modules[0 * 0x2e8 + 3] = 0x07;

		/* perfType==2, +0x74 guard clear -> GetSeqDefaultKarmaPerf*() path */
		eng->m_perfType = 2;
		*(unsigned int *)(g_sharedBuf + 0x7234) = 0; /* != 2, so the dispatch runs */
		*(unsigned int *)(g_sharedBuf + 0x7238) = 0;
		CKGUIMsgProcessor::ms_poInstance[0x74] = 0;
		g_changeBackupGECalls = 0;
		eng->SendChangeGEToEngine(1, 4, false);
		check("SendChangeGEToEngine: perfType==2,+0x74==0 calls ChangeValuesInBackupWhenChangingGE",
		      g_changeBackupGECalls, 1);
		check("SendChangeGEToEngine: ...with the real module arg", g_lastChangeBackupGEModule, 1);
		checkp("SendChangeGEToEngine: ...common from GetSeqDefaultKarmaPerfCommon()",
		       g_lastChangeBackupGECommon, g_sharedBuf + 0x3000);
		checkp("SendChangeGEToEngine: ...rec from GetSeqDefaultKarmaPerfModule()",
		       g_lastChangeBackupGEModulePtr, g_sharedBuf + 0x4000);

		/* perfType==2 but shared[0x7234]==2 -> dispatch skipped entirely */
		*(unsigned int *)(g_sharedBuf + 0x7234) = 2;
		g_changeBackupGECalls = 0;
		eng->SendChangeGEToEngine(1, 4, false);
		check("SendChangeGEToEngine: perfType==2,shared[0x7234]==2 skips the dispatch",
		      g_changeBackupGECalls, 0);

		eng->~CKGEngine();
	}

	/* ---- DoInitModule(module): snapshots the record, calls
	 * SendChangeGEToEngine(), overwrites from a template, restores 3
	 * preserved fields + the velocity-zone word array from the
	 * pre-template snapshot, mirrors the result to shared memory, and
	 * marks shared[0x7222]=1. ---- */
	{
		setup();
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		unsigned char modules[2 * 0x2e8];
		unsigned char common[0x200] = {0};
		__builtin_memset(modules, 0xcc, sizeof(modules)); /* recognizable "old" bytes */
		eng->m_currentModule = modules;
		eng->m_currentCommon = common;
		eng->m_numModules = 2;

		unsigned char *rec = modules + 1 * 0x2e8;
		*(short *)(rec + 0) = 0x1234;      /* typeId */
		rec[0x2] = 0xa5;                   /* programNumber-low5 bits = 0x05 */
		rec[0x3] = 0x07;                   /* voiceModelType */
		rec[0x126] = 0x20;                 /* flag bit5 set */
		*(short *)(rec + 0x20) = (short)0x1111; /* velocity-zone word, preserved verbatim */
		*(short *)(rec + 0x19a + 8 * 5) = (short)0x2222; /* i=5 slot, second triplet */

		/* Template source: m_perfType==0 -> shared+module*0x2e8+0xbaa8. */
		unsigned char *tmpl = g_sharedBuf + 1 * 0x2e8 + 0xbaa8;
		__builtin_memset(tmpl, 0x00, 0x2e8);
		*(short *)(tmpl + 0) = 0x9999; /* template's own typeId, must be overwritten back */
		tmpl[0x2] = 0x40;              /* template's own low-5 bits must be replaced */
		tmpl[0x3] = 0x11;              /* template's own voiceModelType must be replaced */

		g_sharedBuf[0x7222] = 0;
		eng->DoInitModule(1);

		check("DoInitModule: typeId restored from snapshot", *(short *)(rec + 0), 0x1234);
		check("DoInitModule: voiceModelType restored from snapshot", rec[0x3], 0x07);
		check("DoInitModule: programNumber-low5 restored, high bits from template",
		      rec[0x2], (0x40 & 0xe0) | 0x05);
		check("DoInitModule: flag bit5 restored set", rec[0x126] & 0x20, 0x20);
		check("DoInitModule: velocity-zone word preserved from snapshot", *(short *)(rec + 0x20), 0x1111);
		check("DoInitModule: velocity-zone word (2nd triplet, i=5) preserved",
		      *(short *)(rec + 0x19a + 8 * 5), 0x2222);
		check("DoInitModule: marks shared[0x7222]=1", g_sharedBuf[0x7222], 1);
		unsigned char *mirror = g_sharedBuf + 1 * 0x2e8 + 0x909a;
		check("DoInitModule: mirrors rebuilt record to shared+module*0x2e8+0x909a (typeId)",
		      *(short *)(mirror + 0), 0x1234);
		check("DoInitModule: ...mirror voiceModelType too", mirror[0x3], 0x07);

		eng->~CKGEngine();
	}

	/* ---- UpdateEnableDirectPathForVectorCC(): 4 up-front guards all
	 * default to `enable=true` when any fails; the loop OR-accumulates
	 * bit 0x20 of +0x14 across every module whose "effective channel"
	 * equals m_globalChannel, defaulting back to true if no module ever
	 * matched. ---- */
	{
		setup();
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		unsigned char common[8] = {0};
		unsigned char modules[2 * 0x2e8] = {0};
		eng->m_currentCommon = common;
		eng->m_currentModule = modules;
		eng->m_numModules = 2;
		eng->m_globalChannel = 3;
		common[2] = 0x80; /* signed negative -- passes the 3rd guard */

		/* Guard fails (numModules<=0) -> default true. */
		eng->m_numModules = 0;
		g_directPathCalls = 0;
		eng->UpdateEnableDirectPathForVectorCC();
		check("UpdateEnableDirectPathForVectorCC: numModules<=0 -> enable=true", g_lastDirectPathEnable, true);
		eng->m_numModules = 2;

		/* module0: voiceModelType==3==m_globalChannel(3) directly (c==progLow5
		 * path not taken since progLow5(module0[2]&0x1f)!=3 here) -- use the
		 * "c==progLow5" match shape instead: set module0[2]&0x1f==3 too, so
		 * c(3)==progLow5(3) -> matched, and c==m_globalChannel(3) -> updates.
		 * bit 0x20 of +0x14 CLEAR -> lastVal stays 0. module1: type doesn't
		 * match anything -> skipped. Result: matchedAny=true, lastVal=0 ->
		 * enable=false. */
		modules[0 * 0x2e8 + 3] = 3;    /* voiceModelType */
		modules[0 * 0x2e8 + 2] = 3;    /* progLow5 == 3 == voiceModelType */
		modules[0 * 0x2e8 + 0x14] = 0; /* bit 0x20 clear */
		modules[1 * 0x2e8 + 3] = 9;    /* module1: no match at all */
		modules[1 * 0x2e8 + 2] = 9;
		g_directPathCalls = 0;
		eng->UpdateEnableDirectPathForVectorCC();
		check("UpdateEnableDirectPathForVectorCC: matched w/ bit clear -> enable=false",
		      g_lastDirectPathEnable, false);
		check("UpdateEnableDirectPathForVectorCC: calls the KGOutGate notify exactly once",
		      g_directPathCalls, 1);

		/* Same setup but module0's own +0x14 bit 0x20 SET -> enable=true. */
		modules[0 * 0x2e8 + 0x14] = 0x20;
		eng->UpdateEnableDirectPathForVectorCC();
		check("UpdateEnableDirectPathForVectorCC: matched w/ bit set -> enable=true",
		      g_lastDirectPathEnable, true);

		eng->~CKGEngine();
	}

	/* ---- SetMIDIFilterForUnusedModules(): sets m_bSuspended, fires 18
	 * RT_midi_filt_in_* calls (6 funcs * 3 channels), clears
	 * m_bSuspended again. ---- */
	{
		setup();
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();
		CKGMIDIMsgProcessor *proc = (CKGMIDIMsgProcessor *)CKGMIDIMsgProcessor::ms_poInstance;
		proc->m_bSuspended = 0;

		g_midiFiltCalls = 0;
		eng->SetMIDIFilterForUnusedModules();
		check("SetMIDIFilterForUnusedModules: 18 RT_midi_filt_in_* calls", g_midiFiltCalls, 18);
		check("SetMIDIFilterForUnusedModules: m_bSuspended cleared afterward", proc->m_bSuspended, 0);

		eng->~CKGEngine();
	}

	/* ---- ProcessForSeqWhenChangingGE(module) -- round 45, 2026-07-29,
	 * solo. 2 early-return no-op guards (m_perfType==2; shared[0x7234]
	 * ==2), then an EXTRA indexed ChangeValuesInBackupWhenChangingGE()
	 * call (index from bankMgr[+0x97c7d4]) ONLY when
	 * CKGUIMsgProcessor::ms_poInstance[+0x74] is set, ALWAYS followed
	 * by the default-record update (confirmed via the real
	 * fall-through jump, not a separate branch -- both calls fire when
	 * the +0x74 gate is set). ---- */
	{
		setup();
		unsigned char raw[sizeof(CKGEngine) + 64];
		CKGEngine *eng = new (raw) CKGEngine();

		eng->m_perfType = 0;
		*(unsigned int *)(g_sharedBuf + 0x7234) = 0;

		printf("  ProcessForSeqWhenChangingGE\n");

		/* +0x74 clear -> only the default update fires. */
		CKGUIMsgProcessor::ms_poInstance[0x74] = 0;
		g_changeBackupGECalls = 0;
		eng->ProcessForSeqWhenChangingGE(3);
		check("  +0x74==0: exactly 1 call (default only)", g_changeBackupGECalls, 1);
		check("  ...with the real module arg", g_lastChangeBackupGEModule, 3);
		checkp("  ...common from GetSeqDefaultKarmaPerfCommon()",
		       g_lastChangeBackupGECommon, g_sharedBuf + 0x3000);
		checkp("  ...rec from GetSeqDefaultKarmaPerfModule()",
		       g_lastChangeBackupGEModulePtr, g_sharedBuf + 0x4000);

		/* +0x74 set -> BOTH the indexed AND the default update fire
		 * (2 calls total); the LAST call observed is the default one,
		 * since it always runs last. */
		CKGUIMsgProcessor::ms_poInstance[0x74] = 1;
		*(unsigned int *)(g_bankBuf + 0x97c7d4) = 7; /* seq index */
		g_changeBackupGECalls = 0;
		eng->ProcessForSeqWhenChangingGE(5);
		check("  +0x74!=0: exactly 2 calls (indexed + default)", g_changeBackupGECalls, 2);
		check("  ...last call's module arg is still the real module",
		      g_lastChangeBackupGEModule, 5);
		checkp("  ...last call is the DEFAULT one (falls through after the indexed one)",
		       g_lastChangeBackupGECommon, g_sharedBuf + 0x3000);
		checkp("  ...last call's rec is the default one too",
		       g_lastChangeBackupGEModulePtr, g_sharedBuf + 0x4000);

		/* m_perfType==2 -> no-op, 0 calls. */
		eng->m_perfType = 2;
		g_changeBackupGECalls = 0;
		eng->ProcessForSeqWhenChangingGE(1);
		check("  m_perfType==2: no-op", g_changeBackupGECalls, 0);
		eng->m_perfType = 0;

		/* shared[0x7234]==2 -> no-op, 0 calls. */
		*(unsigned int *)(g_sharedBuf + 0x7234) = 2;
		g_changeBackupGECalls = 0;
		eng->ProcessForSeqWhenChangingGE(1);
		check("  shared[0x7234]==2: no-op", g_changeBackupGECalls, 0);
		*(unsigned int *)(g_sharedBuf + 0x7234) = 0;

		eng->~CKGEngine();
	}

	printf(g_fail ? "\n%d CHECK(S) FAILED\n" : "\nALL CHECKS PASSED\n", g_fail);
	return g_fail ? 1 : 0;
}
