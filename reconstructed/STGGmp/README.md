# STGGmp.ko — reconstructed source

Reverse-engineered, compilable source for the Korg Kronos **`STGGmp.ko`** module
(firmware 3.2.2). Target: **Linux 2.6.32.11 + RTAI**, x86-32, `gcc -mregparm=3`.

## What this module is

`STGGmp.ko` is the **GNU MP (GMP) arbitrary-precision arithmetic library, version
4.2.x, built as a Linux kernel module** — the firmware's "Korg GMP Shell" (that exact
string is in the binary's `.modinfo`, original source file `STGGmpModule.c`). It is
used by the copy-protection / authorization code (e.g. `loadmod.ko`, `OA.ko`,
`GetPubIdMod.ko`) which needs big-integer modular exponentiation (`__gmpz_powm`),
modular inverse (`__gmpz_invert`), GCD (`__gmpz_gcdext`), etc. for its RSA-style maths.

It exports the full GMP C API it uses (`__gmpz_*`, `__gmpn_*`, `__gmp_*`) plus GMP's
data tables (`__gmpn_bases`, `__gmpn_clz_tab`, `__gmp_digit_value_tab`,
`__gmp_modlimb_invert_table`, `__gmp_allocate_func`, …).

### Why the bulk is *not* reverse-engineered here

The arithmetic functions (`mul_fft`, `kara_mul_n`, `toom3_mul_n`, `sb_divrem_mn`,
`gcdext`, …) are **stock GMP** — an existing LGPL library. Transcribing GMP's FFT and
Toom-Cook multiplication out of assembly would be pointless and *lower* fidelity than
using the real upstream sources. So this folder contains:

- **`STGGmpModule.c`** — the genuinely Korg-specific glue, fully reconstructed (see below).
- Build scaffolding to combine that glue with **real GMP 4.2.x sources** into `STGGmp.ko`.

## Which GMP version

The exported function set pins it to the **GMP 4.2 series** (4.2.1 recommended):

- `__gmpn_kara_mul_n` / `__gmpn_toom3_mul_n` are the *only* sub-quadratic mults
  (4.3 added Toom-4; 5.0 renamed everything to `toomMN` / `mpn_mu_*`).
- `__gmpn_sb_divrem_mn`, `__gmpn_dc_divrem_n` are the 4.x division names.
- `__gmp_tmp_reentrant_alloc` / `…_free` are the 4.x `TMP_ALLOC` reentrant scheme.

## What `STGGmpModule.c` provides (the reconstructed part)

| Symbol | Role |
|---|---|
| `do_gmp_alloc` | `kmalloc(size, GFP_KERNEL)` |
| `do_gmp_realloc` | grow/shrink using `ksize()`/`kzfree()` (in-place if it fits) |
| `do_gmp_free` | `kzfree()` |
| `__gmp_allocate_func` / `__gmp_reallocate_func` / `__gmp_free_func` | GMP's global hooks, pre-wired to the above |
| `__gmp_assert_fail` | GMP `assert.c`, `fprintf(stderr)`→`printk`, `abort()`→`BUG()` |
| `__gmp_divide_by_zero` | same treatment |
| `__ctype_b_loc` | libc-compat ctype table (GMP `*_set_str` use `isspace`/`isdigit`) |
| `STGGmp_init` / `STGGmp_exit` | module init/exit (init only fills the ctype table) |

These replace GMP's `memory.c`, `assert.c`, `errno.c` and the libc `<ctype.h>`
dependency so GMP can run with no userspace/libc.

## Building the full module

GMP can't be linked from a userspace `libgmp.a`; its generic C sources must be compiled
with the kernel toolchain (freestanding, no libc). That's exactly what Korg did.

1. **Fetch + adapt GMP** into `./gmp/`:

   ```sh
   ./fetch-gmp.sh          # downloads gmp-4.2.1, configures generic/ABI=32, stages sources
   ```

   This produces `gmp/gmp.h`, `gmp/gmp-impl.h`, `gmp/config.h`, `gmp/longlong.h` and the
   `gmp/mpz/*.c`, `gmp/mpn/*.c` listed in the `Makefile`. You may need to comment out the
   `<stdio.h>`/`<stdlib.h>`/`<string.h>` includes in `gmp-impl.h` (the kernel build is
   freestanding; the glue supplies the allocators, `__gmp_assert_fail`, and ctype).

2. **Build** against the Kronos kernel tree:

   ```sh
   make KDIR=/mnt/tank/source/Kronos/linux-kronos
   ```

   Produces `STGGmp.ko`.

3. **Sanity-check just the glue compiles** (no GMP needed):

   ```sh
   make glue KDIR=/mnt/tank/source/Kronos/linux-kronos
   ```

### Symbol → GMP source map

- `mpz/`: `add add_ui sub mul mul_2exp set set_str init iset_str iset_ui clear realloc
  tdiv_q tdiv_r powm invert gcdext`
- `mpn/generic/`: `add_n sub_n mul_1 addmul_1 submul_1 lshift rshift mul mul_n
  mul_basecase sqr_basecase mul_fft set_str tdiv_qr dc_divrem_n sb_divrem_mn
  divrem_1 divrem_2 diveby3 gcdext tal-reent`
- generated tables: `mp_bases mp_clz_tab mp_minv_tab dv_tab`
  (`__gmpn_bases`, `__gmpn_clz_tab`, `__gmp_modlimb_invert_table`,
  `__gmp_digit_value_tab`)

(The `kara_*`, `toom3_*`, `sqr_n`, `mpn_fft_*`, `mpn_dc_div_*`, `div2`, `reduce`,
`toom3_interpolate` symbols are statics that live inside `mul_n.c`, `mul_fft.c`,
`dc_divrem_n.c`, `gcdext.c`, `powm.c` respectively — no separate files needed.)

## Fidelity notes

- `STGGmpModule.c` is functionally faithful to the binary (allocators via
  `kmalloc`/`ksize`/`kzfree` with `GFP_KERNEL` == `0xd0`; asserts via `printk`+`BUG`;
  the ctype table reproduces glibc's `_ISxxx` bit layout, biased to index 0 like
  `__ctype_b_loc`).
- The GMP arithmetic is upstream 4.2.x — match the exact point release to the firmware
  if byte-identical behaviour matters, but any 4.2.x is functionally equivalent.
- `MODULE_LICENSE("GPL")` matches what the loader expects; GMP itself is LGPL-2.1+.
