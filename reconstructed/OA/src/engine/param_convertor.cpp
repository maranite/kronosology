// SPDX-License-Identifier: GPL-2.0
/*
 * param_convertor.cpp  -  USTGParamConvertor (batch 55), OA.ko's
 * front-panel-parameter <-> DSP-value conversion toolbox.
 *
 * Ground truth for the 20 Convertor/UnConvertor pairs and the 4
 * dispatchers (ConvertParam/UnConvertParam/GetTaperValue/
 * GetInverseTaperValue) was derived by careful `objdump -dr -M intel`
 * reading (x87 stack traced by hand instruction-by-instruction),
 * cross-checked internally for stack-balance and control-flow
 * consistency but NOT independently verified against a numeric
 * known-answer harness -- flag this tier as "careful reading,
 * unharnessed" if a future pass finds a discrepancy.
 *
 * Ground truth for the 23 pure single-float Taper/InverseTaper
 * functions (PiecewiseLinear1Taper through SeamlessHoldTimeTaper) was
 * instead derived DATA-FIRST: a throwaway `gcc -m32` harness mmaps the
 * real OA.ko .text bytes for this address range (`.text+0x114560`..
 * `.text+0x115aa0`) into executable memory, patches every R_386_32
 * relocation to `.rodata`/`.rodata.cst4`/`.rodata.cst8` so the
 * `fld DWORD PTR ds:0xNNN`-style operands resolve to real constant
 * data (mmap'd copies of those sections at chosen bases, addend +
 * base substituted for each patched dword), then calls each function
 * natively (real x86, no emulator) with ~90 sample inputs spanning
 * every breakpoint visible in the disassembly. The resulting
 * (input,output) tables were then fit to piecewise linear/reciprocal
 * segments using the ALREADY-EXTRACTED literal `.rodata.cst4`/
 * `.rodata.cst8` constants as candidate breakpoints/slopes, and every
 * segment boundary was cross-checked against at least 2 independent
 * sample points before being trusted. This caught several real bugs
 * in an initial hand-read pass (backwards `fucomi` comparison
 * direction on PiecewiseLinear1Taper; several "collapses to a single
 * branch for all negative x" ABI-adjacent quirks on the InverseTaper
 * family that a pure instruction-semantics read would have missed).
 *
 * Two real, faithfully-preserved quirks worth flagging up front:
 *   - Several forward Tapers are genuinely sign-symmetric
 *     (result = copysign(g(__builtin_fabsf(x)), x)) while their matching
 *     InverseTaper is NOT -- e.g.
 *     EffectLFOFreqDModIntensity1InverseTaper's negative-input branch
 *     always evaluates to the FIRST segment's formula regardless of
 *     magnitude, because the real comparison constant (2.0f) happens
 *     to be greater than every negative float, making the "else"
 *     branch of that specific `ja` unreachable for x<0. Reproduced
 *     verbatim per-function below, not "fixed" into a general
 *     symmetric wrapper.
 *   - Several tapers have genuinely DISCONTINUOUS segment boundaries
 *     (e.g. TaperKeyOnDelay jumps from 52 to 60 crossing x=26) --
 *     confirmed real via the harness, not a derivation artifact.
 *
 * TaperSmoothingFactor's real body calls `am_exp2_ess`
 * (`.text+0x244e6`) -- reconstructed for real too, see am_exp2_ess.cpp
 * (kept in its own TU: real ground truth compiles it with SSE
 * scalar-float codegen, matching this project's existing
 * `-mfpmath=sse` per-TU override precedent, e.g. lfo_tables.cpp).
 */

#include "oa_global.h"
#include "oa_engine.h"

namespace {

inline float AsFloat(int bits) { float f; __builtin_memcpy(&f, &bits, 4); return f; }
inline int AsInt(float f) { int i; __builtin_memcpy(&i, &f, 4); return i; }

/* fisttp-with-explicit-rounding-bias idiom used by several UnConvertors:
 * truncate-toward-zero after adding/subtracting 0.5 based on sign,
 * i.e. round-half-away-from-zero, matching the real
 * `jae L; fsub 0.5; fisttp; jmp done; L: fadd 0.5; fisttp` shape. */
inline int RoundHalfAwayFromZero(float v)
{
	return (int)(v >= 0.0f ? v + 0.5f : v - 0.5f);
}

} // namespace

/* ================= Convertor family (raw int -> tapered float/int) ===== */

/* .text+0x114560, 21 bytes. */
bool USTGParamConvertor::IntToPercentConvertor(float x, const CSTGParamDescriptor &, STGConvertedParam &out)
{
	float r = x * 0.01f;
	out.value = AsInt(r);
	out.displayValue = AsInt(r);
	return true;
}

/* .text+0x114580, 72 bytes. Real quirk: flat 0 below 4000, linear
 * *0.01 at/above -- not a "normal-looking" tempo curve, preserved as
 * read (this Convertor operates in the raw pre-taper integer domain,
 * so "x" here is not itself a BPM value). */
bool USTGParamConvertor::IntToTempoConvertor(float x, const CSTGParamDescriptor &, STGConvertedParam &out)
{
	float r = (x < 4000.0f) ? 0.0f : x * 0.01f;
	out.value = AsInt(r);
	out.displayValue = AsInt(r);
	return true;
}

/* .text+0x1145d0, 107 bytes. Linear raw-int-range -> float-range scale,
 * clamped to [min(floatMin,floatMax), max(floatMin,floatMax)]. */
bool USTGParamConvertor::IntToFloatConvertor(float x, const CSTGParamDescriptor &desc, STGConvertedParam &out)
{
	float ratio = (x - (float)desc.rawMin) / (float)(desc.rawMax - desc.rawMin);
	float r = desc.floatMin + ratio * (desc.floatMax - desc.floatMin);
	float lo = desc.floatMin < desc.floatMax ? desc.floatMin : desc.floatMax;
	float hi = desc.floatMin < desc.floatMax ? desc.floatMax : desc.floatMin;
	if (r < lo) r = lo;
	if (r > hi) r = hi;
	out.value = AsInt(r);
	out.displayValue = AsInt(r);
	return true;
}

/* .text+0x114640, 143 bytes. Same as IntToFloatConvertor but the ratio
 * is squared (sign-preserved) before scaling -- an "audio taper"-style
 * curve for a raw int range that can itself be negative. */
bool USTGParamConvertor::IntToFloatSquaredConvertor(float x, const CSTGParamDescriptor &desc, STGConvertedParam &out)
{
	float ratio = (x - (float)desc.rawMin) / (float)(desc.rawMax - desc.rawMin);
	float sq = (ratio < 0.0f) ? -(ratio * ratio) : (ratio * ratio);
	float r = desc.floatMin + sq * (desc.floatMax - desc.floatMin);
	float lo = desc.floatMin < desc.floatMax ? desc.floatMin : desc.floatMax;
	float hi = desc.floatMin < desc.floatMax ? desc.floatMax : desc.floatMin;
	if (r < lo) r = lo;
	if (r > hi) r = hi;
	out.value = AsInt(r);
	out.displayValue = AsInt(r);
	return true;
}

/* .text+0x1146d0, 61 bytes. x==0.0f is a dedicated sentinel (-2.0f);
 * otherwise (x-1)/63 - 1. */
bool USTGParamConvertor::IntToFloatPanRandomConvertor(float x, const CSTGParamDescriptor &, STGConvertedParam &out)
{
	float r = (x == 0.0f) ? -2.0f : (x - 1.0f) * (1.0f / 63.0f) - 1.0f;
	out.value = AsInt(r);
	out.displayValue = AsInt(r);
	return true;
}

/* .text+0x114710, 54 bytes. t=x-1; t<=0 unchanged, else t/126. */
bool USTGParamConvertor::IntToFloatPanControllerConvertor(float x, const CSTGParamDescriptor &, STGConvertedParam &out)
{
	float t = x - 1.0f;
	float r = (t <= 0.0f) ? t : t * (1.0f / 126.0f);
	out.value = AsInt(r);
	out.displayValue = AsInt(r);
	return true;
}

/* .text+0x114750, 29 bytes. Integer-domain only -- writes .value ONLY,
 * NOT .displayValue (confirmed: single `mov [edx],ecx`, no +0x18 store). */
bool USTGParamConvertor::IntCountFromZeroConvertor(float x, const CSTGParamDescriptor &desc, STGConvertedParam &out)
{
	out.value = (int)x - desc.rawMin;
	return true;
}

/* .text+0x114770, 9 bytes. Real quirk: reads only the LOW 16 BITS of
 * desc.rawMin (`movzx eax, WORD PTR [eax+0x1c]`) where every other
 * reader of rawMin treats it as a full 32-bit int -- faithfully
 * preserved, not "fixed" to a full read. */
int USTGParamConvertor::IntCountFromZeroUnConvertor(const CSTGParamDescriptor &desc, const STGConvertedParam &in)
{
	unsigned short rawMinLow16 = (unsigned short)desc.rawMin;
	return (int)rawMinLow16 + in.value;
}

/* .text+0x114780, 15 bytes. Pure bit-pattern pass-through -- no float
 * math at all, the argument's raw bits go straight into both slots. */
bool USTGParamConvertor::TaperOnlyConvertor(float x, const CSTGParamDescriptor &, STGConvertedParam &out)
{
	int bits = AsInt(x);
	out.value = bits;
	out.displayValue = bits;
	return true;
}

/* .text+0x114790, 12 bytes. Round x to nearest int (fisttp, current
 * rounding mode = round-nearest-even), .value ONLY. */
bool USTGParamConvertor::NoConversion(float x, const CSTGParamDescriptor &, STGConvertedParam &out)
{
	out.value = (int)__builtin_nearbyintf(x);
	return true;
}

/* .text+0x1147a0, 12 bytes. Byte-identical body to NoConversion --
 * confirmed real (both compile to the same round-to-int fallback). */
bool USTGParamConvertor::UnsupportedConversion(float x, const CSTGParamDescriptor &, STGConvertedParam &out)
{
	out.value = (int)__builtin_nearbyintf(x);
	return true;
}

/* .text+0x1147b0, 89 bytes. Inverse of IntToFloatConvertor's linear
 * scale, rounded half-away-from-zero and narrowed to a 16-bit unsigned
 * value (`fisttp WORD`, then `movzx`). */
int USTGParamConvertor::IntToFloatUnConvertor(const CSTGParamDescriptor &desc, const STGConvertedParam &in)
{
	float v = AsFloat(in.value);
	float ratio = (v - desc.floatMin) / (desc.floatMax - desc.floatMin);
	float r = (float)desc.rawMin + ratio * (float)(desc.rawMax - desc.rawMin);
	int rounded = RoundHalfAwayFromZero(r);
	return (unsigned short)(short)rounded;
}

/* .text+0x114810, 74 bytes. Inverse of IntToFloatSquaredConvertor:
 * sign-preserved sqrt of the (already-int-reinterpreted) value, then
 * the same linear rescale, rounded to nearest (fistp, NOT fisttp --
 * current rounding mode, not truncate-with-bias like the sibling
 * above) and narrowed to 16 bits. */
int USTGParamConvertor::IntToFloatSquaredUnConvertor(const CSTGParamDescriptor &desc, const STGConvertedParam &in)
{
	float v = AsFloat(in.value);
	float sq = __builtin_copysignf(__builtin_sqrtf(__builtin_fabsf(v)), v);
	float ratio = (sq - desc.floatMin) / (desc.floatMax - desc.floatMin);
	float r = (float)desc.rawMin + ratio * (float)(desc.rawMax - desc.rawMin);
	int rounded = (int)__builtin_nearbyintf(r);
	return (unsigned short)rounded;
}

/* .text+0x114860, 81 bytes. Dispatches through mInverseTaperFunc (index
 * 0 == NoTaper when convertType==19, else desc.taperType -- the same
 * "unsupported-conversion sentinel forces taper index 0" idiom
 * ConvertParam/UnConvertParam use), then rounds half-away-from-zero
 * to a 16-bit value. */
int USTGParamConvertor::TaperOnlyUnConvertor(const CSTGParamDescriptor &desc, const STGConvertedParam &in)
{
	int idx = (desc.convertType == 0x13) ? 0 : desc.taperType;
	float tapered = mInverseTaperFunc[idx](AsFloat(in.value));
	int rounded = RoundHalfAwayFromZero(tapered);
	return (unsigned short)(short)rounded;
}

/* .text+0x1148c0, 59 bytes. v==0.0f is a dedicated sentinel (3999);
 * otherwise round-half-UP (unconditional +0.5, not sign-aware) of
 * v*100, narrowed to 16 bits. */
int USTGParamConvertor::IntToTempoUnConvertor(const CSTGParamDescriptor &, const STGConvertedParam &in)
{
	float v = AsFloat(in.value);
	if (v == 0.0f)
		return 3999;
	int rounded = (int)(v * 100.0f + 0.5f);
	return (unsigned short)(short)rounded;
}

/* .text+0x114900, 67 bytes. v==-2.0f is a dedicated sentinel (0);
 * otherwise round-half-up of (v+1)*63, narrowed to 16 bits, then +1
 * (the +1 happens AFTER narrowing, confirmed via instruction order). */
int USTGParamConvertor::IntToFloatPanRandomUnConvertor(const CSTGParamDescriptor &, const STGConvertedParam &in)
{
	float v = AsFloat(in.value);
	if (v == -2.0f)
		return 0;
	int rounded = (int)((v + 1.0f) * 63.0f + 0.5f);
	return (int)(unsigned short)(short)rounded + 1;
}

/* .text+0x114950, 3 bytes. */
int USTGParamConvertor::NoUnConversion(const CSTGParamDescriptor &, const STGConvertedParam &in)
{
	return in.value;
}

/* .text+0x114960, 3 bytes. Byte-identical to NoUnConversion. */
int USTGParamConvertor::UnsupportedUnConversion(const CSTGParamDescriptor &, const STGConvertedParam &in)
{
	return in.value;
}

/* .text+0x114970, 97 bytes. t=x-1; pan=(t<=0)?t:t/126; calls the
 * already-real CSTGPan::CalculateMonoPanCoeffs(coeffs,1.0f,pan) and
 * writes coeff0 to out.value as usual BUT writes coeff4 to out+0x04 --
 * a confirmed real quirk (every other Convertor here mirrors to
 * +0x18, this one writes a DIFFERENT offset entirely). */
bool USTGParamConvertor::IntToFloatPanConvertor(float x, const CSTGParamDescriptor &, STGConvertedParam &out)
{
	float t = x - 1.0f;
	float pan = (t <= 0.0f) ? t : t * (1.0f / 126.0f);
	STGMonoPanCoeffs coeffs;
	CSTGPan::CalculateMonoPanCoeffs(coeffs, 1.0f, pan);
	out.value = AsInt(coeffs.coeff0);
	*(int *)((char *)&out + 4) = AsInt(coeffs.coeff4);
	return true;
}

/* ============================ Dispatchers =============================== */

/* .text+0x1149e0, 110 bytes. Clamp rawValue to [rawMin,rawMax], stash
 * the clamped value at out+0x10 (STGConvertedParam::clampedRaw), taper
 * it (index 0 == NoTaper when convertType==19, else taperType), then
 * hand off to mConvertFunc[convertType]. */
bool USTGParamConvertor::ConvertParam(int rawValue, const CSTGParamDescriptor &desc, STGConvertedParam &out)
{
	int clamped = rawValue;
	if (clamped > desc.rawMax) clamped = desc.rawMax;
	if (clamped < desc.rawMin) clamped = desc.rawMin;
	out.clampedRaw = clamped;

	int taperIdx = (desc.convertType == 0x13) ? 0 : desc.taperType;
	float tapered = mTaperFunc[taperIdx]((float)clamped);
	return mConvertFunc[desc.convertType](tapered, desc, out);
}

/* .text+0x114a50, 25 bytes. */
float USTGParamConvertor::GetTaperValue(float x, eSTGTaper taper)
{
	return mTaperFunc[taper](x);
}

/* .text+0x114a70, 54 bytes. Dispatch through mUnConvertFunc, normalize
 * to the low 16 bits sign-extended (`cwde` after the call -- every
 * UnConvertor above already narrows internally, this just makes the
 * normalization explicit/uniform at the dispatcher), then clamp to
 * [rawMin,rawMax]. */
int USTGParamConvertor::UnConvertParam(const STGConvertedParam &in, const CSTGParamDescriptor &desc)
{
	int raw = mUnConvertFunc[desc.convertType](desc, in);
	raw = (short)raw;
	if (raw < desc.rawMin) raw = desc.rawMin;
	if (raw > desc.rawMax) raw = desc.rawMax;
	return raw;
}

/* .text+0x114ab0, 25 bytes. */
float USTGParamConvertor::GetInverseTaperValue(float x, eSTGTaper taper)
{
	return mInverseTaperFunc[taper](x);
}

/* ====================== Taper curve family =============================
 * Every formula below is DATA-DERIVED (see file header) against the
 * real .ko machine code, not hand-simulated from the disassembly.
 * .text addresses/sizes cited per function for cross-reference. */

/* .text+0x114ad0, 3 bytes. */
float USTGParamConvertor::UnsupportedTaper(float) { return 0.0f; }

/* .text+0x114ae0, 5 bytes. */
float USTGParamConvertor::NoTaper(float x) { return x; }

/* .text+0x114af0, 82 bytes. */
float USTGParamConvertor::PiecewiseLinear1Taper(float x)
{
	if (x < 91.0f)
		return 10.0f / (x + 10.0f);
	if (x >= 131.0f)
		return 0.0f;
	return 1.0f / (x - 80.0f);
}

/* .text+0x114b50, 144 bytes. */
float USTGParamConvertor::EffectLFOFreq1Taper(float x)
{
	if (x < 100.0f) return 0.02f * x;
	if (x < 160.0f) return 2.0f + 0.05f * (x - 100.0f);
	if (x < 210.0f) return 5.0f + 0.1f * (x - 160.0f);
	return 10.0f + 0.5f * (x - 210.0f);
}

/* .text+0x114be0, 118 bytes. */
float USTGParamConvertor::EffectLFOFreq1InverseTaper(float x)
{
	if (x < 2.0f)  return 50.0f * x;
	if (x < 5.0f)  return 100.0f + 20.0f * (x - 2.0f);
	if (x < 10.0f) return 160.0f + 10.0f * (x - 5.0f);
	return 210.0f + 2.0f * (x - 10.0f);
}

/* .text+0x114c60, 182 bytes. Sign-symmetric: g(|x|), sign re-applied. */
float USTGParamConvertor::EffectLFOFreqDModIntensity1Taper(float x)
{
	float ax = __builtin_fabsf(x);
	float g;
	if (ax < 50.0f)       g = 0.04f * ax;
	else if (ax < 80.0f)  g = 2.0f + 0.1f * (ax - 50.0f);
	else if (ax < 105.0f) g = 5.0f + 0.2f * (ax - 80.0f);
	else                  g = 10.0f + 1.0f * (ax - 105.0f);
	return __builtin_copysignf(g, x);
}

/* .text+0x114d20, 182 bytes. Real quirk: the negative-input branch's
 * own comparison constant (2.0f) is always satisfied for any x<0, so
 * it collapses to a SINGLE formula (25*|x|) regardless of magnitude --
 * NOT a full symmetric extension of the positive-side piecewise curve. */
float USTGParamConvertor::EffectLFOFreqDModIntensity1InverseTaper(float x)
{
	if (x < 2.0f)  return 25.0f * __builtin_fabsf(x);
	if (x < 5.0f)  return 50.0f + 10.0f * (x - 2.0f);
	if (x < 10.0f) return 80.0f + 5.0f * (x - 5.0f);
	return 105.0f + (x - 10.0f);
}

/* .text+0x114de0, 128 bytes. */
float USTGParamConvertor::EffectLFOFreq2Taper(float x)
{
	if (x < 100.0f) return 0.05f * x;
	if (x < 150.0f) return 5.0f + 0.1f * (x - 100.0f);
	if (x < 170.0f) return 10.0f + 0.5f * (x - 150.0f);
	return 20.0f + 1.0f * (x - 170.0f);
}

/* .text+0x114e70, 176 bytes. Sign-symmetric like DModIntensity1Taper
 * (no InverseTaper counterpart exists -- mInverseTaperFunc[5] ==
 * UnsupportedTaper). */
float USTGParamConvertor::EffectLFOFreqDModIntensity2Taper(float x)
{
	float ax = __builtin_fabsf(x);
	float g;
	if (ax < 50.0f)      g = 0.1f * ax;
	else if (ax < 75.0f) g = 5.0f + 0.2f * (ax - 50.0f);
	else if (ax < 85.0f) g = 10.0f + 1.0f * (ax - 75.0f);
	else                 g = 20.0f + 2.0f * (ax - 85.0f);
	return __builtin_copysignf(g, x);
}

/* .text+0x114f20, 58 bytes. */
float USTGParamConvertor::EffectDelayTime1Taper(float x)
{
	if (x < 100.0f) return 0.1f * x;
	return 10.0f + (x - 100.0f);
}

/* .text+0x114f60, 98 bytes. */
float USTGParamConvertor::EffectDelayTime2Taper(float x)
{
	if (x < 50.0f) return 0.1f * x;
	if (x < 75.0f) return 5.0f + 0.2f * (x - 50.0f);
	return 10.0f + 1.0f * (x - 75.0f);
}

/* .text+0x114fd0, 44 bytes. */
float USTGParamConvertor::EffectDelayTime3Taper(float x)
{
	if (x < 100.0f) return x;
	return 100.0f + 10.0f * (x - 100.0f);
}

/* .text+0x115000, 146 bytes. Rational (division-based, not
 * multiply-based) piecewise curve -- three ratio segments, breakpoints
 * 40/80/101. */
float USTGParamConvertor::EffectDelayModDepth1Taper(float x)
{
	if (x < 40.0f)  return (18.0f * x) / 336.0f;
	if (x < 80.0f)  return (36.0f * x - 721.0f) / 336.0f;
	if (x < 101.0f) return (144.0f * x - 9359.0f) / 336.0f;
	return (792.0f * x - 72000.0f) / 480.0f;
}

/* .text+0x1150a0, 128 bytes. Sign-symmetric, breakpoints 20/118. */
float USTGParamConvertor::EffectFixedFreq1Taper(float x)
{
	float ax = __builtin_fabsf(x);
	float g;
	if (ax < 20.0f)       g = ax;
	else if (ax < 118.0f) g = 10.0f * ax - 180.0f;
	else                  g = 100.0f * ax - 10800.0f;
	return __builtin_copysignf(g, x);
}

/* .text+0x115120, 136 bytes. Sign-symmetric, breakpoints 10/20/60. */
float USTGParamConvertor::EffectFixedFreqDModIntensity1Taper(float x)
{
	float ax = __builtin_fabsf(x);
	float g;
	if (ax < 10.0f)      g = 2.0f * ax;
	else if (ax < 20.0f) g = 20.0f * ax - 180.0f;
	else if (ax < 60.0f) g = 220.0f + 20.0f * (ax - 20.0f);
	else                 g = 1200.0f + 200.0f * (ax - 60.0f);
	return __builtin_copysignf(g, x);
}

/* .text+0x1151b0, 96 bytes. NOT sign-symmetric -- the "x<50" first
 * comparison is naturally satisfied by any negative x, so it extends
 * directly rather than needing a fabsf/copysignf wrapper. */
float USTGParamConvertor::EffectEQFreq1Taper(float x)
{
	if (x < 50.0f)  return 20.0f * x;
	if (x < 230.0f) return 1000.0f + 50.0f * (x - 50.0f);
	return 10000.0f + 100.0f * (x - 230.0f);
}

/* .text+0x115210, 86 bytes. Clamped at the top (max 10100 at x>=146),
 * not sign-symmetric (direct extension for x<50). */
float USTGParamConvertor::WaveSeqDuration1Taper(float x)
{
	if (x < 50.0f)  return 10.0f * x;
	if (x < 146.0f) return 500.0f + 100.0f * (x - 50.0f);
	return 10100.0f;
}

/* .text+0x115270, 88 bytes. Inverse of WaveSeqDuration1Taper, clamped
 * at the top (146 for x>=10100). */
float USTGParamConvertor::InverseWaveSeqDuration1Taper(float x)
{
	if (x < 500.0f)   return 0.1f * x;
	if (x < 10100.0f) return 50.0f + (x - 500.0f) / 100.0f;
	return 146.0f;
}

/* .text+0x1152d0, 162 bytes. Genuinely discontinuous at x=26 and x=41
 * (confirmed real via the harness, not a derivation artifact); clamped
 * to a max of 5001 for x>=56. */
float USTGParamConvertor::TaperKeyOnDelay(float x)
{
	if (x < 26.0f) return 2.0f * x;
	if (x < 41.0f) return 60.0f + 10.0f * (x - 26.0f);
	if (x < 56.0f) return 250.0f + 50.0f * (x - 41.0f);
	float v = 1000.0f + 100.0f * (x - 56.0f);
	return v > 5001.0f ? 5001.0f : v;
}

/* .text+0x115380, 162 bytes. Inverse of TaperKeyOnDelay; clamped to
 * 97.0f (the real param max) for x>=5001 -- the duration range
 * [96.01,5001) forward maps entirely onto param 96.01..97ish via the
 * flat clamp, so this is a genuine one-way saturation, not a
 * derivation gap. */
float USTGParamConvertor::InverseTaperKeyOnDelay(float x)
{
	if (x < 60.0f)   return 0.5f * x;
	if (x < 200.0f)  return 26.0f + 0.1f * (x - 60.0f);
	if (x < 1000.0f) return 40.0f + 0.02f * (x - 200.0f);
	if (x < 5001.0f) return 56.0f + 0.01f * (x - 1000.0f);
	return 97.0f;
}

/* .text+0x115430, 102 bytes. Same shape as WaveSeqDuration1Taper but
 * NO top clamp -- the 3rd segment (x>=145) grows unbounded. */
float USTGParamConvertor::VectorDurationTaper(float x)
{
	if (x < 50.0f)  return 10.0f * x;
	if (x < 145.0f) return 500.0f + 100.0f * (x - 50.0f);
	return 10000.0f + 1000.0f * (x - 145.0f);
}

/* .text+0x1154a0, 98 bytes. Inverse of VectorDurationTaper, no clamp. */
float USTGParamConvertor::VectorDurationInverseTaper(float x)
{
	if (x < 500.0f)   return 0.1f * x;
	if (x < 10000.0f) return 50.0f + (x - 500.0f) / 100.0f;
	return 145.0f + 0.001f * (x - 10000.0f);
}

/* .text+0x115510, 268 bytes. Sign-symmetric, breakpoints
 * 50/60/105/116 (the [60,105) and what looked like a separate [60,65)
 * segment during hand-reading turned out to be the SAME slope --
 * confirmed via the harness, only 4 real segments). */
float USTGParamConvertor::PitchSemitoneTaper(float x)
{
	float ax = __builtin_fabsf(x);
	float g;
	if (ax < 50.0f)       g = 0.01f * ax;
	else if (ax < 60.0f)  g = 0.5f + 0.05f * (ax - 50.0f);
	else if (ax < 105.0f) g = 1.0f + 0.2f * (ax - 60.0f);
	else if (ax < 116.0f) g = 10.0f + 0.2f * (ax - 105.0f);
	else                  g = 13.0f + 1.0f * (ax - 116.0f);
	return __builtin_copysignf(g, x);
}

/* .text+0x115620, 298 bytes. Inverse of PitchSemitoneTaper,
 * sign-symmetric, breakpoints 0.5/1.0/10.0/13.0 (the y-values of the
 * forward function's own breakpoints). */
float USTGParamConvertor::InversePitchSemitoneTaper(float x)
{
	float ax = __builtin_fabsf(x);
	float g;
	if (ax < 0.5f)      g = 100.0f * ax;
	else if (ax < 1.0f) g = 50.0f + 20.0f * (ax - 0.5f);
	else if (ax < 10.0f) g = 60.0f + 5.0f * (ax - 1.0f);
	else if (ax < 13.0f) g = 105.0f + 5.0f * (ax - 10.0f);
	else                 g = 116.0f + 1.0f * (ax - 13.0f);
	return __builtin_copysignf(g, x);
}

/* .text+0x115750, 136 bytes. Not sign-symmetric (direct extension). */
float USTGParamConvertor::EffectCompAttackTime1Taper(float x)
{
	if (x < 20.0f)  return 0.05f * x;
	if (x < 110.0f) return 1.0f + 0.1f * (x - 20.0f);
	if (x < 210.0f) return 10.0f + 1.0f * (x - 110.0f);
	return 200.0f + 10.0f * (x - 210.0f);
}

/* .text+0x1157e0, 80 bytes. Not sign-symmetric; identity below 210,
 * with a genuine discontinuity there (200 vs identity's own 210). */
float USTGParamConvertor::EffectCompReleaseTime1Taper(float x)
{
	if (x < 210.0f) return x;
	if (x < 280.0f) return 300.0f + 10.0f * (x - 210.0f);
	return 1000.0f + 100.0f * (x - 280.0f);
}

/* .text+0x115830, 144 bytes. Not sign-symmetric; y-intercept 100.0f
 * (this taper's raw-int domain is offset, not centered at zero). */
float USTGParamConvertor::TaperEQMidFreq(float x)
{
	if (x < 40.0f) return 100.0f + 10.0f * x;
	if (x < 65.0f) return 500.0f + 20.0f * (x - 40.0f);
	if (x < 85.0f) return 1000.0f + 50.0f * (x - 65.0f);
	return 2000.0f + 100.0f * (x - 85.0f);
}

/* .text+0x1158c0, 144 bytes. Inverse of TaperEQMidFreq. */
float USTGParamConvertor::InverseTaperEQMidFreq(float x)
{
	if (x < 500.0f)  return (x - 100.0f) / 10.0f;
	if (x < 1000.0f) return 40.0f + (x - 500.0f) / 20.0f;
	if (x < 2000.0f) return 65.0f + (x - 1000.0f) / 50.0f;
	return 85.0f + (x - 2000.0f) / 100.0f;
}

/* .text+0x115950, 36 bytes. A 5-entry LUT {1,4,16,64,256} = 4^i,
 * indexed by round(x) clamped to [0,4] (unsigned compare after
 * `fistp`, so negative x also falls through to the "out of range"
 * default of 1.0f). Real table read from `.rodata+0x4be2c` via the
 * m32 harness, not guessed. */
float USTGParamConvertor::TaperHeadroom(float x)
{
	static const float kTable[5] = { 1.0f, 4.0f, 16.0f, 64.0f, 256.0f };
	long idx = __builtin_lrintf(x);
	if ((unsigned long)idx > 4)
		return 1.0f;
	return kTable[idx];
}

/* .text+0x115980, 38 bytes. -100 * log2(x) / 8.25 (`fyl2x` for the
 * log2, matching cst4 0x4cc=8.25 seen in TaperSmoothingFactor's own
 * body -- the two are a real inverse pair). NaN for x<0, +inf for
 * x==0, matching real log2f semantics -- not special-cased. */
float USTGParamConvertor::InverseTaperSmoothingFactor(float x)
{
	return -100.0f * __builtin_log2f(x) * (1.0f / 8.25f);
}

/* .text+0x1159b0, 166 bytes. Not sign-symmetric; breakpoints
 * 3/16/17/21 with two real discontinuities (at x=3 and x=16/17). */
float USTGParamConvertor::SeamlessHoldTimeTaper(float x)
{
	if (x < 3.0f)  return 0.5f * x;
	if (x < 16.0f) return 2.0f + 1.0f * (x - 3.0f);
	if (x < 17.0f) return 20.0f + 2.0f * (x - 16.0f);
	if (x < 21.0f) return 25.0f + 5.0f * (x - 17.0f);
	return 50.0f + 10.0f * (x - 21.0f);
}

/* .text+0x115a60, 62 bytes. am_exp2_ess(-x/100*8.25) -- the exact
 * algebraic inverse of InverseTaperSmoothingFactor above (verified:
 * InverseTaperSmoothingFactor(TaperSmoothingFactor(x)) == x). */
float USTGParamConvertor::TaperSmoothingFactor(float x)
{
	return am_exp2_ess((-x / 100.0f) * 8.25f);
}

/* ===================== Real static dispatch tables =======================
 * Order confirmed by dumping the real `.rel.data` entries for each
 * table (readelf -r, section .data, offsets 0x3080/0x30e0/0x3140/
 * 0x31a0 relative to `.data`'s own base) -- not inferred from the
 * enum, the enum was assigned TO match this real order. */
const USTGParamConvertor::ConvertFn USTGParamConvertor::mConvertFunc[20] = {
	/*  0 */ UnsupportedConversion,
	/*  1 */ IntToFloatConvertor,
	/*  2 */ IntToFloatSquaredConvertor,
	/*  3 */ IntToFloatPanConvertor,
	/*  4 */ UnsupportedConversion,
	/*  5 */ UnsupportedConversion,
	/*  6 */ UnsupportedConversion,
	/*  7 */ UnsupportedConversion,
	/*  8 */ UnsupportedConversion,
	/*  9 */ TaperOnlyConvertor,
	/* 10 */ IntToPercentConvertor,
	/* 11 */ IntToTempoConvertor,
	/* 12 */ NoConversion,
	/* 13 */ NoConversion,
	/* 14 */ IntToFloatPanRandomConvertor,
	/* 15 */ IntToFloatPanControllerConvertor,
	/* 16 */ NoConversion,
	/* 17 */ NoConversion,
	/* 18 */ IntCountFromZeroConvertor,
	/* 19 */ NoConversion,
};

const USTGParamConvertor::UnConvertFn USTGParamConvertor::mUnConvertFunc[20] = {
	/*  0 */ UnsupportedUnConversion,
	/*  1 */ IntToFloatUnConvertor,
	/*  2 */ IntToFloatSquaredUnConvertor,
	/*  3 */ UnsupportedUnConversion,
	/*  4 */ UnsupportedUnConversion,
	/*  5 */ UnsupportedUnConversion,
	/*  6 */ UnsupportedUnConversion,
	/*  7 */ UnsupportedUnConversion,
	/*  8 */ UnsupportedUnConversion,
	/*  9 */ TaperOnlyUnConvertor,
	/* 10 */ UnsupportedUnConversion,
	/* 11 */ IntToTempoUnConvertor,
	/* 12 */ NoUnConversion,
	/* 13 */ NoUnConversion,
	/* 14 */ IntToFloatPanRandomUnConvertor,
	/* 15 */ NoUnConversion,
	/* 16 */ NoUnConversion,
	/* 17 */ NoUnConversion,
	/* 18 */ IntCountFromZeroUnConvertor,
	/* 19 */ NoUnConversion,
};

const USTGParamConvertor::TaperFn USTGParamConvertor::mTaperFunc[23] = {
	/*  0 */ NoTaper,
	/*  1 */ PiecewiseLinear1Taper,
	/*  2 */ EffectLFOFreq1Taper,
	/*  3 */ EffectLFOFreqDModIntensity1Taper,
	/*  4 */ EffectLFOFreq2Taper,
	/*  5 */ EffectLFOFreqDModIntensity2Taper,
	/*  6 */ EffectDelayTime1Taper,
	/*  7 */ EffectDelayTime2Taper,
	/*  8 */ EffectDelayTime3Taper,
	/*  9 */ EffectDelayModDepth1Taper,
	/* 10 */ EffectFixedFreq1Taper,
	/* 11 */ EffectFixedFreqDModIntensity1Taper,
	/* 12 */ EffectEQFreq1Taper,
	/* 13 */ WaveSeqDuration1Taper,
	/* 14 */ TaperKeyOnDelay,
	/* 15 */ VectorDurationTaper,
	/* 16 */ PitchSemitoneTaper,
	/* 17 */ EffectCompAttackTime1Taper,
	/* 18 */ EffectCompReleaseTime1Taper,
	/* 19 */ TaperEQMidFreq,
	/* 20 */ TaperHeadroom,
	/* 21 */ TaperSmoothingFactor,
	/* 22 */ SeamlessHoldTimeTaper,
};

const USTGParamConvertor::TaperFn USTGParamConvertor::mInverseTaperFunc[23] = {
	/*  0 */ NoTaper,
	/*  1 */ UnsupportedTaper,
	/*  2 */ EffectLFOFreq1InverseTaper,
	/*  3 */ EffectLFOFreqDModIntensity1InverseTaper,
	/*  4 */ UnsupportedTaper,
	/*  5 */ UnsupportedTaper,
	/*  6 */ UnsupportedTaper,
	/*  7 */ UnsupportedTaper,
	/*  8 */ UnsupportedTaper,
	/*  9 */ UnsupportedTaper,
	/* 10 */ UnsupportedTaper,
	/* 11 */ UnsupportedTaper,
	/* 12 */ UnsupportedTaper,
	/* 13 */ InverseWaveSeqDuration1Taper,
	/* 14 */ InverseTaperKeyOnDelay,
	/* 15 */ VectorDurationInverseTaper,
	/* 16 */ InversePitchSemitoneTaper,
	/* 17 */ UnsupportedTaper,
	/* 18 */ UnsupportedTaper,
	/* 19 */ InverseTaperEQMidFreq,
	/* 20 */ UnsupportedTaper,
	/* 21 */ InverseTaperSmoothingFactor,
	/* 22 */ UnsupportedTaper,
};
