/*
 * omapvideo_internal.h - internal declarations for OmapVideoModule.ko
 *
 * Reconstructed from OmapVideoModule.ko (Korg Kronos /dev/fb1 front-panel
 * framebuffer driver), x86-32, Linux 2.6.32.11-korg + RTAI.  See
 * kronosology/docs/modules/OmapVideoModule.ko.md for the full writeup.
 *
 * File-layout convention mirrors the original binary's embedded .symtab
 * FILE markers: OmapVideoMod.c (module glue / MODULE_* macros),
 * OmapVideo.c (fb_ops + probe/remove + /proc glue + exported dimension
 * accessors), omapfb_fillrect.c / omapfb_copyarea.c / omapfb_imgblt.c
 * (statically-linked, pre-bswapmask-era cfb generic blit helpers).
 *
 * NOTE on calling convention: linux-kronos's arch/x86/Makefile builds the
 * whole kernel (and therefore every out-of-tree .ko built against it) with
 * -mregparm=3, so every function below uses the kernel's default 3-register
 * (EAX,EDX,ECX) argument-passing convention with NO per-function attribute
 * needed - this matches the original binary's __regparm3 decompilation
 * without requiring __attribute__((regparm(3))) sprinkled everywhere,
 * EXCEPT on the extern declarations for OmapNKS4Module.ko's exported
 * functions, which must be marked explicitly since their prototypes are
 * visible to the compiler only through this header (regparm is part of
 * the function type, not inferred from a global flag alone for externs
 * declared without a body in this TU - being explicit costs nothing and
 * removes any ambiguity).
 */
#ifndef OMAPVIDEO_INTERNAL_H
#define OMAPVIDEO_INTERNAL_H

#include <linux/module.h>
#include <linux/fb.h>
#include <linux/platform_device.h>

/* ------------------------------------------------------------------ */
/* Global module state (defined in OmapVideo.c)                        */
/* ------------------------------------------------------------------ */

extern struct fb_info        *omapfb_info;
extern struct platform_device *omapfb_device;
extern u32                   *videomemory;      /* vmalloc'd fb1 backing store */
extern unsigned long          videomemorysize;   /* module_param, "ulong" */
extern struct proc_dir_entry *gProc;             /* /proc/driver/omapfb (or similar) entry */

extern struct fb_var_screeninfo   omapfb_default;
extern struct fb_fix_screeninfo   omapfb_fix;
extern struct fb_ops              omapfb_ops;
extern struct platform_driver     omapfb_driver;

/* ------------------------------------------------------------------ */
/* fb_ops callbacks (OmapVideo.c)                                      */
/* ------------------------------------------------------------------ */

int  omapfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info);
int  omapfb_set_par(struct fb_info *info);
int  omapfb_setcolreg(unsigned regno, unsigned red, unsigned green,
		       unsigned blue, unsigned transp, struct fb_info *info);
int  omapfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info);
int  omapfb_mmap(struct fb_info *info, struct vm_area_struct *vma);
int  omapfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg);
ssize_t omapfb_sys_read(struct fb_info *info, char __user *buf, size_t count, loff_t *ppos);
ssize_t omapfb_sys_write(struct fb_info *info, const char __user *buf, size_t count, loff_t *ppos);

/* platform_driver .probe / .remove */
int  omapfb_probe(struct platform_device *pdev);
int  omapfb_remove(struct platform_device *pdev);

/* module_init / module_exit glue (OmapVideoMod.c) */
int  omapfb_init(void);
void omapfb_exit(void);

/* ------------------------------------------------------------------ */
/* Accelerated blit callbacks - non-static, but NOT EXPORT_SYMBOL'd in */
/* the original (no __ksymtab entry): confirmed via nm, kept plain     */
/* external linkage here to match the binary's symbol table exactly.  */
/* ------------------------------------------------------------------ */

void omapfb_fillrect(struct fb_info *p, const struct fb_fillrect *rect);
void omapfb_copyarea(struct fb_info *p, const struct fb_copyarea *area);
void omapfb_imageblit(struct fb_info *p, const struct fb_image *image);

/* ------------------------------------------------------------------ */
/* /proc glue (OmapVideo.c)                                            */
/* ------------------------------------------------------------------ */

int  OmapVideoProcInitialize(void);
void OmapVideoProcDone(void);
int  OmapVideoProcInitialized(void);
int  OmapVideoProcRead(char *page, char **start, off_t off, int count, int *eof, void *data);
int  OmapVideoProcWrite(struct file *file, const char __user *buffer,
			 unsigned long count, void *data);

/* ------------------------------------------------------------------ */
/* Exported accessors (__ksymtab'd in the original - the only two)     */
/* ------------------------------------------------------------------ */

int GetScreenXDimension(void);
int GetScreenYDimension(void);

/* ------------------------------------------------------------------ */
/* omapfb ioctl command numbers                                        */
/*                                                                      */
/* Decoded directly from the binary's CMP-chain immediates (magic 'r', */
/* standard Linux _IOW/_IOR encoding). Direction bits are copied        */
/* verbatim from the binary even where the actual data flow does not   */
/* match the encoded direction (several _IOW-encoded commands only     */
/* copy_to_user, never copy_from_user, in the real disassembly) -      */
/* faithfulness to the original wire protocol matters more than        */
/* "fixing" what looks like sloppy encoding upstream.                  */
/* ------------------------------------------------------------------ */

#define OMAPFB_IOCTL_MAGIC 'r'

#define OMAPFB_IOCTL_PING            _IOW(OMAPFB_IOCTL_MAGIC, 1, int)   /* 0x40047201 - reads+discards 4B, no NKS4 call (stub/validate-ptr only) */
#define OMAPFB_IOCTL_GETPROGRESSPCT2 _IOR(OMAPFB_IOCTL_MAGIC, 2, int)   /* 0x80047202 - returns hardcoded 0x37, no NKS4 call (debug stub) */
#define OMAPFB_IOCTL_DUMPPALETTE     _IOW(OMAPFB_IOCTL_MAGIC, 3, int)   /* 0x40047203 - prints palette[n..255] via printk */
#define OMAPFB_IOCTL_DUMPVIDMEM      _IOW(OMAPFB_IOCTL_MAGIC, 4, int)   /* 0x40047204 - prints 8 bytes of videomemory at offset */
#define OMAPFB_IOCTL_INITLCDREGS     _IOW(OMAPFB_IOCTL_MAGIC, 5, __u64) /* 0x40087205 - OmapNKS4InitLCDRegs(s8,s8,s32) */
#define OMAPFB_IOCTL_XAXISBYTESIZE   _IOW(OMAPFB_IOCTL_MAGIC, 6, int)   /* 0x40047206 - OmapNKS4XAxisByteSize(int) */
#define OMAPFB_IOCTL_SENDPIXELDATA   _IOW(OMAPFB_IOCTL_MAGIC, 7, __u32[3]) /* 0x400c7207 - OmapNKS4SendPixelDataRegion(3xint) */
#define OMAPFB_IOCTL_SENDFILLDATA    _IOW(OMAPFB_IOCTL_MAGIC, 8, __u64[2]) /* 0x40107208 - OmapNKS4SendFillData(s8,3xint) */
#define OMAPFB_IOCTL_UPDATECOLORPAL  _IOW(OMAPFB_IOCTL_MAGIC, 9, int)   /* 0x40047209 - OmapNKS4UpdateColorPal(s8,s8,s8,s8) */
#define OMAPFB_IOCTL_GETPROGRESSPCT  _IOW(OMAPFB_IOCTL_MAGIC, 10, int)  /* 0x4004720a - COmapNKS4_GetProgressBarPercent() */
#define OMAPFB_IOCTL_SETPROGRESSPCT  _IOW(OMAPFB_IOCTL_MAGIC, 11, int)  /* 0x4004720b - COmapNKS4_SetProgressBarPercent(u8) */
#define OMAPFB_IOCTL_INCPROGRESSBAR  _IOW(OMAPFB_IOCTL_MAGIC, 12, int)  /* 0x4004720c - COmapNKS4_IncProgressBar() */
#define OMAPFB_IOCTL_ADDTOPROGRESSBAR _IOW(OMAPFB_IOCTL_MAGIC, 13, int) /* 0x4004720d - COmapNKS4_AddToProgressBar(int) */
#define OMAPFB_IOCTL_GETTITLESCRVER  _IOW(OMAPFB_IOCTL_MAGIC, 14, int)  /* 0x4004720e - COmapNKS4_GetTitleScreenVersion() */

/* ------------------------------------------------------------------ */
/* OmapNKS4Module.ko externs                                           */
/*                                                                      */
/* Prototypes reconstructed from the caller side (this module's        */
/* argument setup at each call site) cross-checked against regparm(3)  */
/* register usage in the raw disassembly of omapfb_ioctl / omapfb_probe */
/* A full independent reconstruction of OmapNKS4Module.ko is out of    */
/* scope here - see kronosology/reconstructed/OmapNKS4Module/ for the  */
/* separate effort on that module.                                    */
/* ------------------------------------------------------------------ */

extern int  COmapNKS4_AddToProgressBar(int delta) __attribute__((regparm(3)));
extern unsigned char COmapNKS4_GetProgressBarPercent(void) __attribute__((regparm(3)));
extern int  COmapNKS4_GetTitleScreenVersion(void) __attribute__((regparm(3)));
extern int  COmapNKS4_IncProgressBar(void) __attribute__((regparm(3)));
extern int  COmapNKS4_SetProgressBarPercent(unsigned int percent) __attribute__((regparm(3)));

extern int  OmapNKS4InitLCDRegs(signed char a, signed char b, int c) __attribute__((regparm(3)));
extern int  OmapNKS4SendFillData(signed char a, int b, int c, int d) __attribute__((regparm(3)));
extern int  OmapNKS4SendPixelDataRegion(int a, int b, int c) __attribute__((regparm(3)));
extern int  OmapNKS4UpdateColorPal(signed char b0, signed char b1, signed char b2, signed char b3) __attribute__((regparm(3)));
extern void OmapNKS4UpdateScreenInfo(void *fb_ptr, int width, int height) __attribute__((regparm(3)));
extern int  OmapNKS4XAxisByteSize(int arg) __attribute__((regparm(3)));

#endif /* OMAPVIDEO_INTERNAL_H */
