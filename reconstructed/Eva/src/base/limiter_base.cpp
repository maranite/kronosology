/*
 * limiter_base.cpp  -  see include/limiter_base.h.
 *
 * CLimiterBase::CLimiterBase/~CLimiterBase transcribed from CLimiterBase@0807aa50.c/
 * ~CLimiterBase@0807a210.c/0807a270.c; CWrProtCircularQueue::CWrProtCircularQueue/
 * ~CWrProtCircularQueue/Init/IsEmpty/CountIntegers from CWrProtCircularQueue@
 * 0807a360.c/0807a1c0.c/0807a2f0.c/Init@0807a3a0.c/IsEmpty@0807a9c0.c/
 * CountIntegers@0807aa30.c.
 *
 * Real HAL_DisableInterrupts()/HAL_EnableInterrupts() brackets around every malloc/
 * free below are dropped -- same established "kernel-side critical-section shim,
 * no-op-and-dropped userspace concern" precedent as circ_byte_buffer.cpp/task.cpp/
 * module.cpp/out_link.cpp/ev_buffers_pool.cpp. Real Api-shaped soft-assert-log calls
 * (ds:0x930a1f4, vtbl+0x80/+0x90/+0x94) are likewise omitted -- log-only, no
 * control-flow effect, same convention as every other soft assert in this project.
 */

#include "limiter_base.h"
#include "omega_vtables.h"

#include <cstdlib>

CLimiterBase::CWrProtCircularQueue::CWrProtCircularQueue(int level)
	: mVtbl(0), mLevel(level), mBase(0), mLimit(0), mReadPtr(0), mWritePtr(0),
	  mLastHeader(0)
{
	mVtbl = (void *)PTR__CWrProtCircularQueue_08e81ca8;
}

CLimiterBase::CWrProtCircularQueue::~CWrProtCircularQueue()
{
	mVtbl = (void *)PTR__CWrProtCircularQueue_08e81ca8;
	if (mBase != 0)
		free(mBase);
}

bool CLimiterBase::CWrProtCircularQueue::Init(int sizeShift)
{
	/* Real: soft assert-log if sizeShift > 0x17 (23) -- ground truth still
	 * proceeds to try building the buffer below even after logging (not a
	 * gating condition), so omitting the log call changes nothing observable.
	 */
	if (mBase != 0)
		return false; /* already initialized -- real idempotency guard */

	unsigned capacity = (sizeShift <= 6) ? 0x80u : ((1u << sizeShift) & ~3u);

	unsigned char *buf = (unsigned char *)malloc(capacity);
	mBase = buf;
	if (buf == 0)
		return false;

	mWritePtr = buf;
	mReadPtr = buf;
	mLimit = buf + capacity;
	return true;
}

unsigned int CLimiterBase::CWrProtCircularQueue::CountIntegers(unsigned int len)
{
	/* Real: `((len & 3) != 0) + (len >> 2)` -- word count a payload of `len`
	 * bytes occupies once dword-padded.
	 */
	return (len >> 2) + ((len & 3u) != 0 ? 1u : 0u);
}

bool CLimiterBase::CWrProtCircularQueue::IsEmpty() const
{
	return mReadPtr == mWritePtr;
}

CLimiterBase::CLimiterBase(int sizeShift, int level, TMsgFn unmarshallFn, TMsgFn sendFn)
	: mVtbl(0), mIfcLink(0), mUnknown08(0), mQueue(level), mInitAttempted(0),
	  mUnmarshallFn(unmarshallFn), mSendFn(sendFn)
{
	mVtbl = (void *)PTR__CLimiterBase_08e81c90;
	/* mQueue's own ctor already installed its own vtable + stored `level` +
	 * zeroed its own fields above -- matches ground truth's own inline
	 * duplication of that exact sequence at this constructor's own real
	 * disassembly (rather than a call to CWrProtCircularQueue::Init(), which
	 * this ctor never invokes).
	 */

	/* Real: forces the "already failed" flag if either callback is null --
	 * ground truth still proceeds to try building the buffer regardless
	 * (transcribed literally, not "fixed").
	 */
	if (unmarshallFn == 0 || sendFn == 0)
		mInitAttempted = 4;

	/* Real: up to 2 further soft assert-log calls (sizeShift > 0x17; and again
	 * if the malloc below fails or mInitAttempted was already nonzero) --
	 * log-only, omitted per this file's header comment.
	 */
	unsigned capacity = (sizeShift <= 6) ? 0x80u : ((1u << sizeShift) & ~3u);

	unsigned char *buf = (unsigned char *)malloc(capacity);
	mQueue.mBase = buf;
	if (buf != 0) {
		mQueue.mWritePtr = buf;
		mQueue.mReadPtr = buf;
		mQueue.mLimit = buf + capacity;
	} else if (mInitAttempted == 0) {
		mInitAttempted = 1;
	}
}

CLimiterBase::~CLimiterBase()
{
	mVtbl = (void *)PTR__CLimiterBase_08e81c90;
	mQueue.mVtbl = (void *)PTR__CWrProtCircularQueue_08e81ca8;

	/* Real ground truth frees mQueue.mBase HERE, inline, as flat machine code
	 * (there is no separate sub-object dtor call in the real binary -- this
	 * whole function is one flat CLimiterBase::~CLimiterBase()). This C++
	 * reconstruction instead lets mQueue's OWN destructor (~CWrProtCircularQueue(),
	 * automatically invoked by the compiler right after this function body
	 * returns) do that exact same free() -- calling free(mQueue.mBase) again
	 * here would double-free it. Net observable effect (mQueue.mBase freed
	 * exactly once) is identical either way.
	 */

	/* Real: final identity override right before returning -- CLimiterBase's
	 * own further base is CMsgSender (matches AlphaKeybCode's own
	 * "CMsgSender-shaped thunk vtable" finding, alpha_keyb_ctrl_task.cpp),
	 * same "re-assert own identity right before the inherited-from-shape
	 * cleanup one" idiom CLimiterMan's own dtor already established
	 * (limiter_man.h). `vtable for CMsgSender`+8 = 0x8e81d98, confirmed via
	 * direct `.rodata`/`nm -C` cross-check; CMsgSender itself is not
	 * reconstructed anywhere in this project, so this is a raw address
	 * literal, not a named PTR__ array.
	 */
	mVtbl = (void *)0x08e81d98;
}

void CLimiterBase::Init(CTask & /*owner*/, unsigned int /*level*/)
{
	/* Tier B -- see header comment's "ILimiterNotify interface link" section.
	 * Real body's own `COutLinkIfcBaseC1(...)` base-construction call is
	 * genuinely blocked on COutLinkIfcBase itself, not reconstructed anywhere
	 * in this project.
	 */
}

bool CLimiterBase::Write(unsigned char /*type*/, void * /*data*/, unsigned int /*len*/)
{
	/* Tier B -- see header comment. Real body forwards into
	 * CWrProtCircularQueue::Write() (Tier B) and, on certain conditions, into
	 * mIfcLink's own CMarshaller<ILimiterNotify> dispatch (out of scope).
	 */
	return false;
}

void CLimiterBase::PopMessage()
{
	/* Tier B -- see header comment. Real body forwards into
	 * CWrProtCircularQueue::StaticRead()/SeekNextRead() (Tier B) plus a raw
	 * call through mUnmarshallFn.
	 */
}

bool CLimiterBase::SendWithAnswer(unsigned char type, void *data, unsigned int len)
{
	/* Real: literal tail-jump into Write() (ground truth widens the `bool`
	 * argument to `int` and falls straight through -- .text+0x0807af80, 14
	 * bytes, real virtual slot 3 of PTR__CLimiterBase_08e81c90). The forward
	 * itself is real even though its own target (Write(), above) stays Tier B.
	 */
	return Write(type, data, len);
}

void CLimiterBase::SendNoAnswer(unsigned char type, const void *data, unsigned int len)
{
	/* Real: literal tail-jump into Write() -- .text+0x0807afa0, 14 bytes, real
	 * virtual slot 2 of PTR__CLimiterBase_08e81c90. Same forward-is-real
	 * precedent as SendWithAnswer() above.
	 */
	Write(type, const_cast<void *>(data), len);
}
