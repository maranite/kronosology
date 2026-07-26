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
 *
 * `CPoller::RegisterClient()` (2026-07-26 RegisterClient batch) transcribed from
 * .text+0x089f31c0 (2603 bytes) via direct `objdump -dr -M intel` register tracing
 * of the whole function -- Ghidra's own decompile was a usable first-pass map but
 * needed disassembly cross-checking for 3 sub-pieces (the name-match pointer chain,
 * the vector-insert self-aliasing-range guards, and the CZ name-string dance). See
 * poller.h's own top-of-file and per-method header comments for the full writeup,
 * including the two deliberate simplifications this method uses.
 *
 * `MsgSetLed()`/`MsgSetLed16bits()`/`MsgBackupLEDs()` (2026-07-26 CLEDBlinker/
 * final-prerequisites follow-up batch) transcribed from .text+0x089eff00/0x089f0070/
 * 0x089f01a0 (350/214/620 bytes), from Ghidra's own decompile directly (small enough
 * not to need disassembly cross-checking). See poller.h's own per-method header
 * comments and led_blinker.h for the full derivation, including the real
 * `mLedBackup` (+0x3e0, formerly `mReserved3e0`) finding this batch made.
 *
 * `FindRegisteredClient()`/`MsgGetClientHandleByRef()`/`MsgGetClientHandleByVal()`
 * (2026-07-26, same session, direct follow-up) transcribed from .text+0x089f25e0/
 * 0x089f0470/0x089f0f00 (2512/2601/2590 bytes) -- FindRegisteredClient() via direct
 * `objdump -dr -M intel` register tracing (same technique/opaque pointer chain as
 * RegisterClient()'s own Phase-1 scan); both Msg*() wrappers from Ghidra's own
 * decompile, modeled as real calls to FindRegisteredClient() instead of ground
 * truth's own literal inlined duplicate scan (see poller.h's own header comment).
 *
 * `MsgSetAnalogClient()`/`MsgSetButtonClient()` (2026-07-26 CPoller closeout batch)
 * transcribed from .text+0x089f2190/0x089f1a10 (1085/1505 bytes) via direct
 * `objdump -dr -M intel` register tracing (Ghidra's own decompile of both timed out
 * along with the rest of this session's Ghidra attempts; disasm alone was
 * sufficient). This batch also directly confirmed, via `objdump -s -j .rodata`
 * byte dumps of each function's own real lookup table, which of `mHandleTable1`/
 * `mHandleTable2` is the ANALOG vs. BUTTON client-handle table (poller.h's own
 * field comments previously guessed both the wrong way around) -- see the two new
 * static tables (`s_analogCode[]`, `s_buttonPrimaryCode[]`/`s_buttonAltCode[]`)
 * immediately below, copied verbatim from ground truth's own `.rodata`.
 */

#include "poller.h"
#include "led_blinker.h"
#include "module.h"
#include "omega_vtables.h"
#include "system_api.h"
#include "stg_unsol_msg_handler.h" /* CPanelOut::SAnalogEvt, already real -- see
                                     * header comment. */

#include <cstdio>
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

/* --- MsgSetLed()/MsgSetLed16bits()/MsgBackupLEDs() (2026-07-26 CLEDBlinker/final-
 * prerequisites follow-up batch) -- see poller.h's own per-method header comments and
 * led_blinker.h for the full derivation. All 3 transcribed from Ghidra's own
 * decompile (small enough not to need direct disassembly cross-checking).
 */

int CPoller::MsgSetLed(CMessage &msg)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	if (!(raw[9] & 0x2))
		return 4;
	if (*reinterpret_cast<const short *>(raw + 0xa) != 8)
		return 5;

	int *const *payload = reinterpret_cast<int *const *>(raw + 0x10);
	int ledCode = **payload;
	int state   = (*payload)[1];

	if (mResource == 0)
		return 0;

	int wordIndex = ledCode / 16;
	int bit       = ledCode % 16;
	unsigned short mask = (unsigned short)(1 << bit);
	unsigned short *bitmapWords = reinterpret_cast<unsigned short *>(mZeroBlock);

	struct SResourceMsg {
		unsigned int opcode;
		unsigned int value;
	} local;

	if (state == 1) {
		/* "on" */
		s_oLEDBlinker.Unregister(ledCode);
		if (bitmapWords[wordIndex] & mask)
			return 0; /* already on -- real early-out, no notify */
		bitmapWords[wordIndex] |= mask;
		local.opcode = 2;
	} else if (state == 2) {
		/* "blink" -- real: registers with the global blinker and returns
		 * immediately, no mZeroBlock update, no notify at all on this path.
		 */
		s_oLEDBlinker.Register(ledCode);
		return 0;
	} else {
		/* "off" (any other value, including 0) */
		s_oLEDBlinker.Unregister(ledCode);
		if (!(bitmapWords[wordIndex] & mask))
			return 0; /* already off -- real early-out, no notify */
		bitmapWords[wordIndex] &= (unsigned short)~mask;
		local.opcode = 1;
	}
	local.value = (unsigned int)ledCode;

	typedef void (*NotifyFn)(void *, SResourceMsg *);
	void *resVtbl = *reinterpret_cast<void **>(mResource);
	NotifyFn notify = *reinterpret_cast<NotifyFn *>(reinterpret_cast<char *>(resVtbl) + 0x1c);
	notify(mResource, &local);

	return 0;
}

int CPoller::MsgSetLed16bits(CMessage &msg)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	if (!(raw[9] & 0x2))
		return 4;
	if (*reinterpret_cast<const short *>(raw + 0xa) != 8)
		return 5;

	unsigned int *const *payload = reinterpret_cast<unsigned int *const *>(raw + 0x10);
	int groupIndex = (int)(short)*reinterpret_cast<const short *>(*payload);
	unsigned int packed = (*payload)[1];

	if (mResource == 0)
		return 0;

	unsigned short newBits = (unsigned short)packed;
	unsigned short mask    = (unsigned short)(packed >> 16);

	unsigned short *bitmapWords = reinterpret_cast<unsigned short *>(mZeroBlock);
	unsigned short oldWord = bitmapWords[groupIndex];
	unsigned short newWord = (unsigned short)((~mask & oldWord) | (newBits & mask));
	bitmapWords[groupIndex] = newWord;

	/* Real: always takes the masked LEDs out of the global blink set, regardless
	 * of newBits -- see poller.h's own header comment for this real asymmetry.
	 */
	s_oLEDBlinker.Unregister(groupIndex, mask);

	if (newWord != oldWord) {
		struct SResourceMsg {
			unsigned int opcode;
			unsigned int value;
		} local;
		local.opcode = 6;
		local.value = ((unsigned int)newWord << 16) | (unsigned int)groupIndex;

		typedef void (*NotifyFn)(void *, SResourceMsg *);
		void *resVtbl = *reinterpret_cast<void **>(mResource);
		NotifyFn notify =
		    *reinterpret_cast<NotifyFn *>(reinterpret_cast<char *>(resVtbl) + 0x1c);
		notify(mResource, &local);
	}

	return 0;
}

int CPoller::MsgBackupLEDs(CMessage &msg)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	if (!(raw[9] & 0x1))
		return 4;

	int direction = *reinterpret_cast<const int *>(raw + 0x10);

	if (direction == 0) {
		/* RESTORE: bring back the previously-saved snapshot. */
		memcpy(mZeroBlock, mLedBackup, sizeof(mZeroBlock));
	} else {
		/* SAVE-AND-CLEAR: snapshot the current state, then zero it. */
		memcpy(mLedBackup, mZeroBlock, sizeof(mZeroBlock));
		memset(mZeroBlock, 0, sizeof(mZeroBlock));
	}

	if (mResource != 0) {
		const unsigned short *bitmapWords =
		    reinterpret_cast<const unsigned short *>(mZeroBlock);

		struct SResourceMsg {
			unsigned int opcode;
			unsigned int value;
		} local;

		typedef void (*NotifyFn)(void *, SResourceMsg *);
		void *resVtbl = *reinterpret_cast<void **>(mResource);
		NotifyFn notify =
		    *reinterpret_cast<NotifyFn *>(reinterpret_cast<char *>(resVtbl) + 0x1c);

		/* Real: 32 calls, one per mZeroBlock word (ground truth is a 5-way
		 * unrolled loop -- collapsed to a plain loop, identical result).
		 */
		for (unsigned int i = 0; i < 0x20; i++) {
			local.opcode = 6;
			local.value = ((unsigned int)bitmapWords[i] << 16) | i;
			notify(mResource, &local);
		}
	}

	return 0;
}

/* --- FindRegisteredClient()/MsgGetClientHandleByRef()/MsgGetClientHandleByVal()
 * (2026-07-26 FindRegisteredClient batch) -- see poller.h's own per-method header
 * comments for the full derivation. FindRegisteredClient() transcribed via direct
 * `objdump -dr -M intel` register tracing (same technique as RegisterClient(),
 * same opaque CLink-family pointer chain, confirmed byte-identical to
 * RegisterClient()'s own Phase-1 scan). Both Msg*() wrappers transcribed from
 * Ghidra's own decompile, modeled as real calls to FindRegisteredClient() instead
 * of ground truth's own literal inlined duplicate of the scan (see header comment).
 */

int CPoller::FindRegisteredClient(const char *nameA, const char *nameB) const
{
	if (nameA == 0 || *nameA == 0)
		return 0;
	if (nameB != 0 && *nameB == 0)
		nameB = 0;

	unsigned char **begin = *(unsigned char ***)(mClients + 4);
	unsigned char **end   = *(unsigned char ***)(mClients + 8);
	if (end <= begin)
		return -1;

	for (unsigned char **p = begin; p < end; ++p) {
		const unsigned char *client = *p;
		if (*reinterpret_cast<const int *>(client + 0x14) == 0)
			continue; /* unconnected -- never matched by name */

		int linkP   = *reinterpret_cast<const int *>(client + 0x1c);
		int linkQ   = *reinterpret_cast<const int *>(linkP);
		int nameRec = *reinterpret_cast<const int *>(linkQ + 0x10);
		int nameRecA = *reinterpret_cast<const int *>(nameRec + 0x3c);
		const char *regNameA = *reinterpret_cast<char *const *>(nameRecA + 4);
		if (strcmp(regNameA, nameA) != 0)
			continue;

		if (nameB != 0) {
			const char *regNameB = *reinterpret_cast<char *const *>(nameRec + 4);
			if (strcmp(nameB, regNameB) != 0)
				continue;
		}

		return (int)(p - begin);
	}

	return -1;
}

int CPoller::MsgGetClientHandleByRef(CMessage &msg) const
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	if (!(raw[9] & 0x2))
		return 4;

	unsigned short taggedLen = *reinterpret_cast<const unsigned short *>(raw + 0xa);
	if (taggedLen < 0xc)
		return 5;

	unsigned int *payload = *reinterpret_cast<unsigned int *const *>(raw + 0x10);
	if (payload == 0)
		return 6;

	const char *nameA = reinterpret_cast<const char *>(payload[1]);
	if (nameA == 0 || *nameA == 0)
		return 6;

	/* nameB is OPTIONAL here -- empty string collapses to NULL, no return-6
	 * gate (unlike MsgRegisterClientByRef()'s own mandatory-both-names check).
	 */
	const char *nameB = reinterpret_cast<const char *>(payload[2]);
	if (nameB != 0 && *nameB == 0)
		nameB = 0;

	payload[0] = (unsigned int)FindRegisteredClient(nameA, nameB);
	return 0;
}

int CPoller::MsgGetClientHandleByVal(CMessage &msg) const
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	if (!(raw[9] & 0x2))
		return 4;

	unsigned short taggedLen = *reinterpret_cast<const unsigned short *>(raw + 0xa);
	if (taggedLen < 0x64)
		return 5;

	unsigned char *payload = *reinterpret_cast<unsigned char *const *>(raw + 0x10);
	if (payload == 0)
		return 6;
	if (payload[0x4] == 0)
		return 6;

	const char *nameA = reinterpret_cast<const char *>(payload + 4);
	const char *nameB = reinterpret_cast<const char *>(payload + 0x34);
	if (*nameB == 0)
		nameB = 0;

	*reinterpret_cast<unsigned int *>(payload) = (unsigned int)FindRegisteredClient(nameA, nameB);
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

/* --- MsgSetAnalogClient()/MsgSetButtonClient() (2026-07-26 CPoller closeout batch)
 * -- see poller.h's own per-method header comments for the full derivation.
 * Transcribed via direct `objdump -dr -M intel` register tracing (both functions'
 * SSE `movdqa` bulk-fill loops read more clearly straight from disasm, same
 * rationale as the ctor). Both real lookup tables below are copied VERBATIM from a
 * direct `objdump -s -j .rodata` byte dump (not assumed/guessed) -- see each
 * method's own header comment for what was confirmed.
 */

/* Real .rodata table @ 0x08f7c060, 64 entries x 8 bytes: `{int32_t code; void
 * *namePtr;}`. Only `code` is read by MsgSetAnalogClient() -- `namePtr` (always
 * either NULL or the same single shared address, 0x08f7c260, for every populated
 * entry) is dead data for this function and not modeled. `code == 0` genuinely
 * does NOT match slot 0 (whose own real code is 1) -- see header comment.
 */
static const int s_analogCode[0x40] = {
	 1,  3,  0,  0,  0,  0, 29,  5,  2,  4,  0,  0,  0,  0,  7,  6,
	 8, 16,  0,  0,  0,  0, 28, 14,  9, 17,  0,  0,  0,  0, 27, 15,
	10, 18,  0,  0,  0,  0, 26, 22, 11, 19,  0,  0,  0,  0,  0, 23,
	12, 20,  0,  0,  0,  0,  0, 25, 13, 21,  0,  0,  0,  0,  0, 24,
};

/* Real .rodata table @ 0x08f7b860, 128 entries x 16 bytes: `{int32_t code;
 * int32_t altCode; int32_t flag; void *namePtr;}`. Only `code` (mode 0) and
 * `altCode` (mode 1) are read by MsgSetButtonClient() -- `flag` (always 1) and
 * `namePtr` (always either NULL or the same single shared address for every
 * populated entry, same convention as the analog table above) are dead data for
 * this function and not modeled. Verbatim byte dump confirms `code[i] == i` for
 * i in [0,78] (a real, literal identity mapping in this exact build) and 0 for
 * i in [79,127] (unused padding slots); `altCode[i] == 0` for EVERY entry, i in
 * [0,127] -- see header comment for why this is still modeled as a real loop
 * rather than collapsed to a special case.
 */
static const int s_buttonPrimaryCode[0x80] = {
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static const int s_buttonAltCode[0x80] = { 0 };

int CPoller::MsgSetAnalogClient(CMessage &msg)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	if (!(raw[9] & 0x2))
		return 4;
	if (*reinterpret_cast<const short *>(raw + 0xa) <= 7)
		return 5;

	unsigned int *payload = *reinterpret_cast<unsigned int *const *>(raw + 0x10);
	if (payload == 0)
		return 5; /* real: shares the length gate's own return code, ground
		           * truth never re-sets the return register between the two
		           * checks -- see header comment. */

	unsigned int handle = payload[0];

	if (handle != 0xffffffff) {
		unsigned char **begin = *reinterpret_cast<unsigned char ***>(mClients + 4);
		unsigned char **end   = *reinterpret_cast<unsigned char ***>(mClients + 8);
		unsigned int count = (unsigned int)(end - begin);
		if (handle >= count)
			return 9;
		unsigned char *client = begin[handle];
		if (*reinterpret_cast<const int *>(client + 0x14) == 0)
			return 9;
	}

	int mode = (int)payload[1];

	if (mode == 2) {
		for (unsigned int i = 0; i < 0x40; i++)
			mHandleTable1[i] = handle;
		return 0;
	}
	if (mode != 0)
		return 6;

	if (*reinterpret_cast<const short *>(raw + 0xa) <= 0xb)
		return 5;

	int code = (int)payload[2];

	for (unsigned int i = 0; i < 0x40; i++) {
		if (s_analogCode[i] == code) {
			mHandleTable1[i] = handle;
			break;
		}
	}
	/* Real: an unmatched code is a silent no-op (still returns 0) -- ground
	 * truth's own loop-fallthrough behavior, not an error path.
	 */
	return 0;
}

int CPoller::MsgSetButtonClient(CMessage &msg)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	if (!(raw[9] & 0x2))
		return 4;
	if (*reinterpret_cast<const short *>(raw + 0xa) <= 7)
		return 5;

	unsigned int *payload = *reinterpret_cast<unsigned int *const *>(raw + 0x10);
	if (payload == 0)
		return 5; /* same NULL-shares-length-gate-return-code quirk as
		           * MsgSetAnalogClient() above. */

	unsigned int handle = payload[0];

	if (handle != 0xffffffff) {
		unsigned char **begin = *reinterpret_cast<unsigned char ***>(mClients + 4);
		unsigned char **end   = *reinterpret_cast<unsigned char ***>(mClients + 8);
		unsigned int count = (unsigned int)(end - begin);
		if (handle >= count)
			return 9;
		unsigned char *client = begin[handle];
		if (*reinterpret_cast<const int *>(client + 0x14) == 0)
			return 9;
	}

	int mode = (int)payload[1];

	if (mode == 2) {
		for (unsigned int i = 0; i < 0x80; i++)
			mHandleTable2[i] = handle;
		return 0;
	}
	if (mode != 0 && mode != 1)
		return 6;

	/* Real: modes 0 and 1 share the identical ">0xb" length gate, independently
	 * coded at each real call site (same numeric threshold both places).
	 */
	if (*reinterpret_cast<const short *>(raw + 0xa) <= 0xb)
		return 5;

	int code = (int)payload[2];

	if (mode == 1) {
		for (unsigned int i = 0; i < 0x80; i++) {
			if (s_buttonAltCode[i] == code) {
				mHandleTable2[i] = handle;
				break;
			}
		}
	} else {
		for (unsigned int i = 0; i < 0x80; i++) {
			if (s_buttonPrimaryCode[i] == code) {
				mHandleTable2[i] = handle;
				break;
			}
		}
	}
	/* Real: an unmatched code is a silent no-op (still returns 0), same
	 * fallthrough behavior as MsgSetAnalogClient() above.
	 */
	return 0;
}

/* Tier A (2026-07-26 RegisterClient batch) -- see poller.h's own header comment
 * (both the top-of-file entry and this method's own per-method comment) for the
 * full derivation, including the two deliberate simplifications (CZ name-string
 * construction, vector-insert self-aliasing-range guards) and the one opaque
 * CLink-family pointer chain the Phase-1 name-match scan walks. Transcribed via
 * direct `objdump -dr -M intel` register tracing of the whole 2603-byte body
 * (.text+0x089f31c0), cross-checked against Ghidra's own decompile.
 */
int CPoller::RegisterClient(unsigned int &outHandle, const char *nameA, const char *nameB)
{
	outHandle = 0xffffffff;

	if (nameA == 0 || nameB == 0 || *nameA == 0)
		return 7;
	if (*nameB == 0)
		return 7;

	unsigned char **begin = *reinterpret_cast<unsigned char ***>(mClients + 4);
	unsigned char **end   = *reinterpret_cast<unsigned char ***>(mClients + 8);

	/* Phase 1: scan CONNECTED clients only for an existing (nameA, nameB)
	 * registration. Ground truth is an 8-way Duff's-device-unrolled loop;
	 * collapsed to a plain loop here (identical result, no side effects in the
	 * comparison itself). The name-pair lookup below is an opaque raw pointer
	 * chain into the client's own embedded CLink-family machinery (out_link.h's
	 * mLinks) -- CLink itself is an already-established out-of-scope class
	 * project-wide, so this is transcribed as raw-offset reads, not further
	 * decoded.
	 */
	unsigned char **matched = 0;
	for (unsigned char **p = begin; p < end; ++p) {
		unsigned char *client = *p;
		if (*reinterpret_cast<const int *>(client + 0x14) == 0)
			continue; /* unconnected -- never matched by name */

		int linkP    = *reinterpret_cast<int *>(client + 0x1c);
		int linkQ    = *reinterpret_cast<int *>(linkP);
		int nameRec  = *reinterpret_cast<int *>(linkQ + 0x10);
		int nameRecA = *reinterpret_cast<int *>(nameRec + 0x3c);
		const char *regNameA = *reinterpret_cast<char **>(nameRecA + 4);
		if (strcmp(regNameA, nameA) != 0)
			continue;
		const char *regNameB = *reinterpret_cast<char **>(nameRec + 4);
		if (strcmp(nameB, regNameB) != 0)
			continue;

		matched = p;
		break;
	}

	unsigned int handle;

	if (matched != 0) {
		/* Ground truth re-derives begin/end here and re-checks "index ==
		 * 0xffffffff" before returning -- unreachable in practice (a real
		 * match's index is always within [0, count)), not modeled, same
		 * "unreachable defensive arm" treatment as elsewhere in this project.
		 */
		handle = (unsigned int)(matched - begin);
		outHandle = handle;
		return 1; /* already registered under this exact name pair */
	}

	/* Phase 2: no name match -- reuse the first UNCONNECTED slot, if any (same
	 * scan shape as FindUnconnected() above, independently inlined here rather
	 * than calling it, matching this project's "duplicate real ground-truth
	 * function per call site" convention). Real, preserved quirk: reusing a
	 * free slot does NOT rebind it to (nameA, nameB) -- no new CIfcClient is
	 * built on this path; the slot's own pre-existing name is what the tail
	 * notify call below fires with.
	 */
	unsigned int reuseIdx = 0xffffffff;
	for (unsigned char **p = begin; p < end; ++p) {
		if (*reinterpret_cast<const int *>(*p + 0x14) == 0) {
			reuseIdx = (unsigned int)(p - begin);
			break;
		}
	}

	if (reuseIdx != 0xffffffff) {
		handle = reuseIdx;
		outHandle = handle;
	} else {
		/* Phase 3: append a brand-new CIfcClient named "Out_<mID>". Ground
		 * truth builds that name via the real CZ string-set CONTAINER
		 * (CZ::CZ + CZ::Sprintf) -- a genuinely separate, already
		 * out-of-scope 247-method dependency (cz_util.h) -- modeled here as
		 * a plain snprintf into a fixed stack buffer instead (deliberate
		 * simplification: the ctor below malloc's its own copy of whatever
		 * string is passed in, out_link.h, so this changes nothing any real
		 * caller can observe). `this+0x18` is an opaque CTask field (task.h
		 * doesn't decode it), used here only cosmetically for this
		 * diagnostic display name.
		 */
		char nameBuf[0x78];
		int mIdLike = *reinterpret_cast<int *>(reinterpret_cast<unsigned char *>(this) + 0x18);
		snprintf(nameBuf, sizeof(nameBuf), "Out_%d", mIdLike);

		void *raw = malloc(0x80);
		CIfcClient *client = new (raw) CIfcClient(*this, nameBuf, 0xffffffff);

		/* Append into mClients (TVector<CIfcClient*,1>), growing via the
		 * already-real MakeCapacity() if needed. Ground truth implements the
		 * append as a generic, compiler-emitted insert(iterator, first, last)
		 * with self-aliasing-range guards (does the 1-element source range,
		 * a stack local, overlap mClients's own heap-backed storage?) --
		 * confirmed via direct disassembly (.text+0x089f38c2..0x89f38e9) that
		 * those guards always resolve to the plain grow-then-append path for
		 * this call site on this platform's real address-space layout (a
		 * stack address can never alias a heap allocation); modeled directly
		 * as that path, guards omitted as genuinely dead code.
		 */
		unsigned char **clientsEnd = *reinterpret_cast<unsigned char ***>(mClients + 8);
		unsigned char **clientsCap = *reinterpret_cast<unsigned char ***>(mClients + 0xc);
		if (clientsEnd >= clientsCap) {
			unsigned char **clientsBegin = *reinterpret_cast<unsigned char ***>(mClients + 4);
			unsigned int curCount = (unsigned int)(clientsEnd - clientsBegin);
			TVector_CIfcClientPtr_MakeCapacity(mClients, curCount + 1);
			clientsEnd = *reinterpret_cast<unsigned char ***>(mClients + 8);
		}
		*clientsEnd = reinterpret_cast<unsigned char *>(client);
		*reinterpret_cast<unsigned char ***>(mClients + 8) = clientsEnd + 1;

		unsigned char **newBegin = *reinterpret_cast<unsigned char ***>(mClients + 4);
		unsigned char **newEnd   = *reinterpret_cast<unsigned char ***>(mClients + 8);
		handle = (unsigned int)(newEnd - newBegin) - 1;
		outHandle = handle;

		CTask::Add(client);
	}

	/* Common tail (both Phase 2 and Phase 3 converge here): notify Api's own
	 * vtbl slot +0x44 (system_api.h, new slot this batch) with (Api,
	 * ownerModule->mName, this->mName, mClients[handle]->mName, nameA, nameB,
	 * 0). `this+0x3c` (mOwnerModule) and `this+0x4` (mName) are CTask's own
	 * private fields (task.h) -- crossed via raw `this` reads, same idiom the
	 * ctor above already uses for CTask's own private mVtbl.
	 */
	unsigned char **finalBegin = *reinterpret_cast<unsigned char ***>(mClients + 4);
	unsigned char *chosenClient = finalBegin[handle];
	const char *chosenName = *reinterpret_cast<char **>(chosenClient + 4);

	const unsigned char *ownerModule = *reinterpret_cast<unsigned char *const *>(
	    reinterpret_cast<const unsigned char *>(this) + 0x3c);
	const char *ownerModuleName = *reinterpret_cast<char *const *>(ownerModule + 4);
	const char *taskName =
	    *reinterpret_cast<char *const *>(reinterpret_cast<const unsigned char *>(this) + 4);

	typedef int (*RegisterNotifyFn)(void *, const char *, const char *, const char *,
	                                 const char *, const char *, int);
	void *apiVtbl = *reinterpret_cast<void **>(Api);
	RegisterNotifyFn registerNotify =
	    *reinterpret_cast<RegisterNotifyFn *>(reinterpret_cast<char *>(apiVtbl) + 0x44);
	int result = registerNotify(Api, ownerModuleName, taskName, chosenName, nameA, nameB, 0);

	if (result < 1) {
		outHandle = 0xffffffff;
		return 11;
	}
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
