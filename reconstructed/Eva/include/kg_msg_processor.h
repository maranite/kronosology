/*
 * kg_msg_processor.h  -  CKGMsgProcessor (ctor/dtor/GetInstance() only), Eva
 * deferred-registry re-check batch, 2026-07-27.
 *
 * BACKGROUND: HARDWARE_REVIEW_LOG.md's deferred/out-of-scope registry cites
 * "CKGMsgProcessor" (alongside CControlSurface/CMMI/CModeManager) as one of the
 * genuinely-deep subsystems `CSTGUnsolMsgHandler::ControlMsgHandler`'s still-
 * unpromoted outer hub reaches -- a correct verdict for the class's own real
 * message-processing methods (SetGEMax/Process/CheckAndSet-family/GetKarmaNotes/
 * ClearInvalidNotesCCsDisplay, all genuine Karma-note-generation logic, still out
 * of scope, see below). This batch re-applied the project's own "size is not
 * depth, check for a tractable sub-piece" lens (CJobStack/CLimiterBase/CEditClient
 * precedent) specifically to CKGMsgProcessor's OWN construction/destruction, which
 * a fresh `objdump -dr -M intel` trace of `CKGMsgProcessor::CKGMsgProcessor()`
 * (.text+0x08913620, 570 bytes) / `~CKGMsgProcessor()` (.text+0x08913860, 134
 * bytes) / `GetInstance()` (.text+0x089138f0, 134 bytes) found to be small and
 * fully self-contained -- 9 mallocs + fixed-offset field writes + one vtable-slot
 * dispatch through the project's own already-documented `Api+0x9c` accessor
 * (timer_engine.h's `ApiGetDefault9c()`), no CZ/CStorage/CMMI/CModeManager
 * dependency of its own.
 *
 * REACHABILITY: CKGMsgProcessor::GetInstance() DOES have real, already-
 * reconstructed callers -- `CSTGUnsolMsgHandler::ProgramSlotMsgHandler()`
 * (stg_unsol_msg_handler.cpp, Tier A batch 5) reads/writes its singleton's own
 * +0x28/+0x29 byte fields. That call site deliberately uses its OWN file-local,
 * intentionally-fake `class CKGMsgProcessor { static void *GetInstance(); }`
 * stub (a static byte buffer, not this real reconstruction) -- see that file's
 * own header comment for why (a "safe-to-call opaque stub" convention already
 * established project-wide for CMMI/CModeManager in the same file). This header
 * does NOT collide with that stub: neither file includes the other, so both
 * `class CKGMsgProcessor` definitions coexist in separate translation units with
 * no ODR conflict (same convention already used for CDesktop/CModeManager/CMMI
 * being redeclared file-locally in more than one place across this project).
 * This real reconstruction is therefore NOT wired into ProgramSlotMsgHandler's
 * own call site -- deliberately, to avoid touching working, already-verified code
 * outside this batch's own scope -- and has no live caller of its own on this
 * project's traced boot path. Reconstructed for structural completeness, same
 * "closes a registry-flagged gap even without a live caller in THIS
 * reconstruction" precedent as CLimiterBase/CJobStack.
 *
 * REAL LAYOUT, CKGMsgProcessor (0x34/52 bytes, confirmed by GetInstance()'s own
 * `malloc(0x34)` call size):
 *   +0x00  mCommonHandler        void* -- malloc(0x18/24), vtbl-only poke (see
 *                                 below), owns a CKGCommonMsgHandler
 *   +0x04  mModuleHandler        void* -- malloc(0x1c/28), owns a
 *                                 CKGModuleMsgHandler
 *   +0x08  mUIControlHandler     void* -- malloc(0x18/24), owns a
 *                                 CKGUIControlMsgHandler
 *   +0x0c  mSPRUIControlHandler  void* -- malloc(0x18/24), owns a
 *                                 CSPRUIControlMsgHandler
 *   +0x10  mSPRUICommonParamHandler   void* -- malloc(0x18/24), owns a
 *                                 CSPRUICommonParamMsgHandler
 *   +0x14  mSPRUIAudioTrackParamHandler void* -- malloc(0x18/24), owns a
 *                                 CSPRUIAudioTrackParamMsgHandler
 *   +0x18  mSPRUIDrumTrackParamHandler void* -- malloc(0x18/24), owns a
 *                                 CSPRUIDrumTrackTrackParamMsgHandler
 *   +0x1c  mUnknown1c            int, ctor sets 0xe (14) -- real meaning not
 *                                 decoded (no consumer found in any of this
 *                                 class's own traced methods)
 *   +0x20  mBuffer50             unsigned char* -- malloc(0x50/80), ctor
 *                                 zero-fills all 80 bytes; real meaning not
 *                                 decoded (no vtable poke, plain data buffer,
 *                                 not a polymorphic sub-object)
 *   +0x24  mBuffer10             unsigned char* -- malloc(0x10/16), ctor
 *                                 zero-fills all 16 bytes; same status as
 *                                 mBuffer50
 *   +0x28  mFlag28               unsigned char, ctor sets 0. Real, live consumer:
 *                                 ProgramSlotMsgHandler's own idx==8/idx==9
 *                                 in-timbre-edit-context branches write this to 1
 *                                 (stg_unsol_msg_handler.cpp, via its own separate
 *                                 file-local stub -- see REACHABILITY above).
 *   +0x29  mFlag29               unsigned char -- ctor does NOT initialize this
 *                                 byte at all (confirmed: the ctor's disassembly
 *                                 has no write anywhere in [0x29,0x2b], only
 *                                 +0x28 gets an explicit `mov byte [x+0x28],0`).
 *                                 Real ground-truth quirk, transcribed faithfully
 *                                 rather than "fixed" -- same class of finding as
 *                                 `CEditor::CPanelIfcTask::mBlinkCounter`'s own
 *                                 uninitialized-ctor-field entry in
 *                                 HARDWARE_REVIEW_LOG.md. ProgramSlotMsgHandler's
 *                                 own two write sites (never a read) mean this
 *                                 particular gap is not currently observable as a
 *                                 read-of-uninitialized-memory bug on this
 *                                 project's traced call graph, but is flagged here
 *                                 for the real-hardware comparison pass regardless.
 *   +0x2a..+0x2b                 2 bytes of natural struct padding before the next
 *                                 4-byte-aligned int field; not separately touched
 *                                 by the real ctor either.
 *   +0x2c  mUnknown2c            int, ctor sets 1. Real meaning not decoded.
 *   +0x30  mApiDefault           int, ctor sets to the result of a real dispatch
 *                                 through Api's own vtable slot +0x9c -- the SAME
 *                                 "Api::GetDefaultXxx()"-shaped accessor
 *                                 `CExternalClock`/`CInternalClock`'s own ctors
 *                                 already established (timer_engine.h/cpp's
 *                                 `ApiGetDefault9c()`), reused verbatim here
 *                                 rather than re-derived.
 *
 * The 7 owned handler sub-objects (mCommonHandler..mSPRUIDrumTrackParamHandler)
 * are each malloc'd at their own real, class-specific size, but the ctor writes
 * ONLY their own offset-0 vtable pointer -- no sub-ctor call, no other field
 * write -- matching the "install a vtable pointer with no backing method-table
 * dispatch" convention timer_engine.h's own +0x11c/+0x120 interface slots already
 * established. Modeled here as raw `void*` members (not real C++ objects), each
 * malloc'd to its own real byte size and vtable-poked with the correct real
 * identity (7 new PTR__ arrays, omega_vtables.h/.cpp, sizes confirmed via direct
 * `.rodata` dword reads at each real _ZTV symbol, not inferred from `nm -C`'s
 * mangled-name size field): CKGCommonMsgHandler (21 slots), CKGModuleMsgHandler
 * (21 slots), CKGUIControlMsgHandler (51 slots -- the largest, matching this
 * class's own likely per-Karma-control-widget dispatch breadth), CSPRUIControl-
 * MsgHandler (19 slots), CSPRUICommonParamMsgHandler/CSPRUIAudioTrackParam-
 * MsgHandler/CSPRUIDrumTrackTrackParamMsgHandler (8 slots each). None of these 7
 * classes' own real methods are reconstructed anywhere in this project -- same
 * "structurally real, functionally opaque" bar as CJobStack's own embedded
 * CRMJob.
 *
 * `~CKGMsgProcessor()` (.text+0x08913860): re-installs its own vtable identity,
 * then for EACH of the 7 handler members in offset order (+0x00..+0x18), if
 * non-null, dispatches through that member's OWN vtable slot index 1 (offset+4 --
 * the Itanium *deleting* destructor, D0, which frees the object itself) rather
 * than slot index 0 (the non-deleting complete-object dtor, D1) -- confirmed by
 * the real disassembly reading `[vtbl+4]` for all 7, not `[vtbl+0]`. The LAST of
 * the 7 (+0x18) is compiled as a tail-jmp (sibcall) into that same deleting-dtor
 * slot rather than a call+continue, since it is the final statement in the real
 * function body -- same GCC tail-call-optimization shape this project has
 * documented elsewhere. mBuffer50/mBuffer10 (+0x20/+0x24, the 2 non-polymorphic
 * raw buffers) are NOT freed by this destructor at all -- confirmed: the real
 * disassembly's own member-walk only tests offsets 0x00/0x04/0x08/0x0c/0x10/0x14/
 * 0x18, never 0x20 or 0x24. A real, faithfully-transcribed leak on every
 * CKGMsgProcessor destruction (never actually exercised on this project's traced
 * boot path, since nothing here ever destroys the one live singleton instance
 * either -- flagged for the real-hardware comparison pass, not "fixed").
 *
 * `GetInstance()` (.text+0x089138f0): classic lazy-singleton, `ms_poInstance`
 * (real symbol `_ZN15CKGMsgProcessor13ms_poInstanceE`, `.bss` @0x0acada80) --
 * malloc(0x34), construct in place, store, return. No locking (matches every
 * other lazy singleton already reconstructed in this project -- Eva's own
 * single-threaded-relative-to-this-init-order assumption, not a race this
 * project's own scheduler model exercises).
 *
 * DELIBERATELY NOT RECONSTRUCTED (genuinely deep, registry verdict re-confirmed
 * by this same pass): `SetGEMax(int)`, `Process()` (1110 bytes), `CheckAndSet-
 * ChordName()`/`CheckAndSetCCsDisplay()`/`CheckAndUpdateDisplay()`/`CheckAndSet-
 * NotesDisplay()`/`CheckAndSetRTValueString()`, `ClearInvalidNotesCCsDisplay()`
 * (1679 bytes), `GetKarmaNotes(int, CKarmaNotes**, bool*)` -- all genuine Karma-
 * note-generation/display logic (real `CKarmaNotes*` out-parameters, real display-
 * state dispatch through the 7 opaque handler sub-objects above), exactly the
 * "genuinely too deep" bar HARDWARE_REVIEW_LOG.md's registry already applied to
 * this class -- confirmed still accurate, not stale.
 */

#ifndef KG_MSG_PROCESSOR_H
#define KG_MSG_PROCESSOR_H

class CKGMsgProcessor {
public:
	/* .text+0x08913620, 570 bytes. */
	CKGMsgProcessor();

	/* .text+0x08913860, 134 bytes (D1/D2, identical -- no virtual base). */
	~CKGMsgProcessor();

	/* .text+0x089138f0, 134 bytes. Real lazy singleton -- see file header. */
	static CKGMsgProcessor *GetInstance();

private:
	void *mCommonHandler;              /* +0x00 */
	void *mModuleHandler;              /* +0x04 */
	void *mUIControlHandler;           /* +0x08 */
	void *mSPRUIControlHandler;        /* +0x0c */
	void *mSPRUICommonParamHandler;    /* +0x10 */
	void *mSPRUIAudioTrackParamHandler; /* +0x14 */
	void *mSPRUIDrumTrackParamHandler; /* +0x18 */
	int   mUnknown1c;                  /* +0x1c, ctor sets 0xe */
	unsigned char *mBuffer50;          /* +0x20, ctor mallocs+zeroes 80 bytes */
	unsigned char *mBuffer10;          /* +0x24, ctor mallocs+zeroes 16 bytes */
	unsigned char  mFlag28;            /* +0x28, ctor sets 0 */
	unsigned char  mFlag29;            /* +0x29, ctor leaves UNINITIALIZED -- see
	                                     * file header, transcribed faithfully */
	int   mUnknown2c;                  /* +0x2c, ctor sets 1 */
	int   mApiDefault;                 /* +0x30, ctor sets Api's own +0x9c
	                                     * vtable-call result */

	static CKGMsgProcessor *ms_poInstance; /* .bss @0x0acada80 */

	CKGMsgProcessor(const CKGMsgProcessor &);
	CKGMsgProcessor &operator=(const CKGMsgProcessor &);

	friend struct KGMsgProcessorTestHooks;
};

#endif /* KG_MSG_PROCESSOR_H */
