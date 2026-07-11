// SPDX-License-Identifier: GPL-2.0
/*
 * effect_manager_run_effects.cpp  -  batch 49:
 * CSTGEffectManager::RunEffects() (`.text+0x208f70`, 230 bytes) reconstructed
 * for real. See oa_engine.h for the full confirmed field-layout writeup
 * (including the 120.0f cross-check this reconstruction independently
 * confirms) and oa_engine_init.h for CSTGMIDIClockSync::GetFilteredTempoBPM()/
 * oa_global.h for CSTGPerformanceVarsManager::RunEffects(), both real now
 * too, both called from here.
 *
 * Own dedicated TU (not managers.cpp/engine.cpp), matching this project's
 * established "give a newly-real method its own TU when an existing test
 * file's mock for the SAME symbol is load-bearing" precedent (sec 10.83
 * et al) -- test_engine.cpp's own CSTGEffectManager::RunEffects() mock
 * (a call-tracking log_call(), checked by its own scenario [2]) would
 * otherwise collide with a real body linked into the same binary (both
 * link managers.cpp already).
 *
 * Ground-truth disassembly, fully traced (all four raw dword mirrors AND
 * both x87 stack round-trips independently walked instruction-by-instruction,
 * not guessed):
 *   1. call CSTGPerformanceVarsManager::RunEffects() (this=&sInstance).
 *   2. v0 = CSTGMIDIClockSync::sInstance->GetFilteredTempoBPM(0);
 *      defaultTempoA = UNCONDITIONALLY set to v0 first (fsts, non-popping),
 *      then conditionally overwritten to 40.0f if v0<40.0f, or 300.0f if
 *      v0>300.0f (two chained x87 fucomi(p) compares against confirmed
 *      real `.rodata.cst4` constants at offsets 0xc10/0xc14) -- net effect
 *      is a plain clamp to [40.0f, 300.0f].
 *   3. _tailZeroed[0]/_tailZeroed[1] (+0xb6c/+0xb70) = CSTGMIDIClockSync::
 *      sInstance's own fieldAt(0x90)/fieldAt(0x94) (raw dword copies, no
 *      transformation).
 *   4. Same as step 2 but index=1, storing into defaultTempoB.
 *   5. _tailZeroed[2]/_tailZeroed[3] (+0xb74/+0xb78) = CSTGMIDIClockSync::
 *      sInstance's own fieldAt(0xb0)/fieldAt(0xb4).
 * No null-checks anywhere on CSTGMIDIClockSync::sInstance (matches ground
 * truth exactly, same "no null check where none exists" convention used
 * throughout this codebase).
 *
 * Needs -mhard-float (Makefile CFLAGS_effect_manager_run_effects.o
 * override) -- the clamp comparisons are plain C float relational ops,
 * which would otherwise pull unresolvable libgcc soft-float helpers into
 * this freestanding kernel module (same class of gotcha as
 * midi_clock_sync.cpp et al, sec 10.57).
 */

#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_global.h"

static float ClampTempoBPM(float v)
{
	if (v < 40.0f)
		return 40.0f;
	if (v > 300.0f)
		return 300.0f;
	return v;
}

void CSTGEffectManager::RunEffects()
{
	((CSTGPerformanceVarsManager *)&CSTGPerformanceVarsManager::sInstance)->RunEffects();

	CSTGMIDIClockSync *clock = CSTGMIDIClockSync::sInstance;
	const unsigned char *clockBase = (const unsigned char *)clock;

	defaultTempoA = ClampTempoBPM(clock->GetFilteredTempoBPM(0));
	_tailZeroed[0] = *(const unsigned int *)(clockBase + 0x90);
	_tailZeroed[1] = *(const unsigned int *)(clockBase + 0x94);

	defaultTempoB = ClampTempoBPM(clock->GetFilteredTempoBPM(1));
	_tailZeroed[2] = *(const unsigned int *)(clockBase + 0xb0);
	_tailZeroed[3] = *(const unsigned int *)(clockBase + 0xb4);
}
