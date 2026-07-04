// SPDX-License-Identifier: GPL-2.0
/*
 * controller_rt_data_init.cpp  -  CSTGControllerRTData::Initialize()/
 * RequestAnalogInputPositions() (sec 10.88).
 *
 * Deliberately a separate translation unit from global.cpp: Initialize()'s
 * own mock in test_global.cpp is a load-bearing call-counter for
 * CSTGGlobal::Initialize()'s own dispatch verification, matching the
 * same reasoning as midi_queue_writer.cpp/waveseq_setlist_init.cpp/
 * alias_bank_init.cpp/smoother_init.cpp (sec 10.83-10.86).
 */

#include "oa_global.h"
#include "oa_setup_global_resources.h"

/*
 * CSTGControllerRTData::Initialize() (.text+0xd5a0 in OA_real.ko) --
 * confirmed real: a trivial tail-call to RequestAnalogInputPositions().
 */
void CSTGControllerRTData::Initialize()
{
	RequestAnalogInputPositions();
}

/*
 * CSTGControllerRTData::RequestAnalogInputPositions()
 * (.text+0xd470 in OA_real.ko) -- confirmed real: 19 calls to
 * CSTGFrontPanel::sInstance->RequestAnalogInputStatus(deviceCode),
 * with device codes 0x8..0x17 (16 values) in ascending order followed
 * by 0x1a, 0x19, 0x18 (26, 25, 24) in DESCENDING order -- a genuine
 * confirmed real quirk in the disassembly's own call ordering,
 * reproduced verbatim rather than "cleaned up" into a single loop.
 */
void CSTGControllerRTData::RequestAnalogInputPositions()
{
	for (unsigned int code = 0x8; code <= 0x17; code++)
		CSTGFrontPanel::sInstance->RequestAnalogInputStatus(code);
	CSTGFrontPanel::sInstance->RequestAnalogInputStatus(0x1a);
	CSTGFrontPanel::sInstance->RequestAnalogInputStatus(0x19);
	CSTGFrontPanel::sInstance->RequestAnalogInputStatus(0x18);
}

/*
 * CSTGFrontPanel::RequestAnalogInputStatus() (sec 10.131): see
 * oa_setup_global_resources.h for the full confirmed shape.
 */
void CSTGFrontPanel::RequestAnalogInputStatus(unsigned int deviceCode)
{
	int command = 0x1a00000 | (((int)deviceCode & 0xff) << 8);
	OmapNKS4OutputFifo_WriteCommand(command);
}
