/*
 * tempo.cpp  -  see include/tempo.h.
 *
 * Transcribed from SetLowerLimit@0816ba80.c / SetUpperLimit@0816bb10.c (129 bytes
 * each) and the real static initializer _GLOBAL__I_sm_LowerLimit@0816bc00.c
 * (21 bytes). Real `.data` initial values for BPM::sm_LowerLimit/sm_UpperLimit
 * ({40, 0}) confirmed by direct byte read (readelf -S .data + file-offset math),
 * not guessed.
 *
 * Real, faithfully-preserved hazard: neither function guards against `bpm == 0`
 * (`60000000 / (bpm & 0xffff)` divides by zero if it is) -- confirmed present in
 * the real disassembly too, not an artifact of this reconstruction. The only
 * caller wired up so far (CConfigManager::ConfigureSeqTimer(), config_manager.cpp)
 * is why config_info.cpp's own SeqTimerInfo placeholder was given sane non-zero
 * BPM defaults (40/240) rather than the all-zero convention used for tables whose
 * first field alone gates real execution -- see config_info.cpp's own comment.
 */

#include "tempo.h"
#include "system_api.h"

extern CSystemApi *Api;

int MPQN::sm_LowerLimit = 0;
int MPQN::sm_UpperLimit = 0;

unsigned short BPM::sm_LowerLimit = 40;
unsigned short BPM::sm_UpperLimit = 0;

namespace {

/* Real Api+0x94 assert-report call -- same slot/shape already established in
 * sysex_msg_task_base.cpp's own comment ("Api+0x94, puVar4 == NULL") and
 * config_manager.cpp's RegisterChunkServer(). cdecl, EvaVTableStub-backed
 * (omega_vtables.cpp) wherever not independently confirmed -- safe to call with
 * any argument shape since the real stub ignores all arguments.
 */
inline void ApiAssert(const char *file, int line)
{
	typedef void (*Fn)(void *, const char *, const char *, int);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x94);
	fn(Api, "Assertion failed in module %s, line %i.\n", file, line);
}

/* Real __attribute__((constructor)) matching BPM::_GLOBAL__I_sm_LowerLimit --
 * runs before main(), same convention as mains.cpp's 7 ConstructXxxApiInstance()
 * functions.
 */
__attribute__((constructor))
void ConstructBpmMpqnDefaults()
{
	MPQN::sm_LowerLimit = 250000;
	MPQN::sm_UpperLimit = 1500000;
}

} // namespace

void BPM::SetLowerLimit(unsigned int bpm)
{
	sm_LowerLimit = (unsigned short)bpm;
	if (sm_LowerLimit <= sm_UpperLimit) {
		MPQN::sm_UpperLimit = (int)(60000000LL / (long long)(int)(bpm & 0xffff));
		return;
	}
	ApiAssert("Tempo.cpp", 0x10);
	sm_UpperLimit = sm_LowerLimit;
	MPQN::sm_LowerLimit = (int)(60000000LL / (long long)(int)(unsigned int)sm_LowerLimit);
	MPQN::sm_UpperLimit = MPQN::sm_LowerLimit;
}

void BPM::SetUpperLimit(unsigned int bpm)
{
	sm_UpperLimit = (unsigned short)bpm;
	if (sm_LowerLimit <= sm_UpperLimit) {
		MPQN::sm_LowerLimit = (int)(60000000LL / (long long)(int)(bpm & 0xffff));
		return;
	}
	ApiAssert("Tempo.cpp", 0x1c);
	sm_LowerLimit = sm_UpperLimit;
	MPQN::sm_UpperLimit = (int)(60000000LL / (long long)(int)(unsigned int)sm_UpperLimit);
	MPQN::sm_LowerLimit = MPQN::sm_UpperLimit;
}
