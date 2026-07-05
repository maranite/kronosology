// SPDX-License-Identifier: GPL-2.0
/*
 * audio_input_mixer.cpp  -  CSTGAudioInputMixerBase's four setters (sec
 * 10.150): SetPan/SetFXCtrlBus/SetOutputBus/SetHDRBus.
 *
 * Deliberately a SEPARATE translation unit from global.cpp (matching the
 * CSTGMidiQueueWriter::Write precedent, sec 10.83/10.150): test_engine.cpp,
 * test_global.cpp, and test_global_ctor.cpp all carry their own
 * PRE-EXISTING call-counting mocks for these four methods, load-bearing
 * for roughly twenty CSTGAudioInput-focused assertions across them --
 * rewiring all of that onto the real bodies is a separate, larger task,
 * deliberately left untouched this pass. The real bodies here are instead
 * exercised directly by their own dedicated verify/test_audio_input_mixer.cpp.
 *
 * All five data tables below were extracted directly from OA_real.ko's own
 * `.rodata` section (file offset = section file offset 0x5d3800 + the
 * symbol's own address, confirmed via readelf -S/-sW; independently
 * confirmed to carry NO relocations in that byte range via readelf -r, so
 * these are genuine raw integer data, not unresolved pointers) -- not
 * guessed or inferred from call-site behavior.
 */

#include "oa_global.h"

static unsigned char *FromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}

/* STGAPIOutToBusType[26]/STGAPIOutToPhysBusId[26] (sec 10.150, confirmed
 * real .rodata, 0x68 bytes each, indexed by the caller's own
 * eSTGAPIBusIDOut `value`) -- used by SetOutputBus. */
static const int STGAPIOutToBusType[26] = {
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 4, 3, 4, 3, 4, 3, 4, 2, 2, 2, 2, 0
};
static const int STGAPIOutToPhysBusId[26] = {
	48, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76,
	38, 38, 40, 40, 42, 42, 44, 44, 38, 40, 42, 44, 32
};

/* STGAPIFXCtrlToWritePhysBusId[3] (confirmed real .rodata, 12 bytes) --
 * used by SetFXCtrlBus. */
static const int STGAPIFXCtrlToWritePhysBusId[3] = { 32, 78, 80 };

/* STGAPIHDRBusTypes[7]/STGAPIHDRPhysBusIds[7] (confirmed real .rodata,
 * 28 bytes each) -- used by SetHDRBus. */
static const int STGAPIHDRBusTypes[7] = { 0, 3, 4, 3, 4, 2, 2 };
static const int STGAPIHDRPhysBusIds[7] = { 32, 146, 146, 148, 148, 146, 148 };

/*
 * SetPan(unsigned int, float) (`.text+0x68df0`, 84 bytes) confirmed:
 * calls `CSTGPan::CalculateMonoPanCoeffs(coeffs, 1.0f, value)` into a
 * local `STGMonoPanCoeffs`, then stores the two result floats at
 * `mixerStateArray + busIndex*0x90 + 0x0`/`+0x4`.
 */
void CSTGAudioInputMixerBase::SetPan(unsigned int busIndex, float value)
{
	STGMonoPanCoeffs coeffs;
	CSTGPan::CalculateMonoPanCoeffs(coeffs, 1.0f, value);

	unsigned char *entry = FromU32(mixerStateArray32) + busIndex * 0x90;
	*(float *)(entry + 0x0) = coeffs.coeff0;
	*(float *)(entry + 0x4) = coeffs.coeff4;
}

/*
 * SetFXCtrlBus(unsigned int, int) (`.text+0x68ea0`, 54 bytes) confirmed:
 * a RAW indirect call through this object's own vtable slot 3 (`call
 * *0xc(%esi)`, `this`=eax, arg=edx unchanged from entry -- NOT this
 * project's C++ virtual dispatch, matching the established raw-vtable
 * convention, sec 10.149), passing `STGAPIFXCtrlToWritePhysBusId[value]`;
 * the returned int is stored at `mixerStateArray + busIndex*0x90 + 0x68`.
 */
void CSTGAudioInputMixerBase::SetFXCtrlBus(unsigned int busIndex, int value)
{
	typedef int (*Fn)(void *, int);
	Fn fn = ((Fn *)(*(void ***)this))[3];
	int result = fn(this, STGAPIFXCtrlToWritePhysBusId[value]);

	unsigned char *entry = FromU32(mixerStateArray32) + busIndex * 0x90;
	*(int *)(entry + 0x68) = result;
}

/*
 * SetOutputBus(unsigned int, int) (`.text+0x68e50`, 68 bytes) confirmed:
 * calls `StartBusChange()` as a genuine MEMBER method on the embedded
 * `CBusChangeStateMachine` at `busChangeArray + busIndex*0x10` (this
 * project's oa_global.h has the full confirmed register mapping).
 */
void CSTGAudioInputMixerBase::SetOutputBus(unsigned int busIndex, int value)
{
	CBusChangeStateMachine *bcsm =
		(CBusChangeStateMachine *)(FromU32(busChangeArray32) + busIndex * 0x10);
	bcsm->StartBusChange(STGAPIOutToPhysBusId[value], STGAPIOutToBusType[value], 0x38);
}

/*
 * SetHDRBus(unsigned int, int) (`.text+0x68ee0`, 170 bytes) confirmed:
 * dispatches the SAME raw vtable slot 3 as SetFXCtrlBus (passing
 * `STGAPIHDRPhysBusIds[value]`), storing the result at
 * `mixerStateArray + busIndex*0x90 + 0x6c`; then calls
 * `CSTGBusInfo::GetSignalSelectionForBusType(STGAPIHDRBusTypes[value])`
 * and, based on the confirmed real return-value cases (0/1/2/other),
 * deterministically sets three further fields at `+0x50`/`+0x54`/`+0x58`
 * of the same array entry (see oa_engine.h-style header comments for
 * the per-branch derivation -- this is a real 4-way branch, not a
 * simplification).
 */
void CSTGAudioInputMixerBase::SetHDRBus(unsigned int busIndex, int value)
{
	unsigned char *entry = FromU32(mixerStateArray32) + busIndex * 0x90;

	typedef int (*Fn)(void *, int);
	Fn fn = ((Fn *)(*(void ***)this))[3];
	int routed = fn(this, STGAPIHDRPhysBusIds[value]);
	*(int *)(entry + 0x6c) = routed;

	int signalSelection = CSTGBusInfo::GetSignalSelectionForBusType(STGAPIHDRBusTypes[value]);

	if (signalSelection == 1) {
		*(int *)(entry + 0x50) = -1;
		*(int *)(entry + 0x54) = 0;
		*(int *)(entry + 0x58) = 0;
	} else if (signalSelection == 2) {
		*(int *)(entry + 0x50) = 0;
		*(int *)(entry + 0x54) = -1;
		*(int *)(entry + 0x58) = 0;
	} else if (signalSelection == 0) {
		*(int *)(entry + 0x50) = 0;
		*(int *)(entry + 0x54) = 0;
		*(int *)(entry + 0x58) = -1;
	} else {
		*(int *)(entry + 0x50) = 0;
		*(int *)(entry + 0x54) = 0;
		*(int *)(entry + 0x58) = 0;
	}
}
