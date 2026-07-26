/*
 * rm_job.cpp  -  see include/rm_job.h.
 */

#include "rm_job.h"

#include <cstring>

CRMJob::CRMJob()
	: mUnknown08(1), mUnknown18(1)
{
	std::memset(mUnknown00, 0, sizeof(mUnknown00));
	for (int i = 0; i < 6; i++)
		mUnknown28[i] = 0;
	mUnknown40[0] = 0xff;
	mUnknown40[1] = 0xff;
	mUnknown40[2] = 0xff;
	mUnknown44 = -1;
	mUnknown48 = 0;
	mUnknown4c = 0;
	mUnknown50 = 1;
}

CRMJob::~CRMJob()
{
	/* Real dtor conditionally releases each of the 6 opaque pointers via
	 * their own vtable slot+4 -- never fires in this reconstruction since
	 * nothing sets mUnknown28[] away from the ctor's own zero (see header
	 * comment). Preserved as a real, faithful no-op loop rather than
	 * silently dropped.
	 */
	for (int i = 0; i < 6; i++) {
		if (mUnknown28[i] != 0) {
			/* Real ground truth: (*(void(**)(void*))(*(void**)ptr + 4))(ptr) */
		}
	}
}
