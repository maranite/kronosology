/*
 * tempo.h  -  BPM/MPQN, the global tempo-limit pair CConfigManager::ConfigureSeqTimer()
 * (config_manager.cpp) calls into (Stage 6 breadth sweep, 2026-07-25).
 *
 * Both are real, tiny classes confirmed via `nm -C`/direct disassembly:
 *   BPM::SetLowerLimit(unsigned int)  .text+0x0816ba80, 129 bytes
 *   BPM::SetUpperLimit(unsigned int)  .text+0x0816bb10, 129 bytes
 *   BPM::Normalize()                  .text+0x0816bba0 -- NOT reconstructed, no caller
 *                                      found on the traced boot path
 *   MPQN::Normalize()                 .text+0x0816bbd0 -- ditto
 *
 * BPM stores tempo limits in beats-per-minute (`unsigned short`, confirmed via
 * `nm -C -S`: both `BPM::sm_LowerLimit`/`sm_UpperLimit` are 2-byte data symbols);
 * MPQN mirrors the same pair in microseconds-per-quarter-note (`int`, 4-byte data
 * symbols) -- the two representations are kept in lockstep by whichever of
 * SetLowerLimit/SetUpperLimit runs (each recomputes the OTHER class's matching
 * field: SetLowerLimit writes MPQN::sm_UpperLimit, SetUpperLimit writes
 * MPQN::sm_LowerLimit -- a real, if slightly surprising, cross-wiring, transcribed
 * exactly, not "fixed").
 *
 * Real invariant: sm_LowerLimit <= sm_UpperLimit (BPM). If a caller passes a value
 * that would break it, the real code prints an assertion (`Api`+0x94, "Tempo.cpp"
 * line 0x10/0x1c) and instead forces the OTHER limit to match the just-set one
 * (self-healing, not a hard failure) -- both branches transcribed faithfully below.
 *
 * Static initializer `BPM::_GLOBAL__I_sm_LowerLimit` (.text+0x0816bc00, 21 bytes,
 * real `__attribute__((constructor))`) sets MPQN's own pair to 250000/1500000
 * (240/40 BPM) before any SetLowerLimit/SetUpperLimit call -- confirmed and
 * reproduced in tempo.cpp. Direct `.data` byte read (readelf -S + file-offset math)
 * confirms BPM's own pair starts at {40, 0} (sm_LowerLimit=40, sm_UpperLimit=0) --
 * i.e. the real binary's static data alone leaves sm_UpperLimit temporarily BELOW
 * sm_LowerLimit until something calls SetUpperLimit with a real value later
 * (out of scope here) -- reproduced exactly, not "fixed" into a consistent pair.
 */

#ifndef TEMPO_H
#define TEMPO_H

class MPQN {
public:
	static int sm_LowerLimit;
	static int sm_UpperLimit;
};

class BPM {
public:
	static void SetLowerLimit(unsigned int bpm);
	static void SetUpperLimit(unsigned int bpm);

	static unsigned short sm_LowerLimit;
	static unsigned short sm_UpperLimit;
};

#endif /* TEMPO_H */
