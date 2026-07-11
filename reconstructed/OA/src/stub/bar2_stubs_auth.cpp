// SPDX-License-Identifier: GPL-2.0
/*
 * bar2_stubs_auth.cpp -- Bar 2 (2026-07-02): see bar2_stubs.cpp's own
 * file header for the full rationale. Split into a SEPARATE
 * translation unit from bar2_stubs.cpp specifically because these
 * symbols only have declarations behind `auth.h` (this project's
 * "minimal" `oa_types.h`-based declaration ecosystem, sec 10.48),
 * which cannot be included in the same TU as `oa_global.h`/
 * `oa_engine.h`'s own fuller, incompatible declarations of several
 * shared class names.
 *
 * NOTE: CSTGMultisampleBankManager::Initialize() is deliberately NOT
 * stubbed here -- it's declared in a THIRD, separately incompatible
 * header (oa_setup_global_resources.h), so its stub lives in
 * bar2_stubs.cpp instead, matching that header's own ecosystem.
 */

#include "auth.h"

void CSTGMultisampleBank::ClosePCMDataFiles() {}
bool CSTGMultisampleBank::LoadBankMetaData() { return false; }
bool CSTGMultisampleBank::LoadDrumSample(unsigned long, bool, unsigned char, bool) { return false; }
bool CSTGMultisampleBank::LoadMultisample(unsigned long, bool, unsigned char, bool) { return false; }
void *CSTGMultisampleBankManager::AccessBank(CSTGMultisampleBankManager *, const CSTGMultisampleBankUUID *) { return 0; }
void *CSTGMultisampleBankManager::AccessBankWithLegacyRAMAlias(CSTGMultisampleBankManager *, const CSTGMultisampleBankUUID *) { return 0; }
void CSTGMultisampleBankManager::ReleaseBank(CSTGMultisampleBankManager *, const CSTGMultisampleBankUUID *) {}
void CSTGMultisampleBankManager::CloseAllBankFiles(CSTGMultisampleBankManager *) {}
/* StartupInitializeROMBank/StartupInitializeRAMBank/ScanFileSystem (batch
 * 52) -- confirmed real, called from load_global_resources() (init_module
 * step 11, src/init/load_global_resources.cpp), deliberately deferred
 * here per the sec 10.185 policy (genuine ROM-bank/filesystem-scan
 * subsystem, out of scope) -- see that file's own header comment for the
 * full derivation and why StartupInitializeRAMBank() returns `true`
 * (its own real return value IS checked by the caller, gating a genuine
 * hard-fail path; a safe "succeeded" default lets init proceed). */
void CSTGMultisampleBankManager::StartupInitializeROMBank(const char *, bool, unsigned char) {}
bool CSTGMultisampleBankManager::StartupInitializeRAMBank() { return true; }
void CSTGMultisampleBankManager::ScanFileSystem() {}

CSTGKLEG::CSTGKLEG() {}
void CSTGKLEG::Initialize(CSTGKLMManager *) {}
void *CSTGKLEG::Run() { return 0; }

void *CSTGPatch::GetUpComponent() { return 0; }
bool CSTGPatch::IsUsingAnyUnauthorizedMultisamples() { return false; }

bool CUUID::ConvertFromText(const char *) { return false; }

bool CSTGInstalledEXProducts::ReInitialize() { return true; }
/* Initialize() (batch 52) -- confirmed real, called from
 * load_global_resources() (init_module step 11). Its own real failure
 * path is confirmed SOFT (a bare printk, no hard-fail return -- see
 * load_global_resources.cpp), so `true` is the faithful-enough safe
 * default either way; deliberately deferred per the sec 10.185 policy,
 * same rationale as ReInitialize() above. */
bool CSTGInstalledEXProducts::Initialize() { return true; }
