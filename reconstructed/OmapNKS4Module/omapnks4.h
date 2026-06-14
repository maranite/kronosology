// SPDX-License-Identifier: GPL-2.0
/*
 * omapnks4.h  -  shared types for OmapNKS4Module.ko
 *
 * Reconstructed (Ghidra RE) from the shipping OmapNKS4Module.ko, Korg Kronos firmware
 * 3.2.2.  Target: Linux 2.6.32.11 + RTAI, x86-32, g++ -mregparm=3 -fno-exceptions
 * -fno-rtti.
 *
 * The module is the USB driver + real-time service task for the Kronos front panel
 * (an OMAP-based "NKS4" board: keybed, controllers, LEDs, colour LCD, S/PDIF, Atmel
 * NV2AC security chip).  It speaks a packet protocol over USB bulk/interrupt pipes,
 * exposes /proc entries, and runs an RTAI real-time thread for active-sense + output.
 *
 * Struct field layouts (offsets, sizes) are recovered exactly from the binary.
 */

#ifndef OMAPNKS4_H
#define OMAPNKS4_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/usb.h>
#endif

/* ------------------------------------------------------------------------- *
 *  NKS4 wire protocol
 *
 *  The panel protocol is a stream of 32-bit "commands".  Each command is four
 *  bytes:  [0]=dataLo  [1]=dataHi  [2]=index/param  [3]=opcode .
 *  Buffers of commands are byte-addressed, so NKS4Command is treated as a byte.
 * ------------------------------------------------------------------------- */
typedef unsigned char NKS4Command;	/* one protocol byte; commands are 4 bytes */

/* opcode (4th byte) values seen in ReceiveEventBuffer() */
enum nks4_opcode {
	NKS4_OP_ANALOG       = 0x00,	/* analog/controller value             */
	NKS4_OP_BUTTON       = 0x01,	/* button / key matrix                 */
	NKS4_OP_AFTERTOUCH   = 0x03,	/* poly/channel aftertouch (calibrated)*/
	NKS4_OP_SPDIF_STATUS = 0x07,	/* S/PDIF clock-error status           */
	NKS4_OP_ACK          = 0x08,	/* comm-test ACK                       */
	NKS4_OP_ROTARY       = 0x1f,	/* rotary encoder                      */
	NKS4_OP_ATMEL_READ   = 0xe1,	/* Atmel NV2AC security-chip read data */
	NKS4_OP_SYNC         = 0x87,	/* end-of-buffer / sync marker         */
};

/* ------------------------------------------------------------------------- *
 *  COmapNKS4Driver  -  top-level panel driver state (singleton sInstance).
 *  40 bytes.  Versions are filled by Configure() via GetVersion().
 * ------------------------------------------------------------------------- */
struct COmapNKS4Driver {
	unsigned char bOmapVersion;		/* 0x00 */
	unsigned char bOmapRevision;		/* 0x01 */
	unsigned char bPsocVersion;		/* 0x02 */
	unsigned char bPsocRevision;		/* 0x03 */
	unsigned char bPanelLVersion;		/* 0x04 */
	unsigned char bPanelLRevision;		/* 0x05 */
	unsigned char bPanelRVersion;		/* 0x06 */
	unsigned char bPanelRRevision;		/* 0x07 */
	unsigned char bJackVersion;		/* 0x08 */
	unsigned char bJackRevision;		/* 0x09 */
	bool          fIs88Key;			/* 0x0a 88-key vs 61/73         */
	unsigned char bHardwareVersion;		/* 0x0b 1/2/3 -> panel topology */
	unsigned int  dwMaxWritePacketInts;	/* 0x0c max bulk-out ints/packet*/
	bool          fTestMode;		/* 0x10 */
	bool          fInstallerSupportOn;	/* 0x11 emit raw events to /proc*/
	unsigned char bField_0x12;		/* 0x12 */
	unsigned char bField_0x13;		/* 0x13 */
	void         *pAtmelReadBuffer;		/* 0x14 dest for Atmel chip read*/
	unsigned char bProgressBarColorA;	/* 0x18 */
	unsigned char bProgressBarColorB;	/* 0x19 */
	unsigned char bProgressBarColorBg;	/* 0x1a */
	unsigned char bProgress;		/* 0x1b 0..100 percent          */
	bool          fSpdifClockError;		/* 0x1c */
	unsigned char bField_0x1d;		/* 0x1d */
	bool          fStgInDownload;		/* 0x1e firmware download active*/
	unsigned char bField_0x1f;		/* 0x1f */
	bool          fShutdownByDriverEnabled;	/* 0x20 */
	bool          fShutdownRequested;	/* 0x21 */
	unsigned char bField_0x22;		/* 0x22 */
	unsigned char bField_0x23;		/* 0x23 */
	unsigned int  dwNumberOfKeys;		/* 0x24 (0x58==88 -> fIs88Key)  */

	/* the bytes at 0x1d..0x20 double as an embedded CNKS4EventFilter */
	void ReceiveEventBuffer(NKS4Command *cmd, unsigned int numInts);
	static void SetProgressBarPercent(unsigned char pct);
	void SetProgressBarColor1(unsigned char c);
	void SetProgressBarColor2(unsigned char c);
	unsigned char AddToProgressBar(unsigned char amount);
	unsigned char IncProgressBar(void);
	int  ApplyAftertouchTable(short value);
	void SendAtmelCommand(const unsigned char *data, int len);
	int  ReadAtmelData(const unsigned char *cmd4, unsigned char *dest);
	void NotifyTransmittedCommandComplete(NKS4Command *cmd, unsigned int len);
	void Cleanup(void);
	static void HandleOutputSysReq(void);
};

/* ------------------------------------------------------------------------- *
 *  COmapNKS4VideoAPI  -  colour-LCD command pipeline (singleton sInstance).
 *
 *  Embeds a 384-entry ring of 33-byte draw "events"; a real-time worker pops
 *  them and emits USB video-bulk packets.  12740 bytes.
 * ------------------------------------------------------------------------- */
struct OmapNKS4VideoAPIEvent {		/* 33 bytes, opcode + 8 unaligned args */
	unsigned char bOpcode;		/* 0x00  0xc0/0x81/0xc2/0xc4/0xc5 */
	unsigned int  dwArg0;		/* 0x01 (packed, 1-byte aligned)  */
	unsigned int  dwArg1;		/* 0x05 */
	unsigned int  dwArg2;		/* 0x09 */
	unsigned int  dwArg3;		/* 0x0d */
	unsigned int  dwArg4;		/* 0x11 */
	unsigned int  dwArg5;		/* 0x15 */
	unsigned int  dwArg6;		/* 0x19 */
	unsigned int  dwArg7;		/* 0x1d */
} __attribute__((packed));

struct COmapNKS4VideoAPI {
	struct OmapNKS4VideoAPIEvent pEvents[384];	/* 0x0000 ring buffer   */
	unsigned int  dwMaxEventIndex;			/* 0x3180 == 0x17f      */
	unsigned int  dwWriteIndex;			/* 0x3184               */
	unsigned int  dwReadIndex;			/* 0x3188               */
	struct OmapNKS4VideoAPIEvent currentEvent;	/* 0x318c popped event  */
	unsigned int  dwProcessingActive;		/* 0x31b0               */
	unsigned int  dwScreenBase;			/* 0x31b4 frame-buf base*/
	unsigned int  dwScreenWidth;			/* 0x31b8               */
	unsigned int  dwScreenHeight;			/* 0x31bc               */
	unsigned int  dwTransferRowSize;		/* 0x31c0 == 0x200      */

	COmapNKS4VideoAPI(void);
	unsigned int ProcessEvents(void);	/* worker: pop + emit one event */
	void ContinueProcessingEvent(struct OmapNKS4VideoAPIEvent *event);
	struct OmapNKS4VideoAPIEvent *GetNextFreeFifoEvent(void);
	unsigned int AddFifoEvent(struct OmapNKS4VideoAPIEvent *event);
	unsigned int GetNextEventToProcess(struct OmapNKS4VideoAPIEvent *event);
	/* draw builders -> enqueue an event for the worker (0 ok, -3 ring full) */
	int InitLCDRegs(char reg, char val, int data);		/* op 0xc0 */
	int XAxisByteSize(int bytes);				/* op 0x81 */
	int SendPixelDataRegion(int width, int offset, int rowBytes);/* op 0xc2 */
	int SendFillData(unsigned char color, int width, int base, int height); /* op 0xc4 */
	int UpdateColorPal(char a, char b, char c, char d);	/* op 0xc5 */
	int UpdateScreenInfo(char *base, int x, int y);
} __attribute__((packed));

/* ------------------------------------------------------------------------- *
 *  CSTGOmapNKS4Fifos  -  command/event FIFOs shared with the kernel reader
 *  via the RTAI SRQ mechanism (singleton sInstance, 1304 bytes).
 * ------------------------------------------------------------------------- */
struct CSTGOmapNKS4InputFifo {		/* host<-panel event fifo, 1032 bytes */
	unsigned int  pRing[256];	/* 0x000 */
	unsigned int  dwWriteIndex;	/* 0x400 */
	unsigned int  dwReadIndex;	/* 0x404 */
};

struct CSTGOmapNKS4OutputFifo {		/* host->panel command fifo, 265 bytes */
	unsigned int  pRing[64];	/* 0x000 */
	unsigned int  dwWriteIndex;	/* 0x100 */
	unsigned int  dwReadIndex;	/* 0x104 */
	unsigned char bPending;		/* 0x108 */

	int  WriteCommand(unsigned int cmd);
};

struct CSTGOmapNKS4Fifos {
	struct CSTGOmapNKS4InputFifo  inputFifo;	/* 0x000 */
	struct CSTGOmapNKS4OutputFifo outputFifo;	/* 0x408 */
	unsigned int                  dwEnabled;	/* 0x514 */

	void Initialize(int enable);
	void TriggerOutputInterrupt(void);

	static struct CSTGOmapNKS4Fifos sInstance;
};

/* ------------------------------------------------------------------------- *
 *  CActiveSenseThread  -  RTAI real-time thread that re-arms the panel output
 *  interrupt at a fixed tick (derived from the CPU TSC).  Heap object; the
 *  singleton CActiveSenseThread::sInstance is a *pointer* to it.  28 bytes.
 * ------------------------------------------------------------------------- */
struct CActiveSenseThread {
	void         *pVTable;			/* 0x00 (derives from CSTGThread) */
	unsigned char bActive;			/* 0x04 */
	unsigned long long qwNextTickCycles;	/* 0x08 next deadline (TSC)       */
	unsigned long long qwIntervalCycles;	/* 0x10 tick period (TSC)         */
	float         flNanosPerCycle;		/* 0x18 */

	CActiveSenseThread(void);
	~CActiveSenseThread(void);
	void ThreadRoutine(void);	/* RT loop: sleep-to-deadline, fire output IRQ */
	void Sleep(void);		/* block until the next tick deadline          */
	void Ping(void);		/* re-arm the deadline one interval out        */

	static bool Setup(void);	/* C-ABI: new + start the RT thread            */
	static void Cleanup(void);
	static void DoPing(void);	/* C-ABI CActiveSenseThread_Ping               */

	static CActiveSenseThread *sInstance;	/* heap object pointer */
};

/* ------------------------------------------------------------------------- *
 *  CNKS4EventFilter  -  sustain-pedal aware event suppression (4 bytes).
 * ------------------------------------------------------------------------- */
struct CNKS4EventFilter {
	unsigned char bEnabled;		/* 0x00 */
	unsigned char bSuppressAll;	/* 0x01 */
	unsigned char bSustainState;	/* 0x02 */
	unsigned char bPad;		/* 0x03 */

	/* returns nonzero if the event should be delivered to the host reader */
	unsigned char FilterEvent(unsigned int cmd);
};

#endif /* OMAPNKS4_H */
