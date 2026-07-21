/*
 * OmapVideo.c - Korg Kronos /dev/fb1 front-panel framebuffer driver core.
 *
 * Reconstructed from OmapVideoModule.ko via Ghidra decompilation + raw
 * disassembly cross-check (regparm(3) argument recovery for omapfb_ioctl
 * and the field-offset walk in omapfb_probe were done instruction-by-
 * instruction, not just from decompiler output - see the doc for detail).
 *
 * This is a pure software (vmalloc-backed) fb driver: there is no real
 * OMAP display controller behind it. All actual pixel delivery to the
 * physical LCD happens through OmapNKS4Module.ko's USB/serial protocol
 * (the OmapNKS4Send-family and OmapNKS4Update-family calls below); this
 * driver's screen_base is just a plain malloc'd buffer that gets
 * scraped/pushed via ioctl.
 */

#include <linux/module.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>

#include "omapvideo_internal.h"

/* ------------------------------------------------------------------ */
/* Module state                                                        */
/* ------------------------------------------------------------------ */

struct fb_info        *omapfb_info;
struct platform_device *omapfb_device;
u32                    *videomemory;
unsigned long           videomemorysize = 800UL * 600UL; /* overridden by module_param at load */
struct proc_dir_entry  *gProc;

module_param(videomemorysize, ulong, 0);

/* ------------------------------------------------------------------ */
/* Default mode - 800x600, 8bpp truecolor-tagged palette mode          */
/* (values read back byte-for-byte from the .rodata image of the       */
/* original omapfb_default / omapfb_fix globals)                       */
/* ------------------------------------------------------------------ */

struct fb_var_screeninfo omapfb_default = {
	.xres		= 800,
	.yres		= 600,
	.xres_virtual	= 800,
	.yres_virtual	= 600,
	.bits_per_pixel	= 8,
	.red		= { .offset = 0, .length = 8 },
	.green		= { .offset = 0, .length = 8 },
	.blue		= { .offset = 0, .length = 8 },
	.activate	= FB_ACTIVATE_TEST,
	.height		= (u32)-1,
	.width		= (u32)-1,
	.pixclock	= 20000,
	.left_margin	= 96,
	.right_margin	= 32,
	.upper_margin	= 16,
	.lower_margin	= 4,
	.hsync_len	= 96,
	.vsync_len	= 4,
};

struct fb_fix_screeninfo omapfb_fix = {
	.id		= "OMAP FB",
	.type		= FB_TYPE_PACKED_PIXELS,
	.visual		= FB_VISUAL_TRUECOLOR,
	.xpanstep	= 1,
	.ypanstep	= 1,
	.ywrapstep	= 1,
	.line_length	= 800,
	.accel		= FB_ACCEL_NONE,
};

struct fb_ops omapfb_ops = {
	.fb_read	= omapfb_sys_read,
	.fb_write	= omapfb_sys_write,
	.fb_check_var	= omapfb_check_var,
	.fb_set_par	= omapfb_set_par,
	.fb_setcolreg	= omapfb_setcolreg,
	.fb_pan_display	= omapfb_pan_display,
	.fb_fillrect	= omapfb_fillrect,
	.fb_copyarea	= omapfb_copyarea,
	.fb_imageblit	= omapfb_imageblit,
	.fb_ioctl	= omapfb_ioctl,
	.fb_mmap	= omapfb_mmap,
	/* .owner is left unset (0) here, matching the original binary's
	 * omapfb_ops image, which has a NULL owner field - unusual for a
	 * module-provided fb_ops but confirmed byte-for-byte against the
	 * .data section. */
};

struct platform_driver omapfb_driver = {
	.probe	= omapfb_probe,
	.remove	= omapfb_remove,
	.driver	= {
		.name	= "omapfb",
		.owner	= THIS_MODULE,
	},
};

/* ------------------------------------------------------------------ */
/* fb_ops: fb_check_var                                                */
/* ------------------------------------------------------------------ */

int omapfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	unsigned long line_bytes;

	if (var->vmode & FB_VMODE_SMOOTH_XPAN) {
		var->vmode |= FB_VMODE_YWRAP;
		var->xoffset = info->var.xoffset;
		var->yoffset = info->var.yoffset;
	}

	if (!var->xres)
		var->xres = 1;
	if (!var->yres)
		var->yres = 1;
	if (var->xres > var->xres_virtual)
		var->xres_virtual = var->xres;
	if (var->yres > var->yres_virtual)
		var->yres_virtual = var->yres;

	if (var->bits_per_pixel <= 1)
		var->bits_per_pixel = 1;
	else if (var->bits_per_pixel <= 8)
		var->bits_per_pixel = 8;
	else if (var->bits_per_pixel <= 16)
		var->bits_per_pixel = 16;
	else if (var->bits_per_pixel <= 24)
		var->bits_per_pixel = 24;
	else if (var->bits_per_pixel <= 32)
		var->bits_per_pixel = 32;
	else
		return -EINVAL;

	if (var->xres + var->xoffset > var->xres_virtual)
		var->xres_virtual = var->xres + var->xoffset;
	if (var->yres + var->yoffset > var->yres_virtual)
		var->yres_virtual = var->yres + var->yoffset;

	line_bytes = ((unsigned long)var->xres_virtual * var->bits_per_pixel + 31) & ~31UL;
	line_bytes >>= 3;
	if (line_bytes * var->yres_virtual > videomemorysize)
		return -ENOMEM;

	switch (var->bits_per_pixel) {
	case 1:
	case 8:
		var->red.offset = 0;    var->red.length = 8;
		var->green.offset = 0;  var->green.length = 8;
		var->blue.offset = 0;   var->blue.length = 8;
		var->transp.offset = 0; var->transp.length = 0;
		break;
	case 16:
		if (var->transp.length == 0) {
			/* RGB 565 */
			var->red.offset = 0;    var->red.length = 5;
			var->green.offset = 5;  var->green.length = 6;
			var->blue.offset = 11;  var->blue.length = 5;
			var->transp.offset = 0; var->transp.length = 0;
		} else {
			/* ARGB 1555 */
			var->red.offset = 0;    var->red.length = 5;
			var->green.offset = 5;  var->green.length = 5;
			var->blue.offset = 10;  var->blue.length = 5;
			var->transp.offset = 15; var->transp.length = 1;
		}
		break;
	case 24:
		var->red.offset = 0;    var->red.length = 8;
		var->green.offset = 8;  var->green.length = 8;
		var->blue.offset = 16;  var->blue.length = 8;
		var->transp.offset = 0; var->transp.length = 0;
		break;
	case 32:
		var->red.offset = 0;    var->red.length = 8;
		var->green.offset = 8;  var->green.length = 8;
		var->blue.offset = 16;  var->blue.length = 8;
		var->transp.offset = 24; var->transp.length = 8;
		break;
	}

	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;

	return 0;
}

/* ------------------------------------------------------------------ */
/* fb_ops: fb_set_par                                                  */
/* ------------------------------------------------------------------ */

int omapfb_set_par(struct fb_info *info)
{
	info->fix.line_length =
		((info->var.xres_virtual * info->var.bits_per_pixel + 31) & ~31U) >> 3;
	return 0;
}

/* ------------------------------------------------------------------ */
/* fb_ops: fb_setcolreg                                                */
/* ------------------------------------------------------------------ */

int omapfb_setcolreg(unsigned regno, unsigned red, unsigned green,
		      unsigned blue, unsigned transp, struct fb_info *info)
{
	u32 v;

	if (transp == 0xffff)
		transp = 0;

	if (regno >= 256)
		return 1;

	if (info->fix.visual != FB_VISUAL_TRUECOLOR)
		return 0;

	v = (red   << info->var.red.offset)   |
	    (green << info->var.green.offset) |
	    (blue  << info->var.blue.offset)  |
	    (transp<< info->var.transp.offset);

	switch (info->var.bits_per_pixel) {
	case 8:
	case 16:
	case 24:
	case 32:
		((u32 *)info->pseudo_palette)[regno] = v;
		break;
	default:
		return 0;
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/* fb_ops: fb_pan_display                                              */
/* ------------------------------------------------------------------ */

int omapfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	int xoffset;

	if (var->vmode & FB_VMODE_YWRAP) {
		if ((u32)var->yoffset >= info->var.yres_virtual)
			return -EINVAL;
		if (var->xoffset)
			return -EINVAL;
		xoffset = 0;
	} else {
		if (info->var.xres_virtual < var->xres + var->xoffset)
			return -EINVAL;
		if (info->var.yres_virtual < var->yres + var->yoffset)
			return -EINVAL;
		xoffset = var->xoffset;
	}

	info->var.xoffset = xoffset;
	info->var.yoffset = var->yoffset;

	if (var->vmode & FB_VMODE_YWRAP)
		info->var.vmode |= FB_VMODE_YWRAP;
	else
		info->var.vmode &= ~FB_VMODE_YWRAP;

	return 0;
}

/* ------------------------------------------------------------------ */
/* fb_ops: fb_mmap                                                     */
/*                                                                      */
/* NOTE: the pgprot value passed to remap_pfn_range() is the literal    */
/* constant 0x27, not vma->vm_page_prot - confirmed in the raw          */
/* disassembly (an immediate MOV, not a load from the vma). This is    */
/* unusual (it discards whatever protection flags mmap()/the VFS set   */
/* up) but is reproduced verbatim rather than "corrected".              */
/* ------------------------------------------------------------------ */

int omapfb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	unsigned long start = vma->vm_start;
	unsigned long size  = vma->vm_end - vma->vm_start;
	unsigned long pos = 0;
	int ret = 0;

	if (size == 0)
		return 0;

	while (pos < size) {
		unsigned long pfn = vmalloc_to_pfn((char *)videomemory + pos);

		ret = remap_pfn_range(vma, start + pos, pfn, PAGE_SIZE,
				       __pgprot(0x27));
		if (ret < 0)
			break;
		pos += PAGE_SIZE;
	}

	return ret;
}

/* ------------------------------------------------------------------ */
/* fb_ops: fb_read / fb_write (generic vmalloc-backed sys_read/write)  */
/* ------------------------------------------------------------------ */

ssize_t omapfb_sys_read(struct fb_info *info, char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	u32 fbmemlength;

	if (info->state != FBINFO_STATE_RUNNING)
		return -EPERM;

	fbmemlength = info->screen_size;
	if (!fbmemlength)
		fbmemlength = info->fix.smem_len;

	if (p >= fbmemlength)
		return 0;

	if (count >= fbmemlength)
		count = fbmemlength;
	if (count + p > fbmemlength)
		count = fbmemlength - p;

	if (info->fbops->fb_sync)
		info->fbops->fb_sync(info);

	if (copy_to_user(buf, (char *)videomemory + p, count))
		return -EFAULT;

	*ppos += count;
	return count;
}

ssize_t omapfb_sys_write(struct fb_info *info, const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	u32 fbmemlength;
	ssize_t err;
	size_t wcount;

	if (info->state != FBINFO_STATE_RUNNING)
		return -EPERM;

	fbmemlength = info->screen_size;
	if (!fbmemlength)
		fbmemlength = info->fix.smem_len;

	if (p > fbmemlength)
		return -EFBIG;

	wcount = fbmemlength;
	err = -EFBIG;
	if (count <= fbmemlength) {
		err = 0;
		wcount = count;
	}
	if (wcount + p > fbmemlength) {
		if (err == 0)
			err = -ENOSPC;
		wcount = fbmemlength - p;
	}

	if (info->fbops->fb_sync)
		info->fbops->fb_sync(info);

	if (copy_from_user((char *)videomemory + p, buf, wcount))
		return -EFAULT;

	if (err)
		return err;

	*ppos += wcount;
	return wcount;
}

/* ------------------------------------------------------------------ */
/* fb_ops: fb_ioctl                                                     */
/*                                                                      */
/* Command table decoded directly from the binary's CMP-chain immediates */
/* (see omapvideo_internal.h). Every branch below was verified against   */
/* the raw x86 disassembly, including which commands copy_from_user,     */
/* which copy_to_user, and which do neither (bare trigger calls).        */
/* ------------------------------------------------------------------ */

int omapfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case OMAPFB_IOCTL_PING: {
		int tmp;
		if (copy_from_user(&tmp, argp, sizeof(tmp)))
			return -EFAULT;
		return 0;
	}

	case OMAPFB_IOCTL_GETPROGRESSPCT2: {
		/* Hardcoded stub in the original: no OmapNKS4 call at all. */
		int val = 0x37;
		if (copy_to_user(argp, &val, sizeof(val)))
			return -EFAULT;
		return 0;
	}

	case OMAPFB_IOCTL_DUMPPALETTE: {
		int idx;
		u32 *pal = (u32 *)info->pseudo_palette;
		if (copy_from_user(&idx, argp, sizeof(idx)))
			return -EFAULT;
		if ((unsigned)idx < 256) {
			for (; idx < 256; idx++)
				printk(KERN_INFO "    palette[%d] = 0x%0x\n", idx, pal[idx]);
		}
		return 0;
	}

	case OMAPFB_IOCTL_DUMPVIDMEM: {
		int off;
		signed char *p;
		if (copy_from_user(&off, argp, sizeof(off)))
			return -EFAULT;
		printk(KERN_INFO "OMAPFB_IOCTL_DUMPVIDMEM received 0x%x\n", off);
		p = (signed char *)videomemory + off;
		printk(KERN_INFO "*0x%x = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		       (unsigned)(unsigned long)videomemory,
		       p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
		return 0;
	}

	case OMAPFB_IOCTL_INITLCDREGS: {
		struct { s8 a; s8 b; s8 pad[2]; s32 c; } data;
		if (copy_from_user(&data, argp, sizeof(data)))
			return -EFAULT;
		return OmapNKS4InitLCDRegs(data.a, data.b, data.c);
	}

	case OMAPFB_IOCTL_XAXISBYTESIZE: {
		int val;
		if (copy_from_user(&val, argp, sizeof(val)))
			return -EFAULT;
		return OmapNKS4XAxisByteSize(val);
	}

	case OMAPFB_IOCTL_SENDPIXELDATA: {
		s32 data[3];
		if (copy_from_user(data, argp, sizeof(data)))
			return -EFAULT;
		return OmapNKS4SendPixelDataRegion(data[0], data[1], data[2]);
	}

	case OMAPFB_IOCTL_SENDFILLDATA: {
		struct { s8 a; s8 pad[3]; s32 b, c, d; } data;
		if (copy_from_user(&data, argp, sizeof(data)))
			return -EFAULT;
		return OmapNKS4SendFillData(data.a, data.b, data.c, data.d);
	}

	case OMAPFB_IOCTL_UPDATECOLORPAL: {
		s8 b[4];
		if (copy_from_user(b, argp, sizeof(b)))
			return -EFAULT;
		return OmapNKS4UpdateColorPal(b[0], b[1], b[2], b[3]);
	}

	case OMAPFB_IOCTL_GETPROGRESSPCT: {
		int val = COmapNKS4_GetProgressBarPercent();
		if (copy_to_user(argp, &val, sizeof(val)))
			return -EFAULT;
		return 0;
	}

	case OMAPFB_IOCTL_SETPROGRESSPCT: {
		int val;
		if (copy_from_user(&val, argp, sizeof(val)))
			return -EFAULT;
		COmapNKS4_SetProgressBarPercent((unsigned int)val);
		/* NOTE: the original copies back 4 bytes read from the start
		 * of *info (i.e. info->node), not the percent value, before
		 * returning - reproduced verbatim; almost certainly an
		 * unintentional quirk (ESI/EDX register reuse) in the
		 * original rather than deliberate behavior. */
		if (copy_to_user(argp, info, sizeof(int)))
			return -EFAULT;
		return 0;
	}

	case OMAPFB_IOCTL_INCPROGRESSBAR: {
		int val = COmapNKS4_IncProgressBar();
		if (copy_to_user(argp, &val, sizeof(val)))
			return -EFAULT;
		return 0;
	}

	case OMAPFB_IOCTL_ADDTOPROGRESSBAR: {
		int val;
		if (copy_from_user(&val, argp, sizeof(val)))
			return -EFAULT;
		val = COmapNKS4_AddToProgressBar(val);
		if (copy_to_user(argp, &val, sizeof(val)))
			return -EFAULT;
		return 0;
	}

	case OMAPFB_IOCTL_GETTITLESCRVER: {
		int val = COmapNKS4_GetTitleScreenVersion();
		if (copy_to_user(argp, &val, sizeof(val)))
			return -EFAULT;
		return 0;
	}

	default:
		return -EINVAL;
	}
}

/* ------------------------------------------------------------------ */
/* platform_driver: probe / remove                                     */
/* ------------------------------------------------------------------ */

int omapfb_probe(struct platform_device *pdev)
{
	int ret = -ENOMEM;

	videomemory = vmalloc(videomemorysize);
	if (!videomemory)
		return ret;
	memset(videomemory, 0, videomemorysize);

	/* framebuffer_alloc()'s par_size argument (256*sizeof(u32) = 1024
	 * bytes) is not used as a generic "par" private area here - the
	 * driver repurposes the allocation framebuffer_alloc() points
	 * info->par at as the pseudo_palette buffer instead (see below).
	 * Confirmed via raw disassembly: "MOV AX,0x400" immediately
	 * precedes the framebuffer_alloc call. */
	omapfb_info = framebuffer_alloc(256 * sizeof(u32), &pdev->dev);
	if (!omapfb_info)
		goto err_vfree;

	omapfb_info->var = omapfb_default;
	omapfb_info->fbops = &omapfb_ops;
	omapfb_info->flags = FBINFO_DEFAULT;
	omapfb_info->screen_base = (char __iomem *)videomemory;
	omapfb_info->fix = omapfb_fix;
	omapfb_info->fix.smem_start = (unsigned long)videomemory;
	omapfb_info->fix.mmio_start = (unsigned long)videomemory;
	omapfb_info->fix.smem_len = videomemorysize;
	omapfb_info->fix.mmio_len = videomemorysize;

	/* Reuse framebuffer_alloc()'s extra allocation (info->par) as the
	 * pseudo_palette buffer, then clear par - matches the original's
	 * dword swap between the par and pseudo_palette fields exactly. */
	omapfb_info->pseudo_palette = omapfb_info->par;
	omapfb_info->par = NULL;

	ret = fb_alloc_cmap(&omapfb_info->cmap, 256, 0);
	if (ret < 0)
		goto err_release;

	ret = register_framebuffer(omapfb_info);
	if (ret < 0)
		goto err_dealloc_cmap;

	dev_set_drvdata(&pdev->dev, omapfb_info);
	OmapNKS4UpdateScreenInfo(videomemory, 800, 600);

	printk(KERN_INFO "fb%d: Virtual OMAP frame buffer device, using %ldK of video memory\n",
	       omapfb_info->node, videomemorysize >> 10);

	return 0;

err_dealloc_cmap:
	fb_dealloc_cmap(&omapfb_info->cmap);
err_release:
	framebuffer_release(omapfb_info);
	omapfb_info = NULL;
err_vfree:
	vfree(videomemory);
	videomemory = NULL;
	return ret;
}

int omapfb_remove(struct platform_device *pdev)
{
	struct fb_info *info = dev_get_drvdata(&pdev->dev);

	if (info) {
		unregister_framebuffer(info);
		vfree(videomemory);
		framebuffer_release(info);
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* /proc glue                                                           */
/* ------------------------------------------------------------------ */

int OmapVideoProcRead(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int fbnum = 99;

	if (off > 0) {
		*eof = 1;
		return 0;
	}

	if (omapfb_info)
		fbnum = omapfb_info->node;

	return sprintf(page, "OMAP Video fb%d:\n", fbnum);
}

int OmapVideoProcWrite(struct file *file, const char __user *buffer,
			unsigned long count, void *data)
{
	char *buf;
	int ret;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (!buf) {
		printk(KERN_INFO "OmapVideo:%s: line %d: cannot allocate memory\n",
		       __func__, 805);
		return -ENOMEM;
	}

	if (copy_from_user(buf, buffer, count)) {
		printk(KERN_INFO "OmapVideo:%s: line %d: copy from user\n",
		       __func__, 812);
		ret = -EFAULT;
	} else {
		buf[count] = 0;
		printk(KERN_INFO "OmapVideo:%s: line %d: write='%s'\n",
		       __func__, 819, buf);
		ret = count;
	}

	kfree(buf);
	return ret;
}

int OmapVideoProcInitialize(void)
{
	struct proc_dir_entry *entry = create_proc_entry("omapfb", 0, NULL);

	gProc = entry;
	if (!entry) {
		printk(KERN_INFO "OmapVideo:%s: line %d: cannot create proc entry\n",
		       __func__, 0x346);
		return -ENOMEM;
	}

	entry->read_proc = OmapVideoProcRead;
	entry->write_proc = OmapVideoProcWrite;
	return 0;
}

void OmapVideoProcDone(void)
{
	printk(KERN_INFO "OmapVideo:%s: line %d: enter\n", __func__, 0x358);
	remove_proc_entry("omapfb", NULL);
	gProc = NULL;
	printk(KERN_INFO "OmapVideo:%s: line %d: exit\n", __func__, 0x35e);
}

int OmapVideoProcInitialized(void)
{
	return gProc != NULL;
}

/* ------------------------------------------------------------------ */
/* Exported dimension accessors (the module's only two __ksymtab'd      */
/* symbols)                                                             */
/* ------------------------------------------------------------------ */

int GetScreenXDimension(void)
{
	return 800;
}
EXPORT_SYMBOL(GetScreenXDimension);

int GetScreenYDimension(void)
{
	return 600;
}
EXPORT_SYMBOL(GetScreenYDimension);
