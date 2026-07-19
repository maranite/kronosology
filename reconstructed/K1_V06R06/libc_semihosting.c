/* SPDX-License-Identifier: GPL-2.0 */
/*
 * libc_semihosting.c - the panel firmware's newlib-style C-library syscall layer
 * built on ARM Angel/RDI semihosting: fd-table-backed read/write/open/close/lseek,
 * the errno-fetch tail every failing syscall stub shares, sbrk(), isatty(), and a
 * small cluster of syscall-result "record" helpers.
 *
 * ASSIGNMENT AND SCOPE: assigned address range 0xc001c2ac-0xc001c96c (17 real
 * Ghidra function objects). A full-image string search (this project's established
 * `__FILE__`-anchor method, see README.md's "Ghidra setup" section) found NO
 * `__FILE__` anchor anywhere in this range - this is runtime-library glue, not a
 * subsystem `.cpp` compilation unit, matching heap_alloc.c's own "no anchor, code-
 * shape evidence only" situation.
 *
 * This range is a DIRECT, PLANNED continuation of task_sched.c's own work: that
 * file's header comment explicitly carves out "0xc001c070-0xc001c98c: a complete
 * newlib-style C-library syscall layer built on ARM Angel/RDI semihosting" as
 * block (B) of its own sweep, explicitly declines to transcribe it ("Genuinely a
 * C library / debug-monitor I/O layer, not scheduler code. NOT reconstructed here
 * - flagged for a future `libc_semihosting.c` (or similar) pass"), and names this
 * exact future file. This file IS that follow-up pass, for the portion of block
 * (B) that falls inside this pass's own assigned range (0xc001c2ac-0xc001c96c);
 * see "STILL OPEN" below for the small remainder of block (B) that falls outside
 * it (0xc001c070-0xc001c29c and 0xc001c96c-0xc001c98c) and is therefore NOT
 * defined here, only cited as `extern`.
 *
 * Ground truth: this static dump's own decompiled_c for every listed address, ALSO
 * cross-checked against live `get_disassembly` (kronos_v06r06_panel.elf via the
 * read-only Ghidra MCP bridge - explicitly authorized this pass since only 2
 * agents are running) for every function in this file, because several of these
 * decompiles turned out to hide real parameters or real return values behind ARM
 * register-reuse the static decompile didn't model - see each function's own
 * header comment for the specific correction. `read_memory` was used twice to
 * resolve two literal-pool constants the static dump's own `dat` table didn't
 * have entries for (DAT_c001c54c, DAT_c001c358 - see semihost_stdio_open_streams
 * and newlib_lseek below).
 *
 * THE SEMIHOSTING TRAP ITSELF: every function below that touches "hardware" does
 * it via a single ARM `swi 0x123456` instruction (the well-known ARM Angel/RDI
 * semihosting trap number). Ghidra's own decompile shows this as a bare
 * `software_interrupt(0x123456)` call with NO visible arguments and NO visible
 * return value, but raw disassembly confirms every call site loads a real op
 * number into r0 and a real argument-block pointer (or 0) into r1 immediately
 * before the `swi`, and reads a real result back out of r0 immediately after -
 * this file restores all three explicitly (matching this project's now-standard
 * practice for ARM's implicit-register-argument idiom, e.g. task_sched.c's own
 * sched_delay_heap_extract_min / mcasp.c notes). The op-number table below is
 * CONFIRMED by this file's own functions (each cites which op belongs to which
 * ARM Angel/RDI standard-spec name), matching the standard spec exactly except
 * one hardcoded, unidentified op (see semihost_op18_fixed_call).
 *
 * A RECURRING GHIDRA ARTIFACT found repeatedly in this range, worth flagging once
 * here rather than at every occurrence: several functions load a literal op-number
 * constant into a register, use it as the swi's r0 argument, and then IMMEDIATELY
 * overwrite that same register with the swi's own r0 result - but Ghidra's
 * decompiler, having modeled `software_interrupt()` as a value-less call, loses
 * the reassignment and shows the STALE OP-NUMBER LITERAL being stored instead of
 * the real result (e.g. `*puVar1 = 0x13;` where 0x13 is really SYS_ERRNO's own op
 * number, not the value stored). Confirmed independently at three separate call
 * sites (semihost_fetch_errno_passthrough, newlib_reset_record_pair_time,
 * newlib_reset_record_clock) by re-reading raw disassembly - real swi RESULT is
 * what each site actually stores. Not a new phenomenon for this project (matches
 * kobj_eventflag_set's own documented r0-forwarding gap in task_sched.c) but this
 * is its first appearance as a "stale literal" rather than a "missing forward".
 *
 * FD-TABLE MODEL (resolved base 0xc01ce830, 20 entries x 8 bytes - CONFIRMED
 * identical across SEVEN independent literal-pool copies: DAT_c001c368/c46c/c628/
 * c6dc/c74c/c19c, all resolved via the static dump's own `dat` table, plus
 * DAT_c001c54c which the static dump did NOT have an entry for and was instead
 * resolved with a direct `read_memory` at 0xc001c54c -> bytes `30 e8 1c c0` little-
 * endian = 0xc01ce830, matching the other six exactly): each entry is
 * {int32_t handle; int32_t pos;}. handle == -1 marks a free slot. Three "logical"
 * fds (0/1/2 = stdin/stdout/stderr) are handled OUTSIDE this table entirely, via
 * newlib_fd_translate (FUN_c001c1a0, just below this file's own range - see
 * "STILL OPEN"): any fd < 3 there is looked up in one of three dedicated globals
 * (semihost_stdin_handle/semihost_stdout_handle/semihost_stderr_handle, all
 * populated by semihost_stdio_open_streams below), any fd >= 3 has 0x20 SUBTRACTED
 * to recover the real host handle - the exact inverse of the `+ 0x20` bias
 * newlib_open_raw's own success path applies below. This "handle + 0x20 = user fd"
 * convention is a genuinely new, concrete finding this pass - nothing in the
 * project previously documented how this firmware's fd numbers relate to the
 * host's own semihosting handles.
 */

#include <stdint.h>

/* ============================================================================
 *  The semihosting trap itself, and the op-number table this file confirms
 * ============================================================================
 *
 *  extern, not defined here: this represents the literal `swi 0x123456`
 *  instruction inlined at every call site below, not a real callable symbol in
 *  the binary. `op` -> r0, `arg` -> r1 (a small stack-allocated argument block,
 *  or 0 when the op takes none), result <- r0. Every op number below is
 *  CONFIRMED by a function in this file (see that function's own comment) to
 *  match the standard ARM Angel/RDI semihosting spec, except 0x18 (see
 *  semihost_op18_fixed_call, NOT decoded - not a spec op this file can confirm).
 */
extern int32_t software_interrupt(uint32_t op, void *arg);

#define SYS_OPEN	0x01	/* confirmed: semihost_stdio_open_streams, newlib_open_raw */
#define SYS_CLOSE	0x02	/* confirmed: newlib_close */
#define SYS_WRITE	0x05	/* confirmed: semihost_sys_write_raw */
#define SYS_READ	0x06	/* confirmed: semihost_sys_read_raw */
#define SYS_SEEK	0x0a	/* confirmed: newlib_lseek (SEEK_SET/absolute case) */
#define SYS_FLEN	0x0c	/* confirmed: newlib_lseek (SEEK_END case) */
#define SYS_CLOCK	0x10	/* confirmed: newlib_reset_record_clock */
#define SYS_TIME	0x11	/* confirmed: newlib_reset_record_pair_time */
#define SYS_ERRNO	0x13	/* confirmed: semihost_fetch_errno_passthrough */

/* ============================================================================
 *  Externs this file DEPENDS ON but does NOT define - all outside this file's
 *  own 0xc001c2ac-0xc001c96c assignment. See "STILL OPEN" at the end of this
 *  file for which of these remain genuinely unclaimed by any file.
 * ============================================================================ */

/* FUN_c001c1a0, @0xc001c1a0 (just below this file's range) - translates a
 * "logical" fd into a real host semihosting handle: fd==0/1/2 read one of the
 * three globals below; any other fd has 0x20 subtracted (the exact inverse of
 * newlib_open_raw's own `+ 0x20` bias below). Also does an unrelated one-time
 * lazy-init of some OTHER subsystem on its very first call (guarded by a
 * different resolved flag, 0xc01ce820) - irrelevant to this file's own use of
 * it, not re-described here. */
extern int32_t newlib_fd_translate(int32_t fd);

/* FUN_c001c174, @0xc001c174 (just below this file's range) - linear-searches
 * the 20-entry fd table for a matching `handle` (pass -1 to find a FREE slot,
 * since free slots store -1), returns the index 0..19, or 20 if not found.
 * Ghidra's own decompile types this `void` (no explicit return anywhere in its
 * body) even though every call site here treats r0 as a real index - the same
 * "implicit ARM register-reuse return" idiom task_sched.c already documents for
 * sched_delay_heap_extract_min/mcasp.c; restored explicitly as a real int32_t
 * return here. Several call sites below (newlib_read/_write/_lseek) pass this
 * function's ARGUMENT implicitly too - via the leftover r0 from an immediately
 * preceding newlib_fd_translate() call, with no visible `cpy r0,rX` between the
 * two `bl`s in the raw disassembly - transcribed as an explicit two-step call
 * here since C has no way to express "whatever's still in r0". */
extern int32_t newlib_fd_table_find(int32_t handle);

/* FUN_c001c114, @0xc001c114 (just below this file's range) - a strlen-shaped
 * word-optimized byte counter (same `+0xfefefeff & ~x & 0x80808080` NUL-detect
 * idiom as FUN_c001c070's own strcmp, immediately below it). NOT reconstructed
 * by any file yet as of this pass - see "STILL OPEN" below. */
extern int32_t libc_strlen(const char *s);

/* FUN_c001ca24, @0xc001ca24 (just above this file's range) - `return
 * *DAT_c001ca30;`, i.e. simply returns this firmware's own errno-variable
 * pointer. Already cited (but not defined) by task_sched.c's own block-(B)
 * summary; still not defined by any file as of this pass - see "STILL OPEN". */
extern int32_t *newlib_errno_ptr(void);

/* FUN_c001ada0, heap_alloc.c's own neighbor, already fully defined and named in
 * newlib_dtoa_bigint.c. */
extern void *libc_memset(void *dst, int c, uint32_t n);	/* newlib_dtoa_bigint.c, FUN_c001ada0 */

/* ============================================================================
 *  Standard-stream handles and the fd table itself
 * ============================================================================ */
extern int32_t semihost_stdin_handle;	/* DAT_c001c35c/DAT_c001c2a0, resolved 0xc01ce824 */
extern int32_t semihost_stdout_handle;	/* DAT_c001c360/DAT_c001c2a8, resolved 0xc01ce828 */
extern int32_t semihost_stderr_handle;	/* DAT_c001c364/DAT_c001c2a4, resolved 0xc01ce82c -
					 * populated with the SAME value as semihost_stdout_handle
					 * (see semihost_stdio_open_streams below - only 2 real
					 * SYS_OPEN calls happen, not 3; stdout and stderr share
					 * one host handle) */

struct newlib_fd_entry {
	int32_t handle;		/* +0x00: host semihosting handle, or -1 if this slot is free */
	int32_t pos;		/* +0x04: cached file position, kept in sync by read/write/lseek */
};
/* resolved 0xc01ce830 - CONFIRMED identical across DAT_c001c368/c46c/c628/c6dc/
 * c74c/c19c (all via the static dump's own `dat` table) and DAT_c001c54c (via a
 * direct `read_memory`, see this file's own header comment) - one real 20-entry
 * array, SEVEN independent per-function literal-pool copies of its address, this
 * project's now-familiar pattern. */
extern struct newlib_fd_entry newlib_fd_table[20];

/* ============================================================================
 *  semihost_stdio_open_streams (FUN_c001c2ac, @0xc001c2ac) - crt0-time console
 *  bring-up: opens the ARM semihosting console pseudo-file ":tt" (CONFIRMED by a
 *  direct `read_memory` at 0xc0023c50 -> bytes `3a 74 74 00` = ":tt\0", the
 *  address DAT_c001c358 resolves to) TWICE - once mode 0 ("r", for stdin), once
 *  mode 4 ("w", for stdout) - and stores the SECOND open's handle into BOTH
 *  semihost_stdout_handle AND semihost_stderr_handle. Only two real SYS_OPEN
 *  calls happen; stdout and stderr are the same host file underneath. Then
 *  initializes all 20 fd-table slots to "free" (-1) and installs the two console
 *  handles into slots 0 and 1 with position 0. No confirmed callers in this
 *  static dump (zero xrefs) - almost certainly invoked via a ROM/init-table
 *  function pointer at crt0 time, this project's established pattern (see
 *  task_sched.c's own crt0-table-walk headline finding), not independently
 *  traced this pass.
 * ---------------------------------------------------------------------------
 */
extern const char semihost_console_name[4];	/* resolved 0xc0023c50, confirmed ":tt\0" via read_memory */

void semihost_stdio_open_streams(void)	/* FUN_c001c2ac, @0xc001c2ac */
{
	struct { const char *name; int32_t mode; int32_t namelen; } block;
	int i;

	block.name    = semihost_console_name;
	block.mode    = 0;			/* "r" */
	block.namelen = 3;
	semihost_stdin_handle = software_interrupt(SYS_OPEN, &block);

	block.name    = semihost_console_name;
	block.mode    = 4;			/* "w" */
	block.namelen = 3;
	semihost_stdout_handle = software_interrupt(SYS_OPEN, &block);
	semihost_stderr_handle = semihost_stdout_handle;

	for (i = 0; i < 20; i++)
		newlib_fd_table[i].handle = -1;

	newlib_fd_table[0].handle = semihost_stdin_handle;
	newlib_fd_table[0].pos    = 0;
	newlib_fd_table[1].handle = semihost_stdout_handle;
	newlib_fd_table[1].pos    = 0;
}

/* ---------------------------------------------------------------------------
 *  semihost_fetch_errno_passthrough (FUN_c001c36c, @0xc001c36c) - the shared
 *  tail every failing syscall stub in this file calls: fetches the HOST's own
 *  errno via SYS_ERRNO, stores it into this firmware's own errno variable, and
 *  returns `retval` completely unchanged (conventionally -1, but transcribed
 *  generically since the real code never inspects it).
 *
 *  CORRECTION vs the raw static decompile ("*puVar1 = 0x13;"): 0x13 is SYS_ERRNO's
 *  own op number, loaded into r4 before the swi and then read back INTO r4 a
 *  second time immediately after (`cpy r4,r0`) with the swi's real result -
 *  Ghidra's decompiler, not modeling software_interrupt() as returning a value,
 *  kept the stale op-number literal instead of the reassignment. Confirmed by
 *  re-reading raw disassembly at c001c380-c001c3a0 directly (not from memory).
 * ---------------------------------------------------------------------------
 */
int32_t semihost_fetch_errno_passthrough(int32_t retval)	/* FUN_c001c36c, @0xc001c36c */
{
	*newlib_errno_ptr() = software_interrupt(SYS_ERRNO, 0);
	return retval;
}

/* ---------------------------------------------------------------------------
 *  semihost_check_neg1_errno (FUN_c001c3a8, @0xc001c3a8) - "if the value looks
 *  like a semihosting generic-failure sentinel (-1), fetch and stash the real
 *  errno; otherwise pass it through untouched." Used by newlib_open below as a
 *  belt-and-suspenders re-check after newlib_open_raw's own internal error path
 *  has already potentially fetched errno once (see newlib_open_raw's own note).
 * ---------------------------------------------------------------------------
 */
int32_t semihost_check_neg1_errno(int32_t retval)	/* FUN_c001c3a8, @0xc001c3a8 */
{
	if (retval != -1)
		return retval;
	return semihost_fetch_errno_passthrough(retval);
}

/* ---------------------------------------------------------------------------
 *  semihost_sys_read_raw / semihost_sys_write_raw (FUN_c001c3b4/FUN_c001c568,
 *  @0xc001c3b4/@0xc001c568) - the raw SYS_READ(6)/SYS_WRITE(5) trampolines:
 *  translate `fd` to a host handle, build a 3-word {handle,buf,len} argument
 *  block, trap, and return the RESIDUAL (bytes NOT transferred - 0 means fully
 *  satisfied, per the standard semihosting convention).
 *
 *  Ghidra's own decompile shows both as taking NO parameters (`void FUN_...(void)`)
 *  - confirmed via raw disassembly to be wrong: both callers (newlib_read/
 *  newlib_write below) pass their own (fd,buf,len) straight through in r0/r1/r2,
 *  and each function's own body copies r0/r1/r2 into the argument block exactly
 *  as if they were its own real parameters. Restored explicitly here.
 * ---------------------------------------------------------------------------
 */
int32_t semihost_sys_read_raw(int32_t fd, void *buf, int32_t len)	/* FUN_c001c3b4, @0xc001c3b4 */
{
	struct { int32_t handle; void *buf; int32_t len; } block;

	block.handle = newlib_fd_translate(fd);
	block.buf    = buf;
	block.len    = len;
	return software_interrupt(SYS_READ, &block);
}

int32_t semihost_sys_write_raw(int32_t fd, const void *buf, int32_t len)	/* FUN_c001c568, @0xc001c568 */
{
	struct { int32_t handle; const void *buf; int32_t len; } block;

	block.handle = newlib_fd_translate(fd);
	block.buf    = buf;
	block.len    = len;
	return software_interrupt(SYS_WRITE, &block);
}

/* ---------------------------------------------------------------------------
 *  newlib_read / newlib_write (FUN_c001c400/FUN_c001c5b4, @0xc001c400/
 *  @0xc001c5b4) - the real `_read()`/`_write()` entry points: translate fd to a
 *  handle, look up its fd-table slot, call the matching raw trampoline above,
 *  and on success advance the table's cached file position by the number of
 *  bytes actually transferred (`len - residual`). On failure (residual < 0 for
 *  read; residual == -1 OR residual == len for write - transcribed exactly per
 *  each function's own real comparison, not unified into one shared shape) both
 *  tail into semihost_fetch_errno_passthrough(-1) - CONFIRMED by disassembly
 *  that the literal -1 is what's passed, not the residual value that was in r0
 *  a few instructions earlier (Ghidra's own decompile shows a bare, argument-
 *  less call here too, the same implicit-register idiom as elsewhere in this
 *  file).
 * ---------------------------------------------------------------------------
 */
int32_t newlib_read(int32_t fd, void *buf, int32_t len)	/* FUN_c001c400, @0xc001c400 */
{
	int32_t handle   = newlib_fd_translate(fd);
	int32_t index    = newlib_fd_table_find(handle);
	int32_t residual = semihost_sys_read_raw(fd, buf, len);
	int32_t nread;

	if (residual < 0)
		return semihost_fetch_errno_passthrough(-1);

	nread = len - residual;
	if (index != 20)
		newlib_fd_table[index].pos += nread;
	return nread;
}

int32_t newlib_write(int32_t fd, const void *buf, int32_t len)	/* FUN_c001c5b4, @0xc001c5b4 */
{
	int32_t handle   = newlib_fd_translate(fd);
	int32_t index    = newlib_fd_table_find(handle);
	int32_t residual = semihost_sys_write_raw(fd, buf, len);
	int32_t nwritten;

	if (residual == -1 || residual == len)
		return semihost_fetch_errno_passthrough(-1);

	nwritten = len - residual;
	if (index != 20)
		newlib_fd_table[index].pos += nwritten;
	return nwritten;
}

/* ---------------------------------------------------------------------------
 *  newlib_lseek (FUN_c001c470, @0xc001c470) - `_lseek(fd, offset, whence)`.
 *  whence==1 (SEEK_CUR) adds the fd table's own cached position to `offset`;
 *  whence==2 (SEEK_END) issues a SYS_FLEN call and adds the file length instead;
 *  either way, execution falls into the SAME absolute-seek tail that whence==0
 *  (SEEK_SET) reaches directly - one real SYS_SEEK call, one shared success/
 *  failure return path, not three independent implementations.
 *
 *  Ghidra flags two "Removing unreachable block" warnings inside the absolute-
 *  seek tail (around 0xc001c4e8/0xc001c4f8) - re-checked via raw disassembly and
 *  NOT actually unreachable: both are heavily ARM-condition-coded (predicated)
 *  instruction groups (`ldrne`/`strne`/`cpyeq`/`mvnne`) with no explicit branch
 *  INTO them, which Ghidra's control-flow-graph construction appears to
 *  misclassify as a dead block rather than recognizing as conditional fall-
 *  through - a new decompiler-artifact shape for this project (distinct from
 *  the FUN_c0009534/FUN_c0009540 "ARM/Thumb interworking" false alarm
 *  eva_board_main.c already resolved). Both instruction groups ARE executed
 *  (conditionally) and both are reproduced below as ordinary C conditionals.
 *
 *  DAT_c001c54c (this function's own literal-pool copy of the fd-table base)
 *  had no entry in the static dump's own `dat` table - resolved via a direct
 *  `read_memory` at 0xc001c54c (bytes `30 e8 1c c0` little-endian = 0xc01ce830),
 *  matching every sibling copy exactly; see this file's own header comment.
 * ---------------------------------------------------------------------------
 */
int32_t newlib_lseek(int32_t fd, int32_t offset, int32_t whence)	/* FUN_c001c470, @0xc001c470 */
{
	int32_t xhandle = newlib_fd_translate(fd);
	int32_t index   = newlib_fd_table_find(xhandle);
	int32_t result;
	struct { int32_t handle; int32_t offset; } block;

	if (whence == 1) {			/* SEEK_CUR */
		if (index == 20)
			return -1;
		offset += newlib_fd_table[index].pos;
	} else if (whence == 2) {		/* SEEK_END */
		struct { int32_t handle; } flen_block;

		flen_block.handle = xhandle;
		offset += software_interrupt(SYS_FLEN, &flen_block);
	}
	/* whence == 0 (SEEK_SET), or any other value: `offset` is already the
	 * absolute target position - falls straight through to the seek below,
	 * exactly like the SEEK_CUR/SEEK_END cases do after adjusting `offset`. */

	block.handle = newlib_fd_translate(fd);	/* re-translated, matching the real code's
							 * own second independent call rather than
							 * reusing `xhandle` */
	block.offset = offset;
	result = software_interrupt(SYS_SEEK, &block);

	if (index != 20 && result == 0)
		newlib_fd_table[index].pos = offset;

	return (result == 0) ? offset : -1;
}

/* ---------------------------------------------------------------------------
 *  newlib_open_raw / newlib_open (FUN_c001c62c/FUN_c001c6e0, @0xc001c62c/
 *  @0xc001c6e0) - `_open()`'s real implementation and its public entry point.
 *
 *  Ghidra's own decompile signature for FUN_c001c62c is `(undefined4 param_1)` -
 *  ONE parameter - confirmed WRONG via raw disassembly: `cpy r5,r1` at the
 *  function's own entry reads a real second argument (open flags) that the
 *  static decompile simply never surfaced. Restored explicitly here as a real
 *  2-parameter function; FUN_c001c6e0's own body reads its own second stack
 *  argument back out (`ldr r1,[r11,#4]`, a "push all args then reload" prologue
 *  shape) specifically to forward it into this call, which only makes sense if
 *  FUN_c001c62c genuinely takes it.
 *
 *  Allocates a free fd-table slot (index search on handle==-1, exactly the
 *  inverse of the "mark free" step in newlib_close below), translates POSIX-
 *  shaped open flags (bit values 2/0x200/0x400/0x8 - matching O_RDWR/O_CREAT/
 *  O_TRUNC/O_APPEND on this target) into a semihosting SYS_OPEN mode value via
 *  the exact bit operations disassembly shows (not reinterpreted into named
 *  POSIX mode strings - how this collapses onto the real Angel/RDI 0-11 mode-
 *  number table is NOT independently confirmed), computes the name length via
 *  libc_strlen, and issues SYS_OPEN. On success, installs the host handle into
 *  the newly-found fd-table slot and returns `handle + 0x20` - the SAME +0x20
 *  bias newlib_fd_translate (outside this file) subtracts back off for any fd
 *  it's given that isn't 0/1/2. On failure, fetches errno directly (via
 *  semihost_fetch_errno_passthrough) and returns the raw negative host result.
 *
 *  newlib_open (FUN_c001c6e0) is the "public" 2-argument entry point: reloads
 *  its own second argument off the stack (the "push all args then reload"
 *  prologue noted above) and forwards it, along with the FIRST argument still
 *  live in r0 since function entry, into newlib_open_raw, then runs the result
 *  through semihost_check_neg1_errno as a second (partially redundant, but
 *  transcribed exactly as the real code does it) errno re-check.
 * ---------------------------------------------------------------------------
 */
int32_t newlib_open_raw(const char *name, int32_t flags)	/* FUN_c001c62c, @0xc001c62c */
{
	int32_t index = newlib_fd_table_find(-1);	/* -1 = "find any free slot" */
	int32_t mode;
	int32_t namelen;
	int32_t host_handle;
	struct { const char *name; int32_t mode; int32_t namelen; } block;

	if (index == 20)
		return -1;	/* no free fd-table slot */

	mode = flags & 2;			/* O_RDWR-shaped bit */
	if (flags & 0x200)			/* O_CREAT-shaped bit */
		mode |= 4;
	if (flags & 0x400)			/* O_TRUNC-shaped bit (same target bit as O_CREAT above) */
		mode |= 4;
	if (flags & 0x8) {			/* O_APPEND-shaped bit overrides both */
		mode &= ~4;
		mode |= 8;
	}

	namelen = libc_strlen(name);

	block.name    = name;
	block.mode    = mode;
	block.namelen = namelen;
	host_handle = software_interrupt(SYS_OPEN, &block);

	if (host_handle < 0)
		return semihost_fetch_errno_passthrough(host_handle);

	newlib_fd_table[index].handle = host_handle;
	newlib_fd_table[index].pos    = 0;
	return host_handle + 0x20;
}

int32_t newlib_open(const char *name, int32_t flags)	/* FUN_c001c6e0, @0xc001c6e0 */
{
	int32_t result = newlib_open_raw(name, flags);
	return semihost_check_neg1_errno(result);
}

/* ---------------------------------------------------------------------------
 *  newlib_close (FUN_c001c700, @0xc001c700) - `_close(fd)`: translate fd to a
 *  handle, free the matching fd-table slot if one exists (mark handle == -1,
 *  the exact inverse of newlib_open_raw's own slot-allocation step above), then
 *  issue SYS_CLOSE and return its result directly (no errno-fetch wrapping on
 *  this path in the real code - transcribed as-is).
 * ---------------------------------------------------------------------------
 */
int32_t newlib_close(int32_t fd)	/* FUN_c001c700, @0xc001c700 */
{
	int32_t handle = newlib_fd_translate(fd);
	int32_t index  = newlib_fd_table_find(handle);
	struct { int32_t handle; } block;

	if (index != 20)
		newlib_fd_table[index].handle = -1;

	block.handle = handle;
	return software_interrupt(SYS_CLOSE, &block);
}

/* ---------------------------------------------------------------------------
 *  semihost_op18_fixed_call (FUN_c001c788, @0xc001c788) - a hardcoded op=0x18
 *  (24) semihosting call with a fixed argument value (0x20026, passed as r1
 *  even though op 0x18 isn't one of the standard 1-word-block ops this file
 *  otherwise confirms). 0x18 is NOT a standard ARM Angel/RDI op in the range
 *  this file's other functions confirm (0x01/0x02/0x05/0x06/0x0a/0x0c/0x10/
 *  0x11/0x13) - genuinely NOT decoded, transcribed exactly as observed. Task_
 *  sched.c's own block-(B) summary already flagged this one as "returns a
 *  packed SWI-op/mode constant" without decoding further; this pass didn't
 *  resolve it either. No confirmed callers in this static dump.
 * ---------------------------------------------------------------------------
 */
int32_t semihost_op18_fixed_call(void)	/* FUN_c001c788, @0xc001c788 */
{
	return software_interrupt(0x18, (void *)0x20026);
}

/* ---------------------------------------------------------------------------
 *  newlib_sbrk (FUN_c001c7b8, @0xc001c7b8) - `_sbrk(increment)`: lazily
 *  initializes the heap pointer from a fixed heap-start constant on first call,
 *  refuses to grow past the current stack pointer (ENOMEM), otherwise advances
 *  the heap pointer and returns the OLD value - the standard sbrk() contract.
 *
 *  newlib_sbrk_heap_start (DAT_c001c810, resolved 0xc023c010) is almost
 *  certainly the linker's own `_end`/heap-start symbol, but this pass had no
 *  symbol-table cross-reference to confirm that beyond the resolved address
 *  itself - not asserted as fact.
 *
 *  The real code compares against a raw stack-pointer-relative address
 *  (Ghidra's own `&stack0xfffffff0`, i.e. literally `sp - 0x10` at the point of
 *  comparison) - reproduced here via `__builtin_frame_address(0)`, the same
 *  substitution task_sched.c's own sched_dispatch already established
 *  precedent for (`outgoing->saved_sp = __builtin_frame_address(0);`) when a
 *  raw SP value needs a portable-C stand-in; the exact `-0x10` byte offset is
 *  NOT reproduced (no safe, non-fabricated portable way to express it), so this
 *  guard is slightly more permissive than the real firmware's own check by up
 *  to 16 bytes - flagged, not hidden.
 * ---------------------------------------------------------------------------
 */
extern uint8_t *newlib_sbrk_heap_ptr;		/* DAT_c001c80c, resolved 0xc01ce81c */
extern uint8_t *newlib_sbrk_heap_start;	/* DAT_c001c810, resolved 0xc023c010 - see note above */

void *newlib_sbrk(int32_t increment)	/* FUN_c001c7b8, @0xc001c7b8 */
{
	uint8_t *base = newlib_sbrk_heap_ptr;
	void *sp_probe = __builtin_frame_address(0);	/* stand-in for the raw `sp - 0x10` compare, see above */

	if (base == 0) {
		base = newlib_sbrk_heap_start;
		newlib_sbrk_heap_ptr = base;
	}

	if ((uint8_t *)sp_probe < base + increment) {
		*newlib_errno_ptr() = 0xc;	/* ENOMEM */
		return (void *)-1;
	}

	newlib_sbrk_heap_ptr = base + increment;
	return base;
}

/* ---------------------------------------------------------------------------
 *  newlib_probe_open_close (FUN_c001c84c, @0xc001c84c) - opens `name` read-only
 *  and, on success, zeroes a caller-supplied 0x3c-byte record and stamps two
 *  fixed words into it (+0x4 = 0x8100, +0x2c = 0x400 - roughly FILE-struct-
 *  sized, but these offsets do NOT line up with newlib_dtoa_bigint.c's own
 *  confirmed `struct newlib_file` layout there, +0xc=flags/+0x14=buf_size -
 *  likely a distinct or partial record, not reconciled this pass), then
 *  IMMEDIATELY CLOSES the descriptor it just opened.
 *
 *  CONFIRMED BY RAW DISASSEMBLY, not just the (agreeing) static decompile: the
 *  open-succeeded path (after the memset + field stores + close) and the open-
 *  failed path both fall into the exact same `mov r3,#0; return r3` tail at
 *  0xc001c89c - this function ALWAYS returns 0, regardless of whether the open
 *  actually succeeded. A genuinely odd "best-effort probe" shape; left as an
 *  honest observation rather than reconciled into a tidier story. No confirmed
 *  callers in this static dump - likely reached via a data/init-table function
 *  pointer (this project's now-familiar pattern), not traced further this pass.
 * ---------------------------------------------------------------------------
 */
int32_t newlib_probe_open_close(const char *name, void *rec)	/* FUN_c001c84c, @0xc001c84c */
{
	int32_t fd = newlib_open(name, 0);

	if (fd >= 0) {
		libc_memset(rec, 0, 0x3c);
		*(uint32_t *)((uint8_t *)rec + 0x2c) = 0x400;
		*(uint32_t *)((uint8_t *)rec + 0x4)  = 0x8100;
		newlib_close(fd);
	}
	return 0;
}

/* ---------------------------------------------------------------------------
 *  newlib_reset_record_pair_time / newlib_reset_record_clock (FUN_c001c8b8/
 *  FUN_c001c900, @0xc001c8b8/@0xc001c900) - a matched pair of small "syscall-
 *  timestamp-into-a-record" helpers, one using SYS_TIME the other SYS_CLOCK.
 *
 *  CORRECTION vs both functions' own raw static decompiles ("*param_1 = 0x11;"
 *  / "*param_1 = 0x10;"): 0x11/0x10 are SYS_TIME's/SYS_CLOCK's own op numbers,
 *  each loaded into a register that is then IMMEDIATELY reloaded with the swi's
 *  real r0 result - the same stale-literal decompiler artifact already
 *  documented at the top of this file for semihost_fetch_errno_passthrough,
 *  confirmed independently here too via raw disassembly (r5/r4 respectively are
 *  each reloaded from r0 right after their own `swi`, and that reload is what's
 *  actually stored).
 *
 *  newlib_reset_record_pair_time additionally writes a SECOND, unconditional
 *  {0,0} into a totally separate caller-supplied record regardless of what
 *  happens to the first - transcribed exactly; no story unifies the two
 *  records here. Its own 64-bit return value (Ghidra: `ZEXT48(puVar1) << 0x20`)
 *  is a genuine register artifact (the high word tracks an r1 that's untouched
 *  since function entry, i.e. whatever the CALLER happened to leave there) with
 *  no safe, non-fabricated portable-C equivalent - not reproduced; only the
 *  meaningful low-word-shaped `int32_t` return (always 0 on the paths this file
 *  can confirm) is kept.
 *
 *  Neither function has a confirmed caller in this static dump. The shape (one
 *  syscall-filled timestamp word plus a zeroed sibling) is suggestive of
 *  stat()-style atime/mtime plumbing feeding newlib_dtoa_bigint.c's own
 *  `newlib_fstat_stub` (FUN_c0015b68, itself flagged there as incompletely
 *  resolved in this static dump) - a plausible but UNCONFIRMED hypothesis, not
 *  asserted as fact.
 * ---------------------------------------------------------------------------
 */
int32_t newlib_reset_record_pair_time(int32_t *time_rec, int32_t *zero_rec)	/* FUN_c001c8b8, @0xc001c8b8 */
{
	if (time_rec != 0) {
		time_rec[1] = 0;
		time_rec[0] = software_interrupt(SYS_TIME, 0);
	}
	if (zero_rec != 0) {
		zero_rec[1] = 0;
		zero_rec[0] = 0;
	}
	return 0;
}

int32_t newlib_reset_record_clock(int32_t *rec)	/* FUN_c001c900, @0xc001c900 */
{
	int32_t clock_result = software_interrupt(SYS_CLOCK, 0);

	if (rec != 0) {
		rec[3] = 0;
		rec[0] = clock_result;
		rec[1] = 0;
		rec[2] = 0;
	}
	return clock_result;
}

/* ---------------------------------------------------------------------------
 *  newlib_isatty_stub (FUN_c001c93c, @0xc001c93c) - unconditionally returns 1.
 *  newlib_dtoa_bigint.c ALREADY cites this exact function (same name, same
 *  address) as an `extern` ("either a stubbed-out isatty() or dead code on this
 *  build") because its own assigned range calls it but doesn't own its address;
 *  this IS that function's real, and trivial, definition - confirmed by both
 *  the static decompile and raw disassembly (`mov r0,#1; mov pc,lr`, nothing
 *  else in the function).
 * ---------------------------------------------------------------------------
 */
int32_t newlib_isatty_stub(int32_t fd)	/* FUN_c001c93c, @0xc001c93c */
{
	(void)fd;
	return 1;
}

/* ============================================================================
 *  STILL OPEN
 * ============================================================================
 *
 *  - newlib_fd_translate (FUN_c001c1a0), newlib_fd_table_find (FUN_c001c174),
 *    libc_strlen (FUN_c001c114), and newlib_errno_ptr (FUN_c001ca24) are all
 *    depended on above but NOT defined by any file yet as of this pass -
 *    confirmed by grepping every existing *.c file in this project for each
 *    address individually before writing this file (this project's own
 *    established anti-collision practice, given the coverage script's real
 *    false-negative history). FUN_c001c1a0/c174/c114 sit just below this file's
 *    own range (0xc001c070-0xc001c29c, the unclaimed remainder of task_sched.c's
 *    own block (B)); FUN_c001ca24 sits just above it. All four are natural
 *    material for a follow-up pass extending this same file (or a sibling one)
 *    in both directions.
 *  - FUN_c001c96c itself (immediately AFTER this file's own exclusive upper
 *    bound 0xc001c96c) was glimpsed incidentally while fetching disassembly for
 *    newlib_isatty_stub above - it calls newlib_errno_ptr, stores the literal
 *    0x58 (88) into it, and returns -1, i.e. an "unsupported operation, set a
 *    fixed errno" stub - NOT reconstructed here (starts exactly at this file's
 *    own exclusive range boundary; belongs to whichever pass claims the address
 *    range starting there).
 *  - newlib_open_raw's own POSIX-flag-bits-to-semihosting-mode translation is
 *    transcribed exactly (bit operations only) but not independently verified
 *    against the real Angel/RDI 0-11 mode-number table - see that function's
 *    own comment.
 *  - semihost_op18_fixed_call's op number (0x18) and fixed argument (0x20026)
 *    are transcribed exactly but NOT decoded - not a standard semihosting op
 *    this file can otherwise confirm.
 *  - newlib_reset_record_pair_time / newlib_reset_record_clock have no
 *    confirmed callers in this static dump; their possible link to
 *    newlib_fstat_stub (newlib_dtoa_bigint.c) is a hypothesis, not a finding.
 *  - newlib_probe_open_close likewise has no confirmed callers; its "always
 *    returns 0" behavior is confirmed by disassembly but its PURPOSE (why probe-
 *    open-and-immediately-close a file while stamping fixed constants into an
 *    unrelated struct) is not understood beyond what's transcribed.
 */
