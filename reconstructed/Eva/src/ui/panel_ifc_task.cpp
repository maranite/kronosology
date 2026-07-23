/*
 * panel_ifc_task.cpp  -  see include/panel_ifc_task.h.
 *
 * Transcribed from Decomp/EVA_Decomp/eva_export/functions/SetMargin@0824cc40.c.
 */

#include "panel_ifc_task.h"

namespace CEditor {

static unsigned char sm_aucTouchPanelMargin[4];

void CPanelIfcTask::SetMargin(EMargin which, unsigned char value)
{
	if (value < 0x32)
		sm_aucTouchPanelMargin[which] = value;
}

} /* namespace CEditor */
