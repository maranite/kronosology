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

/* .text+0x3ac5b0, 164 bytes. Same per-type math as DoRandomCapture()'s
 * own unrolled switch, parameterized instead of unrolled -- a separate
 * real entry point (does not call, and is not called by,
 * DoRandomCapture() itself; no type==4/SharedMemBase()[0x7222] tail
 * here). */
void CKGEngine::DoRandomCaptureExec(int arg)
{
	long value = 0;
	RT_pe_rand_capture((unsigned char)arg, &value);

	unsigned int off = (unsigned int)arg * 0x2e8;
	unsigned char *rec = m_currentModule + off + 0x1a;
	unsigned char *shared = SharedMemBase() + off + 0x90b4;
	rec[0] = shared[0] = (unsigned char)(value >> 24);
	rec[1] = shared[1] = (unsigned char)(value >> 16);
	rec[2] = shared[2] = (unsigned char)(value >> 8);
	rec[3] = shared[3] = (unsigned char)value;
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

/* .text+0x3ad460, 256 bytes. Sets CKGMIDIMsgProcessor::ms_poInstance's
 * m_bSuspended, fires all 6 RT_midi_filt_in_* free functions for
 * channels 1,2,3 (arg2 always 0), then clears m_bSuspended -- literally
 * the same 18-call block embedded inline inside ChangePerformance()'s
 * own combi/song-mode setup path (still deferred separately), recognized
 * here as a real standalone entry point via its own distinct symbol. */
void CKGEngine::SetMIDIFilterForUnusedModules()
{
	CKGMIDIMsgProcessor *proc = (CKGMIDIMsgProcessor *)CKGMIDIMsgProcessor::ms_poInstance;
	proc->m_bSuspended = 1;
	for (unsigned char channel = 1; channel <= 3; channel++) {
		RT_midi_filt_in_tch(channel, 0);
		RT_midi_filt_in_bnd(channel, 0);
		RT_midi_filt_in_sus(channel, 0);
		RT_midi_filt_in_cc1(channel, 0);
		RT_midi_filt_in_cc2(channel, 0);
		RT_midi_filt_in_ctl(channel, 0);
	}
	proc->m_bSuspended = 0;
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

/* .text+0x3ad670, 467 bytes. For each of the 8 "PERT" (per-timbre RT
 * param) table slots (18-byte records at SharedMemBase()+idx*0x12),
 * re-derive a sorted (min,max) word pair from KS_get_rtd_min_pe/
 * KS_get_rtd_max_pe at +0x0/+0x2, a sorted (min,max) word pair from
 * KS_get_rtp_min_pe/KS_get_rtp_max_pe at +0x4/+0x6, and (unless
 * KS_get_rtp_bank_menu_pe(idx)==1, which instead force-sets all 4
 * "enabled" bytes to 1 and leaves the control bytes untouched) 4 per-bit
 * control/enabled byte pairs at +0xa+bit/+0xe+bit derived from
 * KS_get_rtp_enabled_bits(idx)'s own bit: clear -> control=0,enabled=1;
 * set -> control=(KS_get_rtp_multi_id_pe(idx)'s own bit)?1:0, enabled=0.
 * Re-fetches SharedMemBase() TWICE per idx (once for the rtd/rtp block,
 * once again for the bank_menu/control-byte block) and skips that
 * section entirely if that particular read is NULL -- transcribed
 * exactly, not simplified/hoisted to one read, since a genuine
 * data-race window can't be ruled out. Same record layout and control/
 * enabled-byte idiom reused by SetPERTParmMinMax()/
 * SetPERTParmControlModule() below for a single caller-supplied idx. */
void CKGEngine::RefreshPERTParmInfo()
{
	for (int idx = 0; idx < 8; idx++) {
		unsigned char idxb = (unsigned char)idx;
		unsigned char *shared = SharedMemBase();
		if (shared) {
			unsigned char *rec = shared + (unsigned int)idx * 0x12;

			short rtdMin = KS_get_rtd_min_pe(idxb);
			short rtdMax = KS_get_rtd_max_pe(idxb);
			*(short *)(rec + 0x0) = (rtdMin > rtdMax) ? rtdMax : rtdMin;
			*(short *)(rec + 0x2) = (rtdMin > rtdMax) ? rtdMin : rtdMax;

			short rtpMin = KS_get_rtp_min_pe(idxb);
			short rtpMax = KS_get_rtp_max_pe(idxb);
			*(short *)(rec + 0x4) = (rtpMin > rtpMax) ? rtpMax : rtpMin;
			*(short *)(rec + 0x6) = (rtpMin > rtpMax) ? rtpMin : rtpMax;
		}

		unsigned char enabledBits = KS_get_rtp_enabled_bits(idxb);
		shared = SharedMemBase();
		if (!shared)
			continue;
		unsigned char *rec = shared + (unsigned int)idx * 0x12;

		if (KS_get_rtp_bank_menu_pe(idxb) == 1) {
			rec[0xe] = 1;
			rec[0xf] = 1;
			rec[0x10] = 1;
			rec[0x11] = 1;
			continue;
		}

		for (int bit = 0; bit < 4; bit++) {
			unsigned char control, enabled;
			if (enabledBits & (1 << bit)) {
				unsigned char multiId = KS_get_rtp_multi_id_pe(idxb);
				control = (multiId & (1 << bit)) ? 1 : 0;
				enabled = 0;
			} else {
				control = 0;
				enabled = 1;
			}
			rec[0xa + bit] = control;
			rec[0xe + bit] = enabled;
		}
	}
}

/* .text+0x3ad860, 192 bytes. Same sorted-(min,max)-pair idiom as the
 * rtd/rtp half of RefreshPERTParmInfo() above, for a single
 * caller-supplied idx; single up-front SharedMemBase() NULL check
 * gates the whole method. */
void CKGEngine::SetPERTParmMinMax(int a)
{
	unsigned char *shared = SharedMemBase();
	if (!shared)
		return;
	unsigned char idxb = (unsigned char)a;
	unsigned char *rec = shared + (unsigned int)a * 0x12;

	short rtdMin = KS_get_rtd_min_pe(idxb);
	short rtdMax = KS_get_rtd_max_pe(idxb);
	*(short *)(rec + 0x0) = (rtdMin > rtdMax) ? rtdMax : rtdMin;
	*(short *)(rec + 0x2) = (rtdMin > rtdMax) ? rtdMin : rtdMax;

	short rtpMin = KS_get_rtp_min_pe(idxb);
	short rtpMax = KS_get_rtp_max_pe(idxb);
	*(short *)(rec + 0x4) = (rtpMin > rtpMax) ? rtpMax : rtpMin;
	*(short *)(rec + 0x6) = (rtpMin > rtpMax) ? rtpMin : rtpMax;
}

/* .text+0x3ad920, 352 bytes. Same enabled-bits-gated control/enabled
 * byte-pair idiom as the tail of RefreshPERTParmInfo() above, for a
 * single caller-supplied idx. KS_get_rtp_enabled_bits(idx) is called
 * BEFORE the SharedMemBase() NULL check (its side effect always
 * happens even if the record write is skipped); the bank_menu/multi_id
 * calls are gated behind the NULL check. */
void CKGEngine::SetPERTParmControlModule(int a)
{
	unsigned char idxb = (unsigned char)a;
	unsigned char enabledBits = KS_get_rtp_enabled_bits(idxb);
	unsigned char *shared = SharedMemBase();
	if (!shared)
		return;
	unsigned char *rec = shared + (unsigned int)a * 0x12;

	if (KS_get_rtp_bank_menu_pe(idxb) == 1) {
		rec[0xe] = 1;
		rec[0xf] = 1;
		rec[0x10] = 1;
		rec[0x11] = 1;
		return;
	}

	for (int bit = 0; bit < 4; bit++) {
		unsigned char control, enabled;
		if (enabledBits & (1 << bit)) {
			unsigned char multiId = KS_get_rtp_multi_id_pe(idxb);
			control = (multiId & (1 << bit)) ? 1 : 0;
			enabled = 0;
		} else {
			control = 0;
			enabled = 1;
		}
		rec[0xa + bit] = control;
		rec[0xe + bit] = enabled;
	}
}

/* .text+0x3ada80, 288 bytes. GE ("GERT") sibling of SetPERTParmMinMax()
 * above -- writes into a per-(module,ge) record at SharedMemBase()+
 * ge*0x3c+module*0x780 (32 ge-slots per module, 0x780=0x20*0x3c):
 * KS_get_rte_val_ge/min_ge/max_ge at +0xc0+8/+4/+6 (display=0) and
 * +0x1ec0+8/+4/+6 (display=1), plus a sorted (min,max) word pair from
 * KS_get_rtd_min_ge/KS_get_rtd_max_ge at +0xc0+0/+2. Whole method
 * no-ops if SharedMemBase() is NULL. */
void CKGEngine::SetGERTParmMinMax(int a, int b)
{
	unsigned char *shared = SharedMemBase();
	if (!shared)
		return;

	unsigned char m = (unsigned char)a;
	unsigned char g = (unsigned char)b;
	unsigned char *recBase = shared + (unsigned int)b * 0x3c + (unsigned int)a * 0x780;
	unsigned char *block0 = recBase + 0xc0;
	unsigned char *block1 = recBase + 0x1ec0;

	*(short *)(block0 + 0x8) = KS_get_rte_val_ge(m, 0, g);
	*(short *)(block0 + 0x4) = KS_get_rte_min_ge(m, 0, g);
	*(short *)(block0 + 0x6) = KS_get_rte_max_ge(m, 0, g);

	*(short *)(block1 + 0x8) = KS_get_rte_val_ge(m, 1, g);
	*(short *)(block1 + 0x4) = KS_get_rte_min_ge(m, 1, g);
	*(short *)(block1 + 0x6) = KS_get_rte_max_ge(m, 1, g);

	short rtdMin = KS_get_rtd_min_ge(m, g);
	short rtdMax = KS_get_rtd_max_ge(m, g);
	*(short *)(block0 + 0x0) = (rtdMin > rtdMax) ? rtdMax : rtdMin;
	*(short *)(block0 + 0x2) = (rtdMin > rtdMax) ? rtdMin : rtdMax;
}

/* .text+0x3adba0, 144 bytes. For every (module,ge) pair (module in
 * [0,m_numModules), ge in [0,0x20)): if SharedMemBase() is non-NULL,
 * calls KS_get_rtp_name_string(module,ge,SharedMemBase()+module*0x780+
 * ge*0x3c+0x90,1) for its side effect (writes the GE's own display name
 * into that shared-memory slot); then unconditionally calls
 * SetGERTParmMinMax(module,ge). Identical inner double-loop body reused
 * verbatim inside SendChangeGEToEngine() below. */
void CKGEngine::RefreshGERTParmInfo()
{
	if (m_numModules <= 0)
		return;

	for (int module = 0; module < m_numModules; module++) {
		for (int ge = 0; ge < 0x20; ge++) {
			unsigned char *shared = SharedMemBase();
			if (shared) {
				char *nameOut = (char *)(shared + (unsigned int)module * 0x780
							  + (unsigned int)ge * 0x3c + 0x90);
				KS_get_rtp_name_string((unsigned char)module, (unsigned char)ge, nameOut, 1);
			}
			SetGERTParmMinMax(module, ge);
		}
	}
}

/* .text+0x3adc30, 816 bytes. Real body, in execution order:
 *  1. If m_geCategoryPopupOpen and m_geCategoryPopupModule==4 (sentinel
 *     "no module cached yet"): snapshot the target module's own +0x294
 *     region (0x50 bytes) into m_geCategoryBackup and set
 *     m_geCategoryPopupModule=module. Always falls through afterward.
 *  2. Compute an "effective channel" for ResetKarmaGeneratedCCValue():
 *     the target module's own voiceModelType byte (record[+3]), read
 *     from m_currentModule+module*0x2e8 if module is in range, else
 *     from m_currentModule+0 (module 0's own record, unclamped `module`
 *     used everywhere else below) -- remapped to m_globalChannel when
 *     that byte==0x10.
 *  3. bankMgr->GetGenEffect(ge, module) -> genEffect pointer.
 *  4. Select a (loadOptions,loadKind) dword pair from SharedMemBase()
 *     at a m_perfType-selected offset pair (1->0x7224/0x7228,
 *     2->0x7234/0x7238, else->0x722c/0x7230); KS_set_ge_load_options()
 *     with loadOptions' low byte; then a real 3-way decode of
 *     (loadOptions!=0, loadKind) into (useRtcModel,resetScenes) booleans
 *     fed to KS_set_ge_load_use_rtc_model()/KS_set_ge_load_reset_scenes()
 *     -- loadOptions!=0 forces both false; else loadKind 1->(true,false),
 *     2->(false,true), 3->(true,true), other->(false,false).
 *  5. RT_ge_select(module, genEffect, 1).
 *  6. If m_perfType==2 AND SharedMemBase()[+0x7234] (re-read, a dword)
 *     != 2: dispatch to ChangeValuesInBackupWhenChangingGE(module,
 *     common, rec) (still deferred -- expected external symbol) using
 *     either GetSeqKarmaPerfModule/Common(bankMgr[+0x97c7d4]) when
 *     CKGUIMsgProcessor::ms_poInstance[+0x74]!=0, else
 *     GetSeqDefaultKarmaPerfModule/Common().
 *  7. Unconditional tail: CopyCurrentParameterToSharedMemory(), the SAME
 *     name-string+SetGERTParmMinMax double loop as
 *     RefreshGERTParmInfo() above, StoreGERTParmMinMaxToBank(), then
 *     (if arg3) CKGUIMsgSender::ChangeGE(module) before, always,
 *     SKSTGGate_NotifyKarmaAllSlidersPosition(); finally, if module is
 *     in range, KS_get_rtcm_name_for_ge()+CKGUIMsgSender::
 *     UpdateRTCModelName() (same idiom as UpdateRTCModelName() itself),
 *     and unconditionally KS_update_rtc_display_value(m_rtcDisplayValue). */
void CKGEngine::SendChangeGEToEngine(int module, int ge, bool arg3)
{
	if (m_geCategoryPopupOpen && m_geCategoryPopupModule == 4) {
		unsigned char *moduleRec = m_currentModule + (unsigned int)module * 0x2e8;
		m_geCategoryPopupModule = module;
		__builtin_memcpy(m_geCategoryBackup, moduleRec + 0x294, 0x50);
	}

	unsigned char voiceModelType = (module < m_numModules)
		? m_currentModule[(unsigned int)module * 0x2e8 + 3]
		: m_currentModule[3];
	int resetChannel = (voiceModelType == 0x10) ? m_globalChannel : module;
	((CKGMIDIMsgProcessor *)CKGMIDIMsgProcessor::ms_poInstance)->ResetKarmaGeneratedCCValue(resetChannel);

	unsigned char *bankMgr = CKGBankManager::ms_poInstance;
	GenEffect_pub *genEffect = (GenEffect_pub *)((CKGBankManager *)bankMgr)->GetGenEffect(ge, module);

	unsigned char *shared = SharedMemBase();
	unsigned int loSlot, hiSlot;
	switch (m_perfType) {
	case 1: loSlot = 0x7224; hiSlot = 0x7228; break;
	case 2: loSlot = 0x7234; hiSlot = 0x7238; break;
	default: loSlot = 0x722c; hiSlot = 0x7230; break;
	}
	unsigned int loadOptions = *(unsigned int *)(shared + loSlot);
	unsigned int loadKind = *(unsigned int *)(shared + hiSlot);

	KS_set_ge_load_options((unsigned char)loadOptions);

	bool useRtcModel, resetScenes;
	if (loadOptions != 0) {
		useRtcModel = false;
		resetScenes = false;
	} else if (loadKind == 1) {
		useRtcModel = true;
		resetScenes = false;
	} else if (loadKind == 2) {
		useRtcModel = false;
		resetScenes = true;
	} else if (loadKind == 3) {
		useRtcModel = true;
		resetScenes = true;
	} else {
		useRtcModel = false;
		resetScenes = false;
	}
	KS_set_ge_load_use_rtc_model(useRtcModel);
	KS_set_ge_load_reset_scenes(resetScenes);

	RT_ge_select((unsigned char)module, genEffect, 1);

	if (m_perfType == 2 && *(unsigned int *)(SharedMemBase() + 0x7234) != 2) {
		CKarmaPerfModule *seqModule;
		CKarmaPerfCommon *seqCommon;
		if (CKGUIMsgProcessor::ms_poInstance[0x74] != 0) {
			unsigned int seqIndex = *(unsigned int *)(bankMgr + 0x97c7d4);
			seqModule = (CKarmaPerfModule *)((CKGBankManager *)bankMgr)->GetSeqKarmaPerfModule(seqIndex);
			seqIndex = *(unsigned int *)(bankMgr + 0x97c7d4);
			seqCommon = (CKarmaPerfCommon *)((CKGBankManager *)bankMgr)->GetSeqKarmaPerfCommon(seqIndex);
		} else {
			seqModule = (CKarmaPerfModule *)((CKGBankManager *)bankMgr)->GetSeqDefaultKarmaPerfModule();
			seqCommon = (CKarmaPerfCommon *)((CKGBankManager *)bankMgr)->GetSeqDefaultKarmaPerfCommon();
		}
		ChangeValuesInBackupWhenChangingGE(module, seqCommon, seqModule);
	}

	CopyCurrentParameterToSharedMemory();
	for (int m = 0; m < m_numModules; m++) {
		for (int g = 0; g < 0x20; g++) {
			unsigned char *sh = SharedMemBase();
			if (sh) {
				char *nameOut = (char *)(sh + (unsigned int)m * 0x780 + (unsigned int)g * 0x3c + 0x90);
				KS_get_rtp_name_string((unsigned char)m, (unsigned char)g, nameOut, 1);
			}
			SetGERTParmMinMax(m, g);
		}
	}
	StoreGERTParmMinMaxToBank();

	if (arg3)
		((CKGUIMsgSender *)(CKGUIMsgProcessor::ms_poInstance + 0x5c))->ChangeGE(module);
	SKSTGGate_NotifyKarmaAllSlidersPosition();

	if (module < m_numModules) {
		unsigned char *sh = SharedMemBase();
		short typeId = *(short *)(m_currentModule + (unsigned int)module * 0x2e8);
		KS_get_rtcm_name_for_ge(typeId, (char *)(sh + 0x14330));
		((CKGUIMsgSender *)(CKGUIMsgProcessor::ms_poInstance + 0x5c))->UpdateRTCModelName();
	}
	KS_update_rtc_display_value((unsigned char)m_rtcDisplayValue);
}

/* .text+0x3adf60, 464 bytes. Snapshots the whole 0x2e8-byte module
 * record to a local stack buffer, calls SendChangeGEToEngine(module,
 * savedTypeId, false), overwrites the record from a template (a fixed
 * SharedMemBase()+0xb5c2 template when m_perfType==1, else a per-module
 * SharedMemBase()+module*0x2e8+0xbaa8 template), restores 3 preserved
 * fields (typeId word, voiceModelType byte, low-5-bits of the
 * program-number byte) from the pre-template snapshot, sets/clears bit
 * 0x20 of byte +0x126 from the snapshot's own value, then unconditionally
 * restores a 32-slot/6-word-per-slot "velocity zone" array (offsets
 * +0x20/+0x22/+0x24/+0x196/+0x198/+0x19a, stride 8) from the snapshot,
 * mirrors the rebuilt record out to SharedMemBase()+module*0x2e8+0x909a,
 * then marks SharedMemBase()[0x7222]=1. */
void CKGEngine::DoInitModule(int module)
{
	unsigned char *shared = SharedMemBase();
	unsigned char *moduleRec = m_currentModule + (unsigned int)module * 0x2e8;

	unsigned char backup[0x2e8];
	__builtin_memcpy(backup, moduleRec, 0x2e8);

	short savedTypeId = *(short *)(moduleRec + 0x0);
	unsigned char savedVoiceModelType = moduleRec[0x3];
	unsigned char savedProgramNumberBits = (unsigned char)(moduleRec[0x2] & 0x1f);
	bool savedFlagBit5 = (moduleRec[0x126] & 0x20) != 0;

	SendChangeGEToEngine(module, savedTypeId, false);

	if (m_perfType == 1)
		__builtin_memcpy(moduleRec, shared + 0xb5c2, 0x2e8);
	else
		__builtin_memcpy(moduleRec, shared + (unsigned int)module * 0x2e8 + 0xbaa8, 0x2e8);

	moduleRec[0x2] = (unsigned char)((moduleRec[0x2] & 0xe0) | savedProgramNumberBits);
	*(short *)(moduleRec + 0x0) = savedTypeId;
	moduleRec[0x3] = savedVoiceModelType;

	if (savedFlagBit5)
		moduleRec[0x126] |= 0x20;
	else
		moduleRec[0x126] &= 0xdf;

	for (int i = 0; i < 0x20; i++) {
		*(short *)(moduleRec + i * 8 + 0x20) = *(short *)(backup + i * 8 + 0x20);
		*(short *)(moduleRec + i * 8 + 0x22) = *(short *)(backup + i * 8 + 0x22);
		*(short *)(moduleRec + i * 8 + 0x24) = *(short *)(backup + i * 8 + 0x24);
		*(short *)(moduleRec + i * 8 + 0x196) = *(short *)(backup + i * 8 + 0x196);
		*(short *)(moduleRec + i * 8 + 0x198) = *(short *)(backup + i * 8 + 0x198);
		*(short *)(moduleRec + i * 8 + 0x19a) = *(short *)(backup + i * 8 + 0x19a);
	}

	__builtin_memcpy(shared + (unsigned int)module * 0x2e8 + 0x909a, moduleRec, 0x2e8);
	shared[0x7222] = 1;
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

/* .text+0x3ae190, 272 bytes. For each module 0..m_numModules-1, computes
 * the same per-module "effective channel" value FakeTimbreThru()/
 * CheckAndSendTimbreBendRange() use (still deferred separately): the
 * module's own voiceModelType byte (record[+3]), remapped to
 * m_globalChannel when ==0x10, is compared against either (a) the
 * record's own program-number-low-5-bits (record[+2]&0x1f), remapped to
 * m_globalChannel when THAT ==0x10, using ONLY that comparison and
 * nothing else if program-number-low-5-bits==0x10; or (b) if
 * program-number-low-5-bits!=0x10, first tried directly unremapped, then
 * (if that fails) against a fallback (-1 if m_perfType==1 OR
 * record[+2]'s raw byte is non-negative as signed, else m_globalChannel).
 * Every module whose resulting "effective channel" equals
 * m_globalChannel OR-accumulates its own record[+0x14] bit 0x20 into the
 * result. Result defaults to `true` both when any of 4 up-front guard
 * checks fail (CKGBankManager[+0x97c7bb] set, m_currentCommon NULL,
 * m_currentCommon[+2]>=0 as a SIGNED byte, or m_numModules<=0) AND when
 * the loop runs to completion but never finds a matching module.
 * Result fed straight to
 * KGOutGate_NotifyEnableDirectPathForVectorCCToSoundEngine(). */
void CKGEngine::UpdateEnableDirectPathForVectorCC()
{
	bool enable = true;

	unsigned char *bankMgr = CKGBankManager::ms_poInstance;
	if (bankMgr[0x97c7bb] == 0 && m_currentCommon != 0
	    && (signed char)m_currentCommon[2] < 0 && m_numModules > 0) {
		unsigned char lastVal = 0;
		bool matchedAny = false;
		unsigned char *rec = m_currentModule;

		for (int i = 0; i < m_numModules; i++, rec += 0x2e8) {
			unsigned char rawByte2 = rec[2];
			int c = rec[3];
			if (c == 0x10)
				c = m_globalChannel;

			unsigned char progLow5 = (unsigned char)(rawByte2 & 0x1f);
			bool matched;
			if (progLow5 == 0x10) {
				matched = (c == m_globalChannel);
			} else if (c == (int)progLow5) {
				matched = true;
			} else {
				int fallback;
				if (m_perfType == 1)
					fallback = -1;
				else if ((signed char)rawByte2 >= 0)
					fallback = -1;
				else
					fallback = m_globalChannel;
				matched = (c == fallback);
			}

			if (matched && c == m_globalChannel) {
				bool bitSet = (rec[0x14] & 0x20) != 0;
				lastVal = bitSet ? 1 : lastVal;
				matchedAny = true;
			}
		}

		enable = matchedAny ? (lastVal != 0) : true;
	}

	KGOutGate_NotifyEnableDirectPathForVectorCCToSoundEngine(enable);
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
