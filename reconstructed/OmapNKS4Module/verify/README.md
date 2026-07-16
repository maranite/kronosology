# OmapNKS4Module verify/ — host-side known-answer tests

Mirrors the convention already used by `../AT88VirtualChip/verify/`, `../OA/verify/`,
`../RTAIVirtualDriver/verify/`: plain `assert`/`check_eq`-style host test binaries,
built and linked directly against the real `.cpp` translation units under test (not
reimplementations of them), so a bug in `command.cpp`/`driver.cpp` itself gets caught
without needing a real Kronos or a kernel build. `make verify` from the parent
directory builds and runs everything here; see the parent `Makefile`'s header comment
for the dual-mode (`make verify` vs `make ko KDIR=...`) structure.

## Why this didn't exist until 2026-07-15

Every other reconstructed module alongside this one (`OA`, `AT88VirtualChip`,
`RTAIVirtualDriver`, `STGGmp`) has a `verify/` directory. This one didn't, despite its
own README claiming "all eight translation units... reconstructed" — meaning
`command.cpp` and `driver.cpp` had never actually been compiled since being
transcribed from Ghidra's decompiler output. Building the first host test for each
immediately surfaced two real, pre-existing bugs that a straight read-through of the
source hadn't caught:

- **`command.cpp`/`driver.cpp` had 5 of 6 setter word encodings and the
  `ReadPortConfiguration` response-echo wrong** (fixed 2026-07-15 via fresh Ghidra
  decompile + disassembly of `OmapNKS4Module.ko` 3.2.2 - see
  `KronosNKS4/docs/gaps.md`, not something this test suite itself found, but exactly
  the kind of regression it's now positioned to catch if it happens again).
- **`driver.cpp` didn't compile at all**, independent of any of the above: the
  file-wide `#define sInstance COmapNKS4Driver_sInstance` macro collided with
  `CSTGOmapNKS4Fifos`'s own, unrelated static member also named `sInstance` (two call
  sites), and the exported `COmapNKS4Driver_ShutDown(void)` was declared/called with
  zero arguments despite `SubmitNKS4CommandWrite` requiring one - ground truth (fresh
  disassembly of `COmapNKS4Driver_ShutDown@0x15600`) shows it really takes a 16-bit
  parameter folded into the command word. Both fixed directly in `driver.cpp`
  (see that file's inline comments) and `main.cpp` (the one real call site).

## What's tested

- **`test_command.cpp`** — every `COmapNKS4Command` setter/query word encoding fixed
  this session: `SetAllAnalogInputFilter`, `SetRotaryEncoderSampleSpeed`,
  `SetLCDBrightness`, `ResetModule`, `ConfigureRotaryEncoders` (including its `n==0`
  no-op case and its 3-word sequence), and `ReadPortConfiguration`'s `0x0171` reg-echo
  decode.
- **`test_driver_receive_event_buffer.cpp`** — the two bugs found in
  `COmapNKS4Driver::ReceiveEventBuffer` this session: the `op==1/idx==0x71` reg-echo
  word (previously missing its opcode contribution) and the aftertouch (`op==3`)
  event byte-packing (previously low/high swapped, and the test-mode two-event branch
  was an unimplemented stub).

Both link the real `command.cpp`/`driver.cpp` source files directly - `host_stubs.cpp`
supplies the STG/RTAI/kernel externs they depend on (`SubmitNKS4CommandWrite`,
`CNKS4EventFilter::FilterEvent`, `OmapNKS4ProcAddEvent`, `ApplyNKS4Calibration`,
`SendNKS4EventToLinuxReader`, the `sAfterTouch*ConvertTable` curves, `g_video`, etc.)
as either recording test doubles (so a test can assert what was sent) or trivial
no-ops, matching the "test double" convention `AT88VirtualChip/verify` already
established. These are deliberately NOT faithful reimplementations of the real kernel
behavior - that would just be re-deriving `driver.cpp`'s own callers, out of scope for
a unit test of `driver.cpp` itself.

## What's NOT covered

- `submit.cpp`, `usb.cpp`, `procfs.cpp`, `main.cpp`, `realtime.cpp`, `video.cpp` -
  each pulls in enough additional RTAI/USB/kernel-specific surface (real threads, real
  USB URB submission, the C++ runtime shim) that a host-testable seam wasn't judged
  worth building this session. `command.cpp`/`driver.cpp` were prioritized because
  they're where this session's actual fixes landed.
- The real curve data behind `sAfterTouch1ConvertTable`/`sAfterTouch2ConvertTable`
  and the `_DAT_0000af38` progress-bar scale factor - these were only ever
  `extern`-declared in the original reconstruction, never defined anywhere (not a gap
  introduced by this test suite). `host_stubs.cpp` supplies zeroed placeholder tables;
  `test_driver_receive_event_buffer.cpp` exercises `ReceiveEventBuffer`'s calibration
  *call site* (idx≠7, so the curve tables aren't reached) and byte-packing logic, not
  the curve values themselves.
- A real `.ko` Kbuild build (`make ko KDIR=...`) - not attempted this session; needs
  the real Kronos kernel tree per `CLAUDE.md`'s Development Environment section.

## Building

```sh
cd kronosology/reconstructed/OmapNKS4Module
make verify        # builds + runs both test binaries
```

Requires 32-bit C++ support (`g++-multilib`'s 32-bit libstdc++, e.g. Debian/Ubuntu's
`g++-12-multilib` or equivalent) - `omapnks4_internal.h` declares a kernel-style
`operator new(unsigned int)`, which only satisfies C++'s required `operator
new(size_t)` signature when `size_t` is 4 bytes (true on the module's real x86-32
target; requires `-m32` on a 64-bit host).
