/*
 * waveform_template.h  -  CWaveformTemplate, the LFO waveform-shape generator
 * (`.text+0x089847a0..0x089861b0` in the real `Eva` binary).
 *
 * PROVENANCE: a prior pass (commit `9a97903`, see list.h's header comment) traced this
 * class, confirmed it is genuinely self-contained (zero external subsystem dependency
 * beyond one leaf vtable call into a global Peg-screen singleton pointer, `DrawWave`
 * only), but rejected it as too complex for that pass's budget: the per-equation bodies
 * are dense GCC divide-by-constant-via-multiply ("magic number") idioms plus multi-branch
 * quantization threshold chains. This pass (2026-07-28) revisits it with a scripted
 * decoder: a small x86-32 instruction interpreter (`tools/` is host-side only, not
 * shipped -- see commit message) that replays the REAL disassembly instruction-by-
 * instruction against synthetic inputs, used as a ground-truth oracle to derive and then
 * regression-verify (thousands of randomized (x,y,z) triples per function, including
 * negative/boundary values) each hand-written C++ translation. Real divide-by-constant
 * magic multipliers confirmed this way (`0x2aaaaaab`->div 6, `0x55555556`->div 3,
 * `0x66666667`->div 5 or 10 depending on an extra coded shift, `0x88888889`->div 15/30/60
 * depending on shift, this last one needing the less-common "add dividend before shift"
 * correction since its top bit is set) -- see the family's other Equation* bodies for
 * where these recur.
 *
 * PARTIAL RECONSTRUCTION, real scope decision, not an oversight: of the class's 28 real
 * methods, this pass (plus the two follow-up batches below) reconstructs the 8
 * pure-integer, no-runtime-table Equation* bodies (`EquationNone`/`Triangle`/`Saw`/
 * `Square`/`StepTri4`/`StepTri6`/`StepSaw4`/`StepSaw6`), the 6 multi-branch/table-driven
 * `EquationRandomSH1-3`/`EquationRandomCnt1-3` bodies (2026-07-28 follow-up #1),
 * `EquationPolyline` (2026-07-28 follow-up #2, see further below), plus `GetData()`/
 * `Shape()` (the two table-lookup accessors) and the destructor.
 * Deliberately NOT reconstructed, each for a distinct, documented reason:
 *   - Constructor (`CWaveformTemplate(EWaveformType,u16,u16)`): allocates both m_pbData/
 *     m_pbShapeTable via two HAL_DisableInterrupts()/malloc()/HAL_EnableInterrupts()-
 *     bracketed loops that call THROUGH `sm_ifpWaveformEquations[type]` (a real function-
 *     pointer table at .data+0x091fb960) -- needs the full 20-entry equation table wired
 *     up (this pass supplies 14 of them) to be meaningfully testable, not just compilable.
 *   - `MakeShapeTable(EWavehapeType,int)`: direct inspection (2026-07-28) confirms this is
 *     NOT cleanly separable into an integer-only subset: the dispatch on its two params is
 *     `type==0` -> integer byte-fill loop; `type==1 && amount==0` -> the SAME integer loop
 *     (shared fallthrough, `cmp ebp,0x0; jne <fpu path>`); `type==1 && amount!=0`, or any
 *     other `type` -> a real x87 curve-fit path (`fld1`/`fild`/`fdiv`/`fmul`/`fisttp`
 *     against runtime float constants at .rodata+0x8e871f0) inside the SAME function body,
 *     one straight-line fallthrough away from the integer loop. Reconstructing even the
 *     integer-only branches faithfully would require either modeling the FPU branch too
 *     (out of scope, same reason as the Equation* FPU outliers below) or silently
 *     truncating real dispatch behavior -- deferred whole, not partially forced in.
 *   - `EquationExpSawUp/ExpSawDown/ExpTriangle/Guitar/Sine`: genuine x87 FPU DSP curve
 *     math (`fsqrt`, real `sqrt@plt`/`sin@plt`/`cos@plt` calls against runtime float/double
 *     constants) -- same "real DSP computation, out of scope" exclusion this project has
 *     already established elsewhere (see OA.ko's `CSTGString::GetPluckDelay`-class
 *     outliers in the STG value-getter family notes).
 *   - `DrawWave(PegThing*,...)`: calls THROUGH a global static `PegThing*` screen-singleton
 *     pointer's vtable slot 0x64/4=25 (confirmed via disassembly: `mov ecx,[ds:0x939b410];
 *     mov edx,[ecx]; call [edx+0x64]`) -- the Peg GUI framework (`PegThing`/`PegPoint`/
 *     `PegColor`) is not modeled anywhere in this project yet; real, out-of-scope external
 *     dependency, not a decoding difficulty.
 *
 * 2026-07-28 FOLLOW-UP BATCH: `EquationRandomSH1/2/3` and `EquationRandomCnt1/2/3`, left
 * "traced but not verified" by the pass above, are now fully reconstructed and regression-
 * verified (the same x86-32 instruction-interpreter oracle described above, ~5000-7000
 * randomized (x,y,z) triples per function including negative/boundary values and small-z
 * edge sweeps, 0 mismatches after fixing an off-by-one in the interpolation-index search
 * caught only by the regression sweep, not by inspection).
 *   - `EquationRandomSH1/2/3`: real multi-branch "sample-and-hold" quantized-breakpoint
 *     generators. Breakpoints are `trunc(k*(z-1)/60)` (magic-multiply `0x88888889`,
 *     shift 5) or `trunc((z-1)/D)` (`0x2aaaaaab`/`0x55555556`, `D` in {3,6,12}) for
 *     hardcoded integer coefficients `k` -- NOT read from any `.rodata` table (that
 *     distinguishes them from `RandomCnt1-3` below, despite sharing the same breakpoint
 *     fractions). Per-bin VALUES are simple fixed fractions of `y` (`y/2`, `y/3`, `y/5`,
 *     `y/6`, `y/10`, `y>>1`, sign-flipped per bin) -- SH3 in particular reduces to a plain
 *     alternating-sign `y>>1`/`-(y>>1)` ladder across 8 bins once decoded.
 *   - `EquationRandomCnt1/2/3`: real breakpoint-table-driven LINEAR INTERPOLATION (not a
 *     plain step lookup) between per-segment start/end values, confirmed via direct byte
 *     inspection of the 3 `.rodata` const-byte tables per function (7/10/8 entries for
 *     Cnt1/Cnt2/Cnt3 respectively, at 0x08f1dd7f..0x08f1dd8d/0x08f1dd6b..0x08f1dd76/
 *     0x08f1dd5b..0x08f1dd6a): an UNSIGNED byte "segment start position" table (in 60ths
 *     of `z-1`, ground truth's own `movzx`), and two SIGNED byte "segment start/end value"
 *     tables (in 30ths of `y`) where `tableEnd[i] == tableStart[i+1]` for every `i` --
 *     i.e. consecutive segments share endpoints, confirmed from the raw bytes, not
 *     assumed. `x == z-1` (the exact last sample) is a real special case, NOT part of the
 *     table walk, computing a distinct fixed fraction of `y` per function (`-y/2` for
 *     Cnt3, `-4y/15` for Cnt2, `-y/15` for Cnt1) -- the "edge-wraparound averaging tail
 *     case" flagged but not yet decoded by the earlier pass. The final per-segment LERP
 *     step (`(valEnd-valStart)*(x-segStart) / (segEnd-segStart)`) uses a real 2-operand
 *     `imul` that keeps only the low 32 bits of that product (unlike every other multiply
 *     in this family, which uses the 1-operand form and keeps the full 64-bit product in
 *     edx:eax) -- this makes the LERP's numerator genuinely wrap at 32 bits for extreme
 *     `y`, reproduced automatically by using plain 32-bit `int` arithmetic in the
 *     translation below rather than anything wider, confirmed by regression-testing at
 *     `y` magnitudes large enough to actually trigger the wraparound.
 *
 * 2026-07-28 FOLLOW-UP BATCH #2: `EquationPolyline`, deferred whole by the first pass as
 * needing its own dedicated batch, is now fully reconstructed. Unlike the rest of this
 * file, verified with a DIFFERENT oracle technique: the prior batches' x86-32 Python
 * interpreter did not survive between sessions, and this function's real machine code
 * (0x08985f80..0x089861a7 in the real `Eva` binary, 560 bytes, confirmed self-contained --
 * no absolute-address memory operands, no external calls, only stack-relative and
 * immediate operands) was extracted verbatim and executed DIRECTLY (mmap'd
 * `PROT_EXEC`, called with real cdecl args from a small host harness) as a strictly
 * stronger ground-truth oracle than any reimplemented interpreter -- regression-tested
 * against a Python reference model of the C++ translation below across ~6900 randomized
 * `(x,y,period,count,pPos[],pVal[])` tuples (including a dedicated large-|y| sweep to
 * exercise 32-bit multiply wraparound), 0 mismatches.
 *   - Real signature: `EquationPolyline(int x, int y, int period, int count,
 *     const unsigned char *pPos, const signed char *pVal)`. NOT part of the
 *     `sm_ifpWaveformEquations[]` 3-arg family below -- a distinct 6-arg static helper
 *     evaluating a caller-supplied polyline (parallel `pPos[]`/`pVal[]` breakpoint arrays)
 *     at query position `x`. `period` is used directly (unlike the RandomSH/RandomCnt
 *     family's `z-1`, this function never subtracts 1 from it).
 *   - Algorithm: real ground truth is a Duff's-device-unrolled (mod-4 remainder preamble +
 *     groups-of-4 main loop) linear scan for the smallest `idx` in `[0, count)` with
 *     `trunc(pPos[idx]*period/60) > x` (the same `0x88888889`/shift-5/div-60 magic-divide
 *     idiom as `EquationRandomCnt3`'s position table), falling back to `idx == count` if
 *     no such index exists. The unrolling is a pure perf optimization with no semantic
 *     effect -- reproduced below as an equivalent plain scan, confirmed behaviorally
 *     identical to the real unrolled code via the direct-execution oracle across every
 *     branch shape (immediate `idx==0` hit, mid-scan hit, not-found/`idx==count`, and the
 *     `count==1` single-element special case). Once `idx` is found, it LERPs between
 *     segment `[idx-1, idx]` exactly like `RandomCntLerp` above (`valA`/`valB` from
 *     `pVal[]` scaled in 30ths of `y`, numerator `(valB-valA)*(x-xValPrev)`, real signed
 *     truncating `idiv` by `(xValCur-xValPrev)`).
 *   - GROUND-TRUTH QUIRK, preserved exactly, not "fixed": both `pPos[idx-1]`/`pVal[idx-1]`
 *     accesses are real and land OUTSIDE the nominal `[0, count)` range at the two
 *     boundaries -- `idx==0` reads index `-1` (one slot BEFORE the buffer the caller
 *     nominally supplies), and `idx==count` (not-found) reads index `count` (one slot
 *     PAST the last nominal valid index). Confirmed via the direct-execution oracle, not
 *     assumed: any real caller of this function must supply `pPos`/`pVal` with one extra
 *     valid element on each side for these accesses to be defined. `EquationPolyline` has
 *     zero call sites anywhere in ground truth's own `.text`/`.rodata`/`.data` (confirmed
 *     by an exhaustive byte-pattern scan for its own address -- the one hit found is the
 *     function's own `.symtab` entry, not a reference), so this quirk cannot be
 *     cross-checked against a real caller; most likely reached only from Peg-editor UI
 *     code (matching this file's `src/editor/` placement) outside this project's scope,
 *     same as `DrawWave` above.
 *   - The `pVal[i]*y` and `pPos[i]*period` premultiplies are real 2-operand (32-bit
 *     truncating) `imul`s, matching plain C `int*int` overflow semantics -- like
 *     `RandomCntLerp`'s numerator, plain `int` arithmetic in the translation reproduces
 *     this automatically, confirmed by the large-|y| wraparound sweep above.
 *
 * REAL LAYOUT (confirmed by GetData()/Shape()/the destructor's own field accesses; no
 * padding needed, natural x86 alignment matches every offset):
 *   +0x00 (u8*)  m_pbData        malloc'd sample-index table, GetData()'s source
 *   +0x04 (u8*)  m_pbShapeTable  malloc'd shape-lookup table, Shape()'s source
 *   +0x08 (u16)  m_wSize         GetData()'s modulus (period length of m_pbData)
 *   +0x0a (u16)  m_wCount        Shape()'s clamp bound (length of m_pbShapeTable)
 *
 * EQUATION FAMILY CALLING CONVENTION: all `Equation*` methods EXCEPT `EquationPolyline`
 * (a distinct 6-arg helper, see its own writeup above) are plain (non-regparm)
 * `static` member functions taking exactly `(int x, int y, int z)` on the stack --
 * confirmed by every body reading its 3 args from `[esp+4]`/`[esp+8]`/`[esp+0xc]` (no
 * `this`). Called only indirectly through `sm_ifpWaveformEquations[]` in ground truth
 * (never a direct call site anywhere in `.text`), so the exact (x,y,z) semantic meaning
 * is inferred from the bodies themselves: `x` is a phase/index into the period `z`
 * (compared against fractions of `z` throughout), `y` is a signed amplitude scale (every
 * output value is some sign/fraction of `y`), `z` is the waveform period length. Every
 * `Equation*` here uses REAL truncating (`idiv`/magic-multiply, matching C's `/`) division
 * for period-fraction thresholds and VALUE computations that came from `idiv`/magic-
 * multiply, but REAL arithmetic-shift-right (`sar`, matching C's `>>` on a signed int,
 * which floors rather than truncates toward zero for negative operands -- deliberately NOT
 * rewritten as `/2`/`/4` for that reason) wherever ground truth used `sar` directly rather
 * than a division idiom. This distinction was load-bearing: an early draft of
 * `EquationStepTri6` that used `/2` in place of `>>1` passed on positive-`y` test vectors
 * but failed on odd negative `y` (`sar` floors, `/` truncates toward zero -- they disagree
 * by exactly 1 for negative odd dividends) until caught by the randomized regression sweep
 * described above.
 */

#ifndef EVA_WAVEFORM_TEMPLATE_H
#define EVA_WAVEFORM_TEMPLATE_H

class CWaveformTemplate {
public:
	~CWaveformTemplate();

	/* m_pbData[idx mod m_wSize], mod normalized into [0, m_wSize) for either sign of idx
	 * (real ground truth: a genuine while-loop normalization, unrolled 8x for speed in
	 * ground truth -- reproduced here as the equivalent, simpler while loops). Ground
	 * truth's own return-value width is `movzx` (zero-extend), i.e. this really does
	 * return the raw stored byte as unsigned, not sign-extended -- a DIFFERENT inlined
	 * copy of this same lookup inside the not-yet-reconstructed DrawWave() uses `movsx`
	 * (sign-extend) instead; both are real, distinct, ground-truth-confirmed behaviors at
	 * their respective call sites, not a contradiction.
	 */
	unsigned char GetData(int idx) const;

	/* m_pbShapeTable[clamp(m_wCount/2 + param, 0, m_wCount - 1)]. */
	unsigned char Shape(char param) const;

	static int EquationNone(int x, int y, int z);
	static int EquationTriangle(int x, int y, int z);
	static int EquationSaw(int x, int y, int z);
	static int EquationSquare(int x, int y, int z);
	static int EquationStepTri4(int x, int y, int z);
	static int EquationStepTri6(int x, int y, int z);
	static int EquationStepSaw4(int x, int y, int z);
	static int EquationStepSaw6(int x, int y, int z);

	static int EquationRandomSH1(int x, int y, int z);
	static int EquationRandomSH2(int x, int y, int z);
	static int EquationRandomSH3(int x, int y, int z);
	static int EquationRandomCnt1(int x, int y, int z);
	static int EquationRandomCnt2(int x, int y, int z);
	static int EquationRandomCnt3(int x, int y, int z);

	static int EquationPolyline(int x, int y, int period, int count,
				     const unsigned char *pPos, const signed char *pVal);

private:
	friend struct CWaveformTemplateTestHooks;

	unsigned char *m_pbData;
	unsigned char *m_pbShapeTable;
	unsigned short m_wSize;
	unsigned short m_wCount;
};

#endif
