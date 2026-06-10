# Makefile for kronosology_updater_msg (DisplayUpdaterMessage replacement)
#
# The Kronos runs a 32-bit i386 Linux userspace, so the binary must be built
# as ELF32.  On a 64-bit host, install gcc-multilib and libfreetype6-dev:i386
# (Debian/Ubuntu) or equivalent, then run: make
#
# For explicit cross-compilation:
#   make CROSS=i686-linux-gnu-
#
# To build a native 64-bit binary for testing on the host (no -m32):
#   make M32=

CROSS ?=
CC    := $(CROSS)gcc
M32   ?= -m32

FT_CFLAGS := $(shell freetype-config --cflags 2>/dev/null || \
                     pkg-config --cflags freetype2 2>/dev/null || \
                     echo -I/usr/include/freetype2)

FT_LIBS   := $(shell freetype-config --libs 2>/dev/null || \
                     pkg-config --libs freetype2 2>/dev/null || \
                     echo -lfreetype)

CFLAGS  := -std=gnu99 $(M32) -fno-pie -fno-stack-protector -U_FORTIFY_SOURCE \
           -D_FORTIFY_SOURCE=0 -O2 -Wall -Wextra -Wno-unused-result \
           $(FT_CFLAGS)
LDFLAGS := $(M32) -no-pie -fno-pie -Wl,--wrap=__libc_start_main $(FT_LIBS) -lm

TARGET := DisplayUpdaterMessage
SRC    := kronosology_updater_msg.c glibc_compat.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	strip $@
	@echo "[build] $(TARGET) ready ($$(wc -c < $(TARGET)) bytes, $$(file $(TARGET) | grep -o 'ELF.*,'))"

clean:
	rm -f $(TARGET)

.PHONY: all clean
