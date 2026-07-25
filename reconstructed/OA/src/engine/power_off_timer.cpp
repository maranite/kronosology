// SPDX-License-Identifier: GPL-2.0
/*
 * power_off_timer.cpp  -  CPowerOffTimer's remaining 7 methods (batch
 * 2026-07-25). CPowerOffTimer/sInstance, its constructor, and
 * Initialize() were already real (managers.cpp / engine_startup_bits2.
 * cpp) -- see oa_engine.h's class comment for the confirmed field
 * layout every method below shares. This is the Kronos "Auto Power
 * Off" inactivity timer: every front-panel input handler already sets
 * `sInstance`'s +0x00 activity flag (front_panel_handlers.cpp,
 * confirmed real), and this file's DoTimerTick() is the periodic
 * driver that consumes it.
 *
 * Ground truth (objdump -dr -M intel against
 * /home/share/Decomp/OA.ko_Decomp/OA.ko):
 *   ReloadTimer()                  .text+0x5da30,  77B
 *   UpdateTimeoutValue(uint)       .text+0x5da80, 264B
 *   UpdateWarningThreshold(uint)   .text+0x5db90,  44B
 *   PowerOffPrepComplete()         .text+0x5dbc0,  37B
 *   DoTimerTick()                  .text+0x5dbf0, 249B
 *   BeginLongProcess()             .text+0x5dd80,  42B
 *   EndLongProcess()               .text+0x5ddb0,  95B
 *
 * All 7 confirmed real via a real caller/callee cross-check:
 * `CSTGAudioBusManager::sInstance->busGainScale` (already-confirmed
 * +0x4 field, 1500.0f) is the same tick-rate conversion factor
 * Initialize() already uses, and `PushUnsolicitedMessage()` (already
 * real) is called with the same {size=0x10,source=1,...} 16-byte
 * packet shape Initialize() already builds -- both cross-checks match
 * byte for byte.
 *
 * Outgoing message subtype constants (the packet's +0x8 dword, this
 * project's already-established convention for this shape -- see
 * Initialize()'s own message in engine_startup_bits2.cpp):
 *   0x27  entering "warning" state (state 1 -> 2)
 *   0x28  countdown expired ("critical", state -> 3)
 *   0x29  countdown reset/cancelled (state 2 -> 1, warning cleared)
 */

#include "oa_engine.h"
#include "oa_setup_global_resources.h"

extern "C" void PushUnsolicitedMessage(void *msg);
extern "C" void rtwrap_pthread_mutex_lock(void *mutex);
extern "C" void rtwrap_pthread_mutex_unlock(void *mutex);
extern "C" int OmapNKS4OutputFifo_WriteCommand(int command);

/* The mutex pointer at +0x18 is a real 32-bit field on target (a
 * 28-byte object, matching this project's own confirmed sizeof,
 * already written this way by the constructor in managers.cpp); on a
 * 64-bit host KAT build a raw `void*` read there would overrun the
 * object and read adjacent memory, so it is unpacked through this
 * project's established FromU32 convention instead of a direct
 * `*(void**)` cast (same host/target ABI hazard already hit and fixed
 * elsewhere in this project, e.g. midi_out_port_serial.cpp/
 * managers.cpp). Only FromU32 is needed here -- nothing in this file
 * writes the mutex field, only managers.cpp's constructor does. */
static void *FromU32(unsigned int v) { return (void *)(unsigned long)v; }

static void SendPowerOffUIMessage(unsigned int subtype)
{
	unsigned char msg[16] = { 0 };
	*(unsigned short *)(msg + 0x0) = 0x10;
	*(unsigned short *)(msg + 0x2) = 0x1;
	*(unsigned int *)(msg + 0x4) = 0;
	*(unsigned int *)(msg + 0x8) = subtype;
	*(unsigned int *)(msg + 0xc) = 0;
	PushUnsolicitedMessage(msg);
}

/*
 * ReloadTimer() -- unconditionally resets the countdown to its reload
 * value; if a warning was already showing (state==2), also tells the
 * UI to clear it.
 */
void CPowerOffTimer::ReloadTimer()
{
	unsigned char *this_ = reinterpret_cast<unsigned char *>(this);
	unsigned int ticksTotal = *(unsigned int *)(this_ + 0x8);
	*(unsigned int *)(this_ + 0x4) = ticksTotal;

	if (*(unsigned int *)(this_ + 0x14) == 2) {
		*(unsigned int *)(this_ + 0x14) = 1;
		SendPowerOffUIMessage(0x29);
	}
}

/*
 * UpdateTimeoutValue(seconds) -- reconfigures the reload value and
 * re-derives the warning lead time from the SAME 3 buckets Initialize()
 * uses (<=1800s: 120s lead; <=3600s: 180s lead; >3600s: 300s lead),
 * `seconds==0` disables the timer entirely (state=0, ticksTotal=-1,
 * still gets the 120s-lead bucket's field writes). Confirmed real: the
 * `state==2` "clear stale warning" check present in this bucket's
 * shared tail is provably UNREACHABLE from either of this function's
 * own two callers into it (`state` is always freshly set to 0 or 1
 * few instructions earlier on both paths that reach it) -- reproduced
 * verbatim anyway, matching this project's "preserve real quirks"
 * policy (same as Initialize()'s own analogous dead branch).
 */
void CPowerOffTimer::UpdateTimeoutValue(unsigned int timeoutSeconds)
{
	unsigned char *this_ = reinterpret_cast<unsigned char *>(this);
	CSTGAudioBusManager *bus = CSTGAudioBusManager::sInstance;

	unsigned int ticksTotal;
	if (timeoutSeconds == 0) {
		ticksTotal = 0xffffffffu;
		*(unsigned int *)(this_ + 0x8) = ticksTotal;
		*(unsigned int *)(this_ + 0x14) = 0;
	} else {
		ticksTotal = (unsigned int)((double)timeoutSeconds * bus->busGainScale);
		*(unsigned int *)(this_ + 0x8) = ticksTotal;
		*(unsigned int *)(this_ + 0x14) = 1;

		if (timeoutSeconds > 1800) {
			float leadSeconds = (timeoutSeconds <= 3600) ? 180.0f : 300.0f;
			*(int *)(this_ + 0xc) = (int)(leadSeconds * bus->busGainScale);
			*(unsigned int *)(this_ + 0x4) = ticksTotal;
			return;
		}
		/* timeoutSeconds in (0,1800]: fall through to the shared
		 * 120s-lead tail below, same as the ==0 path. */
	}

	*(int *)(this_ + 0xc) = (int)(120.0f * bus->busGainScale);
	*(unsigned int *)(this_ + 0x4) = ticksTotal;

	if (*(unsigned int *)(this_ + 0x14) == 2) {
		/* Confirmed real but unreachable from either path into
		 * this tail -- see file header note. */
		*(unsigned int *)(this_ + 0x14) = 1;
		SendPowerOffUIMessage(0x29);
	}
}

/* UpdateWarningThreshold(seconds) -- directly overrides the lead time
 * (in ticks), independent of the timeoutSeconds bucket logic above. */
void CPowerOffTimer::UpdateWarningThreshold(unsigned int thresholdSeconds)
{
	unsigned char *this_ = reinterpret_cast<unsigned char *>(this);
	CSTGAudioBusManager *bus = CSTGAudioBusManager::sInstance;
	*(int *)(this_ + 0xc) = (int)((double)thresholdSeconds * bus->busGainScale);
}

/*
 * PowerOffPrepComplete() -- called once the rest of the system has
 * finished reacting to the "critical" (state==3) UI warning; advances
 * to state 4 and sends the real hardware power-off command to the
 * OMAP/NKS4 subsystem. No-op if not currently in state 3 (e.g. called
 * twice, or the countdown was reset in the meantime).
 */
void CPowerOffTimer::PowerOffPrepComplete()
{
	unsigned char *this_ = reinterpret_cast<unsigned char *>(this);
	if (*(unsigned int *)(this_ + 0x14) != 3)
		return;
	*(unsigned int *)(this_ + 0x14) = 4;
	OmapNKS4OutputFifo_WriteCommand(0x9000000);
}

/*
 * DoTimerTick() -- the periodic driver (not yet wired to a real timer
 * source in this project). Confirmed real gate order:
 *   1. state==0 (disabled) -> no-op.
 *   2. longProcessCount!=0 (Begin/EndLongProcess bracket active) ->
 *      no-op.
 *   3. Atomically test-and-clear the +0x00 activity flag (every
 *      front-panel input handler sets it to 1, confirmed real in
 *      front_panel_handlers.cpp); if it WAS set, treat this tick as
 *      "user was active" and jump straight to the reset-and-return
 *      tail below instead of counting down.
 *   4. Three more independent suppression gates, each ALSO routing to
 *      the same reset-and-return tail: STGAPIFrontPanelStatus+0x109c
 *      nonzero (own meaning not independently determined -- plausibly
 *      "power-off inhibited"), CSTGMessageProcessor::sInstance's
 *      already-confirmed +0x48 byte nonzero (same "messages/UI busy"
 *      gate this project's own comport.h/keybed_init.h already
 *      document), CSTGGlobal::sInstance+0x6a8 in {1,2} (an
 *      unidentified transient system mode -- e.g. load/save in
 *      progress), and NKS4 test mode active.
 *   5. Otherwise: decrement the countdown. Hitting exactly 0 ->
 *      state=3 ("critical") + UI message 0x28. Still above the
 *      warning threshold -> nothing further. At or below it -> if not
 *      already in state 2, transition to it + UI message 0x27, then
 *      (unconditionally, whether newly entered or already there)
 *      update the front panel's live "seconds remaining" display
 *      field (STGAPIFrontPanelStatus+0x2911c) via a genuine INTEGER
 *      divide by `(int)busGainScale` (confirmed real -- ground truth
 *      truncates busGainScale to an int BEFORE dividing, not a float
 *      divide followed by truncation).
 *   Reset-and-return tail: ticksRemaining = ticksTotal; if state==2,
 *   also drop back to state 1 + UI message 0x29 (clear the warning).
 */
void CPowerOffTimer::DoTimerTick()
{
	unsigned char *this_ = reinterpret_cast<unsigned char *>(this);

	if (*(unsigned int *)(this_ + 0x14) == 0)
		return;
	if (*(unsigned int *)(this_ + 0x10) != 0)
		return;

	unsigned char wasActive = this_[0x0];
	this_[0x0] = 0; /* atomic test-and-clear in real ground truth (xchg) */

	bool reset = false;
	if (wasActive) {
		reset = true;
	} else if (*(unsigned int *)(STGAPIFrontPanelStatus::sInstance + 0x109c) != 0) {
		reset = true;
	} else if (((unsigned char *)CSTGMessageProcessor::sInstance)[0x48] != 0) {
		reset = true;
	} else {
		unsigned int mode = *(unsigned int *)((unsigned char *)CSTGGlobal::sInstance + 0x6a8);
		if (mode - 1 <= 1u)
			reset = true;
		else if (COmapNKS4Driver_GetTestMode())
			reset = true;
	}

	if (reset) {
		*(unsigned int *)(this_ + 0x4) = *(unsigned int *)(this_ + 0x8);
		if (*(unsigned int *)(this_ + 0x14) == 2) {
			*(unsigned int *)(this_ + 0x14) = 1;
			SendPowerOffUIMessage(0x29);
		}
		return;
	}

	unsigned int remaining = *(unsigned int *)(this_ + 0x4);
	if (remaining == 0)
		return;

	remaining--;
	*(unsigned int *)(this_ + 0x4) = remaining;

	if (remaining == 0) {
		*(unsigned int *)(this_ + 0x14) = 3;
		SendPowerOffUIMessage(0x28);
		return;
	}
	if (remaining > *(unsigned int *)(this_ + 0xc))
		return;

	if (*(unsigned int *)(this_ + 0x14) != 2) {
		*(unsigned int *)(this_ + 0x14) = 2;
		SendPowerOffUIMessage(0x27);
	}

	CSTGAudioBusManager *bus = CSTGAudioBusManager::sInstance;
	unsigned int secondsRemaining = remaining / (unsigned int)(int)bus->busGainScale;
	*(unsigned int *)(STGAPIFrontPanelStatus::sInstance + 0x2911c) = secondsRemaining;
}

/* BeginLongProcess()/EndLongProcess() -- mutex-guarded nesting counter
 * (+0x10) that DoTimerTick() checks to suppress ticking entirely while
 * nonzero. EndLongProcess() reaching exactly 0 also performs the same
 * "reset ticksRemaining + clear a state==2 warning" tail DoTimerTick()'s
 * own reset path uses -- i.e. finishing the last nested long process
 * gives the countdown a fresh full reload, same as any other detected
 * activity. */
void CPowerOffTimer::BeginLongProcess()
{
	unsigned char *this_ = reinterpret_cast<unsigned char *>(this);
	void *mutex = FromU32(*(unsigned int *)(this_ + 0x18));
	rtwrap_pthread_mutex_lock(mutex);
	(*(unsigned int *)(this_ + 0x10))++;
	rtwrap_pthread_mutex_unlock(mutex);
}

void CPowerOffTimer::EndLongProcess()
{
	unsigned char *this_ = reinterpret_cast<unsigned char *>(this);
	void *mutex = FromU32(*(unsigned int *)(this_ + 0x18));
	rtwrap_pthread_mutex_lock(mutex);

	unsigned int count = *(unsigned int *)(this_ + 0x10) - 1;
	*(unsigned int *)(this_ + 0x10) = count;

	if (count == 0) {
		*(unsigned int *)(this_ + 0x4) = *(unsigned int *)(this_ + 0x8);
		if (*(unsigned int *)(this_ + 0x14) == 2) {
			*(unsigned int *)(this_ + 0x14) = 1;
			SendPowerOffUIMessage(0x29);
		}
	}

	rtwrap_pthread_mutex_unlock(mutex);
}
