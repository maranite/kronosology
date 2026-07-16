/* Freestanding-kernel-build shim for GMP source files that unconditionally
   #include <stdio.h> "for NULL" (or, in mul_fft.c's case, for printf calls
   that are compiled out by that file's own no-op TRACE() macro -- verified,
   not assumed). See fetch-gmp.sh's own comment for the full derivation. */
#ifndef _STGGMP_STDIO_SHIM_H
#define _STGGMP_STDIO_SHIM_H
#include <linux/stddef.h>
#endif
