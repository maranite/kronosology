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
 * methods, this pass reconstructs the 8 pure-integer, no-runtime-table Equation* bodies
 * (`EquationNone`/`Triangle`/`Saw`/`Square`/`StepTri4`/`StepTri6`/`StepSaw4`/`StepSaw6`)
 * plus `GetData()`/`Shape()` (the two table-lookup accessors) and the destructor.
 * Deliberately NOT reconstructed this pass, each for a distinct, documented reason:
 *   - Constructor (`CWaveformTemplate(EWaveformType,u16,u16)`): allocates both m_pbData/
 *     m_pbShapeTable via two HAL_DisableInterrupts()/malloc()/HAL_EnableInterrupts()-
 *     bracketed loops that call THROUGH `sm_ifpWaveformEquations[type]` (a real function-
 *     pointer table at .data+0x091fb960) -- needs the full 20-entry equation table wired
 *     up (this pass only supplies 8 of them) to be meaningfully testable, not just
 *     compilable. A real future increment once the remaining Equation-family and
 *     MakeShapeTable bodies below are done.
 *   - `MakeShapeTable(EWavehapeType,int)`: mixes this pass's integer idiom with a SEPARATE
 *     real x87 floating-point curve-fit path (`fld`/`fmul`/`fdiv`/`fucomi` chains against
 *     runtime float constants at .rodata+0x8e871f0 etc) for a subset of shape types --
 *     genuine DSP curve math, not a quantization idiom, out of scope for this pass same as
 *     the Equation* FPU outliers below.
 *   - `EquationPolyline(int,int,int,int,const u8*,const char*)`: real multi-point linear
 *     interpolation across a caller-supplied polyline (not a single magic-divide idiom) --
 *     traced enough to confirm it reuses the same `0x88888889`/shift-5/div-60 idiom as
 *     `EquationRandomCnt3` and friends (see below) but the interpolation walk itself needs
 *     a dedicated pass.
 *   - `EquationRandomSH1/2/3`, `EquationRandomCnt1/2/3`: real multi-branch "random sample-
 *     and-hold" quantized-breakpoint generators (`RandomCnt1/2/3` table-driven off 3 small
 *     `.rodata` const-byte breakpoint tables at 0x08f1dd5c..0x08f1dd8d, confirmed read
 *     with `objdump -s`; `RandomSH1/2/3` use the same breakpoint fractions but as hardcoded
 *     immediates) with a real edge-wraparound averaging tail case on the last table entry
 *     -- traced far enough (via the same x86 interpreter, see `sweep()`-style breakpoint
 *     probing in the commit) to confirm the shape of the problem, not yet turned into a
 *     verified translation.
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
 * REAL LAYOUT (confirmed by GetData()/Shape()/the destructor's own field accesses; no
 * padding needed, natural x86 alignment matches every offset):
 *   +0x00 (u8*)  m_pbData        malloc'd sample-index table, GetData()'s source
 *   +0x04 (u8*)  m_pbShapeTable  malloc'd shape-lookup table, Shape()'s source
 *   +0x08 (u16)  m_wSize         GetData()'s modulus (period length of m_pbData)
 *   +0x0a (u16)  m_wCount        Shape()'s clamp bound (length of m_pbShapeTable)
 *
 * EQUATION FAMILY CALLING CONVENTION: all `Equation*` methods are plain (non-regparm)
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

private:
	friend struct CWaveformTemplateTestHooks;

	unsigned char *m_pbData;
	unsigned char *m_pbShapeTable;
	unsigned short m_wSize;
	unsigned short m_wCount;
};

#endif
