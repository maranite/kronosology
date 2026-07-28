---
name: eva-vfat-entry-class-facts
description: CVFATEntry (Eva) real field offsets, checksum algorithm, and slot-array layout confirmed 2026-07-28 — reuse before touching GetSlotIndex/CFATEntry or any further CVFATEntry method
metadata:
  type: project
---

`CVFATEntry` (Eva, `.text+0x08142510..0x08147...`, 44 raw `nm -C` symbols) is a real
subclass of `CFATEntry` -> `CDirEntry` (confirmed via dtor tail-`jmp` chain and shared
`CZ`-field offsets), reconstructed in `reconstructed/Eva/include/vfat_entry.h` as a
self-contained opaque-byte-buffer class (16/44 methods) continuing the filesystem series
alongside `CDirEntry` ([[dir_entry]], not yet its own memory file — see
`reconstructed/Eva/include/dir_entry.h`) and `CZ` (`cz_util.h`).

**Confirmed real field offsets** (every one a literal immediate transcribed directly from
disassembly, not inferred):
- `+0x04` (u8*) / `+0x0c` (u32 len): short-NAME raw ptr/len — SAME offsets as
  `CDirEntry::mShortName`'s own `RawPtrField()`/`RawFlagField()` (dir_entry.h), strong
  evidence `CVFATEntry`'s base subobject really is a `CDirEntry` sharing that storage.
- `+0x14` (u8*) / `+0x1c` (u32 len): short-EXT raw ptr/len, same relationship to
  `CDirEntry::mShortExt`.
- `+0x74 + 0x20*i`, `i` in `[0,20)`: "long-name-continuation slot i populated" u32 field
  (nonzero = in use). Stride 0x20, count 20 cross-confirmed independently by
  `GetMaxCharForLongName()`'s own real constant `0x104` == `260` == `20 * 13`
  (`GetMaxCharPerEntry()`).
- `+0x2ec` u8 `mCurrentSlotIndex`, `+0x2ed` u8 `mCurrentAliasChecksum`, `+0x2f0` u32
  `mOutputCodePage`, `+0x2f4` u32 `mHasValidLongNameExt` (own concrete non-virtual field,
  NOT the same storage as `CDirEntry`'s own same-named VIRTUAL method), `+0x2f8` u32
  `mCurrentNumForShortNameExt`.
- **Real overlap, not a bug**: slot 19's own computed range `[0x2d4,0x2f4)` OVERLAPS the 5
  named fields above. Whatever "slot 19" conceptually is, ground truth's own compiler
  output shares that storage with these named fields — kept as raw offsets into ONE shared
  buffer in the header rather than forcing a non-overlapping C++ struct that ground truth
  itself doesn't have. If a future pass adds more `CVFATEntry` fields, check for this kind
  of overlap again rather than assuming disjoint layout.

**Checksum algorithm**: `ComputeChecksum(prev,next) = ror8(prev,1) + next` (mod 256) — the
classic DOS/VFAT short-name alias-checksum recurrence. `GetAliasChecksum(const u8*)` applies
it across exactly 11 bytes (8.3 short name+ext), seeded by byte[0] with no rotate.
`GetAliasChecksum()` (own-field form) builds that same 11-byte buffer from the object's own
short-name/short-ext raw fields, space-padding (`0x20`) any position past the real length —
verified via the direct-execution oracle across lengths swept past the real 8/3-char clamp
bounds (0 mismatches).

**Still open / deferred, each for a specific reason** (see `vfat_entry.h`'s own header
comment for full detail):
- `GetSlotIndex()` (.text+0x08143b30) calls `CFATEntry::GetFirstDataByte()`
  (.text+0x080fae80), which itself calls the ALREADY-REAL `CDirEntry::GetName()`
  (dir_entry.h) — fully traceable, blocked only on `CFATEntry` not existing as a real
  class/base yet. Good next target if `CFATEntry` ever gets modeled.
- `OnShortNameChanged()`/`OnShortExtChanged()` (.text+0x08142df0/0x08143080) are NOT the
  trivial "set flag to 1" shape `OnLongNameChanged()`/`OnLongExtChanged()` turned out to be
  — confirmed by size alone (~15x larger, 0x28e/0x28f bytes vs 0xf) — genuinely unread,
  don't assume symmetry from the Long* pair's own trivial shape.
- `Serialize()`/`Deserialize()`/`GenerateShortNameExt()`/3x `operator=()`/ctors/dtor/etc:
  real `CCodePage::ConvertToUnicode()`/`CLittleEndObj::SetWord()` calls, real un-modeled
  `CZ`-container internals (`Insert`/content ctors), and a per-instance vtable dispatch
  through `this+0x8` (confirmed in `Serialize()`'s own disassembly) — same "real
  subsystem dependency, out of scope" class as `CZ`'s own 55 un-reconstructed container
  methods.

See [[x86-direct-execution-oracle-technique]] for the verification method used on the 3
branchy methods (`GetAliasChecksum()` own-field form, both `IsLongNameBitArrayEmpty()`
overloads) — 60000 randomized trials + 3 exact spot-checks, 0 mismatches, extending that
technique's confirmed-applicable set beyond `EquationPolyline`.
