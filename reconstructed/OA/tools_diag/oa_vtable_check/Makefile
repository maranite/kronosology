# SPDX-License-Identifier: GPL-2.0
# Build against the same prepared local kernel tree OA.ko itself uses
# (see reconstructed/OA/Makefile's own header comment for why /home/build,
# not /home/share).
KDIR ?= /home/build/linux-kronos

obj-m += oa_vtable_check.o

all:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) clean
