/*
 * circ_byte_buffer.cpp  -  see include/circ_byte_buffer.h.
 *
 * Real HAL_DisableInterrupts()/HAL_EnableInterrupts() brackets around the ctor's own
 * `new[]`/dtor's `delete[]` are dropped -- same established "kernel-side critical-
 * section shim, no-op-and-dropped userspace concern" precedent as task.cpp/module.cpp/
 * out_link.cpp/ev_buffers_pool.cpp.
 */

#include "circ_byte_buffer.h"
#include "omega_vtables.h"

#include <cstring>

CCircByteBuffer::CCircByteBuffer(unsigned long requestedCapacity)
	: mVtbl(0), mBuffer(0), mWritePos(0), mReadPos(0), mCount(0), mCapacity(0)
{
	/* Real: find the largest power of two <= requestedCapacity via a bit-by-bit
	 * scan from bit 31 down; collapsed to an equivalent shift-and-test loop -- see
	 * header comment.
	 */
	unsigned cap = 0x80000000u;
	while (cap > 1 && !(cap & requestedCapacity))
		cap >>= 1;
	mCapacity = cap;

	/* Real 2 soft asserts (cap >= 2, cap <= 0x4000) omitted -- see header comment. */
	mBuffer = new unsigned char[mCapacity];

	mVtbl = (void *)PTR__CCircByteBuffer_08e85b68;
}

CCircByteBuffer::~CCircByteBuffer()
{
	mVtbl = (void *)PTR__CCircByteBuffer_08e85b68;
	if (mBuffer != 0)
		delete[] mBuffer;
}

void CCircByteBuffer::Reset()
{
	mWritePos = 0;
	mReadPos = 0;
	mCount = 0;
}

bool CCircByteBuffer::Read(unsigned char *dst, unsigned long len)
{
	if (len > mCount)
		return false;

	unsigned n1 = mCapacity - mReadPos;
	if (len <= n1)
		n1 = (unsigned)len;

	memcpy(dst, mBuffer + mReadPos, n1);
	mReadPos = (mReadPos + n1) & (mCapacity - 1);

	unsigned long remaining = len - n1;
	if (remaining != 0) {
		memcpy(dst + n1, mBuffer + mReadPos, remaining);
		mReadPos = (unsigned)((mReadPos + remaining) & (mCapacity - 1));
	}

	mCount -= (unsigned)len;
	return true;
}

bool CCircByteBuffer::Write(const unsigned char *src, unsigned long len)
{
	if ((unsigned long)(mCapacity - mCount) < len) {
		/* Real: Api+0x90("CCircByteBuffer::Write : overflow!") -- soft diagnostic
		 * only, not modeled, same convention as every other Api+0x90 slot.
		 */
		return false;
	}

	unsigned n1 = mCapacity - mWritePos;
	if (len <= n1)
		n1 = (unsigned)len;

	memcpy(mBuffer + mWritePos, src, n1);
	mWritePos = (mWritePos + n1) & (mCapacity - 1);

	unsigned long remaining = len - n1;
	if (remaining != 0) {
		memcpy(mBuffer + mWritePos, src + n1, remaining);
		mWritePos = (unsigned)((mWritePos + remaining) & (mCapacity - 1));
	}

	mCount += (unsigned)len;
	return true;
}
