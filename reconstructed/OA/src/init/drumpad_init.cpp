// SPDX-License-Identifier: GPL-2.0
/*
 * drumpad_init.cpp  -  CSTGDrumPadInterface_Initialize()/_Cleanup(),
 * ConstructDrumPadClient(), and CSTGDrumPadClient's 3 real methods.
 *
 * Ground truth: `CSTGDrumPadInterface_Initialize` (.text+0x33ce60, 20
 * bytes) and `CSTGDrumPadInterface_Cleanup` (.text+0x33cea0, 17 bytes)
 * are REAL standalone C-linkage symbols in OA_real.ko -- and each is
 * BYTE-IDENTICAL to its own mangled-C++ sibling
 * (`CSTGDrumPadInterface::Initialize()`/`::Cleanup()`, .text+0x33ce40/
 * 0x33ce80): the compiler emitted two separate entry points for the
 * same body (one C-linkage, one mangled), unlike the
 * SCalibrationData/CSTGKeybedKeyDebounceFilter case (see
 * calibration_data.cpp/keybed_debounce.cpp) where only the mangled
 * form exists.
 *
 *   Initialize(): `mov eax,&sDrumPadClient; call
 *     USBMidiAccessory_SetDrumPadClient; <epilogue>; ret` -- the call's
 *     own return value is passed straight through as this function's
 *     return value (no `mov eax,...` after the call), matching the
 *     `int` return this project's header already declares.
 *   Cleanup(): `xor eax,eax; call USBMidiAccessory_SetDrumPadClient;
 *     <epilogue>; ret` -- unregisters with a NULL pointer, `void`
 *     return (matches the header; the call's own return value is
 *     simply discarded since the caller never reads EAX here).
 *
 * `USBMidiAccessory_SetDrumPadClient` is confirmed `U` (genuinely
 * external, resolved by whatever module owns the USB MIDI accessory
 * driver -- KorgUsbMidi* / USBMidiAccessory_SetMidiInClient live in the
 * same `U` family in ground truth) -- declared here, not defined; a
 * real external dependency, not something for this project to
 * substitute.
 *
 * CORRECTION (2026-07-27, gFixAudioInputFrameOrder/CSTGDrumPadClient
 * loose-ends pass): a prior session mis-modeled the argument
 * `Initialize()` passes as an opaque "receive-queue buffer" of
 * unconfirmed size (`gDrumPadReceiveQueue`). Real ground truth
 * (confirmed via `objdump -dr`): the address loaded is
 * `&sDrumPadClient`, a genuine `CSTGDrumPadClient` OBJECT (sizeof 4,
 * `.bss+0x26d38c`) whose vtable pointer is installed by ground truth's
 * own `_GLOBAL__I__ZN20CSTGDrumPadInterface9sInstanceE` global
 * constructor -- see `ConstructDrumPadClient()` below, and
 * oa_control_msg_handler.h's `CSTGDrumPadClient` class comment for the
 * full trace (including the SECOND relocation-hidden-as-literal-8 bug
 * this uncovered).
 *
 * CORRECTION #2 (2026-07-27, broad re-audit for the day's 3 recurring
 * bug classes): `g_drumPadClientVtable`'s original initializer used
 * `AsRawFn(&CSTGDrumPadClient::Method)` directly inside the static
 * aggregate initializer -- the EXACT SAME anti-pattern already found
 * and fixed in `audio_input_mixer.cpp` (804b909): a member-function-
 * pointer-to-`void*` type pun via `__builtin_memcpy` is not a C++
 * constant expression, so GCC silently emitted the whole vtable's
 * population as a dynamic initializer (`_GLOBAL__sub_I_*` in
 * `.text.startup`, registered via `.init_array`) instead of link-time
 * relocations in `.data`. Confirmed by compiling this file standalone
 * with the real kernel-module flags and checking `readelf -S`/`-r`:
 * `g_drumPadClientVtable` lived entirely in `.bss` (all-zero) with its
 * real content only ever written by that `.init_array` entry -- which
 * Linux's module loader never runs (same as every other confirmed
 * instance of this bug class today). Left unfixed, `ConstructDrumPadClient()`
 * below would install a vtable pointer into an all-NULL vtable, and any
 * call through `CanReceiveTriggerEvent`/`ReceiveTriggerEvent`/
 * `ReceiveNotification` would jump through a NULL function pointer.
 * Fixed the same way as `audio_input_mixer.cpp`: plain free-function
 * trampolines instead of member-function-pointer type puns, since a
 * free function's address IS a genuine compile-time constant.
 */

#include "oa_init.h"
#include "oa_control_msg_handler.h"	/* CSTGDrumPadClient, CSTGGlobal, CSTGMessageProcessor */

extern "C" int USBMidiAccessory_SetDrumPadClient(void *client);

namespace {

/* Real vtable shape, confirmed via `.rel.rodata._ZTV17CSTGDrumPadClient`:
 * offsetToTop/rtti are literal 0 (no RTTI, no virtual destructor), then
 * the 3 real methods in declaration order. Populated via plain free-
 * function trampolines (genuine compile-time constants), NOT via
 * `AsRawFn`-style member-function-pointer type puns inside the static
 * initializer -- see this file's own header comment (CORRECTION #2)
 * for the `.init_array`-never-runs bug that pattern caused here. */
struct DrumPadClientVtable {
	void *offsetToTop;
	void *rtti;
	void *canReceiveTriggerEvent;
	void *receiveTriggerEvent;
	void *receiveNotification;
};

} /* anonymous namespace */

static int CSTGDrumPadClientCanReceiveTriggerEventTrampoline(void *self)
{
	return ((CSTGDrumPadClient *)self)->CanReceiveTriggerEvent();
}
static void CSTGDrumPadClientReceiveTriggerEventTrampoline(void *self, unsigned short event)
{
	((CSTGDrumPadClient *)self)->ReceiveTriggerEvent(event);
}
static void CSTGDrumPadClientReceiveNotificationTrampoline(void *self, CUSBMidiAccessory_DrumPadClient::eNotificiation n)
{
	((CSTGDrumPadClient *)self)->ReceiveNotification(n);
}

namespace {
DrumPadClientVtable g_drumPadClientVtable = {
	0, 0,
	(void *)&CSTGDrumPadClientCanReceiveTriggerEventTrampoline,
	(void *)&CSTGDrumPadClientReceiveTriggerEventTrampoline,
	(void *)&CSTGDrumPadClientReceiveNotificationTrampoline,
};
} /* anonymous namespace */

/*
 * The real singleton object ground truth calls `sDrumPadClient`
 * (`.bss+0x26d38c`). sizeof 4 -- just the vtable pointer, zero-
 * initialized by .bss default until ConstructDrumPadClient() installs
 * the real vtable pointer (matching ground truth's own `.ctors` timing,
 * BEFORE init_module() runs -- see init_module.cpp).
 */
static CSTGDrumPadClient sDrumPadClient;

/*
 * CSTGDrumPadInterface::sInstance's own ring-buffer fields that
 * CSTGDrumPadClient's 3 methods touch -- see oa_control_msg_handler.h's
 * CSTGDrumPadClient comment for why this is separate, dedicated storage
 * rather than a real field of the existing (pointer-typed, out-of-scope)
 * CSTGDrumPadInterface class. Layout confirmed via real disassembly:
 * 32 slots of 2 bytes each (a 16-bit STGDrumPadTriggerEvent per slot,
 * index masked with 0x1f), then the write index, read index, and an
 * "armed" gate byte -- `sizeof` comes out to exactly 0x4c, matching
 * ground truth's own confirmed `CSTGDrumPadInterface::sInstance` object
 * size (`nm`: `OBJECT GLOBAL 76 ...`).
 */
namespace {
struct DrumPadRing {
	unsigned char slots[0x40];	/* +0x00 -- 32 * 2-byte trigger events */
	unsigned int writeIndex;	/* +0x40 */
	unsigned int readIndex;	/* +0x44 */
	unsigned char armed;		/* +0x48 */
};
} /* anonymous namespace */
static DrumPadRing gDrumPadInterfaceRing;

/*
 * ConstructDrumPadClient() -- manual substitute for a FOURTH real
 * do_mod_ctors() entry (see oa_init.h's ConstructKorgUsbMidiPorts()/
 * ConstructPerformanceVarsManagerSelectorState()/
 * ConstructChannelValuesTemplate() for the established convention this
 * follows). Ground truth's real
 * `_GLOBAL__I__ZN20CSTGDrumPadInterface9sInstanceE` (.text+0x33d350, 38
 * bytes) does 2 things:
 *   1. Zeroes the ring buffer's write/read indices and armed flag --
 *      redundant with plain BSS zero-init here, matching this project's
 *      existing convention of not reproducing zero-equivalent writes
 *      (ConstructPerformanceVarsManagerSelectorState()'s own precedent).
 *   2. `mov dword ptr [sDrumPadClient], 0x8` -- installs
 *      `sDrumPadClient`'s vtable pointer. THIS WAS PREVIOUSLY MISSED:
 *      init_module.cpp's same-day `.ctors` sweep read this exact
 *      instruction's raw immediate operand and concluded it was a
 *      no-op (a plain literal 8, "already matching plain BSS
 *      zero-init"). `readelf -r` shows a SECOND `R_386_32` relocation
 *      on that same immediate operand, against `_ZTV17CSTGDrumPadClient`
 *      -- the real stored value is `&_ZTV17CSTGDrumPadClient + 8`, not
 *      the integer 8. Confirmed independently via raw `objdump -dr`
 *      against the real ground-truth OA.ko (not just the Ghidra
 *      decompile). This is genuinely NOT a no-op; it's the exact
 *      vtable-pointer install this project's own literal-vs-relocation
 *      sweep was looking for.
 */
extern "C" void ConstructDrumPadClient(void)
{
	sDrumPadClient._vtablePtr = &g_drumPadClientVtable.canReceiveTriggerEvent;
}

int CSTGDrumPadClient::CanReceiveTriggerEvent()
{
	if (!gDrumPadInterfaceRing.armed)
		return 0;
	return (gDrumPadInterfaceRing.writeIndex - gDrumPadInterfaceRing.readIndex) != 0x20;
}

void CSTGDrumPadClient::ReceiveTriggerEvent(unsigned short event)
{
	bool msgProcessorGateClear = (CSTGMessageProcessor::sInstance->_unrecovered[0x48] == 0);
	bool globalFlagSet = *(unsigned char *)((char *)CSTGGlobal::sInstance + 0x6ac) != 0;

	if (!(msgProcessorGateClear || globalFlagSet))
		return;
	if ((gDrumPadInterfaceRing.writeIndex - gDrumPadInterfaceRing.readIndex) == 0x20)
		return;

	unsigned int slot = gDrumPadInterfaceRing.writeIndex & 0x1f;
	gDrumPadInterfaceRing.slots[slot * 2]     = (unsigned char)(event & 0xff);
	gDrumPadInterfaceRing.slots[slot * 2 + 1] = (unsigned char)(event >> 8);
	gDrumPadInterfaceRing.writeIndex++;
}

void CSTGDrumPadClient::ReceiveNotification(CUSBMidiAccessory_DrumPadClient::eNotificiation n)
{
	if (!gDrumPadInterfaceRing.armed)
		return;
	if (n != 0)
		return;

	if (CSTGMessageProcessor::sInstance->_unrecovered[0x48] == 0) {
		/* Ground truth: 8x identical unrolled blocks pushing MIDI
		 * Note-On status bytes 0x90..0x97 (one per channel 0-7),
		 * each independently gated only by ring-full -- a full ring
		 * skips that one push but does NOT stop the remaining ones
		 * (confirmed: no early return in this branch). */
		for (int i = 0; i < 8; i++) {
			if ((gDrumPadInterfaceRing.writeIndex - gDrumPadInterfaceRing.readIndex) != 0x20) {
				unsigned int slot = gDrumPadInterfaceRing.writeIndex & 0x1f;
				gDrumPadInterfaceRing.slots[slot * 2] = (unsigned char)(0x90 + i);
				gDrumPadInterfaceRing.slots[slot * 2 + 1] = 0;
				gDrumPadInterfaceRing.writeIndex++;
			}
		}
	} else {
		/* Ground truth re-checks CSTGGlobal::sInstance->flagAt(0x6ac)
		 * before EVERY one of the 8 pushes -- 8 identical checks
		 * against the same never-mutated-within-this-call address,
		 * so behaviorally this pushes all 8 if the flag is set at
		 * entry, or returns immediately (pushing none) if it's
		 * clear. Reproduced as the real repeated check (an early
		 * RETURN from the whole function, confirmed via disassembly)
		 * rather than collapsed to a single test, matching this
		 * project's "preserve real quirks" convention. */
		for (int i = 0; i < 8; i++) {
			if (!*(unsigned char *)((char *)CSTGGlobal::sInstance + 0x6ac))
				return;
			if ((gDrumPadInterfaceRing.writeIndex - gDrumPadInterfaceRing.readIndex) != 0x20) {
				unsigned int slot = gDrumPadInterfaceRing.writeIndex & 0x1f;
				gDrumPadInterfaceRing.slots[slot * 2] = (unsigned char)(0x90 + i);
				gDrumPadInterfaceRing.slots[slot * 2 + 1] = 0;
				gDrumPadInterfaceRing.writeIndex++;
			}
		}
	}
}

int CSTGDrumPadInterface_Initialize(void)
{
	return USBMidiAccessory_SetDrumPadClient(&sDrumPadClient);
}

void CSTGDrumPadInterface_Cleanup(void)
{
	USBMidiAccessory_SetDrumPadClient(0);
}
