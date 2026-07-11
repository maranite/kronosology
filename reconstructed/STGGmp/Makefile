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

# -Duint_least32_t=...: gmp-impl.h's gmp_uint_least32_t typedef uses this
# libc <stdint.h>/<inttypes.h> type name unconditionally (not inside the
# HAVE_INTTYPES_H/HAVE_STDINT_H #if that guards the *header includes* --
# only the include is guarded, not this one use of the type name itself),
# but the freestanding kernel build has neither header (fetch-gmp.sh forces
# both HAVE_ flags to 0 in the staged config.h since neither exists here) --
# supply the type as a plain object-like macro instead of patching gmp-impl.h.
# -fgnu89-inline: gmp.h declares a batch of small mpz_/mpn_/mpq_ accessor
# wrappers (mpz_abs, mpn_add, mpq_neg, ...) as classic K&R/GNU89 "extern
# inline" functions (`__GMP_EXTERN_INLINE = extern __inline__`, gmp.h's own
# pre-C99 GCC-version-detection idiom -- correct for the GCC ~3.x/4.x this
# 2.6.32-era code was written against). Modern GCC (12.x on this build
# host) defaults to C99/GNU11 inline semantics, under which "extern inline"
# means something entirely different (roughly the opposite: emit a REAL,
# non-weak definition in THIS translation unit unless overridden) -- so
# EVERY .c file that includes gmp.h was independently emitting its own
# strong `__gmpz_abs`/`__gmpn_add`/etc. definitions, causing "multiple
# definition" errors at the final module link even though none of this
# project's own staged/reconstructed sources ever calls any of them.
# -fgnu89-inline restores the old semantics gmp.h's own detection assumed,
# matching how this code was actually built by Korg circa a period-correct
# GCC. (The ancient target kernel's OWN Makefile doesn't need this flag
# itself -- 2.6.32 predates the GCC5 default change -- so it's added only
# here, not globally; this is a genuinely GMP-specific, not kernel-wide,
# compatibility fix.)
ccflags-y := -I$(src)/gmp -ffreestanding -D__GMP_WITHIN_GMP -DHAVE_CONFIG_H \
	-fgnu89-inline \
	-Duint_least32_t="unsigned int"

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
