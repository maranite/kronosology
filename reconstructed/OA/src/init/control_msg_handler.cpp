// SPDX-License-Identifier: GPL-2.0
/*
 * control_msg_handler.cpp  -  CSTGControlMsgHandler, the front-panel/
 * remote "system control" message dispatch table.
 *
 * See include/oa_control_msg_handler.h for the full ground-truth
 * provenance, message-shape derivation, and the "install vs dispatch"
 * vtable note.
 *
 * All 51 real methods were disassembled in one pass (`objdump -dr -M
 * intel` against /home/share/Decomp/OA.ko_Decomp/OA.ko, `.text+0xe7a50`
 * through `.text+0xe89b0`) -- the whole cluster is small (4-135
 * instructions per method) and shares a handful of recurring shapes:
 *
 *   1. Bool-toggle: `field = (param->value != 0)` on some already-real
 *      singleton (CSTGGlobal/CSTGMidiPortManager/CSTGMidiDispatcher/
 *      CSTGMessageProcessor/CSTGControllerRTData/CSTGFrontPanel), often
 *      then forwarded to an already-real setter method.
 *   2. OMAP NKS4 hardware command: pack `param`'s low byte (or nothing)
 *      into a 32-bit command word with a per-method opcode in the top
 *      byte, forward to the already-real `OmapNKS4OutputFifo_
 *      WriteCommand()`. Byte position of the payload within the word
 *      (top-byte-shifted vs bottom-byte-unshifted) varies per real
 *      opcode -- reproduced exactly per method, not unified.
 *   3. "Solicited reply": build a small stack packet (`STGControlReplyMsg`/
 *      `STGControlReplyMsgCPU`) and forward to the already-real
 *      `PushMessage()`.
 *   4. Bound-checked forward to a CSTGGlobal performance/mode-change
 *      producer (`BeginPerformanceChange`/`BeginSetListSlotChange`/
 *      `SetMode`) -- ALL THREE were already fully real in this project
 *      before this cluster (oa_global.h), so these 3 handlers are pure
 *      "unpack param, validate bounds, forward" glue.
 */

#include "oa_control_msg_handler.h"

/* Non-virtual member-function-pointer -> raw void* type pun. Under the
 * GCC Itanium C++ ABI, a pointer-to-member-function is ALWAYS a 2-word
 * `{ptrdiff_t ptr; ptrdiff_t adj;}` struct regardless of virtuality
 * (confirmed the hard way: an earlier version of this code assumed a
 * single-word representation and failed `static_assert` on the real
 * 32-bit `-mregparm=3` kernel build) -- but for a NON-virtual member
 * function with no multiple/virtual-base adjustment (true for both
 * CSTGControlMsgHandler and CSTGMessageHandler here), the leading `ptr`
 * word IS exactly the plain function address, with `adj` always 0. Only
 * that leading word is copied out. Install-only: nothing in this
 * project dispatches through sMsgHandler yet. */
template <typename T>
static void *AsRawFn(T memberFnPtr)
{
	void *raw;
	__builtin_memcpy(&raw, &memberFnPtr, sizeof(raw));
	return raw;
}

CSTGControlMsgHandler *CSTGControlMsgHandler::sInstance;
void *CSTGControlMsgHandler::sMsgHandler[54];

/*
 * Real vtable data (24 bytes / 4 slots, confirmed via `readelf -sW`
 * against ground truth's own `_ZTV21CSTGControlMsgHandler`).
 * Zero-filled placeholder, matching this project's established
 * "install vs dispatch" rule -- nothing in this project dispatches
 * through it.
 *
 * FIXED (2026-07-27): the ctor previously stored a bare literal `0`
 * here instead of this real symbol. Ground truth's own ctor
 * (`.text+0xe8550`) does `mov DWORD PTR [eax],0x8` with a real
 * `R_386_32 _ZTV21CSTGControlMsgHandler` relocation -- confirmed via
 * `objdump -dr` against OA.ko_Decomp/OA.ko -- so a bare `0` was a
 * genuine value mismatch ("install vs dispatch" means nothing reads it
 * back through a vtable slot, not that the field itself should be left
 * null).
 */
extern "C" unsigned char _ZTV21CSTGControlMsgHandler[24] = { 0 };
CSTGDrumPadInterface *CSTGDrumPadInterface::sInstance;

/* CSTGMessageHandler::HandleUnsupportedMessage -- shared base-class
 * fallback, own body deliberately not reconstructed (see header). */
void CSTGMessageHandler::HandleUnsupportedMessage(void *, int) { }

/* CSTGDrumPadInterface::StartScanning -- own body deliberately not
 * reconstructed, real drum-pad scan-trigger hardware protocol is out of
 * scope for this cluster (see header). */
void CSTGDrumPadInterface::StartScanning() { }

/* Round 67 (solo): CONFIRMED genuinely empty (`return;`, 1 byte) in the
 * real binary -- unlike StartScanning() above, this is not an out-of-
 * scope placeholder.
 */
void CSTGDrumPadInterface::Run() { }

/* ------------------------------------------------------------------ */
/* Constructor -- installs the vtable pointer (unused, see header), the */
/* msgHandler table pointer, default reply tag, and the 54 real         */
/* dispatch entries (51 real methods + 3 shared fallback slots).        */
/* ------------------------------------------------------------------ */
CSTGControlMsgHandler::CSTGControlMsgHandler()
{
	_vtablePtr = _ZTV21CSTGControlMsgHandler + 8;	/* real value, install-only, see header note */
	_msgHandlerTable = &sMsgHandler;
	_replyTag = 0x36;
	sInstance = this;

	sMsgHandler[0x00 / 4] = AsRawFn(&CSTGControlMsgHandler::SetModeHandler);
	sMsgHandler[0x04 / 4] = AsRawFn(&CSTGControlMsgHandler::PerformanceChgHandler);
	sMsgHandler[0x08 / 4] = AsRawFn(&CSTGControlMsgHandler::StartDownloadHandler);
	sMsgHandler[0x0c / 4] = AsRawFn(&CSTGControlMsgHandler::EndDownloadHandler);
	sMsgHandler[0x10 / 4] = AsRawFn(&CSTGMessageHandler::HandleUnsupportedMessage);
	sMsgHandler[0x14 / 4] = AsRawFn(&CSTGMessageHandler::HandleUnsupportedMessage);
	sMsgHandler[0x18 / 4] = AsRawFn(&CSTGControlMsgHandler::SliderCC18EnableHandler);
	sMsgHandler[0x1c / 4] = AsRawFn(&CSTGControlMsgHandler::ProgramChangeEnableHandler);
	sMsgHandler[0x20 / 4] = AsRawFn(&CSTGControlMsgHandler::ResetAllEffectsInActivePerf);
	sMsgHandler[0x24 / 4] = AsRawFn(&CSTGControlMsgHandler::NKS4TestModeEnableHandler);
	sMsgHandler[0x28 / 4] = AsRawFn(&CSTGControlMsgHandler::SetProgramBankTypeHandler);
	sMsgHandler[0x2c / 4] = AsRawFn(&CSTGMessageHandler::HandleUnsupportedMessage);
	sMsgHandler[0x30 / 4] = AsRawFn(&CSTGControlMsgHandler::StartSTG);
	sMsgHandler[0x34 / 4] = AsRawFn(&CSTGControlMsgHandler::SysExFilerModeEnableHandler);
	sMsgHandler[0x38 / 4] = AsRawFn(&CSTGControlMsgHandler::MuteDAC);
	sMsgHandler[0x3c / 4] = AsRawFn(&CSTGControlMsgHandler::MuteADC);
	sMsgHandler[0x40 / 4] = AsRawFn(&CSTGControlMsgHandler::EnterKeyCheckMode);
	sMsgHandler[0x44 / 4] = AsRawFn(&CSTGControlMsgHandler::SendKeybedByte);
	sMsgHandler[0x48 / 4] = AsRawFn(&CSTGControlMsgHandler::TakeOverKeybedComm);
	sMsgHandler[0x4c / 4] = AsRawFn(&CSTGControlMsgHandler::ReadI2CDevice);
	sMsgHandler[0x50 / 4] = AsRawFn(&CSTGControlMsgHandler::WriteI2CDevice);
	sMsgHandler[0x54 / 4] = AsRawFn(&CSTGControlMsgHandler::ReadCPUUsagePeak);
	sMsgHandler[0x58 / 4] = AsRawFn(&CSTGControlMsgHandler::ReadDiskThroughputPeak);
	sMsgHandler[0x5c / 4] = AsRawFn(&CSTGControlMsgHandler::ReadFXUsagePeak);
	sMsgHandler[0x60 / 4] = AsRawFn(&CSTGControlMsgHandler::SelectSetListSlotHandler);
	sMsgHandler[0x64 / 4] = AsRawFn(&CSTGControlMsgHandler::EnableOnScreenTouchPads);
	sMsgHandler[0x68 / 4] = AsRawFn(&CSTGControlMsgHandler::SetLCDBrightness);
	sMsgHandler[0x6c / 4] = AsRawFn(&CSTGControlMsgHandler::ResetOMAPModules);
	sMsgHandler[0x70 / 4] = AsRawFn(&CSTGControlMsgHandler::UseGlobalAudioInputSettings);
	sMsgHandler[0x74 / 4] = AsRawFn(&CSTGControlMsgHandler::StealAllVoices);
	sMsgHandler[0x78 / 4] = AsRawFn(&CSTGControlMsgHandler::SetChordAssignState);
	sMsgHandler[0x7c / 4] = AsRawFn(&CSTGControlMsgHandler::SetSendingBulkDump);
	sMsgHandler[0x80 / 4] = AsRawFn(&CSTGControlMsgHandler::EnableSendingMidiParams);
	sMsgHandler[0x84 / 4] = AsRawFn(&CSTGControlMsgHandler::EnableReceivingMidiParams);
	sMsgHandler[0x88 / 4] = AsRawFn(&CSTGControlMsgHandler::UpdateErPTimeout);
	sMsgHandler[0x8c / 4] = AsRawFn(&CSTGControlMsgHandler::ErPNotifySystemActivity);
	sMsgHandler[0x90 / 4] = AsRawFn(&CSTGControlMsgHandler::BeginLongErPActivity);
	sMsgHandler[0x94 / 4] = AsRawFn(&CSTGControlMsgHandler::EndLongErPActivity);
	sMsgHandler[0x98 / 4] = AsRawFn(&CSTGControlMsgHandler::ReadyForErPShutdown);
	sMsgHandler[0x9c / 4] = AsRawFn(&CSTGControlMsgHandler::ForceErPShutdown);
	sMsgHandler[0xa0 / 4] = AsRawFn(&CSTGControlMsgHandler::SetEditInContextState);
	sMsgHandler[0xa4 / 4] = AsRawFn(&CSTGControlMsgHandler::SetSplitLayerWorkState);
	sMsgHandler[0xa8 / 4] = AsRawFn(&CSTGControlMsgHandler::SetRearLEDState);
	sMsgHandler[0xac / 4] = AsRawFn(&CSTGControlMsgHandler::EnableAudioMetering);
	sMsgHandler[0xb0 / 4] = AsRawFn(&CSTGControlMsgHandler::EnableReceivingMidi);
	sMsgHandler[0xb4 / 4] = AsRawFn(&CSTGControlMsgHandler::ReenableUSBPorts);
	sMsgHandler[0xb8 / 4] = AsRawFn(&CSTGControlMsgHandler::DeactivateEncoderAccelerator);
	sMsgHandler[0xbc / 4] = AsRawFn(&CSTGControlMsgHandler::SetPlayStopSwitch);
	sMsgHandler[0xc0 / 4] = AsRawFn(&CSTGControlMsgHandler::SetRecSwitch);
	sMsgHandler[0xc4 / 4] = AsRawFn(&CSTGControlMsgHandler::SetLocateSwitch);
	sMsgHandler[0xc8 / 4] = AsRawFn(&CSTGControlMsgHandler::SetRewSwitch);
	sMsgHandler[0xcc / 4] = AsRawFn(&CSTGControlMsgHandler::SetFFSwitch);
	sMsgHandler[0xd0 / 4] = AsRawFn(&CSTGControlMsgHandler::SetPauseSwitch);
	sMsgHandler[0xd4 / 4] = AsRawFn(&CSTGControlMsgHandler::EnableAfterTouch);
}

CSTGControlMsgHandler::~CSTGControlMsgHandler()
{
	/* volatile: a plain store here is otherwise dead-store-eliminated by
	 * GCC at -O2 (the object's lifetime ends when the dtor returns, so
	 * the optimizer assumes no one can observe this write) -- same fix
	 * as CSTGStreamingEvent::~CSTGStreamingEvent() (round 67). */
	*(void * volatile *)&_vtablePtr = _ZTV18CSTGMessageHandler + 8;
}

/* ------------------------------------------------------------------ */
/* Performance/mode-change producers (shape 4) -- pure "unpack param,  */
/* validate bounds, forward" glue onto already-real CSTGGlobal methods. */
/* ------------------------------------------------------------------ */

void CSTGControlMsgHandler::SetModeHandler(const STGControlMsgDataModeChange *param, int)
{
	if (param->mode <= 2)
		CSTGGlobal::sInstance->SetMode((int)param->mode, param->source);
	/* real: eax=0 on success, eax=6 (uninitialized return value in this
	 * project's void-returning model) if mode>2 -- return value not
	 * modeled since this method (like every other slot) is `void` here,
	 * matching this project's established convention for dispatch-table
	 * entries whose EAX return is never read by any reconstructed
	 * caller. */
}

void CSTGControlMsgHandler::PerformanceChgHandler(const STGControlMsgDataPerformanceChange *param, int)
{
	/* Bound checks are genuinely DIFFERENT per `type` (confirmed via
	 * full disassembly, not simplified): */
	bool valid;
	switch (param->type) {
	case 0:	/* program */
		valid = param->value2 <= 0x7f && param->value1 <= 0xd;
		break;
	case 1:	/* combi */
		valid = (param->value2 <= 0x7f && param->value1 <= 0x1e) ||
			(param->value2 == 0xfffe && param->value1 == 0);
		break;
	case 2:	/* setlist(?) */
		valid = param->value2 <= 0xc7 && param->value1 == 0;
		break;
	default:
		valid = false;
		break;
	}
	if (valid)
		CSTGGlobal::sInstance->BeginPerformanceChange((int)param->type, param->value1,
								param->value2, param->source);
}

void CSTGControlMsgHandler::SelectSetListSlotHandler(const STGControlMsgDataSetListSlotChange *param, int)
{
	if (param->setNumber <= 0x7f && param->slotNumber <= 0x7f)
		CSTGGlobal::sInstance->BeginSetListSlotChange(param->setNumber, param->slotNumber,
								param->source);
}

/* ------------------------------------------------------------------ */
/* Bool-toggle handlers onto already-real singletons/setters.          */
/* ------------------------------------------------------------------ */

void CSTGControlMsgHandler::SliderCC18EnableHandler(const STGMsgDataOneParam *param, int)
{
	/* Set/clear bit 0 of CSTGControllerRTData::sInstance+0x49,
	 * preserving the other 7 bits -- confirmed via the real
	 * `and eax,0xfffffffe` / `or eax,edx` pair. */
	unsigned char *field = (unsigned char *)CSTGControllerRTData::sInstance + 0x49;
	*field = (*field & 0xfe) | (param->value != 0);
}

void CSTGControlMsgHandler::ProgramChangeEnableHandler(const STGMsgDataOneParam *param, int)
{
	*((unsigned char *)CSTGMidiDispatcher::sInstance + 0xa2) = (param->value != 0);
}

void CSTGControlMsgHandler::ResetAllEffectsInActivePerf(const void *, int)
{
	/* Deliberately not reconstructed -- see header comment. */
}

void CSTGControlMsgHandler::NKS4TestModeEnableHandler(const STGMsgDataOneParam *param, int)
{
	CSTGGlobal::sInstance->SetNKS4TestModeFlag(param->value != 0);
}

void CSTGControlMsgHandler::SetProgramBankTypeHandler(const STGMsgDataTwoParam *param, int)
{
	/* Same `CSTGGlobal+0x132e4d0+bankId*0x67603` array arithmetic as
	 * InitializePerformances()/global_ctor.cpp -- confirmed via the
	 * real `imul eax,[edx],0x67603; add eax,0x132e4d0; add eax,
	 * CSTGGlobal::sInstance` sequence. */
	unsigned char *global = (unsigned char *)CSTGGlobal::sInstance;
	unsigned int bankId = param->value0;
	CSTGProgramBank *bank = (CSTGProgramBank *)(global + 0x132e4d0u + bankId * 0x67603u);
	bank->ChangeBankType(param->value1);
}

void CSTGControlMsgHandler::SysExFilerModeEnableHandler(const STGMsgDataOneParam *param, int)
{
	*((unsigned char *)CSTGMidiPortManager::sInstance + 0x2) = (param->value != 0);
}

void CSTGControlMsgHandler::UseGlobalAudioInputSettings(const STGMsgDataOneParam *param, int)
{
	CSTGGlobal::sInstance->SetUseGlobalAudioInputSettings(param->value != 0);
}

void CSTGControlMsgHandler::SetChordAssignState(const STGMsgDataOneParam *param, int)
{
	CSTGControllerRTData::sInstance->SendKarmaCCToKG(0x02, (param->value != 0) ? 0x7f : 0x00);
}

void CSTGControlMsgHandler::EnableSendingMidiParams(const STGMsgDataOneParam *param, int)
{
	*((unsigned char *)CSTGMessageProcessor::sInstance + 0x56) = (param->value != 0);
}

void CSTGControlMsgHandler::EnableReceivingMidiParams(const STGMsgDataOneParam *param, int)
{
	*((unsigned char *)CSTGMessageProcessor::sInstance + 0x57) = (param->value != 0);
}

void CSTGControlMsgHandler::SetEditInContextState(const STGMsgDataTwoParam *param, int)
{
	CSTGGlobal::sInstance->SetEditInContextState((int)param->value0, param->value1);
}

void CSTGControlMsgHandler::SetSplitLayerWorkState(const STGMsgDataOneParam *param, int)
{
	CSTGGlobal::sInstance->SetSplitLayerWorkState(param->value != 0);
}

void CSTGControlMsgHandler::EnableAudioMetering(const STGMsgDataOneParam *param, int)
{
	/* Cross-confirmed: CSTGGlobal's own constructor (global_ctor.cpp)
	 * defaults this SAME byte to 1 ("audio metering enabled" by
	 * default), this handler just toggles it. */
	*((unsigned char *)CSTGGlobal::sInstance + 0x6db) = (param->value != 0);
}

void CSTGControlMsgHandler::EnableReceivingMidi(const STGMsgDataOneParam *param, int)
{
	*((unsigned char *)CSTGMidiPortManager::sInstance + 0x0) = (param->value != 0);
}

void CSTGControlMsgHandler::EnableOnScreenTouchPads(const STGMsgDataOneParam *param, int)
{
	*((unsigned char *)CSTGFrontPanel::sInstance + 0x104) = (param->value != 0);
}

/* ------------------------------------------------------------------ */
/* Karma-CC-mapped transport switches -- all six share ONE shape:      */
/* `SendKarmaCCToKG(ccNo, param->value ? 0x7f : 0)` on the real         */
/* CSTGControllerRTData singleton (already a confirmed real, deferred   */
/* extern in this project -- see oa_global.h).                          */
/* ------------------------------------------------------------------ */

void CSTGControlMsgHandler::SetPauseSwitch(const STGMsgDataOneParam *param, int)
{
	CSTGControllerRTData::sInstance->SendKarmaCCToKG(0x2d, (param->value != 0) ? 0x7f : 0x00);
}

void CSTGControlMsgHandler::SetFFSwitch(const STGMsgDataOneParam *param, int)
{
	CSTGControllerRTData::sInstance->SendKarmaCCToKG(0x25, (param->value != 0) ? 0x7f : 0x00);
}

void CSTGControlMsgHandler::SetRewSwitch(const STGMsgDataOneParam *param, int)
{
	CSTGControllerRTData::sInstance->SendKarmaCCToKG(0x26, (param->value != 0) ? 0x7f : 0x00);
}

void CSTGControlMsgHandler::SetLocateSwitch(const STGMsgDataOneParam *param, int)
{
	CSTGControllerRTData::sInstance->SendKarmaCCToKG(0x2c, (param->value != 0) ? 0x7f : 0x00);
}

void CSTGControlMsgHandler::SetRecSwitch(const STGMsgDataOneParam *param, int)
{
	CSTGControllerRTData::sInstance->SendKarmaCCToKG(0x2b, (param->value != 0) ? 0x7f : 0x00);
}

void CSTGControlMsgHandler::SetPlayStopSwitch(const STGMsgDataOneParam *param, int)
{
	CSTGControllerRTData::sInstance->SendKarmaCCToKG(0x2a, (param->value != 0) ? 0x7f : 0x00);
}

/* ------------------------------------------------------------------ */
/* OMAP NKS4 hardware command handlers (shape 2).                      */
/* ------------------------------------------------------------------ */

void CSTGControlMsgHandler::SetLCDBrightness(const STGMsgDataOneParam *param, int)
{
	unsigned int value = param->value & 0xff;
	OmapNKS4OutputFifo_WriteCommand((int)((value << 16) | 0xc7000000u));
}

void CSTGControlMsgHandler::ResetOMAPModules(const STGMsgDataOneParam *param, int)
{
	unsigned int value = param->value & 0xff;
	OmapNKS4OutputFifo_WriteCommand((int)((value << 16) | 0x06000000u));
}

void CSTGControlMsgHandler::DeactivateEncoderAccelerator(const STGMsgDataOneParam *, int)
{
	/* Ignores its own param entirely -- confirmed real, a fixed
	 * opcode-only command. */
	OmapNKS4OutputFifo_WriteCommand((int)0x0f000000u);
}

void CSTGControlMsgHandler::EnableAfterTouch(const STGMsgDataOneParam *param, int)
{
	/* Real quirk: payload byte lands in the LOW byte here, NOT shifted
	 * into the middle byte like SetLCDBrightness/ResetOMAPModules/
	 * SetRearLEDState above -- confirmed via the real disassembly
	 * (`or eax,0x10000000` with no `shl`), reproduced faithfully. */
	unsigned int value = param->value & 0xff;
	OmapNKS4OutputFifo_WriteCommand((int)(value | 0x10000000u));
}

void CSTGControlMsgHandler::ForceErPShutdown(const STGMsgDataOneParam *param, int)
{
	unsigned int value = param->value & 0xffff;
	OmapNKS4OutputFifo_WriteCommand((int)(value | 0x09000000u));
	/* Real diagnostic string, `.rodata.str1.4+0x520` -- a simulated/
	 * logged ErP (EuP standby-power) shutdown, NOT a real power-off. */
	rt_printk("We would have powered off here!!!\n");
}

/* ------------------------------------------------------------------ */
/* ErP (EuP standby-power) long-process / activity bookkeeping onto    */
/* the already-real CPowerOffTimer singleton (oa_engine.h).            */
/* ------------------------------------------------------------------ */

void CSTGControlMsgHandler::ErPNotifySystemActivity(const STGMsgDataOneParam *, int)
{
	*(unsigned char *)CPowerOffTimer::sInstance = 1;
}

void CSTGControlMsgHandler::UpdateErPTimeout(const STGMsgDataOneParam *param, int)
{
	CPowerOffTimer::sInstance->UpdateTimeoutValue(param->value);
}

void CSTGControlMsgHandler::ReadyForErPShutdown(const STGMsgDataOneParam *, int)
{
	CPowerOffTimer::sInstance->PowerOffPrepComplete();
}

void CSTGControlMsgHandler::BeginLongErPActivity(const STGMsgDataOneParam *, int)
{
	CPowerOffTimer::sInstance->BeginLongProcess();
}

void CSTGControlMsgHandler::EndLongErPActivity(const STGMsgDataOneParam *, int)
{
	CPowerOffTimer::sInstance->EndLongProcess();
}

void CSTGControlMsgHandler::SetSendingBulkDump(const STGMsgDataOneParam *param, int)
{
	bool sending = (param->value != 0);
	*((unsigned char *)CSTGMidiPortManager::sInstance + 0x3) = sending;
	if (sending)
		CPowerOffTimer::sInstance->BeginLongProcess();
	else
		CPowerOffTimer::sInstance->EndLongProcess();
}

/* ------------------------------------------------------------------ */
/* Keybed hardware handlers onto already-real free C-linkage functions */
/* (oa_keybed_init.h -- this project's established "member methods as  */
/* free functions taking the sInstance blob directly" convention).     */
/* ------------------------------------------------------------------ */

void CSTGControlMsgHandler::TakeOverKeybedComm(const STGMsgDataOneParam *param, int)
{
	unsigned char *blob = CSTGKeybedInterface_sInstance();
	blob[KEYBED_OFF_ENQUEUE_GATE1] = (param->value != 0);
}

void CSTGControlMsgHandler::SendKeybedByte(const STGMsgDataOneParam *param, int)
{
	CSTGKeybedInterface_SendByte((unsigned char)param->value);
}

void CSTGControlMsgHandler::EnterKeyCheckMode(const STGMsgDataOneParam *param, int)
{
	/* Real quirk: param!=0 -> ENTER, param==0 -> EXIT (confirmed via
	 * the real `test/jne` branch order). */
	if (param->value != 0)
		CSTGKeybedInterface_EnterKeyCheckMode();
	else
		CSTGKeybedInterface_ExitKeyCheckMode();
}

void CSTGControlMsgHandler::ReenableUSBPorts(const STGMsgDataOneParam *, int)
{
	/* Ignores its own param entirely -- confirmed real, re-enables BOTH
	 * ports unconditionally. */
	CSTGKeybedInterface_EnableUSBPort(0, true);
	CSTGKeybedInterface_EnableUSBPort(1, true);
}

void CSTGControlMsgHandler::SetRearLEDState(const STGMsgDataOneParam *param, int)
{
	/* Real hardware-generation branch, confirmed via the same
	 * STGAPI_OFF_NKS4_HW_VERSION==3 check already established in
	 * calibration_msg_handler.cpp's IsTouchPanelOnlyMode(). */
	unsigned char hwVer = *((unsigned char *)STGAPIFrontPanelStatus::sInstance +
				 STGAPI_OFF_NKS4_HW_VERSION);
	if (hwVer != 3) {
		unsigned int value = param->value & 0xff;
		OmapNKS4OutputFifo_WriteCommand((int)((value << 16) | 0x0a000000u));
	} else {
		CSTGKeybedInterface_EnableRearLED(param->value != 0);
	}
}

void CSTGControlMsgHandler::StartSTG(const void *, int)
{
	COmapNKS4Driver_StartScanning();
	CSTGDrumPadInterface::sInstance->StartScanning();
	*(unsigned char *)CSTGMidiPortManager::sInstance = 1;
	/* Real field name/meaning not independently confirmed -- see
	 * HARDWARE_REVIEW_LOG.md. */
	*((unsigned char *)CLoadBalancer::sInstance + 0xa4) = 1;
	CSTGKeybedInterface_sInstance()[KEYBED_OFF_DISPATCH_GATE2] = 1;
}

/* ------------------------------------------------------------------ */
/* Audio driver mute/unmute -- real virtual dispatch on the ONE real   */
/* CSTGAudioDriverInterface concrete instance (oa_engine.h, already an  */
/* established real `virtual` class in this project).                  */
/* ------------------------------------------------------------------ */

void CSTGControlMsgHandler::MuteDAC(const STGMsgDataOneParam *param, int)
{
	if (param->value != 0)
		CSTGAudioDriverInterface::sInstance->MuteAllAudio();
	else
		CSTGAudioDriverInterface::sInstance->UnmuteAllAudio();
}

void CSTGControlMsgHandler::MuteADC(const STGMsgDataOneParam *param, int)
{
	/* Real quirk, confirmed via the real vtable slot offsets used
	 * (+0x3c/+0x40, MuteAudioOutputs/UnmuteAudioOutputs) -- "MuteADC"
	 * actually mutes the AUDIO OUTPUTS in ground truth, not the inputs.
	 * Reproduced faithfully, not "fixed". See HARDWARE_REVIEW_LOG.md. */
	if (param->value != 0)
		CSTGAudioDriverInterface::sInstance->MuteAudioOutputs();
	else
		CSTGAudioDriverInterface::sInstance->UnmuteAudioOutputs();
}

/* ------------------------------------------------------------------ */
/* Debug I2C bus commands -- both real hardware-quirk stubs: neutered   */
/* on shipping firmware, always return an error code regardless of      */
/* param (reproduced faithfully -- return value itself is not modeled,  */
/* matching this project's `void`-returning dispatch-table convention). */
/* ------------------------------------------------------------------ */

void CSTGControlMsgHandler::ReadI2CDevice(const STGControlMsgDataTwoParam *, int)
{
	STGControlReplyMsg reply = {};
	reply.size = 0x10;
	reply.reserved = 0;
	reply.subtype = 0x13;
	reply.value = 0;
	PushMessage(&reply);
}

void CSTGControlMsgHandler::WriteI2CDevice(const STGControlMsgDataThreeParam *, int)
{
	/* 2-instruction real body: ignores both parameters entirely, no
	 * PushMessage call at all. */
}

/* ------------------------------------------------------------------ */
/* Diagnostic peak-usage readback -- clear-on-read accumulators.        */
/* ------------------------------------------------------------------ */

void CSTGControlMsgHandler::ReadCPUUsagePeak(const STGControlMsgDataOneParam *param, int)
{
	/* CSTGAudioManager+0xc is a real inline array of per-core stats
	 * sub-object pointers, indexed by (coreId+8) -- modeled as a
	 * genuine native `void**` (matching CSTGMidiPortManager::
	 * sMidiInPorts/sMidiOutPorts's own established "opaque native
	 * pointer slots" precedent, oa_engine.h) rather than a packed u32,
	 * since this table is never asserted byte-exact against a captured
	 * real-hardware memory dump elsewhere in this project. */
	unsigned int coreId = param->value;
	unsigned char *audioMgr = (unsigned char *)CSTGAudioManager::sInstance;
	unsigned char **statsTable = (unsigned char **)(audioMgr + 0xc);
	unsigned char *stats = statsTable[coreId + 8];

	int scaled = *(int *)(stats + 0x16e8) * 100;
	double asFloat = (double)scaled * (double)CSTGCPUInfo::sInstance->field10;
	int truncated = (int)asFloat;

	*(int *)(stats + 0x16e8) = 0;
	*(unsigned int *)(stats + 0x16ec) = 0xffffffffu;

	STGControlReplyMsgCPU reply = {};
	reply.size = 0x14;
	reply.reserved = 0;
	reply.subtype = 1;
	reply.coreId = coreId;
	reply.value = (unsigned int)truncated;
	PushMessage(&reply);
}

void CSTGControlMsgHandler::ReadFXUsagePeak(const void *, int)
{
	unsigned char *audioMgr = (unsigned char *)CSTGAudioManager::sInstance;
	unsigned char *stats = *(unsigned char **)(audioMgr + 0x3c);

	int scaled = *(int *)(stats + 0x48) * 100;
	double asFloat = (double)scaled * (double)CSTGCPUInfo::sInstance->field10;
	int truncated = (int)asFloat;

	*(int *)(stats + 0x48) = 0;
	*(unsigned int *)(stats + 0x4c) = 0xffffffffu;

	STGControlReplyMsg reply = {};
	reply.size = 0x10;
	reply.reserved = 0;
	reply.subtype = 3;
	reply.value = (unsigned int)truncated;
	PushMessage(&reply);
}

void CSTGControlMsgHandler::ReadDiskThroughputPeak(const void *, int)
{
	unsigned char *diskMgr = (unsigned char *)CSTGDiskCostManager::sInstance;
	float peak = *(float *)(diskMgr + 0x34);
	*(float *)(diskMgr + 0x34) = 0.0f;

	STGControlReplyMsg reply = {};
	reply.size = 0x10;
	reply.reserved = 0;
	reply.subtype = 2;
	reply.value = (unsigned int)(int)peak;
	PushMessage(&reply);
}

void CSTGControlMsgHandler::StealAllVoices(const void *, int)
{
	CSTGVoiceAllocator::sInstance->StealAllVoices();
	CSTGVoiceAllocator::sInstance->FreeStolenVoices();

	/* Confirmed real: only 3 of the 4 payload dwords are set in ground
	 * truth (the 4th, `value`, is genuinely left as uninitialized stack
	 * bytes on real hardware) -- zeroed here for host-KAT determinism. */
	STGControlReplyMsg reply = {};
	reply.size = 0x10;
	reply.reserved = 0;
	reply.subtype = 0;
	PushMessage(&reply);
}

/* ------------------------------------------------------------------ */
/* Download session bookkeeping -- forward to already-real              */
/* CSTGMessageProcessor methods.                                        */
/* ------------------------------------------------------------------ */

void CSTGControlMsgHandler::StartDownloadHandler(const void *, int)
{
	CSTGMessageProcessor::sInstance->StartDownload();
}

void CSTGControlMsgHandler::EndDownloadHandler(const void *, int)
{
	CSTGMessageProcessor::sInstance->EndDownload();
}
