/*
 * test_spr_clock_handler.cpp  -  host-side known-answer test for
 * CSPRClockHandler's round-62 13-method batch (solo, 2026-07-29). See
 * include/oa_ckg_midi_msg_handler.h for the full derivation.
 */
#include <cstdio>
#include <cstring>
#include "oa_ckg_midi_msg_handler.h"

unsigned char *CSPRClockHandler::ms_poInstance;
unsigned int CSPRClockHandler::ms_oStatusMaster;

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	unsigned char buf[0x80];
	memset(buf, 0, sizeof(buf));
	CSPRClockHandler *h = reinterpret_cast<CSPRClockHandler *>(buf);

	CSPRClockHandler::_GLOBAL__I_ms_poInstance();
	check("_GLOBAL__I_ms_poInstance: no-op, no crash", true);

	*(int *)(buf + 0xc) = 2;
	h->DisableStop();
	check("DisableStop increments this+0xc", *(int *)(buf + 0xc) == 3);
	h->EnableStop();
	h->EnableStop();
	h->EnableStop();
	check("EnableStop decrements, clamped at 0", *(int *)(buf + 0xc) == 0);
	h->EnableStop();
	check("EnableStop stays clamped at 0 (does not go negative)", *(int *)(buf + 0xc) == 0);

	*(int *)(buf + 4) = 0;
	buf[1] = 0xff;
	h->DisableToProcessWhen1ClockUp();
	check("DisableToProcessWhen1ClockUp increments this+4, clears this+1",
	      *(int *)(buf + 4) == 1 && buf[1] == 0);

	*(int *)(buf + 4) = 2;
	buf[1] = 0;
	h->EnableToProcessWhen1ClockUp();
	check("EnableToProcessWhen1ClockUp: count>1 -> decrements only, flag unset",
	      *(int *)(buf + 4) == 1 && buf[1] == 0);
	h->EnableToProcessWhen1ClockUp();
	check("EnableToProcessWhen1ClockUp: count reaches 0 -> sets this+1",
	      *(int *)(buf + 4) == 0 && buf[1] == 1);
	buf[1] = 0;
	h->EnableToProcessWhen1ClockUp();
	check("EnableToProcessWhen1ClockUp: count already 0 -> no-op", *(int *)(buf + 4) == 0 && buf[1] == 0);

	h->InitializeTempo(120);
	check("InitializeTempo writes this+0x64/+0x68/+0x38",
	      *(int *)(buf + 0x64) == 120 && *(int *)(buf + 0x68) == 120 && *(int *)(buf + 0x38) == 120);

	*(int *)(buf + 0x60) = 0;
	*(int *)(buf + 0x6c) = 140;
	*(int *)(buf + 0x64) = 0;
	h->ChangeTempoWhenStarting();
	check("ChangeTempoWhenStarting: gate==0 and pending!=0 -> copies to this+0x64",
	      *(int *)(buf + 0x64) == 140);

	*(int *)(buf + 0x60) = 1;
	*(int *)(buf + 0x64) = 99;
	h->ChangeTempoWhenStarting();
	check("ChangeTempoWhenStarting: gate!=0 -> no-op", *(int *)(buf + 0x64) == 99);

	*(int *)(buf + 0x60) = 0;
	*(int *)(buf + 0x6c) = 0;
	*(int *)(buf + 0x64) = 99;
	h->ChangeTempoWhenStarting();
	check("ChangeTempoWhenStarting: pending==0 -> no-op", *(int *)(buf + 0x64) == 99);

	*(int *)(buf + 0x20) = 4;
	*(int *)(buf + 0x24) = 5;
	h->SetLocationInfoWhenStop(77);
	check("SetLocationInfoWhenStop writes this+0x3c and copies +0x20/+0x24 to +0x40/+0x44",
	      *(int *)(buf + 0x3c) == 77 && *(int *)(buf + 0x40) == 4 && *(int *)(buf + 0x44) == 5);

	CSPRClockHandler::ms_oStatusMasterTick = 0xdeadbeef;
	*(int *)(buf + 8) = 9;
	*(int *)(buf + 0xc) = 9;
	h->ModeOn();
	check("ModeOn zeroes ms_oStatusMasterTick and this+8/+0xc",
	      CSPRClockHandler::ms_oStatusMasterTick == 0 && *(int *)(buf + 8) == 0 && *(int *)(buf + 0xc) == 0);

	{
		CSPRTimerStatus status;
		status.bar = 12;
		status.tick = 34;
		h->CopyStatusLocalToMaster(&status);
		check("CopyStatusLocalToMaster writes ms_oStatusMaster/ms_oStatusMasterTick",
		      CSPRClockHandler::ms_oStatusMaster == 12 && CSPRClockHandler::ms_oStatusMasterTick == 34);
	}

	CSPRClockHandler::CopyStatusMasterToLocal();
	check("CopyStatusMasterToLocal copies master into ms_oStatusLocalCopy",
	      CSPRClockHandler::ms_oStatusLocalCopy[0] == 12 && CSPRClockHandler::ms_oStatusLocalCopy[1] == 34);

	*(int *)(buf + 0x14) = 100;
	*(int *)(buf + 0x18) = 200;
	int bar = 0, tick = 0;
	h->GetCurrentLocation(&bar, &tick);
	check("GetCurrentLocation(2-arg overload) reads this+0x14/+0x18", bar == 100 && tick == 200);

	{
		unsigned char evBuf[16];
		memset(evBuf, 0, sizeof(evBuf));
		/* real bar value 5, stored big-endian at event+6 per ground truth's
		 * own byte-swap (matching a real MIDI-style 16-bit BE field). */
		evBuf[6] = 0;
		evBuf[7] = 5;
		*(int *)(buf + 0x14) = 5;
		bool r = h->HandleBarEventBackward(reinterpret_cast<CSeqEvent *>(evBuf));
		check("HandleBarEventBackward: event bar == current bar -> true (<=)", r == true);

		evBuf[7] = 6;
		r = h->HandleBarEventBackward(reinterpret_cast<CSeqEvent *>(evBuf));
		check("HandleBarEventBackward: event bar > current bar -> false", r == false);

		evBuf[7] = 4;
		r = h->HandleBarEventBackward(reinterpret_cast<CSeqEvent *>(evBuf));
		check("HandleBarEventBackward: event bar < current bar -> true", r == true);
	}

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
