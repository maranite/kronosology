/*
 * seq_timer.h  -  CSeqTimer : public CModule, Stage 6 breadth sweep, 2026-07-25
 * (small-derived-module follow-up batch, see dump_man_mod.h / edit_man.h for the
 * shared "MMainXxx 9-member family" context).
 *
 * GROUND TRUTH: `MMainSeqTimer()` (mains.cpp, already Tier A) builds the base
 * `CModule("SequenceTimer")` and vtable-swaps in `PTR__CSeqTimer_08e892a8` --
 * unchanged, same "mains.cpp already produces the correct object" precedent as
 * every other sibling in this batch. `CSeqTimer::Setup()` (.text+0x08169310, 90
 * bytes) mallocs a real `CTimerEngine` (.text+0x0816bf40 ctor) and registers it
 * via the already-real `CModule::Add(CTask*)` -- same idiom as
 * `CDumpManMod::Setup()`/`CEditMan::Setup()`. It ALSO does one extra thing
 * neither of those 2 siblings do: dispatches through `SeqApi`'s own vtable slot
 * +0x20 with the new `CTimerEngine*` -- `SeqApi` is the real global
 * `ConstructSeqApiInstance()` sets up (mains.cpp), already a real,
 * EvaVTableStub-backed 20-slot array (`PTR__CSeqApiInstance_08e88fa8`,
 * omega_vtables.h) -- in range, safe, inert dispatch (matches this project's
 * "call is real, target is undecoded/inert" convention used throughout).
 * `Config()`/`Start()` (.text+0x081692f0/0x08169300, 3 bytes each) are confirmed
 * genuinely empty.
 *
 * `CTimerEngine` (.text+0x0816bf40, 181 bytes) is the ONE genuinely-too-deep
 * sibling task in this batch: beyond chaining into `CTask::CTask(owner,
 * "Engine", 2, 1, 0x804b)` and the usual opaque +0x08 identity overwrite, its
 * real ctor constructs THREE embedded sub-objects this project has no other
 * reason to build -- `CWheelsContainer`, `CExternalClock`, `CInternalClock`
 * (a real sequencer clock/wheel engine, ~24 further real `CTimerEngine` methods
 * per `nm -C`: `CreateWheel`/`StartWheel`/`SetSyncInput`/`RunVirtualTime`/...) --
 * plus 2 more real per-object vtable installs (`CSyncRXInterface`/
 * `CSyncTXInterface`). Modeled as a Tier-B stub deriving directly from `CTask`
 * (matching `CESCommonTask`'s own precedent, es_common.h) with the real,
 * confirmed ctor args and an otherwise-empty body -- `CWheelsContainer`/
 * `CExternalClock`/`CInternalClock` and the 24 further methods are NOT modeled.
 */

#ifndef SEQ_TIMER_H
#define SEQ_TIMER_H

#include "module.h"
#include "task.h"

/* Tier-B stub -- see file header. Real embedded CWheelsContainer/CExternalClock/
 * CInternalClock sub-objects and the ~24 further CTimerEngine methods are not
 * modeled.
 */
class CTimerEngine : public CTask {
public:
	explicit CTimerEngine(const CModule &owner)
		: CTask(owner, "Engine", 2, 1, 0x804b) {}
};

class CSeqTimer : public CModule {
public:
	/* .text+0x081693a0, 44 bytes. Real: takes a name argument (every real
	 * caller so far -- there is only 1, MMainSeqTimer -- passes a literal;
	 * ground truth's own real boot-path caller builds an equivalent object
	 * by hand instead of calling this, same "provided for structural
	 * completeness" status as CDumpManMod::CDumpManMod()).
	 */
	explicit CSeqTimer(const char *name);

	/* .text+0x08169310, 90 bytes. Real body -- see file header. */
	void Setup();

	/* .text+0x081692f0, 3 bytes. Confirmed genuinely `return 0;`. */
	void Config();

	/* .text+0x08169300, 3 bytes. Confirmed genuinely `return 0;`. */
	void Start();

private:
	CTimerEngine *mEngine; /* +0x2c */
};

extern "C" void CSeqTimerSetupVSlot(void *obj);
extern "C" void CSeqTimerConfigVSlot(void *obj);
extern "C" void CSeqTimerStartVSlot(void *obj);

#endif /* SEQ_TIMER_H */
