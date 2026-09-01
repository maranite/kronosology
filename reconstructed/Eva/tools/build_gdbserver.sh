#!/bin/sh
# build_gdbserver.sh - cross-compile a static i386 gdbserver for the kronos_vm guest,
# using the musl-i386 toolchain at /home/build/devroot/musl-i386.
#
# Why gdb 7.6.2 specifically: gdbserver's C++ rewrite landed in gdb 7.7. Everything
# 7.6.x and earlier is plain C, which builds cleanly with plain musl-gcc -- no musl
# libstdc++ (which doesn't exist in this toolchain) needed. 7.6.2 is the last release
# in the pre-rewrite 7.6 series. Confirmed working against a real ptrace session
# (2026-07-26): gdbserver ran a static musl test binary, a host gdb (Debian 13.1,
# auto-multiarch, no gdb-multiarch package needed) attached over TCP, read registers,
# and let the inferior run to a clean exit.
#
# Two patches were required over vanilla 7.6.2 source, both in gdb/gdbserver/:
#   1. gdb/common/linux-ptrace.h uses pid_t before it's declared. glibc's
#      <sys/ptrace.h> transitively pulls in <sys/types.h>; musl's does not. Fix:
#      add `#include <sys/types.h>` above `#include <sys/ptrace.h>` in that header.
#   2. thread-db.c/proc-service.c (libthread_db integration) do not build against
#      musl -- musl has no libthread_db.so and no <thread_db.h>, so `thread_t`,
#      `LIBTHREAD_DB_SO`, `LIBTHREAD_DB_SEARCH_PATH`, and struct lwp_info's `th`
#      field are all undefined/mismatched (the config.h TD_VERSION check correctly
#      detects "no", but the target Makefile still unconditionally lists
#      thread-db.o/proc-service.o in DEPFILES and force-defines -DUSE_THREAD_DB only
#      for linux-low.o, not thread-db.o, producing a struct-layout mismatch between
#      the two TUs). Fix: drop thread-db.o and proc-service.o from DEPFILES, and drop
#      -DUSE_THREAD_DB from the linux-low.o rule, in gdb/gdbserver/Makefile (post-
#      configure, hand-edited -- these come from a target-fragment Makefile.in
#      that isn't worth patching for a one-off build). Consequence: no libthread_db
#      TLS-variable resolution and no thread_db-driven thread creation events --
#      gdbserver falls back to plain /proc/PID/task LWP enumeration for
#      multi-threaded targets, which is sufficient for breakpoint/backtrace work
#      and is what happens anyway at runtime on a target with no libthread_db.so
#      (i.e. this guest) even with a thread_db-capable build.
#
# The resulting binary needs NO other target support (musl libc.a static-links
# everything it needs): confirmed `file` shows "statically linked", zero dynamic
# interpreter, runs standalone.

set -e

WORK=/home/build/eva-gdbserver
GDB_VER=7.6.2
MUSL=/home/build/devroot/musl-i386
OUT=/home/share/kronosology/reconstructed/Eva/tools/gdbserver-i386-musl

mkdir -p "$WORK"
cd "$WORK"

if [ ! -d "gdb-$GDB_VER" ]; then
    [ -f "gdb-$GDB_VER.tar.bz2" ] || curl -sO "https://ftp.gnu.org/gnu/gdb/gdb-$GDB_VER.tar.bz2"
    tar xjf "gdb-$GDB_VER.tar.bz2"
fi

SRC="$WORK/gdb-$GDB_VER/gdb/gdbserver"

# Patch 1: pid_t visibility under musl.
grep -q '#include <sys/types.h>' "$WORK/gdb-$GDB_VER/gdb/common/linux-ptrace.h" || \
    sed -i 's/#include <sys\/ptrace.h>/#include <sys\/types.h>\n#include <sys\/ptrace.h>/' \
        "$WORK/gdb-$GDB_VER/gdb/common/linux-ptrace.h"

cd "$SRC"
[ -f Makefile ] || \
    CC="$MUSL/bin/musl-gcc" CFLAGS="-static -Os" LDFLAGS="-static" \
    ./configure --disable-nls

# Patch 2: drop libthread_db integration (not available under musl, not needed --
# see header comment above). Only touch the Makefile once.
if grep -q 'thread-db.o proc-service.o' Makefile; then
    sed -i 's/ thread-db.o proc-service.o$//' Makefile
    sed -i '/^linux-low.o: linux-low.c$/,/POSTCOMPILE/{s/\$(COMPILE) \$< -DUSE_THREAD_DB/$(COMPILE) $</}' Makefile
fi

rm -f thread-db.o proc-service.o linux-low.o gdbserver

make -j"$(nproc)" \
    CC="$MUSL/bin/musl-gcc" CFLAGS="-static -Os" LDFLAGS="-static" \
    WERROR_CFLAGS="" \
    gdbserver

strip -o gdbserver.stripped gdbserver
cp gdbserver.stripped "$OUT"
chmod +x "$OUT"

file "$OUT"
echo "Built: $OUT"
