# Makefile for dummy_hcd_fixed.ko -- a standalone, out-of-tree fork of real
# drivers/usb/gadget/dummy_hcd.c with a targeted dummy_timer() reentrancy fix
# for this kernel's I-pipe (Adeos/RTAI-patch) synchronous-replay-on-unstall
# semantics. See dummy_hcd_fixed.c's own header comment and this directory's
# README.md for what this replaces, why, and the live GDB evidence behind
# the fix.
#
# Same convention as every other "VirtualX"/fix module in this project
# (RTAIVirtualDriver.ko, AT88VirtualChip.ko, OmapNKS4VirtualBoard.ko): built
# entirely out-of-tree against /home/build/linux-kronos's HEADERS via KDIR=,
# never modifying that shared tree's own source. Needs
# CONFIG_USB_GADGET=m/CONFIG_USB_DUMMY_HCD=m enabled in the target kernel
# tree (already true -- OmapNKS4VirtualBoard/README.md documents how this
# project's own /home/build/linux-kronos tree got those options added, a
# purely additive config change).
#
#   make KDIR=/path/to/linux-kronos   # build dummy_hcd_fixed.ko
#   make clean

obj-m := dummy_hcd_fixed.o

ccflags-y := -mregparm=3 -Wall

KDIR ?= /home/build/linux-kronos

PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) clean

.PHONY: all clean
