/*
 * omega_interface.cpp  -  see include/omega_interface.h.
 *
 * Transcribed from the Ghidra decompile export
 * (Decomp/EVA_Decomp/eva_export/functions/{Init,Run,Stop,Close,...}@0804e0xx.c).
 *
 * Init()'s own callees are now reconstructed: CKernel::CKernel/~CKernel (ckernel.cpp),
 * SetConfigInfo (config_info.cpp), CKernel::InitSystemLayer (ckernel.cpp), Mains
 * (mains.cpp), and the three OmegaXxxThread bodies (omega_threads.cpp) -- see
 * README.md's Stage 3 section. Init() still doesn't link to a complete, runnable
 * program (each of those pulls in its own further Stage-4+ call-contract externs),
 * same philosophy as reconstructed/OA's "unresolved symbols are fine, not unexplained."
 */

#include "omega_interface.h"
#include "ckernel.h"
#include "config_manager.h"
#include "mains.h"
#include "omega_globals.h"

#include <cstdio>
#include <pthread.h>
#include <sys/time.h>

/* OmegaSchedulingThread/OmegaInitThread bodies return through the standard
 * pthread-trampoline void*(void*) shape (real bodies return `undefined4 0`, cast
 * here same as any other pthread worker); OmegaTimingThread's real signature returns
 * void and is called directly (not via pthread_create) -- see omega_threads.cpp.
 */
extern "C" {
void *OmegaSchedulingThread(void *threadInfo);
void *OmegaInitThread(void *arg);
void OmegaTimingThread(void *arg);
}

/* Real global instance -- main() references it by the confirmed symbol name "Omega". */
COmegaInterface Omega;

/* Definitions for the shared statics declared in omega_globals.h -- see that header
 * for the address-arithmetic evidence tying this whole block together, and for
 * s_hThreads/s_tThreadInfo's real per-element layout.
 */
volatile int s_bRunning = 0;
volatile int s_timingenablelock = 0;
int s_iTimingDisable = 0;
static struct timeval s_tvStart; /* only ever read/written here -- not shared cross-TU */
pthread_t s_hOmegaInitThread;
int g_bCriticalSectionValid = 0;

pthread_mutex_t s_hThreadMutex[6];
OmegaThreadInfo s_tThreadInfo[6];

pthread_t s_hThreads[6];
pthread_mutex_t s_hInitThreadMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t s_hNestingMutex = PTHREAD_MUTEX_INITIALIZER;
int s_iNesting = 0;
pthread_t s_hOmegaTimingThread = 0;

COmegaInterface::COmegaInterface()
{
	mUnused1c = 0;
	mUnused20 = 0;
	mKernel = 0;
	mUnused04 = 0;
	mCreated = 0;
}

COmegaInterface::~COmegaInterface()
{
	s_bRunning = 0;
}

void *COmegaInterface::GetSysApi()
{
	return CKernel::GetSysApi();
}

void COmegaInterface::ExitRequested()
{
	/* Real code makes an indirect virtual call through *(sysapi_vtable + 0x7c);
	 * Ghidra couldn't recover the jump table and the target class/slot isn't
	 * identified yet. Not implemented -- would need the CKernel/sysapi vtable
	 * layout reconstructed first (Stage 3).
	 */
}

void COmegaInterface::Init(_func_int_char_ptr sendCallback)
{
	if (mCreated != 0)
		return;

	puts("create new kernel");
	/* Real code brackets the malloc(0x10) + placement CKernel::CKernel(0) with
	 * HAL_DisableInterrupts()/HAL_EnableInterrupts() -- a kernel-side (not userspace
	 * syscall) primitive pair, presumably a critical-section shim inherited from an
	 * embedded-OS lineage this app's framework originated from. Not yet identified
	 * further; the section is not reconstructed as an actual interrupt toggle here.
	 */
	mKernel = new CKernel(0);

	gettimeofday(&s_tvStart, 0);

	g_bCriticalSectionValid = 1;
	for (int i = 0; i < 6; i++) {
		s_tThreadInfo[i].workerId = i;
		s_tThreadInfo[i].kernel = mKernel;
		s_tThreadInfo[i].reserved = 0;
		pthread_mutex_init(&s_hThreadMutex[i], 0);
		/* Real code stores each of the 6 thread IDs into s_hThreads[i] (see
		 * omega_globals.h) -- read back by OmegaTimingThread/OmegaSchedulingThread's
		 * own signal-nesting logic (omega_threads.cpp), not unused as previously
		 * assumed before that pair was reconstructed.
		 */
		pthread_create(&s_hThreads[i], 0, (void *(*)(void *))OmegaSchedulingThread, &s_tThreadInfo[i]);
	}

	puts("host buf init");
	mSendCallback = sendCallback;
	mCreated = 1;

	puts("set config info");
	SetConfigInfo();

	puts("init system layer");
	CKernel::InitSystemLayer();

	puts("mains");
	Mains();
	puts("done with mains");

	puts("create init thread");
	pthread_create(&s_hOmegaInitThread, 0, (void *(*)(void *))OmegaInitThread, this);

	puts("start timing thread");
	/* Not spawned -- called directly on the calling thread. This is the real
	 * reason Init() (and therefore main()) does not return until app shutdown.
	 */
	OmegaTimingThread(0);
	puts("done with omega init");
}

int COmegaInterface::Run()
{
	while (s_timingenablelock & 1) {
		s_timingenablelock |= 1;
	}
	if (s_iTimingDisable > 0)
		s_iTimingDisable--;
	s_timingenablelock &= ~1;
	return -1;
}

int COmegaInterface::Stop()
{
	while (s_timingenablelock & 1) {
		s_timingenablelock |= 1;
	}
	s_iTimingDisable++;
	s_timingenablelock &= ~1;
	return -1;
}

void COmegaInterface::Close()
{
	s_bRunning = 0;
}
