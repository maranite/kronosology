/*
 * buffering_task.cpp  -  see include/buffering_task.h.
 */

#include "buffering_task.h"
#include "module.h"
#include "omega_vtables.h"

#include <cstdlib>

CBufferingTask::CBufferingTask(const CModule &owner)
	: CTask(owner, "DumpManBuffering", 2, 0, 0x800b),
	  mDumpTask(0), mBuffer(0x800), mLimitActive(0), mChunkClient(0), mScratchBuf(0)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CBufferingTask_08e85aa8;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
		(void *)&EvaDataPlaceholder_08e85ac4;

	/* Real 1 soft assert (new[] result non-null) omitted, same convention as
	 * every other soft assert in this project.
	 */
	mScratchBuf = new unsigned char[0x100];
}

CBufferingTask::~CBufferingTask()
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CBufferingTask_08e85aa8;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
		(void *)&EvaDataPlaceholder_08e85ac4;

	if (mScratchBuf != 0)
		delete[] mScratchBuf;

	/* mBuffer (CDumpBuffer) then base CTask destruct automatically -- matches
	 * ground truth's own explicit-call end state, same reasoning as
	 * sysex_msg_task_base.cpp/dump_buffer.cpp's own dtors.
	 */
}

bool CBufferingTask::GetDumpLength(unsigned long &out) const
{
	unsigned int len = mBuffer.ExpectedLength();
	out = len;
	return len != 0;
}

int CBufferingTask::Exec(CMessage & /*msg*/)
{
	/* Tier B -- see header comment. */
	return -1;
}

bool CBufferingTask::Put(const unsigned char * /*data*/, unsigned char /*len*/)
{
	/* Tier B -- see header comment. */
	return true;
}
