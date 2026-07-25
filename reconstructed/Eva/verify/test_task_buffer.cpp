/*
 * test_task_buffer.cpp  -  host-side known-answer test for CTaskBuffer::SendBuffer()
 * (src/base/task_buffer.cpp, Stage 6, 2026-07-25).
 *
 * CTaskBuffer is a private-field class (mHead is not exposed), so this test drives it
 * through its own real public surface (SendBuffer()) laid over a byte buffer shaped
 * exactly like the real object -- same style as test_level_manager_array.cpp's
 * "synthetic COmegaPtrArray-shaped buffer" approach, needed here because CTaskBuffer's
 * own layout (an 8-byte {mHead, mUnused04} pair, see task_buffer.h) is smaller than
 * `sizeof(CTaskBuffer)` as the compiler lays it out, so a raw byte-offset overlay is
 * used to set mHead directly before calling the real method.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "task_buffer.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Real buffered-message node layout, as read by SendBuffer():
 *   +0x00  next pointer (list link, reused as free-list link after processing)
 *   +0x04  raw "target" value -- this-adjusted by +8, then dispatched through
 *          *its own* vtable slot +8 with (target, node+4) as the call args
 *   +0x0d  flag byte, bit 1 ("& 2") -- if set and +0x14 is non-null, that pointer
 *          gets free()d
 *   +0x14  optional extra payload pointer
 */
struct FakeNode {
	void         *next;       /* +0x00 */
	unsigned int  target;     /* +0x04 */
	unsigned char pad1[0x0c]; /* +0x08..0x13, overlays the +0xd flag byte below */
	void         *extra;      /* +0x14 */
};

/* A minimal fake "target" object: vtable pointer at +0, whose own slot+8 is
 * dispatched with (adjustedTarget, msgArg). Records what it was called with.
 */
static void *g_calledWith_obj;
static void *g_calledWith_arg;
static int   g_callCount;

extern "C" void FakeExecStub(void *obj, void *arg)
{
	g_calledWith_obj = obj;
	g_calledWith_arg = arg;
	g_callCount++;
}

/* Separate vtable array (real object model: an object's first word is a POINTER to
 * its vtable, not an inline array -- see test_run_level.cpp's own identical note).
 */
static void *g_fakeTargetVtbl[3] = { 0, 0, (void *)FakeExecStub };

struct FakeTarget {
	void *vtblPtr; /* points at g_fakeTargetVtbl; slot 2 (byte +8) = FakeExecStub */
};

int main(void)
{
	printf("CTaskBuffer::SendBuffer() known-answer test\n");
	printf("=============================================\n");

	printf("[1] Empty buffer (mHead == NULL) -- no dispatch, no crash\n");
	{
		/* CTaskBuffer's own real layout is 8 bytes {mHead, mUnused04}; overlay it
		 * on a zeroed buffer and reinterpret as the real class to call the real
		 * method.
		 */
		unsigned char raw[sizeof(CTaskBuffer) > 8 ? sizeof(CTaskBuffer) : 8];
		memset(raw, 0, sizeof(raw));
		g_callCount = 0;

		((CTaskBuffer *)raw)->SendBuffer();
		check("no dispatch happened", g_callCount == 0);
	}

	printf("[2] One queued node -- dispatches through (target+8)'s own vtable "
	       "slot+8, passing (node+4) as the message argument, then the node is "
	       "recycled (not freed) via the pool -- head becomes NULL afterward\n");
	{
		FakeTarget ft;
		ft.vtblPtr = g_fakeTargetVtbl;

		FakeNode node;
		memset(&node, 0, sizeof(node));
		node.next = 0;
		/* target - 8, so that (target's stored value + 8) lands on &ft */
		node.target = (unsigned int)((char *)&ft - 8);
		node.pad1[0x0d - 0x08] = 0; /* flag byte clear -- no extra-payload free */
		node.extra = 0;

		unsigned char raw[8];
		*(void **)raw = &node; /* mHead */
		*(int *)(raw + 4) = 0;

		g_callCount = 0;
		g_calledWith_obj = 0;
		g_calledWith_arg = 0;

		((CTaskBuffer *)raw)->SendBuffer();

		check("dispatched exactly once", g_callCount == 1);
		check("dispatch target == &ft (this-adjusted +8)",
		      g_calledWith_obj == (void *)&ft);
		check("dispatch arg == node+4 (embedded message)",
		      g_calledWith_arg == (void *)((char *)&node + 4));
		check("mHead is NULL after draining the single node",
		      *(void **)raw == 0);
	}

	printf("[3] Flag byte bit 1 set + non-null extra payload -- payload gets "
	       "freed (checked indirectly: no crash under a debug allocator, and "
	       "the node is still recycled)\n");
	{
		FakeTarget ft;
		ft.vtblPtr = g_fakeTargetVtbl;

		FakeNode node;
		memset(&node, 0, sizeof(node));
		node.target = (unsigned int)((char *)&ft - 8);
		((unsigned char *)&node)[0x0d] = 2; /* bit 1 set */
		node.extra = malloc(16);

		unsigned char raw[8];
		*(void **)raw = &node;
		*(int *)(raw + 4) = 0;

		g_callCount = 0;
		((CTaskBuffer *)raw)->SendBuffer();

		check("dispatched once", g_callCount == 1);
		check("head drained", *(void **)raw == 0);
	}

	printf("\n%d checks failed\n", g_fail);
	return g_fail != 0;
}
