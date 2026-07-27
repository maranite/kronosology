/*
 * slot_pool.h  -  CSlotPool, a fixed-capacity free-list allocator for CSeqScheduler's own
 * per-event slot records; CSlotStateFree, the "free" singleton of a deep, otherwise
 * entirely unmodeled State-pattern family (CSlotState and 9 derived classes: Locked,
 * ToBeFreed, ToBeLocked, Scheduled, Suspended, ToBeStarted, ToBePaused, WaitForLink --
 * symbols.csv shows ~90 real virtual methods across the family, a genuinely deep
 * sequencer-scheduling state machine, out of scope beyond this one singleton).
 *
 * FOUND 2026-07-27, same fresh broad-survey pass as pool.h/res_family.h/
 * kernel_death_notifier.h -- deferred that pass (needed TVector<T,N>, now real, see
 * tvector.h, and CSlotStateFree, reconstructed here); done this follow-up pass.
 *
 * CSlotPool's OWNING object (`CSeqScheduler::sm_oSlotPool`) and the per-slot record type
 * it manages (`CSeqSlot`, confirmed real via its own default ctor at .text+0x08169260,
 * exactly matching this file's own SSlot field-init pattern below byte-for-byte) are both
 * themselves deep, entirely unmodeled subsystems (the sequencer scheduler) -- same
 * "owning class + element class out of scope, this class's own construction/destruction
 * lifecycle is not" precedent as CPool/CResFamily. No `extern CSlotPool sm_oSlotPool;`
 * global declared here (its real ctor call-site capacity arg wasn't traced this pass).
 * SSlot below is NOT a claimed CSeqSlot reconstruction -- just the byte-exact shape
 * CSlotPool's own PreKernelConstructor()/PostKernelDestructor() need to build/tear down
 * the array; CSeqSlot's own ~90-sibling-method-adjacent business logic stays untouched.
 *
 * REAL CSlotPool OBJECT LAYOUT (0x10 bytes, confirmed by CSlotPool::CSlotPool(unsigned)/
 * PreKernelConstructor()/PostKernelDestructor()'s own field offsets):
 *   +0x00  mVtbl      inherited from CGlobalObjectBase
 *   +0x04  mCapacity  ctor's own unsigned argument, stored verbatim
 *   +0x08  mArray     SSlot* -- operator new[]'d by PreKernelConstructor(), NULL until
 *                      then; operator delete[]'d (and re-NULLed) by PostKernelDestructor()
 *   +0x0c  mFreeListHead  SSlot* -- head of the singly-linked free list PreKernelConstructor()
 *                      builds (== mArray itself once built: element 0 is always the
 *                      initial head)
 *
 * REAL SSlot LAYOUT (0x24 = 36 bytes, matches CSeqSlot::CSeqSlot()'s own uniform-init
 * write pattern at .text+0x08169260 exactly):
 *   +0x00  mNext        SSlot* -- free-list link (address of the next free slot, NULL for
 *                        the last one); PreKernelConstructor()'s own real second pass links
 *                        every slot 0..capacity-2 to slot+1, leaving the last slot's own
 *                        mNext at its ctor-default 0.
 *   +0x04  mUnknown04   int, ctor default 0, never rewritten by anything in this file's
 *                        own traced scope
 *   +0x08  mUnknown08   unsigned short, ctor default 0xffff sentinel, never rewritten
 *   +0x0c  mUnknown0c   int, ctor default 0
 *   +0x10  mUnknown10   int, ctor default -1 (0xffffffff) sentinel
 *   +0x14  mState       void* -- PreKernelConstructor()'s real second pass points every
 *                        slot at `&CSlotStateFree::s_oInstance` (ds:0x931b220, confirmed
 *                        by direct disassembly of PreKernelConstructor's own literal
 *                        operand). Declared void* here, not CSlotStateFree*, since nothing
 *                        in this file's own scope ever dereferences it through a real
 *                        vtable dispatch -- see CSlotStateFree's own note below.
 *   +0x18  mIndex       unsigned short -- the slot's own 0-based array index, written by
 *                        PreKernelConstructor()'s real second pass (overwrites the ctor's
 *                        own 0xffff default)
 *   +0x1c  mUnknown1c   int, ctor default 0
 *   +0x20  mUnknown20   int, ctor default 0
 *
 * Deep per-field semantics of the 5 mUnknownXX fields (what CSeqSlot's OWN ~90-sibling-
 * method business logic actually uses them for) are not modeled -- out of scope, matching
 * the same "opaque past what this narrow slice's own real callers touch" convention as
 * CResFamily's own unmodeled business-logic fields.
 *
 * CSlotStateFree itself: ground truth has NO separate `CSlotStateFree::CSlotStateFree()`
 * symbol -- GCC inlined its trivial 1-field (mVtbl-only, sizeof==4) ctor directly into a
 * shared `_GLOBAL__I_*` static initializer (.text+0x0816b860) that ALSO constructs 8
 * sibling CSlotState-family singletons (Locked/ToBeFreed/ToBeLocked/Scheduled/Suspended/
 * ToBeStarted/ToBePaused/WaitForLink, ds:0x931b22c..0x931b280) in the same merged function
 * -- exactly the same "GCC merges multiple TUs' static initializers into one
 * `_GLOBAL__I_*`, only the alphabetically/address-first symbol's global gets a name" shape
 * already established for CKernelDeathNotifier's own g_oKernelDeathNotifier
 * (kernel_death_notifier.h). Modeled here the same way: a genuine `static CSlotStateFree
 * s_oInstance;` C++ global, letting the real toolchain's own static-init machinery produce
 * the equivalent real behavior; the 8 sibling singletons stay unmodeled (CSlotPool never
 * references any of them). Real vtable has ~13 slots (2 dtors + ~11 more State-pattern
 * virtuals inherited from CSlotState -- Error/GetResidualPreStart/TicksForSequenceHappen/
 * RemoveOnClock/RemoveImmediate/Move/InsertOnLink/InsertOnClock/InsertImmediate/Happen/
 * GetStateName_debug) -- out of scope; nothing in CSlotPool's own traced scope ever
 * dispatches through this pointer (it's only ever stored as a raw address into each
 * SSlot.mState), so a 2-slot (D1/D0) EvaVTableStub-backed placeholder is sufficient for
 * structural fidelity, same "install-only, shape not fully modeled" precedent as
 * CWrProtCircularQueue (limiter_base.h) / PTR__TPtrArray_08e80f88 (res_family.cpp).
 * GetStateName_debug() IS included below as a real, trivial (5-byte, `return "..."`)
 * bonus method -- .text+0x081959e0 confirmed via direct .rodata string read (returns
 * the literal class-name string "CSlotStateFree", not "Free" as might be guessed).
 */

#ifndef SLOT_POOL_H
#define SLOT_POOL_H

#include "global_object_base.h"

class CSlotStateFree {
public:
	/* No real ground-truth ctor symbol -- see file header. */
	CSlotStateFree();

	/* .text+0x081959e0, 5 bytes -- confirmed real string content at .rodata+0x8e805fd. */
	const char *GetStateName_debug() const { return "CSlotStateFree"; }

private:
	void *mVtbl;
};

/* Real global, ds:0x0931b220 -- see file header. */
extern CSlotStateFree g_oSlotStateFreeInstance;

class CSlotPool : public CGlobalObjectBase {
public:
	/* .text+0x08169260 (CSeqSlot::CSeqSlot(), the real ground-truth element ctor this
	 * shape matches byte-for-byte) -- see file header, NOT a claimed CSeqSlot
	 * reconstruction.
	 */
	struct SSlot {
		SSlot *mNext;
		int mUnknown04;
		unsigned short mUnknown08;
		int mUnknown0c;
		int mUnknown10;
		void *mState;
		unsigned short mIndex;
		int mUnknown1c;
		int mUnknown20;

		SSlot()
			: mNext(0), mUnknown04(0), mUnknown08(0xffff), mUnknown0c(0),
			  mUnknown10(-1), mState(0), mIndex(0xffff), mUnknown1c(0), mUnknown20(0)
		{
		}
	};

	/* .text+0x081692b0, 50 bytes. Does NOT allocate -- PreKernelConstructor() does. */
	CSlotPool(unsigned capacity);

	/* .text+0x081690e0 (D1) -- soft-assert-logs "Assertion failed" if mArray is still
	 * non-NULL here (i.e. PostKernelDestructor() wasn't run first) -- omitted, same
	 * blanket soft-assert-log convention as pool.h. D0 (.text+0x08169150) not given a
	 * separate method, shape-only in the vtable array (slot_pool.cpp).
	 */
	~CSlotPool();

	/* .text+0x08168cb0, 1061 bytes. operator new[]s an mCapacity-element SSlot array,
	 * links every slot 0..capacity-2 to the next one (slot capacity-1's mNext stays
	 * NULL), stamps each slot's own 0-based index into mIndex, and points every
	 * slot's mState at CSlotStateFree::s_oInstance. Real ground truth's own uniform
	 * "clear pass" (matching SSlot's own default ctor, redundant with operator new[]'s
	 * own per-element construction) then a SEPARATE "build the free list" pass that
	 * re-writes most of the same fields is collapsed here into ordinary array
	 * construction + one linking loop -- same license this project always takes for
	 * GCC-duplicated-but-behaviorally-redundant passes (see tvector.h's own note).
	 */
	int PreKernelConstructor(unsigned long);

	/* .text+0x08168c80, 32 bytes. */
	int PostKernelDestructor(unsigned long);

private:
	unsigned mCapacity;
	SSlot *mArray;
	SSlot *mFreeListHead;
};

#endif /* SLOT_POOL_H */
