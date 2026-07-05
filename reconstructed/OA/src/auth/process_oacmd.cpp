// SPDX-License-Identifier: GPL-2.0
/*
 * process_oacmd.cpp  -  see include/process_oacmd.h.
 *
 * Ground-truthed offset: ProcessOACmd .text+0xa0c0, 1773 bytes (real OA.ko
 * 3.2.1 ELF symbol table -- unaffected by the +0x10000 Ghidra-address bug
 * documented for OA.ko_auth.md, see MASTER_REFERENCE.md sec 9.2).
 *
 * SCOPE OF THIS RECONSTRUCTION (be explicit about what's verified vs. not):
 *
 *   FULLY DISASSEMBLY-CONFIRMED, byte-exact:
 *     - The "prefix + ':'" precondition and strlen(cmd) > 3 gate.
 *     - Real command strings read directly from the binary's rodata (not
 *       paraphrased): the 2-char prefixes "LM","LD","CM","CD","AU","CL","PT",
 *       "PC","KI", and the 4-character LITERAL matches "CB:*","SO:*","LA:*"
 *       (these three are not "2-char prefix + colon + payload" like the
 *       others -- they are matched as a fixed 4-byte string, wildcard-free
 *       despite the trailing '*' in the literal).
 *     - The "AU:" handler in full (see products.cpp's VerifyAndSaveAuthString).
 *     - The LM/LD/CM/CD shared parsing preamble: strlen > 0x2c, a
 *       36-character UUID (CUUID::ConvertFromText at cmd+3), then
 *       ":%lu:%lu:%lu" via sscanf starting at cmd+0x27.
 *     - "CL:<uuid>" (CORRECTED, see below): requires strlen(cmd) == 0x27
 *       (39) EXACTLY -- 2+1+36, no trailing numbers, unlike LM/LD/CM/CD.
 *       Looks the bank up by UUID (CUUID::ConvertFromText at cmd+3,
 *       CSTGMultisampleBankManager::AccessBank), and if found, locks
 *       PcmModuleMutex, calls CSTGMultisampleBank::ClosePCMDataFiles on that
 *       ONE bank, unlocks. *outResult = 0 if the bank was found, 1 if not.
 *     - "CB:*" (new, closes EVERY bank -- NOT "CL", see correction below):
 *       CSTGMultisampleBankManager::CloseAllBankFiles(mgr). Always
 *       *outResult = 0.
 *     - "SO:*" (new): CSTGInstalledEXProducts::ReInitialize(), reusing the
 *       same heapbase+0x14 registry addressing already established in
 *       products.cpp. *outResult = (ReInitialize() ^ 1) & 0xff, same
 *       "0=ok,1=fail" convention and the exact same shared return-tail code
 *       as "AU:" in the real binary.
 *     - "PT" (new): CSTGPianoModel::sInstance->RescanPianoTypes(), no
 *       argument parsing at all -- any payload after "PT:" is ignored.
 *       Always *outResult = 0.
 *     - "PC" (new): requires strlen(cmd) > 7, else "pcm cmd PC bad format"
 *       and returns unrecognized. Parses ":%lu:%lu:%lu" via sscanf starting
 *       at cmd+2 (the SAME format string offset LM/LD/CM/CD reuse), then
 *       calls CSTGPCMPrecacheManager::Reset with a CONFIRMED-BUT-COUNTERINTUITIVE
 *       argument mapping (see Reset's own comment below -- the parsed
 *       numbers do not map to Reset's parameters in the order they appear
 *       in the command string). Always *outResult = 0.
 *     - "KI" (new): parses ":%lu" via sscanf at cmd+2. On success, writes the
 *       parsed value directly into a heap-relative field at
 *       heapbase+0x6a554 -- NOT a function call, unlike every other command
 *       here. That offset is exactly 8 bytes past
 *       CSTGPCMPrecacheManager's own base (heapbase+0x6a54c, see
 *       oa_pcm_precache_manager()), so it is almost certainly a field
 *       *inside* that same object (a "kill/interrupt" flag, matching
 *       docs/interfaces/proc_oacmd.md's guess) rather than an unrelated
 *       global -- modeled here as a raw offset write, not a named member,
 *       since the struct's real layout is still otherwise opaque. On bad
 *       format: unrecognized (real firmware: printk("pcm cmd KI bad format
 *       %s\n", cmd), returns -1 without touching *outResult at all -- this
 *       reconstruction still sets *outResult=-1 defensively for consistency
 *       with every other bad-format path here, which is harmless since
 *       callers ignore *outResult whenever ProcessOACmd's return is nonzero).
 *     - "LA:*" (new, literal 4-byte match): CSTGPCMPrecacheManager::
 *       AfterProcess() (same manager/offset as "PC"). *outResult =
 *       (AfterProcess() ^ 1) & 0xff, same shared return-tail convention as
 *       "AU:"/"SO:*". CORRECTED (2026-07-01): AfterProcess's return type was
 *       previously guessed as void; the disassembly uses its return value,
 *       so it's actually bool, matching ReInitialize's earlier correction.
 *
 *   CONFIRMED DOC INACCURACY: docs/interfaces/proc_oacmd.md lists a "PR"
 *   command ("post-process / AfterProcess"). No such command exists --
 *   this pass read every string in ProcessOACmd's rodata blob directly and
 *   confirmed the complete real command table is exactly: LM, LD, CM, CD,
 *   AU, CL, CB:*, PT, SO:*, PC, KI, LA:* (12 commands, no more). AfterProcess
 *   is dispatched by "LA:*", not "PR".
 *
 *   CORRECTION (2026-07-01, found while reconstructing SO:/PT:/PC:): an
 *   earlier pass wrongly attributed CSTGMultisampleBankManager::
 *   CloseAllBankFiles to the "CL" prefix. Re-disassembling the region past
 *   "CL"'s match (which the earlier pass never actually looked at -- only
 *   inferred from relocation *order*, not the real control flow) found
 *   "CL" actually takes a UUID and closes ONE bank's PCM data files
 *   (ClosePCMDataFiles); the literal command "CB:*" is what closes ALL
 *   banks. docs/interfaces/proc_oacmd.md's "CL: Close all bank files"
 *   description is imprecise for the same reason.
 *
 *   Also found and fixed while working in this area: oa_heap.h's
 *   oa_heap_base() checked for a NULL CSTGHeapManager::sInstance, but every
 *   real call site (confirmed here and in the already-reconstructed
 *   CSTGKLMManager::AuthorizeMultisampleBank) guards a specific sentinel
 *   value, -44 (0xFFFFFFD4), not zero -- fixed in oa_heap.h.
 *
 *   LM/LD/CM/CD -- FULLY UNTANGLED AND CORRECTED (2026-07-01). An earlier
 *   pass modeled CM/CD as "close" commands (LoadBankMetaData+ReleaseBank for
 *   CM, plain ReleaseBank for CD) based on relocation *order* alone, without
 *   reading the actual per-branch bytes. Re-disassembling
 *   `.text+0xa2a0`-`.text+0xa4f8` in full found this was wrong: CM/CD are
 *   NOT "close" operations at all.
 *
 *     - "LM:<uuid>:<n1>:<n2>:<n3>" -> LoadMultisample(n1, n2!=0, (u8)n3, false)
 *     - "CM:<uuid>:<n1>:<n2>:<n3>" -> LoadMultisample(n1, n2!=0, (u8)n3, TRUE)
 *     - "LD:<uuid>:<n1>:<n2>:<n3>" -> LoadDrumSample(n1, n2!=0, (u8)n3, false)
 *     - "CD:<uuid>:<n1>:<n2>:<n3>" -> LoadDrumSample(n1, n2!=0, (u8)n3, TRUE)
 *
 *   I.e. "C" vs. "L" is a boolean modifier on the SAME two load operations
 *   (confirmed by the exact same relocated symbol, LoadMultisample or
 *   LoadDrumSample, appearing at both the "L" and "C" call sites, differing
 *   only in a hardcoded final-argument constant, 0 vs. 1) -- not a
 *   load/close pair as the command letters and docs/interfaces/proc_oacmd.md
 *   both suggested. What the boolean actually means (replace-in-place?
 *   force-reload? temporary vs. permanent?) is not determinable from the
 *   call site alone -- named `variant` here rather than guessed.
 *
 *   All four also share an identical special case, confirmed at every one
 *   of the four call sites: if the bank AccessBank finds is in a "reserved
 *   but not yet loaded" marker state (`*(int*)bank == -1`), first call
 *   LoadBankMetaData() on it. If that succeeds, retry the normal load (with
 *   the SAME `variant` constant for that command). If it fails,
 *   ReleaseBank(mgr, uuid) and report failure. If AccessBank finds nothing
 *   at all, report failure immediately, no special-casing.
 *
 *   The "two near-identical call sites per command" an earlier pass noticed
 *   via relocation order are exactly this: one site for the direct-load
 *   path, one for the retry-after-LoadBankMetaData path -- not actually
 *   duplicated/redundant logic, and not a mystery once the full control flow
 *   is read rather than inferred.
 *
 *   ProcessOACmd's full 1773 bytes have now been disassembled end to end;
 *   every command it recognizes is listed above. Any command not listed
 *   falls through to the "unrecognized" return, same as the real firmware's
 *   `printk("bad oa cmd %s\n", cmd)` path (the printk itself is not
 *   reproduced here).
 */

#include "process_oacmd.h"
#include "oa_types.h"
#include "oa_heap.h"
#include "oa_internal.h"   /* strlen */

extern "C" int sscanf(const char *buf, const char *fmt, ...);
extern "C" void PcmModuleMutexLock(void);
extern "C" void PcmModuleMutexUnlock(void);

/*
 * CSTGInstalledEXProducts registry singleton -- same heapbase+0x14 fixed
 * offset already established in products.cpp's AuthorizeProductCallback.
 */
static inline struct CSTGInstalledEXProducts *oa_installed_products(void)
{
	return (struct CSTGInstalledEXProducts *)(oa_heap_base() + 0x14);
}

/* CSTGPianoModel / CSTGPCMPrecacheManager: forward-declared (opaque) in
 * oa_types.h; completed here since this is their only current caller
 * ("PT:"/"PC:"). Bodies not yet reconstructed -- Stage 3/4. */
struct CSTGPianoModel {
	static char *sInstance;
	void RescanPianoTypes(void);
};

struct CSTGPCMPrecacheManager {
	/*
	 * Reset -- CONFIRMED-BUT-COUNTERINTUITIVE argument mapping. The real
	 * call site parses "PC:<n1>:<n2>:<n3>" then sets up the call as:
	 *   EAX = this, ECX = (n2 != 0), EDX = (n3 != 0), stack arg0 = n1
	 * Under -mregparm=3 for a member call, `this` takes EAX and the
	 * function's own parameters fill EDX then ECX then the stack in THAT
	 * order -- meaning Reset's real parameter order is (n3-derived flag,
	 * n2-derived flag, n1), not the (n1,n2,n3) order they appear in the
	 * command string. Reproduced exactly as confirmed, not "fixed" to the
	 * more intuitive order.
	 *
	 * Real body now reconstructed (sec 10.154, see
	 * src/init/setup_global_resources.cpp) -- also corrects the return
	 * type the SAME way AfterProcess()'s own comment below already did:
	 * confirmed `mov eax,0x1` before `ret`, so `bool`, not `void`. This
	 * call site already discards the return value either way.
	 */
	bool Reset(bool flagFromN3, bool flagFromN2, unsigned long n1);
	/* CORRECTED (2026-07-01): return type was guessed as void; "LA:*"'s
	 * disassembly uses the return value ((result^1)&0xff), so it's bool. */
	bool AfterProcess(void);
};

char *CSTGPianoModel::sInstance;

static inline bool prefix_is(const char *cmd, char c0, char c1)
{
	return cmd[0] == c0 && cmd[1] == c1;
}

static inline bool literal4_is(const char *cmd, const char *lit4)
{
	return cmd[0] == lit4[0] && cmd[1] == lit4[1] && cmd[2] == lit4[2] && cmd[3] == lit4[3];
}

int ProcessOACmd(const char *cmd, int *outResult)
{
	if (cmd[2] != ':' || strlen(cmd) <= 3) {
		*outResult = -1;
		return -1;
	}

	if (prefix_is(cmd, 'A', 'U')) {
		bool ok = oa_installed_products()->VerifyAndSaveAuthString(cmd + 3);
		*outResult = ok ? 0 : 1;
		return 0;
	}

	if (literal4_is(cmd, "CB:*")) {
		struct CSTGMultisampleBankManager *mgr = oa_multisample_bank_manager();
		CSTGMultisampleBankManager::CloseAllBankFiles(mgr);
		*outResult = 0;
		return 0;
	}

	if (literal4_is(cmd, "SO:*")) {
		bool ok = oa_installed_products()->ReInitialize();
		*outResult = ok ? 0 : 1;
		return 0;
	}

	if (prefix_is(cmd, 'P', 'T')) {
		((struct CSTGPianoModel *)CSTGPianoModel::sInstance)->RescanPianoTypes();
		*outResult = 0;
		return 0;
	}

	if (prefix_is(cmd, 'P', 'C')) {
		if (strlen(cmd) <= 7) {
			*outResult = -1;
			return -1;	/* real firmware: printk("pcm cmd PC bad format %s\n", cmd) */
		}
		unsigned long n1, n2, n3;
		if (sscanf(cmd + 2, ":%lu:%lu:%lu", &n1, &n2, &n3) != 3) {
			*outResult = -1;
			return -1;
		}
		oa_pcm_precache_manager()->Reset(n3 != 0, n2 != 0, n1);
		*outResult = 0;
		return 0;
	}

	if (prefix_is(cmd, 'C', 'L')) {
		if (strlen(cmd) != 0x27) {
			*outResult = -1;
			return -1;
		}
		struct CUUID uuid;
		if (!uuid.ConvertFromText(cmd + 3)) {
			*outResult = -1;
			return -1;
		}
		struct CSTGMultisampleBankManager *mgr = oa_multisample_bank_manager();
		void *bank = CSTGMultisampleBankManager::AccessBank(
			mgr, (const struct CSTGMultisampleBankUUID *)&uuid);
		if (bank) {
			PcmModuleMutexLock();
			((struct CSTGMultisampleBank *)bank)->ClosePCMDataFiles();
			PcmModuleMutexUnlock();
		}
		*outResult = bank ? 0 : 1;
		return 0;
	}

	if (prefix_is(cmd, 'K', 'I')) {
		unsigned long n1;
		if (sscanf(cmd + 2, ":%lu", &n1) != 1) {
			*outResult = -1;
			return -1;	/* real firmware: printk("pcm cmd KI bad format %s\n", cmd) */
		}
		/* heapbase+0x6a554 == CSTGPCMPrecacheManager base (heapbase+0x6a54c) + 8;
		 * almost certainly a field inside that object -- see file header. */
		*(unsigned long *)((char *)oa_pcm_precache_manager() + 8) = n1;
		*outResult = 0;
		return 0;
	}

	if (literal4_is(cmd, "LA:*")) {
		bool ok = oa_pcm_precache_manager()->AfterProcess();
		*outResult = ok ? 0 : 1;
		return 0;
	}

	bool isLM = prefix_is(cmd, 'L', 'M');
	bool isLD = prefix_is(cmd, 'L', 'D');
	bool isCM = prefix_is(cmd, 'C', 'M');
	bool isCD = prefix_is(cmd, 'C', 'D');

	if (isLM || isLD || isCM || isCD) {
		if (strlen(cmd) <= 0x2c) {
			*outResult = -1;
			return -1;
		}

		struct CUUID uuid;
		if (!uuid.ConvertFromText(cmd + 3)) {
			*outResult = -1;
			return -1;
		}

		unsigned long n1, n2, n3;
		if (sscanf(cmd + 0x27, ":%lu:%lu:%lu", &n1, &n2, &n3) != 3) {
			*outResult = -1;
			return -1;
		}

		struct CSTGMultisampleBankManager *mgr = oa_multisample_bank_manager();
		const struct CSTGMultisampleBankUUID *bankUuid =
			(const struct CSTGMultisampleBankUUID *)&uuid;
		void *bankPtr = CSTGMultisampleBankManager::AccessBank(mgr, bankUuid);

		if (!bankPtr) {
			*outResult = 1;
			return 0;
		}

		struct CSTGMultisampleBank *bank = (struct CSTGMultisampleBank *)bankPtr;

		/* "Reserved but not yet loaded" marker -- confirmed identical
		 * special case at all four call sites, see file header. */
		if (*(int *)bankPtr == -1) {
			if (!bank->LoadBankMetaData()) {
				CSTGMultisampleBankManager::ReleaseBank(mgr, bankUuid);
				*outResult = 1;
				return 0;
			}
		}

		bool variant = isCM || isCD;	/* "C" passes true, "L" passes false -- see file header */
		bool ok = (isLM || isCM)
			? bank->LoadMultisample(n1, n2 != 0, (unsigned char)n3, variant)
			: bank->LoadDrumSample(n1, n2 != 0, (unsigned char)n3, variant);
		*outResult = ok ? 0 : 1;
		return 0;
	}

	/* Unrecognized prefix -- real firmware also printk("bad oa cmd %s\n", cmd) here. */
	*outResult = -1;
	return -1;
}
