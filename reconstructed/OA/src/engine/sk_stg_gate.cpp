// SPDX-License-Identifier: GPL-2.0
/*
 * sk_stg_gate.cpp  -  SKSTGGate_ShouldSyncExternalClock() (sec 10.148).
 *
 * Ground-truthed via objdump -dr against OA_real.ko:
 *   `.text+0x349cf0`, 20 bytes, mangled `_Z33SKSTGGate_ShouldSyncExternalClockv`
 *   (an ordinary global C++ function, not `extern "C"` -- the real
 *   mangled name confirms this).
 *
 * Confirmed real body: loads the static `CTimerManager::ms_poInstance`
 * pointer, then makes a direct (non-virtual, plain `call` -- no vtable
 * load) regparm(3) call to `CTimerManager::ShouldSyncExternalClock()`
 * with that pointer as `this` (EAX), returning its result verbatim.
 *
 * A genuine, faithfully-preserved quirk: there is NO null check on
 * `ms_poInstance` before the call -- if the timer manager singleton
 * hasn't been constructed yet, this dispatches a non-virtual member call
 * with `this == 0`. Safe in practice only because
 * `ShouldSyncExternalClock()`'s own real body (not reconstructed in this
 * pass -- see oa_engine_init.h's minimal opaque `CTimerManager`
 * declaration) would need to actually dereference `this` to crash, which
 * this pass hasn't independently confirmed either way. Reproduced
 * verbatim rather than "fixed" with a defensive null check, per this
 * project's established "preserve real quirks" policy.
 */

#include "oa_engine_init.h"

CTimerManager *CTimerManager::ms_poInstance;

bool SKSTGGate_ShouldSyncExternalClock()
{
	return CTimerManager::ms_poInstance->ShouldSyncExternalClock();
}
