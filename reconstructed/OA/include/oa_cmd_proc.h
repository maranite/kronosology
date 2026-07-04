// SPDX-License-Identifier: GPL-2.0
/*
 * oa_cmd_proc.h  -  /proc/.oacmd procfs plumbing: the file_operations
 * handlers and registration that sit on top of ProcessOACmd
 * (process_oacmd.h).
 *
 * Ground-truthed by direct disassembly of the real OA.ko 3.2.1 binary and
 * its .rel.text relocations:
 *   oa_cmd_open    .text+0x9e60  (28 bytes)
 *   oa_cmd_close   .text+0x9e80  (45 bytes)
 *   oa_cmd_read    .text+0x9eb0  (105 bytes)
 *   oa_cmd_write   .text+0x9f20  (188 bytes)
 *   PcmModuleMutexLock/Unlock   .text+0x9fe0/0xa000
 *   ParseOACmd     .text+0xa020  (40 bytes)
 *   InitPcmModProcInterface     .text+0xa060  (61 bytes)
 *   CleanupPcmModProcInterface  .text+0xa0a0  (22 bytes)
 *   oa_cmd_fops (the struct file_operations instance)  .data+0x4e0 (104 bytes)
 *
 * oa_cmd_fops's field layout is confirmed, via relocation, to be the real,
 * unmodified Linux 2.6.32 `struct file_operations` (fs.h) -- offsets +0x08
 * (read), +0x0c (write), +0x30 (open), +0x38 (release) hold the four real
 * function addresses below; every other field is zero/unused.
 *
 * State machine (`sOACmdStatus`, confirmed from all four handlers'
 * disassembly, matches docs/interfaces/proc_oacmd.md exactly):
 *   0 IDLE        -- no client has the file open
 *   1 READY       -- open, no command in flight, ready to accept a write
 *   2 PROCESSING  -- a write is running ProcessOACmd (very briefly; the real
 *                    call is synchronous, this state exists mostly so a
 *                    concurrent read/write during the call is rejected)
 *   3 RESULT      -- ProcessOACmd returned 0 (recognized); a 4-byte result
 *                    is waiting to be read
 * Errors returned to userspace: -EAGAIN (11) for "try again, wrong state,
 * transient", -EINVAL (22) for "wrong state, not transient" / bad read size.
 *
 * `ParseOACmd` is a second, simpler entry point confirmed real via
 * relocation (also calls ProcessOACmd, also updates sOACmdStatus) but
 * distinct from the oa_cmd_write path -- its external caller was not
 * identified in this pass (candidates: an in-kernel direct-call path from
 * elsewhere in OA.ko, not necessarily through procfs at all).
 */

#ifndef OA_CMD_PROC_H
#define OA_CMD_PROC_H

#define OACMD_STATUS_IDLE       0
#define OACMD_STATUS_READY      1
#define OACMD_STATUS_PROCESSING 2
#define OACMD_STATUS_RESULT     3

/*
 * FIX (2026-07-02, found while reconstructing init_module step 7): this
 * whole header was missing an `extern "C"` wrapper, so every function
 * below -- most importantly `InitPcmModProcInterface`/
 * `CleanupPcmModProcInterface` -- was being compiled with a real,
 * MANGLED C++ symbol name (confirmed: `_Z23InitPcmModProcInterfacev`,
 * not the real binary's own plain `InitPcmModProcInterface`). Since
 * `oa_init.h` declares the SAME two functions correctly as `extern "C"`
 * (what `init_module.cpp` actually calls), the two declarations never
 * conflicted at compile time (different translation units, one per
 * header) but produced a genuine LINK-time mismatch: the mangled symbol
 * was defined but never called, and the plain symbol was called but
 * never defined -- both remained unresolved in a real Kbuild build
 * despite `oa_cmd_proc.cpp` already being fully reconstructed and
 * already part of `OA-objs`. A real, pre-existing bug, not something
 * this pass introduced -- caught only because a fresh `readelf` symbol
 * check on the compiled object showed the mangled name directly.
 */
extern "C" {

/* struct file_operations handlers, real Linux 2.6.32 signatures (opaque
 * kernel struct pointers as void* -- this freestanding reconstruction does
 * not pull in real kernel headers). */
int  oa_cmd_open(void *inode, void *file);
int  oa_cmd_close(void *inode, void *file);
long oa_cmd_read(void *file, char *userBuf, unsigned long count, long long *offset);
long oa_cmd_write(void *file, const char *userBuf, unsigned long count, long long *offset);

/* Second, simpler ProcessOACmd entry point -- see header comment above. */
int ParseOACmd(const char *cmd);

/* Registers/unregisters /proc/.oacmd (mode 0600, uid=gid=500 "pocky", per
 * prior CLAUDE.md finding, confirmed here via the literal 0x180/0x1f4/0x1f4
 * immediates in InitPcmModProcInterface's disassembly), wiring it to the
 * oa_cmd_* handlers above via oa_cmd_fops. */
int  InitPcmModProcInterface(void);
void CleanupPcmModProcInterface(void);

} /* extern "C" */

#endif /* OA_CMD_PROC_H */
