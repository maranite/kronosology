# Kronosology

> A knowledge base and patching toolkit for the
> [Korg Kronos](https://www.korg.com/products/synthesizers/kronos2/) workstation.

The Korg Kronos is a Linux box with a digital signal processor and music
workstation rolled into one — but its inner workings are essentially
undocumented outside Korg's engineering team.  **Kronosology** is the result
of several intensive weeks of reverse-engineering: analysis notes covering the
full system, a tool to authorize EX libraries on any stock unit, and an offline
patcher that produces a signed Korg-format OS update package from a stock
firmware tree.

Most visitors will only be interested in the two tools below.  Both run on
Linux (native — see [environments.md](../PROJECT_BRAIN/environments.md), WSL
is no longer part of the dev setup).

> **Shared context**: kronosology is the deep-RE trunk feeding a larger
> Kronos modding ecosystem — its findings drive
> [KronosScreenRemoteDaemon](../KronosScreenRemoteDaemon/)'s kernel modules,
> which in turn serve [KronosScreenRemote](../KronosScreenRemote/) and
> [KronosScreenRemotePy](../KronosScreenRemotePy/). Cross-project
> architecture, shared dev environments, credentials/access pointers, and
> agent/tooling policy live in
> [`/home/share/PROJECT_BRAIN/BRAIN.md`](../PROJECT_BRAIN/BRAIN.md) —
> check there before duplicating knowledge into this repo.

---

## Tools

| You want to... | Go to | Risks Kronos not booting? |
|---|---|---|
| **Authorize all EX libraries on a stock Kronos** (USB, no rooting required) | [`auto-auth/`](auto-auth/) | No |
| **Patch a stock Kronos** — removes EX auth checks, auto-auth on every install | [`offline-patcher/`](offline-patcher/) | Yes |

> **If you are using the offline patcher for the first time,** set up SSH root
> access before patching so you can recover if anything goes wrong.  See
> [uprooting/kronos_rooting](https://github.com/uprooting/kronos_rooting).

---

## Quick start

### Authorize all EX libraries via USB (stock Kronos, no SSH)

```sh
cd auto-auth
python3 build_auto_auth.py
# → copy output/auto-auth/* to FAT USB root, trigger OS Update from Kronos front panel
```

### Patch a stock Kronos firmware offline

```sh
cd offline-patcher
pip install cryptography
python3 patch_firmware_offline.py /path/to/KRONOS_Update_3_2_2/mnt
# → copy output/kronosology-offline-patched/* to FAT USB root, trigger OS Update
```

**After either USB operation: power-cycle the Kronos (≥ 60 s unplugged).**
The OmapNKS4 front-panel chip can wedge across soft reboots.
See [docs/modules/OmapNKS4Module.ko_chip_wedge.md](docs/modules/OmapNKS4Module.ko_chip_wedge.md).

---

## Documentation

The full analysis corpus is in [`docs/`](docs/).

| You want to understand... | Go to |
|---|---|
| Kronos software architecture end-to-end | [docs/system_overview.md](docs/system_overview.md) |
| Boot integrity chain (loadoa → loadmod → cryptoloop → OA → Eva) | [docs/system_overview.md](docs/system_overview.md), [docs/modules/loadoa.md](docs/modules/loadoa.md), [docs/modules/loadmod.ko.md](docs/modules/loadmod.ko.md) |
| Atmel NV2AC security IC protocol (GPA stream cipher) | [docs/crypto/atmel_nv2ac.md](docs/crypto/atmel_nv2ac.md) |
| OS-update signature algorithm (SHA-1 + `UpdaterScriptsKey`) | [docs/crypto/update_signature.md](docs/crypto/update_signature.md) |
| EX-bank authorization algorithm (Base32 + Blowfish-CFB + MD5 + chip secret) | [docs/crypto/auth_string_algorithm.md](docs/crypto/auth_string_algorithm.md) |
| On-disk format of programs / combis / drum kits / wave sequences | [docs/preload/](docs/preload/) |
| Module-by-module studying notes | [docs/modules/](docs/modules/) |
| Setting up Ghidra for Kronos analysis | [docs/workflow/ghidra_setup.md](docs/workflow/ghidra_setup.md) |

Full doc index: [`docs/README.md`](docs/README.md).

---

## How this came to exist

Long-running analysis sessions using [Ghidra](https://ghidra-sre.org/) plus the
[GhidraMCP](https://github.com/LaurieWired/GhidraMCP) bridge, with much trial
and error — and much breaking-then-fixing-the-Kronos-via-SSH.  The operational
lessons are documented alongside the binaries they belong to; see for example
[loadmod.ko_inner_md5_check](docs/modules/loadmod.ko_inner_md5_check.md),
[OA.ko_hot_swap_bug](docs/modules/OA.ko_hot_swap_bug.md), and
[OmapNKS4Module.ko_chip_wedge](docs/modules/OmapNKS4Module.ko_chip_wedge.md).

---

## Caveats & legal

- This is educational work for **personal-use research** purposes.
  Korg owns the Kronos firmware; nothing in this repo redistributes any
  Korg binary.  The patcher works against the user's own already-licensed install.
- The OS-update mechanism uses cryptographic signatures that this repo includes
  the necessary key for.  **Do not use this to distribute pirated content.**
  The point is to let owners customise their own instruments.
- Tested against Kronos OS v3.2.1 and v3.2.2.
- **There is no warranty.**  A Kronos in the wrong state can be bricked enough
  to require a CD-ROM reinstall.  The patcher's rollback-on-failure logic
  minimises this risk but cannot eliminate it.

## License

The analysis (documentation, scripts, RE notes) in this repository is released
into the public domain — use, modify, distribute as you wish.

The patched Korg binaries are not distributed here; the patcher produces them
from the user's own stock files at build time.

## Contributing

If you've used this work, found a bug, extended it to a different Kronos OS
version, or want to discuss any aspect of the analysis, please open an issue or
a PR.

If you're starting your own Kronos exploration,
[`docs/workflow/`](docs/workflow/) has the methodology notes.
