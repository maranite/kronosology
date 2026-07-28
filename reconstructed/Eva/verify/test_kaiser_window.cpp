/*
 * test_kaiser_window.cpp  -  host-side known-answer test for
 * CKaiserWindowCoeffs (src/base/kaiser_window.cpp). See
 * include/kaiser_window.h for full ground-truth provenance.
 *
 * No CSystemApi dependency -- this class makes no Api/HAL calls at all.
 */

#include <cmath>
#include <cstdio>

#include "kaiser_window.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CKaiserWindowCoeffs known-answer test\n");

	CKaiserWindowCoeffs w;

	/* Ctor default: length 48, attenuation 80dB -> beta = 0.1102*(80-8.7) =
	 * 7.85726 exactly (the real >50dB branch of the piecewise formula). */
	check("default window length coeff(48)==0 (past mWindowLength)",
	      w.GetWindowCoeff(48) == 0.0);
	check("default window coeff(100)==0 (well past mWindowLength)",
	      w.GetWindowCoeff(100) == 0.0);

	/* Kaiser window is symmetric about its center: (len-1)/2 = 23.5 for
	 * length 48, so coeff(n) == coeff(47-n) for every valid n. */
	bool symmetric = true;
	for (int n = 0; n < 24; ++n) {
		double a = w.GetWindowCoeff(n);
		double b = w.GetWindowCoeff(47 - n);
		if (std::fabs(a - b) > 1e-9)
			symmetric = false;
	}
	check("default window is symmetric about center", symmetric);

	/* The window should peak at the center and taper toward the edges. */
	double center = w.GetWindowCoeff(23);
	double edge = w.GetWindowCoeff(0);
	check("center tap > edge tap", center > edge);
	check("edge tap is positive (beta=7.86 Kaiser window never goes to 0 "
	      "at the edge for a finite window)", edge > 0.0);

	/* SetSideLobeAttenuation(80.0) recomputes mAlpha via CalcCoeffAlpha()
	 * (the real piecewise beta(A) formula) -- since the ctor ALREADY
	 * hard-codes the same 80dB answer (7.85726), re-deriving it from
	 * scratch via the formula should reproduce (near) bit-identical
	 * window coefficients. This is an end-to-end check that
	 * CalcCoeffAlpha()'s real piecewise formula matches the ctor's own
	 * hard-coded starting point. */
	double before[48];
	for (int n = 0; n < 48; ++n)
		before[n] = w.GetWindowCoeff(n);
	w.SetSideLobeAttenuation(80.0);
	bool matches = true;
	for (int n = 0; n < 48; ++n) {
		if (std::fabs(before[n] - w.GetWindowCoeff(n)) > 1e-6)
			matches = false;
	}
	check("SetSideLobeAttenuation(80.0) reproduces ctor's own default window",
	      matches);

	/* Piecewise formula boundary sanity: <=21dB gives beta=0 (rectangular
	 * window, every non-edge-clipped tap should be uniformly
	 * mDenomAlpha == 1.0 since I0(0)==1). */
	CKaiserWindowCoeffs w2;
	w2.SetSideLobeAttenuation(21.0);
	check("21dB attenuation -> beta=0 -> uniform (rectangular) window",
	      std::fabs(w2.GetWindowCoeff(0) - w2.GetWindowCoeff(23)) < 1e-9);

	/* SetWindowLength() changes which taps are in-range. */
	CKaiserWindowCoeffs w3;
	w3.SetWindowLength(10);
	check("SetWindowLength(10): coeff(9) in range (!=0)", w3.GetWindowCoeff(9) != 0.0);
	check("SetWindowLength(10): coeff(10) out of range (==0)", w3.GetWindowCoeff(10) == 0.0);

	if (g_fail == 0)
		printf("PASS\n");
	else
		printf("FAIL (%d)\n", g_fail);
	return g_fail == 0 ? 0 : 1;
}
