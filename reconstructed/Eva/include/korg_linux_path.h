/*
 * korg_linux_path.h  -  CKorgLinuxPath, CKorgPath's one real concrete
 * override (korg_path.h). See that header for the shared vtable-shape /
 * self-containment / rejected-candidate writeup.
 *
 * OBJECT LAYOUT: identical to CKorgPath's own (no added fields) -- every
 * ctor/Copy() allocates exactly `operator new(0x104)`, matching the base's
 * `sizeof == 0x104` exactly. Vtable at `.rodata+0x08f79dc0` (typeinfo
 * `0x08f79dfc`), 8 slots, ALL real overrides (no `__cxa_pure_virtual` left):
 *   slot 0/1: ~CKorgLinuxPath() D1/D0 (.text+0x089d2d80/0x089d2da0)
 *   slot 2:   Copy() const                       (0x089d2dd0)
 *   slot 3:   Separator() const                  (0x089d2d00)
 *   slot 4:   GetOposPath(char*, unsigned int)    (0x089d2d60)
 *   slot 5:   SetOposPath(char const*)            (0x089d2d10)
 *   slot 6:   TemporaryFileUsingExtension(char const*) const (0x089d30f0)
 *   slot 7:   FindRecurse(char const*, CKorgPath const*) const (0x089d2e20)
 *
 * FindRecurse() ALGORITHM (.text+0x089d2e20, 670 bytes -- the family's
 * deepest real method, fully traced instruction-by-instruction, not
 * inferred): `opendir(dir->mFileName)`; NULL -> return NULL immediately.
 * Otherwise `readdir()`s each entry:
 *   - d_type==DT_REG (8): skip if the entry name is "." or ".." or starts
 *     with '.' (a hidden dotfile -- same 2 literal string compares
 *     ("."/"..") ground truth's own ::Valid() below independently performs).
 *     Otherwise `strcmp(entryName, name)`; on exact match, `closedir()`,
 *     heap-allocate a fresh CKorgLinuxPath, `Set(dir, name)` it (builds
 *     "dir/name"), and return it.
 *   - d_type==DT_DIR (4): same hidden-dotfile skip. Otherwise build a
 *     temporary subdirectory path (`dir->GetPath() + "/" + entryName`, via
 *     a stack CKorgPath forced to CKorgLinuxPath's own vtable) and
 *     recursively call `this->FindRecurse(name, &subdir)` through the
 *     vtable. Non-NULL result -> `closedir()`, destroy the temp subdir,
 *     return it. NULL result -> destroy the temp subdir, keep scanning.
 *   - any other d_type: skip, keep scanning.
 *   End of directory (readdir returns NULL) -> `closedir()`, return NULL.
 * Net effect: recursively finds a REGULAR file named exactly `name`
 * anywhere under `dir`'s tree (directories themselves are never matched
 * against `name`, only recursed into), skipping hidden entries.
 *
 * TemporaryFileUsingExtension(const char *ext) const (.text+0x089d30f0):
 * builds a NEW CKorgPath (base ctor, name=NULL) via `operator new(0x104)`,
 * copies `this->GetPathName()` into a scratch buffer, appends ext (bounded),
 * strips at the last '.' of the ORIGINAL name first if `ext` itself starts
 * with '.', then... (see .cpp for the exact byte-for-byte transcription --
 * ground truth builds `<this's own name, extension replaced by `ext`>` as a
 * NEW CKorgPath::CKorgPath(char const*) using strncat, no directory
 * component). Caller owns the result.
 */

#ifndef KORG_LINUX_PATH_H
#define KORG_LINUX_PATH_H

#include "korg_path.h"

class CKorgLinuxPath : public CKorgPath {
public:
	/* .text+0x089d3280. Thin wrapper: base-constructs via
	 * CKorgPath(name), then installs this class's own vtable. */
	CKorgLinuxPath(const char *name);

	/* .text+0x089d32b0. Same shape, base-constructs via
	 * CKorgPath(const CKorgPath*). */
	CKorgLinuxPath(const CKorgPath *other);

	/* .text+0x089d2d80 (D1) / 0x089d2da0 (D0, additionally
	 * `operator delete(this)`). */
	virtual ~CKorgLinuxPath();

	/* .text+0x089d3330. STATIC (ground truth's own single-arg call site
	 * confirms no `this` receiver -- same shape as FindRecurse's own
	 * inline hidden-file check). True iff `name` is neither "." nor
	 * ".." nor starts with '.'. */
	static int Valid(const char *name);

	virtual CKorgPath *Copy() const;
	virtual char Separator() const;
	virtual void GetOposPath(char *dest, unsigned int maxLen);
	virtual void SetOposPath(const char *path);
	virtual CKorgPath *TemporaryFileUsingExtension(const char *ext) const;
	virtual CKorgPath *FindRecurse(const char *name, const CKorgPath *dir) const;
};

#endif /* KORG_LINUX_PATH_H */
