/*
 * fs_converter.h  -  CDecimationFilterCoeffs / COversamplingFilterCoeffs /
 * CFsConverterNormal / CFsCwInterpolation, Eva's windowed-sinc sample-rate
 * conversion family. See kaiser_window.h for the Kaiser-window math these
 * build on, and pcm_filter.h for the sibling bit-depth/gain PCM utility
 * class found in the same sweep.
 *
 * FOUND 2026-07-28, fresh `nm -C` class-inventory sweep for the next dense,
 * previously-100%-untouched, well-defined cluster (continuing the "shared
 * root + siblings via call-xref" line -- see PROJECT_BRAIN/status.md). Two
 * earlier, larger candidates from the same sweep were traced and REJECTED
 * before landing here:
 *
 *   - `CRTRouter`/`CRTRouterApiInstance` (44+49 methods, real-time MIDI event
 *     router) -- clean of the project's named exclusions (no CZ/ES-family/
 *     Peg/virtual-driver/CFMBrowseForm) but its OWN core dispatch methods
 *     (`PutChMsgEvents` 5847 bytes, `SelectCC`/`SelectAll`, `PanicReset`)
 *     reach deep into `CTrackBase`'s raw private layout (a separate, 0%-done,
 *     26-method class) via hundreds of inlined soft-assert branches over the
 *     full MIDI channel-message-type space -- the same "reach into a whole
 *     other undone class's memory" entanglement trap the project's named
 *     exclusions describe, just not literally on that list. Rejected after
 *     full call-xref tracing, not on method-count alone.
 *   - The `CFileBase` file-format-loader family (`CFileAKP`/`CFileCda`/
 *     `CFileExl`/`CFileKCD`/`CFileKMP`/`CFileMid`, ~74 methods) -- every
 *     sibling except `CFileBase` itself and `CFileDX7Syx` calls directly into
 *     `CFMBrowseForm` (progress-bar UI), the literal named exclusion.
 *     `CFileDX7Syx` alone is clean of `CFMBrowseForm` but pulls into the
 *     unmodeled MOSS voice-model engine (`CMOSSAlgorithm`/`CMOSSProg`/etc,
 *     the same out-of-scope hierarchy `korg_file.h` already flagged).
 *     Rejected.
 *
 * This cluster instead: `CDecimationFilterCoeffs` (root, 11 methods) / its
 * derived `COversamplingFilterCoeffs` (5 methods) / `CFsConverterNormal`
 * (root, 15 methods, embeds a `COversamplingFilterCoeffs`) / its derived
 * `CFsCwInterpolation` (7 methods) -- confirmed via `nm -C "typeinfo for ..."`
 * + a direct `.rodata` byte dump of each typeinfo's own base-class pointer
 * (`__si_class_type_info` shape, 3rd word = base typeinfo address). External
 * dependency surface (`objdump -d -C` call-xref sweep over the full
 * `.text+0x08304160..0x08306050` range): only `CKaiserWindowCoeffs` (own
 * dependency, reconstructed alongside this, kaiser_window.h), `operator new/
 * new[]/delete/delete[]`, `memset`, and libm (`sin`/`pow`). Zero touches of
 * CZ/ES-family/Peg/virtual-driver/CFMBrowseForm/CSTGMultisampleBank.
 *
 * DEFERRED, this pass (2 real ground-truth methods, both real, both large,
 * both genuinely out of scope for a single pass -- same "carve out the
 * hardest 1-2 outlier functions, document why, keep the rest" precedent
 * `stg_unsol_msg_handler.h`/`korg_file.h` already established elsewhere):
 *
 *   - `CFsConverterNormal::Process(float**,float**,int)` (1446 bytes) /
 *     `CFsCwInterpolation::Process(float**,float**,int)` (1643 bytes) -- the
 *     actual polyphase-FIR ring-buffer resampling inner loops (per-channel
 *     circular delay line + per-phase FIR convolution sum, both GCC
 *     8-way-Duff's-device-unrolled AND partially SSE-auto-vectorized in
 *     ground truth). Real, well-understood in outline (see `SRingBufState`
 *     below for the ring-buffer field layout this pass DID recover, from the
 *     ctor/dtor/Reset() bodies, which ARE implemented), but transcribing the
 *     actual sample-accurate convolution/ring-index arithmetic correctly is
 *     high-risk/high-effort and belongs in its own dedicated pass.
 *   - `CFsConverterNormal::SetFilterCoeffs(int,int,int,int,int)` (549 bytes,
 *     NOT virtual, only ever reached through `CFsCwInterpolation`'s own
 *     override below) -- the per-phase coefficient-table builder that calls
 *     `COversamplingFilterCoeffs::GetFilterCoeff()` in a loop and allocates
 *     `mState->mPhaseCoeffs[]`. Deferred alongside `Process()` since it only
 *     matters once `Process()` itself is implemented.
 *   - `CFsCwInterpolation::SetFilterCoeffs(int,int,float,int,int)` (171
 *     bytes, virtual override of `CFsConverterNormal`'s own slot) -- thin
 *     wrapper around the deferred `SetFilterCoeffs(int,int,int,int,int)`
 *     above plus a fixed-point scale-factor computation; deferred with it.
 *
 * `CFsConverterNormal::SetFilterCoeffs(int)` (134 bytes, the *virtual
 * dispatcher* used by real callers, NOT deferred) still gets a real body:
 * picks a canned coefficient-table preset for 44100/48000 Hz and virtual-
 * dispatches to the (deferred, stub) 5-arg override -- faithful regardless,
 * since ground truth's OWN `CFsConverterNormal::SetFilterCoeffs(int,int,
 * float,int,int)` (its un-overridden base slot) is *itself* a literal
 * 1-byte `return;` no-op in the shipped binary (confirmed via `objdump -dr`)
 * -- i.e. using `CFsConverterNormal` on its own (not through
 * `CFsCwInterpolation`) already does nothing here in ground truth too, this
 * reconstruction is not taking a shortcut relative to that path.
 *
 * SRingBufState (`CFsConverterNormal`'s own `+0x70` heap block, plain
 * `operator new(0x874)` -- NOT a constructed C++ object in ground truth, a
 * raw field-initialized buffer, modeled here the same way): fields below
 * were recovered from the ctor/dtor/`Reset()` bodies, which ARE fully
 * reconstructed and exercise every field this struct declares by name.
 * Fields only touched by the deferred `Process()`/`SetFilterCoeffs(5-arg)`
 * bodies (`+0x830` block-size, `+0x838` decimation factor, `+0x83c/0x840/
 * 0x844` -- ctor-initialized, real, but not read back by anything this pass
 * implements) are declared for ctor/dtor faithfulness but their deeper
 * consumers stay out of scope alongside `Process()` itself. A trailing
 * `0x2c`-byte region (`+0x848..0x873`) is real allocated space this pass
 * never observed a field write into (only exercised by the deferred
 * methods) -- kept as an explicit `mReserved` tail so `sizeof` matches
 * ground truth's own `operator new(0x874)` exactly.
 */

#ifndef FS_CONVERTER_H
#define FS_CONVERTER_H

#include "kaiser_window.h"

class CDecimationFilterCoeffs {
public:
	/* .text+0x083043d0, 114 bytes. */
	CDecimationFilterCoeffs();

	/* .text+0x08304380 (D1) / 0x083043a0 (D0, frees `this`). */
	virtual ~CDecimationFilterCoeffs();

	/* .text+0x08304160, 50 bytes. Recomputes mSincScale/mSincArg. */
	int SetSampleRate(int sampleRateHz);

	/* .text+0x083041a0, 33 bytes. Recomputes mSincScale/mSincArg. */
	int SetCutoffFreq(int cutoffHz);

	/* .text+0x08304340, 57 bytes. */
	void SetFilterLength(int length);

	/* .text+0x08304300, 24 bytes. Forwards to the embedded window. */
	void SetBesselFunctionLength(int length);

	/* .text+0x08304320, 24 bytes. Forwards to the embedded window. */
	void SetSideLobeAttenuation(double attenuationDb);

	/* .text+0x08304260, 143 bytes. Windowed-sinc coefficient at tap n. */
	virtual double GetFilterCoeff(int n) const;

	/* .text+0x08304200, 10 bytes. */
	virtual int GetDelayOffsetSamples() const;

	/* .text+0x08304210, 40 bytes. Dispatches to GetDelayOffsetSamples(). */
	double GetDelayOffsetSeconds() const;

	/* .text+0x08304240, 27 bytes. Direct field read, NOT virtual-dispatched
	 * (ground truth reads mFilterLength/mInvSampleRate directly rather than
	 * calling GetDelayOffsetSamples()). */
	double GetDelayOffset() const;

protected:
	/* .text+0x083041d0, 33 bytes. mSincScale=2*fc/fs, mSincArg=2*pi*fc/fs. */
	virtual void CalcFreqCoeffs();

	double mCenter;        /* +0x04, sinc center = (mFilterLength-1)/2 */
	double mSincScale;     /* +0x0c, 2*cutoff/sampleRate */
	double mSincArg;       /* +0x14, 2*pi*cutoff/sampleRate */
	int    mFilterLength;  /* +0x1c */
	int    mSampleRate;    /* +0x20 */
	double mInvSampleRate; /* +0x24, 1/sampleRate */
	int    mCutoffFreq;    /* +0x2c */
	CKaiserWindowCoeffs mWindow; /* +0x30, size 0x34 */
};

class COversamplingFilterCoeffs : public CDecimationFilterCoeffs {
public:
	/* .text+0x08306050, 34 bytes. */
	COversamplingFilterCoeffs();

	/* .text+0x08306000 (D1) / 0x08306020 (D0, frees `this`). */
	virtual ~COversamplingFilterCoeffs();

	/* .text+0x08305fb0, 28 bytes. */
	int SetOversamplingRate(int rate);

	/* .text+0x08305fd0, 34 bytes. Base coeff scaled by mOversamplingRate. */
	virtual double GetFilterCoeff(int n) const;

private:
	double mOversamplingRate; /* +0x64 (== "this+100" in the decompile) */
};

/* Per-channel circular delay line + per-phase FIR coefficient table used by
 * CFsConverterNormal's (deferred) Process()/SetFilterCoeffs(5-arg) core --
 * see header comment for provenance/scope of each field. Plain field-
 * initialized buffer, not a constructed C++ object (matches ground truth's
 * own plain `operator new(0x874)`, no placement-new ctor call). */
struct SRingBufState {
	float       *mPhaseCoeffs[0x200]; /* +0x000, per-oversampling-phase FIR taps */
	float       *mChannelRing[8];     /* +0x800, per-channel 1024-float ring buffer */
	unsigned int mNumChannels;        /* +0x820 */
	unsigned int mRingWritePos;       /* +0x824 */
	unsigned int mCarry;              /* +0x828 */
	unsigned int mRingMask;           /* +0x82c, default 0x3ff (1024-1) */
	unsigned int mBlockSize;          /* +0x830, only read by deferred Process() */
	unsigned int mOversamplingRate;   /* +0x834, phase count - 1, capped 0x1ff */
	unsigned int mDecimationFactor;   /* +0x838, only read by deferred Process() */
	float        mUnknown83c;         /* +0x83c, real, unmodeled meaning */
	unsigned int mUnknown840;         /* +0x840, real, unmodeled meaning */
	unsigned int mUnknown844;         /* +0x844, real, unmodeled meaning */
	unsigned char mReserved[0x874 - 0x848]; /* real allocated tail, no field
	                                          * write observed outside the
	                                          * deferred Process()/SetFilterCoeffs() */
};

class CFsConverterNormal {
public:
	/* .text+0x08305060, 768 bytes. numChannels capped to 8. */
	explicit CFsConverterNormal(int numChannels);

	/* .text+0x08304f40 (D1) / 0x08304fd0 (D0, frees `this`). */
	virtual ~CFsConverterNormal();

	/* .text+0x08304460, 134 bytes. Picks a canned 44100/48000 Hz preset and
	 * virtual-dispatches to SetFilterCoeffs(int,int,float,int,int) below. */
	void SetFilterCoeffs(int sampleRateHz);

	/* .text+0x083044f0, 1 byte ("return;"). Real ground-truth base-class
	 * stub -- this class's own (un-overridden) slot genuinely does nothing;
	 * CFsCwInterpolation provides the real override (deferred, see below). */
	virtual void SetFilterCoeffs(int a, int b, float c, int d, int e);

	/* .text+0x08304cd0, 549 bytes. DEFERRED -- see header comment. Real
	 * per-phase coefficient-table builder, only ever reached through
	 * CFsCwInterpolation::SetFilterCoeffs()'s own (also deferred) override. */
	void BuildFilterCoeffTable(int filterLength, int inputRateFactor,
	                             int cutoffFreq, int maxPhases, int decimation);

	/* .text+0x08304500, 1446 bytes. DEFERRED -- see header comment. Real
	 * polyphase-FIR ring-buffer resampling core. Stub always reports "0
	 * samples produced" (a safe, honest default -- never silently fabricates
	 * output). */
	virtual int Process(float **in, float **out, int inCount);

	/* .text+0x08304b10, 446 bytes. Zeroes every channel ring buffer. */
	void Reset();

	/* .text+0x08304ab0/0x08304ad0/0x08304af0, 18 bytes each. Forward to the
	 * embedded mFilterCoeffs (this+4). */
	int GetDelayOffsetSamples() const;
	double GetDelayOffsetSeconds() const;
	double GetDelayOffset() const;

	void SetBesselFunctionLength(int length) { mFilterCoeffs.SetBesselFunctionLength(length); }
	void SetSideLobeAttenuation(double attenuationDb) { mFilterCoeffs.SetSideLobeAttenuation(attenuationDb); }

protected:
	COversamplingFilterCoeffs mFilterCoeffs; /* +0x04, size 0x6c */
	SRingBufState *mState;                    /* +0x70 */
};

class CFsCwInterpolation : public CFsConverterNormal {
public:
	/* .text+0x08305b00, 76 bytes. */
	explicit CFsCwInterpolation(int numChannels);

	/* .text+0x08305aa0 (D1) / 0x08305ad0 (D0, frees `this`). */
	virtual ~CFsCwInterpolation();

	/* .text+0x083059f0, 171 bytes. DEFERRED -- see header comment. Real
	 * override: un-shifts mOversamplingRate, calls the (deferred)
	 * CFsConverterNormal::BuildFilterCoeffTable(), then computes a
	 * fixed-point scale factor and re-shifts mOversamplingRate back. Stub
	 * preserves the shift/un-shift so mOversamplingRate's bit-shifted
	 * encoding (see .cpp) stays internally consistent even though the
	 * coefficient table itself is not built. */
	virtual void SetFilterCoeffs(int a, int b, float c, int d, int e);

	/* .text+0x08305380, 1643 bytes. DEFERRED -- see header comment, same
	 * "always reports 0 samples produced" stub convention as the base. */
	virtual int Process(float **in, float **out, int inCount);
};

#endif /* FS_CONVERTER_H */
