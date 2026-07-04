// SPDX-License-Identifier: GPL-2.0
/*
 * oa_rtfifo_init.h  -  stg_rtfifo_init(): init_module step 16 (hard-fail).
 *
 * Ground-truthed via readelf's symbol table against
 * ARCHIVE/Ignored/DecryptedImages/OA_real.ko (`stg_rtfifo_init`,
 * `.init.text+0x397`, 163 bytes; `our_fifo_setup`, `.init.text+0x34f`,
 * 72 bytes -- both `__init`-only, colocated with `init_module` itself
 * in `.init.text`, confirmed adjacent to each other and to
 * `init_module` per oa_init.h's own file header note), then a full
 * objdump disassembly + relocation trace.
 *
 * Real RTAI infrastructure, not a stub candidate (per this project's
 * own plan) -- `rtf_create`/`rtf_destroy` are genuine RTAI FIFO
 * primitives (confirmed real via relocation, matching the already-
 * loaded `rtai_fifos.ko` module this project's own kronos_vm boot
 * testing already confirmed loads successfully, sec 10.41).
 *
 * `our_fifo_setup(minor, size)`: `rtf_destroy(minor)` (return value
 * discarded -- tearing down any leftover FIFO on that minor first),
 * then `rtf_create(minor, size)`; on success (0), sets bit `minor` in
 * a confirmed real `.bss` bitmask (tracking which FIFOs were
 * successfully created, presumably read by `stg_rtfifo_cleanup`'s own
 * not-yet-reconstructed body to know which ones to tear down) and
 * returns 0; on failure, returns -1 without touching the bitmask.
 *
 * `stg_rtfifo_init()`: creates 6 RTAI FIFOs via `our_fifo_setup` --
 * minors 0,1,3,4,5,7 with sizes 0x400,0x400,0x400,0x8000,0x10000,0x400
 * respectively (every value a confirmed literal immediate, not
 * estimated) -- bailing to `stg_rtfifo_cleanup()` + return -1 on the
 * first failure. If all 6 succeed, registers a character device
 * (`__register_chrdev(major=0x98, baseminor=0, count=0x100,
 * name="stg_direct", fops=&<a real .data-embedded file_operations
 * blob, .data+0x3200>)` -- the name extracted directly from
 * `.rodata.str1.1`, not guessed) and returns 0 on success, or the same
 * cleanup+return -1 on failure.
 */

#ifndef OA_RTFIFO_INIT_H
#define OA_RTFIFO_INIT_H

extern "C" {

/*
 * FIXED (sec 10.90, found while reconstructing PushUnsolicitedMessage):
 * `rtf_create`/`rtf_destroy` (real RTAI FIFO primitives, exported by
 * `rtai_fifos.ko`) are genuinely `regparm(0)` (cdecl) -- confirmed via
 * `our_fifo_setup`'s own disassembly, which passes BOTH arguments via
 * the stack (`mov %eax,(%esp)` / `mov %esi,0x4(%esp)`), never via
 * eax/edx despite this whole translation unit's own `-mregparm=3`
 * Kbuild default. This is the SAME bug class as the `printk`/
 * `rt_printk` ABI mismatch already fixed in sec 10.87 (real RTAI/kernel
 * primitives explicitly marked `asmlinkage` upstream, unlike OA-internal
 * functions which correctly follow the Kbuild-wide regparm(3) default)
 * -- previously missed here since nothing had exercised these two
 * calls under a scenario that would surface an ABI mismatch (host KATs
 * don't validate calling convention, only link resolution; the real
 * kronos_vm boot test never got far enough to reach step 16). Confirmed
 * `__register_chrdev` does NOT need this fix -- its own real call site
 * passes its first 3 args via eax/edx/ecx exactly as regparm(3) would,
 * only the trailing 2 (beyond 3 register slots) on the stack, matching
 * a normal Kbuild-default-compiled kernel function, not an
 * `asmlinkage`-marked one.
 */
__attribute__((regparm(0))) int rtf_create(unsigned int fifo, int size);
__attribute__((regparm(0))) int rtf_destroy(unsigned int fifo);
/*
 * rtf_put_if(fifo, buf, size) -- confirmed real RTAI FIFO write
 * primitive (sec 10.90, relocation from the new `PushUnsolicitedMessage`
 * reconstruction), same confirmed-real `regparm(0)` ABI as rtf_create/
 * rtf_destroy above (its own real call site also passes all 3 args via
 * the stack, not eax/edx/ecx).
 */
__attribute__((regparm(0))) int rtf_put_if(unsigned int fifo, const void *buf, int size);
int __register_chrdev(unsigned int major, unsigned int baseminor,
		       unsigned int count, const char *name, const void *fops);
int __unregister_chrdev(unsigned int major, unsigned int baseminor,
			 unsigned int count, const char *name);
/* asmlinkage (regparm(0)) required -- see init_module.cpp's own
 * comment on this exact fix, sec 10.87. Parameter type CORRECTED
 * (2026-07-04): the real first argument is a genuine `.rodata`-
 * relocated string address, not the raw offset number this project
 * had been passing as a literal integer (a real wild-pointer bug, not
 * just a placeholder-text gap) -- see init_module.cpp's own updated
 * comment for the full explanation. */
__attribute__((regparm(0))) void rt_printk(const char *fmt, ...);

/*
 * Step 16's own cleanup counterpart -- ground-truthed via readelf+
 * objdump (`-j .text`), .text+0x116ac0, 381 bytes. Confirmed real:
 * destroys whichever of the 6 real FIFO minors (0,1,3,4,5,7) are still
 * marked live in `s_fifoCreatedMask` (the same bitmask
 * our_fifo_setup() sets, shared with stg_rtfifo_init() -- defined in
 * rtfifo_init.cpp, not exposed here), unregisters the "stg_direct" char
 * device (major=0x98, count=0x100, name literal confirmed from
 * `.rodata.str1.1`), then emits 3 `rt_printk` diagnostic dumps of
 * confirmed-real-but-unnamed `.bss` statistic counters (FIFO drop/error
 * counts, presumably -- not independently decoded in this pass, purely
 * logging with no control-flow effect).
 */
void stg_rtfifo_cleanup(void);

int stg_rtfifo_init(void);

/* Test-only accessor, see rtfifo_init.cpp's own note. */
void stg_rtfifo_test_reset_mask(void);

} /* extern "C" */

#endif /* OA_RTFIFO_INIT_H */
