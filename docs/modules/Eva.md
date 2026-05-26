# Eva — Kronos GUI / Front-Panel Application

The userspace application that drives the front-panel UI, touchscreen, mode switching,
patch/combi editing, sequencer UI, and all the user-facing interactions. It is the
largest single binary in the Kronos OS.

| Property | Value |
|---|---|
| Path on device | `/korg/Eva/Eva` (within an encrypted loop-mount) |
| Source path | `dump from kronos/korg/Eva/Eva` |
| Architecture | x86 LE 32-bit ELF executable (ET_EXEC) |
| Size | ~22 MB (22 392 KB) |
| Image base | `0x08048000` (typical static-link x86 ELF) |
| Functions | 38 048 (after Ghidra auto-analysis) / 41 986 nm-defined |
| C++ mangled symbols | 57 601 |
| Compiler | GCC 4.5.0 |

---

## Role in the system

Eva is the *operator*. It:

- Drives the LCD/touchscreen UI ("Peg" widget toolkit — see `PegFormFactory*` classes)
- Owns user gestures and front-panel button/knob/slider/joystick input
- Sends commands to `OA.ko` via `/proc/.oacmd` (see [`../interfaces/proc_oacmd.md`](../interfaces/proc_oacmd.md))
- Manages user files (programs, combis, sets, KARMA GEs)
- Handles import/export, USB stick browsing, and the "Disk" menus
- Holds *no* synthesis or audio code — that's all in `OA.ko`

Eva talks to `OA.ko` exclusively over `/proc/.oacmd` (write-then-read RPC). It also
references string constants for many other paths (`/korg/rw/...`, `/mnt/...`, `/proc/.oa...`).

---

## Notable subsystems (by class prefix)

| Prefix | What | Example |
|---|---|---|
| `Peg*` | UI widget framework (Peg = "Portable Embedded GUI" tookit) | `PegFormFactory<CHelpForm>`, `PegButton`, `PegList` |
| `CForm*` | Mode-specific UI forms | `CFormSeqControl`, `CFormSeqPatControl`, `CHelpForm` |
| `CSK*` | "Synth Kronos" UI model layer — talks to `OA.ko` | `CSKParameterChangeMessage`, `CSKMessage` |
| `CGUI*`, `CDlg*` | Dialogs and global UI state | (many) |

Eva is so large that it is genuinely a multi-week effort to fully understand class-by-class.
For our project's purposes we mostly need to know **what commands it issues** (the `AU:`/`LM:`/etc.
strings) so we can speak the same protocol from `InstallEXs` etc.

---

## Notable strings

| String | Use |
|---|---|
| `/proc/.oacmd` | The command channel to `OA.ko` |
| `/korg/rw/...` | User data paths |
| Various `EX*`, `KSC*` strings | Bank metadata format strings |

---

## Phase 1 / 3 analysis results

| Phase | Result |
|---|---|
| 1 — function prototypes | 34 360 / 37 244 applied (92 %), 2 884 errors (template instantiations & weak symbols at addresses with no Ghidra function) |
| 2 — struct layouts | **Skipped** — would have triggered an estimated 25-hour Ghidra cross-reference cascade. Eva's structs are best derived per-class when needed |
| 3a — return types | 6 465 / 7 016 refined (92 %) |

Eva needs Ghidra's full auto-analysis to be run before applying prototypes (it imports
without it). After a one-time analysis (~6 min wall clock), the prototype/return-type
pass is straightforward.

---

## Where Eva fits in the security model

Eva is **not** trusted with any secrets. It:

- Cannot read the chip secret directly (no port-I/O permission in user space)
- Cannot write `AuthorizationStrings` directly (it's at `/korg/rw/Startup/`, owned by root)
- Submits auth strings via `/proc/.oacmd AU:` and lets `OA.ko` do the verification

This separation is important: any user-space tool can send `AU:<authstring>` to `OA.ko`,
but only valid strings (computed with the per-device chip secret) get accepted. Forging
an auth string in user space requires the chip secret, which is in the kernel module
domain. See [`../crypto/auth_string_algorithm.md`](../crypto/auth_string_algorithm.md)
for the full picture.

---

## Why we didn't deeply analyse Eva

Eva is rich and complex but largely user-facing. For our project goals (boot reliability,
EX authorisation, future port) the relevant code paths are in `OA.ko` and the install
tools (`InstallEXs`, `UpdateOS`). Eva's role is as a *consumer* of OA.ko's interface,
not as a security gate. Once the prototype-typing pass was done, deeper RE was deferred
unless a specific bug or behaviour question arose.
