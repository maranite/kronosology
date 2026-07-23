/*
 * omega_globals.h  -  shared module-scope statics for COmegaInterface::Init()'s
 * spawned-thread family (Stage 3): defined once in omega_interface.cpp, used from
 * both omega_interface.cpp and omega_threads.cpp.
 *
 * Layout of the whole block (s_timingenablelock through s_hOmegaTimingThread) is
 * empirically confirmed contiguous and correctly-sized purely from symbols.csv's own
 * addresses (0x09309474..0x093095e8) -- each pair of neighboring globals' address
 * delta matches its predecessor's real size exactly (e.g. s_hThreadMutex@09309520 to
 * s_hInitThreadMutex@093095b0 is 0x90 = 6 * sizeof(pthread_mutex_t)-on-this-target's
 * 0x18; s_hThreads@09309500 to s_hOmegaInitThread@0930951c is 0x1c, i.e. 6 * 4 (+ 4
 * bytes padding) for 6 pthread_t). This let two real quirks get resolved with
 * confidence rather than guessed:
 *  - s_hThreads is a real 6-element pthread_t array. Ghidra only ever names element 0
 *    "s_hThreads" and the other 5 "DAT_0930950{4,8,c}"/"DAT_09309510"/"DAT_09309514"
 *    (visible in OmegaExitThread, not itself reconstructed -- not reachable from any
 *    traced code path, see README.md) -- a Ghidra array-recognition miss, not 6
 *    separate variables. Declared here as a real `pthread_t s_hThreads[6]`.
 *  - OmegaSchedulingThread's own reference to this same array
 *    (`*(pthread_t*)(s_tThreadInfo + s_iNesting*4 + 0x5c)`) is expressed relative to
 *    the *preceding* symbol s_tThreadInfo instead of s_hThreads, purely because
 *    Ghidra picked a different nearby symbol as its arithmetic base in that function
 *    -- confirmed identical to `s_hThreads[s_iNesting - 1]` by address arithmetic
 *    (s_tThreadInfo@093094a0 + 0x60 == s_hThreads@09309500, and 0x5c == 0x60 - 4).
 *    Transcribed as the clean indexed form, not the raw offset expression.
 */

#ifndef OMEGA_GLOBALS_H
#define OMEGA_GLOBALS_H

#include <pthread.h>

class CKernel;

/* Real per-block layout, confirmed from the explicit byte offsets the disassembly
 * writes (0/4/0xc within each 0x10-byte block; +8 is never written -- real padding,
 * not a missed field): [+0]=workerId [+4]=kernel [+8]=unused [+0xc]=reserved(always 0).
 */
struct OmegaThreadInfo {
	int workerId;
	CKernel *kernel;
	int unused8;
	int reserved;
};

/* s_bRunning is shared between COmegaInterface::Close()/~COmegaInterface() and
 * OmegaTimingThread/OmegaSchedulingThread's own loop conditions -- why it lives here
 * rather than as a class member.
 */
extern volatile int s_bRunning;
extern volatile int s_timingenablelock;
extern int s_iTimingDisable;
extern int g_bCriticalSectionValid;
extern pthread_t s_hOmegaInitThread;

extern pthread_mutex_t s_hThreadMutex[6];
extern OmegaThreadInfo s_tThreadInfo[6];

extern pthread_t s_hThreads[6];
extern pthread_mutex_t s_hInitThreadMutex;
extern pthread_mutex_t s_hNestingMutex;
extern int s_iNesting;
extern pthread_t s_hOmegaTimingThread;

#endif /* OMEGA_GLOBALS_H */
