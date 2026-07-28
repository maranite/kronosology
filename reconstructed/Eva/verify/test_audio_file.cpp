/*
 * test_audio_file.cpp  -  host-side known-answer test for the CAudioFile
 * family (src/base/audio_file.cpp): CAudioFile (abstract base),
 * CAudioFileRead, CAudioFileReadEx, CAudioFileWrite. See
 * include/audio_file.h for full ground-truth provenance.
 *
 * Links against src/base/file_operation_stub.cpp, a REAL host-functional
 * CFileOperation backed by stdio -- writes a real WAV file, reads it back,
 * and compares sample data exactly (genuine round-trip, not just pure-logic
 * checks).
 */

#include <cstdio>
#include <cstring>

#include "audio_file.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

static void WavRoundTrip16Bit()
{
	const char *path = "/tmp/eva_test_audio_file_16bit.wav";
	const unsigned long kFrames = 100;
	const int kChannels = 2;

	long left[kFrames], right[kFrames];
	for (unsigned long i = 0; i < kFrames; ++i) {
		left[i] = (long)i * 100 - 2000;
		right[i] = (long)i * -50 + 500;
	}

	{
		/* Small buffer (64 bytes / 16 bytes-per-frame(?) -- actually
		 * blockAlign for 16-bit stereo is 4 bytes/frame, so a 64-byte
		 * buffer holds 16 frames, forcing several flush cycles across
		 * 100 frames. */
		CAudioFileWrite w(64);
		int status = w.OpenFile(path, 0 /* WAV */, kChannels);
		check("WriteFile: OpenFile status==0", status == 0);
		check("WriteFile: SampleRate defaults to 44100", w.GetSampleRate() == 44100);
		check("WriteFile: BitsPerSample defaults to 16", w.GetBitsPerSample() == 16);

		long *chans[2] = { left, right };
		int written = w.WritePcmData(chans, kFrames, kChannels);
		check("WritePcmData wrote all frames", (unsigned long)written == kFrames);

		w.CloseFile();
	}

	{
		CAudioFileRead r(64);
		int status = r.OpenFile(path, 0, 0);
		check("ReadFile: OpenFile status==0", status == 0);
		check("ReadFile: SampleRate round-trips", r.GetSampleRate() == 44100);
		check("ReadFile: BitsPerSample round-trips", r.GetBitsPerSample() == 16);
		check("ReadFile: NumChannels round-trips", r.GetNumChannels() == kChannels);
		check("ReadFile: GetNumSamples round-trips", r.GetNumSamples() == (long long)kFrames);

		long outL[kFrames], outR[kFrames];
		memset(outL, 0, sizeof(outL));
		memset(outR, 0, sizeof(outR));
		long *outChans[2] = { outL, outR };
		int got = r.ReadPcmData(outChans, kFrames, kChannels);
		check("ReadPcmData decoded all frames", (unsigned long)got == kFrames);

		bool sampleMatch = true;
		for (unsigned long i = 0; i < kFrames; ++i) {
			if (outL[i] != left[i] || outR[i] != right[i]) {
				sampleMatch = false;
				break;
			}
		}
		check("Round-tripped PCM samples match exactly", sampleMatch);

		r.CloseFile();
	}

	remove(path);
}

static void WavRoundTripReadEx()
{
	const char *path = "/tmp/eva_test_audio_file_readex.wav";
	const unsigned long kFrames = 40;

	long mono[kFrames];
	for (unsigned long i = 0; i < kFrames; ++i)
		mono[i] = (long)(i * 37) - 700;

	{
		CAudioFileWrite w(256);
		int status = w.OpenFile(path, 0, 1);
		check("ReadEx setup: OpenFile status==0", status == 0);
		long *chans[1] = { mono };
		w.WritePcmData(chans, kFrames, 1);
		w.CloseFile();
	}

	{
		CAudioFileReadEx r(256);
		int status = r.OpenFile(path, 0, 0);
		check("CAudioFileReadEx::OpenFile status==0", status == 0);
		check("CAudioFileReadEx: NumSamples round-trips", r.GetNumSamples() == (long long)kFrames);

		long out[kFrames];
		memset(out, 0, sizeof(out));
		long *outChans[1] = { out };
		int got = r.ReadPcmData(outChans, kFrames, 1);
		check("CAudioFileReadEx::ReadPcmData decoded all frames", (unsigned long)got == kFrames);

		bool match = true;
		for (unsigned long i = 0; i < kFrames; ++i) {
			if (out[i] != mono[i]) {
				match = false;
				break;
			}
		}
		check("CAudioFileReadEx round-tripped samples match", match);

		r.CloseFile();
	}

	remove(path);
}

static void TimeConversionChecks()
{
	long long s = CAudioFile::TimeToSample(0, 1, 30.0f, 44100);
	check("TimeToSample(0,1,30.0,44100) == 90*44100", s == (long long)90 * 44100);

	int h, m; float sec;
	CAudioFile::SampleToTime(s, 44100, &h, &m, &sec);
	check("SampleToTime round-trips minute", m == 1);
	check("SampleToTime round-trips ~30.0s", sec > 29.99f && sec < 30.01f);

	long long s2 = CAudioFile::SecondToSample(2.0, 48000);
	check("SecondToSample(2.0,48000) == 96000", s2 == 96000);
}

int main()
{
	printf("CAudioFile family known-answer test\n");
	WavRoundTrip16Bit();
	WavRoundTripReadEx();
	TimeConversionChecks();

	printf("%s\n", g_fail ? "FAILED" : "all ok");
	return g_fail ? 1 : 0;
}
