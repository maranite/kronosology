/*
 * CSTGEQ -- five core biquad-coefficient math functions (batch 30, sec
 * 10.178). Identified since batch 22/23 (sec 10.170/10.171) as blocking
 * `CSTGHDRMiniModel::Initialize()`, and confirmed in batch 29 (sec
 * 10.177) as the TRUE root blocker of the entire `CSetListEQ`/
 * `CSTGEffectRack` subsystem (`CSetListEQ::Initialize()` alone calls
 * `CalculatePeakingBeta()` nine times).
 *
 * All five are reconstructed here as WHOLE-FUNCTION verbatim x87
 * inline-asm transcriptions of the real `.text+0x247e0..0x24a00`
 * disassembly, not reduced to plain C float arithmetic -- two use real
 * `fptan`, and the two coefficient functions do heavy multi-register x87
 * stack shuffling across real branches (fxch-heavy code manipulating 4-6
 * live ST(i) registers at once), which does not fit this project's
 * existing single-input/output x87 primitive-wrapper convention (sec
 * 10.117's `MulRoundToFloat`/`FYL2X`, sec 10.151's `FMul`/`FAdd`/
 * `FLess`/`FLessEq`). Every asm block below is self-contained (only "m"
 * memory operands plus the two pointer/int "r" operands actually needed
 * for direct struct-offset addressing), matching the sec 10.151 "no
 * register-tied x87 constraints across chained operations" discipline.
 *
 * `.rodata.cst4` constants resolved via the sec 10.176 symtab-index
 * method (this `.ko`'s thousands of same-named `.rodata.cst4` instances
 * make a name-based `objdump -s -j .rodata.cst4` lookup silently read
 * the wrong instance) -- all ten relocations touched by these five
 * functions share ONE section instance (symtab index 459, section index
 * 14867), confirmed via `readelf -r --wide` + `readelf --syms --wide` +
 * `readelf -x 14867`:
 *   +0x38 = 0x39094 21f = 0.00013089970161672682f  (2*pi/48000)
 *   +0x3c = 0x3f000000  = 0.5f
 *   +0x40 = 0x40490fdb   = 3.1415927410125732f (pi)
 *   +0x44 = 0x3f800000  = 1.0f
 *   +0x48 = 0x3fbf0243   = 1.49225652217865f (tan-argument clamp)
 *
 * Every branch of both coefficient functions was independently
 * cross-checked this batch via a from-scratch Python x87-stack emulator
 * (mechanically replaying the exact objdump mnemonic+operand stream,
 * tracking FPU stack depth/position exactly like real hardware) before
 * being transcribed into the inline asm below -- both coefficient
 * functions' stack depth returns to exactly 0 at every `ret` in every
 * branch, a strong structural self-consistency check (a mis-numbered
 * `%st(i)` register reference would either underflow the emulator's
 * stack or leave a nonzero residual depth).
 */
#include "oa_global.h"

/*
 * CalculateLowShelfBeta(float freq) (`.text+0x247e0`, 34 bytes):
 * beta = tan((freq * 2*pi/48000) * 0.5)
 */
float CSTGEQ::CalculateLowShelfBeta(float freq)
{
	static const float kTwoPiOverFs = 0.00013089970161672682f; /* .rodata.cst4+0x38 */
	static const float kHalf = 0.5f;                            /* .rodata.cst4+0x3c */
	float result;

	__asm__ __volatile__(
		"flds %1\n\t"        /* ST0 = 2*pi/Fs */
		"fmuls %2\n\t"       /* ST0 = ST0 * freq  = omega */
		"fmuls %3\n\t"       /* ST0 = ST0 * 0.5   = omega/2 */
		"fptan\n\t"          /* ST0 = 1.0 (pushed marker), ST1 = tan(omega/2) */
		"fstp %%st(0)\n\t"   /* discard the FPTAN 1.0 marker -> ST0 = tan(omega/2) */
		"fstps %0"
		: "=m" (result)
		: "m" (kTwoPiOverFs), "m" (freq), "m" (kHalf)
	);
	return result;
}

/*
 * CalculateHighShelfBeta(float freq) (`.text+0x24810`, 40 bytes):
 * beta = tan((pi - freq * 2*pi/48000) * 0.5)
 */
float CSTGEQ::CalculateHighShelfBeta(float freq)
{
	static const float kTwoPiOverFs = 0.00013089970161672682f; /* .rodata.cst4+0x38 */
	static const float kPi = 3.1415927410125732f;               /* .rodata.cst4+0x40 */
	static const float kHalf = 0.5f;                            /* .rodata.cst4+0x3c */
	float result;

	__asm__ __volatile__(
		"flds %1\n\t"        /* ST0 = 2*pi/Fs */
		"fmuls %2\n\t"       /* ST0 = ST0 * freq = omega */
		"fsubrs %3\n\t"      /* ST0 = pi - ST0   = pi - omega */
		"fmuls %4\n\t"       /* ST0 = ST0 * 0.5 */
		"fptan\n\t"
		"fstp %%st(0)\n\t"
		"fstps %0"
		: "=m" (result)
		: "m" (kTwoPiOverFs), "m" (freq), "m" (kPi), "m" (kHalf)
	);
	return result;
}

/*
 * CalculatePeakingBeta(float freq, float bw, float *outOmega)
 * (`.text+0x24910`, 68 bytes):
 *   omega = freq * 2*pi/48000
 *   *outOmega = omega                          (byproduct output)
 *   x = omega / (2*bw)
 *   return tan(x <= 1.49225652f ? x : 1.49225652f)
 * The clamp keeps the fptan() argument away from the pi/2 singularity as
 * bandwidth narrows towards 0. `scratch` is reused for both the
 * mid-computation round-trip (matching the original's own `(%esp)`
 * local slot) and, implicitly, is the function's own working register --
 * the real code reuses the identical stack slot for both roles.
 *
 * IMPORTANT direction note on `fdivrp`/`fsubrp` (found the hard way this
 * batch, via a real KAT failure + a from-scratch hardware microtest,
 * NOT assumed): for the POPPING two-register forms
 * (`f{sub,div}{,r}p %st,%st(i)`), the NON-reverse form computes
 * SOURCE-op-DEST (`ST(i) := ST(0) op ST(i)`) and the REVERSE form
 * computes DEST-op-SOURCE (`ST(i) := ST(i) op ST(0)`) -- easy to get
 * backwards by analogy with the non-popping `fsub %st(i),%st`/
 * `fsubr %st(i),%st` forms (dest=ST0 there), which behave the OTHER
 * way around. Confirmed empirically (`/tmp/microtest2.cpp`-style probe,
 * not just re-reading the manual) before trusting any golden value in
 * this file's KAT that depends on one of these four mnemonics.
 */
float CSTGEQ::CalculatePeakingBeta(float freq, float bw, float *outOmega)
{
	static const float kTwoPiOverFs = 0.00013089970161672682f; /* .rodata.cst4+0x38 */
	static const float kThreshold = 1.49225652217865f;          /* .rodata.cst4+0x48 */
	float scratch;

	__asm__ __volatile__(
		"flds %1\n\t"              /* ST0 = 2*pi/Fs */
		"fmuls %2\n\t"             /* ST0 = ST0*freq = omega */
		"fsts (%3)\n\t"            /* *outOmega = omega (no pop, ST0 still = omega) */
		"flds %4\n\t"              /* push bw: ST0=bw, ST1=omega */
		"fadd %%st(0),%%st\n\t"    /* ST0 = bw+bw = 2*bw */
		"fdivrp %%st,%%st(1)\n\t"  /* ST(1) := ST(1)/ST0 = omega/(2*bw); pop -> ST0 = omega/(2*bw) */
		"fstps %0\n\t"             /* scratch = omega/(2*bw); pop (stack empty) */
		"flds %0\n\t"              /* push scratch back */
		"flds %5\n\t"              /* push threshold: ST0=thresh, ST1=scratch */
		"fxch %%st(1)\n\t"         /* ST0=scratch, ST1=thresh */
		"fucomi %%st(1),%%st\n\t"  /* compare scratch vs thresh */
		"jbe 1f\n\t"
		"fstp %%st(0)\n\t"         /* scratch>thresh: discard scratch, keep thresh */
		"jmp 2f\n"
		"1:\n\t"
		"fstp %%st(1)\n\t"         /* scratch<=thresh: ST(1)=scratch; pop -> keep scratch */
		"2:\n\t"
		"fptan\n\t"
		"fstp %%st(0)\n\t"
		"fstps %0"
		: "=m" (scratch)
		: "m" (kTwoPiOverFs), "m" (freq), "r" (outOmega), "m" (bw), "m" (kThreshold)
		: "cc"
	);
	return scratch;
}

/*
 * CalculatePeakingCoefficients(float p0, float p1, float p2, STGEQCoefficients *out)
 * (`.text+0x24960`, 160 bytes). Branches on `p0 >= 1.0` (fucomi+jae).
 * `p2` is the only one of the three floats ever passed through `fcos`,
 * so it reads as a raw angle (not a pre-computed cosine). Field write
 * order in BOTH branches is out->b0, out->b1 (via a round-trip through
 * `scratch`), out->b2, out->a1, out->a2 -- exactly matching
 * `STGEQCoefficients`'s five packed floats at +0x0/+0x4/+0x8/+0xc/+0x10.
 * Every `%%st(i)` index below was cross-checked via the from-scratch
 * Python x87-stack emulator (see file header) -- both branches leave the
 * FPU stack at depth 0 on return.
 */
void CSTGEQ::CalculatePeakingCoefficients(float p0, float p1, float p2, STGEQCoefficients *out)
{
	float scratch;

	__asm__ __volatile__(
		"flds %1\n\t"               /* push p0 */
		"flds %2\n\t"               /* push p1 */
		"flds %3\n\t"               /* push p2 */
		"fld1\n\t"                  /* push 1.0 */
		"fld %%st(2)\n\t"           /* push copy of ST(2) (=p1) */
		"fadd %%st(1),%%st\n\t"     /* ST0 = ST0+ST1 = p1+1.0 */
		"fxch %%st(4)\n\t"          /* swap ST0<->ST4: ST0=p0 */
		"fucomi %%st(1),%%st\n\t"   /* compare p0 vs 1.0 */
		"jae 1f\n\t"

		/* --- p0 < 1.0 branch --- */
		"fdivr %%st(1),%%st\n\t"
		"fmul %%st(3),%%st\n\t"
		"fld %%st(0)\n\t"
		"fadd %%st(2),%%st\n\t"
		"fdivr %%st(2),%%st\n\t"
		"fstps %0\n\t"
		"flds %0\n\t"
		"fmul %%st,%%st(5)\n\t"
		"fxch %%st(5)\n\t"
		"fstps (%4)\n\t"            /* out->b0 */
		"fxch %%st(2)\n\t"
		"fcos\n\t"
		"fadd %%st(0),%%st\n\t"
		"fchs\n\t"
		"fmul %%st(4),%%st\n\t"
		"fstps %0\n\t"
		"flds %0\n\t"
		"fsts 0x4(%4)\n\t"          /* out->b1 */
		"fxch %%st(3)\n\t"
		"fsubr %%st(1),%%st\n\t"
		"fmul %%st(4),%%st\n\t"
		"fstps 0x8(%4)\n\t"         /* out->b2 */
		"fxch %%st(2)\n\t"
		"fchs\n\t"
		"fstps 0xc(%4)\n\t"         /* out->a1 */
		"fsubrp %%st,%%st(1)\n\t"
		"fchs\n\t"
		"fmulp %%st,%%st(1)\n\t"
		"fstps 0x10(%4)\n\t"        /* out->a2 */
		"jmp 2f\n"

		"1:\n\t"
		/* --- p0 >= 1.0 branch --- */
		"fmul %%st(3),%%st\n\t"
		"fxch %%st(4)\n\t"
		"fdivr %%st(1),%%st\n\t"
		"fld %%st(4)\n\t"
		"fadd %%st(2),%%st\n\t"
		"fmul %%st(1),%%st\n\t"
		"fstps (%4)\n\t"            /* out->b0 */
		"fxch %%st(2)\n\t"
		"fcos\n\t"
		"fadd %%st(0),%%st\n\t"
		"fchs\n\t"
		"fmul %%st(2),%%st\n\t"
		"fstps %0\n\t"
		"flds %0\n\t"
		"fsts 0x4(%4)\n\t"          /* out->b1 */
		"fxch %%st(4)\n\t"
		"fsubr %%st(1),%%st\n\t"
		"fmul %%st(2),%%st\n\t"
		"fstps 0x8(%4)\n\t"         /* out->b2 */
		"fxch %%st(3)\n\t"
		"fchs\n\t"
		"fstps 0xc(%4)\n\t"         /* out->a1 */
		"fxch %%st(1)\n\t"
		"fsubrp %%st,%%st(2)\n\t"
		"fxch %%st(1)\n\t"
		"fchs\n\t"
		"fmulp %%st,%%st(1)\n\t"
		"fstps 0x10(%4)\n\t"        /* out->a2 */
		"2:\n\t"
		: "=m" (scratch)
		: "m" (p0), "m" (p1), "m" (p2), "r" (out)
		: "cc"
	);
}

/*
 * CalculateShelvingCoefficients(float gain, float beta, eEQShelvingType type,
 *                                STGEQCoefficients *out)
 * (`.text+0x24840`, 199 bytes). Two orthogonal branches:
 *  - `gain >= 1.0` vs `gain < 1.0` (fucomi+jb, decided FIRST)
 *  - `type == kEQLowShelf` vs `!= kEQLowShelf` (plain integer `test`,
 *    decided per-branch, its zero-flag read only much later -- safe
 *    because no other flag-setting instruction executes in between)
 * Both `gain` branches converge on ONE of two SHARED tail blocks
 * (confirmed real via the disassembly's own control flow: the
 * `gain>=1.0` branch performs its own copy of the tail's first
 * instruction then jumps PAST that instruction into the tail's middle,
 * while the `gain<1.0` branch jumps to the tail's true start -- a real
 * compiler tail-merge). Reproduced here as two shared labels (`3:`/`5:`)
 * that BOTH branches jump to directly -- behaviorally identical to the
 * original's own inlined-then-skip variant, since the skipped
 * instruction (`fstp %%st(1)` / `fstp %%st(0)`) is idempotent regardless
 * of which branch executes it.
 * out->b0 is written unconditionally by both gain-branches. Depending on
 * `type`, exactly one of out->b1/out->a1 gets its SIGN FLIPPED relative
 * to what was written earlier in the same call (both fields are always
 * written twice: once via `fsts` mid-branch, then either left alone or
 * overwritten with its own negation in the shared tail) -- confirmed via
 * the from-scratch Python x87-stack emulator (see file header) for all
 * 4 branch combinations (gain>=1/<1 crossed with type==0/!=0), all
 * ending at FPU stack depth 0.
 */
void CSTGEQ::CalculateShelvingCoefficients(float gain, float beta, eEQShelvingType type,
                                            STGEQCoefficients *out)
{
	float scratch;
	int typeVal = (int)type;

	__asm__ __volatile__(
		"flds %1\n\t"              /* push gain */
		"flds %2\n\t"              /* push beta */
		"fld1\n\t"                 /* push 1.0 */
		"fld %%st(1)\n\t"          /* push copy of ST(1) (=beta) */
		"fadd %%st(1),%%st\n\t"    /* ST0 = beta(copy)+1.0 */
		"fxch %%st(3)\n\t"         /* swap ST0<->ST3: ST0=gain */
		"fucomi %%st(1),%%st\n\t"  /* compare gain vs 1.0 */
		"jb 4f\n\t"

		/* ============ gain >= 1.0 ============ */
		"fmul %%st(2),%%st\n\t"
		"fxch %%st(3)\n\t"
		"fdivr %%st(1),%%st\n\t"
		"testl %3,%3\n\t"          /* type == kEQLowShelf ? (ZF, survives untouched to the je below) */
		"fld %%st(3)\n\t"
		"fadd %%st(2),%%st\n\t"
		"fmul %%st(1),%%st\n\t"
		"fstps (%4)\n\t"           /* out->b0 */
		"fld %%st(1)\n\t"
		"fsubp %%st,%%st(4)\n\t"
		"fmul %%st,%%st(3)\n\t"
		"fxch %%st(3)\n\t"
		"fstps %0\n\t"
		"flds %0\n\t"
		"fsts 0x4(%4)\n\t"         /* out->b1 (tentative) */
		"fxch %%st(1)\n\t"
		"fsubp %%st,%%st(2)\n\t"
		"fxch %%st(1)\n\t"
		"fmulp %%st,%%st(2)\n\t"
		"fxch %%st(1)\n\t"
		"fstps %0\n\t"
		"flds %0\n\t"
		"fsts 0xc(%4)\n\t"         /* out->a1 (tentative) */
		"je 1f\n\t"
		"fstp %%st(1)\n\t"         /* type != kEQLowShelf: keep the a1-side scratch on top */
		"jmp 3f\n"
		"1:\n\t"
		"fstp %%st(0)\n\t"         /* type == kEQLowShelf: discard it, keep the b1-side scratch */
		"jmp 5f\n"

		/* ============ gain < 1.0 ============ */
		"4:\n\t"
		"fdivr %%st(1),%%st\n\t"
		"fmul %%st(2),%%st\n\t"
		"testl %3,%3\n\t"
		"fld %%st(0)\n\t"
		"fadd %%st(2),%%st\n\t"
		"fdivr %%st(2),%%st\n\t"
		"fstps %0\n\t"
		"flds %0\n\t"
		"fmul %%st,%%st(4)\n\t"
		"fxch %%st(4)\n\t"
		"fstps (%4)\n\t"           /* out->b0 */
		"fxch %%st(2)\n\t"
		"fsubr %%st(1),%%st\n\t"
		"fmul %%st(3),%%st\n\t"
		"fstps %0\n\t"
		"flds %0\n\t"
		"fsts 0x4(%4)\n\t"         /* out->b1 (tentative) */
		"fxch %%st(1)\n\t"
		"fsubp %%st,%%st(2)\n\t"
		"fxch %%st(1)\n\t"
		"fmulp %%st,%%st(2)\n\t"
		"fxch %%st(1)\n\t"
		"fstps %0\n\t"
		"flds %0\n\t"
		"fsts 0xc(%4)\n\t"         /* out->a1 (tentative) */
		"jne 3f\n\t"
		"fstp %%st(0)\n\t"         /* type == kEQLowShelf: discard the a1-side scratch */
		"jmp 5f\n\t"

		/* ============ shared tail: type != kEQLowShelf -> negate a1 ============ */
		"3:\n\t"
		"fchs\n\t"
		"movl $0,0x10(%4)\n\t"     /* out->a2 = 0 */
		"fstps 0xc(%4)\n\t"        /* out->a1 = -(tentative a1) */
		"movl $0,0x8(%4)\n\t"      /* out->b2 = 0 */
		"jmp 6f\n\t"

		/* ============ shared tail: type == kEQLowShelf -> negate b1 ============ */
		"5:\n\t"
		"fchs\n\t"
		"movl $0,0x10(%4)\n\t"     /* out->a2 = 0 */
		"fstps 0x4(%4)\n\t"        /* out->b1 = -(tentative b1) */
		"movl $0,0x8(%4)\n\t"      /* out->b2 = 0 */
		"6:\n\t"
		: "=m" (scratch)
		: "m" (gain), "m" (beta), "r" (typeVal), "r" (out)
		: "cc"
	);
}
