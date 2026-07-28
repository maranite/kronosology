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
