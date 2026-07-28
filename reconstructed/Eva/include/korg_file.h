/*
 * korg_file.h  -  CKorgFile, the abstract base for Eva's Korg-native file
 * format loaders (KMP/KSC/KSF multisample bank formats).
 *
 * FOUND 2026-07-28, fresh `nm -C` class-inventory sweep for the next dense,
 * previously-100%-untouched, well-defined cluster (continuing the
 * CFileIoBase/CStorageConverterBase/CPartitionData "shared root + siblings"
 * line -- see PROJECT_BRAIN/status.md). Several higher-count candidates from
 * the same sweep were traced and REJECTED before landing here:
 *
 *   - CDirectoryElem (30 methods, sorted directory-index cache: QuickSort/
 *     BinarySearchToModify/Compare/Exchange/GetPivot over CDirEntry records)
 *     -- a full `objdump -d -C` call-xref sweep of its whole .text range
 *     (0x08128720..0x0812d360) found it pulls in the ENTIRE CDirEntry
 *     accessor surface `dir_entry.h` already deliberately left unimplemented
 *     ("no caller anywhere in this reconstruction's own traced call graph"),
 *     plus `CShortDirEntry::SetLongNameAndExt`/`FreeLongNameAndExt`, which
 *     `seq_pattern_data.h`'s own header comment already documents as calling
 *     the project-wide out-of-scope growable `CZ` container. Same trap,
 *     correctly rejected.
 *   - CFileKge (38 methods, Korg "GE" bank load/save) -- every method touches
 *     `CFMBrowseForm`/`CChunkHeaderBase`/`CStorageBankHeader`, i.e. the same
 *     UI-form/chunk-subsystem god-object network `seq_pattern_data.h` already
 *     rejected the `CLoadKontaktBankMgr` family for. Rejected.
 *   - CAsyn/CSubBuff (40/35 methods) -- both ALREADY explicitly excluded by
 *     `stream_family.h`'s own header comment ("real, larger CStream-family
 *     leaves... real-file/SysEx-backed... NOT pulled in"). Re-confirmed via a
 *     fresh xref (CAsyn forwards into the 225-method `CFMApiInstance` god
 *     object) rather than re-litigated from scratch.
 *
 * CKorgFile ITSELF, by contrast, is a clean, self-contained value/utility
 * class: `objdump -d -C --start-address=0x089c94a0 --stop-address=0x089ca550`
 * (its entire 33-method, ctor/dtor-inclusive .text range) shows ZERO calls
 * into any other Eva class -- only libc (`calloc`/`fopen`/`fclose`/`fread`/
 * `fseek`/`fwrite`/`free`/`strcasecmp`/`strlen`/`strncat`/`strncpy`/
 * `strrchr`). Pure filename/path/extension string manipulation plus a thin
 * stdio wrapper (mFile).
 *
 * SHARED ROOT + SIBLINGS, confirmed via `nm -C "vtable for ..."` +
 * `typeinfo for ...`: CKorgFile is a real polymorphic base (14-slot vtable,
 * `.rodata+0x08f79aa0`) with 3 concrete siblings sharing its vtable shape --
 * `CKorgFileKMP` (5 methods), `CKorgFileKSC` (11 methods), `CKorgFileKSF`
 * (5 methods). All 3 siblings' own methods (`ImportToBank`/`LoadChunk`/etc)
 * take a `CSTGMultisampleBank*` -- the SAME real, entirely unmodeled "MOSS
 * algorithm voice-model database" class hierarchy `stg_unsol_msg_handler.h`
 * already flags out of scope project-wide (its own header comment: "a real,
 * currently entirely unmodeled class hierarchy... IS out of scope for this
 * pass"). Same convention as `file_io_base.h`'s own treatment of
 * `CDDriverIO`/`CFilesys`/`CDiskUtil`: this header claims only the clean base
 * class; the 3 concrete siblings are deliberately deferred, not fabricated.
 *
 * VTABLE SHAPE (from a direct `.rodata` byte dump at 0x08f79aa0, confirmed
 * slot-for-slot against each method's own real address):
 *   slot 0: ~CKorgFile() D1 (complete-object)
 *   slot 1: ~CKorgFile() D0 (deleting)
 *   slot 2: Read()
 *   slot 3: Write()
 *   slot 4: TransferFromBegin(unsigned int)
 *   slot 5: TransferToBegin(unsigned int)
 *   slot 6: TransferFrom(void*, unsigned int, unsigned int)
 *   slot 7: TransferTo(void const*, unsigned int, unsigned int)
 *   slot 8: TransferFromEnd()
 *   slot 9: TransferToEnd()
 *   slot 10: SetPath(char const*)
 *   slot 11: ImportToBank(...) -- __cxa_pure_virtual in the base
 *   slot 12: LoadChunk(...) -- __cxa_pure_virtual in the base
 * Every other CKorgFile method (AddExtension/Capitalized/ExtractName/
 * GetFolder/GetPathName.../HasExtension/MakeName.../MakeFileName/
 * MakePathFromFolder/NameLength/RemoveExtension/Sanitize/ValidExtension/
 * WriteEmptyFile) is a plain non-virtual member -- confirmed absent from the
 * vtable dump.
 *
 * OBJECT LAYOUT (from ctor/SetPath/TransferXxx field offsets, all cross-
 * checked against each other -- no single method alone fixes every offset):
 *   +0x000  vtable ptr
 *   +0x004  char mFileName[0x100]   -- the path/filename buffer
 *   +0x104  char mExtension[8]      -- the fixed extension this instance was
 *                                      constructed with (".KMP" etc, always
 *                                      accessed via ctor arg2/SetPath, never
 *                                      independently settable)
 *   +0x10c  FILE *mFile             -- transfer-session file handle, opened
 *                                      by TransferFromBegin/TransferToBegin,
 *                                      closed+NULLed by TransferFromEnd/
 *                                      TransferToEnd
 *   sizeof == 0x110 (272 bytes)
 *
 * fopen() MODE STRINGS (real, transcribed from their own `.rodata` bytes,
 * not guessed -- each is a genuine 1-2 byte C string, confirmed by dumping
 * the surrounding row so as not to misread word-grouped hex):
 *   Write()            fopen(mFileName, "w")
 *   Read()/            fopen(mFileName, "r")
 *   TransferFromBegin
 *   TransferToBegin    fopen(mFileName, "r+")   -- read/write, no truncate
 *
 * MakeNameStereo()'s own real body (.text+0x089ca000, 0x428 bytes) is a
 * GCC-unrolled-by-8 bounded-strnlen/space-trim/space-pad sequence, reproduced
 * here as an equivalent plain-C helper (`BoundedNameLen`) rather than a
 * literal transcription of the unrolled compare chain -- same "plain C loop,
 * not literal unrolled transcription" convention already established
 * project-wide (see e.g. `seq_pattern_data.h`'s own header comment). Observed
 * behavior faithfully preserved: strip a pre-existing "-<ch>" suffix matching
 * the requested channel char, trim trailing spaces, then pad with spaces and
 * affix "-<ch>" in the buffer's last two bytes. MakeNameRight/MakeNameLeft
 * are confirmed thin wrappers (`MakeNameStereo(name, dest, maxLen, 'R'/'L')`,
 * literal immediates 0x52/0x4c at their own real call sites).
 */

#ifndef KORG_FILE_H
#define KORG_FILE_H

#include <cstdio>

class CKorgFile {
public:
	/* .text+0x089c97a0. Sets mFile=NULL, copies `ext` into mExtension
	 * (8-byte fixed buffer), then if `name` is non-NULL copies it into
	 * mFileName and conditionally appends mExtension -- identical logic
	 * to SetPath() below, just inlined against the freshly-copied
	 * extension instead of a pre-existing one.
	 */
	CKorgFile(const char *name, const char *ext);

	/* .text+0x089c94a0 (D1) / 0x089c96d0 (D0). Real body only resets the
	 * vtable pointer (D0 additionally free()s `this`) -- an ordinary
	 * empty virtual dtor is faithful, same dual-destructor convention
	 * used project-wide.
	 */
	virtual ~CKorgFile();

	/* .text+0x089c95f0. fopen(mFileName, "r"); if opened, dispatches
	 * through vtable slot 0xb (the derived class's real per-format
	 * loader, `*(*this)[0xb]`) with the FILE* and this, then fclose()s.
	 * Returns the loader's own return value, or -1 if fopen() failed.
	 * The dispatched-through slot is `ImportToBank`-shaped in every
	 * sibling but the exact base signature isn't independently fixed by
	 * this method alone -- modeled here via the pure-virtual declared
	 * below, not re-declared as a second unrelated virtual.
	 */
	virtual int Read();

	/* .text+0x089c9590. fopen(mFileName, "w"); if opened, dispatches
	 * through vtable slot 0xc (`LoadChunk`-shaped in every sibling),
	 * then fclose()s. Same return-value convention as Read().
	 */
	virtual int Write();

	/* .text+0x089c9690. fopen(mFileName, "r"), fseek(0, SEEK_SET). Opens
	 * a transfer session for reading; result stored in mFile.
	 */
	virtual void TransferFromBegin(unsigned int startOffset);

	/* .text+0x089c9650. fopen(mFileName, "r+"), fseek(0, SEEK_SET). Opens
	 * a transfer session for read/write (no truncate); result in mFile.
	 */
	virtual void TransferToBegin(unsigned int startOffset);

	/* .text+0x089c9550. fread(buf, size, count, mFile). Always returns 0
	 * (real body discards fread()'s own return value) -- same "sentinel,
	 * not passthrough" shape as CFileIoBase's own fread()/fwrite() stubs.
	 */
	virtual int TransferFrom(void *buf, unsigned int size, unsigned int count);

	/* .text+0x089c94b0. fwrite(buf, size, count, mFile). Same always-0
	 * return shape as TransferFrom().
	 */
	virtual int TransferTo(const void *buf, unsigned int size, unsigned int count);

	/* .text+0x089c9520. fclose(mFile); mFile = NULL. */
	virtual void TransferFromEnd();

	/* .text+0x089c94f0. Identical body to TransferFromEnd() (own separate
	 * function in ground truth, not a shared thunk -- transcribed as such).
	 */
	virtual void TransferToEnd();

	/* .text+0x089c96f0. NULL path -> mFileName[0] = 0. Otherwise copies
	 * `path` into mFileName (truncated to 0xff chars + NUL), then if
	 * mFileName's own last '.'-extension doesn't case-insensitively match
	 * mExtension, appends mExtension.
	 */
	virtual void SetPath(const char *path);

	/* Pure virtual -- every real .KMP/.KSC/.KSF sibling overrides this
	 * vtable slot with its own `ImportToBank(CSTGMultisampleBank*, ...)`,
	 * but each sibling's own real signature differs (CKorgFileKSF takes
	 * `(CSTGMultisampleBank*, bool)`, CKorgFileKSC takes
	 * `(CSTGMultisampleBank*, bool, bool)`, CKorgFileKMP takes
	 * `(CSTGMultisampleBank*, unsigned long*, bool)`) -- meaning only ONE
	 * of those three is the actual vtable override matching this base
	 * slot's real signature and the other two are sibling-local
	 * overloads, not resolved here since all 3 siblings are deliberately
	 * out of scope (see file header). Declared with a placeholder
	 * signature purely so CKorgFile stays a well-formed abstract base;
	 * never called by any reconstructed code.
	 */
	virtual int ImportToBank() = 0;

	/* Pure virtual -- same "exact override signature not needed, siblings
	 * deferred" situation as ImportToBank() above (real siblings:
	 * `LoadChunk(CFileStream&, CFileChunkHeader&)`).
	 */
	virtual int LoadChunk() = 0;

	/* .text+0x089c9880. strrchr(mFileName, '/'); returns the char right
	 * after it, or mFileName itself if no '/' found.
	 */
	const char *GetPathName() const;

	/* .text+0x089c98b0. Same "name after last '/'" extraction as
	 * GetPathName(), copied into `dest` (truncated to maxLen), then
	 * truncated again at its own last '.' if present.
	 */
	void GetPathNameNoExtension(char *dest, unsigned int maxLen) const;

	/* .text+0x089c9930. Copies mFileName into `dest` (truncated to
	 * maxLen), truncates at the last '.' if found. Returns true iff a
	 * '.' was found (matching ground truth's own boolean return, despite
	 * the name -- transcribed faithfully, not renamed to match assumed
	 * intent).
	 */
	int GetFolder(char *dest, unsigned int maxLen);

	/* .text+0x089c9990. Copies mFileName into `dest` (truncated to
	 * maxLen), strips it at the last '.' if present (recording whether
	 * one was found), appends "/", then appends `folder`. Returns true
	 * iff mFileName had an extension to strip.
	 */
	int MakePathFromFolder(char *dest, const char *folder, unsigned int maxLen);

	/* .text+0x089c9a50. True iff `name` has a '.'-extension that
	 * case-insensitively matches `ext`.
	 */
	static int HasExtension(const char *name, const char *ext);

	/* .text+0x089c9a90. Appends `ext` to `name` (in place), bounded so the
	 * result never exceeds `maxLen` total bytes including the NUL.
	 */
	static void AddExtension(char *name, unsigned int maxLen, const char *ext);

	/* .text+0x089c9ae0. Truncates `name` at its last '.', if any. Returns
	 * true iff one was found.
	 */
	static int RemoveExtension(char *name);

	/* .text+0x089c9b10. Copies `name` into `dest` (truncated to maxLen)
	 * with any '.'-extension stripped, and separately truncates `name`
	 * itself at its own last '.'. Returns true iff `name` had an
	 * extension. Mutates `name` itself in place (matches ground truth's own
	 * `char*`, not `const char*`, first-parameter type).
	 */
	static int RemoveExtension(char *name, char *dest, unsigned int maxLen);

	/* .text+0x089c9b80. True iff `ext` is non-NULL and begins with '.'. */
	static int ValidExtension(const char *ext);

	/* .text+0x089c9ba0. Copies the part of `path` after its last '/' (or
	 * all of `path` if none) into `dest` (truncated to maxLen), then
	 * strips `dest`'s own last '.'-extension if present. Returns true iff
	 * an extension was found and stripped.
	 */
	static int ExtractName(const char *path, char *dest, unsigned int maxLen);

	/* .text+0x089c9c20. In-place filter: drops '-', space, and any byte
	 * <= 0x20 from `name`; every other byte is kept as-is.
	 */
	static void Sanitize(char *name);

	/* .text+0x089c9c60. True iff every alphabetic byte in `name` (checked
	 * via `(byte - 'a') <= 0x19` i.e. lowercase a-z) is absent -- i.e.
	 * true iff `name` contains no lowercase letters (vacuously true for a
	 * string with none, matching ground truth's own "already capitalized"
	 * check idiom).
	 */
	static int Capitalized(const char *name);

	/* .text+0x089c9c90. Writes `count` zero bytes to `file` in <=4096-byte
	 * chunks via a 4096-byte calloc'd scratch buffer, freed at the end.
	 * `startOffset` shifts where the chunk-boundary accounting begins
	 * (matches ground truth's own `ebp` running-offset var) but every
	 * byte written is always 0.
	 */
	static void WriteEmptyFile(FILE *file, unsigned int startOffset, unsigned int count);

	/* .text+0x089c9da0. Bounded strlen: length of `name` up to `maxLen`
	 * (returns `maxLen` if no NUL found first). Ground truth is a GCC
	 * 8-unrolled compare chain; behaviorally equivalent to `strnlen`.
	 */
	static unsigned int NameLength(const char *name, unsigned int maxLen);

	/* .text+0x089c9e90. strncpy(dest, name, maxLen), then trims trailing
	 * spaces from the bounded-length copy (nulling them in place).
	 */
	static void MakeName(const char *name, char *dest, unsigned int maxLen);

	/* .text+0x089ca000. strncpy(dest, name, maxLen). If the bounded-length
	 * copy already ends in "-<ch>" (matching the requested channel char),
	 * that suffix is stripped first. Trailing spaces are then trimmed,
	 * the remainder is space-padded out to maxLen-2, and the buffer's
	 * last two bytes are forced to '-' and `ch`. See file header for the
	 * "plain C helper, not literal unrolled transcription" note.
	 */
	static void MakeNameStereo(const char *name, char *dest, unsigned int maxLen, char ch);

	/* .text+0x089ca470. MakeNameStereo(name, dest, maxLen, 'R'). */
	static void MakeNameRight(const char *name, char *dest, unsigned int maxLen);

	/* .text+0x089ca4a0. MakeNameStereo(name, dest, maxLen, 'L'). */
	static void MakeNameLeft(const char *name, char *dest, unsigned int maxLen);

	/* .text+0x089ca4d0. If `strlen(name) - strlen(ext) <= maxLen`, appends
	 * `ext` to `name` directly; otherwise first truncates `name` at
	 * offset `strlen(name)-strlen(ext)` before appending. Either way the
	 * result is hard NUL-terminated at `name[maxLen-1]`.
	 */
	static void MakeFileName(char *name, unsigned int maxLen, const char *ext);

private:
	char mFileName[0x100];
	char mExtension[8];
	FILE *mFile;
};

#endif /* KORG_FILE_H */
