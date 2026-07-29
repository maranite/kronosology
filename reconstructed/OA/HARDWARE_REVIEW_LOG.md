# Hardware Review Log

Questions/issues found during autonomous OA.ko RE sweep (goal set 2026-07-25:
"Reverse engineer all of OA.ko"). Real-hardware testing deferred to end per
goal instructions. Ornith agent (192.168.0.14:8088/v1) used for aux tasks
where noted -- review its output for wrong answers before trusting.

Format: `## <fn/topic>` + what's uncertain + what real-HW test would confirm.

---

## CSTGMidiPortManager::Initialize() port loop — 2 real crashes went live for the first time, ALL NOW FIXED (2026-07-27)

Found during the project's first OA.ko+Eva joint integration boot test.
`5a1b107`'s fix (calling `ConstructKorgUsbMidiPorts()` explicitly as the
first statement of `init_module()`) had an unverified side effect: it made
`CSTGMidiPortManager::Initialize()`'s port-registration loop — previously
confirmed dead code, since `sMidiInPorts[]`/`sMidiOutPorts[]` were always
all-NULL — genuinely live for the first time, since `RegisterMidiInPort()`/
`RegisterMidiOutPort()` now run for real during construction. Two real
crashes surfaced on a live boot as a result (commit `13fba9f`):

1. **Fixed**: `CSTGMidiInPortKorgUsb`'s placeholder vtable was a literal
   `{0, 0, 0}` — safe only while nothing ever dispatched through it.
   `PortQuery()` now calls slot 0 for real, which was NULL, causing a kernel
   NULL-pointer-dereference Oops. Fixed with a clearly-labeled safe stub
   (`KorgUsbInPortPortQueryStub`, returns `false`) instead of the crashing
   literal zero.
2. **Root-caused and fixed (2026-07-27, follow-up)**: after fixing (1), a
   second, deeper crash appeared in the same loop on the out-port side —
   `CSTGMidiOutPort`'s own vtable POINTER FIELD (not just a slot within it)
   read back NULL for `CSTGMidiOutPortKorgUsb` instances. `13fba9f` shipped a
   defensive null-guard in `PortQuery()`/`PortRegister()` (treat a null
   vtable pointer as "query says no") and explicitly deferred root-causing
   WHY. Direct `objdump -dr -M intel` against ground truth
   (`/home/share/Decomp/OA.ko_Decomp/OA.ko`) resolved it:
   - `CSTGMidiOutPort::CSTGMidiOutPort()` (`.text+0xf8270`) writes
     `this+0x00 = &_ZTV15CSTGMidiOutPort + 8` as literally its first
     instruction; `CSTGMidiOutPortKorgUsb::CSTGMidiOutPortKorgUsb()`
     (`.text+0x340650`) then overwrites it with
     `&_ZTV22CSTGMidiOutPortKorgUsb + 8` right after the base ctor call
     returns — standard Itanium-ABI vtable-pointer establishment. Neither
     write existed anywhere in the reconstructed C++ ctors (both faithfully
     transcribe every OTHER field write) — a pure missing-field-write gap,
     NOT a `.ctors`/`.init_array` issue (this ctor genuinely runs, via
     `Construct()`/placement-new, already fixed by `5a1b107`), and NOT a
     "used before constructed" ordering issue (ground truth's own ctors
     never leave a real window where the field is unset).
   - `.rel.rodata._ZTV22CSTGMidiOutPortKorgUsb` (9 relocations) resolves
     EVERY slot to an already-reconstructed real method (no stubs needed):
     slot0 `ShouldActivate() const` (`return true;`), slot1 `Activate`,
     slot2 `Deactivate`, slot3 `BumpTimers` (inherited, not overridden),
     slot4 `CanSendRealTime`, slot5 `CanSendRegular`, slot6
     `ProcessRegularMessage`, slot7 `SendRealTime`, slot8 `SendSingleByte`.
     This also independently resolves the ambiguity `midi_korgusb_port.cpp`'s
     own header comment had flagged as unreconciled ("slot 0 bool query vs.
     dtor"): ground truth's `CSTGMidiPortManager::Initialize()`
     (`.text+0xf4f60`) itself does `call [edx]` (slot 0, `test al,al`-gated)
     then `call [ecx+0x4]` (slot 1, region pointer in edx) for all 8 port
     slots — byte-identical to this project's own `PortQuery()`/
     `PortRegister()` shape, and slot 0 is `ShouldActivate()`
     (unconditionally `return true;` in all 3 ground-truth comdat
     instances), not a destructor.
   - **Fix**: a real `_ZTV22CSTGMidiOutPortKorgUsb[9]` array of forwarding
     trampolines (same technique as `KorgUsbInPortPortQueryStub`) is written
     into `outPort+0x00` immediately after `Construct()`'s placement-new
     (`midi_korgusb_port.cpp`), mirroring the InPort side's own established
     precedent 2 lines up in that same function.
   - Making this path live for the first time exposed **2 further,
     independently root-caused bugs** in the now-reachable
     `STGMidiOutPortKorgUsb_OutputThread()`/`sOutputWaitQueueHead` pump
     infrastructure (both fixed in the same pass, confirmed via
     `objdump -dr` byte-for-byte against `.text+0x3409ea`-`0x340a06` and
     `readelf -x .data`+`readelf -rW` at `.data+0xa5c4`):
     - The per-iteration `wait_queue_t` stand-in (`waitEntry`) was an
       entirely uninitialized raw buffer — ground truth writes all 5 real
       fields (`flags=0`, `private=current`, `func=&autoremove_wake_function`,
       self-linked `task_list`) fresh every time through the sleep branch,
       immediately before `prepare_to_wait()`. Missing this crashed inside
       `finish_wait()`'s own `list_del_init()` on garbage stack contents
       (live boot Oops: `CR2=0x7f`, `EIP: finish_wait+0x37`, pid
       `STGMidiOutKorgU`).
     - The global `sOutputWaitQueueHead` was modeled as a zero-initialized
       `unsigned char[8]` — undersized (real `wait_queue_head_t` is 12
       bytes: 4-byte spinlock + 8-byte `list_head`) AND wrongly zeroed: a
       real empty `list_head` must self-reference, never point at NULL.
       Ground truth's own `.data+0xa5c4` bytes + 2 `R_386_32` relocations
       (both resolving back to `.data+0xa5c8`, the list_head's own address)
       confirm a compile-time `DECLARE_WAIT_QUEUE_HEAD()`-style static
       initializer, never a runtime `init_waitqueue_head()` call. Missing
       this crashed inside `prepare_to_wait()`'s own `list_add()` writing
       through a NULL `task_list.next` (live boot Oops: `CR2=0x00000004`,
       `EIP: prepare_to_wait+0x4b`, same thread/pid). Fixed by replacing the
       raw byte array with a properly-sized, self-referencing C++ static
       initializer (a real link-time constant — a static object's own
       address is a valid constant expression, so this carries no
       `.ctors`/`.init_array` risk).
   - The defensive null-guard in `PortQuery()`/`PortRegister()`
     (`midi_port_manager.cpp`) is KEPT as cheap defense-in-depth (this exact
     "ctor transcribes every field except the vtable pointer" mistake has
     now recurred twice project-wide — this bug, and `CSTGDrumPadClient`'s
     `.init_array` vtable, `87e446d` below) but its comment is rewritten:
     null is never legitimate for a correctly-constructed instance, ground
     truth's own ctors write it unconditionally.

Verified: host `verify/` suite green (97/97 test binaries pass, same set
before/after), clean `make ko-clean && make ko KDIR=/home/build/linux-kronos`
rebuild, and 3 successive live `kronos_vm` boots in a disposable worktree
(disk image copy + `guestfish` module injection, never touching another
session's instance) tracking each fix in turn: boot 1 (vtable-population fix
only) still Oopsed inside `finish_wait()`; boot 2 (+ `waitEntry` fix) Oopsed
one layer deeper inside `prepare_to_wait()`; boot 3 (+ `sOutputWaitQueueHead`
fix) reached `OA_DEBUG_MARKER 17`/`OA: init_module succeeded` with **zero**
`BUG:`/`Oops:`/`Call Trace:` in the console log, Eva launched and confirmed
alive 8s later, then hit the separate, already-documented (2026-07-27)
fakefb.ko `register_framebuffer()` stall — identical to the pre-existing
known issue, unrelated to this fix.

**Follow-up sweep (2026-07-27, same day): the SAME "ctor never writes the
real vtable pointer" bug found in the 2 BASE classes, `CSTGMidiInPort` and
`CSTGMidiOutPort`.** Dispatched to check whether every other
`CSTGMidiXxxPort`/`CSTGMidiInPortXxx`/`CSTGMidiOutPortXxx` variant in this
project has the same gap. Result:

- `CSTGMidiInPortKorgUsb` — explicitly re-verified CLEAN for this specific
  pattern: its vtable-pointer field IS correctly written, by
  `CKorgUsbAudioDriverMidiPorts::Construct()` immediately post-construction
  (the fix above, item 1). The still-open item there (which real function
  belongs at slot 0 — `KorgUsbInPortPortQueryStub` is a deliberately labeled
  safe placeholder, not the resolved real dispatch) is a *different*,
  already-tracked ambiguity, out of scope for this specific bug class.
- **Found and fixed**: `CSTGMidiInPort::CSTGMidiInPort()`
  (`midi_in_port_serial.cpp`) was writing a literal `0` to `self+0x00` where
  ground truth's own `.text+0xf59aa` writes `&_ZTV14CSTGMidiInPort + 8`
  (`R_386_32` relocation, confirmed via `objdump -dr -M intel`) — the prior
  comment on that line ("own vtable not yet reconstructed") was itself the
  bug marker. `.rel.rodata._ZTV14CSTGMidiInPort` (3 slots) resolves: slot0
  `__cxa_pure_virtual` (dtor, still pure in this base), slot1
  `Activate(CSTGMidiQueue*)` (real), slot2 `Deactivate()` (real).
- **Found and fixed**: `CSTGMidiOutPort::CSTGMidiOutPort()`
  (`midi_out_port_serial.cpp`) wrote NO vtable field at all — a bare gap,
  not even a wrong placeholder — where ground truth's own `.text+0xf827a`
  writes `&_ZTV15CSTGMidiOutPort + 8` (same relocation pattern, confirmed
  via `readelf -rW`). `.rel.rodata._ZTV15CSTGMidiOutPort` (9 slots)
  resolves: slot0 `__cxa_pure_virtual` (dtor, pure), slot1 `Activate` (real),
  slot2 `Deactivate` (real), slot3 `BumpTimers` (real, non-pure base body),
  slots4-8 `__cxa_pure_virtual` (`CanSendRealTime`/`CanSendRegular`/
  `ProcessRegularMessage`/`SendRealTime`/`SendSingleByte`, all still pure in
  this base — matches the class's own already-documented vtable-shape
  comment exactly).
- Both fixes use the same free-function-trampoline technique the
  `_ZTV22CSTGMidiOutPortKorgUsb` fix above established, referencing the
  already-real `__cxa_pure_virtual` (`new_delete.cpp`) directly for every
  still-pure slot.
- **Currently dead in practice**: the only live construction site in this
  project, `CKorgUsbAudioDriverMidiPorts::Construct()`, already overwrites
  both fields immediately post-construction with the correct
  derived-class vtables (see the fix above). Fixed anyway, for the same
  reason the defensive `PortQuery()`/`PortRegister()` guard above was kept:
  a future construction site that doesn't override the field should fail
  into a confirmed pure-virtual trap instead of a silent NULL dispatch.
- Also confirmed clean, no fix needed: `CSTGMidiInPortGeneric`/
  `CSTGMidiInPortSerial`/`CSTGMidiOutPortUSB` add no vtable of their own in
  ground truth (`readelf -sW`/`readelf -rW` confirm no `_ZTVxxx` section and
  no own constructor symbol for `CSTGMidiInPortSerial` specifically — both
  are field-less subclasses of the now-fixed base, carrying only their own
  real method names); `CSTGUSBMidiAccessoryMidiInPort`/`CSTGMidiInPortUSB`
  have no construction site anywhere in this project at all (already
  documented as a deliberately deferred scope decision, unrelated to this
  bug class).

Verified: host `verify/` suite green (124 test binaries, 0 failures,
including `test_midi_korgusb_port`'s own static-ctor-sanity checks and the
dedicated `test_midi_in_port_serial`/`test_midi_out_port_serial` KATs).
Clean `make ko-clean && make ko KDIR=/home/build/linux-kronos` rebuild.
Live-verified on a disposable `kronosvm` instance (fresh copy of the
known-good `full_integration_test_20260727` disk image, freshly-built
`OA.ko` injected via `guestfish upload`, MD5-verified to match the local
build exactly): reaches `OA_DEBUG_MARKER 17`/`OA: init_module succeeded`/
`[loadoa] OA.ko: LOADED OK`, Eva alive at 8s, zero `Oops`/`BUG:`/`panic`
(the only case-insensitive "bug" match in the whole console log is the
benign, unrelated, pre-existing "MP-BIOS bug: 8254 timer not connected to
IO-APIC" kernel printk) — identical behavior through the point where the
separate, already-tracked fakefb `register_framebuffer()` stall takes over.
VM instance torn down cleanly afterward.

## CSTGDrumPadClient's own vtable — 6th confirmed `.ctors`-vs-`.init_array` instance, real bug fixed (2026-07-27)

The `CSTGDrumPadClient` reconstruction added earlier the same day (`e00cd3e`,
see the entry below) populated its own 3-method vtable
(`g_drumPadClientVtable`, `src/init/drumpad_init.cpp`) via
`AsRawFn(&CSTGDrumPadClient::Method)` — a member-function-pointer-to-`void*`
conversion inside a static aggregate initializer. That conversion is not a
C++ constant expression, so GCC silently emitted the vtable's population as
a dynamic initializer (`.init_array`) rather than a plain link-time-constant
array — the exact same anti-pattern already fixed 5 times earlier the same
day (`804b909`, `5a1b107`, `9c587a2` ×2), and Linux's kernel module loader
never runs `.init_array` for a loaded module. Left as-is, this would have
installed a vtable of all-zero pointers, and any real dispatch through
`CanReceiveTriggerEvent`/`ReceiveTriggerEvent`/`ReceiveNotification` would
jump through address 0 — i.e. the very fix that made `CSTGDrumPadClient`
real also reintroduced the bug class it was supposed to avoid, caught the
same day by a follow-up survey rather than live-boot testing.

Found via direct `readelf -S`/`-r`/`nm` inspection (not code review): the
vtable lived in all-zero `.bss` before the fix, moved to properly-relocated
`.data` after. Fixed with the same free-function-trampoline pattern used in
`804b909` (compile-time-constant addresses, no dynamic initializer). Audited
every other `AsRawFn(&Class::Method)` call site in the project
(`control_msg_handler.cpp`, `front_panel_msg_handler.cpp`) — both use the
same risky pattern but only inside constructor-body statements for classes
never instantiated anywhere in the current source, so they're dead code, not
live bugs (flagged here so a future reconstruction of either class re-checks
this before instantiating them for real). Fixed as commit `87e446d`; host
`verify/` suite green, real Kbuild rebuild clean. Real-hardware relevance:
none — this bug only existed in the reconstruction's own transcription of
the vtable population, not in ground truth, and is fully fixed with no VM
dependency (invisible to host-only testing by nature, since host ELF
binaries run `.init_array` normally — this specific bug class can only be
caught by inspecting the built kernel-module `.ko` directly).

## Fresh re-audit of prior "dead"/"no-op"/"unreachable" classifications — negative result, one documentation refinement (2026-07-27)

Following the same day's `CSTGDrumPadClient` find (a real vtable-install
bug hiding behind an earlier sweep's "no-op"/"opaque placeholder"
classification, `e00cd3e`), did a fresh pure-static-analysis re-audit of a
sample of this project's other "dead"/"no-op"/"unreachable" classifications,
using the same discipline (`objdump -dr`/`readelf -r` against ground truth
`955636c2...` OA.ko, not re-trusting a prior sweep's conclusion). Result:
3 spot-checks reconfirmed correct, 1 flagged-out-of-scope item got a real
new fact but no change to its deferred status, no new bug found:

1. **`CSTGControllerRTData::SetAudioInSolo()`'s pure-virtual claim**
   (batch 57 entry above) -- independently re-derived `_ZTV15CSTGPerformance`
   vtable slot 27's byte offset (`8 + 27*4 = 0x74`) from the mangled vtable
   layout and confirmed via `readelf -r` that offset resolves to
   `__cxa_pure_virtual` in ground truth. Confirmed correct.
2. **`CSTGMidiOutPortSerial`'s "2 trailing vtable slots are provably dead"
   claim** (`CanTransmitHardware()`/`TransmitHardwareByte()`, MIDI-OUT UART
   cluster entry above) -- `readelf -r` on `.rel.rodata._ZTV21CSTGMidiOutPortSerial`
   confirms the trailing 2 of 11 slots (offsets `0x2c`/`0x30`) are indeed
   `__cxa_pure_virtual`, matching the claim exactly. Confirmed correct.
3. **`CSTGCalibrationMsgHandler::sInstance`'s ctor "touches a different,
   unrelated .bss byte that's already 0" claim** (`LESSON_ctors_vs_init_array.md`)
   -- worth extra scrutiny since a REAL bug was found in this same class
   today (`HandleKeybedCalibrationResult`, entry above). Disassembled
   `_GLOBAL__I__ZN25CSTGCalibrationMsgHandler9sInstanceE` (`.text+0xdf120`,
   8 bytes) directly: `movb $0x0,0x9e710` (`R_386_32` to `.bss`) --
   `sInstance` itself is a separate 4-byte object at `.bss+0x9e660` (176
   bytes away), untouched by this ctor. Confirmed correct: a genuine
   compiler-arbitrary `_GLOBAL__I_` naming coincidence, not related to
   today's real bug.
4. **`CSTGUSBMidiAccessoryMidiInPort`'s "no global object exists yet,
   flagged out of scope" classification** -- see
   `LESSON_ctors_vs_init_array.md`'s own 2026-07-27 re-check note for the
   full trace. Found one genuinely new fact (a previously undocumented
   sibling class, `CMidiInClient`, real 20-byte `Receive()` forwarder, and
   that the ctor at `.text+0xfa840` constructs BOTH objects together) but
   a whole-binary xref sweep confirms neither object nor `Activate()`/
   `Deactivate()` has any caller anywhere in OA.ko outside their own
   mutual references -- unlike `CSTGDrumPadInterface_Initialize()`, nothing
   in `init_module()` or any other unconditional path reaches this pair.
   Correctly deferred, not a hidden reachability gap.

Also spot-checked the 18-entry `CSTGCalibrationMsgHandler::sMsgHandler[]`
and the 54-entry `CSTGControlMsgHandler::sMsgHandler[]` dispatch tables
(the "function referenced by a real dispatcher but left as a plain stub"
pattern) -- all 18 calibration start/end/cancel handlers and all
non-`HandleUnsupportedMessage` slots of the control table resolve to real,
previously-reconstructed method bodies in current source, no stub-in-a-live-
table gaps found.

**Conclusion**: negative result for new bugs. This project's structural/
hardware-integration scope is genuinely close to exhausted along this
specific axis (mis-classified dead code) -- `CSTGDrumPadClient` was real,
but the sample re-audited here holds up under the same fresh-eyes
`objdump -dr`/`readelf -r` scrutiny that found it. `LESSON_ctors_vs_init_array.md`
updated to reflect both this negative result and the now-stale
`CSTGDrumPadClient`/`CMidiInClient` notes it previously carried.

---

## CSTGCalibrationMsgHandler::HandleKeybedCalibrationResult — real bug fixed 2026-07-27 (state 0x10 reply polarity)

Found during a targeted correctness re-audit (not live-boot serendipity),
following the 2026-07-27 dynamic-testing session that found the
literal-vs-relocation and `.ctors`-vs-`.init_array` bug classes elsewhere in
OA.ko (see `SESSION_SUMMARY_2026-07-25.md`'s 2026-07-27 section). Since
`CSTGCalibrationMsgHandler` has never actually been live-boot-tested (no
construction call site exists anywhere in this project yet — confirmed
separately via the 2026-07-27 `oa_vtable_check.ko` sweep), this was pure
static re-verification: re-tracing `HandleKeybedCalibrationResult`'s real
18-entry `.rodata+0x4b31c` jump table instruction-by-instruction against
`/home/share/Decomp/OA.ko_Decomp/OA.ko` (`objdump -dr`, table dumped via
`objdump -s -j .rodata`).

**The bug**: the original reconstruction (`bc06fdd`) had jump-table states
`{2,5,0xc}` (which explicitly force `xor eax,eax` before their shared
message-building tail) and state `0x10` (which does NOT force this — it
falls into the SAME shared tail as `{1,4,0xb}`, with `esi` == the real
`success` parameter still live) both modeled identically as `SendReply(0)`,
hardcoded. Tracing `0xdecd5` (state `0x10`'s jump target) forward to
`0xded02` (the shared reply-building tail) found no `xor eax,eax`/
`mov esi,...` anywhere on that path — meaning state `0x10`'s reply
genuinely depends on the real `success` argument (`success ? 0 : -1`),
exactly like states `{1,4,0xb}`. Only state `0x11` (aftertouch CANCEL, not
END) forces `esi=1` first (`mov esi,0x1` at `0xdecd0`) before falling into
the identical shared body — so `0x11`'s "always reply 0" behavior is a real
but state-*11*-specific side effect, not something states `0x10`/`0x11`
both do by design. The original comment's "then reply is unconditionally
success (0)" for state `0x10` conflated the two.

**Real-world effect had this shipped**: on a real front-panel aftertouch
calibration, if the keybed hardware ACKs the calibration END with a genuine
failure (`success=false`), the reconstruction would have told the UI it
succeeded (`reply.result=0`) instead of the correct `-1`.

**Why host KAT didn't catch it**: `verify/test_calibration_msg_handler.cpp`
had a scenario for state `0x11` (`CancelAftertouchCalibration()`, where
`success` is forced true regardless of the passed-in value, so hardcoding
`SendReply(0)` happened to produce the right answer) but no scenario at all
for state `0x10` alone (`EndAftertouchCalibration()`'s keybed-hw path) with
`success=false` — the exact case that exposes the divergence. Added test
`[17b]` (2 new sub-checks: `success=false` -> `reply.result==-1`,
`success=true` -> `reply.result==0`) closes that gap.

**Fix**: `SendReply(0)` -> `SendReply(success ? 0 : -1)` in the `case 0x10:`
body (`src/init/calibration_msg_handler.cpp`); header comment above the
function corrected with the full per-entry jump-target trace. Host
`verify/` suite re-run clean (124 test binaries, 0 failures); `make ko-clean
&& make ko KDIR=/home/build/linux-kronos` rebuilds clean. Not live-boot
re-verified (no construction call site exists yet, per the note above) --
this is a pure static-correctness fix, same category as the `2c539fb`
literal-vs-relocation fix but caught by re-audit rather than a crash.

Everything else in this cluster's re-audit (all `_vtablePtr =` assignments,
hand-rolled vtable arrays, and placement-new/`new` construction sites
project-wide, spot-checked against the 2026-07-27 systemic sweep's
established "safe" patterns) came back clean -- no further instances of
either the literal-vs-relocation or `.ctors`-vs-`.init_array` bug classes
found in `CSTGControlMsgHandler`/`CSTGCalibrationMsgHandler` or their
neighbors (`CSTGAudioManager`, `CCostProfile`, `CStartupFile`,
`CKorgPreloadFile`/`CKorgProgBankFile`, the 10 `CSTGVoiceModel` subclasses).

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

---

## CSTGControllerInfo::ButtonPressHandler — 37 deferred sub-branches now real, unconfirmed field names (2026-07-25)

Uncertain: all 37 previously-deferred sub-branches (20 Pattern-B, 17
table1/3 special-button else-branches) are now reconstructed from
disassembly with high confidence in the CONTROL FLOW (every branch,
constant, and call target individually traced), but several
`CSTGControllerRTData`/`CSTGGlobal` byte/dword fields involved have no
independently-confirmed NAME, only an observed single-purpose
read/write: `CSTGControllerRTData+0x21` (solo-related byte),
`CSTGGlobal+0x684`/`+0x6a4` (the project's already-established "program/
combi/sequence mode" and a byte flag, reused here in a new context for
`ChangeControlSurfaceMode`'s own dispatch), and the exact real meaning
of `ChangeControlSurfaceMode`'s own `int mode` argument values (0-8)
themselves. `ProcessPerfSwitchPress`/`ResetSolo`/`ChangeControlSurfaceMode`
are new deferred externs -- their own bodies are NOT reconstructed, so
this pass cannot confirm what UI-visible effect they actually have.

Real-HW test that would help: press each of the 12 "special" buttons
(9, 0x2c, 0x35-0x39, 0x4a-0x4e) and the 10 Pattern-B buttons (0x26-0x2b,
0x2f-0x32) individually in each of the ~9 controller-mode states
(`CSTGControllerRTData+0x2b` 0-8), and confirm the real
`SendUnsolControl2MessageToUI`/`SendKarmaCCToKG` traffic matches this
reconstruction's per-branch table -- especially the `ChangeControlSurfaceMode`
cascades for codes 0x35/0x36/0x39, the deepest and least-independently-
cross-checked part of this batch.

---

## CSTGControllerInfo AnalogXxxHandler family — 9 of 22 now real, unconfirmed field names (2026-07-25)

Uncertain: `AnalogRibbonXHandler`/`AnalogVectorXHandler`/
`AnalogVectorYHandler`/`AnalogDamperHandler` all touch
`CSTGControllerRTData`/`CSTGGlobal` byte fields with NO independently-
confirmed name (`CSTGControllerRTData+0x14/0x15/0x16/0x20/0x49`,
`CSTGGlobal+0x6ac/0x6c0/0x6c1/0x29c9fbc`) -- real per-purpose semantics
inferred only from how each byte is used within these five functions,
not cross-checked against any other reconstructed code in this project.
The `STGAPIFrontPanelStatus+0x108/+0x10a` writes in `AnalogRibbonXHandler`
are explicitly NOT claimed to be the same-purpose fields as the
existing `STGAPI_OFF_ANALOG_ECHO_*` constants that happen to share the
0x108 offset (those are written only from a different code path,
`AnalogControllerHandler`'s own busy-flag-SET direct-echo branch) --
this is a real ambiguity, not resolved this pass. The two extracted
`.rodata` tables (`kDamperFilterTable`, `kControllerLockFlagTable`) are
BYTE-EXACT from the real binary (script-extracted, not hand-transcribed
after the first draft caught a manual-transcription row-merge bug), so
their VALUES are not in question, only their semantic PURPOSE.

Real-HW test that would help: move the physical ribbon/vector/damper
controllers through their full range while watching real MIDI CC output
(`SendCCToKG`'s ultimate destination) and the front-panel UI's own
touch-position readback, to confirm this reconstruction's per-field
semantics (especially the ribbon "lock flag" gating and the vector
"assignment" sentinel check) match observed behavior.

---

## CSTGControllerInfo AnalogXxxHandler family — 6 more now real (batch 68, 2026-07-25): pitch-bend double-Filter-call, MIDI terminator byte, echo-slot naming collision, GCC clone signature

Uncertain (four distinct items, all in `AnalogJoystickXHandler`/
`AnalogAftertouchHandler`/`AnalogKnobRTKHandler`):

1. **`CPitchBendFilter::Filter()` called TWICE per invocation with the
   SAME curved value in every real code path.** The control flow is
   traced exactly from disassembly (not assumed), but WHY a stateful
   filter object accepts/rejects the identical repeated call -- and
   whether real hardware ever actually sends two Pitch Bend MIDI
   messages per joystick sample as a result -- is unknown, since
   `Filter()`'s own body is a deliberately deferred extern (matching
   `CJumpCatch`/`CPedalFilter`'s existing treatment). If this doubles
   real Pitch Bend MIDI traffic, a hardware capture would show it as
   two near-simultaneous `0xEn` messages per joystick move.

2. **MIDI message terminator byte's exact meaning is inferred, not
   confirmed.** `AnalogJoystickXHandler`'s Pitch Bend send (2 data
   bytes) and `AnalogAftertouchHandler`'s Channel Pressure send (1 data
   byte, second byte padded 0) both end their 5-byte
   `CSTGMidiQueueWriter::Write()` buffer with `0xff`, while the
   pre-existing Note-On/Off sends (3 data bytes,
   `front_panel_handlers.cpp`) use `0xfe`. This reconstruction's working
   hypothesis -- terminator encodes real MIDI message byte-length, not
   independently confirmed against a third distinct byte-length case --
   is stated in the code comments as inferred, not established.

3. **`AnalogJoystickXHandler`'s own internal echo write target
   (`STGAPI_OFF_ANALOG_ECHO_VECX`, raw offset `0xfe`) is confirmed via
   direct disassembly of THIS function, and is NOT the same offset the
   existing `STGAPI_OFF_ANALOG_ECHO_JOYX` constant (`0x100`) names for
   device 1 -- that other constant comes from `AnalogControllerHandler`'s
   own SEPARATE busy-flag-SET dispatcher echo table (a different call
   site entirely). Both are real, both are correctly reproduced, but the
   existing constant naming (established in an earlier batch, before
   this function's own body was ever disassembled) makes it look like a
   naming collision/error at first glance. Left as-is with a code
   comment pointing here rather than renamed, since renaming
   `STGAPI_OFF_ANALOG_ECHO_VECX` could break `AnalogVectorXHandler`'s
   own already-verified real busy-path use of the same constant name.

4. **`AnalogKnobRTKHandler`'s call to `SetRTKModeKnob`** targets a GCC
   IPA-CP function clone (`.clone.11`) that appears to pass only 2 of
   the real 5-parameter signature's 3 trailing args explicitly at this
   call site -- this reconstruction fills the third with `true`
   (inferred from this project's own "constant true/1 at every observed
   call site" pattern elsewhere, not independently confirmed for this
   specific parameter). Since `SetRTKModeKnob` itself is a deferred
   extern with no reconstructed body, this has no effect on this
   project's own compiled behavior, only on the accuracy of the
   documented call-site values.

Real-HW test that would help: move the physical joystick X-axis through
a large, fast swing while sniffing the real MIDI output stream, to see
whether one or two Pitch Bend messages appear per hardware sample
(resolves item 1); compare the exact byte sequence for a Pitch Bend vs
a Channel Pressure vs a Note-On send side by side to test the
"terminator = byte length" hypothesis (item 2).

---

## SCalibrationData::InitAll() — compiled-in calibration defaults (batch, 2026-07-25)

Uncertain:

1. **Semantics of the 0x00-0x1f "generic curve table" and the 0x9c-0xbc
   "touch screen" field group are not independently confirmed.** Every
   OTHER field group in this function (JoystickX/Y, Ribbon, Vector
   joystick, Half-damper, Aftertouch, LCD control) matches a named
   offset this project already established elsewhere (`calibration_msg_
   handler.cpp`'s `Start*Calibration` methods, or the shared
   `0xc4-0xe7` "LCD control" block). These two groups have no such
   cross-reference anywhere else in this project; their field-by-field
   values are transcribed faithfully from disassembly, but what each
   individual value MEANS (e.g. is 0x9c really a corner-detection pixel
   margin? is the 0x00-0x1f table really shared/reused for something
   other than "drum pad defaults" despite matching InitDrumPads' own
   byte layout almost exactly?) is not established.
2. **The "LCD control" gain (a 1.0f float at +0xc8) and range fields
   (several 0xff/0xffff words) are inferred to be contrast/brightness
   calibration from their position (same struct region `SetLCDBrightness`
   presumably also touches) and shape (a unity gain + saturating
   ranges), not confirmed against `CSTGControlMsgHandler::
   SetLCDBrightness`'s own body (not reconstructed in this pass).**
3. **The known, still-open structural discrepancy flagged in
   `setup_global_resources.cpp`'s own comment at this call site**:
   ground truth calls `InitAll()` then falls through into the SAME
   kind-branch/`SetupNKS4Calibration` code this project's `if
   (calLoaded)` block already runs -- i.e. real hardware exercises that
   code on BOTH the calibration-file-loaded and calibration-file-missing
   paths, this project's reconstruction currently only does so on the
   loaded path. Deliberately left unfixed this pass (see that file's
   own comment for why) -- a real remaining gap, not an oversight.

Real-HW test that would help: delete/rename `/korg/rw/Calibration/
Calibration.img` on a real unit and reboot, then check whether the
front panel's joystick/ribbon/vector-joystick/damper/aftertouch/touch
screen controls and LCD contrast/brightness come up at sane (if
uncalibrated-feeling) defaults rather than being dead/garbage, and
whether `CSTGControlMsgHandler::SetLCDBrightness`'s real UI behavior
matches this pass's "unity gain, 0xff/0xffff range" interpretation of
the 0xc4-0xe7 block.

---

## CPowerOffTimer — Auto Power Off inactivity timer, 7 methods (batch, 2026-07-25)

Uncertain:

1. **Three of DoTimerTick()'s four tick-suppression gate fields have no
   independently-confirmed real name/purpose**, only a raw offset and a
   plausible guess from context: `STGAPIFrontPanelStatus+0x109c`
   ("power-off inhibited", maybe set by an in-progress footswitch/pedal
   calibration or a modal dialog), and `CSTGGlobal::sInstance+0x6a8`
   being in `{1,2}` (an unidentified transient system mode -- e.g.
   save/load in progress). The 4th gate (`CSTGMessageProcessor
   ::sInstance+0x48`) reuses an offset this project already confirmed
   elsewhere as a "messages/UI busy" flag, so that one is on firmer
   ground.
2. **DoTimerTick() itself is not yet wired to a real periodic timer
   source anywhere in this project** -- it is a confirmed-real,
   reconstructed function with no confirmed-real caller yet found (the
   real RTAI/Linux timer tick that drives it was not located in this
   pass). Its behavior is verified in isolation (KAT) but never
   exercised end-to-end.
3. **The exact real-world "lead time" UX is inferred from the numbers,
   not observed**: e.g. a unit with panel-type table value 1200s (20
   min) gets a 120s warning lead, matching the <=1800s bucket, but
   whether the front panel actually shows a "powering off in X seconds"
   countdown, a blinking LED, or something else when state transitions
   to 2 (warning) or 3 (critical) is not confirmed -- `PushUnsolicited
   Message()`'s own UI-side consumer for subtypes 0x27/0x28/0x29 is
   outside OA.ko (EVA or the front-panel firmware) and not examined in
   this pass.

Real-HW test that would help: sit a real unit idle (no keypresses,
knob turns, or MIDI activity) for its full configured timeout and
observe what actually happens on screen/LEDs at the warning threshold
and at expiry, to confirm the 0x27/0x28/0x29 message subtypes map to
the interpretation above; separately, trigger a long save/load
operation and confirm the countdown visibly pauses/resets (tests

---

## CSTGControlMsgHandler — front-panel/remote system control dispatch, 51 methods (batch, 2026-07-25)

Reconstructed in full (Item 1's flagged `SetLCDBrightness` turned out to
be one method of a 100% previously-unclaimed 51-method class -- see
`include/oa_control_msg_handler.h`/`src/init/control_msg_handler.cpp`).

Uncertain:

1. **`MuteADC` actually mutes the AUDIO OUTPUTS in ground truth, not the
   inputs** (confirmed via the real vtable slot offsets used, `+0x3c`/
   `+0x40` = `MuteAudioOutputs`/`UnmuteAudioOutputs`, not `+0x44`/`+0x48`
   = `MuteAudioInputs`/`UnmuteAudioInputs`). Reproduced faithfully, not
   "fixed" -- but worth independently confirming this isn't itself a
   ground-truth compiler/linker mixup (e.g. two adjacent vtable slots
   swapped at build time) rather than a deliberate real behavior. A
   real-hardware test (toggle "Mute ADC" from the UI, listen for which
   signal path actually goes silent) would settle this definitively.
2. **`CLoadBalancer::sInstance+0xa4`** (set by `StartSTG`) and
   **`CSTGAudioManager::sInstance+0xc`'s per-core stats sub-object
   array** / **`+0x3c`'s single FX stats sub-object** (read by
   `ReadCPUUsagePeak`/`ReadFXUsagePeak`) all have no independently
   confirmed field name or purpose beyond "a real, disassembly-confirmed
   offset this function touches" -- transcribed faithfully, semantic
   meaning inferred from the caller's own name/context only.
3. **`ReadCPUUsagePeak`'s 64-bit `fild` load zero-extends its 32-bit
   scaled accumulator (via `movd`+`movq`) rather than sign-extending
   it** -- for realistic (small, non-negative) CPU-usage percentages
   this makes no observable difference, but this project's own C++
   model uses a plain `(double)` cast from a signed `int` (sign-extends)
   for simplicity. Flagged as a known, deliberately-unaddressed
   divergence for pathological/negative accumulator values, not a
   confirmed-safe simplification.
4. **`ChangeBankType`/`FreeStolenVoices`/`StartDownload`/`EndDownload`/
   `CSTGDrumPadInterface::StartScanning`/`COmapNKS4Driver_StartScanning`**
   -- all newly-declared real callees this batch found real callers for,
   own bodies deliberately still deferred (matches this project's
   long-established "reconstruct the caller, defer the DSP/hardware-
   protocol-scale callee" pattern). `ResetAllEffectsInActivePerf` was
   deliberately left a full no-op stub (effect-rack DSP internals, out
   of scope per project policy) despite being a real, disassembly-
   confirmed 416-byte function -- see the header comment for the full
   dependency chain if ever revisited.

Real-HW test that would help: toggle "Mute ADC" and "Mute DAC" from the
front panel/remote UI while monitoring both physical audio inputs and
outputs, to settle uncertainty #1 above; trigger the CPU/FX/disk-
throughput-peak diagnostic reads (if a UI control for them exists) and
compare the reported percentage against a known synthetic CPU load, to
validate the `*100` scale-and-truncate math in `ReadCPUUsagePeak`/
`ReadFXUsagePeak`/`ReadDiskThroughputPeak`.

---

## TurnOnSeqLed / SKSTGGate_ / SPROutGate_ transport LEDs (batch, 2026-07-25)

Reconstructed (`include/oa_seq_led.h`/`src/engine/seq_led.cpp`) --
found while surveying for CSTGControlMsgHandler-adjacent candidates.

Uncertain:

1. **Ids 0/1 (Start/Stop-Red, Start/Stop-Green) are a confirmed real
   no-op in `TurnOnSeqLed` itself** -- re-traced the real jump table
   twice to be sure this isn't a transcription mistake. The real
   Start/Stop transport LEDs must be driven through some OTHER path --
   plausibly `CSTGTempoUtils::FlashStartStopLed`/`FlashTempoLed`
   (`.text+0x27060`/`0x27090`, confirmed real via relocation, only the
   constructor of that class is reconstructed elsewhere in this
   project) -- deliberately NOT pursued this pass: both compute a
   timed one-shot deadline via `CSTGAudioBusManager`'s real-time audio
   clock scale (`fisttp` against a bus-manager-derived tick rate),
   which drifts into real-time audio-tick scheduling/DSP territory
   rather than plain hardware I/O, and was judged to need its own
   dedicated scope judgment rather than a quick fold-in here.
2. **`eSeqLEDId`'s real enum type/name is not independently confirmed**
   -- modeled as a plain `int`, matching this project's established
   convention for not-independently-defined enums.

Real-HW test that would help: press Play/Stop/Rec/Pause/FF/Rew on a
real unit and confirm all 4 (Rec/Pause/FF/Rew) LEDs light via this
exact path; separately confirm the Start/Stop-Red/Green LEDs really do
NOT light via any of the `SKSTGGate_TurnOnStartStopXxxLed`/
`SPROutGate_TurnOnStartStopXxxLed` call sites (would confirm the
no-op is real dead code on shipping firmware, not just unreachable in
this project's current caller graph).
Begin/EndLongProcess and the CSTGGlobal+0x6a8 gate).

---

## CSTGFrontPanelMsgHandler / CSTGFrontPanel::Beep/SetLED16Bits (batch, 2026-07-25)

Reconstructed (`include/oa_front_panel_msg_handler.h`/
`src/init/front_panel_msg_handler.cpp`, plus two new `CSTGFrontPanel`
methods in `front_panel_handlers.cpp`) -- found via a class-level
`nm -C` sweep, wholly unclaimed before this pass.

Uncertain:

1. **`SetLED16Bits` genuinely drops one byte (bits 8-15) of its 32-bit
   input entirely** when repacking the OMAP NKS4 command word -- traced
   instruction-by-instruction twice to rule out a transcription
   mistake (`shr edx,0x8; and edx,0xff00` only ever exposes the
   ORIGINAL bits 16-23, never bits 8-15, and nothing else in the
   function reads that byte). Plausible real explanation: the "16
   bits" of LED state are packed by the caller into bits 0-7 and 24-31
   of the dword (not a contiguous 16-bit field), making the dropped
   byte genuinely unused padding -- but no caller of `SetLED16Bits`
   itself is reconstructed in this project to cross-check that theory.
2. **`Beep()`'s fixed `0x04000000` command word's real effect is
   unconfirmed** beyond "writes to the OMAP NKS4 output FIFO" -- no
   independent cross-reference (e.g. a documented beep-duration/pitch
   parameter elsewhere) to confirm this single opcode is a complete,
   parameterless "beep" rather than one half of a larger protocol.

Real-HW test that would help: trigger a front-panel beep (if a UI
control or SysEx command reaches this path) and listen for the
physical piezo/speaker; for `SetLED16Bits`, would need a live capture
of an actual caller's `m` argument value to confirm the byte-1-is-
padding theory (or find the real caller function to reconstruct and
settle it definitively).

---

## CSTGOmapNKSMsgHandler::ProcessNextNKSEvent (batch, 2026-07-25)

Reconstructed (`include/oa_omap_nks_msg_handler.h`/
`src/init/omap_nks_msg_handler.cpp`) -- the real USB-NKS4-panel event
pump, found via the same sweep as CSTGFrontPanelMsgHandler above; ties
together four already-real `CSTGFrontPanel::Handle*` dispatch targets
that had no reconstructed caller before this pass.

Uncertain:

1. **Byte-order asymmetry between sibling event types is real, not a
   transcription slip** -- re-verified against the raw disassembly
   multiple times: the rotary-delta and touch-panel-coord branches both
   build their 16-bit value as `(b1<<8)|b0`, while the raw-analog-test-
   capture (type 0x61) and its 0x62 companion both build theirs as
   `(b0<<8)|b1` -- byte roles genuinely swapped between the two
   families. Plausible (different firmware authors/eras for the
   "normal" vs "diagnostic" event paths) but not independently
   confirmed against any other source.
2. **Real semantics of the fixed 16-byte `subtype 0x2a` message (event
   type 0x08) are not confirmed** beyond "always sends `value=0`,
   unconditionally, once per occurrence of this event type" --
   plausibly a keybed-ready/panel-connect ping, no independent
   cross-reference.
3. **The second, front-panel-specific `CSTGKeybedKeyDebounceFilter`
   instance's real static storage address (`.bss+0x2367e0` in ground
   truth) and its relationship (if any) to the keybed's own embedded
   instance are not independently cross-checked** -- this handler is
   currently the ONLY reconstructed caller that touches it.
4. **`ShortInvertNkS4RawAnalogValue`'s two output-pointer roles are
   NOT symmetric with its sibling `ShortInvertNkS4AnalogValue`'s own
   (outHi, outLo) naming convention** (see the header comment) --
   reproduced exactly as disassembled, but the naming mismatch between
   the two sibling functions is itself unconfirmed as intentional
   (could reflect two independently-written functions rather than a
   single deliberate convention).

Real-HW test that would help: enable NKS4 panel test mode (if reachable
from a service menu) and watch `chip_sniff_ring.bin`-style USB traffic
while pressing front-panel buttons/touching the touch panel/moving the
joystick, to confirm the type-byte dispatch table and byte-order
theories above against real captured packets.

---

## CSTGHDRFileReader/CSTGStreamingFileReader::ProcessCommands() cluster (batch, 2026-07-25)

Reconstructed (`managers.cpp`): both classes' `ProcessCommands()` plus
all 9 real per-tag dispatch handlers, plus their shared dependency
`CSTGStreamingEventManager::ReturnFreeEvent()`/`CSTGStreamingEvent::
CloseFileDescriptorsIfNecessary()`/`HandleErrorReading()`
(`streaming_event_manager.cpp`). This REVISES a long-standing "blocked
by an unrecovered `TSTGArrayManager<T>::indexArray` function-pointer
table" verdict (batch 28 onward) -- the real per-command dispatch
tables turned out to be per-instance `{funcptr,adj}` pairs baked
directly into each object's own data by its ctor/`Initialize()`,
fully disassembly-recoverable, not a genuinely-missing data table.

Uncertain:

1. **`rtwrap_global_save_flags_and_cli()`/`rtwrap_global_restore_flags()`
   (`bar2_stubs_c.cpp`) are a FUNCTIONAL substitute for RTAI's real
   global ticket-spinlock-plus-per-CPU-bitmap algorithm, not a literal
   opcode-for-opcode reproduction** -- the real algorithm (RTAI's
   `rt_global_cli()`/`rt_global_sti()`) is reproduced via GCC atomic
   builtins operating on the same real `rtai_cpu_lock`/
   `per_cpu__cpu_number` externs at the same confirmed byte offsets,
   including the two "should-never-happen" defensive branches ground
   truth's own `restore_flags()` carries -- but genuine multi-CPU RTAI
   contention behavior (cache-line bouncing, real scheduling
   interaction with `rtai_sched.ko`) has never been exercised, only a
   single-threaded host KAT. This is squarely in-scope for this
   project's own RTAI-substitution policy, but real-hardware SMP
   testing under actual file-daemon load would be the only way to
   confirm the substitute behaves identically to the real primitive
   under contention.
2. **`CSTGStreamingEventManager::field14c3c`'s real high-level purpose
   is not independently determined** -- confirmed real via disassembly
   (touched by both `AddSoundingEvent()` and `ReturnFreeEvent()`,
   compared against the event pointer being added/returned, reset to 0
   on a match), faithfully reproduced, but its semantic role (a
   "pending wake signal for this specific event", by analogy with
   `AddSoundingEvent()`'s own `signal_daemon()` call, is a plausible
   but unconfirmed guess) is unclear.
3. **`CSTGStreamingEvent+0xb8`/`+0xc4`'s real semantics are
   undetermined** beyond "an unsigned dword pair `ProcessCommandFilled
   Bytes()` compares to gate the `CSTGDiskCostManager::
   UpdateDiskThroughputBytesRead()` call" -- plausibly a
   position/limit or read-count/total-size pair, no independent
   cross-reference confirms which.
4. **The underlying vtable-dispatched disk-I/O calls this whole
   cluster ultimately feeds (`CSTGFileCloser`'s own vtable-slot
   dispatch, already flagged in the batch 64ish log entry above) remain
   unreconstructed** -- this pass adds two more real producers into
   that same not-yet-reconstructed sink, so it still can't say anything
   about real SSD/flash timing vs. this project's own ring-capacity
   assumptions.

Real-HW test that would help: trigger real HDR streaming playback
(long sample/multisample-stream voice) and record-then-read-back audio
on a real Kronos while watching for file-daemon-related dmesg/timing
anomalies, particularly under heavy multi-voice load where
`CSTGStreamingEventManager`'s free-list/sounding-list churn and the
`rtwrap_global_*` lock would see genuine contention across the SMP
Atom's 4 logical CPUs -- something a single-CPU VM sandbox can't
exercise.

## CSTGUSBMidiAccessoryMidiInPort / CSTGMidiOutPortUSB — generic USB-MIDI accessory plumbing (batch, 2026-07-25)

Real-hardware-verification uncertainty for the newly-reconstructed
generic-USB-MIDI-class accessory Activate/Deactivate methods
(`src/engine/midi_usb_accessory_port.cpp`):

- **Not reachable from any real USB-MIDI accessory in this VM-
  substitution effort at all**, same situation as the KorgUsb transport
  above: `USBMidiAccessory_SetMidiInClient()`/
  `USBMidiAccessory_SetDrumPadClient()` are real externs owned by
  whatever companion module drives the generic-USB-MIDI-class accessory
  hierarchy (a different module from `KorgUsbAudioVirtualDriver`, not
  independently identified this pass) -- this project provides no
  virtual substitute for it, so these methods are disassembly-verified
  and host-KAT-verified in isolation only, never exercised end-to-end
  with a live or virtually-responding accessory driver. A real-HW test:
  with an actual Kronos and a class-compliant USB-MIDI accessory (not
  Korg's own composite audio+MIDI interface) plugged in, confirm
  `USBMidiAccessory_SetMidiInClient(&sMidiInClient)` actually gets
  called on connect and that `CMidiInClient::Receive()` (still
  unmodeled, see below) correctly forwards received bytes into
  `CSTGMidiInPortGeneric::Receive()`.
- **`sMidiInClient`'s own type (`CMidiInClient`) and the
  `sUSBMidiAccessoryMidiInPort` singleton it hard-redirects into are
  deliberately NOT modeled this pass** -- only `sMidiInClient`'s address
  is used (to register/unregister with the companion module), never its
  behavior. If a future session reconstructs `CMidiInClient::Receive()`,
  real-HW testing would need to confirm actual USB-MIDI-class IN
  traffic reaches the engine via this exact redirect path.
- **The 2 confirmed different-in-kind blockers this batch left alone
  are unchanged and still real dead ends in ground truth itself, not
  gaps here**: `CSTGMidiOutPortUSB::CanSendRealTime()`/`CanSendRegular()`/
  `SendRealTime()`/`SendSingleByte()` all resolve to `__cxa_pure_virtual`
  in this class's own vtable (confirmed via `readelf -r`, no concrete
  override exists anywhere in OA.ko) -- calling any of them on a real,
  unmodified `CSTGMidiOutPortUSB` instance would fault on real hardware
  too, so no real-HW test can ever exercise them differently than a
  crash. `CSTGDrumPadClient::CanReceiveTriggerEvent()`/
  `ReceiveTriggerEvent()` remain blocked by genuine linker-adjacency
  aliasing (fields accessed via addresses relocated against
  `CSTGDrumPadInterface::sInstance+N`) -- not reproducible in a
  clean-room rebuild regardless of real-HW access.

## Virtual-dispatch-invisibility re-check of remaining bar2_stubs.cpp/bar2_stubs_auth.cpp stubs (2026-07-25) — negative result, no real-HW test applicable

Following the `CClientCommServer` precedent from the Eva side (a real
class reached ONLY through a `.rodata` vtable-slot relocation, invisible
to a plain call-site grep), re-checked every remaining stub target in
`bar2_stubs.cpp`/`bar2_stubs_auth.cpp` (56 resolved C++ method targets,
including all 10 explicitly named in this batch's briefing --
`CSTGMonitorMixer::RunMonitors`, `CSTGAudioBusManager::
MixPerformanceOutputs`, `CSTGControllerRTData::OnPerformanceActivate`,
`CSTGMidiDispatcher::ResetAllControllers`, `CSTGVoiceAllocator::
DoPendingMoveVoices`, `CSTGPCMPrecacheManager::AfterProcess`,
`CSTGHDRMiniModel::Initialize`, `CSTGSlotVoiceData::UpdateGlobalTune`,
`CSTGPianoModel::RescanPianoTypes`, `CSTGEffectRackVars::Initialize`)
against ground-truth `OA.ko`'s own relocation table (`readelf -rW`),
classifying every relocation to each target as `R_386_PC32` (a direct
call/branch instruction -- what the existing direct-call-site sweeps
already look for) vs `R_386_32` (a data-pointer reference -- what a
`.rodata` vtable slot uses, and what a call-site grep on the calling
code can NEVER find, since the call site itself is just `call
*offset(%reg)` with no symbol).

**Method validated against a known-real positive control** before
trusting the negative results: `CSTGProgramSlot::
ProcessPreviousSVDOnProgramChange`/`CSTGProgramModeProgramSlot::
ProcessPreviousSVDOnProgramChange` (already-confirmed real vtable slot
56, sec 10.153/batch 47) show up correctly as `R_386_32` hits in their
owning `.rel.rodata._ZTV*` COMDAT sections -- confirming the
address-in-relocated-data-vs-address-in-relocated-code distinction
correctly separates "called by name" from "reached only by vtable slot"
before drawing any conclusion from its absence.

**Result: all 10 named targets, and all 56 resolved stub targets checked in
total, have ZERO `R_386_32` relocations anywhere in the entire binary.**
Every single relocation to every one of them is `R_386_PC32` -- i.e. in
ground truth itself, none of these functions are ever reached through a
vtable slot at all; they are exclusively direct-called. Spot-checked
several call sites (`CSTGEngine::PostAudioTick()` for `RunMonitors`/
`MixPerformanceOutputs`, `CSTGGlobal::CompletePerformanceActivation()`
for `OnPerformanceActivate`, `ProcessOACmd` for `RescanPianoTypes`/
`AfterProcess`, `setup_stg_daemons` for `HDRMiniModel::Initialize`) and
confirmed this project's own reconstructed code already calls each one
from the exact same already-real caller, matching ground truth exactly
-- these are the SAME callers the prior direct-call-site sweeps already
found and correctly deferred, not new information.

**Four targets DID show up with real `R_386_32` (vtable-slot) hits, and
each was individually investigated to rule out the `CClientCommServer`
pattern (an already-real, already-populated, already-dispatched-through
vtable reaching an unreconstructed callee):**

1. **`CSTGParamsOwner::ValidateParamChange`/`UseDefaults`** -- 303/22
   `R_386_32` hits respectively, one per derived class across the ENTIRE
   effects/voice-model/params-owner hierarchy (~300 classes never
   override either method). Both ARE genuinely, currently dispatched
   through in this project's own real code -- `CSTGGlobal::
   ValidateParamChange()`/`UseDefaults()` (global.cpp) each do a real
   `reinterpret_cast<CSTGParamsOwner*>(this)->ValidateParamChange(...)`/
   `->UseDefaults()`, a genuine C++ virtual call (not a hand-crafted raw
   vtable-slot dispatch) that the compiler resolves through
   `_ZTV10CSTGGlobal`'s own (correctly, compiler-generated, non-
   placeholder) vtable slot straight back to the already-real stub
   bodies in `bar2_stubs.cpp`. This is not new: the reconstruction
   already models it faithfully via real C++ inheritance, already
   reaches the already-real stub, and was already the documented
   caller. The 303/22-way fan-out from the wider effects hierarchy adds
   no new caller -- every one of those derived classes' own vtables in
   THIS project (`_ZTV11CSTGProgram`/`_ZTV9CSTGCombi`/
   `_ZTV17CSTGCommonStepSeq`/etc.) are confirmed zero-filled placeholders
   that nothing currently dispatches through (re-verified fresh, not
   just trusting the batch-43/44 note -- `SetAudioInSolo()`'s own
   sec-57 comment shows this project DOES already check for exactly
   this kind of live dispatch when it matters, e.g. its own real
   `CSTGPerformance` vtable-slot-27 dispatch, confirmed-dead only
   because ground truth's own slot resolves to `__cxa_pure_virtual`).
2. **`CKorgPreloadFile::Load()`** -- 2 `R_386_32` hits
   (`_ZTV16CKorgPreloadFile`/`_ZTV17CKorgProgBankFile`, both real
   in this project, batch 54). Not new: `Load()` only has ONE real
   caller anywhere (`CSTGGlobal::InitializePerformances()`,
   `init_performances.cpp:121`, `if (!file.Load())` on a concrete,
   stack-local `CKorgProgBankFile`) and that caller is already
   reconstructed, already known, already deliberately deferring this
   exact callee -- the vtable slot existing (because `~CKorgPreloadFile()`
   is virtual, dragging `Load()` into the same vtable) is irrelevant
   since the one real call site binds to the concrete type regardless.
3. **`CSTGExtMIDIClockSync::ProcessClock()`** -- 1 `R_386_32` hit,
   already flagged in this file's own bar2_stubs.cpp comment
   ("nothing in this project dispatches through this vtable yet") --
   re-verified true, no new caller found.

**No promotions made. Zero code changes this session** (pure
cross-check, matching the discipline of `stub_caller_reachability_
sweep_2026-07-25.md`'s own prior audit pass). This is a genuine,
cross-validated negative result: two independent techniques (direct-
call-site grep, and now ground-truth vtable-relocation analysis) agree
that OA.ko's remaining structural/hardware-integration stub backlog
holds no `CClientCommServer`-style hidden reachability gap. Everything
still deferred is deferred for the reasons already on record (audio-DSP
scope, filesystem I/O scope, or genuinely larger new-class
infrastructure than a single pass affords) -- not because anyone missed
a caller.

No real-HW test applies to this entry -- it is a pure static-analysis
cross-check with no behavioral claim to verify on hardware.

---

## CSTGMidiPortManager::~CSTGMidiPortManager() — physical MIDI port teardown at module unload (batch 57)

Reconstructed (`src/engine/midi_port_manager.cpp`, `.text+0xf5280`, 264
bytes), genuinely reached from `CSTGEngine::~CSTGEngine()`, not dead code.
For each of 4 in-port/4 out-port static slots, IN-PORT THEN OUT-PORT
(confirmed real interleave order, not "all in-ports then all out-ports"):
if the slot is non-NULL AND its "active" flag bit (bit1/`0x2`) is set,
dispatches through vtable slot 2 on that port object (presumably each
port's own virtual destructor -- not independently confirmed beyond the
call shape). This batch is this project's first confirmed real use of
`CSTGMidiOutPort`'s own `+0x5` byte, previously modeled as unconfirmed
padding, now named `flags` (matching `CSTGMidiInPort`'s existing `+0x26`
field of the same name/purpose).

Uncertain: this reconstruction has never exercised this destructor against
a port that is ACTIVELY mid-transmission on real hardware (e.g. `OA.ko`
unloaded/reloaded, or `CSTGEngine` torn down, while a physical MIDI cable
is actively streaming data through `CSTGMidiOutPortSerial`/
`CSTGMidiInPortSerial`) -- only host-KAT-verified with synthetic port
objects. Whether the real vtable-slot-2 "destructor" call ever needs to
flush/drain an in-flight UART transmit buffer, or whether it is safe to
tear down unconditionally the instant `flags & 0x2`, is not confirmed from
static analysis alone.

Real-HW test that would help: with a real serial MIDI cable actively
sending, trigger an OA.ko module unload/reload cycle and confirm no
truncated/garbled MIDI bytes appear on the wire, and that a subsequent
reload's re-`Activate()` cleanly re-establishes the port with no leftover
state from the torn-down instance.

---

## CSTGControllerInfo::SendUnsolicitedUIParam — real mode-0 divergence + raw vtable slot 22/23 dispatch on a not-fully-typed object (batch 60)

Reconstructed (`src/engine/controller_info_send_unsolicited_ui_param.cpp`,
`.text+0x945d0`, 516B), a genuine live call chain (`CSTGControllerRTData::
OnExtModePlayMuteSwitchAssignChange`/`OnExtModeSelectSwitchAssignChange`
already call it for real) that sends real messages toward the front-panel/
remote UI via the already-real `PushUnsolicitedMessage()`.

Uncertain:
- On the "structured" (`CSTGGlobal::sInstance->fieldAt(0x29cc4dc)`
  nonzero) path, this function's own mode-dispatch formula collapses REAL
  ground-truth modes 0 AND 1 onto `ResolveCurrentPerformance()`'s mode-1
  formula, and ground truth's own mode-0 branch is confirmed (by
  instruction-level trace, not assumption) simply never reached by this
  function -- meaning if `CSTGGlobal`'s mode field is ever actually `0`
  when this exact call path fires on real hardware, this reconstruction's
  behavior has no ground-truth divergence to worry about (mode 0 is
  provably unreachable here), but this has not been cross-checked against
  what real front-panel state actually drives that mode field to during
  normal play.
- The resolved pointer plus a `CSTGProgramSlot`-stride (`0xe8`) index
  locates a NOT independently-typed sub-object, and two RAW vtable
  dispatches follow (slots 22/23) whose own real callees are not
  identified anywhere in this project -- matching the existing
  `CSTGEffectRackVars::UpdateDModRoutings` "raw vtable-slot call on an
  opaque object" idiom, but meaning this function's own outgoing UI
  message payload for the "structured" path depends on two entirely
  unverified real computations.

Real-HW test that would help: trigger `OnExtModePlayMuteSwitchAssignChange`/
`OnExtModeSelectSwitchAssignChange` (toggle an ext-mode play/mute or select
switch assignment from the front panel) while the current mode is each of
the real `+0x684` values in turn, and compare the resulting UI-visible
state/traffic against this reconstruction's per-mode message-field table
(header comment in the .cpp) to confirm both the mode-0-unreachable claim
and the two opaque vtable-slot results.

---

## CSTGAudioInputMixerBase — vtable slot 3's real target unidentified, safe zero-stub installed (batch 58)

> **SUPERSEDED 2026-07-27 (commits `2c539fb`, then later the same day the `g_audioInputMixerVtable` static-init fix) — do not treat this entry as describing the current implementation.** Three things changed since this entry was written: (1) the ctor's vtable-pointer install was found to be a real bug, not working reconstruction — it stored the literal integer `8` (a lost `R_386_32` relocation) instead of the real `&vtable+8`, which is what actually caused a live kernel NULL-deref Oops in `SetSendBuses()` on real insmod (fixed, `2c539fb`); (2) slot 3's real target — `CSTGAudioInputMixer::GetOutputBus(int)` — has since been identified and reconstructed for real (indexing a newly-discovered `CSTGAudioBusManager::sSynthesisThreadBusSets`, 960 slots), replacing the `return 0` placeholder described below; (3) a separate, deeper bug found the same day (the vtable pointer reverting to NULL moments later) was root-caused as a VM/QEMU-only PCI-hole memory-map issue (worked around via a `kronos_vm` boot-config change, no source touched), which in turn exposed a FOURTH bug — `g_audioInputMixerVtable`'s `ShouldMute`/`GetOutputBus` slots relied on a C++ static initializer that never runs in a Linux kernel module — now fixed via plain free-function trampolines. **A live boot now reaches a clean, oops-free `init_module()` return (`OA_DEBUG_MARKER 17`, `OA: init_module succeeded`), confirmed 2026-07-27.** See `PROJECT_BRAIN/status.md`'s latest kronosology-section entries and `kronosology/.claude/agent-memory/re-decompiler/oa_audioinputmixer_ctor_never_runs_fixed_2026-07-27.md` for the current, authoritative state. Kept below verbatim for history.

Reconstructed (`src/engine/audio_input_mixer.cpp`): the real ctor installs
a real vtable pointer and the real `SetFXCtrlBus`/`SetHDRBus`/
`SetSendBuses` methods all genuinely dispatch a RAW indirect call through
this object's own vtable slot 3 on real, reachable call paths
(`CSTGAudioInput::UpdateFXControlBus`/`UpdateHDRBus`, once a performance
activates) -- unlike this project's more common "vtable slot never
dispatched by anything real" deferral pattern, this slot's target is
provably exercised in ground truth. Its real callee is not identified
anywhere in this project, so a safe placeholder (`return 0`) is installed
instead of the usual zero-filled (crash-on-call) table.

Uncertain: every audio-input send-bus/FX-control-bus/HDR-bus value this
reconstruction currently computes via slot 3 is unconditionally `0` --
if the real slot-3 target on actual hardware computes a genuine gain,
level, or bus-routing value (plausible, given the surrounding functions'
own names), this reconstruction's audio-input bus routing would read as
"always zero" rather than whatever the real computation produces. This is
structural bus-routing plumbing, not DSP coefficient math, so arguably
in scope per this project's own policy, but was not chased further this
batch.

Real-HW test that would help: on a real Kronos, route an audio input
through the FX-control-bus or HDR-bus with a known, non-trivial setting
and confirm (via a level meter or a captured recording) that the routed
signal reflects a real, non-zero bus value -- if this reconstruction were
ever deployed to real hardware, this would immediately reveal the "always
0" placeholder as wrong, distinguishing it from the other deferred-vtable
entries in this log where the real slot is confirmed unreachable/dead.

## Broader vtable-write sweep, commit `8e12ab1` — 6 more instances found and fixed (2026-07-27)

Follow-up task to `63f099c`/`13ef727`'s "ctor omits/mis-writes the real
vtable pointer" bug class (4 confirmed instances at the time: the
`CSTGAudioInputMixer` static-init fix, `CSTGMidiOutPortKorgUsb`, and the
`CSTGMidiInPort`/`CSTGMidiOutPort` base ctors). This pass swept every
other reconstructed class with its own real vtable — front-panel/
calibration handler classes, `CSTGControllerInfo`/`CSTGAudioInput`, and
every `_vtablePtr`/`virtual` declaration in the codebase — checking each
one's real ctor disassembly (`objdump -dr -M intel` against
`/home/share/Decomp/OA.ko_Decomp/OA.ko`) against the reconstructed C++
ctor. Found and fixed 6 more real instances, in two distinct sub-variants.

**New variant (2 instances): a real C++ `virtual` destructor combined
with a leftover hand-rolled `_vtablePtr` field — a genuine object-layout
bug, not just a wrong value.** `CStartupFile` (`oa_setup_global_resources.h`)
and `CKorgPreloadFile` (`oa_global.h`) both declared `virtual ~Dtor()`
purely to make GCC auto-emit the `_ZTVxxx` vtable data symbol as a "key
function" side effect, while ALSO keeping this project's usual explicit
`_vtablePtr` field (used everywhere else for classes that install but
never C++-dispatch through their vtable). Declaring the dtor virtual made
the class genuinely polymorphic, so GCC inserted its OWN hidden
compiler-managed vtable pointer ahead of the hand-declared field, silently
shifting every subsequent byte offset by one pointer-width. Confirmed via
a throwaway `-m32` `sizeof`/`offsetof` reproduction (`sizeof(CStartupFile)`
came back 12, not the ground-truth-confirmed 8) and then against the
actual compiled `OA.ko`. For `CStartupFile` this inflated
`sizeof(CCostProfile)` by 4 bytes past the FIXED-size
`::operator new(0x12a0)` allocation `setup_global_resources.cpp`
placement-constructs it into — **a genuine 4-byte kernel heap buffer
overflow on every real module load.** `CKorgPreloadFile` has the same
layout bug but is currently harmless at runtime (a plain stack local,
accessed only via real C++ member syntax, no fixed-size external
allocation). Fixed both by dropping `virtual` and hand-declaring the
`_ZTVxxx` arrays as real, correctly-sized (verified via `readelf -sW`)
byte arrays instead — restores the single-vtable-pointer-at-+0x0 layout
ground truth actually has. Re-verified post-fix: `sizeof(CStartupFile)==8`,
`sizeof(CCostProfile)==0x12a0`, `sizeof(CKorgPreloadFile)==8`,
`sizeof(CKorgProgBankFile)==12`, all exactly matching ground truth, and
the actual compiled `OA.ko`'s own ctor disassembly now matches ground
truth's byte-for-byte.

**Established variant (4 instances): the ctor's vtable-pointer write is
simply missing or a bare literal `0`/`nullptr` instead of the real
`&_ZTVxxx+8` value.** `CSTGAudioInput::CSTGAudioInput()`
(`global.cpp`) never wrote `_vtablePtr` at all — left at whatever bytes
the enclosing `CSTGProgram`/`CSTGCombi` placement-new target already
held, where ground truth's own `.text+0xc9ea0` does
`mov DWORD PTR [eax],0x8` + `R_386_32 _ZTV14CSTGAudioInput`.
`CSTGCalibrationMsgHandler`/`CSTGControlMsgHandler`/
`CSTGFrontPanelMsgHandler` (all three `*MsgHandler` dispatch-table
installers) each wrote a bare literal `0` where ground truth's own ctors
(`.text+0xde910`/`0xe8550`/`0xe9f80`) write real, non-null
`&_ZTVxxx+8` values — this project's own "install vs dispatch" rule
means nothing reads the value back through a vtable slot, NOT that the
field itself should be left null; the header comments describing these
as "install-only placeholder" were correct about the dispatch behavior
but the ctor bodies had drifted to installing the wrong value. Fixed all
four the same way as the existing convention: declared correctly-sized
(confirmed via `readelf -sW`), zero-filled `_ZTVxxx` placeholder arrays
and wrote the real `&_ZTVxxx+8` value in each ctor.

All 6 fixes verified byte-for-byte against the actual compiled `OA.ko`'s
own disassembly (not just ground truth) post-rebuild. Host `verify/`
suite green (124/124, one pre-existing test updated to assert the now-
correct non-null value instead of the old, incorrect null expectation).
Clean `make ko-clean && make ko KDIR=/home/build/linux-kronos` rebuild.
Live-booted on a disposable `kronosvm` instance (fresh copy of the
known-good `full_integration_test_20260727` disk image, freshly-built
`OA.ko` injected via `guestfish upload`, MD5-verified): reached
`OA_DEBUG_MARKER 17`/`OA: init_module succeeded`/`[loadoa] OA.ko: LOADED
OK`, Eva alive at 8s, zero `Oops`/`BUG:`/`panic` (one benign
false-positive grep hit on the pre-existing "MP-BIOS bug: 8254 timer"
printk). VM instance torn down cleanly. Full derivation in
`kronosology/.claude/agent-memory/re-decompiler/`
(`oa_broader_vtable_sweep_6_more_instances_2026-07-27.md`).

---

## CKGSeqBackupCommonParam / CKGSeqBackupModuleParam — Karma sequencer param-backup cluster, 201 methods (batch, 2026-07-28)

Two genuinely new classes (first appearance in this project), discovered
as a dense, previously-untouched cluster while surveying pending manifest
entries by class: `CKGSeqBackupCommonParam` (72 real methods,
`.text+0x3d1200`..`.text+0x3d2070`) and `CKGSeqBackupModuleParam` (131
real methods, `.text+0x3d2070`..`.text+0x3d3830`), laid out back-to-back
in the real binary. Both classes read one live/default KARMA-perf record
field into a scratch `m_value` slot per call -- see
`include/oa_karma_seq_backup.h` for the full struct-layout derivation.

201 of the 203 real methods (both ctors, both `GetKarmaPerf{Common,
Module}ForSeqBackup()` helpers, and 197 of 197 `Set*` accessors) are
reconstructed and KAT-verified (`verify/test_ckg_seq_backup.cpp`, 204
checks, all passing) via a scripted instruction-pattern decoder run
against the real disassembly (not hand-transcribed one at a time).

Deliberately deferred, same "don't declare what isn't verified yet"
convention as CSTGLFO's `ProcessSubRate`:

1. **`GetValue(int paramIndex, int subIndex, long *out)`** on both
   classes (CommonParam: `.text+0x3d1240`, 1555 bytes, 69-case jump
   table; ModuleParam: `.text+0x3d20d0`, 2893 bytes, 128-case table).
   Confirmed to be a real, fully self-contained function whose case
   bodies duplicate (not call) the same field offset/width/shift/mask
   logic as the correspondingly-named `Set*` method -- reconstructing it
   faithfully needs the exact case-index -> field mapping pinned down,
   which is real additional verification work (case order need not equal
   `Set*` declaration order, and a wrong mapping would be a silent
   behavioural bug, not a compile error) beyond what this pass's KAT
   covers.
2. **The real caller** that iterates these backup objects (the actual
   Karma step-record/undo path) was not traced -- out of scope for this
   pass, which only reconstructs the two classes' own methods.
3. **`CKGBankManager::GetSeqKarmaPerfCommon`/`GetSeqKarmaPerfModule`/
   `GetSeqDefaultKarmaPerfCommon`** and **`CSPREngine`**'s broader
   surface are declared (real mangled names) but not defined --
   `CKGBankManager`'s own body was already out of scope before this
   batch (see oa_engine_init.h), and `CSPREngine` is a brand-new opaque
   stand-in discovered by this batch, same convention.

Real-HW test that would help: none identified -- this is pure control-
plane bookkeeping (no audio/DSP output, no front-panel I/O) with no
obvious externally-observable effect to probe against a live unit.

**Follow-up (2026-07-28): `GetValue()` case-index mapping investigated,
NOT confirmed -- still deferred.** Extracted the real 69-entry jump table
(`.rodata+0xacb94`, one `R_386_32`-against-`.text`-via-`.rel.rodata`
relocation per 4-byte slot -- readelf's REL-not-RELA format means the
target address is the raw stored bytes, not the `Sym.Value` column) and
disassembled all 69 case bodies. Cases 0/1 DO match `SetTempo`/`SetTimeSig`
in that exact order (word@0, byte@3) -- but case 2 is byte@2 `&0x7` (that's
`SetModuleControl`'s shape), case 3 is byte@2 `>>6 &1` (`SetLatch`'s
shape), case 4 is byte@2 `>>7` no mask (`SetOnOff`'s shape) -- i.e.
`SetPadMode` (byte@2 `>>5 &1`) is genuinely SKIPPED over in this run of
cases, proving `GetValue()`'s case order is NOT simply the Set* list order
used in this file / the KAT / the header's own narrative ordering. Real
mapping is therefore per-offset identity, not position -- would need a
full (offset, index-scale, width, shift, mask) tuple match between all 69
(Common) + 128 (Module) case bodies and the already-written Set* bodies,
same rigor as the original decoder but as a two-sided matcher instead of
a one-sided transcriber. Not attempted this pass (real risk of a wrong
mapping being a silent behavioral bug, per this entry's own original
caution) -- left for a dedicated future batch. The jump-table extraction
method above (readelf -x .rodata + .rel.rodata cross-reference for REL-type
relocations) is reusable as-is for that batch.

## CSTGString value-getter family — 105 methods (batch, 2026-07-28)

`CSTGString` (STG physical-modeled-string patch component) had zero prior
reconstruction. Surveying the pending manifest for other dense Set*/Get*
clusters (same technique as the CKGSeqBackup batch above) found a MUCH
larger structural pattern spanning the entire STG synth engine: ~180
classes share one generic `STGConvertedParam &Get*(CSTGPatchMessageContext
&)` "value getter" convention (already partially known from
`CSTGADSRBase`'s own 20 hand-reconstructed methods, `src/engine/
adsr_base.cpp`) -- read one field into the shared static
`CSTGParamsOwner::sValueGetterTemp` and return its address. Roughly 2300
pending methods across that whole family live in one contiguous real
`.text` range (`~0x5a0000`-`0x5c0000`), each as its own weak/COMDAT
`.text._ZN...` section (the accessors are emitted as `inline`-linkage
per-class instantiations, unlike ordinary member functions which land in
the plain merged `.text`).

Picked `CSTGString` (116 Get* candidates, largest cleanly-scoped single
class in that region) as this batch's target. Built a fresh scripted
instruction-pattern decoder (same methodology as CKGSeqBackup's, see that
entry above) recognizing: `mov eax,[eax+K]` / `movsx`/`movzx eax,BYTE/WORD
[eax+K]` direct field reads; `mov edx,[edx+0x4]` + `lea edx,[edx+edx*4]`
(stride 5) or `shl edx,N` (stride 2^N) for the Pickup*/MixerPickup*
sub-family's per-call dynamic index (read from `ctx`'s own +0x4 field,
NOT `this`'s); `shr al,N` + `and eax,MASK` packed boolean bitfields; and
the fixed `mov ds:0x0,eax` / optional `mov ds:0x18,eax` / `mov eax,0`
epilogue. 105 of 107 real weak-symbol candidates in the class's address
range (`.text+0x5b0e70`..`.text+0x5b1ae0`) parsed cleanly with zero
unhandled instruction shapes -- see `include/oa_stg_string.h` and
`src/engine/stg_string_valuegetters.cpp` for the full derivation and
field-shape summary. `verify/test_stg_string_valuegetters.cpp`
independently re-derives all 169 expected values (105 `.value` +
64 `.displayValue`) via a separate Python evaluator over the same parsed
facts, all passing; the real Kbuild build (`make ko KDIR=/home/build/
linux-kronos`) links all 105 symbols into `OA.ko` cleanly.

3 genuine outliers found and deliberately excluded (documented in the
header, not silently dropped):
1. **`GetSubComponent(unsigned short)`** -- confirmed real `__thiscall`
   (not `__regparm3`), branchy, returns a sub-object pointer. A
   completely different mechanism, not a value-getter at all.
2. **`GetNoiseSaturation`** (in-range, `.text+0x5b0e40`) -- a real
   `fyl2x`-based log2/dB-style conversion, not a plain field copy.
3. **`GetPluckDelay`/`GetPluckDelayAMSIntensity`** (`.text+0x187b70`/
   `.text+0x187bd0`, NOT in the weak/COMDAT cluster -- ordinary
   global-linkage symbols in the merged plain `.text`) -- genuine
   audio-DSP: converts a delay parameter to a sample count via
   `CSTGAudioBusManager`'s live sample rate (`fmul`/`fistp` against a
   runtime float). Out of scope per this project's established
   DSP-fidelity policy.

Also left pending (different mechanism, not part of this family):
`GetId`/`GetName`/`GetNumParams`/`GetParamDescriptors`/
`GetMessageHandlers`/`GetValueGetters`/`GetNumSubComponents` -- the
generic `CSTGParamsOwner` reflection-API virtual-slot overrides.

Real-HW test that would help: none identified -- same as CKGSeqBackup
above, pure parameter-reflection plumbing with no direct front-panel/
audio observable. The much larger ~2300-method "value getter" family this
batch discovered (spanning `CSTGOrganModelPatch`, `CSTGMS20`,
`CSTGAnalog4PoleBase`, `CSTGPolysix`, `CSTGProgram`, `CPianoOsc`, and
~170 more classes) is a strong candidate for repeating this exact
technique class-by-class in future batches.

## CSTGOrganModelPatch + CSTGMS20 value-getter families -- 191 methods (batch, 2026-07-28)

Second batch against the STG value-getter family discovered in the
CSTGString pilot above. Picked the next two largest cleanly-scoped
classes from that pilot's own priority list: `CSTGOrganModelPatch`
(Hammond-style tonewheel-organ patch component, 101 methods) and
`CSTGMS20` (Korg MS-20-style analog dual-VCO/VCF/ESP patch component, 90
methods). Reused and extended the same scripted instruction-pattern
decoder rather than writing a fresh one per class.

`CSTGOrganModelPatch` turned out to use a SIMPLER dialect than
CSTGString's: `this` stays in eax throughout, with zero ctx-dynamic-index
sub-family methods at all (every AMSSource/AMSIntensity/AMSMode sibling
reads a fixed per-field offset, confirmed empirically, not assumed from
the name pattern). One field-shape not seen in the CSTGString pilot: a
boolean NOT (`movzx eax,BYTE[eax+K]` + `xor eax,0x1` + `movzx eax,al`,
single .value-only write) on `GetPercLevelSwitch`. 101 of 103 real
weak-symbol candidates parsed cleanly; 2 genuine outliers correctly
excluded: `GetRotaryHornMicDistance`/`GetRotaryRotorMicDistance` both
compute a real `1.0f - field` via x87 (`fld1`; `fsub DWORD PTR
[eax+K]`; `fst`/`fstp`) -- same rationale as CSTGString's
`GetNoiseSaturation` outlier, a numeric transform excluded from a batch
meant to be mechanically decoded rather than hand-verified per method.
Also confirmed the same "32-bit does not always imply dual-write" quirk
CSTGString established: `GetVCType`, `GetRotaryHornStopPhase`, and
`GetRotaryRotorStopPhase` are discrete/enum 32-bit selector fields that
write `.value` only, derived empirically (instruction presence), not
assumed from width.

`CSTGMS20` used a MIXED dialect: most methods match
CSTGOrganModelPatch's simple eax-based shape, but this class also has a
real ctx-dynamic-index sub-family (the Standard*/Mixer* AMSSource/
AMSIntensity sibling group), which required extending the decoder with
two new shapes not seen in either prior batch: (1) the usual stride-5
`lea edx,[edx+edx*4]` premultiply followed by a field load that ALSO
carries its own SIB scale factor in the addressing mode itself
(`[eax+edx*2+K]`), giving an effective per-index stride of 10 rather
than CSTGString's plain 5 or 32; (2) a bare, unscaled ctx-index load
with no `lea` premultiply at all (`[eax+edx*1+K]`), used by
`GetInputJack` alone. All 90 real weak-symbol candidates in this class's
address range parsed cleanly -- zero outliers, a first for this family.

Both classes' KATs (`verify/test_stg_organ_model_patch_valuegetters.cpp`,
191 checks between them across 90+56 dual-write pairs and 55+34
single-write cases) independently re-derive every expected value from
the SAME parsed (offset, ctx-index*stride, width, signed, invert, dual)
facts via a separate Python evaluator, not by re-using the C renderer's
own output strings -- same independent-oracle discipline as CSTGString's
batch. `make verify` stays green (0 FAIL lines across the whole suite);
`make ko-clean && make ko KDIR=/home/build/linux-kronos` links all 191
new symbols into `OA.ko` cleanly (`nm OA.ko` confirms 101
`_ZN19CSTGOrganModelPatch...` + 90 `_ZN8CSTGMS20...` symbols present).

One real tooling gotcha found and fixed, distinct from the previously
documented "`*/` inside a comment silently ends the block comment"
issue: `manifest/gen_oa_manifest.py`'s NAME-heuristic regex
(`DEF_RE`) does not balance parentheses in its captured parameter-list
group, so an UNBALANCED-looking `(` triggered by ordinary English prose
in a header comment -- e.g. a contraction like "didn't (Foo" or even a
plain "pending (see ...)" aside with no semicolon anywhere between it
and the next real function -- can cause the regex to swallow everything
up to and including the FIRST real function definition's opening brace
as bogus "parameter list" text, hiding that one function from the
reconstructed count entirely (silently, no error) even though the C++
compiles fine. Confirmed this cost exactly one method per file
(`CSTGOrganModelPatch::GetAmpGain`, `CSTGMS20::GetAnalog`) before the
header comments in both `.cpp` files were rewritten to avoid
parenthetical asides ahead of the first function -- verified after the
fix by running `DEF_RE` against both files directly and diffing the
captured name set against every declared method name. `.h` files are
immune to this specific failure mode (method declarations end in `;`,
which resets the regex's runaway match immediately), so this only needs
checking in `.cpp` files' leading comment block, not headers.

Real-HW test that would help: none identified, same rationale as
CSTGString and CKGSeqBackup above.

## STG value-getter family, batches 3-7 -- 513 methods across 12 classes (documentation catch-up, 2026-07-28)

Consolidated entry closing a gap flagged by batch 7's own agent memory:
this log had not been kept current with the STG value-getter family past
batch 2 (`CSTGOrganModelPatch`+`CSTGMS20` above), even though five more
batches shipped the same day. Facts below are transcribed from
`/home/share/PROJECT_BRAIN/status.md`'s own dated entries and
`.claude/agent-memory/re-decompiler/stg_value_getter_family.md`, not
re-derived. Manifest moved 1741 -> 2263/21,689 (8.03% -> 10.43%) across
these five batches, 513 methods total. Same ground truth binary
throughout (`/home/share/Decomp/OA.ko_Decomp/OA.ko`), same scripted
`objdump -dr` -> Python instruction-pattern decoder methodology
established in the CSTGString pilot, extended per-batch rather than
rewritten.

**Batch 3 (commit `e59300a`): `CSTGAnalog4PoleBase` (74/76) +
`CSTGPolysix` (71/71) + `CSTGAnalogSyncOsc` (63/65), 208 methods**,
manifest 1741 -> 1949. New decoder shapes: a ctx-dynamic-index field load
with an explicit ×4 SIB scale stacked on the usual stride-5 `lea`
premultiply (`CSTGPolysix`'s `ExtMod*Intensity`/`ExtModSource` group,
effective stride 20 -- third distinct SIB-scale value confirmed for the
sub-family after CSTGString's ×1 and CSTGMS20's ×2); a boolean
nonzero/zero integer test (`test reg,reg` + `setne`/`sete` + `movzx`)
included as a real mechanical shape, not excluded, since it's a
truth-value test rather than a numeric transform
(`CSTGAnalogSyncOsc::GetRingModModulatorSelect`/`GetRingModCarrierSelect`/
`GetSubOscAudioInModeSelect`). New signature-outlier check added and
immediately validated: filtering the weak-symbol candidate list to
mangled names ending `ER23CSTGPatchMessageContext` before decoding caught
`CSTGAnalog4PoleBase::GetSubComponent(unsigned short)` up front -- same
outlier class as CSTGString's own `GetSubComponent`, but weak/COMDAT
linkage this time, so the plain W/T check alone would have missed it.
4 outliers total: that `GetSubComponent`; `GetFilterALeakage`/
`GetFilterBLeakage` (real x87 ordered-equal-to-1.0 float compare,
`fucomip`-based); `GetNoiseSaturation` (`fyl2x`, same shape as
CSTGString's own); `GetNoiseCutoff` (SSE `sqrtss`, a new numeric-transform
outlier variant). `CSTGPolysix` was the family's first class with
genuinely zero outliers across its full 71-candidate set.

**Batch 4 (commit `515830e`): `CPianoOsc` (46/53) + `CSTGEPModelPatch`
(42/42), 88 methods**, manifest 1949 -> 2037. `CSTGProgram` (65 pending,
next by size on the prior priority list) was spot-checked first and
confirmed already heavily modeled (real ctor, `Initialize`/`Copy`,
vtable, ~97 references in `oa_global.h`) -- same already-modeled
situation as `CSTGProgramSlot`, correctly skipped. `CPianoOsc` is the
first non-`CSTG`-prefixed class shown to fully participate in the
convention (same `sValueGetterTemp` sink, same signature, same
weak/COMDAT sections). New ctx-index shape: its Level/MultisampleNum/
BankType/BottomVelocity group uses effective stride 25 via TWO chained
stride-5 `lea` premultiplies rather than a single `lea` plus an extra SIB
scale -- the first stride variant in the family not built from a single
premultiply-plus-scale. New outlier CLASS (not just a new outlier
instance): `CPianoOsc`'s 7 `Get*BankSelect` methods compute a
ctx-indexed sub-object pointer but then make a real `call` into the
still-unreconstructed `CSTGMultisampleBankUUIDAndStereoFlag::
GetBankIdAndStereoFlag` (348 bytes, itself calling the also-unreconstructed
`FindBankUUID`) -- the first "delegates to another undecoded real member
function" outlier, distinct from every prior outlier's own-data DSP math.
`CSTGEPModelPatch`: simplest dialect yet, 42/42 candidates, zero
outliers, zero ctx-index methods.

**Batch 5 (commit `0863921`): `CSTGOrganOsc` (13/36) + `CSTGVPMOsc`
(44/44) + `CSTGMS20ModelPatch` (19/19), 76 methods**, manifest 2037 ->
2113. `CSTGOrganOsc` reconfirmed "T linkage = different mechanism" even
when a mangled signature superficially matches: 23 of its 36 pending
symbols end in the same `ER23CSTGPatchMessageContext` suffix but are
global ('T') linkage; spot-checking `GetLowerNoteCount`'s own
disassembly (rather than trusting linkage alone) showed a plain per-voice
runtime note-count read straight into eax, zero `sValueGetterTemp` write
-- real per-voice state, not a static patch-value accessor. New field
shape: shift-then-mask bitfield extraction, `movzx` + `shr al,N` +
`and eax,1` (N > 0), extending the existing mask-only (no-shift) bitfield
shape -- confirmed on `CSTGVPMOsc` (4 independent booleans packed in byte
0x1f) and `CSTGMS20ModelPatch` (2 booleans at offset 0x6f7). Both
`CSTGVPMOsc` and `CSTGMS20ModelPatch` are zero-ctx-index dialects;
`CSTGVPMOsc` has 1 pre-excluded outlier (`GetSubComponent`, same
sub-object-accessor shape as always), `CSTGMS20ModelPatch` has zero.

**Batch 6 (commit `8c97b38`): `CSTGPolysixModelPatch` (48/48) +
`CWaveMotionOsc` (23/23) + `CSTGPianoModelPatch` (16/18, plus 2 real
accessor helpers), 89 methods**, manifest 2113 -> 2202.
`CSTGControllerInfo`/`CSTGVectorMotion` were also on the priority list by
raw pending-count but correctly SKIPPED -- both already have real
structs/ctors in `oa_global.h`/`program_ctor.cpp`, same already-modeled
precedent as `CSTGProgramSlot`/`CSTGProgram`. `CSTGPolysixModelPatch`'s
`GetArpeggiator*` group packs FOUR independent booleans into one byte at
`+0x4ac` (Enable/KeySync/MIDITempoSync/Latch), one more bit than any
prior class's bitfield shape. `CSTGPianoModelPatch` -- confirmed distinct
from `CPianoOsc`, the higher-level patch component that owns a
`CPianoOsc` -- is the batch's new-shape class: its 8-method
SustainPedalDown-/SustainPedalUp-prefixed ctx-indexed group derives its
base pointer from a virtual-dispatch call through `this`'s own vtable
(slots `0x170`/`0x174`), not `this` directly. Rather than treating the
whole group as an outlier, both vtable targets
(`AccessSustainPedalDownVelocityZones`/`AccessSustainPedalUpVelocityZones`)
were decompiled directly, found to be trivial constant-offset accessors
(`this+0x14`/`this+0x78`), and reconstructed as real member functions
called from the Get* bodies -- a new "virtual-call-mediated but
mechanically trivial once decompiled" shape, distinct from `CPianoOsc`'s
own BankSelect outlier where the delegate target is genuinely
non-trivial and stays unreconstructed. `this+0x78 - this+0x14 =
0x64 = 4 * stride(25)` cross-checked that each velocity-zone array holds
exactly 4 records. `GetSustainPedalDownMultisampleBank`/
`GetSustainPedalUpMultisampleBank` hit the same still-open
`GetBankIdAndStereoFlag` dependency as `CPianoOsc`'s outlier and were
excluded on the same grounds. New field-width variant: this class's own
ctx-index field is read as a BYTE (`movzx ebx, BYTE [ctx+0x4]`) rather
than the family's usual DWORD read, modeled via a new `CtxIndexByte`
helper alongside `CtxIndex`.

**Batch 7 (commit `22a5cf3`): `CSTGMultiFilter2Pole` (23/23) +
`CSTGMS20EG` (20/20) + `CSTGPolysixMG` (18/18), 61 methods**, manifest
2202 -> 2263. `CSPRSeqDataManager` (28 pending, next by size) was
checked first and confirmed NOT part of this family at all -- entirely
global ('T') linkage with a different, non-ctx signature
(`GetSongSize`/`GetTrackSize`-style sequencer data-area accessors) --
correctly skipped without writing any files. All three chosen classes
came back fully or near-fully clean, the third batch running with zero
excluded outliers. New ctx-index shape: `CSTGMultiFilter2Pole`'s
`GetLFOIntensity`/`GetLFOJSminusYIntensity` use a bare SIB-scaled
ctx-index load with NO `lea` premultiply at all (`mov edx,[edx+0x4]`
straight into `[eax+edx*4+K]`, effective stride 4) -- the first confirmed
case of a raw per-call index used directly as a plain array-of-dwords
subscript. New plain-field variant: `CSTGPolysixMG::GetMIDITempoSyncTimes`
is an UNSIGNED byte field (`movzx`, no shift/mask) rather than the
family's near-universal signed `movsx` byte read on a non-bitfield field.
`CSTGMS20EG`'s own naming was a false lead, not a new shape: its 4
EG-time parameters each carry a 5-method AMS sibling group whose names
suggest a second level of modulation nesting
(`AMSIntensityAMSSource`/`AMSIntensityAMSIntensity`), but direct
disassembly confirmed all 20 fields are plain fixed offsets off `this`
with zero extra indirection.

**Cross-batch tooling notes** (each logged in agent memory as it was
found, consolidated here): the DEF_RE parenthesis-swallowing gotcha
(batch 2's own finding) and the literal-`*/`-in-prose gotcha each
recurred in nearly every batch's new file headers, in progressively
subtler forms -- adjacent similarly-prefixed field names (`FilterA*/
FilterB*`, `Tine*/Reed*`), bare `Get*/Set*` prose with no adjacent-word
pairing needed, a `.h`-file instance whose runaway match reached a real
inline function definition's own `{` (batch 6's `CtxIndexByte`) and,
batch 7, a `.h`-file instance reaching a `struct Name {` opening brace,
plus a genuinely new DEF_RE trigger shape (a bare capitalized word
immediately followed by a parenthetical aside, e.g. `UNSIGNED (movzx,
...)`). Every instance was caught before any build attempt via the
2-check discipline that hardened over these batches: an exact `/*`/`*/`
open-close COUNT balance check, plus an exact DEF_RE captured-name-set
DIFF (not just a match-count check) against every declared method name,
run on both `.h` and `.cpp` files. All five batches independently
KAT-verified via a separate Python evaluator over the same parsed
(offset, ctx-index*stride, width, signed, dual-write) facts rather than
the C renderer's own output, and each closed with `make verify` green
(0 FAIL lines) plus a real `make ko-clean && make ko
KDIR=/home/build/linux-kronos` Kbuild build linking every new symbol into
`OA.ko` cleanly.

Real-HW test that would help: none identified across any of these five
batches, same rationale as every prior entry in this family -- pure
parameter-reflection plumbing with no direct front-panel/audio
observable.

## CSTGAMSMixerBase + CSTGStepSeq + CSTGPitchMod value-getter families -- 43 methods (batch 8, 2026-07-28)

Eighth batch against the STG value-getter family. Before picking classes,
resolved the `CKGModuleParamMsgHandler`/`CKGCommonParamMsgHandler`/
`CKGGlobalParamMsgHandler` question carried over unconfirmed across three
prior batches: `nm`-queried all three directly (130/71/27 pending Get*/Set*
symbols) and found every one is global ('T') linkage with a completely
different signature, `Set*(CKGModuleParamMsg const*)` and its two
siblings, zero matches against this family's own ctx-only mangled-suffix
filter -- confirmed NOT part of this family despite the similar naming,
closing out a question left open since batch 5. Also rejected `CMOSSAlgorithm`
(29 pending, a MOSS DSP-algorithm parameter-descriptor mechanism with
mixed calling conventions and extra arguments beyond ctx, not this
family's shape) and confirmed `CSTGDrumKitData`/`CSTGWaveSequence`/
`CSTGProgramModeDrumTrackSlot` are all already modeled elsewhere in the
project (a 17MB raw-blob table, a vtable-only stub, and a real
`CSTGProgramSlot` subclass respectively) -- correctly skipped.

Picked `CSTGAMSMixerBase` (STG AMS two-input mixer base, 19 candidates),
`CSTGStepSeq` (STG step-sequencer LFO component, 14 candidates, confirmed
distinct from the unrelated already-declared `CSTGStepSeqBase` stub), and
`CSTGPitchMod` (STG pitch-modulation component, 12 candidates, confirmed
distinct from 5 similarly-prefixed unrelated classes --
`CSTGPitchModBase`/`CSTGPitchModCommon`/`CSTGPitchModCommonPlusAMS`/
`CSTGPitchModOsc`/`CSTGPitchModOscBase`). All three's remaining pending Get
symbols beyond the chosen candidate sets were global ('T') linkage with a
different signature (a `CSTGVoice*` argument or a plain `(int,int)`
signature) and were excluded up front, not fed to the decoder.

`CSTGAMSMixerBase` is the simplest dialect yet -- zero ctx-index methods
despite the class's own "mixer" framing, every candidate a fixed-K field
off `this`. 2 outliers, both the familiar `fyl2x` log2-style transform
(`GetAttack`/`GetDecay`), same outlier class as `CSTGString`'s and
`CSTGAnalogSyncOsc`'s own `GetNoiseSaturation`. `CSTGStepSeq` and
`CSTGPitchMod` both came back zero-outlier. `CSTGStepSeq`'s per-step
Value/Duration/Times group uses the bare stride-1 SIB shape (ctx's own
`+0x4` field used directly, no `lea` premultiply, no extra SIB scale)
first confirmed on `CSTGMS20`'s own `GetInputJack`. `CSTGPitchMod` mixes
two ctx-index shapes in one class: bare stride-4 SIB with no `lea`
premultiply on `GetLFOAmount`/`GetJSYToLFOAmount` (same shape as
`CSTGMultiFilter2Pole`'s own LFO group), and the family's usual stride-5
`lea` premultiply on `GetLFOAMSSource`/`GetLFOAMSIntensity`.

Not a new field-shape but a real decoder improvement: the shared scripted
decoder's SIB-operand handling previously had two separate, narrower
cases and no direct bare-`ctxfield` case at all, meaning the bare
stride-1/stride-4 shapes documented in batches 5 and 7 had actually been
verified per-class by hand rather than mechanically decoded end to end.
Generalized this batch to one unified rule -- a `scaled_index` base
multiplies its own stride by the SIB scale, a bare `ctxfield` base uses
the SIB scale directly as the stride -- covering every stride variant
seen in the family so far (x1 bare, x1/x2/x4 lea-plus-scale, x4 bare, x5
lea) through a single code path.

Caught and fixed one genuinely new class of bug this batch, in the
independent KAT oracle rather than in the reconstructed source: the
Python evaluator's dword read initially only treated a width-4 field as
signed when the decoder's own captured `signed` flag said so -- but that
flag is always `False` for width-4 loads (a raw `mov eax,[...]` register
load has no separate signed/unsigned form), while the C renderer always
casts width-4 loads to plain `int` regardless of the flag, matching the
original instruction. First KAT run failed every dual-write field with
`got`/`want` differing by exactly 2**32, an immediately recognizable
signed/unsigned encoding mismatch rather than a real logic bug. Fixed by
making the evaluator treat every width-4 load as signed unconditionally,
matching the C renderer's convention rather than the decoder's own
(width-4-irrelevant) captured flag; re-ran clean. Both known `.cpp`
leading-comment gotchas -- a `"decoder (extended ..."` bare-word-before-
parenthetical DEF_RE trigger and an `"outlier(s)"` variant of the same --
were caught and fixed in all 3 new files via the standard 2-check
discipline (comment open/close-count balance, exact DEF_RE captured-name-
set diff) before any build attempt, same as every prior batch.

`make verify`: exit 0, 0 FAIL lines across the whole suite, all 3 new
KATs passing (43 checks). Real `make ko-clean && make ko
KDIR=/home/build/linux-kronos` Kbuild build: clean link, `OA.ko` produced
(488580 bytes), zero warnings or errors traceable to the 3 new files.
`DECOMPILE_ERRORS.md` stays empty -- no compile/link blocker hit.
`manifest/gen_oa_manifest.py` regenerated, OA.ko manifest
2263 -> 2306/21,689 (10.632%), delta exactly +43 (0 regressions, verified
by a full before/after reconstructed-name-set diff).

Real-HW test that would help: none identified, same rationale as every
prior entry in this family -- pure parameter-reflection plumbing with no
direct front-panel/audio observable.

## CSTGSimple2Pole + CSTGVPMModelPatch + CSTGVPMTG92Osc value-getter families -- 32 methods (batch 9, 2026-07-28)

Ninth batch in the STG value-getter family (see the `stg-value-getter-
family` agent memory entry for the full running derivation notes).
`CSTGSimple2Pole` (13/13), `CSTGVPMModelPatch` (10/10), and
`CSTGVPMTG92Osc` (9/9) all reconstructed clean -- zero outliers across
the whole batch, the third batch in a row with none excluded. All three
picked from the eighth batch's own backlog via the standard checklist:
word-boundary grep confirming each genuinely fresh, then an `nm`-derived
weak-linkage + `ER23CSTGPatchMessageContext`-suffix filter to get the
exact real candidate set before ever running the decoder.
`CSTGPCMModelPatch` (7 raw pending) was checked and correctly rejected
this batch -- both its real symbols are global ('T') linkage with extra
arguments beyond `ctx` (`GetMultisampleIds`, `SetupComponentOffsets`),
zero overlap with this family's convention.

`CSTGSimple2Pole` and `CSTGVPMTG92Osc` are both the family's familiar
simplest dialect -- every candidate a fixed-K field read directly off
`this`, zero ctx-dynamic-index methods. `CSTGSimple2Pole`'s own
`FreqAMS1IntensityAMSSource`/`FreqAMS1IntensityAMSIntensity` naming
implies a second level of modulation nesting but resolves to a plain
fixed offset once disassembled -- same lesson as `CSTGMS20EG`'s own
naming quirk from batch 7, never infer structure from a method's name
alone.

`CSTGVPMModelPatch` contributes a genuinely new ctx-index shape:
`GetInterMixerLink`/`GetOscMacroClass` load ctx's own `+0x4`
dynamic-index field not as an array/record base offset (the family's
usual `CtxIndex` convention) but as a variable SHIFT COUNT --
`mov ecx,[ctx+0x4]; movzx eax,BYTE[this+K]; sar eax,cl; and eax,0x1` --
selecting one single bit of an otherwise-fixed byte field per call.
Since the byte is loaded via `movzx` (top 24 bits always zero), the
`sar` is equivalent to a logical `shr`; modeled as a plain unsigned
right-shift via a new `CtxShift(ctx, off)` helper that applies an
explicit `& 0x1f` mask, matching x86's own 32-bit shift-count masking.
`GetAlgorithm` is also a new-ish plain-field variant: an UNSIGNED byte
(movzx, no shift/mask, not ctx-indexed) -- single-write only, same
"unsigned non-bitfield byte" case first confirmed on
`CSTGPolysixMG::GetMIDITempoSyncTimes` in batch 7, now seen a second
time.

Tooling: hit and fixed one fresh instance of the established
parenthesis-swallow `DEF_RE` gotcha in `oa_stg_vpm_model_patch.h`'s
first-draft header comment -- "the VPM (FM/ring-mod/waveshaper) engine's"
had the bare word `VPM` immediately before a space-then-`(`, which
matched `DEF_RE`'s trigger and swallowed the real `CtxIndex` function
definition into a bogus match. Caught via the standard exact-name-set
diff before ever attempting to build; fixed by removing every
bare-word-then-paren instance in that header's prose in favor of
em-dash-delimited clauses, the established convention.

`make verify`: exit 0, 0 FAIL lines across the whole suite, all 3 new
KATs passing. Real `make ko-clean && make ko
KDIR=/home/build/linux-kronos` Kbuild build: clean link, `OA.ko`
produced (493116 bytes), zero warnings or errors traceable to the 3 new
files. `DECOMPILE_ERRORS.md` stays empty -- no compile/link blocker
hit. `manifest/gen_oa_manifest.py` regenerated, OA.ko manifest
2306 -> 2338/21,689 (10.780%), delta exactly +32 (0 regressions,
verified by a full before/after reconstructed-name-set diff).

Real-HW test that would help: none identified, same rationale as every
prior entry in this family -- pure parameter-reflection plumbing with no
direct front-panel/audio observable.

## CSTGEG + CSTGPanOutputBase + CSTGPianoLPF value-getter families -- 28 methods (batch 10, 2026-07-28)

Tenth batch in the STG value-getter family (see the `stg-value-getter-
family` agent memory entry for the full running derivation notes).
`CSTGEG` (10/10), `CSTGPanOutputBase` (9/9), and `CSTGPianoLPF` (9/9)
all reconstructed clean -- zero outliers across the whole batch, the
third batch in a row with none excluded. All three picked from the
ninth batch's own backlog via the standard checklist: word-boundary
grep confirming each genuinely fresh, then an `nm`-derived weak-linkage
+ `ER23CSTGPatchMessageContext`-suffix filter to get the exact real
candidate set before ever running the decoder. That filter correctly
excluded, up front: `CSTGEG`'s 4 extra-arg overloads
(`GetAMSTimeModSource`/`GetAMSLevelModSource`/`GetAMSTimeModIntensity`/
`GetAMSLevelModIntensity`, all taking an explicit slot-index argument
instead of the family's plain ctx-only signature);
`CSTGPanOutputBase`'s `GetVoiceLevelEstimate(CSTGVoice const&)`, spot-
checked via direct disassembly and confirmed to be the same per-voice-
runtime-state false-positive shape as `CSTGOrganOsc`'s own precedent
from batch 5, plus its `SetMute` (global linkage, extra bool arg); and
`CSTGPianoLPF`'s `GetSubComponent(unsigned short)` (the familiar
sub-object-accessor outlier) plus its `SetupComponentOffsets` (global
linkage, extra args).

`CSTGPianoLPF` (the acoustic-piano voice model's lowpass-filter patch
component, distinct from `CPianoOsc`) is the family's familiar simplest
dialect -- every candidate a fixed-K field read directly off `this`,
zero ctx-dynamic-index methods. Its own
`GetFreqAMS1IntensityAMSSource`/`GetFreqAMS1IntensityAMSIntensity`
naming implies a second modulation level but resolves to a plain fixed
offset once disassembled -- same lesson as `CSTGMS20EG`'s and
`CSTGSimple2Pole`'s own earlier naming quirks. `CSTGEG` (general-purpose
envelope-generator patch component) mixes plain fixed-byte Source
fields with a real ctx-index sub-family on the matching Intensity
fields, using the by-now-established bare stride-4 SIB-scaled load with
no `lea` premultiply (first confirmed on `CSTGMultiFilter2Pole` in
batch 7) -- no new decoder shape needed.

`CSTGPanOutputBase` (STG pan/output-mixer patch component) contributes
a genuinely new field-shape: `GetPatchSolo` never dereferences `this`
at all. Its entire body stores the literal 0 straight into
`sValueGetterTemp.value` (single-write, no `.displayValue`) and returns
the pointer -- a hardcoded-constant getter, the first of its kind in
this family across 10 batches. Modeled directly as a plain literal
assignment with no field-read expression at all. The rest of the class
is the usual mix of fixed-K dword/signed-byte fields plus two
single-bit booleans sharing one byte at offset `0x21`
(`PanUseDrumkitSetting` bit 0, `PatchMute` bit 1) via the established
shift-then-mask shape.

Tooling: hit and fixed 2 fresh instances of the parenthesis-swallow
`DEF_RE` gotcha, both notable for being literal, accurate mentions of
real code rather than incidental prose -- `oa_stg_eg.h`'s own
"GetAMSTimeModSource(unsigned char)" (describing an excluded overload's
real signature) and `stg_eg_valuegetters.cpp`'s own "shape (no lea
premultiply)". Both hit the same `word\s*\(` trigger as every prior
instance; caught via the standard exact-name-set diff before ever
attempting to build, fixed by rewording to drop the parens entirely.

`make verify`: exit 0, 0 FAIL lines across the whole suite, all 3 new
KATs passing. Real `make ko-clean && make ko
KDIR=/home/build/linux-kronos` Kbuild build: clean link, `OA.ko`
produced (496956 bytes), zero warnings or errors traceable to the 3 new
files. `DECOMPILE_ERRORS.md` stays empty -- no compile/link blocker
hit. `manifest/gen_oa_manifest.py` regenerated, OA.ko manifest
2338 -> 2366/21,689 (10.909%), delta exactly +28 (0 regressions,
verified by a full before/after reconstructed-name-set diff).

Real-HW test that would help: none identified, same rationale as every
prior entry in this family -- pure parameter-reflection plumbing with no
direct front-panel/audio observable.

## CSTGAmp + CSTG3BandEQBase + CSTGEGBase value-getter families -- 18 methods (batch 11, 2026-07-28)

Eleventh batch in the STG value-getter family (see the `stg-value-getter-
family` agent memory entry for the full running derivation notes).
`CSTGAmp` (7/10), `CSTG3BandEQBase` (6/6), and `CSTGEGBase` (5/19) all
reconstructed clean -- zero outliers among the real ctx-only candidates,
the fourth batch in a row with none excluded. All three picked from the
tenth batch's own priority list via the standard checklist: word-boundary
grep confirming each genuinely fresh, then an `nm`-derived weak-linkage +
`ER23CSTGPatchMessageContext`-suffix filter to get the exact real
candidate set before ever running the decoder.

`CSTGEGBase` was this batch's headline verification: flagged unconfirmed
across two prior batches' own notes as "19 raw pending but only 5
weak-ctx-only." Directly queried via `nm` and confirmed the other 14 are
a genuinely different mechanism entirely -- ten global-linkage per-voice
state-machine transition helpers (attack, decay, sustain, release, hold,
slope, free, quick-release, plus a generic normal-state dispatcher, each
taking a `STGEGSubRateParamsSlice*` and a `CSTGVoice*`) and a filter-setup
helper with the same slice-pointer shape, three extra-arg setters (EG
type, an explicit control-value setter, a piano half-damper mode flag),
and one two-int accumulator query -- zero overlap with this family's
ctx-only convention, same "T linkage beats a superficially matching
signature" outcome as `CSTGOrganOsc`'s own per-voice-runtime-state false
positives from batch 5. Also confirmed `CSTGEGBase` is DISTINCT from the
already-modeled `CSTGEG` despite the similar name, same
`CSTGPolysix`/`CSTGPolysixModel` precedent.

`CSTGAmp` (STG amplifier patch component) mixes plain fixed-K dwords and
a signed byte for Level/VelocityAmount/LevelAMSIntensity/LevelAMSSource
with a bare-stride-4 SIB-scaled ctx-index dword for LFOAmount and the
stride-5 lea-premultiply ctx-index shape for the LFOAmountAMSSource/
LFOAmountAMSIntensity pair -- both shapes already established, no new
decoder work needed. `CSTG3BandEQBase` (STG 3-band parametric EQ patch
component base) is the simplest dialect yet, a 100% ctx-only-suffix hit
rate across all 6 of its raw pending symbols and zero ctx-index methods
at all -- five plain fixed-K dwords plus one mask-only single-bit
bitfield (`GetBypassValue`). `CSTGEGBase::GetCurve` is the first
confirmed case in this family of a ctx-indexed UNSIGNED byte load (bare
stride-1, `movzx`) -- prior unsigned-byte precedents were always plain
fixed-K fields, never ctx-indexed; no decoder change needed since
sign/width is already tracked independently of ctx-indexing.

Tooling: hit and fixed one fresh instance of the parenthesis-swallow
`DEF_RE` gotcha in `oa_stg_amp.h`'s leading comment -- multiple literal
signature mentions and balanced parenthetical asides with zero real
semicolons anywhere in the span let the greedy capture run all the way
through to the real `CtxIndex` helper's own closing paren, swallowing its
definition (captured `{"GetSubComponent"}` instead of the wanted
`{"CtxIndex"}`). Caught via the standard exact-name-set diff before ever
attempting to build; fixed by removing every literal `(` before the real
code. Notable negative control this batch: `oa_stg_eg_base.h`'s prose had
just as many `word\s*\(` matches but did NOT trigger the bug, saved only
by one incidental real semicolon elsewhere in its own prose -- a reminder
that passing the check by accident isn't the same as being written safely.

`make verify`: exit 0, 0 FAIL lines across the whole suite (175 test
binaries), all 3 new KATs passing. Real `make ko-clean && make ko
KDIR=/home/build/linux-kronos` Kbuild build: clean link, `OA.ko` produced
(499508 bytes), zero warnings or errors traceable to the 3 new files.
`DECOMPILE_ERRORS.md` stays empty -- no compile/link blocker hit.
`manifest/gen_oa_manifest.py` regenerated, OA.ko manifest 2366 ->
2384/21,689 (10.992%), delta exactly +18.

Real-HW test that would help: none identified, same rationale as every
prior entry in this family -- pure parameter-reflection plumbing with no
direct front-panel/audio observable.

## CSTGVPMOutputMixer + CSTGKeyTrack + CSTGPortamentoBase value-getter families -- 20 methods (batch 12, 2026-07-28)

Continuing the ~2300-method STG value-getter family, manifest
2384 -> 2404/21,689 (11.084%). Same ground truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`).

First act this batch: spot-checked every "unconfirmed" candidate carried
over from batches 9-11's own notes before picking anything new.
`CSTGFrontPanelSmoothers`/`CSTGCommonStepSeq`/`CSTGAudioInput`/
`CSTGHDRTrack`/`CSTGHDRMiniModel`/`CSTGProgramModeProgramSlot` all
CONFIRMED already-modeled via word-boundary grep (each has real hits in
`oa_global.h`/`oa_engine_init.h` and multiple `src/engine/*.cpp` ctor/
init call sites) -- zero real ctx-only weak candidates for any of them
either, cross-confirmed via `nm`. `CRPPRManager`/`CSPRRecDataMerger`/
`CSPRAudioPlayer`/`CSPRSongControl` all CONFIRMED NOT part of this
family -- `nm` shows 100% global ('T') linkage for all four (23/22/22/17
methods respectively), every one taking extra args beyond ctx
(`SetRPPRMode(int, int)`, `GetTrackInfo(int)`, `GetPattern(int)`, etc),
same different-mechanism outcome as the already-rejected
`CSPRSeqDataManager`/`CSTGPCMModelPatch` precedents. All 10 carryover
candidates now resolved -- remove from future candidate lists.

Re-ran the full survey with a corrected methodology: rather than
per-class `nm` greps (error-prone on class-name length-prefix
mismatches, caught and fixed one such bug mid-session), dumped every
real ctx-only weak symbol in the whole binary in one pass and grouped by
mangled length-prefixed class name, giving a complete, authoritative
picture of every class in the family with at least one real candidate
regardless of prior pending-count-based surveys. This surfaced
`CSTGTG92OscBase` (10 raw candidates) as the next-largest untried class
by size -- picked first, then dropped after disassembly revealed a
genuinely new, more severe outlier shape (below); replaced with three
smaller but fully clean classes from the same fresh-candidate sweep.

**New outlier class, first of its kind: vtable slot resolves to
`__cxa_pure_virtual` in the candidate's OWN class.**
`CSTGTG92OscBase`'s 9 of 10 real candidates (all but `GetFreqOffset`,
a plain fixed-K dword) load ctx's dynamic-index field, then do
`mov edx,[eax]` (this's own vtable pointer) followed by
`call [edx+0xd4]` BEFORE the usual stride-multiply-and-field-load
sequence -- superficially the same "virtual-call-mediated sub-object
base pointer" shape as `CSTGPianoModelPatch`'s own
`AccessSustainPedalDown/UpVelocityZones` precedent from batch 6. This
time, decompiling the vtable target directly (per that batch's own
"decompile before assuming outlier" rule of thumb) found the raw
`.rodata._ZTV15CSTGTG92OscBase` relocation at that slot points to
`__cxa_pure_virtual`, confirmed by cross-referencing the vtable's
raw-offset-to-call-offset relationship (vptr = section base + 8, so
`call [edx+0xd4]` resolves to raw section offset 0xdc) against two
already-known real, concrete symbols at nearby raw offsets in the same
vtable (`GetRestrikeLimitForNote` at raw 0xc8, `GetRequiredVoiceInfo` at
raw 0xcc) to validate the offset math before trusting the pure-virtual
read. Unlike the `CSTGPianoModelPatch` precedent (concrete override,
mechanically trivial, safely inlined), a pure-virtual slot in the
candidate class's OWN vtable means `CSTGTG92OscBase` is genuinely
abstract at this method -- real behavior depends entirely on which
concrete subclass overrides it at runtime, which cannot be determined
statically from this class's own disassembly alone. Correctly treated as
a class-level Tier-B scope deferral (needs the concrete subclass, e.g.
whichever `CSTGXxxTG92Osc`-family class actually instantiates this base,
identified and its own vtable's slot 0xd4 target decompiled instead) --
not attempted, no file written for this class this batch. Rule of thumb
for future classes: after finding a virtual-call-mediated base pointer,
always decompile/cross-check whether the target resolves to
`__cxa_pure_virtual` before concluding the shape is safely inlineable
like `CSTGPianoModelPatch`'s own case -- a superficially identical call
site can resolve to either a trivial concrete accessor or a genuinely
abstract slot, and only checking the actual relocation target
distinguishes them.

All three picked classes came back fully clean -- zero outliers, fourth
batch of the last five with a full clean sweep (batches 9, 10, 11 also
clean; only the dropped `CSTGTG92OscBase` broke the streak, and it was
never actually attempted as a file). `CSTGKeyTrack` (STG key-tracking/
keyboard-scaling patch component) is the simplest dialect yet -- 7 plain
fixed-K byte fields, zero ctx-index, three unsigned key-position bytes
plus four signed ramp bytes, all single-write. `CSTGPortamentoBase` (STG
pitch-glide patch component) packs three independent single-bit booleans
(Enabled/Fingered/ConstantTime) into one byte at +0x1d via the
established shift-then-mask bitfield shape, plus plain fixed-K Time/
AMSSource/AMSIntensity fields, zero ctx-index.

**New confirmed ctx-index premultiply factor: x9.** `CSTGVPMOutputMixer`
(VPM engine per-operator output mixer -- level, pan, phase invert, plus
AMS siblings for level and pan) uses `lea edx,[edx+edx*8]` to premultiply
ctx's dynamic-index field by 9 -- every prior lea-premultiply shape in
this family used factor 5. Combined with an explicit x2 SIB scale on the
field load itself (`[eax+edx*2+K]`), the effective stride is 18 -- a new
confirmed value, decoded via the existing "SIB scale multiplies into the
existing premultiply stride" generalized rule from batch 8, no decoder
code change needed. `CSTGVPMOutputMixer::GetPhaseInvert` also uses the
established ctx-shift single-bit-boolean shape (`CtxShift`) off a fixed
byte field at +0x78, first confirmed on `CSTGVPMModelPatch`'s
`GetInterMixerLink`/`GetOscMacroClass`.

**Tooling: hit and fixed two fresh instances of the parenthesis-swallow
`DEF_RE` gotcha in `oa_stg_vpm_output_mixer.h`, both in the SAME file on
the first draft** -- "a new confirmed stride value (18, distinct from
the family's prior 10/20/25 ... variants)" and, after fixing the first,
a second independent trigger "Field-shape summary (record base =
CtxIndex(ctx, 0x4, 18)):" whose own embedded parenthesized code-like
expression (a real call-shaped mention of `CtxIndex(...)`) still had no
semicolon before it to break the runaway, letting the greedy capture
skip straight past the real `CtxIndex`/`CtxShift` definitions again.
Also hit the same "field (+0xc)"-shaped bracketed-offset annotations used
throughout that file's field-shape summary list, reworded to "field at
+0xc" prose with zero parens per the established convention, rather than
just fixing the two literal triggers and leaving the rest as latent
risk. Confirmed clean via the standard two-check discipline -- comment
open/close-count balance and an exact `DEF_RE` captured-name-set diff --
re-run after each fix, not just once. `CSTGKeyTrack`'s and
`CSTGPortamentoBase`'s header/`.cpp` pairs passed both checks clean on
the first draft, no fixes needed.

`make verify`: exit 0, 0 FAIL lines across the whole suite, all 3 new
KATs passing. Real `make ko-clean && make ko KDIR=/home/build/linux-kronos`
Kbuild build: clean link, `OA.ko` produced (502300 bytes), zero warnings
or errors traceable to the 3 new files (confirmed via a build-log grep
scoped to each new filename). `DECOMPILE_ERRORS.md` stays empty -- no
compile/link blocker hit (the `CSTGTG92OscBase` pure-virtual finding is
a scope deferral, not a compile/link failure, so it's logged here
instead per this file's own documented distinction).
`manifest/gen_oa_manifest.py` regenerated, OA.ko manifest 2384 ->
2404/21,689 (11.084%), delta exactly +20, matching the sum of all three
classes' real candidate counts (7+7+6) with zero regressions.

Real-HW test that would help: none identified, same rationale as every
prior entry in this family -- pure parameter-reflection plumbing with no
direct front-panel/audio observable.

## CSTGDriver + CSTGVPMNoise + CSTGAnalog4Pole + CSTGPluckedModelPatch + CSTGMOSSAmp + CSTGPitchModOsc value-getter families -- 41 methods (batch 13, 2026-07-28)

Continuing the ~2300-method STG value-getter family, manifest 2404 ->
2445/21,689 (11.273%). Same ground truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`).

All six classes were carried over from batch 12's own notes, which had
already spot-checked and shape-classified all six as zero-outlier before
this session began, but had NOT recorded their exact field offsets --
this session re-pulled the real disassembly for all 41 candidates via `nm`
+ `objdump -dr -M intel -j .text.<mangled>` (per-symbol COMDAT sections,
same technique as every prior batch) rather than trusting the prior
summary's shape description alone. Every offset, width, sign, and
dual/single-write fact below was independently re-derived from the actual
bytes, not copied from batch 12's prose. All six classes' real candidate
counts matched the memory file's carried-over counts exactly (8 + 7 + 7 +
7 + 6 + 6 = 41), and all re-confirmed zero outliers -- a sixth batch in a
row with a full clean sweep across every class attempted (this streak now
spans batches 9-13 with the sole exception of the deliberately-dropped,
never-attempted `CSTGTG92OscBase`).

Word-boundary `grep -rn '\bClassName\b' src include` re-confirmed all six
still genuinely fresh (only `CSTGPitchModOsc` has an incidental,
non-triggering mention in `oa_stg_pitch_mod.h`'s own prose, as already
noted in batch 12). `CSTGAnalog4Pole` reconfirmed distinct from the
already-done `CSTGAnalog4PoleBase` (name-collision precedent). No new
outlier shapes or decoder generalizations were needed this batch -- every
one of the 41 methods fits shapes already established in batches 1-12:
plain fixed-K dword/signed-byte fields (dual/single-write per the
established width rule), the mask-only single-bit bitfield shape
(`CSTGDriver::GetBypass`), the hardcoded-constant-getter shape first seen
on `CSTGPanOutputBase::GetPatchSolo` (`CSTGVPMNoise::GetSaturation`, no
`this` dereference at all, single 0-write), the unsigned non-bitfield byte
variant first seen on `CSTGPolysixMG::GetMIDITempoSyncTimes`
(`CSTGPitchModOsc::GetEGSelect`), and the stride-5 lea-premultiply
ctx-dynamic-index sub-family (`CSTGMOSSAmp`'s and `CSTGPitchModOsc`'s own
AMSSource/AMSIntensity/AMSIntensityAMSSource/AMSIntensityAMSIntensity
groups) -- the same naming-implies-second-modulation-level shape that was
a false alarm on `CSTGMS20EG` but is confirmed REAL ctx-indexing on both
of these two classes, reconfirming that field-shape must always be
verified from disassembly per method/class rather than assumed from a
prior class's outcome with similar naming.

`CSTGAnalog4Pole` is notable for unusually large field offsets (up to
`+0x12c`) reflecting a big struct layout, otherwise a plain fixed-K-only
dialect like `CSTGVPMNoise`/`CSTGDriver`/`CSTGPluckedModelPatch`.
`CSTGPluckedModelPatch` had 5 further pending symbols correctly excluded
up front (`GetRequiredVoiceInfo`/`SetupComponentOffsets`/
`GetFeedbackChannelLevels` global-linkage extra-arg methods,
`GetEG(unsigned int)`/`GetLFO(unsigned int)` weak but explicit-index
signatures) -- none fed to the decoder. `CSTGMOSSAmp` had its own 3
pre-excluded symbols (`GetSubComponent(unsigned short)` sub-object-accessor
outlier, `SetupComponentOffsets`/`SetOutputLevelMultiplier` global-linkage
extra-arg methods).

KAT generation this batch used a small standalone Python evaluator
(`/tmp/.../kat_gen_batch13.py`, one-shot scratch script, not checked in)
mirroring the family's established discipline -- same deterministic
`buf[i] = (i*0x9f + 0x37) & 0xff` pattern, ctx index fixed at 3, 32-bit
signed dword loads, 8-bit sign/zero-extension per field -- generating
every expected constant mechanically rather than by hand. This caught a
real transcription slip: the first hand-typed draft of
`test_stg_driver_valuegetters.cpp`'s expected values (computed mentally
rather than via the script) was wrong in every single constant; replaced
with the script's output before ever running `make verify`, and every
other class's test file was written directly from script output from the
start. Worth reinforcing as a hard rule for all future batches in this
family: NEVER hand-compute a KAT expected constant, always generate it
via a script, even for `make verify`-only host tests where an error would
be immediately visible on the FIRST run -- the risk isn't test failure,
it's a wrong constant that happens to still "pass" against an equally
wrong return value if the same slip were made twice (not the case here,
but not worth risking).

**Tooling: one fresh `DEF_RE` parenthesis-swallow instance found and
fixed**, in `oa_stg_moss_amp.h`'s own derivation prose -- "rather than a
false alarm (unlike CSTGMS20EG's own case, which turned out to be plain
fixed offsets throughout)." had no semicolon before it and reached past
the real `CtxIndex` helper's own closing brace, captured name set was
`{"alarm"}` instead of the wanted `{"CtxIndex"}`. Fixed by rewording to
an em-dash-delimited clause per the established convention. All 12 new
files (6 headers + 6 `.cpp`) were run through both standard checks --
comment open/close-count balance and an exact `DEF_RE` captured-name-set
diff -- before any build attempt; only this one instance needed a fix.

`make verify`: exit 0, 0 FAIL lines across the whole suite, all 6 new
KATs (41 checks total) passing. Real `make ko-clean && make ko
KDIR=/home/build/linux-kronos` Kbuild build: clean link, `OA.ko` produced
(508216 bytes, up from batch 12's 502300), zero warnings or errors
traceable to any of the 6 new files (confirmed via a build-log grep
scoped to each new filename -- only the same boilerplate
command-line-flag warnings shared by every TU in the build appear, no
content-specific warnings). `DECOMPILE_ERRORS.md` unchanged -- no
compile/link blocker hit, and this batch had no new Tier-B scope
deferral either (the `CSTGTG92OscBase` deferral from batch 12 remains the
only open one). `manifest/gen_oa_manifest.py` regenerated, OA.ko manifest
2404 -> 2445/21,689 (11.273%), delta exactly +41 (8+7+7+7+6+6, matching
every class's real candidate count), confirmed via a full reconstructed
qualified-name-set diff -- 41 added, 0 regressions.

Real-HW test that would help: none identified, same rationale as every
prior entry in this family -- pure parameter-reflection plumbing with no
direct front-panel/audio observable.

## CSTGSimpleAMSMixer + CSTGPitchModCommon + CSTGPitchModCommonPlusAMS + CSTGVPMEG value-getter families -- 17 methods (batch 14, 2026-07-28)

Continuing the ~2300-method STG value-getter family, manifest 2445 ->
2462/21,689 (11.351%). Same ground truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`).

Re-ran batch 12's whole-binary weak-symbol sweep fresh (`nm $KO | grep -E
'^[0-9a-f]+ W _ZN[0-9]+.*ER23CSTGPatchMessageContext$'`, grouped by
mangled length-prefixed class name via a small Python parser) since the
genuinely-fresh candidate pool has thinned out considerably -- only 61
classes total still had at least one real ctx-only candidate, most
already individually catalogued across the prior 13 batches. The two
largest raw counts left, `CSTGLFO` (21 candidates) and `CSTGADSRBase` (20
candidates), turned out to BOTH already be fully hand-modeled from an
earlier, separate 2026-07-27 effort predating this scripted family
entirely (`src/engine/lfo_component.cpp`, `src/engine/adsr_base.cpp`) --
confirmed via word-boundary grep and correctly skipped, same
`CSTGProgramSlot`/`CSTGProgram` already-modeled precedent. `CSTGPatch` (4
candidates) is also already-modeled (a real `struct CSTGPatch` at
`include/oa_types.h:123` plus stub member functions in
`src/stub/bar2_stubs_auth.cpp`) -- skipped for the same reason.

This left only small, 4-5-candidate fresh classes as this batch's own
pool. `CSTGPitchModCommon`/`CSTGPitchModCommonPlusAMS` initially looked
risky, since both names appear as bare word-boundary grep hits in the
already-modeled `CSTGPitchMod`'s and `CSTGPitchModOsc`'s own header
prose -- direct inspection confirmed both hits are purely incidental
mentions listing sibling class names in a derivation comment, not real
struct/ctor references, so both classes are genuinely fresh per the
established incidental-vs-real-reference distinction (same kind of check
as `CSTGPolysix`/`CSTGPolysixModel`). `CSTGPitchModCommonPlusAMS` was
picked up as a bonus fourth class this batch because its own 2 real
candidates fell out of the identical `nm` grep used for
`CSTGPitchModCommon` -- its mangled class name is a superset match of the
other's -- so writing both up together cost no extra survey work; the two
are directly related sibling classes (the PlusAMS variant adds one extra
AMS modulation leg on top of the Common base's own fields).

All four classes came back fully clean -- zero outliers, extending the
clean-sweep streak to batches 9-14 (the sole exception remains the
deliberately-dropped, never-attempted `CSTGTG92OscBase` pure-virtual
deferral from batch 12, still open). `CSTGSimpleAMSMixer` (a small
two-input AMS modulation mixer -- Type selector plus two independent
Source/Amount legs) and `CSTGPitchModCommonPlusAMS` are both the
simplest dialect -- zero ctx-index, plain fixed-K bytes and dwords only.
`CSTGPitchModCommon` is notable for sharing method names (`GetLFOAmount`,
`GetLFOAMSSource`, `GetLFOAMSIntensity`, `GetJSYToLFOAmount`) with the
already-modeled `CSTGPitchMod`'s own LFO group, at entirely different
field offsets -- confirming the two are genuinely separate classes
sharing a naming convention, not the same class reused, and also
notable for being zero-ctx-index despite `CSTGPitchMod`'s own matching
fields being ctx-indexed.

**Genuinely new asymmetric ctx-index variant, first of its kind in the
family.** `CSTGVPMEG`'s AMS1LevelModSource/AMS1LevelModIntensity and
AMS1TimeModSource/AMS1TimeModIntensity pairs split the by-now-familiar
bare-stride-4 SIB-scaled ctx-index shape (first confirmed on
`CSTGMultiFilter2Pole`, reused on `CSTGEG`) so that ONLY the Intensity
half of each pair is ctx-indexed:

```
GetAMS1LevelModSource:    movsx eax, BYTE PTR [eax+0x3e]        ; plain fixed byte
GetAMS1LevelModIntensity: mov edx, DWORD PTR [edx+0x4]          ; ctx's own index field
                           mov eax, DWORD PTR [eax+edx*4+0x3f]   ; bare stride-4 SIB
```

Every PRIOR class with this bare-stride-4 shape (`CSTGMultiFilter2Pole`,
`CSTGEG`) had BOTH halves of each Source/Intensity pair ctx-indexed
together -- this is the first confirmed case of the split, verified
directly from the actual disassembly rather than inferred from the
shared "AMS1" naming. No decoder change was needed: the shared decoder
already evaluates each method's field-load shape independently rather
than assuming pair symmetry, this is purely a new confirmed data point
reinforcing the family's standing rule to verify every method
individually. `CSTGVPMEG::GetTriggerAtNoteOn` reuses the established
mask-only single-bit bitfield shape (no shift instruction, bit 0),
single-write only -- no new shape needed there either.

**Tooling: one fresh `DEF_RE` parenthesis-swallow instance found and
fixed**, in `oa_stg_vpm_eg.h`'s own derivation prose -- two plain
parenthetical asides ("stride-4 shape (CSTGMultiFilter2Pole, CSTGEG) had
BOTH halves..." and "bitfield shape (no shift instruction, bit 0),
single-write only") with zero semicolons anywhere in the span before the
real `CtxIndex` helper's own closing brace, letting the runaway match
reach past the comment close and mis-capture the word immediately before
the first trigger paren ("shape") instead of `CtxIndex`. Caught via the
standard exact `DEF_RE` captured-name-set diff (`got=={"shape"}` instead
of the wanted `{"CtxIndex"}`) before ever attempting to build; fixed by
rewording both to em-dash-delimited clauses, the established convention.
All 8 new files (4 headers + 4 `.cpp`) passed both standard
post-generation checks -- comment open/close-count balance and the exact
`DEF_RE` captured-name-set diff -- before any build attempt; only this
one instance needed a fix.

KAT generation used a standalone scratch Python evaluator (same
deterministic `buf[i] = (i*0x9f + 0x37) & 0xff` pattern, ctx index fixed
at 3, 32-bit signed dword loads, 8-bit sign/zero-extension per field)
from the very first draft, per batch 13's own hard-rule reinforcement --
no KAT constant was ever hand-typed this batch.

`make verify`: exit 0, 0 FAIL lines across the whole suite (43 checks
total), all 4 new KATs (17 checks) passing. Real `make ko-clean && make
ko KDIR=/home/build/linux-kronos` Kbuild build: clean link, `OA.ko`
produced (510736 bytes, up from batch 13's 508216), zero warnings or
errors traceable to any of the 4 new files (confirmed via a build-log
grep scoped to each new filename). `DECOMPILE_ERRORS.md` unchanged -- no
compile/link blocker hit, and this batch had no new Tier-B scope
deferral either (the `CSTGTG92OscBase` deferral from batch 12 remains the
only open one). `manifest/gen_oa_manifest.py` regenerated, OA.ko
manifest 2445 -> 2462/21,689 (11.351%), delta exactly +17, confirmed via
a full reconstructed qualified-name-set diff -- 17 added, 0 regressions.

**Open item carried forward, not resolved this batch**: this batch's own
whole-binary sweep found `CSTGPCMModelPatch` with 2 real weak ('W')
ctx-only-suffix candidates, which directly contradicts batch 9's own
verdict that `CSTGPCMModelPatch` is "NOT part of this family" (batch 9
found only 2 symbols total, both global ('T') linkage). Not re-checked
or reconciled this batch -- flagged for a direct `nm` query on
`CSTGPCMModelPatch` specifically next session before trusting either
verdict.

A concurrent session's untracked `reconstructed/Eva/tools/
build_gdbserver.sh` / `gdbserver-i386-musl` files were visible in `git
status` throughout this batch's work -- staged only the 13 intended OA
files by exact path (never `git add -A`/`git add .`) and verified `git
diff --cached --stat` matched exactly before committing, per this
project's shared-repo commit hygiene convention.

Real-HW test that would help: none identified, same rationale as every
prior entry in this family -- pure parameter-reflection plumbing with no
direct front-panel/audio observable.

---

## STG value-getter family batch 20 (2026-07-28): CSTGTG92OscBase partial (1/10), family growth confirmed exhausted

Continuation of the STG value-getter family using batch 18's broader
`sValueGetterTemp`-relocation discovery method. Re-ran the full recipe
fresh against the same ground-truth binary
(`/home/share/Decomp/OA.ko_Decomp/OA.ko`): 1462 unique enclosing
functions, grouped into the identical 75-class list batches 18/19 already
found, byte-for-byte the same class names and per-class hit counts.
Cross-checked all 75 against this project's own done/excluded/
already-modeled/deferred registry (accumulated across batches 1-19): 62
done, 9 already-modeled elsewhere (skip), 2 harmless false positives
(`CSTGParamsOwner` itself, the static-initializer stub), and exactly 2
still-open items -- `CSTGTG92OscBase` (the batch-12 pure-virtual
deferral) and `CSTGDrumKitData` (the batch-19 entangled-blob deferral).
Also independently reconfirmed there is no NINTH `*MessageContext`
sibling type anywhere in the binary (`nm -C $KO | grep -oE
'[A-Za-z_][A-Za-z0-9_]*Context\b'` lists exactly the same 8 real context
types batches 18-19 already found, plus 2 unrelated non-class hits) --
the per-context-type discovery axis from batch 19 is confirmed exhausted
too, not just the `sValueGetterTemp` sweep. **The family's two
established discovery methods are both now provably exhausted against
this ground-truth binary; no further classes can be found by re-running
either one.**

Given the task's own standing exclusion guidance for entangled
opaque-blob classes, `CSTGDrumKitData` was deliberately NOT attempted
this batch (left as the already-documented future target from batch 19's
own entry, full 3-dimensional piecewise-index derivation already
captured there).

`CSTGTG92OscBase` was revisited instead. Re-confirmed the batch-12
pure-virtual finding with fresh, independent evidence rather than trusting
memory: `objdump -r -j .rodata._ZTV15CSTGTG92OscBase $KO` dumped every
relocation in the class's own vtable directly, confirming raw offset 0xd4
(the slot all 9 of the class's ctx-indexed candidates dispatch through
before their own field read) resolves to `__cxa_pure_virtual`. Went
further than batch 12 did: searched the WHOLE symbol table for each of
the 9 pure-virtual candidates' own method names (`GetReverse`,
`GetBankType`, `GetStartOffset`, `GetBankSelectUUID`, `GetBottomVelocity`,
`GetCrossfadeCurve`, `GetCrossfadeRange`, `GetMultisampleNum`,
`GetLevel`) across every OTHER class in the binary -- zero concrete
overrides found anywhere. This binary genuinely never instantiates a
concrete subclass of `CSTGTG92OscBase` through any symbol visible to
static analysis; the 9 pure-virtual candidates remain correctly
undecodable, not merely unattempted.

The class's 10th candidate, `GetFreqOffset`, does NOT touch the vtable at
all -- disassembly confirms a completely ordinary fixed-K signed dword
field read directly off `this` (`this+0xc`, dual-write to both `value`
and `displayValue`), the family's most common shape. Reconstructed as a
deliberate 1-of-10 partial class (`include/oa_stg_tg92_osc_base.h`,
`src/engine/stg_tg92_osc_base_valuegetters.cpp`), matching the family's
established precedent for partial classes with a documented, evidence-backed
exclusion for the rest (`CSTGEGBase` 5/19, `CSTGAmp` 7/10, `CPianoOsc`
46/53, etc).

`make verify`: exit 0, 0 FAIL lines, the 1 new KAT check passing.
Real `make ko-clean && make ko KDIR=/home/build/linux-kronos` Kbuild
build: clean link, `OA.ko` produced (530868 bytes, up from batch 19's
530640), zero warnings/errors traceable to the new file (confirmed via a
build-log grep scoped to the new filename). `DECOMPILE_ERRORS.md`
unchanged -- no compile/link blocker hit, and the `CSTGTG92OscBase`
pure-virtual finding is a reconfirmation of an already-logged Tier-B
deferral, not a new one. `manifest/gen_oa_manifest.py` regenerated
against the TRUE prior committed state via `git stash -u`/`git stash
pop` (not trusting the on-disk CSV, which is untracked and can be stale):
OA.ko manifest 2575 -> 2576/21,689 unique reconstructed names (11.978%
raw-row convention: 2597 -> 2598), delta exactly +1, confirmed via a
full reconstructed qualified-name-set diff -- 0 regressions.

Re-decompiler agent memory updated with the confirmed-exhausted state of
both discovery methods, the `CSTGTG92OscBase` partial-class precedent,
and the manifest count. `CSTGDrumKitData` remains the only real,
attemptable next target for this family; no other fresh candidates exist
under either established discovery method as of this batch. A
concurrent session's untracked `reconstructed/Eva/tools/
build_gdbserver.sh` / `gdbserver-i386-musl` files were visible in `git
status` throughout this batch's work and correctly left untouched, per
this project's shared-repo commit hygiene convention.

Real-HW test that would help: none identified, same rationale as every
prior entry in this family -- pure parameter-reflection plumbing with no
direct front-panel/audio observable.

---

## CKGControlMsgHandler + CKGUIMsgSender — new cluster after STG value-getter/CKG*ParamMsgHandler closure, 53 methods (batch, 2026-07-28)

Fresh broad survey per the standing "find the next dense cluster" directive
(the STG value-getter family and all three `CKG*ParamMsgHandler` classes
were confirmed fully closed as of the prior batch). Grouped every pending
manifest row by class and sorted by count; investigated and correctly ruled
out several large-looking candidates first: `CKGParamEdit` (133 pending --
the checked-write family's own SendXxx() call target, not itself
decoder-friendly, already flagged in an earlier batch), the whole
`CSPR`/`CRPPR`-prefixed sequencer-record family (`CSPRRecorder`,
`CRPPRManager`, `CSPRControlMsgHandler`, etc. -- sample disassembly at
several of their own manifest addresses landed MID-FUNCTION inside
`CSingleRPPR::settrkno`/`CSPRRecorder::RenewSongPlayParamAfterRec`, i.e. the
Ghidra static export's function-boundary detection is unreliable across this
whole family, not a real distinct-function set worth attempting), and the
`CKGSwitch`/`CKGKnob`/`CKGPad` front-panel-control widget hierarchy (~19
classes, ~194 methods, a genuine diamond-multiple-inheritance tree rooted at
an abstract `CKGController` virtual base -- real, tractable in principle
(same technique as Eva's `CStream` family), but each method needs
individual semantic tracing of real branchy vtable-dispatch logic rather
than a mechanical field-offset transcription; investigated in depth
(full vtable relocation dump, construction-vtable inheritance graph,
several methods' real field layout) but deliberately NOT attempted this
batch given the effort-per-method ratio -- left as a well-scoped, evidence-
rich next target, see re-decompiler agent memory for the full findings).

Landed on `CKGControlMsgHandler` (UI-triggered action dispatch,
`.text+0x3c79c0`..) + `CKGUIMsgSender` (UI-notification send-side,
`.text+0x3c84e0`..`.text+0x3c90a0`) -- a genuinely new, third convention
(neither the STG value-getter shape nor the CKG*ParamMsgHandler checked-
write skeleton): plain UI actions forwarding into `CKGEngine`/
`CKGBankManager`/`CKGRTCHandler`/`CKGParamEdit`, and a message-builder that
packs a fixed-shape `CSKMessage` payload and calls `KGOutGate_SendMessageToUI()`.
53 methods reconstructed (24 `CKGControlMsgHandler` + 29 `CKGUIMsgSender`,
both including their ctors), all disassembly-verified individually --
`CKGUIMsgSender`'s two message shapes needed particularly careful per-field
offset extraction since two visually-similar wrapper families
(`{SetModuleParamMax,SetModuleParamMin,UpdateModuleParam,DimOnModuleParam}`
vs `SendModuleParamMessage`) turned out to place their shared "value"
parameter at DIFFERENT byte offsets (`+0x1c` vs `+0x24`) despite otherwise
identical-looking field layouts -- caught only by disassembling each one
individually rather than trusting the first one as a template for the rest.

Deliberately deferred, NOT counted as done: `CKGControlMsgHandler::
HandleMessage(CSKMessage*)` (1181 bytes, a ~40-case jump-table dispatcher
over `msg[+0x8]` whose case bodies mostly INLINE logic duplicating --
not calling -- this class's own separately-addressed methods; a clean,
self-contained standalone target, full disassembly already transcribed) and
its 3 siblings `SharedMemProgramDump`/`SharedMemCombiDump`/
`SharedMemSongDump` (each calls `CKGProgramDownloader::
HandleProgramDownload`/`CKGCombiDownloader::HandleCombiDownload` with
`this` reinterpreted directly from `CKGControlMsg::m_mode` -- a real,
confirmed pointer-smuggled-through-an-int-field idiom, ruling out an
initial `ms_poInstance`-singleton-call assumption -- and, for the Combi/
Song variants, a 3rd `eSTGMsgPerfType` argument whose register/stack
position could not be pinned down with confidence: regparm(3) has only 3
GP registers total, already consumed by `this`+2 explicit args, yet no
stack spill is visible in either observed call site's disassembly window).
Not logged in `DECOMPILE_ERRORS.md` (that file is for compile/link
failures on an attempted reconstruction, not scope deferrals) -- documented
here and in `oa_ckg_control_ui_msg.h`'s own header comment instead, per
this project's established convention.

**New manifest-generator gotcha found and fixed, same session (no
regressions shipped)**: the generator's own address heuristic
(`ADDR_RE = \.text\+0x[0-9a-fA-F]{4,8}`) matches ANY `.text+0xXXXXXX`
literal appearing anywhere under `src/`/`include/`, including inside prose
comments describing a DEFERRED function's real address for documentation
purposes -- writing `HandleMessage`'s own real offset in a header comment
(purely to help a future session find it) caused the generator to
false-credit `HandleMessage` as reconstructed, purely from the mention. A
second, independent false-credit hit `CKGModuleParamMsgHandler`'s own ctor,
caused by an unrelated header comment citing `CKGUIMsgSender`'s class-region
END address, which happened to coincide with the START of
`CKGModuleParamMsgHandler`'s own (different, untouched-this-batch) region.
Separately, a THIRD gotcha: several real, correctly-implemented
`CKGUIMsgSender` methods (`UpdateChordAssignLED`, `ChangeGE`, etc.) were
NOT credited by the generator's name heuristic despite matching real
ground-truth bodies, because a trailing same-line comment
(`void Foo(...)	/* .text+0x... */\n{`) between the closing `)` and the
opening `{` breaks `DEF_RE`'s own `\)\s*...\{` tail pattern (a `/*...*/`
comment is not whitespace to the regex). Fixed by (1) describing
`HandleMessage`'s deferral without a literal `.text+0x` address citation,
(2) rewording the `CKGUIMsgSender` class-region-end comment to cite an
address this batch DID implement instead of the next class's boundary, and
(3) moving every trailing same-line address comment in `ckg_ui_msg_sender.cpp`
to its own line above the signature (the convention already used
everywhere else in both new files, which is why only THIS block was
affected). All three confirmed via a full reconstructed qualified-name-set
diff against the true committed baseline (`git stash -u` trick, not the
on-disk CSV) both before and after each fix -- final delta exactly +53,
0 regressions. Recorded in re-decompiler agent memory as three new,
distinct entries in the DEF_RE/ADDR_RE gotcha catalogue for this project.

`make verify`: exit 0, 0 FAIL lines (10385 ok lines across the whole
suite, 2 new KAT binaries, `test_ckg_control_msg_handler`/
`test_ckg_ui_msg_sender`, checking real byte offsets/values against a
mocked `KGOutGate_SendMessageToUI()` rather than trusting the
implementation's own field-offset choices). Real `make ko-clean && make ko
KDIR=/home/build/linux-kronos` Kbuild build: clean link, `OA.ko` produced
(630032 bytes, up from the prior batch's 573632/530868 range), zero
warnings or errors traceable to any of the 2 new files.
`manifest/gen_oa_manifest.py` regenerated against the TRUE prior committed
state via the `git stash -u` trick: OA.ko manifest 2830 -> 2883/21,689
(13.292%), delta exactly +53, confirmed via a full reconstructed
qualified-name-set diff -- 0 regressions.

Next targets for this same family: `CKGControlMsgHandler::HandleMessage` +
the 3 `SharedMem*Dump` methods (both fully scoped above); the
`CKGSwitch`/`CKGKnob`/`CKGPad`/`CKGController` front-panel-widget diamond-
inheritance hierarchy (~194 methods, investigated in depth this batch, see
re-decompiler agent memory for the full vtable-slot/field-offset findings);
and the broader `CSK`/`CKG` KARMA UI/MIDI family surfaced by the same
survey (`CSKMIDIInMsgHandler`, `CSKMIDILocalCtrlMsgHandler`,
`CSKSysExMsgHandler`, `CKGRTCHandler`'s own remaining 27 methods,
`CKGUIMsgProcessor`'s remaining 17, etc. -- ~845 methods across ~64 classes
total in this broader family, per this batch's own survey; only 53 done so
far).

Real-HW test that would help: none identified -- pure UI-notification/
message-dispatch plumbing with no direct front-panel/audio observable.

## CKGEngine — KARMA performance-editing engine, 55/74 methods (batch, 2026-07-28)

Fresh nm-based survey (per-class weak/global symbol counts, sorted by size)
for the next major dense cluster now that the STG value-getter family and
all three `CKG*ParamMsgHandler` classes were closed. `CKGEngine` was the
obvious next target: `ms_poInstance`/`ms_poKGParamEdit` are already read
by dozens of previously-reconstructed CKG*/CSK* methods across this
project (the widget family, `CSKMIDIMsgHandler`, `CKGControlMsgHandler`,
etc.), but the class itself was still a 24-declaration opaque stand-in
with zero real bodies. Real ground truth: 74 methods, `.text`+0x3a96e0
through +0x3aee80.

55/74 reconstructed for real: ctor/dtor (placement-constructs `CKGParamEdit`/
a NEW opaque dependency `CKGTimerManager`/`CKGEventDisplayManager`,
zero-inits every field the real ctor touches plus `m_numModules` — real
ctor genuinely never sets it, only `Initialize()` does, a documented
"deterministic KAT tests" deviation), `GetKarmaMode()` (the Combi/Program/
Song mode dispatcher, with a Program-mode special case comparing
`CKGBankManager`'s own first field against a fixed offset into the SAME
allocation — the built-in KorgX2100 template — confirmed via the identical
inline computation independently reused inside
`SendChangePerformanceToEngine()`), the KARMA-engine performance-change
pipeline (`SendChangePerformanceToEngine`/`ChangePerformancePtrForEngine`/
`Initialize`), per-module channel/timbre-thru queries (with a real quirk:
`module >= m_numModules` falls back to module 0's own record rather than
erroring), MIDI real-time message forwarding, RTC reset/compare/backup
plumbing, and `CopyCurrentParameterToSharedMemory()` (3 real memcpy
segments, decoded from GCC's own `rep movsd` + conditional
`movsw`/`movsb` tail-copy expansion back into (dst,src,byte-count)
triples — the tail byte-count encoding is the real total size's own low 2
bits, confirmed by cross-checking against the visible dword count rather
than guessed).

**19 methods DEFERRED**, declared but not defined (standard "expected
Unknown symbol at insmod" convention, verified via a real Kbuild build —
`nm` on the linked `OA.ko` shows exactly these 19 plus the new KARMA
externs as `U`, nothing unexpected): `IsEditedPerf()` (9458 bytes, a huge
outlier, almost certainly a giant per-RTParam edited-state comparison —
not attempted at all); the "per-RTParam table" family
(`FakeTimbreThru`/`RefreshPERTParmInfo`/`SetPERTParmMinMax`/
`SetPERTParmControlModule`/`SetGERTParmMinMax`/`RefreshGERTParmInfo`/
`SendChangeGEToEngine`/`DoInitModule`/`DoRandomCaptureExec`/
`UpdateEnableDirectPathForVectorCC`/`ChangePerformance` (the top-level
2-arg orchestrator)/`CloseGECategoryPopup`/`UpdateGEInfo`) — all real,
address-confirmed, and proven mechanical-but-lengthy by contrast with the
two INCLUDED members of the same general shape
(`StoreGERTParmMinMaxToBank`/`DoRandomCapture`, both fully reconstructed
this batch); and `ChangeValuesInBackupWhenChangingGE()` (both overloads)
+ `ProcessForSeqWhenChangingGE()` (whose only 2 real call targets are
those overloads) — a dense, multi-segment field-by-field struct copy
between a live `CKarmaPerfCommon`/`CKarmaPerfModule` record and its
per-seq backup slot, traced far enough to see the overall shape (offsets
+0x4/+0x14/+0x127/+0x128/+0x136/+0x138/+0x148/+0x194, several
reused/rebased scratch registers) but not independently confirmed to the
same byte-exact confidence as the rest of this batch — a real, scoped
follow-up, not abandoned.

New dependency surface: `CKGTimerManager` (5 of 14 real methods — the
ones `CKGEngine` itself calls; the other 9 are a self-contained future
cluster of their own), ~50 free `RT_*`/`KS_*`/`KGOutGate_*` KARMA-library
externs (the generative/sequencing engine core itself, a separate
unmodeled subsystem, same "opaque out-of-project library" treatment
already established for the handful of such externs in
`oa_ckg_switch_family.h`), 11 new `CKGBankManager` methods, 2 new
`CKGRTCHandler` methods, 3 new `CKGParamEdit` methods — all
declare-the-interface-defer-the-body per this project's standing
convention for out-of-scope dependencies.

**Two real bugs caught and fixed in THIS batch's own new code** (not
pre-existing):
1. The SAME `ADDR_RE`-matches-prose gotcha already documented above for
   `CKGControlMsgHandler`/`HandleMessage` — re-triggered independently
   here by every one of this batch's own "DEFERRED -- .text+0xNNNNNN"
   comments, each of which falsely credited whatever unrelated real
   ground-truth function happens to share that manifest address purely
   from the citation (caught 5 collisions via the baseline diff, e.g.
   `CSPRHDRManager::SetErrorCode`/`ShouldPlayCheckingAutoInput`,
   `CSPRAudioPlayer::WaitUntilPlayStandby`/
   `SetStandbyNextEventBeforeRunning`). Fixed by rewording every deferred
   citation to "ground-truth offset 0xNNNNNN" (no `.text+0x` substring).
2. A systematic address-transcription error: every real `.text+0xNNNNNN`
   comment in this batch was computed via a stray subtract-then-mis-readd
   of the 0x10000 Ghidra image base (the correct convention is
   `comment_offset == raw nm address`, unmodified — the `TEXT_BASE` math
   in `gen_oa_manifest.py` already accounts for Ghidra's own base
   assignment). This put every citation at the WRONG manifest address,
   silently colliding with unrelated ground-truth functions the same way
   as gotcha #1 above, just for the 55 REAL (non-deferred) methods this
   time. Caught by the same baseline-diff technique (comparing before/
   after exact address sets, not trusting the raw newly-credited count);
   fixed by recomputing every citation directly from each method's own
   real nm address and re-verifying with a second clean diff (0
   collisions, 55 credited, 0 regressions).

Independent-oracle KAT (`verify/test_ckg_engine.cpp`, 85 checks) caught a
third, genuine logic bug before it shipped: `SendChannelMessage()`'s
`m_field0` gating direction was backwards on the first draft (the real
body does nothing at all when `m_field0 != 0`, not the reverse). Also
caught and fixed a 32-bit-vs-64-bit pointer-width host/target mismatch:
`CKGBankManager::ms_poInstance[+0]`/`[+4]`/`[+8]` are 3 independent 4-byte
pointer slots on the real i386 target, but a naive `unsigned char **`
cast reads/writes 8 bytes on this 64-bit verify host and silently
clobbers the adjacent slot — fixed using this project's established
`ToU32()`/`FromU32()` packed-pointer convention (already documented for
exactly this class of field elsewhere, e.g. `oa_engine_init.h`/
`oa_engine.h`) in both `ckg_engine.cpp` itself and the test's own mocks,
plus `-fno-pie`/`-no-pie` (same fix already established for
`test_tone_adjust_descriptors`) so the mock buffers' addresses fit in 32
bits for the round-trip.

`make objs`, `make verify` (all binaries green), and a real
`make ko-clean && make ko KDIR=/home/build/linux-kronos` build all green.
Manifest 3406 -> 3461/21,689 (+55, 0 regressions). Commit `018ce7e`.

Real-HW test that would help: none identified this batch either — KARMA
performance-change orchestration has no direct single-observable front-
panel/audio effect distinguishable from the dozens of other real KARMA
call paths already exercised by prior batches.

## RTParm free-function family, bottom-up batch — 56 members reconstructed, 7 deliberately deferred (2026-07-29)

Continuation of the RTParm GE/PE dispatch-table work (`rtparm-ge-table-
scripted-decoder`, `rtparm-pe-table-and-rtparm-family-survey` memory
entries): the ~99-function/62,318-byte free-function family those passes
identified but didn't attempt. Per this project's "verify tractability
before committing to a whole cluster" rule, sampled the family's LARGEST
members first (`AssignRTParmFunction_Drm`, 8294B — real nm size, not the
8094B the earlier survey estimated) before committing: confirmed real
(a genuine jump-table dispatcher over `GenMod`/`GenEffect`-domain
`gKS`-relative writes referencing already-declared `RT_*`/`GetFirstOnBit`/
`AssignRTParmGE` symbols, not an exotic unmodeled dependency) but too
dense to attempt this pass — reconstructed bottom-up instead, smallest
(8B) to largest attempted (`IsRTParmPairAssignedPE`/`GetRTParmAssigned_PE`,
400/402B).

56 members reconstructed: 48 free functions
(`src/engine/rtparm_family.cpp`) + `RTParmShortNameGroup`'s ctor and 2
setters + `RTParmNameManager::SetPrependCCInfo` (both in the same file) +
`CKGParamEdit::GetRTParmBufferSelectId`/`CKGSysExBuffer::
{StoreRTParmBySeq,SendParamsDependOnRTParm}` (kept in a SEPARATE new file,
`src/engine/rtparm_ckgparamedit.cpp` — see below). New shared header
`include/oa_rtparm_family.h` documents the reverse-engineered data model
(GenEffect/Performance/GenMod/gKS layout facts, RTParm/RTParmFunction/
RTParmEdit field offsets) other RTParm work can build on.

**7 members deliberately deferred**, all real disassembly read but left
`pending` rather than risk a wrong translation: `LimitRTParmEditValues`/
`LimitRTParmEditValuesRow` (interleaved dual-index clamp logic),
`UpdateRTParmIfSame_GE` (a pointer-identity/byte-compare dual-path loop
whose fail path re-enters mid-loop at a different label than the match
path — the single most confusing control flow sampled this pass),
`GetRTParmModAndID` (dynamic range search, capture also incomplete at
session end), `RTParmShortNameGroup::GetRTParmShortNameStringPtr` (nested
string-scan loops), `DoRTParmMultiEnablePE`/`DoRTParmMultiEnableGE`
(nested 8x8 loop with byte inversion and cross-table bit accumulation).
Full reasoning for each lives in `oa_rtparm_family.h`'s own header
comment. Logged as a real, working `.text+0x`-addressed TODO list, not a
vague "later" — a focused follow-up session can pick any of these up
directly.

**A real, pre-existing latent header conflict found and worked around,
not fixed** (logged in `DECOMPILE_ERRORS.md` too): `oa_ckg_module_param_
msg_handler.h` declares `RT_run(unsigned char, unsigned char)` `extern
"C"` (a deliberate choice for that file's OWN enum-widened KARMA externs,
per its own header comment) while `oa_rtparm_pe_table.h` declares the
SAME symbol `extern "C++"` (the real, GE/PE-table-verified mangled
linkage). No prior file ever included both headers together, so this
never surfaced before. `CKGParamEdit::GetRTParmBufferSelectId` needed
`CKGParamEdit`'s full declaration (only available via the first header)
while the rest of the family needed `gRTParmFunctionTable_PE` (via the
second) — worked around by keeping `CKGParamEdit::
GetRTParmBufferSelectId` in its own translation unit
(`rtparm_ckgparamedit.cpp`) with its own KAT binary
(`test_rtparm_ckgparamedit`), rather than editing either established
header under this batch's own time budget.

**3 real bugs caught by this batch's own independent-oracle KAT**
(`verify/test_rtparm_family.cpp`, 94 checks) before landing: (1)
`IsRTParmPairAssigned{GE,PE}`'s `(unsigned char)((~flag) >> 7)` — C++'s
integer promotion applies `~` to the promoted `int`, not the real 8-bit
`not bl` the ground truth's own register-width operation performs;
fixed by truncating back to `unsigned char` before shifting. (2)
`RTParmShortNameGroup::SetRTParmShortNameStringPtr` wrote a native 8-byte
`void*` into what's really a packed 32-bit field on the true -m32 target
(`mov DWORD PTR[eax],edx`), corrupting the next field's own bytes on this
64-bit verify host — fixed via this project's established packed-32-bit-
field convention (same class of issue as `CKGBankManager::ms_poInstance`,
prior batch). (3) A wrong test assumption, not a source bug: assumed
`gRTParmFunctionTable_GE[0]`'s `funcPtr` was `RT_bnd_amt` (the first
`extern` listed in `oa_rtparm_ge_table.h`) — that header's own listing
order is NOT the real table's initializer order; fixed the test to search
for entry `[5]`'s own real `funcPtr` self-consistently instead of
asserting which named function occupies which index.

`make verify` (all binaries green, including the 2 new ones) and a real
`make ko-clean && make ko KDIR=/home/build/linux-kronos` build both
green — confirmed via `nm OA.ko | c++filt`, every new symbol's real
mangled name matches ground truth exactly (e.g.
`CKGSysExBuffer::SendParamsDependOnRTParm(CSKParameterChangeMessage*)`),
and the handful of deliberately-`extern`-only sibling calls
(`AssignRTParmGE`/`AssignRTParmPE`/`GetFirstOnBit`/`KM_rtp_val_out_pe`/
`Do_KM_rtp_update_name`/`Do_KM_rtp_update_all_names`/`ScaleRTParmValue`)
show up as the expected "Unknown symbol" `U` entries with their own real
mangled names too. Manifest 3473 -> 3529/21,689 (+56, 0 regressions,
verified via a full before/after name-set diff).

Real-HW test that would help: none of this batch is independently
front-panel-observable — same reasoning as the CKGEngine batch above,
these are internal KARMA table-lookup/state-mirror helpers with no
single-path audio/UI effect.

## RTParm family follow-up — ALL 7 deferred members reconstructed (2026-07-29)

Dedicated follow-up to the bottom-up batch immediately above: took the 7
deliberately-deferred members (`LimitRTParmEditValues{,Row}`,
`UpdateRTParmIfSame_GE`, `GetRTParmModAndID`, `RTParmShortNameGroup::
GetRTParmShortNameStringPtr`, `DoRTParmMultiEnable{PE,GE}`) and re-traced
each from fresh `objdump -dr -M intel` disassembly (the `-r` flag turned
out load-bearing — see below). All 7 landed. `AssignRTParmFunction_Drm`
(8294B) still not attempted, remains `pending`.

Key finding: `GetRTParmModAndID`'s literal-looking immediates
(`cmp eax,0x1f40` etc.) all carry real `R_386_32 gKS` relocations — the
true comparison is against `&gKS+0x1f40`, not a bare integer. The prior
pass's own deferral note speculated the parameter was a caller-computed
integer offset; it's actually a genuine pointer into `gKS`. Two of the
7 (`LimitRTParmEditValuesRow`, `UpdateRTParmIfSame_GE`) turned out to be
compiler if-conversion/duplication of much simpler source
(`LimitRTParmEditValuesRow` collapses to
`clamp(f4,lo,hi); clamp(f6,lo,hi); clamp(f0,min(f4,f6),max(f4,f6))`) once
every branch was exhaustively traced and cross-checked — confirmed only
after full tracing, not assumed from the shape. The other 3 stayed as
dense as first assessed and are transcribed close to label-for-label.

Two real new callees found, declared `extern` (`pending`, not attempted):
`Do_KM_rtp_val_out_pe` (distinct mangled symbol from the already-declared
`KM_rtp_val_out_pe`) and `IsRTParmFunctionSameGE` (3907B, real sibling of
the already-reconstructed `IsRTParmFunctionSamePE`), plus `CountOnBits`.
Fixed a real pre-existing header bug on `GetRTParmShortNameStringPtr`
(missing `regparm(3)`, wrong `unsigned short` return type — ground truth
returns a raw string-table pointer, `const char *`).

One real KAT-caught bug in this pass's OWN new code: `GetRTParmShortNameStringPtr`'s
first draft read the class's packed 32-bit string-table pointer field as
a native 8-byte pointer, segfaulting the KAT host immediately — same bug
class already fixed once in this exact class's setter, in the PRIOR pass.
Fixed via the established packed-32-bit read/cast convention.

Also discovered: `RTParm_menu_ge_*`/`RTParm_menu_pe_*` (this project's
own tables) are zero-initialized placeholders, not populated with real
ground-truth `.rodata` bytes — a pre-existing scope boundary. The new
`LimitRTParmEditValuesRow` KAT pokes its one needed descriptor record
directly rather than assuming real linked-in content, after a real-bytes-
based first attempt produced a false failure against otherwise-correct
code.

`make verify` full suite green (0 FAIL) and a real `make ko-clean && make
ko KDIR=/home/build/linux-kronos` build both green; `nm OA.ko | c++filt`
confirms all 7 new symbols' mangled names match ground truth exactly.
Manifest 3529 -> 3536/21,689 (+7, 0 regressions, verified via a
`git stash`-isolated before/after full name-set diff). No new
`DECOMPILE_ERRORS.md` entry — no compile/link blocker this pass.

Real-HW test that would help: none identified — same reasoning as the
parent batch above, these remain internal KARMA table-lookup/edit-value-
clamp helpers with no single-path audio/UI-observable effect.

## CSTGControllerRTData::SendKarmaCCToKG — dead stub replaced with real body, round 44 (2026-07-29, solo)

Session hit its 200-subagent dispatch cap mid-round-42; this round done
solo (no `Agent` tool). `bar2_stubs.cpp` had carried an empty-body
stand-in for this symbol since 2026-07-24 (needed as a link target once
`CSTGFrontPanel::HandleTouchPanel` started calling it for real), explicitly
justified at the time as "safe and inert for kronos_vm boot-testing
purposes" since kronos_vm has no real KG audio DSP core attached. That
justification was correct for VM-boot-testing scope, but it IS a real
functional gap on real hardware: every real caller (`HandleTouchPanel`,
`ButtonPressHandler`, `AnalogControllerHandler`, several
`CKGControlMsgHandler` message handlers) was silently doing nothing
whenever it tried to forward a KARMA-pad realtime CC value to the KG
engine.

Now real (`.text+0xd720`, 80 bytes, confirmed via `objdump -dr`):
sends a 5-byte MIDI-CC-shaped message `{channel|0xb0, ccNo, value, 0x05,
0xff}` via `CSTGMidiQueueWriter::Write()` on the embedded queue-writer
sub-object at `CSTGMidiPortManager::sInstance+0x208` — same overall shape
as `global.cpp`'s own `SendGlobalMidiMessage()` helper (reused for
`UpdateKeyTranspose`/`UpdateLocalControl`), but with the 2nd byte ALSO
variable (the CC number, not `SendGlobalMidiMessage`'s fixed `0x79`), so
that exact helper wasn't reusable verbatim. A real, notable detail: `this`
(eax on entry, this project's regparm(3) convention) is NEVER read
anywhere in the real body — confirmed via KAT (test [3],
`test_controller_rt_data_send_karma_cc.cpp`, constructs the object at a
deliberately poisoned/unconstructed address and confirms the result is
unaffected). The channel byte comes entirely from `CSTGGlobal::sInstance
[+0x6b8]` instead.

`make verify` full suite green (204 test binaries, 0 FAIL) and a real
`make ko-clean && make ko KDIR=/home/build/linux-kronos` build both green;
`nm OA.ko | c++filt` confirms the new symbol links. Manifest count
unchanged (3539/21,689) — `gen_oa_manifest.py`'s name-matching heuristic
had already credited the dead STUB as "reconstructed" purely by symbol-
name presence (a known, pre-existing limitation of the by-name-only
match tier, not something this batch introduced); internally the entry
moved from "by name only" to the higher-confidence "by address AND name"
tier (89/2338/1112, was 89/2339/1111).

Real-HW test that would help: with a real KG audio DSP core attached,
touching a KARMA realtime-controllable front-panel knob/pad/switch should
now produce an audible/observable CC-driven effect that a VM-only build
could never have exercised — this was previously a guaranteed silent
no-op on real hardware too (the stub, not just the VM harness).

## CKGEngine::ProcessForSeqWhenChangingGE — round 45 (2026-07-29, solo)

Session-wide 200-subagent dispatch cap hit; continued solo. Considered
`AssignRTParmFunction_Drm` (8294B, twice-deferred) again via the same
native-execution-harness technique that cracked `IsRTParmFunctionSameGE`
— rejected this time: 196 relocations and multiple real external calls
within its body make it a genuinely stateful function, not a pure leaf
safe to mmap+call directly. Also spot-checked `CSTGControllerRTData::
ResetKnobsJumpCatch()` (2368B) — confirmed real but genuinely dense (an
8-way mode-jump-table dispatch INTO a long per-knob-index sequence, not
a simple unrolled loop); deferred rather than forced.

Landed instead: `CKGEngine::ProcessForSeqWhenChangingGE(int)`
(`.text+0x3accb0`, 186 bytes), already flagged in `oa_ckg_module_param_
msg_handler.h`'s own header comment as "trivial control-flow itself,
deferred only because its 2 real call targets are the still-deferred
`ChangeValuesInBackupWhenChangingGE()` overloads." Reconstructed it
anyway, linking against those 2 overloads as genuinely unresolved
externs (same "expected Unknown symbol at insmod" convention as any
other not-yet-real callee) — a real host KAT mock (already present in
`test_ckg_engine.cpp`, added for `SendChangeGEToEngine()`'s own earlier
batch) satisfies the link requirement for `make verify` without needing
either overload's own real body.

Two early-return no-op guards (`m_perfType==2`; the KARMA shared-memory
blob's own `+0x7234` byte=="2"), then a genuinely NON-mutually-exclusive
extra indexed update (index read from `CKGBankManager::ms_poInstance
[+0x97c7d4]`, the SAME real field `oa_karma_seq_backup.h`'s own
`CKGSeqBackupCommonParam`/`CKGSeqBackupModuleParam::GetValue()`
independently confirms) gated on `CKGUIMsgProcessor::ms_poInstance
[+0x74]`, ALWAYS followed unconditionally by a "default" update —
confirmed via the real disassembly's own `jmp` back into the middle of
the default-path instruction sequence (not a separate return), meaning
BOTH updates fire when the msgProcessor gate is set, not just the
indexed one.

Real host KAT (new section in `verify/test_ckg_engine.cpp`, 10 checks)
covers all 4 real paths: gate-clear (1 call), gate-set (2 calls, last
one confirmed to be the default), and both no-op guards independently.
`make verify` full suite green, real `make ko-clean && make ko
KDIR=/home/build/linux-kronos` build green, `nm OA.ko | c++filt`
confirms the new symbol's mangled name matches ground truth exactly.
Manifest 3539 -> 3540/21,689 (+1, 0 regressions).

Real-HW test that would help: none identified — this is an internal
KARMA-sequencer backup-record bookkeeping path with no directly
observable audio/UI effect on its own.

## MemoryModProcFileOp_{open,close,lseek,mmap,ioctl,write,read}, solo round 46 (2026-07-29)

Continuation of solo mode. Pulled the 7-function `MemoryModProcFileOp_*`
cluster from `/home/share/Decomp/oa_export` (a tight, self-contained
`/proc` file-ops family, ~783 bytes total) — the real handlers behind a
`.data`-resident named struct, `MemoryModProcFileOps` (`.data+0x6da8a0`).

**Found while landing this**: `InitSharedMemProcInterface` (already
reconstructed, `shmemproc_init.cpp`) wires `/proc/.shm`'s `proc_fops` to
this SAME real `MemoryModProcFileOps` global — NOT the invented
per-mode-page virtual stand-in that file's own header comment already
flags as a documented software substitute. Cross-checking against
`CSTGHandle::Access()`'s own already-reconstructed real call sequence
(Eva's `stg_handle.cpp`) confirms the REAL protocol: `mode` is a literal
`CSTGHeapManager` handle number, not an enum of fixed shared-memory
kinds — `ioctl(fd,0x64,mode)`→`GetHandleOffset`, `ioctl(fd,0x65,mode)`→
`GetHandleSize`, then `mmap()` computes a PHYSICAL address
(`vm_pgoff*PAGE_SIZE + sPhysicalHeapBase`) and `remap_pfn_range()`s it
directly — genuine zero-copy shared memory between OA.ko's kernel-side
STG heap and Eva's userspace mapping of the SAME physical page. NOT
swapped into `InitSharedMemProcInterface` this round: doing so needs the
6 `CSTGHeapManager_*` alloc/free/resize/defragment/reserve callees (none
reconstructed yet, declared here as genuinely unresolved externs) AND
re-verifying kronos_vm boot, since the existing virtual stand-in is
itself load-bearing for that exact scenario (see its own header
comment). Left as a documented, deferred cross-reference for a future
round, not silently dropped.

Also found and fixed, incidentally: `stgheap_init.cpp`'s own
`sAlignedHeapBase`/`sPhysicalHeapBase` naming bug (needed to get the
right value into this batch's `mmap` formula) — ground truth has TWO
distinct real globals here; an earlier pass named its single local after
the misleading printk format string ("AlignedHeapBase") while actually
computing and storing the OTHER global's (`sPhysicalHeapBase`) formula,
leaving ground truth's real `sAlignedHeapBase` (the raw
`CSTGHeapManager_Initialize()` return value) unmodeled under any name.
Fixed: renamed accessor to match ground truth, added a new
`stgheap_get_physical_heap_base()` accessor for the value this batch
actually needs. Purely a naming fix — the printk's own real argument
(`sPhysicalHeapBase`, confirmed directly in ground truth's own decompile,
not the string's literal wording) is unchanged, so no behavioral
regression.

**Real bug caught by the KAT, not by inspection**: `MemoryModProcFileOp_mmap`'s
first draft read `vm_start`/`vm_end`/`vm_page_prot` as `unsigned long`
(8 bytes on this 64-bit host) instead of the real x86-32 target's 4-byte
fields — silently reading garbage across adjacent struct fields. Caught
when the host KAT's `remap_pfn_range` mock received nonsense
addr/size/prot values; fixed to `unsigned int` reads, matching
`file_io.cpp`'s own already-established host/target pointer-width
caveat for exactly this bug class.

**Inferred, not directly decompiled** (documented, not fabricated):
`MemoryModProcFileOp_write`/`read`'s own `copy_from_user`/`copy_to_user`
calls and `mmap`'s own `remap_pfn_range` call are each decompiled with
FEWER visible arguments than their real Linux 2.6.32 prototypes take —
Ghidra failed to recover every register-passed arg under this
`-mregparm=3` build. The missing args (kernel-side `to`/`from` pointer =
`sIORemapBase + f_pos`; `remap_pfn_range`'s `vma`/`addr`/`pfn`) are
filled in from the well-known real kernel API contract, not guessed;
every OTHER piece of control flow (the 64-bit-safe bounds check, the
manual carry-preserving 64-bit `pos += count`, the exact ioctl case
dispatch including which cases dereference `arg` as a pointer vs. use it
as a raw integer) is a literal, unmodified transcription of the real
decompile.

Real host KAT (`verify/test_memorymod_procfileop.cpp`, 11 sections)
covers open/close/all 3 lseek origins/lseek error paths/mmap success+
failure/all 9 ioctl cases/write+read success and out-of-bounds paths
(the OOB cases use a valid seek near the mocked heap's end whose COUNT
then overflows — a direct out-of-bounds seek is itself rejected by
lseek's own bounds check, exercised separately). `make verify` full
suite green (223 targets, 0 failures), real `make ko-clean && make ko
KDIR=/home/build/linux-kronos` build green, `nm OA.ko` confirms all 7
function symbols plus `MemoryModProcFileOps` itself. Manifest 3540 ->
3547/21,689 (+7, 0 regressions).

Real-HW test that would help: none directly (this cluster isn't wired
into any live proc entry yet), but a future round that DOES complete the
`CSTGHeapManager_*` family and swaps it into `InitSharedMemProcInterface`
should re-run the full kronos_vm boot-test sequence before trusting it —
the existing virtual stand-in's own header comment documents the exact
Eva-segfault-on-`/proc/.shm`-mmap regression a broken swap could
reintroduce.

## CDrumButtonLED + M3RPPRGlue_*LED + CSPRUIMsgSender::*DrumTrackLED, solo round 47 (2026-07-29)

Continuation of solo mode. Landed a complete, self-contained 3-layer
vertical slice through the front-panel drum-track button LED control
path (11 functions total), found via a fresh class-inventory sweep:

```
CDrumButtonLED::start()/wakeup()/sleep()/initialize()
  -> M3RPPRGlue_TurnOnLED/TurnOffLED/BlinkLED()
    -> CSPRUIMsgSender::TurnOnDrumTrackLED/TurnOffDrumTrackLED/
       BlinkDrumTrackLED()
      -> SKSTGGate_SendToUI(CSKMessage const*)   [unresolved extern,
                                                   real relocation-
                                                   confirmed symbol,
                                                   same treatment as
                                                   KGOutGate_
                                                   SendMessageToUI]
```

`CDrumButtonLED`'s own state: a single byte field (`mState`), confirmed
via `initialize()`/`wakeup()` both writing 0, `sleep()` writing 1, and
`start()` gating `TurnOnLED()` behind `mState==0`. `stop()` is `__cdecl`
in ground truth (touches no member state at all) -- modeled as `static`
to match. `CSKMessage` reused from the already-established
`oa_ckg_control_ui_msg.h` (`raw[0x30]` opaque-payload convention).

Real bug/quirk found and faithfully preserved, not "cleaned up": each of
the 3 `CSPRUIMsgSender::*DrumTrackLED()` senders builds an IDENTICAL
28-byte payload with a genuine 4-byte GAP at offset +0x0c that ground
truth's own disassembly never assigns (`local_24` at offset 8 is only 4
bytes, but the next local, `local_1c`, starts 8 bytes later at offset
16 -- an 8-byte-apart pair of "4-byte" locals, meaning 4 bytes of real
stack memory sent to `SKSTGGate_SendToUI()` are genuine uninitialized
garbage in ground truth). This reconstruction zeroes that gap (rather
than reproducing nondeterministic garbage) purely so the reconstruction
itself has no undefined behavior -- functionally immaterial since
nothing downstream of the unresolved `SKSTGGate_SendToUI` extern is
modeled in this project.

Real host KAT (`verify/test_drum_button_led.cpp`, 5 sections) exercises
the full state machine (initialize/start/sleep/wakeup/stop) and
independently verifies every byte of the built `CSKMessage` payload for
each of the 3 real command ids (0x2e/0x2f/0x30). `make verify` full
suite green (224 targets, 0 failures), real `make ko-clean && make ko
KDIR=/home/build/linux-kronos` build green, `nm OA.ko | c++filt`
confirms all 11 new symbols' mangled names match ground truth exactly.
Manifest 3547 -> 3558/21,689 (+11, 0 regressions).

Real-HW test that would help: pressing the physical drum-track button
on a real Kronos and confirming the LED lights/blinks/turns off exactly
per this state machine would be the natural real-hardware confirmation,
though nothing about this reconstruction is currently wired into a live
call path from real front-panel input (the caller of
`CDrumButtonLED::start()`/`sleep()`/`wakeup()` itself is not yet
traced).

## CSTGGlobal::GetNumParams/GetParamDescriptors/GetMessageHandlers/GetValueGetters, solo round 48 (2026-07-29)

Continuation of solo mode. Landed the 4 framework metadata accessors
CSTGGlobal has been missing since this class's own header was first
opened (its header comment already flagged `GetNumParams()`/
`GetParamDescriptors()` as "genuine OWN virtual slots... own body
deliberately NOT reconstructed" -- that earlier note conflated "the
function body is trivial" with "the backing table contents are
unrecovered"; only the latter is true). Same trivial-return shape/
precedent already established for `CSTGLFO`'s own 4 sibling methods
(`oa_lfo.h`/`lfo_component.cpp`): `GetNumParams()` returns the literal
`0x6e` (110), `GetParamDescriptors()`/`GetMessageHandlers()` return
pointers into 2 real, relocation-confirmed `.data`/`.bss` tables
(`STGGlobalParams`, `_ZN10CSTGGlobal16sMessageHandlersE`), contents out
of scope (same "framework table, not modeled" precedent as CSTGLFO's
own `STGLFOParams`).

`STGGlobalParams`'s declared size (5720 bytes = 110 x the confirmed
52-byte `CSTGParamDescriptor` stride, independently cross-checked
against `CSTGLFO`'s own `STGLFOParams`: 1092/21 = 52 exactly) was
derived rather than taken at face value from ground truth's own
byte-level symbol labeling, which stops 3 bytes short (5717,
`STGGlobalParams[0]`..`[5716]`) -- 5717 isn't evenly divisible by the
confirmed stride while 5720 is exactly 110x, strong evidence the
labeling is a Ghidra artifact, not a real 5717-byte array.
`sMessageHandlers`'s 896-byte size, in contrast, IS a directly measured
ground-truth boundary (gap to the next real symbol, `kBankInfo`), not
extrapolated.

Confirmed real difference from `CSTGLFO`: `CSTGGlobal::GetValueGetters()`
is a genuine literal `return 0` -- no value-getters table exists for
this class in ground truth (unlike `CSTGLFO`'s own, which returns a
real table pointer).

Also surveyed `STGAPIFrontPanelStatus`'s own 4 pending methods
(`SetEffectThreadUsage`/`SetCPUStaticFrontUsage`/`SetCPUVoiceModelUsage`/
`SetCPUStaticBackUsage`) as a second small candidate this round --
deliberately NOT reconstructed: their decompiled bodies reference oddly
-named symbols (`STGOrganModelFrontVars_mDelayLine328` etc, an organ-
model-instrument name that doesn't fit a CPU-usage-tracking class) and
use raw `in_EAX`/`in_EDX`/`in_ECX` register aliases that don't match
their own declared `param_1`/`param_2`/`this` signature -- a real,
unresolved register-to-parameter mapping ambiguity that needs raw
disassembly tracing, not just the auto-decompile, to resolve safely.
Logged here rather than guessed at.

Real host KAT (new section in `test_global.cpp`, using the file's own
`check_eq` helper) confirms all 4 return values, including exact address
equality for the 2 table-pointer accessors. Also had to add matching
link-satisfying array definitions to `test_engine.cpp`/
`test_global_ctor.cpp` (both also link `global.cpp`). `make verify` full
suite green (224 targets), real `make ko-clean && make ko
KDIR=/home/build/linux-kronos` build green, `nm OA.ko | c++filt`
confirms all 4 mangled names match ground truth exactly. Manifest 3558
-> 3562/21,689 (+4, 0 regressions).

Real-HW test that would help: none identified -- pure framework
metadata with no observable I/O or state.

## OA.ko: CFileStream (concrete CStream file-I/O adapter), solo round 49 (2026-07-29)

Continuation of solo mode. Fresh manifest survey (`manifest/oa_functions.
csv`) picked `CFileStream` (16 pending methods) as a coherent,
self-contained cluster -- the disk-backed `STGStream::CStream`
implementation used throughout the file-chunk import subsystem
(`CKorgFileKMP`/`CKorgFileKSF`/`CMultisampleChunk`/`CSampleChunk`/etc,
all already referencing `CFileStream&` in their own mangled signatures).
Landed 18/19 tractable methods: all 16 `CFileStream` methods except
`SetPositionBeginning`, plus 3 new small `CSTGFile_*` primitives
(`file_io.cpp`) that `GetPosition`/`IsAtEnd`/`Flush` needed and that
were themselves separately pending in the manifest.

Confirmed real object layout (regparm(3): `this`=EAX, ground truth's own
decompile shows it as an unused declared param with the real body
reading an `in_EAX` pseudo-variable -- same gotcha as
`oa_ckg_midi_msg_handler.h`): +0x00 vptr, +0x04 error flag (0=ok,
7=generic-I/O-error -- but see the Write/Read asymmetry below), +0x08 a
self-pointer set once in the ctor and never read by any method this
round reconstructs, +0x0c the raw `CSTGFile_Open()` handle.

**`SetPositionBeginning()` deliberately deferred** -- its real body is
`(**(code**)(*in_EAX + 0xc))()`, a genuine virtual dispatch through
CFileStream's OWN vtable (confirmed via `_ZTV11CFileStream`@006c0720 in
symbols.csv: `*in_EAX` == `PTR__CFileStream_006c0728` == slot0, +0xc ==
slot index 3). Almost certainly `this->SetPosition(0)` given the other
3 `SetPosition*` siblings' own semantics, but resolving WHICH named
method occupies vtable slot 3 needs the full 22-slot `STGStream::CStream`
base interface reconstructed first (`_ZTVN9STGStream7CStreamE`@006c07e0,
NOT yet touched) to establish slot-index-to-method-name ordering -- out
of scope for this batch. Left undeclared/uncredited rather than guessed
at, matching this project's "genuinely-unresolvable decompile
recognition" convention (same treatment as the 4 deferred
`STGAPIFrontPanelStatus` methods, sec 10.248-adjacent).

**`CSTGFile_GetPosition`/`CSTGFile_IsAtEnd` resolution, most interesting
find this round**: their ground-truth decompiles call a raw fixed
address `func_0x00d2b268(1)` with no visible file handle argument --
looked like an unrelated function at first glance. Directly diffing
against this file's own ALREADY-COMMITTED `CSTGFile_Seek`, whose own
decompile calls the exact SAME `func_0x00d2b268(param_3)` (its `whence`
arg), confirmed `func_0x00d2b268 == generic_file_llseek` (already
established/verified in this project). The literal `1` GetPosition/
IsAtEnd pass is `SEEK_CUR` -- both are the standard `lseek(fd, 0,
SEEK_CUR)` "tell" idiom, with `handle`/`offset=0` reaching
`generic_file_llseek` via unchanged EAX/EDX (same partial-visibility
decompile artifact already documented for `CSTGFile_Seek`). No guessing
involved -- a direct precedent match against already-verified code.

`CSTGFile_Flush`'s `file->f_op` dispatch at raw offset 0x34 cross-checked
against `/home/build/linux-kronos/include/linux/fs.h`'s own `struct
file_operations` field order (owner/llseek/read/write/aio_read/
aio_write/readdir/poll/ioctl/unlocked_ioctl/compat_ioctl/mmap/open/flush,
4-byte strides 0x00..0x34) -- offset 0x34 is exactly `flush`, and this
independently RE-confirms `CSTGFile_Read`/`_Write`'s own already-verified
`fOp+0x8`=read/`fOp+0xc`=write mapping is the correct struct.

**Own-bug caught before commit, `CFileStream::Copy`'s parameter order**:
first draft assumed `Copy(srcPath, dstPath)` by analogy with common
`cp`-style APIs, but ground truth's own decompile opens `param_2` READ
and `param_1` WRITE/CREATE/TRUNC -- i.e. the REAL signature is
`Copy(dstPath, srcPath)`, destination first. Also preserved 2 further
real quirks verbatim: a zero-byte source file returns success without
ever entering the copy loop, and if the source opens but the destination
fails to open, the source handle is STILL closed (only the destination's
close is gated on the destination having opened) -- an asymmetric
cleanup pattern, not "fixed" to a more symmetric one.

**`Write`/`Read`'s own error-flag asymmetry, preserved not "fixed"**:
unlike the ctor/`SetPosition*` family (which sets the sentinel `7` on
failure), `Write`/`Read` set a plain `(len != actual)` 0/1 boolean into
the SAME error-flag field -- caught by the test harness expecting `7`
and getting `1`, confirmed as a real ground-truth quirk (not a test or
implementation bug) by re-reading the disassembly's own `(uint)(in_ECX
!= iVar1)` cast.

Real host KAT: `verify/test_file_io.cpp`'s new `[10]` section (11
checks, mocked `generic_file_llseek`/`f_op->flush`) for the 3 new
`CSTGFile_*` primitives; new `verify/test_file_stream.cpp` (7 sections,
mocking `CSTGFile_*` directly, no `file_io.cpp` link) for `CFileStream`
itself. `make verify` full suite green (207 targets). Real `make
ko-clean && make ko KDIR=/home/build/linux-kronos` build green. Manifest
3562 -> 3580/21,689 (+18, 0 regressions; `SetPositionBeginning` correctly
still `pending`).

Real-HW test that would help: none identified -- pure VFS-wrapper logic
already exercised extensively by the sibling `CSTGFile_*` cluster's own
prior real-hardware-adjacent verification; no new hardware surface.

## OA.ko: CKGTimerManager (KARMA tempo/clock manager), solo round 50 (2026-07-29)

Continuation of solo mode. Manifest survey picked `CKGTimerManager`
(15 pending methods) -- KARMA's own tempo/clock manager, previously
only a 5-method stub (`oa_ckg_module_param_msg_handler.h`) discovered
while reconstructing `CKGEngine`'s own ctor, flagged then as "a real,
self-contained future cluster of their own." Landed 13/15: everything
except `Process()`/`AdvanceClock()`.

Confirmed real object layout, byte-for-byte -- `sizeof` comes out to
exactly `0x38` (56 bytes), matching the ctor's own confirmed
`operator new(0x38)` allocation size verbatim (a strong independent
cross-check that the field layout is complete/correct, not just
individually-plausible). A genuine ctor quirk preserved: `mCurrentTempo`
(+0x08) and `mLastElapsedTick` (+0x18) are NEVER initialized by the real
ctor -- zeroed here only for this reconstruction's own UB-safety, not
because ground truth zeroes them (same convention as
`CSPRUIMsgSender`'s 4-byte stack gap, sec 10.181-adjacent).

**Replaced, not duplicated, the pre-existing stub**: the old 5-method
`struct CKGTimerManager` in `oa_ckg_module_param_msg_handler.h`
(ChangePerformance/Process/StartSync/StopSync/ctor, all undefined)
INCORRECTLY declared `StartSync`/`StopSync` as instance methods --
ground truth's own decompile shows NO `this` parameter at all for
either (`cc=__cdecl`, unlike every genuinely-`this`-taking method here
which Ghidra always shows an explicit `CKGTimerManager *this` for, even
when the arg is otherwise unused) -- confirming both are genuinely
`static`. Fixed by replacing the whole stub with an `#include` of the
new full header; `Process()`/`AdvanceClock()` stay declared-but-
undefined (real callers, `CKGEngine::Update()`, still reference
`Process()`) -- this project's kernel-module link model tolerates an
unresolved internal C++ method symbol at build time exactly like any
other genuinely-unresolved extern (confirmed: `make ko` still links
clean with them undefined, since LKM partial linking never requires
full resolution -- that only happens, or doesn't, at insmod time).

`Process()`/`AdvanceClock()` deliberately NOT reconstructed -- genuinely
ambiguous register/stack allocation, distinct from the usual "this is
EAX" gotcha: both bodies read `in_stack_ffffffe0`/`in_stack_ffffffe4`/
`unaff_EBX` pseudo-variables (Ghidra's own markers for values inherited
from an unknown caller context, never assigned anywhere in the function
itself) to build a `CKGRTCHandler*` and call 2 of its methods plus a
`CKGEngine::IsKarmaOn(CKGEngine*, int)` call -- needs raw disassembly
tracing of the real caller, not just the auto-decompile, to resolve
safely. Both functions' OTHER logic (the fixed-point interval-clock
accumulator, tempo-LED countdown) is IDENTICAL to the already-
reconstructed `GetIntervalClock`/`ShouldTempoLEDFlash`, so nothing new
would be learned by guessing at the ambiguous part.

Real host KAT (`verify/test_kg_timer_manager.cpp`, 24 checks,
including a `sizeof()` cross-check against the ctor's own allocation
size) -- does NOT link the 1421-line `ckg_engine.cpp`, provides its own
minimal `CKGEngine::ms_poInstance`/`HaveAllModulesStopped()` mock
instead (same "test provides its own mocks" convention as
`test_ckg_engine.cpp` itself). `make verify` full suite green (208
targets). Real `make ko-clean && make ko KDIR=/home/build/linux-kronos`
build green. Manifest 3580 -> 3593/21,689 (+13, 0 regressions).

Real-HW test that would help: the tempo-LED flash cadence and
external-MIDI-clock-sync backlog draining would both be directly
observable on a real Kronos (front-panel tempo LED blink rate, MIDI
clock slaving) -- flagged for the eventual real-hardware verification
pass, not attempted here.

## OA.ko: CSTGKeyTrack (key-tracking DSP component), solo round 51 (2026-07-29)

Continuation of solo mode. Scripted survey of manifest/oa_functions.csv
for small, "no in_stack/unaff_/Could-not-recover" pending clusters
(re-derived the earlier "STG value-getter family" discovery method)
found `CSTGKeyTrack` (24 pending methods, ZERO ambiguous-register
markers in ANY of them per raw grep -- a genuinely clean class).
Landed 15/24; 9 deferred across 2 DISTINCT, independently-verified
reasons (see below) -- not guessed at either way.

Confirmed real object layout cross-validates 3 already-established
project conventions in one class: `_slotInfo` (+0x08) reuses the exact
SAME `CSTGComponentSlotInfo` struct/offset already confirmed by
CSTGADSRBase/CSTGLFO; `GetOutput`/`FreeVoice` independently re-confirm
the SAME "quad table" per-voice addressing formula
(`(note&3)+(note>>2)*0xcc0`) documented in oa_lfo.h; `InitializeQuad`
writes the SAME shared "no AMS source" default address
(`CSTGGlobal::sInstance+0x29c9fa0`) as CSTGADSRBase's own
`InitializeQuad`. `InitializeQuad`/`PrepareSubRateAddressFixupTable`
confirmed genuinely `static` (ground truth shows NO `this` parameter
at all, `cc=__regparm3`, unlike every real-`this`-taking method here).
`PrepareSubRateAddressFixupTable` reuses the already-declared
`CSTGSubRateAddressFixupTable` struct verbatim (own real body appends
exactly 1 entry per call, vs. CSTGADSRBase's own 8 -- confirmed via
direct disassembly, not assumed).

**2 distinct, independently-diagnosed deferral reasons** (own paragraph
since conflating them would be wrong): (1) `PrecomputeData`/
`UpdateLowRamp`/`UpdateMidLowRamp`/`UpdateMidHighRamp`/`UpdateHighRamp`
each make a genuine, fully-concrete virtual call through THIS class's
own vtable at raw offset 0xc0 (no "could not recover" warning, just no
independent confirmation of the target method) -- same class as
`CFileStream::SetPositionBeginning` (round 49). (2)
`ConvertIntRampToSlope`/`ConvertSlopeToIntRamp`/`CalculateKeyTracking`
(and its 3 callers `InitVoice`/`InitVoiceUsingInput`/`ProcessSubRate`)
are fully concrete, recoverable control flow that compares against or
multiplies by real named-but-unrecovered floating-point `.rodata`
literal constants (Ghidra's own `_DAT_006bab6c`-style placeholders) --
genuinely different from (1): not a vtable-slot ambiguity, a
missing-literal-value one, discovered only by reading past the control
flow into the actual comparison operands.

**Own host/target pointer-width bug caught before commit, twice**:
first, `InitializeQuad`'s pointer-slot writes (adjacent to int fields 4
bytes later in the REAL target's 0x30-byte struct) would have used
native 8-byte `void**` writes (this project's OWN established
CSTGADSRBase precedent) and silently overrun into the int block on
this 64-bit host -- caught by re-deriving the exact byte layout before
writing any code, not by a failing KAT. Second, `FreeVoice`'s summed
quad-table address needed an explicit truncate-to-`unsigned int`-then-
zero-extend (this project's established ToU32-style convention) since
naive `int`-width pointer arithmetic on a real host pointer would
silently truncate; caught the SAME way. Also caught a THIRD, more
subtle one only via a live segfault: the class's own `void*`-typed
vptr placeholder field (8 bytes on host) silently shifted every field
after it by 4 bytes relative to the real 32-bit target's own byte
offsets, breaking the test's own raw `+0x08` `_slotInfo` poke -- fixed
by using a 4-byte placeholder array instead (never dereferenced as a
real pointer, so no loss of fidelity).

Real host KAT (`verify/test_stg_key_track.cpp`, 21 checks, including a
`mmap32`'d mock voice-model table for the FreeVoice truncate/zero-
extend round-trip). `make verify` full suite green. Real `make
ko-clean && make ko KDIR=/home/build/linux-kronos` build green.
Manifest 3593 -> 3608/21,689 (+15, 0 regressions).

Real-HW test that would help: none identified -- pure DSP parameter-
modulation math, no observable hardware I/O surface.

- **CKGParamEdit, 101/132 tractable methods
  (`oa_kg_param_edit_rt_externs.h`/`kg_param_edit.cpp`/
  `kg_param_edit_ctor.cpp`), round 52, 2026-07-29 (solo, no
  subagents)**. Fresh manifest survey filtered to pending methods with
  no `in_stack_ffffffXX`/`unaff_EBX`/"Could not recover jumptable"
  warnings, sorted by class average size -- surfaced `CKGParamEdit`
  (132 pending methods, avg 56.7 bytes) as an unclaimed cluster. Nearly
  every method is a thin relay: forward its own arguments straight to
  an already-named `RT_*`/`KS_*` free function -- this is the "real
  caller" `oa_rtparm_pe_table.h`'s own header comment had flagged as a
  TODO ("verify meaning once a real caller of one of these RT_pe_...,
  RT_run, RT_qtz_..., ... is reconstructed").

  Only 5 methods touch real per-instance state (confirmed via
  `ClearSoloStatus`/`ResendSoloStatus`/`SendSolo`/`GetPadChangeSource`,
  plus a new explicit ctor replacing the compiler-provided trivial
  one): `mSoloStatus[4]` (+0x00, one status byte per KARMA module) and
  `mPadChangeSource` (+0x04, never written by any reconstructed
  method). Every other method's own `this` (delivered in EAX per this
  project's established regparm/thiscall-EAX convention) is cast
  DIRECTLY to a value and passed as the call's own 3rd/4th real
  argument (`(char)this`, `(uchar)this`) -- never dereferenced;
  translated faithfully by using the reconstructed method's own last
  explicit parameter in that slot.

  **Pre-existing forward declarations, not a fresh class**: a
  `struct CKGParamEdit` already existed in
  `oa_ckg_module_param_msg_handler.h` (round 27, CKGModuleParamMsgHandler
  family) -- caller-side-only method DECLARATIONS with bodies
  deliberately left unimplemented. This round fills in real bodies
  matching those EXISTING signatures (not this round's own
  freshly-read ground-truth ones, where they differ -- e.g.
  `SendChordMemNote` keeps the pre-existing `(int,int,int)` signature,
  not `(uchar,uchar,char)`) since already-compiled callers rely on
  them; 4 genuinely new methods with no pre-existing caller
  (`SendStartSeed`'s OWN 2-arg overload -- a different real address
  from the already-declared 3-arg one -- `DrumSwitchOn`,
  `DrumSwitchMode`, `RefreshLinkedSceneDisplay`) were added fresh.

  **Real latent bug found and fixed at the root, not worked around
  again**: `oa_ckg_module_param_msg_handler.h`'s own ~50-callee
  CKGEngine externs block declares `RT_run` with `extern "C"` linkage
  (deliberate, for its enum-widened neighbors) while
  `oa_rtparm_pe_table.h` declares the SAME real function with
  `extern "C++"` (independently verified against ground truth's own
  mangled relocation by that earlier round's table work) -- a genuine
  conflict, previously never triggered because no prior TU included
  both headers together (`src/engine/rtparm_ckgparamedit.cpp` had
  documented this exact conflict and deliberately sidestepped it
  rather than fixing it). This round's own `CKGParamEdit` needs both
  header domains simultaneously, so the conflict was unavoidable --
  fixed by having `oa_ckg_module_param_msg_handler.h` `#include
  oa_rtparm_pe_table.h` and drop its own duplicate `RT_run` decl,
  consolidating on the real, independently-verified linkage. Required
  updating 2 pre-existing test targets (`test_ckg_engine`,
  `test_rtparm_ckgparamedit`) whose own `RT_run` mocks were inside an
  `extern "C"` block matching the OLD declaration.

  The ctor was split into its own translation unit
  (`kg_param_edit_ctor.cpp`) since it is the ONLY method needing none
  of the ~35 new `RT_*`/`KS_*` externs -- `test_ckg_engine`/
  `test_rtparm_ckgparamedit` only need the ctor to link (neither calls
  any `SendXxx()`), and linking the FULL `kg_param_edit.cpp` into them
  would require mocking all ~35 new externs those tests don't
  otherwise touch.

  Deferred, 2 distinct reasons: (1) 21 methods with genuine
  `in_stack_`/`unaff_`/jumptable-recovery blockers; (2) 10
  "ForModuleControl"-adjacent GE methods, fully concrete with NO
  decompiler warning, but each writes through 2 unconfirmed
  `.data`/`.bss` symbols (`CSWTCH_69`, a 4-entry lookup table, and one
  of 2 giant per-model front-panel-variable arrays whose own names are
  clearly Ghidra's best-guess label for a much larger unrelated
  structure) this pass has no independent confirmation for -- left
  undeclared/uncredited rather than guessed at.

  Real host KAT (`verify/test_kg_param_edit.cpp`, 41 checks spanning
  every distinct behavioral category: state, empty-stub, plain relay,
  multi-call relay, global-counter-gated, discarded-return, literal-
  constant-selector family, 2-arg-overload, scene-matrix pointer-chain,
  bool-typed relay). `make verify` full suite green (207+ targets,
  zero regressions from the RT_run linkage fix). Real `make ko-clean
  && make ko KDIR=/home/build/linux-kronos` build green. Manifest 3608
  -> 3710/21,689 (+102, 0 regressions).

  Real-HW test that would help: none identified -- pure free-function
  relay forwarding, no new hardware I/O surface beyond what the
  already-declared `RT_*`/`KS_*` externs themselves would eventually
  need (KARMA library internals, out of this project's own stated
  scope).

## OA.ko: CSTGPatch (shared "XModelPatch" family default overrides), solo round 53 (2026-07-29)

Continuation of solo mode. Scripted survey of manifest/oa_functions.csv
for small, "no in_stack_/unaff_/Could-not-recover" pending clusters
found `CSTGPatch` (84 pending methods, avg 53.2 bytes -- smallest
average among the top 35 largest pending classes; 16/84 flagged by the
decompiler itself, 68/84 clean). Landed 34/68 clean methods; the
remaining 34 clean + all 16 flagged deferred across 3 DISTINCT,
independently-verified reasons (see include/oa_stg_patch.h's own
header comment for the full breakdown).

Confirmed real class relationship: `CSTGPatch` is the shared base for
the entire "XModelPatch" family already visible elsewhere in this
project's own manifest (`CSTGOrganModelPatch`, `CSTGPolysixModelPatch`,
`CSTGMS20ModelPatch`, `CSTGVPMModelPatch`, `CSTGPluckedModelPatch`,
`CSTGEPModelPatch`, `CSTGPCMModelPatch`, `CSTGAnalogSyncModelPatch`,
`CSTGPianoModelPatch`) -- confirmed via the dtor's own
`&PTR__CSTGParamsOwner_006c04a8` vptr write, the SAME real base-class
relationship and "opaque placeholder, not a real C++ base" treatment
already established for `CSTGKeyTrack` (round 51). Landed methods are
almost entirely this base class's own DEFAULT virtual override bodies
(trivial no-ops or fixed-constant returns) meant to be overridden by
the concrete "XModelPatch" siblings -- confirmed via 7+ spot-checked
ground-truth decompiles, all `cc=__cdecl`/`(void)` (Ghidra recovered
ZERO real parameters for any of them, despite each one's own
C++-demangled comment showing a much richer real signature that only
the OVERRIDING subclass would actually consume).

Two methods needed closer handling beyond plain constant/no-op
transcription:
- `GetDefaultContext()` -- a function-local-static "default patch
  message context" singleton whose real one-time-init guard is a
  single byte that decompiles as sharing storage with the same
  field's own vptr-slot low byte (a decompiler symbol-overlap
  artifact, same class as this round's own deferred
  GetVoiceDelay/UpdateVoiceDelay pair) -- reconstructed with a real,
  separate C++ static-init guard (`static bool s_inited`) instead of
  literally transcribing the corrupted byte overlap, which reproduces
  the INTENDED one-time-stamp behavior rather than the artifact. Its
  OTHER 11 fields are unconditionally reset to fixed defaults on every
  single call, confirmed and preserved. The real vtable-slot value
  stamped into +0x00 (`&PTR_IsLiveUpdate_006bf728`) is a genuinely
  unmodeled external data symbol -- stored as an opaque non-null
  sentinel, never dereferenced by any reconstructed caller.
- `CheckMatchingToneAdjustTargetParam` -- the real high-level C++
  signature comment's own claimed 2nd-parameter type (`unsigned
  char`) contradicts the decompiled body's actual use of that same
  register as a pointer base into a 4-field descriptor struct --
  reconstructed matching the ACTUAL decompiled parameter usage (a
  descriptor pointer), not the possibly-stale/mismatched doc-comment
  signature.

Deferred, 3 distinct reasons: (1) 16 methods flagged by the decompiler
itself (`in_stack_`/`unaff_`/"Could not recover jumptable") --
SaveParams, HandleCC, UseDefaults, the 158-byte HandleParamChange
overload, InitVoiceNotifyVector/Wave, SetupComponents, the 7
UpdateToneAdjustCommonXxx setters, GetRequiredVoiceInfo,
GetMultisampleIds. (2) ~33 methods making a genuine, fully-concrete
virtual call through an UNNAMED vtable slot -- either operating on a
`CSTGVoice&`'s own vtable (`NoteOff`/`Steal`/`FreeVoice`/
`UpdateUnisonSpread`/`SetMute`/`HandleThreadIdChanged`/the 53-byte
HandleParamChange overload) or the repeated "for each submodule index,
call submodule's own vtable method" loop shape
(`PrecomputeData`/`UpdateGlobalTune`/`UpdateTrackTune`/
`UpdateTrackBendRange`) -- the SAME deferral class already established
for CSTGKeyTrack's own `PrecomputeData`/`UpdateXxxRamp` family (round
51): fully concrete control flow, but no independent confirmation of
which named method occupies the target slot. (3) `GetTransposedNote`
-- decompiled body is a bare, unassigned `undefined4 in_ECX; return
in_ECX;` despite `cc=__cdecl`/`(void)` (Ghidra recovered no real
parameters and no assignment to `in_ECX` anywhere in the function) --
a genuinely unrecoverable return value (reads whatever happens to be
in a register the `__cdecl` ABI never defines as an argument-passing
register at this call shape), not a vtable-slot-naming problem like
(2) -- left undeclared rather than guessing a constant.

Real host KAT (`verify/test_stg_patch.cpp`, 28 checks spanning every
distinct behavioral category: constant-return family, no-op-void
family, 4-field descriptor compare with short-circuit verification,
static-singleton lazy-init + unconditional-per-call field reset).
`make verify` full suite green (208+ targets, zero regressions). Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos` build green.
Manifest 3710 -> 3745/21,689 (+35, 0 regressions).

Real-HW test that would help: none identified -- these are all
default/never-overridden-in-this-pass virtual bodies with no hardware
I/O surface of their own; real value would come from eventually
reconstructing one of the concrete "XModelPatch" siblings and
confirming these defaults are genuinely never reached on a live
instance (every real patch presumably always uses a concrete
override).

## OA.ko: CSTGMultibandDelay (4-band modulated delay effect), solo round 54 (2026-07-29)

Continuation of solo mode. Scripted survey of manifest/oa_functions.csv
for small, "no in_stack_/unaff_/Could-not-recover" pending clusters
found `CSTGMultibandDelay` (88 pending methods, avg 58.6 bytes --
smallest average among the top 35 largest pending classes; 3/88
flagged by the decompiler itself, 85/88 clean). Landed 60/85 clean
methods; the remaining 25 (24 clean + 3 flagged) deferred across 2
DISTINCT, independently-verified reasons.

Key structural discovery driving almost the entire round: EVERY landed
`UpdateBandXxx`/`UpdateBand{1,2,3,4}Xxx` method's own `this` register
is NOT a real object pointer -- ALL real per-instance effect state
lives in `*(CSTGEffectMessageContext*+0x18)` (confirmed via `in_EDX +
0x18` in every single landed method), and `this` instead carries
either nothing at all (the 4 hardcoded per-band variants, fully
specialized by the compiler with a literal baked-in offset) or the
runtime band index as a plain `int` (the generic `UpdateBandXxx(ctx,
val, band)` 4-arg overloads computing `base + band*stride`) -- the
SAME "this-smuggled extra argument" idiom already established for
CKGParamEdit (round 52), just with a per-band index instead of a
small enum/bool. Confirmed this shape holds identically across 10
distinct field families (Feedback/FeedbackDModIntensity/LFOType/
LFOFreq/Level/LevelDModIntensity/Pan/InputSource/HighDamping/
FeedbackSource), each with its own stride (4 bytes for everything
except the 2 DModIntensity families' own 8-byte stride) -- landed via
a small Python code-generator (not hand-transcribed per-method) given
the pattern's mechanical regularity, with every generated body
spot-checked against 2-3 independent ground-truth bands before
trusting the generator's own offset arithmetic.

`GetMessageHandlers()` is the one odd framework accessor: its real
body returns `sMessageHandlers + 0xc`, not the bare `sMessageHandlers`
CSTGKeyTrack's own version returns -- this class's own entries
evidently start 0xc bytes into the shared table, a genuinely different
but equally mechanical real behavior, not a bug.

Deferred, 2 distinct reasons: (1) 3 methods flagged by the decompiler
itself (the real 1267-byte ctor, 129-byte `Init`, 1322-byte `Run` --
almost certainly the actual per-sample DSP process function). (2) 24
methods with fully concrete control flow but each reads one of 6 real,
named-but-unrecovered `.rodata` float-literal symbols
(`_DAT_006bbda0..dc`, `_DAT_006bbdc0`) -- the SAME "missing-literal-
value" deferral class already established for CSTGKeyTrack's own
`ConvertIntRampToSlope`/`ConvertSlopeToIntRamp` (round 51):
LowDamping/Time/LFOPhase/LFODepth (5 each, band1-4 + generic) plus the
3 `UpdateCrossoverFreq{12,23,34}` and `CalculateCrossoverCoefficients`
(the latter confirmed via its OWN `this` register carrying a smuggled
`float` argument -- the SAME idiom as the `int band` smuggling, just a
different value type).

Real host KAT (`verify/test_stg_multiband_delay.cpp`, 30 checks
spanning every distinct field-family shape: plain u32-copy at both
strides, bool-negate, float-1-minus, 4-way piecewise int-constant, the
2 non-banded singles, the 2 broadcast-to-4-fixed-offsets singles, and
a spot-check sweep of the remaining plain-copy families). `make
verify` full suite green (209+ targets, zero regressions). Real `make
ko-clean && make ko KDIR=/home/build/linux-kronos` build green.
Manifest 3745 -> 3806/21,689 (+61, 0 regressions).

Real-HW test that would help: none identified -- pure per-instance
field-write logic operating on an opaque effect-context data blob, no
hardware I/O surface of its own; the deferred `Run()` (the actual
per-sample DSP loop) would be the natural next target if audio-DSP
fidelity were ever brought into this project's scope (currently
explicitly out of scope, see kronos_project_scope_boundaries).

## OA.ko: CSTGProgramModeDrumTrackSlot (32-method batch), solo round 55 (2026-07-29)

Continuation of solo mode. Scripted survey of manifest/oa_functions.csv
for small, "no in_stack_/unaff_/Could-not-recover" pending clusters
found `CSTGProgramModeDrumTrackSlot` (38 pending methods, avg 21.6
bytes -- smallest average among the top 70 largest pending classes;
already a real, extensively-documented C++ class with 4 pre-existing
methods from earlier rounds, not a fresh discovery). Landed 32/35 clean
methods (3 flagged by the decompiler itself, deferred).

Confirmed two distinct real sub-object pointer chases, both landed
methods split cleanly along this line: (1) a genuine UNALIGNED 4-byte
pointer read at `this+5` (no `in_stack_`/`unaff_` warning, not a
decompiler artifact -- independently cross-confirmed by an ALREADY-
PASSING pre-existing round-47 test, `[54] CSTGProgramSlot::
ChangeProgram`, whose own check `"this+0x5 == newProgram (drum-track
slot)"` shows the base class's own `ChangeProgram()` is the real
writer of this exact field), used by the "chord"/"input channel"
accessor family (`GetChordSource`/`GetChordMode`/
`GetInputChannelSelect`/`GetInputBus`); and (2) the already-documented
assigned-drum-program pointer at `this+0xe8` (`ChangeDrumTrackProgram`,
round unknown/earlier), used by the EQ/level/bus accessor family --
every field in that 2nd family is a raw offset INTO the assigned
`CSTGProgram` object itself (an opaque, not-yet-reconstructed sibling
class, ~156 pending methods of its own), not this slot's own
per-instance state. `AccessToneAdjust()` is a confirmed real quirk:
returns a pointer into the ASSIGNED PROGRAM's own tone-adjust data
(`this+0xe8` sub-object +0xc4d), NOT this slot's own embedded
`CSTGToneAdjust` at `this+0x7f` (`CSTGProgramSlot`'s own ctor comment)
-- preserved faithfully, not "fixed".

Added 6 new real-but-content-unread lookup-table symbols this round
introduced dependencies on (`CSTGBusInfo::kInputSourceBusId`,
`STGAPIOutToPhysBusId`, `STGAPIOutToBusType`,
`STGAPIFXCtrlToWritePhysBusId`, `STGAPIHDRPhysBusIds`,
`STGAPIHDRBusTypes`) -- same "confirmed real, content not
independently confirmed" treatment already established for this
project's other unread `.rodata` tables, sized 16 as a safe (not
independently confirmed) upper bound.

`~CSTGProgramModeDrumTrackSlot()` -- both D0/D2 variants byte-
identical, landed as ONE real dtor (established convention); zeroes
both this class's own vptr-shaped field AND the embedded
`CSTGToneAdjust` sub-object's own vptr-shaped field at `this+0x7f`
(confirmed real, both variants) -- matches the SAME "opaque
placeholder, no real vtable pointer symbol independently declared"
treatment already established for CSTGKeyTrack/CSTGPatch/
CSTGMultibandDelay (the referenced `&PTR__CSTGParamsOwner_006c04a8`
symbol is never actually declared anywhere in this codebase, only
mentioned in comments -- discovered the hard way when an initial draft
tried to literally reference it and failed to compile).

Deferred, 1 reason: 3 methods flagged by the decompiler itself
(`InitVoice`, `OnUpdateProgramDrumTrackIgnoreSetListTranspose`,
`SetEffectiveTranspose`).

Real host KAT: appended a new "[round 55]" section directly into the
ALREADY-EXISTING `verify/test_global.cpp` (32 checks) rather than
creating a standalone test file -- `global.cpp` is one giant TU with
many externs that only `test_global.cpp` already has the full mock
infrastructure for (same class of problem already solved for OA.ko's
own CKGParamEdit round 52); a first attempt at a standalone test hit
the same "undefined reference to CSTGMidiPortManager::sInstance"-style
cascade that round 52 hit. A first attempt at the appended checks also
segfaulted from a real, previously-latent host-test-infrastructure gap
this round newly exercised: this class's own `this+5`/`this+0xe8`
sub-object pointers are stored as truncated 32-bit `ToU32`/`FromU32`-
style fields (this project's established host/target pointer-width
convention), so pointing them at plain `static` host arrays (whose
real 64-bit addresses can exceed 4GB on a modern host) truncates to a
garbage address on dereference -- fixed by using the file's own
already-established `mmap32()` helper (`MAP_32BIT`) for every buffer
these new checks point a packed field at, matching what every other
scenario in this same file already does.

`make verify` full suite green (209+ targets, zero regressions). Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos` build green.
Manifest 3806 -> 3840/21,689 (+34, 0 regressions).

Real-HW test that would help: none identified -- pure per-instance/
assigned-program field-read logic, no hardware I/O surface of its own.

## OA.ko: CSTGWaveSequence Update* setter family, solo round 56 (2026-07-29)

Extended the pre-existing `CSTGWaveSequence` class (already had a ctor
+ 34 `Getter*` methods from an earlier round, `Getter*` family in
`src/engine/stg_wave_sequence_valuegetters.cpp`) with dtor, 4
framework accessors (`GetNumParams`/`GetParamDescriptors`/
`GetMessageHandlers`/`GetValueGetters`), `IsStereoSequence`, and 20
`Update*(CSTGWaveSeqDataMessageContext&, STGConvertedParam&)` setters
in a new file, `src/engine/stg_wave_sequence_updaters.cpp`.

Methodology: every field offset used here is a CROSS-CHECK against
the already-confirmed sibling `Getter*` family, not a fresh
derivation -- read the full pre-existing `Getter*` implementation file
before writing a single `Update*` body, and every offset landed here
(`+0x4` bitfield, `+0x6`..`+0x13` whole-sequence fields, `+0x24`.
.`+0x47` ctx.index*0x34-scaled per-step-record fields) matches its own
already-confirmed `Getter*` counterpart exactly. Same cross-check
convention already established for CSTGProgramModeDrumTrackSlot
(round 55) and CKGParamEdit (round 52).

Two field-write shapes: (1) whole-sequence fields at a fixed offset
off `this` (`UpdateRunSequence`/`UpdateNoteOnAdvance`/
`UpdateTimeTempoMode` share a 3-bit RMW bitfield at `+0x4`;
`UpdateStartStep` through `UpdateLoopDirection` are plain fixed-offset
byte/short writes); (2) per-step-record fields at
`this + ctx.index*0x34 + <offset>` (`UpdateStepType` through
`UpdateReverse`, mirroring the `Getter*` family's own per-step
addressing using the SAME already-confirmed `CSTGWaveSeqDataMessageContext::index`
field and `0x34` record stride). `UpdateReverse` is its own 1-bit RMW
field, confirmed via the sibling getter's own mask.

`IsStereoSequence()` is a real loop over the per-step record array
(stride `0x34`), gated on `this[7]` (the same `EndStep` field
`UpdateEndStep` writes) as the step-count bound, checking each
record's own `+0x42` (`StepType`, the same field `UpdateStepType`
writes) and `+0x23` (a stereo-flag bit) -- landed verbatim from the
decompiled loop shape, not simplified.

`~CSTGWaveSequence()` -- zeroes the vptr-shaped field at `this+0..3`
raw (NOT a literal `&PTR__CSTGParamsOwner_006c04a8` reference -- that
symbol is never actually declared anywhere in this codebase, per the
round-55 lesson), same "opaque placeholder" convention as
CSTGKeyTrack/CSTGPatch/CSTGMultibandDelay/
CSTGProgramModeDrumTrackSlot.

Deferred, 3 reasons (see the class's own header comment in
`include/oa_global.h`): float-reinterpretation + unrecovered
`.rodata`-constant methods (`UpdateDuration`, `UpdateCrossfadeTime`,
`UpdateFadeInShape`, `UpdateFadeOutShape`, `UpdateSwingResolution`);
larger-scope methods needing external validation tables/vtable
dispatch not yet reconstructed (`UpdateBankSelect`,
`UpdateBankSelectUUID`, `UpdateMultisampleBank`); and one
unnamed-vtable-slot method (`Initialize`). Plus 4 methods the
decompiler itself flagged as bad.

Real host KAT: new standalone `verify/test_stg_wave_sequence_updaters.cpp`
(37 checks) -- this class's own files are self-contained TUs needing
only `oa_global.h`, confirmed by the pre-existing sibling
`test_stg_wave_sequence_valuegetters` test's own simple link line, so
no `test_global.cpp`-style shared-mock treatment was needed here
(unlike round 55's `CSTGProgramModeDrumTrackSlot`, which extends
`global.cpp` itself). One test-authoring bug caught by the test itself
on first run: the `IsStereoSequence` loop's real stop condition is
`StepType==0 AND (stereo-flag & 1)==1` (i.e. odd), not `StepType==0
AND (stereo-flag & 1)==0` as an initial test fixture assumed --
fixed by re-reading the decompiled `||`/`&&` shape and correcting the
fixture, not the implementation.

`make verify` full suite green (210+ targets, zero regressions). Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos` build green.
Manifest 3840 -> 3872/21,689 (+32, 0 regressions).

## OA.ko: CSTGProgramSlot STG value-getter batch (27 methods), solo round 57 (2026-07-29)

Extended the pre-existing `CSTGProgramSlot` class (already had 12
methods: ctor, `IsActive`/`AccessActiveSlotVoiceData`/
`HasActiveSlotVoiceData`/`HasActiveVoices`, `ChangeProgram`,
`GetProperMidiChannel`, `CompleteLoadProgram`, `Initialize`/
`UseDefaults`) with a fresh 27-method batch: 4 framework accessors
(`GetNumParams`/`GetParamDescriptors`/`GetMessageHandlers`/
`GetValueGetters`), 2 literal-constant-`1` overrides (`HasToneAdjust`/
`ShouldUseSlotEQSettings`), `AccessToneAdjust` (returns `this+0x7f`,
the already-confirmed embedded `CSTGToneAdjust` sub-object), a tight
EQ/bus-routing field cluster (`GetEQTrim`/`GetEQLowGain`/
`GetEQMidFreq`/`GetEQMidGain`/`GetEQHighGain` floats at `+0x48..+0x58`,
`GetEQBypass`/`GetUseDrumkitBusSettings` bitfields at `+0x43`/`+0x44`),
and a handful of plain field/array getters (`GetDetune`,
`GetAliasBankSelect`/`GetAliasProgramId`, `GetMeterIndex`,
`GetSendLevel`, `GetInputChannelSelect`, `UsesProgramChordSource`,
`GetMeterBus`, `GetOutputBus`/`GetOutputBusType`/`GetFXControlBus`/
`GetHDRBus`/`GetHDRBusType`).

Methodology: this project's regparm(3) ABI means `this` is passed in
EAX, and ground truth's own decompile for every one of these methods
shows it as an UNUSED declared parameter with the real body instead
reading a Ghidra `in_EAX` pseudo-variable (same gotcha already
documented in oa_stg_key_track.h/oa_ckg_midi_msg_handler.h/
oa_kg_timer_manager.h/oa_file_stream.h) -- confirmed, not guessed,
before writing a single method body. A 2nd integer arg (where
present, e.g. `GetSendLevel`/`GetInputChannelSelect`) reads from
`in_EDX`. `GetMeterBus()`'s own ground truth mangled name suggested a
1-arg signature, but its actual 9-byte body never reads a 2nd value
at all -- modeled as the true 0-arg method the body proves it to be,
not the possibly-stale demangled name.

`GetMeterIndex()`'s `+0x4` byte field is a CROSS-CHECK, not a fresh
derivation: it's the exact same field `ResolveActiveVoiceDataNode()`
(already landed, global.cpp) independently reads as `idx =
base[0x4]` from a completely different method family, now confirmed
a second time.

`GetOutputBus`/`GetOutputBusType`/`GetFXControlBus`/`GetHDRBus`/
`GetHDRBusType` reuse the SAME 5 real lookup-table symbols
(`STGAPIOutToPhysBusId`/`STGAPIOutToBusType`/
`STGAPIFXCtrlToWritePhysBusId`/`STGAPIHDRPhysBusIds`/
`STGAPIHDRBusTypes`) already declared+defined by round 55's
`CSTGProgramModeDrumTrackSlot` -- a cross-check reuse of an
already-confirmed symbol via `extern`, not a fresh table. New
`STGProgramSlotParams` table (this class's own param-descriptor
array) added with the same "confirmed real, content unread"
treatment as every other such table in this project.

Deferred, 2 reasons: 248 remaining methods (up to 3344 bytes) cover
genuinely separate large sub-areas (DSP voice-model dispatch,
controller/RPN handling, drum-kit bus routing) out of this round's
scope; a further subset decompiler-flagged.

Real host KAT (30 checks, verify/test_stg_program_slot_getters.cpp)
-- self-contained TU, no test_global.cpp-style shared mocks needed
despite CSTGProgramSlot's other methods living in global.cpp (this
batch lives in its own new file, stg_program_slot_getters.cpp).

`make verify` full suite green, zero regressions. Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos` build green.
Manifest 3872 -> 3899/21,689 (+27, 0 regressions).

Real-HW test that would help: none identified -- pure field-read
accessor logic, no hardware I/O surface of its own.

## OA.ko: CSTGProgramSlot Update*/GetValue* batch (58 methods), solo round 58 (2026-07-29)

Continued CSTGProgramSlot with a 58-method batch: a D0/D2 dtor pair
(byte-identical, landed as ONE), `OverridesProgramScale` (inverted
bit test), `GetDKitBus`/`GetDKitBusType` (reuse round 55's
`STGAPIOutToPhysBusId`/`STGAPIOutToBusType` tables), 21
`UpdateXxx(CSTGProgramSlotMessageContext&, STGConvertedParam&)`
setters, and 33 `GetValueXxx(CSTGProgramSlotMessageContext&)`
value-getters returning `STGConvertedParam&` via the shared
`CSTGParamsOwner::sValueGetterTemp` (same wrapping convention as
CSTGWaveSequence's `Getter*` family, round 56).

New `struct CSTGProgramSlotMessageContext` declared (minimal, only
the fields this cluster's own methods read): `ifxSlotIndex` (+0x4)
and `inputChannelIndex` (+0x18), two GENUINELY SEPARATE real int
index fields confirmed by their disjoint offsets, used by the 3
channel-select `UpdateXxx` setters. The sibling `GetValueXxx` family
instead receives a pointer that reads at the SAME byte offsets
`CSTGProgramSlot` itself already uses (confirmed cross-check: e.g.
`GetValueOutputBus`'s `+0x60` matches round 57's own `GetOutputBus`
index byte; `GetValueBankSelectEx2MSB`'s `+0xe` matches this round's
own `UpdateBankSelectEx2MSB` write target) -- modeled by reading the
passed reference at those literal offsets, matching ground truth
exactly regardless of whether the real caller passes `this` itself
or a same-layout view onto it. 2 new real-but-unread `.rodata` slope
tables added (`kKeyZoneSlopeTable`/`kVelZoneSlopeTable`).

REGRESSION FOUND AND FIXED THIS ROUND: landing the FIRST real body
for `CSTGProgramSlot::~CSTGProgramSlot()` broke 3 pre-existing verify
targets (`test_engine`, `test_global`, `test_global_ctor`) that link
`global.cpp` -- `CSTGProgramModeDrumTrackSlot`'s own dtor (round 55)
uses real C++ inheritance (`: public CSTGProgramSlot`), so the
compiler's IMPLICIT base-dtor call, previously silently unresolved
(never linked against a body), suddenly needed
`stg_program_slot_updaters.o`, which itself needed a second
transitive dependency: `CSTGParamsOwner::sValueGetterTemp` (a shared
static defined exactly once project-wide, in `adsr_base.cpp`, which
was NOT already linked into those 3 targets and pulls in its own
large unrelated dependency chain, `CSTGVoice::GetAMSSourceAddress`,
if added). Fixed by adding `stg_program_slot_updaters.cpp` to the 3
affected Makefile link lines and, instead of also linking
`adsr_base.cpp`, appending a local `STGConvertedParam
CSTGParamsOwner::sValueGetterTemp;` definition directly into
`test_engine.cpp`/`test_global.cpp`/`test_global_ctor.cpp` -- the
SAME local-copy convention already established by
`test_stg_wave_sequence_valuegetters.cpp` for this exact symbol.

Deferred, 1 reason: `GetWaveSeqSwingResolution()` forwards to
`CSTGProgram::GetWaveSeqSwingResolution` (a real but wholly
unreconstructed sibling class), deferred rather than guessed at.

Real host KAT (62 checks, verify/test_stg_program_slot_updaters.cpp).
`make verify` full suite green (all regressions from the dtor
landing fixed, zero net regressions). Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos` build green.
Manifest 3899 -> 3958/21,689 (+59, 0 regressions).

Real-HW test that would help: none identified -- pure field-read/
write accessor logic, no hardware I/O surface of its own.

Real-HW test that would help: none identified -- pure in-memory
sequence-data-structure field writes, no hardware I/O surface of its
own.

## Round 59 (OA.ko, solo, 2026-07-29): CSTGProgramSlot, 43-method
mechanical batch

Continuation of `CSTGProgramSlot`'s remaining pending methods.
Classified all 43 via a Python regex-driven scan of the pending
ground-truth `.c` files into 4 clean pattern families (script kept
at `/tmp/gen_round59.py`, same technique as round 54's
`CSTGMultibandDelay` batch generator and round 59's own predecessor
rounds):

- Class A (24 methods): single-bit test getters reading `ctx+0x43`
  through `ctx+0x47` (`GetValueMute`, `GetValuePriority`,
  `GetValueUseProgramScale`, `GetValueEnableProgramChange` and 20
  siblings) -- same byte range round 58 already confirmed via its
  own `UpdateIgnoreSetListTranspose`@0x46/`UpdateEnableRibbon`@0x45/
  `UpdateUseDrumkitBusSettings`@0x44 setters.
- Class B (13 methods): 4-byte int reads dual-written to both
  `STGConvertedParam::value` and `::displayValue` (the round-57-
  confirmed field at +0x18) -- offsets cross-checked against round
  57's own float getters at identical addresses
  (`GetDetune`=+0x1d, `GetEQTrim`=+0x48 both reused verbatim here).
- Class C (3 methods): `GetValueInputSource`/`GetValueInputChannelSelect`/
  `GetValueIFXDrumkitPatch` -- ctx-indexed signed-byte table reads,
  reusing round 58's exact `UpdateInputSource`/`UpdateInputChannelSelect`/
  `UpdateIFXDrumkitPatch` base offsets (0x5c/0x5e/0x63) with the
  ctx-relative `inputChannelIndex`/`ifxSlotIndex` fields as the index.
- Class D (3 methods): single-bit setters
  (`UpdateIgnoreSetListTranspose`, `UpdateEnableRibbon`,
  `UpdateUseDrumkitBusSettings`), same shape as round 58's `Update*`
  family.

`GetValueIFXDrumkitPatch` needed one manual classification fix (its
exact ground-truth formatting didn't match the initial regex);
confirmed correct by direct read of its body before inclusion.

All 43 bodies appended to the existing
`src/engine/stg_program_slot_updaters.cpp` (round 58's file, same
"extend the same file for the same class" convention used by Eva
round 51 for `control_surface.cpp`). Declarations appended to
`struct CSTGProgramSlot` in `include/oa_global.h`.

Real host KAT (43 new checks appended to
`verify/test_stg_program_slot_updaters.cpp`, 105 total in that
file). `make verify` full suite green, zero failures, zero
regressions (round 58's Makefile link-line fix already covers this
file). Real `make ko-clean && make ko KDIR=/home/build/linux-kronos`
build green. Manifest 3958 -> 4001/21,689 (+43, 0 regressions).

Real-HW test that would help: none identified -- pure field-read/
write accessor logic, no hardware I/O surface of its own.

## Round 60 (OA.ko, solo, 2026-07-29): CSTGProgramSlot, 15-method
batch (still 146 pending after round 59)

Re-surveying CSTGProgramSlot's pending backlog turned up 3 clean
families:

- Class A (6 methods): `GetValueEnableKnob2`..`GetValueEnableKnob7`,
  filling in the middle of round 59's `GetValueEnableKnob1`(bit0)/
  `EnableKnob8`(bit7) pair at ctx+0x47 -- same single-bit ctx-read
  shape.
- Class B (5 methods): `UpdatePriority`/`UpdateEnableProgramChange`/
  `UpdateUseProgramScale`/`UpdateProgVectorVolume`/
  `UpdateEQAutoLoadProgram` -- the missing WRITE-side counterparts of
  5 of round 59's own class-A `GetValueXxx` bit getters, confirmed
  by exact offset+bit cross-check (`UpdatePriority`@this+0x43 bit1
  pairs with round 59's `GetValuePriority`@ctx+0x43 bit1, etc.).
  Generalizes round 58/59's always-bit0 setter formula to an
  arbitrary bit position.
- Class C (4 methods): `GetMaxNumNotes`/`GetChordMode`/
  `GetWaveSeqKeySync`/`GetWaveSeqQuantizeTrigger` -- override-with-
  patch-fallback getters. Each reads a per-slot override field; if
  it's the "unset" sentinel, falls back to a byte on the patch
  object pointed to by `this+5`, using field offsets (0xc2a/0xc2b/
  0xc2f/0xc30) independently cross-checked as real via
  `AllocateVoice`/`AutoLoadDrumTrackEQ`/`Copy`/
  `CSTGPianoModelPatch`'s own ctor already using those exact same
  offsets elsewhere in this binary -- not guessed. `this+5`'s patch
  object is treated as an opaque raw pointer, same convention used
  throughout this project.

DEFERRED, 4 distinct reasons (full backlog still ~130 methods after
this round):
- `GetWaveSeqSwingAmount`: compares against a real float `.rodata`
  constant (`_DAT_006ba8e4`) whose actual value isn't cheaply
  recoverable from the available export data; deferred rather than
  guessing the threshold.
- `GetWaveSeqSwingResolution`: forwards to
  `CSTGProgram::GetWaveSeqSwingResolution`, a wholly unreconstructed
  sibling class (already flagged in round 58's log).
- `UpdateEnableSW1`/`SW2`/`EnableKnob2`..`8` (Update side):
  thunks to shared helpers `UpdateEnableAssignableSwitch`/
  `UpdateEnableAssignableKnob` (306B/310B), neither reconstructed
  yet -- good future-round target (2 methods would unblock 8
  thunks).
- `UpdateDetune`/`UpdatePitchBendRange`/`KarmaPitchBendRangeReset`:
  all three forward to `SetEffectiveDetune`(343B)/
  `SetEffectivePitchBendRange`(463B), neither reconstructed yet --
  another good future-round target (2 methods unblock 3+ callers).

Real host KAT (15 new checks appended to
`verify/test_stg_program_slot_updaters.cpp`, 120 total in that
file). Caught and fixed 2 test-authoring bugs during this round: the
new `GetWaveSeqKeySync`/`GetWaveSeqQuantizeTrigger` fallback-path
checks initially failed because round 58's own earlier
`UpdateKeySync`/`UpdateQuantizeTrigger` checks in the SAME test file
leave `this+0x3d`/`this+0x3e` nonzero and never reset them --
fixed by explicitly zeroing both before the new checks (not a bug in
the reconstructed implementation). `make verify` full suite green,
zero regressions. Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos` build
green. Manifest 4001 -> 4016/21,689 (+15, 0 regressions).

Real-HW test that would help: none identified -- pure field-read/
write accessor logic, no hardware I/O surface of its own.

## Round 61 (OA.ko, solo, 2026-07-29): CSTGProgramSlot, 3-method batch
(diminishing returns confirmed)

CSTGProgramSlot's remaining ~130 pending methods were re-surveyed for
this round, including 2 methods round 60's log had flagged as
promising future-round unlock targets
(`UpdateEnableAssignableSwitch`/`Knob`, `SetEffectiveDetune`/
`SetEffectivePitchBendRange`). On closer reading both turned out to
be poor picks after all:

- `UpdateEnableAssignableSwitch`/`Knob`'s own ground truth shows
  `*(int *)this == 0` as a branch condition -- nonsensical as a
  literal read of `this` (never null in a real member call) -- plus
  several `unaff_EBX`/`unaff_ESI`/`in_stack_ffffffXX` locals with no
  clear tie to the function's own declared parameters. This is the
  4-explicit-parameter case exceeding the 3-register regparm3/
  thiscall budget, and Ghidra's own register allocation for the
  spilled 4th argument looks genuinely confused, not just
  differently-shaped from what this project's usual `this`-in-EAX
  convention expects. Deferred rather than guessed at.
- `SetEffectiveDetune` chases a multi-level pointer through
  `CSTGGlobal::sInstance + 0x29c990c + index*0xc`, then two MORE
  levels of indirection, then two SEPARATE raw vtable calls at
  `+0xb6b`/`+0xb6f` -- genuinely deep, multi-subsystem logic, not
  the "simple setter" its name and round 60's framing suggested.

Landed the 3 methods that survived scrutiny instead:
`GetMIDIProgramBank` (reuses the already-reconstructed
`USTGAliasBankTypes::ConvertAliasPgmBankToMidiBank` -- ground
truth's own call site shows only the bankId argument explicitly, but
its own out1/out2 char& params are already sitting in the exact same
registers this function's own out-params arrived in, so they pass
straight through unchanged -- the same implicit-register-passthrough
convention already trusted elsewhere in this project),
`ShouldResendCCOnFilterChange`, and `ShouldSendSeqTrackMIDIOutput`
(both pure `CSTGGlobal::sInstance` + own-field reads, no new
dependencies).

Also surveyed and deferred: `ShouldResetChannelStripKnobJumpCatch`
(computes `this - index*0xe8` to address an unidentified sibling
array via negative offsets -- same risk class as round 58's
"backwards indexing," not confidently attributable without more
cross-referencing) and `ShouldStoreSeqValue` (its own mangled name
`ShouldStoreSeqValue(unsigned long)` disagrees in arity with its
Ghidra-declared 2-int-parameter C signature -- an internal
inconsistency, not just an unfamiliar shape).

**Standing conclusion for future rounds**: CSTGProgramSlot's
remaining backlog (~127 methods after this round) is now
qualitatively harder than rounds 57-61's -- most of what's left
gates behind unrecoverable `.rodata` constants/descriptor addresses
(GetUIInputTrim's `CSTGParamDescriptor` address family, 6 methods;
GetWaveSeqSwingAmount's threshold, round 60), genuinely deep
multi-subsystem pointer chains (the `0x29c990c` table family), or
Ghidra register-allocation confusion on 4-argument thiscall
functions. A future round should either accept this and cherry-pick
what remains, or pivot to a fresh class entirely.

Real host KAT (16 new checks appended to
`verify/test_stg_program_slot_updaters.cpp`, 150 total in that
file). Needed a new cross-TU link: `GetMIDIProgramBank` calls
`USTGAliasBankTypes::ConvertAliasPgmBankToMidiBank`, defined in the
separate `alias_bank_convert.cpp` (not previously linked into this
test) -- added to the Makefile's link line for this target, plus a
local `CSTGGlobal *CSTGGlobal::sInstance;` definition (this test's
own copy, following `test_alias_bank_convert.cpp`'s own established
precedent for that symbol). `make verify` full suite green, zero
regressions. Real `make ko-clean && make ko
KDIR=/home/build/linux-kronos` build green. Manifest 4016 ->
4019/21,689 (+3, 0 regressions).

Real-HW test that would help: none identified -- pure field-read
logic and an already-verified sibling-method reuse, no hardware I/O
surface of its own.

## Round 62 (OA.ko, solo, 2026-07-29): CSPRClockHandler, 13-method batch

`CSPRClockHandler` (sequencer transport-position singleton,
`include/oa_ckg_midi_msg_handler.h`) was ALREADY declared before this
round -- a minimal existing shape with `ms_poInstance`,
`ms_oStatusMaster` (as `unsigned char`), and 2 unimplemented 4-param
`GetCurrentLocation`/`GetPrecountLocation` declarations. Per the
standing instruction added after round 54's near-ODR-violation
(check `include/*.h` for an existing declaration before writing any
new class), this was caught immediately via grep and the EXISTING
declaration was extended rather than duplicated.

Landed 13 methods: `DisableStop`/`EnableStop` (increment/clamped
decrement on `this+0xc`), `DisableToProcessWhen1ClockUp`/
`EnableToProcessWhen1ClockUp` (counter+flag interplay on `this+4`/
`this+1`), `InitializeTempo` (writes `this+0x64`/`+0x68`/`+0x38`),
`ChangeTempoWhenStarting` (conditional copy gated on `this+0x60`/
`+0x6c`), `SetLocationInfoWhenStop` (writes `this+0x3c`, copies
`+0x20`/`+0x24` to `+0x40`/`+0x44`), `ModeOn` (zeroes
`ms_oStatusMasterTick` + `this+8`/`+0xc`), `CopyStatusLocalToMaster`/
`CopyStatusMasterToLocal` (round-trip through 2 new statics via a new
`CSPRTimerStatus{bar,tick}` struct), the 2-param
`GetCurrentLocation(int*,int*)` overload (reads `this+0x14`/`+0x18`),
and `HandleBarEventBackward` (byte-swaps a ushort at `event+6`,
compares SIGNED against `this+0x14`), plus the trivial
`_GLOBAL__I_ms_poInstance` (empty ctor stub, matches the existing
project convention for these).

**Genuine C++ overload, not a naming conflict**: manifest confirmed
`GetCurrentLocation` exists as 2 real, distinct addresses --
`003b3e00` (31B, 4-param) and `003b3e20` (13B, 2-param). Only the
2-param overload was implemented this round; the 4-param one remains
declared-only (deferred, gated behind `CSPRRTMIDIOutManager`-style
sibling state not yet reconstructed). Note: `gen_oa_manifest.py`
matches by name, not by exact signature/address, so it credits BOTH
overloads as done once either has a body -- a known tool limitation
(previously flagged, task #258), not a new one introduced here. The
4-param overload is genuinely NOT implemented; treat the manifest's
`GetCurrentLocation` line as one real body, one false credit.

**Type-widening, not a bug**: `ms_oStatusMaster` was declared
`unsigned char` by an earlier round based on a single `& 0x40`
bit-test caller. New ground truth showed 4-byte reads/writes through
the same relocation, confirmed via Ghidra's own "_"-prefixed
"overlaps smaller symbol" warning on two independent functions.
Widened to `unsigned int`; confirmed behavior-preserving for the one
existing caller (bit-test, unaffected by extra high bytes on
little-endian) and updated the one existing out-of-line definition
(`verify/test_ckg_midi_msg_handler.cpp`, definition-only there, never
read/written -- risk-free).

Deferred ~70 methods: `SendMIDIClock`/`SendStart`/`SendStop`/
`NotifyClockToRecorder`/`ProcessMetroneme`/`InvokeSPREngine`/
`RenewInformationOnCurrentTickBackward` and the rest forward into
wholly-unreconstructed sibling classes (`CSPRRTMIDIOutManager`,
`CSPRRecorder`, `CSPRMetronome`, `CSPREngine`) -- landing thin
wrappers around callees that don't exist yet would misrepresent
unreconstructed code, per the standing deferral pattern.

Real host KAT (new `verify/test_spr_clock_handler.cpp`, 20 checks,
registered in the Makefile following the existing
`test_ckg_midi_msg_handler` two-line pattern). `make verify` full
suite green (109 targets), zero regressions -- specifically checked
`test_ckg_midi_msg_handler` given the `ms_oStatusMaster` width
change. Real `make ko-clean && make ko KDIR=/home/build/linux-kronos`
build green. Manifest 4019 -> 4033/21,689 (+14: 13 real bodies + 1
name-matching false credit on the undeferred `GetCurrentLocation`
overload, see above).

Real-HW test that would help: none identified -- pure sequencer
transport state-machine logic, no hardware I/O surface of its own.

## Round 63 (OA.ko, solo, 2026-07-29): CSTGWaveSequence, 2-method batch

Surveyed several "near-complete" classes first (CKGModuleParamMsgHandler
4 pending, CKGCommonParamMsgHandler 3, CSTGControlMsgHandler 2, etc.) --
all turned out to be the standing "ctor/dtor/`HandleMessage()` real but
deliberately deferred" boilerplate this project already treats as a
closed category for every `*MsgHandler` class (documented in
`oa_ckg_module_param_msg_handler.h`'s own header comment), not a genuine
opportunity. `CSTGProgramModeDrumTrackSlot`'s 4 pending methods and
`CSTGKeyTrack`'s 11 all hit the established `in_EAX`/`in_ECX`/`in_EDX`-
alongside-a-declared-`this`-parameter register-confusion red flag (this
project's regparm3 convention is `this`=EAX/1st arg=EDX/2nd arg=ECX
*implicit*, not usually spelled out as `in_EAX` when Ghidra ALSO
declares a named `this` param in the same signature -- when both forms
appear together for the SAME register, it signals Ghidra's own binding
confusion, not a trustworthy read) plus several raw vtable dispatches
into unconfirmed slots (`+0xc0`, `+0x5c`, `+0xf0`) -- deferred.
`CSTGMultibandDelay`'s per-band `UpdateBandXLFOPhase/LFODepth/Time/
LowDamping` family (16 methods) was VERY close -- clean `this=EAX/
ctx=EDX/val=ECX` register attribution, no vtable dispatch -- but every
one divides/multiplies by an unrecovered `.rodata` float constant
(`_DAT_006bbda0`..`_DAT_006bbdbc`); the generic `UpdateBandXxx(...,int
band)` siblings additionally reuse `this`'s own register slot to smuggle
in the spilled 4th argument (band index) once the explicit-arg count
exceeds regparm3's 3-register budget -- same red flag as
`CSTGKeyTrack`. All deferred.

Landed the 2 that survived: `CSTGWaveSequence::UpdateDuration`/
`UpdateCrossfadeTime` -- write the exact same `+0x3e`/`+0x40` per-step
`short` fields the already-reconstructed `GetterDuration()`/
`GetterCrossfadeTime()` read (`stg_wave_sequence_valuegetters.cpp`),
confirmed byte-for-byte via cross-check rather than fresh derivation.
Clean `this=EAX/ctx=EDX(→ctx.index)/val=ECX` attribution, no dispatch,
no unrecovered constants -- the two clean survivors of an 11-candidate
family sample (`UpdateSwingResolution`/`UpdateDurationAMSSource`/
`UpdatePositionAMSSource`/`ValidateParamChange`/`Initialize` all hit one
of the same red flags above and stay deferred).

Real host KAT (2 new checks + a same-`ctx.index=2` no-clobber check,
appended to the existing `verify/test_stg_wave_sequence_updaters.cpp`,
now 27 checks). `make verify` full suite green (109 targets), zero
regressions. Real `make ko-clean && make ko
KDIR=/home/build/linux-kronos` build green. Manifest 4033 ->
4035/21,689 (18.604%).

Real-HW test that would help: none identified -- pure per-step field
writes, no hardware I/O surface of its own.

## Round 64 (OA.ko, solo, 2026-07-29): CSTGString, 8-method batch

`CSTGString` (105 `Get*` value-getters already reconstructed in an
earlier round) had 8 remaining pending methods, all explicitly
flagged "left pending" in this class's own header derivation notes
as a distinct, well-scoped family: the generic `CSTGParamsOwner`
reflection-API overrides (`GetId`/`GetName`/`GetNumParams`/
`GetParamDescriptors`/`GetMessageHandlers`/`GetValueGetters`/
`GetNumSubComponents`) plus `InitVoice`. Ground truth confirms all 6
reflection accessors are literal-constant/extern-array returns --
same exact shape already established for `CSTGWaveSequence`'s own
`GetNumParams`/`GetParamDescriptors`/`GetMessageHandlers`/
`GetValueGetters` (`stg_wave_sequence_updaters.cpp`) -- and
`InitVoice` is confirmed genuinely empty (`{ return; }`), matching
this project's standing "confirmed empty" convention (e.g.
`CFileMan::Setup`/`Config`/`Start`). `CSTGVoice`/
`CSTGVoiceInitialState` forward-declared only (not fully defined),
same precedent as `CSTGLFO::InitVoice` (`oa_lfo.h`) since the body
never touches either reference.

Deliberately NOT attempted this round (already-documented deferrals
in the class header, re-confirmed, not re-litigated): the 2 real
DSP-computation outliers (`GetPluckDelay`/`GetPluckDelayAMSIntensity`,
runtime-sample-rate float math, out of scope per this project's DSP-
fidelity policy), `GetNoiseSaturation` (real `fyl2x`-based log2 dB
conversion, same reason), and `GetSubComponent(unsigned short)` (a
genuinely different `__thiscall` calling convention and a branchy,
sub-object-pointer-returning shape, not part of either family above).

Real host KAT (8 new checks appended to the existing
`verify/test_stg_string_valuegetters.cpp`, with local placeholder
definitions for the 3 extern arrays these accessors return, same
per-TU-local-definition convention as `test_stg_wave_sequence_updaters.cpp`/
`test_stg_program_slot_getters.cpp`). `make verify` full suite green
(109 targets), zero regressions. Real `make ko-clean && make ko
KDIR=/home/build/linux-kronos` build green. Manifest 4035 ->
4043/21,689 (18.641%).

Real-HW test that would help: none identified -- pure literal-
constant reflection accessors + a confirmed no-op, no hardware I/O
surface of their own.

## Round 65 (OA.ko, solo, 2026-07-29): CSTGControllerRTData, 3-method batch

Wide survey round: `CSTGLFO`, `CSTGProgramModeDrumTrackSlot`,
`CKGGlobalParamMsgHandler`, `CSKMIDILocalCtrlMsgHandler`,
`CSTGCalibrationMsgHandler`, `CKGControlMsgHandler`, `CKGEngine`, and
~15 `CSTGPatch` candidates were all sampled and deferred -- register
confusion (4-explicit-param thiscall exceeding the regparm3 budget),
unrecoverable rodata/jumptables, the standing MsgHandler ctor/dtor/
HandleMessage-triad policy, or the "forwards to an unreconstructed
sibling/callee" pattern (`CSTGPatch`'s `UpdateToneAdjustCommonXxx`
family calling still-pending `CSTGSlotVoiceData::ToneAdjustXxx`
siblings, `InitVoiceNotifyWaveSeq` calling pending
`CSTGWaveSeqGenerator::VoiceInitialized`, etc.) -- all landing a thin
wrapper for one of these would misrepresent unreconstructed code as
real, so none were taken.

Found 3 genuinely clean, self-contained `CSTGControllerRTData`
methods instead: `OnEndDownload()` (39B, zeroes `this+0x30..0x3c` and
clears the low nibble of `this+0x2f`) and
`ResetExtKnobJumpCatch(unsigned int)`/`ResetExtSliderJumpCatch(unsigned int)`
(95B each, same shape at different base offsets -- copy a
`STGAPIFrontPanelStatus::sInstance` byte through if it's a valid
7-bit value, then gate a tri-state jump-catch flag on
`CSTGGlobal::sInstance`'s `+0x29c9fc0` byte, matching this project's
own already-established `oa_adsr_base.h` convention for that global
offset). Both `Reset*` methods were manually cross-checked offset-
by-offset against the ground-truth decompile's `iVar1+N` arithmetic
before committing to the `slot[N]` abstraction used in the rewrite.
Clean `this=EAX/idx=EDX` attribution throughout, no unresolved
callees, no dispatch.

Real host KAT (new file `verify/test_controller_rt_data_reset_ext_jump_catch.cpp`,
11 checks across 3 sections, using the established `mmap32()` helper
for simulating `CSTGGlobal::sInstance`'s large 32-bit offset).
`make verify` full suite green (110 targets), zero regressions. Real
`make ko-clean && make ko KDIR=/home/build/linux-kronos` build
green. Manifest 4043 -> 4046/21,689 (18.655%).

Real-HW test that would help: none identified -- pure field writes
gated on already-modeled global state, no hardware I/O surface of
their own.

## Round 66 (OA.ko, solo, 2026-07-29): 11-method confirmed-trivial sweep across 5 classes

Surveyed classes with only a handful of pending methods left (mostly-
complete classes are cheaper to finish than to open a fresh one) --
`CFileStream`/`CSTGAudioDriverInterface`/`CSTGLFO` each had exactly 1
pending method, but all 3 hit red flags (raw vtable dispatch into an
unconfirmed slot for the first two; an unresolved, unnamed
`func_0x00141724` callee plus deep linked-list traversal for
`CSTGLFO::ProcessSubRate`) and stayed deferred.

Instead landed 11 confirmed genuinely-trivial methods spread across 5
different classes, all either a bare `ret` (1-3 byte bodies) or an
unconditional `return 0`/`return false`, cc=`__cdecl` in ground truth
despite class-scoped doxygen names (i.e. real `this`-ignoring,
effectively-static functions) -- `CSTGAudioInputMixerBase::ShouldMute`,
`CSTGLFOBase::AdvanceFadeEnv`/`ShouldDelayCompensateRestart` (the
latter is a DISTINCT real function from the already-reconstructed
`CSTGLFO::ShouldDelayCompensateRestart`, a derived-class override that
does real work -- both genuinely exist at different addresses, not a
naming collision), `CSTGAudioEvent::HandleFileOpened/HandleFileClosed/
HandleErrorOpening/HandleErrorReading/HandleErrorWriting` (5 methods),
`CSTGMidiPortManager::DumpQueueDepths`, and
`CSTGStreamingFileReader::~CSTGStreamingFileReader`. Several of these
(`AdvanceFadeEnv`, `DumpQueueDepths`) have a doxygen-documented
parameter list that disagrees with their own decompiled `(void)`
signature -- harmless given the body is confirmed empty regardless of
what it's declared to take, same precedent as `CSTGString::InitVoice`.

Real host KAT (11 new checks spread across 5 existing test files --
`test_audio_input_mixer.cpp`/`test_lfo_component.cpp`/
`test_managers.cpp`/`test_midi_port_manager.cpp`/
`test_playback_event_methods.cpp`). `make verify` full suite green
(all targets, exit 0), zero regressions. Real `make ko-clean && make
ko KDIR=/home/build/linux-kronos` build green. Manifest 4046 ->
4057/21,689 (18.705%).

Real-HW test that would help: none identified -- pure no-op/constant-
return stubs, no hardware I/O surface of their own.
