// SPDX-License-Identifier: GPL-2.0
/*
 * test_midi_usb_accessory_port.cpp  -  host-side known-answer test for
 * src/engine/midi_usb_accessory_port.cpp: CSTGUSBMidiAccessoryMidiInPort
 * (Activate/Deactivate/ShouldActivate), CSTGMidiOutPortUSB (Activate),
 * and CUSBMidiAccessory_DrumPadClient::ReceiveNotification (trivial
 * default no-op, exercised directly, no dependency setup needed).
 *
 * Links src/engine/midi_usb_accessory_port.cpp, midi_in_port_serial.cpp
 * (for the real base CSTGMidiInPort ctor/Activate/Deactivate),
 * midi_out_port_serial.cpp (for the real base CSTGMidiOutPort ctor/
 * Activate), and midi_queue.cpp/midi_queue_writer.cpp/
 * midi_queue_writer_byte.cpp (transitive dependency of the above, same
 * as test_midi_korgusb_port.cpp). Same minimal stand-ins for
 * CSTGMidiPortManager::sMidiInPorts/sMidiOutPorts/RegisterMidiInPort()/
 * RegisterMidiOutPort(), CSTGMidiQueue::AllocReader()/Initialize()/
 * SetDesc(), CSTGExtMIDIClockSync::Initialize() -- see that file's own
 * header comment for the full rationale, reused verbatim here since
 * this test links the exact same underlying TUs.
 *
 * CSTGUSBMidiAccessoryMidiInPort/CSTGMidiOutPortUSB placement-construct
 * the BASE class into derived-sized storage then reinterpret, matching
 * this project's established convention for classes that add zero new
 * fields (CSTGMidiInPortKorgUsb, midi_korgusb_port.cpp).
 */

#include <cstdio>
#include <cstring>
#include <new>
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

/* ---- Required externs/singletons this TU owns (matching
 * test_midi_korgusb_port.cpp's own established convention). --------- */
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

unsigned char CSTGMidiQueue::AllocReader()
{
	unsigned char *self = (unsigned char *)this;
	unsigned char old = self[0x20];
	self[0x20] = (unsigned char)(old + 1);
	return old;
}

void CSTGMidiQueue::Initialize(unsigned int, unsigned int)
{
	*(unsigned int *)this = 6; /* allocHandle */
}
void CSTGMidiQueue::SetDesc(const char *, ...)
{
}

static int g_extClockInitCalls;
void CSTGExtMIDIClockSync::Initialize()
{
	g_extClockInitCalls++;
}

extern "C" unsigned char _ZTV20CSTGExtMIDIClockSync[40];
unsigned char _ZTV20CSTGExtMIDIClockSync[40];

void CSTGMidiInPort::StartSysEx() { }
void CSTGMidiInPort::ReceiveSysExData(unsigned char) { }

/* midi_out_port_serial.cpp's other pure hooks -- pulled in because this
 * test links that TU for the shared CSTGMidiOutPort base ctor/Activate;
 * never exercised via CSTGMidiOutPortUSB::Activate(), no-op is fine. */
bool CSTGMidiOutPortSerial::CanTransmitHardware() const { return false; }
void CSTGMidiOutPortSerial::TransmitHardwareByte(unsigned char) { }

/* ---- USBMidiAccessory_SetMidiInClient() mock ------------------------ */
static int g_setMidiInClientCalls;
static void *g_setMidiInClientLastArg;
static bool g_setMidiInClientArgWasNonNull;
extern "C" void USBMidiAccessory_SetMidiInClient(void *client)
{
	g_setMidiInClientCalls++;
	g_setMidiInClientLastArg = client;
	if (client)
		g_setMidiInClientArgWasNonNull = true;
}

struct QueueObj {
	unsigned int allocHandle, format, mask, writeCursor, readerPos[4];
	unsigned char readerCount;
};

int main(void)
{
	printf("CSTGUSBMidiAccessoryMidiInPort / CSTGMidiOutPortUSB known-answer test\n");
	printf("=======================================================================\n");

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
	*(unsigned int *)(heap + 0x24 + 1 * 0x14) = (unsigned int)(unsigned long)buf0;
	*(unsigned int *)(heap + 0x24 + 2 * 0x14) = (unsigned int)(unsigned long)buf1;
	*(unsigned int *)(heap + 0x24 + 3 * 0x14) = (unsigned int)(unsigned long)buf2;
	/* Slot 6 -- CSTGMidiInPort::Activate()'s own embedded q1/q2 queues
	 * both resolve to this SAME slot via the local Initialize() mock
	 * above (this test never distinguishes them). */
	unsigned char *buf6 = (unsigned char *)mmap32(64);
	*(unsigned int *)(heap + 0x24 + 6 * 0x14) = (unsigned int)(unsigned long)buf6;

	unsigned char *mgr = (unsigned char *)mmap32(0x200);
	CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)mgr;
	QueueObj *q1obj = (QueueObj *)(mgr + 0x0c);
	QueueObj *q2obj = (QueueObj *)(mgr + 0x70);
	QueueObj *q0obj = (QueueObj *)(mgr + 0xd4);
	q0obj->allocHandle = 1; q0obj->mask = 0xf;
	q1obj->allocHandle = 2; q1obj->mask = 0xf;
	q2obj->allocHandle = 3; q2obj->mask = 0xf;

	QueueObj *q3obj = (QueueObj *)mmap32(sizeof(QueueObj));
	q3obj->allocHandle = 4; q3obj->mask = 0xf;

	printf("[1] CSTGUSBMidiAccessoryMidiInPort::Activate() -- base wiring + SetMidiInClient(non-NULL)\n");
	{
		/* +4 bytes: CSTGMidiInPort's own declared sizeof (0x2e4) is 4
		 * bytes short of its real 0x2e8 (no explicit vtable field) --
		 * base Activate() writes a real dword at self+0x2e4..0x2e7,
		 * so a buffer sized to plain sizeof() overflows by 4 bytes
		 * (caught live: this exact buffer was too small on a first
		 * draft and segfaulted). See oa_engine.h's own header note on
		 * CSTGMidiInPortKorgUsb for the same real 0x2e8 total. */
		alignas(8) static unsigned char storage[sizeof(CSTGMidiInPort) + 4];
		CSTGUSBMidiAccessoryMidiInPort *inPort = (CSTGUSBMidiAccessoryMidiInPort *)storage;
		new (storage) CSTGMidiInPort(0 /* portType */, 1 /* flagsInit */);

		check_eq("registered in sMidiInPorts[0] by ctor", (unsigned long)CSTGMidiPortManager::sMidiInPorts[0], (unsigned long)inPort);
		check_true("ShouldActivate() always true", inPort->ShouldActivate());

		QueueObj *inQ3 = (QueueObj *)mmap32(sizeof(QueueObj));
		inQ3->allocHandle = 6; inQ3->mask = 0xf;
		inPort->Activate((CSTGMidiQueue *)inQ3);
		check_eq("USBMidiAccessory_SetMidiInClient() called once", g_setMidiInClientCalls, 1);
		check_true("SetMidiInClient() called with non-NULL &sMidiInClient", g_setMidiInClientArgWasNonNull);

		printf("[2] CSTGUSBMidiAccessoryMidiInPort::Deactivate() -- SetMidiInClient(NULL) BEFORE base Deactivate()\n");
		inPort->Deactivate();
		check_eq("USBMidiAccessory_SetMidiInClient() called again (total 2)", g_setMidiInClientCalls, 2);
		check_eq("2nd call's arg is NULL (order confirmed: unregister first)", (unsigned long)g_setMidiInClientLastArg, 0);
	}

	printf("[3] CSTGMidiOutPortUSB::Activate() -- trivial forward to base CSTGMidiOutPort::Activate()\n");
	{
		alignas(8) static unsigned char outStorage[sizeof(CSTGMidiOutPort)];
		CSTGMidiOutPortUSB *outPort = (CSTGMidiOutPortUSB *)outStorage;
		new (outStorage) CSTGMidiOutPort(1 /* portType */, 0 /* flagsInit */);

		check_eq("registered in sMidiOutPorts[1] by ctor", (unsigned long)CSTGMidiPortManager::sMidiOutPorts[1], (unsigned long)outPort);
		check_eq("flags bit1 (active) NOT set before Activate()", (outPort->flags >> 1) & 1, 0);

		outPort->Activate((CSTGMidiQueue *)q3obj);
		check_eq("q0Buf resolved via handle 1 (base Activate() ran for real)", (unsigned long)outPort->q0Buf, (unsigned long)buf0);
		check_eq("q3Queue == caller-supplied", (unsigned long)outPort->q3Queue, (unsigned long)q3obj);
		check_eq("flags bit1 (active) now set", (outPort->flags >> 1) & 1, 1);
	}

	printf("[4] CUSBMidiAccessory_DrumPadClient::ReceiveNotification() -- confirmed 1-byte bare-ret default\n");
	{
		CUSBMidiAccessory_DrumPadClient client;
		client.ReceiveNotification(0); /* must not crash, no observable side effect */
		check_true("ReceiveNotification() returned (no side effects to check)", true);
	}

	printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return g_fail ? 1 : 0;
}
