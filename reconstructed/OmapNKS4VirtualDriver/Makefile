# OmapNKS4VirtualDriver — build (kernel-module only, no host tests)
#
# See module_main.c's own header for why this module exists. Matches
# AT88VirtualChip/Makefile's two-mode structure, minus the host-test
# half (this module has no freestanding logic to unit-test -- every
# function is a trivial one-line stub).
#
#   make ko KDIR=...       - the real kernel-module build
#   make ko-clean KDIR=... - Kbuild clean

ifneq ($(KERNELRELEASE),)

obj-m := OmapNKS4VirtualDriver.o
OmapNKS4VirtualDriver-objs := module_main.o

# CRITICAL (found via a real kernel Oops, 2026-07-03): this whole
# kernel is built with -mregparm=3 (matching every other module in
# this project -- AT88VirtualChip/KorgUsbAudioVirtualDriver/OA.ko all
# set this). Without it, this module's own `init_module`/`cleanup_module`
# entry points use the wrong calling convention for how the kernel's
# own sys_init_module() calls them via a plain function pointer --
# this manifested as a NULL-pointer Oops INSIDE sys_init_module itself
# (not even reaching this module's own init code), hanging the insmod
# and leaving loadoa's subsequent OA.ko attempt with none of this
# module's exports available ("gave up waiting for init of module
# OmapNKS4VirtualDriver", then "Unknown symbol COmapNKS4Driver_*").
ccflags-y := -mregparm=3

else

# CORRECTED (2026-07-04): default KDIR switched to /home/build/linux-kronos
# (Korg's own korg_kronos_defconfig tree) -- the vanilla linux-2.6.32.11
# tree produces a struct module layout mismatch vs the real kernel; see
# /home/share/project_linux_kronos_kernel_tree.md. ARCH=i386 required
# explicitly (32-bit target, 64-bit host).
KDIR ?= /home/build/linux-kronos
PWD  := $(shell pwd)

.PHONY: ko ko-clean
ko:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) modules

ko-clean:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) clean

endif
