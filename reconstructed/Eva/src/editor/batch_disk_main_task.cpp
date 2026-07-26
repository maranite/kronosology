/*
 * batch_disk_main_task.cpp  -  see include/batch_disk_main_task.h.
 */

#include "batch_disk_main_task.h"
#include "rm_job.h"
#include "edit_server.h"
#include "omega_vtables.h"

#include <cstdlib>
#include <new>

namespace {

/* Real ground-truth descCBatchDiskMainTask (.data+0x091b7300) content was
 * never transcribed (only its address is known) -- same "single real,
 * well-formed sentinel-only SDescriptor[1]" placeholder convention
 * CEditTask's own descCEditTask already established (edit_task.cpp).
 */
SDescriptor s_descCBatchDiskMainTaskSentinel[1] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0 }
};

} // namespace

CBatchDiskMainTask::CBatchDiskMainTask(const CModule &owner, const char *preloadList)
	: CTask(owner, "BatchDiskMainTask", 6, 2, 0x8030),
	  CEditable((CEditServer *)((const char *)&owner + 0x2c)),
	  CRMApiCallBack(),
	  mUnknown8c(0), mUnknown90(0), mUnknown94(0xff), mUnknown95(0),
	  mUnknownAA(0),
	  mOutLink(0), mGroupListHead(0),
	  mState(0), mUnknownDC(0), mUnknownE0(-1),
	  mDirEntry(), mCZ(1), mUnknown15C(0)
{
	/* Heap CRMJob, owned by the CRMApiCallBack base subobject. */
	mJob = new CRMJob();

	/* Re-install all 3 vtable groups to this class's own real identity
	 * (base ctors already installed their OWN base identities above). */
	*(void ***)this = PTR__CBatchDiskMainTask_08eabec8;
	*(void **)((char *)this + 8) = PTR__CBatchDiskMainTask_08eabee8;
	*(void **)((char *)this + 0x80) = PTR__CBatchDiskMainTask_08eabefc;

	/* mUnknownVec (+0xc8): opaque TVector<int,1>, install vtable pointer,
	 * zero the 3 data pointers -- default-construct-to-empty, see header
	 * comment.
	 */
	*(void **)mUnknownVec = (void *)PTR__TVectorInt_08e86f78;
	*(void **)(mUnknownVec + 4) = 0;
	*(void **)(mUnknownVec + 8) = 0;
	*(void **)(mUnknownVec + 0xc) = 0;

	/* mOutLink (+0xc0): heap COutLinkMulti, real name/direction/mode
	 * immediates from ground truth's own ctor.
	 */
	void *raw = malloc(0x34);
	mOutLink = new (raw) COutLinkMulti(*this, "Signals", COutLink::eDirectionOut, 0x8031);
	CTask::Add(mOutLink);

	mState = 0;

	CEditable::AddDescriptorsMap((CObjectBase *)this, s_descCBatchDiskMainTaskSentinel, false);

	PrepareGroupsForPreload(preloadList);

	mUnknown15C = 0xff;
	mUnknownE0 = -1;
}

CBatchDiskMainTask::~CBatchDiskMainTask()
{
	*(void ***)this = PTR__CBatchDiskMainTask_08eabec8;
	*(void **)((char *)this + 8) = PTR__CBatchDiskMainTask_08eabee8;
	*(void **)((char *)this + 0x80) = PTR__CBatchDiskMainTask_08eabefc;
	/* Real dtor doesn't explicitly free mOutLink either (matches ground
	 * truth exactly, same as CEditTask::~CEditTask() -- edit_task.cpp).
	 * mJob is released by CRMApiCallBack::~CRMApiCallBack() (base dtor,
	 * runs automatically after this body).
	 */
}
