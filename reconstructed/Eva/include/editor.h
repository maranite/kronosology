/*
 * editor.h  -  CEditor, the real root of Eva's front-panel/UI subsystem.
 * Stage 6 CEditor batch, 2026-07-25 -- the dedicated pass flagged as needed by
 * the CHIDDriver/CLinuxPanelDriver batch (eva_hid_driver_panel_driver_batch.md)
 * and referenced throughout module.h/panel_ifc_task.h/task.h/task_buffer.h/
 * stg_unsol_msg_handler.h's own header comments ("CEditor itself is not
 * reconstructed as a class anywhere in this project").
 *
 * GROUND TRUTH SHAPE: `CEditor::Setup()` (.text+0x08249b60, CEditor@08249b60.c)
 * is dispatched through `CModuleManager::Setup()`'s own per-module vtable+8 call
 * (module_manager.cpp), itself called from `CKernel::InitSystemLayer()`
 * (ckernel.cpp) -- CONFIRMED on the already-real boot-path spine, same as every
 * other MMainXxx-family module. Unlike that 15-member shim family though,
 * CEditor is NOT built by a `mains.cpp` `MMainXxx(void)` factory -- nothing in
 * this reconstruction's own call graph yet constructs a `CEditor` (its own real
 * constructor call site was not found in mains.cpp/ckernel.cpp/module_manager.cpp;
 * likely main()'s own direct call, main() itself being far outside this pass's
 * scope). CEditor's own ctor/dtor/Setup()/Start() are still real, tractable,
 * self-contained reconstructions in their own right regardless of whether their
 * OWN caller is wired up yet -- same "reconstruct the function, note the caller
 * gap" approach as CTask::CTask()/CModule::Add() before the Setup()-callgraph
 * link was found (task.h/module.h's own now-superseded "no caller" notes).
 *
 * REAL LAYOUT (CEditor@08249cd0.c ctor, ~CEditor@082498f0.c dtor, byte-exact
 * cross-checked against every one of CEditor's own field-touching methods):
 *   base   CModule            (0x2c bytes, module.h) -- name = ctor's own
 *                              `param_1` argument
 *   +0x2c  CEditClient        (0xc bytes, edit_man.h -- widened this pass,
 *                              see that header's own note)
 *   +0x38  CEditServer        (0x40038 bytes, edit_server.h -- name = the SAME
 *                              `param_1` argument as the CModule base, real
 *                              ctor passes it to both)
 *   +0x40070  CMainTask*      -- CEditor's OWN nested CMainTask (NOT
 *                              CEditMan::CMainTask, edit_man.h -- confirmed
 *                              different, unrelated classes that happen to
 *                              share an unqualified name, per that header's
 *                              own note). Constructed by Setup(), malloc(0x8c).
 *   +0x40074  CPanelIfcTask*  -- panel_ifc_task.h (already partially real).
 *                              Constructed by Setup(), malloc(0xb8).
 *   +0x40078  void* (opaque)  -- CEditor::CChunkServerTask*, own nested class,
 *                              deferred (see Setup()'s own header comment
 *                              below). Constructed by Setup(), malloc(0x94).
 *   +0x4007c  CAlphaKeybIfcTask* -- top-level (NOT nested under CEditor,
 *                              confirmed by its own mangled ctor name
 *                              `_ZN17CAlphaKeybIfcTaskC1ERK7CEditor` --
 *                              `17CAlphaKeybIfcTask`, no enclosing `CEditor`
 *                              qualifier), deferred. Constructed by Setup()
 *                              ONLY if the "ALPHAKEYBOARD" parameter string
 *                              equals "Yes", malloc(0x84).
 *   +0x40080  CParameterString* -- parameter_string.h (new, fully real this
 *                              pass). Null if the ctor's own `alphaKeybParam`
 *                              argument is null.
 *
 * VTABLE: see omega_vtables.h's own header comment for the full 3-array
 * (primary + 2 multiple-inheritance thunk) breakdown, confirmed by a direct
 * .rodata byte read (not inferred) -- PTR__CEditor_08f29b88/08f29bac/08f29bc0.
 *
 * `CEditor::Setup()`'s own fan-out (the reason this class was flagged for a
 * "map the full fan-out before writing code" pass rather than a rushed
 * partial one): it mallocs and constructs 4 sibling CTask-derived objects,
 * `CModule::Add()`-ing each (already-real, module.h). Investigated in full,
 * not assumed:
 *   - CMainTask       tractable in PART: its OWN CTask::CTask() base call is
 *                     real (task.h) and reconstructed as such here. Its real
 *                     ctor TAIL (PegResourceHandler/CreatePegScreen/
 *                     PegMessageQueue/CDesktop construction, CMainTask@
 *                     0824ad90.c) is genuine Peg-toolkit depth -- explicitly
 *                     out of scope project-wide (PLAN.md Stage 5: "Peg toolkit
 *                     substrate -- confirmed not necessary"). Left a Tier-B
 *                     stub tail (mScreen stays null). 2 of its OWN other
 *                     methods (IsSwitchPressed/IsShowCost) are pure static
 *                     global reads/bit-tests with ZERO Peg dependency and ARE
 *                     reconstructed for real here; InitDesktop/
 *                     StopScreenRefresh/EnterCheckHardware/Exec() all touch
 *                     Peg/CDesktop/LinuxFBScreen or a wide DSP-adjacent
 *                     subsystem sweep (CKGAPIControl/CGlobal/CPCMProg/
 *                     CNetConfig/CMIDI/USTGUserAPI/CKGUserAPI/COmegaInterface,
 *                     EnterCheckHardware@0824b020.c) and stay Tier-B stubs.
 *   - CPanelIfcTask   FINDING FOR A FUTURE DEDICATED PASS: its own real ctor
 *                     (CPanelIfcTask@0824b7e0.c) turns out to be FULLY
 *                     tractable now -- every dependency it needs
 *                     (CTask::CTask, CTask::Add(COutLink*), COutLinkMono::
 *                     COutLinkMono(), CSTGUnsolMsgHandler::CSTGUnsolMsgHandler())
 *                     is already real, reconstructed by earlier Stage 6
 *                     batches today. Likewise SetLEDStatus (x3)/ShortBeep/
 *                     EnterDiagnostics/SetupPanelInterface/SetAllLED/
 *                     OnTouchPanelEvent/OnButtonEvent/Exec(CMessage&) all
 *                     route through the already-real `COutLinkMono::OutMono()`
 *                     IPC primitive or `PegMessageQueue::Push()` (the latter
 *                     is the one remaining real Peg-toolkit gap: `PegThing`/
 *                     `PegMessageQueue` are NOT reconstructed anywhere in this
 *                     project). Only `Exec()` (the bare, 0-arg CTask-poll
 *                     override) additionally needs a full second brand-new
 *                     vtable (`CPanelCfg`, an anonymous COutLinkMono-derived
 *                     override the real ctor builds inline) verified
 *                     byte-exact from .rodata before it could be trusted --
 *                     genuinely a SEPARATE class's worth of work (a real new
 *                     CPanelIfcTask vtable + a real new CPanelCfg vtable, each
 *                     needing their own byte-exact-verified slot count, per
 *                     this project's own recurring undersized-vtable bug
 *                     class). NOT attempted this pass to keep this batch to
 *                     CEditor's own scope -- flagged here explicitly as
 *                     ready-to-go for whoever picks up CPanelIfcTask next.
 *                     `panel_ifc_task.h` gained only the minimal additional
 *                     ctor overload Setup() needs to link (Tier-B, see that
 *                     header).
 *   - CChunkServerTask NOT tractable this pass: needs a new base class
 *                     `CChunkServer` (CChunkServerTask@08245f50.c's own base
 *                     ctor call) -- a genuinely separate, non-trivial 21-real-
 *                     method class (OnUnlock/OnRelock/OnBegin/OnEnd/OnSave x2/
 *                     OnLoad x2/OnAbort/OnStoppedByUser/GetSaveBuffSize/
 *                     GetServerID/Unlock/GetServerHandle/Load/Exec, confirmed
 *                     via `nm -C`, .text+0x080cba90..0x080cc0d0) that nothing
 *                     in this project has touched yet. Deferred entirely,
 *                     including CEditor::CChunkServerTask's own 3 trivial
 *                     methods (OnSave/GetSaveBuffSize both just `return 0;`,
 *                     OnLoad forwards to `PegResourceHandler::Load` -- Peg
 *                     again) -- kept as an untyped `void*` member (Setup()
 *                     still stores the real pointer at the real +0x40078
 *                     offset, just without a named type to dereference it).
 *   - CAlphaKeybIfcTask NOT tractable this pass: needs `CEditable`
 *                     (edit_man.h references it only by name, not
 *                     reconstructed) and `CTask::RegisterIfc` (already
 *                     documented Tier-B in task.h). Also gated behind an
 *                     optional boot-config string ("ALPHAKEYBOARD=Yes") this
 *                     reconstruction's own boot path never sets, so even if
 *                     built, Setup()'s own real branch would skip it today.
 *                     Kept as a forward-declared (incomplete-type) pointer
 *                     member -- real class, just not defined anywhere yet.
 */

#ifndef EDITOR_H
#define EDITOR_H

#include "edit_man.h"    /* CModule, CEditClient */
#include "edit_server.h" /* CEditServer */
#include "parameter_string.h"
#include "task.h"        /* CTask (CMainTask's real base) */

class CAlphaKeybIfcTask; /* top-level, NOT nested -- see header comment. Real
                           * class, deferred; only ever used here as an
                           * incomplete-type pointer. */

class CEditor : public CModule {
public:
	class CMainTask;      /* defined immediately below, out-of-line */
	class CPanelIfcTask;  /* defined out-of-line in panel_ifc_task.h */

	/* .text+0x08249cd0, 199 bytes. `alphaKeybParam` is main()'s own optional
	 * config-string argument (real ground truth: only non-null on some
	 * command-line-driven invocations) -- when non-null, parsed into a
	 * CParameterString this ctor owns; Setup() later queries it for
	 * "ALPHAKEYBOARD".
	 */
	CEditor(const char *name, const char *alphaKeybParam);

	/* .text+0x082498f0 (D1, complete-object dtor). */
	~CEditor();

	/* .text+0x082498a0, 3 bytes. Real: unconditionally `return 0;`, same
	 * "confirmed genuinely empty" status as CEditMan::Config()/CDumpManMod::
	 * Config() (edit_man.h/dump_man_mod.h).
	 */
	static int Config();

	/* .text+0x082498b0, 63 bytes. Real: dispatches CMainTask::InitDesktop()/
	 * CPanelIfcTask::SetupPanelInterface() (both Tier-B/Peg-adjacent stubs
	 * today, see those classes) and, only if the optional CAlphaKeybIfcTask
	 * sibling was built by Setup(), a static `CAlphaKeybIfcTask::Setup()`
	 * call (real ground truth: a genuine 0-argument static/tail-call, per
	 * `Start@082498b0.c` -- not an oversight, confirmed via objdump the same
	 * way IsSwitchPressed/IsShowCost/EnterCheckHardware/StopScreenRefresh
	 * below were).
	 */
	int Start();

	/* .text+0x08249b60, 296 bytes. See header comment above for the full
	 * fan-out analysis. Real body mallocs+constructs CMainTask,
	 * CPanelIfcTask, CChunkServerTask (always) and CAlphaKeybIfcTask (only if
	 * the "ALPHAKEYBOARD" parameter equals "Yes"), `CModule::Add()`-ing each.
	 */
	int Setup();

	/* .text+0x08249df0/0x08249e30/0x08249e80, 3 overloads -- all real,
	 * one-line static forwarders to `sm_poTheEditor`'s own
	 * `CPanelIfcTask::SetLEDStatus` (guarded: no-op if no editor instance or
	 * no panel task yet). `ledCode`/`ledState` are the real ground-truth
	 * `ELedCode`/`CPanelCfg::ELedState` enums, kept opaque `int` here (same
	 * treatment as CTask's own opaque `level`/`scheduleFlag` parameters,
	 * task.h) since neither enum is independently reconstructed.
	 */
	static void SetLEDStatus(int ledCode, int ledState);
	static void SetLEDStatus(int deviceIndex, unsigned short onMask, unsigned short offMask);
	static void SetLEDStatus(int ledState);

	/* .text+0x08249eb0, 36 bytes. Real, unconditional forward. */
	static void ShortBeep();

	/* .text+0x08249ee0, 63 bytes. Real: only beeps if
	 * `CStorage::GetGlobal()`'s own byte at +0x6001 has bit 0x40 set --
	 * CStorage is not reconstructed anywhere in this project, so this is a
	 * Tier-B stub (always silent) rather than a guessed implementation.
	 */
	static void ShortBeepPolite();

	/* .text+0x08249f20, 37 bytes. Real, unconditional forward -- NOTE ground
	 * truth's own call site has NO null-check on `sm_poTheEditor`'s panel
	 * task pointer (unlike the 3 SetLEDStatus overloads/ShortBeep above,
	 * which do) -- preserved as found, not "fixed" to match those siblings.
	 */
	static void EnterDiagnostics(int flag);

	/* .text+0x08249f50, 13 bytes. Real: a literal tail-jump to
	 * `CMainTask::IsSwitchPressed` with the SAME argument register/stack
	 * slot passed straight through (confirmed via objdump -- `jmp`, not
	 * `call`+`ret`; this is a genuine GCC sibcall, not a dropped argument).
	 */
	static bool IsSwitchPressed(unsigned int buttonCode);

	/* .text+0x08249f60, 13 bytes. Same tail-jump shape. */
	static bool IsShowCost();

	/* .text+0x08249f70, 13 bytes. Same tail-jump shape. */
	static void EnterCheckHardware(int flag);

	/* .text+0x08249f80, 13 bytes. Same tail-jump shape. */
	static void StopScreenRefresh();

	/* Real global, `_ZN7CEditor15lastEditMessageE`, 0x0939c1e0, 2 bytes.
	 * Declared here now that `CEditor` is a real class rather than a
	 * placeholder namespace -- moved from stg_unsol_msg_handler.cpp's own
	 * former `namespace CEditor { unsigned short lastEditMessage = 0; }`
	 * (still defined/initialized in that .cpp, just as a qualified static
	 * member definition now). See stg_unsol_msg_handler.h/.cpp for every real
	 * write site.
	 */
	static unsigned short lastEditMessage;

private:
	CEditClient        mEditClient;        /* +0x2c */
	CEditServer        mEditServer;        /* +0x38, 0x40038 bytes */
	CMainTask         *mMainTask;          /* +0x40070 */
	CPanelIfcTask     *mPanelIfcTask;      /* +0x40074 */
	void              *mChunkServerTask;   /* +0x40078, deferred (see header) */
	CAlphaKeybIfcTask *mAlphaKeybIfcTask;  /* +0x4007c, deferred (see header) */
	CParameterString  *mAlphaKeybParam;    /* +0x40080 */

	static CEditor *sm_poTheEditor; /* real global, `_ZN7CEditor13sm_poTheEditorE` */

	/* Not implemented -- same "never copied in ground truth" convention as
	 * CParameterString.
	 */
	CEditor(const CEditor &);
	CEditor &operator=(const CEditor &);
};

/* CEditor::CMainTask -- NOT CEditMan::CMainTask (edit_man.h), a completely
 * unrelated class that happens to share an unqualified name. See editor.h's
 * own header comment for exactly which of this class's methods are real vs.
 * Tier-B this pass.
 */
class CEditor::CMainTask : public CTask {
public:
	/* .text+0x0824ad90, 375 bytes. REAL base construction
	 * (`CTask::CTask(owner, "MainTask", 4, 1, 0x804b)`, task.h) -- the real
	 * ctor's own Peg/CDesktop construction tail is Tier-B, see editor.h's own
	 * header comment. `mScreen` stays null.
	 */
	explicit CMainTask(const CEditor &owner);

	/* Tier-B stub -- no real dtor logic reconstructed (ground truth: never
	 * destroyed by any code this project has reconstructed either).
	 */
	~CMainTask();

	/* .text+0x0824af60, 27 bytes. Tier-B stub -- real body is
	 * `CDesktop::Init(mDesktop)`, Peg-toolkit depth.
	 */
	void InitDesktop();

	/* .text+0x0824af80, 59 bytes. REAL: a pure bit test against the real
	 * `s_oSwitchState` global bitmask (2 x 32-bit words, indexed by
	 * `code >> 6`), zero Peg/subsystem dependency.
	 */
	static bool IsSwitchPressed(unsigned int buttonCode);

	/* .text+0x0824afc0, 6 bytes. REAL: reads the real `sShowCost` global,
	 * zero dependency.
	 */
	static bool IsShowCost();

	/* .text+0x0824afd0, 78 bytes. Tier-B stub -- real body
	 * `dynamic_cast<LinuxFBScreen*>(s_poScreen)` then
	 * `LinuxFBScreen::FrameBuffer::FlushFrameBuffer()`, Peg-toolkit depth.
	 */
	static void StopScreenRefresh();

	/* .text+0x0824b020, 294 bytes. Tier-B stub -- real body is a wide
	 * external-subsystem sweep (CKGAPIControl/CGlobal/CPCMProg/CNetConfig/
	 * CMIDI/USTGUserAPI/CKGUserAPI/COmegaInterface::Close(), plus a
	 * `system("sh /korg/Eva/RunckhdwFromEva.sh ...")` call) -- none of those
	 * subsystems are reconstructed in this project.
	 */
	static void EnterCheckHardware(int flag);

	/* .text+0x0824a260, 2744 bytes. Tier-B stub -- real body is Eva's own
	 * main GUI event-pump (PegMessageQueue::Pop/Push, CMMI, CPageMenuLauncher,
	 * CHelpManager, CControllerGrabber, CFanControlStatusHandler,
	 * CSystemClockErrorHandler, CAutoMountHandler, CHardwareMonitor, CTimer,
	 * CSTGUnsolMsgProcessor, CKGMsgProcessor, EditApi vtable dispatch --
	 * genuinely Peg/UI-toolkit depth, the textbook "disproportionate
	 * sub-piece" this batch was told to defer rather than rush).
	 */
	int Exec();

	/* Real field is `PegThing::mpScreen`, stored at this class's own +0x7c
	 * (CTask's own real size is exactly 0x7c, task.h) -- always null in this
	 * Tier-B reconstruction since the real Peg/CreatePegScreen() construction
	 * that would set it is deferred. Exposed so CPanelIfcTask's own (real)
	 * ctor can read it, matching ground truth's own
	 * `*(PegScreen**)(*(int*)(this+0x40070)+0x7c)` access.
	 */
	void *GetScreen() const { return mScreen; }

private:
	void *mScreen;
};

#endif /* EDITOR_H */
