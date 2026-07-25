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

## CSTGFrontPanel::SetLED/SetLEDBlinking/ResetLED — CSTGKeybedInterface::SetLED now real (batch 64)

UPDATE (batch 64): `CSTGKeybedInterface::SetLED` is no longer a no-op
stub -- see `src/init/keybed_interface.cpp`. Confirmed real: gates on
`KEYBED_OFF_STATE >= 2` (accepts states 2/3/4, unlike most of this
class's other gated senders which require state==2 exactly), maps
`code==0x49`->LED index 0 / `code==0x4a`->LED index 1, packs `action`
into a command byte as `(action & 0x2f) | 0xd0`, and sends `{cmdByte,
ledIndex}` over the same wire as every other command in this class.
STILL not independently confirmed: which two PHYSICAL LEDs indices 0/1
correspond to (`eSTGLEDCode` enum values themselves not recovered,
matching this entry's original note). A real-HW test that would help:
trigger `CSTGFrontPanel::SetLED(0x49, ...)`/`SetLED(0x4a, ...)` (or
watch for it via a live front-panel LED interaction) and correlate
against the already-decoded keybed serial protocol (agent-memory
`kronos_keybed_serial_protocol.md`) to name the two indices for real.

---

## CSTGKeybedInterface::SetKeyChatterGateTime — bit-split command encoding, semantic meaning unconfirmed

Not uncertain that the ENCODING is faithfully reproduced (the exact bit
operations -- `byte1 = (ms>>1)&1`, `byte2 = ms>>2` for `ms<=61`,
clamped to the fixed pair `{1, 0xf}` above that -- are a direct,
disassembly-confirmed transcription, and the KAT in
`test_keybed_interface.cpp` locks the exact byte values in). What's
NOT confirmed is the real hardware-register MEANING of this odd 1-bit +
4-bit split (as opposed to, say, a plain linear byte or a different bit
width) -- no datasheet or further disassembly for the keybed board's own
firmware was available to cross-check. A real-HW test that would help:
call this with a few known millisecond values while watching real key-
chatter-filter behavior (rapid on/off re-triggers on a single physical
key) to confirm the encoded gate window's real-world duration actually
tracks the `ms` argument the way this reconstruction assumes.

---

## CSTGKeybedInterface::ReceiveMessage (state==2) / WriteMessageToQueue — enqueue gate semantics unconfirmed

Not uncertain that the GATING LOGIC ITSELF is faithfully reproduced
(disassembly-confirmed instruction-for-instruction, including the
asymmetry between the heartbeat class's gate1-only check and the
non-heartbeat class's gate2-then-gate1 check -- see
`keybed_receive.cpp`'s own comment and `test_keybed_interface.cpp`'s
[12] for both). What's NOT confirmed is the real MEANING of
`KEYBED_OFF_ENQUEUE_GATE1`/`KEYBED_OFF_DISPATCH_GATE2`/
`CSTGMessageProcessor::sInstance`'s own `+0x48` byte (plausibly
something like "raw passthrough enabled" / "message processor has
claimed the port", but not independently confirmed) -- these bytes are
never written anywhere in this project's own reconstructed code (no
confirmed real setter found for either gate byte in the functions
examined this batch), so in practice, with both defaulting to 0 (zeroed
storage), every non-heartbeat message is currently dropped and every
heartbeat is also dropped unless something else sets gate1. A real-HW
test that would help: instrument a live Kronos (or the QEMU virtual
keybed harness already used for the sec 10.237/comport work) to log
raw keybed traffic reaching the ring buffer during normal operation,
confirming these gates are set somewhere in a not-yet-reconstructed
caller (plausibly `CSTGControlMsgHandler::TakeOverKeybedComm`, which
also remains unreconstructed).

---

## CSTGKeybedInterface::FilterAnalogController — code 1/2 channel identity is a guess (joystick X/Y)

Not uncertain about the CONTROL FLOW (the "first non-centered reading
arms a flag and reports unchanged, second one filters for real" state
machine for `code`==1/2, and the always-filter `code`==0 path, are all
disassembly-confirmed and KAT-covered). The CHANNEL IDENTITY --
`code`==1/2 are joystick X/Y and `code`==0 is aftertouch pressure -- is
inferred only from `ApplyAftertouchTable`'s own confirmed 3-way
dispatch using the SAME `code` values, not from any string or symbol
name. A real-HW test that would help: move the physical joystick along
just one axis while pressing/releasing aftertouch, and confirm which
`code` value each physical control maps to (would also pin down whether
`code`==1 is X or Y).

---

## CSTGKeybedInterface::HandleActiveSense — nibble 8/9/0xa/0xd sub-type meanings are guesses

Not uncertain about the DISPATCH ITSELF (disassembly-confirmed exactly:
nibble 8 and 9 both write `STGAPI_OFF_FOOTSWITCH0/1 = {0x24, 0x3d}`,
nibble 9 additionally sets `STGAPI_OFF_NKS4_PANEL_KIND=1`, nibble 0xa
sets `STGAPI_OFF_PANEL_DETECTED=1` AND arms the debounce filter's own
`+0x0` byte, nibble 0xd sets `STGAPI_OFF_KEYBED_NIBBLE_D_FLAG=1`; the
real production heartbeat byte `0xEA` observed live on hardware
(agent-memory `kronos_keybed_serial_protocol.md`) has low nibble 0xA,
matching the "panel detected" case exactly). What's NOT confirmed:
whether nibble 8/9's fixed `{0x24, 0x3d}` pair is really a "footswitch
capability" announcement (the existing `STGAPI_OFF_FOOTSWITCH0/1`
naming, established by an earlier pass, is REUSED here rather than
independently re-derived) or something else entirely, and what nibble
0xd's flag actually gates. A real-HW test that would help: capture the
keybed's own idle heartbeat stream through a full power-on cycle and
watch for the header byte's low nibble to ever be something OTHER than
0xA (a boot-time 0x28/0x29/0x2D-class heartbeat before settling into
steady-state 0x2A/0xEA, e.g.) to see these other branches fire for
real.

---

## CSTGKeybedInterface::ApplyKeybedCalibration — confirmed real, deliberately deferred (batch 64)

Not a disassembly-uncertainty note but a scope-decision one, recorded
here because `FilterAnalogController`/`ApplyCalibrationAndAfterTouchTable`
both depend on it and this reconstruction's own correctness for THOSE
two methods is only as good as this dependency's real behavior:
`ApplyKeybedCalibration` (`.text+0x33edd0`) is confirmed real but NOT
reconstructed -- it performs a genuine kernel-mode FPU context switch
(`mov %cr0,%eax; clts; ...`) guarded by two `.bss` globals
(`SetupKeybedCalibration`/`CleanupKeybedCalibration`, also confirmed
real but out of `CSTGKeybedInterface`'s own scope), then an
interpolation pass this project has not traced. Host KATs mock it with
a deterministic stand-in (`test_keybed_interface.cpp`'s own
`ApplyKeybedCalibration` returning a fixed value), which validates
`FilterAnalogController`'s CALLING convention and control flow but says
nothing about whether the real calibration MATH is faithfully modeled
(it isn't modeled at all yet). A real-HW test that would help is
downstream of first actually reconstructing this function -- listed
here so a future pass doesn't have to re-discover the FPU-context-
switch gap from scratch.

---

## CSTGKeybedInterface::ProcessNextKeybedEvent — entirely deferred (batch 64)

Scope-decision note, not a disassembly uncertainty: this 1712-byte
method (by far the largest in the class) is the real dispatcher that
would turn decoded ring-buffer messages into actual keybed/joystick/
aftertouch events (calling into the now-real `CSTGKeybedKeyDebounceFilter::
ProcessKeyOn`/`ProcessKeyOff` and `CSTGFrontPanel::HandleAnalogController`),
but depends on FIVE entirely unmodeled classes/functions
(`PushUnsolicitedMessage`, `CSTGDelayedMsg`, `CSTGControllerInfoUnsolMsg::
Send()`, `USTGKeyTouchTable::Convert9bitCountsTo8bitInterval()`,
`CSTGCalibrationMsgHandler::HandleKeybedCalibrationResult()`) on top of
the already-deferred `ApplyKeybedCalibration` above. Genuinely out of
proportion for a single method in this pass -- see
`keybed_interface.cpp`'s own file comment for the full reasoning. This
means the raw wire protocol IS now correctly framed/enqueued
(`ReceiveMessage`/`ReadMessageFromQueue`) but nothing in this
reconstruction yet DRAINS the queue and turns it into musical/UI
events -- a real functional gap for anyone testing end-to-end keybed
behavior against this reconstruction on real hardware.

---

## CSTGKeybedInterface::MemberStartup/MemberCleanup/TryComPort — no confirmed caller

Not uncertain about the reconstruction itself (both are faithful,
disassembly-confirmed transcriptions, matching the already-verified
free-function `CSTGKeybedInterface_Startup`/`_Cleanup` boot-path pair's
own algorithm shape closely enough that the same confidence applies).
What's unconfirmed: NO caller of these member-function versions exists
anywhere in this project yet (plausibly a manual "keybed rescan" UI
action or `CSTGControlMsgHandler::TakeOverKeybedComm`, itself not
reconstructed) -- so unlike the free pair (confirmed boot-reachable from
`init_module()`), these three methods are currently dead code from this
project's own reachability analysis, exercised only by
`test_keybed_interface.cpp`'s own mocks, never by a live boot path. A
real-HW/live-VM test that would help: once a caller is identified and
reconstructed, confirm a "rescan port 0 only" keybed reconnect actually
matches this member pair's own hardcoded-port-0 behavior rather than
the free pair's 6-port scan.
