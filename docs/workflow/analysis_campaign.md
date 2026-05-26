# Deep-Analysis Campaign

The phased pipeline that took OA.ko from "demangled symbols only" to "typed parameters,
struct fields, named globals" — a foundation suitable for translation toward compilable
C/C++ source.

---

## The pipeline (5 phases)

### Phase 1 — Function prototypes from mangled symbols

**Goal**: every C++ function has accurate parameter types, calling convention, and
`this` pointer.

**Input**: 21 693 C++ mangled symbols from OA.ko's `.symtab`, demangled via `c++filt`.

**Method**:
1. Run `nm --defined-only OA.ko` and `c++filt` to get demangled signatures
2. Parse each demangled C++ signature with a custom parser (handles ctor/dtor/free/method/static-init,
   strip `[clone …]`/`non-virtual thunk to` prefixes, split nested template args)
3. Map C++ types to Ghidra types (primitives via direct map; classes by name with
   sanitisation; templates → `void *`; references → pointers)
4. For each function, build prototype string and `set_function_prototype` with
   convention `__regparm3` (the Korg compiler default for kernel code on i386)

**Result (OA.ko)**:

| Metric | Value |
|---|---|
| Prototypes built | 15 958 (after address deduplication for ICF-folded functions) |
| Applied successfully | 15 929 + 29 (after fixing 24 nested-class types as placeholders) |
| Errors after fix | 0 |
| Wall clock | ~26 min |

Scripts: `/tmp/phase1_build.py`, `/tmp/phase1_apply.py`.

---

### Phase 2 — Class struct layouts

**Goal**: every class with field-access evidence gets a real struct definition. Decompiled
code goes from `*(byte *)(pBank + 0x5c)` to `pBank->field_0x5c`.

**Input**: every function's decompiled output (Phase 1 + 1.5 — see below).

**Method**:
1. **Decompile every function** (22 041 of them) in batches of 20 via `batch_decompile`
   (the API caps each batch at 20). Save to `/tmp/oa_decompiled.jsonl`.
2. **Aggregate**: for each function, identify typed-pointer parameters and locals;
   regex-extract every `*(TYPE *)(VAR + OFF)`, `VAR[OFF]`, `(TYPE *)(VAR + OFF)` access;
   contribute `(class, offset, size)` evidence
3. **Infer field names**: for any accessor-style method (`GetX`, `SetX`, `IsX`, etc.),
   look at its dominant `this`-relative offset and name that field with the leaf
   (`GetVolume` → `m_Volume` at the offset it reads from)
4. **Apply structs**: for each class, *append* fields to its existing placeholder
   struct using `add_struct_field` (no offset — appends sequentially), padding gaps
   with `undefined1[N]` arrays. **Never use `delete_data_type`** — that breaks every
   function signature referencing the type

**Result (OA.ko)**:

| Metric | Value |
|---|---|
| Functions decompiled | 22 041 |
| Classes with field evidence | 584 |
| Fields inferred | 13 497 |
| Field names from accessors | 457 |
| Structs successfully built | 583 (one failure: `int3`, a misparsed non-class) |
| Wall clock | ~7 h total (decompile pass ~18 min after fixing batch size; struct-build pass ~6 h due to cross-reference cascade) |

Scripts: `/tmp/phase2_decompile.py`, `/tmp/phase2_aggregate.py`, `/tmp/phase2_names.py`,
`/tmp/phase2_build.py`.

---

### Phase 3a — Return-type refinement

**Goal**: methods with predictable return types (setters → `void`, predicates → `bool`,
counts → `uint`) get them right.

**Method**: for each Phase 1 prototype, look at the function leaf name and apply
heuristics:

- `Set*`, `Update*`, `Clear*`, `Reset*`, `Initialize`, `Cleanup`, `Notify*`,
  `Enable*`/`Disable*`, etc. → `void`
- `Is*`, `Has*`, `Can*`, `Should*`, `Validate*` → `bool`
- `Get*Count`, `Get*Size`, `Get*Length`, `Compute*Num` → `uint`
- `Get*Name`, `Get*String` → `char *`
- `Find*`, `Access*`, `Allocate*`, `New*`, `Lookup*` → `void *`

**Result (OA.ko)**:

| Return type | Count |
|---|---|
| `void` | 7 736 |
| `bool` | 294 |
| `uint` | 79 |
| `void *` | 44 |
| `char *` | 7 |
| **Total refined** | **8 160** |

Wall clock: ~18 min.

Script: `/tmp/phase3_returns.py`.

---

### Phase 3b — Globals: SIMD constants & class singletons

**Goal**: globals that are obviously typed (16-byte SIMD float vectors, class `sInstance`
singleton pointers) get those types.

**Method**:
- For every rodata symbol matching `all*` or `absMask*` → apply `float[4]`
- For every static of pattern `Class::sInstance` → apply `Class *`

**Result (OA.ko)**:

| Metric | Value |
|---|---|
| SIMD constants typed | ~400+ of 934 candidates |
| `sInstance` singletons typed | 78 |
| Total successful applications | 478 |

Wall clock: ~20 s.

Script: `/tmp/phase3_globals.py`.

---

### Phase 4 — Local variables

Effectively **no-op** in this campaign. Ghidra's auto-inference of local variables is
already decent once Phase 1's typed parameters and Phase 2's struct fields are in
place — re-decompiling any function after Phases 1-3 yields meaningfully better local
names automatically. Manual renaming of `iVar1`/`uVar2` across 22 k functions has very
poor cost-benefit; deferred to per-function work when needed.

---

### Phase 5 — Other modules

Applied the same pipeline to the 4 other modules with C++ mangled symbols:

| Module | Prototypes | Structs | Returns | Notes |
|---|---|---|---|---|
| Eva | 34 360 (of 37 244 attempted; 92 %) | n/a (skipped — would cascade ~25 h) | 6 465 | Required `run_analysis` first; auto_analyzed=false on import |
| UpdateOS | 50 | 1 | 13 | Complete |
| OmapNKS4Module.ko | 56 | 0 (small) | 20 | Complete |
| InstallEXs | 26 | 0 (small) | 3 | Complete |

C-only modules (`GetPubIdMod.ko`, `loadmod.ko`, `loadoa`, `STGEnabler.ko`, `STGGmp.ko`)
have no mangled symbols and were left at the level of Ghidra's auto-analysis.

Script: `/tmp/phase5_module.py` (self-contained, takes module name + ELF path; supports
`--no-structs` for the cascade-avoiding mode used on Eva).

---

## What the decompilation now looks like

### Before (no Phase 1)

```c
undefined4 __regparm3 IsAuthorizedMultisampleBank(CSTGKLMManager *this)
{
  int in_EDX;
  ...
  if (((*(byte *)(in_EDX + 0x5c) & 8) == 0)
      && (*(int *)(in_EDX + 4) != 0)) {
    hash = (((*(byte *)(in_EDX + 0x5d) ^ 0x50c5d1f) * 0x1000193) ^ ...
  }
}
```

### After (Phase 1 only — typed params)

```c
undefined4 __regparm3
IsAuthorizedMultisampleBank(CSTGKLMManager *this, CSTGMultisampleBank *pBank)
{
  if (((pBank[0x5c] & 8) == 0) && (*(int *)(pBank + 4) != 0)) {
    hash = (((pBank[0x5d] ^ 0x50c5d1f) * 0x1000193) ^ ...
  }
}
```

### After (Phase 1 + 2 + 3)

```c
undefined4 __regparm3
IsAuthorizedMultisampleBank(CSTGKLMManager *this, CSTGMultisampleBank *pBank)
{
  if ((pBank->pPad_0x4c[0x10] & 8) == 0
      && pBank->pField_0x4 != 0) {
    hash = ((pBank->dwField_0x5d ^ 0x50c5d1f) * 0x1000193) ^ ...
  }
}
```

Field names are still generic (`field_0x5d`, `pPad_0x4c`) because the accessor-name pass
only labels fields with method-name evidence. Further refinement — semantic renaming
based on usage context — is the natural next step for any module being prepared for
source translation.

---

## Cost & time accounting

Approximate wall-clock for the OA.ko pipeline:

| Phase | Time |
|---|---|
| Phase 1 (prototypes) | 26 min |
| Phase 2.1 (decompile all) | 18 min |
| Phase 2.2-4 (aggregate, name, struct build) | ~6 h (cascade-dominated) |
| Phase 3a (returns) | 18 min |
| Phase 3b (globals) | 20 s |
| Phase 5 (Eva, no-structs) | 26 min |
| Phase 5 (3 other small modules) | ~3 min total |
| **Total OA.ko + Phase 5** | **~7.5 h** |

The dominant cost is the Phase 2 struct-build cascade — Ghidra re-validates every
cross-reference each time a struct gains a field, and the validation slows as more
structs gain layouts. For future projects, a Ghidra Java script (which would run inside
the JVM, no HTTP overhead, no per-call transaction) would do the same work in minutes
instead of hours; that requires enabling `GHIDRA_MCP_ALLOW_SCRIPTS=1` in the bridge.

---

## Lessons learned

| Lesson | Detail |
|---|---|
| Verify the address mapping per binary | nm-relative for `.ko`, full VMA for executables, COMDAT sections shift addresses unpredictably |
| Watch the API for quirks | `batch_decompile` silently caps at 20 — initial test of 200 returned only the first 20. Caused a half-completed run before we noticed |
| Save outside the write loop | Mid-loop saves fail with `Unable to lock due to active transaction`; do them at the end of the loop, or pause 1–2 s before |
| Never `delete_data_type` on a referenced struct | It nukes every function signature using it — discovered the hard way with `CSTGCCInfo`, which required re-applying 3 prototypes after the fact |
| Pre-existing placeholder types must be modified in place | Use `add_struct_field` (append) to grow them, NOT `delete + recreate` |
| The Phase 1 + 2 + 3 pipeline really does compose | Each phase makes the next one cleaner — Phase 3a's return-type heuristics work much better on functions with already-typed parameters |
