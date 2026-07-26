# Hardware Review Log

Consolidated real-hardware-verification open questions for the Eva
reconstruction, pulled together 2026-07-26 from Eva's established
per-header-comment convention (see `SESSION_SUMMARY_2026-07-25.md`'s "Two
things distinguish today's session from OA.ko's own equivalent" note --
until now, Eva had no single file like `reconstructed/OA/HARDWARE_REVIEW_LOG.md`,
open items lived only inline in `include/*.h`). Mirrors OA's log format and
selection bar: this is real-hardware-behavior uncertainty only -- NOT a
Tier-B stub/scope-decision tracker (see `SESSION_SUMMARY_2026-07-25.md`'s own
"What's still open" section, and each header's own comment block, for that
kind of item instead).

Every entry below cites where the underlying fact is already established in
this project's own tree (`README.md`, a header comment, or a `.cpp` comment)
-- this file adds organization and a "what real-HW test would confirm" line,
not new claims, EXCEPT where explicitly marked "NEW" (a genuine gap spotted
while auditing for this log, not previously written down anywhere).

Format: `## <topic>` + what's uncertain + what real-HW test would confirm.

---

## CCommDriver — confirmed simulator-only by ground truth; real front-panel event INPUT path is a provable dead end in this exact binary

Not uncertain that `CCommDriver` is dead weight on real hardware --
`README.md`'s "`CCommDriver::setupfifoname()` upgraded Tier-B -> Tier A"
section (`src/ipc/comm_driver.cpp`) already establishes this from
disassembly, confirmed the hard way by a wrong test assumption during KAT
development: **all three of `setupfifoname()`'s fifo-path fields
(`mLcdFifoPath`/`mEventFifoPath`/`mCommandFifoPath`) are gated -- both the
`argv`/`envp`-scan assignment AND the hardcoded-default fallback -- on
`Eva_IsSimulation()`/`Eva_IsSimulationSVGA()`** (`app_mode.h`, both false
whenever the running binary's `argv[0]` basename is plain `"Eva"`, i.e. on
real hardware per `eva_main.cpp`'s own app-mode detection). So on real
hardware `CCommDriver`'s constructor opens **zero** of its three fds, every
single boot -- not a partial gap, a total, ground-truth-confirmed no-op.
`README.md` already draws the architectural conclusion this implies: "real
hardware IPC goes through the separate `USTGUserAPI`/`/dev/rtf*` substrate
instead" (`ustg_user_api.h`'s `Connect()`/`ConnectPanelFifo()`/
`ConnectUnsolicitedFifo()` open `/dev/rtf0`/`/dev/rtf1`/`/dev/dmsg0`/
`/dev/rtf7`/`/dev/rtf5` unconditionally, no simulation gate).

**NEW (spotted auditing for this log, not previously written down anywhere
in this tree)**: nobody has yet connected this to `CLinuxPanelDriver::
GetEvent()` (`panel_driver.h`/`.cpp`), which reads its 8-byte panel
button/touch/encoder events by raw-offset from `CCommDriver::getInstance()`'s
own `mEventFd` (`+0x10`) and returns `false` immediately whenever that fd is
negative. Given the finding above, `mEventFd` is `-1` on every real-hardware
boot -- meaning `CLinuxPanelDriver::GetEvent()` can **never** deliver a
single panel event on real hardware, by ground truth's own logic, not a
reconstruction gap. (LED/beep OUTPUT is unaffected -- `PutCommand()`'s
`USTGAPIFrontPanel::*` wrappers go out through `USTGUserAPI::
SendPanelMessage()`'s own `/dev/rtf7` fd, which `ConnectPanelFifo()` opens
unconditionally, no simulation gate.) The plausible resolution -- real panel
button/touch/joystick events reach Eva through OA.ko's own "unsolicited"
`/dev/rtf5` channel instead (`USTGUserAPI::ReadUnsolicitedMessage()`,
consumed by `CSTGUnsolMsgHandler`, `stg_unsol_msg_handler.h`) rather than
through `CLinuxPanelDriver::GetEvent()` at all -- is architecturally
consistent with everything else this project has found, but this pass did
not trace a confirmed real caller of `CLinuxPanelDriver::GetEvent()` on
either path, nor confirm `CSTGUnsolMsgHandler`'s own dispatch actually
carries panel-event payloads.

Separately, `README.md`'s own "real bug, confirmed at the raw-disassembly
level" callout for `setupfifoname()` (`strchr()`'s result dereferenced with
no NULL check -- any array entry lacking `'='` segfaults) is **substantially
de-risked but not fully closed** by a later fix in the same tree:
`eva_main.cpp`'s "REAL BUG FIX (verification pass, 2026-07-25)" entry found
that `main()`'s real call is `CCommDriver::getInstance(envp)`, NOT
`getInstance(argv)` as first transcribed -- `envp` is POSIX-guaranteed to be
`"NAME=VALUE"`-shaped throughout, so the no-NULL-check bug is not expected to
fire on a normal process environment. What's still genuinely unconfirmed:
Eva's real on-device launch wrapper (`exec /korg/Eva/Eva`, per
`docs/workflow/deploying_patches.md`-style references) lives inside the
encrypted `Eva.img`, not present in any extracted rootfs on this share --
this project could not independently confirm its real `envp` contents, only
that POSIX environment semantics make the crash unlikely in practice.

Real-HW test that would help: on a real Kronos, check `/proc/<Eva-pid>/fd`
for whether `CCommDriver`'s three fifos are ever actually opened (confirming
or refuting "total no-op on real hardware"); separately, capture
`/proc/<Eva-pid>/environ` to see the real launch wrapper's actual `envp`
contents (settling the residual no-NULL-check concern for good); and, if a
service-mode packet trace is possible, confirm real front-panel button/
touch/encoder presses arrive via `/dev/rtf5` (unsolicited channel) rather
than any path through `CCommDriver`.

---

## CHIDDriver — USB-HID keyboard driver: ground-truth uninitialized-stack read + unconfirmed per-scancode keycode table

Two distinct items in `include/hid_driver.h`/`src/hw/hid_driver.cpp`
(Stage 6 breadth sweep, 2026-07-25; also written up in `README.md`'s
"Stage 6: breadth sweep, CHIDDriver/CLinuxPanelDriver batch"):

1. **Real ground-truth bug, confirmed at the disassembly level, not a
   decompiler artifact**: `CHIDDriver::GetKeyboardEvent()`'s own stack
   layout places its `isKeyDown` computation directly over a byte
   `GetEvent()` never actually writes (`HIDUsbKeybEvent::reserved4`,
   traced against every write site in `GetEvent()`'s own body). Reproduced
   faithfully -- the reconstruction's own local variable is left
   genuinely uninitialized, not deterministically zeroed, to match the
   real undefined behavior rather than paper over it. What's unconfirmed:
   whether this manifests as an OBSERVABLE glitch on real hardware (an
   occasional garbage `isKeyDown` value when a USB keyboard is attached to
   a real Kronos) or whether some real-world consistency in the toolchain's
   own stack-garbage happens to make it look deterministic in practice.
2. **`s_kucMappingTable`** (`.rodata+0x08fd9c00`, 127 bytes, byte-read
   directly off the real binary, not decompiled) maps raw evdev USB-HID
   scancodes to Korg-internal keycodes. The table's VALUES are not in
   question (byte-exact extraction); its per-entry semantic correctness
   (does entry N really correspond to the USB HID scancode a real
   keyboard sends for that physical key) has no independent cross-check
   beyond "it's a lookup table indexed by scancode."

Real-HW test that would help: with an actual USB keyboard plugged into a
real Kronos's USB port and whatever real consumer of
`CHIDDriver::GetKeyboardEvent()` exists (not itself identified by this
project -- `README.md` notes `CHIDDriver` is constructed on the real boot
path but nothing in this reconstruction's own call graph yet dispatches
through its higher-level methods), press a representative spread of keys
(letters, modifiers, function row) and confirm both that the decoded
keycodes match the physical keys pressed (settles item 2) and that
`isKeyDown` never visibly flips independent of the physical key state over
a long soak (settles item 1).

---

## CEditor::CPanelIfcTask::mBlinkCounter — uninitialized ctor field; real front-panel LED blink phase on first use is indeterminate

`include/panel_ifc_task.h`'s own field-layout comment (Stage 6 dedicated
`CPanelIfcTask` batch, 2026-07-25): `+0xa8 mBlinkCounter` (`Exec()`'s own
per-tick counter, compared against `0x31`/49 to drive the periodic
all-LED blink once `mBlinkEnabled` goes nonzero via `SetLEDStatus(ledState
== 2)`) is **not initialized by the real ctor** -- confirmed against
ground truth, not a reconstruction gap (same "genuinely uninitialized,
preserved" category as `mReserved84` in the same class and `CEditor`'s own
`mAlphaKeybIfcTask`). Since the real ctor's `malloc(0xb8)` never zeroes
this field, its starting value is whatever the heap allocator happens to
return the first time a `CPanelIfcTask` is constructed -- meaning the real
phase of the front-panel LED blink cycle, the very first time blinking is
ever enabled after boot, depends on real heap-allocator history rather than
being a fixed, predictable offset.

Real-HW test that would help: on a real Kronos, trigger the "all LEDs
blink" state (whatever real UI/diagnostic action calls `SetLEDStatus` with
`ledState==2`) repeatedly across several power cycles and observe whether
the blink phase is always identical (suggesting the real allocator's
behavior is effectively deterministic in practice, e.g. because
`CPanelIfcTask` is always the Nth heap allocation of an otherwise-identical
boot sequence) or genuinely varies boot-to-boot (confirming real,
observable non-determinism rather than something this project's own
malloc-history-blind KATs could ever have caught).

---

## CSTGUnsolMsgHandler::EndHandling / CPanelOut::SEncoderEvt — 3 bytes of real uninitialized stack garbage transmitted to OA.ko in every rotary-encoder event

`include/stg_unsol_msg_handler.h`'s own comment on `SEncoderEvt` (Stage 6
batch, 2026-07-25): the real 8-byte `CPanelOut::SEncoderEvt` structure
`EndHandling()` builds for every `OnEncoderEvent()` occurrence has only
byte 0 ever assigned a real value in ground truth's own disassembly --
bytes 1..3 are read directly out of the real binary's own
uninitialized-stack-garbage local (`local_18._1_3_` in the Ghidra naming),
and the trailing dword is always zeroed. This reconstruction
deterministically zero-initializes those 3 padding bytes (unlike the real
binary, which sends whatever was on the stack) specifically because
nothing downstream is confirmed to read them -- a deliberate, documented
divergence from bit-for-bit ground-truth fidelity, not an oversight.

What's unconfirmed: whether any real consumer on the OA.ko/hardware side of
this message (whatever ultimately receives `CPanelOut::SEncoderEvt` over
the wire once it leaves `EndHandling()`) ever actually reads bytes 1..3 of
the payload. If it does, real hardware would be operating on genuine
per-boot stack garbage in those bytes today (not a hypothetical -- ground
truth really does this), and this reconstruction's own zero-fill would be
a real, deliberate divergence from bit-exact behavior, not just a
harmless simplification.

Real-HW test that would help: identify and trace the real downstream
consumer of the rotary-encoder event payload (on the OA.ko side, or
whatever component of the real firmware processes `OnEncoderEvent`'s
output) and confirm bytes 1..3 are never read, or -- if they are -- capture
whether their value ever visibly affects behavior (e.g. an intermittent,
boot-dependent quirk in encoder response) to settle whether this
reconstruction's zero-fill needs to become a faithful garbage-preserving
read instead.

---

## USTGUserAPI::GetProgress/SetProgress/IncrementProgress — `/proc/OmapNKS4ProgressBar` presence on real hardware not independently confirmed

`include/ustg_user_api.h`'s own comment: these three real, disassembly-
confirmed methods (confirmed via a direct `.rodata` string read, not
guessed -- `IncrementProgress()`'s literal `"inc"` write is byte-exact) are
a small out-of-band progress-reporting channel through a `/proc` file,
`/proc/OmapNKS4ProgressBar` -- entirely separate from the `/dev/rtf*`/
`dmsg0` FIFO substrate everything else in this class uses. All three
silently no-op if the file doesn't exist, and this project's own
`OmapNKS4VirtualDriver` companion project does not currently expose this
specific `/proc` file, so this path has never been exercised even in the
VM/simulator environment, let alone on real hardware. No caller of any of
the three was found anywhere in the 37,795-function export either, so it's
unconfirmed whether real Kronos firmware itself ever actually drives this
progress-bar channel in practice (vs. being dead/vestigial instrumentation
in the shipping binary too).

Real-HW test that would help: on a real Kronos, trigger an operation
plausibly associated with a progress indicator (loading a large sample
bank, an OS update, an HDR export) while watching for `/proc/
OmapNKS4ProgressBar` to appear and update, to confirm both that the file is
real on-device infrastructure (not just a disassembly-recovered dead
string) and what UI element, if any, it drives.

---

## `CPoller::InitButtons()`/`InitAnalogs()` — every button/analog client slot collapses onto ONE real handle (0), confirmed emergent, not yet real-HW-relevant-tested

**NEW** (added 2026-07-26, consolidating a finding already written up in
`SESSION_SUMMARY_2026-07-25.md`'s "genuine emergent behavior" section and in
`include/poller.h`'s own header comments — not a new claim, just newly
organized here because it's real-hardware-behavior-relevant, not a
scope/Tier-B item).

`CPoller::InitButtons()`/`InitAnalogs()` (real, Tier A, `.text+0x089f4830`/
`0x089f3c80` in ground truth) each walk a real `.rodata` name-pair table
(128 button slots, 78 populated; 64 analog slots, 29 populated — both
byte-dumped directly, not assumed) and call the already-real
`CPoller::RegisterClient()` once per populated slot, always with the exact
same real name pair (`"Editor"`/`"PanelIfcTask"`). `RegisterClient()`'s own
real Phase-2 logic (confirmed via disassembly, not a modeling choice) reuses
the first still-**unconnected** `mClients` slot rather than constructing a
new `CIfcClient` — and nothing in `InitButtons()`/`InitAnalogs()` themselves
ever marks the client it just registered as connected. The result, verified
empirically against real, unmocked code in a host-side KAT (not just argued
analytically): running both functions back-to-back from a fresh `CPoller` —
matching `CPanel::Config()`'s own real, unconditional call order
(`InitButtons()` then `InitAnalogs()`) — constructs **exactly one** real
`CIfcClient` object for the whole process, and *every one* of the 78
populated button-table slots and 29 populated analog-table slots resolves to
that same single client handle (0).

What's uncertain for real hardware: this reconstruction only currently
verifies the mechanism (`RegisterClient()`'s reuse-first-unconnected-slot
logic, `InitButtons()`/`InitAnalogs()`'s calling pattern) against
ground-truth disassembly — it does not, and cannot from static analysis
alone, confirm what real downstream code on actual hardware does with 107
button/analog registrations that all resolve to handle 0. Two live-hardware
possibilities this project cannot currently distinguish: (a) this is
entirely benign — perhaps every one of these 107 "registrations" is really
just populating a lookup table for `CPoller::Exec()`'s own per-event
dispatch (which reads the table by button/analog *code*, not by client
handle, per `poller.h`'s own documentation of that function), in which case
the shared handle is irrelevant and this is correct, intended behavior, not
a bug at all; or (b) some real consumer somewhere does distinguish clients
by handle (e.g. for per-client analog-event ring buffers, `CIfcClient`'s own
`PutAnalogEvt()`/`FlushAnalogEvts()` machinery) and would observe cross-talk
between what look like 107 independent registrations but are actually one
shared object. This project's own traced call graph does not currently
include a caller that would surface either outcome.

Real-HW test that would help: on a real Kronos, if any UI/diagnostic
capability exists to introspect `CPoller`'s own `mClients` array or to watch
front-panel button/analog-input event delivery per-registered-client
(rather than per raw hardware code), confirm whether the real firmware
genuinely shares one client object across all 107 button/analog code
registrations the way this reconstruction's own disassembly-derived model
does, or whether some other mechanism (not yet traced by this project)
keeps them logically separate despite the shared handle.

---

## `CAlphaKeybCtrlTask::SetCtrlCondition()` — three real, asymmetric bit read/clear mismatches in the sticky-key toggler

`include/alpha_keyb_ctrl_task.h`'s own header comment (Stage 6 dedicated
batch, 2026-07-26): `SetCtrlCondition()` (268 bytes, a static sticky-key
bitmask toggler for the 'X'/';'/'L'/'a' keys) has three key-up cases,
confirmed via careful mask-arithmetic re-reading of the real disassembly
(not "fixed" into symmetric read/clear pairs, and a first KAT-writing draft
accidentally DID normalize two of them before the KAT's own failing checks
caught the mistake): key-up('X') **reads** bit 4 but **clears** bit 8;
key-up(';') reads bit 2 but clears bit 1; key-up('L') reads bit 8 but clears
bit 4. This is genuine, faithfully-preserved ground-truth behavior — real
hex masks (`0xfffffff7`=~8, `0xfffffffe`=~1, `0xfffffffb`=~4) directly
confirmed against the decompile, not a transcription artifact.

What's uncertain for real hardware: whether this read/clear asymmetry
produces any user-observable effect on a real Kronos's sticky-key/alpha-key
modifier behavior — e.g. whether releasing one of these 4 keys can leave the
sticky-modifier bitmask in a state inconsistent with the key that was
actually released, and whether that inconsistency is ever visible as
incorrect alpha-keyboard input behavior, or whether it's silently harmless
because nothing downstream ever queries the specific bit each case actually
clears (as opposed to the one it reads).

Real-HW test that would help: on a real Kronos with the alpha-keyboard
overlay active, exercise combinations of the 'X'/';'/'L'/'a' keys (in
particular sequences where one of these keys is held/released while a
sticky-modifier state from a DIFFERENT key is active) and check whether any
input misbehavior (a modifier appearing "stuck" or clearing unexpectedly)
is observable, which would indicate this bit mismatch has a real, visible
hardware consequence rather than being an inert quirk.
