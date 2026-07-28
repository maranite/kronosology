/*
 * kontakt_opos_path.h  -  UKontaktOposPath, the free-function-namespace pair
 * that converts between the old "OPOS" drive-letter path format (e.g.
 * "C:\FOLDER\FILE.WAV", one ASCII drive letter A-H mapping to a real Linux
 * mount point) and native Linux paths. Real ground truth:
 * `.text+0x0846ca20`/`0x0846cbe0`, `UKontaktOposPath::ConvertOposToLinux`/
 * `ConvertLinuxToOpos` (a `union`-style helper namespace, per the real
 * mangled `U`-prefix -- same convention as `UKontaktConvert`).
 *
 * Found 2026-07-28, same class-inventory sweep as korg_path.h -- these two
 * methods are `CKorgPath`/`CKorgLinuxPath`'s own `GetOposPath`/`SetOposPath`
 * real bodies (`korg_path.h`'s own header comment), the ONE place this
 * batch's otherwise fully self-contained path family reaches outside
 * itself. A 3rd real `UKontaktOposPath` method,
 * `TemporaryFileUsingExtension(char const*, char const*, char*, unsigned int)`
 * (.text+0x0846c900), is NOT reconstructed here -- confirmed via its own
 * `objdump -d -C` that it calls `CFileOperation::Mkdir(char const*)` and
 * `CDiskModeManager::GetInstance()` (a wholly unrelated, unmodeled
 * out-of-scope class), and -- independently confirmed via a full call-xref
 * check of `CKorgPath::CKorgPath.text` and `CKorgLinuxPath::.text` -- is
 * never called by anything in this batch's own family (the *virtual*
 * `CKorgLinuxPath::TemporaryFileUsingExtension(char const*) const`,
 * .text+0x089d30f0, is a completely different, already-reconstructed
 * function that happens to share a name -- see korg_path.h).
 *
 * REAL ALGORITHM (both confirmed return `int`, 1 == success / 0 == failure --
 * shared tail at .text+0x0846cbc8 sets eax=1 before `ConvertOposToLinux`'s
 * final `ret`; `xor eax,eax` at entry is never overwritten on any bail path.
 * Same shape confirmed for `ConvertLinuxToOpos` via its own eax=0 init /
 * eax=1 success-tail at 0x0846cec7):
 *   ConvertOposToLinux(oposPath, dest, maxLen): `dest[0]=0` first, always.
 *     If `oposPath[1] != ':'` (no drive-letter prefix) OR `oposPath[0]` is
 *     outside 'A'..'H', returns 0 with `dest` left empty (ground truth does
 *     NOT fall back to copying `oposPath` verbatim -- confirmed directly
 *     from the early-bail branch, no further instructions before the
 *     shared restore-and-`ret` tail). Otherwise `oposPath[0]-'A'` selects a
 *     real Linux mount-point string via `CFileOperation::GetLinuxRemapPath()`
 *     (own real body: a small static per-device-id table lookup, out of
 *     scope -- see long_binary_file.h's own updated header comment), copies
 *     that mount path into `dest`, then appends the REST of `oposPath`
 *     (from index 2, past "X:") translating every '\\' to '/', bounded to
 *     maxLen, and returns 1.
 *   ConvertLinuxToOpos(linuxPath, dest, maxLen): `dest[0]=0` first, always.
 *     Tries `GetLinuxRemapPath(0)` through `GetLinuxRemapPath(7)` in turn (a
 *     real, fully-unrolled 8-iteration sequence in ground truth -- matches
 *     ConvertOposToLinux's own 'A'..'H' 8-device range) via
 *     `strncmp(linuxPath, mountPath, strlen(mountPath))`; the first that's a
 *     prefix of `linuxPath` selects that device's drive letter ('A'+id,
 *     followed by ':'), and the remainder of `linuxPath` (past the matched
 *     mount-point prefix) is appended into `dest` with every '/' translated
 *     to '\\', bounded to maxLen; returns 1. No match among all 8 -> `dest`
 *     stays empty, returns 0.
 *
 * `EDevice_Id` here is the SAME opaque enum `file_io_base.h` already
 * declares (`long_binary_file.h` now includes it too, for
 * `CFileOperation::GetLinuxRemapPath()`'s declaration) -- this pass does not
 * independently fix which literal values A-H map to, only that the lookup
 * itself is a `CFileOperation`-owned table this project already treats as
 * out of scope.
 */

#ifndef KONTAKT_OPOS_PATH_H
#define KONTAKT_OPOS_PATH_H

namespace UKontaktOposPath {

/* .text+0x0846ca20, 431 bytes. Returns 1 on success, 0 if oposPath has no
 * valid "X:" drive-letter prefix (dest left as ""). */
int ConvertOposToLinux(const char *oposPath, char *dest, unsigned int maxLen);

/* .text+0x0846cbe0. Returns 1 on success, 0 if linuxPath doesn't start with
 * any of the 8 known device mount points (dest left as ""). */
int ConvertLinuxToOpos(const char *linuxPath, char *dest, unsigned int maxLen);

} /* namespace UKontaktOposPath */

#endif /* KONTAKT_OPOS_PATH_H */
