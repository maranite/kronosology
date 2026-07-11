// SPDX-License-Identifier: GPL-2.0
/*
 * audio_start.cpp  -  CSTGAudioManager_StartAudioEngine()/
 * _StopAudioEngine()/_EnableAudioManagerThread() and the real
 * CSTGAudioManager::StartAudioEngine() body. See oa_audio_start.h for
 * the full ground-truthing details.
 *
 * Faithful, instruction-level reconstruction from a full objdump
 * disassembly + relocation trace of all four real functions in
 * OA_real.ko.
 */

#include "oa_audio_start.h"

/*
 * The confirmed linked-list node shape this function walks (list head
 * at CSTGAudioManager+0x20): each node has a `next` pointer at +0x0
 * (confirmed: `mov ebx,[ebx]` advances the list) and a `payload`
 * pointer at +0x8. The payload itself has: priority at +0x4, cpuId at
 * +0x8, and the CSTGThread object itself embedded at +0x10 (confirmed:
 * `lea eax,[edx+0x10]` becomes CreateRealTimeWithCPUAffinity's `this`).
 * Not independently named/reconstructed as a full struct in this pass
 * -- only the four confirmed field reads needed to reproduce the real
 * call are modeled, via raw offset arithmetic (this project's
 * established convention for a not-fully-recovered structure).
 */
struct AudioDeviceListNode {
	AudioDeviceListNode *next;	/* +0x0 */
	unsigned char _pad4[4];	/* +0x4, not touched here */
	unsigned char *payload;	/* +0x8 */
};

char CSTGAudioManager::StartAudioEngine()
{
	unsigned char *self = (unsigned char *)this;

	AudioDeviceListNode *node = *(AudioDeviceListNode **)(self + 0x20);

	/* Confirmed real field initialization, unconditional regardless of
	 * whether the device list is empty. */
	*(unsigned int *)(self + 0x4564) = 0;
	*(unsigned int *)(self + 0x10) = 0;
	*(unsigned int *)(self + 0x14) = 0;
	*(unsigned int *)(self + 0x4568) = 0x989680;
	*(unsigned int *)(self + 0x456c) = 0;
	self[0xa65] = 1;

	if (node) {
		/*
		 * Confirmed real: the FIRST node's own list-advance step
		 * (`ebx=[ebx]`) is skipped for the very first iteration --
		 * the loop tries the ORIGINAL head node first, only
		 * advancing (`node = node->next`) after a failure.
		 */
		for (;;) {
			unsigned char *payload = node->payload;
			int priority = *(int *)(payload + 0x4);
			unsigned int cpuId = *(unsigned int *)(payload + 0x8);
			CSTGThread *thread = (CSTGThread *)(payload + 0x10);

			if (!thread->CreateRealTimeWithCPUAffinity(
				    CSTGAudioThread::AudioTickLoopRoutine,
				    priority, cpuId, payload))
				goto fail;

			node = node->next;
			if (!node)
				break;
		}
	}

	{
		int priority = *(int *)(self + 0x4560);
		CSTGThread *askThread = (CSTGThread *)(self + 0xa24);
		if (!askThread->CreateRealTimeWithCPUAffinity(
			    CSTGAudioManager::ASKThreadRoutine, priority - 3, 0, self))
			goto fail;

		/* Confirmed real: reloaded fresh (NOT priority-3 carried
		 * over), same field read again. */
		priority = *(int *)(self + 0x4560);
		CSTGThread *audioMgrThread = (CSTGThread *)(self + 0x4);
		if (!audioMgrThread->CreateRealTimeWithCPUAffinity(
			    CSTGAudioManager::AudioManagerThreadRoutine, priority, 0, self))
			goto fail;
	}

	/*
	 * Confirmed real: the audio driver's own Start() (vtable slot 3,
	 * offset +0xc -- confirmed real via sec 10.37) is called through a
	 * raw indirect dispatch, not this reconstruction's own C++ virtual
	 * call mechanism (whose vtable layout isn't independently confirmed
	 * to match the real one beyond a couple of slots) -- and its return
	 * value is discarded UNCONDITIONALLY, exactly as sec 10.37 already
	 * found from the wrapper's own call site.
	 */
	{
		typedef void (*StartFn)(void *);
		StartFn start = ((StartFn *)(*(void ***)CSTGAudioDriverInterface::sInstance))[3];
		start(CSTGAudioDriverInterface::sInstance);
	}
	return 1;

fail:
	/*
	 * Confirmed real: calls back through THIS object's own vtable slot
	 * 1 -- the SAME slot CSTGAudioManager_StopAudioEngine's wrapper
	 * dispatches (see oa_audio_start.h's own note on why this is a
	 * real cleanup/stop method, not the Itanium ABI deleting
	 * destructor).
	 */
	{
		typedef void (*StopFn)(void *);
		StopFn stop = ((StopFn *)(*(void ***)self))[1];
		stop(self);
	}
	return 0;
}

extern "C" int CSTGAudioManager_StartAudioEngine(void)
{
	return CSTGAudioManager::sInstance->StartAudioEngine();
}

/*
 * The three real thread-entry-point bodies (sec 10.149) -- confirmed via
 * a full objdump disassembly of each. All three are only ever taken as
 * function-pointer VALUES above (StartAudioEngine's own
 * CreateRealTimeWithCPUAffinity calls), never invoked directly from this
 * reconstruction's own C++ call graph.
 */

/* Plain C-linkage forward declarations for the siblings these bodies
 * call into. rtwrap_whoami/rtwrap_task_suspend are now REAL bodies
 * (src/init/rtwrap.cpp, batch 37) -- promoting them forced a
 * signature fix here too: ground truth's real ASKThreadRoutine calls
 * `call rtwrap_whoami` immediately followed by `call
 * rtwrap_task_suspend` with NO intervening instruction that touches
 * %eax, i.e. rtwrap_task_suspend's argument (regparm(3) arg1, %eax) IS
 * rtwrap_whoami's own return value (an implicit register-passthrough,
 * not two independent void calls) -- confirmed via a fresh disassembly
 * of `.text+0x67100` (see rtwrap.cpp's own header for why
 * rtwrap_whoami/task_suspend needed a void->void* / 0-arg->1-arg
 * fidelity fix, same class of fix as sec 10.182's
 * stg_log_startup_error int->const-char* correction). SKMain_Run
 * remains a confirmed-real, deliberately deferred sibling (see
 * bar2_stubs.cpp for its trivial no-op definition), declared WITHOUT
 * extern "C" to match the plain-C-linkage CHOICE already made for its
 * own definition (an internal-consistency convention, not a claim
 * about the real binary's own mangling -- see bar2_stubs.cpp's own
 * comment on this). */
extern "C" void *rtwrap_whoami(void);
extern "C" void rtwrap_task_suspend(void *task);
extern "C" void SKMain_Run(void);

/*
 * CSTGAudioThread::AudioTickLoopRoutine(void*) (`.text+0x5dfa0`
 * COMDAT section named after the mangled `EPv` symbol, 17 bytes)
 * confirmed: a pure forwarding wrapper that IGNORES its own incoming
 * `void *arg` entirely (never reads it) and tail-calls the no-arg
 * overload, discarding ITS return value too (`xor eax,eax` after the
 * call -- always returns NULL regardless of what the no-arg overload
 * itself would have returned).
 */
void *CSTGAudioThread::AudioTickLoopRoutine(void *)
{
	CSTGAudioThread::AudioTickLoopRoutine();
	return 0;
}

/*
 * CSTGAudioManager::ASKThreadRoutine(void*) (`.text+0x67100`, 59
 * bytes) confirmed: casts the incoming `void *arg` back to `this`
 * (regparm(3) `this` in eax at entry, but the real code re-derives it
 * from the explicit `void *arg` register instead -- both are the same
 * value at a thread-entry call, modeled here via the `arg` parameter
 * directly), then `while (fieldAt(0xa65))` ("running", confirmed via
 * StartAudioEngine's own +0xa65 field above): `me = rtwrap_whoami();
 * rtwrap_task_suspend(me); SKMain_Run();` -- a real per-tick synthesis-
 * kernel dispatch loop (self-suspend idiom: get the calling task's own
 * handle, then suspend THAT task), gated on the SAME running flag
 * StartAudioEngine sets to 1 and CSTGAudioManager_StopAudioEngine's
 * own vtable-slot-1 target presumably clears (not independently
 * confirmed in this pass).
 */
void *CSTGAudioManager::ASKThreadRoutine(void *arg)
{
	unsigned char *self = (unsigned char *)arg;
	while (self[0xa65]) {
		void *me = rtwrap_whoami();
		rtwrap_task_suspend(me);
		SKMain_Run();
	}
	return 0;
}

/*
 * CSTGAudioManager::AudioManagerThreadRoutine(void*) (`.text+0x670b0`,
 * 67 bytes) confirmed: sets MXCSR to 0x9fc0 (DAZ+FTZ, all exceptions
 * masked -- standard real-time audio-thread FP control word),
 * UNCONDITIONALLY sets `fieldAt(0xd) = 1` (confirmed real quirk: this
 * write happens regardless of whether the loop below ever runs even
 * once -- the condition test that gates the loop is evaluated from the
 * SAME `fieldAt(0xa65)` read that happened just before this write, not
 * re-read after), then while that flag is nonzero: dispatches this
 * object's own vtable slot 2 (raw indirect call through `*(void***)self`
 * offset +0x8, confirmed via `call *0x8(%edx)` -- NOT this
 * reconstruction's own C++ virtual mechanism, whose layout isn't
 * independently confirmed to match beyond a couple of slots, same
 * convention as StartAudioEngine's own raw vtable dispatches above).
 */
void *CSTGAudioManager::AudioManagerThreadRoutine(void *arg)
{
	unsigned char *self = (unsigned char *)arg;

	unsigned int mxcsr = 0x9fc0;
	asm volatile("ldmxcsr %0" : : "m"(mxcsr));

	bool running = self[0xa65] != 0;
	self[0xd] = 1;

	while (running) {
		typedef void (*Fn)(void *);
		Fn fn = ((Fn *)(*(void ***)self))[2];
		fn(self);
		running = self[0xa65] != 0;
	}
	return 0;
}

extern "C" void CSTGAudioManager_StopAudioEngine(void)
{
	typedef void (*StopFn)(void *);
	StopFn stop = ((StopFn *)(*(void ***)CSTGAudioManager::sInstance))[1];
	stop(CSTGAudioManager::sInstance);
}

extern "C" void CSTGAudioManager_EnableAudioManagerThread(void)
{
	((unsigned char *)CSTGAudioManager::sInstance)[0xc] = 1;
}
