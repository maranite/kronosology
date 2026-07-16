/* Freestanding-kernel-build shim for GMP source files (mpz/set_str.c) that
   unconditionally #include <string.h> for a real strlen() -- forward to the
   kernel's own real implementation rather than reimplementing libc. */
#ifndef _STGGMP_STRING_SHIM_H
#define _STGGMP_STRING_SHIM_H
#include <linux/string.h>
#endif
