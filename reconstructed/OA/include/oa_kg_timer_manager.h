// SPDX-License-Identifier: GPL-2.0
/*
 * oa_kg_timer_manager.h  -  CKGTimerManager: KARMA's own tempo/clock
 * manager, tracking elapsed ticks, external MIDI-clock sync, and the
 * front-panel tempo-LED flash cadence.
 *
 * FOUND 2026-07-29 (round 50, solo -- session-wide 200-subagent dispatch
 * cap hit, see PROJECT_BRAIN/status.md). Confirmed via
 * /home/share/Decomp/oa_export's own per-function decompiles + symbols.csv
 * mangled-name cross-check for every method below.
 *
 * Confirmed real object layout (this project's regparm(3) ABI: `this` is
 * EAX, ground truth's own decompile shows it as an unused declared
 * parameter with the real body reading an `in_EAX` pseudo-variable --
 * same gotcha already documented in oa_ckg_midi_msg_handler.h). Byte
 * ranges NOT listed below (+0x0c..+0x13, +0x28..+0x2f) are real fields
 * this batch never touches -- only exercised by `Process()`/
 * `AdvanceClock()` (deliberately deferred, see below) -- modeled as
 * opaque padding to preserve every OTHER field's real offset for
 * whichever future batch reconstructs those two.
 *   +0x00  mInitialized (bool) -- first-call latch for the interval-
 *          clock accumulator (`GetIntervalClock`/`AdvanceClock`/
 *          `Process` all gate on it)
 *   +0x04  mLedFlashCountdown (int) -- `ShouldTempoLEDFlash`'s own
 *          countdown, reset to 0x1e0 (480) on expiry; ALSO written by
 *          the deferred `Process`/`AdvanceClock`'s karma-tick inner
 *          loop (reset to -0x20 there, a different real quirk out of
 *          this batch's scope)
 *   +0x08  mCurrentTempo (int) -- BPM, updated by
 *          `SetTempo`/`SetCurrentTempo`/`ChangePerformance` only when
 *          the new value differs (each then calls `RT_pe_tempo`)
 *   +0x14  mTempoPercent (int) -- `SetTempoPercent`'s own field, ctor
 *          defaults to 100; used as the numerator of a `/100`
 *          fixed-point tick-scaling ratio by `GetIntervalClock`/
 *          `GetKarmaIntervalClock`
 *   +0x18  mLastElapsedTick (int) -- previous `mElapsedTick` snapshot,
 *          used to compute the delta each `GetIntervalClock` call
 *   +0x1c  mFracRemainder (unsigned int) -- mod-100 carry accumulator
 *          for the fixed-point tick-scaling division
 *   +0x20  mElapsedTick (int) -- incremented by `IncElapsedTick`/
 *          `ReceiveMIDIClock`
 *   +0x24  mExternalClockBacklog (int) -- MIDI-clock-sync backlog
 *          counter; `ReceiveMIDIClock` resets it to 0x14 (20),
 *          `IncElapsedTick` drains it by 1 per call while syncing
 *          external
 *   +0x30  mNextDueTimeUs (unsigned long long) -- a monotonic
 *          microsecond due-time, ctor-initialized from
 *          `KGOutGate_GetCurrentTimeUs()`; only read/advanced by the
 *          deferred `Process()` (1ms-stride scheduler due-time, manual
 *          32+32 carry math) -- NOT touched by any method this batch
 *          reconstructs, kept here only to preserve the real field
 *          offset/size for that future batch.
 *
 * `Process()`/`AdvanceClock()` deliberately NOT reconstructed --
 * genuinely ambiguous register/stack allocation this project's own
 * "genuinely-unresolvable decompile recognition" convention flags for
 * deferral: both bodies read `in_stack_ffffffe0`/`in_stack_ffffffe4`/
 * `unaff_EBX` pseudo-variables (Ghidra's own markers for values that
 * are read from stack slots or registers NEVER assigned anywhere within
 * the function -- i.e. genuinely inherited from an unknown caller
 * context, not a simple "this is EAX" register gotcha) to construct a
 * `CKGRTCHandler*` and call `FlashBufferdValue()`/`StartBuffering()` on
 * it, plus a `CKGEngine::IsKarmaOn(CKGEngine*, int)` call with the same
 * ambiguous pointer -- resolving this needs raw disassembly tracing of
 * the real caller context, not just the auto-decompile. Both functions'
 * OTHER logic (the fixed-point interval-clock accumulator, tempo-LED
 * countdown wiring) is IDENTICAL to the already-reconstructed
 * `GetIntervalClock`/`ShouldTempoLEDFlash` below, so nothing new would
 * be learned by guessing at the ambiguous part.
 */

#ifndef OA_KG_TIMER_MANAGER_H
#define OA_KG_TIMER_MANAGER_H

extern "C" unsigned long long KGOutGate_GetCurrentTimeUs(void) __attribute__((regparm(3)));
extern "C" bool KGOutGate_ShouldSyncExternal(void) __attribute__((regparm(3)));
extern "C" int  KGOutGate_GetTempoWhenSyncInternal(void) __attribute__((regparm(3)));
extern "C" int  KGOutGate_GetTempoWhenSyncExternal(void) __attribute__((regparm(3)));
extern "C" void RT_pe_tempo(unsigned short tempo) __attribute__((regparm(3)));
extern "C" bool SKSTGGate_GetDebugMode(void) __attribute__((regparm(3)));
extern "C" void RT_clock_synchronize(bool sync) __attribute__((regparm(3)));
extern "C" unsigned int KS_get_karma_ticks_til_beat_480(void) __attribute__((regparm(3)));

class CKGTimerManager {
public:
	CKGTimerManager();

	/* Process()/AdvanceClock() deliberately declared but NOT defined
	 * this round -- see header comment. `Process()` must stay declared
	 * (real caller: CKGEngine::Update(), ckg_engine.cpp) even though
	 * its own body isn't reconstructed; this project's kernel-module
	 * link model tolerates an unresolved internal method symbol at
	 * build time exactly like any other genuinely-unresolved extern
	 * (resolved, or not, only at insmod time). */
	void Process();
	void AdvanceClock();

	void ChangePerformance();
	void IncElapsedTick();
	void ReceiveMIDIClock();
	bool ShouldTempoLEDFlash();
	void SetTempo(int tempo);
	void SetCurrentTempo();
	void SetTempoPercent(unsigned long percent);
	int GetKarmaIntervalClock(unsigned long deltaTicks);
	int GetIntervalClock();
	unsigned int GetTicksUntilTheBeat(bool wrapNegative);

	static void StartSync();
	static void StopSync();

private:
	unsigned char mInitialized;		/* +0x00 */
	unsigned char mPad04[3];		/* +0x01 */
	int mLedFlashCountdown;			/* +0x04 */
	int mCurrentTempo;			/* +0x08 */
	unsigned char mUnknown0xc[8];		/* +0x0c, see header comment */
	int mTempoPercent;			/* +0x14 */
	int mLastElapsedTick;			/* +0x18 */
	unsigned int mFracRemainder;		/* +0x1c */
	int mElapsedTick;			/* +0x20 */
	int mExternalClockBacklog;		/* +0x24 */
	unsigned char mUnknown0x28[8];		/* +0x28, see header comment */
	unsigned long long mNextDueTimeUs;	/* +0x30 */
};

#endif /* OA_KG_TIMER_MANAGER_H */
