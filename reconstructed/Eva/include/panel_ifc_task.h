/*
 * panel_ifc_task.h  -  CEditor::CPanelIfcTask::SetMargin (Stage 1 boot path).
 *
 * Only the one method main() calls on real hardware (touch-panel calibration
 * margins) is reconstructed. The rest of CEditor::CPanelIfcTask -- and CEditor
 * itself -- is Peg/UI-toolkit territory, deliberately out of scope for this pass
 * (see PLAN.md Stage 4).
 */

#ifndef PANEL_IFC_TASK_H
#define PANEL_IFC_TASK_H

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
};

} /* namespace CEditor */

#endif /* PANEL_IFC_TASK_H */
