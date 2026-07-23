/*
 * error_handler.cpp  -  see include/error_handler.h.
 *
 * ~CErrorHandler() transcribed from _CErrorHandler@0805add0.c (175 bytes) -- the real
 * disassembly is another 8-way-unrolled walk (this one over a singly-linked list, not
 * a flat array), collapsed to a plain while loop here.
 */

#include "error_handler.h"

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

void CErrorHandler::EnableUpdate(int /*enable*/)
{
	/* Tier-B link-stub -- .text+0x0805afb0. See error_handler.h. */
}
