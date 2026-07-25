// SPDX-License-Identifier: GPL-2.0
/*
 * audio_input_mixer.cpp  -  CSTGAudioInputMixerBase's four setters (sec
 * 10.150): SetPan/SetFXCtrlBus/SetOutputBus/SetHDRBus.
 *
 * Deliberately a SEPARATE translation unit from global.cpp (matching the
 * CSTGMidiQueueWriter::Write precedent, sec 10.83/10.150): test_engine.cpp,
 * test_global.cpp, and test_global_ctor.cpp all carry their own
 * PRE-EXISTING call-counting mocks for these four methods, load-bearing
 * for roughly twenty CSTGAudioInput-focused assertions across them --
 * rewiring all of that onto the real bodies is a separate, larger task,
 * deliberately left untouched this pass. The real bodies here are instead
 * exercised directly by their own dedicated verify/test_audio_input_mixer.cpp.
 *
 * All five data tables below were extracted directly from OA_real.ko's own
 * `.rodata` section (file offset = section file offset 0x5d3800 + the
 * symbol's own address, confirmed via readelf -S/-sW; independently
 * confirmed to carry NO relocations in that byte range via readelf -r, so
 * these are genuine raw integer data, not unresolved pointers) -- not
 * guessed or inferred from call-site behavior.
 */

#include "oa_global.h"
#include "oa_engine.h"		/* CSTGAudioBusManager::sGlobalBusSet */
#include "oa_bank_memory.h"	/* CSTGBankMemory::AllocAligned */
#include "oa_new_delete.h"	/* oa_size_t, for operator new[] below */

static unsigned char *FromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}
static unsigned int ToU32(void *p)
{
	return (unsigned int)(unsigned long)p;
}

/*
 * Placeholder vtable for CSTGAudioInputMixerBase (batch 58) -- own real
 * target NOT identified in ground truth (matches this project's
 * established "reconstruct the caller, defer the vtable slot" pattern,
 * e.g. sec 10.157's `_ZTV9CSTGVoice`). UNLIKE that precedent, slot 3 here
 * genuinely IS dispatched by already-real code (SetFXCtrlBus/SetHDRBus/
 * SetSendBuses below) reachable from CSTGAudioInput::UpdateFXControlBus/
 * UpdateHDRBus once a performance activates -- so it is populated with a
 * safe, non-crashing stand-in (returns 0) rather than left zero-filled.
 *
 * A plain `void*[6]` struct (not a `ToU32`'d packed byte array) so that
 * `sizeof(void*)` -- and therefore this table's own total size and the
 * "+8" vtable-ptr convention's actual byte offset -- naturally matches
 * whichever target this file is compiled for: 24 bytes/4-byte slots on
 * the real 32-bit kernel build (byte-identical to ground truth's own
 * confirmed `nm -S` size for `_ZTV23CSTGAudioInputMixerBase`), or a
 * host-native 48-byte/8-byte-slot table for this file's own KAT -- both
 * self-consistent with the SAME `(*(void***)this)[3]`-style raw
 * dispatch SetFXCtrlBus/SetHDRBus/SetSendBuses already use, since the
 * ctor below only ever hands out `&slot0`, never a hardcoded byte offset.
 */
namespace {
struct AudioInputMixerBaseVtable {
	void *offsetToTop;
	void *rtti;
	void *slot0;
	void *slot1;
	void *slot2;
	void *slot3;
};
int AudioInputMixerBaseVtableSlot3Placeholder(void *, int) { return 0; }
AudioInputMixerBaseVtable g_audioInputMixerBaseVtable = {
	0, 0, 0, 0, 0,
	(void *)&AudioInputMixerBaseVtableSlot3Placeholder,
};
} /* anonymous namespace */

/*
 * CSTGAudioInputMixerBase::CSTGAudioInputMixerBase() (batch 58,
 * `.text+0x68a60`, 18 bytes) confirmed: installs the vtable pointer
 * (real ground truth: `vtable-for-CSTGAudioInputMixerBase + 8`, the
 * established Itanium-ABI convention this project already uses
 * elsewhere), zeroes `_gap4[0]` and `mixerStateArray32`. `busChangeArray32`
 * (`+0xc`) is confirmed NOT touched by this ctor at all -- left exactly
 * as-is, a real quirk faithfully preserved.
 */
CSTGAudioInputMixerBase::CSTGAudioInputMixerBase()
{
	/*
	 * Deliberately NOT `vtablePtr32 = ToU32(...)`: `SetFXCtrlBus`/
	 * `SetHDRBus`/`SetSendBuses` all dispatch slot 3 via `*(void***)this`
	 * -- a full HOST-NATIVE (8-byte) pointer read on this file's own
	 * 64-bit KAT build, matching test_audio_input_mixer.cpp's own
	 * established "vtable ptr is the one deliberate exception to the
	 * packed-32-bit-field convention" precedent (see that file's own
	 * header comment). Writing through `*(void**)this` here lets
	 * `sizeof(void*)` do the right thing on BOTH targets natively: a
	 * genuine 4-byte store into `vtablePtr32` alone on the real 32-bit
	 * kernel build (byte-identical to ground truth's own `movl
	 * $vtable+8,(%eax)`), or a full 8-byte store spanning
	 * `vtablePtr32`+`_gap4[0..3]` on this 64-bit host KAT build (the ONLY
	 * way a real host pointer to `g_audioInputMixerBaseVtable` can
	 * round-trip through the SAME dispatch code the real target uses).
	 *
	 * ORDER NOTE (found via a real host KAT segfault, not by inspection):
	 * `_gap4[0] = 0` is written BEFORE the vtable-pointer store below --
	 * on the real 32-bit target this is unobservable either way (the two
	 * writes touch disjoint 4-byte fields there, matching ground truth's
	 * own final byte state regardless of order). On this 64-bit HOST KAT
	 * build there IS no separate "gap" once a real pointer needs all 8
	 * bytes, so GCC correctly treats `_gap4[0] = 0` as a dead store
	 * (fully overwritten by the very next line) and elides it entirely
	 * -- meaning `_gap4[0]`'s own OBSERVED host value after this ctor
	 * runs is really just byte 4 of whatever ASLR slide
	 * `g_audioInputMixerBaseVtable` landed at that run, not 0.
	 * test_audio_input_mixer.cpp's own ctor KAT deliberately does NOT
	 * assert `_gap4[0]==0` for exactly this reason (see its own comment
	 * there). `CSTGAudioInputMixerBase::Initialize()` (batch 22, already
	 * real) independently OVERWRITES `_gap4[0]` with its own `count`
	 * argument regardless, on both targets -- so nothing downstream of
	 * this ctor ever actually depends on `_gap4[0]`'s ctor-time value
	 * surviving anyway.
	 */
	_gap4[0] = 0;
	*(void **)this = (void *)&g_audioInputMixerBaseVtable.slot0;
	mixerStateArray32 = 0;
}

/* STGAPIOutToBusType[26]/STGAPIOutToPhysBusId[26] (sec 10.150, confirmed
 * real .rodata, 0x68 bytes each, indexed by the caller's own
 * eSTGAPIBusIDOut `value`) -- used by SetOutputBus. */
static const int STGAPIOutToBusType[26] = {
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 4, 3, 4, 3, 4, 3, 4, 2, 2, 2, 2, 0
};
static const int STGAPIOutToPhysBusId[26] = {
	48, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76,
	38, 38, 40, 40, 42, 42, 44, 44, 38, 40, 42, 44, 32
};

/* STGAPIFXCtrlToWritePhysBusId[3] (confirmed real .rodata, 12 bytes) --
 * used by SetFXCtrlBus. */
static const int STGAPIFXCtrlToWritePhysBusId[3] = { 32, 78, 80 };

/* STGAPIHDRBusTypes[7]/STGAPIHDRPhysBusIds[7] (confirmed real .rodata,
 * 28 bytes each) -- used by SetHDRBus. */
static const int STGAPIHDRBusTypes[7] = { 0, 3, 4, 3, 4, 2, 2 };
static const int STGAPIHDRPhysBusIds[7] = { 32, 146, 146, 148, 148, 146, 148 };

/*
 * SetPan(unsigned int, float) (`.text+0x68df0`, 84 bytes) confirmed:
 * calls `CSTGPan::CalculateMonoPanCoeffs(coeffs, 1.0f, value)` into a
 * local `STGMonoPanCoeffs`, then stores the two result floats at
 * `mixerStateArray + busIndex*0x90 + 0x0`/`+0x4`.
 */
void CSTGAudioInputMixerBase::SetPan(unsigned int busIndex, float value)
{
	STGMonoPanCoeffs coeffs;
	CSTGPan::CalculateMonoPanCoeffs(coeffs, 1.0f, value);

	unsigned char *entry = FromU32(mixerStateArray32) + busIndex * 0x90;
	*(float *)(entry + 0x0) = coeffs.coeff0;
	*(float *)(entry + 0x4) = coeffs.coeff4;
}

/*
 * SetFXCtrlBus(unsigned int, int) (`.text+0x68ea0`, 54 bytes) confirmed:
 * a RAW indirect call through this object's own vtable slot 3 (`call
 * *0xc(%esi)`, `this`=eax, arg=edx unchanged from entry -- NOT this
 * project's C++ virtual dispatch, matching the established raw-vtable
 * convention, sec 10.149), passing `STGAPIFXCtrlToWritePhysBusId[value]`;
 * the returned int is stored at `mixerStateArray + busIndex*0x90 + 0x68`.
 */
void CSTGAudioInputMixerBase::SetFXCtrlBus(unsigned int busIndex, int value)
{
	typedef int (*Fn)(void *, int);
	Fn fn = ((Fn *)(*(void ***)this))[3];
	int result = fn(this, STGAPIFXCtrlToWritePhysBusId[value]);

	unsigned char *entry = FromU32(mixerStateArray32) + busIndex * 0x90;
	*(int *)(entry + 0x68) = result;
}

/*
 * SetOutputBus(unsigned int, int) (`.text+0x68e50`, 68 bytes) confirmed:
 * calls `StartBusChange()` as a genuine MEMBER method on the embedded
 * `CBusChangeStateMachine` at `busChangeArray + busIndex*0x10` (this
 * project's oa_global.h has the full confirmed register mapping).
 */
void CSTGAudioInputMixerBase::SetOutputBus(unsigned int busIndex, int value)
{
	CBusChangeStateMachine *bcsm =
		(CBusChangeStateMachine *)(FromU32(busChangeArray32) + busIndex * 0x10);
	bcsm->StartBusChange(STGAPIOutToPhysBusId[value], STGAPIOutToBusType[value], 0x38);
}

/*
 * SetHDRBus(unsigned int, int) (`.text+0x68ee0`, 170 bytes) confirmed:
 * dispatches the SAME raw vtable slot 3 as SetFXCtrlBus (passing
 * `STGAPIHDRPhysBusIds[value]`), storing the result at
 * `mixerStateArray + busIndex*0x90 + 0x6c`; then calls
 * `CSTGBusInfo::GetSignalSelectionForBusType(STGAPIHDRBusTypes[value])`
 * and, based on the confirmed real return-value cases (0/1/2/other),
 * deterministically sets three further fields at `+0x50`/`+0x54`/`+0x58`
 * of the same array entry (see oa_engine.h-style header comments for
 * the per-branch derivation -- this is a real 4-way branch, not a
 * simplification).
 */
void CSTGAudioInputMixerBase::SetHDRBus(unsigned int busIndex, int value)
{
	unsigned char *entry = FromU32(mixerStateArray32) + busIndex * 0x90;

	typedef int (*Fn)(void *, int);
	Fn fn = ((Fn *)(*(void ***)this))[3];
	int routed = fn(this, STGAPIHDRPhysBusIds[value]);
	*(int *)(entry + 0x6c) = routed;

	int signalSelection = CSTGBusInfo::GetSignalSelectionForBusType(STGAPIHDRBusTypes[value]);

	if (signalSelection == 1) {
		*(int *)(entry + 0x50) = -1;
		*(int *)(entry + 0x54) = 0;
		*(int *)(entry + 0x58) = 0;
	} else if (signalSelection == 2) {
		*(int *)(entry + 0x50) = 0;
		*(int *)(entry + 0x54) = -1;
		*(int *)(entry + 0x58) = 0;
	} else if (signalSelection == 0) {
		*(int *)(entry + 0x50) = 0;
		*(int *)(entry + 0x54) = 0;
		*(int *)(entry + 0x58) = -1;
	} else {
		*(int *)(entry + 0x50) = 0;
		*(int *)(entry + 0x54) = 0;
		*(int *)(entry + 0x58) = 0;
	}
}

/*
 * SetSendBuses() (batch 58, `.text+0x68c50`, 96 bytes) confirmed:
 * dispatches slot 3 with fixed args `0x32`/`0x34` ONCE each (results
 * cached in registers, not recomputed per entry), then broadcasts both
 * results into every one of the `count` (`_gap4[0]`) mixerStateArray
 * entries' own `+0x70`/`+0x74` fields. A real do-while: the loop body
 * always runs at least once if `count != 0` (checked before entry), so
 * modeled here as a plain `for` -- behaviorally identical.
 */
void CSTGAudioInputMixerBase::SetSendBuses()
{
	typedef int (*Fn)(void *, int);
	Fn fn = ((Fn *)(*(void ***)this))[3];
	int val70 = fn(this, 0x32);
	int val74 = fn(this, 0x34);

	unsigned char count = _gap4[0];
	unsigned char *mixerArr = FromU32(mixerStateArray32);
	for (unsigned int i = 0; i < count; i++) {
		unsigned char *entry = mixerArr + i * 0x90;
		*(int *)(entry + 0x70) = val70;
		*(int *)(entry + 0x74) = val74;
	}
}

/*
 * CSTGAudioInputMixer::Initialize(unsigned int) (batch 58, `.text+0x68800`,
 * 117 bytes) confirmed -- see this method's own declaration comment in
 * oa_global.h for the full derivation (fixed 6-entry base Initialize()
 * call, six confirmed `sGlobalBusSet` index overwrites, SetSendBuses()
 * tail call).
 */
void CSTGAudioInputMixer::Initialize(unsigned int count)
{
	channelCountByte = (unsigned char)count;

	unsigned char *mixerArr = CSTGAudioInputMixerBase::Initialize(6);

	static const unsigned int kBusIndex[6] = { 2, 3, 4, 5, 10, 11 };
	for (int i = 0; i < 6; i++) {
		unsigned char *entry = mixerArr + i * 0x90;
		*(unsigned int *)(entry + 0x60) =
			ToU32(CSTGAudioBusManager::sGlobalBusSet + kBusIndex[i] * 0x80);
	}

	SetSendBuses();
}

/*
 * CSTGMasterLRMixer::Initialize(unsigned int) (batch 58, `.text+0xc09a0`,
 * 25 bytes) confirmed, branch-free -- see this method's own declaration
 * comment in oa_global.h for the full derivation.
 */
void CSTGMasterLRMixer::Initialize(unsigned int index)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int off = index * 120 * 0x80;

	*(unsigned int *)(base + 0x10) =
		ToU32(CSTGAudioBusManager::sEffectThreadBusSets + off + 118 * 0x80);
	*(unsigned int *)(base + 0x14) =
		ToU32(CSTGAudioBusManager::sEffectThreadBusSets + off + 12 * 0x80);
}

/*
 * CSTGBusInfo::GetSignalSelectionForBusType(int) (`.text+0x258a0`, 24
 * bytes, sec 10.151) confirmed:
 *   lea edx,[eax-3]; xor eax,eax; cmp edx,1; jbe <table lookup>; ret
 *   mov eax,[edx*4+kSignalSelectionTable]; ret
 * i.e. a plain 2-entry `{1, 2}` `.rodata` table indexed by
 * `busType - 3`, defaulting to 0 for any busType outside {3, 4}.
 * Independently confirmed (via `readelf -r`) that this table's own
 * byte range carries NO relocations -- genuine raw integer data.
 */
int CSTGBusInfo::GetSignalSelectionForBusType(int busType)
{
	static const int kSignalSelectionTable[2] = { 1, 2 };
	unsigned int idx = (unsigned int)(busType - 3);
	if (idx > 1)
		return 0;
	return kSignalSelectionTable[idx];
}

/*
 * CBusChangeStateMachine::StartBusChange(int, int, unsigned int)
 * (`.text+0x462c0`, 67 bytes, sec 10.151) confirmed regparm(3): this=EAX,
 * busId=EDX (low byte only), busType=ECX (low byte only), third arg on
 * the stack. Real 0x10-byte-stride fields:
 *   +0x0  dword  "started" flag (0 until first (re-)latch)
 *   +0x4  dword  "changeToken" -- set ONCE, to (third arg + 1), the
 *                first time this bus is latched while +0x0 is still 0
 *   +0xa  byte   last-seen busId
 *   +0xb  byte   last-seen busType
 *   +0xc  dword  snapshot of `CSTGPerformanceVarsManager::sInstance[8]`
 *                (the confirmed real "active perf-vars slot selector"
 *                toggle byte, sec 10.71) at the time this bus was last
 *                latched -- a cheap "did the active performance change?"
 *                epoch check
 * Confirmed real 3-way early-out: if busId, busType, AND the perf-vars
 * slot selector are all UNCHANGED since the last call, this function
 * does nothing at all. When ANY of the three differs, (re-)latches
 * +0xa/+0xb/+0xc using a freshly re-read selector byte; only the very
 * first time (+0x0 still 0) does it ALSO set +0x0=1 and +0x4=(third
 * arg + 1) -- +0x4 is never touched again on subsequent re-latches.
 */
void CBusChangeStateMachine::StartBusChange(int busId, int busType, unsigned int arg3)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char curSlot;

	if ((unsigned char)busId == base[0xa] && (unsigned char)busType == base[0xb]) {
		curSlot = CSTGPerformanceVarsManager::sInstance[8];
		if (*(unsigned int *)(base + 0xc) == curSlot)
			return;
	} else {
		curSlot = CSTGPerformanceVarsManager::sInstance[8];
	}

	base[0xa] = (unsigned char)busId;
	base[0xb] = (unsigned char)busType;
	*(unsigned int *)(base + 0xc) = curSlot;

	if (*(unsigned int *)(base + 0x0) == 0) {
		*(unsigned int *)(base + 0x0) = 1;
		*(unsigned int *)(base + 0x4) = arg3 + 1;
	}
}

/*
 * CSTGPan::CalculateMonoPanCoeffs(STGMonoPanCoeffs&, float, float)
 * (`.text+0x24e30`, 104 bytes, sec 10.151) confirmed via a full x87 FPU
 * stack simulation of all three branches: a real (near-)equal-power
 * quadratic pan law using two `.rodata.cst4` constants (independently
 * confirmed via `readelf -r`'s own `_ZN7CSTGPan...` relocations against
 * that section, NOT the main `.rodata` -- an easy trap, since the
 * literal displacement bytes coincidentally look like small `.rodata`
 * offsets at first glance):
 *   kHardGain = 0x3f7fffff (0.99999994f -- one ULP below 1.0, not
 *               exactly 1.0f)
 *   kQuadA    = 0xbf5413cd (-0.828427136f, -2*(sqrt(2)-1))
 *   kQuadB    = 0x3fea09e6 (1.828427076f, 2*sqrt(2)-1)
 * Three branches on `pan`:
 *   pan < 0     : coeff0 = scale*kHardGain, coeff4 = 0            (hard left)
 *   0 <= pan<=1 : quadratic curve (verified: at pan=0.5, both
 *                 coefficients reduce to scale*sqrt(2)/2, the expected
 *                 center-pan equal-power value -- a real, independently
 *                 checked continuity proof, not just a literal
 *                 transcription)
 *   pan > 1     : coeff0 = 0, coeff4 = scale*kHardGain             (hard right)
 * Both hard-limit branches agree with the curve's own pan=0/pan=1
 * endpoints, confirming this is a real continuous pan law, not three
 * unrelated cases.
 */
/*
 * FMul()/FAdd()/FLess()/FLessEq() -- small x87 inline-asm primitives,
 * same rationale and style as global.cpp's own established
 * MulRoundToFloat()/FYL2X() (sec 10.117): this kernel build is
 * `-msoft-float -mno-sse` (no hardware/soft-float libgcc helpers
 * available), so plain C `*`/`+`/`<`/`<=` on `float` would otherwise
 * silently pull in `__mulsf3`/`__addsf3`/`__ltsf2`/`__lesf2` -- symbols
 * this freestanding kernel module can't resolve. Using the real x87
 * instructions directly (matching the ground-truth disassembly's own
 * FPU usage) sidesteps that entirely instead of trying to widen the
 * kernel-wide build flags.
 */
/* All four primitives use ONLY memory ("m") operands for both inputs
 * and the output, with the entire x87 push/pop sequence self-contained
 * inside one asm statement -- deliberately avoiding the "t"/"u"
 * register-tied constraint style (which proved fragile when chained
 * across nested calls in this specific function's own coeff0 expression
 * during this pass's own verification -- caught by a real KAT failure,
 * not by re-reading the asm a second time). */
static inline float FMul(float a, float b)
{
	float result;
	__asm__ __volatile__(
		"flds %1\n\t"
		"flds %2\n\t"
		"fmulp %%st,%%st(1)\n\t"
		"fstps %0"
		: "=m" (result)
		: "m" (a), "m" (b)
	);
	return result;
}

static inline float FAdd(float a, float b)
{
	float result;
	__asm__ __volatile__(
		"flds %1\n\t"
		"flds %2\n\t"
		"faddp %%st,%%st(1)\n\t"
		"fstps %0"
		: "=m" (result)
		: "m" (a), "m" (b)
	);
	return result;
}

static inline int FLess(float a, float b)
{
	unsigned char r;
	__asm__ __volatile__(
		"flds %2\n\t"
		"flds %1\n\t"
		"fucomip %%st(1), %%st\n\t"
		"fstp %%st(0)\n\t"
		"setb %0"
		: "=q" (r)
		: "m" (a), "m" (b)
		: "cc"
	);
	return r;
}

static inline int FLessEq(float a, float b)
{
	unsigned char r;
	__asm__ __volatile__(
		"flds %2\n\t"
		"flds %1\n\t"
		"fucomip %%st(1), %%st\n\t"
		"fstp %%st(0)\n\t"
		"setbe %0"
		: "=q" (r)
		: "m" (a), "m" (b)
		: "cc"
	);
	return r;
}

void CSTGPan::CalculateMonoPanCoeffs(STGMonoPanCoeffs &out, float scale, float pan)
{
	static const float kHardGain = 0.999999940f;	/* 0x3f7fffff */
	static const float kQuadA = -0.828427136f;	/* 0xbf5413cd */
	static const float kQuadB = 1.828427076f;	/* 0x3fea09e6 */

	if (FLess(pan, 0.0f)) {
		out.coeff0 = FMul(scale, kHardGain);
		out.coeff4 = 0.0f;
	} else if (FLessEq(pan, 1.0f)) {
		float oneMinusPan = FAdd(1.0f, FMul(pan, -1.0f));
		out.coeff4 = FMul(FMul(pan, FAdd(FMul(pan, kQuadA), kQuadB)), scale);
		out.coeff0 = FMul(FMul(oneMinusPan, FAdd(FMul(kQuadA, oneMinusPan), kQuadB)), scale);
	} else {
		out.coeff0 = 0.0f;
		out.coeff4 = FMul(kHardGain, scale);
	}
}

/*
 * CBusChangeStateMachine::Reset(eSTGBusID, eSTGBusType) (batch 22,
 * `.text+0x46290`, 35 bytes) confirmed real, branch/call-free -- see
 * this method's own declaration comment in oa_global.h.
 */
void CBusChangeStateMachine::Reset(int busId, int busType)
{
	unsigned char *base = (unsigned char *)this;

	base[0x8] = 0x20;
	base[0x9] = 0;
	base[0xa] = (unsigned char)busId;
	base[0xb] = (unsigned char)busType;
	*(unsigned int *)(base + 0x0) = 1;
	*(unsigned int *)(base + 0x4) = 1;
	*(unsigned int *)(base + 0xc) = 2;
}

/*
 * CSTGAudioInputMixerBase::Initialize(unsigned int) (batch 22,
 * `.text+0x68a80`, 342 bytes) confirmed -- see this method's own
 * declaration comment in oa_global.h for the full derivation.
 */
/*
 * WORKAROUND (2026-07-24): originally `void`, matching ground truth.
 * Now returns the freshly-allocated `mixerArr` (still also stored into
 * `mixerStateArray32` below, for structural fidelity) -- a live
 * kronos_vm boot proved this object's own `mixerStateArray32` field
 * doesn't reliably survive its ONE caller's immediate reread right
 * after this call returns (hdr_manager_init.cpp's CSTGCDAudioPlay::
 * Initialize(), BUG: kernel NULL pointer dereference, CR2=0x60 -- same
 * "write not visible to a later read" symptom hit repeatedly elsewhere
 * in this reconstruction, see heap_manager.cpp's own file comment for
 * the running history). This function has exactly one caller in this
 * whole project (confirmed via grep), so widening its signature to hand
 * back the value directly, rather than requiring a reread, carries no
 * wider risk. */
unsigned char *CSTGAudioInputMixerBase::Initialize(unsigned int count)
{
	_gap4[0] = (unsigned char)count;

	unsigned int mixerBytes = count * 0x90;
	unsigned char *mixerArr = CSTGBankMemory::AllocAligned(mixerBytes, 0x10);
	mixerStateArray32 = ToU32(mixerArr);
	for (unsigned int i = 0; i < mixerBytes; i++)
		mixerArr[i] = 0;

	for (unsigned int i = 0; i < count; i++) {
		unsigned char *entry = mixerArr + i * 0x90;

		STGMonoPanCoeffs coeffs;
		CSTGPan::CalculateMonoPanCoeffs(coeffs, 1.0f, 0.5f);
		*(float *)(entry + 0x0) = coeffs.coeff0;
		*(float *)(entry + 0x4) = coeffs.coeff4;

		*(unsigned int *)(entry + 0x60) =
			ToU32(CSTGAudioBusManager::sGlobalBusSet + 0 * 0x80);
		*(unsigned int *)(entry + 0x64) =
			ToU32(CSTGAudioBusManager::sGlobalBusSet + 32 * 0x80);
		*(unsigned int *)(entry + 0x68) =
			ToU32(CSTGAudioBusManager::sGlobalBusSet + 32 * 0x80);
		*(unsigned int *)(entry + 0x6c) =
			ToU32(CSTGAudioBusManager::sGlobalBusSet + 32 * 0x80);
		*(unsigned int *)(entry + 0x70) =
			ToU32(CSTGAudioBusManager::sGlobalBusSet + 32 * 0x80);
		*(unsigned int *)(entry + 0x74) =
			ToU32(CSTGAudioBusManager::sGlobalBusSet + 32 * 0x80);
	}

	unsigned char *busArr = (unsigned char *)operator new[]((oa_size_t)(count * 0x10));
	busChangeArray32 = ToU32(busArr);
	for (unsigned int i = 0; i < count; i++) {
		unsigned char *entry = busArr + i * 0x10;
		*(unsigned int *)(entry + 0x0) = 0;
		entry[0x8] = 0x20;
		entry[0x9] = 0;
		entry[0xa] = 0x20;
		entry[0xb] = 0;
		*(unsigned int *)(entry + 0xc) = 0;
	}

	if (count == 0)
		return mixerArr;

	for (unsigned int i = 0; i < count; i++) {
		unsigned char *entry = mixerArr + i * 0x90;
		/* movaps-based 16-byte block duplication (pure data movement,
		 * not real SIMD arithmetic -- modeled as a plain byte copy). */
		for (int b = 0; b < 16; b++)
			entry[0x10 + b] = entry[0x0 + b];
		*(unsigned int *)(entry + 0x18) = 0;
		for (int b = 0; b < 16; b++)
			entry[0x30 + b] = entry[0x20 + b];

		/* Confirmed regparm mapping: busId is zero-extended
		 * (`movzbl 0xa(%eax),%edx`), busType is sign-extended
		 * (`movsbl 0xb(%eax),%ecx`) -- not symmetric, faithfully
		 * preserved. */
		CBusChangeStateMachine *bcsm = (CBusChangeStateMachine *)(busArr + i * 0x10);
		unsigned char *bcsmBytes = (unsigned char *)bcsm;
		bcsm->Reset((int)(unsigned char)bcsmBytes[0xa], (int)(signed char)bcsmBytes[0xb]);
	}
	return mixerArr;
}
