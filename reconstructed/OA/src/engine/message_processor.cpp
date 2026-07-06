// SPDX-License-Identifier: GPL-2.0
/*
 * message_processor.cpp -- CSTGMessageProcessor::ClearUnsolicitedMessages()
 * (batch 20, `.text+0xed2e0`, 52 bytes) plus its sole dependency,
 * CSTGDelayedMsgSender::Clear() (`.text+0xf4e00`, 131 bytes -- the first
 * and only method of the brand-new CSTGDelayedMsgSender class reconstructed
 * this pass; see oa_engine.h for the class comment and the deferral reasons
 * for its other five methods).
 *
 * Its own dedicated translation unit: `verify/test_performance_vars_set_is_dying.cpp`
 * (batch 19) carries a LOAD-BEARING call-counting mock of
 * ClearUnsolicitedMessages (`g_clearUnsolicited++`, asserted "called once")
 * and links only `performance_vars_set_is_dying.cpp`, not this file -- so
 * that mock is left completely untouched, exactly the "give the real body
 * its own TU" technique already used for CSTGMidiQueueWriter::Write (sec
 * 10.83) / audio_input_use_settings.cpp (batch 18). This real body is
 * exercised end-to-end by its own `verify/test_message_processor.cpp`.
 *
 * Host/target pointer-width discipline (sec 10.156/10.164): every node
 * link (+0x0 next, +0x4 prev, +0xc back-ref) and every sender-object list
 * pointer is a 32-bit field on the real target. On this 64-bit host a
 * native `void*` would be 8 bytes and adjacent 4-byte-apart writes would
 * stomp each other, so every access below is an explicit 4-byte dword
 * read/write and pointers are reconstituted via FromU32() -- never a
 * native pointer member. The KAT must place all node objects in MAP_32BIT
 * memory so their real addresses fit in 32 bits.
 */

#include "oa_global.h"
#include "oa_engine.h"

static inline unsigned int ToU32(void *p)
{
	return (unsigned int)(unsigned long)p;
}

static inline unsigned char *FromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}

/* 32-bit dword field accessor -- matches the real code's exclusively
 * 4-byte `mov` accesses to every list/link field. */
static inline unsigned int &U32(void *base, unsigned int off)
{
	return *(unsigned int *)((unsigned char *)base + off);
}

/*
 * CSTGDelayedMsgSender::Clear() -- recycle the whole active doubly-linked
 * message list onto the free list. A literal, branch-faithful
 * transliteration of the 131-byte original (loop head at `.text+0xf4e5e`).
 * Per iteration: pop the current active head, unlink it from the active
 * list, zero its own link fields, then splice it into the free ring just
 * before the current free-tail anchor (or seed the ring if it was empty).
 */
void CSTGDelayedMsgSender::Clear()
{
	unsigned char *self = (unsigned char *)this;
	unsigned int anchor = ToU32(self + 0x10);	/* esi = &sender[0x10] */

	for (;;) {
		unsigned int headV = U32(self, 0x4);
		if (headV == 0)
			break;
		/* The active list's "special" node: when Clear reaches it,
		 * drop the sender's own reference to it so it can't dangle
		 * after recycling. */
		if (headV == U32(self, 0x8))
			U32(self, 0x8) = 0;

		unsigned char *node = FromU32(headV);

		/* Advance the active head to node->next (this happens FIRST in
		 * the original, before the unlink). */
		U32(self, 0x4) = U32(node, 0x0);

		/* Unlink node from the active doubly-linked list. */
		unsigned int prevV = U32(node, 0x4);
		if (prevV != 0)
			U32(FromU32(prevV), 0x0) = U32(node, 0x0);	/* prev->next = node->next */
		unsigned int nextV = U32(node, 0x0);
		if (nextV != 0)
			U32(FromU32(nextV), 0x4) = U32(node, 0x4);	/* next->prev = node->prev */

		U32(node, 0x0) = 0;
		U32(node, 0x4) = 0;
		U32(node, 0xc) = 0;

		/* Push node onto the free list; active count--. */
		unsigned int flTail = U32(self, 0x10);
		U32(self, 0xc) -= 1;
		if (flTail == 0) {
			/* First free node: seed the other-end anchor. node's own
			 * next/prev stay 0 (just zeroed above). */
			U32(self, 0x14) = ToU32(node);
		} else {
			unsigned char *tail = FromU32(flTail);
			unsigned int tprev = U32(tail, 0x4);
			U32(node, 0x4) = tprev;			/* node->prev = tail->prev */
			if (tprev != 0)
				U32(FromU32(tprev), 0x0) = ToU32(node);	/* tail->prev->next = node */
			U32(tail, 0x4) = ToU32(node);		/* tail->prev = node */
			U32(node, 0x0) = flTail;		/* node->next = tail */
		}
		U32(self, 0x10) = ToU32(node);			/* free-tail anchor = node */
		U32(node, 0xc) = anchor;			/* node back-ref = &sender[0x10] */
		U32(self, 0x18) += 1;				/* free count++ */
	}
}

/*
 * CSTGMessageProcessor::ClearUnsolicitedMessages() -- clear all three
 * embedded unsol-msg senders (ProgramSlot at +0x6c, ControllerInfo at
 * +0x608, IFX at +0xb24). A direct 1:1 transliteration; the three Clear()
 * calls are non-virtual direct calls in the original (R_386_PC32 to
 * CSTGDelayedMsgSender::Clear), matched here by the raw-offset casts.
 */
void CSTGMessageProcessor::ClearUnsolicitedMessages()
{
	unsigned char *base = (unsigned char *)this;
	((CSTGDelayedMsgSender *)(base + 0x6c))->Clear();
	((CSTGDelayedMsgSender *)(base + 0x608))->Clear();
	((CSTGDelayedMsgSender *)(base + 0xb24))->Clear();
}
