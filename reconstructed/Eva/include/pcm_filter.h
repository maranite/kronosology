/*
 * pcm_filter.h  -  CPcmFilter, Eva's multichannel PCM buffer utility class:
 * int<->float conversion, bit-depth shifting, copy/reverse/fade/mute, and
 * peak-level measurement over arrays of per-channel sample buffers. Found in
 * the same sweep as fs_converter.h/kaiser_window.h -- see fs_converter.h's
 * header comment for the full candidate-selection story (2 larger candidates
 * traced and rejected first) and PROJECT_BRAIN/status.md.
 *
 * Real class, NO base (`class_type_info`, `.rodata+0x08f31458`), 18 methods,
 * `.text+0x08306080..0x083084b8`. Zero dependency on any other Eva class --
 * only `memmove`/`memset` and (for the 2 instance methods) its own 5 float
 * fields. Split into two shapes:
 *
 *   - 2 real INSTANCE methods (`__thiscall`, bit-depth-conversion state:
 *     `mBits`/`mIntToFloatScale`/`mFloatToIntScale`/`mClampMax`/`mClampMin`):
 *     the ctor, `SetBitsPerSample(int)`, `IntToFloat()`, `FloatToInt()`.
 *   - The remaining methods (`Copy`/`Reverse`/`Fade`/`Mute`/`BitShift`/
 *     `IsAlwaysBelow`/`IsSilent`/`GetMaximumAbsValue`/`ClipAndGetPeakLevels`/
 *     `GetPeakLevels`) are real GROUND-TRUTH `__cdecl` STATIC methods (no
 *     `this` parameter at all in their own real prototypes, confirmed via
 *     `nm -C -S` + each decompile's own `cc=__cdecl`/missing-`this`
 *     signature) -- multichannel buffer utilities that don't need any
 *     per-instance state.
 *
 * Every multichannel method below operates on `float**`/`long**` "array of
 * per-channel buffer pointers" arguments -- ground truth's own real bodies
 * are GCC Duff's-device-unrolled (4- or 8-way) per-channel/per-sample loops,
 * one of which (`BitShift`) is additionally SSE-auto-vectorized (a real
 * `psrad`/`pslld`-shaped 4-wide shift with its own scalar remainder tail) --
 * all collapsed here to the equivalent plain nested loop, same "GCC unrolled
 * loop -> plain loop, byte-identical result" license used project-wide
 * (e.g. task.h's own `RegisterIfc()`/`~CTask()` notes). Every collapse below
 * was verified by reading the FIRST unrolled iteration (establishes the
 * per-element body) and the LAST unrolled block plus the function's own
 * return statement (establishes loop bound/return value), not guessed.
 *
 * `BitShift(long**,long**,ulong count,int channels,int shift)`: real
 * ground-truth semantics are a single signed shift, direction chosen by the
 * SIGN of `shift` -- `shift < 0`: arithmetic right-shift by `-shift`
 * (ground truth: `x >> -shift`, an SSE `psrad`); `shift >= 0`: left-shift by
 * `shift` (`x << shift`). Confirmed at both the vectorized-block entry
 * (`uVar6 = -param_5`, used as the right-shift amount) and the scalar tail
 * (`*piVar12 = *piVar7 << (bVar8 & 0x1f)` in the `shift >= 0` branch).
 * Returns `count` unconditionally (real: `return param_3;`).
 *
 * `Fade(float**,float**,ulong count,int channels,int direction)`:
 * `direction==1` (fade IN): per-sample gain ramps `0.0 -> 1.0` linearly over
 * `count` samples (`step = 1.0/count`). `direction==2` (fade OUT): gain
 * ramps from `1.0 - 1.0/count` down to (clamped) `0.0` (real:
 * `step = -1.0/count`, `gain0 = step + 1.0`). Any OTHER `direction` value:
 * real ground truth is a plain per-channel `memmove` (no fade at all) --
 * reproduced faithfully, not simplified away. `count==0`: returns 0
 * immediately (real: literal early `return 0;`, the one place this class
 * returns 0 rather than `count` for a size-based method).
 */

#ifndef PCM_FILTER_H
#define PCM_FILTER_H

#include <cstddef>

class CPcmFilter {
public:
	/* .text+0x083064e0, 77 bytes. */
	explicit CPcmFilter(int bitsPerSample);

	/* .text+0x08306080 (D1) / 0x083064c0 (D0, frees `this`). */
	~CPcmFilter();

	/* .text+0x08306090, 73 bytes. */
	void SetBitsPerSample(int bitsPerSample);

	/* .text+0x083060e0, 513 bytes. Int samples -> float in [-1,1]. */
	void IntToFloat(long **in, float **out, unsigned long count, int channels) const;

	/* .text+0x08306300, 409 bytes. Float samples -> clamped int. */
	void FloatToInt(float **in, long **out, unsigned long count, int channels) const;

	/* .text+0x08306530, 1825 bytes, __cdecl static. Signed shift, direction
	 * by sign of `shift` -- see header comment. Returns `count`. */
	static unsigned long BitShift(long **in, long **out, unsigned long count,
	                                int channels, int shift);

	/* .text+0x08306c60, 550 bytes, __cdecl static. Per-channel memmove.
	 * Returns `count`. */
	static unsigned long Copy(float **in, float **out, unsigned long count, int channels);

	/* .text+0x08306e90, 600 bytes, __cdecl static. Per-channel sample-order
	 * reversal. Returns `count`. */
	static unsigned long Reverse(float **in, float **out, unsigned long count, int channels);

	/* .text+0x083070f0, 1121 bytes, __cdecl static. See header comment.
	 * Returns 0 if count==0, else `count`. */
	static unsigned long Fade(float **in, float **out, unsigned long count,
	                            int channels, int direction);

	/* .text+0x08307560, 520 bytes, __cdecl static. True iff every sample's
	 * |value| <= threshold across all channels. */
	static bool IsAlwaysBelow(float **in, unsigned long count, int channels, float threshold);

	/* .text+0x08307790, 612 bytes, __cdecl static. True iff every sample is
	 * exactly 0.0 across all channels. */
	static bool IsSilent(float **in, unsigned long count, int channels);

	/* .text+0x08307a50, 551 bytes, __cdecl static. Running max(|sample|)
	 * over all channels/samples. */
	static double GetMaximumAbsValue(float **in, unsigned long count, int channels);

	/* .text+0x08307cb0, 756 bytes, __cdecl static. Clips each sample to
	 * [-1,1] into `out`, writes per-channel peak |clipped value| into
	 * `peakLevels[channel]` (0.0 if count==0). */
	static void ClipAndGetPeakLevels(float **in, float **out, float *peakLevels,
	                                    unsigned long count, int channels);

	/* .text+0x08307fe0, 580 bytes, __cdecl static. Per-channel
	 * max(|sample|) into `peakLevels[channel]` (0.0 if count==0). */
	static void GetPeakLevels(float **in, float *peakLevels, unsigned long count, int channels);

	/* .text+0x08308270, 541 bytes, __cdecl static. Per-channel memset(0).
	 * Returns `count`. */
	static unsigned long Mute(float **buf, unsigned long count, unsigned long channels);

private:
	int   mBits;            /* +0x04 */
	float mIntToFloatScale; /* +0x08, 1.0 / 2^(mBits-1) */
	float mFullScale;       /* +0x0c, 2^(mBits-1) */
	float mClampMax;        /* +0x10, 2^(mBits-1) - 1 */
	float mClampMin;        /* +0x14, -2^(mBits-1) */
};

#endif /* PCM_FILTER_H */
