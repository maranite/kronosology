/*
 * poller.h  -  CPoller, Eva Stage 6 batch (2026-07-26) -- reassessing a specific,
 * previously-deferred class now that its own flagged blocker (CTask::SetMask()) is
 * real, per this batch's own dispatch instructions.
 *
 * PRIOR VERDICT, AND WHY IT WAS WORTH RE-CHECKING: task.h's own header comment (Stage
 * 6 SetMask/~CTask batch, 2026-07-25) already corrected the ORIGINAL "genuine Peg-
 * toolkit depth" label an even earlier batch summary used (README.md's now-stale
 * "CPoller (29 methods, genuine Peg depth)" line, Stage 6 CHIDDriver/CLinuxPanelDriver
 * batch) -- CPoller has NOTHING to do with the Peg UI toolkit; it is a plain
 * CTask-derived class, same family as CEditor::CPanelIfcTask. The real reason it
 * stayed deferred was concrete and narrow: ~1900-byte ctor, a (then) not-yet-real
 * CTask::SetMask(int) dependency, and an undecoded Api vtable slot +0xac call. This
 * batch verified fresh from `objdump -dr -M intel` (not assumed): the ctor is fully
 * tractable now. SetMask() is real (Stage 6 SetMask/~CTask batch). Api+0xac resolves
 * to the SAME "call shape only, real meaning undecoded" treatment this project
 * already gives Api+0xa0 (mains.cpp's own "fetch a named sub-API" call, system_api.h)
 * -- not a new kind of blocker, just another opaque named-lookup slot.
 *
 * GROUND TRUTH REACHABILITY, newly found this batch: `CPoller::CPoller(CModule
 * const&, char const*)` (.text+0x089ef740, 1933 bytes) is constructed for real by
 * `CPanel::Setup()` (.text+0x089ee6e0) -- `malloc(0x420)` (confirms sizeof(CPoller) ==
 * 0x420 == 1056 bytes exactly), `CPoller::CPoller(raw, this -- CPanel itself, as the
 * ctor's own CModule&, CParameterString::GetParamStr(this+0x30, "..."))`, then
 * `CModule::Add(CTask*)`
 * (already real, module.h/module.cpp) -- i.e. CPoller sits on the EXACT SAME
 * already-real `CModuleManager::Setup()` -> per-module vtable+8 -> `CModule::Add()`
 * spine `CEditor::Setup()` already uses (task.h). `CPanel` itself is a real,
 * NOT-yet-reconstructed per-module class (`CPanel::CPanel(char const*, char const*)`,
 * `::Setup()`, `::Start()`, `::Config()`, 2 dtors -- all real symbols, `nm -C`
 * confirms) -- mains.cpp's own `PTR__CPanelConstructor_08f7c2f0` currently routes
 * `CPanel`'s creation through the shared `ModuleFactoryCreateStub` (returns NULL,
 * mains.cpp's own header comment) for exactly the same reason `CEditor` once did
 * before `CEditorConstructorCreate()` was added -- reconstructing `CPanel` (a
 * DIFFERENT class, deliberately OUT of this batch's own CPoller-only scope) would be
 * the natural next step to make `CPoller` live on this reconstruction's own wired
 * boot path, mirroring the `CEditorConstructorCreate()` precedent exactly. Flagged
 * for a future batch, not pursued here.
 *
 * REAL CLASS SHAPE (CPoller : public CTask, confirmed via `nm -C`/`objdump -dr`):
 *   +0x00..0x7c  CTask base subobject (task.h)
 *   +0x7c  mResource     void* -- result of the Api+0xac named-resource lookup keyed
 *                         off the ctor's own `name` argument (CPanel::Setup()'s own
 *                         `CParameterString::GetParamStr()` result); 0 if `name` was
 *                         NULL, the lookup itself failed, the found resource's own
 *                         vtbl+0x10 call didn't return 10, or its own vtbl+0x8 call
 *                         (args: 0) didn't return 0 -- ANY of those 4 real gates
 *                         failing takes the SAME real fallback path,
 *                         `SetMask(1)` + clear mResource to 0 (see .cpp).
 *   +0x80  mClients      embedded `TVector<CIfcClient*,1>` (vtbl/begin/end/cap, 0x10
 *                         bytes) -- the real client-registration array
 *                         `FindUnconnected()`/`IsValidHandle()`/`IsRegisteredHandle()`
 *                         all walk (see .cpp for all 3, transcribed byte-exact from
 *                         `objdump -dr`). Each element is a `CIfcClient*`; liveness is
 *                         tested via a raw read of THAT element's own +0x14 byte
 *                         offset (== `CIfcClient`'s embedded `COutLink::mLinks` field
 *                         at its own +0xc, out_link.h/omega_ptr_array.h -- opaque,
 *                         raw-offset access, same convention as everywhere else this
 *                         project reads another class's field across a boundary
 *                         without a friend declaration).
 *   +0x90  mHandleTable1 uint32_t[0x40] (256 bytes) -- Duff's-device-unrolled
 *                         `0xFFFFFFFF`-filled by the ctor (real: an SSE `movdqa`-based
 *                         bulk-fill loop once past the initial scalar alignment
 *                         prologue; collapses to a plain loop here, identical result).
 *                         Real per-slot meaning not decoded (a 64-entry generic handle
 *                         table -- button/keyboard/touch client handles by the size,
 *                         but not confirmed against any of the still-deferred
 *                         `MsgSet*Client`/`RegisterClient` bodies that would populate
 *                         it).
 *   +0x190 mHandleTable2 uint32_t[0x80] (512 bytes) -- same fill idiom, second larger
 *                         table (128 entries -- plausibly analog-input client handles
 *                         by relative size, not confirmed for the same reason as
 *                         above).
 *   +0x390 mFlag390      unsigned char, ctor clears to 0. Real meaning not decoded.
 *   +0x394 mField394,
 *   +0x398 mField398,
 *   +0x39c mField39c     3x uint32_t, ctor sets each to 0xFFFFFFFF (a 3rd, small
 *                         fixed-size handle-ish table, or 3 independent "no handle"
 *                         sentinels -- not decoded).
 *   +0x3a0 mZeroBlock     uint32_t[0x10] (64 bytes) -- ctor `rep stos`-zeroes this
 *                         unconditionally, on EVERY path (success or SetMask(1)
 *                         fallback), as the very last step before returning.
 *   +0x3e0 mReserved3e0   unsigned char[0x40] (64 bytes) -- makes sizeof(CPoller) ==
 *                         0x420 (malloc(0x420) at the real `CPanel::Setup()` call
 *                         site, confirmed), but genuinely NOT touched by the ctor
 *                         itself -- populated by `RegisterClient()`/`InitAnalogs()`/
 *                         `InitButtons()`, all deferred this batch (see below).
 *
 * VTABLE: `CPoller`'s own primary vtable (`vtable for CPoller` @ 08f7c360, byte-read
 * directly, same technique as every other vtable-boundary derivation in this
 * project -- e.g. the CHIDDriver/CLinuxPanelDriver batch, task.h's own CObjectBase/
 * CIfcUnknown/CMessageInput identity resolution) has exactly 5 real vfunc slots: [D1
 * dtor][D0 dtor][Exec() -- the 0-arg 3213-byte overload][Exec(CMessage&) -- the
 * 6747-byte overload][ExecMsg(CMessage&) -- INHERITED verbatim from CTask's own
 * vtable, byte-identical pointer value, confirmed via `nm`, i.e. CPoller does NOT
 * override this slot]. A SECOND (this-adjusted, offset-to-top -8) secondary vtable is
 * installed at CPoller+0x08 (the same multiple-inheritance slot task.h's own
 * "mIfcThunk" note already documents for CTask) with 3 slots: [D1-thunk][D0-thunk]
 * [ExecMsg-thunk, ALSO inherited verbatim from CTask's own secondary vtable]. Neither
 * `Exec()` overload nor the base `ExecMsg()` is reconstructed this batch (see below) --
 * per this project's own established rule ("never use real C++ `virtual` for
 * ground-truth vtable slots"), both `PTR__CPoller_08f7c368[5]` (primary) and
 * `PTR__CPoller_08f7c384[3]` (secondary) are install-only EvaVTableStub arrays --
 * same "size the array to the real install-address-relative vfunc count, all slots
 * EvaVTableStub" convention the same-day CChunkServer/CEditor::CChunkServerTask batch
 * established (omega_vtables.h/.cpp), a refinement of the plainer whole-symbol-sized
 * arrays earlier CTask-family batches used. Named by real install address (vtable
 * symbol + 8), not the raw "vtable for CPoller" symbol address itself.
 *
 * NESTED CLASS `CPoller::CIfcClient` (extends `COutLinkMono`, out_link.h -- confirmed
 * via its own ctor's `COutLinkMono::COutLinkMono(this, owner, name, eDirectionOut,
 * 0x804b)` call, the SAME hardcoded `0x804b` literal constant `CTask::CTask()`'s own
 * `lastArg` uses everywhere else in this project, task.h): a small per-client
 * analog-event ring buffer built on top of the already-real `COutLinkMono::OutMono()`
 * IPC send. Real layout:
 *   +0x00..0x38  COutLinkMono base subobject (out_link.h)
 *   +0x38  mExtra38   the ctor's own trailing `int` argument, stored verbatim (real
 *                       meaning not decoded)
 *   +0x3c  mRingBuf    `CPanelOut::SAnalogEvt[8]` (0x40 bytes -- 8-byte element,
 *                       stg_unsol_msg_handler.h's own already-real
 *                       `{int32_t type; int16_t value;}` struct, confirmed identical
 *                       here: `PutAnalogEvt()` copies exactly 4+2 bytes per element
 *                       and advances the cursor by 8)
 *   +0x7c  mCursor     write cursor into mRingBuf; ctor initializes it to
 *                       `&mRingBuf[0]` (empty). The real "ring full" sentinel is
 *                       `mCursor == &mCursor` itself (i.e. exactly one past the last
 *                       valid ring slot) -- reproduced here as `mCursor >=
 *                       (void*)(mRingBuf + sizeof(mRingBuf))`, numerically identical
 *                       given standard layout.
 *
 * `PutAnalogEvt(const CPanelOut::SAnalogEvt&)` (.text+0x089ef670, 350 bytes... actually
 * `PutAnalogEvt` proper is 128 bytes at .text+0x089ef670; the CIfcClient ctor is 80
 * bytes at .text+0x089ef620) -- real body: if the ring isn't full, append and advance
 * (real). If full: flush the WHOLE ring via `COutLinkMono::OutMono(3, ringStart,
 * ringLenBytes)` (ecb=3, hardcoded -- same "opaque IPC channel id" treatment
 * out_link.h already established for OutMono()'s own `ecb` parameter), reset the
 * cursor to the ring start, THEN append the new element (i.e. a full ring always
 * flushes-and-restarts rather than dropping the newest event) -- EXCEPT the real
 * ground truth has one more subtlety: if the ring was already exactly empty at the
 * moment the "full" check fires (impossible in practice given the ring's own
 * capacity math, but the real compiled code checks for it anyway,
 * `cmp eax,edi; je <skip-flush>`), it skips the OutMono() call and appends directly --
 * transcribed as found, not simplified away.
 *
 * `FlushAnalogEvts()` (.text+0x089ef6f0, 71 bytes) -- real body: if the ring is
 * non-empty, `OutMono(3, ringStart, ringLenBytes)` then reset cursor to start; if
 * already empty, no-op. Same OutMono() call shape as PutAnalogEvt()'s own flush path.
 *
 * `TVector<CPoller::CIfcClient*,1>` -- a plain pointer-array growth container, same
 * family as `TVector<CTask::SRegisteredIfc,1>` (task.h/task.cpp) but for a bare
 * `CIfcClient*` element (4 bytes, not the 12-byte `SRegisteredIfc` triple) --
 * `MakeCapacity(unsigned int)` (.text+0x089f7280, 506 bytes) is genuinely its own
 * separate ground-truth symbol (NOT shared with the SRegisteredIfc instantiation,
 * same "each TVector<T,1> instantiation is transcribed separately" rule
 * task.h/task.cpp already established) -- same min-10-then-doubling growth policy,
 * transcribed directly this batch (`TVector_CIfcClientPtr_MakeCapacity()`, poller.cpp)
 * since `RegisterClient()` (its only real caller) is deferred, but the growth
 * primitive itself is small and mechanical enough to do now for a future batch's
 * benefit and for this batch's own KAT coverage of the embedded `mClients` array.
 *
 * DEFERRED THIS BATCH, size/effort not toolkit depth (all genuinely tractable --
 * same mechanical per-message-handler shape this project's many `CSTGxxxMsgHandler`/
 * `CClientCommServer` batches already reconstruct elsewhere, just not done yet):
 * `MsgSetLed(CMessage&)` (350B), `MsgSetLed16bits(CMessage&)` (214B),
 * `MsgShortBeep(CMessage&)` (65B), `MsgBackupLEDs(CMessage&)` (620B),
 * `MsgRequestAnalogInputValue(CMessage&)` (72B),
 * `MsgGetClientHandleByRef(CMessage&) const` (2601B),
 * `MsgGetClientHandleByVal(CMessage&) const` (2590B),
 * `MsgUnregisterClient(CMessage&)` (114B), `MsgSetButtonClient(CMessage&)` (1505B),
 * `MsgSetEncoderClient(CMessage&)` (120B), `MsgSetTouchPanelClient(CMessage&)` (120B),
 * `MsgSetKeyboardClient(CMessage&)` (120B), `MsgSetAnalogClient(CMessage&)` (1085B),
 * `FindRegisteredClient(char const*, char const*) const` (2512B),
 * `RegisterClient(unsigned int&, char const*, char const*)` (2603B),
 * `InitAnalogs()` (2919B), `InitButtons()` (2925B),
 * `MsgRegisterClientByVal(CMessage&)` (116B), `MsgRegisterClientByRef(CMessage&)`
 * (114B), `Exec(CMessage&)` (6747B), `Exec()` (3213B, the 0-arg scheduler-tick
 * override). All real addresses in `manifest/eva_functions.csv`. A dedicated future
 * batch (same discipline as the DumpManager/CClientCommServer/CSTGUnsolMsgHandler
 * clusters) could plausibly close most of these -- they need `CMessage` (not
 * reconstructed in this project at all yet, a genuinely separate, larger
 * prerequisite) for every `Msg*(CMessage&)` handler, which is the real reason none
 * of them are attempted here, not any Peg-toolkit or CTask-family depth.
 */

#ifndef POLLER_H
#define POLLER_H

#include "task.h"
#include "out_link.h"

class CModule;

namespace CPanelOut {
struct SAnalogEvt;
} /* namespace CPanelOut */

class CPoller : public CTask {
public:
	/* .text+0x089ef740, 1933 bytes. Tier A -- see header comment. Real ctor
	 * hardcodes the CTask name "Poller" (a literal .rodata string,
	 * .rodata+0x8e79942 -- NOT the ctor's own `name` argument, which is used
	 * later only for the Api+0xac lookup) and CTask's own `level`/`scheduleFlag`/
	 * `lastArg` as 2/1/0x804b (matching every other real CTask::CTask() caller's
	 * `lastArg` constant in this project).
	 */
	CPoller(const CModule &owner, const char *name);

	/* .text+0x089ef490 (D1), 107 bytes. Tier A -- see header comment. */
	~CPoller();

	/* .text+0x089f3000, 336 bytes. Tier A -- transcribed directly from
	 * `objdump -dr` (a Duff's-device-unrolled linear scan over mClients looking
	 * for the first NON-connected client, i.e. the first element whose own
	 * +0x14 byte is 0 -- collapsed to a plain loop here). Returns the found
	 * element's index, or -1 if every registered client is connected (or the
	 * array is empty).
	 */
	int FindUnconnected() const;

	/* .text+0x089f3150, 36 bytes. Tier A. Real: `handle != 0xffffffff && handle
	 * < mClients.Count()`.
	 */
	bool IsValidHandle(unsigned int handle) const;

	/* .text+0x089f3180, 50 bytes. Tier A. Real: `IsValidHandle(handle) &&
	 * mClients[handle]->+0x14 != 0` (the same raw "is this client connected"
	 * field FindUnconnected() itself reads).
	 */
	bool IsRegisteredHandle(unsigned int handle) const;

	class CIfcClient : public COutLinkMono {
	public:
		/* .text+0x089ef620, 80 bytes. Tier A -- see header comment. */
		CIfcClient(const CTask &owner, const char *name, int lastArg);

		/* .text+0x089ef670, 128 bytes. Tier A -- see header comment. */
		void PutAnalogEvt(const CPanelOut::SAnalogEvt &evt);

		/* .text+0x089ef6f0, 71 bytes. Tier A -- see header comment. */
		void FlushAnalogEvts();

	private:
		int           mExtra38;
		unsigned char mRingBuf[0x40];
		void         *mCursor;

		friend struct PollerTestHooks;
	};

private:
	void          *mResource;      /* +0x7c */
	unsigned char  mClients[0x10]; /* +0x80, embedded TVector<CIfcClient*,1> */
	unsigned int   mHandleTable1[0x40]; /* +0x90 */
	unsigned int   mHandleTable2[0x80]; /* +0x190 */
	unsigned char  mFlag390;       /* +0x390 */
	unsigned char  mPad391[3];     /* not confirmed real; keeps mField394 aligned */
	unsigned int   mField394;      /* +0x394 */
	unsigned int   mField398;      /* +0x398 */
	unsigned int   mField39c;      /* +0x39c */
	unsigned int   mZeroBlock[0x10]; /* +0x3a0 */
	unsigned char  mReserved3e0[0x40]; /* +0x3e0, pads sizeof(CPoller) to real 0x420 */

	friend struct PollerTestHooks;
};

/* Real, separate ground-truth symbol (.text+0x089f7280, 506 bytes) -- see header
 * comment for why this is its own transcription, not shared with
 * TVector_SRegisteredIfc_MakeCapacity() (task.cpp).
 */
void TVector_CIfcClientPtr_MakeCapacity(unsigned char *vec, unsigned int n);

#endif /* POLLER_H */
