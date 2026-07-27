// SPDX-License-Identifier: GPL-2.0
/*
 * oa_vtable_check.c - one-shot diagnostic module for the OA.ko
 * vtable-dispatch-stub-gap dynamic-verification pass (2026-07-27).
 *
 * Loaded AFTER OA.ko (from loadoa, right after "OA.ko: LOADED OK").
 * Resolves OA.ko's own internal (non-exported) ELF symbol table by
 * walking struct module::symtab directly (module_kallsyms_lookup_name()
 * itself is not EXPORT_SYMBOL'd on this kernel; find_module()/
 * module_mutex ARE, and struct module keeps the full local symtab/strtab
 * under CONFIG_KALLSYMS for exactly this kind of lookup).
 *
 * Reads the LIVE runtime bytes of the three raw-function-pointer
 * dispatch tables identified in the OA.ko dispatch-table sweep:
 *   CSTGControlMsgHandler::sMsgHandler[54]      (54 x void*)
 *   CSTGCalibrationMsgHandler::sMsgHandler[18]  (18 x {fn,ctx} = 2 words)
 *   CSTGFrontPanelMsgHandler::sMsgHandler[5]    (5 x {fn,ctx} = 2 words)
 * and their owning classes' sInstance pointers, printk'ing everything to
 * dmesg (captured by boot_console.log with zero guest-shell interaction
 * needed).
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/elf.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("OA.ko dispatch-table dynamic-verification probe");

extern struct mutex module_mutex;
extern struct module *find_module(const char *name);

static unsigned long resolve(struct module *mod, const char *name)
{
	unsigned int i;
	for (i = 0; i < mod->num_symtab; i++) {
		Elf32_Sym *sym = &mod->symtab[i];
		const char *sname = mod->strtab + sym->st_name;
		if (sname[0] && strcmp(sname, name) == 0)
			return sym->st_value;
	}
	return 0;
}

static void dump_words(const char *label, unsigned long addr, int count)
{
	int i;
	unsigned long *p = (unsigned long *)addr;
	printk(KERN_INFO "OAVCHK: %s @0x%08lx (%d words):\n", label, addr, count);
	for (i = 0; i < count; i += 4) {
		printk(KERN_INFO "OAVCHK:   [%3d] %08lx %08lx %08lx %08lx\n", i,
		       (i+0 < count) ? p[i+0] : 0,
		       (i+1 < count) ? p[i+1] : 0,
		       (i+2 < count) ? p[i+2] : 0,
		       (i+3 < count) ? p[i+3] : 0);
	}
}

static int __init oa_vtable_check_init(void)
{
	struct module *mod;
	unsigned long inst_ctrl, tbl_ctrl;
	unsigned long inst_calib, tbl_calib;
	unsigned long inst_fp, tbl_fp;
	unsigned long ctor_ctrl, ctor_calib, ctor_fp;

	printk(KERN_INFO "OAVCHK: === OA.ko dispatch-table dynamic check starting ===\n");

	mutex_lock(&module_mutex);
	mod = find_module("OA");
	if (!mod) {
		mutex_unlock(&module_mutex);
		printk(KERN_ERR "OAVCHK: module 'OA' not found (not loaded?)\n");
		return 0;
	}
	printk(KERN_INFO "OAVCHK: found module OA, num_symtab=%u core=0x%p state=%d\n",
	       mod->num_symtab, mod->module_core, mod->state);

	inst_ctrl  = resolve(mod, "_ZN21CSTGControlMsgHandler9sInstanceE");
	tbl_ctrl   = resolve(mod, "_ZN21CSTGControlMsgHandler11sMsgHandlerE");
	inst_calib = resolve(mod, "_ZN25CSTGCalibrationMsgHandler9sInstanceE");
	tbl_calib  = resolve(mod, "_ZN25CSTGCalibrationMsgHandler11sMsgHandlerE");
	inst_fp    = resolve(mod, "_ZN24CSTGFrontPanelMsgHandler9sInstanceE");
	tbl_fp     = resolve(mod, "_ZN24CSTGFrontPanelMsgHandler11sMsgHandlerE");
	ctor_ctrl  = resolve(mod, "_ZN21CSTGControlMsgHandlerC1Ev");
	ctor_calib = resolve(mod, "_ZN25CSTGCalibrationMsgHandlerC1Ev");
	ctor_fp    = resolve(mod, "_ZN24CSTGFrontPanelMsgHandlerC1Ev");
	mutex_unlock(&module_mutex);

	printk(KERN_INFO "OAVCHK: symbol resolution:\n");
	printk(KERN_INFO "OAVCHK:   CSTGControlMsgHandler::sInstance     = 0x%08lx (ctor@0x%08lx)\n", inst_ctrl, ctor_ctrl);
	printk(KERN_INFO "OAVCHK:   CSTGControlMsgHandler::sMsgHandler   = 0x%08lx\n", tbl_ctrl);
	printk(KERN_INFO "OAVCHK:   CSTGCalibrationMsgHandler::sInstance = 0x%08lx (ctor@0x%08lx)\n", inst_calib, ctor_calib);
	printk(KERN_INFO "OAVCHK:   CSTGCalibrationMsgHandler::sMsgHandler = 0x%08lx\n", tbl_calib);
	printk(KERN_INFO "OAVCHK:   CSTGFrontPanelMsgHandler::sInstance  = 0x%08lx (ctor@0x%08lx)\n", inst_fp, ctor_fp);
	printk(KERN_INFO "OAVCHK:   CSTGFrontPanelMsgHandler::sMsgHandler = 0x%08lx\n", tbl_fp);

	if (!inst_ctrl || !tbl_ctrl || !inst_calib || !tbl_calib || !inst_fp || !tbl_fp) {
		printk(KERN_ERR "OAVCHK: FAILED to resolve one or more symbols -- aborting dump\n");
		return 0;
	}

	printk(KERN_INFO "OAVCHK: --- CSTGControlMsgHandler ---\n");
	printk(KERN_INFO "OAVCHK: sInstance value = 0x%08lx (%s)\n",
	       *(unsigned long *)inst_ctrl, *(unsigned long *)inst_ctrl ? "CONSTRUCTED" : "NULL - ctor never ran");
	dump_words("CSTGControlMsgHandler::sMsgHandler", tbl_ctrl, 54);

	printk(KERN_INFO "OAVCHK: --- CSTGCalibrationMsgHandler ---\n");
	printk(KERN_INFO "OAVCHK: sInstance value = 0x%08lx (%s)\n",
	       *(unsigned long *)inst_calib, *(unsigned long *)inst_calib ? "CONSTRUCTED" : "NULL - ctor never ran");
	dump_words("CSTGCalibrationMsgHandler::sMsgHandler", tbl_calib, 36);

	printk(KERN_INFO "OAVCHK: --- CSTGFrontPanelMsgHandler ---\n");
	printk(KERN_INFO "OAVCHK: sInstance value = 0x%08lx (%s)\n",
	       *(unsigned long *)inst_fp, *(unsigned long *)inst_fp ? "CONSTRUCTED" : "NULL - ctor never ran");
	dump_words("CSTGFrontPanelMsgHandler::sMsgHandler", tbl_fp, 10);

	printk(KERN_INFO "OAVCHK: === OA.ko dispatch-table dynamic check DONE ===\n");
	return 0;
}

static void __exit oa_vtable_check_exit(void)
{
}

module_init(oa_vtable_check_init);
module_exit(oa_vtable_check_exit);
