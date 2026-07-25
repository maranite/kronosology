/*
 * sysex_msg_task_base.h  -  CSysExMsgTaskBase, the abstract base every real SysEx
 * client task (CDumpTask, and presumably the CSK/expansion-set SysEx handlers) derives
 * from -- Stage 6 breadth sweep, 2026-07-25.
 *
 * GROUND TRUTH REACHABILITY (the actual point of this pass): `CSysExMsgTaskBase::
 * CSysExMsgTaskBase(CModule const&, ECanTransmit, ENeedsTimeout)` (.text+0x080a65e0) is
 * called for real from `CDumpTask::CDumpTask(CModule const&)` (.text+0x080d1b10, not
 * itself reconstructed here -- out of scope, see below), itself called from
 * `CDumpManMod::Setup()` (.text+0x080cf650, CDumpManMod's own real vtable slot +8) --
 * dispatched by `CModuleManager::Setup()`'s already-real per-module vtable+8 loop
 * (module_manager.cpp) once `MMainDumpMan()` (mains.cpp, already Tier A) registers a
 * `CDumpManMod` module via `CModuleManager::AddModule()` (also already Tier A). This is
 * the SAME "real, boot-path-adjacent, not yet exercised by this reconstruction's own
 * wired vtables" shape `CSTGUnsolMsgHandler`/`CEditor::Setup()` established (Eva batch
 * 6, stg_unsol_msg_handler.h) -- confirmed by direct `objdump -dr` call-site tracing,
 * not guessed. `CDumpTask` itself is NOT reconstructed this pass (deep DumpManager
 * protocol logic, out of scope) -- only the base class its ctor calls into.
 *
 * A SECOND, independent real caller exists for the underlying dispatch mechanism:
 * `CConfigManager::SetupSysex()` (.text+0x08056b90, config_manager.cpp, upgraded to
 * Tier A this same pass) constructs a `CClientCommServer` (client_comm_server.h) that
 * takes a `CSysExMsgTaskBase&` -- see that header for the fuller caller chain
 * (`CKernel::InitUserLayer()` -> `CConfigManager::SetupSysex()` -> `SysExApi->
 * RegisterMessageClient()` virtual dispatch -> `CClientCommServer::CClientCommServer`).
 *
 * REAL LAYOUT (base CTask, task.h, 0x7c bytes, THEN):
 *     +0x78/+0x7a  (== CTask's own mPeriod/mCountdown, task.h)   -- written by
 *                  SetTimeout(), not this class's own storage
 *     +0x7c  mTimeoutStart  uint, SetTimeout()'s own `HAL_GetSystemTime()` snapshot,
 *                  read back by Exec() -- this IS this class's own first added field
 *                  (a prior pass's "observed gap: ctor writes this class's own first
 *                  new field at +0x80, 4 bytes past CTask's documented 0x7c-byte size,
 *                  not explained" is now resolved: SetTimeout(), not the ctor, is what
 *                  populates +0x7c, so there never was a gap)
 *     +0x80  mTimeoutTicks  uint, SetTimeout()'s own raw millisecond value, read back
 *                  by Exec()
 *     +0x84  mCommId        byte, the comm/sysex-id this task answers to; ctor sets
 *                            0xff ("uninitialized") unconditionally
 *     +0x88  mOutLink       COutLink* -- real ctor conditionally constructs a
 *                            CSysExMsgClientOutLink (client_comm_server.h family, out
 *                            of scope, see .cpp) and calls the not-yet-reconstructed
 *                            `CTask::Add(COutLink*)` (a DIFFERENT overload from the
 *                            already-real `CModule::Add(CTask*)`, module.h) when
 *                            ECanTransmit requests it; this pass always stores 0 here
 *                            instead (documented deviation, see sysex_msg_task_base.cpp)
 *
 * Real vtable (PTR__CSysExMsgTaskBase_08e84c28 primary + a secondary at +0x8e84c50,
 * both installed by the ctor/dtor now that they're reconstructed -- omega_vtables.h)
 * is never dispatched through by any reconstructed code except this class's OWN
 * `Exec(CMessage&)` (raw slot+0x14) and `Exec()` (raw slot+0x1c, see .cpp) --
 * meaningless without a real derived class (CDumpTask) actually constructed, so this
 * pass's placeholder vtable is never exercised end-to-end (same "genuinely reachable
 * in ground truth, dead in this reconstruction's own current call graph" category as
 * task.h's own CTask note).
 *
 * Tier A (11 of 14 real ground-truth entries -- Stage 6 SetMask/~CTask batch,
 * 2026-07-25, promotes 6 of these from Tier B now that `CTask::SetMask()`/
 * `CTask::~CTask()` exist, task.h):
 *   Exec(CMessage&), OnSexLinkError(), OnReceiveMessage(uchar,uchar,uchar const*,uchar),
 *   OnTimeout() -- unchanged from the prior pass (self-contained, no SetMask/~CTask
 *   dependency).
 *   CSysExMsgTaskBase() ctor -- NOW real for the `mMask & 0x08` -> `SetMask(1)` branch
 *     and the vtable-pair install; the ECanTransmit==1 branch (malloc a
 *     CSysExMsgClientOutLink, call `CTask::Add(COutLink*)`) is STILL not modeled --
 *     unrelated dependency, see Tier B below.
 *   SetTimeout(ushort) -- NOW real, including the real fixed-point period computation
 *     and both SetMask() tail-calls (see .cpp); 2 real Api diagnostic-only calls
 *     (vtbl+0x94) are not modeled, matching Exec(CMessage&)'s own established
 *     precedent for that same undecoded slot -- they don't affect control flow.
 *   Exec() -- NOW real, including the real timeout-elapsed check and the raw
 *     slot+0x1c redispatch (see .cpp).
 *   ~CSysExMsgTaskBase() -- NOW real: reinstalls both own vtable identities, then
 *     calls the now-real `CTask::~CTask()`.
 *
 * Tier B (blocked on CURRENTLY-OUT-OF-SCOPE real dependencies, not guessed):
 *   CSysExMsgTaskBase()'s ECanTransmit==1 branch, SendMsg(uchar const*,uchar),
 *     EventToMessage(...), MessageToEvent(...) -- all terminate in
 *     `CSysExMsgClientOutLink`/`CSexServiceTask`, which themselves sit on top of
 *     `CSysExMsgOutLink`/`COutLinkMono`/`COutLink` -- a genuinely separate,
 *     un-reconstructed output-link subsystem shared broadly across Eva's message
 *     routing, matching the same "pulls in a further out-of-scope subsystem, defer"
 *     bar task.h's own RegisterIfc()/CPoller precedent already set for this project.
 *     Unrelated to SetMask/~CTask -- promoting those didn't unblock this branch.
 *   `HAL_GetSystemTime()`/`HAL_GetScheduleInterval()` (SetTimeout()/Exec()'s own real
 *     dependencies) stay file-local Tier-B stubs (sysex_msg_task_base.cpp), matching
 *     ckernel.cpp's own established per-file HAL-stub convention -- neither is
 *     reconstructed anywhere in this project yet.
 */

#ifndef SYSEX_MSG_TASK_BASE_H
#define SYSEX_MSG_TASK_BASE_H

#include "task.h"

class CModule;
class CMessage;

class CSysExMsgTaskBase : public CTask {
public:
	/* Real enums not individually named in the decompile (functions.csv only
	 * carries the enum TYPE name, not its enumerators) -- opaque int, same
	 * convention task.h already uses for ETaskLevel/EScheduleFlag. Only 0/1 are
	 * ever passed by any real caller traced so far.
	 */
	enum ECanTransmit { eCanTransmit = 1 };
	enum ENeedsTimeout { eNeedsTimeout = 1 };

	/* .text+0x080a65e0, 268 bytes. Tier A for the vtable-pair install and the
	 * `mMask & 0x08` -> `SetMask(1)` branch; the ECanTransmit==1 branch
	 * (CSysExMsgClientOutLink construction + CTask::Add) is still NOT modeled,
	 * see header comment / .cpp.
	 */
	CSysExMsgTaskBase(const CModule &owner, int canTransmit, int needsTimeout);

	/* .text+0x080a67c0, 247 bytes. Tier A -- see header comment / .cpp. */
	void SetTimeout(unsigned short milliseconds);

	/* .text+0x080a64f0, 171 bytes. Tier A -- pure argument redispatch through this
	 * class's own (derived-class-overridden) vtable slot +0x14. Real signature
	 * takes CMessage&; this pass's CMessage is opaque (matching every other
	 * consumer in this project, e.g. omega_interface.h).
	 */
	int Exec(CMessage &msg);

	/* .text+0x080a65a0, 57 bytes. Tier A -- see header comment / .cpp. */
	void Exec();

	/* .text+0x080a6730, 133 bytes. Tier B -- needs CSysExMsgClientOutLink::
	 * SendMessage(), see header comment.
	 */
	bool SendMsg(const unsigned char *data, unsigned char len);

	/* .text+0x080a68c0, 170 bytes. Tier B -- needs CSysExApiInstance::
	 * EventToMessage() (-> CSexServiceTask, out of scope), see header comment.
	 */
	void EventToMessage(const void *linkedEvent, unsigned char *out, unsigned char &outLen);

	/* .text+0x080a6970, 61 bytes. Tier B -- same reason as EventToMessage().
	 */
	void MessageToEvent(const unsigned char *data, unsigned char len, void *linkedEvent);

	/* .text+0x08184ed0/0x08184ec0/0x08184ee0, 1/3/1 bytes -- real, genuinely empty
	 * `return;` bodies in the shipped binary (confirmed by reading each decompile
	 * by hand). Tier A: transcribed exactly as found, not a placeholder.
	 */
	/* NOT declared C++ `virtual` -- this project's own convention (task.h/module.h)
	 * is a manually-managed raw `mVtbl` pointer, never real C++ polymorphism, since
	 * ground truth's own vtable slot numbering never matches whatever a real
	 * compiler-emitted vtable here would independently choose. These three are
	 * real ground-truth virtual overrides (hence documented as such), but nothing
	 * in this reconstruction ever dispatches to them through C++'s own vtable
	 * mechanism -- see Exec(CMessage&)'s own raw `vt[0x14/4]` dispatch above for
	 * how a real cross-class virtual call is modeled here instead.
	 */
	void OnSexLinkError();
	int OnReceiveMessage(unsigned char a, unsigned char b, const unsigned char *c,
	                      unsigned char d);
	void OnTimeout();

	/* .text+0x08184ef0 (+2 non-virtual thunks). Tier A -- see header comment /
	 * .cpp.
	 */
	~CSysExMsgTaskBase();

private:
	unsigned int  mTimeoutStart; /* +0x7c, SetTimeout()'s own HAL_GetSystemTime()
	                               * snapshot, read back by Exec() -- see header
	                               * comment (this class's own real first field,
	                               * not a gap) */
	unsigned int  mTimeoutTicks; /* +0x80, SetTimeout()'s own raw value, read back
	                               * by Exec() */
	unsigned char mCommId;       /* +0x84 */
	void         *mOutLink;      /* +0x88, always 0 this pass -- see header comment */
};

#endif /* SYSEX_MSG_TASK_BASE_H */
