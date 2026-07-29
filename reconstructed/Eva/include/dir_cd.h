// SPDX-License-Identifier: GPL-2.0
/*
 * dir_cd.h  -  CDirCD, the Akai/ISO CD-ROM directory driver.
 *
 * FOUND 2026-07-29 (round-38 survey, `.claude/agent-memory/re-decompiler/
 * eva_round38_survey_rejected_candidates_2026-07-29.md`), reconstructed
 * here (solo -- session-wide 200-subagent dispatch cap hit; standing
 * decompile-everything goal continued directly by the main-loop
 * assistant). Real, `nm -C` confirms `.text+0x080d7e10`..`0x080e2280`,
 * `vtable for CDirCD` (`.rodata+0x08e86160`) + `typeinfo for CDirCD`
 * (`.rodata+0x08e862f8`, a real `__si_class_type_info` -- base type
 * pointer at +0x8 resolves to `typeinfo for CDirectory`, confirmed via a
 * direct `.rodata` byte read, not guessed).
 *
 * `CDirCD : public CDirectory` -- `CDirectory` ITSELF is a real,
 * substantial, entirely separate class (own ctor/dtor/Init/OpenDir/
 * CloseDir/ReadEntry/NameSplit/NameGroup/Reset, ~11 methods) that embeds
 * 3 MORE entirely unmodeled classes (`CRecentDirElems` 23 methods,
 * `CRecentFileElems` 12, `CRecentPathElems` 9, all destroyed by
 * `CDirectory::~CDirectory()`) -- NOT reconstructed this pass. This
 * header therefore does NOT model `CDirectory`'s own base-class layout
 * or provide a working ctor/dtor for `CDirCD` -- only the `CDirCD`-level
 * methods below, accessed via raw `this`-pointer offset arithmetic
 * (this project's established convention for classes whose base extent
 * is unconfirmed/out of scope), are real. Constructing/destroying a real
 * `CDirCD` is out of scope this pass; a raw buffer cast (matching several
 * other partial-class precedents, e.g. `CFileKscList`'s own
 * ctor-avoidance in its own KAT) is how `verify/test_dir_cd.cpp` exercises
 * these.
 *
 * 10 of ~40 real methods reconstructed this pass (the smallest,
 * self-contained, non-`CDirEntry`/non-`CRecentXxxElems`-touching ones):
 *   `GetCurrEntry()`, `GetRootHandle()`, `GetClusterSizeInSect()`,
 *   `GetMaxDirEntrySize()`, `GetNumAkaiPartition()`, `SetError()`,
 *   `ResetBufferedEntries()`, `GetTotalSectors()`, `FindPartition()`,
 *   `GetPTRecord()`.
 *
 * Deferred (real, disassembly not fully traced this pass): ctor/dtor,
 * `IsMixedCD()` (a genuine 350-byte two-phase alternating session-table
 * scan, not a simple accessor -- partially traced but not landed),
 * `FindVolume()` (a real BINARY SEARCH over the volume table followed by
 * a linked-list traversal -- more involved than this batch's other
 * lookups), `ReadNextEntry()`/`AppendAkaiPartition()`/`GetAkaiPartition()`/
 * `ChangeAkaiPartition()`/`ClearAkaiPartition()`/`Register()`/
 * `Unregister()`/`Invalidate()`/`MediaOpen()`/`GetOwnerPartition()`/
 * `GetRootSize()`/`FindLastDataSessionOffset()`/`GetMediaLabel()`/
 * `UpdateElemInPathTable()`/`GetMaxMSNum()`/`GetNumOfMultisample()`,
 * `CCDConfigDir::DeserializePTR()` (a SEPARATE class touching
 * `CDirCD::PTRecord`, not attempted).
 *
 * FIELD OFFSETS (raw, from each reconstructed method's own confirmed
 * reads -- no single method alone fixes every one; `CDirectory`'s own
 * unconfirmed extent means these are `CDirCD`-relative absolute offsets,
 * not "offset past the base class"):
 *   +0x28   pointer to a "media geometry/type" sub-object (own layout
 *           unconfirmed beyond +0xa0, see below) -- ctor-set, presumably
 *           the real `CMachineBase*`/media-descriptor this driver wraps.
 *   +0x50   error-code field (`SetError()`'s own target, `int`-sized).
 *   +0xd8   embedded `CDirEntry`-shaped scratch buffer (`GetCurrEntry()`'s
 *           own confirmed return -- own extent unconfirmed, `dir_entry.h`'s
 *           `CDirEntry` itself only has its ctor/dtor + ~10 accessors
 *           reconstructed, none needed here).
 *   +0x144  raw "total sectors" override, read directly (no computation)
 *           when the +0x28 sub-object's own +0xa0 mode byte == 4.
 *   +0x14b  byte, a session/track-table entry COUNT (max real value
 *           unconfirmed -- `IsMixedCD()`, not reconstructed here, clamps
 *           its OWN local copy to 100, but `GetTotalSectors()` uses the
 *           raw byte directly with no clamp of its own).
 *   +0x14c..  8-byte-stride session/track-table array (`GetTotalSectors()`'s
 *           own real body): a real 16-bit value assembled from 2 non-
 *           adjacent bytes 3 apart within each 8-byte record
 *           (`byte[+0]` low, `byte[+3]` high) -- the other 6 bytes of each
 *           record are not independently determined by this pass's own
 *           methods.
 *   +0xc94  base pointer, `PTRecord` array (0x10c = 268-byte stride,
 *           confirmed via `GetPTRecord()`'s own `imul ...,0x10c`).
 *   +0xc98  `PTRecord` array element COUNT (`GetPTRecord()`'s own bounds
 *           check -- real ground truth calls a project-internal assert/
 *           diagnostic API on out-of-range access via a global object's
 *           vtable slot +0x94, `ds:0x930a1f4` -- modeled here as an
 *           opaque, deliberately-inert extern call, same "confirmed real
 *           but genuinely out of scope" convention as this project's
 *           other internal-diagnostics hooks; ground truth's own control
 *           flow falls through and computes+returns the pointer
 *           regardless of whether the assert fired, so no early-return
 *           branch is needed here).
 *   +0xca8/+0xcac  begin/end pointers, `SAkaiPartition` array (0x18 = 24-
 *           byte stride, confirmed via BOTH `FindPartition()`'s own
 *           linear-scan stride AND `GetNumAkaiPartition()`'s own
 *           `(end-begin)>>3` then magic-multiply-by-(1/3) idiom, i.e.
 *           24 = 8*3 -- both cross-check to the same real element size).
 *
 * `SAkaiPartition`/`PTRecord` are declared here with ONLY their real
 * confirmed extent (24 / 0x10c bytes) and, for `SAkaiPartition`, its one
 * confirmed field (`id`, at +0x0, `FindPartition()`'s own compare target)
 * -- same minimal-declared-surface convention as this project's other
 * partially-modeled nested/dependency types.
 */

#ifndef DIR_CD_H
#define DIR_CD_H

#include "dir_entry.h"

class CDirCD {
public:
	struct SAkaiPartition {
		unsigned long id; /* +0x0, confirmed via FindPartition()'s own compare */
		unsigned char _unknown[20]; /* real total extent 24 bytes, rest not determined */
	};
	struct PTRecord {
		unsigned char _unknown[0x10c]; /* real extent only, no fields determined */
	};

	CDirEntry *GetCurrEntry();
	unsigned long GetRootHandle() const;
	unsigned long GetClusterSizeInSect() const;
	unsigned long GetMaxDirEntrySize();
	unsigned long GetNumAkaiPartition() const;
	/* Real ground-truth param type is `EDrvNotify` (not modeled anywhere
	 * in this project yet); represented as plain `int` per this
	 * project's established convention for unconfirmed enum types --
	 * the real values compared against (0,1,2,5) are all confirmed via
	 * disassembly regardless. NOTE: this makes the compiled mangled
	 * name diverge from ground truth's own
	 * (`_ZN6CDirCD8SetErrorE10EDrvNotify`) -- same accepted tradeoff as
	 * this project's other plain-`int`-for-unconfirmed-enum instances. */
	void SetError(int notify);
	void ResetBufferedEntries();
	unsigned long GetTotalSectors() const;
	bool FindPartition(unsigned long id, const SAkaiPartition *&out) const;
	const PTRecord *GetPTRecord(unsigned int index) const;

private:
	CDirCD(); /* real ctor NOT reconstructed this pass -- see header comment */
};

#endif // DIR_CD_H
