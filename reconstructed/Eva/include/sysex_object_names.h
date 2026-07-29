// SPDX-License-Identifier: GPL-2.0
/*
 * sysex_object_names.h  -  the 8-class CSysEx*Name/CSysExSetListSlotComment
 * family: trivial per-record-type constant accessors (storage id, version,
 * on-disk/export object size) for SysEx-transferable named objects.
 *
 * FOUND 2026-07-29 (round 39, solo -- session-wide 200-subagent dispatch
 * cap hit, see PROJECT_BRAIN/status.md), a fresh `nm -C` class-inventory
 * sweep for a tight, previously-untouched cluster. All 8 classes
 * (`CSysExSetListSlotComment`/`CSysExSetListSlotName`/`CSysExCombiName`/
 * `CSysExProgName`/`CSysExSongName`/`CSysExWaveSeqName`/
 * `CSysExDrumKitName`/`CSysExSetListName`) share the IDENTICAL 4-method
 * shape, confirmed via `/home/share/Decomp/EVA_Decomp/eva_export`'s own
 * per-function decompiles:
 *   GetStorageId()             -- a sequential per-class literal, 0x1c
 *                                  (Comment) through 0x23 (SetListName)
 *   GetVersion()                -- always literal 0
 *   GetObjectSize()              -- literal object size in bytes
 *   GetObjectSizeForExport()     -- SAME literal as GetObjectSize() for
 *                                  every class in this family
 *
 * Object sizes: 0x200 (512) for `CSysExSetListSlotComment` only (a
 * free-text comment field), 0x18 (24) for all 7 name-record classes
 * (fixed-width name buffer + header -- the name buffer itself is not
 * modeled, no method here reads/writes it).
 *
 * NOT MODELED (deliberately, matching this project's established
 * "declare only what this pass reconstructs" discipline): the real base
 * class `CSysExObjectBase` (`HasDigests()`/`GetObjectSize(void const*)`/
 * etc, none of which are called anywhere in this project's current call
 * graph) and any vtable/inheritance relationship to it. Ground truth's
 * own destructor pair for each class (11-byte "reset vtable ptr" +
 * 39-byte "reset vtable ptr, then free(this) inside a real
 * HAL_DisableInterrupts()/HAL_EnableInterrupts() bracket") is the SAME
 * "D0 additionally wraps free(this) -- not reproduced here" pattern
 * already established project-wide (see e.g. `long_binary_file.cpp`'s
 * own header comment) -- a plain empty `~ClassName() {}` here covers the
 * base case; the deleting-destructor variant is left pending, as this
 * project's convention for that pattern already treats it. Every one of
 * the 8 real destructors resets its vtable pointer to the SAME shared
 * `PTR__CSysExObjectBase_08f7a908` symbol (confirmed via
 * `/home/share/Decomp/EVA_Decomp/eva_export/symbols.csv` -- no per-class
 * `PTR__CSysEx*Name` vtable object exists), meaning these 4 accessor
 * methods are NOT virtual overrides in ground truth (no per-class vtable
 * to hold distinct slots) -- confirming a plain, non-polymorphic class
 * shape here is faithful, not a simplification.
 *
 * Method bodies are out-of-line (sysex_object_names.cpp), not inline in
 * this header -- matching this project's established convention of real
 * linkable .cpp translation units for reconstructed methods.
 */

#ifndef SYSEX_OBJECT_NAMES_H
#define SYSEX_OBJECT_NAMES_H

class CSysExSetListSlotComment {
public:
	~CSysExSetListSlotComment() {}
	unsigned int GetStorageId() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
};

class CSysExSetListSlotName {
public:
	~CSysExSetListSlotName() {}
	unsigned int GetStorageId() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
};

class CSysExCombiName {
public:
	~CSysExCombiName() {}
	unsigned int GetStorageId() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
};

class CSysExProgName {
public:
	~CSysExProgName() {}
	unsigned int GetStorageId() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
};

class CSysExSongName {
public:
	~CSysExSongName() {}
	unsigned int GetStorageId() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
};

class CSysExWaveSeqName {
public:
	~CSysExWaveSeqName() {}
	unsigned int GetStorageId() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
};

class CSysExDrumKitName {
public:
	~CSysExDrumKitName() {}
	unsigned int GetStorageId() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
};

class CSysExSetListName {
public:
	~CSysExSetListName() {}
	unsigned int GetStorageId() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
};

#endif // SYSEX_OBJECT_NAMES_H
