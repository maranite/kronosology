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
