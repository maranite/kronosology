# OA.ko hot-swap bug — /proc/.shm dangling fops

Stock OA.ko's `cleanup_module` calls `CleanupSharedMemProcInterface` which calls `remove_proc_entry(".shm", NULL)` correctly — but the kernel doesn't immediately destroy the entry if userspace still has fds open on it. Eva keeps `/proc/.shm` open across its lifetime. So:

1. `rmmod OA` (with Eva running or after killing Eva) → CleanupSharedMemProcInterface calls remove_proc_entry, but the entry's refcount stays > 0 because of held fds.
2. Module text is freed by kernel module unloader.
3. Stale fds still have `proc_dir_entry->proc_fops` pointing to OA's freed `MemoryModProcFileOps` table.
4. When the holder closes the fd (e.g. process exit → `do_exit → put_files_struct → filp_close`), the kernel calls `fops->release(...)` which dereferences freed memory → **kernel oops** at random `EIP`.

**Why:** Observed live on the Kronos when SSH session's `sh` exited after a `rmmod OA` test. Even the SSH shell process closing triggers the oops because something it inherited had `/proc/.shm` open. After the oops, kernel prints `Fixing recursive fault but reboot is needed!` — the system is degraded.

**How to apply:**
- **Do NOT hot-swap OA.ko via simple rmmod+insmod** when Eva (or anything that opened `/proc/.shm`) is running.
- For experimental swaps: must kill Eva *and* any process that has `/proc/.shm` open *and* (probably) wait for proc to garbage-collect the entry. Even then, replacing OA in-place still risks subtle state issues with `/proc/.oacmd`.
- The robust fix is to make stock OA's cleanup more aggressive (refuse to unload while fds open, OR force-close them) — non-trivial in 2.6.32 kernel ABI.
- **Operational guidance:** When testing patched OA, deploy `loadoa-patches` (which loads `/sbin/OA.ko` instead of `/korg/Mod/OA.ko`) + `loadmod-patches` (MD5 bypass so loadoa change isn't detected) and do a clean boot, rather than hot-swapping. See `loadmod-md5-check-files` for which files break stock loadmod when modified.

**Note:** OA.ko also creates `/proc/.oacmd` via InitPcmModProcInterface. That entry IS cleanly removed by stock cleanup (we verified — after rmmod stock, /proc/.oacmd disappeared). The bug is specific to `.shm` because Eva holds it open.
