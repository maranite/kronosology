// SPDX-License-Identifier: GPL-2.0
/*
 * procfs.c  -  /proc interface.
 *
 *  Four entries are created under OmapNKS4ProcInitialize():
 *      <root>          read = event stream      write = control commands
 *      progress        read = "%3d" percent     write = set/inc/add progress
 *      hwversion       read = "%02u"            (hardware version)
 *      omapversion     read = "%02u%02u"        (OMAP version/revision)
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
extern const char *sEvents[];

/* ---- event ring -------------------------------------------------------- */

void OmapNKS4ProcAddEvent(unsigned char event)
{
	_spin_lock();
	((unsigned char *)sEventQueue)[sEventQueueWriteIndex] = event;
	if (sNumEventsInQueue < 0x400)
		__sync_fetch_and_add(&sNumEventsInQueue, 1);
	else
		sEventQueueReadIndex = (sEventQueueReadIndex + 1) & 0x3ff;
	_spin_unlock();
	sEventQueueWriteIndex = (sEventQueueWriteIndex + 1) & 0x3ff;
}

static int proc_read_event(void)
{
	if (sNumEventsInQueue == 0) {
		if (!sBlockOnRead)
			return 0;
		do {
			stg_msleep(1);
			if (sNumEventsInQueue != 0)
				break;
		} while (sBlockOnRead);
		if (sNumEventsInQueue == 0)
			return 0;
	}
	char ev = ((char *)sEventQueue)[sEventQueueReadIndex];
	sEventQueueReadIndex = (sEventQueueReadIndex + 1) & 0x3ff;
	__sync_fetch_and_sub(&sNumEventsInQueue, 1);
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

int OmapNKS4ProcRead(char *page, char **start, int off, int count, int *eof, void *data)
{
	if (off > 0) { *eof = 1; return 0; }
	return sprintf(page, sEvents[proc_read_event()]);
}

int OmapNKS4ProcWrite(void *file, char *buffer, unsigned int count, void *data)
{
	char *buf = (char *)kmalloc_buf(count + 1);
	if (!buf) { printk("<6>OmapNKS4: cannot allocate memory\n"); return -12; }
	if (copy_from_user(buf, buffer, count)) { printk("<6>OmapNKS4: copy from user\n"); return -14; }
	buf[count] = 0;

	if      (strstr(buf, "clearqueue"))     OmapNKS4ClearProcEventQueue();
	else if (strstr(buf, "installeron"))    COmapNKS4Driver_SetInstallerSupportOn(1);
	else if (strstr(buf, "installeroff"))   COmapNKS4Driver_SetInstallerSupportOn(0);
	else if (strstr(buf, "keys"))           COmapNKS4Driver_SetNumberOfKeys(simple_strtoul(buf, 0, 0));
	else if (strstr(buf, "shutdownenable")) COmapNKS4Driver_EnableShutdownByDriver();
	else if (strstr(buf, "block"))          sBlockOnRead = 1;
	else                                    sBlockOnRead = 0;

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

int OmapNKS4ProcWriteProgress(void *file, char *buffer, unsigned int count, void *data)
{
	char *buf = (char *)kmalloc_buf(count + 1);
	if (!buf) { printk("<6>OmapNKS4: cannot allocate memory\n"); return -12; }
	if (copy_from_user(buf, buffer, count)) { printk("<6>OmapNKS4: copy from user\n"); return -14; }
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

int OmapNKS4ProcInitialize(void)
{
	gProc = create_proc_entry("nks4");
	if (!gProc) { printk("<6>OmapNKS4: cannot create proc entry\n"); return -1; }
	proc_set(gProc, OmapNKS4ProcRead, OmapNKS4ProcWrite);

	gProcProgress = create_proc_entry("nks4progress");
	if (!gProcProgress) { printk("<6>OmapNKS4: cannot create progress proc entry\n"); return -1; }
	proc_set(gProcProgress, OmapNKS4ProcReadProgress, OmapNKS4ProcWriteProgress);

	gProcHardwareVersion = create_proc_entry("nks4hwversion");
	if (!gProcHardwareVersion) { printk("<6>OmapNKS4: cannot create hwversion proc entry\n"); return -1; }
	proc_set(gProcHardwareVersion, OmapNKS4ProcReadHardwareVersion, 0);

	gProcOmapVersion = create_proc_entry("nks4omapversion");
	if (!gProcOmapVersion) { printk("<6>OmapNKS4: cannot create omapversion proc entry\n"); return -1; }
	proc_set(gProcOmapVersion, OmapNKS4ProcReadOmapVersion, 0);
	return 0;
}
