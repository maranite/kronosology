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
 *     +0x88  mOutLink       CSysExMsgClientOutLink* -- NOW real (Eva CSysExMsgClientOutLink
 *                            follow-up pass, 2026-07-25, out_link.h): when
 *                            ECanTransmit requests it, the real ctor mallocs a
 *                            CSysExMsgClientOutLink(*this) and registers it via the
 *                            now-real `CTask::Add(COutLink*)` (task.h); otherwise 0.
 *                            The previous pass's documented deviation ("always stores
 *                            0 regardless of canTransmit") is fixed.
 *     +0x8c  mUnknown8c[4]  4 real bytes this class does NOT yet model semantically
 *                            -- added 2026-07-26 (test_client_comm_server heisenbug
 *                            root-cause fix). Confirmed real, not invented padding:
 *                            `CDumpTask` (dump_task.h) places its own first field at
 *                            +0x90, not +0x8c, per that header's own "own fields
 *                            start at CSysExMsgTaskBase's own 0x8c end" comment --
 *                            so ground truth's base class genuinely runs through
 *                            +0x8f. Before this field existed, sizeof(this class)
 *                            was understated by 4 bytes, so any stack instance
 *                            (e.g. a verify/ test's own local) was undersized for
 *                            `CClientCommServer::MessageToEvent()`'s own deliberate
 *                            `mClient+0x8c` read (client_comm_server.cpp) -- a real
 *                            stack-buffer-overflow, confirmed via
 *                            -fsanitize=address,undefined (100% reproducible), and
 *                            the actual cause of this project's intermittent
 *                            `test_client_comm_server` failures (the garbage byte
 *                            read feeds into `CClientCommServer::mEvTag` bits 8-15,
 *                            which no reset path in that file ever clears).
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
 * Tier A (14 of 14 real ground-truth entries -- CSysExMsgClientOutLink follow-up
 * pass, 2026-07-25, promotes the final 3 now that `CSysExMsgClientOutLink`/
 * `CTask::Add(COutLink*)` exist, out_link.h/task.h):
 *   Exec(CMessage&), OnSexLinkError(), OnReceiveMessage(uchar,uchar,uchar const*,uchar),
 *   OnTimeout() -- unchanged from the prior pass (self-contained, no further
 *   dependency).
 *   CSysExMsgTaskBase() ctor -- fully real now: the `mMask & 0x08` -> `SetMask(1)`
 *     branch and vtable-pair install (Stage 6 SetMask/~CTask batch), PLUS (this pass)
 *     the ECanTransmit==1 branch: malloc(0x38) + placement-construct a real
 *     `CSysExMsgClientOutLink(*this)`, register it via the now-real
 *     `CTask::Add(COutLink*)` (task.h). The previous pass's documented deviation
 *     ("mOutLink stays 0 regardless of canTransmit") is fixed.
 *   SetTimeout(ushort) -- real, including the real fixed-point period computation
 *     and both SetMask() tail-calls (see .cpp); 2 real Api diagnostic-only calls
 *     (vtbl+0x94) are not modeled, matching Exec(CMessage&)'s own established
 *     precedent for that same undecoded slot -- they don't affect control flow.
 *   Exec() -- real, including the real timeout-elapsed check and the raw
 *     slot+0x1c redispatch (see .cpp).
 *   ~CSysExMsgTaskBase() -- real: reinstalls both own vtable identities, then
 *     calls the now-real `CTask::~CTask()`. Ground truth does NOT free `mOutLink`
 *     here (confirmed by reading the real dtor byte-for-byte) -- a real latent leak
 *     in the original binary, faithfully reproduced, not a bug in this
 *     reconstruction.
 *   SendMsg(uchar const*, uchar) -- NOW real (this pass): reads `mCommId` (a soft,
 *     omitted assert fires if it's still 0xff, "uninitialized"), then
 *     `mOutLink->SendMessage(mCommId, data, len)` (out_link.h, real), returns
 *     `result == 0`.
 *   EventToMessage(const void*, uchar*, uchar&) -- NOW real: 2 soft, omitted bounds/
 *     class-code asserts (both non-enforcing, ground truth always falls through
 *     regardless -- confirmed by reading both branch targets), then forwards to
 *     `CSysExApiInstance::EventToMessage(mCommId, ev, out, outLen)` -- a DIRECT
 *     (non-virtual, real-address) call into a genuinely separate, un-reconstructed
 *     sibling class (`g_oSysExApiInstance`, mains.cpp's own existing byte-blob
 *     singleton); given a minimal linkage-only counting stub below (same
 *     "first real caller of a new external symbol" pattern client_comm_server.cpp
 *     already established for CSexServiceTask/COutLinkMono).
 *   MessageToEvent(uchar const*, uchar, void*) -- NOW real: no asserts at all,
 *     pure forward to `CSysExApiInstance::MessageToEvent(mCommId, data, len, ev)`,
 *     same stub as above.
 *
 * `HAL_GetSystemTime()`/`HAL_GetScheduleInterval()` (SetTimeout()/Exec()'s own real
 * dependencies) stay file-local Tier-B stubs (sysex_msg_task_base.cpp), matching
 * ckernel.cpp's own established per-file HAL-stub convention -- neither is
 * reconstructed anywhere in this project yet. `CSysExApiInstance::
 * {EventToMessage,MessageToEvent}` stay minimal counting stubs (see .cpp) -- the
 * class itself (a dozen-plus-method facade, config_manager.cpp's own earlier
 * survey) is genuinely out of scope.
 */

#ifndef SYSEX_MSG_TASK_BASE_H
#define SYSEX_MSG_TASK_BASE_H

#include "task.h"

class CModule;
class CMessage;
class CLinkedEvent;
class CSysExMsgClientOutLink;

/* Forward-declared with only the two real methods this class calls -- the class
 * itself (a dozen-plus-method facade over `g_oSysExApiInstance`, mains.cpp's own
 * existing byte-blob singleton) is genuinely out of scope, same status
 * config_manager.cpp's own earlier `CSysExApi` survey already gave it. Real C++
 * signatures confirmed via `nm -C Eva`:
 *   CSysExApiInstance::EventToMessage(unsigned char, CLinkedEvent const*,
 *     unsigned char*, unsigned char&)  -- .text+0x0817a1a0
 *   CSysExApiInstance::MessageToEvent(unsigned char, unsigned char const*,
 *     unsigned char, CLinkedEvent*)    -- .text+0x0817a0f0
 * (both real, direct, non-virtual call targets in ground truth -- not vtable
 * dispatch, unlike SysExApi's own facade methods).
 */
class CSysExApiInstance {
public:
	void EventToMessage(unsigned char commId, const CLinkedEvent *ev, unsigned char *out,
	                      unsigned char &outLen);
	void MessageToEvent(unsigned char commId, const unsigned char *data, unsigned char len,
	                      CLinkedEvent *ev);
};

class CSysExMsgTaskBase : public CTask {
public:
	/* Real enums not individually named in the decompile (functions.csv only
	 * carries the enum TYPE name, not its enumerators) -- opaque int, same
	 * convention task.h already uses for ETaskLevel/EScheduleFlag. Only 0/1 are
	 * ever passed by any real caller traced so far.
	 */
	enum ECanTransmit { eCanTransmit = 1 };
	enum ENeedsTimeout { eNeedsTimeout = 1 };

	/* .text+0x080a65e0, 268 bytes. Tier A, fully real (this pass adds the
	 * ECanTransmit==1 branch: CSysExMsgClientOutLink construction + CTask::Add,
	 * see header comment / .cpp).
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

	/* .text+0x080a6730, 133 bytes. Tier A -- see header comment / .cpp. */
	bool SendMsg(const unsigned char *data, unsigned char len);

	/* .text+0x080a68c0, 170 bytes. Tier A -- see header comment / .cpp. */
	void EventToMessage(const CLinkedEvent *ev, unsigned char *out, unsigned char &outLen);

	/* .text+0x080a6970, 61 bytes. Tier A -- see header comment / .cpp. */
	void MessageToEvent(const unsigned char *data, unsigned char len, CLinkedEvent *ev);

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
	CSysExMsgClientOutLink *mOutLink; /* +0x88, real when ECanTransmit requests it,
	                                    * else 0 -- see header comment */
	unsigned char mUnknown8c[4]; /* +0x8c, 4 real ground-truth bytes this class does
	                               * not yet model semantically -- confirmed to exist
	                               * (not padding we're inventing) two independent
	                               * ways: (1) `CClientCommServer::MessageToEvent()`
	                               * (client_comm_server.cpp) deliberately reads a raw
	                               * byte at `mClient+0x8c` as "real, but currently
	                               * unmodeled, memory in a sibling class this file
	                               * does not own"; (2) `CDumpTask` (dump_task.h), a
	                               * real derived class, places its OWN first field
	                               * (`mMachine`) at +0x90, not +0x8c, per that
	                               * header's own "REAL LAYOUT (0x98 bytes malloc'd;
	                               * own fields start at CSysExMsgTaskBase's own 0x8c
	                               * end...)" comment -- i.e. ground truth's own base
	                               * class genuinely extends 4 bytes past mOutLink.
	                               * Without this field, sizeof(CSysExMsgTaskBase) was
	                               * understated by 4 bytes, which made every
	                               * stack-allocated instance of this class (e.g.
	                               * verify/test_client_comm_server.cpp's `client`
	                               * local) 4 bytes too small -- the +0x8c read above
	                               * landed on whatever unrelated stack memory
	                               * happened to follow, a genuine stack-buffer-
	                               * overflow (confirmed via
	                               * -fsanitize=address,undefined, 100% reproducible)
	                               * and the root cause of this project's
	                               * `test_client_comm_server` heisenbug history: the
	                               * garbage byte read gets folded into
	                               * `CClientCommServer::mEvTag` bits 8-15, which no
	                               * reset path in that file ever clears, so it can
	                               * silently influence later, seemingly-unrelated
	                               * checks for the rest of the same test run. Kept
	                               * as an opaque byte array (not typed/named) since
	                               * the real field's semantics are still unknown --
	                               * same convention as every other "raw offset,
	                               * meaning TBD" gap already documented in this
	                               * project. */

	/* Friend accessor for verify/test_sysex_msg_task_base.cpp -- reads mOutLink so
	 * the ctor's real ECanTransmit branch can be checked without a public
	 * accessor. Same convention as client_comm_server.h's ClientCommServerTestHooks.
	 */
	friend struct SysExMsgTaskBaseTestHooks;
};

#endif /* SYSEX_MSG_TASK_BASE_H */
