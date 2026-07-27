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
10-16. The 7-member `XxxApiInstance` sibling family — `EditApiInstance`, `SeqApiInstance`, `ChkApiInstance`, `DumpApiInstance`, `SysExApiInstance` (all 4 slots fixed, same base no-ops as #9), `RTRouterApiInstance` (fixed with 2 real class-specific override functions, not just the shared base no-ops), `RMApiInstance` (3 of 4 slots fixed; slot 2/`PreKernelConstructor` deliberately left `EvaVTableStub` — it needs an entirely unmodeled `CJobStack` class, precisely scoped and documented rather than forced) — commit `34eda81` (2026-07-27), dynamically re-verified live via 26 real gdbstub hits during a real `CKernel::CKernel()` construction pass on the freshly `tools/build_lenny.sh`-built binary

Running total: 16, found across three sessions (2026-07-25 through 2026-07-27).
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
rebuild, and — for 9 through 16 specifically — a live dynamic trace against
the real rebuilt binary showing the dispatch now lands on the correct
function. There is no known behavioral difference left to test on real
hardware *for this bug class specifically*: real hardware runs Korg's own
binary, which never had this defect, and this project's reconstruction has
now had all 16 known instances of it fixed (with one slot, `RMApiInstance`'s
`PreKernelConstructor`, deliberately and visibly left as a documented stub
rather than silently wrong).

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

- **Peg GUI toolkit** (149 classes) — confirmed genuinely unreached by the
  boot path (Eva reaches its own `Start closing`/`End closing` shutdown
  without it); not tested on real hardware because nothing in this
  reconstruction's own call graph ever needs it. See `README.md` Stage 5.
- **`CZ` string container** (247 methods) — confirmed out of scope
  project-wide, kept opaque everywhere it's a dependency (`CBatchDiskMainTask`,
  `CConfigManager::CreateResourceFamilies()`); not tested on real hardware
  because this reconstruction never implements its real string/set
  semantics, only its `sizeof`.
- **`CStorage`/`CControlSurface`/`CMMI`/`CModeManager`** — confirmed
  genuinely deep UI/control-surface state backing the 2 remaining
  `CSTGUnsolMsgHandler` Tier-B handlers below; not tested on real hardware
  because none of these classes are modeled beyond stub declarations.
- **`CSTGUnsolMsgHandler::ControlMsgHandler`/`VoiceModelMsgHandler`** — the
  last 2 of 30 message handlers, confirmed genuinely deep (re-checked
  multiple times across the session, no tractable angle found); a real
  message of either type arriving on real hardware currently hits an
  unimplemented Tier-B stub in this reconstruction, not real behavior.
- **`CEditClient`'s hash-table + free-list allocator** — confirmed
  genuinely deep (an open-chaining hash table comparable in scope to `CZ`
  itself, re-confirmed with concrete evidence 2026-07-26); not tested on
  real hardware because `Register()`/`Unregister()` are unimplemented.
- **`CDumpManStateMachine` family** — confirmed genuinely deep, deferred;
  real SysEx/dump-protocol state-machine behavior is untested.
- **`COutLinkIfcBase`/`CMarshaller<T>` framework** — confirmed genuinely
  deep shared interface-link infrastructure (also blocks `ILimiterNotify`/
  `IAlphaKeybEvent`/`IAlphaKeybCtrl`, not just `CAlphaKeybCtrlTask`); not
  tested on real hardware because no concrete instantiation exists yet.
- **10 `CXxxTask` ES-family UI god-objects** (`CESCommonTask` through
  `CESSongTask`, 52–1092 real methods each) — confirmed deliberately out of
  scope, not constructed anywhere on the currently-wired boot path; the
  actual per-editor-page UI/model logic behind every edit screen is
  entirely unmodeled.
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
