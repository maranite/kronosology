obj-m := oa_authgen.o

# Path to a *prepared* Kronos 2.6.32 kernel tree (bounds.h must exist).
# /mnt/source/Kronos/linux-kronos cannot be prepared in-place (no-symlink FS);
# use /tmp/linux-kronos which has already had `make prepare` run.
# Override: make KDIR=/path/to/linux-kronos
KDIR ?= /tmp/linux-kronos

# The Kronos hardware is i386; cross-compile if building on x86_64.
ARCH := i386

# Set CROSS_COMPILE to your i686-linux-gnu- or i386-linux-gnu- toolchain prefix
# if your host cannot produce i386 objects natively, e.g.:
#   make CROSS_COMPILE=i686-linux-gnu-
CROSS_COMPILE ?=

# GCC 14+ defaults to C23, which breaks 2.6.32 headers (bool/false redefined).
# -Wno-attributes: GCC 15 pedantry about log2.h conflicting noreturn/const attrs.
EXTRA_CFLAGS += -std=gnu11 -Wno-attributes

all:
	@touch $(KDIR)/Module.symvers 2>/dev/null || true
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) clean

# Quick sanity: confirm the target kernel version looks right
check-kver:
	@grep -r "KRONOS\|2\.6\.32" $(KDIR)/Makefile | head -3

.PHONY: all clean check-kver
