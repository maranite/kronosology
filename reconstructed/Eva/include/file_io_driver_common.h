/*
 * file_io_driver_common.h  -  shared plumbing for the CFileIoAkai / CFileIoDos /
 * CFileIoIso9660 concrete media drivers (file_io_base.h's own "OUT OF SCOPE" list,
 * picked up 2026-07-28 as the next storage-cluster batch after CFileIoUnknown).
 *
 * All three classes' own set_error() bodies -- and several other heavy methods'
 * failure paths that call set_error() -- funnel through the SAME two real
 * process-wide globals, confirmed by direct disassembly of all three (`objdump -dr
 * -M intel`, Decomp/EVA_Decomp/Eva):
 *
 *   CFilesys::theFilesys (.bss+0x93b1640, a CFilesys* singleton pointer) -- every
 *   set_error() first checks `theFilesys->+0x48 != 0` ("an error is already
 *   pending, don't overwrite/re-log it") before doing anything else. CFilesys
 *   itself (87 methods) is out of scope (file_io_base.h) -- modeled here as an
 *   opaque shim exposing only that one int field, `lastError`. Centralized (not
 *   triplicated per-TU like file_io_unknown.cpp's own smaller stand-ins) because
 *   this field's shared-singleton semantics actually matter: three independent
 *   static-local copies would silently desync the "already pending" short-circuit
 *   across classes. Storage defined once, in file_io_akai.cpp.
 *
 *   Api (.bss+0x930a1f4, CSystemApi*) -- same global already declared locally by
 *   file_io_base.cpp/tempo.cpp/mains.cpp/config_manager.cpp (system_api.h). Two of
 *   the three drivers' set_error() (Akai, Iso9660 -- NOT Dos, see file_io_dos.cpp's
 *   own header comment) call its vtable+0x94 assert-report slot with the exact
 *   same format string already established project-wide
 *   ("Assertion failed in module %s, line %i.\n", confirmed byte-identical via
 *   `objdump -s -j .rodata` at .rodata+0x8e7bef8) -- centralized as
 *   FileIoDriverApiAssert() below since only the (filename, line) pair differs
 *   per call site.
 *
 * Also centralizes the two raw-error-code globals each driver's set_error()
 * translates into theFilesys->lastError:
 *
 *   fs_user (.bss+0x9608d90, `int*`) -- shared IDENTICALLY by CFileIoAkai::
 *   set_error() and CFileIoDos::set_error() (confirmed: both read the exact same
 *   address). Points at the underlying aki_ / pc_ driver library's own
 *   last-raw-error-code int; real symbol name confirmed via `nm -C -S` (an odd
 *   name for this role, transcribed as-is). Modeled as a plain int + a pointer to
 *   it, zero-initialized (nothing in this reconstruction's own traced boot path
 *   populates the real pointee) -- default raw code 0 maps to "no error" in both
 *   drivers' own translation tables.
 *
 *   cd_errno (.bss+0x9647ff4, plain `int`, NOT a pointer) -- CFileIoIso9660-only
 *   raw error code, real symbol name confirmed via `nm -C -S`. Read directly (no
 *   pointer indirection), unlike fs_user.
 */

#ifndef FILE_IO_DRIVER_COMMON_H
#define FILE_IO_DRIVER_COMMON_H

#include "system_api.h"

/* Real ground truth: CFilesys::theFilesys (.bss+0x93b1640). CFilesys (87 methods)
 * is out of scope -- modeled as an opaque shim exposing only the +0x48 field every
 * set_error() body reads/writes. Storage defined in file_io_akai.cpp.
 */
struct CFilesysErrShim {
	unsigned char pad[0x48];
	int lastError;
};
extern CFilesysErrShim *g_theFilesys;

/* Real ground truth: CSystemApi *Api (.bss+0x930a1f4) -- same global declared
 * locally elsewhere project-wide (file_io_base.cpp, tempo.cpp, ...). Storage
 * defined in file_io_akai.cpp.
 */
extern CSystemApi *Api;

/* Real ground truth: fs_user (.bss+0x9608d90). Storage defined in
 * file_io_akai.cpp.
 */
extern int g_rawFsErrorStorage;
extern int *fs_user;

/* Real ground truth: cd_errno (.bss+0x9647ff4). Storage defined in
 * file_io_iso9660.cpp (only CFileIoIso9660 reads it).
 */
extern int cd_errno;

/* Real ground truth: DEVICE_ID_STR (.data+0x91b9900, 10 `char*` entries) --
 * shared identically by CFileIoAkai::getwd() and CFileIoDos::getwd()/
 * getmediainfo() (confirmed: all read the exact same address, indexed by
 * `device`). Storage defined in file_io_akai.cpp.
 */
extern char *DEVICE_ID_STR[10];

/* Real Api+0x94 assert-report call, same idiom as file_io_base.cpp's own
 * ApiAssert() -- (Api, fmt, filename, line), fmt is the fixed project-wide
 * "Assertion failed in module %s, line %i.\n" string. Never fatal (same
 * "forwards to the standing EvaVTableStub" note as file_io_base.cpp's copy).
 */
inline void FileIoDriverApiAssert(const char *filename, int line)
{
	typedef void (*Fn)(void *, const char *, const char *, int);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x94);
	fn(Api, "Assertion failed in module %s, line %i.\n", filename, line);
}

#endif /* FILE_IO_DRIVER_COMMON_H */
