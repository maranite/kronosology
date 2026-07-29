// SPDX-License-Identifier: GPL-2.0
/*
 * sysex_control_objects.h  -  CSysExKarmaGEInfo/CSysExSongControl: 2
 * more siblings of the CSysEx.../CSysExObjectBase family (sysex_objects.h/
 * sysex_object_names.h), each with its own extra methods beyond the
 * common GetStorageId/GetVersion/GetObjectSize/GetObjectSizeForExport
 * trio.
 *
 * FOUND 2026-07-29 (round 41, solo -- session-wide 200-subagent dispatch
 * cap hit, see PROJECT_BRAIN/status.md); flagged as a viable follow-up
 * during round 40's survey of the sibling families, picked up here.
 * Confirmed via /home/share/Decomp/EVA_Decomp/eva_export's own
 * per-function decompiles.
 *
 * CSysExKarmaGEInfo (`GetStorageId()`=0x26, `GetVersion()`=0,
 * `GetObjectSize()`/`GetObjectSizeForExport()`=0x7a0, `GetNumBanks()`=1
 * always) has 4 extra small real methods:
 *   GetObjectPointer(int, int) const -- both params genuinely unused;
 *     returns `CKGUtil::sm_poKGUIInfo + 0x3c90` (a real int, ground
 *     truth's own return type -- an address-arithmetic result, not a
 *     dereferenced pointer).
 *   GetSysExBankId(int) const -- `this` unused; returns its own
 *     explicit int argument verbatim (identity passthrough).
 *   GetNumOfObject(int) const / GetTotalSizeForExport(int, int) const
 *     -- both always return 0, all params unused.
 *
 * CSysExSongControl (`GetStorageId()`=0xc, `GetVersion()`=0,
 * `GetObjectSize()`/`GetObjectSizeForExport()`=0x1490) has 1 extra real
 * method:
 *   GetObjectPointer(int, int) const -- calls (real, singleton-style
 *     `this`-through-a-static-pointer idiom already established
 *     project-wide, e.g. CKGBankManager::ms_poInstance)
 *     `CSeqDataManager::GetRegistoredSong(CKGUtil::sm_poSeqDataManager,
 *     param_2)`, discarding its return value -- ground truth's own real
 *     return type IS `void` here (not a decompiler artifact; this
 *     specific function shows no "could not recover" warning, unlike
 *     the 4 GetNumObjectsForDigest cases in sysex_objects.h), so the
 *     apparent waste is reproduced faithfully. `CSeqDataManager` itself
 *     is a real, confirmed, but entirely unmodeled class -- represented
 *     here as a minimal no-op stand-in (this project's established
 *     "declare the minimum viable slice, no-op body, clearly flagged"
 *     convention, same as `storage_converter_ext_stubs.h`'s own
 *     unmodeled-external-class treatment) rather than a genuinely
 *     unresolved extern -- Eva's `make verify` links every test binary
 *     against the FULL reconstructed object tree (unlike OA.ko's
 *     per-test manual linking), so a called-but-undefined symbol would
 *     break every other test, not just this one.
 *
 * `CKGUtil::sm_poKGUIInfo`/`sm_poSeqDataManager`: both real, confirmed
 * static members via relocation (`_ZN7CKGUtil13sm_poKGUIInfoE`/
 * `_ZN7CKGUtil19sm_poSeqDataManagerE`), modeled as a minimal `CKGUtil`
 * stand-in (this project's established "declare only what this pass
 * needs" convention) rather than pulling in KARMA-GE's full, far larger
 * subsystem.
 *
 * Same non-polymorphic-in-this-model / "D0's free(this) not reproduced"
 * dtor convention as the other 2 sibling families -- see
 * sysex_object_names.h's own header comment for the full rationale
 * (independently re-confirmed here too: no per-class vtable object
 * exists for either class).
 */

#ifndef SYSEX_CONTROL_OBJECTS_H
#define SYSEX_CONTROL_OBJECTS_H

struct CKGUtil {
	static unsigned long sm_poKGUIInfo;
	static unsigned long sm_poSeqDataManager;

	/* round 42 additions (sysex_objects_ge_region.h/.cpp) --
	 * sm_poRegionHolder: real, confirmed static member
	 * (`_ZN7CKGUtil16sm_poRegionHolderE`), the base of a 10000-entry,
	 * 0x130-byte-stride region array (CSysExRegion::GetObjectPointer/
	 * GetTotalSizeForExport index directly off it, byte offset +0x18
	 * within each entry is a real "active" flag byte, confirmed via
	 * GetTotalSizeForExport's own real disassembly).
	 * GetUserGE/GetUserKarmaTemplate: real, confirmed free functions
	 * (`_ZN7CKGUtil9GetUserGEEi`/`_ZN7CKGUtil20GetUserKarmaTemplateEi`)
	 * whose own real bodies are NOT modeled here (out of scope, same
	 * "declare the minimum viable slice, no-op body" convention as
	 * CSeqDataManager below) -- both return 0, a neutral default that
	 * still lets the callers' own real pointer arithmetic
	 * (`base + index*stride`) be exercised and verified.
	 */
	static unsigned long sm_poRegionHolder;
	static int GetUserGE(int) { return 0; }
	static int GetUserKarmaTemplate(int) { return 0; }
};

/* CSeqDataManager: real, confirmed, entirely unmodeled elsewhere in
 * this project -- minimal no-op stand-in (see header comment). */
struct CSeqDataManager {
	int GetRegistoredSong(int) { return 0; }
};

class CSysExKarmaGEInfo {
public:
	~CSysExKarmaGEInfo() {}
	int GetObjectPointer(int param1, int param2) const;
	unsigned int GetStorageId() const;
	unsigned int GetNumBanks() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
	int GetSysExBankId(int param1) const;
	unsigned int GetNumOfObject(int param1) const;
	unsigned int GetTotalSizeForExport(int param1, int param2) const;
};

class CSysExSongControl {
public:
	~CSysExSongControl() {}
	/* GetObjectPointer's own real return type is void (see header
	 * comment) -- not reconstructed as a body-only stub returning a
	 * discarded value, since ground truth's own function genuinely
	 * discards CSeqDataManager::GetRegistoredSong()'s result. */
	void GetObjectPointer(int param1, int param2) const;
	unsigned int GetStorageId() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
};

#endif // SYSEX_CONTROL_OBJECTS_H
