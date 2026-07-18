# Makefile for OmapNKS4ProbeInject.ko - direct-probe-injection virtual NKS4 board
# (dummy_hcd TCG-hang workaround; see OmapNKS4ProbeInject.c's own header comment).
#
# Plain C, kernel-only module. Depends at link/insmod time on OmapNKS4Module.ko's
# own exported OmapNKS4Probe() symbol (must be insmod'd first) - KBUILD_EXTRA_SYMBOLS
# points modpost at that module's Module.symvers so it doesn't need to be rebuilt
# just because we're consuming its export.
#
#   make KDIR=/path/to/linux-kronos   # build OmapNKS4ProbeInject.ko
#   make clean

obj-m := OmapNKS4ProbeInject.o

ccflags-y := -mregparm=3 -Wall

KDIR ?= /home/build/linux-kronos
PWD  := $(shell pwd)

OMAPNKS4_DIR := $(PWD)/../OmapNKS4Module
KBUILD_EXTRA_SYMBOLS := $(OMAPNKS4_DIR)/Module.symvers

all:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) KBUILD_EXTRA_SYMBOLS=$(KBUILD_EXTRA_SYMBOLS) modules

clean:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) clean

.PHONY: all clean
