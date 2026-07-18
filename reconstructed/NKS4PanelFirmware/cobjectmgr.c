/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cobjectmgr.c - the firmware's small C++ "active object" manager: a single
 * current-object slot, polled once per dispatch-loop tick, dispatching on a
 * type tag and releasing the object when done.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-17.
 * Anchor: "../cobjectmgr.cpp" has 6 real xrefs (more than CryptoAt88.cpp's or
 * clcdc.cpp's single anchor each) - this subsystem's actual boundary is less
 * clean than those two. Only the functions with a genuinely confirmed
 * "object manager" role are reconstructed here; the other three anchor xrefs
 * are documented as separate, honestly-scoped findings below rather than
 * forced into this file under a label they may not deserve - see
 * README.md's own status section for the full accounting.
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_tick - the confirmed core: called unconditionally once per
 *  master-dispatcher tick (FUN_c0007d1c's sibling FUN_c0008b64 calls this
 *  every single invocation, not gated by any status bit - see
 *  KRONOS_V06R06.VSB.md's "wire-protocol command dispatcher" section for
 *  that context). @0xc0007c2c.
 *
 *  If a "current object" slot (this+0x10) is occupied, reads a one-byte type
 *  tag from the object (offset+3) and dispatches to one of exactly two
 *  handlers (tag -0x3c -> FUN_c0007ad0, tag -0x3a -> FUN_c000769c) - any
 *  other tag value is a hard fault (unrecognized object type). Either way,
 *  the slot is then cleared and the object released (FUN_c0001a80 +
 *  FUN_c0003e04, both not independently traced this pass). This is a
 *  classic "poll the one active managed object, process it, release it"
 *  pattern - a genuinely minimal object manager, not a general allocator or
 *  registry (that's the shared heap allocator found at clcdc.cpp's boundary
 *  - see clcdc.c's own header comment for that correction).
 *
 *  After the object-slot handling, unconditionally polls a hardware-ready
 *  status bit (the same "(*(uint*)(handle+8) & 4)" pattern already seen in
 *  clcdc.c's pixel-fill loops and crypto_at88.c's I2C primitives) and, if
 *  set, calls the shared wait/yield primitive and increments a tick counter
 *  - this function is genuinely called every dispatch cycle regardless of
 *  whether an object is pending, not just when one is.
 * ------------------------------------------------------------------------- */
extern void cobjectmgr_handle_type_a(void);	/* FUN_c0007ad0, tag -0x3c/0xc4, not traced */
extern void cobjectmgr_handle_type_b(void);	/* FUN_c000769c, tag -0x3a/0xc6, not traced */
extern void cobjectmgr_release_object(void *slot);	/* FUN_c0001a80, not traced */
extern void cobjectmgr_object_cleanup(void);		/* FUN_c0003e04, not traced */
extern void cobjectmgr_wait_ready(void);		/* FUN_c000395c, shared with clcdc.c */
extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);

struct cobjectmgr_state {
	uint8_t  pad0[0x10];
	void    *current_object;	/* +0x10: NULL when idle */
};

void cobjectmgr_tick(struct cobjectmgr_state *mgr)	/* FUN_c0007c2c */
{
	if (mgr->current_object) {
		int8_t tag = *((int8_t *)mgr->current_object + 3);

		if (tag == -0x3c)
			cobjectmgr_handle_type_a();
		else if (tag == -0x3a)
			cobjectmgr_handle_type_b();
		else
			crypto_at88_fault(0, 0 /* DAT_c0007cbc */, 0 /* DAT_c0007cc0 */);

		mgr->current_object = 0;
		cobjectmgr_release_object(0 /* DAT_c0007cc4 */);
		cobjectmgr_object_cleanup();
	}

	/* unconditional per-tick hardware-ready poll, independent of the
	 * object-slot handling above - real bookkeeping struct/counter not
	 * further typed here (DAT_c0007cc8, a 5-int-wide state block per the
	 * real decompile's [4] index access). */
	extern int32_t *cobjectmgr_tick_state;		/* DAT_c0007cc8 */
	if ((*(uint32_t *)(*cobjectmgr_tick_state + 8) & 4) != 0) {
		cobjectmgr_wait_ready();
		cobjectmgr_tick_state[4]++;
	}
}

/*
 * cobjectmgr_object_destroy - a real C++ virtual destructor: walks a 15-
 * bucket child-widget hash table (offset+0x4c) and a second linked list
 * (offset+0x148/0x14c) freeing every entry via the shared heap allocator's
 * free() (crypto_at88.c-adjacent finding, NOT reconstructed there - see
 * clcdc.c's own correction note on why the allocator itself lives in its own
 * un-attributed address range, not inside any one subsystem file), frees a
 * single extra pointer (offset+0x54) if present, and - if a vtable-style
 * function pointer is present at offset+0x38 - calls it (offset+0x3c) before
 * possibly recursing into a sibling destroy call. @0xc0015bf8. Zero static
 * callers found (confirmed via xref search) - consistent with this being
 * reached only through a vtable/virtual dispatch, not a direct call, which
 * is exactly what a real C++ destructor being invoked via `delete obj` (a
 * virtual call through the object's own vtable) would look like. This is
 * the strongest evidence in the whole firmware that it's genuinely written
 * in C++ with real virtual dispatch, not just C with __FILE__-per-source-
 * file discipline.
 *
 * NOT reconstructed as executable C here (the real free-list walk logic
 * needs the still-unresolved heap allocator's exact struct layout to
 * express faithfully) - documented as a confirmed structural finding
 * instead, consistent with clcdc.c's own treatment of the neighboring
 * allocator code.
 */
extern void cobjectmgr_object_destroy(void *obj);	/* FUN_c0015bf8, structure only, not transcribed */
/* Correction (re-verification pass, 2026-07-17): the structural description
 * above omitted an early-return guard at the top of the real function -
 * `if (obj == <a fixed self-reference constant, DAT_c0015cd8>) return;` -
 * before any of the hash-bucket/list walking begins. Noted here rather than
 * left silently incomplete. */
