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
 *                         CONFIRMED (2026-07-26, CPoller closeout batch, correcting
 *                         this comment's own prior guess): the 64-entry ANALOG
 *                         client-handle table -- `MsgSetAnalogClient()`'s (below)
 *                         real, sole RUNTIME writer. CORRECTED (2026-07-26,
 *                         final-closeout batch): `InitAnalogs()` (below) is the
 *                         table's real, boot-time FIRST writer -- 29 of the 64
 *                         slots get a live `RegisterClient()` handle at
 *                         `CPanel::Config()` time, before any `MsgSetAnalogClient()`
 *                         call could ever run; "sole writer" only ever meant "sole
 *                         RUNTIME writer".
 *   +0x190 mHandleTable2 uint32_t[0x80] (512 bytes) -- same fill idiom, second larger
 *                         table (128 entries). CONFIRMED (2026-07-26, same batch):
 *                         the 128-entry BUTTON client-handle table --
 *                         `MsgSetButtonClient()`'s (below) real, sole RUNTIME writer.
 *                         CORRECTED (2026-07-26, final-closeout batch): same
 *                         correction as `mHandleTable1` above -- `InitButtons()`
 *                         (below) is the table's real, boot-time first writer (78
 *                         of the 128 slots).
 *   +0x390 mFlag390      unsigned char, ctor clears to 0. Real meaning not decoded.
 *   +0x394 mField394,
 *   +0x398 mField398,
 *   +0x39c mField39c     3x uint32_t, ctor sets each to 0xFFFFFFFF (a 3rd, small
 *                         fixed-size handle-ish table, or 3 independent "no handle"
 *                         sentinels -- not decoded).
 *   +0x3a0 mZeroBlock     uint32_t[0x10] (64 bytes) -- ctor `rep stos`-zeroes this
 *                         unconditionally, on EVERY path (success or SetMask(1)
 *                         fallback), as the very last step before returning.
 *   +0x3e0 mLedBackup     unsigned char[0x40] (64 bytes) -- makes sizeof(CPoller) ==
 *                         0x420 (malloc(0x420) at the real `CPanel::Setup()` call
 *                         site, confirmed), genuinely NOT touched by the ctor
 *                         itself. CORRECTED (2026-07-26, CLEDBlinker/final-
 *                         prerequisites follow-up batch): an earlier version of
 *                         this comment guessed it was "populated by
 *                         RegisterClient()/InitAnalogs()/InitButtons()" -- never
 *                         actually confirmed, and WRONG: all 3 of those are now
 *                         real (RegisterClient() fully, InitAnalogs()/
 *                         InitButtons() as declared Tier-B stubs whose real
 *                         ground-truth bodies are independently known not to
 *                         touch this offset) and none of them reference `+0x3e0`.
 *                         The real, concrete use: `MsgBackupLEDs()` (below) is
 *                         the sole real reader/writer -- a 64-byte BACKUP COPY of
 *                         `mZeroBlock`, saved into on one call direction and
 *                         restored from on the other. Renamed from
 *                         `mReserved3e0` to `mLedBackup` to reflect this.
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
 * [ExecMsg-thunk, ALSO inherited verbatim from CTask's own secondary vtable].
 * `Exec()` (the 0-arg overload) is now reconstructed (2026-07-26 batch, see its own
 * per-method header comment below) but, same as `Exec(CMessage&)`/base `ExecMsg()`
 * (both still deferred), is NOT installed as a real C++ `virtual` slot -- per this
 * project's own established rule ("never use real C++ `virtual` for ground-truth
 * vtable slots"), both `PTR__CPoller_08f7c368[5]` (primary) and
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
 * `InitAnalogs()` above. Genuinely still-deferred at the time this note was written:
 * `MsgSetLed`/`MsgSetLed16bits`/`MsgBackupLEDs` (all three pull in a brand-new,
 * not-yet-reconstructed external singleton class, `CLEDBlinker`, global at
 * `.bss+0x0af09920`, plus reveal that `mZeroBlock` (+0x3a0) is actually a real
 * 512-bit LED-registration bitmap indexed by `ELedCode/16`, not a genuinely-unused
 * scratch block as originally guessed -- worth flagging for whoever reconstructs
 * `CLEDBlinker` next, but out of scope here -- SUPERSEDED, see the "CLEDBlinker/
 * final-prerequisites follow-up batch" UPDATE below: all 3 are now Tier A),
 * `MsgGetClientHandleByRef`/
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
 *
 * UPDATE (2026-07-26, CLEDBlinker/final-prerequisites follow-up batch): dispatched
 * to re-check CPoller's own 3 remaining named prerequisites (`CMessage`,
 * `CLEDBlinker`, `FindRegisteredClient()`'s own siblings) for fresh tractability now
 * that `CPoller` itself is fully structurally exhausted otherwise
 * ([[eva_registerclient_reconstruction_2026-07-26]]). `CLEDBlinker` (flagged by the
 * prior batch as "a brand-new external singleton class, bigger than fits a small-
 * handler sweep") turned out to be the smallest "whole new class" unlock in this
 * project so far -- 6 methods, all under 100 bytes, no vtable at all -- see the new
 * `led_blinker.h`/`led_blinker.cpp` for the full writeup. Reconstructed it, then
 * used it to promote `MsgSetLed`/`MsgSetLed16bits`/`MsgBackupLEDs` (all three,
 * per-method comments above) from deferred to Tier A. This ALSO corrected a stale
 * guess about `mLedBackup` (+0x3e0, previously `mReserved3e0`) -- see that field's
 * own comment above; it's a real LED-state backup/restore buffer, not something
 * `RegisterClient()`/`InitAnalogs()`/`InitButtons()` populate.
 *
 * UPDATE (2026-07-26, FindRegisteredClient batch, same session): the "concrete next
 * candidate" flagged just above turned out fully tractable, same session --
 * `FindRegisteredClient()` (2512B) itself, plus its own real callers
 * `MsgGetClientHandleByRef()` (2601B) and `MsgGetClientHandleByVal()` (2590B), are
 * now all Tier A (per-method comments above). Both `Msg*()` wrappers turned out to
 * be small thin forwarders despite their large raw byte counts -- ground truth
 * INLINES its own full copy of the connected-client name-match scan into each
 * wrapper (same "duplicate real ground-truth function per call site" pattern
 * `RegisterClient()`'s own Phase-2 scan already established) rather than calling a
 * shared function; modeled here as real calls to `FindRegisteredClient()` instead,
 * collapsing ~2600 bytes of duplicated scan logic into a real, small wrapper each
 * (same size-vs-real-depth lesson `CAlphaKeybCtrl`'s own 4289-byte ctor batch
 * already logged). CPoller's own remaining genuinely-deferred surface after this:
 * `MsgSetButtonClient` (1505B), `MsgSetAnalogClient` (1085B -- neither pulls in
 * `CLEDBlinker` or `FindRegisteredClient()`, no new angle tried on either this
 * session), `Exec(CMessage&)` (6747B, the real per-message dispatcher that would
 * route to every `Msg*()` handler above), `Exec()` (3213B, the unrelated 0-arg
 * scheduler-tick override -- confirmed this session, via a direct `objdump -dr`
 * call-target check, to be `CLEDBlinker::Exec()`'s own real, single caller).
 *
 * UPDATE (2026-07-26, Exec() 0-arg batch): `Exec()` (the 0-arg scheduler-tick
 * override, 3213B) is now fully reconstructed (Tier A) -- see its own per-method
 * header comment above for the complete derivation (12-way jump table, the two
 * distinct stack-buffer accumulator/queue mechanisms, the LED-phase bitmap diff
 * tail). `CPoller`'s only remaining genuinely-deferred surface after this is
 * `Exec(CMessage&)` (6747B, the real per-message string-command dispatcher --
 * see its own note above: ~94 `strcmp()` call sites, NOT a numeric switch, a
 * completely separate mechanism from every other handler in this file).
 *
 * UPDATE (2026-07-26, Exec(CMessage&) closeout batch): the "~94 strcmp() sites,
 * not a numeric switch" reading above was a real misdiagnosis, corrected this
 * batch via a full branch-target CFG reachability walk (not just a byte-count/
 * strcmp-count glance) -- see `Exec(CMessage&)`'s own per-method header comment
 * below for the full writeup. There IS a 15-way numeric jump table, and every one
 * of its 15 cases turned out to be ground truth's own inlined duplicate (or, for
 * 3 of them, a real direct call) of one of the 15 `Msg*()` sibling methods already
 * reconstructed above -- modeled here as real calls to those siblings, collapsing
 * the two ~700-instruction duplicated-`FindRegisteredClient()`-scan cases (the
 * true source of the ~94 `strcmp()` count) down to one-line wrappers each.
 * `Exec(CMessage&)` is now Tier A. `CPoller` is now FULLY structurally closed --
 * no remaining deferred surface of its own (`InitButtons()`/`InitAnalogs()` stayed
 * Tier-B link-stubs for the separate, already-documented `CMessage`-machinery
 * reason, not a CPoller-specific gap).
 *
 * UPDATE (2026-07-26, final-closeout batch): the "needs CMessage machinery"
 * verdict on `InitButtons()`/`InitAnalogs()` above was itself wrong, same
 * "size implies depth" misdiagnosis class as `Exec(CMessage&)`'s own prior
 * "~94 strcmp() sites" note two paragraphs up. Both functions' large size is
 * GCC re-inlining `RegisterClient()`'s own already-real Phase-1 scan; net
 * effect is a plain loop over a real `.rodata` table calling the already-real
 * `RegisterClient()` sibling. Both now Tier A -- see their own per-method
 * header comments below for the full byte-verified derivation. `CPoller` has
 * genuinely ZERO remaining deferred surface of its own as of this update.
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

	/* .text+0x089eff00, 350 bytes. Tier A (2026-07-26 CLEDBlinker/final-
	 * prerequisites follow-up batch -- see led_blinker.h for the new CLEDBlinker
	 * class this unlocks). `CMessage`'s own +0x9 byte bit 0x2 gates this (return 4
	 * if clear -- a DIFFERENT bit-plane than every single-shot handler above,
	 * confirmed by direct disassembly, not homogenized); the +0xa length word
	 * must be exactly 8 (return 5 otherwise). +0x10 is a real payload POINTER to a
	 * 2-int struct `{ledCode, state}` (dereferenced twice: `**ptr` and `ptr[1]`).
	 * If `mResource == 0`, returns 0 with no other side effect. Otherwise
	 * dispatches on `state`:
	 *   state == 1 ("on"): `CLEDBlinker::Unregister(ledCode)` on the GLOBAL
	 *     `s_oLEDBlinker` (this LED stops blinking), then sets the corresponding
	 *     bit in THIS `CPoller` instance's OWN `mZeroBlock` LED-state bitmap
	 *     (+0x3a0) -- if that bit was ALREADY set, returns 0 immediately with NO
	 *     notify (a real early-out, transcribed as found); otherwise sets it and
	 *     falls through to the shared notify tail with opcode 2.
	 *   state == 2 ("blink"): `CLEDBlinker::Register(ledCode)` on
	 *     `s_oLEDBlinker` and returns 0 IMMEDIATELY -- no `mZeroBlock` update, no
	 *     notify at all on this path. A real, preserved quirk: entering blink
	 *     mode via this handler never itself pushes a notification (presumably
	 *     `CLEDBlinker::Exec()`'s own real caller, not reconstructed, handles
	 *     that separately once blinking is actually driven) -- not "fixed."
	 *   anything else ("off", including 0): `CLEDBlinker::Unregister(ledCode)`,
	 *     then clears the corresponding `mZeroBlock` bit -- if it was ALREADY
	 *     clear, returns 0 immediately with no notify (same early-out shape as
	 *     the "on" case); otherwise clears it and falls through with opcode 1.
	 * Shared tail (both the "on" and "off" fall-through paths): notifies
	 * `mResource`'s own vtbl slot +0x1c (index 7, the same opaque slot
	 * `MsgShortBeep()`/`MsgRequestAnalogInputValue()` above already use) with a
	 * 2-dword `{opcode (1 or 2), ledCode}` local, then returns 0.
	 */
	int MsgSetLed(CMessage &msg);

	/* .text+0x089f0070, 214 bytes. Tier A -- see led_blinker.h. Same +0x9 bit-0x2
	 * gate and +0xa==8 length check as `MsgSetLed()` above. +0x10 is a payload
	 * POINTER to a 2-dword struct: `*ptr` (read as a signed 16-bit LED-GROUP
	 * index, i.e. an absolute word index into `mZeroBlock`, NOT a per-bit
	 * `ELedCode` to divide/mod -- a real, different interpretation than
	 * `MsgSetLed()`'s own scalar `ledCode`) and `ptr[1]` (a dword whose low 16
	 * bits are the NEW state for every masked bit, high 16 bits are the mask of
	 * WHICH bits this message updates -- i.e. `{mask=high16, newBits=low16}`).
	 * If `mResource == 0`, returns 0 immediately (no `mZeroBlock` update at all --
	 * a real, different early-out shape than `MsgSetLed()`, which always updates
	 * its own bitmap regardless of `mResource`; transcribed as found, not
	 * homogenized). Otherwise: computes `newWord = (oldWord & ~mask) | (newBits &
	 * mask)` and writes it into `mZeroBlock`'s word at the given group index;
	 * calls `CLEDBlinker::Unregister(groupIndex, mask)` on `s_oLEDBlinker` (bulk-
	 * unregisters every bit in `mask` from the global blink set, REGARDLESS of
	 * `newBits` -- i.e. this handler always takes the affected LEDs OUT of
	 * blinking, even if `newBits` would have kept them "on"; a real, preserved
	 * asymmetry with `MsgSetLed()`'s own state==2 path, which is the only way
	 * back INTO blinking); if the word's value actually changed, notifies
	 * `mResource`'s vtbl+0x1c slot with `{opcode=6, value=(newWord<<16)|
	 * groupIndex}` (a DIFFERENT opcode/payload shape than `MsgSetLed()`'s own
	 * tail -- reusing the same slot, third distinct real payload shape seen on
	 * it after `MsgShortBeep()`'s `{7,x}` and `MsgSetLed()`'s `{1/2,ledCode}`).
	 */
	int MsgSetLed16bits(CMessage &msg);

	/* .text+0x089f01a0, 620 bytes. Tier A -- see led_blinker.h. `CMessage`'s own
	 * +0x9 byte bit 0x1 gates this (the "single-shot" bit-plane, same as
	 * `MsgShortBeep()`/`MsgRequestAnalogInputValue()` above -- NOT the bit-0x2
	 * plane `MsgSetLed()`/`MsgSetLed16bits()` use, confirmed by direct
	 * disassembly). +0x10 (a plain scalar int here, not a pointer) selects the
	 * direction: 0 means RESTORE (copy `mLedBackup` -> `mZeroBlock`, i.e. bring
	 * back a previously-saved LED-state snapshot); nonzero means SAVE-AND-CLEAR
	 * (copy `mZeroBlock` -> `mLedBackup`, THEN zero `mZeroBlock`) -- this is
	 * the real, concrete use of `mLedBackup` (+0x3e0), CORRECTING this header's
	 * own prior guess ("populated by RegisterClient()/InitAnalogs()/
	 * InitButtons()," never actually confirmed by any of those 3 methods'
	 * own real bodies, all already reconstructed and none of which touch
	 * `+0x3e0`): it is a 64-byte BACKUP COPY of `mZeroBlock`, save/restore driven
	 * entirely by this one handler. If `mResource != 0`, then unconditionally (on
	 * EITHER direction) notifies `mResource`'s vtbl+0x1c slot once per word of
	 * the (now-current) `mZeroBlock`, 32 calls total (`{opcode=6,
	 * value=(word<<16)|wordIndex}` for `wordIndex` 0..0x1f) -- ground truth is a
	 * 5-way-unrolled loop; modeled as a plain `for` loop, identical result.
	 */
	int MsgBackupLEDs(CMessage &msg);

	/* .text+0x089f25e0, 2512 bytes. Tier A (2026-07-26 FindRegisteredClient
	 * batch -- direct follow-up to the CLEDBlinker unlock above, per that batch's
	 * own flag that this is "a concrete, same-scale sibling of the already-real
	 * RegisterClient(), same objdump -dr register-tracing technique likely
	 * applies directly"). Confirmed: byte-exact same Duff's-device-unrolled
	 * connected-client name-match scan `RegisterClient()`'s own Phase-1 already
	 * uses (identical `+0x1c -> deref -> +0x10 -> +0x3c/+4` and `+4` opaque
	 * `CLink`-family pointer chain -- see RegisterClient()'s own header comment
	 * and poller.cpp), collapsed to a plain loop the same way. Real: `nameA ==
	 * NULL` or empty returns 0 (found nothing, but a distinct return value from
	 * the genuine "array empty" case below); an empty `nameB` (`*nameB == 0`) is
	 * treated as `NULL` (matches `RegisterClient()`'s own "empty string
	 * collapses to null" convention for its own name args); if `mClients` is
	 * empty, returns -1 (genuinely different from the "nameA missing" 0 case --
	 * transcribed as found, not homogenized). Scans only CONNECTED clients
	 * (unconnected ones' own name-pair field is never populated, same
	 * `RegisterClient()` rationale); if `nameB == NULL`, matches on `nameA`
	 * alone; otherwise requires BOTH names to match. Returns the found client's
	 * index, or -1 if the scan completes with no match.
	 */
	int FindRegisteredClient(const char *nameA, const char *nameB) const;

	/* .text+0x089f0470, 2601 bytes. Tier A (2026-07-26 FindRegisteredClient
	 * batch). Code-bit-0x200 gate (return 4 if clear, same bit-plane
	 * `MsgRegisterClientByVal/ByRef()` use). Length must be `>= 0xc` (ground
	 * truth writes this as `< 0xc` -> return 5, the same effective threshold as
	 * `MsgRegisterClientByRef()`'s own `<= 0xb`, just spelled the other way).
	 * `+0x10` is a real payload pointer to a 3-dword struct: `payload[0]` is the
	 * write-back handle output slot, `payload[1]` is a `char*` name A (must be
	 * non-null AND non-empty, else return 6), `payload[2]` is an OPTIONAL `char*`
	 * name B (empty string collapses to `NULL`, same convention
	 * `FindRegisteredClient()` itself uses -- no return-6 check on this one,
	 * unlike `MsgRegisterClientByRef()`'s own mandatory-both-names gate, since a
	 * lookup can legitimately search by name A alone). Ground truth INLINES its
	 * own full byte-exact copy of the scan `FindRegisteredClient()` above
	 * implements (same project-wide "duplicate real ground-truth function per
	 * call site" pattern already established for `RegisterClient()`'s own
	 * Phase-2 reuse scan) -- modeled here as a real call to
	 * `FindRegisteredClient()` instead, semantically identical, not a
	 * simplification of behavior. Writes the result to `payload[0]` and always
	 * returns 0 (no failure return past the initial 3 gates).
	 */
	int MsgGetClientHandleByRef(CMessage &msg) const;

	/* .text+0x089f0f00, 2590 bytes. Tier A (2026-07-26 FindRegisteredClient
	 * batch). Same code-bit-0x200 gate. Length must be `>= 0x64` (100 decimal --
	 * ground truth writes `< 100`, the identical numeric threshold as
	 * `MsgRegisterClientByVal()`'s own `<= 0x63`). `+0x10` is a real payload
	 * pointer into a fixed embedded-buffer layout, same shape as
	 * `MsgRegisterClientByVal()`'s own payload: name A lives at `payload+4`
	 * (must be non-empty, else return 6), name B lives at `payload+0x34`
	 * (OPTIONAL -- empty string collapses to `NULL`, no return-6 check, same
	 * "search by name A alone" allowance as `MsgGetClientHandleByRef()` above).
	 * Same real inlined-scan-modeled-as-a-`FindRegisteredClient()`-call
	 * treatment. Writes the result to `payload[0]` and always returns 0.
	 */
	int MsgGetClientHandleByVal(CMessage &msg) const;

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

	/* .text+0x089f2190, 1085 bytes. Tier A (2026-07-26 CPoller closeout batch).
	 * CORRECTS this header's own prior "not confirmed against MsgSet*Client"
	 * guess for `mHandleTable1` (+0x90): direct `objdump -dr -M intel`
	 * confirms THIS handler is `mHandleTable1`'s real, sole writer, i.e.
	 * `mHandleTable1` is the 64-entry ANALOG client-handle table (see
	 * `mHandleTable2`'s own sibling correction on `MsgSetButtonClient()` below).
	 * Same +0x9 bit-0x2 gate and length `> 7` gate as `MsgSetLed()`/
	 * `MsgSetButtonClient()` (return 4/5). `+0x10` is a real payload POINTER to
	 * a 3-dword struct: `payload[0]` (handle to bind), `payload[1]` (mode),
	 * `payload[2]` (analog channel code, mode-0 only). A NULL payload pointer
	 * shares the SAME return code (5) as the length gate -- ground truth never
	 * re-sets the return register between the two checks, transcribed as
	 * found, not homogenized with `MsgUnregisterClient()`'s own distinct
	 * "return 6 for a bad payload" convention.
	 *
	 * If `payload[0]` (handle) is NOT `0xFFFFFFFF`, it must be a valid,
	 * CONNECTED `mClients` index (same range/connected checks as
	 * `MsgSetEncoderClient()` et al. above) or this returns 9 -- checked
	 * BEFORE `payload[1]`'s mode is even read, confirmed by direct
	 * disassembly ordering (not assumed from the other `MsgSet*Client`
	 * siblings' own shape). `0xFFFFFFFF` itself skips this check entirely
	 * (an explicit "unbind" sentinel that always succeeds).
	 *
	 * `payload[1]` (mode) dispatches 3 ways: mode 2 bulk-fills EVERY one of
	 * `mHandleTable1`'s 64 entries with the handle (ground truth is a
	 * `movdqa`-based SSE fill loop, collapsed to a plain loop here, identical
	 * result) and returns 0 -- `payload[2]` is never read on this path. Any
	 * mode other than 0 or 2 returns 6. Mode 0 re-gates on length `> 0xb`
	 * (return 5 -- a SEPARATE, independently-coded check at this real call
	 * site, same numeric threshold as the gate above it), then reads
	 * `payload[2]` as an analog channel CODE and linearly scans the real,
	 * ground-truth `.rodata` lookup table @ `.rodata+0x08f7c060` (64 entries,
	 * 8-byte stride -- byte-dumped directly via `objdump -s`, NOT assumed;
	 * see `s_analogCode[]` in poller.cpp for the verbatim 64 values) for a
	 * matching code; the FIRST matching table slot's index becomes
	 * `mHandleTable1`'s written index. If no slot matches, this is a SILENT
	 * no-op -- ground truth's own loop-fallthrough returns 0 with no write,
	 * not an error (same shape `MsgSetButtonClient()`'s own code-scan below
	 * uses). Real, preserved quirk: code 0 does NOT match slot 0 in this
	 * table (slot 0's real code is 1) -- it silently binds to whichever
	 * earlier all-zero PADDING slot the scan reaches first instead (slot 2),
	 * an asymmetry with `MsgSetButtonClient()`'s own code table (whose slot 0
	 * genuinely IS code 0) -- transcribed as found, not corrected.
	 */
	int MsgSetAnalogClient(CMessage &msg);

	/* .text+0x089f1a10, 1505 bytes. Tier A (2026-07-26 CPoller closeout batch).
	 * CORRECTS this header's own prior "not confirmed" guess for
	 * `mHandleTable2` (+0x190): confirmed THIS handler's real, sole writer --
	 * `mHandleTable2` is the 128-entry BUTTON client-handle table. Same real
	 * shape as `MsgSetAnalogClient()` above (+0x9 bit-0x2 gate, length `> 7`
	 * gate, NULL-payload-shares-length's-return-code-5 quirk, handle
	 * range/connected pre-check before the mode is read, mode-2 bulk-fill of
	 * the WHOLE table via a collapsed SSE loop, silent-no-op-on-unmatched-code
	 * scan tail) -- differs only in: table size (128, not 64), field offset
	 * (`mHandleTable2` @ +0x190, not `mHandleTable1` @ +0x90), and a genuinely
	 * EXTRA third mode. `payload[1]` dispatches 4 ways here: mode 1 scans a
	 * DIFFERENT field of the same 128-entry, 16-byte-stride button lookup
	 * table @ `.rodata+0x08f7b860` (byte-dumped directly; see
	 * `s_buttonAltCode[]` in poller.cpp) -- its own real "alt code" field,
	 * confirmed via direct disassembly to be uniformly 0 across all 128 real
	 * table entries in this exact ground-truth build, i.e. mode 1 only ever
	 * matches when `payload[2] == 0`, landing on slot 0 (transcribed as a
	 * real, genuine loop over the verbatim all-zero field, not collapsed to a
	 * special case, since a differently-built binary's table need not stay
	 * all-zero). Mode 2 bulk-fills all 128 slots (same shape as
	 * `MsgSetAnalogClient()`'s own mode 2). Mode 0 (the default, same `> 0xb`
	 * re-gate) scans the table's own PRIMARY code field (`s_buttonPrimaryCode[]`,
	 * confirmed via `objdump -s` to be the literal sequential values 0..78 for
	 * slots 0..78, then 0 for the remaining padding slots 79..127 -- a real,
	 * verbatim identity mapping in this exact build, not an assumption). Any
	 * mode other than 0, 1, or 2 returns 6.
	 */
	int MsgSetButtonClient(CMessage &msg);

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

	/* .text+0x089ee7d0, 3213 bytes. Tier A (2026-07-26 Exec() 0-arg batch --
	 * direct follow-up to the CLEDBlinker/final-prerequisites batch, which
	 * characterized this function's overall shape but deferred the actual
	 * transcription; see that batch's own closing memory note). Transcribed via
	 * direct `objdump -dr -M intel` register tracing of the whole body plus a
	 * real `objdump -s -j .rodata` byte dump of the 12-entry jump table @
	 * `.rodata+0x08f7c268` (Ghidra's own `load_binary`/decompile timed out again
	 * this session, consistent with several other same-day batches).
	 *
	 * This is the scheduler-tick override (the OTHER `Exec()` overload,
	 * `Exec(CMessage&)`, is a completely different per-message string-command
	 * dispatcher -- see that method's own separate deferred-status note; the two
	 * share nothing but a name). Real shape, in call order:
	 *
	 * 1. If `mResource == 0`, calls `SetMask(1)` and returns -1 immediately (the
	 *    same masked-fallback shape the ctor's own 4-gate resource-lookup uses).
	 *
	 * 2. Otherwise, drains `mResource`'s own vtbl slot `+0x14` (index 5, a NEW
	 *    opaque slot on the same named resource `MsgShortBeep()`/`MsgSetLed()`
	 *    already call through `+0x1c` -- real signature `bool(*)(void*, SHwEvent*)`
	 *    where `SHwEvent` is an 8-byte `{unsigned int type; unsigned int value;}`
	 *    out-param) in a `while` loop -- one call per pending hardware event,
	 *    until it returns false. Each drained event's `type` field is dispatched
	 *    through the real 12-way jump table (`type > 11` is treated identically
	 *    to `type == 0` by ground truth's own unsigned `ja` bounds check -- a
	 *    silent no-op, confirmed via direct disassembly, not assumed):
	 *      - type 0: genuine no-op (the jump table's own entry 0 address IS the
	 *        loop's own "fetch next event" tail -- a real, confirmed join point,
	 *        not a gap in this reconstruction).
	 *      - types 1/2: BATCH-accumulate an 8-byte record `{tag(4B); byte@4=
	 *        (value>>8)&0xff; byte@5=value&0xff}` (tag=1 for type 1, tag=0 for
	 *        type 2) into a 16-record/128-byte stack buffer, gated on
	 *        `mField39c != 0xFFFFFFFF`; flushes the WHOLE buffer via a single
	 *        `mField39c`-selected client's `OutMono(0, buf, len)` call either
	 *        when the buffer fills (16 records) -- draining continues afterward
	 *        with a fresh empty buffer -- or once more, only if non-empty, when
	 *        the event-drain loop itself runs out of events (confirmed via the
	 *        real `cmp edi,ebp; je <skip>` guard at the loop-exit join point).
	 *      - types 3/4 (opcode=1) and 5 (opcode=0): `mHandleTable2`-selected
	 *        BUTTON client (index = `value`, used directly, unshifted -- a real,
	 *        different indexing convention than type 11's own ANALOG lookup
	 *        below), gated on the real button `.rodata` table's own `flag`
	 *        field (`s_buttonPrimaryCode`'s sibling table entry, offset +8)
	 *        `== 1` (per poller.h's own top-of-file note, always true in this
	 *        exact build -- transcribed as a real, live gate anyway, not
	 *        collapsed). Sends a 16-byte `{opcode; code; altCode; flag390}`
	 *        message via `OutMono(1, ...)`.
	 *      - type 6: `mField394`-selected client, an 8-byte `{byte0=value&0xff;
	 *        3 genuinely UNINITIALIZED padding bytes; dword@4=mFlag390}` message
	 *        via `OutMono(2, ...)` -- ground truth's own real `mov BYTE PTR
	 *        [...],cl` is a single-byte store into a 4-byte stack field, same
	 *        "reproduce the real undefined read, don't paper over it" license
	 *        `MsgShortBeep()` already established.
	 *      - types 7/9 (opcode=1), 8 (opcode=0), 10 (opcode=2): all four share
	 *        ONE real body (confirmed via direct disassembly -- types 7 and 9
	 *        jump to the literal same address), `mField398`-selected client, an
	 *        8-byte `{opcode; byte@4=(value>>24)&0xff; byte@5=(value>>8)&0xff;
	 *        2 uninitialized padding bytes}` message via `OutMono(4, ...)`.
	 *      - type 11: the ANALOG path. `index = value>>16`, `code =
	 *        s_analogCode[index]` (the SAME `.rodata` table
	 *        `MsgSetAnalogClient()` uses), `handle = mHandleTable1[index]`
	 *        (neither `index` bound is checked against the table's own real
	 *        0x40-entry size by ground truth itself -- transcribed unchecked, a
	 *        real, preserved risk if a hardware event ever carried a malformed
	 *        `value`, not a gap in this reconstruction), gated on `handle` being
	 *        a valid CONNECTED `mClients` index. Pushes `{code, (int16_t)(value
	 *        & 0xffff)}` into that client's own analog-event ring -- modeled as
	 *        a real call to the already-reconstructed `CIfcClient::PutAnalogEvt()`
	 *        instead of re-inlining ground truth's own byte-identical duplicate
	 *        of that same ring-push logic (same "duplicate real ground-truth
	 *        function per call site, modeled as a call instead" precedent
	 *        `FindRegisteredClient()`'s own wrappers already established). If
	 *        the client's own `mExtra38` field (opaque raw `+0x38` offset, same
	 *        license as every other cross-`CIfcClient`-boundary read in this
	 *        file) was `-1` (i.e. not already queued for flush THIS tick),
	 *        records the handle into a second, separate 64-entry stack list and
	 *        sets `mExtra38` to a "now queued" sentinel (its exact numeric value
	 *        doesn't matter beyond being `!= -1` -- ground truth stores a
	 *        monotonic counter there, but nothing ever reads it back as a real
	 *        index).
	 *
	 * 3. After the drain loop exits: if the type-11 queue collected zero
	 *    handles, skips straight to step 4. Otherwise, for each queued handle
	 *    (ground truth is a Duff's-device-unrolled 4-at-a-time scan; collapsed
	 *    to a plain loop here, identical result), calls that client's own
	 *    already-real `FlushAnalogEvts()` (flushes the ring via `OutMono(3,...)`
	 *    only if non-empty, same "model the duplicate as a call" precedent as
	 *    step 2's type-11 handling above) and unconditionally resets its
	 *    `mExtra38` back to `-1`.
	 *
	 * 4. Calls the global `s_oLEDBlinker.Exec()` (led_blinker.h). If it returned
	 *    0 (no blink-phase change this tick), returns 0 immediately -- the real,
	 *    dead-in-practice `mResource == 0` re-check here (mResource can't
	 *    change value anywhere else in this function) is still transcribed,
	 *    same "unreachable defensive arm, kept anyway" license as
	 *    `RegisterClient()`'s own dead index checks. Otherwise walks all 32
	 *    words of `s_oLEDBlinker`'s own private "currently blinking" bitmap
	 *    (opaque raw `+0xc` offset -- same license as every other cross-class
	 *    raw read in this file) against THIS `CPoller` instance's own
	 *    `mZeroBlock`: for each word where the blink-bitmap is non-zero,
	 *    computes a new `mZeroBlock` word as `(old & ~blinkBits)` when
	 *    `s_oLEDBlinker`'s own `mBlinkPhase` (opaque raw `+0x4` offset) is 0
	 *    (forces every blinking LED in that word OFF), or `(old | blinkBits)`
	 *    when `mBlinkPhase != 0` (forces every blinking LED ON) -- confirmed via
	 *    direct disassembly which of the two branches corresponds to which
	 *    phase value, not assumed from symmetry. If the computed word actually
	 *    changed, writes it back and notifies `mResource`'s own vtbl `+0x1c`
	 *    slot with the SAME `{opcode=6, value=(newWord<<16)|wordIndex}` shape
	 *    `MsgSetLed16bits()`/`MsgBackupLEDs()` already use on that slot.
	 *    Returns 0 unconditionally once the 32-word sweep completes.
	 */
	int Exec();

	/* .text+0x089f54f0, 6747 bytes. Tier A (2026-07-26 Exec(CMessage&) closeout
	 * batch -- direct follow-up to the CLEDBlinker/FindRegisteredClient/Exec()
	 * batches above, which left this as CPoller's only remaining deferred surface).
	 * A prior pass characterized this as "a genuine NAME-STRING command dispatcher,
	 * 94 `strcmp()` sites, not a numeric switch" -- WRONG, corrected this batch via a
	 * full `objdump -dr -M intel` CFG reconstruction (branch-target reachability
	 * analysis per case, not just a byte-count/strcmp-count glance): there IS a real
	 * numeric switch (a 15-way jump table @ `.rodata+0x8f7c298` on the LOW BYTE of
	 * `CMessage`'s own +0x8 16-bit word (`movzx ecx,[edx+8]; movzx esi,cl` --
	 * ground truth loads the full word but only the low byte ever feeds the
	 * switch; the high byte of that same word is +0x9, the independent bit-flags
	 * byte every `Msg*()` sibling below tests on its own), range 0..14, `> 14`
	 * and every other out-of-range value falling through to a shared default
	 * that returns -1 with zero side effects.
	 *
	 * The concrete, load-bearing finding: EVERY ONE of the 15 cases is ground
	 * truth's own INLINED DUPLICATE of one of the 15 already-real `Msg*()` sibling
	 * methods above (`MsgSetLed`, `MsgSetLed16bits`, `MsgShortBeep`,
	 * `MsgBackupLEDs`, `MsgRequestAnalogInputValue`, `MsgRegisterClientByRef`,
	 * `MsgGetClientHandleByRef`, `MsgRegisterClientByVal`, `MsgGetClientHandleByVal`,
	 * `MsgUnregisterClient`, `MsgSetKeyboardClient`, `MsgSetButtonClient`,
	 * `MsgSetEncoderClient`, `MsgSetAnalogClient`, `MsgSetTouchPanelClient`, in code
	 * order 0..14) -- confirmed by matching each case's own gate bit-plane, length
	 * threshold, and payload-field shape against that sibling's own already-verified
	 * header comment, byte-for-byte, not by assumption. Three cases (0, 11, 13 --
	 * `MsgSetLed`/`MsgSetButtonClient`/`MsgSetAnalogClient`) are REAL calls to the
	 * symbol (confirmed `call` instructions in the disassembly). The other 12 are
	 * ground truth's own literal inlined duplicate of the sibling's body (same
	 * "duplicate real ground-truth function per call site" pattern already
	 * established repeatedly in this file -- `RegisterClient()`'s own Phase-2 reuse
	 * scan, `MsgGetClientHandleByRef/Val()`'s own duplicate of
	 * `FindRegisteredClient()`'s scan, `Exec()`'s own type-11 duplicate of
	 * `CIfcClient::PutAnalogEvt()`) -- modeled here as real calls to the sibling
	 * instead, per that same precedent, NOT a simplification of behavior. This is
	 * what the ~94 `strcmp()` sites actually were: cases 6 and 8
	 * (`MsgGetClientHandleByRef`/`MsgGetClientHandleByVal`) each separately inline
	 * their OWN full byte-exact copy of `FindRegisteredClient()`'s own Duff's-device-
	 * unrolled connected-client name-match scan (confirmed via a branch-target CFG
	 * walk: the two scans' own miss-handlers are plain `mov eax,[esi+4*k]` array-
	 * index advances, NOT a genuine 92-way string table -- the "different literal
	 * command string per branch" reading was a real misdiagnosis this batch
	 * corrected). Every other case's own inline duplicate is comparatively small
	 * (tens of instructions, not hundreds).
	 *
	 * Every case's own return value passes through ONE shared, real translation
	 * (confirmed via the `setg`/`cmp eax,3`/`cmp eax,7` sequence physically present
	 * at cases 0/5/7/9/11/12/13/14's own call/duplicate-tail sites, and via the
	 * cases-6/8 giant duplicates' own hard-coded jump targets being numerically
	 * consistent with the identical mapping): raw sub-result 0..3 -> 0; 4..7 -> -1;
	 * 8+ -> 4 (see `PollerTranslateSubResult()`, poller.cpp). This explains what
	 * looked like inconsistent per-case return codes before this batch (e.g.
	 * `MsgUnregisterClient()`'s own raw 9/2/0/4 becoming this function's own 4/0/0/-1)
	 * -- every sibling's raw return code survives, just remapped through this one
	 * real translation shared by all 15 cases.
	 */
	int Exec(CMessage &msg);

	/* .text+0x089f4830, 2925 bytes. Tier A (2026-07-26 CPoller final-closeout
	 * batch) -- CORRECTS the prior "Tier-B, needs CMessage machinery" verdict
	 * (this header's own previous note, and `MsgSetButtonClient()`'s "sole
	 * writer" claim on `mHandleTable2` below, both now stale). The 2925-byte
	 * size is NOT algorithmic depth: it is GCC re-inlining an entire copy of
	 * `RegisterClient()`'s own already-real Phase-1 "already registered?"
	 * scan directly into this loop body (same duplicated-scan bug class
	 * `Exec(CMessage&)`'s cases 6/8 already found and collapsed) -- confirmed
	 * via direct `objdump -dr -M intel` register tracing: the giant unrolled
	 * middle walks `mClients` (+0x84/+0x88 begin/end) via the identical
	 * Duff's-device shape, then unconditionally stores the resulting index
	 * into `mHandleTable2[i]` and calls the REAL `RegisterClient(&mHandleTable2[i],
	 * nameA, nameB)` (`.text+0x089f31c0`, a direct, unambiguous `call`
	 * instruction, not indirect) when no existing match was found. Modeled
	 * as a real loop that just calls the already-real sibling -- semantically
	 * identical, since `RegisterClient()` itself performs the exact same
	 * lookup-or-insert check internally.
	 *
	 * The per-button `(nameA, nameB)` pair comes from a real, byte-dumped
	 * `.rodata` table (`objdump -s -j .rodata --start-address=0x8f7b860`,
	 * 128 entries x 16 bytes -- the SAME table `s_buttonPrimaryCode[]`/
	 * `s_buttonAltCode[]`/`s_buttonFlag[]` already transcribe from fields
	 * +0/+4/+8; this function reads the entry's 4th field, +0xc, a `void**`
	 * name-pair pointer). Byte-verified: entries 1..78 (78 of the 128 slots,
	 * NOT slot 0 -- slot 0's own name pointer is NULL despite
	 * `s_buttonPrimaryCode[0] == 0`, a real, preserved asymmetry) all share
	 * the identical name-pair pointer (`.rodata+0x8f7c260` -> `("Editor",
	 * "PanelIfcTask")`, both strings verbatim in `.rodata`); slots 0 and
	 * 79..127 have a NULL name pointer and are skipped entirely (handle left
	 * at the loop's own `0xffffffff` init, no `RegisterClient()` call). This
	 * means at boot every one of the 78 populated button slots registers the
	 * SAME sibling link (`CEditor::CPanelIfcTask`, the class already
	 * reconstructed via `editor.h`'s `CPanelIfcTask` -- consistent with that
	 * class's own established role as the button/analog/encoder event sink),
	 * each getting its own distinct `CIfcClient` handle; `MsgSetButtonClient()`
	 * (poller.cpp) can later overwrite individual slots at runtime by code,
	 * matching `s_buttonPrimaryCode[]`'s own identity-mapped 1..78 range.
	 *
	 * CORRECTS `MsgSetButtonClient()`'s own header comment above (and this
	 * one's prior claim): that method is NOT `mHandleTable2`'s "sole writer" --
	 * this constructor-time initializer runs first and is the table's real,
	 * documented first writer; `MsgSetButtonClient()` remains the only
	 * RUNTIME writer, which is what that comment was actually establishing.
	 */
	void InitButtons();

	/* .text+0x089f3c80, 2919 bytes. Tier A (2026-07-26 CPoller final-closeout
	 * batch) -- same shape, bug class, and correction as `InitButtons()`
	 * above, over `mHandleTable1`/64 analog slots instead of `mHandleTable2`/
	 * 128 button slots. Ground truth's per-slot `.rodata` table is the SAME
	 * one `s_analogCode[]` (poller.cpp) already transcribes from
	 * `.rodata+0x8f7c060` (64 entries x 8 bytes, `{int32_t code; void
	 * *namePtr;}`) -- this function reads the entry's 2nd field (`namePtr`,
	 * `s_analogCode[]`'s own comment already calls this "dead data for
	 * MsgSetAnalogClient()"; it is very much alive here). Byte-verified: 29
	 * of the 64 slots (indices 0,1,6,7,8,9,14,15,16,17,22,23,24,25,30,31,32,
	 * 33,38,39,40,41,47,48,49,55,56,57,63) have a non-NULL `namePtr`, and
	 * EVERY one of those 29 shares the identical pointer InitButtons() uses
	 * (`.rodata+0x8f7c260` -> `("Editor", "PanelIfcTask")`); the remaining 35
	 * slots are skipped (handle left at `0xffffffff`). Same
	 * `RegisterClient(&mHandleTable1[i], "Editor", "PanelIfcTask")` real call
	 * for each populated slot, same "not the sole writer" correction to
	 * `MsgSetAnalogClient()`'s own header comment above.
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
	unsigned char  mLedBackup[0x40]; /* +0x3e0, pads sizeof(CPoller) to real 0x420 */

	friend struct PollerTestHooks;
};

/* Real, separate ground-truth symbol (.text+0x089f7280, 506 bytes) -- see header
 * comment for why this is its own transcription, not shared with
 * TVector_SRegisteredIfc_MakeCapacity() (task.cpp).
 */
void TVector_CIfcClientPtr_MakeCapacity(unsigned char *vec, unsigned int n);

#endif /* POLLER_H */
