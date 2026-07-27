/*
 * kg_msg_processor.cpp  -  see include/kg_msg_processor.h.
 *
 * Real HAL_DisableInterrupts()/HAL_EnableInterrupts() brackets around every
 * malloc below are dropped -- same established "kernel-side critical-section
 * shim, no-op-and-dropped userspace concern" precedent as limiter_base.cpp/
 * circ_byte_buffer.cpp/task.cpp/module.cpp/out_link.cpp.
 */

#include "kg_msg_processor.h"
#include "omega_vtables.h"
#include "system_api.h"

#include <cstdlib>
#include <cstring>
#include <new>

extern CSystemApi *Api; /* mains.cpp */

namespace {

/* Real Api+0x9c vtable call -- same slot/shape already documented and reused
 * verbatim from timer_engine.cpp's own ApiGetDefault9c() (CExternalClock/
 * CInternalClock ctors make the identical dispatch).
 */
inline int ApiGetDefault9c()
{
	typedef int (*Fn)(void *);
	Fn fn = (Fn)(((void **)*(void **)Api)[0x9c / 4]);
	return fn(Api);
}

/* Real per-member cleanup shape shared by all 7 owned handler sub-objects in
 * ~CKGMsgProcessor(): if non-null, dispatch through THAT member's own vtable
 * slot index 1 (offset+4 -- the Itanium *deleting* destructor, which frees the
 * object itself) -- same idiom already established by CWheelsContainer's own
 * dtor (timer_engine.cpp), generalized here to a reusable helper since
 * CKGMsgProcessor repeats it 7 times.
 */
inline void DeleteViaVtableSlot1(void *obj)
{
	if (obj == 0)
		return;
	typedef void (*Fn)(void *);
	Fn fn = (Fn)(((void **)*(void **)obj)[1]);
	fn(obj);
}

} /* namespace */

CKGMsgProcessor *CKGMsgProcessor::ms_poInstance = 0;

CKGMsgProcessor::CKGMsgProcessor()
	: mCommonHandler(0), mModuleHandler(0), mUIControlHandler(0),
	  mSPRUIControlHandler(0), mSPRUICommonParamHandler(0),
	  mSPRUIAudioTrackParamHandler(0), mSPRUIDrumTrackParamHandler(0),
	  mUnknown1c(0), mBuffer50(0), mBuffer10(0), mFlag28(0),
	  mUnknown2c(0), mApiDefault(0)
	  /* mFlag29 deliberately NOT in the init list -- real ctor never
	   * initializes it either, see header comment. */
{
	/* 7 handler sub-objects: malloc at each real class's own byte size, poke
	 * only the vtable pointer (no other field write, no sub-ctor call) --
	 * see header comment.
	 */
	mCommonHandler = malloc(0x18);
	*(void **)mCommonHandler = (void *)PTR__CKGCommonMsgHandler_08f752e8;

	mModuleHandler = malloc(0x1c);
	*(void **)mModuleHandler = (void *)PTR__CKGModuleMsgHandler_08f75288;

	mUIControlHandler = malloc(0x18);
	*(void **)mUIControlHandler = (void *)PTR__CKGUIControlMsgHandler_08f751a8;

	mSPRUIControlHandler = malloc(0x18);
	*(void **)mSPRUIControlHandler = (void *)PTR__CSPRUIControlMsgHandler_08f75508;

	mSPRUICommonParamHandler = malloc(0x18);
	*(void **)mSPRUICommonParamHandler = (void *)PTR__CSPRUICommonParamMsgHandler_08f754c8;

	mSPRUIAudioTrackParamHandler = malloc(0x18);
	*(void **)mSPRUIAudioTrackParamHandler = (void *)PTR__CSPRUIAudioTrackParamMsgHandler_08f75488;

	mSPRUIDrumTrackParamHandler = malloc(0x18);
	*(void **)mSPRUIDrumTrackParamHandler = (void *)PTR__CSPRUIDrumTrackTrackParamMsgHandler_08f75448;

	/* 2 plain (non-polymorphic) data buffers, fully zeroed -- real meaning not
	 * decoded, see header comment.
	 */
	mBuffer50 = (unsigned char *)malloc(0x50);
	std::memset(mBuffer50, 0, 0x50);

	mBuffer10 = (unsigned char *)malloc(0x10);
	std::memset(mBuffer10, 0, 0x10);

	mFlag28 = 0;
	mUnknown1c = 0xe;
	mUnknown2c = 1;
	mApiDefault = ApiGetDefault9c();
}

CKGMsgProcessor::~CKGMsgProcessor()
{
	/* Real: dispatches through each of the 7 handler members' own deleting
	 * destructor (vtable slot index 1) in offset order -- see header comment.
	 * mBuffer50/mBuffer10 (+0x20/+0x24) are NOT freed here, matching the real
	 * disassembly's own member-walk (faithfully transcribed leak, not fixed).
	 */
	DeleteViaVtableSlot1(mCommonHandler);
	DeleteViaVtableSlot1(mModuleHandler);
	DeleteViaVtableSlot1(mUIControlHandler);
	DeleteViaVtableSlot1(mSPRUIControlHandler);
	DeleteViaVtableSlot1(mSPRUICommonParamHandler);
	DeleteViaVtableSlot1(mSPRUIAudioTrackParamHandler);
	DeleteViaVtableSlot1(mSPRUIDrumTrackParamHandler);
}

CKGMsgProcessor *CKGMsgProcessor::GetInstance()
{
	/* Real: malloc(0x34) then an explicit CKGMsgProcessorC1Ev() call as a
	 * separate step (not a fused `new` expression) -- modeled here with
	 * `::operator new` + placement new, same convention already established
	 * by CLocaleManager::GetInstance() (locale_manager.cpp).
	 */
	if (ms_poInstance == 0) {
		void *raw = ::operator new(0x34);
		ms_poInstance = new (raw) CKGMsgProcessor();
	}
	return ms_poInstance;
}
