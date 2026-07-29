// SPDX-License-Identifier: GPL-2.0
/*
 * controller_rt_data_send_karma_cc.cpp -- CSTGControllerRTData::
 * SendKarmaCCToKG(eKarmaCCNo, unsigned char) (round 44, 2026-07-29, solo
 * -- session-wide 200-subagent dispatch cap hit, standing decompile-
 * everything goal continued directly by the main-loop assistant).
 *
 * Deliberately a separate TU (same established "give it its own TU"
 * convention as controller_rt_data_set_audio_in_solo.cpp) -- bar2_stubs.cpp
 * previously carried an empty-body stand-in for this exact symbol, now
 * removed.
 *
 * .text+0xd720, 80 bytes -- confirmed real via `objdump -dr`. Notably,
 * `this` (eax on entry, per this project's regparm(3) convention) is
 * NEVER read anywhere in the real body -- eax is immediately overwritten
 * and reused purely as scratch. The function operates entirely on two
 * global singletons instead:
 *   - CSTGGlobal::sInstance[+0x6b8] (the KARMA MIDI channel byte, same
 *     field UpdateKeyTranspose/UpdateLocalControl already use, see
 *     global.cpp's own SendGlobalMidiMessage() helper) ORed with 0xb0
 *     (MIDI Control-Change status byte) to form the status byte.
 *   - CSTGMidiPortManager::sInstance+0x208 (the embedded
 *     CSTGMidiQueueWriter sub-object, same confirmed embedding used
 *     throughout this project's MIDI-out path).
 *
 * The real 5-byte wire message is {status, ccNo, value, 0x05, 0xff} --
 * SAME overall shape as global.cpp's SendGlobalMidiMessage() (status,
 * fixed 0x79, byte2, 0x05, 0xff) but with the SECOND byte also variable
 * (ccNo, not a fixed 0x79), so that exact helper isn't reusable verbatim
 * -- reproduced here as its own small local helper instead of forcing a
 * shared signature change onto global.cpp's existing one.
 */

#include "oa_global.h"
#include "oa_engine.h"

void CSTGControllerRTData::SendKarmaCCToKG(int ccNo, unsigned char value)
{
	unsigned char status = (unsigned char)(((unsigned char *)CSTGGlobal::sInstance)[0x6b8] | 0xb0);
	unsigned char msg[5] = { status, (unsigned char)ccNo, value, 0x05, 0xff };

	CSTGMidiQueueWriter *writer =
		(CSTGMidiQueueWriter *)((unsigned char *)CSTGMidiPortManager::sInstance + 0x208);
	writer->Write(msg, 5, false);
}
