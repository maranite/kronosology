# Eva drop-in reconstruction — build + verify
#
# Eva is a normal dynamically-linked x86-32 userspace ELF, not a kernel module — see
# README.md's "Linking / build-ABI status" for why `make link`/`make eva` are best-effort
# right now (the real on-image glibc/libstdc++ version this needs to match hasn't been
# pinned down yet). Two checks are meaningful today:
#
#   make objs   - compile each reconstructed TU to a target-ABI object (-m32, ordinary
#                 g++, real libstdc++ semantics — unlike OA.ko this is NOT freestanding,
#                 exceptions/RTTI are not disabled). Proves each TU is real, compilable
#                 C++ against the recovered headers.
#   make verify - build & run the host known-answer tests in verify/ (once any exist —
#                 Stage 1's boot-path code has little pure/leaf logic worth KAT-testing
#                 yet; expect this target to be mostly empty until Stage 2).
#   make link   - best-effort real link of everything in objs/ against the HOST's own
#                 32-bit libc (NOT the real Kronos on-image libc — see README.md). This
#                 will show unresolved symbols for every not-yet-reconstructed Stage
#                 2/3 callee (CKernel::CKernel, SetConfigInfo, Mains, the OmegaXxxThread
#                 bodies, CSTGHandle::Access, USTGUserAPI::SendSTGMessageWithSource,
#                 CCommDriver::CCommDriver, ...) — expected, same as OA.ko's own
#                 documented "many Unknown symbol warnings" convention. What this proves
#                 is everything reconstructed so far uses real, correctly-mangled C++
#                 signatures matching what the callers expect, not that the program runs.
#   make        - objs + verify.

CXX      ?= g++
CXXFLAGS ?= -m32 -Wall -Wextra -g -Iinclude -std=gnu++98

SRC := $(shell find src -name '*.cpp')
OBJ := $(patsubst src/%.cpp,objs/%.o,$(SRC))

VERIFY_SRC := $(wildcard verify/test_*.cpp)
VERIFY_BIN := $(patsubst verify/%.cpp,objs/verify/%,$(VERIFY_SRC))

.PHONY: all objs verify link clean

all: objs verify

objs: $(OBJ)

objs/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

link: objs
	@mkdir -p objs
	-$(CXX) $(CXXFLAGS) -o objs/eva_boot_test $(OBJ) -lpthread
	@echo "--- link is best-effort; unresolved Stage 2/3 symbols above are EXPECTED, see Makefile header ---"

verify: $(VERIFY_BIN)
	@for t in $(VERIFY_BIN); do echo "-- $$t --"; $$t || exit 1; done

objs/verify/%: verify/%.cpp $(OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $< $(filter-out objs/init/eva_main.o,$(OBJ)) -lpthread

clean:
	rm -rf objs
