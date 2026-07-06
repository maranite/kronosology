// SPDX-License-Identifier: GPL-2.0
/*
 * smoother_ctor.cpp  -  CSTGSmoother::CSTGSmoother() (batch 22).
 *
 * Deliberately a separate translation unit from smoother_init.cpp/
 * smoother_cancel.cpp: `verify/test_engine_init.cpp` links
 * `src/engine/engine_init.cpp` directly, whose own confirmed real
 * `CSTGEngine::Initialize()` does `new (CSTGBankMemory::AllocAligned(
 * 0xf028, 0x10)) CSTGSmoother();` -- but that same test file already
 * carries its OWN `MOCK_CTOR_ONLY(CSTGSmoother)` call-counting mock
 * (defining `CSTGSmoother::CSTGSmoother()` itself). Putting the real
 * ctor in a brand-new file that test_engine_init.cpp does NOT link
 * avoids a "multiple definition" clash there, matching the
 * `CSTGStreamingEventManager`/`WriteSTGMidiOutQueue` "give it its own
 * dedicated TU" precedent (sec 10.145/10.158). None of the other five
 * verify/ binaries that reference `CSTGSmoother` (test_engine.cpp/
 * test_global.cpp/test_global_ctor.cpp/test_smoother_init.cpp/
 * test_smoother_cancel.cpp/test_slot_voice_data_free.cpp) construct a
 * real instance via the ctor either -- they all cast a raw `calloc`/
 * `mmap32` buffer directly to `CSTGSmoother*`, matching this project's
 * established "opaque struct reinterpreted onto raw memory" pattern --
 * so none of them need any change.
 *
 * CSTGSmoother::CSTGSmoother() (`.text+0x2a110`, 422 bytes) confirmed,
 * fully call/branch-free apart from its own single counting loop (320
 * iterations, `ecx` counts down from 0x140, tested via the flags left
 * by the LAST `sub`, none of the intervening stores touch flags) --
 * the sec 10.160 "big but branch/call-free is safe, just tedious to
 * transcribe" category, same as `CSTGSamplingInterface`'s own ctor.
 *
 * Builds 320 embedded "SmootherMapping" sub-objects, each 0xc0 (192)
 * bytes (matching `smoother_init.cpp`'s own already-documented stride
 * and its own explicit note that these fields were "NOT actually
 * zeroed on the real target yet" prior to this ctor being reconstructed
 * -- now resolved). Each mapping embeds FOUR distinct
 * "CSTGXxxMessageContext" sub-objects back to back, each vtable-
 * installed via the confirmed `+8` Itanium "skip offset-to-top/RTTI"
 * convention already used throughout this project:
 *   +0x00..+0x1f  CSTGProgramMessageContext-shaped region
 *                 (vtable ptr at +0x1c)
 *   +0x20..+0x53  CSTGProgramSlotMessageContext-shaped region
 *                 (vtable ptr at +0x38)
 *   +0x54..+0x87  CSTGPatchMessageContext-shaped region
 *                 (vtable ptr at +0x54)
 *   +0x88..+0x9f  CSTGMessageContext-shaped region (vtable ptr at +0x88)
 *   +0xb0/+0xb4   list-link next/prev (matching `smoother_init.cpp`'s
 *                 own already-documented "+0xb0 offset WITHIN each
 *                 mapping object" free-list note -- CONFIRMS that
 *                 comment's field placement independently)
 *   +0xb8         owner back-ref, set to the mapping's OWN address
 *                 (self-pointer, not the enclosing CSTGSmoother)
 *   +0x84         bit0 explicitly cleared (`andb $0xfe`) each iteration
 *                 -- some other flags byte, real semantics not
 *                 otherwise determined by this ctor alone
 * None of these four vtables are ever DISPATCHED through by any
 * function reconstructed in this project so far (install-only, per the
 * sec 10.153 "install vs dispatch" rule) -- safe zero-filled 12-byte
 * placeholders (confirmed real size via `nm -C -S`, all four report
 * `V ... 0000000c`).
 *
 * After the 320-slot loop, six top-level list-management dwords are
 * zeroed at +0xf004/+0xf008/+0xf00c/+0xf010/+0xf014/+0xf018 -- exactly
 * the three fields (`+0xf004`/`+0xf008`/`+0xf00c`) `smoother_init.cpp`'s
 * OWN header comment already named as "confirmed zeroed by the real
 * ctor" (written before this ctor was reconstructed, based purely on
 * cross-referencing `Initialize()`'s own field usage) -- independently
 * CONFIRMED correct by this pass's fresh disassembly, plus three more
 * (`+0xf010`/`+0xf014`/`+0xf018`, the CC-smoother list head/tail/count
 * used by `CancelAllCCSmoothers()`/`smoother_cancel.cpp`). Finally
 * `sInstance = this`.
 */

#include "oa_engine_init.h"

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

/* Vtable placeholders for the four embedded MessageContext-family
 * sub-objects -- confirmed real 12-byte size (`nm -C -S`: "V ...
 * 0000000c" for all four), never dispatched through anywhere in this
 * project yet, so zero-filled per the established convention. */
extern "C" unsigned char _ZTV25CSTGProgramMessageContext[12];
unsigned char _ZTV25CSTGProgramMessageContext[12];
extern "C" unsigned char _ZTV29CSTGProgramSlotMessageContext[12];
unsigned char _ZTV29CSTGProgramSlotMessageContext[12];
extern "C" unsigned char _ZTV23CSTGPatchMessageContext[12];
unsigned char _ZTV23CSTGPatchMessageContext[12];
extern "C" unsigned char _ZTV18CSTGMessageContext[12];
unsigned char _ZTV18CSTGMessageContext[12];

/* CSTGSmoother::sInstance's own storage already lives in
 * engine_init.cpp (pre-existing, sec 10.86 era) -- NOT redefined here
 * to avoid a duplicate-definition link error in the real .ko build,
 * where both this file and engine_init.cpp are linked together. */

CSTGSmoother::CSTGSmoother()
{
	unsigned char *slot = (unsigned char *)this;

	for (int i = 0; i < 320; i++) {
		slot[0x84] &= 0xfe;

		/* CSTGProgramMessageContext-shaped region */
		*(unsigned int *)(slot + 0x20) = 0;
		*(unsigned int *)(slot + 0x24) = 1;
		*(unsigned int *)(slot + 0x2c) = 0;
		slot[0x30] = 0; slot[0x31] = 1; slot[0x32] = 1; slot[0x33] = 0;
		*(unsigned int *)(slot + 0x1c) = ToU32(_ZTV25CSTGProgramMessageContext + 8);
		*(unsigned int *)(slot + 0x34) = 0;
		*(unsigned int *)(slot + 0x28) = 6;

		/* CSTGProgramSlotMessageContext-shaped region */
		*(unsigned int *)(slot + 0x3c) = 0;
		*(unsigned int *)(slot + 0x40) = 1;
		*(unsigned int *)(slot + 0x48) = 0;
		slot[0x4c] = 0; slot[0x4d] = 1; slot[0x4e] = 1; slot[0x4f] = 0;
		*(unsigned int *)(slot + 0x38) = ToU32(_ZTV29CSTGProgramSlotMessageContext + 8);
		*(unsigned int *)(slot + 0x50) = 0;
		*(unsigned int *)(slot + 0x44) = 5;

		/* CSTGPatchMessageContext-shaped region */
		*(unsigned int *)(slot + 0x58) = 0;
		*(unsigned int *)(slot + 0x5c) = 1;
		*(unsigned int *)(slot + 0x64) = 0;
		slot[0x68] = 0; slot[0x69] = 1; slot[0x6a] = 1; slot[0x6b] = 0;
		*(unsigned int *)(slot + 0x54) = ToU32(_ZTV23CSTGPatchMessageContext + 8);
		*(unsigned int *)(slot + 0x60) = 4;
		*(unsigned int *)(slot + 0x70) = 0xffffffffu;
		*(unsigned int *)(slot + 0x74) = 0;
		*(unsigned int *)(slot + 0x6c) = 0;
		*(unsigned int *)(slot + 0x78) = 0;
		*(unsigned int *)(slot + 0x7c) = 0;

		/* CSTGMessageContext-shaped region */
		*(unsigned int *)(slot + 0x80) = 0;
		*(unsigned int *)(slot + 0x88) = ToU32(_ZTV18CSTGMessageContext + 8);
		*(unsigned int *)(slot + 0x94) = 0;
		*(unsigned int *)(slot + 0x8c) = 0;
		*(unsigned int *)(slot + 0x90) = 1;
		*(unsigned int *)(slot + 0x98) = 0;
		slot[0x9c] = 0; slot[0x9d] = 1; slot[0x9e] = 1; slot[0x9f] = 0;

		/* Free-list link fields + self owner back-ref */
		*(unsigned int *)(slot + 0xb8) = ToU32(slot);
		*(unsigned int *)(slot + 0xb0) = 0;
		*(unsigned int *)(slot + 0xb4) = 0;
		*(unsigned int *)(slot + 0xbc) = 0;

		slot += 0xc0;
	}

	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)(base + 0xf008) = 0;
	*(unsigned int *)(base + 0xf004) = 0;
	*(unsigned int *)(base + 0xf00c) = 0;
	*(unsigned int *)(base + 0xf014) = 0;
	*(unsigned int *)(base + 0xf010) = 0;
	*(unsigned int *)(base + 0xf018) = 0;

	sInstance = this;
}
