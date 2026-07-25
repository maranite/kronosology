/*
 * stg_unsol_msg_handler.h  -  CSTGUnsolMsgHandler, Eva's dispatcher for unsolicited
 * STGMessages arriving from OA.ko (Stage 6 breadth sweep, 2026-07-25).
 *
 * Real, non-zero-caller entry point confirmed by disassembly (not guessed): the one
 * constructor call site is inside `CEditor::CPanelIfcTask::CPanelIfcTask(CEditor
 * const&, PegScreen*)` (.text+0x0824b7e0, real disassembly at +0x924:
 * `call 0891c090 <CSTGUnsolMsgHandlerC1>`), which is itself invoked from
 * `CEditor::Setup()` (.text+0x08249b60, called by `CModuleManager::Setup()`'s
 * already-reconstructed per-module virtual dispatch, src/base/module_manager.cpp) --
 * i.e. this class sits directly on the (nominal) module-Setup boot path, not off in
 * unreached UI territory. `CEditor::CPanelIfcTask` itself is NOT reconstructed here
 * (its own constructor pulls in CTask/COutLinkMono, which belong to a concurrent
 * pass's CModule/CTask/CLevelManagerArray work -- see include/task_buffer.h's own
 * "CTask... genuinely out of scope" note, and this is itself a real, worth-flagging
 * correction to that pass's own "CTask::CTask() has no caller" verdict: it does have
 * one, right here) -- only the two CPanelIfcTask methods CSTGUnsolMsgHandler itself
 * calls (OnAnalogEvent/OnEncoderEvent) are declared below, as real-signature
 * call-contract externs, same convention as every other not-yet-owned class in this
 * project (ckernel.cpp's CTracer/CHostInterfaceBase).
 *
 * Real class layout (confirmed via CSTGUnsolMsgHandler::CSTGUnsolMsgHandler@0891c090.c,
 * cross-checked against CPanelIfcTask's own `malloc(0x98)` call site for the object --
 * 0x98 = 152 bytes, matching exactly):
 *   +0x00        vtable ptr (PTR__CSTGUnsolMsgHandler_08f75688) -- kept a plain raw
 *                void* (mVtbl), NOT a real C++ vtable, same "manual vtable swap, no
 *                `virtual` keyword" convention already established for COmegaPtrArray/
 *                CModule/CScheduler (see omega_ptr_array.h's own header comment) --
 *                a real C++ virtual method here would make the compiler synthesize
 *                its own vtable and fight the ctor's own manual `mVtbl = &PTR__...`
 *                assignment.
 *   +0x04        CEditor::CPanelIfcTask *mOwner  (ctor's own param_1)
 *   +0x08..+0x8f a real 17-entry unsolicited-message dispatch table, one entry per
 *                STGMessage subtype 0..16 (see HandleMessage() below), 8 bytes/entry:
 *                  +0  code* pfn   -- Itanium pointer-to-member-function "ptr" word
 *                  +4  int   adj   -- Itanium "adj" word (always 0 here -- every
 *                                     entry is a plain non-virtual function address,
 *                                     never the odd/vtable-offset encoding; HandleMessage()
 *                                     still contains the generic both-cases dispatch
 *                                     code a real ptr-to-member-function call compiles
 *                                     to, faithfully preserved even though the
 *                                     vtable-offset branch is dead given this ctor's
 *                                     own data)
 *   +0x90        int  mSentinel = 0xffffffff (purpose not traced -- never read by any
 *                method reconstructed here)
 *   +0x94        uint8_t mFlagsA = 0  (not read by any method reconstructed here)
 *   +0x95        uint8_t mForceSaveOnEnd = 0 (EndHandling()'s own end-of-song-edit
 *                save-and-shutdown gate -- see below)
 *
 * Message-subtype -> handler mapping (from the ctor's own literal table, index =
 * STGMessage's own offset+4 field per HandleMessage()'s dispatch -- a real, newly
 * confirmed fact about STGMessage's layout, not previously documented in
 * ustg_user_api.h's own opaque-STGMessage note):
 *   0 ControlMsgHandler        6  VoiceModelMsgHandler    12 CalibrationMsgHandler
 *   1 GlobalMsgHandler         7  EffectMgrMsgHandler     13 FrontPanelMsgHandler
 *   2 CombiMsgHandler          8  EffectSlotMsgHandler    14 HDRTrackMsgHandler
 *   3 ProgramSlotMsgHandler    9  EffectMsgHandler        15 KLMMsgHandler
 *   4 ProgramMsgHandler        10 TestControlMsgHandler   16 SetListMsgHandler
 *   5 PatchMsgHandler          11 ASKMsgHandler
 *
 * Real, worth-flagging quirk: the 5 slots typed `__cdecl(STGMessage*)` in
 * functions.csv (TestControlMsgHandler/ASKMsgHandler/CalibrationMsgHandler/
 * FrontPanelMsgHandler/KLMMsgHandler -- confirmed *static* member functions, no
 * implicit `this`) are still stored in, and called through, the exact same
 * two-argument (owner-adjusted-this, message) dispatch as the fifteen real
 * *instance* handlers. Under cdecl's right-to-left push order the callee's one
 * expected stack slot actually lands on the owner-pointer, not the message pointer --
 * real, harmless only because all five bodies are unconditional `return;` (see
 * below), not something this pass invented or "fixed".
 *
 * Tier A (faithful, transcribed below): ctor, both real destructor-shaped functions
 * (kept as plainly-named methods, not real C++ destructors -- see mVtbl note above),
 * HandleMessage() (the real dispatcher), EndHandling(), SendValueSlider(),
 * SendValueEncoder(), EnterGlobalObjectEdit(int), and 8 of the 17 table slots that
 * are genuinely, confirmed-by-reading-every-one empty `return;` bodies in the real
 * binary already (the 5 static ones above, each literally 1 byte in the real .text,
 * plus Initialize(CCombi*, CCombi*)/InitializeForSong(CCombi*, CCombi*)/
 * BeginHandling(), also 1-byte no-ops, also confirmed static/cdecl). These are real
 * facts about the shipped binary, not something this pass simplified.
 *
 * Tier A, batch 2 (2026-07-25 -- promoted from Tier B below): PatchMsgHandler (340B),
 * EffectMgrMsgHandler (541B), EffectMsgHandler (660B), HDRTrackMsgHandler (488B),
 * SetListMsgHandler (549B). All five share one real, repeated shape confirmed across
 * every one of them by direct disassembly: (1) an optional CStorage-current-selection
 * guard (skippable via the msg's own 0xffff/0xfffe "wildcard target" codes -- same
 * convention CSTGUnsolMsgHandler already establishes elsewhere), (2) a
 * `EditApi_vtbl+0x28` scope-id lookup ("ESProg"/"ESCombi"/"ESSong"/"ESSetList"/
 * "ESEffect"), (3) a small compile-time byte lookup table (a real local `static
 * const` table inside a *different*, not-reconstructed free function --
 * HandleHDRMsg/HandleEffectLFOParam/etc -- read directly out of the real binary's
 * .rodata via `readelf -l` VA->file-offset + raw byte read, not guessed, same
 * technique the ctor's own float constants used in batch 1) mapping the message's own
 * subtype/sub-index field to a (code, value) byte pair, and (4) the same
 * `EditApi_vtbl+0x30` "set param" dispatch already established by EndHandling()'s own
 * dead branch, now a real, unconditionally-exercised call. See
 * src/ipc/stg_unsol_msg_handler.cpp's own header comment for the raw table bytes and
 * the `EditApiGetScopeId`/`EditApiSendParamMsg` shared helpers factoring this repeated
 * shape. A REAL BUG was found and fixed alongside this: EditApi's own vtable
 * (`PTR__CEditApiInstance_08e85da8`, mains.cpp) was sized 6 slots, enough for
 * EndHandling()'s dead-branch-only +0x28/+0x2c reads but not for these five
 * handlers' unconditional +0x28/+0x30 dispatch (+0x30/4 = slot 12) -- bumped to 20,
 * see mains.cpp's own WORKAROUND #2 comment.
 *
 * Tier B (real signature, not implemented -- genuinely deep per-subsystem STG message
 * processing reaching into CCombi/CProg/CGlobal/CControlSurface/CMMI/CModeManager/
 * CControlSurface/CDiskUtil/CForm-family classes,voice-model-algorithm-database state this pass does
 * not reconstruct): ControlMsgHandler (4886B, dispatches into CControlSurface/CMMI/
 * CDiskUtil/CForm-family classes,CHelpManager -- by far the deepest), GlobalMsgHandler (2012B,
 * per-global-param switch over ~0x70 codes plus a `SetWithoutUpdatingSTG()` free-
 * function dependency this pass doesn't reconstruct), CombiMsgHandler (2951B, CMMI/
 * CModeManager/CPrograms/CToneAdjustTool), ProgramSlotMsgHandler (1792B, CMMI::
 * GetInstance()/CModeManager::IsOnTimbreProgramEditInContext/ChangeToTopPage/
 * CKGMsgProcessor::GetInstance()), ProgramMsgHandler (3114B, CMMI), VoiceModelMsgHandler
 * (2487B, CStorage::GetInstance()'s own algorithm-database vtable dispatch,
 * CSTGMultisampleBankUUIDBase, an Api-vtable assertion call, `SetWithoutUpdatingSTG()`).
 *
 * EffectSlotMsgHandler (1796B) is ALSO left Tier B, but for a different, worth-
 * distinguishing reason than the six above: it reaches no new subsystem (same
 * EditApi/CStorage/local-byte-table shape as the five just promoted), but its real
 * body is a genuinely intricate goto/switch tangle with a reused 28-byte stack buffer
 * (`local_2c`) written and read at three different widths (whole int / low byte /
 * `._1_3_` upper-3-bytes-of-a-partially-written-int) across different branches --
 * including at least one spot the real compiler itself left as an uninitialized-
 * stack-garbage read (matching the `SEncoderEvt` padding precedent already documented
 * above, but here feeding directly into an outgoing STG message rather than a
 * discarded struct field). Faithfully untangling that buffer's real per-branch
 * lifetime with confidence was judged disproportionate for one 1796-byte function
 * given this pass's remaining scope -- a good candidate for a future dedicated pass,
 * not a hard blocker.
 *
 * Reference-vs-pointer parameter shape for every method below is taken from
 * symbols.csv's own demangled names, not functions.csv's ABI-level (pointer-only)
 * view -- e.g. real mangling is `CSTGUnsolMsgHandler::ControlMsgHandler(STGMessage
 * const&)`, so that one alone takes `const STGMessage&`; every other STGMessage-
 * taking method (including the 5 static no-ops) takes a plain non-const
 * `STGMessage&`.
 */

#ifndef STG_UNSOL_MSG_HANDLER_H
#define STG_UNSOL_MSG_HANDLER_H

#include "ustg_user_api.h"   /* struct STGMessage (opaque) */
#include "panel_ifc_task.h"  /* CEditor::CPanelIfcTask (forward-usable pointer type) */

#include <stdint.h>

/* Forward-only -- not reconstructed here, see CTask/COutLinkMono note above. Only
 * ever touched as an opaque pointer by CSTGUnsolMsgHandler's own Initialize()/
 * InitializeForSong() (both real, confirmed-empty no-ops, see above).
 */
class CCombi;

/* CSTGUnsolMsgHandler's own SendValueSlider()/SendValueEncoder()/HandleMessage()/
 * EndHandling() forward analog-slider and rotary-encoder values into two
 * CPanelIfcTask methods this pass does not otherwise reconstruct. Declared here as
 * real-signature call-contract externs (not implemented) -- see this header's own
 * top comment. SAnalogEvt/SEncoderEvt sizes are confirmed exactly from the stack
 * layout each real caller builds (see .cpp), not guessed.
 */
/* Real namespace confirmed straight from `nm -C` (not guessed): both events are
 * `CPanelOut::SAnalogEvt`/`CPanelOut::SEncoderEvt`, NOT nested under CEditor at all
 * -- `CPanelOut` itself is a whole other not-reconstructed class (front-panel LED/
 * analog output), only its two nested event-payload structs are needed here.
 */
namespace CPanelOut {

struct SAnalogEvt {
	int32_t type;   /* always 0x19 (25) at every real call site in this class */
	int16_t value;  /* 0..1023 (see HandleMessage()'s own float scale, .cpp) */
};

/* Real size is 8 bytes (two stack dwords) at every call site, but only byte 0 is
 * ever assigned a real value -- bytes 1..3 are the original binary's own
 * uninitialized-stack-garbage read (EndHandling()'s own local_18._1_3_), and the
 * trailing dword is always zeroed. Preserved as found: this reconstruction zero-
 * initializes the padding (deterministic, unlike the original) rather than
 * reproducing genuine UB, since nothing downstream is confirmed to read it anyway.
 */
struct SEncoderEvt {
	uint8_t value;
	uint8_t reserved[3];
	int32_t zero;
};

} /* namespace CPanelOut */

/* CEditor::CPanelIfcTask::OnAnalogEvent(CPanelOut::SAnalogEvt const*) / OnEncoderEvent
 * (.text+0x0824be00 / 0x0824bdb0, 403/79 bytes) -- real signatures confirmed via
 * `nm -C`. Declared as genuine member-function additions to panel_ifc_task.h's
 * existing CPanelIfcTask class (Tier B, not implemented) rather than as free
 * functions bound via an asm() label: a real non-static C++ member declared on the
 * class gets the correct implicit-`this`-as-first-argument call shape for free
 * (matching what Ghidra calls "__thiscall" for this SysV/Itanium binary) with zero
 * extra machinery, whereas a free function would need careful hand-verified calling-
 * convention matching to avoid a real ABI mismatch.
 */

class CSTGUnsolMsgHandler {
public:
	/* .text+0x0891c090, 297 bytes. Builds the 17-entry dispatch table described
	 * above; see .cpp for the full literal table transcription.
	 */
	CSTGUnsolMsgHandler(CEditor::CPanelIfcTask *owner);

	/* .text+0x089b9e30, 11 bytes. Real "complete-object destructor" shape: just
	 * resets the vtable pointer. Named plainly (not `~CSTGUnsolMsgHandler()`)
	 * per this file's own mVtbl convention note above.
	 */
	void ResetVTable();

	/* .text+0x089b9e40, 39 bytes. Real "deleting destructor" shape: resets the
	 * vtable pointer (same as above) then frees `this`. Real disassembly
	 * brackets the free() in HAL_Disable/EnableInterrupts (dropped here, same
	 * documented no-op-in-userspace convention as ckernel.cpp).
	 */
	void DeletingDtor();

	/* .text+0x089162e0, 168 bytes. The real dispatcher: reads STGMessage's own
	 * offset+4 field as a 0..16 subtype index, calls the matching table-slot
	 * handler via the real Itanium ptr-to-member-function encoding (see header
	 * comment), then -- if a pending slider value was consumed by that handler
	 * (sNowValueSlider transitions nonzero->zero) -- forwards the scaled value
	 * to CPanelIfcTask::OnAnalogEvent(). STGMessage stays opaque; only the
	 * offset+4 subtype-index fact is asserted here, consistent with
	 * ustg_user_api.h's existing "confirm one field, don't retype the whole
	 * struct" convention.
	 */
	void HandleMessage(STGMessage &msg);

	/* .text+0x0891c290, 283 bytes. Flushes any still-pending slider/encoder
	 * value, then -- if mForceSaveOnEnd is set -- queries EditApi (real vtable
	 * slots +0x28/+0x2c, both undecoded -- see .cpp) for an "ESSong" scope flag
	 * and, if that flag comes back false, saves the random seed, syncs, sleeps
	 * 3 seconds, and force-shuts-down. Real, faithfully transcribed; dead code
	 * given this pass's own data (mForceSaveOnEnd is never set to nonzero by
	 * anything reconstructed here -- EnterGlobalObjectEdit() only ever touches
	 * the *global* s_bIsInGlobalObjectEdit, a different flag).
	 */
	void EndHandling();

	/* .text+0x0891c1f0/0x0891c240, 71/66 bytes. Unconditionally (Slider) /
	 * conditionally on sEncoderValue!=0 (Encoder) forward the pending value to
	 * CPanelIfcTask, same scale-and-clear shape as HandleMessage()'s own tail.
	 */
	void SendValueSlider();
	void SendValueEncoder();

	/* .text+0x0891c3c0, 10 bytes. `s_bIsInGlobalObjectEdit = enable;` -- real,
	 * trivial.
	 */
	void EnterGlobalObjectEdit(int enable);

	/* Real, confirmed-empty in the shipped binary (each 1 byte, `return;` with
	 * no body), and confirmed *static* (cdecl, no implicit `this` in
	 * functions.csv) -- not simplified by this pass, see header comment.
	 */
	static void Initialize(CCombi *a, CCombi *b);
	static void InitializeForSong(CCombi *a, CCombi *b);
	static void BeginHandling();
	static void TestControlMsgHandler(STGMessage &msg);
	static void ASKMsgHandler(STGMessage &msg);
	static void CalibrationMsgHandler(STGMessage &msg);
	static void FrontPanelMsgHandler(STGMessage &msg);
	static void KLMMsgHandler(STGMessage &msg);

	/* Tier B -- real signatures only, genuinely deep per-subsystem processing,
	 * see header comment for sizes. Not implemented.
	 */
	void ControlMsgHandler(const STGMessage &msg);
	void GlobalMsgHandler(const STGMessage &msg);
	void CombiMsgHandler(STGMessage &msg);
	void ProgramSlotMsgHandler(STGMessage &msg);
	void ProgramMsgHandler(STGMessage &msg);
	void VoiceModelMsgHandler(STGMessage &msg);
	/* Tier B, different reason (intricate goto/switch + reused partial-width
	 * stack buffer) -- see header comment.
	 */
	void EffectSlotMsgHandler(STGMessage &msg);

	/* Tier A, batch 2 (2026-07-25) -- real bodies, see header comment. */
	void PatchMsgHandler(STGMessage &msg);
	void EffectMgrMsgHandler(STGMessage &msg);
	void EffectMsgHandler(STGMessage &msg);
	void HDRTrackMsgHandler(STGMessage &msg);
	void SetListMsgHandler(STGMessage &msg);

	/* Real layout is {code* fn; int32 adj} per HandleMessage()'s own decompile --
	 * kept public/raw (not a real C++ pointer-to-member) so the ctor and
	 * HandleMessage() can both build/walk it with plain pointer arithmetic,
	 * matching the original's own explicit encoding checks instruction-for-
	 * instruction rather than trusting the host compiler to reproduce the
	 * identical dispatch a real ptr-to-member-function call would generate.
	 */
	struct Slot {
		void *pfn;
		int32_t adj;
	};

	/* Test-only seam (verify/test_stg_unsol_msg_handler.cpp) -- peeks the private
	 * dispatch table/owner-pointer the real class has no public accessor for.
	 * Same convention as ustg_user_api.h's own UstgUserApiTestHooks friend.
	 */
	friend struct StgUnsolMsgHandlerTestHooks;

private:
	void *mVtbl;
	CEditor::CPanelIfcTask *mOwner;
	Slot mTable[17];
	int32_t mSentinel;
	uint8_t mFlagsA;
	uint8_t mForceSaveOnEnd;

	static CSTGUnsolMsgHandler *sInstance;

	/* Shared EditApi vtable-dispatch helpers used by the five Tier A batch-2
	 * handlers (PatchMsgHandler/EffectMgrMsgHandler/EffectMsgHandler/
	 * HDRTrackMsgHandler/SetListMsgHandler) -- private static (touch no
	 * instance state, but need friend access into USTGUserAPI::
	 * mNowStopMessaging, hence members of this class rather than free
	 * functions). See src/ipc/stg_unsol_msg_handler.cpp's own header comment.
	 */
	static unsigned char EditApiGetScopeId(const char *name);
	static void EditApiSendParamMsg(unsigned char scope, unsigned char code, unsigned char value,
	                                 void *payload, int len, int flag);
};

#endif /* STG_UNSOL_MSG_HANDLER_H */
