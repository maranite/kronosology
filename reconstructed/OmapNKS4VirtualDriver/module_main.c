// SPDX-License-Identifier: GPL-2.0
/*
 * module_main.c  -  OmapNKS4VirtualDriver.ko: a stand-in for
 * OmapNKS4Module.ko's own 9 exported symbols, for VM/foreign-hardware
 * boot testing only.
 *
 * WHY THIS EXISTS (Bar 2, 2026-07-02): OA.ko's own
 * setup_global_resources()/CSTGGlobal::UpdateXXX code calls 9 real
 * OmapNKS4Module.ko exports directly (COmapNKS4Driver_GetHardwareVersion/
 * GetOmapVersion/GetPSocVersion/Is88Key/SetTestMode,
 * OmapNKS4OutputFifo_WriteCommand, SetupNKS4Calibration -- CORRECTED
 * 2026-07-04, was previously missing SetTestMode entirely, a genuine
 * gap that would have surfaced as a real "Unknown symbol in module"
 * kernel error the moment OA.ko's own SetNKS4TestModeFlag path ran;
 * ADDED 2026-07-08, COmapNKS4_IncProgressBar, see note below). The REAL
 * OmapNKS4Module.ko (already 100%
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
/* ADDED (2026-07-08): closes the gap found while re-attempting the Bar 2
 * boot test -- OA.ko's IncProgressBar() forwarder (src/init/
 * startup_helpers.cpp, sec 10.179/10.182) calls this by name, and it was
 * simply missing from this stub, so insmod OA.ko failed symbol
 * resolution before init_module() could even run.
 *
 * `COmapNKS4_IncProgressBar` is a REAL, DISTINCT exported symbol in the
 * genuine OmapNKS4Module.ko -- not a typo for COmapNKS4Driver_IncProgressBar
 * above. Confirmed via `nm RestoreDVD_SystemMNT/mnt/sbin/OmapNKS4Module.ko`:
 * both `COmapNKS4Driver_IncProgressBar` (0x5510, plain global T, no
 * __ksymtab entry -- internal to that module only) and
 * `COmapNKS4_IncProgressBar` (0x74e0, WITH its own __ksymtab_/__kstrtab_
 * entries -- genuinely EXPORT_SYMBOL'd) exist side by side in the real
 * binary. OA.ko's own disassembly (see startup_helpers.cpp's own ground
 * truth comment) calls the latter, so that's the exact name this stub
 * must export too. Trivial no-op body, matching every other stub in this
 * file -- there is no real front-panel progress LED in a VM to tick. */
void COmapNKS4_IncProgressBar(void) { }
/* ADDED (2026-07-11): closes a gap found in a live boot test (kronosvm) --
 * `insmod OA.ko` failed symbol resolution on 12 unknown symbols, one of
 * which was this one, called by OA.ko's own `GetProgressBarValue()`
 * forwarder (src/init/load_global_resources.cpp, init_module step 11,
 * added in sec 10.204/batch 52 -- confirmed genuinely `U` in ground truth
 * too via `nm -u`, same shape/precedent as `COmapNKS4_IncProgressBar`
 * above). Real ground-truth caller clamps the return value via
 * `(v > 0x30) ? 1 : (0x31 - v)`, always landing in [1..49] -- returning
 * 0x30 here yields the minimal in-range countdown (1), the same
 * "front panel already fully progressed, no real LED to tick in a VM"
 * intent as IncProgressBar's own no-op body. */
unsigned char COmapNKS4_GetProgressBarPercent(void) { return 0x30; }

static int __init OmapNKS4VirtualDriverInit(void)
{
	printk(KERN_INFO "OmapNKS4VirtualDriver: loading (stand-in for "
	       "OmapNKS4Module.ko's 9 exports -- see this file's own header)\n");
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
EXPORT_SYMBOL(COmapNKS4_IncProgressBar);
EXPORT_SYMBOL(COmapNKS4_GetProgressBarPercent);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Stand-in for OmapNKS4Module.ko's 9 OA.ko-referenced exports (VM/foreign-hardware boot testing only)");
MODULE_AUTHOR("Korg (reconstructed)");
