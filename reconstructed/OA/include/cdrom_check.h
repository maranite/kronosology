// SPDX-License-Identifier: GPL-2.0
/*
 * cdrom_check.h  -  InitCdromSupport: the loadmod.ko presence/integrity check.
 *
 * OA.ko refuses to run at full fidelity unless loadmod.ko is loaded and has
 * installed a specific kernel hook + magic value.  InitCdromSupport() is the
 * probe; CSTGEngine::Initialize (.text+0x01B0, ground-truthed, not yet
 * reconstructed -- Stage 3) calls it once during engine bring-up and, on
 * failure, runs a degradation block instead of aborting -- see
 * OA_ApplyCdromDegradation() below, which reproduces that block verbatim so
 * Stage 3 can call it once CSTGEngine's translation unit exists:
 *
 *   if (InitCdromSupport() != 0)
 *           OA_ApplyCdromDegradation();
 *
 * Ground-truthed against the real OA.ko 3.2.1 ELF symbol table:
 *   InitCdromSupport            .text+0x000040
 *   CSTGEngine::Initialize      .text+0x0001b0
 * (both already correct in OA.ko_auth.md's "text+0xNNN" notation -- these two
 * are the ones that were NOT hit by the +0x10000 Ghidra-address bug documented
 * in MASTER_REFERENCE.md sec 9.2).
 *
 * CORRECTED (2026-07-01, re-disassembled from scratch while reconstructing
 * process_oacmd.cpp): the earlier version of cdrom_check.cpp invented a
 * fictional 3-argument "init_cdrom_command" function and a placeholder
 * global "g_pCdromDrvInfo". Neither exists. The function actually called,
 * confirmed via relocation, is the REAL Linux kernel API
 * `register_cdrom(struct cdrom_device_info *)` -- Korg's patched kernel
 * hijacks this legitimate single-argument API as a covert integrity-check
 * channel. The recovered pointer is stored into the real globals `sXCmd`
 * and `sCdromCommand` (both set identically) -- see cdrom_check.cpp for the
 * full corrected sequence, including a pointer-recovery subtraction step
 * that was missing entirely from the earlier version.
 */

#ifndef OA_CDROM_CHECK_H
#define OA_CDROM_CHECK_H

/* Returns 0 if loadmod.ko is present and its magic dword matches, -1 otherwise. */
int InitCdromSupport(void);

/* Reproduces the audible degradation CSTGEngine::Initialize applies when
 * InitCdromSupport() fails: an asymmetric IIR amplitude-smoother pair
 * (0.7f/-0.2f instead of 1.0f/-1.0f, one pair per stereo channel group) that
 * produces a cyclic volume fade, plus a protection-mode flag. */
void OA_ApplyCdromDegradation(void);

#endif /* OA_CDROM_CHECK_H */
