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
 * Tier B (real signature, not implemented -- genuinely deep per-subsystem STG message
 * processing, hundreds to ~4900 bytes each, reaching into CCombi/CProg/CGlobal/
 * CControlSurface/effect-slot/voice-model state this pass does not reconstruct):
 * ControlMsgHandler (4886B), GlobalMsgHandler (2012B), CombiMsgHandler (2951B),
 * ProgramSlotMsgHandler (1792B), ProgramMsgHandler (3114B), PatchMsgHandler (340B),
 * VoiceModelMsgHandler (2487B), EffectMgrMsgHandler (541B), EffectSlotMsgHandler
 * (1796B), EffectMsgHandler (660B), HDRTrackMsgHandler (488B), SetListMsgHandler
 * (549B). Reference-vs-pointer parameter shape for every method below (including
 * these) is taken from symbols.csv's own demangled names, not functions.csv's
 * ABI-level (pointer-only) view -- e.g. real mangling is
 * `CSTGUnsolMsgHandler::ControlMsgHandler(STGMessage const&)`, so that one alone
 * takes `const STGMessage&`; every other STGMessage-taking method (including the 5
 * static no-ops) takes a plain non-const `STGMessage&`.
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
	void PatchMsgHandler(STGMessage &msg);
	void VoiceModelMsgHandler(STGMessage &msg);
	void EffectMgrMsgHandler(STGMessage &msg);
	void EffectSlotMsgHandler(STGMessage &msg);
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
};

#endif /* STG_UNSOL_MSG_HANDLER_H */
