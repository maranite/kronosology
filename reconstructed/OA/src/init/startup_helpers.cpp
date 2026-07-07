// SPDX-License-Identifier: GPL-2.0
/*
 * startup_helpers.cpp  -  small init_module()-call-chain helper
 * functions, promoted from their deliberately-inert placeholders in
 * src/stub/bar2_stubs_c.cpp to faithful reconstructions from the real
 * OA_322.ko disassembly.
 *
 * Batch 34 (sec 10.182) adds two more of the same shape, closing out
 * init_module()'s error-reporting pair:
 *
 *   stg_is_linux_context  @0x118db0, 29 bytes:
 *       call rt_whoami           ; R_386_PC32 rt_whoami -> RT_TASK* in eax
 *       mov  0x1c(%eax),%eax     ; task->[0x1c]
 *       cmp  $0x7fffffff,%eax    ; == RT_SCHED_LINUX_PRIORITY ?
 *       sete %al ; movzbl %al,%eax
 *     Asks RTAI whether the *current* task is the Linux servant rather
 *     than a hard-real-time task: rt_whoami() returns the running
 *     RT_TASK, and its priority word at +0x1c equals the sentinel
 *     0x7fffffff (RT_SCHED_LINUX_PRIORITY) exactly when we are in normal
 *     Linux context. `rt_whoami` is a genuine RTAI export -- undefined
 *     (`U`) in the real OA.ko too, same family as the already-accepted
 *     rt_printk/rtf_* externs -- so promoting this raises nm -u by one.
 *     The prologue's `and $-16,%esp` stack-align is a compiler artifact
 *     of the outermost call, not semantically load-bearing.
 *
 *   stg_log_startup_error  @0x118e10, 99 bytes, regparm3 (msg in EAX):
 *       call stg_is_linux_context ; guard: only log from Linux context
 *       test %eax; je ret
 *       mov $3,%edx ; mov $"/tmp/startupErrorLog",%eax ; call CSTGFile_Open
 *       test %eax; je ret          ; open-fail is a silent no-op
 *       mov %eax,%esi              ; handle
 *       mov %ebx(msg),%eax; call strlen
 *       mov %eax,%ecx; mov %ebx,%edx; mov %esi,%eax; call CSTGFile_Write
 *       mov %esi,%eax; call CSTGFile_Close
 *     Appends the NUL-terminated message string to /tmp/startupErrorLog
 *     (open mode 3), but only when running in Linux context; both a
 *     non-Linux context and an open failure are silent no-ops (no
 *     partial write, no close of a null handle). Its argument is a
 *     `const char *` -- every init_module() call site loads
 *     `.rodata.str1.1 + <offset>` into EAX (the offsets 0xa1/0xd7/...
 *     the reconstruction previously modeled as bare ints; batch 34
 *     resolves them to the real strings "cpu cap"/"memory error"/...).
 *     Dependencies now all real: CSTGFile_Open/Write/Close
 *     (src/init/file_io.cpp, sec 10.180/10.181), strlen (kernel export),
 *     stg_is_linux_context (this file).
 *
 * All are on init_module()'s own confirmed call chain (sec 10.17)
 * and are individually tiny leaf/near-leaf functions -- exactly the
 * "smallest-first, self-contained, host-verifiable" batch shape this
 * project's stub sweep favours. Ground-truth sizes: init_cpp_support 1
 * byte, GetInstalledRAM 6, IncProgressBar 15, SetInstalledOptions 17.
 *
 * Ground truth (nm/objdump of /home/share/Decomp/OA.ko_Decomp/OA.ko):
 *
 *   init_cpp_support  @0x592070, 1 byte:
 *       c3                ret
 *     A literal bare `ret` -- a confirmed no-op in this build (the C++
 *     static-constructor iteration this name suggests is either empty
 *     or run via a different mechanism here; the object code does
 *     nothing at all). Faithful reconstruction is therefore an empty
 *     body, now with disassembly evidence rather than a placeholder's
 *     "not reconstructed" guess. (Its fini-side sibling stg_cpp_exit is
 *     likewise a 1-byte `ret`; cleanup_cpp_support -- which iterates the
 *     real `.dtors` array -- is deliberately NOT promoted in this batch,
 *     see the deferral note in bar2_stubs_c.cpp.)
 *
 *   GetInstalledRAM  @0x9b50, 6 bytes:
 *       a1 20 00 00 00    mov  0x20,%eax     ; R_386_32  .bss
 *       c3                ret
 *     Reads a single 32-bit word from an anonymous `.bss+0x20` global
 *     (the raw hardware-probed installed-RAM byte count, written by the
 *     not-yet-reconstructed board-detection path) and returns it. This
 *     is a DISTINCT global from STGAPIFrontPanelStatus::sInstance+0xd30
 *     (STGAPI_OFF_INSTALLED_RAM), which is merely where
 *     setup_global_resources() later STORES this function's result.
 *     Modeled as a named global `gInstalledRAM`, matching this project's
 *     established convention for naming anonymous BSS globals
 *     (gModuleParam10/14/18 in bar2_stubs_c.cpp).
 *
 *   IncProgressBar  @0x119950, 15 bytes:
 *       55 89 e5 83 e4 f0                    ; push ebp; mov esp,ebp; and $-16,esp
 *       e8 .. call COmapNKS4_IncProgressBar  ; R_386_PC32
 *       89 ec 5d c3                          ; mov ebp,esp; pop ebp; ret
 *     A thin forwarder to COmapNKS4_IncProgressBar(), an external symbol
 *     exported by OmapNKS4Module.ko -- undefined (`U`) in the real
 *     OA.ko too, same family as this project's already-accepted
 *     COmapNKS4Driver_* / OmapNKS4OutputFifo_* externals. Promoting this
 *     therefore legitimately raises the reconstruction's unresolved-
 *     symbol count 32 -> 33 (the real binary's own undefined set is 168;
 *     32 was never a hard ceiling, only the subset reached so far).
 *     The stack-align prologue is a compiler artifact of the outermost
 *     call; not semantically load-bearing, reproduced implicitly by the
 *     compiler.
 *
 *   SetInstalledOptions  @0x118af0, 17 bytes:
 *       8b 15 .. mov 0x0,%edx      ; edx = STGAPIFrontPanelStatus::sInstance (R_386_32)
 *       85 d2    test %edx,%edx
 *       74 06    je   ret
 *       08 82 90 10 00 00  or %al,0x1090(%edx)   ; (sInstance)[0x1090] |= al
 *       c3       ret
 *     ORs the LOW BYTE of its argument (AL, the first regparm(3) int arg
 *     in EAX) into the byte at sInstance+0x1090, but only when sInstance
 *     is non-NULL. init_module() calls it with 0x20 and 0x10 (both fit
 *     in a byte). The prior placeholder's `()` signature silently
 *     ignored the argument entirely; the real function uses it.
 */

#include "oa_setup_global_resources.h"   /* STGAPIFrontPanelStatus + STGAPI_OFF_* */
#include "oa_internal.h"                  /* strlen */

/* Anonymous .bss+0x20 hardware-probed installed-RAM word (see header
 * note). Zero-initialized BSS, exactly like the real global before the
 * board-detection path writes it. */
extern "C" unsigned int gInstalledRAM;
unsigned int gInstalledRAM;

/* Named byte-offset for SetInstalledOptions' target field -- sits one
 * byte below the already-named STGAPI_OFF_CAL_MARKER (0x1091). */
#ifndef STGAPI_OFF_INSTALLED_OPTIONS
#define STGAPI_OFF_INSTALLED_OPTIONS    0x1090
#endif

/* External progress-bar tick, exported by OmapNKS4Module.ko (undefined
 * in the real OA.ko as well). */
extern "C" void COmapNKS4_IncProgressBar(void);

extern "C" void init_cpp_support(void)
{
	/* Confirmed no-op: real object code is a single `ret`. */
}

extern "C" unsigned int GetInstalledRAM(void)
{
	return gInstalledRAM;
}

extern "C" void IncProgressBar(void)
{
	COmapNKS4_IncProgressBar();
}

extern "C" void SetInstalledOptions(int code)
{
	unsigned char *status = STGAPIFrontPanelStatus::sInstance;
	if (status != 0)
		status[STGAPI_OFF_INSTALLED_OPTIONS] |= (unsigned char)code;
}

/* ---- batch 34: init_module()'s error-reporting pair ---- */

/* RTAI: current running RT_TASK (or the Linux servant's shadow task).
 * Undefined (`U`) in the real OA.ko as well; resolves at insmod against
 * rtai_hal/rtai_sched. */
extern "C" void *rt_whoami(void);

/* CSTGFile_* VFS wrappers -- real bodies in src/init/file_io.cpp
 * (declared locally here with matching signatures, same as
 * init_module.cpp does). */
extern "C" void *CSTGFile_Open(const char *path, int mode);
extern "C" int   CSTGFile_Write(void *handle, const void *buf, unsigned int count);
extern "C" int   CSTGFile_Close(void *handle);

/* RTAI priority sentinel: a task scheduled at this "priority" is the
 * Linux servant, i.e. we are NOT inside a hard-real-time task. */
#ifndef RT_SCHED_LINUX_PRIORITY
#define RT_SCHED_LINUX_PRIORITY 0x7fffffff
#endif

extern "C" int stg_is_linux_context(void)
{
	/* rt_whoami()->priority (word at +0x1c) == RT_SCHED_LINUX_PRIORITY */
	return *(unsigned int *)((unsigned char *)rt_whoami() + 0x1c)
	       == (unsigned int)RT_SCHED_LINUX_PRIORITY;
}

extern "C" void stg_log_startup_error(const char *msg)
{
	if (!stg_is_linux_context())
		return;
	void *handle = CSTGFile_Open("/tmp/startupErrorLog", 3);
	if (handle == 0)
		return;
	CSTGFile_Write(handle, msg, (unsigned int)strlen(msg));
	CSTGFile_Close(handle);
}
