# Eva Reconstruction — Session Summary, 2026-07-25

**Update (post-write, same day)**: this doc was written mid-session at ~18:17
UTC; several more commits landed after, including a real regression fix (an
`argv`/`envp` call-site bug in `main()` that combined with this same day's
`CCommDriver::setupfifoname()` work to reintroduce a boot-time segfault —
found by a dedicated full-tree verification pass, root-caused, fixed in
commit `2db4e32`, and re-confirmed live: clean `Start closing`/`End closing`
trace restored) and a large `CDumpManMod`/`CDumpBuffer`/`CCircByteBuffer`
cluster (34 more functions). The specific commit/manifest counts and a few
"still open" items below are therefore stale — see `git log -- reconstructed/Eva/`
and `manifest/eva_functions.csv` for current ground truth; the qualitative
picture (what kind of work this was, the established techniques) still holds.

20+ commits landed today under `reconstructed/Eva/` (17 code-reconstruction/fix
commits, 3 documentation-only or manifest-reconciliation commits, plus more
after this doc was first written — see note above). Eva's boot
path itself was already finished the day before (2026-07-24: the process boots
end-to-end in `kronos_vm` and reaches a clean natural shutdown, zero crashes —
see the README's "Boot-path crash chain closed out" section). Today's work is
entirely **Stage 6, the "breadth sweep"**: following the module/scheduler/task
dependency graph outward from that already-working boot path, converting
Stage-4's placeholder Tier-B link-stubs into real (Tier A), disassembly-verified
bodies wherever a genuine ground-truth caller exists. The manifest went from
96 to **207 of 37,795** functions marked reconstructed as of this doc's first
writing (independently recounted directly from `manifest/eva_functions.csv`,
not taken from any commit message) — higher now, see the update note above.

Two things distinguish today's session from OA.ko's own equivalent (see
`reconstructed/OA/SESSION_SUMMARY_2026-07-25.md`): Eva is a plain userspace
ELF, so there is no separate "hardware review log" — open items are
documented inline in each header's own comment block, cited below instead of
a separate file — and at least two other concurrent agent sessions worked on
this same tree today (self-documented repeatedly in the commit messages
themselves: stashed/restored a colliding `test_task.cpp`, deliberately left
another session's own uncommitted manifest entries untouched, shifted VM port
ranges to avoid a concurrent session's own boot test). `6fa1359`/`3bba5d8`
each explicitly flag their own manifest-file overlap with a concurrent
session rather than resolve it inline; `abf285d` is the follow-up commit
that lands the still-missing side of that overlap, purely reconciliation,
no new reconstruction.

---

## Scheduler / module family

The spine that makes the rest of today's work reachable: getting
`CScheduler`'s per-tick dispatch and `CModuleManager`'s module bookkeeping
from "real code over permanently-empty arrays" to "real code over arrays that
actually get populated."

- **`CScheduler::Exec()` + `CLevelManagerArray::Add()`/`Find()`** (`d7e941f`,
  batch 1) — `CLevelManagerArray` IS-A `COmegaPtrArray`; `Add()` appends via
  the real base method then sifts the new element left while its own level
  number is smaller than its predecessor's, keeping the array sorted
  ascending regardless of insertion order. Previously both were
  always-empty/always-not-found stubs, making `Exec()`'s own per-tick loop
  dead code even once transcribed. `RunLevel()` itself stayed Tier-B this
  batch (needs `CTaskBuffer` + `CModule`'s own vtable dispatch, supplied
  next).
- **`CModule`'s real vtable, `CTaskBuffer`, real `CLevelManager::RunLevel()`**
  (`3bba5d8`, batch 2) — corrects a real mislabeling from batch 1 along the
  way: the level's task queue holds `TNamedPtrArray<CTask>`, not
  `TNamedPtrArray<CModule>` (confirmed by ground-truth field arithmetic
  against `CTask`'s own real ctor layout). `RunLevel()` now genuinely drains
  the level's `CTaskBuffer`, then walks its (still currently always-empty)
  task queue, decrementing/reloading each unmasked task's countdown and
  dispatching `Exec()` on expiry.
- **`CModuleManager::AddModule()`/`EnableUpdate()`** (`aa8843e`, batch 3) —
  upgraded from Tier-B stubs to real by-name-scan/dedup-and-replace logic.
  Both have real boot-path callers (`mains.cpp`'s 8 `MMainXxx` registration
  shims call `AddModule()`; `CKernel::InitSystemLayer()` calls
  `EnableUpdate(1)` directly), so `mModules`/`mTopologyChanged` had been
  stuck at their construction-time zero values the same way batch 1's
  `CLevelManagerArray` was. See "Real bugs" below for the two crash-risk
  fixes this required. Live `kronos_vm` re-boot confirmed identical clean
  trace, zero regressions.
- **`CModule::AdjustTaskMask()`** (`9817e14`, batch 5) — an 8×
  Duff's-device-unrolled reverse walk over `mTasks`, clearing bit `0x02` of
  each task's `+0x4c` mask byte (the same gate bit `CLevelManager::RunLevel()`
  checks). Real body, but provably inert in this reconstruction's own call
  graph at the time (`mTasks` was still permanently empty — nothing yet
  constructed a `CTask`).
- **`CTask::CTask()`/`CLimiterMan::CLimiterMan()`/`CModule::Add(CTask*)`**
  (`f881187`) — corrects a stale "zero callers" verdict from batches 2 and 5:
  `CTask::CTask()` genuinely is called (`CEditor::CPanelIfcTask`'s and
  `CPoller`'s own ctors), and `CModule::Add(CTask*)` — the actual
  `mTasks`-populating method neither prior batch had found — is itself
  boot-path-reachable via `CEditor::Setup() -> CModuleManager::Setup()`. New
  `verify/test_task.cpp` chains all four real functions end to end:
  construct → `Add()` → `RunLevel()` (correctly skips the pre-masked task) →
  `AdjustTaskMask()` (un-masks it) → `RunLevel()` (now ticks it).
- **`CTask::SetMask()`/`~CTask()`** (`988e741`) — `SetMask()` is the trivial
  bit-0x01 sibling to `AdjustTaskMask()`'s own bit 0x02. `~CTask()` (800
  bytes) drains `mOutLinks` and destroys the embedded `CLimiterMan` via the
  already-real `COmegaPtrArray` methods, and identifies three further real
  base-class vtable identities (`CObjectBase`, `CIfcUnknown`,
  `CMessageInput`) read directly off `.rodata` `vtable for X`/`typeinfo for
  X` symbols — confirming `CLimiterMan` IS-A `CIfcUnknown`. Unblocks 6 of
  `CSysExMsgTaskBase`'s methods (below).
- **`CModuleManager::AddConstructor()`/`RemoveConstructor()`** (`7d5bc26`) —
  the *second*, distinct `CModuleManager` registry (`mConstructors`, the
  "module factory array" `CreateUserModules()`/`CreateFMDrivers()` had been
  waiting on). Real by-name dedup/removal, same shape as `AddModule()`. Also
  fixes the vtable-slot bug described below.

## IPC / message substrate

- **`USTGUserAPI`/`CSTGHandle` IPC substrate close-out** (`6fa1359`) —
  reconstructs the remaining 8 real send/receive/teardown methods
  (`Disconnect`, `ConnectUnsolicitedFifo`, `ReadMessage`,
  `ReadMessageWithTimeout`, `ReadUnsolicitedMessage`, `SendPanelMessage`,
  `GetProgress`/`IncrementProgress`/`SetProgress`) plus
  `CSTGHandle::Release`/`GetSize` and `CSTGHandleCache::Cleanup` — all Tier
  A. Finds a 5th real device node (`/dev/rtf5`, the unsolicited-message
  channel) and a separate progress-reporting channel
  (`/proc/OmapNKS4ProgressBar`). A real sweep (not inference) also confirms
  zero call sites from anywhere in the reconstructed boot path into any of
  149 real Peg-prefixed GUI classes — Stage 4/5 (Peg toolkit) stays correctly
  un-started. New host-side KAT drives real wire-format logic against actual
  pipes and catches a real POSIX-pipe-vs-presumed-RTAI-FIFO blocking-semantics
  mismatch.
- **`CCommDriver::setupfifoname()`** (`94ad5fc`, batch 4) — real
  per-`argv`-entry `NAME=VALUE` parser for the 3 fifo paths, gated on
  `Eva_IsSimulation()`/`Eva_IsSimulationSVGA()`. Confirms `CCommDriver` is
  effectively simulator-only on real hardware (all 3 fields stay NULL).
  Flags, but does not fix, a real ground-truth bug — see "Real bugs" below.
- **`CSTGUnsolMsgHandler`, a 30-method unsolicited-STGMessage dispatcher**
  — found via the same broad `nm -C` class-inventory sweep technique that
  found OA.ko's `CSTGControlMsgHandler` earlier this project. Confirmed
  real, boot-path-adjacent (constructed inside `CEditor::CPanelIfcTask`'s
  ctor, itself called from `CEditor::Setup()`). Worked in 3 batches:
  - `12c83d4` (batch 6) — 18 of 30 real: ctor (a real 17-entry
    `{code*,adj}` dispatch table, confirming `STGMessage`'s `offset+4` field
    is the message subtype index), both destructor-shaped functions,
    `HandleMessage()`, `EndHandling()`, `SendValueSlider()`/
    `SendValueEncoder()`, `EnterGlobalObjectEdit()`, and 8 methods
    confirmed genuinely empty `return;` bodies in the shipped binary itself.
  - `72a2909` (batch 2) — 5 more: `PatchMsgHandler`/`EffectMgrMsgHandler`/
    `EffectMsgHandler`/`HDRTrackMsgHandler`/`SetListMsgHandler`, all sharing
    one real shape (a `CStorage` selection guard, an `EditApi` scope-id
    lookup, a real `.rodata` byte table, and the same "set param" vtable
    dispatch `EndHandling()` had already established as a dead branch, now
    real). Found and fixed a real bug — see below.
  - `e37daa6` — `EffectSlotMsgHandler` promoted: a 15-way switch on the
    message's own sub-index plus a reused stack buffer that initially looked
    like it might carry uninitialized-stack-garbage. Resolved (not routed
    around) by tracing every write site: each writes one fully-determined
    byte, and every call site passes `len==1`, so the buffer's other 3 bytes
    are never read — reconstructed as a plain byte scalar with no
    behavioral loss.
  - Net: **24 of 30 methods now Tier A; 6 remain deliberate Tier-B
    link-stubs** (`ControlMsgHandler`, `GlobalMsgHandler`, `CombiMsgHandler`,
    `ProgramSlotMsgHandler`, `ProgramMsgHandler`, `VoiceModelMsgHandler` — all
    reach into genuinely deep, out-of-scope `CControlSurface`/`CMMI`/
    `CModeManager`/`CStorage` algorithm-database state).
- **`CClientCommServer`/`CSysExMsgTaskBase` reachability + `SetupSysex`**
  (`73acd2a`) — thorough follow-up on batch 6's own lower-confidence
  verdict on these two classes. Traces and confirms a real caller chain:
  `CKernel::InitUserLayer() -> CConfigManager::SetupSysex()` (upgraded here
  from an empty Tier-B stub to real Tier A) `-> SysExApi->
  RegisterMessageClient()` (real virtual dispatch, confirmed via a direct
  `.rodata` vtable-slot byte read) `-> CSexServiceTask::
  RegisterMessageClient()` [out of scope] `-> CClientCommServer::
  CClientCommServer()`. `CSysExMsgTaskBase`'s own real caller
  (`CDumpTask::CDumpTask`, via the already-real `CModuleManager::Setup()`/
  `AddModule()` spine) confirmed the same way. New
  `include/client_comm_server.h`/`src/ipc/client_comm_server.cpp` (2 of 26
  real methods this pass: `ComputeCRCByte`/`CheckIncomingSexCRCByte`, a real
  XOR-fold checksum) and `include/sysex_msg_task_base.h`/`.cpp` (8 of 14
  methods this pass). Caught and fixed a real KAT-development bug: declaring
  the 3 ground-truth vtable-override methods `virtual` made the class
  genuinely polymorphic under the compiler's own vtable numbering, colliding
  with the manually-installed ground-truth-numbered fake vtable and
  segfaulting — fixed by dropping `virtual`, matching this project's
  established raw-`mVtbl` convention.
- **`CEvBuffersPool`/`CEvent`, unblocking 8 more `CClientCommServer`
  methods** (`d3f75ff`) — `CEvBuffersPool` is a two-tier fixed-chunk slab
  allocator (256×24B small pool, 128×136B medium pool, heap `new[]`
  fallback) with a refcounted chunk header and copy-on-write `Lock()`
  semantics, transcribed directly from `objdump -dr` (no Ghidra decompile
  needed — small and mechanical enough to read as raw asm). `CEvent` is the
  8-byte `{tag,buf}` handle every chunk is wrapped in. Together these
  unblock `CClientCommServer`'s ctor/dtor plus 6 more leaf methods
  (`SendMessageToClient`/`SendToSysExLink`/`RetryTXPacket`/`TXData`/
  `OnProcessRetry`/`OnRxMsgWhenInWAIT`) — **10 of 26 `CClientCommServer`
  methods now Tier A**, up from 2.

## Config / tempo

- **`CConfigManager`'s remaining `InitUserLayer()` steps + new `BPM`/`MPQN`**
  (`31cdbaf`) — `SetupRouting()`/`MakeConnections()`/`RegisterChunkServer()`/
  `LinkRTRouterTracks()`/`ConfigureSeqTimer()` upgraded from empty Tier-B
  stubs to Tier A. `SetupRouting()` turned out to be a genuinely empty
  1-byte `return;` in the real binary itself (confirmed, not assumed). The
  other 4 walk one of `CConfigManager`'s own static config tables
  (currently all real, non-null pointers to zero-initialized placeholder
  data), dispatching through a per-subsystem "Api" facade's raw vtable
  slots — real, live, boot-path-executed no-ops given today's zeroed table
  contents, not dead code. Surfaced and fixed a real divide-by-zero hazard —
  see "Real bugs" below. `CreateResourceFamilies` surveyed and deliberately
  deferred (a "CZ container" dependency, out of this batch's scope).
- **`CModuleManager`'s module-factory array + `CreateUserModules()`/
  `CreateFMDrivers()`** (`7d5bc26`) — `CConfigManager::CreateUserModules()`/
  `CreateFMDrivers()` (real factory-lookup + `Create()` dispatch) are real,
  safe no-ops on the current traced boot path given today's zero-initialized
  `sm_ptCreateInfo`/`sm_ptFMDriverInfo` placeholders. Found and fixed a real
  bug in the underlying dispatch mechanism — see "Real bugs" below.

## Editor / ES-family boundary

- **`CDataHandler`/`CEditServer` + `CESCommon`** (`1ea8836`) — a broad
  `nm -C` survey of `mains.cpp`'s remaining untraced `MMainXxx(CSystemApi*)`
  shims found that all **10** `CModule`+`CEditServer` "edit server" classes
  (`ESCommon`/`ESProg`/`ESEffect`/`ESCombi`/`ESGlobal`/`ESMOSS`/`ESSampling`/
  `ESSetList`/`ESSong`/`ESDisk`) share one identical shell shape — this
  disproved the batch's own working hypothesis that `ESCommon`/`ESGlobal`
  might be more "core" than the other 8 per-editor-page members. Each wraps
  a generic, shared descriptor-based Get/Set engine (`CDataHandler`/
  `CEditServer`) around a genuinely deep, out-of-scope `CXxxTask` god-object
  — 52 (`CESEffectTask`) to 1092 (`CESSongTask`) real methods per class, all
  10 counted via `nm -C`. Reconstructed the shared engine
  (`CDataHandler::AddDescriptors`/`FindDescriptor` ×2,
  `CEditServer::Get`/`Set`/`SetDefault`/`PutNotify`/`FindDescriptor`) plus
  `CESCommon` as the representative shell instance (`sizeof(CESCommon) ==
  0x40064`, byte-exact KAT-confirmed). Neither is reachable from the
  currently-wired boot path (construction is gated behind
  `CConfigManager::CreateUserModules()`, a confirmed real no-op given
  today's placeholder config table). Corrected two of the batch's own
  initial assumptions, both confirmed by KAT: `Get()` returns `1` (not `0`)
  for a not-found descriptor, and `Set()`'s return code is not a
  success/failure boolean at all (`0` covers both rejection and a
  successful no-notify set).

## Cleanup and reconciliation

- **Landing 2026-07-24's stranded boot-crash-chain fixes** (`19cf95f`) —
  commits work that had been live-boot-debugged in `kronos_vm` the day
  before but never landed (an earlier, now-compacted part of this session):
  `GetFMApiStub` at `Api`'s vtable slot `+0xa0`, and 7
  `PTR__CXxxApiInstance_*` globals resized from bare `void* = 0` to
  properly-sized 6-slot `EvaVTableStub` arrays. Both independently
  re-confirmed live (clean boot to "Start closing"/"End closing", zero
  segfaults) with the bisection debug-print scaffolding stripped back out.
- **`CKernel::Run()`/`Stop()` + `OmegaXxxThread` verification** (`ab9de48`,
  documentation-only) — confirms `CKernel::Run()`/`Stop()` don't exist as
  real symbols at all (likely conflated with the already-reconstructed
  `COmegaInterface::Run()`/`Stop()`); independently re-verifies all 3
  `OmegaXxxThread` bodies instruction-by-instruction against ground truth,
  no discrepancies found. Fixes two stale "not yet reconstructed" comments
  in `ckernel.h` that were already done.
- **Manifest/README reconciliation** (`a67ea92`, `abf285d`) — a README
  writeup for batch 3, and landing Stage-2 IPC manifest entries that got
  left uncommitted when two concurrent sessions touched the same generator
  file at the same time (both sets of entries independently correct and
  non-overlapping).

---

## Real bugs found and fixed this session

1. **Divide-by-zero hazard surfaced by wiring `ConfigureSeqTimer()` for
   real** (`31cdbaf`) — `BPM::SetLowerLimit()`/`SetUpperLimit()`
   (`tempo.cpp`) divide by their own `bpm` argument (`60000000 /
   (bpm & 0xffff)`) with **no zero-guard, confirmed present in the real
   disassembly itself** — this is genuine, faithfully-preserved ground-truth
   behavior, *not* patched. What was fixed is the caller side:
   `ConfigureSeqTimer()` unconditionally calls both setters, and given
   `config_info.cpp`'s usual all-zero placeholder-table convention, this
   would have divided by zero on every single boot. Fixed by giving the
   `SeqTimerInfo` placeholder sane, real-constant-matching non-zero BPM
   defaults (40/240) instead of the usual all-zero pattern — the hazard in
   `BPM` itself is left exactly as ground truth has it.
2. **`mModules`/`mTopologyChanged` permanently empty** (`aa8843e`, batch 3)
   — `CModuleManager::AddModule()`/`EnableUpdate()` were empty Tier-B stubs
   despite already having real boot-path callers (8 `MMainXxx` shims via
   `CSysApiInstance::AddModule()`; `InitSystemLayer()`'s direct
   `EnableUpdate(1)`), so the module list stayed stuck at its
   construction-time zero state indefinitely — the same "real caller, dead
   Tier-B stub" shape batch 1 had already hit with `CLevelManagerArray`.
   Making `AddModule()` real exposed two further, genuine crash risks that
   were fixed in the same commit: (a) 6 of `mains.cpp`'s derived-module
   vtable placeholders (`PTR__CEditMan_...` and siblings) were bare
   always-NULL scalar globals rather than properly-sized slot arrays —
   `AddModule()` now genuinely dispatches through every registered module's
   vtable, so this was upgraded to real `EvaVTableStub`-backed arrays; (b)
   `CFileMan`/`CResMan` had been modeled as independent stub classes with no
   `CModule` base, leaving their malloc'd buffer's "name" slot
   uninitialized — `AddModule()`'s real by-name scan dereferences that
   unconditionally once any other module is registered, a near-certain
   `strcmp(NULL, ...)` crash — fixed by deriving both from `CModule` with a
   placeholder name. Live `kronos_vm` re-boot reproduced the identical
   clean shutdown trace with the fixed chain genuinely exercised, zero
   crashes.
3. **`mConstructors` permanently empty via a dead vtable slot** (`7d5bc26`)
   — `Api`'s own vtable slot `+0x40` is the slot `mains.cpp`'s
   `RegisterModuleDescriptor()` dispatches all 15 real per-module
   descriptors through, and its real target is
   `CSysApiInstance::AddConstructor()` (confirmed by a direct byte read of
   the ground-truth vtable) — but that slot was still wired to the generic
   `EvaVTableStub` no-op, so none of those 15 descriptors ever actually
   reached `mConstructors`, even after `AddConstructor()`/
   `RemoveConstructor()` themselves were made real. Explicitly the same bug
   *class* as item 2 above (a Tier-B stub leaving a real array permanently
   empty), but this time the root cause was the dispatch slot itself, not
   the target method — fixed by wiring `PTR__CSysApiInstance_08e81008[16]`
   (byte offset `+0x40`) to the real forwarder instead of the generic no-op
   stub.

**Found but deliberately left as-is** (genuine ground-truth behavior, not a
reconstruction gap): `CCommDriver::setupfifoname()`'s real `strchr()` call
on each `argv` entry is dereferenced with **no NULL check** — any entry
without an `=` (including, plausibly, a bare `argv[0]`) segfaults in the real
binary itself (`94ad5fc`). Flagged, not "fixed," since Eva's real
launch-wrapper `argv` shape wasn't locatable in this project to confirm
whether it's actually hit in practice.

## What's still open

- **The 10 `CXxxTask` ES-family god-objects** (`CESCommonTask` through
  `CESSongTask`, 52–1092 real methods each) are the actual per-editor-page
  UI/model logic — deliberately out of scope, same "CForm/CSK-scale,
  indefinitely deferred" boundary as the Peg toolkit. Nothing on the traced
  boot path constructs any of them yet (gated behind
  `CConfigManager::CreateUserModules()`'s own placeholder config data).
- **6 of `CSTGUnsolMsgHandler`'s 30 methods** remain Tier-B link-stubs
  (`ControlMsgHandler`/`GlobalMsgHandler`/`CombiMsgHandler`/
  `ProgramSlotMsgHandler`/`ProgramMsgHandler`/`VoiceModelMsgHandler`) — all
  reach into genuinely deep `CControlSurface`/`CMMI`/`CModeManager`/
  `CStorage` algorithm-database state not modeled anywhere in this project.
- ~~**`CSysExMsgTaskBase`'s ctor `ECanTransmit==1` branch and
  `SendMsg`/`EventToMessage`/`MessageToEvent`**~~ — **DONE** (post-write): the
  `CSysExMsgClientOutLink`/`CSexServiceTask`/`CSysExMsgOutLink`/`COutLinkMono`/
  `COutLink` output-link subsystem was reconstructed (commit `7e1e5c1`),
  making `CSysExMsgTaskBase` a full 14/14 Tier A, and the `ECanTransmit==1`
  branch is now genuinely exercised by the reconstruction's own wired call
  graph for the first time (via the later `CDumpManMod` batch).
- **`CClientCommServer`'s remaining Tier-B methods** — the ctor's own
  output-link dependency above is now resolved; as of this doc's first
  writing 16/26 remained Tier B (the wire-transform methods
  `PrepareMsgBuffer`/`UnprepareBuffer`/`EventToMessage`/`MessageToEvent` and
  `Error()`'s de-duplication) — check current header comments for the latest
  count, more landed after this doc was written.
- **`CTask::RegisterIfc()`** — genuinely deep `TVector` growth dependency,
  deliberately deferred.
- **`CFileMan`/`CResMan`'s own real derived constructors** (malloc sizes
  0xa5c/0x21a0) — modeled only as thin `CModule`-deriving placeholders with
  a literal name, not their real ~2.6KB/8.6KB bring-up bodies.
- **`CreateResourceFamilies()`** and the rest of `CConfigManager`'s deeper
  bring-up methods beyond today's 5 — surveyed, deliberately deferred (a
  "CZ container" dependency, genuinely out of this batch's scope).
- **Manifest total (207 of 37,795)** is still a small, deliberately-scoped
  slice by design — today's breadth sweep intentionally follows real
  callers outward from the working boot path rather than attempting broad
  coverage.

Per-item detail, exact addresses, and header-comment-level reasoning for
everything above: see each subsystem's own header (`include/*.h`) — Eva has
no separate `HARDWARE_REVIEW_LOG.md`-equivalent file; open items are
documented inline at the point of use instead.
