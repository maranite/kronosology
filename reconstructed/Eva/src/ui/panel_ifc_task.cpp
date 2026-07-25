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
 */

#include "panel_ifc_task.h"

namespace CEditor {

static unsigned char sm_aucTouchPanelMargin[4];

void CPanelIfcTask::SetMargin(EMargin which, unsigned char value)
{
	if (value < 0x32)
		sm_aucTouchPanelMargin[which] = value;
}

/* Real body has no bounds check on the read side (unlike SetMargin's write-side
 * check) -- preserved as found, not "fixed" to match.
 */
unsigned char CPanelIfcTask::GetMargin(EMargin which)
{
	return sm_aucTouchPanelMargin[which];
}

void CPanelIfcTask::OnAnalogEvent(const CPanelOut::SAnalogEvt *) { /* Tier-B link-stub. .text+0x0824be00, 403 bytes. */ }
void CPanelIfcTask::OnEncoderEvent(const CPanelOut::SEncoderEvt *) { /* Tier-B link-stub. .text+0x0824bdb0, 79 bytes. */ }

} /* namespace CEditor */
