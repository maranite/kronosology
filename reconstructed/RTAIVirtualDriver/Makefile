# Makefile for RTAIVirtualDriver.ko (software substitute for the real
# RTAI stack -- see RTAIVirtualDriver.c's own header comment and
# README.md for what this replaces and why).
#
# Kernel-only module, plain C (matches STGEnabler.c's own precedent for
# avoiding C++ against this ancient kernel's headers) -- no separate host
# build mode, unlike AT88VirtualChip/KorgUsbAudioVirtualDriver (this
# module has no freestanding, hardware-boundary-mockable algorithmic core
# to host-test in isolation the way chip-state/bignum logic does; its own
# logic is inseparable from real Linux kthread/semaphore/workqueue
# primitives that only exist in a real kernel build -- see README.md's
# own "Verification" section for why a host KAT was judged impractical
# here, matching the task's own explicit "a host KAT may not be
# practical for kernel-thread-based logic; use your judgment" allowance).
#
#   make KDIR=/path/to/linux-kronos   # build RTAIVirtualDriver.ko
#   make clean

obj-m := RTAIVirtualDriver.o

# Matches every other virtual driver's own confirmed build flags exactly
# (-mregparm=3 is calling-convention-critical here specifically -- see
# RTAIVirtualDriver.c's own per-function regparm(0) overrides, which only
# make sense relative to this file-wide regparm(3) default).
ccflags-y := -mregparm=3 -Wall

# Default to the running kernel's build tree; override KDIR for the
# Kronos tree, e.g.:
#   make KDIR=/home/build/linux-kronos
KDIR ?= /home/build/linux-kronos

PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) clean

.PHONY: all clean
