// SPDX-License-Identifier: GPL-2.0
/*
 * oa_audio_start.h  -  CSTGAudioManager_StartAudioEngine()/
 * _StopAudioEngine()/_EnableAudioManagerThread(): init_module step 13
 * (hard-fail, INVERTED SUCCESS CONVENTION -- nonzero = success), plus
 * the real CSTGAudioManager::StartAudioEngine() body those three thin
 * wrappers call into.
 *
 * Ground-truthed via readelf's symbol table against
 * ARCHIVE/Ignored/DecryptedImages/OA_real.ko:
 *   CSTGAudioManager_StartAudioEngine          .text+0x671e0 (23 bytes)
 *   CSTGAudioManager_StopAudioEngine            .text+0x67200 (20 bytes)
 *   CSTGAudioManager_EnableAudioManagerThread   .text+0x67220 (10 bytes)
 *   CSTGAudioManager::StartAudioEngine()        .text+0x66f00 (235 bytes)
 * then a full objdump disassembly + relocation trace of all four.
 *
 * Confirms and extends this project's own earlier finding (sec
 * 10.38/10.39): the real gate is N (a linked-list length at
 * `this+0x20`) + 2 fixed calls to `CSTGThread::
 * CreateRealTimeWithCPUAffinity()` -- now fully traced: N real-time
 * threads (one per list node, entry point `CSTGAudioThread::
 * AudioTickLoopRoutine`), then two fixed ones (`CSTGAudioManager::
 * ASKThreadRoutine`, `::AudioManagerThreadRoutine`). If ANY of these
 * fail, the function calls back into its own vtable slot 1 (the SAME
 * slot `CSTGAudioManager_StopAudioEngine`'s wrapper dispatches --
 * confirmed via this exact overlap, not assumed: a real cleanup/stop
 * virtual method, not the Itanium ABI deleting destructor an earlier,
 * more limited reconstruction pass modeled this class's vtable
 * placement around) and returns 0 (failure). If everything succeeds,
 * it calls `CSTGAudioDriverInterface::sInstance`'s own vtable slot 3
 * (confirmed real `Start()`, per sec 10.37) and returns 1 (success)
 * UNCONDITIONALLY -- the audio driver's own return value is discarded,
 * exactly as sec 10.37 already found from the wrapper's own call site.
 */

#ifndef OA_AUDIO_START_H
#define OA_AUDIO_START_H

#include "oa_engine.h"

/* CSTGThread::CreateRealTimeWithCPUAffinity -- now fully reconstructed,
 * see oa_cpu_affinity.h (MASTER_REFERENCE.md sec 10.52). */
#include "oa_cpu_affinity.h"

/*
 * Thread entry-point function -- used as a function-pointer VALUE here
 * (passed to CreateRealTimeWithCPUAffinity), but ALSO genuinely real and
 * reconstructed now (sec 10.149, `.text+0x?` -- see src/init/
 * audio_start.cpp): a tiny (17-byte) forwarding wrapper that ignores its
 * own incoming `void *arg` entirely and tail-calls the no-arg overload
 * below. The no-arg overload itself (`.text+0x5dfa0`, 141 bytes) is a
 * substantial real-time audio tick loop (two `CSTGThreadBarrier::Wait()`
 * barrier syncs plus two vtable dispatches through `this->fieldAt(0x28)`
 * and `CSTGAudioManager::sInstance`) -- confirmed real, deliberately
 * deferred, own body far out of scope for this pass (see bar2_stubs.cpp).
 */
struct CSTGAudioThread {
	static void *AudioTickLoopRoutine(void *arg);
	static void AudioTickLoopRoutine();
};

extern "C" {

int CSTGAudioManager_StartAudioEngine(void);
void CSTGAudioManager_StopAudioEngine(void);
void CSTGAudioManager_EnableAudioManagerThread(void);

} /* extern "C" */

#endif /* OA_AUDIO_START_H */
