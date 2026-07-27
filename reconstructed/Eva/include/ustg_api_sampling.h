/*
 * ustg_api_sampling.h  -  USTGAPISampling, real class name for a shared "simple
 * command" primitive layer 3+ subsystems build on.
 *
 * BACKGROUND: ustg_api_wrappers.h (2026-07-27) flagged USTGAPIPCMBanks/
 * USTGAPISampling (51/46 methods) as "by far the largest [USTGAPIXxx sub-cluster],
 * not spot-checked, presumed genuinely deep". A follow-up spot-check (same day,
 * this file) while reconstructing USTGAPICDAudio found something more specific:
 * USTGAPICDAudio doesn't send STGMessages directly at all -- every one of its 12
 * methods routes through 4 real USTGAPISampling primitive methods
 * (SharedScratch/SendSimpleMessage/ReceiveSimpleMessage/ReceiveMessage), which
 * ARE the thin STGMessage-facade shape (same USTGUserAPI::SendSTGMessageWithSource/
 * ReadMessage substrate ustg_api_wrappers.h's whole family already uses), just
 * indirected through USTGAPISampling's own namespace rather than building each
 * message inline. USTGAPISampling's own ~46 "UpdateXxx"/dump methods (a quick
 * sample of 4 -- UpdateLevelSlider/UpdateTrigger/UpdateThreshold/UpdatePretrigger,
 * .text+0x08e230f0-0x08e231e0 -- confirms this) build their OWN STGMessages
 * directly the same way ustg_api_wrappers.h's family does, NOT through these 4
 * primitives -- so the "USTGAPIPCMBanks/USTGAPISampling are deep" verdict stands
 * for the bulk of the class; only these 4 shared primitives are reconstructed
 * here, because USTGAPICDAudio genuinely needs them. A dedicated future pass
 * could plausibly reconstruct the rest of USTGAPISampling/USTGAPIPCMBanks the
 * same field-by-field way ustg_api_wrappers.h's own batch did -- this is a
 * precise lead, not a guess.
 *
 * All 4 primitives share one wire shape, confirmed from their own real bodies
 * (.text+0x08e24870/0x08e24970/0x08e24a80, plus SharedScratch at 0x08e230d0):
 * a 24-byte STGMessage {u16 len=0x18; u16 subtype=1; u32 type=1; u32 subcode=0xc;
 * u32 payload0; u32 payload1; u32 payload2} -- NOTE this "type"/"subcode" pair is
 * the OPPOSITE role split from ustg_api_wrappers.h's own family (there, `type`
 * identifies the subsystem and `subcode` the per-command opcode; here `type=1` is
 * constant across every one of these calls ("simple 3-int command" shape) and
 * `subcode=0xc` is the constant Sampling/CDAudio subsystem id -- the real
 * per-command opcode lives in a PAYLOAD dword instead, at a position that itself
 * varies per primitive). Documented precisely per-function below rather than
 * asserting one unified field-name scheme across both families.
 */

#ifndef USTG_API_SAMPLING_H
#define USTG_API_SAMPLING_H

class USTGAPISampling {
public:
	/* .text+0x08e230d0, 20 bytes. Returns
	 * `USTGUserAPI::mFrontPanelStatusAddress ? (char*)mFrontPanelStatusAddress +
	 * 0xd34 : NULL` -- reuses the SAME shared-memory region Connect() already
	 * attaches for front-panel status as a general per-process scratch buffer,
	 * offset 0xd34 in. Real, not a new shared-memory attach of its own.
	 */
	static void *SharedScratch();

	/* .text+0x08e24a80, 75 bytes. Sends {type=1,subcode=0xc,payload0=arg3,
	 * payload1=opcode,payload2=arg2} -- real payload order, NOT (opcode,arg2,arg3)
	 * despite the parameter list reading that way at the call site.
	 */
	static bool SendSimpleMessage(int opcode, int arg2, int arg3);

	/* .text+0x08e24970, 271 bytes. Sends {type=1,subcode=0xc,payload0=0,
	 * payload1=opcode,payload2=0}, polls USTGUserAPI::ReadMessage() up to 8x,
	 * writes the response's own payload2 field back through `out`, and returns
	 * whether the response's subcode field == 9 (a fixed ack sentinel -- NOT an
	 * echo of the sent subcode, unlike ustg_api_wrappers.h's WaitForDumpSubcodeEcho()).
	 */
	static bool ReceiveSimpleMessage(int opcode, unsigned long &out);

	/* .text+0x08e24870, 253 bytes. Sends {type=1,subcode=0xc,payload0=arg3,
	 * payload1=opcode,payload2=arg4} into the caller-supplied 24-byte buffer
	 * `buf` (opcode is the 2nd formal parameter here, NOT the 1st, unlike
	 * SendSimpleMessage()/ReceiveSimpleMessage() above), then polls
	 * ReadMessage() up to 8x INTO THE SAME BUFFER (the response overwrites the
	 * request in place -- callers read fields back out of `buf` afterward, e.g.
	 * USTGAPICDAudio::GetCurrentPosition() reads buf+0xc/buf+0x14). Returns
	 * whether the response's subcode field == 9, same sentinel as
	 * ReceiveSimpleMessage().
	 */
	static bool ReceiveMessage(char *buf, int opcode, int arg3, int arg4);
};

#endif /* USTG_API_SAMPLING_H */
