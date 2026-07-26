/*
 * alpha_keyb_ctrl.h  -  CAlphaKeybCtrl, the per-module class behind
 * "AlphaKeybCtrlClass" (mains.cpp's `MMainAlphaKeybCtrl()`). Eva CAlphaKeybCtrl/
 * CAlphaKeybCtrlTask batch, 2026-07-26 -- the same "CModuleConstructor factory
 * currently stubbed to NULL, real per-module class not yet reconstructed" unlock
 * shape as `CEditor`/`CPanel`/`CBatchDiskMan` before it (see mains.cpp's own header
 * comment).
 *
 * REAL CLASS SHAPE (CAlphaKeybCtrl : public CModule, confirmed via `nm -C`/
 * `objdump -dr -M intel` -- `CAlphaKeybCtrl@0823e750.c`/
 * `~CAlphaKeybCtrl@0823e4a0.c,0823e560.c`/`Setup@0823e620.c`/`Config@0823e470.c`/
 * `Start@0823e480.c`):
 *   +0x00..0x2c  CModule base subobject (module.h)
 *   +0x2c  mTask   CAlphaKeybCtrlTask* -- NOT initialized by the ctor (real,
 *                  preserved quirk, same "ground truth's own ctor never zeroes
 *                  this field" shape as CPanel's own mPoller, panel.h). Safe in
 *                  practice for the identical reason: only `Setup()` ever writes
 *                  it, and `CModuleManager`'s own lifecycle always calls Setup()
 *                  before Config()/Start().
 *   +0x30  mParam  CParameterString -- constructed from the ctor's own 2nd
 *                  argument (`CreateUserModules()`'s own config-table param2
 *                  string for this module). `Setup()` queries it for "HIDDRV".
 * Real total size 0x3c (60 bytes), confirmed directly from
 * `CAlphaKeybCtrlConstructor::Create()`'s own real `malloc(0x3c)` call
 * (.text+0x0823e6c0) -- matches 0x2c (CModule) + 4 (mTask) + 0xc
 * (CParameterString) exactly, no gap.
 *
 * `CAlphaKeybCtrl::CAlphaKeybCtrl(const char*, const char*)` (.text+0x0823e750, 56
 * bytes): fully mechanical -- `CModule::CModule(this, name)`, install
 * CAlphaKeybCtrl's own real vtable (`PTR__CAlphaKeybCtrl_08eabb68`, confirmed via
 * direct `.rodata` dword read), `CParameterString::CParameterString(&mParam,
 * param2)`. mTask untouched (see above).
 *
 * `CAlphaKeybCtrl::Setup()` (.text+0x0823e620, 115 bytes): `mParam.GetParamStr
 * ("HIDDRV")`, `malloc(0xc0)` + `CAlphaKeybCtrlTask::CAlphaKeybCtrlTask(raw, *this,
 * hidDrvName)` (already-real ctor, alpha_keyb_ctrl_task.h), `mTask = raw`,
 * `CModule::Add(mTask)` (already-real, module.h) -- unconditional `return 0`.
 *
 * `CAlphaKeybCtrl::Config()` (.text+0x0823e470, 3 bytes): real, literal
 * `return 0;`, no other body (confirmed via objdump).
 *
 * `CAlphaKeybCtrl::Start()` (.text+0x0823e480, 26 bytes): real:
 * `if (mTask) mTask->Initialize();` (already-real, alpha_keyb_ctrl_task.h) --
 * unconditional `return 0`.
 *
 * `CAlphaKeybCtrl::~CAlphaKeybCtrl()` (D0 .text+0x0823e560 / D1 .text+0x0823e4a0):
 * ground truth inlines `CModule::~CModule()`'s own real body directly into this
 * dtor rather than calling it -- SAME light-touch treatment `CPanel::~CPanel()`/
 * `CEditor::~CEditor()` already established for the identical situation (see
 * panel.h's own header comment): rely on the implicit member dtor for mParam,
 * leave the CModule base teardown as the same already-precedented gap. mTask is
 * NEVER freed by either real dtor (ground truth's own preserved behavior, same
 * "doesn't complete the job" shape as CPanel's own mPoller). Not on this
 * reconstruction's own live boot path (Eva exits before any module is ever
 * destroyed).
 *
 * `CAlphaKeybCtrlConstructor` (the `CModuleConstructor`-family factory,
 * `CAlphaKeybCtrlConstructor@0823e7a0.c`/`~CAlphaKeybCtrlConstructor@08240590.c,
 * 082405f0.c`/`Create@0823e6c0.c`) is confirmed to have ZERO reachable callers for
 * its own dedicated ctor/dtor, same as `CPanelConstructor::CPanelConstructor()`
 * (panel.h) -- `MMainAlphaKeybCtrl()` (mains.cpp) builds its own generic 3-word
 * descriptor inline instead (already correctly implemented there, confirmed
 * unchanged this batch). Only `Create()` (the factory's own vtable slot 2, called
 * by `CConfigManager::CreateUserModules()`) has a real, reachable caller -- wired
 * as `CAlphaKeybCtrlConstructorCreate()` in mains.cpp, same
 * `CEditorConstructorCreate()`/`CPanelConstructorCreate()`/
 * `CBatchDiskManConstructorCreate()` shape.
 */

#ifndef ALPHA_KEYB_CTRL_H
#define ALPHA_KEYB_CTRL_H

#include "module.h"
#include "parameter_string.h"

class CAlphaKeybCtrlTask;

class CAlphaKeybCtrl : public CModule {
public:
	/* .text+0x0823e750, 56 bytes. `param2` is the raw config string
	 * (CreateUserModules()'s own table entry, config_info.cpp).
	 */
	CAlphaKeybCtrl(const char *name, const char *param2);

	/* .text+0x0823e4a0 (D1, complete-object dtor). See header comment above. */
	~CAlphaKeybCtrl();

	/* .text+0x0823e620, 115 bytes. Real: constructs mTask and registers it via
	 * CModule::Add(). See header comment above.
	 */
	int Setup();

	/* .text+0x0823e470, 3 bytes. Real: literal `return 0;`, no other body. */
	int Config();

	/* .text+0x0823e480, 26 bytes. Real: `if (mTask) mTask->Initialize();`. */
	int Start();

private:
	CAlphaKeybCtrlTask *mTask;  /* +0x2c -- NOT initialized by the ctor, see
	                              * header comment. */
	CParameterString     mParam; /* +0x30 */

	CAlphaKeybCtrl(const CAlphaKeybCtrl &);
	CAlphaKeybCtrl &operator=(const CAlphaKeybCtrl &);

	friend struct AlphaKeybCtrlTestHooks;
};

#endif /* ALPHA_KEYB_CTRL_H */
