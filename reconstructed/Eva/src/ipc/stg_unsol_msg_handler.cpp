/*
 * stg_unsol_msg_handler.cpp  -  see include/stg_unsol_msg_handler.h.
 *
 * Transcribed from Decomp/EVA_Decomp/eva_export/functions/:
 *   CSTGUnsolMsgHandler@0891c090.c        (ctor, 297 bytes)
 *   _CSTGUnsolMsgHandler@089b9e30.c       (ResetVTable, 11 bytes)
 *   _CSTGUnsolMsgHandler@089b9e40.c       (DeletingDtor, 39 bytes)
 *   HandleMessage@089162e0.c              (168 bytes)
 *   EndHandling@0891c290.c                (283 bytes)
 *   SendValueSlider@0891c1f0.c            (71 bytes)
 *   SendValueEncoder@0891c240.c           (66 bytes)
 *   EnterGlobalObjectEdit@0891c3c0.c      (10 bytes)
 *   Initialize@0891c1c0.c / InitializeForSong@0891c1d0.c / BeginHandling@0891c1e0.c
 *     (1 byte each -- real, confirmed-empty)
 *   TestControlMsgHandler@08916230.c / ASKMsgHandler@08916240.c /
 *     CalibrationMsgHandler@08916250.c / FrontPanelMsgHandler@08916260.c /
 *     KLMMsgHandler@08916270.c (1 byte each -- real, confirmed-empty)
 *
 * See stg_unsol_msg_handler.h for the full layout/Tier A/B breakdown.
 */

#include "stg_unsol_msg_handler.h"

#include <cstdlib>
#include <unistd.h>

/* USTGAPIControl -- not reconstructed elsewhere in this project. Declared file-local
 * (same convention as ckernel.cpp's own local CTracer) with only the two real static
 * methods EndHandling() needs. Real signatures confirmed from functions.csv:
 *   ForceErPShutdown(.text+0x08e1cbe0, 60 bytes)  cc=__cdecl, ushort param_1
 *   SaveRandomSeed(.text+0x08e1d090, 78 bytes)    cc=__cdecl, void
 * Both static (no `this`) -- not implemented (Tier-B call-contract extern; the real
 * bodies belong to a whole not-reconstructed class).
 */
class USTGAPIControl {
public:
	static void SaveRandomSeed();
	static void ForceErPShutdown(unsigned short code);
};

/* Tier-B link-stubs (not Tier A -- see class comment above): real signatures only,
 * empty bodies so the link succeeds, per this project's own Stage-4 tier convention
 * ("every symbol on the unresolved list got a real definition ... the linker needs
 * an actual symbol, not just a compatible declaration").
 */
void USTGAPIControl::SaveRandomSeed() { /* Tier-B link-stub. .text+0x08e1d090, 78 bytes. */ }
void USTGAPIControl::ForceErPShutdown(unsigned short) { /* Tier-B link-stub. .text+0x08e1cbe0, 60 bytes. */ }

CSTGUnsolMsgHandler *CSTGUnsolMsgHandler::sInstance = 0;

/* Real values, confirmed by direct raw-byte read of the binary's own .rodata
 * (readelf -l to map VA->file offset, then read 4 bytes): _DAT_08ea8534 = 1023.0f,
 * _DAT_08f29a40 = 127.0f. HandleMessage()/SendValueSlider() scale a 0..127 slider
 * value up to a 0..1023 range with these -- not asserted from the decompile's
 * opaque DAT_ name alone, actually read.
 */
static const float kAnalogScaleNumerator = 1023.0f;
static const float kAnalogScaleDenominator = 127.0f;

/* File-scope statics -- real globals, not CSTGUnsolMsgHandler instance fields
 * (confirmed: symbols.csv lists all 4 as plain "Global" namespace labels, not under
 * "CSTGUnsolMsgHandler"). Nothing reconstructed anywhere in this project writes
 * sNowValueSlider/sLastValueSlider/sEncoderValue to a nonzero value -- the real
 * producer (presumably inside ControlMsgHandler's own Tier-B-stubbed body) isn't
 * traced, so every slider/encoder-forwarding branch below is real but currently dead
 * given this pass's own data, same "faithful but unreached" license used throughout
 * this project (e.g. CScheduler::Exec()'s own bail branches).
 */
static int sNowValueSlider = 0;
static int sLastValueSlider = 0;
static int sEncoderValue = 0;
static int s_bIsInGlobalObjectEdit = 0;

/* --- Stage 6 batch 2 (2026-07-25): PatchMsgHandler/EffectMgrMsgHandler/
 * EffectMsgHandler/HDRTrackMsgHandler/SetListMsgHandler support data ------------
 *
 * CStorage -- a whole not-reconstructed class (symbols.csv shows dozens of
 * methods). Only its three "current selection" statics are needed here, real
 * addresses/sizes confirmed via symbols.csv's own mangled names:
 *   CStorage::sm_ucCurrentProg   0x0af30548 (1 byte)
 *   <unnamed byte>               0x0af30549 (1 byte, immediately adjacent -- real,
 *                                 read by every one of these handlers as a paired
 *                                 "sub-id" alongside sm_ucCurrentProg, but never
 *                                 independently named by any mangled symbol in this
 *                                 export -- kept as a plain DAT_ name rather than
 *                                 guessing a real member name)
 *   CStorage::sm_ucCurrentCombi  0x0af3054a (1 byte)
 *   <unnamed byte>               0x0af3054b (1 byte, same pairing as above)
 *   CStorage::sm_usCurrentSong   0x0af3054c (2 bytes, "us" = unsigned short)
 * All five live in the real binary's own huge bss segment (mutable runtime
 * selection state, not a compile-time constant) -- declared here as genuine
 * zero-initialized globals, same treatment as EditApi/s_eNowRestoreSeqParameters
 * below, not merely `extern` to a symbol this reconstruction doesn't define.
 */
class CStorage {
public:
	static unsigned char sm_ucCurrentProg;
	static unsigned char sm_ucCurrentCombi;
	static unsigned short sm_usCurrentSong;
};
unsigned char CStorage::sm_ucCurrentProg = 0;
unsigned char CStorage::sm_ucCurrentCombi = 0;
unsigned short CStorage::sm_usCurrentSong = 0;
unsigned char DAT_0af30549 = 0; /* paired with sm_ucCurrentProg, see above */
unsigned char DAT_0af3054b = 0; /* paired with sm_ucCurrentCombi, see above */

/* Real global byte flag (0x0af0df1e, bss), gates PatchMsgHandler's entire body on
 * `(DAT_0af0df1e & 7) == 3` -- purpose not traced (a mode/state byte read nowhere
 * else in this reconstruction), kept opaque per this project's own "confirm one
 * field, don't retype the whole struct" convention. Non-static (plain external
 * linkage) so verify/test_stg_unsol_msg_handler.cpp can drive it directly.
 */
unsigned char DAT_0af0df1e = 0;

/* Real global, confirmed via disassembly of every one of these five handlers'
 * `if (s_eNowRestoreSeqParameters != 0) call(EditApi_vtbl+0x3c)` / `+0x38`
 * bracket -- never set nonzero by anything reconstructed in this project (same
 * "faithful but currently dead branch" status as sNowValueSlider et al. above),
 * so kept `static` (no test needs to touch it).
 */
static int s_eNowRestoreSeqParameters = 0;

/* CEditor::lastEditMessage -- real global, `_ZN7CEditor15lastEditMessageE`,
 * 0x0939c1e0, confirmed 2 bytes (every real store is a 16-bit `mov WORD PTR
 * ...,0x500c`). UPDATE (Stage 6 CEditor batch, 2026-07-25): `CEditor` is now a
 * real class (editor.h), which already declares this as a `static` member --
 * this is just that member's qualified out-of-line definition, dropping the
 * former `namespace CEditor { ... }` wrapper (declared directly in the
 * namespace back when CEditor itself wasn't reconstructed as a class yet).
 */
unsigned short CEditor::lastEditMessage = 0;

/* CESSongTask::ms_bShouldDirectStorePMRStatus -- real static, gates a direct-store
 * mode around HDRTrackMsgHandler's own two-track (subtype 0xb/0xc) special case.
 * CESSongTask is a large not-reconstructed class (many real methods per
 * symbols.csv) -- only this one static is declared here, file-local, same
 * Tier-B-adjacent convention as USTGAPIControl above.
 */
class CESSongTask {
public:
	static unsigned char ms_bShouldDirectStorePMRStatus;
};
unsigned char CESSongTask::ms_bShouldDirectStorePMRStatus = 0;

/* Real local `static const` byte tables, each belonging to a different,
 * not-reconstructed free function in the real binary (Ghidra's own
 * `Function(Args)::s_akbyAP`-style qualified names) -- read directly out of the
 * real binary's .rodata (readelf -l VA->file-offset, then a raw byte read), NOT
 * transcribed from the decompile's opaque table reference alone. Each is a
 * `{code, value}` byte-pair table indexed by the message's own subtype/sub-index
 * field. Real addresses/spans confirmed via each mangled `_ZZ...s_akbyAP`
 * symbol's own address up to the next such symbol in symbols.csv -- NOT the
 * CSWTCH_NNN Ghidra-synthesized switch-table names, which are per-decompile
 * artifacts and not reliable global symbols (see CSWTCH_290/CSWTCH_231 note
 * below, confirmed instead via direct disassembly of the real load instruction's
 * immediate operand).
 */
static const unsigned char kHandleEffectLFOParam_s_akbyAP[16] = {
	0x13,0x00, 0x13,0x01, 0x13,0x02, 0x13,0x04, 0x13,0x03, 0x13,0x05, 0x13,0x06, 0x13,0x07,
}; /* HandleEffectLFOParam(STGEffectSlotMsg*)::s_akbyAP, 0x08f1bd3c, 16 bytes */

static const unsigned char kHandleHDRMsg_s_akbyAP[30] = {
	0x58,0x0b, 0x58,0x10, 0x58,0x0f, 0x58,0x12, 0x58,0x11, 0x58,0x13, 0x58,0x14,
	0x58,0x08, 0x58,0x09, 0x58,0x0a, 0x58,0x03, 0x6b,0x00, 0x6b,0x10, 0x58,0x0c, 0x58,0x0d,
}; /* HandleHDRMsg(STGHDRTrackMsg*)::s_akbyAP, 0x08f1bd00, 30 bytes */

static const unsigned char kSetListMsgHandler_s_akbyAPSlot[10] = {
	0x13,0x04, 0x13,0x05, 0x13,0x06, 0x13,0x07, 0x13,0x0b,
}; /* CSTGUnsolMsgHandler::SetListMsgHandler(STGMessage&)::s_akbyAPSlot, 0x08f1bcdc, 10 bytes */

static const unsigned char kSetListMsgHandler_s_akbyAP[26] = {
	0x01,0x00, 0x01,0x01, 0x01,0x02, 0x01,0x03, 0x01,0x04, 0x01,0x05, 0x01,0x06,
	0x01,0x07, 0x01,0x08, 0x01,0x09, 0x02,0x10, 0x02,0x11, 0x01,0x0a,
}; /* CSTGUnsolMsgHandler::SetListMsgHandler(STGMessage&)::s_akbyAP, 0x08f1bce6, 26 bytes */

/* CSWTCH_290 -- SetListMsgHandler's own switch-validity table. Ghidra's decompile
 * expressed this as `CSWTCH_290[iVar3 + 0xf]` (a table it chose to start 15 bytes
 * before the first byte actually referenced); real disassembly
 * (`cmp BYTE PTR [edx+0x8f1c4a0],0x0` with edx holding the message's own raw
 * subtype field, unadjusted) shows the real base is 0x08f1c4a0 indexed directly
 * by the raw subtype -- both describe the same real bytes; this table uses the
 * direct-index form. Real span confirmed to cover indices 0..23 (only 3/4/5/
 * 0x12/0x14 are nonzero, matching the switch's own 5 real cases exactly).
 */
static const unsigned char kCSWTCH_290[24] = {
	1,0,1,1,1,1,0,0, 0,0,0,0,0,0,0,0, 0,0,1,0,1,0,0,0,
}; /* 0x08f1c4a0, real bytes */

/* CSWTCH_231 (EffectSlotMsgHandler's own real int[9] table at 0x08f1c460,
 * `mov ebp,[edi*4+0x8f1c460]`) is NOT the same table as the byte array Ghidra
 * also happens to name "CSWTCH_231" inside GlobalMsgHandler (a different,
 * unrelated table at a different real address -- confirmed by disassembling both
 * sites separately; Ghidra's CSWTCH_NNN names are a per-decompile-run counter,
 * not a real shared symbol). Used by EffectSlotMsgHandler below, keyed by the
 * message's own +2 `eSTGMidiSource` field (0..8, else default flag 1).
 */
static const int kCSWTCH_231[9] = { 4, 1, 2, 1, 1, 3, 1, 4, 2 };

/* HandleEffectSlotMsg(STGEffectSlotMsg*,eSTGMidiSource)::s_akbyAP, 0x08f1bd1e,
 * 30 bytes (15 {code,value} byte pairs) -- real bytes read directly from
 * .rodata, span confirmed bounded by the next mangled symbol in symbols.csv
 * (HandleEffectLFOParam(STGEffectSlotMsg*)::s_akbyAP @ 0x08f1bd3c). Indices 0
 * and 13 are the real 0xff sentinel ("unused sub-index", handler returns).
 */
static const unsigned char kHandleEffectSlotMsg_s_akbyAP[30] = {
	0xff,0xff, 0x01,0x00, 0x01,0x08, 0x01,0x03, 0x01,0x04, 0x01,0x05, 0x01,0x02,
	0x01,0x06, 0x01,0x07, 0x0e,0x02, 0x0d,0x02, 0x0d,0x00, 0x12,0x00, 0xff,0xff, 0x01,0x0a,
};

/* --- ABI-level helpers to fill the raw {code*, adj} dispatch table -----------------
 *
 * The real ctor stores each handler as a bare code-pointer assignment
 * (`*(code**)(this+off) = ControlMsgHandler;`), which is how Ghidra's decompiler
 * flattens a non-virtual pointer-to-member-function literal once optimized -- on the
 * Itanium C++ ABI (this target), such a literal is a 2-word {ptr, adj} value with
 * adj always 0 and ptr always the function's plain entry address (never the
 * vtable-offset/odd encoding, which only applies to virtual functions). These
 * helpers extract that low word via a union, matching the compiler's own
 * representation directly rather than reproducing it by guesswork -- the same
 * "trust the ABI, do the raw thing" license this project already uses for the
 * CallVSlot1/2-style helpers (ckernel.cpp) and the manual vtable-swap idiom
 * (omega_ptr_array.h et al). Two overloads only, since the 17 real handlers only use
 * two distinct parameter shapes (const STGMessage& and plain STGMessage&).
 */
static inline void *AddrOfConstRefHandler(void (CSTGUnsolMsgHandler::*mfp)(const STGMessage &))
{
	union { void (CSTGUnsolMsgHandler::*m)(const STGMessage &); void *p[2]; } u;
	u.p[1] = 0;
	u.m = mfp;
	return u.p[0];
}

static inline void *AddrOfRefHandler(void (CSTGUnsolMsgHandler::*mfp)(STGMessage &))
{
	union { void (CSTGUnsolMsgHandler::*m)(STGMessage &); void *p[2]; } u;
	u.p[1] = 0;
	u.m = mfp;
	return u.p[0];
}

/* --- ctor --------------------------------------------------------------------- */

CSTGUnsolMsgHandler::CSTGUnsolMsgHandler(CEditor::CPanelIfcTask *owner)
{
	/* Real ctor writes these two bytes first, before the vtable pointer --
	 * preserved in original order even though it has no observable effect
	 * (nothing reads mFlagsA/mForceSaveOnEnd before they're set again, if ever).
	 */
	mFlagsA = 0;
	mForceSaveOnEnd = 0;

	mVtbl = 0; /* real: &PTR__CSTGUnsolMsgHandler_08f75688 -- not reconstructed, see header */
	mOwner = owner;
	sInstance = this;

	mTable[0].pfn  = AddrOfConstRefHandler(&CSTGUnsolMsgHandler::ControlMsgHandler);
	mTable[0].adj  = 0;
	mTable[1].pfn  = AddrOfConstRefHandler(&CSTGUnsolMsgHandler::GlobalMsgHandler);
	mTable[1].adj  = 0;
	mTable[2].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::CombiMsgHandler);
	mTable[2].adj  = 0;
	mTable[3].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::ProgramSlotMsgHandler);
	mTable[3].adj  = 0;
	mTable[4].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::ProgramMsgHandler);
	mTable[4].adj  = 0;
	mTable[5].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::PatchMsgHandler);
	mTable[5].adj  = 0;
	mTable[6].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::VoiceModelMsgHandler);
	mTable[6].adj  = 0;
	mTable[7].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::EffectMgrMsgHandler);
	mTable[7].adj  = 0;
	mTable[8].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::EffectSlotMsgHandler);
	mTable[8].adj  = 0;
	mTable[9].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::EffectMsgHandler);
	mTable[9].adj  = 0;
	/* Slots 10/11/12/13/15 are the 5 confirmed-*static* no-op handlers -- real
	 * plain function-pointer-to-void* casts, no union trick needed.
	 */
	mTable[10].pfn = (void *)&CSTGUnsolMsgHandler::TestControlMsgHandler;
	mTable[10].adj = 0;
	mTable[11].pfn = (void *)&CSTGUnsolMsgHandler::ASKMsgHandler;
	mTable[11].adj = 0;
	mTable[12].pfn = (void *)&CSTGUnsolMsgHandler::CalibrationMsgHandler;
	mTable[12].adj = 0;
	mTable[13].pfn = (void *)&CSTGUnsolMsgHandler::FrontPanelMsgHandler;
	mTable[13].adj = 0;
	mTable[14].pfn = AddrOfRefHandler(&CSTGUnsolMsgHandler::HDRTrackMsgHandler);
	mTable[14].adj = 0;
	mTable[15].pfn = (void *)&CSTGUnsolMsgHandler::KLMMsgHandler;
	mTable[15].adj = 0;
	mTable[16].pfn = AddrOfRefHandler(&CSTGUnsolMsgHandler::SetListMsgHandler);
	mTable[16].adj = 0;

	mSentinel = (int32_t)0xffffffff;
}

/* --- destructor-shaped functions (kept plainly named, not real C++ dtors --
 * see header's mVtbl note) ------------------------------------------------------ */

void CSTGUnsolMsgHandler::ResetVTable()
{
	mVtbl = 0; /* real: &PTR__CSTGUnsolMsgHandler_08f75688 */
}

void CSTGUnsolMsgHandler::DeletingDtor()
{
	ResetVTable();
	/* Real disassembly brackets this free() in HAL_DisableInterrupts()/
	 * HAL_EnableInterrupts() -- the kernel-side critical-section shim already
	 * established as a no-op-and-dropped userspace concern throughout this
	 * project (ckernel.cpp's own header comment); dropped here too, not
	 * declared as an extern call-contract.
	 */
	free(this);
}

/* --- the real dispatcher -------------------------------------------------------
 *
 * Real body reads STGMessage's own offset+4 field as the 0..16 subtype index (a
 * newly confirmed fact about STGMessage's layout -- see header). STGMessage stays
 * opaque otherwise; only that one int field is asserted, via raw byte-offset
 * arithmetic on the reference's address, same as ustg_user_api.cpp's own treatment
 * of this type elsewhere.
 */
void CSTGUnsolMsgHandler::HandleMessage(STGMessage &msg)
{
	int wasSliderPending = sNowValueSlider;
	int subtype = *(int *)((char *)&msg + 4);

	if (subtype < 17) {
		Slot &slot = mTable[subtype];
		void *pcVar4 = slot.pfn;
		void *pCVar3;

		/* Real generic ptr-to-member-function dispatch: low bit of the "ptr"
		 * word selects direct-address (even) vs vtable-offset (odd) encoding.
		 * Every real entry here is the even/direct case (see ctor) -- the odd
		 * branch is faithfully transcribed but dead given this class's own
		 * construction, not exercised by anything in this reconstruction.
		 */
		if (((uintptr_t)pcVar4 & 1) == 0) {
			pCVar3 = (char *)this + slot.adj;
		} else {
			pCVar3 = (char *)this + slot.adj;
			pcVar4 = *(void **)((char *)pCVar3 + (uintptr_t)pcVar4 - 1);
		}

		typedef void (*RawFn)(void *, void *);
		((RawFn)pcVar4)(pCVar3, &msg);
	}

	if (wasSliderPending != 0 && sNowValueSlider == 0) {
		CPanelOut::SAnalogEvt evt;
		evt.type = 0x19;
		evt.value = (int16_t)(int)((sLastValueSlider * kAnalogScaleNumerator) / kAnalogScaleDenominator);
		mOwner->OnAnalogEvent(&evt);
	}
}

/* --- EndHandling ----------------------------------------------------------------
 *
 * EditApi's own class is not reconstructed -- vtable slots +0x28 ("get scope id for
 * a named sub-object", returns a byte) and +0x2c ("query a flag for that id",
 * writes 1 byte through an out-param) are dispatched by hand, matching the original
 * disassembly's own raw vtable-offset calls exactly, same convention already used
 * for COmegaInterface::ExitRequested()'s `*(*sysapi+0x7c)` call (Stage 1) and
 * ckernel.cpp's CTracer/CHostInterfaceBase blobs.
 */
extern "C" void *EditApi; /* real global, defined in mains.cpp */

void CSTGUnsolMsgHandler::EndHandling()
{
	if (sNowValueSlider != 0) {
		CPanelOut::SAnalogEvt evt;
		evt.type = 0x19;
		evt.value = (int16_t)(int)((sLastValueSlider * kAnalogScaleNumerator) / kAnalogScaleDenominator);
		sNowValueSlider = 0;
		mOwner->OnAnalogEvent(&evt);
	}

	if (sEncoderValue != 0) {
		CPanelOut::SEncoderEvt evt;
		evt.value = (uint8_t)((unsigned)sEncoderValue >> 8);
		evt.reserved[0] = evt.reserved[1] = evt.reserved[2] = 0; /* real: uninitialized stack garbage, see header */
		evt.zero = 0;
		sEncoderValue = 0;
		mOwner->OnEncoderEvent(&evt);
	}

	if (mForceSaveOnEnd != 0) {
		typedef unsigned char (*GetScopeIdFn)(void *, const char *);
		typedef void (*QueryFlagFn)(void *, unsigned char, int, int, unsigned char *, int);

		/* EditApi is itself a pointer to the (opaque, not reconstructed)
		 * CEditApiInstance-shaped object -- one level of dereference gets its
		 * vtable pointer (the object's own first field), matching the real
		 * `*EditApi` in the disassembly exactly (not `&EditApi`, which would
		 * just be the global variable's own address).
		 */
		void *vtbl = *(void **)EditApi;
		GetScopeIdFn getScopeId = *(GetScopeIdFn *)((char *)vtbl + 0x28);
		QueryFlagFn queryFlag = *(QueryFlagFn *)((char *)vtbl + 0x2c);

		unsigned char scopeId = getScopeId(EditApi, "ESSong");
		unsigned char flag = 0;
		queryFlag(EditApi, scopeId, 0, 3, &flag, 1);

		if (flag == 0) {
			USTGAPIControl::SaveRandomSeed();
			sync();
			sleep(3);
			USTGAPIControl::ForceErPShutdown(0);
		}
	}
}

/* --- Shared EditApi vtable-dispatch helpers (Stage 6 batch 2, 2026-07-25) --------
 *
 * Every one of the five handlers below repeats the exact same real shape already
 * established (in raw, inlined form) by EndHandling() just above: fetch a scope id
 * via vtbl+0x28, optionally bracket the vtbl+0x30 "set param" call with vtbl+0x3c/
 * +0x38 if a sequencer-parameter restore is in progress, and toggle
 * USTGUserAPI::mNowStopMessaging/CEditor::lastEditMessage around the +0x30 call
 * itself. Factored here rather than re-inlined five times -- still byte-for-byte
 * the same real vtable offsets/argument shapes as each handler's own disassembly,
 * not a behavioral simplification. Every real call site also re-fetches `*EditApi`
 * AFTER the optional +0x3c call (visible in the disassembly as a fresh
 * `iVar = *EditApi;` reload) rather than reusing an earlier cached vtbl pointer --
 * preserved here for faithfulness even though s_eNowRestoreSeqParameters is always
 * 0 in this pass's own data, making the reload currently a no-op.
 *
 * REAL BUG found and fixed alongside this: PTR__CEditApiInstance_08e85da8
 * (mains.cpp) was sized 6 slots -- enough for EndHandling()'s own dead-branch-only
 * +0x28/+0x2c reads above, but not for these five handlers' unconditional +0x28/
 * +0x30 dispatch (+0x30/4 = slot 12). Bumped to 20 slots, see mains.cpp's own
 * WORKAROUND #2 comment.
 */
typedef unsigned char (*EditApiGetScopeIdFn)(void *, const char *);
typedef void (*EditApiVoidSelfFn)(void *);
typedef void (*EditApiSetParamFn)(void *, unsigned char, unsigned char, unsigned char, void *, int, int);

unsigned char CSTGUnsolMsgHandler::EditApiGetScopeId(const char *name)
{
	void *vtbl = *(void **)EditApi;
	EditApiGetScopeIdFn fn = *(EditApiGetScopeIdFn *)((char *)vtbl + 0x28);
	return fn(EditApi, name);
}

void CSTGUnsolMsgHandler::EditApiSendParamMsg(unsigned char scope, unsigned char code, unsigned char value,
                                               void *payload, int len, int flag)
{
	if (s_eNowRestoreSeqParameters != 0) {
		void *vtbl = *(void **)EditApi;
		EditApiVoidSelfFn beginRestore = *(EditApiVoidSelfFn *)((char *)vtbl + 0x3c);
		beginRestore(EditApi);
	}

	void *vtbl = *(void **)EditApi; /* real: fresh reload after the +0x3c call above */
	EditApiSetParamFn setParam = *(EditApiSetParamFn *)((char *)vtbl + 0x30);

	USTGUserAPI::mNowStopMessaging = 1;
	CEditor::lastEditMessage = 0x500c;
	setParam(EditApi, scope, code, value, payload, len, flag);
	USTGUserAPI::mNowStopMessaging = 0;

	if (s_eNowRestoreSeqParameters != 0) {
		void *vtbl2 = *(void **)EditApi;
		EditApiVoidSelfFn endRestore = *(EditApiVoidSelfFn *)((char *)vtbl2 + 0x38);
		endRestore(EditApi);
	}
}

/* --- slider/encoder value senders ------------------------------------------------ */

void CSTGUnsolMsgHandler::SendValueSlider()
{
	CPanelOut::SAnalogEvt evt;
	evt.type = 0x19;
	evt.value = (int16_t)(int)((sLastValueSlider * kAnalogScaleNumerator) / kAnalogScaleDenominator);
	mOwner->OnAnalogEvent(&evt);
}

void CSTGUnsolMsgHandler::SendValueEncoder()
{
	if (sEncoderValue != 0) {
		CPanelOut::SEncoderEvt evt;
		evt.value = (uint8_t)((unsigned)sEncoderValue >> 8);
		evt.reserved[0] = evt.reserved[1] = evt.reserved[2] = 0; /* real: uninitialized stack garbage, see header */
		evt.zero = 0;
		sEncoderValue = 0;
		mOwner->OnEncoderEvent(&evt);
	}
}

void CSTGUnsolMsgHandler::EnterGlobalObjectEdit(int enable)
{
	s_bIsInGlobalObjectEdit = enable;
}

/* --- real, confirmed-empty no-ops (both static and instance shapes) -------------- */

void CSTGUnsolMsgHandler::Initialize(CCombi *, CCombi *) {}
void CSTGUnsolMsgHandler::InitializeForSong(CCombi *, CCombi *) {}
void CSTGUnsolMsgHandler::BeginHandling() {}
void CSTGUnsolMsgHandler::TestControlMsgHandler(STGMessage &) {}
void CSTGUnsolMsgHandler::ASKMsgHandler(STGMessage &) {}
void CSTGUnsolMsgHandler::CalibrationMsgHandler(STGMessage &) {}
void CSTGUnsolMsgHandler::FrontPanelMsgHandler(STGMessage &) {}
void CSTGUnsolMsgHandler::KLMMsgHandler(STGMessage &) {}

/* --- Tier B link-stubs: genuinely deep per-subsystem processing, not implemented -- */

void CSTGUnsolMsgHandler::ControlMsgHandler(const STGMessage &) { /* Tier-B link-stub. .text+0x0891ac70, 4886 bytes. */ }
void CSTGUnsolMsgHandler::GlobalMsgHandler(const STGMessage &) { /* Tier-B link-stub. .text+0x08918b50, 2012 bytes. */ }
void CSTGUnsolMsgHandler::CombiMsgHandler(STGMessage &) { /* Tier-B link-stub. .text+0x08919360, 2951 bytes. */ }
void CSTGUnsolMsgHandler::ProgramSlotMsgHandler(STGMessage &) { /* Tier-B link-stub. .text+0x08918410, 1792 bytes. */ }
void CSTGUnsolMsgHandler::ProgramMsgHandler(STGMessage &) { /* Tier-B link-stub. .text+0x08919fd0, 3114 bytes. */ }
void CSTGUnsolMsgHandler::VoiceModelMsgHandler(STGMessage &) { /* Tier-B link-stub. .text+0x08917100, 2487 bytes. */ }

/* --- Tier A, batch 2 (2026-07-25): real bodies -----------------------------------
 *
 * All five share the guard/scope/table/dispatch shape documented at this file's
 * own top (CStorage/DAT_.../kCSWTCH_290/EditApiGetScopeId/EditApiSendParamMsg).
 * STGMessage stays opaque -- every field access below is raw byte-offset pointer
 * arithmetic on `&msg` cast to `unsigned char *`, same convention as
 * HandleMessage()'s own offset+4 read (STGMessage is Ghidra's own effectively
 * 1-byte-element type here, matching the real `param_1 + 0xN` = byte offset N in
 * every one of these functions' own decompile).
 */

/* CSTGUnsolMsgHandler::PatchMsgHandler(STGMessage&), .text+0x08916d90, 340 bytes. */
void CSTGUnsolMsgHandler::PatchMsgHandler(STGMessage &msg)
{
	unsigned char *p = (unsigned char *)&msg;

	if (*(int *)(p + 8) != 0)
		return;

	unsigned int target = *(unsigned int *)(p + 0x10);
	if ((*(unsigned int *)(p + 0xc) == (unsigned int)CStorage::sm_ucCurrentProg && target == (unsigned int)DAT_0af30549)
	    || target == 0xfffe) {
		if (target == 0xfffe && s_bIsInGlobalObjectEdit == 0)
			return;
	} else if (target != 0xffff) {
		return;
	}

	/* real: entire remaining body gated on this opaque mode/state byte, see
	 * this file's own top comment on DAT_0af0df1e.
	 */
	if ((DAT_0af0df1e & 7) != 3)
		return;

	if (*(int *)(p + 0x18) > 0)
		*(int *)(p + 0x18) -= 1;

	unsigned char value = p[0x14];
	unsigned char scope = EditApiGetScopeId("ESProg");

	EditApiSendParamMsg(scope, 0x53, value, p + 0x18, 4, 1);
}

/* CSTGUnsolMsgHandler::EffectMgrMsgHandler(STGMessage&), .text+0x08916600, 541 bytes. */
void CSTGUnsolMsgHandler::EffectMgrMsgHandler(STGMessage &msg)
{
	unsigned char *p = (unsigned char *)&msg;

	if (*(int *)(p + 8) != 0)
		return;

	int kind = *(int *)(p + 0x20);
	unsigned int target = *(unsigned int *)(p + 0x10);
	unsigned int objId, objSub;

	if (kind == 1)      { objId = (unsigned int)CStorage::sm_ucCurrentProg;  objSub = (unsigned int)DAT_0af30549; }
	else if (kind == 2) { objSub = (unsigned int)CStorage::sm_usCurrentSong; objId = 0; }
	else if (kind == 0) { objId = (unsigned int)CStorage::sm_ucCurrentCombi; objSub = (unsigned int)DAT_0af3054b; }
	else                { objId = 0; objSub = 0; }

	if ((*(unsigned int *)(p + 0xc) != objId || target != objSub) && target != 0xfffe && target != 0xffff)
		return;

	int idx = *(int *)(p + 0x18);
	unsigned char code  = kHandleEffectLFOParam_s_akbyAP[idx * 2];
	unsigned char value = kHandleEffectLFOParam_s_akbyAP[idx * 2 + 1];
	unsigned char scope;

	if (kind == 1) {
		if (target == 0xfffe && s_bIsInGlobalObjectEdit == 0) { scope = EditApiGetScopeId("ESSampling"); code += 3; }
		else                                                  { scope = EditApiGetScopeId("ESProg");     code += 2; }
	} else if (kind == 0) {
		scope = EditApiGetScopeId("ESCombi");
	} else {
		if (kind != 2)
			return;
		scope = EditApiGetScopeId("ESSong");
	}

	unsigned char slotVal = p[0x14];
	EditApiSendParamMsg(scope, (unsigned char)(code + slotVal), value, p + 0x1c, 4, 1);
}

/* CSTGUnsolMsgHandler::EffectMsgHandler(STGMessage&), .text+0x08916840, 660 bytes. */
void CSTGUnsolMsgHandler::EffectMsgHandler(STGMessage &msg)
{
	unsigned char *p = (unsigned char *)&msg;

	if (*(int *)(p + 8) != 0)
		return;

	int kind = *(int *)(p + 0x20);
	unsigned int target = *(unsigned int *)(p + 0x10);
	unsigned int objId, objSub;

	if (kind == 1)      { objId = (unsigned int)CStorage::sm_ucCurrentProg;  objSub = (unsigned int)DAT_0af30549; }
	else if (kind == 2) { objSub = (unsigned int)CStorage::sm_usCurrentSong; objId = 0; }
	else if (kind == 0) { objId = (unsigned int)CStorage::sm_ucCurrentCombi; objSub = (unsigned int)DAT_0af3054b; }
	else                { objId = 0; objSub = 0; }

	if ((*(unsigned int *)(p + 0xc) != objId || target != objSub) && target != 0xfffe && target != 0xffff)
		return;

	unsigned char scope = EditApiGetScopeId("ESEffect");
	unsigned char code, value;

	if (*(unsigned int *)(p + 0x18) == 0) {
		int sub = *(int *)(p + 0x14) + (*(int *)(p + 0x14) > 0xb ? 1 : 0);
		int k2 = *(int *)(p + 0x20);

		if (k2 == 1) {
			if (*(int *)(p + 0x10) == 0xfffe && s_bIsInGlobalObjectEdit == 0) { code = (unsigned char)((sub + 4) & 0xff); scope = EditApiGetScopeId("ESSampling"); }
			else                                                              { code = (unsigned char)((sub + 3) & 0xff); scope = EditApiGetScopeId("ESProg"); }
		} else if (k2 == 0) {
			code = (unsigned char)((sub + 1) & 0xff);
			scope = EditApiGetScopeId("ESCombi");
		} else {
			if (k2 != 2)
				return;
			scope = EditApiGetScopeId("ESSong");
			code = (unsigned char)((sub + 1) & 0xff);
		}

		/* real: turns the payload dword into a plain 0/1 boolean in place
		 * before it's sent (as the 4-byte payload) below.
		 */
		*(unsigned int *)(p + 0x1c) = (*(unsigned int *)(p + 0x1c) == 0) ? 1u : 0u;
		value = 1;
	} else {
		value = (unsigned char)(*(unsigned int *)(p + 0x18) & 0xff);
		code = p[0x14];
	}

	EditApiSendParamMsg(scope, code, value, p + 0x1c, 4, 1);
}

/* CSTGUnsolMsgHandler::HDRTrackMsgHandler(STGMessage&), .text+0x08917ad0, 488 bytes. */
void CSTGUnsolMsgHandler::HDRTrackMsgHandler(STGMessage &msg)
{
	unsigned char *p = (unsigned char *)&msg;

	if (*(int *)(p + 8) != 0)
		return;
	unsigned int target = *(unsigned int *)(p + 0xc);
	if (target != (unsigned int)CStorage::sm_usCurrentSong && target != 0xfffe && target != 0xffff)
		return;

	int idx = *(int *)(p + 0x14);
	unsigned char scope = EditApiGetScopeId("ESSong");
	unsigned int field10 = *(unsigned int *)(p + 0x10);
	unsigned char code, value;

	if ((unsigned int)(idx - 0xb) < 2) {
		/* real: brackets the dispatch below in a "direct store PMR status"
		 * mode -- table byte ordering swaps vs. the else branch (see header).
		 */
		CESSongTask::ms_bShouldDirectStorePMRStatus = 1;
		code  = kHandleHDRMsg_s_akbyAP[idx * 2];
		value = (unsigned char)((char)field10 + (char)kHandleHDRMsg_s_akbyAP[idx * 2 + 1]);
		EditApiSendParamMsg(scope, code, value, p + 0x18, 4, 1);
		CESSongTask::ms_bShouldDirectStorePMRStatus = 0;
	} else {
		code  = (unsigned char)((char)field10 + (char)kHandleHDRMsg_s_akbyAP[idx * 2]);
		value = kHandleHDRMsg_s_akbyAP[idx * 2 + 1];
		EditApiSendParamMsg(scope, code, value, p + 0x18, 4, 1);
	}
}

/* CSTGUnsolMsgHandler::SetListMsgHandler(STGMessage&), .text+0x08916b00, 549 bytes. */
void CSTGUnsolMsgHandler::SetListMsgHandler(STGMessage &msg)
{
	unsigned char *p = (unsigned char *)&msg;

	unsigned char scope = EditApiGetScopeId("ESSetList");
	int subtype = *(int *)(p + 0x14);
	unsigned char code, value;

	if ((unsigned int)(subtype - 3) < 0x12 && kCSWTCH_290[subtype] != 0) {
		int idx;
		switch (subtype) {
		case 3:    idx = 1; break;
		case 4:    idx = 2; break;
		case 5:    idx = 3; break;
		case 0x12: idx = 0; break;
		case 0x14: idx = 4; break;
		default:   return;
		}
		code  = (unsigned char)((kSetListMsgHandler_s_akbyAPSlot[idx * 2] + *(int *)(p + 0x10)) & 0xff);
		value = kSetListMsgHandler_s_akbyAPSlot[idx * 2 + 1];
	} else {
		int idx;
		switch (subtype) {
		case 6:    idx = 0;   break;
		case 7:    idx = 1;   break;
		case 8:    idx = 2;   break;
		case 9:    idx = 3;   break;
		case 10:   idx = 4;   break;
		case 0xb:  idx = 5;   break;
		case 0xc:  idx = 6;   break;
		case 0xd:  idx = 7;   break;
		case 0xe:  idx = 8;   break;
		case 0xf:  idx = 9;   break;
		case 0x10: idx = 10;  break;
		case 0x11: idx = 0xb; break;
		case 0x13: idx = 0xc; break;
		default:   return;
		}
		code  = kSetListMsgHandler_s_akbyAP[idx * 2];
		value = kSetListMsgHandler_s_akbyAP[idx * 2 + 1];
	}

	EditApiSendParamMsg(scope, code, value, p + 0x18, 4, 1);
}

/* CSTGUnsolMsgHandler::EffectSlotMsgHandler(STGMessage&), .text+0x08917cd0, real
 * 1856 bytes (0x08917cd0..0x08918410). Promoted from Tier B, see header comment for
 * the full reasoning (switch/jump-table cross-check, local_2c buffer-reuse
 * resolution). idx==2/idx==0xb/idx==3 all match EditApiSendParamMsg's shape exactly
 * (flag always 1, lastEditMessage always 0x500c); only the generic tail (every other
 * idx) diverges and is written out by hand below.
 */
void CSTGUnsolMsgHandler::EffectSlotMsgHandler(STGMessage &msg)
{
	unsigned char *p = (unsigned char *)&msg;

	if (*(int *)(p + 8) != 0)
		return;

	int kind = *(int *)(p + 0x20);
	unsigned int target = *(unsigned int *)(p + 0x10);
	unsigned int objId, objSub;

	if (kind == 1)      { objId = (unsigned int)CStorage::sm_ucCurrentProg;  objSub = (unsigned int)DAT_0af30549; }
	else if (kind == 2) { objSub = (unsigned int)CStorage::sm_usCurrentSong; objId = 0; }
	else if (kind == 0) { objId = (unsigned int)CStorage::sm_ucCurrentCombi; objSub = (unsigned int)DAT_0af3054b; }
	else                { objId = 0; objSub = 0; }

	if ((*(unsigned int *)(p + 0xc) != objId || target != objSub) && target != 0xfffe && target != 0xffff)
		return;

	/* Real field @+2: eSTGMidiSource, per HandleEffectSlotMsg's own mangled name --
	 * only ever used below as a 0..8 index into kCSWTCH_231.
	 */
	unsigned short midiSource = *(unsigned short *)(p + 2);
	int iVar3 = *(int *)(p + 0x14);
	int idx   = *(int *)(p + 0x18);

	unsigned char bVar1 = kHandleEffectSlotMsg_s_akbyAP[idx * 2];
	unsigned char cVar5 = kHandleEffectSlotMsg_s_akbyAP[idx * 2 + 1];
	if (bVar1 == 0xff)
		return;

	unsigned char scope;
	unsigned int code;

	if (kind == 1) {
		if (target == 0xfffe && s_bIsInGlobalObjectEdit == 0) { scope = EditApiGetScopeId("ESSampling"); code = bVar1 + 3; }
		else                                                  { scope = EditApiGetScopeId("ESProg");     code = bVar1 + 2; }
	} else if (kind == 0) {
		scope = EditApiGetScopeId("ESCombi");
		code = bVar1;
	} else {
		if (kind != 2)
			return;
		scope = EditApiGetScopeId("ESSong");
		code = bVar1;
	}

	/* Real switch on idx (0..14, jump table at 0x08f1bb1c), computing a signed
	 * byte adjustment (cVar4) added to `code` below -- except the "default" group
	 * (idx 0/2/3/4/5/6/7/8/13, or idx>14), which instead adds the message's own
	 * `iVar3` (+2) sub-value directly and skips the cVar4 step entirely (real: a
	 * `goto` past it, confirmed in disassembly). idx==2 within that default group
	 * is further special-cased and returns before reaching any of the idx==0xb/
	 * idx==3/generic tail code below.
	 */
	char cVar4 = (char)iVar3;
	bool skipCVar4Add = false;

	switch (idx) {
	default:
		code = code + (unsigned int)iVar3;
		skipCVar4Add = true;
		if (idx == 2) {
			unsigned char payload;
			if (*(int *)(p + 0x1c) == 0x19) {
				payload = 0;
				EditApiSendParamMsg(scope, (unsigned char)code, cVar5, &payload, 1, 1);
			} else {
				payload = 1;
				EditApiSendParamMsg(scope, (unsigned char)code, cVar5, &payload, 1, 1);
				payload = (unsigned char)((char)*(int *)(p + 0x1c) - 1);
				cVar5 = cVar5 + 1;
				EditApiSendParamMsg(scope, (unsigned char)code, cVar5, &payload, 1, 1);
			}
			return;
		}
		break;
	case 1:
		if (iVar3 > 0xb)
			cVar4 = cVar4 + 1;
		break;
	case 9:
		cVar4 = cVar4 - 0xc;
		break;
	case 10:
	case 11:
	case 12:
		cVar4 = 0;
		break;
	case 14:
		if (iVar3 != 0xb) {
			if ((unsigned int)(iVar3 - 0xc) < 2)
				cVar5 = 3;
			else if ((unsigned int)(iVar3 - 0xe) < 2)
				cVar5 = 2;
			if (iVar3 > 0xb)
				cVar4 = cVar4 + 1;
		} else {
			cVar5 = 8;
			cVar4 = 0xb;
		}
		break;
	}

	if (!skipCVar4Add)
		code = (unsigned char)((char)code + cVar4);

	if (idx == 0xb) {
		unsigned char payload;
		if (*(int *)(p + 0x1c) == 0) {
			payload = 0;
			EditApiSendParamMsg(scope, (unsigned char)code, cVar5, &payload, 1, 1);
		} else {
			payload = 1;
			EditApiSendParamMsg(scope, (unsigned char)code, cVar5, &payload, 1, 1);
			payload = (unsigned char)(*(int *)(p + 0x1c) & 0xff);
			cVar5 = cVar5 + 1;
			EditApiSendParamMsg(scope, (unsigned char)code, cVar5, &payload, 1, 1);
		}
	} else if (idx == 3) {
		int32_t val = *(int *)(p + 0x1c);
		if (val > 0xc)
			val -= 0xc;
		EditApiSendParamMsg(scope, (unsigned char)code, cVar5, &val, 4, 1);
	} else {
		/* Generic tail (every idx other than 2/0xb/3): flag comes from
		 * kCSWTCH_231[midiSource] (default 1 if out of range), payload is the
		 * message's own +0x1c field directly (4 bytes, real, no local copy), and
		 * -- the one real divergence from EditApiSendParamMsg's shape --
		 * CEditor::lastEditMessage is `(flag==3) + 0x500c`, not unconditionally
		 * 0x500c. Written out by hand rather than folded into the shared helper.
		 */
		int flag = 1;
		if (midiSource < 9)
			flag = kCSWTCH_231[midiSource];

		if (s_eNowRestoreSeqParameters != 0) {
			void *vtbl0 = *(void **)EditApi;
			EditApiVoidSelfFn beginRestore = *(EditApiVoidSelfFn *)((char *)vtbl0 + 0x3c);
			beginRestore(EditApi);
		}

		void *vtbl = *(void **)EditApi;
		EditApiSetParamFn setParam = *(EditApiSetParamFn *)((char *)vtbl + 0x30);

		USTGUserAPI::mNowStopMessaging = 1;
		CEditor::lastEditMessage = (uint16_t)((flag == 3 ? 1 : 0) + 0x500c);
		setParam(EditApi, scope, (unsigned char)code, cVar5, p + 0x1c, 4, flag);
		USTGUserAPI::mNowStopMessaging = 0;

		if (s_eNowRestoreSeqParameters != 0) {
			void *vtbl2 = *(void **)EditApi;
			EditApiVoidSelfFn endRestore = *(EditApiVoidSelfFn *)((char *)vtbl2 + 0x38);
			endRestore(EditApi);
		}
	}
}
