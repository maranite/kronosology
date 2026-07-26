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
 *
 * MsgShortBeep()/MsgRequestAnalogInputValue()/MsgUnregisterClient()/
 * MsgSetEncoderClient()/MsgSetTouchPanelClient()/MsgSetKeyboardClient()/
 * MsgRegisterClientByVal()/MsgRegisterClientByRef() (2026-07-26 broad nm-C sweep
 * batch) transcribed from .text+0x089f0150/0x089f0420/0x089f1990/0x089f2010/
 * 0x089f2090/0x089f2110/0x089f53f0/0x089f5470 respectively, all via direct
 * `objdump -dr -M intel` register tracing. See poller.h's own per-method header
 * comments for the full derivation of each.
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

/* --- Msg*() handlers, promoted Tier B -> Tier A, broad nm-C sweep 2026-07-26 -----
 *
 * `CMessage` stays a forward-declared incomplete type throughout (poller.h) -- these
 * follow the same "raw reinterpret_cast<unsigned char*>(&msg), fixed-offset field
 * reads" convention `CChunkServer::Exec()`/`CSysExMsgTaskBase::Exec()` already
 * established (chunk_server.cpp/sysex_msg_task_base.cpp), rather than needing a real
 * `CMessage` class definition. See poller.h's own per-method header comments for the
 * full derivation of each; only the non-obvious bits are re-noted here.
 */

int CPoller::MsgShortBeep(CMessage &msg)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	if (!(raw[9] & 0x1))
		return 4;

	if (mResource == 0)
		return 0;

	/* Real: a 2-dword {opcode, <value>} local passed by pointer through
	 * mResource's own vtbl slot +0x1c (index 7). Ground truth genuinely never
	 * initializes the 2nd dword for THIS handler -- left uninitialized here too,
	 * matching hid_driver.cpp's own established "reproduce the real undefined
	 * read, don't paper over it" precedent. Safe in practice: mResource's own
	 * real callee is out of scope/opaque, never dereferences this struct in this
	 * reconstruction.
	 */
	struct SResourceMsg {
		unsigned int opcode;
		unsigned int value;
	} local;
	local.opcode = 7;

	typedef void (*NotifyFn)(void *, SResourceMsg *);
	void *resVtbl = *reinterpret_cast<void **>(mResource);
	NotifyFn notify = *reinterpret_cast<NotifyFn *>(reinterpret_cast<char *>(resVtbl) + 0x1c);
	notify(mResource, &local);

	return 0;
}

int CPoller::MsgRequestAnalogInputValue(CMessage &msg) const
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	if (!(raw[9] & 0x1))
		return 4;

	if (mResource == 0)
		return 0;

	struct SResourceMsg {
		unsigned int opcode;
		unsigned int value;
	} local;
	local.opcode = 5;
	local.value = *reinterpret_cast<const unsigned int *>(raw + 0x10);

	typedef void (*NotifyFn)(void *, SResourceMsg *);
	void *resVtbl = *reinterpret_cast<void **>(mResource);
	NotifyFn notify = *reinterpret_cast<NotifyFn *>(reinterpret_cast<char *>(resVtbl) + 0x1c);
	notify(mResource, &local);

	return 0;
}

int CPoller::MsgUnregisterClient(CMessage &msg)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	if (!(raw[9] & 0x1))
		return 4;

	unsigned int handle = *reinterpret_cast<const unsigned int *>(raw + 0x10);
	if (handle == 0xffffffff)
		return 9;

	unsigned char **begin = *reinterpret_cast<unsigned char ***>(mClients + 4);
	unsigned char **end   = *reinterpret_cast<unsigned char ***>(mClients + 8);
	unsigned int count = (unsigned int)(end - begin);
	if (handle >= count)
		return 9;

	unsigned char *client = begin[handle];
	if (*reinterpret_cast<const int *>(client + 0x14) == 0)
		return 2;

	/* Real: the SAME Api+0x58 "per-outlink notification" slot CTask::~CTask()
	 * already documents/calls (system_api.h, task.cpp) -- CIfcClient IS-A
	 * COutLink via COutLinkMono, so this is a legitimate reuse of that identical
	 * slot from a different real call site, not a new one.
	 */
	typedef void (*NotifyOutLinkFn)(void *, void *);
	void *apiVtbl = *reinterpret_cast<void **>(Api);
	NotifyOutLinkFn notifyOutLink =
	    *reinterpret_cast<NotifyOutLinkFn *>(reinterpret_cast<char *>(apiVtbl) + 0x58);
	notifyOutLink(Api, client);

	return 0;
}

int CPoller::MsgSetEncoderClient(CMessage &msg)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	if (!(raw[9] & 0x1))
		return 4;

	mField394 = 0xffffffff;

	unsigned int handle = *reinterpret_cast<const unsigned int *>(raw + 0x10);
	if (handle == 0xffffffff) {
		mField394 = handle;
		return 0;
	}

	unsigned char **begin = *reinterpret_cast<unsigned char ***>(mClients + 4);
	unsigned char **end   = *reinterpret_cast<unsigned char ***>(mClients + 8);
	unsigned int count = (unsigned int)(end - begin);
	if (handle >= count)
		return 9;

	unsigned char *client = begin[handle];
	if (*reinterpret_cast<const int *>(client + 0x14) == 0)
		return 9;

	mField394 = handle;
	return 0;
}

int CPoller::MsgSetTouchPanelClient(CMessage &msg)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	if (!(raw[9] & 0x1))
		return 4;

	mField398 = 0xffffffff;

	unsigned int handle = *reinterpret_cast<const unsigned int *>(raw + 0x10);
	if (handle == 0xffffffff) {
		mField398 = handle;
		return 0;
	}

	unsigned char **begin = *reinterpret_cast<unsigned char ***>(mClients + 4);
	unsigned char **end   = *reinterpret_cast<unsigned char ***>(mClients + 8);
	unsigned int count = (unsigned int)(end - begin);
	if (handle >= count)
		return 9;

	unsigned char *client = begin[handle];
	if (*reinterpret_cast<const int *>(client + 0x14) == 0)
		return 9;

	mField398 = handle;
	return 0;
}

int CPoller::MsgSetKeyboardClient(CMessage &msg)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	if (!(raw[9] & 0x1))
		return 4;

	mField39c = 0xffffffff;

	unsigned int handle = *reinterpret_cast<const unsigned int *>(raw + 0x10);
	if (handle == 0xffffffff) {
		mField39c = handle;
		return 0;
	}

	unsigned char **begin = *reinterpret_cast<unsigned char ***>(mClients + 4);
	unsigned char **end   = *reinterpret_cast<unsigned char ***>(mClients + 8);
	unsigned int count = (unsigned int)(end - begin);
	if (handle >= count)
		return 9;

	unsigned char *client = begin[handle];
	if (*reinterpret_cast<const int *>(client + 0x14) == 0)
		return 9;

	mField39c = handle;
	return 0;
}

/* Tier-B link-stub -- see poller.h's own header comment. Real 2603-byte body
 * genuinely out of scope this batch; only its one guaranteed unconditional real
 * side effect (outHandle = 0xFFFFFFFF at entry, confirmed via objdump -dr) is
 * reproduced, so MsgRegisterClientByVal()/ByRef()'s own write-back behavior stays
 * observable in a KAT even with this stubbed.
 */
int CPoller::RegisterClient(unsigned int &outHandle, const char *, const char *)
{
	outHandle = 0xffffffff;
	return 0;
}

int CPoller::MsgRegisterClientByVal(CMessage &msg)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	if (!(raw[9] & 0x2))
		return 4;

	unsigned short taggedLen = *reinterpret_cast<const unsigned short *>(raw + 0xa);
	if (taggedLen <= 0x63)
		return 5;

	unsigned char *payload = *reinterpret_cast<unsigned char *const *>(raw + 0x10);
	if (payload == 0)
		return 6;
	if (payload[0x4] == 0)
		return 6;
	if (payload[0x34] == 0)
		return 6;

	unsigned int handle = 0xffffffff;
	int result = RegisterClient(handle, reinterpret_cast<const char *>(payload + 4),
	                             reinterpret_cast<const char *>(payload + 0x34));
	*reinterpret_cast<unsigned int *>(payload) = handle;
	return result;
}

int CPoller::MsgRegisterClientByRef(CMessage &msg)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	if (!(raw[9] & 0x2))
		return 4;

	unsigned short taggedLen = *reinterpret_cast<const unsigned short *>(raw + 0xa);
	if (taggedLen <= 0xb)
		return 5;

	unsigned int *payload = *reinterpret_cast<unsigned int *const *>(raw + 0x10);
	if (payload == 0)
		return 6;

	const char *nameA = reinterpret_cast<const char *>(payload[1]);
	if (nameA == 0 || *nameA == 0)
		return 6;

	const char *nameB = reinterpret_cast<const char *>(payload[2]);
	if (nameB == 0 || *nameB == 0)
		return 6;

	unsigned int handle = 0xffffffff;
	int result = RegisterClient(handle, nameA, nameB);
	payload[0] = handle;
	return result;
}

/* Tier-B link-stubs -- see poller.h's own header comment (2026-07-26 CPanel unlock
 * batch UPDATE). Real bodies (2925B/2919B) genuinely out of scope, same CMessage-
 * prerequisite reasoning as every other deferred CPoller method.
 */
void CPoller::InitButtons()
{
}

void CPoller::InitAnalogs()
{
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
