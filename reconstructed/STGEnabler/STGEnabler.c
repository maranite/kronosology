// SPDX-License-Identifier: GPL-2.0
/*
 * STGEnabler.ko  -  Korg Kronos "STG" GPL symbol-shim / enabler module
 *
 * Reconstructed from the shipping STGEnabler.ko (Linux 2.6.32.11 + RTAI, x86-32,
 * gcc -mregparm=3) by reverse engineering.  Functionally faithful; not a
 * byte-for-byte rebuild.
 *
 * Purpose
 * -------
 * The proprietary Korg sound-engine modules (OA.ko, OmapNKS4Module.ko, loadmod.ko,
 * GetPubIdMod.ko, ...) are *not* GPL.  The Linux kernel marks many symbols they need
 * - usb_register_driver(), set_cpus_allowed_ptr(), the RTAI real-time API, the VFS
 * helpers - as EXPORT_SYMBOL_GPL or simply not exported to out-of-tree code.
 *
 * This little module is licensed GPL, so it is *allowed* to call those symbols, and it
 * re-exports them under neutral "stg_" names with EXPORT_SYMBOL() (non-GPL).  The
 * proprietary modules then link only against these stg_* shims.  It is the classic
 * "GPL condom" pattern, plus a few genuinely useful helpers (mkdir, free-disk-space,
 * RTAI timer bring-up).
 *
 * Build: see Makefile (needs the Kronos 2.6.32.11 kernel tree with RTAI headers).
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/namei.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/dcache.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/stat.h>

/* ------------------------------------------------------------------------- *
 * Kernel-internal symbols that have no public prototype in the headers but
 * are exported by the (patched) Kronos kernel and imported by this module.
 * ------------------------------------------------------------------------- */
extern int path_lookup(const char *name, unsigned int flags, struct nameidata *nd);
extern struct dentry *lookup_create(struct nameidata *nd, int is_dir);

/* RTAI real-time API (declared here to avoid pulling the full rtai headers). */
extern void rt_linux_use_fpu(int use_fpu);
extern void rt_set_oneshot_mode(void);
extern int  start_rt_timer(int period);

/* ========================================================================= *
 *  USB pass-through shims (tail-call thunks in the binary).
 * ========================================================================= */

struct urb *stg_usb_alloc_urb(int iso_packets, gfp_t mem_flags)
{
	return usb_alloc_urb(iso_packets, mem_flags);
}
EXPORT_SYMBOL(stg_usb_alloc_urb);

void stg_usb_free_urb(struct urb *urb)
{
	usb_free_urb(urb);
}
EXPORT_SYMBOL(stg_usb_free_urb);

int stg_usb_submit_urb(struct urb *urb, gfp_t mem_flags)
{
	return usb_submit_urb(urb, mem_flags);
}
EXPORT_SYMBOL(stg_usb_submit_urb);

int stg_usb_register_driver(struct usb_driver *driver, struct module *owner,
			    const char *mod_name)
{
	return usb_register_driver(driver, owner, mod_name);
}
EXPORT_SYMBOL(stg_usb_register_driver);

void stg_usb_deregister(struct usb_driver *driver)
{
	usb_deregister(driver);
}
EXPORT_SYMBOL(stg_usb_deregister);

int stg_usb_driver_claim_interface(struct usb_driver *driver,
				   struct usb_interface *iface, void *priv)
{
	return usb_driver_claim_interface(driver, iface, priv);
}
EXPORT_SYMBOL(stg_usb_driver_claim_interface);

/* ========================================================================= *
 *  Scheduler / CPU-affinity shims.
 * ========================================================================= */

int stg_sched_setscheduler(struct task_struct *p, int policy,
			   struct sched_param *param)
{
	return sched_setscheduler(p, policy, param);
}
EXPORT_SYMBOL(stg_sched_setscheduler);

/*
 * stg_set_cpus_allowed(task, mask):
 *   The binary takes the affinity as a raw cpumask *word* (EDX), stashes it in a
 *   one-word cpumask on the stack and hands its address to set_cpus_allowed_ptr().
 */
int stg_set_cpus_allowed(struct task_struct *p, unsigned long mask)
{
	cpumask_t cm;

	cpumask_clear(&cm);
	cpumask_bits(&cm)[0] = mask;
	return set_cpus_allowed_ptr(p, &cm);
}
EXPORT_SYMBOL(stg_set_cpus_allowed);

/*
 * stg_cpumask_of_cpu(cpu): first word of the kernel's per-cpu bit mask.
 *   Binary indexes cpu_bit_bitmap[1 + cpu%32] - cpu/32, i.e. exactly get_cpu_mask(cpu).
 */
unsigned long stg_cpumask_of_cpu(unsigned int cpu)
{
	return cpumask_bits(get_cpu_mask(cpu))[0];
}
EXPORT_SYMBOL(stg_cpumask_of_cpu);

/* ========================================================================= *
 *  RTAI real-time timer bring-up.
 * ========================================================================= */

void stg_rtai_setup(void)
{
	rt_linux_use_fpu(1);		/* allow FPU use inside RT context */
	rt_set_oneshot_mode();
	start_rt_timer(0);		/* 0 == one-shot mode, no fixed period */
}
EXPORT_SYMBOL(stg_rtai_setup);

/* ========================================================================= *
 *  VFS helpers.
 * ========================================================================= */

/*
 * stg_mkdir(pathname, mode): in-kernel equivalent of the mkdir(2) syscall body,
 * matching 2.6.32 fs/namei.c:sys_mkdirat().
 */
int stg_mkdir(const char *pathname, int mode)
{
	struct nameidata nd;
	struct dentry *dentry;
	int error;

	error = path_lookup(pathname, LOOKUP_PARENT, &nd);
	if (error)
		return error;

	dentry = lookup_create(&nd, 1);
	error = PTR_ERR(dentry);
	if (!IS_ERR(dentry)) {
		if (!IS_POSIXACL(nd.path.dentry->d_inode))
			mode &= ~current_umask();
		error = vfs_mkdir(nd.path.dentry->d_inode, dentry, mode);
		dput(dentry);
	}
	mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
	path_put(&nd.path);
	return error;
}
EXPORT_SYMBOL(stg_mkdir);

/*
 * stg_get_free_diskspace(path): free bytes on the filesystem containing 'path'.
 *
 * The original does NOT use vfs_statfs(); it walks Korg's private superblock
 * structure (sb->s_fs_info) directly and sums per-bucket free-block counts.  The
 * exact struct types are not in the kernel headers, so the recovered field offsets
 * are reproduced verbatim via a small accessor struct.  (NB: like the binary, this
 * deliberately does not path_put() the looked-up path.)
 */

/* Korg-FS private superblock info, layout recovered from the binary. */
struct korgfs_sb_info {
	unsigned char	_pad00[0x20];
	unsigned int	bucket_size;	/* +0x20: bucket capacity (power of two) */
	unsigned int	bucket_count;	/* +0x24: number of logical buckets     */
	unsigned char	_pad28[0x10];
	void	      **hash;		/* +0x38: bucket hash table             */
	unsigned char	_pad3c[0x18];
	unsigned int	hash_shift;	/* +0x54: index -> hash-slot shift      */
};

long long stg_get_free_diskspace(const char *path)
{
	struct nameidata nd;
	long long free_bytes = 0;

	if (path_lookup(path, LOOKUP_FOLLOW, &nd) != 0)
		return 0;

	{
		struct super_block *sb = nd.path.dentry->d_sb;
		/* sb->s_fs_info lives at offset 0x180 in this kernel's super_block. */
		struct korgfs_sb_info *fsi = *(void **)((char *)sb + 0x180);
		unsigned int n = fsi->bucket_count;

		if (n) {
			unsigned int mask = fsi->bucket_size - 1;
			unsigned int shift = fsi->hash_shift;
			unsigned int free_blocks = 0;
			unsigned int i;

			for (i = 0; i < n; i++) {
				char *bucket = fsi->hash[i >> shift];

				if (bucket) {
					/* records are 0x20 bytes; free count is a u16 at +0x0c */
					char *rec = *(char **)(bucket + 0x18) + ((i & mask) << 5);
					free_blocks += *(unsigned short *)(rec + 0x0c);
				}
			}
			/* sb->s_blocksize is at offset 0x0c. */
			free_bytes = (long long)free_blocks *
				     *(unsigned int *)((char *)sb + 0x0c);
		}
	}
	return free_bytes;
}
EXPORT_SYMBOL(stg_get_free_diskspace);

/* ========================================================================= *
 *  Module init / exit.
 * ========================================================================= */

static int __init STGEnabler_init(void)
{
	stg_rtai_setup();
	return 0;
}

static void __exit STGEnabler_exit(void)
{
	/* nothing to tear down */
}

module_init(STGEnabler_init);
module_exit(STGEnabler_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Korg Kronos STG enabler: GPL symbol shim + VFS/RTAI helpers");
