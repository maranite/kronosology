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
| 5. Peg toolkit substrate | Not started, not yet known to be necessary |
| 6. Breadth sweep | Not started, out of scope for this effort |

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
succeeds with zero unresolved symbols, no regression from Stage 4. Not executed in any
VM as part of this pass (no SSH access from this task) — the resulting binary is
expected, but not yet confirmed, to get past the `MMainEditMan()` crash on the next
live boot test.
