/*
 * alpha_keyb_ctrl.cpp  -  see include/alpha_keyb_ctrl.h.
 *
 * CAlphaKeybCtrl::CAlphaKeybCtrl/~CAlphaKeybCtrl/Setup/Config/Start transcribed from
 * CAlphaKeybCtrl@0823e750.c/~CAlphaKeybCtrl@0823e4a0.c/Setup@0823e620.c/
 * Config@0823e470.c/Start@0823e480.c. Real per-module vtable
 * (PTR__CAlphaKeybCtrl_08eabb68, omega_vtables.h/.cpp) follows the project's
 * established per-class vtable-array convention.
 *
 * Vtable install uses `*reinterpret_cast<void**>(this) = ...` (not a named `mVtbl`
 * member access) -- same convention `CEditor::CMainTask`/`CPanel` already use to
 * avoid needing a `friend class` grant into `module.h` for a field CModule's own
 * ctor already installs a (different, base) identity into.
 */

#include "alpha_keyb_ctrl.h"
#include "alpha_keyb_ctrl_task.h"
#include "omega_vtables.h"

#include <cstdlib>
#include <new>

CAlphaKeybCtrl::CAlphaKeybCtrl(const char *name, const char *param2)
	: CModule(name), mParam(param2)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CAlphaKeybCtrl_08eabb68;
	/* mTask deliberately NOT initialized here -- see header comment. */
}

CAlphaKeybCtrl::~CAlphaKeybCtrl()
{
	/* mParam destroyed automatically (member, real ~CParameterString()). Base
	 * ~CModule() teardown is a known, already-precedented gap (see header
	 * comment, same treatment as CPanel::~CPanel()/CEditor::~CEditor()). mTask
	 * is never freed here, matching ground truth's own preserved behavior.
	 */
}

int CAlphaKeybCtrl::Setup()
{
	const char *hidDrvName = mParam.GetParamStr("HIDDRV");

	void *raw = malloc(0xc0);
	mTask = new (raw) CAlphaKeybCtrlTask(this, hidDrvName);

	CModule::Add(mTask);
	return 0;
}

int CAlphaKeybCtrl::Config()
{
	return 0;
}

int CAlphaKeybCtrl::Start()
{
	if (mTask != 0)
		mTask->Initialize();
	return 0;
}
