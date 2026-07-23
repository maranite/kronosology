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
 * EnableUpdate(int) (.text+0x0805afb0, not measured) is a Tier-B link-stub -- real
 * update-notification refcounting, out of scope for this pass.
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

class CErrorHandler {
public:
	~CErrorHandler();
	void EnableUpdate(int enable);

private:
	void *mHead; /* this+0x00 -- singly-linked error-node list head; ctor zeroes it */
};

#endif /* ERROR_HANDLER_H */
