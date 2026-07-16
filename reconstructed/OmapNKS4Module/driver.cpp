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

/* NOT extern: confirmed via Ghidra (2026-07-16) these are genuinely STATIC
 * (internal-linkage) data in the real binary - their real mangled names
 * (_ZL24sAfterTouch1ConvertTable / _ZL24sAfterTouch2ConvertTable, the "L"
 * marking internal linkage in Itanium mangling) confirm it, and they were
 * simply never resolvable as the extern this reconstruction declared them
 * as ("Unknown symbol sAfterTouch1ConvertTable/sAfterTouch2ConvertTable" at
 * insmod). Byte content read directly from the real 3.2.2 OmapNKS4Module.ko
 * (0x193e0/0x194e0, both indexed by ApplyAftertouchTable via raw>>2, output
 * <<2 - see COmapNKS4Driver::ApplyAftertouchTable below) via
 * read_memory, not guessed. sAfterTouch2ConvertTable's first 160 entries
 * being 0 matches a 61/73-key panel's narrower/less sensitive aftertouch
 * range relative to the 88-key curve. */
static const unsigned char sAfterTouch1ConvertTable[256] = {	/* 88-key curve */
	0x00,0x01,0x01,0x02,0x02,0x03,0x03,0x04,0x04,0x05,0x05,0x06,0x07,0x07,0x08,0x08,
	0x09,0x09,0x0a,0x0a,0x0b,0x0c,0x0c,0x0d,0x0d,0x0e,0x0e,0x0f,0x0f,0x10,0x10,0x11,
	0x12,0x12,0x13,0x13,0x14,0x14,0x15,0x15,0x16,0x17,0x17,0x18,0x18,0x19,0x19,0x1a,
	0x1a,0x1b,0x1b,0x1c,0x1d,0x1d,0x1e,0x1e,0x1f,0x1f,0x20,0x20,0x21,0x21,0x22,0x23,
	0x23,0x24,0x24,0x25,0x25,0x26,0x26,0x27,0x28,0x28,0x29,0x29,0x2a,0x2a,0x2b,0x2b,
	0x2c,0x2c,0x2d,0x2e,0x2e,0x2f,0x2f,0x30,0x30,0x31,0x31,0x32,0x33,0x33,0x34,0x34,
	0x35,0x35,0x36,0x36,0x37,0x38,0x38,0x39,0x3a,0x3b,0x3b,0x3c,0x3d,0x3e,0x3e,0x3f,
	0x40,0x41,0x41,0x42,0x43,0x44,0x45,0x45,0x46,0x47,0x48,0x48,0x49,0x4a,0x4b,0x4c,
	0x4d,0x4e,0x4f,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5b,0x5c,0x5d,
	0x5e,0x5f,0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,
	0x6f,0x70,0x71,0x72,0x73,0x74,0x75,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
	0x81,0x82,0x83,0x85,0x86,0x88,0x89,0x8b,0x8c,0x8e,0x8f,0x91,0x92,0x94,0x95,0x97,
	0x98,0x9a,0x9b,0x9d,0x9e,0x9f,0xa1,0xa2,0xa4,0xa5,0xa7,0xa8,0xaa,0xab,0xad,0xae,
	0xb0,0xb1,0xb3,0xb4,0xb6,0xb7,0xb9,0xba,0xbb,0xbd,0xbe,0xc0,0xc2,0xc4,0xc5,0xc7,
	0xc9,0xcb,0xcc,0xce,0xd0,0xd2,0xd3,0xd5,0xd7,0xd9,0xda,0xdc,0xde,0xe0,0xe1,0xe3,
	0xe5,0xe7,0xe8,0xea,0xec,0xee,0xef,0xf1,0xf3,0xf5,0xf6,0xf8,0xfa,0xfc,0xfd,0xff,
};
static const unsigned char sAfterTouch2ConvertTable[256] = {	/* 61/73-key curve */
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x02,0x04,0x04,0x04,0x04,0x05,0x05,0x05,0x05,0x07,0x09,0x09,0x09,0x09,0x09,
	0x0b,0x0b,0x0d,0x0e,0x10,0x14,0x14,0x14,0x15,0x15,0x17,0x19,0x19,0x19,0x1b,0x1b,
	0x1d,0x1d,0x1e,0x20,0x22,0x24,0x24,0x26,0x27,0x27,0x29,0x2d,0x2d,0x30,0x32,0x32,
	0x34,0x36,0x36,0x39,0x3b,0x3b,0x3f,0x40,0x42,0x44,0x46,0x49,0x4b,0x4f,0x52,0x56,
	0x5a,0x5b,0x5d,0x62,0x66,0x68,0x6d,0x6f,0x73,0x78,0x7c,0x81,0x86,0x8c,0x8f,0x95,
	0x9a,0x9f,0xa3,0xa6,0xaa,0xaf,0xb5,0xba,0xc0,0xc9,0xd0,0xd9,0xe3,0xec,0xfb,0xff,
};
extern "C" {
int ApplyNKS4Calibration(unsigned int chan, short raw);		/* submit.c      */
}
/* Genuinely never defined anywhere despite the extern declaration + comment
 * already documenting its value ("Unknown symbol _DAT_0000af38" at insmod,
 * confirmed on real hardware, 2026-07-16) - a leftover Ghidra data-address
 * placeholder name for a real, simple constant, not a linkage bug. Given a
 * real definition here rather than renamed, to avoid touching every comment/
 * doc that already references this exact symbol name. */
double _DAT_0000af38 = 1.0 / 100.0;	/* progress-bar percent-to-pixel scale */

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
/* extern "C": video.cpp defines this inside its own extern "C" block; this
 * declaration lacking a matching extern "C" gave it a different (mangled)
 * expected name than the real (unmangled) definition, so it genuinely never
 * resolved ("Unknown symbol _Z29OmapNKS4VideoAPI_SendFillData...", confirmed
 * on real hardware, 2026-07-16) despite being fully implemented in video.cpp. */
extern "C" int OmapNKS4VideoAPI_SendFillData(struct COmapNKS4VideoAPI *self, unsigned char color,
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
	/* The file-wide `#define sInstance COmapNKS4Driver_sInstance` below textually
	 * collides with CSTGOmapNKS4Fifos's OWN, unrelated static member also named
	 * sInstance - "CSTGOmapNKS4Fifos::sInstance" macro-expands to
	 * "CSTGOmapNKS4Fifos::COmapNKS4Driver_sInstance", which doesn't exist. This
	 * went undetected because the file had never actually been compiled before a
	 * verify/ test suite existed (see verify/README.md). Fixed the same way at
	 * the other affected call site in HandleOutputSysReq(). */
#pragma push_macro("sInstance")
#undef sInstance
	struct CSTGOmapNKS4Fifos &f = CSTGOmapNKS4Fifos::sInstance;
#pragma pop_macro("sInstance")

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
					/* Ground truth (fresh Ghidra decompile, 2026-07-15): this branch
					 * disassembles to `MOVZX EAX,DX; OR EAX,0x1710000; CALL
					 * SendNKS4EventToLinuxReader`, i.e. it hardcodes the opcode byte
					 * (0x01) together with idx (0x71) into one 0x0171____ immediate -
					 * NOT the generic `word` (dLo|dHi<<8|idx<<16, no opcode) computed
					 * above and reused by every other branch here. Previously this
					 * called SendNKS4EventToLinuxReader(word) unmodified, which is
					 * missing the opcode contribution: word here is 0x0071____, not
					 * 0x0171____. ReadPortConfiguration's expected reg echo is 0x0171,
					 * so the old code could never wake it - not a wire-protocol gap,
					 * a transcription bug in this reconstruction. */
					SendNKS4EventToLinuxReader(word | 0x1000000);
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
						/* Ground truth (fresh Ghidra decompile + disassembly cross-check,
						 * 2026-07-15): the word's low byte is (val>>2)&0xff and its
						 * byte-1 (bits 8-15) is (v<<6)&0xff - OPPOSITE of what this
						 * reconstruction previously had. Ghidra's decompile of this
						 * branch is `CONCAT11(local_24<<6, (char)(local_28>>2))`;
						 * CONCAT11(hi,lo) places its SECOND operand ((val>>2)&0xff,
						 * local_28==val) in the low byte, not the first. Verified
						 * against the test-mode branch below via independent
						 * disassembly trace (same swapped convention, cross-checked
						 * two ways), so this isn't a lone decompiler artifact. */
						unsigned int w = ((unsigned int)idx << 16) |
							(((unsigned int)(unsigned char)(val >> 2)) ) |
							(((unsigned int)(unsigned char)(v << 6)) << 8) |
							0x3000000;
						if (driver_filter()->FilterEvent(w))
							push_event(w);
					} else {
						/* Test mode: emit TWO events instead of one. Ground truth
						 * re-decompiled fresh this session (previously unimplemented -
						 * see OmapNKS4Module.ko 3.2.2 @0x136a5 for event 1, @0x13727 for
						 * event 2), cross-checked against both the Ghidra decompile
						 * (CONCAT11 expressions) and a manual disassembly trace of the
						 * SHL/SAR/OR sequence - both agree.
						 *
						 * Event 1 (opcode 0x61): the RAW (uncalibrated) 10-bit ADC
						 * reading, reassembled from dLo/dHi as
						 * raw10 = (dLo<<2) | (dHi>>6), then re-split with its low byte
						 * in word bits[8:15] and its top 2 bits in word bits[0:7] - a
						 * byte-swapped 16-bit packing, consistent with this driver's
						 * other pairwise-halfword-swap conventions (see
						 * KronosNKS4/docs/protocol.md's AtmelRead sub-format for the
						 * same style of swap elsewhere in this codebase).
						 */
						unsigned int raw10 = ((unsigned int)dLo << 2) | (dHi >> 6);
						unsigned int w1 = ((unsigned int)idx << 16) |
							((raw10 >> 8) & 0xff) |
							((raw10 & 0xff) << 8) |
							0x61000000;
						if (driver_filter()->FilterEvent(w1))
							push_event(w1);

						/* Event 2 (opcode 0x62): the CALIBRATED value (same val/v as
						 * the non-test path above), byte-swapped the same way: high
						 * byte of val in bits[0:7], low byte of v in bits[8:15]. */
						unsigned int w2 = ((unsigned int)idx << 16) |
							(((unsigned int)val >> 8) & 0xff) |
							(((unsigned int)(unsigned char)v) << 8) |
							0x62000000;
						if (driver_filter()->FilterEvent(w2))
							push_event(w2);
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
	/* see push_event()'s comment for why this needs the macro dance */
#pragma push_macro("sInstance")
#undef sInstance
	struct CSTGOmapNKS4Fifos &f = CSTGOmapNKS4Fifos::sInstance;
#pragma pop_macro("sInstance")
	unsigned int max = sInstance.dwMaxWritePacketInts;
	unsigned int buf[64];	/* original uses alloca(max*4 + slack) */

	for (;;) {
		/* type=0: this pumps the command output FIFO (f.outputFifo), not the
		 * video queue (type=1) - see submit.cpp's OmapNKS4WriteQueueNotFull. */
		int notFull = OmapNKS4WriteQueueNotFull(0);
		unsigned char pend = f.outputFifo.bPending;

		unsigned int avail = f.outputFifo.dwWriteIndex - f.outputFifo.dwReadIndex;
		if (!notFull || avail == 0) {
			f.outputFifo.bPending = 0;
			if (OmapNKS4WriteQueueNotFull(0) && pend) {
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

/* Ground truth: fresh Ghidra decompile of COmapNKS4Driver_Configure@0x14570,
 * 2026-07-16 (real hardware: Kronos 2/D2550, confirmed bHardwareVersion==1,
 * taking the "else" branch below - matches the already-observed real dmesg
 * "OMAP V01 R08, PSoc V00 R05" from the stock module). Wires together
 * command.cpp functions that already existed in this reconstruction
 * (CommunicationCheck, ReadPortConfiguration, GetVersion, the SetNumberOf
 * setters, ConfigureRotaryEncoders, SetRotaryEncoderSampleSpeed,
 * ConfigureScanning) -
 * this function itself, despite being called directly from OmapNKS4Init's
 * critical path, was previously entirely undefined (only forward-declared).
 * ConfigureScanning's last 3 bool args (leds/lcd/jack) in the real binary
 * reuse the just-fetched version/revision bytes directly as booleans rather
 * than real hardware-presence flags (uVar3/uVar4/uVar5 in the decompile) -
 * reproduced as bPsocRevision/bPanelLVersion/bPanelLRevision below to match
 * the real register reuse in the default branch specifically (the v2/v3
 * branches reuse different bytes - not independently verified, since they
 * don't apply to this hardware; flagged for revisit if ever tested on a
 * hwVer 2 or 3 unit). */
extern "C" int COmapNKS4Driver_Configure(void)
{
	using namespace COmapNKS4Command;
	if (!CommunicationCheck())
		return -1;
	if (!ReadPortConfiguration(&sInstance.fIs88Key, &sInstance.bHardwareVersion))
		return -1;
	printk("<6>OmapNKS4:Configure: line 109: is88Key: %d\n", sInstance.fIs88Key);
	printk("<6>OmapNKS4:Configure: line 110: HardwareVersion: %d\n", sInstance.bHardwareVersion);

	if (sInstance.bHardwareVersion == 2) {
		if (!GetVersion(0, &sInstance.bOmapVersion, &sInstance.bOmapRevision)) return -1;
		if (!GetVersion(1, &sInstance.bPanelLVersion, &sInstance.bPanelLRevision)) return -1;
		if (!GetVersion(2, &sInstance.bPanelRVersion, &sInstance.bPanelRRevision)) return -1;
		if (!GetVersion(3, &sInstance.bJackVersion, &sInstance.bJackRevision)) return -1;
		printk("<6>OmapNKS4:Configure: line 134: OMAP NKS4 versions: AM V%02u R%02u, PanelL V%02u R%02u, PanelR V%02u R%02u, Jack V%02u R%02u\n\n",
		       sInstance.bOmapVersion, sInstance.bOmapRevision,
		       sInstance.bPanelLVersion, sInstance.bPanelLRevision,
		       sInstance.bPanelRVersion, sInstance.bPanelRRevision,
		       sInstance.bJackVersion, sInstance.bJackRevision);
	} else if (sInstance.bHardwareVersion == 3) {
		if (!GetVersion(0, &sInstance.bOmapVersion, &sInstance.bOmapRevision)) return -1;
		if (!GetVersion(1, &sInstance.bPsocVersion, &sInstance.bPsocRevision)) return -1;
		printk("<6>OmapNKS4:Configure: line 155: OMAP NKS4 versions: AM V%02u R%02u, Panel V%02u R%02u\n\n",
		       sInstance.bOmapVersion, sInstance.bOmapRevision,
		       sInstance.bPsocVersion, sInstance.bPsocRevision);
	} else {
		if (!GetVersion(&sInstance.bOmapVersion, &sInstance.bOmapRevision,
				&sInstance.bPsocVersion, &sInstance.bPsocRevision))
			return -1;
		printk("<6>OmapNKS4:Configure: line 168: OMAP NKS4 versions: OMAP V%02u R%02u, PSoc V%02u R%02u\n\n",
		       sInstance.bOmapVersion, sInstance.bOmapRevision,
		       sInstance.bPsocVersion, sInstance.bPsocRevision);
	}

	if (!SetNumberOfAnalogInputs(0x3f)) {
		printk("<6>OmapNKS4:Configure: line 174: setting num of analog ports failed!\n");
		return -1;
	}
	if (!SetAllAnalogInputFilter(2, 8)) {
		printk("<6>OmapNKS4:Configure: line 182: setting all analog input filter failed!\n");
		return -1;
	}
	if (!SetNumberOfLEDs(0x80)) {
		printk("<6>OmapNKS4:Configure: line 189: setting num of LEDs failed!\n");
		return -1;
	}
	if (!ConfigureRotaryEncoders(1, true, true)) {
		printk("<6>OmapNKS4:Configure: line 196: setting num of LEDs failed!\n");
		return -1;
	}
	if (!SetRotaryEncoderSampleSpeed(100)) {
		printk("<6>OmapNKS4:Configure: line 203: setting rotary encoder's scan interval failed!\n");
		return -1;
	}
	if (!ConfigureScanning(true, true, true, false,
			       sInstance.bPsocRevision != 0, sInstance.bPanelLVersion != 0,
			       sInstance.bPanelLRevision != 0)) {
		printk("<6>OmapNKS4:Configure: line 219: setting scanning failed!\n");
		return -1;
	}
	COmapNKS4Driver_SetProgressBarPercent(0x0f);
	return 0;
}

void COmapNKS4Driver_SetProgressBarColor1(unsigned char c) { sInstance.SetProgressBarColor1(c); }
void COmapNKS4Driver_SetProgressBarColor2(unsigned char c) { sInstance.SetProgressBarColor2(c); }
void COmapNKS4Driver_SetProgressBarPercent(unsigned char p){ COmapNKS4Driver::SetProgressBarPercent(p); }
unsigned char COmapNKS4Driver_AddToProgressBar(unsigned char a){ return sInstance.AddToProgressBar(a); }
void COmapNKS4Driver_IncProgressBar(void)       { sInstance.IncProgressBar(); }
int  COmapNKS4Driver_ApplyAftertouchTable(short v){ return sInstance.ApplyAftertouchTable(v); }
void COmapNKS4Driver_NotifyTransmittedCommandComplete(void) { }
/* Ground truth (fresh Ghidra decompile, 2026-07-15, COmapNKS4Driver_ReceiveEventBuffer
 * @0x14a90): a real, separate exported symbol - not a typo for the member function
 * above. Was entirely missing from this reconstruction (usb.cpp's InterruptCallback
 * calls it, but it was never implemented anywhere), found while fixing a real Kbuild
 * build attempt. Decompiling it shows it's a compiler-generated specialized CLONE of
 * ReceiveEventBuffer's own logic with `this` constant-propagated to &sInstance
 * (confirmed instruction-for-instruction identical dispatch, including the exact
 * same op==1/idx==0x71 echo and aftertouch byte-packing fixed earlier this session -
 * independent re-confirmation, not just a repeat of the same decompile) - i.e. this
 * really is just `sInstance.ReceiveEventBuffer(cmd, numInts)`, matching the thin-
 * wrapper convention every other C-ABI export in this section already uses. */
void COmapNKS4Driver_ReceiveEventBuffer(NKS4Command *cmd, unsigned int numInts)
{
	sInstance.ReceiveEventBuffer(cmd, numInts);
}
void COmapNKS4Driver_HandleOutputSysReq(void)   { COmapNKS4Driver::HandleOutputSysReq(); }
/* Ground truth (fresh Ghidra decompile + disassembly, 2026-07-15,
 * COmapNKS4Driver_ShutDown@0x15600): this takes a real 16-bit parameter (Ghidra's
 * decompiler elided it, same class of regparm-arg-loss seen elsewhere this session)
 * that becomes the low 16 bits of a command word: word = 0x09000000 | (param &
 * 0xffff), sent synchronously via SubmitNKS4CommandWrite (confirmed the same target
 * address command.cpp's own query functions call). Previously declared/called with
 * no parameter at all, which doesn't compile (arity mismatch against
 * SubmitNKS4CommandWrite's real signature) - this went undetected because the file
 * had never been compiled before verify/ existed. What the parameter itself means
 * (delay value? reason code?) is NOT confirmed - named generically pending a real
 * caller-side trace. */
void COmapNKS4Driver_ShutDown(unsigned short param) { SubmitNKS4CommandWrite(0x09000000 | param); }

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
