// SPDX-License-Identifier: GPL-2.0
/*
 * midi_out_port.cpp  -  CSTGMidiPortManager::RegisterMidiOutPort() (real
 * MIDI-OUT path reconstruction).
 *
 * Deliberately a separate TU from midi_port_manager.cpp: that file is
 * already large and actively shared with an in-flight parallel MIDI-IN
 * pass on this same tree (see PLAN.md-adjacent coordination notes) --
 * new code goes in its own file rather than risk a conflicting edit
 * there, matching this project's established per-symbol-cluster TU
 * convention (see midi_queue.cpp/midi_queue_writer.cpp's own precedent).
 *
 * Ground truth is KronosScreenRemoteDaemon/midi_module/midi_bridge.c, an
 * independently developed, real-hardware-validated kernel module that
 * taps this exact subsystem live (see that file's own header comment +
 * resolve_out_ports() / QUEUE_PTR_OFF / QUEUE_BUF_OFF / RC_*
 * definitions). See oa_engine_init.h's CSTGMidiOutPort definition for
 * the full citation of each individual field/offset.
 *
 * CSTGMidiOutPort::Activate() USED to live in this file too, as a
 * speculative single-body guess (`Activate(int qslot, CSTGMidiQueue*,
 * unsigned char*)`) built purely from midi_bridge.c's field layout with
 * no independent disassembly. It has been REMOVED here: full
 * `objdump -dr` against OA.ko_Decomp/OA.ko (OA.ko MIDI-OUT hardware
 * batch) found the real signature takes exactly ONE argument and always
 * wires up all 4 slots itself -- see midi_out_port_serial.cpp's real,
 * disassembly-confirmed `CSTGMidiOutPort::Activate(CSTGMidiQueue*)`
 * body (declared alongside the rest of the base class's now-fully-
 * reconstructed methods in oa_engine_init.h).
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
