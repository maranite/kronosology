# OA.ko Reconstruction — Session Summary (2026-07-25 through 2026-07-27)

**This document was originally written 2026-07-25 covering that day's static
reconstruction work, and rewritten (not just appended) on 2026-07-27 (HEAD
`9c587a2`) to fold in a second phase: this project's first-ever *dynamic*
(live-boot) testing of OA.ko, which found and fixed real bugs the 2026-07-25
static work could not have caught.** The title keeps the original date
because that's when this stream of work started and other docs
(`HARDWARE_REVIEW_LOG.md`, `PROJECT_BRAIN/status.md`) already link to this
filename — treat the date range in the heading above as the real scope.
Every commit hash, function name, and marker number below was independently
re-checked against current `git log`/`git show`/a live rebuild for this
rewrite, not transcribed from a prior draft or from `status.md`'s prose
uncritically — one prior verbal ordering of the 2026-07-27 fixes (see the
chronology note below) did not match `git log`'s actual timestamps and is
corrected here.

---

## 2026-07-25: static reconstruction, scoped to structural/hardware-integration code

Autonomous RE session against the goal "reverse engineer all of OA.ko." 37
commits landed under `reconstructed/OA/` (25 code-reconstruction commits, 7
`HARDWARE_REVIEW_LOG.md`-only documentation commits, plus doc/tracking
commits for new batches). Per the project's standing
`oa_ko_rtai_virtualization_policy` (audio-DSP internals out of scope; getting
the module to load and its control-plane/hardware-I/O logic to run correctly
is in scope), this pass focused entirely on physical front-panel, keybed,
MIDI, calibration, and system-control code — real hardware-facing surfaces
that had previously been silent no-op stubs.

Method/branch counts below are cross-checked directly against the source
files (`grep`-counted method definitions), not taken on faith from commit
messages.

### Front panel: LED / key / button / analog-controller dispatch

The single largest cluster of work this day — genuinely real physical
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
    unreconstructed as of 2026-07-25 — see the 2026-07-27 update below for
    what happened to these exact symbols.
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

### Keybed wire-protocol driver

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

### MIDI: serial UART, KorgUsb transport, USB-MIDI accessory, clock sync

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
  (wrong argument count on the 3 MIDI-specific stub symbols). **This class's
  own `sInstance` static-init mechanism turned out to have a real bug of its
  own — see the 2026-07-27 `.ctors`-vs-`.init_array` section below.**
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

### Calibration and power management

- **`CSTGCalibrationMsgHandler`** (`bc06fdd`) — the front-panel/keybed
  analog-controller calibration state machine (JSX/JSY joystick, vector
  joystick, touch screen, ribbon, half-damper, aftertouch). All 24 real
  methods reconstructed and host-KAT-verified (19 scenarios); decompile
  fidelity spot-checked against raw `objdump -dr` for 6 of the 24 functions,
  including a real 18-entry `.rodata` jump table behind
  `HandleKeybedCalibrationResult`. What actually drives the transition into
  `sCalibrationOp` states 0x1/0x2 has no identified setter in this project;
  `PushMessage` (the reply-delivery sibling of the already-real
  `PushUnsolicitedMessage`) is declared extern only — **given a real no-op
  stub body 2026-07-27, see below (it was one of the 44 insmod-blocking
  gaps).**
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

### Front-panel/remote system control: `CSTGControlMsgHandler`

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
  `COmapNKS4Driver_StartScanning`) remain deliberately deferred bodies as of
  2026-07-25 (stub bodies added 2026-07-27, see below);
  `ResetAllEffectsInActivePerf` (416 bytes, real) is a full no-op stub —
  effect-rack DSP internals, out of project scope by policy.

### File I/O: `File*::ProcessCommands` and streaming/HDR readers

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

### Auth/misc: `CUUID` and multisample bank access

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

### Stub sweep, batches 57-63

Smaller `bar2_stubs.cpp` cleanup pass, in order:

| Batch | Commit | What |
|---|---|---|
| 57 | `9d3be30` | MIDI port teardown, audio-solo dispatch, jump-catch reset |
| 58 | `2bc8798` | `CSTGAudioInputMixerBase` ctor, `CSTGAudioInputMixer`/`CSTGMasterLRMixer::Initialize`, `SetSendBuses` — **superseded 2026-07-27, see below: the ctor installed here had a real transcription bug (a literal `8` instead of a relocated `&vtable+8`) plus a separate never-runs-in-a-kernel-module static-init bug, both since fixed** |
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

## 2026-07-27: from static reconstruction to the project's first live-boot dynamic testing of OA.ko

Everything above was written and verified through host-side known-answer
tests (`make verify`) alone — OA.ko had never actually been `insmod`'d into a
real or virtual kernel with the front-panel/keybed/MIDI/calibration batches
present. This is the headline shift of this update: applying Eva's own
"static sweeps eventually go dry, live dynamic tracing finds what they can't"
methodology to OA.ko for the first time, in a disposable `kronosvm` instance.
It surfaced a chain of real bugs no amount of additional static review would
have caught, because each one only manifests once actual kernel-module-load
machinery (symbol resolution, relocation processing, the *absence* of
`.init_array` execution) runs against the built `.ko`.

**A chronology note, since getting this order right matters and a
non-authoritative summary of this arc floated during the day did not match
`git log`'s actual timestamps**: the real order, re-confirmed directly via
`git log --format='%ci %s'` for this rewrite, is `2c539fb` (02:24 UTC) →
`68853c2` (03:14) → `ba7f7d4` (03:23, docs) → `804b909` (04:56) → `5a1b107`
(05:15) → `13fba9f` (05:44) → `9c587a2` (05:54). The vtable-literal-8 bug
(`2c539fb`) was found and fixed *before* the insmod-blocking stub-drift fix
(`68853c2`), not after — the two were tested against different HEADs that
day (an older build for the first, current-HEAD for the second), which is
what created the confusion. The rest of this section follows the verified
order.

### 1. `2c539fb` — the literal-8 vtable/relocation bug (17th vtable-dispatch-stub-gap instance, 1st in OA.ko)

Applying Eva's dynamic-tracing methodology to OA.ko, an `insmod` test in a
disposable `kronosvm` instance (apparently the first time this project ever
actually loaded a build this complete into a real kernel) immediately hit a
kernel NULL-pointer-deref Oops inside
`CSTGAudioInputMixerBase::SetSendBuses()`'s own raw vtable dispatch.

Root cause, found via `objdump -dr` against the real ground-truth `OA.ko`:
`performance_vars_manager_init.cpp` stored the **literal integer `8`** into a
freshly-constructed `CSTGAudioInputMixer`'s own vtable-pointer field,
clobbering the real pointer its base ctor had just installed. A prior
session's header comment had called this "confirmed real, preserved
verbatim," having missed the `R_386_32` relocation attached to that exact
immediate operand in ground truth's own disassembly — the real instruction
stores `&vtable-for-CSTGAudioInputMixer + 8` (standard Itanium-ABI
"most-derived vtable wins" construction order), not a literal constant.

Fixed by reconstructing the real derived-class vtable slots —
`ShouldMute(unsigned int) const`/`GetOutputBus(int)`, both previously
completely unidentified (`GetOutputBus` indexes a newly-discovered third bus
array, `CSTGAudioBusManager::sSynthesisThreadBusSets`, 960 slots) — and
installing the real vtable pointer instead of the literal. This is the
**3rd** confirmed instance project-wide of the "`objdump` without `-r` hides
a relocation as a plausible small immediate" trap (2 earlier instances
already fixed in `init_module.cpp`'s `printk`/`rt_printk` format-string
pointer and `stg_get_current_task()`'s per-cpu displacement — re-checked and
reconfirmed still correct this same day). A dedicated follow-up sweep of
every placement-new site, every raw-integer-into-pointer-field store, and
every `_vtablePtr =` assignment project-wide found no further instances of
this specific bug class.

A live re-boot test after this fix still Oops'd identically in the same
function — bisection localized a **separate, deeper** issue (the object's
vtable pointer reverting to NULL again moments later) that this fix alone
didn't close; see step 3 below for the root cause.

Also confirmed as a clean negative during the same investigation: the 3
named C-analog dispatch tables (`CSTGControlMsgHandler`/
`CSTGCalibrationMsgHandler`/`CSTGFrontPanelMsgHandler`'s raw fn-ptr
`sMsgHandler` arrays) are provably never populated by any ctor this project
links (confirmed via a new reusable diagnostic kernel module,
`tools_diag/oa_vtable_check.ko`, that walks a loaded module's own ELF symtab
to dump live table contents to dmesg without any guest-shell interaction) —
a real, not-yet-reachable gap, not a bug.

### 2. `68853c2` — 44 missing stub bodies, the insmod-blocking regression

Testing against current HEAD (which by this point included the 2026-07-25
front-panel/MIDI/calibration batches above) found OA.ko could not `insmod`
at all: `nm -u` on a clean `make ko KDIR=/home/build/linux-kronos` rebuild
showed 38 undefined symbols (confirmed via a direct rebuild + `nm -u` for
this rewrite, matching the diff's own 44 new function bodies once the 2
follow-on gaps below are included) — the `ButtonPressHandler`/
`AnalogControllerHandler` cluster's own deferred externs
(`CSTGControllerInfo`/`CSTGControllerRTData`/`CSTGMidiOutPortSerial`/
`CSTGMidiInPortUSB` methods), plus `PushMessage`/`ApplyKeybedCalibration`/
`SetupNKS4Calibration`. `git log -S` on each confirmed none ever had a prior
definition — a pure "convention established but never applied to these
particular symbols" oversight, not a regression from previously-working
code.

Fixed by adding safe no-op/documented-sentinel bodies for all of them in
`reconstructed/OA/src/stub/bar2_stubs.cpp` (matching each symbol's own
already-documented "safe default," e.g. `ApplyKeybedCalibration`'s confirmed
real `0xffff` "no calibration data" sentinel; `CSTGMidiOutPortSerial::
CanTransmitHardware()`/`TransmitHardwareByte()` — genuinely
`__cxa_pure_virtual` dead ends in ground truth itself — made always-false/
no-op so the dead path stays dead).

A live insmod test then surfaced 2 more gaps of the identical class the
symbol count above had missed: `USBMidiAccessory_SetMidiInClient` (sibling
of the already-stubbed `SetDrumPadClient` in
`KorgUsbAudioVirtualDriver.ko`) and 4 symbols missing from
`OmapNKS4VirtualDriver.ko`'s stand-in (`COmapNKS4Driver_GetSPDIFClockError`/
`GetTestMode`/`StartScanning`, `OmapNKS4InputFifo_ReadCommand`). Both fixed
the same way in the same commit — 44 new function bodies total across the 3
files (re-counted directly from the commit diff for this rewrite, not
trusted from the commit message's own "~35"/"~44" wording).

**Result verified for this rewrite**: `insmod` now resolves every symbol and
runs real `init_module` code all the way to `OA_DEBUG_MARKER 8` on this
commit alone — reaching marker 8 is only possible with full symbol
resolution, since Linux resolves a module's symbols before `init_module()`
ever runs. It then hit the *separate* `SetSendBuses()` vtable-reversion Oops
from step 1 above (confirmed not a new regression — the allocator itself was
checked and is a simple, correct monotonic bump allocator). **So: the
"cannot insmod at all" regression was fixed and verified here; the deeper
"zero kernel oops" runtime bug from step 1 was still open at this point.**

A same-day companion check (`68853c2`'s own follow-up) confirmed the
separate, real-hardware-facing `reconstructed/OmapNKS4Module/` project (not
`OmapNKS4VirtualDriver/` — a different project, do not conflate) has **no**
such stub-drift: a clean rebuild showed 94 unresolved externs, all
legitimately accounted for against sibling substitute modules or genuine
kernel exports.

`ba7f7d4` (03:23, doc-only) followed immediately: corrected every doc
location in `kronosology/` + the daemon repo asserting "OA.ko loads clean"/
"zero kernel oops" as a current fact, since as of that moment it wasn't yet
true — preserving each location's original historical content rather than
deleting it.

### 3. The VM memory-map root-cause (no OA.ko source change — a PCI-hole artifact, not a bug)

With `2c539fb` and `68853c2` both in place, the *remaining* `SetSendBuses()`
Oops (the vtable pointer reverting to NULL moments after a demonstrably
correct store) was root-caused the same day via `-O0` codegen + targeted
`printk` checkpoints (a GDB hardware watchpoint was tried first and never
fired — an independently-reproduced instance of the same unexplained wall a
2026-07-24 investigation had already hit). Root cause: `CSTGBankMemory`'s
"AlignedHeap" arena is set up by `ioremap_cache()`-ing a physical address
computed from `high_memory - 1GB` (`stgheap_init.cpp`, itself an
already-audited, correct disassembly transliteration) — and in this VM's
e820 memory map (QEMU's `-M pc`/i440FX chipset), that computed address lands
exactly on the standard sub-4GB PCI/chipset compatibility hole.
`ioremap_cache()` doesn't refuse non-RAM ranges (that's its normal purpose),
so it returns a valid-looking, non-null pointer that passes every existing
null-guard — but writes into it are silently dropped by QEMU, since nothing
backs that guest-physical range.

**No OA.ko source was touched for this** — the fix is a `kronos_vm` boot
configuration change: the kernel cmdline's `mem=3072M` (a prior session's
own attempt, confirmed to have been a no-op — it was above this VM's natural
usable-RAM ceiling and trimmed nothing) was changed to **`mem=2048M`**,
which makes the region scan land on real, e820-usable, kernel-unclaimed RAM
at `0x80000000` instead of the unbacked hole at `0xc0000000`. Confirmed
reproducible across multiple clean boots, byte-identical addresses each
time.

Fixing the memory-map issue immediately exposed a **new, genuinely separate**
bug as the next blocker: with a real non-null vtable pointer now loading
correctly, the specific slot at vtable-offset+0xC (`GetOutputBus`, called
with index 0x32/0x34) was itself still null — a jump to address 0. This is
exactly the gap step 1's vtable reconstruction (`ShouldMute`/`GetOutputBus`)
was aimed at, but a *second*, independent problem (below) kept it from
mattering yet.

### 4. `804b909` — the `.ctors`-vs-`.init_array` bug class, instance 1: first-ever clean `init_module()`

Root cause of the still-null `GetOutputBus`/`ShouldMute` vtable slots:
`audio_input_mixer.cpp`'s `g_audioInputMixerVtable` populated those two slots
via a member-function-pointer-to-`void*` conversion inside a **static
aggregate initializer** — not a real C++ constant expression, so GCC
silently emitted it as a `_GLOBAL__sub_I_*` dynamic initializer in
`.init_array`. **Linux's kernel module loader never runs a module's
`.init_array`** — only the registered `init_module()` function is ever
called — and ground truth's own `init_cpp_support()` (this project's
intended hook for exactly this) is independently confirmed, via real
`OA_322.ko` disassembly, to be a literal 1-byte `ret` — a no-op even in
production. So nothing anywhere in either the real or reconstructed call
graph ever ran that initializer, leaving both fields at their static-storage
zero default.

Fixed by replacing both slots with plain free-function trampolines
(`CSTGAudioInputMixerShouldMuteTrampoline`/`GetOutputBusTrampoline`), whose
addresses **are** genuine compile-time constants — matching the sibling
`dtorComplete`/`dtorDeleting` slots' already-correct pattern. Confirmed via
`objdump`/`readelf` that the translation unit's `.init_array` entry for this
ctor is gone.

**Live-verified end to end**: a full 6-module boot now reaches
`OA_DEBUG_MARKER 17` (independently confirmed this rewrite — `17` is
genuinely the highest marker currently defined, in
`src/init/init_module.cpp`) and prints `OA: init_module succeeded`, with
zero `BUG:`/`Oops:`/`Call Trace:`/`WARNING:` anywhere in the console log —
**the first time this has ever been true on current HEAD.** Host `verify/`
suite green throughout (independently re-run for this rewrite: 124 test
binaries, 0 failures).

This same investigation flagged, but deliberately did not touch, one
closely related open question: `CKorgUsbAudioDriverMidiPorts::sInstance` has
an analogous static-init-driven ctor, and whether the real production
kernel has some mechanism to run it that this project's build/test kernel
lacks was not yet known. Resolved the same day — see step 5.

### 5. `5a1b107` — `.ctors`-vs-`.init_array` instance 2: `CKorgUsbAudioDriverMidiPorts::sInstance`

The `804b909` follow-up question is answered: ground truth's `.ctors`
section is real and load-bearing. `objdump -s -j .ctors` on the real OA.ko
finds this exact symbol's ctor address literally present among 584 entries,
and `/home/build/linux-kronos`'s own `kernel/module.c` genuinely runs every
one of them via `do_mod_ctors()` (gated by this kernel's own
`CONFIG_CONSTRUCTORS=y`), called immediately before `mod->init` — confirmed
by grepping the whole file for `init_array` handling (zero matches). So this
kernel has a real, working `.ctors` mechanism — it just never runs
`.init_array`, which is exactly where this project's GCC 12 host toolchain
(no `-fno-use-init-array` escape hatch) places the equivalent initializer.

Same underlying bug as `804b909`, fixed the same way in spirit: `sInstance`
now has no user constructor at all (zero dynamic init needed), the real
logic moved to an explicit `Construct()` method called via a free-function
wrapper (`ConstructKorgUsbMidiPorts()`) as literally the first statement of
`init_module()`, matching `do_mod_ctors()`'s real relative timing. Rebuilt
clean against `linux-kronos` (`.init_array` down to 1 entry — this project's
own temporary debug-marker scaffolding); host `verify/` suite green.

### 6. `13fba9f` — a genuine new regression exposed by `5a1b107`, found during the first OA.ko+Eva joint boot attempt

While attempting the project's first live integration boot of OA.ko + Eva
together (both sides' full same-day fix chains present, in a `kronosvm`
instance running the confirmed `mem=2048M` config), HEAD moved mid-task —
a concurrent agent landed `5a1b107` while this task was in flight. Per the
task's own instruction to test the *true* current HEAD, rebuilding against
it surfaced a fresh Oops at `OA_DEBUG_MARKER 8`: a kernel NULL-pointer
dereference inside `CSTGMidiPortManager::Initialize()`.

Root cause: `5a1b107`'s fix correctly calls `ConstructKorgUsbMidiPorts()` as
the very first statement of `init_module()` — but this had an unverified
side effect: it made `CSTGMidiPortManager::Initialize()`'s port-registration
loop (`midi_port_manager.cpp`), previously **confirmed dead code**
(`sMidiInPorts[]`/`sMidiOutPorts[]` always all-NULL, documented as such in
that function's own header comment), genuinely live for the first time,
since `RegisterMidiInPort()`/`RegisterMidiOutPort()` now really run during
construction. This exposed two real, previously-dormant bugs in the same
newly-live loop:

1. `CSTGMidiInPortKorgUsb`'s placeholder vtable
   (`_ZTV21CSTGMidiInPortKorgUsb`, `midi_korgusb_port.cpp`) was a literal
   `{0, 0, 0}` — safe only while nothing ever dispatched through it, now a
   NULL function-pointer call. Fixed with a clearly-labeled safe stub
   (returns `false`).
2. A second, deeper crash on the out-port side: `CSTGMidiOutPort`'s own
   vtable *pointer field itself* (not a slot — the whole pointer) read back
   NULL for at least one live instance, despite that class being documented
   elsewhere as using genuine virtual dispatch that should never leave it
   null. Root-causing *why* needs real `objdump -dr` ground-truth work
   against `OA_real.ko`, explicitly flagged as out of scope for this pass
   rather than guessed at — mitigated with a defensive null-guard in the
   generic `PortQuery()`/`PortRegister()` dispatch helpers so it can't Oops
   the kernel while that deeper question stays open.

The fix required care around a concurrent-editing hazard: `git status`
showed the other agent actively editing the same shared working tree.
Isolated into a detached-HEAD `git worktree` at `5a1b107`, applied/built/
tested there without touching their in-flight files, committed cleanly
(`13fba9f`), fast-forwarded `master`, then re-synced only the 2 touched
files in the main tree.

**Result**: host `verify/` suite green, live boot reaches
`OA_DEBUG_MARKER 17`/`OA: init_module succeeded` with zero `BUG:`/`Oops:`,
Eva launches and is confirmed alive at 8 seconds (`kill -0` against the real
process, same pid throughout). Boot then hits the separate, already-
documented (2026-07-25) fbcon/VT-console-lock stall inside `fakefb.ko`'s
`register_framebuffer()` — see "What's still open" below for its current
status.

### 7. `9c587a2` — `.ctors`-vs-`.init_array` instances 3 and 4: the systemic sweep

Rather than wait for more instances to surface one crash at a time, a
systemic sweep extracted and individually ground-truthed all **584** real
`.ctors` entries in `/home/share/Decomp/OA.ko_Decomp/OA.ko`
(`readelf -x`/`-r` plus a sorted-`.text`-symbol bisection to resolve each
offset to its owning `_GLOBAL__I_*` symbol). Only **24 of 584** had any
textual match against this project's current source — the other **560**
belong to not-yet-reconstructed voice-model/tone-adjust/DSP subsystems,
correctly out of scope for now.

Of the 9 `sInstance`-singleton-pattern ctors among those 24: 1 already fixed
(`5a1b107`), 2 newly fixed here — `CSTGPerformanceVarsManager::sInstance[8]`/
`[9]` (ground truth seeds both to `1` at module load; this project's raw-byte
model only ever *read* them, never wrote them, silently diverging
`AllocPerformanceVars()`'s first-slot-toggle direction and `ShouldMute()`'s
first comparison from ground truth) and `CSTGChannelValues::sTemplate`
(ground truth seeds 121 sub-records' `+0xa`/`+0xb` bytes to `1`; this
project's template was getting zero real content from any source, since its
only other populator, `InitializeLongHand()`, is itself a confirmed-real
deliberately-deferred empty stub) — 1 confirmed already-correct via an
existing placement-new fix, 4 confirmed genuinely no-op (ctor writes match
plain BSS zero already), 1 flagged out-of-scope (`CSTGDrumPadInterface`'s
ctor installs a vtable pointer for an entirely unmodeled
`CSTGDrumPadClient` class).

Fixed via the same "explicit free function called early in `init_module()`"
pattern as `5a1b107`. This closes the project's **3rd systemic recurring-bug
class** this session (alongside Eva's 17-instance vtable-dispatch-stub-gap
family and the "literal-vs-relocation" pattern above), documented in a new
standing lesson: `.claude/agent-memory/re-decompiler/
LESSON_ctors_vs_init_array.md` (kronosology project index), which
consolidates all 4 confirmed instances across the 3 commits above.

**Live-verified together with the concurrent `13fba9f` fix** (git
stash/pop used to carry uncommitted work safely across the mid-task HEAD
move): clean boot to `OA_DEBUG_MARKER 17`, `OA: init_module succeeded`, Eva
alive at 8s, zero `BUG:`/`Oops:` across a full 402-line console log. Host
`verify/` suite green throughout — independently re-run for this rewrite
(current HEAD `9c587a2`): 124 test binaries, 0 failures, and a from-scratch
`make ko-clean && make ko KDIR=/home/build/linux-kronos` rebuild completes
clean with no unresolved bar2-stub-class symbols left in `nm -u OA.o`
(remaining unresolved symbols are all legitimate companion-module/kernel
exports — `KorgUsbAudio*`, `OmapNKS4*`, `__gmpz_*`, etc.).

### Net result: OA.ko's first confirmed clean joint boot with Eva

Putting the whole chain together, this is the project's first-ever
confirmed clean simultaneous load of OA.ko (`init_module()` completes
100% clean, `OA_DEBUG_MARKER 17`) and Eva (independently confirmed alive
afterward) — with no new interaction bug at the module-load/process-launch
boundary. This does **not** yet constitute a genuine sustained, progressing
multi-minute joint boot: the pre-existing, unrelated fbcon/VT-console stall
(see below) halts all further progress shortly after Eva launches. What is
newly true is that the load/launch boundary itself, and OA.ko's own
`init_module()`, are both now confirmed clean end to end for the first
time.

---

## New standing lesson documents this created

- **`LESSON_ctors_vs_init_array.md`** (`.claude/agent-memory/re-decompiler/`,
  kronosology project index) — the full pattern behind steps 4/5/7 above:
  ground truth's real kernel genuinely runs `.ctors` via `do_mod_ctors()`
  (`CONFIG_CONSTRUCTORS=y`), but this project's host GCC 12 toolchain has no
  way to target `.ctors` directly, so any equivalent C++ static initializer
  silently lands in `.init_array` instead — a section the module loader
  never scans. 4 confirmed instances across 3 commits (`804b909`,
  `5a1b107`, `9c587a2`×2); coverage caveat: only 24 of 584 real `.ctors`
  entries have been individually ground-truthed (the textual-match subset
  against currently-reconstructed source) — see "What's still open" below.
- **The "literal-vs-relocation" pattern** (no dedicated standing-lesson file
  of its own yet, but referenced from `HARDWARE_REVIEW_LOG.md` and this
  document) — `objdump -d` without `-r` hides an `R_386_32` relocation as a
  plausible small integer immediate. Confirmed 3 total times project-wide:
  2 earlier instances in `init_module.cpp` (a `printk`/`rt_printk`
  format-string pointer, and `stg_get_current_task()`'s per-cpu
  displacement — both re-checked and reconfirmed still correct this same
  day) plus this session's `CSTGAudioInputMixer` vtable-pointer bug
  (`2c539fb`).

---

## What's still open

Carried forward from 2026-07-25 (front-panel/keybed/MIDI/calibration —
mostly unaffected by the 2026-07-27 dynamic-testing phase, since that phase
found bugs in the init/construction path, not in these handlers'
control-flow logic itself):

- **`CSTGKeybedInterface::ProcessNextKeybedEvent`** — the actual
  ring-to-event drain is entirely unreconstructed; keybed/joystick/aftertouch
  events are framed and enqueued but never turned into anything musical yet.
  Five dependent classes/functions are completely unmodeled.
- **`ApplyKeybedCalibration`** — real kernel-mode FPU context switch, not
  reconstructed (has a safe `0xffff`-sentinel stub body as of `68853c2`, so
  it no longer blocks `insmod` — but its real math is still unmodeled);
  blocks confidence in `FilterAnalogController`'s calibration math (control
  flow is validated, math is not).
- **MIDI-OUT UART hardware transmit is provably dead code in this firmware
  image** (`__cxa_pure_virtual`, no override anywhere in the binary) — needs
  a real-hardware test to confirm DIN MIDI-OUT genuinely never transmits, or
  whether some undiscovered companion module patches the vtable at runtime.
- **KorgUsb MIDI transport and the generic USB-MIDI accessory hierarchy**
  are both unreachable without a real or virtual companion module. The
  larger "hierarchy 2" generic USB-MIDI-class accessory tree (drum-pad
  client, `ReceivePacket`) remains deliberately deferred.
  `CSTGExtMIDIClockSync`'s three largest methods (`ProcessClock`,
  `MeasureJitter`, `EstimateTempoAndPredictNextClock`) remain unreconstructed.
- **`ButtonPressHandler`'s six sibling methods** (`HandleEditInContextButton`,
  `ProcessMixerSwitchPress`, `SetMixerKnobMode`, `SetSoloSelected`,
  `ResetAllKnobCCs`, `ResetAllExtModeControllers`) and **7 of 22
  `AnalogXxxHandler` callees** remain deferred externs — as of `68853c2`
  they have safe no-op stub bodies (so they no longer block `insmod`), but
  their real logic is still unreconstructed.
- **`AccessBank`'s `FindBankRecord`** (hash-table bank lookup) and
  **`StartupInitializeROMBank`** are deferred — only the all-zero ROM-bank
  fast path of the `/proc/.oacmd` bank commands is currently live.
- **File-daemon vtable dispatch targets** (the actual SSD/flash I/O behind
  `CSTGFileOpener`/`CSTGFileCloser`/`CSTGHDRFileWriter`/`CSTGHDRFileReader`/
  `CSTGStreamingFileReader`) remain opaque.
- **Field-name/semantic uncertainty is broad but shallow** across this
  session's front-panel/keybed work — faithfully reproduced from
  disassembly but not independently confirmed by name; real-hardware UI
  interaction while watching MIDI/UI traffic is the recurring suggested
  test in `HARDWARE_REVIEW_LOG.md`.

New/updated as of 2026-07-27:

- **The `.ctors` sweep only covered 24 of 584 real ground-truth entries**
  (~4%). The other 560 belong to not-yet-reconstructed voice-model/
  tone-adjust/DSP subsystems and had no textual match against current
  source, so nothing to check yet — **re-run the same cross-reference
  (`LESSON_ctors_vs_init_array.md` documents the exact method) whenever a
  new subsystem gets reconstructed**, since a match against the growing
  24-entry set may newly appear.
- **`CSTGAudioInputMixerBase::SetSendBuses()`'s own vtable slot 3
  (`GetOutputBus`) and `ShouldMute`** are now real and correctly dispatched
  (steps 1 and 4 above) — this specific, long-flagged item is closed;
  `HARDWARE_REVIEW_LOG.md`'s own batch-58 entry carries a superseded-note
  pointing here.
- **The fbcon/VT-console-lock boot stall is still open, unchanged by any of
  today's OA.ko/Eva work.** Checked `PROJECT_BRAIN/status.md`'s latest
  entries specifically for this before writing it here (per this task's own
  instruction not to assume the 2026-07-25 characterization still holds
  unmodified): as of the most recent entry in that file (2026-07-27, the
  joint-boot task above), it's the same already-documented, already-narrowed
  bug — fbcon/VT-console-takeover lock contention inside `fakefb.ko`'s
  `register_framebuffer()` call, a genuine total single-vCPU freeze (not a
  blocked shell, confirmed via a silent background heartbeat), self-
  terminating after roughly 10-11 minutes wall-clock — reproduced identically
  a third time during the `13fba9f`/`9c587a2` joint-boot verification, with
  no new narrowing or fix attempted this pass. It is **not** OA.ko- or
  Eva-related; it predates and is orthogonal to everything in this
  document's 2026-07-27 section. No concurrent task in `status.md` as of
  this writing reports further progress on it.

Full derivation for each 2026-07-27 fix, exact addresses, and header-comment-
level reasoning: see `HARDWARE_REVIEW_LOG.md` in this directory (its own
batch-58 entry carries a superseded-note pointing to this rewrite) and
`PROJECT_BRAIN/status.md`'s kronosology-section entries dated 2026-07-27.
For 2026-07-25 items, see each subsystem's own header (`include/*.h`). Real
hardware-behavior uncertainty specifically (as opposed to scope/Tier-B
deferrals) is tracked separately in `HARDWARE_REVIEW_LOG.md`.
