// SPDX-License-Identifier: GPL-2.0
/*
 * combi_ctor.cpp  -  CSTGCombi::CSTGCombi() (batch 45, `.text+0x8fb40`,
 * 730 bytes).
 *
 * Batch 44 (sec 10.195) resolved the CSTGProgram multiple-inheritance
 * cluster and explicitly deferred this structurally near-identical
 * sibling for "a future batch using this same now-established technique"
 * -- this is that batch. Fresh, full disassembly (no branches, no
 * fall-through -- straight-line, exactly like CSTGProgram::CSTGProgram())
 * confirms CSTGCombi shares CSTGProgram's own base-vtable pair
 * (CSTGPerformance@+0x0, CSTGEffectRack@+0x4) and its ENTIRE sub-object
 * list up through CSTGAudioInput@+0xae7 byte-for-byte identical
 * (twelve CIFXEffectSlot @ the same 0xa8-stride offsets, two
 * CMFXEffectSlot, two CTFXEffectSlot, one CSTGEffectBalance, two
 * CSTGCommonEffectLFO, one CSTGVectorMotion, one CSTGControllerInfo, one
 * CSTGAudioInput -- every one of these sub-object types was ALREADY made
 * real by batch 44, so this batch adds no new sub-object ctors at all).
 * Where CSTGProgram's own ctor continues into a CSTGCommonLFO/
 * CSTGParamsOwner/CSTGStepSeqBase/CSTGCommonStepSeq/CSTGToneAdjust tail,
 * CSTGCombi instead (1) overwrites its own CSTGPerformance base-vtable
 * slot at +0x0 with ITS OWN derived vtable (`_ZTV9CSTGCombi+8`, a
 * confirmed real 0x9c/39-slot vtable per `nm -CS`, NOT reused from
 * anywhere -- CSTGProgram's own overwrite uses a completely different
 * vtable, `_ZTV11CSTGProgram`), and (2) placement-constructs SIXTEEN
 * (not fifteen -- batch 44's own header comment speculated fifteen from
 * memory alone, without having disassembled this function yet; a fresh,
 * independent count off the real disassembly's sixteen `lea`+`call`
 * pairs is what's actually implemented here) embedded CSTGProgramSlot
 * sub-objects at a confirmed real 0xe8-byte stride (+0xb63, +0xc4b,
 * +0xd33, ..., +0x18fb), each followed by CSTGCombi's OWN patch of that
 * slot's +0x4 byte to its own zero-based index (0..15) -- confirmed via
 * `objdump -dr` that `CSTGProgramSlot::CSTGProgramSlot()` itself never
 * touches its own +0x4 byte, so this is a genuine, non-colliding
 * "construct via the real sub-object ctor, then patch one field the
 * ENCLOSING ctor owns" pattern, the same shape CSTGProgram's own ctor
 * used for the CSTGPerformance-vtable overwrite. Finally, ONE more byte
 * is zeroed at +0x19e3 -- immediately past the 16th CSTGProgramSlot's
 * own 0xe8-byte extent (0xb63 + 16*0xe8 == 0x19e3) -- with NO
 * accompanying ctor call, i.e. NOT a 17th CSTGProgramSlot, just a plain
 * CSTGCombi-level scalar field. This cross-checks neatly against
 * oa_global.h's own already-recorded `CSTGSequence : public CSTGCombi`
 * layout (sec 10.153): CSTGSequence's own CSTGHDRTrack array begins at
 * +0x19e7, exactly 4 bytes after this +0x19e3 byte -- consistent with
 * CSTGCombi's own object footprint ending right around there and
 * CSTGSequence's own derived-class tail picking up cleanly after it.
 * Matches this project's own already-recorded "1792 CSTGCombi (14 banks
 * x 128) at 0x19e7 bytes each" total stride (oa_global.h).
 *
 * Safety re-confirmed independently, not just inherited from batch 44's
 * own audit: `CSTGCombi`'s own vtable (`_ZTV9CSTGCombi`) is a BRAND NEW
 * symbol this project has never defined before now, so nothing already
 * built here could possibly have been dispatching through it (it simply
 * didn't exist to dispatch through). The two SHARED base vtables
 * (CSTGPerformance/CSTGEffectRack) were already exhaustively
 * project-wide-grepped by batch 44 (sec 10.195) with zero real dispatch
 * sites found; a fresh `grep -rln` this batch for "CSTGCombi\b" turns up
 * only: oa_global.h (declarations/comments), program_ctor.cpp (its own
 * "deferred" comment, now stale and updated), sequence_ctor.cpp
 * (`CSTGSequence`'s own base-ctor CALL -- install only, confirmed no
 * dispatch), global_ctor.cpp (placement-constructs 1792 instances, see
 * below), and two verify/ files with their own pre-existing local mocks
 * (untouched, see below). CSTGProgramSlot's OWN vtable was already
 * independently audited real/safe by the CSTGProgramSlot::
 * CSTGProgramSlot() promotion itself (sec 10.153) -- this batch adds
 * MORE instances of an already-safe sub-object, not a new risk (its
 * only real dispatcher, CSTGProgramSlot::ChangeProgram's `call
 * *0xe0(*this)`, remains correctly deferred as a bare-`{}` stub in
 * bar2_stubs.cpp, unchanged by this batch).
 *
 * DRIVE-BY FIX, found while re-verifying CSTGProgramSlot's own vtable
 * ahead of adding sixteen MORE placement-new instances of it here: the
 * existing `_ZTV15CSTGProgramSlot` hand-crafted placeholder
 * (program_slot_ctor.cpp, sec 10.153) was declared as only 12 bytes, but
 * a direct `nm -CS` re-check (not `readelf -SW`, per this project's own
 * standing "verify hand-crafted vtable sizes via nm -CS" rule) finds the
 * REAL vtable is 0xf0 bytes (240, 60 slots) -- the exact same class of
 * too-short-placeholder risk sec 10.186 already found and fixed for
 * `CCostProfile`. Currently still harmless here too (nothing
 * reconstructed dispatches through it -- `ChangeProgram` is still a
 * bare-`{}` stub), but since this batch is directly adding MORE
 * placement-new call sites for this exact class, it's corrected now
 * (`_ZTV15CSTGProgramSlot[0xf0]`, oa_global.h + program_slot_ctor.cpp) so
 * the placeholder's own declared size at least matches ground truth,
 * removing the landmine for whichever future batch finally promotes
 * `ChangeProgram`. No file under verify/ has its own local copy of this
 * symbol (confirmed via `grep -rln`), so this is a two-file, size-only
 * change with no other ripple.
 *
 * `CSTGGlobal::CSTGGlobal()` (global_ctor.cpp, already real) ALREADY
 * placement-constructs 1792 `CSTGCombi` objects (14 banks x 128) at
 * `+0x1c77f15` -- promoting this ctor from a no-op stub to a real body is
 * what makes that already-running loop (and `CSTGSequence`'s own,
 * already-real base-ctor call) produce genuinely correctly-shaped
 * objects instead of blank/inert memory.
 *
 * Deliberately its own translation unit (matches the `program_ctor.cpp`/
 * `sequence_ctor.cpp`/`program_slot_ctor.cpp` precedent): confirmed via
 * `grep -l "CSTGCombi::CSTGCombi" verify/` -- two files
 * (`test_sequence_ctor.cpp`, `test_global_ctor.cpp`) each keep their own
 * independent, load-bearing local call-counter mock of this exact ctor;
 * neither links this file, so no multiple-definition collision.
 */

#include "oa_global.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

unsigned char _ZTV9CSTGCombi[0x9c];

static unsigned int ToU32(void *p)
{
	return (unsigned int)(unsigned long)p;
}

CSTGCombi::CSTGCombi()
{
	unsigned char *self = (unsigned char *)this;

	*(unsigned int *)(self + 0x0) = ToU32(_ZTV15CSTGPerformance + 8);
	*(unsigned int *)(self + 0x4) = ToU32(_ZTV14CSTGEffectRack + 8);

	static const unsigned int kIFXOffsets[] = {
		0x8, 0xb0, 0x158, 0x200, 0x2a8, 0x350,
		0x3f8, 0x4a0, 0x548, 0x5f0, 0x698, 0x740,
	};
	for (unsigned int off : kIFXOffsets)
		new (self + off) CIFXEffectSlot();

	/* CMFXEffectSlot #1 @ +0x7e8 */
	self[0x7ec] = 0;
	self[0x7ed] = 0;
	*(unsigned short *)(self + 0x7f0) = 1;
	self[0x7ee] = 0;
	*(unsigned int *)(self + 0x7e8) = ToU32(_ZTV14CMFXEffectSlot + 8);

	/* CMFXEffectSlot #2 @ +0x884 */
	*(unsigned int *)(self + 0x880) = 0;
	self[0x888] = 0;
	self[0x889] = 0;
	*(unsigned short *)(self + 0x88c) = 1;
	self[0x88a] = 0;
	*(unsigned int *)(self + 0x884) = ToU32(_ZTV14CMFXEffectSlot + 8);

	/* CTFXEffectSlot #1 @ +0x920 */
	*(unsigned int *)(self + 0x91c) = 0;
	self[0x924] = 0;
	self[0x925] = 0;
	*(unsigned short *)(self + 0x928) = 1;
	self[0x926] = 0;
	*(unsigned int *)(self + 0x920) = ToU32(_ZTV14CTFXEffectSlot + 8);

	/* CTFXEffectSlot #2 @ +0x9b8 */
	self[0x9bc] = 0;
	self[0x9bd] = 0;
	*(unsigned short *)(self + 0x9c0) = 1;
	self[0x9be] = 0;
	*(unsigned int *)(self + 0x9b8) = ToU32(_ZTV14CTFXEffectSlot + 8);

	/* CSTGEffectBalance @ +0xa55 */
	*(unsigned int *)(self + 0xa55) = ToU32(_ZTV17CSTGEffectBalance + 8);

	/* CSTGCommonEffectLFO x2 @ +0xa59, +0xa6a */
	*(unsigned int *)(self + 0xa59) = ToU32(_ZTV19CSTGCommonEffectLFO + 8);
	self[0xa66] = 0;
	self[0xa68] = 0;
	*(unsigned int *)(self + 0xa6a) = ToU32(_ZTV19CSTGCommonEffectLFO + 8);
	self[0xa77] = 0;
	self[0xa79] = 0;

	new (self + 0xa7b) CSTGVectorMotion();
	new (self + 0xad3) CSTGControllerInfo();
	new (self + 0xae7) CSTGAudioInput();

	/* Overwrite the CSTGPerformance base-vtable ptr with CSTGCombi's own
	 * derived vtable (standard Itanium pattern, same as CSTGProgram's
	 * own overwrite -- but a DIFFERENT vtable symbol). Only +0x0 is
	 * overwritten, +0x4 (CSTGEffectRack) is left as the plain base
	 * pointer, exactly matching CSTGProgram's own confirmed behavior. */
	*(unsigned int *)(self + 0x0) = ToU32(_ZTV9CSTGCombi + 8);

	static const unsigned int kSlotBase = 0xb63;
	static const unsigned int kSlotStride = 0xe8;
	for (unsigned int i = 0; i < 16; i++)
		new (self + kSlotBase + i * kSlotStride) CSTGProgramSlot();

	/* Trailing byte immediately past the 16th CSTGProgramSlot's own
	 * extent (0xb63 + 16*0xe8 == 0x19e3) -- confirmed NOT a 17th
	 * sub-object (no ctor call at this address in ground truth), just a
	 * plain CSTGCombi-level scalar zeroed directly. */
	self[kSlotBase + 16 * kSlotStride] = 0;

	/* Each slot's own +0x4 byte (confirmed untouched by
	 * CSTGProgramSlot::CSTGProgramSlot() itself) is patched by THIS
	 * ctor to its zero-based index -- a real, confirmed
	 * "construct-then-patch-one-field" pattern. */
	for (unsigned int i = 0; i < 16; i++)
		self[kSlotBase + i * kSlotStride + 0x4] = (unsigned char)i;
}
