// SPDX-License-Identifier: GPL-2.0
/*
 * sysex_objects.h  -  the CSysExSong/CSysExDrumKit/CSysExCombi/
 * CSysExWaveSeq/CSysExSetList/CSysExSongTimbreSet family: the main
 * SysEx-transferable object classes (siblings of the smaller
 * CSysEx*Name/CSysExSetListSlotComment family in sysex_object_names.h,
 * found the same round).
 *
 * FOUND 2026-07-29 (round 40, solo -- session-wide 200-subagent dispatch
 * cap hit, see PROJECT_BRAIN/status.md). Confirmed via
 * /home/share/Decomp/EVA_Decomp/eva_export's own per-function decompiles.
 *
 * `CSysExSong`/`CSysExDrumKit`/`CSysExCombi`/`CSysExWaveSeq`/
 * `CSysExSetList` share an IDENTICAL 6-method shape:
 *   GetStorageId()                 -- per-class literal (0x25/3/2/4/0x1b)
 *   HasDigests()                   -- always literal 1
 *   GetVersion()                   -- per-class literal (3/3/3/1/0)
 *   GetObjectSize()/
 *     GetObjectSizeForExport()     -- identical per-class literal size
 *                                    (0x3314/0x9618/0x1e82/0x8a8/0x10f28)
 *   GetNumObjectsForDigest(int)    -- CSysExSong's OWN version is a
 *                                    trivial literal (200); the other 4
 *                                    classes' own real bodies are a
 *                                    genuinely UNRESOLVABLE indirect call
 *                                    (`(**(code**)(*(int*)param_1+0x38))()`
 *                                    -- Ghidra's own decompile even flags
 *                                    "Could not recover jumptable, too
 *                                    many branches") through an
 *                                    unconfirmed vtable slot on a
 *                                    caller-supplied `param_1` object
 *                                    (NOT `this` -- real calling
 *                                    convention is `__cdecl`, no
 *                                    implicit `this`) -- deliberately
 *                                    NOT reconstructed for those 4, real
 *                                    unresolved externs declared instead
 *                                    of a guess.
 *
 * `CSysExSongTimbreSet` is a smaller 4-method sibling (no
 * `HasDigests`/`GetNumObjectsForDigest`): `GetStorageId()`=0xd,
 * `GetVersion()`=3, `GetObjectSize()`/`GetObjectSizeForExport()`=0x1e82
 * (same literal as `CSysExCombi`'s own size -- a timbre set is
 * combi-program-shaped, consistent).
 *
 * Same non-polymorphic-in-this-project's-model convention as
 * `sysex_object_names.h` for the constant accessors and dtors -- see
 * that header's own note for the full "no per-class vtable object
 * exists, D0's free(this) not reproduced" rationale (independently
 * re-confirmed for this family too: no `PTR__CSysExSong`/`PTR__CSysEx
 * DrumKit`/etc symbol exists in ground truth, only the shared
 * `PTR__CSysExObjectBase`).
 */

#ifndef SYSEX_OBJECTS_H
#define SYSEX_OBJECTS_H

/*
 * GetNumObjectsForDigest's own real per-instance dispatch target is
 * unconfirmed (see header comment above) -- declared as a genuinely
 * unresolved extern per class, matching this project's established
 * treatment for a confirmed-real, not-yet-reconstructable callee.
 * Real mangled names would each be distinct (member functions), so
 * these are declared as free functions taking the same `param_1`
 * ground truth itself passes, not as class members, to avoid
 * fabricating a class relationship that isn't confirmed.
 */
extern "C" {
unsigned int CSysExDrumKit_GetNumObjectsForDigest_Unresolved(int param1);
unsigned int CSysExCombi_GetNumObjectsForDigest_Unresolved(int param1);
unsigned int CSysExWaveSeq_GetNumObjectsForDigest_Unresolved(int param1);
unsigned int CSysExSetList_GetNumObjectsForDigest_Unresolved(int param1);
}

class CSysExSong {
public:
	~CSysExSong() {}
	unsigned int GetStorageId() const;
	unsigned int HasDigests() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
	unsigned int GetNumObjectsForDigest(int param1) const;
};

class CSysExDrumKit {
public:
	~CSysExDrumKit() {}
	unsigned int GetStorageId() const;
	unsigned int HasDigests() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
	/* GetNumObjectsForDigest(int) deliberately not declared as a member
	 * here -- see header comment; call
	 * CSysExDrumKit_GetNumObjectsForDigest_Unresolved() directly if
	 * ever needed. */
};

class CSysExCombi {
public:
	~CSysExCombi() {}
	unsigned int GetStorageId() const;
	unsigned int HasDigests() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
};

class CSysExWaveSeq {
public:
	~CSysExWaveSeq() {}
	unsigned int GetStorageId() const;
	unsigned int HasDigests() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
};

class CSysExSetList {
public:
	~CSysExSetList() {}
	unsigned int GetStorageId() const;
	unsigned int HasDigests() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
};

class CSysExSongTimbreSet {
public:
	~CSysExSongTimbreSet() {}
	unsigned int GetStorageId() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
};

#endif // SYSEX_OBJECTS_H
