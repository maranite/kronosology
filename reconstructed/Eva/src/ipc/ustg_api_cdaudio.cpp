/*
 * ustg_api_cdaudio.cpp  -  see include/ustg_api_cdaudio.h.
 *
 * Transcribed field-by-field from `objdump -dr -M intel` (Decomp/EVA_Decomp/Eva,
 * .text+0x08e1b3f0-0x08e1b6f0). Every SetChanXxx()/SetLevel() call below passes
 * its own real (opcode, a, b) order to USTGAPISampling::SendSimpleMessage() --
 * confirmed per-function, not assumed by analogy (e.g. SetChanLevel/SetChanPan
 * both put `level`/`pan` before `chan`, matching SendSimpleMessage()'s own real
 * payload1=arg2 slot -- see ustg_api_sampling.h).
 */

#include "ustg_api_cdaudio.h"
#include "ustg_api_sampling.h"

#include <cstring>

bool USTGAPICDAudio::PlayStandby(const char *name, unsigned long a2, unsigned int a3, unsigned long a4, unsigned int a5)
{
	void *scratch = USTGAPISampling::SharedScratch();
	if (!scratch)
		return false;

	USTGAPISampling::SendSimpleMessage(0x33, static_cast<int>(a3), static_cast<int>(a5));

	strncpy(static_cast<char *>(scratch), name, 0xff);
	static_cast<char *>(scratch)[0xff] = '\0';

	char buf[24];
	if (!USTGAPISampling::ReceiveMessage(buf, 0x32, static_cast<int>(a4), static_cast<int>(a2)))
		return false;

	unsigned int payload2;
	memcpy(&payload2, buf + 0x14, sizeof(payload2));
	return payload2 == 0;
}

bool USTGAPICDAudio::PlayStart()
{
	unsigned long local = 0;
	if (!USTGAPISampling::ReceiveSimpleMessage(0x34, local))
		return false;
	return local == 0;
}

bool USTGAPICDAudio::PlayStop()
{
	unsigned long local = 0;
	if (!USTGAPISampling::ReceiveSimpleMessage(0x35, local))
		return false;
	return local == 0;
}

bool USTGAPICDAudio::GetCurrentPosition(unsigned long &position, EAudioStatus &status)
{
	char buf[24];
	if (!USTGAPISampling::ReceiveMessage(buf, 0x36, 0, 0))
		return false;

	int payload0;
	memcpy(&payload0, buf + 0xc, sizeof(payload0));
	if (payload0 < 0) {
		position = 0;
		status = 3;
		return false;
	}

	unsigned int payload2;
	memcpy(&payload2, buf + 0x14, sizeof(payload2));
	position = payload2;
	status = static_cast<EAudioStatus>(payload0);
	return true;
}

void USTGAPICDAudio::SetLevel(unsigned char level)
{
	USTGAPISampling::SendSimpleMessage(0x37, level, 0);
}

void USTGAPICDAudio::SetChanLevel(unsigned char chan, unsigned char level)
{
	USTGAPISampling::SendSimpleMessage(0x38, level, chan);
}

void USTGAPICDAudio::SetChanPan(unsigned char chan, unsigned char pan)
{
	USTGAPISampling::SendSimpleMessage(0x39, pan, chan);
}

void USTGAPICDAudio::SetChanBusSelect(unsigned char chan, eSTGAPIBusIDOut busId)
{
	USTGAPISampling::SendSimpleMessage(0x3a, static_cast<int>(busId), chan);
}

void USTGAPICDAudio::SetChanSend1Level(unsigned char chan, unsigned char level)
{
	USTGAPISampling::SendSimpleMessage(0x3b, level, chan);
}

void USTGAPICDAudio::SetChanSend2Level(unsigned char chan, unsigned char level)
{
	USTGAPISampling::SendSimpleMessage(0x3c, level, chan);
}

void USTGAPICDAudio::SetChanFXControlBus(unsigned char chan, eSTGAPIFXCtrlBus bus)
{
	USTGAPISampling::SendSimpleMessage(0x3d, static_cast<int>(bus), chan);
}

void USTGAPICDAudio::SetChanHDRBus(unsigned char chan, eSTGAPIHDRBus bus)
{
	USTGAPISampling::SendSimpleMessage(0x3e, static_cast<int>(bus), chan);
}
