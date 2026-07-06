// SPDX-License-Identifier: GPL-2.0
/*
 * test_wave_sample_convert.cpp  -  KAT for ../src/engine/wave_sample_convert.cpp
 * (batch 26): the 5 plain-C-linkage sample-conversion leaf functions plus
 * USTGHDRUtils::ConvertWaveToSTGSamples() itself.
 *
 * Deliberately lightweight: pokes CSTGPlaybackEvent's raw bytes directly
 * (sampleRate/field1d/the +0x5c gain field) rather than going through any
 * CSTGPlaybackEvent method/ctor -- this file never calls one, so it links
 * ONLY wave_sample_convert.cpp, no vtable placeholder storage needed.
 *
 * Provides its own local mock of USTGHDRUtils::Convert44100WaveToSTGSamples
 * (the one still-deferred sibling, bar2_stubs.cpp's own `{ return 0; }` in
 * the real .ko) purely to verify the 44100Hz forward-dispatch passes all 9
 * arguments through UNCHANGED -- the real resampler body itself is out of
 * scope here (see oa_engine.h's USTGHDRUtils class comment).
 */

#include <cstdio>
#include <cstring>
#include "oa_engine.h"
#include "oa_engine_init.h"

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s 0x%lx\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%lx)\n", want);
}
static void check_float(const char *label, float got, float want)
{
	bool ok = (got > want - 0.0005f) && (got < want + 0.0005f);
	if (!ok)
		g_fail++;
	printf("  %s  %-60s %g\n", ok ? "ok  " : "FAIL", label, (double)got);
	if (!ok)
		printf("        (wanted %g)\n", (double)want);
}

/* The 5 leaf converters are plain C-linkage, each taking a pointer to the
 * real (project-internal, not header-exported) SampleConvertParams struct
 * -- declared here via `void *` (extern "C" linkage only cares about the
 * symbol name at link time, not the declared parameter type) and called
 * with a LOCAL struct whose member order/types are kept in lock-step with
 * wave_sample_convert.cpp's own SampleConvertParams (src/dest/gain/
 * scaleConst/count) so the real callee interprets the same bytes
 * correctly. (GCC 12 rejects `extern "C" ...;` as a block-scope
 * declaration inside a function body -- confirmed via a throwaway repro
 * -- so these must be declared here, at file scope, not inside main().) */
extern "C" void ByteSwapMono2ByteStream(void *);
extern "C" void ByteSwapStereo2ByteStream(void *);
extern "C" void ConvertMono2ByteWaveToSTGSamples(void *);
extern "C" void ConvertStereo2ByteWaveToSISTGSamples(void *);
extern "C" void ConvertMono2ByteWaveToSISTGSamples(void *);

/* ---- local mock for the still-deferred sibling ---- */
static int g_conv44100Calls;
static float *g_conv44100_dest;
static bool g_conv44100_p2, g_conv44100_p3, g_conv44100_p5, g_conv44100_p6;
static char *g_conv44100_src;
static unsigned long g_conv44100_count, g_conv44100_p9;
static CSTGPlaybackEvent *g_conv44100_event;
static unsigned long g_conv44100_retval;

unsigned long USTGHDRUtils::Convert44100WaveToSTGSamples(float *dest, bool p2, bool p3, char *src,
                                                          bool p5, bool p6, unsigned long count,
                                                          CSTGPlaybackEvent *event, unsigned long p9)
{
	g_conv44100Calls++;
	g_conv44100_dest = dest;
	g_conv44100_p2 = p2;
	g_conv44100_p3 = p3;
	g_conv44100_src = src;
	g_conv44100_p5 = p5;
	g_conv44100_p6 = p6;
	g_conv44100_count = count;
	g_conv44100_event = event;
	g_conv44100_p9 = p9;
	return g_conv44100_retval;
}

/* Poke a raw CSTGPlaybackEvent-sized buffer's sampleRate/field1d/gain
 * directly -- no ctor/method call needed for this file's own purposes. */
static void setupEvent(unsigned char *buf, unsigned int sampleRate, unsigned char format, float gain)
{
	memset(buf, 0, 0x68);
	CSTGAudioEvent *base = (CSTGAudioEvent *)buf;
	base->sampleRate = sampleRate;
	base->field1d = format;
	*(float *)(buf + 0x5c) = gain;
}

int main(void)
{
	printf("wave_sample_convert known-answer test\n");
	printf("=========================================================\n");

	printf("[1] ByteSwapMono2ByteStream -- per-sample 16-bit endian swap\n");
	{
		short buf[4] = { (short)0x0102, (short)0x0304, (short)0xff00, (short)0x00ff };
		struct { const short *src; float *dest; float gain; float scaleConst; unsigned long count; } p;
		p.src = buf; p.dest = 0; p.gain = 0; p.scaleConst = 0; p.count = 4;
		ByteSwapMono2ByteStream(&p);
		check_eq("buf[0] byte-swapped", (unsigned short)buf[0], 0x0201);
		check_eq("buf[1] byte-swapped", (unsigned short)buf[1], 0x0403);
		check_eq("buf[2] byte-swapped", (unsigned short)buf[2], 0x00ff);
		check_eq("buf[3] byte-swapped", (unsigned short)buf[3], 0xff00);
	}

	printf("[2] ByteSwapStereo2ByteStream -- swaps count*2 elements\n");
	{
		short buf[4] = { (short)0x0102, (short)0x0304, (short)0x0506, (short)0x0708 };
		struct { const short *src; float *dest; float gain; float scaleConst; unsigned long count; } p;
		p.src = buf; p.dest = 0; p.gain = 0; p.scaleConst = 0; p.count = 2; /* count=2 -> 4 elements swapped */
		ByteSwapStereo2ByteStream(&p);
		check_eq("buf[0] byte-swapped", (unsigned short)buf[0], 0x0201);
		check_eq("buf[1] byte-swapped", (unsigned short)buf[1], 0x0403);
		check_eq("buf[2] byte-swapped", (unsigned short)buf[2], 0x0605);
		check_eq("buf[3] byte-swapped", (unsigned short)buf[3], 0x0807);
	}

	printf("[3] ConvertMono2ByteWaveToSTGSamples -- mix-ADD, not overwrite\n");
	{
		short src[2] = { 16384, -16384 }; /* ~0.5 and ~-0.5 of full scale */
		float dest[2] = { 1.0f, 1.0f };   /* pre-existing values -- must survive as a base to add onto */
		struct { const short *src; float *dest; float gain; float scaleConst; unsigned long count; } p;
		p.src = src; p.dest = dest; p.gain = 2.0f; p.scaleConst = 1.0f / 32767.0f; p.count = 2;
		ConvertMono2ByteWaveToSTGSamples(&p);
		check_float("dest[0] == 1.0 + 16384/32767*2", dest[0], 1.0f + (16384.0f / 32767.0f) * 2.0f);
		check_float("dest[1] == 1.0 + (-16384)/32767*2", dest[1], 1.0f + (-16384.0f / 32767.0f) * 2.0f);
	}

	printf("[4] ConvertStereo2ByteWaveToSISTGSamples -- same 1:1 mix-add formula\n");
	{
		short src[2] = { 32767, -32768 };
		float dest[2] = { 0.0f, 0.0f };
		struct { const short *src; float *dest; float gain; float scaleConst; unsigned long count; } p;
		p.src = src; p.dest = dest; p.gain = 1.0f; p.scaleConst = 1.0f / 32767.0f; p.count = 2;
		ConvertStereo2ByteWaveToSISTGSamples(&p);
		check_float("dest[0] == 32767/32767", dest[0], 1.0f);
		check_float("dest[1] == -32768/32767", dest[1], -32768.0f / 32767.0f);
	}

	printf("[5] ConvertMono2ByteWaveToSISTGSamples -- widen to stereo, -3dB, mix-add both slots\n");
	{
		short src[1] = { 32767 };
		float dest[2] = { 0.25f, 0.25f };
		struct { const short *src; float *dest; float gain; float scaleConst; unsigned long count; } p;
		p.src = src; p.dest = dest; p.gain = 1.0f; p.scaleConst = 1.0f / 32767.0f; p.count = 1;
		ConvertMono2ByteWaveToSISTGSamples(&p);
		float expect = 0.25f + 1.0f * 0.70710677f;
		check_float("dest[0] == 0.25 + 1.0*-3dB (L)", dest[0], expect);
		check_float("dest[1] == 0.25 + 1.0*-3dB (R, duplicated)", dest[1], expect);
	}

	unsigned char evtBuf[0x68];
	CSTGPlaybackEvent *evt = (CSTGPlaybackEvent *)evtBuf;

	printf("[6] ConvertWaveToSTGSamples -- count==0 returns 0, no dispatch\n");
	{
		setupEvent(evtBuf, 48000, 2, 1.0f);
		g_conv44100Calls = 0;
		unsigned long ret = USTGHDRUtils::ConvertWaveToSTGSamples(0, false, false, 0, false, false,
		                                                            0, evt, 0);
		check_eq("return == 0", ret, 0);
		check_eq("Convert44100 not called", g_conv44100Calls, 0);
	}

	printf("[7] ConvertWaveToSTGSamples -- 44100Hz source forwards all 9 args unchanged\n");
	{
		setupEvent(evtBuf, 44100, 2, 1.0f);
		g_conv44100Calls = 0;
		g_conv44100_retval = 0x42;
		float destBuf[4];
		char srcBuf[8];
		unsigned long ret = USTGHDRUtils::ConvertWaveToSTGSamples(destBuf, true, true, srcBuf, true,
		                                                            true, 7, evt, 99);
		check_eq("Convert44100 called exactly once", g_conv44100Calls, 1);
		check_eq("dest forwarded", (unsigned long)(g_conv44100_dest == destBuf), 1);
		check_eq("p2 forwarded", g_conv44100_p2, true);
		check_eq("p3 forwarded", g_conv44100_p3, true);
		check_eq("src forwarded", (unsigned long)(g_conv44100_src == srcBuf), 1);
		check_eq("p5 forwarded", g_conv44100_p5, true);
		check_eq("p6 forwarded", g_conv44100_p6, true);
		check_eq("count forwarded", g_conv44100_count, 7);
		check_eq("event forwarded", (unsigned long)(g_conv44100_event == evt), 1);
		check_eq("p9 forwarded", g_conv44100_p9, 99);
		check_eq("return value passed through", ret, 0x42);
	}

	printf("[8] ConvertWaveToSTGSamples -- format==2, mono planar, no byteswap\n");
	{
		setupEvent(evtBuf, 48000, 2, 2.0f);
		short src[2] = { 16384, -16384 };
		float dest[2] = { 1.0f, 1.0f };
		unsigned long ret = USTGHDRUtils::ConvertWaveToSTGSamples(dest, false, false, (char *)src,
		                                                            false, false, 2, evt, 0);
		check_float("dest[0] mixed correctly", dest[0], 1.0f + (16384.0f / 32767.0f) * 2.0f);
		check_float("dest[1] mixed correctly", dest[1], 1.0f + (-16384.0f / 32767.0f) * 2.0f);
		check_eq("return == (u8)count == 2", ret, 2);
	}

	printf("[9] ConvertWaveToSTGSamples -- format==2, needsByteSwap swaps src first\n");
	{
		setupEvent(evtBuf, 48000, 2, 1.0f);
		/* 16384 stored byte-swapped in memory (big-endian) */
		short srcBE[1];
		((unsigned char *)srcBE)[0] = 0x40; ((unsigned char *)srcBE)[1] = 0x00; /* BE(0x4000)=16384 */
		float dest[1] = { 0.0f };
		USTGHDRUtils::ConvertWaveToSTGSamples(dest, false, false, (char *)srcBE, false, true, 1, evt, 0);
		check_float("byteswap-then-convert produced 16384/32767", dest[0], 16384.0f / 32767.0f);
	}

	printf("[10] ConvertWaveToSTGSamples -- format==2, SI mono-widen dispatch\n");
	{
		setupEvent(evtBuf, 48000, 2, 1.0f);
		short src[1] = { 32767 };
		float dest[2] = { 0.0f, 0.0f };
		USTGHDRUtils::ConvertWaveToSTGSamples(dest, true, false, (char *)src, false, false, 1, evt, 0);
		float expect = 1.0f * 0.70710677f;
		check_float("dest[0] (L, -3dB)", dest[0], expect);
		check_float("dest[1] (R, -3dB, duplicated)", dest[1], expect);
	}

	printf("[11] ConvertWaveToSTGSamples -- format==2, real stereo SI dispatch\n");
	{
		setupEvent(evtBuf, 48000, 2, 1.0f);
		short src[2] = { 32767, -32768 };
		float dest[2] = { 0.0f, 0.0f };
		USTGHDRUtils::ConvertWaveToSTGSamples(dest, true, false, (char *)src, true, false, 2, evt, 0);
		check_float("dest[0] == L", dest[0], 1.0f);
		check_float("dest[1] == R", dest[1], -32768.0f / 32767.0f);
	}

	printf("[12] ConvertWaveToSTGSamples -- format==3 (24-bit), mono planar, OVERWRITE not mix-add\n");
	{
		setupEvent(evtBuf, 48000, 3, 1.0f);
		/* 24-bit LE sample: 0x400000 (=4194304), positive */
		unsigned char src[3] = { 0x00, 0x00, 0x40 };
		float dest[1] = { 999.0f }; /* must be OVERWRITTEN, not added onto */
		USTGHDRUtils::ConvertWaveToSTGSamples(dest, false, false, (char *)src, false, false, 1, evt, 0);
		check_float("dest[0] overwritten to sample/8388607", dest[0], 4194304.0f / 8388607.0f);
	}

	printf("[13] ConvertWaveToSTGSamples -- format==3, SI mono-widen duplicate\n");
	{
		setupEvent(evtBuf, 48000, 3, 1.0f);
		unsigned char src[3] = { 0x00, 0x00, 0x40 };
		float dest[2] = { 111.0f, 222.0f };
		USTGHDRUtils::ConvertWaveToSTGSamples(dest, true, false, (char *)src, false, false, 1, evt, 0);
		float expect = 4194304.0f / 8388607.0f;
		check_float("dest[0] (L)", dest[0], expect);
		check_float("dest[1] (R, duplicated)", dest[1], expect);
	}

	printf("[14] ConvertWaveToSTGSamples -- format==3, real stereo SI, needsByteSwap (big-endian)\n");
	{
		setupEvent(evtBuf, 48000, 3, 1.0f);
		/* two BE 24-bit samples: 0x400000 then -0x400000 (0xC00000 two's complement in 24 bits) */
		unsigned char src[6] = { 0x40, 0x00, 0x00, /* BE(0x400000) */
		                          0xC0, 0x00, 0x00 /* BE(0xC00000) = -0x400000 */ };
		float dest[2] = { 0.0f, 0.0f };
		/* count=2 (stereo pair total elements) -> loopN = count>>1 = 1 */
		USTGHDRUtils::ConvertWaveToSTGSamples(dest, true, false, (char *)src, true, true, 2, evt, 0);
		check_float("dest[0] (L) == 0x400000/8388607", dest[0], 4194304.0f / 8388607.0f);
		check_float("dest[1] (R) == -0x400000/8388607", dest[1], -4194304.0f / 8388607.0f);
	}

	printf("[15] ConvertWaveToSTGSamples -- return-value truncation quirk (count&0xff)\n");
	{
		setupEvent(evtBuf, 48000, 2, 0.0f); /* gain 0 -> conversion writes zero, don't care about dest */
		short src[257];
		memset(src, 0, sizeof(src));
		float dest[257];
		memset(dest, 0, sizeof(dest));
		unsigned long ret = USTGHDRUtils::ConvertWaveToSTGSamples(dest, false, false, (char *)src,
		                                                            false, false, 257, evt, 0);
		check_eq("return == count & 0xff == 1 (NOT 257)", ret, 1);
	}

	printf("[16] ConvertWaveToSTGSamples -- invalid format (1 == 8-bit, unhandled): no-op\n");
	{
		setupEvent(evtBuf, 48000, 1, 1.0f);
		short src[1] = { 12345 };
		float dest[1] = { 7.0f };
		unsigned long ret = USTGHDRUtils::ConvertWaveToSTGSamples(dest, false, false, (char *)src,
		                                                            false, false, 3, evt, 0);
		check_float("dest untouched", dest[0], 7.0f);
		check_eq("return == (u8)count == 3", ret, 3);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
