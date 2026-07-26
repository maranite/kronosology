/*
 * poller.cpp  -  see include/poller.h.
 *
 * CPoller::CPoller(...) transcribed directly from `objdump -dr -M intel` of
 * .text+0x089ef740 (1933 bytes) -- Ghidra's own decompile of this one is usable but
 * the SSE `movdqa`-based bulk-fill loops read more clearly straight from disasm.
 * CPoller::~CPoller() transcribed from .text+0x089ef490 (D1, 107 bytes).
 * CPoller::FindUnconnected()/IsValidHandle()/IsRegisteredHandle() transcribed from
 * .text+0x089f3000/0x089f3150/0x089f3180 (336/36/50 bytes).
 * CPoller::CIfcClient::CIfcClient()/PutAnalogEvt()/FlushAnalogEvts() transcribed from
 * .text+0x089ef620/0x089ef670/0x089ef6f0 (80/128/71 bytes).
 * TVector_CIfcClientPtr_MakeCapacity() transcribed from .text+0x089f7280 (506 bytes,
 * _ZN7TVectorIPN7CPoller10CIfcClientELi1EE12MakeCapacityEj) -- min capacity 32
 * elements (NOT 10, unlike task.cpp's own TVector_SRegisteredIfc_MakeCapacity() --
 * confirmed by direct disassembly, not assumed from that precedent), then doubling.
 *
 * Exception-unwind paths are omitted (happy path only), same license as every other
 * ctor/dtor in this project.
 */

#include "poller.h"
#include "module.h"
#include "omega_vtables.h"
#include "system_api.h"
#include "stg_unsol_msg_handler.h" /* CPanelOut::SAnalogEvt, already real -- see
                                     * header comment. */

#include <cstdlib>
#include <cstring>
#include <new>

/* Real module-scope global (mains.cpp). Same CallVSlot idiom used throughout this
 * project for classes whose real vtable layout isn't reconstructed (module.cpp,
 * task.cpp).
 */
extern CSystemApi *Api;

CPoller::CPoller(const CModule &owner, const char *name)
    : CTask(owner, "Poller", 2, 1, 0x804b)
{
	/* Real: install CPoller's own primary + secondary vtable identities right
	 * after the base CTask ctor returns -- see header comment for the byte-exact
	 * derivation (5 real primary slots, 3 real secondary slots, both install-
	 * only per this project's established convention). CTask's own mVtbl field
	 * is private (task.h), so this crosses the class boundary via raw `this`
	 * writes, same idiom CSysExMsgTaskBase's own ctor already uses
	 * (sysex_msg_task_base.cpp) for the identical situation.
	 */
	*reinterpret_cast<void **>(this) = (void *)PTR__CPoller_08f7c368;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
	    (void *)PTR__CPoller_08f7c384;

	mResource = 0;

	*(void **)mClients = (void *)PTR__TVector_08f7c3b0;
	*(void **)(mClients + 4) = 0;
	*(void **)(mClients + 8) = 0;
	*(void **)(mClients + 0xc) = 0;

	for (unsigned int i = 0; i < 0x40; i++)
		mHandleTable1[i] = 0xffffffff;
	for (unsigned int i = 0; i < 0x80; i++)
		mHandleTable2[i] = 0xffffffff;

	mFlag390 = 0;
	mField394 = 0xffffffff;
	mField398 = 0xffffffff;
	mField39c = 0xffffffff;

	bool masked = true;
	if (name != 0) {
		typedef void *(*FnLookup)(void *, const char *);
		void *apiVtbl = *(void **)Api;
		FnLookup lookup = *(FnLookup *)((char *)apiVtbl + 0xac);
		void *resource = lookup(Api, name);
		mResource = resource;

		if (resource != 0) {
			typedef int (*FnGetType)(void *);
			void *resVtbl = *(void **)resource;
			FnGetType getType = *(FnGetType *)((char *)resVtbl + 0x10);
			if (getType(resource) == 10) {
				typedef int (*FnConnect)(void *, int);
				FnConnect connect = *(FnConnect *)((char *)resVtbl + 8);
				if (connect(resource, 0) == 0)
					masked = false;
			}
		}
	}

	if (masked) {
		SetMask(1);
		mResource = 0;
	}

	memset(mZeroBlock, 0, sizeof(mZeroBlock));
}

CPoller::~CPoller()
{
	/* Real: reinstall the same vtable identities the ctor installed (standard
	 * destructor idiom, matches CTask::~CTask()'s own reinstall-then-tear-down
	 * shape, task.cpp). Same raw `this`-write crossing as the ctor above.
	 */
	*reinterpret_cast<void **>(this) = (void *)PTR__CPoller_08f7c368;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
	    (void *)PTR__CPoller_08f7c384;

	if (mResource != 0) {
		typedef int (*FnDisconnect)(void *, int);
		void *resVtbl = *(void **)mResource;
		FnDisconnect disconnect = *(FnDisconnect *)((char *)resVtbl + 0xc);
		disconnect(mResource, 0);
	}

	*(void **)mClients = (void *)PTR__TVector_08f7c3b0;
	void *clientsBegin = *(void **)(mClients + 4);
	free(clientsBegin);

	/* Real: falls through into the base CTask::~CTask() (already reconstructed,
	 * task.cpp) -- modeled here as ordinary C++ base-destruction, not a
	 * transcribed tail jump, same convention CSysExMsgTaskBase::~CSysExMsgTaskBase()
	 * already uses (sysex_msg_task_base.cpp).
	 */
}

int CPoller::FindUnconnected() const
{
	unsigned char **begin = *(unsigned char ***)(mClients + 4);
	unsigned char **end   = *(unsigned char ***)(mClients + 8);

	for (unsigned char **p = begin; p != end; p++) {
		const unsigned char *client = *p;
		/* Real: raw read of the client's own +0x14 byte -- see header comment
		 * for why this is opaque, not a friend-decl field access.
		 */
		if (*(const int *)(client + 0x14) == 0)
			return (int)(p - begin);
	}
	return -1;
}

bool CPoller::IsValidHandle(unsigned int handle) const
{
	if (handle == 0xffffffff)
		return false;
	unsigned char **begin = *(unsigned char ***)(mClients + 4);
	unsigned char **end   = *(unsigned char ***)(mClients + 8);
	unsigned int count = (unsigned int)(end - begin);
	return handle < count;
}

bool CPoller::IsRegisteredHandle(unsigned int handle) const
{
	if (!IsValidHandle(handle))
		return false;
	unsigned char **begin = *(unsigned char ***)(mClients + 4);
	const unsigned char *client = begin[handle];
	return *(const int *)(client + 0x14) != 0;
}

CPoller::CIfcClient::CIfcClient(const CTask &owner, const char *name, int lastArg)
    : COutLinkMono(owner, name, eDirectionOut, 0x804b)
{
	mVtbl = (void *)PTR__CIfcClient_08f7c3c8;
	mExtra38 = lastArg;
	mCursor = mRingBuf;
}

void CPoller::CIfcClient::PutAnalogEvt(const CPanelOut::SAnalogEvt &evt)
{
	unsigned char *ringEnd = mRingBuf + sizeof(mRingBuf);

	if ((unsigned char *)mCursor >= ringEnd) {
		unsigned char *ringStart = mRingBuf;
		if (mCursor != ringStart) {
			unsigned short len = (unsigned short)((unsigned char *)mCursor - ringStart);
			OutMono(3, ringStart, len);
			mCursor = ringStart;
		}
	}

	unsigned char *slot = (unsigned char *)mCursor;
	*(int *)slot = *(const int *)&evt;
	*(short *)(slot + 4) = *(const short *)((const char *)&evt + 4);
	mCursor = slot + 8;
}

void CPoller::CIfcClient::FlushAnalogEvts()
{
	unsigned char *ringStart = mRingBuf;
	if (mCursor != ringStart) {
		unsigned short len = (unsigned short)((unsigned char *)mCursor - ringStart);
		OutMono(3, ringStart, len);
		mCursor = ringStart;
	}
}

void TVector_CIfcClientPtr_MakeCapacity(unsigned char *vec, unsigned int n)
{
	const unsigned int kElemSize = 4;

	unsigned char *begin = *(unsigned char **)(vec + 4);
	unsigned char *cap   = *(unsigned char **)(vec + 0xc);
	unsigned int curCapElems = (unsigned int)(cap - begin) / kElemSize;
	if (n <= curCapElems)
		return;

	/* Real: min capacity 32 elements (confirmed via direct disassembly of
	 * .text+0x089f7280, NOT assumed from TVector_SRegisteredIfc_MakeCapacity()'s
	 * own min-10 policy -- see header comment), then doubling.
	 */
	unsigned int newCapElems = 32;
	if (n > 32) {
		do {
			newCapElems *= 2;
		} while (n > newCapElems);
	}
	unsigned int newBytes = newCapElems * kElemSize;

	/* Real: HAL_DisableInterrupts()/HAL_EnableInterrupts() bracket both malloc
	 * and free below -- not modeled, same established convention as task.cpp's
	 * own TVector_SRegisteredIfc_MakeCapacity().
	 */
	void *newBlock = malloc(newBytes);

	unsigned char *oldBegin = *(unsigned char **)(vec + 4);
	unsigned char *oldEnd   = *(unsigned char **)(vec + 8);
	unsigned int copyBytes = 0;
	if (oldEnd != oldBegin) {
		copyBytes = (unsigned int)(oldEnd - oldBegin);
		memcpy(newBlock, oldBegin, copyBytes);
	}

	*(unsigned char **)(vec + 8) = (unsigned char *)newBlock + copyBytes;
	free(oldBegin);
	*(unsigned char **)(vec + 4) = (unsigned char *)newBlock;
	*(unsigned char **)(vec + 0xc) = (unsigned char *)newBlock + newBytes;
}
