/*
 * ustg_api_cdaudio.h  -  USTGAPICDAudio, real name for Eva's client-side "external
 * CD/digital audio input channel" control surface.
 *
 * BACKGROUND: flagged "confirmed different-shaped" from the USTGAPIXxx thin-
 * STGMessage-facade family by ustg_api_wrappers.h (2026-07-27) -- correct: none of
 * these 12 methods builds an STGMessage inline. All 12 route through 4 real
 * USTGAPISampling primitive methods instead (ustg_api_sampling.h, reconstructed
 * alongside this file the same day) -- CDAudio turns out to be a genuinely simple,
 * fully tractable class once those primitives exist.
 *
 * Every method here is Tier A, transcribed field-by-field from
 * `objdump -dr -M intel` (Decomp/EVA_Decomp/Eva, .text+0x08e1b3f0-0x08e1b6f0).
 */

#ifndef USTG_API_CDAUDIO_H
#define USTG_API_CDAUDIO_H

/* Opaque scalar stand-ins for real enum types named only in symbols.csv's own
 * mangled signatures -- same "declared opaque, byte-for-byte faithful" precedent
 * as ustg_api_wrappers.h's eSTGMsgPerfType/eSTGProgramBankId/eSTGVoiceModelType.
 * Real call sites pass all 4 as plain ints/bytes per their own real widths below.
 */
typedef unsigned int eSTGAPIBusIDOut;
typedef unsigned int eSTGAPIFXCtrlBus;
typedef unsigned int eSTGAPIHDRBus;

/* GetCurrentPosition()'s 2nd out-param. Real values only partially known: the
 * failure path (ReceiveMessage() succeeds but the position field is negative)
 * writes the literal 3 -- everything else about this enum's real member names
 * is unrecovered.
 */
typedef unsigned int EAudioStatus;

class USTGAPICDAudio {
public:
	/* .text+0x08e1b3f0, 138 bytes. Not GetProductInfo()-shaped at all: writes
	 * `name` directly into USTGAPISampling::SharedScratch() via strncpy(255)
	 * (a shared scratch buffer, not per-call caller memory) before sending.
	 * Real quirk preserved: returns false immediately (without touching
	 * `name`/sending anything) if SharedScratch() is NULL.
	 */
	static bool PlayStandby(const char *name, unsigned long a2, unsigned int a3, unsigned long a4, unsigned int a5);

	/* .text+0x08e1b480, opcode 0x34. */
	static bool PlayStart();

	/* .text+0x08e1b4b0, opcode 0x35. */
	static bool PlayStop();

	/* .text+0x08e1b4e0, opcode 0x36. Real quirk preserved: on a negative
	 * position field, writes *position=0, *status=3 and returns false rather
	 * than propagating raw values.
	 */
	static bool GetCurrentPosition(unsigned long &position, EAudioStatus &status);

	/* .text+0x08e1b570, opcode 0x37. */
	static void SetLevel(unsigned char level);

	/* .text+0x08e1b5a0, opcode 0x38. */
	static void SetChanLevel(unsigned char chan, unsigned char level);

	/* .text+0x08e1b5d0, opcode 0x39. */
	static void SetChanPan(unsigned char chan, unsigned char pan);

	/* .text+0x08e1b600, opcode 0x3a. */
	static void SetChanBusSelect(unsigned char chan, eSTGAPIBusIDOut busId);

	/* .text+0x08e1b630, opcode 0x3b. */
	static void SetChanSend1Level(unsigned char chan, unsigned char level);

	/* .text+0x08e1b660, opcode 0x3c. */
	static void SetChanSend2Level(unsigned char chan, unsigned char level);

	/* .text+0x08e1b690, opcode 0x3d. */
	static void SetChanFXControlBus(unsigned char chan, eSTGAPIFXCtrlBus bus);

	/* .text+0x08e1b6c0, opcode 0x3e. */
	static void SetChanHDRBus(unsigned char chan, eSTGAPIHDRBus bus);
};

#endif /* USTG_API_CDAUDIO_H */
