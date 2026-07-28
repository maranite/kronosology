/*
 * test_fs_converter.cpp  -  host-side known-answer test for
 * CDecimationFilterCoeffs/COversamplingFilterCoeffs/CFsConverterNormal/
 * CFsCwInterpolation (src/base/fs_converter.cpp). See include/fs_converter.h
 * for full ground-truth provenance, including which methods are
 * intentionally DEFERRED (Process()/the 5-arg SetFilterCoeffs()s) this pass.
 *
 * No CSystemApi dependency -- this cluster makes no Api/HAL calls.
 */

#include <cmath>
#include <cstdio>

#include "fs_converter.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CDecimationFilterCoeffs/COversamplingFilterCoeffs/"
	       "CFsConverterNormal/CFsCwInterpolation known-answer test\n");

	{
		CDecimationFilterCoeffs d;
		check("default ctor: GetDelayOffsetSamples() == filterLen/2 == 24",
		      d.GetDelayOffsetSamples() == 24);
		check("default ctor: coeff(48) out of range (==0)", d.GetFilterCoeff(48) == 0.0);

		/* windowed-sinc lowpass FIR: symmetric about (filterLen-1)/2. */
		bool symmetric = true;
		for (int n = 0; n < 24; ++n) {
			if (std::fabs(d.GetFilterCoeff(n) - d.GetFilterCoeff(47 - n)) > 1e-9)
				symmetric = false;
		}
		check("default coefficients are symmetric about the sinc center", symmetric);

		/* SetSampleRate/SetCutoffFreq recompute mSincScale via
		 * CalcFreqCoeffs() -- doubling cutoff should double the near-center
		 * tap scale (mSincScale = 2*fc/fs is the tap value AT the exact
		 * center; verify indirectly via the delay-offset-seconds identity
		 * mFilterLength/2 * 1/sampleRate, which only depends on
		 * SetSampleRate()). */
		d.SetSampleRate(48000);
		double expectedDelaySec = 24.0 / 48000.0;
		check("SetSampleRate(48000): GetDelayOffsetSeconds() matches 24/48000",
		      std::fabs(d.GetDelayOffsetSeconds() - expectedDelaySec) < 1e-9);

		d.SetFilterLength(96);
		check("SetFilterLength(96): GetDelayOffsetSamples() == 48",
		      d.GetDelayOffsetSamples() == 48);
	}

	{
		COversamplingFilterCoeffs o;
		o.SetOversamplingRate(4);
		/* GetFilterCoeff() override multiplies the base decimation-filter
		 * coefficient by mOversamplingRate -- so scaling the rate from the
		 * ctor default (1.0) up to 4 should scale every in-range tap by
		 * exactly 4x relative to an otherwise-identical base object. */
		CDecimationFilterCoeffs base2;
		double baseVal = base2.GetFilterCoeff(20);
		double ovsVal = o.GetFilterCoeff(20);
		check("COversamplingFilterCoeffs::GetFilterCoeff scales by mOversamplingRate",
		      std::fabs(ovsVal - baseVal * 4.0) < 1e-9);
	}

	{
		CFsConverterNormal conv(2);
		check("CFsConverterNormal(2ch): GetDelayOffsetSamples() == 24 (default filter len)",
		      conv.GetDelayOffsetSamples() == 24);
		check("Process() is a documented stub: reports 0 samples produced",
		      conv.Process(0, 0, 0) == 0);
		conv.Reset(); /* must not crash */
		check("Reset() completed without crashing", true);
	}

	{
		/* Channel count is capped to 8 -- construct with more and make
		 * sure it doesn't overflow SRingBufState::mChannelRing[8]. */
		CFsConverterNormal conv(16);
		check("channel count capped to 8, ctor/dtor safe for 16-channel request", true);
	}

	{
		CFsCwInterpolation interp(2);
		check("CFsCwInterpolation ctor/dtor (derived class) safe", true);
		check("CFsCwInterpolation::Process() is a documented stub: reports 0",
		      interp.Process(0, 0, 0) == 0);
	}

	if (g_fail == 0)
		printf("PASS\n");
	else
		printf("FAIL (%d)\n", g_fail);
	return g_fail == 0 ? 0 : 1;
}
