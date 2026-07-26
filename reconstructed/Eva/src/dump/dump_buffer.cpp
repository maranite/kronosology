/*
 * dump_buffer.cpp  -  see include/dump_buffer.h.
 */

#include "dump_buffer.h"
#include "omega_vtables.h"

#include <cstring>

CDumpBuffer::CDumpBuffer(unsigned long requestedCapacity)
	: CCircByteBuffer(requestedCapacity), mRemainingLength(0), mExpectedLength(0)
{
	/* Real: vtable overwrite happens AFTER the base ctor's own final install --
	 * same "vtable installed after the part that can fail" idiom used throughout
	 * this project.
	 */
	*reinterpret_cast<void **>(this) = (void *)PTR__CDumpBuffer_08e85c10;
}

CDumpBuffer::~CDumpBuffer()
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CDumpBuffer_08e85c10;
	/* Base CCircByteBuffer::~CCircByteBuffer() runs automatically (C++ base-class
	 * destructor chaining) -- matches ground truth's own explicit call, same
	 * final-state-equivalent reasoning as sysex_msg_task_base.cpp's own dtor.
	 */
}

void CDumpBuffer::Reset()
{
	CCircByteBuffer::Reset();
	mRemainingLength = 0;
	mExpectedLength = 0;
}

bool CDumpBuffer::Read(unsigned char *dst, unsigned long len)
{
	/* mLimitActive lives one dword past this object's own storage, at the owning
	 * CBufferingTask's own +0xa0 -- see header comment / buffering_task.h.
	 */
	unsigned int limitActive = *reinterpret_cast<unsigned int *>(
		reinterpret_cast<char *>(this) + 0x20);

	unsigned long readLen;
	bool truncated;
	if (limitActive != 0) {
		readLen = len;
		truncated = false;
	} else if (len <= mRemainingLength) {
		readLen = len;
		truncated = false;
	} else {
		readLen = mRemainingLength;
		truncated = true;
	}

	if (!CCircByteBuffer::Read(dst, readLen))
		return false;

	if (limitActive != 0)
		return true;

	/* Real soft asserts (mExpectedLength != 0; readLen <= mRemainingLength, both
	 * Api+0x94) omitted -- both non-enforcing, see header comment.
	 */
	mRemainingLength -= readLen;
	if (mRemainingLength == 0)
		mExpectedLength = 0;

	if (truncated)
		memset(dst + readLen, 0, len - readLen);

	return true;
}

bool CDumpBuffer::Write(const unsigned char *src, unsigned long len)
{
	unsigned int limitActive = *reinterpret_cast<unsigned int *>(
		reinterpret_cast<char *>(this) + 0x20);

	unsigned long writeLen = len;
	if (limitActive == 1 && writeLen > mRemainingLength)
		writeLen = mRemainingLength;

	if (!CCircByteBuffer::Write(src, writeLen))
		return false;

	if (limitActive != 1)
		return true;

	/* Real soft assert (mExpectedLength != 0, Api+0x94) omitted, same as Read(). */
	mRemainingLength -= writeLen;
	if (mRemainingLength == 0)
		mExpectedLength = 0;

	return true;
}
