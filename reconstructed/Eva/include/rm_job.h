/*
 * rm_job.h  -  CRMJob, a small "resource-manager job descriptor" value class.
 * Eva "size is not depth" re-check batch, 2026-07-26 -- pulled in while
 * re-tracing `CBatchDiskMainTask::CBatchDiskMainTask()` (batch_disk_main_task.h)
 * to determine whether the ctor's own logic (excluding the CZ sub-object) is
 * tractable. It is: `CRMJob` (.text+0x081660d0 ctor / 0x08166180,0x081661e0
 * dtor variants, symbols.csv) is fully self-contained, 0x54 (84) bytes,
 * confirmed from `CRMApiCallBack::~CRMApiCallBack()`'s own `malloc(0x54)`
 * caller (rm_api_callback.h).
 *
 * REAL LAYOUT (from CRMJob::CRMJob()/~CRMJob()'s own disassembly):
 *   +0x00  4 bytes, zeroed by ctor (real meaning not decoded -- never read by
 *          any reconstructed code)
 *   +0x04  ditto
 *   +0x08  CZ member #1 (cz_util.h, opaque, 0x10 bytes -- spans +0x08..+0x18)
 *   +0x18  CZ member #2 (opaque, spans +0x18..+0x28)
 *   +0x28  void* mUnknown28[6] (+0x28,+0x2c,+0x30,+0x34,+0x38,+0x3c) -- all
 *          zeroed by the ctor; the dtor conditionally calls each non-null
 *          entry's own vtable slot+4 (a "release/delete"-shaped virtual call)
 *          before freeing. Real pointee type not identified (plausibly some
 *          `CDirEntry`/path-fragment descriptor per this class's own name and
 *          neighbourhood -- CBatchDiskMainTask's own PreloadDir/PreloadGroup,
 *          batch_disk_main_task.h, are exactly the deferred methods that would
 *          populate these). Since NOTHING in this reconstruction's own traced
 *          call graph ever sets these fields away from the ctor's own zero
 *          (PreloadDir/PreloadGroup/AddItemToPreload stay Tier-B stubs), the
 *          dtor's own conditional vcall never fires in practice here --
 *          modeled as opaque `void*` with a real, faithful "skip if null"
 *          dtor loop rather than fully typing the pointee.
 *   +0x40  unsigned char mUnknown40[3] (+0x40,+0x41,+0x42) -- ctor sets all 3
 *          to 0xff (a flag/sentinel triple, real meaning not decoded)
 *   +0x44  int mUnknown44 = -1 (ctor: `0xffffffff`)
 *   +0x48  int mUnknown48 = 0
 *   +0x4c  int mUnknown4c = 0
 *   +0x50  int mUnknown50 = 1
 * Total 0x54 bytes, matching the real heap allocator's own `malloc(0x54)` call
 * site exactly.
 *
 * Real caller (`CRMApiCallBack`'s own ctor path) heap-allocates this via
 * `malloc`/`free` (HAL_DisableInterrupts/EnableInterrupts-wrapped, this
 * project's own established "raw malloc'd object" idiom, e.g. task.h) rather
 * than embedding it -- modeled the same way here (rm_api_callback.h).
 */

#ifndef RM_JOB_H
#define RM_JOB_H

#include "cz_util.h"

class CRMJob {
public:
	/* .text+0x081660d0, 150 bytes. Real body -- see header comment. */
	CRMJob();

	/* .text+0x08166180 (D1)/0x081661e0-ish (D0) -- same shape as every other
	 * D1/D0 pair in this project, transcribed as one real destructor.
	 */
	~CRMJob();

private:
	unsigned char mUnknown00[8];  /* +0x00..0x08, zeroed */
	CZ            mUnknown08;     /* +0x08 */
	CZ            mUnknown18;     /* +0x18 */
	void         *mUnknown28[6];  /* +0x28..0x40 */
	unsigned char mUnknown40[3];  /* +0x40..0x43 */
	int           mUnknown44;     /* +0x44 */
	int           mUnknown48;     /* +0x48 */
	int           mUnknown4c;     /* +0x4c */
	int           mUnknown50;     /* +0x50 */

	friend struct RMJobTestHooks;
};

#endif /* RM_JOB_H */
