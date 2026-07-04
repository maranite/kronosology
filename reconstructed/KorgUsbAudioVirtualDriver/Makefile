# KorgUsbAudioVirtualDriver — build + verify
#
# Same dual-mode Makefile convention as AT88VirtualChip/Makefile:
#
#   make            - host-side: compile korgusbaudio_stub.cpp as a
#                      freestanding x86-32 object (sanity check only) and
#                      build+run the host known-answer test. No kernel
#                      source tree needed.
#   make ko KDIR=... - the REAL kernel-module build via Kbuild.
#   make ko-clean KDIR=... - Kbuild clean.
#
# module_main.cpp (EXPORT_SYMBOL, module_init/exit) is kernel-only (real
# Linux headers) and deliberately excluded from the host SRC/OBJ list,
# same reasoning as AT88VirtualChip's own module_main.cpp split.

ifneq ($(KERNELRELEASE),)

# ---- Kbuild sub-make: building the real .ko -------------------------------
obj-m := KorgUsbAudioVirtualDriver.o
KorgUsbAudioVirtualDriver-objs := korgusbaudio_stub.o module_main.o

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

# ---- Top-level invocation: host test, plus the "ko" alias into Kbuild ----

TARGET_CXX      ?= g++
TARGET_CXXFLAGS := -m32 -mregparm=3 -fno-exceptions -fno-rtti -ffreestanding -fno-builtin \
                   -O2 -Wall

HOST_CXX        ?= g++
HOST_CXXFLAGS   := -Wall -Wextra -O2

SRC := korgusbaudio_stub.cpp
# `.hostobj`, NOT `.o` -- same OA.ko sec 10.143 fix (avoids colliding
# with Kbuild's own object paths below; see AT88VirtualChip/Makefile's
# own copy of this comment for the full story).
OBJ := $(SRC:.cpp=.hostobj)

TESTS := verify/test_korgusbaudio_stub

KDIR ?= /home/build/linux-kronos
PWD  := $(shell pwd)

.PHONY: all objs verify clean ko ko-clean
all: objs verify

objs: $(OBJ)

%.hostobj: %.cpp
	$(TARGET_CXX) $(TARGET_CXXFLAGS) -c $< -o $@

verify: $(TESTS)
	@for t in $(TESTS); do echo "== $$t =="; ./$$t; done

verify/test_korgusbaudio_stub: verify/test_korgusbaudio_stub.cpp korgusbaudio_stub.cpp korgusbaudio_stub.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_korgusbaudio_stub.cpp korgusbaudio_stub.cpp -o $@

# The real kernel-module build. Requires KDIR pointing at the Kronos
# 2.6.32.11-korg kernel source tree:
#   make ko KDIR=/path/to/kronos/linux-source
ko:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) modules

ko-clean:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) clean

clean:
	rm -f $(OBJ) $(TESTS)

endif
