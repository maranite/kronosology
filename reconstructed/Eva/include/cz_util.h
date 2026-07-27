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
 * at +0x14c, next field at +0x15c) -- all three agree on a 0x10-byte stride.
 *
 * UPDATE (Eva deferred-registry re-trace, 2026-07-27): `CZ(unsigned)`/`~CZ()` are now
 * their REAL bodies, not stubs -- a fresh full-binary symbol sweep (`nm -C -S`) found
 * the real `CZ` container's method surface is 59 distinctly-named methods, not the
 * 247 raw symbol count (that count includes the SAME small methods -- especially
 * `~CZ()`, duplicated at ~90 different .text addresses -- re-emitted once per
 * translation unit; `objdump -dr -M intel` of the real, single-caller-visible
 * out-of-line `~CZ()` (.text+0x08185c00, the only `W`eak-linkage copy, confirmed via
 * `nm`) is unambiguous, see below). Given that, `CZ::CZ(unsigned int)`
 * (`_ZN2CZC1Ej`, .text+0x080ba5f0, 80 bytes) and `CZ::~CZ()` turned out to be a
 * genuinely tractable, self-contained sub-piece -- same "size is not depth" lens
 * already validated on `CJobStack`/`CLimiterBase`/`CKGMsgProcessor` -- **without**
 * needing any of `Insert`/`RFind`/`Remove`/`Sprintf`/the other 55 real container
 * methods. Real ctor body: `malloc(max(capacity,1))` via `_Znaj` (`operator new[]`),
 * null-terminate byte 0, then `field+0=ptr` (RawPtrField), `field+4=capacity`,
 * `field+8=0` (RawFlagField -- see below), `field+0xc=0`. Real dtor body (canonical
 * out-of-line copy, .text+0x08185c00): `if (field+0xc == 0 && field+0 != 0)
 * operator delete[](field+0);` -- i.e. `CZ` frees its OWN buffer unless some other
 * party (never observed in this project's traced call graph) has set field+0xc
 * nonzero. This project's own dtor uses plain `free()` for symmetry with the ctor's
 * `malloc()` (matching this project's established "malloc/free, not new[]/delete[]"
 * placement convention, e.g. mains.cpp's every `MMainXxx` `Create()` wrapper) rather
 * than reproducing ground truth's real `_Znaj`/`_ZdaPv` mismatch with its OWN
 * dtor's plain `free()` (ground truth itself is inconsistent here across different
 * inlined copies -- some call `free()`, the canonical out-of-line one calls
 * `operator delete[]` -- functionally identical in every C++ runtime this project
 * targets, so not worth preserving verbatim).
 *
 * IMPORTANT CORRECTION this same pass: an earlier draft of this comment (now fixed)
 * mis-transcribed the dtor's OWN free-gate field as "+8" in its prose while showing
 * the correct `[ebx+0xc]` instruction right next to it -- a wording bug, not a code
 * bug (the code below, and `CDirEntry`'s own real ctor -- dir_entry.h -- always used
 * the correct offsets). Fresh disassembly of BOTH the canonical out-of-line dtor
 * (.text+0x08185c00, takes `this` as a normal pointer arg) and `CDirEntry`'s own
 * ctor (which explicitly re-zeroes each embedded CZ's own field+8 right after
 * calling `CZ(1)`, and separately confirmed `HasValidLongNameExt()`'s real body --
 * .text+0x08071500 -- reads exactly this SAME field+8 of `mLongName`/`mLongExt`)
 * together nail down: field+0xc is the dtor's "do I own this buffer" gate (a CZ
 * ctor always sets it 0, so a default/opaque-capacity CZ always frees its buffer);
 * field+8 is a SEPARATE "current string length" field (0 for an empty string,
 * consumed by `HasValidLongNameExt()`'s own real "AnyLongFieldPopulated" check) --
 * `RawFlagField()` below correctly reads field+8, unchanged.
 *
 * REAL-BUG FIX this same pass: because the ctor now genuinely allocates (instead of
 * leaving `mOpaque` all-zero), `CDirEntry::GetName()`/`GetExt()` (dir_entry.h/.cpp)
 * on a freshly-constructed, never-populated `CDirEntry` now correctly return a
 * valid, non-NULL pointer to a heap-allocated empty string (`""`) -- matching ground
 * truth's real behavior -- instead of the previous reconstruction's NULL (a real,
 * previously-undetected divergence: the old opaque ctor never allocated anything, so
 * `RawPtrField()` was always 0 and `GetName()`/`GetExt()` always returned NULL for
 * every `CDirEntry`, forever, even after real directory-scan population code -- not
 * yet reconstructed here -- would eventually fill them in on real hardware).
 * `verify/test_dir_entry.cpp`'s own check [1] updated accordingly (was asserting the
 * bug's own NULL result as if it were correct ground-truth behavior).
 *
 * UPDATE (CBatchDiskMainTask::PreloadDir() investigation, 2026-07-26): added two
 * tiny raw-offset PEEK accessors (`RawPtrField()`/`RawFlagField()`), not full
 * container methods. Ground truth's own `CDirEntry::GetName()`/`GetExt()`/
 * `HasValidLongNameExt()` (dir_entry.h/.cpp) read exactly 2 raw dword fields out of
 * an embedded `CZ`'s own opaque buffer -- offset 0 (used as a `const char*` -- the
 * buffer pointer, now confirmed real/allocated, not a small-buffer-optimization
 * inline case -- see the 2026-07-27 UPDATE above) and offset 8 (the "current string
 * length" field, also confirmed above). Reading these two named offsets is NOT the
 * same as implementing `Insert`/`RFind`/`Remove`/the real `CZ(const char*,
 * unsigned)`/`CZ(const CZ&, unsigned)` constructors that `PreloadDir()`'s own
 * genuinely deep body needs (batch_disk_main_task.h) -- those still require real
 * container append/search semantics and stay out of scope; only the OPAQUE-capacity
 * ctor (`CZ(unsigned)`, always constructing an empty string) is real now. Per the
 * 2026-07-27 UPDATE above, `RawPtrField()` on a freshly-constructed `CZ` is now a
 * real non-NULL pointer to an empty (`""`) heap buffer, not always 0.
 */

#ifndef CZ_UTIL_H
#define CZ_UTIL_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

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

	/* .text+0x080ba5f0, 80 bytes (_ZN2CZC1Ej). REAL body, not a stub (see 2026-07-27
	 * UPDATE above): `n = capacity ? capacity : 1; buf = malloc(n); buf[0] = 0;
	 * field+0 = buf; field+4 = n; field+8 = 0; field+0xc = 0;`. Every ground-truth
	 * call site found so far passes the literal `1` (an empty string with 1 byte of
	 * capacity for the NUL terminator); real callers requesting a larger initial
	 * capacity are not observed but would work identically.
	 */
	explicit CZ(unsigned capacity)
	{
		unsigned cap = capacity ? capacity : 1;
		char *buf = (char *)malloc(cap);
		buf[0] = '\0';
		uint32_t ptr = (uint32_t)(uintptr_t)buf;
		uint32_t capField = cap;
		uint32_t zero = 0;
		memcpy(mOpaque + 0, &ptr, sizeof ptr);
		memcpy(mOpaque + 4, &capField, sizeof capField);
		memcpy(mOpaque + 8, &zero, sizeof zero);
		memcpy(mOpaque + 0xc, &zero, sizeof zero);
	}

	/* .text+0x08185c00 (canonical out-of-line copy), 45 bytes (_ZN2CZD1Ev). REAL
	 * body: `if (field+0xc == 0 && field+0 != 0) delete[] (char*)field+0;` -- see
	 * 2026-07-27 UPDATE above for the field+0xc-vs-field+8 correction and the
	 * malloc()/free() (not new[]/delete[]) substitution rationale.
	 */
	~CZ()
	{
		uint32_t ownsGate;
		memcpy(&ownsGate, mOpaque + 0xc, sizeof ownsGate);
		if (ownsGate == 0) {
			uint32_t ptr;
			memcpy(&ptr, mOpaque + 0, sizeof ptr);
			if (ptr != 0)
				free((void *)(uintptr_t)ptr);
		}
	}

	/* Raw offset 0 -- see header comment. Returned as `uint32_t`, not `char*`,
	 * deliberately: this is a target-32-bit-wide field inside a byte buffer, not
	 * a real host pointer, and this project's convention (verify/'s own I32/U8
	 * helpers) is to never widen a raw target field through a native pointer
	 * type. Callers that need it as an address (CDirEntry::GetName()/GetExt())
	 * cast explicitly at the point of use.
	 */
	uint32_t RawPtrField() const
	{
		uint32_t v;
		memcpy(&v, mOpaque + 0, sizeof v);
		return v;
	}

	/* Raw offset 8 -- see header comment. */
	uint32_t RawFlagField() const
	{
		uint32_t v;
		memcpy(&v, mOpaque + 8, sizeof v);
		return v;
	}

private:
	unsigned char mOpaque[0x10]; /* real sizeof(CZ) -- see header comment. Written
	                               * by the real ctor/dtor above (offsets 0/4/8/0xc);
	                               * only offsets 0/8 are ever READ outside this
	                               * class (via the 2 accessors above), by
	                               * CDirEntry. */

	friend struct CZTestHooks;
};

#endif /* CZ_UTIL_H */
