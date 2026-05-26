# Extension Points — Adding More Banks

This is the deep-dive on **what would it take to add a 24th program bank? a 15th combi
bank? a 9th wave-sequence bank?** — i.e. to expand the Kronos beyond Korg's hardcoded
limits via OA.ko patches.

The Kronos's design is, unfortunately, **highly inelastic** about most bank sizes:
nearly every "how many of X can exist" answer is a hardcoded integer constant in OA.ko's
compiled binary, not a runtime-configurable parameter. The good news is those constants
are *individually* small, fixed-width, and easy to find. The bad news is changing one
without changing all its siblings causes either silent truncation or a crash.

This document maps every constant we've found, what it controls, and what work would be
needed to bump each one.

---

## Quick comparison table

| Resource | Current limit | Hardcoded as | Loader function | Difficulty to bump |
|---|---|---|---|---|
| **Program banks** | 23 (16 + 7) | `0x17` in `CSTGGlobal::InitializePerformances` | `0x00006770` | **Hard** — see below |
| **Combi banks** | 14 | `0x0E` in same function (second loop) | `0x00006770` | **Hard** — same shape as programs |
| **Set lists** | 128 | `0x80` in `CSetListBank::Initialize` | `0x002014c0` | **Medium** |
| **Wave sequences (total)** | 598 | `0x256` in `CSTGWaveSeqData::Initialize` | `0x00081860` | **Easy** (just a loop bound) |
| **Drum kits (per bank, type 3)** | 16 | header-driven (`record_count`) | Eva-side | **Medium** |
| **Drum kit banks** | 16 file slots | per-bank file iteration in Eva | Eva-side | **Medium** |
| **Programs / bank** | 128 | header-driven (`record_count`) **+** inner copy loop bound `0x80` | `CSTGGlobal::InitializePerformances` | **Hard** |
| **Combis / bank** | 128 | header `record_count = 0x80` | same | **Hard** |
| **Timbres / combi** | 16 | per-record layout in combi format | structural | **Impossible** without complete layout redesign |
| **Steps / wave sequence** | 64 | per-record layout | structural | **Impossible** without redesign |
| **Slots / set list** | 128 | per-record layout (`128 × 542 bytes`) | structural | **Impossible** without redesign |
| **Piano types** | Unlimited (dir-scan) | n/a — file enumeration | `CSTGPianoModel::RescanPianoTypes` | **Already extensible** |

---

## Program banks — the constants you'd patch

`CSTGGlobal::InitializePerformances` at `0x00006770` is the loader:

```c
// pseudo-C from decompiled OA.ko
for (iVar7 = 0; iVar7 < 0x17; iVar7++) {            // ← #1 — bump from 0x17 to allow more banks
    char buf[12];
    if (iVar7 < 0x10) {                              // ← #2 — bump to allow more single-letter files
        snprintf(buf, 11, "PROG%c.BIN", 'A' + iVar7);
    } else {
        char l = 'A' + (iVar7 - 16);
        snprintf(buf, 11, "PROG%c%c.BIN", l, l);     // ← #3 — and/or extend the double-letter range
    }
    CKorgProgBankFile bank(buf);
    int rc = bank.Load();
    if (rc == 0) kBankInfo[iVar7 * 2] = bank.bank_type;
    *(uint*)(STGAPIFrontPanelStatus::sInstance + 0x294f8) |= (1 << iVar7);  // ← #4 — 32-bit bitmask
    CSTGProgramBank::Initialize(<base> + iVar7 * 0x67603 + 0x132b102, ...);  // ← #5 — fixed memory stride
}
```

To add a 24th program bank, you need to:

1. **Patch `0x17` → `0x18`** (or whatever). This is one byte to change (`CMP EDI, 0x17` → `CMP EDI, 0x18`). Locate via byte search; the `CMP imm8` opcode is `83 FF 17` (or with `JNE` follow-up).
2. **Make sure the snprintf template path can produce the new bank's filename.** If you bump the single-letter range, change `0x10` to `0x11` and ensure ASCII `'A'+0x10` = `'Q'` is acceptable. If you bump the double-letter range instead, you have to choose what double letters to use — `HH`, `II`, `JJ` ... — and verify ASCII validity.
3. **Verify the bank-type-flag bitmask at offset `0x294f8` has room.** It's a u32, so up to 32 banks can be tracked. Currently 23 are used, so 24, 25 ... 32 are free *but* the rest of the system may interpret unused bits as "absent". You'd want to study the consumers.
4. **Allocate the additional `0x67603` (423 939) bytes per bank in the CSTGGlobal memory area.** The pointer `base + iVar7 * 0x67603 + 0x132b102` walks past the end of the allocated `pPad_0x33ce` buffer at iVar7 = 23. That allocation is in `CSTGGlobal`'s constructor or in `CSTGHeapManager::sInstance`'s setup — you'd need to bump that to accommodate the new bank. **This is the showstopper** — getting an extra 424 KB allocated requires patching the heap setup, which is a deep change.

**Verdict for programs:** technically possible, but the heap allocation expansion in
step 4 is non-trivial. It would likely take a day of work to verify correctness and
prevent memory corruption.

---

## Combi banks — same shape, same difficulties

The combi loader (the second loop in `InitializePerformances`, terminating at iVar7
`!= 0xE`) is structurally identical to the program loader:

| Step | Constant | Notes |
|---|---|---|
| 1 | `0x0E` loop bound | One byte to change |
| 2 | `"COMB%c.BIN"` template | Single-letter only; no double-letter range exists |
| 3 | Bitmask at `+0x294f8` | Combi banks use a different bit range |
| 4 | Per-bank memory stride: `0xCF381` (849 793) bytes | Heap re-allocation needed for extra banks |

Same heap-expansion showstopper. You'd also need to add a `"COMB%c%c.BIN"` template
(it doesn't exist in the current code) if you wanted user combi banks.

---

## Wave sequences — the easy win

`CSTGWaveSeqData::Initialize` at `0x00081860`:

```c
void CSTGWaveSeqData::Initialize(CSTGWaveSeqData *this) {
    iVar1 = 0;
    do {
        iVar1 = iVar1 + 1;
        (**(code **)(*(int *)this + 0x1c))();    // virtual: per-record init
        this = this + 0xd14;                       // 0xd14 = 3 348 bytes per wave-seq runtime entry
    } while (iVar1 != 0x256);                     // 0x256 = 598 — the current total
}
```

To support more wave sequences total:

- Change `0x256` to your desired total (one byte if ≤ 255, two-byte `cmp imm16` if >255)
- Verify the parent `CSTGWaveSeqData` object has enough room — the iteration steps
  `0xd14` bytes each, so the parent must be at least `total × 0xd14` bytes
- Add the corresponding wave-sequence file(s) to PRELOAD with `record_count` summed
  appropriately across files

**Difficulty: low.** This is the cleanest extension point. If you want to add a 16th
internal wave-sequence bank (32 more sequences), bump `0x256` to `0x276` (= 630) and
add `WSEQI.BIN` with `record_count = 32`.

---

## Set lists — moderate

`CSetListBank::Initialize` at `0x002014c0`:

```c
void CSetListBank::Initialize(CSetListBank *this) {
    iVar1 = 0;
    do {
        iVar1 = iVar1 + 1;
        (**(code **)(*(int *)this + 0x1c))();
        this = this + 0x834;                      // 0x834 = 2 100 bytes per set-list runtime entry
    } while (iVar1 != 0x80);                     // 0x80 = 128 set lists
}
```

To bump beyond 128:

- Change `0x80` to your target
- Confirm `STLS.BIN` header `record_count` matches
- Confirm the in-memory `CSetListBank` parent has room (`new_count × 0x834` bytes)
- **Worry:** Set-list UI navigation uses 7 bits (0..127) in several places to address slots
  by index. The 8th bit may be repurposed for something else (e.g. mode flag). Adding a
  129th set list might silently roll over to set list 1.

**Difficulty: medium.** The runtime constant is easy to patch, but verifying the UI's
range handling needs careful per-page review in Eva.

---

## Drum kits and drum-kit banks

Drum-kit loading is **Eva-side**, not OA.ko-side. The file iteration is in Eva's
`CPreloadFile` framework, and the count of drum kits is read from each file's header
(`record_count`). So bumping the per-bank count requires:

- Changing the `DKIT?.BIN` file's `record_count` header field
- Padding the file with the extra records (each `record_size = 38 424` bytes)
- Patching Eva's per-bank capacity check (the constant is in Eva, not in OA.ko)

Adding a NEW drum-kit bank file (e.g. `DKITJ.BIN`):

- Add the file to PRELOAD
- Patch Eva's `KEnumDrumKitBank` / `CFmtDrumKitBankNoToWrite` to know about the new
  bank
- Patch OA.ko's drum-kit bank table size

The exact Eva offsets aren't yet documented in this project (deeper Eva RE is needed).

---

## What about adding "more programs per bank" (e.g. 256 per bank instead of 128)?

This is **structurally impossible** without changing the on-wire bank-index size. Combis
reference programs by `(bank, index)` where `index` is a single byte. The byte can hold
0..255, but Korg's protocol uses values 128..255 for special meanings (e.g. "OFF", bank
type, EX-i variant) — so the usable range is 0..127. Bumping past 128 would silently
collide with the special values.

The same constraint applies to combis-per-bank, drum-kits-per-bank, and wave-seqs-per-bank.

---

## Bigger picture: what Korg got wrong (from a future-proofing standpoint)

The Kronos's bank capacity is hardcoded at the integer level rather than read from a
config. A more elegant design would have:

- A central "bank capacity table" (`g_BankCapacities[]`) consulted by every loader
- A central "bank-ID enum" that grows without code changes
- An on-disk "bank manifest" file listing all valid bank IDs and their file paths

Korg shipped neither, so each loader inlines its own loop bound. Adding banks today
means patching each loader individually.

This is largely a function of when the Kronos was designed (~2010) — embedded music
gear of that era favoured compact, hardcoded layouts to minimise RAM use. Modern
designs would use Lua / JSON config files for this kind of thing.

---

## What's the realistic path forward?

### Easy: more wave sequences
Patch `0x256` → desired total. Provide the new file. Done.

### Medium: more set lists
Patch `0x80` → desired total. Test the UI doesn't roll over. Provide the new file.

### Hard but feasible: an extra "EX-style" program bank
Choose a free letter (Q, R, ...) or a triple-letter (`AAA`, `BBB`). Patch the loader bound (`0x17`).
Patch the snprintf template if needed. Allocate the extra `0x67603` bytes in CSTGGlobal's
heap. Provide the file. Plan for several hours of testing.

### Very hard: more combis per bank
Would require changing the combi-index width across the entire engine — every program-change
event, every PCG export/import, every SysEx command, the UI bank dropdowns, ... not
practical to patch.

### Trivial: piano types
Drop new `PianoType<NN>` files into the PianoTypes directory. Send `PT:` on `/proc/.oacmd`.
Already supported with no patches.

---

## Test methodology if you do try this

1. **Always work on a copy of `OA.ko`.** Patch the constants you've identified.
2. **Update the corresponding header.** If you're bumping wave sequences total, the loader
   reads from N files — update only the affected file's `record_count`.
3. **Boot and check `dmesg`** (via SSH). The audio engine will log if a bank exceeds its
   allocated runtime memory.
4. **Save a known state, reboot, reload, save again.** Compare the files byte-for-byte
   — any corruption shows up as differences in unrelated regions.
5. **Test the relevant UI flows** — bank selection, program write, PCG save/load.

A change that loads cleanly but corrupts saved data on the next reboot is *worse* than
no change. Verify save-and-restore round-trips before relying on any extension.

---

## See also

- [program_banks.md](program_banks.md)
- [combi_banks.md](combi_banks.md)
- [set_list.md](set_list.md)
- [wave_sequence_banks.md](wave_sequence_banks.md)
- [drum_kit_banks.md](drum_kit_banks.md)
- [piano_types.md](piano_types.md) — the one exception, fully extensible
- [../workflow/export_patched_ko.md](../workflow/export_patched_ko.md) — how to ship the patched OA.ko once you've identified your bytes
- [../modules/OA.ko.md](../modules/OA.ko.md) — where to look for the function addresses
