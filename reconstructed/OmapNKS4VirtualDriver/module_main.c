// SPDX-License-Identifier: GPL-2.0
/*
 * module_main.c  -  OmapNKS4VirtualDriver.ko: a stand-in for
 * OmapNKS4Module.ko's own 6 exported symbols, for VM/foreign-hardware
 * boot testing only.
 *
 * WHY THIS EXISTS (Bar 2, 2026-07-02): OA.ko's own
 * setup_global_resources()/CSTGGlobal::UpdateXXX code calls 7 real
 * OmapNKS4Module.ko exports directly (COmapNKS4Driver_GetHardwareVersion/
 * GetOmapVersion/GetPSocVersion/Is88Key/SetTestMode,
 * OmapNKS4OutputFifo_WriteCommand, SetupNKS4Calibration -- CORRECTED
 * 2026-07-04, was previously missing SetTestMode entirely, a genuine
 * gap that would have surfaced as a real "Unknown symbol in module"
 * kernel error the moment OA.ko's own SetNKS4TestModeFlag path ran).
 * The REAL OmapNKS4Module.ko (already 100%
 * reconstructed, see reconstructed/OmapNKS4Module/) can't be used to
 * satisfy this in a VM: its own real init_module() hard-requires a real
 * RTAI SRQ AND blocks waiting for a genuine USB NKS4 front-panel board to
 * probe() -- it will never return in kronos_vm, so its EXPORT_SYMBOLs
 * would never become available. This is the exact same situation
 * AT88VirtualChip.ko/KorgUsbAudioVirtualDriver.ko were already built to
 * solve for their own real modules -- same pattern applied a third time.
 *
 * Deliberately plain C (not C++), matching AT88VirtualChip/module_main.c's
 * own precedent and reasoning: this ancient kernel's headers don't parse
 * as C++ under a modern g++, and none of this needs any real C++ feature.
 *
 * Deliberately trivial stub bodies -- NOT a claim these are reconstructed
 * NKS4/OMAP hardware-access implementations. Returns are chosen to be
 * "safe/inert" for OA.ko's own confirmed call sites (see
 * reconstructed/OA/src/init/setup_global_resources.cpp lines ~170-220 and
 * global.cpp's OmapNKS4OutputFifo_WriteCommand call): version queries
 * return 0, Is88Key returns 0 (not an 88-key model), WriteCommand/
 * SetupNKS4Calibration are no-ops.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

/*
 * Diagnostic addition (2026-07-03): this module previously had ZERO
 * module_param()s, unlike every other precedent module in this project
 * (AT88VirtualChip.ko has one). A real kernel Oops was observed inside
 * sys_init_module() itself (before this module's own init code even
 * ran) specifically when loading this module -- testing whether the
 * "zero kernel params" case trips a latent bug in this kernel's own
 * (possibly Korg/RTAI-patched) module-parameter handling.
 */
static int debug_flag;
module_param(debug_flag, int, 0444);
MODULE_PARM_DESC(debug_flag, "Unused diagnostic flag (Bar 2 crash investigation)");

/*
 * CORRECTED (2026-07-04): these two take TWO OUTPUT POINTER params and
 * return void, NOT a packed unsigned int -- a real signature mismatch
 * found and fixed across this whole project's own reconstructions of
 * this pair (OA's own caller, this stub, and OA's own header all had
 * it wrong the same way). See reconstructed/OA/include/
 * oa_setup_global_resources.h for the full confirmed shape.
 */
void COmapNKS4Driver_GetOmapVersion(unsigned char *version, unsigned char *revision)
{ *version = 0; *revision = 0; }
void COmapNKS4Driver_GetPSocVersion(unsigned char *version, unsigned char *revision)
{ *version = 0; *revision = 0; }
unsigned char COmapNKS4Driver_GetHardwareVersion(void) { return 0; }
int COmapNKS4Driver_Is88Key(void) { return 0; }
/* CORRECTED (2026-07-04): was entirely missing from this stub -- a
 * genuine gap that would show up as a real "Unknown symbol in module"
 * kernel error the moment OA.ko's own SetNKS4TestModeFlag path ran.
 * Parameter type is `int`, not `bool`, matching OmapNKS4Module's own
 * real implementation. */
void COmapNKS4Driver_SetTestMode(int testMode) { (void)testMode; }
/* CORRECTED (2026-07-04): the real function (a ring-buffer FIFO write,
 * confirmed via OmapNKS4Module.ko's own disassembly) returns int
 * (success/failure), not void. Harmless for OA.ko's own caller, which
 * discards the result either way, but corrected for accuracy. */
int OmapNKS4OutputFifo_WriteCommand(int command) { (void)command; return 1; }
/* CORRECTED (2026-07-04): real function takes TWO params (panel,
 * flag), not one -- confirmed via OA.ko's own call site, which always
 * passes a literal 0 for the second. See oa_setup_global_resources.h
 * for the full confirmed shape. */
void SetupNKS4Calibration(void *panel, int flag) { (void)panel; (void)flag; }

static int __init OmapNKS4VirtualDriverInit(void)
{
	printk(KERN_INFO "OmapNKS4VirtualDriver: loading (stand-in for "
	       "OmapNKS4Module.ko's 7 exports -- see this file's own header)\n");
	return 0;
}

static void __exit OmapNKS4VirtualDriverExit(void)
{
	printk(KERN_INFO "OmapNKS4VirtualDriver: unloaded\n");
}

module_init(OmapNKS4VirtualDriverInit);
module_exit(OmapNKS4VirtualDriverExit);

EXPORT_SYMBOL(COmapNKS4Driver_GetOmapVersion);
EXPORT_SYMBOL(COmapNKS4Driver_GetPSocVersion);
EXPORT_SYMBOL(COmapNKS4Driver_GetHardwareVersion);
EXPORT_SYMBOL(COmapNKS4Driver_Is88Key);
EXPORT_SYMBOL(COmapNKS4Driver_SetTestMode);
EXPORT_SYMBOL(OmapNKS4OutputFifo_WriteCommand);
EXPORT_SYMBOL(SetupNKS4Calibration);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Stand-in for OmapNKS4Module.ko's 7 OA.ko-referenced exports (VM/foreign-hardware boot testing only)");
MODULE_AUTHOR("Korg (reconstructed)");
