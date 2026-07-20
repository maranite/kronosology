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
void SetupNKS4Calibration(void *table, void *msgCallback);		/* submit.c      */
void CleanupNKS4Calibration(void);					/* submit.c      */
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
	bool GetRawDipSwitches(unsigned char *sw1, unsigned char *sw2);
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
						/* Ground truth (fresh disassembly, OmapNKS4Module.ko 3.2.2,
						 * ReceiveEventBuffer@0x13360, 2026-07-18): the idx&0xf0==0x50/
						 * installer-support branch @0x13a9a-0x13b92 shows v==0 emits NO
						 * proc event at all - `CMP DX,0; JLE 0x13b8c` then, at 0x13b8c,
						 * `JZ 0x13ac3` skips straight past BOTH OmapNKS4ProcAddEvent call
						 * sites when v==0. Only (short)v>0 (-> event 0xd, @0x13ab0) and
						 * v<0 (-> event 0xe, @0x13b92) actually call it. This reconstruction
						 * previously called OmapNKS4ProcAddEvent(0) unconditionally for
						 * v==0 too - a spurious proc event the real driver never sends. */
						if ((short)v > 0)
							OmapNKS4ProcAddEvent(0xd);
						else if (v != 0)
							OmapNKS4ProcAddEvent(0xe);
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
				/* BUG FOUND AND FIXED (residual-list audit, 2026-07-18): the calibration
				 * input was previously (short)(dLo | (dHi << 8)) - a naive 16-bit
				 * little-endian combine. Ground truth, confirmed via fresh disassembly
				 * of BOTH real compiled instances of this logic - the exported member
				 * function COmapNKS4Driver::ReceiveEventBuffer@0x13360 (its op==3 case at
				 * @0x13618-0x1363c: `SHR CL,6; MOVZX EAX,dLo; SHL EAX,2; OR ECX,EAX;
				 * MOVSX EDX,CX; CALL ApplyNKS4Calibration` with EDI(=idx, loaded at
				 * @0x13397 `MOVZX EDI,[EBX+2]` and preserved unclobbered to the call site)
				 * as the chan arg) and its compiler-generated clone
				 * COmapNKS4Driver_ReceiveEventBuffer@0x14a90 (op==3 case at
				 * @0x14d08-0x14d2d: `MOV EDX,EDI; SHR DL,6; ... SHL EAX,2; OR ECX,EAX;
				 * MOVSX EAX,CX; CALL ApplyNKS4Calibration`, EDI holding dHi unclobbered
				 * from its own entry load @0x14aba) - is that the calibration function
				 * is fed the SAME 10-bit raw10 = (dLo<<2)|(dHi>>6) reassembly already
				 * used (and already correctly documented) below for the test-mode event
				 * 0x61 packing, not the naive dLo|dHi<<8 combine. Both independently
				 * compiled instances agree exactly, and the formula matches this file's
				 * own pre-existing raw10 definition - moved that computation up here so
				 * it's shared by both the calibration call and the test-mode packing
				 * below (was previously duplicated/computed twice, once wrongly). */
				unsigned int raw10 = ((unsigned int)dLo << 2) | (dHi >> 6);
				int v = ApplyNKS4Calibration(idx, (short)raw10);
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
						 * raw10 = (dLo<<2) | (dHi>>6) (now computed once, above, and
						 * shared with the ApplyNKS4Calibration() call - see that fix's
						 * own comment), then re-split with its low byte in word
						 * bits[8:15] and its top 2 bits in word bits[0:7] - a
						 * byte-swapped 16-bit packing, consistent with this driver's
						 * other pairwise-halfword-swap conventions (see
						 * KronosNKS4/docs/protocol.md's AtmelRead sub-format for the
						 * same style of swap elsewhere in this codebase).
						 */
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
			/* CORRECTION (re-verification pass, 2026-07-17): `rec[7]` must be
			 * sign-extended before widening to unsigned - ground truth casts
			 * it to `(char)` (signed) first, so any length byte >= 0x80
			 * sign-extends into a huge unsigned value (a real, intentional
			 * "this shouldn't happen" guard against the bounds check just
			 * below), not a small 0x80-0xff length as the unsigned cast
			 * previously produced. */
			unsigned int n = (unsigned int)(signed char)rec[7];
			if ((numInts - i) * 4 - 5 < n)
				return;
			/* CORRECTION (re-verification pass, 2026-07-17): the byte-swap
			 * below previously did a 16-bit-PAIR swap ([b0,b1,b2,b3] ->
			 * [b1,b0,b3,b2] - each half-word byte-reversed independently,
			 * halves left in place). Ground truth does a true 32-bit
			 * reversal ([b0,b1,b2,b3] -> [b3,b2,b1,b0]) - confirmed via the
			 * decompiled CONCAT22(byteswap16(v&0xffff), byteswap16(v>>16))
			 * expression, which composes to a full bswap32, not a
			 * per-half-word swap. Fixed below. */
			unsigned char *q = rec;
			for (unsigned int k = (n + 4) >> 2; k; k--) {
				q += 4;
				unsigned int v = *(unsigned int *)q;
				*(unsigned int *)q =
					((v & 0xff) << 24) | (((v >> 8) & 0xff) << 16) |
					(((v >> 16) & 0xff) << 8) | (v >> 24);
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
/*
 * RESIDUAL-LIST ITEM, RESOLVED (2026-07-18): this function has TWO real compiled
 * clones - the free-function/regparm3 form at @0x13060 (already the one this
 * reconstruction is transcribed from) and a second, `__thiscall` form at @0x13f50
 * that a prior pass flagged as "Ghidra failed to decompile." get_disassembly also
 * initially failed at this address this pass; a raw read_memory + manual byte-level
 * x86 decode of @0x13f50-0x140a9 (this=EAX/EBX, pct=DL) was used instead and traces
 * instruction-for-instruction to the SAME logic as below: same three early-return
 * guards (hwVer==2; hwVer==1 with the *(ushort*)this==0 null-panel check, reusing one
 * CL flag computed once at entry for both the initial branch-select AND the second
 * guard - confirmed deliberate, not dead code, since CL==(hwVer==1) is exactly the
 * value both checks need), the same x0=(x0+1)-w adjustment, the same
 * (int)((double)(pct*x0)*_DAT_0000af38) FILD/FMUL/FISTTP fill-percentage computation,
 * and the same 4 OmapNKS4VideoAPI_SendFillData() calls with the same
 * color/width/base/height argument sets in the same order (bg,empty; bg,empty+width;
 * colorB,filled; colorA,filled+width). One purely cosmetic difference: the "hwVer==1,
 * ushort==0" return path recomputes an unused alternate constant set
 * (EDX=0x99/ESI=0x149/EDI=0x1f5) before re-testing the same (still-zero) condition and
 * returning - dead computation, no behavioral difference. No bug found in either
 * clone; both match the C below exactly.
 */
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

/*
 * Ground truth (fresh Ghidra decompile + disassembly, 2026-07-17, @0x13d10): pushes
 * cmd into the host<-panel input FIFO, gated by the embedded CNKS4EventFilter
 * (driver_filter(), at this+0x1d) - identical logic to this file's own push_event()
 * helper (already used elsewhere, e.g. ReceiveEventBuffer), reused here rather than
 * re-duplicating its macro-collision workaround (see push_event's own comment on the
 * `sInstance`/CSTGOmapNKS4Fifos::sInstance name collision).
 */
void COmapNKS4Driver::SendNKS4EventToSTG(unsigned int cmd)
{
	if (driver_filter()->FilterEvent(cmd))
		push_event(cmd);
}

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
	/* CORRECTION-FLAGGED, not yet fixed (re-verification pass, 2026-07-17):
	 * ground truth allocates this buffer via `alloca(max*4 + slack)`, sized
	 * from the RUNTIME-configurable dwMaxWritePacketInts, and zero-inits it
	 * at entry. This fixed-size, non-zeroed stack buffer is a real fidelity
	 * gap - if dwMaxWritePacketInts is ever configured above 64 (not
	 * observed in current use, all known callers of
	 * COmapNKS4Driver_Initialize pass small values), this overflows rather
	 * than scaling safely. Left as a flagged risk rather than silently
	 * "fixed" with a guessed bound, since the real max-safe value depends on
	 * runtime configuration this reconstruction doesn't currently model. */
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

		/*
		 * RESOLVED (2026-07-18): fresh disassembly of both compiled clones of
		 * this function (@0x131d0 free-function clone, @0x13d60 __thiscall
		 * clone - OmapNKS4Module.ko 3.2.2, 89849-byte target) pins down the
		 * exact 0x0900-marker re-fetch shape, confirmed identically in both:
		 *
		 * RESIDUAL-LIST RE-CONFIRMATION (same date, later pass): get_disassembly
		 * initially failed on @0x13d60 (same tooling gap seen on
		 * SetProgressBarPercent's @0x13f50 clone); re-derived it via raw
		 * read_memory + manual x86 byte decode instead of trusting the summary
		 * above at face value. Confirmed byte-for-byte equivalent control flow to
		 * @0x131d0 with only register-allocation differences forced by ESI being
		 * pinned to `this` here (EDX/EDI swap roles for avail/n vs the
		 * free-function's EDI/ESI; `avail` gets spilled to [EBP-0x2c] and
		 * reloaded around the SetShutdownDelay() call since EDX isn't
		 * callee-saved, whereas the free clone keeps avail in callee-saved EDI
		 * throughout) - same buf[n]&0xffff SetShutdownDelay argument, same
		 * fShutdownRequested=true at this+0x21, same avail-- /
		 * retry-same-slot-if-avail>n shape, same final avail (not index) passed
		 * to SubmitNKS4CommandMultipleWriteNONBLOCKING(buf,avail). No new bug;
		 * this independently re-verifies (not just repeats) the summary below.
		 *
		 * On hitting a 0x0900 marker in slot `n`, ground truth does NOT break
		 * the batch immediately. It decrements `avail` (the SAME bound the
		 * outer loop tests against, not a private copy), calls
		 * SetShutdownDelay() with the marker word's own low 16 bits (see
		 * below - not a constant 0 as previously assumed here), flags
		 * fShutdownRequested, and then either:
		 *   - re-fetches the NEXT FIFO entry into that SAME slot `n` and
		 *     re-tests it against 0x0900 (loop back), if `n` is still less
		 *     than the now-shrunk `avail`; or
		 *   - abandons the rest of the batch immediately (goto past the
		 *     per-slot loop) once the shrinking `avail` catches up to `n`.
		 * Disassembly: 0x13257 (fetch+CMP word ptr[...],0x900) is the single
		 * shared entry point for both "first visit to slot n" and "retry
		 * slot n after a marker" - JNZ 0x13284 (non-marker: n++, compare
		 * n:avail, continue or finish) vs. fall-through (marker: `LEA EDI,
		 * [EDI-1]` shrinks avail, CALL SetShutdownDelay, `CMP ESI,EDI; JC
		 * 0x13257` retries same n iff n<avail).
		 *
		 * Also found while pinning this down: SetShutdownDelay()'s real
		 * argument is `buf[n] & 0xffff` (disassembly: `MOVZX EAX,byte
		 * [buf+n*4+1]; SHL EAX,8; MOVZX EDX,byte [buf+n*4]; ADD EAX,EDX` -
		 * i.e. the marker word's own low half-word, the same 32-bit FIFO
		 * entry whose high half-word was just tested against 0x900), not the
		 * constant 0 this reconstruction previously passed. Confirmed
		 * identically in both compiled clones.
		 *
		 * Since fShutdownRequested is set unconditionally on every marker
		 * hit and the `continue` below always fires whenever it's set, the
		 * final SubmitNKS4CommandMultipleWriteNONBLOCKING() below is only
		 * ever actually reached when no marker fired anywhere in this batch
		 * - i.e. `avail` was never shrunk - so passing `avail` there (as
		 * ground truth's own EDI-based call does) rather than the old
		 * `n ? n : avail` is behavior-preserving, just a literal match.
		 */
		unsigned int n;
		for (n = 0; n < avail; ) {
			for (;;) {
				if (f.outputFifo.dwReadIndex != f.outputFifo.dwWriteIndex)
					buf[n] = f.outputFifo.pRing[f.outputFifo.dwReadIndex++ & 0x3f];
				if ((short)(buf[n] >> 16) != 0x0900)	/* not a delayed-shutdown marker */
					break;
				avail--;
				SetShutdownDelay(buf[n] & 0xffff);
				sInstance.fShutdownRequested = true;
				if (avail <= n)
					goto batch_done;
			}
			n++;
		}
batch_done:
		if (sInstance.fShutdownRequested)
			continue;
		SubmitNKS4CommandMultipleWriteNONBLOCKING(buf, avail);
	}
}

/* Ground truth (fresh Ghidra decompile, 2026-07-18, @0x13330): a real,
 * separately-exported member function setting the exact same 7 fields, same
 * values, as the free-function COmapNKS4Driver_Initialize below - a second,
 * distinctly-named symbol carrying identical logic, not a different
 * initialization path. Zero internal callers (get_xrefs_to: none inside
 * OmapNKS4Module.ko itself) - exported for some other module to call, same
 * pattern as SendNKS4EventToSTG above. */
void COmapNKS4Driver::Initialize(unsigned int maxWritePacketInts)
{
	fTestMode = false;
	fInstallerSupportOn = false;
	dwMaxWritePacketInts = maxWritePacketInts;
	bProgressBarColorA = 1;
	bProgressBarColorB = 9;
	bProgressBarColorBg = 0xc0;
	bProgress = 0xf;
}

/* ========================================================================= *
 *  C-ABI exported wrappers (operate on the singleton)
 * ========================================================================= */
/*
 * RESIDUAL-LIST ITEM, RESOLVED (2026-07-18): every trivial one-line getter/setter in
 * this block individually confirmed against fresh disassembly of its own real
 * exported address (89849-byte target), not "structurally trusted" - all match the
 * C below byte-for-byte:
 *   COmapNKS4Driver_GetTestMode@0x14990        MOVZX EAX,[this+0x10]; RET
 *   COmapNKS4Driver_SetTestMode@0x149a0         TEST EAX,EAX; SETNZ [this+0x10]
 *   COmapNKS4Driver_GetOmapVersion@0x149b0      *v=[this+0x00]; *r=[this+0x01]
 *   COmapNKS4Driver_GetPSocVersion@0x149d0      *v=[this+0x02]; *r=[this+0x03]
 *   COmapNKS4Driver_GetJackVersion@0x149f0      *v=[this+0x08]; *r=[this+0x09]
 *   COmapNKS4Driver_GetPanelLVersion@0x14a10    *v=[this+0x04]; *r=[this+0x05]
 *   COmapNKS4Driver_GetPanelRVersion@0x14a30    *v=[this+0x06]; *r=[this+0x07]
 *   COmapNKS4Driver_Is88Key@0x14a50             MOVZX EAX,[this+0x0a]; RET
 *   COmapNKS4Driver_GetHardwareVersion@0x14a60  MOVZX EAX,[this+0x0b]; RET
 *   COmapNKS4Driver_SetInstallerSupportOn@0x14a70 (bonus, same pattern) TEST/SETNZ [this+0x11]
 *   COmapNKS4Driver_Initialize (free fn)@0x14540 - all 7 field writes + values
 *     (fTestMode/fInstallerSupportOn=false, dwMaxWritePacketInts=arg,
 *     colorA/B/Bg=1/9/0xc0, bProgress=0xf) confirmed byte-for-byte, offsets
 *     0x10/0x11/0x0c/0x18/0x19/0x1a/0x1b all cross-consistent with every other
 *     confirmed offset in this file.
 *   COmapNKS4Driver_Cleanup (free fn)@0x14980   bare RET - genuine no-op, confirmed.
 *   COmapNKS4Driver_StartScanning@0x15560       MOV byte[this+0x1d],1
 *   COmapNKS4Driver_SetSTGInDownload@0x15570    TEST/SETNZ [this+0x1e]
 *   COmapNKS4Driver_GetSPDIFClockError@0x15580  MOVZX EAX,[this+0x1c]; RET
 *   COmapNKS4Driver_EnableShutdownByDriver@0x15590 MOV byte[this+0x20],1 (unconditional)
 *   COmapNKS4Driver_SetNumberOfKeys@0x155a0     CMP EAX,0x58; [this+0x24]=n;
 *     SETZ [this+0x0a] (fIs88Key) - cross-validates the Is88Key getter's own offset.
 *   SendAtmelCommand (member)@0x13ec0           tail-calls
 *     SubmitOmapNKS4CmdBulkWrite(0xe0,data,len), this-pointer genuinely unused/discarded.
 *   IncProgressBar (C-ABI)@0x15510, SetProgressBarColor1@0x15480,
 *     SetProgressBarColor2@0x15490 - ground truth INLINES the corresponding member
 *     logic at these addresses rather than emitting a CALL to it (no CALL instruction
 *     present at all), but the inlined logic is instruction-for-instruction identical
 *     to what the member does - so calling the member from these three C wrappers, as
 *     this file already did, is behaviorally indistinguishable from the real inlined
 *     body. No change needed for these three.
 * Two real discrepancies found and fixed this pass (each documented in-line at its own
 * fix site below): COmapNKS4Driver_NotifyTransmittedCommandComplete@0x15400 is a bare
 * RET in ground truth (not a delegating call - harmless since the member is also a
 * no-op, but corrected for exactness), and COmapNKS4Driver_ApplyAftertouchTable@0x155b0
 * is NOT a forward to the member of the same name - it calls
 * ApplyNKS4Calibration(chan=7, v) first and table-adjusts the CALIBRATED result, a
 * genuinely different computation (found incidentally while checking SetNumberOfKeys,
 * not originally on the checklist).
 */
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
 * ConfigureScanning's last 3 bool args - CORRECTED 2026-07-18 via a fresh
 * decompile of COmapNKS4Driver_Configure@0x14570 (a second, independent pass
 * over the same function): the previous entry above already correctly
 * identified that the real binary reuses just-fetched version/revision bytes
 * here rather than real hardware-presence flags, but got WHICH bytes wrong.
 * Ground truth shows uVar3/uVar4/uVar5 (the 3 locals feeding this call) are
 * assigned separately in EACH branch of the if/hwVer==2/elif hwVer==3/else
 * chain, then merged at the shared call site below:
 *   - hwVer==2: uVar3=DAT_bb01(bOmapRevision), uVar4=DAT_bb04(bPanelLVersion),
 *               uVar5=DAT_bb05(bPanelLRevision)
 *   - hwVer==3: uVar3=DAT_bb01(bOmapRevision), uVar4=DAT_bb02(bPsocVersion),
 *               uVar5=DAT_bb03(bPsocRevision)
 *   - else:     uVar3=DAT_bb01(bOmapRevision), uVar4=DAT_bb02(bPsocVersion),
 *               uVar5=DAT_bb03(bPsocRevision)   [same fields as hwVer==3]
 * i.e. arg5 is ALWAYS bOmapRevision!=0 (offset 0x01) in every branch - never
 * bPsocRevision (0x03) as this file previously claimed - and arg6/arg7 are
 * bPanelLVersion/bPanelLRevision only for hwVer==2, otherwise bPsocVersion/
 * bPsocRevision. The real-hardware-confirmed default("else") branch was
 * therefore ALSO wrong before this fix (used bPsocRevision/bPanelLVersion/
 * bPanelLRevision - none of which match offsets 0x01/0x02/0x03 ground truth
 * actually uses there), not just the never-tested v2/v3 branches. */
extern "C" int COmapNKS4Driver_Configure(void)
{
	using namespace COmapNKS4Command;
	bool bScanArgB, bScanArgC;	/* ConfigureScanning's branch-specific last 2 args */

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
		bScanArgB = sInstance.bPanelLVersion != 0;
		bScanArgC = sInstance.bPanelLRevision != 0;
	} else if (sInstance.bHardwareVersion == 3) {
		if (!GetVersion(0, &sInstance.bOmapVersion, &sInstance.bOmapRevision)) return -1;
		if (!GetVersion(1, &sInstance.bPsocVersion, &sInstance.bPsocRevision)) return -1;
		printk("<6>OmapNKS4:Configure: line 155: OMAP NKS4 versions: AM V%02u R%02u, Panel V%02u R%02u\n\n",
		       sInstance.bOmapVersion, sInstance.bOmapRevision,
		       sInstance.bPsocVersion, sInstance.bPsocRevision);
		bScanArgB = sInstance.bPsocVersion != 0;
		bScanArgC = sInstance.bPsocRevision != 0;
	} else {
		if (!GetVersion(&sInstance.bOmapVersion, &sInstance.bOmapRevision,
				&sInstance.bPsocVersion, &sInstance.bPsocRevision))
			return -1;
		printk("<6>OmapNKS4:Configure: line 168: OMAP NKS4 versions: OMAP V%02u R%02u, PSoc V%02u R%02u\n\n",
		       sInstance.bOmapVersion, sInstance.bOmapRevision,
		       sInstance.bPsocVersion, sInstance.bPsocRevision);
		bScanArgB = sInstance.bPsocVersion != 0;
		bScanArgC = sInstance.bPsocRevision != 0;
	}

	/* TEMPORARY DIAGNOSTIC (2026-07-19 rate-study continuation session) - not
	 * ground-truthed, remove once the hang below is root-caused. Deliberately
	 * UNGATED (no sVmVirtualProbe check) - this file is also compiled
	 * standalone into the host-side `make verify` suite (verify/host_stubs.cpp),
	 * which does not link main.cpp/sVmVirtualProbe's own definition; gating on
	 * it here broke that host build with an undefined-reference link error
	 * (caught before this pass finished, not left broken). Matches this
	 * codebase's existing convention for other TEMPORARY DIAGNOSTIC printks
	 * (main.cpp's PMR#/SSD# lines) already being unconditional for the same
	 * reason. Real-hardware impact is a handful of extra harmless printk lines
	 * during Configure(), same as those.
	 *
	 * Rationale: a 20-run hang-rate study (tools/run_vm_virtual_probe_test.sh,
	 * see README.md "Hang-rate study") found 9/20 (45%) of clean runs stall,
	 * and EVERY stall's furthest TRACKED milestone is "Configure(): NKS4
	 * versions reported" (the printk two lines above the first call below).
	 * A live QEMU-monitor capture of one such hang (run_20260719_150641)
	 * showed both kOmapNKS4MsgRoutine's AND kShutdownSSDRoutine's own wait
	 * loops ticking normally on schedule throughout the ENTIRE stall -
	 * meaning both worker threads were already alive, which (since
	 * create_thread() for both is called from OmapNKS4Init only AFTER this
	 * function returns) means Configure() itself had ALREADY returned
	 * successfully in that specific hang - the real stuck point in that run
	 * was later (see the main.cpp instrumentation added the same pass, right
	 * after the create_thread()/CActiveSenseThread_Setup() calls). These
	 * per-call markers are added here anyway as a secondary net, in case a
	 * DIFFERENT run's hang (this bug's stall point has not been proven to be
	 * single-cause) turns out to be inside this function's own tail instead. */
	printk("<6>OmapNKS4: DIAG Configure tail: calling SetNumberOfAnalogInputs\n");
	if (!SetNumberOfAnalogInputs(0x3f)) {
		printk("<6>OmapNKS4:Configure: line 174: setting num of analog ports failed!\n");
		return -1;
	}
	printk("<6>OmapNKS4: DIAG Configure tail: calling SetAllAnalogInputFilter\n");
	if (!SetAllAnalogInputFilter(2, 8)) {
		printk("<6>OmapNKS4:Configure: line 182: setting all analog input filter failed!\n");
		return -1;
	}
	printk("<6>OmapNKS4: DIAG Configure tail: calling SetNumberOfLEDs\n");
	if (!SetNumberOfLEDs(0x80)) {
		printk("<6>OmapNKS4:Configure: line 189: setting num of LEDs failed!\n");
		return -1;
	}
	printk("<6>OmapNKS4: DIAG Configure tail: calling ConfigureRotaryEncoders\n");
	if (!ConfigureRotaryEncoders(1, true, true)) {
		printk("<6>OmapNKS4:Configure: line 196: setting num of LEDs failed!\n");
		return -1;
	}
	printk("<6>OmapNKS4: DIAG Configure tail: calling SetRotaryEncoderSampleSpeed\n");
	if (!SetRotaryEncoderSampleSpeed(100)) {
		printk("<6>OmapNKS4:Configure: line 203: setting rotary encoder's scan interval failed!\n");
		return -1;
	}
	printk("<6>OmapNKS4: DIAG Configure tail: calling ConfigureScanning\n");
	if (!ConfigureScanning(true, true, true, false,
			       sInstance.bOmapRevision != 0, bScanArgB, bScanArgC)) {
		printk("<6>OmapNKS4:Configure: line 219: setting scanning failed!\n");
		return -1;
	}
	printk("<6>OmapNKS4: DIAG Configure tail: ConfigureScanning done, calling SetProgressBarPercent\n");
	COmapNKS4Driver_SetProgressBarPercent(0x0f);
	printk("<6>OmapNKS4: DIAG Configure tail: SetProgressBarPercent done, Configure() returning 0\n");
	return 0;
}

void COmapNKS4Driver_SetProgressBarColor1(unsigned char c) { sInstance.SetProgressBarColor1(c); }
void COmapNKS4Driver_SetProgressBarColor2(unsigned char c) { sInstance.SetProgressBarColor2(c); }
void COmapNKS4Driver_SetProgressBarPercent(unsigned char p){ COmapNKS4Driver::SetProgressBarPercent(p); }
unsigned char COmapNKS4Driver_AddToProgressBar(unsigned char a){ return sInstance.AddToProgressBar(a); }
void COmapNKS4Driver_IncProgressBar(void)       { sInstance.IncProgressBar(); }

/*
 * Ground truth (fresh Ghidra decompile, 2026-07-17): a SECOND, separate free-function
 * C-ABI family with no "Driver" in the name - `OmapVideoModule.ko`'s own `omapfb_ioctl`
 * (OMAPFB_IOCTL_INCPROGRESSBAR/ADDTOPROGRESSBAR/GETPROGRESSBARPERCENT/GETTITLESCREENVERSION)
 * calls these exact symbols, not the `COmapNKS4Driver_*` family above. Each is a
 * one-line forward to the same `COmapNKS4Driver` singleton method - genuinely
 * duplicate compiled wrappers around the same logic, not a different implementation
 * (same pattern already established for `COmapNKS4Driver_ReceiveEventBuffer` being a
 * compiler-cloned duplicate of `ReceiveEventBuffer` above).
 */
void COmapNKS4_SetProgressBarColor1(unsigned char c) { sInstance.SetProgressBarColor1(c); }
void COmapNKS4_SetProgressBarColor2(unsigned char c) { sInstance.SetProgressBarColor2(c); }
void COmapNKS4_SetProgressBarPercent(unsigned char p) { COmapNKS4Driver::SetProgressBarPercent(p); }
void COmapNKS4_IncProgressBar(void)                  { sInstance.IncProgressBar(); }
void COmapNKS4_AddToProgressBar(unsigned char a)     { sInstance.AddToProgressBar(a); }
/* Ground truth: reads the same raw backing byte `COmapNKS4Driver_GetProgressBarPercent`
 * exposes as `sInstance.bProgress` - Ghidra shows this call site as a direct global
 * read (`DAT_0001bb1b`) rather than routing through the class method, but it's the
 * identical physical storage (same address as `bProgress`), not a second counter. */
unsigned char COmapNKS4_GetProgressBarPercent(void)  { return sInstance.bProgress; }
/* Ground truth: literally just tail-calls COmapNKS4Driver_GetHardwareVersion() - the
 * "title screen version" shown at boot is the panel hardware version, no separate
 * concept of its own. */
int  COmapNKS4_GetTitleScreenVersion(void)           { return COmapNKS4Driver_GetHardwareVersion(); }
/* BUG FOUND AND FIXED (residual-list audit, 2026-07-18 - found incidentally while
 * verifying the adjacent COmapNKS4Driver_SetNumberOfKeys@0x155a0, not on the original
 * checklist, but confirmed via the same fresh-disassembly pass): this is NOT a thin
 * forward to COmapNKS4Driver::ApplyAftertouchTable, despite the name. Ground truth
 * (COmapNKS4Driver_ApplyAftertouchTable@0x155b0) is a genuinely different, larger
 * body: `MOV EAX,0x7; MOVSX EDX,AX; CALL ApplyNKS4Calibration` - it first CALIBRATES
 * v through ApplyNKS4Calibration with chan HARDCODED to 7 (the same "channel 7"
 * aftertouch-calibration-table chan used by ReceiveEventBuffer's own idx==7 special
 * case), THEN applies the sAfterTouch*ConvertTable lookup to the CALIBRATED result
 * (`CMP AX,0x3ff; JA <return-as-is>; SHR EAX,2; test fIs88Key; table[e]<<2`) - not to
 * the raw input `v` at all. The member function COmapNKS4Driver::ApplyAftertouchTable
 * (@0x14500, used elsewhere in this file) really is the plain table-only lookup with
 * no calibration call, confirmed correct via the same pass - so this free function
 * and the member function are two genuinely different pieces of logic that happen to
 * share a name suffix, not a wrapper/wrappee pair. Previously this file called the
 * member (wrong). */
int  COmapNKS4Driver_ApplyAftertouchTable(short v)
{
	int result = ApplyNKS4Calibration(7, v);
	if ((unsigned short)result > 0x3ff)
		return result;
	int e = (unsigned short)result >> 2;
	return (sInstance.fIs88Key ? sAfterTouch1ConvertTable[e]
				    : sAfterTouch2ConvertTable[e]) << 2;
}
/* Ground truth (fresh disassembly, residual-list audit 2026-07-18,
 * COmapNKS4Driver_NotifyTransmittedCommandComplete@0x15400): the real compiled body
 * is a single bare `RET` - it does NOT call the (also-empty)
 * COmapNKS4Driver::NotifyTransmittedCommandComplete member at all, unlike every other
 * C-ABI wrapper in this section which genuinely does forward to its member. Behaviorally
 * indistinguishable either way since the member itself is a no-op (confirmed elsewhere
 * in this file), but corrected to a literal no-op to match the real function body
 * exactly rather than implying a delegation that doesn't exist in the binary. */
void COmapNKS4Driver_NotifyTransmittedCommandComplete(NKS4Command *, unsigned int) { }
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
 * wrapper convention every other C-ABI export in this section already uses.
 *
 * RESIDUAL-LIST RE-CONFIRMATION (2026-07-18): the 2026-07-15 characterization above
 * was decompile-only per this project's own tracking; this pass re-derived it from
 * FRESH DISASSEMBLY of @0x14a90 (entry through its op dispatch chain: numInts==0 and
 * op==0x87 early returns, the op==8/op<9/op>=9 three-way split, the op==1/idx&0xf0==0x50
 * and idx==0x71 branches, the op==7 SPDIF-error store, and the op==0x1f rotary packing -
 * all confirmed identical to the member body's own already-fixed logic) rather than
 * trusting the prior decompile summary. This re-check is what surfaced a genuine,
 * previously unknown bug shared by BOTH this clone and the member body: see the
 * ApplyNKS4Calibration() raw10 argument fix in the op==3 branch above, found by
 * comparing this clone's @0x14d08 aftertouch-calibration call site against the member's
 * own @0x13618 site and discovering both disagreed with what this file had - the
 * clone's agreement with the member is what made the discrepancy against the OLD C
 * reconstruction obvious. */
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
