# OA.ko Reconstruction — Session Summary, 2026-07-25

Autonomous RE session against the goal "reverse engineer all of OA.ko." 37 commits
landed today under `reconstructed/OA/` (25 code-reconstruction commits, 7
`HARDWARE_REVIEW_LOG.md`-only documentation commits, plus doc/tracking commits for
new batches). This document groups the work by subsystem and summarizes what's
still open. Full per-item detail lives in `HARDWARE_REVIEW_LOG.md`; this is the
skim version.

Method/branch counts below are cross-checked directly against the source files
(`grep`-counted method definitions), not taken on faith from commit messages.

---

## Front panel: LED / key / button / analog-controller dispatch

The single largest cluster of work today — this is genuinely real physical
front-panel I/O that previously did nothing (deliberate no-op stubs) and now
dispatches for real.

- **`CSTGControllerInfo::AnalogControllerHandler`** (`33f6d83`) — the real
  knob/slider/joystick/ribbon/vector/aftertouch/value-wheel/damper dispatcher.
  Structural dispatch (range checks, busy/edit-in-context gating, all four
  `.rodata` dispatch tables) is disassembly- and relocation-confirmed. Of its
  22 real per-controller `AnalogXxxHandler` callees:
  - 9 reconstructed for real in `1f7f8ae` (batch 65: RibbonX, VectorX, VectorY,
    Damper, and others).
  - 6 more in `65b4ce3` (batch 67, "closes cluster").
  - 6 more in the batch-68 work referenced in `HARDWARE_REVIEW_LOG.md`
    (JoystickX, Aftertouch, KnobRTK — pitch-bend double-`Filter()` call, MIDI
    terminator-byte inference, an echo-offset naming collision between two
    unrelated call sites, and a GCC `.clone.11` call-site quirk are all flagged
    there as open items, not bugs).
  - **15 of 22 are now real; 7 remain deferred externs** (own bodies not yet
    reconstructed), plus three Value-wheel edit-mode sub-branches
    (tempo-curve, SetListEQ-curve, effect-rack smoother) deliberately left as
    no-ops (DSP-adjacent, out of this pass's scope).
  - 8 weak-undefined `AnalogXxxT18/T916/A18/A916Handler` knob/slider-assignment
    slots are believed permanently unreachable through the real UI (no writer
    of the controlling mode byte found anywhere in the project) — faithfully
    reproduced as a null-pointer-call crash if ever hit, not given a safe
    fallback, matching ground truth.

- **`CSTGControllerInfo::ButtonPressHandler`** (`6595477`) — the real physical
  button press/release dispatcher, all three `.rodata` jump tables (214 raw
  dword slots) traced and cross-checked against jump targets.
  - All 37 previously-deferred sub-branches (20 "Pattern B", 17 special-button
    else-branches) reconstructed for real in `bae0928`.
  - Six sibling methods (`HandleEditInContextButton`, `ProcessMixerSwitchPress`,
    `SetMixerKnobMode`, `SetSoloSelected`, `ResetAllKnobCCs`,
    `ResetAllExtModeControllers`) plus new deferred externs
    (`ProcessPerfSwitchPress`, `ResetSolo`, `ChangeControlSurfaceMode`) remain
    unreconstructed — pressing the 12 "special" buttons or 16 "mixer switch"
    buttons currently calls into an unresolved symbol.
  - Real button-numbering-to-physical-silkscreen mapping (which `eSTGButtonCode`
    is which physical button) is NOT independently confirmed — only the
    `(msgType, buttonId)` pairs and control flow are ground-truthed.

- **`CSTGFrontPanelMsgHandler` + `CSTGFrontPanel::Beep`/`SetLED16Bits`**
  (`441d54a`) — found via a class-level `nm -C` sweep, wholly unclaimed
  before this pass. `SetLED16Bits` provably drops one byte (bits 8-15) of its
  32-bit input when repacking the OMAP NKS4 command word (verified
  instruction-by-instruction, not a transcription slip).

- **`CSTGOmapNKSMsgHandler::ProcessNextNKSEvent`** (`b80125c`) — the real
  USB-NKS4-panel event pump; ties together four already-real
  `CSTGFrontPanel::Handle*` dispatch targets that had no reconstructed caller
  before this pass. A real, confirmed byte-order asymmetry exists between the
  "normal" event families (`(b1<<8)|b0`) and the "diagnostic" 0x61/0x62
  families (`(b0<<8)|b1`).

- **`TurnOnSeqLed` / `SKSTGGate_*` / `SPROutGate_*` transport LEDs**
  (`8786981`) — Rec/Pause/FF/Rew LEDs are real; Start/Stop-Red/Green are a
  **confirmed real no-op** in this function (re-traced twice to rule out a
  transcription mistake) — those two LEDs are driven through a separate,
  not-yet-reconstructed path (`CSTGTempoUtils::FlashStartStopLed`/
  `FlashTempoLed`).

- **`CSTGFrontPanel` LED + physical-key handlers** (`a498ad0`) and
  **`CSTGKeybedInterface::SetLED`** (part of the batch-64 keybed cluster below)
  round out the front-panel LED-control surface.

---

## Keybed wire-protocol driver

- **`CSTGKeybedInterface`, ~20-method wire-protocol driver class** (`4ade7f1`,
  batch 64) — full objdump disassembly + relocation trace of
  `OA_real.ko`'s `.text+0x33d380`..`0x33e800`. Confirmed real this batch:
  `SetLED` (gates on `KEYBED_OFF_STATE>=2`; maps `code==0x49/0x4a` to LED
  index 0/1 — which physical LEDs these are is still unconfirmed),
  `SetKeyChatterGateTime`, `ReceiveMessage`/`WriteMessageToQueue` gating,
  `FilterAnalogController` (joystick X/Y channel identity is an inference,
  not confirmed), `HandleActiveSense` nibble dispatch (nibble 0xA matches the
  real production heartbeat byte `0xEA` observed live on hardware), and the
  member-function `MemberStartup`/`MemberCleanup`/`TryComPort` trio (no
  confirmed caller).
- **Deliberately NOT reconstructed this pass**: `ApplyKeybedCalibration`
  (genuine kernel-mode FPU context switch, `.text+0x33edd0`) and
  `ProcessNextKeybedEvent` (1712 bytes, by far the largest method in the
  class — depends on five entirely unmodeled classes/functions:
  `PushUnsolicitedMessage`, `CSTGDelayedMsg`, `CSTGControllerInfoUnsolMsg::
  Send()`, `USTGKeyTouchTable::Convert9bitCountsTo8bitInterval()`,
  `CSTGCalibrationMsgHandler::HandleKeybedCalibrationResult()`). Net effect:
  the raw wire protocol is now correctly framed/enqueued, but nothing yet
  drains the queue into musical/UI events.

---

## MIDI: serial UART, KorgUsb transport, USB-MIDI accessory, clock sync

- **`CSTGMidiInPortSerial`** (`a4813e8`) — physical DIN-MIDI-IN UART byte
  parser (running-status/channel/system-common path). New discovery: a
  realtime-message timestamp ring (8×12-byte entries) carved out of a
  previously-undifferentiated blob — plausible MIDI-clock-jitter diagnostic,
  unconfirmed. `StartSysEx()`/`ReceiveSysExData()` deliberately deferred
  (real SysEx dumps would currently be silently dropped). No known live
  caller (UART RX ISR not yet reconstructed anywhere in the project).
- **`CSTGMidiOutPort`/`CSTGMidiOutPortSerial`** (`858f30e`) — physical
  MIDI-OUT UART driver. **Notable finding**: the hardware transmit path is
  *provably dead in this exact firmware image* — `CanTransmitHardware()`/
  `TransmitHardwareByte()` resolve to `__cxa_pure_virtual` in OA.ko's own
  vtable relocations, confirmed via `readelf -r`, not inferred; no deriving
  class exists anywhere in the binary. Physical DIN MIDI-OUT may genuinely
  never transmit on real hardware, or a companion module patches the vtable
  at runtime — the two are statically indistinguishable.
- **`CKorgUsbAudioDriverMidiPorts`** (`8f3f1c6`) — the KorgUsb-composite-USB
  MIDI transport. Depends on a companion module (`KorgUsbMidiInitialize`
  etc.) this project does not implement; flagged (and later fixed, see
  below) a real ABI mismatch in the separate
  `reconstructed/KorgUsbAudioVirtualDriver/korgusbaudio_stub.cpp` project
  (wrong argument count on the 3 MIDI-specific stub symbols).
- **`CSTGMidiInPort::Activate()`/`Deactivate()` + `CSTGExtMIDIClockSync`**
  (`753715e`) — both now real; the KorgUsb ABI mismatch above is fixed in the
  companion stub file. 10 of 13 `CSTGExtMIDIClockSync` methods are real; the
  three largest (`ProcessClock`, 174B; `MeasureJitter`, 460B, a genuine x87
  `fucomi`/`fcmovbe` median-of-3 sort; `EstimateTempoAndPredictNextClock`,
  737B, the largest method in the class, not examined at all) remain
  deliberately deferred — documented, not fixed, in `f00725f`.
- **Generic USB-MIDI accessory Activate/Deactivate plumbing** (`73a3b7d`,
  `6ddcad3`) — `CSTGUSBMidiAccessoryMidiInPort`/`CSTGMidiOutPortUSB`. Same
  "not reachable without a companion module" situation as KorgUsb; two
  `__cxa_pure_virtual` dead ends (`CanSendRealTime`/`CanSendRegular`/
  `SendRealTime`/`SendSingleByte`) confirmed real dead code in ground truth
  itself, not a gap in this reconstruction. The larger generic USB-MIDI-class
  hierarchy (`CSTGMidiInPortUSB::ReceivePacket`, 1287B; a 744-byte
  drum-pad-client notification hierarchy) was surveyed but deliberately not
  pursued — a separate future session's worth of work.

---

## Calibration and power management

- **`CSTGCalibrationMsgHandler`** (`bc06fdd`) — the front-panel/keybed
  analog-controller calibration state machine (JSX/JSY joystick, vector
  joystick, touch screen, ribbon, half-damper, aftertouch). All 24 real
  methods reconstructed and host-KAT-verified (19 scenarios); decompile
  fidelity spot-checked against raw `objdump -dr` for 6 of the 24 functions,
  including a real 18-entry `.rodata` jump table behind
  `HandleKeybedCalibrationResult`. What actually drives the transition into
  `sCalibrationOp` states 0x1/0x2 has no identified setter in this project;
  `PushMessage` (the reply-delivery sibling of the already-real
  `PushUnsolicitedMessage`) is declared extern only.
- **`SCalibrationData::InitAll()`** (`6f024f1`) — compiled-in calibration
  defaults. Most field groups match offsets this project already established
  elsewhere; the 0x00-0x1f "generic curve table" and 0x9c-0xbc "touch screen"
  groups have no independent cross-reference. A known, deliberately-left-open
  structural discrepancy: ground truth falls through into
  `SetupNKS4Calibration`-adjacent code on *both* the calibration-loaded and
  calibration-missing paths; this reconstruction currently only does so on
  the loaded path.
- **`CPowerOffTimer`** (`c2835de`) — remaining 7 methods reconstructed. Three
  of `DoTimerTick()`'s four tick-suppression gate fields have no
  independently-confirmed name (best-guess: an in-progress calibration/modal
  gate, and an unidentified transient system mode at `CSTGGlobal+0x6a8`).
  `DoTimerTick()` itself has no confirmed real periodic-timer caller yet.

---

## Front-panel/remote system control: `CSTGControlMsgHandler`

- **`CSTGControlMsgHandler`, 51 methods** (`c96bd4d`) — reconstructed in
  full (verified by direct grep of the source: 51 methods plus the
  constructor). Covers audio mute, LCD brightness, ErP power-management
  handshake, program/mode-change handlers, CPU/FX/disk usage peak readers,
  I2C read/write, and more. Notable finding: **`MuteADC` actually mutes the
  audio *outputs* in ground truth**, not the inputs (confirmed via the real
  vtable slot offsets used — `MuteAudioOutputs`/`UnmuteAudioOutputs`, not the
  Inputs pair) — reproduced faithfully, flagged as possibly a ground-truth
  naming artifact worth a real-hardware sanity check. `ReadCPUUsagePeak`
  zero-extends where this project's C++ model sign-extends (no observable
  difference for realistic non-negative values). Several newly-declared real
  callees (`ChangeBankType`, `FreeStolenVoices`, `StartDownload`/
  `EndDownload`, `CSTGDrumPadInterface::StartScanning`,
  `COmapNKS4Driver_StartScanning`) remain deliberately deferred bodies;
  `ResetAllEffectsInActivePerf` (416 bytes, real) is a full no-op stub —
  effect-rack DSP internals, out of project scope by policy.

---

## File I/O: `File*::ProcessCommands` and streaming/HDR readers

- **3 of 5 file-daemon `ProcessCommands()` siblings** (`a7b874b`) —
  `CSTGFileOpener`/`CSTGFileCloser`/`CSTGHDRFileWriter`. Pure ring-buffer
  bookkeeping and command-tag dispatch; models nothing about real SSD/flash
  I/O latency behind the dispatched-to vtable slots (still opaque). Real
  ring capacities confirmed exactly from `Initialize()`'s `AllocAligned`
  call sites (e.g. `CSTGFileOpener`: 0x1069 entries / 33608 bytes).
- **`CSTGHDRFileReader`/`CSTGStreamingFileReader::ProcessCommands()`**
  (`c529327`) — the remaining 2 of 5. This **revises a long-standing
  "blocked by an unrecovered `TSTGArrayManager<T>::indexArray`" verdict**
  (batch 28 onward): the real per-command dispatch turned out to be
  per-instance `{funcptr,adj}` pairs baked into each object's own data by
  its ctor, fully disassembly-recoverable after all. Also reconstructed:
  `CSTGStreamingEventManager::ReturnFreeEvent()`/`CSTGStreamingEvent::
  CloseFileDescriptorsIfNecessary()`/`HandleErrorReading()`. Open items:
  `field14c3c`'s exact purpose (plausible "pending wake signal", unconfirmed)
  and the `+0xb8`/`+0xc4` dword-pair semantics on `CSTGStreamingEvent`
  are both unconfirmed beyond their observed single-purpose use.

---

## Auth/misc: `CUUID` and multisample bank access

- **`CUUID::ConvertFromText` + `CSTGMultisampleBankManager::AccessBank`**
  (`ee0a93f`) — these two were the actual gate keeping the *already*
  fully-reconstructed `/proc/.oacmd` `LM:`/`LD:`/`CM:`/`CD:`/`CL:` command
  handlers from ever doing anything: `ConvertFromText` previously always
  returned `false`, `AccessBank` previously always returned null. Both are
  now real (full byte-for-byte walk of `.text+0x46570`/1425B and
  `.text+0x3dce0`/135B), host-KAT-verified (7 + 5 scenarios). `AccessBank`'s
  own callee `FindBankRecord` (661-byte hash-table walk) remains a
  deliberately-deferred stub, so only the ROM-bank fast path is currently
  live — and `kROMBankUUID` itself is still all-zero since its would-be
  writer (`StartupInitializeROMBank`) is also a deferred no-op.

---

## Stub sweep, batches 57-63

Smaller `bar2_stubs.cpp` cleanup pass, in order:

| Batch | Commit | What |
|---|---|---|
| 57 | `9d3be30` | MIDI port teardown, audio-solo dispatch, jump-catch reset |
| 58 | `2bc8798` | `CSTGAudioInputMixerBase` ctor, `CSTGAudioInputMixer`/`CSTGMasterLRMixer::Initialize`, `SetSendBuses` |
| 59 | `1bcc581` | `CSetListEQ::Initialize` (9-band EQ coefficient table) |
| 60 | `9f07288`, `ba9f968` | `CSTGControllerInfo::SendUnsolicitedUIParam` (4-arg); two investigated-but-deferred functions documented |
| 61 | `baa1dfa` | `CSTGProgramBank::Initialize`/`GetPatchSize` |
| 62 | `5795e39` | `CSTGSmoother::FinalizeAllSmoothers` |
| 63 | `7628585` | `CSTGFileOpener::Initialize()` |

Note on `CSTGControllerRTData::ResetSendKnobsJumpCatch` (batch 57): cases 2/3
read two unnamed `CSTGGlobal`-relative tables not exercised by this batch's
own KAT. Note on `CSTGControllerRTData::SetAudioInSolo` (batch 57): confirmed
pure-virtual dispatch resolving to `__cxa_pure_virtual` — assumed genuinely
dead/unreachable on real hardware too, not just in this reconstruction.

---

## What's still open (biggest items)

- **`CSTGKeybedInterface::ProcessNextKeybedEvent`** — the actual
  ring-to-event drain is entirely unreconstructed; keybed/joystick/aftertouch
  events are framed and enqueued but never turned into anything musical yet.
  Five dependent classes/functions are completely unmodeled.
- **`ApplyKeybedCalibration`** — real kernel-mode FPU context switch, not
  reconstructed; blocks confidence in `FilterAnalogController`'s calibration
  math (control flow is validated, math is not).
- **MIDI-OUT UART hardware transmit is provably dead code in this firmware
  image** (`__cxa_pure_virtual`, no override anywhere in the binary) — needs
  a real-hardware test to confirm DIN MIDI-OUT genuinely never transmits, or
  whether some undiscovered companion module patches the vtable at runtime.
- **KorgUsb MIDI transport and the generic USB-MIDI accessory hierarchy**
  are both unreachable without a real or virtual companion module; the
  KorgUsb ABI mismatch was fixed in `KorgUsbAudioVirtualDriver`, but nothing
  has exercised it end-to-end with live traffic. The larger "hierarchy 2"
  generic USB-MIDI-class accessory tree (drum-pad client, `ReceivePacket`)
  was surveyed but deliberately deferred as its own future session.
  `CSTGExtMIDIClockSync`'s three largest methods (`ProcessClock`,
  `MeasureJitter`, `EstimateTempoAndPredictNextClock`) remain unreconstructed
  and are not reachable from anything else in the project yet either.
- **`ButtonPressHandler`'s six sibling methods** (`HandleEditInContextButton`,
  `ProcessMixerSwitchPress`, `SetMixerKnobMode`, `SetSoloSelected`,
  `ResetAllKnobCCs`, `ResetAllExtModeControllers`) and **7 of 22
  `AnalogXxxHandler` callees** remain deferred externs.
- **`AccessBank`'s `FindBankRecord`** (hash-table bank lookup) and
  **`StartupInitializeROMBank`** are deferred — only the all-zero ROM-bank
  fast path of the `/proc/.oacmd` bank commands is currently live.
- **File-daemon vtable dispatch targets** (the actual SSD/flash I/O behind
  `CSTGFileOpener`/`CSTGFileCloser`/`CSTGHDRFileWriter`/`CSTGHDRFileReader`/
  `CSTGStreamingFileReader`) remain opaque — real disk-timing behavior versus
  this project's ring-capacity assumptions is untested.
- **Field-name/semantic uncertainty is broad but shallow**: a long tail of
  `CSTGControllerRTData`/`CSTGGlobal` byte/dword offsets across this
  session's work (solo bytes, edit-in-context gates, controller-lock flags,
  echo-slot offsets) are faithfully reproduced from disassembly but have no
  independently-confirmed name — real-hardware UI interaction while watching
  MIDI/UI traffic is the recurring suggested test across nearly every entry
  in `HARDWARE_REVIEW_LOG.md`.
- **Reachability**: several of today's clusters (keybed member-function
  trio, MIDI-IN/OUT serial UART, KorgUsb transport, `CSTGCalibrationMsgHandler`,
  `CPowerOffTimer::DoTimerTick`) have no confirmed live caller anywhere in
  the project yet — correct in isolation via host KAT, not yet proven
  reachable from a real boot/UI path.

Full detail, exact addresses, and the specific real-hardware test proposed
for each item above: see `HARDWARE_REVIEW_LOG.md` in this directory.
