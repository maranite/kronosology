# OA.ko (OA_322.ko) — reconstructed source

Drop-in source reconstruction of the Korg Kronos firmware-3.2.2 **`OA_322.ko`** — the
STG synthesis engine + copy-protection layer (Linux 2.6.32.11 + RTAI, x86-32, C++).

**22,195 functions.** This is a staged, verified, loop-driven effort — see
[`PLAN.md`](PLAN.md) for the full strategy and the per-function verification
methodology (coverage manifest + recompile/BSim + emulation/known-answer tests).

## Layout

- `PLAN.md` — the plan + verification methodology
- `include/` — recovered type model (class structs → headers)
- `src/<subsystem>/` — reconstructed `.cpp`, in staged order (auth first)
- `manifest/oa_functions.csv` — every function + `status` (regenerated from Ghidra; gitignored, regenerable)
- `verify/` — BSim/emulation harness + crypto known-answer vectors

## Progress

| Stage | Status |
|---|---|
| 0. Foundations (tree, manifest, plan, verify harness) | manifest ✅ · plan ✅ · harness ⏳ |
| 1. Copy-protection / auth | **fully reconstructed** ✅ — KLM stamping, AT88 handshake, product glue, AuthorizationStrings/cdrom-check, `/proc/.oacmd` procfs plumbing, and `ProcessOACmd` (all 12 real commands, including LM/LD/CM/CD's exact byte-for-byte dispatch, no remaining simplifications) |
| 2. Shared utilities | **fully implemented + KAT-verified** ✅ — crypto/hash primitives (`moancjsd82`, `DecodeBytesFromAscii`, `md5`), `CSTGBankMemory` heap, `CSTGQuad`/list primitives, the `Scale*` leaf math family, and the `operator new`/`delete` allocator substrate; every item PLAN.md named for this stage is done |
| 3. Engine core | **`init_module`'s full call graph reconstructed** ✅ — every one of `init_module`'s 17 real steps, `CSTGEngine::Initialize()`, and `CSTGGlobal`'s own 3124-byte constructor are done; the unresolved-symbol audit (`MASTER_REFERENCE.md` sec 10.119-10.121) confirms the remaining 32 unresolved symbols in a from-scratch Kbuild rebuild are ALL genuine, correctly-modeled external dependencies (kernel/RTAI/hardware), not reconstruction gaps — `OA.ko` builds clean end-to-end. `CSTGGlobal`'s broader ~195-method sweep (message handlers, performance-change/setlist-activation chain, etc.) is well past the earlier "46 of ~195" figure; see `MASTER_REFERENCE.md` sec 10.67 onward (not mirrored here blow-by-blow) for the full, current log. |
| 4. Voice models & DSP | pending |
| 5. Breadth sweep | pending |

**Note**: this file's own prose below stops narrating in detail around
sec 10.66 of `MASTER_REFERENCE.md` — that file (not this README) is the
authoritative, continuously-updated record of every subsequent
reconstruction batch, the Bar 2 (`kronos_vm` boot) investigation, and
the unresolved-symbol audit. Read its tail (`sec 10.6x` onward) for the
true current state before assuming this README is up to date.

**Priority pivot (2026-07-01), status as of 2026-07-04**: focus moved from the
`CSTGGlobal` method-by-method sweep to getting `OA.ko` to actually `insmod`
successfully in `kronos_vm` ("Bar 2") — because Eva (the on-screen UI, a separate
userspace binary with no synthesis code) launches if and only if `init_module`
returns 0. That work found and fixed two real bugs that would have crashed a live
boot (`COmapNKS4Driver_GetOmapVersion`/`GetPSocVersion`'s calling convention, and
every `printk`/`rt_printk` call's wild-pointer format-string argument — sec
10.119/10.120), then got a real boot attempt past `insmod` and into
`init_module()` itself for the first time ever, only to hit a kernel-image-level
`%fs`-segment/per-CPU-area bug unrelated to any code this project has written
(sec 10.122 — paused, not resolved, in favor of continued source reconstruction).
That source reconstruction continued through `MASTER_REFERENCE.md` sec 10.141
(the `CSTGGlobal` performance-change/setlist-activation cluster). **Sec 10.142
(2026-07-04)**: a routine from-scratch Kbuild rebuild at the start of this
session caught a real regression — this host's default-PIE `g++` needs
`-fno-pie` alongside the existing `-fno-pic` fix (sec 10.122), or a spurious
`_GLOBAL_OFFSET_TABLE_` dependency reappears in every rebuilt `.ko`, which would
itself have crashed `insmod`. Fixed at the shared `linux-kronos` tree level;
confirmed via rebuild that all four reconstructed C++ kernel modules now emit
zero GOT relocations, `OA.ko`'s unresolved-symbol count is back to exactly 32,
and `.gnu.linkonce.this_module` is still byte-exact. **Sec 10.143 (same day)**:
reconstructed the small `CSTGProgramSlot::IsActive`/`AccessActiveSlotVoiceData`/
`HasActiveSlotVoiceData`/`HasActiveVoices` family, and separately found that
`make` (the host codegen check) followed by `make ko` with no `clean` in
between silently reintroduced sec 10.142's own GOT regression — the host check
and Kbuild shared identical `.o` output paths, so Make's staleness check reused
the wrong-flags host objects. Fixed by giving the host check a distinct
`.hostobj` suffix in `OA`'s, `AT88VirtualChip`'s, and
`KorgUsbAudioVirtualDriver`'s Makefiles so the two build modes can no longer
collide. All 37 `OA` host suites pass; `OA.ko` rebuilds with 32 unresolved
symbols, 0 GOT relocations, byte-exact module header, in either build order now.
**Sec 10.144 (same day)**: reconstructed 8 more small manager methods from the
`bar2_stubs.cpp` sweep (`CSTGMonitorMixer`/`CSTGHDRFileWriter`/
`CSTGSamplingDaemon`/`CSTGFileCloser`/`CSTGCDWorker`'s own `Initialize()`s,
`CSTGHDRManager::ProcessCommands()`, `CSTGPerformance::IsCurrentlyActive()`,
`CSTGPCMPrecacheManager::Initialize()`), plus one real signature bug fixed
(`CSTGPCMPrecacheManager::Initialize()` was declared `void` but the real
disassembly returns `bool true` unconditionally — its one caller just
discards it, hence the miss). Promoted every affected test file's own
pre-existing mocks to real backing state or plain link stubs as appropriate.
All 37 host suites still pass; `OA.ko` still 32 unresolved symbols, 0 GOT
relocations, byte-exact module header.

### Stage-1 auth cluster

The copy-protection core, recovered from the symbol table + string references +
disassembly (`kronosology/docs/modules/OA.ko_auth.md`), all addresses
ground-truthed against the real OA.ko 3.2.1 ELF symbol table (see
`MASTER_REFERENCE.md` sec 8-9.2 — several `OA.ko_auth.md` addresses carried an
uncorrected `+0x10000` Ghidra image-base bias, now fixed below):

**Reconstructed (`src/auth/`):**
- **`CSTGKLMManager`** (Korg license manager) @ `0x2de10` — `AuthorizeProduct`,
  `AuthorizeVoiceModel/Effect/MultisampleBank/Builtins`, `IsAuthorizedVoiceModel/
  MultisampleBank/Effect`, `GetKLMAddressForPatch`, `RunKLM`. (`klm_manager.cpp`)
- **`SetupAtmelForAuthorizations`** @ `0x207a50` — Atmel NV2AC chip GPA handshake.
  (`atmel_setup.cpp`)
- **`CSTGInstalledEXProducts::AuthorizeProductByFilename`** @ `0x481d0`,
  **`AuthorizeProductCallback`** @ `0x47fa0` — CD-ROM/install-path glue.
  (`products.cpp`)
- **`InitCdromSupport`** @ `0x40` — loadmod.ko presence/integrity check, plus
  the `CSTGEngine::Initialize` (`0x1b0`) degradation block it gates,
  reproduced as a standalone helper for Stage 3 to call. (`cdrom_check.cpp`)
  — **CORRECTED (2026-07-01)**: this file previously invented a fictional
  3-arg "init_cdrom_command" function and a placeholder "g_pCdromDrvInfo"
  global. Re-disassembled from scratch: the real call target is the actual
  Linux kernel API `register_cdrom(struct cdrom_device_info *)`, hijacked by
  Korg's patched kernel as a covert integrity channel; the recovered pointer
  (via a `- 0x02D5B9C3` subtraction step this project had missed entirely)
  is stored into the real confirmed globals **`sXCmd`**/**`sCdromCommand`**,
  then checked for the `0x22fb39cc` magic dword at `+5`.
- **`ParseAuths`** @ `0x207c50` / **`ParseAuth`** @ `0x207890` — boot-time
  `/korg/rw/Startup/AuthorizationStrings` reader, callback =
  `AuthorizeProductCallback` (confirmed via relocation in
  `CSTGInstalledEXProducts::Initialize`). (`parse_auth.cpp`) — **fully
  disassembly-confirmed**, not paraphrase: ParseAuth decodes to exactly 15
  plaintext bytes (no 16-byte UUID exists on this path) and its callback
  receives a plain 4-character product code, matching
  `AuthorizeProductCallback(const char *code4)` exactly.
  **CORRECTED FINDING**: `ParseAuths` reads the AT88 dongle itself (zones
  `0x10`/`0x18`/`0x20`, 24 bytes, via `fFfFfFfFfFfF13`) before touching any
  token and aborts entirely if that read fails — the "no dongle required"
  characterization (from `OA.ko_auth.md` and this project's own earlier notes)
  does not hold up against the disassembly.
- **`VerifyAuthorizationString`** @ `0x207de0` — runtime, front-panel-UI
  validate-only path (`callback=NULL`); reads the identical three AT88 zones
  via the same `fFfFfFfFfFfF13`, `DecodeBytesFromAscii`s the string directly
  (no tokenizing needed), then calls `ParseAuth`. (`verify_auth_string.cpp`)
  — fully disassembly-confirmed.
- Per-patch checks: `CSTG*ModelPatch::IsUsingAnyUnauthorizedMultisamples`
  (not yet reconstructed — Stage 3/4, called from voice-model/patch code).
- **`ProcessOACmd`** @ `0xa0c0` (1773 bytes) — the `/proc/.oacmd` command
  dispatcher, **fully reconstructed, byte-exact, all 12 real commands**.
  (`process_oacmd.cpp`) — the dispatch skeleton and the
  `AU:` (authorize) case are fully disassembly-confirmed and route through
  the new **`CSTGInstalledEXProducts::VerifyAndSaveAuthString`**
  (`.text+0x48290`, `products.cpp`) — the missing link between this
  dispatcher and the already-reconstructed `VerifyAuthorizationString`/
  `ParseAuth` chain, with a faithfully-preserved quirk: on the file-append
  success path the function's return value is actually the trailing-newline
  write's result, not the authorization result.
  **`LM`/`LD`/`CM`/`CD` fully untangled and corrected (2026-07-01)** — an
  earlier pass modeled `CM`/`CD` as "close" operations
  (`LoadBankMetaData`+`ReleaseBank`) based on relocation *order* alone.
  Re-disassembling `.text+0xa2a0`–`.text+0xa4f8` in full found this wrong:
  **`CM`/`CD` are not "close" commands at all** — they call the exact same
  `LoadMultisample`/`LoadDrumSample` as `LM`/`LD` (confirmed by the identical
  relocated symbol at both call sites), differing only in a hardcoded final
  boolean argument (`0` for `LM`/`LD`, `1` for `CM`/`CD`) whose real meaning
  isn't determinable from the call site alone (named `variant` rather than
  guessed). All four also share an identical special case: if `AccessBank`
  returns a bank in a "reserved but not yet loaded" marker state
  (`*(int*)bank == -1`), first call `LoadBankMetaData()`; on success, retry
  the normal load (same `variant`); on failure, `ReleaseBank` and fail. The
  "two near-identical call sites per command" a prior pass noticed via
  relocation order are exactly this direct-load-vs.-retry-after-metadata
  pair, not duplicated logic. This is now a byte-exact reconstruction of the
  real compiled dispatch, not a simplified approximation.
  **`CB:*`, `SO:*`, `PT`, `PC`, `CL:<uuid>` fully disassembly-confirmed
  (2026-07-01)** — and a real correction along the way: an earlier pass
  wrongly attributed `CloseAllBankFiles` to `CL`. Re-disassembling the region
  past `CL`'s match (which the earlier pass only ever inferred from
  relocation *order*, never actually read) found `CL:<uuid>` closes ONE
  bank's PCM files (`ClosePCMDataFiles`, exactly 39-char command, no
  trailing numbers unlike LM/LD/CM/CD); the literal 4-byte command `CB:*`
  (not a "2-char prefix" at all — `SO:*`/`LA:*` are the same kind of fixed
  4-byte literal match) is what actually closes *all* banks
  (`CloseAllBankFiles`). `docs/interfaces/proc_oacmd.md`'s "CL: Close all
  bank files" description is imprecise for the same reason. `SO:*` calls
  `CSTGInstalledEXProducts::ReInitialize` (also corrected: returns `bool`,
  not `void` as an earlier pass guessed, per the same `(result^1)&0xff`
  convention as `AU:`). `PT` calls `CSTGPianoModel::RescanPianoTypes` with no
  argument parsing at all. `PC` requires `strlen > 7`, parses
  `":%lu:%lu:%lu"`, and calls `CSTGPCMPrecacheManager::Reset` with a
  confirmed-but-counterintuitive argument order (see the struct's own
  comment in `process_oacmd.cpp` — the parsed numbers do not map to `Reset`'s
  parameters in the order they appear in the command string).
  **Bonus fix**: found and fixed a real bug in `oa_heap.h`'s `oa_heap_base()`
  while working in this area — it checked for a `NULL`
  `CSTGHeapManager::sInstance`, but every real call site (including the
  already-reconstructed `CSTGKLMManager::AuthorizeMultisampleBank`) guards a
  specific sentinel value, `-44` (`0xFFFFFFD4`), not zero.
  **`KI`, `LA:*` reconstructed and `ProcessOACmd` fully disassembled
  end-to-end (2026-07-01)**: `KI` parses `":%lu"` and — uniquely among every
  command here — writes the value directly into a heap-relative field
  (`heapbase+0x6a554`, exactly 8 bytes past `CSTGPCMPrecacheManager`'s own
  base) rather than calling a method; `LA:*` (a literal 4-byte match, like
  `CB:*`/`SO:*`) calls `CSTGPCMPrecacheManager::AfterProcess()` (return type
  also corrected: `bool`, not the earlier guessed `void`).
  **Doc correction**: `docs/interfaces/proc_oacmd.md` lists a "PR" command
  ("post-process / AfterProcess") that does not exist — every string in
  `ProcessOACmd`'s rodata was read directly and the complete real command
  table is exactly `LM, LD, CM, CD, AU, CL, CB:*, PT, SO:*, PC, KI, LA:*` (12
  commands). `AfterProcess` is dispatched by `LA:*`, not a separate "PR".
- **`oa_cmd_open`/`oa_cmd_close`/`oa_cmd_read`/`oa_cmd_write`** @
  `.text+0x9e60/0x9e80/0x9eb0/0x9f20`, **`oa_cmd_fops`** @ `.data+0x4e0`,
  **`ParseOACmd`** @ `.text+0xa020`, **`InitPcmModProcInterface`**/
  **`CleanupPcmModProcInterface`** @ `.text+0xa060/0xa0a0` — the `/proc/.oacmd`
  procfs plumbing sitting on top of `ProcessOACmd`. (`oa_cmd_proc.cpp`) —
  **fully disassembly-confirmed**, including a bonus find: `ParseOACmd` is a
  *second*, simpler entry point (also calls `ProcessOACmd`, also updates the
  state machine) distinct from the `oa_cmd_write` path — its external caller
  wasn't identified in this pass. `oa_cmd_fops`'s field layout is confirmed
  via relocation to be the real, unmodified Linux 2.6.32
  `struct file_operations` (read/write/open/release at the stock offsets).
  `/proc/.oacmd` is created mode `0600`, uid=gid=`500` ("pocky"), matching
  the prior CLAUDE.md finding exactly. State machine: `sOACmdStatus`
  (0 IDLE / 1 READY / 2 PROCESSING / 3 RESULT) and `sOACmdResult` (the
  4-byte result `oa_cmd_read` hands back) — both real confirmed globals, now
  owned by this file (`ProcessOACmd` itself only ever receives
  `&sOACmdResult` as a plain out-parameter, confirmed from disassembly, so
  it doesn't need to know about the global).

**Shared crypto/hash primitives** (`oa_crypto.h`/`oa_md5.h` / `src/crypto/`,
Stage 2 — all software primitives now **implemented**, not just declared):
- `md5_init`/`md5_append`/`md5_finish` (new, 2026-07-01) — real, locally
  defined symbols in OA.ko (`.text+0x4f57d0`/`0x4f5800`/`0x4f5900`), used by
  `ParseAuth`'s MD5 cross-check (`parse_auth.cpp`) but left as unimplemented
  externs when that file was first reconstructed — this closes that gap, so
  the whole `AU:`/`ParseAuth` chain is now functionally complete, not just
  structurally complete. Ground-truthed via disassembly to be the well-known
  public-domain "L. Peter Deutsch" MD5 reference implementation, compiled in
  unmodified (same pattern as `moancjsd82` turning out to be stock
  Blowfish): the confirmed context layout (`count[2]` at `+0x00`, `abcd[4]`
  at `+0x08` with the canonical RFC 1321 initial constants) and the
  confirmed padding/finish sequence in `md5_finish`'s disassembly are an
  exact match. **Implemented** in `src/crypto/md5.cpp`. KAT-verified
  (`verify/test_crypto.cpp`) against the official RFC 1321 Appendix A.5 test
  suite (7 vectors) plus a chunked-vs-single-call append consistency check
  (exercises the multi-block buffering path the RFC vectors alone don't reach).
- `fFfFfFfFfFfF13(zone, len, buf)` — AT88 zone read, confirmed real symbol.
  Real hardware I/O (`OmapNKS4Module.ko`) — call contract only, out of scope
  for a software port.
- `DecodeBytesFromAscii(out, asciiIn)` — the Crockford Base-32 decoder,
  confirmed real symbol; also where the ">= 0x18 bytes" minimum-length gate
  actually lives. **Implemented** in `src/crypto/cb32.cpp`, ported from
  `UIAppGen/Core/Cb32Codec.cs` (which its own doc comment states matches
  OA.ko's `DecodeBytesFromAscii` exactly). KAT-verified against an
  independent from-scratch Python implementation.
- `moancjsd82(chipKeyMaterial, ciphertext, p3, plainOut)` — Blowfish-CFB-64,
  disassembled directly (`.text+0x4f5f00`): treats its first argument as a
  16-byte circular key region (standard Blowfish S-box key schedule).
  **Implemented** in `src/crypto/moancjsd82.cpp` on top of a standard
  Blowfish port (`src/crypto/blowfish.cpp`, P/S-box constants extracted
  programmatically from this exact kernel tree's `linux-2.6.32.11/crypto/blowfish.c`).
  KAT-verified (`verify/test_crypto.cpp`) against a real hardware-extracted
  AT88 key/iv and a ciphertext independently produced/checked by
  `UIAppGen.Tests/BlowfishCfb64Tests.cs` (itself checked against
  `KronosExtract/build/kronos.py`). Hardware-verified end-to-end via
  `Tools/expansion_tools/kronos_auth.py` for the EXs library installer path
  (Korg-issued + self-generated keys both
  authorize on real hardware, 2026-04-19).

**`CSTGBankMemory` heap** (`include/oa_bank_memory.h` / `src/mem/bank_memory.cpp`,
new 2026-07-01) — a small, static bump/arena allocator used throughout the
synthesis engine for bank-associated allocations. **Fully disassembly-confirmed**,
all three methods tiny (31/6/37 bytes) and genuinely static (no `this` at
all — confirmed since `AllocAligned`'s first register argument is its real
`size` parameter, not an instance pointer):
- `Initialize(base, size)` — 16-byte-aligns `base` UP and `size` DOWN, resets
  the allocation offset to 0.
- `SetTotalBytesToManage(size)` — overwrites the manageable-size global
  directly, confirmed to skip the re-alignment `Initialize` does (a distinct
  call site, not a shared helper).
- `AllocAligned(size, alignment)` — a straightforward bump allocator:
  `ptr = align_up(base + offset, alignment); offset = (ptr - base) + size;
  return ptr`. **Confirmed finding**: there is no bounds/overflow check
  against the total-available-size global anywhere in this function —
  callers are trusted not to overrun the pool. That total-size global is
  read elsewhere purely for diagnostics, in a separate, much larger,
  not-yet-reconstructed function (`CSTGMultisampleBank::
  GetTestBankMemoryUsage`, `.text+0x33f70`) that almost certainly reports a
  used/total utilization statistic.

KAT-verified (`verify/test_bank_memory.cpp`) against hand-traced arithmetic
derived directly from the confirmed algorithm (no third-party reference
exists for this Korg-internal bookkeeping, unlike the crypto primitives) —
each expected value is worked out step by step in the test's own comments so
a reader can check it independently, not just re-run through the same
implementation.

**`CSTGQuad`/list primitives** (`include/oa_quad.h` / `src/mem/quad_list.cpp`,
new 2026-07-01) — a per-priority-bucket, ascending-priority-sorted
doubly-linked list used throughout the synthesis engine's DSP graph.
Unlike every other unit reconstructed so far, `CSTGQuad` and its list
container are never their own standalone functions in the binary — a small
value type plus a fully-inlined template (`TListLinkLite<CSTGQuad>`) leave
no single symbol to point at as "the class". The layout and algorithm below
are reconstructed entirely from disassembling the two functions that
manipulate it: `CSTGVoiceModel::AddQuad`/`RemoveQuad`
(`.text+0x1a9c70`/`0x1a9d30`, 188/110 bytes, fully disassembly-confirmed).
- `mNext`/`mPrev` (`+0x00`/`+0x04`) — standard doubly-linked-list pointers.
- `mOwnerList` (`+0x08`) — which bucket a quad currently belongs to, `NULL`
  if unlinked (`RemoveQuad` no-ops silently in that case).
- `mPriority`/`mBucketIndex` (`+0x14`/`+0x16`, 2 bytes each) — the sort key
  and which `CSTGQuadList` bucket (12 bytes: head/tail/count) in the owning
  object's bucket table this quad belongs to.
- `AddQuad` inserts sorted ascending by `mPriority`; ties insert the *new*
  quad before the existing equal-priority one (confirmed by hand-tracing
  the exact comparison operators, not assumed).
- Both methods also maintain a 2-byte "last-touched quad's priority" cache
  on the owning `CSTGVoiceModel` (`+0xd8`, added to its declaration in
  `oa_types.h`) — `AddQuad` always overwrites it; `RemoveQuad` resets it to
  the sentinel `0xffff` only if it currently matches the removed quad's own
  priority. **Faithfully preserved quirk**: the real comparison mixes a
  sign-extending load of the cache against a zero-extending load of the
  quad's priority — indistinguishable from a plain compare for priorities
  under `0x8000` (the expected range), reproduced exactly rather than
  "cleaned up" to a uniform comparison.
- **Confirmed NOT reconstructed here** (exist via relocation, genuinely
  higher-level than "list primitives"): `CSTGVoiceModel::MoveQuadToCPU`,
  `CSTGVoiceAllocator::FreeQuad`, `CLoadBalancer::NotifyQuadAllocated/Freed`
  — Stage 3/4 territory.

KAT-verified (`verify/test_quad_list.cpp`) the same way as `CSTGBankMemory`
— hand-constructed insertion sequences with expected list order worked out
by hand (including a tie-break case and both head/tail removal edge cases),
not just round-tripped through the same implementation. One sequencing bug
was caught and fixed while writing the test itself: the priority cache is
scoped to the *voice model*, not the bucket, so an early draft that
inserted into a second bucket partway through the test silently invalidated
later cache assertions — reordered so the cache-dependent checks run before
the unrelated bucket-independence check.

**`Scale*` leaf math family** (`include/oa_scale.h` / `src/mem/scale.cpp`,
new 2026-07-01) — six tiny linear-rescale/lerp helpers: `ScaleLong`,
`ScaleShort`, `ScaleWord`, `ScaleByte`, `ScaleChar` (49-100 bytes each, all
disassembly-confirmed to share one algorithm differing only by
width/signedness) and `ScaleLongDouble` (94 bytes, a `long`-width sibling
that does the same math via the x87 FPU in double precision instead of
32-bit integer ops, to avoid the `long` version's int32 overflow on wide
ranges). All map `value` from `[inMin,inMax]` to `[outMin,outMax]`:
`outMin + (value-inMin)*(outMax-outMin)/(inMax-inMin)`, with a confirmed
degenerate case when `inMin==inMax` that **skips the division entirely**
(not treated as dividing by 1) — a faithfully-preserved quirk, not "fixed."
A single C++ template (`ScaleInteger<T>`) implements all five integer
variants: C++'s own integer-promotion rules for narrower types reproduce
the exact movzx/movsx-to-32-bit widening the real disassembly does before
its `imul`/`idiv`, so one generic implementation is faithful, not merely
convenient. `ScaleLongDouble`'s FPU control word was decoded to confirm its
rounding mode is truncate-toward-zero (RC=11), matching a plain C++
`(long)` cast from `double` exactly — no special rounding call needed.
KAT-verified (`verify/test_scale.cpp`) with hand-computed vectors, including
the degenerate case and an overflow scenario that only `ScaleLongDouble`
gets right. **Not reconstructed** (bigger/more specialized, not "leaf"):
`ScaleValToIndex`, `ScaleWhiteBlackCC`, `ScaleRTParmValue`.

**`operator new`/`delete` allocator substrate** (`include/oa_new_delete.h` /
`src/mem/new_delete.cpp`, new 2026-07-01) — PLAN.md's "TMP allocators" item;
the closest real match to that name, since OA.ko is a freestanding kernel
module (`-fno-exceptions`, no libstdc++) that defines its own global
`operator new`/`new[]`/`delete`/`delete[]` from scratch. All four are tiny
(15-byte) trampolines, disassembly-confirmed via their own relocations to
forward to two more real, locally-defined OA.ko functions, `stg_kmalloc`/
`stg_kfree` (20/15 bytes) — which are themselves thin: their own relocations
confirm they call straight into the real Linux kernel's
`__kmalloc(size, GFP_KERNEL)` (the `0xd0` flag value decoded and confirmed
to be exactly `__GFP_WAIT|__GFP_IO|__GFP_FS`, i.e. plain `GFP_KERNEL`, in
this 2.6.32 kernel's encoding) and `kfree(ptr)`. No bespoke pool/arena logic
of its own at this layer — that's `CSTGBankMemory`/`CSTGHeapManager`, above
it. `__kmalloc`/`kfree` are genuine kernel APIs, declared `extern "C"` for
the target-ABI compile check with no KAT for them specifically (same
treatment as `register_cdrom()` in `cdrom_check.cpp`) — but the *forwarding
contract itself* (exact size/flags/pointer passthrough down every layer) is
KAT-verified (`verify/test_new_delete.cpp`) against host-side mock
`__kmalloc`/`kfree` definitions. Uses `size_t` (via a `__SIZE_TYPE__`-based
typedef, no header needed so it stays freestanding-safe) rather than a
literal `unsigned int`, even though the disassembly's argument is a plain
32-bit register — on the real 32-bit target these are the same type
(confirmed by `operator new`'s mangled name, `_Znwj`, matching exactly).

This subsystem talks to the Atmel **NV2AC** security chip (via `stgNV2AC_*` in
`OmapNKS4Module.ko`) and uses **GMP** big-integer maths (`__gmpz_powm`/`invert` from
`STGGmp.ko`) — i.e. it ties together the two already-reconstructed modules.

### Stage 3: engine core (started)

**`CSTGEngine`** (`include/oa_engine.h` / `src/engine/engine.cpp`, new
2026-07-01) — the STG synthesis engine's top-level object. Confirmed via the
ELF symbol table to have exactly 7 distinct methods (ctor/dtor + 5 real
methods, no others exist):

| Method | Address | Size | Status |
|---|---|---|---|
| `CSTGEngine()` | `.text+0xe0` | 10 B | ✅ implemented |
| `~CSTGEngine()` | `.text+0xf0` | 191 B | ✅ implemented |
| `RunAudioTick(unsigned int)` | `.text+0xc10` | 44 B | ✅ implemented |
| `PostAudioTick()` | `.text+0xc40` | 94 B | ✅ implemented |
| `RunEffects()` | `.text+0xca0` | 20 B | ✅ implemented |
| `RunFileDaemonSynchronization()` | `.text+0xcc0` | 90 B | ✅ implemented |
| `Initialize()` | `.text+0x1b0` | 1901 B | documented, not compiled (see below) |
| `PreAudioTick()` | `.text+0x920` | 741 B | documented, not compiled (see below) |

The six small methods are pure "call these singletons, in this exact
order" dispatchers, fully disassembly-confirmed with no ambiguity — KAT-
verified (`verify/test_engine.cpp`) by asserting the exact call sequence
against mock manager implementations, the same call-order-log technique
used for nothing else in this codebase so far (everywhere else has been
arithmetic to check; here the thing worth checking is *sequencing*). One
genuine bug surfaced *while writing the test*, not in the reconstruction:
letting a stack-allocated `CSTGEngine` used for tests 1-4 fall out of scope
re-ran its destructor a second time against already-torn-down singleton
pointers from test 5 (this is actually *correct*, faithful behavior — the
real destructor doesn't null out most singleton pointers either, since a
real `CSTGEngine` is only ever destructed once at shutdown) — fixed by
heap-allocating that test object and deliberately never freeing it.

A nice confirmed find in the destructor: it cross-checks cleanly against
`Initialize()`'s construction order. Exactly four classes
(`CLoadBalancer`, `CSTGAudioDriverInterfaceKorgUsb`, `CSTGAudioManager`,
`CPowerOffTimer`) are real heap-`new`'d in `Initialize()` (confirmed via
`_Znwj` relocations) — and those are exactly the four the destructor tears
down with a real dtor-call-then-`operator delete` (or, for
`CSTGAudioDriverInterface`, a virtual `delete` through its vtable, since the
real allocated object is the derived `KorgUsb` subclass). Every *other*
manager `Initialize()` constructs is placement-new'd directly into
`CSTGBankMemory::AllocAligned` memory, and the destructor correspondingly
only in-place-destructs them (no `operator delete`) — a clean, load-bearing
distinction the disassembly makes obvious once you check both functions
against each other, not something guessable from either alone.

`Initialize()` and `PreAudioTick()` are **fully disassembled and their
structure fully confirmed**, but deliberately not added to the compiling
tree yet: `Initialize()` constructs ~40 manager/model singletons (via
`CSTGBankMemory::AllocAligned` + placement-new, exact byte size confirmed
per class — sizes range from 16 bytes to 282,284 bytes) in a fixed order,
and `PreAudioTick()` makes ~40 sequential per-tick calls into those same
singletons. Writing either for real means declaring ~40 manager classes
(`CSTGAudioManager`, `CSTGVoiceAllocator`, `CSTGEffectManager`,
`CSTGHDRManager`, `CSTGWaveSeqManager`, `CSTGVectorManager`,
`CSTGMidiDispatcher`, and so on) that are themselves the bulk of Stage 3's
remaining work — stubbing bare forward declarations just to make these two
functions "compile" would overstate how much of each manager is actually
reconstructed. The full confirmed order/sizes/class list is recorded in
`MASTER_REFERENCE.md` sec 10.13 as a durable reference for whoever
reconstructs each manager next; PLAN.md's "always compilable" principle is
satisfied by simply not including these two bodies yet, not by faking their
dependencies.

**`CSTGGlobal`** (`include/oa_global.h` / `src/engine/global.cpp`) — the
engine's central "global state" object. Has ~195 methods and a 3124-byte
constructor, far too large to reconstruct in one pass; only the pieces
`CSTGEngine`'s own (fully reconstructed) methods actually call have been
touched so far:
- **`IncrementMicrosecondCount()`** (`.text+0x93b0`, 74 bytes) — **fully
  implemented and KAT-verified**. A confirmed, precise find: it maintains a
  64-bit microsecond counter using a 4-phase Bresenham-style fractional-rate
  accumulator (adds 667 three calls out of four, 666 on the fourth) —
  `3×667 + 1×666 = 2667`, `2667/4 = 666.75` exactly, meaning this is called
  at precisely 1500Hz and tracks elapsed microseconds exactly despite
  666.75 not being a whole number.
- **`RunVoiceModelFeedback()`** (`.text+0x4690`, 123 bytes) and
  **`SetCurrentModeTempo(float)`** (`.text+0x4b20`, 90 bytes) — both fully
  disassembled and confirmed to exist, declared (so `CSTGEngine::
  PostAudioTick` compiles against them) but deliberately **not**
  implemented: `RunVoiceModelFeedback` walks into an unrecovered
  `CSTGSlotVoiceData`-shaped object and calls an unidentified virtual
  function, and `SetCurrentModeTempo` computes `log2(tempo/120)` (the
  constants `1/120`, `+16.0`, `-16.0` were extracted from their real
  `.rodata` values and confirmed) clamped to `[-16,16]`, but its exact
  FPU-conditional-move branch structure wasn't resolved with enough
  confidence in this pass to ship as correct rather than merely plausible —
  left honestly undone rather than guessed.

Confirmed, and noted for whoever reconstructs `CSTGGlobal` further:
CSTGGlobal's real object is enormous — every confirmed field offset so far
lands around `0x29c9900`-`0x29c9fc0`, i.e. roughly **43.6 MB** into the
object. It almost certainly embeds large audio/sequencer buffers directly
rather than holding pointers to them, so a from-scratch struct layout isn't
realistic; methods here use raw `(unsigned char *)this + OFFSET` arithmetic
instead, the same treatment `CSTGVoiceModel`/`CSTGMultisampleBank` got
before their layouts were known.

**Manager constructors** (`src/engine/managers.cpp`, new 2026-07-01, extended
same day after the boot-dependency/AT88VirtualChip detour) — 21 of the ~40
classes named in `Initialize()`'s confirmed construction table
(`MASTER_REFERENCE.md` sec 10.13-10.16, 10.24-10.29) have been picked off,
bottom-up, as a first pass at the "declare 40 manager classes" work
`Initialize()`/`PreAudioTick()` are still blocked on: `CSTGDiskCostManager`,
`CSTGSamplingDaemon`, `CSTGHDRFileReader`, `CSTGHDRFileWriter`,
`CSTGStreamingFileReader`, `CSTGFileCloser`, `CSTGMetronome`,
`CSTGTempoUtils`, `CSTGFileOpener`, `CSTGCDWorker`, `CPowerOffTimer`,
`CSTGAudioDriverInterfaceKorgUsb`, `CSTGVoiceModelManager`, `CLoadBalancer`,
`CSTGMonitorMixer`, `CSTGAudioBusManager`, `CSTGEffectManager`,
`CSTGHDRManager` (partial — see below), `CSTGVoiceAllocator` (partial —
see below), `CSTGAudioManager` (partial — see below), `CSTGMessageProcessor`
(partial — see below)
— all fully disassembly-confirmed and KAT-verified (`verify/test_managers.cpp`,
which poisons each object's memory before construction so untouched fields
can't accidentally read as zero and pass by coincidence).

The two mid-sized ones: `CSTGAudioBusManager` (60 bytes — two float
literals, a lookup-table dword, and two confirmed side effects on
*module-global* state entirely outside the object: it resets
`gAllPlusHeadroom`/`gAllMinusHeadroom`, two shared 4-float arrays, to
`{1,1,1,1}`/`{-1,-1,-1,-1}` — very likely the per-bus clipping headroom,
reset to unity gain whenever an audio-bus manager is constructed), and
`CSTGEffectManager` (103 bytes — a lone zeroed dword, a 198-element
zeroed table, a real confirmed 72-byte gap, then two `120.0f` literals —
plausibly a default-tempo pair, flagged as speculation not fact — plus
four more zeroed dwords). Adding these surfaced a real link conflict (not
a logic bug), repeated across several classes: `engine.cpp` held
placeholder `sInstance` storage definitions for classes before they had
real constructors; each gets removed from `engine.cpp` as its real
constructor lands in `managers.cpp`, the same migration that file's own
comments track running.

**`CSTGHDRManager`** (1061 bytes, reconstructed *partially* — the first
class in this project big/complex enough that full reconstruction was
genuinely out of scope for one pass): full relocation resolution across
the entire constructor revealed a 6-sub-object-type aggregate spanning a
confirmed minimum of ~101KB, by far the largest object here. Fully
implemented: a clean `CSTGPlaybackBuffer[16]` array (88-byte stride) and
a `CSTGMonitorMixerChannel[16]` array with a genuinely interesting
confirmed quirk — each channel's real size is 172 bytes but the array
stride is 192, and channels 0-14 (not the last) get 3 extra tail dwords
zeroed by the *outer* constructor, confirmed by exactly where the *next*
sub-object begins after the last channel. Both sub-classes are declared
opaque (empty constructors, same treatment as `CEmergencyStealer`) since
each has many other real, unreconstructed methods. Confirmed but
explicitly **not implemented**: a `CSTGSampler` (real size unknown), a
17th standalone `CSTGPlaybackBuffer`, a `CSTGHDRCircularBuffer`, a
`CSTGPlaybackEvent`, and a `CSTGAudioInputMixerBase` constructed then
vtable-patched to `CSTGCDAudioPlay::CCDAudioInputMixer` — alongside a
genuinely surprising, independently confirmed finding: `CSTGCDAudioPlay::
sInstance` gets aliased to point at the *embedded* `CSTGHDRCircularBuffer`
inside this very object, not a separately allocated instance. This
reconstruction's declared `sizeof()` (`0x11a4`) is stated plainly as far
smaller than the real ~101KB object, not glossed over. **A real off-by-4
bug the KAT caught immediately**: the array was first declared starting
at offset 0, but the disassembly's own first instruction confirms it
starts at `+0x004`; the size-assertion test failed by exactly 4 bytes,
caught before it could be trusted. Fixed by adding an explicit leading
4-byte field.

**`CSTGVoiceAllocator`** (4491 bytes, reconstructed *partially* — an even
bigger case than `CSTGHDRManager`): full relocation resolution across the
whole constructor confirmed a minimum size of `0x44eac` (~281KB, vs.
`CSTGHDRManager`'s ~101KB) — a voice allocator plausibly needs full
per-voice state for every voice the engine can allocate. Fully
implemented: a 50-element self-referencing "empty list node" array, a
400-element array where each record points 4 fields back at its own
base (a genuinely different shape from the first array, confirmed via
the exact instruction operands rather than assumed from surface
similarity), and `CSTGSlotState[16]` (declared opaque, confirmed
6284-byte size via array stride). Confirmed but explicitly **not
implemented**: a second 400-element array whose per-element body is far
too detailed to fully trace (dozens of individual field writes,
including 5 copies of a UUID-prefix constant per element), and a nested
loop (an outer counter wrapping 10 `CModelVoiceRequirementsData::
Clear()` calls per iteration) — both left as explicitly-labeled
unrecovered regions. A real recursive `pthread_mutex_t` (same
allocate/init shape as `CPowerOffTimer`'s mutex, but with a proper
recursive-attribute setup this time) is the last confirmed member-touching
write, implemented faithfully. **Unlike `CSTGHDRManager`, this
reconstruction's declared `sizeof()` matches the real confirmed minimum
size exactly** — every byte through the mutex field is accounted for by
either a real member or an explicitly-labeled gap, stated plainly rather
than glossed over.

**`CSTGAudioManager`** (5785 bytes, reconstructed *partially* — but at a
much more modest confirmed size than the previous two, `0x455c`/~17.3KB):
the first of these partial-reconstruction classes that's ALSO polymorphic
(confirmed real vtable via `_ZTV16CSTGAudioManager`) — kept a real virtual
destructor and used only named C++ members (never raw offset-cast byte
arrays) so the vtable-pointer-width host/target ABI rule applies
correctly. Fully implemented: two complete mutex+condvar pairs (same
allocate/init shape as `CPowerOffTimer`'s/`CSTGVoiceAllocator`'s mutexes,
plus a matching condvar each time) and four trailing scalar constants
(`256`, `255`, `1/256`, `1.0` — plausibly a lookup-table size/mask/
reciprocal/unity-gain quadruple, flagged as speculation not fact).
Confirmed but explicitly **not implemented**: a ~15KB middle region
containing a CPU-core-count-dependent branch (via a second confirmed
cross-class singleton alias, `CSTGCPUInfo::sInstance`) and 13 profiler
"slots" (each a `CProfiler`+`CDurationStats` pair, 3 also with a
`CSTGFrontPanelStatusReporter`) — confirmed NOT a uniform array (the
slot-to-slot byte deltas measured directly from relocation offsets are
mostly `0x10f` but sometimes `0x15f`, real measured irregularity, not
assumed). KAT accesses every field via named members exclusively
(`am->mutex1Handle`, etc.) rather than raw buffer offsets, required here
specifically because of the real vtable. Caught a real test-assertion
bug (not a reconstruction bug): an early `sizeof()` check assumed the
32-bit target's 4-byte vtable pointer and failed on this 64-bit host by
exactly `sizeof(void*)`; fixed by checking `sizeof() - sizeof(void*)`
against the reconstruction's own declared member-data size instead.

**`CSTGMessageProcessor`** (5930 bytes, reconstructed *partially* — the
last sized manager class, and by far the most heterogeneous one here:
664 relocations across the whole constructor, more than any other class
combined). Confirmed: three unsolicited-message sender/message pairs
(ProgramSlot/ControllerInfo/IFX, 6 distinct vtabled types, each Sender
embedding a 32-element `CSTGDelayedMsg` queue with a confirmed
size/stride difference between the ProgramSlot sender and the other
two); `sInstance = this` set **immediately after** those pairs — a
genuine, confirmed exception to every other manager in this file, where
`sInstance` is set last; a real `CEffectorDatabase*` heap-`new`'d at
`+0x64`; ~15 `AllocAligned`-backed buffers and 14 distinct
`CSTGXxxMsgHandler` sub-objects (one per message category this processor
dispatches to); and **198** `CEffectorDatabase::Register()` + **8**
`CMOSSAlgorithmDatabase::Register()` calls — confirmed, by scanning
every object-relative access across the *entire* function
programmatically, to register individually heap-`new`'d effects into an
EXTERNAL database rather than touching this object's own layout at all
(why the constructor is 5930 bytes of code despite a comparatively
modest confirmed object size). **Scoping decision, more conservative
than the previous three partial classes**: implemented only `sInstance`
(at its confirmed exact position in the sequence) and the confirmed
minimum size — modeling the three unsol-msg pairs faithfully would need
7 new opaque vtabled sub-classes for comparatively little additional
value. Confirmed minimum size (`0x1040`) is stated as a **lower bound,
not asserted exact** — a first for this project's partial
reconstructions, unlike `CSTGVoiceAllocator`'s/`CSTGAudioManager`'s
exact `sizeof()` matches.

This was the last SIZED manager constructor among the originally
identified remaining classes.

**`CSTGMidiPortManager`** — investigated to a definitive close rather than
left as "still needs investigation". Exhaustively checked: no
`CSTGMidiPortManagerC1Ev`/`C2Ev` symbol anywhere (defined or imported);
not present in `CSTGEngine::Initialize()`'s confirmed ~44-entry
construction table; `sInstance` has zero relocations writing to it
anywhere in OA.ko; and the real destructor (264 bytes) operates entirely
on the class's two *static* array members (`sMidiInPorts`/
`sMidiOutPorts`), never touching `this`-relative memory at all — this
class has no per-instance state. **Conclusion**: its implicit default
constructor is genuinely empty (nothing per-instance to initialize),
which is exactly why no constructor symbol was ever emitted — not a gap
in this project's search, a confirmed structural fact. All real
"bring-up" work happens in the separate, already-named
`CSTGMidiPortManager::Initialize()` (790 bytes) instead — a genuinely
different pattern from every other manager here. Documented directly in
`oa_engine.h`'s class comment; no constructor code to write, since there
isn't one.

With this closed, every class from the original ~40-manager construction
table has been either reconstructed (21 constructors, 4 partially) or
definitively confirmed to have no constructor to reconstruct.

**`CSTGGlobal`** (`src/engine/global.cpp`, extended 2026-07-01) — its
~195 methods turn out to be dominated by one well-defined family: roughly
150 `UpdateXXX(CSTGMessageContext&, STGConvertedParam&)` message
handlers, one per settable global parameter. Picked the four smallest,
cleanly-confirmed (no-branch) ones, same smallest-first methodology as
the manager constructors: `UpdateMuteMode` (9 bytes, raw int store),
`UpdateRearPanelControllerReset`/`UpdateTmbrTrkOscTransposeType` (12
bytes each, real bool conversions at adjacent flag fields), and
`UpdateUserAllNoteScale` (13 bytes, the first *indexed* one — writes into
an array slot selected by the message context's own index field, not a
fixed offset). All four write into `CSTGGlobal`'s confirmed ~43.6MB-in
field range. New shared types `STGConvertedParam`/`CSTGMessageContext`
declared minimally (only the fields these four handlers actually read).
New dedicated test file `verify/test_global.cpp` (separate from
`test_engine.cpp`, since this family has ~150 members and will keep
growing) — its KAT specifically confirms the bool-conversion handlers
produce `1` for a value of `7`, not `7` itself, catching the "raw store"
mistake a shorter/wrong implementation wouldn't be caught by. 9 KAT
suites now (up from 8), all passing.

**Three more, each a different shape** (2026-07-01, same day): confirmed
that not every `CSTGGlobal` handler simply writes its own field.
`UpdateLRBusIndivAssign` (20 bytes) doesn't touch `CSTGGlobal` at all —
it computes `this+4` and delegates to a newly-added
`CSTGAudioBusManager::SetLRBusIndivAssign(int)` (11 bytes, added to that
already-existing class), which looks up the same
`STGAPILR2IndivToPhysBusId` table its constructor reads its default from.
**Honestly flagged, not glossed over**: why `CSTGGlobal+4` aliases as a
`CSTGAudioBusManager*` isn't determined — the real singleton is
confirmed separately allocated elsewhere, so this is a genuinely
different pointer. `UpdateSPDIFSampleRate` (23 bytes) writes into the
*message context*, not `CSTGGlobal`, and writes a literal constant (`6`)
rather than the incoming parameter value — gated on both a `CSTGGlobal`
flag and the parameter being nonzero. `TranslateAudioInputParamId`
(21 bytes) is a pure, `this`-independent lookup into a confirmed 8-entry
`.rodata` table, declared `static` despite being a real class member.
KAT specifically checks that `UpdateSPDIFSampleRate` writes the literal
`6`, not the parameter value that was fed in — a real "wrote the wrong
source value" mistake that assertion would catch.

**Batch 3, same day** — `SetSplitLayerWorkState(bool)` (7 bytes, the
smallest `CSTGGlobal` method found so far — direct byte store);
`UpdateFootswitchPolarity` (29 bytes, same conditional-delegation shape
as `UpdateLRBusIndivAssign`, this time to a newly-declared
`CSTGControllerRTData::SetFootSwitchPolarity`); and
`UpdateSongPunchMIDIChannel` (30 bytes). **Worth recording plainly**:
this last one caught a real mistake mid-session — a first reading
assumed the function re-reads its own field's old value before
overwriting it, but the disassembly actually shows two SEPARATE,
independent byte fields (one read-only, one write-only, one byte apart).
The C++ reconstruction itself was already correct (it faithfully
followed the two distinct offsets); the mistake was in the English
description and in the KAT's test setup, which assumed the fields were
the same one. Both fixed once the KAT's failure prompted a closer
re-read — a good example of why the KAT step matters even for
"obviously right" small functions.

**Batch 4, same day** — a re-scan of the FULL method list (not just the
first pass's short excerpt) turned up a dozen more simple, no-branch
handlers in one go: 8 raw stores (`UpdateSeqParamMidiOutMode`/
`UpdateAfterTouchCurve`/`UpdateBankMap`/`UpdateVelocityCurve`/
`UpdateSeqTrackMidiOutMode`/`UpdateVectorMIDIOut`/`UpdateNoteReceive`/
`UpdateDamperPolarity`) and 4 bool-converted stores confirmed to write
four CONSECUTIVE flag bytes (`UpdateCombiChangeEnable`/
`UpdateAftertouchChangeEnable`/`UpdateControlChangeEnable`/
`UpdateSysExEnable`, `+0x6d7`..`+0x6da`). KAT is table-driven via member
function pointers to avoid a dozen near-identical hand-written blocks.
**Worth remembering for future batches**: re-scan the full method list
each time rather than trusting an earlier partial view — more than
doubled this session's `CSTGGlobal` count in one pass just by doing
that.

**Full rescan, same day** — a user request to look for easy wins across
the ENTIRE method list (not partial excerpts) paid off big: dumped and
sorted all 173 non-weak `CSTGGlobal` symbols by size. `UpdateSongPunchMIDIChannel`
turned out to be the first of a 23-method family, ALL confirmed
(programmatically, not spot-checked) to share the exact same shape and
the exact same shared array base — refactored into one private helper
with 23 thin public wrappers, replacing the earlier one-off
implementation for consistency. `UpdateHeadroom` (53 bytes) turned out
clean and self-contained despite its size, and is the first confirmed
case of `STGConvertedParam.value` being read as a **float** rather than
`int` — broadcasting it into the same `gAllPlusHeadroom`/
`gAllMinusHeadroom` globals `CSTGAudioBusManager`'s constructor sets to
unity gain.

Two clusters were found but deliberately NOT rushed: the 9
`UpdateAudioInputXXX` handlers all delegate into a whole new,
~30-method `CSTGAudioInput` class (some methods up to 962 bytes) — a
real future project, not an easy win. The 22-method `UpdateXXXCCAssign`
family (141 bytes each, names mirroring many MIDIChannel controls) is a
genuinely complex table-scan-and-reassign mechanism (clears old CC
bindings, updates what looks like a reverse-lookup table) — sampled and
characterized but explicitly deferred rather than guessed at. Both are
documented in MASTER_REFERENCE.md sec 10.35 so a future pass doesn't
need to re-derive the same groundwork.

46 of ~195 `CSTGGlobal` methods done (up from 23 in one session — the
biggest jump yet); the two deferred clusters above plus the voice-model
classes and the sub-handlers left unimplemented inside the partial
manager classes are the natural next targets, if deeper completeness
is wanted there.

A few real, worth-recording finds from this batch:
- Every "file daemon" class (one with a `ProcessCommands()`) zeroes the
  *same* first three fields (`+0x00`/`+0x04`/`+0x08`) before anything
  class-specific — a real, confirmed shared header. What that header
  actually *is* got settled once `CSTGFileOpener` and `CSTGCDWorker` were
  reconstructed and their `ProcessCommands()` bodies read: each daemon has
  its own fixed-capacity ring buffer of small status-tagged command
  records (base pointer / write index / read index / capacity-as-modulus),
  but that ring buffer lives at a **different offset per class**
  (`CSTGCDWorker`: `+0x224..+0x234`; `CSTGFileOpener`: `+0x210..+0x21c`,
  right after 32 identical 16-byte "slots" whose purpose isn't determined
  yet) — the earlier "maybe it's a 12-byte head/tail/count at the very
  front, `CSTGQuadList`-shaped" guess (written after only the smaller
  daemons had been looked at) turned out to be wrong in its specific
  offset claim, confirmed by actually reading `ProcessCommands()` rather
  than inferring from the constructor alone. `ProcessCommands()` itself is
  declared but not implemented for either class — both dispatch through
  vtables on objects of unrecovered types.
- **Caught and fixed two of my own doc mistakes via the KAT, not the
  disassembly**: I initially wrote (and the header briefly claimed)
  `CSTGHDRFileWriter` and `CSTGTempoUtils` zero their *entire* confirmed
  object. The poison-then-construct test caught both immediately — the real
  constructors leave real, confirmed gaps (`CSTGHDRFileWriter`: 4 untouched
  bytes at `+0x0c`; `CSTGTempoUtils`: five small gaps of 2-4 bytes each)
  that a "does it look fully zeroed" read-through missed. Fixed to state
  only what's actually confirmed touched.
- `CSTGMetronome`'s constructor opens with `AND [this],0xfa` (clearing 2
  specific bits of a flags byte, not overwriting the whole byte) —
  confirmed evidence of a base class or preceding sub-object already
  initialized before this constructor runs; not resolved in this pass.
- `CSTGDiskCostManager`'s constructor is a single `sInstance = this` and
  nothing else, despite a confirmed 72-byte real object — every other field
  is either zero-initialized by its `CSTGBankMemory` allocation already, or
  set later by its own (not yet reconstructed) `Initialize()`.
- `CSTGFileOpener`'s 953-byte constructor is almost entirely one repeated
  16-byte zeroing pattern (32 times) — reconstructed as a C++ `for` loop
  rather than 32 manually-unrolled statements, both simpler and arguably
  more faithful to whatever small loop the original source had before the
  compiler unrolled it. Its confirmed 544-byte total size checks out
  exactly against the layout: `12 (header) + 4 (gap) + 32×16 (slots) + 16
  (ring control) = 544`.
- `CPowerOffTimer`'s constructor is a clean cross-check with its own
  already-reconstructed destructor: it allocates a real mutex
  (`rtwrap_malloc(get_sizeof_rtwrap_pthread_mutex())`) and initializes it
  (`rtwrap_pthread_mutex_init`), storing the handle at the exact `+0x18`
  field the destructor was already confirmed to tear down via
  `rtwrap_pthread_mutex_destroy`+`rtwrap_free`. **Found and fixed a real
  host-vs-target ABI bug in this field before it shipped**: the confirmed
  28-byte object has room for exactly a 4-byte pointer at `+0x18` (the
  object's very last field) — a native `void*` write there is 8 bytes on
  this 64-bit host, which `-Warray-bounds` caught as an out-of-bounds
  write. Fixed by storing the handle as an explicit 32-bit value (matching
  the real 32-bit target exactly) instead of a native pointer.
- `CSTGAudioDriverInterfaceKorgUsb` surfaced a **different**, more
  fundamental host/target mismatch: it's the first reconstructed class in
  this batch with a real inherited vtable. A vtable pointer is 4 bytes on
  the confirmed 32-bit target but 8 bytes on this 64-bit host, so *every*
  confirmed offset after it (`+0x38`/`+0x3c`/`+0x40`/`+0x44`) shifts by 4
  bytes on host — a hardcoded `(this)+0x38`-style write, the pattern used
  everywhere else in this file, would silently corrupt the following field
  on host. Fixed by declaring this one class's fields as ordinary named
  C++ members instead of a raw offset-accessed byte blob, letting each
  build's own compiler place them correctly after whatever width vtable
  pointer that build actually has — correct target-ABI layout is what
  matters for the real reconstruction; the host build only needs
  internally consistent placement to KAT-test the confirmed field
  *values*. Also caught, while transcribing this class: two of its writes
  (`[this]=8` and `[this+0x3c]=0`) looked like plain literals in the raw
  disassembly bytes but both carry `.rel.text` relocations (a vtable
  pointer and a member-function pointer respectively) — checked before
  committing this constructor, not caught afterward the way sec 10.14's
  "fully zeroed" mistakes were.
- `CLoadBalancer` embeds a real, separately-named class,
  `CEmergencyStealer` (confirmed via its own constructor's relocation and
  independent `sInstance` singleton), as its first 36 bytes — declared
  here as an opaque member with an intentionally empty constructor body,
  since only its existence and size as `CLoadBalancer`'s leading
  sub-object are confirmed, not its own real behavior.

### `init_module`: the real boot/load sequence — now the priority (2026-07-01)

**Reconstructed and KAT-verified**: `include/oa_init.h` + `src/init/init_module.cpp`,
a new 10th KAT suite (`verify/test_init_module.cpp`). This is the concrete
deliverable of the priority pivot described in the Progress table above — the
user redirected effort here because Eva (the UI) launches if and only if this
function returns 0, regardless of whether the audio engine works.

New ground, not previously examined by this project or by
`kronosology/docs/modules/` (which documents `CSTGEngine::Initialize()` as
if it were the top of the init chain — it isn't; `init_module`,
`.init.text+0x0`, 847 bytes exactly, is the actual `insmod` entry point and
runs first). Reconstructed via a complete `.rel.init.text` relocation
resolution plus a full line-by-line disassembly trace — not just the
`MASTER_REFERENCE.md` sec 10.17 summary table, which turned out to gloss
over one real detail (see below).

It's a linear "call subsystem init → check result → hard-fail via a
partial-unwind cascade, or continue" chain: C++ static-constructor bring-up,
a CPUID feature probe, CPU-affinity pinning, an optional PID-signal step
that degrades gracefully if its target file is missing, then eleven
subsystem `Initialize`-style calls in sequence — `InitializeSTGHeap`,
`InitSharedMemProcInterface`, `InitPcmModProcInterface`,
`setup_global_resources`, **`SetupAtmelForAuthorizations`** (the AT88 chip
GPA handshake — already fully reconstructed, `src/auth/atmel_setup.cpp`),
`setup_stg_decrypt_daemons`, `load_global_resources`, `setup_stg_daemons`,
**`CSTGAudioManager_StartAudioEngine`**, **`CSTGKeybedInterface_Startup`**,
and `stg_rtfifo_init` — every one of which is a **hard `insmod`-time fail**
if it doesn't return success, unwinding cleanly through however much of
init actually completed.

**The single most load-bearing finding here**: `SetupAtmelForAuthorizations`
being called from `init_module` means the AT88 chip handshake is a hard
gate on the module loading *at all* — not merely a DRM/audio-quality
degradation path the way the *other* AT88-adjacent check,
`InitCdromSupport` (called later, from inside `CSTGEngine::Initialize()`,
and degrades gracefully — sec 8), is. These are two separate checks with
two different failure consequences; conflating them would have been a
real mistake. Likewise `CSTGAudioManager_StartAudioEngine` and
`CSTGKeybedInterface_Startup` are hard load gates needing real or emulated
audio/keybed hardware just to get past `insmod`, before any question of
DSP correctness arises.

Also confirms module load order: the `stg_*` shims (from `STGEnabler.ko`,
already reconstructed at `kronosology/reconstructed/STGEnabler/`) must
load before `OA.ko` — Linux resolves every undefined symbol at `insmod`
time, so a missing companion module fails immediately with "unknown
symbol." `STGGmp.ko` (also already reconstructed) covers the `__gmpz_*`
auth-math dependency the same way.

### VM Boot Test — Module Load Order (2026-07-11, `MASTER_REFERENCE.md` sec 10.208 has the full derivation)

A real `kronosvm` boot test found `insmod OA.ko` failing on 12 unresolved
symbols even after `AT88VirtualChip.ko`/`OmapNKS4VirtualDriver.ko`/
`KorgUsbAudioVirtualDriver.ko` all loaded successfully — the load
sequence used simply never loaded `STGEnabler.ko`/`STGGmp.ko` at all, and
two of the three virtual-driver modules were each missing one export.
Both gaps are now closed (`STGGmp.ko` built for real for the first time,
`COmapNKS4_GetProgressBarPercent` added to `OmapNKS4VirtualDriver.ko`,
`USBMidiAccessory_SetDrumPadClient` added to `KorgUsbAudioVirtualDriver.ko`
— see sec 10.208 for the full derivation of each). The correct load order
for any future VM boot test:

1. RTAI core: `rtai_hal.ko` → `rtai_smp.ko` (or `rtai_sched.ko`, sec
   10.39's noted substitute) → `rtai_sem.ko` → `rtai_ndbg.ko` →
   `rtai_fifos.ko` — `STGEnabler.ko` itself needs real RTAI symbols
   (`rt_linux_use_fpu`/`rt_set_oneshot_mode`/`start_rt_timer`).
2. `STGEnabler.ko` — the `stg_*` shim layer (`stg_set_cpus_allowed`,
   `stg_cpumask_of_cpu`, etc.).
3. `STGGmp.ko` — the `__gmpz_*`/`mpz_*`/`mpn_*` bignum symbols
   `cm_ComputeChallenge` needs.
4. `AT88VirtualChip.ko` — `stgNV2AC_sync_cmd`/`stgNV2AC_sync_read_cmd`.
5. `OmapNKS4VirtualDriver.ko` — the `COmapNKS4*`/`OmapNKS4OutputFifo_*`/
   `SetupNKS4Calibration` family.
6. `KorgUsbAudioVirtualDriver.ko` — `KorgUsbAudio*`/`KorgUsbMidi*`/
   `USBMidiAccessory_SetDrumPadClient`.
7. `OA.ko` — last. Modules 2–6 have no dependencies on each other or on
   OA.ko (confirmed via `nm -u` on each), so their relative order among
   themselves doesn't matter — only "after 1, before 7" is load-bearing.

**Update (2026-07-12): step 1 (real RTAI core) can now be replaced
entirely by `RTAIVirtualDriver.ko`** for VM/QEMU-TCG boot testing, where
genuine RTAI cannot be reliably brought up at all (`MASTER_REFERENCE.md`
sec 10.211–10.214). `RTAIVirtualDriver.ko` (`reconstructed/RTAIVirtualDriver/`)
provides the same 26-symbol RTAI-family surface `OA.ko` needs, plus the
3 extra RTAI-timer symbols `STGEnabler.ko` needs directly, using ordinary
Linux kthreads/semaphores/workqueues instead of genuine hard-RT
scheduling — it must load **first**, before `STGEnabler.ko` (see
`reconstructed/RTAIVirtualDriver/README.md` for why). Live-tested: with
this substitute, `OA.ko`'s `insmod` resolves all 84 previously-unresolved
symbols for the first time in this project's history, and `init_module()`
genuinely runs (`OA_DEBUG_MARKER 1/2/3`). Real hardware must still use the
genuine RTAI stack (step 1 above) — `RTAIVirtualDriver.ko` is a VM-only
substitute, never a replacement on real hardware.

**Update (2026-07-12): the sec 10.184/10.215 `fs_base` Oops was NEVER a
bzImage/kernel bug — it was a bug in `stg_get_current_task()`'s own
reconstruction, now fixed.** Full root-cause trace: `MASTER_REFERENCE.md`
sec 10.216. Short version: `stg_get_current_task()`
(`src/stub/bar2_stubs_c.cpp`) used a literal, hardcoded `mov %fs:0x0`
displacement, based on a misreading of `OA_real.ko`'s raw disassembly —
`objdump -d` without `-r` prints an unresolved ELF relocation's
placeholder bytes as `00 00 00 00`, identical-looking to a real immediate
zero. `readelf -r`/`objdump -dr` against the real binary shows every one
of its 8 real `mov %fs:0x0` call sites carries an `R_386_32` relocation
against `per_cpu__current_task`, resolved by the kernel's own module
loader at insmod time to `current_task`'s real (non-zero) per-cpu offset
for that exact kernel build. The real, factory-shipped kernel's own
`setup_per_cpu_areas()`/GDT setup (confirmed by reading
`/home/share/linux-kronos`, the first time this project had real kernel
source for this) is stock, correct, unmodified upstream Linux 2.6.32
x86_32 SMP code — it was never the bug. Fixed by referencing
`per_cpu__current_task` by name in the inline asm operand instead of a
literal 0, which makes GAS emit the same kind of relocation the real
binary has (confirmed byte-for-byte via `objdump -dr` on the rebuilt
`.ko`). Live-tested: `OA.ko`'s `init_module()` now reaches
`OA_DEBUG_MARKER 4` (never reached before) and gets all the way past
`stg_get_current_task()` before hitting a NEW, unrelated, later crash —
a NULL-filename `path_init()` Oops from `CSTGFile_Open(0, 0)`'s literal
`0` filename argument (init_module.cpp step 4) — a separate, pre-existing
reconstruction gap, not investigated further by this task.

**Update (2026-07-12): the long-standing `CSTGEngine` ctor crash (CR2
`0x029cc4f4`) was a VM memory-map artifact, not a source bug — fixed by
a QEMU config change. New standard boot config below.** Full derivation:
`MASTER_REFERENCE.md` sec 10.224,
`.claude/agent-memory/re-decompiler/cstgengine_crash_is_not_qemu_memory_map.md`.
Short version: `InitializeSTGHeap()` (`stgheap_init.cpp`) scans
`iomem_resource` for the single largest unclaimed top-level gap and
`ioremap_cache`s it as OA's private ~100-200MB+ bank-memory pool — a
genuine-hardware strategy (real Kronos has physically-present reserved
DRAM there). Under a stock QEMU `pc`/i440fx machine with a plain `-m`
value and no `mem=` cap, the largest such gap is always the *unbacked*
address space between top-of-installed-RAM and the ~4GB BIOS-reserved
region — `ioremap_cache()` succeeds (returns a non-NULL, plausible-
looking pointer) but the region is phantom: writes are silently dropped,
reads return `0`. This produces a `bankBase==0` heap that looks
initialized but isn't, which downstream sails past every null-guard
(`CSTGBankMemory::Initialize(0, size)` is itself harmless) until
`CSTGEngine`'s storage `AllocAligned` computes a bogus non-null offset
into memory address `0`, and the ctor Oopses at
`CR2 0x029cc4f4`. Confirmed decisively with a temporary
write-then-read-back `printk` probe immediately after
`sIORemapBase = (unsigned long)mapped` in `stgheap_init.cpp`: stock
config read back `0x00000000` at both ends of the mapped region despite
writing `0xA5A5A5A5`/`0x5A5A5A5A`.

**The fix is a QEMU/boot-config change, not a C-source edit** — every
function in the failing chain (`Alloc`, `local_heap_base`,
`local_heap_region`, `setup_global_resources`, the `0xaaf1140` size
constant) was independently disassembly-verified faithful to the real
`OA_real.ko` first (sec 10.224's own opus second-opinion pass), and the
prior three genuine `CSTGHeapManager` bugs (cursor offset, struct
padding gap, `Initialize()` register reuse — commit `44b4b07`) remain
correctly fixed; none of that was reverted or touched here. The scan
always keeps the *largest* top-level gap it finds — not simply "the
first free gap above `high_memory`" — so a plain `mem=` kernel-cmdline
cap alone does NOT work if it leaves the unbacked
"above-installed-`-m`-value" hole bigger than the newly-freed backed
gap (confirmed empirically: `-m 2048M` + `mem=1024M` still handed the
scanner the huge *unbacked* space above the 2048M `-m` ceiling, because
that hole is bigger than the ~1023MB the `mem=` cap freed up — same
`BANKPROBE` all-zero result). **The fix that works**: push `-m` close to
(but safely under) the i440fx PCI-hole boundary (`0xc0000000`, ~3072MB)
so the "leftover" unbacked space above `-m` shrinks to a few hundred MB,
and use a *small* `mem=` cap so the freed backed gap (`-m` minus `mem=`)
is comfortably bigger than that leftover — guaranteeing the scanner's
largest-gap search lands on genuine, QEMU-backed DRAM instead.

**New standard VM boot config for this project (supersedes any
`run_dbg.sh` using a plain `-m` value with no `mem=` cap):**
```
qemu-system-i386 -M pc -cpu n270 -m 3000M -smp 1 -accel tcg \
    -drive file=kronos.img,format=raw,if=ide,index=0,media=disk \
    ... (display/serial/monitor/net/rtc/-no-reboot as usual) ...
```
with the guest's `/boot/grub/grub.conf` kernel line carrying `mem=384M`
(alongside the existing `root=`/`init=`/etc args, e.g.
`... console=tty0 mem=384M init=/korg/kronos_init`). This reserves
physical `[384M, ~3000M)` as genuine QEMU-backed DRAM the kernel itself
never claims as System RAM — confirmed via the same probe re-added
temporarily: `BANKPROBE n=a5a5a5a5 f=5a5a5a5a` (correct read-back at
both ends) under this config, versus `n=00000000 f=00000000` (phantom)
under the old plain `-m 2048M`/no-`mem=` config. `-smp 4` (the older
convention) should still work here too — only `-m`/`mem=` are the
load-bearing change; `-smp 1` was carried over unchanged from the most
recent working baseline, not re-tested at `-smp 4` under this new memory
config.

Live-tested end to end with this config and the temporary probe removed
again: boot now gets all the way past `OA_DEBUG_MARKER 8` and past
`CSTGEngine`'s constructor for the first time ever, reaching a NEW,
later, and completely different crash inside
`CSTGEngine::Initialize()` (`setup_global_resources+0x2a4` →
`CSTGEngine::Initialize()+0x2f7`): a call through a NULL function
pointer (`EIP: [<00000000>] 0x0`, `CR2: 00000000`, `Bad EIP value` — most
likely an uninitialized vtable slot or null function-pointer member
called mid-`Initialize()`), not investigated further this session.

**Two corrections to the picture above**, found by actually reading code
rather than assuming from names (full detail: `MASTER_REFERENCE.md` sec
10.36):

- **`CSTGKeybedInterface_Startup` (step 14) does NOT call into
  `OmapNKS4Module.ko` at all**, contrary to the original assumption.
  Disassembly shows it's OA.ko's OWN serial-port keybed handshake —
  scans up to 7 COM ports via a real 2561-byte UART driver class
  (`CSTGComPort`, entirely inside OA.ko), sending a probe byte and
  waiting for a real keybed board to ACK. This is a deeper hardware
  dependency than a clean external-module stub could satisfy; deferred
  for dedicated investigation.
- **`OmapNKS4Module.ko`'s own real `init_module` hard-requires a real USB
  front-panel board to `probe()`** — it will NOT succeed in a VM with no
  panel attached, even though the module is 100% reconstructed. Not
  usable as-is for VM boot testing.

**`KorgUsbAudioDriver.ko` (step 13) is smaller than feared, AND now built**:
new sibling project `kronosology/reconstructed/KorgUsbAudioVirtualDriver/`
(same host/kernel-split pattern as `AT88VirtualChip`), providing all ~20
real exported symbols with plausible always-ready behavior. The real
binary's exports were fully characterized via direct inspection
(`ARCHIVE/Ignored/DecryptedImages/MOD_Extracted/KorgUsbAudioDriver.ko`,
never decompiled before) — it's ONE combined audio+MIDI driver, and every
audio-family function OA.ko calls takes no arguments at all (base+stride
ring-buffer accessors and small status/flag reads).

**A materially important correction found while tracing the real call
chain**: `init_module` step 13's actual success/failure does NOT depend on
any `KorgUsbAudio*` symbol at all. Full disassembly of `CSTGAudioManager::
StartAudioEngine()` (through a virtual dispatch chain into the
already-reconstructed `CSTGAudioDriverInterfaceKorgUsb::Start()`) shows the
audio driver's own return value is discarded unconditionally — the REAL
gate is N (list-length-dependent) + 2 internal `CSTGThread::
CreateRealTimeWithCPUAffinity` calls (zero KorgUsbAudio dependency). This
stub satisfies OA.ko's *link-time* symbol requirement (an unresolved
`GLOBAL` undefined symbol blocks `insmod` outright) but isn't actually
what gates step 13's pass/fail.

**`CSTGThread::CreateRealTimeWithCPUAffinity` fully investigated and
resolved, with a real correction along the way** (`MASTER_REFERENCE.md`
sec 10.38-10.39): first thought it wraps `rtwrap_*` externs exported by
`loadmod.ko` (per an existing project doc) — checking OA.ko's own symbol
table directly showed this was WRONG: every `rtwrap_*` function
(`rtwrap_pthread_create`, `rtwrap_set_debug_traps_in_rt_task`, etc.) is
DEFINED INSIDE OA.ko itself, wrapping REAL RTAI kernel primitives
directly (`rt_task_init`, `rtheap_alloc`, `rt_set_runnable_on_cpuid`,
etc.) — the SAME kind of real-RTAI dependency already identified for step
16. No stub is appropriate here; the real RTAI `.ko` modules need to
actually load (confirmed order via `loadoa.c`'s own step-3 insmod
sequence: `rtai_hal.ko` → `rtai_smp.ko` → `rtai_sem.ko` → `rtai_ndbg.ko`
→ `rtai_fifos.ko`). Found and flagged a genuine local artifact along the
way: the captured Kronos image's own `rtai_smp.ko` is a 0-byte file with
a mismatched timestamp (a project-local extraction issue, not real
firmware behavior) — a substantial sibling module, `rtai_sched.ko`,
exports the same needed symbols and can substitute for testing, though
that substitution isn't independently confirmed as hardware-equivalent.
`loadmod.ko`'s role reverts to only its original one (orchestration + the
still-open `/korg/Eva` decryption bypass question).

**A genuine, unresolved discrepancy, re-examined and still open**: real
extracted `loadoa.c` source (not just the step-numbered comments —
the actual insmod calls) confirms `insmod OA.ko` (fatal on failure)
happens BEFORE `insmod KorgUsbAudioDriver.ko` (explicitly non-fatal in
the source's own comment). OA.ko's `KorgUsbAudio*` symbol references are
confirmed `GLOBAL` (not `WEAK`) direct-call relocations (ruled out a
`symbol_get()`-style soft runtime lookup) — standard kernel module
loading shouldn't allow this order to work. Not resolved by further
static analysis; flagged as needing actual boot/emulation testing to
settle, rather than guessed at.

Two real bugs caught by `verify/test_init_module.cpp`'s KAT, both in the
test's own assumptions, not the reconstruction — the step-2 (CPU feature
probe) failure path is a distinct, SHALLOWER exit that skips the
CPU-affinity restore call entirely (nothing to restore yet at that
point), and step 5's failure correctly skips `CleanupSharedHeap` (the
heap itself never got created, nothing to clean up) — both confirmed via
disassembly once the KAT's first-draft expectations didn't match. All 10
KAT suites pass after a full clean rebuild.

**The `CSTGComPort` keybed hardware question, advanced but not closed**:
`CSTGComPort::Initialize()` (2561 bytes) references a class `CW83627` —
the Winbond W83627 **Super I/O chip**, real standard PC hardware (legacy
UART via ISA/LPC), confirmed via literal raw `in al,dx`/`out dx,al`
port-I/O instructions in the disassembly, not inferred from the name
alone. This is meaningfully more tractable than a custom NKS4-specific
bus — very likely one of the classic legacy COM1-4 ports
(0x3F8/0x2F8/0x3E8/0x2E8), which QEMU's standard machine types already
emulate. The exact port address wasn't resolved by hand-tracing in this
pass (a real Ghidra decompile of the whole function would likely resolve
it faster than more manual disassembly reading — the full-binary load
remains a confirmed 600s-timeout case for this Ghidra MCP server, not
re-attempted). Full detail: `MASTER_REFERENCE.md` sec 10.40.

**Bar 1 (host/mock KAT harness) is done. RTAI loading is now LIVE-TESTED
in `kronos_vm`, not just planned**: updated `kronos_vm/overlay/sbin/
loadoa` to insmod the real RTAI modules and patched it directly into the
existing `kronos.img` via `guestfish` (non-destructive). A real QEMU boot
test confirmed all 5 modules (`rtai_hal`/`rtai_sched`/`rtai_sem`/
`rtai_ndbg`/`rtai_fifos`) load with the correct dependency chain and
`/dev/rtf0`/`rtf5`/`rtf6` are present — the first genuinely CONFIRMED
(not theorized) real-hardware-dependency success in this whole effort.
Full detail: `MASTER_REFERENCE.md` sec 10.41.

**The real cross-compile is DONE too** (2026-07-02): added a Kbuild
section to this Makefile (`make ko KDIR=...`, same dual-mode convention
as `AT88VirtualChip`/`KorgUsbAudioVirtualDriver`) and built a genuine
41316-byte ELF32 Intel-80386 `OA.ko` from all 19 currently-reconstructed
files — zero real compile errors. Found and fixed a real bug shared by
all three reconstructed C++ kernel modules (none had been Kbuild-tested
before): `$(CXX)` silently expands to empty inside Kbuild (only `$(CC)`
is defined there), corrupting the compile command — fixed with
`CXX ?= g++`. The built `.ko`'s 140 unresolved symbols are exactly the
documented not-yet-reconstructed pieces (real kernel/RTAI primitives +
not-yet-implemented class methods) — expected, resolved at `insmod` time
against the running kernel, not a defect. Also found (while proactively
checking the other two modules too) a separate, unresolved toolchain
issue: their `module_main.cpp` files directly include real kernel
headers, which this modern g++ can't parse as C++ — `OA.ko` avoided this
by hand-declaring its own externs instead, matching its established
convention throughout. Full detail: `MASTER_REFERENCE.md` sec 10.42.

**The first real insmod boot test is DONE too** (2026-07-02): fixed
`AT88VirtualChip`/`KorgUsbAudioVirtualDriver`'s Kbuild issue by renaming
their `module_main.cpp` to `module_main.c` (neither used any genuine C++
feature — compiling as plain C sidesteps g++'s ancient-kernel-header
parsing problem entirely; both now build real `.ko`s too). Uploaded all
three `.ko` files into `kronos_vm`'s image and extended `loadoa` to
insmod them after RTAI. Confirmed via two real, reproducible QEMU boot
tests: **`AT88VirtualChip.ko` and `KorgUsbAudioVirtualDriver.ko` both
insmod successfully** and show `Live` in `/proc/modules` — a genuine
working boot-time result for both stubs, not just a compile-time one.
`OA.ko`'s own insmod fails, exactly as expected given its 140 confirmed
unresolved symbols (real kernel modules are fully/eagerly linked at load
time) — captured the exact, reproducible error: `insmod: error
inserting '/korg/rw/oa_recon/OA.ko': -1 Unknown symbol in module`.
Full detail: `MASTER_REFERENCE.md` sec 10.43, 10.44.

**`InitializeSTGHeap` (init_module step 5) is reconstructed too**
(2026-07-02): finds a free MMIO region, `ioremap_cache`s it, zeroes it,
hands it to `CSTGHeapManager_Initialize`. Confirmed via a real Kbuild
rebuild: it and `CleanupSharedHeap` are now DEFINED, out of the
unresolved-symbol list — only 7 new, fully expected symbols appear (5
real kernel globals, 2 new `CSTGHeapManager_*` targets). Also caught a
real toolchain trap worth remembering for every future function like
this one: literal floating-point math compiles fine here but pulls in
libgcc soft-float helpers under the kernel's own `-msoft-float` Kbuild
flags (unlike genuine kernel/RTAI symbols, these won't resolve on a real
kernel) — fixed with a hand-written `divl`-based division mirroring the
kernel's own `do_div()`. New host KAT: `verify/test_stgheap_init`. Full
detail: `MASTER_REFERENCE.md` sec 10.45.

**`InitSharedMemProcInterface` (init_module step 6) is reconstructed
too** (2026-07-02): `create_proc_entry(".shm", 0600, NULL)` — CORRECTED
a prior guess that this created `/proc/.oacmd_shmem`; the real name,
extracted directly from `.rodata`, is `.shm`. Sets `uid=500`/`gid=500`/
`proc_fops` on success, with `struct proc_dir_entry`'s exact field
offsets confirmed via a compile-time `offsetof` probe against this
project's own local kernel source tree (not hand-derived — a more
reliable method worth reusing for future real-kernel-struct field
access). Confirmed via a real Kbuild rebuild: both this and
`CleanupSharedMemProcInterface` are now DEFINED, removed from the
unresolved list, with **zero new symbol additions** — the cleanest
reconstruction step yet. New host KAT: `verify/test_shmemproc_init`.
Full detail: `MASTER_REFERENCE.md` sec 10.46.

**`InitPcmModProcInterface` (init_module step 7) was ALREADY
reconstructed** (2026-07-02): confirmed to be exactly Stage 1's
`/proc/.oacmd` registration (`src/auth/oa_cmd_proc.cpp`) — the real
proc entry name, extracted from `.rodata`, is `.oacmd`, settling this
project's own long-standing "not yet confirmed whether this is a thin
wrapper" question. **Found a real, pre-existing bug in the process**:
`oa_cmd_proc.h` (Stage 1) never wrapped its declarations in
`extern "C"`, so these two functions compiled under mangled C++ names
that never matched `oa_init.h`'s own correct declarations — silently
broken since Stage 1, only surfacing now because no host KAT had ever
specifically exercised them. Fixed by wrapping the header in
`extern "C"` and consolidating the duplicate declaration. A related,
currently-harmless sibling issue in `process_oacmd.h` was found and
deliberately left alone (documented, not silently patched — nothing
calls `ProcessOACmd` inconsistently yet). Confirmed via a real Kbuild
rebuild: both functions now defined under their real names, zero new
symbol additions. New host KAT: `verify/test_pcmmodproc_init`. Full
detail: `MASTER_REFERENCE.md` sec 10.47.

**`setup_global_resources` (init_module step 8, the "crux") is
reconstructed too** (2026-07-02) — the largest single function this
project has reconstructed to date (7267 bytes, ~11x the next-largest).
Confirms this project's own advance flag: it genuinely constructs
nearly the whole engine (`CSTGGlobal`, `CSTGEngine`, `CSTGFrontPanel`,
`CCostProfile`, `CSTGCPUInfo`, `CMeteredDebugOutput`,
`CSTGSampleRateMonitor`, `CSTGASK`, plus ~20MB of heap allocations and a
~168KB hardware-status struct). Faithfully reconstructed: the exact
order of all ~42 calls and all THREE real hard-fail checks (confirmed
to fire in an unusual order — `CSTGCPUInfo`'s check is LAST despite
being allocated FIRST). Discovered `CSTGHeapManager::Alloc()` actually
returns a slot NUMBER, not a raw pointer, resolved via the same formula
`oa_heap_region()` already established. Found and documented (not
fixed) a real pre-existing structural conflict between two incompatible
`CSTGGlobal` declarations across headers. Found and fixed two real bugs
while building the host KAT (a vtable-pointer/struct-field offset
collision causing a segfault, and an infinite-recursion allocator
mock). Confirmed via a real Kbuild rebuild: both functions now defined,
removed from the unresolved list; 24 new symbols appear, all exactly
the expected, disassembly-confirmed dependencies. New host KAT:
`verify/test_setup_global_resources`. Full detail:
`MASTER_REFERENCE.md` sec 10.48.

**`CSTGKeybedInterface_Startup` (init_module step 14) is reconstructed
too** (2026-07-02) — confirms this project's own earlier finding (real
serial-port keybed handshake, not `OmapNKS4Module.ko`). Discovered
`sInstance` here is a real static/global object (offset arithmetic on
its own symbol address, not a pointer variable), unlike `CSTGGlobal`/
`CSTGHeapManager` elsewhere. **Corrected a real claim from an earlier
session**: scans exactly 6 ports (0-5), not "up to 7" — the real
loop-exit check computes port 6 but never actually attempts it, a
genuine off-by-one reproduced faithfully. Found and fixed a real bug in
this project's own first draft while building the host KAT: an initial
translation's port counter started at 0 instead of directly mirroring
the real register (starting at 1), silently retrying port 0 twice —
caught by the KAT's call-count assertions, not by re-reading the
disassembly. Written with `goto`s matching the real branch structure
directly. Confirmed via a real Kbuild rebuild: both functions defined,
removed from the unresolved list; 5 new fully expected symbols. New
host KAT: `verify/test_keybed_init`. Full detail: `MASTER_REFERENCE.md`
sec 10.49.

**`CSTGAudioManager_StartAudioEngine` (init_module step 13) is
reconstructed too** (2026-07-02) — fully resolves what this project had
only partially traced: the real gate is a device-list-length-dependent
loop of `CSTGThread::CreateRealTimeWithCPUAffinity()` calls plus 2
fixed ones, confirming the earlier "N+2" finding exactly with real
entry points and arguments now identified. **Resolved a real ambiguity
with direct evidence**: `StopAudioEngine`'s wrapper dispatches through
vtable slot 1 — plausibly the Itanium ABI deleting destructor under an
earlier modeling choice, but `StartAudioEngine`'s own failure path
calls the exact same slot as its own cleanup step, proving it's a
genuine "stop" method, not a destructor. Found and handled a genuine,
confirmed extension of `CSTGAudioManager`'s own minimum size (fields
past the previously-confirmed boundary) — extended the class and
updated an existing test's size assertion, verified empirically rather
than hand-computed after an initial hand calculation disagreed by 4
bytes (struct padding). Confirmed via a real Kbuild rebuild: all three
functions defined, removed from the unresolved list; 4 new fully
expected symbols. New host KAT `verify/test_audio_start` — passed
cleanly on the first attempt, a first for this whole effort. Full
detail: `MASTER_REFERENCE.md` sec 10.50.

**`stg_rtfifo_init` (init_module step 16) is reconstructed too, and is
the LAST of `init_module`'s own direct step-function externs**
(2026-07-02) — every one of the 17 real steps `init_module` calls now
has either a faithful reconstruction or a confirmed-real, deliberately
deferred extern. Creates 6 real RTAI FIFOs (confirmed minors/sizes) via
a small shared helper, then registers a "stg_direct" character device
(name extracted from `.rodata`). Real RTAI infrastructure, matching
this project's own confirmed `rtai_fifos.ko` boot-time load, not a
stub candidate. Confirmed via a real Kbuild rebuild: defined, removed
from the unresolved list; 4 new fully expected symbols. New host KAT
`verify/test_rtfifo_init` — passed cleanly on the first attempt, the
second such clean pass in a row. Full detail: `MASTER_REFERENCE.md`
sec 10.51.

**`CSTGThread::CreateRealTimeWithCPUAffinity` is reconstructed too**
(2026-07-02) — fully resolves the earlier "self-contained in OA.ko,
wraps real RTAI primitives" finding into an exact call sequence: query
the RTAI pthread-attr size at runtime, allocate via a genuine
variable-length stack array, configure/create the thread (a confirmed
literal `0x5000` stack size), then install debug traps and pin to CPU
on success, or tear the thread back down if debug-trap installation
fails. Caught a real bug in the host KAT's own mock (not the
reconstruction): the real `rtwrap_pthread_create` populates the
caller's task-handle field as a side effect, which the reconstruction
already correctly re-reads — the test's first-draft mock just hadn't
simulated that side effect yet. Confirmed via a real Kbuild rebuild:
defined, removed from the unresolved list; 9 new fully expected
symbols (every `rtwrap_*` helper this function calls). New host KAT
`verify/test_cpu_affinity`. Full detail: `MASTER_REFERENCE.md`
sec 10.52.

**`CSTGComPort` is reconstructed too** (2026-07-02) — confirms the real
16550-compatible UART hardware finding at full instruction fidelity
for 8 of its methods. **Fixed a real, latent bug**: an earlier
reconstruction had declared these as plain C-linkage functions, but
the real relocations target genuine mangled C++ methods — a plain C
symbol of that name could never have linked against the real one.
Caught a second, subtler instance of the same bug class via the Kbuild
rebuild's own diff: even after switching to a real class method, plain
`int` parameters mangled differently than the real enum-typed
signature — fixed by declaring the confirmed real nested enum types.
`Initialize` itself (2561 bytes, real Super-I/O chip probing) remains
a confirmed-real, deliberately deferred extern — correctly named now,
just not yet implemented. Confirmed via a real Kbuild rebuild: 4
broken symbols gone, 3 methods now genuinely defined, 6 new fully
expected hardware/RTAI primitives. New host KATs `verify/test_comport`
and an updated `verify/test_keybed_init`. Full detail:
`MASTER_REFERENCE.md` sec 10.53.

**`CSTGComPort::Initialize` is reconstructed too, completing
`CSTGComPort` entirely** (2026-07-02) — the largest single method in
this class (2561 bytes, real Winbond Super-I/O chip probing). Traced
the full algorithm: dual config-port (0x2E then 0x4E fallback)
unlock+chip-ID-validate, per-port LDN selection, base-address-register
combination, IRQ discovery, UART hardware reset, IRQ request/CPU
assignment (corrected the real argument order for `rtwrap_request_irq`
from a first assumption), and the same confirmed baud-rate/LCR/FCR/
MCR/IER bring-up already used by `SetBaudRate`. Two comPortIds (0 and
3) have confirmed additional LDN-specific config not implemented in
this pass — a real, deliberately documented gap, unlikely to matter
under bare QEMU since chip-ID validation fails first regardless.
Confirmed via a real Kbuild rebuild: `Initialize` now defined, removed
from the unresolved list; 4 new fully expected additions. New host KAT
`verify/test_comport_init`. Full detail: `MASTER_REFERENCE.md`
sec 10.54.

**`CSTGGlobal::RunVoiceModelFeedback`/`Initialize` are reconstructed
too, resuming the paused `CSTGGlobal` work** (2026-07-02) — picking the
two symbols the unresolved-symbol list actually named. `Initialize`
builds a 32-entry intrusive list and calls five further sub-managers,
**confirming a real open question from an earlier session**:
`CSTGGlobal+0x10` is a genuine embedded `CSTGControllerRTData`
sub-object. Caught and fixed a real transcription bug in an earlier
draft (mis-derived a node's own field offsets), and hit the same
host/target pointer-width overlap bug three separate times across
these methods' tightly-packed 32-bit target fields — fixed in the
reconstruction itself via explicit 32-bit storage, more faithful to
the real binary's own instruction width, not just a test workaround.
`CSTGGlobal`'s own 3124-byte constructor remains a confirmed-real,
deliberately deferred extern. Confirmed via a real Kbuild rebuild: both
methods defined, removed from the unresolved list; 11 new fully
expected additions. New host KAT scenarios in `verify/test_global.cpp`.
Full detail: `MASTER_REFERENCE.md` sec 10.55.

**`CSTGGlobal::CSTGGlobal()` is reconstructed too — the 3124-byte
constructor**, the largest single function this project has directly
transcribed instruction-by-instruction (2026-07-02). Confirms
`CSTGGlobal`'s real, staggering scale directly: default-constructs
2944 `CSTGProgram` (23 banks x 128, an exact match for the real
Kronos program-bank architecture), 1792 `CSTGCombi`, 200 `CSTGSequence`
+1 standalone, 128 `CSetList`, 598 `CSTGWaveSequence` — ~5665 sub-object
constructions total, surfacing 11 new sub-dependencies at once, the
largest single batch this project has surfaced. Confirms sec 10.55's
open question from the opposite direction: an independent constructor
call corroborates `CSTGGlobal+0x10` really is an embedded
`CSTGControllerRTData`. Found and transcribed a genuine real
irregularity honestly (a per-slot default-value table's 4th group
doesn't follow the same stride pattern as the first 3) rather than
"cleaning it up." Hit a real toolchain issue for the first time in this
project (`<cstring>`'s `memset()` breaks the real target-ABI build,
missing 32-bit multilib) — fixed with an inlined zero-fill loop,
matching every prior file's own established precedent. Confirmed via a
real Kbuild rebuild: the constructor now defined, removed from the
unresolved list; 11 new fully expected additions. New host KAT
`verify/test_global_ctor` — completes in under 20ms despite ~5665
constructions, passed cleanly after the toolchain fix. Full detail:
`MASTER_REFERENCE.md` sec 10.56.

**`CW83627` confirmed already complete + a batch of 5 engine-startup
methods reconstructed** (2026-07-02) — checking the real binary's own
symbol table shows `CW83627` has zero real methods, nothing further
needed. Picked the next targets directly from the unresolved-symbol
list: `CSTGFrontPanel`'s constructor/`Initialize()`,
`CMeteredDebugOutput`'s constructor, `CSTGCPUInfo`'s constructor/
`Update()`, and `CSTGSampleRateMonitor::Initialize()`. Found and fixed
a real bug in an earlier reconstruction pass: a prior call had passed
a literal null `this`, missing that the real disassembly's zero
immediate actually carries a relocation on the singleton pointer's own
address — the same confirmed idiom already found once before, now
confirmed a second, recurring time. Hit the recurring host/target
pointer-width overlap bug a fourth time, fixed the same established
way. Hit a new toolchain issue needing a different fix than precedent:
this file's genuine float arithmetic pulls in libgcc soft-float
helpers — since hand-rewriting wasn't practical here and the real
binary confirms genuine hardware FPU instructions are used, fixed with
a per-file Kbuild CFLAGS override instead. Confirmed via a real Kbuild
rebuild: all 5 methods defined, removed from the unresolved list; net
unresolved-symbol count actually DECREASED (204 → 200) — the first
time a single batch has reduced the total. New host KAT
`verify/test_engine_startup_bits`. Full detail: `MASTER_REFERENCE.md`
sec 10.57.

**`CSTGEngine::Initialize()` reconstructed** (2026-07-02) — 1901 bytes,
~44 sub-object constructions, the largest function body this project has
written. Corrects a sizing error in this project's own earlier survey
(sec 10.13): ten "Model" classes' "×2 per class" table rows were a
misinterpretation of the same 264-byte size recurring across eight
different classes. Directly confirms `CSTGRecordEvent : public
CSTGAudioEvent` from the instruction sequence itself (no constructor
symbol of its own — construction is inlined at one call site). Hit the
recurring host/target pointer-width hazard twice more, both via real
segfault+gdb traces — one a new variant (the KAT's own backing buffer
needed `mmap(MAP_32BIT)` since correct truncation logic alone isn't
enough if the buffer itself lives outside 32-bit address space).
Confirmed via a real Kbuild rebuild: defined, removed from the
unresolved list; 46 new fully expected additions, zero toolchain
pollution. Net unresolved-symbol count: 200 → 245. New host KAT
`verify/test_engine_init`. Full detail: `MASTER_REFERENCE.md` sec 10.58.

**Small high-leverage batch reconstructed** (2026-07-02) —
`CSTGHeapManager::Initialize()`/`Alloc()` (the real handle-based
allocator layered on `InitializeSTGHeap`'s ioremap'd region: a
sentinel-anchored active list + a 99999-entry free list of 20-byte
handles, bump-down cursor allocation), `stg_rtfifo_cleanup` (the last
undefined companion to step 16's `stg_rtfifo_init`), and five small
single-caller methods `CSTGEngine::Initialize()` already instantiates:
`CLoadBalancer::Initialize()`/`~CLoadBalancer()`, `CPowerOffTimer::
Initialize()`, `CSTGDiskCostManager::Initialize()`, `CSTGCommonLFO::
Initialize()`, `CSTGCommonStepSeq::Initialize()`. Flagged one real
open discrepancy (not resolved, doesn't block Bar 2): `CSTGHeapManager
::Alloc()`'s own confirmed handle-number formula disagrees with
`oa_heap.h`'s already-established `oa_heap_region()` formula by a
constant 12 bytes. `~CLoadBalancer()` surfaced a new confirmed base-
class relationship (`CEmergencyStealer`) at the destructor level,
whose newly-declared destructor rippled into 3 pre-existing test
files' own mock destructors (C++ implicit member-destruction chaining).
Confirmed via a real Kbuild rebuild: all 10 targets defined, removed
from the unresolved list; only 4 new fully expected additions. Net
unresolved-symbol count: 245 → 239 — the second time a single batch
has net-DECREASED the total. New host KATs `verify/test_heap_manager`
and `verify/test_engine_startup_bits2`. Full detail:
`MASTER_REFERENCE.md` sec 10.59.

**`CCostProfile::CCostProfile()` reconstructed** (2026-07-02) — 2009
bytes, discovers a genuinely new base class `CStartupFile` (real
constructor call confirmed directly in the disassembly, argument
`"CostProfile"` extracted from `.rodata`), which retroactively
explains why `CCostProfile::sInstance->_field4` was already confirmed-
real despite CCostProfile's own constructor never writing it — it's a
`CStartupFile`-owned field. Mostly mechanical: 200 unrolled zero
writes plus a real loop building 198 20-byte `CCostProfileEntry`
objects, each with a permanently-untouched `+0x0` field (a real
quirk, preserved not fixed). Total computed size (`0x12a0`)
independently cross-checks exactly against `setup_global_resources.
cpp`'s own already-confirmed allocation size from an earlier pass.
`CStartupFile` declared as an 8-byte opaque base with 2 confirmed-
real, deferred externs. Confirmed via a real Kbuild rebuild: defined,
removed from the unresolved list; exactly 2 new fully expected
additions, a wash (239 → 239). New host KAT `verify/test_cost_profile`,
clean on the first attempt. Full detail: `MASTER_REFERENCE.md` sec
10.60.

**`CSTGLFOBase::InitializeQuad()`/`CSTGStepSeqBase::InitializeQuad()`
reconstructed** (2026-07-02) — the two new dependencies sec 10.59's
LFO/StepSeq work surfaced, small and confirmed branch-free. Both
decompose into clean loops writing three shared singletons' fixed
sub-addresses into the passed-in quad block; final touched offsets
land exactly on the already-confirmed struct sizes with zero slack.
Discovered `CSTGLFOTables`/`CSTGMIDIClockSync` needed `sInstance`
statics added for the first time. Confirmed via a real Kbuild rebuild:
both defined, zero new symbol additions, net 239 → 237 — the third
batch in a row to net-decrease the unresolved count. New host KAT
`verify/test_lfo_stepseq_quad`, clean on the first attempt. Full
detail: `MASTER_REFERENCE.md` sec 10.61.

**`CSTGWaveSeqManager::CSTGWaveSeqManager()`/`Initialize()`
reconstructed** (2026-07-02) — discovers a genuinely new class,
`CSTGWaveSeqGenerator` (200 sub-objects, 0x120 bytes each, embedded
directly in the manager). Constructor's total confirmed size (0xe134)
independently cross-checks exactly against engine_init.cpp's own
already-confirmed allocation — the third such independent size
cross-check in a row. `Initialize()` threads all 200 generators into a
real intrusive doubly-linked list via a genuine push-front insertion —
notably a different order than `CSTGHeapManager`'s own free-list build
(sec 10.59, which appends at tail). Caught a real KAT segfault (mock
generator ctor not zeroing its own link fields) and a "same field
convention ≠ same traversal semantics" lesson (wrong chain field
assumed for head-to-tail walking on the first draft). Confirmed via a
real Kbuild rebuild: both defined, exactly 2 new fully expected
additions, net unresolved-symbol count unchanged at 237. New host KAT
`verify/test_wave_seq_manager`. Full detail: `MASTER_REFERENCE.md` sec
10.62.

**`CSTGMidiDispatcher::CSTGMidiDispatcher()`/`Initialize()`
reconstructed** (2026-07-02) — mostly a zeroing constructor plus a
real heap-slot resolution in `Initialize()` that RESOLVES sec 10.60's
own flagged "open discrepancy": the worried 12-byte mismatch between
`CSTGHeapManager::Alloc()`'s handle-number formula and `oa_heap.h`'s
established `oa_heap_region()` formula turns out to be a mistaken
assumption (slot 0 is the sentinel itself, not "the first real handle
entry"), not a real bug — confirmed via this independent call site's
own identical addressing. Discovers a new class, `CSTGMidiQueue`, via
its one static-shaped method `AllocReader(void*)`. Confirmed via a
real Kbuild rebuild: both defined, exactly 1 new fully expected
addition, net unresolved-symbol count 237 → 236. New host KAT
`verify/test_midi_dispatcher`. Full detail: `MASTER_REFERENCE.md` sec
10.63.

**`CSTGVectorManager::CSTGVectorManager()` reconstructed** (2026-07-02)
— 3279 bytes, the largest function since `CSTGGlobal::CSTGGlobal()`.
Discovers three new "vector envelope generator" classes
(`CSTGVectorEGXOnly`/`CSTGVectorEGXY`/`CSTGVectorEGCC`), 432/432/34
confirmed instances in a non-type-contiguous interleaved layout, two
confirmed real gaps preserved verbatim. Total size independently
cross-checks exactly against an earlier pass's own allocation — the
fourth such cross-check in a row. Caught and fixed a real mutex-
pointer host/target truncation bug before it shipped, then found the
same bug had already shipped in `CSTGWaveSeqManager` (sec 10.62) —
fixed both. `Initialize()` (2350 bytes, real virtual dispatch across
~868 objects) is a separate, comparably-sized follow-up task. Confirmed
via a real Kbuild rebuild: defined, 3 new fully expected additions,
net unresolved-symbol count 236 → 238 (expected increase). New host
KAT `verify/test_vector_manager`. Full detail: `MASTER_REFERENCE.md`
sec 10.64.

**`CSTGVectorManager::Initialize()` reconstructed** (2026-07-02) — the
deferred follow-up to the constructor. Five real phases: two 400-count
loops (EGXOnly/EGXY) with real vtable slot-0 dispatch + intrusive list
push-front insertion, a confirmed marker write, an EGCC "batch1" loop
(dispatch + literal index, no list), and a paired EGXOnly/EGXY
"batch1" loop sharing one index per pair with no list insertion.
Confirmed real asymmetry preserved verbatim: never touches the
constructor's own "batch2" object ranges — half of each type's
population is built but never activated here. Confirmed a third
instance of the "address of the singleton pointer" idiom. Confirmed
via a real Kbuild rebuild: defined, zero new symbol additions, net
unresolved-symbol count 238 → 237. New host KAT
`verify/test_vector_manager_init`. Full detail: `MASTER_REFERENCE.md`
sec 10.65.

**`CSTGVectorEGXOnly`/`EGXY`/`EGCC`'s own constructors reconstructed**
(2026-07-02) — all three small, branch-free. Discovers a genuinely new
base class, `CSTGVectorEGBase` (confirmed real, declared opaque/
deferred). Confirms the shared +0x3c/+0x40/+0x48 list-node/owner field
convention (even on EGCC, never actually list-inserted) plus a new
+0x44 self-pointer field. EGCC's own constructor sets four fields to
the same real global `STGVJSAssignInfo` and four 16-bit fields to a
centered `0x8000` default. Confirmed via a real Kbuild rebuild: all
three defined, net unresolved-symbol count 237 → 239. New host KAT
`verify/test_vector_eg_ctors`. Full detail: `MASTER_REFERENCE.md` sec
10.66.

**Bar 2 (real `kronos_vm` insmod of OA.ko itself, Eva observed launching)
is the actual redirected goal** and remains — `init_module`'s own
top-level structure, its most-referenced RTAI thread-creation
primitive, its keybed UART driver (`CSTGComPort`, now fully done), and
`CSTGGlobal`'s own constructor are all FULLY reconstructed; what's left is closing
the unresolved-symbol gap via the sub-targets still surfaced
(~150 remaining `UpdateXXX` handlers/
`CSTGEngine`/`CCostProfile`/
`stg_rtfifo_cleanup`/`RTAIInterruptHandler`/the newly-surfaced
`CSTGWaveSeqData`/`CSTGSlotVoiceData`/`CSTGProgram`/`CSTGCombi`/
`CSTGSequence`/`CSetList`/etc./the `rtwrap_*`
family's own real bodies, etc.), resolving
whether `loadmod.ko`'s `/korg/Eva` decryption hooks can be bypassed for
a VM test, and testing the keybed/`CSTGComPort` question directly (the
port is dynamically queried from live Super I/O chip state, not a
static constant — a bare QEMU environment will likely fail that query's
own validation and gracefully retry, per `CSTGKeybedInterface_Startup`'s
confirmed retry loop, worth testing rather than assuming).

## Driven by `/loop`

A 4-hourly session loop advances this: each tick reconstructs + verifies a bounded
batch of functions (auth first), marks them `verified` in the manifest, and commits.
