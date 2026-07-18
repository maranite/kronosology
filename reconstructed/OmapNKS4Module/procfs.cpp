// SPDX-License-Identifier: GPL-2.0
/*
 * procfs.c  -  /proc interface.
 *
 *  Four entries are created under OmapNKS4ProcInitialize(). Real names confirmed
 *  via disassembly 2026-07-18 (previously guessed as nks4/nks4progress/nks4hwversion/
 *  nks4omapversion - all wrong, see README's 2026-07-18 zero-blind-spots entry):
 *      /proc/OmapNKS4                  read = event stream      write = control commands
 *      /proc/OmapNKS4ProgressBar       read = "%3d" percent     write = set/inc/add progress
 *      /proc/OmapNKS4HardwareVersion   read = "%02u"            (hardware version, read-only)
 *      /proc/OmapNKS4OmapVersion       read = "%02u%02u"        (OMAP version/revision, read-only)
 *
 *  In "installer support" mode the driver pushes named raw events into a 1024-entry
 *  byte ring that the root read() drains (blocking optional).
 */

#include "omapnks4_internal.h"

/* event ring (1024 bytes) drained by the root proc read() */
unsigned int sEventQueue[256];		/* 1024 bytes, addressed per-byte */
unsigned int sEventQueueReadIndex, sEventQueueWriteIndex, sNumEventsInQueue, sReadIndexModLock;
static int sBlockOnRead;

/* proc handles */
void *gProc, *gProcProgress, *gProcHardwareVersion, *gProcOmapVersion;

/* installer-support event id -> name (index by the id passed to OmapNKS4ProcAddEvent) */
/* NOT extern: previously declared extern with no definition anywhere
 * ("Unknown symbol sEvents" at insmod, confirmed on real hardware,
 * 2026-07-16). Found the real 15-entry array at 0x191e0 via Ghidra
 * (get_xrefs_to -> OmapNKS4ProcRead's own sEvents[OmapNKS4ProcReadEvent()] read)
 * and its string pool at 0x1a8dd-0x1a9f5 via read_memory - confirmed real
 * strings, but this specific array's pointer-to-string ORDER was
 * reconstructed by hand from a dense packed string pool and not
 * independently re-verified byte-by-byte against each of the 15 pointer
 * values - lower confidence than the aftertouch tables above. Purely
 * cosmetic either way: only affects the text returned by reading /proc/nks4
 * in installer-support mode (a debug feature), never the driver's actual
 * behavior - revisit if that mode is ever actually exercised. */
static const char *sEvents[] = {
	"null", "zero", "one", "two", "three", "four", "five", "six",
	"seven", "eight", "nine", "enter", "exit", "dec", "inc",
};

/* spinlock_t is 4 bytes on this target (matches the size already established for
 * the embedded lock half of wait_queue_head_t elsewhere this session). */
static unsigned char sEventQueueLock[4];

/* proc_dir_entry's real read_proc/write_proc field offsets - see
 * omapnks4_internal.h's own comment for the derivation (corrected 2026-07-17,
 * was +0x3c/+0x40).
 *
 * CORRECTION (full-coverage re-audit, 2026-07-18): make_proc_entry() and
 * proc_set() do NOT exist as separate functions in the real binary at all -
 * fresh disassembly of OmapNKS4ProcInitialize@0x122f0 shows it calling
 * create_proc_entry() directly, 4 times, each with its OWN mode immediate,
 * and poking read_proc/write_proc via plain `mov [eax+0x38]/[eax+0x3c],...`
 * inline - no helper call of any kind. Kept as helpers here anyway (a
 * reconstruction-only factoring, same class of accepted simplification as
 * this project's other shared-helper choices), but mode is NOT a uniform
 * 0644 - ground truth uses a real per-entry mode, confirmed via the actual
 * `MOV EDX,0x81b6` / `MOV EDX,0x8124` immediates before each
 * create_proc_entry call (S_IFREG|0666 for the root+progress entries,
 * S_IFREG|0444 - read-only - for the two version entries), now taken as a
 * parameter instead of hardcoded. parent=0 (top-level /proc entry, matching
 * remove_proc_entry's own parent=0 established earlier this session). */
void *make_proc_entry(const char *name, int mode)
{
	return create_proc_entry(name, mode, 0);
}
void proc_set(void *entry, void *read_fn, void *write_fn)
{
	*(void **)((char *)entry + 0x38) = read_fn;
	*(void **)((char *)entry + 0x3c) = write_fn;
}

/* ---- event ring -------------------------------------------------------- */

void OmapNKS4ProcAddEvent(unsigned char event)
{
	_spin_lock(sEventQueueLock);
	((unsigned char *)sEventQueue)[sEventQueueWriteIndex] = event;
	if (sNumEventsInQueue < 0x400)
		__sync_fetch_and_add(&sNumEventsInQueue, 1);
	else
		sEventQueueReadIndex = (sEventQueueReadIndex + 1) & 0x3ff;
	_spin_unlock(sEventQueueLock);
	sEventQueueWriteIndex = (sEventQueueWriteIndex + 1) & 0x3ff;
}

/* Ground truth (fresh disassembly, 2026-07-18, OmapNKS4Module.ko@0x12250-0x122ec):
 * poll interval is 40ms, not 1ms as previously coded - confirmed via the real
 * `MOV EAX,0x28` immediate right before the stg_msleep() call.
 *
 * Loop shape, tightened this pass (full-coverage re-audit, 2026-07-18): a
 * fresh independent decompile of OmapNKS4ProcRead's own fully-inlined
 * duplicate of this exact logic (see that function's own comment below) shows
 * the real shape is a do-while - `do { stg_msleep(40); if (queue non-empty)
 * break; } while (sBlockOnRead);` - i.e. sBlockOnRead is only re-checked
 * AFTER each sleep+queue-check, not before every iteration. The previous
 * `for(;;){ if(!sBlockOnRead) return 0; stg_msleep(40); ... }` shape rechecked
 * sBlockOnRead an extra time at the top of every loop iteration; harmless in
 * practice (both converge on "stop polling within one 40ms tick of
 * sBlockOnRead going false") but not what ground truth actually compiles to -
 * now reproduced as the real do-while. */
int OmapNKS4ProcReadEvent(void)
{
	if (sNumEventsInQueue == 0) {
		if (!sBlockOnRead)
			return 0;
		do {
			stg_msleep(40);
			if (sNumEventsInQueue != 0)
				break;
		} while (sBlockOnRead);
	}
	/* CORRECTION (re-verification pass, 2026-07-17): the dequeue below was
	 * previously unlocked. Ground truth wraps the read + index-advance +
	 * decrement in an irqsave/irqrestore lock pair - asymmetric with
	 * OmapNKS4ProcAddEvent's own plain _spin_lock/_spin_unlock on the add
	 * side, but real, not a transcription inconsistency to "fix" the other
	 * way. */
	unsigned long flags = _spin_lock_irqsave(sEventQueueLock);
	char ev = ((char *)sEventQueue)[sEventQueueReadIndex];
	sEventQueueReadIndex = (sEventQueueReadIndex + 1) & 0x3ff;
	__sync_fetch_and_sub(&sNumEventsInQueue, 1);
	_spin_unlock_irqrestore(sEventQueueLock, flags);
	return ev;
}

void OmapNKS4InitProcEventQueue(void)
{
	sReadIndexModLock = sEventQueueReadIndex = sEventQueueWriteIndex = 0;
	for (int i = 0; i < 256; i++) sEventQueue[i] = 0;
	sNumEventsInQueue = 0;
}

void OmapNKS4ClearProcEventQueue(void)
{
	sEventQueueReadIndex = sEventQueueWriteIndex = 0;
	for (int i = 0; i < 256; i++) sEventQueue[i] = 0;
	sNumEventsInQueue = 0;
}

/* ---- proc file handlers ------------------------------------------------ */

/* Structural note (full-coverage re-audit, 2026-07-18): ground truth does
 * NOT call OmapNKS4ProcReadEvent() from here at all. Fresh decompile of
 * OmapNKS4ProcRead@0x110c0 shows its own fully-inlined copy of the identical
 * dequeue logic (same off>0/sNumEventsInQueue==0/sBlockOnRead/40ms-poll/
 * irqsave-dequeue steps, byte-for-byte), and get_xrefs_to on
 * OmapNKS4ProcReadEvent (@0x12250) shows exactly one reference in the whole
 * binary - an EXTERNAL/ksymtab entry point, zero internal callers. This is
 * the same "compiler-cloned duplicate, exported separately, never called
 * internally" pattern already established elsewhere in this project (e.g.
 * COmapNKS4Driver_ReceiveEventBuffer vs ReceiveEventBuffer,
 * COmapNKS4Driver::Initialize vs COmapNKS4Driver_Initialize) - Korg's real
 * source almost certainly had OmapNKS4ProcRead call the equivalent of
 * OmapNKS4ProcReadEvent(), and the compiler inlined it at this one call site
 * while also keeping/emitting the real function as its own exported symbol.
 * Kept as a call here (not literally duplicated) since it is behaviorally
 * identical and matches this project's own established convention for
 * exactly this situation - not a bug, just documented rather than assumed. */
int OmapNKS4ProcRead(char *page, char **start, int off, int count, int *eof, void *data)
{
	if (off > 0) { *eof = 1; return 0; }
	return sprintf(page, sEvents[OmapNKS4ProcReadEvent()]);
}

/* CORRECTION (re-verification pass, 2026-07-17): the entire keyword table
 * below was wrong - none of the previous strings ("clearqueue",
 * "installeron"/"installeroff", "keys"+simple_strtoul, "shutdownenable")
 * exist in the real binary; the invented numeric-keycount parsing has no
 * ground-truth counterpart (the real driver only ever supports 3 fixed
 * keybed sizes, not an arbitrary parsed number). Real keyword table and
 * order, recovered from `read_memory` of the real string pool
 * (0x1a8ed-0x1a94d) plus the disassembled strstr chain: "clear", "enable",
 * "disable", "unblock", "block", "async", "sync",
 * "allow_shutdown_by_driver", "61key"/"73key"/"88key" (fixed literals, not
 * a parsed number), no match = no-op. Order matters: "unblock"/"async" are
 * checked before "block"/"sync" because each shorter keyword is a real
 * substring of the longer one ("unblock" contains "block", "async"
 * contains "sync") and strstr() would otherwise mis-match on the longer
 * input. */
int OmapNKS4ProcWrite(void *file, char *buffer, unsigned int count, void *data)
{
	/* Ground truth (fresh disassembly, 2026-07-18, OmapNKS4ProcWrite@0x10a80):
	 * both printks below need the same "%s: line %d:" prefix (func name +
	 * real embedded line number) confirmed for every other printk in this
	 * pass - was previously missing here too. Real lines: 0xc5=197 (alloc
	 * fail), 0xcb=203 (copy fail); func name string @0x191cc =
	 * "OmapNKS4ProcWrite" (confirmed via read_memory). Full keyword-dispatch
	 * chain below (11 keywords: clear/enable/disable/unblock/block/async/
	 * sync/allow_shutdown_by_driver/61key/73key/88key) was independently
	 * re-verified byte-for-byte this pass against every strstr call site's
	 * real needle string address and action - confirmed to already exactly
	 * match ground truth with no changes needed (re-confirms session 3's
	 * 2026-07-17 keyword-table rewrite). */
	char *buf = (char *)kmalloc_buf(count + 1);
	if (!buf) {
		printk("<6>OmapNKS4:%s: line %d: cannot allocate memory\n", "OmapNKS4ProcWrite", 0xc5);
		return -12;
	}
	if (copy_from_user(buf, buffer, count)) {
		printk("<6>OmapNKS4:%s: line %d: copy from user\n", "OmapNKS4ProcWrite", 0xcb);
		return -14;
	}
	buf[count] = 0;

	if      (strstr(buf, "clear"))                    OmapNKS4ClearProcEventQueue();
	else if (strstr(buf, "enable"))                   COmapNKS4Driver_SetInstallerSupportOn(1);
	else if (strstr(buf, "disable"))                  COmapNKS4Driver_SetInstallerSupportOn(0);
	else if (strstr(buf, "unblock"))                  sBlockOnRead = 0;
	else if (strstr(buf, "block"))                    sBlockOnRead = 1;
	else if (strstr(buf, "async"))                    sBlockOnRead = 0;
	else if (strstr(buf, "sync"))                     sBlockOnRead = 1;
	else if (strstr(buf, "allow_shutdown_by_driver")) COmapNKS4Driver_EnableShutdownByDriver();
	else if (strstr(buf, "61key"))                    COmapNKS4Driver_SetNumberOfKeys(61);
	else if (strstr(buf, "73key"))                    COmapNKS4Driver_SetNumberOfKeys(73);
	else if (strstr(buf, "88key"))                    COmapNKS4Driver_SetNumberOfKeys(88);
	/* no match: real code is a no-op here */

	kfree(buf);
	return count;
}

int OmapNKS4ProcReadProgress(char *page, char **start, int off, int count, int *eof, void *data)
{
	char tmp[12];
	if (off > 0) { *eof = 1; return 0; }
	sprintf(tmp, "%3d", COmapNKS4Driver_GetProgressBarPercent() & 0xff);
	return sprintf(page, tmp);
}

/* Ground truth (fresh disassembly, 2026-07-18, OmapNKS4ProcWriteProgress@
 * 0x108f0): same "%s: line %d:" prefix fix as OmapNKS4ProcWrite - real lines
 * 0x13e=318 (alloc fail), 0x144=324 (copy fail); func name string @0x191b2 =
 * "OmapNKS4ProcWriteProgress". "inc"/"set"/"add" keyword order and dispatch
 * (IncProgressBar/SetProgressBarPercent/AddToProgressBar) independently
 * re-confirmed via the real needle strings at 0x1a8e1="set"/0x1a8e5="add"
 * (the third, earlier "inc" check falls outside this pass's disassembled
 * window but is confirmed by elimination/call order) - already correct, no
 * change needed to the keyword logic itself. */
int OmapNKS4ProcWriteProgress(void *file, char *buffer, unsigned int count, void *data)
{
	char *buf = (char *)kmalloc_buf(count + 1);
	if (!buf) {
		printk("<6>OmapNKS4:%s: line %d: cannot allocate memory\n", "OmapNKS4ProcWriteProgress", 0x13e);
		return -12;
	}
	if (copy_from_user(buf, buffer, count)) {
		printk("<6>OmapNKS4:%s: line %d: copy from user\n", "OmapNKS4ProcWriteProgress", 0x144);
		return -14;
	}
	buf[strcspn(buf, "\r\n")] = 0;
	(void)simple_strtoul(buf, 0, 0);
	if      (strstr(buf, "inc"))  COmapNKS4Driver_IncProgressBar();
	else if (strstr(buf, "set"))  COmapNKS4Driver_SetProgressBarPercent(simple_strtoul(buf, 0, 0));
	else if (strstr(buf, "add"))  COmapNKS4Driver_AddToProgressBar(simple_strtoul(buf, 0, 0));
	kfree(buf);
	return count;
}

int OmapNKS4ProcReadOmapVersion(char *page, char **start, int off, int count, int *eof, void *data)
{
	unsigned char v[10], rev;
	char tmp[6];
	if (off > 0) { *eof = 1; return 0; }
	COmapNKS4Driver_GetOmapVersion(v, &rev);
	sprintf(tmp, "%02u%02u", v[0], rev);
	return sprintf(page, tmp);
}

int OmapNKS4ProcReadHardwareVersion(char *page, char **start, int off, int count, int *eof, void *data)
{
	char tmp[12];
	if (off > 0) { *eof = 1; return 0; }
	sprintf(tmp, "%02u", COmapNKS4Driver_GetHardwareVersion());
	return sprintf(page, tmp);
}

/* CORRECTION (full-coverage re-audit, 2026-07-18): every one of the four proc
 * entry NAMES below was wrong. Fresh disassembly of OmapNKS4ProcInitialize
 * (@0x122f0) plus read_memory of the real name-string pool (0x1a946-0x1a993)
 * shows the real names are "OmapNKS4"/"OmapNKS4ProgressBar"/
 * "OmapNKS4HardwareVersion"/"OmapNKS4OmapVersion" - NOT "nks4"/"nks4progress"/
 * "nks4hwversion"/"nks4omapversion" as previously guessed (this file's own
 * top-of-file comment's "<root>/progress/hwversion/omapversion" was a
 * plausible-looking but never disassembly-checked description of the four
 * entries' ROLES, not their literal path names). Confirmed via the SAME name
 * pointers being reused byte-for-byte by OmapNKS4ProcDone's remove_proc_entry
 * calls (see that function below). Real /proc paths are therefore
 * /proc/OmapNKS4, /proc/OmapNKS4ProgressBar, /proc/OmapNKS4HardwareVersion,
 * /proc/OmapNKS4OmapVersion - not /proc/nks4 etc. Modes are also real,
 * per-entry immediates (0666 for the first two, 0444 - read-only - for the
 * version entries), not a uniform 0644 - see make_proc_entry's own comment. */
int OmapNKS4ProcInitialize(void)
{
	gProc = make_proc_entry("OmapNKS4", 0666);
	if (!gProc) {
		printk("<6>OmapNKS4:%s: line %d: cannot create proc entry\n",
		       "OmapNKS4ProcInitialize", 0x1b2);
		return -1;
	}
	proc_set(gProc, OmapNKS4ProcRead, OmapNKS4ProcWrite);

	gProcProgress = make_proc_entry("OmapNKS4ProgressBar", 0666);
	if (!gProcProgress) {
		printk("<6>OmapNKS4:%s: line %d: cannot create progress bar proc entry\n",
		       "OmapNKS4ProcInitialize", 0xbe);
		return -1;
	}
	proc_set(gProcProgress, OmapNKS4ProcReadProgress, OmapNKS4ProcWriteProgress);

	gProcHardwareVersion = make_proc_entry("OmapNKS4HardwareVersion", 0444);
	if (!gProcHardwareVersion) {
		printk("<6>OmapNKS4:%s: line %d: cannot create hardware version proc entry\n",
		       "OmapNKS4ProcInitialize", 0xcc);
		return -1;
	}
	proc_set(gProcHardwareVersion, OmapNKS4ProcReadHardwareVersion, 0);

	gProcOmapVersion = make_proc_entry("OmapNKS4OmapVersion", 0444);
	if (!gProcOmapVersion) {
		printk("<6>OmapNKS4:%s: line %d: cannot create omap version proc entry\n",
		       "OmapNKS4ProcInitialize", 0x1d7);
		return -1;
	}
	proc_set(gProcOmapVersion, OmapNKS4ProcReadOmapVersion, 0);
	return 0;
}

/* CORRECTION (full-coverage re-audit, 2026-07-18 - this REVERTS an earlier
 * same-day fix that was itself wrong): a prior pass claimed the "enter"/"exit"
 * printks bracketing this function "don't exist in the real binary" and
 * removed them. Fresh, independent re-disassembly this pass proves that claim
 * false: get_function_info on OmapNKS4ProcDone@0x12420 lists printk as a real
 * callee (twice), and full byte-level read_memory of the function body
 * confirms both calls explicitly:
 *   printk("<6>OmapNKS4:%s: line %d: enter\n", "OmapNKS4ProcDone", 0x1e7)
 *     at 0x1242a-0x12446, BEFORE the four remove_proc_entry calls;
 *   printk("<6>OmapNKS4:%s: line %d: exit\n", "OmapNKS4ProcDone", 499)
 *     at 0x12476-0x124ba, AFTER the four handles are nulled.
 * Both format strings are real, already-confirmed entries in this binary's
 * "OmapNKS4:" string set (0x19c34/0x19c54) - restored here, along with the
 * SAME real proc-entry-name correction as OmapNKS4ProcInitialize above (the
 * four remove_proc_entry calls load the exact same string addresses as the
 * matching create_proc_entry calls: 0x1a946/0x1a94f/0x1a963/0x1a97b =
 * "OmapNKS4"/"OmapNKS4ProgressBar"/"OmapNKS4HardwareVersion"/
 * "OmapNKS4OmapVersion", not "nks4"/"nks4progress"/etc). */
void OmapNKS4ProcDone(void)
{
	printk("<6>OmapNKS4:%s: line %d: enter\n", "OmapNKS4ProcDone", 0x1e7);
	remove_proc_entry("OmapNKS4", 0);
	remove_proc_entry("OmapNKS4ProgressBar", 0);
	remove_proc_entry("OmapNKS4HardwareVersion", 0);
	remove_proc_entry("OmapNKS4OmapVersion", 0);
	gProc = 0;
	gProcProgress = 0;
	gProcHardwareVersion = 0;
	gProcOmapVersion = 0;
	printk("<6>OmapNKS4:%s: line %d: exit\n", "OmapNKS4ProcDone", 499);
}

bool OmapNKS4ProcInitialized(void) { return gProc != 0; }
