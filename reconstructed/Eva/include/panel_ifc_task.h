/*
 * panel_ifc_task.h  -  CEditor::CPanelIfcTask::SetMargin (Stage 1 boot path), plus
 * two Tier-B method declarations added by the Stage 6 CSTGUnsolMsgHandler pass
 * (2026-07-25, see stg_unsol_msg_handler.h).
 *
 * UPDATE (Stage 6 CEditor batch, 2026-07-25): `CEditor` itself is now a real class
 * (editor.h) rather than a placeholder namespace, so `CPanelIfcTask` is declared
 * there (forward, as a nested type) and defined out-of-line HERE as a genuine
 * nested class `CEditor::CPanelIfcTask` -- same "declare nested, define
 * out-of-line in the sub-object's own file" shape C++ has always supported, just
 * not needed until CEditor itself existed. Every existing caller that already
 * wrote `CEditor::CPanelIfcTask::Xxx(...)` (eva_main.cpp, stg_unsol_msg_handler.h/
 * .cpp) compiles unchanged -- qualified-name lookup doesn't care whether the
 * enclosing scope is a namespace or a class.
 *
 * `CPanelIfcTask` itself (instance fields, vtable, every real instance method --
 * SetupPanelInterface/OnTouchPanelEvent/OnButtonEvent/Exec/SetLEDStatus/ShortBeep/
 * EnterDiagnostics/SetAllLED) is STILL not reconstructed here -- see editor.h's own
 * header comment for a detailed tractability finding (all of the above turn out to
 * be fully reconstructable now, routing through the already-real
 * `COutLinkMono::OutMono()` IPC primitive; only `Exec()`'s own bare-CTask-poll
 * override needs a brand-new, byte-exact-verified `CPanelIfcTask`/anonymous-
 * `CPanelCfg` vtable pair, genuinely a separate class's worth of work) --
 * deliberately left as a dedicated future pass rather than folded into this one.
 *
 * ALSO ADDED this pass: real inheritance from `CTask` (ground truth's own
 * `CEditor::CPanelIfcTask::CPanelIfcTask()` begins with a real
 * `CTask::CTask(owner, "PanelIfcTask", 3, 1, 0x804b)` base-construction call,
 * CPanelIfcTask@0824b7e0.c) -- needed so `CEditor::Setup()` (editor.cpp) can
 * `CModule::Add()` a `CPanelIfcTask*` through its real `CTask*` parameter,
 * matching ground truth's own actual call shape rather than faking a cast. A
 * second constructor overload, `CPanelIfcTask(CEditor*, void*)`, matches the
 * real ctor's own signature (`(CEditor const&, PegScreen*)`) and DOES perform
 * that one real base-construction statement for real -- the rest of the real
 * ctor body (COutLinkMono sub-object + CSTGUnsolMsgHandler construction, own
 * new vtable install) stays Tier-B/deferred, see header comment above. The
 * pre-existing no-argument constructor is KEPT (not replaced) so
 * `verify/test_stg_unsol_msg_handler.cpp`'s existing `CEditor::CPanelIfcTask
 * fakeOwner;` stack object keeps compiling unchanged -- it now default-
 * constructs its `CTask` base via that class's own new test-only placeholder
 * ctor (task.h), which does NOT match any real ground-truth call site either.
 */

#ifndef PANEL_IFC_TASK_H
#define PANEL_IFC_TASK_H

#include "editor.h"
#include "task.h"

namespace CPanelOut {
struct SAnalogEvt;
struct SEncoderEvt;
}

class CEditor::CPanelIfcTask : public CTask {
public:
	/* Real enum recovered from Ghidra's prototype (CEditor::CPanelIfcTask::EMargin);
	 * exact member names are not confirmed, only that main() calls this with
	 * literals 0/1/2/3 -- named generically until the real enumerator names turn up.
	 */
	enum EMargin {
		kMargin0 = 0,
		kMargin1 = 1,
		kMargin2 = 2,
		kMargin3 = 3,
	};

	/* Pre-existing default ctor -- kept for source compatibility with the
	 * already-committed `verify/test_stg_unsol_msg_handler.cpp` stack object.
	 * Never matched any real ground-truth call site.
	 */
	CPanelIfcTask();

	/* .text+0x0824b7e0, 336 bytes real signature -- `owner`/`screen` match
	 * ground truth's own `(CEditor const&, PegScreen*)` (screen kept `void*`
	 * here, PegScreen not reconstructed). Tier-B link-stub: `CEditor::Setup()`
	 * needs this to link/call, but the real body (CTask::CTask() base call +
	 * COutLinkMono sub-object construction + CSTGUnsolMsgHandler construction)
	 * is deferred, see this file's own header comment.
	 */
	CPanelIfcTask(CEditor *owner, void *screen);

	/* Bounds-checked write into a 4-entry static byte table (values >= 0x32 are
	 * silently dropped -- real behavior, not a bug).
	 */
	static void SetMargin(EMargin which, unsigned char value);

	/* .text+0x0824cc30, 12 bytes -- companion read, added alongside the
	 * CSTGUnsolMsgHandler pass while re-touching this header. Real body is just
	 * `return sm_aucTouchPanelMargin[which];`, no bounds check on the read side
	 * (matching SetMargin's own bounds check only guarding the write).
	 */
	static unsigned char GetMargin(EMargin which);

	/* .text+0x0824be00/0x0824bdb0, 403/79 bytes -- real instance methods,
	 * CSTGUnsolMsgHandler's own HandleMessage()/EndHandling()/SendValueSlider()/
	 * SendValueEncoder() call these on `mOwner`. Tier B: real signatures only
	 * (confirmed via `nm -C`), not implemented -- CPanelIfcTask's own instance
	 * layout/vtable/constructor aren't reconstructed here (see top comment), so
	 * there's nothing to implement these against yet.
	 */
	void OnAnalogEvent(const CPanelOut::SAnalogEvt *evt);
	void OnEncoderEvent(const CPanelOut::SEncoderEvt *evt);
};

#endif /* PANEL_IFC_TASK_H */
