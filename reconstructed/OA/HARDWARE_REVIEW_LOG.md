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

---

## CSTGMidiInPortSerial::ReceiveByte/ReceiveBytes/CheckForCompleteMessage — physical MIDI-IN UART parser (new cluster, 2026-07-25)

Real-hardware-verification uncertainty for the whole new physical
DIN-MIDI-IN byte-parser cluster (`src/engine/midi_in_port_serial.cpp`):

- **No known live caller.** Same reachability caveat as several prior
  clusters: nothing else in this reconstruction currently calls
  `CSTGMidiInPortSerial::ReceiveByte()`/`ReceiveBytes()`. On real
  hardware these would be invoked from a UART RX interrupt handler
  (or a DMA-completion callback) that is itself not yet reconstructed
  anywhere in this project. A real-HW test that would help: identify
  the actual physical MIDI-IN interrupt/ISR entry point in OA.ko (or a
  companion module) and confirm it really does call through these two
  methods with raw UART bytes, one at a time or in DMA-sized chunks.
- **Realtime-message timestamp ring (+0x14c..+0x1ac, 8 x 12-byte
  entries) is a genuinely new discovery this batch** (see
  midi_in_port_serial.cpp's own header comment for the full derivation)
  carved out of what was previously an undifferentiated
  `_unrecovered108[0x1d8]` blob. The field NAMES/PURPOSE (timestamped
  MIDI-clock-jitter diagnostic ring) are an inference from the
  rdtsc+ring-index code shape, not from any string/symbol confirming
  intended use -- plausible (used for tempo-sync jitter measurement or
  a diagnostics/`.oacmd` dump) but unconfirmed. A real-HW test: if any
  `/proc/.oacmd` command or debug ioctl ever dumps this region, compare
  its output against live MIDI Clock traffic timing to confirm the
  ring's actual consumer.
- **`StartSysEx()`/`ReceiveSysExData(unsigned char)` are deliberately
  deferred no-op stubs** (bar2_stubs.cpp) -- this means a REAL physical
  SysEx dump into this port would currently be silently dropped by this
  reconstruction (the running-status/channel/system-common path this
  batch actually reconstructs is unaffected and behaves faithfully).
  Confirmed real, substantial (368 + 1297 bytes, plus 178 + 268 bytes
  for the also-deferred `EndSysExScan()`/`EndSysEx()`) -- a good target
  for a future batch, not attempted here (see this project's own
  agent-memory for the size/scope reasoning).
- **The "sub eax,1;jne" dead/defensive branch for status>0xf7 mid-
  message** (`CheckForCompleteMessageImpl`'s `expected = 1` fallback,
  reached only if a data byte somehow gets accumulated against a
  status byte >0xf7) is preserved verbatim as unreachable-in-practice
  code, matching the real disassembly's own apparently-defensive
  structure -- no real-HW scenario is expected to exercise it, since
  0xF8-0xFF are always intercepted earlier as realtime bytes before
  ever reaching the data-byte accumulation path.

---

## CSTGMidiOutPort/CSTGMidiOutPortSerial — physical MIDI-OUT UART driver (new cluster, 2026-07-25)

Real-hardware-verification uncertainty for the output-side counterpart
to the MIDI-IN cluster above (`src/engine/midi_out_port_serial.cpp`):

- **The UART hardware transmit path is provably DEAD in this exact
  firmware image, not just "no known caller."** Unlike every other
  "no known live caller" caveat in this log (where the gap is this
  project's own reconstruction not yet reaching the call site),
  `CSTGMidiOutPortSerial`'s own real vtable
  (`.rel.rodata._ZTV21CSTGMidiOutPortSerial`) has its 2 trailing slots
  (`CanTransmitHardware()`/`TransmitHardwareByte()`) still resolving to
  `__cxa_pure_virtual` in OA.ko ITSELF -- confirmed via relocation, not
  inferred. `nm -C OA.ko_Decomp/OA.ko` was searched for any further
  class deriving from `CSTGMidiOutPortSerial` or providing symbols
  matching either name; none exists anywhere in the binary. This means
  on REAL hardware, if this exact code path (`CanSendRealTime()`/
  `CanSendRegular()`/`SendRealTime()`/`SendSingleByte()`, all 4
  confirmed real, all 4 forwarding to these 2 slots) were ever actually
  invoked, it would hit a kernel `BUG()`, not silently do nothing. A
  real-HW test that would help: with a serial-port MIDI cable connected
  to a real Kronos and something driving `CSTGMidiPortManager::
  ProcessMidiOutPorts()`'s own port loop, confirm whether physical
  DIN MIDI-OUT genuinely never transmits on real hardware (matching
  this dead-code finding), or whether some other, not-yet-discovered
  companion module patches this vtable's trailing 2 slots at runtime
  (e.g. STGEnabler.ko or similar, by analogy with several other
  runtime-patched-vtable patterns already documented elsewhere in this
  project) -- the two outcomes are currently indistinguishable from
  static analysis of OA.ko alone.
- **CSTGMidiPortManager::ProcessMidiOutPorts()** (the presumed real
  caller of `ProcessNormal()`/`GenerateActiveSensing()`/
  `ProcessNKS4TestMode()`, `.text+0xf5590`, confirmed real via `nm`)
  is NOT reconstructed by this batch -- it's the missing link between
  `init_module()`'s own boot path and this cluster ever running at all.
  Until it's reconstructed, this whole cluster remains unreachable from
  this project's own boot trace, same class of gap as the MIDI-IN
  cluster's own UART-ISR caller.
- **`ProcessRegularMessage()`'s exact `state` value semantics (0/1/2)
  are translated literally from disassembly, not fully named** -- the
  transcription is disassembly-exact (independently `objdump -dr`
  cross-checked byte-for-byte against the Ghidra decompile, including
  the register-dropped byte-index computation the initial decompile
  pass missed), but WHY the real firmware structures running-status
  compression as a byte-cursor-into-msgBuf rather than a simpler
  boolean isn't independently confirmed from any string/comment in the
  binary. A real-HW test: capture real DIN MIDI-OUT traffic (were the
  dead-vtable finding above ever found to be wrong) for a
  rapid-repeated-Note-On sequence and confirm the status byte is
  genuinely omitted on repeats within the 75-tick/50ms window, matching
  this reconstruction's own running-status-compression behavior.
- **`resolve_heap_handle()`'s formula is a re-derivation, not a shared
  call to the project's own already-existing `local_heap_region()`**
  (see midi_out_port_serial.cpp's own header comment for why) --
  algebraically identical per both functions' own confirmed
  disassembly, but if `CSTGHeapManager`'s live-boot "captured value"
  workaround (documented in oa_heapmanager.h/setup_global_resources.cpp)
  is ever found to apply to THIS call path too (Activate() running
  before heap state is fully live at boot), this function would need
  the same workaround. Not expected (Activate() is presumed to run well
  after heap bring-up, unlike the sec 10.219-10.221 CSTGEngine-ctor
  timing this workaround was built for), but not independently
  confirmed against a live boot trace either.

---

## CKorgUsbAudioDriverMidiPorts / CSTGMidiOutPortKorgUsb — KorgUsb MIDI transport (new cluster, 2026-07-25)

Real-hardware-verification uncertainty for the KorgUsb-composite-USB-
audio-interface MIDI transport (`src/engine/midi_korgusb_port.cpp`):

- **Not reachable from any Korg USB hardware in this VM-substitution
  effort at all.** This entire cluster's actual I/O happens through a
  companion module (`KorgUsbMidiInitialize`/`Initialized`/`Done`,
  `KorgUsbRealtimeMidiOutput{,CanSend}`, `KorgUsbMidiOutput{,CanSend}`)
  that OA.ko calls but this project does not implement -- the existing
  `reconstructed/KorgUsbAudioVirtualDriver/` project's own
  `korgusbaudio_stub.cpp` implements a DIFFERENT, smaller symbol set
  (`KorgUsbAudioInitialize`/etc, the AUDIO side) and, for the 3
  MIDI-specific symbols it DOES already stub
  (`KorgUsbMidiInitialize`/`Initialized`/`Done`), does so with a
  confirmed WRONG argument count (zero args, vs this file's
  disassembly-confirmed real ABI of `idx`/`idx,bufA,bufB,userdata`/
  `idx`). This is a real ABI mismatch in a separate, already-complete
  project -- flagged here, NOT fixed (out of this task's scope; see
  midi_korgusb_port.cpp's own file-header comment for the full
  derivation). A real-HW test: with an actual Kronos and a USB-MIDI
  accessory / class-compliant MIDI-over-USB peer connected, confirm
  whether `KorgUsbMidiInitialize()` is ever actually called with a
  live USB MIDI endpoint present (this project cannot exercise that at
  all without a real or virtually-responding companion module matching
  the real ABI).
- **`STGMidiOutPortKorgUsb_OutputThread()`'s own body is disassembly-
  transcribed but never actually EXECUTED by any KAT** (an unbounded
  kernel thread loop -- this project's established convention for
  daemon-thread bodies, matching `test_daemon_lifecycle.cpp`'s own
  treatment of `SetupDaemon`'s spawned thread entry points). Its
  individual real kernel primitives (`daemonize`/`stg_sched_scheduler`/
  `prepare_to_wait`/`schedule_timeout`/`finish_wait`/`complete`/
  `complete_and_exit`) are each independently confirmed via raw
  disassembly and exercised in isolation by the surrounding
  `Initialize()`/`Done()`/`ScheduleFromRTAI()`/`ScheduleFromLinux()`
  KATs, but the thread body's own control flow (the `sSRQPending`/
  `sLinuxPending`/`sThreadKeepRunning` 3-flag state machine, including
  the confirmed-real "`sLinuxPending` is set but never cleared, so the
  thread free-runs on every timer tick forever once it's ever fired
  once" quirk) has not been exercised end-to-end. A real-HW test: with
  the ABI-mismatch above independently resolved, trigger a burst of
  MIDI-out traffic large enough to exceed the companion module's
  `CanSend()` throttle at least once, then confirm the thread keeps
  draining the ring on a ~4-jiffy cadence afterward rather than only
  reacting to further explicit `ScheduleFromRTAI()`/`ScheduleFromLinux()`
  calls.
- **UPDATE (2026-07-25 batch): `CSTGMidiInPort::Activate(CSTGMidiQueue*)`/
  `Deactivate()` are now REAL** (src/engine/midi_in_port_serial.cpp),
  and the companion-module ABI mismatch flagged above is now FIXED
  (`reconstructed/KorgUsbAudioVirtualDriver/korgusbaudio_stub.h/.cpp`).
  Both prior caveats in this entry are resolved. What's still open: the
  fix has ONLY been host-KAT-verified (mocked heap/CPU-info/queue-init
  dependencies, see test_midi_korgusb_port.cpp's own header comment) --
  a real-HW test still needs an actual USB-MIDI-capable KorgUsb
  companion connected to confirm the fixed ABI actually round-trips
  correctly end-to-end (idx values, buffer sizes, userdata pointer) once
  a real (not stubbed) `KorgUsbAudioDriver.ko`-equivalent is present.
- **UPDATE (2026-07-25 batch): the embedded `CSTGExtMIDIClockSync`
  sub-object at `CSTGMidiInPort`+0x108 is now largely REAL** (10 of 13
  confirmed methods, oa_engine_init.h/midi_clock_sync.cpp) -- the vtable
  IS now installed by the ctor. Still open, deliberately deferred (NOT a
  guess -- each was fully disassembled and found genuinely disproportionate
  to reconstruct by hand this batch):
  - `ProcessClock()` (`.text+0x68650`, 174 bytes) reads an 8-entry,
    12-byte-stride incoming-clock timestamp ring at fieldAt(0x40) whose
    OWN producer is not yet identified anywhere in this project (the
    ctor's own `*(byte*)(this+0x148)=1` write -- fieldAt(0x40)'s first
    byte -- is also still unreproduced, see midi_in_port_serial.cpp's
    ctor comment).
  - `MeasureJitter()` (`.text+0x68480`, 460 bytes) is a genuine x87
    `fucomi`/`fcmovbe`/`fcmovnbe` median-of-3 conditional-move sort over
    a 32-entry float ring -- high transcription risk without a way to
    KAT-verify against real x87 stack behavior bit-for-bit.
  - `EstimateTempoAndPredictNextClock()` (`.text+0x68130`, 737 bytes,
    the largest method in the class) was not examined in detail at all.
  None of the three (nor `ProcessClock()`'s own ring producer) are
  reachable from anything this project currently reconstructs -- a
  real-HW test isn't meaningful until whatever drives
  `CSTGMIDIClockSync`'s own external-vs-internal dispatch is itself
  reconstructed (not yet touched anywhere in this project).
- **`CSTGExtMIDIClockSync::Initialize()`'s CPU-frequency-dependent
  statics** (`kSecondsToTimeStamp`/`kTimeStampToSeconds`, from
  `CSTGCPUInfo::sInstance->khz`) are only host-KAT-verified with a
  synthetic `khz=1000000` -- never cross-checked against the REAL D510/
  D525/D2550 `stg_get_cpu_khz()` value this field is populated from at
  real boot (`engine_startup_bits.cpp`). The `unsigned int` (not the
  real code's full 64-bit-safe) conversion is provably exact for any
  real Atom-class khz value (see Initialize()'s own header comment),
  but this hasn't been confirmed against an actual captured `khz` read
  on real hardware.
- **The generic USB-MIDI-class accessory hierarchy** (`CSTGMidiInPortUSB`/
  `CSTGMidiOutPortUSB`/`CSTGUSBMidiAccessoryMidiInPort`, survey's own
  "hierarchy 2") was investigated for real this batch (previously only
  surveyed): genuinely large and disproportionate, NOT a size-based
  guess -- `CSTGMidiInPortUSB::ReceivePacket()` alone is 1287 bytes
  (`.text+0xf7500`), `CSTGMidiOutPortUSB::ProcessRegularMessage()` is
  394 bytes, and the cluster also pulls in an entirely separate
  744-byte global-ctor-keyed static singleton
  (`sUSBMidiAccessoryMidiInPort`) plus a drum-pad-client notification
  hierarchy (`CSTGDrumPadClient::ReceiveNotification()`, 1065 bytes,
  `CUSBMidiAccessory_DrumPadClient`/`CUSBMidiAccessory_MidiInClient`
  vtables). Only `CSTGMidiInPortUSB::ReceivePacket()` is
  forward-declared (type-checking only). Deliberately NOT pursued this
  batch -- a whole separate future session's worth of work, bigger than
  the KorgUsb transport cluster already reconstructed.
- **NEW (2026-07-25 batch): `CSTGCalibrationMsgHandler`** (front-panel/
  keybed analog-controller calibration state machine -- JSX/JSY
  joystick, vector joystick, touch screen, ribbon controller,
  half-damper pedal, aftertouch), `src/init/calibration_msg_handler.cpp`.
  All 24 real methods reconstructed and host-KAT-verified (19
  scenarios, `verify/test_calibration_msg_handler.cpp`); decompile
  fidelity spot-checked against raw `objdump -dr` for 6 of the 24
  functions (see file header for the list), including the real
  18-entry `.rodata` jump table behind `HandleKeybedCalibrationResult`.
  Genuinely unverified against real hardware:
  - `sCalibrationOp` values 0x1/0x2 (real, confirmed by the jump
    table's own distinct entries for them) have no setter anywhere in
    this project -- plausibly written by `CSTGKeybedInterface`'s own
    not-yet-reconstructed serial-receive ack path before it calls
    `HandleKeybedCalibrationResult`. The state-1/state-2 REPLY behavior
    is reconstructed faithfully; what actually DRIVES the transition
    into those two states is not.
  - The half-damper polarity auto-detect timing thresholds (0x1d/0x1e
    "ticks", per `GetSTGTickCount()`) are reproduced verbatim from the
    disassembly but never exercised against a real half-damper pedal's
    actual sample-arrival cadence -- the host KAT drives
    `GetSTGTickCount()` synthetically.
  - `PushMessage` (the "solicited command reply" sibling of the
    already-real `PushUnsolicitedMessage`) is declared extern only, not
    reconstructed -- this cluster's 12-byte reply packets are built
    correctly but never actually delivered anywhere in a real boot; a
    real-HW round-trip test isn't meaningful until `PushMessage` itself
    (or whatever `/proc/.oacmd`-style consumer reads its output) is
    reconstructed.
  - No caller anywhere in this project currently reaches any of these
    24 functions (the real dispatch path is presumably a generic
    `*MsgHandler`-family message router this project hasn't
    reconstructed) -- confirmed correct in isolation via KAT, not yet
    confirmed reachable end-to-end from a real front-panel calibration
    menu button press.
  the KorgUsb transport cluster already reconstructed.

- **NEW (2026-07-25 batch): `CUUID::ConvertFromText`** (`src/auth/cuuid_convert.cpp`)
  and **`CSTGMultisampleBankManager::AccessBank`**
  (`src/auth/multisample_bank_access.cpp`) -- these two were the actual
  gate keeping the ALREADY-fully-reconstructed `/proc/.oacmd` `LM:`/`LD:`/
  `CM:`/`CD:`/`CL:` command handlers (`process_oacmd.cpp`, sec 10.x prior
  batch) from ever doing anything: `ConvertFromText` previously always
  returned `false` (every UUID "failed to parse") and `AccessBank`
  previously always returned null (every bank "not found"), so every one
  of those five commands unconditionally reported failure regardless of
  what a real caller sent. Both are now real, disassembly-confirmed
  bodies (full byte-for-byte walk of `.text+0x46570`/1425B and
  `.text+0x3dce0`/135B respectively) and host-KAT-verified
  (`verify/test_cuuid_convert.cpp`, 7 scenarios; `verify/test_multisample_
  bank_access.cpp`, 5 scenarios). Genuinely unverified against real
  hardware / not yet closed:
  - `AccessBank`'s own real callee `FindBankRecord` (`.text+0x3da30`,
    661 bytes -- a genuine hash-table walk over `CSTGMultisampleBankHashList`,
    itself backed by another 341-byte `AccessBankRecord`) is deliberately
    deferred (safe stub returns null), so the "found via UUID hash
    lookup" path of `AccessBank` still can't actually locate a bank yet --
    only the ROM-bank fast path (UUID == the internal `kROMBankUUID`
    constant) is live. `kROMBankUUID`'s real byte content is never
    written anywhere in this reconstruction (its would-be writer,
    `StartupInitializeROMBank`, is itself a deliberately deferred no-op
    stub, `load_global_resources.cpp`), so it is currently all-zero here
    -- meaning the ROM-bank fast path can only match a real caller who
    also happens to pass an all-zero UUID, not yet a genuine "this is the
    factory ROM bank" identity check. End-to-end `AU:`/`LM:` command
    testing against a real Kronos (front panel or `/proc/.oacmd` shell
    write) is the natural real-HW confirmation once `FindBankRecord`
    and/or `StartupInitializeROMBank` are also reconstructed -- not
    meaningful yet on their own.
  - `CUUID::ConvertFromText`'s real kernel dependencies (`_ctype`,
    `simple_strtoul`) are both confirmed genuine `U` kernel exports in
    ground truth OA.ko and now appear as new unresolved symbols in this
    project's own OA.ko too (`nm -u`: 116 -> 118) -- expected and
    unavoidable, not a regression, but never exercised against the real
    kernel's actual `_ctype` table contents (only a host-side mock table

- **NEW (batch 65): `CSTGControllerInfo::AnalogControllerHandler`**
  (`src/engine/controller_info_analog_handler.cpp`) -- the real physical
  knob/slider/joystick/ribbon/vector/aftertouch/value-wheel/damper move
  dispatcher. This is genuinely REAL physical front-panel I/O that
  currently does nothing on real hardware unless/until this module is
  deployed (the prior stub was a deliberate no-op, safe only for
  no-panel VM boot testing -- see bar2_stubs.cpp's own updated comment).
  Structural dispatch (range checks, busy/edit-in-context gating,
  per-mode table selection, message constants) is disassembly-confirmed
  via `objdump -dr` + a `readelf -rW` relocation dump of all four
  `.rodata` dispatch tables -- high confidence, not guessed. Genuinely
  unverified against real hardware / open items:
  - The 22 real per-controller `AnalogXxxHandler` methods this function
    dispatches to are ALL still deliberately deferred externs (own
    bodies not reconstructed) -- so even once this dispatcher is
    correct, nothing downstream of it does anything real yet on
    hardware. Confirmed real via relocation, not yet load-bearing.
  - THREE sub-branches (tempo-curve, SetListEQ-curve, and the default
    effect-rack front-panel-smoother edit, all reachable only from the
    "Value" knob/jog-wheel in specific edit modes) are deliberately left
    as local no-op stubs (DSP-adjacent, not traced this pass) -- turning
    the physical Value wheel while editing tempo, an EQ band, or an
    effect-rack parameter will currently do nothing at all on real
    hardware, not even a partial/wrong effect. See this file's own
    header comment for exact ground-truth address ranges for a future
    pass.
  - The 8 weak-undefined `AnalogXxxT18/T916/A18/A916Handler` slots
    (4 knob + 4 slider assignment modes) are believed permanently
    unreachable on real hardware (no writer of the controlling mode byte
    to those specific values found anywhere in this project so far), and
    this reconstruction faithfully reproduces the real binary's own
    "resolves to a null-pointer call if ever reached" behavior rather
    than inventing a safe no-op -- if this belief is ever wrong, a real
    unit would crash (kernel Oops) turning a knob/slider into one of
    these 4 modes. Worth a deliberate real-hardware "try to select every
    knob/slider assignment mode via the UI" sanity pass before this
    module is trusted on a real unit, specifically to confirm these 4
    modes truly are unreachable through the real UI.
  - `ButtonPressHandler` (the sibling per-BUTTON dispatcher, 5822 bytes,
    ~144-entry action table) is still the deliberate no-op stub it always
    was -- every physical button press still does nothing at all on real
    hardware. Its own confirmed shape/table addresses are documented in
    oa_global.h's own comment and the `oa_front_panel_analog_button_
    handlers` agent-memory note for a future dedicated batch.
  - Device-code-to-symbol-name mapping (e.g. which physical connector is
    "RibbonX" vs "RibbonZ", which UI echo offset belongs to which
    control) is inferred from table POSITION and the real (already
    human-readable) symbol names alone -- not independently cross-checked
    against a real unit's actual wiring/silkscreen labels.
  - `CSTGCCInfo::sCCInfoTable`'s per-entry byte-0 "default/current value"
    semantics (used by the mode==4/busy2 CC-lookup path) are reused from
    an EARLIER pass's own confirmed derivation (oa_global.h's
    `CSTGCCInfo` comment) -- not re-verified independently in this batch.
    covering the two relevant bits was tested here).

---

## CSTGControllerInfo::ButtonPressHandler — REAL now (batch 66); six new deferred callees + ~23 un-traced sub-branches

`ButtonPressHandler` (`src/engine/controller_info_button_handler.cpp`) is
the real physical front-panel BUTTON press/release dispatcher -- this is
genuinely REAL hardware I/O that previously did nothing at all (the prior
stub was a deliberate no-op). Structural dispatch (all THREE `.rodata`
jump tables -- the batch-65 characterization above only found two, this
pass found a third release-side table it missed -- every `(msgType,
buttonId)` immediate pair, every busy/mode/bitmap flag check) is
disassembly-confirmed via `objdump -dr` plus a full byte-level dump of
all three tables' 214 raw dword slots, cross-checked against every
individual jump target's real code. High confidence, not guessed. Real
button-numbering-to-physical-control mapping (which `eSTGButtonCode`
value is which silkscreened front-panel button) is NOT independently
confirmed against a real unit -- only the `(msgType, buttonId)` pairs and
control-flow shape are ground-truthed; this reconstruction never had to
determine "button 0x2c is the Combi button" or similar to be correct.

Genuinely unverified against real hardware / open items:

  - Six new real sibling methods (`HandleEditInContextButton`,
    `ProcessMixerSwitchPress`, `SetMixerKnobMode`, `SetSoloSelected`,
    `ResetAllKnobCCs`, `ResetAllExtModeControllers`) plus a weak-undefined
    `NotifyParam(unsigned int, long)` overload are all still deliberately
    deferred (own bodies not reconstructed) -- so pressing one of the 12
    "special" buttons or one of the 16 "mixer switch" buttons currently
    calls into an unresolved OA.ko-internal symbol on real hardware
    (matches the real binary's own unresolved-symbol shape at this stage
    of the project, not a crash risk on its own -- these become genuine
    `insmod` blockers only once every OTHER unresolved symbol is also
    resolved).
  - Table 2 "Pattern B" (10 codes: 0x26-0x2b, 0x2f-0x32) has 20 individual
    deferred sub-branches (2 per code -- a `!pressed` release action and a
    `busy-flag-clear` action), each with its own real ground-truth address
    but none traced this pass (see `controller_info_button_handler.cpp`'s
    own `HandleButtonPatternBDeferred()` comment for the full 20-address
    table). Pressing/releasing these 10 buttons in anything other than
    "already in busy/UI-edit mode, holding it down" currently does
    nothing on real hardware.
  - The 12 "special" buttons (codes 9, 0x2c, 0x35-0x39, 0x4a-0x4e) have 14
    press-time and 3 release-time deferred sub-branches (mostly the
    "busy/busy2 flag clear" else-paths of an if/else the real code
    guards these buttons' main actions with) -- see the `ButtonDeferred_
    0xNNNNN()` stub functions' own individual comments for exact
    addresses and the little that IS confirmed about each gate. None of
    these are DSP in the audio-engine sense (this cluster is entirely
    front-panel UI/mode-switching state, in scope per this project's
    stated goal) -- just genuinely un-traced this pass given the size of
    the rest of the reconstruction.
  - The real byte at `CSTGControllerRTData::sInstance+0x2f` bit `&4`
    (cleared unconditionally by button 0x39's release handler, distinct
    from the `&2`/`&8` bits this project has already named "busy2"/
    "busy") has no confirmed semantic meaning of its own -- transcribed
    faithfully from the disassembly, not interpreted.
  - The exact real semantics of the per-button "active" bitmap at
    `CSTGControllerRTData::sInstance+0x30` (one bit per `eSTGButtonCode`)
    are inferred purely from its set/clear/test call sites in this
    function (a plausible "is this button currently held down"
    latch) -- not independently confirmed against any other real OA.ko
    code path that reads it.
  - `HandleEditInContextButton`'s real gating condition (`CSTGGlobal::
    sInstance+0x29cc4dc != 0`) is the SAME field this project's other
    "edit in context" gates already use (`AnalogControllerHandler`,
    `SendUnsolicitedUIParam`) -- reused with high confidence, not
    independently re-derived for buttons specifically.

Real-HW test that would help once the six deferred callees above are also
reconstructed: press every physical front-panel button (including
mode/mixer buttons) and confirm the real `SendUnsolControl2MessageToUI`
traffic (observable via `/proc/.oacmd` or a UI-side log) matches this
reconstruction's `(msgType, buttonId)` table for each one.

---

## CSTGFileOpener/CSTGFileCloser/CSTGHDRFileWriter::ProcessCommands() — no real disk-I/O timing modeled (2026-07-25)

Uncertain: this reconstruction pass covers the pure ring-buffer bookkeeping
and command-tag dispatch of three of the five file-daemon
`ProcessCommands()` methods (drain-a-ring, dispatch-by-tag-byte, push a
follow-on record into a sibling daemon's ring). It intentionally does NOT
model anything about the REAL underlying SSD/flash I/O these commands are
presumably orchestrating -- the dispatched-to vtable slots on the
"payload" object (confirmed real call targets: `CSTGFileOpener` tag 0/1
via slots 2/4, `CSTGFileCloser` tag 0 via slot 3, `CSTGHDRFileWriter`
tag 2 via slot 6) are themselves still unreconstructed/opaque, so this
pass cannot say anything about:

- Whether the real per-command latency (actual `open()`/`read()`/`write()`/
  `fsync()` against the Kronos's own SSD, presumably issued from whatever
  these vtable slots ultimately call) ever causes the ring to back up
  faster than `RunFileDaemonSynchronization()`'s own polling cadence can
  drain it -- this reconstruction's ring-drain loop has no bound on
  iteration count per call (drains until producer==consumer), which is
  faithful to the real disassembly, but real disk stalls could make a
  single call to one of these three methods take much longer than a VM's
  instant-return mock I/O ever would.
- The real capacity values transcribed from `Initialize()` (`CSTGFileOpener`
  0x1069 entries/33608 bytes, `CSTGFileCloser` two independent 0x11fa-entry/
  0x8fd0-byte rings, `CSTGHDRFileWriter` 0x129 entries/0x948 bytes) are
  confirmed exactly from the real binary's own `AllocAligned` call sites,
  but whether those sizes are comfortably oversized for real streaming/HDR
  recording workloads on real flash, or whether the real hardware has
  actually been observed hitting ring-full conditions (this project's own
  already-documented "silent overwrite on double overflow" quirk for
  `CSTGFileOpener::AddPlaybackEvent`/`AddRecordEvent`, batch 51), was not
  and cannot be determined from disassembly alone.
- `CSTGHDRFileReader`/`CSTGStreamingFileReader::ProcessCommands()` remain
  unreconstructed specifically because their own dispatch goes through a
  not-yet-recovered `TSTGArrayManager<T>::indexArray` lookup table --
  since that table's real populated contents are unknown, this project
  also has zero visibility into whatever timing/ordering guarantees (if
  any) those two methods' own real command types assume about the
  underlying storage.

Real-HW test that would help: with the vtable dispatch targets on these
"payload" objects eventually reconstructed too, trigger a real HDR
recording/streaming-playback session on physical hardware while an SD
card or a deliberately slow SSD is installed, and watch (via a debug
counter or ring-fill-level log) whether `RunFileDaemonSynchronization()`'s
polling cadence ever lets any of these five rings approach full under
real I/O latency -- would validate or refute the "these ring sizes are
comfortably oversized" assumption above.
