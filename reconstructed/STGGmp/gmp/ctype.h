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
