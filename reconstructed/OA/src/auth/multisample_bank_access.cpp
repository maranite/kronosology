// SPDX-License-Identifier: GPL-2.0
/*
 * multisample_bank_access.cpp  -  CSTGMultisampleBankManager::AccessBank
 * real body (see include/oa_types.h). Unblocks the already-fully-
 * reconstructed LM/LD/CM/CD/CL /proc/.oacmd handlers (process_oacmd.cpp),
 * which all call this to resolve a bank UUID to a live bank pointer before
 * this reconstruction always returned null (bank never found, every one
 * of those commands unconditionally reported failure).
 *
 * Ground-truthed via full disassembly: .text+0x3dce0, 135 bytes (nm -S
 * 0x87), `_ZN26CSTGMultisampleBankManager10AccessBankERK23CSTGMultisampleBankUUID`.
 *
 * Confirmed real algorithm (regparm(3): this=EAX, uuid ref=EDX):
 *
 *   1. `memcmp(uuid, kROMBankUUID, 16)` (real `repz cmpsb`, ecx=0x10)
 *      against a 16-byte internal-linkage constant (ground truth:
 *      `_ZL12kROMBankUUID`, C++ `static` -- NOT an exported symbol in
 *      ground truth either, so there is no real name to match; a local
 *      constant here is faithful). Confirmed a DIFFERENT symbol from
 *      `CSTGMultisampleBankUUIDBase::sLegacyBankPrefix` (an unrelated
 *      .rodata global at a different address) -- not to be conflated.
 *   2. If equal (the caller is asking for the built-in ROM bank): the
 *      heap slot index is read directly from `this+0xa020` (a per-manager
 *      cached field -- confirmed real, brand new offset, not previously
 *      documented anywhere in this project). No call to FindBankRecord on
 *      this path.
 *   3. If not equal: calls the real `CSTGMultisampleBankManager::
 *      FindBankRecord(uuid)` (`.text+0x3da30`, 661 bytes -- a genuine hash
 *      lookup over `CSTGMultisampleBankHashList`, itself backed by
 *      `AccessBankRecord`, another 341 bytes -- a substantial, separate,
 *      genuine filesystem/registry subsystem). If it returns null, this
 *      function returns null immediately. Otherwise the heap slot index
 *      is read from the FOUND RECORD's own `+0x00` dword.
 *   4. Either way, the resulting slot index is bounds-checked against
 *      100000 (`0x1869f` = 99999, `ja` on the real `cmp`) and, if in
 *      range, converted to an absolute pointer via the SAME
 *      `*(heapmgr+slot*20+0x24) + *(heapmgr+0x1e8498)` idiom already
 *      established and reused (via the WORKAROUND-safe captured-snapshot
 *      form) as `oa_heap_region()` in oa_heap.h -- confirmed the exact
 *      same instruction shape (`lea ecx,[ecx+ecx*4]; shl ecx,2; ...
 *      +0x18`/`+0x24` handle-table stride) as that helper's own derivation,
 *      not independently re-derived. Out-of-range or a null heap region
 *      both yield null, matching `oa_heap_region()`'s own bounds check --
 *      no separate bounds check needed here.
 *
 * `FindBankRecord` is deliberately deferred (own real body not
 * reconstructed) -- genuine filesystem/registry-adjacent subsystem, out
 * of scope per the sec 10.185 policy already applied to
 * StartupInitializeROMBank/ScanFileSystem/CSTGInstalledEXProducts::
 * Initialize (see load_global_resources.cpp). Safe default returns null
 * ("bank not registered"), which is this function's OWN prior behavior
 * for every UUID before this reconstruction -- i.e. this change is
 * strictly additive: the ROM-bank fast path becomes real, the general
 * hash-lookup path keeps its previous (safe, "always fails") behavior
 * until FindBankRecord itself is reconstructed.
 */

#include "oa_types.h"
#include "oa_heap.h"

extern "C" void *CSTGMultisampleBankManager_FindBankRecord(
	struct CSTGMultisampleBankManager *self,
	const struct CSTGMultisampleBankUUID *uuid);

/* Ground truth: `_ZL12kROMBankUUID`, a C++ internal-linkage (static) .bss
 * constant -- not exported, no real name to match. Zero-initialized here:
 * its real content would be written by StartupInitializeROMBank (itself
 * deliberately deferred, load_global_resources.cpp), so all-zero is the
 * faithful current state given that upstream dependency isn't real yet. */
static const unsigned char kROMBankUUID[16] = { 0 };

void *CSTGMultisampleBankManager::AccessBank(struct CSTGMultisampleBankManager *self,
					      const struct CSTGMultisampleBankUUID *uuid)
{
	bool isRomBank = true;
	for (int i = 0; i < 16; i++) {
		if (uuid->bytes[i] != kROMBankUUID[i]) {
			isRomBank = false;
			break;
		}
	}

	unsigned int slot;
	if (isRomBank) {
		/* this+0xa020: manager's own cached ROM-bank heap slot index. */
		slot = *(unsigned int *)((char *)self + 0xa020);
	} else {
		void *record = CSTGMultisampleBankManager_FindBankRecord(self, uuid);
		if (!record)
			return 0;
		slot = *(unsigned int *)record;	/* record+0x00: this bank's heap slot */
	}

	return oa_heap_region(slot);
}
