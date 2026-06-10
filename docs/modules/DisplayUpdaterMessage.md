# DisplayUpdaterMessage — Front-Panel Status Display

A small ELF userspace utility bundled on every Kronos update USB stick. Its job is to
render text and progress on the Kronos front-panel LCD during the update procedure
(when Eva isn't running yet — Eva is replaced by the update routine while OS files are
being written).

| Property | Value |
|---|---|
| Path on USB stick | `/mnt/updaterSource/DisplayUpdaterMessage` |
| Architecture | x86 LE 32-bit ELF executable (ET_EXEC) |

---

## Behaviour

| Aspect | Detail |
|---|---|
| Output target | OMAP framebuffer (the LCD); writes palette / text via NKS4 ioctls |
| Interactivity | None — runs once, displays the message, exits after ~600 ms |
| Special modes | `SetTextPalette`, `SetDefaultPalette` (control the LCD colour palette) |
| Progress bar | Written via `/proc/OmapNKS4ProgressBar` (pretar/posttar scripts can update the on-screen bar with `echo NN > /proc/OmapNKS4ProgressBar`) |
| Return value | Always 0 |

---

## Kronosology replacement binary

We ship our own replacement in [`../../auto-auth/updater_msg/`](../../auto-auth/updater_msg/).
It is a clean-room reimplementation that is **drop-in compatible** with the Korg original
while adding smooth FreeType anti-aliasing, a padlock logo, and a "kronosology :: auto-auth"
header above each message.

| Property | Korg original | Our replacement |
|---|---|---|
| Size | ~40 KB (not stripped) | ~18 KB (stripped) |
| Compiler | GCC 4.5.0 | GCC (modern, i386 multilib) |
| GLIBC requirement | GLIBC_2.0, 2.1, 2.1.3 | **GLIBC_2.0 only** |
| PIE | No | No (`-no-pie`) |
| Text rendering | 2-level (Korg red, bitmap) | 9-level FreeType anti-aliasing |
| Extra features | — | Padlock icon, kronosology header, orange accent bar |

### Compatibility fixes (vs. naïve modern build)

A naïve `gcc -m32` build produces an incompatible binary on a modern host:

| Problem | Root cause | Fix |
|---|---|---|
| `__stack_chk_fail@GLIBC_2.4` | Stack protector enabled by default | `-fno-stack-protector` |
| `__fprintf_chk@GLIBC_2.34` | `_FORTIFY_SOURCE` enabled by default | `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0` |
| `__libc_start_main@GLIBC_2.34` | Modern glibc bumped the versioned symbol | `--wrap=__libc_start_main` + `glibc_compat.c` shim |
| PIE binary | Modern GCC defaults to `-pie` | `-no-pie -fno-pie` |
| Missing `OMAPNKS4_XAXIS_BYTES` ioctl | Not called in our code | Added `ioctl(fd, 0x40047206, &width)` after `mmap()` |

All five are addressed in the Makefile and `glibc_compat.c`.

### OmapNKS4 ioctl numbers

| Constant | Value | Description |
|---|---|---|
| `OMAPNKS4_SET_PAL` | `0x40047209` | Set one palette entry (`{index, r, g, b}`) |
| `OMAPNKS4_FILL_RECT` | `0x40107208` | Hardware-accelerated fill rect (16-byte struct) |
| `OMAPNKS4_GET_VERSION` | `0x4004720e` | Returns 0 (original Kronos) or 1 (KRONOS 2) |
| `OMAPNKS4_XAXIS_BYTES` | `0x40047206` | Set scanline stride = screen width in bytes |

### Building

```bash
# Requires: gcc-multilib  libfreetype6-dev:i386
cd auto-auth/updater_msg
make
# → DisplayUpdaterMessage  (i386 ELF, GLIBC_2.0 only, ~18 KB stripped)
```

---

## Usage in custom update scripts

If you write your own `pretar.sh` / `posttar.sh` to be wrapped with `update_builder.py`,
call `DisplayUpdaterMessage` to update the UI:

```sh
# in pretar.sh (USB is mounted at /mnt/updaterSource/)
DUM=/mnt/updaterSource/DisplayUpdaterMessage
[ -x "$DUM" ] || DUM=true          # graceful fallback if binary is absent

"$DUM" "Installing custom firmware..."
echo 25 > /proc/OmapNKS4ProgressBar
```

You **must** include the binary on your USB stick — it is not installed on the device
by default.  Both `build_auto_auth.py` and `build_update.sh` copy it automatically if
it has been built.
