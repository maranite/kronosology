// SPDX-License-Identifier: GPL-2.0
/*
 * quad_list.cpp  -  see include/oa_quad.h and CSTGVoiceModel's AddQuad/
 * RemoveQuad declarations in include/oa_types.h.
 *
 * Ground-truthed offsets: CSTGVoiceModel::AddQuad .text+0x1a9c70 (188 bytes),
 * CSTGVoiceModel::RemoveQuad .text+0x1a9d30 (110 bytes).
 */

#include "oa_types.h"
#include "oa_quad.h"

/* Insert `quad` immediately before `cur` in `bucket`'s list, fixing up
 * bucket->head if `cur` was the head. Matches the shared insert sequence at
 * .text+0x1a9ca0/0x1a9cb5 (reused by both the "prepend before head" and
 * "insert mid-list" cases in the real disassembly). */
static void insert_before(struct CSTGQuadList *bucket, struct CSTGQuad *cur, struct CSTGQuad *quad)
{
	struct CSTGQuad *prev = cur->mPrev;

	quad->mPrev = prev;
	if (prev)
		prev->mNext = quad;
	cur->mPrev = quad;
	quad->mNext = cur;

	if (cur == bucket->head)
		bucket->head = quad;
}

/* Append `quad` after the current tail. Matches .text+0x1a9ce6-0x1a9d0b,
 * including the disassembly's own defensive `if (!tail)` branch (reachable
 * only if bucket->head is set but bucket->tail is not, which should not
 * happen given how AddQuad itself always sets both -- preserved as
 * compiled, not simplified away). */
static void append_at_tail(struct CSTGQuadList *bucket, struct CSTGQuad *quad)
{
	struct CSTGQuad *tail = bucket->tail;

	if (!tail) {
		bucket->head = quad;
	} else {
		struct CSTGQuad *tailNext = tail->mNext;
		quad->mPrev = tail;
		quad->mNext = tailNext;
		if (tailNext)
			tailNext->mPrev = quad;
		tail->mNext = quad;
	}
	bucket->tail = quad;
}

void CSTGVoiceModel::AddQuad(struct CSTGQuad *quad)
{
	struct CSTGQuadList *bucket = &quadBuckets[quad->mBucketIndex];
	struct CSTGQuad *head = bucket->head;
	unsigned short newPriority = quad->mPriority;

	if (!head) {
		bucket->tail = quad;
		bucket->head = quad;
	} else if (head->mPriority >= newPriority) {
		/* new quad sorts at or before the current head: prepend */
		insert_before(bucket, head, quad);
	} else {
		/* walk forward from head->mNext for the first node whose priority
		 * is >= newPriority, and insert before it; append at tail if the
		 * walk reaches the end. */
		struct CSTGQuad *cur = head->mNext;
		while (cur && newPriority > cur->mPriority)
			cur = cur->mNext;

		if (cur)
			insert_before(bucket, cur, quad);
		else
			append_at_tail(bucket, quad);
	}

	quad->mOwnerList = bucket;
	bucket->count++;
	cachedQuadPriority = quad->mPriority;
}

void CSTGVoiceModel::RemoveQuad(struct CSTGQuad *quad)
{
	struct CSTGQuadList *bucket = quad->mOwnerList;

	if (bucket) {
		if (quad == bucket->head)
			bucket->head = quad->mNext;
		if (quad == bucket->tail)
			bucket->tail = quad->mPrev;

		if (quad->mPrev)
			quad->mPrev->mNext = quad->mNext;
		if (quad->mNext)
			quad->mNext->mPrev = quad->mPrev;

		quad->mNext = 0;
		quad->mPrev = 0;
		quad->mOwnerList = 0;
		bucket->count--;
	}

	/*
	 * FAITHFUL QUIRK, preserved rather than "fixed": the real disassembly
	 * loads the cached priority with a SIGN-extending move (movsx) but the
	 * quad's own priority with a ZERO-extending move (movzx), then compares
	 * the two as 32-bit values. For priorities under 0x8000 (the expected
	 * range) this is indistinguishable from a plain unsigned compare; it
	 * only diverges if a priority's top bit is ever set, in which case the
	 * sign-extended cache value becomes negative and can never match.
	 */
	if ((int)(short)cachedQuadPriority == (int)quad->mPriority)
		cachedQuadPriority = 0xffff;
}
