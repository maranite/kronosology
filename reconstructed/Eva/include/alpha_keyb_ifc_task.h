/*
 * alpha_keyb_ifc_task.h  -  CAlphaKeybIfcTask, one of `CEditor::Setup()`'s 4
 * sibling-task fan-out targets (the batch 6 dedicated `CEditor` pass's own
 * README section flagged this one, alongside `CChunkServerTask`, as "NOT tractable
 * this pass" -- re-investigated and found genuinely tractable, Stage 6 breadth sweep,
 * 2026-07-25).
 *
 * IMPORTANT SCOPE NOTE: unlike `CMainTask`/`CPanelIfcTask`/`CChunkServerTask`,
 * `CAlphaKeybIfcTask` is NOT a nested member of `CEditor` in ground truth (confirmed
 * via symbols.csv's own unqualified mangling, `_ZN17CAlphaKeybIfcTaskC1E...` /
 * `_ZTI17CAlphaKeybIfcTask` -- no `N7CEditor...` prefix, unlike
 * `_ZN7CEditor16CChunkServerTaskC1E...`). It is reconstructed here as a fully
 * standalone class, deliberately NOT wired into `CEditor::Setup()`
 * (editor.cpp/editor.h) to avoid touching those files during the concurrent
 * dedicated `CEditor`/`CPanelIfcTask` work this session -- a future pass, once that
 * territory is free, can wire `CEditor::Setup()`'s "ALPHAKEYBOARD=Yes" branch
 * (already fully reconstructed as a dead condition in editor.cpp, per that file's
 * own comment) to actually construct one of these.
 *
 * Real ctor signature is `CAlphaKeybIfcTask(CEditor const&)` (symbols.csv). Taken
 * here as `const CModule &owner` instead (CEditor IS-A CModule at offset 0, so this
 * is a faithful upcast-only view) specifically to avoid `#include "editor.h"` -- same
 * "raw offset arithmetic into another class rather than a hard header dependency"
 * convention this project already uses (e.g. task.cpp's own read of CModule's private
 * mState field by raw `+0x24` offset). The real ctor's own `(CEditServer*)(owner+0x38)`
 * cast (CEditor's own CEditServer sub-object, editor.h) is reproduced the same way.
 *
 * Real layout, 0x84 bytes total (matches editor.cpp's own `malloc(0x84)` call site):
 *   +0x00..0x7c  CTask base (task.h) -- own vtable at +0x00, "mIfcThunk" secondary
 *                slot at +0x08 (specialized below, not left as CTask's own generic
 *                placeholder)
 *   +0x7c        mEditable -- embedded CEditable (editable.h, 4 bytes: a bare
 *                CEditServer* pointer, itself non-polymorphic)
 *   +0x80        mIfcVtbl -- a bare vtable-pointer slot (no other data), the real
 *                CIfcUnknown-adjusted secondary sub-object `RegisterIfc()` is called
 *                against (CTask::RegisterIfc((CIfcUnknown*)(this+0x80)) in the real
 *                ctor)
 *
 * `CAlphaKeybIfcTask(CEditor const&)` (.text+0x08245e10, 124 bytes) is Tier A. Real
 * body, confirmed via a direct .rodata dword read at 0x08f25ae0 for the 3-vtable
 * cluster this installs (see omega_vtables.h/.cpp's own comment for the full
 * derivation): base-constructs CTask ("AlphaKeybIfcTask", level 4, scheduleFlag 2,
 * lastArg 0x804b -- same literal `0x804b` every other CTask-derived ctor in this
 * project passes), placement-constructs `mEditable` against the owner's own
 * CEditServer sub-object, installs its own primary vtable (overriding CTask's own
 * install), its own specialized `mIfcThunk` identity, and `mIfcVtbl`'s own identity,
 * then calls `CTask::RegisterIfc(reinterpret_cast<CIfcUnknown*>(&mIfcVtbl))` --
 * already-real (Tier A, task.h).
 *
 * `~CAlphaKeybIfcTask()` (.text+0x08245d40, 40 bytes, the D1 complete-object
 * destructor) is Tier A -- reinstalls all 3 vtable identities (the tertiary
 * `mIfcVtbl` slot peels all the way back to the raw, generic
 * `PTR__CIfcUnknown_08e81d80` identity, not this class's own tertiary vtable --
 * matching the "peel to base identity before the inherited cleanup" idiom every
 * other reconstructed dtor in this project follows), then falls through to
 * `CTask::~CTask()` -- modeled here as REAL single C++ inheritance from `CTask`
 * (matching `CEditor::CMainTask`'s own precedent, editor.h), so the base dtor call
 * happens automatically as this destructor's own implicit epilogue rather than an
 * explicit statement. `CEditable` has no destructor of its own in ground truth
 * (functions.csv: only a ctor and `AddDescriptorsMap`), so `mEditable` needs no
 * explicit teardown. The separate D0 "deleting destructor" variant
 * (.text+0x08245d90, 64 bytes = this same body + `free(this)`) is not modeled
 * separately, same convention already established for every other reconstructed
 * class's D0/D1 split in this project (e.g. `CBatchDiskMan`'s own, edit_server.h-
 * adjacent classes).
 *
 * `ProcessCode(IAlphaKeybCode::SKeyboardCode*)` (.text+0x08245960, 963 bytes) stays
 * Tier B -- genuine algorithmic depth (real per-keycode dispatch logic), same
 * "several-hundred-plus-bytes, pulls in further subsystems" bar as
 * `CEditor::CMainTask::Exec()`. `IAlphaKeybCode`/`SKeyboardCode` themselves are not
 * reconstructed elsewhere in this project -- kept as an opaque forward-declared
 * nested type, pointer-only.
 */

#ifndef ALPHA_KEYB_IFC_TASK_H
#define ALPHA_KEYB_IFC_TASK_H

#include "task.h"

class CEditServer;

/* Opaque forward declaration -- real class not reconstructed anywhere in this
 * project. Only ever used as an incomplete pointer type (ProcessCode's Tier-B
 * argument), matching this project's convention for undecoded interface types.
 */
class IAlphaKeybCode {
public:
	struct SKeyboardCode;
};

class CAlphaKeybIfcTask : public CTask {
public:
	/* .text+0x08245e10, 124 bytes. See header comment -- `owner` stands in for
	 * the real ctor's `CEditor const&` argument (CModule is CEditor's own base
	 * at offset 0).
	 */
	explicit CAlphaKeybIfcTask(const CModule &owner);

	/* .text+0x08245d40, 40 bytes (D1). See header comment. */
	~CAlphaKeybIfcTask();

	/* .text+0x08245960, 963 bytes. Tier B link-stub -- see header comment. */
	void ProcessCode(IAlphaKeybCode::SKeyboardCode *code);

private:
	unsigned char mEditable[4]; /* embedded CEditable, editable.h */
	void         *mIfcVtbl;
};

#endif /* ALPHA_KEYB_IFC_TASK_H */
