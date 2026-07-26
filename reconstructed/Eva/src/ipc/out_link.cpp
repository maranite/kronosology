/*
 * out_link.cpp  -  see include/out_link.h.
 *
 * All 4 ctors transcribed from Decomp/EVA_Decomp/eva_export/functions/
 * {COutLink@0807cb20,COutLinkMono@0807d2e0,CSysExMsgOutLink@080a69f0,
 * CSysExMsgClientOutLink@080a5aa0}.c. OutMono()/TestResult()/SendMessage() from
 * {OutMono@0807d3c0,TestResult@0807d1f0,SendMessage@080a5ad0}.c. See header comment
 * for the full accounting of what's modeled vs. omitted.
 *
 * Real HAL_DisableInterrupts()/HAL_EnableInterrupts() brackets around the ctor's own
 * malloc/strcpy are dropped -- same established "kernel-side critical-section shim,
 * no-op-and-dropped userspace concern" precedent as task.cpp/module.cpp/
 * ev_buffers_pool.cpp.
 */

#include "out_link.h"
#include "omega_ptr_array.h"
#include "omega_vtables.h"
#include "system_api.h"

#include <cstdlib>
#include <cstring>
#include <new>

/* Real module-scope global (mains.cpp). Same CallVSlot-at-+0x3c idiom
 * CModule::CModule()/CTask::CTask() already use.
 */
extern CSystemApi *Api;

COutLink::COutLink(const CTask &owner, const char *name, int direction, unsigned short mode,
                     int lastArg)
	: mVtbl(0), mName(0), mOwnerTask(&owner), mMode(mode), mPad26(0),
	  mDirectionFlag(direction == eDirectionIn ? 1u : 0u), mLastArg(lastArg), mScopeId(0)
{
	mVtbl = (void *)PTR__CNamedObjectBase_08e81378;

	size_t len = strlen(name);
	char *dup = (char *)malloc(len + 1);
	mName = dup;
	strcpy(dup, name);

	mVtbl = (void *)PTR__COutLink_08e82068;

	new (mLinks) COmegaPtrArray();
	*reinterpret_cast<void **>(mLinks) = (void *)PTR__TPtrArray_08e820d8;

	typedef int (*Fn)(void *);
	void *apiVtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)apiVtbl + 0x3c);
	mScopeId = fn(Api);
}

int COutLink::TestResult(int result, CLink *) const
{
	/* Real body's only side effect (allocate+report a CMessageInputRetError via
	 * Api+0x8c, gated on result<0 and an undecoded CLink field match) never
	 * changes the return value -- see header comment. Omitted.
	 */
	return result;
}

COutLinkMono::COutLinkMono(const CTask &owner, const char *name, int direction,
                             unsigned short mode)
	: COutLink(owner, name, direction, mode, 1), mLink(0)
{
	mVtbl = (void *)PTR__COutLinkMono_08e82048;
}

int COutLinkMono::OutMono(unsigned short ecb, void *buf, unsigned short len)
{
	if (*reinterpret_cast<int *>(mLinks + 0xc) == 0)
		return 5;

	unsigned char *link = reinterpret_cast<unsigned char *>(mLink);

	*reinterpret_cast<unsigned short *>(link + 0x1a) = len;
	*reinterpret_cast<void **>(link + 0x20) = buf;
	unsigned short *flags = reinterpret_cast<unsigned short *>(link + 0x18);
	*flags = static_cast<unsigned short>((*flags & 0xf000) | 0x200 | (ecb & 0xff));

	/* Real: a genuinely data-driven indirect call through the CLink descriptor's
	 * own +0x24 receiver pointer's own vtable slot 8 -- see header comment. This
	 * class does not know or model what the receiver really is.
	 */
	void *receiver = *reinterpret_cast<void **>(link + 0x24);
	void *receiverVtbl = *reinterpret_cast<void **>(receiver);
	typedef int (*RecvFn)(void *, void *);
	RecvFn recv = *reinterpret_cast<RecvFn *>(reinterpret_cast<char *>(receiverVtbl) + 8);
	int result = recv(receiver, link + 0x10);
	*reinterpret_cast<int *>(link + 8) = result;

	return TestResult(result, mLink);
}

int COutLinkMono::OutMono(unsigned short ecb, unsigned long value)
{
	if (*reinterpret_cast<int *>(mLinks + 0xc) == 0)
		return 5;

	unsigned char *link = reinterpret_cast<unsigned char *>(mLink);

	*reinterpret_cast<unsigned long *>(link + 0x20) = value;
	unsigned short *flags = reinterpret_cast<unsigned short *>(link + 0x18);
	*flags = static_cast<unsigned short>((*flags & 0xf000) | 0x100 | (ecb & 0xff));

	/* Real: same genuinely data-driven indirect call as the pointer overload
	 * above -- see that overload's own header-comment writeup.
	 */
	void *receiver = *reinterpret_cast<void **>(link + 0x24);
	void *receiverVtbl = *reinterpret_cast<void **>(receiver);
	typedef int (*RecvFn)(void *, void *);
	RecvFn recv = *reinterpret_cast<RecvFn *>(reinterpret_cast<char *>(receiverVtbl) + 8);
	int result = recv(receiver, link + 0x10);
	*reinterpret_cast<int *>(link + 8) = result;

	return TestResult(result, mLink);
}

CSysExMsgOutLink::CSysExMsgOutLink(const CTask &owner, const char *name)
	: COutLinkMono(owner, name, 0, 0x8007)
{
	mVtbl = (void *)PTR__CSysExMsgOutLink_08e84b28;
}

CSysExMsgClientOutLink::CSysExMsgClientOutLink(const CTask &owner)
	: CSysExMsgOutLink(owner, "MSG_SysEx")
{
	mVtbl = (void *)PTR__CSysExMsgClientOutLink_08e84b08;
}

int CSysExMsgClientOutLink::SendMessage(unsigned char ecb, const unsigned char *data,
                                          unsigned char len)
{
	/* Real soft, non-enforcing null-check on `data` (Api+0x94, omitted -- same
	 * convention as every other soft assert in this project) omitted here.
	 */
	return OutMono(ecb, const_cast<unsigned char *>(data), len);
}

COutLinkMulti::COutLinkMulti(const CTask &owner, const char *name, int direction,
                               unsigned short mode)
	: COutLink(owner, name, direction, mode, /*lastArg=*/0)
{
	mVtbl = (void *)PTR__COutLinkMulti_08e82028;
}
