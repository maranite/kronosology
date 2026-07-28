/*
 * kaiser_window.cpp  -  see include/kaiser_window.h for full ground-truth
 * provenance and the real .rodata constants this file's literals were decoded
 * from (all confirmed via objdump -s -j .rodata + Python struct.unpack, not
 * guessed).
 */

#include "kaiser_window.h"

#include <cmath>

CKaiserWindowCoeffs::CKaiserWindowCoeffs()
    : mCenter(23.5), mAlpha(7.85726), mDenomAlpha(0.0),
      mInvLenMinus1(1.0 / 47.0), mAttenuation(80.0),
      mBesselFunctionLength(15), mWindowLength(0x30)
{
	mDenomAlpha = 1.0 / BesselFunction(mAlpha);
}

CKaiserWindowCoeffs::~CKaiserWindowCoeffs()
{
}

int CKaiserWindowCoeffs::SetWindowLength(int length)
{
	mWindowLength = length;
	mCenter = (double)((float)(length - 1) * 0.5f);
	mInvLenMinus1 = (double)(1.0f / (float)(length - 1));
	return length;
}

double CKaiserWindowCoeffs::SetSideLobeAttenuation(double attenuationDb)
{
	mAttenuation = attenuationDb;
	CalcCoeffAlpha();
	CalcDenomAlpha();
	return mAttenuation;
}

int CKaiserWindowCoeffs::SetBesselFunctionLength(int length)
{
	mBesselFunctionLength = length;
	CalcDenomAlpha();
	return mBesselFunctionLength;
}

void CKaiserWindowCoeffs::CalcDenomAlpha()
{
	mDenomAlpha = 1.0 / BesselFunction(mAlpha);
}

void CKaiserWindowCoeffs::CalcCoeffAlpha()
{
	/* Real, textbook Kaiser beta(A) piecewise formula -- see header comment
	 * for the exact .rodata constants this reproduces. */
	if (mAttenuation <= 21.0) {
		mAlpha = 0.0;
	} else if (mAttenuation >= 50.0) {
		mAlpha = (mAttenuation - 8.7) * 0.1102;
	} else {
		mAlpha = (mAttenuation - 21.0) * 0.07886 +
		         pow(mAttenuation - 21.0, 0.4) * 0.5842;
	}
}

double CKaiserWindowCoeffs::BesselFunction(double x) const
{
	/* I0(x) = sum_{k=0}^{N} [ (x/2)^k / k! ]^2, N = mBesselFunctionLength.
	 * Real ground truth computes this via an 8-way-unrolled factorial
	 * accumulation loop starting at k=1 and adding the k=0 term (== 1.0)
	 * afterward -- collapsed here to a plain loop with the same result. */
	double sum = 0.0;
	double term = 1.0; /* (x/2)^0 / 0! */
	for (int k = 1; k <= mBesselFunctionLength; ++k) {
		term *= (x * 0.5) / (double)k;
		sum += term * term;
	}
	return sum + 1.0;
}

double CKaiserWindowCoeffs::GetWindowCoeff(int n) const
{
	if (n >= mWindowLength)
		return 0.0;
	double norm = (2.0 * ((double)n - mCenter)) * mInvLenMinus1;
	double arg2 = norm * norm;
	if (arg2 > 1.0)
		arg2 = 1.0;
	double root = std::sqrt(1.0 - arg2);
	return BesselFunction(mAlpha * root) * mDenomAlpha;
}
