/*
 * editor.cpp  -  see include/editor.h.
 *
 * Transcribed from Decomp/EVA_Decomp/eva_export/functions/CEditor@08249cd0.c
 * (ctor), ~CEditor@082498f0.c (dtor, D1 complete-object destructor -- the
 * exception-unwind-only path calling `CModule::~CModule()` explicitly is
 * omitted, same "happy path only" license as task.cpp/module.cpp), Config@
 * 082498a0.c, Start@082498b0.c, Setup@08249b60.c, SetLEDStatus@08249df0.c /
 * @08249e30.c / @08249e80.c, ShortBeep@08249eb0.c, ShortBeepPolite@08249ee0.c,
 * EnterDiagnostics@08249f20.c, IsSwitchPressed@08249f50.c (CEditor's own
 * tail-jump wrapper) + IsSwitchPressed@0824af80.c (CMainTask's real body),
 * IsShowCost@08249f60.c + @0824afc0.c, EnterCheckHardware@08249f70.c,
 * StopScreenRefresh@08249f80.c.
 */

#include "editor.h"
#include "panel_ifc_task.h"
#include "alpha_keyb_ifc_task.h"
#include "omega_vtables.h"

#include <cstring>

CEditor *CEditor::sm_poTheEditor = 0;
/* CEditor::lastEditMessage's own out-of-line definition lives in
 * stg_unsol_msg_handler.cpp (its original home, moved from a `namespace
 * CEditor { ... }` global to a qualified static-member definition this same
 * pass) -- NOT duplicated here.
 */

/* Real ground truth's own static, `s_oSwitchState` (CMainTask::IsSwitchPressed/
 * Exec()) -- a 2-word (64-bit) bitmask, `code >> 6` selects which of the 2
 * words, `code & 0x3f` the bit within it, inverted (`^1`) per ground truth's
 * own real expression. Only IsSwitchPressed() reads it in this reconstruction
 * (Exec() itself, the only real WRITER, is Tier-B/deferred) -- always reads
 * back 0 (all switches "pressed", per the `^1` inversion of an all-zero
 * bitmask) until Exec() is reconstructed for real.
 */
static unsigned int s_oSwitchState[2];

/* Real ground truth's own static, `sShowCost` (CMainTask::IsShowCost/Exec()) --
 * toggled by Exec() (Tier-B/deferred here), always reads back 0 in this
 * reconstruction.
 */
static unsigned int sShowCost;

CEditor::CMainTask::CMainTask(const CEditor &owner)
	: CTask(owner, "MainTask", 4, 1, 0x804b), mScreen(0)
{
	/* Real ctor tail (CMainTask@0824ad90.c) past this point:
	 *   CTask::SetScheduleInterval(this, 10);
	 *   PegThing::mpOmegaEditClient = &owner's own CEditClient sub-object;
	 *   real PegResourceHandler/CreatePegScreen()/PegMessageQueue/CDesktop
	 *   construction, storing the real PegScreen* at this+0x7c (mScreen here).
	 * All Peg-toolkit depth -- explicitly out of scope project-wide (PLAN.md
	 * Stage 5). mScreen stays null, matching "no real screen was ever built"
	 * rather than fabricating one.
	 */
}

CEditor::CMainTask::~CMainTask()
{
	/* Tier-B stub -- see header. */
}

void CEditor::CMainTask::InitDesktop()
{
	/* Tier-B stub -- real body is `CDesktop::Init(mDesktop)`, Peg-toolkit
	 * depth (CDesktop not reconstructed anywhere in this project).
	 */
}

bool CEditor::CMainTask::IsSwitchPressed(unsigned int buttonCode)
{
	unsigned int word = s_oSwitchState[(buttonCode >> 6) & 1];
	unsigned int bit = buttonCode & 0x3f;
	return ((word >> (bit & 0x1f)) & 1) ^ 1;
}

bool CEditor::CMainTask::IsShowCost()
{
	return sShowCost != 0;
}

void CEditor::CMainTask::StopScreenRefresh()
{
	/* Tier-B stub -- real body is
	 * `dynamic_cast<LinuxFBScreen*>(s_poScreen)->FrameBuffer::FlushFrameBuffer()`,
	 * Peg-toolkit depth (LinuxFBScreen not reconstructed anywhere in this
	 * project).
	 */
}

void CEditor::CMainTask::EnterCheckHardware(int)
{
	/* Tier-B stub -- real body is a wide external-subsystem sweep
	 * (CKGAPIControl/CGlobal/CPCMProg/CNetConfig/CMIDI/USTGUserAPI/
	 * CKGUserAPI/COmegaInterface::Close(), plus a
	 * `system("sh /korg/Eva/RunckhdwFromEva.sh ...")` call) -- none of those
	 * subsystems are reconstructed in this project.
	 */
}

int CEditor::CMainTask::Exec()
{
	/* Tier-B stub -- real body is Eva's own main GUI event-pump, see
	 * editor.h's own header comment. Real return value observed is always 0
	 * along every path that doesn't early-return via the PegPresentation's
	 * own vtable+0xb4 call (a real `case 0x13` "assertion failed" abort path,
	 * also not modeled here).
	 */
	return 0;
}

CEditor::CChunkServerTask::CChunkServerTask(const CModule &owner)
	: CChunkServer(owner, 0)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CChunkServerTask_08f25b88;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
	    (void *)PTR__CChunkServerTask_08f25bd0;
}

CEditor::CChunkServerTask::~CChunkServerTask()
{
	/* Real ground truth reinstalls the SAME 2 identities its own ctor
	 * installed -- see editor.h's own header comment for why the final
	 * observable state after a full unwind ends up being CChunkServer's OWN
	 * identity (its base dtor's own reinstall), not CChunkServerTask's.
	 */
	*reinterpret_cast<void **>(this) = (void *)PTR__CChunkServerTask_08f25b88;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
	    (void *)PTR__CChunkServerTask_08f25bd0;

	/* CChunkServer::~CChunkServer() runs automatically after this body
	 * returns (real single C++ inheritance), same as every other
	 * CTask-derived class in this project.
	 */
}

unsigned int CEditor::CChunkServerTask::OnSave(CChunk *, unsigned char, unsigned char *, unsigned long)
{
	return 0;
}

int CEditor::CChunkServerTask::GetSaveBuffSize(unsigned char, unsigned char, unsigned char) const
{
	return 0;
}

unsigned int CEditor::CChunkServerTask::OnLoad(CChunk *, unsigned char, unsigned char *, unsigned long)
{
	/* Tier B link-stub -- see header comment (real body's non-trivial branch
	 * calls PegResourceHandler::Load(), Peg-toolkit depth). Every OTHER real
	 * path returns 0, modeled here as unconditional.
	 */
	return 0;
}

CEditor::CEditor(const char *name, const char *alphaKeybParam)
	: CModule(name), mEditClient(), mEditServer(name),
	  mMainTask(0), mChunkServerTask(0)
	  /* mPanelIfcTask/mAlphaKeybIfcTask deliberately NOT initialized here --
	   * see below. */
{
	mVtbl = (void *)PTR__CEditor_08f29b88;
	mEditClient.mVtbl = (void *)PTR__CEditor_08f29bac;
	mEditServer.mVtbl = (void *)PTR__CEditor_08f29bc0;

	sm_poTheEditor = this;

	/* PRESERVED REAL QUIRK, not "fixed": ground truth's own ctor
	 * (CEditor@08249cd0.c) explicitly zeroes ONLY `this+0x40070` (mMainTask)
	 * and `this+0x40078` (mChunkServerTask) -- `this+0x40074` (mPanelIfcTask)
	 * and `this+0x4007c` (mAlphaKeybIfcTask) are NEVER written by the real
	 * ctor at all. `Setup()` unconditionally constructs mPanelIfcTask right
	 * after mMainTask, so that field's own transient uninitialized window is
	 * harmless in practice -- but mAlphaKeybIfcTask genuinely CAN stay
	 * uninitialized garbage if Setup()'s own "ALPHAKEYBOARD" branch isn't
	 * taken, since nothing else ever initializes it. `CEditor::Start()`
	 * (below) reads it with a bare `!= 0` check and no other safeguard --
	 * a real, load-bearing uninitialized-read hazard in ground truth,
	 * transcribed faithfully (mAlphaKeybIfcTask is left genuinely
	 * uninitialized here too, matching malloc's own no-zero behavior) rather
	 * than silently zeroed to "fix" it.
	 */

	if (alphaKeybParam == 0) {
		mAlphaKeybParam = 0;
	} else {
		mAlphaKeybParam = new CParameterString(alphaKeybParam);
	}

	/* Live-boot verification note (2026-07-26): a temporary
	 * puts("CEDITOR_CTOR_MARKER...")+fflush() was added here, rebuilt, and
	 * booted live in kronos_vm -- confirmed present, exactly once, in
	 * /korg/rw/eva_stdout.log, proving CEditor::CEditor() genuinely runs on
	 * the real construction path (mains.cpp's CEditorConstructor::Create()
	 * -> placement new), not just reachable in source. Removed again after
	 * confirmation since ground truth's own ctor has no such call; see
	 * agent-memory eva_ceditor_construction_confirmed_2026-07-26.md for the
	 * full finding (this also surfaced and fixed a real s_bRunning
	 * mis-initialization bug in omega_interface.cpp).
	 */
}

CEditor::~CEditor()
{
	if (mAlphaKeybParam != 0) {
		delete mAlphaKeybParam;
	}
	/* Ground truth's own dtor (~CEditor@082498f0.c) does NOT free
	 * mMainTask/mPanelIfcTask/mChunkServerTask/mAlphaKeybIfcTask -- transcribed
	 * as found, not "completed". mEditServer/mEditClient (this class's own
	 * real sub-objects) are destructed automatically by the compiler after
	 * this body returns, same as ground truth's own explicit
	 * ~CEditServer()/~CEditClient() tail calls.
	 */
}

int CEditor::Config()
{
	return 0;
}

int CEditor::Start()
{
	if (mMainTask != 0)
		mMainTask->InitDesktop();
	/* Real ground truth calls `CPanelIfcTask::SetupPanelInterface()`
	 * unconditionally here (no null-check, matching EnterDiagnostics()'s own
	 * unchecked-pointer shape below). SetupPanelInterface() is now real
	 * (panel_ifc_task.h/.cpp, Stage 6 dedicated CPanelIfcTask batch,
	 * 2026-07-25) -- guarded here only to stay safe against mPanelIfcTask's
	 * own documented possibly-uninitialized state (see ctor comment) rather
	 * than matching ground truth's own unchecked dereference.
	 */
	if (mPanelIfcTask != 0)
		mPanelIfcTask->SetupPanelInterface();
	if (mAlphaKeybIfcTask != 0) {
		/* Real ground truth: `CAlphaKeybIfcTask::Setup()`, unqualified
		 * static call, no `this` argument -- now real (alpha_keyb_ifc_task.h),
		 * confirmed via objdump/decompile to be a literal 1-byte `ret`, no
		 * other real body to model.
		 */
		CAlphaKeybIfcTask::Setup();
	}
	return 0;
}

int CEditor::Setup()
{
	mMainTask = new CMainTask(*this);
	Add(mMainTask);

	void *screen = mMainTask->GetScreen();
	mPanelIfcTask = new CPanelIfcTask(this, screen);
	Add(mPanelIfcTask);

	/* CEditor::CChunkServerTask -- now real (chunk_server.h, this session,
	 * 2026-07-26), unconditionally malloc(0x94)+constructed+Add()'d, matching
	 * ground truth exactly (unlike the ALPHAKEYBOARD-gated sibling below,
	 * this one always runs).
	 */
	mChunkServerTask = new CChunkServerTask(*this);
	Add(mChunkServerTask);

	/* Real "ALPHAKEYBOARD" gate -- kept fully real (ground truth's own
	 * Duff's-device-unrolled 4-character compare against "Yes" collapses
	 * exactly to `strcmp(value, "Yes") == 0`, comparing all 4 bytes
	 * including the nul terminator) even though the actual CAlphaKeybIfcTask
	 * construction it would gate is deferred (see below) -- this way the
	 * branch condition itself stays observably correct for any future code
	 * that wants to know whether Setup() WOULD have built the alpha-keyboard
	 * sibling.
	 */
	if (mAlphaKeybParam != 0) {
		const char *value = mAlphaKeybParam->GetParamStr("ALPHAKEYBOARD");
		if (value != 0 && strcmp(value, "Yes") == 0) {
			/* CAlphaKeybIfcTask -- top-level (not nested under CEditor, see
			 * editor.h). Now real (alpha_keyb_ifc_task.h/.cpp, Stage 6
			 * breadth sweep 2026-07-25) -- wired in here now that
			 * `CEditor::Setup()` itself is confirmed live-boot-reachable
			 * (eva_createusermodules_editor_unlock_2026-07-26): the real
			 * `s_atCreateInfo` row for "EditorClass" passes literal string
			 * "ALPHAKEYBOARD=Yes" as this ctor's own `alphaKeybParam`
			 * argument (config_info.cpp), so this branch is NOT a dead
			 * condition on the real, now-unlocked boot path -- it is taken.
			 */
			mAlphaKeybIfcTask = new CAlphaKeybIfcTask(*this);
			Add(mAlphaKeybIfcTask);
		}
	}

	return 0;
}

void CEditor::SetLEDStatus(int ledCode, int ledState)
{
	if (sm_poTheEditor != 0 && sm_poTheEditor->mPanelIfcTask != 0)
		sm_poTheEditor->mPanelIfcTask->SetLEDStatus(ledCode, ledState);
}

void CEditor::SetLEDStatus(int deviceIndex, unsigned short onMask, unsigned short offMask)
{
	if (sm_poTheEditor != 0 && sm_poTheEditor->mPanelIfcTask != 0)
		sm_poTheEditor->mPanelIfcTask->SetLEDStatus(deviceIndex, onMask, offMask);
}

void CEditor::SetLEDStatus(int ledState)
{
	if (sm_poTheEditor != 0 && sm_poTheEditor->mPanelIfcTask != 0)
		sm_poTheEditor->mPanelIfcTask->SetLEDStatus(ledState);
}

void CEditor::ShortBeep()
{
	if (sm_poTheEditor != 0 && sm_poTheEditor->mPanelIfcTask != 0)
		sm_poTheEditor->mPanelIfcTask->ShortBeep();
}

void CEditor::ShortBeepPolite()
{
	/* Tier-B: real gate is `CStorage::GetGlobal()`'s own byte at +0x6001,
	 * bit 0x40 -- CStorage not reconstructed anywhere in this project, so
	 * this stays a silent no-op rather than a guessed always/never branch.
	 */
}

void CEditor::EnterDiagnostics(int flag)
{
	/* Real ground truth has NO null-check on sm_poTheEditor's panel task
	 * pointer here (unlike SetLEDStatus/ShortBeep above) -- transcribed as
	 * found now that CPanelIfcTask::EnterDiagnostics() is real
	 * (panel_ifc_task.h/.cpp).
	 */
	sm_poTheEditor->mPanelIfcTask->EnterDiagnostics(flag);
}

bool CEditor::IsSwitchPressed(unsigned int buttonCode)
{
	return CMainTask::IsSwitchPressed(buttonCode);
}

bool CEditor::IsShowCost()
{
	return CMainTask::IsShowCost();
}

void CEditor::EnterCheckHardware(int flag)
{
	CMainTask::EnterCheckHardware(flag);
}

void CEditor::StopScreenRefresh()
{
	CMainTask::StopScreenRefresh();
}
