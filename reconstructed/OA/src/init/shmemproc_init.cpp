// SPDX-License-Identifier: GPL-2.0
/*
 * shmemproc_init.cpp  -  InitSharedMemProcInterface()/
 * CleanupSharedMemProcInterface(): init_module step 6 (hard-fail). See
 * oa_shmemproc_init.h for the ground-truthing details.
 *
 * Faithful reconstruction from a full objdump disassembly of the real
 * `InitSharedMemProcInterface` (`.text+0x9b60`, 61 bytes) and
 * `CleanupSharedMemProcInterface` (`.text+0x9ba0`, 22 bytes) in
 * OA_real.ko -- both small enough to trace completely, no ambiguity
 * left in the control flow itself (unlike InitializeSTGHeap's own
 * address-space search loop).
 */

#include "oa_shmemproc_init.h"

/*
 * The real code's own static `struct file_operations` (a real Linux
 * kernel type this project's own reconstructed sources don't otherwise
 * need -- kept opaque/untyped here, see the header's own note on why
 * its actual handler pointers aren't independently confirmed in this
 * pass). Zero-initialized: `create_proc_entry` itself doesn't require
 * any handler to be non-null for the entry to be created successfully.
 */
static const void *s_shmProcFops[16];

int InitSharedMemProcInterface(void)
{
	/* Real name extracted directly from `.rodata.str1.1+0x1c8`:
	 * ".shm" -- so this creates `/proc/.shm`. Mode 0600 (owner
	 * read/write only, confirmed literal 0x180). */
	struct proc_dir_entry *entry = create_proc_entry(".shm", 0600, 0);
	if (!entry)
		return -1;

	entry->proc_fops = s_shmProcFops;
	entry->uid = 500;
	entry->gid = 500;
	return 0;
}

void CleanupSharedMemProcInterface(void)
{
	remove_proc_entry(".shm", 0);
}
