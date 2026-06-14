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

# Configure for portable generic C, 32-bit ABI, no CPU assembly.  This generates
# config.h, gmp.h, gmp-impl.h, gmp-mparam.h, mp_bases.c, fac_ui (unused), etc.
echo ">> configuring (generic C, ABI=32, no assembly)"
./configure --disable-assembly --disable-shared --enable-static ABI=32 \
            CFLAGS="-O2 -m32" >/dev/null

# Build once on the host so the generated tables (mp_bases.c, fib/clz tables) exist.
echo ">> host build (generates tables)"
make -j"$(nproc)" >/dev/null 2>&1 || true   # we only need the generated sources

echo ">> staging sources into $DEST"
rm -rf "$DEST"
mkdir -p "$DEST/mpz" "$DEST/mpn"

# Top-level generated/config headers
for h in gmp.h gmp-impl.h gmp-mparam.h config.h longlong.h; do
    f=$(find . -maxdepth 2 -name "$h" | head -1)
    [ -n "$f" ] && cp "$f" "$DEST/"
done

# mpz sources used by the module
for c in add add_ui sub mul mul_2exp set set_str init iset_str iset_ui clear \
         realloc tdiv_q tdiv_r powm invert gcdext; do
    [ -f "mpz/$c.c" ] && cp "mpz/$c.c" "$DEST/mpz/"
done

# mpn generic sources + generated tables used by the module
for c in add_n sub_n mul_1 addmul_1 submul_1 lshift rshift mul mul_n mul_basecase \
         sqr_basecase mul_fft set_str tdiv_qr dc_divrem_n sb_divrem_mn divrem_1 \
         divrem_2 diveby3 gcdext tal-reent mp_bases mp_clz_tab mp_minv_tab dv_tab; do
    f="mpn/generic/$c.c"; [ -f "$f" ] || f="mpn/$c.c"
    [ -f "$f" ] && cp "$f" "$DEST/mpn/"
done

cat <<EOF

>> staged $(ls "$DEST"/mpz/*.c "$DEST"/mpn/*.c 2>/dev/null | wc -l) GMP source files into $DEST

NEXT: GMP's gmp-impl.h pulls in <stdio.h>/<stdlib.h>/<string.h>.  For a freestanding
kernel build, comment those includes out (the glue in STGGmpModule.c supplies the
allocators, __gmp_assert_fail and ctype).  Then:

    make KDIR=/mnt/tank/source/Kronos/linux-kronos
EOF
