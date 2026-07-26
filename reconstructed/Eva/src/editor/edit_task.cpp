/*
 * edit_task.cpp  -  see include/edit_task.h.
 *
 * CEditTask::CEditTask/~CEditTask/DoPreload/GetOutLinkName transcribed from
 * CEditTask@08243b80.c/~CEditTask@08243af0.c,08243b20.c/DoPreload@08243ac0.c/
 * GetOutLinkName@08243ca0.c.
 */

#include "edit_task.h"
#include "edit_server.h"
#include "omega_vtables.h"

#include <cstdlib>
#include <new>

namespace {

/* Real ground-truth descCEditTask (.data+0x091b75a0) content was never
 * transcribed (only its address is known) -- see edit_task.h's header
 * comment. A single, real, well-formed sentinel row registers zero
 * descriptors instead of ground truth's real (unknown) set. */
SDescriptor s_descCEditTaskSentinel[1] = {
	/* offset, getterFn, getterExtra, setterFn, setterExtra, elemSize,
	 * unused18, count, minValue, maxValue, defaultValue, notifyId, flags,
	 * group, baseIndex -- 15 fields, edit_server.h. Only `group` (0xff, the
	 * sentinel) is non-zero. */
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0 }
};

} // namespace

CEditTask::CEditTask(const CModule &owner)
	: CTask(owner, "EditTask", 4, 0, 0x804b),
	  CEditable((CEditServer *)((const char *)&owner + 0x2c))
{
	*(void ***)this = PTR__CEditTask_08eac1c8;
	*(void **)((char *)this + 8) = PTR__CEditTask_08eac1e4;

	void *raw = malloc(0x38);
	mOutLink = new (raw) COutLinkMono(*this, "Internal", COutLink::eDirectionOut, 0x8030);
	/* Real ground truth swaps mOutLink's own vtable to `vtable for
	 * CBatchDiskCmds + 8` here -- deliberately NOT modeled, see edit_task.h's
	 * header comment (CBatchDiskCmds's own extra state/methods are never
	 * touched by either real caller below). */

	CTask::Add(mOutLink);
	CEditable::AddDescriptorsMap((CObjectBase *)this, s_descCEditTaskSentinel, false);
}

CEditTask::~CEditTask()
{
	*(void ***)this = PTR__CEditTask_08eac1c8;
	*(void **)((char *)this + 8) = PTR__CEditTask_08eac1e4;
	/* Base ~CTask()/~CEditable() teardown cascades automatically. mOutLink is
	 * never explicitly freed here -- matches ground truth exactly (see
	 * header comment).
	 */
}

void CEditTask::DoPreload()
{
	mOutLink->OutMono(1, 0ul);
}

const char *CEditTask::GetOutLinkName() const
{
	return mOutLink->GetName();
}
