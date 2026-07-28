/*
 * fs_converter.cpp  -  see include/fs_converter.h for full ground-truth
 * provenance, the DEFERRED-method list, and SRingBufState's field-by-field
 * recovery. `CFsConverterNormal::BuildFilterCoeffTable()` (the real
 * `SetFilterCoeffs(int,int,int,int,int)` overload) and
 * `CFsCwInterpolation::SetFilterCoeffs(int,int,float,int,int)` are real,
 * reconstructed 2026-07-28. `CFsConverterNormal`'s own un-overridden
 * `SetFilterCoeffs(int,int,float,int,int)` slot really is a 1-byte
 * `return;` in ground truth (see header). Both `Process()` overrides remain
 * intentionally-minimal stubs -- see the header comment for exactly why the
 * convolution core is out of scope this pass.
 */

#include "fs_converter.h"

#include <cmath>
#include <cstring>

/* ---------------------------------------------------------------- */
/* CDecimationFilterCoeffs                                          */
/* ---------------------------------------------------------------- */

CDecimationFilterCoeffs::CDecimationFilterCoeffs()
    : mCenter(0.0), mSincScale(0.11196145124716553), mSincArg(0.35173727272334704),
      mFilterLength(0), mSampleRate(0x2b1100), mInvSampleRate(3.5430839002267574e-07),
      mCutoffFreq(0x26930), mWindow()
{
	int len = mWindow.SetWindowLength(0x30);
	mFilterLength = len;
	mCenter = (double)((float)(len - 1) * 0.5f);
}

CDecimationFilterCoeffs::~CDecimationFilterCoeffs()
{
}

int CDecimationFilterCoeffs::SetSampleRate(int sampleRateHz)
{
	mSampleRate = sampleRateHz;
	mInvSampleRate = (double)(1.0f / (float)sampleRateHz);
	CalcFreqCoeffs();
	return mSampleRate;
}

int CDecimationFilterCoeffs::SetCutoffFreq(int cutoffHz)
{
	mCutoffFreq = cutoffHz;
	CalcFreqCoeffs();
	return mCutoffFreq;
}

void CDecimationFilterCoeffs::CalcFreqCoeffs()
{
	double fc = (double)mCutoffFreq;
	mSincScale = (fc + fc) / (double)mSampleRate;
	mSincArg = (fc * 6.283185307179586) / (double)mSampleRate;
}

void CDecimationFilterCoeffs::SetFilterLength(int length)
{
	int len = mWindow.SetWindowLength(length);
	mFilterLength = len;
	mCenter = (double)((float)(len - 1) * 0.5f);
}

void CDecimationFilterCoeffs::SetBesselFunctionLength(int length)
{
	mWindow.SetBesselFunctionLength(length);
}

void CDecimationFilterCoeffs::SetSideLobeAttenuation(double attenuationDb)
{
	mWindow.SetSideLobeAttenuation(attenuationDb);
}

double CDecimationFilterCoeffs::GetFilterCoeff(int n) const
{
	if (n >= mFilterLength)
		return 0.0;
	double d = (double)n - mCenter;
	if (d != 0.0) {
		double arg = d * mSincArg;
		double s = std::sin(arg);
		return (s / arg) * mSincScale * mWindow.GetWindowCoeff(n);
	}
	return mSincScale;
}

int CDecimationFilterCoeffs::GetDelayOffsetSamples() const
{
	return mFilterLength >> 1;
}

double CDecimationFilterCoeffs::GetDelayOffsetSeconds() const
{
	return (double)GetDelayOffsetSamples() * mInvSampleRate;
}

double CDecimationFilterCoeffs::GetDelayOffset() const
{
	return (double)(mFilterLength >> 1) * mInvSampleRate;
}

/* ---------------------------------------------------------------- */
/* COversamplingFilterCoeffs : CDecimationFilterCoeffs               */
/* ---------------------------------------------------------------- */

COversamplingFilterCoeffs::COversamplingFilterCoeffs()
    : CDecimationFilterCoeffs(), mOversamplingRate(1.0)
{
}

COversamplingFilterCoeffs::~COversamplingFilterCoeffs()
{
}

int COversamplingFilterCoeffs::SetOversamplingRate(int rate)
{
	mOversamplingRate = (double)rate;
	return rate;
}

double COversamplingFilterCoeffs::GetFilterCoeff(int n) const
{
	return CDecimationFilterCoeffs::GetFilterCoeff(n) * mOversamplingRate;
}

/* ---------------------------------------------------------------- */
/* CFsConverterNormal                                                */
/* ---------------------------------------------------------------- */

CFsConverterNormal::CFsConverterNormal(int numChannels)
    : mFilterCoeffs(), mState(0)
{
	mState = new SRingBufState;
	std::memset(mState->mPhaseCoeffs, 0, sizeof(mState->mPhaseCoeffs));

	int ch = numChannels < 9 ? numChannels : 8;
	mState->mNumChannels = (unsigned int)ch;
	mState->mOversamplingRate = 0;
	mState->mDecimationFactor = 0;
	mState->mRingMask = 0x3ff;
	mState->mUnknown83c = 1.0f;
	mState->mUnknown840 = 0;
	mState->mUnknown844 = 0xffffffffu;

	for (int i = 0; i < ch; ++i)
		mState->mChannelRing[i] = new float[0x1000 / sizeof(float)];

	mFilterCoeffs.SetSideLobeAttenuation(100.0);
	mFilterCoeffs.SetBesselFunctionLength(15);

	for (int i = 0; i < ch; ++i)
		std::memset(mState->mChannelRing[i], 0, 0x1000);

	mState->mRingWritePos = 0;
	mState->mCarry = 0;
}

CFsConverterNormal::~CFsConverterNormal()
{
	for (unsigned int i = 0; i <= mState->mOversamplingRate; ++i) {
		if (mState->mPhaseCoeffs[i])
			delete[] mState->mPhaseCoeffs[i];
	}
	for (unsigned int i = 0; i < mState->mNumChannels; ++i) {
		if (mState->mChannelRing[i])
			delete[] mState->mChannelRing[i];
	}
	delete mState;
}

void CFsConverterNormal::SetFilterCoeffs(int sampleRateHz)
{
	if (sampleRateHz == 0xac44) { /* 44100 */
		SetFilterCoeffs(0x3c01, 0xac44, 0.0f, 0xa0, 0x93);
	} else if (sampleRateHz == 48000) {
		SetFilterCoeffs(0x3721, 48000, 0.0f, 0x93, 0xa0);
	}
}

void CFsConverterNormal::SetFilterCoeffs(int, int, float, int, int)
{
	/* Real ground truth: 1-byte "return;" -- this class's own vtable slot
	 * is a genuine no-op; CFsCwInterpolation supplies the real override. */
}

void CFsConverterNormal::BuildFilterCoeffTable(int filterLength, int inputRateFactor,
                                                 int cutoffFreq, int maxPhases, int decimation)
{
	/* Free the previous phase-coefficient tables (old count, inclusive of
	 * the old mOversamplingRate itself -- ground truth's own loop bound). */
	for (unsigned int i = 0; i <= mState->mOversamplingRate; ++i) {
		delete[] mState->mPhaseCoeffs[i];
		mState->mPhaseCoeffs[i] = 0;
	}

	int newRate = (maxPhases <= 0x1ff) ? maxPhases : 0x1ff; /* ground truth: cmovle */
	mState->mOversamplingRate = (unsigned int)newRate;
	mState->mDecimationFactor = (unsigned int)decimation;

	unsigned int rate = mState->mOversamplingRate;
	mFilterCoeffs.SetOversamplingRate((int)rate);
	mFilterCoeffs.SetFilterLength(filterLength);
	mFilterCoeffs.SetSampleRate((int)(rate * (unsigned int)inputRateFactor));
	mFilterCoeffs.SetCutoffFreq(cutoffFreq);

	/* Per-phase tap count, rounded up to a multiple of 4 (ground truth:
	 * unsigned division, `rate` is assumed nonzero here just as ground
	 * truth assumes it -- a caller passing maxPhases<0 would divide by
	 * zero in ground truth too). */
	unsigned int blockSize = (((rate + (unsigned int)filterLength - 1) / rate) + 3) & ~3u;
	mState->mBlockSize = blockSize;

	for (unsigned int j = 0; j <= rate; ++j) {
		float *phase = new float[blockSize];
		mState->mPhaseCoeffs[j] = phase;
		std::memset(phase, 0, blockSize * sizeof(float));

		int k = 0;
		for (int n = (int)j; n < filterLength; n += (int)rate)
			phase[k++] = (float)mFilterCoeffs.GetFilterCoeff(n);
	}

	/* Ground truth: tail call through the virtual Reset() slot. */
	Reset();
}

int CFsConverterNormal::Process(float **, float **, int)
{
	/* DEFERRED -- see header comment. Always reports 0 samples produced
	 * rather than fabricating output. */
	return 0;
}

void CFsConverterNormal::Reset()
{
	for (unsigned int i = 0; i < mState->mNumChannels; ++i)
		std::memset(mState->mChannelRing[i], 0, 0x1000);
	mState->mRingWritePos = 0;
	mState->mCarry = 0;
}

int CFsConverterNormal::GetDelayOffsetSamples() const
{
	return mFilterCoeffs.GetDelayOffsetSamples();
}

double CFsConverterNormal::GetDelayOffsetSeconds() const
{
	return mFilterCoeffs.GetDelayOffsetSeconds();
}

double CFsConverterNormal::GetDelayOffset() const
{
	return mFilterCoeffs.GetDelayOffset();
}

/* ---------------------------------------------------------------- */
/* CFsCwInterpolation : CFsConverterNormal                           */
/* ---------------------------------------------------------------- */

CFsCwInterpolation::CFsCwInterpolation(int numChannels)
    : CFsConverterNormal(numChannels)
{
	mState->mUnknown840 = 0x3fff;
	mState->mUnknown83c = 6.103515625e-05f; /* real bits 0x38800000, == 2^-14 */
	mState->mUnknown844 = 0x3fff;
}

CFsCwInterpolation::~CFsCwInterpolation()
{
	mState->mOversamplingRate >>= 0xe;
}

void CFsCwInterpolation::SetFilterCoeffs(int a, int b, float c, int d, int e)
{
	/* mOversamplingRate is stored left-shifted by 14 bits between calls in
	 * this class; un-shift to the plain phase-count BuildFilterCoeffTable()
	 * expects. Note the param mapping into the int-only overload: d (this
	 * override's 4th param) becomes cutoffFreq, e becomes maxPhases --
	 * decimation is hardcoded to 1 through this path. `c` is NOT forwarded;
	 * it is only used below as the fixed-point scale-factor divisor. */
	mState->mOversamplingRate >>= 0xe;
	CFsConverterNormal::BuildFilterCoeffTable(a, b, d, e, 1);

	/* mOversamplingRate now holds the freshly-clamped new phase count R
	 * (set by BuildFilterCoeffTable() above). Ground truth: fild(b*R);
	 * fdiv(c); 14x fadd st,st(0) (== *16384.0 exactly, no rounding drift --
	 * repeated doubling never loses precision); fisttp (truncates toward
	 * zero, same as a C (int) cast) -> mDecimationFactor. Then re-shift
	 * mOversamplingRate back up by 14. */
	unsigned int rate = mState->mOversamplingRate;
	double scaled = (double)(b * (int)rate);
	mState->mOversamplingRate = rate << 0xe;
	double fixedPoint = (scaled / (double)c) * 16384.0;
	mState->mDecimationFactor = (unsigned int)(int)fixedPoint;

	/* Ground truth: this override ends with its OWN tail call to Reset(),
	 * on top of the one BuildFilterCoeffTable() already ran above -- Reset()
	 * genuinely runs twice per CFsCwInterpolation::SetFilterCoeffs() call. */
	Reset();
}

int CFsCwInterpolation::Process(float **, float **, int)
{
	/* DEFERRED -- see header comment. */
	return 0;
}
