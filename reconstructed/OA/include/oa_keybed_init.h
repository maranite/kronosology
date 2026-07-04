// SPDX-License-Identifier: GPL-2.0
/*
 * oa_keybed_init.h  -  CSTGKeybedInterface_Startup()/_Cleanup(): the
 * real init_module step 14 (hard-fail).
 *
 * Ground-truthed via readelf's symbol table against
 * ARCHIVE/Ignored/DecryptedImages/OA_real.ko (`CSTGKeybedInterface_
 * Startup`, `.text+0x33e5e0`, 319 bytes; `_Cleanup`, `.text+0x33e720`,
 * 30 bytes), then a full objdump disassembly + relocation trace.
 *
 * Confirms and extends what an earlier pass in this project already
 * established (MASTER_REFERENCE.md sec 10.36/10.40): this is OA.ko's
 * own real serial-port keybed handshake, scanning `CSTGComPort::
 * eComPortId` values 0-5 -- CORRECTED from an earlier "0-6" claim: the
 * real loop-exit check computes comPortId=6 but never actually calls
 * Initialize() with it (confirmed via the host KAT catching a real
 * off-by-one in an earlier draft's own C++ translation, then verifying
 * the true bound against the raw disassembly a second time) -- sending
 * a fixed probe byte (0xa5) and busy-waiting for a real keybed board's
 * ACK. INVERTED SUCCESS
 * CONVENTION (matches step 13): nonzero return = success.
 *
 * `CSTGKeybedInterface::sInstance` (confirmed via relocation) is used
 * throughout via DIRECT offset arithmetic on the symbol's own address
 * (`mov eax, sInstance+0x30` etc. -- no bracketed dereference anywhere
 * in this function), NOT loaded as a pointer's VALUE first -- unlike
 * `CSTGGlobal::sInstance`/`CSTGHeapManager::sInstance` elsewhere in
 * this project (real pointer variables, always dereferenced via `[..]`
 * before use). This means `CSTGKeybedInterface`'s singleton is a real
 * STATIC/GLOBAL OBJECT (its own storage IS what the `sInstance` symbol
 * names), not a pointer-to-heap-object -- modeled here as a fixed-size
 * byte blob with named offset constants (this project's established
 * convention for raw offset arithmetic into not-fully-recovered
 * structs), matching that storage model directly.
 *
 * `CSTGComPort` and its nested `TransmitFifo` sub-object are confirmed
 * to live within the same `CSTGKeybedInterface::sInstance` storage
 * (base @+0, `TransmitFifo` sub-object @+4) -- not a separate object.
 * `CSTGComPort::Cleanup`/`TriggerInterrupt`/`TransmitFifo::WriteByte`
 * are now fully reconstructed -- see `oa_comport.h`
 * (MASTER_REFERENCE.md sec 10.53). `Initialize` itself remains a
 * confirmed-real, deliberately deferred extern (that header's own
 * comment explains why).
 *
 * **CORRECTS a real bug in this file's own earlier revision**: these
 * four were originally declared as PLAIN C-linkage functions
 * (`CSTGComPort_Initialize` etc.) -- but the real relocations target
 * genuine MANGLED C++ methods, confirmed via `readelf`. A plain
 * C-linkage symbol of that name would never have matched the real one
 * at Kbuild link time. Fixed by including the real class declaration
 * and calling through it directly (see `keybed_init.cpp`'s own updated
 * call sites).
 */

#ifndef OA_KEYBED_INIT_H
#define OA_KEYBED_INIT_H

#include "oa_comport.h"

/* Fixed-size blob matching CSTGKeybedInterface::sInstance's own
 * storage. Sized to the highest confirmed offset this function
 * touches (+0xb50), rounded up. Only fields this function itself
 * reads/writes are named via offset constants. */
#define KEYBED_SINSTANCE_SIZE            0x0c00
#define KEYBED_OFF_DEBOUNCE_FILTER       0x0030 /* CSTGKeybedKeyDebounceFilter sub-object */
#define KEYBED_OFF_STATE                 0x0b4c /* 0=not started, 1=port open, 2=fully started */
#define KEYBED_OFF_ACK_FLAG              0x0b50 /* cleared before each probe, set by the real ISR */

extern "C" unsigned char *CSTGKeybedInterface_sInstance(void);

extern "C" {

void CSTGKeybedKeyDebounceFilter_Initialize(unsigned char *filter);

void __const_udelay(unsigned long xloops);

int CSTGKeybedInterface_Startup(void);
void CSTGKeybedInterface_Cleanup(void);

} /* extern "C" */

#endif /* OA_KEYBED_INIT_H */
