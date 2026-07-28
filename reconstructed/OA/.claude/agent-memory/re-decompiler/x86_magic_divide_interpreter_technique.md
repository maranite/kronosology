---
name: x86-magic-divide-interpreter-technique
description: Host-side x86-32 instruction interpreter used as a ground-truth execution oracle to derive and verify GCC magic-number divide-by-constant translations, first built for Eva's CWaveformTemplate
metadata:
  type: project
---

Built 2026-07-28 for Eva's `CWaveformTemplate` (see [[ckg_seq_backup_technique]] for the
STATIC-analysis sibling of this technique -- this one is dynamic/execution-based, for
when the branch/threshold structure is too dense to symbolically re-derive by eye with
confidence, e.g. multiple magic constants per function, comparisons whose direction is
easy to get backwards, mixed `sar` (floor) vs `idiv`/magic-multiply (truncate-toward-
zero) divisions in the same function).

**When to reach for this instead of the static decoder**: the static decoder
(`ckg_seq_backup_technique.md`) works well for large FAMILIES of near-identical tiny
accessors sharing one shape. This technique is for a SMALL number of functions, each
individually dense and branchy (GCC divide-by-constant idioms, multi-branch quantization
dispatch), where hand-tracing risks a subtle bug (comparison direction, sign of a
correction) that only shows up on specific inputs. Confirmed on `CWaveformTemplate`'s
`Equation*` LFO waveform-shape family (`.text+0x089847a0..0x08986200` in Eva) after a
prior pass rejected the whole class as too complex for confident manual tracing.

**Build once, reuse across a whole function family**:
1. `objdump -dr -M intel` the WHOLE address range in one shot (works even across many
   small functions).
2. A ~350-line Python x86-32 interpreter (`x86sim.py` in this batch's scratchpad, not
   checked into the repo -- host-side tooling only) parsing objdump's `ADDR: bytes  mnem
   ops` lines into per-function instruction dicts, with a `CPU` class executing:
   `mov/lea/add/sub/cmp/test/and/xor/neg/sar/shr/shl/imul(1/2/3-operand)/idiv/movzx/
   movsx/cwde/push/pop/jmp/jCC/cmovCC/nop/xchg/ret`. Runs against real memory (a sparse
   byte dict) with a virtual `esp` and cdecl-style stack args, computing real x86 flags
   (ZF/SF/OF/CF) for every arithmetic/logic/shift instruction so `jCC`/`cmovCC` conditions
   are exact, not approximated.
3. **Compute instruction length from the NEXT parsed instruction's address, never from
   the printed hex-byte column of a single objdump line.** objdump wraps long-encoded
   instructions (7-8+ byte `movzx`/`imul` immediates) onto a continuation line with no
   mnemonic (just leftover hex bytes) that a `ADDR: bytes<TAB>mnem` regex silently skips
   -- deriving length from that regex's own captured hex-byte count undercounts by
   whatever spilled onto the continuation line and desyncs every subsequent fallthrough
   address. Fixed by computing `length = next_real_instr_addr - this_addr` from the
   already-correctly-ordered list of successfully parsed instructions (which naturally
   skips the same continuation lines the parser skipped, so the arithmetic stays
   consistent even though it never explicitly detects the wrap).
4. Call any function directly (`insns[start_addr]`, run until `ret`) with arbitrary
   arguments written to a synthetic cdecl stack frame -- no need to reconstruct callers
   or a whole running program. For member-function accessors, write field values
   directly into a fake "this" object's memory addresses before the call.
5. Use it as ground truth in TWO ways: (a) `sweep()` a function's output across a wide
   `x` range at generator-friendly `y`/`z` values to visually find bin-boundary fractions
   before writing any C; (b) once a hand-written C translation exists, regression-test it
   against the interpreter's real output over thousands of randomized inputs (including
   negative/boundary values) via a plain Python re-implementation of the candidate C
   logic -- this is the same "two independent renderers sharing only parsed facts, not
   each other's output" discipline as `ckg_seq_backup_technique.md`'s KAT generation, just
   with the interpreter itself as the more-authoritative oracle instead of a second
   symbolic parse.

**Magic-multiply divisor confirmation is itself a sub-tool, not manual lookup**: write
`magicdiv(n, M, shift, correction, add_n)` in pure Python replicating exactly the
`imul reg` (`edx:eax = (int64)a*(int64)b`, taking `edx` = `(a*b)>>32` with Python's
arithmetic right-shift on an arbitrary-precision int, which matches x86 signed `sar`
semantics on the true 64-bit product) plus whatever correction sequence ground truth
actually codes, then brute-force search candidate `D` in `range(1, 4096+)` for which
`magicdiv(n,...) == trunc(n/D)` (C-style truncate-toward-zero) holds across ~4000
random `n` plus small edge values, for EVERY tested `n` -- this is "execute candidate
divisor values" made literal and automatic rather than eyeballed against a memorized
Hacker's Delight table. **Two correction shapes both occur in the same binary and must
be told apart from the actual instruction sequence, not assumed:**
- `edx - sign(n)` (real x86: `sar reg,0x1f` on the numerator, then `sub edx,reg`) →
  `+trunc(n/D)`.
- `sign(n) - edx` (same sign compute, but `sub edx,ecx` with operands reversed, or
  equivalently computed via the OTHER register) → `-trunc(n/D)`. Two branches of the
  exact same function can use both, one after the other, to get a sign-mirrored value
  pair (seen repeatedly in `CWaveformTemplate`'s StepTri6/StepSaw4/StepSaw6 -- one bin's
  positive `y/6` and another bin's negative `-y/6` are literally two different correction
  directions on the same `0x2aaaaaab` multiply, not one value later negated).
- **A third, easy-to-miss ingredient**: when the magic multiplier's top bit is set (its
  32-bit encoding is a "negative" value, e.g. `0x88888889`), Hacker's Delight's algorithm
  needs an EXTRA `add edx,<numerator-reg>` (a plain 32-bit register add, `hi = s32(hi +
  n)` in the simulator, not a true 64-bit add) BEFORE the final shift and sign
  correction. Missing this (`add_n=False`) makes the divisor-finder return `None` for
  every shift/divisor combination even though the constant is real and correct --
  confirmed for `0x88888889` (found `add edx,<numerator>` right after the `imul` in the
  real disassembly, easy to miss since the SAME correction step, `sub edx,sign`, still
  runs afterward and looks identical to the no-`add_n` case at a glance). Once
  `add_n=True` is modeled, `0x88888889` cleanly resolves to div 15 (shift 3), 30 (shift
  4), or 60 (shift 5) depending on the extra shift actually coded -- this constant
  recurs across `EquationRandomSH1-3`/`RandomCnt1-3`/`EquationPolyline` at shift 5 (div
  60), suggesting a shared "byte-domain amplitude scaled through /60" fixed-point
  convention across this whole equation family, not a coincidence.

**`sar` vs `idiv`/magic-multiply is not a stylistic choice, preserve which one ground
truth used.** `sar reg,N` in C is `reg >> N`, which floors toward negative infinity for
negative operands. `idiv`/magic-multiply-with-correction in C is `reg / D`, which
truncates toward zero. For positive dividends they agree; for negative ones they differ
by exactly 1 whenever the division isn't exact. Concretely bit us once: an early
`EquationStepTri6` draft substituted `y/2` for a real `sar eax,1` (`y>>1`) threshold/value
computation and passed every positive-`y` test but failed on odd negative `y` -- caught
only by the randomized regression sweep, not by inspection, because the bug is invisible
on the small hand-picked KAT literals a human would normally reach for first.

See [[stg_value_getter_family]] for the STATIC-decoder family this dynamic technique
complements, and Eva's own `include/waveform_template.h` (commit `de2591d`) for the
worked example this was built for.
