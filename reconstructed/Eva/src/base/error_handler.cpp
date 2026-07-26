/*
 * error_handler.cpp  -  see include/error_handler.h.
 *
 * ~CErrorHandler() transcribed from _CErrorHandler@0805add0.c (175 bytes) -- the real
 * disassembly is another 8-way-unrolled walk (this one over a singly-linked list, not
 * a flat array), collapsed to a plain while loop here.
 */

#include "error_handler.h"
#include "system_api.h"

/* Real global, mains.cpp's own primary singleton (canonical definition there --
 * see sysapi_instance.cpp's own precedent for this same extern-in-a-.cpp pattern).
 */
extern CSystemApi *Api;

namespace {
/* CErrorHandler::EnableUpdate() calls through Api's own abstract vtable (slot
 * +0xf8), not a direct CSysApiInstance:: symbol -- CErrorHandler has no compile-time
 * dependency on the concrete CSysApiInstance type, matching the real disassembly's
 * indirect call. Same idiom as every other file-local CallVSlotN helper in this
 * project (ckernel.cpp/scheduler.cpp/dump_task.cpp).
 */
typedef void (*WriteMessageToHostFn)(void *, int, int);

inline void CallApiWriteMessageToHost(void *api, int a, int b)
{
	void *vtbl = *(void **)api;
	WriteMessageToHostFn fn = *(WriteMessageToHostFn *)((char *)vtbl + 0xf8);
	fn(api, a, b);
}
} // namespace

CErrorHandler::~CErrorHandler()
{
	/* Real: `piVar1 = *(int**)this; while (piVar1 != 0) { next = piVar1[2]; call
	 * vtable-slot-4(piVar1); piVar1 = next; }` -- node[2] (byte offset 8) is the
	 * "next" link; vtable slot +4 is the real deleting destructor for each node.
	 * Always a no-op under this pass's own construction (mHead starts null, nothing
	 * populates it -- see header comment).
	 */
	typedef void (*DtorFn)(void *);

	void *node = mHead;
	while (node != 0) {
		void *next = *(void **)((char *)node + 8);
		void *vtbl = *(void **)node;
		DtorFn fn = *(DtorFn *)((char *)vtbl + 4);
		fn(node);
		node = next;
	}
}

/* .text+0x0805afb0, 61 bytes -- promoted Tier B -> Tier A 2026-07-26 (broad Tier-B
 * recheck sweep). Real body: unconditionally sets this+0xc = 1, then on enable != 0
 * dispatches through the global Api's own vtable slot +0xf8 with args (2, 0x24).
 * That slot, confirmed by direct .rodata byte read of PTR__CSysApiInstance_08e81008
 * (omega_vtables.cpp) at offset 0xf8, is CSysApiInstance::WriteMessageToHost(int,int)
 * itself -- it was wired to the generic EvaVTableStub no-op before this pass (same
 * "LESSON_vtable_dispatch_stub_gap" bug class as CModuleManager::AddModule/
 * CSysApiInstance::RegisterApi's own slot +0x40 fix), silently dead-ending this call.
 * Fixed alongside this reconstruction -- see omega_vtables.cpp.
 */
void CErrorHandler::EnableUpdate(int enable)
{
	*(int *)((char *)this + 0xc) = 1;
	if (enable != 0)
		CallApiWriteMessageToHost(Api, 2, 0x24);
}
