// SPDX-License-Identifier: GPL-2.0
/*
 * test_midi_in_port_serial.cpp  -  host-side known-answer test for
 * CSTGMidiInPortSerial::ReceiveByte()/ReceiveBytes()/
 * CheckForCompleteMessage() (physical MIDI-IN UART byte parser).
 *
 * Links src/engine/midi_in_port_serial.cpp + src/engine/midi_queue.cpp
 * (real GetNumWritableBytes()) + src/engine/midi_queue_writer.cpp +
 * src/engine/midi_queue_writer_byte.cpp (real Write() overloads) +
 * src/stub/bar2_stubs.cpp's StartSysEx()/ReceiveSysExData() deferred
 * no-ops (pulled in via a tiny local stand-in here instead, to avoid
 * dragging bar2_stubs.cpp's much larger dependency footprint into this
 * test -- see below).
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_heapmanager.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
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

/* Same RingCtl shape as test_midi_queue_writer.cpp. */
struct RingCtl {
	unsigned char pad0[8];
	unsigned int mask;
	unsigned int writeCursor;
	unsigned int readerPos[4];
	unsigned char readerCount;
};

/* Required externs this TU must provide (this project's convention: the
 * test itself owns "singleton" storage, not a shared mock file). */
CSTGMidiPortManager *CSTGMidiPortManager::sInstance;
CSTGGlobal *CSTGGlobal::sInstance;
CSTGMessageProcessor *CSTGMessageProcessor::sInstance;

/* CSTGMidiPortManager::sMidiInPorts/RegisterMidiInPort() -- needed since
 * this file's own CSTGMidiInPort::CSTGMidiInPort() ctor (added for the
 * KorgUsb MIDI transport batch, midi_korgusb_port.cpp) calls it; this
 * test never constructs a real CSTGMidiInPort via the ctor itself (only
 * via a raw FakePort cast, below), but the ctor's own symbol is still
 * pulled in by linking this TU and must resolve. */
void *CSTGMidiPortManager::sMidiInPorts[4];
void CSTGMidiPortManager::RegisterMidiInPort(CSTGMidiInPort *port)
{
	unsigned char *p = (unsigned char *)port;
	int index = (signed char)p[0x25];
	if (index >= 0 && index < 4)
		((void **)sMidiInPorts)[index] = port;
}

/* StartSysEx()/ReceiveSysExData() deferred stand-ins, tracked so the
 * "0xF0 dispatches to StartSysEx()" and "in-SysEx data dispatches to
 * ReceiveSysExData()" call sites can be verified without pulling in
 * bar2_stubs.cpp's much larger dependency graph. */
static int g_startSysExCalls;
static int g_receiveSysExDataCalls;
void CSTGMidiInPort::StartSysEx() { g_startSysExCalls++; }
void CSTGMidiInPort::ReceiveSysExData(unsigned char) { g_receiveSysExDataCalls++; }

/* CSTGMidiInPort::Activate()/Deactivate() are now real (this pass, same
 * file this test links) -- their own bodies ODR-use CSTGHeapManager::
 * sInstance/CSTGAudioBusManager::sInstance, CSTGMidiQueue::Initialize()/
 * SetDesc(), and CSTGExtMIDIClockSync::Initialize()/its own vtable, so
 * linking this TU now requires all of them to resolve EVEN THOUGH this
 * test (a byte-parser KAT, never constructs a real port or calls
 * Activate()) never actually calls any of them -- same "unresolved
 * symbol from dead code in a linked TU" situation already documented
 * above for the ctor's own RegisterMidiInPort() dependency. Minimal
 * link-satisfying stand-ins, not exercised by any check in this file. */
CSTGHeapManager *CSTGHeapManager::sInstance;
CSTGAudioBusManager *CSTGAudioBusManager::sInstance;
extern "C" unsigned char _ZTV20CSTGExtMIDIClockSync[40];
unsigned char _ZTV20CSTGExtMIDIClockSync[40];
void CSTGMidiQueue::Initialize(unsigned int, unsigned int) { }
void CSTGMidiQueue::SetDesc(const char *, ...) { }
void CSTGExtMIDIClockSync::Initialize() { }

/* Layout mirrors oa_engine.h's CSTGMidiInPort exactly enough for this
 * test: scratch[0x20] @+0x4, sysExScratchLen @+0x24, portType @+0x25,
 * flags @+0x26, writer offsets @+0xf0.._0x104, sysex-state block
 * @+0x2e0..+0x2e3, realtime ring @+0x140..+0x1af, ring index @+0x1ac. */
struct FakePort {
	unsigned char raw[0x300];
};

int main(void)
{
	printf("CSTGMidiInPortSerial::ReceiveByte()/ReceiveBytes() known-answer test\n");
	printf("=========================================================\n");

	/* MidiPortManager gate: only its OWN storage's low byte matters
	 * (Oddity #1) -- point it at itself so the byte is non-zero. */
	CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)0x1;

	/* CSTGGlobal::sInstance needs to support BOTH the tiny [0x6ac]
	 * KG-bypass-flag read AND the "Oddity #2" [0x29c9fa8] (~44MB)
	 * out-of-bounds read (see file/oa_engine.h's own documented
	 * anomaly) from the SAME base pointer -- unlike test_tick_count.cpp's
	 * single-offset "fabricate a base so the field lands on a local
	 * variable" trick, two very different offsets off one base need an
	 * actual backing allocation here, so a real (if generously sized)
	 * mmap is used instead. */
	unsigned long globalStorageSize = 0x29c9fb0;
	unsigned char *globalStorage =
		(unsigned char *)mmap(0, globalStorageSize, PROT_READ | PROT_WRITE,
				       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	CSTGGlobal::sInstance = (CSTGGlobal *)globalStorage;	/* [0x6ac] = 0: KG-bypass off */

	unsigned char mpStorage[0x50];
	memset(mpStorage, 0, sizeof(mpStorage));
	CSTGMessageProcessor::sInstance = (CSTGMessageProcessor *)mpStorage; /* [0x48] = 0: not busy */

	/* Primary + realtime + KG ring/buffer pairs (16 bytes each). */
	RingCtl *primCtl = (RingCtl *)mmap32(sizeof(RingCtl));
	unsigned char *primBuf = (unsigned char *)mmap32(64);
	RingCtl *rtCtl = (RingCtl *)mmap32(sizeof(RingCtl));
	unsigned char *rtBuf = (unsigned char *)mmap32(64);
	RingCtl *kgCtl = (RingCtl *)mmap32(sizeof(RingCtl));
	unsigned char *kgBuf = (unsigned char *)mmap32(64);

	auto resetRings = [&]() {
		memset(primCtl, 0, sizeof(RingCtl));
		memset(rtCtl, 0, sizeof(RingCtl));
		memset(kgCtl, 0, sizeof(RingCtl));
		primCtl->mask = 0xf;
		rtCtl->mask = 0xf;
		kgCtl->mask = 0xf;
		memset(primBuf, 0xCC, 64);
		memset(rtBuf, 0xCC, 64);
		memset(kgBuf, 0xCC, 64);
	};

	FakePort *fp = new FakePort();
	CSTGMidiInPortSerial *port = (CSTGMidiInPortSerial *)fp;

	auto resetPort = [&]() {
		memset(fp->raw, 0, sizeof(fp->raw));
		fp->raw[0x26] = 0x3;	/* flags: active (bit1) + KG-forward-enabled (bit0) */
		*(unsigned int *)(fp->raw + 0xf0) = (unsigned int)(unsigned long)primCtl;
		*(unsigned int *)(fp->raw + 0xf4) = (unsigned int)(unsigned long)primBuf;
		*(unsigned int *)(fp->raw + 0xf8) = (unsigned int)(unsigned long)rtCtl;
		*(unsigned int *)(fp->raw + 0xfc) = (unsigned int)(unsigned long)rtBuf;
		*(unsigned int *)(fp->raw + 0x100) = (unsigned int)(unsigned long)kgCtl;
		*(unsigned int *)(fp->raw + 0x104) = (unsigned int)(unsigned long)kgBuf;
	};

	printf("[1] simple 3-byte channel message (NoteOn 0x90 0x40 0x7f)\n");
	{
		resetRings(); resetPort();
		port->ReceiveByte(0x90);
		port->ReceiveByte(0x40);
		port->ReceiveByte(0x7f);
		check_eq("cursor advanced by 3", primCtl->writeCursor, 3);
		check_eq("byte0 == status", primBuf[0], 0x90);
		check_eq("byte1", primBuf[1], 0x40);
		check_eq("byte2", primBuf[2], 0x7f);
		check_eq("scratch length reset to 1 (running status)", fp->raw[0x24], 1);
		check_eq("realtime ring untouched", rtCtl->writeCursor, 0);
	}

	printf("[2] running status: two more data bytes reuse status 0x90\n");
	{
		/* Continuing from test [1]'s state (msgLen==1, status==0x90). */
		port->ReceiveByte(0x50);
		port->ReceiveByte(0x60);
		check_eq("cursor advanced by another 3", primCtl->writeCursor, 6);
		check_eq("2nd message byte0 == reused status", primBuf[3], 0x90);
		check_eq("2nd message byte1", primBuf[4], 0x50);
		check_eq("2nd message byte2", primBuf[5], 0x60);
	}

	printf("[3] system-common message (Song Position Pointer, 0xF2 + 2 data bytes)\n");
	{
		resetRings(); resetPort();
		port->ReceiveByte(0xf2);
		port->ReceiveByte(0x10);
		port->ReceiveByte(0x20);
		check_eq("goes to REALTIME writer, not primary", primCtl->writeCursor, 0);
		check_eq("realtime cursor advanced by 3", rtCtl->writeCursor, 3);
		check_eq("byte0 == 0xF2", rtBuf[0], 0xf2);
		check_eq("byte1", rtBuf[1], 0x10);
		check_eq("byte2", rtBuf[2], 0x20);
		check_eq("scratch length reset to 0 (no running status)", fp->raw[0x24], 0);
	}

	printf("[4] Tune Request (0xF6) completes immediately, single byte\n");
	{
		resetRings(); resetPort();
		port->ReceiveByte(0xf6);
		check_eq("realtime cursor advanced by 1", rtCtl->writeCursor, 1);
		check_eq("byte0 == 0xF6", rtBuf[0], 0xf6);
		check_eq("scratch length reset to 0", fp->raw[0x24], 0);
	}

	printf("[5] realtime byte (0xF8 Clock) -> timestamp ring, not the message writers\n");
	{
		resetRings(); resetPort();
		port->ReceiveByte(0xf8);
		check_eq("primary untouched", primCtl->writeCursor, 0);
		/* PushRealtimeByte ALSO forwards the raw byte into the same
		 * +0xf8/+0xfc realtime CSTGMidiQueue the system-common path
		 * uses (confirmed identical to CSTGMidiInPortGeneric::Receive's
		 * own PushRealtimeByte, midi_in_port.cpp) -- not just the
		 * timestamp ring. */
		check_eq("realtime queue gets the raw byte too", rtCtl->writeCursor, 1);
		check_eq("realtime queue byte0 == 0xF8", rtBuf[0], 0xf8);
		check_eq("ring index advanced to 1", *(unsigned int *)(fp->raw + 0x1ac), 1);
		check_eq("ring slot 0 byte == 0xF8", fp->raw[0x14c], 0xf8);

		port->ReceiveByte(0xfa); /* Start */
		port->ReceiveByte(0xfb); /* Continue */
		port->ReceiveByte(0xfc); /* Stop */
		check_eq("ring index advanced to 4 total", *(unsigned int *)(fp->raw + 0x1ac), 4);
		check_eq("ring slot 1 byte == 0xFA", fp->raw[0x158], 0xfa);
		check_eq("ring slot 2 byte == 0xFB", fp->raw[0x164], 0xfb);
		check_eq("ring slot 3 byte == 0xFC", fp->raw[0x170], 0xfc);
		check_eq("realtime queue cursor advanced to 4 total", rtCtl->writeCursor, 4);

		port->ReceiveByte(0xf9); /* undefined -- ignored */
		check_eq("0xF9 does not advance ring index", *(unsigned int *)(fp->raw + 0x1ac), 4);
	}

	printf("[6] Active Sensing (0xFE) sets the flag only, no ring/writer traffic\n");
	{
		resetRings(); resetPort();
		port->ReceiveByte(0xfe);
		check_eq("activeSensingSeen set", fp->raw[0x2e3], 1);
		check_eq("ring index untouched", *(unsigned int *)(fp->raw + 0x1ac), 0);
		check_eq("primary untouched", primCtl->writeCursor, 0);
		check_eq("realtime untouched", rtCtl->writeCursor, 0);
	}

	printf("[7] SysEx start (0xF0) dispatches to StartSysEx()\n");
	{
		resetRings(); resetPort();
		g_startSysExCalls = 0;
		port->ReceiveByte(0xf0);
		check_eq("StartSysEx() called once", (unsigned int)g_startSysExCalls, 1);
		check_eq("no writer traffic (deferred stub)", primCtl->writeCursor, 0);
	}

	printf("[8] in-SysEx data byte dispatches to ReceiveSysExData()\n");
	{
		resetRings(); resetPort();
		g_receiveSysExDataCalls = 0;
		fp->raw[0x2e0] = 2;	/* sysExState = locally-buffering */
		port->ReceiveByte(0x55);
		check_eq("ReceiveSysExData() called once", (unsigned int)g_receiveSysExDataCalls, 1);
	}

	printf("[9] bare 0xF7 with no active SysEx: reset only, no forwarding\n");
	{
		resetRings(); resetPort();
		port->ReceiveByte(0xf7);
		check_eq("no primary traffic", primCtl->writeCursor, 0);
		check_eq("no realtime traffic", rtCtl->writeCursor, 0);
		check_eq("scratch length is 0", fp->raw[0x24], 0);
	}

	printf("[10] stray data byte with no active message is dropped\n");
	{
		resetRings(); resetPort();
		port->ReceiveByte(0x40);	/* data byte, msgLen==0, no running status yet */
		check_eq("no primary traffic", primCtl->writeCursor, 0);
		check_eq("scratch length still 0", fp->raw[0x24], 0);
	}

	printf("[11] port not active (flags bit1 clear): fully gated off\n");
	{
		resetRings(); resetPort();
		fp->raw[0x26] = 0;	/* inactive */
		port->ReceiveByte(0x90);
		port->ReceiveByte(0x40);
		port->ReceiveByte(0x7f);
		check_eq("no primary traffic", primCtl->writeCursor, 0);
	}

	printf("[12] MidiPortManager gate closed (Oddity #1): fully gated off\n");
	{
		resetRings(); resetPort();
		CSTGMidiPortManager::sInstance = 0;	/* low byte of storage == 0 */
		port->ReceiveByte(0x90);
		port->ReceiveByte(0x40);
		port->ReceiveByte(0x7f);
		check_eq("no primary traffic", primCtl->writeCursor, 0);
		CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)0x1;	/* restore */
	}

	printf("[13] ReceiveBytes(): normal path, two channel messages, 2nd via running status\n");
	{
		resetRings(); resetPort();
		/* 2nd NoteOn omits its status byte (real running-status wire
		 * encoding: 0x90 note vel note vel ...). */
		unsigned char buf[5] = { 0x90, 0x40, 0x7f, 0x40, 0x50 };
		port->ReceiveBytes(buf, 5);
		check_eq("cursor advanced by 6 (both messages)", primCtl->writeCursor, 6);
		check_eq("2nd message byte0 == reused status", primBuf[3], 0x90);
		check_eq("2nd message byte1", primBuf[4], 0x40);
		check_eq("2nd message byte2", primBuf[5], 0x50);
	}

	printf("[14] ReceiveBytes(): KG-bulk-bypass path (Global[0x6ac] != 0)\n");
	{
		resetRings(); resetPort();
		globalStorage[0x6ac] = 1;
		unsigned char buf[4] = { 0x11, 0x22, 0x33, 0x44 };
		port->ReceiveBytes(buf, 4);
		check_eq("whole buffer written to KG in one call", kgCtl->writeCursor, 4);
		check_eq("byte0", kgBuf[0], 0x11);
		check_eq("byte3", kgBuf[3], 0x44);
		check_eq("primary untouched (bypass, not the per-byte parser)", primCtl->writeCursor, 0);
		globalStorage[0x6ac] = 0;
	}

	printf("[15] ReceiveByte(): KG-bulk-bypass path (single byte)\n");
	{
		resetRings(); resetPort();
		globalStorage[0x6ac] = 1;
		port->ReceiveByte(0x77);
		check_eq("single byte written to KG", kgCtl->writeCursor, 1);
		check_eq("byte0", kgBuf[0], 0x77);
		globalStorage[0x6ac] = 0;
	}

	printf("[16] CheckForCompleteMessage() called directly, incomplete message\n");
	{
		resetRings(); resetPort();
		fp->raw[4] = 0x90;	/* status */
		fp->raw[0x24] = 2;	/* only 2 of 3 bytes accumulated */
		port->CheckForCompleteMessage();
		check_eq("not flushed yet", primCtl->writeCursor, 0);
		check_eq("length unchanged", fp->raw[0x24], 2);
	}

	printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail ? 1 : 0;
}
