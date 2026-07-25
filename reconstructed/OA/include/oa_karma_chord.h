// SPDX-License-Identifier: GPL-2.0
/*
 * oa_karma_chord.h  -  the "Pads (touch to play)" KARMA chord-memory
 * trigger path: RT_chord_trigger() and its small immediate free-function
 * callees. See src/engine/karma_chord_trigger.cpp for the full ground
 * truth / derivation. New standalone header (not inserted into
 * oa_engine.h/oa_global.h) since no KARMA chord-memory declarations
 * exist anywhere in this tree yet -- avoids a shared-header edit while
 * other agents are concurrently touching those files.
 */

#ifndef OA_KARMA_CHORD_H
#define OA_KARMA_CHORD_H

extern "C" {

/* RT_chord_trigger  .text+0x522842 (OA_real.ko static addr 0x512842,
 * see chord_probe.c), 1688 bytes, regparm(3): EAX=pad_index EDX=velocity
 * ECX=param3 [stack]=param4. THE function: real per-pad "Pads (touch to
 * play)" trigger, confirmed via CESProgTask::GetPad1..GetPad8 and
 * KronosScreenRemoteDaemon's PADCHORD command (nks4_inject.c). */
void RT_chord_trigger(unsigned char pad_index, unsigned char velocity,
                       unsigned char param3, unsigned char param4)
	__attribute__((regparm(3)));

/* Do_KM_note_out_chord_trig  .text+0x56e3fa, 21 bytes, regparm(3):
 * EAX=channel EDX=note ECX=velocity. Real body: pure forward to
 * KM_note_out_chord_trig(channel, note, velocity). */
void Do_KM_note_out_chord_trig(unsigned char channel, unsigned char note,
                                unsigned char velocity)
	__attribute__((regparm(3)));

/* Do_KM_get_global_channel  .text+0x56e9a4, 12 bytes, cdecl, no args.
 * Real body: pure forward to KM_get_global_channel(). Returns the
 * KARMA-engine "local controller channel" byte used whenever a pad's
 * per-pad channel-selector field reads the 0x10 sentinel (see
 * karma_chord_trigger.cpp). */
unsigned char Do_KM_get_global_channel(void);

/* DoDynamicMidi  .text+0x527d5c, 7946 bytes, regparm(3): EAX=srcIndex
 * EDX=b0 ECX=b1 [stack]=b2. Genuine large dynamic-MIDI-mapping dispatch
 * shared by many callers across OA.ko, not specific to chord-memory --
 * out of scope for this pass, given a safe no-op stub in
 * karma_chord_trigger.cpp. RT_chord_trigger's own two call sites
 * (the "dynamic MIDI echo" tail) are reconstructed faithfully; only
 * this callee's body is deferred. */
void DoDynamicMidi(unsigned char srcIndex, unsigned char b0, unsigned char b1,
                    unsigned char b2)
	__attribute__((regparm(3)));

} /* extern "C" */

#endif /* OA_KARMA_CHORD_H */
