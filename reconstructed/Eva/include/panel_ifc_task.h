/*
 * panel_ifc_task.h  -  CEditor::CPanelIfcTask, fully reconstructed (Stage 6
 * dedicated CPanelIfcTask batch, 2026-07-25, following the CEditor batch's own
 * "finding for a future dedicated pass" -- see eva_editor_dedicated_batch.md).
 *
 * Was previously: SetMargin/GetMargin (Stage 1 boot path), a real 5-arg CTask
 * base construction only, and 2 real-signature-but-unimplemented instance
 * methods (OnAnalogEvent/OnEncoderEvent). All of the below is NEW this pass --
 * the ctor's own COutLinkMono sub-object + CSTGUnsolMsgHandler construction, the
 * full field layout, and every instance method that routes through
 * `COutLinkMono::OutMono()` (SetLEDStatus x3/ShortBeep/EnterDiagnostics/
 * SetupPanelInterface/SetAllLED/the bare 0-arg Exec() poll override) or through
 * `PegMessageQueue::Push()` (OnTouchPanelEvent/OnButtonEvent/Exec(CMessage&)'s
 * own subtype==2 branch -- the one remaining Peg-toolkit gap, see below).
 *
 * REAL LAYOUT, confirmed from CPanelIfcTask@0824b7e0.c (ctor) cross-checked
 * against every one of this class's own field-touching methods (SetLEDStatus x3
 * @0824c270/0824c2c0/0824c310, ShortBeep@0824c860, EnterDiagnostics@0824c890,
 * SetupPanelInterface@0824b980, SetAllLED@0824c8d0, OnTouchPanelEvent@0824ba20,
 * OnButtonEvent@0824bbd0, Exec@0824b440 (0-arg), Exec@0824c000 (CMessage&)) --
 * base CTask ends at 0x7c (task.h), this class adds exactly 0x3c more bytes,
 * landing on the ctor's own `malloc(0xb8)` exactly (0x7c+0x3c=0xb8), strong
 * confirmation the field map below has no gaps or extras:
 *   +0x7c  mCfgLink        CPanelCfg* -- the ctor's own malloc(0x3c)'d
 *                          COutLinkMono-derived "PanelCfg" sub-object (see
 *                          CPanelCfg below), registered via the real
 *                          CTask::Add(COutLink*) (task.h)
 *   +0x80  mLastQueryFlags captured from SetupPanelInterface's own ecb=5 query
 *                          buffer after the OutMono() call returns; forwarded
 *                          verbatim via 2 further OutMono(0xc/0xe, value) calls.
 *                          In THIS reconstruction always reads back as the
 *                          0xffffffff sentinel SetupPanelInterface seeds it
 *                          with, since OutMono()'s own real receiver (an
 *                          undecoded CLink-family object, out_link.h) is never
 *                          modeled/never writes back into the caller's buffer
 *                          here -- same limitation out_link.h's own
 *                          OutLinkTestHooks note already documents.
 *   +0x84  mReserved84     never touched by any reconstructed method -- kept
 *                          for exact object-size fidelity only (see byte-count
 *                          note above)
 *   +0x88  mUnknown88      ctor zeroes; not read back by any reconstructed
 *                          method (same "opaque, install-only" category as
 *                          module.h's own mUnknown20)
 *   +0x8c  mUnknown8c      ctor zeroes; not read back
 *   +0x90  mUnknown90      ctor sets 0x100 (256); not read back
 *   +0x94  mUnknown94      ctor sets 0x100 (256); not read back
 *   +0x98  mScreen         PegScreen* (kept `void*` here, PegScreen not
 *                          reconstructed) -- the ctor's own `screen` argument,
 *                          real caller is `CEditor::Setup()` passing
 *                          `CMainTask::GetScreen()` (always null in this
 *                          reconstruction, see editor.h) -- Exec()'s own
 *                          unconditional virtual dispatch through it (below)
 *                          is a real, transcribed-faithfully null-deref hazard
 *                          under those conditions, same "preserved, not fixed"
 *                          category as CEditor's own mAlphaKeybIfcTask note.
 *   +0x9c  mDiagMode       EnterDiagnostics()'s own argument; gates
 *                          OnButtonEvent()'s normal-vs-diagnostic charcode path
 *   +0xa0  mLedAllState    SetAllLED()/SetLEDStatus(ELedState)'s own "all on/off"
 *                          state, also read+toggled by Exec()'s own periodic
 *                          blink logic
 *   +0xa4  mBlinkEnabled   gates Exec()'s own periodic all-LED blink; set by
 *                          SetLEDStatus(ELedState==2), cleared by ledState 0/1
 *                          and by the ctor
 *   +0xa8  mBlinkCounter   Exec()'s own per-tick counter (compared against
 *                          0x31/49); NOT initialized by the ctor (matches
 *                          ground truth -- dead until mBlinkEnabled goes
 *                          nonzero, same "genuinely uninitialized, preserved"
 *                          category as mReserved84/CEditor's own
 *                          mAlphaKeybIfcTask)
 *   +0xac  mTouchActive    OnTouchPanelEvent()'s own touch-down/up latch
 *   +0xb0  mTouchX         short, ctor sets 0xffff; last committed touch X
 *   +0xb2  mTouchY         short, ctor sets 0xffff; last committed touch Y
 *   +0xb4  mTouchGen       OnTouchPanelEvent()'s own touch-move re-entrancy
 *                          gate (case 2 returns early while nonzero)
 *
 * VTABLE: real GCC single-inheritance-plus-this-adjustment shape, confirmed
 * byte-exact via direct `.rodata` dword read (NOT inferred) at 0x08f29ce0 (see
 * omega_vtables.h's own header comment for the full derivation): a 5-slot
 * primary group (D1/D0 dtor, Exec() 0-arg, Exec(CMessage&), ExecMsg(CMessage&)
 * -- the last one NOT overridden, copied through from CTask's own vtable
 * unchanged) followed by a 3-slot secondary (this+8, CTask's own "mIfcThunk"
 * identity) group (this-adjusted D1/D0 thunks, ExecMsg thunk -- also not
 * overridden, copied through). Declared as a 7-slot array
 * (`PTR__CPanelIfcTask_08f29ce8`, matching CTask's own established
 * 5-real+2-header bucketing convention, task.h/omega_vtables.cpp) plus a
 * separate `EvaDataPlaceholder_08f29d04` scalar for the this+8 install target
 * -- never dereferenced by any reconstructed code, same status as CTask's own
 * `EvaDataPlaceholder_08e82144`. All install-only (EvaVTableStub-backed), same
 * convention as every other vtable in this project.
 *
 * PEG-TOOLKIT GAP (the one still-open dependency this pass hits):
 * `PegMessageQueue::Push(PegThing::mpMessageQueue, PegMessage const*)`
 * (.text+0x081a8750) is called for real by OnTouchPanelEvent/OnButtonEvent/
 * Exec(CMessage&)'s own subtype==2 branch -- `PegMessageQueue`/`PegThing` are
 * NOT reconstructed anywhere in this project (editor.h's own header comment
 * already flagged this as "the one remaining real Peg-toolkit gap"). Modeled
 * here as a file-local, inert stand-in (panel_ifc_task.cpp) purely so every
 * OTHER real field-touching side effect in these 3 methods can be transcribed
 * faithfully -- same "genuinely undecoded external call, transcribed anyway"
 * category as `out_link.h`'s own CLink receiver dispatch.
 */

#ifndef PANEL_IFC_TASK_H
#define PANEL_IFC_TASK_H

#include "editor.h"
#include "out_link.h"
#include "task.h"

class CMessage;

namespace CPanelOut {
struct SAnalogEvt;
struct SEncoderEvt;
struct SButtonEvt;
struct STouchPanelEvt;
}

/* CPanelCfg -- real ground-truth class name (confirmed via `nm -C`, not
 * anonymous), the ctor's own malloc(0x3c)'d "PanelCfg" COutLinkMono-derived
 * sub-object (CPanelCfg@0898fbb0.c/@0898fbd0.c for D1/D0). Only overrides its
 * own dtor -- CheckDestinationFamily/OnCreateLink/OnConnect/OnDisconnect are
 * all copied through from COutLinkMono's own vtable unchanged (confirmed via
 * direct .rodata byte comparison against PTR__COutLinkMono_08e82048,
 * omega_vtables.h). D1's own real body is a further optimization: it tail-jumps
 * straight to `COutLink::~COutLink()`, skipping COutLinkMono's own (trivial,
 * no-op-beyond-vtable-write) intermediate dtor entirely -- not reconstructed
 * as a separate call here since this project's COutLinkMono has no dtor of its
 * own to skip either.
 */
class CPanelCfg : public COutLinkMono {
public:
	/* .text+0x0898fbb0 (D1)/.text+0x0898fbd0 (D0). Tier A -- forwards to
	 * COutLinkMono then installs this class's own vtable.
	 */
	CPanelCfg(const CTask &owner, const char *name, int direction, unsigned short mode);

	/* +0x38, ctor zeroes. Every CPanelIfcTask OutMono() call site stores its
	 * own return code here (`*(pCVar1+0x38) = uVar2;` in every one of those
	 * methods' own disassembly) -- write-only, never read back by any
	 * reconstructed code (same "dead diagnostic slot" category as several
	 * other classes' own unread fields in this project). Public since the
	 * write happens from CPanelIfcTask's own methods, not from CPanelCfg
	 * itself.
	 */
	int mLastResult;
};

class CEditor::CPanelIfcTask : public CTask {
public:
	/* Real enum recovered from Ghidra's prototype (CEditor::CPanelIfcTask::EMargin);
	 * exact member names are not confirmed, only that main() calls this with
	 * literals 0/1/2/3 -- named generically until the real enumerator names turn up.
	 */
	enum EMargin {
		kMargin0 = 0,
		kMargin1 = 1,
		kMargin2 = 2,
		kMargin3 = 3,
	};

	/* Pre-existing default ctor -- kept for source compatibility with the
	 * already-committed `verify/test_stg_unsol_msg_handler.cpp` stack object.
	 * Never matched any real ground-truth call site.
	 */
	CPanelIfcTask();

	/* .text+0x0824b7e0, 336 bytes -- REAL, full ctor this pass (was a Tier-B
	 * base-only link-stub). `owner`/`screen` match ground truth's own
	 * `(CEditor const&, PegScreen*)` (screen kept `void*`, PegScreen not
	 * reconstructed). See header comment for the full field/vtable writeup.
	 */
	CPanelIfcTask(CEditor *owner, void *screen);

	/* .text+0x0824b3b0 (D1, complete-object destructor). REAL: vtable-field
	 * resets then falls into the base ~CTask() (automatic, C++ base-dtor
	 * call). Ground truth does NOT destroy mCfgLink or the CSTGUnsolMsgHandler
	 * the ctor allocated -- transcribed as found.
	 */
	~CPanelIfcTask();

	/* Bounds-checked write into a 4-entry static byte table (values >= 0x32 are
	 * silently dropped -- real behavior, not a bug).
	 */
	static void SetMargin(EMargin which, unsigned char value);

	/* .text+0x0824cc30, 12 bytes -- companion read. Real body is just
	 * `return sm_aucTouchPanelMargin[which];`, no bounds check on the read side
	 * (matching SetMargin's own bounds check only guarding the write).
	 */
	static unsigned char GetMargin(EMargin which);

	/* .text+0x0824c270, 69 bytes. REAL -- OutMono(ecb=0, {ledCode, ledState}, 8). */
	void SetLEDStatus(int ledCode, int ledState);

	/* .text+0x0824c2c0, 80 bytes. REAL -- OutMono(ecb=1,
	 * {deviceIndex, (onMask<<16)|offMask}, 8).
	 */
	void SetLEDStatus(int deviceIndex, unsigned short onMask, unsigned short offMask);

	/* .text+0x0824c310, 1346 bytes. REAL -- 3-way (0/1/2) all-LED loop +
	 * mBlinkEnabled/mBlinkCounter side effects, see header comment.
	 */
	void SetLEDStatus(int ledState);

	/* .text+0x0824c860, 45 bytes. REAL -- OutMono(ecb=2, value=0). */
	void ShortBeep();

	/* .text+0x0824c890, 51 bytes. REAL -- stores `flag` to mDiagMode, then
	 * OutMono(ecb=3, value=flag).
	 */
	void EnterDiagnostics(int flag);

	/* .text+0x0824b980, 148 bytes. REAL -- OutMono(ecb=5, query-buffer, 0xc)
	 * then 2 OutMono(ecb, value) forwards. See header comment.
	 */
	void SetupPanelInterface();

	/* .text+0x0824c8d0, 852 bytes. REAL -- unconditional all-LED loop (mask
	 * depends on `state`), stores `state` to mLedAllState.
	 */
	void SetAllLED(int state);

	/* .text+0x0824ba20, 409 bytes. REAL field-touching logic (mTouchActive/
	 * mTouchX/mTouchY/mTouchGen, margin-scaled coordinate math against
	 * mScreen's own +0x14/+0x16 fields) -- the PegMessageQueue::Push() call
	 * itself is a Tier-B inert stand-in, see header comment.
	 */
	void OnTouchPanelEvent(const CPanelOut::STouchPanelEvt *evt);

	/* .text+0x0824bbd0, 463 bytes. REAL field-touching logic + charcode
	 * switch/diagnostic-mode gating (mDiagMode) -- same Push()
	 * caveat as OnTouchPanelEvent.
	 */
	void OnButtonEvent(const CPanelOut::SButtonEvt *evt);

	/* .text+0x0824b440, 924 bytes (0-arg override, real vtable slot 2). REAL --
	 * unconditional virtual tick through mScreen+0x10c (a genuine, transcribed
	 * null-deref hazard while mScreen stays null, see header comment), then a
	 * 50-tick periodic all-LED blink via OutMono(). NOT declared C++ `virtual`
	 * (this project's raw-mVtbl convention).
	 */
	int Exec();

	/* .text+0x0824c000, 601 bytes (1-arg override, real vtable slot 3). REAL --
	 * dispatches on `msg`'s own raw +8 flags word (bit 0x200 gate, low byte
	 * subtype 1..4) to OnButtonEvent/OnAnalogEvent(loop)/OnTouchPanelEvent, or
	 * (subtype==2) a direct PegMessageQueue::Push() of its own. CMessage stays
	 * opaque/raw-offset-accessed, same convention as sysex_msg_task_base.h.
	 */
	int Exec(CMessage &msg);

	/* .text+0x0824be00, 403 bytes. REAL (Stage 6 breadth sweep, 2026-07-25) --
	 * mDiagMode (+0x9c) gate reroutes to a Peg message (cmd 0x500a); otherwise
	 * maps `evt->type` (8..0x18) to a 0..0x10 knob/fader index and forwards to
	 * `CControlSurface::MoveKnobFader()` (inert stand-in, .cpp -- CControlSurface
	 * itself is a large not-reconstructed god-object family, same category as
	 * the ES-family classes); `type==0x19` is its own special case, a Peg
	 * message instead (matches every currently-reconstructed real caller in
	 * stg_unsol_msg_handler.cpp). CSTGUnsolMsgHandler's own HandleMessage()/
	 * EndHandling()/SendValueSlider()/SendValueEncoder() call this on `mOwner`,
	 * and Exec(CMessage&) above calls it in a loop.
	 */
	void OnAnalogEvent(const CPanelOut::SAnalogEvt *evt);

	/* .text+0x0824bdb0, 79 bytes. REAL -- pushes a Peg message (cmd 0x500e)
	 * carrying `evt->value`'s own first byte, sign-extended (see .cpp).
	 * CSTGUnsolMsgHandler's own EndHandling()/SendValueEncoder() call this.
	 */
	void OnEncoderEvent(const CPanelOut::SEncoderEvt *evt);

private:
	CPanelCfg     *mCfgLink;        /* +0x7c */
	unsigned int   mLastQueryFlags; /* +0x80 */
	int            mReserved84;     /* +0x84, never touched -- size fidelity only */
	int            mUnknown88;      /* +0x88 */
	int            mUnknown8c;      /* +0x8c */
	int            mUnknown90;      /* +0x90 */
	int            mUnknown94;      /* +0x94 */
	void          *mScreen;         /* +0x98, PegScreen* */
	int            mDiagMode;       /* +0x9c */
	int            mLedAllState;    /* +0xa0 */
	int            mBlinkEnabled;   /* +0xa4 */
	int            mBlinkCounter;   /* +0xa8, deliberately NOT ctor-initialized */
	int            mTouchActive;    /* +0xac */
	short          mTouchX;         /* +0xb0 */
	short          mTouchY;         /* +0xb2 */
	int            mTouchGen;       /* +0xb4 */

	static unsigned char sm_aucTouchPanelMargin[4]; /* real, `nm -C`-confirmed
	                                                   * `CEditor::CPanelIfcTask::sm_aucTouchPanelMargin`,
	                                                   * 0x0939c310 */
	static CEditor::CPanelIfcTask *sInstance; /* real, `_ZN7CEditor13CPanelIfcTask9sInstanceE`,
	                                            * 0x0939c314 -- set by the ctor, never read back
	                                            * by any reconstructed code */

	/* Friend accessor for verify/test_panel_ifc_task.cpp -- pokes mScreen
	 * with a fake vtable-shaped buffer so Exec()'s own unconditional virtual
	 * tick can be exercised without crashing, same "friend pokes a raw
	 * buffer" convention as OutLinkTestHooks (out_link.h)/
	 * ClientCommServerTestHooks (client_comm_server.h).
	 */
	friend struct PanelIfcTaskTestHooks;
};

#endif /* PANEL_IFC_TASK_H */
