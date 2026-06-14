# Makefile / Kbuild for OmapNKS4Module.ko (Korg Kronos front-panel USB driver)
#
# C++ kernel module for Linux 2.6.32.11 + RTAI, x86-32.  Depends on the Korg "STG"
# support layer + RTAI symbols (stg_* / rtwrap_*, exported by STGEnabler.ko / RTAI).
#
#   make KDIR=/mnt/tank/source/Kronos/linux-kronos
#   make clean
#
# NOTE: reconstruction in progress - see README.md.  Only the completed translation
# units are listed in omapnks4-objs below; add the others (driver/video/usb/...) as
# they are transcribed, or link the original objects for the rest.

obj-m := OmapNKS4Module.o

# All reconstructed translation units (C++ classes + C glue).  main.cpp/usb.cpp etc.
# are compiled as C++ so the whole module links with one toolchain (the STG build does
# the same; the .c-looking units use only C constructs).
OmapNKS4Module-objs := realtime.o command.o driver.o video.o \
		       usb.o procfs.o submit.o main.o

# C++-kernel build flags matching the original toolchain.
ccflags-y := -mregparm=3 -fno-exceptions -fno-rtti -fno-threadsafe-statics \
             -fno-use-cxa-atexit -fpermissive -I$(src)

# Teach Kbuild to compile .cpp with g++ (STG modules ship a similar rule).
$(obj)/%.o: $(src)/%.cpp
	$(CXX) $(c_flags) -x c++ -c -o $@ $<

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean
