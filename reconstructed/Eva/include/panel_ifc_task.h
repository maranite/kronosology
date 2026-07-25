/*
 * panel_ifc_task.h  -  CEditor::CPanelIfcTask::SetMargin (Stage 1 boot path), plus
 * two Tier-B method declarations added by the Stage 6 CSTGUnsolMsgHandler pass
 * (2026-07-25, see stg_unsol_msg_handler.h).
 *
 * CPanelIfcTask itself (constructor, instance fields, vtable, every other real
 * method -- SetupPanelInterface/OnTouchPanelEvent/OnButtonEvent/Exec/SetLEDStatus/
 * ShortBeep/EnterDiagnostics/SetAllLED) is NOT reconstructed here -- the real
 * constructor (.text+0x0824b7e0) pulls in CTask/COutLinkMono, which belong to a
 * concurrent pass's CModule/CTask/CLevelManagerArray work (see include/task_buffer.h).
 * Only SetMargin/GetMargin (real boot-path calls, Tier A) and OnAnalogEvent/
 * OnEncoderEvent (declared, not implemented -- Tier B call-contract externs
 * CSTGUnsolMsgHandler needs) are present. The rest of CEditor::CPanelIfcTask -- and
 * CEditor itself -- is Peg/UI-toolkit-adjacent territory, deliberately out of scope
 * for this pass (see PLAN.md Stage 4).
 */

#ifndef PANEL_IFC_TASK_H
#define PANEL_IFC_TASK_H

namespace CPanelOut {
struct SAnalogEvt;
struct SEncoderEvt;
}

namespace CEditor {

class CPanelIfcTask {
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

} /* namespace CEditor */

#endif /* PANEL_IFC_TASK_H */
