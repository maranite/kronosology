#!/usr/bin/env python3
"""
update_builder.py — Build and sign a Korg Kronos update package.

UpdateOS signature algorithm (Learned from UpdateOS binary v3.2.1, x86 32-bit):
    SIGNATURE = SHA1(pretar.sh_content + posttar.sh_content + UPDATER_KEY)

Rules:
  - Scripts (PRETARSCRIPT / POSTTARSCRIPT) are optional.
  - SIGNATURE is required if and only if at least one script is present.
  - Script order in the hash is always pretar first, then posttar (if both present).
  - The md5sum file on the USB stick is NOT checked by UpdateOS — ignore it.
  - SOURCE tar.gz must physically exist on the USB at the path UpdateOS mounts.

Usage:
    # Re-sign an existing install.info (rewrites SIGNATURE in-place):
    ./update_builder.py /path/to/update_folder

    # Create install.info from scratch:
    ./update_builder.py /path/to/update_folder \\
        --version 3.2.1 \\
        --source MyUpdate.tar.gz \\
        --pretar pretar.sh \\
        --posttar posttar.sh

    # Preview without writing:
    ./update_builder.py /path/to/update_folder --dry-run
"""

import argparse
import hashlib
import sys
from pathlib import Path

# 16-byte key from UpdaterScriptsKey in UpdateOS .data section (VMA 0x0813bac8).
# Verified against KRONOS_Updater_3_2_1 (sig 8844b641...) and kronos_rooting (sig 0849999e...).
UPDATER_KEY = bytes([
    0x13, 0xd0, 0xaf, 0xef, 0xe0, 0x3c, 0x9b, 0x92,
    0x16, 0x2f, 0xae, 0xff, 0x77, 0x53, 0x55, 0xe1,
])

INSTALL_INFO = "install.info"


def compute_signature(folder: Path, pretar: str | None, posttar: str | None) -> str:
    """Return SHA1(pretar? + posttar? + key) as lowercase 40-char hex."""
    h = hashlib.sha1()
    for script_name, label in [(pretar, "PRETARSCRIPT"), (posttar, "POSTTARSCRIPT")]:
        if script_name:
            path = folder / script_name
            if not path.exists():
                sys.exit(f"Error: {label} '{path}' not found")
            h.update(path.read_bytes())
    h.update(UPDATER_KEY)
    return h.hexdigest()


def parse_info(path: Path) -> dict[str, str]:
    fields: dict[str, str] = {}
    for line in path.read_text().splitlines():
        line = line.strip()
        if "=" in line and not line.startswith("#"):
            key, _, val = line.partition("=")
            fields[key.strip()] = val.strip()
    return fields


def write_info(path: Path, version: str, source: str,
               pretar: str | None, posttar: str | None,
               signature: str | None) -> None:
    lines = [f"VERSION={version}", f"SOURCE={source}"]
    if pretar:
        lines.append(f"PRETARSCRIPT={pretar}")
    if posttar:
        lines.append(f"POSTTARSCRIPT={posttar}")
    if signature:
        lines.append(f"SIGNATURE={signature}")
    path.write_text("\n".join(lines) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Build/sign a Kronos update package install.info",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("folder", help="Path to folder containing update files")
    parser.add_argument("--version", help="VERSION value (overrides existing install.info)")
    parser.add_argument("--source",  help="SOURCE filename, e.g. MyUpdate.tar.gz")
    parser.add_argument("--pretar",  help="PRETARSCRIPT filename (use '' to clear)")
    parser.add_argument("--posttar", help="POSTTARSCRIPT filename (use '' to clear)")
    parser.add_argument("--dry-run", action="store_true",
                        help="Show computed values without writing anything")
    args = parser.parse_args()

    folder = Path(args.folder).resolve()
    if not folder.is_dir():
        sys.exit(f"Error: '{folder}' is not a directory")

    info_path = folder / INSTALL_INFO
    existing = parse_info(info_path) if info_path.exists() else {}

    # CLI args override existing; passing '' explicitly clears a field.
    def resolve(cli_val: str | None, field: str) -> str | None:
        if cli_val is not None:
            return cli_val or None  # '' → None (clear)
        return existing.get(field) or None

    version = args.version or existing.get("VERSION")
    source  = args.source  or existing.get("SOURCE")
    pretar  = resolve(args.pretar,  "PRETARSCRIPT")
    posttar = resolve(args.posttar, "POSTTARSCRIPT")

    if not version:
        sys.exit("Error: VERSION not set — pass --version or include it in install.info")
    if not source:
        sys.exit("Error: SOURCE not set — pass --source or include it in install.info")

    source_path = folder / source
    if not source_path.exists():
        print(f"Warning: SOURCE '{source_path}' not found in folder — "
              "UpdateOS will fail at runtime", file=sys.stderr)

    signature = compute_signature(folder, pretar, posttar) if (pretar or posttar) else None

    # Report
    print(f"Folder       : {folder}")
    print(f"VERSION      : {version}")
    print(f"SOURCE       : {source}")
    print(f"PRETARSCRIPT : {pretar or '(none)'}")
    print(f"POSTTARSCRIPT: {posttar or '(none)'}")
    if signature:
        print(f"SIGNATURE    : {signature}")
    else:
        print("SIGNATURE    : (not required — no scripts)")

    if existing.get("SIGNATURE") and signature:
        if existing["SIGNATURE"] == signature:
            print("(signature unchanged)")
        else:
            print(f"(replacing old: {existing['SIGNATURE']})")

    if args.dry_run:
        print("\n[dry-run] No files written.")
        return

    write_info(info_path, version, source, pretar, posttar, signature)
    print(f"\nWrote: {info_path}")


if __name__ == "__main__":
    main()
