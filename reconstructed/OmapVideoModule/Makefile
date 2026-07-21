# Makefile / Kbuild for OmapVideoModule.ko (Korg Kronos /dev/fb1 framebuffer driver)
#
# Pure C kernel module for Linux 2.6.32.11-korg + RTAI, x86-32. Depends on
# 11 exported symbols from OmapNKS4Module.ko (COmapNKS4_*/OmapNKS4_* - see
# omapvideo_internal.h) - those stay unresolved at build time here exactly
# as they do in the original binary (nm shows them all "U"); the original's
# own .modinfo "depends=" field is empty too (OmapNKS4Module.ko wasn't in
# the same Module.symvers pass when this was built upstream), so we
# deliberately do NOT point KBUILD at OmapNKS4Module's Module.symvers -
# doing so would populate depends= and diverge from the ground truth.
#
#   make            (the real .ko build, KDIR defaults to /home/build/linux-kronos)
#   make clean
#
# Same dual-mode Makefile technique used by ../OmapNKS4Module/Makefile and
# ../AT88VirtualChip/Makefile: this file is re-invoked by Kbuild with
# KERNELRELEASE set for the real .ko build (the `ifneq ($(KERNELRELEASE),)`
# branch); a plain top-level `make` takes the host branch below, which just
# forwards into the KDIR build - there is no host-only verify harness for
# this module (unlike OmapNKS4Module, everything here is plain fb_ops /
# cfb-blit logic with no floating point, no RTAI, nothing meaningfully
# testable off-target).
#
# Build location note: /home/share is a CIFS mount and cannot hold the
# symlinks a kernel `modules_prepare` needs. That constraint applies to
# KDIR (the kernel tree doing the preparing), not to this M= directory -
# OmapNKS4Module.ko already proved out M=<cifs path> works fine as long as
# KDIR itself is local (/home/build/linux-kronos, ZFS). This Makefile
# follows the same pattern unchanged.

ifneq ($(KERNELRELEASE),)

obj-m := OmapVideoModule.o

OmapVideoModule-objs := OmapVideoMod.o OmapVideo.o \
			 omapfb_fillrect.o omapfb_copyarea.o omapfb_imgblt.o

ccflags-y := -I$(src)

else

KDIR ?= /home/build/linux-kronos
PWD  := $(shell pwd)

.PHONY: all clean ko ko-clean

all: ko

ko:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

ko-clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

clean: ko-clean

endif
