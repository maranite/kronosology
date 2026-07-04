// SPDX-License-Identifier: GPL-2.0
/*
 * oa_new_delete.h  -  the global operator new/delete substrate every C++
 * class in OA.ko transitively allocates through. Stage 2 shared utility
 * (PLAN.md's "TMP allocators" -- this is the closest real match to that
 * name: OA.ko is a freestanding kernel module built with `-fno-exceptions`
 * and no libstdc++, so it defines its own `operator new`/`delete`/`new[]`/
 * `delete[]` from scratch rather than linking a runtime's).
 *
 * Ground-truthed: all four operators are tiny (15 bytes each) trampolines
 * that just forward to two more locally-defined OA.ko functions:
 *   `operator new(unsigned int)`    .text+0x118d10 -> calls stg_kmalloc
 *   `operator new[](unsigned int)`  .text+0x118d20 -> calls stg_kmalloc
 *   `operator delete(void*)`        .text+0x118d30 -> calls stg_kfree
 *   `operator delete[](void*)`      .text+0x118d40 -> calls stg_kfree
 * (confirmed via `.rel.text` relocations on each trampoline's `call`
 * instruction, not assumed from the naming).
 *
 * `stg_kmalloc`/`stg_kfree` (.text+0x118d60/0x118d80, 20/15 bytes) are
 * themselves real, locally-defined OA.ko functions, but they're thin too --
 * ground-truthed via their own relocations to call straight into the real
 * Linux kernel's `__kmalloc(size, GFP_KERNEL)` (EDX=0xd0, which is exactly
 * `__GFP_WAIT|__GFP_IO|__GFP_FS` in this 2.6.32 kernel's gfp_t encoding --
 * i.e. plain `GFP_KERNEL`) and `kfree(ptr)` respectively. So the entire
 * chain from `new`/`delete` down to the kernel is exactly four hops of pure
 * forwarding, no bespoke pool/arena logic of its own (that's what
 * `CSTGBankMemory`/`CSTGHeapManager` are for, at a different layer).
 *
 * `__kmalloc`/`kfree` are genuine kernel APIs, not host-testable -- same
 * treatment as `register_cdrom()` in cdrom_check.cpp: declared `extern "C"`
 * for the target-ABI compile check, no KAT (there's nothing to compute; the
 * call contract itself is the whole implementation).
 */

#ifndef OA_NEW_DELETE_H
#define OA_NEW_DELETE_H

/*
 * `size_t` here, not `unsigned int`, even though the real disassembly's
 * argument is a plain 32-bit register: on the real 32-bit target `size_t`
 * *is* `unsigned int` (confirmed by `operator new`'s mangled name, `_Znwj`,
 * matching exactly), so this is faithful there and additionally lets this
 * header compile correctly as `operator new`'s required signature on a
 * 64-bit host, without needing a 32-bit host build just for this unit.
 * `__SIZE_TYPE__` (a GCC builtin, no header needed) is used instead of
 * `<cstddef>` since this header is also compiled `-ffreestanding` for the
 * target-ABI check, where 32-bit libstdc++ headers aren't installed.
 */
typedef __SIZE_TYPE__ oa_size_t;

extern "C" void *__kmalloc(oa_size_t size, unsigned int flags);
extern "C" void  kfree(void *ptr);

#define OA_GFP_KERNEL 0xd0u

extern "C" void *stg_kmalloc(oa_size_t size);
extern "C" void  stg_kfree(void *ptr);

#endif /* OA_NEW_DELETE_H */
