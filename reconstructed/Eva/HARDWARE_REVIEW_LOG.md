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
VM/simulator environment, let alone on real hardware.

**CORRECTED 2026-07-27** (re-check triggered by the `CBDApiInstance`
mangled-name-grep false-negative found the same day, see this file's own
`CBDApiInstance` entry -- checking every other "no caller"/"zero callers"
verdict in this project's docs against a fresh, independent `objdump -dr`
sweep of the whole ground-truth binary, not a re-run of whatever the
original search used). The original "no caller of any of the three was
found anywhere in the 37,795-function export" claim was **wrong**, and not
a one-off: it turned out to be representative of essentially this entire
`ustg_user_api.h` "Stage 2 IPC substrate" batch (2026-07-25) -- see
`include/ustg_user_api.h`'s own now-corrected per-method comments for
`Disconnect`/`ConnectUnsolicitedFifo`/`ReadMessage`/
`ReadMessageWithTimeout`/`ReadUnsolicitedMessage`/`SendPanelMessage` too,
all of which had the identical false claim. For the progress channel
specifically: `IncrementProgress()` has 72 real call sites (the KSF/KMP/KSC
sample-format loader family -- `CLoadKsfManager::Load`/`CFileKMP::load`/
`CFileKSC::load` -- plus `CDesktop`'s own ctor and `OnStartup()`);
`SetProgress()` has 1 real caller (`CDesktop::OnStartup()`); `GetProgress()`
has 2 real callers (both inside `CStorage::Initialize(CStaticLabel*,
PegRect&)`). This **confirms**, rather than leaves unconfirmed, that real
Kronos firmware genuinely drives this progress-bar channel during sample/
bank loading and desktop startup -- it is real, actively-used
instrumentation, not vestigial. No functional change needed here: every
real caller found lives in a subsystem (KSF/KMP/KSC sample loaders,
`CDesktop`, `CStorage`) this project has independently and separately
confirmed out of scope, so `GetProgress`/`SetProgress`/`IncrementProgress`
remain unreachable on THIS reconstruction's own traced call graph even
though the "no caller anywhere in the export" framing itself was false.
The real-HW-test question below stays open and is, if anything, now more
likely to pay off (a large sample-bank load is a documented real trigger,
not just a guess).

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

---

## `USTGAPIFsck::GenericMumount()` / `USTGAPIControl::SaveRandomSeed()` — real `fork()`+`execve("/korg/Eva/mumount")`+`waitpid()`, external binary never independently examined

**NEW** (added 2026-07-26, cross-checking commit `c62cbd1`, Stage 6 breadth
sweep, against this log — the fact was written up in `src/ipc/
stg_unsol_msg_handler.cpp`'s own header comments but had no
`HARDWARE_REVIEW_LOG.md` entry).

`USTGAPIFsck::GenericMumount(char *const *argv)` (`.text+0x08e27120`, 356
bytes) is a real, disassembly-confirmed `fork()`+`execve("/korg/Eva/
mumount", argv, {NULL})`+`waitpid()` sequence — genuinely real OS-level
process execution, not a mock or a simulated path. It is reached live from
`USTGAPIControl::SaveRandomSeed()` (`.text+0x08e1d090`, real, called from
`CSTGUnsolMsgHandler::EndHandling()`'s `mForceSaveOnEnd`-gated tail) with
`argv = {"/korg/Eva/mumount", "sr", NULL}` — the literal `"sr"` second
argument is confirmed byte-exact from `.rodata` (not a guess) but its
OWN meaning to `/korg/Eva/mumount` (a mode flag? a device/label name?) is
NOT recovered, since `/korg/Eva/mumount` is a separate on-image binary,
never present in any extracted rootfs on this share and entirely out of
this project's scope.

What's uncertain for real hardware: this reconstruction's `GenericMumount`
faithfully reproduces the parent-side fork/exec/wait control flow and its
three distinct error-reporting paths (fork failure, wait failure sans
`EINTR` retry, nonzero child exit code), but has never actually executed
against the REAL `/korg/Eva/mumount` binary — only host-side test doubles.
Real-hardware execution could differ if the real binary's actual exit-code
contract, argument parsing of `"sr"`, or execution latency (this is
presumably a umount/sync operation against the real SSD, on the critical
path of `EndHandling()`'s "force save on end" flow, i.e. shutdown/panel
power sequencing) doesn't match this reconstruction's assumptions.

Real-HW test that would help: on a real Kronos, trigger whatever real
condition sets `mForceSaveOnEnd` (a clean panel-driven shutdown is the
most likely candidate) and confirm via `/proc/<Eva-pid>/status` or a
`strace`-equivalent that `/korg/Eva/mumount sr` actually runs, exits 0,
and that "saving the random seed" has an observable real-world effect
(e.g. a file under `/korg/rw` updates) — would also finally pin down what
the `"sr"` argument means.

---

## Vtable-dispatch-stub-gap bug class — 16 confirmed instances (2026-07-25/27), all reconstruction-only; carry the *detection method* into real-hardware bring-up, not the individual fixes

**This entry is a different kind of thing than every section above.** The
sections above are open real-hardware-BEHAVIOR questions about code this
project has reconstructed. This one is a closed bug-class post-mortem: a
recurring defect class, found entirely through this project's own internal
cross-checking (reconstruction vs. the real binary's ground-truth bytes), with
no open behavioral question left for real hardware to answer. It's included
here anyway per this project's standing "log all issues and questions for the
end, to work through with real hardware" instruction — the *methodology* that
finally caught it is worth having on hand if anything looks subtly wrong
during real-hardware bring-up later.

### The pattern

When reconstructing a C++ class's per-instance vtable (the `PTR__ClassName_<addr>[]`
arrays in `omega_vtables.cpp`/`mains.cpp`), any slot not yet individually
verified against the real binary defaults to a generic no-op placeholder
(`EvaVTableStub`). That default is silent and structurally harmless: the array
is still the right size, still links, still passes every internal-consistency
check and the full host KAT suite — because those checks only verify the
reconstruction agrees with *itself*. The bug only exists relative to the real
binary: if a genuinely-reconstructed dispatcher elsewhere in the project
(`CModuleManager`, the `CKernel` global-object bring-up/teardown loops,
`Api`-facade slot dispatch, etc.) raw-calls through that exact slot on a real
boot path, the call silently no-ops instead of reaching the real, already-correctly-reconstructed
target method — even though that target method's own code is sitting right
there in the same file, fully correct, just never wired in. The clearest
illustration: `CEditor::Setup()` was independently confirmed live (a debug
marker fired) for an entire session before anyone noticed `CEditor`'s own
vtable slots 2-4 were still stubs — meaning the confirmed-live constructor's
entire `Setup()` fan-out (`CPanelIfcTask`/`CSTGUnsolMsgHandler`/`CChunkServer`/
`CAlphaKeybIfcTask`) had been silently dead the whole time.

A closely related but not identical failure mode shows up in a few of the 16
instances below (`CApiDescriptor`, `CRMApiCallBack`): instead of a
correctly-sized array with one wrong-target slot, the array itself was
undersized or a bare `NULL` scalar rather than a real array at all. The
project's own commit messages label this the "undersized-vtable-array bug
class" in places, distinct in mechanism from the "correctly-sized array,
wrong-target slot" pattern that `CEditor`/`CPanel`/`CSysApiInstance`/the
`XxxApiInstance` family exhibit. Both produce the identical externally-visible
symptom (a real caller's dispatch through a real slot silently fails to reach
the real target) and both are invisible to the same static/host-test checks,
which is why this project's own `LESSON_vtable_dispatch_stub_gap.md` groups
all 16 under one running count — but a future sweep should not assume "check
the slot target" alone is a complete audit; "check the array's own size and
existence" is a separate, necessary check.

### Detection method that finally worked

Six rounds of static analysis (call-graph tracing, `.rodata` vtable-slot byte
reads, cross-referencing header comments against the arrays they described)
found most of the first 8 instances but converged on "exhausted" multiple
times while instances 9-16 sat undetected — including one case
(`CSysApiInstance`, instance 9) where a header comment had already documented
the correct fix, citing a direct ground-truth byte read, and nobody had
actually applied it to the array. **What finally worked was live dynamic
tracing**, using QEMU's native gdbstub against a real `kronosvm` boot (not the
SLIRP-forwarded gdbserver path, which reliably dies at a fixed ~74-75s wall
clock — a QEMU `-gdb tcp::PORT` argument bypasses that entirely). Two
refinements mattered:

- **Boot with `-S` (halt at the reset vector) before attaching.** The
  `CGlobalObjectBase` phase hooks these bugs live in fire during C++
  static-init (`.init_array`), milliseconds after `execve()` — attaching mid-boot
  is provably too late (confirmed by trying it first and getting zero hits).
- **Break on the generic dispatch helper's own indirect-call site
  (`CallVSlot1`/`CallVSlot2`'s `call eax` instruction), not the target stub's
  entry point.** Breaking on the stub's entry seems more direct but is
  actively misleading: gdb stops at the stub's very first instruction, before
  its own `push ebp; mov ebp,esp` runs, so at that exact PC the frame-pointer
  chain still belongs to the *caller's* caller — gdb's automatic backtrace
  misattributes which frame is really "the caller of the stub" (confirmed by
  disassembling several flagged call sites and finding the claimed caller
  didn't actually contain an indirect call to the stub at all). Breaking
  instead on `CallVSlot1`/`CallVSlot2`'s own `call eax` site means the
  helper's own prologue has already executed, so `obj`, the byte offset, the
  resolved call target (`$eax`), and the true caller return address
  (`*(unsigned int*)($ebp+4)`, or read straight off the stack at that PC) are
  all unambiguous direct reads — no backtrace involved.

This is a genuine methodological find, not just a bug list: five to six
independent "the static sweep is exhausted" verdicts across the session were
each individually reasonable and each individually wrong, because this bug
class is by definition invisible to any check that only compares the
reconstruction against itself.

### All 16 confirmed instances

1. `CModuleManager::AddModule()` / `mModules` — commit `aa8843e` (2026-07-25)
2. `CModuleManager::AddConstructor()` / `mConstructors` — commit `7d5bc26` (2026-07-25)
3. `CChunkMan::Config()`'s Api vtable slot 17 — commit `a357593` (2026-07-25)
4. `CRMApiCallBack`'s vtable (`PTR__CRMApiCallBack_08e886e8`, was a bare `NULL` scalar, not sized as an array at all) — commit `f68d0d9` (2026-07-25)
5. `CApiDescriptor`'s vtable, via `CSysApiInstance::RegisterApi()`'s own Api slot `+0xa4` — commit `3f20499` (2026-07-25)
6. `CPanel`'s own per-instance vtable (`PTR__CPanel_08f7c328` slots Setup/Config/Start) — commit `9a72d91` (2026-07-26)
7. `CEditor`'s own per-instance vtable (`PTR__CEditor_08f29b88` slots Setup/Config/Start) — commit `552cbe4` (2026-07-26)
8. `CDirEntry`'s vtable slot 2 (`HasValidLongNameExt`) — commit `4fd12a5` (2026-07-26)
9. `CSysApiInstance`'s own vtable slots 2-5 (`Pre/PostKernelConstructor`, `Pre/PostKernelDestructor`) — commit `e0758e2` (2026-07-27); live-boot-confirmed on the actual rebuilt binary the same day (see `eva_9th_vtable_fix_live_boot_confirmed_2026-07-27.md`, re-decompiler agent memory)
10-16. The 7-member `XxxApiInstance` sibling family — `EditApiInstance`, `SeqApiInstance`, `ChkApiInstance`, `DumpApiInstance`, `SysExApiInstance` (all 4 slots fixed, same base no-ops as #9), `RTRouterApiInstance` (fixed with 2 real class-specific override functions, not just the shared base no-ops), `RMApiInstance` (3 of 4 slots fixed; slot 2/`PreKernelConstructor` deliberately left `EvaVTableStub` at the time — it needed an entirely unmodeled `CJobStack` class, precisely scoped and documented rather than forced) — commit `34eda81` (2026-07-27), dynamically re-verified live via 26 real gdbstub hits during a real `CKernel::CKernel()` construction pass on the freshly `tools/build_lenny.sh`-built binary, including confirming `RMApiInstance`'s then-still-stubbed slot 2 was itself live-reached (fires, as a no-op, on every real boot)

**UPDATE, same day (commit `7413ad4`): instance #16's remaining stub closed for real.**
`RMApiInstance`'s slot 2 above is no longer a stub — `CJobStack::CJobStack()`/
`~CJobStack()` (.text+0x0814da30 / 0x0814d6c0,0x0814d870) turned out small
and fully self-contained (base-construct-then-vtable-swap, one heap
`CRMJob`, an always-empty job-queue on the only reachable path), so it was
reconstructed for real and wired in (`include/job_stack.h`,
`src/editor/job_stack.cpp`) rather than left stubbed. `RMApiInstance`'s slot
2 now calls a real `CRMApiInstance_PreKernelConstructor()`. This is the one
instance of the 16 where the fix added genuinely new reconstructed logic
(not just a wiring correction to an already-existing function) — see the
deferred/out-of-scope registry entry below for what's still out of scope
within `CJobStack` itself. **Not independently live-boot re-verified past
the prior sweep's confirmation that the call site itself fires on every real
boot** (the `7413ad4` pass was host-KAT-only, "no real-hardware or live-VM
access this pass, per task scope") — the 26-hit dynamic trace above confirms
the slot is genuinely live-reached during `CKernel::CKernel()`, but no
follow-up gdbstub trace has yet confirmed the dispatch now lands on the new
real `CJobStack` ctor rather than crashing or silently misbehaving. Worth a
targeted live re-check (same method as instances 9-16) before/during
real-hardware bring-up.

Running total: 16, found across three sessions (2026-07-25 through 2026-07-27);
all 16 now have a real fix (no bare `EvaVTableStub` left standing in for a
still-out-of-scope class), though #16's fix is not yet dynamically
re-verified live per the note above.
Both the count and the commit-by-commit mapping above were independently
re-verified against this repo's actual `git log`/`git show` output while
writing this entry (not copied uncritically from agent memory) — all 10
commit hashes and their descriptions check out against the real diffs.

### Real-hardware relevance

**Low, for this specific bug class.** All 16 instances are bugs purely in
*this reconstruction's* wiring of its own vtable arrays — the real Kronos
binary's actual vtables were never wrong; every fix was derived from and
verified against a direct byte read of the real binary's own `.rodata`. Once
fixed, each instance was verified via the host KAT suite, a from-scratch
rebuild, and — for 9 through 15, plus the initial (pre-`CJobStack`) wiring
of 16, specifically — a live dynamic trace against the real rebuilt binary
showing the dispatch now lands on the correct function (or, for 16 at the
time, correctly no-ops at a confirmed-reached stub). There is no known
behavioral difference left to test on real hardware *for this bug class
specifically*: real hardware runs Korg's own binary, which never had this
defect, and this project's reconstruction has now had all 16 known instances
of it given a real fix. **One open verification gap remains**: instance
#16's stub was subsequently replaced with genuinely new reconstructed logic
(`CJobStack`, commit `7413ad4`, see the instance list above) after the last
live dynamic sweep, and that specific replacement has only been host-KAT
verified so far, not re-confirmed with a live gdbstub trace the way 9-15
were — worth a targeted live re-check using the same proven method before
treating it as closed with the same confidence as the other 15.

What *is* worth carrying forward into real-hardware bring-up of the
reconstructed code: the **methodology**, not the fix list. If reconstructed
Eva code is ever deployed toward real hardware and something behaves subtly
wrong in a way static review and the host test suite can't explain — especially
anything involving a per-instance object that a generic manager/dispatcher
walks polymorphically — the same two-step method (ground-truth `.rodata`
byte comparison of the suspect vtable's slots, plus a live gdbstub trace
breaking on the dispatch helper's own indirect-call site rather than the
target's entry point) is the proven way to find it, having already
outperformed six rounds of static-only analysis in this exact codebase.

---

## Deferred/out-of-scope registry — quick index, added for the final pre-hardware-testing pass (2026-07-26)

Everything below is a genuine "confirmed genuinely out of scope" or
"confirmed genuinely deep" verdict already reached and fully justified
elsewhere in this tree (`SESSION_SUMMARY_2026-07-25.md`'s "What's still
open", `README.md`'s per-subsystem stage tables, or the relevant
`include/*.h` header comment) — this is NOT new investigation, just a
single place listing every one of them with a one-line pointer, per this
project's standing "log all issues for the end, work through with real
hardware" instruction. None of these need their own detailed entry above
(that format is reserved for genuine real-hardware-BEHAVIOR uncertainty
about code this project HAS reconstructed) — but a real-hardware test pass
should know they exist and are untested/unmodeled, not silently absent.

- **Peg GUI toolkit** (149 classes) — RE-CONFIRMED 2026-07-27, refined: a fresh
  `grep -rn Peg src/ include/` (not just re-reading README.md's own "Survey B"
  prose, whose own evidence — "zero real call sites... into any Peg-prefixed
  class", `grep -rl Peg src/ include/` at the time — is now literally stale,
  predating the later Stage 6 `CEditor`/`CPanelIfcTask` batch) found this
  project's own reconstructed code now DOES contain real call sites into a
  Peg-named symbol: `CEditor::CPanelIfcTask::OnTouchPanelEvent`/`OnButtonEvent`/
  `Exec(CMessage&)` (`panel_ifc_task.cpp`) call a project-local
  `PegMessageQueuePush()` stand-in at the exact real ground-truth call target
  (`.text+0x081a8750`, `PegMessageQueue::Push(PegThing::mpMessageQueue,
  PegMessage const*)`) — but that stand-in is an inert no-op (`(void)queue;
  (void)msg;`, confirmed by direct read of its own body), not real Peg-toolkit
  dispatch, so the SUBSTANTIVE verdict is unchanged: no real Peg-toolkit logic
  (widget rendering, `PegScreen`/`PegThing` internals, `CForm`-family dialogs)
  is reconstructed or exercised anywhere in this project, confirmed again by
  the same "zero Peg-toolkit global constructors run before `main()`" check
  Survey B already did (nothing has changed there). Not tested on real hardware
  because the real Peg toolkit itself still has zero functional presence in
  this reconstruction — only the wording "nothing in this reconstruction's own
  call graph ever needs it" is now imprecise (the call graph DOES reach a
  Peg-named call site, just not real Peg internals). See `README.md` Stage 5's
  "Survey B" section for the original sweep this refines.
- **`CZ` string container** (247 raw symbol count / 59 distinctly-named methods,
  RE-TRACED 2026-07-27) — the CONTAINER (`Insert`/`RFind`/`Remove`/`Sprintf`/
  the real 5 string-building constructors, ~55 remaining methods) stays
  confirmed out of scope project-wide, kept opaque everywhere it's a
  dependency (`CBatchDiskMainTask`, `CConfigManager::CreateResourceFamilies()`).
  BUT the opaque-capacity instance ctor/dtor (`CZ(unsigned)`/`~CZ()`, used by
  every class that embeds a `CZ` MEMBER — `CRMJob`, `CDirEntry`, `CBatchDiskMainTask`
  — cz_util.h) turned out to be a genuinely tractable sub-piece, same "size is
  not depth" lens as `CJobStack`/`CLimiterBase`/`CKGMsgProcessor`: NOW REAL
  (was an all-zero stub), fixing a real, previously-undetected divergence —
  `CDirEntry::GetName()`/`GetExt()` used to always return NULL on a freshly-
  constructed entry; ground truth's real ctor allocates a valid empty-string
  buffer, so they now correctly return a non-NULL pointer to `""`. See
  cz_util.h's own header comment for the full ctor/dtor field-layout writeup
  and dir_entry.h/`verify/test_dir_entry.cpp` for the downstream fix. Real
  string/set container semantics (the 247-symbol/59-method surface minus this
  ctor/dtor pair) are still not tested on real hardware.
- **`CStorage`/`CControlSurface`/`CMMI`/`CModeManager`** — RE-TRACED 2026-07-27,
  CStorage specifically: confirmed genuinely deep with precise fresh evidence,
  not just re-affirmed. Unlike `CJobStack` (whose tiny ctor never touched its
  class's own big method surface), `CStorage::CStorage()` itself is 4421 bytes
  (`.text+0x08a5deb0`) — no small, separable ctor/dtor sub-piece exists (no
  `~CStorage()` was even found in the export; this is a permanent singleton).
  Its own central method, `CStorage::Initialize(CStaticLabel*, PegRect&)`
  (`.text+0x08a57e60`), is 23125 bytes and takes REAL Peg-toolkit widget types
  as parameters — i.e. `CStorage` is directly, unavoidably coupled to the
  still-100%-unmodeled real Peg GUI toolkit (`CStaticLabel`/`PegRect`), not
  just adjacent to it. 21 distinctly-named methods total (42 raw symbols).
  Confirmed genuinely deep UI/control-surface state backing
  `CSTGUnsolMsgHandler`'s remaining Tier-B leaves below (`ControlMsgHandler`'s
  38 still-unpromoted outer subcodes, `VoiceModelMsgHandler`'s one
  MOSS-algorithm leaf); not tested on real
  hardware because none of these classes are modeled beyond stub
  declarations.
- **`CSTGUnsolMsgHandler::ControlMsgHandler`** — PARTIALLY PROMOTED 2026-07-27
  (commit `d8a30ed`): re-applied this project's own "size is not depth, check
  for a tractable sub-piece" lens (already validated on `CJobStack`'s and
  `CEditClient`'s ctor/dtor) to the last remaining full Tier-B handler. 6 of
  its 44 outer subcodes turned out to be provably self-contained (zero
  crossjump into or out of their own code range) and now have real bodies:
  subcodes 37/38 call two **new** real `USTGAPIControl` methods
  (`BeginLongErPActivity()`/`EndLongErPActivity()`, same `ErPShutdownMsgShape`
  send shape as the existing `ForceErPShutdown()`), subcode 16 reuses the
  already-real `CEditor::IsSwitchPressed()`, and subcodes 9/10/11 each do a
  bounds-checked lookup in a real `.rodata` button-code table before falling
  into a shared tail that reuses the already-real
  `CEditor::CPanelIfcTask::OnButtonEvent()`.
  The other 38 subcodes are CONFIRMED genuinely deep, and more precisely so
  than before: a full jump/call-target-vs-owning-case-range audit of the real
  disassembly found GCC has cross-jumped/tail-merged them into a single
  ~2KB interconnected CFG hub that carries essentially all 18 out-of-scope
  subsystem calls previously documented (`CMMI`, `CControlSurface`,
  `CHelpManager`, 4 real Peg-toolkit `CForm` dialogs, `CModeManager`,
  `CDiskUtil`, `CSmplModeMgr`, and raw HAL interrupt-mask control) — one
  shared hub reached from ~20 directions, not 44 independent leaves, so
  reconstructing any one of those subcodes faithfully would require
  reconstructing the hub too. Those 38 still route to a documented
  not-implemented stub (`ControlMsgHandlerUnimplementedSubcode()`); a real
  message hitting one of them on real hardware currently gets no real
  behavior from this reconstruction.
  **Real hazard found in ground truth, flagged for the real-hardware
  comparison pass**: subcode 11's real disassembly (0x0891b4ce/0x0891b4d1)
  only excludes `code==7` exactly and has **no upper-bound check at all**
  before indexing its 9-entry button-code table — for `code==9` the real
  binary reads `0x201f` out of unrelated adjacent `.rodata` (not a real
  button code), and `code` 10–15 read zeros. This reconstruction deliberately
  does **not** reproduce this as an out-of-bounds C array read (undefined
  behavior); it adds an explicit `code < 9` bound instead, matching the real
  "0 → no-op" behavior for every input except the one genuinely garbage
  `code==9` case. Open question for real hardware: does a real `code==9`
  message on this subcode actually crash/misbehave on real hardware, or is
  there some other guard (upstream sender never producing `code==9`, a caller
  contract this project hasn't seen, etc.) that makes it harmless in
  practice? Worth deliberately exercising this exact input during
  hardware bring-up.
- **`CSTGUnsolMsgHandler::VoiceModelMsgHandler`** — RECONSTRUCTED FOR REAL
  2026-07-27 (commit `786fcd5`, Tier A batch 8), promoted from Tier B: a
  from-scratch `objdump -dr -M intel` re-trace of the real 2512-byte
  function, both real jump tables (17 + 6 entries) fully case-traced against
  their real `.rodata` bytes. **One leaf stays genuinely out of scope**: a
  `CStorage::GetInstance()`-based "MOSS algorithm" voice-model-database
  dispatch (real call site `0x08917209`, confirmed via a real `.rodata`
  string naming `MOSSAlgorithmDatabase.h`) — an entirely unmodeled class
  hierarchy, precisely documented (see `include/stg_unsol_msg_handler.h`'s
  own header comment) rather than guessed at. A real message that reaches
  specifically this leaf on real hardware (per-slot "type" byte in `[2,9]`
  AND subindex `>5`, gated on `(DAT_0af0df1e&7)==3`) currently hits an
  unimplemented stub in this reconstruction; every other case in the handler
  is real and reconstructed. (This pass also fixed a real, previously-hidden
  bug the new test coverage caught: several per-case `GetScopeId()` calls
  had been hoisted above their own case's real bound check, meaning they
  would have fired even on real out-of-range bail paths — fixed by
  threading scope resolution through bool-returning `Compute*()` helpers
  matching real disassembly case-by-case order.)
- **`CEditClient`'s ctor/dtor** — RECONSTRUCTED FOR REAL 2026-07-27 (commit
  `386c295`), 3rd re-open of a class previously left "genuinely deep." A
  fresh `objdump -dr -M intel` trace of `CEditClient::CEditClient()`/
  `~CEditClient()` found the construction/destruction path never touches the
  `PointerHash<K,V>` template's own Add/Find/Node/Iterator machinery — a
  whole-binary xref sweep confirms `PointerHash<CEditControl*, CEditControl>`/
  `PointerHash<long, CEditControl>` are its ONLY 2 instantiations anywhere,
  both consumed only by this one ctor (2x malloc + vtable-install +
  zero-fill, not real hash logic). Also fixed a real latent bug the old
  Tier-B stub had: it set `mVtbl = 0`, which would have NULL-deref crashed
  the first time a genuinely-constructed client's vtable got dispatched.
  **Still genuinely out of scope**: `CEditClient`'s other 4 real named
  methods (`BlockRegister`/`Register`/`Unregister`/`NotifyControls`) and
  `PointerHash<K,V>`'s own hash-table methods themselves (comparable in
  scope to `CZ`) — same "reconstruct only what the traced call graph needs"
  precedent as `CJobStack` below. **One divergence worth flagging for
  real-hardware bring-up**: `CEditClient`'s own vtable slot 2 (`OnNotify`,
  real ground-truth address `.text+0x0806f6e0`) is real and load-bearing —
  `CEditMan::CMainTask::Notify()` (already reconstructed for real,
  `include/edit_man.h`) dispatches every registered client through exactly
  this slot on every real notification fan-out — but `OnNotify` itself is
  NOT independently reconstructed here and stays `EvaVTableStub` (a silent
  no-op), out of scope per the same header comment. Since `CEditor`'s own
  embedded `mEditClient` member is the one confirmed real construction site
  on this project's currently-wired boot path, a live boot that reaches a
  real notification fan-out would silently no-op instead of updating
  whatever real UI/control-surface state `OnNotify` is supposed to touch —
  a known, documented gap (not a bug), but one that could look like a
  missing-notification symptom during real-hardware bring-up if this log
  entry isn't consulted first.
- **`CDumpManStateMachine` family** — STALE as of 2026-07-27, re-verified and
  corrected: this bullet previously read as if nothing had been reconstructed,
  but the Stage 6 DumpManager cluster batch (2026-07-25) and its 2026-07-26
  re-check already promoted `CCircByteBuffer`/`CDumpBuffer`/`CDumpManMod`/
  `CDumpTask`/`CBufferingTask`'s ctor+dtor pair and `CDumpManStateMachine`'s
  own ctor/dtor/`Init()`/`CDumpMachine`'s full 6-method I/O-adapter surface
  (`SetTimeout`/`SendSexMessage`/`PutMessage`/`ReadPacket`/`WritePacket`/
  `IsDumpEnded`) to real bodies — see dump_man_state_machine.h/dump_buffer.h/
  buffering_task.h for the full per-method breakdown. A fresh 2026-07-27
  `objdump -dr -M intel` re-trace of the REMAINING Tier-B leaves
  (`CBufferingTask::Exec(CMessage&)`/`Put(uchar const*, uchar)`,
  `.text+0x080cd930`/`0x080cde50`) confirmed they are still genuinely deep:
  `Put()`'s own real disassembly dispatches into `CChunkClient::Abort()`/
  `LoadDump()`/`LoadFile()`/`SaveDump()`/`StoppedByUser()` and
  `CDumpHeaderDescr`/`CDumpReqDescr`'s own `DeSerialize()`/ctor/dtor pairs —
  an entirely separate, un-reconstructed chunk-transfer-serialization
  subsystem, not a shallow forward. `CDumpManStateMachine::OnTimeout()`
  (`.text+0x080d0150`) was also re-traced: it dispatches through a 9-entry
  jump table keyed on internal protocol state (its own `+0x8` field) into the
  genuinely out-of-scope ~30-method state-handler family — confirms, does not
  reverse, the existing verdict for that specific piece. Net: real SysEx/
  dump-file-transfer serialization (the `CChunkClient`/`CDumpHeaderDescr`/
  `CDumpReqDescr` side) and the state-handler protocol logic itself remain
  untested; the I/O-adapter shell around them is real and exercised.
- **`COutLinkIfcBase`/`CMarshaller<T>` framework** — STALE as of 2026-07-27,
  corrected: this bullet's "no concrete instantiation exists yet" claim was
  already half-wrong even before today (`CAlphaKeybCtrlTask`'s own `mCodeIfc`,
  alpha_keyb_ctrl_task.h, constructed on this project's own wired boot path
  since the 2026-07-26 CAlphaKeybCtrl/CAlphaKeybCtrlTask batch). A fresh
  `objdump -dr -M intel` re-trace (2026-07-27) found a SECOND, independent
  real instantiation site: `CLimiterBase::Init(CTask&, unsigned int)`
  (`.text+0x0807ac70`) builds a `COutLinkIfc<ILimiterNotify>`/
  `CMarshaller<ILimiterNotify>` sub-object via the exact same "malloc + base-
  construct + raw vtable pokes" idiom, confirmed via a direct `.rodata` byte
  read of `PTR__CLimiterBase_08e81c90`/`PTR__CWrProtCircularQueue_08e81ca8`
  (only 4 and 2 real virtual slots respectively). `CLimiterBase`'s own ctor/
  dtor and its embedded `CWrProtCircularQueue` message-ring-buffer (ctor/
  dtor/`Init(int)`/`IsEmpty()`/`CountIntegers()`) are now reconstructed for
  real (`limiter_base.h`/`.cpp`, commit pending) — `CLimiterBase` itself has
  ZERO callers anywhere in the whole 22MB ground-truth binary (dead code in
  ground truth itself, not just this reconstruction — confirmed by grepping
  every `call` target in a full `objdump -dr` sweep), so this is structural
  completeness, not a reachability change. The `COutLinkIfcBase`/
  `CMarshaller<T>` framework itself (and `Init()`'s own base-construction
  call, which needs it) stays genuinely out of scope, shared by
  `IAlphaKeybEvent`/`IAlphaKeybCtrl` too — not tested on real hardware
  because, while 2 concrete instantiation SITES now exist in this
  reconstruction, neither one's own framework-level machinery
  (`GetDirectIfcPtr()`'s callee-side behavior aside, already real) is
  modeled.
- **10 `CXxxTask` ES-family UI god-objects** (`CESCommonTask`/`CESEffectTask`/
  `CESCombiTask`/`CESGlobalTask`/`CESMOSSTask`/`CESSamplingTask`/
  `CESSetListTask`/`CESSongTask`/`CESDiskTask`/`CESProgTask`, 66–1106 raw
  symbols each) — RE-TRACED 2026-07-27, `objdump -dr -M intel` on all 9
  non-`CESCommonTask` ctors (`CESCommonTask` itself already has a real ctor,
  es_common.h). Confirmed deliberately out of scope, with a precise,
  size-correlates-with-depth finding this time (not just re-affirmed):
  ctor sizes range from 293 bytes (`CESEffectTask`, smallest) to 4061 bytes
  (`CESSongTask`, largest), and the two smallest ctors traced
  (`CESEffectTask`/`CESMOSSTask`) are structurally SIMILAR to `CJobStack`'s own
  tractable shape — `CTask`/`CEditable` base construction (already real) plus
  2 `CEditable::AddDescriptorsMap()` calls (already real) — but, UNLIKE
  `CJobStack`, each one ALSO unconditionally heap-constructs one brand-new,
  not-yet-reconstructed per-class "manager" helper object of its own
  (`CEffectManager::CEffectManager(int)`, .text+0x08bec0c0, 533 bytes/11
  methods, for `CESEffectTask`; `CMOSSManager::CMOSSManager(unsigned)`,
  .text+0x08bf3940, 533 bytes/10 methods, for `CESMOSSTask`) — i.e. even the
  cheapest member of this family is not "free" the way `CJobStack`'s ctor was
  (zero new dependency classes); every unlock drags in at least one full new
  class. `CESSongTask` (the largest) confirms the opposite end is genuinely
  deep, not just big: its ctor directly constructs a `CSongEditBuffer`, calls
  `CMIDI::SetSongEditBuffer()` and `CSTGUnsolMsgHandler::InitializeForSong
  (CCombi&, CCombi&)` (real cross-subsystem calls into the MIDI/voice engine),
  and registers DOZENS of `AddDescriptorsMap()` rows (vs. 2 for the small
  ones). Net: not constructed anywhere on the currently-wired boot path; the
  actual per-editor-page UI/model logic behind every edit screen is entirely
  unmodeled. `CESEffectTask`/`CESMOSSTask` specifically are a plausible target
  for a FUTURE dedicated batch (same "CBatchDiskMan/CPanel/CEditor unlock"
  shape, own new CEffectManager/CMOSSManager class each) — not forced here,
  consistent with this pass's own brief not to force a reconstruction just to
  find something.
- **`CClientCommServer::OnReceiveMessage`** — RECONSTRUCTED 2026-07-27, closing
  `CClientCommServer` to a full 26/26. The long-standing "genuinely blocked on
  a real `CMessage` definition" verdict turned out to be based on the
  parameter type alone, not an actual disassembly: a from-scratch `objdump -dr`
  trace (ground truth @0x08172010, 784 bytes) found it needs only 3 fixed
  `CMessage`-offset reads (`+0x10` data ptr, `+0xa` len byte, `+0x4` an opaque
  3-level pointer chase to a byte@+0x8c tag), the SAME
  `reinterpret_cast<const unsigned char*>(&msg)`-fixed-offset convention this
  project already used successfully in CPoller/CChunkServer/
  CSysExMsgTaskBase, plus dispatch through the 3 already-real
  `OnRxMsgWhenIn{IDLE,SENT,WAIT}` siblings. Closing it also surfaced and fixed
  a real, previously-hidden discrepancy: `OnRxMsgWhenInIDLE()`/
  `OnRxMsgWhenInSENT()` had been committed as `void`, but ground truth's real
  return value is `CSexServiceTask::TransmitSysEx()`'s own, propagated
  through unchanged (confirmed via a genuine tail-jmp sibcall in one path and
  a captured-eax `call` in the other) — invisible until `OnReceiveMessage()`
  became the first real caller to use the result. Verified via 18 new host
  KAT checks (including one that independently confirms the opaque
  pointer-chase reads the correct byte) plus the full existing 38-check host
  suite, all green; `test_client_comm_server` (the project's one known
  build-dependent heisenbug) re-run 3x clean. **Not yet live-boot exercised**:
  `CClientCommServer`'s own sole real ground-truth constructor caller,
  `CSexServiceTask::RegisterMessageClient()`, remains a separate, deliberately
  out-of-scope ~800-byte dependency (see this class's own header comment) --
  so this fix, like the rest of the class, is verified via host KAT/
  disassembly cross-check, not a live gdbstub trace; no per-instance vtable
  was touched by this fix (`CClientCommServer::mVtbl` stays correctly
  null/unwired, confirmed this same pass to be a non-issue rather than an 18th
  vtable-dispatch-stub-gap instance, since nothing in the CURRENT
  reconstruction's call graph ever dispatches through it).
- **`CJobStack` construction/destruction** — RECONSTRUCTED FOR REAL
  2026-07-27 (commit `7413ad4`), closing `RMApiInstance`'s last stubbed
  vtable-dispatch-stub-gap slot (see "All 16 confirmed instances" #10-16's
  own update note above). `CJobStack::CJobStack()`/`~CJobStack()`
  (`.text+0x0814da30` / `0x0814d6c0`,`0x0814d870`) turned out small and
  fully self-contained — base-construct-then-vtable-swap, one heap `CRMJob`
  (already-real), an always-empty job-queue vector on the only reachable
  path — so it was reconstructed for real (`include/job_stack.h`,
  `src/editor/job_stack.cpp`) rather than left stubbed. **Still genuinely
  out of scope, same `CResMan`/`CJobStack` "god object" family as
  `CBatchDiskMainTask` below**: `CJobStack`'s own 8 `AddLoadRes`/
  `AddLoadFile`/`AddLoadSingleRes` (x2)/`AddSave`/`AddDelete`/`AddSetRes`/
  `ExecutePendingCmds` job-queue business-logic methods (real,
  `.text 0x0814dac0..0x0814f800+`, ~0x2d00 bytes total, `TVector<...>`/
  `TPtrArray<SLoadBankOffset>`-driven) — zero reachable caller on this
  project's traced boot path, so a real bank-load/save job queued on real
  hardware is entirely unmodeled here. Also install-only, never dispatched
  through by any reconstructed caller: `CJobStack`'s own secondary
  (multiple-inheritance/IFC) vtable, 2 real, unidentified functions
  (`.text+0x0818f8b0`/`0x0818fb00`). Verified via new
  `verify/test_job_stack.cpp` (14 checks) plus the full existing host suite,
  both green; **not live-boot re-verified this pass** (host-KAT-only, per
  task scope) — see the vtable-dispatch-stub-gap section's own note above
  for the specific open follow-up (confirm live that `RMApiInstance`'s slot
  2 now dispatches into this real ctor, using the same gdbstub method that
  verified instances 9-15).
- **`CBatchDiskMainTask`'s 5 heaviest methods** (`PreloadDir`/
  `PreloadGroup`/`PrepareGroupsForPreload`/`AddItemToPreload`/
  `Exec(CMessage&)`) — confirmed `CZ`-container-scale, deferred; real
  batch-disk-preload behavior (loading a large sample/multisample set from
  the real SSD) is unmodeled by this reconstruction.
- **`CDataHandler`/`CEditServer`/the 10 `CESxxx` model classes** — real
  shell reconstructed, but confirmed NOT reachable from the currently-wired
  boot path (gated behind `CreateUserModules()`'s own placeholder config
  table content) — not a real-hardware behavior question until that gate
  is itself resolved.
- **`CKGMsgProcessor`** — PARTIALLY PROMOTED 2026-07-27: this class was only ever
  cited elsewhere in this project as one of `ControlMsgHandler`'s/`GlobalMsg-
  Handler`'s/`CombiMsgHandler`'s downstream out-of-scope dependencies (README.md),
  never independently re-examined with its own fresh trace. A from-scratch
  `objdump -dr -M intel` re-trace of `CKGMsgProcessor::CKGMsgProcessor()`
  (`.text+0x08913620`, 570 bytes) / `~CKGMsgProcessor()` (`.text+0x08913860`, 134
  bytes) / `GetInstance()` (`.text+0x089138f0`, 134 bytes) found the SAME "size is
  not depth" shape already proven for `CJobStack`/`CLimiterBase`/`CEditClient`:
  9 mallocs + fixed-offset field writes + one already-documented `Api+0x9c`
  vtable dispatch (`timer_engine.h`'s `ApiGetDefault9c()`, reused verbatim), no
  `CZ`/`CStorage`/`CMMI`/`CModeManager` dependency of its own — now reconstructed
  for real (`kg_msg_processor.h`/`.cpp`). Confirms, does not reverse, the existing
  "genuinely deep" verdict for the REST of the class: `SetGEMax`/`Process`
  (1110B)/`CheckAndSetChordName`/`CheckAndSetCCsDisplay`/`CheckAndUpdateDisplay`/
  `CheckAndSetNotesDisplay`/`CheckAndSetRTValueString`/`ClearInvalidNotesCCsDisplay`
  (1679B)/`GetKarmaNotes` are all genuine Karma-note-generation/display logic
  dispatching through 7 real, independently-vtable-confirmed but otherwise
  unreconstructed handler classes (`CKGCommonMsgHandler`/`CKGModuleMsgHandler`/
  `CKGUIControlMsgHandler`/`CSPRUIControlMsgHandler`/`CSPRUICommonParamMsgHandler`/
  `CSPRUIAudioTrackParamMsgHandler`/`CSPRUIDrumTrackTrackParamMsgHandler` — 8 to 51
  real virtual slots each, sizes confirmed via direct `.rodata` byte reads, not
  `nm -C` size fields), still entirely out of scope. Two real, faithfully-
  transcribed ground-truth quirks worth flagging for the real-hardware comparison
  pass: the ctor leaves one byte field (`+0x29`) genuinely uninitialized (never
  written anywhere in the real ctor, only `+0x28` is), and the destructor frees
  all 7 polymorphic handler sub-objects via their own real deleting-destructor
  vtable slot but never the 2 plain (`+0x20`/`+0x24`) data buffers it also owns —
  a real per-destruction leak, not a bug in this reconstruction. No live caller
  of this real reconstruction on this project's own traced boot path
  (`GetInstance()`'s one existing real caller, `ProgramSlotMsgHandler` in
  `stg_unsol_msg_handler.cpp`, deliberately keeps using its own separate
  file-local opaque stub rather than this header, to avoid touching that
  already-verified call site outside this batch's own scope) — reconstructed for
  structural completeness, same precedent as `CLimiterBase`/`CJobStack` above.
- **`CBDApiInstance`** — RECONSTRUCTED FOR REAL 2026-07-27, RETRACTING a "confirmed
  genuine dead end" verdict reached (and independently re-confirmed) TWICE before
  (README.md's Stage 6 breadth-sweep re-check, 2026-07-25, and the
  `CAlphaKeybIfcTask` batch's own re-check the same day). Both concluded
  `RegisterLoader(CBatchDiskMan*)` — the one method with a plausible real caller —
  had zero call sites anywhere in the 37,795-function export. A from-scratch
  `objdump -dr -M intel` re-trace (dispatched specifically to re-examine this
  project's largest deferred items with the same "size is not depth" lens that had
  just unlocked `CLimiterBase`/`CKGMsgProcessor`) found this was a false negative:
  the correct Itanium mangled name is `_ZN14CBDApiInstance14RegisterLoaderEP13
  CBatchDiskMan` (14/13-character length prefixes; the prior greps apparently used
  mistyped 13/12 prefixes and matched nothing). A correct
  `objdump -dr | grep "call.*8243980"` finds exactly ONE real call site:
  `CBatchDiskManConstructor::Create()` (`.text+0x08243d80`) — a function this
  project ALREADY reconstructed (`CBatchDiskManConstructorCreate()`, mains.cpp),
  which deliberately omitted this exact call. Worse, that caller is the SAME
  function the "CBatchDiskMan unlock batch" (2026-07-26) already made
  boot-path-reachable via `CConfigManager::CreateUserModules()`'s "BatchDiskManClass"
  row — i.e. `RegisterLoader()` was reachable from the currently-wired boot path the
  whole time, not dead code, and omitting it was a real (if inconsequential) gap in
  an already-shipped reconstruction. All 6 of `CBDApiInstance`'s own methods turned
  out genuinely tractable and are now real (`bd_api_instance.h`/`.cpp`):
  `RegisterLoader()` (a `TVector<CBatchDiskMan*,1>` push_back, real
  `MakeCapacity()` growth policy transcribed from its own disassembly — a
  DIFFERENT curve from the project's other `TVector<T,1>` instantiation,
  `task.cpp`'s `TVector<CTask::SRegisteredIfc,1>`, confirmed rather than assumed
  by analogy) is now wired into `CBatchDiskManConstructorCreate()` for real.
  `IsBusy()`/`IsPreloadRunning()` x2/the dtor have ZERO callers anywhere in ground
  truth ITSELF (confirmed directly, same status as `CLimiterBase`) — reconstructed
  for structural completeness only. Not tested on real hardware because
  `CBDApiInstance`'s own base-class `CGlobalObjectBase` registration (the real
  static-constructor mechanism every other `XxxApiInstance` sibling uses,
  `global_object_base.h`) is deliberately not modeled here — nothing on this
  project's traced boot path ever dispatches virtually on this object or destroys
  it, only `RegisterLoader()`'s own plain (non-virtual) call path matters.
- **`TVector<T,N>` re-check (2026-07-27): confirmed NOTHING else in this registry is
  unblocked by it.** Following up the same-day `TVector<T,N>` reconstruction
  (`tvector.h`, commit `d5e2e56`) that closed `CPool`/`CSlotPool`, this pass
  specifically re-read every deferred entry above and grepped this project's own
  `include/`/`src/` for "unmodeled TVector" / "needs TVector" / "blocked...vector"
  phrasing to check for other classes citing the same blocker. Result: a clean
  negative. `CJobStack`'s 8 job-queue methods and `CBatchDiskMainTask`'s 5 heaviest
  methods both mention `TVector<...>` in their own real disassembly, but their
  actual documented blocker is the much larger `CZ`/`CResMan` "god object"
  business-logic surface those methods are made of — `TVector<T,N>` itself was
  never their limiting factor, just one ingredient among several genuinely deep
  ones (confirmed by re-reading `job_stack.h`'s and `batch_disk_main_task.h`'s own
  header comments, not re-guessed). Every other `TVector<T,1>` instantiation
  named elsewhere in this project (`limiter_man.h`, `task.h`, `poller.h`,
  `ckernel.h`) was already real before today. No action taken — this confirms the
  earlier "genuinely deep, not just TVector-shaped" verdicts rather than reversing
  any of them.
- **USTGAPIXxx thin-IPC-facade family — non-CValue slice RECONSTRUCTED 2026-07-27**
  (`ustg_api_wrappers.h`/`.cpp`, 21 methods across 10 classes: `USTGAPICombi` (8),
  `USTGAPIEffect`/`USTGAPIEffectSlot`/`USTGAPIEffectMgr`/`USTGAPIGlobal`/
  `USTGAPIHDRTrack` (1 each), `USTGAPIProgramSlot` (2), `USTGAPISetList` (1),
  `USTGAPIDrumkitData` (2 of 3), `USTGAPIPatch` (1), `USTGAPIWaveSequenceData` (2
  of 3)). Found via a fresh `nm -C` whole-binary class-inventory sweep (same
  technique that found `CResFamily`/`CPool`/`CSlotPool`) cross-referenced against
  `ustg_user_api.h`'s own header comment, which already named this ~150-method
  family as the largest remaining unclaimed USTGUserAPI surface. Every method
  builds a small fixed-size STGMessage-shaped struct on the stack (transcribed
  field-by-field from real disassembly, not assumed by argument-count analogy --
  several methods place C++ parameters into the wire struct in a DIFFERENT order
  than the parameter list, e.g. `UpdateProgramSlotParameter`'s real payload order
  is a2,a3,a4,a5,a6,a8,a1,a7) and forwards it via the already-real
  `USTGUserAPI::SendSTGMessageWithSource()`; the 3 "SharedMemXxxDump"-named
  methods additionally poll `USTGUserAPI::ReadMessage()` up to 8x for a
  subcode-echo ack, using a shared new `WaitForDumpSubcodeEcho()` helper (ground
  truth inlines this loop separately 3x with byte-identical control flow -- real
  behavior unchanged by factoring it). Verified with 25 new byte-exact wire-format
  KAT checks (`verify/test_ustg_api_wrappers.cpp`) that read back the literal
  bytes written to a test-hooked pipe and compare field-by-field against the
  decoded shape, plus the full existing host suite (0 failures) and a real Lenny
  cross-build+link ("LINK OK").
  **Deliberately deferred, same batch**: 4 real sibling methods that take a
  `CValue const&` argument (`USTGAPIDrumkitData::UpdateVSplitParam`,
  `USTGAPIVoiceModel::UpdateParam`/`UpdateLinkedParam`,
  `USTGAPIWaveSequenceData::UpdateStepParam`) -- their own real disassembly
  memcpy()s a SELF-DESCRIBING, VARIABLE-LENGTH byte range starting at the CValue
  object itself (`size = *(byte*)((char*)cvalue+1) + 4`, i.e. CValue's own
  byte-at-offset+1 is a length prefix for its own variable-length payload). The
  serialization RULE is fully decoded (see `ustg_api_wrappers.h`'s header
  comment); what's missing is CValue's own field-level layout/semantics, not
  modeled anywhere in this project (same boundary as `CMessage`/`STGMessage`
  themselves) -- a precise, tractable-with-more-effort lead for a future pass.
  `USTGAPIPatch`/`USTGAPIVoiceModel`'s own static data members
  (`m_DefaultProgramId`/`m_DefaultBankId`) also not reconstructed (no traced
  caller touches either). Also out of scope this batch, each confirmed a
  materially different shape from a direct disassembly spot-check rather than
  assumed by family membership: `USTGAPIKLM` (15 methods, `CSTGHandle::Access()`-
  based shared-memory table reads, not message sends), `USTGAPICDAudio` (12),
  `USTGAPIMIDI` (23, real device-queue I/O against 4 static per-port
  `CSTGHandle`s + `CSTGMidiQueue`), and by far the largest,
  `USTGAPIPCMBanks`/`USTGAPISampling` (51/46 methods -- not spot-checked,
  presumed genuinely deep sample-loading logic given their size, a natural
  target for a dedicated future pass). Not tested on real hardware -- none of
  these methods have a reconstructed caller anywhere in this project yet (same
  "structural completeness, not reachability" status as the rest of the
  USTGUserAPI Stage 2 substrate they depend on).
- **USTGAPIKLM (14/15) + USTGAPICDAudio (12/12) + 4 USTGAPISampling primitives
  RECONSTRUCTED 2026-07-27** (`ustg_api_klm.h`/`.cpp`, `ustg_api_cdaudio.h`/`.cpp`,
  `ustg_api_sampling.h`/`.cpp`, `cvalue.h`-in-`eva_types.h`/`cvalue.cpp`).
  Follow-up to the entry above: traced all 5 "confirmed different-shaped" sibling
  classes via direct `objdump -dr -M intel` reads to actually characterize why,
  not just note that they differ.
  **USTGAPIKLM** turned out genuinely tractable: 13 of 15 methods are thin
  wrappers around one real worker, `GetProductInfo()` (.text+0x08e1d690), which
  attaches the real installed-EXs-product shared-memory table via
  `CSTGHandle{mode=1}.Access()` (already-real, `stg_handle.cpp`) and reads a
  164-byte-per-product record array. Confirms the "call `Access()` again on the
  pointer it just returned" quirk `USTGUserAPI::Connect()` already exercises for
  `mFrontPanelStatusAddress` is a general `CSTGHandle` idiom (`Access()` treats
  whatever int sits at `this` as a cache-table index, stg_handle.cpp), not a
  one-off -- `GetProductInfo()`/`GetProductItemInfo()` both do it too, the latter
  a 3rd time for a product's own per-item sub-table. Real, non-obvious quirk
  caught mid-transcription: `GetProductShortName()` and
  `GetProductOptionFileName()` read the OPPOSITE-sized record fields from what
  their names alone would suggest (ShortName -> the 16-byte field, OptionFileName
  -> the 5-byte field, e.g. "S010" -- CLAUDE.md's own S-file naming convention).
  Decoded and used both of CValue's serialization rules for the first time in
  this project (previously only the variable-length-blob rule was known, from
  the deferred batch above) -- `GetProductInfo()`'s own real disassembly writes
  a FIXED "scalar dword" CValue encoding (tag=1,len=4,pad2,dword) for a
  product's identifier, confirming the byte-1-length-prefix convention is
  CValue's general wire shape, not specific to the variable-length case. Also
  reconstructed the 2 real `/proc/.oacmd` command-channel free functions
  `RescanInstalledProducts()`/`SetAuthString()` tail-call
  (`SendCommandRescanInstalledProducts` "SO:*", `SendCommandAuthorizeOption`
  "AU:%s") -- confirms/extends CLAUDE.md's already-documented "AU:" trigger with
  its general request/response envelope (write command, read back a 4-byte int
  status, 0=success) for the first time from the real CLIENT-side sender rather
  than just OA.ko's consumer. **Deliberately deferred**: `InstallOptionFile()`
  calls `CSTGInstalledEXProducts::InstallProductFile()`, a genuinely deep real
  S-file/option-file binary parser+installer (5 more real methods across 3
  classes plus 2 callbacks, .text+0x08e32450-0x08e33900) -- a project of its own,
  matching this batch's overall verdict for the family.
  **USTGAPICDAudio** (all 12 methods) doesn't build STGMessages inline at all --
  every method routes through 4 real `USTGAPISampling` primitive methods
  (`SharedScratch`/`SendSimpleMessage`/`ReceiveSimpleMessage`/`ReceiveMessage`),
  themselves genuinely tractable and reconstructed here for the first time.
  These primitives share the SAME 24-byte STGMessage substrate
  (`USTGUserAPI::SendSTGMessageWithSource`/`ReadMessage`) the batch above's whole
  family uses, but with an INVERTED field-role split: there, `type` identifies
  the subsystem and `subcode` the per-command opcode; here `type=1` is constant
  ("simple 3-int command" shape) and `subcode=0xc` is the constant Sampling/
  CDAudio subsystem id, with the real per-command opcode living in a PAYLOAD
  dword instead (position varies per primitive, documented precisely per-function
  in `ustg_api_sampling.h` rather than asserting one unified scheme).
  `SharedScratch()` turned out to reuse `USTGUserAPI::mFrontPanelStatusAddress`
  (already real) at a fixed +0xd34 offset as a general scratch buffer, not a new
  shared-memory attach. A quick 4-method spot-check of `USTGAPISampling`'s own
  ~46 "UpdateXxx" methods (`UpdateLevelSlider`/`UpdateTrigger`/`UpdateThreshold`/
  `UpdatePretrigger`) confirms they build their OWN STGMessages directly the same
  way the batch above's family does, NOT through these 4 shared primitives --
  so `USTGAPIPCMBanks`/`USTGAPISampling`'s "genuinely deep" verdict stands for
  the bulk of both classes; only the 4 shared primitives CDAudio needed are done
  here, a real, scoped, verified lead for a future pass rather than a guess.
  `USTGAPIMIDI` re-checked and confirmed materially deeper than either of the
  above: real device-queue I/O against `sQueueReaders`/`sQueueWriters` static
  arrays and a `CSTGMidiQueue` class (real tail-call target
  `CSTGMidiQueue::GetNumWritableBytes()` at .text+0x08e47e70), guarded by real
  `__assert_fail()` calls (confirmed real source path
  "../StgAPI/UserAPI/USTGAPIMIDI.cpp" and assert text "sMidiShare"/
  "IsValidPortId(port)" from .rodata) -- left deferred with this more precise
  characterization for a future pass rather than the prior batch's shallower
  "different-shaped" note.
  Verified with 26 new byte-exact/behavioral KAT checks
  (`verify/test_ustg_api_cdaudio.cpp`, covering both CValue rules + all 4
  Sampling primitives + a representative CDAudio slice incl. `PlayStandby`'s
  2-separate-wire-sends-per-call shape) plus the full existing 47-binary host
  suite (0 failures) and a real Lenny cross-build+link ("LINK OK"). USTGAPIKLM's
  own `CSTGHandle::Access()`/`/proc/.oacmd`-dependent methods compile clean
  (`make objs`) but are not host-KAT-tested at the byte level -- same
  already-accepted "real shared-memory/file I/O boundary, not mockable
  host-side" limitation `CSTGHandle::Access()` itself already carries. Not
  tested on real hardware -- no reconstructed caller anywhere in this project
  yet (same status as the rest of the Stage 2 substrate). Eva manifest
  646 -> 678/37,795.
- **`CScsiDriverBase::SetCommandParameter`/`Execute`/`AfterProcess`+6
  `AfterProcessXxx`** (2026-07-28 storage-cluster follow-up) — 11 methods,
  `.text+0x08308760`-`0x0830ab30`, deliberately deferred while the class's
  other 39 methods (ctor/dtor/`GetResultOfScsiCommandAsync` + all 35 standalone
  `SetParamXxx(SDriverIOPbuf*)` CDB builders) are now fully reconstructed --
  see `include/scsi_driver_base.h`'s own header comment for the complete
  per-method address/size table. `SetCommandParameter` alone is a 2935-byte,
  39-case `jmp [ecx*4+...]` jump table that reimplements, INLINE, the exact
  same per-SCSI-command CDB-building logic each standalone `SetParamXxx`
  already covers (spot-checked several cases byte-for-byte identical in
  structure) -- genuinely tractable in principle (same field-mapping technique
  already worked out for the 35 siblings) but a large mechanical transcription
  effort on its own, left for a dedicated follow-up rather than rushed.
  `AfterProcess`/`AfterProcessXxx` are the untouched response-side mirror
  (parses returned sense/mode/inquiry data, `sm_oDataBuf`-shaped) -- not yet
  even field-mapped. Independently confirmed and worth flagging for
  real-hardware relevance: none of the 35 already-reconstructed `SetParamXxx`
  methods has ANY caller anywhere in the whole `Eva` binary (verified by
  grepping every `call` instruction against all 35 addresses) --
  `CScsiDriverBase::Execute()` (also deferred, `.text+0x0830a6c0`) only ever
  calls `SetCommandParameter()` and `AfterProcess()`, meaning the deferred pair
  is what's actually on any real optical-drive I/O path, while the 35
  reconstructed methods are dead code kept for completeness under the
  "decompile everything" goal. `CFileIoCdda` (41 overrides, largest single
  method 1866 bytes) and `CFileIoUdf` (35 overrides, `format()` alone 4791
  bytes of real VFAT-format logic) were surveyed and explicitly passed over
  this batch as less tractable than `CScsiDriverBase`'s mostly-small,
  mostly-independent CDB builders -- both remain fully out of scope, still
  real future-pass candidates. Also worth recording: `file_io_base.h`'s
  original "OUT OF SCOPE" list names a `CFileIoKge` class that does not exist
  in the binary -- `nm -C` only has an unrelated `CFileKge` (GE/bank
  sample-storage file format, no `CFileIoBase` relationship), almost certainly
  a mistaken guess in an earlier pass, corrected in `scsi_driver_base.h`'s own
  header comment; treat any other reference to "CFileIoKge" in this tree as
  stale. Verified with 12 new byte-exact KAT checks
  (`verify/test_scsi_driver_base.cpp`, covering the fixed-CDB/no-pbuf-read
  family, BE16/BE32/24-bit field packing, the READ(10)/WRITE(10)
  direction+opcode branch, and 2 confirmed real quirks --
  `SetParamReserveTrack`'s data pointer sourced from `pbuf+0xc` instead of the
  usual `+0x8`, and `SetParamSetSpeed`'s conditional per-word CDB-byte skip)
  plus the full existing host `make verify` suite (0 failures). Not tested on
  real hardware -- no reconstructed caller anywhere in this project yet (same
  status as the rest of the storage/disk-driver cluster). Eva manifest
  738 -> 778/37,795.
- **`CStorageConverterBase::CheckVersion`/`ValidateExt`/`Save`/`Load`/`Open`/
  `Close`/16x `ExttoIntXXXX`/16x `ValidateExtXXXX`/ctor** (2026-07-28, storage
  cluster follow-up to the batch above) — 40 methods, `.text+0x08de8f20`-
  `0x08e07cb0`, deliberately deferred while the class's real headline finding
  -- the 256-method `Ext{X}toInt{Y}` combinatorial matrix -- is fully
  reconstructed (`include/storage_converter_base.h`/
  `src/init/storage_converter_base.cpp`). Found via a fresh pending-manifest
  class-count sweep (same technique that found OA.ko's
  `CKGSeqBackupCommonParam`/`CKGSeqBackupModuleParam`, commit `efa0926`) --
  this was the single largest untouched cluster left in the storage survey.
  The matrix itself was decoded by a scripted `objdump -dr -M intel` -> Python
  instruction-pattern decoder (all 256 bodies are exactly one of 4 byte sizes,
  1/19/22/37, ZERO anomalies) cross-checked against a direct `.rodata` dump of
  `vtable for CStorageConverterBase` (`0x08fcc9c0`) to resolve every
  forwarding thunk's real target -- confirmed rule with zero exceptions:
  `(X=0,Y=0)` is the one real `memcpy` identity copy (Int0000 is
  byte-identical to Ext0000); `X>Y` is a genuine tail-call thunk to
  `Ext{Y}toInt{Y}` (120 instances, all individually resolved via the vtable
  dump, not assumed from the first couple of samples); `X<=Y` excluding
  `(0,0)` is a bare 1-byte `ret` no-op (135 instances, including all 15
  non-zero diagonals -- meaning every internal version other than 0000 is
  genuinely unimplemented in this build). The 40 deferred methods above are a
  SEPARATE, parallel real-implementation surface the matrix never calls into:
  `ExttoInt0000`..`ExttoInt000F` (16 methods, `.text+0x08deaba0`-`0x08dec4c0`,
  343-379 bytes each, NO `X` digits in the name -- distinct symbols from the
  matrix's own `Ext0000toInt0000`) look like the "real" per-version conversion
  bodies but are not wired to the matrix at all (independently confirmed --
  the matrix's own diagonal stubs for Y>=1 are bare `ret`, not calls into
  these). `Open()` (`.text+0x08deab30`, 103B) is the one method in this whole
  class confirmed to have real external callers (`.text+0x08df76b9`,
  `0x08df778d`, both in an unidentified `0x08df7xxx` region -- a lead for
  whichever future batch reaches `CFilesys`/`CDiskUtil`) and itself calls
  `ValidateExt()` (389B). Independently confirmed the matrix itself has ZERO
  external callers anywhere in the binary (whole-binary `call` grep against
  all 256 addresses plus `Save`/`Load`/`Close`/`CheckVersion`) -- same "real,
  faithfully reconstructed, but dead on the current build's own reachable
  paths" finding as `CScsiDriverBase`'s 35 `SetParamXxx` methods above. No
  ctor symbol found in this export (2 dtor overloads exist but no plain
  constructor) -- likely elided/inline, or only ever constructed through a
  derived class not yet identified; a real open question for a future pass,
  not assumed. Verified with 256 KAT checks
  (`verify/test_storage_converter_base.cpp`) that EXHAUSTIVELY exercise all
  256 `(X,Y)` combinations via an INDEPENDENT black-box rule (does the
  destination buffer end up copied or left at its sentinel value) rather than
  re-deriving the generator's own size/offset classification, plus the full
  existing host `make verify` suite (0 failures across 51 binaries). Not
  tested on real hardware -- no reconstructed caller anywhere in this project
  yet. Eva manifest 778 -> 1034/37,795.

- **`CStorageConverterBase::Open()`'s 2 real external callers, traced + a whole
  new ~32-class converter-family discovery** (2026-07-28, follow-up to the
  entry above) -- both callers (`.text+0x08df76b9`/`0x08df778d`) are inside
  `CProgConverter::Open()` (`prog_converter.h`), NOT `CFilesys`/`CDiskUtil` as
  the entry above guessed. Tracing that one caller surfaced a whole
  previously-unknown family: ~32 concrete `CStorageConverterBase`-derived
  per-file-format converter classes (`CProgConverter`/`CCombiConverter`/
  `CSongConverter`/`CDrumKitConverter`/`CGEConverter`/`CKontaktXxxConverter`/
  `CAKAIConverter`/`CSoundFontConverter`/... -- Prog/Combi/Song/DrumKit/
  SetList/GE-global/Kontakt-import/AKAI-import/SoundFont-import format
  version-migration), ~246 methods total (`storage_format_converters.h`'s own
  header comment has the full per-class count breakdown) -- this project's
  real file-format-version-migration toolbox, genuinely reachable (unlike the
  base class's own 256-method matrix, which stays confirmed dead).
  THIS BATCH reconstructed: `CStorageConverterBase`'s own remaining
  `ValidateExt0000..000F`/`Close()` (17 methods, closes out that class's
  deferred list except `CheckVersion`/`ValidateExt`/`Save`/`Load`/`Open`/
  `ExttoIntXXXX` below), 32 "safe" (no `this`-dependency) sibling
  `ValidateExtXXXX` overrides across 18 of the ~32 concrete classes (found via
  the same scripted `objdump -dr` -> Python classifier technique as the
  matrix above, splitting all 59 `ValidateExtXXXX` symbols in the whole binary
  cleanly into 48 safe vs 11 deferred with zero ambiguous cases), and
  `CProgConverter`'s dtor pair + `Close()`. A genuine dual-use finding on
  `CConvertStorageParam::m_size` (+0x0c): the base class's own `Ext0000toInt0000`
  reads it as a plain memcpy byte count (unchanged), but every
  `CPCMProgConverter`/`CMOSSProgConverter` `ValidateExtXXXX` instead
  dereferences it as a pointer to an unidentified ~0xa00+-byte session/context
  object -- both confirmed independently via direct disassembly, not
  reconciled (documented in `storage_converter_base.h`, not guessed at).
  DEFERRED, precisely documented, real leads for a future batch:
  - `CProgConverter::Open()` (766B) -- the actual entry point that picks
    PCM-vs-MOSS format and drives the whole cluster; large/intricate, not
    rushed.
  - `CProgConverter::Load()`/`Save()` (50B each) -- fully understood (forward
    to `m_pFormatConverter`'s own `Load`/`Save`, using the object's OWN
    internal `m_storedParam` copy, NOT the caller's argument -- a genuine,
    confirmed "ignores its own parameter" behavior) but their real target,
    `CStorageConverterBase::Load()`/`Save()`, is itself a version-dispatch
    jump table into the still-unreconstructed `ExttoInt0000..000F` real
    per-version conversion bodies (343-379B each) -- implementing the
    forwards without those would mean fabricating or stubbing them, which
    this project doesn't do.
  - The 11 `CPCMProgConverter`/`CMOSSProgConverter` `ValidateExtXXXX` that
    need the `m_size`-as-context-pointer interpretation above.
  - The other ~214 methods across the ~32-class family not touched this
    batch: every real `ExtXXXXtoIntYYYY` field-by-field format-migration body
    (the actual payload of this whole cluster), every class's own ctor/dtor
    beside `CProgConverter`'s. A genuinely large future-batch target, same
    "size is not depth, but do check for a tractable sub-piece first" lens
    already validated elsewhere in this log.
  New `include/storage_format_converters.h`/`src/init/storage_format_converters.cpp`,
  `include/prog_converter.h`/`src/init/prog_converter.cpp`. Extended
  `CConvertStorageParam` with 3 more real confirmed fields (`m_extFormatId`
  +0x10, `m_skipValidate` +0x19, `m_variantFlag` +0x1a). 75 new KAT checks
  (`verify/test_storage_format_converters.cpp`); full host `make verify`
  green (25/25 binaries, 0 failures) and a real Lenny target-ABI link OK.
  Along the way, `HAL_DisableInterrupts()`/`HAL_EnableInterrupts()` (real
  mangled C++ free functions, confirmed via `c++filt` on their ground-truth
  symbols) needed their first-ever real (no-op) linkable definitions in this
  project -- every earlier reference to them was comment-only. Also
  independently re-confirmed (via `git stash` isolation) that the
  intermittent `test_client_comm_server` 6-fail signature documented in
  `eva_client_comm_server_6fail_closed_not_a_bug_2026-07-26`/
  `eva_client_comm_server_heisenbug_root_cause_fixed_2026-07-26` (re-decompiler
  agent memory) is STILL genuinely present as an ASLR-dependent flake (not
  caused by anything in this batch, and not the same already-fixed
  struct-undersize root cause -- passes clean under both a plain rebuild with
  ASLR disabled and an `-fsanitize=address,undefined` build) -- see that
  memory file's own updated note. Eva manifest 1034 -> 1086/37,795 (2.873%).

- **`CRamSample::operator=(CUsrSample&)`** (`.text+0x08427ff0`, 2026-07-28) —
  deferred while reconstructing `CRamSample`/`CMultiSample`/`CRamSampleRelative`
  (`ram_sample.h`/`.cpp`, 68/69 methods). Real body needs 2 unmodeled internal
  sub-structs of `CUsrSample` (a flags pair at `*(this+0x4)+0x3c/0x3d` and a
  6-dword block at `*(this+0x8)`) copied verbatim into `CRamSample::mName`,
  which the reconstruction confirms is genuinely dual-use (plain name buffer
  vs. raw scratch storage) depending on caller. Not attempted to avoid
  fabricating an unverified `CUsrSample` layout; see `ram_sample.h`'s own
  header comment for the full disassembly-derived writeup. Not real-hardware
  relevant on its own (host-side reconstruction gap only).

- **`CKontaktXml`** (`kontakt_xml.h`/`.cpp`, 2026-07-28 sweep, 25/29 methods) —
  fresh `nm -C` class-inventory sweep found the ~180-class, previously-100%-
  untouched Kontakt (NKI) import subsystem; before touching any of the ~180
  classes, traced what the whole `CKontaktXxxParameter::AddIndexedParameter`
  family (e.g. `CKontaktGroupParameter`) actually calls into via
  `objdump -dr` and found every dispatch case funnels through
  `CKontaktXml::UnsignedValue`/`SignedValue` — the same "shared write-sink
  helper" pattern as OA's `sValueGetterTemp` — so `CKontaktXml` itself was
  reconstructed first, as the natural root for any future pass over the rest
  of the family (untouched, out of scope this pass). 4 real methods
  deliberately DEFERRED (not attempted, to avoid a low-confidence transcription
  masquerading as ground truth): `TruncateName` (9731 bytes, by far the
  largest method in the class — a genuinely complex name-shortening algorithm
  warranting its own pass), `UnpackPath` (a packed-path token-decoder whose
  token semantics aren't pinned down with confidence yet), `PathName` (its
  shared tail mixes an inlined SWAR `strlen()` with a `strncat()` count not
  confidently disambiguated, and it tail-calls the also-deferred
  `TruncateName` regardless), `RemoveTrailingCharacters` (a backward scan
  that resolves to exactly one `strcpy`-shift-left-by-one whose precise
  boundary rule wasn't disambiguated). See `kontakt_xml.h`'s own header
  comment for full per-method derivation and disassembly addresses. Also
  added `src/convert/libxml2_host_stubs.cpp` — this build host has no i386
  (-m32) libxml2 package (only amd64), and the Makefile's `verify` target
  links every `verify/test_*` binary against the full object set, so
  `CKontaktXml`'s real `xmlTextReader*` calls needed inert stand-in
  definitions somewhere always-linked rather than genuinely-unresolved
  symbols (which would have broken every OTHER class's verify binary, not
  just this one) — the real Kronos/Eva runtime links against real libxml2,
  this is host-build-only. `verify/test_kontakt_xml.cpp` covers every
  reconstructed pure/leaf method (32 checks); `ProcessNode`/`ProcessNodes`/
  `Parse`/`SkipNode`/`AddObject` (the libxml2-calling methods) are not
  independently host-tested for the same reason. Full host `make verify`
  green (65/65 binaries). Eva manifest 1747 -> 1772/37,795 (4.688%).
- **`CFileIoAkai`/`CFileIoDos`/`CFileIoIso9660`** (2026-07-28, storage-cluster
  follow-up to `CFileIoUnknown` -- the 3 concrete `CFileIoBase` media-I/O
  drivers file_io_base.h's own "OUT OF SCOPE" list flagged as good future-pass
  candidates) — 68 of 69 real `.text` entry points reconstructed (19/19 Akai,
  30/31 Dos, 19/19 Iso9660): ctor/dtor + every real virtual override + the
  shared `set_error()`/path-conversion helper(s) for each. Each wraps a
  distinct real embedded filesystem library (`aki_`/`akiutil_` for Akai,
  `pc_`/`po_` -- the well-known HCC/EBS RTFS embedded FAT API shape -- for
  Dos, `cd_` for Iso9660), all out of scope, modeled as inert stand-ins same
  convention as `CFileIoUnknown`'s own. `CFileIoDos::format(EDevice_Id, int,
  EFatType)` (`.text+0x0831afc0`, 0xbd8=3032 bytes, the single largest method
  in this whole batch) is DEFERRED -- see `DECOMPILE_ERRORS.md` for the full
  rationale (a genuine multi-call resumable FAT-format state machine driven by
  the static `CFileIoDos::iStage`, real `pc_mkfs`/`pc_fat_size` calls, real
  boot-sector/volume-label string building -- fully disassembled and
  understood at the control-flow level but too large for this pass, same
  "surveyed and explicitly passed over" treatment `CFileIoCdda`/`CFileIoUdf`
  already got the batch before). Two genuinely interesting independently
  verified findings surfaced along the way: (1) `CFileIoAkai::set_error()`
  and `CFileIoDos::set_error()` share the exact same raw-error-code global
  (`fs_user`, .bss+0x9608d90) and the same 44-entry-table SHAPE, but their
  tables differ at exactly one mapped slot (raw code 24) and Akai logs an
  Api-assert on unmapped codes while Dos silently ignores them -- confirmed
  via two independent `objdump -s` reads of the two distinct `.rodata` table
  addresses; (2) `CFileIoIso9660::fmount()` returns -1 on EVERY code path,
  including full success (the return register is set once at function entry
  and never reassigned anywhere in the function body) -- the real
  `scsi_mode_sel`/`cd_dskopen` side-effecting calls still happen, only the
  reported result is always failure. `dir()`'s inline packed-date-decode
  arithmetic (a real divide-by-100-via-multiply idiom, `0x147b >> 0x11`) is
  byte-identical across all 3 classes and also matches the shape already
  documented for other reconstructed classes project-wide. A few of the
  deepest sub-branches (`CFileIoDos::optimizemedium()`/`scandisk()`/
  `getmediainfo()`/`fmount()`'s own opaque `ddrive`/`fsinfo` field reads,
  `CFileIoIso9660::ConvertPathRtfsToCdfs()`'s final `strcat()` argument
  wiring) are transcribed at a faithful structural level with clearly-flagged
  medium-confidence simplifications rather than forced byte-exact matches --
  see each method's own header/source comment. `verify/test_file_io_akai.cpp`
  (19 checks), `verify/test_file_io_dos.cpp` (32 checks), and
  `verify/test_file_io_iso9660.cpp` (19 checks) all green; full host
  `make verify` green (70/70 binaries, the pre-existing
  `test_client_comm_server` ASLR flake did not reproduce this run). Not
  tested on real hardware -- no reconstructed caller anywhere in this project
  yet (same status as the rest of the storage/disk-driver cluster).
  `CFileIoCdda`/`CFileIoUdf` remain fully out of scope. Eva manifest
  1940 -> 2008/37,795 (5.313%).

- **`CFileIoCdda`/`CFileIoUdf`** (2026-07-28, fresh re-survey of the last 2
  concrete `CFileIoBase` siblings) -- both had been passed over twice before
  (once in `CFileIoBase`'s own original batch, once in the
  `CFileIoAkai`/`CFileIoDos`/`CFileIoIso9660` batch) with a standing "less
  tractable" verdict based on raw override count (41/35) and size
  (`CFileIoUdf::format()` alone 4791 bytes). A fresh, careful re-trace via
  `objdump -dr -M intel` against real ground truth found the prior verdict
  was "larger method count", not "uniformly deeper" -- both classes are
  almost entirely small, mechanical `cdda_*`/`udf_*` forwarding methods
  (SAME "call library fn; 0=fail->set_error(),-1; else 0" shape already
  established for `CFileIoAkai`/`CFileIoDos`) plus a handful of genuinely
  real medium-sized routines, with exactly ONE outlier method each that
  stays genuinely deep. Reconstructed 40/41 `CFileIoCdda` methods (`ctor`,
  dtor pair, `set_error()` with its OWN 23-entry raw-error table --
  distinct raw-code global `cdda_errno` (.bss+0x9600554), NOT the shared
  `fs_user`/`cd_errno` the other 3 siblings use -- plus real `getmediainfo()`
  /`fopen()`/`fmount()`/`finalize()` bodies) and 34/35 `CFileIoUdf` methods
  (ditto, own `udf_errno` raw-code global + 82-entry table, plus real
  `dir()`/`getmediainfo()`/`fmount()`/`writesetup()`/`fopen()`/
  `SetRecoveryParam()` bodies, and the 2 self-contained non-iStage-touching
  helper leaves `formatsub()`/`setfmtparam()` that the deferred `format()`
  itself calls). Deferred `CFileIoCdda::getcurpos()` (1866 bytes, a genuine
  CD-DA track/index binary-search + refinement loop) and
  `CFileIoUdf::format()` (4791 bytes, a genuine resumable UDF-format state
  machine driven by `CFileIoUdf::iStage` -- confirmed via a full-binary grep
  that no other method anywhere touches that global) -- see
  `DECOMPILE_ERRORS.md` for both. Neither deferred method is declared as a
  virtual override in its class's header (same convention `CFileIoDos`'s own
  deferred `format()` established), so both reconstructed vtables fall back
  to the correct inherited `CFileIoBase` stub rather than leaving a
  declared-but-undefined symbol.

  Genuine findings along the way: (1) `CFileIoCdda::chdir()`/`dir()` use
  DIFFERENT sentinels from `CFileIoBase`'s own (`chdir()==0`, `dir()==0` --
  success/no-entry, not -1+assert -- a CD-DA disc has no directory
  hierarchy, so both trivially no-op-succeed); (2) `CFileIoUdf::fopen()`
  with `mode[0]=='v'` returns -1 UNCONDITIONALLY with no other work at all,
  not even `set_error()` (confirmed: that jump target is a bare `mov
  eax,-1; ret` epilogue) -- a real, independently verified difference from
  `CFileIoCdda::fopen()`'s own `'v'` slot, which does real work; (3)
  `CFileIoUdf`'s mode-char `fopen()` scheme is richer than every other
  sibling's own -- 2 of its 23 jump-table slots (`'c'`/`'p'`) route to a
  real embedded `repz cmpsb` full-string compare against the literals "cp"/
  "pcp" (medium-confidence guess at meaning only, exact bit-level logic
  transcribed faithfully); (4) `CFileIoCdda::finalize()`'s success/failure
  sense on its own leading `cdda_writesetup()` call is the OPPOSITE of what
  `settestmode()`'s identical-looking call implies -- SUCCESS continues into
  more real work, FAILURE is the early-out -- caught and fixed during this
  session's own first-draft transcription (verified via direct disassembly
  re-check, not assumed). New `verify/test_file_io_cdda.cpp` (34 checks) and
  `verify/test_file_io_udf.cpp` (28 checks); full host `make verify` green
  (72/72 binaries). Two real host-side bugs caught and fixed before this
  batch's own verify suite went green: an out-of-bounds hardcoded-
  ground-truth-address dereference (`*(unsigned char*)0x93b0d5e` -- ground
  truth `.bss` addresses don't exist in this host process; fixed to real
  local stand-in arrays, same convention as `CFileIoAkai`'s own
  `devstat_tab`) and a stack-buffer overflow in a shared `CDDriverIO::
  scsi_mode_sense10` stand-in whose fixed `memset()` size exceeded one
  caller's smaller stack buffer (`CFileIoUdf::SetRecoveryParam()`'s 20-byte
  `buf` vs. the stub's original 48-byte memset) -- both caught by the host
  verify suite itself (a segfault, not a silent wrong-answer), not
  discovered by inspection. Not tested on real hardware -- no reconstructed
  caller anywhere in this project yet (same status as the rest of the
  storage/disk-driver cluster, now fully closed out: `CFileIoBase`,
  `CFileIoUnknown`, `CFileIoAkai`, `CFileIoDos`, `CFileIoIso9660`,
  `CFileIoCdda`, `CFileIoUdf` all done; `CDDriverIO`/`CScsiDriverBase`
  (partially)/`CFilesys`/`CDiskUtil` remain the cluster's own open leads).
  Eva manifest 2008 -> 2084/37,795 (5.514%).

- **`CNoteTracer`** (`note_tracer.h`/`.cpp`, 2026-07-28, "Tracer" family
  follow-up to `CParamTracer`/`CControllerTracer`/`CCtrlAndParamTracer`) —
  24 real `.text` entries reconstructed (ctor x3, dtor, operator=,
  Insert/Remove, ResetPendingNotes/ClearEntries/RefreshEntries,
  GetLeftMost/GetRightMost, the 4 static `TDynBuffer<CBufferedNote>` helpers
  + the non-static `SwapBuffer`, the virtual `RendundantInsertion` hook, all
  4 event-list emitters (`ListNotesOn` x2/`ListNotesOff`/`ListSoundsOn`/
  `ListSoundsOff`), and the friend free function `Swap`). Introduced a new
  minimal `TDynBuffer<T>` template (distinct from the already-reconstructed
  `TVector<T,N>`, tvector.h) after confirming via disassembly that
  `CreateBuffer`/`ReallocBuffer`/`DestroyBuffer` are genuinely STATIC (no
  `this` load at all) while `SwapBuffer` alone is a real non-static member.
  Went back to `CControllerTracer`'s own prior-session deferral note and
  resolved its open question: `CNoteTransposerOwner` (the blocking interface
  for the still-deferred `CNoteTracerTransposer`, below) IS a genuinely tiny
  abstract interface -- real vtable has exactly ONE pure-virtual slot beyond
  D1/D0, and its exact signature was recovered without guessing by finding
  its one real implementer: `CTrackBase`'s own `__vmi_class_type_info`
  (.rodata+0x8e827fc) lists it as a real base, and `CTrackBase`'s own vtable
  has `OnRejectNotesForTransposeConflict(CNoteTracerTransposer&,
  CLinkedEvent*, CLinkedEvent*, unsigned char)` sitting in exactly that
  inherited slot. `CNoteTracerTransposer` itself STAYS deferred anyway: its
  own `RendundantInsertion` override (.text+0x08093140, ~1400 bytes) is a
  genuinely dense duplicate-note-conflict resolver in its own right
  (channel-mismatch detection, an 8-way unrolled note-index-invalidation
  loop, multiple synthetic-event builds, a real virtual dispatch back out to
  the now-resolved `CNoteTransposerOwner` interface) -- comparable in size/
  depth to an entire prior batch on its own, so the class as a whole stays
  out of scope even though its blocking dependency is gone; a real,
  well-scoped future pickup once that one method gets its own pass. Also
  newly out of scope this pass: `operator<<(CMStream&, const CNoteTracer&)`/
  `operator>>(CMStream&, CNoteTracer&)` (.text+0x08093c80/0x08093cf0) --
  simple, fully-understood bodies (write/read `mSize` then the raw
  `mNotes[]` buffer, rebuilding `mNoteIndex[]` on read via the same
  `RefreshEntries()` idiom used throughout), but they're the FIRST real call
  site this project has found into `CMStream` (`Write(const void*,
  unsigned)`/`Read(void*, unsigned)`), a class with no existing
  reconstruction anywhere in this tree -- deferred rather than hand-adding
  an under-specified new external dependency's shape on faith; note_tracer.h
  documents the exact 2 real bodies for whenever `CMStream` itself gets
  reconstructed. `CSysExTracer`/`CTracer` remain confirmed UNRELATED despite
  the similar name (already established in the prior `CControllerTracer`
  pass's own agent-memory note). Full host `make verify` green (2676 checks
  across all binaries, 0 failures); new `verify/test_note_tracer.cpp` (37
  checks). Eva manifest 2143 -> 2167/37,795 (5.734%).

- **`CBackupChunk` (34 methods) / `CImageStr` (11 methods)** — deferred
  2026-07-29 while reconstructing `CChunkRootBase`/`CChunkRootWithSeek`/
  `CChunkRootWithSeekWithCRC` (`chunk_root_family.h`/`.cpp`), the "index/
  seek/CRC on top of chunked I/O" layer `chunk_family.h`'s own header
  comment flagged as the natural next batch. A real vtable byte-dump
  (`.rodata+0x8e84d60`/`0x8e84e00`/`0x8e85000`/`0x8e850a0`) CORRECTED that
  header's own prior speculative guess ("CBackupChunk is CChunk-derived,
  vtable-diffed byte-for-byte against CChunk's own") -- the real hierarchy
  is a genuine linear chain, `CChunkBase -> CChunkRootBase ->
  CChunkRootWithSeek -> CChunkRootWithSeekWithCRC -> CBackupChunk`, confirmed
  because `CBackupChunk`'s own vtable slot 25 (`GetNumByteAfterIndex`) is the
  LITERAL SAME function pointer as `CChunkRootWithSeekWithCRC`'s own
  (inherited unchanged, only possible with real inheritance). `CBackupChunk`
  itself stays out of scope: every one of its 8 ctor overloads plus
  `GetNextPackSize`/`ReadNextPack`/`SkipNextPack`/`WriteNextPack`/
  `WriteTailPack` calls a real, out-of-scope proprietary compression codec
  (`COComp`, dispatching into `CBarc`'s own ~3KB real LZ-style
  `m_ifnBCompress`/`m_ifnBDeCompress` routines, confirmed via a direct
  call-xref trace of the actual disassembly, not a size guess) -- a
  genuinely separate DSP-like subsystem. `CImageStr` (a memory-backed
  `CStream`, `stream_family.h`'s own sibling family) is the one remaining
  dependency of `CChunkRootWithSeek::BuildSubChunkIndex()`'s deep read-mode
  body (parsing a previously-written index sub-chunk back off disk); its own
  `GetLength()`/`Tell()`/`Open()`/`Seek()` overrides implement a genuinely
  distinct mode-dependent windowing scheme this session did not have budget
  to independently trace. `BuildSubChunkIndex()` itself IS reconstructed for
  its two real, faithful fast-path guards (`mStatus!=eRead -> false`;
  already-built -> `true`); only the deep eRead-mode body is stubbed
  (conservative `false`, correct/safe for every real caller in this batch --
  none can silently corrupt data on that return). `CCrc32`/`GetCRC()`/
  `PostClose()`'s own CRC-patch-back ARE fully reconstructed and exercised by
  a real Close()-round-trip KAT (`verify/test_chunk_root_family.cpp`).
  Real callers of this whole family (a "SaveFile"-shaped high-level entry
  point, presumably somewhere in the still out-of-scope
  `CLoadSoundFontMgr`/`CPCMManager`/`CDiskUtil` cluster or a sibling) not
  independently traced this session either. Eva manifest 2652 -> 2700/37,795
  (7.144%), commit 1cb22b2.

- **CFileKscList, 18/26 methods (`file_ksc_list.h`/`.cpp`), 2026-07-29 (solo,
  no subagents -- session-wide 200-subagent dispatch cap hit)**. Sibling of
  the much larger `CKscSampleManager` (68-method singleton, own separate
  survey, not attempted this pass) -- confirmed via a real `Load()` call
  into `CKscSampleManager::GetInstance()`/`AddAutoLoadKsc()`. This class's
  own per-field accessors are self-contained: every one marshals args and
  calls through the project-wide `FMApi` god-object's vtable, slot `+0x1bc`
  ("read a positional field") / `+0x1c0` ("write a positional field"), both
  `int(*)(void*, void* handle, void* buf, unsigned int* len)`. `mHandle`
  (offset 0, the only field this pass models) is passed as `handle` on
  EVERY call -- not a per-field key, a shared identifier for one already-open
  KSC-list record. Combined with `ReadDot`/`WriteDot` consuming/emitting a
  literal `"\r\n"` and `ReadHeaderId` comparing against a literal `"#KSC"`
  4-byte magic (both confirmed via a direct `.rodata` byte dump at
  `0x8ef2e20`: `0d 0a 00 23 4b 53 43 00` = `"\r\n\0#KSC\0"`), this is a
  CRLF-delimited flat text record read/written sequentially through an
  already-open handle, not a keyed random-access profile API -- `mHandle`'s
  own real type/how it's opened is NOT modeled (that's `Load()`/`Save()`'s
  job, deferred below), every accessor here just forwards it through
  faithfully, matching this project's established "god-object opaque
  forwarding" convention (`config_manager.cpp`'s own `FMApiGetDriverFactory`/
  `FMApiRegisterDriver` wrappers, reused/extended with the same style for
  these 2 new slots).

  Deferred, documented, not fabricated: `ReadFilePath`/`SaveFilePath` (a
  length-prefixed-string protocol with a real odd/even padding-byte branch
  via `CMemoryAccessor::ReadLittle16Bit` -- understood from disassembly but
  not landed this pass, budget reasons), `RefreshFilePath`/`GetDeviceInfo`
  (both call further into `CDeviceMgr`, unmodeled), and `Load()`/`Save()`
  themselves (call into `CKscSampleManager` AND the project-wide
  out-of-scope growable `CZ` container -- the exact same trap
  `korg_file.h`'s own header comment already documents for the rejected
  `CFileKge`). Real host KAT (`verify/test_file_ksc_list.cpp`, 9 sections)
  uses a fake FMApi vtable backing a tiny in-memory flat-record buffer
  (same established fake-vtable-object convention as
  `test_config_manager_create_modules.cpp`), including a full sequential
  multi-field record round-trip (`#KSC`, `\r\n`, VendorId, `\r\n`,
  AutoLoad, `\r\n`) to exercise the CRLF-framing hypothesis end-to-end, not
  just field-by-field. Eva manifest 2700 -> 2718/37,795 (7.191%), commit
  b6117ba.

- **CFileKscList round-46 follow-up: `ReadFilePath`/`SaveFilePath`,
  2026-07-29 (solo, no subagents)**. The length-prefixed-string protocol
  flagged deferred just above, now landed: 20/26 methods real.
  `SaveFilePath` writes a 2-byte little-endian length prefix
  (`CMemoryAccessor::WriteLittle16Bit`, `strlen()` truncated to 16 bits),
  then the raw path bytes, then -- ONLY when the length is ODD -- a third
  write of one `0x00` pad byte (confirmed via the real `and esi,1; je
  <skip>` branch; EVEN lengths write no pad at all). `ReadFilePath` mirrors
  this exactly on the read side, decoding the prefix via
  `CMemoryAccessor::ReadLittle16Bit`, setting `*lenOut = decodedLength+2`
  (or `+3` if odd), with its own return value being the success of
  whichever real FMApi call happened LAST (the pad read when odd, the
  string read when even) -- ground truth's own real register reuse,
  reproduced faithfully rather than "cleaned up" into an AND of both
  results. `CMemoryAccessor::ReadLittle16Bit`/`WriteLittle16Bit` (2 new
  tiny static methods, `storage_converter_ext_stubs.h`) independently
  confirmed real, zero-relocation, zero-call leaf functions via direct
  `objdump -dr` at ground truth `.text+0x838dd40`/`0x838dd60` (17/18
  bytes). Real host KAT (`verify/test_file_ksc_list.cpp`, sections 10-16)
  covers both odd- and even-length round-trips plus 3 distinct failure
  paths (prefix read fails, string data truncated, pad byte truncated --
  the last of which exercises the last-call-wins return-value quirk
  directly: string read succeeds, pad read fails, overall result is still
  false). Still deferred, unchanged: `RefreshFilePath`/`GetDeviceInfo`/
  `Load()`/`Save()`. Eva manifest 2718 -> 2730/37,795 (7.223%).

- **CDirCD, 10/~40 methods (`dir_cd.h`/`.cpp`), 2026-07-29 (solo, no
  subagents)**. Real Akai/ISO CD-ROM directory driver
  (`.rodata+0x08e86160` vtable, `0x08e862f8` typeinfo -- a real
  `__si_class_type_info` whose base-type field, confirmed via a direct
  `.rodata` byte read, points at `typeinfo for CDirectory`). `CDirectory`
  ITSELF is a real, substantial, entirely separate class that embeds 3
  MORE entirely unmodeled classes (`CRecentDirElems` 23 methods,
  `CRecentFileElems` 12, `CRecentPathElems` 9, all destroyed by
  `CDirectory::~CDirectory()`, confirmed via its own disassembly) -- found
  mid-investigation, after the round-38 survey had already flagged
  `CDirCD` as "not obviously entangled"; the entanglement is one level
  deeper (in the BASE class) than that survey checked. Neither
  `CDirectory` nor the 3 `CRecentXxxElems` siblings are reconstructed
  this pass.

  Scoped down to the 10 smallest, self-contained `CDirCD`-level methods
  (accessed via raw `this`-offset arithmetic, no real ctor/dtor, no
  `CDirectory` base-class modeling): `GetCurrEntry`/`GetRootHandle`/
  `GetClusterSizeInSect`/`GetMaxDirEntrySize`/`GetNumAkaiPartition`/
  `SetError`/`ResetBufferedEntries`/`GetTotalSectors`/`FindPartition`/
  `GetPTRecord`. `GetTotalSectors()`'s own real body sums a real 16-bit
  per-entry value (assembled from 2 non-adjacent bytes within an 8-byte
  session-table record) times 75 -- the real CD-DA sectors-per-second
  timing constant, confirmed via the literal `imul ...,0x4b` (75)
  immediate. `GetPTRecord()`'s own real out-of-range path calls a
  project-internal diagnostic/assert API via a global object's vtable
  slot +0x94 -- modeled as an inert no-op extern (ground truth's own
  control flow falls through and computes+returns the pointer regardless
  of whether it fires). A real, caught-before-landing bug during this
  pass: `GetRootHandle()`'s 3-way mode dispatch (modes 1/2/3) was
  initially transcribed with cases 2 and 3 swapped (a raw
  `switch`-vs-jump-table transcription slip, not a ground-truth
  ambiguity) -- caught via careful cross-reading of the real disassembly
  a second time before landing, not by the KAT (which would have passed
  either way without ground-truth-derived expected values to catch it --
  a reminder that swapped-but-internally-consistent branches are exactly
  the class of bug a KAT built from the same transcription can't catch).

  Deferred (real, disassembly not (fully) traced this pass): ctor/dtor,
  `IsMixedCD()` (a genuine 350-byte two-phase alternating session-table
  scan), `FindVolume()` (a real BINARY SEARCH over the volume table
  followed by a linked-list traversal), `ReadNextEntry()`/
  `AppendAkaiPartition()`/`GetAkaiPartition()`/`ChangeAkaiPartition()`/
  `ClearAkaiPartition()`/`Register()`/`Unregister()`/`Invalidate()`/
  `MediaOpen()`/`GetOwnerPartition()`/`GetRootSize()`/
  `FindLastDataSessionOffset()`/`GetMediaLabel()`/
  `UpdateElemInPathTable()`/`GetMaxMSNum()`/`GetNumOfMultisample()`, plus
  `CCDConfigDir::DeserializePTR()` (a separate class touching
  `CDirCD::PTRecord`).

  Real host KAT (`verify/test_dir_cd.cpp`, 10 sections, 26 checks) uses a
  raw manually-populated byte buffer cast to `CDirCD*` (no real ctor/dtor
  call), same ctor-avoidance convention as `CFileKscList`'s own KAT.
  `make verify` full suite green (91 binaries, 0 failures). `nm|c++filt`
  confirms all 10 new symbols' mangled names match ground truth exactly
  EXCEPT `SetError` (real param type `EDrvNotify`, an entirely unmodeled
  enum, represented as plain `int` per this project's established
  convention for unconfirmed enum types -- same accepted mangled-name
  divergence as other instances of this pattern elsewhere in this
  project). Eva manifest 2718 -> 2728/37,795 (7.218%), commit pending.

  Real-HW test that would help: none identified -- this driver only
  matters when a real Akai-format data CD is mounted, and only 10 of its
  ~40 methods (none of the actual directory-traversal ones) are even
  reconstructed yet.

- **CSysEx*Name/CSysExSetListSlotComment family, 32/32 tractable methods
  (`sysex_object_names.h`/`.cpp`), 2026-07-29 (solo, no subagents --
  session-wide 200-subagent dispatch cap hit)**. An 8-class family of
  trivial SysEx-transferable named-object accessors --
  `CSysExSetListSlotComment`/`CSysExSetListSlotName`/`CSysExCombiName`/
  `CSysExProgName`/`CSysExSongName`/`CSysExWaveSeqName`/
  `CSysExDrumKitName`/`CSysExSetListName` -- found via a fresh `nm -C`
  class-inventory sweep. Each class has the identical 4-method shape,
  confirmed via ground-truth decompile: `GetStorageId()` (a sequential
  per-class literal, `0x1c` through `0x23`), `GetVersion()` (always
  literal `0`), `GetObjectSize()`/`GetObjectSizeForExport()` (identical
  literal object size -- `0x200` for the Comment class only, `0x18` for
  all 7 name-record classes).

  Notable finding while ground-truthing the destructors: every one of
  the 8 real dtors resets its vtable pointer to the exact SAME shared
  symbol, `PTR__CSysExObjectBase_08f7a908` -- confirmed via
  `/home/share/Decomp/EVA_Decomp/eva_export/symbols.csv` (no per-class
  `PTR__CSysEx*Name` vtable object exists anywhere). This means the 4
  accessor methods above are NOT virtual overrides (no per-class vtable
  slots exist to hold distinct implementations) -- a plain,
  non-polymorphic class shape for each of the 8 classes is faithful to
  ground truth, not a simplification. The real base class
  `CSysExObjectBase` itself (`HasDigests()`/`GetObjectSize(void const*)`)
  is deliberately NOT modeled -- nothing in this project's current call
  graph calls through it. Each dtor pair (11-byte "reset vtable ptr" +
  39-byte "reset vtable ptr then `free(this)` inside a real
  `HAL_DisableInterrupts()`/`HAL_EnableInterrupts()` bracket") gets the
  SAME "D0's `free(this)` not reproduced" treatment already established
  project-wide (`long_binary_file.cpp` et al.) -- a plain empty
  `~ClassName() {}`; the 32 accessor methods are the only ones credited
  in the manifest, dtors intentionally excluded (matching
  `CLongBinaryFile`'s own precedent).

  Real host KAT (`verify/test_sysex_object_names.cpp`, 32 checks, one
  per method per class) confirms every literal constant against the
  ground-truth decompile independently (not re-derived from this same
  reconstruction). `make verify` full suite green. Eva manifest 2730 ->
  2762/37,795 (7.308%).

  Real-HW test that would help: none identified -- these are pure
  constant accessors with no observable I/O or state, called only as
  part of a larger not-yet-reconstructed SysEx object-transfer framework.

- **CSysExSong/CSysExDrumKit/CSysExCombi/CSysExWaveSeq/CSysExSetList/
  CSysExSongTimbreSet family, 30/36 tractable methods
  (`sysex_objects.h`/`.cpp`), 2026-07-29 (solo, no subagents)**. The
  main SysEx-transferable "full object" classes, sibling of the smaller
  `CSysEx*Name`/`CSysExSetListSlotComment` family above (found the same
  round). 5 of the 6 classes share an identical 6-method shape:
  `GetStorageId()`/`HasDigests()` (always `1`)/`GetVersion()`/
  `GetObjectSize()`/`GetObjectSizeForExport()` (identical per-class
  literal)/`GetNumObjectsForDigest(int)`. `CSysExSongTimbreSet` is a
  smaller 4-method sibling (no `HasDigests`/`GetNumObjectsForDigest`).

  `GetNumObjectsForDigest(int)` deliberately NOT reconstructed for
  `CSysExDrumKit`/`CSysExCombi`/`CSysExWaveSeq`/`CSysExSetList` -- their
  real bodies are a genuinely unresolvable indirect call
  (`(**(code**)(*(int*)param_1+0x38))()`, Ghidra's own decompile flags
  "Could not recover jumptable, too many branches") through an
  unconfirmed vtable slot on a caller-supplied `param_1` object (real
  calling convention is `__cdecl`, no implicit `this` -- `param_1` is a
  plain explicit argument, not this class's own instance). Declared as
  genuinely unresolved externs (`CSysExDrumKit_GetNumObjectsForDigest_
  Unresolved()` etc) rather than guessed at. `CSysExSong`'s OWN version
  IS a trivial literal (`return 200`) and IS reconstructed/credited.

  Independently re-confirmed the same shared-vtable finding as the
  `CSysEx*Name` family: no `PTR__CSysExSong`/`PTR__CSysExDrumKit`/etc
  vtable object exists in ground truth, only the shared
  `PTR__CSysExObjectBase` -- consistent non-polymorphic-in-this-model
  treatment across both families.

  Real host KAT (`verify/test_sysex_objects.cpp`, 31 checks) confirms
  every literal constant against the ground-truth decompile
  independently. `make verify` full suite green. Eva manifest 2762 ->
  2792/37,795 (7.387%).

  Real-HW test that would help: none identified -- same rationale as
  the `CSysEx*Name` family above.

- **CSysExKarmaGEInfo/CSysExSongControl, 14/14 tractable methods
  (`sysex_control_objects.h`/`.cpp`), round 41, 2026-07-29 (solo, no
  subagents)**. Flagged as a viable follow-up during round 40's survey
  of the sibling `CSysEx.../CSysExObjectBase` families, picked up here.
  `CSysExKarmaGEInfo` (`GetStorageId()`=0x26, `GetNumBanks()`=1,
  `GetVersion()`=0, `GetObjectSize()`/`GetObjectSizeForExport()`=0x7a0)
  has 4 extra small real methods on top of the common trio:
  `GetObjectPointer(int,int)` returns `CKGUtil::sm_poKGUIInfo+0x3c90`;
  `GetSysExBankId(int)` is an identity passthrough; `GetNumOfObject`/
  `GetTotalSizeForExport` always return 0. `CSysExSongControl`
  (`GetStorageId()`=0xc, `GetVersion()`=0, `GetObjectSize()`/
  `GetObjectSizeForExport()`=0x1490) has 1 extra method,
  `GetObjectPointer(int,int)`, real return type `void` (ground
  truth genuinely discards `CSeqDataManager::GetRegistoredSong()`'s
  result -- no "could not recover" warning here, unlike the 4 deferred
  `GetNumObjectsForDigest` cases above, so this is a faithfully
  reproduced real waste, not a decompiler artifact). No deferrals this
  batch -- every method decompiled cleanly.

  Architectural finding (most significant part of this batch):
  `CSysExSongControl::GetObjectPointer` genuinely calls a real,
  confirmed, but entirely unmodeled class (`CSeqDataManager`, via the
  same "raw static pointer IS the singleton" idiom used throughout the
  project, e.g. `CKGBankManager::ms_poInstance`). First attempt
  declared `CSeqDataManager::GetRegistoredSong` as a genuinely
  unresolved `extern "C"` free function, matching round 40's
  `GetNumObjectsForDigest` precedent -- but `make verify` failed with
  `undefined reference to
  'CSeqDataManager_GetRegistoredSong_Unresolved'` inside
  `test_alpha_keyb_ctrl`, an entirely unrelated test binary. Root
  cause: unlike OA.ko's per-target manual linking, Eva's Makefile
  links EVERY verify target against the full reconstructed object tree
  (`$(OBJ)`, auto-globbed), so any reconstructed function that actually
  CALLS an undefined extern breaks every test binary in the suite, not
  just its own -- round 40's unresolved externs never triggered this
  because they were declared-but-never-actually-called. Fixed by
  replacing the free-function extern with a minimal no-op class
  stand-in (`struct CSeqDataManager { int GetRegistoredSong(int) {
  return 0; } };`), matching `storage_converter_ext_stubs.h`'s
  established "declare the minimum viable slice, no-op body, clearly
  flagged" convention -- fully link-safe since the body is inline.
  Documented pattern for Eva going forward: a genuinely-unresolved
  free-function extern is safe only when provably uncalled by
  reconstructed code; anything actually invoked from reconstructed
  code needs a class stand-in instead.

  Also hit and fixed an unrelated but instructive bug while drafting
  the header comment: a literal `*/` embedded in prose ("CSysEx*/
  CSysExObjectBase") prematurely closed the enclosing `/* ... */` block
  comment, producing a cascade of downstream parse errors far from the
  real cause -- reworded to avoid embedded `*/` substrings.

  Same shared-vtable / non-virtual-dtor finding as the `CSysEx*Name`
  and `CSysExSong`/etc families re-confirmed for both classes here too
  (no per-class vtable object exists).

  Real host KAT (`verify/test_sysex_control_objects.cpp`, 15 checks).
  `make verify` full suite green. Eva manifest 2792 -> 2806/37,795
  (7.424%).

  Real-HW test that would help: none identified -- same rationale as
  the sibling families above.

  **Follow-up round 43 (2026-07-29)**: while surveying for the next
  cluster, found the family's own utility class `CMemoryAccessor`
  (`storage_converter_ext_stubs.h`) 3/12 already-real methods
  (`WriteBig32Bit`/`ReadLittle16Bit`/`WriteLittle16Bit`, committed
  since round 46) had NEVER been added to `gen_manifest.py`'s
  `RECONSTRUCTED` set -- a genuine manifest-crediting gap, same class
  of bug as the earlier "mangled-name grep" finding (see the
  deferred-registry rounds). Fixed by crediting all 3 alongside landing
  the remaining 9 siblings for real: `ReadBig32Bit`/`ReadBig24Bit`/
  `WriteBig24Bit`/`ReadBig16Bit`/`WriteBig16Bit`/`ReadLittle32Bit`/
  `WriteLittle32Bit`/`ReadLittle24Bit`/`WriteLittle24Bit` -- completing
  the full 12-method Read/Write{Big,Little}{16,24,32}Bit family. All 12
  are plain byte-shuffle encode/decode with zero relocations/calls,
  each independently confirmed against its own ground-truth decompile
  (`.text+0x838dbc0`..`0x838dd80`). Real host KAT
  (`verify/test_memory_accessor.cpp`, 18 checks). `make verify` full
  suite green. Eva manifest 2852 -> 2864/37,795 (7.578%).

  Real-HW test that would help: none identified -- pure byte-shuffle
  utility with no observable I/O or hardware surface.

- **CSysExGlobal/CSysExKarmaGE/CSysExGETemplate/CSysExRegion, 46/57
  tractable methods (`sysex_objects_ge_region.h`/`.cpp`), round 42,
  2026-07-29 (solo, no subagents)**. Fresh manifest survey of the
  `CSysEx.../CSysExObjectBase` family's remaining siblings. Per-class
  real literal constants: `CSysExGlobal`
  (StorageId=5/NumBanks=1/HasDigests=1/Version=2/ObjectSize=
  ObjectSizeForExport=0x6084), `CSysExKarmaGE` (6/0xc/1/0/0x9ec/0x9f0),
  `CSysExGETemplate` (7/4/1/0/0x10580/0x10584), `CSysExRegion`
  (0xb/1/1/1/0x130/0x130).

  All 4 classes' `GetObjectPointer(int,int)` reconstructed:
  `CSysExGlobal`'s discards both `CStorage::GetInstance()`/`GetGlobal()`
  call results (real return type genuinely `void`, same discard shape
  as round 41's `CSysExSongControl::GetObjectPointer`) -- `CStorage`
  added as a new minimal no-op stand-in,
  `storage_converter_ext_stubs.h`. `CSysExKarmaGE`/`CSysExGETemplate`
  each call one new small real-but-unmodeled `CKGUtil` free function
  (`GetUserGE`/`GetUserKarmaTemplate`, both added as no-op-returning-0
  stand-ins on the existing `CKGUtil`, `sysex_control_objects.h`) then
  add `index*stride`. `CSysExRegion`'s is FULLY concrete (no stand-in
  needed): `(idx<10000 ? idx : 0)*0x130 + CKGUtil::sm_poRegionHolder`, a
  new real confirmed static member (base of a 10000-entry region array)
  also added to `CKGUtil` this round.

  `CSysExRegion::GetTotalSizeForExport` is ALSO fully concrete and
  reconstructed -- a genuine 8x-unrolled loop counting `0x130` for every
  region in `[0,10000)` whose "active" flag byte
  (`sm_poRegionHolder+0x18+i*0x130`) is nonzero. Real, confirmed quirk
  preserved verbatim: the function's own 2 explicit arguments
  (`param1`/`param2`, presumably meant to bound the counted range) are
  completely UNUSED in ground truth's real disassembly -- it always
  sums across the FULL 10000-entry range regardless of what's passed,
  not "fixed" to actually respect the requested range.

  Deliberately NOT reconstructed, for 3 distinct reasons: (1)
  `CSysExGlobal::GetTotalSizeForExport` -- genuinely unresolvable vtable
  jumptable ("Could not recover", same as round 40's family); (2)
  `CSysExKarmaGE`/`CSysExGETemplate::GetTotalSizeForExport` -- fully
  concrete, NO decompiler warning, but a real 2-call-per-item virtual
  dispatch through THIS class's own vtable at raw offsets 0x38/0x1c
  summing `GetXAtIndex(i)*GetXSize()` -- resolving which named methods
  occupy those slots needs the base class's full vtable interface
  reconstructed first, same deferral class as OA.ko's `CFileStream::
  SetPositionBeginning` (round 49); (3)
  `GetNumObjectsForDigest(int)` for `CSysExKarmaGE`/`CSysExGETemplate`/
  `CSysExRegion` -- same genuinely-unresolvable vtable-slot-0x38
  indirect call as reason (1). `CSysExGlobal`'s own
  `GetNumObjectsForDigest` IS a trivial literal (`return 1`) and IS
  reconstructed.

  Same shared-vtable / non-virtual-dtor finding as every prior sibling
  family re-confirmed for all 4 classes here too.

  Real host KAT (`verify/test_sysex_objects_ge_region.cpp`, 39 checks,
  including a real ~3MB backing buffer for `GetTotalSizeForExport`'s
  own byte-scan). `make verify` full suite green. Eva manifest 2806 ->
  2852/37,795 (7.546%).

  Real-HW test that would help: none identified -- same rationale as
  the sibling families above.

- **CESDiskCommandTask, 84/95 tractable methods
  (`es_disk_command_task.h`/`stg_disk_command_task.cpp`), round 44,
  2026-07-29 (solo, no subagents)**. Fresh manifest survey filtered to
  pending methods with no `in_stack_ffffffXX`/`unaff_`/"Could not
  recover jumptable" warnings, grouped by class, sorted by average
  method size -- surfaced `CESDiskCommandTask` (95 pending methods,
  avg 89 bytes) as an unclaimed cluster.

  84 of its 95 methods share one exact 44-byte shape (confirmed by
  reading every one of the 84 ground-truth decompiles individually,
  not pattern-guessed): write a literal opcode constant to `this+0xa8`,
  call the already-real `CTask::SetMask(0)` (task.h/task.cpp, round-49
  batch), return 1. Modeled as real single inheritance from `CTask`
  (matching every other `CTask`-derived class in this project, e.g.
  `edit_task.h`) with an opaque 0x2c-byte gap at `+0x7c..+0xa8`
  standing in for an unmodeled `CEditable` base plus
  `CESDiskCommandTaskBase`'s own fields, then `mCommandOpcode` at
  `+0xa8`.

  Deliberately NOT attempted this round: the other 11 methods (2 ctor
  entries, 3 dtor variants, `ExecuteLoadMultiFile`/
  `ExecuteMakeAudioCommand`/`ExecuteUtilityCommand`/
  `ExecuteSaveCommand`/`ExecuteLoadCommand`/`Exec`). The real ctor
  chains through `CESDiskCommandTaskBase::CESDiskCommandTaskBase`
  (178 bytes, its own 8-method base class, not yet reconstructed) and
  `CEditable::AddDescriptorsMap` against a real `descCESDiskCommandTask`
  SDescriptor table whose contents are unrecovered -- out of scope for
  a single round, left for a future dedicated pass.

  Test-only default-constructible via `CTask`'s own protected Tier-B
  ctor (task.h, established for exactly this "CTask-derived class,
  real ctor not reconstructed yet" scenario by `CPanelIfcTask`'s
  earlier precedent) -- none of these 84 methods touch the unmodeled
  gap or any vtable, so this is safe.

  Real host KAT (`verify/test_es_disk_command_task.cpp`, 12 checks,
  spot-checking first/last/all-zero/out-of-sequence opcodes across the
  full address range plus the shared `CTask::SetMask` side effect).
  `make verify` full suite green (all pre-existing tests unaffected --
  new class links cleanly with zero new externs, notable given Eva's
  `make verify` links every test against the full `$(OBJ)` tree). Eva
  manifest 2864 -> 2948/37,795 (7.800%).

  Real-HW test that would help: none identified -- pure host-side
  literal-table + already-real `CTask::SetMask` logic, no
  hardware-observable behavior beyond what the disk-command dispatcher
  (deferred `ExecuteLoadCommand`/`ExecuteSaveCommand`) would eventually
  consume.
