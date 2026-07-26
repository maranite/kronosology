/*
 * notify_list.cpp  -  see include/notify_list.h.
 *
 * All 7 real methods (ctor/dtor/Put/GetList/ReleaseList x2/GrowEventsList/
 * PostKernelDestructor) transcribed from `objdump -dr -M intel` on the ground-truth
 * Decomp/EVA_Decomp/Eva binary (unstripped, real mangled symbols) -- see
 * notify_list.h for the full per-method writeup. Every real malloc/free/free-list
 * pop/push is individually bracketed by its own HAL_DisableInterrupts()/
 * HAL_EnableInterrupts() pair in the disassembly -- dropped here, same established
 * reason as every other occurrence of that pair in this project (ckernel.cpp/
 * omega_interface.cpp/res_man.cpp/scheduler.cpp/task.cpp/...): a kernel-side
 * critical-section shim, not a real userspace primitive, and this reconstruction is
 * single-threaded.
 */

#include "notify_list.h"

#include <cstdlib>

/* Real global, .bss+0x0930a260 (`CNotifyList::sm_ptFirstFreeEvent` in the ground-truth
 * binary's own symbol table) -- the shared free-list head every CNotifyList instance's
 * Put()/ReleaseList()/GrowEventsList() pushes/pops through.
 */
static SNotifyEvent *g_poNotifyFreeList = 0;

namespace {
/* Same "D1/D0 destructor thunk + PreKernelConstructor/PostKernelConstructor/
 * PreKernelDestructor stay the generic base no-ops, only PostKernelDestructor is
 * overridden" shape as every other CGlobalObjectBase-derived vtable in this project
 * (global_object_base.cpp). Not shared across translation units -- each derived
 * class's own file declares its own trivial copies of the 3 unoverridden phase hooks,
 * matching this project's established per-file convention for tiny helpers.
 */
extern "C" {
void CNotifyList_Dtor(void *self)
{
	((CNotifyList *)self)->~CNotifyList();
}

void CNotifyList_DeletingDtor(void *self)
{
	((CNotifyList *)self)->~CNotifyList();
	free(self);
}

int CNotifyList_PreKernelConstructor(unsigned long) { return 0; }
int CNotifyList_PostKernelConstructor(unsigned long) { return 0; }
int CNotifyList_PreKernelDestructor(unsigned long) { return 0; }

int CNotifyList_PostKernelDestructorVSlot(void *self, unsigned long flags)
{
	return ((CNotifyList *)self)->PostKernelDestructor(flags);
}
} // extern "C"
} // namespace

/* Real 6-slot vtable, confirmed by direct raw-byte read of the real Eva binary at
 * .rodata+0x08e81828 (2-word ABI header -- offset-to-top=0, typeinfo -- skipped,
 * matching CGlobalObjectBase's own PTR__CGlobalObjectBase_08e80f08 precedent). Only
 * PostKernelDestructor (slot 5) is a real override; slots 2-4 are the unmodified
 * CGlobalObjectBase phase-hook no-ops, same as every other XxxApiInstance-family
 * vtable this project has checked.
 */
void *PTR__CNotifyList_08e81828[6] = {
	(void *)CNotifyList_Dtor,
	(void *)CNotifyList_DeletingDtor,
	(void *)CNotifyList_PreKernelConstructor,
	(void *)CNotifyList_PostKernelConstructor,
	(void *)CNotifyList_PreKernelDestructor,
	(void *)CNotifyList_PostKernelDestructorVSlot,
};

void CNotifyList::GrowEventsList()
{
	SNotifyEvent *first = 0;
	SNotifyEvent *prev = 0;

	for (int i = 0; i < 32; i++) {
		SNotifyEvent *node = (SNotifyEvent *)malloc(sizeof(SNotifyEvent));

		if (prev != 0)
			prev->next = node;
		else
			first = node;
		prev = node;
	}
	prev->next = 0;

	if (g_poNotifyFreeList != 0)
		prev->next = g_poNotifyFreeList;
	g_poNotifyFreeList = first;
}

CNotifyList::CNotifyList() : CGlobalObjectBase()
{
	mVtbl = &PTR__CNotifyList_08e81828[0];

	/* Real: inlined 32-node GrowEventsList() body when g_poNotifyFreeList starts
	 * empty -- see header comment. */
	if (g_poNotifyFreeList == 0)
		GrowEventsList();

	mFirst = 0;
	mLast = 0;
}

CNotifyList::~CNotifyList()
{
	/* Real: redundantly reasserts this class's own vtable (already installed,
	 * matches ground truth's own no-op re-store) before the implicit
	 * CGlobalObjectBase::~CGlobalObjectBase() call downgrades it. */
	mVtbl = &PTR__CNotifyList_08e81828[0];
}

void CNotifyList::Put(unsigned char group, unsigned char index, unsigned char subIndex)
{
	if (mLast != 0 && mLast->group == group && mLast->index == index &&
	    mLast->subIndex == subIndex) {
		return; /* already the most-recently-posted notification -- no-op */
	}

	SNotifyEvent *node;
	for (;;) {
		node = g_poNotifyFreeList;
		if (node != 0)
			break;
		GrowEventsList();
	}

	g_poNotifyFreeList = node->next;

	node->group = group;
	node->index = index;
	node->subIndex = subIndex;
	node->next = 0;

	if (mFirst == 0) {
		mFirst = node;
		mLast = node;
	} else {
		mLast->next = node;
		mLast = node;
	}
}

SNotifyEvent *CNotifyList::GetList()
{
	static SNotifyEvent *ptFirstNotify = 0;

	if (mFirst == 0)
		return 0;

	ptFirstNotify = mFirst;
	mLast = 0;
	mFirst = 0;

	return ptFirstNotify;
}

void CNotifyList::ReleaseList(SNotifyEvent *first, SNotifyEvent *last)
{
	SNotifyEvent *oldHead = g_poNotifyFreeList;
	g_poNotifyFreeList = first;
	last->next = oldHead;
}

void CNotifyList::ReleaseList(SNotifyEvent *list)
{
	SNotifyEvent *tail = list;
	while (tail->next != 0)
		tail = tail->next;

	ReleaseList(list, tail);
}

int CNotifyList::PostKernelDestructor(unsigned long /*flags*/)
{
	SNotifyEvent *node = g_poNotifyFreeList;
	while (node != 0) {
		SNotifyEvent *next = node->next;
		free(node);
		node = next;
	}
	g_poNotifyFreeList = 0;

	node = mFirst;
	while (node != 0) {
		SNotifyEvent *next = node->next;
		free(node);
		node = next;
	}
	mFirst = 0;
	mLast = 0;

	return 0;
}
