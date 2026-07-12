// SPDX-License-Identifier: GPL-2.0
/*
 * sequence_combi_init.cpp  -  CSTGSequence::Initialize() / CSTGCombi::
 * Initialize() / CSTGProgramSlot::Initialize()+UseDefaults() /
 * CSTGPerformance::Initialize() / CSTGEffectRack::Initialize() (batch
 * 55, sec 10.230).
 *
 * Sec 10.229's own hand-off: after fixing CSTGWaveSeqData::Initialize()/
 * CSetListBank::Initialize(), a live boot cleared that crash and reached
 * a new, later NULL-function-pointer crash one level deeper in the same
 * function -- CSTGGlobal::Initialize()+0x182 -> CSTGGlobal::
 * InitializePerformances()+0x145, EIP:0x0, CR2:0.
 *
 * Root-caused via objdump -sr -j .rodata._ZTV12CSTGSequence on
 * OA_real.ko: InitializePerformances()'s own "block 4" (see
 * init_performances.cpp) does a raw vtable-slot-22 dispatch
 * (`vtbl[0x58/4]`) on each of 200 embedded CSTGSequence array elements
 * (this+0x27cd024, stride 0x1cad, sec 10.77 resolution) -- that slot
 * resolves in ground truth to CSTGSequence::Initialize() ITSELF, but
 * this project's own _ZTV12CSTGSequence placeholder was only 12 bytes
 * (1 function slot past the 8-byte header), so the dispatch read out of
 * bounds and landed on a NULL pointer. Real vtable is 0x9c (156) bytes/
 * 37 slots (readelf/nm -CS-confirmed).
 *
 * Fixed by making CSTGSequence::Initialize() a REAL method
 * (init_performances.cpp's block 4 now calls it directly, bypassing the
 * vtable entirely -- same "known concrete type, skip the vtable"
 * technique as sec 10.228/10.229) and fully tracing its own real call
 * graph, which turned out to be a five-function chain, all small/cheap/
 * boot-reachable (structural field patches and forwarding calls, no
 * DSP/data-table dependency):
 *
 *   CSTGSequence::Initialize()        .text+0xcb1d0  277B
 *     -> CSTGCombi::Initialize()      .text+0x8fa60  196B
 *          -> CSTGProgramSlot::Initialize()  .text+0xac060  42B  (x16)
 *               -> CSTGProgramSlot::UseDefaults()  .text+0 (own symbol)  75B
 *          -> CSTGPerformance::Initialize()  .text+0xb9920  92B
 *               -> CSTGEffectRack::Initialize()  .text+0xbed40  266B
 *
 * Every real vtable-slot-7 dispatch encountered along this chain was
 * independently checked via objdump -sr against the relevant class's
 * own real vtable (_ZTV15CSTGProgramSlot/_ZTV12CSTGHDRTrack/
 * _ZTV21CSTGMetronomeSettings/_ZTV19CSTGCommonEffectLFO) to see whether
 * it resolves to the generic CSTGParamsOwner::UseDefaults() (safe to
 * bypass with a forwarding cast, this project's established idiom) or a
 * genuine per-class override (needs its own real body) -- see each
 * function's own comment below for the per-site finding.
 *
 * Deliberately its own translation unit: none of combi_ctor.cpp/
 * sequence_ctor.cpp/program_slot_ctor.cpp/init_performances.cpp's own
 * existing test mocks reference any of these five methods (confirmed
 * via `grep -rln` over verify/), so adding them here has zero collision
 * risk with any pre-existing ctor-only mock.
 */

#include "oa_global.h"
#include "oa_engine_init.h"	/* for CSTGPerformance::Initialize() */

/*
 * CSTGProgramSlot::UseDefaults() -- this class's OWN override (ground-
 * truthed `.text` symbol, distinct from the generic
 * CSTGParamsOwner::UseDefaults()). First forwards to the generic base
 * implementation (direct, non-virtual call in ground truth -- same
 * established `reinterpret_cast<CSTGParamsOwner*>(this)->UseDefaults()`
 * idiom as CSTGGlobal::Initialize(), sec 10.228), then writes twelve
 * literal index bytes 1..12 into +0x63..+0x6e (a confirmed real,
 * fully-deterministic table -- exact semantic meaning not independently
 * determined, plausibly per-controller-slot default indices).
 */
void CSTGProgramSlot::UseDefaults()
{
	reinterpret_cast<CSTGParamsOwner *>(this)->UseDefaults();

	unsigned char *self = (unsigned char *)this;
	for (unsigned int i = 0; i < 12; i++)
		self[0x63 + i] = (unsigned char)(i + 1);
}

/*
 * CSTGProgramSlot::Initialize() -- confirmed real via CSTGCombi::
 * Initialize()'s own direct (non-virtual) call site below.
 *
 * First action is a genuine virtual dispatch on this object's OWN
 * vtable slot 7 (`mov (%eax),%edx; call *0x1c(%edx)`) -- ground-truthed
 * via objdump -sr -j .rodata._ZTV15CSTGProgramSlot to resolve to
 * CSTGProgramSlot::UseDefaults() ITSELF (an override, NOT the generic
 * CSTGParamsOwner::UseDefaults() -- unlike every other slot-7 site this
 * project has fixed so far). Reproduced as a direct call to this
 * class's own UseDefaults() above, bypassing the vtable fetch (this
 * class's own _ZTV15CSTGProgramSlot[0xf0] placeholder is still
 * zero-filled, sec 10.153/batch 45's own "install vs dispatch" rule).
 *
 * Then copies this slot's own +0x4 index byte (patched in by
 * CSTGCombi::CSTGCombi(), see combi_ctor.cpp) into +0x10 (confirmed
 * zeroed by this class's own ctor, program_slot_ctor.cpp), and finally
 * a DIRECT (non-virtual -- ground truth uses a plain PC32 call, not
 * vtable dispatch) call to the generic CSTGParamsOwner::UseDefaults()
 * on the embedded CSTGToneAdjust sub-object at +0x7f (placement-
 * constructed by this class's own ctor).
 */
void CSTGProgramSlot::Initialize()
{
	UseDefaults();

	unsigned char *self = (unsigned char *)this;
	self[0x10] = self[0x4];

	reinterpret_cast<CSTGParamsOwner *>(self + 0x7f)->UseDefaults();
}

/*
 * CSTGEffectRack::Initialize() -- confirmed real via CSTGPerformance::
 * Initialize()'s own direct (non-virtual) call site below.
 *
 * Sixteen embedded effect-slot sub-objects (twelve CIFXEffectSlot, two
 * CMFXEffectSlot, two CTFXEffectSlot -- the SAME cluster/offsets
 * CSTGCombi::CSTGCombi() already places, byte-for-byte, since this
 * `self` here is the SAME CSTGEffectRack base sub-object at
 * combi_base+4), each patched with a zero-based index byte (+0x4
 * relative to the slot's own base) and a shared "type mismatch" flag
 * byte (+0x6). Ground truth gates the flag byte on a genuine virtual
 * call, `this->GetPerformanceType()` (re-based to the OUTER
 * CSTGPerformance/CSTGCombi/CSTGSequence object via `this-4`, since
 * GetPerformanceType() is declared on the CSTGPerformance base, slot
 * 0x5c) -- comparing the result against the literal 2. Since this
 * method is, in this project's own current call graph, reachable ONLY
 * through CSTGSequence::Initialize()'s own chain (never through a bare
 * CSTGCombi/CSTGProgram), and CSTGSequence::GetPerformanceType() is
 * ground-truthed as a literal `return 2;` (no other logic at all), the
 * comparison always takes its "equal" branch here -- reproduced
 * directly as the ground-truthed constant rather than adding a
 * virtual-dispatch mechanism for a single-caller, single-implementation
 * getter. Two final scalar writes (clear bit 0 of a flags byte, zero a
 * dword) in the gap between the CTFXEffectSlot cluster and the
 * CSTGEffectBalance sub-object CSTGCombi::CSTGCombi() places right
 * after it.
 */
void CSTGEffectRack::Initialize()
{
	unsigned char *self = (unsigned char *)this;

	static const unsigned int kSlotOffsets[16] = {
		0x4,   0xac,  0x154, 0x1fc, 0x2a4, 0x34c, 0x3f4, 0x49c,
		0x544, 0x5ec, 0x694, 0x73c, 0x7e4, 0x880, 0x91c, 0x9b4,
	};

	const unsigned int perfType = 2; /* CSTGSequence::GetPerformanceType(), ground-truthed constant */
	const unsigned char flagByte = (perfType != 2) ? 0x10 : 0x00;

	for (unsigned int i = 0; i < 16; i++) {
		unsigned char *slot = self + kSlotOffsets[i];
		slot[0x4] = (unsigned char)i;
		slot[0x6] = flagByte;
	}

	self[0xa50] &= 0xfe;
	*(unsigned int *)(self + 0xa4c) = 0;
}

/*
 * CSTGPerformance::Initialize() -- confirmed real via CSTGCombi::
 * Initialize()'s own direct (non-virtual) call site below.
 *
 * Calls CSTGEffectRack::Initialize() (direct, non-virtual) on the
 * CSTGEffectRack base sub-object at this+4, then two genuine virtual
 * slot-7 dispatches on the embedded CSTGCommonEffectLFO pair at
 * +0xa59/+0xa6a -- ground-truthed via
 * objdump -sr -j .rodata._ZTV19CSTGCommonEffectLFO to resolve to the
 * generic CSTGParamsOwner::UseDefaults() (no override), bypassed via
 * the established forwarding-cast idiom -- then two DIRECT (non-virtual
 * in ground truth) UseDefaults() calls on the embedded
 * CSTGVectorMotion/CSTGAudioInput sub-objects at +0xa7b/+0xae7, and
 * finally a literal float 1.0f write at +0xb5f (confirmed real
 * immediate, own meaning not determined).
 */
void CSTGPerformance::Initialize()
{
	unsigned char *self = (unsigned char *)this;

	reinterpret_cast<CSTGEffectRack *>(self + 0x4)->Initialize();

	reinterpret_cast<CSTGParamsOwner *>(self + 0xa59)->UseDefaults();
	reinterpret_cast<CSTGParamsOwner *>(self + 0xa6a)->UseDefaults();
	reinterpret_cast<CSTGParamsOwner *>(self + 0xa7b)->UseDefaults();
	reinterpret_cast<CSTGParamsOwner *>(self + 0xae7)->UseDefaults();

	*(float *)(self + 0xb5f) = 1.0f;
}

/*
 * CSTGCombi::Initialize() -- confirmed real via CSTGSequence::
 * Initialize()'s own direct (non-virtual) base-call site below.
 *
 * Calls CSTGProgramSlot::Initialize() (direct, non-virtual) on all
 * sixteen embedded CSTGProgramSlot sub-objects at the SAME +0xb63,
 * 0xe8-stride array CSTGCombi::CSTGCombi() itself builds (combi_ctor.cpp),
 * then CSTGPerformance::Initialize() (direct) on the whole object's own
 * CSTGPerformance base sub-object (+0x0, `this` itself), and finally a
 * genuine virtual dispatch on `this`'s OWN (most-derived) vtable slot 7
 * (`mov (%ebx),%edx; call *0x1c(%edx)`). In this project's current call
 * graph this method is reachable ONLY via CSTGSequence::Initialize()
 * (never on a bare CSTGCombi), so by the time this runs the object's
 * installed vtable is always CSTGSequence's own -- confirmed via
 * objdump -sr -j .rodata._ZTV12CSTGSequence that ITS OWN slot 7
 * (vtable_base+0x24) is the generic, non-overridden
 * CSTGParamsOwner::UseDefaults() -- reproduced as a direct forwarding
 * cast, safe specifically because this method is monomorphic in this
 * project's own graph.
 */
void CSTGCombi::Initialize()
{
	unsigned char *self = (unsigned char *)this;

	static const unsigned int kSlotBase = 0xb63;
	static const unsigned int kSlotStride = 0xe8;
	for (unsigned int i = 0; i < 16; i++)
		reinterpret_cast<CSTGProgramSlot *>(self + kSlotBase + i * kSlotStride)->Initialize();

	reinterpret_cast<CSTGPerformance *>(self)->Initialize();

	reinterpret_cast<CSTGParamsOwner *>(self)->UseDefaults();
}

/*
 * CSTGSequence::Initialize() -- confirmed real via InitializePerformances()'s
 * own block 4 (init_performances.cpp), sec 10.230.
 *
 * Calls the base CSTGCombi::Initialize() first (direct, non-virtual --
 * matching this class's own ctor's "base first" pattern), then a real
 * 16-iteration loop dispatching each embedded CSTGHDRTrack's own vtable
 * slot 7 -- ground-truthed via objdump -sr -j .rodata._ZTV12CSTGHDRTrack
 * to resolve to the generic CSTGParamsOwner::UseDefaults() (no
 * override, despite CSTGHDRTrack overriding the adjacent slot 5,
 * ValidateParamChange), bypassed via the established forwarding cast --
 * then one final DIRECT (non-virtual in ground truth) call to
 * CSTGParamsOwner::UseDefaults() on the CSTGMetronomeSettings sub-object
 * at +0x1ca7.
 */
void CSTGSequence::Initialize()
{
	CSTGCombi::Initialize();

	unsigned char *self = (unsigned char *)this;
	for (unsigned int i = 0; i < 16; i++) {
		unsigned char *track = self + 0x19e7 + i * 0x2c;
		reinterpret_cast<CSTGParamsOwner *>(track)->UseDefaults();
	}

	reinterpret_cast<CSTGParamsOwner *>(self + 0x1ca7)->UseDefaults();
}
