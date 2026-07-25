// SPDX-License-Identifier: GPL-2.0
/*
 * test_midi_out_port_serial.cpp  -  host-side known-answer test for
 * src/engine/midi_out_port_serial.cpp: CSTGMidiOutPort's real base
 * methods (ctor/Activate/BumpTimers/ProcessNormal/
 * GenerateActiveSensing/ProcessNKS4TestMode/ProcessRealTimeMessage/
 * ReadNextMessage) + CSTGMidiOutPortSerial (Activate/
 * ProcessRegularMessage/RefillMsgBuffer/BumpTimers/running-status
 * compression) + CSTGMidiQueueReader::Read()/CSTGMidiQueueMessageReader::
 * ReadMessage()/ReadSysEx().
 *
 * Provides its own minimal stand-ins for CSTGMidiPortManager::
 * sMidiOutPorts/RegisterMidiOutPort() (matching test_midi_in_port_serial.cpp's
 * own "test owns its own singleton storage" convention) rather than
 * linking engine.cpp, and its own tiny real CSTGMidiQueue::AllocReader()
 * (the confirmed real `lock xadd $1,[this+0x20]` body) rather than
 * linking global.cpp.
 */

#include <cstdio>
#include <cstring>
#include <vector>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_heapmanager.h"

static void *mmap32(unsigned long size)
{
	void *p = mmap(0, size, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	memset(p, 0, size);
	return p;
}

static int g_fail;
static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%x)\n", want);
}
static void check_true(const char *label, bool got)
{
	if (!got)
		g_fail++;
	printf("  %s  %s\n", got ? "ok  " : "FAIL", label);
}

/* Required externs/singletons this TU owns. */
CSTGMidiPortManager *CSTGMidiPortManager::sInstance;
void *CSTGMidiPortManager::sMidiInPorts[4];
void *CSTGMidiPortManager::sMidiOutPorts[4];
CSTGHeapManager *CSTGHeapManager::sInstance;
CSTGAudioBusManager *CSTGAudioBusManager::sInstance;

void CSTGMidiPortManager::RegisterMidiOutPort(CSTGMidiOutPort *port)
{
	unsigned char *p = (unsigned char *)port;
	int index = (signed char)p[0x4];
	((void **)sMidiOutPorts)[index] = port;
}

/* CSTGMidiQueue::AllocReader() -- confirmed real `lock xadd $1,[this+0x20]`
 * (sec 10.82, global.cpp), re-derived locally rather than linking that
 * large TU (matches midi_in_port_serial.cpp's own precedent). */
unsigned char CSTGMidiQueue::AllocReader()
{
	unsigned char *self = (unsigned char *)this;
	unsigned char old = self[0x20];
	self[0x20] = (unsigned char)(old + 1);
	return old;
}

/* Real CSTGMidiQueue memory layout (see oa_engine_init.h's own class
 * comment): allocHandle/format/mask/writeCursor/readerPos[4]/readerCount. */
struct QueueObj {
	unsigned int allocHandle;   /* +0x0 */
	unsigned int format;        /* +0x4 */
	unsigned int mask;          /* +0x8 */
	unsigned int writeCursor;   /* +0xc */
	unsigned int readerPos[4];  /* +0x10..+0x1f */
	unsigned char readerCount;  /* +0x20 */
};

/*
 * CanTransmitHardware()/TransmitHardwareByte() -- deliberately declared
 * but left undefined in production code (oa_engine_init.h/
 * midi_out_port_serial.cpp): both are still `__cxa_pure_virtual` in the
 * real binary (no further-derived hardware-backend class exists
 * anywhere in OA.ko). Since these are ordinary (non-virtual) same-class
 * methods -- see oa_engine_init.h's own comment for why real C++
 * `virtual` isn't used here -- a KAT can't plug in behavior via
 * inheritance; this test simply provides the only definition of these
 * two symbols itself, matching the "test supplies its own minimal
 * stand-in for an unresolved symbol" convention already used elsewhere
 * in this project (e.g. StartSysEx()/ReceiveSysExData() in
 * test_midi_in_port_serial.cpp).
 */
static bool g_hwReady = true;
static std::vector<unsigned char> g_sent;

bool CSTGMidiOutPortSerial::CanTransmitHardware() const { return g_hwReady; }
void CSTGMidiOutPortSerial::TransmitHardwareByte(unsigned char byte) { g_sent.push_back(byte); }

typedef CSTGMidiOutPortSerial TestOutPort;

int main(void)
{
	printf("CSTGMidiOutPort/CSTGMidiOutPortSerial known-answer test\n");
	printf("=========================================================\n");

	/* CSTGAudioBusManager: busGainScale=1500.0f (confirmed real constant). */
	CSTGAudioBusManager::sInstance = (CSTGAudioBusManager *)mmap32(sizeof(CSTGAudioBusManager));
	CSTGAudioBusManager::sInstance->busGainScale = 1500.0f;

	/* CSTGHeapManager: a big-enough fake heap with heapBase=0 (so each
	 * handle's own "offset" field IS the final pointer directly). */
	unsigned char *heap = (unsigned char *)mmap32(0x1e849c + 4);
	CSTGHeapManager::sInstance = (CSTGHeapManager *)heap;
	*(unsigned int *)(heap + 0x1e8498) = 0; /* heapBase */

	unsigned char *buf0 = (unsigned char *)mmap32(64);
	unsigned char *buf1 = (unsigned char *)mmap32(64);
	unsigned char *buf2 = (unsigned char *)mmap32(64);
	unsigned char *buf3 = (unsigned char *)mmap32(64);
	/* handle N's own offset field lives at heap+0x24+N*0x14 */
	*(unsigned int *)(heap + 0x24 + 1 * 0x14) = (unsigned int)(unsigned long)buf0;
	*(unsigned int *)(heap + 0x24 + 2 * 0x14) = (unsigned int)(unsigned long)buf1;
	*(unsigned int *)(heap + 0x24 + 3 * 0x14) = (unsigned int)(unsigned long)buf2;
	*(unsigned int *)(heap + 0x24 + 4 * 0x14) = (unsigned int)(unsigned long)buf3;

	/* CSTGMidiPortManager: 3 embedded CSTGMidiQueue objects at
	 * +0xc/+0x70/+0xd4 (confirmed real, see CSTGMidiQueue::Initialize()'s
	 * own header comment). */
	unsigned char *mgr = (unsigned char *)mmap32(0x200);
	CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)mgr;
	QueueObj *q1obj = (QueueObj *)(mgr + 0x0c);
	QueueObj *q2obj = (QueueObj *)(mgr + 0x70);
	QueueObj *q0obj = (QueueObj *)(mgr + 0xd4);
	q0obj->allocHandle = 1; q0obj->mask = 0xf;
	q1obj->allocHandle = 2; q1obj->mask = 0xf;
	q2obj->allocHandle = 3; q2obj->mask = 0xf;

	/* q3: the caller-supplied per-port queue. Must be MAP_32BIT'd like
	 * everything else this test stores through a packed 32-bit field
	 * (CSTGMidiOutPort::q3Queue) -- a plain stack QueueObj's address is
	 * NOT guaranteed to fit in 32 bits on a 64-bit host and silently
	 * truncates to a wild pointer otherwise. */
	QueueObj *q3objPtr = (QueueObj *)mmap32(sizeof(QueueObj));
	QueueObj &q3obj = *q3objPtr;
	q3obj.allocHandle = 4; q3obj.mask = 0xf;

	TestOutPort port(2, 1); /* portType=2, flagsInit=1 (bit0 -> active-sensing enabled) */

	printf("[1] ctor\n");
	{
		check_eq("portIndex == portType", (unsigned char)port.portIndex, 2);
		check_eq("flags bit0 set from ctor param2&1", port.flags & 1, 1);
		check_eq("registered in sMidiOutPorts[2]", (unsigned long)CSTGMidiPortManager::sMidiOutPorts[2] == (unsigned long)&port, 1);
	}

	printf("[2] Activate(q3) -- wires up all 4 slots, resolves buffers via heap handles\n");
	{
		port.Activate((CSTGMidiQueue *)&q3obj);
		check_eq("q0Queue == manager+0xd4", (unsigned long)port.q0Queue == (unsigned long)q0obj, 1);
		check_eq("q0Buf resolved via handle 1", (unsigned long)port.q0Buf == (unsigned long)buf0, 1);
		check_eq("q1Buf resolved via handle 2", (unsigned long)port.q1Buf == (unsigned long)buf1, 1);
		check_eq("q2Buf resolved via handle 3", (unsigned long)port.q2Buf == (unsigned long)buf2, 1);
		check_eq("q3Queue == caller-supplied", (unsigned long)port.q3Queue == (unsigned long)&q3obj, 1);
		check_eq("q3Buf resolved via handle 4", (unsigned long)port.q3Buf == (unsigned long)buf3, 1);
		check_eq("flags bit1 (active) set", (port.flags >> 1) & 1, 1);
		check_eq("roundRobinIdx reset to 0", port.roundRobinIdx, 0);
		check_eq("sActiveSensingTransmitPeriodTicks == 375 (0.25*1500)",
			 CSTGMidiOutPort::sActiveSensingTransmitPeriodTicks, 375);
		check_eq("sRunningStatusTimeoutTicks == 75 (0.05*1500)",
			 CSTGMidiOutPortSerial::sRunningStatusTimeoutTicks, 75);
	}

	printf("[3] a plain 3-byte Note On through q3 -> ProcessNormal() drains it\n");
	{
		unsigned char msg[3] = { 0x91, 0x40, 0x7f };
		memcpy(buf3, msg, 3);
		q3obj.writeCursor = 3;
		g_sent.clear();

		/* One call per byte: pull+send status(0x91), then 2 more calls
		 * drain the remaining 2 data bytes one at a time. */
		check_true("ProcessNormal() call 1 sent a byte", port.ProcessNormal() != 0);
		check_true("ProcessNormal() call 2 sent a byte", port.ProcessNormal() != 0);
		check_true("ProcessNormal() call 3 sent a byte", port.ProcessNormal() != 0);
		check_eq("3 bytes transmitted", (unsigned int)g_sent.size(), 3);
		if (g_sent.size() == 3) {
			check_eq("byte0 == status", g_sent[0], 0x91);
			check_eq("byte1 == data1", g_sent[1], 0x40);
			check_eq("byte2 == data2", g_sent[2], 0x7f);
		}
		check_eq("lastStatus recorded", port.lastStatus, 0x91);
		check_eq("runningStatusTimer reloaded to 75", port.runningStatusTimer, 75);
	}

	printf("[4] running-status compression: same status repeats within the timeout\n");
	{
		unsigned char msg[3] = { 0x91, 0x44, 0x50 };
		memcpy(buf3 + (q3obj.writeCursor & 0xf), msg, 3);
		q3obj.writeCursor += 3;
		g_sent.clear();

		/* First call: status byte matches lastStatus and timer!=0, so
		 * the status byte is OMITTED -- state jumps straight to 2 and
		 * only msgBuf[1] is sent this call. */
		check_true("call 1 sent (running-status path)", port.ProcessNormal() != 0);
		check_eq("only 1 byte sent so far (status omitted)", (unsigned int)g_sent.size(), 1);
		if (!g_sent.empty())
			check_eq("first transmitted byte is data1 (0x44), NOT the status byte",
				 g_sent[0], 0x44);
		check_true("call 2 sends the final data byte", port.ProcessNormal() != 0);
		check_eq("2 bytes sent total (status genuinely omitted)", (unsigned int)g_sent.size(), 2);
		if (g_sent.size() == 2)
			check_eq("second byte is data2 (0x50)", g_sent[1], 0x50);
	}

	printf("[5] Active Sensing (0xFE) generation once activeSensingTimer expires\n");
	{
		g_sent.clear();
		port.activeSensingTimer = 0;
		port.GenerateActiveSensing();
		check_eq("exactly 1 byte sent", (unsigned int)g_sent.size(), 1);
		if (!g_sent.empty())
			check_eq("it's the literal Active Sensing byte 0xFE", g_sent[0], 0xfe);
		check_eq("timer reloaded to 375", port.activeSensingTimer, 375);
	}

	printf("[6] BumpTimers() (Serial override) decrements BOTH timers\n");
	{
		port.activeSensingTimer = 10;
		port.runningStatusTimer = 5;
		port.BumpTimers();
		check_eq("activeSensingTimer -1", port.activeSensingTimer, 9);
		check_eq("runningStatusTimer -1", port.runningStatusTimer, 4);
		port.runningStatusTimer = 0;
		port.BumpTimers();
		check_eq("runningStatusTimer stays 0 (not decremented below 0)", port.runningStatusTimer, 0);
	}

	printf("[7] CSTGMidiQueueReader::Read() wraps correctly at the ring boundary\n");
	{
		/* MAP_32BIT'd, not a stack QueueObj -- see the q3obj comment
		 * above for why a plain stack address can't be packed into the
		 * 32-bit "ringCtl pointer" field these reader objects expect. */
		QueueObj *ringPtr = (QueueObj *)mmap32(sizeof(QueueObj));
		QueueObj &ring = *ringPtr;
		ring.mask = 0xf; /* 16-byte ring */
		unsigned char *rbuf = (unsigned char *)mmap32(16);
		for (int i = 0; i < 16; i++)
			rbuf[i] = (unsigned char)(0x10 + i);

		unsigned char *readerObj = (unsigned char *)mmap32(9);
		*(unsigned int *)(readerObj + 0) = (unsigned int)(unsigned long)&ring;
		*(unsigned int *)(readerObj + 4) = (unsigned int)(unsigned long)rbuf;
		readerObj[8] = 0; /* readerIdx 0 */
		CSTGMidiQueueReader *reader = (CSTGMidiQueueReader *)readerObj;

		ring.readerPos[0] = 14; /* 2 bytes until wrap */
		unsigned char dest[5] = { 0 };
		reader->Read(dest, 5);
		check_eq("dest[0] from tail (0x1e)", dest[0], 0x1e);
		check_eq("dest[1] from tail (0x1f)", dest[1], 0x1f);
		check_eq("dest[2] wrapped to start (0x10)", dest[2], 0x10);
		check_eq("dest[3] wrapped (0x11)", dest[3], 0x11);
		check_eq("dest[4] wrapped (0x12)", dest[4], 0x12);
		check_eq("reader cursor advanced by 5", ring.readerPos[0], 19);
	}

	printf("[8] CSTGMidiQueueMessageReader::ReadMessage() resync past a stray data byte\n");
	{
		QueueObj *ringPtr = (QueueObj *)mmap32(sizeof(QueueObj));
		QueueObj &ring = *ringPtr;
		ring.mask = 0xf;
		unsigned char *rbuf = (unsigned char *)mmap32(16);
		/* stray data byte (0x33) then a clean 2-byte Program Change */
		rbuf[0] = 0x33; rbuf[1] = 0xc5; rbuf[2] = 0x10;
		ring.writeCursor = 3;

		unsigned char *msgReaderObj = (unsigned char *)mmap32(10);
		*(unsigned int *)(msgReaderObj + 0) = (unsigned int)(unsigned long)&ring;
		*(unsigned int *)(msgReaderObj + 4) = (unsigned int)(unsigned long)rbuf;
		CSTGMidiQueueMessageReader *mr = (CSTGMidiQueueMessageReader *)msgReaderObj;

		unsigned char out[3] = { 0, 0, 0 };
		unsigned int n = mr->ReadMessage(out, 3);
		check_eq("Program Change is 2 bytes", n, 2);
		check_eq("resynced past the stray byte -- out[0]==0xC5", out[0], 0xc5);
		check_eq("out[1]==0x10", out[1], 0x10);
		check_eq("reader cursor advanced past all 3 ring bytes", ring.readerPos[0], 3);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
