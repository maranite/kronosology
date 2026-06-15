# OA.ko drop-in reconstruction — build + verify
#
# Two independent checks:
#   make objs    - compile each reconstructed TU to a target-ABI object (codegen check,
#                  faithful to OA_322.ko: x86-32, -mregparm=3, freestanding, no C++ runtime).
#   make verify  - build & run the host known-answer tests for the pure auth math.
#   make         - both.
#
# Needs a multilib g++ (for -m32) and a host cc.  Objects are throwaway proof-of-compile
# artifacts (gitignored); the real .ko link is a later stage once all TUs are reconstructed.

TARGET_CXX      ?= g++
TARGET_CXXFLAGS := -m32 -mregparm=3 -fno-exceptions -fno-rtti -ffreestanding -fno-builtin \
                   -O2 -Wall -Iinclude

HOST_CC         ?= cc
HOST_CFLAGS     := -Wall -Wextra -O2 -Iinclude

SRC := src/auth/klm_manager.cpp \
       src/auth/products.cpp
OBJ := $(SRC:.cpp=.o)

TESTS := verify/test_klm_auth

.PHONY: all objs verify clean
all: objs verify

objs: $(OBJ)

%.o: %.cpp
	$(TARGET_CXX) $(TARGET_CXXFLAGS) -c $< -o $@

verify: $(TESTS)
	@for t in $(TESTS); do echo "== $$t =="; ./$$t; done

verify/test_klm_auth: verify/test_klm_auth.c include/oa_authmath.h
	$(HOST_CC) $(HOST_CFLAGS) $< -o $@

clean:
	rm -f $(OBJ) $(TESTS)
