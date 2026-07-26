/*
 * config_info.cpp  -  see include/config_manager.h.
 *
 * SetConfigInfo() transcribed from the Ghidra decompile
 * (Decomp/EVA_Decomp/eva_export/functions/SetConfigInfo@0804cb70.c, 147 bytes) --
 * a pure table-of-13-pointers assignment, called from COmegaInterface::Init()
 * (src/init/omega_interface.cpp).
 *
 * The 13 real config tables these fields end up pointing at (s_atCreateInfo,
 * s_atFMDriverInfo, s_atConnectInfo, s_atEditServerInfo, s_atSysExModuleInfo,
 * s_atSysExConnectInfo, s_atSysExFilterInfo, s_atRTRouterInfo, s_atChunkInfo,
 * s_atResFamilyInfo, s_tSeqTimerInfo, s_ktVersionInfo, s_apkcSysVars) are large
 * config-metadata blobs -- genuinely out of scope for this pass in general
 * (nothing on the traced boot path dereferences most of them), EXCEPT
 * s_atCreateInfo (see the "CreateUserModules() unlock" note below, this pass's
 * whole point) and s_tSeqTimerInfo (already given real, non-zero defaults by an
 * earlier pass -- see that placeholder's own comment). Each of the rest is
 * declared below as an opaque, zero-initialized placeholder so the
 * *assignment* is real and SetConfigInfo() compiles, without claiming the
 * table *contents* are faithful. Sizes are the real observed byte deltas to
 * the next confirmed symbol in symbols.csv where that neighbor is clearly part
 * of the same contiguous table run; marked "size unconfirmed" (rounded
 * placeholder) where the next label was too far away to trust as a real
 * boundary.
 *
 * One real, if incidental, structural finding: eleven of these tables live packed
 * together in one 0x091adxxx/0x091aexxx rodata region (a real, compiler-emitted
 * table-of-config-tables); s_atSysExConnectInfo/s_atSysExFilterInfo/s_atRTRouterInfo
 * and s_ktVersionInfo live in two entirely separate regions (0x09304fxx and
 * 0x08e79axx respectively) -- not investigated further, not needed for the boot path.
 *
 * ============================================================================
 * BUG FOUND + FIXED (2026-07-25, "why is CreateUserModules() a no-op" pass):
 *
 * SetConfigInfo()'s own real disassembly (.text+0x0804cb70) copies 13 dwords
 * FROM Eva's own .data at 0x091ad9e0 (nm: local symbol `s_tConfigInfo`, a
 * 0x40-byte, 13-pointer-plus-padding table SetConfigInfo() reads *from* at
 * compile time) INTO the 13 CConfigManager::sm_pt* static fields. The FIRST of
 * those 13 dwords (read from 0x091ad9e0 itself) is 0x091ada20 -- the address of
 * a SEPARATE real symbol, `s_atCreateInfo` (nm-confirmed, 0xb4 bytes, real
 * boundary to the next symbol `s_atFMDriverInfo` at 0x091adad4) -- i.e.
 * sm_ptCreateInfo's real value is `&s_atCreateInfo`, NOT `&s_tConfigInfo`.
 *
 * The previous placeholder here (`s_tConfigInfo_placeholder[4]`) pointed
 * sm_ptCreateInfo at a stand-in for `s_tConfigInfo` itself (the SOURCE
 * pointer-table SetConfigInfo() reads FROM, an address CreateUserModules()
 * never touches at all) instead of at `s_atCreateInfo` (the actual per-module
 * factory table CreateUserModules() walks) -- a genuine address mixup between
 * "the pointer variable" and "what it points to", not a scaling/rounding slip.
 * That 4-byte all-zero stand-in's own first (only) field was 0, so
 * `CreateUserModules()`'s `while (entry->name != 0)` loop condition was false
 * on its very first check -- a real, but accidental, no-op (right conclusion,
 * wrong reason: it looked like "config data legitimately empty" when it was
 * actually "pointed at the wrong 4 bytes entirely").
 *
 * Fixed: sm_ptCreateInfo now points at a properly-named, properly-addressed
 * `s_atCreateInfo_placeholder`, populated with the REAL 14-entry table --
 * every {name, param1, param2} triple below is transcribed byte-for-byte from
 * Eva's own .data (091ada20..091adad4) and .rodata (each string chased and
 * read directly), not fabricated. Verified: the table's own real byte length
 * (0xb4 = 15 * 12) exactly matches 14 real entries + a null-name terminator
 * row, and every one of the 14 names matches a real, already-registered
 * mConstructors entry (mains.cpp's own RegisterModuleDescriptor() calls,
 * Mains() -- confirmed by cross-reference, not assumed).
 *
 * IMPORTANT SAFETY COROLLARY: giving CreateUserModules() this real, non-empty
 * table means it now genuinely walks all 14 entries and dereferences each
 * found factory's own vtable+8 ("Create") slot. mains.cpp's own 15
 * PTR__CXxxConstructor vtables had to be upgraded from bare scalar `void*`
 * placeholders (address-of-a-lone-4-byte-global "vtable", safe ONLY while this
 * table stayed empty) to properly-sized, dispatch-safe 3-slot arrays before
 * this fix could be applied -- see mains.cpp's own updated header comment.
 * Only "EditorClass" (CEditorConstructor) does real work (placement-constructs
 * a real CEditor); the other 13 entries' own model classes are not
 * reconstructed in this project and route through a shared, safe "return NULL"
 * stub -- CreateUserModules() already has a real, ground-truth-faithful
 * "unable to create instance of user module <%s>" warning path for exactly
 * this outcome, so those 13 fail loudly but safely rather than being silently
 * dropped from the table (which would be less faithful than transcribing them
 * with a stub factory).
 * ============================================================================
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

/* Real per-entry layout shared by s_atCreateInfo (CreateUserModules()) and
 * s_atFMDriverInfo (CreateFMDrivers()) -- {name, param1, param2} triples,
 * terminated by a null name. Matches config_manager.cpp's own (anonymous-
 * namespace, file-local) `CreateInfoEntry` -- redeclared here rather than
 * shared via the header since both are plain 3-pointer PODs with identical
 * layout and neither namespace is visible outside its own TU.
 */
struct CreateModuleEntry {
	const char *name;
	const char *param1;
	const char *param2;
};

/* .text+0x091ada20, real size confirmed 0xb4 (180 = 15 * 12) via delta to
 * s_atFMDriverInfo (0x091adad4) -- 14 real entries + 1 null-name terminator
 * row, byte-verified against Eva's own .data/.rodata (see this file's header
 * comment for the bug this corrects). Every string below was read directly
 * from the addresses Eva's own .data table points at:
 *   row 0: name=0x08e79794 "BatchDiskManClass"
 *          param1=0x08e797a6 "BatchDiskMan"
 *          param2=0x08e79a24 "PRELOAD=EditResources.Unlocalized;EditResources.Localized.ENG"
 *   row 1: name=0x08e797b3 "PanelClass"
 *          param1=0x08f24f78 "Panel"
 *          param2=0x08e797be "PANELDRV=PanelDriver"
 *   row 2: name=0x08e797d3 "EditorClass"
 *          param1=0x08f777b8 "Editor"
 *          param2=0x08e797df "ALPHAKEYBOARD=Yes"          <- the entry this pass exists for
 *   row 3: name=0x08e797f1 "ProgEditServer",     param1=0x08e79800 "ESProg",     param2=0
 *   row 4: name=0x08e79807 "EffectEditServer",   param1=0x08e79818 "ESEffect",   param2=0
 *   row 5: name=0x08e79821 "CombiEditServer",    param1=0x08e79831 "ESCombi",    param2=0
 *   row 6: name=0x08e79839 "GlobalEditServer",   param1=0x08e7984a "ESGlobal",   param2=0
 *   row 7: name=0x08e79853 "MOSSEditServer",     param1=0x08e79862 "ESMOSS",     param2=0
 *   row 8: name=0x08e79869 "SamplingEditServer", param1=0x08e7987c "ESSampling", param2=0
 *   row 9: name=0x08e79887 "SongEditServer",     param1=0x08e79896 "ESSong",     param2=0
 *   row 10: name=0x08e7989d "DiskEditServer",    param1=0x08e798ac "ESDisk",     param2=0
 *   row 11: name=0x08e798b3 "CommonEditServer",  param1=0x08e798c4 "ESCommon",   param2=0
 *   row 12: name=0x08e798cd "SetListEditServer", param1=0x08e798df "ESSetList",  param2=0
 *   row 13: name=0x08e798e9 "AlphaKeybCtrlClass"
 *           param1=0x08e798fc "AlphaKeybCtrl"
 *           param2=0x08e7990a "HIDDRV=HIDDriver"
 *   row 14: {0, 0, 0} -- real terminator
 * Every one of these 14 `name`s matches a real, already-registered
 * mConstructors entry (mains.cpp's RegisterModuleDescriptor() calls from
 * Mains()) -- cross-checked, not assumed.
 */
CreateModuleEntry s_atCreateInfo_placeholder[15] = {
	{ "BatchDiskManClass", "BatchDiskMan", "PRELOAD=EditResources.Unlocalized;EditResources.Localized.ENG" },
	{ "PanelClass",        "Panel",        "PANELDRV=PanelDriver" },
	{ "EditorClass",       "Editor",       "ALPHAKEYBOARD=Yes" },
	{ "ProgEditServer",    "ESProg",       0 },
	{ "EffectEditServer",  "ESEffect",     0 },
	{ "CombiEditServer",   "ESCombi",      0 },
	{ "GlobalEditServer",  "ESGlobal",     0 },
	{ "MOSSEditServer",    "ESMOSS",       0 },
	{ "SamplingEditServer","ESSampling",   0 },
	{ "SongEditServer",    "ESSong",       0 },
	{ "DiskEditServer",    "ESDisk",       0 },
	{ "CommonEditServer",  "ESCommon",     0 },
	{ "SetListEditServer", "ESSetList",    0 },
	{ "AlphaKeybCtrlClass","AlphaKeybCtrl","HIDDRV=HIDDriver" },
	{ 0, 0, 0 },
};

/* .text+0x091adad4, real size confirmed 0x2c (44) via delta to s_atConnectInfo.
 * Real content, byte-read (not zeroed like this pass's other placeholders):
 * ONE real entry -- name="LinuxDriver", param1="Nand",
 * param2="Unit=K, RegisterUnits=Yes, MaxNumOfPartitions=0, MappedInto=/korg..."
 * (string continues past what this pass chased). Deliberately left DISABLED
 * (sm_ptFMDriverInfo assigned 0, below in SetConfigInfo()) rather than wired
 * up: unlike CreateUserModules()'s target (Api's slot +0x40 ->
 * CModuleManager::AddConstructor(), already real), CreateFMDrivers() looks its
 * factory up through FMApi's own vtable slot +0x2c ("get named driver
 * factory") -- genuinely unreconstructed, out of scope for this pass. Recorded
 * here for whoever picks that up next rather than silently left unexplained.
 */
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

/* .text+0x091ae7b0, real size confirmed 0x10 (16) via delta to s_apkcSysVars.
 * Real layout (confirmed field-by-field from ConfigureSeqTimer@08056ed0.c,
 * config_manager.cpp): 4 dwords -- {mode, lowerLimitBpm, upperLimitBpm,
 * wheelTablePtr}. UNLIKE this file's other placeholders (safe to leave all-zero
 * because their own first field alone gates whether the real loop body ever
 * runs), this one is NOT safe to zero: ConfigureSeqTimer() unconditionally
 * dereferences field 3 as a pointer (`*(int*)sm_ptSeqTimerInfo[3]`) with no
 * NULL check, and unconditionally passes fields 1/2 to BPM::SetLowerLimit()/
 * SetUpperLimit() (tempo.cpp), which divide by them with no zero-guard --
 * both are real, faithfully-preserved hazards in the real binary too (a real
 * Kronos always has non-zero, well-formed config data here; only THIS
 * reconstruction's own placeholder convention risked introducing an artificial
 * crash). Given sane, real-constant-matching non-zero defaults instead: 40/240
 * BPM (matching BPM::sm_LowerLimit's OWN real static-data initial value of 40,
 * tempo.cpp, and MPQN's real static-ctor pairing of 250000/1500000us <->
 * 240/40 BPM) and a valid pointer to a zeroed 2-dword (id=0-terminated) wheel
 * sub-table instead of NULL. mode=0 (falls to ConfigureSeqTimer()'s real
 * "default timer type" else-branch).
 */
unsigned int s_atSeqTimerWheelTable_placeholder[2] = {};
unsigned int s_tSeqTimerInfo_placeholder[4] = {
	0,   /* mode: 0 = default timer type */
	40,  /* lowerLimitBpm */
	240, /* upperLimitBpm */
	(unsigned int)(unsigned long)s_atSeqTimerWheelTable_placeholder,
};

/* .text+0x08e79ab0 -- entirely separate region; size unconfirmed, rounded placeholder. */
unsigned char s_ktVersionInfo_placeholder[16] = {};

/* .text+0x091ae7c0 -- no confirmed next-symbol boundary; size unconfirmed, rounded
 * placeholder.
 */
unsigned char s_apkcSysVars_placeholder[16] = {};

} // namespace

void SetConfigInfo(void)
{
	CConfigManager::sm_ptCreateInfo = (void *)s_atCreateInfo_placeholder;
	CConfigManager::sm_ptFMDriverInfo = 0; /* real table exists (s_atFMDriverInfo_placeholder
	                                         * above) but stays disabled -- see that
	                                         * placeholder's own comment. */
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
