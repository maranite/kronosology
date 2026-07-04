// SPDX-License-Identifier: GPL-2.0
/* TEMPORARY debugging aid (2026-07-03) -- NOT part of the real
 * reconstruction, to be deleted once the Bar 2 module_put crash is
 * root-caused. init_module.cpp's own `printk` declaration uses a
 * special fmt_offset integer convention matching the real binary's own
 * literal calls, so a normal string-based printk needs a separate,
 * differently-named wrapper. */
extern "C" __attribute__((regparm(0))) int printk(const char *fmt, ...);

extern "C" void oa_debug_marker(int n)
{
	printk("<3>OA_DEBUG_MARKER %d\n", n);
}

/* Canary: a REAL global C++ object with a non-trivial constructor,
 * called via the kernel's own do_mod_ctors(mod) -- which runs BEFORE
 * do_one_initcall(mod->init) in sys_init_module. If this prints, we
 * know OA.ko's ELF loading/relocation (load_module()) completed
 * successfully and we reached the ctors phase; if it never prints,
 * the crash is happening even earlier, inside load_module() itself. */
struct OaDebugCanary {
	OaDebugCanary() { printk("<3>OA_DEBUG_CANARY_CTOR_RAN\n"); }
};
static OaDebugCanary g_oaDebugCanary;
