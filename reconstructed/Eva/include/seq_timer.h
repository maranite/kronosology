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
 * `CTimerEngine` (.text+0x0816bf40, 181 bytes) is now ALSO fully reconstructed
 * (2026-07-25 follow-up, see timer_engine.h): its ctor/dtor and its 3 embedded
 * sub-objects (`CWheelsContainer`/`CExternalClock`/`CInternalClock`) are real.
 * The ~24 FURTHER `CTimerEngine` methods per `nm -C` (`CreateWheel`/`StartWheel`/
 * `SetSyncInput`/`RunVirtualTime`/...) remain genuinely out of scope -- a deeper
 * sequencer clock/wheel engine nothing on this pass's traced boot path calls.
 */

#ifndef SEQ_TIMER_H
#define SEQ_TIMER_H

#include "module.h"
#include "task.h"
#include "timer_engine.h"

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
