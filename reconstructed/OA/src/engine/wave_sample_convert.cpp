// SPDX-License-Identifier: GPL-2.0
/*
 * wave_sample_convert.cpp  -  batch 26: the plain-C-linkage sample-format
 * conversion cluster that USTGHDRUtils::ConvertWaveToSTGSamples() dispatches
 * through, plus the dispatcher itself.
 *
 * Pre-scouted by batch 25 (sec 10.173) as CSTGPlaybackBuffer::
 * ProcessSubRate()'s own last remaining external dependency. Ground-truthed
 * via objdump -dr against /home/share/Decomp/OA.ko_Decomp/OA.ko:
 *   ByteSwapMono2ByteStream                  .text+0xd2444,  63B (SSE2)
 *   ByteSwapStereo2ByteStream                .text+0xd2483,  65B (SSE2)
 *   ConvertMono2ByteWaveToSTGSamples          .text+0xd24c4, 105B (SSE2)
 *   ConvertMono2ByteWaveToSISTGSamples        .text+0xd252d, 164B (SSE2)
 *   ConvertStereo2ByteWaveToSISTGSamples      .text+0xd25d1, 105B (SSE2)
 *   USTGHDRUtils::ConvertWaveToSTGSamples     .text+0xd37a0, 581B
 *
 * All five leaf functions have PLAIN C-linkage (no C++ mangling at all --
 * confirmed via `nm OA.ko`, not `nm -C`: `ByteSwapMono2ByteStream` etc show
 * up unmangled), unlike everything else in this cluster -- called via
 * plain `call FunctionName` from ConvertWaveToSTGSamples, each taking a
 * single pointer argument in %eax (this project's global -mregparm=3
 * convention already makes a lone pointer argument land in eax with no
 * extra attribute needed).
 *
 * IMPORTANT BUILD-FLAG GOTCHA (a NEW angle on the sec 10.117/10.151 "this
 * kernel build is -msoft-float -mno-sse -mno-mmx -mno-sse2" family): the
 * five leaf functions above are real SSE2 code in the original binary
 * (movaps/pshufd/cvtdq2ps/punpckhwd/punpcklwd/mulps/addps/pcmpgtw -- GENUINE
 * vector arithmetic, not the "SSE used for a wide copy" case that's a
 * simple byte-loop substitution, sec 10.162). Unlike the CSTGEQ/SetBand
 * precedent (still deferred, sec 10.162/10.170/10.173, because it ALSO has
 * unresolved callees), every one of these five operates on a SINGLE local
 * conversion "job" struct with no other external dependency, and its
 * per-element math (sign-extend int16 -> int32 -> float, scale, optionally
 * duplicate for mono->stereo widen, mix-add into an existing float buffer)
 * is fully expressible as an ordinary scalar loop with IDENTICAL per-sample
 * VALUES (this project has never targeted byte-identical machine code, only
 * functional correctness backed by a real KAT, matching the project's own
 * established "reimplement genuinely-simple SIMD as scalar" precedent --
 * distinct from SetBand, which stays deferred for its OWN callees, not its
 * SSE usage). Written as plain scalar C++, but -- confirmed via an actual
 * `-m32 -mregparm=3 -msoft-float` throwaway compile before wiring this
 * file into the Makefile at all -- plain scalar float multiply/add still
 * pulls in unresolvable libgcc soft-float helpers (`__mulsf3`/`__addsf3`/
 * `__floatsisf`) under this kernel build's own default `-msoft-float`,
 * exactly the sec 10.117/engine_startup_bits.cpp family of gotcha. Fixed
 * the same established way: `CFLAGS_wave_sample_convert.o := -mhard-float
 * -msse2 -mfpmath=sse` (Makefile), confirmed via the same throwaway
 * compile to leave zero unresolved libgcc symbols once applied.
 *
 * The internal "conversion job" struct below is a NEW, project-invented
 * type -- it corresponds to a real local 16-byte-aligned STACK struct
 * ConvertWaveToSTGSamples() builds on its own frame (confirmed real
 * offsets: +0x10 source pointer, +0x18 dest pointer, +0x20 gain float,
 * +0x24 per-format scale float, +0x28 element count), but since it is
 * NEVER touched by anything outside this one translation unit (built and
 * consumed entirely within ConvertWaveToSTGSamples()'s own call graph, no
 * raw-offset reinterpretation from outside), there is no reason to
 * reproduce those literal byte offsets -- an ordinary named C++ struct is
 * both simpler and exactly as faithful.
 */

#include "oa_engine.h"
#include "oa_engine_init.h"

/* Corresponds to the real local stack struct described above. File-scope
 * (not exported via any header, matching this project's own established
 * per-TU-local-helper convention, e.g. playback_event_methods.cpp's
 * ToU32/FromU32) -- deliberately NOT wrapped in an anonymous namespace,
 * since this project has no precedent for that and it would put the
 * `extern "C"` leaf functions below (which take a pointer to this type)
 * into needless linkage-corner-case territory. */
struct SampleConvertParams {
	const short *src;	/* raw source sample buffer, plain int16 elements */
	float       *dest;	/* destination float buffer (mono-planar or stereo-interleaved) */
	float        gain;	/* event->fieldAt(0x5c), a per-event playback gain */
	float        scaleConst; /* per-format normalization constant (1/32767 for 16-bit) */
	unsigned long count;	/* element count (see each wrapper below for exactly what unit) */
};

/*
 * ByteSwapMono2ByteStream/ByteSwapStereo2ByteStream (real SSE2 bodies
 * confirmed BYTE-FOR-BYTE IDENTICAL except for one `shl %ecx` doubling
 * the element count before an otherwise-shared loop -- both derived from
 * the SAME real per-dword byte-swap trick: `pslld $8`/`psrld $8` plus a
 * pair of confirmed real masks (`evenByteMask` = 0x00FF00FF,
 * `oddByteMask` = 0xFF00FF00, both read directly from .rodata.asm) swap
 * the two bytes of each packed 16-bit sample in place -- i.e. this is a
 * plain per-sample 16-bit endian swap over N samples, N being `count` for
 * the mono form and `count*2` for the stereo form (stereo interleaves two
 * channels' worth of int16 elements in one flat buffer, so "twice as many
 * elements" is the whole difference between the two real functions).
 */
static void ByteSwapS16Buffer(short *buf, unsigned long n)
{
	for (unsigned long i = 0; i < n; i++) {
		unsigned short v = (unsigned short)buf[i];
		buf[i] = (short)((v << 8) | (v >> 8));
	}
}

/*
 * ConvertMono2ByteWaveToSTGSamples/ConvertStereo2ByteWaveToSISTGSamples
 * (real SSE2 bodies confirmed BYTE-FOR-BYTE IDENTICAL -- both are a plain
 * 1:1 "int16 -> sign-extend -> float -> scale -> mix-add into dest[i]"
 * loop over `count` elements; the "mono vs stereo" and "planar vs
 * stereo-interleaved" distinction lives entirely in how the CALLER
 * prepares src/dest/count, never inside this shared per-element math).
 * Real per-element formula: dest[i] += (float)src[i] * scaleConst * gain
 * (a genuine ADD-into-existing-buffer mix, confirmed via the real
 * `movups`-load-old-dest-then-`addps`-then-store sequence -- NOT a plain
 * overwrite).
 */
static void ConvertS16ToFloat1to1MixAdd(const SampleConvertParams *p)
{
	float k = p->scaleConst * p->gain;
	for (unsigned long i = 0; i < p->count; i++)
		p->dest[i] += (float)p->src[i] * k;
}

/*
 * allMinus3dB -- real read-only SSE broadcast constant (.rodata.asm+0xb0,
 * 4 identical lanes), confirmed value 0.70710677f == 1/sqrt(2) (a genuine
 * -3dB attenuation factor). Applied only by ConvertMono2ByteWaveToSISTGSamples
 * below, to avoid perceived loudness doubling when a mono source is
 * duplicated into both channels of a stereo mix. Declared as a plain
 * (non-const) 4-float array purely to mirror this project's own
 * established `allPlusOne`/`allMinusOne` real-constant-array convention
 * (cdrom_check.cpp/global.cpp/bar2_stubs_c.cpp) -- nothing in the real
 * binary's own reachable call graph for this cluster ever writes it.
 */
extern "C" float allMinus3dB[4];
float allMinus3dB[4] = { 0.70710677f, 0.70710677f, 0.70710677f, 0.70710677f };

extern "C" void ByteSwapMono2ByteStream(SampleConvertParams *p)
{
	ByteSwapS16Buffer(const_cast<short *>(p->src), p->count);
}

extern "C" void ByteSwapStereo2ByteStream(SampleConvertParams *p)
{
	ByteSwapS16Buffer(const_cast<short *>(p->src), p->count * 2);
}

extern "C" void ConvertMono2ByteWaveToSTGSamples(SampleConvertParams *p)
{
	ConvertS16ToFloat1to1MixAdd(p);
}

extern "C" void ConvertStereo2ByteWaveToSISTGSamples(SampleConvertParams *p)
{
	ConvertS16ToFloat1to1MixAdd(p);
}

/*
 * ConvertMono2ByteWaveToSISTGSamples (real SSE2 body, 164 bytes): same
 * int16->float->scale pipeline as the shared helper above, but the
 * combined multiplier is ADDITIONALLY scaled by `allMinus3dB` (confirmed
 * real extra `mulps 0x0(allMinus3dB)` right after the gain*scaleConst
 * multiply, only in THIS function), and every converted value is written
 * to TWO consecutive destination floats (confirmed real `pshufd $0x50`/
 * `$0xfa` lane-duplicate shuffles) -- i.e. mono source widened into a
 * stereo-interleaved destination, L == R == src[i] * scaleConst * gain *
 * (1/sqrt(2)), both slots mix-added (not overwritten) into the existing
 * dest buffer.
 */
extern "C" void ConvertMono2ByteWaveToSISTGSamples(SampleConvertParams *p)
{
	float k = p->scaleConst * p->gain * allMinus3dB[0];
	for (unsigned long i = 0; i < p->count; i++) {
		float v = (float)p->src[i] * k;
		p->dest[2 * i]     += v;
		p->dest[2 * i + 1] += v;
	}
}

/*
 * USTGHDRUtils::ConvertWaveToSTGSamples (`.text+0xd37a0`, 581 bytes) --
 * the real dispatcher. Confirmed regparm(3) argument order (this
 * project's global -mregparm=3 convention, first 3 integer-class args in
 * eax/edx/ecx): dest(eax), stereoInterleavedOutput(edx),
 * resamplerReservedFlag(ecx), then src/sourceIsStereo/needsByteSwap/count/
 * event/reservedArg9 on the stack in that order.
 *
 * Confirmed real dispatch (event's own already-named CSTGAudioEvent-prefix
 * fields, see playback_event_methods.cpp's own header comment for their
 * provenance -- `field1d` = "per-frame byte-size multiplier", already
 * confirmed there; this batch adds the further confirmed understanding
 * that it ALSO gates format here: 1 = 8-bit PCM (unhandled/no-op in this
 * function), 2 = 16-bit PCM (SSE-accelerated leaf functions above),
 * 3 = 24-bit PCM (inline FPU loop below), any other value = no-op):
 *
 *   if (count == 0) return 0;
 *   if (event->sampleRate == 44100)
 *       return Convert44100WaveToSTGSamples(...all 9 args, unchanged...);
 *
 *   format = event->field1d;
 *   if (format == 2) { dispatch to one of the 3 SSE leaf converters above,
 *                       optionally byte-swapping first; return (u8)count; }
 *   if (format == 3) { inline 24-bit conversion loop below; return (u8)count; }
 *   format 0/1/other: no-op; return (u8)count;
 *
 * CONFIRMED REAL QUIRK, faithfully reproduced (not a guess): on every path
 * except count==0 and the 44100Hz forward, the return value is the
 * ORIGINAL `count` parameter truncated to its LOW BYTE (a bare
 * `movzbl 0x14(%ebp),%edx` reload of the stack argument, not a running
 * tally of samples actually written) -- confirmed load-bearing at the one
 * real caller, CSTGPlaybackBuffer::ProcessSubRate() (still deferred, see
 * oa_engine.h), which does `movzbl %al,%eax` on the result and uses it
 * directly as "samples consumed this call" to decrement a remaining-count
 * and compute an advance-by-bytes value -- i.e. every real caller keeps
 * `count <= 255` for a single call (consistent with the fixed 4096-byte
 * `CSTGPlaybackBuffer::sConvertBuffer` scratch area backing every real
 * invocation).
 *
 * 16-bit (format==2) dispatch table, confirmed via 3 independent boolean
 * tests (stereoInterleavedOutput/sourceIsStereo/needsByteSwap), matching
 * real jump targets exactly:
 *   !stereoInterleavedOutput                            -> [byteswap] + ConvertMono2ByteWaveToSTGSamples
 *    stereoInterleavedOutput && !sourceIsStereo          -> [byteswap] + ConvertMono2ByteWaveToSISTGSamples
 *    stereoInterleavedOutput &&  sourceIsStereo          -> [byteswap-stereo] + ConvertStereo2ByteWaveToSISTGSamples
 * ("[byteswap]" = call the matching ByteSwap*2ByteStream helper first,
 * in place on `src`, only when needsByteSwap is set.)
 *
 * 24-bit (format==3) inline loop, confirmed real per-sample formula
 * `sample * (1/8388607) * gain` (the exact same .rodata.cst4 constant,
 * `1/8388607`, confirmed used for BOTH samples of a stereo pair -- not two
 * different constants as an early read of the disassembly first
 * suggested), OVERWRITING dest (confirmed: no old-value load before the
 * `fsts`/`fstps` store here, unlike the 16-bit SSE mix-add helpers above
 * -- a real, confirmed format-dependent difference, not simplified away).
 * Iteration count is `count` for a mono source, `count>>1` for a stereo
 * source (source already interleaved, one 24-bit triplet per channel per
 * iteration). Byte order: `needsByteSwap==0` reads little-endian
 * (src[0]=LSB..src[2]=signed MSB), `==1` reads big-endian (src[0]=signed
 * MSB..src[2]=LSB) -- applied independently to each of the (up to) two
 * per-iteration reads. Output width per iteration mirrors the 16-bit
 * dispatch exactly: mono-planar writes 1 float (no duplicate), mono
 * source + stereoInterleavedOutput duplicates the single decoded value
 * into both dest slots, real-stereo source + stereoInterleavedOutput
 * writes the two independently-decoded L/R values.
 */
static long Decode24LE(const char *s)
{
	unsigned char b0 = (unsigned char)s[0];
	unsigned char b1 = (unsigned char)s[1];
	signed char   b2 = s[2];
	return (long)b0 | ((long)b1 << 8) | ((long)b2 << 16);
}

static long Decode24BE(const char *s)
{
	signed char   b0 = s[0];
	unsigned char b1 = (unsigned char)s[1];
	unsigned char b2 = (unsigned char)s[2];
	return (long)b2 | ((long)b1 << 8) | ((long)b0 << 16);
}

unsigned long USTGHDRUtils::ConvertWaveToSTGSamples(float *dest, bool stereoInterleavedOutput,
                                                     bool resamplerReservedFlag, char *src,
                                                     bool sourceIsStereo, bool needsByteSwap,
                                                     unsigned long count, CSTGPlaybackEvent *event,
                                                     unsigned long reservedArg9)
{
	if (count == 0)
		return 0;

	CSTGAudioEvent *base = (CSTGAudioEvent *)event;
	if (base->sampleRate == 44100)
		return Convert44100WaveToSTGSamples(dest, stereoInterleavedOutput, resamplerReservedFlag,
		                                     src, sourceIsStereo, needsByteSwap, count, event,
		                                     reservedArg9);

	unsigned char format = base->field1d;
	static const float kFormatScale[4] = { 0.0f, 1.0f / 127.0f, 1.0f / 32767.0f, 1.0f / 8388607.0f };

	if (format == 2) {
		SampleConvertParams conv;
		conv.src = (const short *)src;
		conv.dest = dest;
		conv.scaleConst = kFormatScale[2];
		conv.gain = *(const float *)((const unsigned char *)event + 0x5c);
		conv.count = count;

		if (!stereoInterleavedOutput) {
			if (needsByteSwap)
				ByteSwapMono2ByteStream(&conv);
			ConvertMono2ByteWaveToSTGSamples(&conv);
		} else if (!sourceIsStereo) {
			if (needsByteSwap)
				ByteSwapMono2ByteStream(&conv);
			ConvertMono2ByteWaveToSISTGSamples(&conv);
		} else {
			if (needsByteSwap)
				ByteSwapStereo2ByteStream(&conv);
			ConvertStereo2ByteWaveToSISTGSamples(&conv);
		}
		return (unsigned char)count;
	}

	if (format == 3) {
		unsigned long loopN = sourceIsStereo ? (count >> 1) : count;
		if ((long)loopN <= 0)
			return (unsigned char)count;

		float gain = *(const float *)((const unsigned char *)event + 0x5c);
		float k = kFormatScale[3] * gain;
		const char *s = src;
		float *d = dest;

		for (unsigned long i = 0; i < loopN; i++) {
			long sample1 = needsByteSwap ? Decode24BE(s) : Decode24LE(s);
			s += 3;
			float v1 = (float)sample1 * k;
			*d++ = v1;

			if (!stereoInterleavedOutput)
				continue;
			if (!sourceIsStereo) {
				*d++ = v1;	/* mono->stereo widen: duplicate L into R */
				continue;
			}
			long sample2 = needsByteSwap ? Decode24BE(s) : Decode24LE(s);
			s += 3;
			*d++ = (float)sample2 * k;
		}
		return (unsigned char)count;
	}

	/* format 0/1/other: confirmed no-op in the real binary. */
	return (unsigned char)count;
}
