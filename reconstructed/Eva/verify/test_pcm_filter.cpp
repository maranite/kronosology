/*
 * test_pcm_filter.cpp  -  host-side known-answer test for CPcmFilter
 * (src/base/pcm_filter.cpp). See include/pcm_filter.h for full ground-truth
 * provenance, including the IntToFloat()/FloatToInt() clamp-semantics note
 * (resolved via raw x87 disassembly, not Ghidra's confusing literal
 * decompile of the `fcmovnbe` clamp).
 *
 * No CSystemApi dependency -- this class makes no Api/HAL calls.
 */

#include <cmath>
#include <cstdio>

#include "pcm_filter.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CPcmFilter known-answer test\n");

	/* --- instance methods: 16-bit PCM <-> float --- */
	CPcmFilter f16(16);
	{
		long inSamples[4] = { 0, 32767, -32768, 40000 /* out of range, clamps */ };
		long *inCh[1] = { inSamples };
		float outSamples[4];
		float *outCh[1] = { outSamples };
		f16.IntToFloat(inCh, outCh, 4, 1);
		check("IntToFloat(0) == 0.0", outSamples[0] == 0.0f);
		check("IntToFloat(32767) ~= 1.0", std::fabs(outSamples[1] - 1.0f) < 1e-4f);
		check("IntToFloat(-32768) == -1.0", outSamples[2] == -1.0f);
		check("IntToFloat(40000) clamps to 1.0", outSamples[3] == 1.0f);
	}
	{
		float inSamples[4] = { 0.0f, 1.0f, -1.0f, 2.0f /* out of range, clamps */ };
		float *inCh[1] = { inSamples };
		long outSamples[4];
		long *outCh[1] = { outSamples };
		f16.FloatToInt(inCh, outCh, 4, 1);
		check("FloatToInt(0.0) == 0", outSamples[0] == 0);
		check("FloatToInt(1.0) == 32767 (clampMax)", outSamples[1] == 32767);
		check("FloatToInt(-1.0) == -32768 (clampMin)", outSamples[2] == -32768);
		check("FloatToInt(2.0) clamps to 32767", outSamples[3] == 32767);
	}

	/* --- static multichannel utilities --- */
	{
		long a[4] = { 1, 2, 3, 4 };
		long b[4] = { 0, 0, 0, 0 };
		long *inCh[1] = { a };
		long *outCh[1] = { b };
		unsigned long r = CPcmFilter::BitShift(inCh, outCh, 4, 1, 2); /* left by 2 */
		check("BitShift left by 2: 1<<2==4", b[0] == 4);
		check("BitShift left by 2: 4<<2==16", b[1] == 8);
		check("BitShift returns count", r == 4);

		long c[2] = { -32, 16 };
		long d[2];
		long *inCh2[1] = { c };
		long *outCh2[1] = { d };
		CPcmFilter::BitShift(inCh2, outCh2, 2, 1, -3); /* arithmetic right by 3 */
		check("BitShift right by 3: -32>>3==-4", d[0] == -4);
		check("BitShift right by 3: 16>>3==2", d[1] == 2);
	}
	{
		float a[3] = { 1.0f, 2.0f, 3.0f };
		float b[3] = { 0.0f, 0.0f, 0.0f };
		float *inCh[1] = { a };
		float *outCh[1] = { b };
		CPcmFilter::Copy(inCh, outCh, 3, 1);
		check("Copy: element 0", b[0] == 1.0f);
		check("Copy: element 2", b[2] == 3.0f);
	}
	{
		float a[3] = { 1.0f, 2.0f, 3.0f };
		float b[3];
		float *inCh[1] = { a };
		float *outCh[1] = { b };
		CPcmFilter::Reverse(inCh, outCh, 3, 1);
		check("Reverse: [0]==3.0", b[0] == 3.0f);
		check("Reverse: [1]==2.0", b[1] == 2.0f);
		check("Reverse: [2]==1.0", b[2] == 1.0f);
	}
	{
		float a[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		float b[4];
		float *inCh[1] = { a };
		float *outCh[1] = { b };
		CPcmFilter::Fade(inCh, outCh, 4, 1, 1 /* fade in */);
		check("Fade in: first sample near 0", b[0] < 0.5f);
		check("Fade in: samples increase monotonically", b[0] < b[1] && b[1] < b[2] && b[2] < b[3]);
	}
	{
		float a[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		float b[4] = { 9.0f, 9.0f, 9.0f, 9.0f };
		float *buf[1] = { b };
		(void)a;
		unsigned long r = CPcmFilter::Mute(buf, 4, 1);
		check("Mute: all zero", b[0] == 0.0f && b[3] == 0.0f);
		check("Mute returns count", r == 4);
	}
	{
		float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float nonzero[4] = { 0.0f, 0.0f, 0.5f, 0.0f };
		float *chZero[1] = { zero };
		float *chNonzero[1] = { nonzero };
		check("IsSilent: all-zero buffer -> true", CPcmFilter::IsSilent(chZero, 4, 1));
		check("IsSilent: buffer with a nonzero sample -> false",
		      !CPcmFilter::IsSilent(chNonzero, 4, 1));
	}
	{
		float small[3] = { 0.1f, -0.2f, 0.05f };
		float big[3] = { 0.1f, -2.0f, 0.05f };
		float *chSmall[1] = { small };
		float *chBig[1] = { big };
		check("IsAlwaysBelow: all samples under threshold -> true",
		      CPcmFilter::IsAlwaysBelow(chSmall, 3, 1, 0.5f));
		check("IsAlwaysBelow: one sample exceeds threshold -> false",
		      !CPcmFilter::IsAlwaysBelow(chBig, 3, 1, 0.5f));
	}
	{
		float a[4] = { 0.1f, -0.7f, 0.3f, -0.2f };
		float *inCh[1] = { a };
		double m = CPcmFilter::GetMaximumAbsValue(inCh, 4, 1);
		check("GetMaximumAbsValue finds the true peak (0.7)", std::fabs(m - 0.7) < 1e-6);
	}
	{
		float a[3] = { 0.5f, -2.0f /* clips to -1.0 */, 0.25f };
		float out[3];
		float *inCh[1] = { a };
		float *outCh[1] = { out };
		float peaks[1];
		CPcmFilter::ClipAndGetPeakLevels(inCh, outCh, peaks, 3, 1);
		check("ClipAndGetPeakLevels: clips -2.0 to -1.0", out[1] == -1.0f);
		check("ClipAndGetPeakLevels: leaves in-range samples alone", out[0] == 0.5f);
		check("ClipAndGetPeakLevels: peak == 1.0 (from the clipped sample)", peaks[0] == 1.0f);
	}
	{
		float a[3] = { 0.5f, -0.9f, 0.25f };
		float *inCh[1] = { a };
		float peaks[1];
		CPcmFilter::GetPeakLevels(inCh, peaks, 3, 1);
		check("GetPeakLevels finds true peak (0.9)", std::fabs(peaks[0] - 0.9f) < 1e-6f);
	}

	if (g_fail == 0)
		printf("PASS\n");
	else
		printf("FAIL (%d)\n", g_fail);
	return g_fail == 0 ? 0 : 1;
}
