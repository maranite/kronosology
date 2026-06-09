#!/bin/sh
# build_updater.sh — produce the kronosology USB-stick updater package.
#
# Output: ./output/kronosology-installer/  — copy contents of this directory
# to the root of a FAT-formatted USB stick.
#
# Requires: tar, gzip, python3 (for ../update-builder/update_builder.py).
#
# After the user plugs in the USB and triggers OS update from the Kronos
# front panel, UpdateOS:
#   1. Reads install.info  (we provide it, with the SOURCE / PRETARSCRIPT keys)
#   2. Verifies SHA-1 signature over pretar.sh + UPDATER_KEY
#   3. Runs pretar.sh as root (this IS kronos_patcher.sh)
#   4. Extracts kronosology.tar.gz (just a marker file — no real payload)
#   5. Reports success
# User then power-cycles the Kronos; on next boot the patches are active.

set -e
cd "$(dirname "$0")"
HERE="$(pwd)"

STAGING="$HERE/output/kronosology-installer"
PATCHER="$HERE/../patcher/kronos_patcher.sh"
BUILDER="$HERE/../update-builder/update_builder.py"

[ -f "$PATCHER" ] || { echo "missing patcher at $PATCHER"; exit 1; }
[ -x "$BUILDER" ] || { echo "missing builder at $BUILDER"; exit 1; }

echo "[build] cleaning staging $STAGING"
rm -rf "$STAGING"
mkdir -p "$STAGING" "$STAGING/mnt"

# ---------------------------------------------------------------------------
# pretar.sh = kronos_patcher.sh with an UpdateOS-context banner prepended.
# We keep it as ONE file so signing is straightforward.
# ---------------------------------------------------------------------------
echo "[build] building pretar.sh from kronos_patcher.sh"
{
    cat <<'BANNER'
#!/bin/sh
# Kronosology updater — PRETAR script
# This is the kronos_patcher.sh, invoked by UpdateOS as root during a stock
# Korg Kronos OS update. It applies the EX-bank-authorization bypass patches
# to /sbin/loadmod.ko, /sbin/loadoa, and /sbin/OA.ko, copies OA.ko and
# KorgUsbAudioDriver.ko out of the /korg/Mod cryptoloop into /sbin/, and
# backs up the originals to /korg/rw/kronos_patcher_backup/.
#
# This pretar exits 0 on success AND on already-patched state (idempotent).
# On any patch-site mismatch it rolls back to originals and exits non-zero,
# which causes UpdateOS to abort cleanly without extracting the tarball.
echo "==============================================================="
echo "Kronosology installer — UpdateOS PRETAR"
echo "==============================================================="
BANNER
    # Append everything below the shebang of kronos_patcher.sh
    tail -n +2 "$PATCHER"
} > "$STAGING/pretar.sh"
chmod +x "$STAGING/pretar.sh"

# ---------------------------------------------------------------------------
# kronosology.tar.gz — minimal payload.
#
# UpdateOS REQUIRES a SOURCE tar.gz to exist and extracts it after pretar.
# We don't actually need anything extracted (pretar did all the work), so we
# ship a single marker file in /tmp/. UpdateOS extracts relative to /, so this
# lands at /tmp/kronosology_installed and contains the version string.
# ---------------------------------------------------------------------------
echo "[build] building kronosology.tar.gz"
PAYLOAD=$(mktemp -d)
mkdir -p "$PAYLOAD/tmp"
echo "kronosology-installer 1.0 — built $(date -u +'%Y-%m-%dT%H:%M:%SZ')" \
    > "$PAYLOAD/tmp/kronosology_installed"
( cd "$PAYLOAD" && tar -czf "$STAGING/kronosology.tar.gz" tmp/ )
rm -rf "$PAYLOAD"
ls -la "$STAGING/kronosology.tar.gz"

# ---------------------------------------------------------------------------
# Generate install.info with VERSION + SOURCE + PRETARSCRIPT, then sign.
# ---------------------------------------------------------------------------
echo "[build] generating signed install.info"
# Use --posttar '' to make sure no posttar field sneaks in.
python3 "$BUILDER" "$STAGING" \
    --version "kronosology-1.0" \
    --source  "kronosology.tar.gz" \
    --pretar  "pretar.sh" \
    --posttar ""

# ---------------------------------------------------------------------------
# Include the README on the stick too, so curious users can read what they
# put on the stick. UpdateOS ignores any files not referenced in install.info.
# ---------------------------------------------------------------------------
echo "[build] copying README"
cp "$HERE/README.md" "$STAGING/README.txt"

echo
echo "==============================================================="
echo "Build complete. Stage contents:"
echo "==============================================================="
ls -la "$STAGING"
echo
echo "To install onto a USB stick:"
echo "  1. Format a USB stick as FAT (or ext2)"
echo "  2. Copy the contents of $STAGING/ to the root of the stick"
echo "  3. Insert the stick into the Kronos"
echo "  4. From Kronos front-panel menu, trigger an OS update"
echo "  5. After update completes, POWER-CYCLE (not soft-reboot) the Kronos"
