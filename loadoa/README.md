# loadoa — Drop-in Replacement for Korg Kronos `/sbin/loadoa`

A clean C reimplementation of the Kronos boot-loader binary `/sbin/loadoa`,
reverse-engineered from the stock 3.2.1/3.2.2 binary
(MD5 `8a3d61f3332d7bcf694e8c05845b4754` — identical across both versions).

The replacement replicates every observable behaviour of the stock binary.
Its only functional difference from stock is the three path redirections
that the `offline-patcher` normally applies by binary-patching the string table:

| Stock path | Redirected to |
|---|---|
| `/korg/Mod/OA.ko` | `/sbin/OA.ko` |
| `/korg/Mod/KorgUsbAudioDriver.ko` | `/sbin/KorgUsbAudioDriver.ko` |
| `/korg/Eva/Eva` | `/sbin/Eva` |

Building a replacement binary instead of patching the stock one has several
advantages:
- No need to keep loadoa in sync across firmware versions (its MD5 never changes).
- The three path strings are compile-time constants, not fragile binary offsets.
- Easier to audit and extend.

---

## Prerequisites

The Kronos runs 32-bit i386 ELF binaries on its OA SBC board. Build requires
a 32-bit C compiler:

```bash
# Debian / Ubuntu
sudo apt install gcc-multilib

# Fedora / RHEL
sudo dnf install glibc-devel.i686 libgcc.i686
```

---

## Build

```bash
# Patched paths (default — what the offline-patcher deploys)
make

# Stock-equivalent paths (for testing on a rooted Kronos without patching)
make PATCH_PATHS=0

# Verify it is a 32-bit ELF
file loadoa
# → loadoa: ELF 32-bit LSB executable, Intel 80386, ...
```

---

## What loadoa does

`loadoa` is the first userspace binary exec'd after the Kronos kernel boots.
It runs as root from the init scripts and:

1. Pins itself to CPU 0 via `sched_setaffinity`.
2. Disables swapping (`/proc/sys/vm/swappiness = 0`).
3. Insmod's the RTAI real-time kernel (`rtai_hal`, `rtai_smp/sem/ndbg/fifos`).
4. Insmod's the Korg DSP glue modules (`STGEnabler.ko`, `STGGmp.ko`).
5. Reads `/proc/interrupts` to find the USB host controller IRQ and redirects
   it off CPU 0 via `/proc/irq/<N>/smp_affinity`.  Passes
   `gFixAudioInputFrameOrder=<0|1>` to `OmapNKS4Module.ko`.
6. Insmod's `OmapNKS4Module.ko` (front-panel chip), `OmapVideoModule.ko`,
   `GetPubIdMod.ko` (Atmel NV2AC security IC driver), `USBMidiAccessory.ko`,
   `loadmod.ko` (MD5 integrity driver).
7. Mounts the WaveMotion and `/korg/Mod` cryptoloop images.
8. Forks a progress-counter child, detects secondary SSD, mounts SSD ftp
   bind-mounts, and optionally mounts the PCM Precache partition.
9. Insmod's `OA.ko` and `KorgUsbAudioDriver.ko` from the mounted Mod image
   **(redirected to `/sbin/` in the patched build)**.
10. Umounts `/korg/Mod`, mounts `/korg/Eva`.
11. Starts `fanctrld` in the background.
12. `execvp`s `Eva` **(redirected to `/sbin/Eva` in the patched build)**.

### Additional modes

| Invocation | Effect |
|---|---|
| `loadoa -i` | **UnloadForUpdate** — kills Eva, umounts cryptoloops; called by the OS-update mechanism before installing firmware |
| `loadoa -u` | **UnloadAll** — kills `ckhdw` + Eva, then full rmmod + umount teardown |

---

## Behaviour notes from RE

- `KillAllPids(name)` runs `/sbin/pidof <name>` to `/tmp/pids`, then sends
  SIGINT+SIGURG → 300 ms → SIGTERM+SIGURG → 300 ms → SIGKILL to each PID.
- `RunProcess` forks, optionally redirects stdout/stderr to a file, checks the
  setuid bit on the target binary, then `execvp`s.  Parent waits unless
  `background=1`.
- The `-i` and `-u` modes retry `KillAllPids` up to 14 / 7+6 times at 100 ms
  intervals before proceeding to the umount sequence.
- Fail codes 7–21 correspond to the ordered init steps; higher codes mean
  more mounts are active and more cleanup is needed.

---

## Integration with offline-patcher

`offline-patcher/patch_firmware_offline.py` patches the **stock** loadoa
binary using string replacement (three fixed-length in-place substitutions).
This `loadoa/` source is the alternative: build it once and drop the resulting
binary directly into the tar payload as `sbin/loadoa`, bypassing the stock
binary and its patcher-specific patch sites entirely.

To use in the patcher, set `LOADOA_BIN` in `patch_firmware_offline.py` to
point to the pre-built binary from this directory.
