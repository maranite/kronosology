/*
 * waveform_template.cpp  -  see include/waveform_template.h for full ground-truth
 * provenance, the x86 magic-multiply divisors confirmed via the scripted decoder, and the
 * `sar` (`>>`, floors) vs `idiv`/magic-multiply (`/`, truncates toward zero) distinction
 * that every body below is careful to preserve exactly as ground truth used it.
 */

#include "waveform_template.h"

extern void HAL_DisableInterrupts();
extern void HAL_EnableInterrupts();

#include <cstdlib>

CWaveformTemplate::~CWaveformTemplate()
{
	if (m_pbData) {
		HAL_DisableInterrupts();
		free(m_pbData);
		HAL_EnableInterrupts();
	}
	if (m_pbShapeTable) {
		HAL_DisableInterrupts();
		free(m_pbShapeTable);
		HAL_EnableInterrupts();
	}
}

unsigned char CWaveformTemplate::GetData(int idx) const
{
	int n = m_wSize;
	while (idx < 0)
		idx += n;
	while (idx >= n)
		idx -= n;
	return m_pbData[idx];
}

unsigned char CWaveformTemplate::Shape(char param) const
{
	int count = m_wCount;
	int idx = (count >> 1) + (int)param;
	if (idx < 0)
		idx = 0;
	else if (idx >= count)
		idx = count - 1;
	return m_pbShapeTable[idx];
}

int CWaveformTemplate::EquationNone(int /*x*/, int /*y*/, int /*z*/)
{
	return 0;
}

/* Real ground truth: a single division by (z>>1), with `x` folded into [0,z) first by
 * wrapping the last quarter back by a full period (`x - z`) rather than a separate
 * modulo -- reproduced exactly, not simplified into an equivalent-looking modulo form,
 * to keep the truncating-division rounding identical for every input. */
int CWaveformTemplate::EquationTriangle(int x, int y, int z)
{
	int half = z >> 1;
	if (x < (z >> 2))
		return (x * y) / half;
	if (x < ((z * 3) >> 2))
		return ((half - x) * y) / half;
	return ((x - z) * y) / half;
}

int CWaveformTemplate::EquationSaw(int x, int y, int z)
{
	return (((z >> 1) - x) * y) / z;
}

int CWaveformTemplate::EquationSquare(int x, int y, int z)
{
	if ((z >> 1) > x)
		return y >> 1;
	return -(y >> 1);
}

/* 4-step quantized triangle: real ground truth defaults the two middle quarters to a
 * literal 0 (not `y>>1` scaled by anything) rather than a smaller fraction of `y` --
 * confirmed by the `xor eax,eax` / untouched-`eax` fallthrough in both middle bins,
 * not a placeholder. */
int CWaveformTemplate::EquationStepTri4(int x, int y, int z)
{
	if (x < (z >> 2))
		return -(y >> 1);
	if (x < (z >> 1))
		return 0;
	if (x < ((z * 3) >> 2))
		return y >> 1;
	return 0;
}

/* 6-step quantized triangle. Bin thresholds at z/6, z/3, z>>1, (z*2)/3, (z*5)/6 -- the
 * first/third/fifth via the `0x2aaaaaab`/`0x55555556` magic multipliers (confirmed div 6
 * and div 3, no extra shift), the middle one via a real `sar z,1`. Bin values alternate
 * between `y>>1` (outer two bins) and `y/6` (inner four, sign flips per bin) -- `y/6` uses
 * real truncating division (`0x2aaaaaab` on `y` itself, not `y>>1`-style shifting), so it
 * is written as `y / 6` here, deliberately not folded into the `>>1` idiom. */
int CWaveformTemplate::EquationStepTri6(int x, int y, int z)
{
	if (x < (z / 6))
		return -(y >> 1);
	if (x < (z / 3))
		return -(y / 6);
	if (x < (z >> 1))
		return y / 6;
	if (x < ((z * 2) / 3))
		return y >> 1;
	if (x < ((z * 5) / 6))
		return y / 6;
	return -(y / 6);
}

/* 4-step quantized (ascending) saw: thresholds at z>>2, z>>1, (z*3)>>2 (all real `sar`,
 * no magic multiply), values y>>1, y/6, -(y/6), -(y>>1) -- same `0x2aaaaaab`/div-6 value
 * idiom as StepTri6's inner bins. */
int CWaveformTemplate::EquationStepSaw4(int x, int y, int z)
{
	if (x < (z >> 2))
		return y >> 1;
	if (x < (z >> 1))
		return y / 6;
	if (x < ((z * 3) >> 2))
		return -(y / 6);
	return -(y >> 1);
}

/* 6-step quantized (descending) saw: same z/6, z/3, z>>1, (z*2)/3, (z*5)/6 thresholds as
 * StepTri6, but a genuinely different, evenly-spaced (by y/5 each) value ladder: y>>1,
 * 3y/10, y/10, -(y/10), -(3y/10), -(y>>1). The `y/10` value idiom is a NEW magic multiplier
 * for this class (`0x66666667`, confirmed div 5 at the ground truth's coded shift of 1, or
 * div 10 at shift 2 -- both occurrences here use the shift-2/div-10 form on `y` or `3*y`
 * directly, so those are written as plain `/10` below, matching the confirmed divisor
 * rather than re-deriving `/5` and re-scaling). */
int CWaveformTemplate::EquationStepSaw6(int x, int y, int z)
{
	if (x < (z / 6))
		return y >> 1;
	if (x < (z / 3))
		return (3 * y) / 10;
	if (x < (z >> 1))
		return y / 10;
	if (x < ((z * 2) / 3))
		return -(y / 10);
	if (x < ((z * 5) / 6))
		return -((3 * y) / 10);
	return -(y >> 1);
}

/* ---- 2026-07-28 follow-up batch: EquationRandomSH1-3 / EquationRandomCnt1-3 ----
 * See include/waveform_template.h for full provenance. All 6 regression-verified against
 * the x86-32 instruction-interpreter oracle (thousands of randomized (x,y,z), 0
 * mismatches). Every threshold below is a real ground-truth `trunc(k*(z-1)/D)` --
 * fractions have been reduced to lowest terms only where that reduction is mathematically
 * exact for truncating division (`trunc(a/b) == trunc((a*k)/(b*k))` for any k != 0, so
 * e.g. ground truth's literal `(12*y)/30` is written as `(2*y)/5` below), never
 * approximated. `z - 1` is computed once per call as `n`, matching ground truth's own
 * `sub ecx,0x1` on the raw `z` argument before any threshold work. */

/* Real ground truth: all magic-multiply thresholds here use hardcoded integer
 * coefficients baked into the instruction stream (0x2aaaaaab/0x55555556/0x88888889
 * constants), NOT a .rodata table -- confirmed by their absence from the disassembly's
 * relocation/data-reference column, unlike RandomCnt1-3 below. */
int CWaveformTemplate::EquationRandomSH1(int x, int y, int z)
{
	int n = z - 1;
	if (x < n / 6)
		return (7 * y) / 30;
	if (x < n / 3)
		return y / 2;
	if (x < (n >> 1))
		return -(y / 10);
	if (x < (2 * n) / 3)
		return -(y / 3);
	if (x < (5 * n) / 6)
		return (2 * y) / 5;
	if (x < n)
		return -(y / 2);
	return (2 * y) / 15;
}

int CWaveformTemplate::EquationRandomSH2(int x, int y, int z)
{
	int n = z - 1;
	if (x < n / 12)
		return (7 * y) / 30;
	if (x < (17 * n) / 60)
		return -(y / 10);
	if (x < n / 3)
		return y / 2;
	if (x < (29 * n) / 60)
		return y / 5;
	if (x < (3 * n) / 5)
		return y / 3;
	if (x < (2 * n) / 3)
		return -(y / 3);
	if (x < (5 * n) / 6)
		return y / 3;
	if (x < (19 * n) / 20)
		return y / 10;
	if (x < n)
		return y / 2;
	return -(y / 6);
}

/* Reduces to a plain alternating-sign y>>1/-(y>>1) ladder once decoded -- real ground
 * truth computes y>>1 via a genuine `sar`, not the truncating-division idiom, so it is
 * written as `y >> 1` here (floors for negative y), not `y / 2`. */
int CWaveformTemplate::EquationRandomSH3(int x, int y, int z)
{
	int n = z - 1;
	int half = y >> 1;
	if (x < n / 12)
		return half;
	if (x < (17 * n) / 60)
		return -half;
	if (x < n / 3)
		return half;
	if (x < (29 * n) / 60)
		return -half;
	if (x < (5 * n) / 6)
		return half;
	if (x < (19 * n) / 20)
		return -half;
	if (x < n)
		return half;
	return -half;
}

/* Shared LERP core for EquationRandomCnt1-3: real ground truth walks a small breakpoint
 * table (`coeff[]`, `nCoeff` real threshold coefficients, all thresholds expressed as
 * `trunc(coeff[i]*n/60)`) to pick a segment index, then linearly interpolates between
 * that segment's start/end values. `tableA[]`/`tableB[]` are the real per-segment
 * start/end VALUE tables (signed bytes, scaled in 30ths of `y`); `tableY[]` is the real
 * per-segment start POSITION table (unsigned byte, scaled in 60ths of `n` -- ground
 * truth's own `movzx`, genuinely zero-extended even though its idx-0 entry reads as a
 * large positive value in every instance, a real dead/defensive-only branch for x<0 not
 * exercised by any real caller, reproduced faithfully rather than "fixed").
 * `edgeNumer` is the real fixed `y`-fraction (in 30ths) ground truth returns for the
 * exact-last-sample (`x == n`) special case, which is NOT part of the table walk.
 * The final `(valB-valA)*(x-valY)` product uses a real 2-operand x86 `imul` that keeps
 * only the low 32 bits -- plain `int` arithmetic here reproduces that wraparound
 * automatically on this target, deliberately not widened to a larger type. */
static int RandomCntLerp(int x, int y, int n, const int *coeff, int nCoeff,
			  const signed char *tableA, const signed char *tableB,
			  const unsigned char *tableY, int edgeNumer)
{
	int idx, thr;

	if (x == n)
		return (edgeNumer * y) / 30;

	if (x < 0) {
		idx = 0;
		thr = 0;
	} else {
		idx = nCoeff + 1;
		thr = n;
		for (int i = 0; i < nCoeff; ++i) {
			int t = (coeff[i] * n) / 60;
			if (x < t) {
				idx = i + 1;
				thr = t;
				break;
			}
		}
	}

	int valY = (tableY[idx] * n) / 60;
	int valA = (tableA[idx] * y) / 30;
	int valB = (tableB[idx] * y) / 30;
	int numer = (valB - valA) * (x - valY);
	return valA + numer / (thr - valY);
}

int CWaveformTemplate::EquationRandomCnt1(int x, int y, int z)
{
	static const int kCoeff[5] = { 10, 20, 30, 40, 50 };
	static const signed char kA[7] = { 60, 13, -11, -1, -5, 6, 8 };
	static const signed char kB[7] = { 13, -11, -1, -5, 6, 8, -2 };
	static const unsigned char kY[7] = { 254, 0, 10, 20, 30, 40, 50 };
	return RandomCntLerp(x, y, z - 1, kCoeff, 5, kA, kB, kY, -2);
}

int CWaveformTemplate::EquationRandomCnt2(int x, int y, int z)
{
	static const int kCoeff[8] = { 5, 10, 25, 33, 37, 43, 50, 54 };
	static const signed char kA[10] = { 60, 7, 15, -14, 9, 6, -12, 4, -3, 15 };
	static const signed char kB[10] = { 7, 15, -14, 9, 6, -12, 4, -3, 15, -8 };
	static const unsigned char kY[10] = { 248, 0, 5, 10, 25, 33, 37, 43, 50, 54 };
	return RandomCntLerp(x, y, z - 1, kCoeff, 8, kA, kB, kY, -8);
}

int CWaveformTemplate::EquationRandomCnt3(int x, int y, int z)
{
	static const int kCoeff[6] = { 5, 17, 20, 29, 50, 57 };
	static const signed char kA[8] = { 0, 15, -15, 15, -15, 15, -15, 15 };
	static const signed char kB[8] = { 15, -15, 15, -15, 15, -15, 15, -15 };
	static const unsigned char kY[8] = { 241, 0, 5, 17, 20, 29, 50, 57 };
	return RandomCntLerp(x, y, z - 1, kCoeff, 6, kA, kB, kY, -15);
}
