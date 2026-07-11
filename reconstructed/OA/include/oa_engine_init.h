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
struct CSTGSlotVoiceData;	/* forward decl, real definition in oa_global.h */
struct CSTGPerformanceVars;	/* forward decl, real definition further down this file */

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
 * `Init()` method (`.text+0x81a70`, 261 bytes, called once per
 * generator from CSTGWaveSeqManager::Initialize()) are reconstructed
 * for real, sec 10.152 -- see src/engine/waveseq_generator.cpp. Its
 * own +0x0/+0x4/+0xc fields ARE ALSO touched directly by
 * CSTGWaveSeqManager::Initialize() (a real intrusive doubly-linked
 * list node: next/prev/owner, the same 3-field convention already
 * confirmed for CSTGHeapHandleEntry, sec 10.59) -- manipulated via raw
 * offset arithmetic rather than named members there too, since the
 * rest of the class's layout isn't independently recovered.
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
	/* Confirmed real (`_ZN20CSTGWaveSeqGenerator9sDummyAMSE`, sec
	 * 10.152): a single shared static object whose ADDRESS (never
	 * dereferenced anywhere in Init()) is stored into FIVE of this
	 * class's own per-instance pointer fields (+0xc8/+0xcc/+0xd0/
	 * +0xf8/+0xfc, confirmed via five independent relocations to the
	 * same symbol), evidently a "no modulation source assigned"
	 * placeholder -- own real size/layout not independently confirmed
	 * since nothing in this pass ever reads through it, so left as a
	 * minimal opaque placeholder. */
	static unsigned char sDummyAMS[4];
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
 * public CStartupFile, sec 10.60).
 *
 * CORRECTS sec 10.66's own speculation (sec 10.148): that earlier pass,
 * without having disassembled this constructor yet, guessed it must be
 * the one setting the flags byte at derived-object-relative +0x6e that
 * CSTGVectorEGXY's own constructor partially clears (`and
 * BYTE PTR [this+0x6e],0xfd`). Directly disassembling the real, now-
 * reconstructed base ctor (`.text+0x7f820`, 22 bytes, both C1Ev/C2Ev
 * folded to the same address) proves it does NOT touch +0x6e at all --
 * it only writes the base vtable pointer (`*this = &_ZTV16CSTGVectorEGBase
 * [2]`, standard "+8 to skip offset-to-top/RTTI" convention, immediately
 * overwritten by each derived ctor's own vtable pointer right after),
 * `*(byte*)(this+0xc) = 0`, `*(byte*)(this+0xf) = 0`, and
 * `*(dword*)(this+8) = 0`. CSTGVectorEGXY's own AND-mask is therefore
 * clearing a bit in whatever uninitialized memory preceded construction
 * (this class is placed into pre-allocated CSTGBankMemory storage, sec
 * 10.64), not a value the base ctor set -- test_vector_eg_ctors.cpp's
 * own poison-then-construct mock updated to match (sec 10.148).
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
	 * GetNumWritableBytes() const (sec 10.150, `.text+0x400a0`, 84
	 * bytes) fully reconstructed (see midi_queue.cpp, a separate TU from
	 * midi_queue_writer.cpp -- see that file's own header comment for
	 * why). Confirmed via `CSTGGlobal::SubmitPerfChangeRequest`'s own
	 * call site: `this`
	 * is `*(CSTGMidiPortManager::sInstance + 0x208)` -- a DEREFERENCED
	 * pointer read from that field (not its address), i.e. this
	 * object's `this` IS the same `ringCtl` block `CSTGMidiQueueWriter::
	 * Write`'s own `+0x0` field points to (see oa_global.h's
	 * `CSTGMidiQueueWriter` comment for the confirmed shared field
	 * layout: `+0x8` capacity mask, `+0xc` write cursor, `+0x10+i*4`
	 * reader i's cursor, `+0x20` active reader count) -- modeled here
	 * as a separate opaque type reinterpreting the SAME ringCtl memory,
	 * matching this project's established non-inheritance convention.
	 * Confirmed formula: `(mask+1) - max_i(writeCursor - readerCursor[i])`
	 * for `i` in `[0, readerCount)` -- algebraically identical to
	 * `Write()`'s own "free space" computation, just without the
	 * subsequent copy.
	 */
	unsigned int GetNumWritableBytes() const;

	/*
	 * Reset() (batch 12, `.text+0x40060`, 36 bytes) confirmed real: zeroes
	 * the write cursor (`+0xc`) and all 4 reader cursors (`+0x10..+0x1f`,
	 * matching this class's own already-confirmed "+0x10+i*4 reader i's
	 * cursor" layout above) -- 5 dword stores total, nothing else. Does
	 * NOT touch the capacity mask (`+0x8`) or the active reader count
	 * (`+0x20`), a real, confirmed gap (every real caller reuses an
	 * already-`Initialize()`'d ring, just rewinding both cursors back to
	 * empty). See midi_queue.cpp.
	 */
	void Reset();
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
	/*
	 * Initialize() (.text+0x26a50, 75 bytes) is now real (sec 10.151,
	 * src/engine/global.cpp) confirmed: lazily runs InitializeLongHand()
	 * on a hidden static template object EXACTLY ONCE process-wide
	 * (guarded by `sTemplateReady`), then unconditionally copies the
	 * resulting `sTemplate` -- confirmed real size `0x92c` bytes via
	 * `readelf` (i.e. this IS this project's own first confirmed measure
	 * of `sizeof(CSTGChannelValues)` itself, independent of -- and not
	 * necessarily the same structure as -- the "0x92c-strided per-channel
	 * block" this class was said to live inside of, above) -- verbatim
	 * over `this` on every call, including the first.
	 * `InitializeLongHand()` itself (.text+0x26820, 550 bytes) is a
	 * confirmed-real, deliberately deferred dependency -- substantially
	 * larger, own body not reconstructed this pass; see bar2_stubs.cpp.
	 */
	void Initialize();
	void InitializeLongHand();
	static unsigned char sTemplateReady;
	static unsigned char sTemplate[0x92c];
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
 * layout not reconstructed -- only the confirmed real methods needed
 * to link those callers.
 *
 * SetIsDying(CSTGPerformanceVars*) (batch 19, `.text+0xb9a40`, 64
 * bytes, confirmed via relocation from `CSTGPerformanceVars::
 * SetIsDying()`) confirmed: the passed `CSTGPerformanceVars*` argument
 * is received (edx, regparm(3)) but CONFIRMED UNUSED anywhere in the
 * real body -- preserved faithfully as an unused parameter rather than
 * dropped. Unconditionally calls, in order: `CSTGFrontPanelSmoothers::
 * sInstance->OnPerformanceDeactivate()`, `this->fieldAt(0xad3)`
 * (an embedded `CSTGControllerInfo` sub-object)`-> OnPerformanceDeactivate()`,
 * `this->fieldAt(0xae7)` (an embedded `CSTGAudioInput` sub-object)
 * `-> OnPerformanceDeactivate()`, and `CSTGMessageProcessor::
 * sInstance->ClearUnsolicitedMessages()`. All four callees are newly
 * discovered, confirmed real, deliberately deferred externs -- own
 * bodies not reconstructed this pass (see src/engine/
 * performance_vars_set_is_dying.cpp).
 */
struct CSTGPerformance {
	bool IsCurrentlyActive() const;
	void SetIsDying(CSTGPerformanceVars *unused);

	/*
	 * RunEffects(CSTGPerformanceVars*) (batch 49, `.text+0xb9b50`, 256
	 * bytes, confirmed via relocation from `CSTGPerformanceVarsManager::
	 * RunEffects()`) confirmed real, deliberately deferred: genuine
	 * audio-DSP effect processing -- calls `CSTGEffectRack::
	 * RunEffects(CSTGEffectRackVars*)` (`this+4`, 636 bytes, its own
	 * further not-reconstructed DSP body), a 256-iteration SSE
	 * (`movaps`/`mulps`/`addps`/`subps`) stereo-pan-coefficient
	 * smoothing loop over `this+0x2140` (via `CSTGPan::
	 * CalculateStereoPanCoeffs`), `CSetListEQ::Run()` (conditionally, on
	 * `this+0x23dc`'s own gate byte), and `CSTGEffectRackVars::
	 * ApplyDModTickDelay()` -- out of scope per the sec 10.185
	 * audio-DSP-fidelity policy. `CSTGPerformanceVarsManager::
	 * RunEffects()` (the real caller, see oa_global.h) is fully real;
	 * only this DSP callee is deferred, matching the established
	 * reconstruct-caller-DSP-stub-callee pattern. */
	void RunEffects(CSTGPerformanceVars *vars);
};

/*
 * CSTGFrontPanelSmoothers::CSTGFrontPanelSmoothers() (`.text+0x1e850`,
 * confirmed real, sec 10.153) -- placement-`new`'d onto a
 * `CSTGBankMemory::AllocAligned(0xcb0, 0x10)` pool (`engine_init.cpp`),
 * and this ctor's own field writes account for the FULL 0xcb0 bytes
 * exactly (4+4+0x1f8+0x318+63*12+99*12 = 0xcb0) -- a clean, independent
 * confirmation of the class's total size. Layout:
 *   +0x000  dword  knobSmootherBuf -- packed 32-bit pointer (ToU32/
 *                  FromU32 convention) to a CSTGBankMemory::AllocAligned
 *                  (0x800, 0x10) buffer, fully zeroed then re-populated
 *                  by a confirmed "4-way interleaved" addressing scheme:
 *                  element i (0..62) lives at buf + (i>>2)*0x80 +
 *                  (i&3)*4 -- i.e. groups of 4 elements share a 128-byte
 *                  "row", each element's own fields living 4 bytes apart
 *                  within that row (an SoA-style layout, likely SIMD-
 *                  motivated even though this build has no SSE). 63
 *                  elements exactly fill 16 rows * 128 bytes = 0x800.
 *   +0x004  dword  eqSmootherBuf -- same scheme, CSTGBankMemory::
 *                  AllocAligned(0xc80, 0x10), 99 elements exactly filling
 *                  25 rows * 128 bytes = 0xc80.
 *   +0x008  0x1f8 bytes, confirmed zeroed, own meaning not determined.
 *   +0x200  0x318 bytes, confirmed zeroed, own meaning not determined.
 *   +0x518  63 * 12-byte elements, confirmed zeroed (3 dwords/elem:
 *           +0x0/+0x4/+0x8), own meaning not determined.
 *   +0x80c  99 * 12-byte elements, confirmed zeroed, same shape as above.
 * `sInstance = this` is confirmed set BEFORE the first AllocAligned call
 * (a real, harmless ordering quirk -- AllocAligned never reads
 * sInstance, preserved anyway for faithfulness).
 */
struct CSTGFrontPanelSmoothers {
	CSTGFrontPanelSmoothers();
	static CSTGFrontPanelSmoothers *sInstance;
	unsigned int knobSmootherBuf;			/* +0x000 */
	unsigned int eqSmootherBuf;			/* +0x004 */
	unsigned char _unrecovered1[0x1f8];		/* +0x008 */
	unsigned char _unrecovered2[0x318];		/* +0x200 */
	unsigned char _unrecovered3[63 * 12];		/* +0x518 */
	unsigned char _unrecovered4[99 * 12];		/* +0x80c */

	/* OnPerformanceDeactivate() (batch 19, `.text+0x208d0`, 523 bytes,
	 * confirmed via relocation from `CSTGPerformance::SetIsDying`)
	 * confirmed real, deliberately deferred extern -- own body
	 * substantially larger than this pass's scope, not reconstructed. */
	void OnPerformanceDeactivate();
};

struct CSTGHDRMiniModel {
	CSTGHDRMiniModel();
	static CSTGHDRMiniModel *sInstance;
	void Initialize();
};

/*
 * CSTGStreamingEvent::CSTGStreamingEvent() (`.text+0xd2090`, 72 bytes)
 * fully reconstructed (see src/engine/streaming_event_manager.cpp): calls
 * `CSTGAudioEvent::CSTGAudioEvent()` as its real base-object ctor, same
 * "placement-construct then overwrite vtable pointer" technique already
 * established for CSTGPlaybackEvent (its own derived fields at +0x30/+0x34
 * likewise start INSIDE CSTGAudioEvent's own confirmed +0x2c..+0x38 tail --
 * standard single inheritance can't express that overlap, so this is NOT
 * modeled via C++ `: public CSTGAudioEvent`). Confirmed real size 0xd4
 * (212 bytes) exactly, independently derived TWO ways: (1) the last
 * confirmed touch is the AND-masked flag byte at +0xd1; (2)
 * CSTGStreamingEventManager's own ctor constructs 401 of these back-to-back
 * at a clean, nothing-else-interleaved 0xd4-byte stride (matching the
 * `TSTGArrayManager`-adjacent "clean array" pattern already established
 * elsewhere in this project) -- both give the same number.
 */
struct CSTGStreamingEvent {
	CSTGStreamingEvent();
	unsigned char _unrecovered[0xd4];
};
/* The real vtable symbol (40 confirmed bytes via nm -CS, matching
 * CSTGAudioEvent/CSTGRecordEvent/CSTGPlaybackEvent's own vtable sizes) --
 * storage lives in bar2_stubs.cpp per this project's established "extern C
 * byte-array trick". */
extern "C" unsigned char _ZTV18CSTGStreamingEvent[];

/*
 * CSTGStreamingEventManager -- confirmed real object size EXACTLY 0x14c44
 * (independently confirmed via its own construction call site in
 * engine_init.cpp: `CSTGBankMemory::AllocAligned(0x14c44, 0x10)`, matching
 * this class's own field layout below byte-for-byte). Ctor
 * (`.text+0xd1b40`, 156 bytes) and Initialize() (`.text+0xd1be0`, 200
 * bytes) both fully reconstructed this pass (see
 * src/engine/streaming_event_manager.cpp).
 *
 *   +0x000        confirmed untouched by the ctor (the array below starts
 *                 at +0x004, not +0x000).
 *   +0x004        events[401], a confirmed clean 0xd4-byte-stride array
 *                 (401*0xd4 = 0x14c14 exactly -- the ctor's own
 *                 default-construction loop condition, `cmp esi,0x14c14`,
 *                 confirms this precisely; the real call-site argument to
 *                 Initialize(), 0x191 == 401, independently agrees).
 *   +0x14c18      freeListHead -- head of a singly-linked free list
 *                 threaded through each event's own +0x30 field (see
 *                 CSTGStreamingEvent's own header comment above),
 *                 populated by Initialize(), zeroed by the ctor.
 *   +0x14c1c      freeListTail -- tail of the same list, updated every
 *                 Initialize() iteration.
 *   +0x14c20      count -- incremented once per Initialize() iteration.
 *   +0x14c24/+0x14c28/+0x14c2c  confirmed zeroed by the ctor only; never
 *                 touched by Initialize() -- real semantics undetermined.
 *   +0x14c30      mutexPtr32 -- packed pointer, `rtwrap_malloc
 *                 (get_sizeof_rtwrap_pthread_mutex())` then
 *                 `rtwrap_pthread_mutex_init(mutex, 0)`, same established
 *                 idiom as CPowerOffTimer's own ctor (managers.cpp).
 *   +0x14c34      confirmed real gap between mutexPtr32 and the next
 *                 zeroed field -- never touched by either method here.
 *   +0x14c38      confirmed zeroed by the ctor only.
 *   +0x14c3c      confirmed zeroed by Initialize() only (unconditionally,
 *                 whether or not its own loop ran any iterations).
 *   +0x14c40      confirmed = Initialize()'s own 2nd argument * 2, set
 *                 unconditionally before the loop -- also the per-event
 *                 `m` argument this function forwards into each embedded
 *                 CSTGHDRCircularBuffer::Initialize() call.
 */
struct CSTGStreamingEventManager {
	CSTGStreamingEventManager();
	static CSTGStreamingEventManager *sInstance;
	/* Confirmed args (regparm this=eax/arg1=edx/arg2=ecx): edx=0x191
	 * (401, an unsigned short), ecx=0x10000 (65536, an unsigned long) --
	 * matching the mangled `10InitializeEtm` signature exactly. */
	void Initialize(unsigned short arg1, unsigned long arg2);

	unsigned char _unrecovered_head[4];		/* +0x000 */
	CSTGStreamingEvent events[401];		/* +0x004..+0x14c18 */
	unsigned int freeListHead;			/* +0x14c18 */
	unsigned int freeListTail;			/* +0x14c1c */
	unsigned int count;				/* +0x14c20 */
	unsigned int field14c24;			/* +0x14c24 */
	unsigned int field14c28;			/* +0x14c28 */
	unsigned int field14c2c;			/* +0x14c2c */
	unsigned int mutexPtr32;			/* +0x14c30 */
	unsigned int field14c34;			/* +0x14c34, confirmed gap */
	unsigned int field14c38;			/* +0x14c38 */
	unsigned int field14c3c;			/* +0x14c3c */
	unsigned int field14c40;			/* +0x14c40 */
};

struct CSTGSmoother {
	CSTGSmoother();
	static CSTGSmoother *sInstance;
	void Initialize();

	/* CancelAllSmoothers() (sec 10.78, confirmed via relocation from
	 * several UpdateXXX handlers -- already referenced by name in this
	 * project's own comments since sec 10.33) is real now, sec 10.154 --
	 * see src/engine/smoother_cancel.cpp for the full confirmed shape
	 * (unlinks and finalizes every entry on the real active-smoothers
	 * list, pushing each onto the free list via the same push-front
	 * template already confirmed real in Initialize() above). */
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

	/*
	 * CancelAllSlotVoiceDataCCSmoothers(const CSTGSlotVoiceData*) (batch
	 * 17, `.text+0x2b790`, 83 bytes, confirmed via relocation from
	 * `CSTGSlotVoiceData::FreeSlotVoiceData(bool)`) confirmed: walks the
	 * SAME singly-linked list as `CancelAllCCSmoothers()` above (anchored
	 * at `this->fieldAt(0xf010)`, node `+0x0`=next, `+0x8`=mapping
	 * pointer) but with a DIFFERENT filter -- mapping's own `+0x10` must
	 * equal `8` (not `2` or `8`) AND mapping's own `+0xac` must equal the
	 * passed `target` pointer exactly. On a match, calls `this->
	 * FinalizeSmoother(node, false)` -- see
	 * src/engine/slot_voice_data_free.cpp.
	 */
	void CancelAllSlotVoiceDataCCSmoothers(const CSTGSlotVoiceData *target);
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
	/*
	 * SetIsDying() is real now, batch 19 (`.text+0xbad40`, 478 bytes,
	 * confirmed via relocation from `CSTGGlobal::
	 * PreprocessPerformanceChange`) -- see src/engine/
	 * performance_vars_set_is_dying.cpp for the full confirmed shape:
	 * no-op unless `+0x23d1 == 2`; calls `this->fieldAt(0x23d4)->
	 * SetIsDying(this)` (the owning `CSTGPerformance`, arg confirmed
	 * unused by the callee); walks the SAME `CSTGGlobal::
	 * sInstance+0x29c9900` active-voice-data list `RunVoiceModelFeedback`/
	 * `NotifyAllKeysAndPedalsReleased` use, calling `SetIsDying()` on
	 * every payload whose own `+0x28c8` group id matches `+0x23d0` and
	 * AND-folding their `AreAllKeysAndPedalsReleased()` results; commits
	 * `+0x23d1 = 4` (all released, or list/filter empty) or `= 3`
	 * (still waiting), running the SAME "update front-panel active
	 * manager count, maybe PushUnsolicitedMessage" block
	 * `NotifyAllKeysAndPedalsReleased()`/`AllocPerformanceVars()`
	 * already use -- here CONFIRMED UNREACHABLE in practice (the block's
	 * own `oldState <= 1` guard is read immediately after this
	 * function's own entry guard already established `+0x23d1 == 2`,
	 * and nothing between the two writes that byte), the THIRD
	 * confirmed instance of the "unconditional pre-write makes a later
	 * guard unreachable" quirk in this cluster -- preserved faithfully
	 * as dead code rather than special-cased away. Finally calls
	 * `CSTGMIDIClockSync::sInstance->DisableActivePerfClock()`
	 * unconditionally.
	 */
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
	 * NotifyAllKeysAndPedalsReleased() (batch 17, `.text+0xbafc0`, 279
	 * bytes, confirmed via relocation from `CSTGSlotVoiceData::
	 * FreeSlotVoiceData(bool)`) confirmed: no-op unless `+0x23d1 == 3`.
	 * Otherwise walks the SAME `CSTGGlobal::sInstance+0x29c9900`
	 * active-voice-data list already confirmed for
	 * `FreeVoicelessDyingSlots()` below (node `+0x0`=next, `+0x8`=payload
	 * pointer) -- for each payload whose own `+0x28c8` byte equals
	 * `this->fieldAt(0x23d0)` (the SAME per-manager group id), calls the
	 * payload's `AreAllKeysAndPedalsReleased()`; if ANY qualifying
	 * payload returns false, bails out immediately (a real, confirmed
	 * early-return). Otherwise commits `+0x23d1 = 4` and, only if the
	 * OLD state was `<= 1`, recomputes the SAME front-panel "active
	 * manager count" (`STGAPIFrontPanelStatus::sInstance+0x1094`)
	 * `AllocPerformanceVars()`/`EnterActivatingState()` also maintain.
	 * CONFIRMED REAL, PRESERVED BUG-FOR-BUG: the real disassembly's own
	 * trailing `PushUnsolicitedMessage` block is genuinely UNREACHABLE
	 * (its own guard reads `+0x23d1` AFTER it was already unconditionally
	 * overwritten to `4` earlier in the SAME call) -- the exact same
	 * "unconditional pre-write makes a later guard unreachable" quirk
	 * already confirmed for `CSTGPerformanceVarsManager::
	 * AllocPerformanceVars()` (see oa_global.h/global.cpp). See
	 * src/engine/slot_voice_data_free.cpp for the full implementation.
	 */
	void NotifyAllKeysAndPedalsReleased();

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

/*
 * CSTGLFOTables::CSTGLFOTables() is real now, batch 28 -- see
 * src/engine/lfo_tables.cpp (`.text+0x12e260`, 2433 bytes, confirmed
 * ZERO `call`/vtable-dispatch instructions anywhere -- same "safe by
 * instruction class" category as CSTGSamplingInterface's ctor (sec
 * 10.160) and CSTGCCInfo::sCCInfoTable (sec 10.161), just with several
 * distinct loop shapes instead of one flat byte table). Populates a
 * fixed 0x1830-byte object (`CSTGBankMemory::AllocAligned(0x1830, 0x10)`,
 * engine_init.cpp) with ~15 lookup tables for LFO/step-sequencer
 * waveform generation -- phase ramps, a 128-entry S-curve/tanh-like
 * ramp table (from `.rodata`, reused four different ways: forward,
 * reversed, and two interleaved even/odd half-resolution extractions),
 * a 128-entry sine table (built from a 33-entry literal quarter-sine
 * table + mirror + negate, matching the classic "quarter wave + symmetry"
 * technique), an unidentified 110-entry envelope/window curve (no
 * closed form found), and four "staircase" quantization tables (3, 4,
 * 4, and 6 discrete levels respectively) likely backing a stepped/
 * random LFO mode. `+0x408` is the field `CSTGLFOBase::InitializeQuad()`
 * (`lfo_stepseq_quad.cpp`) already calls "lfoTables" -- the start of the
 * plain 64-entry constant-1.0 fill array, not the sine/S-curve tables;
 * a real, faithfully-preserved quirk (the pointer is just an address
 * value passed elsewhere, not necessarily "the interesting table").
 * Full derivation (every one of the ctor's ~6300 x86/x87 instructions,
 * including a from-scratch mini x87-stack interpreter used to replay
 * and cross-verify every table byte-for-byte, per this project's
 * established "replay-script" technique for large branch-free
 * functions, sec 10.161/10.171/10.172) documented in
 * src/engine/lfo_tables.cpp's own header comment.
 */
struct CSTGLFOTables {
	CSTGLFOTables();
	/* Confirmed real (`_ZN13CSTGLFOTables9sInstanceE`), needed by
	 * CSTGLFOBase::InitializeQuad() -- sec 10.61. */
	static CSTGLFOTables *sInstance;
};

/*
 * CSTGMIDIClockSyncBase / CSTGIntMIDIClockSync (batch 21, `.text+0x67410`
 * ctor cluster): a small polymorphic sub-object embedded at
 * `CSTGMIDIClockSync`'s own `+0x4` (confirmed: the ctor writes the
 * `_ZTV20CSTGIntMIDIClockSync+8` vtable pointer directly to
 * `outerThis+0x4`, then calls `Initialize()` with `eax = outerThis+0x4`
 * -- i.e. the embedded object's own offset-0 IS the outer object's
 * `+0x4`). Only ONE combined vtable exists (`_ZTV20CSTGIntMIDIClockSync`,
 * 40 bytes / 8 slots, readelf-confirmed) -- `CSTGMIDIClockSyncBase` has
 * no data fields of its own beyond what `CSTGIntMIDIClockSync` uses, so
 * modeled as a plain (non-`virtual`, matching this project's own
 * "install-only, never-dispatched" precedent, sec 10.153/10.160) base
 * with a normal derived class. Nothing in this project dispatches
 * through this vtable yet (a real, safe zero-filled placeholder,
 * bar2_stubs.cpp) -- if some future pass adds real dispatch, populate it
 * with these 8 confirmed real slot targets (readelf -Wr
 * '.rel.rodata._ZTV20CSTGIntMIDIClockSync', slot order matches
 * declaration order below):
 *   0x08 GetEventCount, 0x0c NotifySyncDetected, 0x10 GetEventStatusByte,
 *   0x14 ProcessClock, 0x18 ConsumeEvent, 0x1c GetClockEarlyThresholdTicks,
 *   0x20 GetClockLateThresholdTicks, 0x24 PrepareForNextTick.
 */
class CSTGMIDIClockSyncBase {
public:
	/*
	 * Initialize() (`.text+0x67a50`, 152 bytes) confirmed real:
	 *   - once ever (own function-local static guard byte), computes
	 *     `kClockTimeOutTicks = ceil(0.104 * CSTGAudioBusManager::
	 *     sInstance->busGainScale)` (0.104 a confirmed real
	 *     `.rodata.cst8` double; the real code sets the x87 rounding
	 *     control to "round toward +infinity" before `frndint`+`fisttp`,
	 *     reproduced via inline asm, not a plain C cast/truncate).
	 *   - every call: `kMaxNormalizedTempo = 200.0f *
	 *     CSTGAudioBusManager::sInstance->busGainReciprocal` (float);
	 *     zeroes `fieldAt(0x8)` (int) and `fieldAt(0x14)` (byte); sets
	 *     `fieldAt(0xc)` (double) = `48.0 *
	 *     CSTGAudioBusManager::sInstance->busGainReciprocal`.
	 * `fieldAt(0x8)`/`fieldAt(0x14)` have no other confirmed reader in
	 * this pass -- left as raw offsets, not named.
	 */
	void Initialize();

	static int kClockTimeOutTicks;
	static float kMaxNormalizedTempo;
};

class CSTGIntMIDIClockSync : public CSTGMIDIClockSyncBase {
public:
	/* GetEventCount() const (`.text+0x67e80`, 11 bytes): return
	 * fieldAt(0x54) - fieldAt(0x58) (write-index minus read-index of the
	 * 16-byte event-status ring below). */
	unsigned int GetEventCount() const;

	/* GetEventStatusByte() const (`.text+0x67e90`, 12 bytes): return the
	 * ring byte at fieldAt(0x44 + (fieldAt(0x58) & 0xf)) -- a 16-entry
	 * byte ring anchored at +0x44, indexed by the read-counter mod 16. */
	unsigned char GetEventStatusByte() const;

	/* ConsumeEvent() (`.text+0x67ea0`, 10 bytes): fieldAt(0x58) += 1
	 * (advances the ring read-index). */
	void ConsumeEvent();

	/* PrepareForNextTick() (`.text+0x67eb0`, 66 bytes) confirmed real:
	 * ONLY when NOT syncing to an external clock
	 * (`!SKSTGGate_ShouldSyncExternalClock()`), recomputes fieldAt(0xc)
	 * (double) = `(double)SKSTGGate_GetInternalTempo() * 0.01 * 0.4 *
	 * CSTGAudioBusManager::sInstance->busGainReciprocal` (both 0.01/0.4
	 * confirmed real `.rodata.cst8` doubles). Byte-for-byte identical
	 * computation to NotifySyncDetected() below, just gated. */
	void PrepareForNextTick();

	/* NotifySyncDetected() (`.text+0x67f00`, 57 bytes): unconditionally
	 * runs the SAME fieldAt(0xc) computation as PrepareForNextTick()'s
	 * gated branch (confirmed identical opcodes). */
	void NotifySyncDetected();

	/* ProcessClock() (`.text+0x67650` section, 1 byte: bare `ret`) --
	 * confirmed real no-op override. */
	void ProcessClock();

	/* GetClockLateThresholdTicks() const (2 bytes: `fld1;ret`) --
	 * confirmed real: always returns 1.0f. */
	float GetClockLateThresholdTicks() const;

	/* GetClockEarlyThresholdTicks() const (2 bytes: `fldz;ret`) --
	 * confirmed real: always returns 0.0f. */
	float GetClockEarlyThresholdTicks() const;
};

struct CSTGMIDIClockSync {
	/*
	 * CSTGMIDIClockSync() (batch 21, `.text+0x67410`, 250 bytes)
	 * confirmed real: sets `fieldAt(0x44)` (byte) = 1; installs the
	 * embedded `CSTGIntMIDIClockSync` sub-object's vtable at `+0x4` and
	 * calls its `Initialize()` (see class above); zeroes
	 * fieldAt(0x5c/0x68/0x6c/0x70/0x74/0x60/0x64) (int); mirrors
	 * fieldAt(0x58) = fieldAt(0x5c) (both 0); sets
	 * fieldAt(0x78/0x98/0xb8) (double, all three IDENTICAL) = `48.0f *
	 * CSTGAudioBusManager::sInstance->busGainReciprocal` (48.0f a
	 * confirmed real `.rodata.cst4` float); zeroes fieldAt(0x80/0xa0/0xc0)
	 * (double); zeroes fieldAt(0x88/0x8c/0x90/0x94/0xa8/0xac/0xb0/0xb4)
	 * (int); sets fieldAt(0xc8) = -1; sets sInstance = this. */
	CSTGMIDIClockSync();
	/* Confirmed real (`_ZN17CSTGMIDIClockSync9sInstanceE`), needed by
	 * CSTGLFOBase::InitializeQuad() -- sec 10.61. */
	static CSTGMIDIClockSync *sInstance;

	/* DisableActivePerfClock() is real now, batch 19 (`.text+0x675b0`,
	 * 11 bytes, confirmed via relocation from `CSTGPerformanceVars::
	 * SetIsDying()`) -- trivially sets `fieldAt(0xc8) = -1`. See
	 * src/engine/performance_vars_set_is_dying.cpp. */
	void DisableActivePerfClock();

	/*
	 * GetFilteredTempoBPM(unsigned int) const is real now, batch 49
	 * (`.text+0x67990`, 108 bytes, confirmed via relocation from
	 * `CSTGEffectManager::RunEffects()`) confirmed:
	 *   - `index` (regparm edx) is clamped to 0 if >= 2 (unsigned
	 *     `cmovae`, matching the two-slot shape below).
	 *   - if `SKSTGGate_ShouldSyncExternalClock()` AND
	 *     `fieldAt(0x60)` (a packed 32-bit pointer, confirmed zeroed by
	 *     the ctor above -- the SAME field) is non-null: returns
	 *     `(float)*(int*)(fieldAt(0x60)+0x1c4)` (an int-to-float
	 *     conversion of a raw tick count on a not-independently-modeled
	 *     object, real `fildl` instruction) -- no further fields of that
	 *     object are reconstructed in this pass.
	 *   - otherwise: returns `(float)((double)CSTGAudioBusManager::
	 *     sInstance->busGainScale * fieldAt(0x98 + index*0x20) * 2.5)`
	 *     -- `fieldAt(0x98)`/`fieldAt(0xb8)` are the SAME two "smoothed
	 *     tempo interval" doubles the ctor initializes to `48.0 *
	 *     busGainReciprocal` (see ctor comment above); `2.5f` a
	 *     confirmed real `.rodata.cst4` float. CROSS-CHECK: at the
	 *     ctor's own default state (`busGainScale=1500.0`,
	 *     `busGainReciprocal=1/1500`), this evaluates to EXACTLY
	 *     `1500.0 * (48.0/1500.0) * 2.5 == 120.0` -- independently
	 *     confirming `CSTGEffectManager`'s own `defaultTempoA/
	 *     defaultTempoB` "120.0f, plausible default tempo" flag (oa_engine.h)
	 *     as the REAL computed steady-state value, not merely a guess.
	 * See src/engine/midi_clock_sync.cpp for the implementation.
	 */
	float GetFilteredTempoBPM(unsigned int index) const;
};

/*
 * CTimerManager -- a genuinely new, entirely separate class discovered
 * while reconstructing `SKSTGGate_ShouldSyncExternalClock()` (sec
 * 10.148, src/engine/sk_stg_gate.cpp): a real MIDI-clock-sync engine
 * with well over a dozen of its own methods (CTimerManager/
 * ~CTimerManager, Process/Idle/ResetClock/CheckAndSendMIDIClock/
 * UpdateCurrentTime/ProcessWhenSyncInternal/ProcessWhenSyncExternal/
 * ShouldSyncExternalClock/GetInternalTempo/GetTimeUsFromLastClock/
 * GetTimeUsTillNextClock/GetTimeUsTillCurrentClock/
 * AdvanceClockForWaveSequence -- confirmed via the symbol table only).
 * Declared here as a minimal opaque stand-in with just the one method
 * this pass's caller needs -- NOT the same class as the already-
 * reconstructed CSTGMIDIClockSync just above (confirmed separate
 * mangled namespace, separate `sInstance`/`ms_poInstance` symbols).
 */
struct CTimerManager {
	static CTimerManager *ms_poInstance;
	bool ShouldSyncExternalClock();

	/* GetInternalTempo() (batch 21, `.text+0x347250`, 6 bytes) confirmed
	 * real: `return *(int*)(*(int**)this + 0x2c);` -- the SAME "this is
	 * itself a pointer to the real data holder" indirection quirk
	 * already confirmed for ShouldSyncExternalClock()'s own
	 * CKGBankManager reload above, just one level of indirection through
	 * `this` instead of a separate global. Reproduced verbatim, not
	 * "fixed" to read a direct field. */
	int GetInternalTempo();
};

/*
 * CKGBankManager -- a genuinely new, entirely separate class discovered
 * while reconstructing CTimerManager::ShouldSyncExternalClock() itself
 * (sec 10.151, src/engine/sk_stg_gate.cpp): a real, faithfully-preserved
 * quirk -- despite being a (nominal) member method receiving `this` from
 * its one real caller (SKSTGGate_ShouldSyncExternalClock(), which passes
 * CTimerManager::ms_poInstance as `this`), the real disassembly IGNORES
 * `this` entirely and instead reloads this totally different global,
 * CKGBankManager::ms_poInstance (confirmed via its own real relocation,
 * R_386_32 -> `_ZN14CKGBankManager13ms_poInstanceE`). Declared here as a
 * minimal opaque stand-in (own class layout entirely out of scope) --
 * the huge fixed byte offset ShouldSyncExternalClock() reads through it
 * (`+0x97c750`, ~9.9MB) strongly suggests this pointer targets one of
 * this project's already-known giant global aggregate structures
 * (comparable in scale to CSTGGlobal's own multi-hundred-KB layout), not
 * a normal small C++ object.
 */
struct CKGBankManager {
	static unsigned char *ms_poInstance;
};

/* Also declared in oa_global.h (sec 10.98) -- same real, non-`extern
 * "C"` mangled global function, matching signature; a harmless
 * redeclaration where both headers happen to be included together. */
bool SKSTGGate_ShouldSyncExternalClock();

/*
 * SKSTGGate_GetInternalTempo() (batch 21, `.text+0x349d30`, 20 bytes)
 * confirmed real: loads `CTimerManager::ms_poInstance` and makes a
 * direct (non-virtual) regparm(3) call to `CTimerManager::
 * GetInternalTempo()` with it as `this`, same "no null check" shape as
 * SKSTGGate_ShouldSyncExternalClock() above -- reproduced verbatim.
 */
int SKSTGGate_GetInternalTempo();

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

/*
 * Batch 42 (2026-07-11): the ten Model ctors PROMOTED to real bodies,
 * superseding sec 10.147/10.154's own "deliberately NOT promoted,
 * disproportionate structural cost" judgment -- see
 * src/engine/voice_models.cpp for the full implementation. What
 * actually changed since that verdict: `CSTGEngine::Initialize()`
 * (engine_init.cpp) does `CallVtableSlot(new (...) CSTGXxxModel(), 2)`
 * right after constructing each one, and sec 10.154 correctly flagged
 * that promoting these ctors with a zero-filled placeholder vtable
 * would be a confirmed NEW wild call (slot 2 == null) -- exactly the
 * `CCostProfile` "too-short auto-vtable" bug class sec 10.186 already
 * fixed once. The sec 10.185 audio-DSP policy resolves this cleanly:
 * give each model a REAL, correctly-shaped vtable (matching ground
 * truth's own confirmed 0x5c-byte / 23-slot layout, hand-built exactly
 * like `kCCostProfileVtbl`) with the ONE slot any currently-reachable
 * code actually dispatches (slot 2 = `Initialize()`) pointed at a real
 * function -- for 9 of the 10 models that function is a confirmed-real,
 * deliberately-out-of-scope audio-DSP no-op (per-model oscillator/
 * parameter-table setup, 332-2097 bytes each); `CSTGOffModel::
 * Initialize()` is genuinely 1 byte (`ret`) in ground truth, so it's
 * reconstructed as real, not a stub. All 20 other slots stay null,
 * safe because nothing in this project's own reachable call graph
 * dispatches through them (see voice_models.cpp's own header comment
 * for the ADDITIONAL wrinkle this batch found and also fixed --
 * `ProcessSubRate`/`ProcessAudioRate`, slots 18/19, are ALSO reachable
 * once `Register()` below is made real, so those get the same
 * real-for-Off/no-op-for-the-rest treatment too).
 *
 * `CSTGVoiceModel` base class -- confirmed real (`.text+0x1a9b10`,
 * 338 bytes), genuinely mechanical, ZERO DSP or vtable-dispatch
 * content of its own (only calls `CSTGBankMemory::AllocAligned`,
 * `operator new[]`, and `CSTGVoiceModelManager::Register` -- all
 * already-real or newly-reconstructed-this-batch primitives). Modeled
 * via the SAME "opaque + raw offset writes onto `this`" convention as
 * `CSTGHDRMiniModel` (engine_init.cpp) -- the class declares no data
 * members beyond the leading vtable-pointer slot (needed so
 * `CallVtableSlot`'s generic `*(void***)obj` dispatch is well-defined);
 * every other field is written via raw byte offsets in the .cpp, never
 * needing to match `sizeof(CSTGVoiceModel)` since every real instance
 * is always placement-`new`'d onto a correctly-pre-sized
 * `CSTGBankMemory::AllocAligned` allocation (0x108 bytes for 8 of the
 * 10 models, 0x508 for Piano, 0x124 for EP -- Piano/EP's own extra
 * tail bytes beyond the shared 0x108 base are untouched by any ctor,
 * confirmed real gaps, matching this project's "opaque, not fabricated"
 * convention).
 *
 * A NOTE on what's deliberately simplified vs ground truth, for
 * fidelity-auditing purposes: the base ctor's own confirmed `+0x84..
 * +0xd1` (78-byte) zero-fill is executed TWICE in the real disassembly
 * (a genuine compiler-observed redundancy, not two different regions --
 * both zero loops use the identical computed address range) -- collapsed
 * to a single zero-fill here, matching this project's established
 * "functionally-inert redundant write, preserved as one statement"
 * precedent (e.g. `CSTGProgramSlot`'s own `+0x9`/`+0x30` double-zero,
 * sec 10.153). The base ctor's OWN vtable-pointer write (it installs
 * `_ZTV14CSTGVoiceModel` immediately before the derived ctor
 * unconditionally overwrites it with the real derived vtable, the same
 * "harmless overwrite" pattern as `CSTGProgramSlot`/`CSTGToneAdjust`)
 * is likewise NOT reproduced -- nothing ever reads it in between, so
 * modeling it changes no observable behavior.
 *
 * Field offsets below (all confirmed via direct disassembly of the base
 * ctor, `.text+0x1a9b10`):
 *   +0x00        vtable ptr (see above)
 *   +0x84..+0xd1 zeroed (78 bytes, see note above)
 *   +0xd4        packed ptr: `operator new[](channelCount*0xc)`, each
 *                12-byte record zeroed (+0x0/+0x4/+0x8) -- `channelCount`
 *                is `CSTGAudioManager::sInstance`'s own `+0x18` field
 *                (an opaque count read raw, matching this project's
 *                established convention for a not-yet-individually-named
 *                singleton field -- see `oa_engine.h`'s own
 *                `CSTGAudioManager::_unrecovered_head` comment, `+0x18`
 *                falls inside that still-opaque blob).
 *   +0xd8        unsigned short = 0xffff ("unset" sentinel)
 *   +0xe0        byte = 0
 *   +0xe1        byte = 0 (base default; every derived ctor overwrites
 *                with its own per-model flag byte, see below)
 *   +0xe2        byte &= 0xfc (clear low 2 bits; some derived ctors OR
 *                extra bits back in afterward)
 *   +0xe4        packed ptr: `AllocAligned(0x1a80, 0x80)`, zeroed
 *   +0xe8        packed ptr: `AllocAligned(0x3300, 0x80)`, zeroed
 *   +0xec        packed ptr: `AllocAligned(0xcc0, 0x10)` (NOT zeroed by
 *                this ctor -- confirmed real gap, contents whatever
 *                `AllocAligned` handed back)
 *   +0xf0        unsigned short = 0
 *   +0xf4        packed ptr: `AllocAligned(0x6a0, 0x10)` (also NOT
 *                zeroed here, same confirmed real gap as +0xec)
 *   +0xf8        unsigned short = 0
 *   +0xfc        dword = 0
 *   +0x100       dword = 0 (base ctor's own write)
 *   +0x104       dword = 0 (EACH derived ctor's own write, confirmed via
 *                disassembly to be absent from the base ctor -- kept
 *                attributed to the derived ctors below for provenance,
 *                even though the literal value is identical across all
 *                ten)
 * `CSTGVoiceModelManager::Register(type, this)` is called once, near
 * the end of the base ctor (after +0xd4's array is built, before
 * +0xd8/+0x100).
 *
 * Per-model flag-byte writes at `+0xe1`/`+0xe2` (all ten independently
 * re-disassembled this batch, confirming sec 10.147's own earlier
 * survey byte-for-byte): Off=OR 0x3f; PCM=OR 0x7f; AnalogSync=OR 0x57;
 * Organ=MOV 0xc1; Plucked=(read,&0x80,|0x77),OR+0xe2,0x1;
 * MS20=MOV 0xd7,OR+0xe2,0x1; Polysix=MOV 0xd7; VPM=(read,&0x80,|0x77),
 * OR+0xe2,0x1; Piano=MOV 0xd1; EP=MOV 0xd1,OR+0xe2,0x2. Real per-bit
 * meaning still not determined (plausibly per-model synthesis-engine
 * capability flags) -- not needed to reproduce the confirmed byte
 * values faithfully.
 */
class CSTGVoiceModel {
public:
	CSTGVoiceModel(eSTGVoiceModelType type);
	/* Mock/test-only convenience overload -- NOT a second ground-truth
	 * constructor (ground truth has exactly one, confirmed via its own
	 * single mangled symbol `_ZN14CSTGVoiceModelC2E18eSTGVoiceModelType`).
	 * Added so verify/test_engine_init.cpp's own pre-existing MOCK_MODEL
	 * macro (isolated per-model ctor mocks that predate this batch) can
	 * still default-construct the now-real `CSTGVoiceModel` base without
	 * pulling the real ctor's own dependencies (`CSTGBankMemory::
	 * AllocAligned`, `operator new[]`, `CSTGAudioManager::sInstance`,
	 * `CSTGVoiceModelManager::sInstance`) into that test -- the same
	 * "isolated test needs its own lightweight seam" precedent already
	 * used elsewhere in this project. Empty body, touches no fields;
	 * every real (non-mock) derived ctor uses the parameterized overload
	 * above instead. */
	CSTGVoiceModel() {}
	void *_vtablePtr;	/* +0x00 -- MUST stay the object's first word:
				 * `CallVtableSlot`'s generic `*(void***)obj`
				 * dispatch (engine_init.cpp) assumes it. */
};

/*
 * `Initialize()` (vtable slot 2) / `ProcessSubRate(unsigned int)` (slot
 * 18, `.text` offset +0x48) / `ProcessAudioRate(unsigned int)` (slot 19,
 * +0x4c) -- the only three of each model's 21 real virtual methods any
 * CURRENTLY-REACHABLE code in this reconstruction dispatches (slot 2 via
 * `CallVtableSlot` right after construction; slots 18/19 via
 * `CSTGVoiceModelManager::ProcessSubRate`/`ProcessAudioRate`, already
 * real since sec 10.137, once `Register()` -- new this batch -- actually
 * populates the array those two walk). All confirmed real in ground
 * truth (`nm -C -S`); `CSTGOffModel`'s own three are confirmed literally
 * 1 byte each (a bare `ret`) and are reconstructed for real (see
 * voice_models.cpp); the other 27 (9 models x 3 methods) are confirmed
 * substantial (up to ~2KB) genuine per-model DSP init/audio-tick bodies
 * -- out of scope per the sec 10.185 policy, given safe no-op stand-ins
 * in bar2_stubs.cpp (matching the `CSetListEQ::SetBand`/
 * `CSTGControllerInfo::SetPerfSwitch` precedent). Declared here as
 * `extern "C"` free functions (own vtable-slot signature, `void(*)(void*)`/
 * `void(*)(void*,unsigned int)`), NOT C++ methods -- matching the
 * `CStartupFile::Load`/`kCCostProfileVtbl` precedent (sec 10.186)
 * exactly, since nothing in this project ever calls them via `.`/`->`
 * syntax, only through a raw vtable-slot function pointer.
 */
extern "C" void OA_VoiceModel_Off_Initialize(void *self);
extern "C" void OA_VoiceModel_Off_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Off_ProcessAudioRate(void *self, unsigned int tick);

/*
 * The other nine models' own Initialize()/ProcessSubRate()/
 * ProcessAudioRate() -- confirmed real, substantial (332-2097 bytes),
 * genuine per-model DSP -- deliberately deferred, safe no-op bodies
 * defined in bar2_stubs.cpp (matching the CSetListEQ::SetBand
 * precedent, sec 10.192). Declared here only so voice_models.cpp can
 * take their address for each model's own real vtable.
 */
extern "C" void OA_VoiceModel_PCM_Initialize(void *self);
extern "C" void OA_VoiceModel_PCM_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_PCM_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_AnalogSync_Initialize(void *self);
extern "C" void OA_VoiceModel_AnalogSync_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_AnalogSync_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Organ_Initialize(void *self);
extern "C" void OA_VoiceModel_Organ_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Organ_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Plucked_Initialize(void *self);
extern "C" void OA_VoiceModel_Plucked_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Plucked_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_MS20_Initialize(void *self);
extern "C" void OA_VoiceModel_MS20_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_MS20_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Polysix_Initialize(void *self);
extern "C" void OA_VoiceModel_Polysix_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Polysix_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_VPM_Initialize(void *self);
extern "C" void OA_VoiceModel_VPM_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_VPM_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Piano_Initialize(void *self);
extern "C" void OA_VoiceModel_Piano_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Piano_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_EP_Initialize(void *self);
extern "C" void OA_VoiceModel_EP_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_EP_ProcessAudioRate(void *self, unsigned int tick);

struct CSTGOffModel : public CSTGVoiceModel { CSTGOffModel(); static CSTGOffModel *sInstance; };
struct CSTGPCMModel : public CSTGVoiceModel { CSTGPCMModel(); static CSTGPCMModel *sInstance; };
struct CSTGAnalogSyncModel : public CSTGVoiceModel { CSTGAnalogSyncModel(); static CSTGAnalogSyncModel *sInstance; };
struct CSTGOrganModel : public CSTGVoiceModel { CSTGOrganModel(); static CSTGOrganModel *sInstance; };
struct CSTGPluckedModel : public CSTGVoiceModel { CSTGPluckedModel(); static CSTGPluckedModel *sInstance; };
struct CSTGMS20Model : public CSTGVoiceModel { CSTGMS20Model(); static CSTGMS20Model *sInstance; };
struct CSTGPolysixModel : public CSTGVoiceModel { CSTGPolysixModel(); static CSTGPolysixModel *sInstance; };
struct CSTGVPMModel : public CSTGVoiceModel { CSTGVPMModel(); static CSTGVPMModel *sInstance; };
struct CSTGPianoModel : public CSTGVoiceModel {
	CSTGPianoModel();
	/* NOTE: `sInstance`'s own STORAGE is defined in src/auth/process_oacmd.cpp
	 * (as `char *`, that TU's own separate, pre-existing, incompatible
	 * declaration ecosystem for this same class name -- see this
	 * project's already-established "two incompatible CSTGPianoModel
	 * declarations" note, oa_types.h's own forward decl). Declared
	 * `static` here too (same mangled storage, `_ZN14CSTGPianoModel9sInstanceE`)
	 * but DELIBERATELY NOT DEFINED in voice_models.cpp -- doing so would
	 * be a real duplicate-definition link error at `make ko` (the two
	 * ecosystems' storage is the SAME linker symbol despite the type
	 * mismatch, matching the sec 10.154 `CSTGPCMPrecacheManager::Reset`
	 * int/long precedent for tolerated cross-ecosystem type drift). */
	static CSTGPianoModel *sInstance;
	/* RescanPianoTypes() (Bar 2, confirmed real via the real binary's
	 * own symbol table, own body not reconstructed) -- deliberately
	 * deferred extern. */
	void RescanPianoTypes();
};
struct CSTGEPModel : public CSTGVoiceModel { CSTGEPModel(); static CSTGEPModel *sInstance; };

/* Opaque per-quad sub-rate parameter blocks -- real layouts not
 * reconstructed in this pass, only their confirmed sizes (matching the
 * exact stride Initialize()'s own loop advances by: 0x250/592 bytes for
 * LFOs, 0x100/256 bytes for step sequencers). */
struct STGLFOSubRateParams { unsigned char _unrecovered[0x250]; };
struct STGStepSeqSubRateParams { unsigned char _unrecovered[0x100]; };

/*
 * CSTGCommonLFO::CSTGCommonLFO() (batch 44, `.text+0x89950`, 64 bytes)
 * confirmed real -- an INSTANCE ctor for the same real C++ class as the
 * static `Initialize()`/`sSubRateParams` pool-holder members just below
 * (confirmed same mangled class name; C++ freely mixes static and
 * instance members in one class, and `CSTGProgram::CSTGProgram()`
 * places one instance of this class at a confirmed real fixed offset,
 * `+0xb74` -- see src/engine/program_ctor.cpp). Genuine nested multiple
 * inheritance, same shape as `CSTGProgram` itself one level up: installs
 * TWO vtable pointers of its OWN, both into the SAME `_ZTV13CSTGCommonLFO`
 * symbol (0x7c bytes, confirmed via `nm -CS`) at two different
 * sub-offsets -- +0x0 = vtable+8, +0x4 = vtable+0x6c -- plus eleven
 * confirmed zeroed/packed-zero scalar fields (+0xd dword, +0x11/+0x1a/
 * +0x1b/+0x20/+0x26/+0x2b/+0x30 bytes, +0x22/+0x27/+0x2c dwords). No
 * dispatch anywhere in this ctor's own body. Left as a zero-filled
 * placeholder vtable per this project's established "install vs
 * dispatch" rule -- nothing reconstructed in this project reads a
 * function pointer out of it.
 */
extern "C" unsigned char _ZTV13CSTGCommonLFO[0x7c];
struct CSTGCommonLFO {
	CSTGCommonLFO();
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

/*
 * CSTGPlaybackEvent::CSTGPlaybackEvent() (`.text+0xd6c90`, C1Ev/C2Ev
 * folded, 118 bytes) fully reconstructed (see engine_init.cpp): calls
 * `CSTGAudioEvent::CSTGAudioEvent()` as its real base-object ctor (a
 * genuine derived-class relationship, confirmed via the real `C2Ev`
 * relocation -- NOT modeled here via C++ inheritance, since the
 * derived ctor's own field writes start at `+0x30`, INSIDE the base's
 * own confirmed `+0x2c..+0x38` unrecovered tail, i.e. the two field
 * ranges genuinely overlap by 8 bytes -- standard Itanium single
 * inheritance can never place derived fields before `sizeof(Base)`, so
 * plain `: public CSTGAudioEvent` would misrepresent the real layout;
 * reproduced instead via the SAME placement-construct-then-patch-vtable
 * technique already established for `CSTGRecordEvent`, matching the
 * real ctor's own instruction order exactly: base ctor call, own
 * vtable-pointer overwrite, then 13 further confirmed zero-stores at
 * `+0x30/+0x34/+0x38/+0x3c/+0x40/+0x44/+0x48/+0x50/+0x54/+0x58/+0x60/
 * +0x61/+0x64`). Total confirmed real size 0x68 (104 bytes), matching
 * the pre-existing `_unrecovered[0x68]` declaration below exactly.
 *
 * Batch 25 adds the rest of this class's small confirmed methods (see
 * src/engine/playback_event_methods.cpp for the full per-method
 * derivation): Reset()/HandleFileOpened()/HandleFileClosed()/
 * HandleErrorOpening()/HandleErrorReading()/
 * GetDispositionForReadAttempt()/IncrementBufferStartLocation()/
 * SeekSkipFileBytes()/~CSTGPlaybackEvent() -- every one of these,
 * confirmed via `nm -C -S OA.ko | grep CSTGPlaybackEvent::`, is now
 * reconstructed; every offset they touch stays accessed via raw byte
 * arithmetic on `_unrecovered` (same "still-opaque class" treatment
 * playback_buffer_events.cpp already established), EXCEPT the shared
 * `CSTGAudioEvent` prefix fields (`+0x8`/`+0x1d`/`+0x24`, all strictly
 * BEFORE the `+0x2c` overlap boundary, so reinterpreting `this` as
 * `CSTGAudioEvent*` to reach them by name is exact, not a guess).
 * Reset()'s own real vtable-slot-7 installation (confirmed via readelf
 * relocation resolution against `.rodata._ZTV17CSTGPlaybackEvent`) is
 * now the ONLY installed target for that slot across the whole real
 * binary -- see `CSTGPlaybackBuffer::RemoveEvent`/`EventFileError`
 * (playback_buffer_events.cpp) for how that dispatch is reproduced.
 */
struct CSTGPlaybackEvent {
	CSTGPlaybackEvent();
	~CSTGPlaybackEvent();
	void Reset();
	void HandleFileOpened();
	void HandleFileClosed();
	void HandleErrorOpening();
	void HandleErrorReading();
	unsigned int GetDispositionForReadAttempt(unsigned int pos) const;
	void IncrementBufferStartLocation(unsigned int n);
	void SeekSkipFileBytes(unsigned int delta);
	unsigned char _unrecovered[0x68];
};
/* The real vtable symbol (40 confirmed bytes via readelf, `vtable for
 * CSTGPlaybackEvent`, matching CSTGAudioEvent/CSTGRecordEvent's own
 * vtable sizes) -- storage lives in bar2_stubs.cpp per this project's
 * established "extern C byte-array trick". */
extern "C" unsigned char _ZTV17CSTGPlaybackEvent[];

/*
 * CSTGAudioEvent::CSTGAudioEvent() (sec 10.149, `.text+0xd1830`, C1Ev/
 * C2Ev folded to one address, 76 bytes) fully reconstructed: writes the
 * confirmed 32-bit vtable-pointer field then 11 further confirmed
 * scalars, all direct immediate stores (no calls, no branches). Named
 * per-field rather than left as an opaque blob since every byte up to
 * +0x2c is now confirmed; +0x2c..+0x38 remains an explicitly-labeled
 * unrecovered tail (CSTGRecordEvent's own 56-byte/0x38 element stride,
 * confirmed via engine_init.cpp's BuildArrayManager call, leaves this
 * much room past the ctor's own last write). `sampleRate`'s value
 * (0xbb80 = 48000) is the only field whose semantic role is reasonably
 * inferable from its value alone; the rest are confirmed-value-only,
 * not confirmed-semantics (fieldN naming, not a guess at meaning).
 *
 * Reset() (batch 25, `.text+0xd17e0`, 70 bytes) is CONFIRMED
 * BYTE-FOR-BYTE IDENTICAL to the ctor's own 12 field writes above,
 * minus the vtable-pointer install -- see playback_event_methods.cpp.
 */
struct CSTGAudioEvent {
	CSTGAudioEvent();
	void Reset();
	unsigned int  vtablePtr32;	/* +0x0, packed 32-bit vtable pointer (see ctor) */
	unsigned char _gap4[4];		/* +0x4..+0x7, not touched by ctor */
	unsigned int  field8;		/* +0x8, confirmed zeroed */
	unsigned int  fieldC;		/* +0xc, confirmed real value 4 */
	unsigned int  field10;		/* +0x10, confirmed zeroed */
	unsigned char field14;		/* +0x14, confirmed zeroed */
	unsigned char field15;		/* +0x15, confirmed zeroed */
	unsigned char field16;		/* +0x16, confirmed zeroed */
	unsigned char _gap17;		/* +0x17, not touched by ctor */
	unsigned int  field18;		/* +0x18, confirmed zeroed */
	unsigned char field1c;		/* +0x1c, confirmed real value 1 */
	unsigned char field1d;		/* +0x1d, confirmed real value 2 */
	unsigned char _gap1e[2];	/* +0x1e..+0x1f, not touched by ctor */
	unsigned int  sampleRate;	/* +0x20, confirmed real value 0xbb80 (48000) */
	unsigned int  field24;		/* +0x24, confirmed zeroed */
	unsigned int  field28;		/* +0x28, confirmed zeroed */
	unsigned char _unrecovered_tail[0x38 - 0x2c]; /* +0x2c..+0x38, confirmed to exist, not reconstructed */
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
/* CSTGAudioEvent's own real vtable symbol, same treatment (40 confirmed
 * bytes, per readelf, matching _ZTV15CSTGRecordEvent's own size). */
extern "C" unsigned char _ZTV14CSTGAudioEvent[];

/*
 * CSTGRecordBuffer -- CORRECTS a real, previously-undetected bug in this
 * project's own earlier reconstruction (sec 10.148): this struct and the
 * `BuildArrayManager(..., 96, 0x38, 0x0, ConstructRecordBuffer)` call in
 * engine_init.cpp both claimed a 56-byte (0x38) per-instance size before
 * `CSTGRecordBuffer::CSTGRecordBuffer()` itself had ever been
 * disassembled (it was a deliberately-deferred empty stub until then).
 * Directly disassembling the real ctor (`.text+0xd6dc0`, 21 bytes: `mov
 * dword ptr [this+0x3004], 0` / `mov dword ptr [this+0x3008], 0`, no
 * relocations) proves the object is at least 0x300c bytes -- and the real
 * `CSTGEngine::Initialize()` call site confirms the exact real allocation
 * is `CSTGBankMemory::AllocAligned(0x301c, 0x10)` per instance (a literal
 * `mov eax, 0x301c` immediately before the ctor call, not 0x38). The old
 * 0x38 stride would have made every one of the 96 real ctor calls write
 * ~12KB past the end of its own tiny allocation, corrupting whatever
 * CSTGBankMemory carved out next -- masked until now purely because the
 * ctor itself was an empty stub that never actually performed the writes.
 * Layout: a confirmed-real 0x3004-byte leading buffer (contents/purpose
 * NOT recovered -- presumably raw recorded-sample storage, never written
 * by this ctor), two ctor-zeroed dwords at +0x3004/+0x3008, and 0x10
 * trailing bytes (+0x300c..+0x301c) the ctor also never touches.
 */
struct CSTGRecordBuffer {
	CSTGRecordBuffer();
	unsigned char _unrecovered[0x3004];	/* +0x0, raw buffer, never touched by the ctor */
	unsigned int field3004;			/* +0x3004, confirmed real, ctor-zeroed */
	unsigned int field3008;			/* +0x3008, confirmed real, ctor-zeroed */
	unsigned char _tail[0x10];		/* +0x300c, confirmed real, never touched by the ctor */
};
#define CSTGRECORDBUFFER_SIZE 0x301c

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
