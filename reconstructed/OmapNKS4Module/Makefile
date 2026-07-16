# Makefile / Kbuild for OmapNKS4Module.ko (Korg Kronos front-panel USB driver)
#
# C++ kernel module for Linux 2.6.32.11 + RTAI, x86-32.  Depends on the Korg "STG"
# support layer + RTAI symbols (stg_* / rtwrap_*, exported by STGEnabler.ko / RTAI).
#
#   make KDIR=/mnt/tank/source/Kronos/linux-kronos   (the real .ko build)
#   make clean
#   make verify                                       (host-side known-answer tests,
#                                                        no kernel tree needed - see
#                                                        verify/README.md)
#
# NOTE: reconstruction in progress - see README.md.  Only the completed translation
# units are listed in omapnks4-objs below; add the others (driver/video/usb/...) as
# they are transcribed, or link the original objects for the rest.
#
# Same dual-mode Makefile technique as ../AT88VirtualChip/Makefile: this file is
# re-invoked by Kbuild with KERNELRELEASE set for the real .ko build (the
# `ifneq ($(KERNELRELEASE),)` branch); a plain top-level `make`/`make verify`
# takes the host-test branch below instead.

ifneq ($(KERNELRELEASE),)

obj-m := OmapNKS4Module.o

# All reconstructed translation units (C++ classes + C glue).  main.cpp/usb.cpp etc.
# are compiled as C++ so the whole module links with one toolchain (the STG build does
# the same; the .c-looking units use only C constructs).
OmapNKS4Module-objs := realtime.o command.o driver.o video.o \
		       usb.o procfs.o submit.o main.o softfloat.o

# C++-kernel build flags matching the original toolchain.
ccflags-y := -mregparm=3 -fno-exceptions -fno-rtti -fno-threadsafe-statics \
             -fno-use-cxa-atexit -fpermissive -I$(src)

# Teach Kbuild to compile .cpp with g++ (STG modules ship a similar rule).
$(obj)/%.o: $(src)/%.cpp
	$(CXX) $(c_flags) -x c++ -c -o $@ $<

CXX ?= g++

else

# ---- Top-level invocation: host verify tests, plus the "ko" alias into Kbuild ----

KDIR ?= /home/build/linux-kronos
PWD  := $(shell pwd)

# -m32 is required here (unlike a plain host-only project): omapnks4_internal.h
# declares a kernel-style `operator new(unsigned int)`, which only matches the
# required `operator new(size_t)` signature when size_t is 4 bytes - true on the
# module's real x86-32 target, not on a 64-bit host without -m32.
HOST_CXX      ?= g++
HOST_CXXFLAGS := -m32 -Wall -Wextra -O2

TESTS := verify/test_command verify/test_driver_receive_event_buffer

.PHONY: all verify clean ko ko-clean

all: verify

verify: $(TESTS)
	@for t in $(TESTS); do echo "== $$t =="; ./$$t; done

verify/test_command: verify/test_command.cpp verify/host_stubs.cpp command.cpp
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_command.cpp verify/host_stubs.cpp command.cpp -o $@

verify/test_driver_receive_event_buffer: verify/test_driver_receive_event_buffer.cpp verify/host_stubs.cpp driver.cpp
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_driver_receive_event_buffer.cpp verify/host_stubs.cpp driver.cpp -o $@

clean:
	rm -f $(TESTS)

ko:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

ko-clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

endif
