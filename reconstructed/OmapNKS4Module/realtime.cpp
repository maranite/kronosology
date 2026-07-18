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
 *  CSTGThread - real rtwrap_pthread_* implementation.
 *
 *  SWAPPED IN (2026-07-18): this file previously modeled CSTGThread with a
 *  plain kernel_thread() + static-global-state stand-in, deferred pending a
 *  disassembly pass to pin down the real per-instance object layout (see the
 *  README's "Continued RE, 2026-07-17 (session 2)" entry). That pass is now
 *  done - see omapnks4_internal.h's CSTGThread block comment for the full
 *  per-field evidence (pTask @+0x00, bActive @+0x04, both confirmed via
 *  CreateRealTimeWithCPUAffinity/Delete/Wait disassembly in the correct
 *  89849-byte target @0x18b20/0x18bf0/0x18c20) - and these four methods now
 *  route through the real rtwrap_pthread_* layer rtwrap.cpp already
 *  provides, operating on that per-instance state instead of file-scope
 *  statics. No change was needed to CActiveSenseThread's constructor/
 *  destructor: the constructor never touches pTask (ground truth, @0x17b50 -
 *  it's populated later, by CreateRealTimeWithCPUAffinity), and the
 *  destructor's existing `sInstance=0; Delete();` sequence already matches
 *  the real destructor (@0x17bc0) instruction-for-instruction.
 *
 *  CreateRealTimeWithCPUAffinity's real parameter order is (fn, priority,
 *  cpumask, arg) - see omapnks4_internal.h for why this differs from what
 *  this reconstruction previously guessed (no "stack" parameter exists;
 *  0x5000 is hardcoded internally). GetMaxRealTimePriority is a plain static
 *  forward to rtwrap_get_max_priority() (@0x18c50 -> 0x18110), unchanged
 *  from a per-instance standpoint since it has no `this` at all. */

int CSTGThread::CreateRealTimeWithCPUAffinity(stg_thread_fn fn, int priority,
					      unsigned long cpumask, void *arg)
{
	unsigned int attr[4];	/* rtwrap_pthread_attr_t is exactly 4 u32 words / 16
				 * bytes (get_sizeof_rtwrap_pthread_attr() @0x18240
				 * returns 0x10; rtwrap.cpp's own attr_init/
				 * setrtpriority/setstacksize only ever touch
				 * attr[0..3]) - the real CreateRealTimeWithCPUAffinity
				 * allocates this on its own stack the same way. */

	/* Ground truth calls attr_init/setrtpriority/setstacksize back-to-back with
	 * no return-value checks on any of the three (@0x18b64-0x18b7a) - only
	 * pthread_create's result is ever tested. Matched here: attr_init always
	 * returns 0 anyway (@0x1826b: `xor eax,eax; ret`, unconditional). */
	rtwrap_pthread_attr_init(attr);
	rtwrap_pthread_attr_setrtpriority(attr, (unsigned int)priority);
	rtwrap_pthread_attr_setstacksize(attr, 0x5000);	/* hardcoded, ground truth @0x18b73 */

	/* out_task == this: rtwrap_pthread_create writes the new task handle
	 * straight into *this (pTask), ground truth @0x18b84/0x18b89. */
	int create_rc = rtwrap_pthread_create((void **)this, attr, (void (*)(void *))fn, arg);
	rtwrap_pthread_attr_destroy(attr);

	if (create_rc != 0)
		return 0;	/* create failed; pTask/bActive untouched */

	bActive = 1;
	if (rtwrap_set_debug_traps_in_rt_task() != 0) {
		/* debug-trap setup failed post-create: tear the task back down,
		 * ground truth @0x18bd0-0x18be8. */
		rtwrap_clear_debug_traps_in_rt_task();
		rtwrap_pthread_cancel((unsigned char *)pTask);
		bActive = 0;
		return 0;
	}
	rtwrap_set_runnable_on_cpuid(pTask, cpumask);
	return 1;
}

void CSTGThread::Delete(void)
{
	if (!bActive)
		return;
	rtwrap_clear_debug_traps_in_rt_task();
	rtwrap_pthread_cancel((unsigned char *)pTask);
	bActive = 0;
}

void CSTGThread::Wait(void)
{
	if (!bActive)
		return;
	rtwrap_clear_debug_traps_in_rt_task();
	rtwrap_pthread_join((unsigned char *)pTask, 0);
	bActive = 0;
}

int CSTGThread::GetMaxRealTimePriority(void) { return rtwrap_get_max_priority(); }

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
 *
 * CORRECTION (fresh adversarial pass, 2026-07-18): the SRQ call itself was wrong in
 * two ways, confirmed via disassembly of this exact method (@0x17660) in the correct
 * 89849-byte target. First, ground truth calls `rtwrap_pend_linux_srq()` here (`CALL
 * 0x18a50` @0x176a1) - the SAME wrapper the free-function
 * OmapNKS4Fifos_TriggerOutputInterrupt already correctly uses (get_xrefs_to 0x18a50
 * lists BOTH this class method and that free function, plus WriteCommand below and
 * ITS free-function wrapper, as its only 4 callers) - not `rt_pend_linux_srq(0)`
 * directly as this reconstruction previously had it. Second, and more importantly,
 * the argument is NOT a hardcoded 0: `MOV EAX,EBX` immediately before the CALL
 * (@0x17699) reloads EBX, which was set moments earlier from `dwEnabled` itself
 * (`MOV EBX,[this+0x514]` @0x1767a) - i.e. ground truth passes `dwEnabled` as the SRQ
 * number. This makes sense once you read `dwEnabled` as "0 if the SRQ was never
 * registered, else the real SRQ id `rt_request_srq()` returned" rather than a plain
 * boolean - Initialize(enable) (above) stores exactly this value verbatim. The
 * previous `rt_pend_linux_srq(0)` would have raised SRQ 0 (almost certainly the
 * wrong SRQ, or a no-op/misdirected signal) on every real invocation instead of the
 * module's actual registered SRQ. */
void CSTGOmapNKS4Fifos::TriggerOutputInterrupt(void)
{
	unsigned long long now = omapnks4_rdtsc();

	if (dwEnabled &&
	    (outputFifo.dwWriteIndex != outputFifo.dwReadIndex || outputFifo.bPending)) {
		s_ullLastTickSRQ = now;
		rtwrap_pend_linux_srq(dwEnabled);
	}
}

/* Enqueue one 32-bit command into the host->panel output ring (64 deep).
 *
 * CORRECTION (fresh adversarial pass, 2026-07-18): same SRQ bug as
 * TriggerOutputInterrupt above, independently confirmed via this method's own
 * disassembly (@0x176d0): the real call is `rtwrap_pend_linux_srq(dwEnabled)`
 * (`CALL 0x18a50` @0x17755, with EAX reloaded from a stack-saved copy of the
 * second `dwEnabled` read @0x1771e/0x172b - not a plain `rt_pend_linux_srq(0)`. */
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
			rtwrap_pend_linux_srq(CSTGOmapNKS4Fifos::sInstance.dwEnabled);
		}
	}
	return ret;
}

/*
 * Free-function C-ABI wrappers over the CSTGOmapNKS4Fifos/*Fifo singleton -
 * ground truth (fresh Ghidra decompile, 2026-07-17). These are the symbols other
 * modules' generated code actually calls (OmapNKS4Init, ReceiveEventBuffer, etc.),
 * distinct from - but logically equivalent to - the class methods above.
 */

/* Initialize(enable) split into a free function taking the same bool; sets the two
 * SRQ-latency watermarks identically to the class method (same *180000/*2000
 * constants, confirmed identical at this call site too). */
extern "C" void CSTGOmapNKS4Fifos_Initialize(int enable)
{
	CSTGOmapNKS4Fifos::sInstance.Initialize(enable);
}

/* Ground truth: logically identical gating to CSTGOmapNKS4Fifos::TriggerOutputInterrupt
 * (dwEnabled && (writeIndex!=readIndex || bPending)) -> raise SRQ via
 * `rtwrap_pend_linux_srq(dwEnabled)` - CORRECTION (fresh adversarial pass, 2026-07-18):
 * this comment previously claimed the class method calls `rt_pend_linux_srq(0)`
 * directly, unlike this free function's `rtwrap_pend_linux_srq()` - that claim was
 * wrong on a fresh re-disassembly of the class method (@0x17660): it also calls
 * `rtwrap_pend_linux_srq()`, and BOTH pass `dwEnabled` itself as the SRQ number, not a
 * literal 0. Now genuinely the same call, same argument, just one delegates through
 * the other rather than duplicating it - see TriggerOutputInterrupt's own comment for
 * the full disassembly evidence. */
extern "C" void OmapNKS4Fifos_TriggerOutputInterrupt(void)
{
	CSTGOmapNKS4Fifos::sInstance.TriggerOutputInterrupt();
}

/* Pop one command from the 256-deep host<-panel input ring if non-empty. Ground truth:
 * the real disassembly indexes `(&CSTGOmapNKS4Fifos::sInstance)[idx]` as a flat
 * uint32 array - exactly `sInstance.inputFifo.pRing[idx]`, since pRing is the first
 * member at offset 0 of the whole singleton. */
extern "C" int OmapNKS4InputFifo_ReadCommand(unsigned int *out)
{
	struct CSTGOmapNKS4InputFifo *fifo = &CSTGOmapNKS4Fifos::sInstance.inputFifo;

	if (fifo->dwReadIndex == fifo->dwWriteIndex)
		return 0;
	*out = fifo->pRing[fifo->dwReadIndex & 0xff];
	fifo->dwReadIndex++;
	return 1;
}

/* Free-function form of CSTGOmapNKS4OutputFifo::WriteCommand.
 * CORRECTION (full-coverage re-audit, 2026-07-18): the 2026-07-17 comment below
 * claiming a real return-value divergence does NOT hold up under a fresh,
 * complete instruction-by-instruction trace of the real free function
 * (@0x17830-0x178e0). Ground truth IS an independently-compiled duplicate (not
 * a compiled-down call to the class method - confirmed, its own body inlines
 * the whole gate+push+SRQ sequence again), but every one of its reachable exit
 * paths sets EAX the same way this delegating implementation's return value
 * already does: dwEnabled==0 -> 0; ring full ((writeIndex-readIndex)>0x3f) -> 0;
 * otherwise -> 1, via one of two `MOV EAX,0x1` sites (@0x178b0 after the normal
 * writeIndex!=readIndex SRQ-raise path, @0x178cf in the writeIndex==readIndex/
 * bPending path) that both funnel into the same `1` return regardless of
 * whether bPending is set - i.e. even the "impractical 32-bit wraparound" edge
 * case this file's own prior comment hedged on turns out to also return 1, not
 * something else. There is no return-value divergence anywhere, reachable or
 * not - the previous comment's claim was simply incorrect on fresh inspection,
 * not just "impractical to observe".
 *
 * One real (but immaterial) internal-implementation difference DOES exist:
 * ground truth's free function reads dwEnabled exactly ONCE, at function
 * entry (`mov ecx,[dwEnabled]` @0x1783a), and reuses that same register value
 * as rtwrap_pend_linux_srq's argument later (`mov eax,ecx` @0x178a3) - it never
 * re-reads dwEnabled from memory before deciding whether to raise the SRQ,
 * unlike the class method (WriteCommand@0x176d0) which explicitly re-reads it
 * a second time (`mov ecx,[dwEnabled]` @0x1771e) as part of its own inner SRQ
 * gate. Since nothing inside either function ever writes dwEnabled, a stale
 * vs. fresh read of the same never-changing-mid-call value cannot produce a
 * different outcome in practice - noted here as the one genuine byte-level
 * difference between the two compiled bodies, not a functional bug. */
extern "C" int OmapNKS4OutputFifo_WriteCommand(unsigned int cmd)
{
	return CSTGOmapNKS4Fifos::sInstance.outputFifo.WriteCommand(cmd);
}

/* ========================================================================= *
 *  CActiveSenseThread  (heap singleton)
 * ========================================================================= */

CActiveSenseThread *CActiveSenseThread::sInstance;

/* Block until 'qwNextTickCycles', re-reading the deadline each nap (Ping() can move
 * it forward from another context).
 *
 * Cross-checked against fresh disassembly (ThreadRoutine@0x17be0/0x18fa0, correct
 * 89849-byte target, 2026-07-18): logic, field offsets, and reload-on-each-nap
 * behavior all match exactly. Two minor, non-behavior-changing observations, left
 * as-is: (1) ground truth casts the 64-bit cycle delta to `float` before
 * multiplying by flNanosPerCycle (`(float)delta * flNanosPerCycle`), not `double` as
 * here - immaterial for any realistic cpu_khz on this Atom-class hardware (the
 * resulting delta values stay well under float32's 2^24 exact-integer bound, so the
 * wider double-precision path here is a strict superset of correctness, not a
 * behavior change). (2) ground truth calls `rtwrap_nano2count()`/`rtwrap_sleep()`
 * rather than `nano2count()`/`rt_sleep()` directly - confirmed via rtwrap.cpp's own
 * definitions to be pure 1:1 passthroughs with no argument or behavior difference,
 * unlike the CSTGOmapNKS4Fifos SRQ wrapper case above where the indirection
 * actually mattered. */
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
			rt_sleep(nano2count(nanos));
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

/* CORRECTION (full-coverage re-audit, 2026-07-18): re-confirmed via fresh disassembly
 * of CActiveSenseThread_Setup's own CreateRealTimeWithCPUAffinity call site
 * (@0x17e18: `mov edx,0x18fa0` loading the `fn` argument) that there is NO real
 * "OmapNKS4_ActiveSenseThreadEntry" symbol anywhere in this binary - confirmed absent
 * from a full listing of every real "Thread"-named function (38 entries). Ground
 * truth passes the address of `ThreadRoutine`'s own regparm3 GCC clone (@0x18fa0,
 * Ghidra's own name for it is literally "ThreadRoutine", not a distinct trampoline
 * name) DIRECTLY as the RT task entry point - no separate wrapper call at all. This
 * works because a thiscall method (`this` in EAX) and a plain regparm3 one-pointer-
 * argument free function are bit-for-bit the same calling convention on this ABI, so
 * the compiler's own IPA cloning produced a second copy of ThreadRoutine's body
 * shaped exactly like what an RT-task entry point needs, and CreateRealTimeWithCPUAffinity's
 * caller just points at it directly. This free function is therefore a reconstruction-
 * only indirection standing in for that direct clone reference - it reproduces the
 * exact same observable effect (arg cast to CActiveSenseThread*, ThreadRoutine's body
 * runs) at the cost of one extra call/ret frame around the RT task's entry, not a
 * functional divergence. Kept as its own named function (rather than contorting this
 * reconstruction into taking a member-function-pointer's address, which isn't portable
 * C++ even though it happens to work on this exact ABI) since nothing observable
 * depends on the extra frame. */
extern "C" void *OmapNKS4_ActiveSenseThreadEntry(void *arg)
{
	((CActiveSenseThread *)arg)->ThreadRoutine();
	return 0;
}

/*
 * Ground truth (fresh Ghidra decompile, 2026-07-17): these are real, plain free
 * functions in the binary - `CActiveSenseThread_Setup`/`CActiveSenseThread_Cleanup`
 * (0x17da0/0x17e40) - NOT mangled `CActiveSenseThread::Setup`/`::Cleanup` static
 * methods. No such mangled symbols exist anywhere in the object; this
 * reconstruction previously modeled them as C++ statics, which happened to compile
 * and behave identically but didn't match the real exported names. Renamed to
 * plain C functions to match. Logic unchanged - already verified equivalent to the
 * real disassembly (heap-alloc via `operator new`, `CSTGThread::GetMaxRealTimePriority`/
 * `CreateRealTimeWithCPUAffinity`, and the double-Delete-guard pattern respectively).
 *
 * CORRECTED (2026-07-18, CSTGThread pass): the CreateRealTimeWithCPUAffinity
 * call below previously used this reconstruction's old (wrong) parameter
 * order/count and called GetMaxRealTimePriority() TWICE via a comma-operator
 * (an artifact with no ground-truth basis - the real call site @0x17e10-
 * 0x17e2a calls it exactly once). Ground truth: priority is
 * GetMaxRealTimePriority()-10 (`lea ecx,[eax-0xa]` @0x17e15), cpumask is the
 * literal 0 (`mov [esp],0` @0x17e23), and arg is `t` itself (`mov [esp+4],ebx`
 * @0x17e1f) - OmapNKS4_ActiveSenseThreadEntry casts that arg back to
 * CActiveSenseThread* and calls ->ThreadRoutine() on it.
 */
extern "C" bool CActiveSenseThread_Setup(void)
{
	CActiveSenseThread *t = new CActiveSenseThread();

	CActiveSenseThread::sInstance = t;
	if (!t)
		return false;
	int priority = CSTGThread::GetMaxRealTimePriority() - 10;
	return ((CSTGThread *)t)->CreateRealTimeWithCPUAffinity(
			OmapNKS4_ActiveSenseThreadEntry, priority, 0, t) != 0;
}

extern "C" void CActiveSenseThread_Cleanup(void)
{
	if (CActiveSenseThread::sInstance) {
		((CSTGThread *)CActiveSenseThread::sInstance)->Delete();
		if (CActiveSenseThread::sInstance) {
			CActiveSenseThread *t = CActiveSenseThread::sInstance;
			CActiveSenseThread::sInstance = 0;
			((CSTGThread *)t)->Delete();
			operator delete(t);
		}
	}
}

/* Real symbol name is CActiveSenseThread_Ping (0x17e90) - matches this file's own
 * earlier citation of that address; DoPing was this reconstruction's placeholder
 * name for the same thing before the real name was confirmed. */
extern "C" void CActiveSenseThread_Ping(void)
{
	if (CActiveSenseThread::sInstance)
		CActiveSenseThread::sInstance->Ping();
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
