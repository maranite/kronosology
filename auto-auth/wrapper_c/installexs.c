/*
 * InstallEXs — C replacement for the shell wrapper around InstallEXs.real
 *
 * Compiled as a no-stdlib i386 ELF using only raw Linux syscalls so it
 * needs only the kernel, not glibc.  Behaviour is identical to the shell
 * script it replaces:
 *
 *   1. exec InstallEXs.real with every argument unchanged
 *   2. If the install succeeded, derive the option-file ID (Sxxx)
 *   3. Write "GEN:Sxxx" to /proc/.oaauth  (oa_authgen.ko)
 *   4. Read back the 24-char auth string
 *   5. Write "AU:<string>" to /proc/.oacmd  (stock OA.ko)
 *
 * Setuid-safe: preserves the setuid bits that the stock binary carries;
 * the fork/exec approach means InstallEXs.real runs with the same
 * effective UID it would have if called directly.
 *
 * Build:
 *   gcc -std=gnu99 -m32 -fno-pic -fno-PIE -nostdlib -fno-builtin \
 *       -Os -o InstallEXs installexs.c && strip InstallEXs
 */

/* ── raw Linux/i386 syscall numbers ─────────────────────────────────── */
#define SYS_exit        1
#define SYS_fork        2
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_waitpid     7
#define SYS_execve      11

#define O_RDONLY        0
#define O_WRONLY        1

typedef unsigned long  ulong;

/* ── minimal syscall stubs ──────────────────────────────────────────── */
static inline long syscall1(long nr, long a1)
{
    long r;
    __asm__ __volatile__("int $0x80"
        : "=a"(r) : "0"(nr), "b"(a1) : "memory", "cc");
    return r;
}
static inline long syscall2(long nr, long a1, long a2)
{
    long r;
    __asm__ __volatile__("int $0x80"
        : "=a"(r) : "0"(nr), "b"(a1), "c"(a2) : "memory", "cc");
    return r;
}
static inline long syscall3(long nr, long a1, long a2, long a3)
{
    long r;
    __asm__ __volatile__("int $0x80"
        : "=a"(r) : "0"(nr), "b"(a1), "c"(a2), "d"(a3) : "memory", "cc");
    return r;
}

static void do_exit(int code)
{
    syscall1(SYS_exit, code);
    __builtin_unreachable();
}
static long do_write(int fd, const void *buf, ulong n)
{
    return syscall3(SYS_write, fd, (long)buf, (long)n);
}
static long do_read(int fd, void *buf, ulong n)
{
    return syscall3(SYS_read, fd, (long)buf, (long)n);
}
static long do_open(const char *path, int flags)
{
    return syscall3(SYS_open, (long)path, flags, 0644L);
}
static long do_close(int fd)   { return syscall1(SYS_close, fd); }
static long do_fork(void)      { return syscall1(SYS_fork, 0); }
static long do_execve(const char *p, char *const av[], char *const ev[])
{
    return syscall3(SYS_execve, (long)p, (long)av, (long)ev);
}
static long do_waitpid(int pid, int *st, int fl)
{
    return syscall3(SYS_waitpid, pid, (long)st, fl);
}

/* ── minimal string helpers ─────────────────────────────────────────── */
static int sx_strlen(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int sx_strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static const char *sx_strrchr(const char *s, char c)
{
    const char *last = (void *)0;
    while (*s) { if (*s == c) last = s; s++; }
    if (c == '\0') return s;
    return last;
}

static void write_str(int fd, const char *s)
{
    int n = sx_strlen(s);
    if (n > 0) do_write(fd, s, (ulong)n);
}

static void log_err(const char *msg)
{
    write_str(2, "InstallEXs: ");
    write_str(2, msg);
    write_str(2, "\n");
}

static int file_exists(const char *path)
{
    long fd = do_open(path, O_RDONLY);
    if (fd < 0) return 0;
    do_close((int)fd);
    return 1;
}

/* Build path OPTIONS_DIR "/" id into buf (must be >=64 bytes) */
#define OPTIONS_DIR  "/korg/rw/Options"

static void build_opt_path(char *buf, const char *id)
{
    int i = 0, j;
    for (j = 0; OPTIONS_DIR[j]; j++) buf[i++] = OPTIONS_DIR[j];
    buf[i++] = '/';
    for (j = 0; id[j]; j++) buf[i++] = id[j];
    buf[i] = '\0';
}

/*
 * Derive option-file ID from the -f argument.
 * e.g. "/path/EXs285.exsins"  →  "S285"  (or zero-padded "S085")
 * Returns pointer into static storage, or NULL on failure.
 */
static char g_opt_buf[16];

static const char *derive_opt_id(const char *f_val)
{
    char base[64];
    const char *slash;
    const char *p;
    int blen, i, raw, d;
    char path[80];

    /* basename */
    slash = sx_strrchr(f_val, '/');
    p = slash ? slash + 1 : f_val;

    /* copy to mutable buffer */
    for (blen = 0; p[blen] && blen < 63; blen++) base[blen] = p[blen];
    base[blen] = '\0';

    /* strip .exsins suffix (case-insensitive) */
    {
        static const char suf[] = ".exsins";
        int slen = 7;
        if (blen > slen) {
            int ok = 1, k;
            for (k = 0; k < slen; k++) {
                char c = base[blen - slen + k];
                char s = suf[k];
                if (c >= 'A' && c <= 'Z') c += 32;
                if (c != s) { ok = 0; break; }
            }
            if (ok) { blen -= slen; base[blen] = '\0'; }
        }
    }

    /* skip leading "EXs" / "exs" etc. */
    p = base;
    if ((p[0]=='E'||p[0]=='e') && (p[1]=='X'||p[1]=='x') && (p[2]=='S'||p[2]=='s'))
        p += 3;

    /* parse decimal number */
    if (!*p) return (void *)0;
    raw = 0;
    for (i = 0; p[i] >= '0' && p[i] <= '9'; i++)
        raw = raw * 10 + (p[i] - '0');
    if (i == 0) return (void *)0;

    /* Build "Sxxx" */
    g_opt_buf[0] = 'S';
    {
        char tmp[10]; int ti = 0, tj, v = raw;
        if (v == 0) { tmp[ti++] = '0'; }
        else { while (v > 0) { tmp[ti++] = '0' + v % 10; v /= 10; } }
        for (tj = 0; tj < ti; tj++) g_opt_buf[1 + tj] = tmp[ti - 1 - tj];
        g_opt_buf[1 + ti] = '\0';
    }
    build_opt_path(path, g_opt_buf);
    if (file_exists(path)) return g_opt_buf;

    /* Try zero-padded "S%03d" */
    g_opt_buf[0] = 'S';
    d = raw / 100; g_opt_buf[1] = '0' + (char)d;
    d = (raw / 10) % 10; g_opt_buf[2] = '0' + (char)d;
    d = raw % 10; g_opt_buf[3] = '0' + (char)d;
    g_opt_buf[4] = '\0';
    build_opt_path(path, g_opt_buf);
    if (file_exists(path)) return g_opt_buf;

    return (void *)0;
}

static int read_all(int fd, char *buf, int max)
{
    int total = 0, n;
    while (total < max - 1) {
        n = (int)do_read(fd, buf + total, (ulong)(max - 1 - total));
        if (n <= 0) break;
        total += n;
    }
    buf[total] = '\0';
    while (total > 0 &&
           (buf[total-1] == '\n' || buf[total-1] == '\r' ||
            buf[total-1] == ' '  || buf[total-1] == '\t'))
        buf[--total] = '\0';
    return total;
}

/* ── program entry point ────────────────────────────────────────────── */

#define REAL    "/sbin/InstallEXs.real"
#define OAAUTH  "/proc/.oaauth"
#define OACMD   "/proc/.oacmd"

/*
 * The Linux kernel pushes the initial stack as:
 *   [esp+0]  = argc  (integer)
 *   [esp+4]  = argv[0], argv[1], ..., argv[argc-1], NULL
 *   [esp+4+(argc+1)*4] = envp[0], ...
 *
 * This is NOT a standard C call — there is no return address. We use a
 * naked _start to read the values off the stack and call real_main.
 */
static __attribute__((used)) void real_main(int argc, char **argv, char **envp);

__attribute__((naked, noreturn))
void _start(void)
{
    __asm__(
        "xorl  %%ebp, %%ebp\n\t"          /* clear frame pointer (ABI) */
        "popl  %%eax\n\t"                  /* eax = argc; esp -> argv[0] */
        "movl  %%esp, %%ecx\n\t"          /* ecx = argv */
        "leal  4(%%esp,%%eax,4), %%edx\n\t" /* edx = envp */
        "pushl %%edx\n\t"
        "pushl %%ecx\n\t"
        "pushl %%eax\n\t"
        "call  real_main\n\t"
        : : : "eax", "ecx", "edx"
    );
}

static void real_main(int argc, char **argv, char **envp)
{
    const char *f_val = (void *)0;
    int skip_auth = 0;
    int i;

    /* ── 0. Ensure oa_authgen.ko is loaded ──
     * Check for /proc/.oaauth; if absent the module isn't loaded.
     * Fork /bin/sh to insmod it with the OA.ko symbol addresses from
     * /proc/kallsyms — same logic as the boot-time OA.rc snippet. */
    {
        long fd = do_open(OAAUTH, O_RDONLY);
        if (fd < 0) {
            /* /proc/.oaauth absent — attempt to load the module */
            static const char load_cmd[] =
                "[ -f /sbin/oa_authgen.ko ] || exit 0;"
                " S=$(grep ' SetupAtmelForAuthorizations' /proc/kallsyms 2>/dev/null"
                "  | cut -d' ' -f1);"
                " R=$(grep ' fFfFfFfFfFfF13' /proc/kallsyms 2>/dev/null"
                "  | grep '\\[OA\\]' | cut -d' ' -f1);"
                " if [ -n \"$S\" ] && [ -n \"$R\" ];"
                " then /sbin/insmod /sbin/oa_authgen.ko"
                "  setup_atmel_addr=0x${S} chip_read_addr=0x${R};"
                " else /sbin/insmod /sbin/oa_authgen.ko;"
                " fi";
            static const char sh_path[] = "/bin/sh";
            char *const sh_av[] = {
                (char *)sh_path, (char *)"-c", (char *)load_cmd, (void *)0
            };
            {
                long pid2 = do_fork();
                if (pid2 == 0) {
                    do_execve(sh_path, sh_av, envp);
                    do_exit(127);
                }
                if (pid2 > 0) {
                    int st2 = 0; long r2;
                    do { r2 = do_waitpid((int)pid2, &st2, 0); } while (r2 == -4L);
                }
            }
        } else {
            do_close((int)fd);
        }
    }

    /* ── 1. Scan argv for mode flags and -f value ── */
    for (i = 1; i < argc; i++) {
        if (sx_strcmp(argv[i], "-v") == 0 ||
            sx_strcmp(argv[i], "-r") == 0 ||
            sx_strcmp(argv[i], "-u") == 0)
            skip_auth = 1;
        if (sx_strcmp(argv[i], "-f") == 0 && i + 1 < argc)
            f_val = argv[i + 1];
    }

    /* ── 2. Fork + exec InstallEXs.real ── */
    {
        long pid = do_fork();
        if (pid < 0) { log_err("fork failed"); do_exit(1); }

        if (pid == 0) {
            /* replace argv[0] with the real binary path, keep the rest */
            argv[0] = (char *)REAL;
            do_execve(REAL, argv, envp);
            log_err("execve " REAL " failed");
            do_exit(127);
        }

        {
            int status = 0;
            long ret;
            do { ret = do_waitpid((int)pid, &status, 0); } while (ret == -4L);
            if (ret < 0) { log_err("waitpid failed"); do_exit(1); }
            {
                int exited    = ((status & 0x7f) == 0);
                int exit_code = exited ? ((status >> 8) & 0xff) : 1;
                if (!exited || exit_code != 0) do_exit(exit_code);
            }
        }
    }

    /* ── 3. Skip auth for verify / remove / uninstall ── */
    if (skip_auth) do_exit(0);

    /* ── 4. Derive option file ID ── */
    {
        const char *opt_id = (void *)0;
        char gen_cmd[32];
        char au_cmd[40];
        char authstr[32];
        int  alen, j;
        long fd;

        if (f_val) opt_id = derive_opt_id(f_val);
        if (!opt_id) {
            log_err("could not determine option file — skipping auth");
            do_exit(0);
        }

        /* ── 5. GEN:Sxxx → /proc/.oaauth ── */
        fd = do_open(OAAUTH, O_WRONLY);
        if (fd < 0) {
            log_err(OAAUTH " not writable — is oa_authgen.ko loaded?");
            do_exit(0);
        }
        gen_cmd[0]='G'; gen_cmd[1]='E'; gen_cmd[2]='N'; gen_cmd[3]=':';
        for (j = 0; opt_id[j]; j++) gen_cmd[4 + j] = opt_id[j];
        do_write((int)fd, gen_cmd, (ulong)(4 + j));
        do_close((int)fd);

        /* ── 6. Read auth string ← /proc/.oaauth ── */
        fd = do_open(OAAUTH, O_RDONLY);
        if (fd < 0) { log_err("could not read " OAAUTH); do_exit(0); }
        alen = read_all((int)fd, authstr, (int)sizeof(authstr));
        do_close((int)fd);
        if (alen == 0) { log_err("GEN returned empty string"); do_exit(0); }

        /* ── 7. AU:<authstr> → /proc/.oacmd ── */
        fd = do_open(OACMD, O_WRONLY);
        if (fd < 0) {
            log_err(OACMD " not writable — is OA.ko loaded?");
            do_exit(0);
        }
        au_cmd[0]='A'; au_cmd[1]='U'; au_cmd[2]=':';
        for (j = 0; authstr[j]; j++) au_cmd[3 + j] = authstr[j];
        do_write((int)fd, au_cmd, (ulong)(3 + j));
        do_close((int)fd);

        write_str(2, "InstallEXs: submitted auth string for ");
        write_str(2, opt_id);
        write_str(2, "\n");
    }

    do_exit(0);
}
