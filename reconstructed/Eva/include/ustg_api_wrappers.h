/*
 * ustg_api_wrappers.h  -  the USTGAPIXxx "thin IPC facade" family, non-CValue slice.
 *
 * BACKGROUND: ustg_user_api.h's own header comment already names this family --
 * "the only remaining USTGUserAPI members not reconstructed are the ~150
 * per-subsystem USTGAPIXxx::UpdateYyy() wrapper classes" -- as the largest
 * unclaimed surface once the Stage 2 IPC substrate (SendSTGMessageWithSource/
 * ReadMessage) was real. A fresh nm -C class-inventory sweep of the whole binary
 * (2026-07-27, same methodology that found CResFamily/CPool/CSlotPool) confirmed
 * this is genuinely still true and picked it as the next largest tractable
 * unclaimed cluster: every one of these classes is small, non-Peg, non-CStorage,
 * and needs only already-real infrastructure (SendSTGMessageWithSource(),
 * ReadMessage(), both ustg_user_api.cpp).
 *
 * SHAPE: every method here builds a small fixed-size STGMessage-compatible struct
 * on the stack (header {u16 length; u16 subtype=1; u32 type; u32 subcode;} followed
 * by N raw 4-byte payload fields in the call's own argument order -- NOT
 * necessarily the same order as the C++ parameter list, confirmed per-function
 * from real disassembly, not assumed by analogy) and forwards it via the already-
 * real USTGUserAPI::SendSTGMessageWithSource(). A few ("SharedMemXxxDump"-named
 * methods) additionally poll USTGUserAPI::ReadMessage() up to 8 times afterward
 * and return whether the response's own subcode field (response buffer +8) echoes
 * back the subcode that was sent -- the real ack/dump-complete signal.
 *
 * SCOPE (this batch, 2026-07-27): every method in this family that does NOT take a
 * `CValue const&` argument. 4 real methods DO (`USTGAPIDrumkitData::
 * UpdateVSplitParam`, `USTGAPIVoiceModel::UpdateParam`/`UpdateLinkedParam`,
 * `USTGAPIWaveSequenceData::UpdateStepParam`) and are deliberately left out --
 * their own real disassembly shows they memcpy() a SELF-DESCRIBING, VARIABLE-
 * LENGTH byte range starting at the CValue object itself: `size = *(byte*)
 * ((char*)cvalue+1) + 4`, i.e. CValue's own byte at offset+1 is a length prefix
 * for its own variable-length payload (a tagged-value/string blob, size not
 * fixed). CValue's own real layout/semantics are not reconstructed anywhere in
 * this project (same "not modeled, no other reconstructed code needs it"
 * boundary as `CMessage`/`STGMessage` themselves) -- a real, precise, tractable-
 * with-more-effort lead for a future pass (the serialization rule itself IS
 * fully decoded above; what's missing is CValue's own field-level meaning).
 * `USTGAPIPatch`/`USTGAPIVoiceModel`'s own static data members
 * (`m_DefaultProgramId`/`m_DefaultBankId`) are also not reconstructed -- no
 * traced caller reads or writes either one.
 *
 * Also deliberately out of scope this batch (separate, larger sub-clusters of the
 * same USTGAPIXxx family, each confirmed a materially different shape from a
 * direct disassembly spot-check): `USTGAPIKLM` (15 methods, `CSTGHandle::Access()`-
 * based shared-memory table reads, not message sends), `USTGAPICDAudio` (12
 * methods), `USTGAPIMIDI` (23 methods, real device-queue I/O against 4 static
 * per-port `CSTGHandle`s + `CSTGMidiQueue`), `USTGAPIPCMBanks`/`USTGAPISampling`
 * (51/46 methods, by far the largest -- not spot-checked, presumed genuinely
 * deep sample-loading logic given their size).
 */

#ifndef USTG_API_WRAPPERS_H
#define USTG_API_WRAPPERS_H

#include "ustg_user_api.h"

/* Opaque scalar stand-ins for 3 real enum types (eSTGMsgPerfType/eSTGProgramBankId/
 * eSTGVoiceModelType) named in these methods' own real mangled signatures
 * (functions.csv/symbols.csv give the mangled type name only, not the enumerator
 * list). Every real call site passes them as a plain 4-byte int -- same "declared
 * opaque, byte-for-byte faithful, no enumerator names asserted" precedent already
 * used for eSTGMidiSource (stg_unsol_msg_handler.h/.cpp -- named only in comments,
 * never given a real C++ enum body).
 */
typedef unsigned int eSTGMsgPerfType;
typedef unsigned int eSTGProgramBankId;
typedef unsigned int eSTGVoiceModelType;

/* USTGAPICombi -- real type=2 STGMessages. .text+0x08e1b6f0..0x08e1bac0+0x40 (base
 * addresses below are each function's own entry). 8/9 real methods (the 9th,
 * UpdateToneAdjustParameter, IS covered -- earlier scope note in some drafts of
 * this pass undercounted it; see .cpp for its own distinct 8-payload-dword shape).
 */
class USTGAPICombi {
public:
	/* .text+0x08e1b6f0, 245 bytes. Sends {type=2,subcode=7,bankOrSlot,progId,
	 * progId,bankOrSlot,perfType} then polls ReadMessage() up to 8x for a
	 * subcode==7 echo. Real duplicate bankOrSlot/progId fields (base+0xc/+0x18
	 * and +0x10/+0x14) transcribed faithfully, not simplified.
	 */
	static bool SharedMemCombiDump(unsigned bankOrSlot, unsigned progId, eSTGMsgPerfType perfType);

	/* .text+0x08e1b810/0x08e1b880/0x08e1b8f0/0x08e1b9e0/0x08e1ba50, 0x64 bytes
	 * each -- byte-identical shape (type=2, 6 payload dwords in real order
	 * a1,a2,a4,a3,a5,perfType), only the subcode constant differs (0/1/2/4/5).
	 */
	static void UpdateCombiParameter(unsigned a1, unsigned a2, int a3, unsigned a4, int a5, eSTGMsgPerfType perfType);
	static void UpdateVectorMotionParameter(unsigned a1, unsigned a2, int a3, unsigned a4, int a5, eSTGMsgPerfType perfType);
	static void UpdateControllerInfoParameter(unsigned a1, unsigned a2, int a3, unsigned a4, int a5, eSTGMsgPerfType perfType);
	static void UpdateAudioInputParameter(unsigned a1, unsigned a2, int a3, unsigned a4, int a5, eSTGMsgPerfType perfType);
	static void UpdateEffectBalanceParameter(unsigned a1, unsigned a2, int a3, unsigned a4, int a5, eSTGMsgPerfType perfType);

	/* .text+0x08e1b960, 116 bytes. type=2, subcode=3, 8 payload dwords in real
	 * order a1,a2,a3,a4,a6,a5,a7,a8.
	 */
	static void UpdateToneAdjustParameter(unsigned a1, unsigned a2, unsigned a3, unsigned a4, int a5, unsigned a6, int a7, eSTGMsgPerfType a8);

	/* .text+0x08e1bac0, 92 bytes. type=2, subcode=6, 5 payload dwords: a
	 * constant 0, then a1/a2/a3, then a constant 2 -- transcribed exactly, the
	 * two constants are not simplified away.
	 */
	static void UpdateSequenceMetronomeParameter(unsigned a1, int a2, int a3);
};

/* USTGAPIEffect/USTGAPIEffectSlot -- byte-identical shape (type=9 vs type=8,
 * subcode=0), .text+0x08e1d2d0/0x08e1d3c0, 128 bytes each.
 */
class USTGAPIEffect {
public:
	static void UpdateParam(eSTGMsgPerfType perfType, unsigned short a2, unsigned short a3, unsigned short a4, unsigned short a5, short a6);
};

class USTGAPIEffectSlot {
public:
	static void UpdateParam(eSTGMsgPerfType perfType, unsigned short a2, unsigned short a3, unsigned short a4, unsigned short a5, short a6);
};

/* .text+0x08e1d350, 108 bytes. type=7, subcode=5. */
class USTGAPIEffectMgr {
public:
	static void UpdateEffectLFOParameter(eSTGMsgPerfType perfType, int a2, int a3, unsigned a4, unsigned a5, unsigned a6);
};

/* .text+0x08e1d590, 74 bytes. type=1, subcode=0. */
class USTGAPIGlobal {
public:
	static void UpdateGlobalParameter(unsigned a1, unsigned a2, int a3);
};

/* .text+0x08e1d5e0, 86 bytes. type=0xe, subcode=0. */
class USTGAPIHDRTrack {
public:
	static void UpdateHDRTrackParameter(unsigned a1, unsigned a2, unsigned a3, int a4);
};

/* .text+0x08e22fd0/0x08e23050, 116/116 bytes. Both type=3. */
class USTGAPIProgramSlot {
public:
	/* subcode=0, 8 payload dwords in real order a2,a3,a4,a5,a6,a8,a1,a7. */
	static void UpdateProgramSlotParameter(eSTGMsgPerfType a1, unsigned a2, unsigned a3, int a4, unsigned a5, unsigned a6, unsigned a7, int a8);

	/* subcode=2, 8 payload dwords: a2,a3,a4, two real constant-0 dwords, a5
	 * (bool, stored as a full dword), a1, a final constant-0 dword.
	 */
	static void UpdateProgramSlotEnabled(eSTGMsgPerfType a1, unsigned a2, unsigned a3, int a4, bool a5);
};

/* .text+0x08e24ad0, 80 bytes. type=0x10, subcode=0. */
class USTGAPISetList {
public:
	static void UpdateSlotParam(int a1, int a2, int a3, int a4);
};

/* .text+0x08e1ec60, 88 bytes. type=5, subcode=1. m_DefaultProgramId/
 * m_DefaultBankId (real static data members) not reconstructed -- see file header.
 */
class USTGAPIPatch {
public:
	static void UpdateOscSelectByType(eSTGProgramBankId a1, unsigned a2, int a3, eSTGVoiceModelType a4);
};

/* USTGAPIDrumkitData -- 2 of its 3 real methods (UpdateVSplitParam needs CValue,
 * deferred, see file header). Both type=1.
 */
class USTGAPIDrumkitData {
public:
	/* .text+0x08e1d0e0, 60 bytes. subcode=0xd. */
	static void SetCurrentKitId(unsigned a1);

	/* .text+0x08e1d1d0, 256 bytes. subcode=0xf. Sends then polls ReadMessage()
	 * up to 8x for a subcode==0xf echo, same shape as
	 * USTGAPICombi::SharedMemCombiDump.
	 */
	static bool SharedMemDrumKitDump(int a1);
};

/* USTGAPIWaveSequenceData -- 2 of its 3 real methods (UpdateStepParam needs
 * CValue, deferred). Both type=1.
 */
class USTGAPIWaveSequenceData {
public:
	/* .text+0x08e24d60, 60 bytes. subcode=0xe. */
	static void SetCurrentSequenceId(unsigned a1);

	/* .text+0x08e24e40, 256 bytes. subcode=0x10. Same poll-for-echo shape as
	 * SharedMemCombiDump/SharedMemDrumKitDump.
	 */
	static bool SharedMemWaveSequenceDump(int a1);
};

#endif /* USTG_API_WRAPPERS_H */
