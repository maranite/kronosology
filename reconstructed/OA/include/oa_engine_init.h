// SPDX-License-Identifier: GPL-2.0
/*
 * oa_engine_init.h  -  new classes CSTGEngine::Initialize() (oa_engine.h,
 * .text+0x1b0, 1901 bytes) needs, beyond what oa_engine.h/
 * oa_setup_global_resources.h already declare.
 *
 * Ground-truthed via a full objdump -d -r disassembly of the whole
 * function, restricted to the .text section (the naive --start-address/
 * --stop-address form pulls in unrelated .init.text content at the same
 * relative addresses -- caught and fixed this pass with `-j .text`).
 *
 * CORRECTS a sizing error in this project's own earlier survey
 * (MASTER_REFERENCE.md sec 10.13, rows 32-41): that table's "×2 per
 * class" annotations for the ten "Model" classes below were a
 * misinterpretation of "the same 264-byte size recurring across eight
 * DIFFERENT adjacent classes" as "one class allocated twice" -- and its
 * row-32 "528 bytes" value actually belongs to the CSTGMidiPortManager
 * struct-init block that follows CSTGMIDIClockSync, not to CSTGOffModel
 * at all. This pass's direct instruction-level trace supersedes that
 * table for this specific range; every other row already checked out.
 *
 * All ten "Model" classes (CSTGOffModel/CSTGPCMModel/CSTGAnalogSyncModel/
 * CSTGOrganModel/CSTGPluckedModel/CSTGMS20Model/CSTGPolysixModel/
 * CSTGVPMModel/CSTGPianoModel/CSTGEPModel) are constructed then have a
 * real virtual call dispatched through their own vtable slot 2 (`[[obj][0]]
 * [+8]`) immediately after -- the SAME raw-indirect-dispatch pattern
 * already established for CCostProfile (oa_setup_global_resources.h) --
 * modeled the same way here (a shared helper, not a real C++ vtable,
 * since their full vtable layouts aren't independently confirmed).
 *
 * `TSTGArrayManager<T>` (real template, confirmed via 3 distinct mangled
 * instantiations: CSTGPlaybackEvent/CSTGRecordEvent/CSTGRecordBuffer) is a
 * fixed-size, N+1-bucket hash-style array: `bucketArray`(N+1 entries) is
 * filled at a modular `writeCursor` (0..N-1, never actually wrapping since
 * N<N+1) while `indexArray`(N entries) is filled by the loop's own
 * sequential index -- in this specific confirmed usage the two end up
 * holding the same N pointers, just via two different fill orders.
 *
 * `CSTGRecordEvent : public CSTGAudioEvent` is a genuine, directly
 * confirmed inheritance relationship: its construction calls
 * `CSTGAudioEvent::CSTGAudioEvent()` (the C2 "base object" ABI variant)
 * on the allocated storage, then manually overwrites the vtable pointer
 * with `&_ZTV15CSTGRecordEvent` -- the standard Itanium ABI pattern for a
 * derived class's own constructor completing after its base's.
 */

#ifndef OA_ENGINE_INIT_H
#define OA_ENGINE_INIT_H

#include "oa_engine.h"

struct CSTGControllerValue;	/* forward decl, real definition in oa_global.h */

/* Raw indirect dispatch through a confirmed vtable slot 2 (matching
 * CCostProfile's own established treatment) -- shared by all ten "Model"
 * classes below. */
static inline void CallVtableSlot2(void *obj)
{
	typedef void (*Fn)(void *);
	void **vtable = *(void ***)obj;
	Fn fn = (Fn)vtable[2];
	fn(obj);
}

/*
 * CSTGWaveSeqGenerator -- confirmed real (sec 10.62), embedded 200x as
 * a plain array inside CSTGWaveSeqManager (+0x0..+0xe0ff, 0x120/288
 * bytes each). Its own constructor (`.text+0x819a0`, 193 bytes) and
 * `Init()` method (called once per generator from CSTGWaveSeqManager::
 * Initialize()) are confirmed real, deliberately deferred externs --
 * out of scope for this pass, matching the "declare the shape, defer
 * the body" treatment used throughout this project (e.g.
 * CSTGPlaybackEvent). Its own +0x0/+0x4/+0xc fields ARE touched
 * directly by CSTGWaveSeqManager::Initialize() (a real intrusive
 * doubly-linked list node: next/prev/owner, the same 3-field
 * convention already confirmed for CSTGHeapHandleEntry, sec 10.59) --
 * manipulated via raw offset arithmetic rather than named members,
 * since the rest of the class's layout isn't independently recovered.
 */
struct CSTGWaveSeqGenerator {
	CSTGWaveSeqGenerator();
	void Init();
	/* Confirmed real (`_ZN20CSTGWaveSeqGenerator6sMutexE`): NOT the
	 * mutex pointer's VALUE, but the ADDRESS of the pointer FIELD
	 * itself (CSTGWaveSeqManager's own second allocated mutex, at
	 * `this_manager + 0xe130`) -- the same "address of the singleton
	 * pointer" idiom already confirmed twice elsewhere in this project
	 * (CSTGSampleRateMonitor::Initialize(), sec 10.57;
	 * CSTGPerformanceVarsManager::Initialize(), sec 10.55). */
	static void **sMutex;
	unsigned char _unrecovered[0x120];
};

struct CSTGWaveSeqManager {
	CSTGWaveSeqManager();
	static CSTGWaveSeqManager *sInstance;
	void Initialize();
	/*
	 * Confirmed real layout (sec 10.62):
	 *   +0x000..+0xe0ff  200 x CSTGWaveSeqGenerator (0x120 bytes each)
	 *   +0xe100/+0xe104  intrusive doubly-linked "available generator"
	 *                    list head/tail (built by Initialize(), a
	 *                    real push-front insertion identical in shape
	 *                    to CSTGHeapManager's own free-list build,
	 *                    sec 10.59)
	 *   +0xe108          available-generator count
	 *   +0xe10c/+0xe110/+0xe114/+0xe118/+0xe11c/+0xe120  confirmed
	 *                    zeroed by the constructor, not independently
	 *                    named beyond that
	 *   +0xe12c/+0xe130  two real `rtwrap_malloc`'d mutex pointers
	 *                    (each `rtwrap_pthread_mutex_init`'d with the
	 *                    default attr, confirmed via the ctor's own
	 *                    disassembly)
	 *   +0xe124/+0xe125  confirmed zeroed bytes
	 *   +0xe126/+0xe128  confirmed zeroed words
	 * Total confirmed size 0xe134, exactly matching this project's OWN
	 * already-confirmed `CSTGBankMemory::AllocAligned(0xe134, 0x10)`
	 * allocation for this class (engine_init.cpp, sec 10.58) -- an
	 * independent cross-check from a completely different pass.
	 */
	unsigned char _unrecovered[0xe134];
};

/*
 * CSTGVectorEGBase -- confirmed real (sec 10.66), a genuinely new base
 * class discovered from all three derived constructors' own
 * disassembly: each of CSTGVectorEGXOnly/EGXY/EGCC's own constructor
 * calls `CSTGVectorEGBase::CSTGVectorEGBase()` FIRST, before setting
 * its own derived vtable pointer -- the standard confirmed pattern
 * for real inheritance in this codebase (matching CCostProfile :
 * public CStartupFile, sec 10.60). Declared here as an opaque base
 * with a confirmed-real, deliberately deferred constructor -- its own
 * body sets at least one confirmed-real flags byte at derived-object-
 * relative +0x6e (CSTGVectorEGXY's own constructor does `and
 * BYTE PTR [this+0x6e],0xfd`, clearing one bit of whatever the base
 * ctor already wrote there -- proof the base ctor touches that byte,
 * not independently reconstructed which bits/values in this pass).
 */
struct CSTGVectorEGBase {
	CSTGVectorEGBase();
};

/*
 * STGVJSAssignInfo -- confirmed real (sec 10.66, via a direct
 * relocation from CSTGVectorEGCC's own constructor), a global data
 * table referenced (not modified) by CSTGVectorEGCC's own default
 * joystick-assignment field initialization. Declared as an opaque
 * extern; its own layout/contents are not reconstructed in this pass.
 */
extern "C" unsigned char STGVJSAssignInfo[];

/*
 * CSTGVectorEGXOnly/CSTGVectorEGXY/CSTGVectorEGCC : public
 * CSTGVectorEGBase -- confirmed real (sec 10.64/10.66), three "vector
 * envelope generator" classes embedded inside CSTGVectorManager. Each
 * also has a real vtable (confirmed via `Initialize()`'s own indirect
 * `call DWORD PTR [edx]` slot-0 dispatch, not reconstructed in this
 * pass -- their own real vtable DATA symbols are referenced via the
 * `extern "C"` byte-array trick, sec 10.58's own precedent). Sizes
 * are the confirmed real per-instance stride from CSTGVectorManager's
 * own constructor (0x88/0x7c/0x70 bytes).
 *
 * All three share a confirmed real field layout at the SAME object-
 * relative offsets (ground-truthed independently by each of their own
 * constructors' explicit zeroing, and by CSTGVectorManager::
 * Initialize()'s own confirmed reads/writes, sec 10.65):
 *   +0x3c/+0x40  intrusive doubly-linked-list node (next/prev)
 *   +0x44        confirmed real: self-pointer (`this`), set by every
 *                one of these three constructors, not independently
 *                understood beyond its confirmed value
 *   +0x48        list "owner" backpointer (zeroed by the ctor, set by
 *                CSTGVectorManager::Initialize() on list insertion --
 *                CSTGVectorEGCC never gets list-inserted, so this
 *                field stays 0 on that type)
 * Not modeled as named members since the rest of each class's own
 * layout isn't independently recovered; manipulated via raw offset
 * arithmetic in the .cpp instead.
 */
struct CSTGVectorEGXOnly : public CSTGVectorEGBase {
	CSTGVectorEGXOnly();
	/* Confirmed real (`_ZN17CSTGVectorEGXOnly6sMutexE`) -- same
	 * "address of the singleton pointer" idiom already confirmed
	 * elsewhere (e.g. CSTGWaveSeqGenerator::sMutex, sec 10.62). Set by
	 * CSTGVectorManager::Initialize() (sec 10.65) to
	 * `&manager->+0x1c9e0`, the second of the three mutexes the
	 * constructor allocates. */
	static void **sMutex;
	unsigned char _unrecovered[0x88];
};
struct CSTGVectorEGXY : public CSTGVectorEGBase {
	CSTGVectorEGXY();
	/* Confirmed real (`_ZN14CSTGVectorEGXY6sMutexE`) -- set by
	 * CSTGVectorManager::Initialize() (sec 10.65) to
	 * `&manager->+0x1c9e4`, the third of the three constructor-
	 * allocated mutexes. */
	static void **sMutex;
	unsigned char _unrecovered[0x7c];
};
struct CSTGVectorEGCC : public CSTGVectorEGBase {
	CSTGVectorEGCC();
	unsigned char _unrecovered[0x70];
};

struct CSTGVectorManager {
	CSTGVectorManager();
	static CSTGVectorManager *sInstance;
	void Initialize();
	/*
	 * Confirmed real layout (sec 10.64), ground-truthed via readelf+
	 * objdump (`-j .text`) against a full instruction-by-instruction
	 * trace of the 3279-byte constructor, cross-checked exactly
	 * against engine_init.cpp's own already-confirmed
	 * `CSTGBankMemory::AllocAligned(0x1c9e8, 0x10)` allocation:
	 *   +0x00000..+0x0d480  400x CSTGVectorEGXOnly (a real loop)
	 *   +0x0d480..+0x19640  400x CSTGVectorEGXY (a real loop)
	 *   +0x19640..+0x19db0   17x CSTGVectorEGCC (compiler-unrolled)
	 *   +0x19db0..+0x1a630   16x CSTGVectorEGXOnly (unrolled)
	 *   +0x1a630..+0x1adf0   16x CSTGVectorEGXY (unrolled)
	 *   +0x1adf0..+0x1af70  confirmed zeroed (96 dwords)
	 *   +0x1af70..+0x1aff4  confirmed real GAP, never written
	 *   +0x1aff4..+0x1b764   17x CSTGVectorEGCC (unrolled)
	 *   +0x1b764..+0x1bfe4   16x CSTGVectorEGXOnly (unrolled)
	 *   +0x1bfe4..+0x1c7a4   16x CSTGVectorEGXY (unrolled)
	 *   +0x1c7a4..+0x1c924  confirmed zeroed (96 dwords)
	 *   +0x1c924..+0x1c9ac  confirmed real GAP, never written
	 *   +0x1c9ac..+0x1c9dc  confirmed zeroed (12 dwords)
	 *   +0x1c9dc/+0x1c9e0/+0x1c9e4  three real `rtwrap_malloc`'d
	 *                       mutex pointers, each `rtwrap_pthread_
	 *                       mutex_init`'d
	 * Total real instance counts: 432x CSTGVectorEGXOnly (400 via
	 * loop + 32 unrolled), 432x CSTGVectorEGXY (400 via loop + 32
	 * unrolled), 34x CSTGVectorEGCC (all unrolled, no loop version
	 * exists for this type). The two confirmed real gaps are NOT
	 * accounted for by any zero-write or construction call anywhere
	 * in the constructor -- preserved verbatim, not papered over.
	 */
	unsigned char _unrecovered[0x1c9e8];

	/*
	 * OnUpdateGlobalMidiChannel(unsigned char) (sec 10.124, .text+0x78e70,
	 * 13 bytes) confirmed: trivially stores `channel` into two fields,
	 * `+0x19da4` and `+0x1b758` (both within the still-opaque
	 * `_unrecovered` blob above), no branches, no other side effects.
	 */
	void OnUpdateGlobalMidiChannel(unsigned char channel);
};

/*
 * CSTGMidiQueue -- confirmed real (sec 10.63/10.82), a genuinely new
 * class touched via one method, `AllocReader()`. RESOLVES sec 10.63's
 * own "static-shaped ambiguity" note: direct disassembly
 * (`.text+0x40090` in OA_real.ko, `_ZN13CSTGMidiQueue11AllocReaderEv`
 * -- confirmed mangled empty-parameter-list member function) shows a
 * genuine, real regparm(3) instance method: `this` in eax, an atomic
 * `lock xadd $1, [this+0x20]` returning the PRE-increment byte value --
 * a real lock-free reader-slot allocator (each call claims the next
 * slot number, up to whatever wraps a byte at +0x20).
 */
struct CSTGMidiQueue {
	unsigned char AllocReader();

	/*
	 * GetNumWritableBytes() const (confirmed real via a relocation from
	 * CSTGGlobal::SubmitPerfChangeRequest, sec 10.116) is called
	 * directly (non-virtual) on the SAME `CSTGMidiPortManager+0x208`
	 * address `CSTGMidiQueueWriter::Write` is also called on --
	 * modeled here as a separate opaque type reinterpreting the same
	 * address, matching this project's established non-inheritance
	 * convention, rather than asserting a real base-class relationship
	 * between the two that hasn't been independently confirmed.
	 */
	unsigned int GetNumWritableBytes() const;
};

/*
 * CSTGChannelValues (confirmed real via 3 relocations from
 * CSTGMidiDispatcher::PerfChangeControllerReset, sec 10.115) --
 * per-MIDI-channel controller/pitch-bend state, confirmed to live at a
 * fixed `+0x2410` offset within each `0x92c`-strided per-channel block
 * of the active `CSTGPerformanceVarsManager` slot (16 channels, indices
 * `0..0xf`). Own internal layout not reconstructed -- only these 3
 * confirmed real, deliberately deferred methods are declared.
 */
struct CSTGChannelValues {
	void Reset();

	/*
	 * SetPitchBend(CSTGControllerValue const&, bool) (sec 10.128,
	 * .text+0x26ce0, 41 bytes) confirmed: unconditionally copies
	 * `value.field0`/`value.field4`/the packed `field8+fieldA+fieldB`
	 * dword (offsets +0/+4/+8 of `CSTGControllerValue`) into
	 * `this->fieldAt(0x5a0)`/`+0x5a4`/`+0x5a8`; if `flag` is set, ALSO
	 * copies `value.field0` a second time into `this->fieldAt(0x634)`.
	 */
	void SetPitchBend(const CSTGControllerValue &value, bool flag);
	void SetControllerValue(unsigned char ccNumber, const CSTGControllerValue &value);
};

struct CSTGMidiDispatcher {
	CSTGMidiDispatcher();
	static CSTGMidiDispatcher *sInstance;
	void Initialize();
	/* Confirmed real size 0xa8 (matches engine_init.cpp's own already-
	 * confirmed `CSTGBankMemory::AllocAligned(0xa8, 0x10)`). Fields
	 * manipulated via raw offset arithmetic in the .cpp -- not named
	 * here, since only a handful of the many touched offsets have any
	 * independently recoverable meaning (the rest are confirmed-zeroed
	 * but otherwise opaque bytes). */
	unsigned char _unrecovered[0xa8];

	/* HandleController(...) (sec 10.77, confirmed via relocation from
	 * CSTGGlobal::UpdateVJSXAssignment/UpdateVJSYAssignment) confirmed
	 * real, deliberately deferred extern. Real mangled signature is
	 * `(unsigned char, unsigned char, unsigned char, eSTGMidiSource,
	 * eSTGMidiTargetPerformance)` -- the two enum parameters are
	 * represented as plain `int` here (regparm passes them in a
	 * register/stack slot regardless of the real enum's declared
	 * width, matching this project's established convention). Real
	 * per-parameter semantic names not independently confirmed beyond
	 * their confirmed call-site values (channel byte, selector byte,
	 * a literal `0x40`, source=`1`, target=`-1`) -- own body not
	 * reconstructed in this pass. */
	void HandleController(unsigned char arg1, unsigned char arg2, unsigned char arg3,
			       int source, int target);

	/*
	 * HandleController(const unsigned char*, int, int) (sec 10.139,
	 * confirmed real, a genuinely separate real overload -- weak/COMDAT
	 * symbol, 75 bytes, its own dedicated `.text` section) confirmed: a
	 * thin unpacking wrapper over the OTHER `HandleController` overload
	 * above -- unpacks a real 3-byte MIDI message (`bytes[0] & 0xf` as
	 * the channel, `bytes[1]` as arg2, `bytes[2]` as arg3) and forwards
	 * to it unchanged, along with the same `source`/`target` arguments
	 * passed through. The two enum parameters are represented as plain
	 * `int`, matching the other overload's own established convention.
	 */
	void HandleController(const unsigned char *bytes, int source, int target);

	/* ResetAllControllers(unsigned char, bool) (sec 10.78, confirmed
	 * via relocation from several UpdateXXX handlers -- already
	 * referenced by name in this project's own comments since sec
	 * 10.33, now finally declared) confirmed real, deliberately
	 * deferred extern -- own body not reconstructed in this pass. */
	void ResetAllControllers(unsigned char channel, bool flag);

	/*
	 * PerfChangeControllerReset() (sec 10.115, .text+0xd9d30, 536
	 * bytes) confirmed: resolves `mgr = ResolveActivePerformanceVarsManagerRaw()`,
	 * calls `CSTGSmoother::sInstance->CancelAllCCSmoothers()` (newly
	 * discovered, confirmed real, deliberately deferred), then loops
	 * over 16 MIDI channels (`ch = 0..0xf`), each channel's own
	 * `0x92c`-strided block within `mgr`:
	 *   - Resets `(CSTGChannelValues*)(chanBase+0x2410)` (a newly
	 *     discovered class, own body not reconstructed).
	 *   - Fills 120 bytes of `0xff` at
	 *     `STGAPIFrontPanelStatus::sInstance + (ch+2)*0x80 + 0xb`.
	 *   - Zeroes `this->byteAt(ch*4+0x60)`/`byteAt(ch*4+0x61)`.
	 *   - Calls `SetPitchBend()` on that same `CSTGChannelValues`
	 *     object, passing the EXISTING (not freshly computed) content
	 *     of a `CSTGControllerValue`-shaped slot at `chanBase+0x29b0`
	 *     (i.e. re-applies a persisted pitch-bend value after reset).
	 *   - Reads `chanBase+0x2718` ("mVal") and `chanBase+0x2730`
	 *     ("lVal") -- two per-channel controller bytes -- and, for
	 *     EACH, constructs a fresh `CSTGControllerValue` via a
	 *     confirmed real scaling formula (`val<=0x40`: `val/128.0f`;
	 *     `val>0x40`: `(val-1)/126.0f`; `val==0xfe` sentinel `0xff`:
	 *     both `0.0f`) for `field0` (reinterpreted as float) and
	 *     `field4 = 2*that - 1.0f`, `field8 = val`, `fieldA = 1`,
	 *     `fieldB = (stale | 1) & ~2` (a SECOND confirmed real
	 *     uninitialized-stack-read quirk, independent of
	 *     `ResetAllControllers`'s own `| 3` variant, sec 10.92) --
	 *     then calls `SetControllerValue(0x40, cv)` for `mVal` and
	 *     `SetControllerValue(0x42, cv)` for `lVal` (both newly
	 *     discovered, confirmed real, deliberately deferred), and
	 *     stores `mVal`/`lVal` into
	 *     `STGAPIFrontPanelStatus::sInstance + ch*0x80 + 0x14b`/`0x14d`
	 *     respectively.
	 */
	void PerfChangeControllerReset();

	/*
	 * StealingRequiresOneTickStall() (sec 10.136, .text+0xd98e0, 26
	 * bytes, confirmed via a relocation from CSTGPerformanceVarsManager::
	 * StealAllDyingPerformanceVars, sec 10.113/10.114) confirmed
	 * trivial: `this->fieldAt(0xa4) = CSTGGlobal::sInstance->
	 * fieldAt(0x29c9fa8) + 1` -- reads a global counter/tick field and
	 * stores it (incremented by one) into this object's own field, with
	 * no other side effects.
	 */
	void StealingRequiresOneTickStall();
};

/*
 * CSTGPerformance -- confirmed real (sec 10.77, via a direct, non-
 * virtual relocation to `_ZNK15CSTGPerformance17IsCurrentlyActiveEv`
 * from CSTGGlobal::UpdateVJSXAssignment/UpdateVJSYAssignment). Own
 * layout not reconstructed -- only the one confirmed real const
 * method needed to link those two callers.
 */
struct CSTGPerformance {
	bool IsCurrentlyActive() const;
};

struct CSTGFrontPanelSmoothers {
	CSTGFrontPanelSmoothers();
};

struct CSTGHDRMiniModel {
	CSTGHDRMiniModel();
	static CSTGHDRMiniModel *sInstance;
	void Initialize();
};

struct CSTGStreamingEventManager {
	CSTGStreamingEventManager();
	static CSTGStreamingEventManager *sInstance;
	/* Confirmed args (regparm this=eax/arg1=edx/arg2=ecx): edx=0x191
	 * (401, an unsigned short), ecx=0x10000 (65536, an unsigned long) --
	 * matching the mangled `10InitializeEtm` signature exactly. */
	void Initialize(unsigned short arg1, unsigned long arg2);
};

struct CSTGSmoother {
	CSTGSmoother();
	static CSTGSmoother *sInstance;
	void Initialize();

	/* CancelAllSmoothers() (sec 10.78, confirmed via relocation from
	 * several UpdateXXX handlers -- already referenced by name in this
	 * project's own comments since sec 10.33, now finally declared)
	 * confirmed real, deliberately deferred extern -- own body not
	 * reconstructed in this pass. */
	void CancelAllSmoothers();

	/* FinalizeAllSmoothers() (sec 10.95, confirmed via relocation from
	 * CSTGGlobal::PreprocessPerformanceChange) confirmed real,
	 * deliberately deferred extern -- own body not reconstructed. */
	void FinalizeAllSmoothers();

	/*
	 * CancelAllCCSmoothers() (sec 10.130, .text+0x2bc30, 82 bytes,
	 * confirmed real via a relocation from CSTGMidiDispatcher::
	 * PerfChangeControllerReset, sec 10.115) confirmed: walks a real
	 * intrusive singly-linked list anchored at `this->fieldAt(0xf010)`
	 * (each node: `+0x0`=next, `+0x8`=a pointer to a "mapping" object
	 * whose own `+0x10` field is checked against `2` or `8`); for each
	 * matching node, calls a newly-discovered confirmed real,
	 * deliberately deferred sibling, `FinalizeSmoother(TListLink<
	 * CSTGSmootherMapping>*, bool)` -- modeled here as `(void *node,
	 * bool)` per this project's established convention for
	 * not-fully-modeled template types -- with `false`, on this same
	 * node, before advancing.
	 */
	void CancelAllCCSmoothers();

	/*
	 * FinalizeSmoother(TListLink<CSTGSmootherMapping>*, bool) (sec
	 * 10.130, confirmed via relocation from CancelAllCCSmoothers above)
	 * confirmed real, deliberately deferred extern -- own body not
	 * reconstructed in this pass.
	 */
	void FinalizeSmoother(void *node, bool flag);
};

/*
 * CSTGPerformanceVars -- confirmed real (relocation from CSTGGlobal::
 * PreprocessPerformanceChange, sec 10.95), a DIFFERENT class from
 * `CSTGPerformanceVarsManager` (the object `ResolveActivePerformanceVarsManager`
 * resolves is reinterpreted as this class's own `this` for the
 * `SetIsDying()` call -- own real relationship between the two classes
 * not independently confirmed beyond that one call site).
 */
struct CSTGPerformanceVars {
	/* SetIsDying() confirmed real, deliberately deferred extern -- own
	 * body not reconstructed. */
	void SetIsDying();

	/*
	 * BeginActivation(CSTGPerformance*, bool) (confirmed real via a
	 * relocation from ProcessPerformanceChange, sec 10.109/10.110;
	 * .text+0xbabc0, 43 bytes) confirmed: zeroes `+0x23ec`, stores the
	 * passed `CSTGPerformance*` as a PACKED 32-bit pointer at `+0x23d8`
	 * (real hardware field is only 4 bytes, immediately followed by the
	 * `bool` at `+0x23dd` -- a native 8-byte store here would clobber
	 * that flag byte on a 64-bit host, caught via a real test failure)
	 * and the passed `bool` at `+0x23dd`, then -- if `+0x23d1` (the SAME
	 * "active state" byte `IsSetListSlotChangeOnly` reads, sec 10.96) is
	 * `0` -- calls `EnterActivatingState()` (confirmed real, deliberately
	 * deferred extern -- own body not reconstructed, a separate future
	 * task).
	 */
	void BeginActivation(CSTGPerformance *perf, bool flag);
	void EnterActivatingState();

	/*
	 * FreeVoicelessDyingSlots() (confirmed real via a relocation from
	 * ProcessPerformanceChange, sec 10.109/10.110; .text+0xbb490, 140
	 * bytes) confirmed: no-op if `+0x23d1` (the SAME "active state" byte
	 * as above) is `<= 2` (signed). Otherwise walks the confirmed real
	 * "active slot voice data" list at `CSTGGlobal::sInstance+0x29c9900`
	 * (the SAME list `GetFreeSlotVoiceData`, sec 10.100, inserts new
	 * nodes into) -- a DIFFERENT node-field convention than that list's
	 * own free-list counterpart: here node's own `+0x0` is "next" and
	 * `+0x8` is the referenced `CSTGSlotVoiceData*`. For each node whose
	 * voice data's own `+0x28c8` byte matches `this->fieldAt(0x23d0)`
	 * (a per-`CSTGPerformanceVars` group id) AND whose `+0x4c`/`+0x58`
	 * 16-bit fields sum to zero (i.e. "voiceless"), calls
	 * `CSTGSlotVoiceData::FreeSlotVoiceData(false)` on it. If anything
	 * was freed, calls `CLoadBalancer::sInstance->BalanceStaticLoad()`
	 * (newly discovered, confirmed real, deliberately deferred extern).
	 */
	void FreeVoicelessDyingSlots();
};

struct CSTGLFOTables {
	CSTGLFOTables();
	/* Confirmed real (`_ZN13CSTGLFOTables9sInstanceE`), needed by
	 * CSTGLFOBase::InitializeQuad() -- sec 10.61. */
	static CSTGLFOTables *sInstance;
};

struct CSTGMIDIClockSync {
	CSTGMIDIClockSync();
	/* Confirmed real (`_ZN17CSTGMIDIClockSync9sInstanceE`), needed by
	 * CSTGLFOBase::InitializeQuad() -- sec 10.61. */
	static CSTGMIDIClockSync *sInstance;
};

/*
 * CSTGKLMManager is a fully separate, already-complete Stage 1 class
 * (see auth.h/src/auth/klm_manager.cpp) -- NOT re-declared by including
 * auth.h here, since auth.h pulls in oa_types.h, which has this
 * project's own already-documented pre-existing ODR conflict with
 * oa_engine.h/oa_global.h (see oa_setup_global_resources.h's own header
 * note). This minimal local stand-in declares just enough (matching the
 * real mangled constructor/Initialize/sInstance names exactly) to let
 * CSTGEngine::Initialize() link against auth.h's real implementation --
 * `sInstance` is declared `extern` here, NOT defined (it's already
 * defined once in klm_manager.cpp; redefining it here would be a real
 * duplicate-symbol link error, not just a style choice). */
struct CSTGKLMManager {
	CSTGKLMManager();
	void Initialize();
	static CSTGKLMManager *sInstance;
};

struct CSTGOffModel { CSTGOffModel(); };
struct CSTGPCMModel { CSTGPCMModel(); };
struct CSTGAnalogSyncModel { CSTGAnalogSyncModel(); };
struct CSTGOrganModel { CSTGOrganModel(); };
struct CSTGPluckedModel { CSTGPluckedModel(); };
struct CSTGMS20Model { CSTGMS20Model(); };
struct CSTGPolysixModel { CSTGPolysixModel(); };
struct CSTGVPMModel { CSTGVPMModel(); };
struct CSTGPianoModel {
	CSTGPianoModel();
	/* RescanPianoTypes() (Bar 2, confirmed real via the real binary's
	 * own symbol table, own body not reconstructed) -- deliberately
	 * deferred extern. */
	void RescanPianoTypes();
};
struct CSTGEPModel { CSTGEPModel(); };

/* Opaque per-quad sub-rate parameter blocks -- real layouts not
 * reconstructed in this pass, only their confirmed sizes (matching the
 * exact stride Initialize()'s own loop advances by: 0x250/592 bytes for
 * LFOs, 0x100/256 bytes for step sequencers). */
struct STGLFOSubRateParams { unsigned char _unrecovered[0x250]; };
struct STGStepSeqSubRateParams { unsigned char _unrecovered[0x100]; };

struct CSTGCommonLFO {
	static void Initialize();
	/* Confirmed real (.bss, `_ZN13CSTGCommonLFO14sSubRateParamsE`):
	 * a single CSTGBankMemory::AllocAligned(0x4a00, 0x10) pool of 32
	 * (0x4a00/0x250) STGLFOSubRateParams blocks, each initialized via
	 * CSTGLFOBase::InitializeQuad(). */
	static STGLFOSubRateParams *sSubRateParams;
};
struct CSTGCommonStepSeq {
	static void Initialize();
	/* Same shape as CSTGCommonLFO::sSubRateParams: 32 (0x2000/0x100)
	 * STGStepSeqSubRateParams blocks. */
	static STGStepSeqSubRateParams *sSubRateParams;
};

/*
 * Confirmed real mangled member functions (via relocation from
 * CSTGCommonLFO/CSTGCommonStepSeq::Initialize() -- `call
 * _ZN11CSTGLFOBase14InitializeQuadEP19STGLFOSubRateParams` etc, NOT
 * plain C-linkage wrappers, matching the CSTGComPort lesson from sec
 * 10.53: a plain C symbol here would never link against the real
 * mangled one). Bodies not reconstructed in this pass. */
struct CSTGLFOBase { static void InitializeQuad(STGLFOSubRateParams *quad); };
struct CSTGStepSeqBase { static void InitializeQuad(STGStepSeqSubRateParams *quad); };

struct CSTGPlaybackEvent {
	CSTGPlaybackEvent();
	unsigned char _unrecovered[0x68];
};

struct CSTGAudioEvent {
	CSTGAudioEvent();
	unsigned char _unrecovered[0x38];
};

/*
 * CSTGRecordEvent has NO constructor symbol of its own anywhere in the
 * real binary (confirmed: no `_ZN15CSTGRecordEventC1Ev`/`C2Ev` relocation
 * exists) -- its "construction" is genuinely INLINED at the one call site
 * inside CSTGEngine::Initialize() itself: call CSTGAudioEvent::
 * CSTGAudioEvent() (the C2 base-object ctor) on the raw storage, then
 * manually store `&_ZTV15CSTGRecordEvent` as its vtable pointer. Modeled
 * here as a plain struct with NO constructor of its own (deliberately --
 * declaring a fictional `CSTGRecordEvent()` would produce a mangled name
 * that doesn't exist in the real binary); the inline sequence itself is
 * reproduced directly at the one call site in engine_init.cpp. */
struct CSTGRecordEvent : public CSTGAudioEvent {
};
/* The real vtable symbol itself, declared via its own already-mangled
 * name under `extern "C"` (which asks the linker for that literal
 * symbol string, unmangled further) -- the confirmed real relocation
 * target, +0x8 (the standard Itanium "skip offset-to-top/RTTI slots"
 * convention already established elsewhere in this project, e.g.
 * CSTGAudioDriverInterfaceKorgUsb's own constructor). */
extern "C" unsigned char _ZTV15CSTGRecordEvent[];

struct CSTGRecordBuffer {
	CSTGRecordBuffer();
	unsigned char _unrecovered[0x38];
};

/* TSTGArrayManager<T> -- see file header. All 3 confirmed instantiations
 * (CSTGPlaybackEvent/CSTGRecordEvent/CSTGRecordBuffer) share this exact
 * 24-byte (0x18) header layout; per-element storage is built directly by
 * CSTGEngine::Initialize() itself (inlined in the real disassembly, no
 * separate call), not by this class's own (nonexistent, per the same
 * "no C1/C2 symbol at all" pattern already confirmed for
 * CSTGMidiPortManager) constructor. */
/*
 * Pointer fields are explicit `unsigned int` (target-width, 4 bytes),
 * NOT native `unsigned char*` -- the confirmed real target struct is
 * exactly 24 (0x18) bytes with 32-bit pointers; a native-pointer host
 * struct would be ~36-40 bytes (8-byte pointers + alignment padding),
 * overrunning the real 24-byte `CSTGBankMemory::AllocAligned(0x18, 0x10)`
 * allocation this project's own KAT caught via a real out-of-bounds
 * write once tested -- the same host/target struct-size hazard already
 * hit (and fixed the same way) for CSTGGlobal::CSTGGlobal()'s own
 * tightly-packed fields (sec 10.55/10.56/10.57). */
template <typename T>
struct TSTGArrayManager {
	static TSTGArrayManager<T> *sInstance;
	unsigned int bucketArray;	/* +0x00 */
	unsigned int writeCursor;	/* +0x04 */
	unsigned int _unused8;		/* +0x08, confirmed zeroed, never read here */
	unsigned int modulus;		/* +0x0c, confirmed == count+1 */
	unsigned int indexArray;	/* +0x10 */
	unsigned int count;		/* +0x14 */
};

#endif /* OA_ENGINE_INIT_H */
