// SPDX-License-Identifier: GPL-2.0
/*
 * oa_quad.h  -  CSTGQuad / list primitives. Stage 2 shared utility (PLAN.md).
 *
 * Ground-truthed by disassembling CSTGVoiceModel::AddQuad (.text+0x1a9c70,
 * 188 bytes) and CSTGVoiceModel::RemoveQuad (.text+0x1a9d30, 110 bytes) in
 * full. CSTGQuad and the generic list container it lives in are never
 * instantiated as their own standalone functions in the binary (a small
 * value type + inlined template, `TListLinkLite<CSTGQuad>`, both fully
 * inlined at every use site under -O2) -- so unlike every other Stage 1/2
 * unit so far, there is no single function symbol to point at for "this is
 * the class"; the layout below is reconstructed entirely from how AddQuad/
 * RemoveQuad manipulate it.
 *
 * CSTGQuad is a node in a per-priority-bucket, ascending-priority-sorted
 * DOUBLY-LINKED LIST:
 *   +0x00  mNext        CSTGQuad*     (confirmed: standard "insert before"
 *                                      unlink/relink pattern at both ends)
 *   +0x04  mPrev        CSTGQuad*
 *   +0x08  mOwnerList   CSTGQuadList* (which bucket this quad currently
 *                                      belongs to, NULL if unlinked --
 *                                      RemoveQuad no-ops entirely if NULL)
 *   +0x0c..0x13          8 bytes, NOT YET RECOVERED (not touched by either
 *                                 function; presumably the actual per-quad
 *                                 audio-processing payload other
 *                                 ProcessSubRate-style consumers use)
 *   +0x14  mPriority    unsigned short  (sort key; ties insert the NEW quad
 *                                        before existing equal-priority ones)
 *   +0x16  mBucketIndex unsigned short  (selects which CSTGQuadList in the
 *                                        owning object's bucket table --
 *                                        read-only to Add/RemoveQuad, never
 *                                        written by them)
 *
 * CSTGQuadList is the 12-byte bucket header AddQuad/RemoveQuad update:
 *   +0x00  head   CSTGQuad*
 *   +0x04  tail   CSTGQuad*
 *   +0x08  count  unsigned int
 *
 * Confirmed insertion algorithm (CSTGVoiceModel::AddQuad):
 *   bucket = &bucketTable[quad->mBucketIndex]        (12 bytes/entry)
 *   if bucket empty: quad becomes the sole head+tail entry
 *   else if quad->mPriority <= bucket->head->mPriority: prepend
 *   else: walk forward from head while quad->mPriority > cur->mPriority,
 *         insert immediately before the first cur where that's no longer
 *         true (i.e. cur->mPriority >= quad->mPriority), or append at tail
 *         if the walk reaches the end.
 *   Either way: quad->mOwnerList = bucket; bucket->count++; and the owning
 *   object's cached-priority hint (see below) is set to quad->mPriority.
 *
 * Confirmed removal algorithm (CSTGVoiceModel::RemoveQuad): standard
 * doubly-linked unlink (fixing up bucket->head/tail if the removed quad was
 * either), then zeroes mNext/mPrev/mOwnerList and decrements bucket->count.
 * If quad was NOT in any list (mOwnerList == NULL) this is a silent no-op.
 *
 * Both functions also touch a 2-byte "cached priority hint" field on the
 * owning object (CSTGVoiceModel +0xd8, confirmed via disassembly, hence
 * added to CSTGVoiceModel's own declaration in oa_types.h): AddQuad always
 * overwrites it with the just-added quad's priority; RemoveQuad resets it to
 * the sentinel 0xffff only if it currently equals the removed quad's own
 * priority (i.e. "invalidate the hint only if it might have been pointing
 * at the quad we just removed"). Likely a fast-path used by some other,
 * not-yet-reconstructed function to avoid walking a bucket just to read its
 * head's priority; that consumer is not identified in this pass.
 *
 * NOT RECONSTRUCTED HERE (confirmed to exist via relocation, genuinely
 * higher-level than "list primitives", left for Stage 3/4):
 *   CSTGVoiceModel::MoveQuadToCPU(CSTGQuad*, unsigned int)  .text+0x1a9da0 (543 bytes)
 *   CSTGVoiceAllocator::FreeQuad(CSTGVoiceModel*, CSTGQuad*) .text+0x51550 (224 bytes)
 *   CLoadBalancer::NotifyQuadAllocated(CSTGQuad*)            .text+0x616b0 (473 bytes)
 *   CLoadBalancer::NotifyQuadFreed(CSTGQuad*)                .text+0x61890 (259 bytes)
 */

#ifndef OA_QUAD_H
#define OA_QUAD_H

struct CSTGQuadList {
	struct CSTGQuad *head;		/* +0x00 */
	struct CSTGQuad *tail;		/* +0x04 */
	unsigned int     count;	/* +0x08 */
};

struct CSTGQuad {
	struct CSTGQuad     *mNext;		/* +0x00 */
	struct CSTGQuad     *mPrev;		/* +0x04 */
	struct CSTGQuadList *mOwnerList;	/* +0x08 */
	unsigned char        _opaque_0c[8];	/* +0x0c .. +0x13, not yet recovered */
	unsigned short       mPriority;	/* +0x14 */
	unsigned short       mBucketIndex;	/* +0x16 */
};

#endif /* OA_QUAD_H */
