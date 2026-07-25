/*
 * dump_man_mod.h  -  CDumpManMod : public CModule, Stage 6 breadth sweep, 2026-07-25
 * (DumpManager cluster batch).
 *
 * REACHABILITY: `MMainDumpMan()` (mains.cpp, already Tier A) is one of `CKernel::
 * InitSystemLayer()`'s own 9-member `MMainXxx(void)` family. Ground truth's own
 * `MMainDumpMan()` builds a plain `CModule("DumpManager")` base object by hand (base
 * ctor + manual vtable-swap to `PTR__CDumpManMod_08e85ca8`) rather than calling a
 * `CDumpManMod::CDumpManMod()` ctor directly -- BYTE-IDENTICAL to what this class' own
 * real ctor does anyway (`CDumpManMod` adds no fields beyond `CModule`, confirmed:
 * `MMainDumpMan()`'s own `malloc(0x2c)` matches `CModule`'s documented minimum base
 * size exactly, module.h) -- so `mains.cpp` is left unchanged (it already produces the
 * correct object), and THIS class' own ctor is provided for structural completeness /
 * the "ground-truth reachable is the bar" precedent (task.h), not because anything
 * calls it directly.
 *
 * `CModuleManager::Setup()`/`Config()`/`Start()` (module_manager.cpp, already Tier A)
 * dispatch through every registered module's own vtable slots +8/+0xc/+0x10 --
 * `PTR__CDumpManMod_08e85ca8`'s own slots 2/3/4 (dump_man_mod.cpp) are wired to real
 * forwarders calling this class' `Setup()`/`Config()`/`Start()` directly, upgrading
 * this module from "constructed but every lifecycle dispatch lands on EvaVTableStub"
 * to genuinely live. `Setup()` is the one that matters: it constructs the real
 * `CDumpTask`/`CBufferingTask` sibling pair (dump_task.h/buffering_task.h) and
 * registers both via the already-real `CModule::Add(CTask*)` (module.h) -- the first
 * time this reconstruction's own wired call graph populates a module's `mTasks` with
 * something other than the placeholder chain `verify/test_task.cpp` built by hand.
 *
 * `Config()`/`Start()` are both real, genuinely-empty (`return 0;` in the shipped
 * binary itself, confirmed by reading each decompile) -- transcribed as such, not
 * placeholders (same "read it, don't assume" treatment as `CConfigManager::
 * SetupRouting()`'s own confirmed-empty body, config_manager.cpp).
 */

#ifndef DUMP_MAN_MOD_H
#define DUMP_MAN_MOD_H

#include "module.h"

class CDumpManMod : public CModule {
public:
	/* .text+0x080cf820, 37 bytes. See header comment -- ground truth's own real
	 * boot-path caller (mains.cpp's MMainDumpMan()) builds an equivalent object by
	 * hand instead of calling this.
	 */
	CDumpManMod();

	/* .text+0x080cf650, 384 bytes. Real body -- see header comment. */
	void Setup();

	/* .text+0x080cf500, 3 bytes. Confirmed genuinely `return 0;` in the real
	 * binary.
	 */
	void Config();

	/* .text+0x080cf510, 3 bytes. Confirmed genuinely `return 0;` in the real
	 * binary.
	 */
	void Start();
};

/* Real vtable-slot forwarders wired into PTR__CDumpManMod_08e85ca8's own slots 2/3/4
 * (byte offsets +8/+0xc/+0x10) -- dump_man_mod.cpp. Slots 0/1 (dtor pair) and 5/6
 * (Destroy/GetErrorMsg, inherited unmodified from CModule -- confirmed by a direct
 * .rodata vtable-slot byte read resolving to CModule::Destroy()/GetErrorMsg(), not a
 * CDumpManMod-specific override) stay EvaVTableStub, matching CModule's own "never
 * actually reached on this pass's traced boot path" status for those 2 slots
 * (module.h) -- nothing destroys a CDumpManMod, and nothing calls Destroy()/
 * GetErrorMsg() on one, on the boot path this project traces.
 */
extern "C" void CDumpManModSetupVSlot(void *obj);
extern "C" void CDumpManModConfigVSlot(void *obj);
extern "C" void CDumpManModStartVSlot(void *obj);

#endif /* DUMP_MAN_MOD_H */
