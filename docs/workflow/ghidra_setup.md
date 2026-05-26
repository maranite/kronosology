# Ghidra Project Setup & Conventions

## How Ghidra was set up to study the Korg Kronos

I started with several data sources to perform this analysis:
1. The Korg factory v3.2.1 updater. This allowed me to understand the anaotomy of an update. 
   Inside the update tar file, the UpdateOs binary can be found under `/sbin/` which was analysed to understand how updates are validated and applied to the Kronos.
2. The kronos_rooting repo provides a link to an updater which (among other things) installs dropbear. Being able to SSH in to the Kronos was crucial to testing out patches and understanding kernel states as well as uncovering some quirks in the NKS4 USB device that cause it to "wedge"  -  presumably this is the real reason the Kronos requires a phyiscal power cycle after an update - because the Korg developers realised they could not safely soft-reboot their own creation.
3. The Korg 3-DVD factory re-installer. This can be easily unpacked for a comprehensive offline record of what an out-of-box Kronos' bootable filesystem looks like.
4. A dump of files running on my own live Kronos.

In practice, the binaries recovered form the live Kronos were imported into Ghidra for the main analysis.
**All binaries were imported as separate programs** into the Ghidra project:

| Program | Purpose | Value |
|---|---|----|
| `OA.ko` | What you experience as the Kronos - all audio, enginers, banks, etc. | Patching to remove authenticity checks |
| `UpdateOS` | Program to apply updates |  Ability to author & apply own updates without root |
| `loadmod.ko` | Evil, hacky authenticity policeman |  Speeding up boot time, bypassing pointless integrity checks |
| `loadoa` | Boot-up program which loads realtime modules, loadmod and OA, etc. |  Enables us to relocate OA.ko |
| `InstallEXs` | Handles installation of Korg EXs extension banks | Understanding where extensions go. how 
| `Eva` | A program which facilities the Kronos display | None |
| `OmapNKS4Module.ko` | Handles comms with NKS4 (aka. the Shark) sister board | None | 
| `STGEnabler.ko` | Enables bigint math | None |
| `STGGmp.ko` | Enables bigint math | None |
`options` are written |
| `GetPubIdMod.ko` | Retrieves `publid-id` from ATMEL chip | None |

---


## Address mapping cheatsheet

Critical because each binary type has its own convention:

| Binary class | Formula |
|---|---|
| Kernel module (`.ko`, ET_REL) | `Ghidra_addr = nm_symbol_value` for sections at `sh_addr=0`. **Caveat**: COMDAT sections (`.text.ClassName::Method`) place at higher addresses than nm reports; use Ghidra's `read_memory` against Ghidra's address, not nm's |
| Userspace executable (`Eva`, `UpdateOS`, `InstallEXs`, `loadoa`) | `Ghidra_addr = full VMA` (image base + offset) — typically `0x08048000`+ |
| File offset (in the `.ko` ELF on disk) | `file_offset = nm_value + 0xb390` for OA.ko's main `.text` |
| Runtime address on a live Kronos | `runtime_addr = 0x59CE6000 + nm_value` for OA.ko (from `/proc/kallsyms`) |
