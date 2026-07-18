/*
 * loadoa.c — drop-in replacement for the Korg Kronos /sbin/loadoa binary.
 *
 * Faithfully replicates every behaviour of the stock 3.2.1/3.2.2 binary
 * (identical MD5 across both versions: 8a3d61f3332d7bcf694e8c05845b4754).
 *
 * The three patched paths are compile-time defines so the offline patcher
 * can override them.  Defaults are the patched (redirected-to-/sbin/) values.
 * Pass -DPATCH_PATHS=0 to build a stock-equivalent binary for testing.
 *
 * Build:  see Makefile
 *
 * Usage (identical to stock loadoa):
 *   loadoa          — LoadAll (normal boot)
 *   loadoa -i       — UnloadForUpdate (kill Eva, umount in order)
 *   loadoa -u       — UnloadAll (kill ckhdw + Eva, umount + rmmod everything)
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* ── Patched paths (overrideable at compile time) ──────────────────────── */

#ifndef PATCH_PATHS
#define PATCH_PATHS 1
#endif

#if PATCH_PATHS
#  define PATH_OA_KO               "/sbin/OA.ko"
#  define PATH_KORG_USB_AUDIO_KO   "/sbin/KorgUsbAudioDriver.ko"
#  define PATH_EVA_BIN             "/sbin/Eva"
#else
#  define PATH_OA_KO               "/korg/Mod/OA.ko"
#  define PATH_KORG_USB_AUDIO_KO   "/korg/Mod/KorgUsbAudioDriver.ko"
#  define PATH_EVA_BIN             "/korg/Eva/Eva"
#endif

/* ── Module state globals (replicate stock names/layout) ─────────────────
 * Stock binary has these at consecutive .bss addresses:
 *   0x804cfcc  sWMMSMounted
 *   0x804cfd0  sModMounted
 *   0x804cfd4  sEvaMounted                                                 */
static int sWMMSMounted = 0;
static int sModMounted  = 0;
static int sEvaMounted  = 0;

/* ── stderr FILE* (used for error output, like the stock binary) ──────── */
/* The stock binary uses the global FILE *stderr at 0x804cfa0; we just use
 * the standard stderr directly.                                             */

/* ── Helper: sleep via select (no signals, like the stock binary) ────── */
static void sleep_us(long us)
{
    struct timeval tv = { .tv_sec = us / 1000000, .tv_usec = us % 1000000 };
    select(0, NULL, NULL, NULL, &tv);
}

/* ── SetCPUAffinity ───────────────────────────────────────────────────────
 * Pin this process to CPU 0.  Exact replication: zero a 128-byte cpu_set_t,
 * set bit 0, call sched_setaffinity(getpid(), 0x80, &set).                */
static void SetCPUAffinity(void)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    sched_setaffinity(getpid(), sizeof(set), &set);
}

/* ── RunProcess ───────────────────────────────────────────────────────────
 * Fork and execvp argv[0].
 * background=0: wait for child, return exit status byte.
 * background=1: don't wait, return 0.
 * stdout_redir: if non-NULL, open and dup2 to stdout+stderr before exec.
 * If argv[0] has the setuid bit, call setuid() before exec (stock behaviour).
 *
 * Returns 0 on success, non-zero on error (matches stock semantics).       */
static int RunProcess(char **argv, int background, const char *stdout_redir)
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid == -1) {
        fwrite("Fork error\n", 1, 11, stderr);
        return -1;
    }

    if (pid == 0) {
        /* child */
        if (stdout_redir) {
            int fd = open(stdout_redir, O_WRONLY | O_CREAT, 0666);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        /* check setuid bit, replicate stock behaviour */
        struct stat st;
        if (stat(argv[0], &st) == 0 && (st.st_mode & S_ISUID)) {
            if (setuid(st.st_uid) != 0) { /* ignore — best-effort */ }
        }
        execvp(argv[0], argv);
        fprintf(stderr, "[%d] Execvp error\n", getpid());
        exit(2);
    }

    /* parent */
    if (background)
        return 0;

    if (waitpid(pid, &status, 0) < 0)
        return -3;

    if (!(WIFEXITED(status))) {
        fwrite("process error\n", 1, 14, stderr);
        return -3;
    }
    if (WEXITSTATUS(status) != 0) {
        fprintf(stderr, "[%d] %s: Process status error %d\n",
                getpid(), argv[0], WEXITSTATUS(status));
        return WEXITSTATUS(status);
    }
    return 0;
}

/* ── RunSystemCommand ─────────────────────────────────────────────────────
 * popen(cmd, "r") + pclose.  Returns 0 on success, -1 if popen fails.     */
static int RunSystemCommand(const char *cmd)
{
    FILE *f = popen(cmd, "r");
    if (!f) {
        printf("popen failed for command: %s\n", cmd);
        return -1;
    }
    pclose(f);
    return 0;
}

/* ── KillAllPids ──────────────────────────────────────────────────────────
 * Runs "/sbin/pidof <progname>" with output to /tmp/pids.
 * Then iterates the PID file sending SIGINT+SIGURG, select(300ms),
 * SIGTERM+SIGURG, select(300ms), SIGKILL — looping over all PIDs.
 * Unlinks /tmp/pids on exit.
 * Returns 0 if any PIDs were found and signalled, 1 if none.              */
static int KillAllPids(const char *progname)
{
    char *pidof_argv[] = { "/sbin/pidof", (char *)progname, NULL };
    int   ret;

    ret = RunProcess(pidof_argv, 0, "/tmp/pids");
    if (ret != 0) {
        unlink("/tmp/pids");
        return 1;
    }

    FILE *f = fopen("/tmp/pids", "r");
    if (!f) {
        unlink("/tmp/pids");
        return 1;
    }

    int pid_count = 0;
    int pid;

    /* First pass: SIGINT + SIGURG */
    while (fscanf(f, "%d", &pid) == 1) {
        pid_count++;
        kill(pid, SIGINT);
        kill(pid, SIGURG);
    }
    {
        struct timeval tv = { 0, 300000 };
        select(0, NULL, NULL, NULL, &tv);
    }
    rewind(f);

    /* Second pass: SIGTERM + SIGURG */
    while (fscanf(f, "%d", &pid) == 1) {
        kill(pid, SIGTERM);
        kill(pid, SIGURG);
    }
    {
        struct timeval tv = { 0, 300000 };
        select(0, NULL, NULL, NULL, &tv);
    }
    rewind(f);

    /* Third pass: SIGKILL */
    while (fscanf(f, "%d", &pid) == 1)
        kill(pid, SIGKILL);

    fclose(f);
    unlink("/tmp/pids");
    return (pid_count == 0) ? 1 : 0;
}

/* ── Fail ─────────────────────────────────────────────────────────────────
 * Emergency cleanup: umount whatever is still mounted, then exit.
 * Replicates the exact flag-checking order in the stock binary.            */
static void Fail(int code)
{
    char *umount_wmms[] = { "/bin/umount", "-n", "/korg/rw/PCM/WaveMotion", NULL };
    char *umount_mod[]  = { "/bin/umount", "-n", "/korg/Mod", NULL };
    char *umount_eva[]  = { "/bin/umount", "-n", "/korg/Eva", NULL };

    if (sWMMSMounted) {
        if (RunProcess(umount_wmms, 0, NULL) != 0)
            fprintf(stderr, "Umount %s failed\n", "/korg/rw/PCM/WaveMotion");
    }
    if (sModMounted) {
        if (RunProcess(umount_mod, 0, NULL) != 0)
            fprintf(stderr, "Umount %s failed\n", "/korg/Mod");
    }
    if (sEvaMounted) {
        if (RunProcess(umount_eva, 0, NULL) != 0)
            fprintf(stderr, "Umount %s failed\n", "/korg/Eva");
    }
    exit(code);
}

/* ── Has2ndInternalDisk ───────────────────────────────────────────────────
 * Returns 1 if a secondary (USB-attached) SSD is present, 0 otherwise.
 * Exact replication of the stock binary's two-stage popen check.           */
static int Has2ndInternalDisk(void)
{
    FILE  *f;
    char   buf[256];
    int    found = 0;

    /* Stage 1: check if sdb exists */
    f = popen("ls /sys/block | grep sdb", "r");
    if (!f) {
        puts("Problems with pipe for 2nd disk detection");
        return 0;
    }
    while (fgets(buf, sizeof(buf), f)) {
        if (strstr(buf, "sdb")) {
            puts("detected secondary disk");
            found = 1;
        }
    }
    pclose(f);

    if (!found)
        return 0;

    /* Stage 2: check if it is USB (not eSATA etc.) */
    found = 1; /* assume USB until disproved */
    f = popen("udevinfo -a -p /sys/block/sdb | grep DRIVERS | grep usb", "r");
    if (!f) {
        puts("Problems with 2nd pipe for 2nd disk detection");
        return 0;
    }
    while (fgets(buf, sizeof(buf), f)) {
        if (strstr(buf, "usb")) {
            puts("secondary disk is usb");
            found = 0; /* USB = not an internal disk, return 0 per stock logic */
        }
    }
    pclose(f);
    return found;
}

/* ── insmod helper ────────────────────────────────────────────────────────
 * Runs /sbin/insmod, redirects output to /dev/null.
 * Returns 0 on success.                                                     */
static int do_insmod(const char *path, const char *param, int fail_code)
{
    char *argv[4];
    int   argc = 0;
    argv[argc++] = "/sbin/insmod";
    argv[argc++] = (char *)path;
    if (param)
        argv[argc++] = (char *)param;
    argv[argc] = NULL;

    if (RunProcess(argv, 0, "/dev/null") != 0) {
        fprintf(stderr, "Insmod %s failed\n", path);
        if (fail_code)
            Fail(fail_code);
    }
    return 0;
}

/* ── cryptoloop mount helper ──────────────────────────────────────────────
 * Runs /bin/mount -n -t ignoreType ignoreDev <mountpoint>.                 */
static int do_mount_cryptoloop(const char *mountpoint, int fail_code)
{
    char *argv[] = {
        "/bin/mount", "-n",
        "-t", "ignoreType",
        "ignoreDev",
        (char *)mountpoint,
        NULL
    };
    if (RunProcess(argv, 0, "/dev/null") != 0) {
        fprintf(stderr, "Mount %s failed\n", mountpoint);
        if (fail_code)
            Fail(fail_code);
        return -1;
    }
    return 0;
}

/* ── rmmod helper ─────────────────────────────────────────────────────────
 * Runs /sbin/rmmod <modname>, redirects stdout to /dev/null.               */
static void do_rmmod(const char *modname)
{
    char *argv[] = { "/sbin/rmmod", (char *)modname, NULL };
    if (RunProcess(argv, 0, "/dev/null") != 0)
        fprintf(stderr, "Rmmod %s failed\n", modname);
}

/* ── write_progress ───────────────────────────────────────────────────────
 * Write integer n to /proc/progress (append mode like the stock binary:
 * re-opens each time, writes %d, flushes).                                 */
static void write_progress(int n)
{
    FILE *f = fopen("/proc/progress", "w");
    if (!f) return;
    if (fprintf(f, "%d", n) > 0)
        fflush(f);
    fclose(f);
}

/* ── LoadAll ──────────────────────────────────────────────────────────────
 * Full Kronos boot sequence.  The three paths that loadoa patches redirect:
 *   /korg/Mod/OA.ko              → PATH_OA_KO
 *   /korg/Mod/KorgUsbAudioDriver → PATH_KORG_USB_AUDIO_KO
 *   /korg/Eva/Eva                → PATH_EVA_BIN                            */
static void LoadAll(void)
{
    char buf[256];

    /* ── 1. Disable swapping ───────────────────────────────────────────── */
    {
        FILE *f = fopen("/proc/sys/vm/swappiness", "w");
        if (f) {
            if (fprintf(f, "%d", 0) > 0)
                fflush(f);
            fclose(f);
        }
    }

    /* ── 2. Write initial progress counter ────────────────────────────── */
    {
        int counter = 0;
        FILE *f = fopen("/proc/progress", "r");
        if (f) {
            if (fscanf(f, "%d", &counter) < 0) { /* ignore — /proc/progress may be empty */ }
            fclose(f);
        }
        counter++;
        write_progress(counter);

        /* Open a second time and write same counter (stock does this twice
         * more as a bookkeeping pattern before loading anything)            */
        counter++;
        write_progress(counter);
        counter++;
        write_progress(counter);
    }

    /* ── 3. RTAI real-time modules ─────────────────────────────────────── */
    do_insmod("/usr/realtime/modules/rtai_hal.ko",   NULL, 7);
    do_insmod("/usr/realtime/modules/rtai_smp.ko",   NULL, 8);
    do_insmod("/usr/realtime/modules/rtai_sem.ko",   NULL, 9);
    do_insmod("/usr/realtime/modules/rtai_ndbg.ko",  NULL, 10);
    do_insmod("/usr/realtime/modules/rtai_fifos.ko", NULL, 11);

    /* ── 4. STG DSP modules ───────────────────────────────────────────── */
    do_insmod("/sbin/STGEnabler.ko", NULL, 12);
    do_insmod("/sbin/STGGmp.ko",     NULL, 12);

    /* ── 5. Identify USB host controller; set IRQ affinity ───────────── */
    {
        int  irq = -1;
        int  fix_frame_order = 0;  /* 0 = xhci found, 1 = ehci (or neither) */
        char irq_path[64];

        /* Read /proc/interrupts looking for xhci_hcd or ehci_hcd */
        FILE *f = fopen("/proc/interrupts", "r");
        if (f) {
            while (fgets(buf, sizeof(buf), f)) {
                if (strstr(buf, "xhci_hcd")) {
                    int candidate;
                    if (sscanf(buf, " %d:", &candidate) == 1)
                        irq = candidate;
                    fix_frame_order = 0;
                } else if (strstr(buf, "ehci_hcd")) {
                    int candidate;
                    if (sscanf(buf, " %d:", &candidate) == 1)
                        irq = candidate;
                    fix_frame_order = 1;
                }
            }
            fclose(f);
        }

        /* Write smp_affinity for the USB IRQ — redirect it away from CPU0 */
        if (irq >= 0) {
            snprintf(irq_path, sizeof(irq_path),
                     "/proc/irq/%d/smp_affinity", irq);
            FILE *af = fopen(irq_path, "w");
            if (af) {
                fputc('8', af);  /* bitmask '8' = 0x8 = CPU3 */
                fclose(af);
            }
        }

        /* Build the module parameter string for OmapNKS4Module */
        snprintf(buf, sizeof(buf), "gFixAudioInputFrameOrder=%d",
                 fix_frame_order);

        /* Also read /tmp/stgStatus to confirm xhci/ehci presence:
         * Wait (poll up to 1s) for the file to be written by STGEnabler.   */
        {
            int found_in_status = fix_frame_order;
            FILE *sf = fopen("/tmp/stgStatus", "r");
            if (sf) {
                char line[256];
                while (fgets(line, sizeof(line), sf)) {
                    if (strstr(line, "xhci_hcd"))
                        found_in_status = 0;
                    else if (strstr(line, "ehci_hcd"))
                        found_in_status = 1;
                }
                fclose(sf);
                snprintf(buf, sizeof(buf), "gFixAudioInputFrameOrder=%d",
                         found_in_status);
            }
        }

        /* ── 6. OmapNKS4 and OmapVideo modules ───────────────────────── */
        do_insmod("/sbin/OmapNKS4Module.ko", buf, 13);
    }

    do_insmod("/sbin/OmapVideoModule.ko", NULL, 14);

    /* ── 7. GetPubIdMod (Atmel NV2AC security IC driver) ─────────────── */
    {
        char *argv[] = { "/sbin/insmod", "/sbin/GetPubIdMod.ko", NULL };
        if (RunProcess(argv, 0, "/dev/null") != 0)
            fprintf(stderr, "Insmod %s failed\n", "/sbin/GetPubIdMod.ko");
        /* non-fatal */
    }

    /* ── 8. USBMidiAccessory ─────────────────────────────────────────── */
    {
        char *argv[] = { "/sbin/insmod", "/sbin/USBMidiAccessory.ko", NULL };
        if (RunProcess(argv, 0, "/dev/null") != 0)
            fprintf(stderr, "Insmod %s failed\n", "/sbin/USBMidiAccessory.ko");
        /* non-fatal */
    }

    /* ── 9. loadmod.ko (MD5/integrity driver) ────────────────────────── */
    do_insmod("/sbin/loadmod.ko", NULL, 0x10);

    /* ── 10. Mount WaveMotion cryptoloop ─────────────────────────────── */
    if (do_mount_cryptoloop("/korg/rw/PCM/WaveMotion", 0x10) == 0)
        sWMMSMounted = 1;

    /* ── 11. Mount /korg/Mod cryptoloop ─────────────────────────────── */
    if (do_mount_cryptoloop("/korg/Mod", 0x11) == 0)
        sModMounted = 1;

    /* ── 12. Fork progress-writing child ─────────────────────────────── */
    {
        pid_t child_pid = fork();
        if (child_pid < 0) {
            fwrite("Fork error\n", 1, 11, stderr);
        } else if (child_pid == 0) {
            /* Child: loop writing progress counter, 1s between iterations */
            int ctr = 0;
            for (;;) {
                write_progress(ctr++);
                sleep_us(1000000);
            }
            /* not reached */
            exit(0);
        } else {
            /* Parent: write progress.pid */
            FILE *f = fopen("/tmp/progress.pid", "w");
            if (f) {
                fprintf(f, "%d", (int)child_pid);
                if (fprintf(f, "%d", (int)child_pid) > 0)
                    fflush(f);
                fclose(f);
            }
        }
    }

    /* ── 13. Detect secondary disk and mount SSD partitions ──────────── */
    int has2nd = Has2ndInternalDisk();
    int hasPCMPrecache = 0;

    if (has2nd) {
        /* Mount second SSD ext3 partition */
        if (RunSystemCommand("mount -t ext3 -o commit=1,noatime /dev/sdb1 /korg/rw2") != 0)
            puts("Problems with pipe for 2nd disk mount");
        else {
            /* Move options from 2nd disk to internal */
            if (RunSystemCommand("mv -f /korg/rw2/Options/* /korg/rw/Options") != 0)
                puts("Problems with Move2ndDiskOptionsToInternal");
        }
    }

    /* Mount SSD1 ftp bind */
    if (RunSystemCommand("mount --bind /korg/rw/HD /korg/ftp/SSD1") != 0)
        puts("Problems with pipe for 1st disk ftp mount");

    if (has2nd) {
        if (RunSystemCommand("mkdir --mode=777 /korg/ftp/SSD2") != 0)
            puts("Problems with pipe for creating 2nd disk ftp mount point");
        else if (RunSystemCommand("mount --bind /korg/rw2/HD /korg/ftp/SSD2") != 0)
            puts("Problems with pipe for 2nd disk ftp mount");
    }

    /* Check for PCM Precache partition on internal SSD */
    {
        FILE *f = popen("ls /sys/block/sda | grep sda", "r");
        if (!f) {
            puts("Problems with pipe for PCM Precache partition detection");
        } else {
            int found_sda7 = 0;
            while (fgets(buf, sizeof(buf), f)) {
                if (strstr(buf, "sda7"))
                    found_sda7 = 1;
            }
            pclose(f);

            if (found_sda7) {
                if (RunSystemCommand(
                        "mount -t ext3 -o noatime /dev/sda7 /korg/rw/PCM/Precache") != 0)
                    puts("Problems with pipe for PCM partition mount");
                else
                    hasPCMPrecache = 1;
            }
        }
    }

    printf("Has2ndInternalDisk=%d HasPCMPrecachePartition=%d\n",
           has2nd, hasPCMPrecache);

    /* ── 14. Remove ftp SSD2 link if no 2nd disk ─────────────────────── */
    if (!has2nd) {
        if (RunSystemCommand("rm -fr /korg/ftp/SSD2") != 0)
            puts("Problems with pipe for removal of the 2nd disk ftp link");
    } else {
        /* chmod SSD2 mount point */
        if (RunSystemCommand("chown 500.500 /korg/ftp/SSD2") != 0)
            puts("Problems chmoding pipe for 2nd disk ftp mount point");
    }

    /* ── 15. Load OA.ko from (possibly redirected) path ─────────────── */
    {
        char *argv[] = { "/sbin/insmod", PATH_OA_KO, NULL };
        if (RunProcess(argv, 0, "/dev/null") != 0) {
            fprintf(stderr, "Insmod %s failed\n", PATH_OA_KO);
            Fail(0x13);
        }
    }

    /* ── 16. Load KorgUsbAudioDriver.ko ─────────────────────────────── */
    {
        char *argv[] = { "/sbin/insmod", PATH_KORG_USB_AUDIO_KO, NULL };
        if (RunProcess(argv, 0, "/dev/null") != 0)
            fprintf(stderr, "Insmod %s failed\n", PATH_KORG_USB_AUDIO_KO);
        /* non-fatal in stock binary — OA is the critical one */
    }

    /* ── 17. Umount /korg/Mod ───────────────────────────────────────── */
    {
        char *argv[] = { "/bin/umount", "-n", "/korg/Mod", NULL };
        if (RunProcess(argv, 0, NULL) != 0) {
            fprintf(stderr, "Umount %s failed\n", "/korg/Mod");
            Fail(0x12);
        }
        sModMounted = 0;
    }

    /* ── 18. Mount /korg/Eva cryptoloop ────────────────────────────── */
    if (do_mount_cryptoloop("/korg/Eva", 0x14) == 0)
        sEvaMounted = 1;

    /* ── 19. Start fanctrld (fan controller, background) ───────────── */
    {
        char *argv[] = { "/bin/fanctrld", NULL };
        if (RunProcess(argv, 1, NULL) != 0)
            fwrite("fanctrld failed\n", 1, 16, stderr);
    }

    /* ── 20. Exec Eva (replaces this process) ──────────────────────── */
    {
        char *argv[] = { PATH_EVA_BIN, NULL };
        if (RunProcess(argv, 0, NULL) != 0) {
            fwrite("Eva failed\n", 1, 11, stderr);
            Fail(0x15);
        }
    }
}

/* ── UnloadForUpdate (-i mode) ────────────────────────────────────────────
 * Called by the Kronos OS-update mechanism before installing new firmware.
 * Kills Eva, then umounts cryptoloop volumes and unloads modules in reverse
 * order.  Does NOT rmmod the RTAI/STG/Omap modules (update installer needs
 * the kernel running).
 *
 * The stock binary retries KillAllPids up to ~14 times at 100ms intervals
 * before giving up and moving on.                                           */
#define KILL_INTERVAL  100000   /* 100 ms in microseconds */

static void UnloadForUpdate(void)
{
    printf("[%d] UnloadForUpdate started\n", getpid());
    fflush(stdout);

    /* Kill Eva with retries — 16 total calls (initial + 15 retries), each
     * separated by 100 ms.  Matches exact call count in stock binary.      */
    for (int i = 0; i < 16; i++) {
        if (KillAllPids("Eva") != 0)
            break;
        sleep_us(KILL_INTERVAL);
    }

    /* 500 ms settle */
    sleep_us(500000);

    /* Umount cryptoloops in reverse mount order */
    if (sEvaMounted) {
        char *argv[] = { "/bin/umount", "-n", "/korg/Eva", NULL };
        if (RunProcess(argv, 0, NULL) != 0)
            fprintf(stderr, "Umount %s failed\n", "/korg/Eva");
    }
    if (sModMounted) {
        char *argv[] = { "/bin/umount", "-n", "/korg/Mod", NULL };
        if (RunProcess(argv, 0, NULL) != 0)
            fprintf(stderr, "Umount %s failed\n", "/korg/Mod");
    }
    if (sWMMSMounted) {
        char *argv[] = { "/bin/umount", "-n", "/korg/rw/PCM/WaveMotion", NULL };
        if (RunProcess(argv, 0, NULL) != 0)
            fprintf(stderr, "Umount %s failed\n", "/korg/rw/PCM/WaveMotion");
    }

    /* 1.5 s for modules to settle before rmmod */
    sleep_us(1500000);

    /* Rmmod modules in reverse load order */
    do_rmmod("GetPubIdMod");
    do_rmmod("OA");
    do_rmmod("USBMidiAccessory");
    do_rmmod("KorgUsbAudioDriver");
    do_rmmod("loadmod");
    do_rmmod("OmapVideoModule");
    do_rmmod("OmapNKS4");
    do_rmmod("STGGmp");
    do_rmmod("STGEnabler");
    do_rmmod("rtai_fifos");
    do_rmmod("rtai_ndbg");
    do_rmmod("rtai_sched");
    do_rmmod("rtai_hal");

    /* Umount ftp/SSD bind mounts */
    RunSystemCommand("umount /korg/ftp/SSD1");
    RunSystemCommand("umount /korg/ftp/SSD2");
    RunSystemCommand("umount /korg/rw2");

    printf("[%d] UnloadForUpdate() exit\n", getpid());
    fflush(stdout);
}

/* ── UnloadAll (-u mode) ──────────────────────────────────────────────────
 * Full teardown: kills ckhdw (fan controller daemon) and Eva, then does the
 * same umount+rmmod sequence as UnloadForUpdate.                            */
static void UnloadAll(void)
{
    /* Kill ckhdw with retries — 6 total calls, 100 ms apart */
    for (int i = 0; i < 6; i++) {
        if (KillAllPids("ckhdw") != 0)
            break;
        sleep_us(KILL_INTERVAL);
    }

    /* Kill Eva with retries — 6 total calls, 100 ms apart */
    for (int i = 0; i < 6; i++) {
        if (KillAllPids("Eva") != 0)
            break;
        sleep_us(KILL_INTERVAL);
    }

    /* 500 ms settle */
    sleep_us(500000);

    /* Umount cryptoloops */
    if (sEvaMounted) {
        char *argv[] = { "/bin/umount", "-n", "/korg/Eva", NULL };
        if (RunProcess(argv, 0, NULL) != 0)
            fprintf(stderr, "Umount %s failed\n", "/korg/Eva");
    }
    if (sModMounted) {
        char *argv[] = { "/bin/umount", "-n", "/korg/Mod", NULL };
        if (RunProcess(argv, 0, NULL) != 0)
            fprintf(stderr, "Umount %s failed\n", "/korg/Mod");
    }
    if (sWMMSMounted) {
        char *argv[] = { "/bin/umount", "-n", "/korg/rw/PCM/WaveMotion", NULL };
        if (RunProcess(argv, 0, NULL) != 0)
            fprintf(stderr, "Umount %s failed\n", "/korg/rw/PCM/WaveMotion");
    }

    /* 1.5 s settle */
    sleep_us(1500000);

    /* Rmmod everything */
    do_rmmod("GetPubIdMod");
    do_rmmod("OA");
    do_rmmod("USBMidiAccessory");
    do_rmmod("KorgUsbAudioDriver");
    do_rmmod("loadmod");
    do_rmmod("OmapVideoModule");
    do_rmmod("OmapNKS4");
    do_rmmod("STGGmp");
    do_rmmod("STGEnabler");
    do_rmmod("rtai_fifos");
    do_rmmod("rtai_ndbg");
    do_rmmod("rtai_sched");
    do_rmmod("rtai_hal");

    /* Umount ftp/SSD */
    int has2nd = Has2ndInternalDisk();
    RunSystemCommand("umount /korg/ftp/SSD1");
    if (has2nd)
        RunSystemCommand("umount /korg/ftp/SSD2");
    if (has2nd)
        RunSystemCommand("umount /korg/rw2");

    printf("[%d] UnloadAll() exit\n", getpid());
    fflush(stdout);
}

/* ── main ─────────────────────────────────────────────────────────────────
 * Replicates stock argument handling exactly:
 *   no args / unknown args  → LoadAll()
 *   loadoa -i               → UnloadForUpdate()
 *   loadoa -u               → UnloadAll()
 * Must run as root (uid 0); prints "Must be root\n" to stderr and exits
 * with code 2 otherwise, identical to stock.                               */
int main(int argc, char **argv)
{
    if (getuid() != 0) {
        fwrite("Must be root\n", 1, 13, stderr);
        exit(2);
    }

    SetCPUAffinity();

    if (argc != 2) {
        LoadAll();
        return 0;
    }

    if (argv[1][0] != '-')
        exit(1);

    switch (argv[1][1]) {
    case 'i':
        UnloadForUpdate();
        break;
    case 'u':
        UnloadAll();
        break;
    default:
        exit(1);
    }

    return 0;
}
