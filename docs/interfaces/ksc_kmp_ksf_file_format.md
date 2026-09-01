# Korg `.KSC` / `.KMP` / `.KSF` sample family — on-disk format

Companion to [`pcg_file_format.md`](pcg_file_format.md) §7 ("Wave Sequences" — sample
references). `.PCG` files never embed sample audio, only a `(Bank UUID, numeric ID)`
reference. This document covers the other end of that reference for **user-created
sample collections**: the `.KSC` (collection manifest) / `.KMP` (multisample map) /
`.KSF` (single sample) file family.

**Status: hardware-verified end-to-end.** A synthetic sample built entirely from this
spec loads and plays correctly on a real Kronos (§5). All field-level claims are
confirmed by direct decompilation of `CKorgKsf::{ReadChunk,WriteFile}`,
`CKorgKmp::{ReadChunk,WriteFile}`, and `CKorgKsc::{ReadFile,WriteFile}` (Eva 3.2.2).
Each claim below cites the function address so it can be re-verified in Ghidra.
`kronosology/reconstructed/Eva/include/{korg_ksc.h,korg_kmp.h,korg_ksf.h,korg_riff.h}`
give the class shape (ctors, accessors, the generic `CKorgRiff` chunk-framing base).

---

## 1. `.KSC` — sample collection manifest (plain CRLF text, two write modes)

`CKorgKsc` derives directly from `CKorgFile`, not `CKorgRiff`, and is a plain
CRLF-delimited text script — confirmed by its vtable/typeinfo dump and by decompiling
`CKorgKsc::ReadFile` (`089cdcc0`) and `CKorgKsc::WriteFile` (`089ccdd0`).

### 1.1 Header (3 lines, first two strictly validated)

```
#KORG Script Version 1.0\r\n
#v2\r\n
#uuid:<uuid>\r\n
```

`ReadFile` compares line 1 against the literal `"#KORG Script Version 1.0"`
(`.rodata+08f1f39f`) and line 2 against a second literal at `08f1f3b8` (almost
certainly `"#v2"`). **If either check fails, `ReadFile` returns 1 (error) immediately —
a malformed header aborts the whole load.** `WriteFile` emits both lines
unconditionally as the first two lines of every `.KSC` it writes.

Line 3 (`#uuid:<uuid>`) is parsed in the main per-line loop (§1.2), not as a strict
header check. It's `mUUID` (`korg_ksc.h`, offset `0x210`, 64-byte buffer) — the
collection's own Bank UUID, the same `uuid:<uuid>` identity space used by EXs option
files (`pcg_file_format.md` §7).

### 1.2 Body — the `#>User.0.2.` companion line is required

The Eva-side `CKorgKsc::ReadFile` parses a valid `.KSC` on its own, but a file that
passes that parse can still show **completely blank** in the Disk-page browser
(`Sampling Mode Data` > `Multisamples` / `Un-referenced Samples`). The actual
gatekeeper for content visibility is OA.ko's separate `CKorgFileKSC` class (§1.5).

**Every plain-filename entry (`.KMP` or bare `.KSF`) needs a matching
`#>User.0.2.<same filename>` comment line.** Hardware-confirmed: a `.KSC` with a valid
header and one plain `.KMP` line but no `#>User.` line loads with no error but shows
zero content; adding the matching `#>User.0.2.` line makes the content appear.
Real, byte-for-byte confirmed structure:

```
#KORG Script Version 1.0\r\n
#v2\r\n
#uuid:<bank uuid>\r\n
<filename1>\r\n              one line per owned .KMP or bare .KSF, NO # prefix
<filename2>\r\n
...
#>User.0.2.<filename1>\r\n    one paired comment line per entry above, SAME ORDER,
#>User.0.2.<filename2>\r\n    as a SEPARATE BLOCK after all the plain lines
...
```

Plain lines referencing a bare `.KSF` (not wrapped in a `.KMP`) show up as
"Un-referenced Samples" in the browser, distinct from `.KMP`-wrapped entries under
"Multisamples". The `0.2` in `#>User.0.2.` is constant across every real file seen —
treat it as fixed literal text, not a computed value.

Eva-side parsing detail (`CKorgKsc::ReadFile`, the UI-facing reader — separate from the
OA.ko import path above): the main loop reads one line at a time; a line whose first 6
characters match `"#uuid:"` is specially parsed (a new `std::string` built from
character offset 10 onward — reason for `10` rather than the 6-char prefix length is
unresolved). Every other line (plain filenames and every other `#`-prefixed style) is
stored verbatim in an internal list and round-tripped on write, not decoded into
structured fields by this class. `WriteFile` walks two distinct list fields
(`this+0x254` and a region near `this+0x270`) on write, matching that round-trip
behavior.

Confirmed `.rodata` string literals:

| Address | Literal |
|---|---|
| `08f1f39f` | `"#KORG Script Version 1.0"` |
| `08f1f40a` | `"#>>uuid:"` |
| `08f1f419` | `"#>uuid:"` |
| `08f7819c` | `"#>>uuid:%s.MS%d.1.0.%s\n"` |
| `08f781b5` | `"#>>uuid:%s.DS%d.1.0.%s\n"` |
| `08f781ce` | `"#>uuid:%s.%d.%d.%s\n"` |

These `printf`-style formats are the generators for the four comment styles in §1.4.

### 1.3 Two write modes — `usersample.KSC` vs `usersample_UserBank.KSC`

`CKorgKsc` has a boolean field `mFieldA` (`korg_ksc.h`, offset `0x250`). `WriteFile`
branches on it, producing two completely different files:

- **`mFieldA == false` (normal mode)**: writes the header, then `"#uuid:" + mUUID`,
  then walks the two internal string lists (owned filenames + carried-over comment
  lines) writing each verbatim with `"%s\r\n"`. This is a real, stored **collection** —
  what a user builds and owns, and the only form §5's external-construction recipe
  needs to produce.
- **`mFieldA == true` (reference-export mode)**: writes the header, skips the filename
  lists entirely, and instead walks live in-memory Program/Combi sample-usage data
  directly, emitting `"#>>uuid:%s.MS%d.1.0.%s\r\n"` / `"#>>uuid:%s.DS%d.1.0.%s\r\n"`
  lines for every referenced multisample/drum-sample, followed by one closing
  `"#>uuid:%s.%d.%d.%s\r\n"` summary line per distinct bank UUID referenced (a "Save
  All" export can reference many banks; each gets its own block + summary line).

**`_UserBank.KSC` registers HD-1 bank references visible in Program mode's oscillator
Multisample picker — it is not a Sample Mode Disk-browser artifact.** Loading one does
not add anything to the Sample Mode Disk browser; it makes the bank's multisamples
selectable when assigning a Program oscillator's Multisample. Further confirmed:

- **Genuine disk-pointer/streaming behavior, not RAM residency**: the referenced
  `.KSF`/`.KMP` files must actually exist on the SSD at their referenced path. When they
  don't, the Kronos shows a **"SAMPLES NOT LOADED"** banner — the oscillator shows the
  sample's name/path in red as absent, but the Program itself isn't rejected. This is
  the point of the feature: a normal `.KSC` load fills RAM and evicts whatever a
  previous normal `.KSC` load put there, while a `_UserBank.KSC` reference is a
  lightweight pointer, letting far more sample content stay usable across Program/Combi
  work than RAM alone could hold.
- **A collision exists** when loading a second bank whose multisample names already
  exist ("same sample name already exists", refused). Whether this keys on name, bank
  UUID, both, or requires an already-loaded matching normal `.KSC` sibling is not
  determined — see Open Questions.

**Hand-authoring a `_UserBank.KSC` from scratch: hardware-confirmed working,
2026-08-24** (see Open Questions for the full probe history). `KscCollection`'s guard
against writing a `_UserBank.KSC`-suffixed target via its normal-mode `ToBytes`/`Save`
stays in place — that write mode's own format (plain filename list + `#uuid:`) is real
Kronos-generated output only and is a completely different code path from the new
dedicated writer below. `KronosScreenRemote` added a *separate* writer
(`ToUserBankBytes`/`SaveUserBank`) for this format specifically, cross-checked against
real fixtures (see below) before ever touching hardware, then confirmed live via two
probe rounds (single-zone, then a 32-zone multisample spanning the full keyboard) —
both load paths (`.KSC` and `_UserBank.KSC`) played every zone correctly. The write
guard's old code comment claimed hardware confirmation that hand-authoring fails
("There is no readable data"), but no commit/doc record of that test's actual content
was found, and it predates this section's own format confirmation — almost certainly a
normal-mode file saved under a `_UserBank`-suffixed name, not this format; the new
probes are the first real test of the actual question, and it works.

**Two more properties confirmed 2026-08-24**, cross-referencing
`Test2-kronos_UserBank.KSC` (pulled fresh via FTP from a real unit's
`SSD2/CLAUDETEST/`, alongside its normal-mode sibling `Test2-kronos.KSC`) and the three
already-local `SampleFixtures/SMPTEST/{LOOP,LOOPKEY,NOLOOP}_UserBank.KSC` fixtures:

- **`MS<n>`/`DS<n>` is a positional, per-type emission counter (0, 1, 2…), not the
  referenced multisample's own `Mno1` or sample's own `Sno1`.** Settled by
  `ANDRE_K2_73/samplesfeb28_25_UserBank.KSC`: its `DS0`–`DS27` run contiguously with no
  gaps, while nothing constrains the underlying samples' own numbering to be gap-free —
  if `n` were `Sno1`-derived, a real collection this size would almost certainly show a
  gap. (For a fresh, sequentially-numbered collection the two readings are
  indistinguishable — this fixture is what breaks the tie.)
- **A real `_UserBank.KSC` reflects whatever the sampling engine currently has resident
  in RAM at Save time, not just its own sibling `.KSC`'s file list.** `NOLOOP.KSC` and
  `LOOP.KSC` share the exact same own-`#uuid:` value despite being nominally different
  collections (Save As under a different name evidently doesn't always fork a fresh
  bank identity); both
  `_UserBank.KSC` siblings additionally carry an identical stray `DS0.OrchCrash` entry
  that appears in neither `.KSC`'s own plain-filename list at all, and
  `Test2-kronos_UserBank.KSC` carries extra/duplicate-looking `MS` entries traceable to
  unrelated, previously-loaded test collections. A tool with no live sampling-engine RAM
  state to walk (i.e. any external writer) cannot replicate this and — per the doc's own
  framing of the feature's point (disk-pointer references, not RAM residency) —
  arguably shouldn't try to: `KronosScreenRemote`'s writer only ever emits the
  collection's own on-disk entries.

**`_UserBank.KSC` format** (confirmed via `CKorgKsc::WriteFile` disassembly +
computational verification against a real 8095-line file spanning 5 distinct bank
UUIDs):

```
#KORG Script Version 1.0\r\n
#v2\r\n
(no #uuid: line for this file type)
#>>uuid:<bank>.MS<n>.1.0.<name>\r\n   one per multisample, n = 0-based index
#>>uuid:<bank>.DS<n>.1.0.<name>\r\n   one per drum sample, n = 0-based index
... (repeated per bank UUID referenced)
#>uuid:<bank>.<MS_count>.<DS_count>.<long name>\r\n   one summary line per bank,
    immediately after that bank's own entry block
```

`<MS_count>`/`<DS_count>` are exactly the multisample and drum-sample counts for that
bank's block.

`<long name>` depends on whether the summary line is for the `_UserBank.KSC` file's
**own bank** or an **external/dependency bank**:
- **Own bank**: bare filename, e.g. `#>uuid:d1d951ab-....5.2.KORGREALSAVE` — no path
  prefix.
- **External/dependency bank**: factory EXs banks use `"EXs<N> <description>"`
  (matching that EXs's own option-file long name, `pcg_file_format.md` §7); user banks
  use `"HDD:INTERNAL HD:<that bank's own .KSC base filename>"`.

### 1.4 Comment line formats

| Prefix | Example | Meaning |
|---|---|---|
| `#>User.<n>.<m>.<filename>` | `#>User.0.2.GAGA_000.KMP` | Required companion line for every plain-filename entry in a real `.KSC` (§1.2). `0.2` is constant across every real file seen; treat as fixed literal text. |
| `#>EXS<N>.<sampleId>.<field>.<name>` | `#>EXS17.MS7.1.0.ViolinA Vibrato-L` | Reference to a factory legacy EXs bank, labeled by its public EXs number. |
| `#>uuid:<uuid>.<MS_count>.<DS_count>.<long name>` (single `>`) | `#>uuid:2d94b0e4-....12.0.EXs147 Acousticsamples C7 Grand Close` | Per-bank summary line (§1.3) — the two numbers are that bank's multisample and drum-sample counts. |
| `#>>uuid:<uuid>.<sampleId>.<field>.<name>` (double `>>`) | `#>>uuid:e9432454-....DS1092.1.0.Cda-C2-Vel02` | Individual sample-level reference — built by `"#>>uuid:%s.MS%d.1.0.%s\n"` / `"#>>uuid:%s.DS%d.1.0.%s\n"`. |

### 1.5 The real gatekeeper is OA.ko's `CKorgFileKSC`, not Eva's `CKorgKsc`

Eva's `CKorgKsc` (§1.1-§1.4) is the UI-facing class used for browsing/writing files
from the Disk page — it is **not** what OA.ko actually imports. OA.ko has a separate,
parallel class family: `CKorgFileKSC`/`CKorgFileKMP`/`CKorgFileKSF` (confirmed distinct
symbols in `oa_export/symbols.csv`). The real import chain:
`CKorgFileKSC::LoadUserBankMetaData` → `AllocateBankArraysForKSC` (pre-scans the file,
sizing arrays) → `ImportToBank` (the per-entry import). This is the code path that
enforces §1.2's `#>User.0.2.` requirement — Eva's own `ReadFile` alone does not enforce
it.

**Not the same as**: `CSTGMultisampleBank::LoadBankMetaData` reads a different, unrelated
binary format — a compiled `<bank-folder>/BankMetaData.img` (magic `"OASYSMTD"`) +
`SamplesNN.img` files. This is **factory-EXs-only** (e.g.
`/korg/rw/PCM/Bank17/BankMetaData.img`); the real user `usersample/` folder has none of
these. Not relevant to user-bank work.

**Referenced-filename path resolution**
(`CKorgResourceFile::GetPathInSubdirectoryFromFileName`, `oa_export` address `00055540`):
given the `.KSC`'s own path and a referenced filename, builds `<.KSC's own path with
extension stripped>/<filename>` — the same "strip extension, keep the rest" convention
as `CKorgFile::GetFolder` (Eva side, `089c9930`). A `.KSC` at `.../FolderX.KSC`
referencing `KeymapX.KMP` in a plain line expects that file at
`.../FolderX/KeymapX.KMP`.

### 1.6 Bulk `.KSC` import can silently skip a sample's audio if its `SNO1` collides with another sample's — CONFIRMED, fix verified live

Hardware-observed (this session, live FTP against a real Kronos, repeated 3x):
loading a `.KSC` (Clear All) registers every `.KMP` entry correctly (all
multisamples selectable, correctly named — see §2.2), and correctly auto-loads
sample audio into RAM for a **mono** entry, but **fails to auto-load audio for a
stereo pair** (two `.KMP` entries, same base Name, Suffix `-L`/`-R`, adjacent
`MNO1`, per §2.2's convention) — even with the mono entry positioned *after* the
pair in file order. Loading either half's `.KMP` directly (bypassing `.KSC`), or
the raw `.KSF` directly (bypassing both), works correctly, so the `.KMP`/`.KSF`
files themselves are byte-correct; the fault is specific to `CKorgFileKSC`'s own
bulk-import path.

**Binary provenance note:** this class family does not exist in this repo's own
`reconstructed/OA/OA.ko` build artifact (that file is this project's *own*
partial rebuild from `reconstructed/OA/src`, which doesn't implement
`CKorgFileKSC` yet — hence `pending` in the manifest). All addresses below are
real, ground-truth `objdump`-verified addresses in the actual Kronos firmware
binary (`Decomp/OA.ko_Decomp/OA.ko`, md5 `955636c2b11a70a1dbecefaaa7bd4f80`,
identical to `ARCHIVE/Ignored/DecryptedImages/MOD_Extracted/OA.ko`). Ghidra's
own addressing for this binary is a flat `+0x10000` offset from the real file
address (e.g. Ghidra/manifest `000526e0` = real `objdump` address `000426e0`);
both are given below as `oa_export addr (real addr)`.

**Confirmed call graph:**

- `CKorgFileKSC::LoadUserBankMetaData` (`000530c0` / `000430c0`) — calls
  `AllocateBankArraysForKSC` to completion, then calls `ImportToBank`. **True
  two-pass design, not interleaved** — the preflight bound is fully fixed before
  any zone ever loads. (Confirmed from the decompiled pseudocode only, not yet
  independently ground-truthed via `objdump` — but the function is 91 bytes with
  no local state, low misattribution risk.)
- `CKorgFileKSC::AllocateBankArraysForKSC` (`000526e0` / `000426e0`) — preflight
  pass over every line. **Ground-truth `objdump` check found the decompiled
  pseudocode unreliable here**: the printed classification branch
  (`if ((char)local_364 == '\0')`) tests a stack byte at a *different* offset
  (`[esp+0x1a8]`) than the one a nearby literal-string copy loop writes
  (`[esp+0x7c]`) — Ghidra conflated two unrelated stack slots under one variable
  name. The real branch reads a flag byte that `ParseUserSampleLine` itself
  wrote into the parsed-line object (almost certainly a `.KMP`-vs-`.KSF`
  classification the parser already computed) — not a re-derived extension
  check, and *not* a cross-line name/suffix comparison as speculated in an
  earlier draft of this section. **The exact numeric value this function
  computes as the sample-slot bound is unverified** — the earlier draft assumed
  it equals `CKorgFileKMP::GetMaxSampleNumber`'s running max + 1; that is
  plausible from the pseudocode's call graph but has not been ground-truthed and
  is contradicted by the data point below.
- `CKorgFileKSF::GetSampleNumber` (`000548d0` / `000448d0`) — **ground-truth
  `objdump`-confirmed**, clean 0x3b-byte function: pushes the constant
  `0x534e4f31` ("SNO1") and calls `CKorgResourceFile::FindAndLoadChunk`, then
  byte-swaps and returns the chunk's raw value. **It reads the `.KSF`'s own
  `SNO1` chunk verbatim — nothing is parsed or derived from the filename.**
  `CKorgFileKMP::GetMultisampleNumber` is the same pattern one level up (tag
  `0x4d4e4f31`/"MNO1" instead), also ground-truth clean.
- `CKorgFileKSC::ImportToBank` (`00052c00`/`00042c00`, thiscall; identical-logic
  clone at `00051d00`/`00041d00`, regparm3) — real import pass, re-walks the
  same lines; for each `.KMP` line calls `CKorgFileKMP::ImportToBank`.
- `CKorgFileKMP::ImportToBank` (`00053f40` / `00043f40`) — **ground-truth
  `objdump`-confirmed**, 0xf3-byte function, cleanly attributable (regparm3:
  EAX=`this`, EDX=`CSTGMultisampleBank*` bank param, ECX=`ulong*` shared array
  param, bool param on stack). Stores the bank pointer and the bool onto
  `this`, calls `FindAndLoadChunk` for `this`'s own `MNO1` (multisample number),
  and checks it against a bound taken from **the bank object**, not the shared
  array — see below. It also caches `*sharedArray` (the running cursor) onto
  `this` as a starting offset, calls `LoadChunk`, then **writes back
  `*sharedArray = startingOffset + this->zoneCount`** — confirmed, real,
  monotonically-advancing per-line cursor threaded through the shared `ulong*`
  array across every `.KMP` line of the current `.KSC`.
- `CKorgFileKMP::LoadChunk` (`00053320` / `00043320`) — **ground-truth
  `objdump`-confirmed** for the `RLP1` (18 bytes/record, zone list) handler.
  Per zone: `slot = (cachedStartingOffset + zone_index) & 0xffff`, checked
  against **`bank->field_0xc`** (not the shared `ulong*` array — a correction
  to an earlier draft of this section), and gated on **`bank->field_0 !=
  0xffffffff`** (also a bank field, doubling as a `CSTGHeapManager` block
  handle used to locate the actual PCM storage region). Only if both hold does
  it resolve+load the zone's `.KSF` (`GetSampleNumber` + `ImportToBank`); the
  `NAME` handler also calls `CopyAndFixStereoNames` here. If the check fails,
  the zone is silently skipped — no error, no abort, indistinguishable from a
  successful zone to the caller.
- `CKorgResourceFile::CopyAndFixStereoNames` (`000553a0`) — **ruled out** as a
  collision/dedup mechanism. Read in full: pure display-name formatter (copies
  a name, trims trailing spaces, normalizes a trailing `L`/`R` to `-L`/`-R`).
  Runs only after a sample is already decided to load; never gates loading.

**The mechanism, as ground-truth confirmed:** `.KSC` bulk import maintains one
monotonically-advancing "sample slot" cursor in the shared `ulong*` array passed
down from `CKorgFileKSC::ImportToBank`. Each `.KMP` line claims a contiguous run
of slots — sized to its own `RLP1` zone count — starting wherever the previous
line's cursor left off, and every zone's actual load is gated on that slot
being `< bank->field_0xc` (a capacity value on the shared bank object) and the
bank's own heap-handle field not being the `0xffffffff` sentinel. This is a real
mechanism capable of silently dropping a load with no visible error while a
separate, unrelated array (the multisample-number-indexed one, self-consistently
sized since `AllocateBankArraysForKSC`'s `GetMultisampleNumber` max and
`LoadChunk`'s own `MNO1` check both read the *same* field) still succeeds —
which matches the *shape* of the hardware symptom (entry visible, audio absent).

**Unresolved, and actively contradicted by new hardware data — do not treat the
above as the full explanation yet.** A real fixture from this session had all
three real entries (stereo L: `MNO1=0`, zone `MS000000.KSF`, `SNO1=0`; stereo R:
`MNO1=1`, zone `MS001000.KSF`, `SNO1=0`; mono: `MNO1=2`, zone `MS002000.KSF`,
`SNO1=0`) share **identical** `SNO1=0`, so — if the bound really is derived from
`GetMaxSampleNumber`'s running max as previously assumed — `AllocateBankArraysForKSC`
would compute the *same* bound contribution (0) regardless of which lines are
stereo vs. mono, and simulating the confirmed cursor/bound logic with a bound of
1 (`max(0,0,0)+1`) predicts **the L half loads, the R half and the mono entry
both fail** (cursor 0/1/2 vs. bound 1) — which matches neither "both L and R
fail" (the original framing of this bug) nor the new data point that the mono
entry *reliably loads* even positioned last. So either (a) the bound is not
`GetMaxSampleNumber`-derived at all, (b) `AllocateBankArraysForKSC`'s real
computation differs from what the (partially-unreliable) pseudocode suggests in
some other way not yet ground-truthed, or (c) there is a separate, not-yet-found
gate specific to adjacent/paired `MNO1` values. **`AllocateBankArraysForKSC`'s
own bound computation has not been ground-truth verified via `objdump`** (only
a small piece of it, the mis-decompiled classification branch, has been) — that
is the single highest-value next target, along with `CSTGMultisampleBank::InitializeMetaData`
(where `bank->field_0` and `bank->field_0xc` actually get written).

**Root cause, hardware-confirmed 2026-08-24 (resolves the contradiction above
without needing `AllocateBankArraysForKSC`'s exact bound arithmetic):** the
fixture that produced the contradictory prediction had all three real entries
sharing **identical `SNO1=0`** — every sample this session's writer (the
`KronosScreenRemote` C# port) had ever produced, since its `KsfSample.Sno1`
field defaulted to `0` and nothing ever assigned it a real value. A live test
that changed nothing except giving each of the three samples a distinct `SNO1`
(`0`/`1`/`2` instead of `0`/`0`/`0`) — otherwise byte-identical to the
previously-failing fixture — loaded and played **all three** correctly,
including the stereo pair, on the first try. Since `GetSampleNumber` (`000448d0`,
ground-truth confirmed above) reads exactly this field, and `AllocateBankArraysForKSC`
feeds `GetMaxSampleNumber`'s per-`.KMP` result into the array-sizing pass that
produces `bank->field_0xc`, the simplest explanation consistent with both the
decompiled call graph and this result is that **duplicate `SNO1` values across
samples in one collection undercount the true number of distinct samples
`AllocateBankArraysForKSC` needs to size for**, silently starving `bank->field_0xc`
below what the real per-line zone cursor needs once distinct-but-identically-numbered
samples are actually walked — consistent with "some zones load, others don't" and
with a mono entry (whose own `SNO1` happens not to collide with anything earlier in
a smaller/differently-ordered file) surviving while a same-numbered pair doesn't.
This does not fully pin down `AllocateBankArraysForKSC`'s exact arithmetic (still a
valid next `objdump` target for anyone who wants the precise mechanism), but the
**practical, hardware-verified conclusion for any writer is unambiguous: assign
every sample in a collection a real, distinct `SNO1` — never leave multiple samples
at the same value (particularly never leave every sample at the shared default
`0`)**. `KronosScreenRemote`'s fix: `KscCollection.NextFreeSno1` (disk-scanned,
collection-unique) wired into every `.KSF`-creating call site — see its own repo's
Commit Notes.md entry 41 for the full before/after.

**Calling-convention note (superseded):** ground-truthing via `objdump` showed
`CKorgFileKMP::ImportToBank` and `CKorgFileKMP::LoadChunk` are both regparm3 and
decompile/attribute cleanly. `AllocateBankArraysForKSC` and the `00052c00`
`ImportToBank` overload are the two functions actually shown (by ground truth,
not just pseudocode inspection) to have at least one misattributed stack
variable; treat their exact branch structure as unverified until re-checked
against real disassembly the way the functions above were — relevant only to
anyone chasing the exact `bank->field_0xc` arithmetic, not to the practical
writer guidance above, which is independently hardware-confirmed.

---

## 2. `.KMP` — multisample map (binary, `CKorgRiff`-framed, big-endian)

Chunk framing per `korg_riff.h`: `[4-byte ASCII tag][4-byte big-endian length][payload]`.
`CKorgKmp::IsBigEndian()` returns `true` unconditionally.

**Write order** (`CKorgKmp::WriteFile`, `089ca550`): `MSP1` → `MNO1` → `NAME` (via
inherited `CKorgRiff::WriteFile`) → `RLP1` → `RLP3` → `RLP2`, then EOF. This is a
read-write round-trip match with `CKorgKmp::ReadChunk` (`089cabf0`), which dispatches
the same five tags with the record sizes below.

| Tag | Record size | Byte-swapped? | Notes |
|---|---|---|---|
| `MSP1` | 18 bytes, single blob (not a record array) | No — `fread(this+0x128, 0x12, 1, file)`, no `SwapFile` on read or write | 16-byte name + 2 trailing bytes. The trailing 2 bytes are the multisample's own **zone count**, little-endian `u16` — hardware-confirmed 2026-08-24 against 4 independent real fixture pairs (`ANDRE_K2_73/NEWMS000`/`001`: 8 zones each, tail `08 00`; `NEWMS002`/`003`: 9 zones, tail `09 00`; `GAGA_000`/`001`: 2 zones, tail `02 00`; `SMPTEST/*/CLAUD000`/`001`: 1 zone, tail `01 00`) and by a live write/re-save test: a `.KMP` uploaded with this field left `0` registered on a real Kronos as a zero-zone multisample — its zone's own `.KSF` sample still loaded fine as a standalone resource (confirming `RLP1` itself is read correctly regardless), but tapping the multisample in the UI triggered a **"Create New Sample"** prompt instead of selecting it, for every multisample so affected (including a mono one — this is not stereo-specific). Korg's own Eva always recomputes this field on save regardless of whether the zone count changed (confirmed: a re-saved file had it corrected from `0` to the real count with nothing else different) — a writer should always derive it from the real zone count, never round-trip a stale/loaded value. |
| `MNO1` | 4 bytes, single `u32` | Yes | The multisample's own **numeric ID**, matching the number embedded in its own `.KSF` zone filenames (§4). Confirmed by an Eva re-save that renumbered a multisample `044`→`4` (display name `24K_I044`→`24K_I004`) and changed `MNO1` `0x2C`→`0x04` in lockstep while every referenced `.KSF` filename was renumbered to match. Not safety-critical at load time (a mismatched `MNO1` still loads and plays), but Eva re-derives and enforces it as self-consistent on every save — a writer should set it to match the number used in its own filenames rather than defaulting to `0`. |
| `NAME` | 24 bytes | n/a (text) | Written via the inherited `CKorgRiff::WriteFile`. |
| `RLP1` | 18 bytes/record, `count = chunk_len / 0x12` | Field 0 & field 1 not separately swapped (single bytes, see §2.1) | Real record width is 18 bytes total, 12-byte filename field (`korg_kmp.h`'s guess of a 13-byte name field is wrong). |
| `RLP3` | 6 bytes/record, `count = chunk_len / 6` | No swap on read; **byte at record offset+5 is forced to `0` on every write** | `WriteFile`'s RLP3 loop saves the real in-memory byte at offset+5, zeroes it, writes the record, then restores the in-memory value — a deliberate write-time masking. |
| `RLP2` | 4 bytes/record, `count = chunk_len >> 2` | No swap seen | Record contents/semantics unconfirmed — no reconstructed method reads this list back. |

**Practical implication for a writer**: `RLP3`'s offset+5 byte is unusable for
round-tripping any real per-zone data through save/load on real hardware — leave it
`0`, matching Korg's own writer.

### 2.1 `RLP1` record layout (18 bytes/record)

```
offset 0    Original Key (byte) — root/tracking key
offset 1    Top Key (byte) — top of this zone's trigger key range
offset 2-5  unknown, constant 00 00 40 00 in every record seen (confirmed NOT
            original-key or loop data)
offset 6-17 (12 bytes) zone filename — a .KSF filename, or literal "SKIPPEDSAMPLE"
            truncated to 12 chars ("SKIPPEDSAMPL") marking a deliberately-unsampled
            key position
```

Hardware-confirmed (paired `.KMP` fixtures differing only by Orig.Key=C4/Top Key=C5 set
via the Kronos's own Sample Edit UI): byte diff at record offset 0-1 was exactly
`0x3c,0x48` = `60,72` = C4, C5, matching the UI values in that order. There is no
separate "bottom key" field; a zone's trigger range runs from (previous zone's Top Key
+ 1) to (this zone's own Top Key) — consistent with every real multisample examined
(ascending, contiguous Top Key values across zones), though not independently
hardware-tested as its own claim.

`ReadChunk` reads each record with one `fread(ptr, 0x12, 1, file)` — no field-level
`SwapFile` calls inside the RLP1 branch (every field here is either a single byte or
text).

### 2.2 Stereo instruments are two full multisamples, not paired zones

**A stereo Kronos sample instrument is TWO separate `.KMP` files, not two zones inside
one `.KMP`.** There is no channel/pan field anywhere in `RLP1` — a zone maps exactly
one `.KSF` to one key range within a single keymap. Real stereo content uses a matched
*pair* of complete multisamples: same display `Name` (`MSP1`/`NAME`), one with
`Suffix = "-L"`, the other `Suffix = "-R"`, sequential `MNO1` values (`N` and `N+1`),
and the same zone layout in both (same key ranges, same zone order, each zone's own
`.KSF` sharing the base sample name with the matching `-L`/`-R` suffix baked into its
own `SMP1`/`NAME` text, per `MakeNameLeft`/`MakeNameRight`, §5).

`SMF1` (§3.2) is not a reliable stereo-pair link field — every real same-Name `-L`/`-R`
pair examined links purely through `Name`+`Suffix`+adjacent `MNO1`, never through
`SMF1`. Leave `SMF1` unset for new/external content (§3.2, §5).

**Practical implication for a writer**: to build a new stereo instrument, create two
`.KMP` files with identical `Name`, `Suffix = "-L"` / `"-R"`, `MNO1` and `MNO1+1`, and
add matching zones to both — never a single `.KMP` with two same-range zones.

**Correction, 2026-08-24 — the `.KMP` filename is NOT free-form for a stereo pair,
contradicting this section's earlier claim.** That claim was based on Eva's own
`ReadFile`/`WriteFile` (§1.1–§1.5) having no filename-format check, which is true but
was the wrong gate to check — a live test on real hardware showed a stereo pair saved
as `<Name>-L.KMP`/`<Name>-R.KMP` (baking the channel suffix into the filename, matching
the *internal* `Suffix` convention) registers its multisample entries correctly but its
**audio never actually loads** through a `.KSC` load, while byte-identical content
saved as `NEWMS000.KMP`/`NEWMS001.KMP` (no `-L`/`-R` in the filename at all) loads and
plays correctly — the only difference between the two tests was this filename. Every
real Kronos-authored `.KMP` filename examined (`NEWMS000`/`001`/`002`/`003`,
`GAGA_000`/`001`, `CLAUD000`/`001`) follows one pattern: the first 5 characters of the
multisample's own `Name` (uppercased, non-alphanumeric → `_`, underscore-padded if
short) + the 3-digit `MNO1` + `.KMP` — e.g. `Name = "NewMS______________000"` →
`NEWMS000.KMP`/`NEWMS001.KMP`; `Name = "GAGA LEAD"` → `GAGA_000.KMP`/`GAGA_001.KMP`. The
`-L`/`-R` channel marker belongs ONLY in the internal `Suffix` field (`MSP1`/`NAME`),
never in the filename — a writer must derive the filename from `Name` + `MNO1` this way,
not from `Name` + `Suffix`. (Whether this same requirement applies to a plain mono
multisample's filename, or is specific to a `.KSC`-driven stereo-pair load, is untested
— every mono fixture examined so far already happens to follow the same pattern by
the user's own naming choice.)

**Known ambiguity**: `Name`+opposite-`Suffix`+adjacent-`MNO1` is the strongest available
pairing signal but is not unique — a lone `-L` multisample with no `-R` counterpart is
valid and just plays as mono, and it can sit `MNO1`-adjacent to an unrelated pair,
making automatic pairing occasionally pick the wrong sibling in dense real-world
collections. There is no explicit pairing field in the format to disambiguate against.
A tool matching on this heuristic should document that limitation rather than build
elaborate disambiguation the format can't actually support.

---

## 3. `.KSF` — single sample (binary, `CKorgRiff`-framed, big-endian, holds real PCM)

Same `CKorgRiff` framing as `.KMP`. **Confirmed chunk order**: `SMP1` → `SNO1` → `NAME`
(inherited) → `SMF1` (present when populated, §3.2) → `SMD1`.

`CKorgKsf::ReadChunk` (`089cffe0`) and `CKorgKsf::WriteFile` (`089cf610`) were fully
decompiled; every chunk and field below is a direct read of that code.

| Tag | Size | Meaning |
|---|---|---|
| `SMP1` | 32 bytes | 16-byte display name (unswapped, text) + 4 swapped `u32` fields — see §3.1 field table below. |
| `SNO1` | 4 bytes | One swapped `u32`, stored verbatim from the constructor's 3rd argument (`korg_ksf.h`'s `field16a`) — an arbitrary caller-supplied integer, not algorithmically derived. **Not stable across saves**: a real sample's `SNO1` changed `268`→`0` across an Eva re-save that also renumbered its owning multisample — consistent with a session-scoped number Eva reassigns freely. **Correction, 2026-08-24 — `0` is NOT a safe default for a writer, contradicting this row's earlier claim.** `CKorgFileKSF::GetSampleNumber` (`000448d0`, OA.ko, ground-truthed via `objdump`) reads this field verbatim to compute the bulk-`.KSC`-import bound described in §1.6 — hardware-confirmed live: every `.KSF` a writer emits with `SNO1 = 0` is indistinguishable from every other `SNO1 = 0` sample in the same collection, and a real Kronos's `.KSC` bulk import silently drops all but one of a group of identically-numbered samples' audio (the multisample entry itself still registers and shows as selectable — only the underlying audio never lands in RAM). A writer must assign each new sample a real value, unique within the collection (real Kronos-generated content uses small ascending values, e.g. `0,1,2…` for a freshly-built collection, `26,27` for two samples added to an existing library) — never leave every sample at the same constant. |
| `NAME` | 24 bytes | Inherited `CKorgRiff` base field — same as every other name field project-wide. |
| `SMF1` | 12 bytes, optional | See §3.2. Leave unset for external writers. |
| `SMD1` | Whole rest of file | 12-byte sub-header + raw PCM. See §3.1. |

### 3.1 `SMP1` fields (offsets 16-31) and `SMD1` sub-header

**`SMP1`'s 4 trailing `u32` fields** (file offsets 16-31 for a standalone `.KSF`):

- **u32[0] (16-19): Sample Start** — playback start-trim, in samples.
- **u32[1] (20-23): Loop Start** — in samples.
- **u32[2] (24-27): unmapped duplicate** — mirrors u32[1] (Loop Start) in every real
  nonzero instance observed, with no exceptions. Meaning unconfirmed (candidate: a
  legacy/unused second array slot, or a sustain-point field Eva always defaults to Loop
  Start). Writer behavior: always mirror u32[1] exactly.
- **u32[3] (28-31): Loop End** when loop is enabled; equals `frame_count-1` (end of
  sample) in every intact one-shot file when loop is off. **This is a stored value, not
  re-derived at read or write time**: a header-only-corrupted `.KSF` with
  `frame_count==0` still carried its pre-corruption LoopEnd rather than 0 — a writer
  must pass this field through exactly as read/set, never recompute it from the current
  frame count. Callers that resize PCM (crop, WAV re-import) own updating
  LoopStart/LoopEnd/SampleStart themselves.

`CKorgKsf::ReadChunk`'s `SMD1` branch does one `fread(this+0x148, 0xc, 1, file)` — the
sub-header is **12 bytes**, followed immediately by PCM:

```c
// CKorgKsf::ReadChunk, SMD1 branch (089cffe0), paraphrased:
__size = param_2 - 0xc;                       // PCM byte count = declared_chunk_len - 12
fread(this + 0x148, 0xc, 1, param_3);         // the entire 12-byte sub-header, one read
SwapFile(this, (u32*)(this + 0x148));         // swap sub-header offset 0-3
SwapFile(this, (u32*)(this + 0x150));         // swap sub-header offset 8-11
lVar4 = ftell(param_3);
this->pcmFileOffset = lVar4;                  // this+0x158, in-memory bookkeeping only
if (!this->loadNow /* this+0x154 */) {        // lazy-load path: remember where the PCM is
    this->mDataSize = __size;                 // this+0x160
    fseek(param_3, __size, SEEK_CUR);         // skip the PCM, don't read it into memory yet
    return;
}
this->mDataSize = __size;
this->mBuffer = malloc(__size);               // this+0x15c
fread(this->mBuffer, __size, 1, param_3);
if (this->bitDepth /* this+0x14f */ == 0x10) {    // 16-bit: byte-swap every PCM sample word
    // swap loop over frameCount (this+0x150) 16-bit words, unrolled by 8
}
```

**12-byte sub-header, field by field:**

| Offset | Field | Swapped? |
|---|---|---|
| 0–3 | **Sample rate** (BE u32) — `44,100` in every real file seen | Yes |
| 4 | **Flags** — full bit map: `0x80` one-shot/loop-off (`0`=looped, `1`=one-shot); `0x40` Reverse (playback-direction only, does not reverse PCM in place); `0x01` +12dB gain boost (default on). Bits 1-5 unobserved/always `0`. These flags are a property of the shared/resident sample, not per-zone — editing one multisample's assigned sample changes the byte in that resident `.KSF` and in every stub `.KSF` referencing it (via `SMF1`) across other multisamples in the collection. | No (single byte) |
| 5 | **Loop Tune** — signed byte, raw tune value (`+2`→`0x02`, `+99`→`0x63`). The front-panel UI clamps this to -99..+99 even though the byte has headroom to -128..127 — a writer should reproduce that clamp. | No |
| 6 | **Channel count** — always `0x01`, unconditionally, regardless of source material (confirmed by splitting genuine 2-channel and 24-bit sources across stereo/mono multisamples: every resulting resident `.KSF`, including true-stereo L/R content, still shows `0x01`). There is no interleaved-channel representation in this format; stereo is always two `.KMP`s (§2.2). | No |
| 7 | **Bit depth** — `0x10` (16) in every real file. **The Kronos downconverts all imported audio to 16-bit on import, unconditionally** — recording bit depth affects capture quality only, never on-disk storage width (confirmed: a 24-bit source's resident `.KSF` still reads `0x10` here and matches a plain 16-bit file size exactly, not ~1.5x). | No |
| 8–11 | **PCM frame count** (BE u32) — drives the swap-loop's iteration bound. `(SMD1 payload length − 12) / 2` equals this field exactly. | Yes |

PCM data begins at sub-header-relative offset **12**, runs for
`declared_chunk_len − 12` bytes, and is **big-endian 16-bit signed** (the only bit
depth ever seen).

**`.KMP` `RLP1`'s `unknown4` bytes (offset 2-5) are confirmed NOT loop points or
original key** — constant `0x00004000` across every zone examined. Original key lives
in `.KMP` `RLP1` offsets 0-1 (§2.1), not here.

**Decompile discrepancy, flagged**: `WriteFile`'s closing block (gated on
`this[0x16e] != 0`) writes chunk tag `0x534d5031` ("SMP1") — but `ReadChunk` expects
`0x534d4631` ("SMF1") for this position, and real files use `"SMF1"` (matching
`ReadChunk`, not this decompiled `WriteFile`). Either the exact compiled build this
Kronos runs differs from this project's disassembly, or this chunk is written by a
different code path than the one decompiled — worth re-checking
`CKorgKsf::WriteFile` (`089cf610`) against the exact build in use if this matters for a
writer.

### 3.2 The `SMF1` chunk

`ReadChunk` recognizes tag `0x534d4631` = `"SMF1"` (12 bytes, read into `this+0x16e`,
no swap); real files do carry this tag, populated. A real Eva re-save example showed:

```
SMF1 tag, length=0x0c (12), payload = "MS002000.KSF"   (ASCII text, no swap)
```

The 12-byte content is a `.KSF` filename string, but referencing a **different
multisample number** than the file's own — i.e. this field appears to be a
cross-reference to another, related `.KSF` file. Plausible roles, none confirmed: a
stereo-pair link, a "next in round-robin" pointer, or an artifact of Eva's own
cross-referencing when merging content into a session that already had other
multisamples present. **For any external writer: leave this field unset/zero** — a
hardware test with it unset loaded and played correctly.

Chunk position: `SMF1` sits between `NAME` and `SMD1` (§3, chunk order), not strictly
"trailing" everything. In one real fixture set, four header-only-corrupted `.KSF`
files (each zone index 1 of its own multisample) all carried an `SMF1` payload naming
zone index 0 of that same multisample — consistent with "points at another zone in the
same multisample," though not proven as *the* meaning.

### 3.3 Caveat: Eva can write empty PCM data if a sample was never fully loaded

A re-saved `.KSF` was found with its audio payload not preserved: the resaved file was
124 bytes (vs. the original's 2,651,656), with `SMD1`'s frame-count field at `0` — zero
PCM samples — despite the multisample having loaded and played correctly moments
earlier. Every other field (sample rate, flags, bit depth) round-tripped correctly;
only the audio data was dropped.

This is consistent with the lazy-load path in §3.1's `ReadChunk` walkthrough: a sample
can be registered and playable via the live sampling engine's own in-RAM state without
its full PCM buffer ever having been read back into *this particular* `CKorgKsf`
object's memory. Eva's "Save" then has nothing to write for that sample beyond the
already-read header fields, and writes a placeholder instead (`WriteFile`'s
`WriteEmptyFile` fallback when the buffer pointer is `NULL`).

**Practical implication**: don't assume "it played correctly" implies "a subsequent
Save will preserve it" — for archival/backup purposes, verify the *resaved* audio data
specifically, not just that playback worked once.

---

## 4. Filename convention

`CKorgKsf::MakeSampleFileName(a, b, c, dest, maxLen)`: `sprintf(dest, "MS%03u%03u", a,
c+1)` + `.KSF` extension. Real files use the pattern
**`MS<multisample-number:03d><zone-index:03d>.KSF`** — e.g. `MS044000.KSF` /
`MS044002.KSF` are zones 000/002 of multisample 044 (zone 001 in that example was a
skipped key position, per the `.KMP`'s `RLP1` record 1). The naming is unambiguous
either way the ctor's `c+1` offset is read.

---

## 5. How to construct samples externally and have them imported

Putting §1–4 together, a tool that wants to add new sample content to a Kronos from
outside needs to produce, for each new multisample:

1. **One `.KSF` per zone** (per key/velocity split, and per stereo channel if
   applicable — `-L`/`-R` are separate files). `.KSF` starts directly with its first
   chunk (no `KORG`-style file header). Write, in order:
   - `SMP1` (32 bytes): 16-byte name (space-padded, with the type suffix — `-L`/`-R`/
     none for stereo-left/right/mono — baked into the text, matching
     `MakeNameLeft`/`MakeNameRight`/`MakeName`), then Sample Start / Loop Start /
     duplicate-of-Loop-Start / Loop End as 4 BE `u32`s (§3.1). **Correction,
     2026-08-24 — `Loop End = 0` is NOT safe, contradicting this row's earlier claim
     that 0 for all four fields is fine with loop off.** Sample Start / Loop Start /
     the duplicate slot are still fine at `0`. Found via a direct byte-diff against a
     real Kronos's own re-save of a broken probe file that was showing a garbage Loop
     Start readout (`2147483640`) with Loop/Reverse both off: the *only* byte that
     differed anywhere in the file was Loop End's own last byte. Loop End should be set
     to `frame_count − 1`, matching §3.1's own already-documented real-hardware
     one-shot convention, exactly as if loop were actually being used for playback
     bookkeeping even when the one-shot flag disables it. (This doesn't contradict this
     section's own "hardware-verified end-to-end" synthetic-sample test above — that
     test confirmed a normal `.KSC` load/play round-trip; this `Loop End = 0` failure
     mode was only found via a different path, loading through a `_UserBank.KSC`
     reference and viewing the sample's own fields in the UI, not caught by "does it
     play a note" alone.)
   - `SNO1` (4 bytes): **must be unique across every sample in the collection**
     (§1.6, hardware-confirmed 2026-08-24 — this superseded an earlier, wrong
     claim that any value including a shared `0` was safe; OA.ko's real `.KSC`
     bulk-import path reads this field directly and silently drops a sample's
     audio load when its value collides with another sample's in the same
     collection, even though the multisample entry itself still registers). A
     simple incrementing counter, scoped to the whole collection and never reset
     per-multisample, is sufficient — see `KscCollection.NextFreeSno1` in
     `KronosScreenRemote`'s own port for a disk-scanned implementation.
   - `NAME` (24 bytes, via the generic `CKorgRiff` mechanism): the same display name,
     space-padded to 24 bytes.
   - (leave `SMF1` unwritten — §3.2)
   - `SMD1`: 12-byte sub-header — `sample_rate` (BE u32, `44100`), `flags=0x81` (BE
     byte, sets the one-shot bit), `loop_tune=0x00`, `channels=1`, `bits=0x10` (16),
     then `frame_count` (BE u32) — followed immediately by that many big-endian 16-bit
     signed PCM samples. Declared chunk length = `12 + (frame_count × 2)`.
2. **One `.KMP`** referencing those `.KSF` files by name, in a folder with the same
   base name as the `.KMP` file (matching `MakeFolder()`'s convention — e.g.
   `MYKIT.KMP` alongside a `MYKIT/` folder holding its `.KSF` zones). Write, in order:
   `MSP1` (18 bytes, short name + suffix, unswapped, trailing 2 bytes = zone count as
   LE `u16` — **required for the multisample to actually be selectable**; leaving it 0
   registers a zero-zone multisample that shows "Create New Sample" when tapped, even
   though its zones' `.KSF` files load fine on their own) → `MNO1` (4 bytes, BE u32 — the
   multisample's own numeric ID, matching the number used in its `.KSF` zone
   filenames, e.g. `MNO1=1` for zones named `MS001NNN.KSF`) → `NAME` (24 bytes) →
   `RLP1` (one 18-byte record per zone: Original Key, Top Key, `00 00 40 00`, then the
   12-byte `.KSF` filename) → `RLP3` (one 6-byte record per zone, write `0` at
   offset+5 always) → `RLP2` (one 4-byte record per zone, safe default `0`).
3. **One `.KSC`** (normal, `mFieldA == false` mode) with:
   - the 3-line header (`#KORG Script Version 1.0`, `#v2`, `#uuid:<a fresh UUID>` —
     generated per Korg's documented UUID scheme in `pcg_file_format.md` §7, clearing
     bit 0 of byte 15 for the mono/"bank" form),
   - one plain line per owned `.KMP` filename (or bare `.KSF` filename, for an
     un-keymapped sample),
   - then, as a separate block after all the plain lines, one
     `#>User.0.2.<same filename>` line per entry above, in the same order — required
     (§1.2): omitting it produces a file that loads with no error but shows zero
     content in the sample browser.
   - Do not attempt to write a `_UserBank`-suffixed sibling — a normal `.KSC` doesn't
     need one, and `KscCollection`'s guard currently blocks writing one anyway (§1.3).
     Whether an externally hand-authored `_UserBank.KSC` works is untested — see Open
     Questions.
4. Place the `.KSC` at `.../FolderX.KSC`, with its referenced `.KMP` file(s) inside a
   `.../FolderX/` folder (same base name as the `.KSC`, extension stripped — §1.5), and
   each `.KMP`'s own `.KSF` zones inside a same-named subfolder of that (matching
   `MakeFolder()`'s convention, e.g. `.../FolderX/KeymapX.KMP` +
   `.../FolderX/KeymapX/*.KSF`). Upload via FTP under one of the Kronos's own SSD
   drives (e.g. `SSD1/`/`SSD2/` at the FTP root — real user collections live directly
   under a top-level folder there) so the instrument's own sample-bank browser can find
   and load it.
5. **To reference this new content from a `.pcg`** (a Program's oscillator zone, a Drum
   Kit zone, or a Wave Sequence step), write the `(Bank UUID, numeric ID)` pair from
   step 3's fresh UUID into the relevant field, per `pcg_file_format.md` §3.3/§6/§7,
   and recompute the containing PCG bank chunk's checksum (`pcg_file_format.md` §12)
   before writing the `.pcg` back. This step is not yet independently hardware-tested
   (unlike steps 1-4) — see Open Questions.

**Hardware-verified end-to-end**: a synthetic 1-second 440Hz mono tone built entirely
from this recipe (`.KSF` + `.KMP` + `.KSC`, zero real Korg-authored bytes) was uploaded
to a live Kronos and confirmed to load and play correctly, both via the `.KSC`
container and via directly loading the synthetic `.KMP` file on its own.

**Known issue in the in-repo Python POC** (`Z:\Tools\sample_editor\kronos_ksc_format.py`
/ `kronos_sample_editor.py`, out of scope for the C# sample-editor tool but relevant to
anyone still using it):
1. Its `KmpZone` class names `RLP1` bytes 0/1 `key_low`/`key_high` and exposes them as a
   "key range" edit control. That's wrong — byte 0 is Original Key, byte 1 is Top Key,
   not a low/high range pair (§2.1) — editing "key range" in that tool actually edits
   Orig Key + Top Key.
2. **Data loss**: `encode_name_field`'s 16-byte short-field encoding and the 24-byte
   `NAME`-field encoding are both derived from the same decoded base+suffix (in turn
   decoded from the 16-byte short field on read), but those two fields are not simple
   re-truncations of each other — a real name can be up to 19 characters and only fits
   in the 24-byte `NAME` field, while the 16-byte field truncates to 14 base characters.
   The Python POC decodes from the 16-byte field and re-derives the 24-byte field from
   that truncated result, **silently dropping characters on every save of any real name
   longer than 14 base characters**. The C# port fixes this by decoding Name/Suffix
   from the 24-byte `NAME` chunk and independently re-truncating for the 16-byte field
   on write only.

---

## 6. What this means for the future librarian

Combined with `pcg_file_format.md` §7's `(Bank UUID, numeric ID)` scheme for
`.PCG`-embedded sample references, the full external chain for a user-created sample
is traceable end-to-end, both directions:

```
Program/DrumKit/WaveSequence zone (in a .pcg)
  --(Bank UUID, ID)-->  a .KSC file's own "#uuid:" identity
    --(plain filename lines)-->  .KMP files in that KSC's same-named folder
      --(RLP1 records)-->  .KSF files in a per-multisample subfolder
        --(SMD1 chunk)-->  raw 16-bit BE PCM audio
```

Separately, a real (non-`_UserBank`) `.KSC`'s `#>`-prefixed comment lines
self-document every external bank (User RAM, legacy EXs, or 3rd-party UUID bank) its
content was originally sampled/built from — real parsed data (§1.2), a ready-made
dependency list a future librarian could consume directly.

### 6.1 "Delete All Multisamples and All Samples" does NOT empty a save

Hardware-confirmed: starting from a collection with 3 multisamples (a stereo pair +
one mono) and 2 unreferenced samples, running Sample Mode's "Delete All Multisamples
and All Samples" and then Save produced a `.KSC` that still listed the stereo pair and
both unreferenced samples — only the mono multisample was actually gone. Same bank UUID
before and after.

**Practical implication**: "Delete All" is not equivalent to a genuine empty/reset
state — some content survives it. A tool that wants to reason about "what's actually on
the Kronos" from a fresh save should not assume "Delete All" produces an empty
baseline. Whether a genuine "Clear Sample Memory" action produces a true empty baseline
is untested.

---

## Open questions

- ~~**Can a tool hand-author a working `_UserBank.KSC`?**~~ **YES — hardware-confirmed
  2026-08-24.** `KronosScreenRemote.ToUserBankBytes()` (new, §1.3) builds one matching
  this doc's format exactly via the app's real production write path (fresh bank UUID +
  a fresh, distinctive multisample name to avoid the "same sample name already exists"
  collision). Two rounds of real-hardware probes, both uploaded to
  `SSD2/CLAUDETEST/UB1/`:
  1. `UBPROBE1` (single mono zone) — first attempt had no audio, root-caused via a
     direct byte-diff against the user's own hand-corrected re-save to two bugs in the
     *probe tool*, not the writer/format itself: Original Key left at C-1 instead of a
     sane root, and `LoopEnd` left at `0` on a one-shot sample (see the corrected §5
     entry below — this is the actual, generally-applicable finding).
  2. `UPROBE2` (32 zones, 4 keys each, spanning the full MIDI 0-127 keyboard, each
     zone's Original Key its own lowest key) — user-confirmed: **"Both loaded fine,
     all zones sound correct across the keyboard,"** for both `UPROBE2.KSC` (normal
     mode) and `UPROBE2_UserBank.KSC` (streaming/reference mode) side by side. This
     also exercises multi-zone assignment through both load paths, not just a single
     zone registering.

  `KscCollection`'s normal-mode `ToBytes()`/`Save()` guard against writing a
  `_UserBank.KSC`-suffixed target stays as-is — it's about the *other*, wrong-format
  write mode, unaffected by this finding. Two narrower sub-questions remain open
  (neither blocks treating hand-authoring as working, since every probe so far
  happened to keep both siblings present and used fresh identities throughout):
  (a) does the "same sample name already exists" collision check key on multisample
  name, bank UUID, or both; (b) does the referenced bank UUID need to resolve to an
  actual sibling `.KSC` on disk with a matching `#uuid:` line, or is `_UserBank.KSC`
  self-sufficient given the referenced `.KMP`/`.KSF` files exist at the expected path.
- **Chain a `.pcg`'s `(Bank UUID, ID)` reference through to real hardware-loaded
  content** — the sample side (this doc) and the `.pcg` side (`pcg_file_format.md`
  §3.3/§6/§7) are each independently confirmed but never tested together end-to-end.
- The exact `#uuid:`-line substring-extraction offset in `CKorgKsc::ReadFile` (§1.2 —
  currently "starts at character 10," reason unresolved) and which of the two internal
  string lists plain filenames vs. carried-over comments land in. Low priority — §1.5
  established OA.ko's `CKorgFileKSC` (not this Eva-side class) is the real import
  authority.
- `RLP2` record contents (`.KMP`) — confirmed present but not read by any reconstructed
  code; semantics unknown.
- The `#>User.<n>.<m>.` line's two numbers (`0.2` in every real file seen) and the
  `_UserBank.KSC` summary line's first number for a "usersample"-scale bank (didn't
  match a simple per-file count in one large real export) — neither blocking.
- `SMF1`'s cross-reference meaning (§3.2) — payload is confirmed to be another `.KSF`
  filename, but whether it's a stereo-pair link, round-robin pointer, or a
  session-merge artifact is unresolved. Leave unset for external writers regardless.
- **§1.6, practically RESOLVED, exact `AllocateBankArraysForKSC` arithmetic still
  open**: bulk `.KSC` import silently drops a sample's audio when its `SNO1` value
  collides with another sample's in the same collection (entries still register
  correctly either way). Hardware-confirmed live: giving each sample a distinct
  `SNO1` fixes it, no code-level workaround needed beyond that. The real,
  ground-truth (`objdump`)-confirmed monotonically-advancing sample-slot cursor +
  bound check in `CKorgFileKMP::ImportToBank`/`LoadChunk` is almost certainly the
  mechanism, but the *exact* arithmetic `AllocateBankArraysForKSC` uses to turn
  colliding `SNO1`s into an undersized bound — the only remaining unknown — hasn't
  been ground-truthed. `AllocateBankArraysForKSC`'s own bound computation and
  `CSTGMultisampleBank::InitializeMetaData` are the next targets for anyone who
  wants the precise mechanism rather than the (already sufficient) practical fix.
