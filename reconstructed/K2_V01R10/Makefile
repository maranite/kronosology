# Syntax-correctness gate for the NKS4 panel firmware reconstruction.
#
# Each .c file here is reconstructed independently (own extern decls for
# cross-file symbols, matching the real firmware's separate translation
# units) and is compiled standalone with -c -fsyntax-only against the real
# target (ARM926EJ-S / TI OMAP-L1x) — this only proves the C is well-formed
# for that target, not that it behaves identically to the binary.
#
# Requires arm-none-eabi-gcc (build server: 192.168.3.92).

CC      := arm-none-eabi-gcc
CFLAGS  := -mcpu=arm926ej-s -mlittle-endian -ffreestanding -fsyntax-only \
           -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable

SRCS := $(wildcard *.c)

.PHONY: all clean
all:
	@ok=0; fail=0; \
	for f in $(SRCS); do \
		if $(CC) $(CFLAGS) -c $$f -o /dev/null 2>/tmp/nks4_build_err.$$$$; then \
			ok=$$((ok+1)); \
		else \
			fail=$$((fail+1)); \
			echo "=== FAIL: $$f ==="; \
			cat /tmp/nks4_build_err.$$$$; \
		fi; \
		rm -f /tmp/nks4_build_err.$$$$; \
	done; \
	echo "---"; \
	echo "$$ok/$$(($$ok+$$fail)) files compile cleanly"; \
	[ $$fail -eq 0 ]

clean:
	@true
