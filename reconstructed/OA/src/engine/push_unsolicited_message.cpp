// SPDX-License-Identifier: GPL-2.0
/*
 * push_unsolicited_message.cpp  -  PushUnsolicitedMessage(void*) (sec
 * 10.90), .text+0x116860 in OA_real.ko, 88 bytes.
 *
 * Confirmed real via a full objdump disassembly + relocation trace.
 * Every confirmed real caller (CSTGGlobal::SendUnsolGlobalMessageToUI,
 * CPowerOffTimer::Initialize -- sec 10.90/10.59) builds a small tagged
 * struct whose own first word is a size/type tag and second word is an
 * `eSTGMidiSource`-shaped "source" field; this function forwards it to
 * the real RTAI FIFO minor 5 (the same minor `stg_rtfifo_init`, sec
 * 10.51, already creates with a 0x10000-byte capacity -- consistent
 * with an "unsolicited UI message" queue being the largest of the 6
 * real FIFOs).
 *
 * Gated by two confirmed real flags (anonymous fixed `.bss` addresses,
 * no real symbol name recovered for either -- unlike `gSystemIsInitialized`
 * below, which IS a real, directly-extracted symbol name):
 *   - `.bss+0x106de0`: "unsolicited messages enabled" (checked first;
 *     an early-return no-op if clear).
 *   - `gSystemIsInitialized` (.bss+0x10725c, a real confirmed symbol
 *     name straight from OA_real.ko's own symbol table, not guessed):
 *     a second, independent early-return gate.
 *   - `.bss+0x106dec`: if nonzero, increments a confirmed real counter
 *     at `.bss+0x10724c` (own meaning not independently determined --
 *     plausibly a diagnostic "messages sent" tally).
 * Also confirmed real: a genuine tag-rewrite quirk -- if the message's
 * own `source` field (its `+0x2` word) equals `9`, it's rewritten to
 * `8` before sending (real semantics of these two source-code values
 * not independently determined; reproduced verbatim as a confirmed
 * quirk, not "cleaned up").
 */

#include "oa_rtfifo_init.h"

/*
 * Not marked `static` -- these are real .bss globals with no other
 * confirmed writer anywhere in this project yet (plausibly set by a
 * not-yet-reconstructed subsystem elsewhere in OA.ko); left externally
 * visible/settable so host KATs can exercise both gates directly,
 * matching how the real firmware's own not-yet-identified writer would.
 */
unsigned char g_unsolicitedMessagesEnabled;
unsigned char g_unsolicitedMessageCounterEnabled;
unsigned int g_unsolicitedMessageCount;
extern "C" unsigned int gSystemIsInitialized;
unsigned int gSystemIsInitialized;

extern "C" void PushUnsolicitedMessage(void *msg)
{
	if (g_unsolicitedMessagesEnabled == 0)
		return;
	if (gSystemIsInitialized == 0)
		return;

	unsigned char *m = (unsigned char *)msg;
	unsigned short *source = (unsigned short *)(m + 2);
	if (*source == 9)
		*source = 8;

	if (g_unsolicitedMessageCounterEnabled != 0)
		g_unsolicitedMessageCount++;

	unsigned short size = *(unsigned short *)m;
	rtf_put_if(5, msg, size);
}
