# Makefile / Kbuild for STGGmp.ko  (Korg "GMP Shell" = GNU MP 4.2.x in-kernel)
#
# This module = STGGmpModule.o (Korg glue, in this dir) + the GMP 4.2.x generic
# mpz/mpn objects.  The GMP sources are NOT bundled (LGPL upstream); place the
# kernel-adapted GMP tree under ./gmp/ first - see README.md and ./fetch-gmp.sh.
#
#   ./fetch-gmp.sh                       # fetch + adapt GMP 4.2.1 into ./gmp/
#   make KDIR=/mnt/tank/source/Kronos/linux-kronos
#   make clean

obj-m := STGGmp.o

# Korg glue + the GMP object set that the shipping STGGmp.ko exports.
STGGmp-objs := STGGmpModule.o \
	$(addprefix gmp/mpz/, add.o add_ui.o sub.o mul.o mul_2exp.o set.o set_str.o \
	    init.o iset_str.o iset_ui.o clear.o realloc.o tdiv_q.o tdiv_r.o powm.o \
	    invert.o gcdext.o) \
	$(addprefix gmp/mpn/, add_n.o sub_n.o mul_1.o addmul_1.o submul_1.o \
	    lshift.o rshift.o mul.o mul_n.o mul_basecase.o sqr_basecase.o mul_fft.o \
	    set_str.o tdiv_qr.o dc_divrem_n.o sb_divrem_mn.o divrem_1.o divrem_2.o \
	    diveby3.o gcdext.o tal-reent.o mp_bases.o mp_clz_tab.o mp_minv_tab.o \
	    dv_tab.o)

ccflags-y := -I$(src)/gmp -ffreestanding -D__GMP_WITHIN_GMP -DHAVE_CONFIG_H

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	@test -d gmp || { echo "ERROR: ./gmp not found - run ./fetch-gmp.sh first"; exit 1; }
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Build ONLY the Korg glue object (sanity-check the reconstructed source compiles).
glue:
	$(MAKE) -C $(KDIR) M=$(PWD) STGGmpModule.o

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all glue clean
