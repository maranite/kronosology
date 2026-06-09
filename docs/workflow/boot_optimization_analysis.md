# Boot-time optimisation analysis

A first-principles look at where the Kronos spends its ~26 seconds from kernel-ready to
`Eva` spawning, plus another ~10–20 seconds inside Eva before the synth is playable.
Aimed at answering: *what's worth optimising, and what isn't?*

---

## Measured baseline (Kronos OS 3.2.1 with our patches applied)

From a live boot, sampled 2026-05-26 against the running Kronos:

| Milestone | Time after kernel-ready |
|---|---|
| insmod loadmod completes (`/tmp/stgStatus` written = 0) | ~20 s |
| `Eva` process exec'd | ~26 s |
| `[…] starting the synth now…!` printed (Eva's `CSTGEngine::Initialize` finishes) | ~+10–20 s further |

So roughly **20 s in loadoa**, **~5 s in Eva startup before the synth is loadable**,
**~10–20 s in Eva initialising the synth engine** (loading PRELOAD banks, audio engine,
KARMA, etc.).

> Note: precise sub-second timing is hard to extract here. The kernel disables most
> printk timestamps, and the few embedded `[…]` timestamps in OmapNKS4 / "starting the
> synth" lines are RTAI nanosecond ticks since RTAI start, not since kernel boot.
> Sub-second numbers below are estimates from the polling-loop counts in dmesg and
> from CPU load characteristics; absolute breakdowns to ±100 ms would require a
> printk-time-enabled kernel boot.

---

## Where loadoa spends its 20 s

loadoa is a single-threaded ELF executable that fork+execs `/sbin/insmod` and `/bin/mount`
sequentially. Its program flow is, in order:

1. `insmod /usr/realtime/modules/rtai_hal.ko` (+ smp/sched/sem/ndbg/fifos)
2. `insmod /sbin/STGEnabler.ko` + `/sbin/STGGmp.ko`
3. `insmod /sbin/OmapNKS4Module.ko`
   - **Probe waits for USB device to enumerate** — polling loop `OmapNKS4Init: line 1418: Waited 385 cycles for OmapNKS4Probe()`. ~385 ms typical.
   - Then `ReadPortConfiguration`, `Configure` — multiple round-trips to the chip. ~200–500 ms.
4. `insmod /sbin/OmapVideoModule.ko`
5. `insmod /sbin/GetPubIdMod.ko` — reads chip Public ID via OmapNKS4. ~100 ms.
6. `insmod /sbin/loadmod.ko` — runs all 4 security checks (MD5 of 446 files, fake-cdrom
   handshake, pairFact decrypt+verify, Atmel chip RSA challenge–response), then installs
   the six syscall hooks. Conservatively **1.5–3 s**.
7. `mount /korg/ro/Mod.img /korg/Mod` (cryptoloop, intercepted by loadmod's `sys_mount` hook,
   which calls `MountLoopDevAndExec` to ioctl LOOP_CHANGE_PASS with the key). ~100 ms.
8. `insmod /korg/Mod/USBMidiAccessory.ko` (the V2 variant from cryptoloop)
9. `insmod /korg/Mod/OA.ko` (14 MB module → kernel load + relocations ~0.5–1 s)
10. `insmod /korg/Mod/KorgUsbAudioDriver.ko`
11. `umount /korg/Mod`
12. `mount /korg/ro/Eva.img /korg/Eva` (cryptoloop, 130 MB image)
13. `mount /korg/ro/WaveMotion.img /korg/rw/PCM/WaveMotion` (cryptoloop, 248 MB)
14. Various bind mounts and admin commands (`mount --bind /korg/rw/HD /korg/ftp/SSD1`, etc.)
15. `exec /korg/Eva/Eva` (Eva takes over)

Add-up of expected costs (very rough, assuming a healthy chip):

| Phase | Estimated cost |
|---|---|
| rtai_* loads (5 modules) | ~0.5 s |
| STG* (2 modules) | ~0.1 s |
| OmapNKS4Module insmod + USB probe wait + Configure | ~1 s |
| OmapVideoModule, GetPubIdMod | ~0.3 s |
| **loadmod (security chain)** | **~1.5–3 s** |
| Cryptoloop mount of /korg/Mod | ~0.1 s |
| USBMidiAccessory, OA, KorgUsbAudioDriver | ~1–2 s (OA is 14 MB) |
| Cryptoloop mounts of /korg/Eva and WaveMotion (large images) | ~0.3 s |
| **Total loadoa overhead** | **~5–8 s** of CPU/I/O |
| (Remaining ~12–15 s is fork/wait latency + kernel module-loader serialisation) | |

---

## Where Eva spends its ~10–20 s

Out of scope for this analysis. The bulk is `CSTGEngine::Initialize` loading PRELOAD
banks (programs/combis/drum kits/wave sequences/effect presets), KARMA data, and
warming up the audio driver. None of this is touched by loadoa or loadmod and we
haven't studied Eva's startup well enough to suggest optimisations there.

The headline: **optimising loadoa can save at most ~5–8 seconds. The Eva-internal
startup is the real cost**.

---

## Option A — pre-decrypt the cryptoloops and scrap loadmod entirely

**Idea:** decrypt `/korg/ro/{Mod,Eva,WaveMotion}.img` once at "patch time" into a
plaintext copy on `/korg/rw/decrypted/`, then change `loadoa` to plain-mount those
directories rather than cryptoloop-mount the encrypted images. With cryptoloop gone,
`loadmod`'s syscall hooks are no longer needed; `loadmod` itself can be removed from
the boot path.

**Estimated savings:**
- ~1.5–3 s removed from loadmod's init
- ~100–300 ms removed across the three cryptoloop mounts
- The kernel mount intercept stops adding per-mount overhead for *every* mount made by
  Eva later (Eva does many bind mounts; each was paying ~10–50 µs for the hook to
  inspect and pass through)
- **Total: ~2–4 s saved on boot.** Plus a small steady-state win on every mount.

**Costs and constraints:**

| Item | Notes |
|---|---|
| Disk space | 17 + 130 + 247 = **~395 MB** of plaintext data needed somewhere. `/korg/ro` is only 141 MB free, but `/korg/rw` has 16 GB free, so put it under `/korg/rw/decrypted/`. |
| Decryption tool | The cryptoloop key is derived inside `loadmod` from `pairFact` + the Atmel chip's secret. We'd need to either (a) extract the key once via a kernel hook + dump (then anyone with the dump can decrypt — security regression), or (b) decrypt-in-place by booting once normally, copying out the now-plaintext contents from `/korg/Mod`, `/korg/Eva`, `/korg/rw/PCM/WaveMotion` to plaintext destinations. (b) is the practical approach. |
| `loadmod`'s OTHER side-effects | loadmod doesn't ONLY do cryptoloop. It also: (1) installs the magic value `0x22FB39CC` at `g_pCdromDrvInfo+5` that OA checks at startup (audio-degradation gate), (2) installs hooks for `sys_create_module`/`sys_init_module`/`sys_ioctl`/`sys_umount`. Most of those hooks are no-ops in practice (per the kronoshacker blog), but the degradation gate is real. |
| Magic-value gate | Our patched `OA.ko` already includes "Patch 1" (the 1-byte `JE→JMP` at OA.ko file `0xBC46`) which bypasses the degradation cascade. So with the *patched* OA we ship, the magic value is **not required**, and `loadmod` is not required for that purpose either. |
| Eva's expectation of cryptoloop mounts | Eva expects `/korg/Eva`, `/korg/Mod`, `/korg/rw/PCM/WaveMotion` to be valid mount points containing the expected files. Plain-mounting a decrypted directory at those paths satisfies that. |

**Net assessment:** technically viable, **~3 s saved on boot**. The cost is a 395 MB
disk-space hit on `/korg/rw` (acceptable on the 109 GB internal SSD) and a one-time
"decrypt now and shelve the originals" step. The plaintext files would need to be
treated as the new authoritative source (Korg OS updates that replace `Mod.img` would
need to be re-decrypted). The "decrypt" step itself uses the existing cryptoloop —
boot normally, copy from `/korg/Mod` and `/korg/Eva` to `/korg/rw/decrypted/`, edit
`loadoa` and `OA.rc` to use plain bind-mounts.

This is **worth doing** if 3 s matters and you're willing to manage the decrypted-image
state across updates. It's not a huge win.

---

## Option B — parallel insmod from loadoa

**Idea:** have loadoa fork multiple `insmod` processes in parallel for the modules that
don't depend on each other.

**Dependency analysis** (from `/proc/modules`' user lists on a running system):

```
rtai_hal ─┬─ rtai_sched ─┬─ rtai_sem
          │              ├─ rtai_fifos
          │              └─ rtai_ndbg
STGEnabler  (independent of rtai)
STGGmp      (independent of rtai)
OmapNKS4Module  (independent — USB probe is async)
  ├─ OmapVideoModule
  ├─ GetPubIdMod
  └─ loadmod              (uses stgNV2AC_sync_* ksyms from OmapNKS4)
USBMidiAccessory  (uses STGEnabler)
KorgUsbAudioDriver  (uses ehci_hcd from kernel; no inter-module deps)
OA  (uses USBMidi, KorgUsbAudio, STGGmp, OmapNKS4, all rtai_*)
```

So the maximally-parallel boot would be:

```
Phase 1 (parallel):   rtai_hal | STGEnabler | STGGmp | OmapNKS4Module
Phase 2 (parallel):   rtai_sched (waits for rtai_hal)
                      OmapVideoModule | GetPubIdMod | loadmod  (all wait for OmapNKS4)
Phase 3 (parallel):   rtai_sem | rtai_fifos | rtai_ndbg  (after rtai_sched)
Phase 4 (mount):      cryptoloop /korg/Mod  (waits for loadmod)
Phase 5 (parallel):   USBMidiAccessory | KorgUsbAudioDriver  (after Mod mount)
Phase 6:              OA  (waits for ALL prior)
Phase 7 (parallel):   cryptoloop /korg/Eva | cryptoloop /korg/rw/PCM/WaveMotion
Phase 8:              exec Eva
```

**Estimated savings:**

The kernel module loader is **serialised on a global `module_mutex`** in 2.6.32. Two
`insmod` processes calling `sys_init_module` concurrently will run their kernel-side
loading work one at a time — they can't actually load in parallel inside the kernel.

What parallelism *does* save:
- Userspace `insmod` work (read file, parse ELF, mmap, kallsyms lookup) per process
- The fork/wait round-trip latency in loadoa
- `OmapNKS4Module`'s 385-ms USB-enumeration wait — if other modules load in parallel
  during that wait, they hide behind it

Realistic estimate: **~1–2 s saved.** Not transformative because the kernel's
module-loader serialisation is the limiting factor and OmapNKS4's wait is the longest
single blocking item — that wait can only be hidden if other modules genuinely load
in parallel while OmapNKS4Init blocks, which the kernel mostly prevents.

**Cost:** loadoa would need a non-trivial rewrite. It's currently a simple
shell-script-style binary; parallel orchestration with dependency tracking is
substantial work for a 1-2 s win.

**Better focused alternative:** **make `OmapNKS4Init` non-blocking.** It currently
polls for USB enumeration via `wait_for_completion_timeout`. If we patched it to
return 0 immediately after registering the USB driver (and let the chip configure
asynchronously when the device enumerates), we'd recover ~400 ms with no risk of
breaking the dependency graph. This is a **smaller and more profitable patch** than
parallel insmod.

---

## Option C — kill the polling waits inside OmapNKS4Init

`OmapNKS4Init` has two synchronous waits: the "wait for Probe()" loop (`Waited N
cycles`) and the post-probe "Configure" sequence (`ReadPortConfiguration`, the various
`stgNV2AC_sync_cmd` round-trips to read switch positions, hardware version, OMAP
versions). Together: ~500–900 ms.

The Probe wait can plausibly be removed if `OmapNKS4Init` is restructured to do
its post-probe `Configure` from the probe callback itself (which is exactly what
`drivers/usb/...` typically does in mainline drivers). The Configure round-trips
to the chip are protocol overhead and can't easily be batched without breaking the
hardware contract.

**Estimated savings: ~400 ms.** Easy to test (the patch would be inside
`OmapNKS4Init`, ELF surgery similar to `tools/patch_omapnks4_cleanup.py`).

---

## Option D — parallelise the cryptoloop mounts of Eva and WaveMotion

After `OA.ko` insmod, `loadoa` mounts `/korg/Eva` and `/korg/rw/PCM/WaveMotion`
sequentially. These mounts are entirely independent and the cryptoloop ioctl path
doesn't share state between them. Backgrounding the WaveMotion mount and waiting for
it just before `exec Eva` would overlap its setup (~150 ms) with Eva's startup.

**Estimated savings: ~150 ms.** Trivial change to loadoa (or even to a wrapper script
that replaces /sbin/loadoa with a parallel-mounting shell wrapper).

---

## Option E — strip 446-file MD5 to a checksum-of-checksums

`loadmod`'s `VerifyCodeIntegrityMd5` opens 446 files (most under `/lib/` and
`/sbin/`), MD5s each one, and compares 16 bytes against a baked-in value. The
file-open and read time for 446 small files dominates the ~1.5 s loadmod budget.

If we keep loadmod (don't go Option A), we could patch this function to skip the
MD5 (we already patch out the result-check, but not the computation; the
computation still runs and takes ~1 s).

**Estimated savings: ~1 s on `loadmod` init.** Cheap patch (NOP out the
`DecryptAndFeedPathToMd5` call, then jump straight to the comparison's success
path; or just inline-return-0 from `VerifyCodeIntegrityMd5`).

If Option A is taken, this option is moot.

---

## Recommendation

| Option | Saving | Effort | Worth it? |
|---|---|---|---|
| A — Pre-decrypt cryptoloops + remove loadmod | **~3 s** | Medium (one-time decrypt + boot-config change + ongoing maintenance) | **Yes, if you want the cleanest, most maintainable result.** Removes a whole layer of complexity from the boot chain. |
| B — Parallel insmod | ~1–2 s | High (rewrite loadoa with dependency graph) | No — bad effort/reward |
| C — Non-blocking OmapNKS4Init | ~400 ms | Low (one ELF patch, similar to the `usb_reset_device` exercise) | **Yes, easy win** |
| D — Parallel Eva + WaveMotion mounts | ~150 ms | Trivial (loadoa wrapper script) | Yes, free |
| E — Skip MD5 file-hashing | ~1 s | Trivial (one byte patch in loadmod) | Yes IF you keep loadmod (i.e. if you don't take Option A) |

**Total realistic savings with A + C + D**: **~3.5–4 seconds** off a ~26 second
boot-to-Eva-spawn (~13–15 % reduction), or ~5–8 % of the full ~40–50 s
boot-to-playable-synth.

### What's NOT worth doing

- **Trying to parallelise loadoa's insmod sequence as the main lever** — the kernel
  module loader is single-threaded internally, and OA.ko (the largest, slowest
  module) has to wait for almost everything else to load anyway.
- **Trying to reduce Eva's own startup time** — much bigger potential payoff (~30 s)
  but completely out of scope without studying Eva's bank loader, audio
  engine init, KARMA setup, etc.

### The "go fast" path

If you genuinely want a fast-booting Kronos, **the highest-payoff direction is
Eva-internal**: profile Eva's PRELOAD bank loading, identify which banks are
expensive, and either lazy-load them or pre-cache parsed forms on `/korg/rw`. That's
a much larger studying project than the boot-chain work.

For boot-chain alone, the recipe is:

1. **Apply Option C** first — it's a 5-minute patch (extra symbol import, same
   technique as `tools/patch_omapnks4_cleanup.py`)
2. **Then Option A** — write a one-shot script that boots normally, copies decrypted
   files to `/korg/rw/decrypted/`, rewrites `OA.rc` / `loadoa` to plain-mount those
   directories, removes `loadmod.ko` from the boot insmod list
3. **Then Option D** — replace `loadoa` with a small shell wrapper that backgrounds
   the WaveMotion mount

Combined boot-time reduction: ~3.5–4 s, or roughly **a quarter of the visible boot
delay before Eva's logo appears**. The other three quarters are inside Eva itself.
