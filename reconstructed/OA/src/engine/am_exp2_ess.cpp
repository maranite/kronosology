// SPDX-License-Identifier: GPL-2.0
/*
 * am_exp2_ess.cpp  -  am_exp2_ess(float) (`.text+0x244e6`, 177 bytes),
 * a fast single-precision 2^x approximation. The only external symbol
 * USTGParamConvertor::TaperSmoothingFactor calls (param_convertor.cpp,
 * batch 55).
 *
 * Kept in its own translation unit because real ground truth compiles
 * this specific function with SSE scalar-float codegen (`movss`/
 * `mulss`/`addss`/`subss`/`rcpss`/`cvttss2si`/`cvtsi2ss`), unlike the
 * rest of param_convertor.cpp which is plain x87 (`fld`/`fadd`/...) --
 * matching this project's existing `-mfpmath=sse` per-TU override
 * precedent (see lfo_tables.cpp, engine_startup_bits.cpp, etc in the
 * Makefile).
 *
 * Algorithm (a standard degree-2/2 rational minimax approximation of
 * 2^x, confirmed via `objdump -dr`, not guessed):
 *   1. Clamp x to [-127.49999237, 127.49999237].
 *   2. n = round-to-nearest-integer(x) (via `x+0.5` then
 *      truncate-with-a-conditional-decrement-if-the-truncation-rounded-
 *      up-for-negative-inputs -- the classic branchless floor-of-
 *      round-half-up idiom; real code: `t=x+0.5; n=(int)t; if (t<0) --n`
 *      is NOT quite what's there -- real sequence compares t against 0
 *      and only subtracts when t<0 AND the truncation actually
 *      rounded away from zero, i.e. plain `n = (int)__builtin_floorf(x + 0.5f)`
 *      reproduces it exactly for all real inputs here since floorf
 *      matches the confirmed comiss+cvttss2si+cmovb+sub sequence).
 *   3. frac = x - (float)n (range roughly [-0.5, 0.5]).
 *   4. 2^n is built directly via the IEEE-754 bit trick: exponent
 *      field = (n+127)<<23, mantissa/sign = 0 -- i.e. a raw float bit
 *      pattern, not ldexpf().
 *   5. 2^frac ~= 1 + 2*frac*P(frac^2) / (Q(frac^2) - frac*P(frac^2))
 *      with P(t)=(0.023093348f*t + 20.202066f)*t + 1513.9069f and
 *      Q(t)=233.18420f*t + 4368.2114f -- the reciprocal is a real
 *      `rcpss` (a ~12-bit-precision hardware approximation, NOT an
 *      exact division) reproduced here via the matching `_mm_rcp_ss`
 *      intrinsic for the same bit-exact approximation, not `1.0f/q`.
 *   6. return (2^n) * (2^frac).
 */

/* Suppresses xmmintrin.h's own `#include <mm_malloc.h>` (only needed
 * for _mm_malloc/_mm_free, which this file never uses) -- mm_malloc.h
 * pulls in <stdlib.h>, unavailable under the real kernel build's
 * `-nostdinc -ffreestanding`. Same "just enough libc-header shim"
 * approach this project already uses elsewhere for freestanding builds. */
#define _MM_MALLOC_H_INCLUDED
#include <xmmintrin.h>

/* `target("sse")` (rather than a Makefile-level -msse2) so this
 * compiles under BOTH of this project's two build modes without
 * per-file plumbing: `make objs`'s plain `%.hostobj: %.cpp` pattern
 * rule (no per-TU CFLAGS_x.o support -- that's a Kbuild-only
 * mechanism, see the CFLAGS_am_exp2_ess.o line in the Makefile, which
 * only takes effect for the real `make ko` build) and the real
 * -m32 -ffreestanding target build, both of which would otherwise
 * reject <xmmintrin.h>'s `_mm_rcp_ss`/`_mm_set_ss`/`_mm_cvtss_f32`
 * (all `always_inline`, which GCC refuses to inline without SSE
 * enabled for the enclosing function). */
extern "C" __attribute__((target("sse"))) float am_exp2_ess(float x)
{
	if (x > 127.49999237f) x = 127.49999237f;
	if (x < -127.49999237f) x = -127.49999237f;

	float t = x + 0.5f;
	int n = (int)__builtin_floorf(t);

	float nf = (float)n;
	float frac = x - nf;
	float frac2 = frac * frac;

	float q = 233.18420410f * frac2 + 4368.21142578f;
	float p = (0.02309334837f * frac2 + 20.20206642f) * frac2 + 1513.90686035f;
	p = p * frac;
	q = q - p;

	__m128 qv = _mm_set_ss(q);
	float recip = _mm_cvtss_f32(_mm_rcp_ss(qv));

	float r = p * recip;
	r = r + r;
	r = r + 1.0f;

	int expBits = (n + 127) << 23;
	float scale;
	__builtin_memcpy(&scale, &expBits, 4);

	return scale * r;
}
