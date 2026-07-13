// SPDX-License-Identifier: GPL-2.0
/*
 * module_main.c  -  AT88VirtualChip.ko module init/exit and the
 * RTAI-safe deferred blob-loading worker.
 *
 * This file is kernel-only (real Linux headers, EXPORT_SYMBOL) and is
 * deliberately kept separate from nv2ac_exports.cpp/chip_state.cpp/etc.,
 * which stay freestanding and host-testable. Nothing in those files
 * changes to support this module build.
 *
 * Deliberately plain C, not C++: this ancient kernel's headers use
 * syntax (GNU inline-asm string-literal suffixes like "itype"/"rtype",
 * old-style declarations, etc.) that a modern g++ can't parse as C++,
 * even though it parses fine as C via gcc (confirmed the hard way --
 * see MASTER_REFERENCE.md sec 10.43). This file uses no genuine C++
 * features, so compiling it as C sidesteps the problem entirely rather
 * than working around it.
 *
 * RTAI constraint (confirmed in this project's own CLAUDE.md, re-derived
 * the hard way while building kronos_extract.ko): on this kernel
 * (2.6.32.11-korg + RTAI), `filp_open` from `init_module` context FAILS
 * -- RTAI blocks the GFP_KERNEL allocation `filp_open` needs.
 * `create_proc_entry` in init_module fails silently the same way.
 * `kthread_run` starts but RTAI starves the resulting kthread. The one
 * approach that actually works is `schedule_work` (a workqueue worker has
 * full GFP_KERNEL) -- confirmed by kronos_extract.ko, which uses exactly
 * this pattern for its own filp_open calls. Ported that pattern here
 * rather than re-discovering it: `AT88VirtualChipInit` only sets up a
 * workqueue and queues the real work; `load_chip_blob_work` (running in
 * workqueue context) does the actual filp_open/vfs_read/at88_chip_module_init.
 *
 * Allocation constraint (this project's own "Kronos Allocation" finding):
 * no kmalloc/kfree exports are available on this kernel build -- use
 * vmalloc/vfree (or a static pool) instead. The blob is only 188 bytes,
 * comfortably a static buffer, so no allocation is needed for it at all;
 * vmalloc is used here only for symmetry/documentation in case a future
 * change needs a larger buffer.
 *
 * Self-sufficiency (sec 10.233): a real hardware-extracted
 * KronosExtract.bin can only ever exist after running kronos_extract.ko
 * against a real physical AT88 chip -- there is no such chip in a VM or
 * on foreign (non-Kronos) hardware, so blob_path genuinely will not exist
 * there. Every failure path below (filp_open ENOENT, a short/failed
 * vfs_read, or at88_chip_module_init() rejecting a malformed blob) now
 * falls back to at88_chip_load_synthetic() (at88_chip.h/chip_state.cpp)
 * instead of silently leaving the chip singleton at its untouched,
 * all-zero, AAC-byte-at-0 C++ static-init default -- which, confirmed via
 * a real cross-module integration test against OA.ko's actual
 * SetupAtmelForAuthorizations() (MASTER_REFERENCE.md sec 10.233), makes
 * the very first $B8 handshake round fail its own "verified" sentinel
 * check (the AAC byte needs to start pre-saturated at 0xff, not 0 -- see
 * at88_chip_load_synthetic()'s own header comment for the full reasoning).
 * The GPA wire cipher itself has no per-device secret (README.md finding
 * #2), so this synthetic chip is cryptographically self-consistent for
 * SetupAtmelForAuthorizations()'s own handshake even though it carries no
 * real per-device Zone0 secret -- real EXs auth-string validation is a
 * separate concern this fallback does not (and cannot) address.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/errno.h>

int  at88_chip_module_init(const unsigned char *blob, unsigned int blobLen);
void at88_chip_module_init_synthetic(void);
int  stgNV2AC_sync_cmd(unsigned char *address, unsigned int data);
int  stgNV2AC_sync_read_cmd(int cmd4, int dest);

/*
 * Where the real KronosExtract.bin-format blob (188 bytes; see
 * at88_chip.h's at88_chip_load_from_extract() for the exact layout) lives
 * at runtime. A module parameter rather than a fixed path so this works
 * both on real hardware (where KronosExtract/CLAUDE.md's documented
 * output location, /korg/rw/KronosExtract.bin, is the natural default)
 * and in a VM/foreign-motherboard context (the whole point of this
 * project's boot/load-dependency pivot -- see MASTER_REFERENCE.md sec
 * 10.17/10.18), where the blob may need to live somewhere else entirely.
 */
static char *blob_path = (char *)"/korg/rw/KronosExtract.bin";
module_param(blob_path, charp, 0444);
MODULE_PARM_DESC(blob_path,
	"Path to a KronosExtract.bin-format captured chip data blob (188 bytes)");

static struct workqueue_struct *at88_wq;
static struct work_struct at88_work;

static void load_chip_blob_work(struct work_struct *w)
{
	struct file *f;
	mm_segment_t old_fs;
	loff_t pos = 0;
	unsigned char buf[188];
	int n;

	f = filp_open(blob_path, O_RDONLY, 0);
	if (IS_ERR(f)) {
		printk(KERN_INFO "AT88VirtualChip: filp_open(%s) failed: %ld -- no real "
		       "hardware-extracted chip data available (expected in a VM/"
		       "foreign-hardware boot; there is no physical AT88 chip to have "
		       "extracted it from). Falling back to a synthetic chip -- "
		       "SetupAtmelForAuthorizations()'s own GPA handshake will still "
		       "succeed (see at88_chip_load_synthetic()'s header comment), but "
		       "real EXs auth-string validation will not be meaningful.\n",
		       blob_path, PTR_ERR(f));
		at88_chip_module_init_synthetic();
		return;
	}

	old_fs = get_fs();
	set_fs(get_ds());
	n = vfs_read(f, (char __user *)buf, sizeof(buf), &pos);
	set_fs(old_fs);
	filp_close(f, NULL);

	if (n != (int)sizeof(buf)) {
		printk(KERN_ERR "AT88VirtualChip: %s: read %d bytes, expected %zu -- "
		       "falling back to a synthetic chip (see filp_open failure path "
		       "above for why this fallback exists)\n",
		       blob_path, n, sizeof(buf));
		at88_chip_module_init_synthetic();
		return;
	}

	if (at88_chip_module_init(buf, sizeof(buf)) != 0) {
		printk(KERN_ERR "AT88VirtualChip: %s: not a valid captured chip blob "
		       "(bad magic/CRC/overlap -- see at88_chip_load_from_extract()) -- "
		       "falling back to a synthetic chip (see filp_open failure path "
		       "above for why this fallback exists)\n",
		       blob_path);
		at88_chip_module_init_synthetic();
		return;
	}

	printk(KERN_INFO "AT88VirtualChip: chip data loaded from %s\n", blob_path);
}

static int __init AT88VirtualChipInit(void)
{
	printk(KERN_INFO "AT88VirtualChip: loading (stand-in for OmapNKS4Module.ko's "
	       "AT88SC/NV2AC chip access -- see README.md)\n");

	at88_wq = create_singlethread_workqueue("at88_wq");
	if (!at88_wq) {
		printk(KERN_ERR "AT88VirtualChip: workqueue alloc failed\n");
		return -ENOMEM;
	}

	/* Do NOT filp_open here -- see file header. Defer to workqueue context. */
	INIT_WORK(&at88_work, load_chip_blob_work);
	queue_work(at88_wq, &at88_work);

	/*
	 * Return success even though the blob load is still pending: the
	 * real stgNV2AC_sync_cmd/stgNV2AC_sync_read_cmd calls OA.ko/loadmod.ko
	 * make don't happen until well after insmod returns (OA.ko's own
	 * SetupAtmelForAuthorizations runs from VerifyAuthorizationString,
	 * itself well after module init -- see reconstructed/OA/src/auth/
	 * atmel_setup.cpp and MASTER_REFERENCE.md sec 10.17's init_module
	 * sequencing), so the async load has time to complete first in
	 * practice. If chip.dataLoaded is still 0 by the time a real read
	 * comes in, at88_chip_read_config()/at88_chip_read_zone0() simply
	 * return data from an all-zero AT88ChipState (memset by the C++
	 * static-storage default-initialization of the g_chip singleton in
	 * nv2ac_exports.cpp) rather than crashing.
	 */
	return 0;
}

static void __exit AT88VirtualChipExit(void)
{
	if (at88_wq) {
		flush_workqueue(at88_wq);
		destroy_workqueue(at88_wq);
	}
	printk(KERN_INFO "AT88VirtualChip: unloaded\n");
}

module_init(AT88VirtualChipInit);
module_exit(AT88VirtualChipExit);

EXPORT_SYMBOL(stgNV2AC_sync_cmd);
EXPORT_SYMBOL(stgNV2AC_sync_read_cmd);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Software AT88SC/NV2AC chip emulator (stand-in for OmapNKS4Module.ko's chip access, for VM/foreign-hardware boot testing)");
MODULE_AUTHOR("Korg (reconstructed)");
