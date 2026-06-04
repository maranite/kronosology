#!/usr/bin/env bash
# diff_kronos_versions.sh
#
# Compare two Kronos firmware update packages and report what changed,
# including content inside the encrypted Mod / Eva / WaveMotion volumes.
#
# Usage:
#   ./diff_kronos_versions.sh  <old_update_dir>  <new_update_dir>  [workdir]
#
# Each <update_dir> is the unpacked update tree containing
# korg/ro/Mod.img / korg/ro/Eva.img / korg/ro/WaveMotion.img
# (and the unencrypted overlay like sbin/loadmod.ko, korg/VersionInfo).
#
# Examples:
#   ./diff_kronos_versions.sh KRONOS_Update_3_2_1 KRONOS_Update_3_2_2/mnt
#
# Requires: python3 + cryptography, debugfs (e2fsprogs).

set -euo pipefail

OLD_DIR="${1:?usage: $0 <old_update_dir> <new_update_dir> [workdir]}"
NEW_DIR="${2:?usage: $0 <old_update_dir> <new_update_dir> [workdir]}"
WORK="${3:-/tmp/kronos_diff}"

DECRYPT="$(dirname "$0")/decrypt_kronos_img.py"
[ -x "$DECRYPT" ] || { echo "ERROR: $DECRYPT not found/executable" >&2; exit 1; }

OLD_TAG=$(basename "$(realpath "$OLD_DIR")")
NEW_TAG=$(basename "$(realpath "$NEW_DIR")")

echo "================================================================"
echo "  Kronos firmware diff:  $OLD_TAG  →  $NEW_TAG"
echo "================================================================"

find_img() {
    find "$1" -maxdepth 5 -type f -name "$2" 2>/dev/null | head -1
}

mkdir -p "$WORK"
DEC_OLD="$WORK/$OLD_TAG/decrypted";  DEC_NEW="$WORK/$NEW_TAG/decrypted"
EXT_OLD="$WORK/$OLD_TAG/extracted";  EXT_NEW="$WORK/$NEW_TAG/extracted"
mkdir -p "$DEC_OLD" "$DEC_NEW" "$EXT_OLD" "$EXT_NEW"

decrypt_and_extract() {
    local label="$1" src_root="$2" dec_dir="$3" ext_dir="$4"
    for typ in Mod Eva WaveMotion; do
        local img plain
        img=$(find_img "$src_root" "${typ}.img")
        [ -n "$img" ] || { echo "  [$label] WARN: ${typ}.img not found"; continue; }
        plain="$dec_dir/${typ}_plain.img"
        if [ ! -s "$plain" ]; then
            echo "  [$label] decrypt $typ.img ..."
            python3 "$DECRYPT" "$img" "$plain" >/dev/null
        fi
        if [ ! -d "$ext_dir/$typ" ] || [ -z "$(ls -A "$ext_dir/$typ" 2>/dev/null)" ]; then
            echo "  [$label] extract $typ.img ..."
            mkdir -p "$ext_dir/$typ"
            debugfs -R "rdump / $ext_dir/$typ" "$plain" 2>/dev/null \
                | grep -v "while changing ownership" || true
        fi
    done
}

echo
echo "── Decrypt + extract ──"
decrypt_and_extract OLD "$OLD_DIR" "$DEC_OLD" "$EXT_OLD"
decrypt_and_extract NEW "$NEW_DIR" "$DEC_NEW" "$EXT_NEW"

# Build manifests and diff using Python (clean, single source of truth)
python3 - "$EXT_OLD" "$EXT_NEW" "$OLD_DIR" "$NEW_DIR" <<'PY'
import os, sys, hashlib

ext_old, ext_new, raw_old, raw_new = sys.argv[1:5]

def manifest(root):
    out = {}
    for dirpath, _, files in os.walk(root):
        for f in files:
            full = os.path.join(dirpath, f)
            rel  = os.path.relpath(full, root)
            try:
                with open(full, 'rb') as fp:
                    h = hashlib.md5()
                    while True:
                        b = fp.read(1 << 20)
                        if not b: break
                        h.update(b)
                size = os.path.getsize(full)
                out[rel] = (size, h.hexdigest())
            except OSError:
                pass
    return out

def diff(a, b, label):
    a_files, b_files = set(a), set(b)
    removed = sorted(a_files - b_files)
    added   = sorted(b_files - a_files)
    changed = sorted(p for p in (a_files & b_files) if a[p] != b[p])
    print()
    print(f"────────────────────────────────────────────────────────")
    print(f"  {label}")
    print(f"────────────────────────────────────────────────────────")
    print(f"    files (old/new): {len(a)}/{len(b)}")
    if not (removed or added or changed):
        print("    ✓ identical content")
        return
    if removed:
        print("    removed:")
        for p in removed: print(f"      − {p}  ({a[p][0]:,} bytes)")
    if added:
        print("    added:")
        for p in added:   print(f"      + {p}  ({b[p][0]:,} bytes)")
    if changed:
        print("    changed:")
        for p in changed:
            so, _ = a[p]; sn, _ = b[p]
            delta = f"{sn-so:+d}" if sn != so else "±0"
            print(f"      ~ {p}  ({so:,} → {sn:,} bytes, {delta})")

# Per-image diffs
for typ in ("Mod", "Eva", "WaveMotion"):
    o = os.path.join(ext_old, typ)
    n = os.path.join(ext_new, typ)
    if not (os.path.isdir(o) and os.path.isdir(n)): continue
    diff(manifest(o), manifest(n), f"{typ}.img")

# Unencrypted overlay diff — skip the encrypted images themselves
def overlay(root):
    excludes = ("Mod.img", "Eva.img", "WaveMotion.img")
    out = {}
    for dirpath, _, files in os.walk(root):
        for f in files:
            if f in excludes: continue
            full = os.path.join(dirpath, f)
            rel  = os.path.relpath(full, root)
            try:
                with open(full, 'rb') as fp:
                    h = hashlib.md5()
                    while True:
                        b = fp.read(1 << 20)
                        if not b: break
                        h.update(b)
                size = os.path.getsize(full)
                out[rel] = (size, h.hexdigest())
            except OSError:
                pass
    return out

diff(overlay(raw_old), overlay(raw_new), "Unencrypted overlay")
PY

echo
echo "Manifests + extracted trees kept under: $WORK"
