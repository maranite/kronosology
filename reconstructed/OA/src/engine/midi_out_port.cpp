// SPDX-License-Identifier: GPL-2.0
/*
 * midi_out_port.cpp  -  CSTGMidiPortManager::RegisterMidiOutPort() +
 * CSTGMidiOutPort::Activate() (real MIDI-OUT path reconstruction).
 *
 * Deliberately a separate TU from midi_port_manager.cpp: that file is
 * already large and actively shared with an in-flight parallel MIDI-IN
 * pass on this same tree (see PLAN.md-adjacent coordination notes) --
 * new code goes in its own file rather than risk a conflicting edit
 * there, matching this project's established per-symbol-cluster TU
 * convention (see midi_queue.cpp/midi_queue_writer.cpp's own precedent).
 *
 * Ground truth for everything in this file is
 * KronosScreenRemoteDaemon/midi_module/midi_bridge.c, an independently
 * developed, real-hardware-validated kernel module that taps this exact
 * subsystem live (see that file's own header comment + resolve_out_ports()
 * / QUEUE_PTR_OFF / QUEUE_BUF_OFF / RC_* definitions). See
 * oa_engine.h's CSTGMidiPortManager::RegisterMidiOutPort() declaration
 * and oa_engine_init.h's CSTGMidiOutPort definition for the full citation
 * of each individual field/offset.
 */

#include "oa_global.h"
#include "oa_engine.h"
#include "oa_engine_init.h"

/*
 * RegisterMidiOutPort(CSTGMidiOutPort*) -- confirmed real via the exact
 * byte pattern midi_bridge.c's resolve_out_ports() matches on real
 * OA.ko:
 *   0f be 50 04     movsx edx, byte ptr [eax+4]
 *   89 04 95 <d32>  mov   sMidiOutPorts[edx*4], eax
 *   c3              ret
 * A plain static function (no `this`): arg1 arrives in eax under this
 * project's whole-TU regparm(3) convention, matching the real ABI here
 * exactly (the real function also expects its one argument in eax).
 * `index` is read as a SIGNED byte (movsx, not movzx) and used
 * unclamped -- an out-of-range portIndex (e.g. the byte never having
 * been set, still 0 from a freshly zeroed object) writes outside the
 * 4-element array. Reproduced faithfully rather than guarded, matching
 * this project's "preserve real bugs" convention (see e.g.
 * midi_port_manager.cpp's NotifyNKS4TestMode() header comment for the
 * same treatment of a different real near-NULL-deref).
 */
void CSTGMidiPortManager::RegisterMidiOutPort(CSTGMidiOutPort *port)
{
	unsigned char *p = (unsigned char *)port;
	int index = (signed char)p[0x4];

	((void **)sMidiOutPorts)[index] = port;
}

/*
 * Activate(int, CSTGMidiQueue*, unsigned char*) -- real method NAME
 * confirmed (midi_bridge.c's own header comment cites
 * "CSTGMidiOutPort::Activate" as the source of the q0-q3 field layout);
 * this body is NOT independently disassembled -- it is the direct,
 * conservative C expression of that confirmed layout: store the queue
 * pointer and its data buffer at the slot's two confirmed offsets, then
 * claim this port's OWN reader slot on that queue via the already-real
 * CSTGMidiQueue::AllocReader() (global.cpp, sec 10.82 -- a confirmed
 * `lock xadd $1,[queue+0x20]`) and record the returned index at the
 * slot's third confirmed offset. `qslot` is 0..3 (q0..q3); out-of-range
 * values are not guarded, matching RegisterMidiOutPort()'s own real
 * unclamped-index behavior above.
 */
void CSTGMidiOutPort::Activate(int qslot, CSTGMidiQueue *queue, unsigned char *buf)
{
	unsigned char *self = (unsigned char *)this;

	static const unsigned int kQueueOff[4] = { 0x08, 0x14, 0x20, 0x2c };
	static const unsigned int kBufOff[4]   = { 0x0c, 0x18, 0x24, 0x30 };
	static const unsigned int kIdxOff[4]   = { 0x10, 0x1c, 0x28, 0x34 };

	*(CSTGMidiQueue **)(self + kQueueOff[qslot]) = queue;
	*(unsigned char **)(self + kBufOff[qslot])   = buf;
	self[kIdxOff[qslot]] = queue ? queue->AllocReader() : 0;
}
