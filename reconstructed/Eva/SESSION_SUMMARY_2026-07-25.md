# Eva Reconstruction — Session Summary (2026-07-25 through 2026-07-27)

**This document was rewritten from scratch on 2026-07-26 (HEAD `a5ff6aa`) to
correct stale numbers/claims that had accumulated across four separate
"Update" patches appended to the original 2026-07-25 write-up, and updated
again (not just appended) on 2026-07-27 (HEAD `786fcd5`) to fold in that
day's dynamic-investigation phase.** The original 2026-07-25 patches are
preserved in git history if you need the exact point-in-time wording; each
rewrite keeps the narrative of *what happened in what order* intact but
restates every count and claim against current ground truth. The title keeps
the original date because that's when this stream of work started and
because `HARDWARE_REVIEW_LOG.md`/other docs already link to this filename —
treat the date range in the heading above as the real scope.

**Headline numbers, independently verified for this update** (not taken from
any prior commit message): **559 of 37,795** functions reconstructed
(`manifest/gen_manifest.py`, regenerated fresh — `1.479%`), across **82
commits** touching `reconstructed/Eva/` (`git log --oneline -- reconstructed/Eva/
| wc -l`), spanning 2026-07-23 16:24 UTC through 2026-07-27 00:30 UTC (commit
`786fcd5`). The wider `kronosology` repo is at 291 commits total as of this
writing — check `git log --oneline | wc -l` for the current figure, it moves
daily. If you need a number not stated here, regenerate it — several of this
document's own predecessor drafts got a manifest count wrong by trusting a
stale commit-message figure instead of `gen_manifest.py`'s own fresh output.

---

## The headline correction: `s_bRunning` was never `1` — every prior "clean boot" was a near-instant exit, not a real run

This is the single most important correction in this rewrite, because it
invalidates the *interpretation* (not the code) of everything this project
believed about Eva's boot path from 2026-07-24 through most of 2026-07-25.

`omega_interface.cpp` had `volatile int s_bRunning = 0;`. Ground truth's own
`.data` section (not `.bss` — confirmed via `readelf -S`, a real compiled-in
non-zero initializer, not assumed-zero convention) has this symbol
(`s_bRunning@0x091ae7d0` in the original decompile addressing) initialized to
**`1`**, confirmed by a direct file-offset byte read (`01 00 00 00`), not
inferred. `main()` calls `Omega.Init(0)`, which spawns `OmegaInitThread` (the
thread that eventually drives `CKernel::InitUserLayer() ->
CConfigManager::CreateUserModules() -> CEditor::CEditor()`) and then calls
`OmegaTimingThread(0)` **directly on the calling thread**. `OmegaTimingThread`'s
very first statement is `if (s_bRunning == 0) return;`. With the wrong `= 0`,
this fired instantly on every single boot test this project had ever run,
`main()` fell straight through to `Omega.Close()`, and `OmegaInitThread` got at
most a few milliseconds of scheduling — nowhere near enough for
`CreateUserModules()`/`CEditor::CEditor()` to run.

**Consequence**: every "Eva boots end-to-end and reaches a clean natural
shutdown, zero crashes" claim in this project's history through 2026-07-25
(including this document's own original framing of Stage 4c/2026-07-24's
milestone) was observing this bug's near-instant exit, not a genuine
completed run. The individual crash fixes and function reconstructions from
that period are **not** invalidated — the bugs were real, the fixes were
real — only the "the process ran a real boot sequence and then shut down
cleanly" *interpretation* was wrong. Fixed: `volatile int s_bRunning = 1;`.
`s_timingenablelock`/`s_iTimingDisable` (the loop's other two gates) were
checked and confirmed unaffected — both genuinely live in `.bss`, correctly
zero-initialized, no fix needed.

**After the fix, the real terminal state is different and correct**: Eva's
main thread now legitimately blocks forever in `OmegaTimingThread`'s loop,
matching a real embedded GUI process that runs until hardware shutdown —
nothing in this reconstruction's traced call graph ever sets `s_bRunning`
back to 0. This was then independently confirmed with a genuine **300-second
(5-minute) continuous live-boot run** in `kronosvm` (`kill -0` against the
real kernel process table every 10s, same pid throughout, zero
segfault/oops/panic/abort matches across the full window) — the first time
this project ever observed Eva survive past its own old sub-1-second bug
window with direct, non-inferential process-liveness evidence. That same
session also surfaced two unrelated infra findings, neither a code bug: a
reproducible `FAST_RTAI` boot-order race (loadoa's marker-file check can run
before `/korg/rw`'s mount finishes; a retry always resolved it, not yet
fixed at the source) and a scratch-VM setup gap (an incomplete scratch-dir
copy that omitted `fakefb.ko`, which incidentally avoided the already-known
post-fakefb console-takeover stall rather than fixing it).

---

## `CEditor`'s vtable-dispatch gap: the `Setup()` fan-out is now genuinely live, not just reconstructed-but-dormant

A second, independent correction, discovered via the same rigor the
`s_bRunning` fix established (verify a *mechanism*, not just that a
constructor ran). `CModuleManager::Setup()`/`Config()`/`Start()` (already real)
raw-dispatch through each registered module's own per-instance vtable at a
fixed byte offset (`+8`/`+0xc`/`+0x10`) — this is a genuine, ground-truth
raw vtable call, not a stub. `CEditor::Setup()`/`Config()`/`Start()` were
themselves fully real, and `CConfigManager::CreateUserModules()` genuinely
constructs a `CEditor` into `mModules` — but `PTR__CEditor_08f29b88`'s own
Setup/Config/Start slots were still wired to the generic `EvaVTableStub`
no-op. That means `CEditor::CEditor()` firing (confirmed by the `s_bRunning`
fix's own live-boot test) is a **separate fact** from `CEditor::Setup()` ever
actually being dispatched — the ctor running does not imply `Setup()` ran.
This exact gap had already been found and fixed once before on the sibling
class `CPanel` (`PTR__CPanel_08f7c328`) but was explicitly left open for
`CEditor` at the time (different class, out of scope for that batch) — this
session closed it the same way: three real forwarders
(`CEditorSetupVSlot`/`CEditorConfigVSlot`/`CEditorStartVSlot`) wired into the
real slots (byte-verified against `.rodata` — slots 2/3/4 really are
`CEditor::Setup`/`Config`/`Start`, byte-identical shape to `CPanel`'s own
vtable).

Getting to a real live-boot confirmation of this fix surfaced a **second,
independent, previously-latent bug**: the first live boot to ever exercise
`CPoller::CPoller()` (unlocked earlier the same day by the `CPanel` batch)
segfaulted inside the ctor. Root cause: `Api`'s vtable slot `+0xac` (a
named-resource lookup) was still the generic `EvaVTableStub`, which leaves
garbage in `EAX` rather than a real return value — `CPoller::CPoller()`
unconditionally dereferences that return value's own vtable, so a real `Api`
object (not a KAT's hand-built fake, which always fills every slot
predictably) reliably hit undefined behavior. Fixed with a `LookupResourceStub`
that returns `NULL`, steering the ctor into its own well-tested "lookup
failed" fallback path. This is the **third** confirmed instance of this exact
bug class on this one 94-slot `Api` vtable (after `AddConstructorVSlot`/
`+0x40` and `GetFMApiStub`/`+0xa0`) — worth a standing note: this class of bug
only manifests when a caller both reaches the call site live *and*
unconditionally trusts the return value, and hand-built-fake-vtable KATs
structurally cannot catch it, only a real live boot can.

With both fixes in place, a real `CEDITOR_SETUP_MARKER` debug print (removed
after confirmation) fired in `eva_stdout.log` on a 170s+ continuous live
boot, and an audit of every other `CModule`-shape per-instance vtable
(`CEditMan`/`CMessagePort`/`CSeqTimer`/`CFileMan`/`CChunkMan`/`CDumpManMod`/
base `CModule`) confirmed all of them already had slots 2/3/4 wired
correctly — this gap was isolated to `CPanel`/`CEditor`, both now fixed. (Two
other classes, `CResMan`/`CSysEx`, still have stubbed Setup/Config slots, but
for the different, correctly-out-of-scope reason that their real methods
were never transcribed at all — not a "forgot to wire the vtable" gap.)

---

## The QEMU-native-gdbstub breakthrough: a real dynamic-debugging method, finally

Live-boot verification throughout this project had relied on printf-style
debug markers plus `guestfish --ro` readback of a running image's log files —
workable, but no way to actually halt execution and inspect state. Two
attempts at a proper interactive debugger were tried and are now superseded:

- **Guest-side static-musl `gdbserver` + SLIRP `hostfwd`**: connects, but the
  SLIRP NAT layer's idle-connection reaper resets the TCP connection at a
  hard ~74–75s wall-clock mark regardless of host load — confirmed
  repeatedly, not a fluke. **Abandon this approach for future work.**
- **QEMU's own built-in gdbstub** (the HMP monitor command `gdbserver
  tcp::PORT`, or `-s`/`-gdb tcp::PORT` at launch): this is a directly
  host-bound TCP listener owned by the QEMU process itself, with **no SLIRP
  connection-tracking layer in the path at all** — confirmed connecting in
  under a second and surviving well past the 74s wall with zero issue. This
  is a **whole-VM gdbstub** (debugs the emulated CPU, not a ptrace'd guest
  process) — for this project's purposes (breaking on a known virtual
  address inside a non-PIE `ET_EXEC` binary like Eva) this is simpler and
  more robust than the guest-side approach, and needs no VM restart or image
  changes on an already-running instance: attach to the existing monitor,
  issue `gdbserver tcp::1234`, then `gdb -ex "target remote host:1234"` from
  the host. Use `hbreak` (hardware breakpoint, linear-address compare), not
  software `break` — a software breakpoint patches memory via whichever page
  table the *currently scheduled* vCPU holds, which is wrong if the target
  process isn't the one running when the breakpoint is armed.

One important caveat discovered the hard way: launching the target process
**under** a guest-side gdbserver (so it's `PTRACE_TRACEME`'d and frozen at its
own entry point waiting for a resume that the broken SLIRP channel can never
deliver) defeats this entirely — the process stays frozen forever regardless
of how good the native-gdbstub connection is, since nothing ever sends it a
resume. The fix is to launch Eva directly, unwrapped, and attach the native
gdbstub afterward for inspection — not to combine the two mechanisms.

---

## The dynamic-investigation phase (2026-07-27): live tracing found a real bug the static sweeps never could

**This is the single most important methodological result to come out of this
document's 2026-07-27 update**, worth its own section rather than a bullet
buried in a list: the newly-proven QEMU-native-gdbstub method, pointed at a
live `kronosvm` boot, found a genuine, previously-undetected bug within one
session — after this project's own static call-graph/vtable sweeps had
*repeatedly, explicitly* declared the static-analysis well exhausted for
Eva. `PROJECT_BRAIN/status.md`'s own session-arc narrative names five
separate independent convergences on "genuinely exhausted" over 2026-07-26
alone (a "cross-validates the exhaustion" pass, a mop-up pass calling itself
"a third independent time," a "4th consecutive negative broad-sweep pass,"
and a final mop-up explicitly labeled "the 5th independent sweep in a row to
converge on exhaustion"). None of those five static passes — each of which
specifically re-audited per-instance vtables against the standing
`LESSON_vtable_dispatch_stub_gap` pattern — caught this one. A sixth pass,
this time *dynamic* rather than static, found it within a single gdbstub
session.

**The bug**: `CSysApiInstance`'s own per-instance vtable
(`PTR__CSysApiInstance_08e81008`) had slots 2/3/4/5 — the four
`CGlobalObjectBase`-inherited "phase hook" methods
(Pre/PostKernelConstructor, Pre/PostKernelDestructor) — left wired to the
generic no-op `EvaVTableStub`, even though `CKernel::CKernel(int)`'s real
`sm_poGlobalObjectList` bring-up loop genuinely raw-dispatches through those
exact slots on every single boot. This is the same underlying bug class as
the `CEditor`/`CPanel` vtable-dispatch gap described above (a per-instance
vtable slot silently no-op'ing a real caller despite the target method
itself being correctly known) — now confirmed as the **9th** instance of
this pattern across the two-day span, and the first found purely through
live tracing rather than a static audit. See
`.claude/agent-memory/re-decompiler/LESSON_vtable_dispatch_stub_gap.md` for
the full standing lesson and all 9 instances.

**A genuinely new wrinkle this instance surfaced**: `global_object_base.h`'s
own header comment already *documented*, from an earlier ground-truth
raw-byte read, that these exact 4 slots were confirmed to be
`CGlobalObjectBase`'s own no-ops — and even asserted "It's live data now,"
implying the wiring was believed complete. It was not: the vtable array
itself was never actually updated to match its own header's documented
finding. A correct, ground-truth-verified analysis written into a comment is
not a substitute for checking the actual array contents — worth flagging
for any future sweep, static or dynamic.

**Method note, also new**: the first attempt (a hardware breakpoint on
`EvaVTableStub`'s own entry address, then a normal gdb frame-pointer
backtrace to identify the caller) gave unreliable caller attribution — gdb
stops at the stub's very first instruction, before its own `push ebp; mov
ebp, esp` executes, so at that exact PC the frame-pointer chain still
belongs to the *caller's* caller, and different unwinder heuristics can
misattribute the true dispatch site. The fix was breaking instead on the
indirect `call eax` inside the generic `CallVSlot1`/`CallVSlot2` dispatch
helpers (whose own prologue has already run by that point) and reading the
true stack-pushed return address directly — unambiguous, and now the
recommended approach for any future gdbstub-based vtable trace on this
project.

**Fix**: `CGlobalObjectBase_Pre/PostKernelConstructor/Destructor` were
de-`static`'d and declared `extern "C"` so `omega_vtables.cpp` could wire
`PTR__CSysApiInstance_08e81008` slots 2–5 directly to them (reusing the
exact same function pointers, matching ground truth's inherited-vtable-slot
behavior). Slots 0/1 (the dtor pair) were deliberately left as
`EvaVTableStub` — ground truth shows `CSysApiInstance` has its own distinct,
still-unreconstructed destructor there, a separate, already-documented gap,
not this bug class. Rebuilt clean, full host `verify/` suite green (17/17
test binaries, 0 failures). Committed `e0758e2`. Live re-boot verification of
the *fixed* binary inside `kronosvm` is still outstanding — the fix was
confirmed via rebuild + host test suite + static ground-truth cross-check,
not yet via a fresh live trace of the corrected binary, since injecting a
freshly-built `Eva` into the existing baked VM disk image needs
loop-mounting infrastructure not set up this session.

This dynamic-investigation phase ran alongside (not instead of) one more
static pass: a fresh, from-scratch `objdump -dr -M intel` re-trace (not a
reread of prior notes) of `CSTGUnsolMsgHandler`'s last two deferred
handlers, specifically re-checking for the "size is not depth"
misdiagnosis this session had already caught 8 other times. See "IPC /
message substrate" below for the outcome (`VoiceModelMsgHandler` promoted to
Tier A, `ControlMsgHandler` reconfirmed genuinely deep).

---

## Scheduler / module family

The spine that makes the rest of this work reachable: getting `CScheduler`'s
per-tick dispatch and `CModuleManager`'s module bookkeeping from "real code
over permanently-empty arrays" to "real code over arrays that actually get
populated."

- **`CScheduler::Exec()` + `CLevelManagerArray::Add()`/`Find()`** — `Add()`
  appends via the real base `COmegaPtrArray` method then sifts the new
  element left while its own level number is smaller than its predecessor's.
  Previously both were always-empty/always-not-found stubs, making `Exec()`'s
  per-tick loop dead code even once transcribed.
- **`CModule`'s real vtable, `CTaskBuffer`, real `CLevelManager::RunLevel()`**
  — corrected a real mislabeling along the way: the level's task queue holds
  `TNamedPtrArray<CTask>`, not `TNamedPtrArray<CModule>`. `RunLevel()` now
  genuinely drains the level's `CTaskBuffer`, then walks its task queue,
  decrementing/reloading each unmasked task's countdown and dispatching
  `Exec()` on expiry.
- **`CModuleManager::AddModule()`/`EnableUpdate()`** — upgraded from empty
  Tier-B stubs to real by-name-scan/dedup-and-replace logic. Both have real
  boot-path callers, so `mModules`/`mTopologyChanged` had been stuck at their
  construction-time zero values indefinitely. Making `AddModule()` real
  exposed two further crash risks fixed in the same commit — see "Real bugs"
  below.
- **`CModule::AdjustTaskMask()`** — an 8×-unrolled reverse walk clearing bit
  `0x02` of each task's mask byte, the same gate bit `RunLevel()` checks.
- **`CTask::CTask()`/`CLimiterMan::CLimiterMan()`/`CModule::Add(CTask*)`** —
  corrected a stale "zero callers" verdict: `CTask::CTask()` genuinely is
  called, and `CModule::Add(CTask*)` (the actual `mTasks`-populating method)
  is boot-path-reachable via `CEditor::Setup() -> CModuleManager::Setup()`.
- **`CTask::SetMask()`/`~CTask()`** — `SetMask()` is `AdjustTaskMask()`'s
  trivial bit-0x01 sibling. `~CTask()` drains `mOutLinks` and destroys the
  embedded `CLimiterMan`, and identifies three further real base-class vtable
  identities off `.rodata`, confirming `CLimiterMan` IS-A `CIfcUnknown`.
- **`CModuleManager::AddConstructor()`/`RemoveConstructor()`** — the
  *second*, distinct `CModuleManager` registry (`mConstructors`, the "module
  factory array" `CreateUserModules()`/`CreateFMDrivers()` were waiting on).

## IPC / message substrate

- **`USTGUserAPI`/`CSTGHandle` close-out** — reconstructed the remaining 8
  real send/receive/teardown methods, plus `CSTGHandle::Release`/`GetSize` and
  `CSTGHandleCache::Cleanup`. Finds a 5th real device node (`/dev/rtf5`, the
  unsolicited-message channel) and a separate progress-reporting channel
  (`/proc/OmapNKS4ProgressBar`). Confirms zero call sites into any of 149 real
  Peg-prefixed GUI classes — the Peg toolkit substrate stays correctly
  un-started.
- **`CCommDriver::setupfifoname()`** — real per-`argv`-entry parser for the 3
  fifo paths, gated on `Eva_IsSimulation()`/`Eva_IsSimulationSVGA()`.
  Confirms `CCommDriver` is simulator-only on real hardware. Flags a real,
  faithfully-preserved ground-truth bug (see "Real bugs" below).
- **`CSTGUnsolMsgHandler`, a 30-method unsolicited-STGMessage dispatcher** —
  found via the same broad `nm -C` class-inventory sweep technique used
  earlier on OA.ko's `CSTGControlMsgHandler`. Worked in several batches
  across 2026-07-25/26/27; **29 of 30 methods are now Tier A**. The last
  three to close were `CombiMsgHandler`/`ProgramSlotMsgHandler`/
  `ProgramMsgHandler` (all turned out to be the same mechanical pattern
  already used elsewhere in the file, mislabeled rather than genuinely
  deep), and — finally — **`VoiceModelMsgHandler`** (Tier A batch 8,
  2026-07-27, commit `786fcd5`): a full from-scratch `objdump -dr -M intel`
  re-trace of the real 2512-byte function found it ~90% mechanical reuse of
  already-modeled `EditApi`/`Api`/`CStorage`/`SetWithoutUpdatingSTG`
  infrastructure, both real jump tables (17 + 6 entries) fully case-traced
  against `.rodata`. Exactly one leaf stays unimplemented within it: a
  `CStorage::GetInstance()`-based "MOSS algorithm" voice-model-database
  dispatch (real call site `0x08917209`, confirmed via a `.rodata` string
  naming `MOSSAlgorithmDatabase.h`) — an entirely unmodeled class hierarchy,
  precisely documented rather than guessed. Reconstructing it also caught
  and fixed a real bug: several per-case `GetScopeId()` calls had been
  hoisted above their own case's real bound check, which would have fired
  `GetScopeId()` even on real out-of-range bail paths — fixed by threading
  scope resolution through bool-returning `Compute*()` helpers matching
  real disassembly order case-by-case. **`CSTGUnsolMsgHandler` is now down
  to exactly one genuinely-deep handler: `ControlMsgHandler`** (real size
  corrected 4886B → 5152B, `0x0891ac70`–`0x0891c090`), reconfirmed via a
  fresh from-scratch re-trace the same day — a full call-target survey found
  18 distinct out-of-scope subsystems (`CMMI`, `CControlSurface`,
  `CHelpManager`, `CModeManager`, `CDiskUtil`, `CSmplModeMgr`, 4 real Peg
  `CForm` dialogs, raw `HAL_DisableInterrupts`/`HAL_EnableInterrupts`
  hardware interrupt-mask control) — not a misdiagnosis, correctly stays
  Tier-B with much more precise evidence than before.
- **`CClientCommServer`/`CSysExMsgTaskBase`** — **CClientCommServer now closed
  to a full 26/26** (2026-07-27 closeout pass): the last stub,
  `OnReceiveMessage(const CMessage&)`, turned out NOT genuinely blocked on a
  real `CMessage` definition after all -- the standing verdict had been based
  on the parameter type alone, never an actual disassembly. A from-scratch
  trace found it needs only 3 fixed `CMessage`-offset reads (the SAME
  opaque-fixed-offset convention already used elsewhere in this file/
  CPoller/CChunkServer) plus dispatch through the 3 already-real
  `OnRxMsgWhenIn{IDLE,SENT,WAIT}` siblings -- closing it also caught a real,
  previously-hidden bug: those 3 siblings' real return type is `int`
  (`CSexServiceTask::TransmitSysEx()`'s own value, propagated through), not
  `void` as previously committed, invisible until this was the first real
  caller to use the result. See `HARDWARE_REVIEW_LOG.md` for the full
  derivation. `CSysExMsgTaskBase`
  itself reached a full 14/14 Tier A once the `COutLink`/`COutLinkMono`/
  `CSysExMsgOutLink`/`CSysExMsgClientOutLink` output-link family was
  reconstructed. A recurring **6-FAIL** on `test_client_comm_server` was
  chased across three separate sessions before being conclusively closed: it
  was never a real regression, just cross-contamination from a *different*
  concurrent agent's own uncommitted WIP elsewhere in `src/` — every
  `verify/` binary links the entire object set, so anyone's mid-edit,
  not-yet-correct file can transiently break an unrelated test. The fix was
  landing both agents' commits cleanly, not touching `client_comm_server.cpp`
  at all. Worth remembering for any future "reproducible" test failure that
  doesn't touch the file you're working on: check `git status` for *your
  own* uncommitted changes too, not just other agents'.
- **`CEvBuffersPool`/`CEvent`** — a two-tier fixed-chunk slab allocator (256×24B
  small pool, 128×136B medium pool, heap fallback) with copy-on-write `Lock()`
  semantics, transcribed directly from `objdump -dr`.

## Config / tempo

- **`CConfigManager`'s remaining `InitUserLayer()` steps + new `BPM`/`MPQN`**
  — `SetupRouting()`/`MakeConnections()`/`RegisterChunkServer()`/
  `LinkRTRouterTracks()`/`ConfigureSeqTimer()` upgraded to Tier A.
  `SetupRouting()` is a genuinely empty 1-byte `return;` in the real binary.
  Surfaced and fixed a real divide-by-zero hazard — see "Real bugs" below.
- **`CModuleManager`'s module-factory array + `CreateUserModules()`/
  `CreateFMDrivers()`** — real factory-lookup + `Create()` dispatch. This is
  also where the *actual* root-cause bug behind `CEditor` never being
  constructed lived (see "Real bugs" below): `SetConfigInfo()` was pointing
  `CConfigManager::sm_ptCreateInfo` at the wrong symbol entirely.
- **`CDataHandler`/`CEditServer` + `CESCommon`** — a broad survey found all 10
  `CModule`+`CEditServer` "edit server" classes share one identical shell
  shape around a genuinely deep, out-of-scope `CXxxTask` god-object (52 to
  1092 real methods per class). Reconstructed the shared engine and `CESCommon`
  as the representative shell instance. Not on the currently-wired boot path
  (gated behind `CreateUserModules()`'s own placeholder config data).

## Editor / Panel / Poller / Task family

This is where the majority of 2026-07-26's work landed, following the
dependency graph outward from `CEditor::Setup()`'s own fan-out.

- **`CEditor`** — reconstructed as a real class (15 direct methods + ctor/dtor,
  real multiple-inheritance vtable cluster, new self-contained
  `CParameterString`). Its real construction (via `CConfigManager::
  CreateUserModules()`, once the `sm_ptCreateInfo` bug above was fixed) and
  its real `Setup()` dispatch (once the vtable gap above was fixed) are now
  both independently, live-boot confirmed — two separate facts, established
  in two separate sessions, each requiring its own fix.
- **`CPanel`** — reconstructed; the real ground-truth constructor+`Config()`
  call site for `CPoller` (`CPanel::Setup()` builds `CPoller` and registers it
  via the already-real `CModule::Add()`). Found and fixed the same
  `CModuleManager`-vtable-dispatch gap later found on `CEditor` (found here
  first, in fact — flagged for `CEditor` at the time, fixed there later the
  same day).
- **`CBatchDiskMan`** — unlocked via careful dependency-chain mapping (done
  *before* writing any code, not after): `CEditTask` is fully real; the
  disproportionately large `CBatchDiskMainTask` was initially a deliberate
  Tier-B substitute (its real ctor placement-constructs a `CZ` string-set
  container, this project's own long-standing out-of-scope boundary) but was
  later itself made real for the mechanical majority — its ctor turned out to
  be almost entirely subobject-ctor calls and literal field stores once
  4 small dependency classes (`CRMJob`, `CRMApiCallBack`, `CDirEntry`,
  `COutLinkMulti`) were added and `CZ` itself kept opaque. A follow-up pass
  re-checked all 4 new dependency classes for further promotable methods
  (none found — each remaining method is either unreachable or blocked by a
  genuinely deep, separate god-object) but found and fixed one real
  downstream gap: `CResMan`/the `RMApiInstance` global were mallocing a
  `CRMJob` without placement-constructing it, stale from when `CRMJob`'s own
  ctor was blocked on `CZ` — now real-constructed.
- **`CAlphaKeybCtrl`/`CAlphaKeybCtrlTask`** — fully reconstructed; the
  4289-byte ctor that looked like a scale-of-effort blocker turned out to be
  GCC inlining the same ~60-line "build one keyboard layout" sequence 15
  times (see "size is not depth" below). Found and preserved three genuine,
  faithfully-asymmetric bitmask bugs in `SetCtrlCondition()` (a sticky-key
  toggler for X/;/L/a: each key-up case reads one bit but clears a
  *different* bit — real ground truth, not a transcription artifact, though
  a first KAT draft did accidentally "fix" it before the KAT's own failure
  caught the mistake).
- **`CLocaleManager`/`CKeyboardLayoutManager`** — `AddKeyboardLayout()`/
  `GetKeyboardLayout()` made real, closing `CLocaleManager`. Real,
  worth-noting consequence: `CAlphaKeybCtrlTask::ProcessEvent()`'s own
  `GetKeyboardLayout(0x8409)` lookup was previously "faithful but quiescent"
  (always failed against a perpetually-empty list) and now genuinely
  succeeds once any `CAlphaKeybCtrlTask` has been constructed, since the
  layout list lives on `CLocaleManager`'s process-wide singleton. Same return
  value either way, but a real change in which code path executes.
- **`CLEDBlinker`** — a brand-new, self-contained singleton class (no vtable
  at all), 6 methods, all under 100 bytes: a 21-tick blink-phase divider and
  a 512-bit "currently blinking" registration bitmap, separate from
  `CPoller::mZeroBlock`'s own LED on/off display-state bitmap. Unlocked
  `CPoller`'s `MsgSetLed`/`MsgSetLed16bits`/`MsgBackupLEDs` handlers.
- **`CPoller`** — reconstructed across many sessions and, as of this
  rewrite, **fully, genuinely closed** — every method real, zero remaining
  Tier-B surface of its own. This took longer than expected because two
  separate "CPoller is fully closed" claims each turned out to have one more
  real gap:
  - `RegisterClient()` (2603 bytes, the largest single remaining piece) —
    reconstructed for real; found and preserved a genuine quirk where
    reusing an unconnected slot does *not* rebind it to a new name pair,
    meaning two back-to-back registrations with different names can both
    land on client handle 0.
  - `FindRegisteredClient()` and its two `MsgGetClientHandleByXxx()` wrappers
    — the wrappers looked like they needed their own ~2600 bytes of scan
    logic; ground truth actually inlines a full duplicate copy of the scan
    into each wrapper rather than sharing it, collapsing to simple calls once
    recognized.
  - `MsgSetAnalogClient()`/`MsgSetButtonClient()` — corrected a backwards
    field-identity guess (`mHandleTable1` is the 64-entry ANALOG table, not
    button/keyboard as previously guessed; `mHandleTable2` is the 128-entry
    BUTTON table).
  - `Exec()` (0-arg, 3213 bytes) — a real 12-way jump-table hardware-event
    drain loop, confirmed as `CLEDBlinker::Exec()`'s own real, single caller.
  - `Exec(CMessage&)` (6747 bytes) — **misdiagnosed once, then corrected**: an
    initial pass characterized this as "a genuine name-string command
    dispatcher, ~94 `strcmp()` call sites" based on a byte/call-count glance.
    A full CFG reachability walk (parsing every instruction, resolving every
    jump/call target, BFS from each of 15 jump-table entries) showed this was
    wrong: it's a 15-way jump table on `CMessage`'s own low command-code
    byte, and every one of the 15 cases is ground truth's own inlined
    duplicate of an already-real `Msg*()` sibling — the "~94 strcmp()"
    turned out to be two cases each inlining `FindRegisteredClient()`'s own
    scan a second time. Collapsed to 15 one-line wrapper calls plus one shared
    return-code translation function.
  - `InitButtons()`/`InitAnalogs()` — the **last** gap, found only after two
    separate "CPoller is fully closed" claims had already been made and were
    each wrong. Previously labeled "Tier-B, needs the unreconstructed
    `CMessage` machinery" purely from raw byte size (2925B/2919B); actually
    GCC re-inlining `RegisterClient()`'s own scan and then making a real,
    direct call to it for each populated `.rodata` name-pair table slot. See
    "genuine emergent behavior" below for what this exposed.

## The "size is not depth" recurring lesson

This exact misdiagnosis — a large raw byte count on a Ghidra function taken
as evidence of genuine algorithmic depth, without checking whether the bulk
is actually GCC inlining or unrolling something already understood — was
independently hit **at least four separate times** in this stream of work:
`CAlphaKeybCtrlTask`'s 4289-byte ctor (15× an inlined table-construction
pattern), `CPoller::FindRegisteredClient()`'s ~2600-byte wrapper pair (a
duplicated scan, not independent depth), `CPoller::Exec(CMessage&)`'s
"~94 strcmp() sites" (two inlined copies of an already-real scan), and
`CPoller::InitButtons()`/`InitAnalogs()` (the same duplicated-scan pattern a
third time). Each time, an `objdump -dr -M intel` register-tracing pass (or a
full instruction-level CFG walk, for the largest case) found the real shape
in well under the time a "genuinely deep, defer it" write-up would have cost.
**Standing rule for this project going forward**: any "Tier-B, too big,
needs unreconstructed machinery X" verdict resting purely on byte count
should get one more disassembly pass — specifically checking for a
duplicated call to an already-real sibling, or a table-driven construction
loop — before being trusted as genuine depth. The inverse instance also
happened once (`CBatchDiskMan`'s *small* constructor pulling in *large* new
dependencies) — size is simply not a reliable signal in either direction on
this binary.

## Genuine emergent behavior, verified not just argued

Because `RegisterClient()`'s own real Phase-2 logic reuses the first
still-*unconnected* `mClients` slot rather than constructing a new
`CIfcClient`, and neither `InitButtons()` nor `InitAnalogs()` ever connects
the client it just registered, running both functions back-to-back from a
fresh `CPoller` — matching `CPanel::Config()`'s own real, unconditional call
order — constructs **exactly one** real `CIfcClient` object, and *every one*
of the 78 populated button-table slots and 29 populated analog-table slots
resolves to that same single client handle (0). This is not a modeling
shortcut; it was verified empirically against real, unmocked
`RegisterClient()`/`CIfcClient` code in a host-side KAT before being written
up, and it is a real, hardware-relevant behavior worth carrying into
`HARDWARE_REVIEW_LOG.md` (see that file).

---

## Real bugs found and fixed this session

1. **`s_bRunning` mis-initialized (`= 0`, ground truth `= 1`)** — see the
   headline section above. The single highest-impact finding of this whole
   stream of work; invalidated the interpretation of every prior "clean
   boot" claim without invalidating the underlying fixes.
2. **`CModuleManager`/`CConfigManager` vtable-dispatch gaps on `CPanel` and
   `CEditor`** — both classes' real per-instance vtables left their
   Setup/Config/Start slots as the generic no-op stub despite
   `CModuleManager` genuinely raw-dispatching through those exact byte
   offsets for every registered module. Fixed with real forwarders on both;
   the `CEditor` fix additionally surfaced the `CPoller` ctor segfault below.
3. **`CPoller::CPoller()` segfault via `Api+0xac`** — the first live boot to
   ever exercise a real (not hand-built-fake) `Api` object through this
   vtable slot hit undefined behavior from an `EvaVTableStub`-left-garbage
   return value being dereferenced unconditionally. Third confirmed instance
   of this exact bug class on the same 94-slot `Api` vtable.
4. **`CConfigManager::sm_ptCreateInfo` pointed at the wrong symbol** —
   `SetConfigInfo()` wired the real module-factory table pointer at
   `s_tConfigInfo` (a small pointer table `SetConfigInfo()` itself reads
   *from*) instead of `s_atCreateInfo` (the real 14-entry module-factory
   table `CreateUserModules()` actually walks) — an all-zero stand-in made
   `CreateUserModules()`'s own `while (entry->name != 0)` false on the very
   first check, looking like "config legitimately empty" when it was really
   "pointed at the wrong address entirely." This is the actual root cause
   that, once fixed, let `CEditor::CEditor()` construct for the first time.
5. **Divide-by-zero hazard surfaced by wiring `ConfigureSeqTimer()` for
   real** — `BPM::SetLowerLimit()`/`SetUpperLimit()` divide by their own
   `bpm` argument with no zero-guard, confirmed present in the real
   disassembly itself (genuine, faithfully-preserved ground-truth behavior,
   not patched). What was fixed is the caller side: the `SeqTimerInfo`
   placeholder table was given sane non-zero BPM defaults instead of the
   usual all-zero convention, since it would otherwise divide by zero on
   every single boot.
6. **`mModules`/`mTopologyChanged` permanently empty** — `AddModule()`/
   `EnableUpdate()` had real boot-path callers but were empty stubs. Making
   `AddModule()` real exposed two further crash risks fixed in the same
   commit: undersized derived-module vtable placeholders (bare scalar
   globals instead of properly-sized slot arrays), and `CFileMan`/`CResMan`
   modeled with no `CModule` base, leaving their "name" slot uninitialized
   for a near-certain `strcmp(NULL, ...)` crash once any other module was
   registered.
7. **`mConstructors` permanently empty via a dead vtable slot** — `Api`'s
   own vtable slot `+0x40` (the real target for 15 module-descriptor
   registrations) was still wired to the generic no-op stub even after the
   registry methods themselves were made real.
8. **Undersized static buffer exposed by a Tier-B->Tier-A promotion** —
   `CChkApiInstance::SetOwnerModule()`'s real body writes 4 more bytes than
   its owning static buffer had been sized for (sized, at the time, for
   exactly what an empty stub needed). A live call would have silently
   overrun a 4-byte global by 4 bytes; caught by manually checking every
   sibling buffer's size against every field offset a newly-real function
   uses, before building — not by a crash.
9. **`CSysApiInstance`'s own vtable slots 2–5 left as generic stubs
   (2026-07-27, commit `e0758e2`)** — the 9th confirmed instance of the
   vtable-dispatch-stub-gap bug class (same family as item 2 above), and the
   first found via live gdbstub tracing rather than a static audit; see the
   dedicated "dynamic-investigation phase" section above and
   `.claude/agent-memory/re-decompiler/LESSON_vtable_dispatch_stub_gap.md`
   for the full pattern history across all 9 instances.

**Found but deliberately left as-is** (genuine ground-truth behavior, not a
reconstruction gap):
- `CCommDriver::setupfifoname()`'s real `strchr()` call on each `argv` entry
  has no NULL check — any entry without `=` segfaults in the real binary
  itself. Substantially de-risked (not fully closed) by a later finding that
  the real call site actually passes `envp`, not `argv` — POSIX-guaranteed
  `"NAME=VALUE"` shape — but Eva's real on-device launch wrapper's actual
  `envp` contents were never independently confirmed (see
  `HARDWARE_REVIEW_LOG.md`).
- `CAlphaKeybCtrlTask::SetCtrlCondition()`'s three asymmetric bit read/clear
  cases (see "Editor / Panel / Poller / Task family" above) — real,
  preserved, not homogenized into a "fixed" symmetric form.
- `CHIDDriver::GetKeyboardEvent()`'s `isKeyDown` computed from a stack byte
  `GetEvent()` never writes — reproduced as genuinely uninitialized, not
  deterministically zeroed (see `HARDWARE_REVIEW_LOG.md`).

## What's still open

- **The 10 `CXxxTask` ES-family god-objects** (`CESCommonTask` through
  `CESSongTask`, 52–1092 real methods each) remain deliberately out of
  scope — the actual per-editor-page UI/model logic, same "indefinitely
  deferred" boundary as the Peg toolkit. Not constructed anywhere on the
  currently-wired boot path.
- **1 of `CSTGUnsolMsgHandler`'s 30 methods** remains a Tier-B link-stub:
  `ControlMsgHandler` — reaches into 18 distinct out-of-scope
  `CMMI`/`CControlSurface`/`CHelpManager`/`CModeManager`/`CDiskUtil`/
  `CSmplModeMgr`/Peg-`CForm`/raw-HAL-interrupt-mask subsystems, re-checked
  multiple times (most recently a fresh from-scratch re-trace on
  2026-07-27) with no tractable angle found. `VoiceModelMsgHandler`, listed
  here as Tier-B in earlier versions of this document, was reconstructed for
  real on 2026-07-27 (Tier A batch 8, commit `786fcd5`) — see "IPC / message
  substrate" above.
- ~~**`CClientCommServer`'s one remaining method** (`OnReceiveMessage`) —
  genuinely blocked on a real `CMessage` definition.~~ **CLOSED 2026-07-27**:
  reconstructed for real, see "IPC / message substrate" above and
  `HARDWARE_REVIEW_LOG.md` — the class is now a full 26/26. Its own
  ground-truth constructor caller (`CSexServiceTask::RegisterMessageClient()`)
  remains a separate, deliberately out-of-scope dependency, so this fix is
  not yet live-boot exercised — verified via host KAT + disassembly
  cross-check instead.
- **`CBatchDiskMainTask`'s deeper `CZ`-driven business logic** (`PreloadDir`/
  `PreloadGroup`/`PrepareGroupsForPreload`/`AddItemToPreload`/
  `Exec(CMessage&)`) — the mechanical majority of the class is now real, but
  these 5 heaviest methods stay Tier-B, genuinely `CZ`-container-scale.
- **`CAlphaKeybCtrlTask`'s shared `COutLinkIfcBase`/`CMarshaller<T>`
  interface-link framework** — used by multiple other un-reconstructed
  interfaces too (`ILimiterNotify`, `IAlphaKeybEvent`, `IAlphaKeybCtrl`); a
  shared-framework batch, not a one-off.
- ~~**`CTask::RegisterIfc()`** — genuinely deep `TVector` growth dependency,
  deliberately deferred.~~ **STALE-CLAIM CORRECTION (2026-07-26 cross-check
  pass)**: this was reconstructed for real the same day (commit `9093cb1`,
  `CTask::RegisterIfc()` + `TVector<SRegisteredIfc,1>::MakeCapacity()`),
  before this file's own last rewrite (`bb515cc`) — the rewrite simply
  missed removing this line. See `include/task.h`'s own header comment
  ("NOW Tier A").
- **`CreateResourceFamilies()`** and the rest of `CConfigManager`'s deeper
  bring-up methods — surveyed, deliberately deferred (the same `CZ`
  dependency as above).
- **`CDataHandler`/`CEditServer`/the 10 `CESxxx` model classes** — real shell
  reconstructed, but not reachable from the currently-wired boot path
  (gated behind `CreateUserModules()`'s own placeholder config table
  content).
- **Manifest total (559 of 37,795, 1.479%)** remains a small,
  deliberately-scoped slice by design — this stream of work follows real
  callers outward from the working boot path rather than attempting broad
  coverage. Regenerate via `manifest/gen_manifest.py` for the current count;
  do not trust a cached figure from an old commit message.

Per-item detail, exact addresses, and header-comment-level reasoning for
everything above: see each subsystem's own header (`include/*.h`). Real
hardware-behavior uncertainty specifically (as opposed to scope/Tier-B
deferrals) is tracked separately in `HARDWARE_REVIEW_LOG.md`.
