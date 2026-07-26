/*
 * error_handler.h  -  CErrorHandler, Eva's linked-list-of-errors object (Stage 4).
 *
 * CKernel::CKernel() hand-builds one as a raw 0x10-byte blob (g_poErrorHandler,
 * ckernel.cpp), not through a real CErrorHandler ctor -- see ckernel.cpp's own header
 * comment. ~CErrorHandler() (.text+0x0805add0, 175 bytes) is reconstructed faithfully
 * here as a real, self-contained method operating on `this+0` as a singly-linked list
 * head (walks `node[2]` as "next", dispatches vtable slot +4 -- the real deleting
 * destructor -- on each node): with this pass's own construction (this+0 == 0, ctor
 * zeroes it), the list is always empty and the walk is a real no-op, not a fabricated
 * shortcut.
 *
 * EnableUpdate(int) (.text+0x0805afb0, 61 bytes) -- promoted Tier B -> Tier A
 * 2026-07-26 (broad Tier-B recheck sweep). Real body: sets a this+0xc "update
 * pending" flag, then on enable != 0 posts a WriteMessageToHost(2, 0x24)
 * notification through the global Api's own vtable slot +0xf8 (see .cpp) -- same
 * "set a flag, optionally notify host" shape as CScheduler::EnableUpdate()/
 * CTracer::EnableUpdate(). This also fixed a real LESSON_vtable_dispatch_stub_gap
 * instance: that vtable slot was still wired to the generic EvaVTableStub no-op
 * (omega_vtables.cpp), silently swallowing the notification.
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

class CErrorHandler {
public:
	~CErrorHandler();
	void EnableUpdate(int enable);

private:
	void *mHead; /* this+0x00 -- singly-linked error-node list head; ctor zeroes it */
	/* this+0x4..this+0xb: unknown, never touched by ~CErrorHandler()/EnableUpdate() --
	 * EnableUpdate()'s own this+0xc write uses raw offset arithmetic (see .cpp)
	 * rather than a named field here, since this gap isn't otherwise characterized. */
};

#endif /* ERROR_HANDLER_H */
