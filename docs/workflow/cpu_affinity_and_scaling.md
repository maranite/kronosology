# CPU Affinity & Multi-Core Scaling

This document maps the Kronos audio engine's threading model, where every hardcoded
CPU-count constant lives, and what it would take to scale to a 4-core, 8-core, or
larger CPU.

The short answer: **the Kronos software is already designed for up to 4 cores.**
Drop a quad-core CPU in (with appropriate hardware modifications) and the audio
engine will *automatically* use all four. Going beyond 4 cores requires patches.

---

## The current thread model

OA.ko's audio path is structured around **three distinct thread classes**, each with
its own real-time priority and CPU pinning:

| Thread class | Vtable address | Role | Pinned to |
|---|---|---|---|
| **`CSTGAudioThread`** | `0x594d20` | Audio I/O — wakes from the CODEC interrupt, drives the audio device buffer, runs per-tick housekeeping | Core 0 (the "audio core") |
| **`CSTGSynthesisThread`** | `0x594d00` | Voice synthesis — runs the per-voice oscillator + filter + amp pipeline | Cores 1, 2, 3 — one thread per non-audio core |
| **`CSTGEffectThread`** | `0x594d70` | Effects — runs the IFX/MFX/TFX chains | Pinned by load (typically core 0 or 1) |

Plus:
| | | | |
|---|---|---|---|
| **`CSTGAudioManager::ASKThreadRoutine`** | (member fn) | Async service / housekeeping (file daemon, MIDI dispatch) | Whichever core has slack |

All three "audio" classes inherit from `CSTGThread` and get created via:

```c
CSTGThread::CreateRealTimeWithCPUAffinity(
    routine,        // function pointer (e.g. AudioTickLoopRoutine)
    priority,       // RT priority
    cpu_id,         // which core to pin to
    arg);           // thread arg
```

Internally this calls `rtwrap_pthread_attr_setrtpriority` then
`rtwrap_pthread_create` then **`rtwrap_set_runnable_on_cpuid`** — that last call is
the RTAI primitive that pins the new thread to a specific CPU.

`CSTGThread::CreateRealTimeWithCPUAffinity` is at OA.ko Ghidra address **`0x00040a30`**.

---

## Where CPU count is decided

### 1. The Linux kernel build

Korg ships a patched Linux 2.6.32 kernel. The defconfig is at:

```
<Korg-published-linux-kernel-source>/arch/x86/configs/korg_kronos_defconfig
```

with:

```
CONFIG_NR_CPUS=4
```

This is the **compile-time maximum number of CPUs** the kernel can address. If you put
in a 5+ core CPU but boot the existing kernel, the kernel only sees 4 of them.

### 2. `stg_num_online_cpus` — the bitmask cap

OA.ko's `stg_num_online_cpus` at Ghidra `0x00118de0`:

```c
void stg_num_online_cpus(void) {
    hweight32(*_cpu_online_mask & 0xf);    // popcount, masked with 4 bits
    return;  // returns count in EAX (Linux kernel convention)
}
```

The mask `0xF` (= `0b1111`) means **only CPUs 0, 1, 2, 3 are ever counted**, regardless
of how many the kernel actually brought online. Even if `CONFIG_NR_CPUS` were larger,
this mask caps OA.ko's view at 4.

### 3. `CSTGCPUInfo` — explicit 4-cap

OA.ko's `CSTGCPUInfo::CSTGCPUInfo` (constructor, Ghidra `0x00023310`):

```c
CSTGCPUInfo::CSTGCPUInfo(uint param_2) {
    uint count = stg_num_online_cpus();
    if (param_2 == 0) param_2 = count;
    else this->m_NumCPUs = param_2;
    if (4 < param_2) {                    // ← explicit cap
        this->m_NumCPUs = 4;
    }
    ...
}
```

So even if `stg_num_online_cpus` returned >4 (it can't, due to the mask), this
constructor would still cap at 4.

### 4. `CSTGAudioManager` — the audio/synthesis split

OA.ko's `CSTGAudioManager` constructor (`0x000649d0`) does the audio↔synth split:

```c
uVar6 = *puVar3;                          // raw CPU count from CSTGCPUInfo
sInstance = this;
this->NumCPUs = uVar6;
if (uVar6 < 5) {                          // 1, 2, 3, or 4 cores
    if (1 < uVar6) {                      // 2, 3, or 4
        uVar6 -= 1;                       // reserve 1 for audio I/O
    } else {
        this->bSeparateAudioCore = 0;     // 1-core: everything single-threaded
    }
}
else {                                    // > 4 cores (impossible due to 0xF mask)
    uVar6 = 3;                            // safe default: 3 synth threads
    this->bSeparateAudioCore = 1;
    this->NumSynthThreads = 2;            // default to 2
}
this->NumSynthThreads = uVar6;            // = NumCPUs - 1
// initialise per-synth-thread counter array at offset 0xa6c, 4 bytes per entry
```

**This is the design rule**:

| Total cores | Audio I/O | Synthesis | Notes |
|---|---|---|---|
| 1 | shared | shared | Single-threaded fallback |
| 2 | core 0 | core 1 | The current Kronos hardware |
| 3 | core 0 | cores 1, 2 | Untested in shipping hardware |
| 4 | core 0 | cores 1, 2, 3 | **The intended max** |
| > 4 | n/a — the mask caps at 4 | n/a | Extra cores idle |

---

## What's already dynamic vs. static

The audio engine is **mostly** count-driven rather than constant-driven. Once
`CSTGAudioManager::sInstance + 0x18` (the CPU count) is set, every downstream consumer
reads from that field:

| Consumer | Address | Loop bound |
|---|---|---|
| `CModelVoiceRequirementsData::Clear` | `0x0005d0b0` | `uVar2 < AudioManager->NumSynthThreads` |
| `CModelVoiceRequirementsData::ClearQuadCPUPlacements` | `0x0005d0f0` | same |
| `CModelVoiceRequirementsData::CopyQuadCPUPlacementsFrom` | `0x0005d120` | same |
| `CModelVoiceRequirementsData::GetCPUForAllocateQuad` | `0x0005d150` | same |
| 12+ other call sites | various | same |

`GetCPUForAllocateQuad` is the **voice-load-balancer**: when a new voice (technically
a "Quad" — Korg's 4-voice synthesis unit) is allocated, this picks the synth core with
the most remaining capacity:

```c
uint GetCPUForAllocateQuad(CModelVoiceRequirementsData *this) {
    uint cpu = 0;
    if (NumSynthThreads != 0) {
        int free = this->mCpuVoiceCount[0];
        while (true) {
            if (free != 0) {
                this->mCpuVoiceCount[cpu]--;     // claim
                return cpu;                       // ← return chosen core
            }
            cpu++;
            if (NumSynthThreads <= cpu) break;
            free = this->mCpuVoiceCount[cpu];
        }
    }
    return 0;
}
```

This is **dynamic load balancing**: if synth core 1 is busy, the next voice goes to
synth core 2; if 2 is busy too, try 3. No core is statically dedicated to "oscillators"
or "effects" — what you see on the performance meter is the *result* of where work
ended up.

The dimension of the `mCpuVoiceCount` array (at offset `+0xc` in
`CModelVoiceRequirementsData`) is **fixed by the struct layout**. It currently holds
slots for 4 synth cores (offsets `0xc, 0x10, 0x14, 0x18` — 16 bytes total).

Going beyond 4 means changing the struct layout, which ripples into every
program/combi/patch record that *contains* a `CModelVoiceRequirementsData` (the
saved-state structures may include it).

---

## What the front-panel performance meter actually shows

The user-visible "OSC" and "FX" CPU bars on the front-panel meter aren't per-physical-core
readings — they're per-**workload** counters:

| Bar | Sourced from | Meaning |
|---|---|---|
| **OSC** | sum of all `CSTGSynthesisThread::Run` execution time, normalised | Synthesis-engine load — how busy the synth cores are |
| **FX** | sum of all `CSTGEffectThread::Run` execution time, normalised | Effects-chain load — IFX + MFX + TFX |

On a 2-core Kronos, the synth thread and the effect thread happen to be pinned to
different cores (synth on core 1, effects on core 0 alongside the audio I/O thread),
so the meter *looks* like a per-core view. On 4-core hardware, the picture would be
more nuanced — synth would spread across 3 cores while effects would share with audio
I/O on core 0.

The meter values are computed in `CSTGAudioThreadStats`,
`CSTGSynthesisThreadStats`, and `CSTGEffectThreadStats` (initialised at
`0x0005e030`, `0x0005e4d0`, `0x0005e220`) and read by Eva via `/proc/.oacmd`.

---

## Patching options — by ambition level

### Level 0 — Drop in a quad-core CPU, change nothing

If you replace the Kronos's CPU with a **quad-core x86 chip with compatible socket**
and don't touch any binaries:

- Kernel boots, sees 4 cores ✓ (`CONFIG_NR_CPUS=4` already supports it)
- `stg_num_online_cpus` returns 4 ✓ (mask `0xF` permits it)
- `CSTGCPUInfo` accepts 4 ✓ (cap is "≤ 4")
- `CSTGAudioManager` configures `1 audio + 3 synth` ✓
- Voice allocator load-balances across 3 synth cores ✓
- **Result: the Kronos uses all 4 cores automatically. Significant DSP headroom.**

This is the **easy win**: assuming you can get the hardware physically in (CPU socket
match, BIOS quirks, ACPI, thermals), the software needs **no changes at all**.

### Level 1 — Boot a 5+ core CPU on a stock kernel

A modern x86 CPU might have 6, 8, or more cores. With the stock kernel:

- Kernel boots with only 4 cores online (CONFIG_NR_CPUS=4 cap)
- Software runs as in level 0

**Result: works, but extra cores are wasted.** No patching needed if you accept the
4-core ceiling.

### Level 2 — Unlock up to 8 cores

Goal: use 6 or 8 cores fully.

**Required changes**:

1. **Linux kernel rebuild** with `CONFIG_NR_CPUS=8` (or 16) and any related symmetry
   options (e.g. `CONFIG_SMP` is already on; check `CONFIG_HOTPLUG_CPU`).

2. **Patch `stg_num_online_cpus`** at OA.ko Ghidra `0x00118de0`:

   - Find the `AND` instruction with immediate `0x0F` (single byte: `83 e0 0f` or similar)
   - Change `0x0F` → `0xFF` to allow 8 cores (or `0xFFFF` for 16, but that's a 16-bit
     immediate — different opcode)

3. **Patch `CSTGCPUInfo` cap** at Ghidra `0x00023310`:

   - Find the comparison `cmp eax, 4` (`83 f8 04`) — the `04` is the cap
   - Change `04` → `08` (or your target)

4. **Patch `CSTGAudioManager` ctor** at Ghidra `0x000649d0`:

   - Find `cmp uVar6, 5` (the "< 5" check) — change `05` → `09` (or `0x11` for 16)
   - Find the `uVar6 = 3` default (in the else branch) — bump to `7` so the fallback
     scales

5. **Expand `CModelVoiceRequirementsData`'s per-CPU array** at offset `+0xc`:

   - Currently 4 entries × 4 bytes = 16 bytes
   - For 8 cores: needs 8 entries × 4 bytes = 32 bytes
   - The struct grows by 16 bytes, which shifts every subsequent field
   - **This ripples** into every consumer that knows the struct layout — including
     the serialised forms in PROG/COMB record bodies (since voice-requirement data is
     embedded in program records)

5b. **Audit other per-CPU buffers** in OA.ko — there are likely more arrays sized
    for 4 that need to grow. Search the binary for `4` constants near per-CPU
    iteration loops.

6. **Verify the kernel module re-loads cleanly** — bumping struct sizes changes
    ABI, and `loadmod.ko`'s integrity check might reject a re-compiled OA.ko unless
    re-signed.

**Difficulty**: medium-to-hard, with the struct-layout ripple being the worst part.
Estimated 1-3 weeks of careful work for a confident implementation.

### Level 3 — Repurpose more cores for synthesis

Even at 4 cores, the current split is **1 audio + 3 synth**. On a 4-core CPU that's
already optimal. But on a 16-core CPU, you might want **2 audio + 14 synth**, or to
break the synthesis pool into voice-pool + drum-pool + KARMA-pool.

This is a **design change**, not a patch. You'd need to:

- Re-write `CSTGAudioManager::ctor`'s CPU-split policy
- Possibly add new thread classes (`CSTGDrumThread`?) and corresponding work-stealing
  queues
- Update Eva's performance-meter UI to show the new categorisation

**Difficulty**: serious software work. This is closer to "fork the audio engine" than
"patch a few bytes".

### Level 4 — Add hyperthreading awareness, NUMA awareness, dynamic-frequency-scaling integration

For modern hardware (Atom → Core → Ryzen), you'd want the engine to:

- Distinguish physical cores from hyperthreaded siblings
- Pin RT threads to one sibling per physical core (avoiding HT contention for synthesis)
- React to CPU frequency changes (the engine assumes constant `cpu_khz`)

All achievable but each is its own project. None of this is in the current Kronos
software.

---

## Practical recommendation

If your goal is **"more DSP headroom for the same money / footprint"**:

- **Just put in a quad-core CPU** (e.g. Atom Z3735F → Atom x5-Z8350 → Celeron N4020 →
  ... pick something compatible with the original motherboard or board-swap entirely)
- **No software changes** — the Kronos already supports 4 cores natively
- Expected gain: roughly 2× polyphony headroom, more effect slots, less voice-stealing

If your goal is **"port the Kronos to modern hardware with 8+ cores and 16+ GB RAM"**:

- You're in for the multi-month work described in level 2+
- Combine this work with the [64-bit / >4 GB port](../README.md#project-goals) effort
  — they share many of the same constraints (struct layouts, kernel rebuild, ABI)

If your goal is **"just understand the threading model for debugging"**:

- The map above is the whole picture: 3 thread classes (`CSTGAudioThread`,
  `CSTGSynthesisThread`, `CSTGEffectThread`), all RT-pinned via
  `CSTGThread::CreateRealTimeWithCPUAffinity`, load-balanced via
  `GetCPUForAllocateQuad`. Everything is built on RTAI's `rt_set_runnable_on_cpuid`.

---

## Eva-side affinity

Eva (the GUI process) uses **glibc's `sched_setaffinity`** rather than RTAI. Strings
in the binary:

```
sched_setaffinity
ERROR! sched_setaffinity failed, errno = %d
```

Eva pins its drawing / file / network threads to **core 0** (the audio core, where they
share with the audio I/O thread). This is fine because Eva runs at much lower priority;
it gets pre-empted by RT threads whenever audio work appears. The reason for the
pinning is *cache-locality* — Eva's working set stays on one core's L1/L2 so its
context switches don't pollute the synth cores' caches.

A multi-core upgrade doesn't change Eva's design — it can stay on core 0, and the
synth cores get freed up for more voices.

---

## Summary table

| Element | Where | What it caps | Patch difficulty to bump |
|---|---|---|---|
| `CONFIG_NR_CPUS=4` | Kronos kernel build config | Kernel-visible CPU count | Trivial — rebuild kernel |
| `mask & 0xF` | `stg_num_online_cpus` @ `0x118de0` | What OA.ko counts | 1-byte patch |
| `if (4 < x) cap=4` | `CSTGCPUInfo` ctor @ `0x023310` | OA.ko's stored count | 1-byte patch |
| `if (uVar6 < 5)`, `uVar6 = 3` | `CSTGAudioManager` ctor @ `0x649d0` | Audio↔synth split | 2× 1-byte patches |
| `mCpuVoiceCount[4]` | `CModelVoiceRequirementsData` layout | Per-synth voice counters | Struct layout — hard |
| `rtwrap_set_runnable_on_cpuid` | RTAI wrapper | Per-thread CPU pinning | n/a — works with any count |

---

## See also

- [../modules/OA.ko.md](../modules/OA.ko.md) — where the threading classes live
- [../modules/loadmod.ko.md](../modules/loadmod.ko.md) — RTAI wrapper provider (`rtwrap_*`)
- [../preload/extension_points.md](../preload/extension_points.md) — companion doc on
  hardcoded resource limits
- [export_patched_ko.md](export_patched_ko.md) — how to ship a patched OA.ko once
  you've identified your patch bytes
- `linux-kronos/arch/x86/configs/korg_kronos_defconfig` — the kernel
  config to rebuild
