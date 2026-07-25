# Eva — reconstructed source

Drop-in source reconstruction of the Korg Kronos `Eva` GUI/front-panel application
(`/korg/Eva/Eva`, within an encrypted loop-mount on the real device). Companion effort to
`reconstructed/OA/` (the STG synthesis engine kernel module), scoped very differently:
Eva is a normal dynamically-linked x86-32 userspace ELF (GCC 4.5.0, ~22 MB, 38,048
Ghidra-analyzed functions, not stripped), and this pass targets only its **boot path** —
getting the process to actually run in `kronos_vm` without crashing — not full UI
fidelity. See `PLAN.md` for the staged strategy, verification methodology, and the
concrete reasons this differs from OA.ko's approach (userspace ABI instead of kernel
freestanding ABI, no virtual-hardware-driver pattern needed, etc.).

## Repository layout

```
Eva/
  README.md                  this file
  PLAN.md                    staged reconstruction plan + verification methodology
  include/                   recovered type model (class structs -> headers)
  src/<subsystem>/            reconstructed .cpp, in staged order (boot path first)
  manifest/eva_functions.csv  per-function status, regenerated from the Ghidra export (gitignored, regenerable)
  verify/                     host-side known-answer test harness
```

## Reconstruction status

| Stage | Coverage |
|---|---|
| 0. Foundations | Done — source tree, plan, manifest generator, and linking-ABI toolchain (Lenny chroot, `tools/build_lenny.sh`) all in place and validated against the real on-image target libs |
| 1. Boot path | Traced and reconstructed: `_start`, `main`, `USTGUserAPI::Connect`, `USTGAPILCDControl::LoadStoredSettings`, `CCommDriver::getInstance` (both overloads), `COmegaInterface` ctor/dtor/`GetSysApi`/`ExitRequested`/`Init`/`Run`/`Stop`/`Close`, `CEditor::CPanelIfcTask::SetMargin`, `Ouch` (SIGINT handler) |
| 2. IPC/message substrate | Not started as its own stage, but `USTGUserAPI::SendSTGMessageWithSource`/`ConnectPanelFifo` and `CSTGHandle::Access` (the shared-memory attach primitive) landed as part of Stage 4 below |
| 3. CKernel/threading substrate | Done for `COmegaInterface::Init`'s own direct callees: `SetConfigInfo`, `Mains` + all 17 `MMainXxx` registration shims, `CKernel::CKernel`/`~CKernel`/`InitSystemLayer`/`GetSysApi`, the 3 `OmegaXxxThread` worker bodies. See "Stage 3" below |
| 4. Link-completion pass | **Done — reached a real, full link (`LINK OK`).** Every symbol Stage 3 left as a bare call-contract extern now has either a faithful (Tier A) or explicitly-stubbed (Tier B) real definition. See "Stage 4" below for the full breakdown and the tier convention |
| 4b. Api/SysApiInstance crash fix | **Done — 2026-07-23.** A live `kronos_vm` boot test (the first time the Stage-4 link was actually run) hit a NULL-pointer crash in `MMainEditMan()`: `Api` was never set. Root-caused and fixed — see "Api/SysApiInstance crash fix" below |
| 4c. Boot-path crash chain closed out | **Done — 2026-07-24. Eva now boots end-to-end in `kronos_vm` with zero crashes.** Two more real bugs found continuing the same live-boot iteration past 4b (undersized `PTR__CXxxApiInstance_*` vtable arrays; one consumed `Api` vtable slot returning garbage instead of a real object) — see "Boot-path crash chain closed out" below |
| 5. Peg toolkit substrate | Confirmed not necessary — Eva reaches its own natural shutdown (`Start closing`/`End closing`) and exits cleanly without it, per the 4c live boot |
| 6. Breadth sweep | **Batch 6 done — 2026-07-25.** `CScheduler::Exec()`/`CLevelManagerArray::Add()`/`Find()` (batch 1), `CModule`'s real vtable + `CTaskBuffer` + real `CLevelManager::RunLevel()` (batch 2), `CModuleManager::AddModule()`/`EnableUpdate()` (batch 3), `CCommDriver::setupfifoname()` (batch 4), `CModule::AdjustTaskMask()` (batch 5), and now `CSTGUnsolMsgHandler` — a whole 30-method unsolicited-STGMessage-dispatcher class found 100% unclaimed by a broad `nm -C` sweep, 18 methods reconstructed (batch 6 — see "Stage 6: breadth sweep, batch 6" below). 137 of 37,795 functions reconstructed — still a small, deliberately-scoped slice, not a broad sweep yet |

## Ground truth

Two copies of the real binary exist on this share, same size, **different MD5s, not
reconciled** — worth diffing before assuming they're substance-identical (a prior note in
`MASTER_REFERENCE.md` says `Eva`/`Eva.img` differ only by build timestamp across firmware
versions, which may or may not explain this particular pair):

| Path | MD5 |
|---|---|
| `/home/share/Decomp/EVA_Decomp/Eva` | `3f884f6f824ed3e8200ce07ae12c13ea` |
| `/home/share/ARCHIVE/Ignored/DecryptedImages/EVA_Extracted/Eva` | `be9fe1426020f695e293144aa81b7142` |

`readelf`-confirmed: `ET_EXEC`, entry `0x0804ca70`, interpreter `/lib/ld-linux.so.2`,
`NEEDED`: `libpthread.so.0`, `librt.so.1`, `libssl.so.6`, `libxml2.so.2`, `libz.so.1`,
`libuuid.so.1`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6`,
`libcrypto.so.6`.

A full static Ghidra decompile export already exists (no need to re-run analysis) at
`/home/share/Decomp/EVA_Decomp/eva_export/` — `functions/<name>@<addr>.c` (37,795 files),
`functions.csv`, `symbols.csv` (has the demangled `Class::method` names `functions.csv`
lacks — check both), `strings.csv`, `types.csv`. Same pattern as `oa_export/`
([[oa_ghidra_decomp_export]]).

## Boot path (Stage 1) — traced 2026-07-22

Unlike most of OA.ko's boot chain, this is genuinely simple to read: the real symbol
table is intact (not stripped) and the functions below are all small (11-807 bytes) and
live clustered in the low `0x0804xxxx`/`0x08e2xxxx`/`0x08e4xxxx`/`0x0824xxxx` address
range, not buried deep in the 22 MB image.

### `_start` (`.text+0x0804ca70`, 34 bytes)

Standard glibc CRT entry: `__libc_start_main(main, ...)`, infinite-loop trap after (never
reached in practice — `main` calls `exit()`/returns).

### `main` (`.text+0x0804cd50`, 500 bytes, `src/init/eva_main.cpp`)

1. Zeroes a 128-byte CPU-affinity mask, sets bit 2, calls `sched_setaffinity` — pins Eva
   to CPU 2 (`puts("Eva will run on CPU 2")`, a real printed string, not editorializing).
2. `USTGUserAPI::Connect()` — opens the RTAI FIFO / STG-direct IPC channel to `OA.ko` (see
   below). Return value is **not checked** in `main` — a real fire-and-forget call, same
   pattern OA.ko's own `init_module` mostly avoids (that one hard-fails on error); Eva
   apparently tolerates `Connect()` failing and carries on. Worth confirming this is
   really ignored rather than checked deeper inside `Connect()` itself once Stage 2
   reconstructs the FIFO read/write paths.
3. `USTGAPILCDControl::LoadStoredSettings()` — pulls LCD/panel settings out of the shared
   front-panel-status memory `Connect()` mapped, sends one `STGMessage` back
   (`SendSTGMessageWithSource`, type/subtype `0x1a`/`1`, not yet decoded further).
4. `fflush(stdout)`, then blocks `SIGTERM`(0xf)/`SIGCHLD`(0xe)/`SIGRTMIN+8`(0x28)
   (`pthread_sigmask`) — real signal numbers, not placeholders.
5. **App-mode detection from `argv[0]`'s basename**: `strrchr` for the last `/`, then a
   manual byte-compare loop (decompiler artifact of an inlined `strncmp`) against
   `"EvaSim"` (7 chars) and `"EvaSimSVGA"` (11 chars). Sets a global `s_eAppMode` (1 or 2)
   for the simulator builds; on real hardware (`argv[0]` basename is plain `Eva`, matches
   neither), instead calls `CEditor::CPanelIfcTask::SetMargin` four times (margins 0-3,
   values 10/12/5/7) — real touch-panel calibration margins, only set on the hardware
   path. **This means the reconstructed binary's `argv[0]` matters**: naming the staged
   VM binary anything other than `EvaSim`/`EvaSimSVGA`/`Eva` changes which of these three
   branches fires. Recommend staging it as literally `Eva` to match the real hardware
   path (the one we actually care about validating).
6. `CCommDriver::getInstance(argv)` — real constructor call the first time (`operator
   new(0x18)` then placement `CCommDriver(this, argv)`), caches to a static `singleton`.
   A **separate, zero-argument** `CCommDriver::getInstance()` overload exists elsewhere in
   the binary that does *not* construct — it's an assert-style accessor: if `singleton` is
   still null when it's called, it prints `"CCommDriver init error\n"` to stderr and calls
   `exit(1)`. That overload is not on `main`'s own call path but will matter for whatever
   later code calls the no-arg form before `main`'s `getInstance(argv)` has run — call
   ordering is load-bearing here, faithfully preserved rather than "helpfully" made safe.
7. `COmegaInterface::Init((COmegaInterface*)Omega, NULL)` (`puts("begin omega init")` /
   `"end omega init"` bracket it) — see below. This call **does not return until the app
   is exiting** in practice (it spawns the real worker threads and only returns after
   `OmegaTimingThread` itself returns, which per its own body is the actual event-loop
   driver — Stage 3 territory).
8. `signal(SIGINT, Ouch)` — installs the Ctrl-C handler *after* `Init()` returns, which
   given point 7 means realistically only in the shutdown window, not during normal
   operation. Preserved as found rather than "fixed" — may be intentional (SIGINT during
   the brief teardown window) or a genuine quirk; not resolved either way.
9. `COmegaInterface::Close()` (`puts("Start closing")` / `"End closing"` bracket it) —
   just sets `s_bRunning = 0`.
10. `return 0`.

### `USTGUserAPI::Connect` (`.text+0x08e27ea0`, 234 bytes, `src/ipc/ustg_user_api.cpp`)

```
mSharedMem = CSTGHandle::Access(&{1})          // shared-memory handle, arg 1 = some fixed id
mFrontPanelStatusAddress = CSTGHandle::Access(mSharedMem)
if (m_rt2userFifo == -1) m_rt2userFifo = open("/dev/rtf1", O_RDONLY)
if (m_user2rtFifo == -1) m_user2rtFifo = open("/dev/rtf0", O_WRONLY)
if (m_userRtDirect == -1) m_userRtDirect = open("/dev/dmsg0", O_RDWR)
m_activeUser2rtFD = m_user2rtFifo; m_activeRt2userFD = m_rt2userFifo
return (m_rt2userFifo != -1 && m_user2rtFifo != -1 && m_userRtDirect != -1)
```

Confirms Eva's IPC surface is exactly the three device nodes `init_module`'s
`stg_rtfifo_init` step creates (`reconstructed/OA/README.md`'s init-sequence step 11) plus
`/dev/dmsg0` (the `stg_direct` character device, [[eva_oa_ghidra_coordination]]'s "STG
direct messages" channel). All three already exist and work once OA.ko is loaded in
`kronos_vm` — **this is the concrete reason Eva's boot-path milestone was blocked on
OA.ko's own milestone first** (matches the original `MASTER_REFERENCE.md` §10.36 framing
recalled in `PLAN.md`). `CSTGHandle::Access` (shared-memory attach, presumably to
`/proc/.shm`, [[eva_oa_ghidra_coordination]]) is not yet reconstructed — Stage 2.

### `USTGAPILCDControl::LoadStoredSettings` (`.text+0x08e1dde0`, 176 bytes)

Reads 9 fields out of `USTGUserAPI::mFrontPanelStatusAddress + {0xc8..0xe4}` into module
globals, then sends one `STGMessage{type=0x1a, subtype=1, ...}` via
`USTGUserAPI::SendSTGMessageWithSource`. The exact semantic meaning of those 9 fields
(presumably touchscreen calibration / backlight / contrast, per the class name) is not
decoded — not needed for the boot-path milestone, would need `SendSTGMessageWithSource`
and `STGMessage`'s layout reconstructed first (Stage 2).

### `CCommDriver::getInstance` — two overloads (`.text+0x08e4f250` / `0x08e4f6e0`)

See point 6 above. `src/ipc/comm_driver.cpp` implements both faithfully, including the
zero-arg overload's `exit(1)` guard — not softened into a graceful return, since the real
binary genuinely aborts the process there and any caller of that overload was written
assuming `main`'s `getInstance(argv)` already ran.

### `COmegaInterface` (`.text+0x0804db60..0x0804e5a0`, `src/init/omega_interface.cpp`)

The "app kernel" bring-up class. Reconstructed so far, all real and small:

| Method | Address | Size | Behavior |
|---|---|---|---|
| `COmegaInterface()` | `0804e070` | 39 B | zeroes 5 fields (`this+0`,`+4`,`+8`,`+1c`,`+20`) |
| `~COmegaInterface()` | `0804db60` | 11 B | `s_bRunning = 0` |
| `GetSysApi()` | `0804e0a0` | 24 B | forwards to `CKernel::GetSysApi()` |
| `ExitRequested()` | `0804e0c0` | 34 B | `CKernel::GetSysApi()` then a **virtual call** through `*(*sysapi + 0x7c)` — Ghidra couldn't recover the jump table; the vtable-slot-0x7c target class isn't identified yet |
| `Init(int(*)(char const*))` | `0804e0f0` | 807 B | see below — the real bring-up |
| `Run()` | `0804e450` | 156 B | spinlock-protected decrement of a "timing disable" counter, returns `-1` unconditionally |
| `Stop()` | `0804e4f0` | 146 B | same spinlock, increments the counter, also returns `-1` unconditionally |
| `Close()` | `0804e590` | 11 B | `s_bRunning = 0` (same body as the destructor) |

`Init()` — the actual bring-up sequence, gated on `this[0] != 0` (idempotency guard):
prints `"create new kernel"`, allocates+constructs a `CKernel` (`this+8`), records a start
timestamp (`gettimeofday`), then spawns **6** `OmegaSchedulingThread` pthreads (ids 0-5,
each with its own `pthread_mutex_init`'d mutex, thread-info blocks are 16 bytes apart —
`s_tThreadInfo + 0x00/0x10/0x20/0x30/0x40/0x50`), stores the callback param at `this+0xc`
and a module-global `s_pfnSend`, then in order: `SetConfigInfo()`,
`CKernel::InitSystemLayer()`, `Mains()`, spawns one more thread
(`OmegaInitThread`), then calls `OmegaTimingThread(0)` **directly on the calling
thread** (not spawned) — this is why `Init()` doesn't return until shutdown: the timing
thread body is the real event loop. Every step is bracketed with a `puts()` — genuine
progress markers in the real binary, not something to add ourselves; useful as
dmesg-equivalent boot markers once this runs in the VM (`"create new kernel"` /
`"host buf init"` / `"set config info"` / `"init system layer"` / `"mains"` /
`"done with mains"` / `"create init thread"` / `"start timing thread"` /
`"done with omega init"`).

`Init()`'s own direct callees (`CKernel::CKernel`, `SetConfigInfo`, `CKernel::InitSystemLayer`,
`Mains`, the three `OmegaXxxThread` bodies) are now reconstructed — see "Stage 3" below.

### `CEditor::CPanelIfcTask::SetMargin` (`.text+0x0824cc40`, 19 bytes, `src/ui/panel_ifc_task.cpp`)

Trivial: `if (value < 0x32) touch_margin_table[which] = value;`. Bounds-checked write into
a 4-entry static byte table. The only Peg/UI-adjacent code needed for the boot path so
far — confirms Stage 4 (full Peg substrate) genuinely isn't needed yet, matching
`PLAN.md`'s expectation.

### `Ouch` (`.text+0x0804cd10`, 17 bytes) — SIGINT handler, `src/init/eva_main.cpp`

Trivial: sets a real latch global (`s_bIsFinished`) once, guarded so it only fires the
first time. Does no other work itself — some other code (not yet traced) presumably
polls the latch. *(Drive-by correction: this section previously said "not yet
reconstructed"; it already was, in the same `eva_main.cpp` pass as `main()` — stale doc
text, fixed while touching this file for Stage 3.)*

## Stage 3 — `COmegaInterface::Init()`'s direct callees — reconstructed 2026-07-22

All 6 groups from `Init()`'s call sequence (`SetConfigInfo`, `Mains` + its 17
`MMainXxx` registration shims, `CKernel::CKernel`/`~CKernel`/`InitSystemLayer`/
`GetSysApi`, the 3 `OmegaXxxThread` worker bodies) are now reconstructed. `Init()`
itself still doesn't link to a complete, runnable program — every group below pulls in
its own further Stage-4+ call-contract externs (real classes this pass declares but
does not implement) — but the shapes are all real, correctly-mangled C++ now, verified
by `make link`'s unresolved-symbol list containing only genuine Stage-4+ names (real
class/method signatures, real `PTR__ClassName_<addr>` vtable symbols, real
`s_poXxx`/`sm_poGlobalObjectList`/`SysApiInstance` globals) and nothing fabricated.

### `SetConfigInfo` (`.text+0x0804cb70`, 147 bytes, `src/init/config_info.cpp`)

Exactly as simple as it looked: 13 back-to-back assignments of `CConfigManager`'s
static table pointers (`include/config_manager.h`) to the real config-metadata tables
in rodata. One is a direct table (`sm_ptCreateInfo = s_tConfigInfo`, no indirection);
the other 12 go through a contiguous run of `PTR_s_atXxxInfo_<addr>` pointer variables
at `0x091ad9e4..0x091ada10` that Ghidra split into individually-named symbols but which
are really just a compiler-emitted array of table addresses. All 13 real target tables
(`s_tConfigInfo`, `s_atFMDriverInfo`, `s_atConnectInfo`, `s_atEditServerInfo`,
`s_atSysExModuleInfo`, `s_atSysExConnectInfo`, `s_atSysExFilterInfo`,
`s_atRTRouterInfo`, `s_atChunkInfo`, `s_atResFamilyInfo`, `s_tSeqTimerInfo`,
`s_ktVersionInfo`, `s_apkcSysVars`) are large config-metadata blobs, genuinely out of
scope for this pass — nothing on the traced boot path dereferences any of them.
Reconstructed as zero-initialized placeholder objects, sized from real address deltas
to the next confirmed symbol where that boundary was trustworthy (7 of 13), rounded
placeholders where it wasn't (6 of 13) — see `config_info.cpp`'s own per-table
comments for which is which. The assignment is real; the table *contents* are not.

### `Mains` (`.text+0x0804d9e0`, 365 bytes) + 17 `MMainXxx` shims (`src/init/mains.cpp`)

`Mains()` itself is exactly as described going in: fetches `COmegaInterface::GetSysApi()`
fresh before *every* one of the 17 calls (not cached once), then calls each `MMainXxx`
in the order PanelDriver → HIDDriver → AlphaKeybCtrl → LinuxDriver → Editor → Panel →
BatchDiskMan → ESCommon → ESProg → ESEffect → ESCombi → ESGlobal → ESMOSS → ESSampling
→ ESSetList → ESSong → ESDisk.

Reading all 17 real decompile files confirmed the predicted 2-pattern split:

- **15 are registration shims** over one shared private helper
  (`RegisterModuleDescriptor()`, matching `reconstructed/OA/README.md`'s
  `UpdateSongPunchMIDIChannel`-family "N thin wrappers over one private helper" style):
  build a 3-word `{vtbl, namePtr, reserved}` heap object, base-construct it with the
  generic `CNamedObjectBase` vtable, write in a name string, overwrite the vtable with
  the module's own real `PTR__CXxxConstructor` vtable, and register it through a
  `CSystemApi`-shaped object's vtable slot `+0x40`. Every one of the 17 real per-module
  name strings was decoded **by hand** from the packed dword/word/byte literal stores in
  each decompile (a GCC inlined-strcpy-of-a-literal artifact, replaced with a real
  `strcpy()` in the reconstruction — same license as `eva_main.cpp`'s inlined-strncmp
  replacement): `AlphaKeybCtrlClass`, `LinuxDriver`, `EditorClass`, `PanelClass`,
  `BatchDiskManClass`, `CommonEditServer`, `ProgEditServer`, `EffectEditServer`,
  `CombiEditServer`, `GlobalEditServer`, `MOSSEditServer`, `SamplingEditServer`,
  `SetListEditServer`, `SongEditServer`, `DiskEditServer`.

  Two real, confirmed-by-reading-every-one inconsistencies, preserved as found:
  - **Idempotency guard** (`if (Api == 0) Api = param_1;`) is present in 12 of the 15
    (`Editor`/`BatchDiskMan`/`ESCommon`/`ESProg`/`ESEffect`/`ESCombi`/`ESGlobal`/
    `ESMOSS`/`ESSampling`/`ESSetList`/`ESSong`/`ESDisk`) and absent in 3
    (`AlphaKeybCtrl`, `Panel`, and — in the non-descriptor group below —
    `PanelDriver`). Not load-bearing on this call path since `MMainPanelDriver` always
    runs first and unconditionally establishes `Api`.
  - **`MMainLinuxDriver` is the one real outlier**: it has the guard, but *also*
    independently, lazily fetches a second `CSystemApi`-shaped object (`FMApi`) through
    `Api`'s own vtable slot `+0xa0` (arg `DAT_0930b174`, a real but undecoded data
    constant), and registers its descriptor through **`FMApi`'s** vtable slot `+0x24`
    — not through `Api` at `+0x40` like every other descriptor-pattern function.

  The 15 real per-module "ModuleConstructor" vtables this ultimately installs
  (`PTR__CAlphaKeybCtrlConstructor_08eabb48` etc.) are declared as opaque extern data
  symbols — real, existing rodata symbols, installed byte-for-byte correct, but never
  reconstructed or dispatched through here. Per this batch's explicit instruction: one
  shared, empty, no-virtual-methods placeholder is used generically for "the base
  `CNamedObjectBase` shape every descriptor installs before being overwritten" — no
  attempt was made to fabricate any of the 15 real per-module vtables themselves, since
  a wrong one could crash or misbehave the moment Eva later dispatches through it.

- **2 are direct-construction shims** over a real, not-yet-reconstructed driver class,
  registered through vtable slot `+0xb4` instead of `+0x40`: `MMainPanelDriver`
  (`CLinuxPanelDriver`, no guard) and `MMainHIDDriver` (`CHIDDriver`, has the guard).
  Both driver classes' real `__thiscall` constructors are declared as call-contract
  externs (confirmed signatures from `functions.csv`) — not implemented.

### `CKernel::CKernel(int)` / `~CKernel()` (`.text+0x0805d4c0`/`0x0805d820`, 656/505
bytes, `include/ckernel.h`, `src/init/ckernel.cpp`)

Both fully, faithfully transcribed — including exact malloc sizes and every raw field
offset — following OA's own "partial reconstruction, gap documented" precedent only
where genuinely necessary: every class the constructor builds/tears down that Ghidra
never resolves a *named* method for (`CHostInterfaceBase`/`CHostInterface`, `CTracer`,
`CDummyMsgInput`) is left as a raw vtable-dispatched blob, since there's nothing to
reconstruct beyond the vtable install/call itself. Classes it *does* call named methods
on (`CScheduler`, `COmegaPtrArray`, `CErrorHandler`, `CModuleManager`,
`CConfigManager`, `CSysApiInstance`) are call-contract externs — real mangled symbols,
Stage 4+, not implemented.

`CKernel` itself is-a some unreconstructed `TVector<?>` template base (own vtable
install, `PTR__TVector_08e80c58`) — its actually-touched instance layout is just 0x10
bytes (vtable + 3 zeroed/freed ints).

Real quirk preserved: constructor parameter 0 (the branch `COmegaInterface::Init()`
actually takes, via `new CKernel(0)`) selects `PTR__CHostInterfaceBase_08e80b68`; a
nonzero parameter (not exercised on this path) selects `PTR__CHostInterface_08e80b08`
instead — the naming (`Base` for the "real" branch, non-`Base` for the alternate)
reads backwards from what you'd guess.

`sm_poGlobalObjectList` — a global registry of "auto-registering" objects both the
ctor and dtor walk (calling different vtable slots per phase: `+8`/`+0xc` in the
constructor, `+0x10`/`+0x14` in the destructor) — is confirmed `COmegaPtrArray*`-shaped
purely from usage (`+0xc` = count, `+0x14` = flat pointer array); its producer isn't
traced. Preserved as raw offset arithmetic rather than guessed struct fields, per this
project's own convention for structures confirmed only from usage.

The real disassembly brackets nearly every one of the ~12 `malloc`/`free` calls in
these two functions with `HAL_DisableInterrupts()`/`HAL_EnableInterrupts()` — same
kernel-side critical-section shim already dropped in `omega_interface.cpp`'s `Init()`;
dropped here too for the same reason, documented once rather than 12 times.

### `CKernel::InitSystemLayer()` / `GetSysApi()` (`.text+0x0805dba0`/`0x0805db90`, 326/6
bytes)

`GetSysApi()` — trivial, `return SysApiInstance;` — is the **real** one; not to be
confused with `COmegaInterface::GetSysApi()` (`0804e0a0`, already reconstructed in
Stage 1), which just forwards to this one. `SysApiInstance` is never written anywhere
in `CKernel`'s own code (ctor/dtor/`GetSysApi` all only read it) — whatever sets it is
not yet traced.

`InitSystemLayer()` is a flat sequence: 7× `CScheduler::InsertLevel(0..6)`,
`CConfigManager::AssignEditServerIDs()`, `MMainEditMan()`, `CModuleManager::Setup()`+
`Config()`, 7 more `MMainXxx()` system-layer inits (`Viewer`/`SeqTimer`/`FileMan`/
`SysEx`/`ChunkMan`/`RTRouter`/`DumpMan`/`ResMan`), `Setup()`+`Config()` again,
`CScheduler::Enable(1)`, `AdjustTaskMask()`+`Start()`. **Real finding worth flagging
clearly**: this is a *second, unrelated* `MMainXxx` family — all `void(void)`, no
`CSystemApi*` argument — sharing only the naming convention with `Mains()`'s 17-member
`MMainXxx(CSystemApi*, ...)` family above. Genuinely different functions; do not
conflate them. All 12 callees here (`CScheduler`, `CConfigManager`, `CModuleManager`,
the 9 `MMainXxx(void)` functions) are call-contract externs, Stage 4+.

### The 3 `OmegaXxxThread` worker bodies (`src/init/omega_threads.cpp`)

`OmegaSchedulingThread@0804db70` (401 B), `OmegaInitThread@0804dd10` (108 B, trivial —
pins CPU affinity, then one mutex-guarded `CKernel::InitUserLayer()` call), and
`OmegaTimingThread@0804dd80` (297 B, real return type **`void`**, not `undefined4` —
matches the boot path since it's called directly, not spawned via `pthread_create`).
All 3 open with the identical CPU-affinity-pin boilerplate already seen in `main()`.

**Real finding, confirmed by address arithmetic, not guessed**: the whole block of
globals these three (plus the already-reconstructed `Init()`) touch —
`s_timingenablelock` through `s_hOmegaTimingThread`, `0x09309474..0x093095e8` — is one
contiguous, correctly-sized run in `symbols.csv`, which resolved two real quirks with
certainty instead of a guess (see `include/omega_globals.h`'s own header comment for
the full arithmetic):
- `s_hThreads` is a real 6-element `pthread_t` array. Ghidra only names element 0
  `s_hThreads` and the other 5 `DAT_0930950{4,8,c}`/`DAT_09309510`/`DAT_09309514` (an
  array-recognition miss, not 6 separate variables) — visible in `OmegaExitThread`
  (see below), not itself reconstructed.
- `OmegaSchedulingThread`'s own reference to that same array
  (`*(pthread_t*)(s_tThreadInfo + s_iNesting*4 + 0x5c)`) is expressed relative to the
  *preceding* symbol `s_tThreadInfo` in that function specifically, purely because
  Ghidra picked a different nearby base symbol there — confirmed identical to
  `s_hThreads[s_iNesting - 1]` by address arithmetic (`s_tThreadInfo@093094a0 + 0x60 ==
  s_hThreads@09309500`, and `0x5c == 0x60 - 4`). Transcribed as the clean indexed form.

This also retroactively corrects a Stage-1 note: `Init()`'s own comment previously said
the 6 `pthread_create`d thread IDs were "not read back anywhere... collapsed to a
local" — false, now that this trio is reconstructed and reads them back through
`s_hThreads[]`. Fixed in `omega_interface.cpp` while touching it for this stage.

**`OmegaExitThread@0804deb0` (447 bytes) is explicitly out of scope, not an oversight**:
grepped for by name across all 37,795 exported function bodies and found with zero
callers anywhere in the binary — not reachable from the traced boot path, or as far as
this export shows, from anywhere at all. Per this batch's own "stay bounded" scoping
instruction, left unreconstructed.

## Stage 4 — link-completion pass — reached a real, full link (2026-07-22)

Starting point: `tools/build_lenny.sh` compiled cleanly but the real link against
`RestoreDVD_SystemMNT`'s on-image libs failed on ~50 unresolved symbols — every
Stage-3 call-contract extern (`CScheduler`, `COmegaPtrArray`, `CModuleManager`,
`CErrorHandler`, `CSysApiInstance`, `CSTGHandle::Access`,
`USTGUserAPI::SendSTGMessageWithSource`, `CCommDriver::CCommDriver`,
`CHIDDriver`/`CLinuxPanelDriver` ctors, `CKernel::Exec`/`InitUserLayer`, 10 real
`PTR__ClassName_<addr>` vtable symbols, a handful of `DAT_xxxxxxxx` constants, and the
9 `MMainXxx(void)` functions `CKernel::InitSystemLayer()` calls) plus `SysApiInstance`/
`sm_poGlobalObjectList` themselves. End state: **`LINK OK`** — a real, fully-linked ELF
at `/home/build/eva-toolchain/lenny-i386-root/tmp/eva_boot_test` (not executed anywhere,
per this pass's own scope — see PLAN.md).

### Tier convention (new this stage)

Every symbol on the unresolved list got a real definition — the alternative
(leaving them as bare `extern` declarations) cannot produce a working link, since the
linker needs an actual symbol, not just a compatible declaration. Each one is tagged,
in its own file's comments, as one of:

- **Tier A ("faithful")** — real logic transcribed from the Ghidra decompile, verified
  instruction-by-instruction (or, for the classic GCC unrolled-loop bodies, verified
  index-by-index against the unrolled form before collapsing to a plain loop — same
  license already established in Stage 1/3 for `main()`'s inlined `strncmp` and
  `LoadStoredSettings()`'s byte-compare loop).
- **Tier B ("link-stub")** — a real, correctly-mangled function/method signature with
  an empty or trivially-safe body. Exists only so the link succeeds; explicitly *not*
  a behavioral claim. Used only where the real body is genuinely out of this pass's
  scope (typically several hundred to ~1100 bytes deep, pulling in further
  not-reconstructed subsystems — `CModuleManager::AddModule` at 869 bytes,
  `CSysApiInstance::RegisterApi` at 1099 bytes, `CFileMan::CFileMan`/`CResMan::CResMan`
  at 0xa5c/0x21a0 malloc sizes, `CScheduler::Exec` at 1025 bytes, etc.).

Same discipline extends to the 10 real `PTR__ClassName_<addr>` vtable-slot symbols
Stage 3 already flagged as needing this treatment: each is now a real-sized array of a
single shared no-op stub (`EvaVTableStub`, cdecl, zero declared params — safe under
cdecl's caller-cleans-stack convention regardless of what a caller's own typedef
pushes), sized to the **real** vtable slot count read directly off `symbols.csv`'s own
vtable/typeinfo layout (`include/omega_vtables.h` documents the exact address-delta
arithmetic per class) — not a guess, not sized only to what this pass's own code
happens to dispatch through. 13 vtable symbols ended up needing this (the original 10
Stage 3 flagged, plus 3 more this pass's own new `CScheduler`/`CModule` reconstruction
pulled in: `CLevelManagerArray`, `CLevelManager`, `TNamedPtrArray<CTask>`).

### `COmegaPtrArray` — the foundational container (`include/omega_ptr_array.h`, `src/base/omega_ptr_array.cpp`)

Tier A in full: ctor (46B), `Destroy()` (224B), `FindIndex()` (227B),
`RemoveAtIndex()` (331B), `Shrink()` (356B) — all 5 real methods this pass needed.
Real layout: 0x18-byte object, `{vtbl, mUnknown04, mCapacity, mCount, mGrowBy,
mArray}`. This is the class every `TNamedPtrArray<T>`/`TPtrArray<T>` "flavor" seen
throughout the binary (`CModuleManager`'s 2 module lists, `CScheduler`'s level array,
`CModule`'s task array, `CSysApiInstance`'s driver/API lists, `CKernel`'s own
`sm_poGlobalObjectList`) really is underneath — a base-construct-then-vtable-swap
pattern, not real C++ inheritance, so `mVtbl` stays a plain raw `void*` rather than a
real compiler-managed vtable (same reasoning Stage 3 already established for
`CErrorHandler`/`CModuleManager`'s hand-built blobs). All 5 real bodies are the
classic GCC 4-or-8-way Duff's-device unroll over a flat array/list walk — collapsed to
plain loops here, semantics verified index-by-index against each real decompile.

### `CScheduler` (`include/scheduler.h`, `src/base/scheduler.cpp`)

Ctor/`InsertLevel()`/`Enable()` are Tier A. Real fields resolved with confidence:
`+0x1c` is genuinely "`mEnabled`" (`InsertLevel()` saves/zeroes/restores it around its
own critical section; `Enable(int)` is almost literally `mEnabled = enable`) even
though it kept its original Stage-3-era placeholder name internally.
`CLevelManagerArray::Find()`/`Add()` (258/522 bytes) and
`CSysApiInstance::WriteMessageToHost(int,int)` (64 bytes) are Tier B — but `Find()`'s
stub "always not found" answer is coincidentally exactly correct for this pass's own
data (nothing else populates the level array), so `InsertLevel()`'s real "build a new
`CLevelManager` and insert it" path still gets faithfully exercised even though `Add()`
itself is a stub no-op. `CScheduler::Exec()` (1025 bytes — the real per-tick task
dispatch loop) is Tier B.

### `CModuleManager` / `CModule` (`include/module_manager.h`, `include/module.h`, `src/base/module_manager.cpp`, `src/base/module.cpp`)

`Setup()`/`Config()`/`AdjustTaskMask()`/`Start()` (510/505/620/845 bytes) are Tier A —
another 4-way-unrolled array walk, collapsed to clean per-index loops. Real per-module
lifecycle gate confirmed: a single `int` field at module-object `+0x24` (0=constructed,
1=setup, 2=configured, 3=task-mask adjusted, 4=started), read/written by
`CModuleManager`'s own methods via raw offset (not through `CModule`, which never
exposes it — `CModuleManager` treats modules as opaque blobs, matching the real
decompile's own style). **Real, worth-flagging inconsistency preserved as found**:
`AdjustTaskMask()`'s own disassembly writes its "phase" scratch field (`+0x38`) to `1`,
not `3` — the same value `Setup()` writes, even though `AdjustTaskMask()` is a
logically distinct phase; `Config()`/`Start()` write `2`/`3` correctly for their own
phases. Not "fixed" into a consistent `3`. `CModule::CModule(const char*)` (156 bytes)
is Tier A and resolves a real detail: the ctor's own `+0x28` field is the return value
of a virtual call through `Api`'s vtable slot `+0x3c` — some kind of scope/task-level
id, meaning still not decoded, but the mechanism now is. `CModuleManager::AddModule()`
(869 bytes) and `EnableUpdate()`, `CModule::AdjustTaskMask()` (458 bytes) are Tier B.
`CSysApiInstance::AddModule()` (`src/base/sysapi_instance.cpp`, 22 bytes) is a real
Tier A thiscall forwarder straight to `CModuleManager::AddModule()`.

### `CErrorHandler` / `CSysApiInstance` (`include/error_handler.h`, `include/sysapi_instance.h`, `src/base/*.cpp`)

`~CErrorHandler()` (175 bytes) is Tier A — a real singly-linked-list walk (not a flat
array, unlike everything else in this stage), dispatching each node's own vtable slot
`+4` (deleting destructor); a real no-op given this pass's construction (list head
starts null, nothing populates it). `CSysApiInstance::Cleanup()` (497 bytes) is Tier
A — pops-and-frees from 2 embedded `COmegaPtrArray`s (driver list, API-descriptor
list) by repeatedly taking the *last* element, dispatching its own sub-object's vtable
slot `+0x1c` ("uninit"), then removing it — self-contained now that `COmegaPtrArray`
is reconstructed. `EnableMultiTask()`, `WriteMessageToHost(int,int)`, `RegisterApi()`
(22/64/1099 bytes) are Tier B — `RegisterApi()` in particular is genuinely deep (a real
named-API registry, out of scope). Both `CErrorHandler::EnableUpdate()` and
`CScheduler`/`CModuleManager::EnableUpdate()` are Tier B stubs `CKernel::InitUserLayer()`
calls (see below).

### `CConfigManager::AssignEditServerIDs()` + 9 deferred bring-up steps (`src/init/config_manager.cpp`)

`AssignEditServerIDs()` (334 bytes) is Tier A, including its real per-entry loop body
(7 packed `{name, scope}` pairs per row via `CEditApiInstance::AssignScope()`,
Tier B) — **but that loop body is real, dead code given this pass's own data**:
`SetConfigInfo()` (Stage 3, `config_info.cpp`) points `sm_ptEditServerInfo` at a
zero-initialized placeholder table, so the first entry's name is already null and the
loop guard exits before ever calling `AssignScope()`. Transcribed anyway rather than
short-circuited, same license as `LoadStoredSettings()`'s dead `local_10` read (Stage
1). The other 9 real `CConfigManager` methods `CKernel::InitUserLayer()` calls
(`ConfigureSeqTimer`/`CreateResourceFamilies`/`CreateUserModules`/`CreateFMDrivers`/
`SetupRouting`/`LinkRTRouterTracks`/`SetupSysex`/`MakeConnections`/`RegisterChunkServer`)
are Tier B — not individually looked up in the decompile export, genuinely out of
scope (each is presumably its own substantial per-subsystem bring-up, matching the
scale of `CModuleManager`'s own methods).

### `CKernel::Exec()` / `InitUserLayer()` (`src/init/ckernel.cpp`)

Both Tier A in their own control flow. **Real finding, corrects a Stage-3 note**:
`CKernel::Exec()` (169 bytes) resolves what `CKernel`'s own `+0x04`/`+0x08` fields
actually are — not an opaque "TVector-owned heap block" as Stage 3's own comment
speculated, but the begin/end pointers of `CKernel`'s embedded
`TVector<CTimerObject*,1>` base (matching `PTR__TVector_08e80c58`'s real demangled
name in `symbols.csv`) — `Exec()` walks `[this+4, this+8)` as a flat `CTimerObject*`
array, checking each timer's elapsed time against its interval and firing a stored
callback. Real and transcribed faithfully, but dead code given this pass's own
construction (the ctor zeroes both fields and nothing populates the vector).
`CScheduler::Exec()` (Tier B) and the `g_poHostInterface` vtable-`+8` poll are the
rest of the function. `InitUserLayer()` (273 bytes) is a flat 13-call sequence with no
control flow of its own — transcribed as a real call list; every one of its 13
callees (9 `CConfigManager` methods, `CModuleManager`/`CScheduler`/`CTracer`/
`CErrorHandler::EnableUpdate`) is Tier B.

### `CSTGHandle::Access()` (`include/eva_types.h`, `src/ipc/stg_handle.cpp`)

Tier A, 356 bytes — **the one piece of this stage most worth trusting**, since it's
Eva's actual shared-memory attach primitive (`USTGUserAPI::Connect()`'s own call
target). Confirms the real mechanism end to end: opens `/proc/.shm`, two `ioctl()`s
(`100`, `0x65`) against the handle's own mode/id field to get an offset and a size,
then `mmap()`s (flags `0x2001` — `MAP_SHARED | 0x2000`, preserved as the literal real
value rather than normalized) at the page-aligned offset. Results are cached per
mode/id, refcounted, in a real fixed 1.2MB table (`CSTGHandleCache::sCachedHandleInfo`,
100000 entries × 12 bytes, `CSTGHandleCache::Initialize()` also Tier A at 64 bytes) —
so repeat `Access()` calls for the same id are cheap. This is genuinely the "how" behind
[[eva_oa_ghidra_coordination]]'s shared-memory picture, not previously confirmed.

### `USTGUserAPI::SendSTGMessageWithSource()` / `ConnectPanelFifo()` (`src/ipc/ustg_user_api.cpp`)

Both Tier A (275/58 bytes). **Real finding that corrects Stage 1's own STGMessage
documentation**: `SendSTGMessageWithSource()` reads the message's first `u16` field as
a **total byte length** for its `write()` loop, not a "type" tag — meaning
`LoadStoredSettings()`'s own `msg.type = 0x10` (Stage 1) is actually setting the
message's byte length (16, exactly the size of the 4-field local shape it builds), not
a message-type code. `ustg_user_api.h`'s `STGMessage` doc comment is updated to
reflect this — the field name itself isn't renamed (the struct is still opaque/
unrecovered beyond this one fact). Real retry-on-short-write loop, `syslog()`-based
error reporting (with the exact real format strings and the real "normal" vs
"download" fd-kind label logic) preserved faithfully. `ConnectPanelFifo()` opens
`/dev/rtf7` — a **fourth** real device node beyond the 3 `Connect()` already uses,
called from `CLinuxPanelDriver`'s own ctor (below), not from `main()`'s own path.

### `CCommDriver::CCommDriver(char**)` (`src/ipc/comm_driver.cpp`)

Tier A, 242 bytes — opens up to 3 fifos (LCD/Command/Event) from paths
`setupfifoname()` (Tier B, 681 bytes) is supposed to fill in. Since the stub leaves
all 3 paths null, and the real ctor already null-checks each before `open()`ing, this
becomes a real, faithfully-derived no-op under that specific (stubbed) input — not a
fabricated safety check.

### `CHIDDriver` / `CLinuxPanelDriver` constructors (`src/init/mains.cpp`)

Both Tier A (132/91 bytes) — same shape as `CModule::CModule()`: strdup the name,
vtable-swap from the generic `CNamedObjectBase` base to the driver's own real vtable
(both opaque Tier B data, `PTR__CHIDDriver_08fd9ce8`/`PTR__CLinuxPanelDriver_08fd9dc8`).
Real, worth noting: `CHIDDriver`'s own ctor **never touches its `eventsName`/
`commandsName` parameters** — only the name is used in this 132-byte body; those two
presumably matter to a later method (`Open()`?), not construction. `CLinuxPanelDriver`'s
ctor calls `USTGUserAPI::ConnectPanelFifo()` as its very last step.

### The `CKernel::InitSystemLayer()`-adjacent `MMainXxx(void)` family — investigated, confirmed deeper than Stage 3's family (`src/init/mains.cpp`)

Per this pass's own explicit instruction not to assume: checked all 9
(`MMainEditMan`/`MMainViewer`/`MMainSeqTimer`/`MMainFileMan`/`MMainSysEx`/
`MMainChunkMan`/`MMainRTRouter`/`MMainDumpMan`/`MMainResMan`) by reading each real
decompile. **Verdict: this is a third, distinct `MMainXxx` shape** — not the cheap
17-member `CSystemApi`-registration shim family from Stage 3, and not
`InitSystemLayer()`'s own other 12 callees either. Each (all 45–127 bytes, so the
*wrapper* itself is cheap) does up to 3 real things: (1) registers a named sub-API
through `Api`'s vtable slot `+0xa4` — a dispatch slot not used anywhere else in this
reconstruction — except `MMainSysEx`, which calls `CSysApiInstance::RegisterApi()` by
name instead (the one real outlier, mirroring `MMainLinuxDriver`'s outlier status in
Stage 3's family), and `MMainRTRouter`, which does *only* this step (the smallest of
the 9, 45 bytes, no module construction at all); (2) for the other 7: either
base-constructs a `CModule` and vtable-swaps in a real per-subsystem vtable (5 of
them — `CEditMan`/`CViewBase`+`CMessagePort`/`CSeqTimer`/`CSysEx`-module/`CChunkMan`/
`CDumpManMod`, no real derived ctor ever actually called, same manual-vtable-swap
idiom as everywhere else in this project), or calls a **real, distinct derived-class
constructor** for the 2 biggest subsystems — `CFileMan::CFileMan()` (`malloc(0xa5c)` —
2652 bytes) and `CResMan::CResMan()` (`malloc(0x21a0)` — 8608 bytes) — both genuinely
Stage 5/6 depth, Tier B; (3) registers the new module via `CSysApiInstance::AddModule()`
(real, Tier A); 3 of the 9 (`MMainSysEx`/`MMainChunkMan`/`MMainResMan`) additionally
hand the module to its own API-instance object via a named setter
(`CChkApiInstance::SetOwnerModule()`/`CRMApiInstance::SetResMan()`, both Tier B). All 9
wrapper functions themselves are Tier A; the 8 real per-subsystem API-instance globals
(`EditApiInstance`, `SeqApiInstance`, `FMApiInstance`, `g_oSysExApiInstance`,
`ChkApiInstance`, `DumpApiInstance`, `RMApiInstance`, `RTRouterApiInstance`) are opaque
zero-initialized placeholders, sized only to the one real byte offset each function
actually writes into them (e.g. `FMApiInstance+0x4d8`) — contents otherwise
unfaithful, same convention as `config_info.cpp`'s own config-table placeholders.

## Linking / build-ABI status

**Root-caused 2026-07-22, fix in progress.** Confirmed real on-image shared libraries
already exist locally at `/home/share/RestoreDVD_SystemMNT/mnt/lib/` (an extracted real
Kronos rootfs) — all 12 of Eva's `NEEDED` libs present: `libc.so.6` (glibc **2.7** exactly,
confirmed via its own `GNU C Library stable release version 2.7` string and its ELF note
`for GNU/Linux 2.6.9`), `libstdc++.so.6` (exports up to `GLIBCXX_3.4.14`, consistent with
GCC ~4.3-4.5 era, matching Eva's own confirmed GCC 4.5.0), `libpthread.so.0`, `librt.so.1`,
`libssl.so.6`, `libxml2.so.2`, `libz.so.1`, `libuuid.so.1`, `libm.so.6`, `libgcc_s.so.1`,
`libcrypto.so.6`, `ld-linux.so.2`.

**But this host's own toolchain cannot target that ABI.** Confirmed by a direct test: a
trivial `-m32` compile with this host's g++ 12.2 (Debian 12, glibc 2.34) produces an object
requiring `__libc_start_main@GLIBC_2.34` — glibc 2.7 doesn't export that symbol version at
all, so linking (even just against the *real* on-image `libc.so.6`) fails outright. This is
architecturally the same class of problem the project's `musl-i386` toolchain
([[kronos_i386_musl_toolchain]]) was built to route around for *statically-linked* new
tools — but Eva genuinely needs real dynamic linking against `libssl`/`libxml2`/`libstdc++`
etc., so a musl-static build isn't the right fix here (musl binaries can't safely
dynamic-link against glibc-built C++ `.so`s in general).

**RESOLVED 2026-07-22.** Debootstrapped a **Debian Lenny (5.0) i386 chroot**
(`/home/build/eva-toolchain/lenny-i386-root`, `g++ 4.3.2`, `libc6-dev 2.7-18lenny7`) —
Lenny shipped glibc **2.7 exactly**, period-matching the real target. A trivial test
compile+link+run inside the chroot required only `GLIBC_2.0`/`GLIBCXX_3.4` — safely under
the real target's 2.7 ceiling (confirmed the opposite failure mode does NOT occur: old
toolchain targeting old ABI, not new toolchain targeting old ABI). Then, decisively: **the
actual reconstructed Eva object files were linked against the REAL, on-image
`RestoreDVD_SystemMNT` shared libraries (not the chroot's own) and produced ZERO
unresolved libc/libstdc++/OS-level symbols** — every remaining unresolved reference is one
of Eva's own not-yet-reconstructed internal classes (`CScheduler`, `COmegaPtrArray`,
`CHIDDriver`, `CLinuxPanelDriver`, several `PTR__ClassName_<addr>` vtable stand-ins,
`sm_poGlobalObjectList`, `sched_sig_handler`, `CKernel::Exec`/`InitUserLayer`) — exactly
the expected Stage-4 gap, nothing toolchain-related left to solve.

**Reproducible build**: `tools/build_lenny.sh` — mounts the chroot's `/proc`/`/sys`/`/dev`
plus bind-mounts this directory and the real target libs into the chroot, runs
`make objs` with the chroot's own `g++`, then attempts the real link. One-time chroot setup
(debootstrap + apt-get install) is documented in the script's own header comment. Uses
`--no-check-gpg`/`[trusted=yes]` since Lenny's archive signing keys predate this host's
default keyring and Lenny is long EOL (only reachable via `archive.debian.org`).

**LINK OK — 2026-07-22, Stage 4.** Every symbol on the "Stage-4 gap" list above (and
every further Stage-4+ extern Stage 3's own reconstruction pulled in) now has a real
definition — faithful (Tier A) where tractable, an explicit real-signature link-stub
(Tier B) where genuinely out of this pass's scope — and `tools/build_lenny.sh`'s real
link against the on-image `RestoreDVD_SystemMNT` libs reports **`LINK OK`** with zero
remaining unresolved symbols of any kind. Resulting binary:
`/home/build/eva-toolchain/lenny-i386-root/tmp/eva_boot_test` (133835 bytes, ELF
32-bit LSB executable, dynamically linked, interpreter `/lib/ld-linux.so.2`, `for
GNU/Linux 2.6.8`, not stripped) — **not executed anywhere as part of this pass**, per
its own explicit scope boundary; running it in `kronos_vm` is later, separate work.
See "Stage 4" above for the full per-symbol breakdown and the Tier A/B convention.

## Api/SysApiInstance crash fix — 2026-07-23

Stage 4's `LINK OK` binary was, for the first time, actually boot-tested in a live
`kronos_vm`. It ran genuinely deep — past `main()`, `USTGUserAPI::Connect()`,
`COmegaInterface::Init()`'s 6 scheduler threads, `SetConfigInfo()`, into
`CKernel::InitSystemLayer()` — then segfaulted on a NULL-pointer dereference in
`MMainEditMan()` (`src/init/mains.cpp`): `Api` (`CSystemApi *Api = 0;`) was still null.
Confirmed against the real decompile (`functions/MMainEditMan@080d2a00.c`) that the
crash is real and faithful — the original binary dereferences `*Api` with no null
check too, so this was never a "we made something up" bug, just an unfinished
dependency chain: nothing in this reconstruction ever wrote to `Api`.

### The real mechanism (traced end to end, all four pieces ground-truth-confirmed)

`Api`'s real producer turned out to be a chain of ordinary C++ static/global
constructors — machinery every userspace ELF has for free via `.init_array`, running
automatically before `main()`, that this reconstruction simply hadn't modeled yet:

1. **`CGlobalObjectBase`** (new: `include/global_object_base.h`,
   `src/base/global_object_base.cpp`) — a tiny (one `void*`) common base every
   `XxxApiInstance`-style global in the real binary placement-constructs first. Its
   real 23-byte constructor (`.text+0x080632e0`) installs its own vtable, then calls
   `CKernel::AddGlobalObject(this)`.
2. **`CKernel::AddGlobalObject`/`RemoveGlobalObject`** (new methods on `CKernel`,
   `include/ckernel.h`/`src/init/ckernel.cpp`) — real, 123/70-byte bodies. This is the
   actual, previously-untraced producer of `sm_poGlobalObjectList` (ckernel.cpp's own
   Stage-3 comment flagged this as an open question): it's lazily created as a
   `COmegaPtrArray(growBy=1, initialCapacity=0, ownFlag=0)` on first call, then every
   registering object is `COmegaPtrArray::Add()`-ed onto it. `COmegaPtrArray::Add()`
   (`.text+0x080a6da0`, 343 bytes) and the matching 3-int constructor overload
   (`.text+0x080a6c10`, 113 bytes) were added to `omega_ptr_array.h`/`.cpp` to support
   this — Tier A, same Duff's-device-collapse convention as the array's other 5
   methods.
3. **Every `XxxApiInstance` global has its own real static constructor.** Confirmed by
   reading all 9 `global.constructors.keyed.to.<Name>@<addr>.c` decompiles
   (`Decomp/EVA_Decomp/eva_export/functions/`) — `SysApiInstance`, `EditApiInstance`,
   `SeqApiInstance`, `ChkApiInstance`, `DumpApiInstance`, `RMApiInstance`,
   `RTRouterApiInstance`, `g_oSysExApiInstance`, and (not wired here, out of scope —
   nothing on this project's traced boot path reads it) `BDApiInstance`. Each one
   base-constructs a `CGlobalObjectBase`, installs its own real per-class vtable,
   zero-initializes its own fields, and finishes with a `XxxApi = XxxApiInstance;` /
   `DAT_x = "XxxApi";` pair. **`SysApiInstance`'s own copy of this sequence is the one
   that sets `Api = SysApiInstance;`** — this is what makes `Api` non-null before
   `main()` ever runs. Implemented as 8 `__attribute__((constructor))` functions (one
   in `src/base/sysapi_instance.cpp` for `SysApiInstance`, 7 in `src/init/mains.cpp`
   for the rest, each co-located with its own byte-buffer definition) rather than real
   C++ object syntax, matching this project's own established "raw buffer + manual
   vtable install" convention for these globals — `__attribute__((constructor))` is a
   real GCC `.init_array` entry, the same underlying mechanism the original
   compiler-generated `global.constructors.keyed.to.*` functions use, just expressed
   directly instead of via class-object syntax.
4. **The 4 "phase hook" vtable slots** `CKernel::CKernel()`/`~CKernel()` already
   dispatch on every `sm_poGlobalObjectList` entry (vtable +8/+0xc/+0x10/+0x14 — a
   Stage-3 comment flagged these as an unidentified "lifecycle notification pattern")
   are now identified by name too: `CGlobalObjectBase::PreKernelConstructor`/
   `PostKernelConstructor`/`PreKernelDestructor`/`PostKernelDestructor`
   (`.text+0x0804cc10`/`20`/`30`/`40`, 3 bytes each, all literally `return 0`).
   Confirmed by direct raw-byte read of the real binary's installed vtables — both
   this base class's own (`08e80f08`) and `CSysApiInstance`'s/`CEditApiInstance`'s/
   `CSeqApiInstance`'s own derived vtables at the same 4 offsets, spot-checked — that
   none of them are overridden anywhere this project touches. Practical effect: this
   pass's own reconstruction of `sm_poGlobalObjectList` (point 2) means the list is no
   longer permanently empty by the time `CKernel::CKernel()` runs its own two walks
   over it — they're live code paths now, not dead ones, and they're safe.

### Ground truth, not guesswork

Three specific claims above were confirmed by directly reading raw bytes out of the
real `Eva` binary (`python3` + `readelf -l` to map VA → file offset), not inferred from
the decompile alone:

- **`Api`'s real vtable slot +0xa4 is exactly `CSysApiInstance::RegisterApi`**
  (`.text+0x0806bab0`) — read `.rodata+0x08e81008+0xa4` and got `0x0806bab0` back,
  byte for byte matching `sysapi_instance.h`'s own already-documented address for that
  method. This resolved the "outlier" framing Stage 3's own notes gave
  `MMainSysEx` (the one function in the 9-member `InitSystemLayer`-adjacent family that
  called `RegisterApi()` by name instead of through the vtable slot): it was never a
  different mechanism, just a different calling style for the same function. The other
  8 `MMainXxx(void)` functions that used to do
  `(**(code**)(*Api+0xa4))(Api, DAT_x, Instance)` now call
  `((CSysApiInstance*)Api)->RegisterApi(DAT_x, (CApiBase*)Instance)` directly — except
  `MMainFileMan`, whose own `DAT_0930b174` has no matching `global.constructors.keyed.
  to.*` producer setting it to a string (unlike its 7 siblings) and stays a raw
  `int`-typed vtable dispatch, not asserted to be something it isn't confirmed to be.
- **`CGlobalObjectBase`'s real 6-slot vtable** (`0` complete-object dtor / `4` deleting
  dtor / `8`/`0xc`/`0x10`/`0x14` the 4 phase hooks) — read 6 dwords starting at
  `.rodata+0x08e80f08` and matched every one against its own named function's real
  `.text` address.
- **None of `CSysApiInstance`/`CEditApiInstance`/`CSeqApiInstance`'s own derived
  vtables override the 4 phase-hook slots** — spot-checked by reading the same 4 byte
  offsets from each class's own installed vtable address and confirming they're
  bit-for-bit identical to the base class's no-ops.

### The `DAT_xxx` → name-string bug (all 7, not just the one already found)

The task that found this crash had already confirmed `DAT_0930aae8` (passed to
`MMainEditMan`'s registration call) is really a `const char*` holding `"EditApi"`, not
an undecoded `int`. Reading the remaining 6 `global.constructors.keyed.to.*`
decompiles confirmed the same is true across the board — every one of these ends with
a `XxxApi = XxxApiInstance; DAT_x = "XxxApi";` pair:

| Constant | Real value | Function |
|---|---|---|
| `DAT_0930aae8` | `"EditApi"` | `MMainEditMan` |
| `DAT_0931b20c` | `"SeqApi"` | `MMainSeqTimer` |
| `DAT_0930a6ac` | `"ChkApi"` | `MMainChunkMan` |
| `_DAT_0930a324` | `"RTRouterApi"` | `MMainRTRouter` |
| `DAT_0930a6bc` | `"DumpApi"` | `MMainDumpMan` |
| `DAT_0931b1f0` | `"RMApi"` | `MMainResMan` |
| `_DAT_0931b314` | `"SysExApi"` | `MMainSysEx` (already correctly typed before this pass) |

All 7 are now `extern "C" const char*` in `src/init/mains.cpp`, not `int`.
`DAT_0930b174` (the *other* undecoded constant in this file, used by both
`MMainLinuxDriver`'s `FMApi` fetch and `MMainFileMan`'s own registration call) is
**not** one of these seven — it has no matching static-constructor producer anywhere
in the export, Ghidra types it `undefined4` at every site, and it's deliberately left
as an unconfirmed `int`, not reclassified on the strength of the pattern alone.

### A second, related bug: `EditApiInstance` was a null pointer, not an object

While wiring these constructors it became clear `EditApiInstance` had the same class
of bug as the `DAT_xxx` constants, just for the *third* argument instead of the
second: it was declared `void *EditApiInstance = 0;` — a null **pointer** — when the
real global (confirmed by its own static constructor, which zero-initializes up to
byte offset 1027 of it) is a real ~1028-byte **object**, and every real call site
passes the object's own address, not a pointer stored in it. Fixed to
`unsigned char EditApiInstance[0x404] = {};` (array-to-pointer decay gives the same
correct value everywhere it's used, including `config_manager.cpp`'s own
`AssignEditServerIDs()`, updated to match). Its siblings `SeqApiInstance`/
`ChkApiInstance`/`DumpApiInstance`/`RMApiInstance`/`RTRouterApiInstance` were already
declared as byte buffers (correct shape) but several were undersized relative to what
their own real static constructors actually write — bumped to real sizes
(`SeqApiInstance`/`DumpApiInstance` 4→8 bytes, `RMApiInstance` 4→0x2c,
`RTRouterApiInstance` 4→0x1c) rather than left to silently overrun.

### A documentation correction, found while re-deriving `SysApiInstance`'s own layout

`sysapi_instance.h`'s existing `+0x04..+0x1c mDrivers` / `+0x1c..+0x34 mApis` field
labels (originally guessed purely from `Cleanup()`'s own generic offset arithmetic)
turn out to be swapped: `SysApiInstance`'s real static constructor installs
`TNamedPtrArray<CApiDescriptor>`'s own vtable at `+4` and `TNamedPtrArray<CDriverBase>`'s
at `+0x1c` — i.e. `+4` is really `mApis` and `+0x1c` is really `mDrivers`. Corrected in
the header; `Cleanup()`'s own code never used named fields either way, so this is
documentation-only, not a functional change.

### What's faithful vs. stand-in in this pass

- **Tier A (faithful):** `CGlobalObjectBase`'s ctor/dtor/4 phase hooks,
  `CKernel::AddGlobalObject`/`RemoveGlobalObject`, `COmegaPtrArray`'s new 3-int ctor
  and `Add()`, all 8 `XxxApiInstance` static constructors' own field-write sequences,
  the 7 corrected `DAT_x` values, all 7 corrected/resized `XxxApiInstance` buffers, and
  the 6 `MMainXxx(void)` functions switched to a direct `RegisterApi()` call.
- **Tier B / explicit stand-in (unchanged from before, or newly added but still
  opaque):** every `PTR__CXxxApiInstance_<addr>`/`PTR__CRMApi*`/`DAT_08e88d80`
  per-class vtable these constructors install is a single opaque symbol (same
  "install but never dispatch" treatment as `PTR__CHIDDriver_08fd9ce8` etc.) — real
  addresses, null content, since nothing in this reconstruction ever dispatches
  through an `XxxApiInstance` object's own vtable. `CSysApiInstance`'s own vtable
  (`PTR__CSysApiInstance_08e81008`) is the one exception: it's a real, ground-truth
  slot-counted (94 slots) array of the shared `EvaVTableStub` no-op, since `Api`'s
  vtable genuinely *is* dispatched through elsewhere (Mains()'s existing 17-member
  family, `+0x40`/`+0xa0`/`+0xb4`) and needed to stay safely in-bounds. `CRMJob`
  (constructed mid-way through `RMApiInstance`'s own ctor) stays an unreconstructed
  raw blob, same treatment as `CTracer`/`CErrorHandler` in `ckernel.cpp`.
  `RTRouterApiInstance`'s real constructor also initializes 2 unrelated file-scope
  globals (`kInvalidBytePair`/`kPitchBendDefault`) via the same compiler-generated
  function — not modeled, since nothing in this reconstruction reads either.
  `BDApiInstance` (the 9th real sibling in this family) is not wired at all — out of
  scope, nothing on this project's traced boot path reads `BDApi`/`BDApiInstance`.

### Build/verify status

`make objs` and `tools/build_lenny.sh` both stay clean after this pass — the pre-existing
`LINK OK` full link against the real on-image `RestoreDVD_SystemMNT` libs still
succeeds with zero unresolved symbols, no regression from Stage 4. This pass's own binary
did get past the `MMainEditMan()` crash on the next live boot test — see "Boot-path crash
chain closed out" below for what it hit next and how that was resolved.

## Boot-path crash chain closed out — 2026-07-24

Continuing the exact same live-`kronos_vm`-boot iteration loop that found 4b's `Api`/
`SysApiInstance` bug, two more real bugs were found and fixed, both in Eva's own code
(not OA.ko). Diagnostic technique: sprinkle `puts("CHECKPOINT: <label>"); fflush(stdout);`
after every remaining statement in the suspected span, rebuild via `tools/build_lenny.sh`,
redeploy, reboot, read `/korg/rw/eva_test_stdout.log` off the live ttyS1 shell to see
exactly how far execution got before the segfault line in `boot_console.log` — narrows a
whole function body down to one statement per iteration. Reusable for any future "ip
(null)" crash where the fault address alone doesn't pin a location.

1. **`mains.cpp`: 7 of the ~9 `PTR__CXxxApiInstance_*` "vtables" (Edit/Seq/Chk/Dump/
   RTRouter/SysEx/RM ApiInstance) were declared as bare `void* = 0` instead of a
   properly-sized array** (unlike the correct `PTR__CSysApiInstance_08e81008[94]` pattern
   already used elsewhere, `omega_vtables.cpp`). Each of these 7 objects is a real
   `CGlobalObjectBase`-derived global, unconditionally registered into
   `sm_poGlobalObjectList` by its own `__attribute__((constructor))` function, and
   `CKernel::CKernel()`'s own ctor genuinely walks that whole list and dispatches vtable
   slots +8/+0xc/+0x10/+0x14 (the 4 phase hooks) on EVERY entry — reading slot index 2
   (byte +8) out of a single 4-byte variable is UB, landing on adjacent `.data`, crashing
   as `ip (null)` inside `CKernel::CKernel()` right after its own "create new kernel"
   print. This file's own header comment used to claim "nothing in this reconstruction
   ever dispatches through an XxxApiInstance object's own vtable" — written before
   `CKernel::CKernel()`'s own phase-hook walk was traced (4b, same day), directly
   disproven by this live boot. Fixed: each of the 7 is now a proper 6-slot
   `EvaVTableStub`-filled array (6 = the confirmed `CGlobalObjectBase` minimum actually
   dispatched through), and every `= &PTR__Xxx;` assignment site had its stray `&`
   removed (array decay already gives the right `void**`, `&array` doesn't).
2. **`omega_vtables.cpp`: `EvaVTableStub()` is a genuine no-op with no return statement**
   — safe for the vast majority of "installed but never dispatched, or dispatched but
   return value discarded" vtable slots, but ONE slot on `Api`'s own 94-slot vtable
   (index 40, byte offset `+0xa0`) is a real exception: `mains.cpp`'s `MMainLinuxDriver`
   fetches "FMApi" via a virtual call through exactly this slot and USES the return value
   (dispatches through FMApi's own vtable moments later, `+0x24`). Calling a no-op with no
   `return` leaves EAX holding whatever was there before the call (observed live: the call
   target's own address) — that garbage got used as a `CSystemApi*` and immediately
   dereferenced-and-dispatched-through, landing on arbitrary memory. Fixed by installing a
   dedicated `GetFMApiStub(void*)` at that one array index instead, returning
   `SysApiInstance` itself (a real, valid, already-fully-stubbed object) — safe for
   anything downstream to dispatch through again, without claiming to know what the real
   `FMApi` object actually is.

**Confirmed working live, full run** (re-verified 2026-07-25 against the debug-print-free
cleanup of this same fix): every one of the 17 `MMainXxx()` calls in `Mains()` completes,
`Omega.Init(0)` returns normally, and Eva reaches its own natural "Start closing"/
"End closing" shutdown and exits cleanly — zero segfaults, matching this project's
"reasonably boots" goal exactly. `OmegaTimingThread(0)` returning immediately (rather than
blocking forever the way it would on real hardware) is why `main()` reaches its shutdown
path at all in this VM — expected and fine for the boot-milestone bar, not itself a bug to
chase. **This closes out the Stage-1-through-4 boot-path effort**; remaining work is
Stage 6 breadth (see the status table above).

## Stage 6: breadth sweep, first batch — 2026-07-25

With the boot-path effort closed out (4c above), this session's own broader `/goal`
directive ("if OA.ko hits a true blocker, pivot to Eva and continue reverse engineering
it") extends past the original "just get it booting" scoping — picking up real,
tractable function reconstruction beyond the boot path. First candidate: `CScheduler`'s
own last 2 Tier-B methods, both already on the already-reconstructed boot path
(`CKernel::InitSystemLayer()` calls `CScheduler::InsertLevel()` 7 times; `CKernel::Exec()`
— itself called from every `OmegaSchedulingThread` wakeup — calls `CScheduler::Exec()`)
but stubbed to no-ops until now:

- **`CLevelManagerArray::Find()`/`Add()`** (`.text+0x0805ee90`/`0x0805ec70`, 258/522
  bytes — this file's own header comment had mistranscribed both addresses with an
  extra digit before this pass, corrected against `functions.csv`). `CLevelManagerArray`
  IS-A `COmegaPtrArray` (vtable-swapped, no new fields, same idiom used throughout this
  project). `Add()` appends via the real `COmegaPtrArray::Add()` base method then sifts
  the new element left while its own level number (`+0xc`) is smaller than its
  predecessor's — a real insertion-sort step that keeps the array sorted ascending by
  level regardless of insertion order. `Find()` linear-scans for the first element whose
  `+0xc` matches. Both were previously always-empty/always-"not found" stubs — the array
  is now genuinely populated by `InsertLevel()`'s existing real "build a `CLevelManager`,
  call `Add()`" path, which was already faithfully transcribed but had nothing real to
  hand off to.
- **`CScheduler::Exec()`** (`.text+0x080623e0`, 1025 bytes) — the real per-tick dispatch
  loop, now a faithful (Duff's-device-unrolled-to-plain-loop, same collapse license as
  `omega_ptr_array.cpp`) walk over the now-real, sorted `CLevelManagerArray`: for each
  level, decrements a short countdown (`+0x18`, reloaded from a period value at `+0x1a`)
  and calls `CLevelManager::RunLevel()` once it reaches 0. Two real "bail the *entire*
  tick, not just this level" guards (`+0x38` disabled flag, `+0x10` reentrancy flag) are
  transcribed faithfully but stay unreached given this reconstruction's own data (nothing
  sets either field to a nonzero value) — same "faithful but currently-dead branch"
  treatment already established elsewhere in this project (e.g. `CKernel::Exec()`'s own
  empty timer vector). `HAL_DisableInterrupts()`/`HAL_EnableInterrupts()` around the
  countdown read/write are dropped, same established reason as every other occurrence of
  that pair (kernel-side critical-section shim, not applicable to this single-threaded-
  per-tick userspace reconstruction).
- **`CLevelManager::RunLevel()`** (`.text+0x0805ea10`, 567 bytes) stays Tier-B — its real
  body calls `CTaskBuffer::SendBuffer()` (a wholly unintroduced class) then walks the
  level's own embedded `TNamedPtrArray<CModule>` task queue, dispatching through each
  due `CModule`'s own vtable slot `+8` ("Update"). Modeling that for real would pull in
  this project's entire per-module task substrate — genuinely out of scope for this
  batch, deferred to a future breadth-sweep pass. The one real, trivial side effect kept:
  `RunLevel()`'s own tail unconditionally clears the level's missed-tick counter
  (`+0x1c`) — free to preserve faithfully even with the rest stubbed.

`CLevelManagerArray`/`CLevelManager` were broken out of `scheduler.cpp` into their own
header (`include/level_manager_array.h`) purely so `verify/test_level_manager_array.cpp`
could drive `Add()`/`Find()` directly against a synthetic `COmegaPtrArray`-shaped buffer
— same reason `omega_ptr_array.h` exists as its own header. `CScheduler::Exec()` itself
isn't independently KAT-tested (its own fields are private, and `RunLevel()`'s only
externally-observable side effect is the trivial counter clear) — verified instead by
(a) meticulous decompile cross-check while writing it and (b) a live `kronos_vm` re-boot,
which now genuinely exercises it every `OmegaSchedulingThread` tick during the run
(6 threads, 7 real inserted levels) — still reached the same clean `Start closing`/
`End closing` exit, zero crashes.

**Build/verify**: `make` (15/15 `test_level_manager_array` checks) and
`tools/build_lenny.sh` (`LINK OK`) both clean; manifest 96 → 99 reconstructed.

**Next candidates for a future batch** (not started): `CModule`'s own real layout/vtable
(`+0x4c` status byte, `+0x78`/`+0x7a` period/countdown, vtable slot `+8` "Update") and
`CTaskBuffer` — both genuinely needed to make `CLevelManager::RunLevel()` real, and the
natural next step to keep widening `CScheduler`'s already-real substrate. Beyond that,
this is a ~38,000-function binary with 99 reconstructed — Stage 6 breadth-sweep work
remains almost entirely ahead; pick candidates the same way this batch did (Tier-B stubs
already sitting on an already-reconstructed real call path, not cold/unreferenced
functions) rather than trying to move broadly at random.

## Stage 6: breadth sweep, batch 2 — 2026-07-25

Picked up exactly the "next candidates" this batch's own predecessor left open:
`CModule`'s real vtable, a new `CTaskBuffer` class, and making
`CLevelManager::RunLevel()` genuinely real using both.

**Ground-truth correction, found while tracing `RunLevel()`'s own per-element field
reads**: the prior batch's own comment claimed `CLevelManager`'s task queue holds
`TNamedPtrArray<CModule>` and dispatches "Update" through **`CModule`'s** own vtable
slot `+8`. Both halves of that claim were wrong. `RunLevel@0805ea10.c` dereferences
`+0x4c` (mask/flags byte), `+0x78`/`+0x7a` (period/countdown) on each queue element —
`CModule`'s own real ctor (`module.h`) never touches those offsets; its whole base
object is only `0x2c` bytes. Those offsets instead match **`CTask`**'s own real ctor
(`CTask@0807ee80.c`) byte-for-byte. The vtable slot dispatched is `CTask::Exec()` —
confirmed via direct ground truth (`Exec@08180950.c` is a real, 3-byte `return 0;`) —
entirely unrelated to `CModule`'s own vtable slot `+8` (`Setup`, dispatched by name in
`module_manager.cpp`) despite the coincidental identical slot number. Corrected in
`level_manager_array.h`'s own header comment.

### `CModule`'s real vtable (`include/omega_vtables.h`/`.cpp`)

Previously a bare `static void *PTR__CModule_08e81fe8 = 0;` local to `module.cpp` —
upgraded to a proper ground-truth-sized array, matching this project's established
per-class vtable-sizing convention (`omega_vtables.h`'s own methodology). Real slot
count: **7**, derived from the real installed-pointer address
(`PTR_~CModule_08e81fe8`, confirmed by direct inspection of `symbols.csv`'s own
`08e81fe0..08e82020` region — the raw `vtable` label sits 8 bytes *before* the address
actually written into objects, the classic Itanium 2-word RTTI-header offset) to the
next real symbol of any kind, which for `CModule` is its own typeinfo object at
`08e82004`: `(0x08e82004 - 0x08e81fe8) / 4 = 7`. Matches perfectly against the 5 already-
confirmed dispatches (dtor pair, `Setup`@`+8`, `Config`@`+0xc`, `Start`@`+0x10`) plus 2
further real named `CModule` methods this pass didn't individually trace
(`Destroy`@`08181c10`, `GetErrorMsg`@`08181c20`) that exactly fill the remaining 2
slots — not a coincidence. Filled with the shared `EvaVTableStub` no-op throughout
(same as every other array in that file) since nothing in this reconstruction ever
dispatches through this specific vtable (every real `MMainXxx(void)` caller in
`mains.cpp` overwrites it with the derived module's own vtable immediately after
construction, and `CModuleManager::Setup/Config/Start()` only ever iterate `mModules`
— always empty, since `CModuleManager::AddModule()` stays a Tier-B stub).

### `CTaskBuffer` (new: `include/task_buffer.h`, `src/base/task_buffer.cpp`)

Tier A, both real methods this pass needed: `SendBuffer()` (`.text+0x08055f20`, 137
bytes) and `~CTaskBuffer()` (`.text+0x08055ec0`, 57 bytes). Not polymorphic — confirmed
directly from the decompile (`SendBuffer()` dereferences `*this` as a linked-list head
pointer, never as a vtable). Real layout: `{mHead, mUnused04}`, 8 bytes, embedded in
every `CLevelManager` at `+0x04` (already-known real span, `CScheduler::InsertLevel()`
zeroes exactly these two ints).

`SendBuffer()` pops and drains the whole pending-message list: for each node, dispatches
a vtable call through the node's own stored "target" value, this-adjusted by a fixed
`+8` offset (the classic multiple-inheritance interface-adjustment-thunk pattern —
matches `CTask`'s own ctor registering itself via
`RegisterIfc(this, (CIfcUnknown*)(this+0x60))`, itself not reconstructed, see below),
passing the node's own embedded message sub-object as the argument; frees an optional
extra payload if a flag bit is set; then returns the node to a **global** free-list
pool (`sm_poPool`/`sm_wCount`) rather than calling `free()` on it directly. **Nothing in
this reconstruction ever enqueues into a `CTaskBuffer`** (the producer side —
`AddMessage`/`NewBufferMessage`/`PurgeMessages`/`FreeBufferMessage`/`ShrinkPool` — has no
caller anywhere in this reconstruction's own call graph and is not reconstructed), so
`mHead` is always null in practice and the drain loop's own vtable dispatch — whose
target-object identity isn't fully decoded — is real but currently unreached, same
license already established for `CScheduler::Exec()`'s own dead bail branches.
`~CTaskBuffer()` is worth flagging as found: it walks the *global* `sm_poPool`, not
`this`'s own `mHead` list — preserved exactly as found, not "fixed."

### `CLevelManager::RunLevel()` made real (`.text+0x0805ea10`, 567 bytes)

Now genuinely reconstructed in `scheduler.cpp` (moved out of the header, which now just
declares it): calls `CTaskBuffer::SendBuffer()` on the level's own embedded buffer
(`this+4`), then walks the level's own embedded `COmegaPtrArray`-shaped task queue
(`this+0x20`, count/array at the usual relative `+0xc`/`+0x14` → absolute `+0x2c`/
`+0x34`) — a Duff's-device-unrolled 4-way walk in the original, collapsed to a plain
per-index loop, same license/verification method as every other unrolled walk in this
project. Per element: skip entirely (countdown left untouched) if the task's own
mask/flags byte (`+0x4c`) has either low bit set — the real code short-circuits
*before* touching the countdown, confirmed via the `&&` chain in the decompile;
otherwise decrement the countdown (`+0x7a`) and, once it reaches 0, reload it from the
task's own period (`+0x78`) and dispatch `CTask`'s vtable slot `+8` (`Exec()`). The
function's own tail unconditionally clears the level's missed-tick counter (`+0x1c`)
regardless of what the loop did — already modeled by the prior batch, unchanged here.

**`CTask` itself is deliberately not reconstructed as a constructible class.**
`CTask::CTask()` (`.text+0x0807ee80`, 330 bytes) has zero callers anywhere in this
reconstruction's own call graph — nothing on the traced boot path ever calls
`new CTask(...)` — so implementing it would be adding dead code, the same "don't
fabricate an uncalled function" license already established for `OmegaExitThread`
(Stage 3). Its real field layout (confirmed from the ctor's own decompile) is
documented in `level_manager_array.h`'s header comment purely so `RunLevel()`'s own
per-tick reads have ground truth behind them: `+0x4c` mask byte, `+0x78`/`+0x7a`
period/countdown, `+0x48` scope id (`Api` vtable `+0x3c`, same mechanism as `CModule`'s
own `mScopeId`), `+0x60` embedded `CLimiterMan` (own real ctor is tiny — 46 bytes,
`CLimiterMan@0807bd10.c` — but not wired here since nothing constructs a `CTask` to own
one), and a call to `CTask::RegisterIfc()` (`0807ec90`, 472 bytes — a real, genuinely
deep interface-registry duplicate-detection scan + growable-vector append, correctly
out of scope even if `CTask::CTask()` were being implemented).

### Verification

Two new host-side KAT files, same synthetic-byte-buffer-overlay style as
`test_level_manager_array.cpp`:

- `verify/test_task_buffer.cpp` (8 checks) — drives `CTaskBuffer::SendBuffer()`
  directly: empty-list no-op, single-node dispatch (confirms the `+8` this-adjustment
  and the `node+4` message-argument passing land on the exact right addresses), and the
  flag-bit extra-payload-free path.
- `verify/test_run_level.cpp` (13 checks) — drives `CLevelManager::RunLevel()` against
  synthetic `CLevelManager`/`CTask`-shaped byte buffers: countdown-reaches-zero fires
  and reloads from period, countdown-not-yet-zero just decrements, both mask bits (0
  and 1) independently suppress dispatch *and* leave the countdown untouched, a mixed
  masked/unmasked queue dispatches exactly once, an empty queue no-ops safely, and the
  missed-tick counter is unconditionally cleared regardless of task states.

Both suites caught the same real bug during development: the fake "dispatch target"
objects were built with an *inline* vtable array as the object's own first member,
rather than a *pointer* to a separately-allocated vtable array — the real C++ object
model (and this project's own manual-vtable-swap idiom everywhere else) always makes an
object's first word a *pointer* to its vtable, never the vtable's own storage. Building
it wrong meant `*(void**)obj` read the vtable's own first *slot* (zero-initialized, i.e.
NULL) instead of a valid vtable address, producing a segfault at a highly suspicious
"dereferenced 0x8" address — worth remembering for any future KAT that constructs a fake
polymorphic object by hand.

`make` (36/36 across all 4 `verify/test_*` binaries: 15 level-manager-array + 8
task-buffer + 13 run-level, plus the pre-existing 20-check ustg_user_api suite, all still
green) and `tools/build_lenny.sh` (`LINK OK`) both clean. Manifest 111 → 114
reconstructed (`0805ea10`/`08055f20`/`08055ec0`).

**Live `kronos_vm` boot test**: re-verified clean — see the note in the commit/session
history for the exact recipe reused this time (fresh scratch VM, shifted port set to
avoid a concurrent session's own VM on the shared sandbox). `CScheduler::Exec()` now
calls a genuinely real `RunLevel()` every tick (still practically a no-op given the
always-empty task queue, per the scoping above), with zero change in observed behavior
— same clean `Start closing`/`End closing` shutdown, no new crash.

**Next candidates for a future batch**: the natural next widening is giving `CTask` a
real, callable constructor — but that requires first finding *some* real caller
(a `MMainXxx`/`CModule`-adjacent function that actually calls `new CTask(...)` to
register a task with a level) elsewhere in the ~37,700 still-`pending` functions, which
this batch did not go looking for. `CModule::Add(CTask*)`/`Remove(CTask*)`
(`.text+0x0807c410`/`0x0807c470`, 91/461 bytes — the real "attach a constructed task to
its owning module" methods) are likely close companions to that same search once a
`CTask`-constructing caller is found.

## Stage 2: IPC substrate closed out — 2026-07-25

Per this session's own `/goal` follow-on (survey IPC/message substrate + Peg toolkit
substrate, per README's own "Stage 2 ... not started as its own stage" / "Stage 5 ...
not yet known to be necessary" notes). **Note on scope**: a separate, concurrent agent
was working `src/base/scheduler.cpp`/`module.cpp`/the new `task_buffer.cpp`/
`include/level_manager_array.h` at the same time (CModule/CTaskBuffer/
`CLevelManager::RunLevel()`) — this pass deliberately stayed out of that territory
entirely; none of the files below overlap with it.

### Survey A: IPC/message substrate

`nm`/`symbols.csv` swept for every `USTGUserAPI`/`CSTGHandle`/`CComm*`/`*Fifo*` symbol
not yet in `src/`. Two genuinely tractable, still-unreconstructed clusters found:

- **`USTGUserAPI`'s own remaining send/receive/teardown methods** (8 of them):
  `Disconnect`, `ConnectUnsolicitedFifo`, `ReadMessage`, `ReadMessageWithTimeout`,
  `ReadUnsolicitedMessage`, `SendPanelMessage`, `GetProgress`/`IncrementProgress`/
  `SetProgress` — together with the already-reconstructed `Connect`/
  `SendSTGMessageWithSource`/`ConnectPanelFifo`, this is now the class's *complete* real
  send/receive/teardown surface. All 8 are Tier A, transcribed instruction-by-instruction
  from `Decomp/EVA_Decomp/eva_export/functions/`.
- **`CSTGHandle`'s remaining 2 methods** (`GetSize`/`Release`) + `CSTGHandleCache::Cleanup`
  — the rest of the shared-memory-handle class `Access()` (Stage 4) only partly covered.
  All Tier A.

The other ~150 `USTGAPIXxx::UpdateYyy()`-style classes found in the same sweep
(`USTGAPIProgram`, `USTGAPISampling`, `USTGAPIControl`, `USTGAPIKLM`, `USTGAPIPCMBanks`,
`USTGAPIMIDI`, ... — see the sweep's own full output, not reproduced here) are a
different, much larger thing: per-subsystem parameter-update RPC shims, not IPC
plumbing itself — genuinely Stage 5/6 breadth, not this pass's scope.
`CSTGUnsolMsgHandler`'s ~20 `*MsgHandler` methods are similarly out of scope (real
unsolicited-message *dispatch*, i.e. what happens after a message arrives, not the
transport itself).

**Real findings from reading all 12 decompiles:**

- **`Disconnect()`/`ConnectUnsolicitedFifo()` have no caller anywhere in the 37,795-
  function export** — same "real but currently unreachable" status this project has
  already established for `OmegaExitThread` (Stage 3) and `CSTGHandleCache::Cleanup`
  itself. Reconstructed anyway for IPC-surface completeness (Stage 2's whole point), not
  because either is on the traced boot path.
- **A 5th real device node**: `ConnectUnsolicitedFifo()` opens `/dev/rtf5` (`O_NONBLOCK`)
  — the "unsolicited message" channel `ReadUnsolicitedMessage()` reads from. Together
  with `/dev/rtf0`/`/dev/rtf1`/`/dev/dmsg0` (`Connect()`, Stage 1) and `/dev/rtf7`
  (`ConnectPanelFifo()`, Stage 4), Eva's full known RTAI-FIFO/stg_direct device surface
  is now 5 nodes.
- **`GetProgress`/`IncrementProgress`/`SetProgress` are a wholly separate channel**:
  a `/proc/OmapNKS4ProgressBar` text file, not the rtf/dmsg0 FIFO substrate at all.
  `IncrementProgress()`'s 3-byte literal payload was confirmed by reading the real
  binary's own `.rodata` at `DAT_08fd9367` directly (`readelf -l` to map VA→file offset,
  then a raw byte read) rather than guessed — it's the literal ASCII string `"inc"`.
- **`ReadMessageWithTimeout(NULL, ...)` is a real "just wait" mode**, not a bug: with a
  NULL message pointer it does zero I/O and busy-loops purely on `gettimeofday()` until
  the deadline passes, then returns 0 — transcribed faithfully.
- **The whole `ReadMessage`/`ReadMessageWithTimeout`/`ReadUnsolicitedMessage` family
  uses a real busy-poll-to-deadline design with no `select()`/`poll()`/sleep anywhere**,
  which only makes sense at all if the real `/dev/rtf1`/`/dev/rtf5` RTAI-FIFO character
  devices return immediately (short read) when empty rather than blocking — i.e. these
  fds are presumed non-blocking-when-empty on real hardware, unlike a POSIX pipe. Noted
  explicitly since it's an inference from the code's own shape, not independently
  confirmed against the real RTAI FIFO driver's own semantics.
- **`Disconnect()` calls `CSTGHandle::Release()` (both on `mSharedMem` and on a second,
  synthesized `mode=1` stack handle standing in for `mFrontPanelStatusAddress`'s own
  attachment) completely unconditionally, with no NULL check** — faithfully preserved,
  not "fixed": if `Disconnect()` is ever called before `Connect()` has run (never
  observed in this export, and given `Disconnect()` itself has no callers at all, this
  is purely theoretical), this would NULL-deref inside `Release()`. `Release()`/`GetSize()`
  themselves are real and correct against `Access()`'s own `CSTGHandleCache::
  sCachedHandleInfo` layout (refcount decrement / `munmap()` on last release; cached-size
  read with a lazy fresh `ioctl()` on first query) — resolved by direct decompile
  cross-check, both call sites' pointer arithmetic (`psVar2+2`→`addr`, `psVar2+4`→`size`)
  matches `Access()`'s own `CachedHandleEntry{refCount,pad,addr,size}` layout exactly.

**Verification**: `verify/test_ustg_user_api.cpp` (new, 20/20 checks) drives the real
`ReadMessage`/`ReadMessageWithTimeout`/`ReadUnsolicitedMessage`/`SendPanelMessage`/
`Disconnect` bodies against real host pipes — a `UstgUserApiTestHooks` friend struct
(declared in `ustg_user_api.h`, same spirit as `level_manager_array.h`'s own extraction
for testability) points the class's private fd-cache statics at pipe fds instead of the
real (host-nonexistent) `/dev/rtfN` device nodes, so the length-prefix wire-format
parsing is genuinely exercised end-to-end (frame success, oversize-frame rejection,
NULL-buffer, zero-length-send-is-a-no-op-success, timeout-deadline-reached), not just
decompile-cross-checked — matching PLAN.md's own call-out that this specific wire-format
code deserves the extra layer-C scrutiny. One real trap hit and fixed while writing this
test: a plain POSIX pipe *blocks* on an empty read with the write end open, which hangs
`ReadMessageWithTimeout`'s real busy-poll design forever unless the read end is marked
`O_NONBLOCK` — matching the presumed real RTAI-FIFO semantics noted above; without it,
the KAT process itself hangs (caught by running it under a bounded `timeout`, per this
project's own established caution around exactly this class of hazard).

Both `make objs`/`tools/build_lenny.sh` (`LINK OK`, real on-image-lib link) confirm no
regression. A live `kronos_vm` boot-test (fresh scratch copy, `FAST_RTAI` flag,
`systemd-run --collect` launch, same recipe as Stage 6 batch 1) confirmed the full chain
still reaches a clean boot with this build — see this section's own closing note for the
result once the boot-test run completed.

### Survey B: Peg toolkit substrate — confirmed not needed, with actual survey evidence

`PLAN.md`'s Stage 4 ("Peg toolkit substrate ... not assumed necessary yet") and the
existing README Stage-5 note ("Confirmed not necessary ... per the 4c live boot") were
both based on the boot path simply never calling anything Peg-named, inferred rather
than exhaustively swept. This pass did the actual sweep:

- **"Peg" is a real, large embedded GUI widget toolkit namespace** — 7,560 symbol rows,
  149 distinct `Peg`-prefixed classes (`PegScreen`, `PegPresentationManager`,
  `PegDialog`, `PegButton`, `PegMenu`, `PegSlider`, `PegNotebook`, `PegScroll`, ...) plus
  the `CPegForm`/`CPegNotifier` wrapper classes every one of the ~150+ `CFormXxx` mode
  UIs derives from. Consistent with "PEG" (Platinum Embedded Graphics, Swell Software) —
  a real, well-known commercial embedded-GUI toolkit from this era, matching the
  project's own prior guess.
- **Zero real call sites from anything in `reconstructed/Eva/src/`+`include/` into any
  Peg-prefixed class** — confirmed by `grep -rl Peg src/ include/` (only 2 hits, both
  comments referencing this exact scope boundary, no actual code).
- **Zero Peg-toolkit global/static constructors run before `main()`** — checked
  specifically because the `XxxApiInstance` family (Api/EditApi/SeqApi/...) proved
  Eva has real pre-`main()` static-constructor machinery that isn't obviously visible
  from the boot-path trace alone (the 4b/4c crash chain). Swept
  `global.constructors.keyed.to.*` in `symbols.csv` for anything Peg-related: the only
  matches (`g_oAmpEG`/`g_oVpmAmpEG`) are a substring false-positive ("am**peg**"), not
  real Peg-toolkit statics.
- **Nothing in `Mains()`'s 17-member registration-shim family or `InitSystemLayer()`'s
  9-member system-layer-init family constructs a Peg-derived object** — re-confirmed by
  re-reading both families' own already-reconstructed bodies (`mains.cpp`): the 2
  direct-construction shims are `CHIDDriver`/`CLinuxPanelDriver` (not Peg-derived), and
  the 15+9 module-registration shims all vtable-swap onto per-module vtables named
  `CXxxConstructor`/`CFileMan`/`CResMan`/etc — none Peg-prefixed.

**Verdict, now with actual evidence rather than inference: Stage 4/5 (Peg toolkit) is
confirmed genuinely unreached by anything between `main()` and the observed clean
`Start closing`/`End closing` exit, and has no hidden static-constructor path into it
either.** Not started, and per this survey, correctly so — this is a real, ~150-Peg-class,
thousands-of-function UI toolkit that would only matter once actual `CFormXxx` mode UIs
are in scope, which they explicitly aren't (PLAN.md's own "UI feature completeness is
not in scope" boundary).

## Stage 6: breadth sweep, batch 3 — 2026-07-25

Follows directly from batch 2's own leftover candidate: `module_manager.cpp`'s
`AddModule()`/`EnableUpdate()` were the last 2 Tier-B stubs in `CModuleManager`
(`Setup`/`Config`/`AdjustTaskMask`/`Start` were already Tier A). Both have real
boot-path callers (`mains.cpp`'s 8 `MMainXxx` registration shims call `AddModule()`
via `CSysApiInstance::AddModule()`; `ckernel.cpp`'s `InitSystemLayer()` calls
`EnableUpdate(1)` directly), so `mModules`/`mTopologyChanged` were staying
permanently at their construction-time zero values — the exact same "real caller,
dead Tier-B stub" shape batch 1 found in `CLevelManagerArray::Add()`/`Find()`.

**`AddModule()`** (`.text+0x0805efa0`, 869 bytes): real body performs a by-name
linear scan over `mModules` (the decompile renders it as the same scan twice in a
row — an existence check, then a second pass to re-derive the index for
`RemoveAtIndex()` — collapsed to one scan here per this project's established
Duff's-device-collapse license, since nothing mutates `mModules` between the two
real passes). If a module sharing the new module's `mName` is already registered,
it's `RemoveAtIndex()`'d first (using `mModules`' own `mUnknown04` field as
`RemoveAtIndex`'s `callDtorCallback` argument — a genuine re-registration-by-name
mechanism), then the new module is always `Add()`'d, `mBusy` is cleared, and the
host is conditionally notified if `mStarted`+`mTopologyChanged` are both set.

**`EnableUpdate()`** (`.text+0x08061ca0`, 74 bytes): confirmed as the real
(previously-flagged-as-"not traced") setter of `mTopologyChanged` — unconditional
on every call; if `enable != 0`, also clears `mBusy` and conditionally notifies the
host if `mStarted` is set.

### Safety-critical follow-on, not just documentation

Making `AddModule()` real means `CModuleManager::Setup()`/`Config()`/`Start()` (both
already Tier A) now genuinely walk a populated `mModules` and dispatch through every
registered module's vtable — previously provably dead code (batch 2's own
`omega_vtables.cpp` comment: "never actually dispatched through by any reconstructed
code ... since `CModuleManager::AddModule()` is a Tier-B stub"). Two live crash risks
this exposes were found and fixed in the same pass:

1. **6 of `mains.cpp`'s derived-module vtable placeholders were bare, always-NULL
   scalar globals**, not slot arrays (`void *PTR__CEditMan_08e85ea8;` etc.) —
   harmless while `Setup()`/`Config()`/`Start()` never actually dispatched through
   them, a live NULL-pointer-call crash the moment they do (`ckernel.cpp` calls
   `MMainEditMan()` then `Setup()`/`Config()` immediately after — the very first
   thing `InitSystemLayer()` does). Upgraded to real `EvaVTableStub`-backed arrays,
   sized via this project's established `symbols.csv`-boundary methodology:
   `CEditMan`/`CSeqTimer`/`CSysEx`/`CChunkMan`/`CDumpManMod` = 7 slots each (matching
   `CModule`'s own base count), `CMessagePort` (`CViewBase`'s real vtable) = 13 slots
   (real extra virtuals beyond `CModule`'s 7, not individually decoded).
2. **`CFileMan`/`CResMan` were modeled as independent stub classes with no `CModule`
   base** (`class CFileMan { public: CFileMan() {} };`), leaving their malloc'd
   buffer's `+4` "name" slot uninitialized. `AddModule()`'s real by-name scan
   dereferences that slot unconditionally the moment any other module is already
   registered (true for both — `EditMan`/`Viewer`/`SeqTimer` register first) —
   `strcmp(garbage_or_NULL, existingName)`, a near-certain crash (guaranteed if the
   fresh page happens to come back zeroed, since `strcmp(NULL, ...)` dereferences
   NULL). Both real ctors take no name argument (`functions.csv`:
   `CFileMan::CFileMan()`/`CResMan::CResMan()`), which only makes sense if each
   really does chain into `CModule`'s own base ctor with a hardcoded name literal —
   fixed by deriving both from `CModule` and calling `CModule("FileMan")`/
   `CModule("ResMan")` (placeholder name content, not decoded — same "unfaithful
   placeholder name doesn't change this pass's own control flow" license already
   used for `CEditMan_SysName` etc.).

### Verification

New `verify/test_module_manager_add_module.cpp` (13 checks): plain append on an
empty `mModules`, a second distinct-name append, by-name dedup-and-replace, `mBusy`
clearing, `EnableUpdate()`'s gating behavior, and — the safety-critical case —
`Setup()`/`Config()` dispatching through a genuinely-populated `mModules` without
crashing. All 5 verify binaries pass (13 new + all pre-existing). `make` + `make
verify` clean; `tools/build_lenny.sh` real-links against the on-image target libs
(`LINK OK`).

Live `kronos_vm` re-boot (existing `/root/eva_boot_test_20260722` scratch dir on
the `kronosvm` sandbox — no VM/telnet ports were bound at the time, so the
recipe's default ports were reused rather than shifted; freshly rebuilt Eva binary
deployed into the image's `/korg/rw/Eva` via a loop mount of partition 6) reproduced
the full, identical boot trace end to end, now genuinely exercising the fixed
`AddModule()`/`Setup()`/`Config()`/`AdjustTaskMask()`/`Start()`/`EnableUpdate()`
chain inside `init system layer`:

```
Eva will run on CPU 2
begin omega init
create new kernel
host buf init
set config info
init system layer
mains
done with mains
create init thread
start timing thread
done with omega init
end omega init
Start closing
End closing
```

Zero crashes, zero regressions from the previously-established clean-exit trace.

## Stage 6: breadth sweep, batch 4 — 2026-07-25

Per this session's own follow-on `/goal` directive (survey `CEditor::CPanelIfcTask`/
`USTGAPILCDControl`/`CKernel` for further genuinely-reachable methods; if none, broaden to
the next tractable candidate on the boot path). **Note on scope**: a separate, concurrent
agent was working the `CTask`/`CLimiterMan`/`CScheduler`/`CModule`/`CTaskBuffer`/
`CLevelManager` family at the same time (batch 3, `CModuleManager::AddModule()`/
`EnableUpdate()`) — this pass deliberately stayed out of that territory; none of the files
touched below overlap with it.

### Survey: `CEditor::CPanelIfcTask`, `USTGAPILCDControl`, `CKernel` — no further genuinely-reachable methods

All 3 confirmed via direct `nm -C` on the real binary (full method list) cross-checked
against a full `objdump -dr -C` disassembly of the real binary for call-site xrefs (faster
than a `rg`-over-37,795-tiny-files scan on this share's CIFS mount, which timed out
repeatedly — a real environment note worth keeping for future surveys of this kind: dump
the whole binary once with `objdump -d -C --no-show-raw-insn` and `grep`/`rg` that single
~136 MB text file instead of scanning the decompile export's per-function file tree).

- **`CEditor::CPanelIfcTask`** has 24 other real methods beyond `SetMargin`
  (`GetMargin`, `SetupPanelInterface`, `OnButtonEvent`/`OnEncoderEvent`/`OnAnalogEvent`/
  `OnTouchPanelEvent`, `SetLEDStatus` ×4 overloads, `SetAllLED`, `ShortBeep`/
  `ShortBeepPolite`, `EnterDiagnostics`, `Exec`/`Exec(CMessage&)`, ctor/dtor, ...) — but its
  own real constructor (`.text+0x0824b7e0`, takes `CEditor const&, PegScreen*`) has **zero
  callers anywhere in the 37,795-function export**, confirmed by grepping the full
  disassembly for call-sites targeting that address. Since nothing ever constructs a
  `CPanelIfcTask` instance in this reconstruction's own reachable call graph, every
  instance method on the class (including the already-reconstructed `SetMargin`, which is
  only ever called as a bare static-style call in `main()`, never through a real instance)
  is unreachable via a real object. Not pursued — same "real but currently unreachable"
  status already established for `OmegaExitThread`/`CCommDriver::getInstance()`'s
  no-arg-overload/`USTGUserAPI::Disconnect()`.
- **`USTGAPILCDControl`** has 10 other real methods beyond `LoadStoredSettings`
  (`ResetToInit`, `SetContrast`, `SetRGBLevel`, `SetColorTemp`, `SetPadDrive2`,
  `SetBlackLevel`, `SetAllRGBLevels`, `SaveCurrentSettings`, `SetBacklightBrightness`,
  `SetSpreadSpectrumClock`) — all 9 called ones (9 of the 10 have at least 1 real call
  site; `SetAllRGBLevels` has zero) share a **single real caller**:
  `CESGlobalTask::SetLCDCalibration(unsigned char, unsigned char const*)`
  (`.text+0x08c66150`) — which itself has **zero callers anywhere in the export**. Real,
  but two hops deep into unreachable Peg/CForm UI territory (`CESGlobalTask` is the
  `ESGlobal` edit-server mode class `Mains()`'s own `MMainESGlobal` registration shim
  installs a name/vtable-swap placeholder for, never a real constructed instance in this
  reconstruction). Not pursued.
- **`CKernel`** has 5 other real methods beyond the already-reconstructed
  `CKernel`/`~CKernel`/`InitSystemLayer`/`GetSysApi`/`Exec`/`InitUserLayer`/
  `AddGlobalObject`/`RemoveGlobalObject`: `CreateTimer`, `StartTimer`, `StopTimer`,
  `KillTimer`, `Close`. All 5 have **zero callers anywhere in the 37,795-function
  export**, confirmed the same way. `Close()` in particular was worth checking
  specifically (a natural guess for something `COmegaInterface::Close()`/shutdown might
  call) — it does not; `COmegaInterface::Close()`'s own real body is just `s_bRunning = 0`
  (Stage 1), and nothing else calls `CKernel::Close()` either. Not pursued.

This is the same shape of finding Stage 3 already established for `OmegaExitThread`: real,
correctly-named, correctly-signatured methods that simply have no path to them from
anywhere in this binary's own call graph as captured by the static export. Per the task's
own fallback instruction, broadened the search to the next tractable candidate still on
the actual boot path.

### `CCommDriver::setupfifoname()` upgraded Tier-B -> Tier A (`.text+0x08e4f310`, 681 bytes)

Found by re-reading every remaining Tier-B stub comment across `src/`/`include/` for one
sitting directly on an already-reconstructed real call path (same selection method batch
1's own "next candidates" note recommended) — `comm_driver.h`'s own header comment
already flagged this one: `main()` -> `CCommDriver::getInstance(argv)` -> ctor ->
`setupfifoname(argv)`, i.e. directly on the primary boot path, not a peripheral one.

Real per-argv-entry parser: splits each `"NAME=VALUE"`-shaped `argv[]` string on `=` and,
for exactly 3 real names, conditionally assigns the value to one of `CCommDriver`'s 3
fifo-path fields:

| Name | Field | Gate |
|---|---|---|
| `NKS4_LCDFIFO` | `mLcdFifoPath` (+0x00) | `Eva_IsSimulation()` only |
| `NKS4_EVENTSFIFO` | `mEventFifoPath` (+0x04) | `Eva_IsSimulation() \|\| Eva_IsSimulationSVGA()` |
| `NKS4_COMMANDSFIFO` | `mCommandFifoPath` (+0x08) | `Eva_IsSimulation()` only |

Any field still null after the whole `argv[]` scan falls back to one of 3 real hardcoded
default paths (`"/tmp/evaclientfifo"`/`"/tmp/evaeventfifo"`/`"/tmp/evacommandfifo"`,
sizes cross-checked byte-for-byte against the real `new[]` allocation sizes in the
decompile, `0x13`/`0x12`/`0x14`) — **gated by the exact same per-field simulation check**,
not unconditionally. This means, confirmed the hard way (a segfault in this pass's own
first KAT run, caused by a wrong test assumption, not a code bug): **in real hardware mode
(`Eva_IsSimulation()` and `Eva_IsSimulationSVGA()` both false), all 3 fields stay `NULL`
forever, argv values and all** — `CCommDriver` is effectively simulator-only. On real
hardware its constructor becomes a real, faithfully-derived total no-op (all 3 fds stay
`-1`), consistent with this project's own already-established finding that real-hardware
IPC goes through the separate `USTGUserAPI`/`/dev/rtf*` substrate instead (Stage 1/2/4).

**Real bug, confirmed at the raw-disassembly level (not a decompiler artifact)**: every
`argv[]` entry is unconditionally `strchr()`'d for `'='` and the result is dereferenced
with **no NULL check** (`8e4f3b6: call strchr@plt` / `8e4f3bb: movb $0x0,(%eax)`, no
intervening `test`/`je`). Any `argv[]` entry lacking `'='` — including `argv[0]` itself,
the program's own name/path, which this function processes like any other entry —
segfaults here, before `CCommDriver` opens a single fifo. Since real hardware
demonstrably runs Eva successfully, real production `argv` must contain at least one
`"NAME=VALUE"`-shaped entry; this project could not locate Eva's real launch wrapper to
confirm what it actually passes (it lives inside the encrypted `Eva.img`, not present in
any extracted rootfs on this share — `docs/workflow/deploying_patches.md`/
`boot_optimization_analysis.md` both just say `exec /korg/Eva/Eva`, which may be a
paraphrase rather than the literal invocation). Flagged, not "fixed" — adding a NULL
check the real binary doesn't have would misrepresent the function's own contract. **Any
live `kronos_vm` boot test of this reconstruction from here on must invoke Eva with at
least one `argv` entry containing `'='`** to avoid tripping this real bug; this batch did
not perform a live boot test for exactly this reason (no confirmed-real invocation line to
test against) — left for a future pass once/if the real launch wrapper is found.

`Eva_IsSimulation()`/`Eva_IsSimulationSVGA()` (`.text+0x0804cd30`/`0x0804cd40`, 13 bytes
each — real, trivial `return s_eAppMode == N;` accessors) were split out of
`eva_main.cpp` into their own new TU, `src/init/app_mode.cpp` — not because of any
real-binary reason, but because the Makefile's `verify` target deliberately excludes
`objs/init/eva_main.o` from every KAT binary's link (it owns `main()`/`Ouch()`, which would
otherwise collide with each test's own `main()`); keeping the real global (`s_eAppMode`)
and its 2 accessors in their own TU lets every `verify/` KAT link against the real
accessors like any other reconstructed function, rather than needing a second, fake
definition. `include/app_mode.h` is the new shared declaration point.

### Verification

`verify/test_comm_driver.cpp` (new, 12/12 checks) drives the real `setupfifoname()`
directly via a friend hook (`CommDriverTestHooks`, same extraction pattern as
`ustg_user_api.h`/`level_manager_array.h`) on raw, non-constructed `CCommDriver` storage
-- bypassing the real ctor's own `open()` calls entirely, since they're irrelevant to the
function under test and would just spam stderr with "fifo open error" against nonexistent
host paths. `Eva_IsSimulation()`/`Eva_IsSimulationSVGA()` are the real accessors (linked
from `objs/init/app_mode.o`), driven by writing `s_eAppMode` directly, exactly like
`main()`'s own argv[0]-basename detection does. Every synthetic `argv[]` array in this test
deliberately includes at least one `'='`-bearing entry and omits any bare program-name-
style entry, for the same reason documented above -- confirmed real hardware/simulation
gating asymmetry for all 3 fields across hardware mode, plain simulation mode, and
SVGA-only simulation mode (EVENT is the only field also
gated on the SVGA flag), plus the unrecognized-argv-key no-op case.

Two real test-writing bugs caught and fixed while developing this KAT (both instructive,
kept in the header comments): (1) the first draft included a bare `argv[0]`-style entry
with no `'='` in every test array, immediately tripping the real crash bug above — fixed
by removing it, not by adding a NULL check to the function under test; (2) the first draft
assumed hardware mode falls back to the 3 hardcoded defaults, which is wrong -- the
default-assignment gates require simulation mode too, so hardware mode leaves all 3
fields `NULL` (a real finding, not a test framework quirk).

`make` (12/12 new checks, 0 regressions across the other 4 verify binaries + the
concurrent agent's own `test_module_manager_add_module`) and `tools/build_lenny.sh`
(`LINK OK`, real on-image-lib link) both clean. Manifest 116 -> 119 reconstructed
(`08e4f310`/`0804cd30`/`0804cd40`). No live `kronos_vm` boot test this batch -- see the
real-bug writeup above for why.

**Next candidates for a future batch**: locate Eva's real launch wrapper (inside the
encrypted `Eva.img`) to confirm the actual production `argv`, which would resolve the open
question above and unblock a safe live boot test of this specific reconstruction. Beyond
that, continue the same "Tier-B stub already sitting on an already-reconstructed real call
path" search method across the rest of `src/`/`include/` -- `CErrorHandler::EnableUpdate()`
(`.text+0x0805afb0`) and `CSysApiInstance::EnableMultiTask()`/`WriteMessageToHost()` are
both real, on-boot-path-adjacent Tier-B stubs not yet investigated for tractability.

## Stage 6: breadth sweep, batch 6 — 2026-07-25

A broad `nm -C` class-inventory sweep against the real binary (2921 classes with 5+
methods each, filtered against `functions.csv`/`symbols.csv` and cross-referenced for
`grep -rl <Class> src include` misses) — the same technique that found OA.ko's
`CSTGControlMsgHandler` (51 methods, 100% unclaimed) and its 24-method NKS4 event pump
earlier this session. Candidates surveyed, most explicitly deferred as UI/storage-shaped
and out of this pass's own boot-path/system/IPC scope:

| Class | Methods | Verdict |
|---|---|---|
| `CSTGUnsolMsgHandler` | 30 | **Pursued** — IPC message dispatcher, real confirmed boot-path-adjacent caller (see below) |
| `CDDriverIO` | 84 | Skipped — SCSI/CD-ROM optical-drive command set, storage-subsystem depth matching this project's existing `CFileMan`/`CResMan` out-of-scope calls |
| `CControlSurface` | 162 | Skipped — sampled several methods; UI-control-shaped (knobs/sliders/graph widgets), not system/IPC |
| `CDriverTaskBase` | 30 | Skipped — storage driver base class, same depth class as `CDDriverIO`; also risks touching `CModule`/`CTask` territory (ctor takes `CFileMan*`/`COutLinkMono*`) that two concurrent passes this session own |
| `CPoller` | 29 | Skipped — front-panel analog/button/LED client-registration task; genuinely IPC/system-shaped and tempting, but its own ctor is `CPoller(CModule const&, char const*)`, i.e. directly inside the `CModule`/`CTask` family a concurrent agent is actively reconstructing this session — deliberately left alone per this task's own scoping instruction |
| `CClientCommServer` | 26 | Skipped — sampled; no real caller found in a quick xref check, lower confidence than `CSTGUnsolMsgHandler`'s confirmed one |
| `CSysExMsgTaskBase` | 14 | Skipped — not investigated further this batch, noted for a future pass |

`CSTGUnsolMsgHandler` — Eva's dispatcher for unsolicited `STGMessage`s arriving from
OA.ko (`include/stg_unsol_msg_handler.h`, `src/ipc/stg_unsol_msg_handler.cpp`,
`verify/test_stg_unsol_msg_handler.cpp`). Real, non-zero-caller entry point confirmed by
disassembly, not guessed: the one constructor call site (`objdump`-confirmed `call
891c090 <_ZN19CSTGUnsolMsgHandlerC1EPN7CEditor13CPanelIfcTaskE>`) is inside
`CEditor::CPanelIfcTask::CPanelIfcTask(CEditor const&, PegScreen*)`
(`.text+0x0824b7e0`), itself called from `CEditor::Setup()` (`.text+0x08249b60`),
dispatched by `CModuleManager::Setup()`'s own already-reconstructed per-module virtual
call (`src/base/module_manager.cpp`) — i.e. this class sits directly on the (nominal)
module-Setup boot path, not off in unreached UI territory, even though `CPanelIfcTask`
itself is not reconstructed (its real ctor pulls in `CTask`/`COutLinkMono`, explicitly
left to the concurrent `CModule`/`CTask` pass — see below).

**Real class layout** confirmed from the ctor's own decompile, cross-checked against
`CPanelIfcTask`'s own `malloc(0x98)` call site for the object (0x98 = 152 bytes, exact
match): vtable ptr, owner `CEditor::CPanelIfcTask*`, a 17-entry `{code* fn; int adj}`
dispatch table (one slot per `STGMessage` subtype 0..16 — **a newly confirmed fact
about `STGMessage`'s own layout**: its offset+4 field is this subtype index, not
previously documented in `ustg_user_api.h`'s own opaque-`STGMessage` note), a sentinel
dword, and 2 trailing flag bytes. 18 of the class's 30 real methods are Tier A
(faithful): the ctor, both real destructor-shaped functions (kept as plainly-named
methods, not real C++ destructors, matching this project's established "manual vtable
swap, no `virtual` keyword" convention — `COmegaPtrArray`/`CModule`/`CScheduler`),
`HandleMessage()` (the real dispatcher, including the generic both-cases Itanium
ptr-to-member-function dispatch code, faithfully transcribed even though the
vtable-offset branch is dead given this ctor's own even-address-only data),
`EndHandling()`, `SendValueSlider()`/`SendValueEncoder()`, `EnterGlobalObjectEdit()`,
and 8 confirmed-by-reading-every-one genuinely empty `return;` bodies already present
in the shipped binary (5 of them real `static`/cdecl methods, not instance methods —
a real, harmless calling-convention quirk given they're still stored in and called
through the same two-argument instance-dispatch slots as the 15 real instance
handlers). The remaining 12 slots (`ControlMsgHandler`/`GlobalMsgHandler`/
`CombiMsgHandler`/etc., 340–4886 bytes each) are genuinely deep per-subsystem STG
message processing reaching into `CCombi`/`CProg`/`CGlobal`/effect-slot/voice-model
state — Tier-B link-stubs.

**Two small, low-risk additions to the already-owned `CEditor::CPanelIfcTask` class**
(`include/panel_ifc_task.h`, only `SetMargin` previously reconstructed): `GetMargin()`
(real Tier A companion read, no bounds check on the read side unlike `SetMargin`'s
write-side check — preserved as found) and `OnAnalogEvent()`/`OnEncoderEvent()` (real
signatures confirmed directly via `nm -C` — note the real parameter type is
`CPanelOut::SAnalogEvt const*`/`CPanelOut::SEncoderEvt const*`, under a *different*
class's namespace than `CEditor`, not guessed — Tier-B link-stubs, since
`CPanelIfcTask`'s own instance layout/vtable/constructor still aren't reconstructed).
Declaring these as genuine non-static C++ member functions (rather than free functions
bound via an `asm()` label to the real mangled symbol, considered and rejected) gets
the correct implicit-`this`-as-first-stack-argument call shape for free on this
SysV/Itanium ABI, with zero hand-verified-calling-convention risk.

**A real, worth-flagging correction to a concurrent pass's own verdict**: `CTask::CTask()`
does have a caller — `CPanelIfcTask`'s own ctor (`CTask::CTask(this, param_1,
"PanelIfcTask", 3, 1, 0x804b)`) — contradicting `include/task_buffer.h`'s existing note
that it doesn't. Left for the `CModule`/`CTask` pass to reconcile (out of this batch's
own scope, per this task's explicit instruction to stay away from that family); flagged
here rather than silently working around it.

**ABI mechanics worth documenting for future dispatch-table reconstructions**: filling
the raw `{code*, adj}` table entries for 12 real non-virtual instance member functions
with distinct parameter shapes (`const STGMessage&` vs plain `STGMessage&`) can't use a
single pointer-to-member typedef; done here via a small union-based helper
(`AddrOfConstRefHandler`/`AddrOfRefHandler`) that extracts the low word of the Itanium
2-word `{ptr, adj}` member-function-pointer representation — reliable for non-virtual
members on this exact ABI, same "trust the ABI, do the raw thing" license already used
for this project's `CallVSlot1/2` helpers and the manual vtable-swap idiom. The 5 real
`static` handlers need no such trick (plain function-pointer-to-`void*` casts).

Real constants confirmed by direct raw-byte read of the binary's own `.rodata`
(`readelf -l` to map VA→file offset, then read 4 bytes), not asserted from the
decompile's opaque `DAT_` names alone: `_DAT_08ea8534` = `1023.0f`, `_DAT_08f29a40` =
`127.0f` — `HandleMessage()`/`SendValueSlider()`'s own slider-value scale factor,
converting a 0..127 range up to 0..1023.

### Verification

`verify/test_stg_unsol_msg_handler.cpp` (18/18 checks) — a friend hook
(`StgUnsolMsgHandlerTestHooks`, same extraction convention as `ustg_user_api.h`) peeks
the real ctor's dispatch-table wiring and independently re-derives each handler's
expected raw address the same ABI-level way the ctor itself does, confirming the
subtype→handler mapping is correct (not just "doesn't crash"); a garbage
(`0xdeadbeef`) message pointer is passed to all 8 confirmed-empty handlers to catch any
future accidental dereference; `HandleMessage()`'s out-of-range (17) subtype bounds
check is exercised directly.

`make` (18/18 new checks, 0 regressions across every other verify binary) and
`tools/build_lenny.sh` (`LINK OK`, real on-image-lib link against
`RestoreDVD_SystemMNT`) both clean. Manifest 119 → 137 reconstructed.

**Next candidates for a future batch**: `CClientCommServer`/`CSysExMsgTaskBase` (see
survey table above) warrant a proper xref check rather than this batch's quick sample.
Once the concurrent `CModule`/`CTask` pass lands a real `CTask`, revisit whether
`CEditor::CPanelIfcTask`'s own constructor becomes tractable (it would make this
class's boot-path caller fully real end to end, not just "real address, not-yet-owned
base class").

## Stage 6: breadth sweep, `CTask::CTask()` reconstruction batch — 2026-07-25

Directly follows up on batch 6's own flag above: **`CTask::CTask()` genuinely IS
called** in ground truth. Direct `objdump -dr` inspection this batch confirmed TWO real
callers -- `CEditor::CPanelIfcTask::CPanelIfcTask()` (`.text+0x0824b7e0`, matching batch
6's own finding) and, newly found, `CPoller::CPoller(CModule const&, char const*)`
(`.text+0x089ef740`). This corrects Stage 6 batch 2's own verdict ("`CTask::CTask()` has
zero callers anywhere in this reconstruction's call graph -- implementing it would be
dead code") and batch 5's dependent verdict that `CModule::AdjustTaskMask()`'s loop body
is "provably dead... nothing constructs a `CTask`". Per PLAN.md's own verification
methodology (faithfulness judged against ground truth, not against this reconstruction's
own partial call graph), a real, ground-truth-reachable function is a legitimate target
regardless of whether every one of its own callers is reconstructed yet -- the same bar
`CModuleManager::AddModule()` met before `mains.cpp` populated `mModules` (batch 3).

### What changed (uncommitted at time of writing this section; commit hash added on
commit)

1. **`CTask::CTask(CModule const&, char const*, ETaskLevel, EScheduleFlag, unsigned
   short)`** (`.text+0x0807ee80`, 330 bytes) -- new `include/task.h`/`src/base/task.cpp`,
   Tier A. Real 0x7c-byte layout: 2 embedded `COmegaPtrArray`s (link list), owner-module
   pointer, level/lastArg, an Api-vtable-derived scope id (same `+0x3c` call
   `CModule::CModule()` already makes), the mask/period/countdown fields
   `CLevelManager::RunLevel()`/`CModule::AdjustTaskMask()` already expected, an embedded
   `TVector<SRegisteredIfc,1>`, and an embedded `CLimiterMan` sub-object.

   **Real, cross-confirming finding**: the ctor's own mask computation is two-tiered --
   a base value from the `EScheduleFlag` argument (0x04/0x0c/0x0d), bumped by exactly
   +2 (bit 0x02) if the owning module's own `mState` (module.h) is < 4 (not yet
   "started"). Bit 0x02 is the EXACT bit `CModule::AdjustTaskMask()` unconditionally
   clears. Read together: tasks constructed before their module finishes starting come
   into existence pre-masked, and `AdjustTaskMask()`'s own per-module pass is the real
   un-masking mechanism -- a genuine two-phase activation design, not an accidental
   pattern. Confirmed with a real KAT (`verify/test_task.cpp`, case [4]) chaining the
   real ctor, `CModule::Add()`, `CLevelManager::RunLevel()`, and
   `CModule::AdjustTaskMask()` together end to end.

2. **`CLimiterMan::CLimiterMan(CTask*)`** (`.text+0x0807bd10`, 46 bytes) -- new
   `include/limiter_man.h`/`src/base/limiter_man.cpp`, Tier A (ctor only; `~CLimiterMan()`
   not reconstructed, same "construct don't destruct" scope as everywhere else no
   traced caller destroys the enclosing object).

3. **`CModule::Add(CTask*)`** (`.text+0x0807c410`, 91 bytes) -- new method on the
   already-existing `CModule` (module.h/module.cpp), Tier A. **This is the actual
   `mTasks`-populating method neither batch 2 nor batch 5 knew existed.** Real,
   DEFINITIVELY boot-path-reachable caller: `CEditor::Setup()` (`.text+0x08249b60`)
   calls it once per constructed task member, and `CEditor::Setup()` is dispatched by
   `CModuleManager::Setup()`'s own already-real per-module vtable+8 call, from
   `CKernel::InitSystemLayer()` -- the SAME spine `AddModule()`/`AdjustTaskMask()` sit
   on. Also fires 2 new, real, undecoded Api vtable notifications (`system_api.h`:
   `+0x134` with the task, `+0x12c` with the module).

4. **`CTask::RegisterIfc(CIfcUnknown*)`** (472 bytes) stays Tier B -- real dedup-scan +
   `TVector<SRegisteredIfc,1>::MakeCapacity()`-driven append, genuinely deep (the
   `MakeCapacity` growth routine alone is 539 bytes; this project has never
   generalized `TVector<T,1>` growth anywhere it appears, ckernel.h's own note).

5. **`CPoller` surveyed, NOT pursued** (batch 6 flagged it as tempting but
   territory-blocked; now checked with this territory owned): its ctor does call
   `CTask::CTask()` (a real second confirmed caller), but the ctor body itself is
   ~1900 bytes of straight-line handle-table initialization plus a new, not-yet-
   reconstructed `CTask::SetMask(EMask)` dependency and an Api `+0xac` named-resource
   lookup -- deeper than any Tier-A candidate this batch, correctly deferred (same
   several-hundred-plus-byte, pulls-in-further-subsystems shape as
   `CSysApiInstance::RegisterApi`/`CModuleManager::AddModule`'s own precedent).

6. **`CEditor::CPanelIfcTask`'s own ctor stays out of scope** (`panel_ifc_task.h`
   updated) -- its post-`CTask::CTask()` tail does real multiple-inheritance work
   (a second malloc'd `COutLinkMono` sub-object, installed via a `this+8`-adjusted
   secondary vtable pointer) that is Peg/UI-editor-toolkit depth, not
   CModule/CTask/CLevelManagerArray/CPoller family depth. `CEditor` itself (a
   `CModule`-derived class constructed via a not-reconstructed `CEditorConstructor`
   factory object) also stays unreconstructed, so this reconstruction's own call
   graph still does not actually construct a `CPanelIfcTask`/`CTask` on any live path
   -- ground-truth reachability and this-reconstruction's-own-wiring are two
   different questions, and only the first flipped this batch.

7. **`CLevelManager::RunLevel()`'s own task queue is a DIFFERENT container from
   `CModule::mTasks`** -- worth stating precisely, since it would be easy to
   over-claim "the whole chain is now live": `CModule::mTasks` (per-module) is now
   confirmed genuinely populated in ground truth via `CModule::Add()`.
   `CLevelManager`'s own per-scheduling-level array (what `RunLevel()` itself walks)
   is a separate structure; the obvious ground-truth population candidate,
   `CScheduler::InsertTask(CTask const&)` (`.text+0x08062d80`), exists but a full
   disassembly sweep found ZERO direct `call` instructions targeting it anywhere in
   the binary -- left as an open, flagged lead (`level_manager_array.h`), not
   fabricated or assumed reachable. `RunLevel()`'s own "faithful but currently-empty"
   status is therefore unchanged by this batch.

### New vtable-slot arrays (`omega_vtables.h`/`.cpp`)

5 new install-only entries, same installed-pointer-to-next-symbol methodology as
every other entry in this file: `PTR__CTask_08e82128` (7 slots, matching `CModule`'s
own count), `PTR__TNamedPtrArray_08e82198` (3 slots, shared by both of `CTask`'s
embedded `COmegaPtrArray`s), `PTR__TVector_08e82188` (2 slots), `PTR__CLimiterMan_08e81ee8`
(4 slots), `PTR__TVector_08e81f78` (2 slots) -- plus `EvaDataPlaceholder_08e82144`, a
plain safe stand-in for an opaque data blob (`DAT_08e82144`) `CTask`'s own ctor stores
the address of but never dereferences.

### Verification

New `verify/test_task.cpp` (28 checks): both ctor mask-computation branches (fresh vs.
raw `mState>=4` module) across all 3 `EScheduleFlag` cases; `mPeriod`/`mCountdown`/
`mScopeId`; `CModule::Add()`'s real population + both Api notifications, in order; and
the full real chain described in finding 1 above -- a genuine `CTask`, constructed
through its real ctor, appended via the real `CModule::Add()`, correctly SKIPPED by
`CLevelManager::RunLevel()` while pre-masked, un-masked by the real
`CModule::AdjustTaskMask()`, then genuinely ticked by `RunLevel()` on the next call.
This is the first KAT in this project chaining 4 independently-reconstructed real
functions together end to end rather than each in isolation.

`make objs`/`make -k verify`: this batch's own new/changed TUs compile and the new
test passes cleanly (0/28 failed), 0 regressions in every other verify binary that
built. One PRE-EXISTING, NOT-mine verify binary (`test_stg_unsol_msg_handler`) failed
to rebuild this session due to an unrelated, actively-in-progress concurrent edit in
`include/stg_unsol_msg_handler.h` (a stray `CForm*/` in a comment closes it early --
confirmed via file mtime to be a live, uncommitted, in-flight edit by the concurrent
agent explicitly working that class this session, not something introduced here) --
left untouched, not in this batch's scope to fix. `tools/build_lenny.sh` (the real
on-image-lib link) was not run this batch for the same reason (it links every TU
including the currently-broken one) -- deferred rather than worked around. No live
`kronos_vm` boot test: the newly-real chain is provably not yet reachable from this
reconstruction's own currently-wired boot path either (`CEditor`/`CPoller` themselves
still unreconstructed, finding 6 above), so a boot test would show zero new signal,
same reasoning batch 5 used for the same reason.

### Manifest delta

`gen_manifest.py`: added `0807ee80` (`CTask::CTask`), `0807bd10`
(`CLimiterMan::CLimiterMan`), `0807c410` (`CModule::Add`) under a new "Stage 6:
breadth sweep, CTask::CTask() reconstruction batch" section. Regenerated: 120 → 123
of 37,795.
