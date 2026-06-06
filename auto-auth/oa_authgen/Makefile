obj-m := oa_authgen.o

# Path to the Kronos 2.6.32 kernel source tree (patched, as in linux-kronos).
# Override on the command line if your tree is elsewhere:
#   make KDIR=/path/to/linux-kronos
KDIR ?= /mnt/source/Kronos/linux-kronos

# The Kronos hardware is i386; cross-compile if building on x86_64.
ARCH := i386

# Set CROSS_COMPILE to your i686-linux-gnu- or i386-linux-gnu- toolchain prefix
# if your host cannot produce i386 objects natively, e.g.:
#   make CROSS_COMPILE=i686-linux-gnu-
CROSS_COMPILE ?=

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) clean

# Quick sanity: confirm the target kernel version looks right
check-kver:
	@grep -r "KRONOS\|2\.6\.32" $(KDIR)/Makefile | head -3

.PHONY: all clean check-kver
