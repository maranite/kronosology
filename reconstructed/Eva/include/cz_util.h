/*
 * cz_util.h  -  ONE static, self-contained method off the real `CZ` class: not the
 * "CZ" string-set CONTAINER (that's a genuinely separate, 247-method dependency
 * config_manager.h already flags as out of scope project-wide -- see
 * CConfigManager::CreateResourceFamilies()'s own header comment). `CZ::StrCmpIgnoreCase`
 * takes two plain `const char*` (no `CZ` instance involved at all, `cc=__cdecl`,
 * confirmed from StrCmpIgnoreCase@080be680.c) so it's reconstructable in complete
 * isolation from the container class.
 *
 * Needed by CFileMan::CFileMan() (file_man.cpp, Stage 6 breadth sweep, 2026-07-25) to
 * check a config string against the literal "Enabled".
 *
 * A future pass that takes on the full `CZ` container should fold this method's real
 * body into that reconstruction rather than duplicating it -- this header exists only
 * so file_man.cpp isn't blocked on that unrelated, much larger effort.
 *
 * UPDATE (Eva "size is not depth" re-check batch, 2026-07-26): added an opaque
 * instance ctor/dtor (`CZ(unsigned)`/`~CZ()`) so classes that embed a `CZ` MEMBER by
 * value -- `CRMJob` (rm_job.h, 2 members), `CDirEntry` (dir_entry.h, 4 members),
 * `CBatchDiskMainTask` (batch_disk_main_task.h, 1 member) -- can be placement-correct
 * without pulling in the real 247-method container's internals. Real sizeof(CZ) is
 * confirmed as exactly 0x10 (16) bytes via THREE independent cross-checks of
 * consecutive embedded-member offset spacing in ground truth: `CRMJob::CRMJob()`
 * (.text+0x081660d0: CZ members at +8 and +0x18, next field at +0x28), `CDirEntry::
 * CDirEntry()` (.text+0x08071640: CZ members at +4/+0x14/+0x24/+0x34, next field at
 * +0x44), and `CBatchDiskMainTask::CBatchDiskMainTask()` (.text+0x08241920: CZ member
 * at +0x14c, next field at +0x15c) -- all three agree on a 0x10-byte stride. The
 * real ctor (mangled `_ZN2CZC1Ej`, capacity argument observed as literal `1` at every
 * call site found so far) and dtor (`_ZN2CZD1Ev`) both recur BYTE-IDENTICALLY at
 * several different .text addresses across ground truth (e.g. dtor bodies at both
 * 0x08071520 and 0x08166090) -- strong evidence `CZ`'s ctor/dtor are themselves
 * small inline functions duplicated per translation unit, not that this project has
 * somehow found 2 different classes named CZ. The real dtor's own inlined body (seen
 * directly inside `CRMJob`/`CDirEntry`'s OWN non-exceptional cleanup paths, not as a
 * separate call) is a conditional `free()`/`delete[]` on what looks like a
 * heap-buffer pointer + flag pair -- i.e. `CZ` is plausibly a small-buffer-optimized
 * string/array type. Deliberately NOT modeled here: reverse-engineering that layout
 * further would mean reconstructing real container behavior, exactly what this
 * class's own established "stays out of scope project-wide" status
 * (config_manager.h's `CreateResourceFamilies()`) says not to do. `mOpaque` is never
 * read or written by this stub -- any real caller that actually needs `CZ`'s string
 * contents is, by definition, not yet in scope.
 */

#ifndef CZ_UTIL_H
#define CZ_UTIL_H

class CZ {
public:
	/* .text+0x080be680, 173 bytes (mangled: _ZN2CZ16StrCmpIgnoreCaseEPKcS1_). Real
	 * body is a hand-unrolled NULL-aware case-insensitive 3-way compare (ASCII-only
	 * `toupper`, matching every non-negative byte -- i.e. values < 0x80 only, same
	 * "top-bit-set bytes never re-cased" convention used elsewhere in this project's
	 * string routines). Returns 0 if equal (or both NULL), 0xffffffff if `a` sorts
	 * before `b` (or only `a` is NULL), 1 if `a` sorts after `b` (or only `b` is
	 * NULL, or `a` is longer than `b`) -- NOT a plain negative/zero/positive tristate,
	 * matches the real decompile's own literal return constants.
	 */
	static unsigned StrCmpIgnoreCase(const char *a, const char *b);

	/* Opaque instance ctor/dtor -- see header comment. `capacity` is passed through
	 * and discarded (every ground-truth call site found so far passes the literal
	 * `1`; real meaning not decoded).
	 */
	explicit CZ(unsigned capacity) { (void)capacity; }
	~CZ() {}

private:
	unsigned char mOpaque[0x10]; /* real sizeof(CZ) -- see header comment. Never
	                               * touched; reserved purely for correct placement
	                               * of any embedding class's OWN later members. */
};

#endif /* CZ_UTIL_H */
