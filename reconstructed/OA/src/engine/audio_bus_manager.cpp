// SPDX-License-Identifier: GPL-2.0
/*
 * audio_bus_manager.cpp  -  CSTGAudioBusManager::LRBusIndivMirror()
 * (`.text+0x236f0`, 253 bytes, sec 10.153).
 *
 * Deliberately a separate translation unit from managers.cpp: this
 * symbol's own mock in test_engine.cpp (a `log_call("LRBusIndivMirror")`
 * recorder) is load-bearing for a call-ORDER assertion there
 * ("RunMonitors;LRBusIndivMirror;signal_timed_out_daemons;") -- matching
 * this project's established "keep the real body out of the TU that
 * test's own mock depends on" convention (same reasoning as
 * WriteSTGMidiOutQueue/CSTGAudioInputMixerBase, sec 10.150). Confirmed
 * via `grep -l LRBusIndivMirror verify/` -- only test_engine.cpp
 * references it.
 *
 * Confirmed regparm(3): this=EAX, no other args.
 *
 * Real logic: `this->physBusIdTableHead` selects which physical bus this
 * manager mirrors. `curBufIdx = CSTGPerformanceVarsManager::sInstance[9]`
 * (a confirmed real 0/1 "current double-buffer half" selector byte,
 * distinct from the already-established `sInstance[8]` "active perf-vars
 * slot" byte used elsewhere, sec 10.71/10.151). SOURCE is always a FIXED
 * location -- slot 12 of the CURRENT half of `sEffectThreadBusSets`
 * (`&sEffectThreadBusSets[curBufIdx*120 + 12]`, i.e. the running "master
 * LR bus" snapshot for whichever half is currently live). DEST depends
 * on `physBusIdTableHead`:
 *   <= 33:  `&sGlobalBusSet[physBusIdTableHead]` (34 slots total, exactly
 *           filling the confirmed 0x1100-byte array).
 *   >= 34:  `&sEffectThreadBusSets[curBufIdx*120 + (physBusIdTableHead-34)]`
 *           (240 slots total across both halves, exactly filling the
 *           confirmed 0x7800-byte array).
 * Copies 0x100 (256) bytes SOURCE -> DEST. The real binary does this via
 * 16 `movaps` (128-bit SSE) loads/stores; this build has no SSE
 * (`-mno-sse`/`-msoft-float`), and a 256-byte block copy has no
 * floating-point semantics to preserve either way -- reimplemented as a
 * plain byte loop (same "byte-exact result, different codegen strategy"
 * simplification already used for other pure bulk-copy patterns in this
 * project, no behavior difference is observable at this level).
 */

#include "oa_engine.h"
#include "oa_global.h"

unsigned char CSTGAudioBusManager::sGlobalBusSet[34 * 0x80];
unsigned char CSTGAudioBusManager::sEffectThreadBusSets[240 * 0x80];

void CSTGAudioBusManager::LRBusIndivMirror()
{
	unsigned int curBufIdx = CSTGPerformanceVarsManager::sInstance[9];

	unsigned char *src = sEffectThreadBusSets + curBufIdx * 120 * 0x80 + 12 * 0x80;

	/* Confirmed real UNSIGNED compare (`cmp $0x21,%eax; jbe`), not
	 * signed -- preserved via the explicit cast. */
	unsigned char *dst;
	if ((unsigned int)physBusIdTableHead <= 0x21)
		dst = sGlobalBusSet + physBusIdTableHead * 0x80;
	else
		dst = sEffectThreadBusSets + (curBufIdx * 120 + (physBusIdTableHead - 34)) * 0x80;

	for (unsigned int i = 0; i < 0x100; i++)
		dst[i] = src[i];
}
