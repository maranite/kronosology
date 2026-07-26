/*
 * out_link.h  -  COutLink / COutLinkMono / CSysExMsgOutLink / CSysExMsgClientOutLink,
 * the real output-link base-class family `CTask::Add(COutLink*)` (task.h) and
 * `CSysExMsgTaskBase`'s own ctor (sysex_msg_task_base.h) both construct against --
 * Eva CSysExMsgClientOutLink follow-up pass, 2026-07-25 (unblocks
 * `CClientCommServer::SendMessageToClient()`'s own `COutLinkMono::OutMono()` call,
 * client_comm_server.h/cpp, previously a linkage-only counting stub; and
 * `CSysExMsgTaskBase::SendMsg()`, sysex_msg_task_base.h, previously Tier B).
 *
 * GROUND TRUTH REACHABILITY: `CSysExMsgClientOutLink::CSysExMsgClientOutLink(CTask
 * const&)` (.text+0x080a5aa0) is called for real from
 * `CSysExMsgTaskBase::CSysExMsgTaskBase()`'s own ECanTransmit==1 branch
 * (sysex_msg_task_base.cpp, now modeled for real this pass -- was a documented
 * deviation, "mOutLink stays 0 regardless of canTransmit", now fixed). Confirmed via
 * direct `objdump -dr`/decompile reading, not guessed.
 *
 * REAL CLASS HIERARCHY (all 4 ctors read+transcribed from
 * Decomp/EVA_Decomp/eva_export/functions/{COutLink@0807cb20,COutLinkMono@0807d2e0,
 * CSysExMsgOutLink@080a69f0,CSysExMsgClientOutLink@080a5aa0}.c):
 *
 *   COutLink              (base, extends CNamedObjectBase, 0x34 bytes)
 *     COutLinkMono        (+0x04 -> 0x38 bytes total)
 *       CSysExMsgOutLink  (no new fields, vtable-swap only)
 *         CSysExMsgClientOutLink  (no new fields, vtable-swap only)
 *
 * REAL LAYOUT (COutLink, confirmed from COutLink@0807cb20.c):
 *   +0x00  vtbl        CNamedObjectBase's base vtable first, then COutLink's own
 *                       (PTR__COutLink_08e82068, omega_vtables.h) once the name copy
 *                       below succeeds -- same "vtable installed after the part that
 *                       can fail" idiom as CTask::CTask()/CModule::CModule().
 *   +0x04  mName        malloc'd copy of the name argument
 *   +0x08  mLinks       embedded COmegaPtrArray (0x18 bytes), vtable-swapped to
 *                       TPtrArray<CLink> (PTR__TPtrArray_08e820d8, omega_vtables.h) --
 *                       real element type confirmed to be `CLink*` by this vtable's
 *                       own real class name (nm -C); CLink itself is a genuinely
 *                       separate, un-reconstructed message-routing descriptor class,
 *                       out of scope (see OutMono()'s own header comment below for
 *                       exactly how far this reconstruction follows it).
 *   +0x20  mOwnerTask   the ctor's own CTask& argument, stored as a raw pointer
 *   +0x24  mMode        the ctor's own `unsigned short mode` argument, stored verbatim
 *   +0x28  mDirectionFlag  `unsigned int`, real ctor stores `(direction == 1)` --
 *                       i.e. the real EDirection argument is bool-ized on the way in,
 *                       only ever compared for equality to 1 anywhere in this class
 *                       family's own ctors
 *   +0x2c  mLastArg     the ctor's own trailing `int` argument, stored verbatim (real
 *                       meaning not decoded -- COutLinkMono always passes 1,
 *                       CSysExMsgOutLink's own CSysExOutLink sibling passes the same
 *                       constant through a different mode)
 *   +0x30  mScopeId     result of a virtual call through Api's own vtable slot +0x3c
 *                       at construction time -- the EXACT SAME call CModule::CModule()/
 *                       CTask::CTask() already make (module.cpp/task.cpp), same
 *                       undecoded meaning
 *
 * COutLinkMono adds:
 *   +0x34  mLink        `CLink*` -- NOT populated by any ctor in this family (real
 *                       ctor's own COutLinkMono::COutLinkMono() sets it to 0
 *                       unconditionally); ground truth's own real caller that
 *                       populates it (some `Connect()`-shaped method against a
 *                       `CSexServiceTask`-family object, not found in any traced call
 *                       path yet) is genuinely out of scope. OutMono() below
 *                       dereferences it, so any real exercise of OutMono() needs a
 *                       friend test hook to poke a fake `CLink`-shaped buffer here
 *                       first -- see OutLinkTestHooks below, same "friend pokes a raw
 *                       buffer, ctor doesn't populate it" convention this whole
 *                       project already uses (CClientCommServer's own mEvBuf, e.g.).
 *
 * `COutLinkMono::OutMono(unsigned short, void*, unsigned short)` (.text+0x0807d3c0,
 * 151 bytes, mangled `_ZN12COutLinkMono7OutMonoEtPvt`) is Tier A this pass -- real
 * body: if `mLinks` is empty (`mLinks.Count() == 0`, i.e. no downstream `CLink`
 * registered) returns 5 (a real, undecoded error code) immediately; otherwise packs
 * `(ecb, len, buf)` into 3 fields of the `CLink` object `mLink` points at (offsets
 * +0x18/+0x1a/+0x20 of THAT object, not this one -- ground truth's own real field
 * layout for `CLink` is not otherwise reconstructed, these 3 raw writes are the ONLY
 * ones any code in this project performs against it), then makes a genuinely
 * DATA-DRIVEN indirect call: `(*(void**)(mLink + 0x24))`'s own vtable slot 8, passing
 * `(that object, mLink + 0x10)` -- i.e. `mLink` also holds, at +0x24, a pointer to
 * some receiver object this project does not model either (plausibly a
 * `CMessageInput`-family interface, given the vtable-slot-8 dispatch shape, but not
 * confirmed). The result is stored back into `mLink+8` and forwarded through
 * `COutLink::TestResult()` (see below), then returned verbatim. Transcribed
 * byte-for-byte including the indirect call -- this reconstruction does not know or
 * need to know what `CLink`/its +0x24 receiver really are, only that a real caller
 * would have already wired them up before calling OutMono(); `OutLinkTestHooks`
 * below lets a KAT supply a fake but structurally-correct one.
 *
 * `COutLink::TestResult(EMessageResult, CLink*)` (.text+0x0807d1f0, 209 bytes) is
 * Tier A as a pure identity passthrough: ground truth's own real body only has a
 * side effect (allocate+report a `CMessageInputRetError` diagnostic object through
 * Api vtable slot +0x8c) when `result < 0` AND a specific `CLink` field match holds;
 * in EVERY case (both branches) it unconditionally `return`s the SAME `result` it was
 * given, so from any caller's observable perspective (OutMono() is the only one in
 * this project) `TestResult()` IS `return result;` -- the omitted diagnostic path is
 * the exact same "soft, log-only, no control-flow effect" shape this whole project
 * already treats every other Api+0x8c/0x90/0x94 call as (task.h/module.h/
 * client_comm_server.h all document the same convention).
 *
 * `CSysExMsgOutLink`/`CSysExMsgClientOutLink` ctors are Tier A too -- both are pure
 * base-then-vtable-swap forwarders with a hardcoded name/mode, nothing else to model.
 *
 * `CSysExMsgClientOutLink::SendMessage(unsigned char, unsigned char const*,
 * unsigned char)` (.text+0x080a5ad0, 142 bytes) is Tier A -- real body: a soft,
 * non-enforcing null-check on the data pointer (omitted, same convention as above),
 * then `COutLinkMono::OutMono(this, ecb, data, len)` (a direct, non-virtual call
 * against the `COutLinkMono` base subobject -- ground truth's own real call site
 * never goes through any vtable here, same non-virtual-call finding
 * client_comm_server.h's own `SendMessageToClient()` already established for this
 * exact method). This is `CSysExMsgTaskBase::SendMsg()`'s own real dependency,
 * unblocking it this same pass (sysex_msg_task_base.h/cpp).
 *
 * `COutLinkMono::OutMono(unsigned short, unsigned long)` (.text+0x0807d330, 132
 * bytes, mangled `_ZN12COutLinkMono7OutMonoEtm`) -- a SECOND real overload, added
 * for the CEditor::CPanelIfcTask dedicated pass (2026-07-25, panel_ifc_task.h/cpp,
 * its own only caller so far). Same empty-`mLinks`/error-5 gate and CLink+0x24
 * receiver dispatch as the pointer overload above, but embeds `value` directly
 * into CLink+0x20 (as a raw ulong, not a pointer) and sets the CLink+0x18 flags
 * word's mode bit to 0x100 instead of 0x200 (confirmed via direct `objdump -dr`
 * comparison of the two functions' own flag-computation instructions) -- the
 * real ground-truth distinction between "pointer-to-buffer" and "inline scalar"
 * IPC payload modes.
 */

#ifndef OUT_LINK_H
#define OUT_LINK_H

class CTask;

/* Genuinely separate, un-reconstructed message-routing descriptor class -- real name
 * confirmed via `nm -C Eva`'s own "vtable for TPtrArray<CLink>" symbol (see
 * omega_vtables.h). Never fully populated by any code in this project; opaque.
 */
class CLink;

class COutLink {
public:
	/* Real enum not individually named in the decompile (only ever compared for
	 * equality to 1 by any ctor in this family) -- opaque int, same convention as
	 * every other opaque enum in this project (task.h's ETaskLevel, e.g.).
	 */
	enum EDirection { eDirectionOut = 0, eDirectionIn = 1 };

	/* .text+0x0807cb20, 186 bytes. Tier A -- see header comment. */
	COutLink(const CTask &owner, const char *name, int direction, unsigned short mode,
	          int lastArg);

	/* .text+0x0807d1f0, 209 bytes. Tier A -- identity passthrough, see header
	 * comment (the omitted diagnostic path never changes the return value).
	 */
	int TestResult(int result, CLink *link) const;

	/* Trivial public accessor for mName (below) -- NOT a separate ground-truth
	 * function of its own. Added for `CEditTask::GetOutLinkName()`
	 * (edit_task.h, Eva Stage 6 CBatchDiskMan unlock batch, 2026-07-26), whose
	 * real ground-truth body is a raw `*(char**)(outlink+4)` read (matching
	 * this class's own +0x04 mName offset exactly) -- same "expose a raw field
	 * read as a named accessor instead of reaching around encapsulation"
	 * convention this project already uses elsewhere (e.g. module.h's public
	 * wrappers around otherwise-private state).
	 */
	const char *GetName() const { return mName; }

protected:
	void          *mVtbl;
	char          *mName;
	unsigned char  mLinks[0x18]; /* embedded COmegaPtrArray<CLink*>, raw-offset
	                               * accessed (same convention as task.h's
	                               * mOutLinks/mIfcArray) */
	const CTask   *mOwnerTask;
	unsigned short mMode;
	unsigned short mPad26; /* not confirmed real; keeps mDirectionFlag dword-aligned */
	unsigned int   mDirectionFlag;
	int            mLastArg;
	int            mScopeId;
};

class COutLinkMono : public COutLink {
public:
	/* .text+0x0807d2e0, 77 bytes. Tier A -- see header comment. */
	COutLinkMono(const CTask &owner, const char *name, int direction, unsigned short mode);

	/* .text+0x0807d3c0, 151 bytes. Tier A -- see header comment. Real ground-truth
	 * return type is `int` (an error code / TestResult() passthrough); NOT declared
	 * virtual (this project's raw-mVtbl convention, see sysex_msg_task_base.h) even
	 * though it IS a real ground-truth virtual override in some callers' eyes --
	 * every real call site traced so far (SendMessageToClient(),
	 * CSysExMsgClientOutLink::SendMessage()) is direct/non-virtual anyway.
	 */
	int OutMono(unsigned short ecb, void *buf, unsigned short len);

	/* .text+0x0807d330, 132 bytes. Tier A -- see header comment. Same real
	 * "not declared C++ virtual" convention as the pointer overload above.
	 */
	int OutMono(unsigned short ecb, unsigned long value);

protected:
	CLink *mLink; /* +0x34, always 0 from any ctor in this family -- see header
	               * comment / OutLinkTestHooks */

	/* Friend accessor for verify/test_out_link.cpp -- pokes mLink directly (and lets
	 * the KAT build a fake CLink-shaped buffer) so OutMono() can be exercised without
	 * a real (never-modeled) Connect()-type call having run. Same convention as
	 * client_comm_server.h's ClientCommServerTestHooks.
	 */
	friend struct OutLinkTestHooks;
};

class CSysExMsgOutLink : public COutLinkMono {
public:
	/* .text+0x080a69f0, 61 bytes. Tier A -- forwards to COutLinkMono with
	 * direction=0, mode=0x8007 hardcoded, then installs this class's own vtable.
	 */
	CSysExMsgOutLink(const CTask &owner, const char *name);
};

class CSysExMsgClientOutLink : public CSysExMsgOutLink {
public:
	/* .text+0x080a5aa0, 45 bytes. Tier A -- forwards to CSysExMsgOutLink with the
	 * hardcoded name "MSG_SysEx", then installs this class's own vtable.
	 */
	explicit CSysExMsgClientOutLink(const CTask &owner);

	/* .text+0x080a5ad0, 142 bytes. Tier A -- see header comment. */
	int SendMessage(unsigned char ecb, const unsigned char *data, unsigned char len);
};

/* COutLinkMulti -- Eva "size is not depth" re-check batch, 2026-07-26. Pulled in
 * while re-tracing `CBatchDiskMainTask::CBatchDiskMainTask()`'s own ctor
 * (batch_disk_main_task.h), which heap-allocates one (`malloc(0x34)`, matching
 * `COutLink`'s own base size exactly -- no new fields of its own).
 *
 * `COutLinkMulti::COutLinkMulti(CTask const&, char const*, COutLink::EDirection,
 * unsigned short)` (.text+0x0807d620, 70 bytes) is a PURE forwarder: `COutLink(
 * owner, name, direction, mode, lastArg=0)` (hardcoded 0, confirmed via direct
 * register-to-stack-slot tracing of the real disassembly) then installs this
 * class's own real vtable (`PTR__COutLinkMulti_08e82028`, confirmed via
 * `_ZTV13COutLinkMulti` at 0x08e82020). No other members, no other real methods
 * needed by `CBatchDiskMainTask`'s own ctor (which only constructs+`CTask::Add()`s
 * it, never calls `OutMulti()`).
 */
class COutLinkMulti : public COutLink {
public:
	COutLinkMulti(const CTask &owner, const char *name, int direction, unsigned short mode);
};

#endif /* OUT_LINK_H */
