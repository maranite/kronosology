# Wave-Sequence Banks — `WSEQ*.BIN`

A **Wave Sequence** is an ordered list of up to 64 *steps*, each holding a multisample,
duration, level, crossfade time, and pitch offset. Programs that use a wave-sequence
oscillator step through the sequence on each note-on, producing complex evolving timbres
(this is the technology Korg's Wavestation was famous for).

| Property | Value |
|---|---|
| File naming (internal banks) | `WSEQ<A..H>.BIN` — 8 files |
| File naming (user banks) | `WSEQ<AA..GG>.BIN` — 7 files |
| Total banks | **15** |
| Header magic | `PWSQ` |
| Container | See [container_format.md](container_format.md) |

---

## Per-file breakdown

| File | Size | Records | Record size |
|---|---|---|---|
| `WSEQA.BIN` | 332 420 | **150** | 2 216 |
| `WSEQB.BIN`..`WSEQH.BIN` (7) | 70 932 each | 32 | 2 216 |
| `WSEQAA.BIN`..`WSEQGG.BIN` (7) | 70 932 each | 32 | 2 216 |

`WSEQA.BIN` is unique in holding 150 wave sequences — Korg's main factory bank ships
with substantially more than the standard 32-per-bank capacity. The header's
`record_count = 150` overrides any compile-time default. All other banks are normal
32-slot banks.

Total wave-sequence capacity: 150 + 14 × 32 = **598 wave sequences**.

---

## Per-record layout (2 216 bytes per wave sequence)

```
Offset       Size       Field
-------------------------------------------------------------------
0x000        24         Wave-seq name (ASCII, null-padded)
0x018         8         Common: number of steps, start step, loop type, …
0x020     64 × 34       Step 1..64, each:
                          • multisample bank + index (4 bytes)
                          • duration (rate / beat-fraction)
                          • crossfade time
                          • level
                          • pitch offset (semitones, cents)
                          • next-step jump (for sub-loops)
0x880      ~80          Modulation routing / step-rotation / mute mask
-------------------------------------------------------------------
Total: 2 216 bytes  (64 × 34 = 2176, plus 40 bytes header/footer)
```

Each step is ~34 bytes; this is consistent with what the Korg Parameter Guide describes
for wave-sequence step parameters.

---

## Loader

Wave-sequence banks are loaded by **`CSTGWaveSeqData::Initialize`**, called from
`CSTGGlobal::Initialize` (`0x00008340` in OA.ko). This is a separate top-level loader
from the program/combi initializer.

The loader walks all 15 wave-sequence files using a snprintf template along the same
lines as the program loader but with `WSEQ%c.BIN` and `WSEQ%c%c.BIN`. The loop bounds
are likewise hardcoded — see [extension_points.md](extension_points.md).

---

## Why a wave sequence record is so small relative to a program

Wave sequences are pure "playlist + envelope" data. They don't contain any audio
samples — they just *reference* multisamples by (bank, index). The heavy lifting
(audio I/O, filtering, amp envelope) happens in the **program** that hosts the wave
sequence — the wave sequence is just the step list and timing data.

This is why a wave-sequence record (2 216 bytes) is much smaller than a program record
(4 960 bytes), and dramatically smaller than a drum kit (38 424 bytes) — drum kits have
~88 mapped keys, where each key needs its own audio routing data; a wave-sequence step
just stores a multisample reference and a step count.

---

## Performance implication

Korg keeps every wave-sequence record in RAM at all times — they're small and the
engine needs random access to "step N of sequence M" at audio rate. Total RAM cost
for the wave-sequence data is roughly 598 × 2 216 ≈ 1.3 MB — trivial on the Kronos.

---

## See also

- [container_format.md](container_format.md)
- [program_banks.md](program_banks.md) — programs use wave sequences as oscillator sources
- [extension_points.md](extension_points.md) — adding more wave-sequence banks
- `KRONOS_Param_Guide_E10.pdf`, p. 420+ — wave-sequence parameter reference
