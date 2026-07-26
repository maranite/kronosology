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
