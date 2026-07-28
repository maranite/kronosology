---
name: x86-direct-execution-oracle-technique
description: mmap+PROT_EXEC direct execution of raw extracted machine-code bytes as a ground-truth oracle, stronger than a reimplemented interpreter, for self-contained functions (no external symbol/global refs)
metadata:
  type: project
---

Built 2026-07-28 for Eva's `CWaveformTemplate::EquationPolyline` (see
[[x86-magic-divide-interpreter-technique]] for the sibling technique this complements/
supersedes when applicable). The prior batch's ~350-line Python x86-32 interpreter
(`x86sim.py`) did NOT survive between sessions this time (scratchpad is nominally
session-scoped and this time it really was gone -- always check first, but don't assume
it will always be there). Rather than rebuild the interpreter from scratch, extracted and
directly executed the REAL machine code instead -- strictly stronger ground truth than any
reimplementation, and often less total work.

**When this applies**: the target function must be self-contained -- no absolute-address
memory operands (no `.rodata`/`.data`/`.bss` references), no `call`s to anything outside
itself, only stack-relative (`[esp+N]`) and immediate operands. Confirmed for
`EquationPolyline` by reading its full disassembly and checking for any `[0x0...]`-style
absolute operand or `call` instruction -- found none. This rules it out for anything that
reads a global/const table (e.g. `RandomCnt1-3`'s `.rodata` breakpoint tables, or anything
touching `this`) -- those still need either the interpreter or careful manual/static
tracing.

**Recipe**:
1. Find the function's file offset from its VA: `file_offset = VA - section.Addr +
   section.Off` using `readelf -S` on the section (`.text` here) containing it. Slice the
   raw bytes directly out of the binary with Python (`open(path,'rb').read()[fo_start:
   fo_end]`) -- no objdump needed for the extraction step itself, though objdump/`-dr -M
   intel` is still how you first confirm the function has no external refs.
2. Write a tiny C harness: `mmap(NULL, pagesize, PROT_READ|PROT_WRITE|PROT_EXEC,
   MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)`, `memcpy` the extracted bytes in, cast to a function
   pointer matching the REAL cdecl signature (get this right from stack-offset analysis
   first -- `[esp+N]` at function entry maps directly to argument N/4), call it. Compile
   with `gcc -m32` (target is x86-32) -- ordinary host gcc, no cross-toolchain needed, this
   is a userspace host-side tool same as the interpreter was.
3. Drive it via stdin/stdout for batch testing (read a test case, call, print result, loop)
   rather than one-shot per-process invocation -- avoids exec overhead across thousands of
   randomized cases, same batching discipline as the interpreter approach.
4. Write a Python reference model of the CANDIDATE C++ translation, regression-test it
   against the oracle's real stdout across thousands of randomized inputs, exactly the same
   "two independent renderers sharing only parsed facts" discipline as the interpreter
   technique and [[ckg_seq_backup_technique]]'s KAT generation.

**The Python reference model must apply explicit 32-bit truncation (`s32()`) at EVERY
multiply step that ground truth performs as a 2-operand `imul`, not just the final one.**
Bit twice by this in one session: `EquationPolyline`'s `pVal[i]*y` and `pPos[i]*period`
premultiplies are BOTH real 2-operand (`0F AF /r`) truncating `imul`s -- easy to
misdiagnose as the 1-operand full-64-bit form at a glance since the encoding difference is
subtle (`imul reg, r/m32` vs `imul r/m32` -- LOOK at the actual operand count in the
disassembly, don't assume from the surrounding idiom). Using Python's arbitrary-precision
ints for these intermediate products passed thousands of moderate-magnitude test cases
"by accident" (the truncation never triggered) and only broke on a dedicated large-|y|
stress sweep (~2000 mismatches out of ~2000, not subtle at all once triggered -- unlike the
earlier RandomCnt session's single-product wraparound bug, catching this one was easy; the
danger is skipping the stress sweep entirely and never finding it). The eventual C++
translation needs NO special handling either way -- plain 32-bit `int` arithmetic on this
target reproduces every truncation point automatically; the s32() calls are purely an
artifact of verifying with an arbitrary-precision host language.

**Ground-truth OOB-read quirks are real and must be preserved, not "fixed".**
`EquationPolyline`'s Duff's-device search can legitimately land on `idx==0` (reads
`pPos[-1]`/`pVal[-1]`, one slot before the caller's nominal array start) or the
not-found/`idx==count` case (reads `pPos[count]`/`pVal[count]`, one slot past the nominal
last valid index). Confirmed real via the oracle (not a translation artifact) by testing
both boundary shapes directly against the extracted machine code with deliberately padded
test arrays. No caller of this particular function exists anywhere in ground truth's own
`.text`/`.rodata`/`.data` (confirmed by an exhaustive byte-pattern scan for its own
address), so the real caller's actual buffer-padding convention is unverifiable from this
binary alone -- documented as a real, load-bearing quirk for any future caller this project
reconstructs, not silently bounds-clamped away.

See Eva's `include/waveform_template.h` (commit pending as of this note, follow-up #2 on
top of `476a0dd`) for the worked example.
