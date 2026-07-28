/*
 * file_io_cdda.h  -  CFileIoCdda, the CD-DA (audio-CD) media driver
 * (file_io_base.h's own "OUT OF SCOPE" list -- one of the last 2 concrete
 * CFileIoBase siblings, previously flagged "less tractable" in the
 * 2026-07-28 CScsiDriverBase batch, picked up here after a fresh re-survey
 * found the class is 40/41 tractable -- see header comment on getcurpos()).
 *
 * REAL SHAPE: `nm -C` symbols at `.text+0x08318ea0`..`0x0831a520` + `vtable
 * for CFileIoCdda` (`.data.rel.ro+0x08f31940`, 53 slots, same layout as
 * `vtable for CFileIoBase`). 39 real CFileIoBase virtual overrides
 * (get_iotype, fflush, resize, chdir, dir, rename, remove, mkdir, rmdir,
 * getcurpos, totalfreeclus, freebytes, getmediainfo, getwd, format(2-arg
 * ONLY), ftell, funmount, getemphasized, getidxlen, gettrklen, getmaxidx,
 * getmaxtrk, writesetup, stopscan, rewscan, ffscan, resume, pause, stop,
 * play, fseek, fwrite, fread, fclose, fopen, fmount(3-arg), settestmode,
 * finalize) plus 2 non-virtual private helpers (ConvertPathRtfsToCdda,
 * set_error).
 *
 * Wraps a real embedded CD-DA (audio track) driver library (`cdda_*`
 * prefix, `.text+0x8365xxx`..`0x836axxx`) -- out of scope, modeled as inert
 * stand-ins, same convention as the rest of this subsystem.
 *
 * MEMBER LAYOUT (`this`, from ctor `.text+0x08319960` + cross-checked
 * against every method's own field reads/writes) -- SAME shape as
 * CFileIoAkai/CFileIoIso9660's own:
 *   +0x00        vptr (compiler)
 *   +0x04        m_pStat -- cddadstat* (opaque), ctor hardcodes it to the
 *                real global at .bss+0x93b0fc4 (a single shared search/IO
 *                state object).
 *   +0xfc        unknown int, zeroed by ctor, never read by any method here.
 *   +0x104       m_searchActive -- bool-shaped int, "a dir() search is
 *                currently open" flag. Zeroed by ctor.
 *   +0x108..0x1f8 m_path[0xf1] (241 bytes) -- fixed-size scratch path
 *                buffer, filled by ConvertPathRtfsToCdda() (same
 *                strncpy(0xf0)+manual-nul-terminate idiom as CFileIoAkai/
 *                CFileIoIso9660's own m_path).
 *
 * UNLIKE CFileIoAkai/CFileIoIso9660, the ctor's `cdda_memory_init()` call
 * (`.text+0x836aef0`) has its RETURN VALUE DISCARDED -- no `test eax,eax`
 * check at all (confirmed via direct disassembly), unlike
 * CFileIoIso9660::CFileIoIso9660()'s own `cd_memory_init()` check. A real,
 * independently confirmed difference, not a transcription gap.
 *
 * set_error() (`.text+0x08319ab0`, 169 bytes, private) uses its OWN raw
 * error-code global `cdda_errno` (.bss+0x9600554, plain `int`, confirmed via
 * `nm -C -S` -- NOT the shared `fs_user`/`cd_errno` globals the other 4
 * sibling classes use) and its OWN 23-entry jump table
 * (`.rodata+0x8eede84`..`0x8eedee0`, decoded via `objdump -s`, range 0..22)
 * -- see file_io_cdda.cpp's own `kCddaErrTable` comment. Unmapped in-range
 * codes are a genuine no-op (index 0 maps straight to the epilogue, no field
 * write); out-of-range (>22) logs the same Api-assert idiom as
 * CFileIoAkai/CFileIoIso9660's own.
 *
 * getcurpos() (`.text+0x08318f30`, 0x74a=1866 bytes -- the largest method in
 * this class, and the reason CFileIoCdda was passed over for the earlier
 * CScsiDriverBase batch) is DEFERRED -- see DECOMPILE_ERRORS.md for the full
 * rationale. Everything else in this class was surveyed fresh and found
 * genuinely tractable (mostly small forwarding methods matching the shape
 * already established for CFileIoAkai/Dos/Iso9660, plus a handful of
 * medium-sized real routines: getmediainfo, fopen, fmount, finalize, all
 * transcribed below).
 *
 * fopen()'s mode-char translation (`.rodata+0x8eedee0`, 23-entry jump table
 * keyed on `mode[0]-'a'`, range 0..0x16) is a real embedded switch -- SAME
 * shape as CFileIoDos::fopen()'s own 23-entry table, with 5 real
 * letter-specific slots ('a', 'h', 'r', 'v', 'w') plus a shared default.
 * See file_io_cdda.cpp's own comment for the exact per-letter flag/mode
 * construction.
 *
 * Ctor/dtor pair: SAME shape as the other 4 concrete siblings' own.
 */

#ifndef FILE_IO_CDDA_H
#define FILE_IO_CDDA_H

#include "file_io_base.h"

/* Opaque: the real CD-DA library's per-drive search state struct
 * (`cddadstat`, out of scope). Never dereferenced by any method here, only
 * passed through to the (also out-of-scope, stand-in-modeled) `cdda_*`
 * library calls.
 */
struct CddaDstat;

class CFileIoCdda : public CFileIoBase {
public:
	/* .text+0x08319960. See header comment. */
	CFileIoCdda();

	/* .text+0x08995520 (D1) / 0x08995530 (D0). Same shape as the other 4
	 * concrete siblings' own dtor pair.
	 */
	virtual ~CFileIoCdda();

	/* .text+0x08318ea0, 6 bytes. Real: `return 2;` (this class's own
	 * EFileIOType tag -- confirmed by cross-reference from
	 * file_io_base.h's own comment).
	 */
	virtual int get_iotype();

	/* .text+0x08318eb0, 6 bytes. Real: `return -1;` (no assert -- inherits
	 * CFileIoBase's own sentinel but as a direct override, not
	 * inheritance).
	 */
	virtual int fflush(int handle);

	/* .text+0x08318ec0, 6 bytes. Real: `return -1;` */
	virtual int resize(int handle, unsigned int newSize);

	/* .text+0x08318ed0, 3 bytes. Real: `return 0;` -- genuinely DIFFERENT
	 * sentinel from CFileIoBase's own chdir() (-1+assert): a CD-DA disc has
	 * no directory hierarchy, so chdir() trivially "succeeds" doing
	 * nothing.
	 */
	virtual int chdir(const char *path);

	/* .text+0x08318ee0, 3 bytes. Real: `return 0;` -- always reports "no
	 * entry" immediately; this driver never returns real directory
	 * entries.
	 */
	virtual int dir(const char *path, int arg2, unsigned long &arg3, CFileDirEntry *entry);

	/* .text+0x08318ef0, 6 bytes. Real: `return -1;` */
	virtual int rename(const char *oldPath, const char *newPath);

	/* .text+0x08318f00, 6 bytes. Real: `return -1;` */
	virtual int remove(const char *path);

	/* .text+0x08318f10, 6 bytes. Real: `return -1;` */
	virtual int mkdir(const char *path);

	/* .text+0x08318f20, 6 bytes. Real: `return -1;` */
	virtual int rmdir(const char *path);

	/* .text+0x08318f30, 0x74a=1866 bytes -- getcurpos(). DEFERRED -- see
	 * DECOMPILE_ERRORS.md. Real: a genuine CD-DA track/index binary-search
	 * (walks a track-descriptor array via `<>` bound comparisons against a
	 * target LBA, then a real `cdda_getidxstart()`-driven index refinement
	 * loop) plus `USTGAPICDAudio::GetCurrentPosition()` / `CDDriverIO::
	 * scsi_get_event()` calls -- fully disassembled, judged too deep for
	 * this pass. NOT declared as a virtual override here (SAME convention
	 * as CFileIoDos's own deferred format() -- see file_io_dos.h) so this
	 * reconstruction's vtable falls back to CFileIoBase's own inherited
	 * `getcurpos()` stub (assert-log + `return 0;`) rather than leaving a
	 * declared-but-undefined symbol that would fail to link.
	 */

	/* .text+0x08319680, 0x22=34 bytes. Real: `cdda_freebytes(device) /
	 * 2352` (real inline divide-by-2352-via-multiply idiom, 2352 = raw
	 * CD-DA sector size in bytes -- confirmed by direct arithmetic
	 * cross-check).
	 */
	virtual unsigned long totalfreeclus(EDevice_Id device);

	/* .text+0x083196b0, 0x18=24 bytes. Real: `cdda_freebytes(device)`,
	 * genuine 64-bit EDX:EAX return (EDX forced 0).
	 */
	virtual unsigned long long freebytes(EDevice_Id device);

	/* .text+0x083196d0, 0x117=279 bytes. Real: probes `cdda_drvno2drv()`'s
	 * own type byte (+4); type in {1,4,6} -> flag=1 directly; else probes
	 * `CDDriverIO::cdcapstat_tab[device]` (.bss+0x93b0d5e, same shared
	 * per-device flags byte CFileIoUnknown/CFileIoCdda's own format()
	 * selector reads) -- bit4 clear && bit5 clear -> flag=1; else a real
	 * `CDDriverIO::scsi_read_diskinfo()` probe decides flag from the
	 * probe's own +2 byte. Builds name from `cdda_drvno2drv()->+0x664`
	 * (falls back to "No Label") and calls `CMediaInfo::init()` with
	 * EFileIOType=2, extra=2352 (raw CD-DA sector size). See .cpp for the
	 * exact bit-level control flow (transcribed structurally; the
	 * individual capstat bit's semantic name is not recovered beyond
	 * "hidden-track"/"probe-needed"-shaped).
	 */
	virtual int getmediainfo(EDevice_Id device, CMediaInfo *info);

	/* .text+0x083197f0, 0x7a=122 bytes. Real: `strcat(buf, "\\")` +
	 * `strncpy(..., 0xf0)` -- SAME idiom as CFileIoIso9660::getwd()'s own,
	 * unconditional (no failure path).
	 */
	virtual int getwd(EDevice_Id device, char *buf);

	/* .text+0x08319870, 0x49=73 bytes. Real: tail-calls
	 * `CFilesys::get_fileioptr(3)->format(device, arg2)` through the 2-arg
	 * format() vtable slot -- fixed selector 3, SAME tail-call-through-
	 * vtable shape as CFileIoUnknown/CFileIoAkai/CFileIoIso9660's own.
	 */
	virtual int format(EDevice_Id device, int arg2);

	/* .text+0x083198c0, 0x19=25 bytes. Real:
	 * `cdda_fileno2file(handle)->+0x10` (opaque per-handle record field).
	 */
	virtual long ftell(int handle);

	/* .text+0x083198e0, 0x76=118 bytes. Real: closes any open search via
	 * `cdda_gdone()`, builds a per-device CDDA root path
	 * (`.rodata+0x8eef280`, 10 `const char*` entries) via
	 * `CFilePath::operator+=()`, calls `cdda_dskclose()`. Always returns 0.
	 */
	virtual int funmount(EDevice_Id device);

	/* .text+0x08319b60, 0x3b=59 bytes. Real: `cdda_isemphasized(device,
	 * out)`; 0 (failure) -> set_error(), -1; else 0.
	 */
	virtual int getemphasized(int device, int *out);

	/* .text+0x08319ba0, 0x53=83 bytes. Real: `cdda_getidxlen(device,
	 * track, idxA, idxB, out)`; 0 -> set_error(), -1; else 0.
	 */
	virtual int getidxlen(EDevice_Id device, unsigned char track, unsigned char idxA, unsigned char idxB, unsigned long *out);

	/* .text+0x08319c00, 0x4b=75 bytes. Real: `cdda_gettrklen(device,
	 * track, out)`; 0 -> set_error(), -1; else 0.
	 */
	virtual int gettrklen(EDevice_Id device, unsigned char track, unsigned long *out);

	/* .text+0x08319c50, 0x43=67 bytes. Real: `cdda_getmaxidxno(device,
	 * track, out)`; 0 -> set_error(), -1; else 0.
	 */
	virtual int getmaxidx(EDevice_Id device, unsigned char track, unsigned char *out);

	/* .text+0x08319ca0, 0x3b=59 bytes. Real: `cdda_getmaxtrkno(device,
	 * out)`; 0 -> set_error(), -1; else 0.
	 */
	virtual int getmaxtrk(EDevice_Id device, unsigned char *out);

	/* .text+0x08319ce0, 0x3b=59 bytes. Real: `cdda_writesetup(device,
	 * arg2)`; 0 -> set_error(), -1; else 0.
	 */
	virtual int writesetup(EDevice_Id device, int arg2);

	/* .text+0x08319d20, 0x33=51 bytes. Real: `cdda_stopscan(device)`; 0 ->
	 * set_error(), -1; else 0.
	 */
	virtual int stopscan(EDevice_Id device);

	/* .text+0x08319d60, 0x53=83 bytes. Real: `cdda_rewscan(device, a, b,
	 * c, d)`; 0 -> set_error(), -1; else 0.
	 */
	virtual int rewscan(EDevice_Id device, unsigned char a, unsigned char b, unsigned long c, unsigned long d);

	/* .text+0x08319dc0, 0x53=83 bytes. Real: `cdda_ffscan(device, a, b, c,
	 * d)`; 0 -> set_error(), -1; else 0.
	 */
	virtual int ffscan(EDevice_Id device, unsigned char a, unsigned char b, unsigned long c, unsigned long d);

	/* .text+0x08319e20, 0x33=51 bytes. Real: `cdda_resume(device)`; 0 ->
	 * set_error(), -1; else 0.
	 */
	virtual int resume(EDevice_Id device);

	/* .text+0x08319e60, 0x33=51 bytes. Real: `cdda_pause(device)`; 0 ->
	 * set_error(), -1; else 0.
	 */
	virtual int pause(EDevice_Id device);

	/* .text+0x08319ea0, 0x33=51 bytes. Real: `cdda_stop(device)`; 0 ->
	 * set_error(), -1; else 0.
	 */
	virtual int stop(EDevice_Id device);

	/* .text+0x08319ee0, 0x83=131 bytes. Real: writes 4 real "last known
	 * play position" cache globals (.bss+0x93b0fb0/0x91b94a0/0x91b94b0/
	 * 0x91b94c0/0x93b0fc0 -- shared with getcurpos()'s own, deferred, use of
	 * 0x93b0fb0/0x91b94a0; semantic names beyond "position cache" not
	 * recovered) before calling `cdda_play(device, a, b, c, d)`; 0 ->
	 * set_error(), -1; else 0.
	 */
	virtual int play(EDevice_Id device, unsigned char a, unsigned char b, unsigned long c, unsigned long d);

	/* .text+0x08319f70, 0x40=64 bytes. Real: `cdda_lseek(handle, offset,
	 * whence)`; negative -> set_error(), -1; else 0.
	 */
	virtual int fseek(int handle, long offset, int whence);

	/* .text+0x08319fb0, 0x60=96 bytes. Real: `cdda_write(handle, buf,
	 * size*count)`; -1 -> if `theFilesys->lastError` is ALREADY 2, skip
	 * set_error() (SAME method-specific short-circuit as
	 * CFileIoDos::fwrite()'s own); else call set_error(). Returns 0 on
	 * failure, else element count (`n / size`).
	 */
	virtual unsigned int fwrite(const void *buf, unsigned int size, unsigned int count, int handle);

	/* .text+0x0831a010, 0x4c=76 bytes. Real: `cdda_read(handle, buf,
	 * size*count)`; -1 -> set_error(), return 0; else `n / size`.
	 */
	virtual unsigned int fread(void *buf, unsigned int size, unsigned int count, int handle);

	/* .text+0x0831a060, 0x33=51 bytes. Real: `cdda_close(handle)`; NONZERO
	 * (note: opposite convention from most `cdda_*` calls, matches POSIX
	 * close()) -> set_error(), -1; else 0.
	 */
	virtual int fclose(int handle);

	/* .text+0x0831a0a0, 0x1bf=447 bytes. Real: `ConvertPathRtfsToCdda()`,
	 * then a mode-char jump table (23 entries, see header comment)
	 * builds `cdda_open()`'s flags/mode arguments (with an
	 * `CDeviceMgr::is_writable()` gate for write-requiring modes,
	 * theFilesys->lastError=9 on failure -- SAME field=9 "not writable"
	 * convention as CFileIoDos's own); negative -> set_error(), return the
	 * raw negative handle; else return the handle.
	 */
	virtual int fopen(const char *path, const char *mode);

	/* .text+0x0831a260, 0x107=263 bytes. Real: `CDDriverIO::getdevinfo()`
	 * probe (`+1==7`), `read_capacity()`/`scsi_mode_sel(0x800)` retry (SAME
	 * shape as CFileIoIso9660::fmount()'s own probe, but -- unlike
	 * Iso9660's genuine "always -1" bug -- this one's success path is
	 * real), builds a per-device root path, calls `cdda_dskopen(path,
	 * ioType, arg3)`; 0 (failure) -> set_error(), -1; else 0.
	 */
	virtual int fmount(EDevice_Id device, EMountIoType ioType, int *arg3);

	/* .text+0x0831a370, 0x3b=59 bytes. Real: `cdda_writesetup(device, 0)`
	 * -- tail-calls the SAME `cdda_writesetup()` writesetup() itself
	 * wraps, with `arg2` hardcoded to 0 -- SAME set_error()-on-failure
	 * shape.
	 */
	virtual int settestmode(EDevice_Id device, int mode);

	/* .text+0x0831a3b0, 0x16e=366 bytes. Real: `cdda_writesetup(device,
	 * 0)`; on success, returns 0 immediately. On failure, probes
	 * `CDDriverIO::scsi_read_diskinfo()`; if the probe's own low nibble ==1,
	 * skips straight to returning -1 (no further calls); else does a real
	 * `scsi_mode_sense10()` / `scsi_mode_sel10()` / `scsi_close_trk()`
	 * sequence (finalizing a CD-R/RW session), mapping specific failures to
	 * `theFilesys->lastError` = 3 or 4 directly (bypassing set_error()'s own
	 * table).
	 */
	virtual int finalize(EDevice_Id device);

	/* .text+0x08319ab0, 169 bytes, non-virtual helper (used by nearly every
	 * override above). See header comment.
	 */
	void set_error();

private:
	/* .text+0x083199b0, 256 bytes, non-virtual helper. Real: SAME shape as
	 * CFileIoIso9660::ConvertPathRtfsToCdfs()'s own (RTFS-to-CDFS-style
	 * path rewrite: strips a drive-letter prefix, splits the final
	 * component at the first space then the first '.', uppercases/
	 * truncates, appends "." + extension when found -- SAME medium-
	 * confidence tail-`strcat()` disclosure as CFileIoIso9660's own).
	 */
	void ConvertPathRtfsToCdda(const char *path);

	CddaDstat *m_pStat;
	int m_reserved0xfc;
	int m_searchActive;
	char m_path[0xf1];
};

#endif /* FILE_IO_CDDA_H */
