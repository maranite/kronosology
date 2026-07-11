#!/bin/sh
# fetch-gmp.sh - download GNU MP 4.2.x and stage its generic C sources into ./gmp/
# for building STGGmp.ko.  Run from this directory.  Requires network + a host gcc.
#
# GMP 4.2.x is LGPL-2.1+; it is fetched from the GNU archive rather than bundled.

set -e
VER="${GMP_VERSION:-4.2.1}"
URL="https://ftp.gnu.org/gnu/gmp/gmp-${VER}.tar.bz2"
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$HERE/.gmp-build"
DEST="$HERE/gmp"

echo ">> fetching gmp-$VER"
mkdir -p "$WORK"
cd "$WORK"
[ -f "gmp-$VER.tar.bz2" ] || wget -O "gmp-$VER.tar.bz2" "$URL"
rm -rf "gmp-$VER"
tar xjf "gmp-$VER.tar.bz2"
cd "gmp-$VER"

# Configure for a truly generic (no per-CPU assembly) build with 32-bit limbs.
#
# IMPORTANT, found the hard way: GMP 4.2.1's ./configure does NOT actually
# recognize "--disable-assembly" as a real feature flag at all (it's absent
# from `./configure --help`'s Optional Features list -- autoconf silently
# accepts and ignores any unknown --disable-FOO). Passing it is a silent
# no-op: configure still autodetects the BUILD host's own CPU (x86_64 here)
# and picks real x86_64 mpn/x86_64/*.asm files plus HAVE_NATIVE_mpn_*=1
# flags for them, even with ABI=32/CFLAGS=-m32 -- those only affect the
# LIMB WIDTH, not which per-CPU assembly path gets selected.
#
# The actual documented way to force GMP onto its fully generic mpn/generic/
# C path (what a from-scratch kernel-module glue needs, since we're staging
# .c files, not real target assembly) is a genuine cross-configure with the
# magic CPU token "none" in --host -- GMP's own acinclude.m4 special-cases
# "none-*" hosts to select path=generic with every HAVE_NATIVE_mpn_* left
# unset. "none" hosts don't understand ABI=32 (only "long"/"longlong"), so
# ABI=long is used instead -- combined with CFLAGS=-m32 this still yields a
# 32-bit mp_limb_t (plain `long`), matching the real target's 32-bit build
# and this project's own oa_gmp.h ABI assumption.
echo ">> configuring (generic C mpn path, --host=none-* cross-configure trick, 32-bit limbs)"
./configure --disable-shared --enable-static ABI=long \
            --build=x86_64-unknown-linux-gnu --host=none-unknown-linux-gnu \
            CFLAGS="-O2 -m32" >/dev/null

# Build once on the host so the generated tables (mp_bases.c, fib/clz tables) exist.
echo ">> host build (generates tables)"
make -j"$(nproc)" >/dev/null 2>&1 || true   # we only need the generated sources

# Sanity check: fail loudly (not silently ship broken native-call assumptions)
# if the "none" host trick above ever stops working in some other GMP point
# release -- every HAVE_NATIVE_mpn_* must be unset for the staged .c set to
# be self-contained (no missing mpn_*_nc-style native callees).
if grep -q '^#define HAVE_NATIVE_mpn_add_nc' config.h; then
    echo "ERROR: HAVE_NATIVE_mpn_add_nc is still defined -- the --host=none-*" >&2
    echo "generic-path trick did not work for this GMP version; investigate" >&2
    echo "before staging (see the comment above this configure call)." >&2
    exit 1
fi

echo ">> staging sources into $DEST"
rm -rf "$DEST"
mkdir -p "$DEST/mpz" "$DEST/mpn"

# Top-level generated/config headers.  mp.h is GMP's old-style compat header,
# pulled in by every mpz/*.c via "mp.h" -- easy to miss since it's not one of
# the "obviously generated" ones.  fib_table.h/mp_bases.h are generated
# lookup-table headers gmp-impl.h itself unconditionally #includes
# (WANT_FAT_BINARY is 0 for this generic/non-assembly config, so fat.h is
# NOT needed and deliberately not staged).
for h in gmp.h gmp-impl.h gmp-mparam.h config.h longlong.h mp.h fib_table.h mp_bases.h; do
    f=$(find . -maxdepth 2 -name "$h" | head -1)
    [ -n "$f" ] && cp "$f" "$DEST/"
done

# mpz sources used by the module
for c in add add_ui sub mul mul_2exp set set_str init iset_str iset_ui clear \
         realloc tdiv_q tdiv_r powm invert gcdext; do
    [ -f "mpz/$c.c" ] && cp "mpz/$c.c" "$DEST/mpz/"
done

# mpz/*.c files #include private per-directory helper headers (e.g. add.c/
# sub.c both #include "aors.h") that are easy to miss if only .c files are
# staged -- stage every mpz/*.h alongside them, not just the ones referenced
# by name above (cheap, and covers any implicit dependency of the .c set).
cp mpz/*.h "$DEST/mpz/" 2>/dev/null || true

# mpn generic sources used by the module -- live under mpn/generic/ or,
# for a few, directly under mpn/.
for c in add_n sub_n mul_1 addmul_1 submul_1 lshift rshift mul mul_n mul_basecase \
         sqr_basecase mul_fft set_str tdiv_qr dc_divrem_n sb_divrem_mn divrem_1 \
         divrem_2 diveby3 gcdext mp_bases; do
    f="mpn/generic/$c.c"; [ -f "$f" ] || f="mpn/$c.c"
    [ -f "$f" ] && cp "$f" "$DEST/mpn/"
done

# Generated-table + reentrant-allocator sources: in gmp 4.2.1 these live at
# the SOURCE TREE TOP LEVEL (not mpn/ or mpn/generic/) -- easy to miss since
# every other staged file above comes from mpn/*.  mp_dv_tab.c is renamed to
# dv_tab.c on the way in to match this Makefile's own `dv_tab.o` object name
# (the other three keep their real on-disk names).
[ -f tal-reent.c ]    && cp tal-reent.c    "$DEST/mpn/"
[ -f mp_clz_tab.c ]   && cp mp_clz_tab.c   "$DEST/mpn/"
[ -f mp_minv_tab.c ]  && cp mp_minv_tab.c  "$DEST/mpn/"
[ -f mp_dv_tab.c ]    && cp mp_dv_tab.c    "$DEST/mpn/dv_tab.c"

# mpn/generic/mul_fft.c does `#include "generic/addsub_n.c"` (a raw source
# include, not a declaration header) relative to its OWN pre-flatten
# directory (mpn/) -- since we flatten mpn/generic/*.c straight into
# $DEST/mpn/, that relative path needs an actual $DEST/mpn/generic/
# subdirectory holding addsub_n.c, distinct from the flattened copies above.
mkdir -p "$DEST/mpn/generic"
[ -f mpn/generic/addsub_n.c ] && cp mpn/generic/addsub_n.c "$DEST/mpn/generic/"

# config.h's HAVE_*_H flags reflect the BUILD host's own real glibc headers
# (AC_CHECK_HEADERS ran against the actual host gcc, "none" cross-configure
# notwithstanding -- there's no real cross-toolchain here, just a fake CPU
# token) -- but the freestanding kernel build has none of stdio.h/stdlib.h/
# string.h/inttypes.h/stdint.h/alloca.h/locale.h/sys/types.h/unistd.h, and
# gmp-impl.h/gmp.h #include each one only when its HAVE_ flag says to. Force
# every such header-presence flag to 0 in the STAGED copy only (the .gmp-
# build/ host tree's own config.h is untouched, so a stray re-run of `make`
# there for host testing still works normally). The glue in STGGmpModule.c
# supplies what these would have provided (kmalloc-based allocators,
# __gmp_assert_fail, a ctype table for isspace/isdigit).
echo ">> forcing host-only HAVE_*_H flags to 0 in the staged config.h"
sed -i -E 's/^#define (HAVE_(INTTYPES|STDINT|ALLOCA|LOCALE|STDLIB|STRING|SYS_TYPES|UNISTD)_H) 1/\/* #undef \1 *\//' \
    "$DEST/config.h"

# A handful of mpz/*.c and mpn/*.c files #include <stdio.h>/<string.h>
# UNCONDITIONALLY (not gated by any HAVE_*_H check -- GMP assumes a C89
# hosted environment where these always exist). What they actually need
# from them, in every file this project's own Makefile pulls in, is either
# just the NULL macro (mpz/mul.c, mpz/gcdext.c: "for NULL"; mpn/tal-reent.c:
# unused dead weight, no stdio symbol referenced anywhere in the file) or a
# real strlen (mpz/set_str.c) -- or, in mpn/mul_fft.c's case, printf calls
# that are entirely compiled out (wrapped in this file's own `#define
# TRACE(x)` no-op debug-trace macro, confirmed via grep before relying on
# this). Rather than patching each vendor .c file, stage tiny shim headers
# with the SAME names ($DEST is first on the include path via ccflags-y's
# -I$(src)/gmp) that provide exactly this and nothing else -- no libc
# assumed, no libc reimplemented.
cat > "$DEST/stdio.h" <<'SHIM'
/* Freestanding-kernel-build shim for GMP source files that unconditionally
   #include <stdio.h> "for NULL" (or, in mul_fft.c's case, for printf calls
   that are compiled out by that file's own no-op TRACE() macro -- verified,
   not assumed). See fetch-gmp.sh's own comment for the full derivation. */
#ifndef _STGGMP_STDIO_SHIM_H
#define _STGGMP_STDIO_SHIM_H
#include <linux/stddef.h>
#endif
SHIM
cat > "$DEST/string.h" <<'SHIM'
/* Freestanding-kernel-build shim for GMP source files (mpz/set_str.c) that
   unconditionally #include <string.h> for a real strlen() -- forward to the
   kernel's own real implementation rather than reimplementing libc. */
#ifndef _STGGMP_STRING_SHIM_H
#define _STGGMP_STRING_SHIM_H
#include <linux/string.h>
#endif
SHIM
cat > "$DEST/ctype.h" <<'SHIM'
/* Freestanding-kernel-build shim for GMP source files (mpz/set_str.c, and
   any other *_set_str caller) that unconditionally #include <ctype.h> and
   call the glibc isspace()/isdigit()/... macros. STGGmpModule.c's own
   __ctype_b_loc() (see this project's README.md sec 3) is a genuine
   glibc-ABI-compatible ctype table -- this header just supplies the same
   macro expansion glibc's own <ctype.h> uses on top of it
   ((*__ctype_b_loc())[c] & _ISxxx), so real GMP source calling isspace(c)
   needs no further change. Only isspace/isdigit are defined (the only two
   this project's currently-staged .c set actually calls, confirmed via
   grep) -- extend with the same _ISxxx bits STGGmpModule.c already defines
   if a future symbol set needs more. */
#ifndef _STGGMP_CTYPE_SHIM_H
#define _STGGMP_CTYPE_SHIM_H
extern const unsigned short **__ctype_b_loc(void);
#define _STGGMP_ISbit(bit) ((bit) < 8 ? ((1 << (bit)) << 8) : ((1 << (bit)) >> 8))
#define isspace(c) ((*__ctype_b_loc())[(int)(c)] & _STGGMP_ISbit(5))
#define isdigit(c) ((*__ctype_b_loc())[(int)(c)] & _STGGMP_ISbit(3))
#endif
SHIM

cat <<EOF

>> staged $(ls "$DEST"/mpz/*.c "$DEST"/mpn/*.c 2>/dev/null | wc -l) GMP source files into $DEST

Next: make KDIR=/home/build/linux-kronos   (see README.md)
EOF
