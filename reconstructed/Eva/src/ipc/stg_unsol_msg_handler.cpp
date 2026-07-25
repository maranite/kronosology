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
void CSTGUnsolMsgHandler::PatchMsgHandler(STGMessage &) { /* Tier-B link-stub. .text+0x08916d90, 340 bytes. */ }
void CSTGUnsolMsgHandler::VoiceModelMsgHandler(STGMessage &) { /* Tier-B link-stub. .text+0x08917100, 2487 bytes. */ }
void CSTGUnsolMsgHandler::EffectMgrMsgHandler(STGMessage &) { /* Tier-B link-stub. .text+0x08916600, 541 bytes. */ }
void CSTGUnsolMsgHandler::EffectSlotMsgHandler(STGMessage &) { /* Tier-B link-stub. .text+0x08917cd0, 1796 bytes. */ }
void CSTGUnsolMsgHandler::EffectMsgHandler(STGMessage &) { /* Tier-B link-stub. .text+0x08916840, 660 bytes. */ }
void CSTGUnsolMsgHandler::HDRTrackMsgHandler(STGMessage &) { /* Tier-B link-stub. .text+0x08917ad0, 488 bytes. */ }
void CSTGUnsolMsgHandler::SetListMsgHandler(STGMessage &) { /* Tier-B link-stub. .text+0x08916b00, 549 bytes. */ }
