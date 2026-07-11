// SPDX-License-Identifier: GPL-2.0
/*
 * tone_adjust_descriptors.cpp  -  CSTGToneAdjustDescriptor::
 * InitializeCommonToneAdjustDescriptors() (batch 53, `.text+0x29660`,
 * 2662 bytes).
 *
 * Reached from init_module()'s own transitive call graph:
 * setup_global_resources() -> CSTGEngine::Initialize() (its own
 * confirmed ~44-entry construction table, engine_init.cpp) ->
 * `CSTGVoiceModelManager`'s constructor -> (per oa_engine.h's own
 * class comment) this function -- init_module() step 8.
 *
 * Ground-truthed via a full objdump -d -r disassembly plus a Python
 * relocation-stream simulator (same "deterministic-table-via-script"
 * technique already established for CSTGCCInfo::sCCInfoTable, sec
 * 10.161) -- confirmed ZERO calls/branches other than the three lazy-
 * init guard checks themselves ("safe by instruction class").
 *
 * Confirmed shape: three near-identical `static` (function-local,
 * lazily-initialized exactly once each -- matching `-fno-threadsafe-
 * statics`'s plain guard-byte codegen, no real C++ runtime guard
 * machinery) 0x34-byte (52-byte) "param descriptor" objects --
 * `offParamDesc`, `smoothed99ToFloat`, `lfoStopParamDesc` -- followed
 * by an unconditional, always-re-run 37-entry x 16-byte data table,
 * `STGToneAdjustCommonParams` (0x250 bytes total, confirmed via `nm`
 * against the real `.bss` size), each entry referencing one of the
 * three local descriptors above, an external `CSTGParamDescriptor::
 * sTypical99ToFloatParamDesc` (a DIFFERENT not-yet-reconstructed
 * global, populated by its own separate C++ global constructor that
 * runs earlier in the module's `.init_array` phase -- we only ever
 * take its ADDRESS here, never read through it, so its own content
 * being unmodeled is harmless for this function), or (index 31-36) a
 * byte offset into two other large not-yet-modeled external tables,
 * `STGProgramParams` (0xbc8 bytes) / `STGCommonStepSeqParams` (0x2a4
 * bytes).
 *
 * CONFIRMED REAL, PRESERVED FAITHFULLY (a genuine uninitialized-stack-
 * read quirk, the same class already established for `CSTGChannelValues::
 * Reset()`'s own "stale" controller-flag byte, sec 10.92/10.115, just a
 * larger instance of it): each of the three 0x34-byte descriptors has
 * SIX confirmed dword-sized fields (`+0x4`/`+0x8`/`+0xc`/`+0x10`/`+0x14`/
 * `+0x18`) populated via a plain 4-byte stack-to-static `mov`, where the
 * SOURCE stack slot is verified (by an exhaustive scan of every
 * instruction in this function, not just a spot check) to be NEVER
 * WRITTEN anywhere else in the function -- i.e. genuinely uninitialized
 * kernel-stack content at the moment of the (real, one-time) call, not a
 * compile-time constant. The lone exception: the LAST of the six
 * (`+0x18`)'s own source stack slot has its low TWO bytes deterministically
 * pre-set (`1`, then `0`) by two `movb` immediates just before the 4-byte
 * read -- confirmed identical across all three descriptor blocks -- so
 * `+0x18`'s own low 16 bits are `0x0001` every time, while its upper 16
 * bits remain genuine stack garbage. Modeled here via a local,
 * deliberately NOT fully initialized scratch array (see `FillDescriptor()`
 * below) -- not simplified into a made-up constant.
 *
 * All other fields of the three descriptors, and every field of every
 * `STGToneAdjustCommonParams` entry, are confirmed deterministic literal
 * immediates (or pointer relocations), transcribed byte-exact from the
 * relocation-stream simulator's own output.
 */

#include "oa_global.h"
#include "oa_engine.h"

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

/*
 * Storage for the externs declared in oa_engine.h (see there for the full
 * derivation of each). CSTGParamDescriptor::sTypical99ToFloatParamDesc and
 * STGProgramParams/STGCommonStepSeqParams are all owned by OTHER not-yet-
 * reconstructed global constructors -- this function only ever takes their
 * ADDRESS, never reads through them, so zero-initialized stand-ins are
 * sufficient here.
 */
unsigned char CSTGParamDescriptor::sTypical99ToFloatParamDesc[0x34];
unsigned char STGProgramParams[0xbc8];
unsigned char STGCommonStepSeqParams[0x2a4];
STGToneAdjustParamEntry STGToneAdjustCommonParams[37];

/*
 * FillDescriptor() -- shared body for the three lazy-init 0x34-byte
 * descriptors. `garbage12345` and the upper 16 bits of `garbage6` are
 * CONFIRMED REAL uninitialized stack content (see file header comment) --
 * deliberately left unset, not zeroed and not fabricated.
 */
static void FillDescriptor(unsigned char *d, unsigned int f1c, unsigned int f20,
			    unsigned int f24, unsigned int f28,
			    unsigned char f2c, unsigned char f2d,
			    unsigned char f2e, unsigned char f2f)
{
	unsigned int garbage[6]; /* CONFIRMED REAL: never written by ground
				   * truth before being read -- see header
				   * comment. Deliberately left uninitialized. */
	((unsigned char *)&garbage[5])[0] = 1;
	((unsigned char *)&garbage[5])[1] = 0;
	/* garbage[5]'s upper 2 bytes stay genuinely uninitialized too. */

	*(unsigned int *)(d + 0x00) = 0;
	*(unsigned int *)(d + 0x04) = garbage[0];
	*(unsigned int *)(d + 0x08) = garbage[1];
	*(unsigned int *)(d + 0x0c) = garbage[2];
	*(unsigned int *)(d + 0x10) = garbage[3];
	*(unsigned int *)(d + 0x14) = garbage[4];
	*(unsigned int *)(d + 0x18) = garbage[5];
	*(unsigned int *)(d + 0x1c) = f1c;
	*(unsigned int *)(d + 0x20) = f20;
	*(unsigned int *)(d + 0x24) = f24;
	*(unsigned int *)(d + 0x28) = f28;
	d[0x2c] = f2c;
	d[0x2d] = f2d;
	d[0x2e] = f2e;
	d[0x2f] = f2f;
	d[0x30] |= 1;
}

void CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors()
{
	static unsigned char offParamDesc[0x34];
	static bool offReady;
	if (!offReady) {
		FillDescriptor(offParamDesc, 0, 0, 0, 0x3f800000, 0x10, 0, 0, 0);
		/* +0x0 itself is explicitly re-zeroed by FillDescriptor(); ground
		 * truth's own +0x0 write for offParamDesc IS a plain 0 too. */
		offReady = true;
	}

	static unsigned char smoothed99ToFloat[0x34];
	static bool smoothedReady;
	if (!smoothedReady) {
		FillDescriptor(smoothed99ToFloat, 0xffffff9d /* -99 */, 99,
				0xbf800000 /* -1.0f */, 0x3f800000 /* 1.0f */,
				1, 0, 1, 0);
		smoothedReady = true;
	}

	static unsigned char lfoStopParamDesc[0x34];
	static bool lfoReady;
	if (!lfoReady) {
		FillDescriptor(lfoStopParamDesc, 0xffffffff, 1, 0, 0x3f800000,
				0x10, 0, 0, 0);
		/* Ground truth's own +0x0 for lfoStopParamDesc is 0xffffffff,
		 * NOT 0 -- FillDescriptor() unconditionally zeroes +0x0, so
		 * patch it back afterward to preserve this real asymmetry. */
		*(unsigned int *)(lfoStopParamDesc + 0x00) = 0xffffffff;
		lfoReady = true;
	}

	static const struct { unsigned char *ptr; unsigned char b4, b5; int f8; } kEntries[37] = {
		{ offParamDesc,                                          1, 2,  -1 },
		{ smoothed99ToFloat,                                     0, 3,   0 },
		{ smoothed99ToFloat,                                     0, 3,   0 },
		{ smoothed99ToFloat,                                     0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ lfoStopParamDesc,                                      1, 3,  -1 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 3,   0 },
		{ lfoStopParamDesc,                                      1, 3,  -1 },
		{ CSTGParamDescriptor::sTypical99ToFloatParamDesc,       0, 2,   0 },
		{ STGProgramParams + 832,                                1, 0,  16 },
		{ STGProgramParams + 884,                                1, 0,  17 },
		{ STGProgramParams + 936,                                1, 0,  18 },
		{ STGProgramParams + 988,                                1, 0,  19 },
		{ STGCommonStepSeqParams + 260,                          1, 1,   5 },
		{ STGCommonStepSeqParams + 624,                          1, 1,  12 },
	};

	for (unsigned int i = 0; i < 37; i++) {
		STGToneAdjustCommonParams[i].ptr32 = ToU32(kEntries[i].ptr);
		STGToneAdjustCommonParams[i].b4 = kEntries[i].b4;
		STGToneAdjustCommonParams[i].b5 = kEntries[i].b5;
		STGToneAdjustCommonParams[i].b6 = 0xff;
		STGToneAdjustCommonParams[i].b7 = 0xff;
		STGToneAdjustCommonParams[i].f8 = (unsigned int)kEntries[i].f8;
		STGToneAdjustCommonParams[i].fc = 0;
	}
}
