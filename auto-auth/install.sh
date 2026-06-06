#!/bin/sh
#
# install.sh — deploy auto-auth on the Kronos
#
# Run this script on the Kronos (as root) after copying the auto-auth folder
# to a location accessible on the device (USB stick or scp).
#
# What this does:
#   1. Loads oa_authgen.ko (exposes /proc/.oaauth)
#   2. Renames /sbin/InstallEXs → /sbin/InstallEXs.real  (one-time, idempotent)
#   3. Installs the wrapper shell script as /sbin/InstallEXs
#
# After this, every EX installation from the front panel will automatically
# generate and submit the correct auth string for that EX via stock OA.ko.

set -e

# dirname/id may be absent on Kronos busybox — use shell builtins instead
case "$0" in
    */*)  SCRIPT_DIR="${0%/*}" ;;
    *)    SCRIPT_DIR="." ;;
esac
SCRIPT_DIR=$(cd "$SCRIPT_DIR" && pwd)
# Accept the .ko in either a subdirectory (source tree) or flat (deployed)
if [ -f "$SCRIPT_DIR/oa_authgen/oa_authgen.ko" ]; then
    KO="$SCRIPT_DIR/oa_authgen/oa_authgen.ko"
else
    KO="$SCRIPT_DIR/oa_authgen.ko"
fi

# Prefer the compiled C binary; fall back to the shell script wrapper
if [ -f "$SCRIPT_DIR/wrapper_c/InstallEXs" ]; then
    WRAPPER="$SCRIPT_DIR/wrapper_c/InstallEXs"
else
    WRAPPER="$SCRIPT_DIR/InstallEXs"
fi

# ── Sanity checks ─────────────────────────────────────────────────────────────
# Root check: $UID builtin (bash) or id -u; skip if both unavailable
_uid="${UID:-}"
if [ -z "$_uid" ]; then _uid=$(id -u 2>/dev/null); fi
if [ -n "$_uid" ] && [ "$_uid" != "0" ]; then
    echo "ERROR: must run as root" >&2
    exit 1
fi

if [ ! -f "$KO" ]; then
    echo "ERROR: $KO not found — build the module first:" >&2
    echo "  cd $SCRIPT_DIR/oa_authgen && make" >&2
    exit 1
fi

if [ ! -f "$WRAPPER" ]; then
    echo "ERROR: wrapper script $WRAPPER not found" >&2
    exit 1
fi

# ── 1. Load kernel module ─────────────────────────────────────────────────────
if grep -q oa_authgen /proc/modules 2>/dev/null; then
    echo "oa_authgen.ko already loaded — skipping insmod"
else
    cp "$KO" /sbin/oa_authgen.ko
    # Pass OA.ko function addresses for authenticated chip reads.
    # After SetupAtmelForAuthorizations the chip uses an encrypted session;
    # fFfFfFfFfFfF13 handles per-read cipher sync internally.
    SETUP_ADDR=$(grep ' SetupAtmelForAuthorizations' /proc/kallsyms 2>/dev/null | cut -d' ' -f1)
    READ_ADDR=$(grep ' fFfFfFfFfFfF13' /proc/kallsyms 2>/dev/null | grep '\[OA\]' | cut -d' ' -f1)
    if [ -n "$SETUP_ADDR" ] && [ -n "$READ_ADDR" ]; then
        /sbin/insmod /sbin/oa_authgen.ko setup_atmel_addr=0x${SETUP_ADDR} chip_read_addr=0x${READ_ADDR}
        echo "oa_authgen.ko loaded (setup=0x${SETUP_ADDR} read=0x${READ_ADDR})"
    else
        /sbin/insmod /sbin/oa_authgen.ko
        echo "oa_authgen.ko loaded (OA.ko addresses not found in kallsyms)"
    fi
fi

if [ ! -e /proc/.oaauth ]; then
    echo "ERROR: /proc/.oaauth did not appear after insmod" >&2
    exit 1
fi

# ── 2. Wrap InstallEXs ────────────────────────────────────────────────────────
if [ ! -f /sbin/InstallEXs.real ]; then
    mv /sbin/InstallEXs /sbin/InstallEXs.real
    echo "Renamed /sbin/InstallEXs → /sbin/InstallEXs.real"
else
    echo "/sbin/InstallEXs.real already exists — not overwriting"
fi

cp "$WRAPPER" /sbin/InstallEXs
chmod +x /sbin/InstallEXs
echo "Installed wrapper at /sbin/InstallEXs"

# ── 3. Quick smoke test ───────────────────────────────────────────────────────
echo ""
echo "Smoke test: writing dummy GEN command to /proc/.oaauth ..."
printf 'GEN:S000' > /proc/.oaauth 2>/dev/null && {
    result=$(cat /proc/.oaauth 2>/dev/null)
    if [ -n "$result" ]; then
        echo "  Got auth string (${#result} chars): $result"
        echo "  NOTE: S000 probably doesn't exist — this tests the pipe, not the result"
    else
        echo "  /proc/.oaauth returned empty (option file S000 may not exist — that is OK)"
    fi
} || echo "  GEN write failed (chip may not be responding — test after full boot)"

echo ""
echo "auto-auth installation complete."
echo "To make oa_authgen.ko load at boot, add to /etc/rc.local or equivalent:"
echo "  insmod /sbin/oa_authgen.ko"
