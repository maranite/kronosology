/*
 * file_io_udf.h  -  CFileIoUdf, the UDF/ISO-hybrid writable-optical-media
 * driver (file_io_base.h's own "OUT OF SCOPE" list -- the last untouched
 * concrete CFileIoBase sibling, previously flagged "less tractable"
 * alongside CFileIoCdda -- see header comment on format()).
 *
 * REAL SHAPE: `nm -C` symbols at `.text+0x0831cb80`..`0x0831f4c0` + `vtable
 * for CFileIoUdf` (`.data.rel.ro+0x08f31c40`, 53 slots, same layout as
 * `vtable for CFileIoBase`). 31 real CFileIoBase virtual overrides
 * (get_iotype, totalfreeclus, freebytes, getmediainfo, ftell, funmount,
 * dir, isodir, sortdir, closepath, opennextpath, getwd, chdir,
 * format(2-arg), fflush, fseek, fwrite, fread, fclose, fmount(1-arg),
 * writesetup, chmod, rmdir, mkdir, remove, rename, fopen) plus 4 non-virtual
 * helpers (ConvertPathRtfsToUdffs, formatsub, setfmtparam, set_error) and 1
 * non-virtual public extra method (SetRecoveryParam, called from format()'s
 * own state machine but self-contained and independently callable).
 *
 * Wraps a real embedded UDF filesystem library (`udf_*` prefix,
 * `.text+0x8326xxx`..`0x8355xxx`) -- out of scope, modeled as inert
 * stand-ins, same convention as the rest of this subsystem.
 *
 * MEMBER LAYOUT (`this`, from ctor `.text+0x0831d100` + cross-checked
 * against every method's own field reads/writes):
 *   +0x00        vptr (compiler)
 *   +0x04        m_pStat -- udfdstat* (opaque), ctor hardcodes it to the
 *                real global at .bss+0x93b1260.
 *   +0xfc        unknown int, zeroed by ctor, never read by any method here.
 *   +0x104       m_searchActive -- bool-shaped int, same "a dir() search is
 *                open" flag as the other 4 siblings.
 *   +0x108..0x1f8 m_path[0xf1] (241 bytes) -- SAME fixed-size scratch path
 *                buffer idiom as CFileIoAkai/CFileIoCdda/CFileIoIso9660.
 *   +0x1f9       m_diskFlag -- a byte set by fmount() from a real
 *                CDDriverIO::scsi_read_diskinfo() probe bit (`probe[2] &
 *                0x10`) and read back by fflush() as `udf_flush()`'s own
 *                2nd argument -- a real object field, not a scratch local
 *                (confirmed: fmount() writes `this+0x1f9`, fflush() reads
 *                the SAME offset on the SAME object).
 *
 * The ctor's `udf_memory_init()` call (`.text+0x8355aa0`) DOES check its
 * return value (unlike CFileIoCdda's own ctor) -- failure logs an Api-assert
 * (line 0x66=102) before still completing the same field init as the
 * success path, SAME "log-and-continue" shape as CFileIoIso9660's own
 * `cd_memory_init()` check.
 *
 * set_error() (`.text+0x0831d2f0`, 236 bytes, private) uses its OWN raw
 * error-code global `udf_errno` (.bss+0x93b1794, plain `int`, confirmed via
 * `nm -C -S`) and its OWN 82-entry jump table (`.rodata+0x8eee054`..
 * `0x8eee19c`, range 0..0x51=81) -- see file_io_udf.cpp's own
 * `kUdfErrTable` comment. Same kNoop (in-range, no field write)/kLog
 * (out-of-range, Api-assert) convention as the other 4 siblings.
 *
 * format(EDevice_Id, int) (`.text+0x0831d610`, 0x12b7=4791 bytes -- BY FAR
 * the largest method in this whole storage-driver cluster, larger even than
 * CFileIoDos's own already-deferred format()) is DEFERRED -- see
 * DECOMPILE_ERRORS.md. It is a genuine, resumable, multi-call UDF-format
 * state machine driven by the static `CFileIoUdf::iStage` global (confirmed
 * via `nm -C -S`, all `ds:0x93b1240` read/writes are confined to this one
 * method's own address range -- no other method in this class touches
 * iStage). Its own helper functions `formatsub()` and `setfmtparam()` are
 * SEPARATE, self-contained, NON-iStage-touching entry points and ARE
 * reconstructed below (formatsub() is called from within the deferred
 * format() body, but is itself a plain leaf routine with no state-machine
 * dependency).
 *
 * fopen()'s mode-char translation (`.rodata+0x8eee19c`, 23-entry jump table
 * keyed on `mode[0]-'a'`) has a RICHER scheme than the other 4 siblings:
 * besides the usual single-letter 'a'/'h'/'r'/'v'/'w' slots, 'c' and 'p'
 * route to a real embedded `repz cmpsb`-based FULL mode-string comparison
 * against the literals "cp" (.rodata+0x8eee1fc) and "pcp"
 * (.rodata+0x8eee1f8) -- semantic meaning of these two multi-char mode
 * names not recovered (a CD-R "packet write"-style concept, medium
 * confidence guess only). GENUINE, INDEPENDENTLY VERIFIED FINDING: mode[0]
 * == 'v' returns -1 UNCONDITIONALLY with NO other work at all -- not even
 * `set_error()` is called (confirmed via direct disassembly: that jump
 * target is a bare epilogue, `mov eax,-1; ret`), a real difference from
 * CFileIoCdda::fopen()'s own 'v' slot (which does real work).
 */

#ifndef FILE_IO_UDF_H
#define FILE_IO_UDF_H

#include "file_io_base.h"

/* Opaque: the real UDF library's per-drive search state struct (`udfdstat`,
 * out of scope). Never dereferenced by any method here, only passed through
 * to the (also out-of-scope, stand-in-modeled) `udf_*` library calls.
 */
struct UdfDstat;

class CFileIoUdf : public CFileIoBase {
public:
	/* .text+0x0831d100. See header comment. */
	CFileIoUdf();

	/* .text+0x08995610 (D1) / 0x08995620 (D0). Same shape as the other 4
	 * concrete siblings' own dtor pair.
	 */
	virtual ~CFileIoUdf();

	/* .text+0x0831cb80, 6 bytes. Real: `return 3;` (this class's own
	 * EFileIOType tag).
	 */
	virtual int get_iotype();

	/* .text+0x0831cb90, 22 bytes. Real: tail-calls `udf_freeblks(device)`
	 * directly (block count, NOT byte-shifted -- unlike freebytes() below).
	 */
	virtual unsigned long totalfreeclus(EDevice_Id device);

	/* .text+0x0831cbb0, 27 bytes. Real: `(unsigned long long)udf_freeblks(
	 * device) << 11` (2048-byte UDF/ISO sector size), genuine 64-bit
	 * EDX:EAX return.
	 */
	virtual unsigned long long freebytes(EDevice_Id device);

	/* .text+0x0831cbd0, 0xf5=245 bytes. Real: probes `CDDriverIO::
	 * cdcapstat_tab[device]` bits 4/5 -- both clear -> flag=1 directly;
	 * else probes `udf_mediatype()`/`udf_is_wps()` to decide flag (see .cpp
	 * for the exact nested-branch transcription). Builds name from
	 * `udf_drvno2drv()->+0x1a00` (falls back to "No Label"), computes a
	 * real 64-bit size (`udf_totalblks(device) << 11`), calls
	 * `CMediaInfo::init()` with EFileIOType=3, extra=0x800.
	 */
	virtual int getmediainfo(EDevice_Id device, CMediaInfo *info);

	/* .text+0x0831ccd0, 25 bytes. Real: `udf_fileno2file(handle)->+0x18`
	 * (opaque per-handle record field).
	 */
	virtual long ftell(int handle);

	/* .text+0x0831ccf0, 118 bytes. Real: SAME shape as
	 * CFileIoCdda::funmount()/CFileIoIso9660::funmount()'s own -- closes
	 * any open search via `udf_gdone()`, builds a per-device root path
	 * (`.rodata+0x8eef2c0`, the SAME shared 10-entry table
	 * CFileIoIso9660::funmount() uses) via `CFilePath::operator+=()`, calls
	 * `udf_dskclose()`. Always returns 0.
	 */
	virtual int funmount(EDevice_Id device);

	/* .text+0x0831cd70, 0x38f=911 bytes -- the largest non-format() method
	 * in this class. Real: a genuine UDF directory-iteration loop
	 * (`udf_gfirst()`/`udf_gnext()`/`udf_gdone()`) that skips two real
	 * pseudo-entry dstring literals ("NON-ALLOCATABLE" / "NON-ALLOCATABLE
	 * SPACE", compared via `cmpbuf()`), filters entries via a real
	 * attribute-byte bitmask (record+0x237/+0x236 combined into a 16-bit
	 * flags word), decodes a packed date/time field via the SAME real
	 * inline divide-by-100-via-multiply idiom (`0x147b` magic constant)
	 * already reconstructed for CFileIoAkai/Dos/Iso9660's own `dir()`
	 * methods, then calls `CFileDirEntry::Initialize()`. Transcribed at a
	 * faithful structural level -- see .cpp for the full control flow and
	 * a couple of disclosed medium-confidence simplifications.
	 */
	virtual int dir(const char *path, int arg2, unsigned long &arg3, CFileDirEntry *entry);

	/* .text+0x0831d3e0, 67 bytes. Real: `udf_next_isodir(device, a, b)`; 0
	 * -> set_error(), -1; else 0.
	 */
	virtual int isodir(EDevice_Id device, udf_iso_rec *a, udf_iso_rec *b);

	/* .text+0x0831d430, 51 bytes. Real: `udf_sortdir(device)`; 0 ->
	 * set_error(), -1; else 0.
	 */
	virtual int sortdir(EDevice_Id device);

	/* .text+0x0831d470, 59 bytes. Real: `udf_closepath(device, arg2)`; 0
	 * -> set_error(), -1; else 0.
	 */
	virtual int closepath(EDevice_Id device, int arg2);

	/* .text+0x0831d4b0, 51 bytes. Real: `udf_open_nextpath(device)`; 0 ->
	 * set_error(), -1; else 0.
	 */
	virtual int opennextpath(EDevice_Id device);

	/* .text+0x0831d4f0, 189 bytes. Real: SAME `strcat("\\")` +
	 * `strncpy(0xf0)` shape as CFileIoIso9660::getwd()'s own, but with a
	 * real failure path (`udf_gcwd()` returning 0 -> set_error(), return
	 * 0), unlike Iso9660's own unconditional-success getwd().
	 */
	virtual int getwd(EDevice_Id device, char *buf);

	/* .text+0x0831d5b0, 95 bytes. Real: `strncpy` into `m_path` +
	 * `udf_scwd(m_path)`; 0 -> set_error(), -1; else 0.
	 */
	virtual int chdir(const char *path);

	/* .text+0x0831d610, 0x12b7=4791 bytes -- format(). DEFERRED -- see
	 * header comment and DECOMPILE_ERRORS.md. NOT declared as a virtual
	 * override here (SAME convention as CFileIoDos's own deferred format()
	 * -- see file_io_dos.h) so this reconstruction's vtable falls back to
	 * CFileIoBase's own inherited format(2-arg) stub (assert-log +
	 * `return -1;`) rather than leaving a declared-but-undefined symbol
	 * that would fail to link.
	 */

	/* .text+0x0831e8d0, 63 bytes. Real: `udf_flush(handle, m_diskFlag)`; 0
	 * -> set_error(), -1; else 0.
	 */
	virtual int fflush(int handle);

	/* .text+0x0831e910, 64 bytes. Real: `udf_lseek(handle, offset,
	 * whence)`; negative -> set_error(), -1; else 0.
	 */
	virtual int fseek(int handle, long offset, int whence);

	/* .text+0x0831e950, 96 bytes. Real: `udf_write(handle, buf,
	 * size*count)`; -1 -> SAME "skip set_error() if lastError already 2"
	 * short-circuit as CFileIoCdda::fwrite()/CFileIoDos::fwrite()'s own.
	 * Returns 0 on failure, else `n / size`.
	 */
	virtual unsigned int fwrite(const void *buf, unsigned int size, unsigned int count, int handle);

	/* .text+0x0831e9b0, 76 bytes. Real: `udf_read(handle, buf,
	 * size*count)`; -1 -> set_error(), return 0; else `n / size`.
	 */
	virtual unsigned int fread(void *buf, unsigned int size, unsigned int count, int handle);

	/* .text+0x0831ea00, 51 bytes. Real: `udf_close(handle)`; nonzero ->
	 * set_error(), -1; else 0. Note: compares the 16-bit `ax` result (`test
	 * ax,ax`), unlike CFileIoCdda::fclose()'s own 32-bit `eax` compare --
	 * transcribed as-is.
	 */
	virtual int fclose(int handle);

	/* .text+0x0831ea40, 350 bytes. Real: `theFilesys->lastError=0`;
	 * `CDDriverIO::getdevinfo()` probe (`+1==7`); `read_capacity()`/
	 * `scsi_mode_sel(0x800)` retry (SAME shape as CFileIoCdda::fmount()'s
	 * own); builds a per-device root path, calls `udf_dskopen(path)`; 0
	 * (failure) -> `theFilesys->lastError=5`, -1; else probes
	 * `CDDriverIO::scsi_read_diskinfo()` and stores `probe[2]&0x10` into
	 * `m_diskFlag` (+0x1f9) -- see header comment. Failure of the diskinfo
	 * probe itself -> set_error(), -1.
	 */
	virtual int fmount(EDevice_Id device);

	/* .text+0x0831eba0, 733 bytes. Real: `udf_drvno2drv(device)` gate;
	 * already-set-up (`drv->+4 != 0`) -> return 0 immediately; else probes
	 * `CDDriverIO::scsi_read_trkinfo()`/`scsi_mode_sense10()`, builds a
	 * real MMC "WRITE PARAMETERS" mode-select page (fixed template bytes,
	 * 2 real variants selected by `drv->+2==1` i.e. CD-RW vs. CD-R),
	 * calls `scsi_mode_sel10()`, then `scsi_getwritespeed()`/
	 * `scsi_getmedia_recspeed()`/`scsi_set_speed(0xff,0xff)`; on full
	 * success sets `drv->+4=1` and returns 0; any failed probe ->
	 * `theFilesys->lastError=4`, -1 (direct field write, not via
	 * set_error()'s own table -- SAME "direct assignment" idiom as
	 * CFileIoDos::optimizemedium()'s own).
	 */
	virtual int writesetup(EDevice_Id device, int arg2);

	/* .text+0x0831ee80, 178 bytes. Real: `strncpy` into `m_path`,
	 * `udf_drv2drvno(m_path)`, `writesetup(drvno, 0)` (calls THIS class's
	 * own writesetup() with a computed device); on success, converts `mode`
	 * into a real UDF permission bitmask (`(mode&1) ? 0x35ad : 0x7fff` --
	 * exact real values, individual bit meanings not further decoded) and
	 * calls `udf_chmod(m_path, mask)`; 0 -> set_error(), -1; else 0.
	 */
	virtual int chmod(const char *path, unsigned char mode);

	/* .text+0x0831ef40, 177 bytes. Real: SAME `strncpy`+`writesetup()` gate
	 * as chmod()'s own, then `udf_rmdir(m_path)` with the SAME "if a search
	 * is open, gdone() it and retry once" shape as CFileIoDos::rmdir()'s
	 * own.
	 */
	virtual int rmdir(const char *path);

	/* .text+0x0831f000, 134 bytes. Real: SAME `strncpy`+`writesetup()`
	 * gate, then `udf_mkdir(m_path)`, no retry.
	 */
	virtual int mkdir(const char *path);

	/* .text+0x0831f090, 177 bytes. Real: SAME `strncpy`+`writesetup()`
	 * gate, then `udf_unlink(m_path)` with the SAME gdone()-and-retry shape
	 * as rmdir()'s own.
	 */
	virtual int remove(const char *path);

	/* .text+0x0831f150, 146 bytes. Real: SAME `strncpy`+`writesetup()`
	 * gate, then `udf_mv(m_path, newPath)`, no retry.
	 */
	virtual int rename(const char *oldPath, const char *newPath);

	/* .text+0x0831f1f0, 584 bytes. Real: `strncpy` into `m_path`; mode-char
	 * jump table (23 entries, see header comment); write-requiring modes
	 * gate on `CDeviceMgr::is_writable()` (`theFilesys->lastError=9` on
	 * failure, SAME convention as CFileIoCdda::fopen()'s own); calls
	 * `udf_open(m_path, modeNum, flags)`; negative -> set_error(), return
	 * the raw negative handle; else return the handle.
	 */
	virtual int fopen(const char *path, const char *mode);

	/* .text+0x0831f440, 125 bytes, non-virtual, public (called from the
	 * deferred format()'s own state machine but self-contained). Real:
	 * `CDDriverIO::scsi_mode_sense10(device, buf, 1, 0x14)`; 0 -> return
	 * -1; else computes a length (`buf[1]+2`, capped at 0x14) and calls
	 * `scsi_mode_sel10(device, buf, len)`; returns 0 if that returned
	 * nonzero, else -1.
	 */
	int SetRecoveryParam(EDevice_Id device, int arg2);

	/* .text+0x0831d2f0, 236 bytes, non-virtual, private (used by nearly
	 * every override above). See header comment.
	 */
	void set_error();

private:
	/* .text+0x0831d180, 52 bytes, non-virtual, private. Real: plain
	 * `strncpy(m_path, path, 0xf0)` + manual nul-terminate -- SAME idiom as
	 * the other 4 siblings' own path-conversion helpers, but WITHOUT any
	 * further rewrite (no drive-letter stripping, no space/dot splitting --
	 * UDF paths pass through unchanged beyond the fixed-size copy).
	 */
	void ConvertPathRtfsToUdffs(const char *path);

	/* .text+0x0831d1c0, 236 bytes, non-virtual, private (called from the
	 * deferred format()'s own state machine, but itself self-contained --
	 * no iStage dependency). Real: `CDDriverIO::scsi_read_trkinfo(device,
	 * buf, 1)`; 0 -> return -1; else reads a big-endian 32-bit field out of
	 * `buf` at one of two offsets (selected by `arg2==0`), rounds it
	 * (special-cased below/above 0x50000: values above round to the
	 * nearest 0x40 via signed division, all values then round down to the
	 * nearest 0x20), writes the rounded value back into `out3` (same
	 * big-endian byte order) and into `*outLong`, returns 0.
	 */
	int formatsub(EDevice_Id device, int arg2, unsigned char *out3, long *outLong);

	/* .text+0x0831d2b0, 52 bytes, non-virtual, private (called from the
	 * deferred format()'s own state machine, but itself self-contained).
	 * Real: zeroes a 12-byte parameter block, setting byte[3]=8 (all other
	 * bytes 0).
	 */
	void setfmtparam(unsigned char *params);

	UdfDstat *m_pStat;
	int m_reserved0xfc;
	int m_searchActive;
	char m_path[0xf1];
	unsigned char m_diskFlag;

public:
	/* .bss+0x93b1240, real symbol name via `nm -C -S` -- drives the
	 * deferred format()'s resumable state machine (DECOMPILE_ERRORS.md).
	 * Declared here for completeness even though nothing in this TU
	 * currently reads/writes it.
	 */
	static int iStage;
};

#endif /* FILE_IO_UDF_H */
