/*
 * fs_converter.h  -  CDecimationFilterCoeffs / COversamplingFilterCoeffs /
 * CFsConverterNormal / CFsCwInterpolation, Eva's windowed-sinc sample-rate
 * conversion family. See kaiser_window.h for the Kaiser-window math these
 * build on, and pcm_filter.h for the sibling bit-depth/gain PCM utility
 * class found in the same sweep.
 *
 * UPDATE 2026-07-28 (later same-day pass): `CFsConverterNormal::
 * SetFilterCoeffs(int,int,int,int,int)` (0x08304cd0, called
 * `BuildFilterCoeffTable` below for readability) and `CFsCwInterpolation::
 * SetFilterCoeffs(int,int,float,int,int)` (0x083059f0) are now RECONSTRUCTED
 * -- both turned out fully tractable once decoded end to end via
 * `objdump -dr -M intel`. Only `Process()` (both overrides, the actual
 * ring-buffer convolution inner loop) stays deferred; see its own comment
 * below for the now much more precisely documented reason.
 *
 * Correction to the original pass's class layout: `BuildFilterCoeffTable()`
 * (real name `SetFilterCoeffs(int,int,int,int,int)`, an all-int overload
 * distinct from the float-arg one) is NOT a plain non-virtual helper as
 * first declared -- a direct `.rodata` vtable dump (`vtable for
 * CFsConverterNormal` at `.rodata+0x08f31280`, `vtable for
 * CFsCwInterpolation` at `.rodata+0x08f312e0`) shows it occupies slot 5
 * (`vptr+0x14`) in BOTH classes' vtables at the SAME address (0x08304cd0)
 * -- i.e. it is virtual but `CFsCwInterpolation` does not override it. The
 * same dump also resolved 3 previously-unidentified/mis-declared slots:
 * slot 2 (`vptr+0x08`) is `SetSideLobeAttenuation(double)`'s own virtual
 * thin-wrapper body (0x08304f20, confirmed by direct disassembly -- adjusts
 * `this` by +4 to `&mFilterCoeffs` then tail-jumps to
 * `CDecimationFilterCoeffs::SetSideLobeAttenuation`), slot 3 (`vptr+0x0c`)
 * is the same pattern for `SetBesselFunctionLength(int)` (0x08304f00), and
 * slot 8 (`vptr+0x20`) is `Reset()` (0x08304b10) -- also virtual, also not
 * overridden by `CFsCwInterpolation`. All three are marked `virtual` below
 * now; this does not change observable behavior for this project's own
 * test suite (neither derived class overrides any of them) but matches
 * ground truth's real vtable shape. Full 9-slot layout, both classes
 * (D1 dtor, D0 dtor, SetSideLobeAttenuation, SetBesselFunctionLength,
 * SetFilterCoeffs(int), SetFilterCoeffs(int,int,int,int,int), SetFilterCoeffs
 * (int,int,float,int,int), Process, Reset) -- confirmed byte-for-byte via
 * `objdump -s --start-address=0x08f31280 --stop-address=0x08f31340 Eva`.
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
 * RECONSTRUCTED this pass (2026-07-28, both fully decoded via `objdump -dr
 * -M intel` -- see fs_converter.cpp for the bodies):
 *
 *   - `CFsConverterNormal::SetFilterCoeffs(int,int,int,int,int)` (549 bytes,
 *     0x08304cd0, VIRTUAL -- see correction note above; internal name
 *     `BuildFilterCoeffTable` below) -- frees the old `mState->mPhaseCoeffs[
 *     0..mOversamplingRate]` array, clamps the new phase count to
 *     `min(maxPhases, 0x1ff)`, stores it + `decimation` into
 *     `mState->mOversamplingRate`/`mDecimationFactor`, re-derives
 *     `mFilterCoeffs`'s oversampling rate/filter length/sample rate/cutoff,
 *     computes `mState->mBlockSize = ((R+filterLength-1)/R + 3) & ~3` (R =
 *     new `mOversamplingRate`, SIMD-friendly-multiple-of-4 per-phase tap
 *     count), then for each phase `j` in `[0, R]` allocates `mBlockSize`
 *     floats, zeroes them, and fills taps `k=0,1,...` from
 *     `mFilterCoeffs.GetFilterCoeff(n)` for `n = j, j+R, j+2R, ...` while
 *     `n < filterLength` (GCC 4-way-unrolled in ground truth, semantically
 *     a plain strided loop). Ends with a **tail call to the virtual
 *     `Reset()`** (`jmp [vtable+0x20]`, confirmed slot 8 == `Reset()`) --
 *     i.e. ground truth's own `return this->Reset();`, reproduced here as a
 *     plain `Reset();` call since the C++ virtual-dispatch is automatic.
 *   - `CFsCwInterpolation::SetFilterCoeffs(int,int,float,int,int)` (171
 *     bytes, 0x083059f0, virtual override of `CFsConverterNormal`'s slot 6)
 *     -- un-shifts `mState->mOversamplingRate` (`>>= 14`, this class stores
 *     it left-shifted by 14 bits between calls -- see ctor/dtor), calls
 *     `CFsConverterNormal::SetFilterCoeffs(a, b, d, e, 1)` (decimation
 *     hardcoded to 1 through this path; note the int-only overload's 3rd/4th
 *     params -- cutoffFreq/maxPhases -- receive this override's `d`/`e`, NOT
 *     its `c`), then computes `mState->mDecimationFactor = (int)((double)
 *     (b * mOversamplingRate_new) / (double)c * 16384.0)` (Q14 fixed-point;
 *     ground truth's 14x `fadd st,st(0)` self-doublings are bit-exact
 *     equivalent to `* 16384.0`, and `fisttp` truncates toward zero exactly
 *     like a C `(int)` cast), re-shifts `mOversamplingRate <<= 14`, and
 *     itself ends with a second tail call to `Reset()` -- meaning `Reset()`
 *     genuinely runs TWICE per call through this path (once via the base
 *     method's own tail call, once again here); reproduced faithfully even
 *     though it looks redundant, since that really is what ground truth
 *     does.
 *
 * DEFERRED, this pass (the real polyphase-FIR convolution core; matching
 * the "carve out the hardest outlier, document why, keep the rest"
 * precedent `stg_unsol_msg_handler.h`/`korg_file.h` already established
 * elsewhere):
 *
 *   - `CFsConverterNormal::Process(float**,float**,int)` (1446 bytes,
 *     0x08304500) / `CFsCwInterpolation::Process(float**,float**,int)`
 *     (1643 bytes, 0x08305380) -- the actual polyphase-FIR ring-buffer
 *     resampling inner loops. Traced further this pass than before: outer
 *     loop is per-channel (`mState->mNumChannels`); per channel, an inner
 *     loop first mixes new input samples into `mChannelRing[ch]` at
 *     `mRingWritePos & mRingMask` while a running `inPos` (samples consumed
 *     from `in[ch]` this call) stays within `mState->mCarry` (an input-vs-
 *     output-phase leftover counter carried across calls, read once per
 *     channel from `mState->mCarry` -- same starting value for every
 *     channel since input advances in lockstep), 8-way-Duff's-device-
 *     unrolled; once the ring is filled enough, a second inner loop computes
 *     the actual FIR sum for the current output sample via `mPhaseCoeffs[
 *     phase][tap] * mChannelRing[ch][(writeIdx-tap) & mRingMask]` --
 *     GCC emits a bespoke unrolled-by-1..7 "remainder" dispatch (`cmp
 *     ebp,0..6; je ...`) followed by an 8-way main loop, both accumulating
 *     into a single x87 `st(0)` via `fmul`+`faddp` chains (no true SSE
 *     vectorization found on closer inspection -- the header's original
 *     "partially SSE-auto-vectorized" note was an overstatement from a
 *     shallower first pass). `mState->mCarry`/`mRingWritePos` are written
 *     back once at the very end, after all channels. Zero-channel case is a
 *     real, separate early-out (`mNumChannels==0` -> resets `mCarry`/
 *     `mRingWritePos` to 0, returns 0 samples). Genuinely well-understood in
 *     outline now (see `SRingBufState` below for the full field layout, and
 *     this comment for the loop structure), but the exact tap/ring index
 *     arithmetic across 3 nested loops -- and getting the phase-advance
 *     stride and the ring wraparound sign/direction bit-exact -- is still
 *     high-risk (a single off-by-one silently produces plausible-sounding
 *     but wrong audio, and this project has no golden reference PCM stream
 *     to check against) and stays out of scope for this pass. What would
 *     unblock it: a dedicated follow-up with a synthesized known-answer
 *     input (e.g. an impulse or a single sinusoid at a known frequency)
 *     compared against a from-scratch reference polyphase resampler
 *     implementation, not just re-deriving the assembly by inspection.
 *
 * `CFsConverterNormal::SetFilterCoeffs(int)` (134 bytes, the *virtual
 * dispatcher* used by real callers, NOT deferred, unchanged this pass)
 * still gets a real body: picks a canned coefficient-table preset for
 * 44100/48000 Hz and virtual-dispatches to the (now real)
 * `SetFilterCoeffs(int,int,float,int,int)` override -- `CFsConverterNormal`
 * used on its own (not through `CFsCwInterpolation`) still ends up calling
 * its own un-overridden slot 6, which really is a literal 1-byte `return;`
 * no-op in ground truth (confirmed via `objdump -dr`); that path was never
 * in scope to change.
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
 * CFsConverterNormal. mPhaseCoeffs[]/mBlockSize/mOversamplingRate/
 * mDecimationFactor are now written by the reconstructed SetFilterCoeffs()
 * pair; mCarry/mRingWritePos/mChannelRing[]/mRingMask are read+written by
 * the still-deferred Process() (see header comment). Plain field-
 * initialized buffer, not a constructed C++ object (matches ground truth's
 * own plain `operator new(0x874)`, no placement-new ctor call). */
struct SRingBufState {
	float       *mPhaseCoeffs[0x200]; /* +0x000, per-phase FIR taps, mBlockSize floats
	                                    * each, allocated/filled by SetFilterCoeffs() */
	float       *mChannelRing[8];     /* +0x800, per-channel 1024-float ring buffer,
	                                    * read/written only by deferred Process() */
	unsigned int mNumChannels;        /* +0x820 */
	unsigned int mRingWritePos;       /* +0x824, only touched by deferred Process()/Reset() */
	unsigned int mCarry;              /* +0x828, input/output phase carry across Process()
	                                    * calls, only touched by deferred Process()/Reset() */
	unsigned int mRingMask;           /* +0x82c, default 0x3ff (1024-1) */
	unsigned int mBlockSize;          /* +0x830, per-phase tap count, = ((R+filterLength-1)/R
	                                    * + 3) & ~3 (R=mOversamplingRate); set by SetFilterCoeffs() */
	unsigned int mOversamplingRate;   /* +0x834, phase count, clamped to [0,0x1ff]; set by
	                                    * SetFilterCoeffs(). CFsCwInterpolation stores this
	                                    * left-shifted by 14 bits between calls (see its ctor/dtor/
	                                    * SetFilterCoeffs()) -- unshifted only while
	                                    * CFsConverterNormal::SetFilterCoeffs() itself runs */
	unsigned int mDecimationFactor;   /* +0x838, set by SetFilterCoeffs(): plain `decimation` int
	                                    * through CFsConverterNormal (always 1 through
	                                    * CFsCwInterpolation), then overwritten by
	                                    * CFsCwInterpolation::SetFilterCoeffs() with a real Q14
	                                    * fixed-point interpolation scale factor; only read by
	                                    * deferred Process() */
	float        mUnknown83c;         /* +0x83c, real, unmodeled meaning */
	unsigned int mUnknown840;         /* +0x840, real, unmodeled meaning */
	unsigned int mUnknown844;         /* +0x844, real, unmodeled meaning */
	unsigned char mReserved[0x874 - 0x848]; /* real allocated tail, no field
	                                          * write observed outside the
	                                          * deferred Process() */
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
	 * CFsCwInterpolation provides the real override (reconstructed, see
	 * below and fs_converter.cpp). */
	virtual void SetFilterCoeffs(int a, int b, float c, int d, int e);

	/* .text+0x08304cd0, 549 bytes. RECONSTRUCTED 2026-07-28 -- see header
	 * comment. VIRTUAL (vtable slot 5, confirmed via .rodata dump; not
	 * overridden by CFsCwInterpolation, which reaches it through the base
	 * class). Real per-phase coefficient-table builder -- only ever reached
	 * through CFsCwInterpolation::SetFilterCoeffs()'s own override in
	 * practice, but is itself a genuine standalone virtual method. Renamed
	 * from the mangled overload's generic "SetFilterCoeffs" for readability
	 * (see fs_converter.cpp for the real _ZN...SetFilterCoeffsEiiiii body). */
	virtual void BuildFilterCoeffTable(int filterLength, int inputRateFactor,
	                             int cutoffFreq, int maxPhases, int decimation);

	/* .text+0x08304500, 1446 bytes. DEFERRED -- see header comment. Real
	 * polyphase-FIR ring-buffer resampling core. Stub always reports "0
	 * samples produced" (a safe, honest default -- never silently fabricates
	 * output). */
	virtual int Process(float **in, float **out, int inCount);

	/* .text+0x08304b10, 446 bytes. VIRTUAL (vtable slot 8, confirmed via
	 * .rodata dump; not overridden by CFsCwInterpolation). Zeroes every
	 * channel ring buffer. Also reached via virtual dispatch as a tail call
	 * from the end of both SetFilterCoeffs() overloads above -- see their
	 * own comments. */
	virtual void Reset();

	/* .text+0x08304ab0/0x08304ad0/0x08304af0, 18 bytes each. Forward to the
	 * embedded mFilterCoeffs (this+4). NOT virtual (not in the vtable dump). */
	int GetDelayOffsetSamples() const;
	double GetDelayOffsetSeconds() const;
	double GetDelayOffset() const;

	/* .text+0x08304f20/0x08304f00, 15/14 bytes. VIRTUAL (vtable slots 2/3,
	 * confirmed via .rodata dump + direct disassembly -- both are real
	 * tail-jump thin wrappers to CDecimationFilterCoeffs's own methods via
	 * &mFilterCoeffs, not compiler-elided inline calls). Not overridden by
	 * CFsCwInterpolation. */
	virtual void SetSideLobeAttenuation(double attenuationDb) { mFilterCoeffs.SetSideLobeAttenuation(attenuationDb); }
	virtual void SetBesselFunctionLength(int length) { mFilterCoeffs.SetBesselFunctionLength(length); }

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

	/* .text+0x083059f0, 171 bytes. RECONSTRUCTED 2026-07-28 -- see header
	 * comment. Real override: un-shifts mOversamplingRate, calls the (now
	 * real) CFsConverterNormal::BuildFilterCoeffTable(), computes the real
	 * Q14 fixed-point interpolation scale factor into mDecimationFactor,
	 * re-shifts mOversamplingRate, then itself tail-calls Reset() a second
	 * time (see header comment -- Reset() genuinely runs twice per call). */
	virtual void SetFilterCoeffs(int a, int b, float c, int d, int e);

	/* .text+0x08305380, 1643 bytes. DEFERRED -- see header comment, same
	 * "always reports 0 samples produced" stub convention as the base. */
	virtual int Process(float **in, float **out, int inCount);
};

#endif /* FS_CONVERTER_H */
