/*
 * panel_ifc_task.cpp  -  see include/panel_ifc_task.h.
 *
 * Transcribed from Decomp/EVA_Decomp/eva_export/functions/SetMargin@0824cc40.c and
 * GetMargin@0824cc30.c (the latter added 2026-07-25 alongside the CSTGUnsolMsgHandler
 * pass). OnAnalogEvent/OnEncoderEvent are Tier-B link-stubs (real signature, empty
 * body) -- CPanelIfcTask's own instance layout/vtable/constructor are not
 * reconstructed here (see header), so there's nothing real to implement these
 * against yet; empty bodies exist only so CSTGUnsolMsgHandler's own real calls into
 * them link, per this project's Stage-4 tier convention.
 *
 * UPDATE (Stage 6 CEditor batch, 2026-07-25): `namespace CEditor` -> real nested
 * class `CEditor::CPanelIfcTask` (editor.h/panel_ifc_task.h); definitions below
 * just drop the `namespace CEditor { ... }` wrapper and fully qualify instead --
 * semantically identical either way. Two ctor overloads added, both Tier-B (see
 * header): the pre-existing default ctor now has an explicit empty body (was
 * implicit), plus the new `(CEditor*, void*)` overload `CEditor::Setup()` needs.
 */

#include "panel_ifc_task.h"

static unsigned char sm_aucTouchPanelMargin[4];

/* CTask's own test-only placeholder default ctor (task.h) -- not ground truth. */
CEditor::CPanelIfcTask::CPanelIfcTask()
{
}

/* REAL first statement (`CTask::CTask(owner, "PanelIfcTask", 3, 1, 0x804b)`,
 * CPanelIfcTask@0824b7e0.c) -- the rest of the real ctor body (COutLinkMono
 * sub-object + CSTGUnsolMsgHandler construction, own vtable install) is a
 * Tier-B link-stub, deferred, see header comment.
 */
CEditor::CPanelIfcTask::CPanelIfcTask(CEditor *owner, void *)
	: CTask(*owner, "PanelIfcTask", 3, 1, 0x804b)
{
}

void CEditor::CPanelIfcTask::SetMargin(EMargin which, unsigned char value)
{
	if (value < 0x32)
		sm_aucTouchPanelMargin[which] = value;
}

/* Real body has no bounds check on the read side (unlike SetMargin's write-side
 * check) -- preserved as found, not "fixed" to match.
 */
unsigned char CEditor::CPanelIfcTask::GetMargin(EMargin which)
{
	return sm_aucTouchPanelMargin[which];
}

void CEditor::CPanelIfcTask::OnAnalogEvent(const CPanelOut::SAnalogEvt *) { /* Tier-B link-stub. .text+0x0824be00, 403 bytes. */ }
void CEditor::CPanelIfcTask::OnEncoderEvent(const CPanelOut::SEncoderEvt *) { /* Tier-B link-stub. .text+0x0824bdb0, 79 bytes. */ }
