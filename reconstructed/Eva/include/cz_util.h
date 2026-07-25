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
};

#endif /* CZ_UTIL_H */
