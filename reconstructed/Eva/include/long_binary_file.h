/*
 * long_binary_file.h  -  CLongBinaryFile, Eva's polymorphic thin wrapper
 * around the (out-of-scope) global `CFileOperation` file-I/O manager, plus
 * the CAudioFile family (audio_file.h) which is built directly on top of it.
 *
 * Found via a fresh `nm -C` class-inventory sweep for the next dense,
 * previously-100%-untouched cluster (2026-07-28, same standing goal as
 * fs_converter.h/pcm_filter.h). Two larger candidates were traced via full
 * call-xref sweeps and REJECTED before landing here: `CRTRouter`/
 * `CRTRouterApiInstance` (93 methods, reaches into `CTrackBase`'s raw
 * private layout, a separate 0%-done class) and the `CFileBase` file-format-
 * loader family (~74 methods, every sibling except `CFileBase`/
 * `CFileDX7Syx` calls directly into `CFMBrowseForm`, the project's named
 * exclusion). `CAudioFile` + `CLongBinaryFile` (127 methods total) is
 * clean: the only foreign dependency is 6 basic file-I/O primitives on
 * `CFileOperation` (Open/Close/Read/Write/Seek/Tell -- confirmed via a full
 * call-xref sweep of the whole family) and `CPcmFilter`, already
 * reconstructed (pcm_filter.h).
 *
 * Real class, NO base (`class_type_info`, `.rodata+0x08f3109c`), 16
 * methods, ALL virtual, `.text+0x083005b0..0x083014b0`. `CAudioFile`
 * (audio_file.h) inherits it publicly and non-virtually -- confirmed by a
 * direct `.rodata` vtable dump: `CAudioFile`'s own vtable slots 2..15 are
 * literal copies of `CLongBinaryFile`'s function pointers (inherited,
 * un-overridden slots), and `CAudioFile`'s own first field (`mFileIsOpen`)
 * sits at object offset 0x40c -- exactly `sizeof(CLongBinaryFile)`. Object
 * layout: vtable ptr (+0x00), `SFilePointer *mFile` (+0x04), `char
 * mFileName[1024]` (+0x08), `int mByteOrder` (+0x408, 0 = little-endian
 * on-disk multi-byte values, nonzero = big-endian -- see ReadData/WriteData
 * below). `Reset()` zeroes `mFileName`/`mByteOrder` but the CTOR does NOT
 * (ground truth: ctor only sets the vtable pointer and `mFile = 0`) --
 * reproduced faithfully; callers must call `Reset()` before relying on
 * `mFileName` being empty.
 *
 * A **slot-numbering bug was caught and corrected mid-reconstruction**:
 * early hand-disassembly analysis computed virtual-call slot indices as
 * `(codeOffset - 8) / 4` (mistakenly re-subtracting the vtable's own
 * offset-to-top/typeinfo header, which is already excluded once `this->
 * vptr` -- what code offsets are actually relative to -- points at slot 0).
 * The correct formula, confirmed against `CLongBinaryFile::MoveToEnd()`'s
 * real body (`(**(code**)(*this+0x20))(this,0,0,2)`, definitively a call to
 * `Seek(0, eSeekEnd)`), is `slot = codeOffset / 4`. A full programmatic
 * `.rodata` vtable dump + call-site annotation pass (scratchpad tooling,
 * not hand arithmetic) was used to re-verify every virtual call in both
 * this file and audio_file.h before writing any of the C++ below.
 *
 * `EOpenModeX`/`ESeekType`/`SFilePointer` and the `CFileOperation` extern
 * interface below are the minimal 6-method surface actually called
 * (Open/Close/Read/Write/Seek/Tell) -- `CFileOperation` itself is the same
 * out-of-scope global file manager named in this project's `CBatchDiskMan`/
 * `CEditClient`/`ControlMsgHandler` notes elsewhere (`CFileOperation::
 * NotifyOnKSCFileChanges()` et al.), not reconstructed here. `EOpenModeX`
 * meaning: `CLongBinaryFile::Open(path, modeFlag, unusedBool)` compares
 * `modeFlag` against the literal `0x1ff` to select write vs read mode --
 * `CAudioFileWrite::OpenFile()` always calls with the literal `0x1ff`;
 * `CAudioFileRead::OpenFile()` always calls with `0` -- so `0x1ff` is
 * reproduced as an opaque caller-chosen sentinel (its own derivation not
 * recovered), not guessed at. The `bool` 3rd parameter is real (part of the
 * mangled signature) but ground truth's own `Open()` body never reads it.
 *
 * `verify/test_long_binary_file.cpp` / `verify/test_audio_file.cpp` link
 * against `src/base/file_operation_stub.cpp`, a REAL host-functional
 * `CFileOperation` backed by stdio (`SFilePointer` wraps a `FILE*`) rather
 * than an inert stub -- this lets the audio-file KAT tests do genuine
 * round-trip file I/O (write a WAV, read it back, compare) instead of only
 * exercising pure/leaf logic.
 */

#ifndef LONG_BINARY_FILE_H
#define LONG_BINARY_FILE_H

#include <cstddef>

#include "file_io_base.h"	/* EDevice_Id, for CFileOperation::GetLinuxRemapPath() below --
				 * added 2026-07-28 for the CKorgPath/CKorgLinuxPath batch, see
				 * korg_path.h's own header comment. */

/* Opaque file handle -- real ground truth is `CFileOperation`'s own
 * internal per-open-file record; never dereferenced by CLongBinaryFile
 * itself, only carried around and passed back to CFileOperation. */
struct SFilePointer;

/* 0 = read, 1 = write -- see header comment for the `0x1ff` sentinel this
 * project's callers actually use to select eOpenModeWrite. */
enum EOpenModeX { eOpenModeRead = 0, eOpenModeWrite = 1 };

/* Matches CStream's own ESeekType (stream_family.h) in spirit; CFileOperation
 * has its own distinct enum of the same 3 values, confirmed via CLongBinaryFile
 * ::Jump() (eSeekCur) and ::MoveToEnd() (eSeekEnd). */
enum ESeekType { eFileSeekSet = 0, eFileSeekCur = 1, eFileSeekEnd = 2 };

/* Out-of-scope global file manager -- see header comment. Only the 6
 * methods CLongBinaryFile/CAudioFile actually call are declared. Real
 * ground-truth calls are plain (no visible receiver arg in the
 * disassembly), consistent with these being static members of a
 * process-wide singleton-style manager class -- modeled as `static`. */
class CFileOperation {
public:
	static SFilePointer *Open(const char *path, EOpenModeX mode);
	static void Close(SFilePointer *fp);
	static unsigned int Read(void *buf, unsigned int elemSize, unsigned int count, SFilePointer *fp);
	static unsigned int Write(const void *buf, unsigned int elemSize, unsigned int count, SFilePointer *fp);
	static int Seek(SFilePointer *fp, long offset, ESeekType type);
	static unsigned int Tell(SFilePointer *fp, unsigned long *outPos);

	/* .text+0x083d0190, 24 bytes. Added 2026-07-28 for
	 * UKontaktOposPath::ConvertOposToLinux()/ConvertLinuxToOpos()
	 * (kontakt_opos_path.h) -- real body is a 2-level static table lookup
	 * (`gDriveLetterRemap[id]` folded through a second `gDeviceMountPath[]`
	 * table, both `.data`), not reproduced here (same "extern-only slice
	 * of the out-of-scope god object" convention as Open/Close/Read/
	 * Write/Seek/Tell above) -- see file_operation_stub.cpp for the host
	 * KAT-test stand-in. */
	static const char *GetLinuxRemapPath(EDevice_Id id);

	/*
	 * Round 54 batch (2026-07-29, solo, 24 methods -- 23 landed here,
	 * `GetLinuxRemapPath` above already had a slot): a fresh `nm -C`
	 * sweep found `CFileOperation` is a REAL class with 126 methods in
	 * the actual binary, not merely the 7-method "out-of-scope slice"
	 * this header originally modeled. These 23 are genuine, fully
	 * reconstructed bodies (not host stand-ins) -- pure static-member
	 * reads/writes, no calls into unreconstructed code. See
	 * src/init/file_operation.cpp for the bodies.
	 *
	 * DEFERRED, 3 reasons (102 of 126 methods, not implemented this
	 * pass): `ConvertError`/`ConvertFilesysError` index into real
	 * compiler-generated switch/jump tables (`CSWTCH_423`/`CSWTCH_426`)
	 * whose actual `.rodata` contents aren't cheaply recoverable;
	 * `GetNumOfBlockOnMedium` forwards to `CDDriverIO::
	 * GetNumOfBlockOnMedium`, itself not reconstructed (outside
	 * CDDriverIO round 52's own safe subset); and ~87 more (including
	 * `SortDir`/`OpenNextpath`/`CloseLastSession`/`Finalize`/
	 * `TestDeviceChg`/`TestDiskChg`/`Unmount`/`Flush`/`Chmod`/`Chsize`/
	 * `GetMaxClusterno`/`TotalFreeCluster`) all funnel into
	 * `CFileOperation::Execute()`, a genuinely massive 23,258-byte
	 * central dispatcher -- by far the largest single method seen in
	 * either binary this session -- not attempted this pass; a
	 * dedicated future effort on `Execute()` itself would unblock this
	 * whole family at once.
	 */
	static unsigned char *Get1MMemory();
	static unsigned char *Get900KMemory();
	static int IsDirectExecCommand();
	static void EnableDirectIOCall(int enable);
	static void SetForceDiskChangeTestMode(int enable);
	static void EnableWaitPIDSignal(int enable);
	static void SetWaitTimeAfterSync(int seconds);
	static void SetForceDiskChangeTestEvent();
	static int GetForceDiskChangeTestEvent();

	static int Readblk(int deviceId);
	static int Writeblk(int deviceId);

	static void SetAsUsedScsiGenericNo(int deviceId, int value);
	static void SetAsUsedUSBDirectDeviceId(int deviceId, int value);
	static void SetAsUsedUSBDirectDeviceIndex(int deviceId, int value);
	static int GetUSBDirectAccessDeviceIndex(int deviceId);
	static int GetCDDeviceIndex(int deviceId);
	static void SetDiskInfoDirty(int deviceId);
	static int GetDiskInfoDirty(int deviceId);
	static int GetUSBDiskNumber(int index);
	static void SetUSBDiskNumber(int index, int value);
	static const char *GetLinuxMountPoint(int deviceId);
	static unsigned int GetResultBlocknoGetcurpos(int *audioStatus, unsigned char *trackNo);
	static void EnableFileCache(int enable);

	/* Public (not private) so host KAT tests can set up/observe state
	 * directly -- same convention as CDDriverIO round 52's own public
	 * statics. Sizes/bounds are INFERRED from confirmed usage in the 23
	 * landed methods (explicit bounds checks where present, e.g.
	 * `sm_bIsDiskInfoDirty`'s `param_1 < 10`), not proven by exhaustive
	 * caller analysis. Several arrays (`sm_iScsiGenericNoMap`,
	 * `sm_iUSBDirectDeviceIdMap`, `sm_iUSBDirectAccessDeviceIndex`,
	 * `sm_iCDDeviceIndex`) have NO explicit bounds check in the methods
	 * that write them; sized at 10 to match the already-established
	 * `EDevice_Id` 0..9 device-count convention confirmed via
	 * `CDDriverIO::Initialize()`'s own real construction loop (round
	 * 52).
	 */
	static unsigned char s_ucMem;
	static unsigned char s_900kMem;
	static int sm_bExecDirectCom;
	static int directIOCall;
	static int sm_bForceDiskChangeTestMode;
	static int s_bWaitPIDSignal;
	static int sSecondToWait;
	static int sm_bForceDiskChangeTestEventNotify;
	static int sm_iScsiGenericNoMap[10];
	static int sm_iUSBDirectDeviceIdMap[10];
	static int sm_iUSBDirectAccessDeviceIndex[10];
	static int sm_iCDDeviceIndex[10];
	static int sm_bIsDiskInfoDirty[10];
	static int sm_iUSBDiskNumber[127];
	static int s_iPrevUSBDiskNumber[127];
	static char s_akcLinuxMountPoint[10][15];
	static int s_eAudioSts;
	static unsigned char s_ucTrackNo;
	static unsigned int s_ulBlockNo;
	static unsigned char *sm_poFileCache;
	static int sm_bIsFileCacheEnable;
};

class CLongBinaryFile {
public:
	/* .text+0x08301470, 18 bytes. Real: sets vtable ptr + mFile=0 only --
	 * mFileName/mByteOrder are left uninitialized, see header comment. */
	CLongBinaryFile();

	/* .text+0x08301390 (D1) / 0x08301410 (D0, additionally
	 * `HAL_DisableInterrupts(); free(this); HAL_EnableInterrupts();` --
	 * not reproduced here, see this project's established "plain dtor,
	 * document the D0 HAL-bracket in a comment" convention, e.g.
	 * file_io_dos.cpp). Closes the file if still open. */
	virtual ~CLongBinaryFile();

	/* .text+0x083011d0, 32 bytes. strcpy(dest, mFileName). */
	virtual void GetFileName(char *dest);

	/* .text+0x083013c0, 79 bytes. See header comment for the mode/0x1ff
	 * note. Returns the raw SFilePointer* (nonzero = success). */
	virtual SFilePointer *Open(const char *path, int mode, bool unused);

	/* .text+0x08301360, 33 bytes. */
	virtual void Close();

	/* .text+0x08301320, 51 bytes. Zeroes mFileName[1024] + mByteOrder. */
	virtual void Reset();

	/* .text+0x083012e0, 59 bytes. Real: CFileOperation::Read(buf,1,count,mFile);
	 * returns `count` on success, 0 on failure (matches Write's shape). */
	virtual unsigned int Read(void *buf, unsigned int count);

	/* .text+0x083012a0, 59 bytes. */
	virtual unsigned int Write(const void *buf, unsigned int count);

	/* .text+0x08301250, 72 bytes. Real ground truth only forwards the LOW
	 * 32 bits of `offset` to CFileOperation::Seek (a plain `long`) --
	 * the high dword is silently discarded, reproduced faithfully (a
	 * genuine >2GB-file truncation bug in ground truth, not ours to fix).
	 * On CFileOperation::Seek failure, returns -1 (as a 64-bit -1). On
	 * success, TAIL-CALLS this->Tell() (vtable slot 9, confirmed via the
	 * corrected slot-index formula -- see header comment) and returns
	 * that. Every real caller (Jump/MoveToEnd) discards the return
	 * value. */
	virtual long long Seek(long long offset, int type);

	/* .text+0x083011f0, 38 bytes. */
	virtual unsigned long Tell();

	/* .text+0x083005b0, 45 bytes. Real: this->Seek(0, eSeekEnd). */
	virtual void MoveToEnd();

	/* .text+0x08301220, 45 bytes. Real: this->Seek(offset, eSeekCur)
	 * [truncated to 32 bits, see Seek's own note], sign bit of the
	 * (already 32-bit) CFileOperation::Seek result sign-extended into a
	 * long long that no real caller reads -- modeled as void. */
	virtual void Jump(int offset);

	/* .text+0x08300c50, 544 bytes. Reads min(count,8) bytes via Read()
	 * and packs them into a `long long`: mByteOrder==0 -> byte[0] is the
	 * LSB (little-endian on-disk); mByteOrder!=0 -> byte[count-1] is the
	 * LSB (big-endian on-disk, i.e. the on-disk MSB-first bytes are
	 * reinterpreted as a little-endian machine integer). count<=0 -> 0. */
	virtual long long ReadData(int count);

	/* .text+0x083005e0, 1634 bytes. Exact mirror of ReadData: packs the
	 * low min(count,8) bytes of `value` into a local buffer in the same
	 * mByteOrder-selected byte order, then calls Write(buf,count).
	 * Returns whatever Write() returns. */
	virtual unsigned int WriteData(long long value, int count);

	/* .text+0x08300e80, 108 bytes. Reads min(bufSize,logicalLen) bytes
	 * into dest via Read(), NUL-terminates, then Jump()s past any
	 * remaining unread bytes of `logicalLen` (keeps the file position
	 * consistent with "logicalLen bytes were logically consumed" even
	 * though only up to bufSize-1... note: ground truth does NOT reserve
	 * a byte for the NUL beyond what the caller already accounts for in
	 * bufSize, reproduced exactly). */
	virtual unsigned int ReadText(char *dest, int logicalLen, int bufSize);

	/* .text+0x08300ef0, 728 bytes. Writes strlen(str) bytes of `str`; if
	 * that's < minLen, pads with NUL bytes (via WriteData(0,1) calls,
	 * ground truth's own GCC 8-way-unrolled loop, collapsed to a plain
	 * loop here per this project's established "byte-identical result"
	 * license) until minLen total bytes have been written. If
	 * strlen(str) >= minLen, only the first minLen bytes of str are
	 * written verbatim (tail call to Write(str,minLen) in ground truth).
	 * Returns whatever the first Write() call returned. */
	virtual unsigned int WriteText(const char *str, int minLen);

protected:
	SFilePointer *mFile;      /* +0x04 */
	char mFileName[1024];     /* +0x08 */
	int mByteOrder;           /* +0x408, 0=LE on-disk, nonzero=BE on-disk */
};

#endif /* LONG_BINARY_FILE_H */
