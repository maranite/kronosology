/*
 * korg_path.h  -  CKorgPath, an abstract cross-platform path/filename value
 * class with exactly one real concrete override in ground truth,
 * CKorgLinuxPath (korg_linux_path.h). Found 2026-07-28, the same `nm -C`
 * class-inventory sweep that landed `korg_file.h`'s own `CKorgFile` --
 * despite the similar name, this is a SEPARATE, unrelated real class
 * hierarchy (`CKorgPath`/`CKorgLinuxPath`, 26+14 raw symbols) with its own
 * distinct vtable/typeinfo (`.rodata+0x08f79d80`/`0x08f79dc0`) -- NOT a
 * sibling of `CKorgFile`'s own `CKorgFileKMP`/`CKorgFileKSC`/`CKorgFileKSF`
 * trio.
 *
 * SELF-CONTAINMENT: confirmed via a full `objdump -d -C` sweep of every one
 * of CKorgPath's and CKorgLinuxPath's own real (non-inherited) methods --
 * every call target is either libc (`strncpy`/`strncat`/`strlen`/`strcmp`/
 * `strcasecmp`/`strchr`/`strrchr`/`fopen`/`fclose`/`opendir`/`readdir`/
 * `closedir`), `operator new`/`delete`, this same 2-class family's own
 * methods (including virtual dispatch through the object's own vtable), or
 * `UKontaktOposPath::ConvertOposToLinux`/`ConvertLinuxToOpos`
 * (kontakt_opos_path.h) -- the ONE real external dependency, itself only 2
 * methods deep and reaching CFileOperation for exactly one already-modeled
 * static-table-lookup slice (`GetLinuxRemapPath`, long_binary_file.h). A 3rd
 * `UKontaktOposPath` method and a static overload of
 * `TemporaryFileUsingExtension` were checked and found to reach the
 * out-of-scope `CDiskModeManager`/`CFileOperation::Mkdir` -- confirmed via
 * the same sweep to have NO caller anywhere in this family's own .text
 * range, so their exclusion doesn't cost anything here (see
 * kontakt_opos_path.h's own header comment).
 *
 * OTHER CANDIDATES REJECTED THIS SWEEP (before landing on this family):
 *   - CDirectoryElem (30 methods) -- pulls in the entire already-deferred
 *     CDirEntry accessor surface (dir_entry.h) plus the out-of-scope
 *     growable CZ container. Rejected.
 *   - CFileKge (38 methods) -- every method touches CFMBrowseForm/
 *     CChunkHeaderBase/CStorageBankHeader, the same UI-form/chunk-subsystem
 *     god-object network already rejected for CLoadKontaktBankMgr
 *     (seq_pattern_data.h). Rejected.
 *   - CAsyn/CSubBuff (40/35 methods) -- both already explicitly excluded by
 *     stream_family.h's own header comment; re-confirmed via a fresh xref
 *     (CAsyn forwards into the 225-method CFMApiInstance god object).
 *     Rejected.
 *   - CFileKSC/CFileSng/CFilePcg (77/88/146 methods, the other large
 *     untouched file-format classes from the same sweep) -- all three pull
 *     in deep, currently-unmodeled sample-bank/song-storage subsystems
 *     (CSTGMultisampleBank, CSongControl, CStorageChunkHandler chains) on
 *     nearly every method. Rejected.
 *
 * VTABLE SHAPE (direct `.rodata` byte dump at 0x08f79d80, cross-checked
 * slot-for-slot against CKorgLinuxPath's own overriding addresses):
 *   slot 0: ~CKorgPath() D1 (complete-object)
 *   slot 1: ~CKorgPath() D0 (deleting)
 *   slot 2: Copy() const                        -- PURE virtual in base
 *           (`__cxa_pure_virtual`, .text+0x0804c6ac -- confirmed via nm)
 *   slot 3: Separator() const                    -- PURE virtual in base
 *   slot 4: GetOposPath(char*, unsigned int)      -- REAL default body in
 *           base (plain `strncpy` of the raw path, no conversion -- dead in
 *           practice since CKorgPath itself is never instantiable, but
 *           transcribed faithfully)
 *   slot 5: SetOposPath(char const*)              -- REAL default body,
 *           same "plain copy" shape as slot 4
 *   slot 6: TemporaryFileUsingExtension(char const*) const -- PURE virtual
 *   slot 7: FindRecurse(char const*, CKorgPath const*) const -- PURE virtual
 * Every other CKorgPath method (Set/GetPath/SetPath/GetPathName/
 * GetPathExtension/GetPathNameNoExtension/GetFolder/MakePathFromFolder/
 * Find/HasExtension/AddExtension/RemoveExtension x2/ValidExtension/
 * Sanitize/Capitalized/Make/ctors) is a plain non-virtual member --
 * confirmed absent from the vtable dump.
 *
 * OBJECT LAYOUT (from every ctor's own field offsets, all agreeing):
 *   +0x000  vtable ptr
 *   +0x004  char mFileName[0x100]
 *   sizeof == 0x104 (260 bytes) -- confirmed via CKorgLinuxPath::Copy()'s
 *   own `operator new(0x104)` and CKorgPath::Make()'s identical allocation.
 *
 * CKorgPath::Make(const char *path) (.text+0x089d32e0) is the real
 * cross-platform factory: `operator new(0x104)`, `CKorgPath::CKorgPath
 * (char const*)` base ctor, then UNCONDITIONALLY overwrites the vtable
 * pointer with CKorgLinuxPath's own vtable (0x08f79dc8, the SAME literal
 * CKorgLinuxPath's own ctors use) -- i.e. on this Linux build, `Make()`
 * always produces a CKorgLinuxPath, confirmed directly rather than assumed.
 */

#ifndef KORG_PATH_H
#define KORG_PATH_H

class CKorgPath {
public:
	/* .text+0x089d2680, 69 bytes. NULL name -> mFileName[0]=0. Otherwise
	 * strncpy(mFileName, name, 0x100), NUL-terminated at mFileName[0xff]. */
	CKorgPath(const char *name);

	/* .text+0x089d26d0. Same "copy raw path, bounded" shape as the
	 * ctor above, sourced from other->mFileName. Ground truth's own
	 * null-check compiles to `(other+4)!=0` (an address-of-member
	 * proxy for `other!=NULL` that's exact for every real, non-NULL
	 * caller) -- modeled here as the intended `other != 0` check. */
	CKorgPath(const CKorgPath *other);

	/* .text+0x089d25c0 (D1) / 0x089d25d0 (D0, additionally frees `this`).
	 * Real body only resets the vtable pointer -- an ordinary empty
	 * virtual dtor is faithful, same convention used project-wide. */
	virtual ~CKorgPath();

	/* .text+0x089d2b40. True iff `name` has a '.'-extension that
	 * case-insensitively matches `ext`. */
	static int HasExtension(const char *name, const char *ext);

	/* .text+0x089d2b80. Appends `ext` to `name` in place, bounded to
	 * maxLen total bytes including the NUL. */
	static void AddExtension(char *name, unsigned int maxLen, const char *ext);

	/* .text+0x089d2bd0. Truncates `name` at its last '.', if any.
	 * Returns true iff one was found. */
	static int RemoveExtension(char *name);

	/* .text+0x089d2c00. Strips the extension from `name` IN PLACE
	 * (truncates at the last '.'); if one was found, the extension
	 * itself (including the leading '.') is copied into `dest`
	 * (bounded to maxLen, `dest[0]=0` first regardless). Returns true
	 * iff an extension was found and stripped. NOTE: unlike CKorgFile's
	 * own same-named 3-arg overload (which copies the NAME minus its
	 * extension into dest), this one copies the EXTENSION itself into
	 * dest -- confirmed directly from `strncpy(dest, strrchr(name,'.'),
	 * maxLen)`, not assumed from the sibling class's shape. */
	static int RemoveExtension(char *name, char *dest, unsigned int maxLen);

	/* .text+0x089d2c70. True iff `ext` is non-NULL and begins with '.'. */
	static int ValidExtension(const char *ext);

	/* .text+0x089d2c90. In-place filter: drops '-', '_', and any byte
	 * <= 0x20 (space/control chars) from `name`; every other byte is
	 * kept as-is, compacted. */
	static void Sanitize(char *name);

	/* .text+0x089d2cd0. True iff `name` contains no lowercase a-z byte
	 * before its NUL (checked via unsigned `(byte-'a')<=0x19`). */
	static int Capitalized(const char *name);

	/* .text+0x089d32e0. Real cross-platform factory -- see header
	 * comment. Always heap-allocates a CKorgLinuxPath on this build;
	 * caller owns the result (delete via virtual dtor). */
	static CKorgPath *Make(const char *path);

	/* .text+0x089d2720. Builds `this->mFileName = base->mFileName +
	 * base->Separator() + name` (each piece bounded to the 0x100
	 * buffer). Dispatches Separator() through THIS object's own
	 * vtable, not `base`'s -- confirmed directly from the disassembly's
	 * receiver register. */
	void Set(const CKorgPath *base, const char *name);

	/* .text+0x089d27d0. Plain bounded copy of mFileName into dest --
	 * NOT separator-aware (contrast GetOposPath below, which is). */
	void GetPath(char *dest, unsigned int maxLen) const;

	/* .text+0x089d2810. NULL -> mFileName[0]=0. Otherwise plain bounded
	 * strncpy(mFileName, path, 0x100), no extension logic (contrast
	 * CKorgFile::SetPath, which has some). */
	void SetPath(const char *path);

	/* .text+0x089d2860. strrchr(mFileName, this->Separator()); returns
	 * the char right after it, or mFileName itself if not found. */
	const char *GetPathName() const;

	/* .text+0x089d28a0. strrchr(mFileName, '.'). Plain, non-virtual --
	 * NOT separator-aware. */
	const char *GetPathExtension() const;

	/* .text+0x089d28c0. Copies the part of mFileName after its own last
	 * Separator() (or all of it if none) into dest (bounded), then
	 * truncates dest at ITS OWN last '.' if present. Returns true iff
	 * an extension was found and stripped. */
	int GetPathNameNoExtension(char *dest, unsigned int maxLen) const;

	/* .text+0x089d2950. Real return type is CKorgPath* (NOT a string
	 * buffer -- contrast CKorgFile::GetFolder's own char-pointer/int shape):
	 * this->Copy()'s clone, truncated at ITS OWN last Separator() (i.e.
	 * "the containing folder, as a new path object"). Caller owns the
	 * result. */
	CKorgPath *GetFolder() const;

	/* .text+0x089d29a0. Ground truth builds a Copy()'d clone, appends
	 * this->Separator() + folder onto the clone's (own-Separator-
	 * truncated) path, but the clone is never freed OR returned --
	 * every path through this real function ends in `xor eax,eax; ret`
	 * (.text+0x089d2a42), a genuine ground-truth dead-computation/leak,
	 * not ours to fix. Reproduced here as a no-op that always returns 0,
	 * since the mutated clone has zero observable effect on any caller
	 * (nothing ever reads it) -- building-then-discarding it would only
	 * add a pointless real allocation to this reconstruction. */
	int MakePathFromFolder(const char *folder) const;

	/* .text+0x089d2a50. If a file literally exists at `other.mFileName`
	 * (checked via `fopen(...,"r")`+`fclose`), returns `other.Copy()`.
	 * Otherwise: takes the part of `other`'s name after ITS OWN last
	 * separator (or all of it) as the search target, clones `this` and
	 * truncates the clone at the clone's own last separator (making it
	 * a directory-only path), then returns
	 * `this->FindRecurse(searchTarget, clone)` (deletes the clone
	 * afterward). Caller owns the returned pointer (may be NULL if
	 * FindRecurse found nothing). */
	CKorgPath *Find(const CKorgPath &other) const;

	/* .text+0x089d25f0. REAL default body (base is not pure here) --
	 * plain bounded strncpy of mFileName, no conversion. Overridden by
	 * CKorgLinuxPath with real OPOS<->Linux translation. */
	virtual void GetOposPath(char *dest, unsigned int maxLen);

	/* .text+0x089d2630. Same "plain copy" default shape as GetOposPath. */
	virtual void SetOposPath(const char *path);

	/* PURE virtual (vtable slot 2) -- every real override returns a
	 * heap-allocated clone of `*this` (caller owns it). */
	virtual CKorgPath *Copy() const = 0;

	/* PURE virtual (vtable slot 3) -- the platform's path separator
	 * character ('/' on CKorgLinuxPath). */
	virtual char Separator() const = 0;

	/* PURE virtual (vtable slot 6). */
	virtual CKorgPath *TemporaryFileUsingExtension(const char *ext) const = 0;

	/* PURE virtual (vtable slot 7). Recursively searches the directory
	 * tree rooted at `dir` for a regular file named exactly `name`
	 * (skipping hidden/dot entries), returning a newly-allocated path
	 * object for the first match found, or NULL. */
	virtual CKorgPath *FindRecurse(const char *name, const CKorgPath *dir) const = 0;

protected:
	char mFileName[0x100];
};

#endif /* KORG_PATH_H */
