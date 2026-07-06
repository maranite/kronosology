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
 * with `this == 0`. NOW INDEPENDENTLY CONFIRMED SAFE (sec 10.151, not
 * just "not disproven" as this comment originally said):
 * `ShouldSyncExternalClock()`'s own real body (see below) never
 * dereferences `this` at all -- it ignores its incoming `this` entirely
 * and reads a totally different global instead. That is PRECISELY why
 * the real caller never needed a null check in the first place.
 * Reproduced verbatim rather than "fixed" with a defensive null check,
 * per this project's established "preserve real quirks" policy.
 */

#include "oa_engine_init.h"

CTimerManager *CTimerManager::ms_poInstance;
unsigned char *CKGBankManager::ms_poInstance;

bool SKSTGGate_ShouldSyncExternalClock()
{
	return CTimerManager::ms_poInstance->ShouldSyncExternalClock();
}

/*
 * CTimerManager::ShouldSyncExternalClock() (`.text+0x347210`, 54 bytes,
 * sec 10.151) confirmed via full disassembly (`mov eax,ds:0x0` reloc'd
 * to `CKGBankManager::ms_poInstance`, THEN `mov eax,[eax+0x97c750]` --
 * `this` is never read):
 *   int mode = *(int *)(CKGBankManager::ms_poInstance + 0x97c750);
 *   mode == 0            -> false
 *   mode == 1 || mode==3 -> true
 *   mode == 2 || mode==4 -> *(unsigned char *)(CKGBankManager::ms_poInstance + 8) != 0
 *   otherwise            -> false
 * `CKGBankManager` is not reconstructed anywhere in this project (see
 * oa_engine_init.h's minimal opaque declaration) -- both offsets are
 * applied directly to the raw pointer value, matching this project's
 * established treatment of out-of-scope opaque targets.
 */
bool CTimerManager::ShouldSyncExternalClock()
{
	unsigned char *base = CKGBankManager::ms_poInstance;
	int mode = *(int *)(base + 0x97c750);

	if (mode == 2 || mode == 4)
		return *(unsigned char *)(base + 8) != 0;
	if (mode == 1 || mode == 3)
		return true;
	return false;
}

/*
 * SKSTGGate_GetInternalTempo() (batch 21, `.text+0x349d30`, 20 bytes):
 * a plain non-virtual forwarder, see oa_engine_init.h.
 */
int SKSTGGate_GetInternalTempo()
{
	return CTimerManager::ms_poInstance->GetInternalTempo();
}

/*
 * CTimerManager::GetInternalTempo() (batch 21, `.text+0x347250`, 6
 * bytes): see oa_engine_init.h for the confirmed real
 * `*(int*)(*(int**)this + 0x2c)` shape.
 */
int CTimerManager::GetInternalTempo()
{
	return *(int *)(*(unsigned char **)this + 0x2c);
}
