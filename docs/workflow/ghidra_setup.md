# Ghidra Project Setup & Conventions

## How this project's Ghidra setup was built

Analysis draws on several data sources:
1. The Korg factory v3.2.1 updater. Its update tar file contains `UpdateOS` under
   `/sbin/`, analyzed to understand how updates are validated and applied to the
   Kronos.
2. The `kronos_rooting` repo's updater installs dropbear, enabling SSH access —
   essential for testing patches, inspecting kernel state, and studying the NKS4
   USB device's "wedge" failure mode (see
   [`../modules/OmapNKS4Module.ko_chip_wedge.md`](../modules/OmapNKS4Module.ko_chip_wedge.md)),
   which is plausibly the real reason the Kronos requires a physical power cycle
   after certain kinds of restart — Korg's own developers may not have trusted a
   soft-reboot to be safe.
3. The Korg 3-DVD factory re-installer, unpacked for a comprehensive offline record
   of an out-of-box Kronos's bootable filesystem.
4. A dump of files from a live Kronos.

In practice, binaries recovered from a live Kronos are what get imported into
Ghidra for the main analysis. **All binaries are imported as separate programs**
into the Ghidra project:

| Program | Purpose | Value |
|---|---|---|
| `OA.ko` | The synthesis engine — all audio, engines, banks, etc. | Patching to remove authenticity checks |
| `UpdateOS` | Applies firmware updates | Ability to author & apply own updates without root |
| `loadmod.ko` | Boot-time integrity/authenticity gatekeeper | Speeding up boot time, bypassing unneeded integrity checks |
| `loadoa` | Boot-up program that loads realtime modules, loadmod, OA, etc. | Enables relocating OA.ko |
| `InstallEXs` | Handles installation of Korg EXs extension banks | Understanding where extensions go and how options are written |
| `Eva` | Drives the Kronos display | — |
| `OmapNKS4Module.ko` | Handles comms with the NKS4 (front-panel) sister board | — |
| `STGEnabler.ko` | Enables bigint math | — |
| `STGGmp.ko` | Enables bigint math | — |
| `GetPubIdMod.ko` | Retrieves the Public ID from the Atmel chip | — |

---

## Address mapping cheatsheet

Critical because each binary type has its own convention:

| Binary class | Formula |
|---|---|
| Kernel module (`.ko`, ET_REL) | `Ghidra_addr = nm_symbol_value` for sections at `sh_addr=0`. **Caveat**: COMDAT sections (`.text.ClassName::Method`) place at higher addresses than nm reports; use Ghidra's `read_memory` against Ghidra's address, not nm's |
| Userspace executable (`Eva`, `UpdateOS`, `InstallEXs`, `loadoa`) | `Ghidra_addr = full VMA` (image base + offset) — typically `0x08048000`+ |
| File offset (in the `.ko` ELF on disk) | `file_offset = nm_value + 0xb390` for OA.ko's main `.text` |
| Runtime address on a live Kronos | `runtime_addr = 0x59CE6000 + nm_value` for OA.ko (from `/proc/kallsyms`) |
