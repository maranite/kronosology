/*
 * dump_man_state_machine.cpp  -  see include/dump_man_state_machine.h.
 */

#include "dump_man_state_machine.h"
#include "dump_task.h"
#include "buffering_task.h"
#include "dump_buffer.h"
#include "sysex_msg_task_base.h"
#include "omega_vtables.h"

const unsigned CDumpManStateMachine::sm_uiBuffSize = 214;       /* 0xd6, real .data value */
const unsigned CDumpManStateMachine::sm_uiPackDataLen = 212;    /* sm_uiBuffSize - 2, real
                                                                   static initializer */
const unsigned CDumpManStateMachine::sm_uiMaxRetryCounter = 4;  /* real .data value */

namespace {
typedef void (*VCallFn1)(void *, int);
inline void CallVSlot1(void *obj, int byteOffset, int arg)
{
	void *vtbl = *(void **)obj;
	VCallFn1 fn = *(VCallFn1 *)((char *)vtbl + byteOffset);
	fn(obj, arg);
}
} // namespace

CDumpManStateMachine::CDumpManStateMachine()
	: mVtbl(0), mUnknown04(0), mUnknown08(0), mUnknown0c(0), mUnknown10(0), mBuffer(0),
	  mUnknown18(0)
{
	mVtbl = (void *)PTR__CDumpManStateMachine_08e85ce8;

	/* Real 1 soft assert (new[] result non-null) omitted, same convention as
	 * every other soft assert in this project.
	 */
	mBuffer = new unsigned char[sm_uiBuffSize];
}

void CDumpManStateMachine::Init()
{
	mUnknown0c = 0;

	/* Real: virtual dispatch through this object's OWN installed vtable slot
	 * +0x20 (index 8), argument 0 -- see header comment. Whatever concrete vtable
	 * is actually installed by now (CDumpMachine's, in every real caller this
	 * pass traces) is EvaVTableStub-backed, so this is a genuine, faithfully-
	 * modeled dispatch that is currently a safe no-op.
	 */
	CallVSlot1(this, 0x20, 0);

	mUnknown08 = 0;
	mUnknown04 = 0;
}

CDumpManStateMachine::~CDumpManStateMachine()
{
	mVtbl = (void *)PTR__CDumpManStateMachine_08e85ce8;
	if (mBuffer != 0)
		delete[] mBuffer;
}

void CDumpManStateMachine::OnGetMessage(const unsigned char * /*data*/, unsigned char /*len*/)
{
	/* Tier B -- see header comment. */
}

void CDumpManStateMachine::OnReceiveMessage(const unsigned char * /*data*/, unsigned char /*len*/)
{
	/* Tier B -- see header comment. */
}

void CDumpManStateMachine::OnTimeout()
{
	/* Tier B -- see header comment. */
}

CDumpMachine::CDumpMachine(CDumpTask &owner)
	: CDumpManStateMachine(), mOwnerTask(&owner)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CDumpMachine_08e85c48;
}

CDumpMachine::~CDumpMachine()
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CDumpMachine_08e85c48;
	/* Base CDumpManStateMachine::~CDumpManStateMachine() runs automatically. */
}

bool CDumpMachine::SetTimeout(unsigned short milliseconds)
{
	mOwnerTask->CSysExMsgTaskBase::SetTimeout(milliseconds);
	return true;
}

void CDumpMachine::SendSexMessage(const unsigned char *data, unsigned char len)
{
	mOwnerTask->CSysExMsgTaskBase::SendMsg(data, len);
}

void CDumpMachine::PutMessage(const unsigned char *data, unsigned char len)
{
	/* Real soft NULL assert on BufferingTask() omitted. */
	mOwnerTask->BufferingTask()->Put(data, len);
}

void CDumpMachine::ReadPacket(unsigned char *data, unsigned char len)
{
	/* Real soft NULL assert on BufferingTask() omitted -- see header comment. */
	CDumpBuffer *buf = reinterpret_cast<CDumpBuffer *>(
		reinterpret_cast<char *>(mOwnerTask->BufferingTask()) + 0x80);
	buf->Read(data, len);
}

void CDumpMachine::WritePacket(const unsigned char *data, unsigned char len)
{
	/* Real soft NULL assert on BufferingTask() omitted -- see header comment. */
	CDumpBuffer *buf = reinterpret_cast<CDumpBuffer *>(
		reinterpret_cast<char *>(mOwnerTask->BufferingTask()) + 0x80);
	buf->Write(data, len);
}

bool CDumpMachine::IsDumpEnded()
{
	/* Real soft NULL assert on BufferingTask() omitted -- see header comment. */
	CDumpBuffer *buf = reinterpret_cast<CDumpBuffer *>(
		reinterpret_cast<char *>(mOwnerTask->BufferingTask()) + 0x80);
	return buf->RemainingLength() == 0;
}
