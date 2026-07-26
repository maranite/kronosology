/*
 * panel.h  -  CPanel, the per-module class behind "PanelClass" (mains.cpp's
 * MMainPanel()/config_info.cpp's s_atCreateInfo row 1: {"PanelClass", "Panel",
 * "PANELDRV=PanelDriver"}).
 *
 * Dispatched to close the gap poller.h's own header comment flagged: CPoller's real,
 * definitive ground-truth constructor call site is `CPanel::Setup()`
 * (.text+0x089ee6e0), but `CPanel` itself was still routed through mains.cpp's shared
 * `ModuleFactoryCreateStub` (returns NULL) -- the EXACT same "CModuleConstructor
 * factory currently stubbed to NULL, real per-module class not yet reconstructed"
 * situation `CEditor` was in before `CEditorConstructorCreate()` unlocked it
 * (see mains.cpp's own header comment, [[eva_createusermodules_editor_unlock_2026-07-26]]).
 * This class is that same unlock, applied to `CPanel`/`CPanelConstructor`.
 *
 * REAL CLASS SHAPE (CPanel : public CModule, confirmed via `nm -C`/`objdump -dr` --
 * `CPanel@089ee780.c`/`~CPanel@089ee560.c,089ee620.c`/`Setup@089ee6e0.c`/
 * `Config@089ee530.c`/`Start@089ee520.c`):
 *   +0x00..0x2c  CModule base subobject (module.h)
 *   +0x2c  mPoller  CPoller* -- NOT initialized by the ctor (real, preserved quirk,
 *                    same "ground truth's own ctor never zeroes this field" shape as
 *                    CEditor's own mPanelIfcTask/mAlphaKeybIfcTask, editor.cpp). Safe
 *                    in practice because `CModuleManager`'s own lifecycle always calls
 *                    Setup() (which is the ONLY method that ever writes this field)
 *                    before Config()/Start() (module.h's mState comment) -- confirmed
 *                    by direct read of Config()'s own real body, which null-checks
 *                    this field before dispatching through it, matching that ordering
 *                    guarantee rather than defending against a case that can't occur
 *                    on the real lifecycle path.
 *   +0x30  mParam   CParameterString -- constructed from the ctor's own 2nd argument
 *                    (ground truth: `CreateUserModules()`'s own `param2` field, i.e.
 *                    the literal string "PANELDRV=PanelDriver" for this module).
 *                    `Setup()` queries it for the key "PANELDRV" (`.rodata+0x8f7c2d8`,
 *                    read directly) to get CPoller's own `name` ctor argument
 *                    ("PanelDriver") -- this is the "..." poller.h's own header
 *                    comment left undecoded; now resolved.
 * Real total size 0x3c (60 bytes), confirmed directly from CPanelConstructor::Create's
 * own real `malloc(0x3c)` call (.text+0x089ee340) -- matches 0x2c (CModule) + 4
 * (mPoller) + 0xc (CParameterString) exactly, no gap.
 *
 * `CPanel::CPanel(const char *name, const char *param2)` (.text+0x089ee780, 80 bytes):
 * fully mechanical -- `CModule::CModule(this, name)`, install CPanel's own real vtable
 * (`vtable for CPanel` = 0x08f7c320, install address +8 = 0x08f7c328, confirmed via
 * direct `.rodata` dword read), `CParameterString::CParameterString(&mParam, param2)`.
 * `mPoller` untouched (see above).
 *
 * `CPanel::Setup()` (.text+0x089ee6e0, 160 bytes): `mParam.GetParamStr("PANELDRV")`,
 * `malloc(0x420)` + `CPoller::CPoller(raw, *this, panelDrvName)` (already-real ctor,
 * poller.h), `mPoller = raw`, `CModule::Add(mPoller)` (already-real, module.h) --
 * unconditional `return 0` on every path. This is the exact call CPoller's own header
 * comment described in advance; now implemented for real.
 *
 * `CPanel::Config()` (.text+0x089ee530, 48 bytes): `if (mPoller) { mPoller->InitButtons();
 * mPoller->InitAnalogs(); }` -- both already-real CPoller methods
 * (`CPoller::InitButtons()`/`InitAnalogs()`, .text+0x089f4830/0x089f3c80, poller.h) --
 * unconditional `return 0`.
 *
 * `CPanel::Start()` (.text+0x089ee520, 16 bytes): real, literal `return 0;` -- no
 * other body (confirmed via objdump, same "genuinely empty, not a stub" status as
 * `CEditMan::Config()`/`CDumpManMod::Config()`, editor.h).
 *
 * `CPanel::~CPanel()` (D0 .text+0x089ee560 / D1 .text+0x089ee620): ground truth
 * inlines `CModule::~CModule()`'s own real body (reinstall CModule's vtable, destroy
 * the embedded `mTasks` `COmegaPtrArray` via `Destroy()`, free `mName`) directly into
 * CPanel's own dtor rather than calling it -- `CModule` has no explicit `~CModule()`
 * declared in this reconstruction yet (module.h), so this dtor is given the SAME
 * light-touch treatment `CEditor::~CEditor()` already established for the identical
 * situation (editor.cpp's own header comment: "mEditServer/mEditClient... destructed
 * automatically by the compiler after this body returns"): rely on the implicit
 * member dtor for `mParam` (real `~CParameterString()`, matches ground truth's own
 * destruction order -- derived members before base) and leave the `CModule` base
 * teardown as a known, already-precedented gap rather than duplicating ~40 bytes of
 * `COmegaPtrArray`/free-`mName` logic here. `mPoller` itself is NEVER freed by either
 * real dtor (ground truth's own preserved behavior, not an omission -- same "doesn't
 * complete the job" shape already established for `CEditor::~CEditor()` not freeing
 * `mMainTask`/`mPanelIfcTask`/`mChunkServerTask`). Not on this reconstruction's own
 * live boot path (Eva exits before any module is ever destroyed, same reasoning
 * poller.h's own dtor section already documented for `~CPoller()`).
 */

#ifndef PANEL_H
#define PANEL_H

#include "module.h"
#include "parameter_string.h"

class CPoller;

class CPanel : public CModule {
public:
	/* .text+0x089ee780, 80 bytes. `param2` is the raw "PANELDRV=PanelDriver"
	 * config string (CreateUserModules()'s own table entry, config_info.cpp).
	 */
	CPanel(const char *name, const char *param2);

	/* .text+0x089ee620 (D1, complete-object dtor). See header comment above. */
	~CPanel();

	/* .text+0x089ee6e0, 160 bytes. Real: constructs mPoller and registers it via
	 * CModule::Add(). See header comment above.
	 */
	int Setup();

	/* .text+0x089ee530, 48 bytes. Real: dispatches CPoller::InitButtons()/
	 * InitAnalogs() if mPoller is set.
	 */
	int Config();

	/* .text+0x089ee520, 16 bytes. Real: unconditional `return 0`, no other body. */
	int Start();

private:
	CPoller *mPoller;      /* +0x2c -- NOT initialized by the ctor, see header comment. */
	CParameterString mParam; /* +0x30 */

	/* Not implemented -- same "never copied in ground truth, undeclared-but-unused
	 * to make an accidental copy a link error" convention as CParameterString's own
	 * (parameter_string.h) and this project's other module classes.
	 */
	CPanel(const CPanel &);
	CPanel &operator=(const CPanel &);

	/* Friend accessor for verify/test_panel.cpp -- same "friend pokes private state"
	 * convention as EditorTestHooks/PollerTestHooks (editor.h/poller.h).
	 */
	friend struct PanelTestHooks;
};

#endif /* PANEL_H */
