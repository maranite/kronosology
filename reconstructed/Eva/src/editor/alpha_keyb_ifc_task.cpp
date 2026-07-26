/* alpha_keyb_ifc_task.cpp - see include/alpha_keyb_ifc_task.h */

#include "alpha_keyb_ifc_task.h"
#include "editable.h"
#include "omega_vtables.h"

#include <new>

CAlphaKeybIfcTask::CAlphaKeybIfcTask(const CModule &owner)
	: CTask(owner, "AlphaKeybIfcTask", 4, 2, 0x804b)
{
	/* Real: (CEditServer*)(param_1 + 0x38) -- CEditor's own CEditServer
	 * sub-object (editor.h). Raw offset arithmetic rather than an
	 * #include "editor.h" dependency -- see header comment.
	 */
	CEditServer *editServer = reinterpret_cast<CEditServer *>(
	    const_cast<unsigned char *>(reinterpret_cast<const unsigned char *>(&owner) + 0x38));
	new (mEditable) CEditable(editServer);

	/* Own primary vtable, overriding CTask::CTask()'s own install. */
	*reinterpret_cast<void **>(this) = (void *)PTR__CAlphaKeybIfcTask_08f25ae8;
	/* CTask's own "mIfcThunk" field (+0x08), specialized for this class
	 * instead of the generic EvaDataPlaceholder_08e82144 identity.
	 */
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
	    (void *)PTR__CAlphaKeybIfcTask_08f25b08;
	/* The real CIfcUnknown-adjusted sub-object RegisterIfc() is called
	 * against.
	 */
	mIfcVtbl = (void *)PTR__CAlphaKeybIfcTask_08f25b1c;

	RegisterIfc(reinterpret_cast<CIfcUnknown *>(&mIfcVtbl));
}

CAlphaKeybIfcTask::~CAlphaKeybIfcTask()
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CAlphaKeybIfcTask_08f25ae8;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
	    (void *)PTR__CAlphaKeybIfcTask_08f25b08;
	/* Real: peels back to the raw, generic CIfcUnknown identity here (not
	 * this class's own tertiary vtable) -- matches every other reconstructed
	 * dtor's "install the base identity right before the inherited cleanup"
	 * idiom.
	 */
	mIfcVtbl = (void *)PTR__CIfcUnknown_08e81d80;

	/* CTask::~CTask() runs automatically after this body returns (real
	 * single C++ inheritance) -- no explicit call needed, same as
	 * CEditor::CMainTask's own precedent.
	 */
}

void CAlphaKeybIfcTask::ProcessCode(IAlphaKeybCode::SKeyboardCode *)
{
	/* Tier B link-stub. Real body is a 963-byte per-keycode dispatch --
	 * see header comment for why this stays out of scope.
	 */
}

void CAlphaKeybIfcTask::Setup()
{
	/* Real ground truth: a bare `ret`, confirmed via objdump -- genuinely
	 * empty, not an unreconstructed stub standing in for something bigger.
	 */
}
