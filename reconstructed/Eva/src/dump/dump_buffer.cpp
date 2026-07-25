/*
 * dump_buffer.cpp  -  see include/dump_buffer.h.
 */

#include "dump_buffer.h"
#include "omega_vtables.h"

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

bool CDumpBuffer::Read(unsigned char * /*dst*/, unsigned long /*len*/)
{
	/* Tier B -- see header comment. Not reachable from this pass's own wired call
	 * graph (only CDumpMachine::ReadPacket(), itself only called from the
	 * genuinely out-of-scope CDumpManStateMachine state-handler family).
	 */
	return false;
}

bool CDumpBuffer::Write(const unsigned char * /*src*/, unsigned long /*len*/)
{
	/* Tier B -- see header comment / Read()'s own twin comment above. */
	return false;
}
