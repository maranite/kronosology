/*
 * file_io_iso9660.h  -  CFileIoIso9660, the ISO9660 CD-ROM filesystem driver
 * (file_io_base.h's own "OUT OF SCOPE" list -- picked up 2026-07-28 alongside
 * CFileIoAkai/CFileIoDos as the next storage-cluster batch).
 *
 * REAL SHAPE: 17 non-ctor/dtor `nm -C` symbols (`.text+0x0831c020`..
 * `0x0831ca70`, 19 total .text entry points incl. ctor + the D1/D0 dtor pair) +
 * `vtable for CFileIoIso9660` (`.data.rel.ro+0x08f31b40`, 53 slots, same layout
 * as `vtable for CFileIoBase`, confirmed by `objdump -s`). 12 real CFileIoBase
 * virtual overrides (get_iotype, freebytes, getmediainfo, format (2-arg only --
 * same "only the 2-arg overload overridden" shape as CFileIoAkai), ftell,
 * fclose, funmount, fread, getwd, fseek, dir, chdir, fopen, fmount -- that's
 * 14, see below) plus 2 non-virtual private helpers (ConvertPathRtfsToCdfs,
 * set_error) shared by several overrides' bodies.
 *
 * Wraps a real embedded ISO9660/CDFS filesystem library (`cd_*` prefix,
 * `.text+0x8383xxx`..`0x8385xxx`) -- out of scope here (not reconstructed,
 * modeled as inert stand-ins, same convention as file_io_unknown.h's
 * CDDriverIO/CFilesys stand-ins).
 *
 * MEMBER LAYOUT (`this`, from ctor `.text+0x0831c470` + cross-checked against
 * every method's own field reads/writes) -- SAME shape as CFileIoAkai's own:
 *   +0x00        vptr (compiler)
 *   +0x04        m_pStat -- IsoCddstat* (opaque), ctor hardcodes it to the
 *                real global `cfioiso_dir_obj` (.bss+0x93b1100, 0x12a=298
 *                bytes, real symbol name via `nm -C -S`) -- a single shared
 *                global search/IO state object.
 *   +0xfc        unknown int, zeroed by ctor, never read by any method here.
 *   +0x104       m_searchActive -- bool-shaped int, "a dir() search is
 *                currently open" flag. Zeroed by ctor.
 *   +0x108..0x1f8 m_path[0xf1] (241 bytes) -- fixed-size scratch path buffer,
 *                filled via `strncpy(m_path, path, 0xf0)` then a manual
 *                `m_path[0xf0] = 0` null-terminate -- SAME idiom as
 *                CFileIoAkai's own m_path.
 *
 * ConvertPathRtfsToCdfs() (`.text+0x08317ff0`... no, `.text+0x0831c4f0`, 256
 * bytes, private helper called by dir()/chdir()/fopen() before every real
 * `cd_*` call) rewrites an RTFS-style (backslash-separated, optional
 * `C:`-prefix) path into CDFS conventions: strips a leading drive-letter
 * prefix if present, then for the final path component splits at the first
 * space (an 11-char fixed-field name convention) and first `.` after that
 * space, uppercasing/truncating as it goes, appending a literal space
 * (`.rodata+0x8eee3c6`, a single-space padding literal shared with several
 * other reconstructed classes' own volume-label-formatting code) before the
 * extension when one was found (`.rodata+0x8eee3c6` == "." -- confirmed via
 * `objdump -s`, corrects an earlier assumption of a space-padding literal).
 * The exact tail-call argument wiring for the final `strcat()` pair is a
 * medium-confidence transcription -- file_io_iso9660.cpp's own comment has
 * details.
 *
 * set_error() (`.text+0x0831c9e0`, 132 bytes, private, called from nearly
 * every other override's failure path -- NOTE: fread()/fseek()/getwd()/
 * chdir()/fopen() all INLINE this exact same logic instead of calling the
 * named function, confirmed via `objdump -dr` byte-comparison; behaviorally
 * identical, modeled here as all calling the one named set_error() method)
 * uses a COMPLETELY DIFFERENT mechanism from CFileIoAkai/CFileIoDos's own
 * 44-entry jump tables: reads a single raw error code global `cd_errno`
 * (.bss+0x9647ff4, plain `int`, NOT a pointer -- real symbol name via
 * `nm -C -S`), range-checks it against 0..0xa (11 values), then tests it
 * against two bitmasks (`1u << raw`, `& 0x428` -> field 1, else `& 0x3c3` ->
 * field 3, else falls through to the Api-assert log same as CFileIoAkai's own
 * -- decoded via `objdump -dr` bit-shift/AND-immediate reads, not a lookup
 * table) -- raw codes {3,5,10} map to field 1, {0,1,6,7,8,9} map to field 3,
 * {2,4,>10} log (line 706 = 0x2c2, `"DiskUtil/CustomFs/FileIoIso9660.cpp"`,
 * confirmed via `objdump -s -j .rodata`).
 *
 * Ctor/dtor pair: SAME shape as CFileIoAkai's own, with one real difference --
 * the ctor's `cd_memory_init()` call (`.text+0x838bd90`, out of scope) can
 * itself FAIL (return 0), in which case the ctor logs an Api assert (line
 * 0x2b=43, same two `.rodata` strings as set_error()'s own) before still
 * completing the same field init as the success path (real: `eb b8 jmp` back
 * into the shared tail -- transcribed as an unconditional field-init with a
 * conditional log, not a hard failure).
 */

#ifndef FILE_IO_ISO9660_H
#define FILE_IO_ISO9660_H

#include "file_io_base.h"

/* Opaque: the real ISO9660-library per-drive search/IO state struct
 * (`cddstat`, out of scope). Never dereferenced by any method here, only
 * passed through to the (also out-of-scope, stand-in-modeled) `cd_*` library
 * calls.
 */
struct IsoCddstat;

class CFileIoIso9660 : public CFileIoBase {
public:
	/* .text+0x0831c470. Calls CFileIoBase::CFileIoBase(), initializes
	 * m_pStat to the real shared global `cfioiso_dir_obj`, zeroes +0xfc
	 * and m_searchActive, calls `cd_memory_init()` (out of scope, logs an
	 * Api assert on failure -- see header comment).
	 */
	CFileIoIso9660();

	/* .text+0x089955c0 (D1) / 0x089955d0 (D0). Same shape as
	 * CFileIoAkai's own dtor pair.
	 */
	virtual ~CFileIoIso9660();

	/* .text+0x0831c020, 6 bytes. Real: `return 4;` (this class's own
	 * EFileIOType tag).
	 */
	virtual int get_iotype();

	/* .text+0x0831c030, 5 bytes. Real: `xor eax,eax; xor edx,edx; ret` --
	 * genuine 64-bit EDX:EAX return, `return 0;`.
	 */
	virtual unsigned long long freebytes(EDevice_Id device);

	/* .text+0x0831c040, 139 bytes. Real: queries
	 * CDDriverIO::read_capacity() + `cd_drvno2drv(device)->+0x5fa` (a
	 * per-drive byte flag, opaque -- selects between two fixed `.rodata`
	 * name literals via `cmovne`), forwards into CMediaInfo::init() with
	 * EFileIOType=4, extra=0x800, returns 0 unconditionally.
	 */
	virtual int getmediainfo(EDevice_Id device, CMediaInfo *info);

	/* .text+0x0831c0d0, 73 bytes. Real: tail-calls
	 * CFilesys::get_fileioptr(3)->format(device, arg2, EFatType(?)) through
	 * the 3-arg format() vtable slot -- SAME tail-call-through-vtable
	 * shape as CFileIoUnknown/CFileIoAkai's own, fixed selector 3.
	 */
	virtual int format(EDevice_Id device, int arg2);

	/* .text+0x0831c120, 25 bytes. Real: `cd_fileno2file(handle)->+0x14`
	 * (opaque `cddstat`-per-handle record field, meaning not recovered
	 * beyond this one read).
	 */
	virtual long ftell(int handle);

	/* .text+0x0831c140, 24 bytes. Real: `cd_close(handle)`; always
	 * returns 0 (the return value of `cd_close()` itself is discarded).
	 */
	virtual int fclose(int handle);

	/* .text+0x0831c160, 118 bytes. Real: if a search is open, closes it
	 * via `cd_gdone()`; then unconditionally builds a per-device CDFS
	 * root path (`DEVICE_ID_STR`-analog table at `.rodata+0x8eef2c0`,
	 * 10 `const char*` entries) via `CFilePath::operator+=()` and calls
	 * `cd_dskclose(path)`. Always returns 0.
	 */
	virtual int funmount(EDevice_Id device);

	/* .text+0x0831c1e0, 177 bytes. Real: `cd_read(handle, buf,
	 * size*count)`; -1 -> set_error(), return 0; else element count.
	 */
	virtual unsigned int fread(void *buf, unsigned int size, unsigned int count, int handle);

	/* .text+0x0831c2a0, 156 bytes. Real: builds the same per-device CDFS
	 * root path as funmount() via `CFilePath::operator+=()`, calls
	 * `cd_gcwd(path)`; on success, appends a `\` (`CFilePath::get_last()`
	 * gate + `strcat`, `.rodata+0x8eee3c4` -- confirmed "\\" via
	 * `objdump -s`) if the last component wasn't already empty, then
	 * `strncpy`s the result into `buf`; on failure, calls set_error() and
	 * returns 0.
	 */
	virtual int getwd(EDevice_Id device, char *buf);

	/* .text+0x0831c3c0, 168 bytes. Real: `cd_lseek(handle, offset,
	 * whence)`; negative -> set_error(), -1; else 0.
	 */
	virtual int fseek(int handle, long offset, int whence);

	/* .text+0x0831c5f0, 567 bytes -- the largest non-format method in this
	 * batch (matching CFileIoAkai::dir()'s own role). Real: `cd_gfirst()`/
	 * `cd_gnext()`/`cd_gdone()` iteration over `ConvertPathRtfsToCdfs()`'d
	 * paths, filtering entries by a real attribute-byte bitmask
	 * (`record+0x12`, bits 0x4/0x1/0x2 gate a hidden-flag-count check
	 * against `record+0x18`), decodes an ISO9660 packed date/time field
	 * (`record+0xb..0x11`, a real inline divide-by-100-via-multiply idiom
	 * SAME shape as CFileIoAkai/CFileIoDos's own, plus a real
	 * `char2long()` call for the raw LBA at `record+0x7`), then calls
	 * `CFileDirEntry::Initialize()`.
	 */
	virtual int dir(const char *path, int arg2, unsigned long &arg3, CFileDirEntry *entry);

	/* .text+0x0831c830, 181 bytes. Real: `ConvertPathRtfsToCdfs(path)`
	 * into m_path+0x108, `cd_scwd(m_path)`; failure -> set_error(), -1.
	 */
	virtual int chdir(const char *path);

	/* .text+0x0831c8f0, 228 bytes. Real: `ConvertPathRtfsToCdfs(path)`;
	 * if a search is open, `cd_gdone()`s it first; `cd_open(m_path)`;
	 * negative -> set_error(), return the raw negative handle; else
	 * return the handle.
	 */
	virtual int fopen(const char *path, const char *mode);

	/* .text+0x0831ca70, 253 bytes. Real: `CDDriverIO::getdevinfo()`-gated
	 * probe (SAME `+0x1 == 7` check as CFileIoAkai::fmount()'s own) ->
	 * sector-size probe/retry (SAME shape as CFileIoAkai::fmount()'s own)
	 * -> builds a per-device CDFS root path and calls `cd_dskopen()`,
	 * calling set_error() if that fails. GENUINE, INDEPENDENTLY VERIFIED
	 * FINDING: every single path through this method returns -1,
	 * INCLUDING the fully-successful `cd_dskopen()` case -- the return
	 * register (`esi`) is set to -1 once at function entry and never
	 * reassigned anywhere in the function body (confirmed via direct
	 * disassembly, not a transcription slip). The real side-effecting
	 * calls (scsi_mode_sel/cd_dskopen) still happen; only the reported
	 * result is always failure.
	 */
	virtual int fmount(EDevice_Id device);

	/* .text+0x0831c9e0, 132 bytes, non-virtual helper (used by nearly
	 * every override above and callable directly, same convention as
	 * CFileIoAkai::set_error()/CFileIoDos::set_error()). See header
	 * comment.
	 */
	void set_error();

private:
	/* .text+0x0831c4f0, 256 bytes, non-virtual helper. See header
	 * comment.
	 */
	void ConvertPathRtfsToCdfs(const char *path);

	IsoCddstat *m_pStat;
	int m_reserved0xfc;
	int m_searchActive;
	char m_path[0xf1];
};

#endif /* FILE_IO_ISO9660_H */
