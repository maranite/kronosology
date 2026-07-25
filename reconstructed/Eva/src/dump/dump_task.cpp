/*
 * dump_task.cpp  -  see include/dump_task.h.
 */

#include "dump_task.h"
#include "dump_man_state_machine.h"
#include "omega_vtables.h"

#include <cstdlib>
#include <new>

namespace {
typedef void (*VCallFn0)(void *);
typedef void (*VCallFn1)(void *, int);

inline void CallVSlot0(void *obj, int byteOffset)
{
	void *vtbl = *(void **)obj;
	VCallFn0 fn = *(VCallFn0 *)((char *)vtbl + byteOffset);
	fn(obj);
}

inline void CallVSlot1(void *obj, int byteOffset, int arg)
{
	void *vtbl = *(void **)obj;
	VCallFn1 fn = *(VCallFn1 *)((char *)vtbl + byteOffset);
	fn(obj, arg);
}
} // namespace

CDumpTask::CDumpTask(const CModule &owner)
	: CSysExMsgTaskBase(owner, CSysExMsgTaskBase::eCanTransmit, CSysExMsgTaskBase::eNeedsTimeout),
	  mMachine(0), mBufferingTask(0)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CDumpTask_08e85d48;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
		(void *)&EvaDataPlaceholder_08e85d74;

	void *raw = malloc(0x20);
	mMachine = new (raw) CDumpMachine(*this);
	/* Real soft NULL assert on mMachine omitted, same convention as every other
	 * soft assert in this project.
	 */
	mMachine->Init();
}

CDumpTask::~CDumpTask()
{
	CDumpMachine *machine = mMachine;

	*reinterpret_cast<void **>(this) = (void *)PTR__CDumpTask_08e85d48;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
		(void *)&EvaDataPlaceholder_08e85d74;

	/* Real: 2 virtual dispatches through mMachine's OWN vtable (slots +0x20/+0xc)
	 * -- real targets not reconstructed (CDumpManStateMachine's out-of-scope
	 * state-handler family), land on EvaVTableStub. Ground truth does NOT free
	 * mMachine anywhere in this destructor -- a real latent leak, faithfully
	 * reproduced (same status as CSysExMsgTaskBase's own documented mOutLink
	 * leak, sysex_msg_task_base.h).
	 */
	CallVSlot1(machine, 0x20, 0);
	CallVSlot0(machine, 0xc);

	*reinterpret_cast<void **>(this) = (void *)PTR__CSysExMsgTaskBase_08e84c28;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
		(void *)&EvaDataPlaceholder_08e84c50;

	/* Base CSysExMsgTaskBase::~CSysExMsgTaskBase() (reinstalls that class's own
	 * vtable pair again, redundant-but-harmless given the immediately-preceding
	 * swap) then CTask::~CTask() run automatically via C++'s own base-class
	 * destructor chaining. Ground truth's own D1 dtor inlines all 3 levels flat,
	 * ending in a DIRECT `CTask::~CTask()` call instead of going through the
	 * separate `CSysExMsgTaskBase::~CSysExMsgTaskBase()` function -- reaching the
	 * exact same final vtable-identity end state and calling `CTask::~CTask()`
	 * exactly once either way, so chaining through the intermediate here is
	 * behaviorally identical without risking a double-destroy of CTask's own
	 * subobject state that an explicit second call in this body would cause.
	 */
}

void CDumpTask::OnGetMessage(const unsigned char *data, unsigned char len)
{
	/* Real soft NULL assert on `data` omitted. */
	mMachine->OnGetMessage(data, len);
}

void CDumpTask::OnReceiveMessage(unsigned char /*commId*/, const unsigned char *data,
                                    unsigned char len)
{
	/* Real 3 soft asserts omitted -- see header comment. */
	mMachine->OnReceiveMessage(data, len);
}

void CDumpTask::OnTimeout()
{
	/* Real: a genuine tail call, `return mMachine->OnTimeout();` -- see header
	 * comment for why this is re-derived from raw disassembly rather than
	 * Ghidra's own mis-decompiled pseudocode.
	 */
	mMachine->OnTimeout();
}
