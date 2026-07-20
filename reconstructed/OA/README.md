# OA.ko (OA_322.ko) - reconstructed source

Drop-in source reconstruction of the Korg Kronos firmware-3.2.2 `OA_322.ko` -
the STG synthesis engine plus copy-protection layer (Linux 2.6.32.11 + RTAI,
x86-32, C++). The real binary is large: 22,195 functions total. This
directory reconstructs it incrementally, staged so the copy-protection /
authorization subsystem (needed to understand licensing and to get the
module past `insmod`) comes first, followed by shared utilities, then the
synthesis engine core, then voice models/DSP, then a final breadth sweep.

Every reconstructed unit is checked against the real disassembly and
relocations before being treated as done, and is exercised by a host-side
known-answer test in `verify/` so a regression shows up immediately on
rebuild. `PLAN.md` documents the staged strategy and verification
methodology (recompile, structural comparison against the real binary,
emulation/known-answer tests) in full.

This subsystem talks to the Atmel **NV2AC** security chip (via `stgNV2AC_*`,
exported by `OmapNKS4Module.ko`) and uses **GMP** big-integer math
(`__gmpz_powm`/`__gmpz_invert`, exported by `STGGmp.ko`) - it ties together
those two already-reconstructed modules. It also depends on `STGEnabler.ko`
for a `stg_*` shim layer over RTAI primitives. See "Module load order and
dependencies" below.

## Repository layout

```
OA/
  README.md                 this file
  PLAN.md                   staged reconstruction plan + verification methodology
  include/                  recovered type model (class structs -> headers)
  src/<subsystem>/          reconstructed .cpp, in staged order (auth first)
  manifest/oa_functions.csv per-function status, regenerated from Ghidra (gitignored, regenerable)
  verify/                   host-side known-answer test harness
```

## Reconstruction status

| Stage | Coverage |
|---|---|
| 0. Foundations | manifest, plan, and host verify harness in place |
| 1. Copy-protection / auth | Fully reconstructed - KLM stamping, AT88 handshake, product glue, AuthorizationStrings/CD-ROM check, `/proc/.oacmd` procfs plumbing, and `ProcessOACmd` (all 12 real commands, byte-exact dispatch) |
| 2. Shared utilities | Fully implemented and tested - crypto/hash primitives (`moancjsd82`, `DecodeBytesFromAscii`, `md5`), the `CSTGBankMemory` heap, `CSTGQuad`/list primitives, the `Scale*` leaf math family, and the `operator new`/`delete` allocator substrate |
| 3. Engine core | `init_module`'s full call graph is reconstructed, including `CSTGEngine::Initialize()` and `CSTGGlobal`'s 3124-byte constructor. `CSTGGlobal`'s broader ~195-method surface (mostly per-parameter `UpdateXXX` message handlers) is partially covered - see below for what's done and what remains |
| 4. Voice models & DSP | Not started |
| 5. Breadth sweep | Not started |

A from-scratch Kbuild rebuild of every file currently in `src/` produces a
genuine 32-bit ELF `OA.ko` whose only unresolved symbols are the
not-yet-reconstructed pieces described below (real kernel/RTAI primitives
and un-implemented class methods) - it links clean apart from those, and
none of the unresolved symbols are unexplained.

## Copy-protection and authorization (Stage 1)

Recovered from the symbol table, string references, and disassembly, with
every address checked against the real `OA.ko` 3.2.1 ELF symbol table.

### CSTGKLMManager

The Korg License Manager, at `.text+0x2de10` (`src/auth/klm_manager.cpp`).
Methods: `AuthorizeProduct`, `AuthorizeVoiceModel`/`Effect`/
`MultisampleBank`/`Builtins`, `IsAuthorizedVoiceModel`/`MultisampleBank`/
`Effect`, `GetKLMAddressForPatch`, `RunKLM`.

### SetupAtmelForAuthorizations - the AT88/NV2AC chip handshake

`.text+0x207a50` (`src/auth/atmel_setup.cpp`) drives the Atmel NV2AC
"GPA" chip handshake used to gate authorization. It is called directly from
`init_module`, which makes the chip handshake a **hard load gate**: if it
fails, `OA.ko` fails to `insmod` at all. This is a materially different
failure mode from the *other* AT88-adjacent check, `InitCdromSupport` (see
below), which is called later from inside `CSTGEngine::Initialize()` and
degrades gracefully instead of blocking module load.

`ParseAuths`/`ParseAuth` (see below) independently read the AT88 dongle
themselves - zones `0x10`/`0x18`/`0x20` (24 bytes), via `fFfFfFfFfFfF13` -
before touching any authorization token, and abort entirely if that read
fails. There is no "no dongle required" path through this code; every
authorization route touches the chip.

### CD-ROM / installed-product glue

- `CSTGInstalledEXProducts::AuthorizeProductByFilename` (`.text+0x481d0`)
  and `AuthorizeProductCallback` (`.text+0x47fa0`) - CD-ROM/install-path
  glue (`src/auth/products.cpp`).
- `InitCdromSupport` (`.text+0x40`, `src/auth/cdrom_check.cpp`) - the
  `loadmod.ko` presence/integrity check, plus the degradation block it
  gates inside `CSTGEngine::Initialize()` (`.text+0x1b0`), reproduced here
  as a standalone helper. The real call target is the genuine Linux kernel
  API `register_cdrom(struct cdrom_device_info *)`, hijacked by Korg's
  patched kernel as a covert integrity channel: the recovered function
  pointer (reached via a `- 0x02D5B9C3` subtraction step) is stored into
  the real globals `sXCmd`/`sCdromCommand`, then checked for the
  `0x22fb39cc` magic dword at offset `+5`.

### ParseAuths / ParseAuth / VerifyAuthorizationString

- `ParseAuths` (`.text+0x207c50`) - boot-time reader of
  `/korg/rw/Startup/AuthorizationStrings`, callback =
  `AuthorizeProductCallback` (confirmed via relocation in
  `CSTGInstalledEXProducts::Initialize`).
- `ParseAuth` (`.text+0x207890`) - decodes exactly 15 plaintext bytes (no
  16-byte UUID exists on this path) and its callback receives a plain
  4-character product code:
  `AuthorizeProductCallback(const char *code4)`. (`src/auth/parse_auth.cpp`)
- `VerifyAuthorizationString` (`.text+0x207de0`) - the runtime,
  front-panel-UI validate-only path (`callback=NULL`). Reads the same
  three AT88 zones via `fFfFfFfFfFfF13`, decodes the string directly with
  `DecodeBytesFromAscii` (no tokenizing needed), then calls `ParseAuth`.
  (`src/auth/verify_auth_string.cpp`)
- Per-patch checks (`CSTG*ModelPatch::IsUsingAnyUnauthorizedMultisamples`)
  are not yet reconstructed - Stage 3/4 territory, called from
  voice-model/patch code.

### ProcessOACmd - the /proc/.oacmd command dispatcher

`.text+0xa0c0` (1773 bytes, `src/auth/process_oacmd.cpp`) - fully
reconstructed, byte-exact, all 12 real commands. The dispatch skeleton and
the `AU:` (authorize) case route through
`CSTGInstalledEXProducts::VerifyAndSaveAuthString` (`.text+0x48290`,
`src/auth/products.cpp`) - the link between this dispatcher and the
`VerifyAuthorizationString`/`ParseAuth` chain above. Quirk preserved
faithfully: on the file-append success path, the function's return value is
actually the trailing-newline write's result, not the authorization result.

The complete, real command table, read directly from `OA.ko`'s rodata
(a documented "PR"/post-process command in other project notes does not
exist - `AfterProcess` is dispatched by `LA:*`, not a separate command):

| Command | Behavior |
|---|---|
| `LM` / `LD` | `LoadMultisample`/`LoadDrumSample`, final boolean argument `0` |
| `CM` / `CD` | Call the exact same `LoadMultisample`/`LoadDrumSample` as `LM`/`LD` (confirmed via identical relocated call targets) - **not** "close" operations, despite the name similarity - differing only in a hardcoded final boolean argument `1` (named `variant`; its exact semantic meaning isn't determinable from the call site alone) |
| `AU:` | Authorize - routes through `VerifyAndSaveAuthString` |
| `CL:<uuid>` | Closes one bank's PCM files (`ClosePCMDataFiles`) - a 39-character command, no trailing numbers unlike `LM`/`LD` |
| `CB:*` | A literal 4-byte command match (not a 2-char prefix) - closes *all* banks (`CloseAllBankFiles`) |
| `PT` | `CSTGPianoModel::RescanPianoTypes`, no argument parsing |
| `SO:*` | `CSTGInstalledEXProducts::ReInitialize` (returns `bool`, via the same `(result^1)&0xff` convention as `AU:`) |
| `PC` | Requires `strlen > 7`, parses `":%lu:%lu:%lu"`, calls `CSTGPCMPrecacheManager::Reset` - the parsed numbers do not map to `Reset`'s parameters in the order they appear in the command string (see the struct's own comment in `process_oacmd.cpp`) |
| `KI` | Parses `":%lu"` and, uniquely among these commands, writes the value directly into a heap-relative field (`heapbase+0x6a554`, exactly 8 bytes past `CSTGPCMPrecacheManager`'s own base) rather than calling a method |
| `LA:*` | A literal 4-byte match, calls `CSTGPCMPrecacheManager::AfterProcess()` (returns `bool`) |

All four of `LM`/`LD`/`CM`/`CD` share an identical special case: if
`AccessBank` returns a bank in a "reserved but not yet loaded" marker state
(`*(int*)bank == -1`), the dispatcher first calls `LoadBankMetaData()`; on
success it retries the normal load (same `variant`); on failure it calls
`ReleaseBank` and fails. This direct-load-vs-retry-after-metadata pair is
what produces the two near-identical call sites per command visible in the
relocation table.

A related bug found and fixed while reconstructing this area:
`oa_heap.h`'s `oa_heap_base()` checked for a `NULL` `CSTGHeapManager::
sInstance`, but every real call site (including `CSTGKLMManager::
AuthorizeMultisampleBank`) guards a specific sentinel value, `-44`
(`0xFFFFFFD4`), not zero.

### /proc/.oacmd plumbing

`oa_cmd_open`/`oa_cmd_close`/`oa_cmd_read`/`oa_cmd_write`
(`.text+0x9e60`/`0x9e80`/`0x9eb0`/`0x9f20`), `oa_cmd_fops` (`.data+0x4e0`),
`ParseOACmd` (`.text+0xa020`), `InitPcmModProcInterface`/
`CleanupPcmModProcInterface` (`.text+0xa060`/`0xa0a0`) -
(`src/auth/oa_cmd_proc.cpp`). `ParseOACmd` is a second, simpler entry point
(also calls `ProcessOACmd`, also updates the state machine) distinct from
the `oa_cmd_write` path; its external caller was not identified.
`oa_cmd_fops`'s field layout is the real, unmodified Linux 2.6.32
`struct file_operations` (read/write/open/release at the stock offsets).
`/proc/.oacmd` is created mode `0600`, uid=gid=`500` ("pocky"). State
machine globals: `sOACmdStatus` (0 IDLE / 1 READY / 2 PROCESSING / 3
RESULT) and `sOACmdResult` (the 4-byte result `oa_cmd_read` returns) - both
owned by this file; `ProcessOACmd` itself only ever receives
`&sOACmdResult` as a plain out-parameter.

## Shared crypto and utility primitives (Stage 2)

(`oa_crypto.h`/`oa_md5.h`, `src/crypto/`)

### Crypto/hash primitives

- **`md5_init`/`md5_append`/`md5_finish`** (`.text+0x4f57d0`/`0x4f5800`/
  `0x4f5900`) - the well-known public-domain "L. Peter Deutsch" MD5
  reference implementation, compiled in unmodified (the same pattern
  `moancjsd82` turned out to be for Blowfish): the context layout
  (`count[2]` at `+0x00`, `abcd[4]` at `+0x08` with the canonical RFC 1321
  initial constants) and the padding/finish sequence match exactly. Used by
  `ParseAuth`'s MD5 cross-check. Implemented in `src/crypto/md5.cpp`,
  tested against the official RFC 1321 Appendix A.5 vectors plus a
  chunked-vs-single-call append consistency check.
- **`fFfFfFfFfFfF13(zone, len, buf)`** - AT88 zone read. Real hardware I/O
  handled by `OmapNKS4Module.ko`; declared here as a call-contract extern
  only, out of scope for a software port of this file.
- **`DecodeBytesFromAscii(out, asciiIn)`** - the Crockford Base-32 decoder;
  this is also where the ">= 0x18 bytes" minimum-length gate lives.
  Implemented in `src/crypto/cb32.cpp`. Tested against an independent
  from-scratch Python implementation.
- **`moancjsd82(chipKeyMaterial, ciphertext, p3, plainOut)`** -
  Blowfish-CFB-64 (`.text+0x4f5f00`): treats its first argument as a
  16-byte circular key region (standard Blowfish S-box key schedule).
  Implemented in `src/crypto/moancjsd82.cpp` on top of a standard Blowfish
  port (`src/crypto/blowfish.cpp`, P/S-box constants extracted
  programmatically from this exact kernel tree's Blowfish source). Tested
  against a real hardware-extracted AT88 key/IV and independently produced
  ciphertext, and this codepath is the same primitive the EXs library
  auth-key generator (documented elsewhere in this repository) uses; it
  authorizes correctly on real hardware.

### CSTGBankMemory heap

(`include/oa_bank_memory.h`/`src/mem/bank_memory.cpp`) - a small, static
bump/arena allocator used throughout the synthesis engine for
bank-associated allocations. All three methods are tiny (31/6/37 bytes) and
genuinely static (no `this` at all - `AllocAligned`'s first register
argument is its real `size` parameter, not an instance pointer):

- `Initialize(base, size)` - 16-byte-aligns `base` up and `size` down,
  resets the allocation offset to 0.
- `SetTotalBytesToManage(size)` - overwrites the manageable-size global
  directly; a distinct call site from `Initialize`, skipping its
  re-alignment.
- `AllocAligned(size, alignment)` - a straightforward bump allocator:
  `ptr = align_up(base + offset, alignment); offset = (ptr - base) + size;
  return ptr`. There is no bounds/overflow check against the
  total-available-size global anywhere in this function - callers are
  trusted not to overrun the pool. That total-size global is read
  elsewhere purely for diagnostics, in a separate, not-yet-reconstructed
  function (`CSTGMultisampleBank::GetTestBankMemoryUsage`,
  `.text+0x33f70`) that almost certainly reports a used/total utilization
  statistic.

Tested (`verify/test_bank_memory.cpp`) against hand-traced arithmetic
worked out step by step in the test's own comments, since no third-party
reference exists for this Korg-internal bookkeeping.

### CSTGQuad / list primitives

(`include/oa_quad.h`/`src/mem/quad_list.cpp`) - a per-priority-bucket,
ascending-priority-sorted doubly-linked list used throughout the synthesis
engine's DSP graph. `CSTGQuad` and its list container are never their own
standalone functions in the binary - a small value type plus a fully
inlined template (`TListLinkLite<CSTGQuad>`) - so the layout and algorithm
below are reconstructed entirely from disassembling the two functions that
manipulate it: `CSTGVoiceModel::AddQuad`/`RemoveQuad`
(`.text+0x1a9c70`/`0x1a9d30`, 188/110 bytes).

- `mNext`/`mPrev` (`+0x00`/`+0x04`) - standard doubly-linked-list pointers.
- `mOwnerList` (`+0x08`) - which bucket a quad currently belongs to,
  `NULL` if unlinked (`RemoveQuad` no-ops silently in that case).
- `mPriority`/`mBucketIndex` (`+0x14`/`+0x16`, 2 bytes each) - the sort key
  and which `CSTGQuadList` bucket (12 bytes: head/tail/count) in the owning
  object's bucket table this quad belongs to.
- `AddQuad` inserts sorted ascending by `mPriority`; ties insert the new
  quad before the existing equal-priority one.
- Both methods also maintain a 2-byte "last-touched quad's priority" cache
  on the owning `CSTGVoiceModel` (`+0xd8`) - `AddQuad` always overwrites
  it; `RemoveQuad` resets it to the sentinel `0xffff` only if it currently
  matches the removed quad's own priority. Quirk preserved faithfully: the
  real comparison mixes a sign-extending load of the cache against a
  zero-extending load of the quad's priority - indistinguishable from a
  plain compare for priorities under `0x8000` (the expected range).
- Not reconstructed here (exist via relocation, genuinely higher-level):
  `CSTGVoiceModel::MoveQuadToCPU`, `CSTGVoiceAllocator::FreeQuad`,
  `CLoadBalancer::NotifyQuadAllocated/Freed`.

Tested (`verify/test_quad_list.cpp`) with hand-constructed insertion
sequences and expected list order worked out by hand, including a tie-break
case and both head/tail removal edge cases. The priority cache is scoped to
the voice model, not the bucket - a real ordering hazard for any test that
inserts into a second bucket partway through.

### Scale* leaf math family

(`include/oa_scale.h`/`src/mem/scale.cpp`) - six tiny linear-rescale/lerp
helpers: `ScaleLong`, `ScaleShort`, `ScaleWord`, `ScaleByte`, `ScaleChar`
(49-100 bytes each, sharing one algorithm differing only by
width/signedness) and `ScaleLongDouble` (94 bytes, a `long`-width sibling
that does the same math via the x87 FPU in double precision to avoid the
`long` version's int32 overflow on wide ranges). All map `value` from
`[inMin,inMax]` to `[outMin,outMax]`:
`outMin + (value-inMin)*(outMax-outMin)/(inMax-inMin)`, with a degenerate
case when `inMin==inMax` that **skips the division entirely** (not treated
as dividing by 1) - a faithfully preserved quirk. A single C++ template
(`ScaleInteger<T>`) implements all five integer variants: C++'s own
integer-promotion rules for narrower types reproduce the exact
movzx/movsx-to-32-bit widening the real disassembly does before its
`imul`/`idiv`. `ScaleLongDouble`'s FPU control word decodes to
truncate-toward-zero rounding (RC=11), matching a plain C++ `(long)` cast
from `double` exactly. Tested (`verify/test_scale.cpp`) with hand-computed
vectors including the degenerate case and an overflow scenario only
`ScaleLongDouble` gets right. Not reconstructed (bigger/more specialized):
`ScaleValToIndex`, `ScaleWhiteBlackCC`, `ScaleRTParmValue`.

### operator new/delete allocator substrate

(`include/oa_new_delete.h`/`src/mem/new_delete.cpp`) - `OA.ko` is a
freestanding kernel module (`-fno-exceptions`, no libstdc++) that defines
its own global `operator new`/`new[]`/`delete`/`delete[]` from scratch. All
four are tiny (15-byte) trampolines that forward to two more real,
locally-defined `OA.ko` functions, `stg_kmalloc`/`stg_kfree` (20/15 bytes),
which are themselves thin wrappers over the genuine Linux kernel
`__kmalloc(size, GFP_KERNEL)` (the `0xd0` flag value decodes to exactly
`__GFP_WAIT|__GFP_IO|__GFP_FS`, i.e. plain `GFP_KERNEL`, in this 2.6.32
kernel's encoding) and `kfree(ptr)`. No bespoke pool/arena logic of its own
at this layer - that's `CSTGBankMemory`/`CSTGHeapManager`, above it.
`__kmalloc`/`kfree` are declared `extern "C"` for the target-ABI compile
check (same treatment as `register_cdrom()`); the forwarding contract
itself (exact size/flags/pointer passthrough down every layer) is tested
(`verify/test_new_delete.cpp`) against host-side mock `__kmalloc`/`kfree`
definitions. Uses a `size_t` typedef (via `__SIZE_TYPE__`, no header
needed) rather than a literal `unsigned int`, matching `operator new`'s
mangled name `_Znwj` exactly on the real 32-bit target.

## Engine core (Stage 3)

### CSTGEngine

(`include/oa_engine.h`/`src/engine/engine.cpp`) - the STG synthesis
engine's top-level object. The ELF symbol table confirms exactly 7 distinct
methods (ctor/dtor + 5 real methods, no others exist):

| Method | Address | Size | Status |
|---|---|---|---|
| `CSTGEngine()` | `.text+0xe0` | 10 B | implemented |
| `~CSTGEngine()` | `.text+0xf0` | 191 B | implemented |
| `RunAudioTick(unsigned int)` | `.text+0xc10` | 44 B | implemented |
| `PostAudioTick()` | `.text+0xc40` | 94 B | implemented |
| `RunEffects()` | `.text+0xca0` | 20 B | implemented |
| `RunFileDaemonSynchronization()` | `.text+0xcc0` | 90 B | implemented |
| `Initialize()` | `.text+0x1b0` | 1901 B | implemented |
| `PreAudioTick()` | `.text+0x920` | 741 B | documented, not compiled in (see below) |

The six small methods are pure "call these singletons, in this exact order"
dispatchers, tested (`verify/test_engine.cpp`) by asserting the exact call
sequence against mock manager implementations.

The destructor cross-checks cleanly against `Initialize()`'s construction
order: exactly four classes (`CLoadBalancer`,
`CSTGAudioDriverInterfaceKorgUsb`, `CSTGAudioManager`, `CPowerOffTimer`) are
real heap-`new`'d in `Initialize()` (confirmed via `_Znwj` relocations),
and those are exactly the four the destructor tears down with a real
dtor-call-then-`operator delete` (or, for `CSTGAudioDriverInterface`, a
virtual `delete` through its vtable, since the real allocated object is the
derived `KorgUsb` subclass). Every other manager `Initialize()` constructs
is placement-new'd directly into `CSTGBankMemory::AllocAligned` memory, and
the destructor correspondingly only in-place-destructs them (no `operator
delete`).

`Initialize()` is implemented and constructs roughly 40 manager/model
singletons (via `CSTGBankMemory::AllocAligned` + placement-new, sizes
ranging from 16 bytes to 282,284 bytes) in a fixed order.

`PreAudioTick()` is fully disassembled and its structure fully understood
(roughly 40 sequential per-tick calls into those same singletons), but is
deliberately not yet added to the compiling tree: writing it for real needs
the remaining manager classes declared with real member data, not bare
forward declarations, to avoid overstating how much of each manager is
actually reconstructed.

### CSTGGlobal

(`include/oa_global.h`/`src/engine/global.cpp`) - the engine's central
"global state" object. It has roughly 195 methods and a 3124-byte
constructor - far too large to reconstruct in one pass, so only the pieces
`CSTGEngine`'s own methods actually call, plus a set of the smallest/
cleanest handlers, are done so far.

**Object scale.** `CSTGGlobal`'s real object is enormous - confirmed field
offsets land around `0x29c9900`-`0x29c9fc0`, roughly 43.6 MB into the
object. It almost certainly embeds large audio/sequencer buffers directly
rather than holding pointers to them, so a from-scratch struct layout isn't
realistic; methods here use raw `(unsigned char *)this + OFFSET`
arithmetic instead.

**Constructor** (`CSTGGlobal::CSTGGlobal()`, 3124 bytes, the largest single
function transcribed instruction-by-instruction in this project) -
default-constructs 2944 `CSTGProgram` (23 banks x 128, matching the real
Kronos program-bank architecture), 1792 `CSTGCombi`, 200 `CSTGSequence`
plus one standalone, 128 `CSetList`, and 598 `CSTGWaveSequence` - about
5665 sub-object constructions total. `CSTGGlobal+0x10` is a genuine
embedded `CSTGControllerRTData` sub-object (confirmed both from this
constructor and independently from `CSTGGlobal::Initialize()`, below). One
irregularity preserved rather than "cleaned up": a per-slot default-value
table's 4th group does not follow the same stride pattern as the first 3.

**`Initialize()`** - builds a 32-entry intrusive list and calls five
further sub-managers.

**`IncrementMicrosecondCount()`** (`.text+0x93b0`, 74 bytes) - implemented
and tested. It maintains a 64-bit microsecond counter using a 4-phase
Bresenham-style fractional-rate accumulator (adds 667 three calls out of
four, 666 on the fourth): `3x667 + 1x666 = 2667`, `2667/4 = 666.75`
exactly, meaning it is called at precisely 1500Hz and tracks elapsed
microseconds exactly despite 666.75 not being a whole number.

**`RunVoiceModelFeedback()`** (`.text+0x4690`, 123 bytes) and
**`SetCurrentModeTempo(float)`** (`.text+0x4b20`, 90 bytes) - both fully
disassembled and declared (so `CSTGEngine::PostAudioTick` compiles against
them) but deliberately not implemented: `RunVoiceModelFeedback` walks into
an unrecovered `CSTGSlotVoiceData`-shaped object and calls an unidentified
virtual function; `SetCurrentModeTempo` computes `log2(tempo/120)` (the
constants `1/120`, `+16.0`, `-16.0` are extracted from the real rodata and
match) clamped to `[-16,16]`, but its exact FPU-conditional-move branch
structure was not resolved with enough confidence to ship.

**Message handlers.** Roughly 150 of `CSTGGlobal`'s ~195 methods are
`UpdateXXX(CSTGMessageContext&, STGConvertedParam&)` handlers, one per
settable global parameter. Implemented so far:

| Handler(s) | Size | Behavior |
|---|---|---|
| `UpdateMuteMode` | 9 B | raw int store |
| `UpdateRearPanelControllerReset`, `UpdateTmbrTrkOscTransposeType` | 12 B each | bool-conversion store into adjacent flag fields |
| `UpdateUserAllNoteScale` | 13 B | writes into an array slot selected by the message context's index field |
| `SetSplitLayerWorkState(bool)` | 7 B | the smallest handler found - direct byte store |
| `UpdateLRBusIndivAssign` | 20 B | does not touch `CSTGGlobal` at all - computes `this+4` and delegates to `CSTGAudioBusManager::SetLRBusIndivAssign(int)` (11 B, looks up `STGAPILR2IndivToPhysBusId`); why `CSTGGlobal+4` aliases as a `CSTGAudioBusManager*` is not determined - the real singleton is confirmed separately allocated elsewhere |
| `UpdateFootswitchPolarity` | 29 B | same conditional-delegation shape, to `CSTGControllerRTData::SetFootSwitchPolarity` |
| `UpdateSPDIFSampleRate` | 23 B | writes into the *message context*, not `CSTGGlobal`, and writes a literal constant `6` rather than the incoming parameter value, gated on a `CSTGGlobal` flag and the parameter being nonzero |
| `TranslateAudioInputParamId` | 21 B | pure lookup into a confirmed 8-entry rodata table; declared `static` despite being a real class member |
| `UpdateSongPunchMIDIChannel` and 22 siblings | 30 B each | a 23-method family sharing one shape and one shared array base; implemented as one private helper plus 23 thin public wrappers |
| `UpdateSeqParamMidiOutMode`, `UpdateAfterTouchCurve`, `UpdateBankMap`, `UpdateVelocityCurve`, `UpdateSeqTrackMidiOutMode`, `UpdateVectorMIDIOut`, `UpdateNoteReceive`, `UpdateDamperPolarity` | - | 8 raw-store handlers |
| `UpdateCombiChangeEnable`, `UpdateAftertouchChangeEnable`, `UpdateControlChangeEnable`, `UpdateSysExEnable` | - | bool-converted stores into 4 consecutive flag bytes, `+0x6d7..+0x6da` |
| `UpdateHeadroom` | 53 B | the first confirmed case of `STGConvertedParam.value` read as a **float** rather than `int`; broadcasts into the same `gAllPlusHeadroom`/`gAllMinusHeadroom` globals `CSTGAudioBusManager`'s constructor sets to unity gain |

Two clusters are deliberately deferred, not yet implemented: the 9
`UpdateAudioInputXXX` handlers all delegate into a whole separate,
~30-method `CSTGAudioInput` class (some methods up to 962 bytes); the
22-method `UpdateXXXCCAssign` family (141 bytes each) is a genuinely
complex table-scan-and-reassign mechanism that clears old CC bindings and
updates a reverse-lookup table.

### Manager constructors

(`src/engine/managers.cpp`) - 21 of the roughly 40 classes named in
`Initialize()`'s construction table have real constructors implemented,
bottom-up by size: `CSTGDiskCostManager`, `CSTGSamplingDaemon`,
`CSTGHDRFileReader`, `CSTGHDRFileWriter`, `CSTGStreamingFileReader`,
`CSTGFileCloser`, `CSTGMetronome`, `CSTGTempoUtils`, `CSTGFileOpener`,
`CSTGCDWorker`, `CPowerOffTimer`, `CSTGAudioDriverInterfaceKorgUsb`,
`CSTGVoiceModelManager`, `CLoadBalancer`, `CSTGMonitorMixer`,
`CSTGAudioBusManager`, `CSTGEffectManager`, plus partial implementations of
`CSTGHDRManager`, `CSTGVoiceAllocator`, `CSTGAudioManager`, and
`CSTGMessageProcessor` (see below). Tested via
`verify/test_managers.cpp`, which poisons each object's memory before
construction so untouched fields cannot accidentally read as zero and pass
by coincidence.

Notable finds:
- `CSTGAudioBusManager` (60 bytes) resets two shared module-global 4-float
  arrays, `gAllPlusHeadroom`/`gAllMinusHeadroom`, to `{1,1,1,1}`/
  `{-1,-1,-1,-1}` - very likely per-bus clipping headroom reset to unity
  gain on construction.
- `CSTGEffectManager` (103 bytes) has a lone zeroed dword, a 198-element
  zeroed table, a confirmed 72-byte gap, two `120.0f` literals (plausibly a
  default-tempo pair, unconfirmed), and four more zeroed dwords.
- `CSTGMetronome`'s constructor opens with `AND [this],0xfa` (clearing 2
  specific bits of a flags byte, not overwriting the whole byte) -
  evidence of a base class or preceding sub-object already initialized
  before this constructor runs, not otherwise investigated.
- `CSTGDiskCostManager`'s constructor is a single `sInstance = this` and
  nothing else, despite a confirmed 72-byte real object - every other
  field is either zero-initialized by its `CSTGBankMemory` allocation
  already, or set later by its own (not yet reconstructed) `Initialize()`.
- `CSTGFileOpener`'s 953-byte constructor is almost entirely one repeated
  16-byte zeroing pattern (32 times), reconstructed as a C++ `for` loop.
  Confirmed 544-byte total size: `12 (header) + 4 (gap) + 32x16 (slots) +
  16 (ring control) = 544`.
- `CPowerOffTimer`'s constructor allocates a real mutex
  (`rtwrap_malloc(get_sizeof_rtwrap_pthread_mutex())`) and initializes it
  (`rtwrap_pthread_mutex_init`), storing the handle at `+0x18` - the same
  field its destructor tears down via `rtwrap_pthread_mutex_destroy`+
  `rtwrap_free`. The handle is stored as an explicit 32-bit value (not a
  native host pointer), since the real 32-bit target has room for exactly
  a 4-byte pointer at that offset (the object's last field).
- `CSTGAudioDriverInterfaceKorgUsb` is the first reconstructed class with
  a real inherited vtable; its fields are declared as ordinary named C++
  members rather than raw offset-cast byte arrays, so the compiler places
  them correctly relative to whatever vtable-pointer width the build
  actually uses.
- `CLoadBalancer` embeds a real, separately-named class,
  `CEmergencyStealer` (confirmed via its own constructor's relocation and
  an independent `sInstance` singleton), as its first 36 bytes - declared
  as an opaque member with an intentionally empty constructor body.

Every class from the original ~40-manager construction table has now
either been reconstructed (21 constructors, 4 partial) or definitively
confirmed to have no constructor to reconstruct (`CSTGMidiPortManager`,
below).

### Partially reconstructed managers

Four managers are large or complex enough that full reconstruction was out
of scope for a single pass. Each states plainly what is implemented, what
is confirmed-but-not-implemented, and (where relevant) whether the
declared `sizeof()` is exact or a lower bound.

**`CSTGHDRManager`** (1061-byte constructor) - relocation resolution
across the whole constructor reveals a 6-sub-object-type aggregate
spanning a confirmed minimum of ~101 KB, by far the largest object here.
Implemented: a `CSTGPlaybackBuffer[16]` array (88-byte stride) and a
`CSTGMonitorMixerChannel[16]` array with a real quirk - each channel's
real size is 172 bytes but the array stride is 192, and channels 0-14 (not
the last) get 3 extra tail dwords zeroed by the outer constructor. Both
sub-classes are declared opaque (empty constructors). Confirmed but not
implemented: a `CSTGSampler` (real size unknown), a 17th standalone
`CSTGPlaybackBuffer`, a `CSTGHDRCircularBuffer`, a `CSTGPlaybackEvent`, and
a `CSTGAudioInputMixerBase` constructed then vtable-patched to
`CSTGCDAudioPlay::CCDAudioInputMixer` - `CSTGCDAudioPlay::sInstance` is
aliased to point at the embedded `CSTGHDRCircularBuffer` inside this very
object, not a separately allocated instance. This reconstruction's
declared `sizeof()` (`0x11a4`) is far smaller than the real ~101 KB
object, stated plainly rather than glossed over.

**`CSTGVoiceAllocator`** (4491-byte constructor) - relocation resolution
confirms a minimum size of `0x44eac` (~281 KB, larger than
`CSTGHDRManager`) - plausibly because a voice allocator needs full
per-voice state for every voice the engine can allocate. Implemented: a
50-element self-referencing "empty list node" array, a 400-element array
where each record points 4 fields back at its own base, and
`CSTGSlotState[16]` (declared opaque, confirmed 6284-byte stride).
Confirmed but not implemented: a second 400-element array whose
per-element body is too detailed to fully trace (dozens of individual
field writes, including 5 copies of a UUID-prefix constant per element),
and a nested loop (an outer counter wrapping 10
`CModelVoiceRequirementsData::Clear()` calls per iteration). A real
recursive `pthread_mutex_t` (same allocate/init shape as
`CPowerOffTimer`'s mutex, but with a recursive-attribute setup) is the
last confirmed member-touching write. Unlike `CSTGHDRManager`, this
reconstruction's declared `sizeof()` matches the real confirmed minimum
size exactly.

**`CSTGAudioManager`** (5785-byte constructor, confirmed minimum size
`0x455c`/~17.3 KB) - the first partial class that is also polymorphic (a
real vtable, `_ZTV16CSTGAudioManager`); it keeps a real virtual destructor
and uses only named C++ members so the vtable-pointer-width host/target
ABI rule applies correctly. Implemented: two complete mutex+condvar pairs
and four trailing scalar constants (`256`, `255`, `1/256`, `1.0`,
plausibly a lookup-table size/mask/reciprocal/unity-gain quadruple,
unconfirmed). Confirmed but not implemented: a ~15 KB middle region
containing a CPU-core-count-dependent branch (via `CSTGCPUInfo::
sInstance`) and 13 profiler "slots" (each a `CProfiler`+`CDurationStats`
pair, 3 also with a `CSTGFrontPanelStatusReporter`) - confirmed *not* a
uniform array, since the measured slot-to-slot byte deltas are mostly
`0x10f` but sometimes `0x15f`.

**`CSTGMessageProcessor`** (5930-byte constructor, 664 relocations - more
than any other class here, and by far the most heterogeneous). Confirmed:
three unsolicited-message sender/message pairs (ProgramSlot/
ControllerInfo/IFX, 6 distinct vtabled types, each Sender embedding a
32-element `CSTGDelayedMsg` queue, with a confirmed size/stride difference
between the ProgramSlot sender and the other two); `sInstance = this` set
**immediately after** those pairs - a genuine exception to every other
manager here, where `sInstance` is set last; a real `CEffectorDatabase*`
heap-`new`'d at `+0x64`; ~15 `AllocAligned`-backed buffers and 14 distinct
`CSTGXxxMsgHandler` sub-objects (one per message category this processor
dispatches to); and 198 `CEffectorDatabase::Register()` plus 8
`CMOSSAlgorithmDatabase::Register()` calls that register individually
heap-`new`'d effects into an *external* database rather than touching this
object's own layout (why the constructor is 5930 bytes of code despite a
comparatively modest confirmed object size). Scoping decision: only
`sInstance` (at its confirmed exact position) and the confirmed minimum
size are implemented - modeling the three unsolicited-message pairs
faithfully would need 7 new opaque vtabled sub-classes for comparatively
little value. The confirmed minimum size (`0x1040`) is stated as a
**lower bound, not an exact `sizeof()`** - unlike `CSTGVoiceAllocator`'s
and `CSTGAudioManager`'s exact matches.

### CSTGMidiPortManager

Investigated to a definitive close: no `CSTGMidiPortManagerC1Ev`/`C2Ev`
symbol exists anywhere (defined or imported); the class is not present in
`CSTGEngine::Initialize()`'s ~44-entry construction table; `sInstance` has
zero relocations writing to it anywhere in `OA.ko`; and the real
destructor (264 bytes) operates entirely on the class's two static array
members (`sMidiInPorts`/`sMidiOutPorts`), never touching `this`-relative
memory. Conclusion: its implicit default constructor is genuinely empty
(nothing per-instance to initialize), which is exactly why no constructor
symbol was ever emitted - a confirmed structural fact, not a search gap.
All real bring-up happens in the separate, already-named
`CSTGMidiPortManager::Initialize()` (790 bytes) instead.

### init_module - the real boot sequence

`init_module` (`.init.text+0x0`, 847 bytes exactly) is the actual `insmod`
entry point and runs first - `CSTGEngine::Initialize()` is a later,
downstream call, not the top of the boot chain.

It is a linear "call subsystem init, check result, hard-fail via a
partial-unwind cascade, or continue" chain: C++ static-constructor bring-up,
a CPUID feature probe, CPU-affinity pinning, an optional PID-signal step
that degrades gracefully if its target file is missing, then eleven
subsystem `Initialize`-style calls in sequence - every one of which is a
hard `insmod`-time failure if it doesn't return success, unwinding cleanly
through however much of init actually completed:

1. `InitializeSTGHeap` (step 5) - finds a free MMIO region, `ioremap_cache`s
   it, zeroes it, hands it to `CSTGHeapManager_Initialize` (see below).
2. `InitSharedMemProcInterface` (step 6) - `create_proc_entry(".shm", 0600,
   NULL)`, uid=gid=500, `proc_fops` set on success.
3. `InitPcmModProcInterface` (step 7) - exactly the Stage 1 `/proc/.oacmd`
   registration (`src/auth/oa_cmd_proc.cpp`); the real proc entry name,
   from rodata, is `.oacmd`.
4. `setup_global_resources` (step 8, "the crux") - the largest single
   function reconstructed in this project (7267 bytes, ~11x the
   next-largest). Constructs nearly the whole engine (`CSTGGlobal`,
   `CSTGEngine`, `CSTGFrontPanel`, `CCostProfile`, `CSTGCPUInfo`,
   `CMeteredDebugOutput`, `CSTGSampleRateMonitor`, `CSTGASK`, plus ~20 MB
   of heap allocations and a ~168 KB hardware-status struct), in a fixed
   order of ~42 calls with three hard-fail checks (the `CSTGCPUInfo` check
   fires last despite that object being allocated first).
5. **`SetupAtmelForAuthorizations`** - see above; the AT88 chip GPA
   handshake.
6. `setup_stg_decrypt_daemons`
7. `load_global_resources`
8. `setup_stg_daemons`
9. **`CSTGAudioManager_StartAudioEngine`** (step 13) - the real gate is a
   device-list-length-dependent loop of `CSTGThread::
   CreateRealTimeWithCPUAffinity()` calls plus 2 fixed ones. The audio
   driver's own return value is discarded unconditionally by
   `CSTGAudioManager::StartAudioEngine()` - step 13's actual pass/fail does
   *not* depend on any `KorgUsbAudio*` symbol, even though an unresolved
   `KorgUsbAudio*` symbol would still block `insmod` outright at link time.
10. **`CSTGKeybedInterface_Startup`** (step 14) - **not** a call into
    `OmapNKS4Module.ko`. This is `OA.ko`'s own serial-port keybed
    handshake: it scans exactly 6 COM ports (0-5 - the real loop-exit
    check computes port 6 but never attempts it, a genuine off-by-one
    reproduced faithfully) via a real 2561-byte UART driver class
    (`CSTGComPort`, entirely inside `OA.ko` - see below), sending a probe
    byte and waiting for a real keybed board to ACK. `sInstance` here is a
    real static/global object (offset arithmetic on its own symbol
    address), unlike `CSTGGlobal`/`CSTGHeapManager` elsewhere.
11. `stg_rtfifo_init` (step 16, the last of `init_module`'s own direct
    step-function externs) - creates 6 real RTAI FIFOs (confirmed
    minors/sizes) via a small shared helper, then registers a
    "stg_direct" character device (name from rodata).

Every one of these 17 real steps now has either a faithful reconstruction
or a confirmed-real, deliberately deferred extern.

`CSTGThread::CreateRealTimeWithCPUAffinity` wraps real RTAI primitives
directly via `rtwrap_*` helpers that are defined *inside* `OA.ko` itself
(not exported by `loadmod.ko`, as an earlier project note assumed) -
`rtwrap_pthread_create`, `rtwrap_set_debug_traps_in_rt_task`, and similar,
wrapping genuine RTAI kernel primitives (`rt_task_init`, `rtheap_alloc`,
`rt_set_runnable_on_cpuid`, etc.) directly. The sequence: query the RTAI
pthread-attr size at runtime, allocate via a variable-length stack array,
configure/create the thread (a confirmed literal `0x5000` stack size),
install debug traps and pin to CPU on success, or tear the thread back
down if debug-trap installation fails.

`CSTGComPort` (see below) is fully reconstructed, including its
2561-byte `Initialize()`.

### Module load order and dependencies

Linux resolves every undefined symbol at `insmod` time, so a missing
companion module fails `OA.ko`'s own load immediately with "unknown
symbol." The real dependency order:

1. RTAI core (`rtai_hal.ko` -> `rtai_smp.ko` (or the substitute
   `rtai_sched.ko`) -> `rtai_sem.ko` -> `rtai_ndbg.ko` -> `rtai_fifos.ko`)
   - `STGEnabler.ko` needs real RTAI symbols (`rt_linux_use_fpu`,
   `rt_set_oneshot_mode`, `start_rt_timer`).
2. `STGEnabler.ko` - the `stg_*` shim layer (`stg_set_cpus_allowed`,
   `stg_cpumask_of_cpu`, etc.).
3. `STGGmp.ko` - the `__gmpz_*`/`mpz_*`/`mpn_*` bignum symbols
   `cm_ComputeChallenge` needs.
4. The AT88 chip provider (`OmapNKS4Module.ko` on real hardware) -
   `stgNV2AC_sync_cmd`/`stgNV2AC_sync_read_cmd`.
5. The NKS4 board driver (`OmapNKS4Module.ko`) - the `COmapNKS4*`/
   `OmapNKS4OutputFifo_*`/`SetupNKS4Calibration` family.
6. `KorgUsbAudioDriver.ko` - `KorgUsbAudio*`/`KorgUsbMidi*`/
   `USBMidiAccessory_SetDrumPadClient`.
7. `OA.ko` - last. Modules 2-6 have no dependencies on each other or on
   `OA.ko`, so only "after 1, before 7" is load-bearing for their relative
   order.

`loadoa/loadoa.c` is the real orchestrator that drives this sequence on
device. Real extracted `loadoa.c` source shows `insmod OA.ko` (fatal on
failure) happening *before* `insmod KorgUsbAudioDriver.ko` (explicitly
non-fatal in the source's own comment) - see Known limitations below for
why this is surprising given `OA.ko`'s own confirmed `GLOBAL` (not `WEAK`)
relocations against `KorgUsbAudio*` symbols.

`KorgUsbAudioDriver.ko` turned out to be a single combined audio+MIDI
driver exporting roughly 20 symbols; every audio-family function `OA.ko`
calls on it takes no arguments at all (base+stride ring-buffer accessors
and small status/flag reads).

For testing without real RTAI-capable hardware, a substitute module
(`reconstructed/RTAIVirtualDriver/`) provides the same RTAI-family symbol
surface `OA.ko` needs using ordinary Linux kthreads/semaphores/workqueues
instead of genuine hard-real-time scheduling; it must load before
`STGEnabler.ko`. Real hardware must still use the genuine RTAI stack - the
substitute is never appropriate there.

### Vtable population bugs found and fixed

Three real ABI bugs surfaced while bringing engine classes with real
vtables into the reconstructed tree, all found via `objdump -r` against
the real `OA_real.ko`'s own vtable relocations rather than assumption:

1. **`CSTGAudioDriverInterface`/`CSTGAudioDriverInterfaceKorgUsb`**
   previously declared only a pure virtual destructor, producing a 0x10-byte
   compiler-generated vtable. The real `_ZTV31CSTGAudioDriverInterfaceKorgUsb`
   is 0x68 bytes - 24 virtual slots: dtor x2, `Initialize`/`Start`/`Reset`,
   `GetAudioInputFromDriver`/`WriteAudioOutsAndWait`/`WriteAudioOuts`/
   `KeepSynchronized`, `GetNumDriverOutputChannels`/
   `GetNumDriverInputChannels` (const), 8 pure-virtual Mute/UnmuteAll
   Audio/Outputs/Inputs, 4 pure-virtual Mute/UnmuteAudioOutput/Input
   (`unsigned int`), and 3 non-overridden base-only DMA-buffer-counter
   methods. Fixed by adding all 22 missing virtual methods to both classes
   with real bodies where cheap/confirmed (`Initialize`/`Start`/`Reset`/
   `KeepSynchronized` call the reconstructed `KorgUsbAudioInitialize`/
   `Initialized`/`Start`/`Input`/`InputDone`/`Output`/`OutputDone` exports;
   all ten Mute/Unmute overrides are confirmed real 1-byte `ret`s even on
   real hardware) and safe no-op stand-ins for the two genuinely large
   real-time audio-DSP bodies (`GetAudioInputFromDriver`/`WriteAudioOuts`,
   1428/638 bytes of SSE sample-format-conversion code, out of scope per
   the RTAI/audio-DSP virtualization policy this project follows). Also
   fixed the separately-broken `CSTGAudioDriverInterface::sInstance`,
   which a real base-class constructor sets but which had no explicit base
   constructor declared here at all.
2. **`CSTGAudioManager`** declared `virtual ~CSTGAudioManager()`, but the
   real `_ZTV16CSTGAudioManager` has *no* destructor slot at all - its
   three real slots are `Initialize()` (5387 bytes), `StopAudioEngine()`
   (95 bytes), `DoAudioManagerThreadProcessing()` (956 bytes). This was a
   genuine ABI mismatch: `CSTGEngine::Initialize()`'s vtable-slot-0
   dispatch expects `Initialize()`, but `engine.cpp`'s explicit
   `~CSTGAudioManager()` destructor call also dispatches through that same
   slot 0 (an explicit destructor call still uses virtual dispatch when
   the destructor is `virtual`) expecting the Itanium ABI's D1 slot - two
   incompatible uses of the same memory. Fixed at the root: `virtual`
   removed from the destructor declaration, replaced with an explicit
   `void *_vtablePtr` first member (reserving the same native-pointer-width
   leading slot without C++ vtable machinery), and the three real slots
   populated with safe no-op stand-ins (`Initialize`/
   `DoAudioManagerThreadProcessing` are large real-time audio-engine
   bodies out of scope per policy; `StopAudioEngine`'s own callees
   `CSTGThread::Delete`/`CSTGAudioThread::Shutdown` are not reconstructed
   either).
3. **`CSTGVectorEGBase`/`CSTGVectorEGXOnly`/`CSTGVectorEGXY`/
   `CSTGVectorEGCC`** - all four real vtables were already the correct
   confirmed size (12 bytes = 1 real slot each), just left all-zero. Fixed
   by declaring a real `virtual void Init()` on `CSTGVectorEGBase` and an
   override on each derived class, letting the compiler emit the real
   vtable and populate slot 0 itself. `CSTGVectorEGBase`/`CSTGVectorEGCC`
   are fully self-contained; `CSTGVectorEGXOnly`/`CSTGVectorEGXY`
   implement the portion reachable the very first time `Init()` runs on a
   freshly-constructed object, deferring ~150-300 bytes each of intrusive
   pool-removal logic that is provably unreachable at that point.

### stg_get_current_task per-CPU relocation bug

`stg_get_current_task()` (`src/stub/bar2_stubs_c.cpp`) used a literal,
hardcoded `mov %fs:0x0` displacement, based on a misreading of the real
binary's raw disassembly: `objdump -d` without `-r` prints an unresolved
ELF relocation's placeholder bytes as `00 00 00 00`, identical-looking to a
real immediate zero. `readelf -r`/`objdump -dr` against the real binary
shows every one of its 8 real `mov %fs:0x0` call sites carries an
`R_386_32` relocation against `per_cpu__current_task`, resolved by the
kernel's own module loader at `insmod` time to the running kernel build's
real per-CPU offset. The factory kernel's own per-CPU-area setup is stock,
unmodified upstream Linux 2.6.32 x86-32 SMP code - it was never the bug.
Fixed by referencing `per_cpu__current_task` by name in the inline asm
operand instead of a literal `0`, which makes the assembler emit the same
kind of relocation the real binary has.

### InitializeSTGHeap

`InitializeSTGHeap` (`src/init/stgheap_init.cpp`, `init_module` step 5)
scans `iomem_resource` for the single largest unclaimed top-level gap and
`ioremap_cache`s it as `OA.ko`'s private ~100-200MB+ bank-memory pool. On
real hardware this is physically-present reserved DRAM. It zeroes the
region and hands it to `CSTGHeapManager_Initialize`. Floating-point math
in this file compiles fine at the source level but pulls in libgcc
soft-float helpers under the kernel's own `-msoft-float` Kbuild flags,
which won't resolve against a real kernel - fixed with a hand-written
`divl`-based division mirroring the kernel's own `do_div()`.

`CSTGHeapManager::Initialize()`/`Alloc()` layer a handle-based allocator on
top of this ioremap'd region: a sentinel-anchored active list plus a
99999-entry free list of 20-byte handles, with bump-down cursor
allocation.

### CSTGComPort - keybed UART hardware

`CSTGComPort` (`src/init/comport.cpp`) is the class
`CSTGKeybedInterface_Startup` (step 14, above) drives. Its methods are
genuine C++ (an earlier reconstruction pass had incorrectly declared them
as plain C-linkage functions, which could never link against the real
mangled symbols; the real signatures also use enum-typed parameters, not
plain `int`, which mangle differently).

`Initialize()` (2561 bytes, the largest single method in this class)
references a class `CW83627` - the Winbond W83627 Super I/O chip, real
standard PC hardware (a legacy UART reachable over ISA/LPC), confirmed via
literal `in al,dx`/`out dx,al` port-I/O instructions in the disassembly.
(`CW83627` itself has zero real methods in the binary - nothing further to
reconstruct there.) The traced algorithm: dual config-port (`0x2E` then
`0x4E` fallback) unlock and chip-ID validation, per-port LDN selection,
base-address-register combination, IRQ discovery, UART hardware reset, IRQ
request/CPU assignment, and the same baud-rate/LCR/FCR/MCR/IER bring-up
`SetBaudRate` uses. Two `comPortId`s (0 and 3) have confirmed additional
LDN-specific configuration not implemented here. The exact port I/O
addresses were not resolved by hand-tracing the disassembly - very likely
one of the classic legacy COM1-4 ports (`0x3F8`/`0x2F8`/`0x3E8`/`0x2E8`),
but not confirmed. *To validate*: a full Ghidra decompile of
`CSTGComPort::Initialize()` (rather than manual disassembly reading) would
likely resolve the exact address-selection logic faster.

### Additional reconstructed engine classes

Reconstructed while resolving `init_module`'s remaining unresolved
symbols, each fully disassembly-confirmed:

- **`CSTGFrontPanel`** - constructor and `Initialize()`.
- **`CMeteredDebugOutput`** - constructor.
- **`CSTGCPUInfo`** - constructor and `Update()`.
- **`CSTGSampleRateMonitor::Initialize()`**.
- **`stg_rtfifo_cleanup`** - the companion to `stg_rtfifo_init`.
- **`CLoadBalancer::Initialize()`/`~CLoadBalancer()`** - confirms the
  `CEmergencyStealer` base-class relationship (see above) at the
  destructor level too.
- **`CPowerOffTimer::Initialize()`**, **`CSTGDiskCostManager::
  Initialize()`**, **`CSTGCommonLFO::Initialize()`**, **`CSTGCommonStepSeq::
  Initialize()`**.
- **`CCostProfile::CCostProfile()`** (2009 bytes) - discovers a new base
  class, `CStartupFile` (real constructor call confirmed in the
  disassembly, argument `"CostProfile"` from rodata) - which retroactively
  explains why `CCostProfile::sInstance->_field4` was already confirmed
  real despite `CCostProfile`'s own constructor never writing it (it's a
  `CStartupFile`-owned field). Builds a real loop of 198 20-byte
  `CCostProfileEntry` objects, each with a permanently-untouched `+0x0`
  field (a real quirk, preserved). Total computed size (`0x12a0`)
  independently cross-checks against `setup_global_resources`'s own
  allocation size for this object.
- **`CSTGLFOBase::InitializeQuad()`/`CSTGStepSeqBase::InitializeQuad()`** -
  both decompose into clean loops writing three shared singletons' fixed
  sub-addresses into the passed-in quad block.
- **`CSTGWaveSeqManager::CSTGWaveSeqManager()`/`Initialize()`** - discovers
  `CSTGWaveSeqGenerator` (200 sub-objects, 0x120 bytes each, embedded
  directly in the manager); `Initialize()` threads all 200 generators into
  a real intrusive doubly-linked list via push-front insertion (a
  different order from `CSTGHeapManager`'s free-list build, which appends
  at the tail).
- **`CSTGMidiDispatcher::CSTGMidiDispatcher()`/`Initialize()`** - mostly a
  zeroing constructor plus a real heap-slot resolution in `Initialize()`
  that settles an earlier open question: `CSTGHeapManager::Alloc()`'s
  handle-number formula and `oa_heap.h`'s `oa_heap_region()` formula agree
  once accounting for slot 0 being the sentinel itself, not the first real
  handle entry. Discovers `CSTGMidiQueue` via its static-shaped
  `AllocReader(void*)` method.
- **`CSTGVectorManager::CSTGVectorManager()`** (3279 bytes) - discovers
  three "vector envelope generator" classes (`CSTGVectorEGXOnly`/
  `CSTGVectorEGXY`/`CSTGVectorEGCC`), with 432/432/34 confirmed instances
  in a non-type-contiguous interleaved layout, two confirmed real gaps
  preserved verbatim.
- **`CSTGVectorManager::Initialize()`** (2350 bytes) - five real phases:
  two 400-count loops (EGXOnly/EGXY) with real vtable slot-0 dispatch plus
  intrusive list push-front insertion, a confirmed marker write, an EGCC
  "batch1" loop (dispatch plus literal index, no list), and a paired
  EGXOnly/EGXY "batch1" loop sharing one index per pair with no list
  insertion. It never touches the constructor's own "batch2" object
  ranges - half of each type's population is built but never activated
  here, a real asymmetry preserved verbatim.
- **`CSTGVectorEGXOnly`/`EGXY`/`EGCC`'s own constructors** - all three
  small and branch-free. Discovers a new base class, `CSTGVectorEGBase`
  (declared opaque/deferred). Confirms a shared `+0x3c`/`+0x40`/`+0x48`
  list-node/owner field convention (present even on EGCC, which is never
  actually list-inserted) plus a `+0x44` self-pointer field. EGCC's own
  constructor sets four fields to the real global `STGVJSAssignInfo` and
  four 16-bit fields to a centered `0x8000` default.

## Known limitations

- **`CSTGComPort`'s exact UART port addresses are unconfirmed.** The
  Winbond W83627 Super I/O probing logic is fully traced, but the specific
  legacy COM-port addresses it programs were not resolved from the
  disassembly alone. *To validate*: a full Ghidra decompile of
  `CSTGComPort::Initialize()`.
- **The `OA.ko`/`KorgUsbAudioDriver.ko` load-order relationship is
  unresolved.** `loadoa/loadoa.c` loads `OA.ko` (fatal on failure) before
  `KorgUsbAudioDriver.ko` (explicitly non-fatal), yet `OA.ko`'s own
  `KorgUsbAudio*` symbol references are confirmed `GLOBAL` (not `WEAK`)
  direct-call relocations, which should make that order fail under
  standard kernel module loading. Static analysis alone does not resolve
  this. *To validate*: trace the real `insmod` sequence and symbol
  resolution against a live device or an accurate emulation environment.
- **`loadmod.ko`'s `/korg/Eva` decryption-bypass question is open.**
  Whether the on-disk `/korg/Eva` UI binary's decryption hooks can be
  exercised independently of a full engine boot is not determined.
  *To validate*: trace `loadmod.ko`'s own call path to that decryption
  step directly.
- **`OmapNKS4Module.ko`'s real `init_module` hard-requires a real USB
  front-panel board to `probe()`**, even though the module itself is fully
  reconstructed - it cannot succeed in an environment with no panel
  attached. This is a genuine hardware dependency, not a reconstruction
  gap.
- **`CSTGMessageProcessor`'s declared size (`0x1040`) is a lower bound,
  not an exact `sizeof()`** - unlike the other three partially
  reconstructed managers, whose exact real sizes are confirmed.
- **Roughly two-thirds of `CSTGGlobal`'s ~195 methods remain
  unimplemented**, most notably the 9-method `CSTGAudioInput`-delegating
  `UpdateAudioInputXXX` family and the 22-method `UpdateXXXCCAssign`
  table-scan-and-reassign family, both characterized but not written.
- **`CSTGVectorManager::Initialize()`'s "batch2" object ranges are never
  activated by any reconstructed code path** - whether some other,
  not-yet-reconstructed function activates them, or half the constructed
  population is genuinely unused at this stage of boot, is not determined.
- **Voice models and DSP (Stage 4) and the final breadth sweep (Stage 5)
  have not been started.** Large real-time audio-DSP method bodies
  encountered along the way (SSE sample-format conversion,
  `CSTGAudioManager::Initialize()`/`DoAudioManagerThreadProcessing()`,
  etc.) are deliberately stubbed with safe no-ops rather than
  reconstructed, consistent with this project's RTAI/audio-DSP
  virtualization scope (audio fidelity itself is out of scope; getting the
  module to load and its control-plane logic to run correctly is in
  scope).

## Building and testing

```bash
# Host-side: compiles each reconstructed unit and runs the full
# known-answer test suite. No kernel source tree needed.
make

# The real kernel module, via Kbuild.
make ko KDIR=/path/to/kronos-kernel-tree
```

`KDIR` must point at a configured Linux 2.6.32.11-korg source tree whose
module ABI (struct layouts, the `-mregparm=3` calling convention, RTAI
support) matches the Kronos's own kernel build - a generic, unpatched
2.6.32.11 tree produces a module with the wrong `vermagic`/struct layout
and will not load. `$(CXX)` is not defined by Kbuild by default and must
be set explicitly (`CXX ?= g++`) or the C++ compile step silently no-ops.
A from-scratch rebuild must emit zero `R_386_GOTPC`/GOT relocations and a
byte-exact `.gnu.linkonce.this_module` header; a default-PIE host
toolchain needs `-fno-pie` alongside `-fno-pic` to avoid reintroducing a
spurious `_GLOBAL_OFFSET_TABLE_` dependency that would otherwise break
`insmod`. Host-mode and Kbuild-mode object files must use distinct
suffixes (this Makefile uses a `.hostobj` suffix for the host build) -
sharing one `.o` path between the two modes lets Make's staleness check
silently reuse objects built with the wrong flags.

`module_main.c` (kernel-only glue: `init_module`/`cleanup_module`,
deferred blob load where relevant) is deliberately plain C rather than
C++, matching the convention used by the other reconstructed modules in
this project - this kernel's headers use inline-asm string-literal-suffix
syntax a modern `g++` cannot parse.

## Related documentation

- `docs/modules/OA.ko_auth.md` - the original auth-cluster analysis this
  reconstruction started from.
- `docs/interfaces/proc_oacmd.md` - the `/proc/.oacmd` command interface
  (note: its documented "PR" command does not exist in the real binary -
  see `ProcessOACmd` above).
- `loadoa/loadoa.c` - the real module-load orchestrator whose insmod
  sequence this README's load-order section is based on.
- `reconstructed/AT88VirtualChip/` - the software AT88/NV2AC chip emulator
  `SetupAtmelForAuthorizations`, `ParseAuths`, and `VerifyAuthorizationString`
  are designed to run unmodified against.
- `reconstructed/STGEnabler/`, `reconstructed/STGGmp/`,
  `reconstructed/RTAIVirtualDriver/`, `reconstructed/KorgUsbAudioVirtualDriver/`,
  `reconstructed/OmapNKS4Module/` - the companion modules `OA.ko` depends
  on at load time.
