/*
 * config_info.cpp  -  see include/config_manager.h.
 *
 * SetConfigInfo() transcribed from the Ghidra decompile
 * (Decomp/EVA_Decomp/eva_export/functions/SetConfigInfo@0804cb70.c, 147 bytes) --
 * a pure table-of-13-pointers assignment, called from COmegaInterface::Init()
 * (src/init/omega_interface.cpp).
 *
 * The 13 real config tables these fields end up pointing at (s_tConfigInfo,
 * s_atFMDriverInfo, s_atConnectInfo, s_atEditServerInfo, s_atSysExModuleInfo,
 * s_atSysExConnectInfo, s_atSysExFilterInfo, s_atRTRouterInfo, s_atChunkInfo,
 * s_atResFamilyInfo, s_tSeqTimerInfo, s_ktVersionInfo, s_apkcSysVars) are large
 * config-metadata blobs -- genuinely out of scope for this pass (nothing on the
 * traced boot path dereferences any of them; see PLAN.md's boot-path scope). Each is
 * declared below as an opaque, zero-initialized placeholder so the *assignment* is
 * real and SetConfigInfo() compiles, without claiming the table *contents* are
 * faithful. Sizes are the real observed byte deltas to the next confirmed symbol in
 * symbols.csv where that neighbor is clearly part of the same contiguous table run;
 * marked "size unconfirmed" (rounded placeholder) where the next label was too far
 * away to trust as a real boundary.
 *
 * One real, if incidental, structural finding: eleven of these tables live packed
 * together in one 0x091adxxx/0x091aexxx rodata region (a real, compiler-emitted
 * table-of-config-tables); s_atSysExConnectInfo/s_atSysExFilterInfo/s_atRTRouterInfo
 * and s_ktVersionInfo live in two entirely separate regions (0x09304fxx and
 * 0x08e79axx respectively) -- not investigated further, not needed for the boot path.
 */

#include "config_manager.h"

void *CConfigManager::sm_ptCreateInfo = 0;
void *CConfigManager::sm_ptFMDriverInfo = 0;
void *CConfigManager::sm_ptConnectInfo = 0;
void *CConfigManager::sm_ptEditServerInfo = 0;
void *CConfigManager::sm_ptSysExModuleInfo = 0;
void *CConfigManager::sm_ptSysExConnectInfo = 0;
void *CConfigManager::sm_ptSysExFilterInfo = 0;
void *CConfigManager::sm_ptRTRouterInfo = 0;
void *CConfigManager::sm_ptChunkInfo = 0;
void *CConfigManager::sm_ptResFamilyInfo = 0;
void *CConfigManager::sm_ptSeqTimerInfo = 0;
void *CConfigManager::sm_pktVersionInfo = 0;
void *CConfigManager::sm_apkcSysVars = 0;

namespace {

/* .text+0x091ad9e0 -- next symbol (the PTR_s_atXxx pointer-table run below) starts
 * 4 bytes later; real size confirmed as 4.
 */
unsigned char s_tConfigInfo_placeholder[4] = {};

/* .text+0x091adad4, real size confirmed 0x2c (44) via delta to s_atConnectInfo. */
unsigned char s_atFMDriverInfo_placeholder[0x2c] = {};

/* .text+0x091adb00, real size confirmed 0x40 (64) via delta to s_atEditServerInfo. */
unsigned char s_atConnectInfo_placeholder[0x40] = {};

/* .text+0x091adb40, real size confirmed 0x70 (112) via delta to s_atSysExModuleInfo. */
unsigned char s_atEditServerInfo_placeholder[0x70] = {};

/* .text+0x091adbb0, real size confirmed 0x30 (48) via delta to s_atChunkInfo. */
unsigned char s_atSysExModuleInfo_placeholder[0x30] = {};

/* .text+0x09304f78, real size confirmed 0xc (12) via delta to s_atSysExFilterInfo. */
unsigned char s_atSysExConnectInfo_placeholder[0xc] = {};

/* .text+0x09304f84, real size confirmed 8 via delta to s_atRTRouterInfo. */
unsigned char s_atSysExFilterInfo_placeholder[8] = {};

/* .text+0x09304f8c -- no confirmed next-symbol boundary; size unconfirmed, rounded
 * placeholder matching the pattern of its two immediate neighbors above.
 */
unsigned char s_atRTRouterInfo_placeholder[8] = {};

/* .text+0x091adbe0, real size confirmed 0x40 (64) via delta to s_atResFamilyInfo. */
unsigned char s_atChunkInfo_placeholder[0x40] = {};

/* .text+0x091adc20 -- no confirmed next-symbol boundary; size unconfirmed, rounded
 * placeholder.
 */
unsigned char s_atResFamilyInfo_placeholder[64] = {};

/* .text+0x091ae7b0, real size confirmed 0x10 (16) via delta to s_apkcSysVars. */
unsigned char s_tSeqTimerInfo_placeholder[0x10] = {};

/* .text+0x08e79ab0 -- entirely separate region; size unconfirmed, rounded placeholder. */
unsigned char s_ktVersionInfo_placeholder[16] = {};

/* .text+0x091ae7c0 -- no confirmed next-symbol boundary; size unconfirmed, rounded
 * placeholder.
 */
unsigned char s_apkcSysVars_placeholder[16] = {};

} // namespace

void SetConfigInfo(void)
{
	CConfigManager::sm_ptCreateInfo = s_tConfigInfo_placeholder;
	CConfigManager::sm_ptFMDriverInfo = s_atFMDriverInfo_placeholder;
	CConfigManager::sm_ptConnectInfo = s_atConnectInfo_placeholder;
	CConfigManager::sm_ptEditServerInfo = s_atEditServerInfo_placeholder;
	CConfigManager::sm_ptSysExModuleInfo = s_atSysExModuleInfo_placeholder;
	CConfigManager::sm_ptSysExConnectInfo = s_atSysExConnectInfo_placeholder;
	CConfigManager::sm_ptSysExFilterInfo = s_atSysExFilterInfo_placeholder;
	CConfigManager::sm_ptRTRouterInfo = s_atRTRouterInfo_placeholder;
	CConfigManager::sm_ptChunkInfo = s_atChunkInfo_placeholder;
	CConfigManager::sm_ptResFamilyInfo = s_atResFamilyInfo_placeholder;
	CConfigManager::sm_ptSeqTimerInfo = s_tSeqTimerInfo_placeholder;
	CConfigManager::sm_pktVersionInfo = s_ktVersionInfo_placeholder;
	CConfigManager::sm_apkcSysVars = s_apkcSysVars_placeholder;
}
