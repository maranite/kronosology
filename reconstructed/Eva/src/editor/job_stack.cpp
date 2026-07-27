/*
 * job_stack.cpp  -  see include/job_stack.h.
 */

#include "job_stack.h"
#include "omega_vtables.h"
#include "rm_api_callback.h"
#include "rm_job.h"

#include <cstdlib>
#include <new>

extern "C" {

/* Real vtable content -- see job_stack.h header comment. Slots 2-6 are the
 * real, confirmed-empty CRMApiCallBack::OnXxx no-ops (rm_api_callback.h),
 * inherited unchanged; kept as EvaVTableStub since nothing dispatches
 * through them here (same precedent as CBatchDiskMainTask's own vtables).
 */
void *PTR__CJobStack_08e88608[7] = {
	(void *)EvaVTableStub,        /* [0] ~CJobStack() D1 -- fidelity only */
	(void *)CJobStack_DeletingDtor, /* [1] ~CJobStack() D0 -- real, load-bearing */
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};

void *PTR__CJobStack_secondary_08e886c0[2] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};

void CJobStack_Construct(void *self_)
{
	unsigned char *self = (unsigned char *)self_;

	/* Transient base (CRMApiCallBack) vtable install, matching ground
	 * truth's own base-construct-then-vtable-swap idiom. */
	*(void **)self = PTR__CRMApiCallBack_08e886e8;

	void *jobMem = malloc(0x54);
	new (jobMem) CRMJob(); /* real CRMJob::CRMJob(), rm_job.h */
	*(void **)(self + 0x04) = jobMem;

	/* Final, own vtables. */
	*(void **)self = PTR__CJobStack_08e88608;
	*(void **)(self + 0x08) = PTR__CJobStack_secondary_08e886c0;

	/* Empty job-queue TVector<CRMJob> triple -- the only reachable state
	 * (see header comment). */
	*(void **)(self + 0x0c) = 0;
	*(void **)(self + 0x10) = 0;
	*(void **)(self + 0x14) = 0;
}

void CJobStack_DeletingDtor(void *self_)
{
	unsigned char *self = (unsigned char *)self_;

	/* Re-establish this class's own vtables before running member/base
	 * destruction, matching ground truth's own Itanium-ABI boilerplate
	 * (harmless here since nothing calls back through `self`). */
	*(void **)self = PTR__CJobStack_08e88608;
	*(void **)(self + 0x08) = PTR__CJobStack_secondary_08e886c0;

	CRMJob *begin = *(CRMJob **)(self + 0x0c);
	CRMJob *end   = *(CRMJob **)(self + 0x10);
	for (CRMJob *p = begin; p != end; p = (CRMJob *)((char *)p + 0x54))
		p->~CRMJob();
	if (begin)
		free(begin);
	*(void **)(self + 0x0c) = 0;
	*(void **)(self + 0x10) = 0;
	*(void **)(self + 0x14) = 0;

	/* Base (CRMApiCallBack) subobject destruction: re-establish the base's
	 * own vtable, then destruct+free the owned CRMJob at +0x04. */
	*(void **)self = PTR__CRMApiCallBack_08e886e8;
	CRMJob *job = *(CRMJob **)(self + 0x04);
	if (job) {
		job->~CRMJob();
		free(job);
	}
	*(void **)(self + 0x04) = 0;

	free(self);
}

} // extern "C"
