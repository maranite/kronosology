/*
 * omega_threads.cpp  -  OmegaSchedulingThread / OmegaInitThread / OmegaTimingThread,
 * the 3 worker bodies COmegaInterface::Init() spawns/calls (Stage 3).
 *
 * Transcribed from the Ghidra decompile export:
 *   OmegaSchedulingThread(void*)  .text+0x0804db70, 401 bytes
 *   OmegaInitThread(void*)        .text+0x0804dd10, 108 bytes
 *   OmegaTimingThread(void*)      .text+0x0804dd80, 297 bytes (real return type void)
 *
 * OmegaExitThread (.text+0x0804deb0, 447 bytes, right next to these in the export) is
 * NOT reconstructed here -- grepped for across all 37,795 exported function bodies
 * and found with zero callers anywhere in the binary. Not reachable from the traced
 * boot path (or, as far as this export shows, from anywhere at all); out of scope per
 * this batch's own "stay bounded" instruction.
 *
 * All 3 open with the same CPU-affinity-pin boilerplate seen in main() (eva_main.cpp)
 * -- zero a 128-byte mask, set bit 2, sched_setaffinity -- pinning every Omega worker
 * thread to CPU 2 alongside the main thread. Preserved literally (a fixed-size mask
 * write loop in the disassembly, replaced here with a real memset, same license as
 * eva_main.cpp's strcmp() replacement of an inlined byte-compare loop).
 */

#include "omega_globals.h"
#include "ckernel.h"

#include <csignal>
#include <cstring>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

/* .text+0x0804db50, 1 byte (a bare `ret`) -- real body is genuinely empty. sigwait()
 * (not sigaction+a real signal handler) is what OmegaSchedulingThread actually blocks
 * on below; this handler exists only so SIGUSR1 has *some* disposition installed
 * (sigwait() requires the signal to not be SIG_DFL-terminating) and is never expected
 * to run its body for real.
 */
extern "C" void sched_sig_handler(int)
{
}

namespace {

void PinToCpu2()
{
	unsigned char mask[128];
	memset(mask, 0, sizeof(mask));
	mask[0] = 4; /* bit 2 set -- CPU 2, same literal as eva_main.cpp's main() */
	sched_setaffinity(0, sizeof(mask), (cpu_set_t *)mask);
}

} // namespace

extern "C" void *OmegaSchedulingThread(void *param_1)
{
	OmegaThreadInfo *info = (OmegaThreadInfo *)param_1;

	PinToCpu2();

	/* Real code builds this mask via a 32-word manual copy loop from a stack
	 * sigset_t already populated by sigemptyset+sigaddset(10) -- functionally a
	 * plain copy, replaced with the sigset_t itself (no separate copy needed).
	 */
	sigset_t waitSet;
	sigemptyset(&waitSet);
	sigaddset(&waitSet, SIGUSR1);

	struct sigaction sa;
	sa.sa_mask = waitSet;
	sa.sa_handler = sched_sig_handler;
	sa.sa_flags = 0;
	sigaction(SIGUSR1, &sa, 0);

	if (s_bRunning != 0) {
		int caughtSignal;
		do {
			sigwait(&waitSet, &caughtSignal);

			pthread_mutex_lock(&s_hInitThreadMutex);
			pthread_mutex_lock(&s_hThreadMutex[info->workerId]);
			info->kernel->Exec();
			pthread_mutex_unlock(&s_hThreadMutex[info->workerId]);
			pthread_mutex_unlock(&s_hInitThreadMutex);

			pthread_mutex_lock(&s_hNestingMutex);
			/* Real expression: `*(pthread_t*)(s_tThreadInfo + s_iNesting*4 + 0x5c)` --
			 * confirmed by address arithmetic (see omega_globals.h) to be exactly
			 * s_hThreads[s_iNesting - 1] once s_iNesting is decremented below.
			 */
			if (0 < s_iNesting) {
				s_iNesting--;
				if (s_iNesting != 0)
					pthread_kill(s_hThreads[s_iNesting - 1], SIGUSR1);
			}
			pthread_mutex_unlock(&s_hNestingMutex);
		} while (s_bRunning != 0);
	}

	return 0;
}

extern "C" void *OmegaInitThread(void * /*param_1*/)
{
	PinToCpu2();

	pthread_mutex_lock(&s_hInitThreadMutex);
	CKernel::InitUserLayer();
	pthread_mutex_unlock(&s_hInitThreadMutex);

	return 0;
}

extern "C" void OmegaTimingThread(void * /*param_1*/)
{
	sigset_t blockSet;
	sigemptyset(&blockSet);
	sigaddset(&blockSet, SIGTERM); /* 0xf */
	sigaddset(&blockSet, SIGCHLD); /* 0xe */

	setpriority(PRIO_PROCESS, getpid(), -5);

	for (;;) {
		if (s_bRunning == 0)
			return;

		for (;;) {
			usleep(20000);
			if (s_timingenablelock != 0 || s_iTimingDisable != 0)
				break;

			pthread_mutex_lock(&s_hNestingMutex);
			/* Real code: a signal-fanout "nesting" walk. If nobody is currently
			 * nested (s_iNesting == 0), signal worker 0 and bump the nesting
			 * count; if fully nested (== 6), do nothing this tick; otherwise
			 * (partially nested) briefly lock/unlock the *current* worker's own
			 * mutex first (a real synchronization wait -- ensures that worker
			 * has finished its previous Exec() before re-signaling) before
			 * falling into the same signal-and-bump step. Preserved as the
			 * real goto-based control flow, not restructured.
			 */
			if (s_iNesting == 0) {
				pthread_kill(s_hThreads[s_iNesting], SIGUSR1);
				s_iNesting++;
			} else if (s_iNesting != 6) {
				pthread_mutex_lock(&s_hThreadMutex[s_iNesting - 1]);
				pthread_mutex_unlock(&s_hThreadMutex[s_iNesting - 1]);
				pthread_kill(s_hThreads[s_iNesting], SIGUSR1);
				s_iNesting++;
			}
			pthread_mutex_unlock(&s_hNestingMutex);

			if (s_bRunning == 0)
				return;
		}
	}
}
