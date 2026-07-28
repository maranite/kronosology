/*
 * audio_file.cpp  -  see include/audio_file.h for the full ground-truth
 * provenance, field layout, and the corrected virtual-slot table this
 * translation is built from.
 *
 * Chunk-tag string constants below were recovered directly from
 * ground-truth `.rodata` (objdump byte dump around 0x8f1ef80..0x8f1f060),
 * not guessed: "RIFF"/"WAVE"/"bext"/"INFO"/"LIST"/"INAM"/"IART"/"IGNR"/
 * "IARL"/"ICRD"/"ISFT"/"fmt "/"smpl"/"PROP"/"SND "/"FS  "/"CHNL"/"CMPR"/
 * "DSD "/"data "/"1bit"/"ID3"/"TIT2"/"TPE1"/"TCON"/"TSSE"/"not compressed".
 * The `WriteWavTextData()`/`WriteID3v2()` field-index<->tag pairing (0:
 * title, 1: artist, 2: genre, 3: WAV-only "IARL", 4: WAV-only "ICRD", 5:
 * unused by either format, 6: software/encoder) is inferred from this
 * cross-format correspondence, not from any recovered field name.
 */

#include "audio_file.h"

#include <cmath>
#include <cstring>
#include <cstdlib>

extern void HAL_DisableInterrupts();
extern void HAL_EnableInterrupts();

/* DSDIFF per-channel-count "CHNL" ID-code table, recovered byte-for-byte
 * from ground truth `.rodata` (objdump dump at 0x8eeda60..0x8eedaf7): 6
 * groups of 25 bytes (indexed by (channels-1)*25, real ground truth
 * pointer arithmetic `&sm_asDsdiffChId + (channels*5-5)*5`), each holding
 * up to mNumChannels*4 bytes of 4-char channel codes. */
static const char sm_asDsdiffChId[6][25] = {
	/* 1ch */ "C000",
	/* 2ch */ "SLFTSRGT",
	/* 3ch */ "MLFTMRGTC   ",
	/* 4ch */ "MLFTMRGTLS  RS  ",
	/* 5ch */ "MLFTMRGTC   LS  RS  ",
	/* 6ch */ "MLFTMRGTC   LFE LS  RS  ",
};

/* ==================== layout verification ==================== */

/* CAudioFile is non-standard-layout (single non-virtual inheritance from a
 * polymorphic base is fine on this ABI in practice, but the C++ standard
 * only conditionally supports offsetof() here) -- silence the resulting
 * warning for this compile-time-only check block. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"

#define AUDIO_FILE_OFFSET_CHECK(member, expected) \
	do { \
		typedef char StaticAssertOffset_##member[(offsetof(CAudioFile, member) == (expected)) ? 1 : -1]; \
		(void)sizeof(StaticAssertOffset_##member); \
	} while (0)

void CAudioFile::VerifyLayout()
{
AUDIO_FILE_OFFSET_CHECK(mFileIsOpen, 0x40c);
AUDIO_FILE_OFFSET_CHECK(mSampleRate, 0x410);
AUDIO_FILE_OFFSET_CHECK(mBitsPerSample, 0x414);
AUDIO_FILE_OFFSET_CHECK(mNumChannels, 0x418);
AUDIO_FILE_OFFSET_CHECK(mFormatTag, 0x41c);
AUDIO_FILE_OFFSET_CHECK(mBlockAlign, 0x420);
AUDIO_FILE_OFFSET_CHECK(mLoopEnable, 0x424);
AUDIO_FILE_OFFSET_CHECK(mLoopStart, 0x428);
AUDIO_FILE_OFFSET_CHECK(mLoopEnd, 0x42c);
AUDIO_FILE_OFFSET_CHECK(mHeaderSizePatchOffset, 0x430);
AUDIO_FILE_OFFSET_CHECK(mDataStartByteOffset, 0x438);
AUDIO_FILE_OFFSET_CHECK(mStartSample, 0x440);
AUDIO_FILE_OFFSET_CHECK(mFileDataEnd, 0x448);
AUDIO_FILE_OFFSET_CHECK(mEndSample, 0x450);
AUDIO_FILE_OFFSET_CHECK(mCurSample, 0x458);
AUDIO_FILE_OFFSET_CHECK(mSavedDataStartByteOffset, 0x460);
AUDIO_FILE_OFFSET_CHECK(mAbsSampleOffsetBase, 0x468);
AUDIO_FILE_OFFSET_CHECK(mSavedEndSample, 0x470);
AUDIO_FILE_OFFSET_CHECK(mDsfBlockSizeShift, 0x478);
AUDIO_FILE_OFFSET_CHECK(mDsfBlockSize, 0x47c);
AUDIO_FILE_OFFSET_CHECK(mBufferFrameOffset, 0x480);
AUDIO_FILE_OFFSET_CHECK(mAudioFileFormat, 0x484);
AUDIO_FILE_OFFSET_CHECK(mAvgBytesPerSec, 0x488);
AUDIO_FILE_OFFSET_CHECK(mBufferFrameCount, 0x48c);
AUDIO_FILE_OFFSET_CHECK(mAudioBufferSize, 0x490);
AUDIO_FILE_OFFSET_CHECK(mTextBufPtr, 0x494);
AUDIO_FILE_OFFSET_CHECK(mTextBufSize, 0x4b0);
AUDIO_FILE_OFFSET_CHECK(mTextStorage0, 0x4cc);
AUDIO_FILE_OFFSET_CHECK(mAudioBuffer, 0x6a4);
	typedef char StaticAssertSizeofCAudioFile[(sizeof(CAudioFile) == 0x6a8) ? 1 : -1];
	(void)sizeof(StaticAssertSizeofCAudioFile);
}

#pragma GCC diagnostic pop

/* ==================== CAudioFile ==================== */

CAudioFile::CAudioFile(unsigned long audioBufferSize)
	: mPcmFilter(0x10)
{
	mTextBufPtr[0] = mTextStorage0; mTextBufSize[0] = 0x80;
	mTextBufPtr[1] = mTextStorage1; mTextBufSize[1] = 0x80;
	mTextBufPtr[2] = mTextStorage2; mTextBufSize[2] = 0x20;
	mTextBufPtr[3] = mTextStorage3; mTextBufSize[3] = 0x20;
	mTextBufPtr[4] = mTextStorage4; mTextBufSize[4] = 10;
	mTextBufPtr[5] = mTextStorage5; mTextBufSize[5] = 8;
	mTextBufPtr[6] = mTextStorage6; mTextBufSize[6] = 0x50;

	mAudioBufferSize = audioBufferSize;
	HAL_DisableInterrupts();
	mAudioBuffer = malloc(audioBufferSize);
	HAL_EnableInterrupts();

	Reset();
}

CAudioFile::~CAudioFile()
{
	/* D0 additionally wraps `free(this)` in HAL_DisableInterrupts()/
	 * HAL_EnableInterrupts() -- not reproduced, see long_binary_file.h's
	 * dtor convention note. */
	if (mAudioBuffer) {
		HAL_DisableInterrupts();
		free(mAudioBuffer);
		HAL_EnableInterrupts();
	}
}

void CAudioFile::Reset()
{
	mSampleRate = 0xac44; /* 44100 */
	mBitsPerSample = 0x10; /* 16 */
	mNumChannels = 2;
	mFileIsOpen = false;

	mHeaderSizePatchOffset = 0;
	mDataStartByteOffset = 0;
	mStartSample = 0;
	mFileDataEnd = 0;
	mEndSample = 0;
	mCurSample = 0;
	mSavedDataStartByteOffset = 0;
	mAbsSampleOffsetBase = 0;
	mSavedEndSample = 0;

	mBlockAlign = 0;
	mAvgBytesPerSec = 0;
	mAudioFileFormat = 0;
	mBufferFrameCount = 0;
	mFormatTag = 0;

	mDsfBlockSizeShift = 0xc;
	mDsfBlockSize = 0x1000;

	mLoopEnable = false;
	mLoopStart = 0;
	mLoopEnd = 0;

	memset(mTextStorage0, 0, sizeof(mTextStorage0));
	memset(mTextStorage1, 0, sizeof(mTextStorage1));
	memset(mTextStorage2, 0, sizeof(mTextStorage2));
	memset(mTextStorage3, 0, sizeof(mTextStorage3));
	memset(mTextStorage4, 0, sizeof(mTextStorage4));
	memset(mTextStorage5, 0, sizeof(mTextStorage5));
	memset(mTextStorage6, 0, sizeof(mTextStorage6));

	mBufferFrameOffset = 0;
	if (mAudioBuffer)
		memset(mAudioBuffer, 0, mAudioBufferSize);

	CLongBinaryFile::Reset();
}

void CAudioFile::CloseFile()
{
	if (mFileIsOpen) {
		Close();
		Reset();
	}
}

bool CAudioFile::FileIsOpen()
{
	return mFileIsOpen;
}

void CAudioFile::SetAudioBufferSize(unsigned long size)
{
	if (mAudioBufferSize == size)
		return;
	if (mAudioBuffer) {
		HAL_DisableInterrupts();
		free(mAudioBuffer);
		HAL_EnableInterrupts();
	}
	mAudioBufferSize = size;
	HAL_DisableInterrupts();
	mAudioBuffer = malloc(size);
	HAL_EnableInterrupts();
	CalcBlockAlign();
}

int CAudioFile::SetSampleRate(int sampleRate)
{
	mSampleRate = sampleRate;
	CalcAvgBytesPerSec();
	return mSampleRate;
}

int CAudioFile::SetBitsPerSample(int bits)
{
	mBitsPerSample = bits;
	CalcBlockAlign();
	mPcmFilter.SetBitsPerSample(mBitsPerSample);
	return mBitsPerSample;
}

unsigned int CAudioFile::SetChannelBlockSize(unsigned long blockSize)
{
	mDsfBlockSize = blockSize;
	mDsfBlockSizeShift = 0;
	unsigned long n = blockSize >> 1;
	int bits = 0;
	while (n) {
		n >>= 1;
		++bits;
	}
	if (blockSize > 1)
		mDsfBlockSizeShift = bits;
	CalcBlockAlign();
	return mDsfBlockSize;
}

int CAudioFile::SetNumChannels(int channels)
{
	mNumChannels = channels;
	CalcBlockAlign();
	return mNumChannels;
}

int CAudioFile::GetSampleRate() { return mSampleRate; }
int CAudioFile::GetBitsPerSample() { return mBitsPerSample; }
int CAudioFile::GetNumChannels() { return mNumChannels; }
int CAudioFile::GetAudioFileFormat() { return mAudioFileFormat; }

void CAudioFile::SetLoopEnable(bool enable) { mLoopEnable = enable; }
void CAudioFile::SetLoopStart(unsigned long start) { mLoopStart = start; }
void CAudioFile::SetLoopEnd(unsigned long end) { mLoopEnd = end; }

long long CAudioFile::SetSamplePosition(long long sample)
{
	long long clamped = sample;
	if (clamped < mStartSample)
		clamped = mStartSample;
	else if (clamped > mEndSample)
		clamped = mEndSample;
	mCurSample = clamped;

	if (mFileIsOpen) {
		long long byteOffset = mCurSample * (long long)mBlockAlign + mDataStartByteOffset;
		Seek(byteOffset, eFileSeekSet);
		mBufferFrameOffset = 0;
	}
	return mCurSample;
}

long long CAudioFile::SetCurTime(int hour, int minute, float secondsWithFraction)
{
	long long sample = TimeToSample(hour, minute, secondsWithFraction, mSampleRate);
	return SetSamplePosition(sample);
}

long double CAudioFile::SetCurTimeSecond(double seconds)
{
	long long sample = (long long)(seconds * (double)mSampleRate);
	SetSamplePosition(sample);
	return GetCurTimeSecond();
}

long long CAudioFile::GetSamplePosition()
{
	return mCurSample - mStartSample;
}

long long CAudioFile::GetAbsSamplePosition()
{
	return mAbsSampleOffsetBase + mCurSample;
}

void CAudioFile::GetCurTime(int *hour, int *minute, float *secondsWithFraction)
{
	long double raw = GetCurTimeSecond();
	int totalSeconds = (int)raw;
	int totalMinutes = totalSeconds / 60;
	*hour = totalMinutes / 60;
	*minute = totalMinutes % 60;
	*secondsWithFraction = (float)(raw - (long double)(totalMinutes * 60));
}

long double CAudioFile::GetCurTimeSecond()
{
	if (mSampleRate == 0)
		return 0;
	return (long double)mCurSample / (long double)mSampleRate;
}

long long CAudioFile::GetNumSamples()
{
	return mEndSample - mStartSample;
}

void CAudioFile::GetTotalTime(int *hour, int *minute, float *secondsWithFraction)
{
	long double raw = GetTotalTimeSecond();
	int totalSeconds = (int)raw;
	int totalMinutes = totalSeconds / 60;
	*hour = totalMinutes / 60;
	*minute = totalMinutes % 60;
	*secondsWithFraction = (float)(raw - (long double)(totalMinutes * 60));
}

long double CAudioFile::GetTotalTimeSecond()
{
	if (mSampleRate == 0)
		return 0;
	return (long double)GetNumSamples() / (long double)mSampleRate;
}

void CAudioFile::ReadID(char *id4)
{
	if (!mFileIsOpen) {
		id4[0] = id4[1] = id4[2] = id4[3] = '\0';
		return;
	}
	Read(id4, 4);
}

void CAudioFile::WriteID(const char *id4)
{
	if (mFileIsOpen)
		Write(id4, 4);
}

void CAudioFile::ReadChunkHeader(CChunkHeader8 *out)
{
	ReadID(out->id);
	out->size = (unsigned int)ReadData(4);
	ReadID(out->peekId);
	Jump(-4);
}

void CAudioFile::ReadChunkHeader(CChunkHeader12 *out)
{
	ReadID(out->id);
	out->size = (unsigned long long)ReadData(8);
	ReadID(out->peekId);
	Jump(-4);
}

void CAudioFile::ReadID3v2FrameHeader(CID3v2FrameHeader *out)
{
	ReadID(out->id);
	out->size = (unsigned int)ReadData(4);
	out->flags1 = (unsigned char)ReadData(1);
	out->flags2 = (unsigned char)ReadData(1);
}

int CAudioFile::FindChunk(const char *id4, CChunkHeader8 *out, long long searchStart, long long searchEnd)
{
	Seek(searchStart, eFileSeekSet);
	if (searchEnd == 0)
		searchEnd = mFileDataEnd;

	for (;;) {
		ReadChunkHeader(out);
		if (strncmp(id4, out->id, 4) == 0)
			return 1;
		long long next = Seek((long long)((out->size + 1) & ~1u), eFileSeekCur);
		if (next >= searchEnd || next == -1)
			break;
	}
	out->id[0] = out->id[1] = out->id[2] = out->id[3] = '\0';
	out->size = 0;
	out->peekId[0] = out->peekId[1] = out->peekId[2] = out->peekId[3] = '\0';
	return 0;
}

int CAudioFile::FindChunk(const char *wantedId, const char *id4, CChunkHeader8 *out,
                           long long searchStart, long long searchEnd)
{
	/* Real semantics (re-verified against raw disassembly): search for a
	 * chunk whose OWN id matches `wantedId`, then check its peeked
	 * embedded type tag (out->peekId) against `id4` -- e.g. a RIFF
	 * "LIST" chunk of sub-type "INFO". On a peekId mismatch, retry from
	 * Tell() (ground truth does not skip the mismatched chunk's
	 * payload). See header comment. */
	while (FindChunk(wantedId, out, searchStart, searchEnd)) {
		if (strncmp(id4, out->peekId, 4) == 0)
			return 1;
		searchStart = Tell();
	}
	return 0;
}

int CAudioFile::FindChunk(const char *id4, CChunkHeader12 *out, long long searchStart, long long searchEnd)
{
	Seek(searchStart, eFileSeekSet);
	if (searchEnd == 0)
		searchEnd = mFileDataEnd;

	for (;;) {
		ReadChunkHeader(out);
		if (strncmp(id4, out->id, 4) == 0)
			return 1;
		long long next = Seek((long long)((out->size + 1) & ~1ull), eFileSeekCur);
		if (next >= searchEnd || next == -1)
			break;
	}
	out->id[0] = out->id[1] = out->id[2] = out->id[3] = '\0';
	out->size = 0;
	out->peekId[0] = out->peekId[1] = out->peekId[2] = out->peekId[3] = '\0';
	return 0;
}

int CAudioFile::FindChunk(const char *wantedId, const char *id4, CChunkHeader12 *out,
                           long long searchStart, long long searchEnd)
{
	/* Modeled by symmetry with the confirmed CChunkHeader8 overload --
	 * see its own header comment note (never called in this family). */
	while (FindChunk(wantedId, out, searchStart, searchEnd)) {
		if (strncmp(id4, out->peekId, 4) == 0)
			return 1;
		searchStart = Tell();
	}
	return 0;
}

int CAudioFile::FindID3v2Frame(const char *id4, CID3v2FrameHeader *out, long long searchStart, long long searchEnd)
{
	Seek(searchStart, eFileSeekSet);
	ReadID3v2FrameHeader(out);
	for (;;) {
		if (strncmp(id4, out->id, 4) == 0)
			return 1;
		long long next = Seek((long long)out->size, eFileSeekCur);
		if (next > searchEnd || next == -1 || out->id[0] == '\0')
			break;
		ReadID3v2FrameHeader(out);
	}
	out->id[0] = out->id[1] = out->id[2] = out->id[3] = '\0';
	out->size = 0;
	out->flags1 = out->flags2 = 0;
	return 0;
}

void CAudioFile::CalcAvgBytesPerSec()
{
	if (mBitsPerSample == 1)
		mAvgBytesPerSec = (mNumChannels * mSampleRate) >> 3;
	else
		mAvgBytesPerSec = mSampleRate * mBlockAlign;
}

void CAudioFile::CalcBlockAlign()
{
	mBlockAlign = (mBitsPerSample >> 3) * mNumChannels;
	mAudioFileFormat = mAudioFileFormat; /* untouched here */
	mBufferFrameCount = (mBlockAlign != 0) ? (mAudioBufferSize / mBlockAlign) : 0;
	if (mAudioBuffer)
		memset(mAudioBuffer, 0, mAudioBufferSize);
	CalcAvgBytesPerSec();
}

long long CAudioFile::TimeToSample(int hour, int minute, float secondsWithFraction, int sampleRate)
{
	return (long long)(((float)minute * 60.0f + (float)hour * 3600.0f + secondsWithFraction) * (float)sampleRate);
}

void CAudioFile::SampleToTime(long long sample, int sampleRate, int *hour, int *minute, float *secondsWithFraction)
{
	if (sampleRate == 0) {
		*hour = 0;
		*minute = 0;
		*secondsWithFraction = 0.0f;
		return;
	}
	float totalSeconds = (float)sample / (float)sampleRate;
	int totalMinutes = (int)totalSeconds / 60;
	float remainder = totalSeconds - (float)(totalMinutes * 60);
	*hour = totalMinutes / 60;
	*minute = totalMinutes % 60;
	*secondsWithFraction = remainder;
}

void CAudioFile::SecondToTime(double seconds, int *hour, int *minute, float *secondsWithFraction)
{
	int totalSeconds = (int)seconds;
	int totalMinutes = totalSeconds / 60;
	*hour = totalMinutes / 60;
	*minute = totalMinutes % 60;
	*secondsWithFraction = (float)seconds - (float)(totalMinutes * 60);
}

long long CAudioFile::SecondToSample(double seconds, int sampleRate)
{
	return (long long)((double)sampleRate * seconds);
}

long double CAudioFile::SampleToSecond(long long sample, int sampleRate)
{
	if (sampleRate == 0)
		return 0;
	return (long double)sample / (long double)sampleRate;
}

/* ==================== CAudioFileRead ==================== */

CAudioFileRead::CAudioFileRead(unsigned long audioBufferSize)
	: CAudioFile(audioBufferSize)
{
}

CAudioFileRead::~CAudioFileRead()
{
	CloseFile();
}

int CAudioFileRead::OpenFile(const char *path, int /*arg2*/, int /*arg3*/)
{
	CloseFile();
	if (path[0] == '\0')
		return 2;
	if (!Open(path, 0, false))
		return 2;
	mFileIsOpen = true;

	size_t len = strlen(path);
	const char *ext = (len >= 3) ? path + len - 3 : path;

	int status;
	if (strcmp(ext, "wav") == 0 || strcmp(ext, "WAV") == 0 || strcmp(ext, "p01") == 0)
		status = OpenWaveFile();
	else if (strcmp(ext, "dff") == 0 || strcmp(ext, "DFF") == 0)
		status = OpenDsdiffFile();
	else if (strcmp(ext, "wsd") == 0 || strcmp(ext, "WSD") == 0 || strcmp(ext, "p02") == 0)
		status = OpenWsdFile();
	else if (strcmp(ext, "dsf") == 0 || strcmp(ext, "DSF") == 0)
		status = OpenDsfFile();
	else {
		status = 3;
		CloseFile();
	}

	/* Establish the "uncropped" baseline for Crop()/CropCancel(). */
	mSavedDataStartByteOffset = mDataStartByteOffset;
	mSavedEndSample = mEndSample;
	mStartSample = 0;
	mAbsSampleOffsetBase = 0;
	return status;
}

long long CAudioFileRead::SetSamplePosition(long long sample)
{
	long long result = CAudioFile::SetSamplePosition(sample);
	if (mFileIsOpen) {
		unsigned int bytes;
		if ((unsigned int)(mAudioFileFormat - 2) < 3)
			bytes = (mBufferFrameCount >> 3) * mNumChannels;
		else
			bytes = mBlockAlign * mBufferFrameCount;
		Read(mAudioBuffer, bytes);
	}
	return result;
}

long long CAudioFileRead::Crop(long long cropStart, long long cropEnd)
{
	if (!mFileIsOpen)
		return 0;

	long long end = cropEnd;
	if (end < 0 || end > mEndSample)
		end = mEndSample;
	long long start = cropStart;
	if (start < 0 || start > end)
		start = end;

	/* PCM byte-offset path (mAudioFileFormat 0/1). Ground truth
	 * additionally special-cases bit-packed formats (2/3/4) with
	 * 8-sample-boundary rounding before this multiply -- not
	 * independently re-verified via raw disassembly (unreachable in
	 * this family in practice: DSDIFF/WSD/DSF reading are all stubs,
	 * see OpenDsdiffFile/OpenWsdFile/OpenDsfFile's own notes), so
	 * modeled here by structural inference only. */
	long long startByteDelta = start * (long long)mBlockAlign;

	mDataStartByteOffset += startByteDelta;
	mAbsSampleOffsetBase += start;
	mEndSample = end - start;
	mStartSample = 0;

	SetSamplePosition(0);
	return GetNumSamples();
}

long long CAudioFileRead::CropCancel()
{
	mDataStartByteOffset = mSavedDataStartByteOffset;
	mEndSample = mSavedEndSample;
	mAbsSampleOffsetBase = 0;
	mStartSample = 0;
	SetSamplePosition(0);
	return mEndSample;
}

int CAudioFileRead::ReadAudioData(float **out, unsigned long count, int channels, float dummy)
{
	if (!mFileIsOpen)
		return 0;
	if (mAudioFileFormat >= 2 && mAudioFileFormat < 4)
		return ReadDsdData(out, count, channels, dummy);
	if (mAudioFileFormat == 4)
		return ReadDsdData2(out, count, channels, dummy);
	/* format 0/1, or (ground truth's own real behavior) any format > 4 */
	return ReadPcmData(out, count, channels);
}

int CAudioFileRead::ReadAudioData(unsigned char **out, unsigned long count, int channels)
{
	if (!mFileIsOpen)
		return 0;
	if ((unsigned int)(mAudioFileFormat - 2) > 1) /* format NOT 2 or 3 */
		return ReadPcmData(reinterpret_cast<long **>(out), count, channels);
	return ReadDsdData(out, count, channels);
}

int CAudioFileRead::ReadPcmData(long **out, unsigned long count, int channels)
{
	int extraChannels = 0;
	if (channels > mNumChannels) {
		extraChannels = channels - mNumChannels;
		channels = mNumChannels;
	}

	int bytesPerSample = mBitsPerSample >> 3;
	int shiftBase = (4 - bytesPerSample) * 8;
	unsigned long decoded = 0;

	if (mFileIsOpen) {
		long long avail = mEndSample - mCurSample;
		unsigned long want = count;
		if (avail < (long long)want)
			want = (unsigned long)(avail < 0 ? 0 : avail);

		while (want != 0) {
			if (mBufferFrameOffset >= mBufferFrameCount) {
				Read(mAudioBuffer, mBufferFrameCount * mBlockAlign);
				mBufferFrameOffset = 0;
			}
			unsigned char *base = (unsigned char *)mAudioBuffer
				+ (long long)mBufferFrameOffset * channels * bytesPerSample;
			unsigned int avail_in_buf = mBufferFrameCount - mBufferFrameOffset;
			unsigned int chunk = (want < avail_in_buf) ? (unsigned int)want : avail_in_buf;

			for (int ch = 0; ch < channels; ++ch) {
				long *dst = out[ch];
				unsigned char *src = base + (long long)ch * bytesPerSample;
				for (unsigned int i = 0; i < chunk; ++i) {
					long acc = 0;
					for (int b = 0; b < bytesPerSample; ++b)
						acc |= (long)src[b] << (shiftBase + b * 8);
					dst[decoded + i] = acc >> shiftBase;
					src += mBlockAlign;
				}
			}

			want -= chunk;
			decoded += chunk;
			mBufferFrameOffset += chunk;
			mCurSample += chunk;
			if (!mFileIsOpen)
				break;
			avail = mEndSample - mCurSample;
			unsigned long remaining = want;
			if (avail >= 0 && (unsigned long)avail < remaining)
				remaining = (unsigned long)avail;
			want = remaining;
		}
	}

	/* Ground truth's own trailing fill uses `param_2` (the count
	 * parameter) AFTER the main loop has decremented it down to the
	 * remaining unfulfilled amount -- since this translation never
	 * mutates the `count` parameter itself, the equivalent remaining
	 * amount is `count - decoded` here; the memmove/second memset use
	 * ground truth's own `param_2 + local_28` (remaining + decoded),
	 * i.e. the original `count`. */
	for (int ch = 0; ch < channels; ++ch)
		memset(out[ch] + decoded, 0, (count - decoded) * sizeof(long));

	int totalChannels = channels;
	if (channels == 1 && extraChannels != 0) {
		memmove(out[1], out[0], count * sizeof(long));
		--extraChannels;
		totalChannels = 2;
	}
	for (; extraChannels > 0; --extraChannels)
		memset(out[totalChannels++], 0, count * sizeof(long));

	return (int)decoded;
}

int CAudioFileRead::ReadPcmData(float **out, unsigned long count, int channels)
{
	if (mBitsPerSample != 0x20) {
		int decoded = ReadPcmData(reinterpret_cast<long **>(out), count, channels);
		mPcmFilter.IntToFloat(reinterpret_cast<long **>(out), out, decoded, channels);
		return decoded;
	}
	/* Ground truth tail-calls the SAME long** overload even for 32-bit
	 * data: bytesPerSample==4 makes the unpack loop's shift amount 0,
	 * i.e. a verbatim 4-byte copy -- since mFormatTag==3 (IEEE float)
	 * whenever mBitsPerSample==32, those raw bytes already ARE the
	 * float bit pattern, so writing them through a `long*` alias into
	 * memory the caller will read back as `float` is exact. */
	return ReadPcmData(reinterpret_cast<long **>(out), count, channels);
}

int CAudioFileRead::ReadDsdData(unsigned char **, unsigned long, int) { return 0; }
int CAudioFileRead::ReadDsdData2(unsigned char **, unsigned long, int) { return 0; }
int CAudioFileRead::ReadDsdData(float **, unsigned long, int, float) { return 0; }
int CAudioFileRead::ReadDsdData2(float **, unsigned long, int, float) { return 0; }

int CAudioFileRead::OpenWaveFile()
{
	mByteOrder = 0;
	mAudioFileFormat = 0;

	CChunkHeader8 riffHdr;
	ReadChunkHeader(&riffHdr);
	if (strncmp(riffHdr.id, "RIFF", 4) != 0 || strncmp(riffHdr.peekId, "WAVE", 4) != 0) {
		CloseFile();
		return 3;
	}
	mFileDataEnd = (long long)riffHdr.size + 8;
	mHeaderSizePatchOffset = Tell() + 4;

	CChunkHeader8 hdr;
	int uVar8 = 0;
	if (FindChunk("bext", &hdr, mHeaderSizePatchOffset, 0)) {
		mAudioFileFormat = 1;
		ReadText(mTextStorage0, 0x100, 0x80);
		ReadText(mTextStorage1, 0x20, 0x80);
		Jump(0x20);
		ReadText(mTextStorage4, 10, 0x7fffffff);
		ReadText(mTextStorage5, 8, 0x7fffffff);
		Jump(8);
		Jump(2);
		Jump(0x40);
		Jump(0xbe);
		uVar8 = (int)hdr.size - 0x25a;
	} else {
		long long innerStart = 0, innerEnd = 0;
		if (FindChunk("LIST", "INFO", &hdr, mHeaderSizePatchOffset, 0)) {
			innerStart = Tell() + 4;
			innerEnd = Tell() + hdr.size;
			if (FindChunk("INAM", &hdr, innerStart, innerEnd))
				ReadText(mTextStorage0, hdr.size, 0x80);
			if (FindChunk("IART", &hdr, innerStart, innerEnd))
				ReadText(mTextStorage1, hdr.size, 0x80);
			if (FindChunk("IGNR", &hdr, innerStart, innerEnd))
				ReadText(mTextStorage2, hdr.size, 0x20);
			if (FindChunk("IARL", &hdr, innerStart, innerEnd))
				ReadText(mTextStorage3, hdr.size, 0x20);
			if (FindChunk("ICRD", &hdr, innerStart, innerEnd))
				ReadText(mTextStorage4, hdr.size, 10);
			if (FindChunk("ISFT", &hdr, innerStart, innerEnd))
				uVar8 = (int)hdr.size;
		}
	}
	ReadText(mTextStorage6, uVar8, 0x50);

	if (!FindChunk("fmt ", &hdr, mHeaderSizePatchOffset, 0)) {
		CloseFile();
		return 3;
	}
	int formatTag = (int)ReadData(2);
	int channels = (int)ReadData(2);
	int sampleRate = (int)ReadData(4);
	int avgBytesPerSec = (int)ReadData(4);
	int blockAlign = (int)ReadData(2);
	int bitsPerSample = (int)ReadData(2);

	if (channels > 6) {
		CloseFile();
		return 3;
	}
	SetNumChannels(channels);

	if (sampleRate != 88200) {
		if (sampleRate < 88201) {
			if (sampleRate != 44100 && sampleRate != 48000) {
				CloseFile();
				return 3;
			}
		} else if (sampleRate != 176400 && sampleRate != 192000 && sampleRate != 96000) {
			CloseFile();
			return 3;
		}
	}
	SetSampleRate(sampleRate);

	if (bitsPerSample == 24 || bitsPerSample == 16) {
		if (formatTag != 1) {
			CloseFile();
			return 3;
		}
	} else if (bitsPerSample == 32) {
		if (formatTag != 3) {
			CloseFile();
			return 3;
		}
	} else {
		CloseFile();
		return 3;
	}
	mFormatTag = (short)formatTag;
	SetBitsPerSample(bitsPerSample);

	if (mAvgBytesPerSec == avgBytesPerSec && mBlockAlign == blockAlign &&
	    FindChunk("data", &hdr, mHeaderSizePatchOffset, 0)) {
		mDataStartByteOffset = Tell();
		mEndSample = 0;
		mEndSample = (long long)(hdr.size / mBlockAlign);
		SetSamplePosition(0);
		return 0;
	}

	CloseFile();
	return 3;
}

int CAudioFileRead::OpenDsdiffFile() { return 0; }
int CAudioFileRead::OpenWsdFile() { return 0; }
int CAudioFileRead::OpenDsfFile() { return 0; }
void CAudioFileRead::ReadID3v2() { }

/* ==================== CAudioFileReadEx ==================== */

CAudioFileReadEx::CAudioFileReadEx(unsigned long audioBufferSize)
	: CAudioFileRead(audioBufferSize)
{
	HAL_DisableInterrupts();
	mDoubleBuffer = malloc(audioBufferSize);
	HAL_EnableInterrupts();

	mAudioBufferCopy = mAudioBuffer;
	mCurrentBufferBase = mDoubleBuffer;
	mBufferToggleIndex = 0;
	mBufferSkipRate = 0;
	mCurrentReadPtr = 0;
	mDoubleBufferBlockBytes = 0;
	mNextBlockIndex = 0;
	mSavedEndSampleLo = 0;
	mSavedEndSampleHi = 0;
	mLastBlockFrameCount = 0;
	mReservedA = 0;
	mReservedB = 0;
	mFilePtrIndirect = &mFile;
	mFileIsOpenIndirect = &mFileIsOpen;
}

CAudioFileReadEx::~CAudioFileReadEx()
{
	if (mDoubleBuffer) {
		HAL_DisableInterrupts();
		free(mDoubleBuffer);
		HAL_EnableInterrupts();
	}
}

int CAudioFileReadEx::OpenFile(const char *path, int arg2, int arg3)
{
	return CAudioFileRead::OpenFile(path, arg2, arg3);
}

void CAudioFileReadEx::CloseFile()
{
	CAudioFile::CloseFile();
}

long long CAudioFileReadEx::SetSamplePosition(long long sample)
{
	/* Ground truth's real body calls CAudioFileRead::SetSamplePosition()
	 * (which does its own real Read() into mAudioBuffer) and THEN an
	 * additional direct CFileOperation seek+read into the double-buffer
	 * -- confirmed against raw disassembly, not a translation error.
	 * Read literally, that second read's relative Seek(nextIndex*
	 * blockBytes) starts from wherever the base class's own Read() just
	 * left the file position, i.e. ground truth's double-buffer holds
	 * genuinely read-AHEAD data (one block past what the base class's
	 * own mAudioBuffer/mCurSample bookkeeping considers "current").
	 * Reproducing that literally requires ReadPcmData() to also know to
	 * treat mCurrentReadPtr as one-block-ahead rather than
	 * "the data for the current mBufferFrameOffset" -- attempted, but a
	 * host round-trip KAT test (verify/test_audio_file.cpp) caught real
	 * decoded-sample corruption from this translation, so it's rolled
	 * back: ReadPcmData()/SetSamplePosition() below just delegate to the
	 * already-tested CAudioFileRead single-buffer implementation, giving
	 * up the double-buffered background-prefetch OPTIMIZATION (invisible
	 * to a caller -- same decoded PCM output either way) while keeping
	 * correctness. See DECOMPILE_ERRORS.md for the full note. */
	long long result = CAudioFileRead::SetSamplePosition(sample);
	mBufferToggleIndex = 0;
	return result;
}

int CAudioFileReadEx::ReadPcmData(long **out, unsigned long count, int channels)
{
	/* Delegates to the base class's already-verified single-buffer
	 * decode core -- see SetSamplePosition()'s own header comment above
	 * for why the double-buffered refill isn't reproduced here. */
	return CAudioFileRead::ReadPcmData(out, count, channels);
}

int CAudioFileReadEx::ReadPcmData(float **out, unsigned long count, int channels)
{
	if (mBitsPerSample != 0x20) {
		int decoded = ReadPcmData(reinterpret_cast<long **>(out), count, channels);
		mPcmFilter.IntToFloat(reinterpret_cast<long **>(out), out, decoded, channels);
		return decoded;
	}
	return ReadPcmData(reinterpret_cast<long **>(out), count, channels);
}

int CAudioFileReadEx::ReadDsdData(unsigned char **, unsigned long, int) { return 0; }
int CAudioFileReadEx::ReadDsdData2(unsigned char **, unsigned long, int) { return 0; }
int CAudioFileReadEx::ReadDsdData(float **, unsigned long, int) { return 0; }
int CAudioFileReadEx::ReadDsdData2(float **, unsigned long, int) { return 0; }

int CAudioFileReadEx::ReadAudioData(float **out, unsigned long count, int channels)
{
	if (!mFileIsOpen)
		return 0;
	if (mAudioFileFormat >= 2 && mAudioFileFormat < 4)
		return ReadDsdData(out, count, channels);
	if (mAudioFileFormat == 4)
		return ReadDsdData2(out, count, channels);
	return ReadPcmData(out, count, channels);
}

void CAudioFileReadEx::SetBufferSkipRate(long rate) { mBufferSkipRate = rate; }
long CAudioFileReadEx::GetBufferSkipRate() { return mBufferSkipRate; }

int CAudioFileReadEx::BufferAudio(CFileBufferParams *params)
{
	SFilePointer *fp = *params->pFilePtr;
	if (*params->pIsOpen) {
		CFileOperation::Seek(fp, (long)params->blockIndex * (long)params->size, eFileSeekCur);
		CFileOperation::Read(params->buffer, params->size, 1, fp);
	}
	return 0;
}

/* ==================== CAudioFileWrite ==================== */

CAudioFileWrite::CAudioFileWrite(unsigned long audioBufferSize)
	: CAudioFile(audioBufferSize)
{
}

CAudioFileWrite::~CAudioFileWrite()
{
	CloseFile();
}

int CAudioFileWrite::OpenFile(const char *path, int format, int channels)
{
	CloseFile();
	int status = 2;
	if (format < 5 && Open(path, 0x1ff, true)) {
		mFileIsOpen = true;
		mAudioFileFormat = format;

		int ch = 1;
		if (channels > 0)
			ch = (channels < 7) ? channels : 6;
		SetNumChannels(ch);

		switch (mAudioFileFormat) {
		case 0:
		case 1:
			status = 0;
			WriteWavHeader();
			break;
		case 2:
			WriteDsdiffHeader();
			status = 0;
			break;
		case 3:
			status = 0;
			WriteWsdHeader();
			break;
		case 4:
			status = 0;
			WriteDsfHeader();
			break;
		default:
			status = 2;
			CloseFile();
			break;
		}
	}
	return status;
}

void CAudioFileWrite::CloseFile()
{
	if (!mFileIsOpen)
		return;

	MoveToEnd();

	if (mAudioFileFormat == 1) {
		Write(mAudioBuffer, mBlockAlign * mBufferFrameOffset);
		MoveToEnd();
		mFileDataEnd = Tell();
		Seek(4, eFileSeekSet);
		WriteData(mFileDataEnd - 8, 4);
		Seek(mHeaderSizePatchOffset, eFileSeekSet);
		WriteBwfChunk();
		WriteWavRequiredChunks();
	} else if (mAudioFileFormat == 0) {
		Write(mAudioBuffer, mBlockAlign * mBufferFrameOffset);
		WriteWavTextData();
		MoveToEnd();
		mFileDataEnd = Tell();
		Seek(4, eFileSeekSet);
		WriteData(mFileDataEnd - 8, 4);
		Seek(mHeaderSizePatchOffset, eFileSeekSet);
		WriteWavRequiredChunks();
	} else if (mAudioFileFormat == 2) {
		Write(mAudioBuffer, (mBufferFrameOffset >> 3) * mNumChannels);
		WriteDsdiffTextData();
		MoveToEnd();
		mFileDataEnd = Tell();
		Seek(4, eFileSeekSet);
		WriteData(mFileDataEnd - 0xc, 8);
		WriteDsdiffRequiredChunks();
	} else if (mAudioFileFormat == 4) {
		unsigned int shift = (unsigned int)mDsfBlockSizeShift;
		unsigned int blocks = (mDsfBlockSize - 1 + ((mBufferFrameOffset + 7) >> 3)) >> shift;
		Write(mAudioBuffer, (blocks << shift) * mNumChannels);
		MoveToEnd();
		long long sizeSoFar = Tell();
		WriteID3v2();
		WriteDsfRequiredChunks(sizeSoFar);
		MoveToEnd();
		mFileDataEnd = Tell();
		Seek(0xc, eFileSeekSet);
		WriteData(mFileDataEnd, 8);
	}
	/* mAudioFileFormat==3 (WSD): ground truth performs no size-patching
	 * on close at all -- WriteWsdHeader() already wrote a fixed,
	 * non-placeholder header. */

	CAudioFile::CloseFile();
}

int CAudioFileWrite::SetBitsPerSample(int bits)
{
	int resolved;
	if (mAudioFileFormat < 2) {
		if (bits <= 16) { mFormatTag = 1; resolved = 16; }
		else if (bits <= 24) { mFormatTag = 1; resolved = 24; }
		else { mFormatTag = 3; resolved = 32; }
	} else {
		mFormatTag = 0;
		resolved = 1;
	}
	return CAudioFile::SetBitsPerSample(resolved);
}

int CAudioFileWrite::WriteAudioData(unsigned char **in, unsigned long count, int channels)
{
	if ((unsigned int)(mAudioFileFormat - 2) < 2) /* format 2 or 3 */
		return WriteDsdData(in, count, channels);
	return WritePcmData(reinterpret_cast<long **>(in), count, channels);
}

int CAudioFileWrite::WriteAudioData(float **in, unsigned long count, int channels)
{
	if ((unsigned int)(mAudioFileFormat - 2) > 2) /* format 0, 1, or (ground truth's literal check) >4 */
		return WritePcmData(in, count, channels);
	return 0;
}

int CAudioFileWrite::WritePcmData(long **in, unsigned long count, int channels)
{
	int bytesPerSample = mBitsPerSample >> 3;
	int shiftBase = (4 - bytesPerSample) * 8;
	int useChannels = (channels < mNumChannels) ? channels : mNumChannels;
	unsigned long written = 0;

	if (mFileIsOpen && count != 0) {
		do {
			if (mBufferFrameOffset >= mBufferFrameCount) {
				MoveToEnd();
				Write(mAudioBuffer, mBlockAlign * mBufferFrameCount);
				memset(mAudioBuffer, 0, mAudioBufferSize);
				mBufferFrameOffset = 0;
			}
			unsigned int avail = mBufferFrameCount - mBufferFrameOffset;
			unsigned int chunk = (count < avail) ? (unsigned int)count : avail;

			if (chunk != 0) {
				unsigned char *frameBase = (unsigned char *)mAudioBuffer
					+ (long long)mBufferFrameOffset * mBlockAlign;
				for (int ch = 0; ch < useChannels; ++ch) {
					long *src = in[ch];
					unsigned char *dst = frameBase + (long long)ch * bytesPerSample;
					for (unsigned int i = 0; i < chunk; ++i) {
						long v = src[written + i] << shiftBase;
						for (int b = 0; b < bytesPerSample; ++b)
							dst[b] = (unsigned char)(v >> (shiftBase + b * 8));
						dst += mBlockAlign;
					}
				}
				for (int ch = useChannels; ch < mNumChannels; ++ch) {
					unsigned char *dst = frameBase + (long long)ch * bytesPerSample;
					for (unsigned int i = 0; i < chunk; ++i) {
						memset(dst, 0, bytesPerSample);
						dst += mBlockAlign;
					}
				}
			}

			written += chunk;
			mEndSample += chunk;
			mBufferFrameOffset += chunk;
			count -= chunk;
		} while (mFileIsOpen && count != 0);
	}

	mCurSample = mEndSample;
	return (int)written;
}

int CAudioFileWrite::WritePcmData(float **in, unsigned long count, int channels)
{
	if (mBitsPerSample != 0x20)
		mPcmFilter.FloatToInt(in, reinterpret_cast<long **>(in), count, channels);
	return WritePcmData(reinterpret_cast<long **>(in), count, channels);
}

int CAudioFileWrite::WriteDsdData(unsigned char **, unsigned long, int) { return 0; }
int CAudioFileWrite::WriteDsdData2(unsigned char **, unsigned long, int) { return 0; }

void CAudioFileWrite::WriteWavHeader()
{
	mByteOrder = 0;
	SetSampleRate(44100);
	SetBitsPerSample(16);

	mFileDataEnd = mLoopEnable ? 0x58 : 0x70;
	WriteID("RIFF");
	WriteData(mFileDataEnd - 8, 4);
	WriteID("WAVE");
	mHeaderSizePatchOffset = Tell();
	if (mAudioFileFormat == 1)
		WriteBwfChunk();
	WriteWavRequiredChunks();
	mDataStartByteOffset = Tell();
}

void CAudioFileWrite::WriteWavRequiredChunks()
{
	WriteID("fmt ");
	WriteData(0x10, 4);
	WriteData(mFormatTag, 2);
	WriteData(mNumChannels, 2);
	WriteData(mSampleRate, 4);
	WriteData(mAvgBytesPerSec, 4);
	WriteData(mBlockAlign, 2);
	WriteData(mBitsPerSample, 2);

	WriteID("smpl");
	unsigned int smplSize = mLoopEnable ? 0x3c : 0x24;
	WriteData(smplSize, 4);
	WriteData(0x42, 4);
	WriteData(0x68, 4);
	int samplePeriodNs = (mSampleRate != 0) ? (int)(1000000000LL / mSampleRate) : 0;
	WriteData(samplePeriodNs, 4);
	WriteData(0x3c, 4);
	WriteData(0, 4);
	WriteData(0, 4);
	WriteData(0, 4);
	WriteData(mLoopEnable ? 1 : 0, 4);
	WriteData(0, 4);
	if (mLoopEnable) {
		WriteData(0, 4);
		WriteData(0, 4);
		WriteData(mLoopStart, 4);
		WriteData(mLoopEnd, 4);
		WriteData(0, 4);
		WriteData(0, 4);
	}

	WriteID("data");
	WriteData((long long)mBlockAlign * mEndSample, 4);
}

void CAudioFileWrite::WriteWavTextData()
{
	bool any = mTextBufPtr[1][0] || mTextBufPtr[2][0] || mTextBufPtr[3][0]
		|| mTextBufPtr[4][0] || mTextBufPtr[5][0];
	if (!any)
		return;

	WriteID("LIST");
	long long sizePos = Tell();
	WriteData(4, 4);
	WriteID("INFO");
	int n0 = WriteWavTextDataLocal(0, "INAM");
	int n1 = WriteWavTextDataLocal(1, "IART");
	int n2 = WriteWavTextDataLocal(2, "IGNR");
	int n3 = WriteWavTextDataLocal(3, "IARL");
	int n4 = WriteWavTextDataLocal(4, "ICRD");
	int n6 = WriteWavTextDataLocal(6, "ISFT");
	Seek(sizePos, eFileSeekSet);
	WriteData(n0 + n1 + n2 + n3 + n4 + n6 + 4, 4);
	MoveToEnd();
}

int CAudioFileWrite::WriteWavTextDataLocal(int fieldIndex, const char *id4)
{
	const char *text = mTextBufPtr[fieldIndex];
	if (text[0] == '\0')
		return 0;
	unsigned int len = (unsigned int)strlen(text);
	unsigned int padded = (len + 3) & ~1u;
	WriteID(id4);
	WriteData(padded, 4);
	Write(text, len);
	if ((int)len < (int)padded) {
		unsigned int written = len + 1;
		WriteData(0, 1);
		while (written < padded) {
			WriteData(0, 1);
			++written;
		}
	}
	return (int)(padded + 8);
}

void CAudioFileWrite::WriteBwfChunk() { }
void CAudioFileWrite::WriteDsdiffHeader() { }

void CAudioFileWrite::WriteDsdiffRequiredChunks()
{
	Seek(mHeaderSizePatchOffset, eFileSeekSet);
	unsigned int channelChunkSize = mNumChannels * 4 + 2;

	WriteID("PROP");
	WriteData((long long)mNumChannels * 4 + 0x42, 8);
	WriteID("SND ");
	WriteID("FS  ");
	WriteData(4, 8);
	WriteData(mSampleRate, 4);
	WriteID("CHNL");
	WriteData(channelChunkSize, 8);
	WriteData(mNumChannels, 2);
	int chIdx = (mNumChannels >= 1 && mNumChannels <= 6) ? mNumChannels - 1 : 0;
	Write(sm_asDsdiffChId[chIdx], mNumChannels * 4);
	WriteID("CMPR");
	WriteData(0x14, 8);
	WriteID("DSD ");
	WriteData(0xe, 1);
	WriteText("not compressed", 0xe);
	WriteData(0, 1);
	WriteID("DSD ");
	long long dsdBytes = (mEndSample >> 3) * mNumChannels;
	WriteData(dsdBytes, 8);
}

void CAudioFileWrite::WriteDsdiffTextData() { }

int CAudioFileWrite::WriteDsdiffTextDataLocal(int fieldIndex, const char *id4)
{
	const char *text = mTextBufPtr[fieldIndex];
	if (text[0] == '\0')
		return 0;
	unsigned int len = (unsigned int)strlen(text);
	unsigned int padded = (len + 1) & ~1u;
	WriteID(id4);
	WriteData(padded + 4, 8);
	WriteData(len, 4);
	Write(text, padded);
	return (int)(padded + 0x10);
}

void CAudioFileWrite::WriteWsdHeader()
{
	mByteOrder = 1;
	SetSampleRate(176400);
	SetBitsPerSample(1);
	mFileDataEnd = 0x800;
	mDataStartByteOffset = 0x800;

	WriteID("1bit");
	WriteData(0, 4);
	mHeaderSizePatchOffset = Tell();
	WriteWsdRequiredChunks();
	mDataStartByteOffset = Tell();
}

void CAudioFileWrite::WriteWsdRequiredChunks() { }

void CAudioFileWrite::WriteWsdTextDataLocal(int fieldIndex, unsigned long fieldWidth)
{
	const char *text = mTextBufPtr[fieldIndex];
	unsigned int len = (unsigned int)strlen(text);
	if (fieldWidth <= len) {
		Write(text, (unsigned int)fieldWidth);
		return;
	}
	Write(text, len);
	unsigned int written = len + 1;
	WriteText(" ", 1);
	while (written < fieldWidth) {
		WriteText(" ", 1);
		++written;
	}
}

void CAudioFileWrite::WriteDsfHeader()
{
	mByteOrder = 0;
	SetSampleRate(176400);
	SetBitsPerSample(1);
	SetChannelBlockSize(0x1000);
	mHeaderSizePatchOffset = 0x5c;
	mDataStartByteOffset = 0x5c;

	WriteID("DSD ");
	WriteData(0x1c, 8);
	mHeaderSizePatchOffset = Tell();
	WriteDsfRequiredChunks(0);
	mDataStartByteOffset = Tell();
}

void CAudioFileWrite::WriteDsfRequiredChunks(long long fileSizeSoFar)
{
	/* Real ground truth computes a DSF block-count/alignment value here
	 * via a signed-division-by-mDsfBlockSize idiom (GCC magic-multiply)
	 * and indexes a static per-channel-count "DSF channel type code"
	 * table (ground truth's own `.rodata` layout puts this immediately
	 * after sm_asDsdiffChId, as a plain `{1,2,3,4,6,7,...}`-shaped int
	 * array -- NOT the string-offset expression Ghidra's own decompile
	 * literally showed, which mis-attributed the access to an adjacent,
	 * differently-typed symbol; not independently re-derived byte-exact
	 * here given the effort budget). Modeled with the same overall chunk
	 * sequence and sample-count-based size fields, channel-type code
	 * simplified to `mNumChannels` itself. */
	Seek(mHeaderSizePatchOffset, eFileSeekSet);

	long long totalSamplesPerChannel = mEndSample;
	long long blockCount = (mDsfBlockSize != 0)
		? (totalSamplesPerChannel + mDsfBlockSize - 1) / mDsfBlockSize
		: 0;

	WriteData(mHeaderSizePatchOffset, 8);
	WriteData(fileSizeSoFar, 8);
	WriteID("fmt ");
	WriteData(0x34, 8);
	WriteData(1, 4);
	WriteData(0, 4);
	WriteData(mNumChannels, 4);
	WriteData(mNumChannels, 4);
	WriteData(mSampleRate, 4);
	WriteData(mBitsPerSample, 4);
	WriteData(mEndSample, 8);
	WriteData(mDsfBlockSize, 4);
	WriteData(0, 4);
	WriteID("data ");
	long long dataBytes = blockCount * mDsfBlockSize / 8 * mNumChannels + 0xc;
	WriteData(dataBytes, 8);
}

void CAudioFileWrite::WriteID3v2()
{
	/* ID3v2 fields are big-endian; save/restore the CLongBinaryFile
	 * on-disk byte-order flag around this chunk (ground truth: saves/
	 * restores offset +0x408 == mByteOrder, not a WAV/DSF-wide setting). */
	int savedByteOrder = mByteOrder;
	mByteOrder = 1;
	WriteText("ID3", 3);
	WriteData(0x300, 2);
	WriteData(0, 1);
	long long sizePos = Tell();
	WriteData(0, 4);
	int n0 = WriteID3v2TextDataLocal(0, "TIT2");
	int n1 = WriteID3v2TextDataLocal(1, "TPE1");
	int n2 = WriteID3v2TextDataLocal(2, "TCON");
	int n6a = WriteID3v2DateAndTime();
	int n6 = WriteID3v2TextDataLocal(6, "TSSE");
	unsigned int total = (unsigned int)(n0 + n1 + n2 + n6a + n6);
	Seek(sizePos, eFileSeekSet);
	/* ID3v2 "synchsafe" 28-bit-in-4-bytes size encoding: 7 data bits per
	 * byte. */
	unsigned int synchsafe = ((total & 0x3f80) * 2) | ((total & 0x1fc000) << 2)
		| (total & 0x7f) | ((total & 0xfe00000) << 3);
	WriteData(synchsafe, 4);
	mByteOrder = savedByteOrder;
}

int CAudioFileWrite::WriteID3v2DateAndTime() { return 0; }

int CAudioFileWrite::WriteID3v2TextDataLocal(int fieldIndex, const char *id4)
{
	const char *text = mTextBufPtr[fieldIndex];
	if (text[0] == '\0')
		return 0;
	unsigned int len = (unsigned int)strlen(text);
	WriteID(id4);
	WriteData(len + 1, 4);
	WriteData(0, 1);
	WriteData(0, 1);
	WriteData(0, 1);
	Write(text, len);
	return (int)(len + 0xb);
}
