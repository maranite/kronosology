# Makefile for OmapNKS4VirtualBoard.ko (genuine USB gadget presenting a
# virtual NKS4 front-panel board -- see OmapNKS4VirtualBoard.c's own header
# comment for what this is and why).
#
# Kernel-only module, plain C. Needs CONFIG_USB_GADGET=m/CONFIG_USB_DUMMY_HCD=m
# enabled in the target kernel tree (see README.md for how this project's
# own /home/build/linux-kronos tree got those options added -- a purely
# additive config change, no ABI-relevant option touched).
#
#   make KDIR=/path/to/linux-kronos   # build OmapNKS4VirtualBoard.ko
#   make clean

obj-m := OmapNKS4VirtualBoard.o

ccflags-y := -mregparm=3 -Wall

KDIR ?= /home/build/linux-kronos

PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) clean

.PHONY: all clean
