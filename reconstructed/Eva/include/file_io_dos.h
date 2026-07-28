/*
 * file_io_dos.h  -  CFileIoDos, the DOS/FAT disk-image driver (file_io_base.h's
 * own "OUT OF SCOPE" list -- picked up 2026-07-28 alongside CFileIoAkai/
 * CFileIoIso9660 as the next storage-cluster batch, the biggest and most
 * substantive of the three).
 *
 * REAL SHAPE: `nm -C` symbols at `.text+0x0831a520`..`0x0831c020` + `vtable for
 * CFileIoDos` (`.data.rel.ro+0x08f31a40`, 53 slots, same layout as `vtable for
 * CFileIoBase`, confirmed by `objdump -s`). 25 real CFileIoBase virtual
 * overrides (get_iotype, getmaxclusterno, getfilelbaarray, dir, totalfreeclus,
 * freebytes, ftell, getmediainfo, optimizemedium, scandisk, fdummywrite, rmdir,
 * mkdir, remove, rename, getwd, chdir, format(3-arg ONLY -- the 2-arg overload
 * is NOT overridden, inherited straight from CFileIoBase, the opposite of
 * CFileIoAkai/CFileIoUnknown), resize, fflush, fseek, fwrite, fread, fclose,
 * fopen, funmount, fmount) plus 1 non-virtual private helper (set_error).
 *
 * Wraps a real embedded DOS/FAT filesystem library (`pc_*`/`po_*` prefixes,
 * `.text+0x836dxxx`..`0x8383xxx`) -- the function-name shape (`pc_gfirst`,
 * `pc_mkfs`, `pc_dskinit`, ...) matches the well-known HCC/EBS "RTFS" embedded
 * FAT filesystem API, out of scope here (not reconstructed, modeled as inert
 * stand-ins, same convention as file_io_unknown.h's CDDriverIO/CFilesys
 * stand-ins).
 *
 * MEMBER LAYOUT (`this`, from ctor `.text+0x0831aa50` + cross-checked against
 * every method's own field reads/writes):
 *   +0x00        vptr (compiler)
 *   +0x04        m_pStat  -- DosDstat* (opaque), ctor hardcodes it to the real
 *                global `cfiodos_dir_obj` (.bss+0x93b1020, 0xc4=196 bytes, real
 *                symbol name via `nm -C -S`) -- a single shared global
 *                search-state object, same "one shared instance, not
 *                per-object" shape as CFileIoAkai's own m_pStat.
 *   +0xfc        unknown int, zeroed by ctor, never read by any of the 25
 *                methods here -- reserved, meaning not recovered.
 *   +0x104       m_searchActive -- bool-shaped int, "a dir() search is
 *                currently open" flag. Zeroed by ctor.
 * UNLIKE CFileIoAkai/CFileIoIso9660, CFileIoDos has NO member path-scratch
 * buffer -- every path-taking method here passes the caller's own `const char*`
 * straight through to the underlying `pc_*`/`po_*` call, no local copy. The
 * `+0x108`-onward region this class's real object DOES occupy is instead an
 * embedded `fmtparms` struct used ONLY by `format(EDevice_Id, int, EFatType)`
 * (`.text+0x0831afc0`, 0xbd8=3032 bytes -- the single largest method in this
 * whole 3-class batch, a genuine multi-call resumable FAT-format state machine
 * driven by the static `CFileIoDos::iStage` global, .bss+0x93b0fe0, real symbol
 * name via `nm -C -S`). format() is DEFERRED -- see DECOMPILE_ERRORS.md for the
 * full rationale; not implemented here, no vtable slot for it exists on this
 * class.
 *
 * set_error() (`.text+0x0831aaa0`, 120 bytes, private, called from nearly every
 * other override's failure path) is SHORTER than CFileIoAkai::set_error() --
 * same 44-entry raw-code table shape (`.rodata+0x8eedf3c..0x8eedfe8`, fully
 * decoded via `objdump -s`), reads the SAME shared raw-error-code global
 * (`fs_user`, .bss+0x9608d90 -- confirmed identical address to
 * CFileIoAkai::set_error()'s own read, file_io_driver_common.h) and writes the
 * SAME `CFilesys::theFilesys->+0x48` field, but NEVER logs an Api assert --
 * unmapped/out-of-range raw codes are simply ignored (confirmed: no
 * `ds:0x930a1f4`/vtable+0x94 call anywhere in this function's disassembly, a
 * genuine, independently-confirmed difference from CFileIoAkai's own
 * set_error()). The table's MAPPED entries also differ at exactly one slot
 * (raw code 24 -> field 3 for Dos, silently ignored for Akai) -- see
 * file_io_dos.cpp's own `kDosErrTable` comment.
 *
 * fdummywrite()/fwrite() each layer one extra, method-specific fallback on top
 * of the shared set_error() plumbing -- see their own header comments below.
 *
 * fopen()'s mode-char translation (`.rodata+0x8eedfec`, 23-entry jump table
 * keyed on `mode[0]-'a'`, range 0..0x16) is a real embedded switch, NOT a
 * simple lookup table like CFileIoAkai's own -- most letters (including the
 * common 'b'..'g'/'i'..'q'/'s'..'u' range) share the exact same default-flags
 * code block as the out-of-range fallback; only 'a' ("append"), 'r' ("read"),
 * 'w' ("write") get real letter-specific `po_open()` flag/mode construction
 * (each additionally scanning the rest of the mode string for a `'+'` to
 * upgrade to read-write), plus two further real, distinct-but-unexplained
 * table slots at 'h' and 'v' (fixed flags, no `'+'`-scanning) -- transcribed
 * exactly, per-char semantics beyond 'a'/'r'/'w' not recovered.
 *
 * Ctor/dtor pair: SAME shape as CFileIoAkai's own -- ctor calls
 * CFileIoBase::CFileIoBase(), sets m_pStat/+0xfc/m_searchActive, calls
 * `pc_memory_init()` (SAME symbol CFileIoAkai's own ctor calls -- confirmed
 * identical address, a shared library-init hook); dtor (D1 `.text+0x08995570` /
 * D0 `.text+0x08995580`) matches CFileIoAkai's dtor pair byte-for-byte in shape
 * (D0 wraps `free(this)` in HAL_DisableInterrupts()/HAL_EnableInterrupts()).
 */

#ifndef FILE_IO_DOS_H
#define FILE_IO_DOS_H

#include "file_io_base.h"

/* Opaque: the real DOS/FAT library's per-drive search state struct (`dstat`,
 * out of scope). Never dereferenced by any method here, only passed through to
 * the (also out-of-scope, stand-in-modeled) `pc_*` library calls.
 */
struct DosDstat;

class CFileIoDos : public CFileIoBase {
public:
	/* .text+0x0831aa50. See header comment. */
	CFileIoDos();

	/* .text+0x08995570 (D1) / 0x08995580 (D0). See header comment. */
	virtual ~CFileIoDos();

	/* .text+0x0831a520, 6 bytes. Real: `return 6;` (this class's own
	 * EFileIOType tag).
	 */
	virtual int get_iotype();

	/* .text+0x0831a530, 25 bytes. Real: `pc_drno2dr(device)->maxClusterNo`
	 * (opaque `ddrive` field at +0x1c, meaning not recovered beyond this
	 * one read).
	 */
	virtual unsigned long getmaxclusterno(EDevice_Id device);

	/* .text+0x0831a550, 139 bytes. Real: `po_makelbaarray(arg2, &out[0],
	 * out[4])` (CFileLbaArray treated as an opaque byte blob at its own
	 * two known offsets, same "opaque past what's needed" convention as
	 * scsi_driver_base.h's SDriverIOPbuf); on failure returns -1; on
	 * success additionally probes `CDDriverIO::read_capacity()` +
	 * `pc_cluster_size()` and writes the computed sector size / cluster
	 * count / capacity back into the same blob at +0xc/+0xe/+0x10.
	 */
	virtual int getfilelbaarray(EDevice_Id device, int arg2, CFileLbaArray *out);

	/* .text+0x0831a5e0, 570 bytes. Real: `pc_IsVolumeLabelSkipped()`-gated
	 * pc_gfirst()/pc_gnext()/pc_gdone() iteration (skipping `.`/volume-
	 * label-flagged entries), decodes the found `dstat`'s own embedded
	 * date/time fields via 6 `CDateT::get()` calls (SAME kind sequence and
	 * inline divide-by-100-via-multiply idiom as CFileIoAkai::dir()'s own
	 * -- confirmed byte-identical arithmetic), then calls
	 * `CFileDirEntry::Initialize()`.
	 */
	virtual int dir(const char *path, int arg2, unsigned long &arg3, CFileDirEntry *entry);

	/* .text+0x0831a820, 22 bytes. Real: tail-call to
	 * `pc_totalfreecluster(short)` (out of scope).
	 */
	virtual unsigned long totalfreeclus(EDevice_Id device);

	/* .text+0x0831a840, 24 bytes. Real: `pc_ifree(device)`, EDX (high
	 * 32 bits) forced to 0 -- genuine 64-bit EDX:EAX return like
	 * CFileIoBase's own freebytes() default, but with a real non-zero low
	 * half.
	 */
	virtual unsigned long long freebytes(EDevice_Id device);

	/* .text+0x0831a860, 21 bytes. Real: tail-call to `po_ftell(int)` (out
	 * of scope).
	 */
	virtual long ftell(int handle);

	/* .text+0x0831a880, 456 bytes. Real: constructs a local
	 * `CFileDirEntry`, `strcpy`s `DEVICE_ID_STR[device]` into a scratch
	 * buffer, probes `CDDriverIO::read_capacity()` + the same per-device
	 * write-protect flags byte as CFileIoUnknown::getmediainfo() (`.bss+
	 * 0x93b0d54`), `strcpy`s a fixed `.rodata` volume-name suffix +
	 * `CFilePath::operator+=()`, then (if `pc_ShowVolumeLabel()` allows)
	 * iterates the root directory via the SAME `eax->vtable+0x50` virtual
	 * dispatch shape used elsewhere in this project looking for a
	 * `CDirEntry::IsLabel()` entry, extracts its name via
	 * `CFileDirEntry::GetNameExt()`, then calls `CMediaInfo::init()` with
	 * EFileIOType=6.
	 */
	virtual int getmediainfo(EDevice_Id device, CMediaInfo *info);

	/* .text+0x0831ab20, 265 bytes. Real: `pc_optimizemedium()`; on
	 * success, accumulates the moved-cluster count into the static local
	 * `NumOptimizedCluster` (.bss+0x93b0ff0, real symbol name via
	 * `nm -C -S`, capped against the caller-provided limit) and calls
	 * `pc_set_optimize_oemname()`; on failure, translates specific raw
	 * `fs_user` codes (0x12, 0x4) via set_error() and returns 0 if that
	 * mapped a real error, else -1 unconditionally for other failures.
	 */
	virtual int optimizemedium(EDevice_Id device, unsigned long a, unsigned long *b, int c);

	/* .text+0x0831ac30, 368 bytes. Real: `pc_drno2dr()` +
	 * `CDDriverIO::EnableProgress()` bracket a `pc_scandisk()` call;
	 * accumulates freed clusters into the static local `ulTotalFreeClus`
	 * (.bss+0x93b1000, real symbol name via `nm -C -S`); on the "found
	 * hidden hint" branch additionally calls `pc_readfsinfo()`/
	 * `pc_gethiddenclusters()`/`pc_writefsinfo()`.
	 */
	virtual int scandisk(EDevice_Id device, unsigned long a, unsigned long b, unsigned long *c, unsigned long *d);

	/* .text+0x0831ada0, 88 bytes. Real: `po_dummy_write(handle,
	 * size*count)`; on failure calls set_error() THEN, if
	 * `theFilesys->lastError` is STILL 0 afterward (set_error() didn't map
	 * anything), force-sets it to 2 directly -- a genuine method-specific
	 * fallback layered on top of the shared set_error() plumbing. Returns
	 * 0 on any failure (fwrite()-shaped sentinel), else element count.
	 */
	virtual unsigned int fdummywrite(unsigned int size, unsigned int count, int handle);

	/* .text+0x0831ae00, 87 bytes. Real: `pc_rmdir(path)`; on failure, if a
	 * search is open, `pc_gdone()`s it and retries `pc_rmdir()` once more
	 * before calling set_error() and returning -1.
	 */
	virtual int rmdir(const char *path);

	/* .text+0x0831ae60, 51 bytes. Real: `pc_mkdir(path)`; failure ->
	 * set_error(), -1.
	 */
	virtual int mkdir(const char *path);

	/* .text+0x0831aea0, 87 bytes. Real: `pc_unlink(path)`; same
	 * search-open retry shape as rmdir().
	 */
	virtual int remove(const char *path);

	/* .text+0x0831af00, 59 bytes. Real: `pc_mv(oldPath, newPath)`; failure
	 * -> set_error(), -1.
	 */
	virtual int rename(const char *oldPath, const char *newPath);

	/* .text+0x0831af40, 64 bytes. Real: `pc_pwd(DEVICE_ID_STR[device],
	 * buf)` (SAME shared global table as CFileIoAkai::getwd(), "0 ==
	 * failure" convention); failure -> set_error(), return 0; else return
	 * `buf`.
	 */
	virtual int getwd(EDevice_Id device, char *buf);

	/* .text+0x0831af80, 51 bytes. Real: `pc_set_cwd(path)`; failure ->
	 * set_error(), -1.
	 */
	virtual int chdir(const char *path);

	/* .text+0x0831bba0, 59 bytes. Real: `po_resize(handle, newSize)`;
	 * failure -> set_error(), -1.
	 */
	virtual int resize(int handle, unsigned int newSize);

	/* .text+0x0831bbe0, 51 bytes. Real: `po_flush(handle)`; failure ->
	 * set_error(), -1.
	 */
	virtual int fflush(int handle);

	/* .text+0x0831bc20, 63 bytes. Real: `po_lseek(handle, offset,
	 * whence)`; negative -> set_error(), -1.
	 */
	virtual int fseek(int handle, long offset, int whence);

	/* .text+0x0831bc60, 96 bytes. Real: `po_write(handle, buf,
	 * size*count)`; on failure, if `theFilesys->lastError == 2` already,
	 * SKIPS set_error() entirely (a distinct short-circuit from
	 * set_error()'s own "!=0" check); else calls set_error(). Returns 0 on
	 * failure, else element count.
	 */
	virtual unsigned int fwrite(const void *buf, unsigned int size, unsigned int count, int handle);

	/* .text+0x0831bcc0, 76 bytes. Real: `po_read(handle, buf,
	 * size*count)`; -1 -> set_error(), return 0; else element count.
	 */
	virtual unsigned int fread(void *buf, unsigned int size, unsigned int count, int handle);

	/* .text+0x0831bd10, 51 bytes. Real: `po_close(handle)`; nonzero ->
	 * set_error(), -1.
	 */
	virtual int fclose(int handle);

	/* .text+0x0831bd50, 319 bytes. Real: mode[0]-keyed jump table (23
	 * entries, see header comment) builds `po_open()`'s flags/mode
	 * arguments, scanning the rest of `mode` for a `'+'`; negative result
	 * -> set_error(), return the raw negative handle; else return the
	 * handle.
	 */
	virtual int fopen(const char *path, const char *mode);

	/* .text+0x0831be90, 128 bytes. Real: if a search is open AND the
	 * shared search state's own drive index (read through 2 levels of
	 * indirection off m_pStat -- opaque, not modeled, always "no match"
	 * here) equals `device`, closes the search first; either way then
	 * unconditionally calls `pc_dskfree(device, 1)` -- "0 == failure"
	 * convention, failure -> set_error(), -1; else 0.
	 */
	virtual int funmount(EDevice_Id device);

	/* .text+0x0831bf10, 253 bytes. Real: `CDDriverIO::read_capacity()`
	 * probe; if the reported sector size is already 0x200, calls
	 * `pc_dskinit(device)` directly (with an extra `pc_drno2dr()`-gated
	 * capacity-range check on failure); otherwise retries via
	 * `CDDriverIO::scsi_mode_sel(device, 0x200)` + a second
	 * `read_capacity()` bounded-delta check before falling back to the
	 * 0x200 init path. On failure calls set_error() (fixed field=3) and
	 * returns -1; success returns 0.
	 */
	virtual int fmount(EDevice_Id device);

	/* .text+0x0831aaa0, 120 bytes, non-virtual helper (used by nearly
	 * every override above and callable directly, same convention as
	 * CFileIoAkai::set_error()). Real: translates `*fs_user` into
	 * `CFilesys::theFilesys->lastError` via a 44-entry table; unmapped
	 * codes are silently ignored (NO Api-assert log, unlike
	 * CFileIoAkai::set_error()). See header comment.
	 */
	void set_error();

private:
	DosDstat *m_pStat;
	int m_reserved0xfc;
	int m_searchActive;

public:
	/* .bss+0x93b0fe0, real symbol name via `nm -C -S` -- drives the
	 * deferred format()'s resumable state machine (DECOMPILE_ERRORS.md).
	 * Declared here for completeness even though nothing in this TU
	 * currently reads/writes it.
	 */
	static int iStage;
};

#endif /* FILE_IO_DOS_H */
