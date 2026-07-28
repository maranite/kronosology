/*
 * file_io_akai.h  -  CFileIoAkai, the Akai S-series sampler disk-image driver
 * (file_io_base.h's own "OUT OF SCOPE" list -- picked up 2026-07-28 alongside
 * CFileIoDos/CFileIoIso9660 as the next storage-cluster batch).
 *
 * REAL SHAPE: 17 non-ctor/dtor `nm -C` symbols (`.text+0x08317c70`..`0x083183c0`,
 * 19 total .text entry points incl. ctor + the D1/D0 dtor pair) + `vtable for
 * CFileIoAkai` (`.data.rel.ro+0x08f31640`, 53 slots, same layout as `vtable for
 * CFileIoBase` -- confirmed by direct byte read, `objdump -s`). 14 real
 * CFileIoBase virtual overrides (get_iotype, freebytes, getmediainfo, format
 * (2-arg only -- the 3-arg overload is NOT overridden, inherited straight from
 * CFileIoBase), ftell, dir, getwd, chdir, fseek, fread, fclose, fopen, funmount,
 * fmount) plus 2 non-virtual private helpers (ConvertPath, set_error) shared by
 * several of the overrides' failure paths. Every other inherited CFileIoBase
 * virtual resolves straight through to CFileIoBase's own stub body (confirmed:
 * vtable slot addresses for e.g. fmount(3-arg)/fwrite/fflush/resize/format
 * (3-arg)/totalfreeclus/rename/remove/mkdir/rmdir/... all equal file_io_base.h's
 * own documented addresses, unpatched).
 *
 * Wraps a real embedded Akai-format filesystem library (`aki_*`/`akiutil_*`/
 * `ak_memory_init`/`alowl_dskfree`, `.text+0x8362xxx`..`0x8365xxx`) -- out of scope
 * itself (not reconstructed here, modeled as inert stand-ins, same convention as
 * file_io_unknown.h's CDDriverIO/CFilesys stand-ins).
 *
 * MEMBER LAYOUT (`this`, from ctor `.text+0x08317fa0` + cross-checked against
 * every method's own field reads/writes):
 *   +0x00        vptr (compiler)
 *   +0x04        m_pStat  -- AkiAstat* (opaque), ctor hardcodes it to the real
 *                global `cfioakai_dir_obj` (.bss+0x93b0ee0, 0xc4=196 bytes, real
 *                symbol name via `nm -C -S`) -- a SINGLE shared global search/IO
 *                state object, not a per-instance allocation (matches this
 *                driver's real single-drive-letter design).
 *   +0xfc        unknown int, zeroed by ctor, never read by any of the 17
 *                methods here -- reserved, meaning not recovered.
 *   +0x104       m_searchActive -- bool-shaped int, "a dir() search is currently
 *                open" flag (aki_gfirst()'d but not yet aki_gdone()'d). Zeroed by
 *                ctor.
 *   +0x108..0x1f8 m_path[0xf1] (241 bytes) -- fixed-size scratch path buffer,
 *                filled via `strncpy(m_path, path, 0xf0)` then a manual
 *                `m_path[0xf0] = 0` null-terminate (dir()/chdir()/fopen()/
 *                ConvertPath() all share this exact idiom) -- NOT
 *                ctor-initialized (garbage until first use, transcribed as-is).
 *
 * set_error() (`.text+0x08318030`, 236 bytes, private, called from nearly every
 * other override's failure path) translates the underlying Akai library's own raw
 * last-error code (`fs_user`, .bss+0x9608d90, an `int*` -- SHARED IDENTICALLY with
 * CFileIoDos::set_error(), confirmed by direct address comparison, real symbol
 * name via `nm -C -S`) into the shared `CFilesys::theFilesys->+0x48` field
 * (file_io_driver_common.h) via a 44-entry jump table (`.rodata+0x8eedda0..
 * 0x8eede4c`, fully decoded via `objdump -s`) -- 9 raw codes have a direct,
 * silent mapping (no log), most others log via the project-wide Api+0x94 assert
 * idiom (`"DiskUtil/CustomFs/FileIoAkai.cpp"`, line 689 = 0x2b1, confirmed via
 * `objdump -s -j .rodata` byte-reads of both embedded strings) before falling
 * through, and raw code 0 (and 3 other unmapped-but-in-range codes) is a silent
 * no-op. See file_io_akai.cpp's own `kAkaiErrTable` for the exact per-code
 * mapping (Dos's own table, file_io_dos.h, differs at one entry -- a genuine,
 * independently-confirmed per-driver difference, not a transcription slip).
 *
 * fopen()'s mode-char translation (`.rodata+0x8eede60`, 33-byte table, char code
 * `mode[0]-'P'`, range 0..0x20, default 0x73='s' out of range) is transcribed
 * exactly in file_io_akai.cpp; real per-char semantics beyond 'd'->'d' (identity)
 * and 'p'->'p' (identity) and 'o'->'c' are not recovered (most letters,
 * including the common 'r'/'w'/'a' mode-string first chars, all fold to the same
 * default 's' -- this Akai `aki_open()` mode byte apparently does not distinguish
 * read/write via this argument the way a POSIX fopen() mode would).
 *
 * Ctor/dtor pair: SAME shape as CFileIoBase/CFileIoUnknown's own -- ctor calls
 * CFileIoBase::CFileIoBase() then does its own field init + calls
 * `pc_memory_init()`/`ak_memory_init()` (both out-of-scope library init hooks,
 * modeled as inert stand-ins); dtor (D1 `.text+0x08995430` / D0 `.text+0x08995440`)
 * resets the vtable pointer, D0 additionally wraps `free(this)` in
 * HAL_DisableInterrupts()/HAL_EnableInterrupts() (a small but real difference
 * from CFileIoBase/CFileIoUnknown's own plain `free()` dtor -- transcribed as an
 * ordinary compiler-generated virtual dtor here since the HAL interrupt-disable
 * wrapping around `free()` has no observable effect on this reconstruction's own
 * host-side semantics).
 */

#ifndef FILE_IO_AKAI_H
#define FILE_IO_AKAI_H

#include "file_io_base.h"

/* Opaque: the real Akai-format library's per-drive search/IO state struct
 * (`astat`, out of scope). Never dereferenced by any method here, only passed
 * through to the (also out-of-scope, stand-in-modeled) `aki_*` library calls.
 */
struct AkiAstat;

class CFileIoAkai : public CFileIoBase {
public:
	/* .text+0x08317fa0. Calls CFileIoBase::CFileIoBase(), initializes
	 * m_pStat to the real shared global `cfioakai_dir_obj`, zeroes +0xfc and
	 * m_searchActive, calls pc_memory_init()/ak_memory_init() (out of
	 * scope). See header comment.
	 */
	CFileIoAkai();

	/* .text+0x08995430 (D1) / 0x08995440 (D0). See header comment. */
	virtual ~CFileIoAkai();

	/* .text+0x08317c70, 6 bytes. Real: `return 5;` (this class's own
	 * EFileIOType tag).
	 */
	virtual int get_iotype();

	/* .text+0x08317c80, 5 bytes. Real: `xor eax,eax; xor edx,edx; ret` --
	 * genuine 64-bit EDX:EAX return, `return 0;` (same shape as
	 * CFileIoBase's own default).
	 */
	virtual unsigned long long freebytes(EDevice_Id device);

	/* .text+0x08317c90, 118 bytes. Real: queries
	 * CDDriverIO::read_capacity()+akiutil_getvolumename(), forwards into
	 * CMediaInfo::init() with EFileIOType=5, returns 0 unconditionally.
	 */
	virtual int getmediainfo(EDevice_Id device, CMediaInfo *info);

	/* .text+0x08317d10, 62 bytes. Real: tail-calls
	 * CFilesys::get_fileioptr(6)->format(device, arg2, EFatType(0)) through
	 * the 3-arg format() vtable slot (matches CFileIoUnknown::format()'s
	 * own tail-call-through-vtable shape, fixed selector 6 like
	 * CFileIoUnknown's own 3-arg overload).
	 */
	virtual int format(EDevice_Id device, int arg2);

	/* .text+0x08317d50, 21 bytes. Real: tail-call to `akiext_ftell(int)`
	 * (out of scope).
	 */
	virtual long ftell(int handle);

	/* .text+0x08317d70, 547 bytes -- the largest non-format method in this
	 * batch. Real: iterates the shared search state via
	 * aki_gfirst()/aki_gnext()/aki_gdone(), reinterprets the found Akai
	 * directory record as a `_Dos_Direntry` and copies it via
	 * `CDirentry::operator=()`, decodes 3 packed FAT-style date/time WORDs
	 * via `CDateT::get()` (6 calls, kinds 0/1/2/4/5/6) plus an inline
	 * day/month/year split (transcribed byte-for-byte, real division-by-
	 * constant-via-multiply idiom), then calls
	 * `CFileDirEntry::Initialize()`. See file_io_akai.cpp for the full
	 * transcription.
	 */
	virtual int dir(const char *path, int arg2, unsigned long &arg3, CFileDirEntry *entry);

	/* .text+0x08317ff0, 52 bytes, non-virtual helper (used by dir()/
	 * chdir()/fopen() and callable directly). Real: strncpy(m_path, path,
	 * 0xf0) then manual null-terminate at m_path[0xf0].
	 */
	void ConvertPath(const char *path);

	/* .text+0x08318030, 236 bytes, non-virtual helper. Real: translates
	 * `*fs_user` into `CFilesys::theFilesys->lastError` via a 44-entry
	 * table, or logs an Api assert for unmapped codes. See header comment.
	 */
	void set_error();

	/* .text+0x08318120, 64 bytes. Real: `akiutil_pwd(DEVICE_ID_STR[device],
	 * buf)` (real return-value convention: 0 == failure here, NOT the
	 * usual 0-is-success idiom); on failure calls set_error() and returns
	 * 0, else returns `buf`.
	 */
	virtual int getwd(EDevice_Id device, char *buf);

	/* .text+0x08318160, 95 bytes. Real: strncpy path into m_path,
	 * null-terminate, `akiutil_set_cwd(m_path)`; on failure calls
	 * set_error() and returns -1.
	 */
	virtual int chdir(const char *path);

	/* .text+0x083181c0, 63 bytes. Real: `aki_lseek(handle, offset, whence)`;
	 * negative result -> set_error(), return -1; else return 0.
	 */
	virtual int fseek(int handle, long offset, int whence);

	/* .text+0x08318200, 76 bytes. Real: `aki_read(handle, buf,
	 * size*count)`; -1 -> set_error(), return 0; else return
	 * (bytes_read / size) (element count, fread()-shaped).
	 */
	virtual unsigned int fread(void *buf, unsigned int size, unsigned int count, int handle);

	/* .text+0x08318250, 51 bytes. Real: `aki_close(handle)`; nonzero ->
	 * set_error(), return -1; else return 0.
	 */
	virtual int fclose(int handle);

	/* .text+0x08318290, 159 bytes. Real: strncpy path into m_path,
	 * null-terminate; if a search is open, aki_gdone() it first; translates
	 * `mode[0]` through the 33-byte table (see header comment) into an
	 * `aki_open()` mode byte; negative result -> set_error(), return the
	 * raw negative handle; else return the handle.
	 */
	virtual int fopen(const char *path, const char *mode);

	/* .text+0x08318330, 135 bytes. Real: if a search is open AND the
	 * shared search-state's own drive index (read through 2 levels of
	 * indirection off m_pStat, opaque -- not modeled, always treated as
	 * "no match" here) equals `device`, clears m_searchActive and
	 * aki_gdone()s first; either way then unconditionally calls
	 * `alowl_dskfree(device, 1)` -- failure calls set_error(), returns -1;
	 * else 0.
	 */
	virtual int funmount(EDevice_Id device);

	/* .text+0x083183c0, 181 bytes. Real: `CDDriverIO::getdevinfo()` +
	 * `CDDriverIO::read_capacity()`-driven sector-size probe (forces
	 * 0x800-byte sectors unless the device reports type 7, in which case
	 * `CDDriverIO::scsi_mode_sel()` is used instead), then
	 * `akiutil_dskinit(device, DENSITY_AKAI(2), sectorSize, sectorCount)`;
	 * on failure calls set_error(). Returns 0 on success, -1 on any
	 * failure path (real: several distinct failure sub-paths all converge
	 * on the same -1 sentinel).
	 */
	virtual int fmount(EDevice_Id device);

private:
	AkiAstat *m_pStat;
	int m_reserved0xfc;
	int m_searchActive;
	char m_path[0xf1];
};

#endif /* FILE_IO_AKAI_H */
