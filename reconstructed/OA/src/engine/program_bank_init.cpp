// SPDX-License-Identifier: GPL-2.0
/*
 * program_bank_init.cpp -- CSTGProgramBank::Initialize()/GetPatchSize()
 * and CSTGProgram::Initialize() (batch 61, sec 10.[TBD]).
 *
 * CSTGProgramBank::Initialize(eSTGProgramBankId, eSTGProgramBankType, bool)
 * (ground truth `.text+0xa27f0`, 151 bytes) -- confirmed real via direct
 * disassembly. CORRECTS oa_global.h's own prior "bodies DSP/filesystem-
 * stub-callee deferred (genuine program-bank/patch management, out of
 * scope)" claim -- that was never actually disassembled; the real body
 * is a trivial 3-byte header write + a flags/voiceModelType formula + a
 * 128-entry loop over embedded `CSTGProgram` sub-objects (confirmed
 * `0xcec`-byte stride, independently cross-checked against
 * `CSTGProgram::CSTGProgram()`'s own already-confirmed size). No DSP, no
 * vtable dispatch, no filesystem I/O of its own.
 *
 * Confirmed field writes (regparm(3): this=EAX, arg1(bankId)=EDX,
 * arg2(bankType)=ECX, arg3(flag)=stack):
 *   this[0] = bankId    (byte)
 *   this[1] = flag       (byte, the raw bool argument's low byte)
 *   this[2] = bankType  (byte)
 *
 * Confirmed derived locals:
 *   voiceModelType = (bankType == 0) ? 1 : 2
 *       (`cmp $1,%cl; sbb %eax,%eax; add $2,%eax` -- CF set only when
 *       bankType==0, giving eax=-1+2=1; any other bankType gives eax=0+2=2)
 *   flags = (flag == false) ? 0xE14 : 0x610
 *       (`cmp $1,%bl; sbb %edi,%edi; and $0x804,%edi; add $0x610,%edi` --
 *       CF set only when flag==0, giving edi=-1&0x804+0x610=0xE14;
 *       flag!=0 gives edi=0&0x804+0x610=0x610)
 *
 * Confirmed call sequence:
 *   CSTGProgram *programs = (CSTGProgram*)((char*)this + 3);
 *   programs[0].Initialize(bankId, flags, voiceModelType);       // once
 *   for (i = 1; i < 0x80; i++)
 *       programs[i].Copy(&programs[0], bankId, flags, voiceModelType);
 * (`CSTGProgram::Initialize()`/`Copy()` args confirmed via their own
 * regparm(3) register assignment at each call site -- this=dest program,
 * EDX=src pointer [Copy only], ECX=bankId, stack[0]=flags,
 * stack[4]=voiceModelType [Initialize has no src arg, so its own EDX
 * slot instead carries bankId directly, ECX carries flags, and the sole
 * stack slot carries voiceModelType -- see disassembly, argument COUNT
 * differs by one between the two real callees]).
 *
 * CSTGProgramBank::GetPatchSize() const (`.text+0xa29b0`, 17 bytes) --
 * confirmed real, and confirmed to be a MISNOMER: it does not return a
 * byte count at all, it recomputes the EXACT SAME `flags` formula
 * `Initialize()` derives from its own `flag` argument, reading the
 * stored byte back from `this+1`:
 *   `cmpb $1,0x1(%eax); sbb %eax,%eax; and $0x804,%eax; add $0x610,%eax`
 * i.e. `return (this->_flag != 0) ? 0x610 : 0xE14;` -- byte-identical
 * formula to `Initialize()`'s own `flags` local, just re-derived from
 * stored state instead of a fresh argument. `InitializePerformances()`
 * (init_performances.cpp) already calls this and feeds its result
 * straight into `CSTGProgram::Initialize()`'s own `patchSize` parameter
 * -- confirming that parameter is really the same 0x610/0xE14 flags
 * value, not a byte count either, though this file does not rename that
 * parameter (oa_global.h's own existing declaration already uses the
 * ground-truth-mangled-name-derived label "patchSize").
 *
 * CSTGProgram::Initialize(eSTGProgramBankId, unsigned int, eSTGVoiceModelType)
 * (`.text+0xa4f30`, 415 bytes) and CSTGProgram::Copy(...) (`.text+0xa50d0`,
 * 8620 bytes) are each their own separate, still-substantial bodies --
 * deliberately NOT reconstructed here (out of scope, matches this
 * project's established "reconstruct the orchestrating caller, stub the
 * DSP-scale callee" pattern used throughout bar2_stubs.cpp). Their own
 * no-op stub bodies are DELIBERATELY kept in bar2_stubs.cpp, NOT this
 * file, even though this file is their only real caller -- so that
 * test_program_bank_init.cpp (which links only this TU) can supply its
 * own local call-tracking mocks for both without a multiple-definition
 * conflict, matching the CSTGAudioInput/CSTGToneAdjust precedent in
 * test_program_ctor.cpp.
 */

#include "oa_global.h"

void CSTGProgramBank::Initialize(unsigned int bankId, unsigned int bankType, bool flag)
{
	_bankId = (unsigned char)bankId;
	_flag = flag ? 1 : 0;
	_bankType = (unsigned char)bankType;

	unsigned int voiceModelType = (bankType == 0) ? 1u : 2u;
	unsigned int flags = flag ? 0x610u : 0xE14u;

	CSTGProgram *programs = (CSTGProgram *)((char *)this + 3);
	programs[0].Initialize(bankId, flags, voiceModelType);
	for (unsigned int i = 1; i < 0x80; i++)
		programs[i].Copy(&programs[0], bankId, flags, voiceModelType);
}

unsigned int CSTGProgramBank::GetPatchSize() const
{
	return _flag ? 0x610u : 0xE14u;
}
