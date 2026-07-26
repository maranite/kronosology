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
 *
 * `Exec()` (2026-07-26 Exec() 0-arg batch) transcribed from .text+0x089ee7d0 (3213
 * bytes) via direct `objdump -dr -M intel` register tracing of the whole body plus
 * a real `objdump -s -j .rodata` byte dump of the 12-entry jump table @
 * `.rodata+0x08f7c268` (Ghidra's own `load_binary` timed out again this session).
 * See poller.h's own per-method header comment for the complete derivation. Adds
 * one new static table, `s_buttonFlag[]`, the button `.rodata` table's own third
 * field -- dead data for `MsgSetButtonClient()` but a real, live gate here.
 *
 * `Exec(CMessage&)` (2026-07-26 Exec(CMessage&) closeout batch) transcribed from
 * .text+0x089f54f0 (6747 bytes) via a full `objdump -dr -M intel` branch-target CFG
 * reachability walk (Ghidra's own `load_binary` timed out again this session,
 * consistent with every other same-day batch). A prior pass's "94 strcmp() sites,
 * not a numeric switch" characterization was a real misdiagnosis, corrected this
 * batch: there IS a real 15-way jump table, and all 15 cases turned out to be
 * ground truth's own inlined duplicate (or, for 3 of them, a real direct call) of
 * one of the 15 `Msg*()` sibling methods above. Modeled as real calls to those
 * siblings, per this file's own established "duplicate real ground-truth function
 * per call site, modeled as a call instead" precedent -- see this method's own
 * header comment (both here and poller.h) for the full per-case correspondence and
 * the shared `PollerTranslateSubResult()` return-code mapping this discovery
 * required. `CPoller` is now fully structurally closed.
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

/* Real .rodata table's own third field (offset +8 of the same 16-byte entries
 * above) -- dead data for MsgSetButtonClient() (never read there) but a real,
 * live gate for Exec()'s own type-3/4/5 BUTTON dispatch below. Byte-dumped
 * directly (`objdump -s -j .rodata --start-address=0x8f7b860`): all 128 entries
 * are 1 in this exact build -- transcribed as a real, live check anyway (per
 * poller.h's own top-of-file note), not collapsed to `true`.
 */
static const int s_buttonFlag[0x80] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

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

/* --- Exec() (2026-07-26 Exec() 0-arg batch) -- see poller.h's own per-method header
 * comment for the complete derivation (12-way jump table, both stack-buffer
 * mechanisms, the LED-phase bitmap diff tail). Transcribed via direct
 * `objdump -dr -M intel` register tracing of the whole 3213-byte body plus a real
 * `objdump -s -j .rodata` byte dump of the 12-entry jump table @
 * `.rodata+0x08f7c268`.
 */
int CPoller::Exec()
{
	if (mResource == 0) {
		SetMask(1);
		return -1;
	}

	/* mResource's own vtbl slot +0x14 (index 5) -- a NEW opaque slot, real
	 * signature `bool(*)(void*, SHwEvent*)`: fills the 8-byte out-param with the
	 * next pending hardware event and returns true, or returns false once none
	 * remain. Same "raw vtbl-offset call on an opaque named resource" convention
	 * every other mResource use in this file already establishes (ctor's own
	 * +0x8/+0xc/+0x10 gates, MsgShortBeep() et al.'s own +0x1c notify).
	 */
	struct SHwEvent {
		unsigned int type;
		unsigned int value;
	};
	typedef int (*FnPollEvent)(void *, SHwEvent *);
	void *resVtbl = *reinterpret_cast<void **>(mResource);
	FnPollEvent pollEvent =
	    *reinterpret_cast<FnPollEvent *>(reinterpret_cast<char *>(resVtbl) + 0x14);

	typedef void (*NotifyFn)(void *, void *);
	NotifyFn resNotify =
	    *reinterpret_cast<NotifyFn *>(reinterpret_cast<char *>(resVtbl) + 0x1c);

	unsigned char **clientsBegin = *reinterpret_cast<unsigned char ***>(mClients + 4);
	unsigned char **clientsEnd   = *reinterpret_cast<unsigned char ***>(mClients + 8);
	unsigned int clientsCount = (unsigned int)(clientsEnd - clientsBegin);

	/* Batch accumulator for event types 1/2 (mField39c-selected client): up to 16
	 * records of `{tag(4B); byte@4; byte@5}`, flushed as one OutMono(0, ...) call
	 * either mid-drain (buffer full) or once more at loop exit if non-empty.
	 */
	unsigned char keyBatch[0x80];
	unsigned char *keyBatchPos = keyBatch;

	/* Handles of ANALOG (type-11) clients whose ring transitioned from empty to
	 * non-empty this tick -- flushed in a second pass below. Real ground-truth
	 * capacity is 64 entries; `mHandleTable1` itself only has 64 slots and the
	 * `mExtra38` gate below prevents queueing the same client twice in one tick,
	 * so this can never actually overflow.
	 */
	unsigned int flushHandles[0x40];
	unsigned int flushCount = 0;

	SHwEvent evt;
	while (pollEvent(mResource, &evt)) {
		if (evt.type > 0xb)
			continue; /* real: ground truth's own unsigned `ja` bounds check
			           * treats any out-of-range type identically to type 0 --
			           * a silent no-op, not a gap here. */

		switch (evt.type) {
		case 0:
			/* Real, genuine no-op -- jump table entry 0 IS the loop's own
			 * "fetch next event" tail (confirmed via direct disassembly). */
			break;

		case 1:
		case 2:
			if (mField39c == 0xffffffff)
				break;
			*reinterpret_cast<unsigned int *>(keyBatchPos) = (evt.type == 1) ? 1u : 0u;
			keyBatchPos[4] = (unsigned char)(evt.value >> 8);
			keyBatchPos[5] = (unsigned char)evt.value;
			keyBatchPos += 8;
			if (keyBatchPos == keyBatch + sizeof(keyBatch)) {
				CIfcClient *client =
				    reinterpret_cast<CIfcClient *>(clientsBegin[mField39c]);
				client->OutMono(0, keyBatch, (unsigned short)sizeof(keyBatch));
				keyBatchPos = keyBatch;
			}
			break;

		case 3:
		case 4:
		case 5: {
			/* real: `index` (used directly, unshifted -- a different real
			 * indexing convention than type 11's own ANALOG lookup below) is
			 * bounds-checked by ground truth against neither `mHandleTable2`'s
			 * own 128-entry size nor the button `.rodata` table's -- same
			 * "transcribed unchecked, a real preserved risk" license as type
			 * 11's own index below. */
			unsigned int index = evt.value;
			unsigned int handle = mHandleTable2[index];
			if (handle == 0xffffffff)
				break;
			if (handle >= clientsCount)
				break;
			CIfcClient *client = reinterpret_cast<CIfcClient *>(clientsBegin[handle]);
			if (*reinterpret_cast<int *>(reinterpret_cast<unsigned char *>(client) + 0x14) == 0)
				break;
			/* real: button table's own `flag` field (offset +8 of the 16-byte
			 * entry) must equal 1 -- always true in this exact build (per
			 * poller.h's own top-of-file note), transcribed as a real, live
			 * gate anyway. */
			if (s_buttonFlag[index] != 1)
				break;

			struct SButtonMsg {
				unsigned int opcode;
				int          code;
				int          altCode;
				unsigned int flag390;
			} local;
			local.opcode  = (evt.type == 5) ? 0u : 1u;
			local.code    = s_buttonPrimaryCode[index];
			local.altCode = s_buttonAltCode[index];
			local.flag390 = mFlag390;
			client->OutMono(1, &local, sizeof(local));
			break;
		}

		case 6: {
			unsigned int handle = mField394;
			if (handle == 0xffffffff)
				break;
			if (handle >= clientsCount)
				break;
			CIfcClient *client = reinterpret_cast<CIfcClient *>(clientsBegin[handle]);
			if (*reinterpret_cast<int *>(reinterpret_cast<unsigned char *>(client) + 0x14) == 0)
				break;

			/* Real: only the low byte of this 4-byte field is ever written --
			 * bytes 1..3 are genuinely uninitialized stack garbage in ground
			 * truth (same "reproduce the real undefined read" license
			 * MsgShortBeep() already established), left uninitialized here too.
			 */
			struct SField394Msg {
				unsigned char loByte;
				unsigned char pad[3];
				unsigned int  flag390;
			} local;
			local.loByte  = (unsigned char)evt.value;
			local.flag390 = mFlag390;
			client->OutMono(2, &local, sizeof(local));
			break;
		}

		case 7:
		case 8:
		case 9:
		case 10: {
			unsigned int handle = mField398;
			if (handle == 0xffffffff)
				break;
			if (handle >= clientsCount)
				break;
			CIfcClient *client = reinterpret_cast<CIfcClient *>(clientsBegin[handle]);
			if (*reinterpret_cast<int *>(reinterpret_cast<unsigned char *>(client) + 0x14) == 0)
				break;

			/* Real: bytes 6/7 of this 8-byte message are genuinely
			 * uninitialized stack garbage (same license as case 6 above).
			 */
			struct SField398Msg {
				unsigned int  opcode;
				unsigned char byte4;
				unsigned char byte5;
				unsigned char pad[2];
			} local;
			local.opcode = (evt.type == 8) ? 0u : (evt.type == 10) ? 2u : 1u;
			local.byte4  = (unsigned char)(evt.value >> 24);
			local.byte5  = (unsigned char)(evt.value >> 8);
			client->OutMono(4, &local, sizeof(local));
			break;
		}

		case 11: {
			/* real: neither `index` (against mHandleTable1's own 64-entry
			 * size) nor its use as an s_analogCode[] index is bounds-checked
			 * by ground truth itself -- transcribed unchecked, a real,
			 * preserved risk if a hardware event ever carried a malformed
			 * `value`, not a gap in this reconstruction. */
			unsigned int index = evt.value >> 16;
			unsigned int handle = mHandleTable1[index];
			if (handle == 0xffffffff)
				break;
			if (handle >= clientsCount)
				break;
			CIfcClient *client = reinterpret_cast<CIfcClient *>(clientsBegin[handle]);
			if (*reinterpret_cast<int *>(reinterpret_cast<unsigned char *>(client) + 0x14) == 0)
				break;

			CPanelOut::SAnalogEvt analogEvt;
			analogEvt.type  = s_analogCode[index];
			analogEvt.value = (short)(evt.value & 0xffff);
			client->PutAnalogEvt(analogEvt);

			/* real: CIfcClient::mExtra38 (opaque raw +0x38 offset, same license
			 * as every other cross-CIfcClient-boundary read in this file)
			 * gates whether this client gets queued for the second-pass flush
			 * below -- only once per tick. */
			int *extra38 = reinterpret_cast<int *>(reinterpret_cast<unsigned char *>(client) + 0x38);
			if (*extra38 == -1) {
				flushHandles[flushCount] = handle;
				*extra38 = (int)flushCount;
				flushCount++;
			}
			break;
		}

		default:
			break;
		}
	}

	/* Real: flushes any partially-filled type-1/2 batch left over once the
	 * drain loop runs out of events (a real, separate flush site from the
	 * "buffer full" one inside the loop above -- ground truth guards this one
	 * on non-empty, same as the one above is only ever reached already-full).
	 */
	if (keyBatchPos != keyBatch && mField39c != 0xffffffff && mField39c < clientsCount) {
		CIfcClient *client = reinterpret_cast<CIfcClient *>(clientsBegin[mField39c]);
		client->OutMono(0, keyBatch, (unsigned short)(keyBatchPos - keyBatch));
	}

	/* Second pass: flush every ANALOG client queued by a type-11 event above
	 * (ground truth is a Duff's-device-unrolled 4-at-a-time scan; collapsed to
	 * a plain loop here, identical result). Modeled as real calls to the
	 * already-reconstructed FlushAnalogEvts() instead of re-inlining ground
	 * truth's own byte-identical duplicate of that same flush logic (same
	 * "duplicate real ground-truth function per call site, modeled as a call
	 * instead" precedent FindRegisteredClient()'s own wrappers already
	 * established).
	 */
	for (unsigned int i = 0; i < flushCount; i++) {
		CIfcClient *client = reinterpret_cast<CIfcClient *>(clientsBegin[flushHandles[i]]);
		client->FlushAnalogEvts();
		*reinterpret_cast<int *>(reinterpret_cast<unsigned char *>(client) + 0x38) = -1;
	}

	int phaseChanged = s_oLEDBlinker.Exec();
	if (phaseChanged == 0)
		return 0;
	if (mResource == 0) /* real, dead-in-practice re-check -- mResource can't
	                      * change value anywhere else in this function; kept
	                      * anyway, same "unreachable defensive arm" license
	                      * RegisterClient()'s own dead checks already use. */
		return 0;

	const unsigned char *ledBase = reinterpret_cast<const unsigned char *>(&s_oLEDBlinker);
	int blinkPhase = *reinterpret_cast<const int *>(ledBase + 4);
	const unsigned short *blinkBitmap = reinterpret_cast<const unsigned short *>(ledBase + 0xc);
	unsigned short *stateWords = reinterpret_cast<unsigned short *>(mZeroBlock);

	struct SResourceMsg {
		unsigned int opcode;
		unsigned int value;
	} local;

	for (unsigned int w = 0; w < 0x20; w++) {
		unsigned short blinkBits = blinkBitmap[w];
		if (blinkBits == 0)
			continue;

		unsigned short oldWord = stateWords[w];
		unsigned short newWord = (blinkPhase != 0)
		    ? (unsigned short)(oldWord | blinkBits)   /* "on" half: force blinking LEDs on */
		    : (unsigned short)(oldWord & ~blinkBits); /* "off" half: force blinking LEDs off */
		if (newWord == oldWord)
			continue;

		stateWords[w] = newWord;
		local.opcode = 6;
		local.value = ((unsigned int)newWord << 16) | w;
		resNotify(mResource, &local);
	}

	return 0;
}

/* --- Exec(CMessage&) (2026-07-26 Exec(CMessage&) closeout batch) -- see poller.h's
 * own per-method header comment for the complete derivation. Transcribed via a full
 * `objdump -dr -M intel` branch-target CFG reachability walk of the whole 6747-byte
 * body (a prior pass's "94 strcmp() sites, not a numeric switch" characterization
 * was a real misdiagnosis this batch corrected -- see header comment).
 *
 * Real: a 15-way jump table on `CMessage`'s own +0x8 16-bit code word (range 0..14,
 * any other value -- including every value > 14 -- falls through to a shared
 * default that returns -1 with zero side effects). Every one of the 15 cases is
 * ground truth's own inlined duplicate (cases 0/11/13: a real direct call instead;
 * confirmed via the actual `call` instructions in the disassembly) of one of the 15
 * `Msg*()` sibling methods above, confirmed by matching each case's own gate
 * bit-plane / length threshold / payload shape against that sibling's own
 * already-verified body, byte-for-byte -- modeled here as real calls to the
 * sibling, per this project's own established "duplicate real ground-truth
 * function per call site, modeled as a call instead" precedent (`RegisterClient()`'s
 * own Phase-2 reuse scan; `MsgGetClientHandleByRef/Val()`'s own duplicate of
 * `FindRegisteredClient()`'s scan; `Exec()`'s own type-11 duplicate of
 * `CIfcClient::PutAnalogEvt()`). This is what the ~94 `strcmp()` sites actually
 * were: cases 6 and 8 each separately inline their OWN full copy of
 * `FindRegisteredClient()`'s own Duff's-device-unrolled scan (confirmed via CFG
 * walk: the "different miss-handler per site" reading that looked like 92 distinct
 * literal string comparisons was really just each unrolled iteration's own
 * `mov eax,[base+4*i]` array-index advance).
 *
 * Every case's own return value passes through the SAME real translation
 * (`PollerTranslateSubResult()` below), confirmed via the `setg dl`/`cmp eax,3`/
 * `cmp eax,7` sequence physically present at several cases' own call/duplicate-tail
 * sites, and via the two giant duplicated-scan cases' (6, 8) own hard-coded jump
 * targets being numerically consistent with the identical mapping.
 */

static int PollerTranslateSubResult(int subResult)
{
	if (subResult > 3) {
		if (subResult > 7)
			return 4;
		return -1;
	}
	return 0;
}

int CPoller::Exec(CMessage &msg)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);

	/* Real: ground truth loads the full 16-bit word at +0x8 but then only ever
	 * consumes its LOW byte for the switch (`movzx ecx,[edx+8]; movzx esi,cl`) --
	 * the high byte of that same word is +0x9, the independent bit-flags byte
	 * every `Msg*()` sibling above tests on its own (`raw[9] & 0x1`/`& 0x2`).
	 * The dispatch selector itself is therefore just this one byte, 0..14.
	 */
	unsigned char code = raw[8];

	/* Real: unsigned `ja` bounds check -- any code > 14 (not just an explicit
	 * "unknown command" sentinel) silently falls through to the default -1
	 * return, zero side effects.
	 */
	if (code > 14)
		return -1;

	switch (code) {
	case 0:  return PollerTranslateSubResult(MsgSetLed(msg));
	case 1:  return PollerTranslateSubResult(MsgSetLed16bits(msg));
	case 2:  return PollerTranslateSubResult(MsgShortBeep(msg));
	case 3:  return PollerTranslateSubResult(MsgBackupLEDs(msg));
	case 4:  return PollerTranslateSubResult(MsgRequestAnalogInputValue(msg));
	case 5:  return PollerTranslateSubResult(MsgRegisterClientByRef(msg));
	case 6:  return PollerTranslateSubResult(MsgGetClientHandleByRef(msg));
	case 7:  return PollerTranslateSubResult(MsgRegisterClientByVal(msg));
	case 8:  return PollerTranslateSubResult(MsgGetClientHandleByVal(msg));
	case 9:  return PollerTranslateSubResult(MsgUnregisterClient(msg));
	case 10: return PollerTranslateSubResult(MsgSetKeyboardClient(msg));
	case 11: return PollerTranslateSubResult(MsgSetButtonClient(msg));
	case 12: return PollerTranslateSubResult(MsgSetEncoderClient(msg));
	case 13: return PollerTranslateSubResult(MsgSetAnalogClient(msg));
	case 14: return PollerTranslateSubResult(MsgSetTouchPanelClient(msg));
	}

	return -1; /* unreachable given the range check above */
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
