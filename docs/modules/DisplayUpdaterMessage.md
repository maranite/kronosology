# DisplayUpdaterMessage — Front-Panel Status Display

A small ELF userspace utility bundled on every Kronos update USB stick. Its job is to
render text and progress on the Kronos front-panel LCD during the update procedure
(when Eva isn't running yet — Eva is replaced by the update routine while OS files are
being written).

| Property | Value |
|---|---|
| Path on USB stick (when present) | `/mnt/updaterSource/DisplayUpdaterMessage` |
| Architecture | x86 LE 32-bit ELF executable (ET_EXEC) |
| Compiler | GCC 4.5.0 |
| Status | studied in prior session — see `kronos_system.md` and any DisplayUpdaterMessage references in earlier docs |

---

## Behaviour

| Aspect | Detail |
|---|---|
| Output target | OMAP framebuffer (the LCD); writes specific palette / text via the same NKS4 path used by the rest of the OS |
| Interactivity | None — runs once, displays the message, exits after ~600 ms |
| Special modes | `SetTextPalette`, `SetDefaultPalette` (control the LCD colour palette) |
| Progress bar | Written via `/proc/OmapNKS4ProgressBar` (so pretar/posttar scripts can update the on-screen bar with `echo NN > /proc/OmapNKS4ProgressBar`) |
| Return value | Always 0 |

---

## Usage in custom update scripts

If you write your own `pretar.sh` / `posttar.sh` to be wrapped with `update_builder.py`,
you can call `DisplayUpdaterMessage` to update the UI:

```sh
# in pretar.sh (mounted USB is at /mnt/updaterSource/)
/mnt/updaterSource/DisplayUpdaterMessage "Installing custom firmware..."
echo 25 > /proc/OmapNKS4ProgressBar
```

You **must** include the binary on your USB stick (it isn't installed on the device by
default — it lives only on update media).

---

## Why this matters

For a useful, non-locking update procedure (one that displays "doing thing X..." rather
than a blank screen), bundling `DisplayUpdaterMessage` is necessary. The original
`KRONOS_Updater_3_2_1` USB stick provides a known-good copy that can simply be reused.

---

## Status

Documented to the depth this project requires. Not part of the deep analysis campaign
(it's a userspace tool, not in the security path).
