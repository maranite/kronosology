/*
 * dir_entry.h  -  CDirEntry, a real, polymorphic (7-slot vtable) DOS 8.3
 * filename/directory-entry value class (`~40` total methods per symbols.csv:
 * GetName/GetExt/SetShortName/SetLongName/GetFirstCluster/... -- a genuine
 * filesystem-metadata class, NOT out of scope for the reason CZ is, but far
 * larger than this batch's own actual need).
 *
 * Eva "size is not depth" re-check batch, 2026-07-26 -- pulled in while
 * re-tracing `CBatchDiskMainTask::CBatchDiskMainTask()`'s own ctor
 * (batch_disk_main_task.h), which embeds one `CDirEntry` member at +0xe4.
 * Only the ctor/dtor (.text+0x08071640/0x08071540,0x080715c0) are reconstructed
 * here -- the other ~38 Get/Set/Copy/Reset/operator= methods have no caller
 * anywhere in this reconstruction's own traced call graph (nothing here reads
 * or writes the embedded member past construction) and are deliberately left
 * unimplemented, same "opaque past ctor/dtor" convention `CEditClient`
 * (edit_man.h) already established for a similarly out-of-proportion class.
 *
 * UPDATE (`CBatchDiskMainTask::PreloadDir()` reconstruction attempt, 2026-07-26):
 * `PreloadDir()` (.text+0x082421f0) turned out to be genuinely out of scope (see
 * batch_disk_main_task.h's own updated header comment for why), BUT its first
 * ~120 bytes -- before it touches anything CZ-container-internal -- directly call
 * 9 more `CDirEntry` predicate/accessor methods on `this->mDirEntry`, ALL of which
 * are plain (non-virtual) `call`s in ground truth, not vtable dispatch, and NONE
 * of which need real `CZ` string semantics -- only raw offset reads (own fields,
 * or `CZ::RawPtrField()`/`RawFlagField()`, cz_util.h). These 9, plus the one real
 * virtual query they collectively depend on, are now reconstructed:
 *   `IsEmpty()`        .text+0x08072660, `return mUnknown5c;` (raw field, not
 *                       normalized to 0/1 -- ground truth's own 2 call sites use
 *                       both a `== 1` and a `!= 0` check against it, so returning
 *                       the raw value matches either).
 *   `IsDeleted()`      .text+0x08072670, `return mUnknown58;` (same raw-field
 *                       shape as IsEmpty()).
 *   `IsReserved()`     .text+0x08072680, `return (mUnknown50 & 0xf) == 0xf;`
 *   `IsLabel()`        .text+0x08072650, `return (mUnknown50 & 0x8) != 0;`
 *   `IsDir()`          .text+0x08072640, `return (mUnknown50 & 0x10) != 0;`
 *   `IsParentDir()`    .text+0x080726d0, `return mUnknown60 != 0 &&
 *                       (mUnknown50 & 0x10) != 0;`
 *   `IsCurrentDir()`   .text+0x080726f0, same shape as IsParentDir() but tests
 *                       `mUnknown64` instead of `mUnknown60`.
 *   `HasValidLongNameExt()` .text+0x08071500 -- THE real per-instance virtual
 *                       this class's own vtable slot 2 (offset+0x8) dispatches to
 *                       (confirmed via direct `.rodata` byte read at
 *                       0x08e81908+0x8 = 0x08071500; FIXED this batch --
 *                       omega_vtables.cpp's `PTR__CDirEntry_08e81908[2]` was
 *                       `EvaVTableStub` before, and `GetName()`/`GetExt()` below
 *                       genuinely dispatch through and consume that slot's
 *                       return value, same "EvaVTableStub leaves EAX as
 *                       meaningless garbage" hazard class already fixed
 *                       elsewhere in this project -- see omega_vtables.cpp's own
 *                       comment for this slot). Real body:
 *                       `return mLongName.RawFlagField() != 0 ||
 *                       mLongExt.RawFlagField() != 0;`. This class's other 4
 *                       real virtual slots (`OnShortNameChanged`/
 *                       `OnShortExtChanged`/`OnLongNameChanged`/
 *                       `OnLongExtChanged`, .text+0x081806a0..0x081806d0) ARE
 *                       confirmed byte-identical empty (`ret` only, no other
 *                       instruction) -- correctly stay `EvaVTableStub`-backed.
 *   `GetName()`        .text+0x080723b0, `HasValidLongNameExt() ? mLongName :
 *                       mShortName`, dispatched via `this->mVtbl + 0x8` (not a
 *                       direct call, matching ground truth's own indirect-call
 *                       shape), returns that CZ's `RawPtrField()` cast to
 *                       `const char*`. Always NULL in this reconstruction today
 *                       (both candidate CZ's `RawPtrField()`s are always 0,
 *                       nothing populates them -- see cz_util.h).
 *   `GetExt()`         .text+0x080723e0, identical shape to GetName() but on
 *                       `mLongExt`/`mShortExt`.
 * None of the above required decoding `CZ::Insert`/`RFind`/`Remove`/the string
 * ctors -- only raw offset reads, matching this project's usual convention for
 * fields/behavior whose surrounding container stays opaque. The remaining ~29
 * Set/Copy/Reset/operator= methods are unchanged from the original verdict above
 * (no reachable caller either way).
 *
 * REAL LAYOUT (from CDirEntry::CDirEntry()/~CDirEntry()'s own disassembly):
 *   +0x00  vtbl (7 slots: dtor D1/D0 + 5 real virtual overrides, not named
 *          here -- EvaVTableStub-backed, see omega_vtables.h)
 *   +0x04  CZ member #1 (spans +0x04..+0x14)
 *   +0x14  CZ member #2 (spans +0x14..+0x24)
 *   +0x24  CZ member #3 (spans +0x24..+0x34)
 *   +0x34  CZ member #4 (spans +0x34..+0x44)
 *   +0x44  int  = 0
 *   +0x48  short = 0
 *   +0x4c  int  = 0
 *   +0x50  byte = 0
 *   +0x51  byte = 0
 *   +0x52  byte = 0
 *   +0x53  byte = 0
 *   +0x54  byte = 0x13
 *   +0x55  byte = 0x50
 *   +0x56  byte = 0x01
 *   +0x57  byte = 0x01
 *   +0x58  int  = 0
 *   +0x5c  int  = 1
 *   +0x60  int  = 0
 *   +0x64  int  = 0
 * Total 0x68 (104) bytes, confirmed from `CBatchDiskMainTask`'s own embedded
 * member spacing (+0xe4 CDirEntry, next field +0x14c CZ member -> 0x14c-0xe4
 * = 0x68 exactly).
 *
 * Every dword/byte value above is transcribed verbatim from the real ctor's
 * own literal immediates -- this project's usual "preserve real field inits
 * even when their meaning isn't decoded" convention (mains.cpp's `Mains()`,
 * e.g.). The 4 embedded `CZ` members are each real, own separate string-like
 * fields (plausibly short name / short ext / long name / long ext, given this
 * class's own real accessor names -- `GetShortName`/`GetShortExt`/
 * `GetLongName`/`GetLongExt`, symbols.csv -- but NOT confirmed by tracing
 * those accessors, which are themselves unreconstructed) -- kept opaque per
 * `cz_util.h`'s own established policy.
 */

#ifndef DIR_ENTRY_H
#define DIR_ENTRY_H

#include "cz_util.h"

class CDirEntry {
public:
	/* .text+0x08071640, 231 bytes. Real body -- see header comment. */
	CDirEntry();

	/* .text+0x08071540 (D1)/0x080715c0 (D0). Real body -- see header
	 * comment (destructs the 4 CZ members; CZ's own opaque dtor is a
	 * no-op, see cz_util.h).
	 */
	~CDirEntry();

	/* All 9 below: real, self-contained, no CZ-internal decoding needed --
	 * see header comment for exact addresses/bodies. */
	int  IsEmpty() const      { return mUnknown5c; }
	int  IsDeleted() const    { return mUnknown58; }
	bool IsReserved() const   { return (mUnknown50 & 0xf) == 0xf; }
	bool IsLabel() const      { return (mUnknown50 & 0x8) != 0; }
	bool IsDir() const        { return (mUnknown50 & 0x10) != 0; }
	bool IsParentDir() const  { return mUnknown60 != 0 && (mUnknown50 & 0x10) != 0; }
	bool IsCurrentDir() const { return mUnknown64 != 0 && (mUnknown50 & 0x10) != 0; }

	/* .text+0x08071500. Real body -- see header comment. This is the real
	 * per-instance virtual THIS class's own vtable slot 2 dispatches to
	 * (fixed in omega_vtables.cpp this batch).
	 */
	bool HasValidLongNameExt() const
	{
		return mLongName.RawFlagField() != 0 || mLongExt.RawFlagField() != 0;
	}

	/* .text+0x080723b0/0x080723e0. Real bodies -- see header comment.
	 * Both dispatch through `mVtbl + 0x8` (matching ground truth's own
	 * indirect-call shape), not a direct call to HasValidLongNameExt().
	 */
	const char *GetName() const;
	const char *GetExt() const;

private:
	void         *mVtbl;
	CZ            mShortName; /* +0x04 */
	CZ            mShortExt;  /* +0x14 */
	CZ            mLongName;  /* +0x24 */
	CZ            mLongExt;   /* +0x34 */
	int           mUnknown44;
	short         mUnknown48;
	int           mUnknown4c;
	unsigned char mUnknown50;
	unsigned char mUnknown51;
	unsigned char mUnknown52;
	unsigned char mUnknown53;
	unsigned char mUnknown54;
	unsigned char mUnknown55;
	unsigned char mUnknown56;
	unsigned char mUnknown57;
	int           mUnknown58;
	int           mUnknown5c;
	int           mUnknown60;
	int           mUnknown64;

	friend struct DirEntryTestHooks;
};

#endif /* DIR_ENTRY_H */
