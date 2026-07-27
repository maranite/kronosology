/*
 * limiter_base.h  -  CLimiterBase (+ nested CWrProtCircularQueue), Eva "size is not
 * depth" re-check batch, 2026-07-27. Found via a fresh objdump -dr -M intel re-trace
 * of the HARDWARE_REVIEW_LOG.md registry's "COutLinkIfcBase/CMarshaller<T> framework"
 * bullet, which (as of 2026-07-26) claimed "no concrete instantiation exists yet" --
 * that claim was already half-wrong (CAlphaKeybCtrlTask's mCodeIfc, alpha_keyb_ctrl_
 * task.h, IS one) and this batch found a SECOND, independent one: CLimiterBase::Init()
 * builds a COutLinkIfc<ILimiterNotify>/CMarshaller<ILimiterNotify> sub-object using the
 * exact same "malloc + base-construct + raw vtable pokes" idiom, confirmed via a direct
 * `.rodata` byte read of PTR__CLimiterBase_08e81c90/PTR__CWrProtCircularQueue_08e81ca8
 * (only 4 and 2 real virtual slots respectively -- much smaller than `nm -C`'s ~16-method
 * total count for the whole class suggested).
 *
 * REACHABILITY: unlike CAlphaKeybCtrlTask's mCodeIfc (genuinely constructed on this
 * project's own wired boot path), CLimiterBase itself has ZERO callers anywhere in the
 * whole 22MB ground-truth binary -- confirmed by grepping `objdump -dr` for any `call`
 * targeting `_ZN12CLimiterBaseC1E...` or `CLimiterMan::RegisterLimiter(CLimiterBase*)`.
 * This is dead code in ground truth itself, not just in this reconstruction (matches
 * limiter_man.h's own existing note: "nothing calls RegisterLimiter(), not
 * reconstructed -- out of scope"). Reconstructed anyway for structural completeness,
 * same precedent as CJobStack's ctor/dtor (job_stack.h) -- small, fully self-contained,
 * closes out a registry-flagged gap -- even without a live caller.
 *
 * REAL LAYOUT, CLimiterBase (0x34 bytes, confirmed from CLimiterBase@0807aa50.c /
 * Init@0807ac70.c):
 *   +0x00  mVtbl        PTR__CLimiterBase_08e81c90 (4 slots: dtor D1/D0,
 *                        SendNoAnswer, SendWithAnswer -- confirmed by direct
 *                        `.rodata` dword read at 0x8e81c90..0x8e81c9c; "vtable for
 *                        CLimiterBase::CWrProtCircularQueue" begins immediately
 *                        after at 0x8e81ca0, so CLimiterBase's own vtable is
 *                        exactly these 4 slots, not the ~16 nm -C lists across the
 *                        whole class -- most of CLimiterBase's own methods are
 *                        non-virtual).
 *   +0x04  mIfcLink      void* -- lazily built by Init(CTask&, unsigned int); NULL
 *                        until then. Ctor zeroes it.
 *   +0x08  mUnknown08    int, ctor zeroes. No consumer found in ctor/Init/Write/
 *                        PopMessage/SendWithAnswer/SendNoAnswer -- real meaning not
 *                        decoded, faithfully preserved regardless.
 *   +0x0c  mQueue        embedded CWrProtCircularQueue (0x1c bytes, +0x0c..+0x28).
 *   +0x28  mInitAttempted int, ctor zeroes. Dual role, both confirmed from real
 *                        disassembly: (1) Init(CTask&,uint)'s own one-shot guard --
 *                        if mIfcLink is still NULL and this is already nonzero,
 *                        Init() gives up immediately instead of retrying; (2) the
 *                        ctor's own "already logged this specific queue-capacity
 *                        assert" dedup flag, matching the "warn once" idiom seen
 *                        elsewhere in this project.
 *   +0x2c  mUnmarshallFn bool(*)(unsigned char, void*, unsigned int, CIfcUnknown*)
 *                        -- the ctor's 3rd argument, stored verbatim. Real consumer:
 *                        PopMessage() (see below), called with (type, data, len,
 *                        CIfcUnknown* pulled from the message tail, unmodeled).
 *   +0x30  mSendFn       bool(*)(unsigned char, void*, unsigned int, CIfcUnknown*)
 *                        -- the ctor's 4th argument, stored verbatim. No consumer
 *                        found in any of this class's own traced methods (Write(),
 *                        SendWithAnswer(), SendNoAnswer() all route through the
 *                        queue + the out-of-scope ILimiterNotify marshaller
 *                        instead) -- real meaning not decoded.
 *
 * REAL LAYOUT, CWrProtCircularQueue ("Write-Protected Circular Queue", 0x1c bytes,
 * confirmed from CWrProtCircularQueue@0807a360.c/Init@0807a3a0.c). A single-writer/
 * single-reader (RTAI ISR-writer / task-context-reader) message ring buffer with
 * HAL_DisableInterrupts()/HAL_EnableInterrupts() as its critical section, framed
 * messages (1 type byte + 1 length dword header, then the payload, all dword-padded),
 * and a same-type-write-coalescing optimization (mLastHeader, below):
 *   +0x00  mVtbl        PTR__CWrProtCircularQueue_08e81ca8 (2 slots: dtor D1/D0 only
 *                        -- confirmed by direct `.rodata` read, the typeinfo-name
 *                        string for the NEXT class begins immediately after at
 *                        0x8e81cb0).
 *   +0x04  mLevel        int, the ctor's own ETaskLevel argument, stored verbatim.
 *                        Consumer: Write()'s own debug-trace call (Tier B, below).
 *   +0x08  mBase         unsigned char* -- malloc'd backing buffer, NULL until
 *                        Init(int) succeeds.
 *   +0x0c  mLimit        unsigned char* -- mBase + capacity (one-past-the-end).
 *   +0x10  mReadPtr      unsigned char* -- next byte to read.
 *   +0x14  mWritePtr     unsigned char* -- next byte to write.
 *   +0x18  mLastHeader   unsigned char* -- NULL, or points at the most recently
 *                        written message's own type byte inside the buffer; used
 *                        by Write() (Tier B) to detect "the reader hasn't consumed
 *                        the last message of this exact type yet" and coalesce
 *                        instead of enqueuing a second copy.
 *
 * `Init(int)` (.text+0x0807a3a0, 208 bytes) IS Tier A: validates `level` (the ctor's
 * own stored mLevel, re-read via [ebx+4], must be <= 0x17/23 -- logs a real
 * "Assertion failed" via the global Api-shaped debug-log object at ds:0x930a1f4
 * otherwise, same idiom as every other soft assert in this project) and a `sizeShift`
 * argument: `sizeShift <= 6` forces a fixed 0x80-byte (128) buffer; otherwise
 * capacity = `(1 << sizeShift) & ~3` (dword-aligned power-of-two). Fully self-
 * contained -- only HAL_DisableInterrupts/EnableInterrupts + malloc, no CZ/CStorage
 * dependency.
 *
 * `Write(unsigned char, void*, unsigned int)`, `StaticRead(unsigned char&, void*&,
 * unsigned int&)`, `SeekNextRead()` stay Tier B (real signatures declared, no body):
 * genuinely intricate wraparound-fitting state machines (Write() alone branches into
 * 4 distinct "how much contiguous free space is left before mLimit" cases, StaticRead()
 * mirrors the same math on the read side) -- judged disproportionate given this class
 * has ZERO real callers anywhere in ground truth (see REACHABILITY above), same
 * "genuinely intricate, deferred" call this project already made for
 * `CSTGUnsolMsgHandler::ControlMsgHandler`'s cross-jumped hub and
 * `EffectSlotMsgHandler`'s 28-byte reused stack buffer.
 *
 * `CLimiterBase::Write()`/`PopMessage()` (real signatures, Tier B) are real,
 * self-contained FORWARDS into the (Tier-B) queue methods plus, on success, a
 * dispatch through the out-of-scope ILimiterNotify marshaller -- same "the forward
 * is real even if its own target is a stub" precedent as `CDumpMachine::PutMessage()`
 * (dump_man_state_machine.h). `SendWithAnswer()`/`SendNoAnswer()` ARE Tier A: both
 * are literal, byte-for-byte-confirmed tail-jumps into `Write()` (widening the bool
 * `withAnswer` argument to an int and falling straight through -- `.text+0x0807af80`/
 * `0x0807afa0`, 14 bytes each).
 *
 * "ILimiterNotify interface link" (mIfcLink, +0x04), built by `Init(CTask&,
 * unsigned int level)` (.text+0x0807ac70, 480 bytes) is the actually valuable
 * FINDING this batch made (a 2nd real COutLinkIfcBase/CMarshaller<T> instantiation
 * site, independent of CAlphaKeybCtrlTask's mCodeIfc, directly contradicting
 * HARDWARE_REVIEW_LOG.md's prior "no concrete instantiation exists yet" claim for
 * this framework) -- but stays Tier B (real signature declared, no body) rather
 * than Tier A, for a reason AlphaKeybCode's own analogous case didn't have: ground
 * truth's real disassembly here calls `COutLinkIfcBaseC1(CTask const&, char const*,
 * unsigned int, bool(*)(...))` DIRECTLY (`.text+0x0807b7e0`, confirmed by exact call-
 * target address match against `nm -C`'s own `COutLinkIfcBase::COutLinkIfcBase(...)`
 * symbol) -- NOT `COutLink::COutLink(...)` the way `CAlphaKeybCtrlTask`'s own real
 * ctor does (alpha_keyb_ctrl_task.cpp's `BuildAlphaKeybCodeIfcLink()` reuses
 * `COutLink`'s already-real ctor for exactly that reason: it's what ground truth's
 * OWN disassembly at that OTHER call site actually invokes). Since `COutLinkIfcBase`
 * itself is not reconstructed anywhere in this project, and substituting `COutLink`
 * here would not be transcription -- it would be a guess unsupported by this call
 * site's own real disassembly -- the base-subobject bytes it would set up (+0x00..
 * +0x33) are left undocumented rather than fabricated. What IS fully decoded and
 * would be real if/when `COutLinkIfcBase` itself is ever reconstructed: `sprintf(
 * "Limiter_%d", level)` builds the link's name; `malloc(0x50)`; the `COutLinkIfcBaseC1`
 * args are `(owner, name, ILimiterNotify::sm_tInterfaceId, &COutLinkIfc_ILimiterNotify_
 * Unmarshall)` -- the 3rd arg is a REAL ground-truth static read LIVE from `ds:
 * 0x930a290` (not a compile-time literal like AlphaKeybCode's own hardcoded
 * `interfaceId` -- but `ILimiterNotify::sm_tInterfaceId` is itself a `.bss` zero-
 * initializer with no observed real populator either, so it evaluates to 0 in
 * practice regardless); then, matching AlphaKeybCode's exact "override own vtable
 * identity after base ctor" idiom, 4 further raw writes: +0x00 (primary vtable,
 * `PTR__COutLinkIfc_ILimiterNotify_08e81d28`), +0x34 (secondary/thunk vtable,
 * `PTR__COutLinkIfc_ILimiterNotify_08e81d50`), +0x48 (`CMarshaller<ILimiterNotify>`
 * sub-object vtable, `PTR__CMarshaller_ILimiterNotify_08e81f00`), +0x4c (self-back-
 * pointer to +0x34). Then, ONLY if mInitAttempted (+0x28) is still 0 at that point,
 * `CTask::Add(this_ifcLink)` registers it into the owning task's own `mOutLinks`
 * array (already-real, task.h) -- same registration call AlphaKeybCode's ctor makes
 * unconditionally; here it is conditional on the one-shot guard. `PTR__CLimiterBase_
 * 08e81c90`/`PTR__CWrProtCircularQueue_08e81ca8`/`PTR__COutLinkIfc_ILimiterNotify_*`/
 * `PTR__CMarshaller_ILimiterNotify_08e81f00` are all declared (install-only,
 * `EvaVTableStub`-backed) in omega_vtables.h regardless, since `CLimiterBase`'s own
 * (Tier-A) ctor/dtor need `PTR__CLimiterBase_08e81c90`/`PTR__CWrProtCircularQueue_
 * 08e81ca8` to install real vtable identities even though `Init()` itself stays a
 * stub.
 *
 * DELIBERATELY NOT MODELED AS A REAL C++ CLASS (mIfcLink's own contents) -- same bar
 * as AlphaKeybCode's mCodeIfc: `COutLinkIfcBase`/`COutLinkIfc<T>`/`CMarshaller<T>` is
 * a real Itanium-ABI multiple-inheritance-with-thunk hierarchy shared by several
 * un-reconstructed subsystems (`ILimiterNotify` here, `IAlphaKeybEvent`/
 * `IAlphaKeybCtrl` elsewhere) -- reconstructing it properly means reconstructing the
 * shared framework, not a one-off. `CMarshaller<ILimiterNotify>::WakeUp(unsigned int)`
 * (.text+0x0807b980, confirmed via `nm -C` to be this specialization's 4th real vtable
 * slot) is the real target any dispatch through mIfcLink+0x48 would reach -- not
 * reconstructed, same as `CMarshaller<IAlphaKeybCode>::ProcessCode()`.
 */

#ifndef LIMITER_BASE_H
#define LIMITER_BASE_H

class CTask;
class CIfcUnknown;

class CLimiterBase {
public:
	/* Real signature: `bool (*)(unsigned char, void*, unsigned int, CIfcUnknown*)`,
	 * matching CLimiterBase's own ctor argument types (opaque -- the real
	 * `CIfcUnknown` class is not reconstructed elsewhere in this project either,
	 * task.h).
	 */
	typedef bool (*TMsgFn)(unsigned char, void *, unsigned int, CIfcUnknown *);

	/* .text+0x0807aa50, ~0x1b0 bytes. `sizeShift`/`level` map directly onto the
	 * embedded CWrProtCircularQueue's own ctor+Init(int) argument pair (see
	 * header comment) -- this ctor duplicates that same validate-then-malloc
	 * logic inline rather than calling Init(int) as a real function (matches
	 * ground truth's own real disassembly, which has no such call).
	 */
	CLimiterBase(int sizeShift, int level, TMsgFn unmarshallFn, TMsgFn sendFn);

	/* .text+0x0807a210 (D1) / 0x0807a270 (D0). */
	~CLimiterBase();

	/* .text+0x0807ac70, 480 bytes. Tier B (real signature, no body) -- see
	 * header comment's own "ILimiterNotify interface link" section for why
	 * (genuinely blocked on the un-reconstructed COutLinkIfcBase ctor, not a
	 * scope/effort choice).
	 */
	void Init(CTask &owner, unsigned int level);

	/* .text+0x0807ae50/0x0807ac70(tail). Real signatures, Tier B -- both
	 * ultimately depend on CWrProtCircularQueue::Write()/StaticRead() (Tier B)
	 * and, for Write(), on mIfcLink having been built by the (Tier-B) Init().
	 */
	bool Write(unsigned char type, void *data, unsigned int len);
	void PopMessage();

	/* .text+0x0807af80/0x0807afa0, 14 bytes each. Real, literal tail-jumps into
	 * Write() (widen bool to int, fall through) -- real virtual slots 3/2 of
	 * PTR__CLimiterBase_08e81c90. Declared here with real signatures; bodies
	 * forward into the (Tier-B) Write() above, same "the forward is real even
	 * if its own target is a stub" precedent as CDumpMachine::PutMessage()
	 * (dump_man_state_machine.h).
	 */
	bool SendWithAnswer(unsigned char type, void *data, unsigned int len);
	void SendNoAnswer(unsigned char type, const void *data, unsigned int len);

private:
	/* CWrProtCircularQueue -- see header comment. Declared as a private nested
	 * class purely for namespacing (matches ground truth's own C++ nesting);
	 * never used polymorphically by anything outside CLimiterBase's own methods,
	 * so this is not a correctness-relevant choice.
	 */
	class CWrProtCircularQueue {
	public:
		/* .text+0x0807a360, 40 bytes. */
		explicit CWrProtCircularQueue(int level);

		/* .text+0x0807a1c0 (D1) / 0x0807a2f0 (D0, deleting -- also frees `this`
		 * itself; never exercised here since this class is always embedded,
		 * never separately new'd, but transcribed for completeness).
		 */
		~CWrProtCircularQueue();

		/* .text+0x0807a3a0, 208 bytes. Real: level > 0x17 or malloc failure ->
		 * false (with a real Api-shaped assert log on the level check). Returns
		 * true on success.
		 */
		bool Init(int sizeShift);

		/* .text+0x0807aa30, 10 bytes. Real: `((len & 3) != 0) + (len >> 2)` --
		 * word count a payload of `len` bytes occupies once dword-padded.
		 */
		static unsigned int CountIntegers(unsigned int len);

		/* .text+0x0807a9c0, 104 bytes. Real: mReadPtr == mWritePtr, wrapped in
		 * the same Api-shaped debug-trace call bracket every other method here
		 * makes (level, 0/1 -- entry/exit markers).
		 */
		bool IsEmpty() const;

		/* .text+0x0807a470, 736 bytes. Tier B -- see header comment. */
		bool Write(unsigned char type, void *data, unsigned int len);

		/* .text+0x0807a750, 328 bytes. Tier B -- see header comment. */
		bool StaticRead(unsigned char &type, void *&data, unsigned int &len);

		/* .text+0x0807a8a0, 288 bytes. Tier B -- see header comment. */
		bool SeekNextRead();

	private:
		void          *mVtbl;       /* +0x00 */
		int            mLevel;      /* +0x04 */
		unsigned char *mBase;       /* +0x08 */
		unsigned char *mLimit;      /* +0x0c */
		unsigned char *mReadPtr;    /* +0x10 */
		unsigned char *mWritePtr;   /* +0x14 */
		unsigned char *mLastHeader; /* +0x18 */

		/* Never copied anywhere in ground truth (always embedded exactly once
		 * inside a CLimiterBase, never assigned/passed by value) -- disabled
		 * here rather than left to a compiler-generated shallow copy, which
		 * would double-free mBase. Same convention as CLimiterBase's own
		 * disabled copy ctor/assignment below.
		 */
		CWrProtCircularQueue(const CWrProtCircularQueue &);
		CWrProtCircularQueue &operator=(const CWrProtCircularQueue &);

		friend class CLimiterBase;
		friend struct LimiterBaseTestHooks;
	};

	void          *mVtbl;          /* +0x00 */
	void          *mIfcLink;       /* +0x04 */
	int             mUnknown08;    /* +0x08 */
	CWrProtCircularQueue mQueue;   /* +0x0c, 0x1c bytes */
	int             mInitAttempted; /* +0x28 */
	TMsgFn          mUnmarshallFn; /* +0x2c */
	TMsgFn          mSendFn;       /* +0x30 */

	CLimiterBase(const CLimiterBase &);
	CLimiterBase &operator=(const CLimiterBase &);

	friend struct LimiterBaseTestHooks;
};

#endif /* LIMITER_BASE_H */
