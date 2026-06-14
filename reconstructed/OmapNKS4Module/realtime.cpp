// SPDX-License-Identifier: GPL-2.0
/*
 * realtime.cpp  -  OmapNKS4 real-time subsystem
 *
 *   - CSTGOmapNKS4Fifos / CSTGOmapNKS4OutputFifo : command/event ring buffers shared
 *     with the Linux-side reader via an RTAI SRQ (software interrupt request).
 *   - CActiveSenseThread : RTAI real-time thread that fires the panel output IRQ on a
 *     fixed tick (active sensing / output pacing).
 *   - CNKS4EventFilter : sustain-pedal aware event suppression.
 */

#include "omapnks4_internal.h"

/* ========================================================================= *
 *  CSTGOmapNKS4Fifos  (singleton)
 * ========================================================================= */

struct CSTGOmapNKS4Fifos CSTGOmapNKS4Fifos::sInstance;

/* timing watermarks computed at Initialize() from the CPU clock. */
static unsigned long long s_ullCheckStartCycles;
static unsigned long long s_ullMaxIntervalCycles;
/* TSC of the last SRQ we raised (used by the Linux-side latency check). */
static unsigned long long s_ullLastTickSRQ;

void CSTGOmapNKS4Fifos::Initialize(int enable)
{
	dwEnabled = enable;
	s_ullCheckStartCycles = (unsigned long long)stg_get_cpu_khz() * 180000;
	s_ullMaxIntervalCycles = (unsigned long long)stg_get_cpu_khz() * 2000;
}

/*
 * Raise the Linux SRQ so the kernel-side reader drains the output FIFO - but only if
 * the FIFO actually has work pending.
 */
void CSTGOmapNKS4Fifos::TriggerOutputInterrupt(void)
{
	unsigned long long now = omapnks4_rdtsc();

	if (dwEnabled &&
	    (outputFifo.dwWriteIndex != outputFifo.dwReadIndex || outputFifo.bPending)) {
		s_ullLastTickSRQ = now;
		rtwrap_pend_linux_srq(0);
	}
}

/* Enqueue one 32-bit command into the host->panel output ring (64 deep). */
int CSTGOmapNKS4OutputFifo::WriteCommand(unsigned int cmd)
{
	int ret = 0;

	if (CSTGOmapNKS4Fifos::sInstance.dwEnabled &&
	    (dwWriteIndex - dwReadIndex) < 0x40) {
		pRing[dwWriteIndex & 0x3f] = cmd;
		dwWriteIndex++;
		ret = 1;
		if (CSTGOmapNKS4Fifos::sInstance.dwEnabled &&
		    (dwWriteIndex != dwReadIndex || bPending)) {
			s_ullLastTickSRQ = omapnks4_rdtsc();
			rtwrap_pend_linux_srq(0);
		}
	}
	return ret;
}

/* ========================================================================= *
 *  CActiveSenseThread  (heap singleton)
 * ========================================================================= */

CActiveSenseThread *CActiveSenseThread::sInstance;

/* Block until 'qwNextTickCycles', re-reading the deadline each nap (Ping() can move
 * it forward from another context). */
static void wait_until_deadline(CActiveSenseThread *t)
{
	unsigned long long next = t->qwNextTickCycles;

	for (;;) {
		long long delta = (long long)(next - omapnks4_rdtsc());
		if (delta <= 0)
			return;
		if ((long long)((double)delta * t->flNanosPerCycle) > 0) {
			unsigned long long nanos =
				(unsigned long long)((double)delta * t->flNanosPerCycle);
			rtwrap_sleep(rtwrap_nano2count(nanos));
			next = t->qwNextTickCycles;
		}
	}
}

CActiveSenseThread::CActiveSenseThread(void)
{
	unsigned int khz = stg_get_cpu_khz();

	sInstance = this;
	bActive = 0;
	qwIntervalCycles = (unsigned long long)khz * 500;	/* 500 ms tick */
	flNanosPerCycle = OMAPNKS4_NANOS_PER_MS / (float)khz;
	qwNextTickCycles = (unsigned long long)khz * 500 + omapnks4_rdtsc();
}

CActiveSenseThread::~CActiveSenseThread(void)
{
	sInstance = 0;
	((CSTGThread *)this)->Delete();
}

/* RT loop: at each deadline, mark the output FIFO pending and kick the output IRQ. */
void CActiveSenseThread::ThreadRoutine(void)
{
	for (;;) {
		wait_until_deadline(this);
		qwNextTickCycles = omapnks4_rdtsc() + qwIntervalCycles;
		CSTGOmapNKS4Fifos::sInstance.outputFifo.bPending = 1;
		CSTGOmapNKS4Fifos::sInstance.TriggerOutputInterrupt();
	}
}

void CActiveSenseThread::Sleep(void)
{
	wait_until_deadline(this);
}

/* Push the next deadline one interval out from now (called when real traffic flows,
 * so the active-sense tick only fires during idle). */
void CActiveSenseThread::Ping(void)
{
	qwNextTickCycles = omapnks4_rdtsc() + qwIntervalCycles;
}

/* C entry the RT scheduler jumps to (free function ThreadRoutine @0x8fa0). */
extern "C" void *OmapNKS4_ActiveSenseThreadEntry(void *arg)
{
	((CActiveSenseThread *)arg)->ThreadRoutine();
	return 0;
}

bool CActiveSenseThread::Setup(void)
{
	CActiveSenseThread *t = new CActiveSenseThread();

	sInstance = t;
	if (!t)
		return false;
	return CSTGThread::GetMaxRealTimePriority(),
	       ((CSTGThread *)t)->CreateRealTimeWithCPUAffinity(
			OmapNKS4_ActiveSenseThreadEntry, t,
			CSTGThread::GetMaxRealTimePriority(), 0, 0) != 0;
}

void CActiveSenseThread::Cleanup(void)
{
	if (sInstance) {
		((CSTGThread *)sInstance)->Delete();
		if (sInstance) {
			CActiveSenseThread *t = sInstance;
			sInstance = 0;
			((CSTGThread *)t)->Delete();
			operator delete(t);
		}
	}
}

void CActiveSenseThread::DoPing(void)
{
	if (sInstance)
		sInstance->Ping();
}

/* ========================================================================= *
 *  CNKS4EventFilter
 *
 *  Drops events while a sustain ("damper") gesture is active, except it lets the
 *  final note-off-equivalent through when the pedal is released.  'cmd' is the 32-bit
 *  protocol word; the 0x1x sub-opcode in byte 2 carries the sustain on/off.
 * ========================================================================= */

unsigned char CNKS4EventFilter::FilterEvent(unsigned int cmd)
{
	if (!bEnabled)
		return 0;

	if ((cmd >> 0x18) == 0 && ((cmd >> 0x10) & 0xf0) == 0x10) {
		unsigned int sub = (cmd >> 0x10) & 0x0f;
		unsigned char prev = bSustainState;
		unsigned char now = prev;

		if (sub == 1) {				/* sustain ON  */
			if (!bSuppressAll) {
				bSustainState = 1;
				now = 1;
			}
		} else if (sub == 2) {			/* sustain OFF */
			bSustainState = 0;
			now = 0;
		}
		if (prev && !now)			/* released: flush */
			return 1;
	}
	return bSuppressAll ^ 1;
}
