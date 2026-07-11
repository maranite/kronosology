#!/bin/sh
# run_xsize_kat.sh -- host-side KAT for the fetch-gmp.sh xsize
# (__floatunsidf/__fixdfsi/__divdf3 avoidance) patch to mpz/set_str.c.
#
# Prerequisite: ./fetch-gmp.sh must already have been run in this
# module's own top directory (../gmp/ must exist, with both
# mpz/set_str.c (patched) and mpz/set_str.c.orig (pristine backup) --
# fetch-gmp.sh creates both automatically as part of staging).
#
# Two independent checks, both must pass:
#
#   1. test_xsize_sweep: a direct numeric safety-margin sweep proving
#      the integer replacement never under-estimates the original
#      double-precision formula, for every base GMP defines and
#      str_size 0..8192 -- exit code is the pass/fail signal.
#
#   2. test_xsize_patch_driver: built TWICE, once against the ORIGINAL
#      (float) mpz/set_str.c.orig and once against the PATCHED (integer)
#      mpz/set_str.c, both linked against the real vendor mpz/*.c +
#      mpn/*.c object set (the same curated set STGGmp.ko's own Makefile
#      uses) via a tiny host-only allocator/assert glue (malloc-based,
#      not kmalloc -- this runs as a plain host program, not a kernel
#      module). The two builds' outputs are diffed: `alloc=` (internal
#      pre-allocation hint) may differ (patched >= original is expected
#      and fine); `size=`/`limbs=` (the actual computed bignum result)
#      must be byte-identical on every line, proving the patch has zero
#      effect on any user-visible/cryptographic output.
#
# Both builds use -O2 -fgnu89-inline (matching STGGmp.ko's own Kbuild
# ccflags-y): at -O0 several small mpn_add_1/mpn_sub_1/mpn_cmp GNU89
# "extern inline" wrappers in gmp.h never get inlined and are left as
# genuinely unresolved externals (GNU89 "extern inline" never emits an
# out-of-line definition anywhere) -- -O2 reliably inlines them away,
# same as the real kernel module build relies on.

set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../gmp"
WORK="${STG_XSIZE_KAT_WORK:-$HERE/.xsize_kat_work}"

if [ ! -d "$SRC" ] || [ ! -f "$SRC/mpz/set_str.c.orig" ]; then
    echo "ERROR: $SRC not staged (or missing set_str.c.orig) -- run" >&2
    echo "../fetch-gmp.sh first." >&2
    exit 1
fi

rm -rf "$WORK"
mkdir -p "$WORK/orig/mpz" "$WORK/orig/mpn" "$WORK/patched/mpz" "$WORK/patched/mpn"

for variant in orig patched; do
    d="$WORK/$variant"
    for h in gmp.h gmp-impl.h gmp-mparam.h config.h longlong.h mp.h fib_table.h mp_bases.h stg_gmp_xsize.h; do
        cp "$SRC/$h" "$d/"
    done
    cp "$SRC"/mpz/*.c "$d/mpz/" 2>/dev/null
    cp "$SRC"/mpz/*.h "$d/mpz/" 2>/dev/null
    cp "$SRC"/mpn/*.c "$d/mpn/" 2>/dev/null
    # Host build: use the REAL system libc headers for stdio/string/ctype
    # (this is a plain host userspace program, not freestanding) -- do
    # NOT copy the kernel-only stdio.h/string.h/ctype.h shims, and
    # re-enable the HAVE_*_H flags fetch-gmp.sh forces off for the
    # kernel build (the host genuinely has all of these).
    python3 - "$d/config.h" <<'PYEOF'
import re, sys
p = sys.argv[1]
s = open(p).read()
s = re.sub(r'/\* #undef (HAVE_(?:INTTYPES|STDINT|ALLOCA|LOCALE|STDLIB|STRING|SYS_TYPES|UNISTD)_H) \*/', r'#define \1 1', s)
open(p, 'w').write(s)
PYEOF
done
cp "$SRC/mpz/set_str.c.orig" "$WORK/orig/mpz/set_str.c"
# $WORK/patched/mpz/set_str.c is already the real patched copy (from the
# `cp "$SRC"/mpz/*.c` above).

cat > "$WORK/host_glue.c" <<'EOF'
/* Host-only glue for this KAT -- mirrors STGGmpModule.c's role but uses
 * plain malloc/realloc/free instead of kmalloc/ksize/kzfree, since this
 * runs as a host userspace program, not a kernel module. */
#include <stdio.h>
#include <stdlib.h>
#include "gmp.h"

static void *host_alloc(size_t size) { return malloc(size); }
static void *host_realloc(void *p, size_t old_size, size_t new_size) { (void)old_size; return realloc(p, new_size); }
static void host_free(void *p, size_t size) { (void)size; free(p); }

void *(*__gmp_allocate_func)(size_t) = host_alloc;
void *(*__gmp_reallocate_func)(void *, size_t, size_t) = host_realloc;
void (*__gmp_free_func)(void *, size_t) = host_free;

void __gmp_assert_fail(const char *filename, int linenum, const char *expr)
{
	fprintf(stderr, "GMP assertion failed: %s:%d: %s\n", filename, linenum, expr);
	abort();
}

void __gmp_divide_by_zero(void)
{
	fprintf(stderr, "GMP: divide by zero\n");
	abort();
}
EOF

echo ">> [1/2] numeric safety-margin sweep"
gcc -O2 -fgnu89-inline -I"$WORK/orig" -o "$WORK/test_xsize_sweep" \
    "$HERE/test_xsize_sweep.c" "$WORK/orig/mpn/mp_bases.c"
"$WORK/test_xsize_sweep"
SWEEP_RC=$?
if [ "$SWEEP_RC" -ne 0 ]; then
    echo "FAIL: numeric safety-margin sweep found an under-estimate (see above)." >&2
    exit 1
fi
echo "PASS: sweep found zero under-estimates across every base and str_size 0..8192."
echo

echo ">> [2/2] end-to-end mpz_set_str() baseline-vs-patched comparison"
for variant in orig patched; do
    d="$WORK/$variant"
    gcc -O2 -fgnu89-inline -I"$d" -I"$d/mpz" -I"$d/mpn" -o "$WORK/test_driver_$variant" \
        "$HERE/test_xsize_patch_driver.c" "$WORK/host_glue.c" "$d"/mpz/*.c "$d"/mpn/*.c
    "$WORK/test_driver_$variant" > "$WORK/out_$variant.txt"
done

# Every line must match except the `alloc=` field (patched may be >=
# original there, and is expected to differ on several lines -- that's
# the whole point of the patch). Strip that one field before diffing.
sed -E 's/alloc=[0-9]+/alloc=X/' "$WORK/out_orig.txt"    > "$WORK/out_orig.norm.txt"
sed -E 's/alloc=[0-9]+/alloc=X/' "$WORK/out_patched.txt" > "$WORK/out_patched.norm.txt"

if diff -u "$WORK/out_orig.norm.txt" "$WORK/out_patched.norm.txt"; then
    echo "PASS: size=/limbs= identical on every test case (alloc= excluded, allowed to differ)."
else
    echo "FAIL: a computed bignum result changed -- investigate before trusting the patch." >&2
    exit 1
fi

# Separately confirm patched alloc= is NEVER smaller than orig alloc= on
# any line (the actual safety property, checked precisely rather than
# just "the field is allowed to differ").
python3 - "$WORK/out_orig.txt" "$WORK/out_patched.txt" <<'PYEOF'
import re, sys
a = open(sys.argv[1]).read().splitlines()
b = open(sys.argv[2]).read().splitlines()
bad = 0
for la, lb in zip(a, b):
    ma = re.search(r'alloc=(\d+)', la)
    mb = re.search(r'alloc=(\d+)', lb)
    if int(mb.group(1)) < int(ma.group(1)):
        print("UNDER-ALLOCATION:", la, "->", lb)
        bad += 1
print("alloc-never-decreases check: %d violation(s) out of %d lines" % (bad, len(a)))
sys.exit(1 if bad else 0)
PYEOF

echo
echo "ALL CHECKS PASSED."
