/*
 * glibc_compat.c — force __libc_start_main to resolve against GLIBC_2.0
 *
 * Modern glibc (≥2.34) versions __libc_start_main at GLIBC_2.34.  The Kronos
 * runs glibc ~2.9-2.11, which only has GLIBC_2.0 … GLIBC_2.11.  Without this
 * shim the dynamic linker refuses to load the binary.
 *
 * Build with: -Wl,--wrap=__libc_start_main
 * The linker then routes CRT's call through __wrap___libc_start_main (here),
 * which in turn calls __libc_start_main@GLIBC_2.0 explicitly.
 */

/* Declare the GLIBC_2.0 version of __libc_start_main under a private name. */
extern int __libc_start_main_glibc20(
    int (*main)(int, char **, char **),
    int argc,
    char **argv,
    void (*init)(void),
    void (*fini)(void),
    void (*rtld_fini)(void),
    void *stack_end);

__asm__(".symver __libc_start_main_glibc20,__libc_start_main@GLIBC_2.0");

/* The linker redirects CRT's __libc_start_main call here. */
int __wrap___libc_start_main(
    int (*main)(int, char **, char **),
    int argc,
    char **argv,
    void (*init)(void),
    void (*fini)(void),
    void (*rtld_fini)(void),
    void *stack_end)
{
    return __libc_start_main_glibc20(main, argc, argv, init, fini, rtld_fini,
                                     stack_end);
}
