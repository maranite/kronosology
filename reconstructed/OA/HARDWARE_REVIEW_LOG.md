# Hardware Review Log

Questions/issues found during autonomous OA.ko RE sweep (goal set 2026-07-25:
"Reverse engineer all of OA.ko"). Real-hardware testing deferred to end per
goal instructions. Ornith agent (192.168.0.14:8088/v1) used for aux tasks
where noted -- review its output for wrong answers before trusting.

Format: `## <fn/topic>` + what's uncertain + what real-HW test would confirm.

---

## CSTGKeybedInterface_Startup() / CSTGComPort::Initialize() — VM-only stall trigger?

Uncertain: on the VM (no real W83627 Super-I/O chip), this init-module step
(`keybed_init.cpp`) runs 10 rounds x 6 ports of `DetectChipAt()` failures
(60 fast ISA-port probes, no delay calls on the failure path) before
falling through the 2026-07-24 WORKAROUND that downgrades the real
hard-fail to a soft/logged one. Immediately after this step, a separate,
still only partially-diagnosed kernel-level stall appears inside
`register_framebuffer()`'s own fbcon/VT console-takeover code (see
`KronosScreenRemoteDaemon/docs/vm_environment.md` section 0d) — not proven
to be caused by the keybed probing, but temporally adjacent and worth
checking.

Real-HW test that would help: on a real Kronos (real W83627 chip always
present, `DetectChipAt()` succeeds on round 1), does `CSTGKeybedInterface_
Startup()`'s success path (the ACK-wait `udelay` loop, not exercised in the
VM) ever interact with the console/VT subsystem in a way this VM-only
no-chip path doesn't? If real hardware's framebuffer driver (`OmapVideoModule`,
not `fakefb.ko`) registers cleanly with a live VT console every boot, that
would suggest the VM's stall is specific to `fakefb.ko`/QEMU's environment,
not a latent bug that could ever surface on real hardware.

---

## CSTGControllerRTData::ResetSendKnobsJumpCatch() — per-track bus-routing table math (batch 57)

Uncertain: cases 2/3 of this function's own real jump table (audio-send
knob "jump catch" reset for a specific track/bus) read TWO unnamed,
giant `CSTGGlobal`-relative tables (`+0x27cdb08`, `+0x27cea0f`, `0x1cad`
per-sequence-row stride) whose real semantic field names were not
determined -- reproduced as raw offset arithmetic straight from the
disassembly, matching this project's established "preserve real offsets
faithfully even when the name isn't known" convention, but NOT exercised
by this batch's own KAT (would need a much larger synthetic `CSTGGlobal`
buffer than the smallest-offset mode-1 case the KAT already covers for
the OTHER cases). `STGAPIOutToBusType`/`STGAPIOutToPhysBusId` (already
real, `audio_input_mixer.cpp`) are indexed by a byte this function reads
out of the second unnamed table, with no bounds check in ground truth
either (reproduced faithfully, not guarded).

Real-HW test that would help: with a known-good performance loaded that
has audio-input sends routed to a specific bus, trigger this function's
own real caller path (soloing/deactivating an audio-send knob) and
confirm `UpdateAudioTrackSendJumpCatch`/`UpdateJumpCatchWithIFXSendKnobValues`
(both still deliberately deferred no-ops in this reconstruction) receive
the SAME track/bus identity a real Kronos's own equivalent internal state
would compute -- would validate both unnamed tables' row/column math at
once.

---

## CSTGControllerRTData::SetAudioInSolo() — confirmed-pure-virtual dispatch, assumed dead (batch 57)

Not uncertain from the disassembly itself (`readelf -r` directly confirms
ground truth's own `_ZTV15CSTGPerformance` vtable slot 27 resolves to
`__cxa_pure_virtual`, and no derived class anywhere in the whole binary
overrides it) -- but WORTH a real-hardware sanity check anyway: this
project's own conclusion is that soloing an audio input while the current
performance is a genuine `CSTGProgram`/`CSTGCombi` instance can never
actually reach a live call through this slot on REAL hardware either (a
defensive trap, not reachable code). If a real Kronos's own "Solo" button
on the Audio Input page were ever observed to visibly do something at
this exact call site (not just the local bit-toggle this reconstruction
already performs unconditionally), that would falsify this assumption and
mean either a different, not-yet-identified derived vtable exists, or
this project's own `ResolveCurrentPerformance()` formula lands somewhere
other than a `CSTGProgram`/`CSTGCombi` in some mode this batch didn't
consider.

---

## CSTGFrontPanel::HandleKeyOn — note-range-fold branches use plain C div/mod instead of the real reciprocal-multiply bit trick

Uncertain: the real disassembly folds an out-of-range computed note
number (keyNum + a 3-byte `CSTGControllerRTData` transpose/octave sum)
back into 0-127 via a genuine x86 signed-division-by-12
reciprocal-multiply sequence (two DIFFERENT reciprocal constants for the
high-overflow vs low-underflow branches). This reconstruction computes
the mathematically equivalent result via plain C `/`/`%` instead of
hand-transcribing the exact instruction sequence -- confirmed to produce
the identical LOW BYTE (the only part any real downstream consumer ever
reads: the per-key state table, the MIDI Note-On message, and the
`STGAPIFrontPanelStatus` echo bytes all only ever read `dl`/`al` in the
real disassembly), and cross-checked by hand for four representative
cases (in-range, +overflow, -underflow non-multiple-of-12, -underflow
exact-multiple-of-12) in `verify/test_front_panel_key_handlers.cpp`. NOT
verified against real hardware: this fold path only fires when a
front-panel key's own transpose/octave sum pushes it outside 0-127,
which would need a specific (and unusual) combination of
`CSTGControllerRTData::sInstance[0x28]/[0x29]/[0x2a]` values -- fields
whose own real names/semantics were not independently determined by
this pass. A real-HW test that would help: set an extreme transpose/
octave-shift combination via the front panel UI (if exposed) and confirm
a physical front-panel key near the top or bottom of its range still
sends the musically-expected (same pitch class) note.

---

## CSTGFrontPanel::SetLED/SetLEDBlinking/ResetLED — CSTGKeybedInterface::SetLED left as a no-op stub

Not uncertain from the disassembly itself (the dispatch shape --
2-value range check on `code`, forward to the real
`CSTGKeybedInterface::SetLED(code, action)` for that narrow range,
otherwise a packed command word via the already-real
`OmapNKS4OutputFifo_WriteCommand()` -- is fully confirmed and KAT-
covered). What's genuinely deferred is the CALLEE:
`CSTGKeybedInterface::SetLED` itself (batch-63's own un-triaged
candidate 1, the whole ~20-method keybed wire-protocol driver class) is
still a no-op stub in `bar2_stubs.cpp`, so on a real Kronos this
reconstruction would silently fail to light the two front-panel LEDs
whose codes fall in the `0x49`/`0x4a` range (identity not independently
determined -- `eSTGLEDCode` enum values not recovered). Every OTHER LED
code (the vast majority) already goes out for real via
`OmapNKS4OutputFifo_WriteCommand()`, unaffected by this gap. A real-HW
test that would help: once `CSTGKeybedInterface::SetLED` gets its own
real reconstruction pass, confirm which two physical LEDs
`0x49`/`0x4a` correspond to (compare against the already-decoded keybed
serial protocol, agent-memory `kronos_keybed_serial_protocol.md`) and
that they light/blink/reset correctly through this path.
