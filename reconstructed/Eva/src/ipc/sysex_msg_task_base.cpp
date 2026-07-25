/*
 * sysex_msg_task_base.cpp  -  see include/sysex_msg_task_base.h.
 *
 * Transcribed from Decomp/EVA_Decomp/eva_export/functions/{CSysExMsgTaskBase@080a65e0,
 * SetTimeout@080a67c0, Exec@080a64f0, Exec@080a65a0, SendMsg@080a6730,
 * EventToMessage@080a68c0, MessageToEvent@080a6970, OnSexLinkError@08184ed0,
 * OnReceiveMessage@08184ec0, OnTimeout@08184ee0, _CSysExMsgTaskBase@08184ef0}.c.
 *
 * Tier split rationale is in the header comment. Short version: everything gated on
 * `CTask::SetMask()` (ctor's ECanTransmit==1 branch aside, ctor/SetTimeout/Exec()) or on
 * the CSysExMsgClientOutLink/CSysExApiInstance/CSexServiceTask output-link chain
 * (ctor's other branch, SendMsg/EventToMessage/MessageToEvent) or on `CTask::~CTask()`
 * (dtor) is Tier B this pass -- all three are real, currently-unavailable dependencies
 * owned by a concurrent agent's pass (CTask/CModule family) or a genuinely separate,
 * un-reconstructed link-abstraction subsystem, not guesses.
 */

#include "sysex_msg_task_base.h"

#include <cstddef>

/* Observed gap: the real ctor writes this class's own first new field at `this+0x80`,
 * 4 bytes past CTask's own documented 0x7c-byte size (task.h). Not explained (possibly
 * an alignment pad task.h's own field list doesn't capture, possibly one more CTask
 * field this pass hasn't traced) -- immaterial here since every Tier-A method in this
 * file is offset-independent (Exec(CMessage&) touches only its CMessage argument and
 * this object's own vtable slot; the trivial OnXxx overrides touch nothing). Left as a
 * flagged discrepancy rather than silently "fixed" by picking a byte count that makes
 * the arithmetic reconcile.
 */

CSysExMsgTaskBase::CSysExMsgTaskBase(const CModule &owner, int canTransmit, int needsTimeout)
	: CTask(owner, "SysExMsgClient", 2, needsTimeout == 1, 0x8007),
	  mTimeoutTicks(0), mCommId(0xff), mOutLink(0)
{
	/* Real ctor's ECanTransmit==1 branch additionally malloc's a
	 * CSysExMsgClientOutLink(this) and calls the not-yet-reconstructed
	 * `CTask::Add(COutLink*)` (a distinct overload from CModule::Add(CTask*),
	 * module.h) -- NOT modeled here, see header comment. mOutLink stays 0
	 * regardless of `canTransmit`, a documented deviation from the real ctor when
	 * canTransmit == eCanTransmit.
	 */
	(void)canTransmit;

	/* Real ctor also does `if ((this->mMask & 8) != 0) CTask::SetMask(this, 1);`
	 * right after the base CTask::CTask() call -- NOT modeled, see header comment
	 * (CTask::SetMask() unavailable this pass).
	 */
}

/* SetTimeout()/Exec() (zero-arg) -- Tier B, both need CTask::SetMask(). Empty bodies,
 * real signatures only.
 */
void CSysExMsgTaskBase::SetTimeout(unsigned short /*milliseconds*/) {}
void CSysExMsgTaskBase::Exec() {}

/* Tier A -- .text+0x080a64f0, 171 bytes. Pure argument-unpack + redispatch through this
 * object's own (derived-class-overridden) vtable slot +0x14 (index 5). CMessage's real
 * layout: +0x08 a "tag" byte, +0x0a a signed short length-plus-one, +0x10 a pointer to
 * [tag byte][payload...]. Real assert (Api+0x94, "puVar4 == NULL") is dead in practice
 * (only fires if the payload pointer is 0xFFFFFFFF) -- omitted rather than modeling
 * Api's assert-report call for a branch that can never really take it.
 */
int CSysExMsgTaskBase::Exec(CMessage &msg)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);
	unsigned char tag = raw[8];
	short taggedLen = *reinterpret_cast<const short *>(raw + 0xa);
	const unsigned char *const *payloadPtrSlot =
		reinterpret_cast<const unsigned char *const *>(raw + 0x10);
	unsigned char firstPayloadByte = **payloadPtrSlot;
	const unsigned char *payload = *payloadPtrSlot + 1;

	typedef int (*DerivedHandlerFn)(void *, unsigned char, unsigned char, const unsigned char *,
	                                 unsigned char);
	void **vt = *reinterpret_cast<void ***>(this);
	DerivedHandlerFn fn = reinterpret_cast<DerivedHandlerFn>(vt[0x14 / 4]);

	int result = fn(this, firstPayloadByte, tag, payload,
	                 static_cast<unsigned char>((taggedLen - 1) & 0xff));
	return -(int)(result == 0);
}

/* Tier B stubs -- real signatures only, see header comment. */
bool CSysExMsgTaskBase::SendMsg(const unsigned char * /*data*/, unsigned char /*len*/)
{
	return false;
}

void CSysExMsgTaskBase::EventToMessage(const void * /*linkedEvent*/, unsigned char * /*out*/,
                                        unsigned char & /*outLen*/)
{
}

void CSysExMsgTaskBase::MessageToEvent(const unsigned char * /*data*/, unsigned char /*len*/,
                                        void * /*linkedEvent*/)
{
}

/* Tier A -- real, genuinely empty `return;` bodies in the shipped binary (1/3/1 bytes),
 * confirmed by reading each decompile by hand.
 */
void CSysExMsgTaskBase::OnSexLinkError() {}

int CSysExMsgTaskBase::OnReceiveMessage(unsigned char /*a*/, unsigned char /*b*/,
                                         const unsigned char * /*c*/, unsigned char /*d*/)
{
	return 0;
}

void CSysExMsgTaskBase::OnTimeout() {}

/* Tier B -- real body calls CTask::~CTask() (task.h: "NOT reconstructed"). Empty. */
CSysExMsgTaskBase::~CSysExMsgTaskBase() {}
