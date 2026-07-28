/*
 * audio_file.h  -  CAudioFile (abstract base), CAudioFileRead,
 * CAudioFileReadEx (double-buffered reader), CAudioFileWrite: Eva's
 * WAV/DSDIFF/WSD/DSF audio-file reader/writer family, built directly on
 * CLongBinaryFile (long_binary_file.h -- see that file's header comment for
 * the candidate-selection story and the slot-numbering bug caught and
 * corrected mid-reconstruction).
 *
 * Real inheritance, confirmed via a full programmatic `.rodata` vtable
 * dump of all 4 classes (not sampled/guessed): `CAudioFile : public
 * CLongBinaryFile` (non-virtual, single inheritance -- CAudioFile's vtable
 * slots 2..15 are literal copies of CLongBinaryFile's own function
 * pointers), then `CAudioFileRead : public CAudioFile`,
 * `CAudioFileReadEx : public CAudioFileRead`, `CAudioFileWrite : public
 * CAudioFile`. `CAudioFile` itself is ABSTRACT: vtable slot 16
 * (`OpenFile`, code offset 0x40) is `__cxa_pure_virtual` in CAudioFile's
 * own vtable, confirmed unresolved to any real symbol; CAudioFileRead/
 * CAudioFileReadEx/CAudioFileWrite each override it with a real
 * OpenFile(). `CloseFile()` (slot 17) DOES have a real CAudioFile-level
 * default body (closes+resets), overridden only by CAudioFileReadEx and
 * CAudioFileWrite for format-specific finalization.
 *
 * Every method below is annotated with its real vtable slot number and the
 * exact `this->vptr[slot]` code offset (`slot*4`, confirmed against
 * `CLongBinaryFile::MoveToEnd()`'s known-good `Seek(0,0,2)` call at code
 * offset 0x20 = slot 8) -- every virtual call site in the original
 * disassembly was re-resolved against a full per-class slot table
 * generated from the real `.rodata` vtables (scratchpad tooling), not by
 * hand, after an earlier hand-computed `(offset-8)/4` formula was found to
 * be off by one slot on several calls (e.g. it would have named
 * `CAudioFile::CloseFile()`'s own 2 calls `GetFileName`/`Open` instead of
 * the real `Close()`/`Reset()`).
 *
 * Field layout (all offsets from `this`, i.e. `CLongBinaryFile`-relative;
 * `CAudioFile`'s own fields start at 0x40c == sizeof(CLongBinaryFile)):
 *
 *   0x40c  bool          mFileIsOpen
 *   0x410  int           mSampleRate           (default 44100, Reset())
 *   0x414  int           mBitsPerSample        (default 16)
 *   0x418  int           mNumChannels          (default 2)
 *   0x41c  short         mFormatTag            (WAVE_FORMAT_PCM=1 / _FLOAT=3)
 *   0x420  int           mBlockAlign           (bytes/frame, CalcBlockAlign())
 *   0x424  bool          mLoopEnable
 *   0x428  unsigned int  mLoopStart
 *   0x42c  unsigned int  mLoopEnd
 *   0x430  long long     mHeaderSizePatchOffset (Tell() right after a chunk-
 *                                                 size placeholder, patched
 *                                                 back in on Close())
 *   0x438  long long     mDataStartByteOffset  (file offset of sample data)
 *   0x440  long long     mStartSample          (Crop() start, sample-domain)
 *   0x448  long long     mFileDataEnd          (default FindChunk search
 *                                                 bound = RIFF size + 8)
 *   0x450  long long     mEndSample            (Crop() end / total samples)
 *   0x458  long long     mCurSample            (current absolute sample pos)
 *   0x460  long long     mSavedDataStartByteOffset (Crop()/CropCancel() backup
 *                                                     of mDataStartByteOffset)
 *   0x468  long long     mAbsSampleOffsetBase
 *   0x470  long long     mSavedEndSample       (Crop()/CropCancel() backup
 *                                                 of mEndSample)
 *   0x478  int           mDsfBlockSizeShift    (default 0xc, log2 of below)
 *   0x47c  unsigned int  mDsfBlockSize         (default 0x1000, DSF-format
 *                                                 samples-per-block)
 *   0x480  unsigned int  mBufferFrameOffset    (write/read cursor within
 *                                                 the current audio buffer,
 *                                                 in frames)
 *   0x484  int           mAudioFileFormat      (WAV=0/1, DSDIFF=2, WSD=3,
 *                                                 DSF=4 for CAudioFileWrite;
 *                                                 doubles as a "has BWF
 *                                                 chunk" 0/1 flag set by
 *                                                 CAudioFileRead::OpenWaveFile())
 *   0x488  int           mAvgBytesPerSec       (CalcAvgBytesPerSec())
 *   0x48c  unsigned int  mBufferFrameCount     (mAudioBufferSize / mBlockAlign)
 *   0x490  unsigned int  mAudioBufferSize      (malloc'd buffer size, bytes)
 *   0x494  char*         mTextBufPtr[7]        (points into mTextStorageN[])
 *   0x4b0  unsigned int  mTextBufSize[7]       ({0x80,0x80,0x20,0x20,10,8,0x50})
 *   0x4cc..0x686  char   mTextStorage0..6[]    (7 fixed metadata-text buffers,
 *                                                 sizes above +4 bytes slack
 *                                                 each -- ID3v2/WAV-LIST/
 *                                                 DSDIFF comment-style tags;
 *                                                 exact semantic field
 *                                                 identity, e.g. which index
 *                                                 is "Artist" vs "Title", not
 *                                                 recovered beyond index
 *                                                 order -- every real caller
 *                                                 only ever uses the numeric
 *                                                 index)
 *   0x68c  CPcmFilter    mPcmFilter            (already reconstructed,
 *                                                 pcm_filter.h; ground truth
 *                                                 is 24 bytes here, 4 more
 *                                                 than the reconstructed
 *                                                 class's own `sizeof` --
 *                                                 pcm_filter.h's own header
 *                                                 comment already flags an
 *                                                 unmodeled leading field at
 *                                                 its own +0x04; compensated
 *                                                 here with an explicit
 *                                                 `mPcmFilterPad0`, verified
 *                                                 via `static_assert`s in
 *                                                 audio_file.cpp)
 *   0x6a4  void*         mAudioBuffer          (malloc'd, mAudioBufferSize bytes)
 *   0x6a8  == sizeof(CAudioFile), where CAudioFileReadEx's own fields begin.
 *
 * `CAudioFileRead` adds no new fields (only vtable overrides + `Crop()`/
 * `CropCancel()` reusing CAudioFile's own 0x460/0x468/0x470 backup slots).
 *
 * `CAudioFileReadEx` (double-buffered/background-prefetch reader) adds, from
 * 0x6a8: 2 reserved/zeroed ints, `SFilePointer **mFilePtrIndirect` (+0x6b0,
 * ctor sets it to `&mFile`, i.e. `this+4` -- lets BufferAudio() dereference
 * through a generic `CFileBufferParams` without needing `this`), `void
 * *mCurrentReadPtr` (+0x6b4), `bool *mFileIsOpenIndirect` (+0x6b8, ctor sets
 * `&mFileIsOpen`), `unsigned int mDoubleBufferBlockBytes` (+0x6bc),
 * `unsigned int mNextBlockIndex` (+0x6c0), `unsigned int mSavedEndSampleLo`/
 * `int mSavedEndSampleHi` (+0x6c4/+0x6c8, per-refill snapshot of mEndSample),
 * `unsigned int mLastBlockFrameCount` (+0x6cc), `void *mDoubleBuffer`
 * (+0x6d0, a SECOND malloc'd buffer the same size as mAudioBuffer), `void
 * *mAudioBufferCopy` (+0x6d4, == mAudioBuffer, ctor-copied), `void
 * *mCurrentBufferBase` (+0x6d8, starts == mDoubleBuffer), `unsigned int
 * mBufferToggleIndex` (+0x6dc, `&1` selects between mDoubleBuffer/+0x6d0 and
 * mAudioBufferCopy/+0x6d4 -- real ping-pong double buffering for read-ahead),
 * `long mBufferSkipRate` (+0x6e0, Get/SetBufferSkipRate()).
 *
 * `CAudioFileWrite` adds no new fields either -- purely format-specific
 * vtable overrides (WriteWavHeader/WriteDsdiffHeader/WriteWsdHeader/
 * WriteDsfHeader + their own RequiredChunks/TextDataLocal siblings).
 */

#ifndef AUDIO_FILE_H
#define AUDIO_FILE_H

#include "long_binary_file.h"
#include "pcm_filter.h"

/* RIFF/WAV-style 8-byte on-disk chunk header (4-byte size field) --
 * CAudioFile::ReadChunkHeader(CChunkHeader8*) reads `id`, then a 4-byte
 * `size` via ReadData(4), then peeks the NEXT chunk's `id` into `peekId`
 * (Jump(-4) un-reads it) -- 12 bytes in memory, "8" names the on-disk
 * header size (id+4-byte-size). */
struct CChunkHeader8 {
	char id[4];
	unsigned int size;
	char peekId[4];
};

/* DSDIFF-style 12-byte on-disk chunk header (8-byte size field) -- same
 * shape as CChunkHeader8 but with a 64-bit `size`; 16 bytes in memory, "12"
 * names the on-disk header size (id+8-byte-size). */
struct CChunkHeader12 {
	char id[4];
	unsigned long long size;
	char peekId[4];
};

/* ID3v2 frame header: 4-byte id, 4-byte size, 2 flag bytes. */
struct CID3v2FrameHeader {
	char id[4];
	unsigned int size;
	unsigned char flags1;
	unsigned char flags2;
};

/* CAudioFileReadEx::BufferAudio()'s own parameter block -- a generic
 * "refill one double-buffer block" descriptor built from raw field
 * addresses inside a CAudioFileReadEx instance (see mFilePtrIndirect/
 * mFileIsOpenIndirect above), so BufferAudio() itself needs no `this`. */
struct CFileBufferParams {
	SFilePointer **pFilePtr;
	void *buffer;
	bool *pIsOpen;
	unsigned int size;
	int blockIndex;
};

/* Abstract base -- OpenFile() is pure virtual (confirmed __cxa_pure_virtual
 * in ground truth's own vtable). See header comment for full field list. */
class CAudioFile : public CLongBinaryFile {
public:
	/* Compile-time-only offsetof()/sizeof() checks against the field
	 * list documented above -- defined in audio_file.cpp, never called
	 * at runtime. A free function can't offsetof() a protected member
	 * from outside the class, hence this lives as a member. */
	static void VerifyLayout();

	/* .text+0x082fbca0, 271 bytes. Allocates the 7 text buffers' sizes,
	 * the mAudioBufferSize-byte sample buffer, then calls Reset(). */
	explicit CAudioFile(unsigned long audioBufferSize);

	/* .text+0x082fbae0 (D1) / 0x082fbc10 (D0, + HAL-bracketed free(this),
	 * see long_binary_file.h's convention note). Frees mAudioBuffer. */
	virtual ~CAudioFile();

	/* slot16, pure virtual. Real signature per every override:
	 * (path, format-or-unused, numChannels-or-unused) -> status. */
	virtual int OpenFile(const char *path, int arg2, int arg3) = 0;

	/* slot5, .text+0x082fb870, 623 bytes. Overrides CLongBinaryFile's own
	 * Reset() (calls it at the very end via CLongBinaryFile::Reset(),
	 * confirmed in the raw disassembly). Resets every CAudioFile-owned
	 * field to its ground-truth default: mFileIsOpen=false;
	 * mSampleRate=44100; mBitsPerSample=16; mNumChannels=2; the 12
	 * position/offset int64 fields (0x430-0x474) to 0; mDsfBlockSizeShift
	 * =0xc; mDsfBlockSize=0x1000; mBlockAlign/mBufferFrameOffset/
	 * mAudioFileFormat/mAvgBytesPerSec/mBufferFrameCount=0;
	 * mFormatTag=0; mLoopEnable=false; mLoopStart=mLoopEnd=0; zeroes
	 * (size+4 bytes each) all 7 text storage buffers; zeroes
	 * mAudioBuffer (mAudioBufferSize bytes). */
	virtual void Reset();

	/* slot17, .text+0x082faca0, 48 bytes. Real default body: if
	 * mFileIsOpen, Close() then Reset(). */
	virtual void CloseFile();

	/* slot18. */
	virtual bool FileIsOpen();

	/* slot19, .text+0x082fbb60, 139 bytes. Reallocs mAudioBuffer if the
	 * size actually changed, then calls CalcBlockAlign() (slot52). */
	virtual void SetAudioBufferSize(unsigned long size);

	/* slot20, .text+0x082face0, 42 bytes. Calls CalcAvgBytesPerSec()
	 * (slot51). Returns the new mSampleRate. */
	virtual int SetSampleRate(int sampleRate);

	/* slot21, .text+0x082fb820, 66 bytes. Calls CalcBlockAlign() (slot52),
	 * then CPcmFilter::SetBitsPerSample(). Returns the new value. */
	virtual int SetBitsPerSample(int bits);

	/* slot22, .text+0x082faee0, 167 bytes. Real ground truth computes
	 * `bitLength(blockSize)` (0 if blockSize==0, else the position of the
	 * highest set bit + 1) into mDsfBlockSizeShift -- collapsed from a
	 * GCC-unrolled bit-scan loop to the equivalent plain loop. Calls
	 * CalcBlockAlign() (slot52). Returns the stored mDsfBlockSize. */
	virtual unsigned int SetChannelBlockSize(unsigned long blockSize);

	/* slot23, .text+0x082faf90, 42 bytes. Calls CalcBlockAlign() (slot52).
	 * Returns the new mNumChannels. */
	virtual int SetNumChannels(int channels);

	/* slot24. */
	virtual int GetSampleRate();
	/* slot25. */
	virtual int GetBitsPerSample();
	/* slot26. */
	virtual int GetNumChannels();
	/* slot27. */
	virtual int GetAudioFileFormat();

	/* slot28. */
	virtual void SetLoopEnable(bool enable);
	/* slot29. */
	virtual void SetLoopStart(unsigned long start);
	/* slot30. */
	virtual void SetLoopEnd(unsigned long end);

	/* slot31, .text+0x082fafe0, 198 bytes. Clamps `sample` into
	 * [mStartSample, mEndSample], stores it as mCurSample; if mFileIsOpen,
	 * seeks to mCurSample*mBlockAlign + mDataStartByteOffset (slot8,
	 * Seek) and clears mBufferFrameOffset. Returns the clamped sample
	 * position actually stored. */
	virtual long long SetSamplePosition(long long sample);

	/* slot32, .text+0x082fb0b0, 57 bytes. Real ground truth: converts
	 * (hour,minute,secondsWithFraction) to a sample count and TAIL-CALLS
	 * SetSamplePosition (slot31) -- so the real return type is whatever
	 * SetSamplePosition returns, not void. */
	virtual long long SetCurTime(int hour, int minute, float secondsWithFraction);

	/* slot33, .text+0x082fb0f0, 50 bytes. Real ground truth: calls
	 * SetSamplePosition(seconds*mSampleRate) (slot31), then TAIL-CALLS
	 * GetCurTimeSecond() (slot37) -- returns the resulting actual time,
	 * not void. */
	virtual long double SetCurTimeSecond(double seconds);

	/* slot34. mCurSample - mStartSample. */
	virtual long long GetSamplePosition();
	/* slot35. mAbsSampleOffsetBase + mCurSample. */
	virtual long long GetAbsSamplePosition();

	/* slot36, .text+0x082fad50, 110 bytes. Calls GetCurTimeSecond()
	 * (slot37, a pure hook every subclass shares via CAudioFile's own
	 * body -- NOT overridden anywhere in this family) and splits it into
	 * hour/minute/secondsWithFraction. */
	virtual void GetCurTime(int *hour, int *minute, float *secondsWithFraction);

	/* slot37, .text+0x082fadc0, 77 bytes. mCurSample / mSampleRate (0.0
	 * if mSampleRate==0). NOT overridden by any subclass in this family
	 * (i.e. every subclass's "current time" is derived from the sample
	 * position CAudioFile itself tracks, not a format-specific hook). */
	virtual long double GetCurTimeSecond();

	/* slot38. mEndSample - mStartSample. */
	virtual long long GetNumSamples();

	/* slot39, .text+0x082fae10, 110 bytes. Same split as GetCurTime()
	 * but sourced from GetTotalTimeSecond() (slot40). */
	virtual void GetTotalTime(int *hour, int *minute, float *secondsWithFraction);

	/* slot40, .text+0x082fae80, 76 bytes. GetNumSamples() (slot38) /
	 * mSampleRate (0.0 if mSampleRate==0). */
	virtual long double GetTotalTimeSecond();

	/* slot41, .text+0x082fb760, 57 bytes. Read()s 4 bytes via Read()
	 * (slot6) if mFileIsOpen, else zero-fills. */
	virtual void ReadID(char *id4);
	/* slot42, .text+0x082fb1d0, 46 bytes. Writes 4 bytes via Write()
	 * (slot7) if mFileIsOpen. */
	virtual void WriteID(const char *id4);

	/* slot43. */
	virtual void ReadID3v2FrameHeader(CID3v2FrameHeader *out);
	/* slot44. */
	virtual void ReadChunkHeader(CChunkHeader8 *out);
	/* slot45. */
	virtual void ReadChunkHeader(CChunkHeader12 *out);

	/* slot46, .text+0x082fb680, 217 bytes. Linear scan via
	 * ReadID3v2FrameHeader (slot43) until `id4` matches or the search
	 * range/EOF is exhausted. Zeroes *out and returns 0 on failure. */
	virtual int FindID3v2Frame(const char *id4, CID3v2FrameHeader *out,
	                            long long searchStart, long long searchEnd);

	/* slot47, .text+0x082fb590, 230 bytes. Linear scan via
	 * ReadChunkHeader(CChunkHeader8*) (slot44, code offset 0xb0), Jump()ing
	 * past each non-matching chunk's (word-aligned) payload. Search range
	 * defaults to [0, mFileDataEnd) when both searchStart/searchEnd are 0.
	 * Zeroes *out and returns 0 on failure. */
	virtual int FindChunk(const char *id4, CChunkHeader8 *out,
	                       long long searchStart, long long searchEnd);

	/* slot48, .text+0x082fb3d0, 59 bytes. Re-verified against raw
	 * disassembly after Ghidra's own decompile of this one merged/
	 * mis-split its parameters beyond reliable hand-reading. Real
	 * semantics: repeatedly calls the 4-arg FindChunk(wantedId, out,...)
	 * overload (slot47) -- i.e. `wantedId`, not `id4`, is what's matched
	 * against each candidate chunk's OWN id -- then strncmp()s `id4`
	 * against `out->peekId` (the chunk's peeked-and-unread embedded type
	 * tag, e.g. a RIFF "LIST" chunk's own "INFO"/"adtl" sub-type). On a
	 * peekId mismatch, retries from Tell() (ground truth does NOT skip
	 * past the mismatched chunk's payload here -- reproduced exactly).
	 * Real caller: `OpenWaveFile()`'s `FindChunk("LIST","INFO",...)`. */
	virtual int FindChunk(const char *wantedId, const char *id4,
	                       CChunkHeader8 *out, long long searchStart, long long searchEnd);

	/* slot49, CChunkHeader12 counterpart of slot47 (.text+0x082fb480,
	 * 256 bytes; uses ReadChunkHeader(CChunkHeader12*), slot45 code
	 * offset 0xb4, and Jump()s past 2-byte- rather than -- DSDIFF
	 * payloads are NOT word-aligned the same way, ground truth's own
	 * Jump() advance here is a plain `size+1 & ~1` -- reproduced as-is). */
	virtual int FindChunk(const char *id4, CChunkHeader12 *out,
	                       long long searchStart, long long searchEnd);

	/* slot50, CChunkHeader12 counterpart of slot48 -- same peekId-based
	 * semantics (see slot48's note), implemented analogously; not
	 * independently re-verified against raw disassembly (never called
	 * anywhere in this family -- DSDIFF reading is not implemented, see
	 * CAudioFileRead::OpenDsdiffFile()'s own note) so this is modeled by
	 * symmetry with the confirmed CChunkHeader8 overload rather than its
	 * own disassembly. */
	virtual int FindChunk(const char *wantedId, const char *id4,
	                       CChunkHeader12 *out, long long searchStart, long long searchEnd);

	/* slot51, .text+0x082fb190, 56 bytes. mBitsPerSample==1 (DSD 1-bit):
	 * (mNumChannels*mSampleRate)>>3; else mSampleRate*mBlockAlign. */
	virtual void CalcAvgBytesPerSec();

	/* slot52, .text+0x082fb7a0, 118 bytes. mBlockAlign =
	 * (mBitsPerSample>>3)*mNumChannels; mBufferFrameCount =
	 * mAudioBufferSize/mBlockAlign; zeroes+refills mAudioBuffer; calls
	 * CalcAvgBytesPerSec() (slot51). */
	virtual void CalcBlockAlign();

	/* Non-virtual utilities -- __cdecl free functions in ground truth
	 * (no vtable slot, confirmed by the missing `this` in every real
	 * disassembly), kept as plain (non-member-dispatching) static
	 * methods here for the same reason CPcmFilter's multichannel
	 * utilities are static (pcm_filter.h). */

	/* .text+0x082fbde0, 53 bytes. (minute*60 + hour*3600 + secondsWithFraction) * sampleRate. */
	static long long TimeToSample(int hour, int minute, float secondsWithFraction, int sampleRate);
	/* .text+0x082fbe20, 163 bytes. Inverse of TimeToSample. */
	static void SampleToTime(long long sample, int sampleRate, int *hour, int *minute, float *secondsWithFraction);
	/* .text+0x082fbed0, 95 bytes. */
	static void SecondToTime(double seconds, int *hour, int *minute, float *secondsWithFraction);
	/* .text+0x082fbf30, 27 bytes. */
	static long long SecondToSample(double seconds, int sampleRate);
	/* .text+0x082fbf50, 64 bytes. 0.0 if sampleRate==0. */
	static long double SampleToSecond(long long sample, int sampleRate);

protected:
	/* .text+0x082fb1d0-adjacent helpers used by CAudioFileWrite -- exposed
	 * protected since format writers live in a derived class. */
	bool mFileIsOpen;                 /* +0x40c */
	int mSampleRate;                  /* +0x410 */
	int mBitsPerSample;                /* +0x414 */
	int mNumChannels;                  /* +0x418 */
	short mFormatTag;                  /* +0x41c */
	int mBlockAlign;                   /* +0x420 */
	bool mLoopEnable;                  /* +0x424 */
	unsigned int mLoopStart;           /* +0x428 */
	unsigned int mLoopEnd;             /* +0x42c */
	long long mHeaderSizePatchOffset;  /* +0x430 */
	long long mDataStartByteOffset;    /* +0x438 */
	long long mStartSample;            /* +0x440 */
	long long mFileDataEnd;            /* +0x448 */
	long long mEndSample;              /* +0x450 */
	long long mCurSample;              /* +0x458 */
	long long mSavedDataStartByteOffset; /* +0x460 */
	long long mAbsSampleOffsetBase;    /* +0x468 */
	long long mSavedEndSample;         /* +0x470 */
	int mDsfBlockSizeShift;            /* +0x478 */
	unsigned int mDsfBlockSize;        /* +0x47c */
	unsigned int mBufferFrameOffset;   /* +0x480 */
	int mAudioFileFormat;              /* +0x484 */
	int mAvgBytesPerSec;               /* +0x488 */
	unsigned int mBufferFrameCount;    /* +0x48c */
	unsigned int mAudioBufferSize;     /* +0x490 */

	char *mTextBufPtr[7];              /* +0x494 */
	unsigned int mTextBufSize[7];      /* +0x4b0 */
	char mTextStorage0[0x84];          /* +0x4cc */
	char mTextStorage1[0x84];          /* +0x550 */
	char mTextStorage2[0x24];          /* +0x5d4 */
	char mTextStorage3[0x24];          /* +0x5f8 */
	char mTextStorage4[0x0e];          /* +0x61c */
	char mTextStorage5[0x0c];          /* +0x62a */
	char mTextStorage6[0x54];          /* +0x636 */

	unsigned int mPcmFilterPad0;        /* compensates CPcmFilter's own
	                                      * unmodeled +0x04 leading field,
	                                      * see header comment */
	CPcmFilter mPcmFilter;             /* +0x68c (ground truth) */
	void *mAudioBuffer;                 /* +0x6a4 */
};

/* Concrete reader: WAV/DSDIFF/WSD/DSF -> PCM (int or float) sample arrays. */
class CAudioFileRead : public CAudioFile {
public:
	/* .text+0x082fd4e0, 37 bytes. */
	explicit CAudioFileRead(unsigned long audioBufferSize);
	/* .text+0x082fd450 (D1) / 0x082fd490 (D0). Real body: CloseFile()
	 * then ~CAudioFile(). */
	virtual ~CAudioFileRead();

	/* slot16, .text+0x082fd260, 466 bytes. Dispatches on the file
	 * extension (.wav/.WAV/.p01/.p02 -> OpenWaveFile (slot63); .dff/.DFF
	 * -> OpenDsdiffFile (slot64); .wsd/.WSD -> OpenWsdFile (slot65);
	 * .dsf/.DSF -> OpenDsfFile (slot66); anything else -> CloseFile,
	 * status 3). After the format-specific open, snapshots
	 * mDataStartByteOffset/mEndSample into the Crop backup slots
	 * (0x460/0x470) and zeroes mStartSample/mAbsSampleOffsetBase
	 * (0x440/0x468) -- establishes the "uncropped" baseline. */
	virtual int OpenFile(const char *path, int arg2, int arg3);

	/* slot31, .text+0x082fc570, 147 bytes. Calls CAudioFile's own
	 * SetSamplePosition, then (if mFileIsOpen) refills mAudioBuffer via
	 * Read() (slot6) -- size is (mBufferFrameCount>>3)*mNumChannels for
	 * bit-packed formats (mAudioFileFormat 2/3/4 = DSDIFF/WSD/DSF) or
	 * mBlockAlign*mBufferFrameCount otherwise. */
	virtual long long SetSamplePosition(long long sample);

	/* slot53, .text+0x082fc110, 823 bytes. Clamps [cropStart,cropEnd]
	 * against [mStartSample,mEndSample] (bit-packed formats round to
	 * 8-sample/word boundaries), sets mStartSample/mEndSample to the
	 * clamped range, clears mAbsSampleOffsetBase, calls
	 * SetSamplePosition(0) (slot31) then returns GetNumSamples() (slot38). */
	virtual long long Crop(long long cropStart, long long cropEnd);

	/* slot54, .text+0x082fc460, 139 bytes. Restores mDataStartByteOffset/
	 * mEndSample from the 0x460/0x470 backups, clears mAbsSampleOffsetBase/
	 * mStartSample, calls SetSamplePosition(0) (slot31). Returns mEndSample. */
	virtual long long CropCancel();

	/* slot55/56 -- float/byte "generic" read dispatchers. Real ground
	 * truth: format in [2,4) -> ReadDsdData; format==4 -> ReadDsdData2;
	 * format 0/1 (and, ground truth's own real fallback, any format >4)
	 * -> ReadPcmData. Since DSD paths are all literal `return 0;` stubs
	 * (see below), only the PCM branch does real work in this family. */
	virtual int ReadAudioData(float **out, unsigned long count, int channels, float dummy);
	virtual int ReadAudioData(unsigned char **out, unsigned long count, int channels);

	/* slot57/58, .text+0x082fc6b0 (1241 bytes) / 0x082fc610 (153 bytes).
	 * The real PCM decode core: refills mAudioBuffer in mBufferFrameCount-
	 * frame chunks as needed, unpacks mBitsPerSample/8 bytes per sample
	 * per channel (GCC Duff's-device-unrolled in ground truth, collapsed
	 * to a plain loop here -- same license as pcm_filter.h) into `out`,
	 * left-justified into the top of a 32-bit word then arithmetic-
	 * shifted back down (sign-extends). Silence-pads any trailing
	 * requested-but-unavailable frames and (if channels==1 but the
	 * caller passed 2 destination buffers) duplicates channel 0 into
	 * channel 1. The float overload calls the long overload then
	 * CPcmFilter::IntToFloat(). Returns frames actually decoded (not
	 * counting the silence padding). */
	virtual int ReadPcmData(long **out, unsigned long count, int channels);
	virtual int ReadPcmData(float **out, unsigned long count, int channels);

	/* slot59-62, DSD variants -- real ground truth bodies are literal
	 * `return 0;` (byte size 3, i.e. `xor eax,eax; ret`) for every
	 * overload; this family never implements real DSD sample decode. */
	virtual int ReadDsdData(unsigned char **out, unsigned long count, int channels);
	virtual int ReadDsdData2(unsigned char **out, unsigned long count, int channels);
	virtual int ReadDsdData(float **out, unsigned long count, int channels, float dummy);
	virtual int ReadDsdData2(float **out, unsigned long count, int channels, float dummy);

	/* slot63, .text+0x082fcb90, 1725 bytes. Real RIFF/WAVE parser:
	 * "RIFF"===magic, "WAVE"===form type; if a "bext" (BWF) chunk is
	 * found, sets mAudioFileFormat=1 and reads the 7 text buffers as
	 * fixed-size BWF description/originator/reference/origination-date/
	 * origination-time fields (plus 2 more, sizes per header comment);
	 * else scans a "LIST"/"INFO" chunk for 5 named sub-chunks into text
	 * buffers 0-4 and, if found, a 6th text field from a following
	 * chunk. Then requires an "fmt " chunk: wFormatTag(2)/nChannels(2)/
	 * nSamplesPerSec(4)/nAvgBytesPerSec(4)/nBlockAlign(2)/
	 * wBitsPerSample(2), validated against {16/24-bit PCM (formatTag 1),
	 * 32-bit float (formatTag 3)} and {44100,48000,88200,96000,176400,
	 * 192000} Hz -- any other combination fails with status 3. Finally requires
	 * a "data" chunk; on success sets mDataStartByteOffset=Tell(),
	 * mEndSample=dataSize/mBlockAlign, mStartSample=0, and calls
	 * SetSamplePosition(0) (slot31). Returns 0 on success, 3 on any
	 * validation failure (via CloseFile(), slot17/44). */
	virtual int OpenWaveFile();

	/* slot64-66, real ground truth bodies are literal `return 0;`
	 * (3-byte stubs) -- DSDIFF/WSD/DSF reading is not implemented in
	 * this family (only writing is, see CAudioFileWrite). */
	virtual int OpenDsdiffFile();
	virtual int OpenWsdFile();
	virtual int OpenDsfFile();

	/* slot67, real ground truth body is a literal `ret` (1-byte, i.e.
	 * an empty function) -- ID3v2 tag reading is not implemented. */
	virtual void ReadID3v2();

protected:
	/* Real ground truth field name/identity not recovered beyond "the 3
	 * fixed-size BWF text fields OpenWaveFile() additionally reads when
	 * mAudioFileFormat becomes 1" -- reuses mTextStorage4/5/6 (indices
	 * 4/5/6) exactly as CAudioFile's own text-buffer array already
	 * models, no extra fields needed here. */
};

/* Double-buffered (background read-ahead) reader -- adds a second malloc'd
 * buffer ping-ponged against the base mAudioBuffer. See audio_file.h's own
 * top-of-file field list for the full 0x6a8+ layout. */
class CAudioFileReadEx : public CAudioFileRead {
public:
	/* .text+0x082fe0d0, 210 bytes. Allocates the 2nd buffer. */
	explicit CAudioFileReadEx(unsigned long audioBufferSize);
	/* .text+0x082fde30 (D1) / 0x082fe060 (D0). Frees the 2nd buffer, then
	 * ~CAudioFileRead(). */
	virtual ~CAudioFileReadEx();

	/* slot16/17 -- thin forwarders to CAudioFileRead::OpenFile/
	 * CAudioFile::CloseFile (ground truth: 13-byte tail-call stubs). */
	virtual int OpenFile(const char *path, int arg2, int arg3);
	virtual void CloseFile();

	/* slot31, .text+0x082fde90, 451 bytes. Real ground truth calls
	 * CAudioFileRead::SetSamplePosition() (which does its own real
	 * Read() into mAudioBuffer) and THEN an additional direct
	 * CFileOperation seek+read into the double-buffer, confirmed against
	 * raw disassembly -- read literally, the double-buffer ends up
	 * holding genuinely read-AHEAD data (one block past the base class's
	 * own mAudioBuffer/mCurSample bookkeeping). DEFERRED: reproducing
	 * that read-ahead semantics in ReadPcmData() below caused real
	 * decoded-sample corruption caught by verify/test_audio_file.cpp's
	 * host round-trip KAT test -- rolled back to a plain delegation to
	 * the base class (correct decoded output, double-buffered
	 * background-prefetch OPTIMIZATION not reproduced; see
	 * DECOMPILE_ERRORS.md). Resets mBufferToggleIndex to 0. */
	virtual long long SetSamplePosition(long long sample);

	/* slot57/58, .text+0x082fd700 (1839 bytes) / 0x082fd640 (153 bytes).
	 * Real ground truth: same per-sample unpack core as
	 * CAudioFileRead::ReadPcmData but reading from whichever of the 2
	 * double-buffers is current, refilling (inlined, not a call to the
	 * standalone BufferAudio()) via CFileOperation directly when the
	 * current block is exhausted. DEFERRED here -- see
	 * SetSamplePosition()'s own note just above; this delegates to
	 * CAudioFileRead::ReadPcmData() instead (verified correct decoded
	 * output via KAT test, background-prefetch optimization not
	 * reproduced). */
	virtual int ReadPcmData(long **out, unsigned long count, int channels);
	virtual int ReadPcmData(float **out, unsigned long count, int channels);

	/* slot59-62/71-72, all literal `return 0;` stubs, same as the base
	 * class. */
	virtual int ReadDsdData(unsigned char **out, unsigned long count, int channels);
	virtual int ReadDsdData2(unsigned char **out, unsigned long count, int channels);
	virtual int ReadDsdData(float **out, unsigned long count, int channels);
	virtual int ReadDsdData2(float **out, unsigned long count, int channels);

	/* slot68, .text+0x082fd510, 187 bytes. Same 3-way dispatch shape as
	 * CAudioFileRead::ReadAudioData(float**,...) but with its own extra
	 * slots 0x47/0x48 for the 3- and 4-byte-per-sample cases (never
	 * implemented in this family -- see CAudioFileRead's own note). */
	virtual int ReadAudioData(float **out, unsigned long count, int channels);

	/* slot69/70. */
	virtual void SetBufferSkipRate(long rate);
	virtual long GetBufferSkipRate();

	/* Non-virtual (__cdecl, no `this` in ground truth's own
	 * disassembly). .text+0x082fe1d0, 107 bytes. Seeks+reads one
	 * double-buffer block directly via CFileOperation, given a
	 * pre-built CFileBufferParams (see struct comment above). */
	static int BufferAudio(CFileBufferParams *params);

protected:
	int mReservedA;                    /* +0x6a8 */
	int mReservedB;                    /* +0x6ac */
	SFilePointer **mFilePtrIndirect;   /* +0x6b0, == &mFile */
	void *mCurrentReadPtr;             /* +0x6b4 */
	bool *mFileIsOpenIndirect;         /* +0x6b8, == &mFileIsOpen */
	unsigned int mDoubleBufferBlockBytes; /* +0x6bc */
	unsigned int mNextBlockIndex;      /* +0x6c0 */
	unsigned int mSavedEndSampleLo;    /* +0x6c4 */
	int mSavedEndSampleHi;             /* +0x6c8 */
	unsigned int mLastBlockFrameCount; /* +0x6cc */
	void *mDoubleBuffer;               /* +0x6d0 */
	void *mAudioBufferCopy;            /* +0x6d4, == mAudioBuffer */
	void *mCurrentBufferBase;          /* +0x6d8, starts == mDoubleBuffer */
	unsigned int mBufferToggleIndex;   /* +0x6dc */
	long mBufferSkipRate;              /* +0x6e0 */
};

/* Concrete writer: PCM (int or float) sample arrays -> WAV/DSDIFF/WSD/DSF. */
class CAudioFileWrite : public CAudioFile {
public:
	/* .text+0x08300580, 37 bytes. */
	explicit CAudioFileWrite(unsigned long audioBufferSize);
	/* .text+0x083004d0 (D1) / 0x08300510 (D0). Real body: CloseFile()
	 * then ~CAudioFile(). */
	virtual ~CAudioFileWrite();

	/* slot16, .text+0x082ff250, 263 bytes. CloseFile()s any prior file,
	 * opens for write (mode 0x1ff, see long_binary_file.h), sets
	 * mAudioFileFormat=arg2 (clamped channels arg3 to [1,6]), then
	 * dispatches on mAudioFileFormat: 0/1 -> WriteWavHeader (slot59);
	 * 2 -> WriteDsdiffHeader (slot64); 3 -> WriteWsdHeader (slot68); 4 ->
	 * WriteDsfHeader (slot71); default -> CloseFile(), status 2. */
	virtual int OpenFile(const char *path, int format, int channels);

	/* slot17, .text+0x08300160, 862 bytes. Format-specific finalization:
	 * flushes any partially-filled mAudioBuffer via Write() (slot7),
	 * writes format-specific trailer/text chunks (WriteWavTextData +
	 * WriteBwfChunk for format<2; WriteDsdiffTextData for format==2;
	 * nothing extra for WSD; WriteID3v2+WriteDsfRequiredChunks for
	 * DSF), then Seeks back (slot8) to mHeaderSizePatchOffset and
	 * WriteData()s the real final chunk size (Tell() minus the patch
	 * offset, minus 8 or 0xc depending on format) over the placeholder
	 * written by the matching Write*Header(). Then calls CAudioFile::
	 * CloseFile() (slot17's own base, via the corrected slot table --
	 * i.e. this override's own tail call). */
	virtual void CloseFile();

	/* slot21, .text+0x082ff9d0, 110 bytes. For mAudioFileFormat<2 (WAV):
	 * bits<=16 -> mFormatTag=1 (PCM), 16-bit; bits<=24 -> mFormatTag=1,
	 * 24-bit; else -> mFormatTag=3 (float), 32-bit. Else (DSDIFF/WSD/DSF):
	 * mFormatTag=0, 1 bit (DSD). Calls CAudioFile::SetBitsPerSample with
	 * the resolved bit depth. */
	virtual int SetBitsPerSample(int bits);

	/* slot53/54 -- byte/float "generic" write dispatchers analogous to
	 * CAudioFileRead's Read* dispatchers; real ground truth switches on
	 * (out-of-range channel-count sentinel checks against the same
	 * `param_1[0x121]` shape seen on the read side) and forwards to
	 * WritePcmData or a not-implemented DSD hook. */
	virtual int WriteAudioData(unsigned char **in, unsigned long count, int channels);
	virtual int WriteAudioData(float **in, unsigned long count, int channels);

	/* slot55/56, .text+0x082ff410 (1458 bytes) / 0x082ff380 (125 bytes).
	 * The real PCM encode core, mirror image of CAudioFileRead::
	 * ReadPcmData: packs each sample's low mBitsPerSample/8 bytes (GCC
	 * Duff's-device-unrolled in ground truth, collapsed to a plain loop)
	 * into mAudioBuffer, flushing (Write(), slot7) whenever
	 * mBufferFrameCount frames have accumulated, zero-filling any
	 * channels beyond mNumChannels present in `in`. The float overload
	 * calls CPcmFilter::FloatToInt() then the long overload. Returns
	 * frames actually written. */
	virtual int WritePcmData(long **in, unsigned long count, int channels);
	virtual int WritePcmData(float **in, unsigned long count, int channels);

	/* slot57/58, real ground truth bodies are literal `return 0;`
	 * stubs -- DSD encode is not implemented in this family. */
	virtual int WriteDsdData(unsigned char **in, unsigned long count, int channels);
	virtual int WriteDsdData2(unsigned char **in, unsigned long count, int channels);

	/* slot59, .text+0x082fe240, 245 bytes. Writes "RIFF"/placeholder
	 * size/"WAVE", records mHeaderSizePatchOffset = Tell() right after
	 * the placeholder, then (if mAudioFileFormat==1) WriteBwfChunk()
	 * (slot63, a no-op stub), then WriteWavRequiredChunks() (slot60),
	 * recording mDataStartByteOffset = Tell() at the very end. */
	virtual void WriteWavHeader();

	/* slot60, .text+0x082fe340, 907 bytes. Writes the "fmt " chunk (16 or
	 * 40 bytes depending on mLoopEnable... actually depending on
	 * mAudioFileFormat==1/BWF -- ground truth: extended "fmt " (0x3c
	 * bytes incl. WAVEFORMATEXTENSIBLE-shaped sub-fields) whenever
	 * mLoopEnable is set, else the plain 16-byte PCM "fmt "; the
	 * extended form encodes a 1ns-resolution "average time per sample"
	 * field (1e9/mSampleRate) and, only if mLoopEnable, the loop
	 * start/end sample numbers), then a placeholder "data" chunk size
	 * (mBlockAlign * total sample count so far). */
	virtual void WriteWavRequiredChunks();

	/* slot61, .text+0x082fe6e0, 458 bytes. If any of text buffers 1-5
	 * are non-empty, writes a "LIST"/"INFO" chunk containing whichever
	 * of the 5 corresponding WriteWavTextDataLocal() (slot62) fields are
	 * set, patching the LIST chunk's own size afterward via Seek+
	 * WriteData. */
	virtual void WriteWavTextData();

	/* slot62, .text+0x082ffe40, 790 bytes. Per-field WAV LIST/INFO
	 * sub-chunk writer: id + word-aligned-padded size + text + NUL +
	 * any extra pad byte to keep the chunk size even (GCC-unrolled pad
	 * loop, collapsed). Returns 0 if the field is empty, else
	 * paddedSize+8 (bytes written including its own 8-byte sub-header). */
	virtual int WriteWavTextDataLocal(int fieldIndex, const char *id4);

	/* slot63, real ground truth body is a literal `ret` (1-byte no-op)
	 * -- BWF (Broadcast Wave Format) "bext" chunk writing is not
	 * implemented (mAudioFileFormat can still be set to 1, but no BWF
	 * data is ever actually emitted). */
	virtual void WriteBwfChunk();

	/* slot64, literal `ret` no-op -- see WriteBwfChunk. */
	virtual void WriteDsdiffHeader();

	/* slot65, .text+0x082fe8d0, 608 bytes. Real DSDIFF ("FRM8"/"DSD "-
	 * family) chunk writer: seeks back to mHeaderSizePatchOffset,
	 * writes "PROP"/"SND "/"FS  "(sample rate)/"CHNL"(channel count +
	 * per-channel ID codes from the static `sm_asDsdiffChId` table)/
	 * "CMPR"(4-byte "not compressed" flag)/a "not compressed" text
	 * field, then the "DSD " sample-data chunk's own size placeholder
	 * (mEndSample>>3 * mNumChannels, i.e. bit-packed DSD byte count). */
	virtual void WriteDsdiffRequiredChunks();

	/* slot66, literal `ret` no-op -- see WriteBwfChunk. */
	virtual void WriteDsdiffTextData();

	/* slot67, .text+0x082ffd80, 187 bytes. Per-field DSDIFF text
	 * sub-chunk writer (8-byte id + 8-byte size + text, 2-byte aligned).
	 * Returns 0 if the field is empty, else paddedLen+0x10. */
	virtual int WriteDsdiffTextDataLocal(int fieldIndex, const char *id4);

	/* slot68, .text+0x082ff090, 199 bytes. Writes a fixed placeholder
	 * "size" field then records mHeaderSizePatchOffset/
	 * mDataStartByteOffset (both == Tell() at that point, i.e. WSD has
	 * no separate header-vs-data-start gap) and calls
	 * WriteWsdRequiredChunks() (slot69, a no-op stub in this family). */
	virtual void WriteWsdHeader();

	/* slot69, literal `ret` no-op. */
	virtual void WriteWsdRequiredChunks();

	/* slot70, .text+0x082ffa50, 549 bytes. Per-field WSD text writer:
	 * writes the string then space-pads (GCC-unrolled, collapsed) to
	 * exactly `fieldWidth` bytes (truncates if the string is already
	 * >= fieldWidth). */
	virtual void WriteWsdTextDataLocal(int fieldIndex, unsigned long fieldWidth);

	/* slot71, .text+0x082feb40, 231 bytes. Writes a fixed-size DSF
	 * header (sample rate forced to 176400, bits-per-sample forced to 1,
	 * DSF block size forced to 0x1000 via SetChannelBlockSize, slot22),
	 * records mHeaderSizePatchOffset/mDataStartByteOffset, calls
	 * WriteDsfRequiredChunks(0) (slot72). */
	virtual void WriteDsfHeader();

	/* slot72, .text+0x082fec30, 680 bytes. Real ground truth: writes a
	 * DSF-format "fmt " sub-chunk (format version/id/channel-type code
	 * from a static lookup table keyed by mNumChannels/channel count/
	 * sample rate/bits-per-sample/sample-count/block-size/reserved) then
	 * a "data " chunk with a size placeholder computed from the sample
	 * count rounded up to a whole DSF block. */
	virtual void WriteDsfRequiredChunks(long long fileSizeSoFar);

	/* slot73, .text+0x082feee0, 412 bytes. Writes an "ID3" v2.3 header
	 * (flags byte, size placeholder), then 3 WriteID3v2TextDataLocal()
	 * (slot75) fields + WriteID3v2DateAndTime() (slot74, a no-op stub in
	 * this family) + a 4th text field, then patches the tag size back in
	 * as a 4-byte ID3v2 "synchsafe" integer (7 data bits per byte). */
	virtual void WriteID3v2();

	/* slot74, literal `return 0;` no-op stub. */
	virtual int WriteID3v2DateAndTime();

	/* slot75, .text+0x082ffc80, 249 bytes. Per-field ID3v2 text frame
	 * writer (4-byte id + 4-byte size + 2 flag bytes + 1 encoding byte +
	 * text, no NUL). Returns 0 if the field is empty, else len+0xb. */
	virtual int WriteID3v2TextDataLocal(int fieldIndex, const char *id4);
};

#endif /* AUDIO_FILE_H */
