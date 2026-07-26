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
