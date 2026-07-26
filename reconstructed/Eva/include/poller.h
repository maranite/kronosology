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
 * `MsgRegisterClientByVal(CMessage&)` (116B), `MsgRegisterClientByRef(CMessage&)`
 * (114B), `Exec(CMessage&)` (6747B), `Exec()` (3213B, the 0-arg scheduler-tick
 * override). All real addresses in `manifest/eva_functions.csv`. A dedicated future
 * batch (same discipline as the DumpManager/CClientCommServer/CSTGUnsolMsgHandler
 * clusters) could plausibly close most of these -- they need `CMessage` (not
 * reconstructed in this project at all yet, a genuinely separate, larger
 * prerequisite) for every `Msg*(CMessage&)` handler, which is the real reason none
 * of them are attempted here, not any Peg-toolkit or CTask-family depth.
 *
 * UPDATE (Eva Stage 6 CPanel unlock batch, 2026-07-26): `InitAnalogs()`/
 * `InitButtons()` (2919B/2925B, both listed above) are now DECLARED (public, real
 * signatures) but still Tier-B STUBBED (empty bodies) -- CPoller's own real
 * ground-truth caller, `CPanel::Config()`, was found and reconstructed this batch
 * (panel.h/.cpp), so these two needed real call sites even though their own bodies
 * stay out of scope for the same CMessage-prerequisite reason as their still-fully-
 * deferred siblings above.
 *
 * UPDATE (Eva broad nm-C sweep, 2026-07-26, second batch): re-examined the
 * "needs CMessage machinery" framing directly against ground truth (not assumed) --
 * WRONG for 8 of the smaller handlers. This project's own already-established
 * `CMessage`-forward-declared-incomplete-type convention (`CChunkServer::Exec`,
 * chunk_server.cpp; `CSysExMsgTaskBase::Exec`, sysex_msg_task_base.cpp -- raw
 * `reinterpret_cast<unsigned char*>(&msg)` field reads at fixed offsets, `CMessage`
 * itself never given a real definition) applies here just as well; nobody had
 * actually tried it on CPoller's own smaller Msg* handlers yet. 8 promoted Tier B ->
 * Tier A this batch: `MsgShortBeep`, `MsgRequestAnalogInputValue`,
 * `MsgUnregisterClient`, `MsgSetEncoderClient`, `MsgSetTouchPanelClient`,
 * `MsgSetKeyboardClient`, `MsgRegisterClientByVal`, `MsgRegisterClientByRef`. See
 * poller.cpp for the full per-method derivation. `RegisterClient()` itself (the
 * common real callee of the last two) stays Tier-B stubbed -- its own 2603-byte body
 * is genuine, separate depth (drives `FindRegisteredClient()`, the `mClients` TVector
 * growth path, and more), out of scope for this batch -- but IS now declared with its
 * real signature so the two `MsgRegisterClientByXxx` wrappers can compile and
 * dispatch a real call, same declare-real/stub-body convention as `InitButtons()`/
 * `InitAnalogs()` above. Genuinely still-deferred, no new angle found this batch:
 * `MsgSetLed`/`MsgSetLed16bits`/`MsgBackupLEDs` (all three pull in a brand-new,
 * not-yet-reconstructed external singleton class, `CLEDBlinker`, global at
 * `.bss+0x0af09920`, plus reveal that `mZeroBlock` (+0x3a0) is actually a real
 * 512-bit LED-registration bitmap indexed by `ELedCode/16`, not a genuinely-unused
 * scratch block as originally guessed -- worth flagging for whoever reconstructs
 * `CLEDBlinker` next, but out of scope here), `MsgGetClientHandleByRef`/
 * `MsgGetClientHandleByVal` (2600B each, pull in `FindRegisteredClient()`),
 * `MsgSetButtonClient` (1505B), `MsgSetAnalogClient` (1085B),
 * `FindRegisteredClient()` (2512B), `RegisterClient()` itself (2603B, see above),
 * `Exec(CMessage&)` (6747B, the real per-message dispatcher that would call all of
 * these), `Exec()` (3213B, the unrelated 0-arg scheduler-tick override).
 *
 * UPDATE (2026-07-26, RegisterClient reconstruction batch): `RegisterClient(unsigned
 * int&, char const*, char const*)` itself (.text+0x089f31c0, 2603 bytes) is now fully
 * reconstructed (Tier A), via `objdump -dr -M intel` register tracing of the whole
 * function body (Ghidra's own decompile of it was usable as a first-pass map but
 * needed direct disassembly cross-checking for the 3 gnarliest sub-pieces: the name-
 * match scan's raw pointer-chain, the vector-insert self-aliasing-range guards, and
 * the `CZ` name-string construction -- see poller.cpp's own header comment and the
 * per-method comment below for exactly what each of those turned out to mean). Real
 * shape: (1) validate both names non-null/non-empty (return 7); (2) linear scan for
 * an already-CONNECTED client whose registered name pair already matches (nameA,
 * nameB) -- if found, return handle=that index, code 1 ("already registered"); (3)
 * else linear scan for the first UNCONNECTED slot to reuse -- if found, return that
 * index directly WITHOUT constructing anything new (a real, preserved quirk: reusing
 * a free slot does not rebind it to the new name pair, it just re-fires the generic
 * registration notify below using that slot's own pre-existing name); (4) else
 * construct a brand-new `CIfcClient` (name "Out_<mID>") and append it to `mClients`,
 * growing capacity via the already-real `TVector_CIfcClientPtr_MakeCapacity()` if
 * needed; (5) in every case, end with an unconditional call through `Api`'s own vtbl
 * slot `+0x44` (a new opaque slot, system_api.h) with `(Api, ownerModule->mName,
 * this->mName, mClients[handle]->mName, nameA, nameB, 0)` -- success (`result > 0`)
 * returns 0, failure returns 11 and resets `outHandle` to `0xFFFFFFFF`.
 *
 * Two deliberate simplifications, neither changing any real caller's observable
 * behavior (same license as `edit_task.h`'s own `CBatchDiskCmds`-as-`COutLinkMono`
 * simplification): (a) ground truth builds the new client's display name via the
 * real `CZ` string-set CONTAINER (`CZ::CZ(buf,0x78,"")` + `CZ::Sprintf("Out_%d",
 * mID)`) -- that container is a genuinely separate, already-established 247-method
 * out-of-scope dependency (`cz_util.h`'s own header comment); modeled here as a
 * plain `snprintf` into a fixed local buffer instead, since the only externally
 * observable effect is the resulting C string handed to `COutLinkMono`'s ctor, which
 * malloc's its own copy (out_link.h) regardless of how the source buffer was built.
 * (b) ground truth's real "append" is a generic, compiler-emitted `insert(iterator,
 * first, last)` with self-aliasing-range guards (checking whether the source range
 * `[&tmpLocal, &tmpLocal+1)` overlaps `mClients`'s own heap-backed storage) --
 * confirmed via direct disassembly (`objdump -dr` at .text+0x089f38c2..0x89f38e9)
 * that those guards always resolve to the plain grow-then-append path for this call
 * site, given this platform's real address-space layout (a stack-local source range
 * can never alias a heap-allocated `mClients` backing array); modeled directly as
 * that path, guards omitted as genuinely dead code for every real invocation (same
 * "unreachable defensive arm, not modeled" treatment already used for the ctor/
 * `MsgUnregisterClient()` family's own dead index-underflow checks below).
 *
 * The one raw pointer-chain this reconstruction does NOT further decode: each
 * connected client's own `+0x1c` field is walked (`client+0x1c` -> deref -> `+0x10`
 * -> `+0x3c`/`+4` and `+4`) to recover its registered name pair for the Phase-2
 * match scan. This reaches into the client's own embedded `COutLink::mLinks`
 * (`CLink`-family) machinery -- `CLink` itself is already an established, genuinely
 * out-of-scope class project-wide (out_link.h's own header comment) -- so the chain
 * is transcribed as opaque raw-offset reads, same license as every other place this
 * project crosses that same boundary (e.g. the `+0x14` "connected" field itself).
 */

#ifndef POLLER_H
#define POLLER_H

#include "task.h"
#include "out_link.h"

class CModule;
class CMessage;

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

	/* .text+0x089f0150, 65 bytes. Tier A. `CMessage`'s command-code byte (+0x9,
	 * the high byte of the +0x8 16-bit code word, same field every other
	 * `Msg*`/`Exec(CMessage&)` handler in this project tests) bit 0x1 (== code bit
	 * 0x100) gates whether this does anything at all -- matches `CChunkServer::
	 * Exec()`'s own established "bit 0x100 selects single-shot commands" reading of
	 * the same field. If clear, returns 4 with zero side effects. If set and
	 * `mResource` is non-null, notifies it via its own vtbl slot +0x1c (index 7,
	 * real meaning not decoded -- an opaque "trigger" call on the same named
	 * resource the ctor's own Api+0xac lookup populates, poller.cpp) with a 2-dword
	 * `{opcode=7, <uninitialized>}` local. Real ground truth genuinely never
	 * initializes the 2nd dword for THIS handler (unlike
	 * `MsgRequestAnalogInputValue()` below, which does) -- transcribed as found,
	 * not papered over, same license as `hid_driver.cpp`'s own documented
	 * uninitialized-read precedent.
	 */
	int MsgShortBeep(CMessage &msg);

	/* .text+0x089f0420, 80 bytes. Tier A. Same code-bit-0x100 gate and same
	 * `mResource->vtbl[7]` opaque-notify shape as `MsgShortBeep()` above, but
	 * `{opcode=5, value=msg's own +0x10 dword, read as a plain scalar (an analog
	 * channel index), not a pointer}`.
	 */
	int MsgRequestAnalogInputValue(CMessage &msg) const;

	/* .text+0x089f1990, 128 bytes. Tier A. Same code-bit-0x100 gate (return 4 if
	 * clear). Reads `CMessage`'s own +0x10 dword as a plain client HANDLE (not a
	 * pointer -- a different real interpretation of the same generic slot than
	 * `MsgRegisterClientByVal/ByRef()` below use, both real, both confirmed
	 * separately via disassembly). Returns 9 if the handle is `0xFFFFFFFF` or
	 * `>= mClients.Count()`. Returns 2 if the handle is in range but that client's
	 * own +0x14 "connected" field (the same field `FindUnconnected()`/
	 * `IsRegisteredHandle()` already read) is 0. Otherwise notifies the real global
	 * `Api` object's own already-documented `+0x58` "per-outlink" slot
	 * (system_api.h -- the SAME slot `CTask::~CTask()` already calls, task.cpp;
	 * `CIfcClient` genuinely IS-A `COutLink` via `COutLinkMono`, so this is the
	 * identical notification, just fired from a different real call site) with
	 * `(Api, client)`, then returns 0.
	 */
	int MsgUnregisterClient(CMessage &msg);

	/* .text+0x089f2010/0x089f2090/0x089f2110, 128 bytes each. Tier A. All 3 share
	 * one real shape (transcribed separately, not merged into a shared helper --
	 * matches this project's own convention of one real ground-truth function per
	 * method even when byte-identical apart from which field they touch), differing
	 * only in which of `mField394`/`mField398`/`mField39c` they update:
	 * code-bit-0x100 gate (return 4 if clear); the target field is unconditionally
	 * reset to `0xFFFFFFFF` first; `CMessage`'s own +0x10 dword is read as a plain
	 * client handle (same interpretation as `MsgUnregisterClient()` above). If the
	 * handle is exactly `0xFFFFFFFF`, the reset value IS the final value -- return 0
	 * ("clear this client slot" succeeds trivially). Otherwise the handle must be
	 * in-range AND that client's own +0x14 "connected" field must be non-zero, or
	 * this returns 9 (a DIFFERENT return code than `MsgUnregisterClient()`'s own
	 * "not connected" case, which returns 2 -- confirmed via direct disassembly,
	 * not homogenized). On success the target field is set to the validated handle
	 * and this returns 0.
	 */
	int MsgSetEncoderClient(CMessage &msg);
	int MsgSetTouchPanelClient(CMessage &msg);
	int MsgSetKeyboardClient(CMessage &msg);

	/* .text+0x089f31c0, 2603 bytes. Tier A (2026-07-26 RegisterClient batch -- see
	 * the top-of-file header comment for the full derivation, including the two
	 * deliberate simplifications and the one opaque `CLink`-family pointer chain).
	 * Real: validate both names (return 7 if either is null/empty); if a CONNECTED
	 * client already has this exact name pair, return its index with code 1; else
	 * if any UNCONNECTED slot exists, return that slot's index directly (no new
	 * object built); else construct+append a new `CIfcClient` named "Out_<mID>"
	 * (growing `mClients` via `TVector_CIfcClientPtr_MakeCapacity()` if needed) and
	 * call `CTask::Add()` on it. Every path ends by calling through `Api`'s own
	 * vtbl slot `+0x44` (system_api.h) with `(Api, ownerModule->mName, this->mName,
	 * mClients[handle]->mName, nameA, nameB, 0)`; success (`result > 0`) returns 0,
	 * else returns 11 and resets `outHandle` to `0xFFFFFFFF`.
	 */
	int RegisterClient(unsigned int &outHandle, const char *nameA, const char *nameB);

	/* .text+0x089f53f0, 128 bytes. Tier A. Code-bit-0x200 gate (bit 0x2 of the
	 * +0x9 byte -- a DIFFERENT bit-plane than the single-shot handlers above,
	 * matches `CChunkServer::Exec()`'s own established "bit 0x200 gates a
	 * multi-way dispatch" reading of the same field; return 4 if clear). The
	 * length field at +0xa (CMessage's own real "tagged length" word, same field
	 * `CSysExMsgTaskBase::Exec()` already documents) must be `> 0x63` (unsigned),
	 * else return 5. +0x10 is a real payload POINTER here (unlike
	 * `MsgUnregisterClient()`'s scalar-handle use of the same slot) into a fixed
	 * layout: byte +0x4 and byte +0x34 of the payload must both be non-zero
	 * (non-empty embedded C-strings), else return 6. On success, forwards to
	 * `RegisterClient(handle, (char*)payload+4, (char*)payload+0x34)` and
	 * writes the resulting handle back to `payload+0x0`, returning
	 * `RegisterClient()`'s own return value.
	 */
	int MsgRegisterClientByVal(CMessage &msg);

	/* .text+0x089f5470, 128 bytes. Tier A. Same code-bit-0x200 gate. Length must be
	 * `> 0xb` (unsigned), else return 5. +0x10 is a real payload pointer to a
	 * SEPARATE 3-pointer struct here (not the embedded-buffer layout
	 * `MsgRegisterClientByVal()` uses): `payload[0]` is the write-back handle
	 * output slot, `payload[1]`/`payload[2]` are themselves `char*` pointers to
	 * the two names (each checked non-null AND non-empty, i.e. `*name != 0`),
	 * else return 6. On success, forwards to `RegisterClient(handle, payload[1],
	 * payload[2])` and writes the handle back to `payload[0]`, returning
	 * `RegisterClient()`'s own return value.
	 */
	int MsgRegisterClientByRef(CMessage &msg);

	/* .text+0x089f4830, 2925 bytes. Tier-B link-stub (empty body) -- genuinely
	 * large, needs the CMessage machinery this project hasn't reconstructed at
	 * all yet (see "DEFERRED THIS BATCH" above), out of scope for this batch.
	 * Declared/stubbed so CPoller's own real ground-truth caller,
	 * `CPanel::Config()` (.text+0x089ee530, panel.h/.cpp -- Eva Stage 6 CPanel
	 * unlock batch, 2026-07-26), can compile and dispatch a real call, matching
	 * this project's established declare-real-signature/stub-body convention for
	 * out-of-scope callees (e.g. CChkApiInstance::SetOwnerModule/
	 * CRMApiInstance::SetResMan, mains.cpp).
	 */
	void InitButtons();

	/* .text+0x089f3c80, 2919 bytes. Tier-B link-stub, same reasoning as
	 * InitButtons() above -- CPanel::Config()'s other real, unconditional call.
	 */
	void InitAnalogs();

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
