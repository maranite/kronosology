// SPDX-License-Identifier: GPL-2.0
/*
 * oa_cpu_affinity.h  -  CSTGThread::CreateRealTimeWithCPUAffinity(): the
 * real RTAI-wrapping thread-creation method this project has referenced
 * (via `CSTGAudioManager_StartAudioEngine`, sec 10.50) since sec
 * 10.38/10.39 first identified it as self-contained in OA.ko, wrapping
 * real RTAI primitives directly.
 *
 * Ground-truthed via readelf's symbol table against
 * ARCHIVE/Ignored/DecryptedImages/OA_real.ko
 * (`_ZN10CSTGThread29CreateRealTimeWithCPUAffinityEPFPvS0_EijS0_`,
 * `.text+0x40a30`, 202 bytes), then a full objdump disassembly +
 * relocation trace.
 *
 * Confirmed real fields (via disassembly, not guessed): `+0x0` is a
 * task-handle slot (populated by `rtwrap_pthread_create` itself, as
 * part of that not-yet-reconstructed function's own body -- this
 * function only reads it back afterward), `+0x4` is a one-byte "debug
 * traps installed" flag this function itself sets/clears.
 *
 * Confirmed real call sequence: get the real RTAI pthread-attr
 * structure's size at runtime (`get_sizeof_rtwrap_pthread_attr()` --
 * OA.ko deliberately doesn't hardcode this, presumably because it can
 * vary across RTAI builds/versions), allocate that much stack space
 * (a genuine variable-length stack array in the real disassembly,
 * modeled here with `alloca()` to match exactly), initialize it
 * (`rtwrap_pthread_attr_init`), set its priority
 * (`rtwrap_pthread_attr_setrtpriority`) and a FIXED stack size
 * (`rtwrap_pthread_attr_setstacksize(0x5000)`, confirmed literal, not
 * derived from any argument), create the real-time thread
 * (`rtwrap_pthread_create`), then destroy the attr object
 * (`rtwrap_pthread_attr_destroy`) regardless of whether creation
 * succeeded. On success, installs debug traps
 * (`rtwrap_set_debug_traps_in_rt_task`) and, if THAT succeeds, pins the
 * thread to the given CPU (`rtwrap_set_runnable_on_cpuid`) and returns
 * true; if debug-trap installation itself fails, tears the thread back
 * down (`rtwrap_clear_debug_traps_in_rt_task` + `rtwrap_pthread_cancel`)
 * and returns false.
 *
 * None of the `rtwrap_*`/`get_sizeof_rtwrap_*` helpers this function
 * calls are reconstructed in this pass -- confirmed real (self-
 * contained in OA.ko, per sec 10.39), same "declare the shape, defer
 * the body" treatment as every other not-yet-implemented dependency in
 * this project.
 */

#ifndef OA_CPU_AFFINITY_H
#define OA_CPU_AFFINITY_H

struct CSTGThread {
	void *taskHandle;		/* +0x0, populated by rtwrap_pthread_create */
	unsigned char debugTrapsInstalled;	/* +0x4 */

	char CreateRealTimeWithCPUAffinity(void *(*entryFn)(void *), int priority,
					    unsigned int cpuId, void *arg);
};

extern "C" {

unsigned int get_sizeof_rtwrap_pthread_attr(void);
void rtwrap_pthread_attr_init(void *attr);
void rtwrap_pthread_attr_setrtpriority(void *attr, int priority);
void rtwrap_pthread_attr_setstacksize(void *attr, unsigned int stackSize);
void *rtwrap_pthread_create(void *this_, void *attr, void *(*entryFn)(void *), void *arg);
void rtwrap_pthread_attr_destroy(void *attr);
int rtwrap_set_debug_traps_in_rt_task(void *taskHandle);
void rtwrap_set_runnable_on_cpuid(void *taskHandle, unsigned int cpuId);
void rtwrap_clear_debug_traps_in_rt_task(void *taskHandle);
void rtwrap_pthread_cancel(void *taskHandle);

} /* extern "C" */

#endif /* OA_CPU_AFFINITY_H */
