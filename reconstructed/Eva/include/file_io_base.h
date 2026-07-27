/*
 * file_io_base.h  -  CFileIoBase, the abstract media/filesystem I/O interface every
 * concrete driver in the DiskUtil/CustomFs subsystem derives from.
 *
 * BACKGROUND: a fresh `nm -C` class-inventory sweep (2026-07-27) over
 * /home/share/Decomp/EVA_Decomp/Eva found a dense storage/disk-driver cluster --
 * CFileIoBase (52), CDDriverIO (85), CScsiDriverBase (50), CFilesys (87),
 * CDiskUtil (59) among others -- all previously 100% untouched. CFileIoBase is the
 * cleanest, most self-contained piece of that cluster: it is the pure interface
 * layer, 49 stub virtual methods + ctor + dtor, every single one still 100% stub
 * in the real binary (no derived class in this batch's scope needed). Concrete
 * drivers (CDDriverIO for optical media, CFilesys for the VFAT layer, a
 * CFileIoUnknown/CFileIoCdda/CFileIoKge/CFileIoUdf family) override the relevant
 * subset per media type -- deliberately out of scope for this pass, see below.
 *
 * SHAPE: every one of the 49 non-ctor/dtor methods in the real binary
 * (`.text+0x08318480`..`0x08318cf0`, `DiskUtil/CustomFs/FileIoBase.cpp` per its own
 * embedded __FILE__-shaped literal at `.rodata+0x8f26020`) does ONE of two things:
 *
 *   "heavy" (30 methods) -- calls the real Api+0x94 assert-report slot (same
 *   idiom already established project-wide, see tempo.cpp's own ApiAssert()
 *   comment) with the real embedded format string "Assertion failed in module
 *   %s, line %i.\n", this file's own name, and the real source line number
 *   (transcribed from each function's own `mov [esp+0xc], <line>` immediate --
 *   confirmed monotonically increasing across the function list, a strong
 *   internal-consistency check that these are genuinely per-call-site literals,
 *   not guessed), then returns a fixed sentinel.
 *
 *   "light" (19 methods, incl. get_iotype()) -- no assert call at all, just an
 *   immediate return of the same kind of sentinel. No behavioral difference
 *   observed between the two groups beyond the logging call; both are
 *   unconditional, argument-independent stubs.
 *
 * Sentinel convention observed: int-returning methods return -1 (failure) except
 * fread()/fwrite()/fdummywrite() and totalfreeclus()/getmaxclusterno(), which
 * return 0 (xor eax,eax) -- consistent with "0 bytes/clusters" rather than "-1"
 * for those. freebytes() is the one 64-bit-returning method (`xor eax,eax; xor
 * edx,edx; ret`, EDX:EAX pair) -- modeled as `unsigned long long` returning 0.
 * get_iotype() returns plain 0 (CFileIoUnknown/CFileIoCdda overrides observed
 * returning 1/2 respectively -- a per-media-type type tag, not reconstructed
 * here since those classes are out of scope).
 *
 * OUT OF SCOPE (deliberately, this batch): CFileIoUnknown/CFileIoCdda/CFileIoKge/
 * CFileIoUdf (concrete per-media overrides), CDDriverIO/CScsiDriverBase (SCSI/ATAPI
 * command layer), CFilesys (the VFAT-backed concrete driver + CFileIoBase*
 * dispatch table, `get_fileioptr()`), CDiskUtil (higher-level UI-facing disk
 * helper). All confirmed real, all substantial, deliberately left for a future
 * pass -- this header only claims the pure interface layer.
 *
 * Ctor (`.text+0x08318e90`, `CFileIoBase::CFileIoBase()`) and dtor
 * (`.text+0x08995480` D1 / `0x089954e0` D0) both do nothing beyond the compiler's
 * own vtable-pointer bookkeeping in ground truth (ctor: `*(void**)this = <own
 * vtable>`; dtor: same reset, D0 additionally free()s `this` -- the standard
 * Itanium dual-destructor shape already established project-wide, e.g.
 * kernel_death_notifier.h) -- an ordinary empty `{ }` definition of each is
 * faithful; no hand-transcription needed for either.
 *
 * Types EDevice_Id/EMountIoType/EFatType/EAudioStatusMMC are real enums (confirmed
 * by their capitalized `E`-prefixed name mangling) whose enumerator values are NOT
 * recovered here -- declared as opaque single-placeholder-value stand-ins, same
 * precedent as ustg_api_klm.h's `eSTGOptionType`. CMediaInfo/CFileDirEntry/
 * CFileLbaArray/udf_iso_rec are real classes/structs used here only as opaque
 * pointer types (never dereferenced by any CFileIoBase method itself) -- forward
 * declared only, same "opaque past what's needed" convention as dir_entry.h's own
 * CZ members.
 */

#ifndef FILE_IO_BASE_H
#define FILE_IO_BASE_H

/* Opaque enum stand-ins -- real enumerator lists not recovered (nm -C carries the
 * type name via mangling, not the enumerator values). Placeholder value only.
 */
enum EDevice_Id { kDeviceId_Placeholder = 0 };
enum EMountIoType { kMountIoType_Placeholder = 0 };
enum EFatType { kFatType_Placeholder = 0 };
enum EAudioStatusMMC { kAudioStatusMMC_Placeholder = 0 };

/* Opaque forward declarations -- never dereferenced by CFileIoBase itself. */
class CMediaInfo;
class CFileDirEntry;
class CFileLbaArray;
struct udf_iso_rec;

class CFileIoBase {
public:
	/* .text+0x08318e90. Real body only sets the vtable pointer (compiler
	 * bookkeeping) -- an empty ctor is faithful.
	 */
	CFileIoBase();

	/* .text+0x08995480 (D1) / 0x089954e0 (D0). Real body only resets the
	 * vtable pointer (D0 additionally free()s `this`, both compiler
	 * bookkeeping) -- an empty virtual dtor is faithful, matches this
	 * project's established dual-destructor convention.
	 */
	virtual ~CFileIoBase();

	/* .text+0x08318480, 3 bytes, light. Real: `return 0;` */
	virtual int get_iotype();

	/* .text+0x08318490, line 70, heavy. Real: assert-log, `return -1;` */
	virtual int fmount(EDevice_Id device);

	/* .text+0x083184d0, line 79, heavy. Real: assert-log, `return -1;` */
	virtual int fmount(EDevice_Id device, EMountIoType ioType, int *arg3);

	/* .text+0x08318510, line 88, heavy. Real: assert-log, `return -1;` */
	virtual int funmount(EDevice_Id device);

	/* .text+0x08318550, line 97, heavy. Real: assert-log, `return -1;` */
	virtual int fopen(const char *path, const char *mode);

	/* .text+0x08318590, line 106, heavy. Real: assert-log, `return -1;` */
	virtual int fclose(int handle);

	/* .text+0x083185d0, line 115, heavy. Real: assert-log, `return 0;`
	 * (0 bytes read, not -1 -- fread()-shaped sentinel).
	 */
	virtual unsigned int fread(void *buf, unsigned int size, unsigned int count, int handle);

	/* .text+0x08318610, line 124, heavy. Real: assert-log, `return 0;`
	 * (0 bytes written, same fwrite()-shaped sentinel as fread()).
	 */
	virtual unsigned int fwrite(const void *buf, unsigned int size, unsigned int count, int handle);

	/* .text+0x08318650, line 133, heavy. Real: assert-log, `return -1;` */
	virtual int fseek(int handle, long offset, int whence);

	/* .text+0x08318690, line 142, heavy. Real: assert-log, `return -1;` */
	virtual long ftell(int handle);

	/* .text+0x083186d0, line 151, heavy. Real: assert-log, `return -1;` */
	virtual int fflush(int handle);

	/* .text+0x08318710, line 160, heavy. Real: assert-log, `return -1;` */
	virtual int resize(int handle, unsigned int newSize);

	/* .text+0x08318750, line 169, heavy. Real: assert-log, `return -1;` */
	virtual int format(EDevice_Id device, int arg2);

	/* .text+0x08318790, 6 bytes, light. Real: `return -1;` (no assert call --
	 * the one 3-arg overload that skips it, faithfully preserved).
	 */
	virtual int format(EDevice_Id device, int arg2, EFatType fatType);

	/* .text+0x083187a0, 5 bytes, light. Real: `xor eax,eax; xor edx,edx;
	 * ret` -- genuine 64-bit EDX:EAX return, `return 0;`.
	 */
	virtual unsigned long long freebytes(EDevice_Id device);

	/* .text+0x083187b0, line 198, heavy. Real: assert-log, `return 0;`
	 * (0 free clusters, not -1).
	 */
	virtual unsigned long totalfreeclus(EDevice_Id device);

	/* .text+0x083187f0, 6 bytes, light. Real: `return -1;` */
	virtual int chdir(const char *path);

	/* .text+0x08318800, 3 bytes, light. Real: `return 0;` */
	virtual int getwd(EDevice_Id device, char *buf);

	/* .text+0x08318810, 3 bytes, light. Real: `return 0;` */
	virtual int dir(const char *path, int arg2, unsigned long &arg3, CFileDirEntry *entry);

	/* .text+0x08318820, line 235, heavy. Real: assert-log, `return -1;` */
	virtual int rename(const char *oldPath, const char *newPath);

	/* .text+0x08318860, line 244, heavy. Real: assert-log, `return -1;` */
	virtual int remove(const char *path);

	/* .text+0x083188a0, line 253, heavy. Real: assert-log, `return -1;` */
	virtual int mkdir(const char *path);

	/* .text+0x083188e0, line 262, heavy. Real: assert-log, `return -1;` */
	virtual int rmdir(const char *path);

	/* .text+0x08318920, 6 bytes, light. Real: `return -1;` */
	virtual int getmediainfo(EDevice_Id device, CMediaInfo *info);

	/* .text+0x08318930, 6 bytes, light. Real: `return -1;` */
	virtual int play(EDevice_Id device, unsigned char a, unsigned char b, unsigned long c, unsigned long d);

	/* .text+0x08318940, 6 bytes, light. Real: `return -1;` */
	virtual int stop(EDevice_Id device);

	/* .text+0x08318950, 6 bytes, light. Real: `return -1;` */
	virtual int pause(EDevice_Id device);

	/* .text+0x08318960, 6 bytes, light. Real: `return -1;` */
	virtual int resume(EDevice_Id device);

	/* .text+0x08318970, 6 bytes, light. Real: `return -1;` */
	virtual int ffscan(EDevice_Id device, unsigned char a, unsigned char b, unsigned long c, unsigned long d);

	/* .text+0x08318980, 6 bytes, light. Real: `return -1;` */
	virtual int rewscan(EDevice_Id device, unsigned char a, unsigned char b, unsigned long c, unsigned long d);

	/* .text+0x08318990, 6 bytes, light. Real: `return -1;` */
	virtual int stopscan(EDevice_Id device);

	/* .text+0x083189a0, 13 bytes, light. Real: `return 0;` */
	virtual int getcurpos(EDevice_Id device, EAudioStatusMMC *status, unsigned char *a, unsigned char *b, int c, int d);

	/* .text+0x083189b0, 6 bytes, light. Real: `return -1;` */
	virtual int getmaxtrk(EDevice_Id device, unsigned char *out);

	/* .text+0x083189c0, 6 bytes, light. Real: `return -1;` */
	virtual int getmaxidx(EDevice_Id device, unsigned char track, unsigned char *out);

	/* .text+0x083189d0, 6 bytes, light. Real: `return -1;` */
	virtual int gettrklen(EDevice_Id device, unsigned char track, unsigned long *out);

	/* .text+0x083189e0, 6 bytes, light. Real: `return -1;` */
	virtual int getidxlen(EDevice_Id device, unsigned char track, unsigned char idx, unsigned char c, unsigned long *out);

	/* .text+0x083189f0, line 386, heavy. Real: assert-log, `return -1;` */
	virtual int finalize(EDevice_Id device);

	/* .text+0x08318a30, line 395, heavy. Real: assert-log, `return -1;` */
	virtual int settestmode(EDevice_Id device, int mode);

	/* .text+0x08318a70, line 401, heavy. Real: assert-log, `return 0;`
	 * (0 bytes written, fwrite()-shaped sentinel).
	 */
	virtual unsigned int fdummywrite(unsigned int size, unsigned int count, int handle);

	/* .text+0x08318ab0, line 407, heavy. Real: assert-log, `return -1;` */
	virtual int getfilelbaarray(EDevice_Id device, int arg2, CFileLbaArray *out);

	/* .text+0x08318af0, line 416, heavy. Real: assert-log, `return -1;` */
	virtual int opennextpath(EDevice_Id device);

	/* .text+0x08318b30, line 424, heavy. Real: assert-log, `return -1;` */
	virtual int closepath(EDevice_Id device, int arg2);

	/* .text+0x08318b70, line 432, heavy. Real: assert-log, `return -1;` */
	virtual int sortdir(EDevice_Id device);

	/* .text+0x08318bb0, line 440, heavy. Real: assert-log, `return -1;` */
	virtual int isodir(EDevice_Id device, udf_iso_rec *a, udf_iso_rec *b);

	/* .text+0x08318bf0, line 449, heavy. Real: assert-log, `return -1;` */
	virtual int getemphasized(int arg1, int *out);

	/* .text+0x08318c30, line 458, heavy. Real: assert-log, `return 0;`
	 * (0 clusters, not -1).
	 */
	virtual unsigned long getmaxclusterno(EDevice_Id device);

	/* .text+0x08318c70, line 466, heavy. Real: assert-log, `return -1;` */
	virtual int scandisk(EDevice_Id device, unsigned long a, unsigned long b, unsigned long *c, unsigned long *d);

	/* .text+0x08318cb0, line 474, heavy. Real: assert-log, `return -1;` */
	virtual int optimizemedium(EDevice_Id device, unsigned long a, unsigned long *b, int c);

	/* .text+0x08318cf0, line 482, heavy. Real: assert-log, `return -1;` */
	virtual int chmod(const char *path, unsigned char mode);
};

#endif /* FILE_IO_BASE_H */
