/*
 * kaiser_window.h  -  CKaiserWindowCoeffs, the Kaiser-window FIR-filter-design
 * helper used by Eva's sample-rate converter family (see fs_converter.h).
 *
 * FOUND 2026-07-28, same `nm -C` class-inventory sweep as fs_converter.h/
 * pcm_filter.h's cluster (see PROJECT_BRAIN/status.md). Real class, NO base (a
 * `class_type_info` typeinfo with zero bases, `.rodata+0x08f31384`), 9 real
 * methods, `.text+0x08305b50..0x08305fa3`. Embedded (not heap-owned) as the last
 * member of `CDecimationFilterCoeffs` (fs_converter.h) at that class's own
 * `+0x30` offset -- confirmed via `CDecimationFilterCoeffs::CDecimationFilterCoeffs()`
 * calling `CKaiserWindowCoeffs::CKaiserWindowCoeffs((CKaiserWindowCoeffs*)(this+0x30))`.
 *
 * ALGORITHM: this is a textbook Kaiser-window design (Oppenheim/Schafer,
 * Rabiner) -- confirmed term-for-term against the real `.rodata` constants
 * (all decoded via `objdump -s -j .rodata` + Python `struct.unpack`, not
 * guessed):
 *
 *   beta(A) (`CalcCoeffAlpha()`, real field `mAlpha` at +0xc) is the standard
 *   piecewise formula for the Kaiser-window shape parameter from a target
 *   stopband attenuation A (dB):
 *     A <= 21              : beta = 0
 *     21 < A <= 50         : beta = 0.5842*(A-21)^0.4 + 0.07886*(A-21)
 *     A > 50               : beta = 0.1102*(A-8.7)
 *   confirmed via the real constants 21.0/50.0/8.7/0.1102/0.4/0.5842/0.07886
 *   at .rodata+0x08f29a80/0x08f29a60/0x08f2b018/0x08f2b020/0x08f2b028/
 *   0x08f2b030/0x08f2b038 -- exact match to the textbook formula, not an
 *   approximation guess.
 *
 *   I0(x) (`BesselFunction(double)`) is the modified Bessel function of the
 *   first kind, order 0, via the standard truncated power series
 *   I0(x) = sum_{k=0}^{N} [ (x/2)^k / k! ]^2 -- confirmed by unrolling the
 *   real (GCC 8-way Duff's-device-unrolled) factorial-accumulation loop by
 *   hand: term k contributes `pow(x*0.5, k)^2 / (k!)^2`, and the loop's
 *   final `+ 1.0` (`_DAT_08ea8530`, a real `float 1.0` constant) supplies the
 *   k=0 term. `N` is `mBesselFunctionLength` (+0x2c, default 15).
 *
 *   `CalcDenomAlpha()` computes `mDenomAlpha` (+0x14) = 1.0 / I0(beta) -- the
 *   window's own normalization constant (I0(beta*sqrt(1-x^2)) / I0(beta) is
 *   the real formula `GetWindowCoeff()` evaluates per-tap).
 *
 *   `GetWindowCoeff(n)` returns 0 for n >= mWindowLength (+0x30), else
 *   `I0(beta * sqrt(1 - ((2*(n-center))/(len-1))^2)) * mDenomAlpha`, the
 *   standard per-tap Kaiser window value, `center` = (len-1)/2 (+0x04).
 *
 * VTABLE: real (`vtable for CKaiserWindowCoeffs`, `.rodata+0x08f31348`).
 * `SetSideLobeAttenuation()`/`SetBesselFunctionLength()` dispatch to
 * `CalcCoeffAlpha()`/`CalcDenomAlpha()`/`BesselFunction()` through this vtable
 * (slots +0x18/+0x1c/+0x20) rather than calling them directly -- modeled here
 * as genuine C++ `virtual` methods (real RTTI-bearing class, real vtable; same
 * "let the compiler manage the real vtable" convention stream_family.h already
 * established, not the separate manual-raw-`mVtbl`-pointer idiom task.h uses
 * for its own, differently-shaped classes). No derived class exists anywhere
 * in ground truth's own symbol table for this class, so marking these
 * `virtual` is a faithfulness note, not something an override currently
 * exercises.
 *
 * CONSTRUCTOR DEFAULTS (`.text+0x08305f50`, all confirmed real `.rodata`
 * constants): mCenter=23.5 (window length 48 => (48-1)/2=23.5), mWindowLength=
 * 0x30 (48), mInvLenMinus1=1/47=0.0212765957..., mBesselFunctionLength=15,
 * mAttenuation=80.0 dB, mAlpha=7.85726 (== the real >50dB branch of the beta
 * formula evaluated at 80.0: 0.1102*(80-8.7)=7.85726, exact match -- the ctor
 * hard-codes the already-known answer rather than calling CalcCoeffAlpha()
 * itself, faithfully reproduced), mDenomAlpha = 1.0/I0(7.85726) (computed via
 * a real BesselFunction() call in the ctor body itself).
 *
 * `SQRT()`/`NAN()` in `GetWindowCoeff()`'s ground truth (an `fsqrt` fast path
 * with a NaN fallback to libm `sqrt()`) collapses to plain `std::sqrt()` here
 * (same "hardware fsqrt vs libm sqrt, identical result for in-domain values"
 * simplification already used project-wide for float ops).
 */

#ifndef KAISER_WINDOW_H
#define KAISER_WINDOW_H

class CKaiserWindowCoeffs {
public:
	/* .text+0x08305f50, 93 bytes. */
	CKaiserWindowCoeffs();

	/* .text+0x08305b50 (D1) / 0x08305f30 (D0, frees `this`). */
	virtual ~CKaiserWindowCoeffs();

	/* .text+0x08305b60, 49 bytes. Real ground truth leaves EAX == the `length`
	 * argument on return (confirmed at 2 independent call sites in
	 * fs_converter.cpp, both of which use the "return value" as if it were
	 * simply `length` echoed back) -- modeled as an explicit `return length;`
	 * rather than void, matching that observed behavior. */
	int SetWindowLength(int length);

	/* .text+0x08305ba0, 41 bytes. Recomputes mAlpha/mDenomAlpha. */
	double SetSideLobeAttenuation(double attenuationDb);

	/* .text+0x08305bd0, 33 bytes. Recomputes mDenomAlpha. */
	int SetBesselFunctionLength(int length);

	/* .text+0x08305c30, 118 bytes. */
	virtual double GetWindowCoeff(int n) const;

protected:
	/* .text+0x08305c00, 39 bytes. */
	virtual void CalcDenomAlpha();

	/* .text+0x08305ea0, 130 bytes. Real piecewise beta(A) formula. */
	virtual void CalcCoeffAlpha();

	/* .text+0x08305cb0, 472 bytes. I0(x) via truncated power series. */
	virtual double BesselFunction(double x) const;

	double mCenter;              /* +0x04, (mWindowLength-1)/2 */
	double mAlpha;                /* +0x0c, Kaiser beta */
	double mDenomAlpha;           /* +0x14, 1/I0(beta) */
	double mInvLenMinus1;         /* +0x1c, 1/(mWindowLength-1) */
	double mAttenuation;          /* +0x24, target stopband attenuation, dB */
	int    mBesselFunctionLength; /* +0x2c, series truncation N */
	int    mWindowLength;         /* +0x30 */
};

#endif /* KAISER_WINDOW_H */
