---
name: eva-bitmaskl-facts-and-vfatentry-closure
description: CVFATEntry's remaining ~28 methods re-confirmed genuinely out of scope (2 new vtable-dispatch findings); CBitMaskL (13 methods) reconstructed as the follow-up cluster
metadata:
  type: project
---

**CVFATEntry closure (2026-07-28), see [[eva-vfat-entry-class-facts]] for the base facts.**
Re-checked all 28 deferred methods via direct `objdump -dr` before moving on. Two findings
not previously enumerated in `vfat_entry.h`'s own header comment:
- `GetNumByteToSerialize() const` (.text+0x08145900) and `GetNumSlotToSerialize() const`
  (.text+0x081458b0) both dispatch through `call *0x8(%eax)` — the SAME vtable slot
  `CDirEntry::GetName()` uses (dir_entry.h). Genuinely out of scope for this project's
  "raw-buffer, no real vtable" convention, same reason as `GetSlotIndex()`.
- `OnShortNameChanged()`/`OnShortExtChanged()` (0x08142df0/0x08143080) both dispatch
  through `call *0x94(%esi)` (a DIFFERENT vtable slot from CBitMaskL's `Api+0x94` global —
  this one is per-instance, on whatever `esi` holds, not the global `Api`). Confirmed
  genuinely deep, not just "unread" as the header speculated.
No new tractable ground in CVFATEntry — verdict unchanged from the original 16/44 split.

**CBitMaskL (2026-07-28, commit `415d6a0`).** Found via a fresh `nm -C` sweep (grep class
name, count uncovered classes 4-14 methods each, filter GUI/dialog/god-object noise). Small
(13 methods, .text+0x0838e350..0x0838e730), non-polymorphic, self-contained 32-bit
bitmask/iterator value class. Real layout, all 4 fields confirmed by `ProcessEndian()`
byte-swapping all of them:
```
+0x00 (u16) mLo      -- low 16 bits of a 32-bit mask
+0x02 (s16) mSize    -- bit-count bound (GetNumOfSetBit()/getbit() loop bound)
+0x04 (u16) mHi      -- high 16 bits of the mask
+0x06 (u16) mCursor  -- getbit()'s persistent scan-position cache
```
`is_set(unsigned long mask) const` is the one method with an external ref: for ODD `mask`
!= 1 it fires a real `Api`+0x94 soft-assert (`ds:0x930a1f4`, same global/slot already
established at ~15 other Eva call sites, "Assertion failed in module %s, line %i.\n" +
`PcgSaveInfoProg.cpp`-rooted filename) — but tracing the control flow by hand proved the
post-assert jump target is IDENTICAL machine code to the even-mask path, so the omission
(this project's standard Api+0x94 convention) provably changes nothing about the return
value, not just "probably fine by convention."

**Oracle technique refinement — testing functions with ONE excludable unsafe branch.**
`is_set()` has an external call on the odd-mask-!=1 branch only; rather than skip oracle
testing for the whole function, proved the branch is behaviorally a no-op by static
tracing, then oracle-tested every OTHER input domain (even masks + mask==1) at full
volume and treated the excluded branch as covered by the static proof, not by execution.
Worth reusing: a single conditional external call doesn't disqualify a function from the
oracle technique if you can first prove (via `objdump -dr` control-flow reading) that the
call's own successor state is identical to an already-oracle-tested path.

`getbit()`'s asm found/not-found exits store subtly different `mCursor` values (found:
cursor = tested-position+1; not-found: cursor = wherever the bound check tripped) — this
was NOT obvious from a first read and only got caught by writing a literal goto-preserving
translation and diffing against the oracle across a 5-call sequential-walk test, not by
"cleaning up" the loop shape first. Same lesson as `EquationPolyline`'s OOB-read quirks:
translate branchy stateful asm literally, verify, THEN consider simplifying — never the
reverse order.

Manifest 1648 -> 1661/37795 (13 methods, all header-only in `include/bit_mask_l.h`).
