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
 *
 * UPDATE (sec 10.227): this class (and its three derived siblings) is
 * genuinely C++-polymorphic, not a manual "raw byte array + hand-written
 * vtable pointer" placeholder -- confirmed via `objdump -r` on
 * OA_real.ko's own `.rodata._ZTV16CSTGVectorEGBase`/
 * `.rodata._ZTV{14CSTGVectorEGCC,17CSTGVectorEGXOnly,14CSTGVectorEGXY}`:
 * all four are EXACTLY 12 bytes (offset-to-top + a null RTTI slot, this
 * build's confirmed `-fno-rtti`, + ONE real vtable slot at +0x8), each
 * with a single real relocation pointing at that class's own `Init()`
 * method (`_ZN16CSTGVectorEGBase4InitEv`/`_ZN14CSTGVectorEGCC4InitEv`/
 * `_ZN17CSTGVectorEGXOnly4InitEv`/`_ZN14CSTGVectorEGXY4InitEv`) -- i.e.
 * this project's own earlier placeholder (`unsigned char _ZTVxxx[12]`,
 * bar2_stubs.cpp) already guessed the real SIZE correctly but left the
 * one real slot all-zero. `CSTGVectorManager::Initialize()`'s own
 * confirmed slot-0 dispatch (sec 10.65) reads that zero and calls
 * through NULL on the very first object it touches -- EIP=0/CR2=0,
 * exactly the live boot crash this update fixes (MASTER_REFERENCE.md
 * sec 10.226/10.227). Fixed the same way as `CSTGAudioDriverInterface`
 * (sec 10.225): a real `virtual void Init()` declared here and in each
 * derived class below, so the compiler emits the real vtable and
 * populates slot 0 itself -- the derived classes' own manual
 * `*(unsigned char**)p = _ZTVxxx + 8` vtable-pointer writes are removed
 * accordingly (vector_eg_ctors.cpp), C++ does that automatically now.
 */
struct CSTGVectorEGBase {
	CSTGVectorEGBase();
	/*
	 * Init() (.text+0x7f810, 5 bytes) confirmed real: called directly
	 * (non-virtually -- i.e. the derived overrides below call
	 * `CSTGVectorEGBase::Init()` by qualified name, not through the
	 * vtable) as the very first statement of each derived override,
	 * matching the confirmed disassembly (`call
	 * _ZN16CSTGVectorEGBase4InitEv`). Real body: resets one flag byte at
	 * +0xf to 0 -- the SAME byte the constructor above already sets to
	 * 0 (an idempotent re-prime), not independently understood beyond
	 * its confirmed effect.
	 */
	virtual void Init();
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
 * also has a real vtable, now genuinely C++-polymorphic (sec 10.227,
 * see CSTGVectorEGBase's own header comment above for the full
 * `objdump -r` ground-truthing). Sizes are the confirmed real
 * per-instance stride from CSTGVectorManager's own constructor
 * (0x88/0x7c/0x70 bytes).
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
 * arithmetic in the .cpp instead. `_unrecovered` is sized `stride - 4`,
 * not the full confirmed stride: the compiler-managed vtable pointer
 * now occupies the object's own leading slot (4 bytes on the confirmed
 * 32-bit target, 8 on a 64-bit host build) -- same host/target ABI
 * convention already established for `CSTGAudioDriverInterfaceKorgUsb`
 * (oa_engine.h, sec 10.225). The raw-offset arithmetic in the .cpp
 * counts from `this` (i.e. target-relative offsets unchanged either
 * way); `CSTGVectorManager`'s own constructor/`Initialize()` place
 * these objects at their real LITERAL byte strides (0x88/0x7c/0x70),
 * never via `sizeof()`, so a host build's slightly larger `sizeof()`
 * doesn't affect correctness there either (each object's own
 * construction fully re-initializes its own memory, self-healing any
 * transient host-only overlap into the next slot).
 */
struct CSTGVectorEGXOnly : public CSTGVectorEGBase {
	CSTGVectorEGXOnly();
	/*
	 * Init() (.text+0x7ece0, 379 bytes) confirmed real. Reachable-at-
	 * boot portion (this project's own KAT covers exactly this): base
	 * Init(); clears +0x80; zeroes +0x5c/+0x58/+0x54/+0x50; mirrors
	 * CSTGGlobal::sInstance's own confirmed "mode" field (+0x684,
	 * already load-bearing elsewhere -- global.cpp) into this object's
	 * own +0x4c. The remaining ~300 bytes are two confirmed real
	 * intrusive-pool-removal loops (gated on +0x60 / +0x6c being
	 * non-NULL) that touch an unidentified external pool-manager
	 * object's own +0xb0/+0xb8 fields via each list node's own +0x8 --
	 * both gates are PROVABLY false the very first time Init() can ever
	 * run (called exactly once per object, immediately after that same
	 * object's own constructor placement-new'd it with +0x60/+0x6c
	 * freshly zeroed, before anything else could link it into either
	 * pool) -- a confirmed-real-but-here-unreachable deferral, not a
	 * guess, left undone rather than modeling an unidentified external
	 * type.
	 */
	virtual void Init();
	/* Confirmed real (`_ZN17CSTGVectorEGXOnly6sMutexE`) -- same
	 * "address of the singleton pointer" idiom already confirmed
	 * elsewhere (e.g. CSTGWaveSeqGenerator::sMutex, sec 10.62). Set by
	 * CSTGVectorManager::Initialize() (sec 10.65) to
	 * `&manager->+0x1c9e0`, the second of the three mutexes the
	 * constructor allocates. */
	static void **sMutex;
	unsigned char _unrecovered[0x88 - 4];
};
struct CSTGVectorEGXY : public CSTGVectorEGBase {
	CSTGVectorEGXY();
	/*
	 * Init() (.text+0x7de90, 211 bytes) confirmed real. Reachable-at-
	 * boot portion: base Init(); the SAME `+0x6e &= 0xfd` bit-clear the
	 * constructor already does (idempotent); zeroes +0x54/+0x58/+0x50/
	 * +0x4c. Remaining ~150 bytes: one confirmed real intrusive-pool-
	 * removal loop (gated on +0x5c non-NULL), same external-pool-object
	 * shape and same "provably unreachable on this exact call" argument
	 * as CSTGVectorEGXOnly::Init() above -- deferred for the same
	 * reason.
	 */
	virtual void Init();
	/* Confirmed real (`_ZN14CSTGVectorEGXY6sMutexE`) -- set by
	 * CSTGVectorManager::Initialize() (sec 10.65) to
	 * `&manager->+0x1c9e4`, the third of the three constructor-
	 * allocated mutexes. */
	static void **sMutex;
	unsigned char _unrecovered[0x7c - 4];
};
struct CSTGVectorEGCC : public CSTGVectorEGBase {
	CSTGVectorEGCC();
	/*
	 * Init() (.text+0x7bb10, 69 bytes) confirmed real and FULLY
	 * self-contained (no deferred portion, unlike its two siblings):
	 * base Init(); always zeroes +0x4c; if this object's own +0x4
	 * index is NOT 16 (i.e. every EGCC except the last of each 17-
	 * element batch), also resets the four STGVJSAssignInfo pointer
	 * fields (+0x54/+0x58/+0x5c/+0x60, the ctor's own default) back to
	 * a literal 0.
	 */
	virtual void Init();
	unsigned char _unrecovered[0x70 - 4];
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

	/*
	 * Initialize(unsigned int format, unsigned int size) (sec 10.230/
	 * MASTER_REFERENCE, `.text+0x3ffe0` in OA_real.ko, mangled
	 * `_ZN13CSTGMidiQueue10InitializeENS_7eFormatEj` -- i.e. the real
	 * signature's first parameter is a nested `CSTGMidiQueue::eFormat`
	 * enum, modeled here as a plain `unsigned int` since only two
	 * concrete values (0 and 1) are ever observed at real call sites and
	 * neither's semantic meaning is independently determined) confirmed
	 * via full disassembly: calls the real `SetDesc()` below with a
	 * fixed label string (caller-supplied, see midi_port_manager.cpp),
	 * then `buf = CSTGHeapManager::sInstance->Alloc(size)` (the REAL
	 * instance method, `_ZN15CSTGHeapManager5AllocEm`, oa_heapmanager.h
	 * -- confirmed via relocation, NOT the "raw-offset static" `Alloc
	 * (unsigned int)` stand-in used elsewhere in this project), storing
	 * the returned HANDLE (not a raw pointer -- same small-integer-slot
	 * convention as every other `CSTGHeapManager::Alloc()` call site in
	 * this codebase) into `+0x0`. Fields, all confirmed via disassembly:
	 *   +0x0  allocHandle -- CSTGHeapManager::Alloc()'s return value
	 *   +0x4  format -- the raw `format` parameter, stored verbatim
	 *   +0x8  mask -- `size - 1` (capacity mask, matches Write()'s own
	 *         confirmed `(mask+1)-backlog` formula)
	 *   +0xc  writeCursor -- 0
	 *   +0x10..+0x1c  4 reader cursors -- 0
	 *   +0x20  active reader count -- 0 (confirmed real, matches
	 *         AllocReader()'s own `lock xadd` target)
	 *   +0x21..+0x60  64-byte SetDesc() label buffer
	 * This is the object CSTGMidiPortManager::Initialize() embeds 5 of
	 * (at +0xc/+0x70/+0xd4/+0x140/+0x1a4) -- the ringCtl NULL-pointer
	 * crash traced there (sec 10.230) is this project's own
	 * CSTGMidiPortManager::Initialize() never having called this method
	 * for real, not anything wrong with this class itself.
	 */
	void Initialize(unsigned int format, unsigned int size);

	/*
	 * SetDesc(const char *fmt, ...) (`.text+0x3ffb0`,
	 * `_ZN13CSTGMidiQueue7SetDescEPKcz` -- confirmed real variadic
	 * member, matching this class's real regparm(3)+stack-varargs
	 * calling convention) confirmed via disassembly: a plain
	 * `vsnprintf(this+0x21, 0x40, fmt, args)` -- writes a cosmetic
	 * debug/label string into the 64-byte buffer right after this
	 * object's own confirmed fields. Every real call site in
	 * CSTGMidiPortManager::Initialize() passes a literal label string
	 * with zero variadic arguments (confirmed: "STG MIDI Out"/"KG
	 * Regular MIDI Out"/"KG Real Time MIDI Out"/"STG->KG"/"KG->STG",
	 * extracted directly from `.rodata.str1.1`), so the `...` machinery
	 * is exercised here only for signature fidelity, not because any
	 * real caller needs it.
	 */
	void SetDesc(const char *fmt, ...);
};

/*
 * CSTGMidiOutPort -- base field layout originally ported from
 * KronosScreenRemoteDaemon/midi_module/midi_bridge.c's own independently
 * ground-truthed real-hardware findings (that daemon taps these exact
 * fields live via raw offset arithmetic on a real Kronos, no guessing):
 * see midi_bridge.c's QUEUE_PTR_OFF/QUEUE_BUF_OFF tables and its own
 * "Port layout from CSTGMidiOutPort::Activate" comment, which names this
 * class's own real method -- reused verbatim below as `Activate()`.
 *
 * UPDATED (OA.ko MIDI-OUT hardware batch, candidate 1): `Activate()`'s
 * REAL signature/body is now independently confirmed via full
 * `objdump -dr` against OA.ko_Decomp/OA.ko (`.text+0xf7f10`, 365 bytes,
 * `_ZN15CSTGMidiOutPort8ActivateEP13CSTGMidiQueue`) -- the field LAYOUT
 * midi_bridge.c ground-truthed was already exactly right, but the
 * earlier stand-in body/signature here (`Activate(int qslot,
 * CSTGMidiQueue*, unsigned char*)`) was a guess and WRONG: the real
 * method takes exactly ONE argument (the q3/"per-port" queue) and
 * unconditionally wires up all 4 slots itself every call. See the real
 * body's own citation below and midi_out_port_serial.cpp's header
 * comment for the full disassembly-confirmed derivation (relocations,
 * vtable slot numbering, the `CSTGMidiQueueMessageReader` embedding,
 * and the two `.rodata.cst4` float constants this file's lazy-init
 * caches use).
 *
 * Also newly confirmed real (same batch): `Deactivate()`, `BumpTimers()`
 * (a real, non-pure base-class body -- decrements the countdown timer at
 * +0x3c), `ProcessNormal()`, `ProcessNKS4TestMode()`,
 * `GenerateActiveSensing()`, `ProcessRealTimeMessage()`,
 * `ReadNextMessage()`, and the 2-argument constructor. Real vtable
 * (`_ZTV15CSTGMidiOutPort`, confirmed via `.rel.rodata` relocations)
 * has 9 slots, byte offsets relative to the IN-OBJECT vtable pointer
 * (which itself already points past the 2-word RTTI header):
 *   +0x00 dtor (still __cxa_pure_virtual -- this class is abstract)
 *   +0x04 Activate(CSTGMidiQueue*)      -- REAL (see above)
 *   +0x08 Deactivate()                  -- REAL
 *   +0x0c BumpTimers()                  -- REAL
 *   +0x10 CanSendRealTime() const       -- pure in this class
 *   +0x14 CanSendRegular() const        -- pure
 *   +0x18 ProcessRegularMessage()       -- pure
 *   +0x1c SendRealTime(unsigned char)   -- pure
 *   +0x20 SendSingleByte(unsigned char) -- pure
 * `CSTGMidiOutPortSerial` (below) overrides everything except
 * `Deactivate()`, and adds 2 MORE trailing slots (+0x24/+0x28) that are
 * STILL `__cxa_pure_virtual` even in Serial's own vtable -- see that
 * class's own comment.
 *
 * Each instance holds up to 4 queue slots. Confirmed real offsets:
 *   +0x00  vtable ptr (CallVtableSlot-style dispatch, see
 *          midi_port_manager.cpp's PortQuery()/PortRegister() -- slot 0
 *          bool query, slot 1 hands the port a CSTGMidiQueue region;
 *          that dispatch is this class's own vtable, confirmed real but
 *          independently not fully mapped beyond those 2 slots)
 *   +0x04  portIndex (signed byte) -- this port's own slot number in
 *          CSTGMidiPortManager::sMidiOutPorts[4], read by
 *          RegisterMidiOutPort()'s `movsx edx,[eax+4]` (oa_engine.h).
 *          Exact write site not independently confirmed (plausibly set
 *          by CKorgUsbAudioDriverMidiPorts's static init alongside
 *          constructing the 2 real out-ports -- not reconstructed here).
 *   +0x08/+0x0c  q0: CSTGMidiQueue* / resolved data-buffer pointer
 *                (active-sensing/realtime -- midi_bridge.c: "not
 *                interesting", excluded from its own capture on
 *                purpose). Sourced from the embedded CSTGMidiQueue at
 *                `CSTGMidiPortManager::sInstance + 0xd4`.
 *   +0x14/+0x18  q1: CSTGMidiQueue* / resolved data-buffer pointer
 *                (SHARED across every out-port -- identical pointer
 *                value in all instances). Sourced from
 *                `CSTGMidiPortManager::sInstance + 0x0c`.
 *   +0x20/+0x24  q2: CSTGMidiQueue* / resolved data-buffer pointer
 *                (SHARED; carries live notes/CC/program-change/combi
 *                SysEx per midi_bridge.c). Sourced from
 *                `CSTGMidiPortManager::sInstance + 0x70`.
 *   +0x2c/+0x30  q3: CSTGMidiQueue* / resolved data-buffer pointer
 *                (per-port bulk-dump queue -- where a data-dump reply
 *                routes, USB vs DIN). THE ONLY ONE OF THE 4 THAT IS
 *                CALLER-SUPPLIED: `Activate()`'s own one argument.
 *   +0x10/+0x1c/+0x28/+0x34  one reader-index byte per slot, immediately
 *                after that slot's own buffer pointer -- THIS port's own
 *                reader slot on that queue (the value AllocReader()
 *                returned when the port itself became a reader in order
 *                to transmit from it), NOT a tap module's reader (a
 *                third party like midi_bridge.c claims its OWN separate
 *                index via its own AllocReader() call and stores it in
 *                its own private state, never writing here).
 *   +0x1d/+0x29/+0x35  CONFIRMED (this batch, `ReadNextMessage()`/
 *                `CSTGMidiOutPortSerial::ProcessRegularMessage()`/
 *                `RefillMsgBuffer()` all read it) -- NOT independent
 *                padding as previously modeled. The q1/q2/q3 slots
 *                (Queue* / Buf* / ReaderIdx, 9 bytes, +0x14/+0x20/+0x2c
 *                respectively) are laid out BYTE-IDENTICALLY to
 *                `CSTGMidiQueueMessageReader` (ringCtl/buf/readerIdx/
 *                inSysEx, oa_engine_init.h below) and are literally
 *                reinterpreted in place as one whenever
 *                `CSTGMidiQueueMessageReader::ReadMessage()` is called
 *                on them -- this byte IS that embedded reader's own
 *                `inSysEx` flag, not a separate field. q0's own pad
 *                (+0x11..+0x13) is never touched this way: q0 is read
 *                via raw ring-cursor peek/advance in
 *                `ProcessRealTimeMessage()`/`ProcessNormal()`/
 *                `GenerateActiveSensing()` instead, never through
 *                `CSTGMidiQueueMessageReader`.
 *   +0x05  flags -- CONFIRMED (not merely inferred) by
 *          `CSTGMidiPortManager::~CSTGMidiPortManager()`'s own real
 *          disassembly (.text+0xf5280, 264 bytes, src/engine/
 *          midi_port_manager.cpp): `testb $0x2,0x5(%eax)` gates whether
 *          this out-port's vtable slot 2 (presumably its own virtual
 *          destructor) gets called during manager teardown -- bit1 =
 *          "active"/live, set by `Activate()`. Bit0 CONFIRMED this
 *          batch: initialized from the low bit of the constructor's
 *          2nd parameter (`flags = (flags & 0xfe) | (ctorParam2 & 1)`,
 *          real disassembly of the ctor, `.text+0xf8270`). The other 6
 *          bits' meaning is still not independently determined.
 *   +0x38  roundRobinIdx (byte, CONFIRMED this batch) -- cycles 0/1/2
 *          selecting which of q1/q2/q3's embedded
 *          `CSTGMidiQueueMessageReader` to poll next
 *          (`ReadNextMessage()`/Serial's `ProcessRegularMessage()`/
 *          `RefillMsgBuffer()` all share this exact field+algorithm).
 *   +0x3c  activeSensingTimer (int, CONFIRMED this batch) -- decremented
 *          by `BumpTimers()` while nonzero; reloaded to
 *          `sActiveSensingTransmitPeriodTicks` whenever ANY byte is
 *          actually transmitted (`ProcessNormal()`) or when a due
 *          Active Sensing byte (0xFE) is sent (`GenerateActiveSensing()`).
 * CSTGMidiOutPort's own real size is exactly 0x40 bytes (confirmed:
 * `CSTGMidiOutPortSerial`'s own new fields start immediately at +0x40,
 * no gap) -- previously modeled as ending at 0x38.
 */
struct CSTGMidiOutPort {
	/*
	 * All pointer-shaped fields below are PACKED 32-bit
	 * (`unsigned int`), not native C++ pointer types -- matching this
	 * project's established `ToU32()`/`FromU32()` convention (see
	 * CSTGMidiQueueWriter, oa_global.h) for any field whose offset was
	 * derived from 32-bit target disassembly. Using native
	 * `CSTGMidiQueue*`/`unsigned char*` members here was tried first and
	 * reverted: on a 64-bit host KAT build those are 8 bytes, not 4,
	 * silently doubling every field's real size and shifting all
	 * offsets from +0x38 onward -- caught via a live KAT segfault
	 * reading garbage through a q2/q3 slot reinterpreted as a
	 * `CSTGMidiQueueMessageReader`. The kernel target itself is -m32,
	 * where this distinction doesn't exist; packing keeps the struct's
	 * `sizeof`/offsets IDENTICAL on both.
	 */
	unsigned int vtable;          /* +0x00 */
	signed char portIndex;        /* +0x04 -- this port's own sMidiOutPorts[] slot */
	unsigned char flags;          /* +0x05 -- bit1 = active/live, bit0 = ctor param2&1, see class comment above */
	unsigned char _pad6[2];       /* +0x06..+0x07, natural alignment, not independently confirmed */

	unsigned int q0Queue;  unsigned int q0Buf;  unsigned char q0ReaderIdx; unsigned char _pad11[3];
	unsigned int q1Queue;  unsigned int q1Buf;  unsigned char q1ReaderIdx; unsigned char q1InSysEx; unsigned char _pad1e[2];
	unsigned int q2Queue;  unsigned int q2Buf;  unsigned char q2ReaderIdx; unsigned char q2InSysEx; unsigned char _pad2a[2];
	unsigned int q3Queue;  unsigned int q3Buf;  unsigned char q3ReaderIdx; unsigned char q3InSysEx; unsigned char _pad36[2];

	unsigned char roundRobinIdx;  /* +0x38 -- cycles 0/1/2 over q1/q2/q3 */
	unsigned char _pad39[3];
	int activeSensingTimer;       /* +0x3c */

	/* sActiveSensingTransmitPeriodTicks -- confirmed real static
	 * (`_ZN15CSTGMidiOutPort31sActiveSensingTransmitPeriodTicksE`,
	 * BSS, .bss+0x9ef6c). Lazily computed on the first `Activate()`
	 * call as `(int)(0.25f * CSTGAudioBusManager::sInstance->
	 * busGainScale)` -- with the confirmed real `busGainScale`=1500.0f
	 * this is exactly 375 ticks (0.25s of Active-Sensing period at a
	 * 1500Hz tick rate). Shared by every CSTGMidiOutPort instance. */
	static int sActiveSensingTransmitPeriodTicks;

	/*
	 * CSTGMidiOutPort(eSTGMidiPort portType, unsigned int flagsInit) --
	 * CONFIRMED real (`.text+0xf8270`, 95 bytes, regparm(3): this=EAX,
	 * portType=EDX, flagsInit=ECX). Sets the base (still mostly pure)
	 * vtable, `portIndex = (unsigned char)portType`, `flags` bit0 from
	 * `flagsInit & 1`, zeroes all 4 queue-slot Queue* / Buf* pointers
	 * (NOT the reader-index/inSysEx bytes, left uninitialized until
	 * `Activate()`), then calls the already-real
	 * `CSTGMidiPortManager::RegisterMidiOutPort(this)`.
	 */
	CSTGMidiOutPort(int portType, unsigned int flagsInit);

	/*
	 * Activate(CSTGMidiQueue *q3) -- CONFIRMED real
	 * (`_ZN15CSTGMidiOutPort8ActivateEP13CSTGMidiQueue`, `.text+0xf7f10`,
	 * 365 bytes). Takes exactly ONE argument (the per-port "bulk-dump"
	 * queue, stored as q3) and unconditionally wires up ALL 4 slots:
	 * q0/q1/q2 always resolve to the 3 SAME `CSTGMidiPortManager`-
	 * embedded `CSTGMidiQueue` objects (+0xd4/+0x0c/+0x70 respectively);
	 * q3 is the caller-supplied argument. For each slot: store the
	 * `CSTGMidiQueue*`, resolve its data buffer (read the queue's own
	 * `allocHandle` at its +0x0, translate via `CSTGHeapManager`'s
	 * handle table -- see midi_out_port_serial.cpp's `resolve_heap_
	 * handle()`, the exact formula this real body uses), and claim this
	 * port's own reader slot via `CSTGMidiQueue::AllocReader()`. q1/q2/
	 * q3 additionally zero their `inSysEx` byte (q0 does not -- it has
	 * no embedded reader). Finally zeroes `roundRobinIdx`/
	 * `activeSensingTimer` and sets `flags` bit1 (active).
	 */
	void Activate(CSTGMidiQueue *q3);

	/* Deactivate() -- CONFIRMED real (`.text+0xf7d70`, 5 bytes): clears
	 * `flags` bit1. */
	void Deactivate();

	/* BumpTimers() -- CONFIRMED real, non-pure base body
	 * (`.text+0x5a6880`, comdat/weak, 14 bytes): decrements
	 * `activeSensingTimer` by 1 while nonzero. `CSTGMidiOutPortSerial`
	 * overrides this to ALSO decrement its own `runningStatusTimer`. */
	void BumpTimers();

	/*
	 * ReadNextMessage(unsigned char*, unsigned int) -- CONFIRMED real
	 * member of THIS class (`_ZN15CSTGMidiOutPort15ReadNextMessageEPhj`,
	 * `.text+0xf84a0`, 191 bytes) -- hosted here (unlike the 5 methods
	 * below) because it does NOT call any of the still-pure vtable
	 * slots, only `CSTGMidiQueueMessageReader::ReadMessage()` (a
	 * different class entirely), so there is no non-virtual-dispatch
	 * hazard in declaring it on the base. Round-robins q1/q2/q3 up to 3
	 * times (or polls the current slot once, unadvanced, mid-SysEx) via
	 * the shared `PollNextRegularMessage()` helper (midi_out_port_
	 * serial.cpp) and RETURNS whatever `ReadMessage()` returned (0 if no
	 * slot had data, else the message length copied into `buf`) --
	 * confirmed via raw disassembly that `eax` is NOT clobbered between
	 * the last `ReadMessage()` call and this function's own `ret` on
	 * every path. CORRECTED (candidate-3/KorgUsb batch): previously
	 * modeled as `void`, declared on `CSTGMidiOutPortSerial`, and
	 * discarding this return value -- true for every CALLER within
	 * `CSTGMidiOutPortSerial` itself (which never calls this exact
	 * function, only its own separate `ProcessRegularMessage()`
	 * override), but WRONG once `CSTGMidiOutPortKorgUsb::
	 * ProcessRegularMessage()` (midi_korgusb_port.cpp) is confirmed via
	 * its own disassembly to call this SAME base-class function and use
	 * the returned length to know how many bytes to copy into its own
	 * ring buffer.
	 */
	unsigned int ReadNextMessage(unsigned char *buf, unsigned int bufLen);

	/*
	 * NOTE on ProcessNormal()/ProcessNKS4TestMode()/GenerateActiveSensing()/
	 * ProcessRealTimeMessage(): all 4 are CONFIRMED
	 * real members of THIS class in the actual binary (vtable slots
	 * 4-8, dispatched through the real vtable at runtime). They are
	 * declared on `CSTGMidiOutPortSerial` below instead of here,
	 * deliberately: every one of them internally calls
	 * `CanSendRealTime()`/`CanSendRegular()`/`ProcessRegularMessage()`/
	 * `SendRealTime()`/`SendSingleByte()` -- 5 more real vtable slots
	 * that are still pure `__cxa_pure_virtual` in THIS class and only
	 * become concrete in `CSTGMidiOutPortSerial`. Reproducing that with
	 * genuine C++ `virtual` would insert a compiler-generated vtable
	 * pointer that does NOT line up with the single explicit `vtable`
	 * field this struct already models by hand (a real, confirmed
	 * mismatch hit and fixed during this reconstruction: every field
	 * offset below `+0x40` shifted by 4 bytes and corrupted the q1/q2/q3
	 * embedded-`CSTGMidiQueueMessageReader` reinterpretation). Since
	 * `CSTGMidiOutPortSerial` is the only concrete class this project
	 * models at all (matching every other non-virtual class in this
	 * codebase), hosting these 5 methods there and calling the other 5
	 * as ordinary (non-virtual) same-class member functions produces
	 * IDENTICAL observable behavior with zero ABI risk.
	 */

	/* Pure vtable hooks 4-8 in the real binary (`CanSendRealTime()`/
	 * `CanSendRegular()`/`ProcessRegularMessage()`/`SendRealTime()`/
	 * `SendSingleByte()`) are likewise declared+defined only on
	 * `CSTGMidiOutPortSerial` below, for the same reason. */
};

/*
 * CSTGMidiOutPortUSB -- the generic-USB-MIDI-class accessory transmit
 * side (own class, own vtable, sibling of `CSTGMidiInPortUSB`/
 * `CSTGUSBMidiAccessoryMidiInPort`, oa_engine.h). CONFIRMED real via
 * `.rel.rodata._ZTV18CSTGMidiOutPortUSB` (11 slots, same base shape as
 * `CSTGMidiOutPortSerial`): dtor still pure; `ProcessRegularMessage()`
 * (`.text+0xf7d80`, 394 bytes) IS a real, defined, non-pure override
 * (own `kSysComCodeTable` rodata table) but deliberately deferred --
 * disproportionate on its own, unrelated to this batch; `CanSendRealTime()
 * const`/`CanSendRegular() const`/`SendRealTime(uchar)`/`SendSingleByte
 * (uchar)` are ALL still `__cxa_pure_virtual` in THIS class's own vtable
 * (confirmed via `readelf -r`, offsets +0x18/+0x1c/+0x24/+0x28 relative
 * to slot 0) -- a genuinely UNRESOLVED dead end in ground truth itself
 * (no further-derived concrete class anywhere in OA.ko provides them),
 * deliberately NOT declared here, matching this project's "confirmed
 * real dead end, not a gap in this reconstruction" convention (same as
 * `CSTGMidiOutPortSerial::CanTransmitHardware()`/`TransmitHardwareByte()`
 * above).
 *
 * `Activate(CSTGMidiQueue*)` (`.text+0xf7d70`... own weak comdat, 15
 * bytes, `_ZN18CSTGMidiOutPortUSB8ActivateEP13CSTGMidiQueue`) IS
 * reconstructed here: CONFIRMED real, a trivial forward to the base
 * `CSTGMidiOutPort::Activate(CSTGMidiQueue*)` with no other work (no
 * new fields touched, no companion-module call unlike the InPort-side
 * sibling `CSTGUSBMidiAccessoryMidiInPort::Activate()`). Adds no new
 * fields of its own in this reconstruction (the real class likely has
 * additional state for the still-deferred `ProcessRegularMessage()`,
 * not independently determined since that method is out of scope here).
 */
class CSTGMidiOutPortUSB : public CSTGMidiOutPort {
public:
	void Activate(CSTGMidiQueue *q3);
};

/*
 * CSTGMidiOutPortSerial -- the physical (5-pin DIN) hardware MIDI-OUT
 * UART port: the direct output-side counterpart to
 * `CSTGMidiInPortSerial` (midi_in_port_serial.cpp). CONFIRMED real via
 * `.rel.rodata._ZTV21CSTGMidiOutPortSerial` (11 slots: the base's own 9
 * at identical offsets, PLUS 2 trailing slots at +0x24/+0x28 that are
 * STILL `__cxa_pure_virtual` even here -- i.e. this class remains
 * abstract; some more-derived hardware-backend class not present
 * anywhere in this .ko provides the actual UART transmit primitives).
 * `CanSendRealTime()`/`CanSendRegular()` (both real, both 15 bytes, BYTE-
 * IDENTICAL bodies) simply forward to vtable slot +0x24; `SendRealTime()`/
 * `SendSingleByte()` (both real, both 18 bytes, byte-identical) forward
 * to vtable slot +0x28 with the byte in EDX -- i.e. Serial itself does
 * not distinguish realtime vs. regular transmit priority at the
 * hardware level, it has exactly ONE "can transmit"/"transmit byte"
 * pair underneath, shared for both. Modeled here as 2 unresolved pure-
 * virtual hooks (`CanTransmitHardware()`/`TransmitHardwareByte()`) that
 * this reconstruction does NOT implement, matching the real binary
 * exactly: if ever actually invoked on a real `CSTGMidiOutPortSerial`
 * object (rather than some further-derived concrete class), real
 * hardware would hit `__cxa_pure_virtual` too. New fields (all
 * CONFIRMED via `Activate()`/`ProcessRegularMessage()`/
 * `RefillMsgBuffer()` disassembly, `.text+0xf8080`/`0xf80e0`/`0xf8570`):
 *   +0x40..+0x42  3-byte decoded regular-message buffer (status,data1,
 *                 data2) -- `ReadMessage()`'s own `buf` argument, always
 *                 called with `bufLen=3`.
 *   +0x43  msgLen -- the length `ReadMessage()` last returned (0..3).
 *   +0x44  state (byte) -- gates whether `ProcessRegularMessage()` needs
 *          to pull a NEW message this call or is still delivering a
 *          previously-decoded one via running-status compression (0/1/2
 *          observed values; exact state-machine semantics not fully
 *          named, translated literally from the real disassembly).
 *   +0x45  lastStatus (byte) -- the most recent channel-message status
 *          byte sent, for running-status comparison.
 *   +0x48  runningStatusTimer (int) -- decremented by this class's own
 *          `BumpTimers()` override; reloaded to `sRunningStatusTimeoutTicks`
 *          whenever a status byte is (re)confirmed.
 * `sRunningStatusTimeoutTicks` (confirmed real static,
 * `_ZN21CSTGMidiOutPortSerial25sRunningStatusTimeoutTicksE`, .bss+0x9ef70):
 * lazily computed on the first `Activate()` call as
 * `(int)(0.05f * CSTGAudioBusManager::sInstance->busGainScale)` = 75
 * ticks (50ms at 1500Hz) with the confirmed real `busGainScale`=1500.0f.
 */
class CSTGMidiOutPortSerial : public CSTGMidiOutPort {
public:
	unsigned char msgBuf[3];      /* +0x40..+0x42 */
	unsigned char msgLen;         /* +0x43 */
	unsigned char state;          /* +0x44 */
	unsigned char lastStatus;     /* +0x45 */
	unsigned char _pad46[2];
	int runningStatusTimer;       /* +0x48 */

	static int sRunningStatusTimeoutTicks;

	CSTGMidiOutPortSerial(int portType, unsigned int flagsInit)
		: CSTGMidiOutPort(portType, flagsInit) {}

	/*
	 * Activate(CSTGMidiQueue*) override -- CONFIRMED real
	 * (`.text+0xf8080`, 83 bytes): calls the base `Activate()` first,
	 * lazily initializes `sRunningStatusTimeoutTicks`, then zeroes
	 * `msgLen`/`state`/`lastStatus`/`runningStatusTimer`.
	 */
	void Activate(CSTGMidiQueue *q3);

	/*
	 * ProcessRegularMessage() override -- CONFIRMED real
	 * (`.text+0xf80e0`, 378 bytes, full `objdump -dr` cross-checked
	 * against the Ghidra decompile). See midi_out_port_serial.cpp's
	 * own header comment for the full state-machine derivation
	 * (running-status compression: when the same channel-status byte
	 * repeats within `runningStatusTimer`, the status byte is omitted
	 * and a 2-state "already sent status this round" flag is tracked
	 * instead of re-sending it). Returns true iff a byte was actually
	 * transmitted this call (CONFIRMED real: `ProcessNormal()`'s own
	 * raw disassembly tests this return value in AL/DL) -- the earlier
	 * `void` declaration here was wrong.
	 */
	bool ProcessRegularMessage();

	/*
	 * RefillMsgBuffer() -- CONFIRMED real (`.text+0xf8570`, 324 bytes).
	 * The "prepare next message without sending" half of the same
	 * running-status logic `ProcessRegularMessage()` uses -- pulls the
	 * next message via the same round-robin `ReadMessage()` polling,
	 * but only updates `lastStatus`/`runningStatusTimer`/`state`
	 * bookkeeping, never calls a Send hook itself.
	 */
	void RefillMsgBuffer();

	/* BumpTimers() override -- CONFIRMED real (`.text+0x5a68b0`, 27
	 * bytes): decrements BOTH the base's `activeSensingTimer` (+0x3c)
	 * AND this class's own `runningStatusTimer` (+0x48). */
	void BumpTimers();

	/* CanSendRealTime()/CanSendRegular()/SendRealTime()/SendSingleByte()
	 * -- CONFIRMED real (`.text+0x5a6890`/`0x5a68a0`/`0x5a68d0`/
	 * `0x5a68f0`), all thin forwarders to the 2 still-pure "hardware"
	 * slots described in this class's own header comment above
	 * (`CanTransmitHardware()`/`TransmitHardwareByte()` below). */
	bool CanSendRealTime() const;
	bool CanSendRegular() const;
	void SendRealTime(unsigned char byte);
	void SendSingleByte(unsigned char byte);

	/*
	 * The 2 trailing vtable slots (+0x24/+0x28, CONFIRMED via
	 * `.rel.rodata._ZTV21CSTGMidiOutPortSerial` -- both still
	 * `__cxa_pure_virtual` in the real binary's own vtable for THIS
	 * class). No further-derived class providing them exists anywhere
	 * in OA.ko: real hardware would hit `__cxa_pure_virtual` (a kernel
	 * BUG()) if this code path were ever actually reached, meaning this
	 * exact UART-transmit path is genuinely dead/unwired in this
	 * firmware image as shipped. DECLARED ONLY, deliberately no
	 * definition in production code (src/engine/midi_out_port_serial.cpp)
	 * -- matches the real binary's own "reaching this is a kernel BUG()"
	 * semantics as an honest link-time gap rather than invented
	 * behavior. (Not modeled as C++ `virtual`/pure: doing so would
	 * insert a compiler-generated vtable pointer for THIS class's own
	 * portion of the object, which does not exist in the real binary's
	 * layout here and would corrupt the `msgBuf`/`msgLen`/etc offsets
	 * below -- same class of mistake documented on the base class
	 * above.) This project's own host KATs provide their own definition
	 * of these two directly, matching the "test supplies its own
	 * minimal stand-in for an unresolved symbol" convention used
	 * throughout this codebase.
	 */
	bool CanTransmitHardware() const;
	void TransmitHardwareByte(unsigned char byte);

	/*
	 * ProcessNormal()/ProcessNKS4TestMode()/GenerateActiveSensing()/
	 * ProcessRealTimeMessage() -- all 4 CONFIRMED real
	 * members of the BASE class `CSTGMidiOutPort` in the actual binary
	 * (vtable slots 4-8's own dispatch is exercised FROM these methods'
	 * bodies, not the other way around); declared here instead purely
	 * so they can call `CanSendRealTime()`/`CanSendRegular()`/
	 * `ProcessRegularMessage()`/`SendRealTime()`/`SendSingleByte()` as
	 * ordinary non-virtual same-class calls -- see the base class's own
	 * header comment for why. Bodies match the real disassembly exactly
	 * regardless of which C++ class hosts the declaration.
	 * (`ReadNextMessage()` moved to the base `CSTGMidiOutPort` class
	 * itself -- see that class's own comment; it needs none of this
	 * treatment and is inherited here unchanged.)
	 */
	int ProcessNormal();
	void ProcessNKS4TestMode();
	void GenerateActiveSensing();
	bool ProcessRealTimeMessage();
};

/*
 * CSTGMidiQueueReader -- CONFIRMED real (`_ZN19CSTGMidiQueueReader4ReadEPhj`,
 * `.text+0x40100`, 152 bytes), the read-side mirror of the already-real
 * `CSTGMidiQueueWriter` (oa_global.h). Same opaque-struct, raw-offset-
 * in-.cpp convention as that class. Real (unaligned, packed) layout:
 *   +0x0  ringCtl pointer (same shared ringCtl shape as CSTGMidiQueueWriter)
 *   +0x4  data buffer base pointer
 *   +0x8  reader index (byte, 0..3) -- this reader's own slot number,
 *         used as `ringCtl+0x10+(idx)*4` for its cursor (the "+4" bias
 *         seen in the raw disassembly is this same `+0x10` expressed in
 *         dword units)
 * Total 9 bytes, NO padding (confirmed: `CSTGMidiQueueMessageReader`
 * below places its own next field immediately at +9).
 */

/*
 * CSTGMidiOutPortKorgUsb -- the transmit (Korg-USB-audio-composite-
 * interface) half of the KorgUsb MIDI transport (survey candidate 3,
 * midi_korgusb_port.cpp). Full raw `objdump -dr` transcription of the
 * whole cluster: `.text+0x340650` (ctor, 113 bytes) through
 * `.text+0x340850` (`Output()`, 214 bytes), plus the free-function pump
 * `STGMidiOutPortKorgUsb_*` (`.text+0x340300`-`0x340bc8`).
 *
 * CONFIRMED real via `.rel.rodata._ZTV22CSTGMidiOutPortKorgUsb` (9
 * slots, IDENTICAL shape to the base `CSTGMidiOutPort` vtable): dtor
 * still pure; `BumpTimers()` resolves to address 0 in the relocation
 * table, i.e. NOT overridden -- inherits the base's own real 14-byte
 * comdat body unchanged; every other slot (`Activate`/`Deactivate`/
 * `CanSendRealTime`/`CanSendRegular`/`ProcessRegularMessage`/
 * `SendRealTime`/`SendSingleByte`) IS overridden.
 *
 * Unlike `CSTGMidiInPort` (oa_engine.h), the base `CSTGMidiOutPort`
 * class DOES carry an explicit, CONFIRMED-correct `vtable` field and
 * its whole declared field list sums to exactly the documented 0x40
 * bytes with no hidden gap -- so (unlike the InPort side) ordinary C++
 * inheritance + explicitly-sized fields below is safe and used
 * throughout this class. Added fields, all CONFIRMED via the ctor's
 * own `rep stos` zero-fills and the 4 methods that read/write them
 * (`RealtimeInput`/`Input`/`RealtimeOutput`/`Output`/`SendRealTime`/
 * `SendSingleByte`/`ProcessRegularMessage`/`CanSendRealTime`/
 * `CanSendRegular`), two independent 1024-byte ring buffers -- one for
 * realtime bytes (fed by `SendRealTime()`/`RealtimeInput()`, drained by
 * `RealtimeOutput()`), one for regular/running-status-free messages
 * (fed by `SendSingleByte()`/`Input()`/`ProcessRegularMessage()`,
 * drained by `Output()`) -- each with its own write/read cursor pair,
 * wrapping at 0x400 via `cmovae`, mirroring `CanSendRealTime()`/
 * `CanSendRegular()`'s own "free space > 3 bytes" gate (0x400 minus the
 * wrapped write-minus-read distance):
 *   +0x40  korgUsbPortIndex (byte) -- ctor's own 1st param
 *          (`eKorgUsbMidiPort`, 0 or 1 for the pair's two instances);
 *          passed as the `portId` arg to every `KorgUsbRealtimeMidi*`/
 *          `KorgUsbMidi*` companion-module call this class makes.
 *   +0x41..+0x440  realtimeRing[0x400]
 *   +0x444/+0x448  realtimeWriteIdx / realtimeReadIdx
 *   +0x44c..+0x84b  regularRing[0x400]
 *   +0x84c/+0x850  regularWriteIdx / regularReadIdx
 * Total added size 0x814 bytes; combined with the base's own 0x40,
 * `CSTGMidiOutPortKorgUsb`'s real total size is exactly 0x854 bytes --
 * CONFIRMED independently by `CKorgUsbAudioDriverMidiPorts`'s own ctor
 * placing its 2nd pair's embedded instance exactly 0x854 bytes after
 * the 1st (`.text+0x340139`/`0x34018d`, midi_korgusb_port.cpp).
 *
 * `RealtimeOutput()`/`Output()` both drain up to
 * `KorgUsb{Realtime}MidiOutputCanSend(portId)` bytes per call (clamped;
 * if the ring holds MORE than the companion module says it can accept
 * right now, `STGMidiOutPortKorgUsb_ScheduleFromLinux()` is called
 * first to arrange a retry) into a local on-stack staging buffer, then
 * hand that batch to `KorgUsb{Realtime}MidiOutput(portId, buf, count)`
 * in one call -- byte-copy loop with the SAME wraparound idiom as the
 * ring-push methods. Both bodies also contain a real but PROVABLY
 * UNREACHABLE `jle` branch (stale-EFLAGS artifact from an earlier `cmp
 * ..,0` two instructions back, testing a quantity already known
 * positive at that point) that would skip the copy entirely and call
 * the companion output function with an uninitialized buffer pointer --
 * confirmed dead by disassembly, NOT reproduced (this project's
 * established convention: note genuinely-dead compiler artifacts rather
 * than encode unreachable behavior).
 *
 * `ShouldActivate() const` -- same real-but-unreachable `return true;`
 * as the InPort side (own un-merged comdat, zero xrefs).
 */
class CSTGMidiOutPortKorgUsb : public CSTGMidiOutPort {
public:
	unsigned char korgUsbPortIndex;      /* +0x40 */
	unsigned char realtimeRing[0x400];   /* +0x41..+0x440 */
	unsigned char _pad441[3];            /* +0x441..+0x443, alignment */
	unsigned int  realtimeWriteIdx;      /* +0x444 */
	unsigned int  realtimeReadIdx;       /* +0x448 */
	unsigned char regularRing[0x400];    /* +0x44c..+0x84b */
	unsigned int  regularWriteIdx;       /* +0x84c */
	unsigned int  regularReadIdx;        /* +0x850 */

	/* Ctor params: korgUsbPort (`eKorgUsbMidiPort`, 0/1), stgPort
	 * (`eSTGMidiPort`, passed straight through to the base ctor),
	 * flagsInit (base ctor's own 2nd param). Both enums modeled as
	 * plain `int`, matching this project's existing `CSTGMidiOutPort`/
	 * `CSTGMidiInPort` convention of not inventing a fully-named enum
	 * for an unconfirmed value set. */
	CSTGMidiOutPortKorgUsb(int korgUsbPort, int stgPort, unsigned int flagsInit);

	void Activate(CSTGMidiQueue *q3);
	void Deactivate();
	bool CanSendRealTime() const;
	bool CanSendRegular() const;
	void SendRealTime(unsigned char b);
	void SendSingleByte(unsigned char b);
	bool ProcessRegularMessage();

	void RealtimeInput(const unsigned char *data, unsigned int len);
	void Input(const unsigned char *data, unsigned int len);
	void RealtimeOutput();
	void Output();

	bool ShouldActivate() const { return true; }
};

struct CSTGMidiQueueReader {
	void Read(unsigned char *dest, unsigned int count);
};

/*
 * CSTGMidiQueueMessageReader -- CONFIRMED real, genuinely new class
 * (`ReadMessage()` `.text+0x403d0`/377 bytes, `ReadSysEx()`
 * `.text+0x40250`/364 bytes, both `objdump -dr` + Ghidra-decompile
 * cross-checked). Starts with the EXACT SAME 9-byte layout as
 * `CSTGMidiQueueReader` above (ringCtl/buf/readerIdx) -- confirmed by
 * `ReadMessage()`'s own body treating `this` interchangeably as a
 * `CSTGMidiQueueReader*` when it calls `CSTGMidiQueueReader::Read()` --
 * plus one new field:
 *   +0x9  inSysEx (byte) -- set/cleared by `ReadSysEx()` to record
 *         whether a SysEx transfer is still in progress across calls.
 * THIS IS WHY `CSTGMidiOutPort`'s own q1/q2/q3 slots are laid out
 * Queue* / Buf* / ReaderIdx/inSysEx (12-byte stride, 9 real bytes + pad):
 * each slot's 9-byte Queue* / Buf* / ReaderIdx prefix is byte-identical to
 * this class's own layout, and `CSTGMidiOutPort::ReadNextMessage()`/
 * `CSTGMidiOutPortSerial::ProcessRegularMessage()`/`RefillMsgBuffer()`
 * all call `ReadMessage()` with `this` = `outPort + 0x14 +
 * roundRobinIdx*0xc` (CONFIRMED via raw disassembly of
 * `CSTGMidiOutPortSerial::ProcessRegularMessage()`, `lea eax,
 * [ebx+eax*4+0x14]` with `eax`=roundRobinIdx*3 pre-scaled) -- i.e. the
 * port's own q1/q2/q3 slot storage IS reinterpreted in place as a
 * `CSTGMidiQueueMessageReader`, no separate object exists.
 */
struct CSTGMidiQueueMessageReader {
	/*
	 * ReadMessage(unsigned char *buf, unsigned int bufLen) -- returns
	 * the decoded message length (0 = nothing ready). If the ring is
	 * empty, returns 0. If `inSysEx` is set, forwards straight to
	 * `ReadSysEx()`. Otherwise peeks the next ring byte: if it's a
	 * stray data byte or a bare 0xF7, resyncs forward until a real
	 * status byte is found (or the ring runs dry), discarding the skipped
	 * bytes without returning a message. Once a status byte is found:
	 * 0xF0 (SysEx start) dispatches to `ReadSysEx()`; otherwise the
	 * expected length comes from `USTGMidiUtils::kChannelMsgLen[]`/
	 * `kSystemMsgLen[]` (same real tables as
	 * `CSTGMidiInPortSerial`/midi_in_port_serial.cpp) unless the
	 * queue's own `format` field (its +0x4) is 1, in which case every
	 * message is treated as exactly 1 byte -- then copies that many
	 * bytes out via `CSTGMidiQueueReader::Read()`.
	 */
	unsigned int ReadMessage(unsigned char *buf, unsigned int bufLen);

	/*
	 * ReadSysEx(unsigned char *buf, unsigned int bufLen, unsigned int
	 * alreadyWritten) -- scans forward for an 0xF7 (EOX) terminator
	 * within the available ring backlog (clamped to `bufLen`), copies
	 * the scanned span via `CSTGMidiQueueReader::Read()`, and (if EOX
	 * was found within the copied span) appends a synthesized trailing
	 * 0xF7 if the copy didn't already end with one, clearing `inSysEx`.
	 * If no EOX was found within the available/allowed span, sets
	 * `inSysEx=1` and returns the partial byte count (SysEx continues
	 * on the next call). Two real callers within `ReadMessage()`.
	 */
	unsigned int ReadSysEx(unsigned char *buf, unsigned int bufLen, unsigned int alreadyWritten);
};

/*
 * CKorgUsbAudioDriverMidiPorts -- the top-level KorgUsb MIDI transport
 * singleton (survey candidate 3, midi_korgusb_port.cpp). CONFIRMED real
 * via full `objdump -dr` of its ctor (`.text+0x3400f0`, 170 bytes),
 * `CMidiPortPair::Connect()`/`Disconnect()` (`.text+0x3401a0`/`0x340210`),
 * and `ProcessOutput()` (`.text+0x340260`, 154 bytes) -- plus the
 * compiler-generated static initializer for the real global
 * `sInstance` (`_GLOBAL__I__ZN28CKorgUsbAudioDriverMidiPorts9sInstanceE`,
 * `.text+0x340380`) which duplicates the ctor's own logic against
 * absolute addresses; not separately reconstructed, the ordinary
 * static-`sInstance`-with-a-real-ctor pattern already used elsewhere in
 * this project (e.g. `CSTGAudioDriverInterfaceKorgUsb::sInstance`,
 * managers.cpp) produces byte-identical observable behavior.
 *
 * Holds exactly 2 `CMidiPortPair`s (an embedded receive-side
 * `CSTGMidiInPortKorgUsb` + transmit-side `CSTGMidiOutPortKorgUsb`,
 * always constructed/torn-down together) back to back:
 *   pair[0] at +0x000, pair[1] at +0xb48 (CONFIRMED: ctor's own 2nd
 *   `lea eax,[ebx+0xb48]` and every one of `ProcessOutput()`'s stride-
 *   0xb48-apart field reads).
 * Each `CMidiPortPair` (0xb48 = 2888 bytes) is NOT modeled as a nested
 * C++ struct (deliberately, matching `CSTGMidiInPortKorgUsb`'s own
 * "don't trust compiler layout across this InPort/OutPort boundary"
 * reasoning above) -- its 2 named sub-objects sit at fixed raw byte
 * offsets from the pair's own base:
 *   +0x00  selfPtr (packed u32) -- the pair's OWN address, written by
 *          the ctor and passed to the companion module as the
 *          `InputCallback` thunk's implicit context.
 *   +0x04  callbackFnPtr (packed u32) -- always
 *          `&CMidiPortPair::InputCallback`, a fixed thunk (below), not
 *          actually invoked anywhere within OA.ko itself -- the
 *          companion USB module calls back through it asynchronously
 *          when a real USB MIDI packet arrives from hardware.
 *   +0x08  embedded `CSTGMidiInPortKorgUsb` (0x2e9 bytes: base
 *          `CSTGMidiInPort`'s own 0x2e8 + 1 added `midiPortIndex` byte
 *          at object-relative +0x2e8, i.e. absolute pair-relative
 *          +0x2f0 -- CONFIRMED, the ctor writes 0/1 there for the 2
 *          pairs) -- ends at +0x2f1, then 3 bytes natural alignment
 *          padding to +0x2f4.
 *   +0x2f4  embedded `CSTGMidiOutPortKorgUsb` (0x854 bytes, see that
 *          class's own header comment) -- ends exactly at +0xb48, the
 *          next pair's own base, with ZERO trailing padding (CONFIRMED:
 *          this project's own field-by-field sum for that class, cross-
 *          checked against the ctor's 2nd-pair placement).
 * Total `CKorgUsbAudioDriverMidiPorts` size: 2 * 0xb48 = 0x1690 bytes
 * (5776).
 *
 * Ctor params for the 2 pairs (CONFIRMED via raw register loads at each
 * of the 2 `CSTGMidiInPort`/`CSTGMidiOutPortKorgUsb` sub-ctor call
 * sites): pair0 = {korgUsbPort=0, stgPort=0, inPortFlagsInit=1,
 * outPortFlagsInit=0}, pair1 = {korgUsbPort=1, stgPort=1,
 * inPortFlagsInit=1, outPortFlagsInit=0}.
 */
/*
 * `sInstance`'s real construction is NOT run via an automatic C++ static
 * initializer -- deliberately. Ground truth's own compiler placed this
 * ctor's dynamic-init call in `.ctors` (CONFIRMED: `_GLOBAL__I__ZN28
 * CKorgUsbAudioDriverMidiPorts9sInstanceE`, `.text+0x340380`, is a
 * literal entry inside OA.ko's own 584-entry `.ctors` array -- found at
 * raw byte offset 0x4e0, via `objdump -s -j .ctors`), and this exact
 * kernel (`/home/build/linux-kronos`, `CONFIG_CONSTRUCTORS=y`) really
 * does run every `.ctors` entry: `kernel/module.c`'s `do_mod_ctors()`
 * is called from the `init_module` syscall handler BEFORE `mod->init`
 * (this project's own `init_module()`) is ever entered. This is a
 * genuine, working ctor-array mechanism -- NOT the same situation as
 * `init_cpp_support()` being a confirmed no-op (that function is a
 * no-op precisely BECAUSE the kernel already ran `.ctors` before
 * `init_module()` started, not because no ctor mechanism exists at all).
 *
 * The problem is toolchain drift: this project's host GCC (12.x) has no
 * `-fno-use-init-array` escape hatch, so a plain global object with a
 * non-trivial constructor here lands in `.init_array` instead --
 * confirmed via `nm OA.o | grep GLOBAL`, symbol `_GLOBAL__sub_I__ZN28
 * CKorgUsbAudioDriverMidiPorts9sInstanceE`. `do_mod_ctors()` only walks
 * `.ctors` (grepped the whole of kernel/module.c -- no `.init_array`
 * handling exists anywhere in it), so that initializer would NEVER run
 * on the real target kernel, leaving `sInstance` permanently
 * all-zero -- the same underlying bug class fixed in commit `804b909`
 * for `CSTGAudioInputMixer`'s vtable, just manifesting as a whole-object
 * ctor instead of two vtable-slot writes.
 *
 * Fix: give this class NO user-declared constructor at all (so
 * declaring `sInstance` needs no dynamic initializer whatsoever -- it's
 * plain zeroed BSS, exactly like it would be immediately before
 * `do_mod_ctors()` ran in ground truth), and move the real construction
 * logic into an ordinary method, `Construct()`, called explicitly and
 * FIRST from `init_module()` (src/init/init_module.cpp) -- matching
 * `do_mod_ctors()`'s real relative timing (strictly before every other
 * init_module step, including `init_cpp_support()`). The host verify
 * build (verify/test_midi_korgusb_port.cpp) calls the same `Construct()`
 * explicitly too, for the same reason -- this project's convention of
 * "explicit populate call instead of trusting a compiler-emitted ctor"
 * already used for the `sMsgHandler` dispatch tables elsewhere.
 */
class CKorgUsbAudioDriverMidiPorts {
public:
	unsigned char storage[2 * 0xb48];

	static CKorgUsbAudioDriverMidiPorts sInstance;

	/* Real construction logic -- see the class comment above for why
	 * this is a plain method, not a constructor. */
	void Construct();

	/* Raw sub-object accessors -- see the class comment for why these
	 * are offset-based rather than nested C++ members. `pairIdx` is 0
	 * or 1. */
	CSTGMidiInPortKorgUsb  *InPort(int pairIdx)  { return (CSTGMidiInPortKorgUsb *)(storage + pairIdx * 0xb48 + 0x08); }
	CSTGMidiOutPortKorgUsb *OutPort(int pairIdx) { return (CSTGMidiOutPortKorgUsb *)(storage + pairIdx * 0xb48 + 0x2f4); }

	/*
	 * CMidiPortPair::Connect()/Disconnect() -- CONFIRMED real
	 * (`.text+0x3401a0`, 94 bytes / `.text+0x340210`, 66 bytes), called
	 * from `CSTGMidiInPortKorgUsb::Activate()`/`Deactivate()` with
	 * `this` = the pair's OWN base address (InPort object address - 8).
	 * Connect(): if the companion module is NOT yet initialized for
	 * this port index (`KorgUsbMidiInitialized(idx)`), call
	 * `KorgUsbMidiInitialize(idx, 0x400, 0x400, pairBase)` first (the 2
	 * 0x400 args plausibly size the companion module's OWN internal
	 * buffers -- same magic constant as this class's own ring sizes,
	 * not independently confirmed to be related). Either way, then
	 * ensure the LOCAL output pump is running
	 * (`STGMidiOutPortKorgUsb_Initialized()` / `_Initialize()`).
	 * Disconnect(): symmetric teardown, LOCAL pump first
	 * (`STGMidiOutPortKorgUsb_Initialized()`/`_Done()`), then the
	 * companion module (`KorgUsbMidiInitialized(idx)`/`KorgUsbMidiDone(idx)`).
	 */
	static void Connect(unsigned char *pairBase);
	static void Disconnect(unsigned char *pairBase);

	/*
	 * InputCallback(void *ctx, USBMidiPacket pkt) -- CONFIRMED real but
	 * genuinely never called from anywhere within OA.ko itself (own
	 * un-merged comdat, zero xrefs in this binary -- the companion USB
	 * module is the only real caller, reached only with actual USB MIDI
	 * hardware attached). A pure thunk: adjusts `this` by +8 (pair base
	 * -> embedded InPort) and forwards, UNTOUCHED, into
	 * `CSTGMidiInPortUSB::ReceivePacket(USBMidiPacket)` -- see that
	 * (deliberately-deferred) method's own declaration, oa_engine.h.
	 */
	static void InputCallback(void *ctx, USBMidiPacket pkt);

	/*
	 * ProcessOutput() -- CONFIRMED real (`.text+0x340260`, 154 bytes).
	 * For EACH of the 2 pairs: if that pair's OutPort `flags` bit1
	 * (active, base `CSTGMidiOutPort::flags`) is set, drain
	 * `RealtimeOutput()` if its realtime ring has pending bytes and
	 * `Output()` if its regular ring does. `STGMidiOutPortKorgUsb_
	 * Output()` (below) is an independently-compiled duplicate of this
	 * SAME logic against the fixed `sInstance` address rather than a
	 * generic `this` -- modeled as a thin call-through here, producing
	 * IDENTICAL observable behavior (this project's established
	 * "compiler inlined a known-address singleton call" convention).
	 */
	void ProcessOutput();
};

/*
 * STGMidiOutPortKorgUsb_* -- the RTAI-real-time-domain-to-Linux-domain
 * deferred-work pump for CKorgUsbAudioDriverMidiPorts::ProcessOutput()
 * (full derivation + real addresses in midi_korgusb_port.cpp's own
 * header comment). Declared here (not just file-local in that .cpp) so
 * both it and its own KAT can see the same real symbols.
 */
void STGMidiOutPortKorgUsb_Output();
int  STGMidiOutPortKorgUsb_Initialized();
void STGMidiOutPortKorgUsb_Initialize();
void STGMidiOutPortKorgUsb_Done();
void STGMidiOutPortKorgUsb_ScheduleFromRTAI();
void STGMidiOutPortKorgUsb_ScheduleFromLinux();

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
	 * Initialize() (batch 55, `.text+0xb9920`, 92 bytes) confirmed real
	 * -- see src/engine/sequence_combi_init.cpp for the full derivation
	 * (sec 10.230, `CSTGSequence::Initialize()`'s own hand-off chain).
	 * Calls `CSTGEffectRack::Initialize()` (direct, non-virtual) on the
	 * `CSTGEffectRack` base sub-object at `this+4`, two genuine virtual
	 * slot-7 dispatches on the embedded `CSTGCommonEffectLFO` pair at
	 * `+0xa59`/`+0xa6a` (bypassed via the established
	 * `CSTGParamsOwner::UseDefaults()` forwarding cast -- confirmed
	 * neither overrides it, `objdump -sr -j .rodata._ZTV19CSTGCommonEffectLFO`),
	 * two DIRECT (non-virtual in ground truth) `UseDefaults()` calls on
	 * the embedded `CSTGVectorMotion`/`CSTGAudioInput` sub-objects at
	 * `+0xa7b`/`+0xae7`, and finally a literal `float 1.0f` write at
	 * `+0xb5f` (a confirmed real immediate, own meaning not determined).
	 */
	void Initialize();

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
 *
 * Batch 2026-07-25 adds 5 more confirmed real methods (see
 * src/engine/streaming_event_manager.cpp for the full derivation):
 * HandleErrorReading()/CloseFileDescriptorsIfNecessary() plus 3 fields
 * newly named via those two methods' own disassembly:
 *   +0x1c  fdCount        -- byte, number of valid entries in the fd
 *          array below; loop bound for CloseFileDescriptorsIfNecessary().
 *   +0x24  fds[]           -- packed-32-bit `CSTGFile*` handle array,
 *          up to fdCount entries, 4 bytes each starting at this offset
 *          (real per-slot upper bound not independently confirmed --
 *          every real caller so far uses a small fdCount).
 *   +0x40  circBuf         -- an EMBEDDED (not pointer) CSTGHDRCircularBuffer
 *          sub-object (confirmed via the ctor's own `lea eax,[this+0x40];
 *          call CSTGHDRCircularBuffer::CSTGHDRCircularBuffer()`), used
 *          directly by ProcessCommandFilledBytes() (managers.cpp).
 *   +0x70  voice           -- packed-32-bit `CSTGVoice*`, read only by
 *          HandleErrorReading() (forwarded to
 *          `CSTGVoiceAllocator::StealVoice()`, deliberately deferred).
 *   +0x94  fdsEnabled      -- dword flag, gates whether
 *          CloseFileDescriptorsIfNecessary() actually pushes each fd
 *          entry (re-read every loop iteration, a real quirk: a caller
 *          could in principle flip it mid-loop, though no confirmed real
 *          caller does).
 *   +0xb8/+0xc4  a confirmed real dword pair compared (unsigned) by
 *          ProcessCommandFilledBytes() (managers.cpp) to gate the
 *          `CSTGDiskCostManager::UpdateDiskThroughputBytesRead()` call --
 *          real high-level semantics (position vs. limit?) not
 *          independently determined, every touch faithfully reproduced.
 */
struct CSTGStreamingEvent {
	CSTGStreamingEvent();
	/*
	 * HandleErrorReading() (`.text+0xd2070`, 22 bytes) confirmed real:
	 * forwards `voice` (+0x70) to `CSTGVoiceAllocator::sInstance->
	 * StealVoice()` (oa_engine.h, deliberately deferred -- genuine
	 * voice-reallocation DSP logic, out of scope). Also the real target
	 * resolved by `CSTGHDRFileReader`/`CSTGStreamingFileReader::
	 * ProcessCommandError()`'s own vtable-slot-5 dispatch on a
	 * `CSTGPlaybackEvent*`/`CSTGStreamingEvent*` respectively (confirmed
	 * via readelf relocation resolution against
	 * `.rodata._ZTV18CSTGStreamingEvent`/`.rodata._ZTV17CSTGPlaybackEvent`
	 * -- CSTGPlaybackEvent's own override is a bare `ret`, already real).
	 */
	void HandleErrorReading();
	/*
	 * CloseFileDescriptorsIfNecessary() (`.text+0xd2330`, 86 bytes)
	 * confirmed real: iterates `fds[0..fdCount)`, pushing each non-null,
	 * fdsEnabled-gated entry as `{fd, 0}` onto `CSTGFileCloser::
	 * sInstance`'s own first embedded ring (the SAME target/shape every
	 * other file-daemon `ProcessCommands()` sibling already pushes into
	 * -- see managers.cpp). Real caller: `CSTGStreamingEventManager::
	 * ReturnFreeEvent()` (streaming_event_manager.cpp).
	 */
	void CloseFileDescriptorsIfNecessary();
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
	/*
	 * ReturnFreeEvent(CSTGStreamingEvent*) (`.text+0xd1e10`, 320 bytes,
	 * batch 2026-07-25) confirmed real -- see
	 * src/engine/streaming_event_manager.cpp for the full derivation.
	 * Reentrant-safe (nesting counted via `field14c38`): only the
	 * OUTERMOST call (depth 0->1) acquires a global CLI-disabling lock
	 * (`rtwrap_global_save_flags_and_cli()`/`rtwrap_global_restore_
	 * flags()`, RTAI hal primitives genuinely defined INSIDE OA.ko
	 * itself, not forwarded -- see bar2_stubs_c.cpp), but
	 * `event->CloseFileDescriptorsIfNecessary()` itself is called
	 * UNCONDITIONALLY on every call, nested or not; every call unlinks the
	 * event from the `field14c24`/`field14c28`-headed doubly-linked
	 * "sounding events" list (the same list `AddSoundingEvent()`, sec
	 * 10.145, already threads through), decrements `field14c2c`, then
	 * pushes the event onto the `freeListHead`/`freeListTail` free list
	 * and increments `count` -- symmetric with `GetFreeEvent()`'s own
	 * already-real pop-from-freelist logic.
	 */
	void ReturnFreeEvent(CSTGStreamingEvent *event);

	unsigned char _unrecovered_head[4];		/* +0x000 */
	CSTGStreamingEvent events[401];		/* +0x004..+0x14c18 */
	unsigned int freeListHead;			/* +0x14c18 */
	unsigned int freeListTail;			/* +0x14c1c */
	unsigned int count;				/* +0x14c20 */
	unsigned int field14c24;	/* +0x14c24 -- "sounding events" list head, see AddSoundingEvent()/ReturnFreeEvent() */
	unsigned int field14c28;	/* +0x14c28 -- "sounding events" list tail, same list */
	unsigned int field14c2c;	/* +0x14c2c -- sounding-events count, decremented by ReturnFreeEvent() */
	unsigned int mutexPtr32;			/* +0x14c30 */
	unsigned int field14c34;	/* +0x14c34 -- ReturnFreeEvent()'s own saved-flags token, valid while field14c38>0 */
	unsigned int field14c38;	/* +0x14c38 -- ReturnFreeEvent() reentrancy/nesting depth counter */
	unsigned int field14c3c;	/* +0x14c3c -- confirmed real, touched by both AddSoundingEvent()/ReturnFreeEvent(), exact semantics not fully determined */
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
	 * CSTGGlobal::PreprocessPerformanceChange) is real now, batch 61 --
	 * see src/engine/smoother_finalize_all.cpp for the full derivation
	 * (a confirmed hybrid of CancelAllSmoothers()'s own unlink/free-list
	 * logic plus FinalizeSmoother(node, true)'s own dispatch call). */
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

/*
 * CSTGExtMIDIClockSync : public CSTGMIDIClockSyncBase -- the EXTERNAL
 * (incoming-clock-following) sibling of CSTGIntMIDIClockSync, discovered
 * embedded at `CSTGMidiInPort+0x108` (batch: KorgUsb MIDI transport,
 * midi_korgusb_port.cpp/midi_in_port_serial.cpp) filling that class's own
 * previously-`_unrecovered108[0x1d8]` gap EXACTLY (0x108..0x2e0, 0x1d8
 * bytes -- confirmed by this class's own highest touched field,
 * fieldAt(0x1d4), landing 4 bytes short of that gap's full size). Unlike
 * CSTGIntMIDIClockSync (embedded via a `+0x4` sub-object offset inside
 * CSTGMIDIClockSync), this class inherits `CSTGMIDIClockSyncBase`
 * directly at offset 0 -- confirmed via `Initialize()`'s own call to
 * `CSTGMIDIClockSyncBase::Initialize()` passing `this` completely
 * unmodified (no `lea eax,[eax+0x4]` the way CSTGMIDIClockSync's own ctor
 * does for its Int sub-object).
 *
 * Real vtable (`_ZTV20CSTGExtMIDIClockSync`, readelf-confirmed 0x28/40
 * bytes, 8 slots, same slot layout as CSTGMIDIClockSyncBase/
 * CSTGIntMIDIClockSync above): 0x08 GetEventCount, 0x0c NotifySyncDetected,
 * 0x10 GetEventStatusByte, 0x14 ProcessClock, 0x18 ConsumeEvent, 0x1c
 * GetClockEarlyThresholdTicks, 0x20 GetClockLateThresholdTicks, 0x24
 * PrepareForNextTick.
 *
 * ALL 13 methods below are confirmed real (`nm -CS`/full `objdump -dr`),
 * but only 10 are reconstructed by this pass -- ProcessClock()/
 * MeasureJitter()/EstimateTempoAndPredictNextClock() are declared but
 * DELIBERATELY DEFERRED (no-op stubs in bar2_stubs.cpp), matching this
 * project's established convention. Unlike most prior deferrals, this one
 * is backed by full disassembly, not a size-based guess (this class was
 * originally flagged "disproportionate" without being examined at all --
 * this pass genuinely investigated it): ProcessClock() (`.text+0x68650`,
 * 174 bytes) reads a 4-slot/12-byte-stride incoming-clock timestamp ring
 * at fieldAt(0x40) (indexed by `fieldAt(0xa8) & 7`, i.e. the SAME
 * read-index GetEventStatusByte()/ConsumeEvent() advance -- so this ring
 * is fed by whatever writes fieldAt(0xa4)/status-ring entries, not yet
 * identified) and calls EstimateTempoAndPredictNextClock() then
 * MeasureJitter(); MeasureJitter() (`.text+0x68480`, 460 bytes) is a
 * genuine median-of-3 smoothing filter over 3 samples from a 32-entry
 * float ring at fieldAt(0xbc) (indexed `fieldAt(0xb8) & 0x1f`), expressed
 * via x87 `fucomi`/`fcmovbe`/`fcmovnbe` conditional-move sorting -- high
 * transcription risk, genuinely disproportionate to reproduce by hand
 * without a way to KAT-verify against real x87 stack behavior;
 * EstimateTempoAndPredictNextClock() (`.text+0x68130`, 737 bytes, NOT
 * examined in detail this pass) is by far the largest method in the
 * class. None of the 3 deferred methods, nor ProcessClock()'s own
 * still-unidentified ring producer, are reachable from anything this
 * project currently reconstructs -- the sole confirmed real caller
 * anywhere in this codebase is `CSTGMidiInPort::Activate()`'s direct call
 * to `Initialize()` (midi_in_port_serial.cpp), which this pass DOES
 * implement for real, along with the small always-correct accessor
 * methods below. A future session reconstructing whatever drives
 * `CSTGMIDIClockSync`'s own external-vs-internal dispatch (not yet
 * touched anywhere in this project) is the natural point to revisit the
 * deferred trio.
 */
class CSTGExtMIDIClockSync : public CSTGMIDIClockSyncBase {
public:
	/*
	 * Initialize() (`.text+0x68010`, 283 bytes) confirmed real. Calls
	 * the base `CSTGMIDIClockSyncBase::Initialize()` directly on `this`
	 * (see class comment). Then, guarded by its OWN function-local
	 * static byte (separate from the base's own guard):
	 *   - `kSecondsToTimeStampE` (double) = `(double)(1000u *
	 *     CSTGCPUInfo::sInstance->khz)` -- CPU clock rate in Hz. Real
	 *     code does a full unsigned-64-bit-safe conversion (mul,
	 *     64-bit fild + 2^64 correction if the high dword's sign bit is
	 *     set); reproduced here as a plain `unsigned int` conversion,
	 *     which is behaviorally identical for every real CPU frequency
	 *     this target ever runs at (khz well under 4,294,967, the
	 *     point the 32-bit product would even reach the correction
	 *     branch) -- also sidesteps a real 32-bit-kernel-module hazard:
	 *     a genuine `unsigned long long -> double` cast needs
	 *     `__floatundidf` from libgcc, unavailable in this Kbuild
	 *     target (same class of problem documented in the STGGmp.ko
	 *     build, project docs).
	 *   - `kTimeStampToSecondsE` (double) = `1.0 / kSecondsToTimeStampE`.
	 *   - `kClockEarlyThresholdTicksE` (float) = `floor(-0.0004f *
	 *     CSTGAudioBusManager::sInstance->busGainScale)` -- confirmed
	 *     real FLOOR rounding (x87 CW round-toward-`-inf`, `0x0400`),
	 *     genuinely different from every other rounding in this class
	 *     (all CEIL) -- NOT reproducible as a plain truncating cast
	 *     (input is negative and non-integer at the real busGainScale
	 *     default: `floor(-0.6) = -1 != (int)(-0.6) = 0`). Uses this
	 *     file's local `FloorToInt()`/`CeilToInt()` helpers.
	 * Unconditionally (every call, matching the base's own "every call"
	 * tail): `fieldAt(0xa4)` = 0, `fieldAt(0xa8)` = 0, `fieldAt(0x1bc)`
	 * (jitter estimate) = 0.0f, `fieldAt(0x1d4)` (dynamic late-threshold,
	 * see `GetClockLateThresholdTicks()`) = `ceil(0.001f *
	 * CSTGAudioBusManager::sInstance->busGainScale)` (CEIL, `0x0800`,
	 * SAME formula/rounding as `NotifySyncDetected()`/
	 * `UpdateDynamicThresholds()` below -- cross-checked, all three
	 * confirmed identical opcodes), `fieldAt(0x1c4)` (target tempo) =
	 * `0x78` (120, a default BPM-ish sentinel).
	 */
	void Initialize();

	/* GetEventCount() const (`.text+0x67f40`, 15 bytes): return
	 * fieldAt(0xa4) - fieldAt(0xa8) -- SAME shape as
	 * CSTGIntMIDIClockSync::GetEventCount(), different offsets. */
	unsigned int GetEventCount() const;

	/* GetEventStatusByte() const (`.text+0x67f50`, 21 bytes): return the
	 * ring byte at fieldAt(0x40 + (fieldAt(0xa8) & 7) * 0xc + 0x4) -- an
	 * 8-entry, 12-byte-stride ring anchored at +0x40 (the SAME ring
	 * ProcessClock() reads, see class comment), indexed by the
	 * read-counter mod 8. Real disasm: `lea edx,[edx+edx*2]` (index*3)
	 * then `lea eax,[eax+edx*4+0x40]` (index*3*4 = index*0xc), then
	 * `movzx eax,[eax+0x4]`. */
	unsigned char GetEventStatusByte() const;

	/* ConsumeEvent() (`.text+0x67f70`, 8 bytes): fieldAt(0xa8) += 1. */
	void ConsumeEvent();

	/*
	 * NotifySyncDetected() (`.text+0x67f80`, 144 bytes) confirmed real:
	 * zeroes fieldAt(0xb4)/fieldAt(0x1c0) (int), sets fieldAt(0xb8) =
	 * -1 (int), zero-fills the two 0x80-byte (32 x float) ring buffers
	 * at fieldAt(0xbc) and fieldAt(0x13c) (`rep stosb`), then sets
	 * fieldAt(0x1d4) = `ceil(0.001f *
	 * CSTGAudioBusManager::sInstance->busGainScale)` -- see
	 * `Initialize()`'s own comment for the cross-check.
	 */
	void NotifySyncDetected();

	/*
	 * ProcessClock() (`.text+0x68650`, 174 bytes) -- CONFIRMED REAL,
	 * examined but DELIBERATELY DEFERRED. See class comment. Follow-up
	 * pass (independent re-disassembly, full instruction-by-instruction
	 * decode) confirms the class comment's own characterization exactly
	 * (fieldAt(0xa8)&7 ring index into the fieldAt(0x40) ring, calls to
	 * EstimateTempoAndPredictNextClock()/MeasureJitter()) and adds:
	 * increments/clamps TWO separate counters, fieldAt(0xb4) (int,
	 * saturates at 0x7fffffff via `cmovs`) and fieldAt(0xb8) (plain
	 * int, no clamp -- this is the SAME counter MeasureJitter's own
	 * `fieldAt(0xb8)&0x1f` indexes into the 32-entry delta ring at
	 * fieldAt(0xbc), i.e. ProcessClock is confirmed to be that ring's
	 * real producer too, not just the event ring's consumer-side
	 * reader). Reads the ring slot at fieldAt(0x40+idx*0xc) TWO dwords
	 * at once, +0x8 and +0xc -- the +0xc read is OUTSIDE this same
	 * slot's own 0xc-byte span (it lands in the FIRST dword of ring
	 * slot idx+1, or one dword past the ring's own end when idx==7) --
	 * a genuine unresolved structural question (not a transcription
	 * slip -- re-decoded the `lea edx,[edx+edx*2]` / `lea edx,[ebx+
	 * edx*4+0x40]` pair by hand against the raw ModRM/SIB bytes, matches
	 * objdump exactly): either the two dwords are read from DIFFERENT,
	 * not-yet-identified parallel arrays that merely alias this
	 * addressing arithmetic, or the ring's real per-entry stride is NOT
	 * simply 0xc/12 bytes despite GetEventStatusByte()'s own indexing
	 * using that exact multiplier. Computes a delta (from fieldAt(0xac),
	 * a cached "previous timestamp", UNLESS fieldAt(0x8)==2 in which case
	 * the subtrahend is fieldAt(0x1cc) instead) and stores it into the
	 * fieldAt(0xbc) 32-entry ring at index fieldAt(0xb8)&0x1f -- but only
	 * once fieldAt(0xb4) > 1 (the first call after NotifySyncDetected()'s
	 * own zero-init just caches fieldAt(0xac)/fieldAt(0xb0) without
	 * computing a delta). This delta-producer shape is now confirmed;
	 * the +0xc boundary question above is the one thing worth resolving
	 * FIRST in any future session attempting this trio, before writing
	 * the ring slot layout down as ground truth.
	 */
	void ProcessClock();

	/*
	 * MeasureJitter() (`.text+0x68480`, 460 bytes) -- CONFIRMED REAL,
	 * examined but DELIBERATELY DEFERRED. See class comment.
	 */
	void MeasureJitter();

	/*
	 * EstimateTempoAndPredictNextClock() (`.text+0x68130`, 737 bytes) --
	 * CONFIRMED REAL, NOT examined in detail, DELIBERATELY DEFERRED.
	 * See class comment.
	 */
	void EstimateTempoAndPredictNextClock();

	/*
	 * UpdateFilteredTempo(double) (`.text+0x68420`, 91 bytes) confirmed
	 * real: `predicted = (int)(CSTGAudioBusManager::sInstance->
	 * busGainScale * arg * kConst_0x1b0)` (kConst_0x1b0 a confirmed
	 * `.rodata.cst4` float, plain `fistp`-truncated, NOT rounded --
	 * genuinely different from every fieldAt(0x1d4)-style CEIL
	 * computation above). If `predicted != fieldAt(0x1c4)` (the current
	 * target tempo): a debounce counter at fieldAt(0x1c8) is compared
	 * against `0x1f` (31) -- `<= 31`: just increment the counter and
	 * return (predicted value NOT yet adopted); `> 31`: adopt
	 * `fieldAt(0x1c4) = predicted` AND reset the counter to 0. If
	 * `predicted == fieldAt(0x1c4)` already: reset the counter to 0
	 * directly (no adoption needed, already current).
	 */
	void UpdateFilteredTempo(double bpm);

	/*
	 * UpdateDynamicThresholds() (`.text+0x68700`, 100 bytes) confirmed
	 * real: `clampedJitter = clamp(fieldAt(0x1bc), 0.001f, 0.008f)`
	 * (both confirmed `.rodata.cst4` floats, via `fucomi`/`fcmovnbe`
	 * conditional moves); `fieldAt(0x1d4) = ceil(clampedJitter *
	 * CSTGAudioBusManager::sInstance->busGainScale)` (CEIL, `0x0800` --
	 * BYTE-FOR-BYTE IDENTICAL tail to `MeasureJitter()`'s own last ~40
	 * bytes, confirmed via opcode comparison -- this project's
	 * established "shared identical tail -> one helper" convention
	 * applies, expressed as `ClampAndStoreLateThreshold()` below, though
	 * `MeasureJitter()` itself stays deferred).
	 */
	void UpdateDynamicThresholds();

	/* GetClockLateThresholdTicks() const (`.text+0x68660` section, 6
	 * bytes: `fld [eax+0x1d4]; ret`) confirmed real: returns the
	 * DYNAMIC per-instance fieldAt(0x1d4) -- genuinely different from
	 * CSTGIntMIDIClockSync's own trivial constant-1.0f override. */
	float GetClockLateThresholdTicks() const;

	/* GetClockEarlyThresholdTicks() const (`.text+0x68650` section, 6
	 * bytes: `fld kClockEarlyThresholdTicksE; ret`) confirmed real:
	 * returns the STATIC `kClockEarlyThresholdTicksE` computed once in
	 * `Initialize()`. */
	float GetClockEarlyThresholdTicks() const;

	/* PrepareForNextTick() (`.text+0x68660` section, 1 byte: bare `ret`)
	 * confirmed real no-op override -- genuinely different from
	 * CSTGIntMIDIClockSync's own real (non-trivial) override. */
	void PrepareForNextTick();

	static double kSecondsToTimeStamp;
	static double kTimeStampToSeconds;
	static float kClockEarlyThresholdTicks;
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
 * `Initialize()` (vtable slot 2) / `GetId()` (const, slot 3) /
 * `ProcessSubRate(unsigned int)` (slot 18, `.text` offset +0x48) /
 * `ProcessAudioRate(unsigned int)` (slot 19, +0x4c) -- of each model's 21
 * real virtual methods, these four are the ones CURRENTLY-REACHABLE code
 * in this reconstruction dispatches (slot 2 via `CallVtableSlot` right
 * after construction; slot 3 via `CSTGKLMManager::AuthorizeBuiltins()`'s
 * own `VM_GET_ID` vcall, `klm_manager.cpp`, wired for real sec 10.234 --
 * see `voice_models.cpp`'s own header note on the live-boot NULL-slot
 * crash (`EIP=CR2=0`) this uncovered; slots 18/19 via
 * `CSTGVoiceModelManager::ProcessSubRate`/`ProcessAudioRate`, already
 * real since sec 10.137, once `Register()` -- new this batch -- actually
 * populates the array those two walk). All confirmed real in ground
 * truth (`nm -C -S`); `CSTGOffModel`'s own `Initialize`/`ProcessSubRate`/
 * `ProcessAudioRate` are confirmed literally 1 byte each (a bare `ret`)
 * and are reconstructed for real (see voice_models.cpp), and EVERY
 * model's own `GetId()` (3 or 5 bytes: `xor eax,eax; ret` for Off,
 * `mov $N,%eax; ret` for the rest, `N` = that model's own
 * `eSTGVoiceModelType` ordinal 1..9) is likewise reconstructed for real,
 * not deferred -- trivially cheap, zero dependencies. The other 27
 * (9 models x 3 methods, `Initialize`/`ProcessSubRate`/`ProcessAudioRate`)
 * are confirmed substantial (up to ~2KB) genuine per-model DSP init/
 * audio-tick bodies -- out of scope per the sec 10.185 policy, given safe
 * no-op stand-ins in bar2_stubs.cpp (matching the `CSetListEQ::SetBand`/
 * `CSTGControllerInfo::SetPerfSwitch` precedent). Declared here as
 * `extern "C"` free functions (own vtable-slot signature, `void(*)(void*)`/
 * `unsigned int(*)(const void*)`/`void(*)(void*,unsigned int)`), NOT C++
 * methods -- matching the `CStartupFile::Load`/`kCCostProfileVtbl`
 * precedent (sec 10.186) exactly, since nothing in this project ever
 * calls them via `.`/`->` syntax, only through a raw vtable-slot function
 * pointer.
 */
extern "C" void OA_VoiceModel_Off_Initialize(void *self);
extern "C" unsigned int OA_VoiceModel_Off_GetId(const void *self);
/*
 * GetAuthField()const / SetAuthField(int) / SetProductId(unsigned long) --
 * real CSTGVoiceModel BASE-CLASS methods (ONE shared implementation for
 * all ten models, confirmed via ground truth's own identical vtable slots
 * across every derived class -- see voice_models.cpp's own header note).
 * Newly reachable for the same reason as GetId (sec 10.234):
 * klm_manager.cpp's stamp_object() dispatches through these slots from
 * CSTGKLMManager::AuthorizeBuiltins().
 */
extern "C" unsigned int OA_VoiceModel_GetAuthField(const void *self);
extern "C" void OA_VoiceModel_SetAuthField(void *self, unsigned int value);
extern "C" void OA_VoiceModel_SetProductId(void *self, unsigned int value);
extern "C" void OA_VoiceModel_Off_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Off_ProcessAudioRate(void *self, unsigned int tick);

/*
 * The other nine models' own Initialize()/ProcessSubRate()/
 * ProcessAudioRate() -- confirmed real, substantial (332-2097 bytes),
 * genuine per-model DSP -- deliberately deferred, safe no-op bodies
 * defined in bar2_stubs.cpp (matching the CSetListEQ::SetBand
 * precedent, sec 10.192). GetId() (unlike those three) is real for all
 * nine too, see the header note above. Declared here only so
 * voice_models.cpp can take their address for each model's own real
 * vtable.
 */
extern "C" void OA_VoiceModel_PCM_Initialize(void *self);
extern "C" unsigned int OA_VoiceModel_PCM_GetId(const void *self);
extern "C" void OA_VoiceModel_PCM_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_PCM_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_AnalogSync_Initialize(void *self);
extern "C" unsigned int OA_VoiceModel_AnalogSync_GetId(const void *self);
extern "C" void OA_VoiceModel_AnalogSync_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_AnalogSync_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Organ_Initialize(void *self);
extern "C" unsigned int OA_VoiceModel_Organ_GetId(const void *self);
extern "C" void OA_VoiceModel_Organ_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Organ_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Plucked_Initialize(void *self);
extern "C" unsigned int OA_VoiceModel_Plucked_GetId(const void *self);
extern "C" void OA_VoiceModel_Plucked_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Plucked_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_MS20_Initialize(void *self);
extern "C" unsigned int OA_VoiceModel_MS20_GetId(const void *self);
extern "C" void OA_VoiceModel_MS20_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_MS20_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Polysix_Initialize(void *self);
extern "C" unsigned int OA_VoiceModel_Polysix_GetId(const void *self);
extern "C" void OA_VoiceModel_Polysix_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Polysix_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_VPM_Initialize(void *self);
extern "C" unsigned int OA_VoiceModel_VPM_GetId(const void *self);
extern "C" void OA_VoiceModel_VPM_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_VPM_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Piano_Initialize(void *self);
extern "C" unsigned int OA_VoiceModel_Piano_GetId(const void *self);
extern "C" void OA_VoiceModel_Piano_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Piano_ProcessAudioRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_EP_Initialize(void *self);
extern "C" unsigned int OA_VoiceModel_EP_GetId(const void *self);
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
