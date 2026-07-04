// SPDX-License-Identifier: GPL-2.0
/*
 * oa_cmd_proc.cpp  -  see include/oa_cmd_proc.h.
 *
 * Ground-truthed by direct disassembly + relocations (offsets in the header
 * comment). All four fops handlers, ParseOACmd, and the Init/Cleanup pair
 * are fully disassembly-confirmed, not paraphrase.
 */

#include "oa_cmd_proc.h"
#include "process_oacmd.h"

/* Real confirmed globals (.bss): sOACmdStatus (the state machine above),
 * sOACmdResult (the 4-byte integer result oa_cmd_read hands back). Owned
 * here; ProcessOACmd itself only ever receives &sOACmdResult as a plain
 * out-parameter (confirmed from disassembly -- it never touches the global
 * directly), which is why process_oacmd.cpp does not declare these. */
extern "C" int sOACmdStatus;
extern "C" int sOACmdResult;

extern "C" unsigned long copy_to_user(void *to, const void *from, unsigned long n);
extern "C" unsigned long copy_from_user(void *to, const void *from, unsigned long n);
extern "C" void mutex_lock(void *mutex);
extern "C" void mutex_unlock(void *mutex);
extern "C" void *PcmModuleMutex;	/* the mutex PcmModuleMutexLock/Unlock wrap */
extern "C" void *create_proc_entry(const char *name, unsigned int mode, void *parent);
extern "C" void  remove_proc_entry(const char *name, void *parent);

extern "C" void PcmModuleMutexLock(void)   { mutex_lock(&PcmModuleMutex); }
extern "C" void PcmModuleMutexUnlock(void) { mutex_unlock(&PcmModuleMutex); }

int oa_cmd_open(void *inode, void *file)
{
	(void)inode; (void)file;
	if (sOACmdStatus != OACMD_STATUS_IDLE)
		return -11;	/* -EAGAIN: already open by another client */
	sOACmdStatus = OACMD_STATUS_READY;
	return 0;
}

int oa_cmd_close(void *inode, void *file)
{
	(void)inode; (void)file;
	if (sOACmdStatus == OACMD_STATUS_READY || sOACmdStatus == OACMD_STATUS_RESULT) {
		sOACmdStatus = OACMD_STATUS_IDLE;
		return 0;
	}
	/* Real binary computes this 2-way split via a branchless bit trick
	 * (cmp/sbb/and/sub); equivalent result reproduced directly here. */
	return (sOACmdStatus == OACMD_STATUS_IDLE) ? -22 : -11;	/* -EINVAL : -EAGAIN (PROCESSING) */
}

long oa_cmd_read(void *file, char *userBuf, unsigned long count, long long *offset)
{
	(void)file;

	if (sOACmdStatus == OACMD_STATUS_IDLE)
		return -22;			/* -EINVAL: never opened a command */
	if (sOACmdStatus != OACMD_STATUS_RESULT)
		return -11;			/* -EAGAIN: READY or PROCESSING, no result yet */
	if (count != 4)
		return -22;			/* -EINVAL: must read exactly one 4-byte result */

	sOACmdStatus = OACMD_STATUS_READY;	/* consume the result, go back to READY */
	copy_to_user(userBuf, &sOACmdResult, 4);
	*offset += 4;
	return 4;
}

long oa_cmd_write(void *file, const char *userBuf, unsigned long count, long long *offset)
{
	(void)file;

	if (sOACmdStatus == OACMD_STATUS_PROCESSING)
		return -11;			/* -EAGAIN: a command is already running */
	if (sOACmdStatus != OACMD_STATUS_READY)
		return -22;			/* -EINVAL: IDLE (not open) or RESULT (unread) */

	char cmdBuf[128];
	unsigned long len = (count > 0x7f) ? 0x7f : count;
	copy_from_user(cmdBuf, userBuf, len);
	cmdBuf[len] = '\0';
	*offset += len;

	sOACmdStatus = OACMD_STATUS_PROCESSING;
	int rc = ProcessOACmd(cmdBuf, &sOACmdResult);
	if (rc != 0) {
		sOACmdStatus = OACMD_STATUS_READY;	/* bad/unrecognized command: let client retry */
		return -22;				/* -EINVAL */
	}
	sOACmdStatus = OACMD_STATUS_RESULT;
	return (long)len;				/* POSIX write(): bytes accepted */
}

int ParseOACmd(const char *cmd)
{
	if (ProcessOACmd(cmd, &sOACmdResult) != 0) {
		sOACmdStatus = OACMD_STATUS_READY;
		return -1;
	}
	sOACmdStatus = OACMD_STATUS_RESULT;
	return 0;
}

/*
 * Mirrors the real, unmodified Linux 2.6.32 struct file_operations layout
 * (fs.h) -- confirmed via relocation: +0x08 read, +0x0c write, +0x30 open,
 * +0x38 release are the only four non-null fields in the real 104-byte
 * oa_cmd_fops data blob; .owner (+0x00) is set to THIS_MODULE at link time,
 * not modeled here.
 */
struct oa_file_operations {
	void *owner;
	void *llseek;
	long (*read)(void *, char *, unsigned long, long long *);
	long (*write)(void *, const char *, unsigned long, long long *);
	void *aio_read, *aio_write, *readdir, *poll, *ioctl, *unlocked_ioctl, *compat_ioctl, *mmap;
	int (*open)(void *, void *);
	void *flush;
	int (*release)(void *, void *);
};

static struct oa_file_operations oa_cmd_fops = {
	/* owner   */ 0,
	/* llseek  */ 0,
	/* read    */ oa_cmd_read,
	/* write   */ oa_cmd_write,
	/* aio_read..mmap */ 0,0,0,0,0,0,0,0,
	/* open    */ oa_cmd_open,
	/* flush   */ 0,
	/* release */ oa_cmd_close,
};

/*
 * create_proc_entry(".oacmd", 0600, NULL) confirmed via the literal
 * immediates in the disassembly (mode=0x180=0600 octal); the created
 * entry's uid/gid fields are then set to 500/500 ("pocky:pocky", per prior
 * CLAUDE.md finding) and its fops field wired to oa_cmd_fops. Exact
 * proc_dir_entry field offsets (+0x10 uid, +0x14 gid, +0x24 proc_fops) are
 * confirmed from the disassembly's immediates, not from a real 2.6.32
 * proc_fs.h header (not included in this freestanding reconstruction).
 */
int InitPcmModProcInterface(void)
{
	void *entry = create_proc_entry(".oacmd", 0600, 0);
	if (!entry)
		return -1;

	*(int *)((char *)entry + 0x10) = 500;	/* uid: pocky */
	*(int *)((char *)entry + 0x14) = 500;	/* gid: pocky */
	*(struct oa_file_operations **)((char *)entry + 0x24) = &oa_cmd_fops;
	return 0;
}

void CleanupPcmModProcInterface(void)
{
	remove_proc_entry(".oacmd", 0);
}
