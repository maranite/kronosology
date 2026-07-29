---
name: rtparm-family-deferred-followup-2026-07-29
description: "All 7 members deferred from the RTParm bottom-up batch reconstructed in a dedicated follow-up pass; 2 turned out simpler than first-assessed (compiler if-conversion artifacts), 3 stayed genuinely dense; 1 real KAT-caught bug in this pass's own code (packed-32-bit pointer); manifest 3529->3536/21689, this session (uncommitted at write time, see status.md)."
metadata:
  type: project
---

## What was done

Follow-up to [[rtparm_family_bottom_up_batch_2026-07-29]] (the prior batch,
commit `48b1bf0`): took the 7 deliberately-deferred members
(`LimitRTParmEditValues{,Row}`, `UpdateRTParmIfSame_GE`,
`GetRTParmModAndID`, `RTParmShortNameGroup::GetRTParmShortNameStringPtr`,
`DoRTParmMultiEnable{PE,GE}`) and fully re-traced each one from fresh
`objdump -dr -M intel -r` disassembly (the `-r` for relocations turned out
load-bearing -- see below). All 7 landed; `AssignRTParmFunction_Drm`
(8294B) was NOT attempted -- still too dense for one session's budget,
remains `pending`.

## Key technical findings, in case the pattern recurs elsewhere in OA.ko

1. **A disassembly listing's literal immediate can secretly be a
   relocated symbol+offset.** `GetRTParmModAndID`'s `cmp eax,0x1f40` etc.
   LOOK like comparisons against a raw integer, but `objdump -dr` (not
   plain `-d`) shows every one of them carries a real `R_386_32 gKS`
   relocation -- the true runtime comparison is `eax` (a genuine pointer)
   against `&gKS+0x1f40`, not `eax` against the bare integer `0x1f40`.
   Missing this the first pass led to a wrong "caller passes a computed
   integer offset" theory; ALWAYS pass `-r` to objdump for any function
   whose immediates look suspiciously "just numbers" and cross-check
   against the function's own known parameter TYPE (here, `RTParm*` per
   the mangled name -- a real pointer type is a strong hint the compare
   is pointer-vs-relocated-symbol, not int-vs-constant).

2. **Ground-truth branchy control flow can be a pure if-conversion
   artifact of a much simpler source, but only prove it by exhaustively
   tracing every branch, not by pattern-matching.**
   `LimitRTParmEditValuesRow` (421B, flagged by the first pass as its
   single highest-risk function) turned out to be exactly
   `clamp(f4,lo,hi); clamp(f6,lo,hi); clamp(f0,min(f4,f6),max(f4,f6))`
   once every one of its ~15 basic blocks was individually traced and
   cross-checked for self-consistency (two DIFFERENT compiled copies of
   the final `clamp(f0,...)` logic exist in the binary -- one for the
   `f4<=f6` case, one for `f4>f6` -- confirmed to be true mirror images of
   each other, not two different algorithms). Do not shortcut this: the
   first pass's own "too dense" verdict was reasonable caution, not
   wrong, and the simplification was only trustworthy after full tracing,
   not assumed from the shape.

3. **Two differently-addressed copies of loop-continue code in the
   decompiled output does not necessarily mean two different algorithms.**
   `UpdateRTParmIfSame_GE`'s "the fail path re-enters mid-loop at a
   different label than the match path" (the first pass's stated reason
   for deferral) is exactly this: GCC duplicated the loop-continue
   sequence once for the self-skip path and once for the post-match path.
   Both do the identical `i++; continue`. A plain C `for`/`continue` loop
   reproduces both without needing `goto`.

4. **`DoRTParmMultiEnable{PE,GE}`'s pointer-arithmetic "sum" gotcha**: a
   secondary table pointer that starts at `outerLoopVar*STRIDE` and a
   loop counter that ALSO starts at `outerLoopVar` and both advance by 1
   step per iteration cancel out algebraically -- the table pointer at
   any point in the loop equals `innerLoopVar*STRIDE`, NOT
   `(outerLoopVar+innerLoopVar)*STRIDE` as a naive reading suggests. Worth
   double-checking this exact pattern (a "walking pointer initialized
   from the SAME variable that also seeds the loop counter") anywhere
   else nested-loop table code appears in this project.

## The `RTParm_menu_ge_*`/`RTParm_menu_pe_*` tables are placeholder-zero, not real data

`src/engine/rtparm_family.cpp` declares `unsigned char RTParm_menu_ge_ge[0x80];`
etc. with NO initializer -- these are genuinely zero-filled `.bss`
placeholders in this project's own build, NOT populated with the real
`.rodata` bytes from the ground-truth ELF, even though the real bytes are
easily dumpable (`objdump -s -j .rodata`). This is apparently a
deliberate, pre-existing scope boundary (only the CODE that indexes into
these tables was ever the reconstruction target, not the tables' own
content) -- confirmed by checking `include/oa_rtparm_family.h`'s own
declarations, which are plain `extern unsigned char [...]` with no
mention of real content. **Any future KAT that needs a specific descriptor
min/max or menu string must poke the placeholder array directly in the
test** (`RTParm_menu_ge_ge[idx*0x20+0x18] = ...`), NOT assume the real
ground-truth `.rodata` bytes are present -- wasted real debugging time
this pass tracking down why a hand-computed "expected" value (read from
the real ELF) didn't match the (actually-correct) reconstructed code's
output, when the real bug was the test's false assumption about the
linked-in table's content.

## Real bug this pass caught in its OWN new code (not ground truth)

`GetRTParmShortNameStringPtr`'s first draft read the class's packed
32-bit string-table-pointer field (`this->+0x0`, a 4-byte field on the
real -m32 target, already documented via `SetRTParmShortNameStringPtr`'s
own comment) as a native 8-byte pointer:
`unsigned char *strTable = *(unsigned char **)base;` -- reads 4 garbage
bytes from `base+4` as the upper 32 bits of a 64-bit pointer on a 64-bit
KAT host, immediate segfault. Fixed:
`(unsigned char *)(unsigned long)(*(unsigned int *)base)`. This is the
SAME bug class already documented multiple times elsewhere in this
project's memory (`CKGBankManager::ms_poInstance`,
`RTParmShortNameGroup::SetRTParmShortNameStringPtr` itself in the PRIOR
pass) -- caught it in the SETTER but wrote the identical bug fresh in the
GETTER of the same field one pass later. **General reminder: every field
this project's own disassembly shows as a plain `mov DWORD PTR[...],reg`
(4-byte store OR load of something semantically a pointer) needs the
packed-32-bit treatment on BOTH the read and write side -- fixing one
direction doesn't inoculate the other.**

## Verification

`make verify` full suite green (2 files touched, no new binaries this
pass -- everything added to `test_rtparm_family`/`rtparm_family_stubs`).
Real `make ko-clean && make ko KDIR=/home/build/linux-kronos` build green;
`nm OA.ko | c++filt` confirms all 7 new symbols' mangled names match
ground truth exactly (e.g.
`RTParmShortNameGroup::GetRTParmShortNameStringPtr(RTParmNameProductID, unsigned char, unsigned char)`).
Manifest delta verified via a `git stash`-isolated baseline (stashed only
this session's own 4 files, regenerated, popped, regenerated again,
diffed the full name sets) -- confirmed exactly `+7, 0 regressions`
(`DoRTParmMultiEnableGE`, `DoRTParmMultiEnablePE`, `GetRTParmModAndID`,
`GetRTParmShortNameStringPtr`, `LimitRTParmEditValues`,
`LimitRTParmEditValuesRow`, `UpdateRTParmIfSame_GE`), matching the 7
targeted names with nothing unexpected added or lost.
