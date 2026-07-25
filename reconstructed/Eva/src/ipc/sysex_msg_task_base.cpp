/*
 * sysex_msg_task_base.cpp  -  see include/sysex_msg_task_base.h.
 *
 * Transcribed from Decomp/EVA_Decomp/eva_export/functions/{CSysExMsgTaskBase@080a65e0,
 * SetTimeout@080a67c0, Exec@080a64f0, Exec@080a65a0, SendMsg@080a6730,
 * EventToMessage@080a68c0, MessageToEvent@080a6970, OnSexLinkError@08184ed0,
 * OnReceiveMessage@08184ec0, OnTimeout@08184ee0, _CSysExMsgTaskBase@08184ef0}.c.
 *
 * Tier split rationale is in the header comment. Short version: everything gated on
 * `CTask::SetMask()`/`CTask::~CTask()` (ctor, SetTimeout, Exec(), dtor) is NOW Tier A
 * (Stage 6 SetMask/~CTask batch, 2026-07-25 -- both now exist, task.h). Still Tier B:
 * the ctor's ECanTransmit==1 branch and SendMsg/EventToMessage/MessageToEvent, all
 * gated on the genuinely separate, un-reconstructed CSysExMsgClientOutLink/
 * CSysExApiInstance/CSexServiceTask output-link chain -- unrelated to SetMask/~CTask.
 */

#include "sysex_msg_task_base.h"
#include "omega_vtables.h"

#include <cstddef>

/* Real, currently-unreconstructed HAL dependencies -- file-local Tier-B stubs, same
 * established per-file convention as ckernel.cpp's own `HAL_GetSystemTime()` stub.
 * HAL_GetScheduleInterval()'s return value is used as a divisor in SetTimeout() below,
 * so it must stay non-zero -- 1 is a safe "no scaling" placeholder.
 */
namespace {
unsigned HAL_GetSystemTime() { return 0; }
unsigned HAL_GetScheduleInterval() { return 1; }
} // namespace

CSysExMsgTaskBase::CSysExMsgTaskBase(const CModule &owner, int canTransmit, int needsTimeout)
	: CTask(owner, "SysExMsgClient", 2, needsTimeout == 1, 0x8007),
	  mTimeoutTicks(0), mCommId(0xff), mOutLink(0)
{
	/* Real: reads CTask's own mMask (+0x4c) BEFORE overwriting the vtable fields
	 * below -- raw offset access (not a named-member read), matching this whole
	 * project's established "treat objects as raw blobs across class boundaries"
	 * idiom (module_manager.cpp/level_manager_array.h) rather than needing a
	 * friend declaration into CTask's private section.
	 */
	unsigned char mask = *(reinterpret_cast<unsigned char *>(this) + 0x4c);

	/* Real: install this class's own vtable pair (primary + the CTask+0x8-
	 * equivalent secondary), same "derived ctor overwrites [this+0]/[this+8]
	 * right after the base ctor returns" idiom CTask::CTask() itself uses for
	 * its own base CNamedObjectBase (task.cpp).
	 */
	*reinterpret_cast<void **>(this) = (void *)PTR__CSysExMsgTaskBase_08e84c28;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
		(void *)&EvaDataPlaceholder_08e84c50;

	if (mask & 0x08)
		SetMask(1);

	/* Real ctor's ECanTransmit==1 branch additionally malloc's a
	 * CSysExMsgClientOutLink(this) and calls the not-yet-reconstructed
	 * `CTask::Add(COutLink*)` (a distinct overload from CModule::Add(CTask*),
	 * module.h) -- NOT modeled here, see header comment. mOutLink stays 0
	 * regardless of `canTransmit`, a documented deviation from the real ctor when
	 * canTransmit == eCanTransmit.
	 */
	(void)canTransmit;
}

/* Tier A -- .text+0x080a67c0, 247 bytes. Real fixed-point period computation: divides
 * the requested millisecond timeout by the real per-tick schedule interval, then
 * rescales by GCC's classic multiply-shift replacement for a divide-by-10
 * (0xcccd/2^19 == 0.100000381...) -- transcribed literally rather than re-derived,
 * since the exact rounding behavior matters for byte-faithfulness and the intended
 * semantic (period is apparently counted in "10-tick units") isn't otherwise
 * documented anywhere in this project. 2 real Api diagnostic-only calls (vtbl slot
 * +0x94, undecoded) are NOT modeled -- same established precedent as
 * Exec(CMessage&)'s own header comment for this exact slot: they don't affect control
 * flow, only logging.
 */
void CSysExMsgTaskBase::SetTimeout(unsigned short milliseconds)
{
	unsigned char mask = *(reinterpret_cast<unsigned char *>(this) + 0x4c);
	unsigned short *periodField =
		reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(this) + 0x78);
	unsigned short *countdownField =
		reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(this) + 0x7a);

	if (!(mask & 0x08)) {
		/* Real: unconditional Api diagnostic call here (not modeled, see above).
		 * If a nonzero timeout was requested anyway, ground truth still honors
		 * it (falls into the same real-timeout path below); a zero timeout
		 * instead just re-enables the task (SetMask(1)) and returns.
		 */
		if (milliseconds == 0) {
			SetMask(1);
			return;
		}
	} else if (milliseconds == 0) {
		SetMask(1);
		return;
	}

	if (milliseconds <= 9) {
		/* Real: a SECOND Api diagnostic call here (not modeled, see above) --
		 * a warning, not a hard error; ground truth falls through to the same
		 * real-timeout path regardless.
		 */
	}

	mTimeoutTicks = milliseconds;

	unsigned scheduleInterval = HAL_GetScheduleInterval();
	unsigned short ticks = (unsigned short)(milliseconds / scheduleInterval);
	unsigned short period = (unsigned short)(((unsigned)ticks * 0xcccdu) >> 0x13);

	if (period != 0) {
		*periodField = period;
		*countdownField = period;
	} else {
		*periodField = 1;
		*countdownField = 1;
	}

	mTimeoutStart = HAL_GetSystemTime();

	SetMask(0);
}

/* Tier A -- .text+0x080a65a0, 57 bytes. Real: if the timeout hasn't elapsed yet
 * (HAL_GetSystemTime() - mTimeoutStart < mTimeoutTicks), do nothing; otherwise
 * re-mask (SetMask(1)) and redispatch through this object's own installed vtable at
 * slot+0x1c (index 7 -- one past CTask's own 7-slot primary vtable, i.e. one of this
 * class's own ADDED virtual slots, matching its own 13-slot total; almost certainly
 * OnTimeout(), but modeled as a raw vtable dispatch rather than a direct call to that
 * named method, same "how ground truth's own compiled indirection actually works"
 * reasoning as Exec(CMessage&)'s own raw slot+0x14 dispatch above -- a further-derived
 * class could legitimately override this slot instead).
 */
void CSysExMsgTaskBase::Exec()
{
	if (HAL_GetSystemTime() - mTimeoutStart < mTimeoutTicks)
		return;

	SetMask(1);

	typedef void (*TimeoutFn)(void *);
	void **vt = *reinterpret_cast<void ***>(this);
	TimeoutFn fn = reinterpret_cast<TimeoutFn>(vt[0x1c / 4]);
	fn(this);
}

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

/* Tier A -- .text+0x08184ef0 (+2 real non-virtual thunks, both `this`-adjust-by-8 then
 * tail-jump here, not separately modeled -- a plain non-virtual C++ dtor call already
 * reaches this same body regardless of which base subobject pointer the caller holds).
 * Real body: reinstalls this class's own vtable pair (same identity the ctor installs,
 * matching the "re-assert own identity right before the base dtor" idiom every other
 * destructor in this project follows -- task.cpp/limiter_man.cpp), then calls the base
 * `CTask::~CTask()` (now real, task.cpp).
 */
CSysExMsgTaskBase::~CSysExMsgTaskBase()
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CSysExMsgTaskBase_08e84c28;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
		(void *)&EvaDataPlaceholder_08e84c50;
}
