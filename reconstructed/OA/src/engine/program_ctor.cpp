// SPDX-License-Identifier: GPL-2.0
/*
 * program_ctor.cpp  -  CSTGProgram::CSTGProgram() (batch 44, sec 10.195,
 * `.text+0xa4c00`, 328 bytes) and its four newly-discovered, previously
 * brand-new sub-object ctors: CIFXEffectSlot::CIFXEffectSlot(),
 * CSTGVectorMotion::CSTGVectorMotion(), CSTGControllerInfo::
 * CSTGControllerInfo(), CSTGCommonLFO::CSTGCommonLFO().
 *
 * This resolves the multiple-inheritance cluster batch 43 (sec 10.194)
 * investigated in depth and left deferred as "too large for one batch" --
 * see MASTER_REFERENCE.md sec 10.195 for the full derivation this batch
 * did to make it tractable: every vtable involved turned out to be much
 * smaller than sec 10.194's own preliminary `readelf -SW` estimate
 * (`CSTGEffectRack` is 0x60/24 slots, not 0x98/38 -- that number belonged
 * to `CSTGPerformance`, a DIFFERENT class this file also needs), and,
 * critically, a project-wide grep (`grep -rln "CSTGProgram\|CSTGCombi\|
 * CSTGPerformance\|CSTGEffectRack\|CIFXEffectSlot\|CMFXEffectSlot\|
 * CTFXEffectSlot"`) confirmed every ALREADY-REAL caller that references
 * any of these classes either (a) only stores/passes the pointer, never
 * dispatches (`CSTGProgramModeDrumTrackSlot::ChangeDrumTrackProgram`), or
 * (b) is itself a non-virtual method already confirmed real
 * (`CSTGPerformance::IsCurrentlyActive`/`SetIsDying`), or (c) only calls
 * into another function that is STILL a bare-`{}` stub
 * (`CSTGPerformanceVars::BeginActivation` -> `EnterActivatingState()`,
 * still stubbed; `CLoadBalancer::BalanceStaticLoadHelper` ->
 * `GetPatchStaticCosts()`, still stubbed; `CSetListEQ::Initialize()`,
 * itself still stubbed) -- i.e. NOTHING already-linked in this project
 * ever actually dispatches through a `CSTGPerformance`/`CSTGEffectRack`/
 * `CIFXEffectSlot`/`CMFXEffectSlot`/`CTFXEffectSlot` vtable today. This
 * makes the multiple-inheritance case safe under the exact same
 * "install vs dispatch" rule (sec 10.153) the ten `CSTGVoiceModel`
 * Model ctors already used (sec 10.193/batch 42) -- just with TWO base
 * vtables to hand-build instead of one, plus a handful of small,
 * genuinely new sub-object ctors uncovered along the way (all of them
 * ALSO vtable-install-only, no dispatch, confirmed via fresh
 * disassembly of each one individually before writing any code).
 *
 * `CSTGGlobal::CSTGGlobal()` (global_ctor.cpp, already real) ALREADY
 * placement-constructs 2944 `CSTGProgram` objects (23 banks x 128) plus
 * one standalone -- promoting this ctor from a no-op stub to a real
 * body is what makes that already-running loop produce genuinely
 * correctly-shaped objects instead of blank memory.
 *
 * Deliberately its own translation unit (matches the `program_slot_ctor.cpp`
 * precedent): `test_global_ctor.cpp`/`test_engine.cpp`/`test_global.cpp`
 * each keep their OWN independent local mock of `CSTGProgram::
 * CSTGProgram()` (confirmed via `grep -l "CSTGProgram::CSTGProgram"`
 * over verify/ -- only `test_global_ctor.cpp` has one, a simple
 * call-counter, `g_programCalls++`); none of them link this file, so no
 * multiple-definition collision.
 *
 * CSTGCombi::CSTGCombi() (a structurally near-identical sibling ctor,
 * same dual-vtable-install shape, same CIFXEffectSlot/CMFXEffectSlot/
 * CTFXEffectSlot/CSTGEffectBalance/CSTGCommonEffectLFO sub-objects) was
 * NOT done in this (batch 44) pass -- deliberately left for a future
 * batch to close out using this same now-established technique, given
 * the time already spent deriving this one.
 *
 * UPDATE (batch 45): now done -- see src/engine/combi_ctor.cpp. Turned
 * out to have SIXTEEN embedded CSTGProgramSlot instances (a fresh
 * disassembly count, not the "15" guessed above from memory alone),
 * not the ToneAdjust/CommonLFO/StepSeq tail this file has.
 */

#include "oa_global.h"
#include "oa_engine_init.h" /* CSTGCommonLFO */
#include "oa_internal.h"    /* placement operator new(size_t, void*) */

static unsigned int ToU32(void *p)
{
	return (unsigned int)(unsigned long)p;
}

unsigned char _ZTV14CIFXEffectSlot[0x88];
unsigned char _ZTV16CSTGVectorMotion[0x60];
unsigned char _ZTV18CSTGControllerInfo[0x60];
unsigned char _ZTV13CSTGCommonLFO[0x7c];
unsigned char _ZTV15CSTGPerformance[0x98];
unsigned char _ZTV14CSTGEffectRack[0x60];
unsigned char _ZTV11CSTGProgram[0x98];
unsigned char _ZTV14CMFXEffectSlot[0x88];
unsigned char _ZTV14CTFXEffectSlot[0x88];
unsigned char _ZTV17CSTGEffectBalance[0x60];
unsigned char _ZTV19CSTGCommonEffectLFO[0x60];
unsigned char _ZTV15CSTGParamsOwner[0x60];
unsigned char _ZTV15CSTGStepSeqBase[0xc];
unsigned char _ZTV17CSTGCommonStepSeq[0x6c];

/*
 * CIFXEffectSlot::CIFXEffectSlot() -- see oa_global.h's own header
 * comment for the confirmed field list.
 */
CIFXEffectSlot::CIFXEffectSlot()
{
	unsigned char *base = (unsigned char *)this;

	base[0x4] = 0;
	base[0x5] = 0;
	*(unsigned short *)(base + 0x8) = 1;
	base[0x6] = 0;
	*(unsigned int *)base = ToU32(_ZTV14CIFXEffectSlot + 8);
	base[0x98] = 0;
	base[0x99] = 0;
	base[0x9a] = 0;
	*(float *)(base + 0x9c) = 64.0f;
	*(unsigned int *)(base + 0xa0) = 0;
	*(unsigned int *)(base + 0xa4) = 0;
	base[0x9b] = 0x19;
}

/*
 * CSTGVectorMotion::CSTGVectorMotion() -- see oa_global.h's own header
 * comment for the confirmed field list (four dword writes of the same
 * packed constant, `0x3b810204`, NOT a float).
 */
CSTGVectorMotion::CSTGVectorMotion()
{
	unsigned char *base = (unsigned char *)this;
	const unsigned int packed = 0x3b810204u;

	*(unsigned int *)base = ToU32(_ZTV16CSTGVectorMotion + 8);
	base[0x4] = 0;
	base[0x5] = 0;
	base[0x6] = 0;
	base[0x7] = 0;
	base[0x8] = 0;
	base[0x9] = 0;
	base[0xa] = 0;
	base[0xb] = 0;
	base[0x2e] = 0;
	base[0x38] = 0;
	base[0x42] = 0;
	base[0x4c] = 0;
	base[0x56] = 0;
	*(unsigned int *)(base + 0x15) = packed;
	*(unsigned int *)(base + 0x19) = packed;
	*(unsigned int *)(base + 0x1d) = packed;
	*(unsigned int *)(base + 0x21) = packed;
}

/*
 * CSTGControllerInfo::CSTGControllerInfo() -- see oa_global.h's own
 * header comment for the confirmed field list (a single contiguous
 * 16-byte zeroed run, +0x4..+0x13, one instruction per byte in the real
 * disassembly, collapsed to a loop here since order doesn't matter for
 * a set of independent zero-writes to disjoint bytes).
 */
CSTGControllerInfo::CSTGControllerInfo()
{
	unsigned char *base = (unsigned char *)this;

	*(unsigned int *)base = ToU32(_ZTV18CSTGControllerInfo + 8);
	for (unsigned int off = 0x4; off <= 0x13; off++)
		base[off] = 0;
}

/*
 * CSTGCommonLFO::CSTGCommonLFO() -- see oa_engine_init.h's own header
 * comment for the confirmed field list (nested multiple inheritance:
 * two vtable pointers of its own, both into the same _ZTV13CSTGCommonLFO
 * symbol at two different sub-offsets).
 */
CSTGCommonLFO::CSTGCommonLFO()
{
	unsigned char *base = (unsigned char *)this;

	base[0x11] = 0;
	*(unsigned int *)(base + 0xd) = 0;
	base[0x1a] = 0;
	base[0x1b] = 0;
	base[0x20] = 0;
	base[0x26] = 0;
	*(unsigned int *)(base + 0x22) = 0;
	base[0x2b] = 0;
	*(unsigned int *)(base + 0x27) = 0;
	base[0x30] = 0;
	*(unsigned int *)(base + 0x2c) = 0;
	*(unsigned int *)base = ToU32(_ZTV13CSTGCommonLFO + 8);
	*(unsigned int *)(base + 0x4) = ToU32(_ZTV13CSTGCommonLFO + 0x6c);
	base[0x1e] = 0;
	base[0x1d] = 0;
}

/*
 * CSTGProgram::CSTGProgram() -- see oa_global.h's own header comment for
 * the full multiple-inheritance derivation. Real instruction order,
 * transcribed literally offset-by-offset from `.text+0xa4c00`:
 *   1. Install the two base vtable pointers (CSTGPerformance@+0x0,
 *      CSTGEffectRack@+0x4).
 *   2. Placement-construct twelve CIFXEffectSlot sub-objects at a
 *      confirmed real 0xa8-byte stride (+0x8, +0xb0, +0x158, ..., +0x740).
 *   3. Two CMFXEffectSlot instances (+0x7e8, +0x884), two CTFXEffectSlot
 *      instances (+0x920, +0x9b8), one CSTGEffectBalance (+0xa55), and
 *      two CSTGCommonEffectLFO instances (+0xa59, +0xa6a) -- NONE of
 *      these four classes has an out-of-line ctor in ground truth
 *      (confirmed via a whole-symbol-table grep), so each is a direct
 *      inline vtable-pointer + scalar field write, not a placement-new
 *      call.
 *   4. CSTGVectorMotion (+0xa7b), CSTGControllerInfo (+0xad3, matching
 *      this project's own already-documented "+0xad3" offset from
 *      sec 10.101), CSTGAudioInput (+0xae7, already real -- sec 10.80).
 *   5. Overwrite the CSTGPerformance base-vtable pointer at +0x0 with
 *      CSTGProgram's own derived vtable (standard Itanium "derived ctor
 *      overwrites the vtable ptr the base ctor just installed" pattern,
 *      same as the ten Model ctors).
 *   6. CSTGCommonLFO (+0xb74), which itself installs two MORE nested
 *      vtable pointers for its own two bases -- then CSTGProgram's own
 *      ctor writes a FURTHER pair of vtable pointers at +0xba5/+0xba9
 *      (CSTGParamsOwner, CSTGStepSeqBase), zeroes 32 bytes at +0xbe3,
 *      then OVERWRITES that same +0xba5/+0xba9 pair with
 *      CSTGCommonStepSeq (a real, confirmed "install base vtables then
 *      overwrite with the more-derived one" pattern one level deeper,
 *      exactly mirroring step 5 above).
 *   7. CSTGToneAdjust (+0xc4d, already real -- sec 10.153), then a
 *      trailing run of confirmed zeroed scalar fields.
 * Every offset below is read directly off the real disassembly, not
 * inferred from any assumed class layout -- this ctor does not need to
 * "understand" the C++ hierarchy semantically to be byte-exact.
 */
CSTGProgram::CSTGProgram()
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

	/* Overwrite the CSTGPerformance base-vtable ptr with our own derived
	 * vtable (standard Itanium pattern -- see header comment). */
	*(unsigned int *)(self + 0x0) = ToU32(_ZTV11CSTGProgram + 8);
	self[0xb73] = 0;

	new (self + 0xb74) CSTGCommonLFO();

	*(unsigned int *)(self + 0xba5) = ToU32(_ZTV15CSTGParamsOwner + 8);
	*(unsigned int *)(self + 0xba9) = ToU32(_ZTV15CSTGStepSeqBase + 8);
	self[0xbb4] = 0;
	*(unsigned int *)(self + 0xbb0) = 0;
	self[0xbc1] = 0;
	*(unsigned int *)(self + 0xbbd) = 0;
	self[0xbc2] = 0;
	for (unsigned int off = 0xbe3; off < 0xbe3 + 0x20; off++)
		self[off] = 0;

	/* Overwrite +0xba5/+0xba9 with the more-derived CSTGCommonStepSeq
	 * vtable (same "install base(s), then overwrite" pattern one level
	 * deeper than the CSTGProgram-level overwrite above). */
	*(unsigned int *)(self + 0xba5) = ToU32(_ZTV17CSTGCommonStepSeq + 8);
	*(unsigned int *)(self + 0xba9) = ToU32(_ZTV17CSTGCommonStepSeq + 0x68);

	self[0xc23] = 0;
	self[0xc24] = 0;
	self[0xc27] = 0;
	self[0xc30] = 0;
	self[0xc46] = 0;
	self[0xc47] = 0;
	self[0xc48] = 0;
	self[0xc49] = 0;
	self[0xc4a] = 0;
	self[0xc4b] = 0;
	self[0xc4c] = 0;

	new (self + 0xc4d) CSTGToneAdjust();

	self[0xcb6] = 0;
	self[0xcb7] = 0;
	self[0xcd5] = 0;
	self[0xcd6] = 0;
	self[0xcd7] = 0;
	self[0xcd8] = 0;
	self[0xcd9] = 0;
	self[0xcda] = 0;
	self[0xcdb] = 0;
	self[0xcdc] = 0;
	self[0xcdd] = 0;
	self[0xcde] = 0;
	self[0xcdf] = 0;
	self[0xce0] = 0;
	self[0xce1] = 0;
	self[0xce2] = 0;
	self[0xce3] = 0;

	*(unsigned int *)(self + 0xb63) = 0;
	*(unsigned int *)(self + 0xb6b) = 0;
	*(unsigned int *)(self + 0xb67) = 0;
	*(unsigned int *)(self + 0xb6f) = 0;
}
