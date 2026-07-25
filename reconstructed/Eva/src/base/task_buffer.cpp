/*
 * task_buffer.cpp  -  see include/task_buffer.h.
 *
 * Transcribed from:
 *   CTaskBuffer::SendBuffer()   .text+0x08055f20, 137 bytes
 *   CTaskBuffer::~CTaskBuffer() .text+0x08055ec0, 57 bytes
 */

#include "task_buffer.h"

#include <cstdlib>

void *CTaskBuffer::sm_poPool = 0;
int   CTaskBuffer::sm_wCount = 0;

void CTaskBuffer::SendBuffer()
{
	unsigned int *node = *(unsigned int **)&mHead;

	while (node != 0) {
		/* Pop the list head. */
		mHead = (void *)node[0];

		/* Real: `(**(code**)(*(int*)(puVar1[1] + 8) + 8))(puVar1[1] + 8, puVar1 + 1)`.
		 * node[1] is a raw stored value, this-adjusted by +8 (interface-adjustment
		 * thunk pattern -- see header comment), then dispatched through ITS OWN
		 * vtable slot +8, passing the node's own embedded CMessage sub-object
		 * (this node's address + 4, i.e. `node + 1`) as the argument. Never
		 * actually reached given this reconstruction's own data (see header
		 * comment) -- transcribed faithfully anyway, same license as
		 * CScheduler::Exec()'s own unreached bail branches.
		 */
		{
			typedef void (*Fn)(void *, void *);
			void *target = (void *)(node[1] + 8);
			Fn fn = *(Fn *)(*(int *)target + 8);
			fn(target, node + 1);
		}

		if ((*((unsigned char *)node + 0xd) & 2) != 0) {
			void *extra = (void *)node[5];
			if (extra != 0)
				free(extra);
		}

		/* Return the node to the global free-list pool rather than free()ing it
		 * directly.
		 */
		node[0] = (unsigned int)sm_poPool;
		sm_poPool = node;
		sm_wCount = sm_wCount + 1;

		node = *(unsigned int **)&mHead;
	}
}

CTaskBuffer::~CTaskBuffer()
{
	/* Real: walks the GLOBAL sm_poPool free-list, not `this`'s own mHead -- see
	 * header comment.
	 */
	while (sm_poPool != 0) {
		void *node = sm_poPool;
		void *next = *(void **)sm_poPool;
		free(node);
		sm_poPool = next;
	}
}
