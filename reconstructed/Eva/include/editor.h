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
 *   +0x40074  CPanelIfcTask*  -- panel_ifc_task.h, fully real (Stage 6
 *                              dedicated CPanelIfcTask batch). Constructed by
 *                              Setup(), malloc(0xb8).
 *   +0x40078  CChunkServerTask* -- own nested class, now fully real (this
 *                              session, 2026-07-26 -- see Setup()'s own
 *                              header comment below and chunk_server.h).
 *                              Constructed by Setup(), malloc(0x94),
 *                              UNCONDITIONALLY (not gated, unlike the
 *                              ALPHAKEYBOARD sibling).
 *   +0x4007c  CAlphaKeybIfcTask* -- top-level (NOT nested under CEditor,
 *                              confirmed by its own mangled ctor name
 *                              `_ZN17CAlphaKeybIfcTaskC1ERK7CEditor` --
 *                              `17CAlphaKeybIfcTask`, no enclosing `CEditor`
 *                              qualifier). Now real (alpha_keyb_ifc_task.h)
 *                              AND wired into Setup()/Start() (this session,
 *                              2026-07-26) -- see that bullet below and
 *                              editor.cpp for the wiring itself. Constructed
 *                              by Setup() ONLY if the "ALPHAKEYBOARD"
 *                              parameter string equals "Yes", malloc(0x84) --
 *                              and, per config_info.cpp's own real
 *                              `s_atCreateInfo` "EditorClass" row (literal
 *                              `"ALPHAKEYBOARD=Yes"`), this branch IS taken
 *                              on the real boot path now that
 *                              `CConfigManager::CreateUserModules()` is
 *                              unlocked (eva_createusermodules_editor_unlock_
 *                              2026-07-26) -- not a dead/never-taken branch.
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
 *   - CPanelIfcTask   STALE NOTE, CORRECTED 2026-07-26: this paragraph
 *                     originally flagged CPanelIfcTask as a "finding for a
 *                     future dedicated pass" (the CEditor batch itself, this
 *                     same day, deliberately left it Tier-B to stay in scope).
 *                     That future pass already happened, same day
 *                     (eva_panel_ifc_task_dedicated_batch.md, commit
 *                     `365743a`) -- `CPanelIfcTask` (ctor/dtor/SetMargin/
 *                     GetMargin/SetLEDStatus x3/ShortBeep/EnterDiagnostics/
 *                     SetupPanelInterface/SetAllLED/OnTouchPanelEvent/
 *                     OnButtonEvent/both Exec overloads/OnAnalogEvent/
 *                     OnEncoderEvent) AND its own new `CPanelCfg` sub-object
 *                     class are BOTH fully real now, including the "full
 *                     second brand-new vtable" this note used to flag as
 *                     outstanding work -- see panel_ifc_task.h for the
 *                     complete, current reconstruction and its own real
 *                     byte-exact vtable derivation. Left this note in place
 *                     (rather than deleted) as a marker of how fast this
 *                     specific "flagged, not yet attempted" claim went stale
 *                     -- worth remembering generally: check panel_ifc_task.h
 *                     directly rather than trusting this file's own account
 *                     of CPanelIfcTask's status.
 *   - CChunkServerTask now fully real AND wired (this session, 2026-07-26,
 *                     the same fan-out re-survey that unlocked CAlphaKeybIfcTask
 *                     above). The base class this needs, `CChunkServer`
 *                     (chunk_server.h, .text+0x080cba90..0x080cc0d0),
 *                     turned out genuinely tractable once actually surveyed --
 *                     18 of its own 21 real methods are either 1-6 byte
 *                     trivial constant returns or small (<100 byte) real
 *                     logic; only `Load()`/`Exec(CMessage&)` stay Tier B
 *                     (genuine chunk-protocol depth / an unrecoverable
 *                     indirect-call argument list even Ghidra's own
 *                     decompiler flagged). `CEditor::CChunkServerTask`'s own
 *                     3 overrides (OnSave/GetSaveBuffSize both just
 *                     `return 0;`, both real; OnLoad forwards to
 *                     `PegResourceHandler::Load` -- Peg-toolkit depth, stays
 *                     Tier B) are below. UNLIKE `CAlphaKeybIfcTask`'s
 *                     "ALPHAKEYBOARD=Yes" gate, this construction is
 *                     UNCONDITIONAL in `Setup()` -- always on the real boot
 *                     path, not contingent on any config string.
 *   - CAlphaKeybIfcTask now fully real AND wired (this session, 2026-07-26,
 *                     following up on the now-live `CreateUserModules()` fix --
 *                     eva_createusermodules_editor_unlock_2026-07-26). Was
 *                     reconstructed standalone but deliberately left unwired
 *                     by an earlier Stage 6 breadth-sweep batch (see
 *                     alpha_keyb_ifc_task.h's own header comment) to avoid
 *                     touching this file during concurrent CEditor work, and
 *                     because at the time `CreateUserModules()`'s own bug
 *                     meant `CEditor`'s ctor never actually received the real
 *                     `"ALPHAKEYBOARD=Yes"` config string anyway -- so the
 *                     branch was a dead condition on this project's own
 *                     traced boot path either way. Both preconditions are now
 *                     resolved: `CEditable`/`CTask::RegisterIfc` (task.h) are
 *                     real, and `s_atCreateInfo`'s real "EditorClass" row
 *                     (config_info.cpp) DOES pass `"ALPHAKEYBOARD=Yes"` as
 *                     `CEditor`'s own ctor argument on the real boot path --
 *                     this branch is taken for real. `CAlphaKeybIfcTask::
 *                     Setup()` (`CEditor::Start()`'s own conditional call,
 *                     see below) is also now real -- ground truth's own body
 *                     is a literal 1-byte `ret`, confirmed via objdump, so
 *                     there was nothing left to defer.
 */

#ifndef EDITOR_H
#define EDITOR_H

#include "edit_man.h"    /* CModule, CEditClient */
#include "edit_server.h" /* CEditServer */
#include "parameter_string.h"
#include "task.h"        /* CTask (CMainTask's real base) */
#include "chunk_server.h" /* CChunkServer (CChunkServerTask's real base) */

class CAlphaKeybIfcTask; /* top-level, NOT nested -- see header comment. Real
                           * class, deferred; only ever used here as an
                           * incomplete-type pointer. */

class CEditor : public CModule {
public:
	class CMainTask;        /* defined immediately below, out-of-line */
	class CPanelIfcTask;    /* defined out-of-line in panel_ifc_task.h */
	class CChunkServerTask; /* defined immediately below, out-of-line */

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

	/* .text+0x082498b0, 63 bytes. Real: dispatches `CMainTask::InitDesktop()`
	 * (Tier-B/Peg-adjacent stub, see that class) and
	 * `CPanelIfcTask::SetupPanelInterface()` (fully real, Stage 6 dedicated
	 * CPanelIfcTask batch) unconditionally, and, only if the optional
	 * `CAlphaKeybIfcTask` sibling was built by `Setup()`, a static
	 * `CAlphaKeybIfcTask::Setup()` call (real ground truth: a genuine
	 * 0-argument static/tail-call, per `Start@082498b0.c` -- not an
	 * oversight, confirmed via objdump the same way
	 * IsSwitchPressed/IsShowCost/EnterCheckHardware/StopScreenRefresh below
	 * were; that callee is itself now real too -- a literal 1-byte `ret`,
	 * alpha_keyb_ifc_task.h).
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
	CChunkServerTask  *mChunkServerTask;   /* +0x40078, now real (see header) */
	CAlphaKeybIfcTask *mAlphaKeybIfcTask;  /* +0x4007c, now real+wired (see header) */
	CParameterString  *mAlphaKeybParam;    /* +0x40080 */

	static CEditor *sm_poTheEditor; /* real global, `_ZN7CEditor13sm_poTheEditorE` */

	/* Not implemented -- same "never copied in ground truth" convention as
	 * CParameterString.
	 */
	CEditor(const CEditor &);
	CEditor &operator=(const CEditor &);

	/* Friend accessor for verify/test_editor.cpp -- CEditor has no public
	 * accessor for mAlphaKeybIfcTask (ground truth doesn't either), same
	 * "friend pokes private state" convention as PanelIfcTaskTestHooks
	 * (panel_ifc_task.h)/OutLinkTestHooks (out_link.h).
	 */
	friend struct EditorTestHooks;
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

/* CEditor::CChunkServerTask -- own nested class, base CChunkServer
 * (chunk_server.h). Reconstructed 2026-07-26 alongside the CAlphaKeybIfcTask
 * wiring, once CEditor::Setup() itself was confirmed live-boot-reachable --
 * see editor.h's own header comment above and chunk_server.h for the base
 * class's own full writeup.
 *
 * REAL LAYOUT: no new fields of its own (`CChunkServerTask@08245f50.c`, 52
 * bytes, base-constructs `CChunkServer::CChunkServer(owner, 0)` [EAccessMode
 * literal 0] then installs its own primary + secondary vtable identities,
 * nothing else) -- matches `malloc(0x94)`, the SAME size as its own base
 * `CChunkServer` (chunk_server.h).
 *
 * VTABLE: real 16-slot primary + 3-slot secondary, SAME shape/size as
 * `CChunkServer`'s own (confirmed byte-exact via a direct `.rodata` dword
 * read at 0x08f25b78 -- omega_vtables.h) -- only 3 of the 16 primary slots
 * differ from `CChunkServer`'s own (GetSaveBuffSize/OnSave(CChunk*)/
 * OnLoad(CChunk*), slots 5/10/12), confirmed address-for-address against
 * every other slot.
 */
class CEditor::CChunkServerTask : public CChunkServer {
public:
	/* .text+0x08245f50, 52 bytes. `owner` stands in for the real ctor's
	 * `CModule const&` argument (matches ground truth's own parameter type
	 * exactly -- unlike CAlphaKeybIfcTask's own upcast-from-CEditor
	 * convenience, this ctor's real argument IS just `CModule const&`).
	 */
	explicit CChunkServerTask(const CModule &owner);

	/* .text+0x0898f6a0 (D1). Real ground truth reinstalls the SAME 2 vtable
	 * identities its own ctor installed, then falls through to the
	 * (automatic) base `CChunkServer::~CChunkServer()` -- same "written, then
	 * immediately superseded by the inherited cleanup" shape already
	 * documented elsewhere in this project (e.g. CAlphaKeybIfcTask's own
	 * dtor, alpha_keyb_ifc_task.h) EXCEPT here the derived write ISN'T
	 * superseded (CChunkServer's own dtor reinstalls the SAME
	 * PTR__CChunkServer identities CChunkServerTask's own derived write
	 * would otherwise be overwritten to -- but since CChunkServerTask's own
	 * dtor writes ITS OWN identities first and CChunkServer's dtor doesn't
	 * know that happened, the final observable state after a full
	 * `~CChunkServerTask()` unwind is genuinely CChunkServer's OWN identity,
	 * not CChunkServerTask's -- transcribed faithfully, not "fixed" to keep
	 * the derived identity).
	 */
	~CChunkServerTask();

	/* .text+0x08245ec0, 3 bytes. REAL: unconditionally `return 0;`
	 * (overrides CChunkServer::OnSave(CChunk*,...), which also just returns
	 * 0 -- this override is a genuine, if behaviorally redundant, real
	 * ground-truth override, not a no-op fabrication).
	 */
	unsigned int OnSave(CChunk *chunk, unsigned char a, unsigned char *b, unsigned long c);

	/* .text+0x08245ed0, 3 bytes. REAL: unconditionally `return 0;` (same
	 * "redundant but genuine override" status as OnSave above).
	 */
	int GetSaveBuffSize(unsigned char a, unsigned char b, unsigned char c) const;

	/* .text+0x08245ee0, 107 bytes. Tier B link-stub -- real body (for
	 * `param3 > 2 && (param4[0]-0xd) < 2`) calls
	 * `PegResourceHandler::Load(PegThing::mpResourceHandler, a, param4[0],
	 * param4[1], param4[2])` -- Peg-toolkit depth, the same gap
	 * panel_ifc_task.h's own header comment already documents project-wide.
	 * Real ground truth returns 0 on every other path -- modeled here as an
	 * unconditional `return 0;`, matching every path this reconstruction can
	 * actually exercise without fabricating the Peg call.
	 */
	unsigned int OnLoad(CChunk *chunk, unsigned char a, unsigned char *b, unsigned long c);
};

#endif /* EDITOR_H */
