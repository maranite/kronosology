/*
 * pcm_filter.cpp  -  see include/pcm_filter.h for full ground-truth
 * provenance and the collapse-from-unrolled-loop convention every method
 * below follows.
 *
 * IntToFloat()/FloatToInt() NOTE: their real x87 bodies use an `fcmovnbe`
 * conditional move for the upper-bound clamp, which Ghidra's C decompile
 * renders as a confusing (and, read literally, tautological -- `1.0 <= x ||
 * x != 1.0` is true for every float, including NaN) boolean expression.
 * Read the RAW disassembly instead (`objdump -d -C`) to resolve it: the real
 * sequence is `fucomi`/`ja` (branch to the -1.0 default when x < -1.0) then,
 * on the fallthrough, `fucomi`/`fcmovnbe` (conditionally move the upper
 * bound in when x > upperBound) -- i.e. an ordinary 3-way clamp, confirmed
 * by cross-checking against `ClipAndGetPeakLevels()`'s OWN plain nested-if
 * decompile of the exact same clamp shape (same bounds, unambiguous Ghidra
 * rendering there). Implemented here as a plain clamp, not the confusing
 * literal expression.
 */

#include "pcm_filter.h"

#include <cmath>
#include <cstring>

namespace {
inline float Clamp(float v, float lo, float hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}
} // namespace

CPcmFilter::CPcmFilter(int bitsPerSample)
    : mBits(0), mIntToFloatScale(0.0f), mFullScale(0.0f), mClampMax(0.0f), mClampMin(0.0f)
{
	SetBitsPerSample(bitsPerSample);
}

CPcmFilter::~CPcmFilter()
{
}

void CPcmFilter::SetBitsPerSample(int bitsPerSample)
{
	mBits = bitsPerSample;
	int fullScale = 1 << (bitsPerSample - 1);
	mIntToFloatScale = 1.0f / (float)fullScale;
	mFullScale = (float)fullScale;
	mClampMax = (float)(fullScale - 1);
	mClampMin = -(float)fullScale;
}

void CPcmFilter::IntToFloat(long **in, float **out, unsigned long count, int channels) const
{
	for (int ch = 0; ch < channels; ++ch) {
		const long *src = in[ch];
		float *dst = out[ch];
		for (unsigned long i = 0; i < count; ++i)
			dst[i] = Clamp((float)src[i] * mIntToFloatScale, -1.0f, 1.0f);
	}
}

void CPcmFilter::FloatToInt(float **in, long **out, unsigned long count, int channels) const
{
	for (int ch = 0; ch < channels; ++ch) {
		const float *src = in[ch];
		long *dst = out[ch];
		for (unsigned long i = 0; i < count; ++i)
			dst[i] = (long)Clamp(src[i] * mFullScale, mClampMin, mClampMax);
	}
}

unsigned long CPcmFilter::BitShift(long **in, long **out, unsigned long count,
                                     int channels, int shift)
{
	for (int ch = 0; ch < channels; ++ch) {
		const int *src = (const int *)in[ch];
		int *dst = (int *)out[ch];
		for (unsigned long i = 0; i < count; ++i)
			dst[i] = (shift < 0) ? (src[i] >> -shift) : (src[i] << shift);
	}
	return count;
}

unsigned long CPcmFilter::Copy(float **in, float **out, unsigned long count, int channels)
{
	for (int ch = 0; ch < channels; ++ch)
		std::memmove(out[ch], in[ch], count * sizeof(float));
	return count;
}

unsigned long CPcmFilter::Reverse(float **in, float **out, unsigned long count, int channels)
{
	for (int ch = 0; ch < channels; ++ch) {
		const float *src = in[ch];
		float *dst = out[ch];
		for (unsigned long i = 0; i < count; ++i)
			dst[i] = src[count - 1 - i];
	}
	return count;
}

unsigned long CPcmFilter::Fade(float **in, float **out, unsigned long count,
                                 int channels, int direction)
{
	if (count == 0)
		return 0;

	if (direction != 1 && direction != 2) {
		/* Real ground truth: no fade at all, plain per-channel copy. */
		for (int ch = 0; ch < channels; ++ch)
			std::memmove(out[ch], in[ch], count * sizeof(float));
		return count;
	}

	float step, gain;
	if (direction == 1) { /* fade in: 0.0 -> 1.0 */
		step = 1.0f / (float)count;
		gain = 0.0f;
	} else { /* fade out: (1.0 - step) -> 0.0 */
		step = -1.0f / (float)count;
		gain = step + 1.0f;
	}

	for (unsigned long i = 0; i < count; ++i) {
		for (int ch = 0; ch < channels; ++ch)
			out[ch][i] = in[ch][i] * gain;
		gain += step;
		if (gain < 0.0f)
			gain = 0.0f;
		else if (gain > 1.0f)
			gain = 1.0f;
	}
	return count;
}

bool CPcmFilter::IsAlwaysBelow(float **in, unsigned long count, int channels, float threshold)
{
	for (int ch = 0; ch < channels; ++ch) {
		const float *src = in[ch];
		for (unsigned long i = 0; i < count; ++i) {
			float a = std::fabs(src[i]);
			if (a > threshold)
				return false;
		}
	}
	return true;
}

bool CPcmFilter::IsSilent(float **in, unsigned long count, int channels)
{
	for (int ch = 0; ch < channels; ++ch) {
		const float *src = in[ch];
		for (unsigned long i = 0; i < count; ++i) {
			if (src[i] != 0.0f)
				return false;
		}
	}
	return true;
}

double CPcmFilter::GetMaximumAbsValue(float **in, unsigned long count, int channels)
{
	double m = 0.0;
	for (int ch = 0; ch < channels; ++ch) {
		const float *src = in[ch];
		for (unsigned long i = 0; i < count; ++i) {
			double a = std::fabs((double)src[i]);
			if (a > m)
				m = a;
		}
	}
	return m;
}

void CPcmFilter::ClipAndGetPeakLevels(float **in, float **out, float *peakLevels,
                                         unsigned long count, int channels)
{
	for (int ch = 0; ch < channels; ++ch) {
		const float *src = in[ch];
		float *dst = out[ch];
		float peak = 0.0f;
		for (unsigned long i = 0; i < count; ++i) {
			float clipped = Clamp(src[i], -1.0f, 1.0f);
			dst[i] = clipped;
			float a = std::fabs(clipped);
			if (a > peak)
				peak = a;
		}
		peakLevels[ch] = (count != 0) ? peak : 0.0f;
	}
}

void CPcmFilter::GetPeakLevels(float **in, float *peakLevels, unsigned long count, int channels)
{
	for (int ch = 0; ch < channels; ++ch) {
		const float *src = in[ch];
		float peak = 0.0f;
		for (unsigned long i = 0; i < count; ++i) {
			float a = std::fabs(src[i]);
			if (a > peak)
				peak = a;
		}
		peakLevels[ch] = peak;
	}
}

unsigned long CPcmFilter::Mute(float **buf, unsigned long count, unsigned long channels)
{
	for (unsigned long ch = 0; ch < channels; ++ch)
		std::memset(buf[ch], 0, count * sizeof(float));
	return count;
}
