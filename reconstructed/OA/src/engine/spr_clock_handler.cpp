/*
 * spr_clock_handler.cpp  -  CSPRClockHandler's round-62 13-method batch.
 * See oa_ckg_midi_msg_handler.h's own header comment (CSPRClockHandler
 * section) for the full derivation and the deferred-item list.
 */
#include "oa_ckg_midi_msg_handler.h"

unsigned int CSPRClockHandler::ms_oStatusMasterTick;
unsigned int CSPRClockHandler::ms_oStatusLocalCopy[2];

void CSPRClockHandler::_GLOBAL__I_ms_poInstance()
{
}

void CSPRClockHandler::DisableStop()
{
	unsigned char *base = (unsigned char *)this;
	*(int *)(base + 0xc) += 1;
}

void CSPRClockHandler::EnableStop()
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xc) - 1;
	*(int *)(base + 0xc) = (v < 0) ? 0 : v;
}

void CSPRClockHandler::DisableToProcessWhen1ClockUp()
{
	unsigned char *base = (unsigned char *)this;
	*(int *)(base + 4) += 1;
	base[1] = 0;
}

void CSPRClockHandler::EnableToProcessWhen1ClockUp()
{
	unsigned char *base = (unsigned char *)this;
	if (*(int *)(base + 4) > 0) {
		int v = *(int *)(base + 4) - 1;
		*(int *)(base + 4) = v;
		if (v == 0)
			base[1] = 1;
	}
}

void CSPRClockHandler::InitializeTempo(int tempo)
{
	unsigned char *base = (unsigned char *)this;
	*(int *)(base + 100) = tempo; /* 0x64 */
	*(int *)(base + 0x68) = tempo;
	*(int *)(base + 0x38) = tempo;
}

void CSPRClockHandler::ChangeTempoWhenStarting()
{
	unsigned char *base = (unsigned char *)this;
	if (*(int *)(base + 0x60) == 0 && *(int *)(base + 0x6c) != 0)
		*(int *)(base + 100) = *(int *)(base + 0x6c); /* 0x64 */
}

void CSPRClockHandler::SetLocationInfoWhenStop(int value)
{
	unsigned char *base = (unsigned char *)this;
	*(int *)(base + 0x3c) = value;
	*(int *)(base + 0x40) = *(int *)(base + 0x20);
	*(int *)(base + 0x44) = *(int *)(base + 0x24);
}

void CSPRClockHandler::ModeOn()
{
	ms_oStatusMasterTick = 0;
	unsigned char *base = (unsigned char *)this;
	*(int *)(base + 8) = 0;
	*(int *)(base + 0xc) = 0;
}

void CSPRClockHandler::CopyStatusLocalToMaster(CSPRTimerStatus *status)
{
	ms_oStatusMaster = status->bar;
	ms_oStatusMasterTick = status->tick;
}

void CSPRClockHandler::CopyStatusMasterToLocal()
{
	ms_oStatusLocalCopy[0] = ms_oStatusMaster;
	ms_oStatusLocalCopy[1] = ms_oStatusMasterTick;
}

void CSPRClockHandler::GetCurrentLocation(int *bar, int *tick)
{
	unsigned char *base = (unsigned char *)this;
	*bar = *(int *)(base + 0x14);
	*tick = *(int *)(base + 0x18);
}

bool CSPRClockHandler::HandleBarEventBackward(CSeqEvent *ev)
{
	unsigned char *base = (unsigned char *)this;
	const unsigned char *evBytes = (const unsigned char *)ev;
	unsigned short raw;
	__builtin_memcpy(&raw, evBytes + 6, 2);
	unsigned short swapped = (unsigned short)((raw << 8) | (raw >> 8));
	return (int)(unsigned int)swapped <= *(int *)(base + 0x14);
}
