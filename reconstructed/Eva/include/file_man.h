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
 *   +0x2c..+0x42c  mUnitTable[128]     128 x 8-byte entries, ctor sets every one to
 *                  {1, 0} (SSlot8 below). Real element type/meaning not decoded --
 *                  plausibly a per-logical-unit slot table, given
 *                  FindUnit()/AddUnit()/RemoveUnit()/GetUnit() all operate on
 *                  something at this class's own scale, but not confirmed.
 *   +0x42c..+0x82c mHandleTable[128]   128 x 8-byte entries, same {1, 0} pattern --
 *                  plausibly a per-open-file-handle table, given
 *                  InitHandleTable()/ClearHandleTable()/AddNewHandle()/RemoveHandle()
 *                  exist at this class's own scale; not confirmed.
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

	SSlot8         mUnitTable[128];
	SSlot8         mHandleTable[128];
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
