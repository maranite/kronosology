// SPDX-License-Identifier: GPL-2.0
/*
 * kg_timer_manager.cpp  -  CKGTimerManager method bodies (round 50,
 * solo). See include/oa_kg_timer_manager.h for the full object-layout
 * derivation and the deliberately-deferred Process()/AdvanceClock()
 * note.
 */
#include "oa_kg_timer_manager.h"
#include "oa_ckg_module_param_msg_handler.h" /* CKGEngine, ms_poInstance */

CKGTimerManager::CKGTimerManager()
{
	/* Real ctor never initializes mCurrentTempo (+0x08) or
	 * mLastElapsedTick (+0x18) -- a genuine ground-truth quirk. Zeroed
	 * here only for this reconstruction's own UB-safety (matching this
	 * project's established convention, e.g. CSPRUIMsgSender's 4-byte
	 * gap), not because ground truth zeroes them. */
	mCurrentTempo = 0;
	mLastElapsedTick = 0;

	mTempoPercent = 100;
	mLedFlashCountdown = 0;
	mInitialized = 0;
	mFracRemainder = 0;
	mNextDueTimeUs = KGOutGate_GetCurrentTimeUs();
	mElapsedTick = 0;
	mExternalClockBacklog = 0;
}

void CKGTimerManager::ChangePerformance()
{
	mLedFlashCountdown = 0;
	int tempo = KGOutGate_ShouldSyncExternal() ? KGOutGate_GetTempoWhenSyncExternal()
						    : KGOutGate_GetTempoWhenSyncInternal();
	if (tempo != mCurrentTempo) {
		mCurrentTempo = tempo;
		RT_pe_tempo((unsigned short)tempo);
	}
}

void CKGTimerManager::IncElapsedTick()
{
	if (KGOutGate_ShouldSyncExternal()) {
		if (mExternalClockBacklog > 0) {
			mElapsedTick++;
			mExternalClockBacklog--;
		}
		return;
	}
	mElapsedTick++;
}

void CKGTimerManager::ReceiveMIDIClock()
{
	if (KGOutGate_ShouldSyncExternal()) {
		if (mExternalClockBacklog > 0)
			mElapsedTick += mExternalClockBacklog;
		mExternalClockBacklog = 0x14;
	}
}

bool CKGTimerManager::ShouldTempoLEDFlash()
{
	int v = mLedFlashCountdown - 1;
	mLedFlashCountdown = v;
	if (v < 1)
		mLedFlashCountdown = 0x1e0;
	return v < 1;
}

void CKGTimerManager::SetTempo(int tempo)
{
	if (mCurrentTempo != tempo) {
		mCurrentTempo = tempo;
		RT_pe_tempo((unsigned short)tempo);
	}
}

void CKGTimerManager::SetCurrentTempo()
{
	int tempo = KGOutGate_ShouldSyncExternal() ? KGOutGate_GetTempoWhenSyncExternal()
						    : KGOutGate_GetTempoWhenSyncInternal();
	if (tempo != mCurrentTempo) {
		mCurrentTempo = tempo;
		RT_pe_tempo((unsigned short)tempo);
	}
}

void CKGTimerManager::SetTempoPercent(unsigned long percent)
{
	mTempoPercent = (int)percent;
}

int CKGTimerManager::GetKarmaIntervalClock(unsigned long deltaTicks)
{
	int count = 0;
	unsigned int acc = (unsigned int)deltaTicks * (unsigned int)mTempoPercent + mFracRemainder;
	while (acc > 99) {
		count++;
		acc -= 100;
	}
	mFracRemainder = acc;
	return count;
}

int CKGTimerManager::GetIntervalClock()
{
	int elapsed = mElapsedTick;
	if (!mInitialized) {
		mLastElapsedTick = elapsed;
		mInitialized = 1;
		return 0;
	}
	int prev = mLastElapsedTick;
	mLastElapsedTick = elapsed;
	int count = 0;
	unsigned int acc = (unsigned int)((elapsed - prev) * mTempoPercent) + mFracRemainder;
	while (acc > 99) {
		count++;
		acc -= 100;
	}
	mFracRemainder = acc;
	return count;
}

unsigned int CKGTimerManager::GetTicksUntilTheBeat(bool wrapNegative)
{
	bool stopped = ((CKGEngine *)CKGEngine::ms_poInstance)->HaveAllModulesStopped();
	unsigned int ticks = 0;
	if (!stopped) {
		ticks = KS_get_karma_ticks_til_beat_480() & 0xffff;
		if (ticks > 0x1e0)
			ticks %= 0x1e0;
		if (wrapNegative) {
			if (ticks > 0x1a3)
				ticks -= 0x1e0;
			return ticks;
		}
	}
	return ticks;
}

void CKGTimerManager::StartSync()
{
	/* Real body discards SKSTGGate_GetDebugMode()'s return value --
	 * transcribed verbatim, not "cleaned up" to remove the dead call. */
	SKSTGGate_GetDebugMode();
	RT_clock_synchronize(true);
}

void CKGTimerManager::StopSync()
{
	SKSTGGate_GetDebugMode();
	RT_clock_synchronize(false);
}
