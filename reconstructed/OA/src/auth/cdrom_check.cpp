// SPDX-License-Identifier: GPL-2.0
/*
 * cdrom_check.cpp  -  see include/cdrom_check.h.
 *
 * Ground-truthed against the real OA.ko 3.2.1 ELF symbol table (re-disassembled
 * from scratch 2026-07-01, correcting an earlier, wrong version of this file
 * that invented a fictional "init_cdrom_command" and a placeholder
 * "g_pCdromDrvInfo" global -- see include/cdrom_check.h):
 *
 *   InitCdromSupport   .text+0x000040   (149 bytes)
 *
 * Full sequence, byte-for-byte from the disassembly:
 *
 *   1. Build a zeroed, throwaway 144-byte fake `struct cdrom_device_info` on
 *      the stack. Field +0x00 (the `ops` pointer in the real kernel struct)
 *      is pointed at a second, separately-zeroed 60-byte scratch block
 *      within the same buffer -- register_cdrom() presumably dereferences
 *      it, so it can't be null, but its contents don't otherwise matter
 *      here. Field +0x10 is set to a magic marker, 0xA0F3.
 *   2. Call the REAL Linux kernel API register_cdrom(&fake_struct)
 *      (drivers/cdrom/cdrom.c). The Korg-patched kernel recognizes the
 *      0xA0F3 marker and answers with a fixed sentinel, -42 (0xFFFFFFD6),
 *      instead of a real registration result. Any other return (stock
 *      kernel, or loadmod.ko's hook not installed) -> return -1 immediately.
 *   3. On the -42 sentinel: read back field +0x48 of the same struct
 *      (register_cdrom's hook wrote something there as a covert
 *      out-parameter) and subtract a fixed magic constant, 0x02D5B9C3, to
 *      recover a real pointer. Store that pointer into BOTH `sXCmd` and
 *      `sCdromCommand` (real confirmed globals, shared with
 *      process_oacmd.cpp's oa_cmd_* fops and ProcessOACmd's "AU:" path).
 *   4. If the recovered pointer is NULL, return a nonzero failure value
 *      (0xFF, preserved as-is -- CSTGEngine::Initialize, the caller, only
 *      ever checks for exactly 0, so any nonzero reads as failure).
 *   5. Otherwise, dereference pointer+5 as a dword and compare against a
 *      second magic value, 0x22FB39CC. Return 0 on match (success), -1 on
 *      mismatch.
 */

#include "cdrom_check.h"

#define CDROM_MAGIC_FIELD_OFFSET   0x10
#define CDROM_MAGIC_FIELD_VALUE    0xa0f3u
#define KORG_KERNEL_MAGIC_RETURN   (-42)		/* 0xFFFFFFD6 */
#define CDROM_PTR_RECOVER_DELTA    0x02d5b9c3u
#define OA_CDROM_MAGIC_DWORD       0x22fb39ccu

/* Real Linux kernel API (drivers/cdrom/cdrom.c) -- Korg's patched kernel
 * hijacks it as a covert integrity-check channel, not for an actual drive. */
extern "C" int register_cdrom(void *cdrom_device_info);

/* Confirmed real globals (not placeholders), shared with process_oacmd.cpp. */
extern "C" void *sXCmd;
extern "C" void *sCdromCommand;

int InitCdromSupport(void)
{
	unsigned char fakeDevInfo[144] = {0};

	*(void **)(fakeDevInfo + 0x00) = fakeDevInfo + 0x54;	/* ops field: non-null scratch */
	*(unsigned int *)(fakeDevInfo + CDROM_MAGIC_FIELD_OFFSET) = CDROM_MAGIC_FIELD_VALUE;

	int rc = register_cdrom(fakeDevInfo);
	if (rc != KORG_KERNEL_MAGIC_RETURN)
		return -1;

	unsigned int written = *(unsigned int *)(fakeDevInfo + 0x48);
	void *recovered = (void *)(written - CDROM_PTR_RECOVER_DELTA);
	sCdromCommand = recovered;
	sXCmd = recovered;

	if (!recovered)
		return 0xff;

	unsigned int magic = *(const unsigned int *)((const unsigned char *)recovered + 5);
	return (magic == OA_CDROM_MAGIC_DWORD) ? 0 : -1;
}

/* .data globals CSTGEngine::Initialize's degradation block writes; normally
 * 1.0f / -1.0f (four stereo-channel-group pairs) and 0 respectively. Defined
 * here until Stage 3 (CSTGEngine) reconstructs the translation unit that owns
 * them for real. */
extern "C" float        allPlusOne[4];
extern "C" float        allMinusOne[4];
extern "C" unsigned int kAudXBZD;

void OA_ApplyCdromDegradation(void)
{
	for (int i = 0; i < 4; i++) {
		allPlusOne[i]  =  0.7f;	/* 0x3f333333 */
		allMinusOne[i] = -0.2f;	/* 0xbe4ccccd */
	}
	kAudXBZD = 0x1f;	/* protection-mode flag, checked elsewhere in the DSP path */
}
