// SPDX-License-Identifier: GPL-2.0
#ifndef OA_CONTROL_MSG_HANDLER_H
#define OA_CONTROL_MSG_HANDLER_H

/*
 * oa_control_msg_handler.h  -  CSTGControlMsgHandler: the front-panel/
 * remote "system control" message dispatch table -- I2C/CPU-usage/disk-
 * throughput diagnostics, LCD brightness, ErP (EuP standby-power)
 * simulated shutdown, USB port re-enable, rear LED, program-bank type,
 * setlist slot select, performance change, global mode change, keybed
 * comm takeover, and the karma-CC-mapped transport switches (play/stop/
 * rec/ff/rew/locate/pause).
 *
 * Ground truth: `CSTGControlMsgHandler::*`, OA_real.ko `.text+0xe7a50`..
 * `.text+0xe89b0` (nm addresses against `/home/share/Decomp/
 * OA.ko_Decomp/OA.ko`; add the project's documented `+0x10000` for the
 * Ghidra-export `oa_export/functions/` filenames, see docs/modules/
 * OA.ko.md's "Address mapping" section). Every body transcribed from a
 * raw `objdump -dr -M intel` disassembly (not decompile text) -- all 51
 * real methods are small (4-135 instructions), so full disassembly was
 * tractable for the whole cluster in one pass.
 *
 * Found while investigating `SetLCDBrightness` specifically (flagged by
 * a prior session, sec `oa_calibration_defaults_and_power_off_timer.md`)
 * -- the WHOLE class turned out to be a single cohesive, 100% previously
 * unclaimed unit (confirmed via `grep -rl CSTGControlMsgHandler` across
 * `reconstructed/OA/` turning up only doc-comment mentions, no prior
 * declaration), so it was reconstructed in full rather than just the one
 * flagged method.
 *
 * Not a real C++-virtual dispatch target in this project: same "install
 * vs dispatch" treatment as `CSTGCalibrationMsgHandler` (oa_calibration.h)
 * -- the real object installs a vtable pointer at construction, but
 * nothing in this project dispatches through it, so it's modeled as a
 * raw untyped pointer field, not a real `virtual` base (avoids the
 * established host/target vtable-corruption bug, sec 10.153/10.225).
 *
 * `sMsgHandler` (`.bss`, 54 entries, confirmed real via the constructor's
 * own 108-relocation table-populate sequence) is a DIFFERENT shape from
 * `CSTGCalibrationMsgHandler::sMsgHandler`: a flat array of raw function
 * pointers (NOT {fn,ctx} pairs -- there is no second word per entry).
 * 51 real methods + 3 slots (`+0x10`/`+0x14`/`+0x2c`) that install the
 * shared `CSTGMessageHandler::HandleUnsupportedMessage` fallback instead
 * of a `CSTGControlMsgHandler`-owned method. Installed with real function
 * pointer VALUES here (matching the same "install is safe, nothing
 * dispatches through it yet" rule) via a `memcpy`-based non-virtual
 * member-function-pointer type-pun (`AsRawFn`, see its own comment in
 * the .cpp) -- the GCC Itanium C++ ABI's pointer-to-member-function is
 * ALWAYS a 2-word `{ptr,adj}` struct regardless of virtuality, but for
 * a non-virtual method with no multiple/virtual-base adjustment (true
 * here, neither class has a REAL C++ `virtual` method of its own), the
 * leading word IS the plain function address, which is all `AsRawFn`
 * copies out.
 *
 * `eSTGMidiSource` modeled as plain `int` throughout, matching this
 * project's established convention for this not-independently-defined
 * enum (see oa_global.h's `SendUnsolicitedUIParam`/`SetRTKModeKnob`
 * comments) -- confirmed genuinely UNUSED by every one of these 51
 * methods (always loaded into ECX per the regparm(3) `this,arg1,arg2`
 * member-function ABI, but never read back by any real method body).
 */

#include "oa_global.h"
#include "oa_engine.h"
#include "oa_engine_init.h"	/* CSTGMidiDispatcher */
#include "oa_setup_global_resources.h"
#include "oa_keybed_init.h"

/*
 * Message-payload parameter shapes. Every one of these is read via a
 * plain `[edx]`/`[edx+4]`/`[edx+8]`/`[edx+0xc]` dword access in the real
 * disassembly (a handful of methods -- `SendKeybedByte`, `SetRearLEDState`,
 * `EnableAfterTouch`, `ResetOMAPModules`, `SetLCDBrightness`,
 * `ForceErPShutdown` -- read only the LOW BYTE of the first field via
 * `movzx eax, BYTE PTR [edx]`, still the same 4-byte field). The
 * "Control"-prefixed variants (`STGControlMsgDataOneParam` etc.) are
 * genuinely DIFFERENT real C++ types from the plain `STGMsgData*`
 * variants (confirmed via distinct Itanium mangled names at 4 different
 * call sites), even though their observed field shapes are identical --
 * kept as separate structs to preserve the real type distinction.
 */
struct STGMsgDataOneParam {
	unsigned int value;	/* +0x0 */
};

struct STGMsgDataTwoParam {
	unsigned int value0;	/* +0x0 */
	unsigned int value1;	/* +0x4 */
};

struct STGControlMsgDataOneParam {
	unsigned int value;	/* +0x0 */
};

/* Never actually dereferenced by `ReadI2CDevice` (its own body ignores
 * this parameter entirely -- see that method's own comment) -- shape
 * inferred from the "TwoParam" name only, not independently confirmed. */
struct STGControlMsgDataTwoParam {
	unsigned int value0;	/* +0x0 */
	unsigned int value1;	/* +0x4 */
};

/* Never actually dereferenced by `WriteI2CDevice` either (2-instruction
 * body, ignores both its own parameters) -- shape inferred from the
 * "ThreeParam" name only, not independently confirmed. */
struct STGControlMsgDataThreeParam {
	unsigned int value0;	/* +0x0 */
	unsigned int value1;	/* +0x4 */
	unsigned int value2;	/* +0x8 */
};

/* `SelectSetListSlotHandler`'s own param -- ground-truthed via its
 * confirmed forward to `CSTGGlobal::BeginSetListSlotChange`. */
struct STGControlMsgDataSetListSlotChange {
	unsigned int setNumber;	/* +0x0, bound-checked <= 0x7f */
	unsigned int slotNumber;	/* +0x4, bound-checked <= 0x7f */
	unsigned int source;	/* +0x8, eSTGPerformanceChangeSource, passed through */
};

/* `PerformanceChgHandler`'s own param -- ground-truthed via its
 * confirmed forward to `CSTGGlobal::BeginPerformanceChange`. Field
 * names match that function's own parameter names, NOT this struct's
 * own bound-check semantics (which differ per `type`, see the .cpp). */
struct STGControlMsgDataPerformanceChange {
	unsigned int type;	/* +0x0, eSTGPerformanceType: 0=program 1=combi 2=setlist(?) */
	unsigned int value1;	/* +0x4, bound-checked per type */
	unsigned int value2;	/* +0x8, bound-checked per type */
	unsigned int source;	/* +0xc, eSTGPerformanceChangeSource, passed through */
};

/* `SetModeHandler`'s own param -- ground-truthed via its confirmed
 * forward to `CSTGGlobal::SetMode`. */
struct STGControlMsgDataModeChange {
	unsigned int mode;	/* +0x0, eGlobalMode, bound-checked <= 2 */
	unsigned int source;	/* +0x4, eSTGPerformanceChangeSource, passed through */
};

/*
 * Generic "solicited reply" packets built on the stack and forwarded to
 * `PushMessage` -- two confirmed real shapes (16 and 20 bytes), same
 * `size/_pad2(uninitialized in ground truth)/reserved/subtype[/extra]/
 * value` convention as `CSTGCalibrationMsgHandler`'s own
 * `STGCalibrationReplyMsg` (oa_calibration.h), zeroed here for host-KAT
 * determinism on the genuinely-uninitialized `_pad2` field.
 */
struct STGControlReplyMsg {
	unsigned short size;      /* +0x0 */
	unsigned short _pad2;     /* +0x2, never written in ground truth */
	unsigned int reserved;    /* +0x4, always 0 in every observed call site */
	unsigned int subtype;     /* +0x8 */
	unsigned int value;       /* +0xc */
};

struct STGControlReplyMsgCPU {
	unsigned short size;      /* +0x0, always 0x14 */
	unsigned short _pad2;     /* +0x2, never written in ground truth */
	unsigned int reserved;    /* +0x4, always 0 */
	unsigned int subtype;     /* +0x8, always 1 */
	unsigned int coreId;      /* +0xc, echoes the request param */
	unsigned int value;       /* +0x10 */
};

extern "C" {
void PushMessage(void *msg) __attribute__((regparm(3)));
int OmapNKS4OutputFifo_WriteCommand(int command);
__attribute__((regparm(0))) void rt_printk(const char *fmt, ...);

/* `COmapNKS4Driver_StartScanning()` (drum-pad hardware scan trigger via
 * the OMAP NKS4 companion module) -- confirmed real via relocation from
 * `StartSTG`, deliberately deferred: no OmapNKS4Module.ko-side
 * reconstruction of this symbol exists in this project yet (matches the
 * already-established `COmapNKS4Driver_GetTestMode`/`_SetTestMode`
 * "confirmed real companion-module export, own body out of scope"
 * treatment, see power_off_timer.cpp/oa_global.h). */
void COmapNKS4Driver_StartScanning(void);
}

/*
 * `CSTGDrumPadInterface` -- referenced by `StartSTG` only. This project
 * has `CSTGDrumPadInterface_Initialize()`/`_Cleanup()` as real free
 * functions (oa_init.h, init_module Step 15, drumpad_init.cpp -- see
 * `CSTGDrumPadClient` below for the 2026-07-27 fix to their bodies);
 * `StartScanning()` is a genuinely different real method (confirmed via
 * relocation, `.text+0xaa90`-adjacent region) added here as its own
 * minimal declaration -- own body deliberately not reconstructed (real
 * drum-pad scan-trigger hardware protocol, out of scope for this
 * control-message dispatch cluster). Ground truth's real `sInstance` is
 * actually a 76-byte (0x4c) VALUE object (confirmed `nm`/`readelf -sW`:
 * `OBJECT GLOBAL 76 CSTGDrumPadInterface::sInstance`), not a pointer --
 * this class's pointer typing is a known, separate, already-repeatedly-
 * deferred mismatch (see `CSTGDrumPadClient`'s own comment below for how
 * its 3 real methods route around this rather than fix it this pass).
 */
class CSTGDrumPadInterface {
public:
	static CSTGDrumPadInterface *sInstance;
	void StartScanning();

	/* .text+0x0034ced0, 1 byte (round 67, solo). Confirmed genuinely
	 * empty (bare `ret`) in the real binary.
	 */
	static void Run();
};

/*
 * CSTGDrumPadClient -- the REAL, live drum-pad receive interface that
 * CSTGDrumPadInterface's Initialize()/Cleanup() register/unregister with
 * the external USB MIDI accessory driver. Ground truth: OA_real.ko
 * `.text._ZN17CSTGDrumPadClient*` (3 real methods, confirmed via raw
 * `objdump -dr`) + `.rodata._ZTV17CSTGDrumPadClient` (confirmed via
 * `.rel.rodata._ZTV17CSTGDrumPadClient`: 3 relocations at vtable
 * +0x8/+0xc/+0x10 -- CanReceiveTriggerEvent/ReceiveTriggerEvent/
 * ReceiveNotification; +0x0/+0x4, offset-to-top/RTTI, are literal 0, no
 * relocation -- confirmed -fno-rtti, no virtual destructor). `sizeof ==
 * 4`: this class has NO data members beyond its own vtable pointer --
 * every method operates entirely on 3 already-real global singletons
 * (a private CSTGDrumPadInterface::sInstance ring buffer,
 * CSTGGlobal::sInstance, CSTGMessageProcessor::sInstance), never on
 * `this` (confirmed: none of the 3 real bodies ever reads EAX).
 *
 * THIS IS THE EXACT GAP flagged during the 2026-07-27 literal-vs-
 * relocation sweep ("CSTGDrumPadInterface's constructor installs a
 * CSTGDrumPadClient vtable pointer that isn't modeled anywhere in this
 * project"). Ground truth's real singleton, `sDrumPadClient` (`.bss+
 * 0x26d38c`, immediately after CSTGDrumPadInterface::sInstance's own
 * 0x4c-byte footprint), gets its vtable pointer installed by
 * `_GLOBAL__I__ZN20CSTGDrumPadInterface9sInstanceE` -- and that install
 * is a SECOND, independently-found instance of this project's recurring
 * "objdump without -r hides a relocation as a plausible small immediate"
 * trap (see oa_audioinputmixer_vtable_literal8_bug_2026-07-27 for the
 * first 3 instances): the real instruction is `mov dword ptr
 * [0x26d38c], 0x8`, which LOOKS like a literal-8 store, but carries a
 * SECOND `R_386_32` relocation (in addition to the expected one on the
 * `.bss` destination operand) against `_ZTV17CSTGDrumPadClient` on the
 * immediate source operand -- the real stored value is
 * `&_ZTV17CSTGDrumPadClient + 8`, the standard Itanium-ABI vtable-ptr
 * convention already used throughout this project, not the integer 8.
 * This directly corrects init_module.cpp's own same-day claim that this
 * exact `.ctors` entry was "confirmed to write only values already
 * matching plain BSS zero-init, i.e. genuinely no-op" -- that check read
 * the raw immediate without checking for an attached relocation on it,
 * the same class of miss as the other 3 instances. See drumpad_init.cpp
 * for the real install (`ConstructDrumPadClient()`).
 *
 * A DIFFERENT, UNRELATED class -- `CUSBMidiAccessory_DrumPadClient`
 * (oa_engine.h) -- shares a similar name and lives in the same OA.ko
 * translation unit, but is confirmed DEAD CODE there:
 * `.rel.rodata._ZTV31CUSBMidiAccessory_DrumPadClient` has only 3
 * relocations (2 to `__cxa_pure_virtual` for its own
 * CanReceiveTriggerEvent/ReceiveTriggerEvent, 1 to its own no-op
 * ReceiveNotification), and a whole-binary `readelf -r` sweep confirms
 * ZERO relocations anywhere construct an object with that vtable -- it's
 * compiled into OA.ko only because a shared header got included, never
 * instantiated here (the real concrete instance, if any, would live in
 * USBMidiAccessory.ko, out of this project's scope). This corrects
 * oa_engine.h's own existing comment on that class ("the only concrete
 * override ... remains genuinely blocked") -- it isn't blocked, it's
 * simply inert/unused in this binary; left exactly as-is, not touched
 * this pass.
 *
 * CSTGDrumPadInterface::sInstance's own ring-buffer fields (write index
 * +0x40, read index +0x44, "armed" gate flag +0x48 -- all CONFIRMED via
 * this class's own real disassembly) are NOT added as real fields of the
 * existing (already out-of-scope, pointer-typed) `CSTGDrumPadInterface`
 * class above -- fixing THAT class's own representation is a separate,
 * larger, already-repeatedly-deferred task (this file's own existing
 * comment: "this project does not model CSTGDrumPadInterface as a class
 * at all"). Instead, the 3 known fields these 3 methods actually touch
 * get their own small, dedicated raw storage (`gDrumPadInterfaceRing`,
 * drumpad_init.cpp) at the confirmed real offsets -- logically the SAME
 * memory ground truth's ring buffer occupies (the struct's own `sizeof`
 * comes out to exactly 0x4c, matching ground truth's real object size,
 * a nice independent confirmation), just not yet unified with
 * `CSTGDrumPadInterface::sInstance`'s own (currently wrong) pointer
 * typing. A future session that fixes that representation only needs to
 * repoint this storage at the real singleton object.
 *
 * regparm(3): `this` in EAX (confirmed real, but genuinely UNUSED by all
 * 3 bodies). `STGDrumPadTriggerEvent` is a 16-bit type (confirmed: EDX's
 * DL/DH split into 2 consecutive ring-buffer bytes, low byte first).
 * `eNotificiation` reuses `CUSBMidiAccessory_DrumPadClient::eNotificiation`
 * (already modeled `int` in oa_engine.h) -- the real mangled parameter
 * type is literally that same nested typedef, confirmed via the real
 * mangled symbol name.
 *
 * NOT modeled with real C++ `virtual` (this project's established
 * host/target vtable-corruption caution, sec 10.153/10.225) -- the
 * vtable pointer is a plain hand-installed field, matching every other
 * "install a real vtable, dispatch through it only where a real live
 * caller needs it" class in this project. Nothing in OA.ko itself
 * dispatches through `sDrumPadClient` (the external USBMidiAccessory.ko
 * driver does, out of scope) -- so the 3 methods below are ordinary,
 * directly-callable member functions, with the vtable's own slot values
 * installed purely for byte-faithful in-memory representation.
 */
class CSTGDrumPadClient {
public:
	void *_vtablePtr;	/* +0x0 -- real: `_ZTV17CSTGDrumPadClient + 8` */

	int CanReceiveTriggerEvent();
	void ReceiveTriggerEvent(unsigned short event);
	void ReceiveNotification(CUSBMidiAccessory_DrumPadClient::eNotificiation n);
};

/*
 * `CSTGMessageHandler::HandleUnsupportedMessage(void*, eSTGMidiSource)`
 * -- the shared base-class fallback handler `CSTGControlMsgHandler`'s
 * own constructor installs into 3 of its 54 dispatch slots (`+0x10`/
 * `+0x14`/`+0x2c`) for message types this subclass doesn't implement.
 * Confirmed real via relocation only -- own body not reconstructed
 * (generic infrastructure shared by every `*MsgHandler` subclass in
 * OA.ko, matching this project's existing "confirmed real, deliberately
 * deferred" treatment for shared base-class plumbing). Given an empty
 * body purely so it exists as a real, linkable symbol for the
 * `sMsgHandler` table to install a genuine pointer to (install-only,
 * never dispatched through in this project, same rule as everywhere
 * else in this cluster).
 */
class CSTGMessageHandler {
public:
	void HandleUnsupportedMessage(void *msg, int source);
};

class CSTGControlMsgHandler {
public:
	static CSTGControlMsgHandler *sInstance;
	static void *sMsgHandler[54];	/* .bss, real 54-entry raw fn-ptr table */

	void *_vtablePtr;		/* +0x0, install-only placeholder, see header comment */
	void *_msgHandlerTable;	/* +0x4, always &sMsgHandler */
	unsigned char _replyTag;	/* +0x8, default 0x36 */

	CSTGControlMsgHandler();

	void SetModeHandler(const STGControlMsgDataModeChange *param, int source);
	void PerformanceChgHandler(const STGControlMsgDataPerformanceChange *param, int source);
	void StartDownloadHandler(const void *param, int source);
	void EndDownloadHandler(const void *param, int source);
	void SliderCC18EnableHandler(const STGMsgDataOneParam *param, int source);
	void ProgramChangeEnableHandler(const STGMsgDataOneParam *param, int source);
	/* Own body deliberately NOT reconstructed -- a genuine effect-rack
	 * DSP walk (16 effect slots, `CEffectSlotBase::UpdateEffectType`/
	 * `CSTGEffectRack::AccessEffectSlot`/`CEffectSlot::
	 * PrepareEffectMessageContext`/`CSTGPerformance::IsCurrentlyActive`,
	 * none of which are reconstructed elsewhere in this project either),
	 * out of scope per this project's audio-DSP-fidelity policy (sec
	 * 10.185/`oa_ko_rtai_virtualization_policy.md`). `.text+0xe8250`,
	 * 416 bytes, confirmed real via full disassembly (not reproduced
	 * here). Empty body given purely so the real symbol exists for
	 * `sMsgHandler` to install a genuine pointer to. */
	void ResetAllEffectsInActivePerf(const void *param, int source);
	void NKS4TestModeEnableHandler(const STGMsgDataOneParam *param, int source);
	void SetProgramBankTypeHandler(const STGMsgDataTwoParam *param, int source);
	void StartSTG(const void *param, int source);
	void SysExFilerModeEnableHandler(const STGMsgDataOneParam *param, int source);
	void MuteDAC(const STGMsgDataOneParam *param, int source);
	void MuteADC(const STGMsgDataOneParam *param, int source);
	void EnterKeyCheckMode(const STGMsgDataOneParam *param, int source);
	void SendKeybedByte(const STGMsgDataOneParam *param, int source);
	void TakeOverKeybedComm(const STGMsgDataOneParam *param, int source);
	/* Own body ignores `param` entirely and always returns 4 (real
	 * hardware quirk: the debug I2C-read command is neutered on shipping
	 * firmware -- reproduced faithfully). */
	void ReadI2CDevice(const STGControlMsgDataTwoParam *param, int source);
	/* 2-instruction body: ignores BOTH parameters, always returns 4.
	 * Same "debug I2C command neutered on shipping firmware" quirk as
	 * ReadI2CDevice. */
	void WriteI2CDevice(const STGControlMsgDataThreeParam *param, int source);
	void ReadCPUUsagePeak(const STGControlMsgDataOneParam *param, int source);
	void ReadDiskThroughputPeak(const void *param, int source);
	void ReadFXUsagePeak(const void *param, int source);
	void SelectSetListSlotHandler(const STGControlMsgDataSetListSlotChange *param, int source);
	void EnableOnScreenTouchPads(const STGMsgDataOneParam *param, int source);
	void SetLCDBrightness(const STGMsgDataOneParam *param, int source);
	void ResetOMAPModules(const STGMsgDataOneParam *param, int source);
	void UseGlobalAudioInputSettings(const STGMsgDataOneParam *param, int source);
	void StealAllVoices(const void *param, int source);
	void SetChordAssignState(const STGMsgDataOneParam *param, int source);
	void SetSendingBulkDump(const STGMsgDataOneParam *param, int source);
	void EnableSendingMidiParams(const STGMsgDataOneParam *param, int source);
	void EnableReceivingMidiParams(const STGMsgDataOneParam *param, int source);
	void UpdateErPTimeout(const STGMsgDataOneParam *param, int source);
	void ErPNotifySystemActivity(const STGMsgDataOneParam *param, int source);
	void BeginLongErPActivity(const STGMsgDataOneParam *param, int source);
	void EndLongErPActivity(const STGMsgDataOneParam *param, int source);
	void ReadyForErPShutdown(const STGMsgDataOneParam *param, int source);
	/* Real diagnostic string, confirmed via `.rodata.str1.4+0x520`:
	 * "We would have powered off here!!!\n" -- a simulated/logged ErP
	 * (EuP standby-power) shutdown, NOT a real power-off. */
	void ForceErPShutdown(const STGMsgDataOneParam *param, int source);
	void SetEditInContextState(const STGMsgDataTwoParam *param, int source);
	void SetSplitLayerWorkState(const STGMsgDataOneParam *param, int source);
	void SetRearLEDState(const STGMsgDataOneParam *param, int source);
	void EnableAudioMetering(const STGMsgDataOneParam *param, int source);
	void EnableReceivingMidi(const STGMsgDataOneParam *param, int source);
	void ReenableUSBPorts(const STGMsgDataOneParam *param, int source);
	void DeactivateEncoderAccelerator(const STGMsgDataOneParam *param, int source);
	void SetPlayStopSwitch(const STGMsgDataOneParam *param, int source);
	void SetRecSwitch(const STGMsgDataOneParam *param, int source);
	void SetLocateSwitch(const STGMsgDataOneParam *param, int source);
	void SetRewSwitch(const STGMsgDataOneParam *param, int source);
	void SetFFSwitch(const STGMsgDataOneParam *param, int source);
	void SetPauseSwitch(const STGMsgDataOneParam *param, int source);
	void EnableAfterTouch(const STGMsgDataOneParam *param, int source);
};

#endif /* OA_CONTROL_MSG_HANDLER_H */
