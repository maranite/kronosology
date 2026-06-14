# Makefile for STGEnabler.ko (Korg Kronos STG enabler shim)
#
# Builds an out-of-tree kernel module against the Kronos 2.6.32.11 + RTAI tree.
#
#   make                 # build STGEnabler.ko
#   make KDIR=/path/...  # build against a specific kernel tree
#   make clean

obj-m := STGEnabler.o

# Default to the running kernel's build tree; override KDIR for the Kronos tree, e.g.:
#   make KDIR=/mnt/tank/source/Kronos/linux-kronos
KDIR ?= /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean
