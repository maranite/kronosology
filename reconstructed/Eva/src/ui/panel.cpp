/*
 * panel.cpp  -  see include/panel.h.
 *
 * CPanel::CPanel/~CPanel/Setup/Config/Start transcribed from
 * CPanel@089ee780.c/~CPanel@089ee560.c,089ee620.c/Setup@089ee6e0.c/Config@089ee530.c/
 * Start@089ee520.c. Real per-module vtable (PTR__CPanel_08f7c328, 08f7c320+8, a
 * ground-truth-counted 7-slot array matching CModule's own shape -- dtor pair/Setup/
 * Config/Start/Destroy/GetErrorMsg, same convention as every other CModule-derived
 * per-module vtable in omega_vtables.h/.cpp) lives there, following the project's
 * established per-class vtable-array convention.
 */

#include "panel.h"
#include "poller.h"
#include "omega_vtables.h"

#include <cstdlib>
#include <new>

CPanel::CPanel(const char *name, const char *param2)
	: CModule(name), mParam(param2)
{
	mVtbl = (void *)PTR__CPanel_08f7c328;
	/* mPoller deliberately NOT initialized here -- see panel.h's header comment. */
}

CPanel::~CPanel()
{
	/* mParam destroyed automatically (member, real ~CParameterString()). Base
	 * ~CModule() teardown (free mName, destroy mTasks) is a known, already-
	 * precedented gap -- see panel.h's header comment (same treatment as
	 * CEditor::~CEditor(), editor.cpp). mPoller is never freed here, matching
	 * ground truth's own preserved behavior.
	 */
}

int CPanel::Setup()
{
	const char *panelDrvName = mParam.GetParamStr("PANELDRV");

	void *raw = malloc(0x420);
	mPoller = new (raw) CPoller(*this, panelDrvName);

	CModule::Add(mPoller);
	return 0;
}

int CPanel::Config()
{
	if (mPoller != 0) {
		mPoller->InitButtons();
		mPoller->InitAnalogs();
	}
	return 0;
}

int CPanel::Start()
{
	return 0;
}
