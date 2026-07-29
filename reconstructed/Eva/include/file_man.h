/*
 * file_man.h  -  CFileMan, Eva's file/storage-driver manager module (`MMainFileMan()`,
 * mains.cpp). Real ctor reconstructed here (Stage 6 breadth sweep, 2026-07-25 -- the
 * "What's still open" CFileMan/CResMan ctor batch); the rest of the class (~60 further
 * methods -- RegisterDriver/FDisk/ScanPartitionTable/GetUnitNames/... -- some of them
 * multi-KB) is a genuine god-object, out of scope for this pass, same
 * "CForm/Peg-scale, indefinitely deferred" boundary as the ES-family CXxxTask
 * god-objects (README.md's "What's still open").
 *
 * Real layout confirmed from CFileMan@081011e0.c (the only reconstructed method,
 * ctor, 1079 bytes) -- 0xa5c (2652) bytes total, base CModule (0x2c) + 0xa30 of its
 * own fields:
 *   +0x2c..+0x42c  mHandleTable[128]   128 x 8-byte entries, ctor sets every one to
 *                  {1, 0} (SSlot8 below). CONFIRMED (round 58) via the real
 *                  ground-truth function names themselves: InitHandleTable()/
 *                  AddNewHandle()/RemoveHandle()/GetFile() all operate on this
 *                  exact offset range -- a per-open-file-handle table. (Round 55
 *                  originally mis-labeled this range "mUnitTable"; GetFile()'s
 *                  own byte-offset arithmetic was always correct, only the
 *                  field's name/purpose comment was backwards.)
 *   +0x42c..+0x82c mUnitTable[128]     128 x 8-byte entries, same {1, 0} pattern.
 *                  CONFIRMED (round 58) the same way: InitUnitTable()/
 *                  RemoveUnit()/GetUnitForModify() all operate on this exact
 *                  offset range -- a per-logical-unit slot table.
 *   +0x82c..+0xa2c mIOCTLDevTable[32]  32 x 16-byte entries, ctor sets every one to
 *                  {1, 0, 0, 0} (SSlot16 below). Plausibly the IOCTL-device table
 *                  InitIOCTLDevTable()/AddIOCTLDev()/RemoveIOCTLDev()/GetIOCTLDev()
 *                  operate on; not confirmed.
 *   +0xa2c..+0xa44 mDriverConstructors  embedded COmegaPtrArray (default ctor,
 *                  0x18 bytes), vtable-swapped to the real
 *                  `TNamedPtrArray<CFMDriverConstructor>` (confirmed via `nm -C`'s
 *                  own "vtable for TNamedPtrArray<CFMDriverConstructor>" symbol) --
 *                  the registry AddDriverConstructor()/RemoveDriverConstructor()/
 *                  FindDriverConstructor() operate on (none reconstructed).
 *   +0xa44         mUnknownA44   ctor sets 0x400; real meaning not decoded
 *   +0xa48         mUnknownA48   ctor sets 0x2000; real meaning not decoded
 *   +0xa4c         mUnknownA4c   ctor zeroes; real meaning not decoded
 *   +0xa50         mBackgroundJobsEnabled  0 by default; set 1 if the config string
 *                  "FMBackGroundJobs" (via Api's vtable slot +0x38 -- see below)
 *                  case-insensitively equals "Enabled". Read back by the real
 *                  IsBackgroundJobsEnabled()/written by EnableBackgroundJobs(int)
 *                  (both reconstructed here too, confirming this field's meaning).
 *   +0xa54         mMinIdleTimeToStartBGJobs  default 1000; overridden by
 *                  strtol()-parsing the config string "FMMinIdleTimeToStartBGJobs"
 *                  if present.
 *   +0xa58         mDeltaTimeBetweenBGJobs  default 800; overridden by
 *                  strtol()-parsing the config string "FMDeltaTimeBetweenBGJobs"
 *                  if present.
 *
 * Api's own vtable slot +0x38 (called 3 times in the ctor, `(**(code**)(*Api+0x38))
 * (Api, "<name>")`) is a new, previously-undocumented dispatch site (system_api.h) --
 * a named config-string getter: returns a `char*` (NULL if the key is absent), same
 * per-key-string lookup shape as `CConfigManager`'s own config-table reads elsewhere
 * in this project. Real identity not decoded, only this call shape.
 */

#ifndef FILE_MAN_H
#define FILE_MAN_H

#include "module.h"
#include "omega_ptr_array.h"

class CFileMan : public CModule {
public:
	CFileMan();

	/* .text+0x080fc710/0x080fc720/0x080fc730, 3 bytes each -- all 3 confirmed
	 * genuinely empty (`return 0;` in the real binary; kept `void` here since
	 * CModuleManager::Setup()/Config()/Start()'s own real vtable dispatch
	 * (module_manager.cpp's `CallVSlot`) always discards the return value, same
	 * convention as CDumpManMod::Setup()/Config()/Start()). Wired into this
	 * class' own real vtable slots 2/3/4 (omega_vtables.h/.cpp) so that existing
	 * dispatch lands on the real (trivial) body instead of a generic no-op stub.
	 */
	void Setup();
	void Config();
	void Start();

	/* Round 55 batch (2026-07-29, solo) -- 6 further small, self-contained
	 * methods confirming mUnitTable/mHandleTable/mIOCTLDevTable's own real
	 * {flag, ...} slot semantics (header comment above), plus 2 that don't
	 * touch `this` at all. Deferred siblings from the same survey:
	 * DriverSupportPartitions/GetNumDriverDescriptions/GetDriverName (all 3
	 * dispatch through this's own unconfirmed vtable slot 0xc0 into a wholly
	 * undeclared CVirtualDriverBase), GetNumInstalledUnit (dispatches through
	 * this's own unconfirmed vtable slot 0xbc), EndWritePartitionTable
	 * (CDriverTaskBase undeclared + CSysApiInstance::EnableLevel unreconstructed),
	 * ~CFileMan (both the 41B D0 wrapper and the 1928B D1 body it forwards to --
	 * the latter is a genuine god-object teardown, out of scope, same boundary
	 * as this class's own ~60-method backlog).
	 */

	/* .text+0x080fc8a0, 31 bytes. mHandleTable[i]: mField0==0 (in use) -> return
	 * mField4 (the stored value); else 0. Confirms mHandleTable's {flag,value}
	 * slot semantics from the outside (GetUnitForModify does the same for
	 * mUnitTable below). NOTE: 0x080fc760 (this method's address as recorded
	 * here through round 65) is actually CFileMan::GetUnit, a distinct still-
	 * pending jumptable-dispatch method -- fixed round 58, see
	 * manifest/gen_manifest.py's own round-58 comment. Also round 58: this
	 * method's own byte-offset arithmetic was always correct; only the
	 * mUnitTable/mHandleTable field NAMES were swapped by round 55 -- see the
	 * class-level layout comment above and gen_manifest.py.
	 */
	unsigned int GetFile(int index) const;

	/* .text+0x080fc780, 34 bytes. Same shape as GetFile() but on mUnitTable. */
	unsigned int GetUnitForModify(int index) const;

	/* Round 58 batch (2026-07-29, solo): 7 further CFileMan methods, all
	 * self-contained field-table init/scan/mutate operations on the now-
	 * correctly-named mHandleTable/mUnitTable/mIOCTLDevTable arrays above, no
	 * unresolved callees. Deferred siblings from this round's own survey:
	 * GetNumInstalledUnit/DriverSupportPartitions/GetNumDriverDescriptions/
	 * GetDriverName (all dispatch through this's own unconfirmed vtable slots
	 * 0xbc/0xc0 into a wholly undeclared CVirtualDriverBase, same as round 55's
	 * own deferral list), BeginWritePartitionTable/EndWritePartitionTable/
	 * EndScanPartitionTable/BeginScanPartitionTable (CDriverTaskBase/
	 * CSysApiInstance::EnableLevel unreconstructed), RestoreUnitReadWriteProtectInfo/
	 * BackupUnitReadWriteProtectInfo (iterate the still-unreconstructed
	 * mDriverConstructors registry).
	 */

	/* .text+0x08101650, 221 bytes. Resets every mUnitTable[i] to {1,0}
	 * (ctor-default free state) -- same shape as the ctor's own init loop,
	 * confirmed byte-for-byte against it.
	 */
	void InitUnitTable();

	/* .text+0x081018f0, 173 bytes. Same shape as InitUnitTable() but on
	 * mHandleTable.
	 */
	void InitHandleTable();

	/* .text+0x080ff7a0, 96 bytes. mUnitTable[idx]: decrements the shared
	 * "installed unit count" field (mUnknownA4c), frees the slot's stored
	 * pointer (mField4) under a HAL interrupt-disable/enable pair if
	 * non-NULL, then resets the slot to {1,0}.
	 */
	void RemoveUnit(int index);

	/* .text+0x08102510, 165 bytes. Linear scan of mHandleTable for the first
	 * free slot (mField0!=0), starting at index 0; on success stores
	 * `descriptor` into that slot's mField4 and marks it in-use
	 * (mField0=0), returning the found index; returns -1 if the table is
	 * completely full. (Ghidra's own unrolled do-while loop collapsed
	 * 1:1 into a plain `for`.)
	 */
	int AddNewHandle(void *descriptor);

	/* .text+0x081029a0, 250 bytes. mHandleTable[idx]: if in use, walks 4
	 * fixed sub-object offsets inside the stored CFileDescriptor* (+0x170/
	 * +0x160/+0x14c/+0x13c, each paired with its own "owned" flag at
	 * +0x17c/+0x16c/+0x158/+0x148) freeing any owned, non-NULL one under a
	 * HAL interrupt-disable/enable pair, then frees the descriptor itself
	 * the same way; finally resets the slot to {1,0} regardless. The 4
	 * sub-object offsets are modeled as an opaque byte blob (CFileDescriptor
	 * itself is not reconstructed) -- see the .cpp for the exact struct.
	 */
	void RemoveHandle(int index);

	/* .text+0x080fc8c0, 184 bytes. Two soft-assert calls (Api's vtable slot
	 * +0x94, same convention as res_man.cpp's ApiAssert()) if `task` or
	 * `iface` is NULL, then a linear scan of mIOCTLDevTable for the first
	 * free slot (mField0==1); on success fills all 4 fields and returns the
	 * found index, else -1.
	 */
	int AddIOCTLDev(int access, void *task, void *iface);

	/* .text+0x080fc800, 151 bytes. Counts mUnitTable[i].mField0==0 (in-use)
	 * slots across all 128 entries and compares the total against
	 * mUnknownA4c (the same "installed unit count" field RemoveUnit()
	 * decrements) -- a consistency self-check between the table's actual
	 * occupancy and the tracked count.
	 */
	bool CheckNumInstalledUnit() const;

	/* .text+0x080fc9c0, 38 bytes. Real return type is the raw address of
	 * mIOCTLDevTable[index] (Ghidra mis-inferred it as CFileMan* from the
	 * shared `this + offset` arithmetic shape) when that slot's mField0==0
	 * (in use), else NULL.
	 */
	void *GetIOCTLDev(int index);

	/* .text+0x080fc980, 63 bytes. Resets mIOCTLDevTable[index] back to its
	 * ctor-default free state ({1,0,0,0}) if it was in use (mField0==0);
	 * returns 1 on success, 0 otherwise (bad index or already free).
	 */
	int RemoveIOCTLDev(int index);

	/* .text+0x08105390, 13 bytes. Real: `void`, not `int` -- calls
	 * CZ::StrCmpIgnoreCase (cz_util.h) but DISCARDS its result and returns
	 * nothing. Faithfully preserved as a genuinely useless real function
	 * rather than "fixed" into returning the comparison it computes.
	 */
	static void UnitNameCompare(const char *a, const char *b);

	/* .text+0x08105350, 56 bytes. Pure bit-flag comparison, doesn't touch
	 * `this` at all -- real access-permission-vs-request bit test (bit0/1/2 =
	 * read/write/exclusive-style flags, exact meaning not decoded).
	 */
	static bool IsAccessDenied(unsigned char requested, unsigned char granted);

	/* .text+0x080fc740, 11 bytes / .text+0x080fc750, 15 bytes -- trivial real
	 * accessors for mBackgroundJobsEnabled, confirming that field's meaning.
	 */
	int  IsBackgroundJobsEnabled() const;
	void EnableBackgroundJobs(int enabled);

private:
	struct SSlot8 {
		int mField0; /* ctor sets 1 */
		int mField4; /* ctor sets 0 */
	};
	struct SSlot16 {
		int mField0; /* ctor sets 1 */
		int mField4; /* ctor sets 0 */
		int mField8; /* ctor sets 0 */
		int mFieldC; /* ctor sets 0 */
	};

	SSlot8         mHandleTable[128];
	SSlot8         mUnitTable[128];
	SSlot16        mIOCTLDevTable[32];
	COmegaPtrArray mDriverConstructors;
	int            mUnknownA44;
	int            mUnknownA48;
	int            mUnknownA4c;
	int            mBackgroundJobsEnabled;
	int            mMinIdleTimeToStartBGJobs;
	int            mDeltaTimeBetweenBGJobs;
};

#endif /* FILE_MAN_H */
