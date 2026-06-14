# Makefile for loadoa — Kronos boot loader replacement
#
# Target: i386 Linux ELF (the Kronos runs a 32-bit ARM-derived Linux, but
# the /sbin binaries are i386 ELF cross-compiled for the OA SBC board).
#
# Prerequisites:
#   gcc-multilib     (for -m32 cross-build on an x86-64 host)
#   or a native i686-linux-gnu toolchain
#
#   sudo apt install gcc-multilib       # Debian/Ubuntu
#
# To build a stock-path-equivalent binary (for testing without patching):
#   make PATCH_PATHS=0

CC       ?= gcc
CFLAGS   := -m32 -O2 -Wall -std=gnu99
LDFLAGS  := -m32 -s

# Default: use patched (redirected) paths
PATCH_PATHS ?= 1

CFLAGS  += -DPATCH_PATHS=$(PATCH_PATHS)

.PHONY: all clean

all: loadoa

loadoa: loadoa.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f loadoa
