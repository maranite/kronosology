// SPDX-License-Identifier: GPL-2.0
/*
 * filesys.cpp  -  CFilesys method bodies (round 46, solo). See
 * include/filesys.h for the full object-layout derivation and the
 * deliberately-deferred methods' 3 distinct reasons.
 */
#include "filesys.h"

static unsigned int msg;
static int *buf;

CFilesys::~CFilesys()
{
	/* Real dtor: 2 ground-truth addresses, genuinely different bodies
	 * (11-byte vptr-reset-only vs 39-byte vptr-reset +
	 * HAL_DisableInterrupts()/free(this)/HAL_EnableInterrupts()) --
	 * modeled as vptr-reset only, see header comment. */
	mVptrPlaceholder[0] = mVptrPlaceholder[1] = mVptrPlaceholder[2] = mVptrPlaceholder[3] = 0;
}

void CFilesys::eventhandling() {}
void CFilesys::startup() {}

void CFilesys::run()
{
	/* Real ground truth: `do {} while(true)` with a completely empty
	 * body -- see header comment. Deliberately never called. */
	for (;;) {
	}
}

int CFilesys::new_fptr(int, int value)
{
	return value + 0x4c;
}

void CFilesys::CheckError(int errCode)
{
	if (errCode == 0 && mStickyErrorFlag == 0) {
		mStickyErrorFlag = 3;
		buf[2] = 1;
		*buf = 0;
		return;
	}
	*buf = errCode;
}

void *CFilesys::get_fileioptr(unsigned int fileIoType) const
{
	switch (fileIoType) {
	default:
		return mDefaultDriver;
	case 1:
		return mFileIoPtr1;
	case 2:
		return mFileIoPtr2;
	case 3:
		return mFileIoPtr3;
	case 4:
		return mFileIoPtr4;
	case 5:
		return mFileIoPtr5;
	case 6:
		return mFileIoPtr6;
	}
}

void *CFilesys::get_fileioptr(const char *path, int *outDeviceId) const
{
	int idx = 10;
	if (path[1] == ':')
		idx = path[0] - 'A';
	if (outDeviceId)
		*outDeviceId = idx;
	if (idx < 10)
		return mDeviceDrivers[idx];
	return mDefaultDriver;
}

void CFilesys::setbuf(unsigned int msgType, int *pbuf)
{
	msg = msgType;
	buf = pbuf;
	buf[2] = 0;
}
