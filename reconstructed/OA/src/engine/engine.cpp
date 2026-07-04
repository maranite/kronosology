// SPDX-License-Identifier: GPL-2.0
/*
 * engine.cpp  -  see include/oa_engine.h for ground-truthing of which
 * methods are implemented here vs. deliberately deferred, and why.
 */

#include "oa_engine.h"
#include "oa_global.h"

CSTGEngine           *CSTGEngine::sInstance;
/* CSTGVoiceModelManager/CSTGFileOpener/CSTGFileCloser/CSTGHDRFileReader/
 * CSTGHDRFileWriter/CSTGStreamingFileReader/CSTGSamplingDaemon/
 * CSTGCDWorker/CSTGMonitorMixer/CSTGAudioBusManager/CSTGEffectManager/
 * CSTGHDRManager::sInstance are now defined in managers.cpp, alongside
 * the real constructors reconstructed there. */
CSTGMidiPortManager   *CSTGMidiPortManager::sInstance;
/* CPowerOffTimer/CEmergencyStealer/CLoadBalancer::sInstance are now defined
 * in managers.cpp, alongside the real constructors reconstructed there. */
/* CSTGMessageProcessor::sInstance is now defined in managers.cpp,
 * alongside the real constructor reconstructed there. */
CSTGAudioDriverInterface *CSTGAudioDriverInterface::sInstance;
/* CSTGAudioManager::sInstance is now defined in managers.cpp, alongside
 * the real constructor reconstructed there. */
/* CSTGVoiceAllocator::sInstance is now defined in managers.cpp, alongside
 * the real constructor reconstructed there. */

/* Real kernel-side RTAI wrapper functions the destructor calls for
 * CPowerOffTimer's mutex teardown -- confirmed real symbol names via
 * relocation, not host-testable (same treatment as __kmalloc/kfree). */
extern "C" void rtwrap_pthread_mutex_destroy(void *mutex);
extern "C" void rtwrap_free(void *ptr);
extern "C" void signal_timed_out_daemons(void);

CSTGEngine::CSTGEngine()
{
	CSTGEngine::sInstance = this;
	((unsigned char *)this)[4] = 0;
}

CSTGEngine::~CSTGEngine()
{
	if (CSTGMidiPortManager::sInstance)
		CSTGMidiPortManager::sInstance->~CSTGMidiPortManager();

	CPowerOffTimer *powerOffTimer = CPowerOffTimer::sInstance;
	if (powerOffTimer) {
		/* Read as a 32-bit value, not a native `void*` -- see
		 * CPowerOffTimer's constructor in managers.cpp for why (the
		 * confirmed field is 4 bytes on the real 32-bit target, with no
		 * room for a native 8-byte host pointer). */
		unsigned char *p = (unsigned char *)powerOffTimer;
		void *mutex = (void *)(unsigned long)*(unsigned int *)(p + 0x18);
		CPowerOffTimer::sInstance = 0;
		rtwrap_pthread_mutex_destroy(mutex);
		rtwrap_free(mutex);
		operator delete(powerOffTimer);
	}

	if (CSTGVoiceModelManager::sInstance)
		CSTGVoiceModelManager::sInstance->~CSTGVoiceModelManager();

	if (CSTGMessageProcessor::sInstance)
		CSTGMessageProcessor::sInstance->~CSTGMessageProcessor();

	if (CSTGAudioDriverInterface::sInstance)
		delete CSTGAudioDriverInterface::sInstance;	/* virtual dispatch, confirmed via vtable slot +0x4 */

	CSTGAudioManager *audioMgr = CSTGAudioManager::sInstance;
	if (audioMgr) {
		audioMgr->~CSTGAudioManager();
		operator delete(audioMgr);
	}

	if (CSTGVoiceAllocator::sInstance)
		CSTGVoiceAllocator::sInstance->~CSTGVoiceAllocator();

	CLoadBalancer *loadBalancer = CLoadBalancer::sInstance;
	if (loadBalancer) {
		loadBalancer->~CLoadBalancer();
		operator delete(loadBalancer);
	}

	CSTGEngine::sInstance = 0;
}

void CSTGEngine::RunAudioTick(unsigned int tick)
{
	CSTGVoiceModelManager::sInstance->ProcessSubRate(tick);
	CSTGVoiceModelManager::sInstance->ProcessAudioRate(tick);
}

void CSTGEngine::RunEffects()
{
	CSTGEffectManager::sInstance->RunEffects();
}

void CSTGEngine::PostAudioTick()
{
	CSTGGlobal::sInstance->RunVoiceModelFeedback();
	CSTGAudioBusManager::sInstance->MixPerformanceOutputs();
	CSTGHDRManager::sInstance->ProcessHDRRecord();
	CSTGMonitorMixer::sInstance->RunMonitors();
	CSTGAudioBusManager::sInstance->LRBusIndivMirror();
	CSTGGlobal::sInstance->IncrementMicrosecondCount();

	/*
	 * A SECOND, separate 64-bit counter, inlined directly here rather than
	 * called through a method -- confirmed distinct from the one
	 * IncrementMicrosecondCount() maintains (that one lives at
	 * CSTGGlobal+0x29c9fb0/+0x29c9fb4; this one is a plain +1 per call,
	 * with no fractional-rate phase logic, at CSTGGlobal+0x29c9fa8/+0x29c9fac).
	 * Real field identity/purpose not determined in this pass.
	 */
	unsigned int *counterLo = (unsigned int *)((unsigned char *)CSTGGlobal::sInstance + 0x29c9fa8);
	unsigned int *counterHi = (unsigned int *)((unsigned char *)CSTGGlobal::sInstance + 0x29c9fac);
	unsigned int old = *counterLo;
	*counterLo = old + 1;
	if (*counterLo < old)
		(*counterHi)++;

	signal_timed_out_daemons();
}

void CSTGEngine::RunFileDaemonSynchronization()
{
	CSTGHDRManager::sInstance->ProcessCommands();
	CSTGFileOpener::sInstance->ProcessCommands();
	CSTGHDRFileReader::sInstance->ProcessCommands();
	CSTGHDRFileWriter::sInstance->ProcessCommands();
	CSTGFileCloser::sInstance->ProcessCommands();
	CSTGStreamingFileReader::sInstance->ProcessCommands();
	CSTGCDWorker::sInstance->ProcessCommands();
	CSTGSamplingDaemon::sInstance->ProcessCommands();
}
