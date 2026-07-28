// SPDX-License-Identifier: GPL-2.0
/*
 * ckg_engine.cpp  -  out-of-line real bodies for CKGEngine (and its own
 * new small dependencies: CKGTimerManager's ctor-visible surface, several
 * new CKGBankManager/CKGRTCHandler methods, CKGParamEdit's 3 solo-status
 * methods, and ~50 free RT_*, KS_*, KGOutGate_* KARMA-library externs --
 * all declared in oa_ckg_module_param_msg_handler.h, see that header's own
 * "CKGEngine" section for the full field-layout writeup).
 *
 * 55/74 real ground-truth methods reconstructed this batch (2026-07-28),
 * a fresh continuation of the CKG/CSK KARMA cluster sweep after
 * `ckg_midi_msg_processor_evtdisp_2026-07-28.md`'s own next-target list --
 * CKGEngine was the last major opaque stand-in left in the "KARMA
 * performance-editing engine" family every other CKG*, CSK* class already
 * references heavily (`ms_poInstance`/`ms_poKGParamEdit` are read by
 * dozens of already-real methods across this project).
 *
 * DEFERRED this batch (declared in the header, NOT defined here -- the
 * established "expected Unknown symbol at insmod" convention, same as any
 * other not-yet-reconstructed class surface in this project):
 *   IsEditedPerf()          ground-truth offset 0x3a98e0, 9458 bytes -- a huge outlier,
 *                            almost certainly a giant per-RTParam
 *                            edited-state comparison table; genuinely not
 *                            attempted this batch.
 *   FakeTimbreThru()        ground-truth offset 0x3acfa0,  531 bytes
 *   RefreshPERTParmInfo()   ground-truth offset 0x3ad670,  467 bytes
 *   SetPERTParmMinMax()     ground-truth offset 0x3ad860,  192 bytes
 *   SetPERTParmControlModule() ground-truth offset 0x3ad920, 352 bytes
 *   SetGERTParmMinMax()     ground-truth offset 0x3ada80,  288 bytes
 *   RefreshGERTParmInfo()   ground-truth offset 0x3adba0,  144 bytes
 *   SendChangeGEToEngine()  ground-truth offset 0x3adc30,  816 bytes
 *   DoInitModule()          ground-truth offset 0x3adf60,  464 bytes
 *   DoRandomCaptureExec()   ground-truth offset 0x3ac5b0,  164 bytes
 *   UpdateEnableDirectPathForVectorCC() ground-truth offset 0x3ae190, 272 bytes
 *   ChangePerformance()     ground-truth offset 0x3ae2a0,  960 bytes -- the top-level
 *                            2-arg Combi/Program/Song orchestrator.
 *   CloseGECategoryPopup()  ground-truth offset 0x3ae940, 1072 bytes
 *   UpdateGEInfo()          ground-truth offset 0x3aee80,  368 bytes
 *   ChangeValuesInBackupWhenChangingGE() (both overloads) ground-truth offset 0x3ac920/
 *                            0x3acb00, 480/432 bytes -- traced far enough
 *                            to see the overall shape (a dense,
 *                            multi-segment field-by-field struct copy
 *                            between a CKarmaPerfCommon/CKarmaPerfModule
 *                            "live" record and its per-seq backup slot,
 *                            touching offsets +0x4/+0x14/+0x127/+0x128/
 *                            +0x136/+0x138/+0x148/+0x194 with several
 *                            reused/rebased scratch registers) but NOT
 *                            independently confirmed to the same
 *                            byte-exact confidence as the rest of this
 *                            batch -- a real, scoped follow-up rather
 *                            than a guessed transcription.
 *   ProcessForSeqWhenChangingGE() ground-truth offset 0x3accb0, 192 bytes -- trivial
 *                            control flow itself, but its only 2 real
 *                            call targets are the 2 deferred overloads
 *                            directly above.
 *
 * All 12 "per-RTParam table" / struct-copy methods above are the SAME
 * family already proven mechanical-but-lengthy while transcribing
 * StoreGERTParmMinMaxToBank()/DoRandomCapture() (both INCLUDED this
 * batch, see below) -- deferring them keeps this batch's own confidence
 * bar high rather than rushing a fragile transcription.
 *
 * === Shared idiom: SharedMemBase() ===
 * Every one of CopyCurrentParameterToSharedMemory()/DoAutoRTCSetup()/
 * DoClearRTCSetup()/DoRandomCapture()/DoAutoAssignRTName()/
 * NotifyRTCSetupStatus()/InitializeRTCSetup()/UpdateRTCModelName()
 * dereferences `CKGBankManager::ms_poInstance[+8]` as a POINTER (not a
 * byte), the base of a large shared-memory region distinct from
 * CKGBankManager's own `+0x97c7xx`-range per-switch config table
 * (single-dereference `ms_poInstance[N]`, established elsewhere in this
 * project) -- same "double-dereference vs single-dereference" distinction
 * oa_ckg_switch_family.h's own `OA_CKGBankMgrState()` helper already
 * documents for a DIFFERENT field (`+0`, not `+8`). Factored into one
 * helper here since so many real methods share it.
 *
 * === CopyCurrentParameterToSharedMemory()'s 3 real memcpy segments ===
 * GCC's own inline-memcpy expansion (`rep movsd` + conditional trailing
 * `movsw`/`movsb`) was decoded back into its real (dst, src, byte-count)
 * triple for each segment -- the trailing-byte-count encoding is the
 * REAL total byte count's own low 2 bits (`count & 2` / `count & 1`),
 * confirmed by cross-checking `mov eax,0x1fe` against the visible
 * `ecx=0x7f` (0x7f*4 = 0x1fc, +2 = 0x1fe -- exact match) rather than
 * guessed. `__builtin_memcpy()` reproduces the identical byte-for-byte
 * result without needing to hand-transcribe the rep-movs/tail dance.
 */

#include "oa_ckg_midi_msg_handler.h"	/* CKGEngine + everything it depends
					 * on (CKGBankManager, CKGRTCHandler,
					 * CKGParamEdit, CKGUIMsgSender,
					 * CKGMIDIMsgProcessor,
					 * CSKMIDIMsgProcessor,
					 * CKGEventDisplayManager), same
					 * top-level include already used by
					 * ckg_midi_msg_handler.cpp. */
#include "oa_bank_memory.h"		/* CSTGBankMemory::AllocAligned() */
#include "oa_internal.h"		/* placement operator new(size_t, void*) */

/* ToU32()/FromU32() -- this project's established "packed 32-bit
 * pointer" convention (see e.g. src/engine/engine_init.cpp) for any
 * field that's a real 4-byte pointer on the i386 target but would
 * silently be read/written as 8 bytes (corrupting an adjacent field) if
 * naively cast through `unsigned char **`/`void **` on this 64-bit host
 * verify build. Every raw-offset pointer read into CKGBankManager's own
 * blob below (`bankMgr[+0]`/`[+4]`/`[+8]`, 3 independent 4-byte slots on
 * the real target) uses this pair instead of a native pointer cast. */
static inline unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static inline unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

static inline unsigned char *SharedMemBase()
{
	return FromU32(*(unsigned int *)(CKGBankManager::ms_poInstance + 8));
}

/* .text+0x3a96e0, 428 bytes (C1==C2). Real body: register `this` as the
 * singleton, placement-construct CKGParamEdit/CKGTimerManager/
 * CKGEventDisplayManager (the last via plain `operator new`, NOT
 * CSTGBankMemory::AllocAligned -- confirmed distinct real relocation),
 * then zero-init every field the ctor itself actually touches. Fields
 * never written here (m_perfType/m_field_a4/m_geCategoryPopupModule/
 * m_geCategoryBackup) are zero-initialized anyway for deterministic KAT
 * tests -- a documented, behavior-preserving deviation, same convention
 * as CKGController::m_value in oa_ckg_switch_family.h. */
CKGEngine::CKGEngine()
{
	ms_poInstance = (unsigned char *)this;
	m_field14 = 0;
	m_field0 = 1;
	m_editSuppressed = 1;

	ms_poKGParamEdit = new (CSTGBankMemory::AllocAligned(8, 0x10)) CKGParamEdit();
	ms_poKGTimerManager = new (CSTGBankMemory::AllocAligned(0x38, 0x10)) CKGTimerManager();
	ms_poKGEventDisplayManager = (unsigned char *)new (::operator new(0xedc)) CKGEventDisplayManager();

	m_globalChannel = 0;
	m_numModules = 0;	/* real ctor leaves this uninitialized (only
				 * Initialize() sets it) -- zeroed here too,
				 * same documented "deterministic KAT tests"
				 * deviation as the other never-set fields
				 * below. */
	m_currentCommon = 0;
	m_currentModule = 0;
	m_field10 = 0;
	for (int i = 0; i < 16; i++) {
		m_bendRangeLo[i] = 0x7f;
		m_bendRangeHi[i] = 0x7f;
	}
	m_bendRangeDirty = 0;
	m_rtcDisplayValue = 0;
	m_geCategoryPopupOpen = 0;

	m_perfType = 0;
	m_field_a4 = 0;
	m_geCategoryPopupModule = 0;
	__builtin_memset(m_geCategoryBackup, 0, sizeof(m_geCategoryBackup));
}

/* .text+0x3a9890, 30 bytes. Real body: `operator delete()` the
 * CKGEventDisplayManager block directly (NOT its own dtor -- a real
 * quirk, same "no virtual dtor anywhere in this cluster" convention
 * documented in oa_ckg_switch_family.h for CKGController), then clear
 * the singleton pointer. */
CKGEngine::~CKGEngine()
{
	::operator delete(ms_poKGEventDisplayManager);
	ms_poInstance = 0;
}

/* .text+0x3a98b0, 47 bytes. Real body: m_perfType==Program(1) -> a
 * special "is the active edit buffer the built-in KorgX2100 template"
 * check (CKGBankManager's own first field compared against a fixed
 * offset 0x385522 bytes into the SAME allocation -- the bank manager's
 * own huge memory blob embeds this fixed built-in template at that
 * offset; confirmed via the identical inline computation reused,
 * un-factored, inside SendChangePerformanceToEngine() below); returns
 * 3 if so, 0 otherwise. m_perfType==Song(2) -> 2. Anything else -> 1. */
int CKGEngine::GetKarmaMode()
{
	if (m_perfType == 1) {
		unsigned char *bankMgr = CKGBankManager::ms_poInstance;
		unsigned int korgX2100Template = ToU32(bankMgr) + 0x385522;
		bool isKorgX2100 = (*(unsigned int *)bankMgr == korgX2100Template);
		return isKorgX2100 ? 3 : 0;
	}
	return (m_perfType == 2) ? 2 : 1;
}

/*
 * EditBufferKorgX2100Raw -- the real fixed `.bss` blob
 * RT_pe_select_KorgX2100() reads (own real name/layout not confirmed
 * beyond this shape; ground truth's own `struct EditBufferKorgX2100`
 * mangled type name, own fields never dereferenced by CKGEngine itself,
 * only written and passed through by address). Real per-field write
 * stride confirmed via SendChangePerformanceToEngine()'s own `[ebx*4+
 * 0x591c04]`/`[ebx*4+0x591c14]` relocations, 0x10 bytes (4 dwords) apart
 * -- exactly `sizeof(moduleRecords)`, so no gap between the two arrays.
 */
namespace {
struct EditBufferKorgX2100Raw {
	void *common;
	void *moduleRecords[4];
	void *genEffects[4];
};
}
static EditBufferKorgX2100Raw s_editBufferKorgX2100;

/* .text+0x3abdf0, 315 bytes. */
void CKGEngine::SendChangePerformanceToEngine(CKarmaPerfCommon *common, CKarmaPerfModule *modules, int count)
{
	unsigned char *rec = (unsigned char *)modules;
	for (int i = 0; i < count && i < 4; i++) {
		short type = *(short *)rec;
		s_editBufferKorgX2100.genEffects[i] =
			((CKGBankManager *)CKGBankManager::ms_poInstance)->GetGenEffect(i, type);
		s_editBufferKorgX2100.moduleRecords[i] = rec;
		rec += 0x2e8;
	}
	s_editBufferKorgX2100.common = common;

	bool edited = IsEditedPerf();
	int mode = GetKarmaMode();

	if (count == 1) {
		RT_run(1, 0);
		RT_run(2, 0);
		RT_run(3, 0);
		RT_run(0, 1);
		RT_timbre_thru(0, 1);
	}

	RT_pe_select_KorgX2100(&s_editBufferKorgX2100, (unsigned char)edited, mode, count == 1);

	if (count <= 0)
		return;

	for (int i = 0; i < count; i++) {
		if (i >= m_numModules)
			continue;
		short typeId = *(short *)(m_currentModule + (unsigned int)i * 0x2e8);
		char *outName = (char *)(SharedMemBase() + 0x14330);
		KS_get_rtcm_name_for_ge(typeId, outName);
		((CKGUIMsgSender *)(CKGUIMsgProcessor::ms_poInstance + 0x5c))->UpdateRTCModelName();
	}
}

/* .text+0x3abf40, 255 bytes. */
void CKGEngine::Initialize()
{
	BirthOfKarma();
	KS_set_ge_load_options(0);
	KS_set_ge_load_use_rtc_model(true);
	KS_set_ge_load_reset_scenes(true);

	unsigned char *shared = SharedMemBase();
	*(int *)(shared + 0x7230) = 3;
	*(int *)(shared + 0x7228) = 3;
	*(int *)(shared + 0x7238) = 3;

	((CKGBankManager *)CKGBankManager::ms_poInstance)->InitializePerfData();
	((CKGBankManager *)CKGBankManager::ms_poInstance)->SetupInitUserGEForUI();
	InitScheduler(0, 0x80, 0);
	RT_sync_mode(1);
	KS_sync_mode_x9100(0);
	KS_set_enable_midi_in_to_karma(true);

	unsigned char *bankMgr = CKGBankManager::ms_poInstance;
	m_currentCommon = FromU32(*(unsigned int *)bankMgr);
	m_currentModule = FromU32(*(unsigned int *)(bankMgr + 4));
	m_numModules = 4;
	m_field0 = 0;
	m_field_a4 = 0;

	((CKGRTCHandler *)CKGRTCHandler::ms_poInstance)->ChangePerformance();
	SendChangePerformanceToEngine((CKarmaPerfCommon *)m_currentCommon, (CKarmaPerfModule *)m_currentModule, m_numModules);

	m_field0 = 0;
	m_field14 = 4;
	((CKGEventDisplayManager *)ms_poKGEventDisplayManager)->Initialize();
}

/* .text+0x3ac040, 60 bytes. */
void CKGEngine::ChangePerformancePtrForEngine(CKarmaPerfCommon *common, CKarmaPerfModule *modules, int count)
{
	unsigned char *rec = (unsigned char *)modules;
	for (int i = 0; i < count; i++) {
		s_editBufferKorgX2100.moduleRecords[i] = rec;
		rec += 0x2e8;
	}
	s_editBufferKorgX2100.common = common;
	KM_process_before_tx_cc();
}

/* .text+0x3ac080, 47 bytes. */
void CKGEngine::UpdateSoloStatus(bool solo)
{
	if (m_perfType == 1)
		return;
	if (!solo)
		ms_poKGParamEdit->ClearSoloStatus();
	else
		ms_poKGParamEdit->ResendSoloStatus();
}

/* .text+0x3ac0c0, 159 bytes. Real body: dispatches on m_perfType to pick
 * one of 3 (val,valUseRtc) dword pairs out of the shared-memory blob
 * (+0x7224/+0x7228 for Song, +0x7234/+0x7238 for Program, +0x722c/+0x7230
 * otherwise/Combi) then forwards them to
 * KS_set_ge_load_options()/KS_set_ge_load_use_rtc_model()/
 * KS_set_ge_load_reset_scenes() the same way every other "3 real
 * instance methods" caller in this class does. */
void CKGEngine::NotifyRTCSetupStatus()
{
	unsigned char *shared = SharedMemBase();
	int val, useRtc;
	if (m_perfType == 1) {
		val = *(int *)(shared + 0x7224);
		useRtc = *(int *)(shared + 0x7228);
	} else if (m_perfType == 2) {
		val = *(int *)(shared + 0x7234);
		useRtc = *(int *)(shared + 0x7238);
	} else {
		val = *(int *)(shared + 0x722c);
		useRtc = *(int *)(shared + 0x7230);
	}

	KS_set_ge_load_options((unsigned char)val);

	bool useRtcModel;
	if (val != 0)
		useRtcModel = (useRtc == 1);
	else
		useRtcModel = (useRtc == 2) ? true : false;
	KS_set_ge_load_use_rtc_model(useRtcModel);
	KS_set_ge_load_reset_scenes(useRtc == 3);
}

/* .text+0x3ac170, 75 bytes. */
void CKGEngine::InitializeRTCSetup()
{
	KS_set_ge_load_options(0);
	KS_set_ge_load_use_rtc_model(true);
	KS_set_ge_load_reset_scenes(true);
	unsigned char *shared = SharedMemBase();
	*(int *)(shared + 0x7230) = 3;
	*(int *)(shared + 0x7228) = 3;
	*(int *)(shared + 0x7238) = 3;
}

/* .text+0x3ac1c0, 296 bytes. Per-module, per-RTParam (0..0x1f) min/max/
 * value snapshot from the KARMA library into the live per-module record's
 * own "GE RTParam" sub-table -- 2 independent 8-byte-stride tables at
 * +0x20/+0x190 relative to the per-module record base, "display 0"
 * (Combi/Program) and "display 1" (Song) respectively. Every offset
 * below was read directly off the real disassembly's own displacement
 * bytes, not inferred from the method name. */
void CKGEngine::StoreGERTParmMinMaxToBank()
{
	for (int module = 0; module < m_numModules; module++) {
		unsigned char moduleByte = (unsigned char)module;
		unsigned char *base = m_currentModule + (unsigned int)module * 0x2e8;
		for (int rt = 0; rt < 0x20; rt++) {
			*(short *)(base + rt * 8 + 0x24) = KS_get_rte_val_ge(moduleByte, 0, (unsigned char)rt);
			*(short *)(base + rt * 8 + 0x20) = KS_get_rte_min_ge(moduleByte, 0, (unsigned char)rt);
			*(short *)(base + rt * 8 + 0x22) = KS_get_rte_max_ge(moduleByte, 0, (unsigned char)rt);

			*(short *)(base + rt * 8 + 0x19a) = KS_get_rte_val_ge(moduleByte, 1, (unsigned char)rt);
			*(short *)(base + rt * 8 + 0x196) = KS_get_rte_min_ge(moduleByte, 1, (unsigned char)rt);
			*(short *)(base + rt * 8 + 0x198) = KS_get_rte_max_ge(moduleByte, 1, (unsigned char)rt);
		}
	}
}

/* .text+0x3ac2f0, 16 bytes. Real body ignores its own `long` argument
 * entirely -- byte-identical to DoClearRTCSetup(). */
void CKGEngine::DoAutoRTCSetup(long)
{
	SharedMemBase()[0x7222] = 1;
}

/* .text+0x3ac300, 16 bytes. */
void CKGEngine::DoClearRTCSetup(long)
{
	SharedMemBase()[0x7222] = 1;
}

/* .text+0x3ac310, 654 bytes. Real body: for `type` in {4}, unconditionally
 * write a fixed shared-memory byte; for `type` in {0,1,2,3}, call
 * RT_pe_rand_capture(type, &tmp) then splat the resulting 32-bit value's
 * 4 bytes (big-endian order: bits 24-31,16-23,8-15,0-7) across BOTH the
 * live per-module record (at a per-type fixed offset) AND the shared
 * memory blob (at a DIFFERENT, also per-type fixed offset) before
 * looping back to the type==4 tail write. Every (module-record offset,
 * shared-memory offset) pair below was read directly off the real
 * disassembly for that exact `type` value, not inferred from a pattern. */
void CKGEngine::DoRandomCapture(long type)
{
	if (type == 4) {
		SharedMemBase()[0x7222] = 1;
		return;
	}

	long value = 0;
	unsigned int recOff, sharedOff;
	switch (type) {
	case 0: recOff = 0x1a; sharedOff = 0x90b4; break;
	case 1: recOff = 0x302; sharedOff = 0x939c; break;
	case 2: recOff = 0x5ea; sharedOff = 0x9684; break;
	case 3: recOff = 0x8d2; sharedOff = 0x996c; break;
	default: return;
	}
	RT_pe_rand_capture((unsigned char)type, &value);

	unsigned char *rec = m_currentModule + recOff;
	unsigned char *shared = SharedMemBase() + sharedOff;
	rec[0] = (unsigned char)(value >> 24);
	shared[0] = (unsigned char)(value >> 24);
	rec[1] = (unsigned char)(value >> 16);
	shared[1] = (unsigned char)(value >> 16);
	rec[2] = (unsigned char)(value >> 8);
	shared[2] = (unsigned char)(value >> 8);
	rec[3] = (unsigned char)value;
	shared[3] = (unsigned char)value;

	SharedMemBase()[0x7222] = 1;
}

/* .text+0x3ac660, 32 bytes. */
void CKGEngine::UpdateRTCDisplay(int value)
{
	m_rtcDisplayValue = value;
	KS_update_rtc_display_value((unsigned char)value);
}

/* .text+0x3ac680, 80 bytes. */
void CKGEngine::UpdateRTCModelName(int module)
{
	if (module >= m_numModules)
		return;
	short typeId = *(short *)(m_currentModule + (unsigned int)module * 0x2e8);
	char *outName = (char *)(SharedMemBase() + 0x14330);
	KS_get_rtcm_name_for_ge(typeId, outName);
	((CKGUIMsgSender *)(CKGUIMsgProcessor::ms_poInstance + 0x5c))->UpdateRTCModelName();
}

/* .text+0x3ac6d0, 208 bytes. 3 real memcpy segments into the shared
 * memory blob -- see this file's own header comment for how the GCC
 * inline-memcpy expansion was decoded back into (dst,src,count). The
 * 3rd (per-module GE-record) segment only runs when m_numModules==4,
 * confirmed via the real body's own explicit `cmp ...,4` guard. */
void CKGEngine::CopyCurrentParameterToSharedMemory()
{
	unsigned char *shared = SharedMemBase();
	__builtin_memcpy(shared + 0x8e9c, m_currentCommon, 0x1fe);
	__builtin_memcpy(shared + 0x909a, m_currentModule, (unsigned int)m_numModules * 0x2e8);
	if (m_numModules == 4) {
		for (int i = 0; i < m_numModules; i++) {
			__builtin_memcpy(shared + (unsigned int)i * 0x154 + 0x6dd0,
					  m_currentModule + (unsigned int)i * 0x2e8 + 0x294, 0x50);
		}
	}
}

/* .text+0x3ac7a0, 240 bytes. Captures the CURRENTLY-EDITED live
 * common/module record's own +4..+0x1c (0x1c bytes starting at +4) into
 * the matching Seq-backup slot -- confirmed direction via the real
 * disassembly's own read-from/write-to registers (reads m_currentCommon/
 * m_currentModule, writes the GetSeqKarmaPerf{Common,Module}() result),
 * NOT the other way around. `module==0` targets the Common record;
 * `module!=0` targets that module's own per-timbre record, indexed via
 * CKGRTCHandler::GetDestinationModule(module) rather than `module`
 * directly (confirmed, not assumed -- the two are NOT interchangeable
 * elsewhere in this class either). */
void CKGEngine::DoAutoAssignRTName(int module)
{
	if (!KS_rtc_auto_assign_names(module, 0))
		return;

	CopyCurrentParameterToSharedMemory();
	SharedMemBase()[0x7222] = 1;
	if (m_perfType != 2)
		return;

	unsigned char *bankMgr = CKGBankManager::ms_poInstance;
	unsigned int seqIndex = *(unsigned int *)(bankMgr + 0x97c7d4);
	unsigned char *src, *dst;

	if (module != 0) {
		int destModule = ((CKGRTCHandler *)CKGRTCHandler::ms_poInstance)->GetDestinationModule(module);
		unsigned int off = (unsigned int)destModule * 0x2e8;
		src = m_currentModule + off + 0x128;
		dst = ((CKGBankManager *)CKGBankManager::ms_poInstance)->GetSeqKarmaPerfModule(seqIndex) + off + 0x128;
	} else {
		src = m_currentCommon + 4;
		dst = ((CKGBankManager *)CKGBankManager::ms_poInstance)->GetSeqKarmaPerfCommon(seqIndex) + 4;
	}
	__builtin_memcpy(dst, src, 0x1c);

	((CKGBankManager *)CKGBankManager::ms_poInstance)->ResetKarmaPerfForSeq();
}

/* .text+0x3ac890, 32 bytes. */
void CKGEngine::DoCurrentDump()
{
	CopyCurrentParameterToSharedMemory();
	SharedMemBase()[0x7221] = 1;
}

/* .text+0x3ac8b0, 32 bytes. */
void CKGEngine::DoCompare()
{
	CopyCurrentParameterToSharedMemory();
	SharedMemBase()[0x7221] = 1;
}

/* .text+0x3ac8d0, 73 bytes. */
void CKGEngine::WritePerformance()
{
	KS_pe_write();
	CopyCurrentParameterToSharedMemory();
	((CKGRTCHandler *)CKGRTCHandler::ms_poInstance)->ChangePerformance();
	((CKGBankManager *)CKGBankManager::ms_poInstance)->RenewBackupKarmaPerf((eSTGMsgPerfType)m_perfType);
}

/* .text+0x3acd70, 48 bytes. */
bool CKGEngine::IsKarmaOn()
{
	unsigned char *bankMgr = CKGBankManager::ms_poInstance;
	if (bankMgr[0x97c7bb] != 0)
		return false;
	unsigned char *common = m_currentCommon;
	if (!common)
		return false;
	return (signed char)common[2] < 0;
}

/* .text+0x3acda0, 48 bytes. */
bool CKGEngine::IsTimbreZoneThru(int module)
{
	if (module >= m_numModules)
		return true;
	unsigned char *rec = m_currentModule + (unsigned int)module * 0x2e8;
	return (rec[0x126] >> 1) & 1;
}

/* .text+0x3acdd0, 16 bytes. */
int CKGEngine::GetLocalControllerChannel()
{
	return m_globalChannel;
}

/* .text+0x3acde0, 35 bytes. */
bool CKGEngine::IsTimbreThruParam(int module)
{
	if (module >= m_numModules)
		return true;
	unsigned char *rec = m_currentModule + (unsigned int)module * 0x2e8;
	return (rec[0x126] >> 2) & 1;
}

/* .text+0x3ace10, 32 bytes. */
bool CKGEngine::IsTimbreThruInternalAction(int channel)
{
	return KS_get_timbre_thru((unsigned char)channel);
}

/* .text+0x3ace30, 16 bytes. */
int CKGEngine::GetNumOfModule()
{
	return m_numModules;
}

/* .text+0x3ace40, 64 bytes. Real per-module "input channel" byte at
 * +0x2, sentinel 0x10 -> fall back to m_globalChannel. `module>=
 * m_numModules` reads directly off m_currentModule[+0x2] with no
 * per-module offset (i.e. module 0's own record), same real quirk
 * GetRealOutputChannel() below shares. */
int CKGEngine::GetRealInputChannel(int module)
{
	unsigned char *rec = (module < m_numModules)
		? m_currentModule + (unsigned int)module * 0x2e8
		: m_currentModule;
	int channel = rec[2] & 0x1f;
	return (channel == 0x10) ? m_globalChannel : channel;
}

/* .text+0x3ace80, 48 bytes. */
int CKGEngine::GetRealOutputChannel(int module)
{
	unsigned char *rec = (module < m_numModules)
		? m_currentModule + (unsigned int)module * 0x2e8
		: m_currentModule;
	int channel = rec[3];
	return (channel == 0x10) ? m_globalChannel : channel;
}

/* .text+0x3aceb0, 64 bytes. */
int CKGEngine::GetRealInputLocalControllerChannel(int module)
{
	if (m_perfType == 1)
		return -1;
	if (module >= m_numModules)
		return -1;
	unsigned char *rec = m_currentModule + (unsigned int)module * 0x2e8;
	if ((signed char)rec[2] >= 0)
		return -1;
	return m_globalChannel;
}

/* .text+0x3acef0, 176 bytes. If m_field0!=0 (still mid-Initialize()), do
 * nothing at all. Otherwise, if `CKGBankManager::ms_poInstance[0x20]`
 * (a plain raw-offset byte, single-dereference -- NOT the shared blob)
 * is clear, forward through CKGMIDIMsgProcessor::
 * ProcessTimbreThruChannelMessage(); if set, forward as a real
 * RT_channel_in() 5-arg message instead, whose own first argument is
 * `(CSKMIDIMsgProcessor::ms_poInstance[0x18] == 0)` -- confirmed via the
 * real disassembly's own register flow, not a guessed arg order. */
void CKGEngine::SendChannelMessage(unsigned char statusType, unsigned char channel,
				    signed char data1, signed char data2)
{
	if (m_field0 != 0)
		return;

	unsigned char *bankMgr = CKGBankManager::ms_poInstance;
	if (bankMgr[0x20] == 0) {
		((CKGMIDIMsgProcessor *)CKGMIDIMsgProcessor::ms_poInstance)
			->ProcessTimbreThruChannelMessage((int)statusType, channel, (char)data1, (char)data2, false);
		return;
	}

	bool localOff = (CSKMIDIMsgProcessor::ms_poInstance[0x18] == 0);
	RT_channel_in((short)localOff, (short)statusType, (short)channel, (short)data1, (short)data2);
}

/* .text+0x3ad1c0, 16 bytes. */
void CKGEngine::SendShutUp()
{
	RT_stop_and_rpt_damp();
}

/* .text+0x3ad1d0, 16 bytes. */
void CKGEngine::ClearScheduler()
{
	KS_clear_scheduler();
}

/* .text+0x3ad360, 64 bytes. */
void CKGEngine::Idle()
{
	CheckAndSendTimbreBendRange();
	ms_poKGTimerManager->Process();
	if (m_field14 == 4)
		SchedulerTask();
	((CKGEventDisplayManager *)ms_poKGEventDisplayManager)->Idle();
}

/* .text+0x3ad3a0, 32 bytes. */
void CKGEngine::SetBendRange(int module, unsigned int lo, unsigned int hi)
{
	m_bendRangeLo[module] = (int)lo;
	m_bendRangeHi[module] = (int)hi;
	m_bendRangeDirty = 1;
}

/* .text+0x3ad3c0, 96 bytes. True iff KARMA modules 0,1,2,3 are all
 * confirmed not-running -- any one still running short-circuits to
 * false immediately. */
bool CKGEngine::HaveAllModulesStopped()
{
	if (KS_is_module_running(0)) return false;
	if (KS_is_module_running(1)) return false;
	if (KS_is_module_running(2)) return false;
	return !KS_is_module_running(3);
}

/* .text+0x3ad420, 64 bytes. Real body: unconditionally sends a fixed
 * 5-byte SysEx-shaped MIDI message (0xb0,0x79,0x04,0x05,0xff) to the
 * sound engine -- literal bytes read directly off the real disassembly's
 * own immediate `mov BYTE PTR` writes, not a guessed template. */
void CKGEngine::NotifyEndProcessPerformanceChangeOfSTG()
{
	unsigned char msg[5] = { 0xb0, 0x79, 0x04, 0x05, 0xff };
	KGOutGate_SendToSoundEngine(msg, 5);
}

/* .text+0x3ad560, 64 bytes. */
void CKGEngine::ReceiveDisableMIDIInput(unsigned char *buf, int len)
{
	RT_sysex_in(buf, len);
	m_field10 = buf[6];
}

/* .text+0x3ad5a0, 160 bytes. */
bool CKGEngine::ShouldForceTimbreZoneBypass(int channel, int flagsChannel)
{
	if (m_field10 == 0)
		return false;
	if (flagsChannel != 2) {
		if (flagsChannel != 0)
			return false;
	}
	unsigned char *bankMgr = CKGBankManager::ms_poInstance;
	if (bankMgr[0x97c7bb] != 0)
		return false;
	unsigned char *common = m_currentCommon;
	if (!common)
		return false;
	if ((signed char)common[2] >= 0)
		return false;

	for (int m = 0; m < m_numModules; m++) {
		unsigned char *rec = m_currentModule + (unsigned int)m * 0x2e8;
		int outCh = rec[3];
		if (outCh == 0x10)
			outCh = m_globalChannel;
		if (outCh == channel && (rec[0x126] & 2))
			return true;
	}
	return false;
}

/* .text+0x3ad650, 32 bytes. */
bool CKGEngine::ShouldKeepKarmaPerformance()
{
	unsigned char *bankMgr = CKGBankManager::ms_poInstance;
	if (bankMgr[0x97c7bd] != 0)
		return false;
	return m_perfType == 1;
}

/* .text+0x3ae130, 96 bytes. `module` is scaled by 0x780 and `index` by
 * 0x3c into the shared memory blob's own name-string table (base
 * +0x90); no-op if the shared blob pointer itself is NULL. */
void CKGEngine::SetGERTParmName(int module, int index)
{
	unsigned char *shared = SharedMemBase();
	if (!shared)
		return;
	char *out = (char *)(shared + (unsigned int)module * 0x780 + (unsigned int)index * 0x3c + 0x90);
	KS_get_rtp_name_string((unsigned char)index, (unsigned char)module, out, 1);
}

/* .text+0x3ae660, 96 bytes. Status bytes 0xf8/0xf9 (SendRealTimeMIDIMessage
 * treats `status-0x6..0x7` as "clock-family") forward straight to
 * RT_real_time_in(); 0xfc (Stop) additionally stops the KARMA timer sync
 * and resets the MIDI chord trigger. */
void CKGEngine::SendRealTimeMIDIMessage(unsigned char status)
{
	RT_real_time_in((short)status);
	unsigned char rel = (unsigned char)(status + 6);
	if (rel <= 1) {
		ms_poKGTimerManager->StartSync();
		return;
	}
	if (status == 0xfc) {
		ms_poKGTimerManager->StopSync();
		((CKGRTCHandler *)CKGRTCHandler::ms_poInstance)->ResetMIDIChordTrigger();
	}
}

/* .text+0x3ae6c0, 32 bytes. */
void CKGEngine::SendSongPositionPointer(int position)
{
	RT_spp_in((short)(position & 0x7f), (short)((position >> 7) & 0x7f));
}

/* .text+0x3ae6e0, 16 bytes. */
void CKGEngine::SendEnterPrecount()
{
	KS_start_precount();
}

/* .text+0x3ae6f0, 32 bytes. */
void CKGEngine::KarmaTurnOffWhenStartDump()
{
	RT_karma_on(0);
	((CKGMIDIMsgProcessor *)CKGMIDIMsgProcessor::ms_poInstance)->ResetKarmaGeneratedCCValue();
}

/* .text+0x3ae710, 64 bytes. Real body dereferences m_currentCommon
 * directly with NO null check (unlike IsKarmaOn(), which does check) --
 * transcribed exactly, not defensively guarded, since by the time this
 * fires m_currentCommon is always already set (post-Initialize()). */
void CKGEngine::KarmaTurnOnWhenFinishDump()
{
	bool wasOn = (signed char)m_currentCommon[2] < 0;
	if (!wasOn) {
		unsigned char *bankMgr = CKGBankManager::ms_poInstance;
		if (bankMgr[0x97c7bb] == 0)
			RT_karma_on(1);
	}
	KS_reset_sst(false);
}

/* .text+0x3ae750, 48 bytes. Same no-null-check real quirk as
 * KarmaTurnOnWhenFinishDump() above. */
void CKGEngine::SendCCOffsetBack()
{
	bool karmaOn = (signed char)m_currentCommon[2] < 0;
	if (!karmaOn) {
		unsigned char *bankMgr = CKGBankManager::ms_poInstance;
		if (bankMgr[0x97c7bb] == 0)
			RT_karma_on(1);
	}
}

/* .text+0x3ae780, 48 bytes. */
void CKGEngine::ResetLocalController()
{
	((CSKMIDIMsgProcessor *)CSKMIDIMsgProcessor::ms_poInstance)->TrunAllNotesFromKeyboardOff();
	((CKGRTCHandler *)CKGRTCHandler::ms_poInstance)->ResetMIDIChordTrigger();
	RT_stop_and_rpt_damp();
}

/* .text+0x3ae7b0, 64 bytes. */
void CKGEngine::ResetAllRTC()
{
	KS_rtc_revert_all_buffers();
	CopyCurrentParameterToSharedMemory();
	SKSTGGate_NotifyKarmaAllSlidersPosition();
	((CKGUIMsgSender *)(CKGUIMsgProcessor::ms_poInstance + 0x5c))->ResetValuesInControlBuffer(0);
}

/* .text+0x3ae7f0, 80 bytes. */
void CKGEngine::ResetOneBuffer(int select)
{
	KS_rtc_revert_one_buffer(select);
	CopyCurrentParameterToSharedMemory();
	SKSTGGate_NotifyKarmaAllSlidersPosition();
	((CKGUIMsgSender *)(CKGUIMsgProcessor::ms_poInstance + 0x5c))->ResetValuesInControlBuffer(select);
}

/* .text+0x3ae840, 63 bytes. */
void CKGEngine::CompareScene(int select, int scene, bool arg3)
{
	KS_rtc_compare_one_scene(select, (unsigned char)scene, arg3);
	CopyCurrentParameterToSharedMemory();
	SKSTGGate_NotifyKarmaAllSlidersPosition();
	((CKGUIMsgSender *)(CKGUIMsgProcessor::ms_poInstance + 0x5c))->ResetCurrentScene();
}

/* .text+0x3ae880, 80 bytes. `select` for a slider is always
 * RTParmBufferSelect literal 1 in the real body; `module` is derived
 * from CKGRTCHandler::ms_poInstance[+0xdc] & 7 (the current scene
 * number, same field oa_ckg_switch_family.h's own CKGSceneSw::
 * GetCurrentValue() already reads). */
void CKGEngine::ResetKRTCSlider(int ccNumber)
{
	unsigned char module = CKGRTCHandler::ms_poInstance[0xdc] & 7;
	KS_rtc_compare_one_control(1, module, (unsigned char)ccNumber, true);
	CopyCurrentParameterToSharedMemory();
}

/* .text+0x3ae8d0, 80 bytes. */
void CKGEngine::ResetKRTCSwitch(int ccNumber)
{
	unsigned char module = CKGRTCHandler::ms_poInstance[0xdc] & 7;
	KS_rtc_compare_one_control(1, module, (unsigned char)ccNumber, true);
	CopyCurrentParameterToSharedMemory();
}

/* .text+0x3ae920, 24 bytes. */
void CKGEngine::OpenGECategoryPopup()
{
	m_geCategoryPopupOpen = 1;
	m_geCategoryPopupModule = 4;
}

/* .text+0x3aed70, 128 bytes. No-op unless the popup is open AND still on
 * its "no module selected" sentinel (4) -- once a module has been picked
 * the saved snapshot is never overwritten again by this method. Snapshot
 * is the per-module record's own +0x294..+0x2e4 (0x50-byte) tail, the
 * SAME real field range CopyCurrentParameterToSharedMemory()'s own 3rd
 * segment already establishes as this record's final block. */
void CKGEngine::CheckAndStoreModifiedStateWhenOpenGECategoryPopup(int module)
{
	if (!m_geCategoryPopupOpen || m_geCategoryPopupModule != 4)
		return;
	m_geCategoryPopupModule = module;
	unsigned char *rec = m_currentModule + (unsigned int)module * 0x2e8;
	__builtin_memcpy(m_geCategoryBackup, rec + 0x294, 0x50);
}

/* .text+0x3aedf0, 144 bytes. For every module whose own real "type" word
 * falls in [a,b], forward its GE-category change into shared memory --
 * confirmed via the real body's own signed range check (`type>=a &&
 * type<=b`, both inclusive) before calling SendChangeGEToEngine(). */
void CKGEngine::UpdateUserGE(int a, int b)
{
	unsigned char *bankMgr = CKGBankManager::ms_poInstance;
	bool isKorgX2100 = (*(unsigned int *)bankMgr == ToU32(bankMgr) + 0x385522);
	/* Real body SKIPS the per-module loop when the active buffer IS the
	 * built-in KorgX2100 template (the `je` at this exact comparison
	 * jumps FORWARD past the loop, not into it) -- the template's own
	 * GE assignments aren't user-editable. */
	if (!isKorgX2100) {
		for (int m = 0; m < m_numModules; m++) {
			short type = *(short *)(m_currentModule + (unsigned int)m * 0x2e8);
			if (type >= a && type <= b)
				SendChangeGEToEngine(m, type, true);
		}
	}
	((CKGBankManager *)CKGBankManager::ms_poInstance)->SetGECategoryToSharedMemory(a, b);
}
