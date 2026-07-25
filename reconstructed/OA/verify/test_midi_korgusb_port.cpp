// SPDX-License-Identifier: GPL-2.0
/*
 * test_midi_korgusb_port.cpp  -  host-side known-answer test for
 * src/engine/midi_korgusb_port.cpp: CKorgUsbAudioDriverMidiPorts (ctor/
 * Connect/Disconnect/InputCallback/ProcessOutput), CSTGMidiInPortKorgUsb
 * (Activate/Deactivate), CSTGMidiOutPortKorgUsb (ctor/Activate/
 * Deactivate/CanSendRealTime/CanSendRegular/SendRealTime/SendSingleByte/
 * ProcessRegularMessage/RealtimeInput/Input/RealtimeOutput/Output), and
 * the STGMidiOutPortKorgUsb_* RTAI-SRQ pump (Initialize/Initialized/
 * Done/ScheduleFromRTAI/ScheduleFromLinux -- OutputThread's own body is
 * NOT exercised directly, it is an unbounded loop; its component real
 * kernel primitives are individually mocked/observed instead, matching
 * this project's established treatment of similar daemon-thread bodies,
 * e.g. test_daemon_lifecycle.cpp).
 *
 * Links src/engine/midi_korgusb_port.cpp, midi_out_port_serial.cpp (for
 * the real base CSTGMidiOutPort ctor/Activate/Deactivate/
 * ReadNextMessage this file's classes build on), midi_in_port_serial.cpp
 * (for the real base CSTGMidiInPort ctor), and midi_queue.cpp/
 * midi_queue_writer.cpp/midi_queue_writer_byte.cpp (transitive
 * CSTGMidiQueueWriter::Write() dependency of midi_in_port_serial.cpp's
 * OWN other methods, even though this test never calls them). Provides
 * its own minimal stand-ins for CSTGMidiPortManager::sMidiInPorts/
 * sMidiOutPorts/RegisterMidiInPort()/RegisterMidiOutPort(),
 * CSTGMidiQueue::AllocReader(), and CSTGMidiInPort::Activate()/
 * Deactivate() (deliberately-deferred real methods, see oa_engine.h),
 * matching test_midi_out_port_serial.cpp's/test_midi_in_port_serial.cpp's
 * own established conventions.
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
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	bool ok = got == want;
	if (!ok) g_fail++;
	printf("  %s  %-60s 0x%lx\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok) printf("        (wanted 0x%lx)\n", want);
}
static void check_true(const char *label, bool got)
{
	if (!got) g_fail++;
	printf("  %s  %s\n", got ? "ok  " : "FAIL", label);
}

/* ---- Required externs/singletons this TU owns (matching sibling
 * midi_*_serial tests' own established convention). ---------------- */
CSTGMidiPortManager *CSTGMidiPortManager::sInstance;
void *CSTGMidiPortManager::sMidiInPorts[4];
void *CSTGMidiPortManager::sMidiOutPorts[4];
CSTGHeapManager *CSTGHeapManager::sInstance;
CSTGAudioBusManager *CSTGAudioBusManager::sInstance;
CSTGGlobal *CSTGGlobal::sInstance;
CSTGMessageProcessor *CSTGMessageProcessor::sInstance;

void CSTGMidiPortManager::RegisterMidiOutPort(CSTGMidiOutPort *port)
{
	unsigned char *p = (unsigned char *)port;
	int index = (signed char)p[0x4];
	if (index >= 0 && index < 4)
		((void **)sMidiOutPorts)[index] = port;
}
void CSTGMidiPortManager::RegisterMidiInPort(CSTGMidiInPort *port)
{
	unsigned char *p = (unsigned char *)port;
	int index = (signed char)p[0x25];
	if (index >= 0 && index < 4)
		((void **)sMidiInPorts)[index] = port;
}

/* CSTGMidiQueue::AllocReader() -- confirmed real `lock xadd $1,[this+0x20]`,
 * re-derived locally rather than linking global.cpp (matches sibling
 * tests' own precedent). */
unsigned char CSTGMidiQueue::AllocReader()
{
	unsigned char *self = (unsigned char *)this;
	unsigned char old = self[0x20];
	self[0x20] = (unsigned char)(old + 1);
	return old;
}

/* CSTGMidiInPort::Activate()/Deactivate() -- deliberately deferred real
 * methods (oa_engine.h); test supplies call-counting stand-ins. */
static int g_inActivateCalls, g_inDeactivateCalls;
void CSTGMidiInPort::Activate(CSTGMidiQueue *) { g_inActivateCalls++; }
void CSTGMidiInPort::Deactivate() { g_inDeactivateCalls++; }
void CSTGMidiInPort::StartSysEx() { }
void CSTGMidiInPort::ReceiveSysExData(unsigned char) { }

/* CSTGMidiOutPortSerial::CanTransmitHardware()/TransmitHardwareByte() --
 * also deliberately-deferred (midi_out_port_serial.cpp), pulled in
 * because this test links that TU for the shared CSTGMidiOutPort base
 * methods; never exercised via CSTGMidiOutPortKorgUsb, no-op is fine. */
bool CSTGMidiOutPortSerial::CanTransmitHardware() const { return false; }
void CSTGMidiOutPortSerial::TransmitHardwareByte(unsigned char) { }

/* CSTGMidiInPortUSB::ReceivePacket() -- deliberately deferred (whole
 * separate generic-USB-MIDI-class hierarchy, oa_engine.h); this test's
 * InputCallback() thunk needs SOME real definition to link. */
void CSTGMidiInPortUSB::ReceivePacket(USBMidiPacket) { }

struct QueueObj {
	unsigned int allocHandle, format, mask, writeCursor, readerPos[4];
	unsigned char readerCount;
};

/* ---- Companion-module + RTAI/kernel extern mocks -------------------- */
extern "C" {

static int g_korgInitializedRet[2] = { 0, 0 };
static int g_korgInitializeCalls;
static int g_korgInitializeLastIdx = -1;
static void *g_korgInitializeLastUserdata;
int KorgUsbMidiInitialized(int idx) { return g_korgInitializedRet[idx]; }
int KorgUsbMidiInitialize(int idx, unsigned int a, unsigned int b, void *userdata)
{
	g_korgInitializeCalls++;
	g_korgInitializeLastIdx = idx;
	g_korgInitializeLastUserdata = userdata;
	(void)a; (void)b;
	g_korgInitializedRet[idx] = 1;
	return 0;
}
static int g_korgDoneCalls;
int KorgUsbMidiDone(int idx) { g_korgDoneCalls++; g_korgInitializedRet[idx] = 0; return 0; }

static int g_rtCanSend = 100;
static std::vector<unsigned char> g_rtSent;
static int g_rtOutputCalls;
static int g_rtLastPortId;
int KorgUsbRealtimeMidiOutputCanSend(int portId) { g_rtLastPortId = portId; return g_rtCanSend; }
void KorgUsbRealtimeMidiOutput(int portId, unsigned char *buf, unsigned int count)
{
	g_rtOutputCalls++;
	g_rtLastPortId = portId;
	g_rtSent.assign(buf, buf + count);
}

static int g_regCanSend = 100;
static std::vector<unsigned char> g_regSent;
static int g_regOutputCalls;
int KorgUsbMidiOutputCanSend(int portId) { (void)portId; return g_regCanSend; }
void KorgUsbMidiOutput(int portId, unsigned char *buf, unsigned int count)
{
	(void)portId;
	g_regOutputCalls++;
	g_regSent.assign(buf, buf + count);
}

static int g_srqCalls;
static void *g_lastSrqHandler;
static int g_srqReturnValue = 7;
int rt_request_srq(unsigned int label, void (*handler)(void), void *rt_handler)
{
	(void)label; (void)rt_handler;
	g_srqCalls++;
	g_lastSrqHandler = (void *)handler;
	return g_srqReturnValue;
}
static int g_freeSrqCalls;
static unsigned int g_lastFreedSrq;
void rt_free_srq(unsigned int srq) { g_freeSrqCalls++; g_lastFreedSrq = srq; }

static int g_kthreadCalls;
static long g_kthreadPid = 55;
long kernel_thread(int (*fn)(void *), void *arg, unsigned long flags)
{
	(void)fn; (void)arg; (void)flags;
	g_kthreadCalls++;
	return g_kthreadPid;
}

static int g_waitCompletionCalls;
void wait_for_completion(void *completion) { (void)completion; g_waitCompletionCalls++; }
static int g_waitCompletionTimeoutCalls;
long wait_for_completion_timeout(void *completion, unsigned long timeout)
{
	(void)completion; (void)timeout;
	g_waitCompletionTimeoutCalls++;
	return 1;
}

static int g_wakeUpCalls;
void __wake_up(void *q, unsigned int mode, int nr_exclusive, void *key)
{
	(void)q; (void)mode; (void)nr_exclusive; (void)key;
	g_wakeUpCalls++;
}

static int g_pendSrqCalls;
static unsigned int g_lastPendedSrq;
void rt_pend_linux_srq(unsigned int srq) { g_pendSrqCalls++; g_lastPendedSrq = srq; }

void daemonize(const char *name, ...) { (void)name; }
int stg_sched_setscheduler(void *task, int policy, void *param) { (void)task; (void)policy; (void)param; return 0; }
unsigned long stg_cpumask_of_cpu(unsigned int cpu) { return 1u << cpu; }
int stg_set_cpus_allowed(void *p, unsigned long mask) { (void)p; (void)mask; return 0; }
void prepare_to_wait(void *waitq, void *waitEntry, int state) { (void)waitq; (void)waitEntry; (void)state; }
long schedule_timeout(long jiffies) { (void)jiffies; return 0; }
void finish_wait(void *waitq, void *waitEntry) { (void)waitq; (void)waitEntry; }
int autoremove_wake_function(void *w, unsigned int m, int s, void *k) { (void)w; (void)m; (void)s; (void)k; return 0; }
void complete(void *completion) { (void)completion; }
void complete_and_exit(void *completion, long code) { (void)completion; (void)code; }
void *stg_get_current_task(void) { static int dummy; return &dummy; }

} /* extern "C" */

int main(void)
{
	printf("CKorgUsbAudioDriverMidiPorts / CSTGMidiOutPortKorgUsb known-answer test\n");
	printf("=========================================================================\n");

	/* CSTGAudioBusManager: busGainScale=1500.0f (confirmed real constant,
	 * needed by the base CSTGMidiOutPort::Activate()). */
	CSTGAudioBusManager::sInstance = (CSTGAudioBusManager *)mmap32(sizeof(CSTGAudioBusManager));
	CSTGAudioBusManager::sInstance->busGainScale = 1500.0f;

	/* CSTGHeapManager: heapBase=0 so each handle's own offset IS the
	 * final pointer directly. */
	unsigned char *heap = (unsigned char *)mmap32(0x1e849c + 4);
	CSTGHeapManager::sInstance = (CSTGHeapManager *)heap;
	*(unsigned int *)(heap + 0x1e8498) = 0;

	unsigned char *buf0 = (unsigned char *)mmap32(64);
	unsigned char *buf1 = (unsigned char *)mmap32(64);
	unsigned char *buf2 = (unsigned char *)mmap32(64);
	unsigned char *buf3a = (unsigned char *)mmap32(64);
	unsigned char *buf3b = (unsigned char *)mmap32(64);
	*(unsigned int *)(heap + 0x24 + 1 * 0x14) = (unsigned int)(unsigned long)buf0;
	*(unsigned int *)(heap + 0x24 + 2 * 0x14) = (unsigned int)(unsigned long)buf1;
	*(unsigned int *)(heap + 0x24 + 3 * 0x14) = (unsigned int)(unsigned long)buf2;
	*(unsigned int *)(heap + 0x24 + 4 * 0x14) = (unsigned int)(unsigned long)buf3a;
	*(unsigned int *)(heap + 0x24 + 5 * 0x14) = (unsigned int)(unsigned long)buf3b;

	unsigned char *mgr = (unsigned char *)mmap32(0x200);
	CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)mgr;
	QueueObj *q1obj = (QueueObj *)(mgr + 0x0c);
	QueueObj *q2obj = (QueueObj *)(mgr + 0x70);
	QueueObj *q0obj = (QueueObj *)(mgr + 0xd4);
	q0obj->allocHandle = 1; q0obj->mask = 0xf;
	q1obj->allocHandle = 2; q1obj->mask = 0xf;
	q2obj->allocHandle = 3; q2obj->mask = 0xf;

	QueueObj *q3objA = (QueueObj *)mmap32(sizeof(QueueObj));
	q3objA->allocHandle = 4; q3objA->mask = 0xf;
	QueueObj *q3objB = (QueueObj *)mmap32(sizeof(QueueObj));
	q3objB->allocHandle = 5; q3objB->mask = 0xf;

	printf("[1] CKorgUsbAudioDriverMidiPorts::sInstance -- static ctor sanity\n");
	{
		CSTGMidiInPortKorgUsb *in0 = CKorgUsbAudioDriverMidiPorts::sInstance.InPort(0);
		CSTGMidiInPortKorgUsb *in1 = CKorgUsbAudioDriverMidiPorts::sInstance.InPort(1);
		CSTGMidiOutPortKorgUsb *out0 = CKorgUsbAudioDriverMidiPorts::sInstance.OutPort(0);
		CSTGMidiOutPortKorgUsb *out1 = CKorgUsbAudioDriverMidiPorts::sInstance.OutPort(1);

		check_eq("in0's midiPortIndex byte == 0", ((unsigned char *)in0)[0x2e8], 0);
		check_eq("in1's midiPortIndex byte == 1", ((unsigned char *)in1)[0x2e8], 1);
		check_eq("out0.korgUsbPortIndex == 0", out0->korgUsbPortIndex, 0);
		check_eq("out1.korgUsbPortIndex == 1", out1->korgUsbPortIndex, 1);
		check_eq("out0.portIndex (eSTGMidiPort) == 0", (unsigned char)out0->portIndex, 0);
		check_eq("out1.portIndex (eSTGMidiPort) == 1", (unsigned char)out1->portIndex, 1);
		check_eq("out0 flags bit1 (active) NOT set before Activate()", (out0->flags >> 1) & 1, 0);
		check_eq("out0 ring indices start at 0", out0->realtimeWriteIdx + out0->realtimeReadIdx
		         + out0->regularWriteIdx + out0->regularReadIdx, 0);
		check_eq("out0 registered in sMidiOutPorts[0]", (unsigned long)CSTGMidiPortManager::sMidiOutPorts[0], (unsigned long)out0);
		check_eq("out1 registered in sMidiOutPorts[1]", (unsigned long)CSTGMidiPortManager::sMidiOutPorts[1], (unsigned long)out1);
	}

	CSTGMidiOutPortKorgUsb *out0 = CKorgUsbAudioDriverMidiPorts::sInstance.OutPort(0);

	printf("[2] CSTGMidiOutPortKorgUsb::Activate() -- base wiring + companion Connect()\n");
	{
		out0->Activate((CSTGMidiQueue *)q3objA);
		check_eq("q0Buf resolved via handle 1", (unsigned long)out0->q0Buf, (unsigned long)buf0);
		check_eq("q3Queue == caller-supplied", (unsigned long)out0->q3Queue, (unsigned long)q3objA);
		check_eq("flags bit1 (active) now set", (out0->flags >> 1) & 1, 1);
		check_eq("KorgUsbMidiInitialize() called once for port 0", g_korgInitializeCalls, 1);
		check_eq("KorgUsbMidiInitialize() idx == 0", g_korgInitializeLastIdx, 0);
		check_eq("KorgUsbMidiInitialized(0) now true", KorgUsbMidiInitialized(0), 1);
		check_eq("STGMidiOutPortKorgUsb_Initialize() ran kernel_thread() once", g_kthreadCalls, 1);
		check_eq("kernel_thread()'s completion arg was later waited on", g_waitCompletionCalls, 1);
		check_eq("rt_request_srq() called once", g_srqCalls, 1);
		check_eq("STGMidiOutPortKorgUsb_Initialized() == true", STGMidiOutPortKorgUsb_Initialized(), 1);
	}

	printf("[3] CanSendRealTime()/CanSendRegular() -- free-space > 3 gate\n");
	{
		check_true("CanSendRealTime() true on an empty ring", out0->CanSendRealTime());
		check_true("CanSendRegular() true on an empty ring", out0->CanSendRegular());
		out0->realtimeWriteIdx = 0x3fe; out0->realtimeReadIdx = 0; /* free = 0x400-0x3fe = 2 */
		check_true("CanSendRealTime() false when free space <= 3", !out0->CanSendRealTime());
		out0->realtimeWriteIdx = 0; out0->realtimeReadIdx = 0;
	}

	printf("[4] SendRealTime()/SendSingleByte() -- push + wrap + pend SRQ (srq now valid)\n");
	{
		out0->realtimeWriteIdx = 0x3fe;
		out0->SendRealTime(0xAA);
		check_eq("realtime byte written at old write idx", out0->realtimeRing[0x3fe], 0xAA);
		check_eq("realtimeWriteIdx advanced to 0x3ff", out0->realtimeWriteIdx, 0x3ff);
		out0->SendRealTime(0xBB);
		check_eq("realtimeWriteIdx wrapped to 0", out0->realtimeWriteIdx, 0);
		check_eq("byte written at wrapped slot 0x3ff", out0->realtimeRing[0x3ff], 0xBB);
		check_eq("rt_pend_linux_srq() called (srq valid since Activate)", g_pendSrqCalls > 0, 1);
		check_eq("rt_pend_linux_srq()'s own srq == rt_request_srq()'s return", g_lastPendedSrq, (unsigned int)g_srqReturnValue);

		out0->regularWriteIdx = 0x3fe;
		out0->SendSingleByte(0x90);
		out0->SendSingleByte(0x40);
		check_eq("regularWriteIdx wrapped to 0", out0->regularWriteIdx, 0);
		check_eq("2nd byte written at slot 0x3ff (pre-wrap)", out0->regularRing[0x3ff], 0x40);

		out0->realtimeWriteIdx = out0->realtimeReadIdx = 0;
		out0->regularWriteIdx = out0->regularReadIdx = 0;
	}

	printf("[5] RealtimeInput()/Input() -- bulk push with per-byte wrap\n");
	{
		out0->realtimeWriteIdx = 0x3fe;
		unsigned char data[4] = { 1, 2, 3, 4 };
		out0->RealtimeInput(data, 4);
		check_eq("realtimeWriteIdx after 4 bytes from 0x3fe", out0->realtimeWriteIdx, 2);
		check_eq("byte at 0x3fe", out0->realtimeRing[0x3fe], 1);
		check_eq("byte at 0x3ff", out0->realtimeRing[0x3ff], 2);
		check_eq("byte at wrapped 0", out0->realtimeRing[0], 3);
		check_eq("byte at wrapped 1", out0->realtimeRing[1], 4);
		out0->realtimeWriteIdx = out0->realtimeReadIdx = 0;

		out0->Input(data, 4);
		check_eq("regularWriteIdx after 4 bytes", out0->regularWriteIdx, 4);
		out0->regularWriteIdx = out0->regularReadIdx = 0;
	}

	printf("[6] ProcessRegularMessage() -- polls q1/q2/q3 via base ReadNextMessage()\n");
	{
		/* q3objA's own writer ring + a 3-byte Note On, matching
		 * test_midi_out_port_serial.cpp's own established pattern for
		 * feeding CSTGMidiQueueMessageReader::ReadMessage() through a
		 * real CSTGMidiQueue writer path is more machinery than this
		 * batch needs -- exercise ReadNextMessage()'s own q-slot
		 * round-robin directly instead, matching
		 * CSTGMidiQueueMessageReader's own test precedent (verify/
		 * test_midi_out_port_serial.cpp section [8]): reinterpret the
		 * q1 slot (out0+0x14) as a CSTGMidiQueueMessageReader by hand. */
		unsigned char *ringCtl = (unsigned char *)mmap32(16);
		*(unsigned int *)(ringCtl + 0) = 0;     /* readerPos[0] cursor */
		*(unsigned int *)(ringCtl + 8) = 0xf;   /* mask */
		*(unsigned int *)(ringCtl + 0xc) = 3;   /* writeCursor: 3 bytes available */
		unsigned char *rbuf = (unsigned char *)mmap32(16);
		rbuf[0] = 0x90; rbuf[1] = 0x40; rbuf[2] = 0x60; /* Note On ch0, key 0x40, vel 0x60 */

		unsigned char *q1slot = (unsigned char *)out0 + 0x14;
		*(unsigned int *)(q1slot + 0) = (unsigned int)(unsigned long)ringCtl;
		*(unsigned int *)(q1slot + 4) = (unsigned int)(unsigned long)rbuf;
		q1slot[8] = 0;      /* readerIdx 0 */
		q1slot[9] = 0;      /* inSysEx */
		out0->roundRobinIdx = 0;

		bool sent = out0->ProcessRegularMessage();
		check_true("ProcessRegularMessage() returns true (message found)", sent);
		check_eq("3 bytes landed in the regular ring", out0->regularWriteIdx, 3);
		check_eq("byte 0 == status", out0->regularRing[0], 0x90);
		check_eq("byte 1 == data1", out0->regularRing[1], 0x40);
		check_eq("byte 2 == data2", out0->regularRing[2], 0x60);

		out0->regularWriteIdx = out0->regularReadIdx = 0;

		bool sent2 = out0->ProcessRegularMessage();
		check_true("ProcessRegularMessage() returns false once queue is empty", !sent2);
	}

	printf("[7] RealtimeOutput()/Output() -- drain + clamp to companion's CanSend()\n");
	{
		out0->realtimeRing[0] = 0xF8; out0->realtimeRing[1] = 0xF8; out0->realtimeRing[2] = 0xF8;
		out0->realtimeWriteIdx = 3;
		out0->realtimeReadIdx = 0;
		g_rtCanSend = 100;
		out0->RealtimeOutput();
		check_eq("KorgUsbRealtimeMidiOutput() called once", g_rtOutputCalls, 1);
		check_eq("all 3 pending bytes drained", (unsigned long)g_rtSent.size(), 3ul);
		check_eq("realtimeReadIdx caught up to write", out0->realtimeReadIdx, out0->realtimeWriteIdx);
		check_eq("portId forwarded correctly", g_rtLastPortId, out0->korgUsbPortIndex);

		out0->regularRing[0] = 1; out0->regularRing[1] = 2; out0->regularRing[2] = 3; out0->regularRing[3] = 4;
		out0->regularWriteIdx = 4;
		out0->regularReadIdx = 0;
		g_regCanSend = 2; /* clamp: only 2 of the 4 pending bytes fit */
		out0->Output();
		check_eq("Output() drains only up to canSend (2 bytes)", (unsigned long)g_regSent.size(), 2ul);
		check_eq("regularReadIdx advanced by exactly 2", out0->regularReadIdx, 2);
		check_eq("ScheduleFromLinux() fired (sLinuxPending) via the clamp path", g_regOutputCalls, 1);

		out0->realtimeWriteIdx = out0->realtimeReadIdx = 0;
		out0->regularWriteIdx = out0->regularReadIdx = 0;
	}

	printf("[8] CKorgUsbAudioDriverMidiPorts::ProcessOutput() -- gates on active flag + pending diff\n");
	{
		g_rtOutputCalls = 0; g_regOutputCalls = 0;
		out0->realtimeRing[0] = 0xFA; out0->realtimeWriteIdx = 1; out0->realtimeReadIdx = 0;
		CKorgUsbAudioDriverMidiPorts::sInstance.ProcessOutput();
		check_eq("ProcessOutput() drained out0's pending realtime byte", g_rtOutputCalls, 1);
		check_eq("out1 (never activated) produced no output calls", g_regOutputCalls, 0);
		out0->realtimeWriteIdx = out0->realtimeReadIdx = 0;
	}

	printf("[9] STGMidiOutPortKorgUsb_Output() -- singleton call-through, same effect as ProcessOutput()\n");
	{
		g_rtOutputCalls = 0;
		out0->realtimeRing[0] = 0xFC; out0->realtimeWriteIdx = 1; out0->realtimeReadIdx = 0;
		STGMidiOutPortKorgUsb_Output();
		check_eq("STGMidiOutPortKorgUsb_Output() drained the same pending byte", g_rtOutputCalls, 1);
		out0->realtimeWriteIdx = out0->realtimeReadIdx = 0;
	}

	printf("[10] CSTGMidiOutPortKorgUsb::Deactivate() -- companion Disconnect() first, then base\n");
	{
		int doneBefore = g_korgDoneCalls;
		out0->Deactivate();
		check_eq("flags bit1 (active) cleared", (out0->flags >> 1) & 1, 0);
		check_eq("STGMidiOutPortKorgUsb_Done() ran (rt_free_srq)", g_freeSrqCalls, 1);
		check_eq("KorgUsbMidiDone(0) called", g_korgDoneCalls, doneBefore + 1);
		check_eq("STGMidiOutPortKorgUsb_Initialized() == false after Done()", STGMidiOutPortKorgUsb_Initialized(), 0);
	}

	printf("[11] CSTGMidiInPortKorgUsb::Activate()/Deactivate() -- base (deferred stand-in) + Connect/Disconnect\n");
	{
		CSTGMidiInPortKorgUsb *in1 = CKorgUsbAudioDriverMidiPorts::sInstance.InPort(1);
		int initBefore = g_korgInitializeCalls;
		in1->Activate((CSTGMidiQueue *)q3objB);
		check_eq("base CSTGMidiInPort::Activate() stand-in was called", g_inActivateCalls, 1);
		check_eq("KorgUsbMidiInitialize() called for port 1", g_korgInitializeCalls, initBefore + 1);
		check_eq("KorgUsbMidiInitialize() idx == 1", g_korgInitializeLastIdx, 1);

		in1->Deactivate();
		check_eq("base CSTGMidiInPort::Deactivate() stand-in was called", g_inDeactivateCalls, 1);
	}

	printf("[12] ShouldActivate() -- real but unreachable, always true\n");
	{
		CSTGMidiInPortKorgUsb *in0 = CKorgUsbAudioDriverMidiPorts::sInstance.InPort(0);
		check_true("InPort ShouldActivate() == true", in0->ShouldActivate());
		check_true("OutPort ShouldActivate() == true", out0->ShouldActivate());
	}

	printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return g_fail ? 1 : 0;
}
