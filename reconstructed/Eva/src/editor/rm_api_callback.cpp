/*
 * rm_api_callback.cpp  -  see include/rm_api_callback.h.
 */

#include "rm_api_callback.h"
#include "rm_job.h"

CRMApiCallBack::~CRMApiCallBack()
{
	if (mJob) {
		delete mJob;
		mJob = 0;
	}
}
