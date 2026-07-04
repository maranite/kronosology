# AT88VirtualChip — build + verify
#
# Two completely different build modes share this one Makefile, same
# technique every out-of-tree kernel module Makefile uses (matching
# OmapNKS4Module/Makefile's own convention alongside this one):
#
#   make            - host-side: compile each unit as a freestanding
#                      x86-32 object (sanity check only, not linked) and
#                      build+run the host known-answer tests. Works on
#                      this dev machine, no kernel source tree needed.
#   make ko KDIR=... - the REAL kernel-module build: invokes Kbuild
#                      (`$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) modules`), which
#                      re-invokes THIS Makefile with KERNELRELEASE set,
#                      taking the `ifneq ($(KERNELRELEASE),)` branch below
#                      instead. KDIR must point at the actual Kronos
#                      2.6.32.11-korg kernel source tree (lives in WSL
#                      per CLAUDE.md's Development Environment section,
#                      not in this sandboxed environment -- this repo
#                      can write and sanity-check the source, but the
#                      real .ko has to be built where that tree exists).
#   make ko-clean KDIR=... - Kbuild clean.
#
# module_main.cpp (the real kernel glue: EXPORT_SYMBOL, module_init/exit,
# the RTAI-safe workqueue blob loader) is ONLY part of the `ko` build --
# it includes real Linux headers and won't compile as a freestanding host
# object, so it's deliberately excluded from SRC/OBJ below.

ifneq ($(KERNELRELEASE),)

# ---- Kbuild sub-make: building the real .ko -------------------------------
obj-m := AT88VirtualChip.o
AT88VirtualChip-objs := chip_state.o bignum.o b8_handshake.o nv2ac_exports.o \
			pairfact_fixture.o module_main.o

# Matches OmapNKS4Module's own confirmed build flags exactly (this module
# links into the same kernel, alongside it, and must use the same ABI).
ccflags-y := -mregparm=3 -fno-exceptions -fno-rtti -fno-threadsafe-statics \
             -fno-use-cxa-atexit -fpermissive -I$(src)

# CXX is NOT one of Kbuild's own variables (only $(CC) is defined in this
# context) -- it expands to empty here unless set explicitly, silently
# corrupting the compile command. Confirmed by actually running a real
# Kbuild build for the first time (see reconstructed/OA/'s own build,
# MASTER_REFERENCE.md sec 10.42) -- this bug was latent and untested here
# too until then.
CXX ?= g++

$(obj)/%.o: $(src)/%.cpp
	$(CXX) $(c_flags) -x c++ -c -o $@ $<

else

# ---- Top-level invocation: host tests, plus the "ko" alias into Kbuild ---

TARGET_CXX      ?= g++
TARGET_CXXFLAGS := -m32 -mregparm=3 -fno-exceptions -fno-rtti -ffreestanding -fno-builtin \
                   -O2 -Wall

HOST_CXX        ?= g++
HOST_CXXFLAGS   := -Wall -Wextra -O2

SRC := chip_state.cpp bignum.cpp b8_handshake.cpp nv2ac_exports.cpp pairfact_fixture.cpp
# `.hostobj`, NOT `.o` -- matches the OA.ko reconstruction's own sec
# 10.143 fix: this rule's outputs used to collide with Kbuild's own
# OA-objs-equivalent list below at the exact same `.o` paths, so
# running the host codegen check before `make ko` (no `clean` between)
# could silently link stale, wrong-flags host objects into the real
# `.ko`. A distinct suffix makes the two build modes unable to collide.
OBJ := $(SRC:.cpp=.hostobj)

TESTS := verify/test_chip_state verify/test_b8_handshake verify/test_nv2ac_exports verify/test_pairfact_fixture

KDIR ?= /home/build/linux-kronos
PWD  := $(shell pwd)

.PHONY: all objs verify clean ko ko-clean
all: objs verify

objs: $(OBJ)

%.hostobj: %.cpp
	$(TARGET_CXX) $(TARGET_CXXFLAGS) -c $< -o $@

verify: $(TESTS)
	@for t in $(TESTS); do echo "== $$t =="; ./$$t; done

verify/test_chip_state: verify/test_chip_state.cpp chip_state.cpp at88_chip.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_chip_state.cpp chip_state.cpp -o $@

verify/test_b8_handshake: verify/test_b8_handshake.cpp chip_state.cpp bignum.cpp b8_handshake.cpp at88_chip.h bignum.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_b8_handshake.cpp chip_state.cpp bignum.cpp b8_handshake.cpp -o $@

verify/test_nv2ac_exports: verify/test_nv2ac_exports.cpp chip_state.cpp bignum.cpp b8_handshake.cpp nv2ac_exports.cpp at88_chip.h bignum.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_nv2ac_exports.cpp chip_state.cpp bignum.cpp b8_handshake.cpp nv2ac_exports.cpp -o $@

verify/test_pairfact_fixture: verify/test_pairfact_fixture.cpp pairfact_fixture.cpp pairfact_fixture.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_pairfact_fixture.cpp pairfact_fixture.cpp -o $@

# The real kernel-module build. Requires KDIR pointing at the Kronos
# 2.6.32.11-korg kernel source tree (WSL-side, not this environment):
#   make ko KDIR=/path/to/kronos/linux-source
ko:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) modules

ko-clean:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) clean

clean:
	rm -f $(OBJ) $(TESTS)

endif
