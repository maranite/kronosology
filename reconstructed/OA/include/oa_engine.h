// SPDX-License-Identifier: GPL-2.0
/*
 * oa_engine.h  -  CSTGEngine, the STG synthesis engine's top-level object.
 * Stage 3 (PLAN.md: "CSTGEngine, CSTGGlobal, the managers").
 *
 * CSTGEngine itself has only 7 distinct methods (ctor/dtor plus 5 real
 * methods -- confirmed via the ELF symbol table, no others exist):
 *   CSTGEngine()                          .text+0xe0    (10 bytes)
 *   ~CSTGEngine()                         .text+0xf0    (191 bytes)
 *   Initialize()                          .text+0x1b0   (1901 bytes)
 *   PreAudioTick()                        .text+0x920   (741 bytes)
 *   RunAudioTick(unsigned int)            .text+0xc10   (44 bytes)
 *   PostAudioTick()                       .text+0xc40   (94 bytes)
 *   RunEffects()                          .text+0xca0   (20 bytes)
 *   RunFileDaemonSynchronization()        .text+0xcc0   (90 bytes)
 *
 * ctor/dtor/RunAudioTick/PostAudioTick/RunEffects/RunFileDaemonSynchronization
 * are all fully disassembly-confirmed and implemented in engine.cpp.
 *
 * `Initialize()` and `PreAudioTick()` are NOT implemented here. Both are
 * fully disassembled and their exact structure is confirmed (Initialize:
 * ~40 manager/model singletons constructed via `CSTGBankMemory::
 * AllocAligned` + placement-new, in a fixed order, with exact byte sizes
 * for each -- see MASTER_REFERENCE.md sec 10.13 for the full table;
 * PreAudioTick: ~40 sequential per-tick calls into those same singletons).
 * Both are deliberately left out of the compiling tree: writing them for
 * real means declaring ~40 manager classes (CSTGAudioManager,
 * CSTGVoiceAllocator, CSTGEffectManager, CSTGHDRManager, and so on) that
 * are themselves Stage 3's remaining, much larger body of work -- adding
 * bare forward declarations just to make these two functions "compile"
 * would misrepresent how much of each manager is actually reconstructed.
 * PLAN.md guiding principle #5 ("always compilable") is satisfied by
 * simply not including these two function bodies yet, not by stubbing
 * their dependencies.
 *
 * The small set of manager singletons the SIX implemented methods touch are
 * declared as minimal opaque classes below -- just the static instance
 * pointer and the specific method(s) actually called, nothing more (same
 * treatment as CSTGVoiceModel/CSTGMultisampleBank before their full
 * layouts were known).
 */

#ifndef OA_ENGINE_H
#define OA_ENGINE_H

/* ---- Minimal opaque manager singletons touched by the implemented methods ---- */

/* Real static method, confirmed via its own mangled-name relocation; not
 * implemented -- declared here (not with a body) so its mangled name
 * matches the real symbol exactly. Used by CSTGVoiceModelManager's
 * constructor below. */
class CSTGToneAdjustDescriptor {
public:
	static void InitializeCommonToneAdjustDescriptors();
};

/*
 * CSTGVoiceModelManager's constructor (.text+0x1a9950, 143 bytes) confirmed:
 * two big `CSTGBankMemory::AllocAligned` pools stored as raw pointers at
 * +0x00 (339,712 bytes) and +0x04 (652,288 bytes), ten dwords zeroed
 * (+0x08..+0x2c), a word zeroed at +0x58, then a call to
 * `CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors()` -- a
 * plain static/free-standing call (no receiver register loaded before it),
 * not implemented here. Confirmed 92-byte total size leaves real, confirmed
 * gaps: +0x30..+0x57 (40 bytes) and +0x5a..+0x5b (2 bytes) are untouched.
 */
/*
 * ProcessSubRate(unsigned int)/ProcessAudioRate(unsigned int) (sec
 * 10.137, .text+0x1a9ac0/.text+0x1a9a70, 65 bytes each, byte-for-byte
 * identical shape) confirmed: RESOLVES the constructor's own "+0x30..
 * +0x57 real confirmed gap" note above -- `+0x30` is a real array of up
 * to 10 pointers (40 bytes / 4), `+0x58` its own real count (a signed
 * short, re-read fresh from `this` on EVERY loop iteration, not cached
 * -- a genuine confirmed quirk, not "cleaned up" into a cached-count
 * loop). For each of the first `count` array entries, makes a raw
 * virtual call through that entry's own vtable -- slot `0x4c/4 == 19`
 * for ProcessAudioRate, slot `0x48/4 == 18` for ProcessSubRate (the
 * only difference between the two functions) -- passing `tick` through
 * unchanged. Modeled with this project's established raw-vtable-slot-
 * call idiom (e.g. `NotifySoloChange`, sec 10.107) since these array
 * entries' own real class isn't otherwise typed/modeled.
 */
class CSTGVoiceModelManager {
public:
	static CSTGVoiceModelManager *sInstance;
	CSTGVoiceModelManager();
	~CSTGVoiceModelManager();
	void ProcessSubRate(unsigned int tick);
	void ProcessAudioRate(unsigned int tick);
	unsigned char _unrecovered[92];
};

/*
 * CSTGEffectManager's constructor (.text+0x207ef0, 103 bytes) confirmed:
 * `sInstance = this`, a lone zeroed dword at +0x800, a 198-element
 * zeroed dword array at +0x804..+0xb1b (792 bytes -- real per-element
 * meaning not determined; 198 doesn't match any Kronos constant this
 * project has independently confirmed so far), then a real, CONFIRMED
 * gap at +0xb1c..+0xb63 (72 bytes, untouched by this constructor), then
 * six more confirmed dwords: two float literals (120.0f, 120.0f) at
 * +0xb64/+0xb68 followed by four zeroed dwords at +0xb6c/+0xb70/+0xb74/
 * +0xb78. Everything from +0x000 to +0x7ff (2048 bytes) is also untouched
 * by this constructor -- not confirmed zero, just not written here.
 *
 * The repeated 120.0f value is a plausible-but-NOT-confirmed default tempo
 * (120 BPM is the near-universal synth/DAW default, and this project's own
 * CSTGMetronome constructor already confirmed a similar
 * tempo-related-constants pattern) -- flagged as speculation, not asserted
 * as the real semantic meaning; only the VALUE and OFFSET are confirmed
 * facts here.
 *
 * Total confirmed minimum size is 0xb7c (2940) bytes; the class's real
 * total size is not independently confirmed (no destructor or other
 * offset-touching method reconstructed yet to cross-check against).
 */
class CSTGEffectManager {
public:
	static CSTGEffectManager *sInstance;
	CSTGEffectManager();
	void RunEffects();
	/* Confirmed real (called from CSTGEngine::Initialize(), sec 10.58),
	 * body not reconstructed in this pass. */
	void Initialize();

	unsigned char _unrecovered_head[0x800];	/* +0x000..+0x7ff, untouched by this ctor */
	unsigned int  zeroedCounter;			/* +0x800, confirmed zeroed */
	unsigned int  zeroedTable[198];		/* +0x804..+0xb1b, confirmed zeroed */
	unsigned char _unrecovered_gap[0x48];		/* +0xb1c..+0xb63, confirmed untouched -- a real gap */
	float         defaultTempoA;			/* +0xb64, confirmed: 120.0f (see comment above) */
	float         defaultTempoB;			/* +0xb68, confirmed: 120.0f (same value, same caveat) */
	unsigned int  _tailZeroed[4];			/* +0xb6c/+0xb70/+0xb74/+0xb78, confirmed zeroed */
};

/*
 * CSTGAudioBusManager's constructor (.text+0x23460, 60 bytes) confirmed:
 * two float literals at +0x00/+0x04 (0.0006666666595265269f and 1500.0f —
 * a reciprocal pair, 1/1500 and 1500; real semantic role not determined
 * in this pass), a dword at +0x08 copied from the first 4 bytes of the
 * module-global table `STGAPILR2IndivToPhysBusId` (20 bytes total, only
 * its first dword read here), `sInstance = this`, and then two SIDE
 * EFFECTS on module-global state outside this object entirely: it resets
 * `gAllPlusHeadroom`/`gAllMinusHeadroom` (two other module globals, 16
 * bytes/4 floats each) to `{1.0f,1.0f,1.0f,1.0f}` and
 * `{-1.0f,-1.0f,-1.0f,-1.0f}` respectively (confirmed via
 * `.rodata.cst16`'s raw bytes at the two relocation targets the
 * constructor's `movaps` loads reference). Given the names, these are
 * very likely the positive/negative clipping thresholds for every
 * audio bus, reset to unity gain (no headroom reduction) whenever a
 * CSTGAudioBusManager is constructed — a real, confirmed side effect on
 * shared state, not a guess, though the exact consuming code that reads
 * these two globals elsewhere hasn't been traced in this pass. Only 12
 * bytes of this class's own fields are confirmed (+0x00..+0x0b); its
 * total object size is not yet determined (no destructor or other
 * offset-touching method reconstructed yet).
 */
class CSTGAudioBusManager {
public:
	static CSTGAudioBusManager *sInstance;
	CSTGAudioBusManager();
	void MixPerformanceOutputs();
	void LRBusIndivMirror();

	/*
	 * SetLRBusIndivAssign(int) (.text+0x234a0, 11 bytes) confirmed:
	 * looks up `STGAPILR2IndivToPhysBusId[busIndex]` (the SAME 20-byte/
	 * 5-int table the constructor reads its first element from) and
	 * overwrites `physBusIdTableHead` with the result -- i.e. this is
	 * the setter that changes which physical bus the constructor's
	 * default (index 0) choice gets replaced with. Real parameter type
	 * is a small enum (`eSTGAPILR2IndivBus`, per its mangled name) whose
	 * enumerator names/values aren't confirmed -- represented as a plain
	 * `int` index rather than inventing enumerator names with no
	 * evidence for them.
	 *
	 * Called from `CSTGGlobal::UpdateLRBusIndivAssign` via a pointer
	 * computed as `(unsigned char*)cstgGlobalThis + 4` -- confirmed at
	 * the instruction level (a `lea`, not a load, so it's genuinely
	 * computing an address into CSTGGlobal's own object, not
	 * dereferencing a stored pointer) but WHY CSTGGlobal+4 aliases as a
	 * CSTGAudioBusManager* isn't determined in this pass -- flagged as
	 * an open question, not asserted as "an embedded sub-object" or any
	 * other specific explanation. See oa_global.h.
	 */
	void SetLRBusIndivAssign(int busIndex);

	float busGainReciprocal;	/* +0x00, confirmed: 0.0006666666595265269f (~1/1500) */
	float busGainScale;		/* +0x04, confirmed: 1500.0f */
	int   physBusIdTableHead;	/* +0x08, confirmed: first dword of STGAPILR2IndivToPhysBusId;
					 * also the field SetLRBusIndivAssign() overwrites */
};

/*
 * Real, fully-fleshed classes with many other confirmed methods
 * (CSTGPlaybackBuffer: AddEvent/RemoveEvent/ProcessSubRate/Initialize/...;
 * CSTGMonitorMixerChannel: RunMonitor/StartRampIn/StartRampOut/
 * SetMonitorLevel/GetMeterLevel/...) -- reconstructing their own internals
 * is out of scope for this pass. Declared here only as opaque, empty-body
 * classes (same treatment as CEmergencyStealer) so CSTGHDRManager's own
 * confirmed array layout can be embedded and constructed faithfully.
 * Sizes are confirmed independently of these classes' own code, via
 * CSTGHDRManager::CSTGHDRManager()'s array-element address deltas (see
 * that class's own comment below) -- not guessed.
 */
class CSTGPlaybackBuffer {
public:
	CSTGPlaybackBuffer();
	unsigned char _unrecovered[88];		/* confirmed size (array stride, see CSTGHDRManager) */
};

class CSTGMonitorMixerChannel {
public:
	CSTGMonitorMixerChannel();
	unsigned char _unrecovered[172];		/* confirmed size -- see CSTGHDRManager's comment
							 * for how this differs from the array's 192-byte
							 * storage stride */
};

/*
 * CSTGHDRManager's constructor (.text+0xd3d60, 1061 bytes) -- by far the
 * largest and most structurally rich class in this codebase so far.
 * Confirmed via full relocation resolution across the whole function
 * (not just spot-checked call sites):
 *
 *   +0x004..+0x584  CSTGPlaybackBuffer[16], clean 88-byte array (confirmed:
 *                   16 constructor calls at addresses stepping by exactly
 *                   0x58, nothing else interleaved between them).
 *   +0x584..+0x5a4  32-byte gap; 3 of these bytes (+0x590/+0x594/+0x598)
 *                   are confirmed zeroed by this constructor, the rest
 *                   (+0x584..+0x590, +0x59c..+0x5a4) are untouched -- a
 *                   real, confirmed partial gap.
 *   +0x5a4..+0x11a4 CSTGMonitorMixerChannel[16], stored at a 192-byte
 *                   (0xc0) stride -- but the class's own confirmed real
 *                   size is only 172 (0xac) bytes (see below for how that
 *                   was determined). For channels 0-14 (NOT the last),
 *                   this constructor explicitly zeros 3 more dwords at
 *                   the channel's own +0xac/+0xb0/+0xb4 (i.e. right after
 *                   its true 172-byte body) -- interleaved with setting
 *                   up the NEXT channel's `this` pointer, confirmed via
 *                   relocation-resolved call targets, not guessed from
 *                   the "lea+call" shape alone. The last 4 bytes of that
 *                   20-byte per-slot pad (+0xb8..+0xbb) are a real,
 *                   confirmed gap for channels 0-14. Channel 15 (the
 *                   last) gets NONE of this tail treatment: the very next
 *                   confirmed construction (CSTGSampler) begins at
 *                   channel[15]'s start + 0xac exactly -- which is how
 *                   this pass determined CSTGMonitorMixerChannel's real
 *                   size is 172 (0xac), not the 192-byte array stride:
 *                   if the class were genuinely 192 bytes, CSTGSampler
 *                   would start 20 bytes later than it actually does.
 *
 * Confirmed but NOT implemented past this point (out of scope for this
 * pass -- reconstructing four more distinct sub-object classes' full
 * internals, several with unconfirmed real sizes, is a separate
 * undertaking): at +0x1190, a `CSTGSampler` is constructed (real size
 * unconfirmed -- the next confirmed field, a 17th standalone
 * `CSTGPlaybackBuffer`, doesn't begin until +0x18970, ~97KB later, so
 * CSTGSampler is either genuinely enormous or there's a large untouched
 * gap after it; not determined here). That 17th `CSTGPlaybackBuffer`
 * (+0x18970..+0x189c8, confirmed 88 bytes, matching the array elements'
 * size) is followed immediately by a `CSTGHDRCircularBuffer` (+0x189c8,
 * a "C2" base-object constructor variant) and then a `CSTGPlaybackEvent`
 * (+0x189fc). At +0x18a68, a `CSTGAudioInputMixerBase` is constructed via
 * its own C2 base constructor, then its vtable pointer is manually
 * overwritten to `&_ZTVN15CSTGCDAudioPlay18CCDAudioInputMixerE + 8` (the
 * confirmed Itanium ABI "+8 to skip offset-to-top/RTTI" convention seen
 * elsewhere in this project, e.g. CSTGAudioDriverInterfaceKorgUsb) --
 * meaning this embedded object is really a `CSTGCDAudioPlay::
 * CCDAudioInputMixer`, constructed via its base class's constructor then
 * vtable-patched to the derived type, all inside CSTGHDRManager. Six
 * more confirmed-zeroed bytes follow at +0x18a78..+0x18a7d. Then a real,
 * notable, independently confirmed finding: `CSTGCDAudioPlay::sInstance`
 * gets set to the address of the embedded `CSTGHDRCircularBuffer` at
 * +0x189c8 -- i.e. CSTGCDAudioPlay's own singleton pointer is aliased to
 * point INSIDE this CSTGHDRManager instance, not a separately allocated
 * CSTGCDAudioPlay object. Nine more confirmed-zeroed dwords follow (three
 * groups of three, each group followed by one confirmed-untouched dword)
 * through +0x18b00. Finally, `CSTGHDRManager::sInstance = this` --
 * reproduced faithfully in this reconstruction's constructor (functionally
 * necessary for any other code that reads the singleton), even though the
 * intervening sub-object constructions above are not.
 *
 * Confirmed minimum real object size: 0x18b04 (~101KB) -- far larger than
 * this reconstruction's own declared size, which only covers through the
 * channel array (+0x11a4) plus an explicitly-labeled placeholder for
 * everything after. `sizeof(CSTGHDRManager)` in this reconstruction does
 * NOT match the real target size -- a known, documented limitation, not
 * an oversight.
 */
class CSTGHDRManager {
public:
	static CSTGHDRManager *sInstance;
	CSTGHDRManager();
	void ProcessHDRRecord();
	/*
	 * ProcessCommands() (sec 10.144, `.text+0xd5dd0`, 41 bytes) confirmed:
	 * calls these three confirmed-real siblings on `this`, in this exact
	 * order. None of the three are implemented in this pass (see
	 * managers.cpp).
	 */
	void ProcessPlaybackCommands();	/* .text+0xd5950, confirmed real, deferred */
	void ProcessRecordCommands();		/* .text+0xd5b20, confirmed real, deferred */
	void ProcessSamplerCommands();		/* .text+0xd5c50, confirmed real, deferred */
	void ProcessCommands();
	/* Confirmed real (called from CSTGEngine::Initialize(), sec 10.58),
	 * body not reconstructed in this pass. */
	void Initialize();

	unsigned char _unrecovered_head[4];		/* +0x000..+0x003, confirmed untouched by this ctor
							 * (the array itself starts at +0x004, not +0x000) */
	CSTGPlaybackBuffer playbackBuffers[16];	/* +0x004..+0x584, confirmed clean array */
	unsigned char _unrecovered_gap[0x20];		/* +0x584..+0x5a4, partially confirmed zeroed (see ctor) */
	unsigned char monitorMixerChannelSlots[16][0xc0]; /* +0x5a4..+0x11a4, see class comment for the
							    * 172-vs-192-byte size/stride distinction */
	/* Everything from CSTGSampler (+0x1190 in the real object) onward is
	 * confirmed to exist (see class comment) but not reconstructed here.
	 * This placeholder covers none of that real space -- this class's
	 * sizeof() is intentionally smaller than the real ~101KB object. */
};

/* CSTGMonitorMixer's constructor (.text+0x69000, 6 bytes) confirmed to do
 * exactly one thing: `sInstance = this;` -- the smallest manager
 * constructor found in this codebase so far. Total object size not
 * determined (no other offset-touching method reconstructed here). */
class CSTGMonitorMixer {
public:
	static CSTGMonitorMixer *sInstance;
	CSTGMonitorMixer();
	void RunMonitors();
	/* Confirmed real (called from CSTGEngine::Initialize(), sec 10.58),
	 * body not reconstructed in this pass. */
	void Initialize();
};

/*
 * CSTGFileOpener's constructor (.text+0x119bd0, 953 bytes) confirmed
 * layout, cross-checked against its confirmed 544-byte total size exactly
 * (0x10 header + 32*16 slots + 16 ring-control = 0x220 = 544):
 *   +0x00/+0x04/+0x08  header fields, zeroed (+0x0c untouched)
 *   +0x10..+0x20c      32 identical 16-byte "slots", each zeroing its own
 *                      +0/+4/+8 (that slot's +0xc left untouched) -- same
 *                      shape as the class-level header, repeated; real
 *                      per-slot semantics not determined in this pass
 *   +0x210/+0x214/+0x218  a ring buffer's base-pointer/write-index/
 *                         read-index, zeroed
 *   +0x21c             that ring buffer's capacity -- CONFIRMED NOT
 *                      zeroed by the constructor (set later, presumably
 *                      by its own not-yet-reconstructed Initialize())
 *
 * `ProcessCommands()` (.text+0x11a870, 201 bytes) confirmed to walk that
 * ring buffer: `capacity = [+0x21c]`, `base = [+0x210]`, entries are 8
 * bytes each (`status:1 byte, data-or-vtable-object-ptr:4 bytes` -- exact
 * shape not fully resolved), dispatched by status byte to one of three
 * virtual calls (vtable slots +0x8/+0x10, or a special status==2 path that
 * re-enqueues into a second, globally-shared ring buffer). `CSTGCDWorker::
 * ProcessCommands` (.text+0x11b720, 124 bytes) is confirmed to be the
 * exact same ring-buffer-of-status-tagged-records shape, just at different
 * offsets (+0x224 base/+0x22c write-idx/+0x230 read-idx/+0x234 capacity)
 * and with a simpler single-dispatch body -- a real, confirmed pattern
 * shared across the "file daemon" classes, not the uniform +0x00/+0x04/
 * +0x08-across-every-class theory floated when only the smaller daemons
 * (sec 10.14) had been looked at; each daemon's ring buffer lives at its
 * own offset, not a fixed one. Neither `ProcessCommands()` is implemented
 * here: both dispatch through vtables on objects of unrecovered types.
 */
class CSTGFileOpener {
public:
	static CSTGFileOpener *sInstance;
	CSTGFileOpener();
	void ProcessCommands();
	/* Confirmed real (called from CSTGEngine::Initialize(), sec 10.58),
	 * body not reconstructed in this pass. */
	void Initialize();
	unsigned char _unrecovered[544];
};

/*
 * CSTGFileCloser/CSTGHDRFileReader/CSTGHDRFileWriter/CSTGStreamingFileReader/
 * CSTGSamplingDaemon's constructors (below, engine.cpp is not the home for
 * these -- see managers.cpp) all zero the SAME first three fields (+0x00,
 * +0x04, +0x08) before anything class-specific -- a strong signal of a
 * shared base class or common leading sub-object (plausibly a pending-
 * command queue head/tail/count, given every one of these classes is a
 * "file daemon" with a `ProcessCommands()` the same shape as
 * `CSTGQuadList`'s head/tail/count -- not confirmed, just a reasonable
 * hypothesis worth a future pass checking `ProcessCommands()` itself).
 * Each class's padding array below is sized to its CONFIRMED real object
 * size (`CSTGBankMemory::AllocAligned`'s argument in `CSTGEngine::
 * Initialize()`, MASTER_REFERENCE.md sec 10.13), not just what the
 * constructor happens to touch -- for several of these the constructor
 * touches noticeably less than the full confirmed size (remaining bytes'
 * meaning not determined in this pass), which is itself a useful fact to
 * keep visible rather than pad silently to a smaller, "just enough" size.
 */

class CSTGFileCloser {
public:
	static CSTGFileCloser *sInstance;
	CSTGFileCloser();
	void ProcessCommands();
	void Initialize();	/* confirmed real, body not reconstructed, sec 10.58 */
	unsigned char _unrecovered[32];	/* confirmed size; ctor touches +0x00..+0x18 */
};

class CSTGHDRFileReader {
public:
	static CSTGHDRFileReader *sInstance;
	CSTGHDRFileReader();
	void ProcessCommands();
	void Initialize();	/* confirmed real, body not reconstructed, sec 10.58 */
	unsigned char _unrecovered[68];	/* confirmed size; ctor touches +0x00..+0x10 */
};

class CSTGHDRFileWriter {
public:
	static CSTGHDRFileWriter *sInstance;
	CSTGHDRFileWriter();
	void ProcessCommands();
	void Initialize();	/* confirmed real, body not reconstructed, sec 10.58 */
	unsigned char _unrecovered[20];	/* confirmed size; ctor leaves a real 4-byte gap at +0x0c */
};

class CSTGStreamingFileReader {
public:
	static CSTGStreamingFileReader *sInstance;
	CSTGStreamingFileReader();
	void ProcessCommands();
	/* Confirmed real arg (regparm edx): 0x8000 (32768) at this
	 * project's own only confirmed call site. Body not reconstructed. */
	void Initialize(unsigned long bufferSize);
	unsigned char _unrecovered[56];	/* confirmed size; ctor touches +0x00..+0x1c */
};

/*
 * CSTGCDWorker's constructor (.text+0x11b2a0, 77 bytes) confirmed to zero
 * +0x00(dword)/+0x04(byte)/+0xc/+0x10/+0x20/+0x224/+0x228/+0x22c/+0x230,
 * cross-checked against its confirmed 568-byte total size: the highest
 * touched offset (+0x230, a dword ending at +0x234) leaves exactly 4 bytes
 * (+0x234..+0x238) untouched -- that's the ring buffer's capacity field
 * `ProcessCommands()` uses as a modulus divisor (same "capacity set later,
 * not by the ctor" pattern as CSTGFileOpener, see its own comment above).
 * `ProcessCommands()` itself (.text+0x11b720, 124 bytes) is not
 * implemented -- see CSTGFileOpener's comment for why (vtable dispatch
 * into unrecovered types).
 */
class CSTGCDWorker {
public:
	static CSTGCDWorker *sInstance;
	CSTGCDWorker();
	void ProcessCommands();
	void Initialize();	/* confirmed real, body not reconstructed, sec 10.58 */
	unsigned char _unrecovered[568];
};

class CSTGSamplingDaemon {
public:
	static CSTGSamplingDaemon *sInstance;
	CSTGSamplingDaemon();
	void ProcessCommands();
	void Initialize();	/* confirmed real, body not reconstructed, sec 10.58 */
	unsigned char _unrecovered[16];	/* confirmed size; ctor touches +0x00..+0x08 */
};

/* Constructor is a single `sInstance = this` with no field writes at all --
 * confirmed by disassembly (6 bytes total, no other instructions). Real
 * 72-byte object presumably has other fields, but none are constructor-
 * initialized (zero-initialized by its CSTGBankMemory allocation, or set
 * later by its own Initialize()). */
class CSTGDiskCostManager {
public:
	static CSTGDiskCostManager *sInstance;
	CSTGDiskCostManager();
	void Initialize();	/* confirmed real, body not reconstructed, sec 10.58 */
	unsigned char _unrecovered[72];
};

/*
 * CSTGMetronome's constructor starts with `AND BYTE PTR [this],0xfa`
 * (clearing bits 0 and 2 of a flags byte, not setting the whole byte) --
 * confirmed evidence of a base class or preceding sub-object whose own
 * constructor already ran and left other bits meaningful; not resolved in
 * this pass. Every other field is a plain unconditional write. The
 * constructor's last write lands exactly on the confirmed object's final
 * 4 bytes (`+0x28`, size 44) -- a clean signal this class has no
 * additional trailing fields beyond what's shown here.
 */
class CSTGMetronome {
public:
	static CSTGMetronome *sInstance;
	CSTGMetronome();
	unsigned char _unrecovered[44];
};

/* All writes are plain zeroing (byte or dword), and the last one lands
 * exactly on the confirmed object's final 4 bytes (+0x38, size 60) -- same
 * clean no-trailing-fields signal as CSTGMetronome. NOT every byte is
 * touched, though: confirmed real gaps at +0x02-0x03, +0x0a-0x0b,
 * +0x19-0x1b, +0x29-0x2b, +0x35-0x37 (a mix of byte/dword writes with
 * small skipped ranges between them, not one uniform sweep) -- see
 * managers.cpp for the exact per-offset list. */
class CSTGTempoUtils {
public:
	static CSTGTempoUtils *sInstance;
	CSTGTempoUtils();
	unsigned char _unrecovered[60];
};

/* ---- Minimal opaque singletons touched only by the destructor ---- */

/*
 * CSTGMidiPortManager -- CONFIRMED to have no constructor function at all,
 * not merely "not yet found". Checked exhaustively, not assumed:
 *   - No `CSTGMidiPortManagerC1Ev`/`C2Ev` symbol anywhere in OA.ko's ELF
 *     symbol table (defined OR undefined/imported).
 *   - Not in CSTGEngine::Initialize()'s confirmed ~44-entry construction
 *     table (MASTER_REFERENCE.md sec 10.13) -- every other manager in
 *     that table gets an explicit `AllocAligned`/`operator new` +
 *     constructor-call pair; this class simply never appears.
 *   - `CSTGMidiPortManager::sInstance` (a real, defined GLOBAL symbol) has
 *     ZERO relocations writing to it anywhere in OA.ko -- confirmed by
 *     scanning every relocation in the whole binary, not spot-checked.
 *   - The real destructor (.text+0xf5280, 264 bytes) operates ENTIRELY on
 *     the class's two STATIC array members (`sMidiInPorts`/
 *     `sMidiOutPorts`, each 16 bytes = 4 pointers, confirmed via absolute
 *     `mov eax, ds:CONST`-style addressing, never `[this+OFFSET]`) --
 *     this class appears to have NO per-instance state at all.
 *
 * Conclusion: this class's implicit default constructor is genuinely
 * empty (nothing per-instance to initialize), which is exactly why GCC
 * never emitted a standalone C1Ev/C2Ev symbol for it -- not a gap in
 * this project's search. All of its real "bring-up" work happens in the
 * separate, already-named `CSTGMidiPortManager::Initialize()`
 * (.text+0xf4f60, 790 bytes, confirmed substantial: buffer clears,
 * table-driven lookups, and more) -- a genuinely different pattern from
 * every other manager class in this file, where the constructor does the
 * bulk of the setup. Where `sInstance` actually gets assigned (if ever,
 * within this specific firmware image) was not determined -- possibly a
 * companion kernel module (STGEnabler.ko or similar) rather than OA.ko
 * itself, though that wasn't independently confirmed either. Reconstructing
 * `Initialize()` itself would be the natural equivalent next step for this
 * class, but that's a different task from "find the missing constructor",
 * which is now closed.
 */
class CSTGMidiPortManager {
public:
	static CSTGMidiPortManager *sInstance;
	~CSTGMidiPortManager();
	void Initialize();	/* .text+0xf4f60, 790 bytes -- confirmed real
				 * and substantial (buffer clears, table-driven
				 * lookups), body not reconstructed in this pass. */

	/*
	 * WriteSTGMidiOutQueue(const unsigned char*, unsigned int) (sec
	 * 10.73/10.145, `.text+0xf57d0`, 53 bytes) confirmed via relocation
	 * from CSTGGlobal::UpdateMFX/IFX/TFXDisable, all three calling it
	 * identically with a real 3-byte MIDI Control Change message:
	 * `{0xB0|channel, ccNumber, value}`. Body fully confirmed by
	 * disassembly (sec 10.145): no-op if `CSTGGlobal::sInstance->
	 * fieldAt(0x6ac)` (a confirmed real gate byte) is nonzero; otherwise
	 * forwards to the already-reconstructed `CSTGMidiQueueWriter::
	 * Write(data, length, false)` (sec 10.83) on an embedded
	 * `CSTGMidiQueueWriter` at `this->fieldAt(0x138)` -- `data`/`length`
	 * pass through untouched from this method's own regparm(3)
	 * `edx`/`ecx`. A genuine REFINEMENT of this class's own "no
	 * per-instance state" conclusion above (drawn from the destructor
	 * alone): the destructor simply doesn't happen to touch this
	 * embedded `+0x138` sub-object, a confirmed real gap, not a
	 * contradiction. **Deliberately NOT promoted to a real body in this
	 * pass**: `test_global.cpp` already has two SEPARATE, independent
	 * mocks/counters for this method and for `CSTGMidiQueueWriter::
	 * Write` (~30 assertions across the file), and this method calling
	 * through to that one for real would require rewiring every one of
	 * them onto a single counter -- correctly out of scope for a
	 * same-pass "small function batch," left as its own dedicated future
	 * task rather than rushed.
	 */
	void WriteSTGMidiOutQueue(const unsigned char *data, unsigned int length);

	/* NotifyNKS4TestMode() (.text+0xf5390, 115 bytes -- confirmed via
	 * relocation from CSTGGlobal::SetNKS4TestModeFlag) confirmed real:
	 * a table-driven lookup + up to 3 indirect calls through fields of
	 * a not-yet-identified structure, substantially more involved than
	 * this class's other confirmed methods -- deliberately deferred,
	 * own body not reconstructed in this pass. */
	void NotifyNKS4TestMode();
};

/*
 * CPowerOffTimer's constructor (.text+0x5d860, 58 bytes) confirmed and a
 * clean cross-check with its own destructor (sec 10.13): it allocates a
 * real mutex via `rtwrap_malloc(get_sizeof_rtwrap_pthread_mutex())` and
 * stores the result at +0x18, then `rtwrap_pthread_mutex_init(mutex, 0)`
 * -- the exact allocate/init counterpart to the destructor's confirmed
 * `rtwrap_pthread_mutex_destroy`+`rtwrap_free` teardown of that same
 * field. Also zeroes a byte at +0x00 and a dword at +0x14. Real heap-
 * `new`'d (not CSTGBankMemory-placed), confirmed by Initialize()'s
 * `_Znwj` call for it -- so the destructor's matching `operator delete`
 * (not just an in-place dtor) is faithful, not a guess. Confirmed
 * 28-byte total size leaves +0x1c..+0x1b (nothing -- 0x18+4=0x1c is
 * exactly the end) with no gap at all: this class's last field is the
 * mutex pointer itself.
 */
class CPowerOffTimer {
public:
	static CPowerOffTimer *sInstance;
	CPowerOffTimer();
	void Initialize();	/* confirmed real, body not reconstructed, sec 10.58 */
	unsigned char _unrecovered[28];
};

/*
 * CSTGMessageProcessor's constructor (.text+0xebb60, 5930 bytes) -- by
 * FAR the most heterogeneous class reconstructed in this project so far
 * (664 relocations across the whole function, more than any other class
 * combined). No real vtable of its OWN (unlike CSTGAudioManager), so
 * plain raw-offset-cast bytes are safe here -- no host/target ABI hazard
 * to work around.
 *
 * Confirmed structure (full relocation resolution across the whole
 * function):
 *   - Three "unsolicited message" sender/message pairs, one each for
 *     ProgramSlot/ControllerInfo/IFX (`CSTGProgramSlotUnsolMsgSender`+
 *     `CSTGProgramSlotUnsolMsg`, etc. -- 6 distinct vtabled types total,
 *     confirmed via their own `_ZTV*` relocations). Each Sender embeds a
 *     32-element `CSTGDelayedMsg` array (a message queue) -- confirmed
 *     stride 0x2c (44 bytes) for the ProgramSlot sender, 0x28 (40 bytes)
 *     for the other two, a real confirmed difference between sender
 *     types, not assumed to be uniform. Spans roughly `+0x00..+0x1040`
 *     (the last array element alone ends at `0xb40+32*0x28=0x1040`).
 *   - `CSTGMessageProcessor::sInstance = this` is confirmed to be set
 *     immediately after those three pairs -- a genuine, confirmed
 *     deviation from every other manager in this file, where `sInstance`
 *     is set LAST. Everything below happens with `sInstance` already
 *     valid.
 *   - `+0x64`: a real `CEffectorDatabase*`, heap-`new`'d here (confirmed
 *     via `_Znwj` + `CEffectorDatabase::CEffectorDatabase(int,
 *     CEffector*)`, seeded with a global placeholder `g_oNoEffect`).
 *   - ~15 `CSTGBankMemory::AllocAligned()`-backed buffers of various
 *     small sizes, and 14 distinct `CSTGXxxMsgHandler` sub-object
 *     constructions (Combi/Control/Global/Patch/VoiceModelParam/
 *     ProgramSlot/Program/EffectParam/EffectSlot/EffectMgr/Calibration/
 *     FrontPanel/HDRTrack, plus `CSTGNullMsgHandler`, which also sets
 *     its OWN separate `sInstance`) -- one handler type per message
 *     category this processor dispatches to, matching the class's name.
 *   - **198** `CEffectorDatabase::Register(int, CEffector*, char
 *     const*)` calls and **8** `CMOSSAlgorithmDatabase::Register(
 *     CMOSSAlgorithm const*)` calls -- confirmed to register every
 *     built-in DSP effect type and MOSS algorithm with their respective
 *     EXTERNAL, separately-allocated databases (each `CEffector`
 *     instance is individually heap-`new`'d, not embedded in `this`).
 *     This is why the function is 5930 bytes of code despite the object
 *     itself being comparatively modest: registering 198 effects
 *     dominates the CODE size but barely touches `this`'s own layout
 *     (confirmed by scanning every `[ebx+CONST]`-style access across the
 *     whole function -- none appear past the unsol-msg-pairs region).
 *
 * None of the above past the three unsol-msg pairs is reconstructed in
 * this pass -- the sheer number of distinct sub-object types (7 new
 * vtabled classes for the pairs alone, 14 more for the handlers, plus
 * two external databases) made full reconstruction genuinely out of
 * scope, more so than any earlier partial class. What IS implemented:
 * `sInstance` (functionally necessary, and confirmed to happen at this
 * specific point in the sequence) and the class's overall confirmed
 * minimum size.
 *
 * Confirmed minimum size: `0x1040` (a lower bound from the last unsol-msg
 * array element alone; the real total, including the trailing
 * `CSTGIFXUnsolMsg`'s own fields and everything after `+0x64`, is
 * somewhat larger but not precisely pinned down in this pass -- stated
 * as a lower bound, not asserted as exact, unlike this project's usual
 * "sizeof matches exactly" claims for smaller classes).
 */
class CSTGMessageProcessor {
public:
	static CSTGMessageProcessor *sInstance;
	CSTGMessageProcessor();
	~CSTGMessageProcessor();

	unsigned char _unrecovered[0x1040];	/* confirmed MINIMUM size -- see class comment */
};

/* Confirmed abstract base with a real vtable -- the destructor tears it
 * down via a virtual call (vtable slot +0x4, the Itanium ABI "deleting
 * destructor" slot), not a direct non-virtual destructor call, because the
 * real allocated object is the derived CSTGAudioDriverInterfaceKorgUsb
 * (confirmed via Initialize()'s `_Znwj` + that derived class's own
 * constructor call). */
class CSTGAudioDriverInterface {
public:
	static CSTGAudioDriverInterface *sInstance;
	virtual ~CSTGAudioDriverInterface() = 0;
};

/*
 * CSTGAudioDriverInterfaceKorgUsb's constructor (.text+0x340090, 57 bytes)
 * confirmed: calls the base `CSTGAudioDriverInterface` constructor, then
 * sets +0x40/+0x44 to `6` each (channel counts?), +0x38 to a self-pointer
 * (`this` -- a callback-registration context argument), and +0x3c to a
 * pointer to this class's own `Callback(void*)` member function. A near
 * miss caught before writing this: the raw disassembly shows `mov
 * DWORD PTR [ebx],0x8` and `mov DWORD PTR [ebx+0x3c],0x0`, which read like
 * two more literal writes -- but both addresses carry `.rel.text`
 * relocations (on `_ZTV31CSTGAudioDriverInterfaceKorgUsb` and
 * `Callback` respectively), meaning those "immediates" are actually
 * relocated addresses (the vtable pointer setup, standard Itanium "+8 to
 * skip the offset-to-top/RTTI slots" convention, and the callback function
 * pointer). Reading the disassembly's literal bytes without checking for a
 * relocation there would have produced two wrong field values -- the same
 * class of mistake this project's methodology exists to catch (checked
 * before committing this one, not caught after like sec 10.14's docs). The
 * vtable-pointer store itself isn't hand-written below: C++ does that
 * automatically for a real derived class with virtual functions.
 */
class CSTGAudioDriverInterfaceKorgUsb : public CSTGAudioDriverInterface {
public:
	CSTGAudioDriverInterfaceKorgUsb();
	/* A real destructor exists (.text+0x33f7e0, 69 bytes, confirmed via
	 * the ELF symbol table -- required anyway since the base class's
	 * destructor is pure virtual) but its body isn't reconstructed in
	 * this pass; defined empty in managers.cpp only so this class is
	 * concrete/instantiable. */
	virtual ~CSTGAudioDriverInterfaceKorgUsb();
	void Callback(void *arg);

	/*
	 * Deliberately NAMED members here, not a raw `_unrecovered[]` byte
	 * blob accessed via `(this)+OFFSET` casts (the pattern used
	 * everywhere else in this file): this class has a real inherited
	 * vtable pointer, which is 4 bytes on the confirmed 32-bit target but
	 * 8 bytes on a 64-bit host build -- a hardcoded target-relative offset
	 * like `+0x38` would land 4 bytes short of the real field on host,
	 * corrupting whatever comes after it. Named members let each build's
	 * own compiler place them correctly after the (target- or
	 * host-width) vtable pointer, consistently within that build, which
	 * is what actually matters: the real target build is what has to be
	 * byte-correct, and the host build only needs internally-consistent
	 * relative placement to KAT-test the confirmed field *values*.
	 * +0x34..+0x37 (target-relative, between the confirmed 0x00-vtable
	 * and the first named field here) is unaccounted-for space --
	 * real content there not determined in this pass.
	 */
	unsigned char _pad04[0x38 - 0x04];
	void *selfPtr;			/* +0x38, confirmed: set to `this` */
	void *callbackFnPtr;		/* +0x3c, confirmed: &Callback trampoline */
	unsigned int channelsIn;	/* +0x40, confirmed: 6 */
	unsigned int channelsOut;	/* +0x44, confirmed: 6 */
};

/*
 * CSTGAudioManager's constructor (.text+0x649d0, 5785 bytes) -- the third
 * class (after CSTGHDRManager/CSTGVoiceAllocator) reconstructed only
 * partially, though at a far more modest confirmed size (0x455c/~17.3KB,
 * vs. their ~101KB/~281KB). Real heap-`new`'d (confirmed via
 * `Initialize()`'s `_Znwj` call), so the destructor does dtor-then-
 * `operator delete`, not an in-place-only dtor like the
 * CSTGBankMemory-placed managers elsewhere in this file.
 *
 * Confirmed to have a REAL VTABLE (`_ZTV16CSTGAudioManager` relocation on
 * the object's own vtable-pointer store) -- this class MUST stay
 * polymorphic (a real virtual destructor below) so the compiler places
 * the vtable pointer correctly on every build; per this project's own
 * established host/target ABI rule (see e.g.
 * CSTGAudioDriverInterfaceKorgUsb), fields after a real vtable are named
 * C++ members here, not raw offset-cast byte arrays, since the vtable
 * pointer is 4 bytes on the confirmed 32-bit target but 8 on a 64-bit
 * host build.
 *
 * Confirmed structure (full relocation resolution across the whole
 * function):
 *   - Two real mutex+condvar pairs (`+0xa48..+0xa5c`, target-relative,
 *     i.e. right after the vtable pointer): same allocate/init shape as
 *     `CPowerOffTimer`'s and `CSTGVoiceAllocator`'s already-confirmed
 *     mutexes (`get_sizeof_rtwrap_pthread_mutex`/`rtwrap_malloc`/
 *     `rtwrap_pthread_mutex_init`), plus a matching
 *     `get_sizeof_rtwrap_pthread_cond`/`rtwrap_pthread_cond_init` pair
 *     for a real condition variable -- TWO complete mutex+cond pairs, one
 *     right after the other. Fully implemented.
 *   - `+0xa60..+0x454c` (~15KB): confirmed to exist but NOT reconstructed
 *     in this pass. Contains: a runtime branch reading a CPU core count
 *     (confirmed via a `CSTGCPUInfo::sInstance` relocation) that clamps
 *     to a small range and drives a short per-core array; and 13 profiler
 *     "slots", each constructing a `CProfiler` + `CDurationStats` pair
 *     (confirmed via `_ZTV9CProfiler`/`_ZTV14CDurationStats` vtable-
 *     pointer relocations, 2 and 1 per slot respectively) linked into a
 *     shared `CProfiler::sListOfProfilers` list, with 3 of the 13 slots
 *     also constructing a `CSTGFrontPanelStatusReporter`. The slots are
 *     NOT uniformly spaced (confirmed via the relocation offsets
 *     themselves -- deltas of `0x10f` and `0x15f` bytes, not a single
 *     constant stride), so this is not a simple C++ array; fully tracing
 *     13 individually-sized sub-object groups was out of scope for this
 *     pass. Left as one explicitly-labeled unrecovered region rather than
 *     guessed at.
 *   - `+0x454c..+0x455c`: four confirmed trailing scalars -- `0x100`
 *     (256), `0xff` (255), `0.00390625f` (1/256 exactly), `1.0f`.
 *     Plausibly a lookup-table size/mask/reciprocal/unity-gain quadruple;
 *     flagged as speculation, not asserted as fact -- only the values and
 *     offsets are confirmed.
 *   - `CSTGAudioManager::sInstance = this` is confirmed to happen BEFORE
 *     the trailing-scalars/CPU-count logic in instruction order (not
 *     strictly last, unlike every other manager in this file) --
 *     reproduced at the end of this reconstruction's constructor anyway,
 *     since the exact ordering only matters if something the unimplemented
 *     middle section touches reads `sInstance` reentrantly, which was not
 *     confirmed either way.
 *
 * Total confirmed size (target, including the 4-byte vtable pointer):
 * 0x455c (~17.3KB). This reconstruction's own declared member data spans
 * 0x4558 bytes (excluding the compiler-placed vtable pointer), so
 * `sizeof(CSTGAudioManager)` matches the real confirmed size exactly on
 * the 32-bit target build (host builds differ only by the vtable
 * pointer's own width, the same tolerated/documented gap as every other
 * vtabled class here).
 */
class CSTGAudioManager {
public:
	static CSTGAudioManager *sInstance;
	CSTGAudioManager();
	virtual ~CSTGAudioManager();

	/* Confirmed real (via relocation, .text+0x66f00, 235 bytes) -- see
	 * oa_audio_start.h/src/init/audio_start.cpp for the full
	 * reconstruction. */
	char StartAudioEngine();

	/* Thread entry points -- ONLY ever used as function-pointer VALUES
	 * (passed to CSTGThread::CreateRealTimeWithCPUAffinity, never
	 * called directly), own bodies not reconstructed. */
	static void *ASKThreadRoutine(void *arg);
	static void *AudioManagerThreadRoutine(void *arg);

	unsigned char _unrecovered_head[0xa44];	/* vtable(target:+0x00) then this reaches +0xa48 */
	unsigned int  mutexCondFlag1;			/* +0xa48, confirmed zeroed */
	unsigned int  mutex1Handle;			/* +0xa4c, confirmed real mutex handle */
	unsigned int  cond1Handle;			/* +0xa50, confirmed real condvar handle */
	unsigned char mutexCondFlag2;			/* +0xa54, confirmed zeroed (byte) */
	unsigned char _unrecovered_gap1[3];		/* +0xa55..+0xa57, confirmed untouched */
	unsigned int  mutex2Handle;			/* +0xa58, confirmed real mutex handle */
	unsigned int  cond2Handle;			/* +0xa5c, confirmed real condvar handle */
	unsigned char _unrecovered_middle[0x454c - 0xa60]; /* +0xa60..+0x454c, confirmed to
							     * exist (CPU-count logic + 13
							     * profiler slots), not reconstructed */
	unsigned int  trailingCount;			/* +0x454c, confirmed: 0x100 (256) */
	unsigned int  trailingMask;			/* +0x4550, confirmed: 0xff (255) */
	float         trailingReciprocal;		/* +0x4554, confirmed: 0.00390625f (1/256) */
	float         trailingUnity;			/* +0x4558, confirmed: 1.0f */

	/*
	 * EXTENDED (2026-07-02, while reconstructing the real
	 * CSTGAudioManager::StartAudioEngine() -- see oa_audio_start.h):
	 * the class's real confirmed minimum size grows past this file's
	 * earlier +0x455c boundary. `+0x4560` is READ (a priority value,
	 * presumably constructor-set, not written by StartAudioEngine
	 * itself) and `+0x4564`/`+0x4568`/`+0x456c` are WRITTEN by
	 * StartAudioEngine (0, 0x989680, 0 respectively) -- kept as an
	 * opaque trailing blob rather than named individual members since
	 * only the write side is independently confirmed here, matching
	 * this class's own established "named where cleanly recovered,
	 * opaque blob otherwise" convention. Also +0x4/+0x10/+0x14/+0x20
	 * (a linked-list head) and +0xa65 (a "running" flag byte) are
	 * touched by StartAudioEngine but already fall within the existing
	 * `_unrecovered_head`/`_unrecovered_middle` blobs above, needing no
	 * further struct changes.
	 */
	unsigned char _unrecovered_tail[0x4570 - 0x455c];
};

/*
 * Real, fully-fleshed class (many other confirmed methods, same treatment
 * as CSTGMonitorMixerChannel) -- declared here only as an opaque,
 * empty-body class so CSTGVoiceAllocator's array of 16 can be
 * placement-constructed. Confirmed size (6284/0x188c bytes) comes from
 * CSTGVoiceAllocator::CSTGVoiceAllocator()'s own array-element address
 * deltas (16 constructor calls, consistent stride), not guessed.
 */
class CSTGSlotState {
public:
	CSTGSlotState();
	unsigned char _unrecovered[0x188c];
};

/*
 * CSTGVoiceAllocator's constructor (.text+0x4b750, 4491 bytes) -- the
 * second class in this project (after CSTGHDRManager) where full
 * relocation resolution across the whole function revealed an object far
 * too large/complex to fully reconstruct in one pass. Confirmed minimum
 * size: 0x44eac (~281KB) -- even larger than CSTGHDRManager's ~101KB,
 * consistent with a voice allocator holding full per-voice polyphony
 * state for every voice the engine can allocate.
 *
 * Confirmed structure (four distinct array regions, precisely bounded by
 * relocation-resolved call targets and instruction operands, not guessed):
 *
 *   +0x0000..+0x0898  A 50-element, 44-byte-stride array of small
 *                     self-referencing records: each zeroes 8 fields and
 *                     sets one field (+0x24 relative) to its own address
 *                     -- the classic "empty circular/self-linked list
 *                     node" init pattern. No sub-object constructor is
 *                     called for these; the writes are inline.
 *   +0x0898..+0x0bb8  32-byte... actually 0x320 (800) byte gap, entirely
 *                     untouched by this constructor.
 *   +0x0bb8..+0xb478  A 400-element, 108-byte (0x6c) stride array. Each
 *                     element zeroes several small field groups and sets
 *                     4 fields (+0x34/+0x44/+0x54/+0x64 relative) to the
 *                     element's OWN base address -- confirmed via the
 *                     exact instruction operands, not the same "points at
 *                     itself" shape as the first array (different
 *                     relative offsets), so likely 4 sub-records each
 *                     needing an "owner" back-pointer to their shared
 *                     parent, rather than a single self-referencing node.
 *   +0xb478..+0x23d38 CSTGSlotState[16], a clean array at the class's
 *                     own confirmed 0x188c-byte size (16 constructor
 *                     calls, consistent stride, nothing interleaved).
 *   +0x23d38..+0x3a7b8 A 400-element, 232-byte (0xe8) stride array whose
 *                     per-element body is far too detailed to fully trace
 *                     in this pass (dozens of individual byte/word/dword
 *                     writes per element, including 5 copies of
 *                     `CSTGMultisampleBankUUIDBase::sLegacyBankPrefix`
 *                     per element) -- confirmed to exist at this exact
 *                     offset/count/stride, contents NOT reconstructed.
 *   +0x3a7b8..+0x44ea8 Confirmed to exist (a nested loop -- an outer
 *                     counter wrapping 10 `CModelVoiceRequirementsData::
 *                     Clear()` calls per iteration, each iteration's
 *                     sub-elements 0x2f4/756 bytes apart) but NOT
 *                     reconstructed in this pass -- the outer loop's own
 *                     trip count and the surrounding per-iteration field
 *                     layout were not fully traced.
 *   +0x44ea8           A real recursive `pthread_mutex_t` handle
 *                     (`rtwrap_malloc` + `rtwrap_pthread_mutex_init`,
 *                     with `rtwrap_pthread_mutexattr_init`/`_settype`/
 *                     `_destroy` around it using
 *                     `get_pthread_recursive_attr_constant` for the
 *                     recursive-mutex attribute -- the same allocate/init
 *                     shape as `CPowerOffTimer`'s already-confirmed
 *                     mutex, but recursive here instead of default type).
 *                     This is the LAST member-touching write in the real
 *                     constructor -- confirmed, not assumed, since the
 *                     very next instruction is `CSTGVoiceAllocator::
 *                     sInstance = this`.
 *
 * `sizeof(CSTGVoiceAllocator)` in this reconstruction (0x44eac) matches
 * the real confirmed minimum size exactly (unlike CSTGHDRManager, where
 * the reconstruction's declared size was deliberately smaller than the
 * real object) -- every byte up to the mutex field is accounted for by
 * an explicit member or an explicitly-labeled unrecovered gap, even
 * though two of those regions' internal contents aren't reconstructed.
 */
class CSTGVoiceAllocator {
public:
	static CSTGVoiceAllocator *sInstance;
	CSTGVoiceAllocator();
	~CSTGVoiceAllocator();
	void Initialize();	/* confirmed real, body not reconstructed, sec 10.58 */

	/* Confirmed real (via relocation from CSTGGlobal::
	 * UpdateConvertPosition, sec 10.70), own body not reconstructed in
	 * this pass. */
	void StealAllVoices();

	/*
	 * EmergencyFreeVoiceList(TLinkedList<TListLink<CSTGVoice>>*) (sec
	 * 10.138, confirmed via relocation from CSTGSlotVoiceData::
	 * EmergencyFreeAllVoices, called twice there on `this+0x44`/
	 * `this+0x50`) confirmed real, deliberately deferred extern -- own
	 * body not reconstructed in this pass. Real parameter type not
	 * modeled -- represented as `void*` per this project's established
	 * convention for not-fully-modeled template types.
	 */
	void EmergencyFreeVoiceList(void *list);

	/*
	 * StealVoiceList(TLinkedList<TListLink<CSTGVoice>>*) (sec 10.140,
	 * confirmed via relocation from CSTGSlotVoiceData::Steal, called
	 * twice there on `this+0x44`/`this+0x50`, the same shape as
	 * `EmergencyFreeVoiceList` above) confirmed real, deliberately
	 * deferred extern -- own body not reconstructed in this pass.
	 */
	void StealVoiceList(void *list);

	unsigned char selfRefNodes[50][0x2c];		/* +0x0000..+0x0898, confirmed self-ref pattern */
	unsigned char _unrecovered_gap1[0x320];	/* +0x0898..+0x0bb8, confirmed untouched */
	unsigned char ownerBackRefRecords[400][0x6c];	/* +0x0bb8..+0xb478, confirmed pattern (see above) */
	CSTGSlotState slotStates[16];			/* +0xb478..+0x23d38, confirmed clean array */
	unsigned char _unrecovered_bigArray[400][0xe8]; /* +0x23d38..+0x3a7b8, confirmed count/stride only */
	unsigned char _unrecovered_tail[0x44ea8 - 0x3a7b8]; /* +0x3a7b8..+0x44ea8, confirmed to exist, not reconstructed */
	unsigned int  requirementsMutex;		/* +0x44ea8, confirmed real recursive pthread mutex handle */
};

/*
 * CLoadBalancer's constructor (.text+0x60b70, 281 bytes) confirmed to
 * start by constructing a `CEmergencyStealer` -- a real, separately-named
 * class (confirmed via `CEmergencyStealer::CEmergencyStealer()`'s own
 * relocation, and `CEmergencyStealer::sInstance` appears independently
 * elsewhere in `Initialize()`/`PreAudioTick()`'s relocations) embedded as
 * CLoadBalancer's first 36 bytes (+0x00..+0x23, confirmed untouched by the
 * rest of CLoadBalancer's own constructor -- exactly the space its own
 * ctor call would need). Everything from +0x24 to +0xa4 is then a long,
 * unremarkable run of zeroed dwords (details omitted here; see
 * managers.cpp). `CEmergencyStealer` itself is declared here only as an
 * opaque 36-byte member with an empty constructor body -- its own real
 * constructor's behavior is not reconstructed in this pass, only its
 * existence and size as CLoadBalancer's leading sub-object are confirmed.
 */
class CEmergencyStealer {
public:
	static CEmergencyStealer *sInstance;
	CEmergencyStealer();
	/* Confirmed real (called from CLoadBalancer::~CLoadBalancer(), sec
	 * 10.59), body not reconstructed in this pass. */
	~CEmergencyStealer();
	unsigned char _unrecovered[36];
};

/* Also real heap-`new`'d, same reasoning as CSTGAudioManager. */
class CLoadBalancer {
public:
	static CLoadBalancer *sInstance;
	CLoadBalancer();
	~CLoadBalancer();
	void Initialize();	/* confirmed real, body not reconstructed, sec 10.58 */
	/* BalanceStaticLoad() (confirmed real via a relocation from
	 * CSTGPerformanceVars::FreeVoicelessDyingSlots, sec 10.110) --
	 * confirmed real, deliberately deferred extern; own body not
	 * reconstructed in this pass. */
	void BalanceStaticLoad();
	CEmergencyStealer emergencyStealer;	/* +0x00, confirmed embedded sub-object */
	unsigned char _unrecovered[132];
};

/* ---- CSTGEngine itself ---- */

class CSTGEngine {
public:
	static CSTGEngine *sInstance;

	CSTGEngine();
	~CSTGEngine();

	/* .text+0x1b0, 1901 bytes -- confirmed to exist, called from
	 * setup_global_resources (init_module step 8); not yet
	 * disassembled/implemented in this pass, see
	 * oa_setup_global_resources.h. */
	void Initialize();

	void RunAudioTick(unsigned int tick);
	void PostAudioTick();
	void RunEffects();
	void RunFileDaemonSynchronization();

	/* Object layout otherwise unrecovered (no vtable -- confirmed no
	 * virtual-related relocations at construction). The one confirmed
	 * field, a bool-ish flag the constructor zeroes at +0x4, is accessed
	 * via raw offset arithmetic in engine.cpp rather than named here, to
	 * avoid silently implying bytes +0x0..+0x3 are also recovered (they
	 * are not). This padding exists only so the class's declared size
	 * actually covers that confirmed offset -- without it, `new
	 * CSTGEngine()` wouldn't reserve enough space for the ctor's own
	 * offset+4 write. Real total object size not determined in this pass.
	 */
	unsigned char _unrecovered[16];
};

#endif /* OA_ENGINE_H */
