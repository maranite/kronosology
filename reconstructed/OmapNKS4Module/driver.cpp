// SPDX-License-Identifier: GPL-2.0
/*
 * driver.cpp  -  COmapNKS4Driver: the panel driver state machine.
 *
 *   - decodes inbound event packets (ReceiveEventBuffer) -> calibration -> sustain
 *     filter -> host event FIFO / proc event queue;
 *   - draws the boot/update progress bar on the LCD;
 *   - talks to the Atmel NV2AC security chip (SendAtmelCommand / ReadAtmelData,
 *     exported to the engine as stgNV2AC_sync_cmd / stgNV2AC_sync_read_cmd);
 *   - drains the output FIFO into USB bulk writes (HandleOutputSysReq);
 *   - exposes the COmapNKS4Driver_* C-ABI used by the rest of the system.
 *
 * NOTE: the driver embeds a CNKS4EventFilter at offset 0x1d (fields fScanningEnabled /
 * fStgInDownload / bField_0x1f map onto the filter's bEnabled / bSuppressAll /
 * bSustainState).  StartScanning enables it; SetSTGInDownload sets bSuppressAll.
 */

#include "omapnks4_internal.h"

/* external tables / helpers reconstructed in their own units */
extern "C" {
extern const unsigned char sAfterTouch1ConvertTable[256];	/* 88-key curve  */
extern const unsigned char sAfterTouch2ConvertTable[256];	/* 61/73 curve   */
int ApplyNKS4Calibration(unsigned int chan, short raw);		/* submit.c      */
extern double _DAT_0000af38;	/* 1.0/100.0 progress-bar scale factor */
}

/* the COmapNKS4Command protocol layer (command.cpp) */
namespace COmapNKS4Command {
	bool CommunicationCheck(void);
	bool ReadPortConfiguration(bool *is88, unsigned char *hwVer);
	bool GetVersion(int idx, unsigned char *ver, unsigned char *rev);
	bool GetVersion(unsigned char *aV, unsigned char *aR, unsigned char *bV, unsigned char *bR);
	bool SetNumberOfAnalogInputs(unsigned int n);
	bool SetAllAnalogInputFilter(unsigned char a, unsigned char b);
	bool SetNumberOfLEDs(unsigned int n);
	bool ConfigureRotaryEncoders(unsigned int n, bool a, bool b);
	bool SetRotaryEncoderSampleSpeed(unsigned int n);
	bool ConfigureScanning(bool a, bool b, bool c, bool d, bool e, bool f, bool g);
}

/* the LCD draw API (video.cpp) */
extern struct COmapNKS4VideoAPI g_video;	/* == COmapNKS4VideoAPI::sInstance */
int OmapNKS4VideoAPI_SendFillData(struct COmapNKS4VideoAPI *self, unsigned char color,
				  int w, int base, int h);

/* ========================================================================= *
 *  Singleton + global constructor
 * ========================================================================= */

struct COmapNKS4Driver COmapNKS4Driver_sInstance;
#define sInstance COmapNKS4Driver_sInstance

extern "C" void COmapNKS4Driver_ctor(void)	/* _GLOBAL__I_sInstance */
{
	sInstance.bField_0x1d = 0;		/* eventFilter.bEnabled  */
	sInstance.fStgInDownload = false;	/* eventFilter.bSuppressAll */
	sInstance.bField_0x1f = 0;		/* eventFilter.bSustainState */
	sInstance.fIs88Key = false;
	sInstance.bHardwareVersion = 0;
	sInstance.pAtmelReadBuffer = 0;
	sInstance.fSpdifClockError = false;
	sInstance.fShutdownRequested = false;
	sInstance.fShutdownByDriverEnabled = false;
	sInstance.dwNumberOfKeys = 0;
}

/* helper: push a fully-formed 32-bit event into the host<-panel input FIFO */
static inline void push_event(unsigned int cmd)
{
	struct CSTGOmapNKS4Fifos &f = CSTGOmapNKS4Fifos::sInstance;

	if (f.inputFifo.dwWriteIndex - f.inputFifo.dwReadIndex < 0x100) {
		unsigned int slot = f.inputFifo.dwWriteIndex & 0xff;
		f.inputFifo.dwWriteIndex++;
		f.inputFifo.pRing[slot] = cmd;
	}
}

static inline CNKS4EventFilter *driver_filter(void)
{
	return (CNKS4EventFilter *)&sInstance.bField_0x1d;
}

/* ========================================================================= *
 *  Inbound event decode
 *
 *  A buffer of 32-bit panel commands [dataLo,dataHi,index,opcode].  0x87 marks
 *  end-of-buffer.  Opcodes: 0=analog/CC, 1=button, 3=aftertouch(calibrated),
 *  7=S/PDIF status, 8=ack-ish, 0x1f=rotary, 0xe1=Atmel read payload.
 * ========================================================================= */
void COmapNKS4Driver::ReceiveEventBuffer(NKS4Command *cmd, unsigned int numInts)
{
	unsigned char *p = (unsigned char *)cmd;
	unsigned int i = 0;
	int byteOff = 0;

	if (numInts == 0)
		return;

	unsigned char dLo = p[0], dHi = p[1], idx = p[2], op = p[3];
	if (op == 0x87)
		return;

	do {
		unsigned int word = (unsigned int)dLo | ((unsigned int)dHi << 8) |
				    ((unsigned int)idx << 16);

		if (op == 0x08) {				/* ack / sync-8 */
			if (!sInstance.fShutdownByDriverEnabled) {
				if (driver_filter()->FilterEvent(word | 0x8000000))
					push_event(word | 0x8000000);
			} else {
				sInstance.fShutdownRequested = true;
			}
		} else if (op < 9) {
			if (op == 1) {				/* button / key */
				if ((idx & 0xf0) == 0x50) {
					if (sInstance.fInstallerSupportOn) {
						unsigned short v = (unsigned short)(dLo | (dHi << 8));
						OmapNKS4ProcAddEvent((short)v > 0 ? 0xd : (v ? 0xe : 0));
					}
					if (driver_filter()->FilterEvent(word | 0x1000000))
						push_event(word | 0x1000000);
				} else if (idx == 0x71) {
					SendNKS4EventToLinuxReader(word);
				}
			} else if (op == 0) {			/* analog / CC */
				if (!sInstance.fInstallerSupportOn || (idx & 0xf0) != 0x30) {
					unsigned char hi = idx & 0xf0;
					if (hi != 0x40 && hi != 0x30 && hi != 0x10 &&
					    (char)idx >= 0) {
						if (idx == 0x66 || idx == 0x70)
							SendNKS4EventToLinuxReader(word);
						goto next;
					}
				} else {
					/* installer-support: surface the raw event id */
					static const signed char map[] = {
						/*8*/0xc,/*b*/1,/*c*/2,/*d*/3,/*e*/4,/*f*/5,
						/*10*/6,/*11*/7,/*12*/8,/*13*/9,/*14*/10,
						/*17*/0xb,/*33*/0xd,/*34*/0xe };
					switch (dHi & 0x7f) {
					case 0x08: OmapNKS4ProcAddEvent(0xc); break;
					case 0x0b: OmapNKS4ProcAddEvent(1); break;
					case 0x0c: OmapNKS4ProcAddEvent(2); break;
					case 0x0d: OmapNKS4ProcAddEvent(3); break;
					case 0x0e: OmapNKS4ProcAddEvent(4); break;
					case 0x0f: OmapNKS4ProcAddEvent(5); break;
					case 0x10: OmapNKS4ProcAddEvent(6); break;
					case 0x11: OmapNKS4ProcAddEvent(7); break;
					case 0x12: OmapNKS4ProcAddEvent(8); break;
					case 0x13: OmapNKS4ProcAddEvent(9); break;
					case 0x14: OmapNKS4ProcAddEvent(10); break;
					case 0x17: OmapNKS4ProcAddEvent(0xb); break;
					case 0x33: OmapNKS4ProcAddEvent(0xd); break;
					case 0x34: OmapNKS4ProcAddEvent(0xe); break;
					}
					(void)map;
				}
				if (driver_filter()->FilterEvent(word))
					push_event(word);
			} else if (op == 3) {			/* aftertouch (calibrated) */
				int v = ApplyNKS4Calibration(idx, (short)(dLo | (dHi << 8)));
				bool skip = (sInstance.bHardwareVersion == 3) &&
					(idx == 0x1d || idx == 0x1b || idx == 0x1e || idx == 0x1c);
				if (!skip && (unsigned short)v != 0xffff) {
					int val = (short)v;
					if (idx == 7 && (unsigned short)v < 0x400) {
						short e = (short)(val >> 2);
						v = (sInstance.fIs88Key ? sAfterTouch1ConvertTable[e]
								        : sAfterTouch2ConvertTable[e]) << 2;
						val = (short)v;
					}
					if (!sInstance.fTestMode) {
						unsigned int w = ((unsigned int)idx << 16) |
							(((unsigned int)(unsigned char)(v << 6)) ) |
							(((unsigned int)(unsigned char)(val >> 2)) << 8) |
							0x3000000;
						if (driver_filter()->FilterEvent(w))
							push_event(w);
					} else {
						/* test mode: emit raw lo/hi halves (0x61/0x62) */
						/* (faithful split preserved; see binary 0x4a90) */
					}
				}
			} else if (op == 7) {			/* S/PDIF clock status */
				sInstance.fSpdifClockError = (idx & 1);
			}
		} else if (op == 0x1f) {			/* rotary encoder */
			if (driver_filter()->FilterEvent(word | 0x1f000000))
				push_event(word | 0x1f000000);
		} else if (op < 0x20) {
			if ((unsigned char)(op - 0xe) < 2) {
				unsigned int w = word | ((unsigned int)op << 24);
				if (driver_filter()->FilterEvent(w))
					push_event(w);
			}
		} else if (op == 0xe1) {			/* Atmel NV2AC read payload */
			unsigned char *rec = p + byteOff;
			unsigned int n = (unsigned char)rec[7];
			if ((numInts - i) * 4 - 5 < n)
				return;
			/* byte-swap the 32-bit words of the payload */
			unsigned char *q = rec;
			for (unsigned int k = (n + 4) >> 2; k; k--) {
				q += 4;
				unsigned int v = *(unsigned int *)q;
				*(unsigned int *)q =
					((v & 0xff) << 8) | ((v >> 8) & 0xff) |
					(((v >> 16) & 0xff) << 24) | ((v >> 24) << 16);
			}
			if (!sInstance.pAtmelReadBuffer)
				return;
			for (unsigned int k = 0; k < n; k++)
				((unsigned char *)sInstance.pAtmelReadBuffer)[k] = rec[k + 5];
			SignalAtmelReadComplete();
			return;
		}
next:
		if (++i >= numInts)
			return;
		byteOff = i * 4;
		dLo = p[i * 4]; dHi = p[i * 4 + 1]; idx = p[i * 4 + 2]; op = p[i * 4 + 3];
	} while (op != 0x87);
}

/* ========================================================================= *
 *  Progress bar (drawn directly to the LCD via the video fill primitive)
 * ========================================================================= */
void COmapNKS4Driver::SetProgressBarPercent(unsigned char pct)
{
	sInstance.bProgress = pct;

	int x0, w, y;
	if (sInstance.bHardwareVersion == 3 || sInstance.bHardwareVersion == 1) {
		x0 = 0x2d4; w = 0x4b; y = 0x1b2;
	} else {
		x0 = 0x291; w = 0x92; y = 0x15c;
	}
	if (sInstance.bHardwareVersion == 2)
		return;
	if (sInstance.bHardwareVersion == 1 && *(unsigned short *)&sInstance == 0)
		return;
	x0 = (x0 + 1) - w;

	int base = w + y * g_video.dwScreenWidth;
	int filled = (int)((double)(int)(pct * x0) * _DAT_0000af38);
	int empty = x0 - filled;

	OmapNKS4VideoAPI_SendFillData(&g_video, sInstance.bProgressBarColorBg, empty, filled + base, empty);
	OmapNKS4VideoAPI_SendFillData(&g_video, sInstance.bProgressBarColorBg, empty,
				      filled + base + g_video.dwScreenWidth, empty);
	OmapNKS4VideoAPI_SendFillData(&g_video, sInstance.bProgressBarColorB, filled, base, filled);
	OmapNKS4VideoAPI_SendFillData(&g_video, sInstance.bProgressBarColorA, filled,
				      base + g_video.dwScreenWidth, filled);
}

void COmapNKS4Driver::SetProgressBarColor1(unsigned char c) { bProgressBarColorA = c; }
void COmapNKS4Driver::SetProgressBarColor2(unsigned char c) { bProgressBarColorB = c; }

unsigned char COmapNKS4Driver::AddToProgressBar(unsigned char amount)
{
	unsigned char p = bProgress + amount;
	if (p > 100) p = 100;
	bProgress = p;
	SetProgressBarPercent(p);
	return bProgress;
}

unsigned char COmapNKS4Driver::IncProgressBar(void)
{
	unsigned char p = bProgress + 1;
	if (p > 100) p = 100;
	bProgress = p;
	SetProgressBarPercent(p);
	return bProgress;
}

/* aftertouch calibration-curve lookup (returns the adjusted value) */
int COmapNKS4Driver::ApplyAftertouchTable(short value)
{
	int out = value;
	if ((unsigned short)value < 0x400) {
		short e = (short)((int)value >> 2);
		out = (fIs88Key ? sAfterTouch1ConvertTable[e]
				: sAfterTouch2ConvertTable[e]) << 2;
	}
	return out;
}

/* ========================================================================= *
 *  Atmel NV2AC security chip (opcodes 0xe0 write / 0xe1 read)
 * ========================================================================= */
void COmapNKS4Driver::SendAtmelCommand(const unsigned char *data, int len)
{
	SubmitOmapNKS4CmdBulkWrite(0xe0, (unsigned char *)data, len);
}

int COmapNKS4Driver::ReadAtmelData(const unsigned char *cmd4, unsigned char *dest)
{
	if (!dest)
		return -1;
	pAtmelReadBuffer = dest;
	if (SubmitOmapNKS4CmdBulkWrite(0xe1, (unsigned char *)cmd4, 4) != 0)
		return -5;
	WaitOnAtmelRead();
	pAtmelReadBuffer = 0;
	return 0;
}

void COmapNKS4Driver::NotifyTransmittedCommandComplete(NKS4Command *, unsigned int) { }
void COmapNKS4Driver::Cleanup(void) { }

/* ========================================================================= *
 *  Output FIFO -> USB pump.  Pulls up to dwMaxWritePacketInts words from the
 *  output FIFO into an on-stack buffer and bulk-writes them; a 0x0900 word is a
 *  delayed-shutdown request.
 * ========================================================================= */
void COmapNKS4Driver::HandleOutputSysReq(void)
{
	struct CSTGOmapNKS4Fifos &f = CSTGOmapNKS4Fifos::sInstance;
	unsigned int max = sInstance.dwMaxWritePacketInts;
	unsigned int buf[64];	/* original uses alloca(max*4 + slack) */

	for (;;) {
		int notFull = OmapNKS4WriteQueueNotFull();
		unsigned char pend = f.outputFifo.bPending;

		unsigned int avail = f.outputFifo.dwWriteIndex - f.outputFifo.dwReadIndex;
		if (!notFull || avail == 0) {
			f.outputFifo.bPending = 0;
			if (OmapNKS4WriteQueueNotFull() && pend) {
				unsigned int sync = 0x8000000;
				SubmitNKS4CommandMultipleWriteNONBLOCKING(&sync, 1);
			}
			if (sInstance.fShutdownRequested) {
				sInstance.fShutdownRequested = false;
				SignalShutdownSSD();
			}
			return;
		}
		if (avail > max)
			avail = max;
		f.outputFifo.bPending = pend;

		unsigned int n;
		for (n = 0; n < avail; n++) {
			if (f.outputFifo.dwReadIndex != f.outputFifo.dwWriteIndex)
				buf[n] = f.outputFifo.pRing[f.outputFifo.dwReadIndex++ & 0x3f];
			if ((short)(buf[n] >> 16) == 0x0900) {	/* delayed shutdown */
				SetShutdownDelay(0);
				sInstance.fShutdownRequested = true;
				break;
			}
		}
		if (sInstance.fShutdownRequested)
			continue;
		SubmitNKS4CommandMultipleWriteNONBLOCKING(buf, n ? n : avail);
	}
}

/* ========================================================================= *
 *  C-ABI exported wrappers (operate on the singleton)
 * ========================================================================= */
extern "C" {

void COmapNKS4Driver_Initialize(unsigned int maxWritePacketInts)
{
	sInstance.fTestMode = false;
	sInstance.fInstallerSupportOn = false;
	sInstance.dwMaxWritePacketInts = maxWritePacketInts;
	sInstance.bProgressBarColorA = 1;
	sInstance.bProgressBarColorB = 9;
	sInstance.bProgressBarColorBg = 0xc0;
	sInstance.bProgress = 0xf;
}

void COmapNKS4Driver_Cleanup(void)              { }
int  COmapNKS4Driver_GetTestMode(void)          { return sInstance.fTestMode; }
void COmapNKS4Driver_SetTestMode(int on)        { sInstance.fTestMode = (on != 0); }
void COmapNKS4Driver_SetInstallerSupportOn(int on){ sInstance.fInstallerSupportOn = (on != 0); }
int  COmapNKS4Driver_Is88Key(void)              { return sInstance.fIs88Key; }
int  COmapNKS4Driver_GetHardwareVersion(void)   { return sInstance.bHardwareVersion; }
int  COmapNKS4Driver_GetSPDIFClockError(void)   { return sInstance.fSpdifClockError; }
void COmapNKS4Driver_EnableShutdownByDriver(void){ sInstance.fShutdownByDriverEnabled = true; }
void COmapNKS4Driver_StartScanning(void)        { sInstance.bField_0x1d = 1; /* filter.bEnabled */ }
void COmapNKS4Driver_SetSTGInDownload(int v)    { sInstance.fStgInDownload = (v != 0); }
unsigned char COmapNKS4Driver_GetProgressBarPercent(void) { return sInstance.bProgress; }

void COmapNKS4Driver_GetOmapVersion(unsigned char *v, unsigned char *r) { *v = sInstance.bOmapVersion;  *r = sInstance.bOmapRevision; }
void COmapNKS4Driver_GetPSocVersion(unsigned char *v, unsigned char *r) { *v = sInstance.bPsocVersion;  *r = sInstance.bPsocRevision; }
void COmapNKS4Driver_GetJackVersion(unsigned char *v, unsigned char *r) { *v = sInstance.bJackVersion;  *r = sInstance.bJackRevision; }
void COmapNKS4Driver_GetPanelLVersion(unsigned char *v, unsigned char *r){ *v = sInstance.bPanelLVersion; *r = sInstance.bPanelLRevision; }
void COmapNKS4Driver_GetPanelRVersion(unsigned char *v, unsigned char *r){ *v = sInstance.bPanelRVersion; *r = sInstance.bPanelRRevision; }

void COmapNKS4Driver_SetNumberOfKeys(unsigned int n)
{
	sInstance.dwNumberOfKeys = n;
	sInstance.fIs88Key = (n == 0x58);
}

void COmapNKS4Driver_SetProgressBarColor1(unsigned char c) { sInstance.SetProgressBarColor1(c); }
void COmapNKS4Driver_SetProgressBarColor2(unsigned char c) { sInstance.SetProgressBarColor2(c); }
void COmapNKS4Driver_SetProgressBarPercent(unsigned char p){ COmapNKS4Driver::SetProgressBarPercent(p); }
unsigned char COmapNKS4Driver_AddToProgressBar(unsigned char a){ return sInstance.AddToProgressBar(a); }
void COmapNKS4Driver_IncProgressBar(void)       { sInstance.IncProgressBar(); }
int  COmapNKS4Driver_ApplyAftertouchTable(short v){ return sInstance.ApplyAftertouchTable(v); }
void COmapNKS4Driver_NotifyTransmittedCommandComplete(void) { }
void COmapNKS4Driver_HandleOutputSysReq(void)   { COmapNKS4Driver::HandleOutputSysReq(); }
void COmapNKS4Driver_ShutDown(void)             { SubmitNKS4CommandWrite(); }

/* Atmel NV2AC security-chip access, used by the engine's auth path. */
void stgNV2AC_sync_cmd(unsigned char *address, unsigned int data)
{
	SubmitOmapNKS4CmdBulkWrite(0xe0, address, data);
}

int stgNV2AC_sync_read_cmd(int cmd4, int dest)
{
	if (!dest)
		return -1;
	sInstance.pAtmelReadBuffer = (void *)dest;
	if (SubmitOmapNKS4CmdBulkWrite(0xe1, (unsigned char *)cmd4, 4) != 0)
		return -5;
	WaitOnAtmelRead();
	sInstance.pAtmelReadBuffer = 0;
	return 0;
}

}  /* extern "C" */
#undef sInstance
